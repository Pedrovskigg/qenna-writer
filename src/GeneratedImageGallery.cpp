#include "GeneratedImageGallery.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace GeneratedImageGallery {

namespace {

QString imagesDir(const QString& projectRoot)
{
    return projectRoot + QStringLiteral("/ai_context/imagens_geradas");
}

QString indexPath(const QString& projectRoot)
{
    return imagesDir(projectRoot) + QStringLiteral("/index.json");
}

} // namespace

Entry save(const QString& projectRoot, const QImage& img,
           const QString& characterName, const QString& prompt)
{
    Entry entry;
    if (projectRoot.isEmpty() || img.isNull()) return entry;

    QDir().mkpath(imagesDir(projectRoot));
    const QString fileName = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmsszzz"))
        + QStringLiteral(".png");
    const QString filePath = imagesDir(projectRoot) + QStringLiteral("/") + fileName;
    if (!img.save(filePath, "PNG")) return entry;

    entry.filePath = filePath;
    entry.characterName = characterName;
    entry.prompt = prompt;
    entry.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    QVector<Entry> entries = load(projectRoot);
    entries.prepend(entry);

    QJsonArray arr;
    for (const Entry& e : entries) {
        QJsonObject o;
        o[QStringLiteral("filePath")] = e.filePath;
        o[QStringLiteral("characterName")] = e.characterName;
        o[QStringLiteral("prompt")] = e.prompt;
        o[QStringLiteral("createdAt")] = e.createdAt;
        arr.append(o);
    }
    QFile f(indexPath(projectRoot));
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    }
    return entry;
}

QVector<Entry> load(const QString& projectRoot)
{
    QVector<Entry> entries;
    if (projectRoot.isEmpty()) return entries;

    QFile f(indexPath(projectRoot));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return entries;

    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        Entry e;
        e.filePath = o.value(QStringLiteral("filePath")).toString();
        e.characterName = o.value(QStringLiteral("characterName")).toString();
        e.prompt = o.value(QStringLiteral("prompt")).toString();
        e.createdAt = o.value(QStringLiteral("createdAt")).toString();
        entries.append(e);
    }
    return entries;
}

} // namespace GeneratedImageGallery
