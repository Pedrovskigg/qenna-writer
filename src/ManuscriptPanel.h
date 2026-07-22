#pragma once

#include <QPoint>
#include <QString>
#include <QWidget>

class QComboBox;
class QVBoxLayout;
class QScrollArea;
class QToolButton;
class QPushButton;
class ProjectModel;
class DialogueStore;
class WordCounter;
class ElementsStore;

class ManuscriptPanel : public QWidget {
    Q_OBJECT
public:
    explicit ManuscriptPanel(ProjectModel* model, QWidget* parent = nullptr);

    void open();
    void closePanel();
    bool isPanelOpen() const { return isVisible(); }

    // Fontes pra barrinha de proporção diálogo/narração por capítulo — sem
    // as duas, a barra simplesmente não aparece (painel continua utilizável
    // durante a construção, antes do MainWindow acabar de ligar tudo).
    void setDialogueStore(DialogueStore* store);
    void setWordCounter(WordCounter* counter);
    // Só guardado pra repassar pro ChapterStatsDialog quando a pilula for
    // clicada — não precisa reagir a mudanças (a janela de estatísticas é
    // "burra", calcula tudo na abertura).
    void setElementsStore(ElementsStore* store) { m_elementsStore = store; }

signals:
    void chapterActivated(QString manuscriptId, QString chapterId);
    void sceneActivated(QString manuscriptId, QString chapterId, int sceneIndex);
    void newChapterRequested(QString manuscriptId);
    void newManuscriptRequested();
    void renameManuscriptRequested(QString manuscriptId);
    void deleteManuscriptRequested(QString manuscriptId);
    void panelClosed();
    // Context menus
    void renameChapterRequested(QString chapterId);
    void deleteChapterRequested(QString chapterId);
    void renameSceneRequested(QString chapterId, int sceneIndex);
    void deleteSceneRequested(QString chapterId, int sceneIndex);
    void createVariationRequested(QString chapterId, int sceneIndex);
    void openChapterInRefMenuRequested(QString manuscriptId, QString chapterId);
    void openSceneInRefMenuRequested(QString manuscriptId, QString chapterId, int sceneIndex);
    void elementsPresentRequested(QString manuscriptId, QString chapterId);
    void sceneElementsPresentRequested(QString manuscriptId, QString chapterId, int sceneIndex);
    // Clique na pilula de diálogo/narração de um capítulo — pede a janela de
    // estatísticas (ChapterStatsDialog), montada por quem escuta (MainWindow).
    // anchorGlobalPos: canto (borda direita do painel, topo da pilula clicada)
    // em coordenadas globais, pra a janela abrir ao lado do painel, alinhada
    // com o capítulo clicado, em vez de em cima do cursor.
    void chapterStatsRequested(QString manuscriptId, QString chapterId, QPoint anchorGlobalPos);
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
    void applyTheme();

private:
    void rebuildList();
    QString activeManuscriptId() const;
    void syncCombo();
    void showManuscriptContextMenu(const QString& manuscriptId, const QPoint& globalPos);
    void showChapterContextMenu(const QString& manuscriptId, const QString& chapterId, const QPoint& globalPos);
    void showSceneContextMenu(const QString& manuscriptId, const QString& chapterId, int sceneIndex, const QPoint& globalPos);
    void startChapterDrag(QWidget* sourceBtn, const QString& chapterId);
    void startSceneDrag(QWidget* sourceBtn, const QString& chapterId, int sceneIndex);
    void clearDropIndicator();
    void showDropIndicatorAt(QWidget* target, bool before);

    void applyHeaderStyles();

    ProjectModel* m_model;
    DialogueStore* m_dialogueStore = nullptr;
    WordCounter* m_wordCounter = nullptr;
    ElementsStore* m_elementsStore = nullptr;
    QComboBox* m_combo;
    QVBoxLayout* m_listLayout;
    QScrollArea* m_scroll;
    QWidget* m_header = nullptr;
    QPushButton* m_createChapterBtn = nullptr;
    QList<QToolButton*> m_headerIconBtns;

    // Drag state
    QPoint m_dragStartPos;
    QString m_pressedChapterId;   // se != "" e sceneIdx == -1 → drag de capítulo
    int m_pressedSceneIndex = -1; // se >= 0 → drag de cena (em m_pressedChapterId)
    QWidget* m_dropIndicator = nullptr;
};
