#include "DialogueDetector.h"

#include "DialogueVerbs.h"
#include "DialogueVerbsEN.h"
#include "DialogueVerbsES.h"

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

    // PT-BR + EN + ES combinados — projeto pode estar escrito em qualquer um
    // dos três (ou misturado), sem precisar escolher idioma.
    static const QString verbsSrc = DialogueVerbs::speechVerbsSource()
        + QStringLiteral("|") + DialogueVerbsEN::speechVerbsSource()
        + QStringLiteral("|") + DialogueVerbsES::speechVerbsSource();
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
        // Busca a partir do índice 1 (não 0) — com aspas retas ("), a própria
        // abertura já bate nesta regex; buscando do início, o primeiro match
        // seria sempre a abertura, nunca o fechamento real mais adiante.
        static const QRegularExpression closingQuoteRe(QStringLiteral("[\"”]"));
        const QRegularExpressionMatch m = closingQuoteRe.match(text, 1);
        if (m.hasMatch())
            attributionText = text.mid(m.capturedStart() + 1).trimmed();
    }

    // Tenta casar um personagem NOMEADO primeiro — tag explícita
    // (attributionText) ou, sem tag, nome perto de verbo de fala na própria
    // linha (proximidade). Um nome reconhecido é sinal mais forte que a
    // presença de "I/my/me" (que aparece em quase qualquer fala, seja de
    // quem for) — por isso corre ANTES do atalho de narrador em 1ª pessoa
    // abaixo. Sem isso, uma linha sem tag mas com o nome de outro
    // personagem por perto (proximidade) era sempre roubada pelo narrador
    // só por conter um pronome de 1ª pessoa.
    QSet<QString> matched;
    if (!attributionText.isEmpty()) {
        for (const DialogueScannerToken& tok : tokens)
            if (tok.attrRe.match(attributionText).hasMatch()) matched.insert(tok.elementId);
    } else {
        for (const DialogueScannerToken& tok : tokens)
            if (tok.proximityRe.match(text).hasMatch()) matched.insert(tok.elementId);
    }
    if (matched.size() == 1) return *matched.begin();

    // Narrador em 1ª pessoa: só como fallback, quando nenhum personagem
    // nomeado foi encontrado com confiança na linha. Usa o texto de
    // atribuição (ou a linha inteira, quando não há atribuição) em busca de
    // marcador de 1ª pessoa.
    if (narrator) {
        const QString checkTarget = attributionText.isEmpty() ? text : attributionText;
        if (DialogueVerbs::firstPersonMarkersRegex().match(checkTarget).hasMatch()
            || DialogueVerbsEN::firstPersonMarkersRegex().match(checkTarget).hasMatch()
            || DialogueVerbsES::firstPersonMarkersRegex().match(checkTarget).hasMatch())
            return narrator->id;
    }

    return QString();
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

        // Retido mesmo se vazio (ambíguo/sem locutor) — vira "sem
        // atribuição" no Pensário, em vez de ser descartado silenciosamente.
        const QString characterId = attributeLine(text, tokens, narrator);

        out.append(DetectedDialogueLine{ text, characterId });
    }
    return out;
}
