#include "LousaView.h"
#include "LousaScene.h"

#include <QGraphicsRectItem>
#include <QMouseEvent>
#include <QPen>
#include <QScrollBar>
#include <QWheelEvent>

LousaView::LousaView(LousaScene* scene, QWidget* parent)
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

QPointF LousaView::scrollPos() const
{
    return {qreal(horizontalScrollBar()->value()),
            qreal(verticalScrollBar()->value())};
}

void LousaView::applyZoomAndPan(qreal zoom, qreal panX, qreal panY)
{
    m_zoom = qBound(0.15, zoom, 4.0);
    QTransform t;
    t.scale(m_zoom, m_zoom);
    setTransform(t);
    horizontalScrollBar()->setValue(qRound(panX));
    verticalScrollBar()->setValue(qRound(panY));
}

void LousaView::wheelEvent(QWheelEvent* event)
{
    const qreal step     = event->angleDelta().y() > 0 ? 1.12 : (1.0 / 1.12);
    const qreal newZoom  = qBound(0.15, m_zoom * step, 4.0);
    if (qFuzzyCompare(newZoom, m_zoom)) { event->accept(); return; }

    const QPointF scenePos = mapToScene(event->position().toPoint());
    m_zoom = newZoom;
    QTransform t;
    t.scale(m_zoom, m_zoom);
    setTransform(t);

    // Mantém o ponto sob o cursor estático.
    const QPointF newViewPos = mapFromScene(scenePos);
    const QPointF delta      = event->position() - newViewPos;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - qRound(delta.x()));
    verticalScrollBar()->setValue(verticalScrollBar()->value()   - qRound(delta.y()));

    emit zoomChanged(m_zoom);
    event->accept();
}

void LousaView::setPlanMode(bool on)
{
    m_planMode = on;
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
    if (!on && m_planGhost) {
        scene()->removeItem(m_planGhost);
        delete m_planGhost;
        m_planGhost = nullptr;
    }
    m_drawing = false;
}

void LousaView::mousePressEvent(QMouseEvent* event)
{
    // Plan mode: arrastar no canvas vazio cria zona
    if (m_planMode && event->button() == Qt::LeftButton) {
        m_drawing   = true;
        m_drawStart = mapToScene(event->pos());
        if (!m_planGhost) {
            m_planGhost = new QGraphicsRectItem();
            m_planGhost->setZValue(300);
            QPen ghost(QColor(QStringLiteral("#6ea8fe")), 2, Qt::DashLine);
            ghost.setDashPattern({6, 4});
            m_planGhost->setPen(ghost);
            m_planGhost->setBrush(QColor(110, 168, 254, 18));
            scene()->addItem(m_planGhost);
        }
        m_planGhost->setRect(QRectF(m_drawStart, QSizeF(0, 0)));
        event->accept();
        return;
    }

    // Pan com botão do meio, ou arrastar o fundo vazio com botão esquerdo.
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

void LousaView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_drawing && m_planGhost) {
        const QPointF cur = mapToScene(event->pos());
        m_planGhost->setRect(QRectF(m_drawStart, cur).normalized());
        event->accept();
        return;
    }
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

void LousaView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_drawing && event->button() == Qt::LeftButton) {
        m_drawing = false;
        const QRectF zone = m_planGhost ? m_planGhost->rect() : QRectF();
        if (m_planGhost) {
            scene()->removeItem(m_planGhost);
            delete m_planGhost;
            m_planGhost = nullptr;
        }
        setPlanMode(false);
        if (zone.width() > 50 && zone.height() > 50)
            emit zoneDrawn(zone);
        event->accept();
        return;
    }
    if (m_panning && (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}
