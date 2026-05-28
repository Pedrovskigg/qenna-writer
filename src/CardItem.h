#pragma once

#include "LousaTypes.h"

#include <QColor>
#include <QGraphicsObject>
#include <QPixmap>
#include <QPointF>
#include <QSizeF>

class QGraphicsTextItem;

class CardItem : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit CardItem(const CanvasCard& data, QGraphicsItem* parent = nullptr);

    CanvasCard cardData() const;
    void syncFromData();
    void setLinkedHtml(const QString& html);
    void setSnapping(bool active, const QColor& color = QColor());
    // Chamado pelo LousaScene quando o card snapa em uma conexão:
    // adota a cor da linha e registra linkedToConn.
    void setSnapConnected(const QColor& color, const QString& connId);

    QRectF       boundingRect() const override;
    QPainterPath shape()        const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

    static constexpr qreal kHeaderH  = 28.0;
    static constexpr qreal kFoldSize = 20.0;
    static constexpr qreal kRadius   =  8.0;
    static constexpr qreal kShadow   =  6.0;
    static constexpr qreal kTailH    = 12.0;  // comment: altura do rabinho
    static constexpr qreal kMinW     = 120.0;
    static constexpr qreal kMinH     =  80.0;

signals:
    void dataChanged(const CanvasCard& data);
    void deleteRequested(const QString& id);
    void createDocRequested(const QString& id);
    void positionChanged(const QString& id);      // CardItem se moveu na cena
    void pinDragStarted(const QString& fromId, const QPointF& pinScenePos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* e)       override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* e)        override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e)     override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* e)        override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* e)       override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* e) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    bool   isOnPin(const QPointF& p) const;
    bool   isOnDeleteBtn(const QPointF& p) const;
    bool   isOnColorDot(const QPointF& p) const;
    bool   isOnDocBtn(const QPointF& p) const;
    bool   isOnResizeZone(const QPointF& p) const;
    void   showColorMenu(const QPoint& screenPos);
    void   openImagePicker();
    void   loadPixmapFromContent();
    void   toggleImageDesc(bool focus = true);
    void   updateTextItem();
    void   applyTextColor();
    QColor bodyColor() const;      // cor de fundo da área de texto
    QColor contrastColor() const;  // cor de texto sobre bodyColor()
    bool   isDark() const;

    CanvasCard         m_data;
    QGraphicsTextItem* m_textItem    = nullptr;
    QPixmap            m_pixmap;       // image + character photo
    bool               m_showDesc     = false;
    // Snap glow (waypoint snapping)
    bool               m_snapping     = false;
    QColor             m_snapColor;
    // Pin drag state
    bool               m_draggingPin  = false;

    bool    m_dragging        = false;
    bool    m_resizing        = false;
    QPointF m_pressScene;
    QPointF m_pressItemOrigin;
    QSizeF  m_pressSize;
    bool    m_hoverDelete     = false;
    bool    m_hoverColor      = false;
    bool    m_hoverDoc        = false;
    bool    m_hoverResize     = false;
};
