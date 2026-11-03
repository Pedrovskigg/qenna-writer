#pragma once

// As vistas de arte do Galaxy Maps: Tinta, Pergaminho, Carta estelar, Topográfico, Aquarela e Planta de guerra
// (porte de views.js). Papel e tinta seguem os do Mapa (CartoPaint). A galáxia vira campo de densidade, e daí saem
// as manchas, as curvas e o pontilhado; planeta, peça 3D e nave passam pelo stylize, que troca a cor pela técnica.

#include "GalaxyCore.h"

#include <QColor>
#include <QImage>
#include <QString>

namespace galaxy {

using Rgb = std::array<int, 3>;
enum class SkinKind { Dither, Bands, Water };
enum class WashKind { None, Parch, Chart, Topo, Aqua, Blue };
struct Curve { double t, width, alpha; bool dashed; };
struct Skin {
    QString id;
    Rgb paper, ink, dark, light, sel, hz, halo;
    SkinKind kind = SkinKind::Dither;
    WashKind wash = WashKind::None;
    int levels = 1, grain = 3, stipN = 0;
    double gain = 1.2, gamma = 0.85, stipA = 0, stipP = 1;
    bool mono = false, hatch = true, vignette = false, chart = false, graticule = false, darkPaper = false, colored = false, soft = false;
    std::vector<Curve> curves;
    QColor paperC() const { return QColor(paper[0], paper[1], paper[2]); }
    QColor inkC(double a = 1) const { return QColor(ink[0], ink[1], ink[2], int(a * 255 + 0.5)); }
    QColor selC(double a = 1) const { return QColor(sel[0], sel[1], sel[2], int(a * 255 + 0.5)); }
    QColor hzC(double a = 1) const { return QColor(hz[0], hz[1], hz[2], int(a * 255 + 0.5)); }
    QColor haloC(double a = 1) const { return QColor(halo[0], halo[1], halo[2], int(a * 255 + 0.5)); }
};
const Skin* skinOf(const QString& id);

void stylize(uint8_t* d, int w, int h, const Skin& sk, bool outline = false, double floor = 0);

struct Field { int N = 0; std::vector<float> D; };
Field densityField(const QImage& gal, int N = 512);
QImage washImage(const Field& F, const Skin& sk, uint32_t seed);
struct CurveSegs { Curve L; std::vector<float> seg; };   // x0 y0 x1 y1, no espaço do mundo
std::vector<CurveSegs> contours(const Field& F, const std::vector<Curve>& levels);
struct Stipple { std::vector<float> xs, ys; int n = 0; };
Stipple stipple(const Field& F, const Skin& sk, uint32_t seed);
QImage paperTile(const Skin& sk);

} // namespace galaxy
