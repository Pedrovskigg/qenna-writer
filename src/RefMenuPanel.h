#pragma once

#include <QPoint>
#include <QSize>
#include <QString>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QScrollArea;
class QStackedWidget;
class QTextBrowser;
class QToolButton;
class QVBoxLayout;
class FindBar;
struct Drawer;
struct DrawerItem;
class ProjectModel;
class EditorHost;
class DocCache;
class ElementsStore;

// Painel flutuante de Referência. Reescrito 0.5.13 inspirado no RefPanels do Mira 1:
// header com drag handle, tabs (Manuscritos / Timeline / view mode / Drawer picker ▾),
// drilling com breadcrumb, modo visual (grid de cards com foto + role) para gavetas
// de personagens/cenários/objetos, preview com imagens extraídas no topo.
// Drag livre via ⠿; resize livre nas bordas; geometria persistida em QSettings.
class RefMenuPanel : public QWidget {
    Q_OBJECT
public:
    RefMenuPanel(ProjectModel* model, EditorHost* host, DocCache* cache,
                 ElementsStore* elements, QWidget* parent = nullptr);

    void setProjectRoot(const QString& root);

    void togglePanel();
    void openPanel();
    void closePanel();
    void openForDrawer(const QString& drawerKey, const QString& itemId = QString());

    // Busca dentro do RefMenu (Ctrl+Alt+F). Abre o painel se fechado e foca o
    // campo de busca. Filtra a navegação por nome em todas as fontes
    // (manuscritos, capítulos, cenas, gavetas) ao mesmo tempo.
    void openSearch();
    void closeSearch();

    // Find inline no preview (Alt+F). Abre uma FindBar atrelada ao
    // QTextBrowser do preview, com navegação anterior/próximo e contador.
    void openPreviewFind();

signals:
    void geometryChanged();

public slots:
    void refresh();

protected:
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onDrawerPickerClicked();
    void onToggleNav();
    void onTogglePin();
    void onCycleFontSize();
    void onToggleVisualMode();
    void onCloseClicked();
    void onToggleSearch();
    void onSearchQueryChanged(const QString& q);
    void applyTheme();

private:
    enum class SourceKind { Manuscript, Drawer, MarkersPlaceholder, TimelinesPlaceholder, ElementsPlaceholder, MemoriesPlaceholder };
    enum class ResizeEdge { None, Left, Right, Top, Bottom, TL, TR, BL, BR };

    void layoutResizeHandles();

    void buildUi();
    void applyMainStyleSheet();
    void rebuildTabs();
    void rebuildNavBody();
    void buildManuscriptsView();
    void buildDrawerView();
    void buildSearchAllView();
    void buildPlaceholderView(const QString& title, const QString& subtitle);
    void rebuildPreview();
    void applyNavVisibility();
    void enterManuscriptMode(const QString& manuscriptId = QString());
    void enterDrawerMode(const QString& drawerKey);
    void enterPlaceholderMode(SourceKind kind);
    void setSelected(const QString& selectionKey);
    void applyPreviewFont();
    bool matchesSearch(const QString& text) const;
    void positionPreviewFindBar();
    void setupCharacterDrawerVisualDefault();

    QString resolveDocHtml(const QString& key) const;
    void extractImagesFromHtml(const QString& html, QStringList* imagesOut, QString* restOut) const;
    QString resolveImageSrc(const QString& src) const;
    bool drawerIsVisual(const Drawer* d) const;
    QString roleOrLabelForItem(const DrawerItem& it) const;
    QString imageForItem(const DrawerItem& it) const; // data URL ou caminho local

    void loadGeometryFromSettings();
    void saveGeometryToSettings();
    void scheduleGeometrySave();

    ProjectModel* m_model;
    EditorHost* m_host;
    DocCache* m_cache;
    ElementsStore* m_elements;
    QString m_projectRoot;

    // Estado lógico
    SourceKind m_sourceKind = SourceKind::Manuscript;
    QString m_currentManuscriptId;
    QString m_currentDrawerKey;
    QString m_currentFolderId;
    QString m_selectedKey;
    bool m_visualMode = true;
    bool m_pinned = false;
    bool m_navHidden = false;
    int m_previewFontPt = 13;

    // UI - root
    QWidget* m_frame = nullptr;          // filho centralizado deixando 8px de borda
    QVBoxLayout* m_frameLay = nullptr;

    // header
    QWidget* m_header = nullptr;
    QToolButton* m_dragHandle = nullptr;
    QLabel* m_title = nullptr;
    QToolButton* m_toggleNavBtn = nullptr;
    QToolButton* m_searchBtn = nullptr;
    QToolButton* m_fontSizeBtn = nullptr;
    QToolButton* m_editBtn = nullptr;
    QToolButton* m_pinBtn = nullptr;
    QToolButton* m_closeBtn = nullptr;

    // Search inline. Aparece abaixo do header quando ativado via m_searchBtn
    // ou Ctrl+Alt+F. Filtra a nav em todas as fontes.
    QWidget* m_searchRow = nullptr;
    QLineEdit* m_searchInput = nullptr;
    QString m_searchQuery;

    // FindBar do preview (Alt+F). Flutua sobre o canto superior direito do
    // m_previewWrap; opera no QTextBrowser m_preview.
    FindBar* m_previewFind = nullptr;

    // tabs row
    QWidget* m_tabsRow = nullptr;
    QToolButton* m_msTabBtn = nullptr;
    QToolButton* m_timelineTabBtn = nullptr;
    QToolButton* m_viewModeBtn = nullptr;
    QToolButton* m_drawerPickerBtn = nullptr;

    // nav body
    QScrollArea* m_navScroll = nullptr;
    QWidget* m_navInner = nullptr;
    QVBoxLayout* m_navInnerLay = nullptr;

    // preview
    QWidget* m_previewWrap = nullptr;
    QLabel* m_previewTitle = nullptr;
    QLabel* m_previewRole = nullptr;
    QScrollArea* m_previewImagesScroll = nullptr;
    QWidget* m_previewImagesHost = nullptr;
    QHBoxLayout* m_previewImagesLay = nullptr;
    QTextBrowser* m_preview = nullptr;
    QLabel* m_previewPlaceholder = nullptr;

    // drag/resize
    bool m_dragging = false;
    bool m_resizing = false;
    QPoint m_dragOffset;       // parent-coords - widget pos
    QRect m_resizeStartGeom;   // geometria no momento do press
    QPoint m_resizeStartMouse; // global
    ResizeEdge m_activeEdge = ResizeEdge::None;

    // Resize handles (estilo DrawerListPanel): widgets dedicados em cima das
    // 4 bordas + 4 cantos, cada um com cursor próprio e hover visual.
    QWidget* m_hL = nullptr;
    QWidget* m_hR = nullptr;
    QWidget* m_hT = nullptr;
    QWidget* m_hB = nullptr;
    QWidget* m_hTL = nullptr;
    QWidget* m_hTR = nullptr;
    QWidget* m_hBL = nullptr;
    QWidget* m_hBR = nullptr;

    bool m_savePending = false;
};
