#pragma once

#include "TimelineTypes.h"

#include <QColor>
#include <QGraphicsObject>
#include <QString>

class TimelineScene;

class TimelineConnItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit TimelineConnItem(const TimelineConn& data,
                              const QColor& color,
                              TimelineScene* scene,
                              QGraphicsItem* parent = nullptr);

    const TimelineConn& connData() const { return m_data; }
    void setColor(const QColor& c);

    QRectF       boundingRect() const override;
    QPainterPath shape()        const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

    void invalidate(); // chamar quando um evento vinculado se move

signals:
    void removeRequested(const QString& id);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) override;

private:
    QPainterPath computePath() const;

    TimelineConn  m_data;
    QColor        m_color{QStringLiteral("#6c8ebf")};
    TimelineScene* m_scene;
    bool          m_hovered = false;

    mutable QPainterPath m_cachedPath;
    mutable QRectF       m_cachedBounds;
    mutable bool         m_dirty = true;
};
