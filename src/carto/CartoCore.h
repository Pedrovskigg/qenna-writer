#pragma once

// Núcleo do gerador de mapas (porte do Map Lab, concepts/map-lab/core.js).
// Só matemática: nada de widget. Os números seguem o JavaScript do Lab à risca
// (campos guardados em float, conta feita em double, os mesmos sorteios), pra
// que o mesmo mapa saia igual nos dois e o teste possa comparar pixel a pixel.

#include <QString>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

namespace carto {

// ── utilidades com a mesma semântica do JavaScript ──
inline double clampd(double v, double a, double b) { return v < a ? a : v > b ? b : v; }
inline double smoothstep(double a, double b, double x)
{
    const double t = clampd((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
// Math.hypot do JS (V8: divide pelo maior e soma com Kahan; o std::hypot difere no último dígito)
inline double jsHypot(double x, double y)
{
    x = std::fabs(x); y = std::fabs(y);
    if (std::isinf(x) || std::isinf(y)) return INFINITY;
    if (std::isnan(x) || std::isnan(y)) return NAN;
    const double mx = x > y ? x : y;
    if (mx == 0) return 0;
    double sum = 0, comp = 0;
    for (double v : { x, y }) { const double n = v / mx, summand = n * n - comp, pre = sum + summand; comp = (pre - sum) - summand; sum = pre; }
    return std::sqrt(sum) * mx;
}
inline double jsHypot3(double x, double y, double z)
{
    x = std::fabs(x); y = std::fabs(y); z = std::fabs(z);
    const double mx = std::max(x, std::max(y, z));
    if (mx == 0) return 0;
    double sum = 0, comp = 0;
    for (double v : { x, y, z }) { const double n = v / mx, summand = n * n - comp, pre = sum + summand; comp = (pre - sum) - summand; sum = pre; }
    return std::sqrt(sum) * mx;
}
// Math.round do JS (meio arredonda pra +infinito)
inline double jsRound(double x) { return std::floor(x + 0.5); }
// ToInt32 / ToUint32 do JS (x | 0, x >>> 0)
int32_t toInt32(double v);
uint32_t toUint32(double v);
// Uint8ClampedArray: corta em 0–255 e arredonda o meio pro par
inline uint8_t u8c(double v)
{
    if (!(v > 0)) return 0;
    if (v >= 255) return 255;
    const double f = std::floor(v), d = v - f;
    if (d < 0.5) return static_cast<uint8_t>(f);
    if (d > 0.5) return static_cast<uint8_t>(f + 1);
    return (static_cast<int>(f) % 2 == 0) ? static_cast<uint8_t>(f) : static_cast<uint8_t>(f + 1);
}
// Int8Array (normal guardada)
inline int8_t toI8(double v) { return static_cast<int8_t>(static_cast<uint8_t>(toInt32(v) & 0xFF)); }

// aleatório determinístico (o mesmo LCG do Mira)
class SeededRand {
public:
    explicit SeededRand(double seed) : m_s(toUint32(seed)) {}
    double next() { m_s = m_s * 1664525u + 1013904223u; return m_s / 4294967296.0; }
private:
    uint32_t m_s;
};

double hash2(double x, double y, double s);
double vnoise(double x, double y, double s);
double fbm(double x, double y, int oct, double s);

// ── presets ──
struct SizePreset { const char* id; int gw; int gh; };
extern const SizePreset kSizes[6];
const SizePreset& sizePreset(int size);

struct Planet { double lat0 = 0.0; double span = 78.0; bool set = false; };

// O mundo: altura (terra > 0, mar <= 0), umidade (0–1) e frio extra, na resolução final.
struct World {
    int W = 0, H = 0;
    std::vector<float> h, m, k;
    int seed = 0;
    int water = 60;
    QString type = QStringLiteral("continent");
    int size = 1;
    QString climate = QStringLiteral("mixed");
    double ref = 1.0;           // altura de referência das cores (fica fixa durante pinceladas)
    Planet planet;
    double scale = 1.0;         // escala do mundo (1, 0,7 ou 0,5)
    // caches preguiçosos (relevo escondido dos pincéis, ondulação da temperatura)
    std::vector<float> F, T;
};

World generate(int seed, int water, const QString& type, int size, const QString& climate);
World emptyWorld(int seed, int size, const QString& climate);
void cleanup(World& w);
void updateRef(World& w);
const std::vector<float>& ensureTempWave(World& w);
const std::vector<float>& ensureDetail(World& w);
inline double worldScale(const World& w) { return w.scale > 0 ? w.scale : 1.0; }

// clima
int biomeId(double t, double latC, double mois, double cold);   // 0 gélido · 1 floresta · 2 planície · 3 deserto · 4 pântano
double latitudeAt(const World& w, double qy);
double latCold(const World& w, double qy, double wave);
struct ClimateSample { bool valid = false; double h = 0, t = 0, m = 0; int id = 0; double temp = 0; };
ClimateSample climateAt(World& w, double x, double y);

// campos
std::vector<float> faultMap(int W, int H, int numFaults, double seed);
double computeThreshold(const std::vector<float>& map, double pct);
struct Upsampled { std::vector<float> map; int W = 0, H = 0; };
Upsampled upsample(const std::vector<float>& map, int W, int H, int s);
Upsampled islandRelief(int gw, int gh, int faults, double seed, double pct);   // relevo de uma ilha (mesmo pipeline do Mira)
std::vector<float> distanceToLand(const std::vector<float>& h, int W, int H);
void boxBlurWeighted(const std::vector<std::vector<float>*>& ch, std::vector<float>& wt, int W, int H, int r);

// azulejos de ruído (256×256, emendam nas bordas): A = manchas, B = detalhe fino
struct NoiseTiles { int S = 256; std::vector<float> A, B; };
const NoiseTiles& noiseTiles();
double tileAt(const std::vector<float>& T, double x, double y);

// paleta (a do terreno isométrico do Mira + o visual novo, Atlas e Tinta)
using RGB = std::array<double, 3>;
extern const RGB kBiomeColors[5];
extern const RGB kRock, kRockHi, kSnow, kSand;
extern const RGB kRich[5][3];
extern const RGB kAtlas[5];
extern const RGB kAtlasRock, kAtlasRockHi, kAtlasSnow;
extern const RGB kPaper, kInk;
constexpr double kBioStart = 0.30, kBioEnd = 0.58, kRockMid = 0.62, kSnowAt = 0.84, kLiftFloor = 0.30;
constexpr double kAtlasLift = 0.35, kInkLift = 0.8;
RGB richTone(int id, double u);

// nomes de fantasia (raiz inventada, presa a um sorteio) e o título sugerido do mapa
QString nameRoot(const std::function<double(int)>& r);
QString mapTitle(const World& w);   // "Terras de X", preso à semente

} // namespace carto
