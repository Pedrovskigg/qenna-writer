#include "CarouselViewport.h"

#include "UiScale.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

constexpr qreal kThetaMax = 1.2;   // quanto a ponta curva (~69°)
constexpr qreal kRamp = 40.0;      // px de giro até a ponta curvar inteira
constexpr int kBandBase = 44;      // comprimento de cada ponta curva
constexpr int kEdgeZoneBase = 56;  // zona que gira no modo "só pelas pontas"

const char *kEdgeModeKey = "selectionPopup/edgeMode";

}

CarouselViewport::CarouselViewport(Qt::Orientation orientation, QWidget *parent)
    : QWidget(parent)
    , m_orientation(orientation)
{
    setMouseTracking(true);
    m_clip = new QWidget(this);
    m_clip->installEventFilter(this);
    m_tick.setInterval(16);
    connect(&m_tick, &QTimer::timeout, this, [this]() { step(); });
}

bool CarouselViewport::edgeModePreferred()
{
    return QSettings().value(QLatin1String(kEdgeModeKey), false).toBool();
}

void CarouselViewport::setEdgeModePreferred(bool on)
{
    QSettings().setValue(QLatin1String(kEdgeModeKey), on);
}

void CarouselViewport::setContent(QWidget *content)
{
    if (m_content) m_content->removeEventFilter(this);
    m_content = content;
    if (!content) return;
    content->setParent(m_clip);
    content->installEventFilter(this);
    content->show();
    m_x = m_target = 0;
    relayout();
    updateGeometry();
}

void CarouselViewport::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation) return;
    m_orientation = orientation;
    m_x = m_target = m_vel = 0;
    relayout();
    updateGeometry();
}

void CarouselViewport::setMinimumLength(int px)
{
    m_minLength = qMax(0, px);
    updateGeometry();
}

QRect CarouselViewport::axisRect(int start, int length, int cross) const
{
    return m_orientation == Qt::Horizontal ? QRect(start, 0, length, cross) : QRect(0, start, cross, length);
}

int CarouselViewport::bandPx() const
{
    return qMax(24, qRound(kBandBase * UiScale::scale()));
}

QRect CarouselViewport::flatRect() const
{
    return m_clip ? m_clip->geometry() : rect();
}

QSize CarouselViewport::sizeHint() const
{
    if (!m_content) return QSize(0, 0);
    return m_content->sizeHint().expandedTo(m_content->minimumSizeHint());
}

QSize CarouselViewport::minimumSizeHint() const
{
    const QSize h = sizeHint();
    const int len = qMin(mainOf(h), m_minLength);
    const int cross = m_content ? crossOf(m_content->minimumSizeHint()) : 0;
    return m_orientation == Qt::Horizontal ? QSize(len, cross) : QSize(cross, len);
}

bool CarouselViewport::eventFilter(QObject *watched, QEvent *event)
{
    // O conteúdo mudou de tamanho (botão escondido, gaveta nova, fonte com
    // nome maior): refaz a conta depois que o layout dele assentar.
    if ((watched == m_clip || watched == m_content) && event->type() == QEvent::LayoutRequest)
        scheduleRelayout();
    return QWidget::eventFilter(watched, event);
}

void CarouselViewport::scheduleRelayout()
{
    if (m_relayoutPending) return;
    m_relayoutPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_relayoutPending = false;
        relayout();
        updateGeometry();
    });
}

void CarouselViewport::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayout();
}

void CarouselViewport::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    relayout();
}

void CarouselViewport::relayout()
{
    if (!m_content) return;
    const int vp = mainOf(size());
    const int need = mainOf(sizeHint());
    m_len = qMax(vp, need);
    const bool wasOver = m_max > 0;
    m_max = (vp > 0 && need > vp + 1) ? qreal(need - vp) : 0.0;
    m_x = std::clamp(m_x, 0.0, m_max);
    m_target = std::clamp(m_target, 0.0, m_max);
    if (m_max <= 0) { m_vel = 0; m_tick.stop(); unsetCursor(); }
    applyGeometry();
    if (wasOver != (m_max > 0)) emit overflowChanged(m_max > 0);
}

void CarouselViewport::applyGeometry()
{
    if (!m_content) return;
    const int vp = mainOf(size());
    const int cross = crossOf(size());
    int lead = 0, trail = 0;
    if (m_max > 0) {
        const int band = bandPx();
        lead = qRound(band * qMin(1.0, m_x / kRamp));
        trail = qRound(band * qMin(1.0, (m_max - m_x) / kRamp));
        // Espaço curto demais: as pontas não comem o miolo inteiro.
        const int room = qMax(0, vp - 16);
        if (lead + trail > room && lead + trail > 0) {
            const qreal k = qreal(room) / (lead + trail);
            lead = qRound(lead * k);
            trail = qRound(trail * k);
        }
    }
    const QRect oldLead = axisRect(0, m_bandLead, cross);
    const QRect oldTrail = axisRect(vp - m_bandTrail, m_bandTrail, cross);
    m_bandLead = lead;
    m_bandTrail = trail;
    m_clip->setGeometry(axisRect(lead, qMax(0, vp - lead - trail), cross));
    m_content->setGeometry(axisRect(-(qRound(m_x) + lead), m_len, cross));
    update(oldLead.united(axisRect(0, lead, cross)));
    update(oldTrail.united(axisRect(vp - trail, trail, cross)));
    emit scrolled();
}

qreal CarouselViewport::toContent(qreal u, qreal *cosOut) const
{
    if (cosOut) *cosOut = 1.0;
    const qreal vp = mainOf(size());
    const qreal sinT = std::sin(kThetaMax);
    if (m_bandLead > 0 && u < m_bandLead) {
        const qreal R = m_bandLead / sinT;
        const qreal th = std::asin(std::clamp((m_bandLead - u) / R, 0.0, 1.0));
        if (cosOut) *cosOut = std::cos(th);
        return m_x + m_bandLead - R * th;
    }
    if (m_bandTrail > 0 && u > vp - m_bandTrail) {
        const qreal R = m_bandTrail / sinT;
        const qreal th = std::asin(std::clamp((u - (vp - m_bandTrail)) / R, 0.0, 1.0));
        if (cosOut) *cosOut = std::cos(th);
        return m_x + vp - m_bandTrail + R * th;
    }
    return m_x + u;
}

bool CarouselViewport::inBand(qreal u) const
{
    if (m_max <= 0) return false;
    return u < m_bandLead || u > mainOf(size()) - m_bandTrail;
}

void CarouselViewport::paintEvent(QPaintEvent *)
{
    if (m_max <= 0 || !m_content || !m_content->isVisible()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    paintBand(p, true);
    paintBand(p, false);
}

// Uma ponta: o pedaço do conteúdo logo depois do miolo, fatiado e espremido
// sobre um tambor; apaga conforme curva e some rente à borda.
void CarouselViewport::paintBand(QPainter &p, bool leading)
{
    const int band = leading ? m_bandLead : m_bandTrail;
    if (band < 2) return;
    const qreal vp = mainOf(size());
    const int cross = crossOf(size());
    const qreal R = band / std::sin(kThetaMax);
    const qreal span = R * kThetaMax;
    const qreal inner = leading ? m_x + band : m_x + vp - band; // conteúdo na divisa com o miolo
    const qreal s0 = std::clamp(leading ? inner - span : inner, 0.0, qreal(m_len));
    const qreal s1 = std::clamp(leading ? inner : inner + span, 0.0, qreal(m_len));
    if (s1 - s0 < 1) return;

    const int base = int(std::floor(s0));
    const int srcLen = int(std::ceil(s1)) - base;
    const qreal dpr = devicePixelRatioF();
    const QRect src = axisRect(base, srcLen, cross);
    QPixmap pm((QSizeF(src.size()) * dpr).toSize());
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    m_content->render(&pm, QPoint(), QRegion(src), QWidget::DrawChildren);

    const qreal fadeW = band * 0.6;
    const qreal uInner = leading ? band : vp - band;
    constexpr qreal S = 2.0;
    for (qreal d = 0; d < band; d += S) {
        const qreal d1 = qMin<qreal>(band, d + S);
        const qreal a0 = R * std::asin(std::clamp(d / R, 0.0, 1.0));
        const qreal a1 = R * std::asin(std::clamp(d1 / R, 0.0, 1.0));
        qreal u0, u1, c0, c1;
        if (leading) { u0 = uInner - d1; u1 = uInner - d; c0 = inner - a1; c1 = inner - a0; }
        else         { u0 = uInner + d;  u1 = uInner + d1; c0 = inner + a0; c1 = inner + a1; }
        c0 = std::clamp(c0, s0, s1);
        c1 = std::clamp(c1, s0, s1);
        if (c1 - c0 <= 0.01) continue;
        const qreal cm = std::cos(std::asin(std::clamp(((d + d1) / 2.0) / R, 0.0, 1.0)));
        qreal op = 1.0 - (1.0 - cm) * 0.9;
        const qreal fromEdge = band - (d + d1) / 2.0;
        if (fromEdge < fadeW) op *= qMax(0.0, fromEdge / fadeW);
        if (op <= 0.01) continue;
        const qreal th = cross * (0.88 + 0.12 * cm);
        const qreal off = (cross - th) / 2.0;
        p.setOpacity(op);
        const qreal sm = (c0 - base) * dpr, sl = (c1 - c0) * dpr;
        if (m_orientation == Qt::Horizontal)
            p.drawPixmap(QRectF(u0, off, u1 - u0 + 0.6, th), pm, QRectF(sm, 0, sl, cross * dpr));
        else
            p.drawPixmap(QRectF(off, u0, th, u1 - u0 + 0.6), pm, QRectF(0, sm, cross * dpr, sl));
    }
    p.setOpacity(1.0);
}

QAbstractButton *CarouselViewport::buttonAtContent(qreal s) const
{
    if (!m_content) return nullptr;
    QAbstractButton *best = nullptr;
    qreal bestDist = 1e9;
    for (QAbstractButton *b : m_content->findChildren<QAbstractButton*>()) {
        if (!b->isVisibleTo(m_content)) continue;
        const QPoint tl = b->mapTo(m_content.data(), QPoint(0, 0));
        const qreal a = mainOf(QPointF(tl)), z = a + mainOf(b->size());
        if (s >= a && s < z) return b;
        const qreal dist = s < a ? a - s : s - z;
        if (dist < bestDist) { bestDist = dist; best = b; }
    }
    return bestDist <= 4 ? best : nullptr;
}

void CarouselViewport::ensureVisible(QWidget *w, bool animated)
{
    if (!m_content || !w || m_max <= 0 || !m_content->isAncestorOf(w)) return;
    const QPoint tl = w->mapTo(m_content.data(), QPoint(0, 0));
    const qreal a = mainOf(QPointF(tl)), z = a + mainOf(w->size());
    const qreal vp = mainOf(size());
    const qreal pad = bandPx() + 6;
    qreal t = animated ? m_target : m_x;
    if (a < t + pad) t = a - pad;
    else if (z > t + vp - pad) t = z - vp + pad;
    t = std::clamp(t, 0.0, m_max);
    m_vel = 0;
    if (animated) {
        m_target = t;
        kick();
    } else {
        m_x = m_target = t;
        applyGeometry();
    }
}

void CarouselViewport::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    m_edgeMode = edgeModePreferred();
    if (m_max > 0) kick();
}

void CarouselViewport::mouseMoveEvent(QMouseEvent *event)
{
    const qreal u = mainOf(event->position());
    if (inBand(u)) setCursor(buttonAtContent(toContent(u)) ? Qt::PointingHandCursor : Qt::ArrowCursor);
    else unsetCursor();
    event->ignore();
}

void CarouselViewport::mousePressEvent(QMouseEvent *event)
{
    const qreal u = mainOf(event->position());
    if (event->button() == Qt::LeftButton && inBand(u)) {
        m_pressed = buttonAtContent(toContent(u));
        if (m_pressed) { event->accept(); return; }
    }
    m_pressed.clear();
    event->ignore();   // a barra decide (sair do modo de edição etc.)
}

void CarouselViewport::mouseReleaseEvent(QMouseEvent *event)
{
    const qreal u = mainOf(event->position());
    QAbstractButton *b = (event->button() == Qt::LeftButton && inBand(u)) ? buttonAtContent(toContent(u)) : nullptr;
    if (b && b == m_pressed) {
        // Botão na ponta curva: só traz pro miolo, sem disparar.
        ensureVisible(b, true);
        event->accept();
    } else {
        event->ignore();
    }
    m_pressed.clear();
}

void CarouselViewport::wheelEvent(QWheelEvent *event)
{
    if (m_max <= 0) { event->ignore(); return; }
    const QPoint d = event->angleDelta();
    const int delta = std::abs(d.x()) > std::abs(d.y()) ? -d.x() : -d.y();
    m_target = std::clamp(m_target + delta * 0.6, 0.0, m_max);
    m_vel = 0;
    kick();
    event->accept();
}

void CarouselViewport::steer(qreal u)
{
    const qreal vp = mainOf(size());
    // Botão apertado = arrastando alguma coisa: só as pontas giram, senão a
    // fileira fugiria do que está sendo levado.
    const bool edge = m_edgeMode || QGuiApplication::mouseButtons() != Qt::NoButton;
    if (!edge) {
        const qreal pad = qMin(40.0, vp / 6.0);
        const qreal t = std::clamp((u - pad) / qMax(1.0, vp - pad * 2), 0.0, 1.0);
        m_vel = 0;
        m_target = t * m_max;
        return;
    }
    const qreal zone = qMax<qreal>(bandPx(), qRound(kEdgeZoneBase * UiScale::scale()));
    if (u < zone) m_vel = -std::pow((zone - u) / zone, 1.6);
    else if (u > vp - zone) m_vel = std::pow((u - (vp - zone)) / zone, 1.6);
    else m_vel = 0;
}

void CarouselViewport::kick()
{
    if (!m_tick.isActive()) m_tick.start();
}

void CarouselViewport::step()
{
    const QPoint g = QCursor::pos();
    const QPointF pos = mapFromGlobal(QPointF(g));
    const qreal vp = mainOf(size());
    bool over = isVisible() && rect().contains(pos.toPoint());
    if (!over && isVisible() && extraHover && extraHover(g)) {
        const qreal u = mainOf(pos);
        over = u >= 0 && u < vp;
    }
    // Menu aberto (fonte, tamanho...) preso num botão: a fileira não sai do lugar.
    if (over && QApplication::activePopupWidget()) over = false;

    if (over && m_max > 0) steer(mainOf(pos));
    else m_vel = 0;

    if (m_vel != 0) m_target = std::clamp(m_target + m_vel * 7.0, 0.0, m_max);
    const qreal before = m_x;
    m_x += (m_target - m_x) * 0.2;
    if (std::abs(m_target - m_x) < 0.3) m_x = m_target;
    if (m_x != before) applyGeometry();

    const bool pushing = (m_vel < 0 && m_x > 0) || (m_vel > 0 && m_x < m_max);
    if (!over && m_x == m_target && !pushing) m_tick.stop();
}
