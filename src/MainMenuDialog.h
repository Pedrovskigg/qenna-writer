#pragma once

#include <QDialog>
#include <QHash>
#include <QImage>
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
class QVariantAnimation;
class QVBoxLayout;
class QPoint;
class StackView;
struct LibraryHooks;

// Tela inicial / menu do app — figurino próprio do Mira 2, layout editorial
// em duas colunas (deliberadamente diferente do topo-centralizado do Mira 1):
//   - Barra lateral à esquerda: logo em bom tamanho, quote literário rotativo
//     numa caixa de altura estável, e botões Novo/Carregar com ícones. Seletor
//     de idioma discreto no rodapé da barra.
//   - Área principal à direita: cabeçalho "Biblioteca" + contagem e as
//     vistas (ver LibraryViews):
//       * Continuar (padrão): o último projeto em destaque, com o Onde parei,
//         a meta de hoje e um botão que volta pro cursor; os outros embaixo.
//       * Vitrine: grade de capas com informação, busca e ordem.
//       * Cinema: a capa do projeto desfocada no fundo, carrossel embaixo.
//       * Seu dia: lista compacta + coluna com meta, sequência, semana e
//         lembretes de hoje.
//       * Estante: prateleiras com as capas de frente, o atual puxado pra fora.
//       * Pilha: livro herói em destaque e os demais numa faixa lateral em
//         "peek" — roda do mouse gira o baralho — ver StackView.
//     Sem projeto nenhum, no lugar das vistas entram as boas-vindas.
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
    void newIdeaRequested();
    void loadProjectRequested();
    void openRecentRequested(const QString& path);
    // Abre o projeto e volta pro último ponto do "Onde parei" (cursor na frase).
    void resumeRequested(const QString& path);
    void removeRecentRequested(const QString& path);
    void deleteProjectRequested(const QString& path);
    void autoOpenChanged(const QString& path, bool enabled);
    // Emitido após o Mira Cover fechar e a capa ser gravada no JSON do projeto.
    void coverUpdated(const QString& projectPath);
    // Emitido quando o user clica em "Verificar atualizações".
    void checkUpdatesRequested();
    // Emitido quando "Criar capa" é clicado mas o Cover Creator não está
    // instalado nem disponível localmente — MainWindow reage baixando a
    // release mais recente do GitHub.
    void coverCreatorInstallRequested();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void applyDialogStyle();

private:
    // A ordem é a dos botões do cabeçalho (e o índice deles).
    enum class ViewMode { Continuar, Vitrine, Cinema, SeuDia, Estante, Pilha };

    void buildUi();
    void buildSidebar(QVBoxLayout* col);
    void buildMainArea(QVBoxLayout* col);
    void refreshRecents();
    // Monta de novo a vista ativa a partir de m_recentPaths.
    void populateActiveView();
    // O que as vistas podem pedir (abrir, continuar, menu…), já adiado pro
    // próximo giro do loop.
    LibraryHooks makeHooks();
    // Botão direito num projeto, em qualquer vista.
    void showProjectMenu(const QString& path, const QPoint& globalPos);
    // Grava o idioma e reinicia o app (seletor da barra e das boas-vindas).
    void applyLanguage(const QString& lang);
    void setViewMode(ViewMode mode);
    void rotateQuote();
    // Troca o texto do quote pelo próximo e reagenda o timer, com fade-in.
    // Chamado ao fim do fade-out de rotateQuote (ou direto na 1ª exibição).
    void showNextQuote();
    // --- Logo rotativo da sidebar ---
    // Varre assets/logo/main-menu-Q em busca das artes da letra Q. A pasta é
    // copiada pro lado do executável pelo alvo sync-runtime-assets, então
    // basta jogar um PNG novo lá: nada neste arquivo precisa mudar.
    void loadLogoVariants();
    // Arte do índice, já normalizada e cacheada — carregada na primeira vez
    // que é pedida, não todas de uma vez na abertura do menu.
    QImage logoVariant(int index);
    // Faz o crossfade da arte atual pra próxima e reagenda o timer.
    void rotateLogo();
    // Abre o diálogo de edição do projeto (nome/autor/gêneros/sinopse/capa)
    // e grava as alterações direto no índice. Atualiza o grid no fim.
    void editProject(const QString& path);
    // Mostra o diálogo de confirmação com countdown; ao confirmar, emite
    // deleteProjectRequested para o MainWindow fazer a exclusão de fato.
    void confirmDeleteProject(const QString& path);
    // Lança o Mira Cover passando o caminho do projeto como argumento.
    void launchMiraCover(const QString& projectPath);
    // Inicia o processo do Mira Cover e conecta o sinal finished.
    void startCoverProcess(const QString& program, const QStringList& args,
                           const QString& workingDir, const QString& projectPath);
    // Lê cover.jpg do projeto, converte para data URL e grava no JSON.
    void updateCoverFromFile(const QString& projectPath);
    // Recolore os ícones dos botões de ação conforme o tema atual (SVG tintado
    // no load não acompanha troca de tema sozinho).
    void refreshActionIcons();

    // --- Barra lateral ---
    QLabel* m_quoteLabel = nullptr;
    QGraphicsOpacityEffect* m_quoteOpacity = nullptr;
    QLabel* m_logoLabel = nullptr;
    QTimer* m_quoteTimer = nullptr;
    // Caminhos das artes do Q, em ordem alfabética. Vazio = logo fixo antigo.
    QStringList m_logoPaths;
    QHash<int, QImage> m_logoFrames;
    int m_logoIndex = -1;
    QTimer* m_logoTimer = nullptr;
    QVariantAnimation* m_logoAnim = nullptr;
    QPushButton* m_newBtn = nullptr;
    QPushButton* m_newIdeaBtn = nullptr;
    QPushButton* m_loadBtn = nullptr;
    QPushButton* m_trashBtn = nullptr;
    QComboBox* m_langCombo = nullptr;

    // --- Área principal ---
    QLabel* m_headingLabel = nullptr;
    QLabel* m_countLabel = nullptr;
    QWidget* m_header = nullptr;          // título + botões das vistas (some sem projeto)
    QList<QPushButton*> m_viewBtns;       // índice = ViewMode
    QWidget* m_viewHost = nullptr;        // onde a vista ativa mora
    QWidget* m_currentView = nullptr;     // vista montada agora (menos a Pilha)
    StackView* m_stackView = nullptr;     // pilha — fixa, só esconde

    ViewMode m_viewMode = ViewMode::Continuar;
    QStringList m_recentPaths;
    QString m_autoOpenPath;
};
