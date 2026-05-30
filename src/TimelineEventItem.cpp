#include "TimelineEventItem.h"

#include <QAction>
#include <QFont>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

// ── Helpers de forma ─────────────────────────────────────────────────────────
// Cada forma usa geometria correta, não distorcida pelo bounding box retangular.
// r = bounding box disponível (com padding já descontado pelo chamador).

// Retângulo arredondado — usa o rect inteiro; é naturalmente horizontal
QPainterPath squarePath(const QRectF& r, qreal radius)
{
    QPainterPath p;
    p.addRoundedRect(r, radius, radius);
    return p;
}

// Círculo perfeito — inscrito no quadrado menor, centralizado
QPainterPath circlePath(const QRectF& r)
{
    const qreal d  = qMin(r.width(), r.height());
    const QRectF sq(r.center().x() - d/2, r.center().y() - d/2, d, d);
    QPainterPath p;
    p.addEllipse(sq);
    return p;
}

// Triângulo equilátero — maior que cabe no rect, centralizado
QPainterPath trianglePath(const QRectF& r)
{
    // lado = min(largura, altura * 2/sqrt(3))
    const qreal maxSideByW = r.width();
    const qreal maxSideByH = r.height() * 2.0 / 1.7320508; // 2/sqrt(3)
    const qreal side = qMin(maxSideByW, maxSideByH);
    const qreal h    = side * 1.7320508 / 2.0;              // sqrt(3)/2 * lado
    const qreal cx   = r.center().x();
    const qreal top  = r.center().y() - h / 2.0;
    const qreal bot  = top + h;
    QPainterPath p;
    p.moveTo(cx,              top);
    p.lineTo(cx + side / 2.0, bot);
    p.lineTo(cx - side / 2.0, bot);
    p.closeSubpath();
    return p;
}

// Losango — usa o rect inteiro; é bom com qualquer proporção
QPainterPath diamondPath(const QRectF& r)
{
    QPainterPath p;
    p.moveTo(r.center().x(), r.top());
    p.lineTo(r.right(),       r.center().y());
    p.lineTo(r.center().x(), r.bottom());
    p.lineTo(r.left(),        r.center().y());
    p.closeSubpath();
    return p;
}

// Polígono regular inscrito em círculo de raio = min(w,h)/2, centralizado
static QPainterPath regularPolygon(const QRectF& r, int sides, double offsetDeg = -90.0)
{
    QPainterPath p;
    const qreal rad = qMin(r.width(), r.height()) / 2.0;
    const qreal cx  = r.center().x(), cy = r.center().y();
    for (int i = 0; i < sides; ++i) {
        const double a = kPi / 180.0 * (360.0 / sides * i + offsetDeg);
        const qreal x = cx + rad * std::cos(a);
        const qreal y = cy + rad * std::sin(a);
        if (i == 0) p.moveTo(x, y); else p.lineTo(x, y);
    }
    p.closeSubpath();
    return p;
}

QPainterPath hexagonPath(const QRectF& r) { return regularPolygon(r, 6,  0.0); }
QPainterPath pentagonPath(const QRectF& r){ return regularPolygon(r, 5, -90.0);}

// Estrela de 5 pontas inscrita em círculo, centralizada
QPainterPath starPath(const QRectF& r)
{
    QPainterPath p;
    const qreal ro = qMin(r.width(), r.height()) / 2.0;
    const qreal ri = ro * 0.40;
    const qreal cx = r.center().x(), cy = r.center().y();
    for (int i = 0; i < 10; ++i) {
        const double a = kPi / 180.0 * (36.0 * i - 90.0);
        const qreal  rr = (i % 2 == 0) ? ro : ri;
        const qreal x = cx + rr * std::cos(a);
        const qreal y = cy + rr * std::sin(a);
        if (i == 0) p.moveTo(x, y); else p.lineTo(x, y);
    }
    p.closeSubpath();
    return p;
}

// Seta apontando para a direita — usa o rect inteiro (forma naturalmente horizontal)
QPainterPath arrowPath(const QRectF& r)
{
    const qreal l      = r.left();
    const qreal ri     = r.right();
    const qreal cy     = r.center().y();
    const qreal shaftT = r.top()    + r.height() * 0.28;
    const qreal shaftB = r.bottom() - r.height() * 0.28;
    const qreal shaftR = r.left()   + r.width()  * 0.62;

    QPainterPath p;
    p.moveTo(l,      shaftT);
    p.lineTo(shaftR, shaftT);
    p.lineTo(shaftR, r.top());
    p.lineTo(ri,     cy);
    p.lineTo(shaftR, r.bottom());
    p.lineTo(shaftR, shaftB);
    p.lineTo(l,      shaftB);
    p.closeSubpath();
    return p;
}

QPainterPath shapeForItem(const QString& shape, const QRectF& r)
{
    if (shape == QStringLiteral("circle"))   return circlePath(r);
    if (shape == QStringLiteral("triangle")) return trianglePath(r);
    if (shape == QStringLiteral("diamond"))  return diamondPath(r);
    if (shape == QStringLiteral("hexagon"))  return hexagonPath(r);
    if (shape == QStringLiteral("pentagon")) return pentagonPath(r);
    if (shape == QStringLiteral("star"))     return starPath(r);
    if (shape == QStringLiteral("arrow"))    return arrowPath(r);
    return squarePath(r, TimelineEventItem::kRadius);
}

// ── Gradiente de preenchimento ────────────────────────────────────────────────

QBrush fillBrush(const QColor& base, const QRectF& r)
{
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, base.lighter(130));
    g.setColorAt(0.5, base);
    g.setColorAt(1.0, base.darker(125));
    return QBrush(g);
}

// ── Pin ──────────────────────────────────────────────────────────────────────

constexpr qreal kPinR = 5.0;

QRectF pinRect(const QPointF& c)
{
    return QRectF(c.x() - kPinR, c.y() - kPinR, kPinR * 2, kPinR * 2);
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

TimelineEventItem::TimelineEventItem(const TimelineEvent& data,
                                     QGraphicsItem* parent)
    : QGraphicsObject(parent), m_data(data)
{
    setPos(data.x, data.y);
    // Drag nativo do Qt — igual à lousa
    setFlags(QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemSendsGeometryChanges |
             QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);
}

void TimelineEventItem::setEventData(const TimelineEvent& d)
{
    prepareGeometryChange();
    m_data = d;
    setPos(d.x, d.y);
    update();
}

void TimelineEventItem::setTimelineColor(const QColor& c)
{
    m_timelineColor = c;
    update();
}

QPointF TimelineEventItem::pinPos() const
{
    return QPointF(kW + kShadow + kPinR, kH / 2.0);
}

QColor TimelineEventItem::effectiveColor() const
{
    return m_data.color.isValid() ? m_data.color : m_timelineColor;
}

QColor TimelineEventItem::textColor() const
{
    const QColor bg = effectiveColor();
    const qreal lum = 0.299 * bg.redF() + 0.587 * bg.greenF() + 0.114 * bg.blueF();
    return lum > 0.5 ? QColor(30, 30, 30) : QColor(240, 240, 240);
}

bool TimelineEventItem::isOnPin(const QPointF& p) const
{
    return pinRect(pinPos()).contains(p);
}

QRectF TimelineEventItem::boundingRect() const
{
    return QRectF(-kShadow, -kShadow,
                  kW + kShadow * 2 + kPinR * 2 + 2,
                  kH + kShadow * 2);
}

QPainterPath TimelineEventItem::shape() const
{
    QPainterPath p = shapeForItem(m_data.shape, QRectF(0, 0, kW, kH));
    QPainterPath pin;
    pin.addEllipse(pinRect(pinPos()));
    return p.united(pin);
}

void TimelineEventItem::paint(QPainter* p,
                              const QStyleOptionGraphicsItem*,
                              QWidget*)
{
    p->setRenderHint(QPainter::Antialiasing);

    const QRectF body(0, 0, kW, kH);
    const QColor fill   = effectiveColor();
    const QColor border = fill.darker(160);
    const QColor txt    = textColor();

    // ── Sombra suave (3 camadas) ────────────────────────────────────────────
    for (int i = 3; i >= 1; --i) {
        const QRectF sh = body.adjusted(i, i, i, i);
        QPainterPath shPath = shapeForItem(m_data.shape, sh);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, 18 * i));
        p->drawPath(shPath);
    }

    // ── Corpo com gradiente ─────────────────────────────────────────────────
    const QPainterPath bodyPath = shapeForItem(m_data.shape, body);
    p->setBrush(fillBrush(fill, body));
    p->setPen(QPen(border, isSelected() ? 2.0 : 1.0));
    p->drawPath(bodyPath);

    // ── Anel de seleção ─────────────────────────────────────────────────────
    if (isSelected()) {
        const QPainterPath ring = shapeForItem(m_data.shape, body.adjusted(-3, -3, 3, 3));
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(QColor(255, 255, 255, 200), 1.5, Qt::DashLine));
        p->drawPath(ring);
    }

    // ── Marcador de tempo ───────────────────────────────────────────────────
    if (!m_data.timeMarker.isEmpty()) {
        QFont f;
        f.setPointSizeF(7.0);
        p->setFont(f);
        p->setPen(QColor(txt.red(), txt.green(), txt.blue(), 150));
        p->drawText(body.adjusted(6, 5, -6, 0),
                    Qt::AlignTop | Qt::AlignHCenter,
                    m_data.timeMarker);
    }

    // ── Título ──────────────────────────────────────────────────────────────
    QFont fTitle;
    fTitle.setPointSizeF(9.0);
    fTitle.setBold(true);
    p->setFont(fTitle);
    p->setPen(txt);
    const QRectF textRect = body.adjusted(8, 16, -8, -6);
    p->drawText(textRect,
                Qt::AlignVCenter | Qt::AlignHCenter | Qt::TextWordWrap,
                m_data.title.isEmpty() ? QStringLiteral("Evento") : m_data.title);

    // ── Pin ─────────────────────────────────────────────────────────────────
    const QPointF pin = pinPos();
    const QColor pinFill = m_hoverPin ? fill.lighter(160) : fill.darker(110);
    p->setBrush(pinFill);
    p->setPen(QPen(border, 1.0));
    p->drawEllipse(pinRect(pin));
}

// ── Eventos de mouse ─────────────────────────────────────────────────────────

void TimelineEventItem::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && isOnPin(e->pos())) {
        m_draggingPin = true;
        emit pinDragStarted(m_data.id, mapToScene(pinPos()));
        e->accept();
        return;
    }
    // Qt gerencia o drag nativo
    QGraphicsObject::mousePressEvent(e);
}

void TimelineEventItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
    if (m_draggingPin) {
        m_draggingPin = false;
        e->accept();
        return;
    }
    QGraphicsObject::mouseReleaseEvent(e);
}

void TimelineEventItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        emit editRequested(m_data.id);
        e->accept();
        return;
    }
    QGraphicsObject::mouseDoubleClickEvent(e);
}

void TimelineEventItem::hoverMoveEvent(QGraphicsSceneHoverEvent* e)
{
    const bool onPin = isOnPin(e->pos());
    if (onPin != m_hoverPin) {
        m_hoverPin = onPin;
        setCursor(onPin ? Qt::CrossCursor : Qt::ArrowCursor);
        update();
    }
}

void TimelineEventItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    if (m_hoverPin) {
        m_hoverPin = false;
        setCursor(Qt::ArrowCursor);
        update();
    }
}

void TimelineEventItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* e)
{
    QMenu menu;
    auto* actEdit   = menu.addAction(QStringLiteral("Editar evento"));
    menu.addSeparator();
    auto* actDelete = menu.addAction(QStringLiteral("Remover"));
    const QAction* chosen = menu.exec(e->screenPos());
    if (chosen == actEdit)   emit editRequested(m_data.id);
    if (chosen == actDelete) emit deleteRequested(m_data.id);
    e->accept();
}

QVariant TimelineEventItem::itemChange(GraphicsItemChange change,
                                       const QVariant& value)
{
    if (change == ItemPositionHasChanged) {
        m_data.x = pos().x();
        m_data.y = pos().y();
        emit positionChanged(m_data.id);
    }
    return QGraphicsObject::itemChange(change, value);
}
