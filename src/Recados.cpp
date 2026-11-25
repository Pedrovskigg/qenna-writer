#include "Recados.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <algorithm>

namespace {
const char* kRecadosUrl = "https://qennawriter.com/recados.json";
// Worker na conta Cloudflare do Qenna (tools/comentarios-worker). Vazio = comentários desligados.
const char* kComentariosUrl = "https://qenna-comentarios.qennawriter.workers.dev/comments";

Comentario parseComment(const QJsonObject& o)
{
    Comentario c;
    c.id = o.value(QStringLiteral("id")).toInteger();
    c.name = o.value(QStringLiteral("name")).toString();
    c.body = o.value(QStringLiteral("body")).toString();
    c.at = QDateTime::fromMSecsSinceEpoch(o.value(QStringLiteral("at")).toInteger());
    c.author = o.value(QStringLiteral("author")).toBool();
    c.parent = o.value(QStringLiteral("parent")).toInteger();
    return c;
}

// "pt_BR" -> "pt"; o resto já vem com duas letras.
QString langKey()
{
    const QString code = QSettings().value(QStringLiteral("app/language")).toString();
    return code.isEmpty() ? QStringLiteral("en") : code.left(2);
}

// Campo traduzido: aceita texto puro (vale pra todos) ou um objeto por idioma.
QString pick(const QJsonValue& v, const QString& lang)
{
    if (v.isString()) return v.toString();
    const QJsonObject o = v.toObject();
    for (const QString& k : { lang, QStringLiteral("en"), QStringLiteral("pt") }) {
        const QString s = o.value(k).toString();
        if (!s.trimmed().isEmpty()) return s;
    }
    return {};
}
}

RecadosFeed* RecadosFeed::instance()
{
    static RecadosFeed* feed = new RecadosFeed(qApp);
    return feed;
}

RecadosFeed::RecadosFeed(QObject* parent)
    : QObject(parent)
{
    QSettings qs;
    const QStringList read = qs.value(QStringLiteral("recados/read")).toStringList();
    m_read = QSet<QString>(read.begin(), read.end());
    const QStringList dis = qs.value(QStringLiteral("recados/dismissed")).toStringList();
    m_dismissedIds = QSet<QString>(dis.begin(), dis.end());
    // A última lista baixada: o menu já abre com ela, sem esperar a rede.
    parse(qs.value(QStringLiteral("recados/cache")).toByteArray());
}

QNetworkAccessManager* RecadosFeed::network()
{
    if (!m_nam) m_nam = new QNetworkAccessManager(this);
    return m_nam;
}

void RecadosFeed::fetch()
{
    if (m_fetched) return;
    m_fetched = true;
    QString override = qEnvironmentVariable("QENNA_RECADOS_URL");
#if !defined(NDEBUG) && defined(DEV_ASSETS_DIR)
    // Build de desenvolvimento: lê o docs/recados.json do próprio repo, pra
    // testar recado novo antes de ele ir pro site.
    if (override.isEmpty()) {
        const QString local = QStringLiteral(DEV_ASSETS_DIR "/../../docs/recados.json");
        if (QFile::exists(local)) override = QUrl::fromLocalFile(QFileInfo(local).absoluteFilePath()).toString();
    }
#endif
    const QUrl url(override.isEmpty() ? QString::fromLatin1(kRecadosUrl) : override);
    if (url.isLocalFile()) {
        QFile f(url.toLocalFile());
        if (f.open(QIODevice::ReadOnly)) parse(f.readAll());
        emit changed();
        return;
    }
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("qenna-writer-recados"));
    req.setTransferTimeout(15000);
    // Sem cache do Qt: recado novo tem que aparecer na próxima abertura.
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    QNetworkReply* reply = network()->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QByteArray data = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) return;   // página de erro ou arquivo quebrado: fica a lista antiga
        QSettings().setValue(QStringLiteral("recados/cache"), data);
        parse(data);
        emit changed();
    });
}

void RecadosFeed::parse(const QByteArray& json)
{
    m_recados.clear();
    if (json.isEmpty()) return;
    const QJsonArray arr = QJsonDocument::fromJson(json).object().value(QStringLiteral("recados")).toArray();
    const QString lang = langKey();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        Recado r;
        r.id = o.value(QStringLiteral("id")).toString();
        r.date = QDate::fromString(o.value(QStringLiteral("date")).toString(), Qt::ISODate);
        r.title = pick(o.value(QStringLiteral("title")), lang);
        r.body = pick(o.value(QStringLiteral("body")), lang);
        const QJsonObject link = o.value(QStringLiteral("link")).toObject();
        r.linkUrl = link.value(QStringLiteral("url")).toString();
        r.linkLabel = pick(link.value(QStringLiteral("label")), lang);
        r.imageUrl = o.value(QStringLiteral("image")).toString();
        if (r.id.isEmpty() || r.title.isEmpty()) continue;
        m_recados.append(r);
    }
    std::stable_sort(m_recados.begin(), m_recados.end(),
                     [](const Recado& a, const Recado& b) { return a.date > b.date; });
}

void RecadosFeed::markRead(const QString& id)
{
    if (m_read.contains(id)) return;
    m_read.insert(id);
    saveState();
    emit stateChanged();
}

void RecadosFeed::markAllRead()
{
    for (const Recado& r : m_recados) m_read.insert(r.id);
    saveState();
    emit stateChanged();
}

bool RecadosFeed::stripDismissed() const
{
    if (m_dismissedIds.isEmpty()) return false;
    for (const Recado& r : m_recados)
        if (!m_dismissedIds.contains(r.id)) return false;
    return true;
}

void RecadosFeed::dismissStrip()
{
    for (const Recado& r : m_recados) m_dismissedIds.insert(r.id);
    saveState();
    emit stateChanged();
}

QString RecadosFeed::commentsUrl()
{
    const QString override = qEnvironmentVariable("QENNA_COMENTARIOS_URL");
    return override.isEmpty() ? QString::fromLatin1(kComentariosUrl) : override;
}

void RecadosFeed::fetchComments()
{
    if (!commentsEnabled()) return;
    QNetworkRequest req{QUrl(commentsUrl())};
    req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("qenna-writer-recados"));
    req.setTransferTimeout(15000);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    QNetworkReply* reply = network()->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) return;
        m_comments.clear();
        const QJsonObject all = doc.object();
        for (auto it = all.begin(); it != all.end(); ++it) {
            QList<Comentario> list;
            for (const QJsonValue& v : it.value().toArray()) list.append(parseComment(v.toObject()));
            m_comments.insert(it.key(), list);
        }
        m_commentsLoaded = true;
        emit commentsChanged();
    });
}

void RecadosFeed::postComment(const QString& recadoId, const QString& name, const QString& body, qint64 parent)
{
    if (!commentsEnabled()) return;
    QNetworkRequest req{QUrl(commentsUrl())};
    req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("qenna-writer-recados"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/json"));
    req.setTransferTimeout(15000);
    const QString key = QSettings().value(QStringLiteral("recados/adminKey")).toString();
    if (!key.isEmpty()) req.setRawHeader("Authorization", "Bearer " + key.toUtf8());
    QJsonObject o;
    o.insert(QStringLiteral("recado"), recadoId);
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("body"), body);
    if (parent > 0) o.insert(QStringLiteral("parent"), parent);
    o.insert(QStringLiteral("client"), clientId());   // pro ban pegar o aparelho (o servidor guarda só o hash)
    QNetworkReply* reply = network()->post(req, QJsonDocument(o).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, recadoId]() {
        reply->deleteLater();
        const QJsonObject res = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonObject c = res.value(QStringLiteral("comment")).toObject();
        if (res.value(QStringLiteral("ok")).toBool() && !c.isEmpty()) {
            m_comments[recadoId].append(parseComment(c));
            emit commentsChanged();
            emit commentPosted(recadoId);
            return;
        }
        const QString err = res.value(QStringLiteral("error")).toString();
        emit commentFailed(recadoId, err.isEmpty() ? QStringLiteral("network") : err);
    });
}

namespace {
const char* const kReacaoKeys[kReacoes] = { "heart", "fire", "clap", "laugh" };

Reacoes parseReactions(const QJsonObject& o)
{
    Reacoes r;
    for (int i = 0; i < kReacoes; ++i) r.count[i] = o.value(QLatin1String(kReacaoKeys[i])).toInt();
    for (const QJsonValue& v : o.value(QStringLiteral("mine")).toArray())
        for (int i = 0; i < kReacoes; ++i)
            if (v.toString() == QLatin1String(kReacaoKeys[i])) r.mine[i] = true;
    return r;
}
}

QString RecadosFeed::apiBase() const
{
    QString u = commentsUrl();
    if (u.endsWith(QLatin1String("/comments"))) u.chop(9);
    return u;
}

QString RecadosFeed::clientId()
{
    QSettings qs;
    QString id = qs.value(QStringLiteral("recados/clientId")).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        qs.setValue(QStringLiteral("recados/clientId"), id);
    }
    return id;
}

void RecadosFeed::fetchReactions()
{
    if (!commentsEnabled()) return;
    QUrl url(apiBase() + QStringLiteral("/reactions"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("me"), clientId());
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("qenna-writer-recados"));
    req.setTransferTimeout(15000);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    QNetworkReply* reply = network()->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) return;
        m_reactions.clear();
        const QJsonObject all = doc.object();
        for (auto it = all.begin(); it != all.end(); ++it) m_reactions.insert(it.key(), parseReactions(it.value().toObject()));
        emit reactionsChanged(QString());
    });
}

void RecadosFeed::toggleReaction(const QString& recadoId, Reacao kind)
{
    if (!commentsEnabled()) return;
    const int k = int(kind);
    // na hora, sem esperar a rede; a resposta do servidor acerta os números
    Reacoes& r = m_reactions[recadoId];
    r.mine[k] = !r.mine[k];
    r.count[k] = qMax(0, r.count[k] + (r.mine[k] ? 1 : -1));
    emit reactionsChanged(recadoId);

    QNetworkRequest req{QUrl(apiBase() + QStringLiteral("/reactions"))};
    req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("qenna-writer-recados"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/json"));
    req.setTransferTimeout(15000);
    QJsonObject o;
    o.insert(QStringLiteral("recado"), recadoId);
    o.insert(QStringLiteral("kind"), QLatin1String(kReacaoKeys[k]));
    o.insert(QStringLiteral("client"), clientId());
    QNetworkReply* reply = network()->post(req, QJsonDocument(o).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, recadoId, k]() {
        reply->deleteLater();
        const QJsonObject res = QJsonDocument::fromJson(reply->readAll()).object();
        if (res.value(QStringLiteral("ok")).toBool()) {
            m_reactions[recadoId] = parseReactions(res.value(QStringLiteral("reactions")).toObject());
        } else {
            Reacoes& r = m_reactions[recadoId];   // não foi: desfaz
            r.mine[k] = !r.mine[k];
            r.count[k] = qMax(0, r.count[k] + (r.mine[k] ? 1 : -1));
        }
        emit reactionsChanged(recadoId);
    });
}

bool RecadosFeed::isAuthor() const
{
    return !QSettings().value(QStringLiteral("recados/adminKey")).toString().isEmpty();
}

void RecadosFeed::setAdminKey(const QString& key)
{
    if (key.isEmpty()) QSettings().remove(QStringLiteral("recados/adminKey"));
    else QSettings().setValue(QStringLiteral("recados/adminKey"), key);
    emit commentsChanged();   // o "apagar" aparece ou some
}

void RecadosFeed::deleteComment(const QString& recadoId, qint64 commentId)
{
    const QString key = QSettings().value(QStringLiteral("recados/adminKey")).toString();
    if (!commentsEnabled() || key.isEmpty()) return;
    QNetworkRequest req{QUrl(commentsUrl() + QLatin1Char('/') + QString::number(commentId))};
    req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("qenna-writer-recados"));
    req.setRawHeader("Authorization", "Bearer " + key.toUtf8());
    req.setTransferTimeout(15000);
    QNetworkReply* reply = network()->deleteResource(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, recadoId, commentId]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 403) { setAdminKey(QString()); emit adminKeyRejected(); return; }
        if (reply->error() != QNetworkReply::NoError) return;
        QList<Comentario>& list = m_comments[recadoId];   // o comentário e as respostas dele
        list.erase(std::remove_if(list.begin(), list.end(), [commentId](const Comentario& c) { return c.id == commentId || c.parent == commentId; }), list.end());
        emit commentsChanged();
    });
}

void RecadosFeed::banComment(const QString& recadoId, qint64 commentId)
{
    const QString key = QSettings().value(QStringLiteral("recados/adminKey")).toString();
    if (!commentsEnabled() || key.isEmpty()) return;
    QNetworkRequest req{QUrl(apiBase() + QStringLiteral("/bans/") + QString::number(commentId))};
    req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("qenna-writer-recados"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/json"));
    req.setRawHeader("Authorization", "Bearer " + key.toUtf8());
    req.setTransferTimeout(15000);
    QNetworkReply* reply = network()->post(req, QByteArray("{}"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, recadoId, commentId]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 403) { setAdminKey(QString()); emit adminKeyRejected(); return; }
        if (reply->error() != QNetworkReply::NoError) return;
        Q_UNUSED(recadoId); Q_UNUSED(commentId);
        fetchComments();   // o ban leva tudo o que a pessoa escreveu, em qualquer recado
    });
}

void RecadosFeed::saveState()
{
    QSettings qs;
    qs.setValue(QStringLiteral("recados/read"), QStringList(m_read.begin(), m_read.end()));
    qs.setValue(QStringLiteral("recados/dismissed"), QStringList(m_dismissedIds.begin(), m_dismissedIds.end()));
}
