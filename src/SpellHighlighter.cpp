#include "SpellHighlighter.h"

#include "SpellChecker.h"

#include <QColor>
#include <QRegularExpressionMatchIterator>
#include <QTextDocument>

SpellHighlighter::SpellHighlighter(QTextDocument* doc, SpellChecker* checker, QObject* parent)
    : QSyntaxHighlighter(doc)
    , m_checker(checker)
    , m_wordRe(QStringLiteral(R"(\b[\p{L}][\p{L}\p{M}'’\-]*[\p{L}\p{M}]|\b[\p{L}]\b)"),
               QRegularExpression::UseUnicodePropertiesOption)
{
    Q_UNUSED(parent);

    m_errorFormat.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    m_errorFormat.setUnderlineColor(QColor(QStringLiteral("#e85a4f")));

    if (m_checker) {
        connect(m_checker, &SpellChecker::changed, this, [this]() { rehighlight(); });
    }
}

void SpellHighlighter::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    rehighlight();
}

void SpellHighlighter::highlightBlock(const QString& text)
{
    if (!m_enabled || m_suspended || !m_checker || !m_checker->isEnabled()) return;
    if (text.isEmpty()) return;

    auto it = m_wordRe.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString word = match.captured(0);
        if (word.isEmpty()) continue;
        if (!m_checker->isCorrect(word)) {
            setFormat(match.capturedStart(), match.capturedLength(), m_errorFormat);
        }
    }
}
