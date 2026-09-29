#pragma once
// Base das "folhas" do Qenna: as janelas de criar/editar (capítulo, cena,
// manuscrito, personagem, gaveta, pasta…) no lugar das janelas do sistema.
// Uma folha na cor da página do editor, sem moldura, com sombra; a janela do
// app escurece por baixo enquanto ela está aberta. Em cima, uma linha dizendo
// o que se está fazendo ("NOVO MANUSCRITO") e o ×; embaixo, a dica do Enter,
// Cancelar e o botão principal. Os estilos (campos, pílulas, rótulos) são
// compartilhados por objectName — ver applySheetTheme().

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QWidget>

class QButtonGroup;
class QFrame;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QToolButton;
class QVBoxLayout;

class SheetDialog : public QDialog {
    Q_OBJECT
public:
    explicit SheetDialog(QWidget* parent, int width = 420);

    // Abre com o véu escuro na janela do app e centralizado nela.
    int exec() override;
    // Abre SEM modal, ao lado de um painel (a gaveta), sem escurecer nada e sem
    // tirar o foco do editor. Quem chama liga accepted() e usa
    // WA_DeleteOnClose; o resultado chega depois, não na volta desta função.
    void openBeside(QWidget* anchor);
    // Igual, mas flutuando DENTRO de uma área (o quadro da Lousa), no canto de
    // cima à esquerda: quando não há gaveta do lado pra encostar.
    void openInside(QWidget* area);

    // ── peças prontas, já com o estilo da folha ──
    static QLabel* sectionLabel(const QString& text, QWidget* parent);
    // Título grande em serifa (o nome do que está sendo criado).
    static QLineEdit* titleEdit(QWidget* parent, bool centered = true, qreal px = 27);
    static QLineEdit* field(QWidget* parent, bool mono = false);
    static QTextEdit* textArea(QWidget* parent, int height);
    // Pílula de escolha (checkable); agrupe com QButtonGroup. Pra escolher
    // entre opções, prefira SheetChoice (ela decide pílula ou lista).
    static QToolButton* pill(const QString& text, QWidget* parent);
    // A setinha "▾" das linhas que abrem lista.
    static QIcon chevronIcon(const QColor& c);

protected:
    QFrame*      card() const { return m_card; }
    QVBoxLayout* body() const { return m_body; }
    void setEyebrow(const QString& text);
    // Troca a linha de cima ("NOVO X" + ×) por um cabeçalho próprio, colado
    // nas bordas da folha (o pôster do personagem). Esc continua cancelando.
    void replaceHeader(QWidget* header);
    // Rodapé: dica do Enter + Cancelar + principal. Devolve o principal
    // (já ligado a accept; troque a conexão se precisar validar antes).
    QPushButton* addFooter(const QString& okText, const QString& enterHint);
    void setEnterHint(const QString& text);   // troca o "cria" por "salva" no modo editar
    // A fila do rodapé (depois de addFooter), pra quem quer um botão a mais à esquerda.
    QHBoxLayout* footer() const { return m_foot; }
    // Enter nos campos de linha única confirma (o principal precisa estar ativo).
    void confirmOnEnter(QWidget* w);
    bool eventFilter(QObject* o, QEvent* e) override;
    // Estilo compartilhado + o que a subclasse quiser acrescentar.
    void applySheetTheme(const QString& extraQss = QString());
    void setPlaceholderColors();
    void resizeEvent(QResizeEvent* e) override;

    QPushButton* m_okBtn = nullptr;

private:
    void fitToContent();
    void keepOnScreen();
    QFrame*      m_card = nullptr;
    QVBoxLayout* m_root = nullptr;
    QVBoxLayout* m_body = nullptr;
    QLayout*     m_top = nullptr;
    QLabel*      m_eyebrow = nullptr;
    QToolButton* m_close = nullptr;
    QLabel*      m_hint = nullptr;
    QHBoxLayout* m_foot = nullptr;
};

// Escolha entre opções numa folha, com a regra do Pe: de 2 a 4 opções,
// pílulas; acima disso, pílula polui (a categoria do sistema dava 4 linhas
// delas) — vira uma linha só ("Magia ▾") que abre a lista na própria folha,
// em duas colunas, como o Papel do pôster. Nada de menu suspenso: janela
// suspensa sai com fundo transparente no Windows sobre a folha translúcida.
class SheetChoice : public QWidget {
    Q_OBJECT
public:
    static constexpr int kMaxPills = 4;

    // pillsAlign: onde as pílulas ficam na linha (a folha de capítulo centraliza).
    // availWidth: largura pra quebrar as pílulas em mais de uma linha (0 = uma só).
    SheetChoice(const QStringList& labels, QWidget* parent,
                Qt::Alignment pillsAlign = Qt::AlignLeft, int availWidth = 0);

    int  currentIndex() const { return m_index; }
    void setCurrentIndex(int i);
    bool isList() const { return m_line != nullptr; }
    // Lista sempre aberta, sem a linha (quando a escolha é a folha inteira).
    void setAlwaysOpen();

signals:
    void activated(int index);   // o autor escolheu (não dispara no setCurrentIndex)

private:
    void refresh();
    void toggleList(bool open);
    QStringList   m_labels;
    int           m_index = -1;
    QButtonGroup* m_group = nullptr;   // modo pílula
    QPushButton*  m_line = nullptr;    // modo lista
    QLabel*       m_lineText = nullptr;
    QFrame*       m_list = nullptr;
    QList<QPushButton*> m_cells;
};
