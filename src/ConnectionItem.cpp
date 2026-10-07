#include "ConnectionItem.h"

#include "CardItem.h"
#include "LousaScene.h"

#include <QCursor>
#include <QFontMetricsF>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QtMath>

namespace {
QFont labelFont()
{
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(11);
    f.setWeight(QFont::DemiBold);
    f.setStyleStrategy(QFont::NoSubpixelAntialias);
    return f;
}
} // namespace

ConnectionItem::ConnectionItem(const CanvasConnection& data, LousaScene* scene,
                               QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_data(data)
    , m_scene(scene)
{
    setZValue(-1.0);          // abaixo dos cards
    setAcceptHoverEvents(true);
    setFlag(ItemIsSelectable, false);
    rebuildPath();
}

void ConnectionItem::setConnData(const CanvasConnection& d)
{
    m_data = d;
    invalidateGeometry();
}

void ConnectionItem::invalidateGeometry()
{
    // A caixa acompanha a linha de verdade. (Antes era a cena inteira, de
    // 24.000 px: a exportação como imagem enquadrava esse vazio todo.)
    prepareGeometryChange();
    rebuildPath();
    update();
}

void ConnectionItem::setLineSelected(bool on)
{
    if (m_selected == on) return;
    m_selected = on;
    update();
}

QVector<QPointF> ConnectionItem::computePoints() const
{
    if (!m_scene) return {};
    const CardItem* from = m_scene->findCard(m_data.fromId);
    const CardItem* to   = m_scene->findCard(m_data.toId);
    if (!from || !to) return {};

    QVector<QPointF> pts;
    pts.reserve(2 + m_data.waypointCardIds.size());
    pts << from->pinScenePos();
    for (const QString& wid : m_data.waypointCardIds)
        if (const CardItem* wp = m_scene->findCard(wid)) pts << wp->pinScenePos();
    pts << to->pinScenePos();
    return pts;
}

void ConnectionItem::rebuildPath()
{
    const QVector<QPointF> pts = computePoints();
    m_path = QPainterPath();
    if (pts.size() < 2) { m_bounds = QRectF(); return; }

    m_path.moveTo(pts[0]);
    if (!m_data.curved) {
        for (int i = 1; i < pts.size(); ++i) m_path.lineTo(pts[i]);
    } else if (pts.size() == 2) {
        // Barbante frouxo: cai um pouco no meio, como fio preso em dois alfinetes.
        const QPointF a = pts[0], b = pts[1];
        const qreal len = QLineF(a, b).length();
        const QPointF sag(0, qMin(120.0, len * 0.22));
        m_path.cubicTo(a + (b - a) / 3.0 + sag, a + (b - a) * 2.0 / 3.0 + sag, b);
    } else {
        // Catmull-Rom passando pelas paradas
        for (int i = 0; i + 1 < pts.size(); ++i) {
            const QPointF p0 = pts[qMax(0, i - 1)];
            const QPointF p1 = pts[i];
            const QPointF p2 = pts[i + 1];
            const QPointF p3 = pts[qMin(int(pts.size()) - 1, i + 2)];
            m_path.cubicTo(p1 + (p2 - p0) / 6.0, p2 - (p3 - p1) / 6.0, p2);
        }
    }
    QRectF b = m_path.boundingRect().adjusted(-12, -12, 12, 12);
    if (!m_data.label.isEmpty()) b = b.united(labelRect().adjusted(-4, -4, 4, 4));
    m_bounds = b;
}

QPointF ConnectionItem::labelAnchor() const
{
    if (m_path.isEmpty()) return {};
    return m_path.pointAtPercent(0.5);
}

QRectF ConnectionItem::labelRect() const
{
    const QFontMetricsF fm(labelFont());
    const qreal w = fm.horizontalAdvance(m_data.label) + 20.0;
    const qreal h = fm.height() + 6.0;
    const QPointF c = labelAnchor();
    return QRectF(c.x() - w / 2.0, c.y() - h / 2.0, w, h);
}

QRectF ConnectionItem::boundingRect() const
{
    return m_bounds;
}

QPainterPath ConnectionItem::shape() const
{
    if (m_path.isEmpty()) return QPainterPath();
    QPainterPathStroker stroker;
    stroker.setWidth(14);
    stroker.setCapStyle(Qt::RoundCap);
    QPainterPath s = stroker.createStroke(m_path);
    if (!m_data.label.isEmpty()) s.addRoundedRect(labelRect(), 9, 9);
    return s;
}

void ConnectionItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (m_path.isEmpty()) return;
    const QColor clr = m_data.color.isValid() ? m_data.color : QColor(Qt::white);
    p->setRenderHint(QPainter::Antialiasing);

    // A ponta da seta para antes do pin, senão ela some embaixo dele.
    QPainterPath drawn = m_path;
    QPointF tip, tail;
    const qreal len = m_path.length();
    if (m_data.arrow && len > 30.0) {
        const qreal tEnd  = m_path.percentAtLength(len - 9.0);
        const qreal tBack = m_path.percentAtLength(len - 19.0);
        tip  = m_path.pointAtPercent(tEnd);
        tail = m_path.pointAtPercent(tBack);
    }

    if (m_selected || m_hovered) {
        QColor glow = m_selected ? CardItem::accent() : clr;
        glow.setAlpha(m_selected ? 110 : 60);
        p->setPen(QPen(glow, 9.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(Qt::NoBrush);
        p->drawPath(drawn);
    }

    // Barbante: contorno escuro fino por baixo e a cor por cima
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(QColor(0, 0, 0, 90), 4.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->drawPath(drawn);
    p->setPen(QPen(clr, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->drawPath(drawn);

    if (m_data.arrow && len > 30.0) {
        const QPointF dir = tip - tail;
        const qreal angle = qAtan2(dir.y(), dir.x());
        const qreal arrowLen = 11.0, spread = 0.42;
        const QPointF p1 = tip - QPointF(arrowLen * qCos(angle - spread), arrowLen * qSin(angle - spread));
        const QPointF p2 = tip - QPointF(arrowLen * qCos(angle + spread), arrowLen * qSin(angle + spread));
        QPolygonF arrow; arrow << tip << p1 << p2;
        p->setPen(QPen(QColor(0, 0, 0, 90), 1.2));
        p->setBrush(clr);
        p->drawPolygon(arrow);
    }

    if (!m_data.label.isEmpty()) {
        const QRectF lr = labelRect();
        const QColor bg = m_scene ? m_scene->canvasColorForLabels() : QColor(0x1a, 0x1a, 0x19);
        p->setPen(QPen(clr, 1.5));
        p->setBrush(bg);
        p->drawRoundedRect(lr, lr.height() / 2.0, lr.height() / 2.0);
        p->setFont(labelFont());
        const bool bgLight = bg.lightness() > 150;
        QColor tc = bgLight ? clr.darker(170) : clr.lighter(135);
        if (tc.lightness() < 40 && !bgLight) tc = QColor(240, 240, 240);
        p->setPen(tc);
        p->drawText(lr, Qt::AlignCenter, m_data.label);
    }
}

// ── events ───────────────────────────────────────────────────────────────────

void ConnectionItem::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
    m_hovered = true;
    setCursor(Qt::PointingHandCursor);
    update();
}

void ConnectionItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    m_hovered = false;
    unsetCursor();
    update();
}

void ConnectionItem::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) { e->ignore(); return; }
    emit clicked(m_data.id);
    e->accept();
}

void ConnectionItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e)
{
    emit labelEditRequested(m_data.id);
    e->accept();
}

void ConnectionItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* e)
{
    emit clicked(m_data.id);
    emit menuRequested(m_data.id, e->screenPos());
    e->accept();
}
