#pragma once

#include "TimelineTypes.h"

#include <QColor>
#include <QGraphicsScene>
#include <QHash>
#include <QList>
#include <QPair>

class TimelineEventItem;
class TimelineConnItem;
class QTimer;

class TimelineScene : public QGraphicsScene {
    Q_OBJECT
public:
    enum class ViewMode { Rail, Constellation };
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
    TimelineConn      autoConnect(const QString& newEventId, const QString& timelineId);

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

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)  override;

private slots:
    void onEventPositionChanged(const QString& id);
    void onEventMovedByUser(const QString& id);
    void onEventOpenToggled(const QString& id, bool open);
    void stepSim();

private:
    void wireEvent(TimelineEventItem* item);
    void applyConnVisibility();
    // ── Constelação (force-directed) ───────────────────────────────────────────
    void startSim();          // (re)aquece e roda a simulação
    void stopSim();           // congela, persiste e salva
    void seedConstellation(); // posições iniciais espalhadas
    void buildEdges();        // backbone sequencial por trilho
    QHash<QString, int> orderMap() const;

    QColor m_bgColor{QStringLiteral("#1c1c1c")};
    ViewMode m_viewMode = ViewMode::Rail;
    AxisMode m_axisMode = AxisMode::Narrative;

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
};
