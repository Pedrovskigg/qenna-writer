#include "SpellChecker.h"

#include <hunspell.hxx>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace {

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

void SpellChecker::setLanguage(const QString& langCode)
{
    if (m_lang == langCode) return;
    m_lang = langCode;
    loadHunspell();
    emit changed();
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
    const QString spellDir = assetsSpellDir();
    if (spellDir.isEmpty()) return result;
    QDir d(spellDir);
    const QStringList subdirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& sub : subdirs) {
        const QString aff = d.absoluteFilePath(sub + QStringLiteral("/index.aff"));
        const QString dic = d.absoluteFilePath(sub + QStringLiteral("/index.dic"));
        if (QFile::exists(aff) && QFile::exists(dic)) {
            result.append({sub, labelFor(sub)});
        }
    }
    return result;
}

void SpellChecker::loadHunspell()
{
    unloadHunspell();
    if (m_lang.isEmpty()) return;

    const QString spellDir = assetsSpellDir();
    if (spellDir.isEmpty()) return;

    const QString affPath = spellDir + QStringLiteral("/") + m_lang + QStringLiteral("/index.aff");
    const QString dicPath = spellDir + QStringLiteral("/") + m_lang + QStringLiteral("/index.dic");
    if (!QFile::exists(affPath) || !QFile::exists(dicPath)) {
        qWarning("SpellChecker: dictionary not found for '%s' at %s",
                 qUtf8Printable(m_lang), qUtf8Printable(spellDir));
        return;
    }

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
