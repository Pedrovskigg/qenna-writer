#include "LousaScene.h"

#include "CardItem.h"
#include "ConnectionItem.h"
#include "ZoneItem.h"

#include "Theme.h"

#include <QGraphicsLineItem>
#include <QGraphicsView>
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
    return c->pinScenePos();
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
    if (m_color == color && m_color.isValid() == color.isValid()) return;
    m_color = color;
    refreshBoardLook();
}

QColor LousaScene::effectiveCanvasColor() const
{
    if (m_style == QStringLiteral("cork"))       return QColor(0x9c, 0x74, 0x4c);
    if (m_style == QStringLiteral("whiteboard")) return QColor(0xf4, 0xf3, 0xee);
    if (m_color.isValid()) return m_color;
    const QColor themed(Theme::appBackground());
    return themed.isValid() ? themed : QColor(0x1a, 0x1a, 0x19);
}

void LousaScene::setBoardStyle(const QString& style)
{
    static const QStringList kStyles = { QStringLiteral("dots"), QStringLiteral("grid"),
        QStringLiteral("lines"), QStringLiteral("plain"), QStringLiteral("cork"),
        QStringLiteral("whiteboard") };
    const QString st = kStyles.contains(style) ? style : QStringLiteral("dots");
    if (st == m_style) return;
    m_style = st;
    refreshBoardLook();
}

void LousaScene::refreshBoardLook()
{
    const QColor eff = effectiveCanvasColor();
    CardItem::setBoardIsLight(eff.lightness() > 150);
    CardItem::setBoardColor(eff);
    ZoneItem::setBoardColor(eff);
    for (CardItem* c : m_cards) c->onBoardChanged();
    for (QGraphicsView* v : views()) v->resetCachedContent();
    for (ConnectionItem* c : m_connections) c->update();
    update();
    emit boardLookChanged();
}

void LousaScene::setTiltEnabled(bool on)
{
    if (CardItem::tiltEnabled() == on) return;
    CardItem::setTiltEnabled(on);
    for (CardItem* c : m_cards) c->refreshTilt();
    for (ConnectionItem* c : m_connections) c->invalidateGeometry();
}

void LousaScene::setViewZoom(qreal zoom)
{
    ZoneItem::setViewZoom(m_zones, zoom);
}

QRectF LousaScene::contentBounds() const
{
    QRectF r;
    for (const CardItem* c : m_cards) {
        if (c->isSticker()) {
            // a caixa do adesivo tem folga pras alças; no quadro conta só ele
            const CanvasCard d = c->cardData();
            r = r.united(c->mapRectToScene(QRectF(-12, -12, d.width + 24, d.height + 24)));
        } else {
            r = r.united(c->sceneBoundingRect());
        }
    }
    for (const ZoneItem* z : m_zones)       r = r.united(z->sceneBoundingRect());
    for (const ConnectionItem* c : m_connections) r = r.united(c->boundingRect());
    return r;
}

void LousaScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    if (m_skipBackground) return;
    const QColor base = effectiveCanvasColor();
    const bool light = base.lightness() > 150;

    if (m_style == QStringLiteral("cork")) {
        if (m_corkTile.isNull()) {
            // Cortiça: grãos claros e escuros espalhados, sempre o mesmo desenho.
            QImage img(160, 160, QImage::Format_RGB32);
            img.fill(base);
            QPainter tp(&img);
            tp.setRenderHint(QPainter::Antialiasing);
            tp.setPen(Qt::NoPen);
            quint32 seed = 0x9e3779b9u;
            auto rnd = [&seed]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.0; };
            for (int i = 0; i < 900; ++i) {
                const qreal x = rnd() * 160, y = rnd() * 160, r = 0.5 + rnd() * 1.5;
                const bool dark = rnd() < 0.6;
                tp.setBrush(dark ? QColor(60, 34, 14, 40 + int(rnd() * 60))
                                 : QColor(255, 228, 190, 25 + int(rnd() * 45)));
                tp.drawEllipse(QPointF(x, y), r, r * (0.6 + rnd() * 0.6));
            }
            tp.end();
            m_corkTile = QPixmap::fromImage(img);
        }
        const qreal ox = std::fmod(std::fmod(rect.left(), 160.0) + 160.0, 160.0);
        const qreal oy = std::fmod(std::fmod(rect.top(), 160.0) + 160.0, 160.0);
        painter->drawTiledPixmap(rect, m_corkTile, QPointF(ox, oy));
        return;
    }

    painter->fillRect(rect, base);
    if (m_style == QStringLiteral("plain")) return;

    // Com o zoom longe, os pontos/linhas a cada 32 px viram manchas (moiré):
    // o espaçamento dobra até ficar com pelo menos 14 px na tela, e o ponto
    // mantém o mesmo tamanho na tela em qualquer zoom.
    const qreal scale = qMax(0.01, painter->worldTransform().m11());
    qreal kSpacing = 32.0;
    while (kSpacing * scale < 14.0) kSpacing *= 2.0;
    const QColor ink = light ? QColor(0, 0, 0) : QColor(255, 255, 255);
    if (m_style == QStringLiteral("grid")) {
        QPen gp(QColor(ink.red(), ink.green(), ink.blue(), light ? 22 : 16), 1.0);
        gp.setCosmetic(true);
        painter->setPen(gp);
        const qreal x0 = std::floor(rect.left() / kSpacing) * kSpacing;
        const qreal y0 = std::floor(rect.top() / kSpacing) * kSpacing;
        for (qreal x = x0; x <= rect.right(); x += kSpacing)
            painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        for (qreal y = y0; y <= rect.bottom(); y += kSpacing)
            painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        return;
    }
    if (m_style == QStringLiteral("lines")) {
        // Caderno: pauta azul
        QPen lp(light ? QColor(80, 120, 190, 70) : QColor(130, 165, 220, 46), 1.0);
        lp.setCosmetic(true);
        painter->setPen(lp);
        const qreal y0 = std::floor(rect.top() / kSpacing) * kSpacing;
        for (qreal y = y0; y <= rect.bottom(); y += kSpacing)
            painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        return;
    }
    // dots (e a lousa branca, com pontos bem fracos)
    const int alpha = (m_style == QStringLiteral("whiteboard")) ? 22 : (light ? 40 : 36);
    const qreal dotR = 1.25 / scale;
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(ink.red(), ink.green(), ink.blue(), alpha));
    const qreal x0 = std::floor(rect.left()  / kSpacing) * kSpacing;
    const qreal y0 = std::floor(rect.top()   / kSpacing) * kSpacing;
    for (qreal x = x0; x <= rect.right();  x += kSpacing)
        for (qreal y = y0; y <= rect.bottom(); y += kSpacing)
            painter->drawEllipse(QPointF(x, y), dotR, dotR);
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
    connect(item, &CardItem::hoverPreviewRequested, this, &LousaScene::cardHoverPreview);
    connect(item, &CardItem::hoverPreviewDismissed, this, &LousaScene::cardHoverDismissed);
    connect(item, &CardItem::selectionFlagChanged, this, &LousaScene::scheduleSelectionSignal);
    connect(item, &CardItem::openRequested, this, [this](const QString& id) {
        if (CardItem* c = findCard(id)) emit cardOpenRequested(c->cardData());
    });
    connect(item, &CardItem::gestureFinished, this, [this]() {
        refreshZoneCounts();
        emit gestureFinished();
    });
    // Texto livre que terminou vazio some sozinho. Na fila: o sinal sai de
    // dentro do próprio texto, que não pode ser apagado no meio do evento.
    connect(item, &CardItem::emptyTextFinished, this, [this](const QString& id) {
        removeCard(id);
        refreshZoneCounts();
    }, Qt::QueuedConnection);
    refreshZoneCounts();
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
    selectConnection(QString());
    // Clicar num card da área marcada com tudo dentro mantém o grupo (pra
    // arrastar tudo junto); qualquer outro clique desfaz.
    const bool keepGroup = !m_zoneWithContents.isEmpty() && item->isCardSelected()
                        && !(QGuiApplication::keyboardModifiers() & Qt::ShiftModifier);
    if (!keepGroup) clearZoneSelection();
    if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)
        toggleCardSelection(item);
    else if (!item->isCardSelected())
        selectOnlyCard(item);
}

void LousaScene::onCardDragStarted(const QString& id)
{
    m_groupOrigins.clear();
    m_groupLeader.clear();
    m_groupZoneId.clear();
    CardItem* leader = findCard(id);
    if (!leader || !leader->isCardSelected()) return;  // arrastando card avulso
    const QList<CardItem*> sel = selectedCardItems();
    const bool withZone = !m_zoneWithContents.isEmpty();
    if (sel.size() < 2 && !withZone) return;            // sem grupo
    m_groupLeader = id;
    for (CardItem* c : sel)
        m_groupOrigins.insert(c->cardData().id, c->pos());
    // Área marcada com tudo dentro: ela vai junto.
    if (withZone)
        for (ZoneItem* z : m_zones)
            if (z->zoneData().id == m_zoneWithContents) { m_groupZoneId = z->zoneData().id; m_groupZoneOrigin = z->pos(); }
}

void LousaScene::onCardDraggedBy(const QString& id, const QPointF& delta)
{
    if (id != m_groupLeader || m_groupOrigins.isEmpty()) return;
    if (!m_groupZoneId.isEmpty())
        for (ZoneItem* z : m_zones)
            if (z->zoneData().id == m_groupZoneId) z->setPos(m_groupZoneOrigin + delta);
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
            CardItem* dead = m_cards.takeAt(i);
            removeItem(dead);
            const bool wasSelected = dead->isCardSelected();
            dead->deleteLater();
            refreshZoneCounts();
            emit cardDataChanged();
            if (wasSelected) scheduleSelectionSignal();
            return;
        }
    }
}

void LousaScene::clearCards()
{
    cancelSnap();
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
    connect(item, &ConnectionItem::clicked, this, [this](const QString& id) {
        clearCardSelection();
        clearZoneSelection();
        selectConnection(id);
    });
    connect(item, &ConnectionItem::labelEditRequested, this, &LousaScene::connectionLabelEditRequested);
    connect(item, &ConnectionItem::menuRequested, this, &LousaScene::connectionMenuRequested);
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
    if (m_selectedConnId == id) {
        m_selectedConnId.clear();
        scheduleSelectionSignal();
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
    m_selectedConnId.clear();
}

void LousaScene::selectConnection(const QString& id)
{
    if (m_selectedConnId == id) return;
    m_selectedConnId = id;
    for (ConnectionItem* c : m_connections)
        c->setLineSelected(!id.isEmpty() && c->connData().id == id);
    scheduleSelectionSignal();
}

void LousaScene::clearAllSelection()
{
    clearCardSelection();
    clearZoneSelection();
    selectConnection(QString());
}

void LousaScene::scheduleSelectionSignal()
{
    // Junta várias mudanças (desmarcar 10 cards, marcar 1) num aviso só.
    if (m_selectionSignalPending) return;
    m_selectionSignalPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_selectionSignalPending = false;
        emit selectionChanged();
    });
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
    connect(item, &ZoneItem::contentsSelectRequested, this, &LousaScene::selectZoneWithContents);
    refreshZoneCounts();
    return item;
}

void LousaScene::resetZoneContents()
{
    if (m_zoneWithContents.isEmpty()) return;
    for (ZoneItem* z : m_zones)
        if (z->zoneData().id == m_zoneWithContents) z->setContentsSelected(false);
    m_zoneWithContents.clear();
}

void LousaScene::selectZoneWithContents(const QString& id)
{
    ZoneItem* zone = nullptr;
    for (ZoneItem* z : m_zones) if (z->zoneData().id == id) zone = z;
    if (!zone) return;
    onZoneClicked(id);
    const QRectF zr(zone->pos(), QSizeF(zone->zoneData().width, zone->zoneData().height));
    for (CardItem* c : m_cards) {
        const CanvasCard d = c->cardData();
        if (zr.contains(QPointF(d.x + d.width / 2.0, d.y + d.height / 2.0))) c->setCardSelected(true);
    }
    m_zoneWithContents = id;
    zone->setContentsSelected(true);
    scheduleSelectionSignal();
}

void LousaScene::clearZoneSelection()
{
    resetZoneContents();
    if (m_selectedZoneId.isEmpty()) return;
    m_selectedZoneId.clear();
    for (ZoneItem* z : m_zones) z->setSelected(false);
    scheduleSelectionSignal();
}

void LousaScene::onZoneClicked(const QString& id)
{
    // Já marcada com tudo dentro: um clique simples mantém o grupo (o
    // arrasto leva tudo); só um clique noutro lugar desfaz.
    if (!m_zoneWithContents.isEmpty() && id == m_zoneWithContents) return;
    resetZoneContents();
    clearCardSelection();
    selectConnection(QString());
    m_selectedZoneId = id;
    for (ZoneItem* z : m_zones)
        z->setSelected(z->zoneData().id == id);
    scheduleSelectionSignal();
}

void LousaScene::refreshZoneCounts()
{
    for (ZoneItem* z : m_zones) {
        const QRectF zr(z->pos(), QSizeF(z->zoneData().width, z->zoneData().height));
        int n = 0;
        for (const CardItem* c : m_cards) {
            if (c->isSticker()) continue;   // adesivo é enfeite, não conta
            const CanvasCard d = c->cardData();
            if (zr.contains(QPointF(d.x + d.width / 2.0, d.y + d.height / 2.0))) ++n;
        }
        z->setCardCount(n);
    }
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
        if (zr.contains(QPointF(d.x + d.width / 2.0, d.y + d.height / 2.0)))
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
            if (m_zones[i]->zoneData().id == m_selectedZoneId) {
                m_selectedZoneId.clear();
                scheduleSelectionSignal();
            }
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
    m_selectedZoneId.clear();
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
    // Verifica snap para cards arrastáveis (note/comment sem linkedToConn).
    // Só quando é a mão do autor arrastando: alinhar, desfazer ou colar
    // também mexem no card, e não podem grudar ele numa linha sozinhos.
    const CardItem* card = findCard(cardId);
    if (card && mouseGrabberItem() == card) {
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
        if (ci && !ci->isSticker() && ci->cardData().id != fromId) { target = ci; break; }
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
