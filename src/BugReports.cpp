#include "BugReports.h"

#include "Recados.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>

namespace {
QString apiBase() { return RecadosFeed::instance()->apiBase(); }

QString adminKey() { return QSettings().value(QStringLiteral("recados/adminKey")).toString(); }

QNetworkRequest request(const QString& path, bool withKey = false)
{
    QNetworkRequest req{QUrl(apiBase() + path)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("qenna-writer-bugs"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/json"));
    req.setTransferTimeout(15000);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    if (withKey && !adminKey().isEmpty()) req.setRawHeader("Authorization", "Bearer " + adminKey().toUtf8());
    return req;
}

QDateTime fromMs(const QJsonValue& v)
{
    return v.isNull() || v.isUndefined() ? QDateTime() : QDateTime::fromMSecsSinceEpoch(v.toInteger());
}

BugReport parseBug(const QJsonObject& o)
{
    BugReport b;
    b.id = o.value(QStringLiteral("id")).toInteger();
    b.code = o.value(QStringLiteral("code")).toString();
    b.name = o.value(QStringLiteral("name")).toString();
    b.title = o.value(QStringLiteral("title")).toString();
    b.body = o.value(QStringLiteral("body")).toString();
    b.steps = o.value(QStringLiteral("steps")).toString();
    b.env = o.value(QStringLiteral("env")).toString();
    b.stage = o.value(QStringLiteral("stage")).toString();
    b.reply = o.value(QStringLiteral("reply")).toString();
    b.version = o.value(QStringLiteral("version")).toString();
    b.at = fromMs(o.value(QStringLiteral("at")));
    b.replyAt = fromMs(o.value(QStringLiteral("reply_at")));
    b.updated = fromMs(o.value(QStringLiteral("updated")));
    return b;
}
}

BugFeed* BugFeed::instance()
{
    static BugFeed* feed = new BugFeed(qApp);
    return feed;
}

BugFeed::BugFeed(QObject* parent) : QObject(parent) {}

bool BugFeed::enabled() const { return RecadosFeed::instance()->commentsEnabled(); }
bool BugFeed::isAuthor() const { return RecadosFeed::instance()->isAuthor(); }

QString BugFeed::newCode()
{
    // sem os que se confundem ao digitar: 0/O, 1/I/L
    static const char kAlpha[] = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";
    QString c = QStringLiteral("QW-");
    for (int i = 0; i < 5; ++i) c += QLatin1Char(kAlpha[QRandomGenerator::global()->bounded(int(sizeof(kAlpha) - 1))]);
    return c;
}

QString BugFeed::draftCode()
{
    QSettings qs;
    QString c = qs.value(QStringLiteral("bugs/draftCode")).toString();
    if (c.isEmpty()) {
        c = newCode();
        qs.setValue(QStringLiteral("bugs/draftCode"), c);
    }
    return c;
}

void BugFeed::fetchMine()
{
    if (!enabled()) return;
    QNetworkRequest req = request(QStringLiteral("/bugs/mine"));
    QUrl u = req.url();
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("client"), RecadosFeed::clientId());
    u.setQuery(q);
    req.setUrl(u);
    QNetworkReply* reply = RecadosFeed::instance()->network()->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray()) return;
        m_mine.clear();
        for (const QJsonValue& v : doc.array()) m_mine.append(parseBug(v.toObject()));
        m_mineLoaded = true;
        emit changed();
    });
}

void BugFeed::fetchResolved()
{
    if (!enabled()) return;
    QNetworkReply* reply = RecadosFeed::instance()->network()->get(request(QStringLiteral("/bugs/resolved")));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray()) return;
        m_resolved.clear();
        for (const QJsonValue& v : doc.array()) {
            const QJsonObject o = v.toObject();
            ResolvedBug r;
            r.id = o.value(QStringLiteral("id")).toInteger();
            r.title = o.value(QStringLiteral("title")).toString();
            r.version = o.value(QStringLiteral("version")).toString();
            r.updated = fromMs(o.value(QStringLiteral("updated")));
            m_resolved.append(r);
        }
        m_resolvedLoaded = true;
        emit changed();
    });
}

void BugFeed::fetchAll()
{
    if (!enabled() || !isAuthor()) return;
    QNetworkReply* reply = RecadosFeed::instance()->network()->get(request(QStringLiteral("/bugs/all"), true));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 403) { RecadosFeed::instance()->setAdminKey(QString()); emit adminKeyRejected(); return; }
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray()) return;
        m_all.clear();
        for (const QJsonValue& v : doc.array()) m_all.append(parseBug(v.toObject()));
        m_allLoaded = true;
        emit changed();
    });
}

void BugFeed::submit(const QString& name, const QString& title, const QString& body,
                     const QString& steps, const QString& env)
{
    if (!enabled()) return;
    QJsonObject o;
    o.insert(QStringLiteral("code"), draftCode());
    o.insert(QStringLiteral("client"), RecadosFeed::clientId());
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("title"), title);
    o.insert(QStringLiteral("body"), body);
    o.insert(QStringLiteral("steps"), steps);
    o.insert(QStringLiteral("env"), env);
    QNetworkReply* reply = RecadosFeed::instance()->network()->post(request(QStringLiteral("/bugs")),
                                                                    QJsonDocument(o).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, name, title, body, steps, env]() {
        reply->deleteLater();
        const QJsonObject res = QJsonDocument::fromJson(reply->readAll()).object();
        if (res.value(QStringLiteral("ok")).toBool() && res.contains(QStringLiteral("bug"))) {
            const BugReport b = parseBug(res.value(QStringLiteral("bug")).toObject());
            m_mine.prepend(b);
            QSettings().remove(QStringLiteral("bugs/draftCode"));   // o próximo relato ganha outro
            markSeen();
            emit changed();
            emit submitted(b.code);
            return;
        }
        const QString err = res.value(QStringLiteral("error")).toString();
        if (err == QLatin1String("code-taken")) {
            // raríssimo: o código sorteado já existia; sorteia outro e manda de novo
            QSettings().remove(QStringLiteral("bugs/draftCode"));
            submit(name, title, body, steps, env);
            return;
        }
        emit failed(err.isEmpty() ? QStringLiteral("network") : err);
    });
}

void BugFeed::update(qint64 id, const QString& stage, const QString& reply, const QString& version)
{
    if (!enabled() || !isAuthor()) return;
    QJsonObject o;
    o.insert(QStringLiteral("stage"), stage);
    o.insert(QStringLiteral("reply"), reply);
    o.insert(QStringLiteral("version"), version);
    QNetworkReply* r = RecadosFeed::instance()->network()->post(request(QStringLiteral("/bugs/%1").arg(id), true),
                                                                QJsonDocument(o).toJson(QJsonDocument::Compact));
    connect(r, &QNetworkReply::finished, this, [this, r, id]() {
        r->deleteLater();
        const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 403) { RecadosFeed::instance()->setAdminKey(QString()); emit adminKeyRejected(); return; }
        const QJsonObject res = QJsonDocument::fromJson(r->readAll()).object();
        if (!res.value(QStringLiteral("ok")).toBool()) { emit failed(QStringLiteral("network")); return; }
        const BugReport b = parseBug(res.value(QStringLiteral("bug")).toObject());
        for (BugReport& x : m_all) if (x.id == id) x = b;
        for (BugReport& x : m_mine) if (x.id == id) { const QString n = x.name; x = b; x.name = n; }
        emit changed();
        fetchResolved();
    });
}

void BugFeed::remove(qint64 id)
{
    if (!enabled() || !isAuthor()) return;
    QNetworkReply* r = RecadosFeed::instance()->network()->deleteResource(request(QStringLiteral("/bugs/%1").arg(id), true));
    connect(r, &QNetworkReply::finished, this, [this, r, id]() {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) return;
        m_all.erase(std::remove_if(m_all.begin(), m_all.end(), [id](const BugReport& b) { return b.id == id; }), m_all.end());
        m_mine.erase(std::remove_if(m_mine.begin(), m_mine.end(), [id](const BugReport& b) { return b.id == id; }), m_mine.end());
        emit changed();
        fetchResolved();
    });
}

QString BugFeed::seenKey(const BugReport& b)
{
    return b.stage + QLatin1Char('|') + (b.replyAt.isValid() ? QString::number(b.replyAt.toMSecsSinceEpoch()) : QString());
}

bool BugFeed::hasNews() const
{
    const QVariantMap seen = QSettings().value(QStringLiteral("bugs/seen")).toMap();
    for (const BugReport& b : m_mine) {
        const QString k = QString::number(b.id);
        if (seen.contains(k)) { if (seen.value(k).toString() != seenKey(b)) return true; }
        else if (b.stage != QLatin1String("sent") || !b.reply.isEmpty()) return true;
    }
    return false;
}

void BugFeed::markSeen()
{
    QVariantMap seen;
    for (const BugReport& b : m_mine) seen.insert(QString::number(b.id), seenKey(b));
    QSettings().setValue(QStringLiteral("bugs/seen"), seen);
}
