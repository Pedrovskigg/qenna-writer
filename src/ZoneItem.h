#pragma once

#include "LousaTypes.h"

#include <QColor>
#include <QList>
#include <QGraphicsObject>
#include <QPointF>
#include <QSizeF>

class ZoneItem : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit ZoneItem(const CanvasZone& data, QGraphicsItem* parent = nullptr);

    const CanvasZone& zoneData() const { return m_data; }
    void setZoneData(const CanvasZone& d);
    void setSelected(bool on);
    bool isSelected() const { return m_selected; }
    void setCardCount(int n);          // quantos cards estão dentro (vai na etiqueta)
    // Cor do quadro atrás da etiqueta (a etiqueta "corta" a borda da área).
    static void setBoardColor(const QColor& c) { s_boardColor = c; }
    // Zoom da tela: com zoom longe a etiqueta cresce pra continuar legível.
    static void setViewZoom(const QList<ZoneItem*>& zones, qreal zoom);
    // Marcada com tudo o que tem dentro (Shift/Ctrl+clique): o arrasto leva junto.
    void setContentsSelected(bool on) { m_contentsSelected = on; }

    QRectF       boundingRect() const override;
    QPainterPath shape()         const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

signals:
    void dataChanged(const CanvasZone& data);
    void removeRequested(const QString& id);
    void gestureStarted();   // início de mover/redimensionar/recolorir/renomear (p/ undo)
    void zoneClicked(const QString& id);  // clique na zona (para seleção/export)
    void exportRequested(const QString& id);  // "Exportar área" no context menu
    void dragStartedWithContents(const QString& id, bool withContents);
    void contentsSelectRequested(const QString& id);   // Shift/Ctrl+clique
    void draggedBy(const QString& id, const QPointF& delta);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* e)    override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* e)     override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e)  override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* e)    override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* e)    override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* e) override;

private:
    // Hit-test helpers
    qreal  labelScale() const;         // quanto a etiqueta cresce agora (zoom longe)
    QRectF tagRect() const;            // etiqueta com o nome (canto de cima)
    QRectF controlsRect() const;       // cor + × (canto de cima, à direita)
    bool isOnDelete(const QPointF& p) const;
    bool isOnColorDot(const QPointF& p) const;
    int  handleAt(const QPointF& p) const;  // -1 = none, 0-7 = handle index

    void emitData();

    CanvasZone m_data;
    int        m_count = 0;
    bool       m_contentsSelected = false;
    static QColor s_boardColor;
    static qreal  s_labelScale;

    // Interaction state
    bool    m_hovered    = false;
    bool    m_selected   = false;
    bool    m_moveContents = false;   // arrastando com Ctrl+Shift (move conteúdo junto)
    bool    m_dragging   = false;
    bool    m_resizing   = false;
    int     m_resizeHandle = -1;
    QPointF m_pressScene;
    QPointF m_pressOrigin;   // zone (x,y) at press start
    QSizeF  m_pressSize;
};
