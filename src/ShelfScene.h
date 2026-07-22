#pragma once

#include <QGraphicsScene>
#include <QList>
#include <QPair>
#include <QPixmap>
#include <QPointF>
#include <QString>

#include "ShelfBookItem.h" // ShelfSpineStyle, ShelfBookItem

// Cena da Prateleira 3D: pinta o fundo/chão de madeira e posiciona os
// ShelfBookItem lado a lado, encostados na mesma linha de base (como livros
// de pé numa prateleira de verdade). Diferente do Mira 1 (que rolava na
// horizontal), aqui quando os livros não cabem mais na largura disponível
// (setWrapWidth), o excedente vira uma NOVA prateleira embaixo — cada uma com
// sua própria tábua de madeira.
//
// Também orquestra o arrastar-pra-reordenar: os livros avisam via sinal
// quando são arrastados, a cena decide o índice-alvo (comparando a posição do
// cursor com o centro dos vizinhos DA MESMA prateleira, já que o arrasto só
// se move na horizontal) e desliza os outros pra abrir o vão.
class ShelfScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit ShelfScene(QObject* parent = nullptr);

    void clearBooks();
    ShelfBookItem* addBook(const QString& path, const QString& name, const QString& author,
                           const QString& genres, const QString& synopsis,
                           const QPixmap& cover, const QColor& spineColor, qreal spineWidth,
                           const ShelfSpineStyle& style);

    // Largura disponível pra decidir quando quebrar pra próxima prateleira.
    // 0 = sem quebra (uma prateleira só, tão larga quanto precisar).
    void setWrapWidth(qreal w);

    // Recalcula posições de todos os livros (chamar depois de popular, depois
    // de mudar wrapWidth, ou depois de um resize da view).
    void relayout();

    // Material do fundo/tábua da prateleira — "none" | "wood-1".."wood-8" |
    // "steel-1".."steel-3" (mesmos arquivos do Mira 1, ver assets/textures).
    void setFloorTexture(const QString& id);
    QString floorTexture() const { return m_floorTextureId; }
    // (id, rótulo) de cada opção, "Nenhuma" incluída — pra popular o seletor.
    static QList<QPair<QString, QString>> floorTextureChoices();

signals:
    void openRequested(const QString& path);
    void editRequested(const QString& path);
    void coverCreateRequested(const QString& path);
    void removeRequested(const QString& path);
    void deleteRequested(const QString& path);
    // Emitido ao soltar um livro arrastado, com a nova ordem de paths — quem
    // ouve (MainMenuDialog → MainWindow) decide como persistir.
    void orderChanged(const QStringList& newOrder);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    // A tábua fica no foreground (desenhada DEPOIS dos livros) — precisa
    // cobrir o pé deles por cima, como mobília de verdade na frente.
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private slots:
    void onBookDragStarted(const QString& path);
    void onBookDraggedTo(const QString& path, qreal centerSceneX);
    void onBookDragFinished(const QString& path);
    // Dominó: cascateia um tombo nos vizinhos DA MESMA prateleira, com
    // ângulo decrescente e atraso escalonado pela distância.
    void onBookKnocked(const QString& path);
    // Hover-darken: quando um livro entra em hover, escurece TODOS os
    // outros — igual a Estante. Só quem realmente terminou o hover mais
    // recente limpa o estado (evita corrida entre leave/enter de vizinhos).
    void onBookHoverChanged(const QString& path, bool entered);

private:
    // Layout em fluxo: posição (x,y) e índice de prateleira de cada livro, na
    // ordem atual de m_books, respeitando m_wrapWidth.
    void computeFlowLayout(QList<QPointF>& outPos, QList<int>& outRow) const;
    void animateBookTo(ShelfBookItem* book, const QPointF& target);
    int  indexOfPath(const QString& path) const;
    int  rowCountFor(const QList<int>& rows) const;
    // Aplica a leve inclinação de repouso (por posição na prateleira) e o
    // "peek" de capa sempre visível no último livro de cada prateleira —
    // igual o Mira 1. Chamar depois de qualquer relayout/reordenação.
    void applyRestStates(const QList<int>& rows);

    QList<ShelfBookItem*> m_books;
    QString m_hoveredPath; // vazio = ninguém hoverado (nenhum livro escurecido)
    qreal m_wrapWidth = 0.0;
    QString m_floorTextureId = QStringLiteral("wood-1");
    QPixmap m_floorTexturePixmap; // cacheado — recarregado só quando o id muda

    // Estado do arrasto em andamento (vazio quando ninguém está sendo arrastado)
    QString m_dragPath;
    int     m_dragOriginalIndex = -1;
    int     m_dragTargetIndex   = -1;
    int     m_dragRowStart = -1, m_dragRowEnd = -1; // limites (inclusive) da prateleira do livro arrastado
    QList<QPointF> m_dragHomePos;   // posição de repouso de cada livro, capturada no início do arrasto
};
