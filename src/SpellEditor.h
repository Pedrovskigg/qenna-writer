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

    // Devolve uma lista CURTA de sinônimos para o submenu rápido do menu de
    // contexto (o gesto do Word: botão direito, Sinônimos, clica, pronto).
    // Injetado de fora pra não amarrar o editor ao dicionário.
    using SynonymProvider = std::function<QStringList(const QString&)>;
    void setSynonymProvider(SynonymProvider fn) { m_synProvider = std::move(fn); }

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

    // "Sinônimos" no menu de contexto — abre a lista completa. Só dispara para
    // uma palavra única.
    void synonymsRequested(QString word, QPoint globalPos);

    // Troca direta pelo submenu rápido, sem abrir painel nenhum.
    void synonymChosen(QString replacement);

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
    SynonymProvider m_synProvider;
    bool m_screenplayMode = false;
};
