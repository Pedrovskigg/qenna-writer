#pragma once

#include "TimelineTypes.h"

#include <QColor>
#include <QGraphicsScene>
#include <QHash>
#include <QList>

class TimelineEventItem;
class TimelineConnItem;

class TimelineScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit TimelineScene(QObject* parent = nullptr);

    void setBackgroundColor(const QColor& color);

    // ── Timelines ────────────────────────────────────────────────────────────
    void setTimelines(const QList<TimelineDef>& timelines);
    const QList<TimelineDef>& timelines() const { return m_timelines; }
    QColor timelineColor(const QString& timelineId) const;

    // ── Eventos ──────────────────────────────────────────────────────────────
    TimelineEventItem* addEvent(const TimelineEvent& data);
    void               removeEvent(const QString& id);
    void               clearEvents();
    QList<TimelineEvent> allEventData() const;
    TimelineEventItem*   findEvent(const QString& id) const;

    // ── Conexões ─────────────────────────────────────────────────────────────
    TimelineConnItem* addConnection(const TimelineConn& data);
    void              removeConnection(const QString& id);
    void              clearConnections();
    QList<TimelineConn> allConnectionData() const;
    // Cria conexão automática do último evento da timeline para o novo
    TimelineConn      autoConnect(const QString& newEventId, const QString& timelineId);

signals:
    void eventDataChanged();
    void eventEditRequested(const QString& id);
    void eventPinDragStarted(const QString& fromId, const QPointF& scenePos);
    void canvasDoubleClicked(const QPointF& scenePos); // clique duplo em área vazia

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)  override;

private slots:
    void onEventPositionChanged(const QString& id);

private:
    QColor m_bgColor{QStringLiteral("#1c1c1c")};
    QList<TimelineDef>   m_timelines;
    QList<TimelineEventItem*> m_events;
    QHash<QString, TimelineEventItem*> m_eventById;
    QList<TimelineConnItem*> m_connections;
    QHash<QString, TimelineConnItem*> m_connById;
};
