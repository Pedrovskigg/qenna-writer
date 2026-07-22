#pragma once

#include <QGraphicsView>

class ShelfScene;

// View fina da Prateleira 3D. Sem scroll próprio — quando os livros não cabem
// na largura disponível, a ShelfScene quebra pra uma nova prateleira embaixo
// (refreshLayout() avisa a cena da largura atual); quem rola a página toda
// (múltiplas prateleiras incluídas) é o QScrollArea vertical de fora, igual
// Estante/Lista. Sem zoom/pan próprio — o mouse pertence aos ShelfBookItem
// (hover/clique/arrastar).
class ShelfView : public QGraphicsView {
    Q_OBJECT
public:
    explicit ShelfView(ShelfScene* scene, QWidget* parent = nullptr);

    // Recalcula a quebra de linha pra largura atual do viewport e ajusta a
    // altura do widget pro conteúdo resultante (1+ prateleiras). Chamar depois
    // de popular a cena, e automaticamente a cada resize.
    void refreshLayout();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    ShelfScene* m_scene = nullptr;
};
