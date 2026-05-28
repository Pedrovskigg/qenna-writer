#include "DrawerListPanel.h"
#include "BondsLayer.h"
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
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressBar>
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

const QStringList kPresetStatuses = {
    QStringLiteral("Morto"),       QStringLiteral("Desaparecido"), QStringLiteral("Ferido"),
    QStringLiteral("Curado"),      QStringLiteral("Apaixonado"),   QStringLiteral("Raivoso"),
    QStringLiteral("Feliz"),       QStringLiteral("Triste"),       QStringLiteral("Confuso"),
    QStringLiteral("Traído"),      QStringLiteral("Com medo"),     QStringLiteral("Em fuga"),
    QStringLiteral("Preso"),       QStringLiteral("Transformado"), QStringLiteral("Aliviado"),
    QStringLiteral("Perdido"),
};

QString pickerPopupQss() {
    return QStringLiteral(R"(
        QFrame#consistencyPicker {
            background: %1;
            border: 1px solid %2;
            border-radius: 8px;
        }
        QPushButton#pickerOption {
            background: transparent;
            color: %3;
            border: none;
            border-radius: 5px;
            padding: 5px 10px;
            text-align: left;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 12px;
        }
        QPushButton#pickerOption:hover { background: %4; color: %5; }
        QPushButton#pickerOptionClear {
            background: transparent;
            color: %6;
            border: none;
            border-radius: 5px;
            padding: 5px 10px;
            text-align: left;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 11px;
            font-style: italic;
        }
        QPushButton#pickerOptionClear:hover { background: %4; color: %7; }
        QLineEdit#pickerInput {
            background: %8;
            color: %3;
            border: 1px solid %2;
            border-radius: 5px;
            padding: 4px 8px;
            font-size: 12px;
        }
    )").arg(Theme::panelBackground(), Theme::panelBorder(),
            Theme::textPrimary(), Theme::hoverOverlay(), Theme::textBright(),
            Theme::textMuted(), Theme::accentDanger(), Theme::inputBackground());
}

} // namespace (presets + picker QSS)

namespace {

struct CardSizeParams {
    int cardW;
    int cardH;
    int cardHConsistency;
    int cardPhoto;
    int cols;
};
static const CardSizeParams kCardSizes[3] = {
    {  90,  90, 145,  48, 3 },  // 0 = Small
    { 134, 118, 192,  70, 2 },  // 1 = Medium (default)
    { 134, 155, 245,  92, 2 },  // 2 = Large (foto maior, card mais alto, 2 colunas)
};

constexpr int kPanelWidth = 300;
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
            background: %3;
            color: %1;
            border: 1px solid %4;
            border-radius: 11px;
            padding: 2px 10px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 11px;
            text-align: left;
        }
        QPushButton:hover {
            background: %5;
            color: %2;
            border-color: %6;
        }
    )").arg(Theme::textPrimary(), Theme::textBright(),
           Theme::hoverOverlay(), Theme::subtleBorder(),
           Theme::hoverStrong(), Theme::borderStrong());
}

QString folderBackQss() {
    // Botão "voltar" exibido quando se está dentro de uma pasta.
    return QStringLiteral(R"(
        QPushButton {
            background: %3;
            color: %1;
            border: 1px solid %4;
            border-radius: 11px;
            padding: 2px 10px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 11px;
            text-align: left;
        }
        QPushButton:hover {
            background: %5;
            color: %2;
            border-color: %6;
        }
    )").arg(Theme::textBright(), Theme::textBright(),
           Theme::hoverOverlay(), Theme::subtleBorder(),
           Theme::hoverStrong(), Theme::focusBorder());
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
    // Mira 1 usa um tom só (cyan) pra todos os papéis. Aqui usamos accentInfo()
    // pra ter contraste tanto no tema escuro quanto no claro.
    Q_UNUSED(role);
    return Theme::accentInfo();
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
    , m_sizeBtn(nullptr)
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
    {
        QSettings qs;
        m_cardSizeIdx = qBound(0, qs.value(QStringLiteral("ui/cardSizeIdx"), 1).toInt(), 2);
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

    // Botão de modo consistência — só visível em gavetas de personagem
    m_consistencyBtn = new QToolButton(this);
    m_consistencyBtn->setToolTip(tr("Modo consistência narrativa"));
    m_consistencyBtn->setCursor(Qt::PointingHandCursor);
    m_consistencyBtn->setCheckable(true);
    m_consistencyBtn->setAutoRaise(true);
    m_consistencyBtn->setMinimumHeight(24);
    m_consistencyBtn->setFixedWidth(28);
    m_consistencyBtn->setStyleSheet(miniIconBtnQss());
    m_consistencyBtn->setVisible(false);
    {
        QIcon ic = IconUtils::loadToolbarIcon(
            QStringLiteral(":/icons/leftbar/consistency.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textPrimary()),
            QColor(Theme::textBright()), QSize(16, 16));
        if (!ic.isNull()) {
            m_consistencyBtn->setIcon(ic);
            m_consistencyBtn->setIconSize(QSize(16, 16));
            m_consistencyBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        } else {
            m_consistencyBtn->setText(QStringLiteral("C"));
            m_consistencyBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        }
    }
    connect(m_consistencyBtn, &QToolButton::toggled, this, [this](bool on) {
        m_consistencyMode = on;
        if (on) refreshPresenceCache();
        rebuildContents();
        emit consistencyModeChanged(on);
    });
    headerLayout->addWidget(m_consistencyBtn);

    m_viewBtn = makeMiniBtn(QStringLiteral("⊞"), tr("Alternar exibição"), /*checkable=*/false);
    m_viewBtn->setFixedWidth(28);
    connect(m_viewBtn, &QToolButton::clicked, this, [this]() {
        m_gridView = !m_gridView;
        updateViewButton();
        rebuildContents();
    });
    headerLayout->addWidget(m_viewBtn);

    m_sizeBtn = makeMiniBtn(QStringLiteral("M"), tr("Tamanho dos cards"));
    m_sizeBtn->setFixedWidth(28);
    m_sizeBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(m_sizeBtn, &QToolButton::clicked, this, [this]() {
        m_cardSizeIdx = (m_cardSizeIdx + 1) % 3;
        QSettings qs;
        qs.setValue(QStringLiteral("ui/cardSizeIdx"), m_cardSizeIdx);
        updateSizeButton();
        rebuildContents();
    });
    headerLayout->addWidget(m_sizeBtn);

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

    m_listHost = new QWidget(m_scroll);
    m_listHost->setObjectName(QStringLiteral("drawerListHost"));
    m_listHost->setStyleSheet(QStringLiteral("background: transparent;"));
    m_listLayout = new QVBoxLayout(m_listHost);
    m_listLayout->setContentsMargins(12, 0, 12, 12);
    m_listLayout->setSpacing(8);
    m_listLayout->addStretch();
    m_scroll->setWidget(m_listHost);
    root->addWidget(m_scroll, /*stretch=*/1);

    // Overlay de vínculos — desenha por cima dos rowWraps (transparente p/ mouse).
    m_bondsLayer = new BondsLayer(m_listHost);
    m_bondsLayer->raise();
    m_listHost->setMouseTracking(true);
    m_listHost->installEventFilter(this);

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
        "#drawerResizeHandle:hover { background: %1; }").arg(Theme::subtleBorder()));
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
        "#drawerResizeHandleBottom:hover { background: %1; }").arg(Theme::subtleBorder()));
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

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &DrawerListPanel::applyTheme);

    hide();
}

void DrawerListPanel::applyTheme() {
    setStyleSheet(Theme::panelQss(QStringLiteral("drawerListPanel")));
    // Os elementos internos (header, cards, chips, action bar) são reconstruídos
    // ao reabrir o painel — basta reaplicar a base do painel pro background/border
    // novo aparecerem imediatamente.
    if (isPanelOpen()) rebuildContents();
}

void DrawerListPanel::setElementsStore(ElementsStore* store) {
    m_elementsStore = store;
    if (store) {
        connect(store, &ElementsStore::changed, this, [this]() {
            if (!isPanelOpen()) return;
            if (m_consistencyMode) refreshPresenceCache();
            rebuildContents();
        });
    }
}

void DrawerListPanel::openDrawer(const QString& drawerKey, const QString& folderId) {
    if (m_currentKey != drawerKey) {
        m_currentKey = drawerKey;
        m_currentFolderId.clear();
        // Reset view mode default por gaveta: grid pra element drawer, list pra genérica.
        m_gridView = currentDrawerIsElement();
        // Reset modo consistência ao trocar de gaveta
        if (m_consistencyMode) {
            m_consistencyMode = false;
            if (m_consistencyBtn) {
                m_consistencyBtn->blockSignals(true);
                m_consistencyBtn->setChecked(false);
                m_consistencyBtn->blockSignals(false);
            }
            emit consistencyModeChanged(false);
        }
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

void DrawerListPanel::openInConsistencyMode(const QString& drawerKey) {
    // Se drawerKey foi especificada, usa ela; senão, procura a primeira gaveta de personagem
    QString targetKey = drawerKey;
    if (targetKey.isEmpty() && m_model) {
        for (const auto& d : m_model->drawers()) {
            if (d.drawerElementType == QStringLiteral("character")) {
                targetKey = d.key;
                break;
            }
        }
    }
    if (targetKey.isEmpty()) return;

    // Abre a gaveta (resetará consistency se era outro drawer)
    openDrawer(targetKey);

    // Ativa modo consistência
    if (!m_consistencyMode) {
        m_consistencyMode = true;
        if (m_consistencyBtn) {
            m_consistencyBtn->blockSignals(true);
            m_consistencyBtn->setChecked(true);
            m_consistencyBtn->blockSignals(false);
        }
        refreshPresenceCache();
        rebuildContents();
        emit consistencyModeChanged(true);
    }
}

void DrawerListPanel::refreshPresenceCache() {
    m_presenceResults.clear();
    m_totalScenes   = 0;
    m_totalChapters = 0;
    if (!m_model || !m_presenceProvider) return;

    const Drawer* drawer = m_model->findDrawer(m_currentKey);
    if (!drawer) return;

    QStringList names;
    for (const auto& it : drawer->items) {
        if (!it.title.isEmpty()) names.append(it.title);
    }
    if (names.isEmpty()) return;

    m_presenceProvider(names, &m_presenceResults, &m_totalScenes, &m_totalChapters);
}

void DrawerListPanel::setCurrentDocKey(const QString& key)
{
    if (m_currentDocKey == key) return;
    m_currentDocKey = key;
    if (m_consistencyMode && isPanelOpen()) rebuildContents();
}

void DrawerListPanel::showStatusPicker(const QString& itemId, const QPoint& globalPos) {
    if (!m_model) return;
    const DrawerItem* item = m_model->findDrawerItem(itemId);
    if (!item) return;

    const QString currentStatus = item->charStatus;
    const QString currentDetail = item->charStatusDetail;
    const QString currentLocation = item->charLocation;

    auto* popup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setObjectName(QStringLiteral("consistencyPicker"));
    popup->setStyleSheet(pickerPopupQss());
    popup->setFixedWidth(180);

    auto* lay = new QVBoxLayout(popup);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(2);

    // Campo de input personalizado
    auto* customInput = new QLineEdit(popup);
    customInput->setObjectName(QStringLiteral("pickerInput"));
    customInput->setPlaceholderText(tr("Status personalizado..."));
    customInput->setFixedHeight(28);
    lay->addWidget(customInput);

    // Separador
    auto* sep = new QFrame(popup);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("background: %1; border: none; margin: 2px 0;").arg(Theme::subtleBorder()));
    sep->setFixedHeight(1);
    lay->addWidget(sep);

    // Limpar status (se tiver)
    if (!currentStatus.isEmpty()) {
        auto* clearBtn = new QPushButton(tr("Limpar status"), popup);
        clearBtn->setObjectName(QStringLiteral("pickerOptionClear"));
        clearBtn->setFixedHeight(26);
        clearBtn->setCursor(Qt::PointingHandCursor);
        connect(clearBtn, &QPushButton::clicked, popup, [this, popup, itemId, currentLocation]() {
            emit consistencyUpdateRequested(itemId, QString(), QString(), currentLocation);
            popup->close();
        });
        lay->addWidget(clearBtn);
    }

    // Presets
    for (const auto& s : kPresetStatuses) {
        auto* btn = new QPushButton(s, popup);
        btn->setObjectName(QStringLiteral("pickerOption"));
        btn->setFixedHeight(26);
        btn->setCursor(Qt::PointingHandCursor);
        if (s == currentStatus) {
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; color: %2; border: none; border-radius: 5px; "
                "padding: 0 10px; text-align: left; font-size: 12px; "
                "font-family: 'Lora','Crimson Text',serif; }")
                .arg(Theme::hoverOverlay(), Theme::accentDefault()));
        }
        connect(btn, &QPushButton::clicked, popup, [this, popup, itemId, s, currentLocation]() {
            emit consistencyUpdateRequested(itemId, s, QString(), currentLocation);
            popup->close();
        });
        lay->addWidget(btn);
    }

    // Confirma campo personalizado
    connect(customInput, &QLineEdit::returnPressed, popup, [this, popup, customInput, itemId, currentLocation]() {
        const QString val = customInput->text().trimmed();
        if (!val.isEmpty()) {
            emit consistencyUpdateRequested(itemId, val, QString(), currentLocation);
            popup->close();
        }
    });

    popup->adjustSize();
    // Posiciona para não sair da tela
    QPoint pos = globalPos;
    const QRect screen = popup->screen() ? popup->screen()->availableGeometry() : QRect(0, 0, 1920, 1080);
    if (pos.x() + popup->width() > screen.right())  pos.setX(screen.right() - popup->width());
    if (pos.y() + popup->height() > screen.bottom()) pos.setY(globalPos.y() - popup->height() - 30);
    popup->move(pos);
    popup->show();
    customInput->setFocus();
}

void DrawerListPanel::showLocationPicker(const QString& itemId, const QPoint& globalPos) {
    if (!m_model) return;
    const DrawerItem* item = m_model->findDrawerItem(itemId);
    if (!item) return;

    const QString currentStatus = item->charStatus;
    const QString currentDetail = item->charStatusDetail;
    const QString currentLocation = item->charLocation;

    // Coleta cenários de todas as gavetas com drawerElementType == "setting"
    QStringList settingNames;
    for (const auto& d : m_model->drawers()) {
        if (d.drawerElementType == QStringLiteral("setting")) {
            for (const auto& it : d.items) {
                if (!it.title.isEmpty()) settingNames.append(it.title);
            }
        }
    }

    auto* popup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setObjectName(QStringLiteral("consistencyPicker"));
    popup->setStyleSheet(pickerPopupQss());
    popup->setFixedWidth(180);

    auto* lay = new QVBoxLayout(popup);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(2);

    // Campo personalizado
    auto* customInput = new QLineEdit(popup);
    customInput->setObjectName(QStringLiteral("pickerInput"));
    customInput->setPlaceholderText(tr("Local personalizado..."));
    customInput->setFixedHeight(28);
    lay->addWidget(customInput);

    // Separador
    auto* sep = new QFrame(popup);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("background: %1; border: none; margin: 2px 0;").arg(Theme::subtleBorder()));
    sep->setFixedHeight(1);
    lay->addWidget(sep);

    // Limpar local (se tiver)
    if (!currentLocation.isEmpty()) {
        auto* clearBtn = new QPushButton(tr("Limpar local"), popup);
        clearBtn->setObjectName(QStringLiteral("pickerOptionClear"));
        clearBtn->setFixedHeight(26);
        clearBtn->setCursor(Qt::PointingHandCursor);
        connect(clearBtn, &QPushButton::clicked, popup, [this, popup, itemId, currentStatus, currentDetail]() {
            emit consistencyUpdateRequested(itemId, currentStatus, currentDetail, QString());
            popup->close();
        });
        lay->addWidget(clearBtn);
    }

    if (settingNames.isEmpty()) {
        auto* emptyLbl = new QLabel(tr("Nenhum cenário criado."), popup);
        emptyLbl->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; padding: 4px 10px; font-style: italic;").arg(Theme::textMuted()));
        lay->addWidget(emptyLbl);
    } else {
        for (const auto& s : settingNames) {
            auto* btn = new QPushButton(s, popup);
            btn->setObjectName(QStringLiteral("pickerOption"));
            btn->setFixedHeight(26);
            btn->setCursor(Qt::PointingHandCursor);
            if (s == currentLocation) {
                btn->setStyleSheet(QStringLiteral(
                    "QPushButton { background: %1; color: %2; border: none; border-radius: 5px; "
                    "padding: 0 10px; text-align: left; font-size: 12px; "
                    "font-family: 'Lora','Crimson Text',serif; }")
                    .arg(Theme::hoverOverlay(), Theme::accentDefault()));
            }
            connect(btn, &QPushButton::clicked, popup, [this, popup, itemId, s, currentStatus, currentDetail]() {
                emit consistencyUpdateRequested(itemId, currentStatus, currentDetail, s);
                popup->close();
            });
            lay->addWidget(btn);
        }
    }

    connect(customInput, &QLineEdit::returnPressed, popup, [this, popup, customInput, itemId, currentStatus, currentDetail]() {
        const QString val = customInput->text().trimmed();
        if (!val.isEmpty()) {
            emit consistencyUpdateRequested(itemId, currentStatus, currentDetail, val);
            popup->close();
        }
    });

    popup->adjustSize();
    QPoint pos = globalPos;
    const QRect screen = popup->screen() ? popup->screen()->availableGeometry() : QRect(0, 0, 1920, 1080);
    if (pos.x() + popup->width() > screen.right())  pos.setX(screen.right() - popup->width());
    if (pos.y() + popup->height() > screen.bottom()) pos.setY(globalPos.y() - popup->height() - 30);
    popup->move(pos);
    popup->show();
    customInput->setFocus();
}

void DrawerListPanel::showPresenceDetail(const QString& charName,
                                         const CharPresenceResult& res,
                                         QPoint globalPos)
{
    if (m_presenceDetailPopup) {
        m_presenceDetailPopup->close();
        m_presenceDetailPopup = nullptr;
    }

    auto* popup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setObjectName(QStringLiteral("presenceDetail"));
    popup->setStyleSheet(QStringLiteral(R"(
        QFrame#presenceDetail {
            background: %1;
            border: 1px solid %2;
            border-radius: 8px;
        }
        QLabel#pdTitle { color: %3; font-size: 10px; font-weight: 700; letter-spacing: 0.4px; }
        QLabel#pdEntry { color: %4; font-size: 11px; padding: 2px 0; }
        QLabel#pdEmpty { color: %3; font-style: italic; font-size: 11px; }
        QPushButton#pdModeBtn {
            background: transparent; color: %4;
            border: none; border-radius: 4px;
            padding: 3px 8px; text-align: left; font-size: 11px;
        }
        QPushButton#pdModeBtn:hover { background: %5; }
        QPushButton#pdModeBtnSel {
            background: %5; color: %6;
            border: none; border-radius: 4px;
            padding: 3px 8px; text-align: left; font-size: 11px; font-weight: 600;
        }
    )").arg(Theme::panelBackground(), Theme::panelBorder(),
            Theme::textMuted(), Theme::textPrimary(),
            Theme::hoverOverlay(), Theme::textBright()));
    popup->setFixedWidth(230);

    auto* outerLay = new QVBoxLayout(popup);
    outerLay->setContentsMargins(10, 8, 10, 10);
    outerLay->setSpacing(5);

    // Header: nome + gear
    auto* headerRow = new QHBoxLayout();
    headerRow->setSpacing(4);
    auto* titleLbl = new QLabel(charName.toUpper() + QStringLiteral(" — APARECE EM"), popup);
    titleLbl->setObjectName(QStringLiteral("pdTitle"));
    headerRow->addWidget(titleLbl);
    headerRow->addStretch();

    auto* gearBtn = new QToolButton(popup);
    gearBtn->setText(QStringLiteral("⚙"));
    gearBtn->setStyleSheet(QStringLiteral(
        "QToolButton { background: transparent; color: %1; border: none; font-size: 13px; }"
        "QToolButton:hover { color: %2; }").arg(Theme::textMuted(), Theme::textPrimary()));
    gearBtn->setCursor(Qt::PointingHandCursor);
    gearBtn->setFixedSize(20, 20);
    headerRow->addWidget(gearBtn);
    outerLay->addLayout(headerRow);

    // Painel de modo (toggle via gear)
    auto* modePanel = new QWidget(popup);
    auto* modeLay = new QVBoxLayout(modePanel);
    modeLay->setContentsMargins(0, 2, 0, 2);
    modeLay->setSpacing(2);
    modePanel->setVisible(false);

    for (int m = 0; m < 2; ++m) {
        const QString label = (m == 0) ? tr("Contar somente cenas")
                                       : tr("Contar somente capítulos inteiros");
        const bool sel = (m == m_presenceMode);
        auto* btn = new QPushButton(label, modePanel);
        btn->setObjectName(sel ? QStringLiteral("pdModeBtnSel") : QStringLiteral("pdModeBtn"));
        btn->setCursor(Qt::PointingHandCursor);
        const int modeIdx = m;
        connect(btn, &QPushButton::clicked, popup, [this, popup, modeIdx]() {
            m_presenceMode = modeIdx;
            popup->close();
            rebuildContents();
        });
        modeLay->addWidget(btn);
    }
    outerLay->addWidget(modePanel);

    connect(gearBtn, &QToolButton::clicked, popup, [modePanel, popup]() {
        modePanel->setVisible(!modePanel->isVisible());
        popup->adjustSize();
    });

    // Separador
    auto* sep = new QFrame(popup);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("background: %1; border: none;").arg(Theme::subtleBorder()));
    sep->setFixedHeight(1);
    outerLay->addWidget(sep);

    // Lista scrollável
    auto* scroll = new QScrollArea(popup);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidgetResizable(true);

    auto* listHost = new QWidget();
    listHost->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* listLay = new QVBoxLayout(listHost);
    listLay->setContentsMargins(0, 0, 4, 0);
    listLay->setSpacing(0);

    if (res.chapters.isEmpty()) {
        auto* emptyLbl = new QLabel(tr("Sem ocorrências registradas."), listHost);
        emptyLbl->setObjectName(QStringLiteral("pdEmpty"));
        listLay->addWidget(emptyLbl);
    } else if (m_presenceMode == 0) {
        // Por cena
        for (const auto& chEntry : res.chapters) {
            if (chEntry.scenes.isEmpty()) {
                auto* lbl = new QLabel(chEntry.title, listHost);
                lbl->setObjectName(QStringLiteral("pdEntry"));
                listLay->addWidget(lbl);
            } else {
                for (const auto& scEntry : chEntry.scenes) {
                    const QString scTitle = scEntry.title.isEmpty()
                        ? tr("Cena %1").arg(scEntry.index + 1) : scEntry.title;
                    auto* lbl = new QLabel(
                        QStringLiteral("%1: %2").arg(chEntry.title, scTitle), listHost);
                    lbl->setObjectName(QStringLiteral("pdEntry"));
                    listLay->addWidget(lbl);
                }
            }
        }
    } else {
        // Por capítulo
        for (const auto& chEntry : res.chapters) {
            auto* lbl = new QLabel(chEntry.title, listHost);
            lbl->setObjectName(QStringLiteral("pdEntry"));
            listLay->addWidget(lbl);
        }
    }
    listLay->addStretch();
    scroll->setWidget(listHost);
    scroll->viewport()->setStyleSheet(
        QStringLiteral("background: %1;").arg(Theme::panelBackground()));

    const int rows = qMax(1, (m_presenceMode == 0) ? res.sceneCount : res.chapterCount);
    scroll->setFixedHeight(qBound(40, rows * 22 + 8, 200));
    outerLay->addWidget(scroll);

    m_presenceDetailPopup = popup;
    connect(popup, &QObject::destroyed, this, [this]() {
        m_presenceDetailPopup = nullptr;
    });

    popup->adjustSize();

    QPoint pos = globalPos;
    const QRect screen = popup->screen() ? popup->screen()->availableGeometry()
                                         : QRect(0, 0, 1920, 1080);
    if (pos.x() + popup->width()  > screen.right())  pos.setX(screen.right()  - popup->width());
    if (pos.y() + popup->height() > screen.bottom())
        pos.setY(globalPos.y() - popup->height() - 10);
    popup->move(pos);
    popup->show();
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
    updateSizeButton();
}

void DrawerListPanel::updateSortButton() {
    if (!m_sortBtn) return;
    m_sortBtn->setText(QStringLiteral("⇅ %1").arg(sortLabelFor(m_sortMode, m_sortAscending)));
}

void DrawerListPanel::updateSizeButton() {
    if (!m_sizeBtn) return;
    const bool visible = currentDrawerIsElement() && m_gridView;
    m_sizeBtn->setVisible(visible);
    const char* labels[] = { "S", "M", "G" };
    m_sizeBtn->setText(QLatin1String(labels[m_cardSizeIdx]));
    const char* tips[] = {
        QT_TR_NOOP("Cards: Pequeno"),
        QT_TR_NOOP("Cards: Médio"),
        QT_TR_NOOP("Cards: Grande")
    };
    m_sizeBtn->setToolTip(tr(tips[m_cardSizeIdx]));
}

void DrawerListPanel::updateViewButton() {
    if (!m_viewBtn) return;
    // Só faz sentido alternar quando é element drawer.
    m_viewBtn->setVisible(currentDrawerIsElement());
    m_viewBtn->setText(m_gridView ? QStringLiteral("⊞") : QStringLiteral("☰"));
    m_viewBtn->setToolTip(m_gridView
        ? tr("Exibição: Blocos — clique para Lista")
        : tr("Exibição: Lista — clique para Blocos"));
    updateSizeButton();
    updateConsistencyBtn();
}

void DrawerListPanel::updateConsistencyBtn() {
    if (!m_consistencyBtn) return;
    const bool isChar = currentDrawerIsCharacter();
    m_consistencyBtn->setVisible(isChar);
    if (!isChar && m_consistencyMode) {
        m_consistencyMode = false;
        m_consistencyBtn->blockSignals(true);
        m_consistencyBtn->setChecked(false);
        m_consistencyBtn->blockSignals(false);
    }
    m_consistencyBtn->setChecked(m_consistencyMode);
}

void DrawerListPanel::rebuildContents() {
    if (!m_listLayout) return;

    m_cardByItemId.clear();
    m_bondHintLabel = nullptr;

    while (m_listLayout->count() > 1) {
        QLayoutItem* item = m_listLayout->takeAt(0);
        if (auto* w = item->widget()) w->deleteLater();
        delete item;
    }

    if (!m_model || m_currentKey.isEmpty()) {
        if (m_bondsLayer) {
            m_bondsLayer->setBonds({});
            m_bondsLayer->setCardPositions({});
        }
        return;
    }
    const Drawer* drawer = m_model->findDrawer(m_currentKey);
    if (!drawer) {
        if (m_bondsLayer) {
            m_bondsLayer->setBonds({});
            m_bondsLayer->setCardPositions({});
        }
        return;
    }

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
        const CardSizeParams& csz = kCardSizes[m_cardSizeIdx];
        const int maxCols = csz.cols;
        const int rowH = (m_consistencyMode && currentDrawerIsCharacter())
                         ? csz.cardHConsistency : csz.cardH;
        QHBoxLayout* currentRow = nullptr;
        int colInRow = 0;
        for (const auto& it : items) {
            if (colInRow == 0) {
                auto* rowWrap = new QWidget(this);
                rowWrap->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
                rowWrap->setFixedHeight(rowH);
                rowWrap->setMouseTracking(true);
                rowWrap->setProperty("isBondRow", true);
                rowWrap->installEventFilter(this);
                currentRow = new QHBoxLayout(rowWrap);
                currentRow->setSpacing(0);
                currentRow->setContentsMargins(0, 0, 0, 0);
                currentRow->addStretch(1); // antes do primeiro
                m_listLayout->insertWidget(row++, rowWrap, /*stretch=*/0, Qt::AlignTop);
            }
            currentRow->addWidget(makeElementCard(it.id, it.title, effectiveRole(it), it.elementId));
            ++colInRow;
            ++displayedCount;
            if (colInRow < maxCols) {
                currentRow->addStretch(1); // entre cards
            } else {
                currentRow->addStretch(1); // após o último da linha
                colInRow = 0;
                currentRow = nullptr;
            }
        }
        if (currentRow) {
            // Linha incompleta no fim — fecha com stretches pros lados ficarem balanceados.
            for (int i = colInRow; i < maxCols; ++i) {
                currentRow->addStretch(1); // célula vazia
                currentRow->addStretch(1);
            }
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

    // Hint de vínculos no rodapé (só em gavetas de personagem)
    if (currentDrawerIsCharacter() && displayedCount > 0) {
        m_bondHintLabel = new QLabel(this);
        m_bondHintLabel->setText(tr("Clique e arraste no nome dos personagens pra criar vínculos entre eles."));
        m_bondHintLabel->setAlignment(Qt::AlignCenter);
        m_bondHintLabel->setWordWrap(true);
        m_bondHintLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-family: 'Lora','Crimson Text',serif; font-size: 11px; "
            "font-style: italic; padding: 14px 8px 4px 8px;").arg(Theme::textMuted()));
        m_listLayout->insertWidget(row++, m_bondHintLabel);
    }

    // Após o rebuild, agenda recompute de posições e atualiza overlay de bonds.
    // Modo S (3 colunas) não renderiza vínculos — o corredor central fica sobre
    // a coluna do meio e o roteamento do BondsLayer atropela os cards.
    QMetaObject::invokeMethod(this, [this]() {
        recomputeCardPositions();
        if (m_bondsLayer && m_model) {
            const bool showBonds = (kCardSizes[m_cardSizeIdx].cols < 3);
            m_bondsLayer->setBonds(showBonds
                ? m_model->characterBondsForDrawer(m_currentKey)
                : QList<CharacterBond>{});
        }
        positionBondsLayer();
    }, Qt::QueuedConnection);
}

QWidget* DrawerListPanel::makeElementCard(const QString& itemId, const QString& title, const QString& role, const QString& elementId)
{
    const CardSizeParams& csz = kCardSizes[m_cardSizeIdx];

    QString imageDataUrl;
    if (m_elementsStore && !elementId.isEmpty()) {
        if (const Element* e = m_elementsStore->findElement(elementId)) {
            imageDataUrl = e->image;
        }
    }

    // Dados de consistência do item (pode ser nulo se não for personagem)
    QString charStatus, charStatusDetail, charLocation;
    if (m_consistencyMode && currentDrawerIsCharacter()) {
        if (const DrawerItem* it = m_model ? m_model->findDrawerItem(itemId) : nullptr) {
            charStatus       = it->charStatus;
            charStatusDetail = it->charStatusDetail;
            charLocation     = it->charLocation;
        }
    }
    const bool showConsistency = m_consistencyMode && currentDrawerIsCharacter();

    // Presença no documento atual (via ElementsStore::docElements)
    const bool presentInDoc = showConsistency
        && m_elementsStore
        && !m_currentDocKey.isEmpty()
        && !elementId.isEmpty()
        && m_elementsStore->hasDocElement(m_currentDocKey, elementId);

    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("elemCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setCursor(Qt::PointingHandCursor);
    card->setContextMenuPolicy(Qt::CustomContextMenu);
    card->setFixedSize(csz.cardW, showConsistency ? csz.cardHConsistency : csz.cardH);
    card->setStyleSheet(QStringLiteral(R"(
        QFrame#elemCard {
            background: %1;
            border: %5 solid %2;
            border-radius: 8px;
        }
        QFrame#elemCard:hover {
            background: %3;
            border-color: %4;
        }
    )").arg(Theme::pressedOverlay(),
            presentInDoc ? Theme::accentDefault() : Theme::subtleBorder(),
            Theme::hoverOverlay(),
            presentInDoc ? Theme::accentDefault() : Theme::borderStrong(),
            presentInDoc ? QStringLiteral("2px") : QStringLiteral("1px")));

    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(4);

    auto* photo = new QLabel(card);
    photo->setFixedSize(csz.cardPhoto, csz.cardPhoto);
    photo->setAlignment(Qt::AlignCenter);
    photo->setStyleSheet(QStringLiteral(
        "background: %1; border-radius: 6px; color: %2; font-size: 9px;")
        .arg(Theme::inputBackground(), Theme::textMuted()));
    QPixmap pm = pixFromDataUrl(imageDataUrl);
    if (!pm.isNull()) {
        photo->setPixmap(photoRounded(pm, csz.cardPhoto, 6));
    } else {
        photo->setText(tr("sem foto"));
    }
    // Foto vira handle de drag: instala event filter para interceptar press/move.
    photo->setCursor(Qt::OpenHandCursor);
    photo->setProperty("itemId", itemId);
    photo->setProperty("isItemPhoto", true);
    photo->installEventFilter(this);
    lay->addWidget(photo, 0, Qt::AlignHCenter);

    // Linha do nome: dot de presença + label
    auto* nameLbl = new QLabel(title.isEmpty() ? tr("(sem nome)") : title, card);
    nameLbl->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 12px; font-weight: 700;").arg(Theme::textBright()));
    nameLbl->setAlignment(Qt::AlignHCenter);
    nameLbl->setWordWrap(true);

    if (presentInDoc) {
        auto* nameRow = new QHBoxLayout();
        nameRow->setSpacing(4);
        nameRow->setContentsMargins(0, 0, 0, 0);
        nameRow->setAlignment(Qt::AlignHCenter);
        auto* dot = new QLabel(card);
        dot->setFixedSize(7, 7);
        dot->setStyleSheet(QStringLiteral(
            "background: %1; border-radius: 3px;").arg(Theme::accentDefault()));
        nameRow->addWidget(dot, 0, Qt::AlignVCenter);
        nameRow->addWidget(nameLbl);
        lay->addLayout(nameRow);
    } else {
        lay->addWidget(nameLbl);
    }

    // Botão discreto de criar vínculo — overlay no canto sup direito do card,
    // só visível ao hover. Vira o handle de drag (substituindo o nome).
    if (currentDrawerIsCharacter()) {
        constexpr int kBtnSize = 18;
        auto* bondBtn = new QToolButton(card);
        bondBtn->setObjectName(QStringLiteral("bondCreateBtn"));
        bondBtn->setFixedSize(kBtnSize, kBtnSize);
        bondBtn->move(csz.cardW - kBtnSize - 4, 4);
        bondBtn->setCursor(Qt::OpenHandCursor);
        bondBtn->setAutoRaise(true);
        bondBtn->setVisible(false);
        bondBtn->setToolTip(tr("Arrastar para criar vínculo"));
        bondBtn->setStyleSheet(QStringLiteral(
            "QToolButton { background: transparent; border: none; border-radius: 4px; }"
            "QToolButton:hover { background: %1; }").arg(Theme::hoverOverlay()));
        {
            const QIcon ic = IconUtils::loadToolbarIcon(
                QStringLiteral(":/icons/create-bond.svg"),
                QColor(Theme::textMuted()), QColor(Theme::textPrimary()),
                QColor(Theme::textBright()), QSize(14, 14));
            if (!ic.isNull()) {
                bondBtn->setIcon(ic);
                bondBtn->setIconSize(QSize(14, 14));
            }
        }
        bondBtn->setProperty("isBondDragHandle", true);
        bondBtn->setProperty("itemId", itemId);
        bondBtn->installEventFilter(this);
        // WA_Hover no card → eventFilter recebe HoverEnter/HoverLeave para show/hide
        card->setAttribute(Qt::WA_Hover);
    }

    if (!role.isEmpty()) {
        auto* roleLbl = new QLabel(role.toUpper(), card);
        roleLbl->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 9px; font-weight: 800; letter-spacing: 0.8px;").arg(roleColor(role)));
        roleLbl->setAlignment(Qt::AlignHCenter);
        lay->addWidget(roleLbl);
    }

    if (showConsistency) {
        // Separador fino
        auto* sep = new QFrame(card);
        sep->setFrameShape(QFrame::HLine);
        sep->setFixedHeight(1);
        sep->setStyleSheet(QStringLiteral("background: %1; border: none; margin: 2px 0;").arg(Theme::subtleBorder()));
        lay->addWidget(sep);

        // Barra de presença (clicável, exibe lista de cenas/capítulos)
        const CharPresenceResult& presRes = m_presenceResults.value(title.toLower());
        const int presCount = (m_presenceMode == 0) ? presRes.sceneCount : presRes.chapterCount;
        const int presTotal = (m_presenceMode == 0) ? m_totalScenes    : m_totalChapters;
        const int pct = (presTotal > 0) ? qMin(100, (presCount * 100) / presTotal) : 0;
        const QString modeStr = (m_presenceMode == 0) ? tr("cena(s)") : tr("capítulo(s)");

        auto* presenceBar = new QProgressBar(card);
        presenceBar->setRange(0, 100);
        presenceBar->setValue(pct);
        presenceBar->setFixedHeight(5);
        presenceBar->setTextVisible(false);
        presenceBar->setCursor(Qt::PointingHandCursor);
        presenceBar->setToolTip(tr("Aparece em %1 de %2 %3 (%4%) — clique para detalhes")
            .arg(presCount).arg(presTotal).arg(modeStr).arg(pct));
        presenceBar->setProperty("presenceBarName", title);
        presenceBar->installEventFilter(this);
        presenceBar->setStyleSheet(QStringLiteral(R"(
            QProgressBar {
                background: %1;
                border: none;
                border-radius: 2px;
            }
            QProgressBar::chunk {
                background: %2;
                border-radius: 2px;
            }
        )").arg(Theme::hoverOverlay(), Theme::accentDefault()));
        lay->addWidget(presenceBar);

        // Botão Status
        auto* statusBtn = new QPushButton(card);
        statusBtn->setObjectName(QStringLiteral("pickerOption"));
        statusBtn->setCursor(Qt::PointingHandCursor);
        statusBtn->setFixedHeight(22);
        const QString statusText = charStatus.isEmpty()
            ? tr("Status: —")
            : tr("Status: %1").arg(charStatus);
        statusBtn->setText(statusText);
        statusBtn->setStyleSheet(QStringLiteral(R"(
            QPushButton {
                background: %1;
                color: %2;
                border: 1px solid %3;
                border-radius: 5px;
                padding: 0 6px;
                text-align: left;
                font-family: 'Lora','Crimson Text',serif;
                font-size: 10px;
            }
            QPushButton:hover { background: %4; color: %5; border-color: %6; }
        )").arg(Theme::pressedOverlay(),
                charStatus.isEmpty() ? Theme::textMuted() : Theme::textBright(),
                Theme::subtleBorder(), Theme::hoverOverlay(),
                Theme::textBright(), Theme::borderStrong()));
        if (!charStatusDetail.isEmpty())
            statusBtn->setToolTip(charStatusDetail);
        connect(statusBtn, &QPushButton::clicked, this, [this, itemId, statusBtn]() {
            showStatusPicker(itemId, statusBtn->mapToGlobal(
                QPoint(0, statusBtn->height() + 2)));
        });
        lay->addWidget(statusBtn);

        // Botão Último local
        auto* locBtn = new QPushButton(card);
        locBtn->setObjectName(QStringLiteral("pickerOption"));
        locBtn->setCursor(Qt::PointingHandCursor);
        locBtn->setFixedHeight(22);
        const QString locText = charLocation.isEmpty()
            ? tr("Local: —")
            : tr("Local: %1").arg(charLocation);
        locBtn->setText(locText);
        locBtn->setStyleSheet(QStringLiteral(R"(
            QPushButton {
                background: %1;
                color: %2;
                border: 1px solid %3;
                border-radius: 5px;
                padding: 0 6px;
                text-align: left;
                font-family: 'Lora','Crimson Text',serif;
                font-size: 10px;
            }
            QPushButton:hover { background: %4; color: %5; border-color: %6; }
        )").arg(Theme::pressedOverlay(),
                charLocation.isEmpty() ? Theme::textMuted() : Theme::textBright(),
                Theme::subtleBorder(), Theme::hoverOverlay(),
                Theme::textBright(), Theme::borderStrong()));
        connect(locBtn, &QPushButton::clicked, this, [this, itemId, locBtn]() {
            showLocationPicker(itemId, locBtn->mapToGlobal(
                QPoint(0, locBtn->height() + 2)));
        });
        lay->addWidget(locBtn);
    } else {
        lay->addStretch();
    }

    const QString drawerKey = m_currentKey;
    card->installEventFilter(this);
    connect(card, &QWidget::customContextMenuRequested, this, [this, card, itemId](const QPoint& pos) {
        showItemContextMenu(itemId, card->mapToGlobal(pos));
    });
    card->setProperty("itemId", itemId);
    card->setProperty("drawerKey", drawerKey);

    installDropTargetOnCard(card, itemId);

    m_cardByItemId.insert(itemId, card);

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

    // ---- listHost OU rowWrap: hover/click em bonds (overlay é click-through) ----
    const bool isBondHitSurface = w && (w == m_listHost
        || (w->property("isBondRow").toBool()));
    if (isBondHitSurface) {
        if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            updateBondHover(me->globalPosition().toPoint());
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                const QPoint global = me->globalPosition().toPoint();
                if (m_bondsLayer && m_model) {
                    const QPointF local = mapToListHost(global);
                    const QString bondId = m_bondsLayer->hitTestBond(local);
                    if (!bondId.isEmpty()) {
                        emit bondViewRequested(m_currentKey, bondId, global);
                        return true;
                    }
                }
            }
        } else if (event->type() == QEvent::Resize && w == m_listHost) {
            // Quando o listHost muda (scroll content cresce ou painel resize),
            // re-alinha a overlay e recompute as posições.
            QMetaObject::invokeMethod(this, [this]() {
                recomputeCardPositions();
                positionBondsLayer();
            }, Qt::QueuedConnection);
        }
    }

    // ---- Name label: drag-to-create bond (gaveta de personagem) ----
    if (w && w->property("isBondDragHandle").toBool()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_bondDragSource = w->property("itemId").toString();
                m_bondDragStartGlobal = me->globalPosition().toPoint();
                m_bondDragActive = false;
                w->setCursor(Qt::ClosedHandCursor);
                // Grab para receber todos os MouseMove/Release mesmo fora do label.
                m_bondDragWidget = w;
                w->grabMouse();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (!m_bondDragSource.isEmpty() && (me->buttons() & Qt::LeftButton)) {
                const QPoint global = me->globalPosition().toPoint();
                if (!m_bondDragActive) {
                    const int d = (global - m_bondDragStartGlobal).manhattanLength();
                    if (d >= QApplication::startDragDistance()) {
                        startBondDrag(m_bondDragSource);
                    }
                }
                if (m_bondDragActive) {
                    updateBondDragPreview(global);
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                // Libera o grab antes de qualquer ação.
                if (m_bondDragWidget) {
                    m_bondDragWidget->releaseMouse();
                    m_bondDragWidget = nullptr;
                }
                w->setCursor(Qt::OpenHandCursor);
                if (m_bondDragActive) {
                    finishBondDrag(me->globalPosition().toPoint());
                    return true;
                }
                if (!m_bondDragSource.isEmpty()) {
                    // Clique curto: abre o item como doc.
                    const QString itemId = m_bondDragSource;
                    m_bondDragSource.clear();
                    emit itemActivated(m_currentKey, itemId);
                    return true;
                }
            }
        }
    }

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

    // ---- Hover no card: mostra/oculta o botão de criar vínculo ----
    if (w && w->objectName() == QStringLiteral("elemCard")) {
        if (event->type() == QEvent::HoverEnter) {
            if (auto* btn = w->findChild<QToolButton*>(QStringLiteral("bondCreateBtn")))
                btn->setVisible(true);
        } else if (event->type() == QEvent::HoverLeave) {
            if (auto* btn = w->findChild<QToolButton*>(QStringLiteral("bondCreateBtn")))
                btn->setVisible(false);
        }
    }

    // ---- Click na barra de presença abre popup de detalhes ----
    if (w && w->property("presenceBarName").isValid()) {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                const QString charName = w->property("presenceBarName").toString();
                const CharPresenceResult& res = m_presenceResults.value(charName.toLower());
                showPresenceDetail(charName, res,
                                   w->mapToGlobal(QPoint(0, w->height() + 4)));
                return true;
            }
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
    // Bonds: recomputa após o layout assentar.
    QMetaObject::invokeMethod(this, [this]() {
        recomputeCardPositions();
        positionBondsLayer();
    }, Qt::QueuedConnection);
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

void DrawerListPanel::recomputeCardPositions() {
    if (!m_listHost || !m_bondsLayer) return;
    QHash<QString, BondCardPos> positions;
    for (auto it = m_cardByItemId.cbegin(); it != m_cardByItemId.cend(); ++it) {
        QWidget* card = it.value();
        if (!card || !card->isVisible()) continue;
        // Mapeia geometria do card para coords do m_listHost.
        const QPoint topLeftLocal = card->mapTo(m_listHost, QPoint(0, 0));
        BondCardPos p;
        p.left = topLeftLocal.x();
        p.top = topLeftLocal.y();
        p.right = p.left + card->width();
        p.bottom = p.top + card->height();
        p.x = p.left + card->width() / 2.0;
        p.y = p.top + card->height() / 2.0;
        // anchor Y/X: usa o photoHalf real do tamanho atual
        const qreal ph = kCardSizes[m_cardSizeIdx].cardPhoto / 2.0;
        p.photoHalf = ph;
        p.ay = p.top + 8 + ph; // padding 8 + metade da foto
        positions.insert(it.key(), p);
    }
    m_bondsLayer->setCardPositions(positions);
    m_bondsLayer->setDrawerWidth(m_listHost->width());
}

void DrawerListPanel::positionBondsLayer() {
    if (!m_bondsLayer || !m_listHost) return;
    m_bondsLayer->setGeometry(0, 0, m_listHost->width(), m_listHost->height());
    m_bondsLayer->raise();
}

QPointF DrawerListPanel::mapToListHost(const QPoint& globalPos) const {
    if (!m_listHost) return QPointF();
    return QPointF(m_listHost->mapFromGlobal(globalPos));
}

void DrawerListPanel::updateBondHover(const QPoint& globalPos) {
    if (!m_bondsLayer) return;
    const QPointF local = mapToListHost(globalPos);
    const QString bondId = m_bondsLayer->hitTestBond(local);
    m_bondsLayer->setHoveredBond(bondId);
    if (m_listHost) {
        m_listHost->setCursor(bondId.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
    }
}

void DrawerListPanel::dispatchBondClick(const QPoint& globalPos) {
    if (!m_bondsLayer || !m_model) return;
    const QPointF local = mapToListHost(globalPos);
    const QString bondId = m_bondsLayer->hitTestBond(local);
    if (!bondId.isEmpty()) emit bondViewRequested(m_currentKey, bondId, globalPos);
}

void DrawerListPanel::startBondDrag(const QString& fromItemId) {
    if (fromItemId.isEmpty()) return;
    m_bondDragActive = true;
    auto it = m_cardByItemId.find(fromItemId);
    if (it == m_cardByItemId.end() || !it.value()) return;
    QWidget* card = it.value();
    // Anchor inicial = centro da foto desse card.
    const QPoint topLeftLocal = card->mapTo(m_listHost, QPoint(0, 0));
    m_bondDragFromLocal = QPointF(topLeftLocal.x() + card->width() / 2.0,
                                  topLeftLocal.y() + 8 + 35);
    if (m_bondsLayer) {
        m_bondsLayer->setDragPreview(true, m_bondDragFromLocal, m_bondDragFromLocal);
    }
    // Grab mouse no name label fica via Qt::ClosedHandCursor; eventFilter cuida.
}

void DrawerListPanel::updateBondDragPreview(const QPoint& globalPos) {
    if (!m_bondsLayer || !m_bondDragActive) return;
    const QPointF local = mapToListHost(globalPos);
    m_bondsLayer->setDragPreview(true, m_bondDragFromLocal, local);
}

void DrawerListPanel::finishBondDrag(const QPoint& globalPos) {
    if (!m_bondDragActive) { m_bondDragSource.clear(); return; }
    m_bondDragActive = false;
    const QString fromId = m_bondDragSource;
    m_bondDragSource.clear();
    if (m_bondsLayer) m_bondsLayer->clearDragPreview();

    // Localiza o card de destino sob o ponteiro.
    QWidget* under = QApplication::widgetAt(globalPos);
    QWidget* card = under;
    QString toId;
    while (card) {
        if (card->objectName() == QStringLiteral("elemCard")) {
            toId = card->property("itemId").toString();
            break;
        }
        card = card->parentWidget();
    }
    if (toId.isEmpty() || toId == fromId) return;

    emit bondCreateRequested(m_currentKey, fromId, toId, globalPos);
}

void DrawerListPanel::cancelBondDrag() {
    if (m_bondDragWidget) {
        m_bondDragWidget->releaseMouse();
        m_bondDragWidget = nullptr;
    }
    m_bondDragActive = false;
    m_bondDragSource.clear();
    if (m_bondsLayer) m_bondsLayer->clearDragPreview();
}

void DrawerListPanel::refreshBonds() {
    if (!m_bondsLayer || !m_model) return;
    const bool showBonds = !m_currentKey.isEmpty() && (kCardSizes[m_cardSizeIdx].cols < 3);
    m_bondsLayer->setBonds(showBonds
        ? m_model->characterBondsForDrawer(m_currentKey)
        : QList<CharacterBond>{});
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
                    "QPushButton { background: %1; border-color: %2; }")
                    .arg(Theme::hoverStrong(), Theme::focusBorder()));
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
