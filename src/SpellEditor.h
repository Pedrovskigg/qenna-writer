#pragma once

#include <QTextEdit>

class SpellChecker;
class QContextMenuEvent;

class SpellEditor : public QTextEdit {
    Q_OBJECT
public:
    explicit SpellEditor(QWidget* parent = nullptr);

    void setSpellChecker(SpellChecker* checker);
    SpellChecker* spellChecker() const { return m_checker; }

    // Expõe setViewportMargins (protected em QAbstractScrollArea) para a
    // MainWindow ajustar o respiro interno da "página" de escrita.
    void setPageMargins(int left, int top, int right, int bottom)
    {
        setViewportMargins(left, top, right, bottom);
    }

signals:
    // Disparado quando o usuário escolhe "Adicionar ao Glossário..." no menu de
    // contexto. word = texto selecionado (ou WordUnderCursor), pos = global.
    void addToGlossaryRequested(QString word, QPoint globalPos);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    SpellChecker* m_checker = nullptr;
};
