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
class QPainterPath;

class CardItem : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit CardItem(const CanvasCard& data, QGraphicsItem* parent = nullptr);

    CanvasCard cardData() const;
    void syncFromData();
    void setLinkedHtml(const QString& html);
    void setCharacterPhoto(const QString& dataUrl); // re-carrega m_pixmap e repinta
    // Linha de cima do documento/capítulo (nome da gaveta, "Capítulo 3") e a
    // de baixo (contagem de palavras). Vêm do projeto, não ficam no canvas.json.
    void setLinkedMeta(const QString& kicker, const QString& footer);
    // Papel do personagem, já traduzido ("Protagonista").
    void setRoleLabel(const QString& role);
    // Título vindo do projeto (documento/personagem/capítulo renomeado).
    void setLinkedTitle(const QString& title);
    void setSnapping(bool active, const QColor& color = QColor());
    void toggleImageDesc(bool show);   // personagem: vira a ficha (verso)
    bool isFlipped() const { return m_showDesc; }
    // Chamado pelo LousaScene quando o card snapa em uma conexão:
    // adota a cor da linha e registra linkedToConn.
    void setSnapConnected(const QColor& color, const QString& connId);
    // Popup de escolha de símbolo, no desenho do app. Retorna true se escolheu.
    static bool pickSymbol(QWidget* parent, QString& symbol, const QPoint& globalPos = QPoint());
    // Seleção: anel de destaque; text/symbol mostram as alças.
    void setCardSelected(bool on);
    bool isCardSelected() const { return m_selected; }
    // Rola o conteúdo do card. Retorna true se o card é rolável (consome o wheel,
    // impedindo o zoom enquanto o cursor está sobre ele — regra do Mira 1).
    bool wheelScroll(int angleDeltaY);
    // Texto livre: escrever direto no quadro.
    void beginTextEdit(bool cursorAtEnd = true);
    bool isEditingText() const { return m_editingText; }
    void onTextEditFinished();  // chamado pelo editor do texto ao perder o foco
    // Edição inline do título (note/comment) e da legenda (image).
    void beginTitleEdit();
    void onTitleEditFinished();
    void beginCaptionEdit();
    void onCaptionEditFinished();

    // Ações vindas da barra de ações (o card emite dataChanged).
    void setCardColor(const QColor& c);
    void setTextStyle(const QString& family, bool bold, bool italic);
    void setSymbol(const QString& symbol);
    void chooseImage();

    // Ponto de onde a linha sai: o pin (topo-centro), já com a inclinação.
    QPointF pinScenePos() const;
    // Inclinação de "papel preso no quadro" — vem do id, sem gravar nada.
    static void setTiltEnabled(bool on) { s_tilt = on; }
    static bool tiltEnabled() { return s_tilt; }
    void refreshTilt();
    // Fundo da lousa claro ou escuro: muda placeholder e cor padrão do texto livre.
    static void setBoardIsLight(bool light) { s_boardLight = light; }
    static void setBoardColor(const QColor& c) { s_boardColor = c; }
    void onBoardChanged();   // fundo mudou: texto/símbolo recalculam a tinta
    static bool boardIsLight() { return s_boardLight; }
    static QColor accent();

    QRectF       boundingRect() const override;
    QPainterPath shape()        const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

    static constexpr qreal kHeaderH   = 28.0;
    static constexpr qreal kFoldSize  = 18.0;
    static constexpr qreal kTailH     = 13.0;  // comment: altura do rabinho
    static constexpr qreal kMinW      = 120.0;
    static constexpr qreal kMinH      =  80.0;
    static constexpr qreal kCaptionH  =  30.0; // image: faixa da legenda
    static constexpr qreal kPolaroidFoot = 46.0; // character: nome + papel
    static qreal radius();

signals:
    void dataChanged(const CanvasCard& data);
    void deleteRequested(const QString& id);   // remoção permanente
    void stashRequested(const QString& id);    // guarda na gaveta da lousa
    void createDocRequested(const QString& id);
    void createTimelineEventRequested(const QString& id); // criar evento na Timeline (note/comment)
    void openRequested(const QString& id);       // doc/capítulo: abrir no editor
    void emptyTextFinished(const QString& id);   // texto livre terminou vazio → some
    void positionChanged(const QString& id);      // CardItem se moveu na cena
    void pinDragStarted(const QString& fromId, const QPointF& pinScenePos);
    void cardPressed();                            // qualquer clique no card (para seleção)
    void selectionFlagChanged();
    void hoverPreviewRequested(const CanvasCard& data, const QPoint& screenPos);
    void hoverPreviewDismissed();
    void gestureStarted();                         // início de um gesto mutável (drag/resize/rot)
    void gestureFinished();                        // fim do drag/resize/rot (barra de ações volta)
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
    bool   isNoteLike() const;       // note / comment
    bool   isPaperDoc() const;       // doc / chapter
    bool   isOnPin(const QPointF& p) const;
    bool   isOnResizeZone(const QPointF& p) const;
    bool   isOnOpenLink(const QPointF& p) const;
    void   loadCharacterPhoto();
    void   loadPixmapFromContent();
    void   updateTextItem();
    void   applyTextColor();
    void   rebuildRichDoc();   // (re)constrói m_richDoc (doc / chapter / verso do personagem)
    void   richContentRect(qreal& x, qreal& y, qreal& w, qreal& h) const;
    bool   scrollRegion(qreal& visH, qreal& contentH) const;
    void   paintScrollbar(QPainter* p, qreal top, qreal visH,
                          qreal contentH, const QColor& thumb) const;
    void   paintSelectionRing(QPainter* p, const QPainterPath& outline) const;
    void   paintPin(QPainter* p, const QColor& c) const;
    void   paintShadow(QPainter* p, const QRectF& r, qreal radius, qreal lift) const;
    void   paintCover(QPainter* p, const QRectF& r, qreal radius) const;  // foto (cover) ou silhueta
    QPainterPath outlinePath() const;  // contorno do card (sem rabinho/sombra)
    qreal  tiltDegrees() const;
    // text / symbol
    bool   isTextSymbol() const;
    int    effFontSize() const;          // fontSize com fallback por tipo
    QFont  contentFont() const;          // fonte aplicada ao texto/símbolo
    QRectF contentBounds() const;        // caixa do conteúdo (item coords, ancorada em 0,0)
    QRectF tsResizeRect() const;         // alça de tamanho da letra (canto)
    QRectF tsRotateRect() const;         // alça de rotação (topo-centro)
    QRectF tsWidthRect() const;          // alça da largura (meio da direita), só texto
    void   applyContentFont();
    void   refreshContentMetrics();      // recalcula geometria/origem de rotação
    QColor inkColor() const;       // cor do texto sobre o papel do card
    bool   isDark() const;

    CanvasCard         m_data;
    QString            m_kicker;        // doc/chapter: linha de cima
    QString            m_footer;        // doc/chapter: linha de baixo
    QString            m_role;          // character: papel traduzido
    QGraphicsRectItem* m_bodyClip = nullptr; // clipa o texto ao corpo (note/comment/image)
    QGraphicsTextItem* m_textItem = nullptr; // note/comment corpo, image legenda, text conteúdo
    QGraphicsTextItem* m_titleEditor = nullptr; // editor inline do título (note/comment)
    bool               m_editingTitle = false;
    bool               m_editingText  = false; // text: escrevendo / image: legenda
    QTextDocument*     m_richDoc  = nullptr; // conteúdo pintado (doc, chapter, verso do personagem)
    QPixmap            m_pixmap;       // image + character photo
    bool               m_showDesc     = false; // character: mostrando o verso
    bool               m_snapping     = false;
    QColor             m_snapColor;
    bool               m_draggingPin  = false;

    qreal   m_scrollOffset    = 0.0;
    bool    m_dragging        = false;
    bool    m_resizing        = false;
    QPointF m_pressScene;
    QPointF m_pressItemOrigin;
    QSizeF  m_pressSize;
    bool    m_hovered         = false;
    bool    m_hoverResize     = false;
    bool    m_hoverOpen       = false;
    bool    m_selected        = false;
    // text / symbol
    bool    m_rotating        = false;  // shift+drag ou alça de rotação
    bool    m_rotByHandle     = false;  // true = ângulo absoluto pela alça
    qreal   m_rotStartDeg     = 0.0;
    QPointF m_rotStartScene;
    bool    m_fontResizing    = false;  // alça do canto = tamanho da letra
    bool    m_widthResizing   = false;  // alça da direita = largura do texto
    int     m_pressFontSize   = 0;
    qreal   m_pressWrap       = 0.0;
    QRectF  m_paintedSceneRect;          // text/symbol: última área ocupada no quadro

    QColor readableInk() const;    // cor do texto/símbolo, legível no fundo atual
    static bool s_tilt;
    static bool s_boardLight;
    static QColor s_boardColor;
};
