#pragma once

#include "TimelineTypes.h"

#include <QColor>
#include <QGraphicsObject>
#include <QPointF>

// Evento da linha do tempo renderizado como PONTO interativo:
//  - hover  → rótulo flutuante (título + marcação temporal)
//  - clique → popover com a descrição (e vínculos)
//  - duplo  → abre o popup de edição
class TimelineEventItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit TimelineEventItem(const TimelineEvent& data,
                               QGraphicsItem* parent = nullptr);

    const TimelineEvent& eventData() const { return m_data; }
    void setEventData(const TimelineEvent& d);
    void setTimelineColor(const QColor& c);

    void setOpen(bool open);
    bool isOpen() const { return m_open; }

    QRectF       boundingRect() const override;
    QPainterPath shape()        const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

    // Rola a descrição quando o popover está aberto. true = scroll consumido.
    bool wheelScroll(int angleDeltaY);

    // Raio do ponto: eventos autorais são cheios; eventos auto (presença) menores.
    qreal dotRadius() const { return m_data.autoEvent ? kDotAutoR : kDotR; }

    static constexpr qreal kDotR     = 7.0;   // ponto autoral
    static constexpr qreal kDotAutoR = 4.5;   // ponto auto (presença)
    static constexpr qreal kCardW    = 248.0; // largura do popover
    static constexpr qreal kCardHMax = 220.0; // altura máxima do popover

signals:
    void dataChanged(const TimelineEvent& data);
    void deleteRequested(const QString& id);
    void positionChanged(const QString& id);
    void geometryChanged(const QString& id);   // open/close → revalida bounds
    void editRequested(const QString& id);
    void movedByUser(const QString& id);        // soltou após arrastar → snap no rail
    void openToggled(const QString& id, bool open);
    void exportAsDocRequested(TimelineEvent data);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* e)        override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* e)         override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e)      override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e)  override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* e)        override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* e)        override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* e) override;
    QVariant itemChange(GraphicsItemChange change,
                        const QVariant& value)               override;

private:
    QColor effectiveColor() const;
    QString labelText() const;
    QRectF  labelRect() const;   // rótulo de hover (acima do ponto)
    QRectF  cardRect()  const;   // popover (abaixo do ponto)
    qreal   descVisH()     const;
    qreal   descContentH() const;
    void    paintScrollbar(QPainter* p, const QRectF& card) const;

    TimelineEvent m_data;
    QColor        m_timelineColor{QStringLiteral("#6c8ebf")};

    bool    m_hover        = false;
    bool    m_open         = false;
    bool    m_dragMoved    = false;
    QPointF m_pressScenePos;
    qreal   m_scrollOffset = 0.0;
};
