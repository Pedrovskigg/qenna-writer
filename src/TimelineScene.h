#pragma once

#include "TimelineTypes.h"

#include <QColor>
#include <QGraphicsScene>
#include <QHash>
#include <QList>
#include <QPair>
#include <QPointF>
#include <QSet>

class TimelineEventItem;
class TimelineConnItem;
class QTimer;

class TimelineScene : public QGraphicsScene {
    Q_OBJECT
public:
    enum class ViewMode { Rail, Constellation, Spiral };
    enum class AxisMode { Narrative, Story };

    explicit TimelineScene(QObject* parent = nullptr);

    void setBackgroundColor(const QColor& color);

    void setViewMode(ViewMode m);
    ViewMode viewMode() const { return m_viewMode; }
    void setAxisMode(AxisMode m);
    AxisMode axisMode() const { return m_axisMode; }

    // ── Timelines ────────────────────────────────────────────────────────────
    void setTimelines(const QList<TimelineDef>& timelines);
    const QList<TimelineDef>& timelines() const { return m_timelines; }
    QColor  timelineColor(const QString& timelineId) const;
    QString timelineWeight(const QString& timelineId) const;
    QString timelineName(const QString& timelineId) const;
    bool    isCharacterTimeline(const QString& timelineId) const;
    // Regra de eixo: personagem só no eixo Narrativa; enredo autoral (kind
    // Main/Backstory — trilhas "História"/"Flashback" da sincronização de
    // capítulo/cena) aparece nos DOIS eixos sempre; o resto (Lore/Parallel/
    // Custom) segue a regra antiga (só no eixo História).
    bool    timelineVisibleInAxis(const QString& timelineId, bool narrativeAxis) const;

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
    TimelineConn      autoConnect(const QString& newEventId, const QString& timelineId);

    // ── Foco / Filtro por linha (graph depth) ──────────────────────────────────
    // Foca uma timeline: ela fica 100%, e os eventos/fios a até `depth` saltos
    // de distância pelas conexões entram; o resto esmaece (mantém contexto).
    void    setFocus(const QString& timelineId, int depth);
    void    clearFocus();
    QString focusTimelineId() const { return m_focusTimelineId; }
    int     focusDepth()      const { return m_focusDepth; }
    // Foco por clique: id vazio = limpa; expand=Shift (mesma linha → +1 salto,
    // outra linha → 1 salto; sem expand → só a linha = 0 saltos).
    void    focusLine(const QString& timelineId, bool expand);
    // Faixa/rótulo sob um ponto da cena (Modo Trilho). Vazio = nenhum.
    QString timelineIdAtRailPos(const QPointF& scenePos) const;

    // Filtro por personagem: esmaece eventos onde ele NÃO está presente
    // (combina com o foco por linha acima — os dois podem estar ativos juntos).
    // active=false = filtro desligado (ignora matchingEventIds).
    void setCharacterFilter(bool active, const QSet<QString>& matchingEventIds);

    // ── Layout do Modo Trilho ──────────────────────────────────────────────────
    void  relayout();                       // reposiciona tudo conforme o modo
    qreal railY(int order) const;           // y central da faixa
    qreal tickX(qreal tick) const;          // x de um tick
    int   nearestRailOrder(qreal y) const;  // faixa mais próxima de um y

signals:
    void eventDataChanged();
    void eventEditRequested(const QString& id);
    void canvasDoubleClicked(const QPointF& scenePos);
    void exportEventAsDoc(TimelineEvent data);
    void focusChanged(); // foco mudou (clique/limpeza) → toolbar revalida
    void editTimelineRequested(const QString& timelineId);   // clique-direito na faixa
    void deleteTimelineRequested(const QString& timelineId); // clique-direito na faixa

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)  override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private slots:
    void onEventPositionChanged(const QString& id);
    void onEventMovedByUser(const QString& id);
    void onEventOpenToggled(const QString& id, bool open);
    void onEventShiftClicked(const QString& eventId); // conectar a partir do aberto, ou expandir foco
    void stepSim();

private:
    void wireEvent(TimelineEventItem* item);
    void applyConnVisibility();
    // ── Foco ────────────────────────────────────────────────────────────────────
    void          applyFocusDimming();      // (re)aplica opacidade conforme o foco
    QSet<QString> eventsWithinFocus() const; // BFS: ids dentro do alcance de saltos
    // ── Constelação (force-directed) ───────────────────────────────────────────
    void startSim();          // (re)aquece e roda a simulação
    void stopSim();           // congela, persiste e salva
    void seedConstellation(); // posições iniciais espalhadas
    void buildSpiralTargets(); // alvo de cada evento: anel=importância, ângulo=tempo
    void seedSpiral();         // posições iniciais perto dos alvos (a sim assenta)
    void buildEdges();        // backbone sequencial por trilho
    QHash<QString, int> orderMap() const;

    QColor m_bgColor{QStringLiteral("#1c1c1c")};
    ViewMode m_viewMode = ViewMode::Rail;
    AxisMode m_axisMode = AxisMode::Narrative;

    // Foco / Filtro por linha
    QString       m_focusTimelineId;   // vazio = sem foco (tudo 100%)
    int           m_focusDepth = 1;    // alcance em saltos (1..3)
    QSet<QString> m_focusSet;          // cache dos ids dentro do alcance

    // Filtro por personagem (independente do foco por linha acima)
    bool          m_charFilterActive = false;
    QSet<QString> m_charFilterEventIds;

    QList<TimelineDef>   m_timelines;
    QList<TimelineEventItem*> m_events;
    QHash<QString, TimelineEventItem*> m_eventById;
    QList<TimelineConnItem*> m_connections;
    QHash<QString, TimelineConnItem*> m_connById;

    // Simulação da Constelação
    QTimer* m_simTimer   = nullptr;
    double  m_simAlpha   = 0.0;
    bool    m_simulating = false;   // true só durante stepSim (guarda re-entrância)
    QString m_pinnedId;             // nó preso pelo arraste do usuário
    QList<QPair<QString, QString>> m_seqEdges; // backbone sequencial (desenhado)
    QHash<QString, int> m_ageRank;  // posição (anel) de cada evento na sua linha
    QHash<QString, QPointF> m_spiralTarget; // alvo de cada evento (Modo Espiral)
};
