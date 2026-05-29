#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class SpellChecker;

class SpellHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    SpellHighlighter(QTextDocument* doc, SpellChecker* checker, QObject* parent = nullptr);

    // Suspende a marcação (útil durante load de conteúdo grande).
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // suspend/resume: pausa highlightBlock sem disparar rehighlight.
    // Usar em torno de operações que modificam o documento em massa;
    // chamar rehighlight() manualmente após resume() se necessário.
    void suspend() { m_suspended = true; }
    void resume()  { m_suspended = false; }

protected:
    void highlightBlock(const QString& text) override;

private:
    SpellChecker* m_checker;
    QTextCharFormat m_errorFormat;
    QRegularExpression m_wordRe;
    bool m_enabled = true;
    bool m_suspended = false;
};
