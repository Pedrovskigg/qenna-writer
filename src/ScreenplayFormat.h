#pragma once

#include <QSet>
#include <QString>
#include <QTextBlockFormat>

class QTextBlock;
class QTextCursor;
class QTextDocument;

// Elementos de formatação de roteiro (padrão Cena/Ação/Personagem/Diálogo/
// Parênteses/Transição). A geometria (margens/alinhamento) é a própria fonte
// da verdade — sobrevive ao toHtml()/setHtml() usado para salvar o documento,
// diferente de uma propriedade custom em QTextFormat (que se perde nesse
// round-trip, como já vimos com marcadores do Word). detect() reconstrói o
// elemento a partir da geometria + conteúdo ao reabrir um documento salvo.
// Durante a sessão o bloco também carrega o elemento numa propriedade — é o
// que separa Cena de Ação (mesma caixa) enquanto a linha ainda está vazia.
enum class ScreenplayElement { Scene, Action, Character, Dialogue, Parenthetical, Transition };

namespace ScreenplayFormat {

QString label(ScreenplayElement element);

// Ordem de ciclo do Tab: Cena → Ação → Personagem → Diálogo → Parênteses →
// Transição → Cena. Shift+Tab anda ao contrário.
ScreenplayElement cycleElement(ScreenplayElement current, bool backwards = false);

// Elemento "lógico" seguinte ao apertar Enter, dado o elemento atual.
ScreenplayElement nextElement(ScreenplayElement current);

// Aplica a geometria (margens/alinhamento/espaço antes) do elemento ao bloco
// do cursor. Não mexe em fonte/tamanho (isso continua vindo do Courier New global).
void applyBlockFormat(QTextCursor& cursor, ScreenplayElement element);

// A mesma geometria em medida de papel, pra exportação: 1 caractere = 0,1
// polegada e 6 linhas por polegada, a 96 dpi (a régua do QTextDocument).
void applyPageFormat(QTextCursor& cursor, ScreenplayElement element);
constexpr qreal kPageCharPx = 9.6;
constexpr qreal kPageLinePx = 16.0;

// Reconstrói o elemento de um bloco existente a partir da geometria e do
// texto (prefixo de cena tipo "INT."/"EXT." desempata Cena vs. Ação, que
// compartilham a mesma caixa flush-left/full-width).
ScreenplayElement detect(const QTextBlockFormat& format, const QString& text);

// Cena do Qenna = trecho entre quebras (<hr>). No roteiro, o cabeçalho é a
// quebra: ensureSceneBreaks põe um <hr> antes de cada INT./EXT. que não abre o
// documento (devolve quantos pôs) e compactSceneBreaks tira o espaço e o traço
// dele — quem separa na página é o próprio cabeçalho.
bool isSceneBreak(const QTextBlock& block);
int ensureSceneBreaks(QTextDocument* doc, bool joinPrevious);
void compactSceneBreaks(QTextDocument* doc, bool joinPrevious);
// Primeiro cabeçalho de cena de um trecho de HTML ("" se não tem).
QString firstSceneHeading(const QString& html);

// Reaplica a geometria certa em todos os blocos (documento recém-aberto, ou
// salvo por uma versão que não tinha o espaçamento por elemento).
void normalizeDocument(QTextDocument* doc);

// "INT. CASA - NOITE", "EXT./INT.", "INTERNA"...
bool isSceneHeading(const QString& text);

// Partes do cabeçalho de cena "INT. LANCHONETE - NOITE": o comprimento do
// prefixo com o espaço depois ("INT. " → 5; -1 se não tem), o local
// ("LANCHONETE") e a hora ("NOITE", depois do último " - ").
int scenePrefixLength(const QString& text);
QString sceneLocation(const QString& heading);
QString sceneTime(const QString& heading);
// INT, EXT ou os dois (INT./EXT.).
bool sceneIsInterior(const QString& heading);
bool sceneIsExterior(const QString& heading);

// Cena, Personagem e Transição vão em caixa alta.
bool isUppercaseElement(ScreenplayElement element);

// Nome do personagem sem as extensões: "CIDA (CONT'D)" → "CIDA",
// "IRENE (V.O.)" → "IRENE".
QString cueName(const QString& text);

// Ao sair de uma linha de Ação (Enter, clique, setas): uma linha curta toda em
// maiúsculas vira Personagem; "CORTA PARA:" vira Transição. knownCues (em
// MAIÚSCULAS) deixa passar nome conhecido mesmo comprido.
ScreenplayElement refineOnEnter(ScreenplayElement current, const QString& text,
                                const QSet<QString>& knownCues);

// Largura da coluna de texto do roteiro (60 caracteres de Courier 12, a
// página-padrão de 6 polegadas), em pixels de tela.
qreal columnWidthPx();

}
