#pragma once

// Peças internas dos sprites (faces 3D, caixa, tronco de cone, cone, ameias, bandeira),
// divididas entre CartoSprites.cpp (casas, castelo, artefatos de sempre) e
// CartoSpritesArts.cpp (as levas novas). Não é API do mapa: só os dois arquivos usam.

#include "CartoSprites.h"

#include <QColor>
#include <QPainter>
#include <QPen>

#include <array>
#include <functional>
#include <vector>

namespace carto::spr::kit {

using V3 = std::array<double, 3>;

QColor rgb(const RGB& c, double k = 1, double a = 1);
RGB mix(const RGB& a, const RGB& b, double t);
double frac(double x);
// xorshift32 (o mesmo do Lab)
struct Rng {
    uint32_t s;
    explicit Rng(double seed) { s = toUint32(seed); if (!s) s = 1; }
    double operator()() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s / 4294967296.0; }
};
QPen pen(const QColor& c, double w, Qt::PenCapStyle cap = Qt::FlatCap);
void line(QPainter& p, QPointF a, QPointF b);

using PtFn = std::function<QPointF(double, double)>;
using Deco = std::function<void(QPainter&, const PtFn&, double)>;
// uma face: vértices no mundo, cor, centro do sólido (pra saber o lado de fora), detalhe desenhado em cima
struct Face {
    std::vector<V3> v;
    RGB col{};
    bool hasC = false;
    V3 c{};
    double bias = 0;
    bool flat = false;            // plano fino (vela, lâmina, membrana): desenha dos dois lados, sem sombra
    std::array<int, 4> q = { 0, 1, 2, 3 };
    Deco deco;
};
using Faces = std::vector<Face>;
Face face(std::vector<V3> v, const RGB& col, const V3& c, Deco deco = {}, double bias = 0);
void drawFaces(QPainter& p, const Faces& faces, const Cam& cam);
void quad(QPainter& p, const PtFn& pt, double s0, double s1, double t0, double t1, const QColor& fill);
Faces boxFaces(double cx, double cy, double yaw, double w, double d, double z0, double z1, const RGB& col, const RGB* topCol = nullptr,
               const std::function<Deco(int, double)>& deco = {});
void append(Faces& a, Faces b);
extern const RGB DARK;
void roofDeco(QPainter& p, const PtFn& pt, double);
Deco slitDeco(int n);
void merlons(Faces& faces, double ax, double ay, double bx, double by, double z, double thick, const RGB& col);
Faces taperFaces(double x, double y, double r0, double r1, double z0, double z1, int n, const RGB& col, const std::function<Deco(int)>& deco = {});
Faces coneFaces(double x, double y, double r, double z0, double h, int n, const RGB& col);
void flag(QPainter& p, const Cam& cam, double x, double y, double z, const RGB& col);

} // namespace carto::spr::kit
