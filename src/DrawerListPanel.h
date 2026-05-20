#pragma once

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

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void itemActivated(QString drawerKey, QString itemId);
    void newItemRequested(QString drawerKey, QString folderId);
    void newFolderRequested(QString drawerKey, QString parentFolderId);
    void panelClosed();

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
};
