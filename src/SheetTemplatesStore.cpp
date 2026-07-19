#include "SheetTemplatesStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>

SheetTemplatesStore::SheetTemplatesStore(QObject* parent)
    : QObject(parent)
{
}

void SheetTemplatesStore::setProjectRoot(const QString& root)
{
    if (m_root == root) return;
    m_root = root;
    m_templates.clear();
}

QString SheetTemplatesStore::sidecarPath() const
{
    if (m_root.isEmpty()) return QString();
    return QDir::cleanPath(m_root + QStringLiteral("/sheet_templates.json"));
}

bool SheetTemplatesStore::load()
{
    m_templates.clear();
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
        SheetTemplate t;
        t.id        = o.value(QStringLiteral("id")).toString();
        t.name      = o.value(QStringLiteral("name")).toString();
        t.createdAt = qint64(o.value(QStringLiteral("createdAt")).toDouble());
        t.sheet     = ProjectModel::characterSheetFromJson(o.value(QStringLiteral("sheet")).toObject());
        if (t.id.isEmpty() || t.name.isEmpty()) continue;
        m_templates.append(t);
    }
    emit changed();
    return true;
}

bool SheetTemplatesStore::save() const
{
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;

    QJsonArray arr;
    for (const SheetTemplate& t : m_templates) {
        QJsonObject o;
        o.insert(QStringLiteral("id"),        t.id);
        o.insert(QStringLiteral("name"),      t.name);
        o.insert(QStringLiteral("createdAt"), double(t.createdAt));
        o.insert(QStringLiteral("sheet"),     ProjectModel::characterSheetToJson(t.sheet));
        arr.append(o);
    }

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return f.commit();
}

const SheetTemplate* SheetTemplatesStore::findById(const QString& id) const
{
    for (const SheetTemplate& t : m_templates)
        if (t.id == id) return &t;
    return nullptr;
}

QString SheetTemplatesStore::add(const QString& name, const CharacterSheet& sheet)
{
    const QString n = name.trimmed();
    if (n.isEmpty()) return QString();

    SheetTemplate t;
    t.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    t.name = n;
    t.createdAt = QDateTime::currentMSecsSinceEpoch();
    t.sheet = sheet;
    // IDs de campo novos — cópia independente da ficha de origem.
    for (auto& f : t.sheet.fields) f.id = ProjectModel::uid();
    m_templates.prepend(t);
    emit changed();
    return t.id;
}

bool SheetTemplatesStore::remove(const QString& id)
{
    const int before = m_templates.size();
    m_templates.erase(
        std::remove_if(m_templates.begin(), m_templates.end(),
                       [&](const SheetTemplate& t) { return t.id == id; }),
        m_templates.end());
    if (m_templates.size() == before) return false;
    emit changed();
    return true;
}

bool SheetTemplatesStore::rename(const QString& id, const QString& newName)
{
    const QString n = newName.trimmed();
    if (n.isEmpty()) return false;
    for (SheetTemplate& t : m_templates) {
        if (t.id == id) {
            t.name = n;
            emit changed();
            return true;
        }
    }
    return false;
}
