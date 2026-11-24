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
#include <algorithm>

namespace {
const char* kRecadosUrl = "https://qennawriter.com/recados.json";

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

void RecadosFeed::saveState()
{
    QSettings qs;
    qs.setValue(QStringLiteral("recados/read"), QStringList(m_read.begin(), m_read.end()));
    qs.setValue(QStringLiteral("recados/dismissed"), QStringList(m_dismissedIds.begin(), m_dismissedIds.end()));
}
