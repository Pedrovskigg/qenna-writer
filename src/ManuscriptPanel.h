#pragma once

#include <QPoint>
#include <QString>
#include <QWidget>

class QComboBox;
class QVBoxLayout;
class QScrollArea;
class QToolButton;
class ProjectModel;

class ManuscriptPanel : public QWidget {
    Q_OBJECT
public:
    explicit ManuscriptPanel(ProjectModel* model, QWidget* parent = nullptr);

    void open();
    void closePanel();
    bool isPanelOpen() const { return isVisible(); }

signals:
    void chapterActivated(QString manuscriptId, QString chapterId);
    void sceneActivated(QString manuscriptId, QString chapterId, int sceneIndex);
    void newChapterRequested(QString manuscriptId);
    void newManuscriptRequested();
    void panelClosed();
    // Context menus
    void renameChapterRequested(QString chapterId);
    void deleteChapterRequested(QString chapterId);
    void renameSceneRequested(QString chapterId, int sceneIndex);
    void deleteSceneRequested(QString chapterId, int sceneIndex);
    void createVariationRequested(QString chapterId, int sceneIndex);
    // Drag&drop reorder
    void reorderChapterRequested(QString chapterId, int targetIndex);
    void reorderSceneRequested(QString chapterId, int srcIndex, int targetIndex);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void dragEnterEvent(class QDragEnterEvent* event) override;
    void dragMoveEvent(class QDragMoveEvent* event) override;
    void dragLeaveEvent(class QDragLeaveEvent* event) override;
    void dropEvent(class QDropEvent* event) override;

private slots:
    void onManuscriptsChanged();
    void onChaptersChanged();
    void onComboChanged(int index);

private:
    void rebuildList();
    QString activeManuscriptId() const;
    void syncCombo();
    void showChapterContextMenu(const QString& chapterId, const QPoint& globalPos);
    void showSceneContextMenu(const QString& chapterId, int sceneIndex, const QPoint& globalPos);
    void startChapterDrag(QWidget* sourceBtn, const QString& chapterId);
    void startSceneDrag(QWidget* sourceBtn, const QString& chapterId, int sceneIndex);
    void clearDropIndicator();
    void showDropIndicatorAt(QWidget* target, bool before);

    ProjectModel* m_model;
    QComboBox* m_combo;
    QVBoxLayout* m_listLayout;
    QScrollArea* m_scroll;

    // Drag state
    QPoint m_dragStartPos;
    QString m_pressedChapterId;   // se != "" e sceneIdx == -1 → drag de capítulo
    int m_pressedSceneIndex = -1; // se >= 0 → drag de cena (em m_pressedChapterId)
    QWidget* m_dropIndicator = nullptr;
};
