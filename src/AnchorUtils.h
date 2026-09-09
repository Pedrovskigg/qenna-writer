#pragma once

#include <QRect>
#include <QSize>
#include <QPoint>
#include <Qt>

// Posicionamento de popup "grudado" numa barra que pode estar em qualquer
// borda da janela (Top/Left/Right/Bottom via Qt::Edge) — extraído pra cá
// porque antes de existir isso, cada popup (VariationBar, AmbiencePanel,
// HelpPanel, RemindersPanel, FontPickerPopup) reimplementava a conta na mão
// assumindo "a barra-mãe está sempre no topo, então abre embaixo do botão" —
// virou problema real quando a TopToolbar ganhou a opção de ficar vertical
// na lateral direita (2026-09-05): os 5 popups nasciam esticados a partir de
// um botão colado na borda da tela em vez de abrir pro lado certo.
//
// Desenhado pra ser reaproveitado por QUALQUER barra móvel do app — inclusive
// a LeftBar, se um dia ela ganhar a opção de ficar na direita (ideia já
// cogitada, ver memória "geladeira" fora do repo): o mesmo `anchorEdge`
// resolve os dois casos, não é específico da TopToolbar.
namespace AnchorUtils {

// Onde o popup deve "crescer" a partir do anchor, dado o lado da tela onde
// mora a barra que contém o botão-anchor. Ex.: barra no Top -> popup cresce
// pra baixo (abaixo do botão); barra no Right -> popup cresce pra esquerda.
Qt::Edge growthEdgeForBarSide(Qt::Edge barSide);

// Calcula a posição (canto superior-esquerdo) de um popup de tamanho
// `popupSize` ancorado em `anchorGlobal` (retângulo do botão, em coordenadas
// globais), dado o lado da tela onde a barra-mãe mora (`barSide`) e a área
// útil da tela (`screenAvail`). `gap` é o respiro entre o botão e o popup.
//
// Cresce a partir do lado oposto ao de `barSide` (ver growthEdgeForBarSide);
// se não couber nesse sentido, inverte (mesmo espírito do flip vertical que
// já existia nos popups originais) — e sempre faz clamp no eixo perpendicular
// pra nunca deixar o popup vazar pra fora da tela.
QPoint positionNear(const QRect& anchorGlobal, const QSize& popupSize,
                     Qt::Edge barSide, const QRect& screenAvail, int gap = 6);

} // namespace AnchorUtils
