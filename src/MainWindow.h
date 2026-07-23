#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "CharacterDetector.h"
#include "DialogueStore.h"
#include "EditorHost.h"
#include "MemoriesStore.h"
#include "PresenceTypes.h"
#include "TopToolbar.h"

#include <QColor>
#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QString>
#include <QTextCursor>
#include <QTextEdit>
#include <functional>
#include <memory>
#include <optional>

class QLabel;
class QScrollArea;
class QScrollBar;
class QTextEdit;
class QTimer;
// TopToolbar já incluído acima (necessário para TopToolbar::AlignScope)
class ImageOverlay;
class LeftBar;
class DrawerListPanel;
class ManuscriptPanel;
class ProjectModel;
class DocCache;
class EditorHost;
class VariationBar;
class ProjectSaver;
class WordCounter;
class WordCountPanel;
class RefMenuPanel;
class PensarioPanel;
class ElementsStore;
class SpellChecker;
class SpellHighlighter;
class SpellEditor;
class SettingsPanel;
class SelectionPopup;
class ThemesPanel;
class ProjectInfoPanel;
class ProjectInfoHover;
class BondPopup;
class BondViewPanel;
class MarkerStore;
class NotesStore;
class MapPinsStore;
class MarkerPickPopup;
class MarkerHoverPopup;
class AmbienceManager;
class AmbiencePanel;
class GlossaryStore;
class GlossaryPanel;
class HelpPanel;
class GlossaryAddPopup;
class MemoriesStore;
class MemoryAddPopup;
class ConstrutorStore;
class ConstrutorWindow;
class MainMenuDialog;
class BackgroundWidget;
class RemindersStore;
class RemindersPanel;
class GroupsPanel;
class LousaPanel;
class TimelinePanel;
class CharacterSheetPanel;
class SheetTemplatesStore;
class MentionPopup;
class UpdateChecker;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class AutoNavHint;
class QToolButton;
class FindBar;
class GlobalSearchPanel;
class PresencePopup;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void setAvailableFontFamilies(const QStringList &families);
    bool hasProjectLoaded() const { return !projectRoot.isEmpty(); }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void setupEditor();
    void setupToolbar();
    void updateEditorContainerMargins();
    void applyEditorStyle();
    void applyTextColor();
    void applyEditorFont();
    void updateDocCachePinnedKeys();

    // Camadas de ExtraSelections do editor — focus mode (cor do bloco focado)
    // e find (highlight da busca local) precisam coexistir. Cada chamador
    // atualiza só a sua camada via setEditorSelectionsLayer e o helper aplica
    // a união no editor.
    void setEditorSelectionsLayer(const QString& layer, const QList<QTextEdit::ExtraSelection>& sels);
    void positionFindBar();
    void positionGlobalSearchPanel();

    void setFontFamily(const QString &family);
    void setFontSize(qreal pt);
    void setLineHeight(int percent);
    void applyProjectTypeDefaults();
    void onAlignmentRequested(Qt::Alignment alignment, TopToolbar::AlignScope scope);
    void setFirstLineIndent(bool enabled);
    void setParagraphSpacingBefore(int px);
    void setParagraphSpacingAfter(int px);
    void setBold(bool enabled);
    void setItalic(bool enabled);
    void setUnderline(bool enabled);
    void setStrikethrough(bool enabled);
    void syncInlineFormatButtons();
    void setFocusMode(bool enabled);
    void setReadMode(bool enabled);
    void positionReadModeHotzones();
    void updateFocusedBlock();
    void onAddImageRequested();
    void onNewProjectRequested();
    void onOpenProjectRequested();
    void onSettingsRequested();
    void onExportRequested();
    void onThemePanelRequested();
    void onThemeChanged();
    void onEditorLayoutChanged();
    void applyEditorLayout();
    void resizeEditorColumnToViewport();
    // Altura útil da "folha" visível (viewport do editorScroll menos elementos
    // fora da folha). É o teto natural do comprimento de página: acima disso a
    // folha seria cortada fora da janela.
    int availableSheetHeight() const;
    void applyPageShadow();
    void applyBackgroundFromTheme();
    void applySpellLanguageFromModel();
    void positionWordCountPanel();
    void positionSidePanels();
    // Ficha de Personagem: painel embutido que substitui o editor para itens isSheet.
    CharacterSheetPanel* ensureCharacterSheetPanel();
    void showCharacterSheet(const QString& itemId);
    void hideCharacterSheet();
    void positionCharacterSheet();
    bool confirmDiscardOrSave();
    bool loadProjectFrom(const QString& root, QString* errorOut = nullptr);
    void rememberLastProject(const QString& root);
    QString loadLastProjectPath() const;
    QStringList loadRecentProjects() const;
    void saveRecentProjects(const QStringList& list);
    void openMainMenu();
    void rememberLastDocFor(const QString& root);
    void restoreLastDocFor(const QString& root);
    void applyProjectRoot(const QString& root);

    // Pensário: abre o documento de um comentário e salta até o trecho.
    void openMarkerInEditor(const QString& docKey, int start, int end, const QString& text);
    EditorHost::ViewMode viewModeForDocKey(const QString& docKey) const;

    void openMarkerPickerForSelection(bool withComment);
    void openMarkerPickerForEdit(const QString& markerId);
    void applyMarkerFromPicker(const QColor& color, const QString& comment);
    void removeMarkerFromPicker();
    bool markerAtViewportPos(const QPoint& viewportPos, QString& outId,
                             QRect& outBoundsGlobal) const;

    void showReminderToast(const QString& title, const QString& body);
    void positionReminderToast();

    // Atualizações (GitHub Releases): checagem silenciosa no startup; se
    // houver versão nova, mostra um toast com botão pra baixar+instalar.
    void showUpdateToast(const QString& version, const QString& downloadUrl,
                         const QString& releaseNotes, bool isCover = false);
    void positionUpdateToast();
    void startUpdateDownload();
    void cancelUpdateDownload();
    void showUpdateDownloadError(const QString& message);
    void resetUpdateToastIdle();

    // Cover Creator via download (sem bundle no instalador do Qenna desde
    // 2026-07-19): checa a release mais recente no GitHub e, se aceito,
    // baixa o portable direto pra "<appDir>/Cover Creator/Mira Cover.exe" —
    // não precisa "instalar" de verdade, é Electron portable (single exe).
    // silent=true não mostra nada se não houver internet/release (usado no
    // gatilho automático de primeiro launch); silent=false avisa o usuário
    // (usado quando ele pede explicitamente, ex. clicou em "Criar capa").
    void requestCoverCreatorInstall(bool silent);

    void closeBondPopup();
    void closeBondViewPanel();
    void openBondCreatePopup(const QString& drawerKey, const QString& fromItemId,
                             const QString& toItemId, const QPoint& spawnGlobal);
    void openBondViewPanel(const QString& drawerKey, const QString& bondId,
                           const QPoint& spawnGlobal);
    void openBondEditPopup(const QString& drawerKey, const QString& bondId,
                           const QPoint& spawnGlobal);
    void createDocFromBond(const QString& drawerKey, const QString& bondId);
    void createDocFromSelection();
    void createTimelineEventFromSelection();
    // Abre o popup de "Adicionar à memória…" a partir da seleção atual.
    void addSelectionToMemory();
    // Abre a fonte da memória no editor e seleciona o trecho ("Ctrl+F" auto).
    void openMemoryInEditor(const MemoriesStore::Memory& mem);
    // Abre a fonte de um diálogo detectado no editor e seleciona o trecho.
    void openDialogueInEditor(const DialogueStore::Dialogue& dlg);
    TimelinePanel* ensureTimelinePanel();  // cria o painel (lazy) e devolve
    // Texto puro de um doc do projeto p/ a descrição de um evento da timeline.
    // linkKey: "ch:<id>" | "sc:<id>" | "doc:<id>". Trunca em ~600 palavras + aviso.
    QString docTextForLink(const QString& linkKey);
    void navigateAdjacentChapter(int dir);
    void activateNavZone(int dir, const EditorHost::ViewMode& vm);
    void deactivateNavZone();
    void positionExternalScrollBar();
    void positionAutoNavHint();

    bool findImageAt(const QPoint &viewportPos, QTextCursor &imageCursor) const;
    int detectAlignmentForImage(const QTextCursor &imageCursor) const;
    void showOverlayForImage(const QTextCursor &imageCursor);
    void hideOverlay();
    void changeSelectedImageWidth(int delta);
    void changeSelectedImageAlignment(int alignment);
    void detectScenesForPending();
    // Deriva a presença por CENA (docKey "...::scene:<id>") a partir de quem já
    // está confirmado/auto-marcado no capítulo inteiro + match de token na cena.
    // Sem popup de confirmação própria — a confirmação já aconteceu no capítulo.
    void syncScenePresenceForChapter(const QString& chapterId);
    // Botão em Configurações: roda syncScenePresenceForChapter em todos os
    // capítulos do projeto, um por tick de timer (não trava a UI, ver pedido
    // do usuário após crash anterior do detector rodando tudo de uma vez).
    void rescanAllChapterScenesPresence();
    // Lê um capítulo do docCache/disco (não do editor vivo) e roda o motor
    // de diálogos nele — usado pelo scan em lote abaixo. O timer de diálogo
    // "ao vivo" (dialogueDetectionTimer) continua separado, dedicado ao
    // capítulo aberto no editor.
    void scanChapterDialogues(const QString& chapterId);
    // Botão no Pensário > Diálogos: mesmo padrão incremental de
    // rescanAllChapterScenesPresence, mas pro motor de diálogos — evita ter
    // que abrir capítulo por capítulo em projetos grandes.
    void rescanAllChapterDialogues();
    // Toast flutuante (centro inferior da janela) com barra de progresso do
    // scan em lote acima — criado no primeiro tick, atualizado a cada um,
    // destruído no final. Aviso visual mais forte pedido pelo usuário depois
    // de ver o app "travar" (na real, só sem feedback) num projeto grande.
    void updateDialogueScanToast(int done, int total);
    void hideDialogueScanToast();
    // Persiste markAll/neverIds/rejectedKeys em QSettings, por projeto (mesma
    // chave/grupo já usados pro markAll) — chamar sempre que qualquer um dos
    // três mudar, pra decisão do usuário nunca precisar ser repetida.
    void saveDetectionState();

    SpellEditor *editor;
    TopToolbar *toolbar;
    ImageOverlay *imageOverlay;
    LeftBar *leftBar;
    DrawerListPanel *drawerListPanel;
    ManuscriptPanel *manuscriptPanel;
    // Análise de presença por capítulo/cena — compartilhada entre o Drawer
    // (consistência) e a Timeline (trilhas automáticas de personagem).
    std::function<void(const QStringList&, QHash<QString, CharPresenceResult>*,
                       int*, int*)> m_presenceProvider;
    ProjectModel *projectModel;
    DocCache *docCache;
    EditorHost *editorHost;
    VariationBar *variationBar;
    ProjectSaver *projectSaver;
    WordCounter *wordCounter;
    WordCountPanel *wordCountPanel;
    RefMenuPanel *refMenuPanel;
    PensarioPanel *pensarioPanel = nullptr;
    ElementsStore *elementsStore;
    SpellChecker *spellChecker;
    SpellHighlighter *spellHighlighter;
    SettingsPanel *settingsPanel;
    ThemesPanel *themesPanel;
    ProjectInfoPanel *projectInfoPanel = nullptr;
    ProjectInfoHover *projectInfoHover = nullptr;
    QTimer *projectInfoHoverOpenTimer = nullptr;
    QTimer *projectInfoHoverCloseTimer = nullptr;
    SelectionPopup *selectionPopup;
    BondPopup *bondPopup = nullptr;
    BondViewPanel *bondViewPanel = nullptr;
    MarkerStore *markerStore = nullptr;
    NotesStore *notesStore = nullptr;
    MapPinsStore *mapPinsStore = nullptr;
    MarkerPickPopup *markerPickPopup = nullptr;
    MarkerHoverPopup *markerHoverPopup = nullptr;
    AmbienceManager *ambienceManager = nullptr;
    AmbiencePanel *ambiencePanel = nullptr;
    GlossaryStore *glossaryStore = nullptr;
    GlossaryPanel *glossaryPanel = nullptr;
    HelpPanel *helpPanel = nullptr;
    GlossaryAddPopup *glossaryAddPopup = nullptr;
    MemoriesStore *memoriesStore = nullptr;
    MemoryAddPopup *memoryAddPopup = nullptr;
    DialogueStore *dialogueStore = nullptr;
    ConstrutorStore *construtorStore = nullptr;
    ConstrutorWindow *construtorWindow = nullptr;
    // Memória sendo criada: preenchida em addSelectionToMemory() (texto + fonte)
    // e completada no confirmed do popup (nome + destino).
    std::optional<MemoriesStore::Memory> m_pendingMemory;
    RemindersStore *remindersStore = nullptr;
    RemindersPanel *remindersPanel = nullptr;
    SheetTemplatesStore *sheetTemplatesStore = nullptr;
    QTimer *m_reminderPollTimer = nullptr;
    GroupsPanel *groupsPanel = nullptr;
    LousaPanel *lousaPanel = nullptr;
    TimelinePanel *timelinePanel = nullptr;
    CharacterSheetPanel *characterSheetPanel = nullptr;
    MentionPopup *mentionPopup = nullptr;
    QFrame  *m_reminderToast      = nullptr;
    QLabel  *m_reminderToastTitle = nullptr;
    QLabel  *m_reminderToastBody  = nullptr;
    QTimer  *m_reminderToastTimer = nullptr;

    UpdateChecker *m_updateChecker = nullptr;
    QFrame      *m_updateToast         = nullptr;
    QLabel      *m_updateToastLabel    = nullptr;
    QLabel      *m_updateToastNotes    = nullptr;
    QLabel      *m_updateToastError    = nullptr;
    QToolButton *m_updateToastBtn      = nullptr;
    QToolButton *m_updateToastCancelBtn = nullptr;
    QProgressBar *m_updateToastProgress = nullptr;
    QNetworkAccessManager *m_updateNam = nullptr;
    QNetworkReply *m_updateReply = nullptr;
    bool m_updateCancelledByUser = false;
    bool m_updateIsCover = false; // true = o toast/download atual é do Cover Creator, não auto-update do Qenna
    QString m_updateDownloadUrl;
    QString m_updateVersion;
    MainMenuDialog *mainMenuDialog = nullptr;
    BackgroundWidget *backgroundWidget = nullptr;
    FindBar *findBar = nullptr;
    GlobalSearchPanel *globalSearchPanel = nullptr;
    QString markerEditId; // GUID em edição (vazio = aplicar novo)
    QString markerHoverId; // GUID hover atual (pra evitar reabrir)
    int markerPendingStart = -1; // seleção capturada antes do popup abrir
    int markerPendingEnd = -1;
    QWidget *editorContainer;
    QWidget *editorColumn;
    QScrollArea *editorScroll = nullptr;
    QScrollBar *externalScrollBar = nullptr;
    QWidget *toolbarHolder;
    QToolButton *selBoldBtn;
    QToolButton *selItalicBtn;
    QToolButton *selUnderlineBtn;
    QToolButton *selStrikeBtn;
    QString projectRoot;
    QString baseWindowTitle;
    QTimer *sceneDetectTimer;
    QString sceneDetectKey;
    QTimer *m_autoNavTimer = nullptr;
    int m_autoNavDir = 0;
    bool m_autoNavEnabled = true;
    bool m_autoNavCooldown = false;
    bool m_sliderHeld = false;
    int m_wheelOverscroll = 0;
    AutoNavHint *m_autoNavHint = nullptr;
    QTimer *m_autoNavProgressTimer = nullptr;
    int m_autoNavProgressMs = 0;
    QString m_autoNavTargetTitle;
    QTimer *detectionTimer = nullptr;
    QTimer *detectionBatchTimer = nullptr;
    // Timer irmão do de presença, mas dedicado à detecção de diálogos —
    // roda sozinho, sem entrar no DetectionScanState incremental acima.
    QTimer *dialogueDetectionTimer = nullptr;
    // true enquanto rescanAllChapterDialogues está rodando — evita disparar
    // um segundo scan em cima do primeiro se o usuário clicar de novo.
    bool dialogueScanRunning = false;
    QWidget *m_dialogueScanToast = nullptr;
    QLabel *m_dialogueScanToastLabel = nullptr;
    QProgressBar *m_dialogueScanToastBar = nullptr;
    class PresencePopup *presencePopup = nullptr;
    bool detectionEnabled = true;
    bool detectionMarkAll = false;
    QSet<QString> detectionAutoIds;
    QSet<QString> detectionNeverIds;
    QSet<QString> detectionRejectedKeys;
    // Estado do scan incremental (válido enquanto detectionBatchTimer estiver ativo)
    struct DetectionScanState {
        QString plainText;
        QString lowerText;
        QString docKey;
        QList<Element> elements;
        QSet<QString> alreadyPresent;
        QSet<QString> autoIds;
        QSet<QString> neverIds;
        QSet<QString> rejectedKeys;
        bool markAll = false;
        int wordCount = 0;
        int idx = 0;
        ScanResult result;
    };
    std::unique_ptr<DetectionScanState> m_scanState;
    QStringList availableFontFamilies;
    QString currentFontFamily;
    qreal currentFontSize;
    int currentLineHeight;
    bool firstLineIndentEnabled;
    int paragraphSpacingBefore;
    int paragraphSpacingAfter;
    bool focusModeEnabled;
    // Enquanto o Ctrl está segurado ("ver os links"), o foco é suspenso para não
    // cobrir o realce das menções (uma seleção de foco que contém a da menção a
    // engole no render do Qt). Solto o Ctrl, o foco volta.
    bool refHighlightActive = false;
    bool readModeEnabled = false;
    QWidget *readModeHotTop = nullptr;   // faixa de hover no topo (filho de this)
    QWidget *readModeHotLeft = nullptr;  // faixa de hover à esquerda (filho do container)
    int savedHolderHeight = 0;
    QColor baseTextColor;
    QTextCursor selectedImageCursor;
    QMap<QString, QList<QTextEdit::ExtraSelection>> editorSelectionLayers;
};

#endif
