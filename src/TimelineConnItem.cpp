#include "TimelineConnItem.h"
#include "TimelineScene.h"
#include "TimelineEventItem.h"

#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

TimelineConnItem::TimelineConnItem(const TimelineConn& data,
                                   const QColor& color,
                                   TimelineScene* scene,
                                   QGraphicsItem* parent)
    : QGraphicsObject(parent), m_data(data), m_color(color), m_scene(scene)
{
    setZValue(-1); // fica atrás dos eventos
    setAcceptHoverEvents(true);
    setFlags(QGraphicsItem::ItemHasNoContents); // só para hoverEvents; paint manual
    setFlags({}); // reseta e deixa só o hover
    setAcceptHoverEvents(true);
}

void TimelineConnItem::setColor(const QColor& c)
{
    m_color = c;
    update();
}

void TimelineConnItem::invalidate()
{
    prepareGeometryChange();
    m_dirty = true;
    update();
}

QPainterPath TimelineConnItem::computePath() const
{
    auto* from = m_scene->findEvent(m_data.fromEventId);
    auto* to   = m_scene->findEvent(m_data.toEventId);
    if (!from || !to) return QPainterPath();

    const QPointF a = from->mapToScene(QPointF(0, 0));
    const QPointF b = to->mapToScene(QPointF(0, 0));

    // recua as pontas para a borda do ponto, p/ não nascerem dentro do círculo
    const QPointF raw = b - a;
    const qreal   len = std::sqrt(raw.x() * raw.x() + raw.y() * raw.y());
    if (len < 1.0) { QPainterPath pp; pp.moveTo(a); return pp; }
    const QPointF u  = raw / len;
    const QPointF a2 = a + u * (from->dotRadius() + 2.0);
    const QPointF b2 = b - u * (to->dotRadius() + 2.0);

    QPainterPath path;
    path.moveTo(a2);
    const qreal dx = qAbs(b2.x() - a2.x()) * 0.4;
    const qreal dy = (b2.y() - a2.y()) * 0.2;
    path.cubicTo(a2 + QPointF(dx, dy), b2 - QPointF(dx, dy), b2);
    return path;
}

QRectF TimelineConnItem::boundingRect() const
{
    if (m_dirty) {
        m_cachedPath   = computePath();
        m_cachedBounds = m_cachedPath.boundingRect().adjusted(-6, -6, 6, 6);
        m_dirty = false;
    }
    return m_cachedBounds;
}

QPainterPath TimelineConnItem::shape() const
{
    if (m_dirty) boundingRect(); // força recálculo
    // stroke de 12px de área clicável ao redor da linha
    QPainterPathStroker st;
    st.setWidth(12);
    return st.createStroke(m_cachedPath);
}

void TimelineConnItem::paint(QPainter* p,
                             const QStyleOptionGraphicsItem*,
                             QWidget*)
{
    if (m_dirty) boundingRect();
    if (m_cachedPath.isEmpty()) return;

    p->setRenderHint(QPainter::Antialiasing);

    // sombra suave
    QPen shadowPen(QColor(0, 0, 0, 40), m_hovered ? 5.0 : 4.0);
    shadowPen.setCapStyle(Qt::RoundCap);
    p->setPen(shadowPen);
    p->setBrush(Qt::NoBrush);
    p->translate(1, 1);
    p->drawPath(m_cachedPath);
    p->translate(-1, -1);

    // linha principal
    QPen pen(m_color, m_hovered ? 3.0 : 2.0);
    pen.setCapStyle(Qt::RoundCap);
    p->setPen(pen);
    p->drawPath(m_cachedPath);

    // seta na ponta
    const QPointF end = m_cachedPath.currentPosition();
    const QPointF cp  = m_cachedPath.elementAt(m_cachedPath.elementCount() - 2);
    const QPointF raw = end - cp;
    const qreal   len = std::sqrt(raw.x()*raw.x() + raw.y()*raw.y());
    const QPointF dir = len > 0.001 ? raw / len : QPointF(1, 0);
    const QPointF perp(-dir.y(), dir.x());
    const qreal aLen = 7.0;

    QPainterPath arrow;
    arrow.moveTo(end);
    arrow.lineTo(end - dir * aLen + perp * (aLen * 0.45));
    arrow.lineTo(end - dir * aLen - perp * (aLen * 0.45));
    arrow.closeSubpath();
    p->setBrush(m_color);
    p->setPen(Qt::NoPen);
    p->drawPath(arrow);
}

void TimelineConnItem::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
    m_hovered = true; update();
}

void TimelineConnItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    m_hovered = false; update();
}

void TimelineConnItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e)
{
    emit removeRequested(m_data.id);
    e->accept();
}
