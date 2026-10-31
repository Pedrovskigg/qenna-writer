#include "CharacterDetector.h"

#include <QHash>

namespace {

// Partículas de nome: aparecem em qualquer texto, não denunciam ninguém.
bool isParticle(const QString& lower)
{
    static const QSet<QString> particles = {
        QStringLiteral("de"), QStringLiteral("da"), QStringLiteral("do"), QStringLiteral("das"), QStringLiteral("dos"),
        QStringLiteral("e"), QStringLiteral("y"), QStringLiteral("di"), QStringLiteral("du"), QStringLiteral("des"),
        QStringLiteral("del"), QStringLiteral("della"), QStringLiteral("delle"), QStringLiteral("dei"),
        QStringLiteral("van"), QStringLiteral("von"), QStringLiteral("der"), QStringLiteral("den"), QStringLiteral("zu"),
        QStringLiteral("la"), QStringLiteral("le"), QStringLiteral("les"), QStringLiteral("los"), QStringLiteral("las"),
        QStringLiteral("el"), QStringLiteral("al"), QStringLiteral("of"), QStringLiteral("the"),
        QStringLiteral("bin"), QStringLiteral("ibn"),
    };
    return particles.contains(lower);
}

// "III", "IV", "XII": algarismo romano escrito em maiúsculas, como num nome.
bool isRoman(const QString& part)
{
    static const QRegularExpression re(QStringLiteral("^[IVXLCDM]+$"));
    return re.match(part).hasMatch();
}

// Pedaços do nome: separa no espaço e na barra ("Langdon/River" = dois nomes).
QStringList nameParts(const QString& name)
{
    static const QRegularExpression ws(QStringLiteral("[\\s/]+"));
    return name.trimmed().split(ws, Qt::SkipEmptyParts);
}

} // namespace

QSet<QString> CharacterDetector::sharedNameParts(const QList<Element>& elements)
{
    QHash<QString, int> seen;
    for (const Element& e : elements) {
        if (e.type != QStringLiteral("character")) continue;
        QSet<QString> mine;
        for (const QString& p : nameParts(e.name)) {
            const QString l = p.toLower();
            if (l.length() >= 3 && !isParticle(l) && !isRoman(p)) mine.insert(l);
        }
        for (const QString& l : std::as_const(mine)) ++seen[l];
    }
    QSet<QString> out;
    for (auto it = seen.cbegin(); it != seen.cend(); ++it)
        if (it.value() >= 2) out.insert(it.key());
    return out;
}

QStringList CharacterDetector::buildTokens(const Element& e, const QSet<QString>& sharedParts)
{
    QStringList tokens;

    const QStringList parts = nameParts(e.name);
    for (int i = 0; i < parts.size(); ++i) {
        const QString& part = parts[i];
        const QString l = part.toLower();
        if (l.length() < 3 || isParticle(l) || isRoman(part)) continue;
        // sobrenome da família: sozinho dispararia todos os Lloyd de uma vez
        if (i > 0 && sharedParts.contains(l)) continue;
        tokens.append(l);
    }
    // o nome inteiro também conta ("Uther Lloyd")
    if (parts.size() > 1) tokens.append(e.name.simplified().toLower());

    for (const QString& alias : e.aliases) {
        const QStringList ap = nameParts(alias);
        if (ap.isEmpty()) continue;
        const int minLen = (ap.size() == 1) ? 2 : 3;
        for (const QString& part : ap) {
            const QString l = part.toLower();
            if (l.length() >= minLen && !isParticle(l) && !isRoman(part))
                tokens.append(l);
        }
    }

    tokens.removeDuplicates();
    return tokens;
}

int CharacterDetector::countNamed(const QRegularExpression& re, const QString& text, int limit)
{
    int n = 0;
    for (auto it = re.globalMatch(text); it.hasNext() && n < limit;) {
        const QRegularExpressionMatch m = it.next();
        const QString hit = m.captured(0);
        if (!hit.isEmpty() && hit.at(0).isUpper()) ++n;
    }
    return n;
}

QRegularExpression CharacterDetector::tokenRegex(const QString& token)
{
    return QRegularExpression(QStringLiteral("\\b") + QRegularExpression::escape(token) + QStringLiteral("\\b"),
                              QRegularExpression::CaseInsensitiveOption
                                  | QRegularExpression::UseUnicodePropertiesOption);
}
