#pragma once

#include "LousaTypes.h"

#include <QColor>
#include <QGraphicsScene>
#include <QHash>
#include <QList>
#include <QPointF>
#include <QPixmap>
#include <QString>

class CardItem;
class ConnectionItem;
class ZoneItem;
class QGraphicsLineItem;
class QTimer;

class LousaScene : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit LousaScene(QObject* parent = nullptr);

    // Fundo: cor própria da lousa (inválida = a cor da mesa do tema) e o
    // desenho ("dots", "grid", "lines", "plain", "cork", "whiteboard").
    void setCanvasColor(const QColor& color);
    QColor canvasColor() const { return m_color; }          // inválida = do tema
    QColor effectiveCanvasColor() const;                    // a que aparece de fato
    QColor canvasColorForLabels() const { return effectiveCanvasColor(); }
    void setBoardStyle(const QString& style);
    QString boardStyle() const { return m_style; }
    void refreshBoardLook();           // tema mudou: recalcula a cor e repinta
    void setTiltEnabled(bool on);      // cards meio tortos
    void setSkipBackground(bool skip) { m_skipBackground = skip; }   // exportar sem fundo
    void setViewZoom(qreal zoom);      // etiqueta das áreas cresce com zoom longe
    // Tudo o que tem no quadro (cards, áreas, linhas), sem as sobras da cena.
    QRectF contentBounds() const;

    // ── Cards ────────────────────────────────────────────────────────────────
    CardItem* addCard(const CanvasCard& data);
    void      removeCard(const QString& id);
    void      clearCards();
    QList<CanvasCard> allCardData() const;
    const QList<CardItem*>& cardItems() const { return m_cards; }
    CardItem* findCard(const QString& id) const;
    void      selectOnlyCard(CardItem* sel);   // seleciona um, desseleciona os outros
    void      clearCardSelection();
    void      toggleCardSelection(CardItem* c);   // shift+click
    void      addCardToSelection(CardItem* c);    // brush select
    QList<CardItem*> selectedCardItems() const;

    // ── Conexões ─────────────────────────────────────────────────────────────
    ConnectionItem* addConnection(const CanvasConnection& data);
    void            removeConnection(const QString& id);
    void            clearConnections();
    QList<CanvasConnection> allConnectionData() const;
    ConnectionItem* findConnection(const QString& id) const;

    // ── Linha selecionada ────────────────────────────────────────────────────
    QString selectedConnectionId() const { return m_selectedConnId; }
    void      selectConnection(const QString& id);   // "" = nenhuma
    void      clearAllSelection();

    // ── Zonas ────────────────────────────────────────────────────────────────
    ZoneItem* addZone(const CanvasZone& data);
    void      removeZone(const QString& id);
    void      clearZones();
    QList<CanvasZone> allZoneData() const;
    void      clearZoneSelection();
    QString   selectedZoneId() const { return m_selectedZoneId; }
    void      refreshZoneCounts();
    void      selectZone(const QString& id) { onZoneClicked(id); }
    // Shift/Ctrl+clique na área: ela e tudo o que tem dentro, pra mover junto.
    void      selectZoneWithContents(const QString& id);
    const QList<ZoneItem*>& zoneItems() const { return m_zones; }

    // ── Pin drag (chamado por CardItem) ──────────────────────────────────────
    void startPinDrag(const QString& fromCardId, const QPointF& fromScene);
    void updatePinDrag(const QPointF& cursorScene);
    void endPinDrag(const QPointF& cursorScene);

    // ── Snap de waypoint (chamado quando card se move) ───────────────────────
    void checkSnapForCard(const QString& cardId, const QPointF& cardTopCenter);

signals:
    void cardDataChanged();
    void connectionDataChanged();
    void zoneDataChanged();
    void pendingConnection(const QString& fromId, const QString& toId);
    void undoSnapshotRequested();   // um gesto mutável começou — capturar estado p/ undo
    void cardStashRequested(const CanvasCard& card); // guardar card no stash
    void cardHoverPreview(const CanvasCard& data, const QPoint& screenPos);
    void cardHoverDismissed();
    void cardCreateDocRequested(const CanvasCard& card); // criar doc a partir do card
    void cardCreateTimelineEventRequested(const CanvasCard& card); // criar evento na Timeline
    void zoneExportRequested(const QString& id);     // exportar zona (context menu)
    void selectionChanged();                         // cards, linha ou área marcados mudaram
    void cardOpenRequested(const CanvasCard& card);  // doc/capítulo: abrir no editor
    void gestureFinished();                          // fim de arrasto/redimensão
    void connectionLabelEditRequested(const QString& id);
    void connectionMenuRequested(const QString& id, const QPoint& screenPos);
    void boardLookChanged();

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private slots:
    void onCardPositionChanged(const QString& cardId);
    void onSnapTimerFired();
    void onCardPressedSelect(CardItem* item);          // resolve seleção (shift = toggle)
    void onCardDragStarted(const QString& id);          // captura origens do grupo
    void onCardDraggedBy(const QString& id, const QPointF& delta); // move o grupo
    void onZoneClicked(const QString& id);              // seleciona zona p/ exportar
    void onZoneDragStartedWithContents(const QString& id, bool withContents);
    void onZoneDraggedBy(const QString& id, const QPointF& delta);

private:
    void cancelSnap();

    QColor  m_color;                 // inválida = cor da mesa do tema
    QString m_style = QStringLiteral("dots");
    QPixmap m_corkTile;
    QString m_selectedConnId;
    bool    m_selectionSignalPending = false;
    bool    m_skipBackground = false;
    QString m_zoneWithContents;      // área marcada com tudo dentro
    QString m_groupZoneId;           // ...que vai junto no arrasto do grupo
    QPointF m_groupZoneOrigin;
    void    resetZoneContents();
    void    scheduleSelectionSignal();
    QList<CardItem*>       m_cards;
    QList<ConnectionItem*> m_connections;
    QList<ZoneItem*>       m_zones;

    // Pin drag state
    QString            m_dragFromId;
    QGraphicsLineItem* m_ghostLine = nullptr;
    QPointF            m_ghostFrom;

    // Snap timer
    QTimer*  m_snapTimer   = nullptr;
    QString  m_snapCardId;
    QString  m_snapConnId;

    // Arrasto de grupo (seleção múltipla)
    QString               m_groupLeader;
    QHash<QString, QPointF> m_groupOrigins;

    // Seleção e arrasto-com-conteúdo de zona
    QString               m_selectedZoneId;
    QString               m_zoneDragLeader;
    QHash<QString, QPointF> m_zoneContentOrigins;
};
