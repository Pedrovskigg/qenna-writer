#pragma once

#include "LousaTypes.h"

#include <QColor>
#include <QGraphicsObject>
#include <QPointF>
#include <QSizeF>

class ZoneItem : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit ZoneItem(const CanvasZone& data, QGraphicsItem* parent = nullptr);

    const CanvasZone& zoneData() const { return m_data; }
    void setZoneData(const CanvasZone& d);

    QRectF       boundingRect() const override;
    QPainterPath shape()         const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

signals:
    void dataChanged(const CanvasZone& data);
    void removeRequested(const QString& id);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* e)    override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* e)     override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e)  override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* e)    override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* e)    override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* e) override;

private:
    // Hit-test helpers
    bool isOnGrip(const QPointF& p) const;
    bool isOnDelete(const QPointF& p) const;
    bool isOnColorDot(const QPointF& p) const;
    int  handleAt(const QPointF& p) const;  // -1 = none, 0-7 = handle index

    void emitData();

    CanvasZone m_data;

    // Interaction state
    bool    m_hovered    = false;
    bool    m_dragging   = false;
    bool    m_resizing   = false;
    int     m_resizeHandle = -1;
    QPointF m_pressScene;
    QPointF m_pressOrigin;   // zone (x,y) at press start
    QSizeF  m_pressSize;
};
