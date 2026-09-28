#pragma once
// Quebra de cena no editor: o desenho no lugar do <hr>.
//
// O Qt pinta o <hr> com QPalette::Dark, uma cor do sistema que o tema nunca
// define — em página clara ela some, em página escura grita — e herda o recuo
// da primeira linha (fica torta). Aqui o traço do Qt fica transparente e a
// quebra é pintada por cima, em um de dois estilos (Configurações → Escrita):
//   Knot — "Nó da Timeline": a bolinha da Timeline entre dois fios, na cor da
//          linha da cena (Flashback na cor dele, vazada quando sem data);
//   Name — "Nome da cena": CENA 2 em versalete entre fios, com o título e o
//          "quando se passa" da cena embaixo.
// A cor sai do tema e é puxada na direção do texto até ter contraste mínimo
// contra a página — nunca some, nunca fica mais forte que o texto.
//
// Nada disso vai pro arquivo: o espaço em volta são margens do bloco do <hr>,
// que o exportador de HTML do Qt não grava (ele escreve só "<hr />").

#include <QList>
#include <QObject>
#include <QPair>
#include <QRect>
#include <QString>
#include <functional>

class QPainter;
class QTextDocument;
class QTextEdit;

namespace SceneBreaks {

enum Style { Knot = 0, Name = 1 };
Style style();
void setStyle(Style s);

class Notifier : public QObject {
    Q_OBJECT
signals:
    void styleChanged();
};
Notifier* notifier();

struct Info {
    bool    scene = false;    // false = <hr> fora de manuscrito: só um fio discreto
    int     number = 0;       // número da cena que começa depois da quebra
    QString title;            // título próprio da cena (vazio = só o número)
    QString marker;           // "quando se passa" efetivo (a cena herda do capítulo)
    bool    flashback = false;
};
using InfoFn = std::function<Info(int breakIndex)>;

// Some com o traço que o próprio Qt desenha no <hr>: no Qt 6 ele sai na cor
// WindowText da paleta do editor (a cor de fundo do bloco também serviria,
// mas ela VAI pro HTML salvo e quebra a divisão de cenas, que procura <hr>
// puro). Chamar depois de cada setPalette do editor.
void hideQtRuler(QTextEdit* editor);
// true se a paleta do editor ainda esconde o traço (barato: chamar a cada pintura).
bool qtRulerHidden(const QTextEdit* editor);

// Espaço em volta de cada quebra + recuo zerado. Só mexe no que está diferente
// (seguro chamar a cada edição). joinPrevious = junta ao último passo do
// desfazer (a quebra recém-digitada e o espaço dela saem juntos no Ctrl+Z).
void applySpacing(QTextDocument* doc, bool joinPrevious);

// Pinta as quebras visíveis. Devolve a área de cada uma com o texto do tooltip.
QList<QPair<QRect, QString>> paint(QTextEdit* editor, QPainter& p, const QRect& clip, const InfoFn& info);

// Contraste WCAG entre duas cores; e a cor base puxada na direção do texto
// até chegar a `min` contra a página.
double contrast(const QColor& a, const QColor& b);
QColor guarded(const QColor& base, const QColor& page, const QColor& text, double min = 3.0);

} // namespace SceneBreaks
