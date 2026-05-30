#include "TimelineScene.h"
#include "TimelineConnItem.h"
#include "TimelineEventItem.h"

#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QTimer>
#include <QUuid>
#include <QVector>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kRailTop  = 70.0;
constexpr qreal kRailGap  = 96.0;
constexpr qreal kRailLeft = 60.0;
constexpr qreal kTickGap  = 150.0;

// pequeno deslocamento aleatório p/ desempatar nós exatamente sobrepostos
qreal rand0() { return (QRandomGenerator::global()->bounded(100) - 50) / 50.0; }
} // namespace

TimelineScene::TimelineScene(QObject* parent) : QGraphicsScene(parent)
{
    setSceneRect(-5000, -5000, 10000, 10000);
}

void TimelineScene::setBackgroundColor(const QColor& color)
{
    m_bgColor = color;
    update();
}

void TimelineScene::setViewMode(ViewMode m)
{
    if (m_viewMode == m) return;
    m_viewMode = m;
    if (m_viewMode == ViewMode::Constellation) {
        seedConstellation();
        buildEdges();
        applyConnVisibility();
        startSim();
    } else {
        stopSim();
        relayout();
    }
    update();
}

void TimelineScene::setAxisMode(AxisMode m)
{
    if (m_axisMode == m) return;
    m_axisMode = m;
    relayout();
    update();
}

// ── Layout helpers ─────────────────────────────────────────────────────────────

qreal TimelineScene::railY(int order) const   { return kRailTop + order * kRailGap; }
qreal TimelineScene::tickX(qreal tick) const   { return kRailLeft + tick * kTickGap; }

int TimelineScene::nearestRailOrder(qreal y) const
{
    const int n = qMax(1, m_timelines.size());
    int order = qRound((y - kRailTop) / kRailGap);
    return qBound(0, order, n - 1);
}

// ── Timelines ────────────────────────────────────────────────────────────────

void TimelineScene::setTimelines(const QList<TimelineDef>& timelines)
{
    m_timelines = timelines;
    for (auto* item : m_events)
        item->setTimelineColor(timelineColor(item->eventData().timelineId));
    relayout();
    update();
}

QColor TimelineScene::timelineColor(const QString& timelineId) const
{
    for (const auto& t : m_timelines)
        if (t.id == timelineId) return t.color;
    return QColor(QStringLiteral("#6c8ebf"));
}

// ── Eventos ──────────────────────────────────────────────────────────────────

void TimelineScene::wireEvent(TimelineEventItem* item)
{
    connect(item, &TimelineEventItem::positionChanged,
            this, &TimelineScene::onEventPositionChanged);
    connect(item, &TimelineEventItem::geometryChanged,
            this, &TimelineScene::onEventPositionChanged);
    connect(item, &TimelineEventItem::movedByUser,
            this, &TimelineScene::onEventMovedByUser);
    connect(item, &TimelineEventItem::openToggled,
            this, &TimelineScene::onEventOpenToggled);
    connect(item, &TimelineEventItem::deleteRequested,
            this, [this](const QString& id) { removeEvent(id); relayout(); emit eventDataChanged(); });
    connect(item, &TimelineEventItem::editRequested,
            this, &TimelineScene::eventEditRequested);
    connect(item, &TimelineEventItem::exportAsDocRequested,
            this, &TimelineScene::exportEventAsDoc);
}

TimelineEventItem* TimelineScene::addEvent(const TimelineEvent& data)
{
    auto* item = new TimelineEventItem(data);
    item->setTimelineColor(timelineColor(data.timelineId));
    addItem(item);
    m_events.append(item);
    m_eventById.insert(data.id, item);
    wireEvent(item);
    return item;
}

void TimelineScene::removeEvent(const QString& id)
{
    QStringList connIds;
    for (const auto* conn : std::as_const(m_connections)) {
        const auto& d = conn->connData();
        if (d.fromEventId == id || d.toEventId == id)
            connIds.append(d.id);
    }
    for (const QString& cid : std::as_const(connIds))
        removeConnection(cid);

    auto* item = m_eventById.value(id, nullptr);
    if (!item) return;
    m_events.removeOne(item);
    m_eventById.remove(id);
    removeItem(item);
    delete item;
}

void TimelineScene::clearEvents()
{
    for (auto* item : m_events) { removeItem(item); delete item; }
    m_events.clear();
    m_eventById.clear();
}

QList<TimelineEvent> TimelineScene::allEventData() const
{
    QList<TimelineEvent> result;
    for (const auto* item : m_events) result.append(item->eventData());
    return result;
}

TimelineEventItem* TimelineScene::findEvent(const QString& id) const
{
    return m_eventById.value(id, nullptr);
}

void TimelineScene::onEventPositionChanged(const QString& id)
{
    if (m_simulating) return; // movimento da própria simulação: tratado em stepSim

    for (auto* conn : m_connections) {
        const auto& d = conn->connData();
        if (d.fromEventId == id || d.toEventId == id) conn->invalidate();
    }

    if (m_viewMode == ViewMode::Constellation) {
        // usuário arrastando um nó: prende-o e reaquece a teia ao redor.
        m_pinnedId = id;
        if (!m_simTimer || !m_simTimer->isActive()) startSim();
        else m_simAlpha = qMax(m_simAlpha, 0.6);
        return; // salva só no release (movedByUser) / ao esfriar
    }

    emit eventDataChanged();
}

void TimelineScene::onEventMovedByUser(const QString& id)
{
    auto* item = m_eventById.value(id, nullptr);
    if (!item) { emit eventDataChanged(); return; }

    if (m_viewMode == ViewMode::Constellation) {
        // soltou o nó: libera o pino e dá um leve reaquecimento p/ acomodar.
        m_pinnedId.clear();
        m_simAlpha = qMax(m_simAlpha, 0.3);
        if (m_simTimer && !m_simTimer->isActive()) startSim();
        emit eventDataChanged();
        return;
    }

    // Modo Trilho: snap na faixa mais próxima + reordena pelo x onde foi solto
    TimelineEvent d = item->eventData();
    const int order = nearestRailOrder(item->pos().y());

    // mapeia order → timelineId (timelines ordenadas por railOrder)
    QList<TimelineDef> sorted = m_timelines;
    std::sort(sorted.begin(), sorted.end(),
              [](const TimelineDef& a, const TimelineDef& b){ return a.railOrder < b.railOrder; });
    if (order >= 0 && order < sorted.size()) {
        const QString newTid = sorted[order].id;
        if (newTid != d.timelineId) {
            d.timelineId = newTid;
            item->setTimelineColor(timelineColor(newTid));
        }
    }
    // ordem fracionária pela posição horizontal → relayout re-inteiriza
    const qreal frac = (item->pos().x() - kRailLeft) / kTickGap;
    if (m_axisMode == AxisMode::Story) d.storyOrder = frac;
    else                               d.narrativeTick = frac;
    item->setEventData(d);

    relayout();
    emit eventDataChanged();
}

void TimelineScene::onEventOpenToggled(const QString& id, bool open)
{
    if (!open) return;
    for (auto* item : m_events)
        if (item->eventData().id != id && item->isOpen())
            item->setOpen(false);
}

// ── Conexões ─────────────────────────────────────────────────────────────────

TimelineConnItem* TimelineScene::addConnection(const TimelineConn& data)
{
    const auto* from = m_eventById.value(data.fromEventId);
    const auto* to   = m_eventById.value(data.toEventId);
    QColor color = QColor(QStringLiteral("#888888"));
    if (from && to) {
        const QColor cA = timelineColor(from->eventData().timelineId);
        const QColor cB = timelineColor(to->eventData().timelineId);
        color = QColor((cA.red()+cB.red())/2, (cA.green()+cB.green())/2, (cA.blue()+cB.blue())/2);
    }

    auto* item = new TimelineConnItem(data, color, this);
    addItem(item);
    m_connections.append(item);
    m_connById.insert(data.id, item);

    connect(item, &TimelineConnItem::removeRequested,
            this, [this](const QString& id) { removeConnection(id); emit eventDataChanged(); });

    applyConnVisibility();
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
    for (auto* item : m_connections) { removeItem(item); delete item; }
    m_connections.clear();
    m_connById.clear();
}

QList<TimelineConn> TimelineScene::allConnectionData() const
{
    QList<TimelineConn> result;
    for (const auto* item : m_connections) result.append(item->connData());
    return result;
}

TimelineConn TimelineScene::autoConnect(const QString& newEventId, const QString& timelineId)
{
    QString prevId;
    for (const auto* item : m_events) {
        const auto& d = item->eventData();
        if (d.id != newEventId && d.timelineId == timelineId) prevId = d.id;
    }
    if (prevId.isEmpty()) return {};

    TimelineConn conn;
    conn.id          = QUuid::createUuid().toString(QUuid::WithoutBraces);
    conn.fromEventId = prevId;
    conn.toEventId   = newEventId;
    conn.type        = QStringLiteral("sequence");
    addConnection(conn);
    return conn;
}

void TimelineScene::applyConnVisibility()
{
    // No Modo Trilho, conexões "sequence" entre eventos do MESMO trilho são
    // redundantes com a própria faixa → escondidas. As demais (cruzamentos) ficam.
    const bool rail = (m_viewMode == ViewMode::Rail);
    for (auto* conn : m_connections) {
        const auto& d = conn->connData();
        const auto* from = m_eventById.value(d.fromEventId);
        const auto* to   = m_eventById.value(d.toEventId);
        bool sameRail = from && to
            && from->eventData().timelineId == to->eventData().timelineId;
        // Rail: esconde sequência do mesmo trilho (a faixa já mostra a ordem).
        // Constelação: esconde conexões "sequence" (o backbone já as desenha).
        const bool hide = (rail && sameRail)
                       || (!rail && d.type == QLatin1String("sequence"));
        conn->setVisible(!hide);
        conn->invalidate();
    }
}

// ── Layout ─────────────────────────────────────────────────────────────────────

void TimelineScene::relayout()
{
    if (m_viewMode == ViewMode::Constellation) {
        // Modelo efêmero: re-semeia a partir do Trilho e simula de novo.
        seedConstellation();
        buildEdges();
        applyConnVisibility();
        startSim();
        update();
        return;
    }

    // Modo Trilho — ordem das faixas por railOrder
    QList<TimelineDef> sorted = m_timelines;
    std::sort(sorted.begin(), sorted.end(),
              [](const TimelineDef& a, const TimelineDef& b){ return a.railOrder < b.railOrder; });
    QHash<QString, int> orderOf;
    for (int i = 0; i < sorted.size(); ++i) orderOf.insert(sorted[i].id, i);

    const bool narrative = (m_axisMode == AxisMode::Narrative);

    for (const auto& t : sorted) {
        // eventos desta faixa
        QList<TimelineEventItem*> items;
        for (auto* it : m_events)
            if (it->eventData().timelineId == t.id) items.append(it);

        std::sort(items.begin(), items.end(),
                  [narrative](TimelineEventItem* a, TimelineEventItem* b) {
            const auto& da = a->eventData();
            const auto& db = b->eventData();
            const qreal ka = narrative ? da.narrativeTick
                                       : (da.storyOrder >= 0 ? da.storyOrder : da.narrativeTick);
            const qreal kb = narrative ? db.narrativeTick
                                       : (db.storyOrder >= 0 ? db.storyOrder : db.narrativeTick);
            if (!qFuzzyCompare(ka + 1.0, kb + 1.0)) return ka < kb;
            return a->pos().x() < b->pos().x();
        });

        const int order = orderOf.value(t.id, 0);
        for (int i = 0; i < items.size(); ++i) {
            TimelineEvent d = items[i]->eventData();
            if (narrative) d.narrativeTick = i;  // re-inteiriza o eixo ativo
            else           d.storyOrder    = i;  // (e semeia quem estava em -1)
            d.x = tickX(i);
            d.y = railY(order);
            items[i]->setEventData(d);
        }
    }

    applyConnVisibility();
    update();
}

// ── Constelação (force-directed) ───────────────────────────────────────────────

QHash<QString, int> TimelineScene::orderMap() const
{
    QList<TimelineDef> sorted = m_timelines;
    std::sort(sorted.begin(), sorted.end(),
              [](const TimelineDef& a, const TimelineDef& b){ return a.railOrder < b.railOrder; });
    QHash<QString, int> m;
    for (int i = 0; i < sorted.size(); ++i) m.insert(sorted[i].id, i);
    return m;
}

void TimelineScene::buildEdges()
{
    m_seqEdges.clear();
    QHash<QString, QList<TimelineEventItem*>> byLine;
    for (auto* it : m_events) byLine[it->eventData().timelineId].append(it);
    for (auto it = byLine.begin(); it != byLine.end(); ++it) {
        QList<TimelineEventItem*>& items = it.value();
        std::sort(items.begin(), items.end(), [](TimelineEventItem* a, TimelineEventItem* b){
            return a->eventData().narrativeTick < b->eventData().narrativeTick;
        });
        for (int i = 1; i < items.size(); ++i)
            m_seqEdges.append({ items[i-1]->eventData().id, items[i]->eventData().id });
    }
}

void TimelineScene::seedConstellation()
{
    const QHash<QString, int> om = orderMap();
    auto* rnd = QRandomGenerator::global();
    for (auto* it : m_events) {
        const auto& d = it->eventData();
        const int order = om.value(d.timelineId, 0);
        const qreal jx = rnd->bounded(60) - 30;
        const qreal jy = rnd->bounded(60) - 30;
        it->setPos(tickX(d.narrativeTick) + jx, railY(order) + jy);
    }
}

void TimelineScene::startSim()
{
    m_simAlpha = 1.0;
    if (!m_simTimer) {
        m_simTimer = new QTimer(this);
        m_simTimer->setInterval(16);
        connect(m_simTimer, &QTimer::timeout, this, &TimelineScene::stepSim);
    }
    if (!m_simTimer->isActive()) m_simTimer->start();
}

void TimelineScene::stopSim()
{
    if (m_simTimer && m_simTimer->isActive()) m_simTimer->stop();
    m_simAlpha = 0.0;
    m_pinnedId.clear();
}

void TimelineScene::stepSim()
{
    const int n = m_events.size();
    if (n == 0 || m_viewMode != ViewMode::Constellation) { stopSim(); return; }

    // Constantes do modelo
    constexpr qreal kRep     = 9000.0;  // repulsão entre nós
    constexpr qreal kSpring  = 0.018;   // rigidez das molas (arestas)
    constexpr qreal kRest    = 120.0;   // comprimento de repouso da aresta
    constexpr qreal kGravity = 0.015;   // gravidade ao centro
    constexpr qreal kMaxDisp = 45.0;    // deslocamento máx por frame

    QVector<QPointF> pos(n);
    QVector<QPointF> disp(n, QPointF(0, 0));
    QHash<QString, int> idx;
    for (int i = 0; i < n; ++i) {
        pos[i] = m_events[i]->pos();
        idx.insert(m_events[i]->eventData().id, i);
    }

    // Repulsão (O(n²) — n pequeno)
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            QPointF d = pos[i] - pos[j];
            qreal dist2 = d.x()*d.x() + d.y()*d.y();
            if (dist2 < 1.0) { d = QPointF(rand0(), rand0()); dist2 = 1.0; }
            const qreal dist = std::sqrt(dist2);
            const QPointF dir = d / dist;
            const qreal f = kRep / dist2;
            disp[i] += dir * f;
            disp[j] -= dir * f;
        }
    }

    // Molas — backbone sequencial + conexões explícitas
    auto applySpring = [&](const QString& a, const QString& b) {
        const int ia = idx.value(a, -1), ib = idx.value(b, -1);
        if (ia < 0 || ib < 0) return;
        QPointF d = pos[ib] - pos[ia];
        const qreal dist = std::sqrt(d.x()*d.x() + d.y()*d.y());
        if (dist < 0.01) return;
        const QPointF dir = d / dist;
        const qreal f = kSpring * (dist - kRest);
        disp[ia] += dir * f;
        disp[ib] -= dir * f;
    };
    for (const auto& e : m_seqEdges)    applySpring(e.first, e.second);
    for (const auto* c : m_connections) applySpring(c->connData().fromEventId,
                                                     c->connData().toEventId);

    // Gravidade + integração
    for (int i = 0; i < n; ++i) {
        disp[i] -= pos[i] * kGravity;
        if (m_events[i]->eventData().id == m_pinnedId) continue; // nó preso
        QPointF mv = disp[i] * m_simAlpha;
        const qreal mlen = std::sqrt(mv.x()*mv.x() + mv.y()*mv.y());
        if (mlen > kMaxDisp) mv *= (kMaxDisp / mlen);
        m_simulating = true;          // silencia onEventPositionChanged
        m_events[i]->setPos(pos[i] + mv);
        m_simulating = false;
    }

    // Conexões acompanham
    for (auto* conn : m_connections) conn->invalidate();
    update();

    m_simAlpha *= 0.985;             // resfriamento
    if (m_simAlpha < 0.02) {
        stopSim();
        emit eventDataChanged();      // persiste o repouso uma vez
    }
}

// ── Canvas ──────────────────────────────────────────────────────────────────────

void TimelineScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    painter->fillRect(rect, m_bgColor);

    // ── Constelação: desenha o backbone (arestas sequenciais por trilho) ────────
    if (m_viewMode == ViewMode::Constellation) {
        painter->setRenderHint(QPainter::Antialiasing);
        for (const auto& e : m_seqEdges) {
            auto* a = m_eventById.value(e.first, nullptr);
            auto* b = m_eventById.value(e.second, nullptr);
            if (!a || !b) continue;
            QColor c = timelineColor(a->eventData().timelineId);
            c.setAlpha(110);
            painter->setPen(QPen(c, 1.6));
            painter->drawLine(a->pos(), b->pos());
        }
        return;
    }

    if (m_viewMode != ViewMode::Rail || m_timelines.isEmpty()) return;

    painter->setRenderHint(QPainter::Antialiasing);

    QList<TimelineDef> sorted = m_timelines;
    std::sort(sorted.begin(), sorted.end(),
              [](const TimelineDef& a, const TimelineDef& b){ return a.railOrder < b.railOrder; });

    QFont f; f.setPointSizeF(8.0); f.setBold(true);
    painter->setFont(f);

    for (int i = 0; i < sorted.size(); ++i) {
        const qreal y = railY(i);
        if (y < rect.top() - 20 || y > rect.bottom() + 20) continue;
        const QColor c = sorted[i].color;

        // linha-guia da faixa
        QColor line = c; line.setAlpha(70);
        painter->setPen(QPen(line, 2.0));
        painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));

        // rótulo "grudado" à esquerda (acompanha o scroll horizontal)
        const QString name = sorted[i].name.isEmpty() ? tr("Linha") : sorted[i].name;
        const qreal lx = rect.left() + 10.0;
        const QRectF tagR(lx, y - 26, painter->fontMetrics().horizontalAdvance(name) + 18, 18);
        QColor tagBg = c; tagBg.setAlpha(40);
        painter->setPen(Qt::NoPen);
        painter->setBrush(tagBg);
        painter->drawRoundedRect(tagR, 9, 9);
        QColor txt = c.lighter(160);
        painter->setPen(txt);
        painter->drawText(tagR.adjusted(9, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, name);
    }
}

void TimelineScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (!itemAt(event->scenePos(), QTransform())) {
        emit canvasDoubleClicked(event->scenePos());
        event->accept();
        return;
    }
    QGraphicsScene::mouseDoubleClickEvent(event);
}
