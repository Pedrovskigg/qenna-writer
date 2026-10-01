#include "ScreenplayFormat.h"

#include <QCoreApplication>
#include <QFont>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <qmath.h>

namespace {

const QFont& courier()
{
    static const QFont f(QStringLiteral("Courier New"), 12);
    return f;
}

qreal charWidth()
{
    static const qreal w = QFontMetrics(courier()).horizontalAdvance(QLatin1Char('M'));
    return w;
}

qreal lineHeight()
{
    static const qreal h = QFontMetrics(courier()).lineSpacing();
    return h;
}

// Colunas de indentação em caracteres (convenção padrão de roteiro americano,
// Courier 12pt/10cpi) — relativas ao início da área de texto, não à página.
constexpr qreal kColumnChars          = 60.0;
constexpr qreal kCharacterIndentChars = 22.0;
constexpr qreal kDialogueLeftChars    = 10.0;
constexpr qreal kDialogueRightChars   = 15.0;
constexpr qreal kParenLeftChars       = 16.0;
constexpr qreal kParenRightChars      = 20.0;
constexpr qreal kToleranceChars       = 4.0;

// Elemento do bloco durante a sessão (não vai pro HTML — ver o .h).
constexpr int kElementProperty = QTextFormat::UserProperty + 0x5C0;

bool nearChars(qreal px, qreal targetChars)
{
    return qAbs(px - targetChars * charWidth()) < kToleranceChars * charWidth();
}

bool isRightAligned(const QTextBlockFormat& format)
{
    // Ao reabrir, o Qt devolve AlignRight | AlignAbsolute — comparar com == falhava
    // e toda Transição virava Ação.
    return (format.alignment() & Qt::AlignHorizontal_Mask & ~Qt::AlignAbsolute) == Qt::AlignRight;
}

enum class Box { Flush, Character, Dialogue, Parenthetical, Right };

Box boxOf(const QTextBlockFormat& format)
{
    if (isRightAligned(format)) return Box::Right;
    const qreal left = format.leftMargin();
    const qreal right = format.rightMargin();
    if (nearChars(left, kCharacterIndentChars) && nearChars(right, 0)) return Box::Character;
    if (nearChars(left, kParenLeftChars) && nearChars(right, kParenRightChars)) return Box::Parenthetical;
    if (nearChars(left, kDialogueLeftChars) && nearChars(right, kDialogueRightChars)) return Box::Dialogue;
    return Box::Flush;
}

Box boxOf(ScreenplayElement element)
{
    switch (element) {
    case ScreenplayElement::Scene:
    case ScreenplayElement::Action:        return Box::Flush;
    case ScreenplayElement::Character:     return Box::Character;
    case ScreenplayElement::Dialogue:      return Box::Dialogue;
    case ScreenplayElement::Parenthetical: return Box::Parenthetical;
    case ScreenplayElement::Transition:    return Box::Right;
    }
    return Box::Flush;
}

// Transições de praxe nos cinco idiomas do app. Comparadas sem caixa.
const QStringList& knownTransitions()
{
    static const QStringList list = {
        QStringLiteral("CORTA PARA:"), QStringLiteral("CORTE PARA:"), QStringLiteral("CORTE SECO:"),
        QStringLiteral("FUSÃO PARA:"), QStringLiteral("FUSÃO:"), QStringLiteral("FADE PARA:"),
        QStringLiteral("CUT TO:"), QStringLiteral("SMASH CUT TO:"), QStringLiteral("MATCH CUT TO:"),
        QStringLiteral("DISSOLVE TO:"), QStringLiteral("FADE TO:"), QStringLiteral("FADE TO BLACK."),
        QStringLiteral("FADE OUT."), QStringLiteral("FADE OUT"), QStringLiteral("FADE IN:"),
        QStringLiteral("CORTE A:"), QStringLiteral("FUNDIDO A:"), QStringLiteral("FUNDIDO A NEGRO."),
        QStringLiteral("COUPE À:"), QStringLiteral("FONDU AU NOIR."), QStringLiteral("FONDU ENCHAÎNÉ:"),
        QStringLiteral("STACCO SU:"), QStringLiteral("DISSOLVENZA A:"), QStringLiteral("DISSOLVENZA IN NERO."),
    };
    return list;
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

}

QString ScreenplayFormat::label(ScreenplayElement element)
{
    switch (element) {
    case ScreenplayElement::Scene:         return QCoreApplication::translate("ScreenplayFormat", "Cena");
    case ScreenplayElement::Action:        return QCoreApplication::translate("ScreenplayFormat", "Ação");
    case ScreenplayElement::Character:     return QCoreApplication::translate("ScreenplayFormat", "Personagem");
    case ScreenplayElement::Dialogue:      return QCoreApplication::translate("ScreenplayFormat", "Diálogo");
    case ScreenplayElement::Parenthetical: return QCoreApplication::translate("ScreenplayFormat", "Parênteses");
    case ScreenplayElement::Transition:    return QCoreApplication::translate("ScreenplayFormat", "Transição");
    }
    return QString();
}

ScreenplayElement ScreenplayFormat::cycleElement(ScreenplayElement current, bool backwards)
{
    static const ScreenplayElement order[] = {
        ScreenplayElement::Scene, ScreenplayElement::Action, ScreenplayElement::Character,
        ScreenplayElement::Dialogue, ScreenplayElement::Parenthetical, ScreenplayElement::Transition,
    };
    constexpr int n = 6;
    int i = 0;
    while (i < n && order[i] != current) ++i;
    if (i == n) return ScreenplayElement::Action;
    return order[(i + (backwards ? n - 1 : 1)) % n];
}

ScreenplayElement ScreenplayFormat::nextElement(ScreenplayElement current)
{
    switch (current) {
    case ScreenplayElement::Scene:         return ScreenplayElement::Action;
    case ScreenplayElement::Action:        return ScreenplayElement::Action;
    case ScreenplayElement::Character:     return ScreenplayElement::Dialogue;
    case ScreenplayElement::Dialogue:      return ScreenplayElement::Action;
    case ScreenplayElement::Parenthetical: return ScreenplayElement::Dialogue;
    case ScreenplayElement::Transition:    return ScreenplayElement::Scene;
    }
    return ScreenplayElement::Action;
}

namespace {
void applyGeometry(QTextBlockFormat& bf, ScreenplayElement element, qreal cw, qreal lh, bool hasPrevious)
{
    bf.setAlignment(element == ScreenplayElement::Transition ? Qt::AlignRight : Qt::AlignLeft);
    bf.setTextIndent(0);
    switch (element) {
    case ScreenplayElement::Scene:
    case ScreenplayElement::Action:
    case ScreenplayElement::Transition:
        bf.setLeftMargin(0);
        bf.setRightMargin(0);
        break;
    case ScreenplayElement::Character:
        bf.setLeftMargin(kCharacterIndentChars * cw);
        bf.setRightMargin(0);
        break;
    case ScreenplayElement::Dialogue:
        bf.setLeftMargin(kDialogueLeftChars * cw);
        bf.setRightMargin(kDialogueRightChars * cw);
        break;
    case ScreenplayElement::Parenthetical:
        bf.setLeftMargin(kParenLeftChars * cw);
        bf.setRightMargin(kParenRightChars * cw);
        break;
    }
    // Linha em branco antes de cada elemento, menos dentro do bloco de fala
    // (Personagem, Parênteses e Diálogo ficam colados). Cena ganha duas.
    const bool insideSpeech = element == ScreenplayElement::Dialogue
                           || element == ScreenplayElement::Parenthetical;
    const qreal lines = element == ScreenplayElement::Scene ? 2 : insideSpeech ? 0 : 1;
    bf.setTopMargin(hasPrevious ? lines * lh : 0);
    bf.setBottomMargin(0);
    bf.setProperty(kElementProperty, int(element));
}
}

void ScreenplayFormat::applyBlockFormat(QTextCursor& cursor, ScreenplayElement element)
{
    QTextBlockFormat bf = cursor.blockFormat();
    applyGeometry(bf, element, charWidth(), lineHeight(), cursor.block().previous().isValid());
    cursor.setBlockFormat(bf);
}

void ScreenplayFormat::applyPageFormat(QTextCursor& cursor, ScreenplayElement element)
{
    QTextBlockFormat bf = cursor.blockFormat();
    applyGeometry(bf, element, kPageCharPx, kPageLinePx, true);
    bf.setLineHeight(kPageLinePx, QTextBlockFormat::FixedHeight);
    cursor.setBlockFormat(bf);
}

bool ScreenplayFormat::isSceneHeading(const QString& text)
{
    const QString t = text.trimmed();
    static const QStringList anyCase = {
        QStringLiteral("INT./EXT."), QStringLiteral("EXT./INT."),
        QStringLiteral("INT/EXT"),   QStringLiteral("EXT/INT"),
        QStringLiteral("I/E."),      QStringLiteral("I/E "),
        QStringLiteral("INT."),      QStringLiteral("EXT."),
        QStringLiteral("EST."),
    };
    for (const QString& p : anyCase)
        if (t.startsWith(p, Qt::CaseInsensitive)) return true;
    // Por extenso (comum no Brasil) só em caixa alta — "Externa a casa" é ação.
    static const QStringList upperOnly = { QStringLiteral("INTERNA"), QStringLiteral("EXTERNA") };
    for (const QString& p : upperOnly)
        if (t.startsWith(p) && (t.size() == p.size() || !t.at(p.size()).isLetter())) return true;
    return false;
}

ScreenplayElement ScreenplayFormat::detect(const QTextBlockFormat& format, const QString& text)
{
    const Box box = boxOf(format);
    // O elemento guardado na sessão vale enquanto a caixa bater (se alguém mexeu
    // nas margens por fora, a geometria manda).
    if (format.hasProperty(kElementProperty)) {
        const auto stored = static_cast<ScreenplayElement>(format.intProperty(kElementProperty));
        if (boxOf(stored) == box) return stored;
    }
    switch (box) {
    case Box::Right:         return ScreenplayElement::Transition;
    case Box::Character:     return ScreenplayElement::Character;
    case Box::Dialogue:      return ScreenplayElement::Dialogue;
    case Box::Parenthetical: return ScreenplayElement::Parenthetical;
    case Box::Flush:         break;
    }
    return isSceneHeading(text) ? ScreenplayElement::Scene : ScreenplayElement::Action;
}

bool ScreenplayFormat::isSceneBreak(const QTextBlock& block)
{
    return block.blockFormat().hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth);
}

namespace {
QTextBlockFormat compactBreakFormat(QTextBlockFormat f)
{
    f.setTopMargin(0);
    f.setBottomMargin(0);
    f.setTextIndent(0);
    f.setLeftMargin(0);
    f.setRightMargin(0);
    f.setLineHeight(1, QTextBlockFormat::FixedHeight);
    return f;
}
}

int ScreenplayFormat::ensureSceneBreaks(QTextDocument* doc, bool joinPrevious)
{
    if (!doc) return 0;
    QList<QTextBlock> need;
    bool seenContent = false, afterBreak = false;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        if (isSceneBreak(b)) { afterBreak = true; continue; }
        if (b.text().trimmed().isEmpty()) continue;
        if (seenContent && !afterBreak && detect(b.blockFormat(), b.text()) == ScreenplayElement::Scene
            && isSceneHeading(b.text()))
            need << b;
        seenContent = true;
        afterBreak = false;
    }
    if (need.isEmpty()) return 0;
    QTextCursor c(doc);
    if (joinPrevious) c.joinPreviousEditBlock(); else c.beginEditBlock();
    // De trás pra frente: inserir não mexe na posição dos que faltam.
    for (int i = need.size() - 1; i >= 0; --i) {
        QTextCursor bc(need.at(i));
        bc.movePosition(QTextCursor::StartOfBlock);
        bc.insertBlock();   // o texto do cabeçalho desce; o bloco de cima fica vazio
        QTextCursor hr(bc.block().previous());
        QTextBlockFormat f;
        // Largura "variável" (a mesma do <hr/> que o "----" insere): com
        // largura explícita o Qt grava <hr width="100%" />, e o separador de
        // cenas do Qenna só reconhece <hr> puro.
        f.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth, QTextLength());
        hr.setBlockFormat(compactBreakFormat(f));
    }
    c.endEditBlock();
    return int(need.size());
}

void ScreenplayFormat::compactSceneBreaks(QTextDocument* doc, bool joinPrevious)
{
    if (!doc) return;
    QList<QTextBlock> fix;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        if (!isSceneBreak(b)) continue;
        const QTextBlockFormat f = b.blockFormat();
        if (!qFuzzyIsNull(f.topMargin()) || !qFuzzyIsNull(f.bottomMargin())
            || f.lineHeightType() != QTextBlockFormat::FixedHeight || f.lineHeight() > 1)
            fix << b;
    }
    if (fix.isEmpty()) return;
    const bool wasModified = doc->isModified();
    QTextCursor c(doc);
    if (joinPrevious) c.joinPreviousEditBlock(); else c.beginEditBlock();
    for (const QTextBlock& b : std::as_const(fix)) {
        QTextCursor bc(b);
        bc.setBlockFormat(compactBreakFormat(b.blockFormat()));
    }
    c.endEditBlock();
    doc->setModified(wasModified);
}

QString ScreenplayFormat::firstSceneHeading(const QString& html)
{
    QTextDocument d;
    d.setHtml(html);
    for (QTextBlock b = d.begin(); b.isValid(); b = b.next()) {
        if (isSceneBreak(b) || b.text().trimmed().isEmpty()) continue;
        if (detect(b.blockFormat(), b.text()) == ScreenplayElement::Scene && isSceneHeading(b.text()))
            return b.text().simplified().toUpper();
    }
    return QString();
}

void ScreenplayFormat::normalizeDocument(QTextDocument* doc)
{
    if (!doc) return;
    QTextCursor c(doc);
    c.beginEditBlock();
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        if (isSceneBreak(b)) continue;
        QTextCursor bc(b);
        applyBlockFormat(bc, detect(b.blockFormat(), b.text()));
    }
    c.endEditBlock();
}

namespace {
const QRegularExpression& scenePrefixRe()
{
    static const QRegularExpression re(
        QStringLiteral("^\\s*(INT\\.?\\s*/\\s*EXT\\.?|EXT\\.?\\s*/\\s*INT\\.?|I/E\\.?|INT\\.|EXT\\.|EST\\.|INTERNA|EXTERNA)\\s*"),
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

// Separadores entre local e hora: hífen, meia-risca ou travessão com espaços.
int lastTimeSeparator(const QString& rest, int* sepLen)
{
    int best = -1;
    *sepLen = 0;
    for (const QString& sep : { QStringLiteral(" - "), QStringLiteral(" \u2013 "), QStringLiteral(" \u2014 ") }) {
        const int i = rest.lastIndexOf(sep);
        if (i > best) { best = i; *sepLen = sep.size(); }
    }
    return best;
}
}

int ScreenplayFormat::scenePrefixLength(const QString& text)
{
    const QRegularExpressionMatch m = scenePrefixRe().match(text);
    return m.hasMatch() ? int(m.capturedEnd(0)) : -1;
}

QString ScreenplayFormat::sceneLocation(const QString& heading)
{
    const int pre = scenePrefixLength(heading);
    const QString rest = heading.mid(qMax(0, pre));
    int sepLen = 0;
    const int sep = lastTimeSeparator(rest, &sepLen);
    return (sep >= 0 ? rest.left(sep) : rest).simplified().toUpper();
}

QString ScreenplayFormat::sceneTime(const QString& heading)
{
    const int pre = scenePrefixLength(heading);
    const QString rest = heading.mid(qMax(0, pre));
    int sepLen = 0;
    const int sep = lastTimeSeparator(rest, &sepLen);
    return sep >= 0 ? rest.mid(sep + sepLen).simplified().toUpper() : QString();
}

bool ScreenplayFormat::sceneIsInterior(const QString& heading)
{
    const QRegularExpressionMatch m = scenePrefixRe().match(heading);
    if (!m.hasMatch()) return false;
    const QString p = m.captured(1).toUpper();
    return p.startsWith(QLatin1String("INT")) || p.contains(QLatin1String("/INT"))
        || p.startsWith(QLatin1String("I/E")) || p.startsWith(QLatin1String("INTERNA"));
}

bool ScreenplayFormat::sceneIsExterior(const QString& heading)
{
    const QRegularExpressionMatch m = scenePrefixRe().match(heading);
    if (!m.hasMatch()) return false;
    const QString p = m.captured(1).toUpper();
    return p.startsWith(QLatin1String("EXT")) || p.startsWith(QLatin1String("EST"))
        || p.contains(QLatin1String("/EXT")) || p.startsWith(QLatin1String("I/E"))
        || p.startsWith(QLatin1String("EXTERNA"));
}

bool ScreenplayFormat::isUppercaseElement(ScreenplayElement element)
{
    return element == ScreenplayElement::Scene || element == ScreenplayElement::Character
        || element == ScreenplayElement::Transition;
}

QString ScreenplayFormat::cueName(const QString& text)
{
    static const QRegularExpression ext(QStringLiteral("\\(.*?\\)|\\bCONT['’]?D\\b|\\bCONT\\.(?=\\s|$)"));
    return QString(text).remove(ext).simplified();
}

ScreenplayElement ScreenplayFormat::refineOnEnter(ScreenplayElement current, const QString& text,
                                                  const QSet<QString>& knownCues)
{
    if (current != ScreenplayElement::Action) return current;
    const QString t = text.trimmed();
    if (t.isEmpty() || !hasLetter(t)) return current;
    for (const QString& tr : knownTransitions())
        if (t.compare(tr, Qt::CaseInsensitive) == 0) return ScreenplayElement::Transition;
    if (hasLowercase(t)) return current;
    if (t.endsWith(QLatin1Char(':')) && t.size() <= 30) return ScreenplayElement::Transition;
    // Nome em caixa alta vira Personagem: alguém conhecido, ou uma linha curta
    // sem pontuação no fim (o figurante, "MOTOBOY"). "BUM!" e "SILÊNCIO." numa
    // linha de ação continuam ação.
    const QString name = cueName(t);
    if (name.isEmpty() || t.size() > 40) return current;
    if (knownCues.contains(name)) return ScreenplayElement::Character;
    static const QString endPunct = QStringLiteral(".!?:;,…\"'”»");
    const QChar last = name.at(name.size() - 1);
    if (name.size() <= 30 && name.count(QLatin1Char(' ')) <= 3 && !endPunct.contains(last))
        return ScreenplayElement::Character;
    return current;
}

qreal ScreenplayFormat::columnWidthPx()
{
    return (kColumnChars + 1) * charWidth();
}
