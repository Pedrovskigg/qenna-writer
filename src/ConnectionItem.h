#pragma once

#include "LousaTypes.h"

#include <QGraphicsObject>
#include <QPointF>
#include <QVector>

class LousaScene;

class ConnectionItem : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit ConnectionItem(const CanvasConnection& data, LousaScene* scene,
                            QGraphicsItem* parent = nullptr);

    const CanvasConnection& connData() const { return m_data; }
    void setConnData(const CanvasConnection& d);
    void invalidateGeometry();      // chamar quando um card vinculado se move

    QRectF       boundingRect() const override;
    QPainterPath shape()         const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

signals:
    void removeRequested(const QString& id);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) override;

private:
    QVector<QPointF> computePoints() const;
    static QColor darkenColor(const QColor& c, qreal amount);

    CanvasConnection m_data;
    LousaScene*      m_scene;
    bool             m_hovered = false;
};
