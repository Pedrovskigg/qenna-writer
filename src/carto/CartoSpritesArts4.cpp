// Sprites do Construir, leva 4 (porte do sprites4.js do Map Lab, v32 a v36):
//  · castelos humanos (normando, com fosso, château, penhasco) e de mais quatro povos
//    (oriental, deserto, sombrio, gelo), nos mesmos quatro tamanhos do castelo de sempre;
//  · os monumentos grandes: Colosso, Guardiões, Deus Sentado, Catedral, Arco do Triunfo,
//    Mercado e Aqueduto;
//  · as peças dos pincéis novos: o sobrado da Cidade e a paliçada.
// Mesma linguagem das outras levas: faces 3D no mundo, giram com a mesa e pegam a luz da hora.

#include "CartoSprites.h"
#include "CartoSpritesKit.h"

#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <optional>

namespace carto::spr {

using namespace kit;

namespace {

int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
RGB shade(const RGB& c, double k) { return { std::clamp(c[0] * k, 0.0, 255.0), std::clamp(c[1] * k, 0.0, 255.0), std::clamp(c[2] * k, 0.0, 255.0) }; }
// o mesmo sistema do artefato com a origem em outro ponto (mesmo giro)
Loc shifted(const Loc& L, double ox, double oy) { Loc o = L; const P2 q = L.at(ox, oy); o.x = q[0]; o.y = q[1]; return o; }
Faces lifted(Faces f, double dz) { for (Face& q : f) { for (V3& p : q.v) p[2] += dz; q.c[2] += dz; } return f; }

// anel elíptico no sistema local (rx pra frente, ry pro lado), de z0 a z1
Faces ringL(const Loc& L, double lx, double ly, double rx0, double ry0, double rx1, double ry1, double z0, double z1, int n, const RGB& col,
            const std::function<Deco(int, int)>& deco = {})
{
    Faces f;
    const V3 C = L(lx, ly, (z0 + z1) / 2);
    auto P = [&](double a, double rx, double ry, double z) { return L(lx + std::cos(a) * rx, ly + std::sin(a) * ry, z); };
    for (int k = 0; k < n; k++) {
        const double a0 = double(k) / n * M_PI * 2, a1 = double(k + 1) / n * M_PI * 2;
        f.push_back(face({ P(a0, rx0, ry0, z0), P(a1, rx0, ry0, z0), P(a1, rx1, ry1, z1), P(a0, rx1, ry1, z1) }, col, C, deco ? deco(k, n) : Deco()));
    }
    return f;
}
// torneado: perfil [raio, z] girado em volta de (lx, ly) — cúpula, cebola, bacia
using Prof = std::vector<std::array<double, 2>>;
Faces lathe(const Loc& L, double lx, double ly, const Prof& prof, int n, const RGB& col)
{
    Faces f;
    const V3 C = L(lx, ly, (prof.front()[1] + prof.back()[1]) / 2);
    auto P = [&](double a, double r, double z) { return L(lx + std::cos(a) * r, ly + std::sin(a) * r, z); };
    for (size_t i = 0; i + 1 < prof.size(); i++) {
        const double r0 = prof[i][0], z0 = prof[i][1], r1 = prof[i + 1][0], z1 = prof[i + 1][1];
        for (int k = 0; k < n; k++) {
            const double a0 = double(k) / n * M_PI * 2, a1 = double(k + 1) / n * M_PI * 2;
            if (r1 < 0.01) f.push_back(face({ P(a0, r0, z0), P(a1, r0, z0), P(a0, 0, z1) }, col, C));
            else if (r0 < 0.01) f.push_back(face({ P(a0, 0, z0), P(a1, r1, z1), P(a0, r1, z1) }, col, C));
            else f.push_back(face({ P(a0, r0, z0), P(a1, r0, z0), P(a1, r1, z1), P(a0, r1, z1) }, col, C));
        }
    }
    return f;
}
// tronco de pirâmide alinhado ao artefato: meio-lados (hx0, hy0) embaixo e (hx1, hy1) em cima
Faces frustum(const Loc& L, double lx, double ly, double hx0, double hy0, double hx1, double hy1, double z0, double z1, const RGB& col,
              const RGB* topCol = nullptr, const std::function<Deco(int)>& deco = {})
{
    static const int cn[4][2] = { { 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 } };
    Faces f;
    const V3 C = L(lx, ly, (z0 + z1) / 2);
    for (int k = 0; k < 4; k++) {
        const int ax = cn[k][0], ay = cn[k][1], bx = cn[(k + 1) % 4][0], by = cn[(k + 1) % 4][1];
        f.push_back(face({ L(lx + ax * hx0, ly + ay * hy0, z0), L(lx + bx * hx0, ly + by * hy0, z0), L(lx + bx * hx1, ly + by * hy1, z1), L(lx + ax * hx1, ly + ay * hy1, z1) },
                         col, C, deco ? deco(k) : Deco()));
    }
    if (hx1 > 0.01) {
        std::vector<V3> t;
        for (const auto& q : cn) t.push_back(L(lx + q[0] * hx1, ly + q[1] * hy1, z1));
        f.push_back(face(t, topCol ? *topCol : col, C));
    }
    return f;
}
// telhado de beiral curvo (o do pagode): as quinas sobem, o meio do beiral desce; a tampa fecha o miolo
Faces curvedRoof(const Loc& L, double lx, double ly, double E, double I, double zt, double up, double rh, const RGB& col)
{
    static const int cn[4][2] = { { 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 } };
    Faces f;
    const V3 C = L(lx, ly, zt - 2);
    for (int k = 0; k < 4; k++) {
        const double ax = cn[k][0], ay = cn[k][1], bx = cn[(k + 1) % 4][0], by = cn[(k + 1) % 4][1], mx = (ax + bx) / 2, my = (ay + by) / 2;
        const V3 A = L(lx + ax * E, ly + ay * E, zt + up - 0.15), M = L(lx + mx * E * 0.98, ly + my * E * 0.98, zt - 0.15), B = L(lx + bx * E, ly + by * E, zt + up - 0.15);
        const V3 IA = L(lx + ax * I, ly + ay * I, zt + rh), IM = L(lx + mx * I, ly + my * I, zt + rh), IB = L(lx + bx * I, ly + by * I, zt + rh);
        f.push_back(face({ A, M, IM, IA }, col, C, roofDeco, 0.01));
        f.push_back(face({ M, B, IB, IM }, col, C, roofDeco, 0.01));
    }
    std::vector<V3> cap;
    for (const auto& q : cn) cap.push_back(L(lx + q[0] * I, ly + q[1] * I, zt + rh));
    f.push_back(face(cap, col, C, Deco(), 0.012));
    return f;
}
// telhado de quatro águas sobre um retângulo do sistema local (meio-lados hx, hy), cumeeira no lado mais comprido
Faces hipRoof(const Loc& L, double cx, double cy, double hx, double hy, double z, double rh, const RGB& col, Deco deco = {})
{
    const V3 C = L(cx, cy, z - 1);
    const bool along = hy >= hx;
    const double e = std::fabs(hy - hx);
    const V3 A = L(cx - hx, cy - hy, z), B = L(cx + hx, cy - hy, z), Cc = L(cx + hx, cy + hy, z), D = L(cx - hx, cy + hy, z);
    const V3 r0 = along ? L(cx, cy - e, z + rh) : L(cx - e, cy, z + rh), r1 = along ? L(cx, cy + e, z + rh) : L(cx + e, cy, z + rh);
    Faces f;
    auto tri3 = [&](const V3& a, const V3& b, const V3& c) { Face q = face({ a, b, c }, col, C, Deco(), 0.01); q.q = { 0, 1, 2, 2 }; return q; };
    if (along) {
        f.push_back(face({ B, Cc, r1, r0 }, col, C, deco, 0.01)); f.push_back(face({ D, A, r0, r1 }, col, C, deco, 0.01));
        f.push_back(tri3(A, B, r0)); f.push_back(tri3(Cc, D, r1));
    } else {
        f.push_back(face({ A, B, r1, r0 }, col, C, deco, 0.01)); f.push_back(face({ Cc, D, r0, r1 }, col, C, deco, 0.01));
        f.push_back(tri3(B, Cc, r1)); f.push_back(tri3(D, A, r0));
    }
    return f;
}
// cristal: prisma de n lados de p0 na direção d (comprimento len, raio r) com a ponta
Faces crystal(const V3& p0, const V3& d, double len, double r, int n, const RGB& col, double tip = 1.6)
{
    const V3 D = norm(d);
    V3 u = { D[1], -D[0], 0 };
    if (std::hypot(u[0], u[1]) < 0.1) u = { 1, 0, 0 };
    u = norm(u);
    const V3 v = cross(D, u);
    auto at = [&](double t, double a, double rr) {
        return V3{ p0[0] + D[0] * t + (u[0] * std::cos(a) + v[0] * std::sin(a)) * rr, p0[1] + D[1] * t + (u[1] * std::cos(a) + v[1] * std::sin(a)) * rr,
                   p0[2] + D[2] * t + (u[2] * std::cos(a) + v[2] * std::sin(a)) * rr };
    };
    const V3 top = add3(p0, mul3(D, len + r * tip)), C = add3(p0, mul3(D, len / 2));
    Faces f;
    for (int k = 0; k < n; k++) {
        const double a0 = double(k) / n * M_PI * 2, a1 = double(k + 1) / n * M_PI * 2;
        const RGB cc = k % 2 ? col : shade(col, 1.07);
        f.push_back(face({ at(0, a0, r), at(0, a1, r), at(len, a1, r), at(len, a0, r) }, cc, C));
        f.push_back(face({ at(len, a0, r), at(len, a1, r), top }, shade(cc, 1.05), C, Deco(), 0.01));
    }
    return f;
}
// tubo que afina de r0 (em p0) a r1 (em p1): braço, coxa, canela, cabo
Faces limb(const V3& p0, const V3& p1, double r0, double r1, int n, const RGB& col)
{
    const V3 D = norm(sub3(p1, p0));
    V3 u = { D[1], -D[0], 0 };
    if (std::hypot(u[0], u[1]) < 0.1) u = { 1, 0, 0 };
    u = norm(u);
    const V3 v = cross(D, u);
    auto at = [&](const V3& p, double a, double r) { return V3{ p[0] + (u[0] * std::cos(a) + v[0] * std::sin(a)) * r, p[1] + (u[1] * std::cos(a) + v[1] * std::sin(a)) * r, p[2] + (u[2] * std::cos(a) + v[2] * std::sin(a)) * r }; };
    const V3 C = mul3(add3(p0, p1), 0.5);
    Faces f;
    for (int k = 0; k < n; k++) {
        const double a0 = double(k) / n * M_PI * 2, a1 = double(k + 1) / n * M_PI * 2;
        f.push_back(face({ at(p0, a0, r0), at(p0, a1, r0), at(p1, a1, r1), at(p1, a0, r1) }, col, C));
    }
    return f;
}
// disco (escudo): face da frente, de trás, a borda e o anel polido por dentro
Faces disc(const V3& c, const V3& nrm, double r, double depth, int n, const RGB& col, const RGB& rimCol)
{
    const V3 N = norm(nrm);
    V3 u = { N[1], -N[0], 0 };
    if (std::hypot(u[0], u[1]) < 0.1) u = { 1, 0, 0 };
    u = norm(u);
    const V3 v = cross(N, u);
    auto ring = [&](double off, double rr) {
        std::vector<V3> o;
        for (int k = 0; k < n; k++) { const double a = double(k) / n * M_PI * 2; o.push_back({ c[0] + N[0] * off + (u[0] * std::cos(a) + v[0] * std::sin(a)) * rr, c[1] + N[1] * off + (u[1] * std::cos(a) + v[1] * std::sin(a)) * rr, c[2] + N[2] * off + (u[2] * std::cos(a) + v[2] * std::sin(a)) * rr }); }
        return o;
    };
    const std::vector<V3> F = ring(depth, r), B = ring(-depth * 0.5, r), I = ring(depth * 1.15, r * 0.82), I2 = ring(depth * 1.15, r * 0.7);
    Faces f;
    f.push_back(face(F, col, c));
    f.push_back(face(std::vector<V3>(B.rbegin(), B.rend()), rimCol, c));
    for (int k = 0; k < n; k++) f.push_back(face({ B[k], B[(k + 1) % n], F[(k + 1) % n], F[k] }, rimCol, c));
    for (int k = 0; k < n; k++) f.push_back(face({ I[k], I[(k + 1) % n], I2[(k + 1) % n], I2[k] }, rimCol, c, Deco(), 0.01));
    return f;
}
// chama: dois cones de fogo e o brilho em volta (de dia também; é fogo)
void flame(QPainter& p, const Cam& cam, const V3& at, double s)
{
    Faces f = coneFaces(at[0], at[1], 0.55 * s, at[2], 1.5 * s, 6, { 255, 150, 50 });
    append(f, coneFaces(at[0], at[1], 0.3 * s, at[2] + 0.1 * s, 1.0 * s, 6, { 255, 226, 120 }));
    drawFaces(p, f, cam);
    glowAt(p, cam, { at[0], at[1], at[2] + 0.6 * s }, 1.6 * s, { 255, 170, 70 }, 0.45);
}
// janelas: ogivais (catedral), em arco redondo (palácio do deserto), fileiras em arco (castelos humanos)
Deco lancet(int n, RGB col, double t0 = 0.22, double t1 = 0.78)
{
    return [=](QPainter& q, const PtFn& pt, double kk) {
        for (int i = 0; i < n; i++) {
            const double s = (i + 0.5) / n, w = 0.32 / n;
            QPainterPath pa;
            pa.moveTo(pt(s - w, t0)); pa.lineTo(pt(s - w, t1 - 0.12)); pa.quadTo(pt(s - w, t1), pt(s, t1 + 0.04));
            pa.quadTo(pt(s + w, t1), pt(s + w, t1 - 0.12)); pa.lineTo(pt(s + w, t0)); pa.closeSubpath();
            q.fillPath(pa, rgb(col, kk, 0.9));
        }
    };
}
Deco roundWin(int n, RGB col, double t0 = 0.25, double t1 = 0.7)
{
    return [=](QPainter& q, const PtFn& pt, double kk) {
        for (int i = 0; i < n; i++) {
            const double s = (i + 0.5) / n, w = 0.28 / n;
            QPainterPath pa;
            pa.moveTo(pt(s - w, t0)); pa.lineTo(pt(s - w, t1 - 0.1));
            for (int j = 0; j <= 6; j++) { const double a = M_PI - j / 6.0 * M_PI; pa.lineTo(pt(s + std::cos(a) * w, t1 - 0.1 + std::sin(a) * 0.12)); }
            pa.lineTo(pt(s + w, t0)); pa.closeSubpath();
            q.fillPath(pa, rgb(col, kk, 0.9));
        }
    };
}
Deco windowRows(int rows, int n, RGB col = DARK)
{
    return [=](QPainter& q, const PtFn& pt, double kk) {
        for (int r = 0; r < rows; r++) for (int i = 0; i < n; i++) {
            const double s = (i + 0.5) / n, t0 = (r + 0.3) / rows, t1 = t0 + 0.42 / rows;
            QPainterPath pa;
            pa.moveTo(pt(s - 0.22 / n, t0)); pa.lineTo(pt(s - 0.22 / n, t1 - 0.06 / rows)); pa.quadTo(pt(s, t1 + 0.06 / rows), pt(s + 0.22 / n, t1 - 0.06 / rows));
            pa.lineTo(pt(s + 0.22 / n, t0)); pa.closeSubpath();
            q.fillPath(pa, rgb(col, kk, 0.85));
        }
    };
}
Deco linesDeco(std::vector<std::array<double, 4>> ls, QColor c, double w)
{
    return [=](QPainter& q, const PtFn& pt, double) { strokeLines(q, c, w); for (const auto& l : ls) line(q, pt(l[0], l[1]), pt(l[2], l[3])); };
}

// ── figura de pé pra estátua: pés em z0, altura H; braços = [cotovelo, mão] em frações de H; robe = saia no lugar das pernas ──
struct Arm { std::array<double, 3> el, ha; };
struct FigOpt { double stance = 0.06; bool robe = false; RGB robeCol{}; std::optional<Arm> armL, armR; double yaw = 0; };
Faces figure(const Loc& L, double z0, double H, const RGB& col, const FigOpt& o)
{
    auto P = [&](double x, double y, double t) { return L(x * H, y * H, z0 + t * H); };
    auto ring = [&](double rx0, double ry0, double rx1, double ry1, double t0, double t1, int n = 8, const RGB* c = nullptr, double cx = 0) {
        return ringL(L, cx * H, 0, rx0 * H, ry0 * H, rx1 * H, ry1 * H, z0 + t0 * H, z0 + t1 * H, n, c ? *c : col);
    };
    Faces f;
    if (o.robe) {
        append(f, ring(0.13, 0.17, 0.075, 0.11, 0, 0.5, 12, &o.robeCol));
        append(f, ring(0.075, 0.11, 0.07, 0.105, 0.5, 0.56, 10, &o.robeCol));
    } else {
        const double st = o.stance;
        for (int s : { -1, 1 }) {
            const V3 hip = P(0, s * 0.055, 0.5), knee = P(0.015, s * (st * 0.8 + 0.02), 0.27), foot = P(0, s * st, 0.03);
            append(f, beamFaces(hip, knee, 0.07 * H, 0.075 * H, col)); append(f, beamFaces(knee, foot, 0.06 * H, 0.065 * H, col));
            const V3 fp = P(0.03, s * st, 0);
            append(f, boxFaces(fp[0], fp[1], o.yaw, 0.12 * H, 0.06 * H, z0, z0 + 0.03 * H, col));
        }
        append(f, ring(0.07, 0.105, 0.065, 0.1, 0.47, 0.56));
    }
    append(f, ring(0.065, 0.1, 0.075, 0.13, 0.56, 0.76)); append(f, ring(0.075, 0.13, 0.045, 0.07, 0.76, 0.815));
    append(f, ring(0.028, 0.028, 0.028, 0.028, 0.81, 0.86, 6));
    append(f, ring(0.045, 0.04, 0.055, 0.048, 0.855, 0.92, 8, nullptr, 0.005)); append(f, ring(0.055, 0.048, 0.024, 0.02, 0.92, 0.99, 8, nullptr, 0.005));
    for (int s : { -1, 1 }) {
        const std::optional<Arm>& arm = s < 0 ? o.armL : o.armR;
        const std::array<double, 3> sh = { 0, s * 0.135, 0.79 };
        const std::array<double, 3> el = arm ? arm->el : std::array<double, 3>{ 0.02, s * 0.165, 0.62 }, ha = arm ? arm->ha : std::array<double, 3>{ 0.035, s * 0.155, 0.47 };
        append(f, beamFaces(P(sh[0], sh[1], sh[2]), P(el[0], el[1], el[2]), 0.05 * H, 0.055 * H, col));
        append(f, beamFaces(P(el[0], el[1], el[2]), P(ha[0], ha[1], ha[2]), 0.045 * H, 0.045 * H, col));
        const V3 hp = P(ha[0], ha[1], ha[2]);
        append(f, boxFaces(hp[0], hp[1], o.yaw, 0.06 * H, 0.05 * H, z0 + (ha[2] - 0.035) * H, z0 + (ha[2] + 0.015) * H, col));
    }
    return f;
}

// ════════════════════════ monumentos ════════════════════════

// ── Colosso: guerreiro de bronze com verdete num pedestal de friso; elmo de crista, couraça, saiote de tiras,
// a tocha erguida e o escudo na outra mão, a capa caindo atrás ──
constexpr double kColZ = 3.6, kColH = 24;
void drawColossus(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const double H = kColH, Z = kColZ;
    static const RGB pats[3] = { { 100, 150, 128 }, { 116, 152, 120 }, { 128, 142, 106 } };
    const RGB pat = pats[static_cast<int>(std::floor(b.v * 3)) % 3];
    const RGB bronze = pat, dark = shade(pat, 0.8), cuir = shade(pat, 1.08), gold = { 190, 150, 88 }, gold2 = { 206, 170, 104 };
    auto P = [&](double x, double y, double t) { return L(x * H, y * H, Z + t * H); };
    auto R = [&](double r) { return r * H; };
    auto ball = [&](const V3& at, double r, const RGB& col, double rz = -1) { return blobFaces(at[0], at[1], at[2], R(r), R(rz < 0 ? r : rz), 10, 5, col, 7, 0); };
    Faces f;
    // pedestal: base, degrau, o bloco com friso e a placa, a cornija
    append(f, boxFaces(b.x, b.y, b.yaw, 9.2, 9.2, 0, 0.8, { 172, 164, 150 }));
    append(f, boxFaces(b.x, b.y, b.yaw, 7.8, 7.8, 0.8, 1.5, { 186, 178, 162 }));
    append(f, boxFaces(b.x, b.y, b.yaw, 6.4, 6.4, 1.5, 3.1, { 208, 200, 184 }, nullptr, [gold](int k, double) -> Deco {
        return [k, gold](QPainter& q, const PtFn& pt, double kk) {
            strokeLines(q, rgb({ 120, 108, 88 }, kk, 0.6), 0.7);
            line(q, pt(0, 0.82), pt(1, 0.82)); line(q, pt(0, 0.18), pt(1, 0.18));
            for (int i = 0; i < 6; i++) { const double s0 = (i + 0.5) / 6; quad(q, pt, s0 - 0.04, s0 + 0.04, 0.84, 0.96, rgb({ 150, 138, 116 }, kk, 0.7)); }
            if (k == 1) {
                quad(q, pt, 0.3, 0.7, 0.32, 0.66, rgb(gold, kk));
                strokeLines(q, rgb({ 110, 80, 40 }, kk, 0.6), 0.7);
                line(q, pt(0.36, 0.44), pt(0.64, 0.44)); line(q, pt(0.36, 0.54), pt(0.64, 0.54));
            }
        };
    }));
    append(f, boxFaces(b.x, b.y, b.yaw, 7.0, 7.0, 3.1, Z, { 198, 190, 174 }));
    // pernas: coxa e canela afinando, joelho redondo, caneleira e sandália
    for (int s : { -1, 1 }) {
        const V3 hip = P(0, s * 0.055, 0.47), knee = P(0.02, s * 0.085, 0.265), ank = P(0, s * 0.095, 0.045);
        append(f, limb(hip, knee, R(0.054), R(0.04), 10, bronze)); append(f, ball(knee, 0.041, bronze));
        append(f, limb(knee, ank, R(0.039), R(0.025), 10, bronze));
        append(f, limb(P(0.021, s * 0.087, 0.24), P(0.006, s * 0.093, 0.09), R(0.044), R(0.031), 10, gold));
        const RGB sandal = shade(gold, 0.85);
        append(f, frustum(L, R(0.028), s * R(0.096), R(0.058), R(0.026), R(0.045), R(0.02), Z, Z + R(0.032), sandal));
        append(f, ball(P(0.002, s * 0.095, 0.05), 0.027, bronze, 0.022));
    }
    // saiote de tiras (pteruges), alternando o tom
    {
        Faces sk = ringL(L, 0, 0, R(0.072), R(0.106), R(0.098), R(0.138), Z + R(0.53), Z + R(0.38), 18, gold);
        for (size_t i = 0; i < sk.size(); i++) {
            sk[i].col = i % 2 ? gold : shade(gold, 0.82);
            sk[i].deco = linesDeco({ { 0.5, 0.15, 0.5, 1 } }, QColor(60, 40, 20, 115), 0.6);
        }
        append(f, std::move(sk));
    }
    // tronco: a couraça com o desenho do peito e da barriga; cinto
    static const double T[6][3] = { { 0.47, 0.068, 0.1 }, { 0.53, 0.062, 0.092 }, { 0.6, 0.068, 0.106 }, { 0.68, 0.082, 0.126 }, { 0.745, 0.077, 0.136 }, { 0.79, 0.05, 0.085 } };
    const std::function<Deco(int, int)> muscle = [](int k, int n) -> Deco {
        if (k != 0 && k != n - 1) return Deco();
        return [k](QPainter& q, const PtFn& pt, double) {
            strokeLines(q, QColor(40, 60, 50, 140), 0.8);
            QPainterPath pa; pa.moveTo(pt(k ? 0.1 : 0.9, 0.55)); pa.quadTo(pt(0.5, 0.25), pt(k ? 0.95 : 0.05, 0.6)); q.drawPath(pa);
            line(q, pt(k ? 0.98 : 0.02, 0), pt(k ? 0.98 : 0.02, 1));
        };
    };
    for (int i = 0; i < 5; i++) {
        const bool c = i >= 1 && i <= 3;
        append(f, ringL(L, 0, 0, R(T[i][1]), R(T[i][2]), R(T[i + 1][1]), R(T[i + 1][2]), Z + R(T[i][0]), Z + R(T[i + 1][0]), 12, c ? cuir : bronze, c ? muscle : std::function<Deco(int, int)>()));
    }
    append(f, ringL(L, 0, 0, R(0.07), R(0.1), R(0.07), R(0.1), Z + R(0.52), Z + R(0.545), 12, gold2));
    for (int s : { -1, 1 }) append(f, ball(P(0, s * 0.132, 0.77), 0.045, gold, 0.036));
    // pescoço, cabeça e o elmo coríntio com a crista
    append(f, limb(P(0, 0, 0.785), P(0.005, 0, 0.855), R(0.027), R(0.024), 8, bronze));
    append(f, ball(P(0.008, 0, 0.895), 0.044, bronze, 0.054));
    {
        const V3 hc = P(0.002, 0, 0.9);
        Faces helm = blobFaces(hc[0], hc[1], hc[2], R(0.052), R(0.056), 12, 6, gold, 9, 0);
        const double zc = Z + R(0.888);
        for (Face& q : helm) if (std::all_of(q.v.begin(), q.v.end(), [zc](const V3& v) { return v[2] >= zc; })) f.push_back(std::move(q));
    }
    for (int s : { -1, 1 }) f.push_back(face({ P(0.04, s * 0.03, 0.9), P(0.0, s * 0.05, 0.9), P(0.0, s * 0.046, 0.855), P(0.035, s * 0.024, 0.858) }, gold, P(0, 0, 0.88)));
    append(f, limb(P(0.052, 0, 0.91), P(0.056, 0, 0.87), R(0.007), R(0.006), 4, gold));
    {
        auto arc = [&](double r) { std::vector<V3> o; for (int i = 0; i <= 9; i++) { const double a = 0.35 + i / 9.0 * 2.5; o.push_back(P(-0.004 + std::cos(a) * r, 0, 0.905 + std::sin(a) * r)); } return o; };
        const std::vector<V3> crest = arc(0.068), plume = arc(0.098);
        for (size_t i = 0; i + 1 < crest.size(); i++) {
            Face q = face({ crest[i], crest[i + 1], plume[i + 1], plume[i] }, { 150, 60, 46 }, P(0, -0.2, 0.9), linesDeco({ { 0.3, 0, 0.3, 1 }, { 0.7, 0, 0.7, 1 } }, QColor(70, 20, 14, 128), 0.6));
            q.flat = true;
            f.push_back(std::move(q));
        }
    }
    // braço direito erguido com a tocha
    const V3 sh = P(0, 0.14, 0.775), el = P(0.035, 0.195, 0.9), ha = P(0.05, 0.175, 1.025);
    append(f, limb(sh, el, R(0.035), R(0.028), 9, bronze)); append(f, ball(el, 0.029, bronze));
    append(f, limb(el, ha, R(0.028), R(0.021), 9, bronze)); append(f, ball(ha, 0.03, bronze));
    append(f, limb(P(0.05, 0.175, 0.99), P(0.05, 0.175, 1.13), R(0.013), R(0.017), 6, dark));
    append(f, lathe(L, 0.05 * H, 0.175 * H, { { R(0.018), Z + R(1.12) }, { R(0.045), Z + R(1.15) }, { R(0.05), Z + R(1.165) } }, 10, gold2));
    // braço esquerdo com o escudo redondo
    const V3 sh2 = P(0, -0.14, 0.775), el2 = P(0.05, -0.2, 0.64), ha2 = P(0.12, -0.17, 0.6);
    append(f, limb(sh2, el2, R(0.035), R(0.028), 9, bronze)); append(f, ball(el2, 0.029, bronze));
    append(f, limb(el2, ha2, R(0.028), R(0.021), 9, bronze));
    const double yw = b.yaw;
    const V3 nrm = { std::cos(yw) * 1 - std::sin(yw) * -0.55, std::sin(yw) * 1 + std::cos(yw) * -0.55, 0.05 }, scn = P(0.15, -0.2, 0.62);
    append(f, disc(scn, nrm, R(0.105), R(0.012), 18, cuir, gold));
    const double nl = std::hypot(nrm[0], nrm[1], nrm[2]);
    append(f, blobFaces(scn[0] + nrm[0] / nl * R(0.018), scn[1] + nrm[1] / nl * R(0.018), scn[2], R(0.026), R(0.026), 8, 4, gold2, 3, 0));
    // capa com dobras: dos ombros até o tornozelo, abrindo
    const V3 C0 = P(0, 0, 0.5);
    const RGB capeCol = shade(pat, 0.88);
    const int cols = 7;
    for (int i = 0; i < cols - 1; i++) {
        const double y0 = -1 + double(i) / (cols - 1) * 2, y1 = -1 + double(i + 1) / (cols - 1) * 2;
        auto fold = [](int j) { return j % 2 ? 0.022 : 0.0; };
        f.push_back(face({ P(-0.07, y0 * 0.125, 0.79), P(-0.07, y1 * 0.125, 0.79), P(-0.15 - fold(i + 1), y1 * 0.19, 0.12), P(-0.15 - fold(i), y0 * 0.19, 0.12) },
                         i % 2 ? capeCol : shade(capeCol, 0.92), C0));
    }
    drawFaces(p, f, cam);
    flame(p, cam, P(0.05, 0.175, 1.165), 1.8);
}

// ── Guardiões: dois reis de pedra de manto, a mão erguida (o portão do rio), cada um no seu rochedo ──
void drawGuardians(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const RGB stone = { 176, 172, 162 }, rock = { 140, 136, 128 }, rockTop = { 154, 150, 140 };
    const double H = 16, Z = 2.4;
    Faces f;
    for (int s : { -1, 1 }) {
        const Loc Lg = shifted(L, 0, s * 5.6);
        append(f, frustum(Lg, 0, 0, 2.8, 2.6, 2.2, 2.0, 0, Z, rock, &rockTop, [](int) { return linesDeco({ { 0.2, 0, 0.35, 1 }, { 0.7, 0, 0.62, 1 } }, QColor(70, 64, 58, 102), 0.6); }));
        // a mão de fora erguida pra frente, a de dentro segura o machado
        const Arm up = { { 0.1, s * 0.17, 0.88 }, { 0.2, s * 0.18, 1.0 } }, down = { { 0.04, -s * 0.16, 0.62 }, { 0.08, -s * 0.14, 0.52 } };
        FigOpt o; o.robe = true; o.robeCol = shade(stone, 0.94); o.yaw = b.yaw;
        if (s < 0) { o.armL = up; o.armR = down; } else { o.armL = down; o.armR = up; }
        append(f, figure(Lg, Z, H, stone, o));
        auto P = [&](double x, double y, double t) { return Lg(x * H, y * H, Z + t * H); };
        // elmo com coroa e o machado
        append(f, ringL(Lg, 0.005 * H, 0, 0.06 * H, 0.054 * H, 0.05 * H, 0.045 * H, Z + 0.93 * H, Z + 1.0 * H, 8, shade(stone, 1.06)));
        const P2 cr = Lg.at(0.005 * H, 0);
        append(f, coneFaces(cr[0], cr[1], 0.045 * H, Z + 1.0 * H, 0.09 * H, 8, shade(stone, 1.06)));
        append(f, beamFaces(P(0.09, -s * 0.14, 0.04), P(0.09, -s * 0.14, 0.78), 0.022 * H, 0.022 * H, shade(stone, 0.9)));
        const V3 ax = P(0.09, -s * 0.14 - s * 0.03, 0);
        append(f, boxFaces(ax[0], ax[1], b.yaw, 0.1 * H, 0.012 * H * 4, Z + 0.68 * H, Z + 0.8 * H, shade(stone, 1.1)));
    }
    drawFaces(p, f, cam);
}

// ── Deus Sentado: gigante de arenito no trono, mãos nos joelhos, toucado listrado; braseiros na frente ──
void drawSeatedGod(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    static const RGB sands[3] = { { 216, 186, 138 }, { 200, 176, 140 }, { 190, 196, 190 } };
    const RGB sand = sands[static_cast<int>(std::floor(b.v * 3)) % 3], cloth = { 64, 104, 156 }, gold = { 226, 186, 90 };
    Faces f = frustum(L, 0, 0, 5.6, 5.6, 5.2, 5.2, 0, 0.7, { 180, 164, 132 });
    // trono: assento e encosto alto com relevo
    const P2 seat = L.at(-0.6, 0), back = L.at(-3.2, 0);
    append(f, boxFaces(seat[0], seat[1], b.yaw, 4.6, 6.6, 0.7, 4.6, shade(sand, 0.92)));
    append(f, boxFaces(back[0], back[1], b.yaw, 1.2, 7.2, 0.7, 14.2, shade(sand, 0.9), nullptr, [](int k, double) -> Deco {
        if (k != 1) return Deco();
        return [](QPainter& q, const PtFn& pt, double kk) { strokeLines(q, rgb(DARK, kk, 0.35), 0.7); for (int i = 1; i < 5; i++) line(q, pt(0.1, i / 5.0), pt(0.9, i / 5.0)); };
    }));
    // pernas: coxas pra frente, canelas descendo, pés na base
    for (int s : { -1, 1 }) {
        append(f, beamFaces(L(-1.4, s * 1.0, 5.2), L(1.5, s * 1.05, 5.2), 1.5, 1.4, sand));
        append(f, beamFaces(L(1.6, s * 1.05, 5.4), L(1.7, s * 1.05, 0.9), 1.2, 1.3, sand));
        const P2 ft = L.at(2.2, s * 1.05);
        append(f, boxFaces(ft[0], ft[1], b.yaw, 1.6, 1.2, 0.7, 1.3, sand));
    }
    // tronco, ombros, pescoço e cabeça
    append(f, ringL(L, -1.2, 0, 1.2, 1.7, 1.1, 1.9, 4.8, 8.4, 10, sand)); append(f, ringL(L, -1.2, 0, 1.1, 1.9, 0.6, 0.9, 8.4, 9.2, 10, sand));
    append(f, ringL(L, -1.1, 0, 0.42, 0.42, 0.42, 0.42, 9.1, 9.8, 6, sand));
    append(f, ringL(L, -1.0, 0, 0.75, 0.66, 0.85, 0.75, 9.7, 10.8, 8, sand)); append(f, ringL(L, -1.0, 0, 0.85, 0.75, 0.45, 0.4, 10.8, 11.7, 8, sand));
    // toucado (as abas listradas caindo nos ombros) e a coroa alta
    const Deco stripes = [gold](QPainter& q, const PtFn& pt, double) { strokeLines(q, rgb(gold, 1, 0.9), 0.8); for (int i = 1; i < 5; i++) line(q, pt(0, i / 5.0), pt(1, i / 5.0)); };
    for (int s : { -1, 1 }) { Face q = face({ L(-0.6, s * 0.7, 11.2), L(-1.5, s * 0.75, 11.2), L(-1.6, s * 1.35, 8.6), L(-0.4, s * 1.25, 8.6) }, cloth, L(-1.0, 0, 10), stripes); q.flat = true; f.push_back(std::move(q)); }
    append(f, ringL(L, -1.05, 0, 0.8, 0.72, 0.62, 0.55, 11.4, 12.8, 8, cloth)); append(f, ringL(L, -1.05, 0, 0.62, 0.55, 0.3, 0.28, 12.8, 13.6, 8, gold));
    // braços: ombro, cotovelo, mão no joelho
    for (int s : { -1, 1 }) { append(f, beamFaces(L(-1.1, s * 1.85, 8.8), L(-0.5, s * 2.0, 6.4), 0.9, 0.9, sand)); append(f, beamFaces(L(-0.5, s * 2.0, 6.4), L(1.2, s * 1.3, 5.9), 0.8, 0.8, sand)); }
    // braseiros
    for (int s : { -1, 1 }) {
        append(f, beamFaces(L(4.0, s * 3.4, 0.7), L(4.0, s * 3.4, 2.0), 0.35, 0.35, { 110, 90, 64 }));
        append(f, lathe(L, 4.0, s * 3.4, { { 0.2, 2.0 }, { 0.7, 2.3 }, { 0.8, 2.6 } }, 8, { 150, 110, 64 }));
    }
    drawFaces(p, f, cam);
    for (int s : { -1, 1 }) flame(p, cam, L(4.0, s * 3.4, 2.5), 0.8);
}

// ── Catedral: nave comprida com contrafortes, transepto, abside, duas torres com flecha na fachada ──
void drawCathedral(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    static const RGB stones[3] = { { 208, 200, 184 }, { 196, 190, 182 }, { 214, 196, 168 } };
    const RGB stone = stones[static_cast<int>(std::floor(b.v * 3)) % 3], roof = { 86, 92, 108 }, glass = { 70, 84, 130 }, top = shade(stone, 0.92);
    Faces f;
    // uma "casa" de pedra: paredes w × d até wh, telhado de duas águas até wh + rh; cumeeira no x local (ou no y)
    auto hall = [&](double cx, double cy, double w, double d, double wh, double rh, bool alongY, Deco side) {
        const Loc Lc = shifted(L, cx, cy);
        const double hw = w / 2, hd = d / 2;
        const V3 A = Lc(-hw, -hd, 0), B = Lc(hw, -hd, 0), Cc = Lc(hw, hd, 0), D = Lc(-hw, hd, 0), ctr = Lc(0, 0, wh / 2);
        auto up = [](const V3& q, double z) { return V3{ q[0], q[1], z }; };
        auto pent = [&](const V3& a, const V3& bb, const V3& r, Deco dc) { Face q = face({ a, bb, up(bb, wh), r, up(a, wh) }, stone, ctr, std::move(dc)); q.q = { 0, 1, 2, 4 }; return q; };
        if (!alongY) {
            const V3 R1 = Lc(-hw, 0, wh + rh), R2 = Lc(hw, 0, wh + rh);
            f.push_back(face({ A, B, up(B, wh), up(A, wh) }, stone, ctr, side)); f.push_back(face({ Cc, D, up(D, wh), up(Cc, wh) }, stone, ctr, side));
            f.push_back(pent(B, Cc, R2, Deco())); f.push_back(pent(D, A, R1, Deco()));
            f.push_back(face({ up(A, wh - 0.05), up(B, wh - 0.05), R2, R1 }, roof, ctr, roofDeco, 0.01)); f.push_back(face({ up(Cc, wh - 0.05), up(D, wh - 0.05), R1, R2 }, roof, ctr, roofDeco, 0.01));
        } else {
            const V3 R1 = Lc(0, -hd, wh + rh), R2 = Lc(0, hd, wh + rh);
            const Deco rose = [glass](QPainter& q, const PtFn& pt, double kk) {
                const QPointF c = pt(0.5, 0.75), e = pt(0.6, 0.75);
                const double r = std::max(1.0, std::hypot(e.x() - c.x(), e.y() - c.y()));
                q.setPen(Qt::NoPen); q.setBrush(rgb(glass, kk, 0.85)); q.drawEllipse(c, r, r);
            };
            f.push_back(face({ B, Cc, up(Cc, wh), up(B, wh) }, stone, ctr, side)); f.push_back(face({ D, A, up(A, wh), up(D, wh) }, stone, ctr, side));
            f.push_back(pent(A, B, R1, rose)); f.push_back(pent(Cc, D, R2, Deco()));
            f.push_back(face({ up(B, wh - 0.05), up(Cc, wh - 0.05), R2, R1 }, roof, ctr, roofDeco, 0.01)); f.push_back(face({ up(D, wh - 0.05), up(A, wh - 0.05), R1, R2 }, roof, ctr, roofDeco, 0.01));
        }
    };
    hall(-1.4, 0, 10.4, 5, 6.6, 3.8, false, lancet(5, glass));
    hall(-3.0, 0, 3.8, 10.4, 6.2, 3.4, true, lancet(2, glass));
    // abside: meio polígono atrás da nave
    {
        const int n = 6; const double r = 2.5, cx = -6.6;
        const V3 ctr = L(cx, 0, 3);
        auto P = [&](double a, double rr, double z) { return L(cx + std::cos(a) * rr, std::sin(a) * rr, z); };
        for (int k = 0; k < n; k++) {
            const double a0 = M_PI / 2 + double(k) / n * M_PI, a1 = M_PI / 2 + double(k + 1) / n * M_PI;
            f.push_back(face({ P(a0, r, 0), P(a1, r, 0), P(a1, r, 6.0), P(a0, r, 6.0) }, stone, ctr, lancet(1, glass)));
            f.push_back(face({ P(a0, r + 0.1, 5.95), P(a1, r + 0.1, 5.95), L(cx + 0.02, 0, 9.0) }, roof, ctr, Deco(), 0.01));
        }
    }
    // contrafortes com arcobotante
    for (double x : { -5.6, -1.0, 1.4 }) for (int s : { -1, 1 }) {
        const P2 q = L.at(x, s * 3.9);
        append(f, boxFaces(q[0], q[1], b.yaw, 0.7, 1.0, 0, 4.6, top));
        append(f, beamFaces(L(x, s * 3.9, 4.4), L(x, s * 2.6, 6.2), 0.35, 0.3, top));
        append(f, coneFaces(q[0], q[1], 0.4, 4.6, 1.1, 4, top));
    }
    // fachada: duas torres com flecha, portal e a rosácea no meio
    for (int s : { -1, 1 }) {
        const P2 t = L.at(4.4, s * 2.9);
        append(f, boxFaces(t[0], t[1], b.yaw, 2.6, 2.6, 0, 12.4, stone, &top, [glass](int k, double) -> Deco { return (k == 0 || k == 1 || k == 2) ? lancet(1, glass, 0.62, 0.86) : Deco(); }));
        append(f, taperFaces(t[0], t[1], 1.15, 1.0, 12.4, 13.4, 8, top, [](int i) { return i % 2 ? slitDeco(1) : Deco(); }));
        append(f, coneFaces(t[0], t[1], 1.05, 13.4, 6.6, 8, roof));
        for (int dx : { 1, -1 }) for (int dy : { 1, -1 }) { const P2 q = L.at(4.4 + dx * 1.15, s * 2.9 + dy * 1.15); append(f, coneFaces(q[0], q[1], 0.22, 12.4, 1.4, 4, top)); }
    }
    {
        const P2 fc = L.at(4.6, 0);
        append(f, boxFaces(fc[0], fc[1], b.yaw, 1.2, 3.4, 0, 9.0, stone, &top, [stone, glass](int k, double) -> Deco {
            if (k != 1) return Deco();
            return [stone, glass](QPainter& q, const PtFn& pt, double kk) {
                QPainterPath door;
                door.moveTo(pt(0.32, 0)); door.lineTo(pt(0.32, 0.26)); door.quadTo(pt(0.32, 0.38), pt(0.5, 0.42)); door.quadTo(pt(0.68, 0.38), pt(0.68, 0.26)); door.lineTo(pt(0.68, 0)); door.closeSubpath();
                q.fillPath(door, rgb({ 60, 44, 34 }, kk));
                const QPointF c = pt(0.5, 0.7), e = pt(0.78, 0.7);
                const double r = std::max(1.2, std::hypot(e.x() - c.x(), e.y() - c.y()));
                q.setPen(Qt::NoPen); q.setBrush(rgb(glass, kk, 0.95)); q.drawEllipse(c, r, r);
                q.setPen(pen(rgb(stone, kk), std::max(0.5, r * 0.12))); q.setBrush(Qt::NoBrush);
                for (int i = 0; i < 8; i++) { const double a = i / 8.0 * M_PI * 2; q.drawLine(c, QPointF(c.x() + std::cos(a) * r, c.y() + std::sin(a) * r)); }
                q.drawEllipse(c, r * 0.35, r * 0.35);
            };
        }));
    }
    f.push_back(face({ L(5.2, -1.7, 9.0), L(5.2, 1.7, 9.0), L(5.2, 0, 10.8) }, stone, L(4.6, 0, 9)));
    // flecha do cruzeiro
    const P2 cr = L.at(-3.0, 0);
    append(f, taperFaces(cr[0], cr[1], 0.9, 0.8, 9.0, 10.6, 8, top)); append(f, coneFaces(cr[0], cr[1], 0.85, 10.6, 4.4, 8, roof));
    drawFaces(p, f, cam);
}

// ── Arco do Triunfo: dois pilares, o arco no meio, o ático com inscrição e a estatuazinha de bronze em cima ──
void drawArch(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    static const RGB stones[3] = { { 224, 214, 192 }, { 210, 204, 196 }, { 226, 204, 170 } };
    const RGB stone = stones[static_cast<int>(std::floor(b.v * 3)) % 3], top = shade(stone, 0.92);
    const Deco relief = [](QPainter& q, const PtFn& pt, double kk) {
        q.setPen(pen(rgb(DARK, kk, 0.35), 0.7)); q.setBrush(Qt::NoBrush);
        q.drawPath(pathOf({ pt(0.18, 0.62), pt(0.82, 0.62), pt(0.82, 0.84), pt(0.18, 0.84) }));
        QPainterPath a; a.moveTo(pt(0.3, 0.08)); a.lineTo(pt(0.3, 0.32)); a.quadTo(pt(0.5, 0.46), pt(0.7, 0.32)); a.lineTo(pt(0.7, 0.08)); a.closeSubpath();
        q.fillPath(a, rgb({ 70, 58, 46 }, kk, 0.85));
    };
    Faces f = boxFaces(b.x, b.y, b.yaw, 3.6, 8.4, 0, 0.5, { 190, 182, 166 });
    for (int s : { -1, 1 }) { const P2 q = L.at(0, s * 2.3); append(f, boxFaces(q[0], q[1], b.yaw, 2.6, 2.4, 0.5, 7.4, stone, &top, [relief](int k, double) { return (k == 1 || k == 3) ? relief : Deco(); })); }
    // o vão é reto até 4,2 e a curva fica desenhada no lintel
    append(f, boxFaces(b.x, b.y, b.yaw, 2.6, 2.2, 4.2, 7.4, stone, &top, [](int k, double) -> Deco {
        if (k != 1 && k != 3) return Deco();
        return [](QPainter& q, const PtFn& pt, double kk) {
            QPainterPath a; a.moveTo(pt(0, 0));
            for (int i = 0; i <= 10; i++) { const double an = M_PI - i / 10.0 * M_PI; a.lineTo(pt(0.5 + std::cos(an) * 0.5, std::sin(an) * 0.42)); }
            a.closeSubpath();
            q.fillPath(a, rgb({ 70, 58, 46 }, kk, 0.92));
            strokeLines(q, rgb(DARK, kk, 0.3), 0.6); line(q, pt(0.44, 0.42), pt(0.44, 0.5)); line(q, pt(0.56, 0.42), pt(0.56, 0.5));
        };
    }));
    append(f, boxFaces(b.x, b.y, b.yaw, 2.9, 7.2, 7.4, 7.7, top));
    append(f, boxFaces(b.x, b.y, b.yaw, 2.6, 6.8, 7.7, 9.3, stone, &top, [](int k, double) -> Deco {
        if (k != 1 && k != 3) return Deco();
        return [](QPainter& q, const PtFn& pt, double kk) { strokeLines(q, rgb(DARK, kk, 0.45), 0.8); for (int i = 0; i < 2; i++) line(q, pt(0.15, 0.38 + i * 0.26), pt(0.85, 0.38 + i * 0.26)); };
    }));
    append(f, boxFaces(b.x, b.y, b.yaw, 1.6, 2.2, 9.3, 9.8, top));
    FigOpt o; o.stance = 0.05; o.yaw = b.yaw; o.armR = Arm{ { 0.04, 0.18, 0.9 }, { 0.06, 0.17, 1.05 } };
    append(f, figure(L, 9.8, 3.0, { 96, 136, 116 }, o));
    drawFaces(p, f, cam);
}

// ── Mercado: praça calçada, barracas de lona listrada, o chafariz no meio, caixotes e barris ──
void drawMarket(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    static const RGB tents[5][2] = { { { 196, 64, 52 }, { 236, 228, 208 } }, { { 58, 96, 160 }, { 232, 206, 120 } }, { { 72, 128, 82 }, { 236, 226, 200 } },
                                     { { 210, 150, 60 }, { 128, 44, 40 } }, { { 150, 70, 130 }, { 236, 224, 206 } } };
    Rng R(seedOf(b, 41));
    Faces ground;
    {
        std::vector<V3> ring;
        for (int k = 0; k < 20; k++) { const double a = k / 20.0 * M_PI * 2; ring.push_back(L(std::cos(a) * 6.6, std::sin(a) * 6.6, 0.12)); }
        Face q = face(ring, { 198, 186, 162 }, L(0, 0, -1)); q.flat = true; ground.push_back(std::move(q));
    }
    Faces g;
    // chafariz
    append(g, lathe(L, 0, 0, { { 1.6, 0.1 }, { 1.6, 0.7 }, { 1.3, 0.7 } }, 12, { 196, 188, 172 }));
    {
        std::vector<V3> water;
        for (int k = 0; k < 12; k++) { const double a = k / 12.0 * M_PI * 2; water.push_back(L(std::cos(a) * 1.3, std::sin(a) * 1.3, 0.6)); }
        Face q = face(water, { 110, 160, 196 }, L(0, 0, -1)); q.flat = true; g.push_back(std::move(q));
    }
    const P2 o0 = L.at(0, 0);
    append(g, taperFaces(o0[0], o0[1], 0.3, 0.22, 0.6, 2.0, 6, { 206, 198, 182 }));
    append(g, lathe(L, 0, 0, { { 0.2, 2.0 }, { 0.8, 2.2 }, { 0.7, 2.4 }, { 0.0, 2.5 } }, 8, { 206, 198, 182 }));
    // barracas
    for (int i = 0; i < 7; i++) {
        const double a = i / 7.0 * M_PI * 2 + R() * 0.3, d = 4.0 + R() * 0.8;
        const P2 c0 = L.at(std::cos(a) * d, std::sin(a) * d);
        const double yaw = b.yaw + a;
        const int ti = static_cast<int>(std::floor(R() * 5)) % 5;
        const RGB c1 = tents[ti][0], c2 = tents[ti][1];
        const double h = 2.0 + R() * 0.4, w = 2.2, dd = 1.8;
        const double c = std::cos(yaw), s = std::sin(yaw);
        auto P = [&](double lx, double ly, double z) { return V3{ c0[0] + c * lx - s * ly, c0[1] + s * lx + c * ly, z }; };
        for (const P2& q : std::initializer_list<P2>{ { -w / 2, -dd / 2 }, { w / 2, -dd / 2 }, { w / 2, dd / 2 }, { -w / 2, dd / 2 } }) append(g, beamFaces(P(q[0], q[1], 0), P(q[0], q[1], h), 0.14, 0.14, { 96, 72, 48 }));
        const V3 st = P(w * 0.25, 0, 0);
        append(g, boxFaces(st[0], st[1], yaw, 0.9, dd * 0.8, 0, 0.8, { 150, 112, 74 }));
        const V3 apex = P(0, 0, h + 1.1);
        const V3 cn[4] = { P(-w / 2 - 0.2, -dd / 2 - 0.2, h), P(w / 2 + 0.2, -dd / 2 - 0.2, h), P(w / 2 + 0.2, dd / 2 + 0.2, h), P(-w / 2 - 0.2, dd / 2 + 0.2, h) };
        const Deco stripes = [c2](QPainter& q, const PtFn& pt, double kk) {
            for (int j = 0; j < 4; j++) q.fillPath(pathOf({ pt(j / 4.0, 0), pt(j / 4.0 + 0.125, 0), pt(0.5, 1) }), rgb(c2, kk));
        };
        for (int k = 0; k < 4; k++) { Face q = face({ cn[k], cn[(k + 1) % 4], apex }, c1, P(0, 0, h), stripes, 0.02); q.q = { 0, 1, 2, 2 }; g.push_back(std::move(q)); }
    }
    // caixotes e barris
    for (int i = 0; i < 6; i++) {
        const double a = R() * M_PI * 2, d = 2.2 + R() * 1.4;
        const P2 q = L.at(std::cos(a) * d, std::sin(a) * d);
        if (R() < 0.5) append(g, boxFaces(q[0], q[1], R() * 3, 0.7, 0.7, 0, 0.7, { 168, 128, 82 }));
        else append(g, taperFaces(q[0], q[1], 0.36, 0.36, 0, 0.8, 6, { 130, 90, 56 }));
    }
    drawFaces(p, ground, cam);
    drawFaces(p, g, cam);
}

// ── Aqueduto: duas fileiras de arcos de pedra, o canal com água em cima ──
void drawAqueduct(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    static const RGB stones[2] = { { 206, 192, 166 }, { 196, 186, 172 } };
    const RGB stone = stones[static_cast<int>(std::floor(b.v * 2)) % 2], top = shade(stone, 0.92), water = { 102, 150, 186 };
    const double Lh = 10.4;
    auto arches = [](int n, double depth) {
        return [n, depth](QPainter& q, const PtFn& pt, double kk) {
            for (int i = 0; i < n; i++) {
                const double s0 = double(i) / n, s1 = double(i + 1) / n, cx = (s0 + s1) / 2, rw = (s1 - s0) * 0.36;
                QPainterPath a; a.moveTo(pt(cx - rw, 0));
                for (int j = 0; j <= 8; j++) { const double an = M_PI - j / 8.0 * M_PI; a.lineTo(pt(cx + std::cos(an) * rw, std::sin(an) * depth)); }
                a.closeSubpath();
                q.fillPath(a, rgb({ 70, 58, 46 }, kk, 0.9));
            }
        };
    };
    Faces f;
    const int np = 7;
    const double gap = 2 * Lh / (np - 1);
    for (int i = 0; i < np; i++) { const P2 q = L.at(-Lh + i * gap, 0); append(f, boxFaces(q[0], q[1], b.yaw, 1.1, 1.8, -1.2, 4.0, stone, &top)); }
    append(f, boxFaces(b.x, b.y, b.yaw, 2 * Lh + 1.1, 1.8, 4.0, 6.0, stone, &top, [&](int k, double) -> Deco { return (k == 0 || k == 2) ? Deco(arches(np - 1, 0.62)) : Deco(); }));
    const int n2 = 2 * (np - 1) + 1;
    const double g2 = 2 * Lh / (n2 - 1);
    for (int i = 0; i < n2; i++) { const P2 q = L.at(-Lh + i * g2, 0); append(f, boxFaces(q[0], q[1], b.yaw, 0.6, 1.4, 6.0, 7.6, stone, &top)); }
    append(f, boxFaces(b.x, b.y, b.yaw, 2 * Lh + 0.6, 1.4, 7.6, 8.6, stone, &top, [&](int k, double) -> Deco { return (k == 0 || k == 2) ? Deco(arches(n2 - 1, 0.5)) : Deco(); }));
    append(f, boxFaces(b.x, b.y, b.yaw, 2 * Lh + 0.9, 1.7, 8.6, 9.3, top, &water));
    drawFaces(p, f, cam);
}

// ════════════════════════ castelos ════════════════════════

struct CastleOut4 { Faces faces, ground; V3 flag{}; bool orb = false, frost = false; V3 orbAt{}, frostAt{}; };
const CastleCfg& cfgOf(const Art& b) { return kCastle[clampi(b.size, 0, 3)]; }
// trecho de muralha com ameias entre dois pontos do mundo
void wallBetween(Faces& f, const P2& a, const P2& q, double z0, double h, double thick, const RGB& col, const RGB& top, const std::function<Deco(double)>& deco = {})
{
    const double mx = (a[0] + q[0]) / 2, my = (a[1] + q[1]) / 2, len = std::hypot(q[0] - a[0], q[1] - a[1]), yaw = std::atan2(q[1] - a[1], q[0] - a[0]);
    for (Face& x : boxFaces(mx, my, yaw, len, thick, z0, h, col, &top, [&](int s, double) { return (deco && (s == 0 || s == 2)) ? deco(len) : Deco(); })) { x.c = { mx, my, h / 2 }; f.push_back(std::move(x)); }
    merlons(f, a[0], a[1], q[0], q[1], h, thick, col);
}
// torre quadrada com ameias
void squareTower(Faces& f, double x, double y, double yaw, double w, double h, const RGB& col, const RGB& top, const std::function<Deco(int)>& deco = {})
{
    append(f, boxFaces(x, y, yaw, w, w, 0, h, col, &top, [&](int k, double) { return deco ? deco(k) : slitDeco(1); }));
    const double c = std::cos(yaw), s = std::sin(yaw), hw = w / 2;
    auto P = [&](double lx, double ly) { return P2{ x + c * lx - s * ly, y + s * lx + c * ly }; };
    const P2 k[4] = { P(-hw, -hw), P(hw, -hw), P(hw, hw), P(-hw, hw) };
    for (int i = 0; i < 4; i++) merlons(f, k[i][0], k[i][1], k[(i + 1) % 4][0], k[(i + 1) % 4][1], h, 0.5, col);
}
const RGB kHStones[3] = { { 200, 190, 170 }, { 190, 182, 168 }, { 208, 196, 174 } };
RGB realmRoof(const RGB& rc) { return { rc[0] * 0.7, rc[1] * 0.7, rc[2] * 0.7 }; }
std::function<Deco(double)> slitsAlong(double per) { return [per](double len) { return slitDeco(std::max(2, static_cast<int>(std::lround(len / per)))); }; }

// normando: muralha com torres quadradas, guarita do portão e a menagem quadrada maciça com torreões nas quinas
CastleOut4 normanCastle(const Art& b, const RGB&)
{
    const CastleCfg& cfg = cfgOf(b);
    const double R = cfg.R;
    const Loc L(b);
    const RGB stone = kHStones[static_cast<int>(std::floor(b.v * 3)) % 3], top = shade(stone, 0.9);
    CastleOut4 o;
    Faces& f = o.faces;
    auto ringSq = [&](double RR, double wh, double tw, double th, bool mid) {
        static const int cn[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
        std::vector<P2> pts;
        for (int k = 0; k < 4; k++) {
            pts.push_back({ cn[k][0] * RR, cn[k][1] * RR });
            if (mid) pts.push_back({ (cn[k][0] + cn[(k + 1) % 4][0]) / 2.0 * RR, (cn[k][1] + cn[(k + 1) % 4][1]) / 2.0 * RR });
        }
        for (size_t k = 0; k < pts.size(); k++) wallBetween(f, L.at(pts[k][0], pts[k][1]), L.at(pts[(k + 1) % pts.size()][0], pts[(k + 1) % pts.size()][1]), 0, wh, 1.2, stone, top, slitsAlong(2.4));
        for (const P2& q : pts) { const P2 w = L.at(q[0], q[1]); squareTower(f, w[0], w[1], b.yaw, tw, th, stone, top); }
    };
    ringSq(R, cfg.wallH, cfg.towerR * 1.8, cfg.towerH * 0.92, cfg.mid);
    if (cfg.hasInner) ringSq(cfg.iR, cfg.iWallH, cfg.iTowerR * 1.6, cfg.iTowerH * 0.9, false);
    // guarita do portão na frente
    const P2 g = L.at(R, 0);
    append(f, boxFaces(g[0], g[1], b.yaw, 2.6, 4.6, 0, cfg.wallH * 1.4, stone, &top, [](int k, double) { return (k == 1 || k == 3) ? Deco(gateDeco) : Deco(); }));
    for (int s : { -1, 1 }) { const P2 t = L.at(R, s * 2.6); squareTower(f, t[0], t[1], b.yaw, 2.0, cfg.wallH * 1.65, stone, top); }
    // a menagem: pilastras nas faces, janelas em arco em duas fileiras
    const double kw = cfg.keep * 1.2, kh = cfg.keepH * 1.12;
    const Deco rows = windowRows(2, 2);
    const RGB pil = shade(stone, 0.72);
    const Deco pilaster = [rows, pil](QPainter& q, const PtFn& pt, double kk) {
        strokeLines(q, rgb(pil, kk, 0.8), 1.1);
        for (double s : { 0.04, 0.35, 0.65, 0.96 }) line(q, pt(s, 0), pt(s, 1));
        rows(q, pt, kk);
    };
    const RGB kst = shade(stone, 1.0);
    append(f, boxFaces(b.x, b.y, b.yaw, kw, kw, 0, kh, kst, &top, [pilaster](int, double) { return pilaster; }));
    const double hk = kw / 2;
    const P2 kc[4] = { L.at(-hk, -hk), L.at(hk, -hk), L.at(hk, hk), L.at(-hk, hk) };
    for (int i = 0; i < 4; i++) merlons(f, kc[i][0], kc[i][1], kc[(i + 1) % 4][0], kc[(i + 1) % 4][1], kh, 0.6, stone);
    for (const P2& q : kc) squareTower(f, q[0], q[1], b.yaw, kw * 0.26, kh + 2.0, stone, top);
    // escada coberta subindo pra porta da menagem
    const P2 e = L.at(hk + 1.0, -hk * 0.35);
    squareTower(f, e[0], e[1], b.yaw, 2.0, kh * 0.5, stone, top);
    o.flag = { kc[1][0], kc[1][1], kh + 2.6 };
    return o;
}

// com fosso: a muralha alta sai da água (o fosso é o próprio terreno, cavado em volta: moatGeom), torres redondas
// de topo plano, guarita com ponte levadiça e o salão no pátio
CastleOut4 moatCastle(const Art& b, const RGB& realmCol)
{
    const CastleCfg& cfg = cfgOf(b);
    const double Rx = cfg.R, Ry = cfg.R * 0.84, M = cfg.R * 0.42 + 2.6;
    const Loc L(b);
    const RGB stone = kHStones[static_cast<int>(std::floor(b.v * 3)) % 3], top = shade(stone, 0.9);
    CastleOut4 o;
    Faces& f = o.faces;
    {
        Face q = face({ L(Rx + 0.5, -1.0, 0.3), L(Rx + M + 1.4, -1.0, 0.3), L(Rx + M + 1.4, 1.0, 0.3), L(Rx + 0.5, 1.0, 0.3) }, { 150, 112, 72 }, L(0, 0, -1),
                      [](QPainter& qq, const PtFn& pt, double) { strokeLines(qq, QColor(60, 40, 24, 140), 0.6); for (int i = 1; i < 6; i++) line(qq, pt(i / 6.0, 0), pt(i / 6.0, 1)); });
        q.flat = true;
        o.ground.push_back(std::move(q));
    }
    const double wh = cfg.wallH * 1.25;
    const P2 cn[4] = { { -Rx, -Ry }, { Rx, -Ry }, { Rx, Ry }, { -Rx, Ry } };
    for (int k = 0; k < 4; k++) wallBetween(f, L.at(cn[k][0], cn[k][1]), L.at(cn[(k + 1) % 4][0], cn[(k + 1) % 4][1]), -2.5, wh, 1.3, stone, top, slitsAlong(2.2));
    for (const P2& q : cn) { const P2 w = L.at(q[0], q[1]); append(f, towerFaces(w[0], w[1], cfg.towerR * 1.2, cfg.towerH * 1.05, 0, stone, stone, true)); }
    if (cfg.mid) for (const P2& q : { P2{ 0, -Ry }, P2{ 0, Ry }, P2{ -Rx, 0 } }) { const P2 w = L.at(q[0], q[1]); squareTower(f, w[0], w[1], b.yaw, cfg.towerR * 1.7, cfg.towerH * 0.95, stone, top); }
    // guarita: duas torres redondas com o portão no meio
    for (int s : { -1, 1 }) { const P2 w = L.at(Rx + 0.5, s * 1.9); append(f, towerFaces(w[0], w[1], 1.15, wh * 1.35, 0, stone, stone, true)); }
    const P2 gt = L.at(Rx + 0.5, 0);
    append(f, boxFaces(gt[0], gt[1], b.yaw, 2.2, 2.8, 0, wh * 1.15, stone, &top, [](int k, double) { return (k == 1 || k == 3) ? Deco(gateDeco) : Deco(); }));
    // o salão no pátio, encostado na muralha do fundo
    const double hx = Rx * 0.22 + 1.2, hy = Ry * 0.62, hcx = -Rx + hx + 1.0, hh = wh * 0.85;
    const P2 hc = L.at(hcx, 0);
    append(f, boxFaces(hc[0], hc[1], b.yaw, hx * 2, hy * 2, 0, hh, shade(stone, 1.03), nullptr, [](int k, double) { return k == 1 ? windowRows(1, 4) : Deco(); }));
    append(f, hipRoof(L, hcx, 0, hx + 0.2, hy + 0.2, hh, std::min(hx, hy) * 0.9, realmRoof(realmCol), roofDeco));
    const P2 fl = L.at(Rx + 0.5, -1.9);
    o.flag = { fl[0], fl[1], wh * 1.35 + 0.7 };
    return o;
}

// château: corpo branco comprido com telhado de ardósia e mansardas, torres redondas de cone alto, a torre da escada;
// jardim de canteiros na frente e um muro baixo em volta. Grande e Colossal ganham as alas
CastleOut4 chateauCastle(const Art& b, const RGB&)
{
    const CastleCfg& cfg = cfgOf(b);
    const double R = cfg.R;
    const Loc L(b);
    static const RGB whites[3] = { { 240, 234, 220 }, { 232, 226, 214 }, { 244, 238, 226 } };
    static const RGB slates[3] = { { 74, 86, 108 }, { 68, 74, 90 }, { 88, 96, 112 } };
    const RGB white = whites[static_cast<int>(std::floor(b.v * 3)) % 3], slate = slates[static_cast<int>(std::floor(b.v * 7)) % 3], winCol = { 70, 80, 100 };
    CastleOut4 o;
    Faces& f = o.faces;
    const double lw = cfg.keep * 1.15, ld = cfg.keep * 0.42, lh = cfg.keepH * 0.58, rh = cfg.keepH * 0.42;
    // muro baixo e o jardim
    const P2 cn[4] = { { -R, -R }, { R, -R }, { R, R }, { -R, R } };
    for (int k = 0; k < 4; k++) {
        const P2 a = L.at(cn[k][0], cn[k][1]), q = L.at(cn[(k + 1) % 4][0], cn[(k + 1) % 4][1]);
        append(f, boxFaces((a[0] + q[0]) / 2, (a[1] + q[1]) / 2, std::atan2(q[1] - a[1], q[0] - a[0]), std::hypot(q[0] - a[0], q[1] - a[1]), 0.6, 0, cfg.wallH * 0.42, shade(white, 0.94)));
    }
    for (int s : { -1, 1 }) { const P2 g = L.at(R, s * 1.6); append(f, boxFaces(g[0], g[1], b.yaw, 0.9, 0.9, 0, cfg.wallH * 0.75, white)); append(f, coneFaces(g[0], g[1], 0.5, cfg.wallH * 0.75, 0.9, 4, slate)); }
    const double gx0 = ld + 1.4, gx1 = R - 1.0;
    if (gx1 - gx0 > 2) for (int s : { -1, 1 }) {
        const double y0 = s * 1.2, y1 = s * std::min(lw, R - 1.2);
        Face q = face({ L(gx0, y0, 0.08), L(gx1, y0, 0.08), L(gx1, y1, 0.08), L(gx0, y1, 0.08) }, { 96, 140, 74 }, L(0, 0, -1), [](QPainter& qq, const PtFn& pt, double) {
            strokeLines(qq, QColor(214, 200, 160, 230), 1);
            line(qq, pt(0.5, 0), pt(0.5, 1)); line(qq, pt(0, 0.5), pt(1, 0.5));
            const QPointF c = pt(0.5, 0.5), e = pt(0.68, 0.5);
            const double r = std::max(1.0, std::hypot(e.x() - c.x(), e.y() - c.y()));
            qq.drawEllipse(c, r, r);
        });
        q.flat = true;
        o.ground.push_back(std::move(q));
    }
    // corpo principal e o telhado com mansardas e chaminés
    append(f, boxFaces(b.x, b.y, b.yaw, ld * 2, lw * 2, 0, lh, white, nullptr, [winCol](int k, double) { return windowRows(2, k % 2 ? 7 : 3, winCol); }));
    const Deco dormers = [white](QPainter& q, const PtFn& pt, double kk) {
        for (int i = 0; i < 5; i++) { const double s = 0.18 + i * 0.16; quad(q, pt, s - 0.025, s + 0.025, 0.12, 0.42, rgb(white, kk)); quad(q, pt, s - 0.014, s + 0.014, 0.16, 0.34, rgb({ 70, 80, 100 }, kk)); }
    };
    append(f, hipRoof(L, 0, 0, ld + 0.25, lw + 0.25, lh, rh, slate, dormers));
    const RGB chimTop = { 90, 80, 72 };
    for (int s : { -1, 1 }) { const P2 c = L.at(-ld * 0.2, s * lw * 0.5); append(f, boxFaces(c[0], c[1], b.yaw, 0.7, 1.0, lh + rh * 0.4, lh + rh + 1.4, white, &chimTop)); }
    // alas (grande e colossal): saem pra frente nas pontas, formando um U
    if (b.size >= 2) for (int s : { -1, 1 }) {
        const double wx = ld + cfg.keep * 0.55, wy = s * (lw - ld * 0.7), hx = cfg.keep * 0.55, hy = ld * 0.7;
        const P2 w = L.at(wx, wy);
        append(f, boxFaces(w[0], w[1], b.yaw, hx * 2, hy * 2, 0, lh * 0.9, white, nullptr, [winCol](int k, double) { return windowRows(2, k % 2 ? 2 : 5, winCol); }));
        append(f, hipRoof(L, wx, wy, hx + 0.2, hy + 0.2, lh * 0.9, rh * 0.8, slate, roofDeco));
    }
    // torres redondas nas quinas, cone alto de ardósia com o pináculo
    const double tr = cfg.towerR * 1.05, th = lh * 1.12;
    for (const auto& q : { P2{ 1, 1 }, P2{ 1, -1 }, P2{ -1, 1 }, P2{ -1, -1 } }) {
        const P2 t = L.at(q[0] * ld, q[1] * lw);
        append(f, taperFaces(t[0], t[1], tr, tr, 0, th, 12, white, [winCol](int i) { return i % 3 == 0 ? windowRows(2, 1, winCol) : Deco(); }));
        append(f, ringL(L, q[0] * ld, q[1] * lw, tr * 1.08, tr * 1.08, tr * 1.08, tr * 1.08, th, th + 0.3, 12, shade(white, 0.92)));
        append(f, coneFaces(t[0], t[1], tr * 1.08, th + 0.3, tr * 3.0, 12, slate));
        append(f, beamFaces({ t[0], t[1], th + 0.3 + tr * 3.0 - 0.2 }, { t[0], t[1], th + 0.3 + tr * 3.0 + 0.8 }, 0.12, 0.12, { 196, 168, 96 }));
    }
    // torre da escada no meio da fachada, a mais alta
    const double sr = cfg.towerR * 0.85, shh = lh * 1.45;
    const P2 st = L.at(ld + sr * 0.6, 0);
    append(f, taperFaces(st[0], st[1], sr, sr, 0, shh, 10, white, [](int i) { return i % 2 ? slitDeco(2) : Deco(); }));
    append(f, coneFaces(st[0], st[1], sr * 1.1, shh, sr * 3.4, 10, slate));
    o.flag = { st[0], st[1], shh + sr * 3.4 };
    return o;
}

// penhasco: o castelo assenta no relevo que já existe. Cada torre fica no chão do seu lugar e a muralha sobe e
// desce a encosta em trechos curtos; os alicerces descem pela pedra. cam.gz dá a altura do chão (sem ela, plano)
CastleOut4 cragCastle(const Art& b, const RGB& realmCol, const Cam& cam)
{
    const CastleCfg& cfg = cfgOf(b);
    const double R = cfg.R * 0.9;
    const Loc L(b);
    Rng Rr(seedOf(b, 29));
    const RGB stone = kHStones[static_cast<int>(std::floor(b.v * 3)) % 3], top = shade(stone, 0.9), found = { 150, 142, 128 }, roof = realmRoof(realmCol);
    auto zAt = [&](double x, double y) { return cam.gz ? cam.gz(x, y) : 0.0; };
    CastleOut4 o;
    Faces& f = o.faces;
    // muralha: anel torto, cada trecho dividido em pedaços de ~2,4 que pegam o chão das duas pontas
    const int n = 6 + clampi(b.size, 0, 3) * 2;
    std::vector<P2> pts;
    for (int k = 0; k < n; k++) { const double a = double(k) / n * M_PI * 2 + 0.3, rr = R * (0.82 + Rr() * 0.3); pts.push_back(L.at(std::cos(a) * rr, std::sin(a) * rr)); }
    const double wh = cfg.wallH, th = 1.2;
    for (int k = 0; k < n; k++) {
        const P2 a = pts[k], q = pts[(k + 1) % n];
        const double len = std::hypot(q[0] - a[0], q[1] - a[1]);
        const int m = std::max(1, static_cast<int>(std::lround(len / 2.4)));
        const double dx = (q[0] - a[0]) / len, dy = (q[1] - a[1]) / len, nx = -dy * th / 2, ny = dx * th / 2, yaw = std::atan2(dy, dx);
        for (int i = 0; i < m; i++) {
            const P2 p0 = { a[0] + (q[0] - a[0]) * i / m, a[1] + (q[1] - a[1]) * i / m }, p1 = { a[0] + (q[0] - a[0]) * (i + 1) / m, a[1] + (q[1] - a[1]) * (i + 1) / m };
            const double z0 = zAt(p0[0], p0[1]), z1 = zAt(p1[0], p1[1]);
            const V3 C = { (p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2, (z0 + z1) / 2 + wh / 2 };
            auto V = [&](const P2& pp, double s, double z) { return V3{ pp[0] + nx * s, pp[1] + ny * s, z }; };
            const Deco sl = i % 2 ? slitDeco(1) : Deco();
            f.push_back(face({ V(p0, 1, z0 - 3), V(p1, 1, z1 - 3), V(p1, 1, z1 + wh), V(p0, 1, z0 + wh) }, stone, C, sl));
            f.push_back(face({ V(p1, -1, z1 - 3), V(p0, -1, z0 - 3), V(p0, -1, z0 + wh), V(p1, -1, z1 + wh) }, stone, C, sl));
            f.push_back(face({ V(p0, 1, z0 + wh), V(p1, 1, z1 + wh), V(p1, -1, z1 + wh), V(p0, -1, z0 + wh) }, top, C));
            const double mx = (p0[0] + p1[0]) / 2, my = (p0[1] + p1[1]) / 2, mz = (z0 + z1) / 2 + wh;
            for (Face& x : boxFaces(mx, my, yaw, 0.62, th, mz, mz + 0.7, stone)) { x.bias = 0.05; f.push_back(std::move(x)); }
        }
    }
    // torres em cada canto do anel, cada uma no seu chão, com alicerce descendo
    for (int i = 0; i < n; i++) {
        const P2& q = pts[i];
        const double z = zAt(q[0], q[1]), tr = cfg.towerR * (i % 2 ? 0.8 : 0.95), tht = cfg.towerH * (0.85 + Rr() * 0.3);
        append(f, lifted(taperFaces(q[0], q[1], tr * 1.25, tr * 1.05, 0, 2.6, 10, found), z - 2.6));
        append(f, lifted(towerFaces(q[0], q[1], tr, tht, 2.6 * tr / 1.6, stone, roof, false), z));
    }
    // menagem no meio, sobre um embasamento que desce até o ponto mais baixo do seu pé
    const double kw = cfg.keep * 0.82, hk = kw / 2;
    const P2 kc[4] = { L.at(-hk, -hk), L.at(hk, -hk), L.at(hk, hk), L.at(-hk, hk) };
    double zLow = zAt(b.x, b.y), zHigh = zLow;
    for (const P2& q : kc) { const double z = zAt(q[0], q[1]); zLow = std::min(zLow, z); zHigh = std::max(zHigh, z); }
    const RGB foundTop = shade(found, 0.92);
    append(f, lifted(boxFaces(b.x, b.y, b.yaw, kw * 1.25, kw * 1.25, 0, zHigh - zLow + 1.2, found, &foundTop), zLow - 1.5));
    const double kz = zHigh - 0.3;
    Faces keep = boxFaces(b.x, b.y, b.yaw, kw, kw, 0, cfg.keepH, shade(stone, 1.04), &top, [](int, double) { return slitDeco(2); });
    for (int i = 0; i < 4; i++) merlons(keep, kc[i][0], kc[i][1], kc[(i + 1) % 4][0], kc[(i + 1) % 4][1], cfg.keepH, 0.5, stone);
    append(keep, towerFaces(b.x, b.y, cfg.kt[0], cfg.kt[1], cfg.kt[2], stone, roof, false));
    append(f, lifted(std::move(keep), kz));
    o.flag = { b.x, b.y, kz + cfg.kt[1] + cfg.kt[2] + 0.2 };
    return o;
}

// oriental: muralha branca sobre base de pedra com telhadinho, guaritas de dois andares nas quinas, a torre de vários telhados
CastleOut4 orientalCastle(const Art& b, const RGB&)
{
    const CastleCfg& cfg = cfgOf(b);
    const double R = cfg.R;
    const Loc L(b);
    static const RGB tiles[3] = { { 100, 112, 120 }, { 112, 106, 100 }, { 94, 118, 114 } };
    const RGB white = { 250, 248, 242 }, tile = tiles[static_cast<int>(std::floor(b.v * 3)) % 3], stone = { 186, 178, 164 };
    CastleOut4 o;
    Faces& f = o.faces;
    auto winDeco = [](int n) { return [n](QPainter& q, const PtFn& pt, double kk) { for (int i = 0; i < n; i++) { const double s = (i + 0.5) / n; quad(q, pt, s - 0.04, s + 0.04, 0.5, 0.72, rgb(DARK, kk, 0.85)); } }; };
    const Deco courses = linesDeco({ { 0, 0.25, 1, 0.25 }, { 0, 0.5, 1, 0.5 }, { 0, 0.75, 1, 0.75 } }, QColor(60, 54, 48, 90), 0.6);
    auto ringWall = [&](double RR, double wh) {
        static const int cn[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
        for (int k = 0; k < 4; k++) {
            const P2 a = L.at(cn[k][0] * RR, cn[k][1] * RR), q = L.at(cn[(k + 1) % 4][0] * RR, cn[(k + 1) % 4][1] * RR);
            const double mx = (a[0] + q[0]) / 2, my = (a[1] + q[1]) / 2, len = std::hypot(q[0] - a[0], q[1] - a[1]), yaw = std::atan2(q[1] - a[1], q[0] - a[0]);
            append(f, boxFaces(mx, my, yaw, len, 1.6, 0, wh * 0.45, stone, nullptr, [courses](int s, double) { return s % 2 == 0 ? courses : Deco(); }));
            const int nw = std::max(2, static_cast<int>(std::lround(len / 2.4)));
            append(f, boxFaces(mx, my, yaw, len, 1.0, wh * 0.45, wh, white, nullptr, [&](int s, double) { return s % 2 == 0 ? Deco(winDeco(nw)) : Deco(); }));
            for (Face& x : boxFaces(mx, my, yaw, len + 0.3, 1.3, wh, wh + 0.28, tile)) { x.bias = 0.03; f.push_back(std::move(x)); }
        }
    };
    ringWall(R, cfg.wallH * 0.95);
    if (cfg.hasInner) ringWall(cfg.iR, cfg.iWallH * 0.9);
    // guaritas nas quinas: base de pedra, dois andares, telhado de beiral curvo
    const double tr = cfg.towerR * 1.05, th = cfg.towerH * 0.5;
    for (const auto& q : { P2{ -1, -1 }, P2{ 1, -1 }, P2{ 1, 1 }, P2{ -1, 1 } }) {
        const double lx = q[0] * R, ly = q[1] * R;
        append(f, frustum(L, lx, ly, tr * 1.25, tr * 1.25, tr, tr, 0, cfg.wallH * 0.6, stone));
        double z = cfg.wallH * 0.6;
        const P2 w = L.at(lx, ly);
        for (int i = 0; i < 2; i++) {
            const double hs = tr * (1 - i * 0.28), hh = th * (i ? 0.45 : 0.6), rh = i ? 1.2 : 0.6;
            append(f, boxFaces(w[0], w[1], b.yaw, hs * 2, hs * 2, z, z + hh, white, nullptr, [&](int, double) { return Deco(winDeco(2)); }));
            append(f, curvedRoof(L, lx, ly, hs + 0.7, hs * 0.55, z + hh, 0.35, rh, tile));
            z += hh + rh - 0.05;
        }
    }
    // torre principal: base de pedra inclinada e de 3 a 5 andares, cada um com seu telhado
    const double kb = cfg.keep * 0.62;
    const int size = clampi(b.size, 0, 3), tiers = size == 3 ? 5 : size == 2 ? 4 : 3;
    const double tierH = cfg.keepH * (size == 3 ? 0.2 : 0.26);
    append(f, frustum(L, 0, 0, kb * 1.25, kb * 1.1, kb, kb * 0.88, 0, cfg.keepH * 0.32, stone, nullptr,
                      [](int) { return linesDeco({ { 0, 0.2, 1, 0.2 }, { 0, 0.4, 1, 0.4 }, { 0, 0.6, 1, 0.6 }, { 0, 0.8, 1, 0.8 } }, QColor(60, 54, 48, 90), 0.6); }));
    double z = cfg.keepH * 0.32;
    for (int i = 0; i < tiers; i++) {
        const double hx = kb * (1 - i * 0.16), hy = kb * 0.88 * (1 - i * 0.16), hh = tierH * (i ? 0.78 : 1), rh = i == tiers - 1 ? 2.2 : 0.75;
        append(f, boxFaces(b.x, b.y, b.yaw, hx * 2, hy * 2, z, z + hh, white, nullptr, [&](int, double) { return Deco(winDeco(3)); }));
        append(f, curvedRoof(L, 0, 0, std::max(hx, hy) + 0.9, std::max(hx, hy) * (i == tiers - 1 ? 0.18 : 0.6), z + hh, 0.45, rh, tile));
        z += hh + rh - 0.05;
    }
    // os peixes dourados no topo
    for (int s : { -1, 1 }) { const P2 q = L.at(s * 0.5, 0); append(f, blobFaces(q[0], q[1], z + 0.2, 0.28, 0.38, 6, 3, { 230, 190, 84 }, seedOf(b, 3 + s), 0.08)); }
    o.flag = { b.x, b.y, z + 0.6 };
    return o;
}

// deserto: muralha de adobe, torres redondas com cúpula, o palácio com a cúpula de cebola e dois minaretes
CastleOut4 desertCastle(const Art& b, const RGB&)
{
    const CastleCfg& cfg = cfgOf(b);
    const double R = cfg.R;
    const Loc L(b);
    static const RGB adobes[3] = { { 226, 194, 144 }, { 214, 176, 128 }, { 232, 214, 180 } };
    static const RGB domes[3] = { { 68, 158, 166 }, { 214, 176, 80 }, { 222, 226, 228 } };
    const RGB adobe = adobes[static_cast<int>(std::floor(b.v * 3)) % 3], dome = domes[static_cast<int>(std::floor(b.v * 7)) % 3], gold = { 230, 190, 84 }, top = shade(adobe, 0.9), win = { 70, 50, 34 };
    CastleOut4 o;
    Faces& f = o.faces;
    auto onion = [&](double lx, double ly, double z, double r, const RGB& col) {
        return lathe(L, lx, ly, { { r, z }, { r * 1.16, z + r * 0.4 }, { r * 1.08, z + r * 0.85 }, { r * 0.78, z + r * 1.2 }, { r * 0.38, z + r * 1.55 }, { r * 0.1, z + r * 1.82 }, { 0, z + r * 2.0 } }, 10, col);
    };
    static const int cn[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
    std::vector<P2> pts;
    for (int k = 0; k < 4; k++) { pts.push_back({ cn[k][0] * R, cn[k][1] * R }); if (cfg.mid) pts.push_back({ (cn[k][0] + cn[(k + 1) % 4][0]) / 2.0 * R, (cn[k][1] + cn[(k + 1) % 4][1]) / 2.0 * R }); }
    for (size_t k = 0; k < pts.size(); k++) {
        const P2 a = L.at(pts[k][0], pts[k][1]), q = L.at(pts[(k + 1) % pts.size()][0], pts[(k + 1) % pts.size()][1]);
        const double mx = (a[0] + q[0]) / 2, my = (a[1] + q[1]) / 2, len = std::hypot(q[0] - a[0], q[1] - a[1]), yaw = std::atan2(q[1] - a[1], q[0] - a[0]);
        const bool gate = k == 0;
        append(f, boxFaces(mx, my, yaw, len, 1.4, 0, cfg.wallH * 0.95, adobe, &top, [gate, win](int s, double) -> Deco {
            if (!gate || (s != 0 && s != 2)) return Deco();
            return [win](QPainter& qq, const PtFn& pt, double kk) {
                QPainterPath d; d.moveTo(pt(0.42, 0)); d.lineTo(pt(0.42, 0.5)); d.quadTo(pt(0.5, 0.78), pt(0.58, 0.5)); d.lineTo(pt(0.58, 0)); d.closeSubpath();
                qq.fillPath(d, rgb(win, kk));
            };
        }));
        merlons(f, a[0], a[1], q[0], q[1], cfg.wallH * 0.95, 1.4, adobe);
    }
    for (size_t i = 0; i < pts.size(); i++) {
        const bool corner = !cfg.mid || i % 2 == 0;
        const double tr = cfg.towerR * (corner ? 0.95 : 0.75), th = cfg.towerH * (corner ? 1.0 : 0.85);
        const P2 w = L.at(pts[i][0], pts[i][1]);
        append(f, taperFaces(w[0], w[1], tr, tr * 0.86, 0, th, 10, adobe, [](int j) { return j % 3 == 0 ? slitDeco(1) : Deco(); }));
        append(f, ringL(L, pts[i][0], pts[i][1], tr * 1.08, tr * 1.08, tr * 1.08, tr * 1.08, th, th + 0.35, 10, top));
        append(f, lathe(L, pts[i][0], pts[i][1], { { tr * 0.86, th + 0.35 }, { tr * 0.82, th + tr * 0.55 }, { tr * 0.55, th + tr * 0.95 }, { tr * 0.2, th + tr * 1.15 }, { 0, th + tr * 1.22 } }, 10, dome));
    }
    // palácio: o bloco com janelas em arco, as cupulazinhas nas quinas e a grande cebola
    const double kw = cfg.keep * 0.62, kd = cfg.keep * 0.5, kh = cfg.keepH * 0.48;
    append(f, boxFaces(b.x, b.y, b.yaw, kw * 2, kd * 2, 0, kh, adobe, &top, [win](int s, double) { return roundWin(s % 2 ? 2 : 3, win); }));
    for (const auto& c : cn) {
        const double lx = c[0] * kw * 0.9, ly = c[1] * kd * 0.9;
        const P2 w = L.at(lx, ly);
        append(f, taperFaces(w[0], w[1], 0.6, 0.55, kh, kh + 0.6, 8, top));
        append(f, onion(lx, ly, kh + 0.6, 0.55, dome));
    }
    const double kr = std::min(kw, kd) * 0.68;
    append(f, taperFaces(b.x, b.y, kr * 0.95, kr * 0.95, kh, kh + kr * 0.5, 12, top, [win](int j) { return j % 2 ? roundWin(1, win, 0.2, 0.8) : Deco(); }));
    append(f, onion(0, 0, kh + kr * 0.5, kr, dome));
    const double domeTop = kh + kr * 0.5 + kr * 2.0;
    append(f, beamFaces({ b.x, b.y, domeTop - 0.1 }, { b.x, b.y, domeTop + 1.2 }, 0.12, 0.12, gold));
    // minaretes atrás do palácio
    const double mh = cfg.kt[1] * 1.15;
    for (int s : { -1, 1 }) {
        const double lx = -kw * 1.2, ly = s * kd * 1.15;
        const P2 w = L.at(lx, ly);
        append(f, taperFaces(w[0], w[1], 0.62, 0.5, 0, mh, 8, shade(adobe, 1.04), [](int j) { return j % 4 == 0 ? slitDeco(2) : Deco(); }));
        append(f, ringL(L, lx, ly, 0.85, 0.85, 0.85, 0.85, mh * 0.78, mh * 0.78 + 0.3, 8, top));
        append(f, onion(lx, ly, mh, 0.5, dome));
        append(f, beamFaces({ w[0], w[1], mh + 0.95 }, { w[0], w[1], mh + 1.7 }, 0.08, 0.08, gold));
    }
    const P2 fl = L.at(-kw * 1.2, -kd * 1.15);
    o.flag = { fl[0], fl[1], mh + 1.7 };
    return o;
}

// sombrio: pedra negra, muralha torta com espinhos, torres finas e altíssimas, a agulha no meio; janelas acesas de verde
const RGB kEvil = { 150, 255, 140 };
CastleOut4 darkCastle(const Art& b, const RGB&)
{
    const CastleCfg& cfg = cfgOf(b);
    const double R = cfg.R * 1.02;
    const Loc L(b);
    Rng Rr(seedOf(b, 13));
    static const RGB stones[3] = { { 64, 60, 70 }, { 56, 54, 58 }, { 72, 58, 62 } };
    static const RGB roofs[3] = { { 84, 46, 96 }, { 40, 40, 46 }, { 110, 36, 40 } };
    const RGB stone = stones[static_cast<int>(std::floor(b.v * 3)) % 3], top = shade(stone, 0.8), roof = roofs[static_cast<int>(std::floor(b.v * 5)) % 3];
    auto glowWin = [](int n) { return Deco([n](QPainter& q, const PtFn& pt, double) { for (int i = 0; i < n; i++) { const double s = (i + 0.5) / n; quad(q, pt, s - 0.03, s + 0.03, 0.45, 0.72, QColor(150, 255, 140, 230)); } }); };
    CastleOut4 o;
    Faces& f = o.faces;
    const int n = 7;
    std::vector<P2> pts;
    for (int k = 0; k < n; k++) { const double a = double(k) / n * M_PI * 2 + 0.2, rr = R * (0.88 + Rr() * 0.22); pts.push_back({ std::cos(a) * rr, std::sin(a) * rr }); }
    for (int k = 0; k < n; k++) {
        const P2 a = L.at(pts[k][0], pts[k][1]), q = L.at(pts[(k + 1) % n][0], pts[(k + 1) % n][1]);
        const double mx = (a[0] + q[0]) / 2, my = (a[1] + q[1]) / 2, len = std::hypot(q[0] - a[0], q[1] - a[1]), yaw = std::atan2(q[1] - a[1], q[0] - a[0]);
        const double wh = cfg.wallH * 1.15;
        append(f, boxFaces(mx, my, yaw, len, 1.2, 0, wh, stone, &top, [len](int s, double) { return (s == 0 || s == 2) ? slitDeco(std::max(2, static_cast<int>(std::lround(len / 2.6)))) : Deco(); }));
        const int m = std::max(2, static_cast<int>(std::floor(len / 1.3)));
        for (int i = 0; i < m; i++) { const double t = (i + 0.5) / m; append(f, coneFaces(a[0] + (q[0] - a[0]) * t, a[1] + (q[1] - a[1]) * t, 0.42, wh, 0.9 + (i % 2) * 0.5, 4, top)); }
    }
    for (const P2& q : pts) {
        const P2 w = L.at(q[0], q[1]);
        const double tr = cfg.towerR * 0.8, th = cfg.towerH * (1.35 + Rr() * 0.4);
        append(f, taperFaces(w[0], w[1], tr, tr * 0.62, 0, th, 8, stone, [glowWin](int j) { return j % 2 ? glowWin(1) : Deco(); }));
        append(f, ringL(L, q[0], q[1], tr * 0.85, tr * 0.85, tr * 0.85, tr * 0.85, th, th + 0.4, 8, top));
        append(f, coneFaces(w[0], w[1], tr * 0.8, th + 0.4, th * 0.75, 8, roof));
    }
    // a agulha do meio e as quatro flechas em volta
    const double kr = cfg.kt[0] * 1.35, kt = cfg.kt[1] * 1.55;
    append(f, taperFaces(b.x, b.y, kr * 1.7, kr * 1.15, 0, kt * 0.42, 8, stone, [glowWin](int j) { return j % 2 ? glowWin(2) : Deco(); }));
    append(f, taperFaces(b.x, b.y, kr * 1.0, kr * 0.62, kt * 0.42, kt, 8, stone, [glowWin](int j) { return j % 3 == 0 ? glowWin(1) : Deco(); }));
    append(f, ringL(L, 0, 0, kr * 1.3, kr * 1.3, kr * 1.3, kr * 1.3, kt * 0.42, kt * 0.42 + 0.5, 8, top));
    append(f, coneFaces(b.x, b.y, kr * 0.75, kt, kt * 0.62, 8, roof));
    for (const auto& q : { P2{ 1, 1 }, P2{ 1, -1 }, P2{ -1, 1 }, P2{ -1, -1 } }) {
        const P2 w = L.at(q[0] * kr * 1.9, q[1] * kr * 1.9);
        const double h = kt * 0.55;
        append(f, taperFaces(w[0], w[1], kr * 0.45, kr * 0.32, 0, h, 6, stone)); append(f, coneFaces(w[0], w[1], kr * 0.4, h, h * 0.55, 6, roof));
    }
    o.flag = { b.x, b.y, kt + kt * 0.62 };
    o.orb = true; o.orbAt = { b.x, b.y, kt * 0.42 + 1.4 };
    return o;
}

// gelo: muralha de gelo com lascas, torres de cristal e o palácio de cristais tortos no meio
CastleOut4 iceCastle(const Art& b, const RGB&)
{
    const CastleCfg& cfg = cfgOf(b);
    const double R = cfg.R;
    const Loc L(b);
    Rng Rr(seedOf(b, 17));
    static const RGB ices[3] = { { 192, 228, 244 }, { 176, 214, 240 }, { 206, 232, 238 } };
    const RGB ice = ices[static_cast<int>(std::floor(b.v * 3)) % 3], top = { 236, 248, 252 }, deep = { 150, 196, 230 };
    CastleOut4 o;
    Faces& f = o.faces;
    const int n = 6;
    std::vector<P2> pts;
    for (int k = 0; k < n; k++) { const double a = double(k) / n * M_PI * 2 + M_PI / 6; pts.push_back({ std::cos(a) * R, std::sin(a) * R }); }
    for (int k = 0; k < n; k++) {
        const P2 a = L.at(pts[k][0], pts[k][1]), q = L.at(pts[(k + 1) % n][0], pts[(k + 1) % n][1]);
        const double mx = (a[0] + q[0]) / 2, my = (a[1] + q[1]) / 2, len = std::hypot(q[0] - a[0], q[1] - a[1]), yaw = std::atan2(q[1] - a[1], q[0] - a[0]);
        const double wh = cfg.wallH * 0.9;
        append(f, boxFaces(mx, my, yaw, len, 1.1, 0, wh, ice, &top, [](int s, double) {
            return (s == 0 || s == 2) ? linesDeco({ { 0.1, 0.2, 0.3, 0.9 }, { 0.6, 0.1, 0.75, 0.7 } }, QColor(255, 255, 255, 140), 0.6) : Deco();
        }));
        const int m = std::max(2, static_cast<int>(std::floor(len / 1.6)));
        for (int i = 0; i < m; i++) {
            const double t = (i + 0.5) / m, h = 0.8 + Rr() * 1.2;
            const double dx = (Rr() - 0.5) * 0.4, dy = (Rr() - 0.5) * 0.4;
            append(f, crystal({ a[0] + (q[0] - a[0]) * t, a[1] + (q[1] - a[1]) * t, wh }, { dx, dy, 1 }, h, 0.3, 4, top, 1.8));
        }
    }
    for (const P2& q : pts) {
        const P2 w = L.at(q[0], q[1]);
        const double tr = cfg.towerR * 0.8, th = cfg.towerH * (1.05 + Rr() * 0.3);
        append(f, crystal({ w[0], w[1], 0 }, { 0, 0, 1 }, th, tr, 6, ice, 2.0));
        for (int i = 0; i < 2; i++) {
            const double a = Rr() * M_PI * 2, hh = th * (0.35 + Rr() * 0.2);
            append(f, crystal({ w[0] + std::cos(a) * tr, w[1] + std::sin(a) * tr, 0 }, { std::cos(a) * 0.4, std::sin(a) * 0.4, 1 }, hh, tr * 0.45, 5, deep, 1.8));
        }
    }
    // o palácio: o cristal alto do meio e os tortos em volta
    const double kr = cfg.kt[0] * 1.4, kt = cfg.kt[1] * 1.3;
    append(f, crystal({ b.x, b.y, 0 }, { 0, 0, 1 }, kt, kr, 6, ice, 2.4));
    const int m = 5 + clampi(b.size, 0, 3);
    for (int i = 0; i < m; i++) {
        const double a = double(i) / m * M_PI * 2 + Rr() * 0.4, d = kr * (1.3 + Rr() * 0.5), tilt = 0.18 + Rr() * 0.3;
        const P2 w = L.at(std::cos(a) * d, std::sin(a) * d);
        const double len = kt * (0.4 + Rr() * 0.35), r = kr * (0.5 + Rr() * 0.25);
        append(f, crystal({ w[0], w[1], 0 }, { std::cos(a + b.yaw) * tilt, std::sin(a + b.yaw) * tilt, 1 }, len, r, 6, i % 2 ? deep : ice, 2.0));
    }
    o.flag = { b.x, b.y, kt + kr * 2.4 + 0.2 };
    o.frost = true; o.frostAt = { b.x, b.y, kt * 0.6 };
    return o;
}

bool drawStyledCastle4(QPainter& p, const Art& b, const Cam& cam, const RGB& realmCol)
{
    const QString& s = b.style;
    CastleOut4 o;
    if (s == QLatin1String("norman")) o = normanCastle(b, realmCol);
    else if (s == QLatin1String("moat")) o = moatCastle(b, realmCol);
    else if (s == QLatin1String("chateau")) o = chateauCastle(b, realmCol);
    else if (s == QLatin1String("crag")) o = cragCastle(b, realmCol, cam);
    else if (s == QLatin1String("oriental")) o = orientalCastle(b, realmCol);
    else if (s == QLatin1String("desert")) o = desertCastle(b, realmCol);
    else if (s == QLatin1String("dark")) o = darkCastle(b, realmCol);
    else if (s == QLatin1String("ice")) o = iceCastle(b, realmCol);
    else return false;
    if (!o.ground.empty()) drawFaces(p, o.ground, cam);     // o chão do castelo (ponte levadiça, jardim) vem antes
    drawFaces(p, o.faces, cam);
    if (o.orb) glowAt(p, cam, o.orbAt, 2.4, kEvil, 0.35);
    if (o.frost) glowAt(p, cam, o.frostAt, 5, { 190, 236, 255 }, 0.22);
    flag(p, cam, o.flag[0], o.flag[1], o.flag[2], realmCol);
    return true;
}

// ── sobrado do pincel Cidade ──
const RGB kCityWalls[6] = { { 234, 226, 208 }, { 218, 204, 180 }, { 200, 176, 146 }, { 228, 216, 194 }, { 186, 176, 166 }, { 222, 196, 168 } };
const RGB kCityRoofs[5] = { { 152, 74, 54 }, { 92, 96, 110 }, { 170, 94, 62 }, { 112, 82, 66 }, { 128, 64, 52 } };
Deco cityDeco(double len, int floors, bool door)
{
    return [=](QPainter& q, const PtFn& pt, double k) {
        const int n = std::max(1, static_cast<int>(std::floor(len / 1.25)));
        auto lit = [&](int i, int r) { return frac(std::sin((i + 1) * 12.9 + r * 7.7 + len * 3.1) * 999) > 0.45; };
        for (int r = 0; r < floors; r++) {
            const double t0 = (r + 0.32) / floors, t1 = t0 + 0.38 / floors;
            for (int i = 0; i < n; i++) {
                const double s = (i + 0.5) / n;
                if (door && r == 0 && std::fabs(s - 0.5) < 0.5 / n) continue;
                quad(q, pt, s - 0.1, s + 0.1, t0, t1, lit(i, r) ? QColor(255, 214, 120, 242) : rgb(DARK, k, 0.85));
            }
            if (r) { strokeLines(q, rgb(DARK, k, 0.22), 0.6); line(q, pt(0, double(r) / floors), pt(1, double(r) / floors)); }
        }
        if (door) quad(q, pt, 0.42, 0.58, 0, 0.62 / floors, rgb({ 88, 58, 38 }, k));
    };
}

} // namespace

// ════════════════════════ entrada ════════════════════════

void drawCityHouse(QPainter& p, const House& b, const Cam& cam, bool snowy)
{
    const double c = std::cos(b.yaw), s = std::sin(b.yaw);
    auto L = [&](double lx, double ly, double z) { return V3{ b.x + c * lx - s * ly, b.y + s * lx + c * ly, z }; };
    const int floors = std::max(2, b.floors);
    const double w = 2.8 + b.v * 1.4, d = 3.4, fh = 1.75, wh = floors * fh, rh = 2.0, hw = w / 2, hd = d / 2;
    const RGB wall = kCityWalls[static_cast<int>(std::floor(b.v * 11)) % 6];
    const RGB roof = snowy ? RGB{ 236, 240, 246 } : kCityRoofs[static_cast<int>(std::floor(b.v * 17)) % 5];
    const V3 A = L(-hw, -hd, 0), B = L(hw, -hd, 0), Cc = L(hw, hd, 0), D = L(-hw, hd, 0);
    auto up = [](const V3& q, double z) { return V3{ q[0], q[1], z }; };
    const V3 R1 = L(0, -hd, wh + rh), R2 = L(0, hd, wh + rh), ctr = { b.x, b.y, wh / 2 };
    // a cumeeira corre pro fundo: a fachada estreita com o oitão fica pra rua (como nas cidades velhas)
    Faces f;
    f.push_back(face({ B, Cc, up(Cc, wh), up(B, wh) }, wall, ctr, cityDeco(d, floors, false)));
    f.push_back(face({ D, A, up(A, wh), up(D, wh) }, wall, ctr, cityDeco(d, floors, false)));
    { Face q = face({ A, B, up(B, wh), R1, up(A, wh) }, wall, ctr, cityDeco(w, floors, true)); q.q = { 0, 1, 2, 4 }; f.push_back(std::move(q)); }
    { Face q = face({ Cc, D, up(D, wh), R2, up(Cc, wh) }, wall, ctr, cityDeco(w, floors, false)); q.q = { 0, 1, 2, 4 }; f.push_back(std::move(q)); }
    f.push_back(face({ up(B, wh - 0.05), up(Cc, wh - 0.05), R2, R1 }, roof, ctr, roofDeco, 0.01));
    f.push_back(face({ up(D, wh - 0.05), up(A, wh - 0.05), R1, R2 }, roof, ctr, roofDeco, 0.01));
    if (b.bell) {
        // campanário: torre fina de pedra com a flecha
        const V3 t = L(hw * 0.2, hd + 1.2, 0);
        append(f, boxFaces(t[0], t[1], b.yaw, 1.6, 1.6, 0, wh + rh + 3.2, { 206, 198, 182 }, nullptr, [](int, double) { return slitDeco(1); }));
        append(f, coneFaces(t[0], t[1], 1.0, wh + rh + 3.2, 3.0, 4, roof));
    }
    drawFaces(p, f, cam);
}

// paliçada: toras pontudas lado a lado (trechos colados como os da muralha de pedra)
void drawPalisadeSeg(QPainter& p, const Pt& a, const Pt& b, const Cam& cam, double k)
{
    static const RGB LOG = { 128, 94, 62 };
    const double len = std::hypot(b[0] - a[0], b[1] - a[1]) + 0.2, yaw = std::atan2(b[1] - a[1], b[0] - a[0]);
    const double c = std::cos(yaw), s = std::sin(yaw), ox = -s * 0.35 * k, oy = c * 0.35 * k;
    const int n = std::max(2, static_cast<int>(std::lround(len / (0.55 * k))));
    Faces faces;
    for (int side : { -1, 1 }) {
        std::vector<V3> v = { { a[0] + ox * side, a[1] + oy * side, -1.4 * k }, { b[0] + c * 0.1 + ox * side, b[1] + s * 0.1 + oy * side, -1.4 * k } };
        for (int i = n; i >= 0; i--) {
            const double t = double(i) / n;
            v.push_back({ a[0] + (b[0] + c * 0.1 - a[0]) * t + ox * side, a[1] + (b[1] + s * 0.1 - a[1]) * t + oy * side, (i % 2 ? 3.4 : 2.7) * k });
        }
        const int last = static_cast<int>(v.size()) - 1;
        Face q = face(v, side > 0 ? LOG : shade(LOG, 0.94), { (a[0] + b[0]) / 2, (a[1] + b[1]) / 2, 1 }, [n](QPainter& qq, const PtFn& pt, double) {
            strokeLines(qq, QColor(52, 34, 20, 140), 0.6);
            for (int i = 1; i < n; i++) line(qq, pt(double(i) / n, 0), pt(double(i) / n, 0.86));
        });
        q.q = { 0, 1, 2, last };
        faces.push_back(std::move(q));
    }
    drawFaces(p, faces, cam);
}
void drawPalisadeTower(QPainter& p, const Pt& at, const Cam& cam, double k)
{
    static const RGB LOG = { 128, 94, 62 };
    const double h = 5.2 * k, sz = 1.1 * k;
    Faces f;
    for (const auto& d : { P2{ -sz, -sz }, P2{ sz, -sz }, P2{ sz, sz }, P2{ -sz, sz } }) append(f, beamFaces({ at[0] + d[0], at[1] + d[1], -0.5 * k }, { at[0] + d[0], at[1] + d[1], h }, 0.3 * k, 0.3 * k, LOG));
    const RGB plat = { 140, 104, 68 }, platTop = { 116, 86, 56 };
    append(f, boxFaces(at[0], at[1], 0, 2.9 * k, 2.9 * k, h, h + 0.9 * k, plat, &platTop));
    append(f, coneFaces(at[0], at[1], 2.1 * k, h + 1.6 * k, 1.5 * k, 4, { 96, 72, 48 }));
    drawFaces(p, f, cam);
}

bool drawArt4(QPainter& p, const Art& a, const Cam& cam, const RGB& realmCol)
{
    const QString& k = a.kind;
    if (k == QLatin1String("castle")) return drawStyledCastle4(p, a, cam, realmCol);
    if (k == QLatin1String("colossus")) drawColossus(p, a, cam);
    else if (k == QLatin1String("guardians")) drawGuardians(p, a, cam);
    else if (k == QLatin1String("seatedgod")) drawSeatedGod(p, a, cam);
    else if (k == QLatin1String("cathedral")) drawCathedral(p, a, cam);
    else if (k == QLatin1String("arch")) drawArch(p, a, cam);
    else if (k == QLatin1String("market")) drawMarket(p, a, cam);
    else if (k == QLatin1String("aqueduct")) drawAqueduct(p, a, cam);
    else return false;
    return true;
}

MoatGeom moatGeom(int size, double k)
{
    const CastleCfg& cfg = kCastle[clampi(size, 0, 3)];
    return { cfg.R * k, cfg.R * 0.84 * k, (cfg.R * 0.42 + 2.6) * k };
}

bool castleLightRing(const Art& a, double& R, double& z)
{
    if (a.kind != QLatin1String("castle") || a.style != QLatin1String("crag")) return false;
    const CastleCfg& cfg = kCastle[clampi(a.size, 0, 3)];
    R = cfg.R * 0.9; z = cfg.wallH + 0.6;   // cada luz pega o chão do próprio lugar
    return true;
}

const QStringList& castleStyles()
{
    static const QStringList s = { QString(), QStringLiteral("norman"), QStringLiteral("moat"), QStringLiteral("chateau"), QStringLiteral("crag"),
                                   QStringLiteral("elven"), QStringLiteral("dwarf"), QStringLiteral("orc"), QStringLiteral("oriental"),
                                   QStringLiteral("desert"), QStringLiteral("dark"), QStringLiteral("ice") };
    return s;
}

} // namespace carto::spr
