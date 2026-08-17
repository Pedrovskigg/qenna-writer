#pragma once

#include "ConstrutorStore.h"

#include <QWidget>

class TerritorioStore;
class WorldContentEditor;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QScrollArea;
class QSlider;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class SystemItemDelegate;

// Navegação/edição de Sistemas do Construtor — sempre embutida no Criador de
// Mundos como widget filho comum (nunca janela própria; "coexiste"
// literalmente com o resto da ferramenta). NÃO tem editor de conteúdo
// próprio: usa o WorldContentEditor compartilhado que o Criador de Mundos já
// injeta pros nós de Território — Território e Sistema são a mesma função,
// editados no mesmo lugar, só a árvore de navegação à esquerda muda.
class ConstrutorWindow : public QWidget {
    Q_OBJECT
public:
    // `editor` é o WorldContentEditor COMPARTILHADO com o resto do Criador
    // de Mundos — este widget nunca cria o seu próprio.
    explicit ConstrutorWindow(ConstrutorStore* store, WorldContentEditor* editor,
                              QWidget* parent = nullptr);

    void setStore(ConstrutorStore* store);

    // Filtra a lista de sistemas por Território (vazio = mostra todos,
    // sistemas tagueados a outros territórios ficam ocultos; sistema sem
    // tag nenhuma = global, sempre aparece).
    void setTerritoryFilter(const QString& territorioId);

    // Território(s) do Criador de Mundos, pra alimentar o botão de tag na
    // ficha do sistema ("pertence a qual território?"). Opcional — sem isso
    // o botão fica oculto e todo sistema segue implicitamente global.
    void setTerritorioStore(TerritorioStore* store);

    // Navega direto pra um sistema/nó — usado pelo Ctrl+clique numa menção @
    // que aponta pro Construtor (ver refActivated em MainWindow.cpp). Se
    // nodeId vier vazio, abre só o resumo do sistema.
    void openNode(const QString& systemId, const QString& nodeId);

    // Liga/desliga a reação ao editor compartilhado — o Criador de Mundos
    // chama isso pra arbitrar qual lado (Território ou Construtor) "possui"
    // o editor no momento, evitando que os dois tentem salvar o mesmo
    // conteúdo em lugares diferentes. Quando false, este widget ignora
    // contentChanged() do editor e não escreve nada na store.
    void setEditorActive(bool active);
    // Salva o conteúdo atual do editor compartilhado nesta store, mas só se
    // este widget for o dono corrente (no-op caso contrário) — chamado pelo
    // host ao fechar a janela, pra não perder edição pendente sem saber de
    // qual lado ela é.
    void flushPendingContent();

signals:
    // Clique num card da seção "Menções no projeto" — pede pra abrir a
    // origem (capítulo/cena/gaveta) no editor principal.
    void openMentionInEditorRequested(const ConstrutorStore::Mention& mention);
    // Emitido sempre que este widget está prestes a assumir o editor
    // compartilhado (usuário selecionou sistema/nó aqui) — o host
    // (TerritorioWindow) escuta pra ceder a posse e não sobrescrever.
    void editorOwnershipRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void applyTheme();
    void onStoreChanged();
    void onSystemSelected();
    void onSystemNameEdited(const QString& name);
    void onSliderChanged(int index);
    void onTreeSelectionChanged();
    void onTreeItemChanged(QTreeWidgetItem* item, int column);
    void onEditorContentChanged();
    void onNewSystem();
    void onDeleteSystem();
    void onAddRule();
    void onAddSection();
    void onAddChild(ConstrutorStore::NodeType type);
    void onDeleteNode();
    void onTreeContextMenu(const QPoint& pos);
    void onSearchTextChanged(const QString& text);

private:
    void buildUi();
    void rebuildTerritoryMenu();
    void rebuildSystemsList();
    // sizeHint() padrão do QListWidget vazio ainda reserva um vão grande
    // mesmo sem itens (o cap fixo de 210px sozinho não bastava — sobrava
    // espaço morto entre a busca e o "+ Novo sistema"). Ajusta a altura de
    // fato ao conteúdo: linhas * altura da linha, com teto de 210px.
    void updateSystemsListHeight();
    void loadSystem(const QString& id);
    void rebuildTree();
    void populateTreeNode(QTreeWidgetItem* parent, const ConstrutorStore::Node& node);
    void updateSliderDisplay(int index);
    // Nenhum sistema aberto — desabilita o editor com uma mensagem explicando
    // que é preciso selecionar/criar um sistema (distinta da mensagem de
    // "sistema aberto, sem nó selecionado" usada pelo resumo do sistema).
    void showNoSystemOpenState();
    void saveCurrentNodeContent();
    QString selectedSystemId() const;
    QString selectedNodeId() const;

    // Seção "Menções no projeto" — trechos salvos via "Salvar como menção ao
    // sistema..." (mini-toolbar de seleção do editor principal). Painel
    // flutuante ancorado ao canto superior direito da janela (mesmo padrão
    // visual de RefMenuPanel/PensarioPanel), aberto/fechado pelo botão "@"
    // no cabeçalho. rebuildMentionsPanel() é o ponto único de verdade:
    // decide o filtro (sistema inteiro vs. nó selecionado) e repopula os
    // cards.
    void rebuildMentionsPanel();
    QWidget* buildMentionCard(const ConstrutorStore::Mention& mention, QWidget* parent);
    void anchorMentionsPanel();

    // Busca global entre sistemas e nós (por nome) — navega direto pro
    // resultado ao clicar, sem precisar abrir sistema por sistema.
    void selectSystemAndNode(const QString& systemId, const QString& nodeId);

    // Assume o editor compartilhado pra este widget (emite
    // editorOwnershipRequested, liga onEditorContentChanged).
    void claimEditor();

    ConstrutorStore* m_store = nullptr;
    WorldContentEditor* m_editor = nullptr;
    bool m_editorActive = false;

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
    QPushButton* m_mentionsToggleBtn = nullptr;
    // Tag de Território(s) — só visível se m_territorioStore estiver setado.
    QPushButton* m_territoryBtn    = nullptr;
    TerritorioStore* m_territorioStore = nullptr;
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

    // ── Seção "Menções no projeto" — overlay flutuante no canto superior
    // direito, toggle "@" no cabeçalho do painel.
    QWidget*     m_mentionsPanel      = nullptr;
    QLabel*      m_mentionsTitleLabel = nullptr;
    QToolButton* m_mentionsCloseBtn   = nullptr;
    QScrollArea* m_mentionsScroll     = nullptr;
    QWidget*     m_mentionsColumn     = nullptr;
    QVBoxLayout* m_mentionsLay        = nullptr;

    QString m_currentSystemId;
    QString m_currentNodeId;
    QString m_territoryFilter;
    bool    m_rebuilding  = false;
};
