#include "MapPinsStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include <algorithm>

MapPinsStore::MapPinsStore(QObject* parent)
    : QObject(parent)
{
}

void MapPinsStore::setProjectRoot(const QString& root)
{
    if (m_root == root) return;
    m_root = root;
    m_pins.clear();
}

QString MapPinsStore::sidecarPath() const
{
    if (m_root.isEmpty()) return QString();
    return QDir::cleanPath(m_root + QStringLiteral("/map_pins.json"));
}

bool MapPinsStore::load()
{
    m_pins.clear();
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;
    QFile f(path);
    if (!f.exists()) return true;
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = f.readAll();
    f.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return false;
    for (const auto& v : doc.object().value(QStringLiteral("pins")).toArray()) {
        const QJsonObject o = v.toObject();
        Pin p;
        p.id = o.value(QStringLiteral("id")).toString();
        p.lon = o.value(QStringLiteral("lon")).toDouble();
        p.lat = o.value(QStringLiteral("lat")).toDouble();
        p.label = o.value(QStringLiteral("label")).toString();
        p.note = o.value(QStringLiteral("note")).toString();
        p.linkId = o.value(QStringLiteral("linkId")).toString();
        p.linkLabel = o.value(QStringLiteral("linkLabel")).toString();
        if (p.id.isEmpty()) continue;
        m_pins.append(p);
    }
    return true;
}

bool MapPinsStore::save() const
{
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;

    QJsonArray arr;
    for (const Pin& p : m_pins) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), p.id);
        o.insert(QStringLiteral("lon"), p.lon);
        o.insert(QStringLiteral("lat"), p.lat);
        o.insert(QStringLiteral("label"), p.label);
        if (!p.note.isEmpty()) o.insert(QStringLiteral("note"), p.note);
        if (!p.linkId.isEmpty()) {
            o.insert(QStringLiteral("linkId"), p.linkId);
            o.insert(QStringLiteral("linkLabel"), p.linkLabel);
        }
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("pins"), arr);

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return f.commit();
}

QString MapPinsStore::addPin(const Pin& p)
{
    Pin n = p;
    n.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_pins.append(n);
    save();
    emit pinsChanged();
    return n.id;
}

bool MapPinsStore::updatePin(const Pin& p)
{
    for (Pin& n : m_pins) {
        if (n.id == p.id) {
            n.lon = p.lon; n.lat = p.lat;
            n.label = p.label; n.note = p.note;
            n.linkId = p.linkId; n.linkLabel = p.linkLabel;
            save();
            emit pinsChanged();
            return true;
        }
    }
    return false;
}

bool MapPinsStore::removePin(const QString& id)
{
    const int before = m_pins.size();
    m_pins.erase(std::remove_if(m_pins.begin(), m_pins.end(),
                                [&](const Pin& n) { return n.id == id; }),
                 m_pins.end());
    if (m_pins.size() == before) return false;
    save();
    emit pinsChanged();
    return true;
}
