#include "DialogueDetector.h"

#include "DialogueVerbs.h"

#include <QHash>
#include <QSet>

namespace {

// Personagem citado + verbo de fala perto (com ou sem tag explícita) — uma
// única regex composta reaproveitada tanto pra atribuição quanto pra
// proximidade, igual ao buildScannerTokens do Mira 1.
DialogueScannerToken makeToken(const QString& elementId, const QString& phrase,
                                const QString& verbsSrc,
                                QRegularExpression::PatternOptions opts)
{
    const QString esc = QRegularExpression::escape(phrase);
    const QString boundary =
        QStringLiteral("(?<![\\p{L}\\p{N}])%1(?![\\p{L}\\p{N}])").arg(esc);
    const QString proximity = QStringLiteral(
        "(\\b(%1)\\s+%2(?![\\p{L}\\p{N}])|(?<![\\p{L}\\p{N}])%2\\s+(%1)\\b)")
        .arg(verbsSrc, esc);

    DialogueScannerToken tok;
    tok.elementId = elementId;
    tok.attrRe = QRegularExpression(boundary, opts);
    tok.proximityRe = QRegularExpression(proximity, opts);
    return tok;
}

} // namespace

QVector<DialogueScannerToken> DialogueDetector::buildScannerTokens(const QList<Element>& characters)
{
    QVector<DialogueScannerToken> tokens;

    // Primeiro nome só vira token próprio se for único entre os personagens
    // (evita "— Não vou, disse Ana." casar com duas Anas diferentes).
    QHash<QString, int> firstNameCount;
    for (const Element& e : characters) {
        if (e.type != QStringLiteral("character")) continue;
        const QString name = e.name.trimmed();
        if (name.isEmpty()) continue;
        const QStringList parts = name.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        const QString first = parts.first().toLower();
        firstNameCount[first] = firstNameCount.value(first, 0) + 1;
    }

    const QString verbsSrc = DialogueVerbs::speechVerbsSource();
    const auto opts = QRegularExpression::CaseInsensitiveOption
                     | QRegularExpression::UseUnicodePropertiesOption;

    for (const Element& e : characters) {
        if (e.type != QStringLiteral("character")) continue;
        const QString full = e.name.trimmed();
        if (full.isEmpty()) continue;
        tokens.append(makeToken(e.id, full, verbsSrc, opts));

        const QStringList parts = full.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        const QString first = parts.isEmpty() ? QString() : parts.first();
        if (!first.isEmpty() && first.compare(full, Qt::CaseInsensitive) != 0
            && firstNameCount.value(first.toLower(), 0) == 1) {
            tokens.append(makeToken(e.id, first, verbsSrc, opts));
        }

        for (const QString& alias : e.aliases) {
            const QString a = alias.trimmed();
            if (!a.isEmpty()) tokens.append(makeToken(e.id, a, verbsSrc, opts));
        }
    }

    return tokens;
}

QString DialogueDetector::attributeLine(const QString& text,
                                         const QVector<DialogueScannerToken>& tokens,
                                         const Element* narrator)
{
    if (tokens.isEmpty()) return QString();

    QString attributionText;
    if (text.startsWith(QChar(0x2014))) { // travessão —
        QStringList parts;
        for (const QString& p : text.split(QChar(0x2014))) {
            const QString t = p.trimmed();
            if (!t.isEmpty()) parts.append(t);
        }
        if (parts.size() >= 2) {
            parts.removeFirst();
            attributionText = parts.join(QLatin1Char(' '));
        }
    } else {
        static const QRegularExpression closingQuoteRe(QStringLiteral("[\"”]"));
        const QRegularExpressionMatch m = closingQuoteRe.match(text);
        if (m.hasMatch() && m.capturedStart() > 1)
            attributionText = text.mid(m.capturedStart() + 1).trimmed();
    }

    // Narrador em 1ª pessoa: se o texto de atribuição (ou a linha inteira,
    // quando não há atribuição) tem marcador de 1ª pessoa, é o narrador.
    if (narrator) {
        const QString checkTarget = attributionText.isEmpty() ? text : attributionText;
        if (DialogueVerbs::firstPersonMarkersRegex().match(checkTarget).hasMatch())
            return narrator->id;
    }

    QSet<QString> matched;
    if (!attributionText.isEmpty()) {
        for (const DialogueScannerToken& tok : tokens)
            if (tok.attrRe.match(attributionText).hasMatch()) matched.insert(tok.elementId);
    } else {
        for (const DialogueScannerToken& tok : tokens)
            if (tok.proximityRe.match(text).hasMatch()) matched.insert(tok.elementId);
    }

    return matched.size() == 1 ? *matched.begin() : QString();
}

QVector<DetectedDialogueLine> DialogueDetector::scanConfidentDialogues(
    const QString& plainText,
    const QVector<DialogueScannerToken>& tokens,
    const Element* narrator)
{
    QVector<DetectedDialogueLine> out;
    if (tokens.isEmpty()) return out;

    // QTextEdit::toPlainText() do documento inteiro normaliza separadores de
    // bloco para '\n' comum (diferente de QTextCursor::selectedText(), que
    // usa U+2029 — não confundir os dois).
    const QStringList paragraphs = plainText.split(QChar('\n'));
    for (const QString& raw : paragraphs) {
        const QString text = raw.trimmed();
        if (text.isEmpty()) continue;

        const bool startsWithDash = text.startsWith(QChar(0x2014));
        const bool startsWithQuote = text.startsWith(QChar('"'))
            || text.startsWith(QChar(0x201C))
            || text.startsWith(QChar(0x201E));
        if (!startsWithDash && !startsWithQuote) continue;

        const QString characterId = attributeLine(text, tokens, narrator);
        if (characterId.isEmpty()) continue;

        out.append(DetectedDialogueLine{ text, characterId });
    }
    return out;
}
