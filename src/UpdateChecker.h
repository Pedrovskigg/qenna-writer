#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// Checagem silenciosa de atualizações via GitHub Releases
// (Pedrovskigg/Qenna-Writer-releases). Roda em background no startup; se a release
// mais recente for mais nova que QApplication::applicationVersion(), emite
// updateAvailable() com a URL do instalador (.exe) anexado à release.
// Falha de rede/sem release/sem asset: silenciosamente não emite nada.
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);

    void check();
    // Checa atualizações do Cover Creator.
    // coverDir = pasta onde está Mira Cover.exe (lê cover-version.txt de lá).
    void checkCover(const QString& coverDir);

signals:
    void updateAvailable(const QString& version, const QString& downloadUrl, const QString& releaseUrl, const QString& releaseNotes);
    void checkFinished();       // dispara ao fim da checagem (update ou não)
    void coverUpdateAvailable(const QString& version, const QString& downloadUrl, const QString& releaseUrl);
    void coverCheckFinished();  // dispara ao fim da checagem do Cover (update ou não)

private:
    void onReplyFinished(QNetworkReply* reply);
    void onCoverReplyFinished(QNetworkReply* reply, const QString& installedVersion);
    static int compareVersions(const QString& a, const QString& b);
    static QString readInstalledCoverVersion(const QString& coverDir);

    QNetworkAccessManager* m_nam;
    bool m_pending = false;
    bool m_coverPending = false;
};
