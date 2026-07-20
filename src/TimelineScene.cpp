#include "TimelineScene.h"
#include "TimelineChrono.h"
#include "TimelineConnItem.h"
#include "TimelineEventItem.h"

#include <QAction>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QQueue>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>
#include <QUuid>
#include <QVector>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kRailTop  = 70.0;
constexpr qreal kRailGap  = 96.0;
constexpr qreal kRailLeft = 150.0;  // calha à esquerda p/ os rótulos das faixas
constexpr qreal kTickGap  = 150.0;
constexpr qreal kDimOpacity = 0.16; // opacidade dos elementos fora do foco

// Ramificações: raio = idade do evento. Mais antigo (ordem menor) fica no
// centro; mais novo, na borda. O ângulo é livre → bagunça visual, ordem real.
constexpr qreal kTwoPi     = 6.283185307179586;
constexpr qreal kConstR0   = 55.0;  // raio do evento mais antigo (ordem 0) — +20%
constexpr qreal kConstRing = 106.0; // acréscimo de raio por passo de ordem — +20%

// Modo Espiral: cada linha é um anel concêntrico. Raio = importância (núcleo →
// borda); o tempo corre no ângulo (gira a partir do topo), com leve abertura do
// raio ao longo da volta p/ dar o efeito de espiral e marcar a direção.
constexpr qreal kSpiralR0    = 95.0;       // raio do anel mais interno (1ª linha)
constexpr qreal kSpiralRing  = 125.0;      // distância entre anéis de linhas
constexpr qreal kSpiralRise  = 58.0;       // abertura do raio ao longo de 1 volta
constexpr qreal kSpiralStart = -1.5707963; // -90° = topo; gira no sentido horário

// Rank de importância p/ ordenar os anéis: principal (núcleo) → secundária →
// backstory (borda). Desconhecido cai junto da secundária.
int weightRank(const QString& w) {
    if (w == QLatin1String(TimelineWeight::Primary))   return 0;
    if (w == QLatin1String(TimelineWeight::Backstory)) return 2;
    return 1; // secondary / padrão
}

// pequeno deslocamento aleatório p/ desempatar nós exatamente sobrepostos
qreal rand0() { return (QRandomGenerator::global()->bounded(100) - 50) / 50.0; }

// Parser cronológico movido pra TimelineChrono.h (reaproveitado por
// TimelinePanel.cpp pra comparar marcador de capítulo × data-base da história).
qreal markerChrono(const QString& raw, bool* okOut) { return TimelineChrono::parse(raw, okOut); }
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
        buildEdges();          // calcula ranks ANTES de semear (raio depende deles)
        seedConstellation();
        applyConnVisibility();
        startSim();
    } else if (m_viewMode == ViewMode::Spiral) {
        buildEdges();          // backbone p/ molas + desenho
        buildSpiralTargets();  // ponto-alvo de cada nó na espiral
        seedSpiral();          // posiciona perto do alvo; a sim assenta
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
    for (auto* item : m_events) {
        const QString tid = item->eventData().timelineId;
        item->setTimelineColor(timelineColor(tid));
        item->setTimelineWeight(timelineWeight(tid));
        item->setTimelineName(timelineName(tid));
    }
    relayout();
    update();
}

QColor TimelineScene::timelineColor(const QString& timelineId) const
{
    for (const auto& t : m_timelines)
        if (t.id == timelineId) return t.color;
    return QColor(QStringLiteral("#6c8ebf"));
}

QString TimelineScene::timelineWeight(const QString& timelineId) const
{
    for (const auto& t : m_timelines)
        if (t.id == timelineId) return t.weight;
    return QStringLiteral("secondary");
}

QString TimelineScene::timelineName(const QString& timelineId) const
{
    for (const auto& t : m_timelines)
        if (t.id == timelineId) return t.name.isEmpty() ? tr("Linha") : t.name;
    return {};
}

bool TimelineScene::isCharacterTimeline(const QString& timelineId) const
{
    for (const auto& t : m_timelines)
        if (t.id == timelineId)
            return t.kind == QLatin1String(TimelineKind::Character);
    return false;
}

bool TimelineScene::timelineVisibleInAxis(const QString& timelineId, bool narrativeAxis) const
{
    for (const auto& t : m_timelines) {
        if (t.id != timelineId) continue;
        // Personagem e trilha principal (história "adiante") ficam no eixo
        // Narrativa; Flashback (história "antes" da data-base) fica exclusivo
        // no outro eixo. Cada trilha aparece em só um dos dois, nunca nos dois.
        const bool narrativeKind = t.kind == QLatin1String(TimelineKind::Character)
                                 || t.kind == QLatin1String(TimelineKind::Main);
        return narrativeKind == narrativeAxis;
    }
    return false;
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
    connect(item, &TimelineEventItem::focusLineRequested,
            this, [this](const QString& timelineId, bool expand) { focusLine(timelineId, expand); });
    connect(item, &TimelineEventItem::shiftClicked,
            this, &TimelineScene::onEventShiftClicked);
}

TimelineEventItem* TimelineScene::addEvent(const TimelineEvent& data)
{
    auto* item = new TimelineEventItem(data);
    item->setTimelineColor(timelineColor(data.timelineId));
    item->setTimelineWeight(timelineWeight(data.timelineId));
    item->setTimelineName(timelineName(data.timelineId));
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

    if (m_viewMode == ViewMode::Constellation || m_viewMode == ViewMode::Spiral) {
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

    if (m_viewMode == ViewMode::Constellation || m_viewMode == ViewMode::Spiral) {
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
            item->setTimelineWeight(timelineWeight(newTid));
            item->setTimelineName(timelineName(newTid));
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

void TimelineScene::onEventShiftClicked(const QString& eventId)
{
    // Origem do fio = evento aberto no momento (≠ este).
    TimelineEventItem* source = nullptr;
    for (auto* it : m_events)
        if (it->isOpen() && it->eventData().id != eventId) { source = it; break; }

    if (!source) {
        // Nada aberto → mantém o atalho de expandir o foco da linha clicada.
        if (auto* it = m_eventById.value(eventId, nullptr))
            focusLine(it->eventData().timelineId, true);
        return;
    }

    const QString fromId = source->eventData().id;
    // Evita fio duplicado (em qualquer direção).
    for (const auto* c : m_connections) {
        const auto& d = c->connData();
        if ((d.fromEventId == fromId && d.toEventId == eventId)
         || (d.fromEventId == eventId && d.toEventId == fromId))
            return;
    }

    TimelineConn conn;
    conn.id          = QUuid::createUuid().toString(QUuid::WithoutBraces);
    conn.fromEventId = fromId;
    conn.toEventId   = eventId;
    conn.type        = QStringLiteral("reference"); // fio manual (cruzamento)
    addConnection(conn);
    emit eventDataChanged(); // persiste no timeline.json
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
    // Regra de eixo: Narrativa mostra a presença dos personagens; História mostra
    // os eventos de enredo (autorais); enredo de capítulo/cena (kind Main/
    // Backstory) aparece nos dois eixos sempre — ver timelineVisibleInAxis().
    const bool narrativeAxis = (m_axisMode == AxisMode::Narrative);
    for (auto* it : m_events)
        it->setVisible(timelineVisibleInAxis(it->eventData().timelineId, narrativeAxis));

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
        // Constelação/Espiral: esconde "sequence" (o backbone já as desenha).
        bool hide = (rail && sameRail)
                 || (!rail && d.type == QLatin1String("sequence"));
        // Regra de eixo: esconde conexões que tocam um evento oculto no eixo atual.
        if (from && !timelineVisibleInAxis(from->eventData().timelineId, narrativeAxis)) hide = true;
        if (to   && !timelineVisibleInAxis(to->eventData().timelineId,   narrativeAxis)) hide = true;
        conn->setVisible(!hide);
        conn->invalidate();
    }
    applyFocusDimming();
}

// ── Foco / Filtro por linha ──────────────────────────────────────────────────

void TimelineScene::setFocus(const QString& timelineId, int depth)
{
    m_focusTimelineId = timelineId;
    m_focusDepth      = qBound(0, depth, 3);
    applyFocusDimming();
    update();
    emit focusChanged();
}

void TimelineScene::clearFocus()
{
    if (m_focusTimelineId.isEmpty()) return;
    m_focusTimelineId.clear();
    applyFocusDimming();
    update();
    emit focusChanged();
}

void TimelineScene::focusLine(const QString& timelineId, bool expand)
{
    if (timelineId.isEmpty()) { clearFocus(); return; }
    int depth = 0;                               // clique simples: só a linha
    if (expand)
        depth = (timelineId == m_focusTimelineId) ? qMin(3, m_focusDepth + 1) // +1 anel
                                                  : 1;                         // outra linha: 1 salto
    setFocus(timelineId, depth);
}

QString TimelineScene::timelineIdAtRailPos(const QPointF& scenePos) const
{
    if (m_viewMode != ViewMode::Rail) return {};

    QList<TimelineDef> sorted = m_timelines;
    std::sort(sorted.begin(), sorted.end(),
              [](const TimelineDef& a, const TimelineDef& b){ return a.railOrder < b.railOrder; });

    const bool haveView = !views().isEmpty();
    const qreal viewLeft = haveView ? views().first()->mapToScene(0, 0).x() : 0.0;
    QFont f; f.setPointSizeF(8.0); f.setBold(true);
    const QFontMetrics fm(f);

    for (int i = 0; i < sorted.size(); ++i) {
        const qreal y = railY(i);
        // rótulo grudado à esquerda
        if (haveView) {
            const QString name = sorted[i].name.isEmpty() ? tr("Linha") : sorted[i].name;
            const QRectF tagR(viewLeft + 10.0, y - 26, fm.horizontalAdvance(name) + 18, 18);
            if (tagR.contains(scenePos)) return sorted[i].id;
        }
        // a própria linha-guia (tolerância vertical)
        if (std::abs(scenePos.y() - y) <= 6.0) return sorted[i].id;
    }
    return {};
}

QSet<QString> TimelineScene::eventsWithinFocus() const
{
    QSet<QString> inSet;
    if (m_focusTimelineId.isEmpty()) return inSet;

    // Grafo de saltos = as conexões persistidas (qualquer tipo).
    QHash<QString, QStringList> adj;
    for (const auto* c : m_connections) {
        const auto& d = c->connData();
        adj[d.fromEventId] << d.toEventId;
        adj[d.toEventId]   << d.fromEventId;
    }

    // Semente (distância 0) = todos os eventos da linha focada.
    QHash<QString, int> dist;
    QQueue<QString> q;
    for (const auto* it : m_events) {
        const auto& d = it->eventData();
        if (d.timelineId == m_focusTimelineId) {
            dist.insert(d.id, 0);
            inSet.insert(d.id);
            q.enqueue(d.id);
        }
    }
    // BFS limitada a m_focusDepth saltos.
    while (!q.isEmpty()) {
        const QString cur = q.dequeue();
        const int dc = dist.value(cur);
        if (dc >= m_focusDepth) continue;
        for (const QString& nb : adj.value(cur)) {
            if (!dist.contains(nb)) {
                dist.insert(nb, dc + 1);
                inSet.insert(nb);
                q.enqueue(nb);
            }
        }
    }
    return inSet;
}

void TimelineScene::applyFocusDimming()
{
    const bool hasTrackFocus = !m_focusTimelineId.isEmpty();
    m_focusSet = hasTrackFocus ? eventsWithinFocus() : QSet<QString>();

    if (!hasTrackFocus && !m_charFilterActive) {
        for (auto* it : m_events)      { it->setOpacity(1.0); it->setShowMarker(false); }
        for (auto* c  : m_connections) c->setOpacity(1.0);
        return;
    }
    for (auto* it : m_events) {
        const auto& d = it->eventData();
        bool lit = true;
        if (hasTrackFocus)       lit = lit && m_focusSet.contains(d.id);
        if (m_charFilterActive)  lit = lit && m_charFilterEventIds.contains(d.id);
        it->setOpacity(lit ? 1.0 : kDimOpacity);
        // marcador ao lado da bolinha só nos eventos da linha focada
        it->setShowMarker(hasTrackFocus && d.timelineId == m_focusTimelineId);
    }
    for (auto* c : m_connections) {
        const auto& d = c->connData();
        bool lit = true;
        if (hasTrackFocus) {
            lit = lit && m_focusSet.contains(d.fromEventId) && m_focusSet.contains(d.toEventId);
        }
        if (m_charFilterActive) {
            lit = lit && m_charFilterEventIds.contains(d.fromEventId)
                       && m_charFilterEventIds.contains(d.toEventId);
        }
        c->setOpacity(lit ? 1.0 : kDimOpacity);
    }
}

void TimelineScene::setCharacterFilter(bool active, const QSet<QString>& matchingEventIds)
{
    m_charFilterActive   = active;
    m_charFilterEventIds = matchingEventIds;
    applyFocusDimming();
}

// ── Layout ─────────────────────────────────────────────────────────────────────

void TimelineScene::relayout()
{
    if (m_viewMode == ViewMode::Constellation) {
        // Modelo efêmero: re-semeia e simula de novo.
        buildEdges();          // ranks ANTES de semear (raio depende deles)
        seedConstellation();
        applyConnVisibility();
        startSim();
        update();
        return;
    }

    if (m_viewMode == ViewMode::Spiral) {
        buildEdges();
        buildSpiralTargets();
        seedSpiral();
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

    // Eixo História: ordena pelo marcador de relógio quando há; senão cai na
    // ordem manual (storyOrder) jogada pro fim, depois na ordem de leitura.
    auto storyKey = [](const TimelineEvent& d) -> qreal {
        bool ok = false;
        const qreal c = markerChrono(d.timeMarker, &ok);
        if (ok) return c;
        // sem marcador cronológico: ordem manual, jogada bem pro fim
        return 1.0e12 + (d.storyOrder >= 0 ? d.storyOrder : d.narrativeTick);
    };

    for (const auto& t : sorted) {
        // eventos desta faixa
        QList<TimelineEventItem*> items;
        for (auto* it : m_events)
            if (it->eventData().timelineId == t.id) items.append(it);

        std::sort(items.begin(), items.end(),
                  [narrative, &storyKey](TimelineEventItem* a, TimelineEventItem* b) {
            const auto& da = a->eventData();
            const auto& db = b->eventData();
            const qreal ka = narrative ? da.narrativeTick : storyKey(da);
            const qreal kb = narrative ? db.narrativeTick : storyKey(db);
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
    m_ageRank.clear();
    const bool narrative = (m_axisMode == AxisMode::Narrative);

    QHash<QString, QList<TimelineEventItem*>> byLine;
    for (auto* it : m_events) byLine[it->eventData().timelineId].append(it);

    for (auto it = byLine.begin(); it != byLine.end(); ++it) {
        QList<TimelineEventItem*>& items = it.value();

        // backbone segue a ordem de leitura (narrativeTick)
        std::sort(items.begin(), items.end(), [](TimelineEventItem* a, TimelineEventItem* b){
            return a->eventData().narrativeTick < b->eventData().narrativeTick;
        });
        for (int i = 1; i < items.size(); ++i)
            m_seqEdges.append({ items[i-1]->eventData().id, items[i]->eventData().id });

        // rank p/ o RAIO (= idade): no eixo História é cronológico pelo marcador.
        // É a POSIÇÃO (0,1,2...) na linha, nunca o valor cru — senão uma data
        // distante jogaria o nó pra fora do canvas.
        if (!narrative) {
            std::sort(items.begin(), items.end(), [](TimelineEventItem* a, TimelineEventItem* b){
                const auto& da = a->eventData(); const auto& db = b->eventData();
                bool oa = false, ob = false;
                qreal ka = markerChrono(da.timeMarker, &oa);
                if (!oa) ka = 1.0e12 + (da.storyOrder >= 0 ? da.storyOrder : da.narrativeTick);
                qreal kb = markerChrono(db.timeMarker, &ob);
                if (!ob) kb = 1.0e12 + (db.storyOrder >= 0 ? db.storyOrder : db.narrativeTick);
                if (ka != kb) return ka < kb;
                return da.id < db.id;
            });
        }
        for (int i = 0; i < items.size(); ++i)
            m_ageRank.insert(items[i]->eventData().id, i);
    }
}

void TimelineScene::seedConstellation()
{
    const QHash<QString, int> om = orderMap();
    const int nT = qMax(1, m_timelines.size());
    auto* rnd = QRandomGenerator::global();
    for (auto* it : m_events) {
        const auto& d = it->eventData();
        const int order = om.value(d.timelineId, 0);

        // raio = idade = POSIÇÃO do evento na linha (anel); centro = mais antigo
        const int rnk = m_ageRank.value(d.id, 0);
        const qreal radius = kConstR0 + rnk * kConstRing + (rnd->bounded(40) - 20);

        // ângulo base por trilho (espalhado no círculo) + dispersão aleatória
        const qreal baseAng = (kTwoPi * order) / nT;
        const qreal ang = baseAng + (rnd->bounded(1000) / 1000.0 - 0.5) * 1.1;
        it->setPos(std::cos(ang) * radius, std::sin(ang) * radius);
    }
}

// ── Espiral (viva) ───────────────────────────────────────────────────────────
// Mesma simulação das Ramificações; muda só o ALVO de cada nó. Cada linha é um
// anel concêntrico ordenado por importância (principal no núcleo, backstory na
// borda); dentro do anel o tempo corre no ângulo e o raio abre um pouco ao longo
// da volta. A espiral emerge da física — repulsão e molas a fazem respirar.
void TimelineScene::buildSpiralTargets()
{
    m_spiralTarget.clear();

    QList<TimelineDef> sorted = m_timelines;
    std::sort(sorted.begin(), sorted.end(), [](const TimelineDef& a, const TimelineDef& b){
        const int wa = weightRank(a.weight), wb = weightRank(b.weight);
        if (wa != wb) return wa < wb;
        return a.railOrder < b.railOrder;
    });
    QHash<QString, int> ringOf;
    for (int i = 0; i < sorted.size(); ++i) ringOf.insert(sorted[i].id, i);

    const bool narrative = (m_axisMode == AxisMode::Narrative);
    auto storyKey = [](const TimelineEvent& d) -> qreal {
        bool ok = false;
        const qreal c = markerChrono(d.timeMarker, &ok);
        if (ok) return c;
        return 1.0e12 + (d.storyOrder >= 0 ? d.storyOrder : d.narrativeTick);
    };

    for (const auto& t : sorted) {
        QList<TimelineEventItem*> items;
        for (auto* it : m_events)
            if (it->eventData().timelineId == t.id) items.append(it);

        std::sort(items.begin(), items.end(),
                  [narrative, &storyKey](TimelineEventItem* a, TimelineEventItem* b) {
            const auto& da = a->eventData();
            const auto& db = b->eventData();
            const qreal ka = narrative ? da.narrativeTick : storyKey(da);
            const qreal kb = narrative ? db.narrativeTick : storyKey(db);
            if (!qFuzzyCompare(ka + 1.0, kb + 1.0)) return ka < kb;
            return a->eventData().id < b->eventData().id;
        });

        const int   k = ringOf.value(t.id, 0);
        const qreal R = kSpiralR0 + k * kSpiralRing;
        const int   n = items.size();
        for (int i = 0; i < n; ++i) {
            // frac em [0,1): divide por n (não n-1) p/ deixar uma fenda entre o
            // último e o primeiro evento — marca onde a volta começa/termina.
            const qreal frac = (n <= 1) ? 0.0 : qreal(i) / qreal(n);
            const qreal ang  = kSpiralStart + frac * kTwoPi;
            const qreal rad  = R + frac * kSpiralRise;
            m_spiralTarget.insert(items[i]->eventData().id,
                                  QPointF(std::cos(ang) * rad, std::sin(ang) * rad));
        }
    }
}

void TimelineScene::seedSpiral()
{
    auto* rnd = QRandomGenerator::global();
    for (auto* it : m_events) {
        const QPointF tgt = m_spiralTarget.value(it->eventData().id, QPointF(0, 0));
        it->setPos(tgt.x() + (rnd->bounded(40) - 20),
                   tgt.y() + (rnd->bounded(40) - 20));
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
    const bool sims = (m_viewMode == ViewMode::Constellation || m_viewMode == ViewMode::Spiral);
    if (n == 0 || !sims) { stopSim(); return; }
    const bool spiral = (m_viewMode == ViewMode::Spiral);

    // Constantes do modelo. Na Espiral o atrator ao ponto-alvo DOMINA: a repulsão
    // cai pra só evitar sobreposição e as molas das conexões desligam (conexão
    // cruzada rasgaria a forma). A vida vem da repulsão leve + arraste/reaquecer.
    const qreal kRep     = spiral ? 1800.0 : 11500.0; // repulsão entre nós (+respiro)
    const qreal kSpring  = spiral ? 0.0    : 0.018;   // molas (arestas) — off na espiral
    constexpr qreal kRest    = 144.0;   // comprimento de repouso da aresta — +20%
    constexpr qreal kRadialK = 0.030;   // rigidez do anel radial (raio = idade)
    constexpr qreal kSpiralK = 0.140;   // atração ao ponto-alvo da espiral (forte)
    constexpr qreal kMaxDisp = 45.0;    // deslocamento máx por frame

    QVector<QPointF> pos(n);
    QVector<QPointF> disp(n, QPointF(0, 0));
    QHash<QString, int> idx;
    for (int i = 0; i < n; ++i) {
        pos[i] = m_events[i]->pos();
        idx.insert(m_events[i]->eventData().id, i);
    }

    // Repulsão (O(n²) — n pequeno). Na Espiral é de alcance curto: só separa nós
    // que se encostam, sem empurrar de longe (senão borra a forma).
    constexpr qreal kRepRange2 = 78.0 * 78.0;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            QPointF d = pos[i] - pos[j];
            qreal dist2 = d.x()*d.x() + d.y()*d.y();
            if (spiral && dist2 > kRepRange2) continue; // fora do alcance
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
    if (!spiral) { // na Espiral as molas ficam off (o atrator organiza tudo)
        for (const auto& e : m_seqEdges)    applySpring(e.first, e.second);
        for (const auto* c : m_connections) applySpring(c->connData().fromEventId,
                                                         c->connData().toEventId);
    }

    // Atração estrutural + integração
    for (int i = 0; i < n; ++i) {
        const QString id = m_events[i]->eventData().id;
        if (spiral) {
            // Espiral: puxa o nó pro seu ponto-alvo (anel = importância, ângulo =
            // tempo). Repulsão/molas deformam organicamente → a espiral "respira".
            const QPointF tgt = m_spiralTarget.value(id, QPointF(0, 0));
            disp[i] += (tgt - pos[i]) * kSpiralK;
        } else {
            // Ramificações: puxa só pro raio-alvo (idade); ângulo fica livre.
            const int rnk = m_ageRank.value(id, 0);
            const qreal target = kConstR0 + rnk * kConstRing;
            qreal r = std::sqrt(pos[i].x() * pos[i].x() + pos[i].y() * pos[i].y());
            QPointF dir;
            if (r < 0.01) { dir = QPointF(rand0(), rand0()); r = 0.01; }
            else          dir = pos[i] / r;
            disp[i] += dir * (target - r) * kRadialK;
        }

        if (id == m_pinnedId) continue; // nó preso
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

    // ── Ramificações / Espiral: backbone (arestas sequenciais por trilho) ───────
    // Mesmo desenho; o que muda é a POSIÇÃO dos nós (livre vs. espiral).
    // ── Espiral: backbone de cada linha como CURVA SUAVE pelos eventos ──────────
    // É o que faz a espiral "aparecer" — um traço contínuo seguindo cada anel.
    if (m_viewMode == ViewMode::Spiral) {
        painter->setRenderHint(QPainter::Antialiasing);
        const bool focusing = !m_focusTimelineId.isEmpty();

        // núcleo discreto (origem da espiral)
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 22));
        painter->drawEllipse(QPointF(0, 0), 4.0, 4.0);

        // eventos por linha, na MESMA ordem que posicionou os nós (eixo ativo)
        const bool narrative = (m_axisMode == AxisMode::Narrative);
        auto storyKey = [](const TimelineEvent& d) -> qreal {
            bool ok = false;
            const qreal c = markerChrono(d.timeMarker, &ok);
            if (ok) return c;
            return 1.0e12 + (d.storyOrder >= 0 ? d.storyOrder : d.narrativeTick);
        };
        QHash<QString, QList<TimelineEventItem*>> byLine;
        for (auto* it : m_events) byLine[it->eventData().timelineId].append(it);

        const bool showChars = (m_axisMode == AxisMode::Narrative);
        for (auto it = byLine.begin(); it != byLine.end(); ++it) {
            QList<TimelineEventItem*>& items = it.value();
            if (items.size() < 2) continue;
            if (!timelineVisibleInAxis(it.key(), showChars)) continue; // regra de eixo
            std::sort(items.begin(), items.end(),
                      [narrative, &storyKey](TimelineEventItem* a, TimelineEventItem* b){
                const auto& da = a->eventData();
                const auto& db = b->eventData();
                const qreal ka = narrative ? da.narrativeTick : storyKey(da);
                const qreal kb = narrative ? db.narrativeTick : storyKey(db);
                if (!qFuzzyCompare(ka + 1.0, kb + 1.0)) return ka < kb;
                return da.id < db.id;
            });

            // curva suave (controle = ponto anterior, fim = ponto médio)
            QPainterPath path;
            path.moveTo(items.first()->pos());
            for (int i = 1; i < items.size(); ++i) {
                const QPointF prev = items[i - 1]->pos();
                const QPointF cur  = items[i]->pos();
                path.quadTo(prev, (prev + cur) * 0.5);
            }
            path.lineTo(items.last()->pos());

            QColor c = timelineColor(it.key());
            const qreal mult = (!focusing || it.key() == m_focusTimelineId) ? 1.0 : 0.30;
            c.setAlpha(int(150 * mult));
            const QString w = timelineWeight(it.key());
            QPen pen(c, weightLineWidth(w));
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            if (weightIsDashed(w)) {
                pen.setStyle(Qt::CustomDashLine);
                pen.setDashPattern({5.0, 4.0});
            }
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(path);
        }
        return;
    }

    // ── Ramificações: backbone reto entre nós consecutivos ──────────────────────
    if (m_viewMode == ViewMode::Constellation) {
        painter->setRenderHint(QPainter::Antialiasing);
        const bool focusing = !m_focusTimelineId.isEmpty();
        const bool showChars = (m_axisMode == AxisMode::Narrative);
        for (const auto& e : m_seqEdges) {
            auto* a = m_eventById.value(e.first, nullptr);
            auto* b = m_eventById.value(e.second, nullptr);
            if (!a || !b) continue;
            if (!timelineVisibleInAxis(a->eventData().timelineId, showChars)) continue;
            QColor c = timelineColor(a->eventData().timelineId);
            int alpha = 110;
            if (focusing) {
                const bool lit = m_focusSet.contains(e.first)
                              && m_focusSet.contains(e.second);
                alpha = lit ? 110 : 26;
            }
            c.setAlpha(alpha);
            // backbone mais fino que o trilho, mas mantém a hierarquia de importância
            const QString w = timelineWeight(a->eventData().timelineId);
            QPen bbPen(c, qMax(1.2, weightLineWidth(w) - 1.4));
            if (weightIsDashed(w)) {
                bbPen.setStyle(Qt::CustomDashLine);
                bbPen.setDashPattern({5.0, 4.0});
            }
            painter->setPen(bbPen);
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

    const bool focusing = !m_focusTimelineId.isEmpty();
    const bool showChars = (m_axisMode == AxisMode::Narrative);
    for (int i = 0; i < sorted.size(); ++i) {
        const qreal y = railY(i);
        if (y < rect.top() - 20 || y > rect.bottom() + 20) continue;
        if (!timelineVisibleInAxis(sorted[i].id, showChars)) continue; // regra de eixo
        const QColor c = sorted[i].color;
        // Faixa focada fica plena; as demais esmaecem (mas seguem visíveis).
        const qreal mult = (!focusing || sorted[i].id == m_focusTimelineId) ? 1.0 : 0.32;

        // linha-guia da faixa — espessura/estilo pela importância
        QColor line = c; line.setAlpha(int(70 * mult));
        QPen guidePen(line, weightLineWidth(sorted[i].weight));
        guidePen.setCapStyle(Qt::RoundCap);
        if (weightIsDashed(sorted[i].weight)) {
            guidePen.setStyle(Qt::CustomDashLine);
            guidePen.setDashPattern({5.0, 4.0}); // tracejado de backstory/flashback
        }
        painter->setPen(guidePen);
        painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));

        // rótulo "grudado" à esquerda (acompanha o scroll horizontal)
        const QString name = sorted[i].name.isEmpty() ? tr("Linha") : sorted[i].name;
        const qreal lx = rect.left() + 10.0;
        const QRectF tagR(lx, y - 26, painter->fontMetrics().horizontalAdvance(name) + 18, 18);
        QColor tagBg = c; tagBg.setAlpha(int(40 * mult));
        painter->setPen(Qt::NoPen);
        painter->setBrush(tagBg);
        painter->drawRoundedRect(tagR, 9, 9);
        QColor txt = c.lighter(160); txt.setAlphaF(mult);
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

void TimelineScene::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    // Eventos têm o próprio menu (clique-direito na bolinha) — deixa-os primeiro.
    QGraphicsScene::contextMenuEvent(event);
    if (event->isAccepted()) return;

    // Clique-direito numa faixa/rótulo (Modo Trilho) → editar a linha.
    const QString id = timelineIdAtRailPos(event->scenePos());
    if (id.isEmpty()) return;

    QMenu menu;
    auto* actEdit = menu.addAction(tr("Editar linha..."));
    auto* actDelete = menu.addAction(tr("Excluir linha..."));
    QAction* chosen = menu.exec(event->screenPos());
    if (chosen == actEdit)
        emit editTimelineRequested(id);
    else if (chosen == actDelete)
        emit deleteTimelineRequested(id);
    event->accept();
}
