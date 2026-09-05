#include "TrashService.h"

#include "SystemFolderGuard.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

namespace {

QString indexPath()
{
    return QDir::cleanPath(TrashService::trashRootPath() + QStringLiteral("/index.json"));
}

QVector<TrashedProjectEntry> readIndex()
{
    QVector<TrashedProjectEntry> out;
    QFile f(indexPath());
    if (!f.open(QIODevice::ReadOnly)) return out;
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) return out;

    for (const auto& v : doc.array()) {
        const QJsonObject o = v.toObject();
        TrashedProjectEntry e;
        e.id = o.value(QStringLiteral("id")).toString();
        e.originalPath = o.value(QStringLiteral("originalPath")).toString();
        e.displayName = o.value(QStringLiteral("displayName")).toString();
        e.deletedAtMs = qint64(o.value(QStringLiteral("deletedAtMs")).toDouble());
        if (e.id.isEmpty()) continue;
        out.append(e);
    }
    return out;
}

bool writeIndex(const QVector<TrashedProjectEntry>& entries)
{
    QJsonArray arr;
    for (const auto& e : entries) {
        QJsonObject o;
        o[QStringLiteral("id")] = e.id;
        o[QStringLiteral("originalPath")] = e.originalPath;
        o[QStringLiteral("displayName")] = e.displayName;
        o[QStringLiteral("deletedAtMs")] = double(e.deletedAtMs);
        arr.append(o);
    }
    QDir().mkpath(TrashService::trashRootPath());
    QFile f(indexPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

// Copia uma árvore de diretórios inteira (Qt não tem equivalente pronto pra
// QDir, só QFile::copy de arquivo único). Usada como fallback de
// moveDirectory quando origem e destino estão em unidades/volumes diferentes
// e QDir::rename falha.
bool copyRecursively(const QString& src, const QString& dst)
{
    QDir dstDir(dst);
    if (!dstDir.exists() && !QDir().mkpath(dst)) return false;

    QDirIterator it(src, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                     QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString srcFile = it.next();
        const QString rel = QDir(src).relativeFilePath(srcFile);
        const QString dstFile = QDir::cleanPath(dst + QStringLiteral("/") + rel);
        QDir().mkpath(QFileInfo(dstFile).path());
        if (!QFile::copy(srcFile, dstFile)) return false;
    }
    return true;
}

bool moveDirectory(const QString& src, const QString& dst, QString* errorOut)
{
    QDir().mkpath(QFileInfo(dst).path());
    if (QDir().rename(src, dst)) return true;

    // rename() falha entre unidades diferentes (ex.: projeto no D:, lixeira
    // no C:) — nesse caso copia tudo e só apaga a origem se a cópia inteira
    // deu certo, pra nunca ficar sem as duas cópias ao mesmo tempo.
    if (!copyRecursively(src, dst)) {
        if (errorOut) *errorOut = QStringLiteral("Não foi possível copiar os arquivos para o destino.");
        QDir(dst).removeRecursively();
        return false;
    }
    if (!QDir(src).removeRecursively()) {
        if (errorOut) {
            *errorOut = QStringLiteral(
                "Os arquivos foram copiados, mas a pasta original não pôde ser apagada. "
                "Os dados estão duplicados em \"%1\" e \"%2\".").arg(src, dst);
        }
        return false;
    }
    return true;
}

QString uniqueDestination(const QString& desiredPath)
{
    if (!QFileInfo::exists(desiredPath)) return desiredPath;
    const QFileInfo fi(desiredPath);
    const QString base = fi.fileName();
    const QString dir = fi.path();
    for (int n = 1; n < 1000; ++n) {
        const QString candidate = QDir::cleanPath(
            dir + QStringLiteral("/%1 (restaurado%2)").arg(base).arg(n == 1 ? QString() : QStringLiteral(" %1").arg(n)));
        if (!QFileInfo::exists(candidate)) return candidate;
    }
    return desiredPath + QStringLiteral(".restaurado");
}

} // namespace

namespace TrashService {

QString trashRootPath()
{
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                            + QStringLiteral("/Trash"));
}

QString trashedProjectsPath()
{
    return QDir::cleanPath(trashRootPath() + QStringLiteral("/Projects"));
}

bool trashProject(const QString& projectPath, QString* errorOut)
{
    const QString cleanSrc = QDir::cleanPath(projectPath);

    QString guardReason;
    if (SystemFolderGuard::isProtected(cleanSrc, &guardReason)) {
        if (errorOut) {
            *errorOut = QCoreApplication::translate("TrashService",
                "Esta pasta é %1, não um projeto — o Qenna Writer se recusa a "
                "excluí-la ou movê-la para a lixeira.").arg(guardReason);
        }
        return false;
    }

    QDir srcDir(cleanSrc);
    if (!srcDir.exists()) {
        if (errorOut) *errorOut = QStringLiteral("Pasta do projeto não encontrada.");
        return false;
    }

    QDir().mkpath(trashedProjectsPath());
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString dst = QDir::cleanPath(trashedProjectsPath() + QStringLiteral("/") + id);

    QString moveErr;
    if (!moveDirectory(cleanSrc, dst, &moveErr)) {
        if (errorOut) *errorOut = moveErr;
        return false;
    }

    TrashedProjectEntry entry;
    entry.id = id;
    entry.originalPath = cleanSrc;
    entry.displayName = QFileInfo(cleanSrc).fileName();
    entry.deletedAtMs = QDateTime::currentMSecsSinceEpoch();

    QVector<TrashedProjectEntry> entries = readIndex();
    entries.append(entry);
    if (!writeIndex(entries)) {
        if (errorOut) *errorOut = QStringLiteral("Projeto movido, mas não foi possível registrar na lixeira.");
        // Não é fatal pro dado em si — o projeto está seguro em disco, só não
        // aparece listado até o index ser regravado.
    }
    return true;
}

QVector<TrashedProjectEntry> listTrashedProjects()
{
    QVector<TrashedProjectEntry> entries = readIndex();
    std::sort(entries.begin(), entries.end(),
              [](const TrashedProjectEntry& a, const TrashedProjectEntry& b) {
                  return a.deletedAtMs > b.deletedAtMs;
              });
    return entries;
}

bool restoreProject(const QString& id, QString* restoredPathOut, QString* errorOut)
{
    QVector<TrashedProjectEntry> entries = readIndex();
    const int idx = std::find_if(entries.begin(), entries.end(),
                                  [&](const TrashedProjectEntry& e) { return e.id == id; })
                     - entries.begin();
    if (idx < 0 || idx >= entries.size()) {
        if (errorOut) *errorOut = QStringLiteral("Item não encontrado na lixeira.");
        return false;
    }

    const TrashedProjectEntry entry = entries[idx];
    const QString src = QDir::cleanPath(trashedProjectsPath() + QStringLiteral("/") + entry.id);
    const QString dst = uniqueDestination(entry.originalPath);

    QString moveErr;
    if (!moveDirectory(src, dst, &moveErr)) {
        if (errorOut) *errorOut = moveErr;
        return false;
    }

    entries.remove(idx);
    writeIndex(entries);
    if (restoredPathOut) *restoredPathOut = dst;
    return true;
}

bool purgeProject(const QString& id, QString* errorOut)
{
    QVector<TrashedProjectEntry> entries = readIndex();
    const int idx = std::find_if(entries.begin(), entries.end(),
                                  [&](const TrashedProjectEntry& e) { return e.id == id; })
                     - entries.begin();
    if (idx < 0 || idx >= entries.size()) {
        if (errorOut) *errorOut = QStringLiteral("Item não encontrado na lixeira.");
        return false;
    }

    const QString path = QDir::cleanPath(trashedProjectsPath() + QStringLiteral("/") + entries[idx].id);
    QDir dir(path);
    if (dir.exists() && !dir.removeRecursively()) {
        if (errorOut) *errorOut = QStringLiteral("Não foi possível apagar os arquivos da lixeira.");
        return false;
    }

    entries.remove(idx);
    writeIndex(entries);
    return true;
}

} // namespace TrashService
