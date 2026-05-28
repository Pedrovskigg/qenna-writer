#include "ConnectionItem.h"

#include "CardItem.h"
#include "LousaScene.h"

#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QtMath>

ConnectionItem::ConnectionItem(const CanvasConnection& data, LousaScene* scene,
                               QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_data(data)
    , m_scene(scene)
{
    setZValue(-1.0);          // abaixo dos cards
    setAcceptHoverEvents(true);
    setFlag(ItemIsSelectable, false);
}

void ConnectionItem::setConnData(const CanvasConnection& d)
{
    prepareGeometryChange();
    m_data = d;
    update();
}

void ConnectionItem::invalidateGeometry()
{
    // Não chamamos prepareGeometryChange() — o boundingRect é fixo (sceneRect),
    // então Qt sempre repinta a área correta sem deixar fantasmas.
    update();
}

// ── helpers ─────────────────────────────────────────────────────────────────

QColor ConnectionItem::darkenColor(const QColor& c, qreal amount)
{
    return QColor(
        qRound(c.red()   * (1.0 - amount)),
        qRound(c.green() * (1.0 - amount)),
        qRound(c.blue()  * (1.0 - amount)));
}

// Topo-centro de um card (getCardCenter do Mira 1): ponto de ancoragem para
// note/comment. Outros tipos usariam a borda mais próxima — para o MVP
// todos usam o topo-centro.
static QPointF cardAnchor(const CardItem* c)
{
    return c->pos() + QPointF(c->cardData().width / 2.0, 0.0);
}

QVector<QPointF> ConnectionItem::computePoints() const
{
    if (!m_scene) return {};
    const CardItem* from = m_scene->findCard(m_data.fromId);
    const CardItem* to   = m_scene->findCard(m_data.toId);
    if (!from || !to) return {};

    QVector<QPointF> pts;
    pts.reserve(2 + m_data.waypointCardIds.size());
    pts << cardAnchor(from);
    for (const QString& wid : m_data.waypointCardIds) {
        const CardItem* wp = m_scene->findCard(wid);
        if (wp) pts << cardAnchor(wp);
    }
    pts << cardAnchor(to);
    return pts;
}

// ── geometria ───────────────────────────────────────────────────────────────

QRectF ConnectionItem::boundingRect() const
{
    // Rect fixo = cena inteira. Equivalente ao SVG overflow:visible do Mira 1.
    // Evita o bug de "linha fantasma" causado por boundingRect dinâmico que
    // nunca apaga a área anterior quando um card se move.
    return scene() ? scene()->sceneRect()
                   : QRectF(-12000, -12000, 24000, 24000);
}

QPainterPath ConnectionItem::shape() const
{
    const auto pts = computePoints();
    if (pts.size() < 2) return QPainterPath();
    // Área de hit: stroke de 14px ao longo da linha (igual ao Mira 1)
    QPainterPath path;
    path.moveTo(pts[0]);
    for (int i = 1; i < pts.size(); ++i) path.lineTo(pts[i]);
    QPainterPathStroker stroker;
    stroker.setWidth(14);
    stroker.setCapStyle(Qt::RoundCap);
    return stroker.createStroke(path);
}

// ── paint ───────────────────────────────────────────────────────────────────

void ConnectionItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    const auto pts = computePoints();
    if (pts.size() < 2) return;

    const QColor clr = m_data.color.isValid() ? m_data.color : QColor(Qt::white);
    const QColor darkClr = darkenColor(clr, 0.5);

    p->setRenderHint(QPainter::Antialiasing);

    // Glow quando é snap target (não implementado aqui; futuro)

    // Caminho
    QPainterPath linePath;
    linePath.moveTo(pts[0]);
    for (int i = 1; i < pts.size(); ++i) linePath.lineTo(pts[i]);

    // Outer — borda escura 4px (efeito 3D)
    QPen outerPen(darkClr, 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p->setPen(outerPen);
    p->setOpacity(m_hovered ? 1.0 : 0.7);
    p->setBrush(Qt::NoBrush);
    p->drawPath(linePath);

    // Inner — cor original 3px
    QPen innerPen(clr, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p->setPen(innerPen);
    p->setOpacity(m_hovered ? 1.0 : 0.85);
    p->drawPath(linePath);

    p->setOpacity(1.0);

    // Seta na ponta final
    if (pts.size() >= 2) {
        const QPointF tip = pts.last();
        const QPointF dir = tip - pts[pts.size() - 2];
        const qreal angle = qAtan2(dir.y(), dir.x());
        const qreal arrowLen = 9.0, arrowSpread = 0.45;
        const QPointF p1 = tip - QPointF(arrowLen * qCos(angle - arrowSpread),
                                          arrowLen * qSin(angle - arrowSpread));
        const QPointF p2 = tip - QPointF(arrowLen * qCos(angle + arrowSpread),
                                          arrowLen * qSin(angle + arrowSpread));
        QPolygonF arrow; arrow << tip << p1 << p2;
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(255, 255, 255, 115)); // rgba(255,255,255,0.45)
        p->drawPolygon(arrow);
    }
}

// ── events ───────────────────────────────────────────────────────────────────

void ConnectionItem::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
    m_hovered = true;
    update();
}

void ConnectionItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    m_hovered = false;
    update();
}

void ConnectionItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent*)
{
    emit removeRequested(m_data.id);
}
