#include "GalaxyCore.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace galaxy {

namespace {
constexpr double AU_KM = 1.496e8, RE_KM = 6371, ME_KG = 5.972e24, MSUN_ME = 332946, G_SI = 6.674e-11;

template <typename T>
T pickW(Rng& r, std::initializer_list<std::pair<T, double>> list)
{
    double s = 0;
    for (const auto& it : list) s += it.second;
    double u = r() * s;
    for (const auto& it : list) { u -= it.second; if (u <= 0) return it.first; }
    return (list.end() - 1)->first;
}
V3 jitter(const V3& c, Rng& r, double amt)
{
    V3 o;
    for (int i = 0; i < 3; i++) o[i] = clamp(c[i] * (1 + (r() - 0.5) * amt), 0, 1);
    return o;
}
template <typename T, size_t N>
const T& pick(Rng& r, const T (&list)[N]) { return list[irand(r, 0, int(N) - 1)]; }
inline double jsRound(double x) { return std::floor(x + 0.5); }
// Math.hypot do JS (V8): o std::hypot difere no último dígito
inline double jsHypot(double x, double y)
{
    x = std::fabs(x); y = std::fabs(y);
    const double mx = x > y ? x : y;
    if (mx == 0) return 0;
    double sum = 0, comp = 0;
    for (double v : { x, y }) { const double n = v / mx, summand = n * n - comp, pre = sum + summand; comp = (pre - sum) - summand; sum = pre; }
    return std::sqrt(sum) * mx;
}
} // namespace

double gauss(Rng& r)
{
    double u = 0;
    while (u == 0) u = r();
    const double s = std::sqrt(-2 * std::log(u));
    return s * std::cos(TAU * r());
}

// ── ruído ──
Noise::Noise(uint32_t seed)
{
    Rng r(uint32_t(uint64_t(seed) * 7919u + 13u));
    uint8_t p[256];
    for (int i = 0; i < 256; i++) p[i] = uint8_t(i);
    for (int i = 255; i > 0; i--) { const int j = int(std::floor(r() * (i + 1))); std::swap(p[i], p[j]); }
    for (int i = 0; i < 768; i++) perm[i] = p[i & 255];
    for (int i = 0; i < 256; i++) val[i] = float(r() * 2 - 1);
}
static inline double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
double Noise::n2(double x, double y) const
{
    const double xi = std::floor(x), yi = std::floor(y);
    const double u = fade(x - xi), v = fade(y - yi);
    const int X = int(int64_t(xi)) & 255, Y = int(int64_t(yi)) & 255;
    const double a = val[perm[X + perm[Y]]], b = val[perm[X + 1 + perm[Y]]];
    const double c = val[perm[X + perm[Y + 1]]], d = val[perm[X + 1 + perm[Y + 1]]];
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
double Noise::n3(double x, double y, double z) const
{
    const double xi = std::floor(x), yi = std::floor(y), zi = std::floor(z);
    const double u = fade(x - xi), v = fade(y - yi), w = fade(z - zi);
    const int X = int(int64_t(xi)) & 255, Y = int(int64_t(yi)) & 255, Z = int(int64_t(zi)) & 255;
    const int z0 = perm[Z], z1 = perm[Z + 1];
    const int y00 = perm[Y + z0], y10 = perm[Y + 1 + z0], y01 = perm[Y + z1], y11 = perm[Y + 1 + z1];
    const double c00 = mix(val[perm[X + y00]], val[perm[X + 1 + y00]], u);
    const double c10 = mix(val[perm[X + y10]], val[perm[X + 1 + y10]], u);
    const double c01 = mix(val[perm[X + y01]], val[perm[X + 1 + y01]], u);
    const double c11 = mix(val[perm[X + y11]], val[perm[X + 1 + y11]], u);
    return mix(mix(c00, c10, v), mix(c01, c11, v), w);
}
double Noise::fbm2(double x, double y, int o) const
{
    double s = 0, a = 0.5, f = 1, n = 0;
    for (int i = 0; i < o; i++) { s += a * n2(x * f + i * 17.3, y * f - i * 9.1); n += a; a *= 0.5; f *= 2.03; }
    return s / n;
}
double Noise::fbm3(double x, double y, double z, int o) const
{
    double s = 0, a = 0.5, f = 1, n = 0;
    for (int i = 0; i < o; i++) { s += a * n3(x * f + i * 17.3, y * f - i * 9.1, z * f + i * 5.7); n += a; a *= 0.5; f *= 2.03; }
    return s / n;
}
double Noise::ridge3(double x, double y, double z, int o) const
{
    double s = 0, a = 0.5, f = 1, n = 0;
    for (int i = 0; i < o; i++) { const double v = 1 - std::fabs(n3(x * f + i * 17.3, y * f - i * 9.1, z * f + i * 5.7)); s += a * v * v; n += a; a *= 0.5; f *= 2.03; }
    return s / n;
}

// ── tabelas ──
const char* styleKey(Style s)
{
    switch (s) { case Style::Espiral: return "espiral"; case Style::Barrada: return "barrada"; case Style::Eliptica: return "eliptica"; case Style::Irregular: return "irregular"; default: return "anel"; }
}
const char* styleLabel(Style s)
{
    switch (s) { case Style::Espiral: return "Espiral"; case Style::Barrada: return "Espiral barrada"; case Style::Eliptica: return "Elíptica"; case Style::Irregular: return "Irregular"; default: return "Anel"; }
}
const char* sizeKey(Size s)
{
    switch (s) { case Size::Pequena: return "pequena"; case Size::Media: return "media"; case Size::Grande: return "grande"; default: return "imensa"; }
}
int sizeSystems(Size s) { switch (s) { case Size::Pequena: return 60; case Size::Media: return 120; case Size::Grande: return 200; default: return 300; } }
int sizeLy(Size s) { switch (s) { case Size::Pequena: return 30000; case Size::Media: return 70000; case Size::Grande: return 110000; default: return 170000; } }
const char* sizeStars(Size s)
{
    switch (s) { case Size::Pequena: return "cerca de 10 bilhões"; case Size::Media: return "cerca de 100 bilhões"; case Size::Grande: return "cerca de 300 bilhões"; default: return "cerca de 1 trilhão"; }
}
static const struct { Type t; const char* key; const char* label; } TYPE_TABLE[] = {
    { Type::Lava, "lava", "Mundo de lava" }, { Type::Arido, "arido", "Rochoso árido" }, { Type::Deserto, "deserto", "Desértico" }, { Type::Temperado, "temperado", "Temperado" },
    { Type::Oceano, "oceano", "Oceânico" }, { Type::Gelado, "gelado", "Gelado" }, { Type::Toxico, "toxico", "Estufa tóxica" }, { Type::Anao, "anao", "Planeta anão" },
    { Type::Gigante, "gigante", "Gigante gasoso" }, { Type::Quente, "quente", "Júpiter quente" }, { Type::GeloG, "gelo", "Gigante de gelo" }, { Type::Subnetuno, "subnetuno", "Subnetuno" },
    { Type::Cometa, "cometa", "Cometa" }, { Type::Fortaleza, "fortaleza", "Lua de guerra" }, { Type::Ecumeno, "ecumeno", "Planeta-cidade" }, { Type::Casco, "casco", "Planeta-casco" },
    { Type::Forja, "forja", "Mundo-forja" }, { Type::Rochosa, "rochosa", "Lua rochosa" }, { Type::Gelada, "gelada", "Lua gelada" }, { Type::Vulcanica, "vulcanica", "Lua vulcânica" },
    { Type::Temperada, "temperada", "Lua habitável" }, { Type::Estrela, "estrela", "Estrela" },
};
const char* typeKey(Type t) { for (const auto& e : TYPE_TABLE) if (e.t == t) return e.key; return ""; }
const char* typeLabel(Type t) { for (const auto& e : TYPE_TABLE) if (e.t == t) return e.label; return ""; }
bool typeFromKey(const std::string& k, Type& out) { for (const auto& e : TYPE_TABLE) if (k == e.key) { out = e.t; return true; } return false; }

static const struct { LookStyle s; const char* key; const char* label; } LOOK_TABLE[] = {
    { LookStyle::None, "", "" }, { LookStyle::Continental, "continental", "Continental" }, { LookStyle::Arquipelago, "arquipelago", "Arquipélago" }, { LookStyle::Floresta, "floresta", "Mundo-floresta" },
    { LookStyle::Savana, "savana", "Savana" }, { LookStyle::Tundra, "tundra", "Tundra" }, { LookStyle::Pantano, "pantano", "Pântano" }, { LookStyle::Aberto, "aberto", "Oceano aberto" }, { LookStyle::Olho, "olho", "Planeta-olho" },
    { LookStyle::Craterado, "craterado", "Crateras antigas" }, { LookStyle::Mares, "mares", "Mares de basalto" }, { LookStyle::Ferrugem, "ferrugem", "Ferrugem" }, { LookStyle::Fraturado, "fraturado", "Fraturado" },
    { LookStyle::Carbono, "carbono", "Escuro, de carbono" }, { LookStyle::Sal, "sal", "Planícies de sal" }, { LookStyle::Estratos, "estratos", "Camadas minerais" }, { LookStyle::Tolinas, "tolinas", "Tolinas e gelo" }, { LookStyle::Geada, "geada", "Geada" },
};
const char* lookStyleKey(LookStyle s) { for (const auto& e : LOOK_TABLE) if (e.s == s) return e.key; return ""; }
const char* lookStyleLabel(LookStyle s) { for (const auto& e : LOOK_TABLE) if (e.s == s) return e.label; return ""; }
bool lookStyleFromKey(const std::string& k, LookStyle& out) { for (const auto& e : LOOK_TABLE) if (k == e.key) { out = e.s; return true; } return false; }
const std::vector<LookStyle> LIVING_STYLES { LookStyle::Continental, LookStyle::Arquipelago, LookStyle::Floresta, LookStyle::Savana, LookStyle::Tundra, LookStyle::Pantano, LookStyle::Aberto, LookStyle::Olho };
const char* rockDesc(LookStyle s)
{
    switch (s) {
    case LookStyle::Craterado: return "Chão velho, sem ar nem erosão: bilhões de anos de impacto guardados";
    case LookStyle::Mares: return "Bacias de lava antiga, escuras e lisas, entre planaltos claros e marcados";
    case LookStyle::Ferrugem: return "Rocha oxidada, cânions e vulcões-escudo apagados";
    case LookStyle::Fraturado: return "Faixas claras de sulcos cortando o terreno escuro e mais velho";
    case LookStyle::Carbono: return "Chão quase preto; as crateras novas abrem rocha clara por baixo";
    case LookStyle::Sal: return "Planícies claras, rachadas em polígonos, com poeira por cima";
    case LookStyle::Estratos: return "Mesas e terraços de camadas coloridas, gastas pelo tempo";
    case LookStyle::Tolinas: return "Gelo de nitrogênio numa planície lisa, cercado de chão avermelhado";
    case LookStyle::Geada: return "Superfície coberta de gelo claro, quase sem cor";
    default: return "";
    }
}
const std::vector<LookStyle>& rockStylesOf(Type t)
{
    static const std::vector<LookStyle> arido { LookStyle::Craterado, LookStyle::Mares, LookStyle::Ferrugem, LookStyle::Fraturado, LookStyle::Carbono, LookStyle::Sal, LookStyle::Estratos };
    static const std::vector<LookStyle> rochosa { LookStyle::Craterado, LookStyle::Mares, LookStyle::Fraturado, LookStyle::Carbono, LookStyle::Ferrugem, LookStyle::Sal };
    static const std::vector<LookStyle> anao { LookStyle::Tolinas, LookStyle::Geada, LookStyle::Carbono, LookStyle::Craterado, LookStyle::Sal };
    static const std::vector<LookStyle> none;
    return t == Type::Arido ? arido : t == Type::Rochosa ? rochosa : t == Type::Anao ? anao : none;
}
const char* vegKey(Veg v)
{
    switch (v) { case Veg::Verde: return "verde"; case Veg::Selva: return "selva"; case Veg::Oliva: return "oliva"; case Veg::Carmim: return "carmim"; case Veg::Roxa: return "roxa"; case Veg::Azulada: return "azulada"; case Veg::Outono: return "outono"; case Veg::Escura: return "escura"; default: return ""; }
}
const char* vegLabel(Veg v)
{
    switch (v) { case Veg::Verde: return "Verde"; case Veg::Selva: return "Verde-escura e densa"; case Veg::Oliva: return "Amarelada"; case Veg::Carmim: return "Vermelha"; case Veg::Roxa: return "Roxa"; case Veg::Azulada: return "Azulada"; case Veg::Outono: return "Alaranjada"; case Veg::Escura: return "Quase preta"; default: return ""; }
}
V3 vegColor(Veg v)
{
    switch (v) {
    case Veg::Verde: return { 0.14, 0.3, 0.09 }; case Veg::Selva: return { 0.06, 0.2, 0.06 }; case Veg::Oliva: return { 0.34, 0.36, 0.12 }; case Veg::Carmim: return { 0.4, 0.09, 0.07 };
    case Veg::Roxa: return { 0.3, 0.11, 0.36 }; case Veg::Azulada: return { 0.07, 0.27, 0.3 }; case Veg::Outono: return { 0.52, 0.26, 0.07 }; case Veg::Escura: return { 0.11, 0.09, 0.07 };
    default: return { 0.14, 0.3, 0.09 };
    }
}
const char* starKindKey(StarKind k)
{
    switch (k) { case StarKind::O: return "O"; case StarKind::B: return "B"; case StarKind::A: return "A"; case StarKind::F: return "F"; case StarKind::G: return "G"; case StarKind::K: return "K"; case StarKind::M: return "M"; case StarKind::Gigante: return "gigante"; case StarKind::AnaBranca: return "anabranca"; case StarKind::Neutrons: return "neutrons"; case StarKind::BuracoNegro: return "buraconegro"; case StarKind::Nucleo: return "nucleo"; default: return "binaria"; }
}
bool starKindFromKey(const std::string& k, StarKind& out)
{
    for (StarKind s : { StarKind::O, StarKind::B, StarKind::A, StarKind::F, StarKind::G, StarKind::K, StarKind::M, StarKind::Gigante, StarKind::AnaBranca, StarKind::Neutrons, StarKind::BuracoNegro, StarKind::Nucleo, StarKind::Binaria })
        if (k == starKindKey(s)) { out = s; return true; }
    return false;
}
const char* starKindLabel(StarKind k, bool pulsar)
{
    switch (k) {
    case StarKind::O: return "Estrela azul"; case StarKind::B: return "Estrela azul-branca"; case StarKind::A: return "Estrela branca"; case StarKind::F: return "Estrela branco-amarelada";
    case StarKind::G: return "Anã amarela"; case StarKind::K: return "Anã laranja"; case StarKind::M: return "Anã vermelha"; case StarKind::Gigante: return "Gigante vermelha";
    case StarKind::AnaBranca: return "Anã branca"; case StarKind::Neutrons: return pulsar ? "Pulsar" : "Estrela de nêutrons"; case StarKind::BuracoNegro: return "Buraco negro";
    case StarKind::Binaria: return "Estrela dupla"; default: return "Buraco negro supermassivo";
    }
}
const std::vector<StarKind> STAR_KINDS { StarKind::O, StarKind::B, StarKind::A, StarKind::F, StarKind::G, StarKind::K, StarKind::M, StarKind::Gigante, StarKind::AnaBranca, StarKind::Neutrons, StarKind::BuracoNegro };
const char* const DETAIL_COLORS[5] = { "#a9c4ff", "#d7e3ff", "#fff4e6", "#ffd9a8", "#ffb98a" };

// Cor de corpo negro (aproximação de Tanner Helland), 0..255.
std::array<double, 3> tempRGB(double T)
{
    const double t = T / 100;
    double r, g, b;
    if (t <= 66) { r = 255; g = 99.4708025861 * std::log(t) - 161.1195681661; b = t <= 19 ? 0 : 138.5177312231 * std::log(t - 10) - 305.0447927307; }
    else { r = 329.698727446 * std::pow(t - 60, -0.1332047592); g = 288.1221695283 * std::pow(t - 60, -0.0755148492); b = 255; }
    return { clamp(r, 0, 255), clamp(g, 0, 255), clamp(b, 0, 255) };
}
// A cor que a estrela mostra no mapa: corpo negro puxado um pouco pro branco.
std::array<int, 3> starColor(double T)
{
    const auto c = tempRGB(T);
    return { int(jsRound(mix(c[0], 255, 0.22))), int(jsRound(mix(c[1], 255, 0.22))), int(jsRound(mix(c[2], 255, 0.22))) };
}

// ── o campo da galáxia ──
namespace { struct FieldOut { double l, y, d, w; }; }
static void galaxyField(const Galaxy& G, const Noise& nz, double x, double y, FieldOut& out)
{
    const double r = jsHypot(x, y), th = std::atan2(y, x);
    const double n1 = nz.fbm2(x * 2.4 + 11, y * 2.4 - 7, 4), n2 = nz.fbm2(x * 6 + 3, y * 6 + 9, 4);
    const double edge = 1 - smooth(0.74, 1.06, r + n1 * 0.1);
    double l = 0, yo = 0, d = 0, w = 0;
    if (G.style == Style::Espiral || G.style == Style::Barrada) {
        const bool barred = G.style == Style::Barrada;
        const double r0 = barred ? 0.24 : 0.07;
        const double rr = std::max(r, r0);
        const double phi = G.arms * (th - G.angle - G.k * std::log(rr / r0)) + n1 * G.warp * 2.2;
        const double c = 0.5 + 0.5 * std::cos(phi);
        const double arm = std::pow(c, 3.4);
        const double spur = std::pow(0.5 + 0.5 * std::cos(phi * 2 + n2 * 4), 7) * 0.35;
        const double mask = smooth(r0 * 0.6, r0 * 1.7, r) * edge;
        const double disk = std::exp(-r / 0.33);
        const double bulge = (std::exp(-(r * r) / (0.1 * 0.1)) + 0.4 * std::exp(-r / 0.11)) * (G.bulgeK != 0 ? G.bulgeK : 1);
        double bar = 0;
        if (barred) {
            const double ca = std::cos(G.angle), sa = std::sin(G.angle);
            const double bx = x * ca + y * sa, by = -x * sa + y * ca;
            bar = std::exp(-(bx * bx) / (0.26 * 0.26) - (by * by) / (0.06 * 0.06));
        }
        const double knot = 0.8 + 0.4 * (n2 * 0.5 + 0.5);
        l = bulge * 1.5 + bar * 0.9 + disk * edge * (0.16 + (arm * 1.6 + spur) * mask * knot);
        yo = disk * edge * mask * (arm + spur * 0.6) * knot;
        d = disk * mask * std::pow(0.5 + 0.5 * std::cos(phi + 0.8), 8) * (0.5 + 0.5 * (n2 * 0.5 + 0.5)) * 1.4;
        w = bulge * 1.5 + bar * 0.8;
        if (G.ringAdd != 0) { const double dr = r - G.ringR - n1 * 0.03, q = G.ringW != 0 ? G.ringW : 0.05, ring = std::exp(-(dr * dr) / (q * q)) * (0.55 + 0.6 * (n2 * 0.5 + 0.5)) * G.ringAdd; l += ring * 0.7; yo += ring; }
    } else if (G.style == Style::Eliptica) {
        const double ca = std::cos(G.angle), sa = std::sin(G.angle);
        const double ex = x * ca + y * sa, ey = (-x * sa + y * ca) / G.q;
        const double re = jsHypot(ex, ey) * (1 + n1 * 0.05);
        l = 1.9 * std::exp(-std::pow(re / 0.05, 0.5)) * (1 - smooth(0.7, 1.05, re));
        w = l; yo = 0; d = 0;
    } else if (G.style == Style::Irregular) {
        const double rr = jsHypot(x - G.ox, (y - G.oy) * 1.15);
        const double blob = 0.5 + 0.5 * nz.fbm2(x * 1.7 + G.sx, y * 1.7 + G.sy, 5) / 0.5;
        const double m = std::pow(clamp(blob * 1.3 - 0.35, 0, 1), 1.5);
        l = std::exp(-rr / 0.3) * m * 2.2 * edge;
        if (G.hasBarI) { const double ca = std::cos(G.barA), sa = std::sin(G.barA), bx = (x - G.ox) * ca + (y - G.oy) * sa, by = -(x - G.ox) * sa + (y - G.oy) * ca; l += std::exp(-(bx * bx) / (0.38 * 0.38) - (by * by) / (0.09 * 0.09)) * G.barK * (0.75 + 0.5 * (n2 * 0.5 + 0.5)); }
        yo = l * 0.85 * (0.5 + 0.5 * (n2 * 0.5 + 0.5) * 2);
        d = l * 0.5 * std::pow(std::max(0.0, n2 * 2), 2);
        w = 0.6 * std::exp(-rr / 0.12);
        l += w * 0.5;
    } else {
        const double cr = G.coreR != 0 ? G.coreR : 0.07, core = (std::exp(-(r * r) / (cr * cr)) * 1.6 + 0.35 * std::exp(-r / cr)) * (G.coreK != 0 ? G.coreK : 1) * (G.coreR != 0 ? 1 - smooth(0.55, 1, r) : 1);
        const double rw = (G.ringW != 0 ? G.ringW : 0.085) + 0.03 * n1;
        const double dr = r - G.ringR - n1 * 0.05;
        const double ring = std::exp(-(dr * dr) / (rw * rw)) * (0.55 + 0.7 * (n2 * 0.5 + 0.5));
        l = core + ring * 1.15; yo = ring; d = ring * 0.3 * std::pow(clamp(0.5 + n2, 0, 1), 3) * (G.dustK != 0 ? G.dustK : 1); w = core;
        if (G.ring2R != 0) { const double q = r - G.ring2R - n1 * 0.02, r2 = std::exp(-(q * q) / (0.05 * 0.05)); l += r2 * 0.9; yo += r2 * 0.4; }
        if (G.spokes != 0) { const double in = G.ring2R != 0 ? G.ring2R : 0.1, s = std::pow(0.5 + 0.5 * std::cos(th * G.spokes + n1 * 2.5), 22) * smooth(in, in + 0.06, r) * (1 - smooth(G.ringR - 0.08, G.ringR, r)); l += s * 0.22; yo += s * 0.1; }
    }
    // O que mora em cima da forma: companheiras, nebulosas e o jato.
    for (const Blob& b : G.blobs) {
        const double dx = x - b.x, dy = y - b.y, q = std::exp(-(dx * dx + dy * dy) / (b.r * b.r)) * b.k;
        l += q;
        if (b.young) yo += q * 1.3; else w += q;
    }
    if (G.hasJet) {
        const double ca = std::cos(G.jetA), sa = std::sin(G.jetA), u = x * ca + y * sa, v = -x * sa + y * ca;
        if (u > 0 && u < G.jetLen) { const double q = std::exp(-(v * v) / (0.008 * 0.008)) * (1 - u / G.jetLen) * 1.6; l += q; yo += q; }
    }
    out.l = l; out.y = yo; out.d = d; out.w = w;
}
static inline double bil(const std::vector<float>& a, int N, double gx, double gy)
{
    gx = clamp(gx, 0, N - 1.001); gy = clamp(gy, 0, N - 1.001);
    const int i = int(gx), j = int(gy);
    const double u = gx - i, v = gy - j;
    const int k = j * N + i;
    return (double(a[k]) * (1 - u) + double(a[k + 1]) * u) * (1 - v) + (double(a[k + N]) * (1 - u) + double(a[k + N + 1]) * u) * v;
}
static Cdf makeCdf(const std::vector<float>& w)
{
    Cdf cdf; cdf.c.resize(w.size());
    double s = 0;
    for (size_t i = 0; i < w.size(); i++) { s += w[i]; cdf.c[i] = s; }
    cdf.total = s;
    return cdf;
}
static int sampleCdf(const Cdf& cdf, double u)
{
    const double t = u * cdf.total;
    int lo = 0, hi = int(cdf.c.size()) - 1;
    while (lo < hi) { const int m = (lo + hi) >> 1; if (cdf.c[m] < t) lo = m + 1; else hi = m; }
    return lo;
}
static inline void cellPos(const Galaxy& G, int k, Rng& rr, double& x, double& y)
{
    const int N = G.N, i = k % N, j = k / N;
    const double cs = (2 * EXT) / N;
    x = -EXT + (i + rr()) * cs;
    y = EXT - (j + rr()) * cs;
}

// ── estrelas ──
namespace {
struct ClassInfo { double m[2]; double T[2]; const char* label; };
const ClassInfo* classOf(StarKind k)
{
    static const ClassInfo O { { 16, 60 }, { 30000, 45000 }, "Estrela azul" }, B { { 2.1, 16 }, { 10000, 30000 }, "Estrela azul-branca" }, A { { 1.4, 2.1 }, { 7500, 10000 }, "Estrela branca" },
        F { { 1.04, 1.4 }, { 6000, 7500 }, "Estrela branco-amarelada" }, Gc { { 0.8, 1.04 }, { 5200, 6000 }, "Anã amarela" }, Kc { { 0.45, 0.8 }, { 3700, 5200 }, "Anã laranja" }, Mc { { 0.08, 0.45 }, { 2400, 3700 }, "Anã vermelha" };
    switch (k) { case StarKind::O: return &O; case StarKind::B: return &B; case StarKind::A: return &A; case StarKind::F: return &F; case StarKind::G: return &Gc; case StarKind::K: return &Kc; case StarKind::M: return &Mc; default: return nullptr; }
}
double lumOf(double m) { return m < 0.43 ? 0.23 * std::pow(m, 2.3) : m < 2 ? std::pow(m, 4) : m < 55 ? 1.4 * std::pow(m, 3.5) : 32000 * m; }
std::string numStr(int n) { return std::to_string(n); }
Star mainSeq(Rng& r, StarKind cls)
{
    const ClassInfo* c = classOf(cls);
    const double t = r();
    const double m = c->m[0] * std::pow(c->m[1] / c->m[0], t);
    const double T = mix(c->T[0], c->T[1], t);
    const double L = lumOf(m), R = std::sqrt(L) * std::pow(5778 / T, 2);
    const double life = 10 * std::pow(m, -2.5);
    Star s;
    s.kind = cls; s.label = c->label;
    s.spec = std::string(starKindKey(cls)) + numStr(int(clamp(std::floor((1 - t) * 10), 0, 9))) + " V";
    s.mass = m; s.radius = R; s.lum = L; s.temp = T;
    s.age = std::min(life * uni(r, 0.05, 0.9), 12.5);
    s.color = starColor(T);
    return s;
}
Star makeStar(Rng& r, StarKind kind)
{
    if (classOf(kind)) return mainSeq(r, kind);
    Star s;
    s.kind = kind;
    if (kind == StarKind::Gigante) {
        const double m = uni(r, 0.9, 3), T = uni(r, 3300, 4800), R = logU(r, 10, 110);
        const bool isM = T < 3700;
        const double sub = isM ? std::floor((3700 - T) / 130) : std::floor((5200 - T) / 150);
        s.label = starKindLabel(StarKind::Gigante);
        s.spec = std::string(isM ? "M" : "K") + numStr(int(clamp(sub, 0, 9))) + " III";
        s.mass = m; s.radius = R; s.lum = R * R * std::pow(T / 5778, 4); s.temp = T; s.age = uni(r, 1, 11); s.color = starColor(T);
        return s;
    }
    if (kind == StarKind::AnaBranca) {
        const double m = uni(r, 0.5, 1.1), T = logU(r, 5000, 40000), R = 0.0126 * std::pow(m / 0.6, -1.0 / 3);
        s.label = starKindLabel(StarKind::AnaBranca);
        s.spec = "DA" + numStr(int(clamp(jsRound(50400 / T), 1, 9)));
        s.mass = m; s.radius = R; s.lum = R * R * std::pow(T / 5778, 4); s.temp = T; s.age = uni(r, 1, 12); s.color = starColor(T); s.nebula = r() < 0.45;
        return s;
    }
    if (kind == StarKind::Neutrons) {
        const bool pulsar = r() < 0.7;
        const double km = uni(r, 10, 13), T = logU(r, 3e5, 1e6), R = km / 696000;
        s.label = starKindLabel(StarKind::Neutrons, pulsar); s.spec = "—";
        s.mass = uni(r, 1.3, 2.1); s.radius = R; s.radiusKm = km; s.lum = R * R * std::pow(T / 5778, 4); s.temp = T;
        s.age = logU(r, 0.001, 1); s.color = { 200, 225, 255 }; s.pulsar = pulsar; s.period = logU(r, 0.0016, 2); s.remnant = r() < 0.6;
        return s;
    }
    if (kind == StarKind::BuracoNegro || kind == StarKind::Nucleo) {
        const double m = kind == StarKind::Nucleo ? logU(r, 1e6, 8e7) : logU(r, 5, 40);
        const double km = 2.95 * m;
        s.label = starKindLabel(kind); s.spec = "—";
        s.mass = m; s.radius = km / 696000; s.radiusKm = km; s.lum = 0; s.temp = 0; s.age = uni(r, 0.5, 12); s.color = { 255, 180, 110 };
        s.disk = kind == StarKind::Nucleo || r() < 0.65;
        return s;
    }
    return mainSeq(r, StarKind::G);
}
} // namespace

// ── aparência (sementes e paletas, decididas na geração) ──
namespace {
using Pal = std::vector<PalStop>;
const Pal GAS_PALS[] = {
    { { 0, { 0.55, 0.38, 0.25 } }, { 0.35, { 0.82, 0.66, 0.48 } }, { 0.62, { 0.94, 0.88, 0.77 } }, { 1, { 0.7, 0.5, 0.34 } } },
    { { 0, { 0.72, 0.62, 0.42 } }, { 0.4, { 0.88, 0.8, 0.6 } }, { 0.7, { 0.95, 0.9, 0.75 } }, { 1, { 0.78, 0.68, 0.48 } } },
    { { 0, { 0.42, 0.3, 0.38 } }, { 0.4, { 0.7, 0.55, 0.55 } }, { 0.7, { 0.9, 0.78, 0.7 } }, { 1, { 0.55, 0.38, 0.4 } } },
    { { 0, { 0.35, 0.42, 0.5 } }, { 0.45, { 0.62, 0.68, 0.72 } }, { 0.75, { 0.85, 0.85, 0.8 } }, { 1, { 0.5, 0.52, 0.55 } } },
    { { 0, { 0.18, 0.38, 0.42 } }, { 0.4, { 0.38, 0.62, 0.62 } }, { 0.7, { 0.72, 0.86, 0.82 } }, { 1, { 0.26, 0.48, 0.5 } } },
    { { 0, { 0.48, 0.18, 0.1 } }, { 0.4, { 0.75, 0.4, 0.22 } }, { 0.7, { 0.92, 0.7, 0.5 } }, { 1, { 0.6, 0.28, 0.16 } } },
    { { 0, { 0.3, 0.22, 0.48 } }, { 0.4, { 0.52, 0.42, 0.7 } }, { 0.7, { 0.8, 0.72, 0.88 } }, { 1, { 0.38, 0.3, 0.55 } } },
    { { 0, { 0.34, 0.4, 0.2 } }, { 0.4, { 0.58, 0.62, 0.38 } }, { 0.7, { 0.84, 0.84, 0.66 } }, { 1, { 0.42, 0.48, 0.28 } } },
    { { 0, { 0.86, 0.8, 0.7 } }, { 0.5, { 0.95, 0.92, 0.86 } }, { 1, { 0.8, 0.72, 0.62 } } },
};
const Pal HOT_PALS[] = {
    { { 0, { 0.16, 0.07, 0.06 } }, { 0.4, { 0.42, 0.17, 0.1 } }, { 0.7, { 0.62, 0.32, 0.18 } }, { 1, { 0.28, 0.11, 0.08 } } },
    { { 0, { 0.08, 0.08, 0.14 } }, { 0.4, { 0.2, 0.2, 0.32 } }, { 0.7, { 0.45, 0.35, 0.4 } }, { 1, { 0.12, 0.1, 0.18 } } },
    { { 0, { 0.35, 0.15, 0.04 } }, { 0.4, { 0.7, 0.35, 0.1 } }, { 0.7, { 0.9, 0.6, 0.3 } }, { 1, { 0.5, 0.2, 0.06 } } },
};
struct V3Pair { V3 a, b; };
const V3Pair ICE_GIANT_PALS[] = {
    { { 0.3, 0.5, 0.82 }, { 0.45, 0.66, 0.92 } }, { { 0.5, 0.78, 0.8 }, { 0.68, 0.88, 0.9 } },
    { { 0.68, 0.84, 0.9 }, { 0.85, 0.93, 0.96 } }, { { 0.16, 0.22, 0.55 }, { 0.3, 0.38, 0.75 } }, { { 0.4, 0.62, 0.6 }, { 0.6, 0.8, 0.74 } },
};
const Pal DESERT_PALS[] = {
    { { 0, { 0.45, 0.2, 0.12 } }, { 0.5, { 0.72, 0.4, 0.24 } }, { 1, { 0.86, 0.62, 0.42 } } },
    { { 0, { 0.5, 0.38, 0.22 } }, { 0.5, { 0.78, 0.62, 0.4 } }, { 1, { 0.92, 0.82, 0.6 } } },
    { { 0, { 0.7, 0.66, 0.6 } }, { 0.5, { 0.86, 0.84, 0.8 } }, { 1, { 0.97, 0.96, 0.93 } } },
    { { 0, { 0.3, 0.27, 0.25 } }, { 0.5, { 0.5, 0.45, 0.4 } }, { 1, { 0.7, 0.64, 0.56 } } },
    { { 0, { 0.08, 0.07, 0.07 } }, { 0.55, { 0.3, 0.27, 0.25 } }, { 1, { 0.85, 0.82, 0.78 } } },
    { { 0, { 0.55, 0.3, 0.1 } }, { 0.5, { 0.85, 0.55, 0.2 } }, { 1, { 0.95, 0.8, 0.5 } } },
};
struct V3Tri { V3 a, b, c; };
const V3Tri TOXIC_PALS[] = {
    { { 0.72, 0.58, 0.34 }, { 0.88, 0.78, 0.52 }, { 0.96, 0.9, 0.7 } }, { { 0.7, 0.42, 0.2 }, { 0.86, 0.6, 0.34 }, { 0.95, 0.8, 0.6 } },
    { { 0.55, 0.6, 0.3 }, { 0.74, 0.78, 0.44 }, { 0.9, 0.9, 0.66 } }, { { 0.7, 0.7, 0.68 }, { 0.84, 0.84, 0.8 }, { 0.95, 0.94, 0.9 } },
};
const V3 ROCK_TINTS[] = { { 0.92, 0.9, 0.86 }, { 0.8, 0.7, 0.6 }, { 0.85, 0.6, 0.48 }, { 0.6, 0.58, 0.56 }, { 0.95, 0.9, 0.8 }, { 0.7, 0.72, 0.78 } };
const V3 DWARF_TINTS[] = { { 0.85, 0.72, 0.6 }, { 0.9, 0.85, 0.8 }, { 0.75, 0.6, 0.55 } };
const V3 LAVA_TINTS[] = { { 1, 0.45, 0.12 }, { 1, 0.7, 0.2 }, { 0.95, 0.25, 0.1 } };
const V3 ICE_TINTS[] = { { 0.86, 0.92, 0.98 }, { 0.78, 0.92, 0.95 }, { 0.95, 0.86, 0.86 }, { 0.94, 0.92, 0.8 }, { 0.9, 0.9, 0.92 } };
const V3 LINE_TINTS[] = { { 0.6, 0.42, 0.32 }, { 0.45, 0.3, 0.4 }, { 0.3, 0.45, 0.55 }, { 0.7, 0.5, 0.2 } };
const V3Pair OCEANS[] = {
    { { 0.1, 0.34, 0.5 }, { 0.02, 0.09, 0.26 } }, { { 0.1, 0.5, 0.55 }, { 0.02, 0.17, 0.24 } }, { { 0.24, 0.22, 0.55 }, { 0.05, 0.05, 0.22 } },
    { { 0.12, 0.42, 0.32 }, { 0.02, 0.15, 0.12 } }, { { 0.08, 0.22, 0.3 }, { 0.01, 0.04, 0.09 } }, { { 0.22, 0.55, 0.62 }, { 0.03, 0.2, 0.36 } },
};
const V3 DRY[] = { { 0.66, 0.5, 0.3 }, { 0.62, 0.36, 0.18 }, { 0.76, 0.66, 0.46 }, { 0.56, 0.44, 0.3 }, { 0.7, 0.56, 0.36 } };

struct LookCtx { double hostT = 0; bool locked = false; double az = 0; double temp = 0; };

void livingLook(Look& L, Rng& r, Type type, const LookCtx& ctx)
{
    const bool redSun = ctx.hostT != 0 && ctx.hostT < 3900;
    const bool eye = ctx.locked && type != Type::Temperada;
    LookStyle style;
    if (eye) style = LookStyle::Olho;
    else if (type == Type::Oceano) style = r() < 0.5 ? LookStyle::Aberto : LookStyle::Arquipelago;
    else style = pickW<LookStyle>(r, { { LookStyle::Continental, 3 }, { LookStyle::Arquipelago, 1.6 }, { LookStyle::Floresta, 1.4 }, { LookStyle::Savana, 1.2 }, { LookStyle::Tundra, 1 }, { LookStyle::Pantano, 0.8 } });
    L.style = style;
    L.hasFreq = true;
    L.freq = style == LookStyle::Arquipelago ? uni(r, 2.2, 3.4) : style == LookStyle::Continental ? uni(r, 0.8, 1.5) : uni(r, 1.0, 2.0);
    // o Lab sorteia todos e pega um: a ordem dos sorteios tem que ser a mesma
    const double seaAberto = uni(r, 0.24, 0.32), seaArq = uni(r, 0.12, 0.2), seaCont = uni(r, -0.09, 0.02), seaFlo = uni(r, -0.1, -0.02);
    const double seaSav = uni(r, -0.12, -0.03), seaTun = uni(r, -0.08, 0.02), seaPan = uni(r, -0.03, 0.03), seaOlho = uni(r, -0.06, 0.08);
    switch (style) {
    case LookStyle::Aberto: L.sea = seaAberto; break; case LookStyle::Arquipelago: L.sea = seaArq; break; case LookStyle::Continental: L.sea = seaCont; break; case LookStyle::Floresta: L.sea = seaFlo; break;
    case LookStyle::Savana: L.sea = seaSav; break; case LookStyle::Tundra: L.sea = seaTun; break; case LookStyle::Pantano: L.sea = seaPan; break; default: L.sea = seaOlho; break;
    }
    const bool first = r() < 0.2;
    const V3Pair& oc = first ? OCEANS[0] : pick(r, OCEANS);
    L.shallow = jitter(oc.a, r, 0.15); L.deep = jitter(oc.b, r, 0.15);
    Veg vk = redSun ? pickW<Veg>(r, { { Veg::Escura, 3 }, { Veg::Carmim, 2 }, { Veg::Roxa, 2 }, { Veg::Verde, 1 } })
                    : pickW<Veg>(r, { { Veg::Verde, 5 }, { Veg::Selva, 3 }, { Veg::Oliva, 2 }, { Veg::Outono, 1 }, { Veg::Azulada, 1 }, { Veg::Carmim, 0.7 }, { Veg::Roxa, 0.7 } });
    if (style == LookStyle::Floresta && !redSun) vk = pickW<Veg>(r, { { Veg::Selva, 3 }, { Veg::Verde, 2 }, { Veg::Azulada, 1 }, { Veg::Roxa, 0.6 } });
    if (style == LookStyle::Savana) vk = pickW<Veg>(r, { { Veg::Oliva, 3 }, { Veg::Outono, 2 }, { Veg::Carmim, 0.7 } });
    L.vegKeyV = vk; L.veg = jitter(vegColor(vk), r, 0.2);
    L.dry = jitter(pick(r, DRY), r, 0.12);
    L.dryBias = style == LookStyle::Floresta ? -0.45 : style == LookStyle::Savana ? 0.25 : style == LookStyle::Tundra ? -0.1 : style == LookStyle::Pantano ? -0.3 : 0;
    L.belt = style == LookStyle::Floresta ? 0 : uni(r, 0.05, 0.3);
    L.rock = jitter(V3 { 0.34, 0.29, 0.24 }, r, 0.12);
    L.snowAt = style == LookStyle::Tundra ? 0.3 : uni(r, 0.5, 0.6);
    L.cap = style == LookStyle::Tundra ? uni(r, 0.7, 0.95) : r() < 0.15 ? 9 : uni(r, 1.05, 1.35);
    enum Sky { Limpo, Medio, Fechado };
    const Sky sky = pickW<Sky>(r, { { Limpo, 2 }, { Medio, 3 }, { Fechado, 1 } });
    L.cloud = sky == Limpo ? uni(r, 0.16, 0.3) : sky == Medio ? uni(r, 0.05, 0.16) : uni(r, -0.06, 0.05);
    L.swirl = r() < 0.5;
    L.cloudCol = r() < 0.15 ? V3 { 1.0, 0.97, 0.9 } : V3 { 1.02, 1.02, 1.03 };
    L.ice = r() < 0.2 ? V3 { 0.85, 0.92, 0.98 } : V3 { 0.93, 0.95, 0.98 };
    L.murk = jitter(V3 { 0.2, 0.24, 0.13 }, r, 0.2);
    if (eye) { double v = -PI_D / 2 - ctx.az; v = std::fmod(std::fmod(v + PI_D, TAU) + TAU, TAU) - PI_D; L.hasSub = true; L.sub = v; }
}

// [chão escuro, chão claro, cor de destaque]
const std::vector<V3Tri>& rockPal(LookStyle s)
{
    static const std::vector<V3Tri> craterado { { { 0.3, 0.29, 0.28 }, { 0.62, 0.6, 0.57 }, { 0.8, 0.79, 0.76 } }, { { 0.33, 0.28, 0.23 }, { 0.66, 0.58, 0.48 }, { 0.82, 0.76, 0.66 } }, { { 0.27, 0.28, 0.3 }, { 0.58, 0.58, 0.6 }, { 0.78, 0.79, 0.8 } } };
    static const std::vector<V3Tri> mares { { { 0.5, 0.49, 0.46 }, { 0.74, 0.72, 0.68 }, { 0.17, 0.17, 0.19 } }, { { 0.55, 0.49, 0.42 }, { 0.78, 0.7, 0.6 }, { 0.22, 0.19, 0.17 } }, { { 0.48, 0.48, 0.5 }, { 0.7, 0.7, 0.72 }, { 0.16, 0.17, 0.19 } } };
    static const std::vector<V3Tri> ferrugem { { { 0.4, 0.19, 0.1 }, { 0.76, 0.46, 0.28 }, { 0.22, 0.12, 0.08 } }, { { 0.48, 0.26, 0.12 }, { 0.84, 0.58, 0.34 }, { 0.28, 0.16, 0.1 } }, { { 0.32, 0.14, 0.11 }, { 0.62, 0.34, 0.26 }, { 0.18, 0.09, 0.08 } } };
    static const std::vector<V3Tri> fraturado { { { 0.24, 0.23, 0.22 }, { 0.42, 0.4, 0.37 }, { 0.7, 0.69, 0.66 } }, { { 0.3, 0.25, 0.2 }, { 0.48, 0.41, 0.33 }, { 0.74, 0.68, 0.58 } }, { { 0.22, 0.24, 0.26 }, { 0.38, 0.4, 0.43 }, { 0.66, 0.7, 0.74 } } };
    static const std::vector<V3Tri> carbono { { { 0.075, 0.07, 0.068 }, { 0.17, 0.16, 0.15 }, { 0.62, 0.61, 0.58 } }, { { 0.09, 0.08, 0.07 }, { 0.2, 0.17, 0.14 }, { 0.66, 0.6, 0.52 } } };
    static const std::vector<V3Tri> sal { { { 0.6, 0.55, 0.46 }, { 0.9, 0.87, 0.8 }, { 0.66, 0.52, 0.38 } }, { { 0.62, 0.58, 0.56 }, { 0.92, 0.9, 0.88 }, { 0.55, 0.5, 0.45 } }, { { 0.66, 0.54, 0.48 }, { 0.94, 0.86, 0.8 }, { 0.72, 0.5, 0.42 } } };
    static const std::vector<V3Tri> estratos { { { 0.42, 0.26, 0.17 }, { 0.8, 0.66, 0.48 }, { 0.6, 0.36, 0.22 } }, { { 0.34, 0.33, 0.24 }, { 0.7, 0.66, 0.5 }, { 0.42, 0.44, 0.3 } }, { { 0.4, 0.3, 0.32 }, { 0.82, 0.7, 0.62 }, { 0.58, 0.38, 0.4 } } };
    static const std::vector<V3Tri> tolinas { { { 0.3, 0.16, 0.1 }, { 0.66, 0.5, 0.38 }, { 0.93, 0.9, 0.86 } }, { { 0.36, 0.2, 0.12 }, { 0.72, 0.56, 0.44 }, { 0.9, 0.9, 0.92 } } };
    static const std::vector<V3Tri> geada { { { 0.6, 0.6, 0.62 }, { 0.92, 0.93, 0.95 }, { 0.42, 0.4, 0.4 } }, { { 0.66, 0.62, 0.6 }, { 0.95, 0.93, 0.9 }, { 0.5, 0.44, 0.4 } } };
    switch (s) { case LookStyle::Mares: return mares; case LookStyle::Ferrugem: return ferrugem; case LookStyle::Fraturado: return fraturado; case LookStyle::Carbono: return carbono; case LookStyle::Sal: return sal; case LookStyle::Estratos: return estratos; case LookStyle::Tolinas: return tolinas; case LookStyle::Geada: return geada; default: return craterado; }
}

// Escolhe a paisagem pela temperatura (perto da estrela é cratera e basalto; no frio, gelo nos fundos).
// Usa um acaso próprio, tirado da semente do visual: a galáxia em volta não muda.
void rockLook(Look& L, Type type, const LookCtx& ctx, LookStyle force = LookStyle::None)
{
    Rng rr(L.seed ^ 0x0c4a7u);
    const double T = ctx.temp != 0 ? ctx.temp : 230;
    using W = std::pair<LookStyle, double>;
    LookStyle st = force;
    if (force == LookStyle::None) {
        using S = LookStyle;
        if (type == Type::Anao) st = pickW<S>(rr, { W { S::Tolinas, 2.2 }, W { S::Geada, 1.6 }, W { S::Carbono, 1.2 }, W { S::Craterado, 1 }, W { S::Sal, 0.7 } });
        else if (type == Type::Rochosa) st = pickW<S>(rr, { W { S::Craterado, 2.4 }, W { S::Mares, 1.4 }, W { S::Fraturado, 1.4 }, W { S::Carbono, 1.6 }, W { S::Ferrugem, 0.7 }, W { S::Sal, 0.5 } });
        else if (T > 500) st = pickW<S>(rr, { W { S::Craterado, 3 }, W { S::Mares, 2 }, W { S::Carbono, 1.4 }, W { S::Fraturado, 1.4 }, W { S::Estratos, 0.8 }, W { S::Ferrugem, 1 }, W { S::Sal, 0.3 } });
        else if (T > 230) st = pickW<S>(rr, { W { S::Ferrugem, 2.6 }, W { S::Estratos, 2 }, W { S::Sal, 1.5 }, W { S::Craterado, 1.6 }, W { S::Mares, 1 }, W { S::Fraturado, 1 }, W { S::Carbono, 0.8 } });
        else st = pickW<S>(rr, { W { S::Craterado, 2 }, W { S::Fraturado, 2 }, W { S::Carbono, 1.8 }, W { S::Mares, 1 }, W { S::Sal, 1 }, W { S::Ferrugem, 0.6 }, W { S::Estratos, 0.5 } });
    }
    L.style = st;
    const auto& pals = rockPal(st);
    const V3Tri& pal = pals[irand(rr, 0, int(pals.size()) - 1)];
    L.hasRock = true;
    // Varia o brilho e só um pouco a matiz (senão o cinza vira verde).
    const auto jit = [&](const V3& c) { const double k = 1 + (rr() - 0.5) * 0.22; V3 o; for (int i = 0; i < 3; i++) o[i] = clamp(c[i] * k * (1 + (rr() - 0.5) * 0.05), 0, 1); return o; };
    L.lo = jit(pal.a); L.hi = jit(pal.b); L.ac = jit(pal.c);
    L.k1 = uni(rr, 0.8, 1.4); L.thr = uni(rr, -0.08, 0.1);
    // todos sorteados em ordem, como no Lab
    const int nC = irand(rr, 80, 120), nM = irand(rr, 45, 75), nF = irand(rr, 18, 36), nFr = irand(rr, 28, 48), nCa = irand(rr, 55, 85), nS = irand(rr, 4, 12), nE = irand(rr, 6, 14), nT = irand(rr, 10, 22), nG = irand(rr, 22, 40);
    switch (st) {
    case LookStyle::Mares: L.nCr = nM; L.cells = 0.75; break; case LookStyle::Ferrugem: L.nCr = nF; L.cells = 0.3; break; case LookStyle::Fraturado: L.nCr = nFr; L.cells = 0.55; break;
    case LookStyle::Carbono: L.nCr = nCa; L.cells = 0.9; break; case LookStyle::Sal: L.nCr = nS; L.cells = 0.08; break; case LookStyle::Estratos: L.nCr = nE; L.cells = 0.1; break;
    case LookStyle::Tolinas: L.nCr = nT; L.cells = 0.15; break; case LookStyle::Geada: L.nCr = nG; L.cells = 0.45; break; default: L.nCr = nC; L.cells = 1; break;
    }
    L.fresh = uni(rr, 0.08, 0.2);
    L.frost = T < 170 && st != LookStyle::Geada && st != LookStyle::Tolinas ? uni(rr, 0.35, 0.8) : 0;
    L.cseed = irand(rr, 1, 1 << 30);
    L.volc.clear();
    if (st == LookStyle::Ferrugem) {
        for (int i = irand(rr, 1, 3); i > 0; i--) { const double z = uni(rr, -0.5, 0.5), a = rr() * TAU, s = std::sqrt(1 - z * z); const double q = uni(rr, 0.08, 0.2); L.volc.push_back({ s * std::cos(a), s * std::sin(a), z, q }); }
        L.canyon = rr() < 0.75;
    }
    if (st == LookStyle::Estratos) {
        L.bandsI = irand(rr, 7, 13);
        L.bandCols.clear();
        for (int i = 0; i < 5; i++) {
            const double t = rr();
            V3 base;
            for (int j = 0; j < 3; j++) { const double v = mix(L.lo[j], L.hi[j], t); base[j] = i % 2 ? mix(v, L.ac[j], 0.6) : v; }
            L.bandCols.push_back(jitter(base, rr, 0.15));
        }
    }
    if (st == LookStyle::Tolinas) { const double z = uni(rr, -0.4, 0.5), a = rr() * TAU, s = std::sqrt(1 - z * z); L.plain = { s * std::cos(a), s * std::sin(a), z }; L.plainR = uni(rr, 0.35, 0.6); }
    if (st == LookStyle::Sal) L.cellF = uni(rr, 7, 12);
}

Look makeLook(Type type, Rng& r, const LookCtx& ctx)
{
    Look L;
    L.seed = uint32_t(r() * 4294967296.0);
    if (isArtificial(type)) {
        L.artificial = true;
        L.tone = irand(r, 0, 2);
        if (type == Type::Fortaleza) {
            // O prato do canhão fica no hemisfério de cima, em algum ponto do meridiano.
            const double lat = uni(r, 0.35, 0.8), lon = uni(r, -3, 3), cl = std::cos(lat);
            L.hasDish = true; L.dish = { cl * std::cos(lon), cl * std::sin(lon), std::sin(lat) };
        }
        if (type == Type::Ecumeno) L.pop = 1;
        return L;
    }
    const auto storms = [&](int n, std::initializer_list<V3> colors, double wMax) {
        std::vector<Storm> out;
        const std::vector<V3> cs(colors);
        for (int i = 0; i < n; i++) {
            Storm s;
            s.lat = uni(r, -0.6, 0.6); s.lon = uni(r, -3, 3); s.w = uni(r, 0.08, wMax); s.h = uni(r, 0.04, 0.11);
            s.c = jitter(cs[irand(r, 0, int(cs.size()) - 1)], r, 0.2);
            out.push_back(s);
        }
        return out;
    };
    if (type == Type::Gigante) {
        const Pal& p = pick(r, GAS_PALS);
        for (const PalStop& s : p) L.pal.push_back({ s.t, jitter(s.c, r, 0.18) });
        L.bandsF = uni(r, 3, 16); L.turb = uni(r, 0.4, 2.2);
        const int n = r() < 0.6 ? irand(r, 1, 3) : 0;
        L.storms = storms(n, { V3 { 0.85, 0.45, 0.3 }, V3 { 0.95, 0.92, 0.85 }, V3 { 0.35, 0.25, 0.2 } }, 0.35);
    } else if (type == Type::Quente) {
        L.pal = pick(r, HOT_PALS);
        L.bandsF = uni(r, 3, 9); L.turb = uni(r, 1, 2.2);
    } else if (type == Type::GeloG) {
        const V3Pair& c = pick(r, ICE_GIANT_PALS);
        const V3 a = jitter(c.a, r, 0.12), b = jitter(c.b, r, 0.12), d = jitter(c.a, r, 0.1);
        L.pal = { { 0, a }, { 0.5, b }, { 1, d } };
        L.bandsF = uni(r, 2, 7); L.turb = uni(r, 0.2, 0.9);
        const int n = r() < 0.35 ? irand(r, 1, 2) : 0;
        L.storms = storms(n, { V3 { 0.12, 0.2, 0.45 }, V3 { 0.92, 0.95, 1 } }, 0.25);
    } else if (type == Type::Subnetuno) {
        const V3 a = jitter(V3 { 0.55, 0.6, 0.72 }, r, 0.25), b = jitter(V3 { 0.72, 0.76, 0.86 }, r, 0.2), d = jitter(V3 { 0.58, 0.62, 0.74 }, r, 0.2);
        L.pal = { { 0, a }, { 0.5, b }, { 1, d } };
        L.bandsF = uni(r, 2, 6); L.turb = uni(r, 0.3, 0.8);
    } else if (isLiving(type)) {
        livingLook(L, r, type, ctx);
    } else if (type == Type::Deserto) {
        const Pal& p = pick(r, DESERT_PALS);
        for (const PalStop& s : p) L.pal.push_back({ s.t, jitter(s.c, r, 0.15) });
        L.cap = r() < 0.5 ? uni(r, 1.15, 1.4) : 9;
        L.canyon = r() < 0.4;
    } else if (type == Type::Arido || type == Type::Rochosa || type == Type::Anao) {
        const V3& base = type == Type::Anao ? pick(r, DWARF_TINTS) : pick(r, ROCK_TINTS);
        L.tint = jitter(base, r, 0.12);
        L.craters = irand(r, 15, 70);
        rockLook(L, type, ctx);
    } else if (type == Type::Lava) {
        L.tint = jitter(pick(r, LAVA_TINTS), r, 0.15);
        L.crack = uni(r, 0.68, 0.82); L.pool = uni(r, 0.15, 0.38);
    } else if (type == Type::Gelado || type == Type::Gelada) {
        L.tint = jitter(pick(r, ICE_TINTS), r, 0.06); L.line = jitter(pick(r, LINE_TINTS), r, 0.2);
    } else if (type == Type::Toxico) {
        const V3Tri& c = pick(r, TOXIC_PALS);
        const V3 a = jitter(c.a, r, 0.1), b = jitter(c.b, r, 0.08), d = jitter(c.c, r, 0.06);
        L.pal = { { 0, a }, { 0.5, b }, { 1, d } };
    } else if (type == Type::Vulcanica) {
        L.tint = jitter(V3 { 0.88, 0.78, 0.36 }, r, 0.12);
    }
    return L;
}
} // namespace

Frame frameOf(double tiltDeg, double az)
{
    const double t = (tiltDeg * PI_D) / 180;
    Frame f;
    f.ax = { std::sin(t) * std::cos(az), std::sin(t) * std::sin(az), std::cos(t) };
    f.e1 = { -std::sin(az), std::cos(az), 0 };
    f.e2 = { f.ax[1] * f.e1[2] - f.ax[2] * f.e1[1], f.ax[2] * f.e1[0] - f.ax[0] * f.e1[2], f.ax[0] * f.e1[1] - f.ax[1] * f.e1[0] };
    return f;
}

static std::unique_ptr<Rings> makeRings(Rng& r, Cls cls)
{
    auto R = std::make_unique<Rings>();
    const Noise nz(uint32_t(r() * 1e9));
    const double gapAt = uni(r, 0.45, 0.7);
    for (int i = 0; i < 48; i++) {
        const double t = i / 47.0;
        double v = 0.35 + 0.65 * (nz.fbm2(t * 9, 0.5, 3) + 0.5);
        if (std::fabs(t - gapAt) < 0.035) v *= 0.08;
        v *= smooth(0, 0.12, t) * (1 - smooth(0.82, 1, t));
        R->bands[i] = float(clamp(v, 0, 1));
    }
    const bool rocky = cls == Cls::Rocha || cls == Cls::Anao;
    static const V3 tGas[] = { { 0.86, 0.78, 0.64 }, { 0.8, 0.72, 0.6 }, { 0.9, 0.86, 0.8 }, { 0.75, 0.6, 0.5 } };
    static const V3 tRock[] = { { 0.62, 0.6, 0.58 }, { 0.78, 0.8, 0.84 }, { 0.6, 0.5, 0.42 } };
    static const V3 tIce[] = { { 0.7, 0.75, 0.8 }, { 0.6, 0.7, 0.78 } };
    const V3& base = cls == Cls::Gas ? tGas[irand(r, 0, 3)] : rocky ? tRock[irand(r, 0, 2)] : tIce[irand(r, 0, 1)];
    R->tint = jitter(base, r, 0.15);
    R->alpha = cls == Cls::Gas ? uni(r, 0.55, 0.85) : rocky ? uni(r, 0.35, 0.6) : uni(r, 0.2, 0.4);
    R->inner = rocky ? uni(r, 1.35, 1.6) : uni(r, 1.25, 1.5);
    R->outer = rocky ? uni(r, 1.9, 2.4) : uni(r, 2.0, 2.5);
    return R;
}

static const char* const ATM_LIVING = "Nitrogênio, vapor d’água e CO₂";

static std::vector<std::unique_ptr<Body>> genMoons(Body& p, const System& sys, Rng& r)
{
    int n = 0;
    if (p.cls == Cls::Gas) n = irand(r, p.wantHabMoon ? 3 : 2, 9);
    else if (p.cls == Cls::Gelo) n = irand(r, 1, 6);
    else if (p.cls == Cls::Sub) n = irand(r, 0, 2);
    else if (p.cls == Cls::Rocha) n = p.mass > 0.3 && r() < 0.4 ? irand(r, 1, 2) : 0;
    else n = r() < 0.25 ? 1 : 0;
    const double hillKm = p.a * std::pow(p.mass / (3 * sys.M * MSUN_ME), 1.0 / 3) * AU_KM;
    const double RpKm = p.radius * RE_KM;
    const double lo = RpKm * (p.cls == Cls::Gas ? 2.6 : 3), hi = std::min(hillKm * 0.4, lo * 80);
    if (hi < lo * 1.5) n = 0;
    const double MpKg = p.mass * ME_KG;
    std::vector<std::unique_ptr<Body>> out;
    // Gigante na zona habitável: uma das luas grandes pode ser um mundo vivo.
    const bool giant = p.cls == Cls::Gas || p.cls == Cls::Gelo;
    const int habMoon = giant && p.hz == Hz::Dentro && n > 0 && (p.wantHabMoon || r() < 0.4 + sys.life * 0.55) ? std::min(1, n - 1) : -1;
    for (int i = 0; i < n; i++) {
        const double d = lo * std::pow(hi / lo, (i + uni(r, 0.15, 0.85)) / n);
        Type type;
        if (i == habMoon) type = Type::Temperada;
        else if (giant) type = i == 0 && p.cls == Cls::Gas && r() < 0.5 ? Type::Vulcanica : r() < 0.6 ? Type::Gelada : Type::Rochosa;
        else type = p.temp < 160 && r() < 0.6 ? Type::Gelada : Type::Rochosa;
        const double big = p.cls == Cls::Gas ? 2700 : p.cls == Cls::Gelo ? 1400 : std::min(RpKm * 0.3, 1800.0);
        const double rad = type == Type::Temperada ? logU(r, 2600, 6400) : logU(r, std::min(8.0, big * 0.5), big);
        const double rho = type == Type::Gelada ? 1.9 : type == Type::Vulcanica ? 3.5 : type == Type::Temperada ? 4.8 : 3.0;
        const double mKg = rho * 1000 * (4.0 / 3) * PI_D * std::pow(rad * 1000, 3);
        const double Psec = TAU * std::sqrt(std::pow(d * 1000, 3) / (G_SI * MpKg));
        auto m = std::make_unique<Body>();
        m->body = BodyKind::Lua; m->type = type; m->idx = i + 1; m->uid = i + 1; m->dist = d; m->radiusKm = rad; m->massKg = mKg;
        m->gravity = (G_SI * mKg) / std::pow(rad * 1000, 2) / 9.81; m->P = Psec / 86400; m->M0 = r() * TAU;
        const double ft = uni(r, 0, 4), fa = r() * TAU;
        m->frame = frameOf(ft, fa);
        LookCtx ctx; ctx.hostT = sys.stars[0].temp; ctx.locked = false; ctx.temp = p.teq;
        m->look = makeLook(type, r, ctx);
        if (type == Type::Temperada) {
            const double k = p.teq * std::pow(0.7 / (1 - p.albedo), 0.25);
            m->temp = k + std::max(0.0, uni(r, 280, 298) - k);
            m->atm = ATM_LIVING; m->water = true; m->hz = Hz::Dentro;
        }
        out.push_back(std::move(m));
    }
    return out;
}

static std::unique_ptr<Body> makePlanet(const System& sys, Rng& r, double a, bool hotJ, bool hab = false, bool giantHab = false, const Cls* forceCls = nullptr)
{
    const double Ls = std::max(sys.L, 1e-7), Ms = sys.M, frost = sys.frostEff;
    Cls cls;
    if (forceCls) cls = *forceCls;
    else if (hab) cls = Cls::Rocha;
    else if (hotJ || giantHab) cls = Cls::Gas;
    else if (sys.kind == StarKind::Neutrons) cls = Cls::Rocha;
    else if (a < frost) cls = r() < 0.1 && Ms > 0.4 ? Cls::Sub : Cls::Rocha;
    else if (a < frost * 4 && r() < (Ms < 0.5 ? 0.35 : 0.7)) cls = Cls::Gas;
    else if (a < frost * 14 && r() < 0.65) cls = Cls::Gelo;
    else cls = r() < 0.6 ? Cls::Anao : Cls::Rocha;
    // Gigante que migrou pra zona habitável: é ele que pode ter lua habitável.
    if (!forceCls && !hab && !hotJ && cls == Cls::Rocha && sys.hasHz && a > sys.hz[0] && a < sys.hz[1] && r() < 0.22) cls = Cls::Gas;
    double M, R;
    if (cls == Cls::Rocha) { M = hab ? logU(r, 0.4, 3) : r() < 0.55 ? logU(r, 0.03, 1.2) : logU(r, 1.2, 8); R = std::pow(M, 0.27); }
    else if (cls == Cls::Anao) { M = logU(r, 0.0005, 0.02); R = std::pow(M, 0.3) * 1.1; }
    else if (cls == Cls::Sub) { M = logU(r, 3, 15); R = 1.8 + (M - 3) * 0.08; }
    else if (cls == Cls::Gelo) { M = logU(r, 8, 35); const double q = uni(r, 0.95, 1.06); R = 3.7 * std::pow(M / 15, 0.12) * q; }
    else { M = logU(r, 25, 2500); R = M < 318 ? 11.2 * std::pow(M / 318, 0.15) : 11.2 * std::pow(M / 318, -0.04); }
    const double T0 = 278.6 * std::pow(Ls, 0.25) / std::sqrt(a);
    Type type = Type::Arido;
    double albedo = 0, greenhouse = 0;
    std::string atm = "Nenhuma";
    bool water = false;
    const bool rockish = cls == Cls::Rocha || cls == Cls::Anao;
    if (cls == Cls::Gas) {
        type = T0 > 900 ? Type::Quente : Type::Gigante; albedo = type == Type::Quente ? 0.1 : 0.34;
        atm = "Hidrogênio e hélio";
        if (type == Type::Quente) R *= uni(r, 1.1, 1.4);
    } else if (cls == Cls::Gelo) { type = Type::GeloG; albedo = 0.29; atm = "Hidrogênio, hélio e metano"; }
    else if (cls == Cls::Sub) { type = Type::Subnetuno; albedo = 0.3; atm = "Hidrogênio espesso e vapor"; }
    else if (cls == Cls::Anao) { type = Type::Anao; albedo = 0.55; }
    else if (hab) {
        type = r() < 0.35 ? Type::Oceano : Type::Temperado; albedo = 0.3; water = true;
        greenhouse = std::max(0.0, uni(r, 280, 298) - T0 * std::pow(0.7, 0.25));
        atm = ATM_LIVING;
    } else {
        const bool watery = r() < (sys.hasHz && a > sys.hz[0] * 0.7 ? 0.55 : 0.2);
        albedo = 0.3;
        const double Teq0 = T0 * std::pow(1 - albedo, 0.25);
        const bool holds = M > 0.2 + (Teq0 > 500 ? 0.8 : 0);
        if (holds && sys.hasHz && a < sys.hz[0] && a > sys.hz[0] * 0.45 && M > 0.5 && r() < 0.6) {
            type = Type::Toxico; albedo = 0.7; greenhouse = uni(r, 300, 450); atm = "Dióxido de carbono denso, nuvens ácidas";
        } else {
            greenhouse = holds ? uni(r, 5, 40) * std::sqrt(std::min(M, 4.0)) : 0;
            const double Ts = Teq0 + greenhouse;
            if (Ts > 900) { type = Type::Lava; albedo = 0.1; atm = holds ? "Vapor de rocha, fina" : "Nenhuma"; }
            else if (holds && Ts > 260 && Ts < 320 && M > 0.25 && M < 6) {
                type = watery && r() < 0.4 ? Type::Oceano : Type::Temperado; water = true;
                atm = ATM_LIVING;
            } else if (Ts >= 320) { if (holds) { type = Type::Deserto; atm = "Fina, CO₂ e poeira"; } else type = Type::Arido; }
            else if (Ts < 210 && watery) { type = Type::Gelado; albedo = 0.6; atm = holds ? "Fina, nitrogênio" : "Nenhuma"; }
            else if (holds && Ts >= 210 && r() < 0.45) { type = Type::Deserto; atm = "Fina, CO₂"; }
            else if (holds && Ts >= 210 && watery) { type = Type::Gelado; albedo = 0.5; atm = "Fina, nitrogênio e CO₂"; }
            else { type = Type::Arido; atm = "Nenhuma"; }
        }
    }
    const double Teq = T0 * std::pow(1 - albedo, 0.25);
    const double temp = Teq + greenhouse;
    const double P = 365.25 * std::sqrt((a * a * a) / Ms);
    const double aLock = 0.28 * std::pow(Ms, 1.0 / 3) * std::pow(std::max(sys.age, 0.1) / 4.5, 1.0 / 6);
    const bool locked = (rockish && a < aLock) || (cls == Cls::Gas && a < 0.1) || (cls == Cls::Sub && a < aLock * 0.7);
    const double dayH = locked ? P * 24 : rockish ? logU(r, 9, 140) : cls == Cls::Gas ? uni(r, 9, 18) : uni(r, 13, 19);
    const bool retro = !locked && r() < 0.08;
    double tilt = locked ? uni(r, 0, 3) : std::min(80.0, std::fabs(gauss(r)) * 18);
    if (!locked && r() < 0.05) tilt = uni(r, 80, 100);
    const double az = r() * TAU;
    auto p = std::make_unique<Body>();
    p->body = BodyKind::Planeta; p->cls = cls; p->type = type; p->a = a;
    {
        const double g = std::fabs(gauss(r)) * 0.04;
        const double extra = r() < 0.1 ? uni(r, 0.08, 0.3) : 0;
        p->e = std::min(0.5, g + extra);
    }
    p->w = r() * TAU; p->M0 = r() * TAU; p->P = P; p->mass = M; p->radius = R; p->gravity = M / (R * R); p->density = 5.51 * M / (R * R * R);
    p->albedo = albedo; p->teq = Teq; p->temp = temp; p->atm = atm; p->water = water; p->locked = locked; p->dayH = dayH; p->retro = retro; p->tilt = tilt; p->az = az;
    p->hz = sys.hasHz ? (a < sys.hz[0] ? Hz::Quente : a > sys.hz[1] ? Hz::Fria : Hz::Dentro) : Hz::None;
    p->wantHabMoon = giantHab;
    LookCtx ctx; ctx.hostT = sys.stars[0].temp; ctx.locked = locked; ctx.az = az; ctx.temp = temp;
    p->look = makeLook(type, r, ctx);
    p->frame = frameOf(tilt, az);
    const double ringP = cls == Cls::Gas ? 0.4 : cls == Cls::Gelo ? 0.3 : cls == Cls::Sub ? 0.1 : cls == Cls::Anao ? 0.05 : M > 0.2 ? 0.08 : 0;
    if (r() < ringP) p->rings = makeRings(r, cls);
    p->moons = genMoons(*p, sys, r);
    return p;
}

static void genPlanets(System& sys, Rng& r, double life)
{
    const StarKind kind = sys.kind;
    int N0 = 0, N1 = 0;
    switch (kind) {
    case StarKind::M: N0 = 1; N1 = 6; break; case StarKind::K: N0 = 2; N1 = 8; break; case StarKind::G: N0 = 3; N1 = 9; break; case StarKind::F: N0 = 2; N1 = 9; break;
    case StarKind::A: N0 = 1; N1 = 7; break; case StarKind::B: N0 = 0; N1 = 5; break; case StarKind::O: N0 = 0; N1 = 3; break; case StarKind::Gigante: N0 = 1; N1 = 6; break;
    case StarKind::AnaBranca: N0 = 0; N1 = 3; break; case StarKind::Neutrons: N0 = 0; N1 = 3; break; case StarKind::BuracoNegro: N0 = 0; N1 = 2; break; default: N0 = 0; N1 = 6; break;
    }
    const int n = irand(r, N0, N1);
    double maxR = 0;
    for (const Star& s : sys.stars) maxR = std::max(maxR, s.radius);
    const double rStarAU = maxR * 0.00465;
    double a = std::max(rStarAU * 4, logU(r, 0.03, 0.35) * std::sqrt(sys.M));
    if (kind == StarKind::Binaria) a = std::max(a, sys.sep * 3.2);
    if (kind == StarKind::Gigante) a = std::max(std::max(a, rStarAU * 3), logU(r, 0.8, 2.5));
    if (kind == StarKind::AnaBranca || kind == StarKind::BuracoNegro) a = logU(r, 0.5, 4);
    if (kind == StarKind::Neutrons) a = logU(r, 0.15, 0.5);
    const bool hotJ = (kind == StarKind::G || kind == StarKind::K || kind == StarKind::F) && r() < 0.09;
    std::vector<std::unique_ptr<Body>> list;
    for (int i = 0; i < n; i++) {
        const double aa = hotJ && i == 0 ? logU(r, 0.02, 0.07) * std::sqrt(sys.M) : a;
        list.push_back(makePlanet(sys, r, aa, hotJ && i == 0));
        a = std::max(a, aa) * uni(r, 1.4, 2.15);
    }
    // Mundos habitáveis a mais (controle da geração). Usa um acaso à parte, pra galáxia e as estrelas não mudarem.
    if (life > 0 && sys.hasHz) {
        Rng rL(sys.seed ^ 0x5eed1u);
        for (int k = 0; k < 2; k++) {
            const double chance = k ? std::max(0.0, life - 0.6) * 0.75 : life * 0.92;
            if (rL() >= chance) continue;
            const double mid = std::sqrt(sys.hz[0] * sys.hz[1]);
            const double aH = mid * (k ? (rL() < 0.5 ? 0.86 : 1.14) : uni(rL, 0.94, 1.06));
            bool near0 = false;
            for (const auto& p : list) if (p->hab && std::fabs(std::log(p->a / aH)) < 0.1) { near0 = true; break; }
            if (near0) continue;
            // Às vezes o mundo vivo é a lua de um gigante que mora na zona habitável.
            const bool pandora = rL() < 0.3;
            auto np = makePlanet(sys, rL, aH, false, !pandora, pandora);
            np->hab = true;
            int nearI = -1; double best = 0.3;
            for (size_t i = 0; i < list.size(); i++) { const Body& p = *list[i]; const double d = std::fabs(std::log(p.a / aH)); if (!p.hab && p.cls != Cls::Gas && p.cls != Cls::Gelo && d < best) { best = d; nearI = int(i); } }
            if (nearI >= 0) list[size_t(nearI)] = std::move(np); else list.push_back(std::move(np));
            std::stable_sort(list.begin(), list.end(), [](const std::unique_ptr<Body>& p, const std::unique_ptr<Body>& q) { return p->a < q->a; });
        }
    }
    for (size_t i = 0; i < list.size(); i++) { list[i]->idx = int(i) + 1; list[i]->uid = int(i) + 1; }
    sys.planetCount = int(list.size());
    // Cinturões: no vão entre o último rochoso e o primeiro gigante; e um disco gelado depois do último planeta.
    sys.belts.clear();
    for (size_t i = 0; i + 1 < list.size(); i++) {
        const Body& p = *list[i]; const Body& q = *list[i + 1];
        const bool rocky = p.cls == Cls::Rocha || p.cls == Cls::Sub, giant = q.cls == Cls::Gas || q.cls == Cls::Gelo;
        if (rocky && giant && q.a / p.a > 1.8 && r() < 0.65) { Belt b; b.a0 = p.a * 1.25; b.a1 = q.a * 0.72; b.n = 900; sys.belts.push_back(b); break; }
    }
    if (sys.belts.empty() && list.size() < 3 && sys.hasFrost && r() < 0.3) { Belt b; b.a0 = sys.frost * 0.6; b.a1 = sys.frost * 0.95; b.n = 700; sys.belts.push_back(b); }
    const double last = !list.empty() ? list.back()->a : sys.frostEff * 3;
    if ((!list.empty() && r() < 0.55) || (list.empty() && r() < 0.5)) { Belt b; b.kuiper = true; b.a0 = last * 1.35; b.a1 = last * 2.4; b.n = 1100; sys.belts.push_back(b); }
    double am = std::max(last, 0.05);
    for (const Belt& b : sys.belts) am = std::max(am, b.a1);
    sys.amax = am;
    sys.bodies = std::move(list);
    // Cometas: elipses compridas que mergulham perto da estrela.
    bool hasKuiper = false;
    for (const Belt& b : sys.belts) if (b.kuiper) hasKuiper = true;
    const int nc = irand(r, 0, 2) + (hasKuiper ? 1 : 0);
    for (int i = 0; i < nc; i++) {
        const double q = std::max(rStarAU * 6, logU(r, 0.25, 2) * std::sqrt(std::max(sys.L, 0.02)));
        double e = uni(r, 0.6, 0.94), ca = q / (1 - e);
        if (ca * (1 + e) > sys.amax * 2.5) { const double Q = sys.amax * 2.5; ca = (q + Q) / 2; e = (Q - q) / (Q + q); }
        auto c = std::make_unique<Body>();
        c->body = BodyKind::Cometa; c->type = Type::Cometa; c->a = ca; c->e = e; c->w = r() * TAU; c->M0 = r() * TAU; c->P = 365.25 * std::sqrt((ca * ca * ca) / sys.M);
        c->radius = logU(r, 0.0003, 0.004); c->idx = i + 1;
        c->look.seed = uint32_t(r() * 4294967296.0);
        sys.bodies.push_back(std::move(c));
    }
}

static void buildSystem(System& sys, double life)
{
    Rng r(sys.seed);
    double wO = 1, wB = 3.5, wA = 6, wGi = 4.5;
    if (sys.young > 0.45) { wO *= 2.6; wB *= 2.6; wA *= 2.6; }
    if (sys.rCore < 0.2) { wGi *= 2; wO *= 0.3; wB *= 0.3; }
    const StarKind kind = pickW<StarKind>(r, { { StarKind::M, 30 }, { StarKind::K, 19 }, { StarKind::G, 15 }, { StarKind::F, 9 }, { StarKind::A, wA }, { StarKind::B, wB }, { StarKind::O, wO }, { StarKind::Gigante, wGi }, { StarKind::AnaBranca, 4 }, { StarKind::Neutrons, 2 }, { StarKind::BuracoNegro, 1.4 }, { StarKind::Binaria, 9 } });
    sys.kind = kind;
    if (kind == StarKind::Binaria) {
        const StarKind k1 = pickW<StarKind>(r, { { StarKind::M, 3 }, { StarKind::K, 3 }, { StarKind::G, 3 }, { StarKind::F, 2 }, { StarKind::A, 1 } });
        Star s1 = makeStar(r, k1);
        const StarKind k2 = pickW<StarKind>(r, { { StarKind::M, 4 }, { StarKind::K, 3 }, { StarKind::G, 2 }, { StarKind::F, 1 } });
        Star s2 = makeStar(r, k2);
        if (s2.mass > s1.mass) std::swap(s1, s2);
        sys.stars = { s1, s2 };
        sys.sep = logU(r, 0.05, 0.5);
        sys.binP = 365.25 * std::sqrt(std::pow(sys.sep, 3) / (s1.mass + s2.mass));
        sys.label = "Estrela dupla";
    } else {
        sys.stars = { makeStar(r, kind) };
        sys.label = sys.stars[0].label;
    }
    sys.M = 0; sys.L = 0;
    for (const Star& s : sys.stars) { sys.M += s.mass; sys.L += s.lum; }
    const double Ls = std::max(sys.L, 1e-7);
    const bool remnant = kind == StarKind::AnaBranca || kind == StarKind::Neutrons || kind == StarKind::BuracoNegro;
    sys.hasHz = !(remnant || kind == StarKind::O);
    if (sys.hasHz) { sys.hz[0] = std::sqrt(Ls / 1.1); sys.hz[1] = std::sqrt(Ls / 0.53); }
    sys.hasFrost = !remnant;
    sys.frost = remnant ? 0 : 2.7 * std::sqrt(Ls);
    sys.frostEff = 2.7 * std::sqrt(std::max(Ls, 1e-4));
    sys.age = sys.stars[0].age;
    sys.life = life;
    genPlanets(sys, r, life);
}

static void buildCoreSystem(System& sys)
{
    Rng r(sys.seed);
    sys.kind = StarKind::Nucleo;
    sys.stars = { makeStar(r, StarKind::Nucleo) };
    sys.label = starKindLabel(StarKind::Nucleo);
    sys.M = sys.stars[0].mass; sys.L = 0; sys.hasHz = false; sys.hasFrost = false;
    sys.bodies.clear(); sys.belts.clear();
    // Estrelas S: correm em elipses fechadas em volta do buraco negro.
    const int n = irand(r, 8, 13);
    double am = 0;
    for (int i = 0; i < n; i++) {
        const StarKind k = pickW<StarKind>(r, { { StarKind::B, 5 }, { StarKind::O, 1 }, { StarKind::A, 2 }, { StarKind::Gigante, 1.5 } });
        Star st = makeStar(r, k);
        const double a = logU(r, 300, 3000), e = uni(r, 0.3, 0.96);
        auto b = std::make_unique<Body>();
        b->body = BodyKind::Estrela; b->type = Type::Estrela; b->star = std::make_unique<Star>(st); b->a = a; b->e = e; b->w = r() * TAU; b->M0 = r() * TAU;
        b->P = 365.25 * std::sqrt((a * a * a) / sys.M); b->idx = i + 1;
        am = std::max(am, a);
        sys.bodies.push_back(std::move(b));
    }
    sys.amax = am;
    sys.planetCount = 0;
}

// ── geração da galáxia ──
static void sampleSystems(Galaxy& G, Rng& r)
{
    const int want = G.count;
    const double dmin = std::min(0.3, 0.72 / std::sqrt(double(want)));
    if (G.style != Style::Irregular) {
        auto core = std::make_unique<System>();
        core->id = 0; core->seed = uint32_t(uint64_t(G.seed) * 101u + 7u); core->isCore = true;
        buildCoreSystem(*core);
        G.systems.push_back(std::move(core));
    }
    int tries = 0;
    while (int(G.systems.size()) < want && tries < want * 60) {
        tries++;
        const int k = sampleCdf(G.cdfSys, r());
        double x, y;
        cellPos(G, k, r, x, y);
        bool ok = true;
        for (const auto& s : G.systems) { const double dx = s->x - x, dy = s->y - y; if (dx * dx + dy * dy < dmin * dmin * (s->isCore ? 2.2 : 1)) { ok = false; break; } }
        if (!ok) continue;
        const double rr = jsHypot(x, y);
        const double youngness = clamp(double(G.grid.y[size_t(k)]) / (double(G.grid.l[size_t(k)]) + 1e-3), 0, 1);
        auto sys = std::make_unique<System>();
        sys->id = int(G.systems.size()); sys->x = x; sys->y = y;
        const double zg = gauss(r);
        sys->z = zg * (0.012 + 0.05 * std::exp(-rr / 0.15)) * (G.style == Style::Eliptica ? 4 : 1);
        sys->seed = uint32_t(r() * 4294967296.0); sys->young = youngness; sys->rCore = rr;
        buildSystem(*sys, G.life);
        G.systems.push_back(std::move(sys));
    }
}

// Mais sistemas numa galáxia que já existe: os antigos ficam onde estão; os novos respeitam a distância mínima.
int addSystems(Galaxy& G, int n)
{
    Rng r(uint32_t(uint64_t(G.seed) * 7u + uint64_t(G.systems.size()) * 131u + 17u));
    const int total = int(G.systems.size()) + n;
    const double dmin = std::min(0.3, 0.72 / std::sqrt(double(total)));
    int added = 0, tries = 0;
    while (added < n && tries < n * 80) {
        tries++;
        const int k = sampleCdf(G.cdfSys, r());
        double x, y;
        cellPos(G, k, r, x, y);
        const double lim = tries > n * 40 ? dmin * 0.6 : dmin;
        bool ok = true;
        for (const auto& s : G.systems) { const double dx = s->x - x, dy = s->y - y; if (dx * dx + dy * dy < lim * lim * (s->isCore ? 2.2 : 1)) { ok = false; break; } }
        if (!ok) continue;
        const double rr = jsHypot(x, y);
        auto sys = std::make_unique<System>();
        sys->id = int(G.systems.size()); sys->x = x; sys->y = y;
        const double zg = gauss(r);
        sys->z = zg * (0.012 + 0.05 * std::exp(-rr / 0.15)) * (G.style == Style::Eliptica ? 4 : 1);
        sys->seed = uint32_t(r() * 4294967296.0); sys->young = clamp(double(G.grid.y[size_t(k)]) / (double(G.grid.l[size_t(k)]) + 1e-3), 0, 1); sys->rCore = rr;
        buildSystem(*sys, G.life);
        G.systems.push_back(std::move(sys));
        added++;
    }
    if (G.mix.size() == 7 && added) {
        std::vector<System*> fresh;
        for (size_t i = G.systems.size() - size_t(added); i < G.systems.size(); i++) fresh.push_back(G.systems[i].get());
        applyMix(fresh, G.mix, (G.seed ^ 0xc1a55u) + uint32_t(G.systems.size()));
    }
    return added;
}

static DetailStars detailStars(const Galaxy& G, Rng& r, int n)
{
    DetailStars d;
    d.xs.resize(size_t(n)); d.ys.resize(size_t(n)); d.cs.resize(size_t(n)); d.bs.resize(size_t(n)); d.n = n;
    for (int s = 0; s < n; s++) {
        const int k = sampleCdf(G.cdfStars, r());
        double x, y;
        cellPos(G, k, r, x, y);
        d.xs[size_t(s)] = float(x); d.ys[size_t(s)] = float(y);
        const double young = double(G.grid.y[size_t(k)]) / (double(G.grid.l[size_t(k)]) + 1e-3);
        d.cs[size_t(s)] = uint8_t(r() < clamp(young, 0.05, 0.8) ? irand(r, 0, 1) : irand(r, 2, 4));   // 0-1 azuladas, 2-4 brancas a laranja
        d.bs[size_t(s)] = uint8_t(r() < 0.08 ? 2 : r() < 0.35 ? 1 : 0);
    }
    return d;
}

std::unique_ptr<Galaxy> makeGalaxy(const GalaxyOpts& o)
{
    const uint32_t seed = o.seed;
    Rng r(seed ^ 0x9e3779b9u);
    auto Gp = std::make_unique<Galaxy>();
    Galaxy& G = *Gp;
    G.seed = seed; G.style = o.style; G.size = o.size;
    G.arms = int(clamp(o.arms ? o.arms : 4, 2, 6));
    G.ly = sizeLy(o.size); G.starsText = sizeStars(o.size);
    G.angle = r() * TAU; G.k = uni(r, 0.95, 1.55); G.warp = uni(r, 0.6, 1.3); G.q = uni(r, 0.55, 0.85);
    G.ox = uni(r, -0.15, 0.15); G.oy = uni(r, -0.15, 0.15); G.sx = uni(r, 0, 50); G.sy = uni(r, 0, 50);
    G.ringR = uni(r, 0.5, 0.64); G.N = 300; G.life = clamp(o.life, 0, 1);
    G.count = int(clamp(jsRound(o.count ? o.count : sizeSystems(o.size)), 1, 800));
    if (o.tune.has) {
        const Tune& t = o.tune;
        if (t.ly != 0) G.ly = t.ly;
        if (!t.starsText.empty()) G.starsText = t.starsText;
        if (t.hasAngle) G.angle = t.angle;
        if (t.hasK) G.k = t.k;
        if (t.hasWarp) G.warp = t.warp;
        if (t.hasQ) G.q = t.q;
        if (t.hasOx) G.ox = t.ox;
        if (t.hasOy) G.oy = t.oy;
        if (t.hasRingR) G.ringR = t.ringR;
        G.ringW = t.ringW; G.bulgeK = t.bulgeK; G.ringAdd = t.ringAdd; G.coreK = t.coreK; G.coreR = t.coreR; G.dustK = t.dustK; G.ring2R = t.ring2R; G.spokes = t.spokes;
        G.hasBarI = t.hasBarI; G.barA = t.barA; G.barK = t.barK; G.hasJet = t.hasJet; G.jetA = t.jetA; G.jetLen = t.jetLen; G.blobs = t.blobs;
    }
    G.nz = std::make_unique<Noise>(seed);
    const int N = G.N, n = N * N;
    G.grid.l.resize(size_t(n)); G.grid.y.resize(size_t(n)); G.grid.d.resize(size_t(n)); G.grid.w.resize(size_t(n));
    FieldOut f {};
    for (int j = 0; j < N; j++) {
        const double y = EXT - ((j + 0.5) / N) * 2 * EXT;
        for (int i = 0; i < N; i++) {
            const double x = -EXT + ((i + 0.5) / N) * 2 * EXT;
            galaxyField(G, *G.nz, x, y, f);
            const size_t k = size_t(j * N + i);
            G.grid.l[k] = float(f.l); G.grid.y[k] = float(f.y); G.grid.d[k] = float(f.d); G.grid.w[k] = float(f.w);
        }
    }
    std::vector<float> wStars(static_cast<size_t>(n)), wYoung(static_cast<size_t>(n)), wSys(static_cast<size_t>(n));
    for (int k = 0; k < n; k++) {
        const int i = k % N, j = k / N;
        const double x = -EXT + ((i + 0.5) / N) * 2 * EXT, y = EXT - ((j + 0.5) / N) * 2 * EXT;
        const double rr = jsHypot(x, y);
        const double l = G.grid.l[size_t(k)], yo = G.grid.y[size_t(k)];
        wStars[size_t(k)] = float(l + yo * 0.8);
        wYoung[size_t(k)] = float(std::pow(yo, 2));
        wSys[size_t(k)] = float(rr < 0.06 ? 0 : (l * 0.6 + yo) * (rr < 0.15 ? 0.5 : 1));
    }
    G.cdfStars = makeCdf(wStars); G.cdfYoung = makeCdf(wYoung); G.cdfSys = makeCdf(wSys);
    sampleSystems(G, r);
    if (o.mix.size() == 7) {
        G.mix = o.mix;
        std::vector<System*> list;
        for (const auto& q : G.systems) list.push_back(q.get());
        applyMix(list, G.mix, seed ^ 0xc1a55u);
    }
    Rng rd(uint32_t(uint64_t(seed) * 3u + 1u));
    G.detail = detailStars(G, rd, 26000);
    return Gp;
}

// Imagem da galáxia vista de cima (RGBA T×T): luz difusa, estrelas, nebulosas, poeira.
std::vector<uint8_t> buildGalaxyImage(const Galaxy& G, int T)
{
    const int N = G.N;
    const Grid& g = G.grid;
    const size_t TT = size_t(T) * size_t(T);
    std::vector<float> buf(TT * 3, 0.f), dust(TT, 0.f);
    for (int j = 0; j < T; j++) {
        const double gy = ((j + 0.5) / T) * N - 0.5;
        for (int i = 0; i < T; i++) {
            const double gx = ((i + 0.5) / T) * N - 0.5;
            const double l = bil(g.l, N, gx, gy), yo = bil(g.y, N, gx, gy), w = bil(g.w, N, gx, gy);
            const double wf = clamp(w / (l + 1e-3), 0, 1);
            const size_t k = size_t(j * T + i) * 3;
            buf[k] = float(l * 0.6 * mix(0.8, 1.0, wf) + yo * 0.22 * 0.62);
            buf[k + 1] = float(l * 0.6 * mix(0.83, 0.82, wf) + yo * 0.22 * 0.74);
            buf[k + 2] = float(l * 0.6 * mix(0.95, 0.6, wf) + yo * 0.22 * 1.0);
            dust[size_t(j * T + i)] = float(bil(g.d, N, gx, gy));
        }
    }
    Rng r(uint32_t(uint64_t(G.seed) * 31u + 7u));
    const auto toPx = [&](double x, double y, double& px, double& py) { px = ((x + EXT) / (2 * EXT)) * T; py = ((EXT - y) / (2 * EXT)) * T; };
    const auto splat = [&](double px, double py, double rad, const V3& c, double amp) {
        const double R = std::ceil(rad * 2.5);
        const int x0 = int(std::max(0.0, std::floor(px - R))), x1 = int(std::min(T - 1.0, std::ceil(px + R)));
        const int y0 = int(std::max(0.0, std::floor(py - R))), y1 = int(std::min(T - 1.0, std::ceil(py + R)));
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            const double dx = x + 0.5 - px, dy = y + 0.5 - py;
            const double f = amp * std::exp(-(dx * dx + dy * dy) / (rad * rad));
            if (f < 0.002) continue;
            const size_t k = size_t(y * T + x) * 3;
            buf[k] += float(c[0] * f); buf[k + 1] += float(c[1] * f); buf[k + 2] += float(c[2] * f);
        }
    };
    const double scale = T / 1400.0;
    // Regiões H II: berçários rosados nos braços.
    if (G.cdfYoung.total > 0) {
        const int nh = G.style == Style::Eliptica ? 0 : 140;
        for (int s = 0; s < nh; s++) {
            const int k = sampleCdf(G.cdfYoung, r());
            double x, y, px, py;
            cellPos(G, k, r, x, y); toPx(x, y, px, py);
            const double rad = uni(r, 1.2, 4.2) * scale, amp = uni(r, 0.08, 0.32);
            splat(px, py, rad, V3 { 1.0, 0.36, 0.55 }, amp);
            if (r() < 0.5) { const double ox = uni(r, -3, 3) * scale, oy = uni(r, -3, 3) * scale, rr = uni(r, 1, 2.5) * scale, aa = uni(r, 0.05, 0.16); splat(px + ox, py + oy, rr, V3 { 0.55, 0.75, 1.0 }, aa); }
        }
        // Nebulosas grandes, recortadas pelo ruído.
        const int nn = G.style == Style::Eliptica ? 0 : irand(r, 4, 8);
        static const V3 cols[] = { { 1.0, 0.32, 0.45 }, { 0.35, 0.8, 0.9 }, { 0.6, 0.42, 1.0 }, { 1.0, 0.55, 0.3 } };
        for (int s = 0; s < nn; s++) {
            const int k = sampleCdf(G.cdfYoung, r());
            double x, y, px, py;
            cellPos(G, k, r, x, y); toPx(x, y, px, py);
            const double rad = uni(r, 10, 26) * scale;
            const V3& c = cols[irand(r, 0, 3)];
            const double amp = uni(r, 0.05, 0.12);
            const double R = std::ceil(rad * 2.4), off = uni(r, 0, 99);
            for (int yy = int(std::max(0.0, std::floor(py - R))); yy <= std::min(T - 1.0, py + R); yy++) {
                for (int xx = int(std::max(0.0, std::floor(px - R))); xx <= std::min(T - 1.0, px + R); xx++) {
                    const double dx = (xx - px) / rad, dy = (yy - py) / rad;
                    const double f0 = std::exp(-(dx * dx + dy * dy));
                    if (f0 < 0.01) continue;
                    const double m = std::max(0.0, G.nz->fbm2(dx * 1.6 + off, dy * 1.6 - off, 4) * 2 + 0.25);
                    const double f = amp * f0 * m * m;
                    const size_t k2 = size_t(yy * T + xx) * 3;
                    buf[k2] += float(c[0] * f); buf[k2 + 1] += float(c[1] * f); buf[k2 + 2] += float(c[2] * f);
                }
            }
        }
    }
    // Poeira: escurece e avermelha o que está atrás.
    for (size_t k = 0; k < TT; k++) {
        const double d = dust[k];
        if (d <= 0.001) continue;
        buf[k * 3] = float(buf[k * 3] * std::exp(-d * 1.6)); buf[k * 3 + 1] = float(buf[k * 3 + 1] * std::exp(-d * 1.95)); buf[k * 3 + 2] = float(buf[k * 3 + 2] * std::exp(-d * 2.4));
    }
    // Estrelas.
    const int nStars = int(jsRound(T * double(T) * 0.05));
    V3 hot[16], cool[16];
    for (int i = 0; i < 16; i++) {
        const auto h = tempRGB(logU(r, 8000, 30000)); hot[i] = { h[0] / 255, h[1] / 255, h[2] / 255 };
        const auto q = tempRGB(uni(r, 3000, 6200)); cool[i] = { q[0] / 255, q[1] / 255, q[2] / 255 };
    }
    for (int s = 0; s < nStars; s++) {
        const int k = sampleCdf(G.cdfStars, r());
        double x, y, px, py;
        cellPos(G, k, r, x, y); toPx(x, y, px, py);
        const double young = double(G.grid.y[size_t(k)]) / (double(G.grid.l[size_t(k)]) + 1e-3);
        const bool isHot = r() < clamp(young * 0.9, 0.04, 0.85);
        const V3& c = isHot ? hot[irand(r, 0, 15)] : cool[irand(r, 0, 15)];
        const double b0 = 0.06 + std::pow(r(), 8) * 2.2;
        const size_t di = std::min(TT - 1, size_t(std::max(0, int(py) * T + int(px))));
        const double b = b0 * std::exp(-double(dust[di]) * 0.9);
        const int ix = int(std::floor(px - 0.5)), iy = int(std::floor(py - 0.5));
        const double fx = px - 0.5 - ix, fy = py - 0.5 - iy;
        const auto put = [&](int x2, int y2, double f) {
            if (x2 < 0 || y2 < 0 || x2 >= T || y2 >= T) return;
            const size_t k2 = size_t(y2 * T + x2) * 3;
            buf[k2] += float(c[0] * f); buf[k2 + 1] += float(c[1] * f); buf[k2 + 2] += float(c[2] * f);
        };
        put(ix, iy, b * (1 - fx) * (1 - fy)); put(ix + 1, iy, b * fx * (1 - fy));
        put(ix, iy + 1, b * (1 - fx) * fy); put(ix + 1, iy + 1, b * fx * fy);
        if (b > 0.9) splat(px, py, 1.6 * scale, c, b * 0.12);
    }
    // Aglomerados globulares no halo.
    if (G.style != Style::Irregular) {
        const int ng = irand(r, 8, 16);
        for (int s = 0; s < ng; s++) {
            const double a = r() * TAU, rr = uni(r, 0.35, 1.05);
            double px, py;
            toPx(std::cos(a) * rr, std::sin(a) * rr, px, py);
            const double sig = uni(r, 2, 4.5) * scale;
            for (int q = 0; q < 160; q++) {
                const V3& c = cool[irand(r, 0, 15)];
                const double qx = px + gauss(r) * sig;
                const double qy = py + gauss(r) * sig;
                const int xi = int(qx), yi = int(qy);
                if (xi < 0 || yi < 0 || xi >= T || yi >= T) continue;
                const size_t k2 = size_t(yi * T + xi) * 3;
                const double f = uni(r, 0.08, 0.35);
                buf[k2] += float(c[0] * f); buf[k2 + 1] += float(c[1] * f); buf[k2 + 2] += float(c[2] * f);
            }
            splat(px, py, sig * 1.2, V3 { 1, 0.9, 0.75 }, 0.12);
        }
    }
    // Revelação: 1 - e^(-x), com um pouco de gama.
    std::vector<uint8_t> img(TT * 4);
    const double E = 1.7;
    const auto dev = [&](float v) { const double q = std::pow(1 - std::exp(-double(v) * E), 0.88) * 255; if (!(q > 0)) return uint8_t(0); if (q >= 255) return uint8_t(255); return uint8_t(jsRound(q)); };
    for (size_t k = 0; k < TT; k++) { img[k * 4] = dev(buf[k * 3]); img[k * 4 + 1] = dev(buf[k * 3 + 1]); img[k * 4 + 2] = dev(buf[k * 3 + 2]); img[k * 4 + 3] = 255; }
    return img;
}

// ── edição (o que o usuário muda à mão) ──
namespace {
Cls clsOfType(Type t)
{
    switch (t) { case Type::Anao: return Cls::Anao; case Type::Gigante: case Type::Quente: return Cls::Gas; case Type::GeloG: return Cls::Gelo; case Type::Subnetuno: return Cls::Sub; default: return Cls::Rocha; }
}
const char* typeAtm(Type t)
{
    switch (t) {
    case Type::Fortaleza: case Type::Casco: case Type::Forja: case Type::Arido: case Type::Anao: return "Nenhuma";
    case Type::Ecumeno: return "Filtrada, mantida por máquinas";
    case Type::Temperado: case Type::Oceano: return ATM_LIVING;
    case Type::Deserto: return "Fina, CO₂ e poeira"; case Type::Gelado: return "Fina, nitrogênio"; case Type::Lava: return "Vapor de rocha, fina";
    case Type::Toxico: return "Dióxido de carbono denso, nuvens ácidas"; case Type::Gigante: case Type::Quente: return "Hidrogênio e hélio";
    case Type::GeloG: return "Hidrogênio, hélio e metano"; case Type::Subnetuno: return "Hidrogênio espesso e vapor"; default: return "Nenhuma";
    }
}
LookCtx ctxOf(const System& sys, const Body& b)
{
    LookCtx c; c.hostT = sys.stars[0].temp; c.locked = b.locked; c.az = b.az; c.temp = b.temp != 0 ? b.temp : b.teq != 0 ? b.teq : 230;
    return c;
}
void bodyDerived(Body& b)
{
    if (b.body == BodyKind::Planeta) { b.gravity = b.mass / (b.radius * b.radius); b.density = (5.51 * b.mass) / std::pow(b.radius, 3); }
}
double livingHeightOf(const Noise& nz, const Look& L, double x, double y, double z)
{
    const double F = L.freq;
    return nz.fbm3(x * F + 3.1, y * F, z * F, 6) + 0.16 * nz.fbm3(x * 4, y * 4, z * 4, 3);
}
std::vector<float> heightSamples(const Body& b)
{
    const Noise nz(b.look.seed);
    const int n = 2400;
    std::vector<float> out(static_cast<size_t>(n));
    for (int i = 0; i < n; i++) {
        const double z = 1 - (2 * (i + 0.5)) / n, rr = std::sqrt(1 - z * z), a = i * 2.399963;
        out[size_t(i)] = float(livingHeightOf(nz, b.look, rr * std::cos(a), rr * std::sin(a), z));
    }
    std::sort(out.begin(), out.end());
    return out;
}
} // namespace

void tuneTemperature(Body& b, double tempK)
{
    b.temp = tempK;
    Look& L = b.look;
    if (!L.hasDryBase) { L.dryBase = L.dryBias; L.hasDryBase = true; }
    if (!L.hasFreq) return;
    const double tC = tempK - 273.15;
    L.cap = tC > 28 ? 9 : clamp(1.2 + (tC - 10) * 0.025, 0.05, 1.6);
    L.snowAt = clamp(0.52 + (tC - 10) * 0.007, 0.15, 0.8);
    L.dryBias = L.dryBase + clamp((tC - 15) * 0.012, -0.3, 0.45);
}
// Tipo novo: muda o visual e, se a classe mudar, massa e raio típicos dela.
void setType(System& sys, Body& b, Type type)
{
    const double pop = b.look.pop;
    const uint32_t seed = b.look.seed;
    if (b.body == BodyKind::Lua) {
        b.type = type;
        { Rng r(seed); b.look = makeLook(type, r, ctxOf(sys, b)); }
        b.look.seed = seed; b.look.pop = pop;
        if (type == Type::Temperada) { if (b.temp == 0) b.temp = 288; b.atm = ATM_LIVING; b.water = true; b.hz = Hz::Dentro; }
        else { b.atm.clear(); b.water = false; b.temp = 0; }
        b.texRev++;
        return;
    }
    const Cls cls = clsOfType(type);
    if (cls != b.cls) {
        b.cls = cls;
        switch (cls) { case Cls::Rocha: b.mass = 1; b.radius = 1; break; case Cls::Anao: b.mass = 0.002; b.radius = 0.18; break; case Cls::Gas: b.mass = 318; b.radius = 11.2; break; case Cls::Gelo: b.mass = 17; b.radius = 3.9; break; case Cls::Sub: b.mass = 6; b.radius = 2.3; break; }
    }
    b.type = type;
    b.atm = typeAtm(type);
    b.water = type == Type::Temperado || type == Type::Oceano;
    if (b.water && (b.temp < 250 || b.temp > 320)) b.temp = 288;
    if (type == Type::Lava) b.temp = std::max(b.temp, 1300.0);
    if (type == Type::Gelado) b.temp = std::min(b.temp, 200.0);
    if (type == Type::Toxico) b.temp = std::max(b.temp, 700.0);
    if (type == Type::Quente) b.temp = std::max(b.temp, 1200.0);
    { Rng r(seed); b.look = makeLook(type, r, ctxOf(sys, b)); }
    b.look.seed = seed; b.look.pop = type == Type::Ecumeno ? 1 : pop;
    if (b.water) tuneTemperature(b, b.temp);
    bodyDerived(b);
    b.texRev++;
}
// Outro visual, mesma física.
void regenLook(System& sys, Body& b, uint32_t seed)
{
    const double pop = b.look.pop;
    const LookStyle style = b.look.style;
    { Rng r(seed); b.look = makeLook(b.type, r, ctxOf(sys, b)); }
    b.look.pop = b.type == Type::Ecumeno ? 1 : pop;
    if (style == LookStyle::Olho && b.look.style != LookStyle::None) b.look.style = LookStyle::Olho;
    if (b.water || b.type == Type::Temperada) tuneTemperature(b, b.temp);
    b.texRev++;
}
void rockRestyle(System& sys, Body& b, LookStyle style) { rockLook(b.look, b.type, ctxOf(sys, b), style); b.texRev++; }
void restyle(Body& b, LookStyle style)
{
    Look& L = b.look;
    L.style = style;
    L.dryBase = style == LookStyle::Floresta ? -0.45 : style == LookStyle::Savana ? 0.25 : style == LookStyle::Tundra ? -0.1 : style == LookStyle::Pantano ? -0.3 : 0;
    L.hasDryBase = true;
    L.belt = style == LookStyle::Floresta ? 0 : 0.18;
    tuneTemperature(b, b.temp);
    b.texRev++;
}
double waterOf(const Body& b)
{
    const auto hs = heightSamples(b);
    size_t k = 0;
    while (k < hs.size() && hs[k] < b.look.sea) k++;
    return double(k) / double(hs.size());
}
void setWater(Body& b, double frac)
{
    const auto hs = heightSamples(b);
    b.look.sea = frac <= 0 ? double(hs.front()) - 1 : frac >= 1 ? double(hs.back()) + 1 : double(hs[size_t(std::floor(frac * double(hs.size() - 1)))]);
    b.texRev++;
}
void setRings(Body& b, bool on, uint32_t seed)
{
    if (!on) { b.rings.reset(); return; }
    Rng r(seed != 0 ? seed : b.look.seed + 77u);
    b.rings = makeRings(r, b.cls);
}
void reorder(System& sys)
{
    std::vector<std::unique_ptr<Body>> planets, rest;
    for (auto& b : sys.bodies) (b->body == BodyKind::Planeta ? planets : rest).push_back(std::move(b));
    std::stable_sort(planets.begin(), planets.end(), [](const std::unique_ptr<Body>& x, const std::unique_ptr<Body>& y) { return x->a < y->a; });
    for (size_t i = 0; i < planets.size(); i++) planets[i]->idx = int(i) + 1;
    sys.planetCount = int(planets.size());
    double am = std::max(planets.empty() ? 0.05 : planets.back()->a, 0.05);
    for (const Belt& b : sys.belts) am = std::max(am, b.a1);
    sys.amax = am;
    sys.bodies = std::move(planets);
    for (auto& b : rest) sys.bodies.push_back(std::move(b));
}
// Planeta novo numa órbita escolhida.
Body* addPlanet(System& sys, Type type, double a, uint32_t seed)
{
    Rng r(seed);
    const Cls cls = clsOfType(type);
    const bool hab = type == Type::Temperado || type == Type::Oceano;
    auto p = makePlanet(sys, r, a, false, hab, false, hab ? nullptr : &cls);
    if (p->type != type) setType(sys, *p, type);
    if (isArtificial(type)) { p->moons.clear(); p->rings.reset(); }
    int mx = 0;
    for (const auto& x : sys.bodies) if (x->body == BodyKind::Planeta) mx = std::max(mx, x->uid ? x->uid : x->idx);
    p->uid = mx + 1;
    Body* raw = p.get();
    sys.bodies.push_back(std::move(p));
    reorder(sys);
    return raw;
}
std::unique_ptr<Body> removeBody(System& sys, Body* b)
{
    std::unique_ptr<Body> out;
    if (b->body == BodyKind::Lua) {
        for (auto& p : sys.bodies) {
            for (size_t i = 0; i < p->moons.size(); i++) if (p->moons[i].get() == b) {
                out = std::move(p->moons[i]);
                p->moons.erase(p->moons.begin() + long(i));
                for (size_t j = 0; j < p->moons.size(); j++) p->moons[j]->idx = int(j) + 1;
                return out;
            }
        }
        return out;
    }
    for (size_t i = 0; i < sys.bodies.size(); i++) if (sys.bodies[i].get() == b) { out = std::move(sys.bodies[i]); sys.bodies.erase(sys.bodies.begin() + long(i)); break; }
    reorder(sys);
    return out;
}
void restoreBody(System& sys, std::unique_ptr<Body> b, Body* parent)
{
    if (b->body == BodyKind::Lua && parent) {
        parent->moons.push_back(std::move(b));
        std::stable_sort(parent->moons.begin(), parent->moons.end(), [](const std::unique_ptr<Body>& x, const std::unique_ptr<Body>& y) { return x->dist < y->dist; });
        for (size_t j = 0; j < parent->moons.size(); j++) parent->moons[j]->idx = int(j) + 1;
        return;
    }
    sys.bodies.push_back(std::move(b));
    reorder(sys);
}
// Lua nova, depois da última (a física fica de lado se passar do limite: é ficção).
Body* addMoon(System& sys, Body& p, Type type, uint32_t seed)
{
    Rng r(seed);
    const double RpKm = p.radius * RE_KM;
    const double last = !p.moons.empty() ? p.moons.back()->dist : RpKm * 2.4;
    const double d = last * uni(r, 1.3, 1.8);
    const double rad = type == Type::Temperada ? logU(r, 2600, 6400) : type == Type::Vulcanica ? logU(r, 900, 2200) : logU(r, 150, std::max(400.0, std::min(2600.0, RpKm * 0.25)));
    const double rho = type == Type::Gelada ? 1.9 : type == Type::Vulcanica ? 3.5 : type == Type::Temperada ? 4.8 : 3.0;
    const double mKg = rho * 1000 * (4.0 / 3) * PI_D * std::pow(rad * 1000, 3);
    const double Psec = TAU * std::sqrt(std::pow(d * 1000, 3) / (G_SI * p.mass * ME_KG));
    auto m = std::make_unique<Body>();
    m->body = BodyKind::Lua; m->type = type; m->idx = int(p.moons.size()) + 1;
    int mx = 0;
    for (const auto& x : p.moons) mx = std::max(mx, x->uid ? x->uid : x->idx);
    m->uid = mx + 1;
    m->dist = d; m->radiusKm = rad; m->massKg = mKg; m->gravity = (G_SI * mKg) / std::pow(rad * 1000, 2) / 9.81; m->P = Psec / 86400; m->M0 = r() * TAU;
    const double ft = uni(r, 0, 4), fa = r() * TAU;
    m->frame = frameOf(ft, fa);
    LookCtx ctx; ctx.hostT = sys.stars[0].temp; ctx.locked = false; ctx.temp = p.teq;
    m->look = makeLook(type, r, ctx);
    if (type == Type::Temperada) { m->temp = 288; m->atm = ATM_LIVING; m->water = true; m->hz = Hz::Dentro; }
    Body* raw = m.get();
    p.moons.push_back(std::move(m));
    return raw;
}
void massRange(StarKind kind, double& lo, double& hi)
{
    if (const ClassInfo* c = classOf(kind)) { lo = c->m[0]; hi = c->m[1]; return; }
    switch (kind) {
    case StarKind::Gigante: lo = 0.9; hi = 3; break; case StarKind::AnaBranca: lo = 0.5; hi = 1.1; break; case StarKind::Neutrons: lo = 1.3; hi = 2.1; break;
    case StarKind::BuracoNegro: lo = 5; hi = 40; break; default: lo = 0.5; hi = 2; break;
    }
}
// Estrela trocada: tipo e massa; a física do sistema inteiro acompanha.
Star starOf(StarKind kind, double mass, uint32_t seed)
{
    Rng r(seed);
    Star st = makeStar(r, kind);
    double m0, m1;
    massRange(kind, m0, m1);
    const double m = clamp(mass != 0 ? mass : st.mass, m0, m1);
    if (const ClassInfo* c = classOf(kind)) {
        const double t = clamp(std::log(m / c->m[0]) / std::log(c->m[1] / c->m[0]), 0, 1);
        const double T = mix(c->T[0], c->T[1], t), L = lumOf(m), R = std::sqrt(L) * std::pow(5778 / T, 2);
        st.mass = m; st.temp = T; st.lum = L; st.radius = R; st.color = starColor(T);
        st.spec = std::string(starKindKey(kind)) + std::to_string(int(clamp(std::floor((1 - t) * 10), 0, 9))) + " V";
        st.age = std::min(st.age, 10 * std::pow(m, -2.5) * 0.9);
    } else {
        st.mass = m;
        if (kind == StarKind::BuracoNegro) { st.radiusKm = 2.95 * m; st.radius = st.radiusKm / 696000; }
    }
    return st;
}
void refreshSystem(System& sys)
{
    sys.M = 0; sys.L = 0;
    for (const Star& s : sys.stars) { sys.M += s.mass; sys.L += s.lum; }
    const double Ls = std::max(sys.L, 1e-7);
    const StarKind k0 = sys.stars[0].kind;
    const bool remnant = k0 == StarKind::AnaBranca || k0 == StarKind::Neutrons || k0 == StarKind::BuracoNegro;
    sys.hasHz = !(remnant || k0 == StarKind::O);
    if (sys.hasHz) { sys.hz[0] = std::sqrt(Ls / 1.1); sys.hz[1] = std::sqrt(Ls / 0.53); }
    sys.hasFrost = !remnant;
    sys.frost = remnant ? 0 : 2.7 * std::sqrt(Ls);
    sys.frostEff = 2.7 * std::sqrt(std::max(Ls, 1e-4));
    sys.label = sys.stars.size() > 1 ? "Estrela dupla" : sys.stars[0].label;
    if (sys.stars.size() > 1) sys.binP = 365.25 * std::sqrt(std::pow(sys.sep, 3) / sys.M);
    for (auto& bp : sys.bodies) {
        Body& b = *bp;
        b.P = 365.25 * std::sqrt((b.a * b.a * b.a) / sys.M);
        if (b.body != BodyKind::Planeta) continue;
        const double gh = b.temp - b.teq;
        b.teq = (278.6 * std::pow(Ls, 0.25)) / std::sqrt(b.a) * std::pow(1 - b.albedo, 0.25);
        b.temp = std::max(3.0, b.teq + gh);
        b.hz = sys.hasHz ? (b.a < sys.hz[0] ? Hz::Quente : b.a > sys.hz[1] ? Hz::Fria : Hz::Dentro) : Hz::None;
    }
}
double lightLevel(double people, double radiusEarth)
{
    if (people == 0) return 0;
    const double d = people / (8e9 * radiusEarth * radiusEarth);
    return clamp(0.62 + 0.19 * std::log10(d), 0, 1);
}
// Kepler: posição na órbita (r em UA e ângulo) num instante t (dias).
void orbitPos(const Body& b, double t, double& rr, double& ang)
{
    const double M = b.M0 + (TAU * t) / b.P, e = b.e;
    double E = M;
    for (int i = 0; i < 8; i++) E -= (E - e * std::sin(E) - M) / (1 - e * std::cos(E));
    rr = b.a * (1 - e * std::cos(E));
    const double nu = 2 * std::atan2(std::sqrt(1 + e) * std::sin(E / 2), std::sqrt(1 - e) * std::cos(E / 2));
    ang = nu + b.w;
}

// ── classes de planeta ──
// A régua sai dos dados que o mundo já tem; o autor pode trocar a classe e o ar à mão.
static bool noGround(Type t) { return t == Type::Gigante || t == Type::Quente || t == Type::GeloG || t == Type::Subnetuno || t == Type::Lava || t == Type::Vulcanica; }
static bool hostileStar(StarKind k) { return k == StarKind::Neutrons || k == StarKind::BuracoNegro || k == StarKind::O || k == StarKind::B; }
static bool has(const std::string& s, const char* q) { return s.find(q) != std::string::npos; }
AirKind airOf(const Body& b)
{
    if (b.air > 0) return AirKind::Breathable;
    const std::string t = b.atm.empty() ? std::string("Nenhuma") : b.atm;
    AirKind k = has(t, "xigênio") || has(t, "apor d’água") ? AirKind::Breathable
              : has(t, "denso") || has(t, "ácida") ? AirKind::Poison
              : has(t, "enhuma") ? AirKind::None
              : has(t, "fina") || has(t, "Fina") ? AirKind::Thin : AirKind::None;
    if (b.air < 0 && k == AirKind::Breathable) k = AirKind::Thin;
    return k;
}
HabInfo habInfo(const Body& b, const System* sys, const Body* host)
{
    HabInfo out;
    if (isArtificial(b.type)) { out.why.push_back({ HabWhy::Built, 0 }); return out; }
    int n;
    if (noGround(b.type)) {
        n = 6;
        const HabWhy::Code c = b.type == Type::Gigante ? HabWhy::NoGroundGas : b.type == Type::Quente ? HabWhy::NoGroundHot : b.type == Type::GeloG ? HabWhy::NoGroundIce
                             : b.type == Type::Subnetuno ? HabWhy::NoGroundThick : b.type == Type::Lava ? HabWhy::NoGroundLava : HabWhy::NoGroundVolcano;
        out.why.push_back({ c, 0 });
    } else {
        const double T = b.temp != 0 ? b.temp : host && host->teq != 0 ? host->teq : 230, g = b.gravity, C_ = std::round(T - 273.15);
        const AirKind air = airOf(b);
        const auto heavy = [&]() { if (g > 1.4) out.why.push_back({ HabWhy::Heavy, g }); else if (g < 0.6) out.why.push_back({ HabWhy::Light, g }); };
        if (air == AirKind::Breathable) {
            out.why.push_back({ HabWhy::Breathable, 0 });
            const bool easy = T >= 268 && T <= 308, ok = T >= 245 && T <= 325, gEasy = g >= 0.6 && g <= 1.4, gOk = g >= 0.35 && g <= 2, hard = T >= 200 && T <= 360 && g <= 3;
            n = easy && gEasy ? 1 : ok && gOk ? 2 : hard ? 3 : 4;
            if (!easy) out.why.push_back({ T > 298 ? HabWhy::TooHot : HabWhy::TooCold, C_ });
            if (!gEasy) heavy();
        } else if (air == AirKind::Poison) { n = 5; out.why.push_back({ HabWhy::Poison, C_ }); }
        else {
            const bool thin = air == AirKind::Thin;
            out.why.push_back({ thin ? HabWhy::ThinAir : HabWhy::NoAir, 0 });
            n = thin && T >= 215 && T <= 345 && g >= 0.25 && g <= 2 ? 3 : T >= 90 && T <= 420 && g <= 3 ? 4 : 5;
            if (T < 215 || T > 345) out.why.push_back({ T > 345 ? HabWhy::TooHot : HabWhy::TooCold, C_ });
            if (g > 2) heavy();
        }
        if (n == 1 && b.locked) { n = 2; out.why.push_back({ HabWhy::Locked, 0 }); }
        if (n < 5 && sys && !sys->stars.empty() && hostileStar(sys->stars[0].kind)) { n++; out.why.push_back({ HabWhy::Radiation, 0 }); }
    }
    out.autoCls = n;
    out.cls = b.habSet ? b.habSet : n;
    return out;
}

// ── divisão das classes na geração ──
// Os mundos nascem normalmente e os que mais combinam com o pedido são adaptados; estrelas e órbitas não mudam.
namespace {
struct MixWorld { Body* b; System* s; Body* h; int cls; };
std::vector<MixWorld> worldsOf(const std::vector<System*>& systems)
{
    std::vector<MixWorld> out;
    for (System* s : systems) for (const auto& p : s->bodies) {
        if (p->body != BodyKind::Planeta) continue;
        out.push_back({ p.get(), s, nullptr, 0 });
        for (const auto& m : p->moons) out.push_back({ m.get(), s, p.get(), 0 });
    }
    return out;
}
bool worldAlive(const MixWorld& w)
{
    const auto inBodies = [&](const Body* q) { for (const auto& x : w.s->bodies) if (x.get() == q) return true; return false; };
    if (!w.h) return inBodies(w.b);
    if (!inBodies(w.h)) return false;
    for (const auto& m : w.h->moons) if (m.get() == w.b) return true;
    return false;
}
double moonRho(Type t) { return t == Type::Gelada ? 1.9 : t == Type::Vulcanica ? 3.5 : t == Type::Temperada ? 4.8 : 3.0; }
void moonSize(Body& m, double radKm)
{
    const double rho = moonRho(m.type) * 1000;
    m.radiusKm = radKm;
    m.massKg = rho * (4.0 / 3) * PI_D * std::pow(radKm * 1000, 3);
    m.gravity = (G_SI * m.massKg) / std::pow(radKm * 1000, 2) / 9.81;
}
void setGravityIn(Body& b, double lo, double hi, Rng& r)
{
    if (b.gravity >= lo && b.gravity <= hi) return;
    // Lua tem limite de tamanho: a gravidade fica onde esse tamanho deixa.
    if (b.body == BodyKind::Lua) {
        const double kmPerG = 9.81 / ((4.0 / 3) * PI_D * G_SI * moonRho(b.type) * 1000) / 1000;
        const double a = std::max(lo, 0.02) * kmPerG, z = std::min(hi * kmPerG, 6800.0);
        moonSize(b, a >= z ? a : uni(r, a, z));
        return;
    }
    const double g = uni(r, std::max(lo, 0.05), hi);
    b.mass = std::pow(g, 1 / 0.46); b.radius = std::pow(b.mass, 0.27);
    bodyDerived(b);
}
// Primeiro os mundos sem chão, porque trocar um gigante por um rochoso muda quantas luas existem.
void balanceNoGround(const std::vector<System*>& systems, double share, Rng& r)
{
    std::vector<std::unique_ptr<Body>> dead;   // o que saiu fica guardado até o fim, pra nenhum endereço voltar a valer
    const auto swapPlanet = [&](MixWorld& w, Cls cls, int& total, int& cur) {
        const Body* old = w.b;
        total -= 1 + int(old->moons.size());
        cur -= noGround(old->type) ? 1 : 0;
        for (const auto& m : old->moons) cur -= noGround(m->type) ? 1 : 0;
        auto np = makePlanet(*w.s, r, old->a, false, false, false, &cls);
        np->idx = old->idx; np->uid = old->uid;
        if (cls == Cls::Rocha && noGround(np->type)) setType(*w.s, *np, Type::Arido);
        total += 1 + int(np->moons.size());
        cur += noGround(np->type) ? 1 : 0;
        for (const auto& m : np->moons) cur += noGround(m->type) ? 1 : 0;
        for (auto& x : w.s->bodies) if (x.get() == old) { dead.push_back(std::move(x)); x = std::move(np); break; }
    };
    struct Ranked { MixWorld w; double k; };
    for (int pass = 0; pass < 8; pass++) {
        std::vector<MixWorld> all = worldsOf(systems);
        int total = int(all.size()), cur = 0;
        for (const MixWorld& w : all) if (noGround(w.b->type)) cur++;
        const auto want = [&]() { return int(std::floor(share * total + 0.5)); };
        if (cur == want()) return;
        std::vector<Ranked> pool;
        if (cur > want()) {
            for (const MixWorld& w : all) if (noGround(w.b->type)) pool.push_back({ w, (w.h ? 0 : w.b->type == Type::Lava ? 1 : 2) + r() * 0.5 });
            std::stable_sort(pool.begin(), pool.end(), [](const Ranked& x, const Ranked& y) { return x.k < y.k; });
            for (Ranked& q : pool) {
                if (cur <= want()) break;
                if (!worldAlive(q.w)) continue;
                if (q.w.h) { setType(*q.w.s, *q.w.b, Type::Rochosa); cur--; }
                else if (q.w.b->type == Type::Lava) { setType(*q.w.s, *q.w.b, Type::Arido); cur--; }
                else swapPlanet(q.w, Cls::Rocha, total, cur);
            }
        } else {
            const auto rank = [](const MixWorld& w) {
                if (w.h) return w.h->cls == Cls::Gas || w.h->cls == Cls::Gelo ? 1 : 3;
                if (w.b->teq > 600) return 0;
                return w.b->a > w.s->frostEff ? 2 : 4;
            };
            for (const MixWorld& w : all) if (!noGround(w.b->type)) pool.push_back({ w, rank(w) + r() * 0.5 });
            std::stable_sort(pool.begin(), pool.end(), [](const Ranked& x, const Ranked& y) { return x.k < y.k; });
            for (Ranked& q : pool) {
                if (cur >= want()) break;
                if (!worldAlive(q.w)) continue;
                if (q.w.h) { setType(*q.w.s, *q.w.b, Type::Vulcanica); cur++; }
                else if (q.w.b->teq > 600) { setType(*q.w.s, *q.w.b, Type::Lava); cur++; }
                else { const Cls c = q.w.b->a > q.w.s->frostEff * 4 && r() < 0.5 ? Cls::Gelo : Cls::Gas; swapPlanet(q.w, c, total, cur); }
            }
        }
    }
}
// Quão longe um mundo está de uma classe: quem está mais perto é adaptado primeiro.
double mixFit(const MixWorld& w, int c, Rng& r)
{
    const Body& b = *w.b;
    const double T = b.temp != 0 ? b.temp : w.h && w.h->teq != 0 ? w.h->teq : 230, base = w.h ? w.h->teq : b.teq, g = b.gravity != 0 ? b.gravity : 0.01;
    double k = r() * 0.15;
    if (c <= 3) {
        k += std::fabs(base - (c == 3 ? 250 : 255)) / 120 + std::fabs(std::log(g)) * 0.3;
        if (w.h) k += w.h->cls == Cls::Gas || w.h->cls == Cls::Gelo ? 0.4 : w.h->radius * RE_KM < 7000 ? 2 : 1;
        else if (b.cls == Cls::Anao) k += 0.8;
        if (c == 1 && b.locked) k += 0.5;
    } else if (c == 4) k += T < 90 ? (90 - T) / 200 : T > 420 ? (T - 420) / 200 : 0;
    else k += T < 90 || T > 420 ? 0 : 0.2 + std::fabs(base - 330) / 200;
    return k;
}
// Adapta um mundo pra uma classe e devolve a classe que ele ficou de fato.
int shapeWorld(const MixWorld& w, int c, Rng& r)
{
    Body& b = *w.b;
    System& sys = *w.s;
    const bool moon = b.body == BodyKind::Lua;
    const double T0 = b.temp != 0 ? b.temp : w.h && w.h->teq != 0 ? w.h->teq : 230;
    const int base = hostileStar(sys.stars[0].kind) && c > 1 ? c - 1 : c;
    const auto into = [&](double lo, double hi) { return T0 >= lo && T0 <= hi ? T0 : uni(r, lo, hi); };
    const auto living = [&](double T) {
        if (moon) setType(sys, b, Type::Temperada);
        else setType(sys, b, r() < 0.35 ? Type::Oceano : Type::Temperado);
        tuneTemperature(b, T);
    };
    const auto dry = [&](double T, const char* atm, bool cold) {
        if (moon) {
            const Type want = T < 160 ? Type::Gelada : Type::Rochosa;
            setType(sys, b, atm ? Type::Rochosa : b.type == Type::Gelada || b.type == Type::Rochosa ? b.type : want);
            b.atm = atm ? atm : "";
            b.temp = T;
        } else {
            const bool keepDwarf = b.cls == Cls::Anao && !atm;
            if (!keepDwarf) setType(sys, b, atm && std::string(atm) == typeAtm(Type::Toxico) ? Type::Toxico : cold ? Type::Gelado : atm ? Type::Deserto : Type::Arido);
            b.atm = atm ? atm : keepDwarf || !cold ? "Nenhuma" : typeAtm(Type::Gelado);
            b.temp = T;
        }
    };
    if (base == 1) {
        if (!moon && b.locked) { b.locked = false; b.dayH = logU(r, 9, 140); }
        const double T = into(272, 304);
        living(T);
        setGravityIn(b, 0.62, 1.38, r);
    } else if (base == 2) {
        if (!moon && b.locked) { const double T = into(272, 304); living(T); setGravityIn(b, 0.62, 1.38, r); }
        else {
            const double g = b.gravity;
            const bool off = (g >= 0.36 && g < 0.59) || (g > 1.41 && g <= 1.98);
            const double T = off ? into(246, 324) : T0 >= 288 ? uni(r, 310, 324) : uni(r, 246, 266);
            living(T);
            setGravityIn(b, 0.38, 1.95, r);
        }
    } else if (base == 3) {
        const double T = into(218, 342);
        dry(T, T < 250 ? typeAtm(Type::Gelado) : "Fina, CO₂", T < 250);
        setGravityIn(b, 0.27, 1.95, r);
    } else if (base == 4) {
        const double T = into(95, 415);
        dry(T, nullptr, false);
        setGravityIn(b, 0.01, 2.9, r);
    } else if (T0 < 120) { const double T = uni(r, 40, 88); dry(T, nullptr, false); }
    else if (T0 > 420) dry(T0, nullptr, false);
    else { const double T = moon ? uni(r, 480, 740) : std::max(T0, 700.0); dry(T, typeAtm(Type::Toxico), false); }
    b.texRev++;
    return habInfo(b, &sys, w.h).cls;
}
} // namespace
void applyMix(const std::vector<System*>& systems, const std::vector<double>& mix, uint32_t seed)
{
    if (mix.size() != 7) return;
    double sum = 0;
    for (int i = 1; i <= 6; i++) sum += std::max(0.0, mix[size_t(i)]);
    if (!(sum > 0)) return;
    Rng r(seed);
    double share[7] { 0, 0, 0, 0, 0, 0, 0 };
    for (int i = 1; i <= 6; i++) share[i] = std::max(0.0, mix[size_t(i)]) / sum;
    balanceNoGround(systems, share[6], r);
    std::vector<MixWorld> all = worldsOf(systems);
    if (all.empty()) return;
    int cur[7] { 0, 0, 0, 0, 0, 0, 0 };
    for (MixWorld& w : all) { w.cls = habInfo(*w.b, w.s, w.h).cls; cur[w.cls]++; }
    // A conta exata de cada classe: o que sobrar do arredondamento vai pra quem chegou mais perto.
    const int rest = int(all.size()) - cur[6];
    double sum5 = 0;
    for (int c = 1; c <= 5; c++) sum5 += share[c];
    if (sum5 == 0) sum5 = 1;
    int want[7] { 0, 0, 0, 0, 0, 0, cur[6] };
    std::vector<std::pair<double, int>> frac;
    int left = rest;
    for (int c = 1; c <= 5; c++) { const double x = (rest * share[c]) / sum5; want[c] = int(std::floor(x)); left -= want[c]; frac.push_back({ x - want[c], c }); }
    std::stable_sort(frac.begin(), frac.end(), [](const std::pair<double, int>& x, const std::pair<double, int>& y) { return x.first > y.first; });
    for (int i = 0; i < left; i++) want[frac[size_t(i % 5)].second]++;
    const auto shapeTo = [&](MixWorld& w, int c) { const int got = shapeWorld(w, c, r); cur[w.cls]--; cur[got]++; w.cls = got; return got == c; };
    for (int c = 1; c <= 5; c++) {
        if (cur[c] >= want[c]) continue;
        struct Ranked { MixWorld* w; double k; };
        std::vector<Ranked> pool;
        for (MixWorld& w : all) if (w.cls != c && w.cls != 6 && cur[w.cls] > want[w.cls] && !(c == 1 && hostileStar(w.s->stars[0].kind))) pool.push_back({ &w, mixFit(w, c, r) });
        std::stable_sort(pool.begin(), pool.end(), [](const Ranked& x, const Ranked& y) { return x.k < y.k; });
        for (const Ranked& q : pool) {
            if (cur[c] >= want[c]) break;
            if (cur[q.w->cls] > want[q.w->cls]) shapeTo(*q.w, c);
        }
    }
    // Pedido que a régua não alcança (Éden em volta de estrela hostil): o pedido do autor manda.
    for (int c = 1; c <= 5; c++) {
        if (cur[c] >= want[c]) continue;
        for (MixWorld& w : all) {
            if (cur[c] >= want[c]) break;
            if (w.cls == c || w.cls == 6 || cur[w.cls] <= want[w.cls]) continue;
            if (!shapeTo(w, c)) { cur[w.cls]--; cur[c]++; w.cls = c; w.b->habSet = c; }
        }
    }
}

} // namespace galaxy
