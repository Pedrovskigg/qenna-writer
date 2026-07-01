#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

// Dados de worldbuilding estruturado — a store do Construtor.
// Persiste em `construtor.json` no root do projeto (sidecar, como NotesStore).
// Cada projeto tem sua própria lista de sistemas; cada sistema é uma árvore de
// nós (Regra ou Seção) de profundidade ilimitada.
class ConstrutorStore : public QObject {
    Q_OBJECT
public:
    enum class NodeType { Rule, Section };

    struct Node {
        QString id;
        QString name;
        NodeType type = NodeType::Section;
        QString content;
        QList<Node> children;
    };

    struct System {
        QString id;
        QString name;
        QString categoryId;
        int sliderIndex = 0;
        qint64 createdAt = 0;
        QList<Node> nodes;
    };

    struct CategoryWaypoint {
        QString label;
        QString tooltip;
        QStringList favors;   // "Favorece" — até 10, ordenados por relevância
        QStringList demands;  // "Exige" — até 10, ordenados por relevância
    };

    struct Category {
        QString id;
        QString displayName;
        QList<CategoryWaypoint> waypoints;
    };

    explicit ConstrutorStore(QObject* parent = nullptr);

    void setProjectRoot(const QString& root);
    bool load();
    bool save() const;

    const QList<System>& systems() const { return m_systems; }
    const System* system(const QString& id) const;

    // CRUD — sistemas
    QString addSystem(const QString& name, const QString& categoryId, int sliderIndex);
    bool updateSystem(const QString& id, const QString& name, int sliderIndex);
    bool removeSystem(const QString& id);

    // CRUD — nós (parentNodeId vazio = filho direto do sistema)
    QString addNode(const QString& systemId, const QString& parentNodeId,
                    NodeType type, const QString& name);
    bool updateNode(const QString& systemId, const QString& nodeId,
                    const QString& name, const QString& content);
    bool removeNode(const QString& systemId, const QString& nodeId);

    // Definições de categorias (estático, não varia por projeto)
    static const QList<Category>& categories();
    static const Category* categoryById(const QString& id);

signals:
    void changed();

private:
    System* findSystem(const QString& id);
    static Node* findNode(QList<Node>& nodes, const QString& id);
    static bool removeNodeRecursive(QList<Node>& nodes, const QString& id);
    QString sidecarPath() const;

    QString m_root;
    QList<System> m_systems;
};
