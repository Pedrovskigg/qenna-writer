#pragma once

// Núcleo do Galaxy Maps (porte do Lab, concepts/galaxy-maps/core.js). Só matemática,
// nada de widget: gera a galáxia, as estrelas, os planetas e as luas, calcula a física
// de cada corpo e pinta as texturas. Tudo determinístico pela semente, seguindo o
// JavaScript do Lab sorteio por sorteio, pro teste poder comparar os dois.

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace galaxy {

constexpr double TAU = 6.283185307179586;
constexpr double PI_D = 3.141592653589793;
constexpr double EXT = 1.15;   // a galáxia cabe no quadrado [-EXT, EXT]; raio 1 = borda do disco

using V3 = std::array<double, 3>;

// ── acaso (mulberry32, igual ao rngOf do Lab) ──
class Rng {
public:
    explicit Rng(uint32_t seed) : a(seed) {}
    double operator()()
    {
        a = a + 0x6D2B79F5u;
        uint32_t t = (a ^ (a >> 15)) * (1u | a);
        t = (t + (t ^ (t >> 7)) * (61u | t)) ^ t;
        return double(t ^ (t >> 14)) / 4294967296.0;
    }
private:
    uint32_t a;
};
inline double uni(Rng& r, double a, double b) { return a + (b - a) * r(); }
inline int irand(Rng& r, int a, int b) { return int(std::floor(a + (b - a + 1) * r())); }
inline double logU(Rng& r, double a, double b) { return std::exp(uni(r, std::log(a), std::log(b))); }
inline double clamp(double x, double a, double b) { return x < a ? a : x > b ? b : x; }
inline double smooth(double a, double b, double x) { const double t = clamp((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }
inline double mix(double a, double b, double t) { return a + (b - a) * t; }
double gauss(Rng& r);

// ── ruído de valor 2D/3D ──
class Noise {
public:
    explicit Noise(uint32_t seed);
    double n2(double x, double y) const;
    double n3(double x, double y, double z) const;
    double fbm2(double x, double y, int o) const;
    double fbm3(double x, double y, double z, int o) const;
    double ridge3(double x, double y, double z, int o) const;
private:
    uint16_t perm[768];
    float val[256];
};

// ── galáxia ──
enum class Style { Espiral, Barrada, Eliptica, Irregular, Anel };
enum class Size { Pequena, Media, Grande, Imensa };
const char* styleKey(Style s);
const char* styleLabel(Style s);
const char* sizeKey(Size s);
int sizeSystems(Size s);
int sizeLy(Size s);
const char* sizeStars(Size s);

// Tamanho de planeta: a mesma ordem das classes físicas.
enum class Cls { Rocha, Anao, Sub, Gelo, Gas };
enum class BodyKind { Planeta, Lua, Cometa, Estrela };
enum class Type {
    Lava, Arido, Deserto, Temperado, Oceano, Gelado, Toxico, Anao, Gigante, Quente, GeloG, Subnetuno, Cometa,
    Fortaleza, Ecumeno, Casco, Forja, Rochosa, Gelada, Vulcanica, Temperada, Estrela
};
const char* typeKey(Type t);
const char* typeLabel(Type t);
bool typeFromKey(const std::string& k, Type& out);
inline bool isArtificial(Type t) { return t == Type::Fortaleza || t == Type::Ecumeno || t == Type::Casco || t == Type::Forja; }
inline bool isLiving(Type t) { return t == Type::Temperado || t == Type::Oceano || t == Type::Temperada; }

enum class LookStyle { None, Continental, Arquipelago, Floresta, Savana, Tundra, Pantano, Aberto, Olho,
                       Craterado, Mares, Ferrugem, Fraturado, Carbono, Sal, Estratos, Tolinas, Geada };
const char* lookStyleKey(LookStyle s);
const char* lookStyleLabel(LookStyle s);
bool lookStyleFromKey(const std::string& k, LookStyle& out);

enum class Veg { None, Verde, Selva, Oliva, Carmim, Roxa, Azulada, Outono, Escura };
const char* vegKey(Veg v);
const char* vegLabel(Veg v);
V3 vegColor(Veg v);

struct PalStop { double t; V3 c; };
struct Storm { double lat, lon, w, h; V3 c; };

struct Look {
    uint32_t seed = 0;
    double pop = 0;
    LookStyle style = LookStyle::None;
    // mundos construídos
    bool artificial = false;
    int tone = 0;
    bool hasDish = false;
    V3 dish {};
    // gigantes, desertos, estufas
    std::vector<PalStop> pal;
    double bandsF = 0, turb = 0;
    std::vector<Storm> storms;
    double cap = 0;
    bool canyon = false;
    // mundos vivos
    bool hasFreq = false;
    double freq = 0, sea = 0, dryBias = 0, dryBase = 0, belt = 0, snowAt = 0, cloud = 0;
    bool hasDryBase = false;
    bool swirl = false;
    V3 shallow {}, deep {}, veg {}, dry {}, rock {}, cloudCol {}, ice {}, murk {};
    Veg vegKeyV = Veg::None;
    bool hasSub = false;
    double sub = 0;
    // rochosos
    bool hasRock = false;
    V3 tint {}, line {}, lo {}, hi {}, ac {};
    int craters = 0, nCr = 0, cseed = 0, bandsI = 0;
    double k1 = 0, thr = 0, cells = 0, fresh = 0, frost = 0, plainR = 0, cellF = 0;
    std::vector<std::array<double, 4>> volc;
    std::vector<V3> bandCols;
    V3 plain {};
    // lava
    double crack = 0, pool = 0;
};

struct Frame { V3 ax, e1, e2; };
Frame frameOf(double tiltDeg, double az);

struct Rings {
    double inner = 0, outer = 0, alpha = 0;
    V3 tint {};
    std::array<float, 48> bands {};
};

enum class StarKind { O, B, A, F, G, K, M, Gigante, AnaBranca, Neutrons, BuracoNegro, Nucleo, Binaria };
const char* starKindKey(StarKind k);
bool starKindFromKey(const std::string& k, StarKind& out);
const char* starKindLabel(StarKind k, bool pulsar = false);

struct Star {
    StarKind kind = StarKind::G;
    std::string label, spec;
    double mass = 0, radius = 0, lum = 0, temp = 0, age = 0, radiusKm = 0, period = 0;
    std::array<int, 3> color { 255, 255, 255 };
    bool nebula = false, pulsar = false, remnant = false, disk = false;
};

enum class Hz { None, Quente, Fria, Dentro };

struct Body {
    BodyKind body = BodyKind::Planeta;
    Type type = Type::Arido;
    Cls cls = Cls::Rocha;
    int idx = 0, uid = 0;
    double a = 0, e = 0, w = 0, M0 = 0, P = 0, mass = 0, radius = 0, gravity = 0, density = 0, albedo = 0;
    double teq = 0, temp = 0, dayH = 0, tilt = 0, az = 0;
    std::string atm;
    bool water = false, locked = false, retro = false, wantHabMoon = false, hab = false;
    Hz hz = Hz::None;
    Look look;
    Frame frame {};
    std::unique_ptr<Rings> rings;
    std::vector<std::unique_ptr<Body>> moons;
    // lua
    double dist = 0, radiusKm = 0, massKg = 0;
    // estrela S do núcleo
    std::unique_ptr<Star> star;
    // o que o autor pôs
    double people = 0;
    int air = 0;      // 0: vale o que a atmosfera diz; 1: dá pra respirar; -1: não dá
    int habSet = 0;   // a classe escolhida à mão (0 = a da conta)
    // no esquema do mapa (preenchido pelo layout)
    double layOrb = 0, layR = 0, layW = 0, layA0 = 0, layX = 0, layY = 0, layExt = 0, layMoonW = 0;
    bool layHasW = false, layHasA0 = false;
    double spinU = 0, pitchU = 0.3;
    bool spunByUser = false;
    uint32_t texRev = 0;   // sobe a cada edição, pros caches saberem
};

struct Belt { bool kuiper = false; double a0 = 0, a1 = 0; int n = 0, uid = 0; };

struct System {
    int id = 0;
    double x = 0, y = 0, z = 0;
    uint32_t seed = 0;
    bool isCore = false, gone = false, hand = false;
    double young = 0, rCore = 0;
    StarKind kind = StarKind::G;
    std::vector<Star> stars;
    double sep = 0, binP = 0, M = 0, L = 0, age = 0, life = 0;
    std::string label;
    bool hasHz = false, hasFrost = false;
    double hz[2] { 0, 0 };
    double frost = 0, frostEff = 0, amax = 0;
    std::vector<std::unique_ptr<Body>> bodies;
    std::vector<Belt> belts;
    int planetCount = 0;
    bool edited = false;   // o autor mexeu: vai inteiro pro arquivo
    // no esquema do mapa
    double layR = 0, layH = 0, layStar = 0, layGap = 0;
};

struct Blob { double x, y, r, k; bool young; };
struct Tune {
    bool has = false;
    double ly = 0;
    std::string starsText;
    bool hasAngle = false, hasK = false, hasWarp = false, hasQ = false, hasOx = false, hasOy = false, hasRingR = false;
    double angle = 0, k = 0, warp = 0, q = 0, ox = 0, oy = 0, ringR = 0, ringW = 0, bulgeK = 0, ringAdd = 0, coreK = 0, coreR = 0, dustK = 0, ring2R = 0;
    int spokes = 0;
    bool hasBarI = false, hasJet = false;
    double barA = 0, barK = 0, jetA = 0, jetLen = 0;
    std::vector<Blob> blobs;
};

struct GalaxyOpts {
    uint32_t seed = 4207;
    Style style = Style::Espiral;
    Size size = Size::Media;
    int arms = 4;
    double life = 0.3;
    int count = 0;    // 0 = o do tamanho
    Tune tune;
    std::vector<double> mix;   // quanto de cada classe, de 1 a 6 (índice 0 fica de fora); vazio = o que o sorteio der
};

struct Grid { std::vector<float> l, y, d, w; };
struct Cdf { std::vector<double> c; double total = 0; };
struct DetailStars { std::vector<float> xs, ys; std::vector<uint8_t> cs, bs; int n = 0; };

struct Galaxy {
    uint32_t seed = 0;
    Style style = Style::Espiral;
    Size size = Size::Media;
    int arms = 4, N = 300, count = 0;
    double ly = 0;
    std::string starsText;
    double angle = 0, k = 0, warp = 0, q = 0, ox = 0, oy = 0, sx = 0, sy = 0, ringR = 0, life = 0;
    double ringW = 0, bulgeK = 0, ringAdd = 0, coreK = 0, coreR = 0, dustK = 0, ring2R = 0;
    int spokes = 0;
    bool hasBarI = false, hasJet = false;
    double barA = 0, barK = 0, jetA = 0, jetLen = 0;
    std::vector<Blob> blobs;
    std::unique_ptr<Noise> nz;
    Grid grid;
    Cdf cdfStars, cdfYoung, cdfSys;
    std::vector<std::unique_ptr<System>> systems;
    DetailStars detail;
    std::vector<double> mix;
};

std::unique_ptr<Galaxy> makeGalaxy(const GalaxyOpts& opts);
int addSystems(Galaxy& G, int n);
// RGBA T×T
std::vector<uint8_t> buildGalaxyImage(const Galaxy& G, int T);
extern const char* const DETAIL_COLORS[5];

// ── física e edição ──
std::array<double, 3> tempRGB(double T);
std::array<int, 3> starColor(double T);
Star starOf(StarKind kind, double mass, uint32_t seed);
void massRange(StarKind kind, double& lo, double& hi);
void refreshSystem(System& sys);
void setType(System& sys, Body& b, Type type);
void regenLook(System& sys, Body& b, uint32_t seed);
void tuneTemperature(Body& b, double tempK);
void rockRestyle(System& sys, Body& b, LookStyle style);
void restyle(Body& b, LookStyle style);
double waterOf(const Body& b);
void setWater(Body& b, double frac);
void setRings(Body& b, bool on, uint32_t seed);
Body* addPlanet(System& sys, Type type, double a, uint32_t seed);
Body* addMoon(System& sys, Body& p, Type type, uint32_t seed);
std::unique_ptr<Body> removeBody(System& sys, Body* b);
void restoreBody(System& sys, std::unique_ptr<Body> b, Body* parent);
void reorder(System& sys);
double lightLevel(double people, double radiusEarth);
void orbitPos(const Body& b, double t, double& r, double& ang);
const std::vector<LookStyle>& rockStylesOf(Type t);
const char* rockDesc(LookStyle s);
extern const std::vector<LookStyle> LIVING_STYLES;
extern const std::vector<StarKind> STAR_KINDS;

// ── classes de planeta ──
// A régua de quanto custa viver num mundo, de 1 (Éden) a 6 (Sem chão). Mundo construído fica fora: classe 0.
enum class AirKind { Breathable, Poison, Thin, None };
AirKind airOf(const Body& b);
struct HabWhy {
    enum Code { Built, NoGroundGas, NoGroundHot, NoGroundIce, NoGroundThick, NoGroundLava, NoGroundVolcano, Breathable, TooHot, TooCold, Heavy, Light, Poison, ThinAir, NoAir, Locked, Radiation } code;
    double v;   // a temperatura em °C ou a gravidade, quando o motivo fala de uma
};
struct HabInfo { int cls = 0, autoCls = 0; std::vector<HabWhy> why; };
HabInfo habInfo(const Body& b, const System* sys, const Body* host);
// O autor diz quanto quer de cada classe: os mundos que mais combinam são adaptados, sem mexer em estrelas nem órbitas.
void applyMix(const std::vector<System*>& systems, const std::vector<double>& mix, uint32_t seed);

// ── texturas ──
struct Atm { V3 c; double k; };
const Atm* atmOf(const Body& b);

struct Texture {
    int w = 0, h = 0, cw = 0, ch = 0;
    std::vector<uint8_t> rgb, emi, city, spec;
};
Texture bodyTexture(const Body& b, int w, int h);
Texture starTexture(const Star& star, uint32_t seed, int w, int h);

// ── câmera e esfera ──
struct SphereTable {
    int D = 0;
    std::vector<float> lat, lon, alpha, nx, ny, nz;
};
SphereTable sphereTable(int D, double yaw, double pitch, const Frame& fr);
// Lc = direção da luz na câmera; nullptr = o corpo brilha sozinho. out = RGBA D×D
void shadeSphere(uint8_t* out, const SphereTable& T, const Texture& tex, double spin, const V3* Lc, const Atm* atm);
// As mesmas contas por faixa de linhas [j0, j1), pra dividir entre os núcleos. A tabela nasce de sphereTableAlloc.
SphereTable sphereTableAlloc(int D);
void sphereTableRows(SphereTable& T, double yaw, double pitch, const Frame& fr, int j0, int j1);
void shadeSphereRows(uint8_t* out, const SphereTable& T, const Texture& tex, double spin, const V3* Lc, const Atm* atm, int j0, int j1);

} // namespace galaxy
