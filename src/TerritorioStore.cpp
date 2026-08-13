#include "TerritorioStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

// ── Helpers — JSON ────────────────────────────────────────────────────────────

static QJsonObject nodeToJson(const TerritorioStore::Node& node)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"),   node.id);
    o.insert(QStringLiteral("name"), node.name);
    o.insert(QStringLiteral("type"), node.type == TerritorioStore::NodeType::Doc
                                         ? QStringLiteral("doc")
                                         : QStringLiteral("folder"));
    if (!node.content.isEmpty())
        o.insert(QStringLiteral("content"), node.content);
    o.insert(QStringLiteral("updatedAt"), node.updatedAt);
    if (!node.children.isEmpty()) {
        QJsonArray children;
        for (const auto& c : node.children)
            children.append(nodeToJson(c));
        o.insert(QStringLiteral("children"), children);
    }
    return o;
}

static TerritorioStore::Node nodeFromJson(const QJsonObject& o)
{
    TerritorioStore::Node node;
    node.id      = o.value(QStringLiteral("id")).toString();
    node.name    = o.value(QStringLiteral("name")).toString();
    node.type    = o.value(QStringLiteral("type")).toString() == QLatin1String("doc")
                       ? TerritorioStore::NodeType::Doc
                       : TerritorioStore::NodeType::Folder;
    node.content = o.value(QStringLiteral("content")).toString();
    node.updatedAt = o.value(QStringLiteral("updatedAt")).toVariant().toLongLong();
    for (const auto& v : o.value(QStringLiteral("children")).toArray())
        node.children.append(nodeFromJson(v.toObject()));
    return node;
}

static QJsonObject mentionToJson(const TerritorioStore::Mention& m)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"),   m.id);
    o.insert(QStringLiteral("text"), m.text);
    if (!m.nodeId.isEmpty())       o.insert(QStringLiteral("nodeId"), m.nodeId);
    if (!m.sourceType.isEmpty())   o.insert(QStringLiteral("sourceType"), m.sourceType);
    if (!m.sourceLabel.isEmpty()) o.insert(QStringLiteral("sourceLabel"), m.sourceLabel);
    if (!m.chapterId.isEmpty())    o.insert(QStringLiteral("chapterId"), m.chapterId);
    if (m.sceneIndex >= 0)         o.insert(QStringLiteral("sceneIndex"), m.sceneIndex);
    if (!m.manuscriptId.isEmpty()) o.insert(QStringLiteral("manuscriptId"), m.manuscriptId);
    if (!m.itemId.isEmpty())       o.insert(QStringLiteral("itemId"), m.itemId);
    if (!m.category.isEmpty())     o.insert(QStringLiteral("category"), m.category);
    o.insert(QStringLiteral("createdAt"), m.createdAt);
    return o;
}

static TerritorioStore::Mention mentionFromJson(const QJsonObject& o)
{
    TerritorioStore::Mention m;
    m.id           = o.value(QStringLiteral("id")).toString();
    m.text         = o.value(QStringLiteral("text")).toString();
    m.nodeId       = o.value(QStringLiteral("nodeId")).toString();
    m.sourceType   = o.value(QStringLiteral("sourceType")).toString();
    m.sourceLabel  = o.value(QStringLiteral("sourceLabel")).toString();
    m.chapterId    = o.value(QStringLiteral("chapterId")).toString();
    m.sceneIndex   = o.value(QStringLiteral("sceneIndex")).toInt(-1);
    m.manuscriptId = o.value(QStringLiteral("manuscriptId")).toString();
    m.itemId       = o.value(QStringLiteral("itemId")).toString();
    m.category     = o.value(QStringLiteral("category")).toString();
    m.createdAt    = qint64(o.value(QStringLiteral("createdAt")).toDouble());
    return m;
}

static QJsonObject linkToJson(const TerritorioStore::TerritorioLink& l)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"),               l.id);
    o.insert(QStringLiteral("fromTerritorioId"), l.fromTerritorioId);
    o.insert(QStringLiteral("toTerritorioId"),   l.toTerritorioId);
    if (!l.color.isEmpty())      o.insert(QStringLiteral("color"), l.color);
    if (!l.docContent.isEmpty()) o.insert(QStringLiteral("docContent"), l.docContent);
    o.insert(QStringLiteral("createdAt"), l.createdAt);
    o.insert(QStringLiteral("updatedAt"), l.updatedAt);
    return o;
}

static TerritorioStore::TerritorioLink linkFromJson(const QJsonObject& o)
{
    TerritorioStore::TerritorioLink l;
    l.id               = o.value(QStringLiteral("id")).toString();
    l.fromTerritorioId = o.value(QStringLiteral("fromTerritorioId")).toString();
    l.toTerritorioId   = o.value(QStringLiteral("toTerritorioId")).toString();
    l.color            = o.value(QStringLiteral("color")).toString();
    l.docContent       = o.value(QStringLiteral("docContent")).toString();
    l.createdAt        = o.value(QStringLiteral("createdAt")).toVariant().toLongLong();
    l.updatedAt        = o.value(QStringLiteral("updatedAt")).toVariant().toLongLong();
    return l;
}

// ── TerritorioStore ───────────────────────────────────────────────────────────

TerritorioStore::TerritorioStore(QObject* parent)
    : QObject(parent)
{
}

void TerritorioStore::setProjectRoot(const QString& root)
{
    if (m_root == root) return;
    m_root = root;
    m_territorios.clear();
    m_links.clear();
}

QString TerritorioStore::sidecarPath() const
{
    if (m_root.isEmpty()) return {};
    return QDir::cleanPath(m_root + QStringLiteral("/territorios.json"));
}

bool TerritorioStore::load()
{
    m_territorios.clear();
    m_links.clear();
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;
    QFile f(path);
    if (!f.exists()) return true;
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;

    for (const auto& v : doc.object().value(QStringLiteral("territorios")).toArray()) {
        const QJsonObject o = v.toObject();
        Territorio t;
        t.id            = o.value(QStringLiteral("id")).toString();
        t.name          = o.value(QStringLiteral("name")).toString();
        t.content       = o.value(QStringLiteral("content")).toString();
        t.avatarDataUrl = o.value(QStringLiteral("avatarDataUrl")).toString();
        t.createdAt     = o.value(QStringLiteral("createdAt")).toVariant().toLongLong();
        t.updatedAt     = o.value(QStringLiteral("updatedAt")).toVariant().toLongLong();
        for (const auto& nv : o.value(QStringLiteral("nodes")).toArray())
            t.nodes.append(nodeFromJson(nv.toObject()));
        for (const auto& mv : o.value(QStringLiteral("mentions")).toArray())
            t.mentions.append(mentionFromJson(mv.toObject()));
        if (!t.id.isEmpty())
            m_territorios.append(std::move(t));
    }
    for (const auto& v : doc.object().value(QStringLiteral("links")).toArray()) {
        TerritorioLink l = linkFromJson(v.toObject());
        if (!l.id.isEmpty())
            m_links.append(std::move(l));
    }
    return true;
}

bool TerritorioStore::save() const
{
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;

    QJsonArray territorios;
    for (const Territorio& t : m_territorios) {
        QJsonObject o;
        o.insert(QStringLiteral("id"),        t.id);
        o.insert(QStringLiteral("name"),      t.name);
        if (!t.content.isEmpty())
            o.insert(QStringLiteral("content"), t.content);
        if (!t.avatarDataUrl.isEmpty())
            o.insert(QStringLiteral("avatarDataUrl"), t.avatarDataUrl);
        o.insert(QStringLiteral("createdAt"), t.createdAt);
        o.insert(QStringLiteral("updatedAt"), t.updatedAt);
        QJsonArray nodes;
        for (const Node& n : t.nodes)
            nodes.append(nodeToJson(n));
        o.insert(QStringLiteral("nodes"), nodes);
        QJsonArray mentions;
        for (const Mention& m : t.mentions)
            mentions.append(mentionToJson(m));
        o.insert(QStringLiteral("mentions"), mentions);
        territorios.append(o);
    }
    QJsonArray links;
    for (const TerritorioLink& l : m_links)
        links.append(linkToJson(l));

    QJsonObject root;
    root.insert(QStringLiteral("territorios"), territorios);
    root.insert(QStringLiteral("links"), links);

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return f.commit();
}

// ── Territorios CRUD ──────────────────────────────────────────────────────────

TerritorioStore::Territorio* TerritorioStore::findTerritorio(const QString& id)
{
    for (Territorio& t : m_territorios)
        if (t.id == id) return &t;
    return nullptr;
}

const TerritorioStore::Territorio* TerritorioStore::territorio(const QString& id) const
{
    for (const Territorio& t : m_territorios)
        if (t.id == id) return &t;
    return nullptr;
}

QString TerritorioStore::addTerritorio(const QString& name)
{
    Territorio t;
    t.id        = QUuid::createUuid().toString(QUuid::WithoutBraces);
    t.name      = name;
    t.createdAt = QDateTime::currentMSecsSinceEpoch();
    t.updatedAt = t.createdAt;
    m_territorios.append(std::move(t));
    save();
    emit changed();
    return m_territorios.last().id;
}

bool TerritorioStore::updateTerritorio(const QString& id, const QString& name)
{
    Territorio* t = findTerritorio(id);
    if (!t) return false;
    t->name      = name;
    t->updatedAt = QDateTime::currentMSecsSinceEpoch();
    save();
    emit changed();
    return true;
}

bool TerritorioStore::updateTerritorioContent(const QString& id, const QString& content)
{
    Territorio* t = findTerritorio(id);
    if (!t || t->content == content) return false;
    t->content   = content;
    t->updatedAt = QDateTime::currentMSecsSinceEpoch();
    save();
    emit changed();
    return true;
}

bool TerritorioStore::updateTerritorioAvatar(const QString& id, const QString& avatarDataUrl)
{
    Territorio* t = findTerritorio(id);
    if (!t) return false;
    t->avatarDataUrl = avatarDataUrl;
    t->updatedAt     = QDateTime::currentMSecsSinceEpoch();
    save();
    emit changed();
    return true;
}

bool TerritorioStore::removeTerritorio(const QString& id)
{
    const int before = m_territorios.size();
    m_territorios.erase(std::remove_if(m_territorios.begin(), m_territorios.end(),
                                       [&](const Territorio& t) { return t.id == id; }),
                        m_territorios.end());
    if (m_territorios.size() == before) return false;
    removeLinksForTerritorio(id);
    save();
    emit changed();
    return true;
}

// ── Nodes CRUD ────────────────────────────────────────────────────────────────

TerritorioStore::Node* TerritorioStore::findNode(QList<Node>& nodes, const QString& id)
{
    for (Node& n : nodes) {
        if (n.id == id) return &n;
        Node* c = findNode(n.children, id);
        if (c) return c;
    }
    return nullptr;
}

bool TerritorioStore::removeNodeRecursive(QList<Node>& nodes, const QString& id)
{
    for (int i = 0; i < nodes.size(); ++i) {
        if (nodes[i].id == id) {
            nodes.removeAt(i);
            return true;
        }
        if (removeNodeRecursive(nodes[i].children, id)) return true;
    }
    return false;
}

QString TerritorioStore::addNode(const QString& territorioId, const QString& parentNodeId,
                                  NodeType type, const QString& name)
{
    Territorio* t = findTerritorio(territorioId);
    if (!t) return {};

    Node newNode;
    newNode.id        = QUuid::createUuid().toString(QUuid::WithoutBraces);
    newNode.name      = name;
    newNode.type      = type;
    newNode.updatedAt = QDateTime::currentMSecsSinceEpoch();

    if (parentNodeId.isEmpty()) {
        t->nodes.append(newNode);
    } else {
        Node* parent = findNode(t->nodes, parentNodeId);
        if (!parent) return {};
        parent->children.append(newNode);
    }

    save();
    emit changed();
    return newNode.id;
}

bool TerritorioStore::updateNode(const QString& territorioId, const QString& nodeId,
                                  const QString& name, const QString& content)
{
    Territorio* t = findTerritorio(territorioId);
    if (!t) return false;
    Node* node = findNode(t->nodes, nodeId);
    if (!node) return false;
    node->name      = name;
    node->content   = content;
    node->updatedAt = QDateTime::currentMSecsSinceEpoch();
    save();
    emit changed();
    return true;
}

bool TerritorioStore::removeNode(const QString& territorioId, const QString& nodeId)
{
    Territorio* t = findTerritorio(territorioId);
    if (!t) return false;
    if (!removeNodeRecursive(t->nodes, nodeId)) return false;
    save();
    emit changed();
    return true;
}

// ── Mentions CRUD ─────────────────────────────────────────────────────────────

QString TerritorioStore::addMention(const QString& territorioId, const Mention& mention)
{
    Territorio* t = findTerritorio(territorioId);
    if (!t) return {};

    Mention m = mention;
    m.id        = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m.createdAt = QDateTime::currentMSecsSinceEpoch();
    t->mentions.append(std::move(m));

    save();
    emit changed();
    return t->mentions.last().id;
}

bool TerritorioStore::removeMention(const QString& territorioId, const QString& mentionId)
{
    Territorio* t = findTerritorio(territorioId);
    if (!t) return false;
    const int before = t->mentions.size();
    t->mentions.erase(std::remove_if(t->mentions.begin(), t->mentions.end(),
                                     [&](const Mention& m) { return m.id == mentionId; }),
                      t->mentions.end());
    if (t->mentions.size() == before) return false;
    save();
    emit changed();
    return true;
}

// ── Links CRUD ────────────────────────────────────────────────────────────────

const TerritorioStore::TerritorioLink* TerritorioStore::link(const QString& id) const
{
    for (const TerritorioLink& l : m_links)
        if (l.id == id) return &l;
    return nullptr;
}

const TerritorioStore::TerritorioLink* TerritorioStore::linkBetween(const QString& aId,
                                                                      const QString& bId) const
{
    for (const TerritorioLink& l : m_links) {
        if ((l.fromTerritorioId == aId && l.toTerritorioId == bId) ||
            (l.fromTerritorioId == bId && l.toTerritorioId == aId))
            return &l;
    }
    return nullptr;
}

QString TerritorioStore::addLink(const QString& fromTerritorioId, const QString& toTerritorioId,
                                  const QString& color)
{
    if (fromTerritorioId.isEmpty() || toTerritorioId.isEmpty() || fromTerritorioId == toTerritorioId)
        return {};
    if (linkBetween(fromTerritorioId, toTerritorioId))
        return {};

    TerritorioLink l;
    l.id               = QUuid::createUuid().toString(QUuid::WithoutBraces);
    l.fromTerritorioId = fromTerritorioId;
    l.toTerritorioId   = toTerritorioId;
    l.color            = color;
    l.createdAt        = QDateTime::currentMSecsSinceEpoch();
    l.updatedAt        = l.createdAt;
    m_links.append(std::move(l));

    save();
    emit changed();
    return m_links.last().id;
}

bool TerritorioStore::updateLinkContent(const QString& linkId, const QString& docContent)
{
    for (TerritorioLink& l : m_links) {
        if (l.id == linkId) {
            if (l.docContent == docContent) return false;
            l.docContent = docContent;
            l.updatedAt  = QDateTime::currentMSecsSinceEpoch();
            save();
            emit changed();
            return true;
        }
    }
    return false;
}

bool TerritorioStore::removeLink(const QString& linkId)
{
    const int before = m_links.size();
    m_links.erase(std::remove_if(m_links.begin(), m_links.end(),
                                 [&](const TerritorioLink& l) { return l.id == linkId; }),
                 m_links.end());
    if (m_links.size() == before) return false;
    save();
    emit changed();
    return true;
}

bool TerritorioStore::removeLinksForTerritorio(const QString& territorioId)
{
    const int before = m_links.size();
    m_links.erase(std::remove_if(m_links.begin(), m_links.end(),
                                 [&](const TerritorioLink& l) {
                                     return l.fromTerritorioId == territorioId ||
                                            l.toTerritorioId == territorioId;
                                 }),
                 m_links.end());
    if (m_links.size() == before) return false;
    save();
    emit changed();
    return true;
}
