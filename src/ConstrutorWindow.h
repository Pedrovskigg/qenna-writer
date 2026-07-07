#pragma once

#include "ConstrutorStore.h"

#include <QColor>
#include <QIcon>
#include <QList>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QWidget>

class ImageOverlay;
class QAction;
class QButtonGroup;
class QComboBox;
class QFontComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QScrollArea;
class QSlider;
class QTextEdit;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class SystemItemDelegate;

// Janela dedicada do Construtor — worldbuilding estruturado.
// Layout: painel esquerdo colapsável (sistemas + slider + árvore de nós)
// + editor rich text com toolbar de formatação.
class ConstrutorWindow : public QWidget {
    Q_OBJECT
public:
    explicit ConstrutorWindow(ConstrutorStore* store, QWidget* parent = nullptr);

    void setStore(ConstrutorStore* store);

    // Navega direto pra um sistema/nó — usado pelo Ctrl+clique numa menção @
    // que aponta pro Construtor (ver refActivated em MainWindow.cpp). Se
    // nodeId vier vazio, abre só o resumo do sistema.
    void openNode(const QString& systemId, const QString& nodeId);

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void applyTheme();
    void applyPageLayout();
    void onStoreChanged();
    void onSystemSelected();
    void onSystemNameEdited(const QString& name);
    void onSliderChanged(int index);
    void onTreeSelectionChanged();
    void onTreeItemChanged(QTreeWidgetItem* item, int column);
    void onNodeContentChanged();
    void onNewSystem();
    void onDeleteSystem();
    void onAddRule();
    void onAddSection();
    void onAddChild(ConstrutorStore::NodeType type);
    void onDeleteNode();
    void onTreeContextMenu(const QPoint& pos);
    void onSearchTextChanged(const QString& text);
    void onTogglePanel();
    void onCurrentCharFormatChanged(const QTextCharFormat& fmt);
    void onInsertImage();
    void setFocusMode(bool enabled);
    void updateFocusedBlock();

private:
    void buildUi();
    void rebuildSystemsList();
    void loadSystem(const QString& id);
    void rebuildTree();
    void populateTreeNode(QTreeWidgetItem* parent, const ConstrutorStore::Node& node);
    void updateSliderDisplay(int index);
    // Carrega HTML/plain text no editor, normaliza formatação global (indent/
    // entrelinha/margens) e sincroniza a toolbar — usado tanto pro conteúdo de
    // um nó quanto pro resumo do sistema (sem nó selecionado).
    void loadContentIntoEditor(const QString& content);
    // Nenhum sistema aberto — desabilita o editor com uma mensagem explicando
    // que é preciso selecionar/criar um sistema (distinta da mensagem de
    // "sistema aberto, sem nó selecionado" usada pelo resumo do sistema).
    void showNoSystemOpenState();
    void saveCurrentNodeContent();
    void updateToolbarState(const QTextCharFormat& fmt);
    QString selectedSystemId() const;
    QString selectedNodeId() const;

    // Timestamp de última edição — exibido no toolbar do editor de conteúdo,
    // reflete sys->updatedAt (resumo do sistema) ou node->updatedAt (nó).
    void updateLastEditedLabel(qint64 updatedAt);

    // Busca global entre sistemas e nós (por nome) — navega direto pro
    // resultado ao clicar, sem precisar abrir sistema por sistema.
    void selectSystemAndNode(const QString& systemId, const QString& nodeId);

    // Formatação global do editor de conteúdo (aplica ao documento inteiro,
    // não apenas à seleção — espelha o comportamento de fonte/tamanho/
    // alinhamento/espaçamento do editor principal)
    void buildSpacingMenu();
    void applyGlobalAlignment(Qt::Alignment align);
    void applyLineHeight(int percent);
    void applyParaSpaceBefore(int px);
    void applyParaSpaceAfter(int px);
    void updateLineHeightMenuChecks();

    // Focus Mode — esmaece todo o texto exceto o parágrafo com o cursor.
    // Mesmo mecanismo do editor principal (MainWindow::applyTextColor/
    // updateFocusedBlock), simplificado por não haver markers/@menções aqui.
    void applyFocusTextColor();

    // Overlay de tamanho de imagem (clica na imagem → mostra controles de
    // +/- largura) — mesmo mecanismo do editor principal (MainWindow::
    // findImageAt/showOverlayForImage/changeSelectedImageWidth), sem a parte
    // de alinhamento/frame flutuante, que o Construtor não usa.
    bool findImageAt(const QPoint& viewportPos, QTextCursor& imageCursor) const;
    void showOverlayForImage(const QTextCursor& imageCursor);
    void hideOverlay();
    void changeSelectedImageWidth(int delta);

    ConstrutorStore* m_store = nullptr;

    // ── Painel esquerdo colapsável ─────────────────────────────────────────────
    QWidget*            m_leftPanel    = nullptr;
    QFrame*             m_vsep         = nullptr;  // separador vertical
    QLineEdit*          m_searchEdit   = nullptr;
    QListWidget*        m_searchResultsList = nullptr;
    QListWidget*        m_systemsList  = nullptr;
    QPushButton*        m_newSystemBtn = nullptr;
    SystemItemDelegate* m_sysDelegate  = nullptr;

    // Detalhe do sistema (visível quando há sistema selecionado)
    QWidget*     m_sysDetail       = nullptr;
    QLineEdit*   m_systemNameEdit  = nullptr;
    QLabel*      m_categoryLabel   = nullptr;
    QPushButton* m_deleteSystemBtn = nullptr;
    QLabel*      m_waypointName    = nullptr;
    QSlider*     m_slider          = nullptr;
    QLabel*      m_waypointFirst   = nullptr;
    QLabel*      m_waypointLast    = nullptr;
    QLabel*      m_waypointTip     = nullptr;
    QLabel*      m_favorsList      = nullptr;
    QLabel*      m_demandsList     = nullptr;
    QPushButton* m_tradeoffExpandBtn = nullptr;
    bool         m_tradeoffExpanded  = false;
    QPushButton* m_addRuleBtn      = nullptr;
    QPushButton* m_addSectionBtn   = nullptr;
    QPushButton* m_deleteNodeBtn   = nullptr;
    QTreeWidget* m_tree            = nullptr;

    // ── Toolbar + editor (lado direito) ───────────────────────────────────────
    QPushButton*   m_togglePanelBtn = nullptr;
    QPushButton*   m_boldBtn        = nullptr;
    QPushButton*   m_italicBtn      = nullptr;
    QPushButton*   m_underlineBtn   = nullptr;
    QPushButton*   m_strikeBtn      = nullptr;
    QFontComboBox* m_fontCombo      = nullptr;
    QComboBox*     m_sizeCombo      = nullptr;
    QButtonGroup*  m_alignGroup     = nullptr;
    QPushButton*   m_alignLeftBtn   = nullptr;
    QPushButton*   m_alignCenterBtn = nullptr;
    QPushButton*   m_alignRightBtn  = nullptr;
    QPushButton*   m_indentBtn      = nullptr;
    QPushButton*   m_spacingBtn     = nullptr;
    QPushButton*   m_focusBtn       = nullptr;
    QIcon          m_focusOffIcon;
    QIcon          m_focusOnIcon;
    QPushButton*   m_insertImageBtn = nullptr;
    QLabel*        m_lastEditedLabel = nullptr;
    QScrollArea*   m_pageScroll     = nullptr;  // centraliza a "página" do editor
    QWidget*       m_pageColumn     = nullptr;  // largura/margens de EditorLayout::Manager
    QTextEdit*     m_contentEdit    = nullptr;
    ImageOverlay*  m_imageOverlay   = nullptr;
    QTextCursor    m_selectedImageCursor;  // imagem atualmente selecionada pelo overlay

    bool    m_focusModeEnabled        = false;
    bool    m_firstLineIndentEnabled  = false;  // preferência global, persiste em QSettings
    QColor  m_baseTextColor;  // Theme::editorTextColor() — base do dim do Focus Mode

    // Estado de espaçamento global (entrelinha + margens de parágrafo)
    QLabel*         m_paraBeforeLabel  = nullptr;
    QLabel*         m_paraAfterLabel   = nullptr;
    QList<QAction*> m_lineHeightActions;
    int             m_lineHeightPercent = 170;
    int             m_paraSpaceBefore   = 0;
    int             m_paraSpaceAfter    = 8;

    QString m_currentSystemId;
    QString m_currentNodeId;
    bool    m_rebuilding  = false;
    bool    m_updatingFmt = false;
    QTimer* m_saveTimer   = nullptr;
};
