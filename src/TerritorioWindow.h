#pragma once

#include "ConstrutorStore.h"
#include "TerritorioStore.h"

#include <QWidget>

#include <functional>

class ConstrutorWindow;
class QFrame;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class WorldContentEditor;

// Janela do Criador de Mundos (Fase 1 — fundação, sem mapa visual).
// Layout: Seletor de Território (círculos-avatar) + Explorador de Território
// (personagens + árvore de pastas/docs) à esquerda, editor de conteúdo
// (WorldContentEditor) ao centro. Painel do Construtor (M2) e vínculos entre
// territórios (M3) chegam em milestones seguintes.
class TerritorioWindow : public QWidget {
    Q_OBJECT
public:
    explicit TerritorioWindow(TerritorioStore* store, QWidget* parent = nullptr);

    void setStore(TerritorioStore* store);

    // Construtor absorvido — a ConstrutorWindow de verdade (não uma lista
    // reduzida) embutida como widget filho no painel direito, filtrada pelo
    // território selecionado (tagueado OU global). "Coexiste" literalmente:
    // é a mesma ferramenta, morando aqui em vez de numa janela separada.
    void setConstrutorStore(ConstrutorStore* store);

    // Navega direto pra um sistema/nó do Construtor embutido — usado pelo
    // Ctrl+clique numa menção @ que aponta pro Construtor. Ignora o filtro
    // de território corrente (mostra tudo) pra garantir que o alvo apareça
    // mesmo se pertencer a outro território.
    void openConstrutorNode(const QString& systemId, const QString& nodeId);

    // "O que aconteceu aqui" — quem hospeda (MainWindow) injeta uma função
    // que devolve rótulos de eventos da Timeline cujo placeId bate com o
    // território pedido. Filtro puro sobre dado já existente, sem duplicar
    // nada e sem acoplar este widget a TimelineTypes/TimelinePanel.
    using PlaceEventsProvider = std::function<QStringList(const QString& territorioId)>;
    void setPlaceEventsProvider(PlaceEventsProvider fn) { m_placeEventsProvider = std::move(fn); }

    // Navega direto pra um território/nó — usado pelo Ctrl+clique numa
    // menção @ que aponta pro Criador de Mundos (ver refActivated em
    // MainWindow.cpp, a partir do M5). Se nodeId vier vazio, abre só o
    // resumo do território.
    void openNode(const QString& territorioId, const QString& nodeId);

signals:
    // Repassado do ConstrutorWindow embutido — clique num card de menção do
    // Construtor pede pra abrir a origem (capítulo/cena/gaveta) no editor
    // principal. MainWindow escuta este sinal em vez de conectar direto no
    // ConstrutorWindow interno (que agora é um detalhe de implementação).
    void openMentionInEditorRequested(const ConstrutorStore::Mention& mention);

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void applyTheme();
    void onStoreChanged();
    void onNewTerritorio();
    void onTerritorioClicked(QListWidgetItem* item);
    void onTerritorioItemChanged(QListWidgetItem* item);
    void onTerritorioContextMenu(const QPoint& pos);
    void onTreeSelectionChanged();
    void onTreeItemChanged(QTreeWidgetItem* item, int column);
    void onTreeContextMenu(const QPoint& pos);
    void onAddFolder();
    void onAddDoc();
    void onDeleteNode();
    void onEditorContentChanged();
    void rebuildPlaceEvents();
    void rebuildMentions();
    void onToggleLeftPanel();
    void onToggleConstrutorPanel();

private:
    void buildUi();
    void rebuildSelector();
    void rebuildTree();
    void populateTreeNode(QTreeWidgetItem* parent, const TerritorioStore::Node& node);
    void loadTerritorio(const QString& id);
    void showNoTerritorioOpenState();
    void addChildNode(TerritorioStore::NodeType type);
    void updateLastEditedLabel(qint64 updatedAt);
    void saveCurrentContent();
    void selectTerritorioAndNode(const QString& territorioId, const QString& nodeId);

    // ── Vínculo entre Territórios (M3) ───────────────────────────────────────
    void loadLink(const QString& linkId);
    void repositionLinksOverlay();
    // Acha o vínculo cuja linha passa perto de `pos` (coords do viewport do
    // seletor), com folga de alguns pixels. nullptr se nenhum.
    const TerritorioStore::TerritorioLink* linkNear(const QPoint& pos, qreal threshold = 8.0) const;

    QString selectedTerritorioId() const;
    QString selectedNodeId() const;

    TerritorioStore* m_store = nullptr;

    QPushButton* m_toggleLeftBtn  = nullptr;
    QPushButton* m_toggleConstrutorBtn = nullptr;
    QWidget*     m_leftPanel      = nullptr;
    QFrame*      m_vsep1          = nullptr;
    QFrame*      m_vsep2          = nullptr;

    QListWidget* m_selector       = nullptr;
    QPushButton* m_newTerritorioBtn = nullptr;
    QWidget*     m_linksOverlay   = nullptr; // pinta as linhas de vínculo entre círculos

    QLabel*      m_charactersLabel = nullptr;
    QLabel*      m_placeEventsLabel = nullptr; // "O que aconteceu aqui" (eventos da Timeline)
    QLabel*      m_mentionsLabel    = nullptr; // menções salvas (categoria + trecho)
    PlaceEventsProvider m_placeEventsProvider;
    QPushButton* m_addFolderBtn    = nullptr;
    QPushButton* m_addDocBtn       = nullptr;
    QPushButton* m_deleteNodeBtn   = nullptr;
    QTreeWidget* m_tree            = nullptr;

    WorldContentEditor* m_editor = nullptr;

    // ── Construtor absorvido (embutido de verdade, não janela separada) ──────
    ConstrutorStore* m_construtorStore   = nullptr;
    QWidget*          m_construtorPanel   = nullptr; // container simples, hospeda m_embeddedConstrutor
    ConstrutorWindow*  m_embeddedConstrutor = nullptr;

    QString m_currentTerritorioId;
    QString m_currentNodeId;
    QString m_currentLinkId;
    QString m_hoveredLinkId; // vínculo sob o mouse no momento — hover visual + cursor
    bool    m_rebuilding = false;
    // Território e Construtor compartilham UM único editor (m_editor) — só
    // um lado por vez "possui" ele. true = Território é quem deve reagir a
    // WorldContentEditor::contentChanged (o Construtor embutido tem o
    // espelho disso via ConstrutorWindow::setEditorActive()).
    bool m_editorActiveHere = true;
};
