#pragma once
// Caneta da Lousa (2026-10-10, pedido do Pe; concept "Pincel na Lousa"):
// riscar direto no quadro. Cada traço guarda os pontos e a pressão em
// coordenadas do quadro e é redesenhado na escala do zoom, então fica nítido
// em qualquer aproximação. Não é card: não tem papel, alfinete nem ligação, e
// só pega o clique em cima do risco (um círculo em volta de um post-it não
// bloqueia o post-it).

#include <QColor>
#include <QGraphicsObject>
#include <QJsonObject>
#include <QPainterPath>
#include <QString>
#include <QVector>

struct InkPoint {
    float x = 0, y = 0;
    float p = 0.5f;   // pressão 0..1
};

struct CanvasInk {
    QString id;
    QString tool = QStringLiteral("pen");   // "pen" | "marker" | "highlight"
    QColor  color = QColor(QStringLiteral("#c0392b"));
    qreal   size = 4;                       // espessura de referência (px no quadro)
    QVector<InkPoint> pts;                  // coordenadas do quadro
};

namespace LousaInk {

QJsonObject toJson(const CanvasInk& ink);
CanvasInk   fromJson(const QJsonObject& o);

// Área que o traço ocupa, com a espessura.
QRectF bounds(const CanvasInk& ink);
// Faixa em volta do risco que pega o clique (e a borracha).
QPainterPath hitPath(const CanvasInk& ink, qreal extra = 4.0);
void paint(QPainter* p, const CanvasInk& ink);
void translate(CanvasInk& ink, const QPointF& delta);

// Forma perfeita: segurar parado no fim do traço. Fechado vira oval (ou
// círculo, se quase redondo); aberto vira reta. Devolve false se o traço é
// curto demais. `name` recebe "oval" | "circle" | "line".
bool snapShape(QVector<InkPoint>& pts, QString* name = nullptr);

// Estabilizador (o StreamLine do Procreate): 0..100, guardado no app, vale pra
// Caneta e pro Esboço. É um fio: a ponta do traço só anda quando a caneta se
// afasta mais que o comprimento dele, e vem atrás dela. Medido em pixels da
// tela, não em leituras da caneta (uma mesa manda centenas por segundo, e a
// primeira versão, que andava uma fração a cada leitura, ficava fraca nela).
int  stabilizerSetting();
void setStabilizerSetting(int v);

class Stabilizer {
public:
    void reset(const QPointF& at) { m_pos = at; m_has = true; }
    // `pxPerUnit`: quantos pixels de tela vale uma unidade do desenho (zoom).
    QPointF feed(const QPointF& raw, int strength, qreal pxPerUnit = 1.0);
    static qreal stringLengthPx(int strength);   // comprimento do fio, na tela
    QPointF tip() const { return m_pos; }
private:
    QPointF m_pos;
    bool m_has = false;
};

} // namespace LousaInk

class InkItem : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit InkItem(const CanvasInk& data, QGraphicsItem* parent = nullptr);

    const CanvasInk& inkData() const { return m_data; }
    void setInkData(const CanvasInk& data);   // traço em andamento (prévia)
    void setInkSelected(bool on);
    bool isInkSelected() const { return m_selected; }
    // Prévia do traço enquanto desenha: não pega clique.
    void setPreview(bool on);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;

    static constexpr qreal kZ = 900.0;   // por cima dos cards e das áreas

signals:
    void pressed(InkItem* item);
    void gestureStarted();
    void moved(InkItem* item);           // fim do arrasto (salvar)

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* e) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* e) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* e) override;

private:
    void rebuildGeometry();
    CanvasInk    m_data;
    QRectF       m_bounds;
    QPainterPath m_hit;
    bool         m_selected = false;
    bool         m_preview  = false;
    bool         m_hovered  = false;
    bool         m_dragging = false;
    bool         m_moved    = false;
    QPointF      m_pressScene;
    QPointF      m_lastScene;
};
