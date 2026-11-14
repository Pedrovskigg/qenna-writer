#pragma once

#include "AmbienceManager.h"

class QPainter;
class QRectF;

// Vinheta de um som ambiente: cena pequena desenhada à mão por categoria
// (chuva caindo, mar, floresta, cidade de noite, espaço, névoa de suspense),
// variando pelo nome do som. Som com imagem ao lado do áudio (ex.: a capa do
// P.H.L) usa a imagem. `t` em segundos anima a cena (chuva cai, estrela
// pisca); passe sempre o mesmo t pra uma vinheta parada.
// Porta direta do drawArt() do lab (concepts/som-imersivo/som-imersivo.html).
namespace AmbienceArt {

void paint(QPainter& p, const QRectF& rect, const AmbienceManager::Track& track, double t);

}
