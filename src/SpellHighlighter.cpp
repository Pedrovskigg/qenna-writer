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
    // Contraste em markers: fragmentos com background → foreground calculado por luminância.
    // Roda sempre (mesmo suspenso) porque é visual e barato.
    const QTextBlock blk = currentBlock();
    for (auto fi = blk.begin(); !fi.atEnd(); ++fi) {
        const QTextFragment frag = fi.fragment();
        if (!frag.isValid()) continue;
        const QBrush bg = frag.charFormat().background();
        if (bg.style() == Qt::NoBrush || bg.color().alpha() == 0) continue;
        const QColor c = bg.color();
        const double lum = 0.299 * c.redF() + 0.587 * c.greenF() + 0.114 * c.blueF();
        QTextCharFormat fmt;
        fmt.setForeground(lum > 0.5 ? QColor(Qt::black) : QColor(Qt::white));
        setFormat(frag.position() - blk.position(), frag.length(), fmt);
    }

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
