#pragma once

#include "TimelineTypes.h"

#include <QColor>
#include <QGraphicsObject>
#include <QPointF>

class TimelineEventItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit TimelineEventItem(const TimelineEvent& data,
                               QGraphicsItem* parent = nullptr);

    const TimelineEvent& eventData() const { return m_data; }
    void setEventData(const TimelineEvent& d);
    void setTimelineColor(const QColor& c);

    QRectF       boundingRect() const override;
    QPainterPath shape()        const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

    // posição local do pin de saída de conexão (centro-direita da bbox)
    QPointF pinPos() const;

    static constexpr qreal kW      = 96.0;
    static constexpr qreal kH      = 56.0;
    static constexpr qreal kRadius =  7.0;
    static constexpr qreal kShadow =  3.0;

signals:
    void dataChanged(const TimelineEvent& data);
    void deleteRequested(const QString& id);
    void positionChanged(const QString& id);
    void editRequested(const QString& id);
    void pinDragStarted(const QString& fromId, const QPointF& pinScenePos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* e)        override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e)      override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e)  override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* e)         override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* e)        override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* e) override;
    QVariant itemChange(GraphicsItemChange change,
                        const QVariant& value)               override;

private:
    QColor effectiveColor() const;
    QColor textColor() const;
    bool   isOnPin(const QPointF& localPos) const;

    TimelineEvent m_data;
    QColor        m_timelineColor{QStringLiteral("#6c8ebf")};
    bool          m_draggingPin  = false;
    bool          m_hoverPin     = false;
};
