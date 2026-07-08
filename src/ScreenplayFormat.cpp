#include "ScreenplayFormat.h"

#include <QCoreApplication>
#include <QFont>
#include <QFontMetrics>
#include <QTextCursor>
#include <qmath.h>

namespace {

qreal charWidth()
{
    static const qreal w =
        QFontMetrics(QFont(QStringLiteral("Courier New"), 12)).horizontalAdvance(QLatin1Char('M'));
    return w;
}

// Colunas de indentação em caracteres (convenção padrão de roteiro americano,
// Courier 12pt/10cpi) — relativas ao início da área de texto, não à página.
constexpr qreal kCharacterIndentChars = 22.0;
constexpr qreal kDialogueLeftChars    = 10.0;
constexpr qreal kDialogueRightChars   = 15.0;
constexpr qreal kParenLeftChars       = 16.0;
constexpr qreal kParenRightChars      = 20.0;
constexpr qreal kToleranceChars       = 4.0;

bool nearChars(qreal px, qreal targetChars)
{
    return qAbs(px - targetChars * charWidth()) < kToleranceChars * charWidth();
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

ScreenplayElement ScreenplayFormat::cycleElement(ScreenplayElement current)
{
    switch (current) {
    case ScreenplayElement::Scene:         return ScreenplayElement::Action;
    case ScreenplayElement::Action:        return ScreenplayElement::Character;
    case ScreenplayElement::Character:     return ScreenplayElement::Dialogue;
    case ScreenplayElement::Dialogue:      return ScreenplayElement::Parenthetical;
    case ScreenplayElement::Parenthetical: return ScreenplayElement::Transition;
    case ScreenplayElement::Transition:    return ScreenplayElement::Scene;
    }
    return ScreenplayElement::Action;
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

void ScreenplayFormat::applyBlockFormat(QTextCursor& cursor, ScreenplayElement element)
{
    QTextBlockFormat bf = cursor.blockFormat();
    const qreal cw = charWidth();
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
    cursor.setBlockFormat(bf);
}

ScreenplayElement ScreenplayFormat::detect(const QTextBlockFormat& format, const QString& text)
{
    if (format.alignment() == Qt::AlignRight) return ScreenplayElement::Transition;

    const qreal left = format.leftMargin();
    const qreal right = format.rightMargin();

    if (nearChars(left, kCharacterIndentChars) && nearChars(right, 0)) return ScreenplayElement::Character;
    if (nearChars(left, kParenLeftChars) && nearChars(right, kParenRightChars)) return ScreenplayElement::Parenthetical;
    if (nearChars(left, kDialogueLeftChars) && nearChars(right, kDialogueRightChars)) return ScreenplayElement::Dialogue;

    const QString t = text.trimmed();
    static const QStringList scenePrefixes = {
        QStringLiteral("INT./EXT."), QStringLiteral("EXT./INT."),
        QStringLiteral("INT/EXT"),   QStringLiteral("EXT/INT"),
        QStringLiteral("INT."),      QStringLiteral("EXT."),
        QStringLiteral("EST."),
    };
    for (const QString& p : scenePrefixes) {
        if (t.startsWith(p, Qt::CaseInsensitive)) return ScreenplayElement::Scene;
    }
    return ScreenplayElement::Action;
}
