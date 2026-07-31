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
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QTextEdit;
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

    // Navegação: abre no editor o documento (chave "ch:ms:chId"/"it:itemId")
    // referenciado numa busca — usado pelas citações clicáveis.
    void setDocOpener(std::function<void(const QString& docKey)> opener);
    // Título do documento atualmente aberto no editor — usado como padrão
    // do checkbox "Foco em documento" quando ele é marcado.
    void setCurrentDocTitleProvider(std::function<QString()> provider);

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
    void openImageGallery();
    void addUserBubble(const QString& text, const QString& imageDataUrl = QString());
    void addMiraBubble(const QString& text, const QVector<ToolTraceEntry>& traces = {});
    void beginMiraStreamBubble();
    void appendStreamToken(const QString& token);
    void finalizeMiraStreamBubble(const QVector<ToolTraceEntry>& traces,
                                  const QVector<QImage>& images = {});
    void clearTranscriptUi();

    void saveCurrentSession();
    QVector<AIChatSessionInfo> listSessions() const;
    void loadSession(const QString& id);
    void startNewSession();
    void showSessionMenu();

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
    void handleResummarizeDocumentTool(const QString& id, const QJsonObject& arguments);
    void handleLookupWorldDataTool(const QString& id, const QJsonObject& arguments);
    void handleGenerateCharacterImageTool(const QString& id, const QJsonObject& arguments);
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
    QString m_projectRoot;
    QString m_dailyGoalNotifiedDateKey; // dia (YYYY-MM-DD) já avisado — evita repetir

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
    BubbleHandle m_currentMiraBubble; // bolha em streaming, se houver
    QString m_streamingText;          // acumulado token a token da bolha atual

    bool m_scanning = false;
    QVector<ScanDoc> m_scanQueue;
    int m_scanIndex = 0;
    QStringList m_scanSummaries;                          // resumo cru de cada item de m_scanQueue, na mesma ordem
    QVector<QPair<QString, QString>> m_scanExistingSections; // seções já existentes no arquivo, carregadas antes do scan começar

    QWidget* m_header = nullptr;
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
    QCheckBox* m_docFocusCheck = nullptr;
    QComboBox* m_docFocusCombo = nullptr;
    QPlainTextEdit* m_inputEdit = nullptr;
    QPushButton* m_sendBtn = nullptr;
    QToolButton* m_attachBtn = nullptr;
    QWidget* m_attachPreviewRow = nullptr;
    QLabel* m_attachThumb = nullptr;
    QToolButton* m_attachClearBtn = nullptr;
    QImage m_pendingAttachImage; // imagem escolhida pelo usuário, ainda não enviada
    QLabel* m_statusLabel = nullptr;
    QWidget* m_resizeGrip = nullptr;

    int m_topInset = 0;
    bool m_positioned = false;
    bool m_windowMode = false; // false = ancorado (padrão), true = janela centralizada maior
    bool m_dragging = false;
    QPoint m_dragOffset;
    bool m_resizingPanel = false;
    QPoint m_resizeStartMouse;
    QSize m_resizeStartSize;
};
