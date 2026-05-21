#include "DrawerListPanel.h"
#include "ElementsStore.h"
#include "IconUtils.h"
#include "ProjectModel.h"
#include "Theme.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSettings>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr const char* kItemMime = "application/x-mira-drawer-item";
}

namespace {

constexpr int kPanelWidth = 300;
constexpr int kCardWidth  = 134;   // (300 - 24 padding - 8 gap) / 2 = 134
constexpr int kCardHeight = 118;   // foto 70 + nome + role + paddings
constexpr int kCardPhoto  = 70;
constexpr int kMinPanelWidth = 240;
constexpr int kMaxPanelWidth = 600;
constexpr int kMinPanelHeight = 220;
constexpr int kResizeHandleWidth = 5;

QString sortLabelFor(DrawerListPanel::SortMode mode, bool asc) {
    switch (mode) {
        case DrawerListPanel::SortCreation: return asc ? QStringLiteral("Criação ↑") : QStringLiteral("Criação ↓");
        case DrawerListPanel::SortAlpha:    return QStringLiteral("A → Z");
        case DrawerListPanel::SortRole:     return QStringLiteral("Por papel");
    }
    return QStringLiteral("Criação ↑");
}

QString miniIconBtnQss() {
    return QStringLiteral(R"(
        QToolButton {
            background: transparent;
            color: %1;
            border: 1px solid transparent;
            border-radius: 6px;
            font-size: 12px;
            padding: 2px 6px;
        }
        QToolButton:hover {
            background: %2;
            color: %3;
            border-color: %4;
        }
        QToolButton:checked, QToolButton[active="true"] {
            background: %5;
            color: %3;
            border-color: %4;
        }
    )").arg(Theme::textMuted(), Theme::hoverOverlay(), Theme::textBright(), Theme::subtleBorder(), Theme::pressedOverlay());
}

QString folderPillQss() {
    return QStringLiteral(R"(
        QPushButton {
            background: transparent;
            color: %1;
            border: 1px dashed %2;
            border-radius: 11px;
            padding: 2px 10px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 11px;
        }
        QPushButton:hover {
            background: %3;
            color: %4;
            border-color: %5;
        }
    )").arg(Theme::textMuted(), Theme::panelBorder(), Theme::hoverOverlay(), Theme::textBright(), Theme::textMuted());
}

QString folderChipQss() {
    // Chip de pasta: arredondado, fundo translúcido, ícone de pasta à esquerda.
    return QStringLiteral(R"(
        QPushButton {
            background: rgba(255,255,255,0.05);
            color: %1;
            border: 1px solid rgba(255,255,255,0.10);
            border-radius: 11px;
            padding: 2px 10px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 11px;
            text-align: left;
        }
        QPushButton:hover {
            background: rgba(255,255,255,0.10);
            color: %2;
            border-color: rgba(255,255,255,0.18);
        }
    )").arg(Theme::textPrimary(), Theme::textBright());
}

QString folderBackQss() {
    // Botão "voltar" exibido quando se está dentro de uma pasta.
    return QStringLiteral(R"(
        QPushButton {
            background: rgba(255,255,255,0.05);
            color: %1;
            border: 1px solid rgba(255,255,255,0.12);
            border-radius: 11px;
            padding: 2px 10px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 11px;
            text-align: left;
        }
        QPushButton:hover {
            background: rgba(255,255,255,0.10);
            color: %2;
            border-color: rgba(255,255,255,0.22);
        }
    )").arg(Theme::textBright(), Theme::textBright());
}

QString createButtonQss(const QString& accent) {
    // Botão grande "Criar novo X" — borda na cor da gaveta, fundo translúcido
    // com leve hint do accent. Espelha o .drawerNewItemGhost do Mira 1.
    const QColor c(accent);
    QColor bg = c; bg.setAlphaF(0.12);
    QColor bgHover = c; bgHover.setAlphaF(0.20);
    const QString bgStr = QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(0.10);
    const QString bgHoverStr = QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(0.20);
    return QStringLiteral(R"(
        QPushButton {
            background: %4;
            color: %1;
            border: 1px solid %2;
            border-radius: 8px;
            padding: 10px 12px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 14px;
            font-weight: 600;
            text-align: left;
        }
        QPushButton:hover {
            background: %3;
        }
        QPushButton:pressed {
            background: %5;
        }
    )").arg(accent, accent, bgHoverStr, bgStr, bgHoverStr);
}

QPixmap circlePlusPixmap(const QColor& color, int size = 18) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color);
    pen.setWidthF(1.6);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    const int margin = 2;
    const QRectF r(margin, margin, size - margin * 2, size - margin * 2);
    p.drawEllipse(r);
    const double cx = size / 2.0;
    const double cy = size / 2.0;
    const double L = (size - margin * 2) * 0.34;
    p.drawLine(QPointF(cx - L, cy), QPointF(cx + L, cy));
    p.drawLine(QPointF(cx, cy - L), QPointF(cx, cy + L));
    return pm;
}

QPixmap pixFromDataUrl(const QString& dataUrl) {
    if (dataUrl.isEmpty()) return QPixmap();
    const int comma = dataUrl.indexOf(QLatin1Char(','));
    if (comma < 0) return QPixmap();
    const QByteArray raw = QByteArray::fromBase64(dataUrl.mid(comma + 1).toLatin1());
    QPixmap pm;
    pm.loadFromData(raw);
    return pm;
}

QPixmap photoRounded(const QPixmap& src, int side, int radius) {
    if (src.isNull()) return QPixmap();
    QPixmap scaled = src.scaled(side, side, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPixmap out(side, side);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(0, 0, side, side, radius, radius);
    p.setClipPath(path);
    const int dx = (scaled.width() - side) / 2;
    const int dy = (scaled.height() - side) / 2;
    p.drawPixmap(-dx, -dy, scaled);
    return out;
}

QString roleColor(const QString& role) {
    // Mira 1 usa um tom só (cyan) pra todos os papéis. Mantemos pra ficar
    // visualmente consistente; refina depois se Pedro pedir cor por papel.
    Q_UNUSED(role);
    return QStringLiteral("#7dd3fc");
}

} // namespace

DrawerListPanel::DrawerListPanel(ProjectModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_titleLabel(nullptr)
    , m_listLayout(nullptr)
    , m_scroll(nullptr)
    , m_pinBtn(nullptr)
    , m_viewBtn(nullptr)
    , m_sortBtn(nullptr)
    , m_createBtn(nullptr)
    , m_folderBtn(nullptr)
{
    setObjectName(QStringLiteral("drawerListPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    {
        const int stored = loadStoredWidth();
        const int w = stored > 0 ? qBound(kMinPanelWidth, stored, kMaxPanelWidth) : kPanelWidth;
        setMinimumWidth(kMinPanelWidth);
        setMaximumWidth(kMaxPanelWidth);
        resize(w, height());
    }
    setStyleSheet(Theme::panelQss(QStringLiteral("drawerListPanel")));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- Header ----
    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("drawerListHeader"));
    header->setStyleSheet(QStringLiteral(
        "#drawerListHeader { border-bottom: 1px solid %1; }").arg(Theme::panelBorder()));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 8, 10);
    headerLayout->setSpacing(4);

    m_titleLabel = new QLabel(tr("Gaveta"), header);
    m_titleLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Lora','Crimson Text',serif; font-size: 14px; font-weight: 600;")
        .arg(Theme::textBright()));
    m_titleLabel->setTextFormat(Qt::RichText);
    m_titleLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    connect(m_titleLabel, &QLabel::linkActivated, this, [this](const QString& link) {
        if (link == QStringLiteral("root")) m_currentFolderId.clear();
        else m_currentFolderId = link;
        rebuildContents();
    });
    headerLayout->addWidget(m_titleLabel, /*stretch=*/1);

    auto makeMiniBtn = [this](const QString& glyph, const QString& tip, bool checkable = false) {
        auto* b = new QToolButton(this);
        b->setText(glyph);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setCheckable(checkable);
        b->setAutoRaise(true);
        b->setMinimumHeight(24);
        b->setStyleSheet(miniIconBtnQss());
        return b;
    };

    m_pinBtn = makeMiniBtn(QStringLiteral("📌"), tr("Fixar painel"), /*checkable=*/true);
    m_pinBtn->setFixedWidth(28);
    connect(m_pinBtn, &QToolButton::toggled, this, [this](bool on) {
        m_pinned = on;
    });
    headerLayout->addWidget(m_pinBtn);

    m_viewBtn = makeMiniBtn(QStringLiteral("⊞"), tr("Alternar exibição"), /*checkable=*/false);
    m_viewBtn->setFixedWidth(28);
    connect(m_viewBtn, &QToolButton::clicked, this, [this]() {
        m_gridView = !m_gridView;
        updateViewButton();
        rebuildContents();
    });
    headerLayout->addWidget(m_viewBtn);

    m_sortBtn = makeMiniBtn(QStringLiteral("⇅ Criação ↑"), tr("Ordem de exibição"));
    m_sortBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(m_sortBtn, &QToolButton::clicked, this, [this]() {
        QMenu menu(this);
        menu.setStyleSheet(QStringLiteral(R"(
            QMenu {
                background: %1;
                color: %2;
                border: 1px solid %3;
                border-radius: 6px;
                padding: 4px;
            }
            QMenu::item { padding: 6px 14px; border-radius: 4px; font-size: 12px; }
            QMenu::item:selected { background: %4; color: %5; }
        )").arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
               Theme::hoverOverlay(), Theme::textBright()));

        auto addOpt = [&](const QString& label, SortMode mode, bool ascending) {
            QAction* act = menu.addAction(label);
            act->setCheckable(true);
            act->setChecked(m_sortMode == mode && m_sortAscending == ascending);
            connect(act, &QAction::triggered, this, [this, mode, ascending]() {
                m_sortMode = mode;
                m_sortAscending = ascending;
                updateSortButton();
                rebuildContents();
            });
        };
        addOpt(tr("Criação ↑"), SortCreation, true);
        addOpt(tr("Criação ↓"), SortCreation, false);
        addOpt(tr("A → Z"),     SortAlpha,    true);
        if (currentDrawerIsCharacter()) {
            addOpt(tr("Por papel"), SortRole, true);
        }
        menu.exec(m_sortBtn->mapToGlobal(QPoint(0, m_sortBtn->height() + 2)));
    });
    headerLayout->addWidget(m_sortBtn);

    auto* btnClose = makeMiniBtn(QStringLiteral("×"), tr("Fechar"));
    btnClose->setFixedWidth(28);
    btnClose->setStyleSheet(miniIconBtnQss());
    connect(btnClose, &QToolButton::clicked, this, &DrawerListPanel::closePanel);
    headerLayout->addWidget(btnClose);

    root->addWidget(header);

    // ---- Barra de ação (criar item + pasta) ----
    auto* actionBar = new QWidget(this);
    actionBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* actionLayout = new QVBoxLayout(actionBar);
    actionLayout->setContentsMargins(12, 12, 12, 10);
    actionLayout->setSpacing(8);

    m_createBtn = new QPushButton(actionBar);
    m_createBtn->setCursor(Qt::PointingHandCursor);
    m_createBtn->setIconSize(QSize(18, 18));
    m_createBtn->setMinimumHeight(40);
    connect(m_createBtn, &QPushButton::clicked, this, [this]() {
        if (!m_currentKey.isEmpty()) emit newItemRequested(m_currentKey, m_currentFolderId);
    });
    actionLayout->addWidget(m_createBtn);

    // Strip horizontal: chips de pastas + botão "+ Pasta", igual Mira 1.
    m_folderStrip = new QWidget(actionBar);
    m_folderStripLayout = new QHBoxLayout(m_folderStrip);
    m_folderStripLayout->setContentsMargins(0, 0, 0, 0);
    m_folderStripLayout->setSpacing(6);
    m_folderBtn = new QPushButton(QStringLiteral("+ Pasta"), m_folderStrip);
    m_folderBtn->setCursor(Qt::PointingHandCursor);
    m_folderBtn->setStyleSheet(folderPillQss());
    m_folderBtn->setMinimumHeight(24);
    connect(m_folderBtn, &QPushButton::clicked, this, [this]() {
        if (!m_currentKey.isEmpty()) emit newFolderRequested(m_currentKey, m_currentFolderId);
    });
    // Chips de pasta vão à esquerda; m_folderBtn fica logo depois; stretch enche o resto.
    m_folderStripLayout->addWidget(m_folderBtn, 0, Qt::AlignLeft);
    m_folderStripLayout->addStretch();
    actionLayout->addWidget(m_folderStrip);

    root->addWidget(actionBar);

    // ---- Scroll ----
    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName(QStringLiteral("drawerListScroll"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setStyleSheet(QStringLiteral("#drawerListScroll { background: transparent; }"));

    auto* listHost = new QWidget(m_scroll);
    listHost->setObjectName(QStringLiteral("drawerListHost"));
    listHost->setStyleSheet(QStringLiteral("background: transparent;"));
    m_listLayout = new QVBoxLayout(listHost);
    m_listLayout->setContentsMargins(12, 0, 12, 12);
    m_listLayout->setSpacing(8);
    m_listLayout->addStretch();
    m_scroll->setWidget(listHost);
    root->addWidget(m_scroll, /*stretch=*/1);

    if (m_model) {
        connect(m_model, &ProjectModel::drawersChanged, this, &DrawerListPanel::onDrawersChanged);
    }

    updateSortButton();
    updateViewButton();

    // ---- Resize handle (borda direita) ----
    m_resizeHandle = new QWidget(this);
    m_resizeHandle->setObjectName(QStringLiteral("drawerResizeHandle"));
    m_resizeHandle->setCursor(Qt::SizeHorCursor);
    m_resizeHandle->setStyleSheet(QStringLiteral(
        "#drawerResizeHandle { background: transparent; }"
        "#drawerResizeHandle:hover { background: rgba(255,255,255,0.10); }"));
    m_resizeHandle->setAttribute(Qt::WA_StyledBackground, true);
    m_resizeHandle->installEventFilter(this);
    m_resizeHandle->setGeometry(width() - kResizeHandleWidth, 0, kResizeHandleWidth, height());
    m_resizeHandle->raise();

    // ---- Resize handle (borda inferior) ----
    m_resizeHandleBottom = new QWidget(this);
    m_resizeHandleBottom->setObjectName(QStringLiteral("drawerResizeHandleBottom"));
    m_resizeHandleBottom->setCursor(Qt::SizeVerCursor);
    m_resizeHandleBottom->setStyleSheet(QStringLiteral(
        "#drawerResizeHandleBottom { background: transparent; }"
        "#drawerResizeHandleBottom:hover { background: rgba(255,255,255,0.10); }"));
    m_resizeHandleBottom->setAttribute(Qt::WA_StyledBackground, true);
    m_resizeHandleBottom->installEventFilter(this);
    m_resizeHandleBottom->setGeometry(0, height() - kResizeHandleWidth,
                                      width() - kResizeHandleWidth, kResizeHandleWidth);
    m_resizeHandleBottom->raise();

    // Restaurar altura salva, se houver
    {
        const int storedH = loadStoredHeight();
        if (storedH > 0) {
            m_heightUserSet = true;
            m_desiredHeight = qMax(kMinPanelHeight, storedH);
            resize(width(), m_desiredHeight);
        }
    }

    hide();
}

void DrawerListPanel::setElementsStore(ElementsStore* store) {
    m_elementsStore = store;
    if (store) {
        connect(store, &ElementsStore::changed, this, [this]() { if (isPanelOpen()) rebuildContents(); });
    }
}

void DrawerListPanel::openDrawer(const QString& drawerKey, const QString& folderId) {
    if (m_currentKey != drawerKey) {
        m_currentKey = drawerKey;
        m_currentFolderId.clear();
        // Reset view mode default por gaveta: grid pra element drawer, list pra genérica.
        m_gridView = currentDrawerIsElement();
    }
    if (!folderId.isEmpty()) {
        m_currentFolderId = folderId;
    }
    updateActionBar();
    updateSortButton();
    updateViewButton();
    rebuildFolderStrip();
    rebuildContents();
    show();
}

void DrawerListPanel::closePanel() {
    m_currentKey.clear();
    m_currentFolderId.clear();
    hide();
    emit panelClosed();
}

void DrawerListPanel::onDrawersChanged() {
    if (m_currentKey.isEmpty()) return;
    if (m_model && !m_model->findDrawer(m_currentKey)) {
        closePanel();
        return;
    }
    updateActionBar();
    rebuildFolderStrip();
    rebuildContents();
}

void DrawerListPanel::enterFolder(const QString& folderId) {
    m_currentFolderId = folderId;
    rebuildFolderStrip();
    rebuildContents();
}

void DrawerListPanel::goUpOneLevel() {
    if (m_currentFolderId.isEmpty() || !m_model) return;
    const Drawer* drawer = m_model->findDrawer(m_currentKey);
    if (!drawer) return;
    QString parent;
    for (const auto& f : drawer->folders) {
        if (f.id == m_currentFolderId) { parent = f.parentId; break; }
    }
    m_currentFolderId = parent;
    rebuildFolderStrip();
    rebuildContents();
}

void DrawerListPanel::updateBreadcrumb() {
    if (!m_model) return;
    const Drawer* drawer = m_model->findDrawer(m_currentKey);
    if (!drawer) return;

    const QString linkStyle    = QStringLiteral("color: %1; text-decoration: none;").arg(Theme::textMuted());
    const QString currentStyle = QStringLiteral("color: %1;").arg(Theme::textBright());
    const QString sepStyle     = QStringLiteral("color: %1;").arg(Theme::textMuted());

    QString html;
    const QString drawerTitle = drawer->title.isEmpty() ? tr("Gaveta") : drawer->title;

    if (m_currentFolderId.isEmpty()) {
        html = QStringLiteral("<span style='%1'>%2</span>").arg(currentStyle, drawerTitle.toHtmlEscaped());
    } else {
        html = QStringLiteral("<a href='root' style='%1'>%2</a>").arg(linkStyle, drawerTitle.toHtmlEscaped());
        const QStringList ancestors = ancestorFolderIds(m_currentFolderId);
        for (int i = ancestors.size() - 1; i >= 1; --i) {
            const QString fid = ancestors.at(i);
            html += QStringLiteral(" <span style='%1'>/</span> ").arg(sepStyle);
            html += QStringLiteral("<a href='%1' style='%2'>%3</a>")
                .arg(fid.toHtmlEscaped(), linkStyle, folderTitle(fid).toHtmlEscaped());
        }
        html += QStringLiteral(" <span style='%1'>/</span> ").arg(sepStyle);
        html += QStringLiteral("<span style='%1'>%2</span>")
            .arg(currentStyle, folderTitle(m_currentFolderId).toHtmlEscaped());
    }
    m_titleLabel->setText(html);
}

QString DrawerListPanel::createButtonLabel() const {
    if (!m_model || m_currentKey.isEmpty()) return tr("Criar novo documento");
    const Drawer* d = m_model->findDrawer(m_currentKey);
    if (!d) return tr("Criar novo documento");
    if (d->drawerElementType == QStringLiteral("character")) return tr("Criar novo personagem");
    if (d->drawerElementType == QStringLiteral("setting"))   return tr("Criar novo cenário");
    if (d->drawerElementType == QStringLiteral("object"))    return tr("Criar novo objeto");
    return tr("Criar novo documento");
}

QString DrawerListPanel::currentDrawerColor() const {
    if (!m_model || m_currentKey.isEmpty()) return Theme::accentDefault();
    const Drawer* d = m_model->findDrawer(m_currentKey);
    if (!d || d->color.isEmpty()) return Theme::accentDefault();
    return d->color;
}

bool DrawerListPanel::currentDrawerIsCharacter() const {
    if (!m_model || m_currentKey.isEmpty()) return false;
    const Drawer* d = m_model->findDrawer(m_currentKey);
    return d && d->drawerElementType == QStringLiteral("character");
}

bool DrawerListPanel::currentDrawerIsElement() const {
    if (!m_model || m_currentKey.isEmpty()) return false;
    const Drawer* d = m_model->findDrawer(m_currentKey);
    if (!d) return false;
    return d->drawerElementType == QStringLiteral("character")
        || d->drawerElementType == QStringLiteral("setting")
        || d->drawerElementType == QStringLiteral("object");
}

void DrawerListPanel::rebuildFolderStrip() {
    if (!m_folderStripLayout || !m_folderBtn) return;

    // Limpa todos os itens menos o m_folderBtn e o stretch final.
    QList<QLayoutItem*> toRemove;
    for (int i = 0; i < m_folderStripLayout->count(); ++i) {
        QLayoutItem* it = m_folderStripLayout->itemAt(i);
        if (!it) continue;
        QWidget* w = it->widget();
        if (w == m_folderBtn) continue;
        if (!w && it->spacerItem()) continue; // stretch
        toRemove.append(it);
    }
    for (QLayoutItem* it : toRemove) {
        m_folderStripLayout->removeItem(it);
        if (auto* w = it->widget()) w->deleteLater();
        delete it;
    }

    if (!m_model || m_currentKey.isEmpty()) {
        m_folderBtn->setVisible(true);
        return;
    }
    const Drawer* drawer = m_model->findDrawer(m_currentKey);
    if (!drawer) {
        m_folderBtn->setVisible(true);
        return;
    }

    // Ícone de pasta tintado com a cor do texto (mesmo pipeline da TopBar).
    static const QIcon folderIcon = IconUtils::loadToolbarIcon(
        QStringLiteral(":/icons/folder.svg"),
        QColor(Theme::textPrimary()),
        QColor(Theme::textBright()),
        QColor(Theme::textBright()),
        QSize(12, 12));

    if (!m_currentFolderId.isEmpty()) {
        // Dentro de uma pasta: mostra só o botão de voltar com o nome da pasta.
        const QString name = folderTitle(m_currentFolderId);
        auto* back = new QPushButton(QStringLiteral("◂  %1").arg(name.isEmpty() ? tr("Pasta") : name), m_folderStrip);
        back->setCursor(Qt::PointingHandCursor);
        back->setStyleSheet(folderBackQss());
        back->setMinimumHeight(24);
        connect(back, &QPushButton::clicked, this, [this]() {
            m_currentFolderId.clear();
            rebuildFolderStrip();
            rebuildContents();
        });
        // Insere antes do m_folderBtn (que ficamos invisível neste modo).
        const int idx = m_folderStripLayout->indexOf(m_folderBtn);
        m_folderStripLayout->insertWidget(qMax(0, idx), back, 0, Qt::AlignLeft);
        m_folderBtn->setVisible(false);
        return;
    }

    // Raiz: pastas filhas de "" como chips, e o m_folderBtn fica visível ao lado.
    m_folderBtn->setVisible(true);
    QList<Folder> roots;
    for (const auto& f : drawer->folders) {
        if (f.parentId.isEmpty()) roots.append(f);
    }
    std::sort(roots.begin(), roots.end(), [](const Folder& a, const Folder& b) {
        return QString::localeAwareCompare(a.title, b.title) < 0;
    });

    const int folderBtnIdx = m_folderStripLayout->indexOf(m_folderBtn);
    int insertAt = qMax(0, folderBtnIdx);
    for (const auto& f : roots) {
        const QString fid = f.id;
        auto* chip = new QPushButton(f.title, m_folderStrip);
        chip->setIcon(folderIcon);
        chip->setIconSize(QSize(12, 12));
        chip->setCursor(Qt::PointingHandCursor);
        chip->setStyleSheet(folderChipQss());
        chip->setMinimumHeight(24);
        chip->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(chip, &QPushButton::clicked, this, [this, fid]() {
            enterFolder(fid);
            rebuildFolderStrip();
        });
        connect(chip, &QPushButton::customContextMenuRequested, this, [this, chip, fid](const QPoint& pos) {
            showFolderContextMenu(fid, chip->mapToGlobal(pos));
        });
        installDropTargetOnFolderChip(chip, fid);
        m_folderStripLayout->insertWidget(insertAt++, chip, 0, Qt::AlignLeft);
    }
}

void DrawerListPanel::updateActionBar() {
    if (!m_createBtn) return;
    const QString accent = currentDrawerColor();
    m_createBtn->setText(QStringLiteral("  ") + createButtonLabel());
    m_createBtn->setIcon(QIcon(circlePlusPixmap(QColor(accent), 18)));
    m_createBtn->setStyleSheet(createButtonQss(accent));
}

void DrawerListPanel::updateSortButton() {
    if (!m_sortBtn) return;
    m_sortBtn->setText(QStringLiteral("⇅ %1").arg(sortLabelFor(m_sortMode, m_sortAscending)));
}

void DrawerListPanel::updateViewButton() {
    if (!m_viewBtn) return;
    // Só faz sentido alternar quando é element drawer.
    m_viewBtn->setVisible(currentDrawerIsElement());
    m_viewBtn->setText(m_gridView ? QStringLiteral("⊞") : QStringLiteral("☰"));
    m_viewBtn->setToolTip(m_gridView
        ? tr("Exibição: Blocos — clique para Lista")
        : tr("Exibição: Lista — clique para Blocos"));
}

void DrawerListPanel::rebuildContents() {
    if (!m_listLayout) return;

    while (m_listLayout->count() > 1) {
        QLayoutItem* item = m_listLayout->takeAt(0);
        if (auto* w = item->widget()) w->deleteLater();
        delete item;
    }

    if (!m_model || m_currentKey.isEmpty()) return;
    const Drawer* drawer = m_model->findDrawer(m_currentKey);
    if (!drawer) return;

    updateBreadcrumb();
    updateViewButton();

    int row = 0;
    if (!m_currentFolderId.isEmpty()) {
        auto* back = new QToolButton(this);
        back->setText(QStringLiteral("↑  %1").arg(tr("Voltar")));
        back->setCursor(Qt::PointingHandCursor);
        back->setMinimumHeight(28);
        back->setStyleSheet(QStringLiteral(R"(
            QToolButton {
                background: transparent;
                color: %1;
                border: 1px solid transparent;
                border-radius: 6px;
                padding: 4px 10px;
                text-align: left;
                font-family: 'Lora','Crimson Text',serif;
                font-size: 13px;
                font-style: italic;
            }
            QToolButton:hover { background: %2; color: %3; }
        )").arg(Theme::textMuted(), Theme::hoverOverlay(), Theme::textBright()));
        back->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(back, &QToolButton::clicked, this, &DrawerListPanel::goUpOneLevel);
        m_listLayout->insertWidget(row++, back);
    }

    int displayedCount = 0;

    // Pastas agora ficam no strip horizontal acima (rebuildFolderStrip),
    // não como linhas verticais.

    // Itens filtrados pelo folder atual.
    QList<DrawerItem> items;
    for (const auto& it : drawer->items) {
        if (it.folderId == m_currentFolderId) items.append(it);
    }

    // Resolve role efetivo (item.role; senão, do Element vinculado).
    auto effectiveRole = [this](const DrawerItem& it) -> QString {
        if (!it.role.isEmpty()) return it.role;
        if (m_elementsStore && !it.elementId.isEmpty()) {
            if (const Element* e = m_elementsStore->findElement(it.elementId)) return e->role;
        }
        return QString();
    };

    // Sort
    const SortMode mode = m_sortMode;
    const bool asc = m_sortAscending;
    if (mode == SortAlpha) {
        std::sort(items.begin(), items.end(), [](const DrawerItem& a, const DrawerItem& b) {
            return QString::localeAwareCompare(a.title, b.title) < 0;
        });
    } else if (mode == SortRole) {
        std::sort(items.begin(), items.end(), [&effectiveRole](const DrawerItem& a, const DrawerItem& b) {
            const QString ra = effectiveRole(a);
            const QString rb = effectiveRole(b);
            const int cmp = QString::localeAwareCompare(ra, rb);
            if (cmp != 0) return cmp < 0;
            return QString::localeAwareCompare(a.title, b.title) < 0;
        });
    } else if (!asc) {
        std::reverse(items.begin(), items.end());
    }

    const bool gridView = m_gridView && currentDrawerIsElement();
    if (gridView) {
        // Distribui os cards com stretches iguais antes, entre e depois — assim o
        // espaço extra (quando o painel é alargado) vira afastamento proporcional
        // entre os cards, deixando respiro pra linhas de vínculo entre eles.
        QHBoxLayout* currentRow = nullptr;
        int colInRow = 0;
        for (const auto& it : items) {
            if (colInRow == 0) {
                auto* rowWrap = new QWidget(this);
                rowWrap->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
                rowWrap->setFixedHeight(kCardHeight);
                currentRow = new QHBoxLayout(rowWrap);
                currentRow->setSpacing(0);
                currentRow->setContentsMargins(0, 0, 0, 0);
                currentRow->addStretch(1); // antes do primeiro
                m_listLayout->insertWidget(row++, rowWrap, /*stretch=*/0, Qt::AlignTop);
            }
            currentRow->addWidget(makeElementCard(it.id, it.title, effectiveRole(it), it.elementId));
            ++colInRow;
            ++displayedCount;
            if (colInRow < 2) {
                currentRow->addStretch(1); // entre os dois
            } else {
                currentRow->addStretch(1); // depois do segundo
                colInRow = 0;
                currentRow = nullptr;
            }
        }
        if (currentRow) {
            // Linha ímpar no fim — fecha com stretches pros lados ficarem balanceados.
            currentRow->addStretch(1);
            currentRow->addStretch(1);
        }
    } else {
        for (const auto& it : items) {
            m_listLayout->insertWidget(row++, makeRow(it.title, /*isFolder=*/false, it.id, effectiveRole(it)));
            ++displayedCount;
        }
    }

    if (displayedCount == 0) {
        m_listLayout->insertWidget(row++, makeEmptyState());
    }
}

QWidget* DrawerListPanel::makeElementCard(const QString& itemId, const QString& title, const QString& role, const QString& elementId)
{
    QString imageDataUrl;
    if (m_elementsStore && !elementId.isEmpty()) {
        if (const Element* e = m_elementsStore->findElement(elementId)) {
            imageDataUrl = e->image;
        }
    }
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("elemCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setCursor(Qt::PointingHandCursor);
    card->setContextMenuPolicy(Qt::CustomContextMenu);
    card->setFixedSize(kCardWidth, kCardHeight);
    card->setStyleSheet(QStringLiteral(R"(
        QFrame#elemCard {
            background: rgba(255,255,255,0.02);
            border: 1px solid rgba(255,255,255,0.08);
            border-radius: 8px;
        }
        QFrame#elemCard:hover {
            background: rgba(255,255,255,0.05);
            border-color: rgba(255,255,255,0.18);
        }
    )"));

    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(4);

    auto* photo = new QLabel(card);
    photo->setFixedSize(kCardPhoto, kCardPhoto);
    photo->setAlignment(Qt::AlignCenter);
    photo->setStyleSheet(QStringLiteral(
        "background: rgba(0,0,0,0.35); border-radius: 6px; color: %1; font-size: 9px;").arg(Theme::textMuted()));
    QPixmap pm = pixFromDataUrl(imageDataUrl);
    if (!pm.isNull()) {
        photo->setPixmap(photoRounded(pm, kCardPhoto, 6));
    } else {
        photo->setText(tr("sem foto"));
    }
    // Foto vira handle de drag: instala event filter para interceptar press/move.
    photo->setCursor(Qt::OpenHandCursor);
    photo->setProperty("itemId", itemId);
    photo->setProperty("isItemPhoto", true);
    photo->installEventFilter(this);
    lay->addWidget(photo, 0, Qt::AlignHCenter);

    auto* nameLbl = new QLabel(title.isEmpty() ? tr("(sem nome)") : title, card);
    nameLbl->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 12px; font-weight: 700;").arg(Theme::textBright()));
    nameLbl->setAlignment(Qt::AlignHCenter);
    nameLbl->setWordWrap(true);
    lay->addWidget(nameLbl);

    if (!role.isEmpty()) {
        auto* roleLbl = new QLabel(role.toUpper(), card);
        roleLbl->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 9px; font-weight: 800; letter-spacing: 0.8px;").arg(roleColor(role)));
        roleLbl->setAlignment(Qt::AlignHCenter);
        lay->addWidget(roleLbl);
    }
    lay->addStretch();

    const QString drawerKey = m_currentKey;
    card->installEventFilter(this);
    connect(card, &QWidget::customContextMenuRequested, this, [this, card, itemId](const QPoint& pos) {
        showItemContextMenu(itemId, card->mapToGlobal(pos));
    });
    card->setProperty("itemId", itemId);
    card->setProperty("drawerKey", drawerKey);

    installDropTargetOnCard(card, itemId);

    return card;
}

QWidget* DrawerListPanel::makeRow(const QString& label, bool isFolder, const QString& id, const QString& role) {
    auto* btn = new QToolButton(this);
    // Folder usa ícone "▸", item usa marcador discreto. Role aparece à direita em cinza.
    const QString glyph = isFolder ? QStringLiteral("▸") : QStringLiteral("·");
    QString text = QStringLiteral("%1  %2").arg(glyph, label);
    if (!isFolder && !role.isEmpty()) {
        text += QStringLiteral("   —  %1").arg(role.toUpper());
    }
    btn->setText(text);
    btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumHeight(30);
    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    btn->setStyleSheet(QStringLiteral(R"(
        QToolButton {
            background: transparent;
            color: %1;
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 4px 10px;
            text-align: left;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 13px;
        }
        QToolButton:hover {
            background: %2;
            color: %3;
            border-color: %4;
        }
    )").arg(Theme::textPrimary(), Theme::hoverOverlay(), Theme::textBright(), Theme::subtleBorder()));
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    const QString drawerKey = m_currentKey;
    connect(btn, &QToolButton::clicked, this, [this, isFolder, id, drawerKey]() {
        if (isFolder) enterFolder(id);
        else emit itemActivated(drawerKey, id);
    });
    connect(btn, &QToolButton::customContextMenuRequested, this, [this, btn, isFolder, id](const QPoint& pos) {
        if (isFolder) showFolderContextMenu(id, btn->mapToGlobal(pos));
        else showItemContextMenu(id, btn->mapToGlobal(pos));
    });
    return btn;
}

bool DrawerListPanel::eventFilter(QObject* watched, QEvent* event)
{
    auto* w = qobject_cast<QWidget*>(watched);

    // ---- Resize handle (borda direita / largura) ----
    if (w && w == m_resizeHandle) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_resizing = true;
                m_resizeStartGlobalX = me->globalPosition().toPoint().x();
                m_resizeStartWidth = width();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (m_resizing) {
                auto* me = static_cast<QMouseEvent*>(event);
                const int dx = me->globalPosition().toPoint().x() - m_resizeStartGlobalX;
                int newW = m_resizeStartWidth + dx;
                newW = qBound(kMinPanelWidth, newW, kMaxPanelWidth);
                if (newW != width()) {
                    resize(newW, height());
                    emit panelWidthChanged();
                }
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_resizing) {
                m_resizing = false;
                saveStoredWidth();
                return true;
            }
        }
    }

    // ---- Resize handle (borda inferior / altura) ----
    if (w && w == m_resizeHandleBottom) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_resizingV = true;
                m_resizeStartGlobalY = me->globalPosition().toPoint().y();
                m_resizeStartHeight = height();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (m_resizingV) {
                auto* me = static_cast<QMouseEvent*>(event);
                const int dy = me->globalPosition().toPoint().y() - m_resizeStartGlobalY;
                int newH = m_resizeStartHeight + dy;
                // Limite máximo: até onde o parent permite (- margem 10*2)
                int maxH = QWIDGETSIZE_MAX;
                if (parentWidget()) maxH = parentWidget()->height() - 20;
                newH = qBound(kMinPanelHeight, newH, maxH);
                if (newH != height()) {
                    m_heightUserSet = true;
                    m_desiredHeight = newH;
                    resize(width(), newH);
                    emit panelHeightChanged();
                }
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_resizingV) {
                m_resizingV = false;
                saveStoredHeight();
                return true;
            }
        }
    }

    // ---- Foto do card vira handle de drag ----
    if (w && w->property("isItemPhoto").toBool()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragStartPos = me->pos();
                m_pressedItemId = w->property("itemId").toString();
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            if ((me->buttons() & Qt::LeftButton) && !m_pressedItemId.isEmpty()) {
                if ((me->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
                    const QString itemId = m_pressedItemId;
                    m_pressedItemId.clear();
                    startItemDrag(w, itemId);
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            // Click curto na foto = abrir o item (mesmo comportamento do card).
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && !m_pressedItemId.isEmpty()) {
                const QString itemId = m_pressedItemId;
                m_pressedItemId.clear();
                emit itemActivated(m_currentKey, itemId);
                return true;
            }
            m_pressedItemId.clear();
        }
    }

    // ---- Click no card abre o item ----
    if (event->type() == QEvent::MouseButtonRelease) {
        if (w && w->objectName() == QStringLiteral("elemCard")) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                const QString itemId = w->property("itemId").toString();
                const QString drawerKey = w->property("drawerKey").toString();
                if (!itemId.isEmpty()) emit itemActivated(drawerKey, itemId);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

QWidget* DrawerListPanel::makeEmptyState() {
    auto* lbl = new QLabel(tr("Vazio. Use o botão acima pra criar."), this);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(QStringLiteral(
        "color: %1; font-style: italic; padding: 20px 12px;")
        .arg(Theme::textMuted()));
    lbl->setWordWrap(true);
    return lbl;
}

QString DrawerListPanel::folderTitle(const QString& folderId) const {
    if (!m_model || folderId.isEmpty()) return QString();
    const Drawer* drawer = m_model->findDrawer(m_currentKey);
    if (!drawer) return QString();
    for (const auto& f : drawer->folders) {
        if (f.id == folderId) return f.title;
    }
    return QString();
}

QStringList DrawerListPanel::ancestorFolderIds(const QString& folderId) const {
    QStringList chain;
    if (!m_model || folderId.isEmpty()) return chain;
    const Drawer* drawer = m_model->findDrawer(m_currentKey);
    if (!drawer) return chain;

    QString cursor = folderId;
    int safety = 0;
    while (!cursor.isEmpty() && safety++ < 64) {
        chain << cursor;
        bool found = false;
        for (const auto& f : drawer->folders) {
            if (f.id == cursor) { cursor = f.parentId; found = true; break; }
        }
        if (!found) break;
    }
    return chain;
}

void DrawerListPanel::showItemContextMenu(const QString& itemId, const QPoint& globalPos) {
    if (!m_model) return;
    const Drawer* drawer = m_model->findDrawer(m_currentKey);
    if (!drawer) return;
    const DrawerItem* item = m_model->findDrawerItem(itemId);
    if (!item) return;

    const QString drawerKey = m_currentKey;
    const bool hasElement = !item->elementType.isEmpty();

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(R"(
        QMenu {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item { padding: 6px 16px; border-radius: 4px; }
        QMenu::item:selected { background: %4; color: %5; }
        QMenu::item:disabled { color: %6; }
        QMenu::separator { height: 1px; background: %3; margin: 4px 6px; }
    )").arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
           Theme::hoverOverlay(), Theme::textBright(), Theme::textMuted()));

    auto* editAct = menu.addAction(tr("Editar metadados…"));
    connect(editAct, &QAction::triggered, this, [this, drawerKey, itemId]() {
        emit editItemRequested(drawerKey, itemId);
    });

    if (hasElement) {
        auto* removeElemAct = menu.addAction(tr("Remover elemento"));
        connect(removeElemAct, &QAction::triggered, this, [this, drawerKey, itemId]() {
            emit removeElementRequested(drawerKey, itemId);
        });
    } else {
        auto* addElemAct = menu.addAction(tr("Adicionar elemento…"));
        connect(addElemAct, &QAction::triggered, this, [this, drawerKey, itemId]() {
            emit addElementRequested(drawerKey, itemId);
        });
    }

    auto* markerAct = menu.addAction(tr("Adicionar marcador"));
    markerAct->setEnabled(false);
    markerAct->setToolTip(tr("Em breve"));

    auto* refMenuAct = menu.addAction(tr("Abrir no Menu de Referência"));
    connect(refMenuAct, &QAction::triggered, this, [this, drawerKey, itemId]() {
        emit openInRefMenuRequested(drawerKey, itemId);
    });

    menu.addSeparator();

    QMenu* moveMenu = menu.addMenu(tr("Mover para"));

    // ---- Dentro desta gaveta ----
    auto* hereLabel = moveMenu->addSection(drawer->title.isEmpty() ? tr("Esta gaveta") : drawer->title);
    Q_UNUSED(hereLabel);
    auto* rootAction = moveMenu->addAction(tr("Raiz da gaveta"));
    connect(rootAction, &QAction::triggered, this, [this, itemId]() {
        m_model->moveDrawerItem(m_currentKey, itemId, QString());
    });
    for (const auto& f : drawer->folders) {
        const QString fid = f.id;
        auto* a = moveMenu->addAction(QStringLiteral("📁 %1").arg(f.title));
        connect(a, &QAction::triggered, this, [this, itemId, fid]() {
            m_model->moveDrawerItem(m_currentKey, itemId, fid);
        });
    }

    // ---- Outras gavetas ----
    const QString srcKey = m_currentKey;
    bool firstOther = true;
    for (const auto& other : m_model->drawers()) {
        if (other.key == srcKey) continue;
        if (firstOther) {
            moveMenu->addSection(tr("Outras gavetas"));
            firstOther = false;
        }
        const QString destKey = other.key;
        const QString destTitle = other.title.isEmpty() ? tr("(sem nome)") : other.title;
        if (other.folders.isEmpty()) {
            auto* a = moveMenu->addAction(destTitle);
            connect(a, &QAction::triggered, this, [this, srcKey, itemId, destKey]() {
                m_model->moveDrawerItemToDrawer(srcKey, itemId, destKey, QString());
            });
        } else {
            QMenu* sub = moveMenu->addMenu(destTitle);
            auto* rootA = sub->addAction(tr("Raiz da gaveta"));
            connect(rootA, &QAction::triggered, this, [this, srcKey, itemId, destKey]() {
                m_model->moveDrawerItemToDrawer(srcKey, itemId, destKey, QString());
            });
            for (const auto& f : other.folders) {
                const QString fid = f.id;
                auto* a = sub->addAction(QStringLiteral("📁 %1").arg(f.title));
                connect(a, &QAction::triggered, this, [this, srcKey, itemId, destKey, fid]() {
                    m_model->moveDrawerItemToDrawer(srcKey, itemId, destKey, fid);
                });
            }
        }
    }

    menu.addSeparator();

    auto* deleteAct = menu.addAction(tr("Excluir"));
    connect(deleteAct, &QAction::triggered, this, [this, drawerKey, itemId]() {
        emit deleteItemRequested(drawerKey, itemId);
    });

    menu.exec(globalPos);
}

void DrawerListPanel::showFolderContextMenu(const QString& folderId, const QPoint& globalPos) {
    if (!m_model) return;
    const Drawer* drawer = m_model->findDrawer(m_currentKey);
    if (!drawer) return;

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(R"(
        QMenu {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item { padding: 6px 16px; border-radius: 4px; }
        QMenu::item:selected { background: %4; color: %5; }
    )").arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
           Theme::hoverOverlay(), Theme::textBright()));

    QMenu* moveMenu = menu.addMenu(tr("Mover pasta para"));
    auto* rootAction = moveMenu->addAction(tr("Raiz da gaveta"));
    connect(rootAction, &QAction::triggered, this, [this, folderId]() {
        m_model->moveDrawerFolder(m_currentKey, folderId, QString());
    });

    QStringList disallowed;
    disallowed << folderId;
    QStringList queue; queue << folderId;
    while (!queue.isEmpty()) {
        const QString cur = queue.takeFirst();
        for (const auto& f : drawer->folders) {
            if (f.parentId == cur) {
                disallowed << f.id;
                queue << f.id;
            }
        }
    }

    bool addedSeparator = false;
    for (const auto& f : drawer->folders) {
        if (disallowed.contains(f.id)) continue;
        if (!addedSeparator) { moveMenu->addSeparator(); addedSeparator = true; }
        const QString fid = f.id;
        auto* a = moveMenu->addAction(f.title);
        connect(a, &QAction::triggered, this, [this, folderId, fid]() {
            m_model->moveDrawerFolder(m_currentKey, folderId, fid);
        });
    }
    menu.exec(globalPos);
}

void DrawerListPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_resizeHandle) {
        m_resizeHandle->setGeometry(width() - kResizeHandleWidth, 0, kResizeHandleWidth, height());
        m_resizeHandle->raise();
    }
    if (m_resizeHandleBottom) {
        m_resizeHandleBottom->setGeometry(0, height() - kResizeHandleWidth,
                                          width() - kResizeHandleWidth, kResizeHandleWidth);
        m_resizeHandleBottom->raise();
    }
    // Os stretches do row se ajustam automaticamente — não precisa rebuild,
    // o QHBoxLayout redistribui o espaço extra entre/ao redor dos cards.
}

void DrawerListPanel::saveStoredWidth() {
    QSettings settings;
    settings.setValue(QStringLiteral("ui/drawerListPanelWidth"), width());
}

int DrawerListPanel::loadStoredWidth() const {
    QSettings settings;
    return settings.value(QStringLiteral("ui/drawerListPanelWidth"), 0).toInt();
}

void DrawerListPanel::saveStoredHeight() {
    QSettings settings;
    // Salva a altura DESEJADA (não o height() atual, que pode ter sido encolhido
    // por show/hide reaplicando o sizeHint do layout interno).
    settings.setValue(QStringLiteral("ui/drawerListPanelHeight"),
                      m_desiredHeight > 0 ? m_desiredHeight : height());
}

int DrawerListPanel::loadStoredHeight() const {
    QSettings settings;
    return settings.value(QStringLiteral("ui/drawerListPanelHeight"), 0).toInt();
}

void DrawerListPanel::startItemDrag(QWidget* photoLabel, const QString& itemId) {
    if (!photoLabel || itemId.isEmpty()) return;
    QDrag* drag = new QDrag(photoLabel);
    auto* mime = new QMimeData();
    // Mime carrega "drawerKey|itemId" — drop targets sabem da origem.
    const QByteArray payload = (m_currentKey + QLatin1Char('|') + itemId).toUtf8();
    mime->setData(QLatin1String(kItemMime), payload);
    drag->setMimeData(mime);
    const QPixmap pm = photoLabel->grab();
    drag->setPixmap(pm);
    drag->setHotSpot(QPoint(pm.width() / 2, pm.height() / 2));
    drag->exec(Qt::MoveAction);
    clearItemDropIndicator();
}

void DrawerListPanel::clearItemDropIndicator() {
    if (m_dropIndicator) {
        m_dropIndicator->hide();
        m_dropIndicator->deleteLater();
        m_dropIndicator = nullptr;
    }
}

void DrawerListPanel::showItemDropIndicatorAt(QWidget* targetCard, bool before) {
    if (!targetCard) return;
    if (!m_dropIndicator) {
        // Indicador é uma linha fina filha do mesmo parent do card.
        m_dropIndicator = new QWidget(targetCard->parentWidget());
        m_dropIndicator->setFixedHeight(2);
        m_dropIndicator->setStyleSheet(QStringLiteral(
            "background: %1; border-radius: 1px;").arg(Theme::accentDefault()));
        m_dropIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
    } else if (m_dropIndicator->parentWidget() != targetCard->parentWidget()) {
        m_dropIndicator->setParent(targetCard->parentWidget());
    }
    const int y = before ? targetCard->y() - 2 : targetCard->y() + targetCard->height();
    m_dropIndicator->setGeometry(targetCard->x(), y, targetCard->width(), 2);
    m_dropIndicator->raise();
    m_dropIndicator->show();
}

// Decompõe payload "drawerKey|itemId" do MIME.
static bool parseItemPayload(const QByteArray& payload, QString& drawerKey, QString& itemId) {
    const QString s = QString::fromUtf8(payload);
    const int sep = s.indexOf(QLatin1Char('|'));
    if (sep < 0) return false;
    drawerKey = s.left(sep);
    itemId = s.mid(sep + 1);
    return !drawerKey.isEmpty() && !itemId.isEmpty();
}

void DrawerListPanel::installDropTargetOnCard(QWidget* card, const QString& targetItemId) {
    card->setAcceptDrops(true);
    // Filtro de eventos pra responder a drag enter/move/leave/drop sem subclassar.
    class CardDropFilter : public QObject {
    public:
        CardDropFilter(DrawerListPanel* panel, QWidget* card, QString targetItemId)
            : QObject(card), m_panel(panel), m_card(card), m_targetItemId(std::move(targetItemId)) {}
    protected:
        bool eventFilter(QObject* obj, QEvent* ev) override {
            if (obj != m_card) return QObject::eventFilter(obj, ev);
            // Só aceita drag de item se sort = SortCreation (reorder)
            //   OU dst é card de outra pasta (mover entre pastas dentro da mesma gaveta).
            // Pra simplificar: aceitamos só reorder em SortCreation. Mover entre pastas
            // só funciona soltando no chip da pasta.
            if (ev->type() == QEvent::DragEnter) {
                auto* de = static_cast<QDragEnterEvent*>(ev);
                if (!de->mimeData()->hasFormat(QLatin1String(kItemMime))) return false;
                if (m_panel->m_sortMode != DrawerListPanel::SortCreation) return false;
                QString srcKey, srcId; parseItemPayload(de->mimeData()->data(QLatin1String(kItemMime)), srcKey, srcId);
                if (srcKey != m_panel->m_currentKey) return false; // só dentro da mesma gaveta
                if (srcId == m_targetItemId) return false;
                de->acceptProposedAction();
                return true;
            } else if (ev->type() == QEvent::DragMove) {
                auto* de = static_cast<QDragMoveEvent*>(ev);
                if (!de->mimeData()->hasFormat(QLatin1String(kItemMime))) return false;
                const bool before = de->position().y() < m_card->height() / 2;
                m_panel->showItemDropIndicatorAt(m_card, before);
                de->acceptProposedAction();
                return true;
            } else if (ev->type() == QEvent::DragLeave) {
                m_panel->clearItemDropIndicator();
                return true;
            } else if (ev->type() == QEvent::Drop) {
                auto* de = static_cast<QDropEvent*>(ev);
                if (!de->mimeData()->hasFormat(QLatin1String(kItemMime))) return false;
                QString srcKey, srcId; parseItemPayload(de->mimeData()->data(QLatin1String(kItemMime)), srcKey, srcId);
                m_panel->clearItemDropIndicator();
                if (srcKey != m_panel->m_currentKey || srcId == m_targetItemId || !m_panel->m_model) {
                    return false;
                }
                // Localiza índice do alvo no array geral da gaveta.
                const Drawer* d = m_panel->m_model->findDrawer(m_panel->m_currentKey);
                if (!d) return false;
                int targetIdx = -1;
                for (int i = 0; i < d->items.size(); ++i) {
                    if (d->items.at(i).id == m_targetItemId) { targetIdx = i; break; }
                }
                if (targetIdx < 0) return false;
                const bool before = de->position().y() < m_card->height() / 2;
                if (!before) targetIdx += 1;
                m_panel->m_model->reorderDrawerItem(m_panel->m_currentKey, srcId, targetIdx);
                de->acceptProposedAction();
                return true;
            }
            return false;
        }
    private:
        DrawerListPanel* m_panel;
        QWidget* m_card;
        QString m_targetItemId;
    };
    card->installEventFilter(new CardDropFilter(this, card, targetItemId));
}

void DrawerListPanel::installDropTargetOnFolderChip(QWidget* chip, const QString& folderId) {
    chip->setAcceptDrops(true);
    class ChipDropFilter : public QObject {
    public:
        ChipDropFilter(DrawerListPanel* panel, QWidget* chip, QString folderId)
            : QObject(chip), m_panel(panel), m_chip(chip), m_folderId(std::move(folderId)) {}
    protected:
        bool eventFilter(QObject* obj, QEvent* ev) override {
            if (obj != m_chip) return QObject::eventFilter(obj, ev);
            if (ev->type() == QEvent::DragEnter) {
                auto* de = static_cast<QDragEnterEvent*>(ev);
                if (!de->mimeData()->hasFormat(QLatin1String(kItemMime))) return false;
                QString srcKey, srcId; parseItemPayload(de->mimeData()->data(QLatin1String(kItemMime)), srcKey, srcId);
                if (srcKey != m_panel->m_currentKey) return false;
                de->acceptProposedAction();
                m_chip->setProperty("dropHover", true);
                m_chip->setStyleSheet(m_chip->styleSheet() + QStringLiteral(
                    "QPushButton { background: rgba(255,255,255,0.18); border-color: rgba(255,255,255,0.32); }"));
                return true;
            } else if (ev->type() == QEvent::DragMove) {
                auto* de = static_cast<QDragMoveEvent*>(ev);
                if (!de->mimeData()->hasFormat(QLatin1String(kItemMime))) return false;
                de->acceptProposedAction();
                return true;
            } else if (ev->type() == QEvent::DragLeave) {
                if (m_chip->property("dropHover").toBool()) {
                    m_chip->setProperty("dropHover", false);
                    m_chip->style()->unpolish(m_chip);
                    m_chip->style()->polish(m_chip);
                }
                return true;
            } else if (ev->type() == QEvent::Drop) {
                auto* de = static_cast<QDropEvent*>(ev);
                if (!de->mimeData()->hasFormat(QLatin1String(kItemMime))) return false;
                QString srcKey, srcId; parseItemPayload(de->mimeData()->data(QLatin1String(kItemMime)), srcKey, srcId);
                if (srcKey != m_panel->m_currentKey || !m_panel->m_model) return false;
                m_panel->m_model->moveDrawerItem(m_panel->m_currentKey, srcId, m_folderId);
                de->acceptProposedAction();
                return true;
            }
            return false;
        }
    private:
        DrawerListPanel* m_panel;
        QWidget* m_chip;
        QString m_folderId;
    };
    chip->installEventFilter(new ChipDropFilter(this, chip, folderId));
}
