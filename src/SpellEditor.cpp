#include "SpellEditor.h"

#include "SpellChecker.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QFont>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QTextCursor>

namespace {
bool isRefHref(const QString& href) {
    return href.startsWith(QStringLiteral("ref:"));
}
}

namespace {
constexpr int kMaxSuggestions = 6;
}

SpellEditor::SpellEditor(QWidget* parent)
    : QTextEdit(parent)
{
}

void SpellEditor::setSpellChecker(SpellChecker* checker)
{
    m_checker = checker;
}

void SpellEditor::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu(event->pos());

    // Marcador de inserção — tudo que adicionamos (spell + glossário) vai antes
    // das ações padrão (Undo, Cut, Copy...).
    QAction* stdAnchor = menu->actions().isEmpty() ? nullptr : menu->actions().first();

    if (m_checker && m_checker->isEnabled()) {
        QTextCursor cursor = cursorForPosition(event->pos());
        cursor.select(QTextCursor::WordUnderCursor);
        const QString word = cursor.selectedText();

        if (!word.isEmpty() && !m_checker->isCorrect(word)) {
            const QStringList suggestions = m_checker->suggest(word);

            QAction* firstStandardAction = menu->actions().isEmpty()
                ? nullptr : menu->actions().first();

            QFont boldFont;
            boldFont.setBold(true);

            // Insere as sugestões no topo do menu, em ordem.
            if (suggestions.isEmpty()) {
                QAction* noSugg = new QAction(tr("(sem sugestões)"), menu);
                noSugg->setEnabled(false);
                menu->insertAction(firstStandardAction, noSugg);
            } else {
                const int n = qMin(kMaxSuggestions, suggestions.size());
                for (int i = 0; i < n; ++i) {
                    const QString suggestion = suggestions.at(i);
                    QAction* act = new QAction(suggestion, menu);
                    act->setFont(boldFont);
                    connect(act, &QAction::triggered, this, [this, cursor, suggestion]() mutable {
                        cursor.beginEditBlock();
                        cursor.insertText(suggestion);
                        cursor.endEditBlock();
                    });
                    menu->insertAction(firstStandardAction, act);
                }
            }

            // Ações específicas da palavra: adicionar / ignorar (uma vez nesta sessão).
            menu->insertSeparator(firstStandardAction);

            QAction* addAct = new QAction(tr("Adicionar \"%1\" ao dicionário").arg(word), menu);
            connect(addAct, &QAction::triggered, this, [this, word]() {
                if (m_checker) m_checker->addToPersonalDictionary(word);
            });
            menu->insertAction(firstStandardAction, addAct);

            menu->insertSeparator(firstStandardAction);
        }
    }

    // "Adicionar ao Glossário..." — SEMPRE disponível, mesmo sem seleção.
    // Usa a seleção atual ou, se vazia, a palavra sob o cursor. Inserido logo
    // antes das ações padrão (Undo, Cut, Copy...). Trim de paragraph-separator
    // ( ) e whitespace pra label não vir vazia com "lixo".
    {
        QString glossarySeed;
        QTextCursor selCursor = textCursor();
        if (selCursor.hasSelection()) {
            glossarySeed = selCursor.selectedText();
        } else {
            QTextCursor probe = cursorForPosition(event->pos());
            probe.select(QTextCursor::WordUnderCursor);
            glossarySeed = probe.selectedText();
        }
        glossarySeed = glossarySeed.trimmed();
        glossarySeed.remove(QChar(0x2029));

        const QString label = glossarySeed.isEmpty()
            ? tr("Adicionar ao Glossário...")
            : tr("Adicionar \"%1\" ao Glossário...").arg(glossarySeed.left(40));
        QAction* glsAct = new QAction(label, menu);
        const QPoint globalPos = event->globalPos();
        const QString seedCopy = glossarySeed;
        connect(glsAct, &QAction::triggered, this, [this, seedCopy, globalPos]() {
            emit addToGlossaryRequested(seedCopy, globalPos);
        });
        menu->insertAction(stdAnchor, glsAct);
        menu->insertSeparator(stdAnchor);
    }

    menu->exec(event->globalPos());
    delete menu;
}

void SpellEditor::mousePressEvent(QMouseEvent* event)
{
    // Ctrl+clique num link de referência abre o doc no RefMenu (não posiciona cursor).
    if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ControlModifier)) {
        const QString href = anchorAt(event->pos());
        if (isRefHref(href)) {
            emit refActivated(href);
            event->accept();
            return;
        }
    }
    QTextEdit::mousePressEvent(event);
}

void SpellEditor::mouseMoveEvent(QMouseEvent* event)
{
    // Com Ctrl segurado, mostra a mãozinha sobre links de referência.
    if (event->modifiers() & Qt::ControlModifier) {
        const QString href = anchorAt(event->pos());
        viewport()->setCursor(isRefHref(href) ? Qt::PointingHandCursor : Qt::IBeamCursor);
    }
    QTextEdit::mouseMoveEvent(event);
}

void SpellEditor::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control && !event->isAutoRepeat())
        emit refHighlightRequested(true);   // "modo ver os links"
    QTextEdit::keyPressEvent(event);
}

void SpellEditor::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control && !event->isAutoRepeat())
        emit refHighlightRequested(false);
    QTextEdit::keyReleaseEvent(event);
}
