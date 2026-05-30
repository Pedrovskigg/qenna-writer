#include "TimelineView.h"

#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>

TimelineView::TimelineView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent)
{
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::NoAnchor);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setCacheMode(QGraphicsView::CacheBackground);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setInteractive(true);
    setMouseTracking(true);
}

QPointF TimelineView::scrollPos() const
{
    return {qreal(horizontalScrollBar()->value()),
            qreal(verticalScrollBar()->value())};
}

void TimelineView::applyZoomAndPan(qreal zoom, qreal panX, qreal panY)
{
    m_zoom = qBound(0.1, zoom, 4.0);
    QTransform t;
    t.scale(m_zoom, m_zoom);
    setTransform(t);
    horizontalScrollBar()->setValue(qRound(panX));
    verticalScrollBar()->setValue(qRound(panY));
}

void TimelineView::fitAll()
{
    const QRectF items = scene() ? scene()->itemsBoundingRect() : QRectF();
    if (items.isNull() || items.isEmpty()) {
        // canvas vazio: centraliza na origem
        applyZoomAndPan(1.0, 0, 0);
        centerOn(0, 0);
        return;
    }
    constexpr qreal pad = 80.0;
    const QRectF target = items.adjusted(-pad, -pad, pad, pad);
    const qreal sx = viewport()->width()  / target.width();
    const qreal sy = viewport()->height() / target.height();
    m_zoom = qBound(0.1, qMin(sx, sy), 4.0);
    QTransform t;
    t.scale(m_zoom, m_zoom);
    setTransform(t);
    centerOn(items.center());
    emit zoomChanged(m_zoom);
}

void TimelineView::wheelEvent(QWheelEvent* event)
{
    const qreal step    = event->angleDelta().y() > 0 ? 1.12 : (1.0 / 1.12);
    const qreal newZoom = qBound(0.1, m_zoom * step, 4.0);
    if (qFuzzyCompare(newZoom, m_zoom)) { event->accept(); return; }

    const QPointF scenePos = mapToScene(event->position().toPoint());
    m_zoom = newZoom;
    QTransform t;
    t.scale(m_zoom, m_zoom);
    setTransform(t);

    const QPointF newViewPos = mapFromScene(scenePos);
    const QPointF delta      = event->position() - newViewPos;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - qRound(delta.x()));
    verticalScrollBar()->setValue(verticalScrollBar()->value()   - qRound(delta.y()));

    emit zoomChanged(m_zoom);
    event->accept();
}

void TimelineView::mousePressEvent(QMouseEvent* event)
{
    const bool isMiddle = (event->button() == Qt::MiddleButton);
    const bool isBgLeft = (event->button() == Qt::LeftButton) && !itemAt(event->pos());
    if (isMiddle || isBgLeft) {
        m_panning = true;
        m_panLast = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void TimelineView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        const QPoint delta = event->pos() - m_panLast;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value()   - delta.y());
        m_panLast = event->pos();
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void TimelineView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_panning && (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}
