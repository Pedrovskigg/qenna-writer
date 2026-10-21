#pragma once

// Pintor isométrico do mapa (porte do paintRegion do Map Lab): pinta qualquer
// retângulo da vista, nas quatro roupas (Mira, Novo, Atlas, Tinta), guardando
// por pixel a linha de origem, quanto o chão subiu e a normal (pra sprite saber
// onde pisa e pra hora do dia reacender o relevo sem repintar).

#include "CartoCore.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace carto {

enum class Style { Mira, Rich, Atlas, Ink };

struct View {
    double ang = 0.0;       // rotação da mesa (rad), em volta do centro do mundo
    double pitch = 40.5;    // inclinação (graus): 90 = de cima
    int step = 1;           // px do mundo por px da vista (1 = cheio; 2–3 = prévia)
    Style style = Style::Rich;
    bool info = false;      // pinta territórios
};

struct Camera { double ang, sq, liftK; int step; };
Camera cameraOf(const View& v);
int padFor(const World& w);

// O pixel (u, v) da vista fica no plano em ((u − cu)·step, (v − cv)·step), centro do mundo na origem.
struct Geom {
    int W = 0, H = 0;
    double cu = 0, cv = 0;
    int step = 1;
    double sq = 1, ang = 0;
    int pad = 0;
    double ca = 1, sa = 0, ccx = 0, ccy = 0, U = 1;
    Style style = Style::Rich;
    bool info = false;
    double extF = 0;
    int reach = 0, nearReach = 0;
    const std::atomic<bool>* cancel = nullptr;   // pintura de fundo que ficou velha: para no meio (só durante o render)
};
Geom viewGeom(const World& w, const View& v);
std::array<int, 4> worldRectToView(const Geom& g, double x0, double y0, double x1, double y1);
std::array<int, 4> dirtyToView(const Geom& g, const std::array<double, 4>& box, bool live);
double liftAt(const World& w, const View& v, double qx, double qy);

// por pixel da vista: linha de origem (−1 = água), quanto o chão subiu, normal (×127)
struct Aux {
    std::vector<int16_t> depth;
    std::vector<float> lift;
    std::vector<int8_t> nrm;
    void resize(size_t n) { depth.assign(n, -1); lift.assign(n, 0.0f); nrm.assign(n * 2, 0); }
};

// territórios na pintura (tinta leve no território, linha na fronteira)
struct RealmSample { int id = -1; double edge = 0; int eid = -1; };
class RealmPaint {
public:
    virtual ~RealmPaint() = default;
    virtual RealmSample realmAt(double qx, double qy) const = 0;
    virtual RGB realmColor(int id) const = 0;
};

// destino opcional: `out`/`aux` são um azulejo de largura w com origem em (x0, y0)
struct Dst { int w; int x0; int y0; };

// distância até a costa em meia resolução (a água rasa usa): calculada uma vez pro
// retângulo inteiro e dividida entre as faixas (cada faixa refazia uma janela enorme)
struct DistField { int sx0 = 0, sy0 = 0, cw = 0, ch = 0; std::vector<float> d; };
DistField makeDistField(const World& w, const Geom& g, int tx0, int ty0, int tx1, int ty1, int threads = 1);

// Pinta [tx0,tx1)×[ty0,ty1) da vista em `out` (RGBA não pré-multiplicado).
// inkEdges: a Tinta risca as cristas no fim (quem pinta em faixas paralelas deixa pra depois)
void paintRegion(World& w, const Geom& g, uint8_t* out, int tx0, int ty0, int tx1, int ty1,
                 Aux* aux, const Dst* dst = nullptr, const RealmPaint* realms = nullptr, bool inkEdges = true, const DistField* df = nullptr);
void inkEdgePass(const Geom& g, uint8_t* out, const Aux& aux, int tx0, int ty0, int tx1, int ty1, const Dst* dst = nullptr);
// o mesmo retângulo em faixas paralelas (threads ≤ 0 = um por núcleo); resultado idêntico
void paintBands(World& w, const Geom& g, uint8_t* out, int tx0, int ty0, int tx1, int ty1, Aux* aux,
                const RealmPaint* realms = nullptr, int threads = 0);

struct Render { Geom g; std::vector<uint8_t> data; Aux aux; };
Render render(World& w, const View& v, const RealmPaint* realms = nullptr, bool withAux = true, int threads = 0,
              const std::atomic<bool>* cancel = nullptr);

// ── hora do dia ──
constexpr double kHourDefault = 14.4;
struct Sun { std::array<double, 3> L; double el; double sunW; };
Sun sunAt(double hour);
struct SpriteSun { std::array<double, 3> L; bool mirror; };
SpriteSun spriteSun(double hour);
// fator de luz por normal guardada (256×256); vazio = a luz de sempre
std::vector<float> sunLUT(double hour, Style style);
void relight(const uint8_t* src, const int8_t* nrm, const int16_t* depth, uint8_t* dst, int W,
             int x0, int y0, int x1, int y1, const std::vector<float>& lut);
struct TimeOfDay { std::array<double, 3> mul; double night; std::array<double, 4> warm; Sun sun; };
TimeOfDay timeOfDay(double hour);
int hourPhase(double hour);   // 0 noite · 1 amanhecer · 2 manhã · 3 meio-dia · 4 tarde · 5 entardecer · 6 anoitecer

// ── camada de clima ──
struct ClimateKind { const char* id; RGB c; };
extern const ClimateKind kClimate[4];
void paintClimate(World& w, const Geom& g, uint8_t* out, int tx0, int ty0, int tx1, int ty1);

// ── nuvens aquarela (RGBA do tamanho do mundo + margem pad) e grão do oceano ──
// ds: 1 amostra a cada ds px do mundo (nuvem é mancha macia; nos mapas grandes basta 1 por unidade do desenho)
struct Clouds { int W = 0, H = 0, pad = 0, ds = 1; std::vector<uint8_t> rgba; };
Clouds clouds(const World& w, int seed, int ds = 1);
extern const RGB kOcean;
std::vector<uint8_t> oceanTile(int size, int seed);

} // namespace carto
