#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QColor>
#include <QMainWindow>
#include <QString>
#include <QTextCursor>

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
class QToolButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void setAvailableFontFamilies(const QStringList &families);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupEditor();
    void setupToolbar();
    void applyEditorStyle();
    void applyTextColor();

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
    void applySpellLanguageFromModel();
    void positionWordCountPanel();
    void positionSidePanels();
    bool confirmDiscardOrSave();
    bool loadProjectFrom(const QString& root, QString* errorOut = nullptr);
    void rememberLastProject(const QString& root);
    QString loadLastProjectPath() const;
    void rememberLastDocFor(const QString& root);
    void restoreLastDocFor(const QString& root);
    void applyProjectRoot(const QString& root);

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
    SelectionPopup *selectionPopup;
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
    QString currentFontFamily;
    int currentFontSize;
    int currentLineHeight;
    bool firstLineIndentEnabled;
    int paragraphSpacingBefore;
    int paragraphSpacingAfter;
    bool focusModeEnabled;
    QColor baseTextColor;
    QTextCursor selectedImageCursor;
};

#endif
