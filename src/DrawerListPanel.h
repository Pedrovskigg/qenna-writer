#pragma once

#include <QFrame>
#include <QHash>
#include <QList>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <functional>

struct PresenceSceneEntry { int index = 0; QString title; };
struct PresenceChapterEntry { QString id; QString title; QList<PresenceSceneEntry> scenes; };
struct CharPresenceResult {
    int sceneCount    = 0;
    int chapterCount  = 0;
    QList<PresenceChapterEntry> chapters;
};

class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class QHBoxLayout;
class QScrollArea;
class ProjectModel;
class ElementsStore;
class BondsLayer;

class DrawerListPanel : public QWidget {
    Q_OBJECT
public:
    enum SortMode { SortCreation, SortAlpha, SortRole };

    explicit DrawerListPanel(ProjectModel* model, QWidget* parent = nullptr);

    void setElementsStore(ElementsStore* store);

    void openDrawer(const QString& drawerKey, const QString& folderId = QString());
    void openInConsistencyMode(const QString& drawerKey = QString());
    void closePanel();
    bool isPanelOpen() const { return !m_currentKey.isEmpty(); }
    QString currentDrawerKey() const { return m_currentKey; }
    bool isConsistencyMode() const { return m_consistencyMode; }
    bool heightIsUserSet() const { return m_heightUserSet; }
    int desiredHeight() const { return m_desiredHeight; }

    // Fornece dados de presença por cena/capítulo para cada personagem.
    // fn(charNames, outResults, outTotalScenes, outTotalChapters)
    using PresenceProvider = std::function<void(
        const QStringList&,
        QHash<QString, CharPresenceResult>*,
        int* /*totalScenes*/,
        int* /*totalChapters*/)>;
    void setPresenceProvider(PresenceProvider fn) { m_presenceProvider = std::move(fn); }

    // Doc key do documento aberto no editor (para destacar presença no doc atual).
    void setCurrentDocKey(const QString& key);

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
    // Vínculos: arrastar nome de personagem em outro card
    void bondCreateRequested(QString drawerKey, QString fromItemId, QString toItemId,
                             QPoint spawnGlobalPos);
    void bondViewRequested(QString drawerKey, QString bondId, QPoint spawnGlobalPos);
    // Consistência: solicitação de atualização de status/local de personagem
    void consistencyUpdateRequested(QString itemId, QString status, QString statusDetail, QString location);
    void consistencyModeChanged(bool on);

public slots:
    // Re-emitidos pelo MainWindow após characterBondsChanged: dão a chance
    // do painel atualizar a BondsLayer e expandir a largura interna.
    void refreshBonds();

private slots:
    void onDrawersChanged();
    void applyTheme();

private:
    void rebuildContents();
    void rebuildFolderStrip();
    void updateActionBar();
    void updateSortButton();
    void updateViewButton();
    void updateSizeButton();
    void updateConsistencyBtn();
    void refreshPresenceCache();
    void showStatusPicker(const QString& itemId, const QPoint& globalPos);
    void showLocationPicker(const QString& itemId, const QPoint& globalPos);
    void showPresenceDetail(const QString& charName, const CharPresenceResult& res, QPoint globalPos);
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

    void recomputeCardPositions();
    void positionBondsLayer();
    QPointF mapToListHost(const QPoint& globalPos) const;
    void updateBondHover(const QPoint& globalPos);
    void dispatchBondClick(const QPoint& globalPos);
    void startBondDrag(const QString& fromItemId);
    void updateBondDragPreview(const QPoint& globalPos);
    void finishBondDrag(const QPoint& globalPos);
    void cancelBondDrag();

    ProjectModel* m_model;
    ElementsStore* m_elementsStore = nullptr;
    QString m_currentKey;
    QString m_currentFolderId;
    QLabel* m_titleLabel;
    QVBoxLayout* m_listLayout;
    QScrollArea* m_scroll;
    QWidget* m_listHost = nullptr;

    // Vínculos
    BondsLayer* m_bondsLayer = nullptr;
    QHash<QString, QWidget*> m_cardByItemId;
    QString m_bondDragSource;            // itemId em drag de criação de bond
    QPointF m_bondDragFromLocal;         // anchor no listHost
    QPoint  m_bondDragStartGlobal;       // ponto inicial do press (pra threshold)
    bool m_bondDragActive = false;
    QWidget* m_bondDragWidget = nullptr; // widget que tem o grabMouse durante drag
    QLabel* m_bondHintLabel = nullptr;

    // Header / action bar
    QToolButton* m_pinBtn;
    QToolButton* m_viewBtn;
    QToolButton* m_sortBtn;
    QToolButton* m_sizeBtn;
    QPushButton* m_createBtn;
    QPushButton* m_folderBtn;
    QWidget* m_folderStrip = nullptr;
    QHBoxLayout* m_folderStripLayout = nullptr;

    // Estado de UI por painel (não persiste entre projetos)
    SortMode m_sortMode = SortCreation;
    bool m_sortAscending = true;
    bool m_gridView = true;
    bool m_pinned = false;
    int m_cardSizeIdx = 2; // 0=S 1=M 2=G

    // Modo consistência narrativa
    bool m_consistencyMode = false;
    QToolButton* m_consistencyBtn = nullptr;
    QHash<QString, CharPresenceResult> m_presenceResults;
    int m_totalScenes    = 0;
    int m_totalChapters  = 0;
    int m_presenceMode   = 0;  // 0 = cenas, 1 = capítulos
    PresenceProvider m_presenceProvider;
    QString m_currentDocKey;
    QFrame* m_presenceDetailPopup = nullptr;

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
