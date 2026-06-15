#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// Checagem silenciosa de atualizações via GitHub Releases
// (Pedrovskigg/mira-writing-2-releases). Roda em background no startup; se a release
// mais recente for mais nova que QApplication::applicationVersion(), emite
// updateAvailable() com a URL do instalador (.exe) anexado à release.
// Falha de rede/sem release/sem asset: silenciosamente não emite nada.
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);

    void check();

signals:
    void updateAvailable(const QString& version, const QString& downloadUrl, const QString& releaseUrl);

private:
    void onReplyFinished(QNetworkReply* reply);
    static int compareVersions(const QString& a, const QString& b);

    QNetworkAccessManager* m_nam;
    bool m_pending = false;
};
