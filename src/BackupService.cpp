#include "BackupService.h"

#include "ZipWriter.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

namespace {

QString stateKey(const QString& prefix, const QString& projectRoot)
{
    const QString clean = QDir::cleanPath(projectRoot);
    const QByteArray hash = QCryptographicHash::hash(clean.toUtf8(), QCryptographicHash::Sha1).toHex();
    return prefix + QLatin1Char('/') + QString::fromLatin1(hash);
}

} // namespace

namespace BackupService {

Settings loadSettings()
{
    QSettings s;
    Settings out;
    out.mode = static_cast<Mode>(s.value(QStringLiteral("backup/mode"), 0).toInt());
    out.folder = s.value(QStringLiteral("backup/folder")).toString();
    out.intervalMinutes = s.value(QStringLiteral("backup/intervalMinutes"), 1440).toInt();
    if (out.intervalMinutes < 1440) out.intervalMinutes = 1440;
    return out;
}

void saveSettings(const Settings& s)
{
    QSettings qs;
    qs.setValue(QStringLiteral("backup/mode"), static_cast<int>(s.mode));
    qs.setValue(QStringLiteral("backup/folder"), s.folder);
    qs.setValue(QStringLiteral("backup/intervalMinutes"), s.intervalMinutes);
}

qint64 lastBackupAt(const QString& projectRoot)
{
    return QSettings().value(stateKey(QStringLiteral("backup/lastRun"), projectRoot), 0).toLongLong();
}

qint64 lastReminderAt(const QString& projectRoot)
{
    return QSettings().value(stateKey(QStringLiteral("backup/lastReminder"), projectRoot), 0).toLongLong();
}

void markBackupDone(const QString& projectRoot, qint64 whenMs)
{
    QSettings().setValue(stateKey(QStringLiteral("backup/lastRun"), projectRoot), whenMs);
}

void markReminderShown(const QString& projectRoot, qint64 whenMs)
{
    QSettings().setValue(stateKey(QStringLiteral("backup/lastReminder"), projectRoot), whenMs);
}

bool runBackupNow(const QString& projectRoot, const QString& destFolder, QString* outZipPath, QString* error)
{
    const QString cleanRoot = QDir::cleanPath(projectRoot);
    QDir rootDir(cleanRoot);
    if (!rootDir.exists()) {
        if (error) *error = QObject::tr("Pasta do projeto não encontrada.");
        return false;
    }
    if (destFolder.trimmed().isEmpty()) {
        if (error) *error = QObject::tr("Nenhuma pasta de destino configurada para o backup.");
        return false;
    }

    const QString cleanDest = QDir::cleanPath(destFolder);
    if (!QDir().mkpath(cleanDest)) {
        if (error) *error = QObject::tr("Não foi possível criar a pasta de destino: %1").arg(cleanDest);
        return false;
    }

    // Se o destino cair dentro do próprio projeto, o conteúdo já escrito ali
    // (backups anteriores) não deve entrar de novo dentro do zip novo.
    const bool destInsideRoot = cleanDest == cleanRoot
        || cleanDest.startsWith(cleanRoot + QLatin1Char('/'));

    ZipWriter zip;
    QDirIterator it(cleanRoot, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                     QDirIterator::Subdirectories);
    bool any = false;
    while (it.hasNext()) {
        const QString filePath = it.next();
        if (destInsideRoot && QDir::cleanPath(filePath).startsWith(cleanDest + QLatin1Char('/'))) {
            continue; // dentro da própria pasta de destino do backup
        }
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QByteArray data = f.readAll();
        f.close();
        const QString rel = QDir(cleanRoot).relativeFilePath(filePath);
        zip.addFile(rel, data);
        any = true;
    }
    if (!any) {
        if (error) *error = QObject::tr("A pasta do projeto está vazia — nada para incluir no backup.");
        return false;
    }

    const QString projectName = QFileInfo(cleanRoot).fileName();
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("dd-MM-yyyy HHmm"));
    const QString zipPath = QDir::cleanPath(cleanDest + QStringLiteral("/%1 - backup %2.zip")
                                                             .arg(projectName, stamp));

    QFile out(zipPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = QObject::tr("Não foi possível gravar o arquivo de backup em: %1").arg(zipPath);
        return false;
    }
    out.write(zip.finish());
    out.close();

    if (outZipPath) *outZipPath = zipPath;
    markBackupDone(cleanRoot, QDateTime::currentMSecsSinceEpoch());
    return true;
}

} // namespace BackupService
