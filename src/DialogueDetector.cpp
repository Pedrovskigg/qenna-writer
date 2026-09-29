#include "DialogueDetector.h"

#include "ScreenplayFormat.h"

#include <QTextBlock>
#include <QTextDocument>

#include <algorithm>

namespace {

constexpr auto kUnicode = QRegularExpression::UseUnicodePropertiesOption;

// ---------------------------------------------------------------------------
// Corte do parágrafo em fala / narração
// ---------------------------------------------------------------------------

struct Seg {
    bool speech = false;
    QString text;
};

bool isOpenQuote(QChar c)
{
    return c == QLatin1Char('"') || c == QChar(0x201C) || c == QChar(0x201E)
        || c == QChar(0x00AB) || c == QChar(0x2039) || c == QChar(0x2018);
}

QChar closeQuoteFor(QChar open)
{
    switch (open.unicode()) {
    case 0x201C: return QChar(0x201D); // “ ”
    case 0x201E: return QChar(0x201C); // „ “
    case 0x00AB: return QChar(0x00BB); // « »
    case 0x2039: return QChar(0x203A); // ‹ ›
    case 0x2018: return QChar(0x2019); // ‘ ’
    default:     return QLatin1Char('"');
    }
}

// Fecho da aspa a partir de `from`. Com ‘ ’ o fecho é o mesmo caractere do
// apóstrofo ("I’m"), então só vale se não vier letra logo depois.
int findClose(const QString& s, QChar close, int from)
{
    for (int j = from; j < s.size(); ++j) {
        if (s.at(j) != close) continue;
        if (close == QChar(0x2019) && j + 1 < s.size() && s.at(j + 1).isLetter()) continue;
        return j;
    }
    return -1;
}

// "— Je pars, dit Marie." / "— Vamos, disse Ana, e saiu." — a tag mora
// dentro do trecho de fala, depois de uma vírgula. Divide em fala/narr/fala.
void appendSpeechWithIncise(QVector<Seg>& out, const QString& speech, const DialogueLang& lang)
{
    const QRegularExpressionMatch m = lang.incise.match(speech);
    if (!m.hasMatch() || m.capturedStart() == 0) {
        out.append({ true, speech });
        return;
    }
    const QString before = speech.left(m.capturedStart()).trimmed();
    const int tagStart = m.capturedStart() + 1; // depois da vírgula
    static const QRegularExpression stop(QStringLiteral("[,.;!?…]"));
    const QRegularExpressionMatch sm = stop.match(speech, m.capturedEnd());
    const int tagEnd = sm.hasMatch() ? sm.capturedStart() : speech.size();
    const QString tag = speech.mid(tagStart, tagEnd - tagStart).trimmed();
    QString rest = sm.hasMatch() ? speech.mid(tagEnd + 1).trimmed() : QString();

    if (!before.isEmpty()) out.append({ true, before });
    if (!tag.isEmpty()) out.append({ false, tag });
    bool restHasLetter = false;
    for (const QChar c : rest) if (c.isLetterOrNumber()) { restHasLetter = true; break; }
    if (restHasLetter) out.append({ true, rest });
}

QVector<Seg> segment(const QString& paragraph, const DialogueLang& lang, bool quotesDialogue)
{
    QVector<Seg> out;
    QString s = paragraph.trimmed();
    if (s.isEmpty()) return out;

    // Abertura com hífen, "--", meia-risca ou barra vira travessão.
    static const QRegularExpression openNorm(QStringLiteral("^(?:--?|\\x{2013}|\\x{2015})\\s*"));
    if (s.at(0) != QChar(0x2014)) {
        const QRegularExpressionMatch m = openNorm.match(s);
        if (m.hasMatch()) s = QStringLiteral("— ") + s.mid(m.capturedEnd());
    }

    if (s.at(0) == QChar(0x2014)) {
        static const QRegularExpression splitRe(
            QStringLiteral("\\s*[\\x{2014}\\x{2013}\\x{2015}]\\s*|\\s+--?\\s+"));
        QStringList parts = s.split(splitRe);
        if (!parts.isEmpty() && parts.first().trimmed().isEmpty()) parts.removeFirst();
        bool speech = true;
        for (const QString& raw : parts) {
            const QString part = raw.trimmed();
            if (!part.isEmpty()) {
                if (speech) appendSpeechWithIncise(out, part, lang);
                else out.append({ false, part });
            }
            speech = !speech;
        }
        return out;
    }

    if (isOpenQuote(s.at(0))) {
        int i = 0;
        while (i < s.size()) {
            const QChar c = s.at(i);
            if (isOpenQuote(c)) {
                int j = findClose(s, closeQuoteFor(c), i + 1);
                if (j < 0) j = s.size();
                const QString sp = s.mid(i + 1, j - i - 1).trimmed();
                // Francês abre « e só fecha no fim da troca: a tag fica dentro.
                if (!sp.isEmpty()) appendSpeechWithIncise(out, sp, lang);
                i = j + 1;
            } else {
                int j = s.size();
                for (int k = i; k < s.size(); ++k)
                    if (isOpenQuote(s.at(k))) { j = k; break; }
                const QString nr = s.mid(i, j - i).trimmed();
                if (!nr.isEmpty()) out.append({ false, nr });
                i = j;
            }
        }
        return out;
    }

    // Travessão no meio: "Ana olhou pra ele e disse: — Vamos."
    static const QRegularExpression midDash(
        QStringLiteral(":\\s*[\\x{2014}\\x{2013}]\\s*(?=\\p{L})"), kUnicode);
    const QRegularExpressionMatch m = midDash.match(s);
    if (m.hasMatch()) {
        out.append({ false, s.left(m.capturedStart() + 1).trimmed() });
        out += segment(QStringLiteral("— ") + s.mid(m.capturedEnd()), lang, quotesDialogue);
        return out;
    }

    // Ação antes da fala, depois de uma frase fechada: "Oda shrugged. “Fair
    // has nothing to do with it.”" / "Oda sorrise, stanca. «Hai paura?»".
    // O gesto é a tag. Só em livro de diálogo com aspas (quotesDialogue): com
    // travessão, aspas no meio da narração são pensamento, mensagem ou grito
    // ("“Medo não existe, medo não machuca”"), não fala.
    static const QRegularExpression beat(
        QStringLiteral("[.!?\\x{2026}]\\s+(?=[\"\\x{201C}\\x{201E}\\x{00AB}\\x{2018}]\\s*\\p{L})"),
        kUnicode);
    const QRegularExpressionMatch b = quotesDialogue ? beat.match(s) : QRegularExpressionMatch();
    if (b.hasMatch()) {
        const QVector<Seg> rest = segment(s.mid(b.capturedEnd()), lang, quotesDialogue);
        bool hasSpeech = false;
        for (const Seg& r : rest) if (r.speech) { hasSpeech = true; break; }
        if (hasSpeech) {
            out.append({ false, s.left(b.capturedStart() + 1).trimmed() });
            out += rest;
            return out;
        }
    }

    out.append({ false, s });
    return out;
}

// ---------------------------------------------------------------------------
// Nomes no texto
// ---------------------------------------------------------------------------

struct Hit {
    int pos;
    int end;
    QString id;
};

QVector<Hit> mentions(const QString& s, const DialogueDetector::Cast& cast)
{
    QVector<Hit> hits;
    if (s.isEmpty()) return hits;
    for (const DialogueDetector::Token& t : cast.tokens) {
        auto it = t.re.globalMatch(s);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            hits.append({ int(m.capturedStart()), int(m.capturedEnd()), t.id });
        }
    }
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        if (a.pos != b.pos) return a.pos < b.pos;
        return (a.end - a.pos) > (b.end - b.pos);
    });
    QVector<Hit> out;
    int last = -1;
    for (const Hit& h : hits) {
        if (h.pos >= last) { out.append(h); last = h.end; }
    }
    return out;
}

QString cleanWord(const QString& w)
{
    static const QRegularExpression edge(
        QStringLiteral("^[^\\p{L}\\p{N}']+|[^\\p{L}\\p{N}']+$"), kUnicode);
    QString c = w;
    c.remove(edge);
    return c;
}

// Palavras da tag pra achar pronome: "dit-il" também vira "dit", "il".
QStringList pronounWords(const QString& s)
{
    static const QRegularExpression sep(QStringLiteral("[\\s\\-]+"));
    QStringList out;
    for (const QString& w : s.split(sep, Qt::SkipEmptyParts)) out.append(cleanWord(w).toLower());
    return out;
}

// ---------------------------------------------------------------------------
// Sujeito da tag
// ---------------------------------------------------------------------------

struct Subject {
    enum Kind { None, Name, Pronoun, Extra, FirstPerson } kind = None;
    QString id;      // Name
    QChar gender;    // Pronoun
    QString label;   // Extra
};

QString firstClauseOf(const QString& narr)
{
    static const QRegularExpression stop(QStringLiteral("[.!?;…]"));
    const QRegularExpressionMatch m = stop.match(narr);
    return m.hasMatch() ? narr.left(m.capturedStart()) : narr;
}

Subject subjectOf(const QString& narr, const DialogueDetector::Cast& cast, const DialogueLang& lang)
{
    Subject out;
    const QString clause = firstClauseOf(narr);
    const QStringList words = clause.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (words.isEmpty()) return out;

    // "A voz dela", "Her voice" — possessivo aponta quem fala.
    for (int i = 0; i < qMin(4, int(words.size())); ++i) {
        const QChar g = lang.possessives.value(cleanWord(words.at(i)).toLower());
        if (g.isNull()) continue;
        if (lang.possessiveLeads ? i == 0 : i > 0) {
            out.kind = Subject::Pronoun;
            out.gender = g;
            return out;
        }
    }

    // Nome na primeira oração, sem preposição antes ("olhou para Klara"
    // não conta). "A voz de Klara" conta. "O João disse" / "disse a Klara"
    // — artigo antes do nome, no começo ou logo depois do verbo — conta.
    for (const Hit& h : mentions(clause, cast)) {
        const QStringList before = clause.left(h.pos).split(QRegularExpression(QStringLiteral("\\s+")),
                                                             Qt::SkipEmptyParts);
        const QString prev = before.isEmpty() ? QString() : cleanWord(before.last()).toLower();
        if (!prev.isEmpty() && lang.preps.contains(prev)) {
            const QString prev2 = before.size() >= 2 ? cleanWord(before.at(before.size() - 2)).toLower()
                                                     : QString();
            const bool voiceOf = !prev2.isEmpty() && lang.voiceNouns.contains(prev2);
            const bool articleAsSubject = lang.articles.contains(prev)
                && (before.size() == 1 || lang.tagVerbs.contains(prev2));
            if (!voiceOf && !articleAsSubject) continue;
        }
        out.kind = Subject::Name;
        out.id = h.id;
        return out;
    }

    const QStringList pw = pronounWords(clause);
    for (int i = 0; i < qMin(3, int(pw.size())); ++i) {
        const QChar g = lang.pronouns.value(pw.at(i));
        if (g.isNull()) continue;
        out.kind = Subject::Pronoun;
        out.gender = g;
        return out;
    }

    const QString w0 = cleanWord(words.at(0));
    const QString w0l = w0.toLower();
    const bool w0Comma = words.at(0).endsWith(QLatin1Char(','));
    // "O porteiro chamou", "Uma das garotas respondeu".
    if (words.size() > 1 && lang.articles.contains(w0l)) {
        out.kind = Subject::Extra;
        out.label = (w0l + QLatin1Char(' ') + cleanWord(words.at(1)).toLower()).trimmed();
        return out;
    }
    // "Éder atropelou", "Rosana disse" — nome próprio fora do elenco.
    if (words.size() > 1 && !w0.isEmpty() && w0.at(0).isUpper()
        && !lang.tagVerbs.contains(w0l) && !lang.pronouns.contains(w0l)
        && !lang.firstPerson.match(w0).hasMatch()
        && !words.at(1).isEmpty() && words.at(1).at(0).isLower()) {
        out.kind = Subject::Extra;
        out.label = w0;
        return out;
    }
    // "disse o homem", "said the old man".
    if (words.size() > 2 && !w0Comma && lang.articles.contains(cleanWord(words.at(1)).toLower())) {
        out.kind = Subject::Extra;
        out.label = cleanWord(words.at(1)).toLower() + QLatin1Char(' ') + cleanWord(words.at(2)).toLower();
        return out;
    }

    if (lang.firstPerson.match(clause.trimmed()).hasMatch()) {
        out.kind = Subject::FirstPerson;
        return out;
    }
    return out;
}

int wordCount(const QString& s)
{
    int n = 0;
    bool in = false;
    for (const QChar c : s) {
        const bool w = c.isLetterOrNumber();
        if (w && !in) ++n;
        in = w;
    }
    return n;
}

bool isExtraTurn(const QString& t) { return t.startsWith(QLatin1String("fig:")); }

void pushRecent(QStringList& recent, const QString& id, int at = 0)
{
    recent.removeAll(id);
    recent.insert(qMin(at, int(recent.size())), id);
}

} // namespace

// ---------------------------------------------------------------------------

DialogueDetector::Cast DialogueDetector::buildCast(const QList<Element>& elements)
{
    Cast cast;
    QSet<QString> ambiguous;
    QHash<QString, int> firstCount;
    for (const Element& e : elements) {
        if (e.type != QStringLiteral("character")) continue;
        const QStringList parts = e.name.trimmed().split(QRegularExpression(QStringLiteral("\\s+")),
                                                         Qt::SkipEmptyParts);
        if (!parts.isEmpty()) firstCount[parts.first().toLower()] += 1;
    }

    for (const Element& e : elements) {
        if (e.type != QStringLiteral("character")) continue;
        const QString full = e.name.trimmed();
        if (full.isEmpty()) continue;
        cast.ids.insert(e.id);
        cast.nameById.insert(e.id, full);

        QStringList names{ full };
        const QStringList parts = full.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        // Primeiro nome sozinho só se for único no elenco — e com pelo menos
        // duas letras: nome de uma letra só não existe, e "O Coletor" viraria
        // "O", batendo em toda frase que começa com o artigo.
        if (parts.size() > 1 && parts.first().size() >= 2
            && firstCount.value(parts.first().toLower()) == 1) names.append(parts.first());
        for (const QString& a : e.aliases)
            if (!a.trimmed().isEmpty()) names.append(a.trimmed());

        for (const QString& n : names) {
            const QString key = n.toLower();
            if (cast.idByLowerName.contains(key) && cast.idByLowerName.value(key) != e.id)
                ambiguous.insert(key);
            cast.idByLowerName.insert(key, e.id);
            QRegularExpression::PatternOptions opts = kUnicode;
            if (!n.at(0).isUpper()) opts |= QRegularExpression::CaseInsensitiveOption;
            cast.tokens.append({ QRegularExpression(
                QStringLiteral("(?<![\\p{L}\\p{N}])%1(?![\\p{L}\\p{N}])").arg(QRegularExpression::escape(n)),
                opts), e.id });
        }
    }
    for (const QString& k : ambiguous) cast.idByLowerName.remove(k);
    return cast;
}

QStringList DialogueDetector::paragraphsOf(const QString& plainText)
{
    QStringList out;
    for (const QString& raw : plainText.split(QRegularExpression(QStringLiteral("[\\n\\x{2029}\\x{2028}]")))) {
        const QString t = raw.trimmed();
        if (!t.isEmpty()) out.append(t);
    }
    return out;
}

QVector<DetectedDialogueLine> DialogueDetector::scanScene(const QStringList& paragraphs,
                                                          const Cast& cast,
                                                          const DialogueLang& lang,
                                                          const QString& narratorId,
                                                          const QHash<QString, QChar>& gender)
{
    QVector<DetectedDialogueLine> out;
    QStringList recent; // citados recentemente, mais novo primeiro
    QStringList turns;  // locutores da troca atual ("fig:…" = figurante)
    int sinceDialog = 0;

    // Convenção da cena: diálogo com aspas ou com travessão? Decide se aspas
    // no meio da narração são fala ("Oda shrugged. “Fair...”") ou pensamento.
    int quoteStarts = 0, dashStarts = 0;
    for (const QString& raw : paragraphs) {
        const QString t = raw.trimmed();
        if (t.isEmpty()) continue;
        if (isOpenQuote(t.at(0))) ++quoteStarts;
        else if (t.at(0) == QChar(0x2014) || t.at(0) == QChar(0x2013) || t.startsWith(QLatin1String("- ")))
            ++dashStarts;
    }
    const bool quotesDialogue = quoteStarts > dashStarts
        || (quoteStarts == 0 && dashStarts == 0 && (lang.code == QLatin1String("en") || lang.code == QLatin1String("it")));

    for (const QString& raw : paragraphs) {
        const QString p = raw.trimmed();
        if (p.isEmpty()) continue;
        const QVector<Seg> segs = segment(p, lang, quotesDialogue);

        QStringList speechParts, narrParts;
        for (const Seg& s : segs) (s.speech ? speechParts : narrParts).append(s.text);

        Subject subj;
        for (const QString& n : narrParts) {
            subj = subjectOf(n, cast, lang);
            if (subj.kind != Subject::None) break;
        }

        // Aspas curtas seguidas de narração comprida sem sujeito = citação,
        // não fala ("“Ele”? Na verdade, Miya nem sabia...").
        bool isDialogue = !speechParts.isEmpty();
        if (isDialogue && isOpenQuote(p.at(0)) && subj.kind == Subject::None) {
            const QString speech = speechParts.join(QLatin1Char(' '));
            int narrLen = 0;
            for (const QString& n : narrParts) narrLen += n.size();
            if (wordCount(speech) <= 3 && narrLen > 60) isDialogue = false;
        }

        if (!isDialogue) {
            for (const Hit& h : mentions(p, cast)) pushRecent(recent, h.id);
            if (++sinceDialog > 3) turns.clear();
            continue;
        }
        sinceDialog = 0;

        const QString speech = speechParts.join(QLatin1Char(' '));
        QSet<QString> vocatives;
        for (const Hit& h : mentions(speech, cast)) vocatives.insert(h.id);

        QString who;
        DialogueConfidence conf = DialogueConfidence::None;
        QString extraLabel;

        if (subj.kind == Subject::Name) {
            who = subj.id;
            conf = DialogueConfidence::Certain;
        } else if (subj.kind == Subject::Extra) {
            conf = DialogueConfidence::Extra;
            extraLabel = subj.label;
        } else if (subj.kind == Subject::FirstPerson && !narratorId.isEmpty()) {
            who = narratorId;
            conf = DialogueConfidence::Certain;
        } else {
            // Conversa: quem está na troca, depois quem foi citado.
            QStringList parts;
            for (int i = turns.size() - 1; i >= 0; --i)
                if (!parts.contains(turns.at(i))) parts.append(turns.at(i));
            // Em 1ª pessoa o narrador está em toda cena, mesmo sem ser citado
            // ("entrei", "olhou para mim"): vem antes de quem só foi citado.
            if (!narratorId.isEmpty() && !parts.contains(narratorId)) parts.append(narratorId);
            for (const QString& r : recent)
                if (!parts.contains(r)) parts.append(r);
            QStringList cands;
            for (const QString& c : parts)
                if (!vocatives.contains(c) && !isExtraTurn(c)) cands.append(c);

            const bool pron = subj.kind == Subject::Pronoun;
            QStringList sameGender;
            if (pron) {
                for (const QString& c : cands)
                    if (gender.value(c) == subj.gender) sameGender.append(c);
                if (!sameGender.isEmpty()) cands = sameGender;
            }

            const QString last = turns.isEmpty() ? QString() : turns.last();
            const QString prevTurn = turns.size() >= 2 ? turns.at(turns.size() - 2) : QString();

            if (pron && sameGender.size() == 1
                && (turns.isEmpty() || gender.value(last) != subj.gender || last == sameGender.first())) {
                who = sameGender.first();
            } else if (turns.size() >= 2 && last != prevTurn && isExtraTurn(prevTurn)) {
                conf = DialogueConfidence::Extra;
                extraLabel = prevTurn.mid(4);
            } else if (turns.size() >= 2 && last != prevTurn && cands.contains(prevTurn)) {
                who = prevTurn;
            } else if (!turns.isEmpty() && !cands.isEmpty() && cands.first() != last) {
                who = cands.first();
            } else if (!turns.isEmpty() && cands.size() >= 2) {
                who = cands.at(1);
            } else if (turns.isEmpty() && !cands.isEmpty() && !vocatives.isEmpty()) {
                who = cands.first();
            }
            if (pron && !who.isEmpty() && !gender.value(who).isNull() && gender.value(who) != subj.gender)
                who.clear();
            if (!who.isEmpty()) conf = DialogueConfidence::Probable;
        }

        out.append({ p, speech, who, conf, extraLabel });

        if (conf == DialogueConfidence::Extra) {
            turns.append(QStringLiteral("fig:") + (extraLabel.isEmpty() ? QStringLiteral("?") : extraLabel));
        } else if (!who.isEmpty()) {
            turns.append(who);
            pushRecent(recent, who);
        } else if (subj.kind != Subject::None && subj.kind != Subject::Pronoun && !turns.isEmpty()) {
            turns = QStringList{ turns.last() };
        }
        for (const QString& n : narrParts)
            for (const Hit& h : mentions(n, cast))
                if (h.id != who) pushRecent(recent, h.id, recent.isEmpty() ? 0 : 1);
        // Quem foi CHAMADO pelo nome ("Ilse, you shouldn't be here", "…ao
        // ponto, Skill.") está na conversa: é o candidato natural pra
        // responder. Nome só citado ("— Samantha da Silva Lira. 36 anos…")
        // não entra — só o vocativo, colado em vírgula/pontuação.
        for (const Hit& h : mentions(speech, cast)) {
            if (h.id == who) continue;
            const QString before = speech.left(h.pos).trimmed();
            const QString after = speech.mid(h.end).trimmed();
            static const QString punct = QStringLiteral(",.!?;…");
            const bool openOk = before.isEmpty() || punct.contains(before.back());
            const bool closeOk = after.isEmpty() || punct.contains(after.front());
            if (openOk && closeOk) pushRecent(recent, h.id, recent.isEmpty() ? 0 : 1);
        }
    }
    return out;
}

QVector<DetectedDialogueLine> DialogueDetector::scanScreenplay(const QTextDocument& doc, const Cast& cast)
{
    QVector<DetectedDialogueLine> out;
    // Nome, primeiro nome e apelidos valem como deixa ("KLARA", "CAPITÃ").
    const QHash<QString, QString>& idByName = cast.idByLowerName;

    static const QRegularExpression ext(QStringLiteral("\\(.*?\\)|\\bCONT['’]?D\\b"));
    QString cue;
    for (QTextBlock b = doc.begin(); b.isValid(); b = b.next()) {
        const QString text = b.text().trimmed();
        const ScreenplayElement el = ScreenplayFormat::detect(b.blockFormat(), text);
        if (el == ScreenplayElement::Character) {
            cue = QString(text).remove(ext).trimmed();
            continue;
        }
        if (el == ScreenplayElement::Parenthetical) continue;
        if (el != ScreenplayElement::Dialogue) { cue.clear(); continue; }
        if (text.isEmpty() || cue.isEmpty()) continue;

        DetectedDialogueLine line;
        line.text = text;
        line.speech = text;
        const QString id = idByName.value(cue.toLower());
        if (!id.isEmpty()) {
            line.characterId = id;
            line.confidence = DialogueConfidence::Certain;
        } else {
            line.confidence = DialogueConfidence::Extra;
            line.extraLabel = cue;
        }
        out.append(line);
    }
    return out;
}

QString DialogueDetector::legacyAttribution(const QString& text, const Cast& cast)
{
    QString attribution;
    if (text.startsWith(QChar(0x2014))) {
        QStringList parts;
        for (const QString& p : text.split(QChar(0x2014))) {
            const QString t = p.trimmed();
            if (!t.isEmpty()) parts.append(t);
        }
        if (parts.size() >= 2) {
            parts.removeFirst();
            attribution = parts.join(QLatin1Char(' '));
        }
    } else {
        static const QRegularExpression closing(QStringLiteral("[\"\\x{201D}]"));
        const QRegularExpressionMatch m = closing.match(text, 1);
        if (m.hasMatch()) attribution = text.mid(m.capturedStart() + 1).trimmed();
    }
    if (attribution.isEmpty()) return QString();

    QSet<QString> matched;
    for (auto it = cast.idByLowerName.constBegin(); it != cast.idByLowerName.constEnd(); ++it) {
        const QRegularExpression re(
            QStringLiteral("(?<![\\p{L}\\p{N}])%1(?![\\p{L}\\p{N}])").arg(QRegularExpression::escape(it.key())),
            QRegularExpression::CaseInsensitiveOption | kUnicode);
        if (re.match(attribution).hasMatch()) matched.insert(it.value());
    }
    return matched.size() == 1 ? *matched.begin() : QString();
}

void DialogueDetector::addGenderVotes(const QString& text, const Cast& cast, const DialogueLang& lang,
                                      GenderVotes& votes)
{
    if (text.isEmpty()) return;
    for (const Token& t : cast.tokens) {
        auto it = t.re.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            // Artigo antes: "a Klara", "o João".
            const QString before = text.mid(qMax(0, int(m.capturedStart()) - 12),
                                            qMin(12, int(m.capturedStart())));
            static const QRegularExpression ws(QStringLiteral("\\s+"));
            const QStringList bw = before.split(ws, Qt::SkipEmptyParts);
            if (!bw.isEmpty() && before.endsWith(QLatin1Char(' '))) {
                const QString a = cleanWord(bw.last()).toLower();
                if (lang.femArticles.contains(a)) votes[t.id].first += 1.0;
                else if (lang.mascArticles.contains(a)) votes[t.id].second += 1.0;
            }
        }
    }

    // Pronome que abre a frase seguinte a uma frase que cita UM personagem só:
    // "Oda posa la navette. Elle le regarda." O pronome "logo depois do nome"
    // (usado antes) pegava o de outra pessoa com frequência — "Bram me
    // esperava... ela" chegou a dar o Bram como mulher.
    static const QRegularExpression sentenceEnd(QStringLiteral("(?<=[.!?\\x{2026}])\\s+"), kUnicode);
    for (const QString& para : paragraphsOf(text)) {
        const QStringList sentences = para.split(sentenceEnd, Qt::SkipEmptyParts);
        for (int i = 0; i + 1 < sentences.size(); ++i) {
            QSet<QString> ids;
            for (const Hit& h : mentions(sentences.at(i), cast)) ids.insert(h.id);
            if (ids.size() != 1) continue;
            const QStringList next = pronounWords(sentences.at(i + 1));
            if (next.isEmpty()) continue;
            const QChar g = lang.pronouns.value(next.first());
            if (g.isNull()) continue;
            if (g == QLatin1Char('f')) votes[*ids.begin()].first += 1.0;
            else votes[*ids.begin()].second += 1.0;
        }
    }
}

QHash<QString, QChar> DialogueDetector::genderFromVotes(const GenderVotes& votes)
{
    QHash<QString, QChar> out;
    for (auto it = votes.constBegin(); it != votes.constEnd(); ++it) {
        const double f = it.value().first, m = it.value().second;
        if (f > m * 1.5 && f >= 1.0) out.insert(it.key(), QLatin1Char('f'));
        else if (m > f * 1.5 && m >= 1.0) out.insert(it.key(), QLatin1Char('m'));
    }
    return out;
}
