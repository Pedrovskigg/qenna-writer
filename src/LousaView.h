#pragma once

#include <QGraphicsView>
#include <QPoint>
#include <QPointF>

class LousaScene;
class QGraphicsRectItem;

class LousaView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit LousaView(LousaScene* scene, QWidget* parent = nullptr);

    qreal   zoomFactor() const { return m_zoom; }
    QPointF scrollPos()  const;

    void applyZoomAndPan(qreal zoom, qreal panX, qreal panY);
    void fitSceneRect(const QRectF& r);   // ajusta zoom+pan para enquadrar um retângulo

    // Plan mode: cursor crosshair, arrastar no canvas cria uma zona.
    void setPlanMode(bool on);
    bool isPlanMode() const { return m_planMode; }

    // Brush select (Shift+S): passar o mouse seleciona os cards tocados.
    void setBrushMode(bool on);
    bool isBrushMode() const { return m_brushMode; }

signals:
    void zoomChanged(qreal zoom);
    void zoneDrawn(const QRectF& sceneRect); // emitido ao soltar o mouse no plan mode

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    qreal  m_zoom    = 1.0;
    bool   m_panning = false;
    QPoint m_panLast;

    // Plan mode
    bool               m_planMode   = false;
    bool               m_drawing    = false;
    QPointF            m_drawStart;
    QGraphicsRectItem* m_planGhost  = nullptr;

    // Brush select
    bool               m_brushMode  = false;
};
