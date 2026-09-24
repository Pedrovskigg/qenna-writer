#pragma once

#include "ConstrutorStore.h"
#include "MemoriesStore.h"
#include "TerritorioStore.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QPixmap>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QWidget>

class QBoxLayout;
class QFrame;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QScrollArea;
class QSplitter;
class QStackedWidget;
class QTabBar;
class QTimer;
class RefUsageMap;
class RefDividers;
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
    // Estilo de layout do painel — preferência visual GLOBAL (QSettings), como
    // o estilo do contador. Os dados, a busca, os fixados e os recentes são os
    // mesmos em todos; muda só onde cada coisa mora.
    //   Classic  navegação em cima, preview embaixo (o original)
    //   Columns  navegação numa coluna retrátil ao lado do preview
    //   Rail     igual Columns, mas recolhido vira um trilho de ícones (PADRÃO)
    //   Overlay  preview cheio; a navegação desliza por cima quando chamada
    //   Miller   fontes → itens → documento, em três colunas
    //   Reader   quase só o documento; navegar é buscar pelo nome
    //   Dock     o trilho deitado no pé do painel; as listas sobem como folha
    //   Gallery  o projeto como mural de cartões com foto; o documento sobe
    //            por cima do mural
    //   Book     livro aberto: sumário na página da esquerda, documento na direita
    //   Binder   fichário: as gavetas viram divisórias coloridas na borda
    // Abas, Comparar e Mapa de Uso não são estilos: valem em todos.
    enum class Layout { Classic, Columns, Rail, Overlay, Miller, Reader, Dock, Gallery, Book, Binder };

    RefMenuPanel(ProjectModel* model, EditorHost* host, DocCache* cache,
                 ElementsStore* elements, QWidget* parent = nullptr);

    void setProjectRoot(const QString& root);
    // Store do Construtor — habilita "Construtor" como fonte navegável no
    // picker de gaveta (leitura apenas; edição continua na ConstrutorWindow).
    void setConstrutorStore(ConstrutorStore* store) { m_construtorStore = store; }
    void setTerritorioStore(TerritorioStore* store) { m_territorioStore = store; }
    // Família da fonte de escrita do editor — usada na preview (ex.: fichas, que
    // não trazem font-family embutida no html).
    void setEditorFontFamily(const QString& family);

    void togglePanel();
    void openPanel();
    // Largura da chrome do app colada na direita da janela principal (a
    // TopToolbar quando esta no modo lateral). Mesmo padrao de
    // PensarioPanel/StatsPanel/AIChatPanel. Zero = nada na direita.
    void setRightInset(int px) { m_rightInset = px; }
    void closePanel();
    void openForDrawer(const QString& drawerKey, const QString& itemId = QString());
    void openForChapter(const QString& manuscriptId, const QString& chapterId);
    void openForScene(const QString& manuscriptId, const QString& chapterId, int sceneIndex);

    // Busca dentro do RefMenu (Ctrl+Alt+F). Abre o painel se fechado e foca o
    // campo de busca. Filtra a navegação por nome em todas as fontes
    // (manuscritos, capítulos, cenas, gavetas) ao mesmo tempo.
    void openSearch();
    void closeSearch();

    // Documento aberto no editor principal, no formato de chave do
    // ElementsStore (elementDocKeyForChapter/ForScene). O MainWindow avisa a
    // cada troca de capítulo/cena; é daqui que sai a seção "Nesta cena" da
    // tela inicial — os elementos presentes já estão computados no
    // ElementsStore, não recalculamos nada.
    void setCurrentDocKey(const QString& docKey);

    // Find inline no preview (Alt+F). Abre uma FindBar atrelada ao
    // QTextBrowser do preview, com navegação anterior/próximo e contador.
    void openPreviewFind();

    // Chave de cache do item selecionado no momento (para LRU pinning).
    // Formato DocCache: "ch:<ms>:<ch>", "it:<id>", ou "" se nada selecionado.
    QString selectedCacheKey() const;

    // Abre a fonte de uma memória no preview do RefMenu (capítulo/cena/gaveta),
    // marcando o trecho. Chamado pelo Pensário via MainWindow.
    void openMemoryInRef(const MemoriesStore::Memory& mem);

signals:
    void geometryChanged();
    void selectedKeyChanged(QString cacheKey);

public slots:
    void refresh();

protected:
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onToggleNav();
    void onTogglePin();
    void onCycleFontSize();
    void onToggleVisualMode();
    void onCloseClicked();
    void onToggleSearch();
    void onSearchQueryChanged(const QString& q);
    void onToggleEdit(bool on);
    void applyTheme();

private:
    enum class SourceKind { Manuscript, Drawer, MarkersPlaceholder, TimelinesPlaceholder, WorldExplorer };
    enum class ResizeEdge { None, Left, Right, Top, Bottom, TL, TR, BL, BR };

    // Onde a navegação está. Substitui os dois seletores suspensos que
    // disputavam o comando (Manuscritos ▾ / Gaveta ▾) e não mostravam estado:
    //
    //   Home    tela inicial — Nesta cena, Fixados, Recentes, Percorrer
    //   Section uma fonte aberta (um manuscrito, uma gaveta, Mundos, Grupos)
    //   Search  busca ativa; atravessa todas as fontes e ignora a seção
    //
    // A trilha no topo mostra o caminho e volta com um clique.
    enum class NavMode { Home, Section, Search };

    void layoutResizeHandles();

    // --- estilos de layout ---
    // A navegação flutuante (gaveta do Overlay, lista do Trilho, dropdown do
    // Leitor) é o MESMO m_navPane, reparentado pra dentro de m_floatHost —
    // uma navegação só, construída pelas mesmas funções em todos os estilos.
    // Sheet = lista subindo da Doca; DocSheet = o DOCUMENTO (m_previewPane)
    // subindo por cima do mural da Galeria — o único tipo que flutua o preview
    // em vez da navegação.
    // Divider = a lista de uma divisória do Fichário, entrando pela direita.
    enum class FloatKind { None, Drawer, Flyout, Dropdown, Sheet, DocSheet, Divider };
    static QString layoutId(Layout l);
    static Layout layoutFromId(const QString& id);
    static int defaultWidthFor(Layout l);
    QString geometryKeyFor(Layout l) const;
    QString navHiddenKeyFor(Layout l) const;
    void setLayoutStyle(Layout l);
    void applyLayout();          // reorganiza os widgets pro estilo atual
    void applySplitSizes();
    void rebuildLayoutMenu();
    void updateToggleNavButton();
    int navSplitIndex() const;   // posição do m_navPane no splitter
    bool navPaneShown() const;   // navegação visível (coluna ou flutuante)
    // Filtro da tela inicial: "" tudo; "home" só Nesta cena/Fixados/Recentes;
    // "pinned"/"recent" uma seção; "ms:<id>"/"dr:<key>"/"world" uma fonte.
    QString homeFilter() const;
    FloatKind floatKindForLayout() const;
    void showFloat(FloatKind kind, const QString& filter);
    void hideFloat();
    void layoutFloat();
    void rebuildRail();
    void rebuildMillerSources();
    void rebuildReaderChips();
    void clampToScreen();
    QWidget* floatingWidget() const;   // o que a flutuante carrega agora

    // --- Livro aberto e Fichário ---
    void buildTocView();
    void rebuildDividers();
    QString sourceKeyOf(const QString& selectionKey) const;   // "dr:…", "ms:…", "world"

    // --- Mapa de Uso (global) ---
    void setUsageMap(bool on);
    void rebuildUsageMap();
    QString usageMapManuscript() const;

    // --- Galeria ---
    void rebuildGalleryBar();
    void buildGalleryView();
    QString gallerySource();           // resolve "" pro melhor ponto de partida

    // --- Página de documento ---
    // A principal é a "viva" (segue a navegação, edita, busca); a lateral do
    // Comparar é uma segunda instância, só leitura. As duas saem do mesmo
    // buildDocView/renderDoc.
    struct DocView {
        QWidget* wrap = nullptr;
        QWidget* bar = nullptr;          // faixa do Comparar ("abre aqui")
        QLabel* barLabel = nullptr;
        QLabel* title = nullptr;
        QLabel* role = nullptr;
        QScrollArea* imgScroll = nullptr;
        QWidget* imgHost = nullptr;
        QVBoxLayout* imgLay = nullptr;
        QList<QPixmap> pix;
        QList<QLabel*> labels;
        QTextBrowser* browser = nullptr;
        QLabel* placeholder = nullptr;
        QLabel* folio = nullptr;         // "— 3 —" no pé da página (Livro aberto)
    };
    void buildDocView(DocView& v, QWidget* parent);
    void renderDoc(DocView& v, const QString& key);
    void rescaleDoc(DocView& v);
    void applyDocFont(DocView& v);

    // --- Abas (globais) ---
    void syncTabs(const QString& key);  // chamado a cada troca de seleção
    void rebuildTabBar();
    void openInNewTab(const QString& key);
    void closeTab(int index);
    void pruneTabs();                   // tira abas de documentos apagados

    // --- Comparar (global) ---
    void setCompare(bool on);
    void openBeside(const QString& key);
    void activateSidePane();            // a lateral vira a viva (troca os papéis)
    void placeCompareViews();

    // Quem está marcado no documento aberto (seção "Nesta cena", trilho e
    // fileira do Leitor). key = ficha na gaveta, vazia se não houver ficha.
    struct ScenePerson { QString key; QString id; QString name; QString role; QString image; };
    QList<ScenePerson> scenePeople() const;

    void buildUi();
    void applyMainStyleSheet();
    // Filtro por território (M7) — repopula o menu; a aplicação em si
    // (esmaecer cards) acontece dentro de buildDrawerView() a cada rebuild.
    void rebuildTerritorioFilterMenu();
    void rebuildNavBody();

    // --- navegação nova ---
    void buildHomeView();      // Nesta cena / Fixados / Recentes / Percorrer
    void rebuildCrumbs();      // trilha no lugar dos seletores suspensos
    void goHome();
    void enterSection(SourceKind kind, const QString& key = QString());
    // Rótulo curto da seção corrente, pro meio da trilha.
    QString currentSectionLabel() const;

    // Descreve uma chave de seleção ("it:…", "ch:…", "ctr:…", "lug:…") o
    // bastante pra desenhar uma linha na tela inicial sem abrir o documento.
    // Devolve false se a chave não existe mais (item apagado) — aí a entrada
    // é descartada de fixados/recentes em silêncio.
    struct KeyInfo {
        QString name;
        QString meta;      // "Capítulo 3", "12 documentos"…
        QString role;      // papel do personagem, quando houver
        QString imagePath; // miniatura, quando houver
        QString sectionLabel;
    };
    bool describeKey(const QString& selectionKey, KeyInfo* out) const;
    // Chave do documento aberto no editor, deduzida do EditorHost.
    QString editorDocKey() const;

    // Linha de navegação padrão: miniatura (ou bolinha por tipo) + nome +
    // meta + estrela de fixar. Usada pelas seções da tela inicial e pela busca.
    QWidget* makeNavRow(const QString& selectionKey, const KeyInfo& info, bool withPin,
                        int fontPx = 13);

    // Fixados e recentes — persistidos em QSettings por projeto.
    void loadPinsAndRecents();
    void savePinsAndRecents() const;
    void addRecent(const QString& selectionKey);
    void togglePin(const QString& selectionKey);
    bool isPinned(const QString& selectionKey) const;

    void buildManuscriptsView();
    void buildDrawerView();
    void buildGroupsView();
    void buildWorldExplorerView(); // territórios + sistemas do Criador de Mundos, unificados
    void buildSearchAllView();
    // Achado recursivo de nó por id — usados pelo preview, pela edição
    // in-place e pela busca global, pra não duplicar a mesma varredura.
    const ConstrutorStore::Node* findConstrutorNode(const ConstrutorStore::System* sys, const QString& nodeId) const;
    const TerritorioStore::Node* findTerritorioNode(const TerritorioStore::Territorio* ter, const QString& nodeId) const;
    void buildPlaceholderView(const QString& title, const QString& subtitle);
    void highlightInPreview(const QString& query);            // "Ctrl+F" no preview
    void rebuildPreview();
    void rescalePreviewImages(); // reaplica os pixmaps às novas dimensões do host (resize do painel)
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
    // Chave de cache (DocCache) editável para m_selectedKey, ou vazio se a
    // seleção atual não pode ser editada no RefMenu (cena individual,
    // placeholders, doc aberto no editor principal etc.).
    QString editableCacheKey() const;
    void updateEditAvailability();
    void commitEdit(); // salva m_preview->toHtml() na DocCache se m_editing
    void changeSelectedKey(const QString& key); // atribui + emite selectedKeyChanged
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
    ConstrutorStore* m_construtorStore = nullptr;
    QString m_currentConstrutorSystemId; // drill-down: sistema aberto na view do Construtor (vazio = lista de sistemas)
    TerritorioStore* m_territorioStore = nullptr;
    QString m_currentLugarTerritorioId; // drill-down: território aberto na view de Lugares
    // Filtro por território (M7) — só se aplica à view de Drawer visual;
    // esmaece (não esconde) cards de item cuja origem/local atual não bate.
    QString m_territorioFilterId;
    QString m_projectRoot;

    // Estado lógico
    NavMode m_navMode = NavMode::Home;
    QStringList m_pinnedKeys;   // fixados pelo usuário, no topo pra sempre
    QStringList m_recentKeys;   // últimos abertos, mais novo primeiro
    QString m_currentDocKey;    // doc aberto no editor → "Nesta cena"
    // Galhos abertos na árvore de PERCORRER ("ms:<id>", "ch x:<id>", "dr:<key>",
    // "world"). Clicar alterna; navegar não troca mais a tela de lugar.
    QSet<QString> m_expanded;
    SourceKind m_sourceKind = SourceKind::Manuscript;
    QString m_currentManuscriptId;
    QString m_currentDrawerKey;
    QString m_currentGroupId;
    QString m_currentFolderId;
    QString m_selectedKey;
    bool m_visualMode = true;
    bool m_pinned = false;
    bool m_editing = false;
    QString m_editingKey; // chave DocCache do doc em edição (vazio = nenhum)
    bool m_navHidden = false;
    // Layout
    Layout m_layout = Layout::Rail;
    bool m_navRight = false;       // Columns/Trilho: navegação do lado direito
    bool m_autoCollapsed = false;  // painel estreito demais pra coluna
    int m_navColW = 250;           // largura da coluna de navegação (horizontal)
    QString m_gallerySource;       // "scene", "ms:<id>", "dr:<key>", "world"
    QHash<QString, int> m_wordCache;       // palavras por capítulo (sumário do Livro)
    int m_bookColW = 320;                  // largura do sumário no Livro aberto
    // Mapa de Uso
    bool m_mapOn = false;
    QString m_mapFilter = QStringLiteral("all");   // "all" | "people" | "things"
    QString m_mapMsId;                     // manuscrito escolhido ("" = o do editor)
    QString m_mapRow;                      // linha destacada
    QHash<QString, QString> m_excerptCache; // trechos dos capítulos na Galeria
    QTimer* m_galleryRelayout = nullptr;    // refaz o mural quando a largura muda
    // Abas: cada uma guarda a chave do documento ("" = aba nova, vazia).
    QStringList m_tabs;
    int m_tabIdx = 0;
    bool m_syncingTabs = false;
    QStringList m_tabBarKeys;      // o que a QTabBar mostra agora
    // Comparar: a página lateral segura um documento enquanto a viva navega.
    bool m_compare = false;
    int m_livePane = 0;            // 0 = viva à esquerda
    QString m_sideKey;
    QString m_millerSource = QStringLiteral("home");
    FloatKind m_floatKind = FloatKind::None;
    QString m_floatFilter;
    int m_previewFontPt = 13;
    int m_rightInset = 0;
    void nudgeClearOfChrome();
    QString m_editorFontFamily;   // família da fonte de escrita (preview de fichas)

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

    // trilha (substituiu a antiga tabs row com os dois seletores suspensos)
    QWidget* m_crumbsRow = nullptr;
    QHBoxLayout* m_crumbsLay = nullptr;
    // O modo visual (cards com foto) e o filtro por território continuam, mas
    // agora moram na barra da seção, não numa fileira fixa de abas.
    QToolButton* m_viewModeBtn = nullptr;
    QToolButton* m_territorioFilterBtn = nullptr;
    QMenu* m_territorioFilterMenu = nullptr;

    // Divisor arrastavel entre a arvore de navegacao e o preview.
    QSplitter* m_bodySplit = nullptr;

    // Containers dos estilos. m_navPane = [busca?] + m_navScroll;
    // m_previewPane = [trilha?] + m_previewWrap. applyLayout() move a busca e a
    // trilha entre eles e o m_frameLay conforme o estilo.
    QWidget* m_body = nullptr;
    QHBoxLayout* m_bodyLay = nullptr;
    QWidget* m_navPane = nullptr;
    QVBoxLayout* m_navPaneLay = nullptr;
    QWidget* m_previewPane = nullptr;
    QVBoxLayout* m_previewPaneLay = nullptr;
    QToolButton* m_layoutBtn = nullptr;
    QMenu* m_layoutMenu = nullptr;
    QToolButton* m_edgeBtn = nullptr;       // lingueta da coluna recolhida
    QScrollArea* m_rail = nullptr;          // trilho de ícones
    QWidget* m_railInner = nullptr;
    QBoxLayout* m_railLay = nullptr;       // vertical no Trilho, deitado na Doca
    QScrollArea* m_millerCol = nullptr;     // 1ª coluna do Miller (fontes)
    QWidget* m_millerInner = nullptr;
    QVBoxLayout* m_millerLay = nullptr;
    QWidget* m_readerBar = nullptr;         // busca + fileira do Leitor
    QVBoxLayout* m_readerBarLay = nullptr;
    QScrollArea* m_readerChipsScroll = nullptr; // rola, pra não impor largura mínima
    QWidget* m_readerChips = nullptr;
    QHBoxLayout* m_readerChipsLay = nullptr;
    QScrollArea* m_galleryBar = nullptr;   // filtros do mural (Galeria)
    QWidget* m_galleryChips = nullptr;
    QHBoxLayout* m_galleryChipsLay = nullptr;
    QToolButton* m_backBtn = nullptr;      // "‹ Voltar ao mural" na trilha
    QWidget* m_tabRow = nullptr;
    QTabBar* m_tabBar = nullptr;
    QToolButton* m_tabAddBtn = nullptr;
    QToolButton* m_compareBtn = nullptr;
    QSplitter* m_cmpSplit = nullptr;       // [viva, lateral] no Comparar
    DocView m_main;
    DocView m_side;
    QToolButton* m_mapBtn = nullptr;
    QWidget* m_mapPanel = nullptr;
    QScrollArea* m_mapScroll = nullptr;
    RefUsageMap* m_map = nullptr;
    QToolButton* m_mapMsBtn = nullptr;
    QHBoxLayout* m_mapBarLay = nullptr;
    QWidget* m_rings = nullptr;            // argolas do Fichário
    RefDividers* m_dividers = nullptr;     // divisórias do Fichário
    QWidget* m_floatBand = nullptr;        // faixa colorida no topo da divisória aberta
    QFrame* m_floatHost = nullptr;
    QVBoxLayout* m_floatLay = nullptr;
    QWidget* m_scrim = nullptr;

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
    QVBoxLayout* m_previewImagesLay = nullptr;
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
