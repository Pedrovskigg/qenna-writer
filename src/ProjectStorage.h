#pragma once

#include <QString>
#include <QJsonObject>

class ProjectStorage {
public:
    enum class WriteOutcome {
        Written,
        BlankOverwriteBlocked,
        Failed,
    };

    static QString joinPath(const QString& root, const QString& rel);
    static QString contentDir(const QString& root);
    static QString chaptersDir(const QString& root);
    static QString itemsDir(const QString& root);
    static QString manuscriptDir(const QString& root, const QString& manuscriptId);
    static QString manuscriptChaptersDir(const QString& root, const QString& manuscriptId);
    static QString manuscriptVarsDir(const QString& root, const QString& manuscriptId);
    static QString variationPath(const QString& root, const QString& manuscriptId,
                                 const QString& sceneId, const QString& variationId);
    static QString backupsDir(const QString& root);
    static QString indexPath(const QString& root);

    static bool isBlankHtml(const QString& html);

    static bool ensureProjectDirs(const QString& root, QString* error = nullptr);
    static bool ensureManuscriptDirs(const QString& root, const QString& manuscriptId, QString* error = nullptr);

    static bool writeIndex(const QString& root, const QJsonObject& index, QString* error = nullptr);
    static QJsonObject readIndex(const QString& root, bool* ok = nullptr, QString* error = nullptr);

    static WriteOutcome writeChapter(const QString& root, const QString& relativeFile, const QString& html, QString* error = nullptr);
    static QString readChapter(const QString& root, const QString& relativeFile, bool* ok = nullptr, QString* error = nullptr);

    static WriteOutcome writeTextGuarded(const QString& absolutePath, const QString& content, const QString& backupRoot, const QString& relativeForBackup, QString* error = nullptr);
    static QString readText(const QString& absolutePath, bool* ok = nullptr, QString* error = nullptr);

    static bool writeBinary(const QString& absolutePath, const QByteArray& data, QString* error = nullptr);
};
