#pragma once

#include "AIClient.h"

#include <QFrame>
#include <QImage>
#include <QJsonObject>
#include <QPair>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QVector>
#include <functional>

class ProjectModel;
class ElementsStore;
class DocCache;
class MarkerStore;
class NotesStore;
class WordCounter;
class DialogueStore;
class ConstrutorStore;
class GlossaryStore;
class MapPinsStore;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QScrollArea;
class QTextEdit;
class QTimer;
class QToolButton;

// Uma conversa salva em disco — título (derivado da 1ª pergunta) + data da
// última mensagem. Não guarda as mensagens em si (isso só é lido na hora de
// abrir uma conversa específica via loadSession).
struct AIChatSessionInfo {
    QString id;
    QString title;
    QString updatedAt; // ISO 8601, pronto pra ordenar por string
};

// Painel de chat livre com a Mira — acessível pela TopToolbar, junto de
// Pensário/RefMenu/Construtor. Ao contrário do AISelectionChat (efêmero,
// ancorado a uma seleção de texto), este painel é criado uma vez no
// construtor da MainWindow e só é mostrado/escondido: a conversa persiste
// durante a sessão do app, mesmo fechando e reabrindo o painel.
//
// Transcrição em formato de bolhas de chat de verdade (esquerda/direita,
// cor por autor), conteúdo em QTextEdit (markdown real, não QLabel — QLabel
// tinha bug de quebra de linha patológica em layouts aninhados). Cada
// mensagem da Mira que envolveu busca/leitura de documento carrega essas
// referências recolhidas dentro da própria bolha, atrás de uma setinha
// ("▸ Ver pesquisas") — só expande em itálico pequeno se o usuário clicar.
// Documentos referenciados numa busca viram link clicável dentro da seção
// expandida (abre o documento no editor via setDocOpener) — citação de
// texto livre da resposta ("no Capítulo 3...") não é parseada (frágil
// demais em texto natural); o clicável vive na estrutura que já temos
// certeza, não em NLP sobre a prosa da resposta.
//
// Botão "Ler documentos do projeto" (ícone 📚 no cabeçalho): varre todos os
// capítulos e itens de gaveta do projeto, pede um resumo de cada um pra API
// — um de cada vez, sequencial — e salva tudo em
// <projeto>/ai_context/resumo_projeto.md. Esse arquivo é lido de volta e
// injetado como contexto de base em toda conversa. Incremental por design:
// só resume documentos que ainda não têm seção salva — reler de novo só
// acontece sob pedido explícito do usuário (tool resummarize_document).
//
// Memória estruturada (save_project_note): notas que a própria Mira registra
// durante a conversa (canon/personagem/lore/planejamento/ideia/pendência/
// preferência do autor), cada uma com status (confirmada/em_discussão/
// ideia_futura/descartada). Salvo em <projeto>/ai_context/memoria_mira.md.
//
// Fontes de dados extras, pesquisáveis via search_project/read_document
// (setters opcionais): marcadores com comentário (MarkerStore), notas
// soltas do Pensário (NotesStore), estatísticas de diálogo/química por
// personagem (DialogueStore + DialogueChemistry), sistemas do Construtor
// (ConstrutorStore), Glossário (GlossaryStore) e pins do Mapa-múndi
// (MapPinsStore). Progresso da meta diária (WordCounter) fica direto no
// system prompt e dispara uma bolha de comemoração automática quando a
// meta do dia é batida. lookup_world_data é tool separada — dataset
// geográfico REAL do mundo (GeoData), não passa por collectAllDocs.
//
// Foco em documento (checkbox no rodapé): quando marcado, injeta o texto
// INTEIRO de um documento específico no system prompt (não só o resumo) —
// por padrão o documento atualmente aberto no editor (setCurrentDocTitle
// Provider), mas o usuário pode trocar no combo ao lado. Controle manual,
// mesma filosofia do "considerar cena inteira" do AISelectionChat: custo de
// contexto sob escolha explícita, não decidido pela IA sozinha.
//
// Dois modos de layout, alternáveis no cabeçalho (botão ⛶): "painel"
// (ancorado à direita, estreito) e "janela" (centralizada, maior, estilo
// ChatGPT/Claude). Redimensionável livremente pelo grip no canto
// inferior direito em qualquer um dos dois modos — tamanho persistido por
// modo em QSettings (ai/chatPanelSize / ai/chatWindowSize), não reseta ao
// reabrir.
//
// Conversas persistem em <projeto>/ai_context/sessoes/<id>.json (mensagens
// estruturadas completas) — salva automaticamente a cada turno concluído.
// Botão de histórico no cabeçalho lista conversas anteriores; "Nova
// conversa" arquiva a atual e começa vazia.
class AIChatPanel : public QFrame {
    Q_OBJECT
public:
    AIChatPanel(ProjectModel* projectModel,
                ElementsStore* elementsStore,
                DocCache* docCache,
                QWidget* parent);

    void setProjectRoot(const QString& root);
    void setTopInset(int px);

    // Fontes de dados adicionais (opcionais — Mira funciona sem elas, só com
    // capítulos/gavetas). Injetadas via setter pelo MainWindow, mesmo padrão
    // de PensarioPanel::setMapPinsStore etc.
    void setMarkerStore(MarkerStore* store);
    void setNotesStore(NotesStore* store);
    void setWordCounter(WordCounter* counter);
    void setDialogueStore(DialogueStore* store);
    void setConstrutorStore(ConstrutorStore* store);
    void setGlossaryStore(GlossaryStore* store);
    void setMapPinsStore(MapPinsStore* store);

    // Raízes de todos os projetos conhecidos (recentes), pra alimentar a
    // coluna de projetos/conversas do modo janela. Chamado pela MainWindow
    // sempre que o projeto ativo muda (mesma fonte que já alimenta a
    // Biblioteca — ver loadRecentProjects).
    void setKnownProjects(const QStringList& roots);

    // Navegação: abre no editor o documento (chave "ch:ms:chId"/"it:itemId")
    // referenciado numa busca — usado pelas citações clicáveis.
    void setDocOpener(std::function<void(const QString& docKey)> opener);
    // Título do documento atualmente aberto no editor — usado como padrão
    // do checkbox "Foco em documento" quando ele é marcado.
    void setCurrentDocTitleProvider(std::function<QString()> provider);
    // Ponteiro pro QTextEdit ativo no momento (mesmo widget reaproveitado
    // pra qualquer documento, ver EditorHost) — habilita a tool
    // propose_document_edit, que permite a Mira propor e (sob confirmação
    // do autor) aplicar uma correção pontual direto no documento aberto,
    // sem precisar passar pelo AISelectionChat/seleção manual.
    void setActiveEditorProvider(std::function<QTextEdit*()> provider);

    void togglePanel();
    void openPanel();
    void closePanel();
    bool isPanelOpen() const;

signals:
    // Emitido depois que a tool generate_character_image atualiza a foto de
    // um personagem — MainWindow escuta pra atualizar a CharacterSheetPanel
    // se ela estiver aberta no mesmo item (senão a foto só apareceria ao
    // reabrir a ficha).
    void characterImageUpdated(const QString& itemId);
    // Emitido quando o autor clica "Abrir esse projeto" na barra de
    // conversa somente-leitura (ver previewForeignSession) — MainWindow
    // reage trocando o projeto ativo de verdade (mesmo fluxo de
    // confirmDiscardOrSave + loadProjectFrom usado pela Biblioteca).
    void openProjectRequested(const QString& root);

protected:
    void showEvent(QShowEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct ScanDoc {
        QString title;
        QString plainText;
        QString key; // "ch:ms:chId" / "it:itemId" — vazio pra docs sintéticos (estatísticas etc.)
    };

    // Um trecho do rastro de tool calls do turno atual — texto explicativo +
    // quais documentos (por título) foram tocados, pra virar link clicável.
    struct ToolTraceEntry {
        QString text;
        QStringList docTitles;
    };

    // Resultado de runProjectSearch: texto formatado pra mostrar/mandar pro
    // modelo + títulos únicos dos documentos que bateram, pra citação clicável.
    struct SearchResult {
        QString text;
        QStringList docTitles;
    };

    // Uma edição proposta pela tool propose_document_edit neste turno — vira
    // um cartão com Aplicar/Descartar na bolha final (mesmo padrão do
    // m_pendingBubbleImages: efêmero, populado durante os tool calls do
    // turno e consumido só na hora de montar a bolha). docTitleAtProposal
    // guarda o título do documento que estava aberto quando a sugestão foi
    // criada — como o editor é um QTextEdit ÚNICO reaproveitado pra
    // qualquer documento (troca de conteúdo por baixo via EditorHost), o
    // ponteiro continua o mesmo mesmo se o autor trocar de capítulo entre a
    // sugestão aparecer e o clique em "Aplicar"; sem essa checagem de título
    // a edição podia acabar caindo no documento errado.
    struct PendingEditSuggestion {
        QString originalText;
        QString newText;
        QString explanation;
        QString docTitleAtProposal;
    };

    // Referências de uma bolha já inserida no layout — usado tanto pra
    // mensagens prontas quanto pra atualizar uma bolha da Mira token a
    // token durante o streaming.
    struct BubbleHandle {
        QFrame* bubble = nullptr;
        QTextEdit* textEdit = nullptr;
        QVBoxLayout* bubbleLayout = nullptr;
        int textWidth = 0; // largura usada no fitBubbleHeight — evita reconsultar width() antes do 1º layout
    };

    void buildUi();
    // Reaplica TODAS as stylesheets derivadas do tema (painel, header, combos,
    // input, botões, transcrição e as bolhas já na tela). Chamada no construtor
    // e a cada Theme::Manager::themeChanged — sem isso o painel congela nas
    // cores do tema que estava ativo quando ele foi construído (no boot do
    // app), e passa a brigar com o resto da janela quando o usuário troca de
    // tema ao vivo. Mesmo padrão de PensarioPanel/StatsPanel/RefMenuPanel.
    void applyTheme();
    void ancorRight();
    void applyLayoutMode();
    void toggleLayoutMode();
    void fitBubbleHeight(QTextEdit* te, int textWidth) const;
    void fitInputHeight();
    void refitAllBubbles();
    int transcriptAvailableWidth() const;
    void saveCurrentPanelSize();
    QString buildSystemPrompt() const;
    QString loadProjectSummaryFile() const;
    QString loadProjectMemoryFile() const;
    QString buildDailyProgressSummary() const;
    void onWordCounterProgressChanged();
    void sendUserMessage(const QString& text);
    void setBusy(bool busy);
    void logConversation(const QString& speakerLabel, const QString& text) const;
    void pickAttachImage();
    void clearAttachImage();

    BubbleHandle createBubbleRow(bool isUser, const QString& initialText,
                                 const QString& imageDataUrl = QString());
    void attachToolTraces(BubbleHandle& handle, const QVector<ToolTraceEntry>& traces);
    // Anexa uma ou mais imagens (resolução plena) ao final da bolha — usado
    // pra mostrar imagens que a Mira gerou durante o turno, clicáveis (ver
    // grande/salvar via ClickableImageLabel). Efêmero por design: essas
    // imagens nunca entram em AIChatMessage/m_messages (ver comentário em
    // m_pendingBubbleImages) — só a galeria em disco (GeneratedImageGallery)
    // preserva isso entre sessões.
    void attachBubbleImages(BubbleHandle& handle, const QVector<QImage>& images);
    // Anexa um cartão por sugestão de edição (texto original/novo + botões
    // Aplicar/Descartar) — mesma efemeridade de attachBubbleImages: nunca
    // entra em AIChatMessage/m_messages, some ao recarregar a sessão.
    void attachEditSuggestions(BubbleHandle& handle, const QVector<PendingEditSuggestion>& suggestions);
    void attachFeedbackButtons(BubbleHandle& handle, const QString& fullText);
    void openImageGallery();
    void addUserBubble(const QString& text, const QString& imageDataUrl = QString());
    void addMiraBubble(const QString& text, const QVector<ToolTraceEntry>& traces = {});
    void beginMiraStreamBubble();
    void appendStreamToken(const QString& token);
    void finalizeMiraStreamBubble(const QVector<ToolTraceEntry>& traces,
                                  const QVector<QImage>& images = {},
                                  const QVector<PendingEditSuggestion>& editSuggestions = {});
    void clearTranscriptUi();

    // Bolha "Pensando…" — vive DENTRO do transcript (não mais um status
    // solto abaixo do scroll), pulsa enquanto aguarda resposta, e expande
    // ao clicar pra mostrar o rastro de tool calls ao vivo (mesmo visual de
    // attachToolTraces, só que atualizado durante o turno em vez de só no
    // fim). Criada em showThinkingBubble() (chamada por setBusy(true)),
    // destruída em hideThinkingBubble() (setBusy(false) ou início do
    // streaming de verdade, quando a bolha de resposta toma o lugar dela).
    void showThinkingBubble();
    void hideThinkingBubble();
    void refreshThinkingBubbleDetails();

    void saveCurrentSession();
    QVector<AIChatSessionInfo> listSessionsForRoot(const QString& root) const;
    QVector<AIChatSessionInfo> listSessions() const { return listSessionsForRoot(m_projectRoot); }
    // Lê e parseia uma sessão de QUALQUER projeto (root arbitrário), sem
    // tocar m_messages/m_currentSessionId/UI — usado pelo modo somente-
    // leitura (previewForeignSession). loadSession() continua sendo o
    // caminho pro projeto ATIVO, com todo o efeito colateral de sempre.
    QVector<AIChatMessage> loadSessionMessagesForRoot(const QString& root, const QString& id) const;
    // Apaga um ou mais arquivos de sessão em disco (ai_context/sessoes/
    // <id>.json) — usado pelo menu de contexto "Excluir conversa(s)" do
    // rail. Se alguma das sessões apagadas for a que está carregada agora
    // (projeto ativo), começa uma conversa nova pra não deixar
    // m_currentSessionId apontando pro nada; se estava em preview de uma
    // delas, sai do preview. Reconstrói o rail só UMA vez no final — fazer
    // isso por item (como uma versão antiga chegou a fazer) reabre e
    // reparseia a lista de sessões de TODOS os projetos conhecidos a cada
    // exclusão, o que trava a UI visivelmente com dezenas de itens
    // selecionados.
    void deleteSessionsForRoot(const QString& root, const QStringList& ids);
    // Reconstrói as bolhas da transcrição a partir de uma lista de
    // mensagens (mesma regra de loadSession(): só user/assistant viram
    // bolha, system/tool ficam de fora) — reusado por loadSession() e pelo
    // modo somente-leitura.
    void renderMessageHistory(const QVector<AIChatMessage>& messages);
    void loadSession(const QString& id);
    void startNewSession();
    void showSessionMenu();

    // Modo somente-leitura: mostra uma conversa salva de OUTRO projeto
    // (root != m_projectRoot) sem mexer no projeto ativo nem nas tools —
    // ver nota de escopo no plano (Mira Studio). Composer fica desabilitado
    // e uma barra oferece "Abrir esse projeto" (openProjectRequested).
    void previewForeignSession(const QString& root, const QString& projectName, const QString& id);
    void exitPreviewMode();

    void rebuildProjectRail();
    // Aberto ao clicar no avatar/nome da Mira no cabeçalho (modo janela) —
    // ver eventFilter() e MiraPersonalityDialog.
    void openPersonalityDialog();
    // Nome de exibição de um projeto qualquer (root arbitrário) — lê
    // project.mira.json (mesma API root-agnóstica de rebuildProjectRail),
    // com fallback pro nome da pasta. Reusado pela barra de contexto.
    QString projectDisplayName(const QString& root) const;
    // Atualiza a barra de contexto do topo do chat ("Projeto · Conversa")
    // e o chip de memória do cabeçalho — chamado sempre que o
    // projeto/sessão/preview mudam.
    void updateChatContext();

    void refreshDocFocusCombo();
    void onDocFocusChanged();

    void startProjectScan();
    void processNextScanItem();
    void onScanDocFinished(const QString& summary);
    void finishScan();
    QVector<ScanDoc> collectAllDocs() const;
    // Formato de resumo_projeto.md: blocos "## título\n\nconteúdo". Parsear/
    // regravar em vez de só concatenar permite scan incremental (item novo
    // = ainda não tem título no arquivo) sem perder o que já foi lido antes.
    QVector<QPair<QString, QString>> parseSummaryFile(const QString& raw) const;
    void writeSummaryFile(const QVector<QPair<QString, QString>>& sections) const;

    void handleToolCall(const QString& id, const QString& name, const QJsonObject& arguments);
    void handleSearchProjectTool(const QString& id, const QJsonObject& arguments);
    void handleReadDocumentTool(const QString& id, const QJsonObject& arguments);
    void handleSaveProjectNoteTool(const QString& id, const QJsonObject& arguments);
    void handleSaveUserNoteTool(const QString& id, const QJsonObject& arguments);
    void handleResummarizeDocumentTool(const QString& id, const QJsonObject& arguments);
    void handleLookupWorldDataTool(const QString& id, const QJsonObject& arguments);
    void handleGenerateCharacterImageTool(const QString& id, const QJsonObject& arguments);
    void handleGenerateSceneImageTool(const QString& id, const QJsonObject& arguments);
    void handleProposeDocumentEditTool(const QString& id, const QJsonObject& arguments);
    QString runWorldDataLookup(const QString& query) const;
    void finishToolRoundTrip(const QString& id, const QString& toolName,
                             const QJsonObject& argumentsEcho, const QString& resultText);
    SearchResult runProjectSearch(const QString& query) const;
    const ScanDoc* findDocByTitle(const QVector<ScanDoc>& docs, const QString& title) const;

    AIClient* m_client;
    ProjectModel* m_projectModel;
    ElementsStore* m_elementsStore;
    DocCache* m_docCache;
    MarkerStore* m_markerStore = nullptr;
    NotesStore* m_notesStore = nullptr;
    WordCounter* m_wordCounter = nullptr;
    DialogueStore* m_dialogueStore = nullptr;
    ConstrutorStore* m_construtorStore = nullptr;
    GlossaryStore* m_glossaryStore = nullptr;
    MapPinsStore* m_mapPinsStore = nullptr;
    std::function<void(const QString&)> m_docOpener;
    std::function<QString()> m_currentDocTitleProvider;
    std::function<QTextEdit*()> m_activeEditorProvider;
    QString m_projectRoot;
    QString m_dailyGoalNotifiedDateKey; // dia (YYYY-MM-DD) já avisado — evita repetir

    // Modo somente-leitura (ver previewForeignSession) — true enquanto a
    // transcrição mostra uma conversa de um projeto que NÃO é m_projectRoot.
    bool m_previewMode = false;
    QString m_previewRoot;
    QStringList m_knownProjectRoots; // recentes, ver setKnownProjects

    QVector<AIChatMessage> m_messages;
    QString m_currentSessionId; // vazio = conversa ainda não salva em disco
    bool m_assistantTurnOpen = false; // há uma bolha de streaming em aberto
    bool m_pendingToolCall = false;   // true entre um toolCallReceived() e o reenvio com o resultado
    int m_toolHopCount = 0;           // trava de segurança contra loop de tool calling
    QVector<ToolTraceEntry> m_pendingToolTraces; // rastro de buscas/leituras do turno atual
    // Imagens (resolução plena) geradas pela tool generate_character_image
    // neste turno — só pra EXIBIR na bolha final, nunca gravadas em
    // AIChatMessage/m_messages (senão AIClient::sendMessage reenviaria a
    // imagem em todo turno futuro da conversa pra sempre, já que ela
    // serializa imageDataUrl pra qualquer role, não só "user"). Persistência
    // de verdade fica a cargo de GeneratedImageGallery (arquivo em disco).
    QVector<QImage> m_pendingBubbleImages;
    QVector<PendingEditSuggestion> m_pendingEditSuggestions; // sugestões de propose_document_edit neste turno
    BubbleHandle m_currentMiraBubble; // bolha em streaming, se houver
    QString m_streamingText;          // acumulado token a token da bolha atual

    bool m_scanning = false;
    QVector<ScanDoc> m_scanQueue;
    int m_scanIndex = 0;
    QStringList m_scanSummaries;                          // resumo cru de cada item de m_scanQueue, na mesma ordem
    QVector<QPair<QString, QString>> m_scanExistingSections; // seções já existentes no arquivo, carregadas antes do scan começar

    QWidget* m_header = nullptr;
    // "Identidade" da Mira — avatar + nome/subtítulo + chip de memória, só
    // visível em modo janela (mesmo critério do rail: espaço maior, UI mais
    // rica). Fica ACIMA de m_header, que continua com os ícones de ação em
    // qualquer modo.
    QWidget* m_studioHeaderWidget = nullptr;
    QLabel* m_studioAvatarLabel = nullptr;
    QLabel* m_studioNameLabel = nullptr;
    QLabel* m_studioSubtitleLabel = nullptr;
    QLabel* m_memoryChipLabel = nullptr;
    // Corpo abaixo do header: rail (só visível em modo janela) + coluna de
    // chat (transcrição+composer, sempre visível) lado a lado.
    QWidget* m_railWidget = nullptr;
    QLineEdit* m_railSearchEdit = nullptr;
    QString m_railFilterText;
    QWidget* m_railScrollContent = nullptr;
    QVBoxLayout* m_railScrollLayout = nullptr;
    QWidget* m_chatColumnWidget = nullptr;
    // Barra "Projeto · Conversa" (modo janela, fora do preview) — troca de
    // lugar com m_previewBanner conforme o estado (ver updateChatContext).
    QLabel* m_chatContextLabel = nullptr;
    QWidget* m_previewBanner = nullptr;
    QLabel* m_previewBannerLabel = nullptr;
    QPushButton* m_previewOpenBtn = nullptr;
    QLabel* m_titleLabel = nullptr;
    QComboBox* m_modelCombo = nullptr;
    QToolButton* m_scanBtn = nullptr;
    QToolButton* m_galleryBtn = nullptr;
    QToolButton* m_historyBtn = nullptr;
    QToolButton* m_newChatBtn = nullptr;
    QToolButton* m_layoutBtn = nullptr;
    QToolButton* m_closeBtn = nullptr;
    QScrollArea* m_transcriptScroll = nullptr;
    QWidget* m_transcriptContent = nullptr;
    QVBoxLayout* m_transcriptLayout = nullptr;
    // m_docFocusCheck/m_docFocusCombo continuam existindo como estado (lidos
    // por buildSystemPrompt/onDocFocusChanged), mas não vivem mais numa linha
    // própria da UI — m_docFocusBtn (no composer) é o único jeito de mexer
    // neles agora, via menu popup.
    QCheckBox* m_docFocusCheck = nullptr;
    QComboBox* m_docFocusCombo = nullptr;
    QToolButton* m_docFocusBtn = nullptr;
    QTextEdit* m_inputEdit = nullptr;
    QPushButton* m_sendBtn = nullptr;
    QToolButton* m_attachBtn = nullptr;
    QWidget* m_attachPreviewRow = nullptr;
    QLabel* m_attachThumb = nullptr;
    QToolButton* m_attachClearBtn = nullptr;
    QImage m_pendingAttachImage; // imagem escolhida pelo usuário, ainda não enviada
    QLabel* m_statusLabel = nullptr; // usado só pela varredura de documentos (m_scanning) — ver showThinkingBubble() pro status por turno de chat
    QWidget* m_resizeGrip = nullptr;

    // Ver showThinkingBubble()/hideThinkingBubble()/refreshThinkingBubbleDetails().
    QWidget* m_thinkingRow = nullptr;
    QToolButton* m_thinkingToggle = nullptr;
    QVBoxLayout* m_thinkingDetailsLay = nullptr;
    QTimer* m_thinkingPulseTimer = nullptr;
    qreal m_thinkingPulsePhase = 0.0;

    int m_topInset = 0;
    bool m_positioned = false;
    bool m_windowMode = false; // false = ancorado (padrão), true = janela centralizada maior
    bool m_dragging = false;
    QPoint m_dragOffset;
    bool m_resizingPanel = false;
    QPoint m_resizeStartMouse;
    QSize m_resizeStartSize;
};
