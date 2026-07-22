#include "ShelfView.h"
#include "ShelfScene.h"

#include <QResizeEvent>

ShelfView::ShelfView(ShelfScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent)
    , m_scene(scene)
{
    setObjectName(QStringLiteral("menuShelfView"));
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setFrameShape(QFrame::NoFrame);
    setDragMode(QGraphicsView::NoDrag);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::NoAnchor);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setInteractive(true);
    setMouseTracking(true);
}

void ShelfView::refreshLayout()
{
    if (!m_scene) return;
    const qreal w = qMax(1, viewport()->width());
    m_scene->setWrapWidth(w);
    m_scene->relayout();
    const int h = int(m_scene->sceneRect().height());
    if (h != height()) setFixedHeight(h);
}

void ShelfView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    refreshLayout();
}
