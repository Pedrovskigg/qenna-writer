#include "TimelineScene.h"
#include "TimelineConnItem.h"
#include "TimelineEventItem.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QUuid>

TimelineScene::TimelineScene(QObject* parent) : QGraphicsScene(parent)
{
    setSceneRect(-5000, -5000, 10000, 10000);
}

void TimelineScene::setBackgroundColor(const QColor& color)
{
    m_bgColor = color;
    update();
}

void TimelineScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    painter->fillRect(rect, m_bgColor);
}

// ── Timelines ────────────────────────────────────────────────────────────────

void TimelineScene::setTimelines(const QList<TimelineDef>& timelines)
{
    m_timelines = timelines;
    // Atualiza cor de cada evento já na cena
    for (auto* item : m_events) {
        const QString tid = item->eventData().timelineId;
        item->setTimelineColor(timelineColor(tid));
    }
    update();
}

QColor TimelineScene::timelineColor(const QString& timelineId) const
{
    for (const auto& t : m_timelines) {
        if (t.id == timelineId)
            return t.color;
    }
    return QColor(QStringLiteral("#6c8ebf")); // fallback
}

// ── Eventos ──────────────────────────────────────────────────────────────────

TimelineEventItem* TimelineScene::addEvent(const TimelineEvent& data)
{
    auto* item = new TimelineEventItem(data);
    item->setTimelineColor(timelineColor(data.timelineId));
    addItem(item);
    m_events.append(item);
    m_eventById.insert(data.id, item);

    connect(item, &TimelineEventItem::positionChanged,
            this, &TimelineScene::onEventPositionChanged);
    connect(item, &TimelineEventItem::deleteRequested,
            this, [this](const QString& id) {
                removeEvent(id);
                emit eventDataChanged();
            });
    connect(item, &TimelineEventItem::editRequested,
            this, &TimelineScene::eventEditRequested);
    connect(item, &TimelineEventItem::pinDragStarted,
            this, &TimelineScene::eventPinDragStarted);

    return item;
}

void TimelineScene::removeEvent(const QString& id)
{
    auto* item = m_eventById.value(id, nullptr);
    if (!item) return;
    m_events.removeOne(item);
    m_eventById.remove(id);
    removeItem(item);
    delete item;
}

void TimelineScene::clearEvents()
{
    for (auto* item : m_events) {
        removeItem(item);
        delete item;
    }
    m_events.clear();
    m_eventById.clear();
}

QList<TimelineEvent> TimelineScene::allEventData() const
{
    QList<TimelineEvent> result;
    for (const auto* item : m_events)
        result.append(item->eventData());
    return result;
}

TimelineEventItem* TimelineScene::findEvent(const QString& id) const
{
    return m_eventById.value(id, nullptr);
}

void TimelineScene::onEventPositionChanged(const QString& id)
{
    // Invalida conexões que tocam neste evento
    for (auto* conn : m_connections) {
        const auto& d = conn->connData();
        if (d.fromEventId == id || d.toEventId == id)
            conn->invalidate();
    }
    emit eventDataChanged();
}

// ── Conexões ─────────────────────────────────────────────────────────────────

TimelineConnItem* TimelineScene::addConnection(const TimelineConn& data)
{
    // Cor: mix das timelines dos dois eventos
    const auto* from = m_eventById.value(data.fromEventId);
    const auto* to   = m_eventById.value(data.toEventId);
    QColor color = QColor(QStringLiteral("#888888"));
    if (from && to) {
        const QColor cA = timelineColor(from->eventData().timelineId);
        const QColor cB = timelineColor(to->eventData().timelineId);
        color = QColor((cA.red()+cB.red())/2,
                       (cA.green()+cB.green())/2,
                       (cA.blue()+cB.blue())/2);
    }

    auto* item = new TimelineConnItem(data, color, this);
    addItem(item);
    m_connections.append(item);
    m_connById.insert(data.id, item);

    connect(item, &TimelineConnItem::removeRequested,
            this, [this](const QString& id) {
                removeConnection(id);
                emit eventDataChanged();
            });
    return item;
}

void TimelineScene::removeConnection(const QString& id)
{
    auto* item = m_connById.value(id, nullptr);
    if (!item) return;
    m_connections.removeOne(item);
    m_connById.remove(id);
    removeItem(item);
    delete item;
}

void TimelineScene::clearConnections()
{
    for (auto* item : m_connections) {
        removeItem(item);
        delete item;
    }
    m_connections.clear();
    m_connById.clear();
}

QList<TimelineConn> TimelineScene::allConnectionData() const
{
    QList<TimelineConn> result;
    for (const auto* item : m_connections)
        result.append(item->connData());
    return result;
}

TimelineConn TimelineScene::autoConnect(const QString& newEventId,
                                        const QString& timelineId)
{
    // Acha o evento anterior da mesma timeline (o último criado antes deste)
    QString prevId;
    for (const auto* item : m_events) {
        const auto& d = item->eventData();
        if (d.id != newEventId && d.timelineId == timelineId)
            prevId = d.id; // último encontrado na ordem de inserção
    }
    if (prevId.isEmpty()) return {}; // primeiro evento da timeline, sem conexão

    TimelineConn conn;
    conn.id          = QUuid::createUuid().toString(QUuid::WithoutBraces);
    conn.fromEventId = prevId;
    conn.toEventId   = newEventId;
    addConnection(conn);
    return conn;
}

// ── Canvas interaction ────────────────────────────────────────────────────────

void TimelineScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    // Clique duplo no vazio → sinal para o painel criar um novo evento nessa posição
    if (!itemAt(event->scenePos(), QTransform())) {
        emit canvasDoubleClicked(event->scenePos());
        event->accept();
        return;
    }
    QGraphicsScene::mouseDoubleClickEvent(event);
}
