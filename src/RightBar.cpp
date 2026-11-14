#include "RightBar.h"

#include "IconUtils.h"
#include "MiraPersonality.h"
#include "Theme.h"
#include "UiScale.h"
#include "LeftBar.h"
#include "LeftBarLabels.h"
#include "PanelMotion.h"

#include <QApplication>
#include <QCursor>
#include <QtMath>
#include <QEnterEvent>
#include <QEvent>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPropertyAnimation>
#include <QSettings>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// Menor que a LeftBar (13/11, pedido do Pe): uma plaquinha discreta, do
// tamanho só dos ícones, pra não tampar o fundo do tema.
constexpr int kBtnSizeBase  = 32;
constexpr int kIconSizeBase = 19;
constexpr int kPadBase      = 5;
constexpr int kHideDelayMs  = 600;   // o mesmo respiro do modo focado

int btnSizePx()  { return qMax(22, qRound(kBtnSizeBase  * UiScale::scale())); }
int padPx()      { return qMax(3, qRound(kPadBase * UiScale::scale())); }
int barWidthPx() { return btnSizePx() + 2 * padPx(); }
int iconSizePx() { return qMax(14, qRound(kIconSizeBase * UiScale::scale())); }

QIcon loadIcon(const QString& name)
{
    const int px = iconSizePx();
    return IconUtils::loadToolbarIcon(
        QStringLiteral(":/icons/%1").arg(name),
        QColor(Theme::textMuted()),
        QColor(Theme::textPrimary()),
        QColor(Theme::textBright()),
        QSize(px, px));
}

// A mesma receita dos botões fixos da LeftBar.
QString buttonQss()
{
    return Theme::qss(QStringLiteral(R"(
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: @radius-control;
        }
        QToolButton:hover {
            background: %1;
            border-color: %2;
        }
        QToolButton:checked {
            background: %3;
            border-color: %4;
        }
    )")).arg(Theme::hoverOverlay(), Theme::subtleBorder(),
            Theme::pressedOverlay(), Theme::accentDefault());
}

QString separatorQss()
{
    return QStringLiteral("background: %1; border: none; max-height: 1px; margin: 2px 4px;")
        .arg(Theme::subtleBorder());
}

QString badgeQss()
{
    return QStringLiteral("background: %1; border-radius: 4px;").arg(Theme::accentDefault());
}

} // namespace

bool RightBar::enabledInSettings()
{
    return QSettings().value(QStringLiteral("ui/rightBarEnabled"), false).toBool();
}

void RightBar::setEnabledInSettings(bool on)
{
    QSettings().setValue(QStringLiteral("ui/rightBarEnabled"), on);
}

RightBar::RightBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("rightBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(barWidthPx());

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(padPx(), padPx() + 2, padPx(), padPx() + 2);
    m_layout->setSpacing(3);
    // Altura do conteúdo: a plaquinha não estica de cima a baixo.
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto sep = [this]() {
        auto* s = new QFrame(this);
        s->setFixedHeight(1);
        m_separators << s;
        m_layout->addWidget(s);
    };

    // Mundo: o que existe na história.
    m_layout->addWidget(makeButton(Construtor, QStringLiteral("construtor.svg"), tr("Criador de Mundos")), 0, Qt::AlignHCenter);
    m_layout->addWidget(makeButton(Reference, QStringLiteral("refmenu.svg"), tr("Painel de Referência (F6)")), 0, Qt::AlignHCenter);
    sep();
    // Anotações: o que você não quer esquecer.
    m_layout->addWidget(makeButton(Pensario, QStringLiteral("pensario.svg"), tr("Pensário (F4)")), 0, Qt::AlignHCenter);
    m_layout->addWidget(makeButton(Reminders, QStringLiteral("reminder.svg"), tr("Lembretes (F7)")), 0, Qt::AlignHCenter);
    sep();
    m_layout->addWidget(makeButton(Statistics, QStringLiteral("stats-chart.svg"), tr("Estatísticas")), 0, Qt::AlignHCenter);
    sep();
    // A assistente fecha a plaquinha: é conversa, não painel.
    m_layout->addWidget(makeButton(Assistant, QStringLiteral("elements/star.svg"),
                                   tr("%1 — chat com a assistente de IA (F9)").arg(miraAssistantName())),
                        0, Qt::AlignHCenter);

    auto badge = [this](QToolButton* host) {
        auto* b = new QLabel(host);
        b->setFixedSize(8, 8);
        b->setAttribute(Qt::WA_TransparentForMouseEvents);
        b->hide();
        return b;
    };
    m_reminderBadge = badge(m_buttons.value(Reminders));
    m_pensarioBadge = badge(m_buttons.value(Pensario));

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(kHideDelayMs);
    connect(m_hideTimer, &QTimer::timeout, this, [this]() {
        if (!m_autoHide || m_chromeHidden) return;
        if (rect().contains(mapFromGlobal(QCursor::pos())) || labelsUnderMouse()) return;
        setChromeHidden(true);
    });

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, &RightBar::applyTheme);
    connect(UiScale::Manager::instance(), &UiScale::Manager::scaleChanged, this, &RightBar::applyUiScale);
    applyTheme();
    applyUiScale();
}

QToolButton* RightBar::makeButton(Tool t, const QString& icon, const QString& tooltip)
{
    auto* b = new QToolButton(this);
    b->setToolTip(tooltip);
    b->setCursor(Qt::PointingHandCursor);
    b->setCheckable(true);
    b->setAutoRaise(true);
    b->setToolButtonStyle(Qt::ToolButtonIconOnly);
    b->setProperty("iconRes", icon);
    connect(b, &QToolButton::clicked, this, [this, t, b]() {
        // Quem manda no aceso é o painel, não o clique (igual à LeftBar).
        b->setChecked(isToolOpen ? isToolOpen(t) : false);
        emit toolTriggered(t);
        // O painel abre ou fecha no mesmo giro ou logo depois (alguns animam).
        QTimer::singleShot(0, this, &RightBar::refreshActiveStates);
        QTimer::singleShot(400, this, &RightBar::refreshActiveStates);
    });
    m_buttons.insert(int(t), b);
    return b;
}

QRect RightBar::toolGlobalRect(Tool t) const
{
    QToolButton* b = m_buttons.value(int(t));
    if (!b || !b->isVisible()) {
        // Escondida no modo focado: ancora na própria barra.
        return QRect(mapToGlobal(QPoint(0, 0)), size());
    }
    return QRect(b->mapToGlobal(QPoint(0, 0)), b->size());
}

void RightBar::refreshActiveStates()
{
    for (auto it = m_buttons.constBegin(); it != m_buttons.constEnd(); ++it)
        if (it.value()) it.value()->setChecked(isToolOpen ? isToolOpen(Tool(it.key())) : false);
}

void RightBar::watchPanel(Tool t, QWidget* panel)
{
    if (!panel || m_watched.contains(panel)) return;
    m_watched.insert(panel, int(t));
    panel->installEventFilter(this);
    connect(panel, &QObject::destroyed, this, [this, panel]() { m_watched.remove(panel); });
}

bool RightBar::eventFilter(QObject* watched, QEvent* event)
{
    // Faixa aberta: o nome já está nela, e clicar num botão fecha a faixa.
    if (auto* b = qobject_cast<QToolButton*>(watched); b && b->parentWidget() == this
        && m_labels && m_labels->isVisible()) {
        if (event->type() == QEvent::ToolTip) return true;
        if (event->type() == QEvent::MouseButtonPress) hideLabels(false);
    }
    if (m_watched.contains(watched)
        && (event->type() == QEvent::Show || event->type() == QEvent::Hide)) {
        QTimer::singleShot(0, this, &RightBar::refreshActiveStates);
    }
    return QWidget::eventFilter(watched, event);
}

void RightBar::setBarSide(Qt::Edge side)
{
    m_side = (side == Qt::LeftEdge) ? Qt::LeftEdge : Qt::RightEdge;
    if (!m_chromeHidden) setStyleSheet(panelStyle());
}

QString RightBar::panelStyle() const
{
    // A cor do painel é a da LeftBar ("só este painel" no Criador de Temas
    // pinta as duas): são a mesma peça, uma de cada lado.
    QString qss = Theme::panelQss(QStringLiteral("rightBar"), Theme::PanelKey::LeftBar);
    // Colada na beira da janela, como uma lingueta: sem borda e sem canto
    // redondo do lado da beira.
    if (m_side == Qt::RightEdge)
        qss += QStringLiteral("#rightBar { border-right: none; border-top-right-radius: 0px; border-bottom-right-radius: 0px; }");
    else
        qss += QStringLiteral("#rightBar { border-left: none; border-top-left-radius: 0px; border-bottom-left-radius: 0px; }");
    return qss;
}

void RightBar::setJoinedTop(bool on)
{
    if (m_joinedTop == on) return;
    m_joinedTop = on;
    if (!m_chromeHidden) setStyleSheet(panelStyle());
}

void RightBar::applyTheme()
{
    setStyleSheet(m_chromeHidden ? QStringLiteral("#rightBar { background: transparent; border: none; }")
                                 : panelStyle());
    for (QToolButton* b : std::as_const(m_buttons)) {
        if (!b) continue;
        b->setStyleSheet(buttonQss());
        const QIcon ic = loadIcon(b->property("iconRes").toString());
        if (!ic.isNull()) b->setIcon(ic);
    }
    for (QWidget* s : std::as_const(m_separators)) s->setStyleSheet(separatorQss());
    for (QLabel* l : { m_reminderBadge, m_pensarioBadge })
        if (l) l->setStyleSheet(badgeQss());
}

void RightBar::applyUiScale()
{
    setFixedWidth(barWidthPx());
    if (m_layout) m_layout->setContentsMargins(padPx(), padPx() + 2, padPx(), padPx() + 2);
    const int btn = btnSizePx();
    const int ico = iconSizePx();
    for (QToolButton* b : std::as_const(m_buttons)) {
        if (!b) continue;
        b->setFixedSize(btn, btn);
        const QIcon ic = loadIcon(b->property("iconRes").toString());
        if (!ic.isNull()) b->setIcon(ic);
        b->setIconSize(QSize(ico, ico));
    }
    positionBadges();
}

void RightBar::positionBadges()
{
    for (QLabel* l : { m_reminderBadge, m_pensarioBadge }) {
        if (!l || !l->parentWidget()) continue;
        l->move(l->parentWidget()->width() - 11, 3);
        l->raise();
    }
}

void RightBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_labels && m_labels->isVisible()) updateLabelsGeometry();
    positionBadges();
}

void RightBar::setReminderBadge(bool active)
{
    if (!m_reminderBadge) return;
    m_reminderBadge->setVisible(active);
    positionBadges();
}

// Pisca e some: "salvou, e é aqui que fica" (o mesmo da barra de cima).
void RightBar::pulsePensarioBadge()
{
    if (!m_pensarioBadge) return;
    m_pensarioBadge->show();
    positionBadges();
    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(m_pensarioBadge->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(m_pensarioBadge);
        m_pensarioBadge->setGraphicsEffect(effect);
    }
    auto* anim = new QPropertyAnimation(effect, "opacity", this);
    anim->setDuration(600);
    anim->setStartValue(1.0);
    anim->setKeyValueAt(0.5, 0.25);
    anim->setEndValue(1.0);
    anim->setLoopCount(4);
    connect(anim, &QPropertyAnimation::finished, this, [this]() {
        if (m_pensarioBadge) m_pensarioBadge->hide();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void RightBar::applyContentVisibility()
{
    const bool on = !m_chromeHidden;
    for (QToolButton* b : std::as_const(m_buttons)) if (b) b->setVisible(on);
    for (QWidget* s : std::as_const(m_separators)) s->setVisible(on);
}

void RightBar::setChromeHidden(bool hidden)
{
    if (m_chromeHidden == hidden) return;
    if (hidden) hideLabels(false);
    m_chromeHidden = hidden;
    applyContentVisibility();
    setStyleSheet(hidden ? QStringLiteral("#rightBar { background: transparent; border: none; }")
                         : panelStyle());
    if (!hidden) refreshActiveStates();
}

void RightBar::setAutoHide(bool on)
{
    m_autoHide = on;
    if (m_hideTimer) m_hideTimer->stop();
    setChromeHidden(on);
}

void RightBar::enterEvent(QEnterEvent* event)
{
    QWidget::enterEvent(event);
    if (m_autoHide) {
        if (m_hideTimer) m_hideTimer->stop();
        setChromeHidden(false);
    }
    if (!LeftBar::labelsEnabled() || (m_labels && m_labels->isVisible())) return;
    if (!m_labelsTimer) {
        m_labelsTimer = new QTimer(this);
        m_labelsTimer->setSingleShot(true);
        connect(m_labelsTimer, &QTimer::timeout, this, &RightBar::showLabels);
    }
    m_labelsTimer->start(kLabelsOpenDelayMs);
}

void RightBar::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    if (m_labelsTimer) m_labelsTimer->stop();
    maybeHideLabels();
    // No modo focado, a faixa aberta segura a barra acordada (ver o timer).
    if (m_autoHide && m_hideTimer) m_hideTimer->start();
}

void RightBar::moveEvent(QMoveEvent* event)
{
    QWidget::moveEvent(event);
    if (m_labels && m_labels->isVisible()) updateLabelsGeometry();
}

void RightBar::hideEvent(QHideEvent* event)
{
    hideLabels(false);
    QWidget::hideEvent(event);
}

bool RightBar::labelsUnderMouse() const
{
    return m_labels && m_labels->isVisible() && m_labels->rect().contains(m_labels->mapFromGlobal(QCursor::pos()));
}

void RightBar::updateLabelsGeometry()
{
    if (!m_labels) return;
    const int w = m_labelsWidth + kLabelsShadow;
    const QRect g = geometry();
    const bool mirrored = (m_side == Qt::RightEdge);   // abre pra dentro da tela
    // 1px por cima da borda da barra, pra não ficar uma linha dupla no meio.
    const int x = mirrored ? g.x() - w + 1 : g.x() + g.width() - 1;
    const int h = m_labelsHeight > 0 ? qMin(g.height(), m_labelsHeight) : g.height();
    m_labels->setGeometry(x, g.y(), w, h);
}

void RightBar::showLabels()
{
    if (!LeftBar::labelsEnabled() || m_chromeHidden || !isVisible()
        || QApplication::mouseButtons() != Qt::NoButton) return;
    QWidget* host = parentWidget();
    if (!host) return;
    if (!m_labels || m_labels->parentWidget() != host) {
        delete m_labels;
        m_labels = new LeftBarLabels(host);
        m_labels->onLeave = [this]() { maybeHideLabels(); };
    }
    refreshLabels();
    updateLabelsGeometry();
    m_labels->raise();
    m_labels->show();
    m_labels->open(PanelMotion::enabled());
    for (QToolButton* b : std::as_const(m_buttons)) if (b) b->installEventFilter(this);
}

void RightBar::refreshLabels()
{
    if (!m_labels) return;
    QList<LeftBarLabels::Row> rows;
    QList<LeftBarLabels::Head> heads;
    // Na faixa, o nome curto; a dica completa continua no tooltip do botão.
    const QList<QPair<Tool, QString>> order = {
        { Construtor, tr("Criador de Mundos") },
        { Reference,  tr("Referência") },
        { Pensario,   tr("Pensário") },
        { Reminders,  tr("Lembretes") },
        { Statistics, tr("Estatísticas") },
        { Assistant,  miraAssistantName() },
    };
    for (const auto& [tool, name] : order) {
        QToolButton* b = m_buttons.value(int(tool));
        if (!b || !b->isVisible()) continue;
        LeftBarLabels::Row r;
        r.y = b->mapTo(this, QPoint(0, 0)).y();
        r.h = b->height();
        r.text = name;
        r.active = b->isChecked();
        QPointer<QToolButton> pb(b);
        r.click = [this, pb]() { hideLabels(false); if (pb) pb->click(); };
        rows << r;
    }
    // Sem títulos de grupo (13/11, o Pe não entendeu): seis botões dispensam.
    // Largura pelo nome mais comprido (em negrito, o mais largo).
    const qreal s = UiScale::scale();
    QFont nf(QStringLiteral("Segoe UI"));
    nf.setPixelSize(qMax(10, qRound(13 * s)));
    nf.setWeight(QFont::DemiBold);
    QFont hf(QStringLiteral("Segoe UI"));
    hf.setPixelSize(qMax(7, qRound(8.5 * s)));
    hf.setBold(true);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
    qreal need = 0;
    for (const auto& r : std::as_const(rows)) need = qMax(need, QFontMetricsF(nf).horizontalAdvance(r.text));
    for (const auto& h : std::as_const(heads)) need = qMax(need, QFontMetricsF(hf).horizontalAdvance(h.text.toUpper()) - 8);
    m_labelsWidth = qBound(labelsMinWidthPx(), qCeil(need) + 30, labelsMaxWidthPx());
    int bottom = 0;
    for (const auto& r : std::as_const(rows)) bottom = qMax(bottom, r.y + r.h);
    m_labelsHeight = bottom > 0 ? bottom + qRound(10 * s) : 0;
    m_labels->setContent(rows, heads, m_side == Qt::RightEdge);
    updateLabelsGeometry();
}

void RightBar::hideLabels(bool animated)
{
    if (m_labelsTimer) m_labelsTimer->stop();
    if (m_labels && m_labels->isVisible()) m_labels->close(animated && PanelMotion::enabled());
}

void RightBar::maybeHideLabels()
{
    // Um respiro pro mouse atravessar da barra pra faixa (e de volta).
    QTimer::singleShot(70, this, [this]() {
        const QPoint g = QCursor::pos();
        if (rect().contains(mapFromGlobal(g))) return;
        if (labelsUnderMouse()) return;
        hideLabels(true);
        if (m_autoHide && m_hideTimer) m_hideTimer->start();
    });
}
