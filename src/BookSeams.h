#pragma once
// Manuscrito completo: a costura entre os capítulos no editor.
//
// Com o livro inteiro aberto, cada capítulo começa num bloco vazio marcado
// (a "abertura"). O bloco não guarda texto nenhum e nunca vai pro arquivo:
// quem salva separa o livro de volta em capítulos justamente por ele. O que
// se vê ali (CAPÍTULO 3 entre fios, o título embaixo) é pintado por cima,
// como a quebra de cena.

#include <QList>
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

struct Info {
    QString label;     // "Capítulo 3", "Prólogo"...
    QString title;     // título do capítulo (pode ser vazio)
    bool    focus = false;   // é o capítulo do cursor (onde as ferramentas estão)
};
using InfoFn = std::function<Info(const QString& chapterId)>;

// Espaço da abertura (margens do bloco, sem recuo). Só mexe no que está
// diferente; joinPrevious junta ao último passo do desfazer.
void applySpacing(QTextDocument* doc, bool joinPrevious);

// Pinta as aberturas visíveis. Devolve a área de cada uma com o tooltip.
QList<QPair<QRect, QString>> paint(QTextEdit* editor, QPainter& p, const QRect& clip, const InfoFn& info);

} // namespace BookSeams
