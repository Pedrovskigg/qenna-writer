#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QColor>
#include <QMainWindow>
#include <QString>
#include <QTextCursor>

class QLabel;
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

    void setFontFamily(const QString &family);
    void setFontSize(int pt);
    void setLineHeight(int percent);
    void setFirstLineIndent(bool enabled);
    void setFocusMode(bool enabled);
    void updateFocusedBlock();
    void onAddImageRequested();
    void onNewProjectRequested();
    void onOpenProjectRequested();
    void positionWordCountPanel();
    void positionSidePanels();
    bool confirmDiscardOrSave();
    bool loadProjectFrom(const QString& root, QString* errorOut = nullptr);
    void rememberLastProject(const QString& root);
    QString loadLastProjectPath() const;
    void applyProjectRoot(const QString& root);

    bool findImageAt(const QPoint &viewportPos, QTextCursor &imageCursor) const;
    int detectAlignmentForImage(const QTextCursor &imageCursor) const;
    void showOverlayForImage(const QTextCursor &imageCursor);
    void hideOverlay();
    void changeSelectedImageWidth(int delta);
    void changeSelectedImageAlignment(int alignment);
    void detectScenesForPending();

    QTextEdit *editor;
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
    QString projectRoot;
    QString baseWindowTitle;
    QTimer *sceneDetectTimer;
    QString sceneDetectKey;
    QString currentFontFamily;
    int currentFontSize;
    int currentLineHeight;
    bool firstLineIndentEnabled;
    bool focusModeEnabled;
    QColor baseTextColor;
    QTextCursor selectedImageCursor;
};

#endif
