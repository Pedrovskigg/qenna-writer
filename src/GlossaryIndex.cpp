#include "GlossaryIndex.h"

#include "DocCache.h"
#include "ProjectModel.h"
#include "ProjectStorage.h"

#include <QDateTime>
#include <QFileInfo>

namespace {
// Texto plano a partir do html do capítulo: quebras de parágrafo viram "\n",
// tags somem e as entidades comuns voltam a ser caractere. Mais leve que
// passar por QTextDocument, e o bastante pra contar palavras e achar frase.
QString htmlToPlain(const QString& html) {
    static const QRegularExpression kStyle(QStringLiteral("<(style|head)[^>]*>.*?</\\1>"),
                                           QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression kBreaks(QStringLiteral("</p>|</h[1-6]>|<br[^>]*>|</li>|</div>"),
                                            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kTags(QStringLiteral("<[^>]*>"));
    static const QRegularExpression kNum(QStringLiteral("&#(x?)([0-9a-fA-F]+);"));
    QString t = html;
    t.remove(kStyle);
    t.replace(kBreaks, QStringLiteral("\n"));
    t.remove(kTags);
    t.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    t.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    t.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    t.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    t.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    QString out;
    out.reserve(t.size());
    int last = 0;
    auto it = kNum.globalMatch(t);
    while (it.hasNext()) {
        const auto m = it.next();
        out += QStringView(t).mid(last, m.capturedStart() - last);
        bool ok = false;
        const uint code = m.captured(2).toUInt(&ok, m.captured(1).isEmpty() ? 10 : 16);
        if (ok && code > 0) out += QString::fromUcs4(reinterpret_cast<const char32_t*>(&code), 1);
        last = m.capturedEnd();
    }
    out += QStringView(t).mid(last);
    out.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return out;
}

bool isAllCaps(const QString& s) {
    bool letter = false;
    for (const QChar c : s) {
        if (c.isLetter()) { letter = true; if (c.isLower()) return false; }
    }
    return letter;
}

// A frase em volta da posição: do fim da frase anterior ao fim desta.
QString sentenceAround(const QString& text, int pos, int len) {
    auto isEnd = [](QChar c) { return c == QLatin1Char('.') || c == QLatin1Char('!') || c == QLatin1Char('?')
                                      || c == QChar(0x2026) || c == QLatin1Char('\n'); };
    int a = pos;
    while (a > 0 && !isEnd(text.at(a - 1)) && pos - a < 260) --a;
    int b = pos + len;
    while (b < text.size() && !isEnd(text.at(b)) && b - pos < 260) ++b;
    if (b < text.size() && text.at(b) != QLatin1Char('\n')) ++b;   // leva o ponto junto
    QString s = text.mid(a, b - a).simplified();
    if (a > 0 && pos - a >= 260) s.prepend(QChar(0x2026));
    return s;
}
}

GlossaryIndex::GlossaryIndex(ProjectModel* model, DocCache* cache, QObject* parent)
    : QObject(parent), m_model(model), m_cache(cache) {}

void GlossaryIndex::setProjectRoot(const QString& root) {
    if (m_root == root) return;
    m_root = root;
    m_plain.clear();
    m_usage.clear();
}

void GlossaryIndex::invalidate() { m_usage.clear(); }

QRegularExpression GlossaryIndex::patternFor(const QStringList& spellings) {
    QStringList alts;
    for (const QString& raw : spellings) {
        const QString s = raw.trimmed();
        if (s.isEmpty()) continue;
        QString esc = QRegularExpression::escape(s);
        esc.replace(QStringLiteral("\\ "), QStringLiteral("\\s+"));
        alts << (isAllCaps(s) ? esc : QStringLiteral("(?i:%1)").arg(esc));
    }
    if (alts.isEmpty()) return QRegularExpression();
    // Maior primeiro: "Comando Alto Leste" antes de "Comando".
    std::sort(alts.begin(), alts.end(), [](const QString& a, const QString& b) { return a.size() > b.size(); });
    return QRegularExpression(QStringLiteral("(?<![\\p{L}\\p{N}])(?:%1)(?![\\p{L}\\p{N}])").arg(alts.join(QLatin1Char('|'))),
                              QRegularExpression::UseUnicodePropertiesOption);
}

QString GlossaryIndex::plainFor(const Chapter& c) const {
    const QString key = DocCache::chapterKey(c.manuscriptId, c.id);
    QString sig, html;
    bool fromCache = false;
    if (m_cache && m_cache->has(key)) {
        html = m_cache->get(key);
        sig = QStringLiteral("m%1:%2").arg(html.size()).arg(qHash(html));
        fromCache = true;
    } else if (!m_root.isEmpty() && !c.file.isEmpty()) {
        const QFileInfo fi(ProjectStorage::joinPath(m_root, c.file));
        sig = QStringLiteral("f%1:%2").arg(fi.lastModified().toMSecsSinceEpoch()).arg(fi.size());
    }
    auto it = m_plain.constFind(c.id);
    if (it != m_plain.constEnd() && it->sig == sig) return it->text;
    if (!fromCache && !m_root.isEmpty() && !c.file.isEmpty()) {
        bool ok = false;
        html = ProjectStorage::readChapter(m_root, c.file, &ok);
        if (!ok) html.clear();
    }
    const QString plain = htmlToPlain(html);
    m_plain.insert(c.id, Plain{ sig, plain });
    return plain;
}

GlossaryIndex::Usage GlossaryIndex::usage(const QStringList& spellings) const {
    Usage u;
    if (!m_model || spellings.isEmpty()) return u;
    const QString key = spellings.join(QChar(0x1f));
    auto hit = m_usage.constFind(key);
    if (hit != m_usage.constEnd()) return *hit;
    const QRegularExpression re = patternFor(spellings);
    if (!re.isValid() || re.pattern().isEmpty()) return u;
    for (const Manuscript& ms : m_model->manuscripts()) {
        for (const Chapter* c : m_model->orderedChaptersForManuscript(ms.id)) {
            const QString text = plainFor(*c);
            if (text.isEmpty()) continue;
            int n = 0;
            auto it = re.globalMatch(text);
            while (it.hasNext()) {
                const auto m = it.next();
                if (u.firstChapterId.isEmpty()) {
                    u.firstChapterId = c->id;
                    u.firstDocKey = DocCache::chapterKey(c->manuscriptId, c->id);
                    u.firstSentence = sentenceAround(text, m.capturedStart(), m.capturedLength());
                    u.firstLabel = m_model->chapterDisplayLabel(*c);
                }
                ++n;
            }
            if (n > 0) { u.count += n; u.chapterIds << c->id; }
        }
    }
    m_usage.insert(key, u);
    return u;
}
