#pragma once

#include <QString>
#include <QTextBlockFormat>

// Elementos de formatação de roteiro (padrão Cena/Ação/Personagem/Diálogo/
// Parênteses/Transição). A geometria (margens/alinhamento) é a própria fonte
// da verdade — sobrevive ao toHtml()/setHtml() usado para salvar o documento,
// diferente de uma propriedade custom em QTextFormat (que se perde nesse
// round-trip, como já vimos com marcadores do Word). detect() reconstrói o
// elemento a partir da geometria + conteúdo ao reabrir um documento salvo.
enum class ScreenplayElement { Scene, Action, Character, Dialogue, Parenthetical, Transition };

namespace ScreenplayFormat {

QString label(ScreenplayElement element);

// Ordem de ciclo do Tab: Cena → Ação → Personagem → Diálogo → Parênteses →
// Transição → Cena.
ScreenplayElement cycleElement(ScreenplayElement current);

// Elemento "lógico" seguinte ao apertar Enter, dado o elemento atual.
ScreenplayElement nextElement(ScreenplayElement current);

// Aplica a geometria (margens/alinhamento) do elemento ao bloco do cursor.
// Não mexe em fonte/tamanho (isso continua vindo do Courier New global).
void applyBlockFormat(QTextCursor& cursor, ScreenplayElement element);

// Reconstrói o elemento de um bloco existente a partir da geometria e do
// texto (prefixo de cena tipo "INT."/"EXT." desempata Cena vs. Ação, que
// compartilham a mesma caixa flush-left/full-width).
ScreenplayElement detect(const QTextBlockFormat& format, const QString& text);

}
