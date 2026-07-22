#pragma once

#include <QColor>
#include <QGraphicsObject>
#include <QPainterPath>
#include <QPixmap>
#include <QPointF>
#include <QString>

class QVariantAnimation;

// Customização da lombada — mesmos campos do menu de contexto do Mira 1,
// editáveis agora pela aba "Lombada" do ProjectEditDialog. Strings vazias /
// cores inválidas = usar o padrão (cor por índice, Alegreya, creme, vertical,
// centro, automático).
struct ShelfSpineStyle {
    QPixmap coverBg;            // fundo sem texto (Cover Creator); nulo cai pro m_cover
    QString imageTexture;       // "" | "cover"
    int     bgPosX = 0;         // 0-100, usado só quando imageTexture=="cover"
    QString finish;             // "" | couro | linho | veludo | fosco | metalico
    QString fontFamily;
    QColor  fontColor;
    int     fontSize = 0;       // 0 = automático (proporcional à largura)
    QString textOrientation;    // "" (vertical) | horizontal
    QString textPosition;       // "" (center) | top | bottom
};

// Um livro da Prateleira 3D — uma caixa de verdade (lombada + capa + capa
// traseira + bloco de páginas), não um recorte 2D. As faces vivem em planos
// 3D distintos (frente/trás em torno de um pivô vertical central; topo/base
// em torno de um pivô horizontal central); o corpo rígido gira em dois eixos
// — m_yaw (esquerda/direita, revela a capa de um lado ou a "contracapa" do
// outro) e m_pitch (cima/baixo, revela o bloco de páginas) — seguindo o
// mouse ao vivo durante o arrasto, exatamente como pegar um livro de
// verdade. Um "tilt" cosmético (rotação 2D nativa do item) dá aquele leve
// entortar de lombada encostada nas vizinhas. Cada face é um quadrilátero
// projetado (perspectiva simples + QTransform::quadToQuad).
class ShelfBookItem : public QGraphicsObject {
    Q_OBJECT
public:
    ShelfBookItem(const QString& path, const QString& name, const QString& author,
                  const QString& genres, const QString& synopsis,
                  const QPixmap& cover, const QColor& spineColor,
                  qreal spineWidth, const ShelfSpineStyle& style,
                  QGraphicsItem* parent = nullptr);
    ~ShelfBookItem() override;

    QRectF       boundingRect() const override;
    void         paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

    QPainterPath shape() const override;

    QString path() const { return m_path; }
    qreal   spineWidth() const { return m_spineW; }

    // Chamado pela ShelfScene nos vizinhos de um livro derrubado (dominó):
    // tomba por `magnitude` graus e volta, com um atraso pra dar o efeito
    // cascata.
    void triggerKnock(qreal magnitude, int delayMs);

    // Leve inclinação de repouso (como um livro encostado nos vizinhos,
    // igual ao Mira 1) — a ShelfScene chama de novo sempre que a ordem muda,
    // já que a inclinação segue a POSIÇÃO na prateleira, não o livro em si.
    void setBaseTilt(qreal deg);
    // O último livro da prateleira sempre mostra um pouco da capa, sem
    // precisar de hover (igual o "--first" do Mira 1).
    void setFeaturedPeek(bool on);

    // Z-value de REPOUSO — a ShelfScene chama de novo a cada relayout/
    // reordenação, passando o índice atual do livro na fileira (esquerda
    // pra direita). Sem isso, dois livros vizinhos que ficam sobrepostos
    // (o de peek permanente, ou logo depois de um hover fechando) empatam
    // em zValue e o Qt desempata pela ORDEM DE CRIAÇÃO na cena — que já não
    // bate mais com a posição visual atual depois de um arrasto/reorder,
    // fazendo um livro à esquerda ficar grudado na frente do vizinho à
    // direita. Hover/drag sobem bem acima disso temporariamente.
    void setRestZValue(qreal z);

    // Posição (em coordenadas de CENA) do ponto de fuga compartilhado por
    // toda a prateleira — equivalente ao `perspective`/`perspective-origin`
    // do Mira 1, que fica no CONTAINER (a prateleira inteira), não em cada
    // livro. Sem isso, cada livro projeta como se fosse o centro do próprio
    // universo, e um livro na ponta da prateleira (como o de peek permanente)
    // não ganha aquele ângulo dramático de quem está "fora do centro" — só
    // livros com yaw/pitch diferente de zero são afetados.
    void setSharedOriginX(qreal sceneX);

    // Escurece este livro um pouco (usado pela ShelfScene em TODOS os
    // outros quando um é hoverado — igual o hover-darken da Estante).
    void setDimmed(bool on);

    static constexpr qreal kBookH   = 420.0; // altura da lombada (todas iguais, como no Mira 1)
    static constexpr qreal kDepth   = 260.0; // profundidade do livro = largura da capa revelada

signals:
    void openRequested(const QString& path);
    // Arrastar pra reordenar: começou (snapshot), posição atual do centro em
    // cena (a ShelfScene decide o índice-alvo e desliza os vizinhos), e
    // soltou (a ShelfScene comita a nova ordem e anima o snap final).
    void dragStarted(const QString& path);
    void draggedTo(const QString& path, qreal centerSceneX);
    void dragFinished(const QString& path);
    // Hover começou/terminou (ignora o peek permanente) — a ShelfScene usa
    // pra escurecer todos os outros livros enquanto este está em destaque.
    void hoverChanged(const QString& path, bool entered);
    // Arremesso rápido pra baixo ao soltar — pede pra ShelfScene cascatear
    // um tombo nos vizinhos.
    void knocked(const QString& path);
    // Menu de contexto (clique direito)
    void editRequested(const QString& path);
    void coverCreateRequested(const QString& path);
    void removeRequested(const QString& path);
    void deleteRequested(const QString& path);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* e)   override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* e)     override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* e)   override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* e)   override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* e) override;

private:
    void buildSpineFace();      // pré-renderiza o pixmap da lombada (cor + texto)
    void buildCoverFace();      // pré-recorta o pixmap da capa pro tamanho da face
    void buildBackCoverFace();  // contracapa: sombra da cor da lombada + gênero/sinopse
    void buildAuxFaces();       // páginas (topo/base) e fore-edge — cacheadas, não geradas por frame
    void animateYawTo(qreal target, int durationMs = 220);
    void animatePitchTo(qreal target, int durationMs = 220);
    void springBack(); // volta yaw/pitch pro repouso (ou peek de hover) com mola

    QString m_path;
    QString m_name;
    QString m_author;
    QString m_genres;
    QString m_synopsis;
    QPixmap m_cover;         // capa original (decodificada, pode ser nula)
    QColor  m_spineColor;
    qreal   m_spineW;
    ShelfSpineStyle m_style;

    QPixmap m_spineFace;      // pré-renderizado: cor + título/autor
    QPixmap m_coverFace;      // pré-recortado: capa em depth x bookH
    QPixmap m_backCoverFace;  // pré-renderizado: contracapa em depth x bookH
    QPixmap m_pagesTopFace, m_pagesBottomFace, m_foreedgeFace; // cacheadas (perf)
    // Versões saturadas/mais claras de lombada+capa — trocam de lugar das
    // normais durante o hover (em vez de um véu branco por cima), pra dar
    // aquele "a capa fica mais vibrante" sem recalcular nada por frame.
    QPixmap m_spineFaceVibrant, m_coverFaceVibrant;

    qreal m_yaw   = 0.0;  // esquerda/direita — 0 fechado, + revela capa, - revela contracapa
    qreal m_pitch = 0.0;  // cima/baixo — 0 reto, revela páginas ao tombar
    qreal m_baseTilt = 0.0;      // inclinação de repouso (posição na prateleira, não do livro)
    qreal m_sharedOriginX = 0.0; // ponto de fuga compartilhado, em coordenadas de cena
    qreal m_restZValue = 0.0;    // baseline de empilhamento — índice atual na fileira
    bool  m_dimmed = false;      // outro livro está hoverado — escurece este
    bool  m_featuredPeek = false; // último da prateleira: sempre mostra um pouco da capa
    bool  m_hovered = false;
    QVariantAnimation* m_yawAnim = nullptr;
    QVariantAnimation* m_pitchAnim = nullptr;
    bool m_pressed = false;

    // Arrastar (reordenar + girar ao vivo)
    QPointF m_pressScenePos;
    QPointF m_pressItemOrigin;
    QPointF m_lastMoveScenePos;
    QPointF m_lastMoveDelta;   // pra estimar velocidade no soltar (detectar arremesso)
    bool m_dragging  = false;
    bool m_hasMoved  = false;
    static constexpr qreal kDragThreshold = 5.0;

    static constexpr qreal kHoverYaw  = 22.0;  // peek suave só de passar o mouse, sem arrastar
    // Tombo pra trás que acompanha QUALQUER abertura de capa (hover ou peek
    // permanente) — sem isso, a dobradiça lombada-capa projeta como uma
    // linha vertical perfeita (yaw sozinho não desloca Z ao longo da
    // dobradiça, só entre esquerda/direita). É o pitch que faz a ponta de
    // baixo da dobradiça (a parte que fica encostada no fundo da
    // prateleira) descer mais que a de cima, dando aquele "deitar pro
    // fundo" que faz o livro parecer apoiado de verdade.
    static constexpr qreal kRestPitch = 10.0;
    static constexpr qreal kYawMax    = 48.0;
    static constexpr qreal kPitchMax  = 55.0;
    static constexpr qreal kYawFactor   = 0.16;
    static constexpr qreal kPitchFactor = 0.20;
    static constexpr qreal kTiltFactor  = 0.035; // graus por px, rotação 2D nativa (cosmética)
    static constexpr qreal kKnockPitch  = 78.0;
    static constexpr qreal kFocal   = 1800.0;
    static constexpr qreal kCamDist = 1800.0;
};
