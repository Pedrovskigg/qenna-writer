#pragma once
// O selo do grupo (27/10, grupos em árvore): Lugar = bolinha cheia na cor do
// grupo com um alfinete; Social = só o contorno na cor, com duas pessoas.
// O mesmo desenho no painel de Grupos, no seletor de grupo e no Explorar do mapa.

#include <QColor>
#include <QPointF>

class QPainter;

namespace GroupBadge {
// r = raio da bolinha (8 dá um selo de 16 px)
void draw(QPainter& p, QPointF c, double r, const QColor& color, bool social);
}
