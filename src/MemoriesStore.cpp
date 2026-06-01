#include "MemoriesStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>

MemoriesStore::MemoriesStore(QObject* parent)
    : QObject(parent)
{
}

void MemoriesStore::setProjectRoot(const QString& root)
{
    if (m_root == root) return;
    m_root = root;
    m_memories.clear();
}

QString MemoriesStore::sidecarPath() const
{
    if (m_root.isEmpty()) return QString();
    return QDir::cleanPath(m_root + QStringLiteral("/memories.json"));
}

bool MemoriesStore::load()
{
    m_memories.clear();
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;
    QFile f(path);
    if (!f.exists()) {
        emit changed();
        return true;
    }
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) return false;

    for (const auto& v : doc.array()) {
        const QJsonObject o = v.toObject();
        Memory m;
        m.id           = o.value(QStringLiteral("id")).toString();
        m.name         = o.value(QStringLiteral("name")).toString();
        m.text         = o.value(QStringLiteral("text")).toString();
        m.targetType   = o.value(QStringLiteral("targetType")).toString();
        m.elementId    = o.value(QStringLiteral("elementId")).toString();
        m.sourceType   = o.value(QStringLiteral("sourceType")).toString();
        m.sourceLabel  = o.value(QStringLiteral("sourceLabel")).toString();
        m.chapterId    = o.value(QStringLiteral("chapterId")).toString();
        m.sceneIndex   = o.value(QStringLiteral("sceneIndex")).toInt(-1);
        m.manuscriptId = o.value(QStringLiteral("manuscriptId")).toString();
        m.drawerKey    = o.value(QStringLiteral("drawerKey")).toString();
        m.itemId       = o.value(QStringLiteral("itemId")).toString();
        m.createdAt    = qint64(o.value(QStringLiteral("createdAt")).toDouble());
        if (m.id.isEmpty() || m.text.isEmpty()) continue;
        if (m.targetType.isEmpty()) m.targetType = QStringLiteral("project");
        m_memories.append(m);
    }
    emit changed();
    return true;
}

bool MemoriesStore::save() const
{
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;

    QJsonArray arr;
    for (const Memory& m : m_memories) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), m.id);
        o.insert(QStringLiteral("name"), m.name);
        o.insert(QStringLiteral("text"), m.text);
        o.insert(QStringLiteral("targetType"), m.targetType);
        o.insert(QStringLiteral("elementId"), m.elementId);
        o.insert(QStringLiteral("sourceType"), m.sourceType);
        o.insert(QStringLiteral("sourceLabel"), m.sourceLabel);
        o.insert(QStringLiteral("chapterId"), m.chapterId);
        o.insert(QStringLiteral("sceneIndex"), m.sceneIndex);
        o.insert(QStringLiteral("manuscriptId"), m.manuscriptId);
        o.insert(QStringLiteral("drawerKey"), m.drawerKey);
        o.insert(QStringLiteral("itemId"), m.itemId);
        o.insert(QStringLiteral("createdAt"), double(m.createdAt));
        arr.append(o);
    }

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return f.commit();
}

QVector<MemoriesStore::Memory> MemoriesStore::projectMemories() const
{
    QVector<Memory> out;
    for (const Memory& m : m_memories)
        if (m.targetType != QStringLiteral("character")) out.append(m);
    std::sort(out.begin(), out.end(),
              [](const Memory& a, const Memory& b) { return a.createdAt > b.createdAt; });
    return out;
}

QVector<MemoriesStore::Memory> MemoriesStore::characterMemories(const QString& elementId) const
{
    QVector<Memory> out;
    if (elementId.isEmpty()) return out;
    for (const Memory& m : m_memories)
        if (m.targetType == QStringLiteral("character") && m.elementId == elementId)
            out.append(m);
    std::sort(out.begin(), out.end(),
              [](const Memory& a, const Memory& b) { return a.createdAt > b.createdAt; });
    return out;
}

QString MemoriesStore::add(const Memory& memIn)
{
    Memory m = memIn;
    if (m.text.trimmed().isEmpty()) return QString();
    if (m.id.isEmpty()) m.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (m.createdAt == 0) m.createdAt = QDateTime::currentMSecsSinceEpoch();
    if (m.targetType.isEmpty()) m.targetType = QStringLiteral("project");
    if (m.targetType != QStringLiteral("character")) m.elementId.clear();
    m_memories.append(m);
    emit changed();
    return m.id;
}

bool MemoriesStore::remove(const QString& id)
{
    const int before = m_memories.size();
    m_memories.erase(std::remove_if(m_memories.begin(), m_memories.end(),
                                    [&](const Memory& m) { return m.id == id; }),
                     m_memories.end());
    if (m_memories.size() == before) return false;
    emit changed();
    return true;
}
