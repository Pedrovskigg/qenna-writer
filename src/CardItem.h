#pragma once

#include "LousaTypes.h"

#include <QColor>
#include <QGraphicsObject>
#include <QPixmap>
#include <QPointF>
#include <QSizeF>

class QGraphicsTextItem;
class QGraphicsRectItem;
class QTextDocument;
class QPainter;

class CardItem : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit CardItem(const CanvasCard& data, QGraphicsItem* parent = nullptr);

    CanvasCard cardData() const;
    void syncFromData();
    void setLinkedHtml(const QString& html);
    void setCharacterPhoto(const QString& dataUrl); // re-carrega m_pixmap e repinta
    void setSnapping(bool active, const QColor& color = QColor());
    void toggleImageDesc(bool show);   // public: chamado pelo BodyTextItem
    // Chamado pelo LousaScene quando o card snapa em uma conexão:
    // adota a cor da linha e registra linkedToConn.
    void setSnapConnected(const QColor& color, const QString& connId);
    // Popup de escolha de símbolo + cor (reutilizável). Retorna true se confirmado.
    // `symbol`/`color` entram com os valores atuais e saem com os escolhidos.
    static bool pickSymbol(QWidget* parent, QString& symbol, QColor& color);
    // Popup de texto + cor + fonte (texto livre). Retorna true se confirmado.
    static bool pickText(QWidget* parent, QString& text, QColor& color, QString& fontFamily);
    // Seleção (text/symbol): controles flutuantes aparecem só quando selecionado.
    void setCardSelected(bool on);
    bool isCardSelected() const { return m_selected; }
    // Rola o conteúdo do card. Retorna true se o card é rolável (consome o wheel,
    // impedindo o zoom enquanto o cursor está sobre ele — regra do Mira 1).
    bool wheelScroll(int angleDeltaY);
    // Chamado pelo BodyTextItem do card de texto ao perder o foco (sai do modo edição).
    void onTextEditFinished();
    // Edição inline do título (note/comment): abre a textbox direto no header.
    void beginTitleEdit();
    void onTitleEditFinished();   // chamado pelo editor de título ao perder o foco

    QRectF       boundingRect() const override;
    QPainterPath shape()        const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

    static constexpr qreal kHeaderH   = 28.0;
    static constexpr qreal kDocHeaderH = 26.0;
    static constexpr qreal kFoldSize  = 20.0;
    static constexpr qreal kRadius   =  8.0;
    static constexpr qreal kShadow   =  6.0;
    static constexpr qreal kTailH    = 12.0;  // comment: altura do rabinho
    static constexpr qreal kMinW     = 120.0;
    static constexpr qreal kMinH     =  80.0;

signals:
    void dataChanged(const CanvasCard& data);
    void deleteRequested(const QString& id);   // remoção permanente (Shift+×)
    void stashRequested(const QString& id);    // guarda no stash (× simples)
    void createDocRequested(const QString& id);
    void createTimelineEventRequested(const QString& id); // criar evento na Timeline (note/comment)
    void positionChanged(const QString& id);      // CardItem se moveu na cena
    void pinDragStarted(const QString& fromId, const QPointF& pinScenePos);
    void cardPressed();                            // qualquer clique no card (para seleção)
    void hoverPreviewRequested(const CanvasCard& data, const QPoint& screenPos);
    void hoverPreviewDismissed();
    void gestureStarted();                         // início de um gesto mutável (drag/resize/rot)
    void dragStarted(const QString& id);           // começou a arrastar este card
    void draggedBy(const QString& id, const QPointF& deltaScene); // delta acumulado do arrasto

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* e)       override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* e)        override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e)     override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* e)       override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* e)        override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* e)       override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* e) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    QString cardTooltipText() const;
    bool   isOnPin(const QPointF& p) const;
    bool   isOnDeleteBtn(const QPointF& p) const;
    bool   isOnColorDot(const QPointF& p) const;
    bool   isOnDocBtn(const QPointF& p) const;
    bool   isOnEventBtn(const QPointF& p) const;
    bool   isOnResizeZone(const QPointF& p) const;
    void   showColorMenu(const QPoint& screenPos);
    void   loadCharacterPhoto();
    void   openImagePicker();
    void   loadPixmapFromContent();
    void   updateTextItem();
    void   applyTextColor();
    void   rebuildRichDoc();   // (re)constrói m_richDoc a partir do content (doc/character)
    void   richContentRect(qreal& x, qreal& y, qreal& w, qreal& h) const; // região do conteúdo rico
    bool   scrollRegion(qreal& visH, qreal& contentH) const; // métricas de scroll do tipo atual
    void   paintScrollbar(QPainter* p, qreal top, qreal visH,
                          qreal contentH, const QColor& thumb) const;
    void   paintSelectionRing(QPainter* p) const; // contorno azul quando selecionado (não-text/symbol)
    // text / symbol
    bool   isTextSymbol() const;
    int    effFontSize() const;          // fontSize com fallback por tipo
    QFont  contentFont() const;          // fonte aplicada ao texto/símbolo
    QRectF contentBounds() const;        // caixa do conteúdo (item coords, ancorada em 0,0)
    void   controlRects(QRectF& del, QRectF& color, QRectF& sym) const;
    QRectF tsResizeRect() const;         // alça de resize de fonte
    QRectF tsRotateRect() const;         // alça de rotação (topo-centro)
    void   applyContentFont();           // aplica fonte/cor ao m_textItem do card de texto
    void   refreshContentMetrics();      // recalcula geometria/origem de rotação
    void   openSymbolPicker();
    QColor bodyColor() const;      // cor de fundo da área de texto
    QColor contrastColor() const;  // cor de texto sobre bodyColor()
    bool   isDark() const;

    CanvasCard         m_data;
    QGraphicsRectItem* m_bodyClip = nullptr; // clipa o BodyTextItem ao corpo (note/comment/image)
    QGraphicsTextItem* m_textItem = nullptr; // note/comment textarea + image overlay
    QGraphicsTextItem* m_titleEditor = nullptr; // editor inline do título (note/comment)
    bool               m_editingTitle = false;
    QTextDocument*     m_richDoc  = nullptr; // conteúdo rico pintado (doc + character)
    QPixmap            m_pixmap;       // image + character photo
    bool               m_showDesc     = false;
    // Snap glow (waypoint snapping)
    bool               m_snapping     = false;
    QColor             m_snapColor;
    // Pin drag state
    bool               m_draggingPin  = false;

    qreal   m_scrollOffset    = 0.0;    // scroll da textarea (note/comment)
    bool    m_dragging        = false;
    bool    m_resizing        = false;
    QPointF m_pressScene;
    QPointF m_pressItemOrigin;
    QSizeF  m_pressSize;
    bool    m_hoverDelete     = false;
    bool    m_hoverColor      = false;
    bool    m_hoverDoc        = false;
    bool    m_hoverEvent      = false;
    bool    m_hoverResize     = false;
    // text / symbol
    bool    m_hovered         = false;
    bool    m_selected        = false;  // mostra controles flutuantes só quando selecionado
    bool    m_rotating        = false;  // shift+drag ou alça de rotação
    bool    m_rotByHandle     = false;  // true = ângulo absoluto pela alça
    qreal   m_rotStartDeg     = 0.0;
    QPointF m_rotStartScene;
    bool    m_fontResizing    = false;  // alça de resize = mudar fontSize
    int     m_pressFontSize   = 0;
};
