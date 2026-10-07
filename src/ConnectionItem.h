#pragma once

#include "LousaTypes.h"

#include <QGraphicsObject>
#include <QPainterPath>
#include <QPointF>
#include <QVector>

class LousaScene;

// Linha entre dois cards, de pin a pin (barbante do quadro de investigação).
// Reta por padrão; curva, seta e nome são opções da própria linha.
class ConnectionItem : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit ConnectionItem(const CanvasConnection& data, LousaScene* scene,
                            QGraphicsItem* parent = nullptr);

    const CanvasConnection& connData() const { return m_data; }
    void setConnData(const CanvasConnection& d);
    void invalidateGeometry();      // chamar quando um card vinculado se move
    void setLineSelected(bool on);
    bool isLineSelected() const { return m_selected; }
    QPointF labelAnchor() const;    // meio da linha, em coordenadas de cena
    QRectF  pathBounds() const { return m_path.boundingRect(); }

    QRectF       boundingRect() const override;
    QPainterPath shape()         const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

signals:
    void removeRequested(const QString& id);
    void clicked(const QString& id);
    void labelEditRequested(const QString& id);
    void menuRequested(const QString& id, const QPoint& screenPos);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override;
    void mousePressEvent(QGraphicsSceneMouseEvent*) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent*) override;

private:
    QVector<QPointF> computePoints() const;
    void rebuildPath();
    QRectF labelRect() const;

    CanvasConnection m_data;
    LousaScene*      m_scene;
    QPainterPath     m_path;      // em coordenadas de cena (o item fica em 0,0)
    QRectF           m_bounds;
    bool             m_hovered  = false;
    bool             m_selected = false;
};
