#pragma once

#include <QPointF>

class QPainter;
class QWidget;

namespace galaxy {

struct System;

// Até onde o desenho vai, em raios da sombra: quem pinta usa pra saber o tamanho na tela.
inline constexpr double BLACK_HOLE_EXTENT = 5.0;

// O disco do buraco negro: a direção do eixo no mundo (a = quanto ele deita, b = pra que lado).
void blackHoleAxis(const System& s, double& a, double& b);

// Pinta o buraco negro com a luz do disco curvada em volta da sombra (shadowPx = raio da sombra na tela).
// tilt e yaw são os da câmera do mapa; simT gira o disco. Com wake, o que for pesado acontece em segundo plano
// e o widget é avisado quando ficar pronto; sem wake, sai tudo certo já nesta pintura.
void paintBlackHole(QPainter& p, QPointF at, double shadowPx, double a, double b, double tilt, double yaw, double simT, QWidget* wake = nullptr);

} // namespace galaxy
