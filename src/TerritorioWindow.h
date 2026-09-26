#pragma once

#include "ConstrutorStore.h"
#include "TerritorioStore.h"

#include <QHash>
#include <QPointer>
#include <QWidget>

#include <functional>

class ElementsStore;
class EncSection;
class EncFormatBar;
class ProjectModel;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QScrollArea;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

// Criador de Mundos — a Enciclopédia.
//
// Duas metades na mesma janela, trocadas pela chave do topo:
//   - Lugares: os Territórios (TerritorioStore) — pastas, documentos,
//     vizinhos (vínculos), quem é de lá, o que aconteceu ali.
//   - Sistemas: o Construtor (ConstrutorStore) — magia, política, religião…,
//     com espectro, "Favorece / Exige" e regras/seções.
//
// Em qualquer uma, o item aberto é lido como um verbete corrido: o resumo e,
// em seguida, cada nó da árvore como uma seção (regras numeradas como
// artigos, "§" nas seções, pastas como capítulos). Cada trecho é editável no
// lugar (EncSection) e salva sozinho; a barra de formatação do topo age no
// trecho em foco. À esquerda, o sumário; à direita, a margem (espectro e
// menções do sistema) ou a ficha do lugar (vizinhos, gente daqui, sistemas,
// Timeline, menções).
class TerritorioWindow : public QWidget {
    Q_OBJECT
public:
    enum class Mode { Lugares, Sistemas };

    explicit TerritorioWindow(TerritorioStore* store, QWidget* parent = nullptr);

    void setStore(TerritorioStore* store);
    void setConstrutorStore(ConstrutorStore* store);

    // Fichas de personagem com Origem/Local atual apontando pro território
    // ("Gente daqui") e limpeza dessas referências ao excluir um território.
    void setProjectModel(ProjectModel* model) { m_projectModel = model; }
    // Foto dos personagens em "Gente daqui". Opcional: sem ela, inicial.
    void setElementsStore(ElementsStore* store) { m_elementsStore = store; }

    // Zera placeId de eventos da Timeline que apontam pro território
    // excluído — injetado pelo MainWindow, sem acoplar a TimelinePanel.
    using PlaceReferenceCleaner = std::function<void(const QString& territorioId)>;
    void setPlaceReferenceCleaner(PlaceReferenceCleaner fn) { m_placeReferenceCleaner = std::move(fn); }

    // "O que aconteceu aqui": rótulos dos eventos da Timeline marcados neste
    // território — injetado pelo MainWindow.
    using PlaceEventsProvider = std::function<QStringList(const QString& territorioId)>;
    void setPlaceEventsProvider(PlaceEventsProvider fn) { m_placeEventsProvider = std::move(fn); }

    Mode mode() const { return m_mode; }
    void setMode(Mode mode);

    // Navegação externa (Ctrl+clique numa menção @, botão Construtor…).
    // nodeId vazio = abre o verbete no topo.
    void openConstrutorNode(const QString& systemId, const QString& nodeId);
    void openNode(const QString& territorioId, const QString& nodeId);

signals:
    // Clique numa menção (de sistema ou de território): abrir a origem no
    // editor principal.
    void openMentionInEditorRequested(const ConstrutorStore::Mention& mention);
    // Clique numa pessoa em "Gente daqui": consultar a ficha.
    void openCharacterRequested(const QString& drawerKey, const QString& itemId);

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void applyTheme();
    void onTerritorioStoreChanged();
    void onConstrutorStoreChanged();

private:
    // O que está aberto no meio.
    enum class Kind { None, Territorio, Link, Sistema };

    void buildUi();
    void rebuildSidebar();
    void rebuildPage();       // verbete inteiro (título, resumo, seções)
    void rebuildMargin();     // coluna da direita
    void openTerritorio(const QString& id, const QString& nodeId = QString());
    void openLink(const QString& linkId);
    void openSistema(const QString& id, const QString& nodeId = QString());
    void showEmptyPage();
    void scrollToSection(const QString& nodeId);
    void flushAll();          // salva o que estiver pendente em qualquer trecho
    QString pageSignature() const; // estrutura do verbete aberto (ids, nomes, tipos)

    // Ações
    void newTerritorio();
    void newSistema();
    void addTerritorioNode(const QString& parentId, TerritorioStore::NodeType type);
    void addSistemaNode(const QString& parentId, ConstrutorStore::NodeType type);
    void deleteNode(const QString& nodeId);
    void territorioMenu(const QString& id, const QPoint& globalPos);
    void sistemaMenu(const QString& id, const QPoint& globalPos);
    void nodeMenu(const QString& nodeId, const QPoint& globalPos);
    void deleteTerritorio(const QString& id);
    void deleteSistema(const QString& id);
    void changeTerritorioImage(const QString& id);
    void runSearch(const QString& text);

    // Salvar um trecho do verbete.
    void saveSection(EncSection* section);
    void saveTitle(const QString& name);

    TerritorioStore* m_store = nullptr;
    ConstrutorStore* m_construtorStore = nullptr;
    ProjectModel* m_projectModel = nullptr;
    ElementsStore* m_elementsStore = nullptr;
    PlaceEventsProvider m_placeEventsProvider;
    PlaceReferenceCleaner m_placeReferenceCleaner;

    Mode m_mode = Mode::Lugares;
    Kind m_kind = Kind::None;
    QString m_currentId;      // território, vínculo ou sistema aberto
    QString m_lastTerritorioId, m_lastSistemaId; // pra voltar ao trocar de modo

    // Topo
    QPushButton* m_lugaresBtn = nullptr;
    QPushButton* m_sistemasBtn = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QListWidget* m_searchResults = nullptr;
    EncFormatBar* m_formatBar = nullptr;

    // Esquerda
    QTreeWidget* m_sidebar = nullptr;
    QPushButton* m_newBtn = nullptr;

    // Meio
    QScrollArea* m_pageScroll = nullptr;
    QWidget* m_page = nullptr;
    QVBoxLayout* m_pageLay = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QList<EncSection*> m_sections;
    QPointer<EncSection> m_focusedSection;
    QString m_pageSig;

    // Direita
    QScrollArea* m_marginScroll = nullptr;
    QWidget* m_margin = nullptr;

    bool m_selfWriting = false;   // a mudança na store veio daqui: não refazer a página
    bool m_rebuilding = false;
};
