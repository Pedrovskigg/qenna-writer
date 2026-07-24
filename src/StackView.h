#pragma once

#include <QPixmap>
#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QCheckBox;
class QScrollArea;

// Uma entrada da Pilha — já vem pronta (capas pré-renderizadas na estética
// vitrine, só em dois tamanhos) do MainMenuDialog::populateActiveView(), que
// é quem conhece RecentInfo/renderVitrineCover. O StackView não sabe nada de
// projetos, JSON ou disco — só recebe isso e exibe/anima.
struct StackEntry {
    QString path;
    QString name;
    QString author;
    QString genres;
    QString synopsis;
    int     totalWords = -1;
    int     manuscriptCount = 0;
    int     chapterCount    = 0;
    int     documentCount   = 0;
    QPixmap heroCover;   // tamanho grande, pro livro em destaque
    QPixmap sideCover;   // tamanho normal (igual card da Estante), pra pilha lateral
    QPixmap fullCover;   // resolução original decodificada (até 1200px), sem o
                         // encolhimento pro tamanho de exibição — fonte do hero
                         // banner borrado, que senão ficava pixelado partindo do
                         // heroCover já reduzido
    bool    autoOpen = false;
};

// Modo de visualização "Pilha": um livro herói em destaque (capa grande +
// título/autor/sinopse) e os demais recentes numa pilha lateral com "peek"
// (cada capa revela uma fatia da anterior, como um baralho de verdade).
//
// Rotação, não índice: rolar o mouse gira o baralho — o herói atual vai pra
// ÚLTIMA posição e o próximo assume seu lugar (e vice-versa na direção
// contrária). Isso já dá o efeito de loop de graça, sem tratar ponta de
// lista como caso especial. Clicar num livro da pilha lateral gira até ele
// virar herói; só clicar no HERÓI abre o projeto de fato.
//
// Crossfade feito à mão (blend de pixmap por QVariantAnimation), nunca via
// QGraphicsOpacityEffect — este widget mora dentro do mesmo QScrollArea
// externo que o BookCard da Estante, e o comentário em BookCard já registra
// que QGraphicsOpacityEffect corta o repaint ali dentro.
class StackView : public QWidget {
    Q_OBJECT
public:
    explicit StackView(QWidget* parent = nullptr);

    // entries[0] é sempre tratado como o herói — cada chamada reseta a
    // rotação pro início da lista recebida (decisão consciente: reabrir o
    // menu/trocar de modo sempre volta pro projeto mais recente, sem
    // guardar "onde o usuário parou").
    void setEntries(const QVector<StackEntry>& entries);

signals:
    void openRequested(const QString& path);
    void autoOpenChanged(const QString& path, bool enabled);
    void editRequested(const QString& path);
    void coverCreateRequested(const QString& path);
    void removeRequested(const QString& path);
    void deleteRequested(const QString& path);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void rotateBy(int k);
    void rebuildHeroPanel(bool animate, int direction);
    void rebuildSidePile(bool animate);
    void showContextMenuFor(const QString& path, const QPoint& globalPos);
    void crossfadeLabel(QLabel* label, const QPixmap& from, const QPixmap& to,
                         int durationMs, int slideDirection = 0);

    QVector<StackEntry> m_entries; // [0] = herói atual

    // Hero banner: versão ampliada/borrada/escurecida da capa em foco,
    // atrás de toda a composição (preenche o vão vazio de cima, tipo
    // Netflix/Spotify). Filho direto de "this", fora de qualquer QLayout —
    // geometria calculada à mão em resizeEvent().
    QLabel* m_backdropLbl = nullptr;
    QPixmap m_backdropLastPixmap;

    // --- Coluna do herói ---
    QWidget* m_heroStage = nullptr;    // palco (maior que a capa, dá folga pro slide)
    QLabel*  m_heroCoverLbl = nullptr;
    QPixmap  m_heroLastPixmap;         // "from" do próximo crossfade

    // Legenda discreta com as estatísticas do projeto em foco, flutuando
    // acima da capa (sem caixa/borda — só tipografia, pra não competir
    // visualmente com a capa).
    QLabel* m_heroStatsLbl = nullptr;

    QWidget*   m_textCol = nullptr;
    QLabel*    m_heroTitleLbl = nullptr;
    QLabel*    m_heroAuthorLbl = nullptr;
    QLabel*    m_heroBreadcrumbLbl = nullptr;
    QScrollArea* m_synopsisScroll = nullptr;
    QLabel*    m_synopsisLbl = nullptr;
    QCheckBox* m_heroAutoOpenChk = nullptr;

    // --- Pilha lateral (peek horizontal, tipo leque de cartas) ---
    QWidget* m_sidePile = nullptr;         // container de tamanho fixo, sem QLayout
    QVector<QLabel*> m_sideSlots;          // pool fixo de kVisibleSideSlots
    QVector<QPixmap> m_sideLastPixmaps;    // "from" do crossfade de cada slot (paralelo a m_sideSlots)

    int  m_wheelAccum = 0;                 // acumula angleDelta() até completar um "detent"
    bool m_animating = false;              // trava reentrância de rotação

    static constexpr int kVisibleSideSlots = 5;
    static constexpr int kSidePeekOffset = 70; // deslocamento horizontal entre slots
    static constexpr int kHeroSlideMargin = 30;
};
