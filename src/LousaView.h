#pragma once

#include "LousaInk.h"

#include <QElapsedTimer>
#include <QGraphicsView>
#include <QImage>
#include <QPoint>
#include <QPointF>

class LousaScene;
class QGraphicsRectItem;
class QTimer;

class LousaView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit LousaView(LousaScene* scene, QWidget* parent = nullptr);

    qreal   zoomFactor() const { return m_zoom; }
    QPointF scrollPos()  const;

    void applyZoomAndPan(qreal zoom, qreal panX, qreal panY);
    void fitSceneRect(const QRectF& r);   // ajusta zoom+pan para enquadrar um retângulo
    void zoomBy(qreal factor);            // botões −/+: zoom em torno do centro da tela
    void centerOnAnimated(const QPointF& scenePos);  // busca: leva a tela até o card

    // Ligar: clica num card e depois noutro pra criar a linha.
    void setConnectMode(bool on);
    bool isConnectMode() const { return m_connectMode; }

    // Plan mode: cursor crosshair, arrastar no canvas cria uma zona.
    void setPlanMode(bool on);
    bool isPlanMode() const { return m_planMode; }

    // Brush select (Shift+S): passar o mouse seleciona os cards tocados.
    void setBrushMode(bool on);
    bool isBrushMode() const { return m_brushMode; }

    // Caneta: arrastar risca no quadro. tool = "pen" | "marker" | "highlight" | "eraser".
    // A ponta de trás da caneta da mesa apaga em qualquer ponta.
    void setInkMode(bool on);
    bool isInkMode() const { return m_inkMode; }
    void setInkTool(const QString& tool, const QColor& color, qreal size);

signals:
    void zoomChanged(qreal zoom);
    void zoneDrawn(const QRectF& sceneRect); // emitido ao soltar o mouse no plan mode
    void viewportMoved();                    // rolou ou deu zoom (barra de ações, minimapa)
    void backgroundDoubleClicked(const QPointF& scenePos);
    void itemDropped(const QString& payload, const QPointF& scenePos);  // veio da bandeja
    void imageDropped(const QImage& image, const QPointF& scenePos);   // arrastada do Windows
    void connectPicked(const QString& fromId, const QString& toId);
    void connectModeChanged(bool on);
    void inkStrokeFinished(const CanvasInk& ink);   // traço novo pronto (sem id)
    void inkEraseStarted();                         // começou a apagar (um passo de desfazer)
    void inkErased(const QString& id);              // a borracha tocou este traço
    void inkEraseFinished();

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    bool viewportEvent(QEvent* event) override;   // caneta da mesa (pressão)

private:
    qreal  m_zoom    = 1.0;
    bool   m_panning = false;
    QPoint m_panLast;

    // Plan mode
    bool               m_planMode   = false;
    bool               m_drawing    = false;
    QPointF            m_drawStart;
    QGraphicsRectItem* m_planGhost  = nullptr;

    // Brush select
    bool               m_brushMode  = false;

    // Ligar
    bool               m_connectMode = false;
    QString            m_connectFrom;

    // Caneta
    QPointF toScene(const QPointF& viewPos) const;
    void    inkBegin(const QPointF& viewPos, qreal pressure, bool eraser, bool straight);
    void    inkMove(const QPointF& viewPos, qreal pressure);
    void    inkEnd();
    void    inkSnapNow();
    qreal   mousePressure(const QPointF& viewPos);
    bool               m_inkMode    = false;
    QString            m_inkTool    = QStringLiteral("pen");
    QColor             m_inkColor   = QColor(QStringLiteral("#c0392b"));
    qreal              m_inkSize    = 4.0;
    bool               m_inkDrawing = false;
    bool               m_inkErasing = false;
    bool               m_inkStraight = false;
    bool               m_inkSnapped = false;
    bool               m_tabletDown = false;
    CanvasInk          m_ink;
    class InkItem*     m_inkPreview = nullptr;
    LousaInk::Stabilizer m_inkStab;
    int                m_inkStabStrength = 0;
    QPointF            m_inkRaw;          // onde a caneta está (sem estabilizador)
    QPointF            m_inkLastView;
    qreal              m_inkMouseP  = 0.55;
    QTimer*            m_inkHold    = nullptr;
    class QGraphicsLineItem* m_inkString = nullptr;   // o fio do estabilizador
    void    inkShowString(bool on);
};
