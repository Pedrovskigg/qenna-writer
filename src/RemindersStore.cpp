#include "RemindersStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>

RemindersStore::RemindersStore(QObject* parent)
    : QObject(parent)
{
}

void RemindersStore::setProjectRoot(const QString& root)
{
    if (m_root == root) return;
    m_root = root;
    m_reminders.clear();
}

QString RemindersStore::sidecarPath() const
{
    if (m_root.isEmpty()) return QString();
    return QDir::cleanPath(m_root + QStringLiteral("/reminders.json"));
}

bool RemindersStore::load()
{
    m_reminders.clear();
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
        Reminder r;
        r.id          = o.value(QStringLiteral("id")).toString();
        r.text        = o.value(QStringLiteral("text")).toString();
        r.createdAt   = qint64(o.value(QStringLiteral("createdAt")).toDouble());
        r.dueAt       = qint64(o.value(QStringLiteral("dueAt")).toDouble());
        r.notify      = o.value(QStringLiteral("notify")).toBool();
        r.completedAt = qint64(o.value(QStringLiteral("completedAt")).toDouble());
        r.notifiedAt  = qint64(o.value(QStringLiteral("notifiedAt")).toDouble());
        if (r.id.isEmpty() || r.text.isEmpty()) continue;
        m_reminders.append(r);
    }
    emit changed();
    return true;
}

bool RemindersStore::save() const
{
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;

    QJsonArray arr;
    for (const Reminder& r : m_reminders) {
        QJsonObject o;
        o.insert(QStringLiteral("id"),          r.id);
        o.insert(QStringLiteral("text"),        r.text);
        o.insert(QStringLiteral("createdAt"),   double(r.createdAt));
        o.insert(QStringLiteral("dueAt"),       double(r.dueAt));
        o.insert(QStringLiteral("notify"),      r.notify);
        o.insert(QStringLiteral("completedAt"), double(r.completedAt));
        o.insert(QStringLiteral("notifiedAt"),  double(r.notifiedAt));
        arr.append(o);
    }

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return f.commit();
}

QVector<Reminder> RemindersStore::active() const
{
    QVector<Reminder> result;
    for (const Reminder& r : m_reminders)
        if (r.completedAt == 0) result.append(r);
    return result;
}

QVector<Reminder> RemindersStore::completed() const
{
    QVector<Reminder> result;
    for (const Reminder& r : m_reminders)
        if (r.completedAt != 0) result.append(r);
    return result;
}

bool RemindersStore::hasActive() const
{
    for (const Reminder& r : m_reminders)
        if (r.completedAt == 0) return true;
    return false;
}

QString RemindersStore::add(const QString& text, qint64 dueAt, bool notify)
{
    const QString t = text.trimmed();
    if (t.isEmpty()) return QString();
    Reminder r;
    r.id        = QUuid::createUuid().toString(QUuid::WithoutBraces);
    r.text      = t;
    r.createdAt = QDateTime::currentMSecsSinceEpoch();
    r.dueAt     = dueAt;
    r.notify    = notify;
    m_reminders.prepend(r);
    emit changed();
    return r.id;
}

bool RemindersStore::complete(const QString& id)
{
    for (Reminder& r : m_reminders) {
        if (r.id == id && r.completedAt == 0) {
            r.completedAt = QDateTime::currentMSecsSinceEpoch();
            emit changed();
            return true;
        }
    }
    return false;
}

bool RemindersStore::remove(const QString& id)
{
    const int before = m_reminders.size();
    m_reminders.erase(
        std::remove_if(m_reminders.begin(), m_reminders.end(),
                       [&](const Reminder& r) { return r.id == id; }),
        m_reminders.end());
    if (m_reminders.size() == before) return false;
    emit changed();
    return true;
}

bool RemindersStore::setDue(const QString& id, qint64 dueAt, bool notify)
{
    for (Reminder& r : m_reminders) {
        if (r.id == id) {
            r.dueAt      = dueAt;
            r.notify     = notify;
            r.notifiedAt = 0;
            emit changed();
            return true;
        }
    }
    return false;
}

bool RemindersStore::markNotified(const QString& id)
{
    for (Reminder& r : m_reminders) {
        if (r.id == id) {
            r.notifiedAt = QDateTime::currentMSecsSinceEpoch();
            emit changed();
            return true;
        }
    }
    return false;
}

void RemindersStore::clearCompleted()
{
    const int before = m_reminders.size();
    m_reminders.erase(
        std::remove_if(m_reminders.begin(), m_reminders.end(),
                       [](const Reminder& r) { return r.completedAt != 0; }),
        m_reminders.end());
    if (m_reminders.size() < before) emit changed();
}

const Reminder* RemindersStore::findById(const QString& id) const
{
    for (const Reminder& r : m_reminders)
        if (r.id == id) return &r;
    return nullptr;
}
