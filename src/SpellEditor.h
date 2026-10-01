#pragma once

#include <QList>
#include <QMargins>
#include <QPair>
#include <QSet>
#include <QTextCursor>
#include <QTextEdit>
#include <functional>

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
    // Em roteiro, a margem cresce dos dois lados até sobrar a coluna de 60
    // caracteres da página-padrão (senão o diálogo esparrama na tela larga).
    void setPageMargins(int left, int top, int right, int bottom);

    // Modo roteiro: Tab/Shift+Tab ciclam o elemento do bloco atual (Cena/Ação/
    // Personagem/Diálogo/Parênteses/Transição) e Enter avança pro elemento
    // lógico seguinte. Setado pela MainWindow via applyEditorStyle() — só true
    // quando o doc é manuscrito (capítulo/cena) de um projeto tipo roteiro.
    void setScreenplayMode(bool on);
    bool isScreenplayMode() const { return m_screenplayMode; }
    // Nomes (em MAIÚSCULAS) que fazem uma linha de Ação virar Personagem no
    // Enter: elenco do projeto + apelidos. Os já usados no documento contam sozinhos.
    void setScreenplayCues(const QSet<QString>& cues) { m_screenplayCues = cues; }

    // Desenho por cima do texto, depois da pintura normal (a quebra de cena,
    // ver SceneBreaks). Devolve as áreas desenhadas com o texto do tooltip.
    using OverlayPainter = std::function<QList<QPair<QRect, QString>>(QPainter&, const QRect&)>;
    void setOverlayPainter(OverlayPainter fn) { m_overlay = std::move(fn); }
    // Roda logo antes da pintura normal (ex.: garantir a paleta que esconde o
    // traço do <hr> do Qt, que troca de tema e folha de estilo podem desfazer).
    void setBeforePaint(std::function<void()> fn) { m_beforePaint = std::move(fn); }

signals:
    // Disparado quando o usuário escolhe "Adicionar ao Glossário..." no menu de
    // contexto. word = texto selecionado (ou WordUnderCursor), pos = global.
    void addToGlossaryRequested(QString word, QPoint globalPos);

    // "Sinônimos" no menu de contexto — abre a lista completa. Só dispara para
    // uma palavra única.
    void synonymsRequested(QString word, QPoint globalPos);

    // Troca direta pelo submenu rápido, sem abrir painel nenhum.
    void synonymChosen(QString replacement);

    // "Ler em voz alta" no menu de contexto. Com seleção, lê só o trecho
    // (start/end em posições de documento); sem seleção, end = -1 e a leitura
    // vai do cursor até o fim.
    void readAloudRequested(int start, int end);

    // Ctrl+clique sobre um link de referência (menção @ ou nome do Codex).
    // href no formato "ref:<drawerKey>:<itemId>".
    void refActivated(QString href);

    // Ctrl pressionado/solto — pede pra realçar/ocultar os links de referência.
    void refHighlightRequested(bool on);

    // Roteiro: o elemento da linha do cursor pode ter mudado sem o cursor
    // andar (Tab, Shift+Tab, "(", Enter na linha vazia) — pro guia do DocHeader.
    void screenplayElementChanged();
    // Roteiro: a pessoa saiu de uma linha (Enter, clique, setas). É a hora de
    // transformar o cabeçalho recém-escrito em cena do Qenna.
    void screenplayLineFinished();

protected:
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    bool screenplayKeyPress(QKeyEvent* event);
    void onScreenplayCursorMoved();
    void updateScreenplayColumn();
    QSet<QString> screenplayCuesWithDocument() const;

    SpellChecker* m_checker = nullptr;
    SynonymProvider m_synProvider;
    bool m_screenplayMode = false;
    QSet<QString> m_screenplayCues;
    // Última linha digitada à mão: ao sair dela (Enter, clique, setas) a Cena,
    // o Personagem e a Transição vão pra caixa alta.
    QTextCursor m_typedBlock;
    bool m_uppercasing = false;
    QMargins m_pageMargins;
    OverlayPainter m_overlay;
    std::function<void()> m_beforePaint;
    QList<QPair<QRect, QString>> m_overlayTips;
};
