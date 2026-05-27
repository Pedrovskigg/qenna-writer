#include "LeftBar.h"
#include "IconUtils.h"
#include "ProjectModel.h"
#include "Theme.h"

#include <QApplication>
#include <QColor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFrame>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QString separatorQss() {
    return QStringLiteral(
        "background: %1; border: none; max-height: 1px; margin: 2px 6px;"
    ).arg(Theme::subtleBorder());
}
}

namespace {
constexpr const char* kDrawerMime = "application/x-mira-drawer-key";
}

namespace {

constexpr int kBarWidth = 60;
constexpr int kBtnSize  = 40;
constexpr int kIconSize = 26;

QIcon loadLeftbarIcon(const QString& fileName) {
    return IconUtils::loadToolbarIcon(
        QStringLiteral(":/icons/leftbar/%1").arg(fileName),
        QColor(Theme::textMuted()),
        QColor(Theme::textPrimary()),
        QColor(Theme::textBright()),
        QSize(kIconSize, kIconSize));
}

QString fixedButtonQss() {
    // Sem fundo no estado normal; borda translúcida no hover; outline em
    // accent + leve tint no estado checked (= painel aberto). Sem mudança
    // de tamanho — todas as bordas têm 1px reservado em transparent.
    return QStringLiteral(R"(
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 8px;
            color: %1;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 15px;
            font-weight: 600;
        }
        QToolButton:hover {
            background: %2;
            border-color: %3;
            color: %4;
        }
        QToolButton:checked {
            background: %6;
            border-color: %5;
            color: %4;
        }
    )").arg(Theme::textMuted(),
            Theme::hoverOverlay(),
            Theme::subtleBorder(),
            Theme::textBright(),
            Theme::accentDefault(),
            Theme::pressedOverlay());
}

QString drawerButtonQss(const QString& accent) {
    // Letra colorida com o accent da gaveta; fundo sempre transparente.
    // Hover e checked usam a própria cor da gaveta como borda.
    return QStringLiteral(R"(
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 8px;
            color: %1;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 17px;
            font-weight: 700;
        }
        QToolButton:hover {
            background: %3;
            border-color: %4;
            color: %2;
        }
        QToolButton:checked {
            background: %5;
            border-color: %1;
            color: %2;
        }
    )").arg(accent, Theme::textBright(),
           Theme::hoverOverlay(), Theme::subtleBorder(),
           Theme::pressedOverlay());
}

QString newDrawerQss() {
    return QStringLiteral(R"(
        QToolButton {
            background: transparent;
            color: %1;
            border: 1px dashed %2;
            border-radius: 8px;
            font-size: 22px;
            font-weight: 300;
        }
        QToolButton:hover {
            color: %3;
            border-color: %4;
            background: %5;
        }
    )").arg(Theme::textMuted(),
            Theme::panelBorder(),
            Theme::textBright(),
            Theme::textMuted(),
            Theme::pressedOverlay());
}

QFrame* makeGroupSeparator(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    line->setStyleSheet(separatorQss());
    return line;
}

} // namespace

int LeftBar::barWidth() {
    return kBarWidth;
}

LeftBar::LeftBar(ProjectModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_drawerLayout(nullptr)
    , m_rootLayout(nullptr)
{
    setObjectName(QStringLiteral("leftBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(kBarWidth);
    setStyleSheet(Theme::panelQss(QStringLiteral("leftBar")));
    setAcceptDrops(true);

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(8, 10, 8, 10);
    m_rootLayout->setSpacing(4);

    // ---- Grupo 1: Projeto ----
    m_rootLayout->addWidget(makeFixedButton(
        QStringLiteral("projectinfo.svg"),
        QStringLiteral("i"),
        tr("Informações do projeto"),
        Info));

    {
        auto* sep = makeGroupSeparator(this);
        m_groupSeparators.append(sep);
        m_rootLayout->addWidget(sep);
    }

    // ---- Grupo 2: Planejamento ----
    m_rootLayout->addWidget(makeFixedButton(
        QStringLiteral("whiteboard.svg"),
        QStringLiteral("L"),
        tr("Lousa de planejamento"),
        Whiteboard));

    {
        auto* sep = makeGroupSeparator(this);
        m_groupSeparators.append(sep);
        m_rootLayout->addWidget(sep);
    }

    // ---- Grupo 3: Escrita ----
    m_rootLayout->addWidget(makeFixedButton(
        QStringLiteral("manuscriptpanel.svg"),
        QStringLiteral("T"),
        tr("Manuscritos"),
        Manuscripts));
    m_rootLayout->addWidget(makeFixedButton(
        QStringLiteral("consistency.svg"),
        QStringLiteral("C"),
        tr("Consistência narrativa"),
        Consistency));

    {
        auto* sep = makeGroupSeparator(this);
        m_groupSeparators.append(sep);
        m_rootLayout->addWidget(sep);
    }

    // ---- Botão "+" (nova gaveta) — fica logo acima da lista, igual Mira 1
    m_newDrawerBtn = makeNewDrawerButton();
    m_rootLayout->addWidget(m_newDrawerBtn);

    // ---- Gavetas ----
    m_drawerLayout = new QVBoxLayout();
    m_drawerLayout->setContentsMargins(0, 0, 0, 0);
    m_drawerLayout->setSpacing(4);
    m_rootLayout->addLayout(m_drawerLayout);

    m_rootLayout->addStretch();

    if (m_model) {
        connect(m_model, &ProjectModel::drawersChanged, this, &LeftBar::rebuildDrawerButtons);
        connect(m_model, &ProjectModel::loaded, this, &LeftBar::rebuildDrawerButtons);
        rebuildDrawerButtons();
    }

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &LeftBar::applyTheme);
}

void LeftBar::applyTheme() {
    setStyleSheet(Theme::panelQss(QStringLiteral("leftBar")));
    for (auto it = m_fixedButtons.constBegin(); it != m_fixedButtons.constEnd(); ++it) {
        if (auto* btn = it.value()) btn->setStyleSheet(fixedButtonQss());
    }
    if (m_newDrawerBtn) m_newDrawerBtn->setStyleSheet(newDrawerQss());
    for (auto* sep : m_groupSeparators) {
        if (sep) sep->setStyleSheet(separatorQss());
    }
    // Recria botões de gaveta pra refletirem accent/textBright atualizados.
    rebuildDrawerButtons();
}

void LeftBar::setChromeHidden(bool hidden) {
    for (auto it = m_fixedButtons.constBegin(); it != m_fixedButtons.constEnd(); ++it)
        if (it.value()) it.value()->setVisible(!hidden);
    for (auto it = m_drawerButtons.constBegin(); it != m_drawerButtons.constEnd(); ++it)
        if (it.value()) it.value()->setVisible(!hidden);
    if (m_newDrawerBtn) m_newDrawerBtn->setVisible(!hidden);
    for (auto* sep : m_groupSeparators)
        if (sep) sep->setVisible(!hidden);
    if (hidden)
        setStyleSheet(QStringLiteral("#leftBar { background: transparent; }"));
    else
        setStyleSheet(Theme::panelQss(QStringLiteral("leftBar")));
}

void LeftBar::setMirrored(bool mirrored) {
    if (m_mirrored == mirrored) return;
    m_mirrored = mirrored;
    applyMirrorStyle();
}

void LeftBar::applyMirrorStyle() {
    // O lado físico (esquerda/direita) na janela é decidido pelo MainWindow.
    // Aqui o painel tem borda completa (radius); por ora nada muda visualmente.
    setStyleSheet(Theme::panelQss(QStringLiteral("leftBar")));
}

void LeftBar::setActiveFixedAction(FixedAction action) {
    m_activeFixed = static_cast<int>(action);
    m_activeDrawer.clear();
    refreshActiveStates();
}

void LeftBar::setActiveDrawer(const QString& drawerKey) {
    m_activeDrawer = drawerKey;
    m_activeFixed = -1;
    refreshActiveStates();
}

void LeftBar::clearSelection() {
    m_activeFixed = -1;
    m_activeDrawer.clear();
    refreshActiveStates();
}

void LeftBar::refreshActiveStates() {
    for (auto it = m_fixedButtons.constBegin(); it != m_fixedButtons.constEnd(); ++it) {
        if (auto* btn = it.value()) {
            btn->setChecked(it.key() == m_activeFixed);
        }
    }
    for (auto it = m_drawerButtons.constBegin(); it != m_drawerButtons.constEnd(); ++it) {
        if (auto* btn = it.value()) {
            btn->setChecked(it.key() == m_activeDrawer);
        }
    }
}

QToolButton* LeftBar::fixedButton(FixedAction action) const {
    return m_fixedButtons.value(static_cast<int>(action), nullptr);
}

QToolButton* LeftBar::makeFixedButton(const QString& iconResource,
                                     const QString& placeholderLetter,
                                     const QString& tooltip,
                                     FixedAction action) {
    auto* btn = new QToolButton(this);
    btn->setToolTip(tooltip);
    btn->setFixedSize(kBtnSize, kBtnSize);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setCheckable(true);
    btn->setAutoRaise(true);
    btn->setStyleSheet(fixedButtonQss());

    if (!iconResource.isEmpty()) {
        QIcon ic = loadLeftbarIcon(iconResource);
        if (!ic.isNull()) {
            btn->setIcon(ic);
            btn->setIconSize(QSize(kIconSize, kIconSize));
            btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        } else {
            btn->setText(placeholderLetter);
            btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        }
    } else {
        btn->setText(placeholderLetter);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

    // Os botões são "radio-like" entre si — clicar emite o sinal e o MainWindow
    // decide se abre/fecha. O setChecked é controlado por setActiveFixedAction.
    connect(btn, &QToolButton::clicked, this, [this, action, btn]() {
        // Bloqueia o toggle automático do QToolButton — quem manda no estado é
        // o MainWindow via setActiveFixedAction. Reverte o checked imediato.
        btn->setChecked(m_activeFixed == static_cast<int>(action));
        emit fixedActionTriggered(action);
    });

    m_fixedButtons.insert(static_cast<int>(action), btn);
    return btn;
}

QToolButton* LeftBar::makeDrawerButton(const QString& drawerKey,
                                      const QString& title,
                                      const QString& color,
                                      const QString& iconId) {
    auto* btn = new QToolButton(this);
    btn->setToolTip(title);
    btn->setFixedSize(kBtnSize, kBtnSize);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setCheckable(true);
    btn->setAutoRaise(true);

    const QString accent = color.isEmpty() ? Theme::accentDefault() : color;

    // Se a gaveta tem ícone escolhido, renderiza-o tintado com a cor da gaveta;
    // senão, cai pra letra inicial (fallback).
    bool iconLoaded = false;
    if (!iconId.isEmpty()) {
        QIcon ic = IconUtils::loadToolbarIcon(
            QStringLiteral(":/icons/elements/%1.svg").arg(iconId),
            QColor(accent),
            QColor(accent),
            QColor(Theme::textBright()),
            QSize(kIconSize, kIconSize));
        if (!ic.isNull()) {
            btn->setIcon(ic);
            btn->setIconSize(QSize(kIconSize, kIconSize));
            btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
            iconLoaded = true;
        }
    }
    if (!iconLoaded) {
        const QString letter = title.isEmpty() ? QStringLiteral("?") : title.left(1).toUpper();
        btn->setText(letter);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

    btn->setStyleSheet(drawerButtonQss(accent));

    connect(btn, &QToolButton::clicked, this, [this, drawerKey, btn]() {
        btn->setChecked(m_activeDrawer == drawerKey);
        emit drawerSelected(drawerKey);
    });

    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(btn, &QToolButton::customContextMenuRequested, this, [this, btn, drawerKey](const QPoint& pos) {
        emit drawerContextRequested(drawerKey, btn->mapToGlobal(pos));
    });

    btn->installEventFilter(this);
    btn->setProperty("drawerKey", drawerKey);

    m_drawerButtons.insert(drawerKey, btn);
    return btn;
}

QToolButton* LeftBar::makeNewDrawerButton() {
    auto* btn = new QToolButton(this);
    btn->setText(QStringLiteral("+"));
    btn->setToolTip(tr("Nova gaveta"));
    btn->setFixedSize(kBtnSize, kBtnSize);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(newDrawerQss());
    connect(btn, &QToolButton::clicked, this, &LeftBar::newDrawerRequested);
    return btn;
}

bool LeftBar::eventFilter(QObject* watched, QEvent* event) {
    auto* btn = qobject_cast<QToolButton*>(watched);
    if (btn && m_drawerButtons.values().contains(btn)) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragStartPos = me->pos();
                m_pressedDrawerKey = btn->property("drawerKey").toString();
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            if ((me->buttons() & Qt::LeftButton) && !m_pressedDrawerKey.isEmpty()) {
                if ((me->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
                    const QString key = m_pressedDrawerKey;
                    m_pressedDrawerKey.clear();
                    startDrawerDrag(btn, key);
                    return true; // consome o move para não disparar click
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_pressedDrawerKey.clear();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void LeftBar::startDrawerDrag(QToolButton* btn, const QString& drawerKey) {
    QDrag* drag = new QDrag(btn);
    auto* mime = new QMimeData();
    mime->setData(QLatin1String(kDrawerMime), drawerKey.toUtf8());
    drag->setMimeData(mime);
    const QPixmap pm = btn->grab();
    drag->setPixmap(pm);
    drag->setHotSpot(QPoint(pm.width() / 2, pm.height() / 2));
    drag->exec(Qt::MoveAction);
    clearDropIndicator();
}

int LeftBar::drawerInsertIndexAt(const QPoint& posInBar) const {
    // Calcula índice de inserção (0..N) com base na coordenada Y dentro da barra.
    // Pega o midpoint vertical de cada drawer button; se posY < midpoint, insere antes.
    if (!m_drawerLayout) return 0;
    const int count = m_model ? m_model->drawers().size() : 0;
    int idx = 0;
    for (const auto& d : (m_model ? m_model->drawers() : QList<Drawer>{})) {
        QToolButton* b = m_drawerButtons.value(d.key, nullptr);
        if (!b) { ++idx; continue; }
        const QPoint topLeft = b->mapTo(const_cast<LeftBar*>(this), QPoint(0, 0));
        const int mid = topLeft.y() + b->height() / 2;
        if (posInBar.y() < mid) return idx;
        ++idx;
    }
    return count;
}

void LeftBar::updateDropIndicator(int targetIndex) {
    if (targetIndex == m_dropTargetIndex && m_dropIndicator && m_dropIndicator->isVisible()) return;
    m_dropTargetIndex = targetIndex;

    if (!m_dropIndicator) {
        m_dropIndicator = new QWidget(this);
        m_dropIndicator->setFixedHeight(2);
        m_dropIndicator->setStyleSheet(QStringLiteral(
            "background: %1; border-radius: 1px;").arg(Theme::accentDefault()));
        m_dropIndicator->hide();
    }

    // Posição Y do indicador: topo do botão no targetIndex, ou base do último se idx == count.
    int y = 0;
    const QList<Drawer>& list = m_model ? m_model->drawers() : QList<Drawer>{};
    if (targetIndex >= list.size()) {
        if (!list.isEmpty()) {
            QToolButton* b = m_drawerButtons.value(list.last().key);
            if (b) y = b->mapTo(this, QPoint(0, b->height())).y();
        }
    } else {
        QToolButton* b = m_drawerButtons.value(list.at(targetIndex).key);
        if (b) y = b->mapTo(this, QPoint(0, 0)).y();
    }

    const int margin = 6;
    m_dropIndicator->setGeometry(margin, y - 1, width() - margin * 2, 2);
    m_dropIndicator->raise();
    m_dropIndicator->show();
}

void LeftBar::clearDropIndicator() {
    if (m_dropIndicator) m_dropIndicator->hide();
    m_dropTargetIndex = -1;
}

void LeftBar::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat(QLatin1String(kDrawerMime))) {
        event->acceptProposedAction();
    }
}

void LeftBar::dragMoveEvent(QDragMoveEvent* event) {
    if (!event->mimeData()->hasFormat(QLatin1String(kDrawerMime))) return;
    updateDropIndicator(drawerInsertIndexAt(event->position().toPoint()));
    event->acceptProposedAction();
}

void LeftBar::dragLeaveEvent(QDragLeaveEvent* /*event*/) {
    clearDropIndicator();
}

void LeftBar::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasFormat(QLatin1String(kDrawerMime))) return;
    const QString key = QString::fromUtf8(event->mimeData()->data(QLatin1String(kDrawerMime)));
    const int target = drawerInsertIndexAt(event->position().toPoint());
    clearDropIndicator();
    if (m_model && !key.isEmpty()) {
        m_model->reorderDrawer(key, target);
    }
    event->acceptProposedAction();
}

void LeftBar::rebuildDrawerButtons() {
    if (!m_drawerLayout) return;
    m_drawerButtons.clear();
    QLayoutItem* child;
    while ((child = m_drawerLayout->takeAt(0)) != nullptr) {
        if (auto* w = child->widget()) w->deleteLater();
        delete child;
    }
    if (!m_model) return;
    for (const auto& d : m_model->drawers()) {
        // Preferência: drawerIcon (escolha do usuário). Fallback:
        // drawerElementIcon (herdado do tipo). Vazio => letra.
        const QString iconId = !d.drawerIcon.isEmpty() ? d.drawerIcon : d.drawerElementIcon;
        m_drawerLayout->addWidget(makeDrawerButton(d.key, d.title, d.color, iconId));
    }
    refreshActiveStates();
}
