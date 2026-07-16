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
    void scrollToRailStart(); // encosta o começo do trilho na borda esquerda

signals:
    void zoomChanged(qreal zoom);
    // clique no fundo SEM arrastar (não foi pan) → foco por clique
    void bgClicked(const QPointF& scenePos, Qt::KeyboardModifiers mods);

protected:
    void wheelEvent(QWheelEvent* event)        override;
    void mousePressEvent(QMouseEvent* event)   override;
    void mouseMoveEvent(QMouseEvent* event)    override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    qreal  m_zoom     = 1.0;
    bool   m_panning  = false;
    bool   m_panMoved = false;   // o pan chegou a arrastar? (senão é clique)
    QPoint m_panLast;
    QPoint m_pressPos;           // posição do press p/ distinguir clique de arraste
};
