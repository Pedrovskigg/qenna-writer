#include "Thesaurus.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

namespace {

// Arquivos do repositório de dicionários do LibreOffice. Nomes não seguem um
// padrão único (o pt_BR não tem sufixo de versão, o espanhol não tem região),
// então cada um é mapeado na mão.
struct RemoteEntry { const char* lang; const char* path; };
const RemoteEntry kRemotes[] = {
    { "pt_BR", "pt_BR/th_pt_BR.dat" },
    { "en_US", "en/th_en_US_v2.dat" },
    { "es_ES", "es/th_es_v2.dat" },
};

const char* kRemoteBase = "https://raw.githubusercontent.com/LibreOffice/dictionaries/master/";

} // namespace

Thesaurus::Thesaurus(QObject* parent) : QObject(parent) {}

Thesaurus::~Thesaurus() { cancelDownload(); }

bool Thesaurus::isLanguageSupported(const QString& code)
{
    for (const RemoteEntry& e : kRemotes)
        if (code == QLatin1String(e.lang)) return true;
    return false;
}

QString Thesaurus::remoteUrlFor(const QString& code)
{
    for (const RemoteEntry& e : kRemotes)
        if (code == QLatin1String(e.lang))
            return QLatin1String(kRemoteBase) + QLatin1String(e.path);
    return QString();
}

void Thesaurus::setLanguage(const QString& code)
{
    if (m_lang == code) return;
    cancelDownload();
    m_lang = code;
    m_indexed = false;
    m_offsets.clear();
    emit readyChanged();
}

QString Thesaurus::dataDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
         + QStringLiteral("/thesaurus");
}

QString Thesaurus::dataPath() const
{
    if (m_lang.isEmpty()) return QString();
    const QString file = QStringLiteral("/th_") + m_lang + QStringLiteral(".dat");

    // O português vai embarcado: são 1,8 MB (base OpenWordnet-PT, muito melhor
    // curada que o MyThes do LibreOffice) e é o idioma da esmagadora maioria
    // dos usuários — não faz sentido fazer download pra isso. Os outros idiomas
    // continuam sob demanda.
    const QString bundled = QCoreApplication::applicationDirPath()
                          + QStringLiteral("/thesaurus") + file;
    if (QFile::exists(bundled)) return bundled;
#ifdef DEV_ASSETS_DIR
    const QString dev = QString::fromUtf8(DEV_ASSETS_DIR) + QStringLiteral("/thesaurus") + file;
    if (QFile::exists(dev)) return dev;
#endif
    return dataDir() + file;
}

bool Thesaurus::hasData() const
{
    const QString p = dataPath();
    return !p.isEmpty() && QFile::exists(p);
}

bool Thesaurus::buildIndex()
{
    if (m_indexed) return true;
    if (!hasData()) return false;

    QFile f(dataPath());
    if (!f.open(QIODevice::ReadOnly)) return false;

    m_offsets.clear();

    // A primeira linha é o nome da codificação. Só o pt_BR é UTF-8; os outros
    // podem vir em ISO-8859-1, então o decode é por linha, conforme declarado.
    const QByteArray encLine = f.readLine().trimmed();
    m_utf8 = encLine.isEmpty()
        || encLine.compare(QByteArrayLiteral("UTF-8"), Qt::CaseInsensitive) == 0;

    const bool isUtf8 = m_utf8;
    auto decode = [isUtf8](const QByteArray& raw) {
        return isUtf8 ? QString::fromUtf8(raw) : QString::fromLatin1(raw);
    };

    while (!f.atEnd()) {
        const qint64 pos = f.pos();
        const QByteArray raw = f.readLine();
        if (raw.isEmpty()) break;

        const int bar = raw.lastIndexOf('|');
        if (bar < 0) continue;

        bool okNum = false;
        const int senses = raw.mid(bar + 1).trimmed().toInt(&okNum);
        if (!okNum || senses <= 0) continue; // não é linha de entrada

        const QString word = decode(raw.left(bar)).trimmed().toLower();
        if (!word.isEmpty()) m_offsets.insert(word, pos);

        // Pula as linhas de significado: elas nunca são entradas, e testá-las
        // faria "(adj)|x|3" virar uma entrada fantasma.
        for (int i = 0; i < senses && !f.atEnd(); ++i) f.readLine();
    }

    f.close();
    m_indexed = !m_offsets.isEmpty();
    emit readyChanged();
    return m_indexed;
}

QList<Thesaurus::Sense> Thesaurus::lookup(const QString& word) const
{
    QList<Sense> out;
    if (!m_indexed) return out;

    const QString key = word.trimmed().toLower();
    if (key.isEmpty()) return out;
    const auto it = m_offsets.constFind(key);
    if (it == m_offsets.constEnd()) return out;

    QFile f(dataPath());
    if (!f.open(QIODevice::ReadOnly)) return out;
    if (!f.seek(it.value())) { f.close(); return out; }

    const QByteArray head = f.readLine();
    const int bar = head.lastIndexOf('|');
    if (bar < 0) { f.close(); return out; }
    const int senses = head.mid(bar + 1).trimmed().toInt();

    for (int i = 0; i < senses && !f.atEnd(); ++i) {
        const QByteArray rawLine = f.readLine();
        const QString line = (m_utf8 ? QString::fromUtf8(rawLine)
                                     : QString::fromLatin1(rawLine)).trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(QLatin1Char('|'));
        if (parts.isEmpty()) continue;

        Sense s;
        // O primeiro campo vem como "(categoria)palavra" — a categoria está
        // colada no primeiro sinônimo, sem barra separando.
        QString first = parts.takeFirst();
        const int close = first.indexOf(QLatin1Char(')'));
        if (first.startsWith(QLatin1Char('(')) && close > 0) {
            s.category = first.mid(1, close - 1);
            first = first.mid(close + 1);
        }
        s.label = first.trimmed();
        if (!s.label.isEmpty()) s.synonyms << s.label;

        for (const QString& p : parts) {
            const QString t = p.trimmed();
            // O próprio verbete costuma aparecer entre os sinônimos, e repetidos
            // são comuns dentro de uma acepção.
            if (t.isEmpty() || t.compare(key, Qt::CaseInsensitive) == 0) continue;
            if (!s.synonyms.contains(t, Qt::CaseInsensitive)) s.synonyms << t;
        }

        if (!s.synonyms.isEmpty()) out.append(s);
    }

    f.close();
    return out;
}

void Thesaurus::download()
{
    if (m_reply || m_lang.isEmpty()) return;
    const QString url = remoteUrlFor(m_lang);
    if (url.isEmpty()) {
        emit downloadFinished(false, tr("Não há dicionário de sinônimos para este idioma."));
        return;
    }
    QDir().mkpath(dataDir());

    if (!m_net) m_net = new QNetworkAccessManager(this);
    QNetworkRequest req{ QUrl(url) };
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_net->get(req);

    connect(m_reply, &QNetworkReply::downloadProgress, this, &Thesaurus::downloadProgress);
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply* r = m_reply;
        m_reply = nullptr;
        r->deleteLater();

        if (r->error() != QNetworkReply::NoError) {
            emit downloadFinished(false, r->errorString());
            return;
        }
        const QByteArray body = r->readAll();
        if (body.isEmpty()) {
            emit downloadFinished(false, tr("Download vazio."));
            return;
        }
        // Grava sempre em AppData, nunca em cima do que veio embarcado — o
        // dataPath() prefere o arquivo do app, e sobrescrevê-lo seria mexer na
        // instalação.
        const QString target = dataDir() + QStringLiteral("/th_") + m_lang + QStringLiteral(".dat");
        // QSaveFile: um download interrompido não deixa arquivo pela metade que
        // depois seria indexado como se estivesse completo.
        QSaveFile out(target);
        if (!out.open(QIODevice::WriteOnly) || out.write(body) != body.size() || !out.commit()) {
            emit downloadFinished(false, tr("Falha ao gravar o arquivo."));
            return;
        }
        m_indexed = false;
        m_offsets.clear();
        buildIndex();
        emit downloadFinished(true, QString());
    });
}

void Thesaurus::cancelDownload()
{
    if (!m_reply) return;
    QNetworkReply* r = m_reply;
    m_reply = nullptr;
    r->abort();
    r->deleteLater();
}
