#pragma once

// Peças internas dos sprites (faces 3D, caixa, tronco de cone, cone, ameias, bandeira),
// divididas entre CartoSprites.cpp (casas, castelo, artefatos de sempre) e
// CartoSpritesArts.cpp (as levas novas). Não é API do mapa: só os dois arquivos usam.

#include "CartoSprites.h"

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <array>
#include <cmath>
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
void gateDeco(QPainter& p, const PtFn& pt, double k);
Faces towerFaces(double x, double y, double r, double z1, double roofH, const RGB& col, const RGB& roofCol, bool noRoof);
void wallRing(Faces& out, const std::function<std::array<double, 2>(double, double)>& L, double R, double wallH, double towerR, double towerH,
              bool mid, const RGB& stone, const RGB& roof, bool gate);

// ── peças das levas novas (CartoSpritesArts.cpp) ──
using P2 = std::array<double, 2>;
// sistema do artefato: x pra frente, girado por yaw
struct Loc {
    double x, y, c, s;
    explicit Loc(const Art& b) : x(b.x), y(b.y), c(std::cos(b.yaw)), s(std::sin(b.yaw)) {}
    V3 operator()(double lx, double ly, double z) const { return { x + c * lx - s * ly, y + s * lx + c * ly, z }; }
    P2 at(double lx, double ly) const { return { x + c * lx - s * ly, y + s * lx + c * ly }; }
};
double seedOf(const Art& b, int k);
RGB sc(const RGB& c, double k);
V3 add3(const V3& a, const V3& b);
V3 sub3(const V3& a, const V3& b);
V3 mul3(const V3& a, double k);
V3 cross(const V3& a, const V3& b);
V3 norm(const V3& a);
Face poly(std::vector<V3> v, const RGB& col, const V3& c, Deco deco = {}, double bias = 0);
Face tri(const V3& a, const V3& b, const V3& c, const RGB& col, const V3& C);
Face flatPoly(std::vector<V3> v, const RGB& col, const V3& c, Deco deco = {});
QPainterPath pathOf(const std::vector<QPointF>& pts);
void strokeLines(QPainter& p, const QColor& c, double w);
Faces beamFaces(const V3& p0, const V3& p1, double w, double h, const RGB& col);
Faces ringFaces(double x, double y, double yaw, double ax0, double ay0, double ax1, double ay1, double z0, double z1, int n, const RGB& col,
                const std::function<Deco(int, int)>& deco = {});
Faces blobGeneric(const std::function<V3(double, double, double)>& T, double rx, double ry, double rz, int n, int rings, const RGB& col, double seed,
                  double jitter, const std::vector<RGB>& cols);
Faces blobFaces(double cx, double cy, double cz, double rx, double rz, int n, int rings, const RGB& col, double seed, double jitter = 0.18, const std::vector<RGB>& cols = {});
Faces ellip(const Loc& L, double lx, double ly, double lz, double rx, double ry, double rz, int n, int rings, const RGB& col, double seed, double jitter = 0.1);
Faces above(Faces f, double z);
Faces below(Faces f, double z);
Faces moundFaces(double x, double y, double r, double h, double seed, const RGB& col);
Faces tube(const std::vector<V3>& pts, double w0, double w1, const RGB& col, const RGB* alt = nullptr);
void glowAt(QPainter& p, const Cam& cam, const V3& at, double rWorld, const RGB& col, double a, double minR = 4);
void foam(QPainter& p, const Cam& cam, const P2& at, double rx, double ry, double yaw, double a);
void groundShadow(QPainter& p, const Cam& cam, double x, double y, double r, double a);
Cam detailed(const Cam& cam);

} // namespace carto::spr::kit
