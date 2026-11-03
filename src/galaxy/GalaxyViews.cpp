#include "GalaxyViews.h"

#include <algorithm>
#include <map>

namespace galaxy {

namespace {
// A escala do Topográfico é a do Mapa, do verde baixo ao branco do pico.
const Rgb TOPO[9] = { { 166, 198, 136 }, { 184, 212, 146 }, { 206, 222, 158 }, { 228, 222, 166 }, { 222, 198, 146 }, { 202, 168, 122 }, { 180, 144, 110 }, { 200, 188, 176 }, { 246, 246, 244 } };
const double TOPO_LIM[9] = { 0.035, 0.07, 0.11, 0.17, 0.25, 0.35, 0.47, 0.62, 0.8 };

struct GradStop { double t; Rgb c; };
std::shared_ptr<const std::array<uint8_t, 768>> gradOf(const std::vector<GradStop>& stops)
{
    auto t = std::make_shared<std::array<uint8_t, 768>>();
    for (int i = 0; i < 256; i++) {
        const double u = i / 255.0;
        size_t k = 0;
        while (k + 2 < stops.size() && u > stops[k + 1].t) k++;
        const GradStop& a = stops[k];
        const GradStop& b = stops[k + 1];
        const double m = clamp((u - a.t) / (b.t - a.t), 0, 1);
        for (int c = 0; c < 3; c++) (*t)[size_t(i * 3 + c)] = uint8_t(a.c[size_t(c)] + (b.c[size_t(c)] - a.c[size_t(c)]) * m);
    }
    return t;
}
// As vistas nascidas do espaço: cada degradê diz o que a cor significa nessa vista.
void addSpaceSkins(std::map<QString, Skin>& m)
{
    const auto base = [](const char* id) { Skin s; s.id = QString::fromLatin1(id); s.space = true; s.hatch = false; s.grain = 1; s.levels = 1; s.labelPlus = 1; return s; };
    {
        Skin s = base("holo"); s.paper = { 2, 9, 18 }; s.ink = { 108, 226, 248 }; s.sel = { 255, 214, 120 }; s.hz = { 150, 255, 196 }; s.dark = { 4, 22, 36 }; s.kind = SkinKind::Lut;
        s.grad = gradOf({ { 0, { 2, 10, 20 } }, { 0.28, { 8, 64, 96 } }, { 0.62, { 34, 170, 208 } }, { 1, { 190, 248, 255 } } }); s.gain = 1.25; s.gamma = 0.75; s.scan = 3; s.wash = WashKind::Holo; s.photo = true; s.stipN = 22000; s.stipA = 0.5; s.stipP = 1.15;
        s.paint = SpacePaint::Holo; s.sign = StarSign::Holo; s.darkPaper = true; s.hasSun = true; s.sun = { 8, 44, 66 }; s.hasShadow = true; s.shadow = { 0, 6, 14 }; s.face.clear(); s.halo = s.paper; m[s.id] = s;
    }
    {
        Skin s = base("routes"); s.paper = { 245, 242, 233 }; s.ink = { 26, 30, 42 }; s.sel = { 214, 40, 40 }; s.hz = { 0, 132, 61 }; s.kind = SkinKind::Poster; s.levels = 4; s.wash = WashKind::Routes; s.paint = SpacePaint::Routes; s.sign = StarSign::Station; s.colored = true;
        s.hasSun = true; s.sun = { 236, 158, 40 }; s.hasShadow = true; s.shadow = { 30, 34, 48 }; s.face.clear(); s.grain = 2; s.halo = s.paper; m[s.id] = s;
    }
    {
        Skin s = base("domains"); s.paper = { 11, 14, 22 }; s.ink = { 226, 232, 242 }; s.sel = { 255, 214, 120 }; s.hz = { 150, 230, 170 }; s.dark = { 24, 30, 44 }; s.kind = SkinKind::Poster; s.levels = 5; s.paint = SpacePaint::Domains; s.sign = StarSign::Domain; s.darkPaper = true; s.colored = true;
        s.noTerr = true; s.regionPx = 21; s.regionCenter = true; s.hasShadow = true; s.shadow = { 2, 3, 8 }; s.face.clear(); s.halo = s.paper; m[s.id] = s;
    }
    {
        Skin s = base("gravity"); s.paper = { 5, 7, 20 }; s.ink = { 128, 170, 255 }; s.sel = { 255, 184, 108 }; s.hz = { 130, 240, 190 }; s.dark = { 14, 20, 54 }; s.kind = SkinKind::Lut;
        s.grad = gradOf({ { 0, { 5, 7, 20 } }, { 0.42, { 40, 58, 142 } }, { 0.78, { 128, 170, 255 } }, { 1, { 240, 246, 255 } } }); s.gain = 1.2; s.gamma = 0.8; s.paint = SpacePaint::Gravity; s.sign = StarSign::Grav; s.darkPaper = true;
        s.hasSun = true; s.sun = { 226, 234, 255 }; s.hasShadow = true; s.shadow = { 2, 3, 10 }; s.face.clear(); s.halo = s.paper; m[s.id] = s;
    }
    {
        Skin s = base("harmony"); s.paper = { 10, 22, 58 }; s.ink = { 233, 201, 132 }; s.sel = { 255, 236, 190 }; s.hz = { 176, 216, 170 }; s.dark = { 14, 28, 68 }; s.kind = SkinKind::Lut;
        s.grad = gradOf({ { 0, { 10, 22, 58 } }, { 0.3, { 44, 54, 86 } }, { 0.62, { 164, 126, 62 } }, { 1, { 250, 228, 170 } } }); s.gain = 1.25; s.gamma = 0.8; s.wash = WashKind::Harmony; s.photo = true; s.stipN = 16000; s.stipA = 0.42; s.stipP = 1.25;
        s.paint = SpacePaint::Harmony; s.sign = StarSign::Engraved; s.darkPaper = true; s.hasSun = true; s.sun = { 233, 201, 132 }; s.hasShadow = true; s.shadow = { 4, 10, 34 }; s.grain = 3; s.halo = s.paper; m[s.id] = s;
    }
    {
        Skin s = base("thermal"); s.paper = { 0, 0, 4 }; s.ink = { 252, 226, 170 }; s.sel = { 120, 240, 255 }; s.hz = { 120, 240, 255 }; s.dark = { 40, 11, 84 }; s.kind = SkinKind::Lut;
        s.grad = gradOf({ { 0, { 0, 0, 4 } }, { 0.16, { 40, 11, 84 } }, { 0.34, { 101, 21, 110 } }, { 0.5, { 159, 42, 99 } }, { 0.66, { 212, 72, 66 } }, { 0.8, { 245, 125, 21 } }, { 0.92, { 250, 193, 39 } }, { 1, { 252, 255, 164 } } });
        s.gain = 1.1; s.gamma = 0.9; s.wash = WashKind::Thermal; s.photo = true; s.paint = SpacePaint::Thermal; s.sign = StarSign::Thermal; s.darkPaper = true; s.thermal = true; s.hasSun = true; s.sun = { 252, 240, 180 }; s.shadeNone = true; s.face = QStringLiteral("IBM Plex Mono"); s.grain = 0; s.halo = s.paper; m[s.id] = s;
    }
    {
        Skin s = base("xray"); s.paper = { 1, 3, 12 }; s.ink = { 176, 216, 250 }; s.sel = { 255, 150, 110 }; s.hz = { 176, 216, 250 }; s.dark = { 10, 34, 92 }; s.kind = SkinKind::Lut;
        s.grad = gradOf({ { 0, { 1, 3, 12 } }, { 0.3, { 10, 34, 92 } }, { 0.6, { 36, 96, 196 } }, { 0.85, { 130, 196, 250 } }, { 1, { 246, 252, 255 } } }); s.gain = 0.72; s.gamma = 1.25; s.wash = WashKind::Xray; s.photo = true; s.sign = StarSign::Xray; s.darkPaper = true;
        s.hasSun = true; s.sun = { 226, 244, 255 }; s.hasShadow = true; s.shadow = { 0, 0, 6 }; s.face = QStringLiteral("IBM Plex Mono"); s.grain = 0; s.halo = s.paper; m[s.id] = s;
    }
    {
        Skin s = base("plate"); s.paper = { 228, 229, 221 }; s.ink = { 24, 26, 32 }; s.sel = { 180, 40, 36 }; s.hz = { 32, 44, 110 }; s.dark = { 150, 152, 150 }; s.kind = SkinKind::Lut;
        s.grad = gradOf({ { 0, { 232, 233, 226 } }, { 0.45, { 150, 152, 150 } }, { 1, { 14, 15, 20 } } }); s.gain = 1.15; s.gamma = 0.85; s.wash = WashKind::Plate; s.photo = true; s.paint = SpacePaint::Plate; s.sign = StarSign::Plate;
        s.hasShadow = true; s.shadow = { 240, 241, 234 }; s.hasSun = true; s.sun = { 18, 20, 26 }; s.hasLabelInk = true; s.labelInk = { 32, 44, 110 }; s.face = QStringLiteral("Caveat"); s.plainWeight = true; s.labelPlus = 4; s.grain = 3; s.halo = s.paper; m[s.id] = s;
    }
}

std::map<QString, Skin> makeSkins()
{
    std::map<QString, Skin> m;
    addSpaceSkins(m);
    {
        Skin s; s.id = QStringLiteral("ink"); s.paper = { 240, 231, 208 }; s.ink = { 40, 33, 28 }; s.dark = { 40, 33, 28 }; s.light = { 240, 231, 208 }; s.levels = 1; s.gain = 1.3; s.gamma = 0.8; s.sel = { 168, 52, 36 }; s.hz = { 40, 33, 28 }; s.mono = true; s.grain = 3;
        s.curves = { { 0.15, 1.15, 0.85, false }, { 0.1, 0.9, 0.5, false }, { 0.065, 0.8, 0.3, false }, { 0.04, 0.7, 0.18, false } }; s.stipN = 52000; s.stipA = 0.72; s.stipP = 1.2; s.halo = s.paper; m[s.id] = s;
    }
    {
        Skin s; s.id = QStringLiteral("parch"); s.paper = { 234, 214, 174 }; s.ink = { 74, 48, 30 }; s.dark = { 74, 48, 30 }; s.light = { 240, 224, 188 }; s.levels = 3; s.gain = 1.2; s.gamma = 0.85; s.sel = { 150, 44, 28 }; s.hz = { 86, 110, 60 }; s.wash = WashKind::Parch; s.grain = 5; s.vignette = true;
        s.curves = { { 0.15, 1.1, 0.8, false }, { 0.09, 0.8, 0.34, false } }; s.stipN = 26000; s.stipA = 0.55; s.stipP = 1.3; s.halo = s.paper; m[s.id] = s;
    }
    {
        Skin s; s.id = QStringLiteral("chart"); s.paper = { 238, 226, 196 }; s.ink = { 62, 48, 36 }; s.dark = { 62, 48, 36 }; s.light = { 242, 232, 206 }; s.levels = 3; s.gain = 1.2; s.gamma = 0.85; s.sel = { 164, 52, 40 }; s.hz = { 56, 110, 110 }; s.wash = WashKind::Chart; s.grain = 3; s.chart = true; s.graticule = true;
        s.curves = { { 0.15, 1, 0.7, false }, { 0.08, 0.7, 0.3, true } }; s.stipN = 14000; s.stipA = 0.42; s.stipP = 1.4; s.halo = s.paper; m[s.id] = s;
    }
    {
        Skin s; s.id = QStringLiteral("topo"); s.paper = { 196, 216, 226 }; s.halo = { 242, 238, 216 }; s.ink = { 84, 58, 38 }; s.kind = SkinKind::Bands; s.sel = { 168, 40, 30 }; s.hz = { 40, 110, 70 }; s.hatch = false; s.wash = WashKind::Topo; s.grain = 1; s.colored = true;
        for (int i = 0; i < 9; i++) s.curves.push_back({ TOPO_LIM[i], i % 3 == 2 ? 1.05 : 0.6, i % 3 == 2 ? 0.75 : 0.45, false });
        m[s.id] = s;
    }
    {
        Skin s; s.id = QStringLiteral("aqua"); s.paper = { 246, 242, 232 }; s.ink = { 70, 78, 96 }; s.kind = SkinKind::Water; s.sel = { 176, 66, 74 }; s.hz = { 84, 150, 116 }; s.hatch = false; s.wash = WashKind::Aqua; s.grain = 6; s.colored = true; s.soft = true; s.halo = s.paper; m[s.id] = s;
    }
    {
        Skin s; s.id = QStringLiteral("blue"); s.paper = { 40, 80, 136 }; s.ink = { 232, 240, 250 }; s.dark = { 26, 54, 98 }; s.light = { 232, 240, 250 }; s.levels = 2; s.gain = 1.35; s.gamma = 0.8; s.sel = { 255, 214, 120 }; s.hz = { 232, 240, 250 }; s.mono = true; s.wash = WashKind::Blue; s.grain = 2; s.graticule = true; s.darkPaper = true;
        s.curves = { { 0.05, 0.7, 0.34, false }, { 0.11, 0.75, 0.46, false }, { 0.2, 0.85, 0.6, false }, { 0.34, 0.9, 0.72, false }, { 0.52, 0.95, 0.85, false }, { 0.74, 1, 0.95, false } }; s.stipN = 9000; s.stipA = 0.42; s.stipP = 1.4; s.halo = s.paper; m[s.id] = s;
    }
    return m;
}
const std::array<float, 64>& bayer()
{
    static const std::array<float, 64> b = [] {
        std::vector<int> m { 0 };
        for (int n = 1; n < 8; n *= 2) {
            const int N = n * 2;
            std::vector<int> o(size_t(n * n * 4));
            for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) {
                const int v = m[size_t(y * n + x)] * 4;
                o[size_t(y * N + x)] = v; o[size_t(y * N + x + n)] = v + 2; o[size_t((y + n) * N + x)] = v + 3; o[size_t((y + n) * N + x + n)] = v + 1;
            }
            m = o;
        }
        std::array<float, 64> r {};
        for (size_t i = 0; i < 64; i++) r[i] = float((m[i] + 0.5) / 64);
        return r;
    }();
    return b;
}
const std::array<float, 256>& lutOf(double gain, double gamma, double floorV)
{
    static std::map<std::array<double, 3>, std::array<float, 256>> luts;
    const std::array<double, 3> k { gain, gamma, floorV };
    auto it = luts.find(k);
    if (it != luts.end()) return it->second;
    std::array<float, 256> t {};
    for (int i = 0; i < 256; i++) t[size_t(i)] = float(std::min(1.0, floorV + (1 - floorV) * std::pow((i / 255.0) * gain, gamma)));
    return luts.emplace(k, t).first->second;
}
inline std::array<double, 3> mix3(const std::array<double, 3>& a, const std::array<double, 3>& b, double t) { return { a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t }; }
inline double sampleF(const Field& F, double u, double v)
{
    const int N = F.N;
    const double x = std::max(0.0, std::min(N - 1.001, u * N - 0.5)), y = std::max(0.0, std::min(N - 1.001, v * N - 0.5));
    const int i = int(x), j = int(y);
    const double fx = x - i, fy = y - j;
    const size_t o = size_t(j * N + i);
    const float* D = F.D.data();
    return (double(D[o]) * (1 - fx) + double(D[o + 1]) * fx) * (1 - fy) + (double(D[o + size_t(N)]) * (1 - fx) + double(D[o + size_t(N) + 1]) * fx) * fy;
}
} // namespace

SoftNoise::SoftNoise(uint32_t seed) : t(size_t(256 * 256))
{
    uint32_t h = seed ^ 0x5bd1e995u;
    for (auto& v : t) { h = (h ^ (h >> 15)) * 0x2c1b3c6du; h ^= h >> 13; v = float(double(h) / 4294967296.0); }
}
double SoftNoise::at(double x, double y) const
{
    const int X = int(std::floor(x)), Y = int(std::floor(y));
    const double fx = x - X, fy = y - Y, ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
    const int a = X & 255, b = (X + 1) & 255, c = (Y & 255) * 256, d = ((Y + 1) & 255) * 256;
    return (double(t[size_t(c + a)]) * (1 - ux) + double(t[size_t(c + b)]) * ux) * (1 - uy) + (double(t[size_t(d + a)]) * (1 - ux) + double(t[size_t(d + b)]) * ux) * uy;
}
double sampleField(const Field& F, double u, double v) { return sampleF(F, u, v); }

const Skin* skinOf(const QString& id)
{
    static const std::map<QString, Skin> m = makeSkins();
    auto it = m.find(id);
    return it == m.end() ? nullptr : &it->second;
}

void stylize(uint8_t* d, int w, int h, const Skin& sk, bool outline, double floorV, double heat)
{
    const auto& lut = lutOf(sk.gain, sk.gamma, floorV);
    const auto& B = bayer();
    const size_t n = size_t(w) * size_t(h) * 4;
    const Rgb inkC = sk.ink;
    if (sk.kind == SkinKind::Lut) {
        // Degradê: a claridade vira a cor da vista. Na térmica, a temperatura do mundo pesa mais que a luz.
        const auto& G = *sk.grad;
        const int scan = sk.scan;
        size_t i = 0;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++, i += 4) {
            if (d[i + 3] < 8) continue;
            float L = lut[size_t((d[i] * 77 + d[i + 1] * 150 + d[i + 2] * 29) >> 8)];
            if (heat >= 0) L = float(heat * (0.62 + L * 0.42));
            if (scan && y % scan == 0) L *= 0.62f;
            const size_t k = size_t(std::min(255, int(L * 255))) * 3;
            d[i] = G[k]; d[i + 1] = G[k + 1]; d[i + 2] = G[k + 2];
        }
    } else if (sk.kind == SkinKind::Poster) {
        // Cor chapada: a cor do mundo fica, a luz vira poucas faixas.
        const int lv = sk.levels;
        for (size_t i = 0; i < n; i += 4) {
            if (d[i + 3] < 8) continue;
            const double L = std::max(0.09, (d[i] * 77 + d[i + 1] * 150 + d[i + 2] * 29) / 65280.0), q = (std::floor(std::min(0.999, L * 1.35) * lv) + 0.65) / lv, f = q / L;
            d[i] = uint8_t(std::min(255.0, d[i] * f)); d[i + 1] = uint8_t(std::min(255.0, d[i + 1] * f)); d[i + 2] = uint8_t(std::min(255.0, d[i + 2] * f));
        }
    } else if (sk.kind == SkinKind::Dither) {
        const Rgb D = sk.dark, Li = sk.light;
        const int lv = sk.levels;
        size_t i = 0;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++, i += 4) {
            if (d[i + 3] < 8) continue;
            const float L = lut[size_t((d[i] * 77 + d[i + 1] * 150 + d[i + 2] * 29) >> 8)];
            const double q = double(std::min(lv, int(std::floor(L * lv + B[size_t(((y & 7) << 3) | (x & 7))])))) / lv;
            d[i] = uint8_t(D[0] + (Li[0] - D[0]) * q); d[i + 1] = uint8_t(D[1] + (Li[1] - D[1]) * q); d[i + 2] = uint8_t(D[2] + (Li[2] - D[2]) * q);
        }
    } else if (sk.kind == SkinKind::Bands) {
        for (size_t i = 0; i < n; i += 4) {
            if (d[i + 3] < 8) continue;
            const Rgb& c = TOPO[std::min(8, int(std::floor(lut[size_t((d[i] * 77 + d[i + 1] * 150 + d[i + 2] * 29) >> 8)] * 6)) + 1)];
            d[i] = uint8_t(c[0]); d[i + 1] = uint8_t(c[1]); d[i + 2] = uint8_t(c[2]);
        }
        // Onde a faixa muda, a curva de nível.
        for (int y = 0; y < h - 1; y++) {
            size_t i = size_t(y) * size_t(w) * 4;
            for (int x = 0; x < w - 1; x++, i += 4) {
                const size_t j = i + 4, k = i + size_t(w) * 4;
                if (d[i + 3] > 100 && d[j + 3] > 100 && d[k + 3] > 100 && (d[i] != d[j] || d[i] != d[k])) { d[i] = uint8_t(inkC[0]); d[i + 1] = uint8_t(inkC[1]); d[i + 2] = uint8_t(inkC[2]); }
            }
        }
    } else {
        const Rgb P = sk.paper;
        const double SH[3] = { 122, 122, 176 };
        for (size_t i = 0; i < n; i += 4) {
            if (d[i + 3] < 8) continue;
            // Aguada: a sombra é violeta rala, nunca preta, e a tinta fica em camadas de borda limpa.
            const double L = ((d[i] * 77 + d[i + 1] * 150 + d[i + 2] * 29) >> 8) / 255.0, t = std::min(1.0, L / 0.34), up = 0.86 / std::max(0.3, L), k = std::floor((0.34 + 0.46 * t) * 7 + 0.5) / 7;
            for (int c = 0; c < 3; c++) { const double pig = SH[c] + (std::min(255.0, d[i + size_t(c)] * up) - SH[c]) * t; d[i + size_t(c)] = uint8_t(clamp(P[size_t(c)] + (pig - P[size_t(c)]) * k, 0, 255)); }
        }
    }
    if (outline) {
        const int W = w * 4;
        const std::array<double, 3> ink3 { double(inkC[0]), double(inkC[1]), double(inkC[2]) }, pap3 { double(sk.paper[0]), double(sk.paper[1]), double(sk.paper[2]) };
        const auto Ci = sk.soft ? mix3(ink3, pap3, 0.2) : ink3;
        for (int y = 1; y < h - 1; y++) {
            const int r = y * W;
            for (int x = 1; x < w - 1; x++) {
                const int i = r + x * 4;
                if (d[i + 3] > 100 && (d[i - 1] < 60 || d[i + 7] < 60 || d[i - W + 3] < 60 || d[i + W + 3] < 60)) { d[i] = uint8_t(Ci[0]); d[i + 1] = uint8_t(Ci[1]); d[i + 2] = uint8_t(Ci[2]); d[i + 3] = 255; }
            }
        }
    }
}

// ── a galáxia vira densidade ──
Field densityField(const QImage& gal, int N)
{
    const QImage c = gal.scaled(N, N, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);
    Field F;
    F.N = N;
    F.D.resize(size_t(N) * size_t(N));
    std::vector<float>& D = F.D;
    for (int j = 0; j < N; j++) {
        const uchar* row = c.constScanLine(j);
        for (int i = 0; i < N; i++) D[size_t(j * N + i)] = float(std::pow((row[i * 4] * 0.3 + row[i * 4 + 1] * 0.59 + row[i * 4 + 2] * 0.11) / 255.0, 0.62));
    }
    std::vector<float> T(D.size());
    const auto box = [&](int R) {
        for (int y = 0; y < N; y++) {
            double s = 0;
            for (int x = -R; x <= R; x++) s += D[size_t(y * N + std::max(0, std::min(N - 1, x)))];
            for (int x = 0; x < N; x++) { T[size_t(y * N + x)] = float(s / (2 * R + 1)); s += D[size_t(y * N + std::min(N - 1, x + R + 1))] - D[size_t(y * N + std::max(0, x - R))]; }
        }
        for (int x = 0; x < N; x++) {
            double s = 0;
            for (int y = -R; y <= R; y++) s += T[size_t(std::max(0, std::min(N - 1, y)) * N + x)];
            for (int y = 0; y < N; y++) { D[size_t(y * N + x)] = float(s / (2 * R + 1)); s += T[size_t(std::min(N - 1, y + R + 1) * N + x)] - T[size_t(std::max(0, y - R) * N + x)]; }
        }
    };
    box(3); box(2);
    std::vector<float> sorted = D;
    std::sort(sorted.begin(), sorted.end());
    float top = sorted[size_t(double(sorted.size()) * 0.998)];
    if (top <= 0) top = 1;
    for (float& v : D) v = std::min(1.f, v / top);
    return F;
}

// As manchas de cada vista, numa imagem só, presa ao mapa.
QImage washImage(const Field& F, const Skin& sk, uint32_t seed, const QImage& gal)
{
    if (sk.wash == WashKind::None) return QImage();
    const int S = sk.wash == WashKind::Topo || sk.photo ? 1400 : 900;
    std::vector<float> lum;
    if (sk.photo && !gal.isNull()) {
        const QImage c = gal.scaled(S, S, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);
        lum.resize(size_t(S) * size_t(S));
        // O fundo da imagem nunca é zero exato: sem esse piso, o negativo ganha uma borda dura em volta da galáxia.
        for (int j = 0; j < S; j++) { const uchar* row = c.constScanLine(j); for (int i = 0; i < S; i++) lum[size_t(j * S + i)] = float(std::max(0.0, (row[i * 4] * 0.3 + row[i * 4 + 1] * 0.59 + row[i * 4 + 2] * 0.11) / 255.0 - 0.018)); }
    }
    QImage img(S, S, QImage::Format_RGBA8888);
    img.fill(Qt::transparent);
    const SoftNoise noise(seed);
    const double P = 1.0 / S;
    using C3 = std::array<double, 3>;
    for (int y = 0; y < S; y++) {
        uchar* d = img.scanLine(y);
        for (int x = 0; x < S; x++) {
            const double u = x * P, v = y * P, t = sampleF(F, u, v), l = lum.empty() ? 0.0 : double(lum[size_t(y * S + x)]);
            C3 col { 0, 0, 0 };
            double al = 0;
            switch (sk.wash) {
            case WashKind::Holo:
                col = mix3({ 18, 110, 158 }, { 176, 246, 255 }, std::min(1.0, l * 1.25));
                al = std::pow(l, 0.66) * 0.92;
                break;
            case WashKind::Harmony:
                col = mix3({ 138, 106, 58 }, { 252, 232, 178 }, std::min(1.0, l * 1.3));
                al = std::pow(l, 0.72) * 0.82;
                break;
            case WashKind::Thermal: {
                const double q = std::pow(l, 0.5);
                const Rgb c = sk.gradAt(q);
                col = { double(c[0]), double(c[1]), double(c[2]) };
                al = std::min(1.0, q * 5);
                break;
            }
            case WashKind::Xray: {
                const double q = std::pow(l, 1.05);
                const Rgb c = sk.gradAt(q * 1.2);
                col = { double(c[0]), double(c[1]), double(c[2]) };
                al = std::min(1.0, q * 2.4) * 0.72;
                break;
            }
            case WashKind::Plate: {
                // O negativo: onde tem luz, a chapa escurece. Grãos e riscos da emulsão ficam por cima.
                double a = std::min(1.0, std::pow(l, 0.52) * 1.08);
                const double sp = noise(u * 160 + 3, v * 160 + 9);
                if (sp > 0.8) a = std::max(a, (sp - 0.8) * 2.2);
                col = { 14, 15, 20 };
                al = a + (noise(u * 3, v * 3) - 0.5) * 0.05;
                break;
            }
            case WashKind::Routes:
                col = { 214, 208, 188 };
                al = smooth(0.03, 0.34, t) * 0.5;
                break;
            case WashKind::Parch: {
                const double n = noise(u * 11, v * 11), a = smooth(0.03, 0.55, t + (n - 0.5) * 0.08) * 0.62, stain = smooth(0.58, 0.74, noise(u * 3.1 + 40, v * 3.1 + 11)) * 0.1;
                col = t < 0.4 ? mix3({ 206, 176, 118 }, { 170, 130, 84 }, t / 0.4) : mix3({ 170, 130, 84 }, { 122, 86, 52 }, std::min(1.0, (t - 0.4) / 0.6));
                al = std::max(a, stain);
                break;
            }
            case WashKind::Chart:
                col = { 140, 170, 166 };
                al = smooth(0.05, 0.6, t + (noise(u * 9, v * 9) - 0.5) * 0.05) * 0.42;
                break;
            case WashKind::Topo: {
                int b = -1;
                while (b < 8 && t >= TOPO_LIM[b + 1]) b++;
                if (b < 0) continue;
                const double e = 2.0 / F.N, sh = (sampleF(F, u - e, v - e) - sampleF(F, u + e, v + e)) * 7, k = 1 + std::max(-0.2, std::min(0.14, sh));
                col = { std::min(255.0, TOPO[b][0] * k), std::min(255.0, TOPO[b][1] * k), std::min(255.0, TOPO[b][2] * k) };
                al = 1;
                break;
            }
            case WashKind::Aqua: {
                const double n = noise(u * 9, v * 9), t2 = t + (n - 0.5) * 0.09;
                if (t2 < 0.04) continue;
                // A tinta junta na beira da mancha, como no papel molhado.
                const double pool = std::exp(-std::pow((t2 - 0.075) / 0.018, 2)) * 0.22, gran = 0.9 + noise(u * 90, v * 90) * 0.2;
                col = t2 < 0.3 ? mix3({ 120, 150, 200 }, { 150, 126, 196 }, t2 / 0.3) : t2 < 0.62 ? mix3({ 150, 126, 196 }, { 216, 132, 150 }, (t2 - 0.3) / 0.32) : mix3({ 216, 132, 150 }, { 244, 196, 120 }, std::min(1.0, (t2 - 0.62) / 0.38));
                al = (smooth(0.04, 0.32, t2) * 0.68 + pool) * gran;
                break;
            }
            case WashKind::Blue:
                col = { 232, 240, 250 };
                al = smooth(0.05, 0.75, t) * 0.13;
                break;
            default: break;
            }
            d[x * 4] = uint8_t(col[0]); d[x * 4 + 1] = uint8_t(col[1]); d[x * 4 + 2] = uint8_t(col[2]); d[x * 4 + 3] = uint8_t(clamp(al, 0, 1) * 255);
        }
    }
    return img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

// Curvas de mesma densidade, em segmentos soltos no espaço do mundo.
std::vector<CurveSegs> contours(const Field& F, const std::vector<Curve>& levels)
{
    const int N = F.N;
    const float* D = F.D.data();
    const double E = EXT;
    std::vector<CurveSegs> out;
    for (const Curve& L : levels) {
        const double t = L.t;
        CurveSegs cs; cs.L = L;
        const auto cut = [&](double a, double b) { const double den = b - a; return (t - a) / (den != 0 ? den : 1e-6); };
        const auto add = [&](double px, double py, double qx, double qy) {
            cs.seg.push_back(float(-E + (px / N) * 2 * E)); cs.seg.push_back(float(E - (py / N) * 2 * E));
            cs.seg.push_back(float(-E + (qx / N) * 2 * E)); cs.seg.push_back(float(E - (qy / N) * 2 * E));
        };
        for (int j = 0; j < N - 1; j++) for (int i = 0; i < N - 1; i++) {
            const int o = j * N + i;
            const double a = D[o], b = D[o + 1], c = D[o + N + 1], d = D[o + N];
            const int m = (a > t ? 1 : 0) | (b > t ? 2 : 0) | (c > t ? 4 : 0) | (d > t ? 8 : 0);
            if (m == 0 || m == 15) continue;
            const double x = i + 0.5, y = j + 0.5;
            const double Tx = x + cut(a, b), Ty = y, Rx = x + 1, Ry = y + cut(b, c), Bx = x + cut(d, c), By = y + 1, Lx = x, Ly = y + cut(a, d);
            switch (m) {
            case 1: case 14: add(Lx, Ly, Tx, Ty); break;
            case 2: case 13: add(Tx, Ty, Rx, Ry); break;
            case 3: case 12: add(Lx, Ly, Rx, Ry); break;
            case 4: case 11: add(Rx, Ry, Bx, By); break;
            case 6: case 9: add(Tx, Ty, Bx, By); break;
            case 7: case 8: add(Lx, Ly, Bx, By); break;
            case 5: add(Lx, Ly, Tx, Ty); add(Rx, Ry, Bx, By); break;
            case 10: add(Tx, Ty, Rx, Ry); add(Lx, Ly, Bx, By); break;
            default: break;
            }
        }
        out.push_back(std::move(cs));
    }
    return out;
}
// O pontilhado: mais ponto onde tem mais estrela.
Stipple stipple(const Field& F, const Skin& sk, uint32_t seed)
{
    uint32_t h = seed + 0x6d2b79f5u;
    const auto r = [&]() { h = h + 0x6d2b79f5u; uint32_t t = (h ^ (h >> 15)) * (1u | h); t = (t + (t ^ (t >> 7)) * (61u | t)) ^ t; return double(t ^ (t >> 14)) / 4294967296.0; };
    Stipple s;
    s.xs.resize(size_t(sk.stipN)); s.ys.resize(size_t(sk.stipN));
    int n = 0;
    for (int k = 0; k < sk.stipN * 80 && n < sk.stipN; k++) {
        const double u = r(), v = r(), t = sampleF(F, u, v);
        if (t > 0.05 && r() < std::pow(t, sk.stipP)) { s.xs[size_t(n)] = float(-EXT + u * 2 * EXT); s.ys[size_t(n)] = float(EXT - v * 2 * EXT); n++; }
    }
    s.n = n;
    return s;
}
QImage paperTile(const Skin& sk)
{
    const int S = 256;
    QImage img(S, S, QImage::Format_RGB32);
    const double G = sk.grain;
    uint32_t h = 1234567u;
    for (int y = 0; y < S; y++) {
        QRgb* d = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < S; x++) {
            h = (h ^ (h >> 13)) * 0x5bd1e995u;
            // grão ponto a ponto e uma ondulação que fecha nas emendas do ladrilho
            const double q = (TAU * x) / S, w = (TAU * y) / S, v = (double(h) / 4294967296.0 - 0.5) * G * 2 + (std::sin(q * 3 + 1.3) * std::sin(w * 5 + 0.7) + std::sin(q * 7) * std::sin(w * 2 + 2)) * G * 0.3;
            d[x] = qRgb(int(clamp(sk.paper[0] + v, 0, 255)), int(clamp(sk.paper[1] + v, 0, 255)), int(clamp(sk.paper[2] + v, 0, 255)));
        }
    }
    return img;
}

} // namespace galaxy
