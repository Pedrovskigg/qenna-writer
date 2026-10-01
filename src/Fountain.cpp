#include "Fountain.h"

#include "SceneUtils.h"

#include <QCoreApplication>
#include <QFont>
#include <QHash>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>

namespace {

// Cabeçalho que o Fountain reconhece sozinho (sem o "." de forçar).
const QRegularExpression& fountainSceneRe()
{
    static const QRegularExpression re(
        QStringLiteral("^(INT|EXT|EST|INT\\.?/EXT|EXT\\.?/INT|I/E)[\\.\\s]"),
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

bool hasLetter(const QString& t)
{
    for (const QChar c : t) if (c.isLetter()) return true;
    return false;
}

bool hasLowercase(const QString& t)
{
    for (const QChar c : t) if (c.isLower()) return true;
    return false;
}

// *itálico*, **negrito**, ***os dois***, _sublinhado_ e o "\" de escape.
QString stripEmphasis(QString t)
{
    static const QRegularExpression bold3(QStringLiteral("(?<!\\\\)\\*{3}(.+?)(?<!\\\\)\\*{3}"));
    static const QRegularExpression bold2(QStringLiteral("(?<!\\\\)\\*{2}(.+?)(?<!\\\\)\\*{2}"));
    static const QRegularExpression ital(QStringLiteral("(?<!\\\\)\\*(.+?)(?<!\\\\)\\*"));
    static const QRegularExpression under(QStringLiteral("(?<!\\\\)_(.+?)(?<!\\\\)_"));
    t.replace(bold3, QStringLiteral("\\1"));
    t.replace(bold2, QStringLiteral("\\1"));
    t.replace(ital, QStringLiteral("\\1"));
    t.replace(under, QStringLiteral("\\1"));
    t.replace(QStringLiteral("\\*"), QStringLiteral("*"));
    t.replace(QStringLiteral("\\_"), QStringLiteral("_"));
    return t;
}

// "INT. CASA - NOITE #12#" → sem o número de cena.
QString stripSceneNumber(QString t)
{
    static const QRegularExpression num(QStringLiteral("\\s*#[^#\\s]+#\\s*$"));
    return t.remove(num).trimmed();
}

}

namespace Fountain {

int Document::sceneCount() const
{
    int n = 0;
    for (const Chapter& c : chapters)
        for (const Line& l : c.lines)
            if (l.element == ScreenplayElement::Scene) ++n;
    return n;
}

QList<QPair<QString, int>> Document::characters() const
{
    QList<QPair<QString, int>> out;
    QHash<QString, int> idx;
    for (const Chapter& c : chapters)
        for (const Line& l : c.lines) {
            if (l.element != ScreenplayElement::Character) continue;
            const QString cue = ScreenplayFormat::cueName(l.text).toUpper();
            if (cue.isEmpty()) continue;
            if (!idx.contains(cue)) { idx.insert(cue, int(out.size())); out.append({ cue, 0 }); }
            ++out[idx.value(cue)].second;
        }
    return out;
}

Document parse(const QString& input)
{
    Document doc;
    QString text = input;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    static const QRegularExpression boneyard(QStringLiteral("/\\*.*?\\*/"),
                                             QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression notes(QStringLiteral("\\[\\[.*?\\]\\]"),
                                          QRegularExpression::DotMatchesEverythingOption);
    text.remove(boneyard);
    text.remove(notes);
    const QStringList lines = text.split(QLatin1Char('\n'));
    const int n = int(lines.size());
    auto blank = [&](int i) { return i < 0 || i >= n || lines.at(i).trimmed().isEmpty(); };

    int i = 0;
    // Página de rosto: "Chave: valor" na primeira linha, até a primeira linha vazia.
    static const QRegularExpression keyRe(QStringLiteral("^\\s*([A-Za-z][A-Za-z ]*):\\s*(.*)$"));
    while (i < n && lines.at(i).trimmed().isEmpty()) ++i;
    if (i < n && keyRe.match(lines.at(i)).hasMatch()) {
        QString key;
        QHash<QString, QStringList> values;
        for (; i < n && !lines.at(i).trimmed().isEmpty(); ++i) {
            const QRegularExpressionMatch m = keyRe.match(lines.at(i));
            const bool indented = lines.at(i).startsWith(QLatin1Char(' ')) || lines.at(i).startsWith(QLatin1Char('\t'));
            if (m.hasMatch() && !indented) {
                key = m.captured(1).trimmed().toLower();
                if (!m.captured(2).trimmed().isEmpty()) values[key] << stripEmphasis(m.captured(2).trimmed());
            } else if (!key.isEmpty()) {
                values[key] << stripEmphasis(lines.at(i).trimmed());
            }
        }
        doc.title = values.value(QStringLiteral("title")).join(QLatin1Char(' '));
        doc.author = values.value(QStringLiteral("author")).join(QStringLiteral(", "));
        if (doc.author.isEmpty()) doc.author = values.value(QStringLiteral("authors")).join(QStringLiteral(", "));
        doc.contact = values.value(QStringLiteral("contact")).join(QLatin1Char('\n'));
    }

    Chapter current;
    auto push = [&](ScreenplayElement el, const QString& t) {
        current.lines.append({ el, stripEmphasis(t).trimmed() });
    };

    for (; i < n; ++i) {
        const QString raw = lines.at(i);
        const QString t = raw.trimmed();
        if (t.isEmpty()) continue;
        if (t.startsWith(QLatin1String("==="))) continue;                              // quebra de página
        if (t.startsWith(QLatin1Char('=')) && !t.startsWith(QLatin1String("=="))) continue;  // sinopse
        if (t.startsWith(QLatin1Char('#'))) {                                          // seção → capítulo
            int lvl = 0;
            while (lvl < t.size() && t.at(lvl) == QLatin1Char('#')) ++lvl;
            if (!current.lines.isEmpty() || !current.title.isEmpty()) doc.chapters.append(current);
            current = Chapter();
            current.title = t.mid(lvl).trimmed();
            continue;
        }
        const bool before = blank(i - 1);
        const bool after = blank(i + 1);

        // Cena: forçada com "." ou começando com INT/EXT/EST/I/E.
        if (before && t.startsWith(QLatin1Char('.')) && !t.startsWith(QLatin1String(".."))) {
            push(ScreenplayElement::Scene, stripSceneNumber(t.mid(1)).toUpper());
            continue;
        }
        if (before && fountainSceneRe().match(t).hasMatch()) {
            push(ScreenplayElement::Scene, stripSceneNumber(t).toUpper());
            continue;
        }
        // Transição: forçada com ">" (sem o "<" do centralizado) ou "... TO:".
        if (t.startsWith(QLatin1Char('>'))) {
            if (t.endsWith(QLatin1Char('<'))) push(ScreenplayElement::Action, t.mid(1, t.size() - 2));
            else push(ScreenplayElement::Transition, t.mid(1).toUpper());
            continue;
        }
        if (before && after && !hasLowercase(t) && hasLetter(t) && t.endsWith(QLatin1String("TO:"))) {
            push(ScreenplayElement::Transition, t);
            continue;
        }
        // Personagem: "@" força; ou linha em caixa alta com fala logo embaixo.
        const bool forcedChar = t.startsWith(QLatin1Char('@'));
        QString cue = forcedChar ? t.mid(1) : t;
        if (cue.endsWith(QLatin1Char('^'))) cue.chop(1);   // diálogo duplo: vira fala comum
        const QString cueBody = ScreenplayFormat::cueName(cue);
        if (before && !after && !t.startsWith(QLatin1Char('!'))
            && (forcedChar || (hasLetter(cueBody) && !hasLowercase(cueBody)))) {
            push(ScreenplayElement::Character, cue.trimmed());
            int j = i + 1;
            bool inDialogue = false;
            for (; j < n && !lines.at(j).trimmed().isEmpty(); ++j) {
                const QString s = lines.at(j).trimmed();
                if (s.startsWith(QLatin1Char('('))) {
                    push(ScreenplayElement::Parenthetical, s);
                    inDialogue = false;
                } else if (inDialogue) {
                    // Fala em várias linhas: a mesma fala, com quebra de linha.
                    current.lines.last().text += QChar(QChar::LineSeparator) + stripEmphasis(s);
                } else {
                    push(ScreenplayElement::Dialogue, s);
                    inDialogue = true;
                }
            }
            i = j - 1;
            continue;
        }
        // Ação ("!" força). Linhas seguidas sem linha vazia = o mesmo parágrafo.
        const QString act = t.startsWith(QLatin1Char('!')) ? t.mid(1) : t;
        if (!before && !current.lines.isEmpty() && current.lines.last().element == ScreenplayElement::Action)
            current.lines.last().text += QChar(QChar::LineSeparator) + stripEmphasis(act).trimmed();
        else
            push(ScreenplayElement::Action, act);
    }
    if (!current.lines.isEmpty() || !current.title.isEmpty()) doc.chapters.append(current);
    return doc;
}

QString chapterHtml(const QList<Line>& lines)
{
    QTextDocument doc;
    QFont f(QStringLiteral("Courier New"));
    f.setPointSizeF(12.0);
    doc.setDefaultFont(f);
    QTextCursor cur(&doc);
    bool first = true;
    for (const Line& l : lines) {
        if (l.text.isEmpty()) continue;
        if (!first) cur.insertBlock();
        first = false;
        ScreenplayFormat::applyBlockFormat(cur, l.element);
        cur.insertText(l.text);
    }
    ScreenplayFormat::ensureSceneBreaks(&doc, false);
    ScreenplayFormat::compactSceneBreaks(&doc, false);
    return doc.toHtml();
}

QString write(const QList<ChapterSource>& chapters, const QString& title, const QString& author,
              const QString& contact, bool sceneNumbers)
{
    QString out;
    // Página de rosto (as chaves do Fountain são em inglês por especificação).
    if (!title.trimmed().isEmpty()) out += QStringLiteral("Title: ") + title.trimmed() + QLatin1Char('\n');
    if (!author.trimmed().isEmpty()) {
        out += QStringLiteral("Credit: ") + QCoreApplication::translate("Exporter", "Escrito por") + QLatin1Char('\n');
        out += QStringLiteral("Author: ") + author.trimmed() + QLatin1Char('\n');
    }
    const QStringList contactLines = contact.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (!contactLines.isEmpty()) {
        out += QStringLiteral("Contact:\n");
        for (const QString& c : contactLines) out += QStringLiteral("    ") + c.trimmed() + QLatin1Char('\n');
    }
    if (!out.isEmpty()) out += QLatin1Char('\n');

    auto escape = [](QString t) {
        t.replace(QLatin1Char('*'), QStringLiteral("\\*"));
        t.replace(QLatin1Char('_'), QStringLiteral("\\_"));
        return t;
    };
    // Texto do bloco com a ênfase do editor em marcação Fountain.
    auto blockText = [&](const QTextBlock& b, bool emphasis) {
        QString s;
        for (auto it = b.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;
            QString t = escape(frag.text());
            t.replace(QChar(QChar::LineSeparator), QLatin1Char('\n'));
            const QTextCharFormat cf = frag.charFormat();
            if (emphasis && !t.trimmed().isEmpty()) {
                const bool bold = cf.fontWeight() >= QFont::DemiBold;
                const bool ital = cf.fontItalic();
                if (bold && ital) t = QStringLiteral("***") + t + QStringLiteral("***");
                else if (bold)    t = QStringLiteral("**") + t + QStringLiteral("**");
                else if (ital)    t = QLatin1Char('*') + t + QLatin1Char('*');
                if (cf.fontUnderline()) t = QLatin1Char('_') + t + QLatin1Char('_');
            }
            s += t;
        }
        return s.trimmed();
    };

    int sceneNo = 1;
    ScreenplayElement prev = ScreenplayElement::Action;
    bool any = false;
    for (const ChapterSource& ch : chapters) {
        if (!ch.title.trimmed().isEmpty()) {
            out += QStringLiteral("# ") + ch.title.trimmed() + QStringLiteral("\n\n");
            any = false;
        }
        for (const QString& seg : SceneUtils::splitHtmlIntoScenes(ch.html)) {
            QTextDocument d;
            d.setHtml(seg);
            for (QTextBlock b = d.begin(); b.isValid(); b = b.next()) {
                if (ScreenplayFormat::isSceneBreak(b) || b.text().trimmed().isEmpty()) continue;
                const ScreenplayElement el = ScreenplayFormat::detect(b.blockFormat(), b.text());
                const bool inSpeech = (el == ScreenplayElement::Dialogue || el == ScreenplayElement::Parenthetical)
                    && (prev == ScreenplayElement::Character || prev == ScreenplayElement::Dialogue
                        || prev == ScreenplayElement::Parenthetical);
                if (any && !inSpeech) out += QLatin1Char('\n');
                QString line;
                switch (el) {
                case ScreenplayElement::Scene: {
                    const QString h = b.text().simplified().toUpper();
                    line = (fountainSceneRe().match(h).hasMatch() ? QString() : QStringLiteral(".")) + h;
                    if (sceneNumbers && ScreenplayFormat::isSceneHeading(h))
                        line += QStringLiteral(" #%1#").arg(sceneNo++);
                    break;
                }
                case ScreenplayElement::Character: {
                    const QString c = b.text().simplified();
                    line = (hasLowercase(ScreenplayFormat::cueName(c)) ? QStringLiteral("@") : QString()) + c;
                    break;
                }
                case ScreenplayElement::Parenthetical: {
                    QString p = b.text().trimmed();
                    if (!p.startsWith(QLatin1Char('('))) p.prepend(QLatin1Char('('));
                    if (!p.endsWith(QLatin1Char(')'))) p.append(QLatin1Char(')'));
                    line = p;
                    break;
                }
                case ScreenplayElement::Transition: {
                    const QString tr = b.text().simplified().toUpper();
                    line = tr.endsWith(QLatin1String("TO:")) ? tr : QStringLiteral("> ") + tr;
                    break;
                }
                case ScreenplayElement::Dialogue:
                    line = blockText(b, true);
                    break;
                case ScreenplayElement::Action: {
                    line = blockText(b, true);
                    // Ação que o Fountain leria como outra coisa (caixa alta,
                    // começa com marcador) vai forçada com "!".
                    const QString plain = b.text().trimmed();
                    static const QString markers = QStringLiteral(".@#=>~!");
                    const bool risky = (!hasLowercase(plain) && hasLetter(plain))
                        || (!plain.isEmpty() && markers.contains(plain.at(0)))
                        || fountainSceneRe().match(plain).hasMatch();
                    if (risky) line.prepend(QLatin1Char('!'));
                    break;
                }
                }
                out += line + QLatin1Char('\n');
                prev = el;
                any = true;
            }
        }
        out += QLatin1Char('\n');
        any = false;
    }
    return out;
}

}
