#include "UpdateChecker.h"

#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QUrl>

namespace {
const char* kLatestReleaseUrl     = "https://api.github.com/repos/Pedrovskigg/Qiyva-Writer-releases/releases/latest";
const char* kCoverLatestReleaseUrl = "https://api.github.com/repos/Pedrovskigg/qiyva-cover-creator/releases/latest";
}

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void UpdateChecker::check()
{
    if (m_pending) return;
    m_pending = true;

    QNetworkRequest req(QUrl(QString::fromLatin1(kLatestReleaseUrl)));
    req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("mira-writing-update-checker"));
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onReplyFinished(reply);
        reply->deleteLater();
    });
}

void UpdateChecker::onReplyFinished(QNetworkReply* reply)
{
    m_pending = false;

    auto done = [this]() { emit checkFinished(); };

    if (reply->error() != QNetworkReply::NoError) { done(); return; }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) { done(); return; }
    const QJsonObject obj = doc.object();

    QString tag = obj.value(QStringLiteral("tag_name")).toString();
    if (tag.isEmpty()) { done(); return; }
    if (tag.startsWith(QLatin1Char('v')) || tag.startsWith(QLatin1Char('V'))) tag = tag.mid(1);

    const QString current = QApplication::applicationVersion();
    if (compareVersions(tag, current) <= 0) { done(); return; }

    QString downloadUrl;
    for (const QJsonValue& av : obj.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject asset = av.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
            downloadUrl = asset.value(QStringLiteral("browser_download_url")).toString();
            break;
        }
    }
    if (downloadUrl.isEmpty()) { done(); return; }

    emit updateAvailable(tag, downloadUrl,
                         obj.value(QStringLiteral("html_url")).toString(),
                         obj.value(QStringLiteral("body")).toString());
    done();
}


int UpdateChecker::compareVersions(const QString& a, const QString& b)
{
    const QStringList pa = a.split(QLatin1Char('.'));
    const QStringList pb = b.split(QLatin1Char('.'));
    const int n = qMax(pa.size(), pb.size());
    for (int i = 0; i < n; ++i) {
        const int va = i < pa.size() ? pa.at(i).toInt() : 0;
        const int vb = i < pb.size() ? pb.at(i).toInt() : 0;
        if (va != vb) return va < vb ? -1 : 1;
    }
    return 0;
}

QString UpdateChecker::readInstalledCoverVersion(const QString& coverDir)
{
    QFile f(coverDir + QStringLiteral("/cover-version.txt"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QStringLiteral("0.0.0");
    return QString::fromUtf8(f.readAll()).trimmed();
}

void UpdateChecker::checkCover(const QString& coverDir)
{
    if (m_coverPending) return;
    m_coverPending = true;

    const QString installed = readInstalledCoverVersion(coverDir);

    QNetworkRequest req(QUrl(QString::fromLatin1(kCoverLatestReleaseUrl)));
    req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("mira-writing-update-checker"));
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, installed]() {
        onCoverReplyFinished(reply, installed);
        reply->deleteLater();
    });
}

void UpdateChecker::onCoverReplyFinished(QNetworkReply* reply, const QString& installedVersion)
{
    m_coverPending = false;

    auto done = [this]() { emit coverCheckFinished(); };

    if (reply->error() != QNetworkReply::NoError) { done(); return; }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) { done(); return; }
    const QJsonObject obj = doc.object();

    QString tag = obj.value(QStringLiteral("tag_name")).toString();
    if (tag.isEmpty()) { done(); return; }
    if (tag.startsWith(QLatin1Char('v')) || tag.startsWith(QLatin1Char('V'))) tag = tag.mid(1);

    if (compareVersions(tag, installedVersion) <= 0) { done(); return; }

    QString downloadUrl;
    for (const QJsonValue& av : obj.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject asset = av.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
            downloadUrl = asset.value(QStringLiteral("browser_download_url")).toString();
            break;
        }
    }
    if (downloadUrl.isEmpty()) { done(); return; }

    emit coverUpdateAvailable(tag, downloadUrl, obj.value(QStringLiteral("html_url")).toString());
    done();
}
