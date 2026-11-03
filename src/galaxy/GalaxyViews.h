#pragma once

// As vistas de arte do Galaxy Maps: as de papel (Tinta, Pergaminho, Carta estelar, Topográfico, Aquarela e Planta de guerra)
// e as nascidas do espaço, que leem o que a galáxia tem e o papel não mostra (porte de views.js). Papel e tinta seguem os do Mapa (CartoPaint). A galáxia vira campo de densidade, e daí saem
// as manchas, as curvas e o pontilhado; planeta, peça 3D e nave passam pelo stylize, que troca a cor pela técnica.

#include "GalaxyCore.h"

#include <QColor>
#include <QImage>
#include <QString>

#include <array>
#include <memory>

namespace galaxy {

using Rgb = std::array<int, 3>;
enum class SkinKind { Dither, Bands, Water, Lut, Poster };
enum class WashKind { None, Parch, Chart, Topo, Aqua, Blue, Holo, Harmony, Thermal, Xray, Plate, Routes };
enum class SpacePaint { None, Holo, Routes, Domains, Gravity, Harmony, Thermal, Plate };
enum class StarSign { Atlas, Holo, Station, Domain, Grav, Engraved, Thermal, Xray, Plate };
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
    // só nas nascidas do espaço
    bool space = false, photo = false, thermal = false, noTerr = false, regionCenter = false, shadeNone = false, hasSun = false, hasShadow = false, hasLabelInk = false, plainWeight = false;
    int scan = 0, regionPx = 0, labelPlus = 1;
    Rgb sun {}, shadow {}, labelInk {};
    std::shared_ptr<const std::array<uint8_t, 768>> grad;
    SpacePaint paint = SpacePaint::None;
    StarSign sign = StarSign::Atlas;
    QString face = QStringLiteral("IM FELL English SC");   // vazio = a fonte do app
    Rgb gradAt(double t) const { const int i = int(clamp(t, 0, 1) * 255 + 0.5) * 3; return { (*grad)[size_t(i)], (*grad)[size_t(i + 1)], (*grad)[size_t(i + 2)] }; }
    QColor paperC() const { return QColor(paper[0], paper[1], paper[2]); }
    QColor inkC(double a = 1) const { return QColor(ink[0], ink[1], ink[2], int(a * 255 + 0.5)); }
    QColor selC(double a = 1) const { return QColor(sel[0], sel[1], sel[2], int(a * 255 + 0.5)); }
    QColor hzC(double a = 1) const { return QColor(hz[0], hz[1], hz[2], int(a * 255 + 0.5)); }
    QColor haloC(double a = 1) const { return QColor(halo[0], halo[1], halo[2], int(a * 255 + 0.5)); }
};
const Skin* skinOf(const QString& id);

// heat: na térmica, o calor do mundo de 0 a 1 (negativo = não vale)
void stylize(uint8_t* d, int w, int h, const Skin& sk, bool outline = false, double floor = 0, double heat = -1);

struct Field { int N = 0; std::vector<float> D; };
Field densityField(const QImage& gal, int N = 512);
double sampleField(const Field& F, double u, double v);
QImage washImage(const Field& F, const Skin& sk, uint32_t seed, const QImage& gal = QImage());
// Ruído suave, pra mancha não ter borda de computador.
class SoftNoise {
public:
    explicit SoftNoise(uint32_t seed);
    double at(double x, double y) const;
    double operator()(double x, double y) const { return at(x, y) * 0.55 + at(x * 2.1 + 7, y * 2.1 + 3) * 0.3 + at(x * 4.3 + 13, y * 4.3 + 9) * 0.15; }
private:
    std::vector<float> t;
};
struct CurveSegs { Curve L; std::vector<float> seg; };   // x0 y0 x1 y1, no espaço do mundo
std::vector<CurveSegs> contours(const Field& F, const std::vector<Curve>& levels);
struct Stipple { std::vector<float> xs, ys; int n = 0; };
Stipple stipple(const Field& F, const Skin& sk, uint32_t seed);
QImage paperTile(const Skin& sk);

} // namespace galaxy
