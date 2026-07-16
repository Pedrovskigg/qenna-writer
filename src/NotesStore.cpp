#include "NotesStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include <algorithm>

NotesStore::NotesStore(QObject* parent)
    : QObject(parent)
{
}

void NotesStore::setProjectRoot(const QString& root)
{
    if (m_root == root) return;
    m_root = root;
    m_notes.clear();
}

QString NotesStore::sidecarPath() const
{
    if (m_root.isEmpty()) return QString();
    return QDir::cleanPath(m_root + QStringLiteral("/notes.json"));
}

bool NotesStore::load()
{
    m_notes.clear();
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;
    QFile f(path);
    if (!f.exists()) return true; // sem notas ainda, ok
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    for (const auto& v : doc.object().value(QStringLiteral("notes")).toArray()) {
        const QJsonObject o = v.toObject();
        Note n;
        n.id = o.value(QStringLiteral("id")).toString();
        n.color = o.value(QStringLiteral("color")).toString();
        n.title = o.value(QStringLiteral("title")).toString();
        n.text = o.value(QStringLiteral("text")).toString();
        n.createdAt = o.value(QStringLiteral("createdAt")).toVariant().toLongLong();
        if (n.id.isEmpty()) continue;
        m_notes.append(n);
    }
    return true;
}

bool NotesStore::save() const
{
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;

    QJsonArray arr;
    for (const Note& n : m_notes) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), n.id);
        o.insert(QStringLiteral("color"), n.color);
        if (!n.title.isEmpty()) o.insert(QStringLiteral("title"), n.title);
        o.insert(QStringLiteral("text"), n.text);
        if (n.createdAt > 0) o.insert(QStringLiteral("createdAt"), n.createdAt);
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("notes"), arr);

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return f.commit();
}

QString NotesStore::addNote(const QString& color, const QString& title, const QString& text)
{
    Note n;
    n.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    n.color = color;
    n.title = title;
    n.text = text;
    n.createdAt = QDateTime::currentMSecsSinceEpoch();
    m_notes.append(n);
    save();
    emit notesChanged();
    return n.id;
}

bool NotesStore::updateNote(const QString& id, const QString& color,
                            const QString& title, const QString& text)
{
    for (Note& n : m_notes) {
        if (n.id == id) {
            n.color = color;
            n.title = title;
            n.text = text;
            save();
            emit notesChanged();
            return true;
        }
    }
    return false;
}

bool NotesStore::removeNote(const QString& id)
{
    const int before = m_notes.size();
    m_notes.erase(std::remove_if(m_notes.begin(), m_notes.end(),
                                 [&](const Note& n) { return n.id == id; }),
                  m_notes.end());
    if (m_notes.size() == before) return false;
    save();
    emit notesChanged();
    return true;
}
