#include "BackupService.h"

#include "ZipWriter.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QTimer>

namespace {

QString stateKey(const QString& prefix, const QString& projectRoot)
{
    const QString clean = QDir::cleanPath(projectRoot);
    const QByteArray hash = QCryptographicHash::hash(clean.toUtf8(), QCryptographicHash::Sha1).toHex();
    return prefix + QLatin1Char('/') + QString::fromLatin1(hash);
}

// Quanto tempo cada fatia pode segurar a janela antes de devolver a vez.
constexpr int kSliceMs = 12;

// Imagem, áudio e afins já vêm comprimidos: tentar de novo só gasta tempo.
bool worthCompressing(const QString& path)
{
    static const QSet<QString> packed = {
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("webp"),
        QStringLiteral("gif"), QStringLiteral("mp3"), QStringLiteral("ogg"), QStringLiteral("m4a"),
        QStringLiteral("mp4"), QStringLiteral("zip"), QStringLiteral("epub"), QStringLiteral("docx"),
        QStringLiteral("odt"), QStringLiteral("pdf"),
    };
    return !packed.contains(QFileInfo(path).suffix().toLower());
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

Job::Job(const QString& projectRoot, const QString& destFolder, QObject* parent)
    : QObject(parent)
    , m_root(QDir::cleanPath(projectRoot))
    , m_dest(QDir::cleanPath(destFolder))
    , m_destIsBlank(destFolder.trimmed().isEmpty())
    , m_zip(new ZipWriter)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(0);
    connect(m_timer, &QTimer::timeout, this, &Job::processSlice);
}

Job::~Job()
{
    discardPartialFile();
    delete m_zip;
}

void Job::start()
{
    if (!QDir(m_root).exists()) {
        fail(tr("Pasta do projeto não encontrada."));
        return;
    }
    if (m_destIsBlank) {
        fail(tr("Nenhuma pasta de destino configurada para o backup."));
        return;
    }
    if (!QDir().mkpath(m_dest)) {
        fail(tr("Não foi possível criar a pasta de destino: %1").arg(m_dest));
        return;
    }

    // Se o destino cair dentro do próprio projeto, o conteúdo já escrito ali
    // (backups anteriores) não deve entrar de novo dentro do zip novo.
    const bool destInsideRoot = m_dest == m_root || m_dest.startsWith(m_root + QLatin1Char('/'));
    QDirIterator it(m_root, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filePath = it.next();
        if (destInsideRoot && QDir::cleanPath(filePath).startsWith(m_dest + QLatin1Char('/'))) continue;
        m_files.append(filePath);
        m_totalBytes += it.fileInfo().size();
    }
    if (m_files.isEmpty()) {
        fail(tr("A pasta do projeto está vazia — nada para incluir no backup."));
        return;
    }

    const QString projectName = QFileInfo(m_root).fileName();
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("dd-MM-yyyy HHmm"));
    m_zipPath = QDir::cleanPath(m_dest + QStringLiteral("/%1 - backup %2.zip").arg(projectName, stamp));

    // Grava com outro nome e só renomeia no fim: backup pela metade (app fechado,
    // pendrive arrancado) nunca fica parecendo um .zip bom.
    m_part.setFileName(m_zipPath + QStringLiteral(".part"));
    if (!m_part.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(tr("Não foi possível gravar o arquivo de backup em: %1").arg(m_zipPath));
        return;
    }
    m_zip->setDevice(&m_part);
    m_timer->start();
}

void Job::cancel()
{
    if (m_finished) return;
    finishWith(false, QString());
}

// Tudo na thread da janela, em fatias curtas, e não numa thread separada: ler um
// arquivo enquanto o salvamento automático grava nele faz a gravação falhar no
// Windows. Assim as duas coisas nunca acontecem ao mesmo tempo.
void Job::processSlice()
{
    const QDir root(m_root);
    QElapsedTimer slice;
    slice.start();
    while (m_next < m_files.size() && slice.elapsed() < kSliceMs) {
        const QString filePath = m_files.at(m_next++);
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QByteArray data = f.readAll();
        f.close();
        m_doneBytes += data.size();
        m_zip->addFile(root.relativeFilePath(filePath), data, worthCompressing(filePath));
        if (m_zip->failed()) {
            fail(tr("Não foi possível gravar o arquivo de backup em: %1").arg(m_zipPath));
            return;
        }
    }

    const int percent = m_totalBytes > 0 ? int(m_doneBytes * 100 / m_totalBytes) : 100;
    emit progress(qMin(percent, 99), m_next, int(m_files.size()));

    if (m_next >= m_files.size()) completeArchive();
}

void Job::completeArchive()
{
    m_timer->stop();
    m_zip->finish();
    const bool written = !m_zip->failed() && m_part.flush();
    m_part.close();
    if (!written) {
        fail(tr("Não foi possível gravar o arquivo de backup em: %1").arg(m_zipPath));
        return;
    }
    QFile::remove(m_zipPath); // dois backups no mesmo minuto: fica o mais novo
    if (!QFile::rename(m_part.fileName(), m_zipPath)) {
        fail(tr("Não foi possível gravar o arquivo de backup em: %1").arg(m_zipPath));
        return;
    }
    m_partHandled = true; // virou o .zip, não existe mais o que limpar
    markBackupDone(m_root, QDateTime::currentMSecsSinceEpoch());
    finishWith(true, QString());
}

void Job::fail(const QString& error)
{
    finishWith(false, error);
}

void Job::finishWith(bool ok, const QString& error)
{
    m_finished = true;
    m_timer->stop();
    if (!ok) discardPartialFile();
    emit finished(ok, ok ? m_zipPath : QString(), error);
}

void Job::discardPartialFile()
{
    if (m_partHandled) return;
    m_partHandled = true;
    m_part.close();
    if (!m_part.fileName().isEmpty()) QFile::remove(m_part.fileName());
}

} // namespace BackupService
