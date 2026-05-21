#pragma once

#include <QPoint>
#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class QHBoxLayout;
class QScrollArea;
class ProjectModel;
class ElementsStore;

class DrawerListPanel : public QWidget {
    Q_OBJECT
public:
    enum SortMode { SortCreation, SortAlpha, SortRole };

    explicit DrawerListPanel(ProjectModel* model, QWidget* parent = nullptr);

    void setElementsStore(ElementsStore* store);

    void openDrawer(const QString& drawerKey, const QString& folderId = QString());
    void closePanel();
    bool isPanelOpen() const { return !m_currentKey.isEmpty(); }
    QString currentDrawerKey() const { return m_currentKey; }
    bool heightIsUserSet() const { return m_heightUserSet; }
    int desiredHeight() const { return m_desiredHeight; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

signals:
    void itemActivated(QString drawerKey, QString itemId);
    void newItemRequested(QString drawerKey, QString folderId);
    void newFolderRequested(QString drawerKey, QString parentFolderId);
    void panelClosed();
    void editItemRequested(QString drawerKey, QString itemId);
    void deleteItemRequested(QString drawerKey, QString itemId);
    void openInRefMenuRequested(QString drawerKey, QString itemId);
    void addElementRequested(QString drawerKey, QString itemId);
    void removeElementRequested(QString drawerKey, QString itemId);
    void panelWidthChanged();
    void panelHeightChanged();

private slots:
    void onDrawersChanged();

private:
    void rebuildContents();
    void rebuildFolderStrip();
    void updateActionBar();
    void updateSortButton();
    void updateViewButton();
    void enterFolder(const QString& folderId);
    void goUpOneLevel();
    void updateBreadcrumb();
    void showItemContextMenu(const QString& itemId, const QPoint& globalPos);
    void showFolderContextMenu(const QString& folderId, const QPoint& globalPos);

    QWidget* makeRow(const QString& label, bool isFolder, const QString& id, const QString& role);
    QWidget* makeElementCard(const QString& itemId, const QString& title, const QString& role, const QString& elementId);
    QWidget* makeEmptyState();
    QString folderTitle(const QString& folderId) const;
    QStringList ancestorFolderIds(const QString& folderId) const;

    QString createButtonLabel() const;
    QString currentDrawerColor() const;
    bool currentDrawerIsCharacter() const;
    bool currentDrawerIsElement() const;

    ProjectModel* m_model;
    ElementsStore* m_elementsStore = nullptr;
    QString m_currentKey;
    QString m_currentFolderId;
    QLabel* m_titleLabel;
    QVBoxLayout* m_listLayout;
    QScrollArea* m_scroll;

    // Header / action bar
    QToolButton* m_pinBtn;
    QToolButton* m_viewBtn;
    QToolButton* m_sortBtn;
    QPushButton* m_createBtn;
    QPushButton* m_folderBtn;
    QWidget* m_folderStrip = nullptr;
    QHBoxLayout* m_folderStripLayout = nullptr;

    // Estado de UI por painel (não persiste entre projetos)
    SortMode m_sortMode = SortCreation;
    bool m_sortAscending = true;
    bool m_gridView = true;
    bool m_pinned = false;

    // Drag state (foto -> reorder/mover)
    QPoint m_dragStartPos;
    QString m_pressedItemId;
    QWidget* m_dropIndicator = nullptr;

    // Resize state (handles direita / inferior)
    QWidget* m_resizeHandle = nullptr;       // borda direita (largura)
    QWidget* m_resizeHandleBottom = nullptr; // borda inferior (altura)
    bool m_resizing = false;                 // resize horizontal em curso
    bool m_resizingV = false;                // resize vertical em curso
    int m_resizeStartGlobalX = 0;
    int m_resizeStartGlobalY = 0;
    int m_resizeStartWidth = 0;
    int m_resizeStartHeight = 0;
    bool m_heightUserSet = false;            // se true, MainWindow respeita altura escolhida
    int m_desiredHeight = 0;                 // altura escolhida pelo user (preserva entre show/hide)
    void saveStoredWidth();
    int loadStoredWidth() const;
    void saveStoredHeight();
    int loadStoredHeight() const;
    void startItemDrag(QWidget* photoLabel, const QString& itemId);
    void clearItemDropIndicator();
    void showItemDropIndicatorAt(QWidget* targetCard, bool before);
    void installDropTargetOnCard(QWidget* card, const QString& itemId);
    void installDropTargetOnFolderChip(QWidget* chip, const QString& folderId);
};
