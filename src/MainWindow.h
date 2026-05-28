#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "CharacterDetector.h"

#include <QColor>
#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QString>
#include <QTextCursor>
#include <QTextEdit>
#include <memory>

class QLabel;
class QScrollArea;
class QScrollBar;
class QTextEdit;
class QTimer;
class TopToolbar;
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
class MarkerPickPopup;
class MarkerHoverPopup;
class AmbienceManager;
class AmbiencePanel;
class GlossaryStore;
class GlossaryPanel;
class GlossaryAddPopup;
class MainMenuDialog;
class BackgroundWidget;
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

private:
    void setupEditor();
    void setupToolbar();
    void applyEditorStyle();
    void applyTextColor();
    void applyEditorFont();

    // Camadas de ExtraSelections do editor — focus mode (cor do bloco focado)
    // e find (highlight da busca local) precisam coexistir. Cada chamador
    // atualiza só a sua camada via setEditorSelectionsLayer e o helper aplica
    // a união no editor.
    void setEditorSelectionsLayer(const QString& layer, const QList<QTextEdit::ExtraSelection>& sels);
    void positionFindBar();
    void positionGlobalSearchPanel();

    void setFontFamily(const QString &family);
    void setFontSize(int pt);
    void setLineHeight(int percent);
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
    void onThemePanelRequested();
    void onThemeChanged();
    void onEditorLayoutChanged();
    void applyEditorLayout();
    void resizeEditorColumnToViewport();
    void applyPageShadow();
    void applyBackgroundFromTheme();
    void applySpellLanguageFromModel();
    void positionWordCountPanel();
    void positionSidePanels();
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

    void openMarkerPickerForSelection(bool withComment);
    void openMarkerPickerForEdit(const QString& markerId);
    void applyMarkerFromPicker(const QColor& color, const QString& comment);
    bool markerAtViewportPos(const QPoint& viewportPos, QString& outId,
                             QRect& outBoundsGlobal) const;

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

    bool findImageAt(const QPoint &viewportPos, QTextCursor &imageCursor) const;
    int detectAlignmentForImage(const QTextCursor &imageCursor) const;
    void showOverlayForImage(const QTextCursor &imageCursor);
    void hideOverlay();
    void changeSelectedImageWidth(int delta);
    void changeSelectedImageAlignment(int alignment);
    void detectScenesForPending();

    SpellEditor *editor;
    TopToolbar *toolbar;
    ImageOverlay *imageOverlay;
    LeftBar *leftBar;
    DrawerListPanel *drawerListPanel;
    ManuscriptPanel *manuscriptPanel;
    ProjectModel *projectModel;
    DocCache *docCache;
    EditorHost *editorHost;
    VariationBar *variationBar;
    ProjectSaver *projectSaver;
    WordCounter *wordCounter;
    WordCountPanel *wordCountPanel;
    RefMenuPanel *refMenuPanel;
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
    MarkerPickPopup *markerPickPopup = nullptr;
    MarkerHoverPopup *markerHoverPopup = nullptr;
    AmbienceManager *ambienceManager = nullptr;
    AmbiencePanel *ambiencePanel = nullptr;
    GlossaryStore *glossaryStore = nullptr;
    GlossaryPanel *glossaryPanel = nullptr;
    GlossaryAddPopup *glossaryAddPopup = nullptr;
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
    QTimer *detectionTimer = nullptr;
    QTimer *detectionBatchTimer = nullptr;
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
    QString currentFontFamily;
    int currentFontSize;
    int currentLineHeight;
    bool firstLineIndentEnabled;
    int paragraphSpacingBefore;
    int paragraphSpacingAfter;
    bool focusModeEnabled;
    bool readModeEnabled = false;
    QWidget *readModeHotTop = nullptr;   // faixa de hover no topo (filho de this)
    QWidget *readModeHotLeft = nullptr;  // faixa de hover à esquerda (filho do container)
    int savedHolderHeight = 0;
    QColor baseTextColor;
    QTextCursor selectedImageCursor;
    QMap<QString, QList<QTextEdit::ExtraSelection>> editorSelectionLayers;
};

#endif
