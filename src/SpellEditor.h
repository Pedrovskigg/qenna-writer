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

    // Modo roteiro: Tab cicla o elemento do bloco atual (Cena/Ação/Personagem/
    // Diálogo/Parênteses/Transição) e Enter avança pro elemento lógico
    // seguinte. Setado pela MainWindow via applyEditorStyle() — só true
    // quando o doc é manuscrito (capítulo/cena) de um projeto tipo roteiro.
    void setScreenplayMode(bool on) { m_screenplayMode = on; }
    bool isScreenplayMode() const { return m_screenplayMode; }

signals:
    // Disparado quando o usuário escolhe "Adicionar ao Glossário..." no menu de
    // contexto. word = texto selecionado (ou WordUnderCursor), pos = global.
    void addToGlossaryRequested(QString word, QPoint globalPos);

    // Ctrl+clique sobre um link de referência (menção @ ou nome do Codex).
    // href no formato "ref:<drawerKey>:<itemId>".
    void refActivated(QString href);

    // Ctrl pressionado/solto — pede pra realçar/ocultar os links de referência.
    void refHighlightRequested(bool on);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    SpellChecker* m_checker = nullptr;
    bool m_screenplayMode = false;
};
