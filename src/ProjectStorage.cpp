#include "ProjectStorage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QDateTime>
#include <QRegularExpression>
#include <QTextStream>

namespace {

QString setError(QString* error, const QString& message) {
    if (error) *error = message;
    return message;
}

bool writeTextAtomic(const QString& absolutePath, const QString& content, QString* error) {
    QFileInfo info(absolutePath);
    QDir parent = info.dir();
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        setError(error, QStringLiteral("Falha ao criar diretório %1").arg(parent.absolutePath()));
        return false;
    }
    QSaveFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(error, QStringLiteral("Falha ao abrir %1 para escrita: %2").arg(absolutePath, file.errorString()));
        return false;
    }
    const QByteArray bytes = content.toUtf8();
    if (file.write(bytes) != bytes.size()) {
        setError(error, QStringLiteral("Falha ao escrever em %1: %2").arg(absolutePath, file.errorString()));
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        setError(error, QStringLiteral("Falha ao commitar %1: %2").arg(absolutePath, file.errorString()));
        return false;
    }
    return true;
}

QString readTextSync(const QString& absolutePath, bool* ok, QString* error) {
    if (ok) *ok = false;
    QFile file(absolutePath);
    if (!file.exists()) {
        setError(error, QStringLiteral("Arquivo não existe: %1").arg(absolutePath));
        return QString();
    }
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Falha ao abrir %1: %2").arg(absolutePath, file.errorString()));
        return QString();
    }
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    const QString content = in.readAll();
    if (ok) *ok = true;
    return content;
}

QString sanitizeStampForFilename(const QString& iso) {
    QString s = iso;
    s.replace(QLatin1Char('.'), QLatin1Char('-'));
    s.replace(QLatin1Char(':'), QLatin1Char('-'));
    return s;
}

bool tryBackup(const QString& absolutePath, const QString& backupRoot, const QString& relativeForBackup, QString* error) {
    if (backupRoot.isEmpty() || relativeForBackup.isEmpty()) return true;
    QFile existing(absolutePath);
    if (!existing.exists()) return true;
    bool readOk = false;
    const QString prev = readTextSync(absolutePath, &readOk, error);
    if (!readOk) return true; // sem leitura prévia, segue sem backup (não bloqueia o write)
    if (ProjectStorage::isBlankHtml(prev)) return true;
    const QString stamp = sanitizeStampForFilename(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    const QString backupAbs = ProjectStorage::joinPath(ProjectStorage::backupsDir(backupRoot), QStringLiteral("%1.%2.bak").arg(relativeForBackup, stamp));
    return writeTextAtomic(backupAbs, prev, error);
}

} // namespace

QString ProjectStorage::joinPath(const QString& root, const QString& rel) {
    if (root.isEmpty()) return rel;
    if (rel.isEmpty()) return root;
    QString out = root;
    if (!out.endsWith(QLatin1Char('/')) && !out.endsWith(QLatin1Char('\\'))) {
        out.append(QLatin1Char('/'));
    }
    QString tail = rel;
    while (tail.startsWith(QLatin1Char('/')) || tail.startsWith(QLatin1Char('\\'))) {
        tail.remove(0, 1);
    }
    out.append(tail);
    return QDir::cleanPath(out);
}

QString ProjectStorage::contentDir(const QString& root) {
    return joinPath(root, QStringLiteral("content"));
}

QString ProjectStorage::chaptersDir(const QString& root) {
    return joinPath(root, QStringLiteral("content/chapters"));
}

QString ProjectStorage::itemsDir(const QString& root) {
    return joinPath(root, QStringLiteral("content/items"));
}

QString ProjectStorage::manuscriptDir(const QString& root, const QString& manuscriptId) {
    return joinPath(root, QStringLiteral("content/manuscripts/ms_%1").arg(manuscriptId));
}

QString ProjectStorage::manuscriptChaptersDir(const QString& root, const QString& manuscriptId) {
    return joinPath(root, QStringLiteral("content/manuscripts/ms_%1/chapters").arg(manuscriptId));
}

QString ProjectStorage::manuscriptVarsDir(const QString& root, const QString& manuscriptId) {
    return joinPath(root, QStringLiteral("content/manuscripts/ms_%1/chapters/vars").arg(manuscriptId));
}

QString ProjectStorage::variationPath(const QString& root, const QString& manuscriptId,
                                      const QString& sceneId, const QString& variationId) {
    return joinPath(manuscriptVarsDir(root, manuscriptId),
                    QStringLiteral("scene_%1_%2.html").arg(sceneId, variationId));
}

QString ProjectStorage::backupsDir(const QString& root) {
    return joinPath(root, QStringLiteral("bak"));
}

QString ProjectStorage::indexPath(const QString& root) {
    return joinPath(root, QStringLiteral("project.mira.json"));
}

bool ProjectStorage::isBlankHtml(const QString& html) {
    if (html.isNull()) return true;
    static const QRegularExpression tagRe(QStringLiteral("<[^>]*>"));
    static const QRegularExpression nbspRe(QStringLiteral("&nbsp;|\\x{00A0}"));
    static const QRegularExpression wsRe(QStringLiteral("\\s+"));
    QString stripped = html;
    stripped.replace(tagRe, QStringLiteral(" "));
    stripped.replace(nbspRe, QStringLiteral(" "));
    stripped.replace(wsRe, QStringLiteral(" "));
    return stripped.trimmed().isEmpty();
}

bool ProjectStorage::ensureProjectDirs(const QString& root, QString* error) {
    if (root.isEmpty()) {
        setError(error, QStringLiteral("Raiz do projeto vazia."));
        return false;
    }
    QDir base(root);
    if (!base.exists() && !base.mkpath(QStringLiteral("."))) {
        setError(error, QStringLiteral("Falha ao criar %1").arg(root));
        return false;
    }
    if (!QDir().mkpath(chaptersDir(root))) {
        setError(error, QStringLiteral("Falha ao criar %1").arg(chaptersDir(root)));
        return false;
    }
    if (!QDir().mkpath(itemsDir(root))) {
        setError(error, QStringLiteral("Falha ao criar %1").arg(itemsDir(root)));
        return false;
    }
    return true;
}

bool ProjectStorage::ensureManuscriptDirs(const QString& root, const QString& manuscriptId, QString* error) {
    if (manuscriptId.isEmpty()) return true;
    if (!QDir().mkpath(manuscriptChaptersDir(root, manuscriptId))) {
        setError(error, QStringLiteral("Falha ao criar %1").arg(manuscriptChaptersDir(root, manuscriptId)));
        return false;
    }
    return true;
}

bool ProjectStorage::writeIndex(const QString& root, const QJsonObject& index, QString* error) {
    if (!ensureProjectDirs(root, error)) return false;
    const QJsonDocument doc(index);
    const QString json = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    return writeTextAtomic(indexPath(root), json, error);
}

QJsonObject ProjectStorage::readIndex(const QString& root, bool* ok, QString* error) {
    if (ok) *ok = false;
    bool readOk = false;
    const QString text = readTextSync(indexPath(root), &readOk, error);
    if (!readOk) return QJsonObject();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(error, QStringLiteral("JSON inválido em %1: %2").arg(indexPath(root), parseError.errorString()));
        return QJsonObject();
    }
    if (ok) *ok = true;
    return doc.object();
}

ProjectStorage::WriteOutcome ProjectStorage::writeChapter(const QString& root, const QString& relativeFile, const QString& html, QString* error) {
    if (relativeFile.isEmpty()) {
        setError(error, QStringLiteral("Caminho do capítulo vazio."));
        return WriteOutcome::Failed;
    }
    if (!ensureProjectDirs(root, error)) return WriteOutcome::Failed;
    const QString abs = joinPath(root, relativeFile);
    return writeTextGuarded(abs, html, root, relativeFile, error);
}

QString ProjectStorage::readChapter(const QString& root, const QString& relativeFile, bool* ok, QString* error) {
    return readTextSync(joinPath(root, relativeFile), ok, error);
}

ProjectStorage::WriteOutcome ProjectStorage::writeTextGuarded(const QString& absolutePath, const QString& content, const QString& backupRoot, const QString& relativeForBackup, QString* error) {
    const bool incomingBlank = isBlankHtml(content);
    QFile existing(absolutePath);
    if (incomingBlank && existing.exists()) {
        bool readOk = false;
        const QString prev = readTextSync(absolutePath, &readOk, nullptr);
        if (readOk && !isBlankHtml(prev)) {
            setError(error, QStringLiteral("Bloqueado overwrite em branco: %1").arg(absolutePath));
            return WriteOutcome::BlankOverwriteBlocked;
        }
    }
    if (!incomingBlank) {
        QString backupError;
        if (!tryBackup(absolutePath, backupRoot, relativeForBackup, &backupError)) {
            qWarning("Backup falhou (%s); seguindo com o write mesmo assim", qUtf8Printable(backupError));
        }
    }
    if (!writeTextAtomic(absolutePath, content, error)) {
        return WriteOutcome::Failed;
    }
    return WriteOutcome::Written;
}

QString ProjectStorage::readText(const QString& absolutePath, bool* ok, QString* error) {
    return readTextSync(absolutePath, ok, error);
}

bool ProjectStorage::writeBinary(const QString& absolutePath, const QByteArray& data, QString* error) {
    QFileInfo info(absolutePath);
    QDir parent = info.dir();
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        setError(error, QStringLiteral("Falha ao criar diretório %1").arg(parent.absolutePath()));
        return false;
    }
    QSaveFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(error, QStringLiteral("Falha ao abrir %1: %2").arg(absolutePath, file.errorString()));
        return false;
    }
    if (file.write(data) != data.size()) {
        setError(error, QStringLiteral("Falha ao escrever em %1: %2").arg(absolutePath, file.errorString()));
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        setError(error, QStringLiteral("Falha ao commitar %1: %2").arg(absolutePath, file.errorString()));
        return false;
    }
    return true;
}
