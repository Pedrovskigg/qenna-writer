#include "SpellChecker.h"

#include <QVector>

#include <memory>

#include <hunspell.hxx>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

namespace {

// Dicionários que o app sabe baixar do repositório do LibreOffice (o mesmo de
// onde saíram os embarcados e o thesaurus). Os nomes não seguem padrão — o
// francês mora numa subpasta e não tem região —, então cada um é mapeado na
// mão. Os três embarcados ficam de fora: já vêm com o app.
struct RemoteDict { const char* lang; const char* aff; const char* dic; };
const RemoteDict kRemoteDicts[] = {
    { "it_IT", "it_IT/it_IT.aff", "it_IT/it_IT.dic" },
    { "fr_FR", "fr_FR/dictionaries/fr.aff", "fr_FR/dictionaries/fr.dic" },
};
const char* kRemoteDictBase = "https://raw.githubusercontent.com/LibreOffice/dictionaries/master/";

const RemoteDict* remoteDictFor(const QString& code)
{
    for (const RemoteDict& r : kRemoteDicts)
        if (code == QLatin1String(r.lang)) return &r;
    return nullptr;
}

QString hunspellPathString(const QString& path)
{
    // Hunspell em Windows usa fopen() com const char*. Mantemos paths ASCII-safe
    // já que vivem em src/assets/spell/ (sem caracteres especiais).
    return QDir::toNativeSeparators(path);
}

QString labelFor(const QString& code)
{
    if (code == QLatin1String("pt_BR")) return QStringLiteral("Português (Brasil)");
    if (code == QLatin1String("pt_PT")) return QStringLiteral("Português (Portugal)");
    if (code == QLatin1String("en_US")) return QStringLiteral("English (US)");
    if (code == QLatin1String("en_GB")) return QStringLiteral("English (UK)");
    if (code == QLatin1String("es_ES")) return QStringLiteral("Español");
    if (code == QLatin1String("fr_FR")) return QStringLiteral("Français");
    if (code == QLatin1String("de_DE")) return QStringLiteral("Deutsch");
    if (code == QLatin1String("it_IT")) return QStringLiteral("Italiano");
    if (code == QLatin1String("ru_RU")) return QStringLiteral("Русский");
    if (code == QLatin1String("pl_PL")) return QStringLiteral("Polski");
    return code;
}

} // namespace

SpellChecker::SpellChecker(QObject* parent)
    : QObject(parent)
{
}

SpellChecker::~SpellChecker()
{
    unloadHunspell();
}

QString SpellChecker::dictionaryForAppLanguage()
{
    const QString app = QSettings().value(QStringLiteral("app/language")).toString();
    const QString lang = app.left(2).toLower();
    if (lang == QLatin1String("pt")) return QStringLiteral("pt_BR");
    if (lang == QLatin1String("es")) return QStringLiteral("es_ES");
    if (lang == QLatin1String("it")) return QStringLiteral("it_IT");
    if (lang == QLatin1String("fr")) return QStringLiteral("fr_FR");
    // Sem preferência gravada, o main.cpp resolve a interface para inglês.
    return QStringLiteral("en_US");
}

QString SpellChecker::resolveLanguage(const QString& setting)
{
    return setting == followAppValue() ? dictionaryForAppLanguage() : setting;
}

QString SpellChecker::labelForLanguage(const QString& code)
{
    return labelFor(code);
}

QString SpellChecker::downloadedSpellDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
         + QStringLiteral("/spell");
}

QString SpellChecker::dictionaryDir(const QString& code)
{
    if (code.isEmpty()) return QString();
    // Embarcado primeiro: o baixado nunca substitui o que veio com o app.
    for (const QString& base : { assetsSpellDir(), downloadedSpellDir() }) {
        if (base.isEmpty()) continue;
        const QString dir = base + QStringLiteral("/") + code;
        if (QFile::exists(dir + QStringLiteral("/index.aff"))
            && QFile::exists(dir + QStringLiteral("/index.dic")))
            return dir;
    }
    return QString();
}

bool SpellChecker::isInstalled(const QString& code)
{
    return !dictionaryDir(code).isEmpty();
}

bool SpellChecker::isDownloadable(const QString& code)
{
    return remoteDictFor(code) != nullptr;
}

void SpellChecker::setLanguage(const QString& langCode)
{
    if (m_lang == langCode) return;
    m_lang = langCode;
    loadHunspell();
    // Idioma sem dicionário em disco que dá pra baixar: baixa e ativa sozinho.
    // Até terminar, o corretor fica desligado (nada grifado) em vez de corrigir
    // italiano com dicionário português.
    if (!m_hunspell && !m_lang.isEmpty() && !isInstalled(m_lang) && isDownloadable(m_lang))
        downloadDictionary(m_lang);
    emit changed();
}

void SpellChecker::downloadDictionary(const QString& code)
{
    const RemoteDict* remote = remoteDictFor(code);
    if (!remote || m_downloadingLang == code) return;
    m_downloadingLang = code;
    if (!m_net) m_net = new QNetworkAccessManager(this);
    emit dictionaryDownloadStarted(code);

    // Dois arquivos; só grava quando os DOIS chegaram. Um .aff sem o .dic
    // correspondente faria isInstalled() mentir e o download nunca mais rodar.
    struct State { QByteArray aff, dic; int pending = 2; bool failed = false; QString error; };
    auto state = std::make_shared<State>();

    auto fetch = [this, code, state](const char* path, bool isAff) {
        QNetworkRequest req{ QUrl(QLatin1String(kRemoteDictBase) + QLatin1String(path)) };
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* reply = m_net->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, code, state, reply, isAff]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                state->failed = true;
                state->error = reply->errorString();
            } else {
                (isAff ? state->aff : state->dic) = reply->readAll();
            }
            if (--state->pending > 0) return;

            m_downloadingLang.clear();
            if (!state->failed && (state->aff.isEmpty() || state->dic.isEmpty())) {
                state->failed = true;
                state->error = tr("Download vazio.");
            }
            if (!state->failed) {
                const QString dir = downloadedSpellDir() + QStringLiteral("/") + code;
                QDir().mkpath(dir);
                // QSaveFile: download interrompido não deixa arquivo pela metade
                // que depois seria carregado como se estivesse completo. O .dic
                // vai por último — é ele que isInstalled() confere junto do .aff.
                for (const auto& [name, bytes] : { std::pair{ QStringLiteral("index.aff"), state->aff },
                                                   std::pair{ QStringLiteral("index.dic"), state->dic } }) {
                    QSaveFile out(dir + QStringLiteral("/") + name);
                    if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size()
                        || !out.commit()) {
                        state->failed = true;
                        state->error = tr("Falha ao gravar o arquivo.");
                        break;
                    }
                }
            }
            if (!state->failed && m_lang == code) {
                loadHunspell();
                emit changed();
            }
            emit dictionaryDownloadFinished(code, !state->failed, state->error);
        });
    };
    fetch(remote->aff, true);
    fetch(remote->dic, false);
}

void SpellChecker::setProjectRoot(const QString& root)
{
    if (m_projectRoot == root) return;
    m_projectRoot = root;
    m_personal.clear();
    loadPersonalDictionary();
    // Re-adiciona personal words ao Hunspell (em RAM).
    if (m_hunspell) {
        for (const QString& w : m_personal) {
            m_hunspell->add(w.toStdString());
        }
    }
    emit changed();
}

bool SpellChecker::isCorrect(const QString& word) const
{
    if (!m_hunspell) return true;
    if (word.isEmpty()) return true;
    if (m_glossary.contains(word)) return true;
    if (m_personal.contains(word)) return true;
    return m_hunspell->spell(word.toStdString());
}

QStringList SpellChecker::suggest(const QString& word) const
{
    QStringList result;
    if (!m_hunspell || word.isEmpty()) return result;
    const auto suggestions = m_hunspell->suggest(word.toStdString());
    result.reserve(static_cast<int>(suggestions.size()));
    for (const auto& s : suggestions) {
        result.append(QString::fromStdString(s));
    }
    return result;
}

QString SpellChecker::stemOf(const QString& word) const
{
    if (!m_hunspell || word.isEmpty()) return QString();
    const std::vector<std::string> roots =
        m_hunspell->stem(word.toLower().toStdString());
    if (roots.empty()) return QString();
    // O hunspell pode devolver várias raízes (ambiguidade morfológica). A
    // primeira é a análise mais provável.
    return QString::fromStdString(roots.front());
}

QStringList SpellChecker::stemsOf(const QString& word) const
{
    QStringList out;
    if (!m_hunspell || word.isEmpty()) return out;
    for (const std::string& r : m_hunspell->stem(word.toLower().toStdString()))
        out << QString::fromStdString(r);
    return out;
}

QString SpellChecker::inflectLike(const QString& lemma, const QString& inflected,
                                  const QString& baseLemma) const
{
    if (!m_hunspell) return QString();
    const QString target = lemma.trimmed().toLower();
    const QString form = inflected.trimmed().toLower();
    const QString base = baseLemma.trimmed().toLower();
    if (target.isEmpty() || form.isEmpty() || base.isEmpty()) return QString();
    if (form == base) return target; // não estava flexionado

    static const QStringList kInf = { QStringLiteral("ar"), QStringLiteral("er"),
                                      QStringLiteral("ir"), QStringLiteral("or") };

    // Radical vem de CORTAR A TERMINAÇÃO DO INFINITIVO, não do prefixo comum
    // entre lema e forma: "chegar"/"chegando" compartilham "chega", o que
    // deixaria o sufixo como "ndo" e não casaria com nada.
    QString baseStem;
    for (const QString& inf : kInf) {
        if (base.endsWith(inf)) { baseStem = base.left(base.size() - inf.size()); break; }
    }
    if (baseStem.isEmpty() || !form.startsWith(baseStem)) return QString();

    const QString formSuffix = form.mid(baseStem.size()); // "ando", "ou", "aria"
    if (formSuffix.isEmpty()) return QString();

    // Mesmo tempo escrito nas três conjugações — sem isto um sinônimo de outra
    // conjugação ("seguir" para "chegando") viraria "seguando".
    static const QVector<QStringList> kGroups = {
        { QStringLiteral("ar"),    QStringLiteral("er"),    QStringLiteral("ir")    },
        { QStringLiteral("ando"),  QStringLiteral("endo"),  QStringLiteral("indo")  },
        { QStringLiteral("ado"),   QStringLiteral("ido")   },
        { QStringLiteral("ada"),   QStringLiteral("ida")   },
        { QStringLiteral("ados"),  QStringLiteral("idos")  },
        { QStringLiteral("adas"),  QStringLiteral("idas")  },
        { QStringLiteral("ou"),    QStringLiteral("eu"),    QStringLiteral("iu")    },
        { QStringLiteral("ava"),   QStringLiteral("ia")    },
        { QStringLiteral("avam"),  QStringLiteral("iam")   },
        { QStringLiteral("ava"),   QStringLiteral("ia")    },
        { QStringLiteral("aria"),  QStringLiteral("eria"), QStringLiteral("iria")  },
        { QStringLiteral("ariam"), QStringLiteral("eriam"),QStringLiteral("iriam") },
        { QStringLiteral("aram"),  QStringLiteral("eram"), QStringLiteral("iram")  },
        { QStringLiteral("asse"),  QStringLiteral("esse"), QStringLiteral("isse")  },
        { QStringLiteral("assem"), QStringLiteral("essem"),QStringLiteral("issem") },
        { QStringLiteral("ei"),    QStringLiteral("i")     },
        { QStringLiteral("amos"),  QStringLiteral("emos"), QStringLiteral("imos")  },
        { QStringLiteral("aremos"),QStringLiteral("eremos"),QStringLiteral("iremos")},
        { QStringLiteral("ará"),   QStringLiteral("erá"),  QStringLiteral("irá")   },
        { QStringLiteral("arão"),  QStringLiteral("erão"), QStringLiteral("irão")  },
        { QStringLiteral("am"),    QStringLiteral("em")    },
        { QStringLiteral("a"),     QStringLiteral("e")     },
        { QStringLiteral("as"),    QStringLiteral("es")    },
    };

    QStringList suffixes { formSuffix };
    for (const QStringList& g : kGroups) {
        if (!g.contains(formSuffix)) continue;
        for (const QString& alt : g)
            if (!suffixes.contains(alt)) suffixes << alt;
    }

    QString targetStem;
    for (const QString& inf : kInf) {
        if (target.endsWith(inf)) { targetStem = target.left(target.size() - inf.size()); break; }
    }
    // Alvo que não é verbo não tem como receber flexão verbal.
    if (targetStem.isEmpty()) return QString();

    for (const QString& suf : suffixes) {
        const QString cand = targetStem + suf;
        if (cand == target) continue;
        // O dicionário é o juiz: nunca sai palavra inventada.
        if (isCorrect(cand)) return cand;
    }
    return QString();
}

bool SpellChecker::addToPersonalDictionary(const QString& word)
{
    const QString trimmed = word.trimmed();
    if (trimmed.isEmpty()) return false;
    if (m_personal.contains(trimmed)) return true;
    m_personal.insert(trimmed);
    if (m_hunspell) {
        m_hunspell->add(trimmed.toStdString());
    }
    savePersonalDictionary();
    emit changed();
    return true;
}

void SpellChecker::setGlossaryWords(const QSet<QString>& words)
{
    if (m_glossary == words) return;
    m_glossary = words;
    emit changed();
}

QList<QPair<QString, QString>> SpellChecker::availableLanguages()
{
    QList<QPair<QString, QString>> result;
    QStringList vistos;
    for (const QString& base : { assetsSpellDir(), downloadedSpellDir() }) {
        if (base.isEmpty()) continue;
        QDir d(base);
        const QStringList subdirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString& sub : subdirs) {
            if (vistos.contains(sub) || !isInstalled(sub)) continue;
            vistos.append(sub);
            result.append({sub, labelFor(sub)});
        }
    }
    std::sort(result.begin(), result.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    return result;
}

void SpellChecker::loadHunspell()
{
    unloadHunspell();
    if (m_lang.isEmpty()) return;

    const QString dir = dictionaryDir(m_lang);
    if (dir.isEmpty()) {
        qWarning("SpellChecker: dictionary not found for '%s'", qUtf8Printable(m_lang));
        return;
    }
    const QString affPath = dir + QStringLiteral("/index.aff");
    const QString dicPath = dir + QStringLiteral("/index.dic");

    const QByteArray affBytes = hunspellPathString(affPath).toLocal8Bit();
    const QByteArray dicBytes = hunspellPathString(dicPath).toLocal8Bit();
    m_hunspell = new Hunspell(affBytes.constData(), dicBytes.constData());

    // Reaplica palavras do dicionário pessoal já carregado.
    for (const QString& w : m_personal) {
        m_hunspell->add(w.toStdString());
    }
}

void SpellChecker::unloadHunspell()
{
    if (m_hunspell) {
        delete m_hunspell;
        m_hunspell = nullptr;
    }
}

QString SpellChecker::personalDictPath() const
{
    if (m_projectRoot.isEmpty()) return QString();
    return m_projectRoot + QStringLiteral("/spell/custom.dic");
}

void SpellChecker::loadPersonalDictionary()
{
    const QString path = personalDictPath();
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (!line.isEmpty() && !line.startsWith(QLatin1Char('#'))) {
            m_personal.insert(line);
        }
    }
}

void SpellChecker::savePersonalDictionary()
{
    const QString path = personalDictPath();
    if (path.isEmpty()) return;
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning("SpellChecker: failed to write %s", qUtf8Printable(path));
        return;
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << "# Qenna Writer — dicionário pessoal do projeto\n";
    QStringList sorted = m_personal.values();
    sorted.sort(Qt::CaseInsensitive);
    for (const QString& w : sorted) {
        ts << w << "\n";
    }
}

QString SpellChecker::assetsSpellDir()
{
    QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/spell");
    if (QDir(dir).exists()) return dir;
#ifdef DEV_ASSETS_DIR
    dir = QString::fromUtf8(DEV_ASSETS_DIR) + QStringLiteral("/spell");
    if (QDir(dir).exists()) return dir;
#endif
    return QString();
}
