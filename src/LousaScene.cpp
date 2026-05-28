#include "LousaScene.h"

#include "CardItem.h"
#include "ConnectionItem.h"

#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
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
        removeCard(id);
    });
    connect(item, &CardItem::pinDragStarted, this,
            [this](const QString& fromId, const QPointF& pinScene) {
        startPinDrag(fromId, pinScene);
    });
    return item;
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
