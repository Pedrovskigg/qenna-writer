#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

// Dados do Criador de Mundos — a store de Territórios (geografia do mundo
// fictício). Espelha o ConstrutorStore (árvore recursiva + menções + sidecar
// JSON), mas sem o conceito de categoria/slider (Território não tem "quão
// rígido é esse eixo").
// Persiste em `territorios.json` no root do projeto (sidecar, como
// ConstrutorStore/NotesStore).
class TerritorioStore : public QObject {
    Q_OBJECT
public:
    // Folder = pasta que pode ter pastas/docs dentro; Doc = documento em si.
    // (Vocabulário do Explorador de Território, não "Regra/Seção" do
    // Construtor — geografia não tem regra.)
    enum class NodeType { Folder, Doc };

    struct Node {
        QString id;
        QString name;
        NodeType type = NodeType::Folder;
        QString content;
        qint64 updatedAt = 0;
        QList<Node> children;
    };

    // Um trecho do manuscrito vinculado a este território (ou a um nó
    // específico dele) via "Salvar como menção ao Território..." na
    // mini-toolbar de seleção. Snapshot imutável — só cria/remove, nunca
    // edita (mesma filosofia de ConstrutorStore::Mention/MemoriesStore::Memory).
    struct Mention {
        QString id;
        QString text;          // snapshot do trecho selecionado
        QString nodeId;        // vazio = menção do território como um todo
        QString sourceType;    // "chapter" | "scene" | "drawer" | ""
        QString sourceLabel;   // rótulo pronto: "Capítulo 3 — Cena 2"
        QString chapterId;
        int     sceneIndex = -1;
        QString manuscriptId;
        QString itemId;
        QString category;      // livre: "guerra" | "criatura" | "batalha" | "" ...
        qint64  createdAt = 0;
    };

    struct Territorio {
        QString id;
        QString name;
        QString content;       // lore/resumo — dobra como corpo da página e
                                // como texto do tooltip (truncado) do Seletor
        QString avatarDataUrl; // imagem do círculo do Seletor (data URL)
        qint64  createdAt = 0;
        qint64  updatedAt = 0;
        QList<Node> nodes;
        QList<Mention> mentions;
    };

    // Vínculo entre dois Territórios (a linha do Seletor). Ao contrário do
    // CharacterBond/BondsLayer (que só gera um doc avulso pré-preenchido via
    // MainWindow::createDocFromBond), este carrega um documento PRÓPRIO e
    // vivo — reabre sempre o mesmo conteúdo ao clicar na linha.
    struct TerritorioLink {
        QString id;
        QString fromTerritorioId;
        QString toTerritorioId;
        QString color;
        QString docContent;    // HTML — guerra/aliança/história compartilhada
        qint64  createdAt = 0;
        qint64  updatedAt = 0;
    };

    explicit TerritorioStore(QObject* parent = nullptr);

    void setProjectRoot(const QString& root);
    bool load();
    bool save() const;

    const QList<Territorio>& territorios() const { return m_territorios; }
    const Territorio* territorio(const QString& id) const;

    const QList<TerritorioLink>& links() const { return m_links; }
    const TerritorioLink* link(const QString& id) const;
    const TerritorioLink* linkBetween(const QString& aId, const QString& bId) const;

    // CRUD — territórios
    QString addTerritorio(const QString& name);
    bool updateTerritorio(const QString& id, const QString& name);
    bool updateTerritorioContent(const QString& id, const QString& content);
    bool updateTerritorioAvatar(const QString& id, const QString& avatarDataUrl);
    bool removeTerritorio(const QString& id);

    // CRUD — nós (parentNodeId vazio = filho direto do território)
    QString addNode(const QString& territorioId, const QString& parentNodeId,
                    NodeType type, const QString& name);
    bool updateNode(const QString& territorioId, const QString& nodeId,
                    const QString& name, const QString& content);
    bool removeNode(const QString& territorioId, const QString& nodeId);

    // CRUD — menções (nodeId vazio = menção no nível do território)
    QString addMention(const QString& territorioId, const Mention& mention);
    bool removeMention(const QString& territorioId, const QString& mentionId);

    // CRUD — vínculos entre territórios
    QString addLink(const QString& fromTerritorioId, const QString& toTerritorioId,
                    const QString& color = QString());
    bool updateLinkContent(const QString& linkId, const QString& docContent);
    bool removeLink(const QString& linkId);
    // remove todos os vínculos que referenciam o território (usado ao excluir)
    bool removeLinksForTerritorio(const QString& territorioId);

signals:
    void changed();

private:
    Territorio* findTerritorio(const QString& id);
    static Node* findNode(QList<Node>& nodes, const QString& id);
    static bool removeNodeRecursive(QList<Node>& nodes, const QString& id);
    QString sidecarPath() const;

    QString m_root;
    QList<Territorio> m_territorios;
    QList<TerritorioLink> m_links;
};
