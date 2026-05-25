#include "GlossaryStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>

GlossaryStore::GlossaryStore(QObject* parent)
    : QObject(parent)
{
}

void GlossaryStore::setProjectRoot(const QString& root)
{
    if (m_root == root) return;
    m_root = root;
    m_entries.clear();
}

QString GlossaryStore::sidecarPath() const
{
    if (m_root.isEmpty()) return QString();
    return QDir::cleanPath(m_root + QStringLiteral("/glossary.json"));
}

bool GlossaryStore::load()
{
    m_entries.clear();
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
        Entry e;
        e.id = o.value(QStringLiteral("id")).toString();
        e.term = o.value(QStringLiteral("term")).toString().trimmed();
        e.definition = o.value(QStringLiteral("definition")).toString();
        e.addedAt = qint64(o.value(QStringLiteral("addedAt")).toDouble());
        if (e.id.isEmpty() || e.term.isEmpty()) continue;
        m_entries.append(e);
    }
    sortEntries();
    emit changed();
    return true;
}

bool GlossaryStore::save() const
{
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;

    QJsonArray arr;
    for (const Entry& e : m_entries) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), e.id);
        o.insert(QStringLiteral("term"), e.term);
        o.insert(QStringLiteral("definition"), e.definition);
        o.insert(QStringLiteral("addedAt"), double(e.addedAt));
        arr.append(o);
    }

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return f.commit();
}

QSet<QString> GlossaryStore::terms() const
{
    QSet<QString> s;
    for (const Entry& e : m_entries) s.insert(e.term.toLower());
    return s;
}

void GlossaryStore::sortEntries()
{
    std::sort(m_entries.begin(), m_entries.end(),
        [](const Entry& a, const Entry& b) {
            return a.term.localeAwareCompare(b.term) < 0;
        });
}

QString GlossaryStore::add(const QString& term, const QString& definition)
{
    const QString t = term.trimmed();
    if (t.isEmpty()) return QString();
    // Se o termo já existe (case-insensitive), atualiza a definição em vez de duplicar.
    for (Entry& e : m_entries) {
        if (e.term.compare(t, Qt::CaseInsensitive) == 0) {
            if (!definition.isEmpty()) e.definition = definition;
            emit changed();
            return e.id;
        }
    }
    Entry e;
    e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    e.term = t;
    e.definition = definition;
    e.addedAt = QDateTime::currentMSecsSinceEpoch();
    m_entries.append(e);
    sortEntries();
    emit changed();
    return e.id;
}

bool GlossaryStore::update(const QString& id, const QString& term, const QString& definition)
{
    for (Entry& e : m_entries) {
        if (e.id == id) {
            const QString t = term.trimmed();
            if (t.isEmpty()) return false;
            e.term = t;
            e.definition = definition;
            sortEntries();
            emit changed();
            return true;
        }
    }
    return false;
}

bool GlossaryStore::remove(const QString& id)
{
    const int before = m_entries.size();
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                                   [&](const Entry& e) { return e.id == id; }),
                    m_entries.end());
    if (m_entries.size() == before) return false;
    emit changed();
    return true;
}

GlossaryStore::Entry GlossaryStore::findById(const QString& id) const
{
    for (const Entry& e : m_entries) if (e.id == id) return e;
    return Entry{};
}

bool GlossaryStore::hasTerm(const QString& term) const
{
    const QString t = term.trimmed();
    if (t.isEmpty()) return false;
    for (const Entry& e : m_entries) {
        if (e.term.compare(t, Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}
