#pragma once
// Manuscrito completo: a costura entre os capítulos no editor.
//
// Com o livro inteiro aberto, cada capítulo começa num bloco vazio marcado
// (a "abertura"). O bloco não guarda texto nenhum e nunca vai pro arquivo:
// quem salva separa o livro de volta em capítulos justamente por ele. O que
// se vê ali (CAPÍTULO 3 entre fios, o título embaixo) é pintado por cima,
// como a quebra de cena.
//
// O desenho tem seis estilos (o "nó" do capítulo), escolhidos no botão direito
// da abertura ou do nome na faixa de cima, e em Configurações → Escrita. A
// faixa de cima (DocHeaderBar) usa o mesmo nó em miniatura, em qualquer
// documento, a não ser que a pessoa prefira a faixa simples de antes.

#include <QColor>
#include <QList>
#include <QObject>
#include <QPair>
#include <QRect>
#include <QString>
#include <QTextFormat>
#include <functional>

class QPainter;
class QTextBlock;
class QTextDocument;
class QTextEdit;

namespace BookSeams {

// Propriedade do bloco de abertura: o id do capítulo que começa ali.
constexpr int kSeamProperty = QTextFormat::UserProperty + 0x5E7;

bool isSeam(const QTextBlock& b);
QString seamChapter(const QTextBlock& b);

// ---- estilo do nó (vale pro app inteiro, como a quebra de cena) ----
enum Style { Threads = 0, Knot = 1, Numeral = 2, Roman = 3, Margin = 4, Fleuron = 5 };
constexpr int kStyleCount = 6;
Style style();
void setStyle(Style s);
QString styleName(Style s);
QString styleHint(Style s);
// A faixa de cima com o nó (padrão) ou simples, como era antes.
bool nodeInHeader();
void setNodeInHeader(bool on);

class Notifier : public QObject {
    Q_OBJECT
signals:
    void changed();
};
Notifier* notifier();

// O título do capítulo, sem repetir o rótulo: "Capítulo 15" com título
// "Capítulo 15" vira vazio; "Capítulo 10 - Fim do ato" vira "Fim do ato".
QString ownTitle(const QString& title, const QString& label);

struct Info {
    QString label;     // "Capítulo 3", "Prólogo"...
    QString kind;      // "Capítulo", "Prólogo" (o rótulo sem o número)
    QString number;    // "3" ou "III", como o projeto numera; vazio = sem número
    int     numberInt = 0;
    QString title;     // título próprio, já sem repetir o rótulo (ver ownTitle)
    bool    focus = false;   // é o capítulo do cursor (onde as ferramentas estão)
};
using InfoFn = std::function<Info(const QString& chapterId)>;

// Espaço da abertura (margens do bloco, sem recuo). Só mexe no que está
// diferente; joinPrevious junta ao último passo do desfazer. O respiro de
// cima depende do estilo (o Numeral é mais alto que os Fios).
void applySpacing(QTextDocument* doc, bool joinPrevious);

// Pinta as aberturas visíveis. Devolve a área de cada uma com o tooltip.
QList<QPair<QRect, QString>> paint(QTextEdit* editor, QPainter& p, const QRect& clip, const InfoFn& info);

// ---- o nó em miniatura, na faixa de cima ----
struct Head {
    QString label;      // rótulo pequeno: "Capítulo 14", o nome da gaveta, o papel
    QString title;      // linha principal (título próprio, a cena ou o nome do doc)
    QString scene;      // cena, quando o título é o próprio do capítulo
    QString kind;       // "Capítulo" (manuscrito)
    QString number;     // número como o projeto mostra; vazio fora do manuscrito
    int     numberInt = 0;
    bool    manuscript = false;
    bool    sceneAsTitle = false;   // capítulo sem título: a cena virou a linha principal
    bool    hasAvatar = false;      // a foto já está do lado (ficha): nada de marca no lugar
    bool isEmpty() const { return label.isEmpty() && title.isEmpty(); }
};
struct HeadPalette {
    QColor accent, text, muted;
    QString serifFamily;
    qreal scale = 1.0;
};
// Largura natural do nó (sem elipse).
qreal headWidth(Style s, const Head& h, const HeadPalette& pal, qreal height);
// Desenha dentro de r (centralizado; o Margem encosta à esquerda). Devolve o
// retângulo ocupado.
QRectF paintHead(QPainter& p, const QRectF& r, Style s, const Head& h, const HeadPalette& pal);

} // namespace BookSeams
