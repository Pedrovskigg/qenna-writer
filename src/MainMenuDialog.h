#pragma once

#include <QDialog>
#include <QPixmap>
#include <QString>
#include <QStringList>

class QComboBox;
class QFrame;
class QGraphicsOpacityEffect;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QTimer;
class QVBoxLayout;
class FlowLayout;
class FlowScrollArea;

// Tela inicial / menu do app — figurino próprio do Mira 2, layout editorial
// em duas colunas (deliberadamente diferente do topo-centralizado do Mira 1):
//   - Barra lateral à esquerda: logo em bom tamanho, quote literário rotativo
//     numa caixa de altura estável, e botões Novo/Carregar com ícones. Seletor
//     de idioma discreto no rodapé da barra.
//   - Área principal à direita: cabeçalho "Biblioteca" + contagem, alternador
//     de visualização (Estante / Lista) e o grid de projetos.
//       * Estante: parede de capas (vitrine 3D) que quebra em linhas, com
//         scroll vertical. Hover em um card escurece os demais.
//       * Lista: linhas ricas em metadados (capa miniatura + autor + gêneros).
//
// Aparece automaticamente no startup quando não há projeto carregado, e
// é invocado também quando o usuário pede "Carregar projeto" pela barra.
class MainMenuDialog : public QDialog {
    Q_OBJECT
public:
    explicit MainMenuDialog(QWidget* parent = nullptr);

    void setRecentProjects(const QStringList& paths);
    void setAutoOpenPath(const QString& path);

signals:
    void newProjectRequested();
    void loadProjectRequested();
    void openRecentRequested(const QString& path);
    void removeRecentRequested(const QString& path);
    void deleteProjectRequested(const QString& path);
    void autoOpenChanged(const QString& path, bool enabled);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void applyDialogStyle();

private:
    enum class ViewMode { Estante, Lista };

    void buildUi();
    void buildSidebar(QVBoxLayout* col);
    void buildMainArea(QVBoxLayout* col);
    void refreshRecents();
    // Repovoa o container da visualização ativa (Estante ou Lista) a partir de
    // m_recentPaths e reflui o scroll.
    void populateActiveView();
    void setViewMode(ViewMode mode);
    void rotateQuote();
    // Troca o texto do quote pelo próximo e reagenda o timer, com fade-in.
    // Chamado ao fim do fade-out de rotateQuote (ou direto na 1ª exibição).
    void showNextQuote();
    // Hover-darken: realça o card sob o cursor escurecendo todos os outros.
    // Passar nullptr restaura todos pra opacidade cheia. Só age na Estante.
    void setHoveredCard(QWidget* hovered);
    // Abre o diálogo de edição do projeto (nome/autor/gêneros/sinopse/capa)
    // e grava as alterações direto no índice. Atualiza o grid no fim.
    void editProject(const QString& path);
    // Mostra o diálogo de confirmação com countdown; ao confirmar, emite
    // deleteProjectRequested para o MainWindow fazer a exclusão de fato.
    void confirmDeleteProject(const QString& path);
    // Recolore os ícones dos botões de ação conforme o tema atual (SVG tintado
    // no load não acompanha troca de tema sozinho).
    void refreshActionIcons();

    // --- Barra lateral ---
    QLabel* m_quoteLabel = nullptr;
    QGraphicsOpacityEffect* m_quoteOpacity = nullptr;
    QLabel* m_logoLabel = nullptr;
    QTimer* m_quoteTimer = nullptr;
    QPushButton* m_newBtn = nullptr;
    QPushButton* m_loadBtn = nullptr;
    QComboBox* m_langCombo = nullptr;

    // --- Área principal ---
    QLabel* m_headingLabel = nullptr;
    QLabel* m_countLabel = nullptr;
    QPushButton* m_estanteBtn = nullptr;
    QPushButton* m_listaBtn = nullptr;
    FlowScrollArea* m_recentsScroll = nullptr;
    QWidget* m_holder = nullptr;          // conteúdo do scroll
    QWidget* m_gridContainer = nullptr;   // parede de capas (FlowLayout)
    QWidget* m_listContainer = nullptr;   // linhas (QVBoxLayout)
    FlowLayout* m_gridFlow = nullptr;
    QVBoxLayout* m_listCol = nullptr;
    QLabel* m_emptyLabel = nullptr;

    ViewMode m_viewMode = ViewMode::Estante;
    QList<QWidget*> m_cards;   // BookCards da Estante (para hover-darken)
    QStringList m_recentPaths;
    QString m_autoOpenPath;
};
