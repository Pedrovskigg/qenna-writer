#include "GalaxyModels.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPainter>

#include <algorithm>
#include <cstring>
#include <functional>
#include <map>
#include <unordered_map>

namespace galaxy {

namespace {
constexpr double PI = PI_D;

// o acaso dos modelos é a variante do mulberry32 usada também nos nomes
class MRng {
public:
    explicit MRng(uint32_t seed) : a(seed) {}
    double operator()()
    {
        a = a + 0x6d2b79f5u;
        uint32_t t = a;
        t = (t ^ (t >> 15)) * (t | 1u);
        t ^= t + (t ^ (t >> 7)) * (t | 61u);
        return double(t ^ (t >> 14)) / 4294967296.0;
    }
private:
    uint32_t a;
};

// ── materiais ── kind: 0 casco em placas, 1 painel solar, 2 terra de habitat, 3 luz própria.
struct Mat { int id; int kind; V3 c; int nu, nv; double vary, seam, win; V3 wc; int wg; double spec; };
const V3 WC { 1, 0.84, 0.56 };
const V3 C0 { 0.6, 0.62, 0.66 };
enum MatId { hull, hullLit, hullDark, hullPale, hullWarm, strut, gold, solar, radiator, rock, rockDark, land, glass, warm, red, engine, portalM, tower, boxA, boxB, boxC, boxD };
const Mat MATS[] = {
    { hull, 0, C0, 8, 8, 0.14, 0.4, 0.05, WC, 4, 0.22 },
    { hullLit, 0, C0, 14, 8, 0.14, 0.4, 0.2, WC, 4, 0.22 },
    { hullDark, 0, { 0.27, 0.29, 0.33 }, 8, 8, 0.14, 0.4, 0.03, WC, 4, 0.22 },
    { hullPale, 0, { 0.82, 0.84, 0.87 }, 8, 8, 0.06, 0.25, 0, WC, 4, 0.22 },
    { hullWarm, 0, { 0.62, 0.56, 0.45 }, 8, 8, 0.14, 0.4, 0.03, WC, 4, 0.22 },
    { strut, 0, { 0.5, 0.53, 0.58 }, 1, 1, 0, 0, 0, WC, 4, 0.3 },
    { gold, 0, { 0.86, 0.66, 0.26 }, 3, 3, 0.2, 0.5, 0, WC, 4, 0.5 },
    { solar, 1, { 0.09, 0.2, 0.42 }, 8, 3, 0.14, 0.4, 0, WC, 4, 0.75 },
    { radiator, 0, { 0.86, 0.88, 0.91 }, 6, 1, 0.03, 0.3, 0, WC, 4, 0.1 },
    { rock, 0, { 0.44, 0.39, 0.33 }, 34, 18, 0.24, 0, 0, WC, 4, 0.03 },
    { rockDark, 0, { 0.13, 0.11, 0.09 }, 1, 1, 0, 0, 0, WC, 4, 0.02 },
    { land, 2, C0, 56, 3, 0.14, 0.4, 0, WC, 4, 0.1 },
    { glass, 3, { 0.55, 0.85, 1 }, 8, 8, 0.14, 0.4, 0, WC, 4, 0 },
    { warm, 3, { 1, 0.82, 0.52 }, 8, 8, 0.14, 0.4, 0, WC, 4, 0 },
    { red, 3, { 1, 0.3, 0.22 }, 8, 8, 0.14, 0.4, 0, WC, 4, 0 },
    { engine, 3, { 0.5, 0.82, 1 }, 8, 8, 0.14, 0.4, 0, WC, 4, 0 },
    { portalM, 3, { 0.55, 0.9, 1 }, 8, 8, 0.14, 0.4, 0, WC, 4, 0 },
    { tower, 0, { 0.6, 0.62, 0.68 }, 2, 6, 0.14, 0.2, 0.45, WC, 3, 0.22 },
    { boxA, 0, { 0.62, 0.42, 0.2 }, 2, 1, 0.14, 0.55, 0, WC, 4, 0.22 },
    { boxB, 0, { 0.28, 0.42, 0.58 }, 2, 1, 0.14, 0.55, 0, WC, 4, 0.22 },
    { boxC, 0, { 0.6, 0.26, 0.22 }, 2, 1, 0.14, 0.55, 0, WC, 4, 0.22 },
    { boxD, 0, { 0.36, 0.48, 0.28 }, 2, 1, 0.14, 0.55, 0, WC, 4, 0.22 },
};

inline double h3(double x, double y, double z)
{
    x = std::fabs(x); y = std::fabs(y); z = std::fabs(z);
    const double mx = std::max(x, std::max(y, z));
    if (mx == 0) return 0;
    double sum = 0, comp = 0;
    for (double v : { x, y, z }) { const double n = v / mx, summand = n * n - comp, pre = sum + summand; comp = (pre - sum) - summand; sum = pre; }
    return std::sqrt(sum) * mx;
}

// a ordem de aplicação é sempre translação, rz, ry, rx, escala
struct O {
    bool hasT = false, hasS = false;
    double t[3] { 0, 0, 0 }, s[3] { 1, 1, 1 }, rz = 0, ry = 0, rx = 0;
    O& T(double x, double y, double z) { hasT = true; t[0] = x; t[1] = y; t[2] = z; return *this; }
    O& T(const V3& p) { return T(p[0], p[1], p[2]); }
    O& RZ(double a) { rz = a; return *this; }
    O& RY(double a) { ry = a; return *this; }
    O& RX(double a) { rx = a; return *this; }
    O& S(double x, double y, double z) { hasS = true; s[0] = x; s[1] = y; s[2] = z; return *this; }
    O& S(double v) { return S(v, v, v); }
};
inline O T(double x, double y, double z) { O o; o.T(x, y, z); return o; }
inline O RZ(double a) { O o; o.RZ(a); return o; }
inline O RY(double a) { O o; o.RY(a); return o; }
inline O RX(double a) { O o; o.RX(a); return o; }

using Prof = std::vector<std::array<double, 2>>;

class Mesh {
public:
    explicit Mesh(int l) : lod(l) { stack.push_back({ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 }); }
    int lod;
    std::vector<double> v;
    std::vector<uint32_t> t;
    using M12 = std::array<double, 12>;
    std::vector<M12> stack;
    static M12 mul(const M12& A, const M12& B)
    {
        M12 r;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) r[size_t(i * 4 + j)] = A[size_t(i * 4)] * B[size_t(j)] + A[size_t(i * 4 + 1)] * B[size_t(4 + j)] + A[size_t(i * 4 + 2)] * B[size_t(8 + j)];
            r[size_t(i * 4 + 3)] = A[size_t(i * 4)] * B[3] + A[size_t(i * 4 + 1)] * B[7] + A[size_t(i * 4 + 2)] * B[11] + A[size_t(i * 4 + 3)];
        }
        return r;
    }
    template <typename F> Mesh& with(const O& o, F fn)
    {
        M12 M = stack.back();
        if (o.hasT) M = mul(M, { 1, 0, 0, o.t[0], 0, 1, 0, o.t[1], 0, 0, 1, o.t[2] });
        if (o.rz != 0) { const double c = std::cos(o.rz), s = std::sin(o.rz); M = mul(M, { c, -s, 0, 0, s, c, 0, 0, 0, 0, 1, 0 }); }
        if (o.ry != 0) { const double c = std::cos(o.ry), s = std::sin(o.ry); M = mul(M, { c, 0, s, 0, 0, 1, 0, 0, -s, 0, c, 0 }); }
        if (o.rx != 0) { const double c = std::cos(o.rx), s = std::sin(o.rx); M = mul(M, { 1, 0, 0, 0, 0, c, -s, 0, 0, s, c, 0 }); }
        if (o.hasS) M = mul(M, { o.s[0], 0, 0, 0, 0, o.s[1], 0, 0, 0, 0, o.s[2], 0 });
        stack.push_back(M);
        fn();
        stack.pop_back();
        return *this;
    }
    uint32_t vert(double x, double y, double z, double nx, double ny, double nz, double u, double vv)
    {
        const M12& M = stack.back();
        const double X = M[0] * x + M[1] * y + M[2] * z + M[3], Y = M[4] * x + M[5] * y + M[6] * z + M[7], Z = M[8] * x + M[9] * y + M[10] * z + M[11];
        const double NX = M[0] * nx + M[1] * ny + M[2] * nz, NY = M[4] * nx + M[5] * ny + M[6] * nz, NZ = M[8] * nx + M[9] * ny + M[10] * nz;
        double n = h3(NX, NY, NZ);
        if (n == 0) n = 1;
        for (double q : { X, Y, Z, NX / n, NY / n, NZ / n, u, vv }) v.push_back(q);
        return uint32_t(v.size() / 8 - 1);
    }
    void tri(uint32_t a, uint32_t b, uint32_t c, int m) { t.push_back(a); t.push_back(b); t.push_back(c); t.push_back(uint32_t(m)); }
    void quad(uint32_t a, uint32_t b, uint32_t c, uint32_t d, int m) { tri(a, b, c, m); tri(a, c, d, m); }
    // Caixa com o centro na origem.
    Mesh& box(double w, double d, double h, int m)
    {
        const double x = w / 2, y = d / 2, z = h / 2;
        const auto face = [&](const std::array<std::array<double, 3>, 4>& p, double n0, double n1, double n2) {
            uint32_t i[4];
            for (int k = 0; k < 4; k++) i[k] = vert(p[size_t(k)][0], p[size_t(k)][1], p[size_t(k)][2], n0, n1, n2, k == 1 || k == 2 ? 1 : 0, k >= 2 ? 1 : 0);
            quad(i[0], i[1], i[2], i[3], m);
        };
        face({ { { -x, -y, z }, { x, -y, z }, { x, y, z }, { -x, y, z } } }, 0, 0, 1);
        face({ { { -x, y, -z }, { x, y, -z }, { x, -y, -z }, { -x, -y, -z } } }, 0, 0, -1);
        face({ { { -x, -y, -z }, { x, -y, -z }, { x, -y, z }, { -x, -y, z } } }, 0, -1, 0);
        face({ { { x, y, -z }, { -x, y, -z }, { -x, y, z }, { x, y, z } } }, 0, 1, 0);
        face({ { { x, -y, -z }, { x, y, -z }, { x, y, z }, { x, -y, z } } }, 1, 0, 0);
        face({ { { -x, y, -z }, { -x, -y, -z }, { -x, -y, z }, { -x, y, z } } }, -1, 0, 0);
        return *this;
    }
    // Superfície de revolução em volta do eixo z. perfil = [[raio, z], ...]; flat = quinas vivas.
    Mesh& lathe(const Prof& profile, int seg, int m, bool flat = false, double a0 = 0)
    {
        const int P = int(profile.size());
        const uint32_t base = uint32_t(v.size() / 8);
        if (flat) {
            for (int j = 0; j < P - 1; j++) for (int i = 0; i < seg; i++) {
                const double a = a0 + (double(i) / seg) * TAU, b = a0 + (double(i + 1) / seg) * TAU, midA = (a + b) / 2;
                const double r0 = profile[size_t(j)][0], z0 = profile[size_t(j)][1], r1 = profile[size_t(j + 1)][0], z1 = profile[size_t(j + 1)][1];
                const double dr = r1 - r0, dz = z1 - z0;
                double l = std::hypot(dr, dz);
                if (l == 0) l = 1;
                const double nr = dz / l, nz = -dr / l, nx = std::cos(midA) * nr, ny = std::sin(midA) * nr;
                const uint32_t q0 = vert(std::cos(a) * r0, std::sin(a) * r0, z0, nx, ny, nz, 0, 0), q1 = vert(std::cos(b) * r0, std::sin(b) * r0, z0, nx, ny, nz, 1, 0), q2 = vert(std::cos(b) * r1, std::sin(b) * r1, z1, nx, ny, nz, 1, 1), q3 = vert(std::cos(a) * r1, std::sin(a) * r1, z1, nx, ny, nz, 0, 1);
                quad(q0, q1, q2, q3, m);
            }
            return *this;
        }
        for (int j = 0; j < P; j++) {
            const double r = profile[size_t(j)][0], z = profile[size_t(j)][1];
            const auto& pr = profile[size_t(std::max(0, j - 1))];
            const auto& ne = profile[size_t(std::min(P - 1, j + 1))];
            const double dr = ne[0] - pr[0], dz = ne[1] - pr[1];
            double l = std::hypot(dr, dz);
            if (l == 0) l = 1;
            const double nr = dz / l, nz = -dr / l;
            for (int i = 0; i <= seg; i++) { const double a = a0 + (double(i) / seg) * TAU, c = std::cos(a), s = std::sin(a); vert(c * r, s * r, z, c * nr, s * nr, nz, double(i) / seg, double(j) / (P - 1)); }
        }
        const uint32_t W = uint32_t(seg + 1);
        for (int j = 0; j < P - 1; j++) for (int i = 0; i < seg; i++) { const uint32_t a = base + uint32_t(j) * W + uint32_t(i); quad(a, a + 1, a + W + 1, a + W, m); }
        return *this;
    }
    Mesh& cyl(double r, double len, int seg, int m, double r2 = -1, bool caps = true, int capMat = -1)
    {
        if (r2 < 0) r2 = r;
        if (capMat < 0) capMat = m;
        const double h = len / 2;
        lathe({ { r, -h }, { r2, h } }, seg, m);
        if (caps) { lathe({ { 0.0001, -h }, { r, -h } }, seg, capMat, true); lathe({ { r2, h }, { 0.0001, h } }, seg, capMat, true); }
        return *this;
    }
    Mesh& sphere(double r, int seg, int m, double sz = 1)
    {
        const int n = std::max(4, int(std::floor(seg / 2.0 + 0.5)));
        Prof prof;
        for (int j = 0; j <= n; j++) { const double a = -PI / 2 + (double(j) / n) * PI; prof.push_back({ std::max(0.0001, std::cos(a) * r), std::sin(a) * r * sz }); }
        return lathe(prof, seg, m);
    }
    Mesh& torus(double R, double r, int seg, int side, int m, double a0 = 0, double a1 = TAU)
    {
        const uint32_t base = uint32_t(v.size() / 8), W = uint32_t(side + 1);
        for (int i = 0; i <= seg; i++) {
            const double a = a0 + ((a1 - a0) * i) / seg, c = std::cos(a), s = std::sin(a);
            for (int j = 0; j <= side; j++) { const double b = (double(j) / side) * TAU, cb = std::cos(b), sb = std::sin(b); vert(c * (R + r * cb), s * (R + r * cb), r * sb, c * cb, s * cb, sb, double(i) / seg, double(j) / side); }
        }
        for (int i = 0; i < seg; i++) for (int j = 0; j < side; j++) { const uint32_t q = base + uint32_t(i) * W + uint32_t(j); quad(q, q + W, q + W + 1, q + 1, m); }
        return *this;
    }
    // Viga entre dois pontos.
    Mesh& beam(const V3& p0, const V3& p1, double th, int m)
    {
        const double dx = p1[0] - p0[0], dy = p1[1] - p0[1], dz = p1[2] - p0[2];
        double L = h3(dx, dy, dz);
        if (L == 0) L = 1;
        const double ax = std::acos(std::max(-1.0, std::min(1.0, dz / L))), az = std::atan2(dy, dx);
        return with(T((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2, (p0[2] + p1[2]) / 2).RZ(az).RY(ax), [&] { box(th, th, L, m); });
    }
    // Treliça: quatro longarinas com travessas. De longe vira uma viga só.
    Mesh& truss(const V3& p0, const V3& p1, double w, int m = strut)
    {
        if (lod < 1) return beam(p0, p1, w * 0.6, m);
        const double dx = p1[0] - p0[0], dy = p1[1] - p0[1], dz = p1[2] - p0[2];
        double L = h3(dx, dy, dz);
        if (L == 0) L = 1;
        const double ax = std::acos(std::max(-1.0, std::min(1.0, dz / L))), az = std::atan2(dy, dx), q = w / 2, th = w * 0.13;
        const int n = std::max(2, int(std::floor(L / w + 0.5)));
        return with(T(p0[0], p0[1], p0[2]).RZ(az).RY(ax), [&] {
            for (const auto& ab : { std::array<double, 2> { -q, -q }, std::array<double, 2> { q, -q }, std::array<double, 2> { q, q }, std::array<double, 2> { -q, q } }) beam({ ab[0], ab[1], 0 }, { ab[0], ab[1], L }, th, m);
            if (lod < 2) return;
            for (int i = 0; i < n; i++) {
                const double z0 = (L * i) / n, z1 = (L * (i + 1)) / n;
                beam({ -q, -q, z0 }, { q, -q, z1 }, th * 0.7, m); beam({ q, q, z0 }, { -q, q, z1 }, th * 0.7, m);
                beam({ q, -q, z0 }, { q, q, z1 }, th * 0.7, m); beam({ -q, q, z0 }, { -q, -q, z1 }, th * 0.7, m);
            }
        });
    }
    Mesh& solarWing(double len, double wide) { return box(len, wide, 0.012, solar); }
    Mesh& lamp(double x, double y, double z, double r, int m) { return with(T(x, y, z), [&] { sphere(r, lod > 1 ? 8 : 5, m); }); }
    MeshData freeze() const
    {
        MeshData d;
        d.V.resize(v.size());
        for (size_t i = 0; i < v.size(); i++) d.V[i] = float(v[i]);
        d.T = t;
        double rad = 0;
        for (size_t i = 0; i < d.V.size(); i += 8) rad = std::max(rad, h3(d.V[i], d.V[i + 1], d.V[i + 2]));
        d.rad = rad != 0 ? rad : 1;
        d.nv = int(d.V.size() / 8);
        d.nt = int(d.T.size() / 4);
        return d;
    }
};

// ── as naves ──
void addShip(Mesh& m, int kind, int seg)
{
    if (kind == 0) { // Nave pequena: fuselagem, asas e dois motores.
        m.with(RY(PI / 2).S(0.1, 0.13, 1), [&] { m.lathe({ { 0.0001, 0.5 }, { 0.5, 0.2 }, { 1, -0.05 }, { 1, -0.4 }, { 0.75, -0.5 }, { 0.0001, -0.5 } }, seg, hullPale); });
        m.with(T(-0.14, 0.26, 0).RZ(-0.3), [&] { m.box(0.3, 0.42, 0.018, hull); });
        m.with(T(-0.14, -0.26, 0).RZ(0.3), [&] { m.box(0.3, 0.42, 0.018, hull); });
        m.with(T(0.16, 0, 0.055).S(1.8, 0.8, 0.5), [&] { m.sphere(0.05, seg, glass); });
        for (double y : { -0.45, 0.45 }) { m.with(T(-0.22, y, 0).RY(PI / 2), [&] { m.cyl(0.04, 0.24, seg, hullDark); }); m.lamp(-0.35, y, 0, 0.03, engine); }
    } else if (kind == 1) { // Cargueiro: cabine na frente e contêineres presos numa espinha.
        m.with(T(-0.06, 0, 0), [&] { m.box(0.72, 0.03, 0.03, strut); });
        const int cols[] = { boxA, boxB, boxC, boxD };
        int n = 0;
        for (int i = 0; i < 4; i++) for (const auto& yz : { std::array<double, 2> { -0.09, 0.08 }, std::array<double, 2> { 0.09, 0.08 }, std::array<double, 2> { -0.09, -0.08 }, std::array<double, 2> { 0.09, -0.08 } }) m.with(T(-0.3 + i * 0.16, yz[0], yz[1]), [&] { m.box(0.14, 0.15, 0.13, cols[n++ % 4]); });
        m.with(T(0.32, 0, 0).RY(PI / 2), [&] { m.lathe({ { 0.0001, 0.18 }, { 0.09, 0.12 }, { 0.15, 0.02 }, { 0.17, -0.12 }, { 0.0001, -0.12 } }, seg, hullPale); });
        m.lamp(0.42, 0, 0.05, 0.03, glass);
        m.with(T(-0.45, 0, 0).RY(PI / 2), [&] { m.cyl(0.13, 0.08, seg, hullDark); });
        m.lamp(-0.5, 0, 0, 0.09, engine);
    } else if (kind == 3) { // Tanque: esferas de carga em fila, presas numa espinha.
        m.with(T(-0.02, 0, 0), [&] { m.box(0.78, 0.03, 0.03, strut); });
        for (int i = 0; i < 4; i++) m.with(T(-0.3 + i * 0.17, 0, 0), [&] { m.sphere(0.085, seg, i % 2 ? hullPale : hullWarm); });
        m.with(T(0.42, 0, 0).RY(PI / 2), [&] { m.lathe({ { 0.0001, 0.08 }, { 0.05, 0.05 }, { 0.07, -0.05 }, { 0.0001, -0.05 } }, seg, hull); });
        m.with(T(-0.44, 0, 0).RY(PI / 2), [&] { m.cyl(0.06, 0.1, seg, hullDark); });
        m.lamp(-0.5, 0, 0, 0.045, engine);
    } else if (kind == 4) { // Paquete: um tubo comprido cheio de janelas, com asas curtas.
        m.with(RY(PI / 2), [&] { m.lathe({ { 0.0001, 0.5 }, { 0.07, 0.42 }, { 0.11, 0.26 }, { 0.11, -0.36 }, { 0.08, -0.46 }, { 0.0001, -0.46 } }, std::max(6, seg), hullLit); });
        for (double y : { -0.16, 0.16 }) m.with(T(-0.22, y, 0).RZ(y > 0 ? -0.2 : 0.2), [&] { m.box(0.2, 0.14, 0.014, hull); });
        m.with(T(-0.28, 0, 0.11), [&] { m.box(0.14, 0.012, 0.1, hull); });
        m.lamp(0.42, 0, 0.03, 0.022, glass);
        m.lamp(-0.47, 0, 0, 0.05, engine);
    } else { // Cruzador: casco em cunha, ponte alta e aletas.
        m.with(RY(PI / 2).S(0.11, 0.17, 1), [&] { m.lathe({ { 0.0001, 0.5 }, { 0.4, 0.3 }, { 0.8, 0.02 }, { 1, -0.3 }, { 1, -0.44 }, { 0.8, -0.5 }, { 0.0001, -0.5 } }, std::max(6, seg), hull); });
        m.with(T(-0.26, 0, 0).S(1, 1.5, 0.6), [&] { m.box(0.34, 0.18, 0.08, hullDark); });
        m.with(T(-0.12, 0, 0.1), [&] { m.box(0.3, 0.14, 0.08, hullLit); });
        m.with(T(-0.16, 0, 0.17), [&] { m.box(0.14, 0.09, 0.07, hullLit); });
        for (double y : { -0.21, 0.21 }) m.with(T(-0.34, y, 0).RZ(y > 0 ? -0.3 : 0.3), [&] { m.box(0.24, 0.14, 0.018, hullDark); });
        for (double y : { -0.08, 0.08 }) m.lamp(-0.5, y, 0, 0.05, engine);
    }
}

// ── as construções ──
void bSat(Mesh& m, int q, MRng&)
{
    m.with(O().S(1, 1, 1), [&] { m.box(0.34, 0.34, 0.4, gold); });
    m.with(T(0.6, 0, 0), [&] { m.solarWing(0.72, 0.36); });
    m.with(T(-0.6, 0, 0), [&] { m.solarWing(0.72, 0.36); });
    m.beam({ -0.96, 0, 0 }, { 0.96, 0, 0 }, 0.03, strut);
    m.beam({ 0, 0, 0.2 }, { 0, 0, 0.42 }, 0.03, strut);
    m.with(T(0, 0, 0.42), [&] { m.lathe({ { 0.0001, -0.02 }, { 0.14, 0.03 }, { 0.26, 0.14 } }, q, hullPale); });
    m.lamp(0, 0, -0.24, 0.04, red);
}
void bPosto(Mesh& m, int q, MRng&)
{
    m.sphere(0.22, q, hullLit);
    for (int i = 0; i < 4; i++) m.with(RZ((i / 4.0) * TAU + 0.5), [&] {
        m.with(T(0.46, 0, 0).RY(PI / 2), [&] { m.cyl(0.085, 0.6, q, hullLit); });
        m.with(T(0.78, 0, 0), [&] { m.sphere(0.1, q, hull); });
    });
    m.with(RZ(0.5 + PI / 4), [&] { m.beam({ -0.64, 0, 0 }, { 0.64, 0, 0 }, 0.025, strut); m.with(T(0.82, 0, 0), [&] { m.solarWing(0.34, 0.42); }); m.with(T(-0.82, 0, 0), [&] { m.solarWing(0.34, 0.42); }); });
    m.with(T(0, 0, 0.3), [&] { m.cyl(0.08, 0.26, q, hull); });
    m.lamp(0, 0, 0.46, 0.05, red);
}
void bRoda(Mesh& m, int q, MRng&)
{
    // Aro, raios e cubo: gira pra ter gravidade.
    m.torus(0.8, 0.105, q * 3, std::max(6, q), hullLit);
    m.torus(0.8, 0.125, q * 3, 4, hullDark, 0, 0.0001);
    for (int i = 0; i < 12; i++) m.with(RZ((i / 12.0) * TAU).T(0.8, 0, 0), [&] { m.torus(0, 0.0001, 3, 3, strut); });
    for (int i = 0; i < 4; i++) m.with(RZ((i / 4.0) * TAU + 0.4), [&] { m.with(T(0.45, 0, 0).RY(PI / 2), [&] { m.cyl(0.04, 0.66, q, hull); }); });
    m.cyl(0.17, 0.34, q, hull);
    m.with(T(0, 0, 0.17), [&] { m.sphere(0.17, q, hullPale, 0.45); });
    m.with(T(0, 0, -0.17), [&] { m.sphere(0.17, q, hullPale, 0.45); });
    m.with(T(0, 0, 0.38), [&] { m.cyl(0.05, 0.3, q, hull); });
    m.with(T(0, 0, 0.56), [&] { m.torus(0.13, 0.022, q * 2, 5, strut); });
    m.with(T(0, 0, -0.42), [&] { m.cyl(0.07, 0.2, q, hullDark); });
    m.lamp(0, 0, 0.56, 0.035, glass);
}
void bBase(Mesh& m, int q, MRng&)
{
    m.truss({ -0.98, 0, 0 }, { 0.98, 0, 0 }, 0.09);
    for (double x : { -0.74, -0.42, 0.42, 0.74 }) for (double y : { -1.0, 1.0 }) {
        m.beam({ x, 0, 0 }, { x, y * 0.14, 0 }, 0.024, strut);
        m.with(T(x, y * 0.56, 0).RX(0.1 * y), [&] { m.solarWing(0.24, 0.82); });
    }
    m.with(RY(PI / 2), [&] { m.cyl(0.11, 0.7, q, hullLit); });
    m.with(T(0, 0, 0), [&] { m.sphere(0.15, q, hull); });
    for (double y : { -1.0, 1.0 }) {
        m.with(T(0, y * 0.28, 0).RX(PI / 2), [&] { m.cyl(0.09, 0.42, q, hullLit); });
        m.with(T(0, y * 0.5, 0), [&] { m.sphere(0.1, q, hull); });
    }
    m.with(T(0, 0, 0.26), [&] { m.cyl(0.08, 0.36, q, hullWarm); });
    m.with(T(0, 0, 0.47), [&] { m.sphere(0.09, q, glass, 0.7); });
    m.with(T(0.3, 0, -0.26), [&] { m.box(0.34, 0.03, 0.42, radiator); });
    m.with(T(-0.3, 0, -0.26), [&] { m.box(0.34, 0.03, 0.42, radiator); });
    m.lamp(-0.98, 0, 0, 0.035, red); m.lamp(0.98, 0, 0, 0.035, glass);
}
void bEstaleiro(Mesh& m, int q, MRng&)
{
    // Berço de treliça com uma nave pela metade dentro.
    const double X = 0.92, Y = 0.34, Z = 0.3;
    for (const auto& yz : { std::array<double, 2> { -Y, -Z }, std::array<double, 2> { Y, -Z }, std::array<double, 2> { Y, Z }, std::array<double, 2> { -Y, Z } }) m.truss({ -X, yz[0], yz[1] }, { X, yz[0], yz[1] }, 0.05);
    for (double x : { -X, 0.0, X }) {
        m.beam({ x, -Y, -Z }, { x, Y, -Z }, 0.022, strut); m.beam({ x, -Y, Z }, { x, Y, Z }, 0.022, strut);
        m.beam({ x, -Y, -Z }, { x, -Y, Z }, 0.022, strut); m.beam({ x, Y, -Z }, { x, Y, Z }, 0.022, strut);
    }
    // O casco: nariz e popa prontos, o meio só em costelas.
    m.with(T(0.55, 0, 0).RY(PI / 2), [&] { m.lathe({ { 0.0001, 0.3 }, { 0.1, 0.18 }, { 0.17, 0 }, { 0.19, -0.26 } }, q, hull); });
    m.with(T(-0.56, 0, 0).RY(PI / 2), [&] { m.cyl(0.19, 0.44, q, hullLit); });
    m.with(T(-0.84, 0, 0).RY(PI / 2), [&] { m.cyl(0.13, 0.12, q, hullDark); });
    for (int i = 0; i < 7; i++) m.with(T(-0.28 + i * 0.09, 0, 0).RY(PI / 2), [&] { m.torus(0.19, 0.011, q * 2, 4, strut); });
    for (double a : { 0.0, PI / 2, PI, -PI / 2 }) m.with(RX(a), [&] { m.beam({ -0.34, 0, 0.19 }, { 0.28, 0, 0.19 }, 0.018, strut); });
    m.with(T(-0.3, -Y - 0.1, Z + 0.04), [&] { m.box(0.26, 0.14, 0.14, hullWarm); });
    m.with(T(0.4, Y + 0.1, -Z - 0.04), [&] { m.box(0.26, 0.14, 0.14, hullWarm); });
    for (double x : { -X, 0.0, X }) for (double y : { -Y, Y }) m.lamp(x, y, Z, 0.022, warm);
}
void bMina(Mesh& m, int q, MRng& r)
{
    // Asteroide irregular de verdade: uma esfera amassada, com o corte da mina e as máquinas.
    const int seg = q * 3, n = int(std::floor(seg / 2.0 + 0.5));
    const uint32_t base = uint32_t(m.v.size() / 8);
    struct Bump { double x, y, z, p, k; };
    std::vector<Bump> bumps;
    for (int i = 0; i < 46; i++) {
        const double z = r() * 2 - 1, a = r() * TAU, c = std::sqrt(1 - z * z);
        const bool big = i < 10;
        const double p = big ? 8 + r() * 10 : 60 + r() * 170;
        const double k = big ? (r() - 0.5) * 0.42 : -(0.05 + r() * 0.09);
        bumps.push_back({ c * std::cos(a), c * std::sin(a), z, p, k });
    }
    const auto R = [&](double x, double y, double z) { double v = 0.78; for (const Bump& b : bumps) { const double d = x * b.x + y * b.y + z * b.z; if (d > 0.6) v += b.k * std::pow(d, b.p); } return v; };
    const auto P = [&](int j, int i) { const double la = -PI / 2 + (double(j) / n) * PI, lo = (double(i) / seg) * TAU, c = std::cos(la), x = c * std::cos(lo), y = c * std::sin(lo), z = std::sin(la), k = R(x, y, z); return V3 { x * k * 1.12, y * k * 0.92, z * k * 0.82 }; };
    for (int j = 0; j <= n; j++) for (int i = 0; i <= seg; i++) {
        const V3 p = P(j, i), a = P(std::min(n, j + 1), i), b = P(std::max(0, j - 1), i), c = P(j, i + 1), d = P(j, i - 1);
        const double ux = a[0] - b[0], uy = a[1] - b[1], uz = a[2] - b[2], vx = c[0] - d[0], vy = c[1] - d[1], vz = c[2] - d[2];
        double nx = vy * uz - vz * uy, ny = vz * ux - vx * uz, nz = vx * uy - vy * ux;
        if (nx * p[0] + ny * p[1] + nz * p[2] < 0) { nx = -nx; ny = -ny; nz = -nz; }
        m.vert(p[0], p[1], p[2], nx, ny, nz, double(i) / seg, double(j) / n);
    }
    const uint32_t W = uint32_t(seg + 1);
    for (int j = 0; j < n; j++) for (int i = 0; i < seg; i++) { const uint32_t a = base + uint32_t(j) * W + uint32_t(i); m.quad(a, a + 1, a + W + 1, a + W, rock); }
    // O corte, as máquinas e a esteira saindo.
    m.with(T(0.26, 0.04, 0.58).RZ(0.4).S(1.3, 0.9, 0.34), [&] { m.lathe({ { 0.3, 0.06 }, { 0.2, -0.05 }, { 0.12, -0.16 }, { 0.0001, -0.2 } }, q, rockDark); });
    m.with(T(0.26, 0.04, 0.72).RY(0.3), [&] { m.beam({ 0, 0, -0.1 }, { 0, 0, 0.22 }, 0.05, gold); m.with(T(0, 0, 0.24), [&] { m.box(0.26, 0.1, 0.05, gold); }); });
    m.truss({ 0.3, 0.1, 0.62 }, { -0.96, 0.52, 0.5 }, 0.07);
    m.with(T(-0.96, 0.52, 0.5), [&] { m.box(0.22, 0.2, 0.14, hullWarm); });
    m.with(T(-0.46, -0.36, 0.56).RX(-0.4), [&] { m.box(0.3, 0.2, 0.12, hullLit); });
    m.with(T(0.6, -0.34, 0.42).RY(0.5), [&] { m.box(0.24, 0.18, 0.12, hullLit); });
    m.lamp(0.26, 0.04, 0.5, 0.05, warm); m.lamp(-0.96, 0.52, 0.6, 0.03, warm); m.lamp(0.6, -0.34, 0.52, 0.03, warm);
}
void bCilindro(Mesh& m, int q, MRng&)
{
    // Colônia cilíndrica: gira no próprio eixo, com janelões, faixas de terra e espelhos abertos.
    m.with(RY(PI / 2), [&] {
        m.cyl(0.25, 1.3, q * 2, land, 0.25, false);
        m.with(T(0, 0, 0), [&] { for (int i = 0; i < 3; i++) m.with(RZ((i / 3.0) * TAU), [&] { m.with(T(0.252, 0, 0), [&] { m.box(0.006, 0.1, 1.28, glass); }); }); });
        for (double z : { -0.6, -0.3, 0.0, 0.3, 0.6 }) m.with(T(0, 0, z), [&] { m.torus(0.255, 0.014, q * 2, 4, hull); });
        m.with(T(0, 0, -0.65), [&] { m.sphere(0.25, q * 2, hullLit, 0.5); });
        m.with(T(0, 0, 0.65), [&] { m.lathe({ { 0.25, 0 }, { 0.1, 0.1 }, { 0.07, 0.28 } }, q, hull); });
        for (int i = 0; i < 3; i++) m.with(RZ((i / 3.0) * TAU + 0.5).T(0, 0, 0.65), [&] { m.with(T(0.42, 0, 0.3).RY(0.8), [&] { m.box(0.56, 0.26, 0.014, hullPale); }); });
    });
    m.lamp(0.98, 0, 0, 0.05, glass);
}
void bCidade(Mesh& m, int q, MRng& r)
{
    // Cidade em disco: casco de pires, anéis de torres e o núcleo no centro.
    m.lathe({ { 0.0001, -0.34 }, { 0.2, -0.3 }, { 0.6, -0.16 }, { 0.93, -0.03 }, { 0.96, 0.02 }, { 0.92, 0.06 }, { 0.6, 0.08 }, { 0.0001, 0.09 } }, q * 3, hull);
    const int n = m.lod >= 2 ? 150 : m.lod == 1 ? 60 : 18;
    for (int i = 0; i < n; i++) {
        const double a = r() * TAU, d = 0.24 + std::sqrt(r()) * 0.62, nearK = 1 - (d - 0.24) / 0.62;
        const double h1 = r(), h2 = r();
        const double h = 0.04 + h1 * h2 * 0.3 * (0.4 + nearK), w = 0.024 + r() * 0.04;
        m.with(T(std::cos(a) * d, std::sin(a) * d, 0.08 + h / 2).RZ(a), [&] { m.box(w, w * (0.7 + r()), h, tower); });
    }
    m.with(T(0, 0, 0.12), [&] { m.cyl(0.18, 0.1, q * 2, hullPale); });
    m.with(T(0, 0, 0.17), [&] { m.sphere(0.17, q * 2, glass, 0.65); });
    m.with(T(0, 0, 0.42), [&] { m.cyl(0.03, 0.34, q, tower, 0.008); });
    m.with(T(0, 0, 0.07), [&] { m.torus(0.6, 0.012, q * 3, 4, strut); });
    m.with(T(0, 0, -0.34), [&] { m.sphere(0.06, q, engine); });
}
void bFarol(Mesh& m, int q, MRng&)
{
    // Mastro de navegação com o anel do transponder e a lâmpada na ponta.
    m.truss({ 0, 0, -0.62 }, { 0, 0, 0.62 }, 0.1);
    m.with(T(0, 0, 0.1), [&] { m.torus(0.42, 0.025, q * 2, 5, hull); });
    for (int i = 0; i < 3; i++) m.with(RZ((i / 3.0) * TAU), [&] { m.beam({ 0, 0, 0.1 }, { 0.42, 0, 0.1 }, 0.02, strut); });
    m.with(T(0, 0, -0.68), [&] { m.box(0.22, 0.22, 0.18, hullDark); });
    m.with(T(0.42, 0, -0.68), [&] { m.solarWing(0.5, 0.22); });
    m.with(T(-0.42, 0, -0.68), [&] { m.solarWing(0.5, 0.22); });
    m.with(T(0, 0, 0.7), [&] { m.sphere(0.1, q, red); });
    m.with(T(0, 0, 0.62), [&] { m.cyl(0.1, 0.05, q, hullDark); });
}
void bTelescopio(Mesh& m, int q, MRng&)
{
    // Prato parabólico de verdade, com o foco suspenso e o mastro atrás.
    Prof prof;
    for (int i = 0; i <= 10; i++) { const double k = i / 10.0; prof.push_back({ std::max(0.0001, k * 0.92), k * k * 0.34 - 0.1 }); }
    m.lathe(prof, q * 3, hullPale);
    m.with(T(0, 0, 0.24), [&] { m.torus(0.92, 0.018, q * 3, 5, strut); });
    for (int i = 0; i < 3; i++) { const double a = (i / 3.0) * TAU + 0.5; m.beam({ std::cos(a) * 0.88, std::sin(a) * 0.88, 0.22 }, { 0, 0, 0.74 }, 0.022, strut); }
    m.with(T(0, 0, 0.76), [&] { m.cyl(0.07, 0.14, q, hull); });
    m.lamp(0, 0, 0.68, 0.035, glass);
    m.truss({ 0, 0, -0.1 }, { 0, 0, -0.52 }, 0.1);
    m.with(T(0, 0, -0.58), [&] { m.box(0.3, 0.3, 0.14, hullDark); });
    m.with(T(0.46, 0, -0.58), [&] { m.solarWing(0.5, 0.2); });
    m.with(T(-0.46, 0, -0.58), [&] { m.solarWing(0.5, 0.2); });
}
void bForte(Mesh& m, int q, MRng&)
{
    // Plataforma de defesa: casco hexagonal facetado e seis torres de canhão.
    m.lathe({ { 0.0001, -0.2 }, { 0.42, -0.2 }, { 0.76, -0.05 }, { 0.76, 0.05 }, { 0.5, 0.15 }, { 0.0001, 0.15 } }, 6, hull, true, 0.26);
    m.lathe({ { 0.42, 0.15 }, { 0.3, 0.27 }, { 0.0001, 0.27 } }, 6, hullLit, true, 0.8);
    m.with(T(0, 0, 0.3), [&] { m.cyl(0.11, 0.1, q, hullDark); });
    m.lamp(0, 0, 0.37, 0.05, red);
    for (int i = 0; i < 6; i++) m.with(RZ((i / 6.0) * TAU), [&] { m.with(T(0.66, 0, 0.09), [&] {
        m.cyl(0.09, 0.06, q, hullLit);
        m.with(T(0, 0, 0.05), [&] { m.sphere(0.075, q, hull, 0.7); });
        for (double y : { -0.035, 0.035 }) m.beam({ 0, y, 0.06 }, { 0.32, y, 0.1 }, 0.024, hullDark);
    }); });
    m.with(T(0, 0, -0.26), [&] { m.cyl(0.2, 0.1, q, hullDark, 0.1); });
}
void bPortal(Mesh& m, int q, MRng&)
{
    // Anel de salto em oito segmentos, com torres para fora. A energia de dentro anda e é desenhada à parte.
    for (int i = 0; i < 8; i++) m.torus(0.8, 0.09, q, std::max(6, q), hullLit, (i / 8.0) * TAU + 0.05, ((i + 1) / 8.0) * TAU - 0.05);
    m.torus(0.8, 0.055, q * 4, 6, hullDark);
    m.torus(0.69, 0.012, q * 4, 4, portalM);
    for (int i = 0; i < 8; i++) m.with(RZ((i / 8.0) * TAU), [&] {
        m.with(T(0.92, 0, 0).RY(PI / 2), [&] { m.lathe({ { 0.1, -0.1 }, { 0.06, 0.08 }, { 0.02, 0.12 } }, std::max(6, q), hull, true); });
        m.lamp(1.03, 0, 0, 0.022, portalM);
        m.with(T(0.8, 0, 0), [&] { m.cyl(0.085, 0.24, q, hullDark); });
    });
}
void bNaveMundo(Mesh& m, int q, MRng& r)
{
    // Uma cidade que viaja: espinha comprida, módulos em volta, motores na popa.
    m.with(RY(PI / 2).T(0, 0, 0).S(0.2, 0.26, 1), [&] { m.lathe({ { 0.0001, 1 }, { 0.5, 0.86 }, { 0.85, 0.6 }, { 1, 0.2 }, { 1, -0.7 }, { 0.82, -0.86 }, { 0.0001, -0.86 } }, q * 2, hullLit); });
    const int n = m.lod >= 2 ? 60 : m.lod == 1 ? 26 : 8;
    for (int i = 0; i < n; i++) {
        const double x = -0.7 + r() * 1.34, a = r() * TAU, w = 0.05 + r() * 0.1, tall = 0.03 + r() * 0.07, taper = 1 - std::max(0.0, x - 0.2) * 0.9, ra = 0.21 * taper;
        m.with(T(x, std::cos(a) * ra * 1.25, std::sin(a) * ra * 0.95).RX(a + PI / 2), [&] { m.box(w, 0.05 + r() * 0.06, tall, tower); });
    }
    for (double a : { 0.8, 2.34, 3.94, 5.48 }) m.with(RX(a), [&] { m.with(T(-0.42, 0.33, 0).RY(PI / 2), [&] { m.cyl(0.07, 0.36, q, hull); }); });
    for (double a : { 0.0, 2.09, 4.19 }) m.with(RX(a), [&] { m.with(T(-0.9, 0.11, 0).RY(PI / 2), [&] { m.cyl(0.075, 0.16, q, hullDark, 0.05); m.with(T(0, 0, -0.1), [&] { m.sphere(0.06, q, engine, 0.4); }); }); });
    m.lamp(0.74, 0, 0.14, 0.05, glass);
}
void bFrota(Mesh& m, int q, MRng&)
{
    static const double FROTA[][5] = { { -0.1, 0, 0, 1.15, 2 }, { 0.42, -0.5, 0.14, 0.6, 1 }, { 0.42, 0.5, -0.12, 0.6, 1 }, { -0.6, -0.56, -0.1, 0.56, 1 }, { -0.6, 0.56, 0.12, 0.56, 1 }, { 0.8, 0, 0.1, 0.42, 0 }, { 0.05, -0.88, 0, 0.32, 0 }, { 0.05, 0.88, 0.02, 0.32, 0 }, { -0.86, 0, -0.06, 0.32, 0 } };
    for (const auto& s : FROTA) m.with(T(s[0], s[1], s[2]).S(s[3]), [&] { addShip(m, int(s[4]), std::max(6, q)); });
}
void bHalo(Mesh& m, int q, MRng&)
{
    // Anel-habitat: terra por dentro, muralhas dos lados e casco por fora. Inclinado, dá pra ver o chão.
    const int seg = q * 5;
    const double R = 0.84, H = 0.26;
    m.lathe({ { R, H }, { R, -H } }, seg, land);
    m.lathe({ { R + 0.09, -H }, { R + 0.09, H } }, seg, hull);
    m.lathe({ { R, H }, { R + 0.09, H } }, seg, hullPale, false);
    m.lathe({ { R + 0.09, -H }, { R, -H } }, seg, hullPale, false);
    m.lathe({ { R - 0.03, H }, { R, H } }, seg, hullPale, false);
    m.lathe({ { R, -H }, { R - 0.03, -H } }, seg, hullPale, false);
    for (int i = 0; i < 12; i++) m.with(RZ((i / 12.0) * TAU).T(R + 0.1, 0, 0), [&] { m.box(0.03, 0.05, H * 2.05, hullDark); });
}
void bColetor(Mesh& m, int q, MRng&)
{
    // Coletor do sifão: a boca aberta pra estrela, um anel e o tanque atrás.
    m.with(RY(-PI / 2), [&] {
        m.lathe({ { 0.72, 0.5 }, { 0.34, 0.06 }, { 0.2, -0.14 } }, q * 2, hullDark);
        m.with(T(0, 0, 0.5), [&] { m.torus(0.72, 0.035, q * 2, 5, portalM); });
        m.with(T(0, 0, -0.5), [&] { m.cyl(0.22, 0.72, q, hullLit); });
    });
}
void bAnelNo(Mesh& m, int q, MRng&)
{
    // Estação presa no anel orbital: a braçadeira abraça o anel, com doca pra fora e o pé do cabo pra dentro.
    m.box(0.42, 1.5, 0.3, hull);
    for (double y : { -0.62, 0.62 }) m.with(T(0, y, 0), [&] { m.box(0.5, 0.12, 0.38, hullDark); });
    m.with(T(0, 0, 0.1), [&] { m.cyl(0.42, 0.34, q * 2, hullLit); });
    m.with(T(0, 0, 0.27), [&] { m.lathe({ { 0.42, 0 }, { 0.3, 0.08 }, { 0.12, 0.12 }, { 0.0001, 0.12 } }, q * 2, hullPale); });
    m.with(T(0, 0, 0.42), [&] { m.cyl(0.05, 0.22, q, strut); });
    m.lamp(0, 0, 0.54, 0.035, red);
    m.with(T(0.62, 0, 0.02).RY(PI / 2), [&] { m.cyl(0.11, 0.42, q, hull); });
    m.with(T(0.86, 0, 0.02), [&] { m.torus(0, 0.0001, 3, 3, strut); });
    m.with(T(0.86, 0, 0.02).RY(PI / 2), [&] { m.torus(0.15, 0.022, q * 2, 5, hullDark); });
    m.with(T(-0.6, 0, -0.02).RY(PI / 2), [&] { m.cyl(0.14, 0.32, q, hullDark, 0.07); });
    for (double y : { -0.34, 0.34 }) m.with(T(0.3, y, -0.22), [&] { m.box(0.46, 0.2, 0.02, radiator); });
}
void bContrapeso(Mesh& m, int q, MRng&)
{
    m.sphere(0.5, q * 2, hullLit);
    m.with(T(0, 0, 0), [&] { m.torus(0.74, 0.04, q * 2, 5, hull); });
    for (int i = 0; i < 4; i++) m.with(RZ((i / 4.0) * TAU), [&] { m.beam({ 0.48, 0, 0 }, { 0.74, 0, 0 }, 0.04, strut); });
}
using BuildFn = void (*)(Mesh&, int, MRng&);
const std::map<std::string, BuildFn>& builders()
{
    static const std::map<std::string, BuildFn> t {
        { "sat", bSat }, { "posto", bPosto }, { "roda", bRoda }, { "base", bBase }, { "estaleiro", bEstaleiro }, { "mina", bMina }, { "cilindro", bCilindro }, { "cidade", bCidade },
        { "farol", bFarol }, { "telescopio", bTelescopio }, { "forte", bForte }, { "portal", bPortal }, { "naveMundo", bNaveMundo }, { "frota", bFrota }, { "halo", bHalo }, { "coletor", bColetor },
        { "anelNo", bAnelNo }, { "contrapeso", bContrapeso },
        { "nave0", [](Mesh& m, int q, MRng&) { addShip(m, 0, q); } }, { "nave1", [](Mesh& m, int q, MRng&) { addShip(m, 1, q); } }, { "nave2", [](Mesh& m, int q, MRng&) { addShip(m, 2, q); } },
        { "nave3", [](Mesh& m, int q, MRng&) { addShip(m, 3, q); } }, { "nave4", [](Mesh& m, int q, MRng&) { addShip(m, 4, q); } },
    };
    return t;
}
std::unordered_map<std::string, std::unique_ptr<MeshData>>& meshCache() { static std::unordered_map<std::string, std::unique_ptr<MeshData>> c; return c; }

inline double hash2(int i, int j, int s)
{
    uint32_t h = (uint32_t(i) * 73856093u) ^ (uint32_t(j) * 19349663u) ^ uint32_t(s);
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 1023) / 1023.0;
}
std::vector<float>& zbufRef() { static std::vector<float> z; return z; }
inline uint8_t toU8(double v) { if (!(v > 0)) return 0; if (v >= 255) return 255; const double f = std::floor(v), d = v - f; if (d < 0.5) return uint8_t(f); if (d > 0.5) return uint8_t(f + 1); return int(f) % 2 == 0 ? uint8_t(f) : uint8_t(f + 1); }
} // namespace

bool hasModel(const std::string& type) { return builders().count(type) != 0; }

const MeshData& meshOf(const std::string& type, uint32_t seed, int lod)
{
    const std::string k = type + ":" + std::to_string(seed) + ":" + std::to_string(lod);
    auto& cache = meshCache();
    auto it = cache.find(k);
    if (it != cache.end()) return *it->second;
    Mesh m(lod);
    MRng r(seed);
    static const int Q[] = { 6, 10, 16 };
    builders().at(type)(m, Q[lod], r);
    auto d = std::make_unique<MeshData>(m.freeze());
    const MeshData& ref = *d;
    cache[k] = std::move(d);
    return ref;
}

void rasterize(const MeshData& mesh, int S, double rot, double pitch, const V3& L, uint8_t* out, uint32_t seed)
{
    const float* V = mesh.V.data();
    const uint32_t* T = mesh.T.data();
    const int nv = mesh.nv;
    const double cr = std::cos(rot), sr = std::sin(rot), cp = std::cos(pitch), sp = std::sin(pitch);
    const double k = ((S / 2.0) * 0.97) / mesh.rad, mid = S / 2.0;
    static thread_local std::vector<float> px, py, pz, nx, ny, nz;
    px.resize(size_t(nv)); py.resize(size_t(nv)); pz.resize(size_t(nv)); nx.resize(size_t(nv)); ny.resize(size_t(nv)); nz.resize(size_t(nv));
    for (int i = 0; i < nv; i++) {
        const int o = i * 8;
        const double x = V[o], y = V[o + 1], z = V[o + 2], X = x * cr - y * sr, Y = x * sr + y * cr, Yv = Y * cp + z * sp, Zv = -Y * sp + z * cp;
        px[size_t(i)] = float(mid + X * k); py[size_t(i)] = float(mid - Yv * k); pz[size_t(i)] = float(Zv * k);
        const double a = V[o + 3], b = V[o + 4], c = V[o + 5], A = a * cr - b * sr, B = a * sr + b * cr;
        nx[size_t(i)] = float(A); ny[size_t(i)] = float(B * cp + c * sp); nz[size_t(i)] = float(-B * sp + c * cp);
    }
    const size_t need = size_t(S) * size_t(S);
    auto& zbuf = zbufRef();
    if (zbuf.size() < need) zbuf.resize(need);
    std::fill(zbuf.begin(), zbuf.begin() + long(need), -1e9f);
    std::memset(out, 0, need * 4);
    const double hx0 = L[0], hy0 = L[1], hz0 = L[2] + 1;
    double hn = h3(hx0, hy0, hz0);
    if (hn == 0) hn = 1;
    const double hx = hx0 / hn, hy = hy0 / hn, hz = hz0 / hn;
    const int nT = mesh.nt;
    const bool detail = S >= 96;
    const int iSeed = int(seed);
    for (int t = 0; t < nT; t++) {
        const uint32_t ia = T[t * 4], ib = T[t * 4 + 1], ic = T[t * 4 + 2];
        const Mat& M = MATS[T[t * 4 + 3]];
        const double ax = px[ia], ay = py[ia], bx = px[ib], by = py[ib], cx = px[ic], cy = py[ic];
        const double area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
        if (area == 0) continue;
        const int x0 = int(std::max(0.0, std::floor(std::min(ax, std::min(bx, cx))))), x1 = int(std::min(S - 1.0, std::ceil(std::max(ax, std::max(bx, cx)))));
        const int y0 = int(std::max(0.0, std::floor(std::min(ay, std::min(by, cy))))), y1 = int(std::min(S - 1.0, std::ceil(std::max(ay, std::max(by, cy)))));
        if (x0 > x1 || y0 > y1) continue;
        const double ia1 = 1 / area;
        const size_t oa = size_t(ia) * 8, ob = size_t(ib) * 8, oc = size_t(ic) * 8;
        const double ua = V[oa + 6], va = V[oa + 7], ub = V[ob + 6], vb = V[ob + 7], uc = V[oc + 6], vc = V[oc + 7];
        const double za = pz[ia], zb = pz[ib], zc = pz[ic];
        for (int y = y0; y <= y1; y++) {
            const double fy = y + 0.5;
            for (int x = x0; x <= x1; x++) {
                const double fx = x + 0.5;
                const double w0 = ((bx - fx) * (cy - fy) - (by - fy) * (cx - fx)) * ia1, w1 = ((cx - fx) * (ay - fy) - (cy - fy) * (ax - fx)) * ia1, w2 = 1 - w0 - w1;
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                const double Z = w0 * za + w1 * zb + w2 * zc;
                const size_t zi = size_t(y * S + x);
                if (Z <= zbuf[zi]) continue;
                zbuf[zi] = float(Z);
                const size_t o = zi * 4;
                if (M.kind == 3) { out[o] = toU8(M.c[0] * 255); out[o + 1] = toU8(M.c[1] * 255); out[o + 2] = toU8(M.c[2] * 255); out[o + 3] = 255; continue; }
                double Nx = w0 * nx[ia] + w1 * nx[ib] + w2 * nx[ic], Ny = w0 * ny[ia] + w1 * ny[ib] + w2 * ny[ic], Nz = w0 * nz[ia] + w1 * nz[ib] + w2 * nz[ic];
                double nl = std::sqrt(Nx * Nx + Ny * Ny + Nz * Nz);
                if (nl == 0) nl = 1;
                Nx /= nl; Ny /= nl; Nz /= nl;
                // Face virada pro outro lado (painel fino, interior do anel): luz pelo avesso.
                if (Nz < 0) { Nx = -Nx; Ny = -Ny; Nz = -Nz; }
                const double u = w0 * ua + w1 * ub + w2 * uc, v = w0 * va + w1 * vb + w2 * vc;
                double cR = M.c[0], cG = M.c[1], cB = M.c[2], emR = 0, emG = 0, emB = 0, specK = M.spec, lightFloor = 0;
                bool em = false;
                const double U = u * M.nu, W = v * M.nv;
                const int iu = int(std::floor(U)), iv = int(std::floor(W));
                const double fu = U - iu, fv = W - iv;
                if (M.kind == 0) {
                    if (M.vary != 0) { const double q = 1 + M.vary * (hash2(iu, iv, iSeed + M.id) - 0.5) * 2; cR *= q; cG *= q; cB *= q; }
                    if (detail && M.seam != 0) { const double d = std::min(std::min(fu, 1 - fu), std::min(fv, 1 - fv)); if (d < 0.05) { const double q = 1 - M.seam; cR *= q; cG *= q; cB *= q; } }
                    if (M.win != 0) {
                        const double Uw = U * M.wg, Vw = W * M.wg;
                        const int iw = int(std::floor(Uw)), jw = int(std::floor(Vw));
                        if (hash2(iw, jw, iSeed + 77) < M.win && Uw - iw > 0.2 && Uw - iw < 0.8 && Vw - jw > 0.25 && Vw - jw < 0.75) { em = true; emR = M.wc[0]; emG = M.wc[1]; emB = M.wc[2]; }
                    }
                } else if (M.kind == 1) {
                    const double d = std::min(std::min(fu, 1 - fu), std::min(fv, 1 - fv));
                    if (detail && d < 0.06) { cR = 0.62; cG = 0.68; cB = 0.76; specK = 0.2; }
                } else {
                    // Terra, mar, deserto e gelo em manchas contínuas.
                    const double coarse = hash2(int(std::floor(U / 5)), 3, iSeed), q = hash2(iu, iv, iSeed + 5);
                    const double pickv = coarse * 0.8 + q * 0.2;
                    if (pickv < 0.46) { cR = 0.3; cG = 0.6; cB = 0.3; } else if (pickv < 0.68) { cR = 0.18; cG = 0.42; cB = 0.72; } else if (pickv < 0.88) { cR = 0.72; cG = 0.6; cB = 0.36; } else { cR = 0.95; cG = 0.96; cB = 0.98; }
                    // A luz da estrela chega por dentro do anel, então a terra nunca fica no breu.
                    lightFloor = 0.34;
                }
                const double dl = Nx * L[0] + Ny * L[1] + Nz * L[2], lit = dl > 0 ? dl : 0;
                const double light = std::max(lightFloor, 0.045 + 0.1 * std::max(0.0, Nz) * 0.45 + 0.95 * lit);
                double r = cR * light, g2 = cG * light, b2 = cB * light;
                if (specK > 0 && lit > 0) { const double nh = std::max(0.0, Nx * hx + Ny * hy + Nz * hz), sq = nh * nh, s4 = sq * sq, s8 = s4 * s4, s16 = s8 * s8, s = s16 * s8 * specK; r += s; g2 += s * 0.97; b2 += s * 0.92; }
                if (em) { const double on = 1 - 0.62 * std::min(1.0, lit * 1.5); r = r * (1 - on) + emR * on + r * on * 0.3; g2 = g2 * (1 - on) + emG * on + g2 * on * 0.3; b2 = b2 * (1 - on) + emB * on + b2 * on * 0.3; }
                out[o] = toU8(std::min(255.0, std::pow(r, 0.85) * 255)); out[o + 1] = toU8(std::min(255.0, std::pow(g2, 0.85) * 255)); out[o + 2] = toU8(std::min(255.0, std::pow(b2, 0.85) * 255)); out[o + 3] = 255;
            }
        }
    }
}

// ── no mapa ──
namespace {
const int BUCKETS[] = { 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768 };
QElapsedTimer& clock() { static QElapsedTimer t; if (!t.isValid()) t.start(); return t; }
double frameLeft = 0;
int renderedThisFrame = 0;
std::function<void(uint8_t*, int, int)> toneFn;
QString toneId;
struct Inst { QImage img; int D = 0; double rot = 0, pitch = 0, la = 0; QString tone; };
QHash<QString, Inst>& insts() { static QHash<QString, Inst> h; return h; }
QHash<QString, QImage>& shipCache() { static QHash<QString, QImage> h; return h; }
// Quanto a peça pode ficar parada em cada tamanho antes de redesenhar.
inline double angStep(double r) { return r > 120 ? 0.006 : r > 50 ? 0.014 : r > 18 ? 0.03 : 0.07; }
QImage renderAt(const std::string& type, uint32_t seed, int D, double rot, double pitch, const V3& L)
{
    const int lod = D >= 192 ? 2 : D >= 64 ? 1 : 0, SS = D <= 192 ? 2 : 1, S = D * SS;
    const MeshData& mesh = meshOf(type, seed, lod);
    QImage buf(S, S, QImage::Format_RGBA8888);
    rasterize(mesh, S, rot, pitch, L, buf.bits(), seed & 4095u);
    QImage pm = buf.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (SS > 1) pm = pm.scaled(D, D, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (toneFn) {
        QImage st = pm.convertToFormat(QImage::Format_RGBA8888);
        toneFn(st.bits(), D, D);
        pm = st.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    return pm;
}
} // namespace

void beginFrame(double ms) { frameLeft = double(clock().elapsed()) + ms; renderedThisFrame = 0; }
void setTone(std::function<void(uint8_t*, int, int)> fn, const QString& id) { toneFn = std::move(fn); toneId = id; }
void clearModelCaches() { insts().clear(); shipCache().clear(); }

void drawModel(QPainter& p, const std::string& type, uint32_t seed, double x, double y, double r, double rot, double pitch, double lx, double ly, double alpha, double dpr, const QString& key)
{
    const double want = std::min(768.0, r * 2 * std::min(dpr, 1.5));
    int D = 768;
    for (int v : BUCKETS) if (v >= want) { D = v; break; }
    const QString id = key.isEmpty() ? QStringLiteral("%1:%2:%3").arg(QString::fromStdString(type)).arg(seed).arg(D) : key;
    Inst& e = insts()[id];
    const double P = clampPitch(pitch), La = std::atan2(ly, lx), step = angStep(r);
    const bool stale = e.D != D || e.tone != toneId || std::fabs(e.rot - rot) > step || std::fabs(e.pitch - P) > step || std::fabs(e.la - La) > 0.08;
    // O que mudou de tamanho (chegou perto) passa na frente de quem só girou um pouquinho.
    if (stale && (e.D == 0 || renderedThisFrame == 0 || ((e.D != D || e.tone != toneId) && renderedThisFrame < 3) || double(clock().elapsed()) < frameLeft)) {
        renderedThisFrame++;
        const double vx = lx, vy = -ly, vz = 0.32;
        const double n = h3(vx, vy, vz);
        e.img = renderAt(type, seed, D, rot, P, V3 { vx / n, vy / n, vz / n });
        e.D = D; e.tone = toneId; e.rot = rot; e.pitch = P; e.la = La;
    }
    if (e.img.isNull()) return;
    // como no Lab: o alpha da peça vale sozinho, não multiplica o do pintor
    const double old = p.opacity();
    p.setOpacity(alpha);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(QRectF(x - r, y - r, r * 2, r * 2), e.img);
    p.setOpacity(old);
}
void localToScreen(double px, double py, double pz, double rot, double pitch, double rad, double& ox, double& oy, double& oz)
{
    const double cr = std::cos(rot), sr = std::sin(rot), P = clampPitch(pitch), X = px * cr - py * sr, Y = px * sr + py * cr;
    ox = (X * 0.97) / rad;
    oy = -(Y * std::cos(P) + pz * std::sin(P)) * 0.97 / rad;
    oz = -Y * std::sin(P) + pz * std::cos(P);
}
double radOf(const std::string& type, uint32_t seed) { return meshOf(type, seed, 0).rad; }
const QImage& shipImage(int kind, int D, double heading, double pitch)
{
    const int qh = int(std::floor((heading / TAU) * 32 + 0.5)) & 31, qp = int(std::floor(clampPitch(pitch) / 0.2 + 0.5));
    const QString k = QStringLiteral("%1:%2:%3:%4:%5").arg(kind).arg(D).arg(qh).arg(qp).arg(toneId);
    auto& cache = shipCache();
    auto it = cache.find(k);
    if (it != cache.end()) return it.value();
    const double vx = -0.5, vy = 0.45, vz = 0.74, n = h3(vx, vy, vz);
    return cache.insert(k, renderAt("nave" + std::to_string(kind), 3, D, (qh / 32.0) * TAU, qp * 0.2, V3 { vx / n, vy / n, vz / n })).value();
}
QImage renderModel(const std::string& type, uint32_t seed, int D, double rot, double pitch, const V3& L)
{
    const auto saved = toneFn;
    toneFn = nullptr;
    QImage out = renderAt(type, seed, D, rot, pitch, L);
    toneFn = saved;
    return out;
}

} // namespace galaxy
