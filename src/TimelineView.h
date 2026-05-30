#pragma once

#include <QGraphicsView>
#include <QPoint>

class TimelineView : public QGraphicsView {
    Q_OBJECT
public:
    explicit TimelineView(QGraphicsScene* scene, QWidget* parent = nullptr);

    qreal   zoomFactor() const { return m_zoom; }
    QPointF scrollPos()  const;

    void applyZoomAndPan(qreal zoom, qreal panX, qreal panY);
    void fitAll();  // enquadra o conteúdo todo (ou centraliza se vazio)

signals:
    void zoomChanged(qreal zoom);

protected:
    void wheelEvent(QWheelEvent* event)        override;
    void mousePressEvent(QMouseEvent* event)   override;
    void mouseMoveEvent(QMouseEvent* event)    override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    qreal  m_zoom    = 1.0;
    bool   m_panning = false;
    QPoint m_panLast;
};
