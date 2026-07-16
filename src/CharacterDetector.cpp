#include "CharacterDetector.h"

#include <QRegularExpression>

QStringList CharacterDetector::buildTokens(const Element& e)
{
    QStringList tokens;

    const QStringList nameParts = e.name.trimmed().split(
        QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString& part : nameParts) {
        if (part.length() >= 3)
            tokens.append(part.toLower());
    }

    for (const QString& alias : e.aliases) {
        const QString a = alias.trimmed();
        if (a.isEmpty()) continue;
        const QStringList parts = a.split(
            QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        const int minLen = (parts.size() == 1) ? 2 : 3;
        for (const QString& part : parts) {
            if (part.length() >= minLen)
                tokens.append(part.toLower());
        }
    }

    tokens.removeDuplicates();
    return tokens;
}

ScanResult CharacterDetector::scan(const QString& plainText,
                                   const QString& docKey,
                                   const QList<Element>& elements,
                                   const QSet<QString>& alreadyPresent,
                                   const QSet<QString>& autoIds,
                                   const QSet<QString>& neverIds,
                                   const QSet<QString>& rejectedKeys,
                                   bool markAll)
{
    const QString lowerText = plainText.toLower();
    const int wordCount = lowerText.split(
        QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size();

    ScanResult result;

    for (const Element& elem : elements) {
        if (elem.type != QStringLiteral("character")) continue;
        if (alreadyPresent.contains(elem.id)) continue;
        if (rejectedKeys.contains(docKey + QLatin1Char(':') + elem.id)) continue;

        bool detected = false;

        if (elem.narrator) {
            detected = (wordCount >= kNarratorMinWords);
        } else {
            const QStringList tokens = buildTokens(elem);
            for (const QString& token : tokens) {
                // Simples word boundary sem lookahead Unicode pesado
                const QString pattern = QStringLiteral("\\b") + QRegularExpression::escape(token)
                                      + QStringLiteral("\\b");
                QRegularExpression re(pattern, QRegularExpression::CaseInsensitiveOption);
                int count = 0;
                QRegularExpressionMatchIterator it = re.globalMatch(lowerText);
                while (it.hasNext()) {
                    it.next();
                    ++count;
                    if (count >= kThreshold) { detected = true; break; }
                }
                if (detected) break;
            }
        }

        if (!detected) continue;

        const CharacterDetection det{ elem.id, docKey };

        if (elem.narrator || markAll || autoIds.contains(elem.id))
            result.autoMark.append(det);
        else if (!neverIds.contains(elem.id))
            result.confirm.append(det);
    }

    return result;
}
