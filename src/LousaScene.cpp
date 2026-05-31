#include "LousaScene.h"

#include "CardItem.h"
#include "ConnectionItem.h"
#include "ZoneItem.h"

#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QGuiApplication>
#include <QPainter>
#include <QTimer>
#include <algorithm>
#include <cmath>

// ─── Util ──────────────────────────────────────────────────────────────────

static qreal distToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const QPointF ab = b - a;
    const qreal len2 = ab.x()*ab.x() + ab.y()*ab.y();
    if (len2 < 1e-9) return QLineF(p, a).length();
    const qreal t = qBound(0.0, (QPointF::dotProduct(p-a, ab))/len2, 1.0);
    return QLineF(p, a + t*ab).length();
}

static QPointF projectOnSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const QPointF ab = b - a;
    const qreal len2 = ab.x()*ab.x() + ab.y()*ab.y();
    if (len2 < 1e-9) return a;
    const qreal t = qBound(0.0, QPointF::dotProduct(p-a, ab)/len2, 1.0);
    return a + t*ab;
}

static QPointF cardTopCenter(const CardItem* c)
{
    return c->pos() + QPointF(c->cardData().width / 2.0, 0.0);
}

// ─── Constructor ───────────────────────────────────────────────────────────

LousaScene::LousaScene(QObject* parent)
    : QGraphicsScene(parent)
{
    setSceneRect(-12000, -12000, 24000, 24000);

    m_snapTimer = new QTimer(this);
    m_snapTimer->setSingleShot(true);
    m_snapTimer->setInterval(1000);
    connect(m_snapTimer, &QTimer::timeout, this, &LousaScene::onSnapTimerFired);
}

// ─── Canvas color + background ─────────────────────────────────────────────

void LousaScene::setCanvasColor(const QColor& color)
{
    if (m_color == color) return;
    m_color = color;
    update();
}

void LousaScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    painter->fillRect(rect, m_color);
    constexpr qreal kSpacing = 32.0;
    const int r = m_color.red()   + (255 - m_color.red())   / 6;
    const int g = m_color.green() + (255 - m_color.green()) / 6;
    const int b = m_color.blue()  + (255 - m_color.blue())  / 6;
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(r, g, b));
    const qreal x0 = std::floor(rect.left()  / kSpacing) * kSpacing;
    const qreal y0 = std::floor(rect.top()   / kSpacing) * kSpacing;
    for (qreal x = x0; x <= rect.right();  x += kSpacing)
        for (qreal y = y0; y <= rect.bottom(); y += kSpacing)
            painter->drawEllipse(QPointF(x, y), 1.2, 1.2);
}

// ─── Cards ──────────────────────────────────────────────────────────────────

CardItem* LousaScene::addCard(const CanvasCard& data)
{
    auto* item = new CardItem(data);
    addItem(item);
    m_cards.append(item);
    connect(item, &CardItem::dataChanged,      this, &LousaScene::cardDataChanged);
    connect(item, &CardItem::positionChanged,  this, &LousaScene::onCardPositionChanged);
    connect(item, &CardItem::deleteRequested,  this, [this](const QString& id) {
        emit undoSnapshotRequested();
        removeCard(id);
    });
    connect(item, &CardItem::stashRequested, this, [this](const QString& id) {
        if (CardItem* c = findCard(id)) {
            emit undoSnapshotRequested();
            emit cardStashRequested(c->cardData());
            removeCard(id);
        }
    });
    connect(item, &CardItem::createDocRequested, this, [this](const QString& id) {
        if (CardItem* c = findCard(id)) emit cardCreateDocRequested(c->cardData());
    });
    connect(item, &CardItem::createTimelineEventRequested, this, [this](const QString& id) {
        if (CardItem* c = findCard(id)) emit cardCreateTimelineEventRequested(c->cardData());
    });
    connect(item, &CardItem::pinDragStarted, this,
            [this](const QString& fromId, const QPointF& pinScene) {
        startPinDrag(fromId, pinScene);
    });
    connect(item, &CardItem::cardPressed, this, [this, item]() {
        onCardPressedSelect(item);
    });
    connect(item, &CardItem::gestureStarted, this, &LousaScene::undoSnapshotRequested);
    connect(item, &CardItem::dragStarted, this, &LousaScene::onCardDragStarted);
    connect(item, &CardItem::draggedBy,   this, &LousaScene::onCardDraggedBy);
    return item;
}

void LousaScene::selectOnlyCard(CardItem* sel)
{
    for (CardItem* c : m_cards)
        c->setCardSelected(c == sel);
}

void LousaScene::clearCardSelection()
{
    for (CardItem* c : m_cards)
        c->setCardSelected(false);
}

void LousaScene::toggleCardSelection(CardItem* c)
{
    if (c) c->setCardSelected(!c->isCardSelected());
}

void LousaScene::addCardToSelection(CardItem* c)
{
    if (c && !c->isCardSelected()) c->setCardSelected(true);
}

QList<CardItem*> LousaScene::selectedCardItems() const
{
    QList<CardItem*> out;
    for (CardItem* c : m_cards)
        if (c->isCardSelected()) out.append(c);
    return out;
}

// Resolve a seleção quando um card é clicado.
// Shift = alterna esse card; clique normal num card não-selecionado = seleciona só ele;
// clique num card já selecionado = mantém a seleção (permite arrastar o grupo).
void LousaScene::onCardPressedSelect(CardItem* item)
{
    if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)
        toggleCardSelection(item);
    else if (!item->isCardSelected())
        selectOnlyCard(item);
}

void LousaScene::onCardDragStarted(const QString& id)
{
    m_groupOrigins.clear();
    m_groupLeader.clear();
    CardItem* leader = findCard(id);
    if (!leader || !leader->isCardSelected()) return;  // arrastando card avulso
    const QList<CardItem*> sel = selectedCardItems();
    if (sel.size() < 2) return;                         // sem grupo
    m_groupLeader = id;
    for (CardItem* c : sel)
        m_groupOrigins.insert(c->cardData().id, c->pos());
}

void LousaScene::onCardDraggedBy(const QString& id, const QPointF& delta)
{
    if (id != m_groupLeader || m_groupOrigins.isEmpty()) return;
    for (auto it = m_groupOrigins.constBegin(); it != m_groupOrigins.constEnd(); ++it) {
        if (it.key() == id) continue;  // o líder já se moveu sozinho
        if (CardItem* c = findCard(it.key()))
            c->setPos(it.value() + delta);
    }
}

void LousaScene::removeCard(const QString& id)
{
    // Remove de conexões como waypoint
    for (ConnectionItem* ci : m_connections) {
        CanvasConnection d = ci->connData();
        if (d.waypointCardIds.removeAll(id) > 0) {
            ci->setConnData(d);
        }
    }
    // Remove conexões que têm este card como from ou to
    QList<QString> toRemove;
    for (const ConnectionItem* ci : m_connections)
        if (ci->connData().fromId == id || ci->connData().toId == id)
            toRemove << ci->connData().id;
    for (const QString& cid : toRemove) removeConnection(cid);

    for (int i = 0; i < m_cards.size(); ++i) {
        if (m_cards[i]->cardData().id == id) {
            removeItem(m_cards[i]);
            delete m_cards[i];
            m_cards.removeAt(i);
            emit cardDataChanged();
            return;
        }
    }
}

void LousaScene::clearCards()
{
    for (auto* c : m_cards) { removeItem(c); delete c; }
    m_cards.clear();
}

QList<CanvasCard> LousaScene::allCardData() const
{
    QList<CanvasCard> out;
    for (const auto* c : m_cards) out.append(c->cardData());
    return out;
}

CardItem* LousaScene::findCard(const QString& id) const
{
    for (CardItem* c : m_cards) if (c->cardData().id == id) return c;
    return nullptr;
}

// ─── Conexões ───────────────────────────────────────────────────────────────

ConnectionItem* LousaScene::addConnection(const CanvasConnection& data)
{
    auto* item = new ConnectionItem(data, this);
    addItem(item);
    m_connections.append(item);
    connect(item, &ConnectionItem::removeRequested, this, [this](const QString& id) {
        removeConnection(id);
    });
    emit connectionDataChanged();
    return item;
}

void LousaScene::removeConnection(const QString& id)
{
    // Deslinka waypoints desta conexão
    for (CardItem* c : m_cards) {
        CanvasCard d = c->cardData();
        if (d.linkedToConn == id) {
            d.linkedToConn.clear();
            // Não chamamos setCardData aqui — não existe. Emitimos dataChanged
            // via o cardItem interno. Por ora só atualizamos o display.
            c->setSnapping(false);
        }
    }
    for (int i = 0; i < m_connections.size(); ++i) {
        if (m_connections[i]->connData().id == id) {
            removeItem(m_connections[i]);
            delete m_connections[i];
            m_connections.removeAt(i);
            emit connectionDataChanged();
            return;
        }
    }
}

void LousaScene::clearConnections()
{
    for (auto* c : m_connections) { removeItem(c); delete c; }
    m_connections.clear();
}

QList<CanvasConnection> LousaScene::allConnectionData() const
{
    QList<CanvasConnection> out;
    for (const auto* c : m_connections) out.append(c->connData());
    return out;
}

ConnectionItem* LousaScene::findConnection(const QString& id) const
{
    for (ConnectionItem* c : m_connections) if (c->connData().id == id) return c;
    return nullptr;
}

// ─── Zonas ──────────────────────────────────────────────────────────────────

ZoneItem* LousaScene::addZone(const CanvasZone& data)
{
    auto* item = new ZoneItem(data);
    addItem(item);
    m_zones.append(item);
    connect(item, &ZoneItem::dataChanged, this, [this](const CanvasZone& d) {
        // Actualiza m_data na lista
        for (ZoneItem* zi : m_zones)
            if (zi->zoneData().id == d.id) { /* ZoneItem já atualizou internamente */ break; }
        emit zoneDataChanged();
    });
    connect(item, &ZoneItem::removeRequested, this, [this](const QString& id) {
        emit undoSnapshotRequested();
        removeZone(id);
    });
    connect(item, &ZoneItem::gestureStarted, this, &LousaScene::undoSnapshotRequested);
    connect(item, &ZoneItem::zoneClicked, this, &LousaScene::onZoneClicked);
    connect(item, &ZoneItem::exportRequested, this, &LousaScene::zoneExportRequested);
    connect(item, &ZoneItem::dragStartedWithContents, this, &LousaScene::onZoneDragStartedWithContents);
    connect(item, &ZoneItem::draggedBy, this, &LousaScene::onZoneDraggedBy);
    return item;
}

void LousaScene::clearZoneSelection()
{
    m_selectedZoneId.clear();
    for (ZoneItem* z : m_zones) z->setSelected(false);
}

void LousaScene::onZoneClicked(const QString& id)
{
    m_selectedZoneId = id;
    for (ZoneItem* z : m_zones)
        z->setSelected(z->zoneData().id == id);
}

void LousaScene::onZoneDragStartedWithContents(const QString& id, bool withContents)
{
    m_zoneContentOrigins.clear();
    m_zoneDragLeader.clear();
    if (!withContents) return;
    // Acha a zona e captura os cards inteiramente dentro dela.
    ZoneItem* zone = nullptr;
    for (ZoneItem* z : m_zones) if (z->zoneData().id == id) { zone = z; break; }
    if (!zone) return;
    const QPointF zp = zone->pos();
    const CanvasZone zd = zone->zoneData();
    const QRectF zr(zp.x(), zp.y(), zd.width, zd.height);
    m_zoneDragLeader = id;
    for (CardItem* c : m_cards) {
        const CanvasCard d = c->cardData();
        const QRectF cr(d.x, d.y, d.width, d.height);
        if (zr.contains(cr))
            m_zoneContentOrigins.insert(d.id, c->pos());
    }
}

void LousaScene::onZoneDraggedBy(const QString& id, const QPointF& delta)
{
    if (id != m_zoneDragLeader || m_zoneContentOrigins.isEmpty()) return;
    for (auto it = m_zoneContentOrigins.constBegin(); it != m_zoneContentOrigins.constEnd(); ++it)
        if (CardItem* c = findCard(it.key()))
            c->setPos(it.value() + delta);
}

void LousaScene::removeZone(const QString& id)
{
    for (int i = 0; i < m_zones.size(); ++i) {
        if (m_zones[i]->zoneData().id == id) {
            removeItem(m_zones[i]);
            delete m_zones[i];
            m_zones.removeAt(i);
            emit zoneDataChanged();
            return;
        }
    }
}

void LousaScene::clearZones()
{
    for (auto* z : m_zones) { removeItem(z); delete z; }
    m_zones.clear();
}

QList<CanvasZone> LousaScene::allZoneData() const
{
    QList<CanvasZone> out;
    for (const ZoneItem* z : m_zones) {
        CanvasZone d = z->zoneData();
        const QPointF p = z->pos();
        d.x = p.x(); d.y = p.y();
        out.append(d);
    }
    return out;
}

// ─── Atualização de conexões quando card se move ─────────────────────────────

void LousaScene::onCardPositionChanged(const QString& cardId)
{
    for (ConnectionItem* ci : m_connections) {
        const auto& d = ci->connData();
        if (d.fromId == cardId || d.toId == cardId || d.waypointCardIds.contains(cardId)) {
            ci->invalidateGeometry();
        }
    }
    // Verifica snap para cards arrastáveis (note/comment sem linkedToConn)
    const CardItem* card = findCard(cardId);
    if (card) {
        const CanvasCard& cd = card->cardData();
        if ((cd.type == QStringLiteral("note") || cd.type == QStringLiteral("comment"))
            && cd.linkedToConn.isEmpty()) {
            checkSnapForCard(cardId, cardTopCenter(card));
        }
    }
}

// ─── Pin drag ────────────────────────────────────────────────────────────────

void LousaScene::startPinDrag(const QString& fromCardId, const QPointF& fromScene)
{
    m_dragFromId = fromCardId;
    m_ghostFrom  = fromScene;

    if (!m_ghostLine) {
        m_ghostLine = new QGraphicsLineItem();
        m_ghostLine->setZValue(100);
        QPen ghostPen(QColor(110, 168, 254, 153), 2, Qt::DashLine);
        ghostPen.setDashPattern({6, 4});
        m_ghostLine->setPen(ghostPen);
        addItem(m_ghostLine);
    }
    m_ghostLine->setLine(QLineF(fromScene, fromScene));
    m_ghostLine->setVisible(true);
}

void LousaScene::updatePinDrag(const QPointF& cursorScene)
{
    if (m_ghostLine && !m_dragFromId.isEmpty())
        m_ghostLine->setLine(QLineF(m_ghostFrom, cursorScene));
}

void LousaScene::endPinDrag(const QPointF& cursorScene)
{
    if (m_ghostLine) m_ghostLine->setVisible(false);

    if (m_dragFromId.isEmpty()) return;
    const QString fromId = m_dragFromId;
    m_dragFromId.clear();

    // Encontra o card alvo sob o cursor
    const QList<QGraphicsItem*> hits = items(cursorScene);
    CardItem* target = nullptr;
    for (QGraphicsItem* it : hits) {
        auto* ci = dynamic_cast<CardItem*>(it);
        if (ci && ci->cardData().id != fromId) { target = ci; break; }
    }
    if (!target) return;
    emit pendingConnection(fromId, target->cardData().id);
}

void LousaScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (!m_dragFromId.isEmpty())
        updatePinDrag(event->scenePos());
    QGraphicsScene::mouseMoveEvent(event);
}

void LousaScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (!m_dragFromId.isEmpty())
        endPinDrag(event->scenePos());
    QGraphicsScene::mouseReleaseEvent(event);
}

// ─── Snap de waypoint ────────────────────────────────────────────────────────

void LousaScene::checkSnapForCard(const QString& cardId, const QPointF& topCenter)
{
    constexpr qreal kSnapDist = 30.0;

    ConnectionItem* nearest = nullptr;
    qreal minDist = kSnapDist + 1.0;

    for (ConnectionItem* ci : m_connections) {
        const auto& d = ci->connData();
        if (d.fromId == cardId || d.toId == cardId) continue;
        if (d.waypointCardIds.contains(cardId)) continue;

        // Distância ao segmento principal (from → to, ignorando waypoints para o snap)
        const CardItem* from = findCard(d.fromId);
        const CardItem* to   = findCard(d.toId);
        if (!from || !to) continue;

        const qreal dist = distToSegment(topCenter, cardTopCenter(from), cardTopCenter(to));
        if (dist < minDist) { minDist = dist; nearest = ci; }
    }

    if (nearest && minDist < kSnapDist) {
        const QString connId = nearest->connData().id;
        if (m_snapCardId == cardId && m_snapConnId == connId) return; // já esperando

        cancelSnap();
        m_snapCardId = cardId;
        m_snapConnId = connId;
        // Mostra glow no card
        CardItem* card = findCard(cardId);
        if (card) card->setSnapping(true, nearest->connData().color);
        // Glow na conexão (já tratado via m_hovered se quiser — ou não, por ora)
        m_snapTimer->start();
    } else {
        if (m_snapCardId == cardId) cancelSnap();
    }
}

void LousaScene::onSnapTimerFired()
{
    if (m_snapCardId.isEmpty() || m_snapConnId.isEmpty()) return;

    CardItem*       card = findCard(m_snapCardId);
    ConnectionItem* conn = findConnection(m_snapConnId);
    if (!card || !conn) { cancelSnap(); return; }

    const CardItem* from = findCard(conn->connData().fromId);
    const CardItem* to   = findCard(conn->connData().toId);
    if (!from || !to) { cancelSnap(); return; }

    // Projeta o topo-centro do card no segmento
    const QPointF tc   = cardTopCenter(card);
    const QPointF proj = projectOnSegment(tc, cardTopCenter(from), cardTopCenter(to));
    const qreal newX   = proj.x() - card->cardData().width / 2.0;
    const qreal newY   = proj.y();
    card->setPos(newX, newY);

    // Vincula card à conexão: adota a cor + linkedToConn (igual ao Mira 1)
    card->setSnapping(false);
    card->setSnapConnected(conn->connData().color, m_snapConnId);

    // Adiciona como waypoint na conexão
    CanvasConnection cd2 = conn->connData();
    if (!cd2.waypointCardIds.contains(m_snapCardId))
        cd2.waypointCardIds.append(m_snapCardId);
    conn->setConnData(cd2);

    emit cardDataChanged();
    emit connectionDataChanged();
    cancelSnap();
}

void LousaScene::cancelSnap()
{
    m_snapTimer->stop();
    if (!m_snapCardId.isEmpty()) {
        CardItem* card = findCard(m_snapCardId);
        if (card) card->setSnapping(false);
    }
    m_snapCardId.clear();
    m_snapConnId.clear();
}
