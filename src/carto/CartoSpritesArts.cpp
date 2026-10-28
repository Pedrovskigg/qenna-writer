// Sprites das levas novas do Construir (porte do sprites2.js e sprites3.js do Map Lab):
//  · leva 2: estátua, navio, coliseu, caverna, mina, torre do mago, Árvore Sagrada,
//    círculo de pedras, acampamento e os castelos élfico, anão e orc;
//  · leva 3: dragão, ossada, serpente marinha, kraken, drakkar, naufrágio, pirâmide,
//    zigurate, pagode, obelisco, cemitério, portal, cabeças de pedra, ilha flutuante,
//    fazenda e espada na pedra.
// Mesma linguagem dos sólidos de CartoSprites.cpp: faces 3D no mundo, que giram com a
// mesa e recebem a luz da hora.

#include "CartoSprites.h"
#include "CartoSpritesKit.h"

#include <QHash>
#include <QLinearGradient>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>

namespace carto::spr {

using namespace kit;

// peças das levas novas (declaradas no CartoSpritesKit.h: a leva 4, em CartoSpritesArts4.cpp, usa as mesmas)
namespace kit {
double seedOf(const Art& b, int k) { return std::floor(b.v * 1e6) + k; }
RGB sc(const RGB& c, double k) { return { c[0] * k, c[1] * k, c[2] * k }; }
V3 add3(const V3& a, const V3& b) { return { a[0] + b[0], a[1] + b[1], a[2] + b[2] }; }
V3 sub3(const V3& a, const V3& b) { return { a[0] - b[0], a[1] - b[1], a[2] - b[2] }; }
V3 mul3(const V3& a, double k) { return { a[0] * k, a[1] * k, a[2] * k }; }
V3 cross(const V3& a, const V3& b) { return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] }; }
V3 norm(const V3& a) { double l = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); if (!l) l = 1; return { a[0] / l, a[1] / l, a[2] / l }; }
Face poly(std::vector<V3> v, const RGB& col, const V3& c, Deco deco, double bias) { return face(std::move(v), col, c, std::move(deco), bias); }
Face tri(const V3& a, const V3& b, const V3& c, const RGB& col, const V3& C) { Face f = face({ a, b, c }, col, C); f.flat = true; return f; }
Face flatPoly(std::vector<V3> v, const RGB& col, const V3& c, Deco deco) { Face f = face(std::move(v), col, c, std::move(deco)); f.flat = true; return f; }
QPainterPath pathOf(const std::vector<QPointF>& pts)
{
    QPainterPath path;
    if (pts.empty()) return path;
    path.moveTo(pts[0]);
    for (size_t i = 1; i < pts.size(); i++) path.lineTo(pts[i]);
    path.closeSubpath();
    return path;
}
void strokeLines(QPainter& p, const QColor& c, double w) { p.setPen(pen(c, w)); p.setBrush(Qt::NoBrush); }

// viga entre dois pontos 3D (braço, mastro, escora, espada): prisma de seção w × h
Faces beamFaces(const V3& p0, const V3& p1, double w, double h, const RGB& col)
{
    const V3 d = norm(sub3(p1, p0));
    V3 u = cross(d, { 0, 0, 1 });
    if (std::hypot(u[0], u[1], u[2]) < 0.1) u = cross(d, { 1, 0, 0 });
    u = norm(u);
    const V3 v = norm(cross(d, u));
    const V3 c = mul3(add3(p0, p1), 0.5);
    auto corner = [&](const V3& p, double su, double sv) { return add3(p, add3(mul3(u, su * w / 2), mul3(v, sv * h / 2))); };
    const V3 A[4] = { corner(p0, -1, -1), corner(p0, 1, -1), corner(p0, 1, 1), corner(p0, -1, 1) };
    const V3 B[4] = { corner(p1, -1, -1), corner(p1, 1, -1), corner(p1, 1, 1), corner(p1, -1, 1) };
    Faces f;
    for (int k = 0; k < 4; k++) f.push_back(face({ A[k], A[(k + 1) % 4], B[(k + 1) % 4], B[k] }, col, c));
    f.push_back(face({ A[3], A[2], A[1], A[0] }, col, c));
    f.push_back(face({ B[0], B[1], B[2], B[3] }, col, c));
    return f;
}
// anel elíptico (tronco, saia, torso achatado): de z0 com raios (ax0, ay0) até z1 com (ax1, ay1), girado por yaw
Faces ringFaces(double x, double y, double yaw, double ax0, double ay0, double ax1, double ay1, double z0, double z1, int n, const RGB& col,
                const std::function<Deco(int, int)>& deco)
{
    const double c = std::cos(yaw), s = std::sin(yaw);
    auto P = [&](double a, double rx, double ry, double z) { const double lx = std::cos(a) * rx, ly = std::sin(a) * ry; return V3{ x + c * lx - s * ly, y + s * lx + c * ly, z }; };
    Faces f;
    for (int k = 0; k < n; k++) {
        const double a0 = double(k) / n * M_PI * 2, a1 = double(k + 1) / n * M_PI * 2;
        f.push_back(face({ P(a0, ax0, ay0, z0), P(a1, ax0, ay0, z0), P(a1, ax1, ay1, z1), P(a0, ax1, ay1, z1) }, col, { x, y, (z0 + z1) / 2 }, deco ? deco(k, n) : Deco()));
    }
    return f;
}
// bolha facetada (copa, rocha, cúpula): esfera achatada com o raio sorteado vértice a vértice; T leva do sistema dela pro mundo
Faces blobGeneric(const std::function<V3(double, double, double)>& T, double rx, double ry, double rz, int n, int rings, const RGB& col, double seed,
                  double jitter, const std::vector<RGB>& cols)
{
    Rng R(seed);
    std::vector<std::vector<V3>> lat;
    for (int i = 0; i <= rings; i++) {
        const double phi = -M_PI / 2 + double(i) / rings * M_PI;
        std::vector<V3> row;
        for (int k = 0; k < n; k++) {
            const double th = double(k) / n * M_PI * 2 + (i % 2) * M_PI / n, j = 1 + (R() - 0.5) * 2 * jitter;
            row.push_back(T(std::cos(phi) * std::cos(th) * rx * j, std::cos(phi) * std::sin(th) * ry * j, std::sin(phi) * rz * j));
        }
        lat.push_back(std::move(row));
    }
    Faces f;
    const V3 C = T(0, 0, 0);
    for (int i = 0; i < rings; i++) for (int k = 0; k < n; k++) {
        const V3 a = lat[i][k], b = lat[i][(k + 1) % n], c = lat[i + 1][(k + 1) % n], d = lat[i + 1][k];
        const RGB fc = cols.empty() ? col : cols[static_cast<size_t>(std::floor(R() * cols.size()))];
        if (i == 0) f.push_back(face({ a, c, d }, fc, C));
        else if (i == rings - 1) f.push_back(face({ a, b, d }, fc, C));
        else f.push_back(face({ a, b, c, d }, fc, C));
    }
    return f;
}
Faces blobFaces(double cx, double cy, double cz, double rx, double rz, int n, int rings, const RGB& col, double seed, double jitter, const std::vector<RGB>& cols)
{
    return blobGeneric([=](double x, double y, double z) { return V3{ cx + x, cy + y, cz + z }; }, rx, rx, rz, n, rings, col, seed, jitter, cols);
}
// elipsoide no sistema do artefato (corpo, cabeça, coxa): raios diferentes em x, y e z
Faces ellip(const Loc& L, double lx, double ly, double lz, double rx, double ry, double rz, int n, int rings, const RGB& col, double seed, double jitter)
{
    return blobGeneric([&](double x, double y, double z) { return L(lx + x, ly + y, lz + z); }, rx, ry, rz, n, rings, col, seed, jitter, {});
}
// corta no plano z: fica só o que está acima (ou abaixo), achatando o resto no corte
Faces above(Faces f, double z)
{
    Faces out;
    for (Face& q : f) {
        if (std::none_of(q.v.begin(), q.v.end(), [z](const V3& p) { return p[2] > z + 0.02; })) continue;
        for (V3& p : q.v) p[2] = std::max(z, p[2]);
        out.push_back(std::move(q));
    }
    return out;
}
Faces below(Faces f, double z)
{
    Faces out;
    for (Face& q : f) {
        if (std::none_of(q.v.begin(), q.v.end(), [z](const V3& p) { return p[2] < z - 0.02; })) continue;
        for (V3& p : q.v) p[2] = std::min(z, p[2]);
        out.push_back(std::move(q));
    }
    return out;
}
// monte de pedra (caverna, mina): bolha cortada no chão
Faces moundFaces(double x, double y, double r, double h, double seed, const RGB& col)
{
    Faces f = blobFaces(x, y, 0, r, h, 12, 5, col, seed, 0.16, { col, sc(col, 0.9), sc(col, 1.06) });
    Faces out;
    for (Face& q : f) {
        if (std::none_of(q.v.begin(), q.v.end(), [](const V3& p) { return p[2] > 0.05; })) continue;
        for (V3& p : q.v) p[2] = std::max(0.0, p[2]);
        out.push_back(std::move(q));
    }
    return out;
}
// tubo por uma linha de pontos (pescoço, cauda, tentáculo, costela): vigas que afinam de w0 a w1
Faces tube(const std::vector<V3>& pts, double w0, double w1, const RGB& col, const RGB* alt)
{
    Faces f;
    for (size_t i = 0; i + 1 < pts.size(); i++) {
        const double t = double(i) / std::max<size_t>(1, pts.size() - 2), w = w0 + (w1 - w0) * t;
        const V3& a = pts[i]; const V3& b = pts[i + 1];
        const V3 d = sub3(b, a);
        append(f, beamFaces(sub3(a, mul3(d, 0.12)), add3(b, mul3(d, 0.12)), w, w, alt && i % 2 ? *alt : col));
    }
    return f;
}
void glowAt(QPainter& p, const Cam& cam, const V3& at, double rWorld, const RGB& col, double a, double minR)
{
    const QPointF g = cam.proj(at[0], at[1], at[2]), e = cam.proj(at[0], at[1], at[2] + rWorld);
    const double R = std::max(minR, std::hypot(e.x() - g.x(), e.y() - g.y()));
    QRadialGradient gr(g, R);
    gr.setColorAt(0, rgb(col, 1, a));
    gr.setColorAt(1, rgb(col, 1, 0));
    p.setPen(Qt::NoPen); p.setBrush(gr);
    p.drawEllipse(g, R, R);
}
// espuma: anel branco na linha d'água em volta de onde algo sai do mar
void foam(QPainter& p, const Cam& cam, const P2& at, double rx, double ry, double yaw, double a)
{
    const double c = std::cos(yaw), s = std::sin(yaw);
    std::vector<QPointF> pts;
    for (int i = 0; i < 16; i++) { const double t = i / 16.0 * M_PI * 2, lx = std::cos(t) * rx, ly = std::sin(t) * ry; pts.push_back(cam.proj(at[0] + c * lx - s * ly, at[1] + s * lx + c * ly, 0.02)); }
    p.setBrush(QColor(236, 246, 250, static_cast<int>(a * 0.28 * 255)));
    p.setPen(pen(QColor(246, 252, 255, static_cast<int>(a * 255)), 1.2));
    p.drawPath(pathOf(pts));
}
void groundShadow(QPainter& p, const Cam& cam, double x, double y, double r, double a)
{
    std::vector<QPointF> pts;
    for (int i = 0; i < 16; i++) { const double t = i / 16.0 * M_PI * 2; pts.push_back(cam.proj(x + std::cos(t) * r, y + std::sin(t) * r, 0.02)); }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 24, 18, static_cast<int>(a * 255)));
    p.drawPath(pathOf(pts));
}
Cam detailed(const Cam& cam) { Cam c = cam; c.detail = true; return c; }
} // namespace kit

namespace {

// ════════════════════════ leva 2 ════════════════════════

// ── estátua: herói de bronze esverdeado sobre pedestal em degraus, espada erguida e escudo ──
void drawStatue(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const RGB stone = { 196, 188, 172 }, bronze = { 96, 142, 122 }, top = { 210, 202, 186 };
    Faces f = boxFaces(b.x, b.y, b.yaw, 3.2, 3.2, 0, 0.5, { 176, 168, 152 });
    append(f, boxFaces(b.x, b.y, b.yaw, 2.5, 2.5, 0.5, 1.0, stone));
    append(f, boxFaces(b.x, b.y, b.yaw, 1.9, 1.9, 1.0, 2.4, stone, &top, [](int k, double) -> Deco {
        if (k != 0) return Deco();
        return [](QPainter& q, const PtFn& pt, double kk) { quad(q, pt, 0.18, 0.82, 0.35, 0.62, rgb({ 150, 140, 122 }, kk, 0.9)); };
    }));
    const double Z = 2.4, H = 4.4;                         // a figura: 4,4 de altura sobre o pedestal
    auto z = [&](double t) { return Z + t * H; };
    const P2 o = L.at(0, 0);
    // pernas, com o passo: uma à frente
    append(f, beamFaces(L(0.28, -0.22, z(0.47)), L(0.36, -0.34, z(0.02)), 0.36, 0.42, bronze));
    append(f, beamFaces(L(-0.28, 0.22, z(0.47)), L(-0.18, 0.4, z(0.02)), 0.36, 0.42, bronze));
    // quadril e tronco (ombro mais largo que a cintura), capa caindo atrás
    append(f, ringFaces(o[0], o[1], b.yaw, 0.42, 0.34, 0.46, 0.36, z(0.45), z(0.55), 8, bronze));
    append(f, ringFaces(o[0], o[1], b.yaw, 0.46, 0.36, 0.62, 0.42, z(0.55), z(0.8), 8, bronze));
    append(f, ringFaces(o[0], o[1], b.yaw, 0.62, 0.42, 0.3, 0.24, z(0.8), z(0.84), 8, bronze));
    f.push_back(poly({ L(-0.55, -0.42, z(0.82)), L(-0.55, 0.42, z(0.82)), L(-0.8, 0.55, z(0.22)), L(-0.8, -0.55, z(0.22)) }, { 84, 126, 108 }, { o[0], o[1], z(0.5) }));
    // pescoço e cabeça (dois troncos de cone fazem a cabeça redonda)
    append(f, ringFaces(o[0], o[1], b.yaw, 0.15, 0.14, 0.15, 0.14, z(0.84), z(0.88), 6, bronze));
    const P2 hc = L.at(0.02, 0);
    append(f, ringFaces(hc[0], hc[1], b.yaw, 0.2, 0.18, 0.26, 0.23, z(0.88), z(0.94), 8, bronze));
    append(f, ringFaces(hc[0], hc[1], b.yaw, 0.26, 0.23, 0.12, 0.1, z(0.94), z(1.0), 8, bronze));
    // braço da espada erguido (cotovelo dobrado) e a espada
    const V3 sh = L(0.05, -0.58, z(0.79)), el = L(0.25, -0.72, z(0.97)), ha = L(0.32, -0.62, z(1.16));
    append(f, beamFaces(sh, el, 0.24, 0.26, bronze)); append(f, beamFaces(el, ha, 0.22, 0.22, bronze));
    append(f, beamFaces(L(0.3, -0.48, z(1.17)), L(0.3, -0.8, z(1.17)), 0.12, 0.12, { 140, 160, 150 }));
    append(f, beamFaces(L(0.32, -0.62, z(1.19)), L(0.4, -0.6, z(1.62)), 0.1, 0.05, { 178, 196, 190 }));
    // braço do escudo e o escudo à frente do corpo
    append(f, beamFaces(L(0.05, 0.58, z(0.79)), L(0.42, 0.62, z(0.6)), 0.24, 0.26, bronze));
    const P2 sc0 = L.at(0.55, 0.62);
    append(f, ringFaces(sc0[0], sc0[1], b.yaw + M_PI / 2, 0.06, 0.42, 0.06, 0.42, z(0.48), z(0.74), 8, { 104, 150, 128 }));
    drawFaces(p, f, cam);
}

// ── navio: galeão de três mastros, castelo de popa, velas redondas e a bandeira do território ──
void drawShip(QPainter& p, const Art& b, const Cam& cam, const RGB& realmCol)
{
    const Loc L(b);
    static const RGB hulls[3] = { { 104, 72, 46 }, { 90, 62, 42 }, { 118, 82, 50 } };
    const RGB hull = hulls[static_cast<int>(std::floor(b.v * 3)) % 3], deckCol = { 176, 142, 100 };
    const double DZ = 1.4;
    // contorno do convés, da popa (−6) à proa (+6,6)
    static const double out[6][2] = { { -6, 1.7 }, { -3, 1.85 }, { 1, 1.8 }, { 3.8, 1.4 }, { 5.6, 0.7 }, { 6.6, 0 } };
    std::vector<V3> deckL, deckR, keelL, keelR;
    for (const auto& q : out) { deckL.push_back(L(q[0], -q[1], DZ)); deckR.push_back(L(q[0], q[1], DZ)); keelL.push_back(L(q[0] * 0.94, -q[1] * 0.42, 0)); keelR.push_back(L(q[0] * 0.94, q[1] * 0.42, 0)); }
    const V3 C0 = { b.x, b.y, 0.6 };
    const Deco planks = [](QPainter& q, const PtFn& pt, double) { strokeLines(q, QColor(40, 24, 14, 102), 0.6); line(q, pt(0, 0.55), pt(1, 0.55)); line(q, pt(0, 0.82), pt(1, 0.82)); };
    Faces f;
    for (int i = 0; i < 5; i++) {
        f.push_back(poly({ keelL[i], keelL[i + 1], deckL[i + 1], deckL[i] }, hull, C0, planks));
        f.push_back(poly({ keelR[i], keelR[i + 1], deckR[i + 1], deckR[i] }, hull, C0, planks));
    }
    f.push_back(poly({ keelL[0], keelR[0], deckR[0], deckL[0] }, hull, C0));
    std::vector<V3> deck = deckL;
    for (int i = 5; i >= 0; i--) deck.push_back(deckR[i]);
    f.push_back(poly(deck, deckCol, { b.x, b.y, 0 }));
    // castelo de popa com janelinhas e o de proa, menor
    const RGB cabin = { 128, 90, 58 }, cabinTop = { 160, 124, 86 };
    const P2 s0 = L.at(-4.6, 0);
    for (Face& q : boxFaces(s0[0], s0[1], b.yaw, 2.8, 3.3, DZ, DZ + 1.5, cabin, &cabinTop, [](int k, double) -> Deco {
             if (k != 2) return Deco();
             return [](QPainter& q, const PtFn& pt, double) { for (int i = 0; i < 4; i++) { const double s = 0.15 + i * 0.2; quad(q, pt, s, s + 0.1, 0.35, 0.7, QColor(255, 210, 120, 217)); } };
         })) { q.bias = 0.02; f.push_back(std::move(q)); }
    const P2 f0 = L.at(4.2, 0);
    for (Face& q : boxFaces(f0[0], f0[1], b.yaw, 1.6, 2.4, DZ, DZ + 0.8, cabin, &cabinTop)) { q.bias = 0.02; f.push_back(std::move(q)); }
    drawFaces(p, f, cam);
    // gurupés, mastros e velas (as velas são planos, desenhados dos dois lados)
    const RGB wood = { 74, 54, 36 };
    static const double masts[3][2] = { { -2.4, 8.6 }, { 0.8, 10.4 }, { 3.4, 8.2 } };
    Faces rig = beamFaces(L(5.8, 0, DZ + 0.5), L(8.2, 0, DZ + 1.9), 0.14, 0.14, wood);
    for (const auto& m : masts) append(rig, beamFaces(L(m[0], 0, DZ), L(m[0], 0, DZ + m[1]), 0.28, 0.28, wood));
    drawFaces(p, rig, cam);
    Faces sails;
    const RGB canvas = b.v > 0.5 ? RGB{ 242, 234, 214 } : RGB{ 230, 214, 186 }, lateen = { 236, 224, 200 };
    for (const auto& m : masts) {
        const double mx = m[0], mh = m[1];
        for (const auto& t : { std::array<double, 3>{ 0.25, 0.58, 1.9 }, std::array<double, 3>{ 0.62, 0.9, 1.45 } }) {
            const double z0 = DZ + mh * t[0], z1 = DZ + mh * t[1], half = t[2], bulge = 0.35;
            Face s = flatPoly({ L(mx + 0.25, -half, z0), L(mx + 0.25 + bulge, 0, z0 - 0.15), L(mx + 0.25, half, z0), L(mx + 0.25, half * 0.92, z1), L(mx + 0.25 + bulge * 0.6, 0, z1), L(mx + 0.25, -half * 0.92, z1) },
                              canvas, L(mx - 2, 0, (z0 + z1) / 2), [](QPainter& q, const PtFn& pt, double) { strokeLines(q, QColor(120, 100, 76, 89), 0.6); for (int i = 1; i < 4; i++) line(q, pt(i / 4.0, 0), pt(i / 4.0, 1)); });
            s.q = { 0, 2, 3, 5 };
            sails.push_back(std::move(s));
        }
    }
    // bujarrona (do gurupés ao topo do mastro de proa) e a vela latina na mezena
    sails.push_back(tri(L(8.1, 0, DZ + 1.9), L(3.5, 0, DZ + 7.8), L(5.4, 0, DZ + 1.0), lateen, L(5, 1, DZ + 3)));
    sails.push_back(tri(L(-2.4, 0, DZ + 1.6), L(-5.6, 0, DZ + 1.8), L(-2.4, 0, DZ + 7.4), lateen, L(-3, 1, DZ + 3)));
    drawFaces(p, sails, detailed(cam));
    const P2 fm = L.at(0.8, 0);
    flag(p, cam, fm[0], fm[1], DZ + 10.4, realmCol);
}

// ── coliseu: anel oval de arcadas em três andares, arena de areia; um lado já caiu ──
void drawColosseum(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const RGB stone = { 214, 196, 164 };
    const int n = 28;
    const double RX = 10, RY = 7.6, IX = 6.2, IY = 4.2;
    Rng R(seedOf(b, 5));
    const int broken0 = static_cast<int>(std::floor(R() * n));
    auto P = [&](double a, double rx, double ry, double z) { return L(std::cos(a) * rx, std::sin(a) * ry, z); };
    auto arches = [](int rows) -> Deco {
        return [rows](QPainter& q, const PtFn& pt, double kk) {
            for (int r = 0; r < rows; r++) {
                const double t0 = double(r) / rows + 0.06, t1 = double(r + 1) / rows - 0.08;
                std::vector<QPointF> pts = { pt(0.22, t0), pt(0.22, t0 + (t1 - t0) * 0.62) };
                for (int i = 0; i <= 6; i++) { const double a = M_PI - i / 6.0 * M_PI; pts.push_back(pt(0.5 + std::cos(a) * 0.28, t0 + (t1 - t0) * (0.62 + std::sin(a) * 0.38))); }
                pts.push_back(pt(0.78, t0));
                q.fillPath(pathOf(pts), rgb({ 70, 54, 40 }, kk, 0.85));
            }
            strokeLines(q, QColor(110, 90, 64, 128), 0.6);
            for (int r = 1; r < rows; r++) line(q, pt(0, double(r) / rows), pt(1, double(r) / rows));
        };
    };
    const Deco seats = [](QPainter& q, const PtFn& pt, double) { strokeLines(q, QColor(120, 100, 70, 115), 0.6); for (int i = 1; i < 6; i++) line(q, pt(0, i / 6.0), pt(1, i / 6.0)); };
    Faces f;
    std::vector<V3> sand;
    for (int k = 0; k < n; k++) sand.push_back(P(double(k) / n * M_PI * 2, IX, IY, 0.15));
    f.push_back(flatPoly(sand, { 222, 196, 146 }, { b.x, b.y, -1 }));
    for (int k = 0; k < n; k++) {
        const double a0 = double(k) / n * M_PI * 2, a1 = double(k + 1) / n * M_PI * 2;
        const int rel = (k - broken0 + n) % n;
        const bool broken = rel < 6;                     // seis vãos do lado caído só têm dois andares
        const double h = broken ? 3.6 - (rel % 3) * 0.5 : 5.4;
        f.push_back(poly({ P(a0, RX, RY, 0), P(a1, RX, RY, 0), P(a1, RX, RY, h), P(a0, RX, RY, h) }, stone, { b.x, b.y, 2.5 }, arches(broken ? 2 : 3)));
        f.push_back(poly({ P(a0, IX, IY, 0), P(a1, IX, IY, 0), P(a1, IX, IY, 2.2), P(a0, IX, IY, 2.2) }, { 196, 180, 150 }, { b.x, b.y, 8 }, arches(1)));
        // arquibancada: rampa da muralha de dentro até a de fora, em degraus
        f.push_back(poly({ P(a0, IX, IY, 2.2), P(a1, IX, IY, 2.2), P(a1, RX - 0.5, RY - 0.5, h - 0.4), P(a0, RX - 0.5, RY - 0.5, h - 0.4) }, { 200, 184, 154 }, { b.x, b.y, -4 }, seats));
        f.push_back(poly({ P(a0, RX, RY, h), P(a1, RX, RY, h), P(a1, RX - 0.5, RY - 0.5, h - 0.4), P(a0, RX - 0.5, RY - 0.5, h - 0.4) }, { 226, 210, 180 }, { b.x, b.y, -2 }));
    }
    drawFaces(p, f, cam);
}

// ── entrada de caverna: monte de rocha com a boca escura virada pra frente, estalactites e duas tochas ──
void drawCave(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const RGB rock = { 118, 110, 100 };
    const P2 m = L.at(-1.2, 0);
    Faces f = moundFaces(m[0], m[1], 5.4, 4.6, std::floor(b.v * 1e5) + 3, rock);
    // a boca: um arco escuro na frente do monte, com dentes de pedra no alto
    std::vector<V3> mouth;
    for (int i = 0; i <= 10; i++) { const double a = M_PI - i / 10.0 * M_PI; mouth.push_back(L(3.75 - std::sin(a) * 0.6, std::cos(a) * 1.7, std::sin(a) * 2.6)); }
    Face mf = poly(mouth, { 22, 18, 16 }, L(-2, 0, 1), [rock](QPainter& q, const PtFn& pt, double) {
        for (int i = 1; i < 6; i++) { const double s0 = 0.12 + i * 0.13; q.fillPath(pathOf({ pt(s0 - 0.05, 0.98), pt(s0, 0.72), pt(s0 + 0.05, 0.98) }), rgb(rock, 0.7)); }
    }, 0.08);
    mf.q = { 0, 10, 5, 5 };
    f.push_back(std::move(mf));
    // pedregulhos na frente
    Rng R(seedOf(b, 11));
    for (int k = 0; k < 4; k++) {
        const double lx = 4.6 + R() * 1.6, ly = (R() - 0.5) * 5.4, r = 0.5 + R() * 0.4, sd = std::floor(R() * 1e5);
        const P2 q = L.at(lx, ly);
        append(f, blobFaces(q[0], q[1], 0, r, 0.45, 6, 3, sc(rock, 0.92), sd, 0.2));
    }
    // tochas nos lados da boca
    for (int sg : { -1, 1 }) append(f, beamFaces(L(4.2, sg * 2.3, 0), L(4.2, sg * 2.3, 2.2), 0.14, 0.14, { 70, 50, 34 }));
    drawFaces(p, f, cam);
    for (int sg : { -1, 1 }) glowAt(p, cam, L(4.2, sg * 2.3, 2.4), 0.7, { 255, 170, 80 }, 0.9);
}

// ── mina: boca escorada com madeira na encosta, trilhos, vagonete com minério e um galpão ──
void drawMine(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const RGB rock = { 126, 116, 102 }, wood = { 116, 84, 54 };
    const P2 m = L.at(-1.6, 0);
    Faces f = moundFaces(m[0], m[1], 4.6, 3.6, std::floor(b.v * 1e5) + 9, rock);
    f.push_back(poly({ L(2.95, -1.0, 0), L(2.95, 1.0, 0), L(2.95, 1.0, 2.0), L(2.95, -1.0, 2.0) }, { 20, 16, 14 }, L(-2, 0, 1), {}, 0.08));
    append(f, beamFaces(L(3.15, -1.15, 0), L(3.15, -1.15, 2.3), 0.3, 0.3, wood)); append(f, beamFaces(L(3.15, 1.15, 0), L(3.15, 1.15, 2.3), 0.3, 0.3, wood));
    append(f, beamFaces(L(3.15, -1.5, 2.35), L(3.15, 1.5, 2.35), 0.34, 0.34, wood));
    // trilhos e dormentes saindo da boca
    for (int i = 0; i < 7; i++) { const P2 q = L.at(3.6 + i * 0.85, 0); append(f, boxFaces(q[0], q[1], b.yaw + M_PI / 2, 1.5, 0.28, 0.02, 0.1, { 96, 72, 50 })); }
    for (double sg : { -0.5, 0.5 }) append(f, beamFaces(L(3.2, sg, 0.14), L(9.4, sg, 0.14), 0.1, 0.08, { 120, 120, 126 }));
    // vagonete com minério
    const P2 c = L.at(6.6, 0);
    const RGB cartTop = { 70, 56, 44 };
    append(f, boxFaces(c[0], c[1], b.yaw, 1.6, 1.1, 0.3, 1.1, { 88, 70, 52 }, &cartTop));
    append(f, above(blobFaces(c[0], c[1], 1.1, 0.6, 0.3, 6, 3, { 86, 82, 92 }, 77, 0.3), 1.08));
    // monte de minério e o galpão de tábuas
    const P2 o = L.at(5.2, 2.8);
    append(f, above(blobFaces(o[0], o[1], 0, 1.3, 0.9, 8, 3, { 96, 90, 98 }, 31, 0.25), 0));
    const P2 g = L.at(5.4, -3.2);
    const RGB shedTop = { 120, 90, 62 };
    append(f, boxFaces(g[0], g[1], b.yaw, 2.6, 2.0, 0, 1.6, { 150, 116, 80 }, &shedTop));
    f.push_back(poly({ L(4.0, -4.3, 1.6), L(6.8, -4.3, 1.6), L(6.8, -2.1, 2.3), L(4.0, -2.1, 2.3) }, { 96, 70, 50 }, { g[0], g[1], 0 }, roofDeco, 0.02));
    drawFaces(p, f, cam);
}

// ── torre do mago: pedra lilás em espiral, sacada, telhado cônico e um cristal flutuando que brilha ──
void drawMageTower(QPainter& p, const Art& b, const Cam& cam)
{
    const RGB stone = { 150, 142, 172 }, dark = { 104, 92, 140 };
    auto spiral = [dark](int k, int n) -> Deco {
        return [dark, k, n](QPainter& q, const PtFn& pt, double kk) {
            for (int i = 0; i < 5; i++) { const double t = (i + double(k) / n) / 5; q.fillPath(pathOf({ pt(0, t), pt(1, t + 0.2), pt(1, t + 0.25), pt(0, t + 0.05) }), rgb(dark, kk, 0.55)); }
            if (k % 3 == 0) quad(q, pt, 0.35, 0.65, 0.66, 0.78, QColor(210, 180, 255, 217));
        };
    };
    Faces f = taperFaces(b.x, b.y, 2.3, 1.7, 0, 2.0, 10, { 132, 124, 150 });
    append(f, ringFaces(b.x, b.y, 0, 1.5, 1.5, 1.12, 1.12, 2.0, 12.6, 10, stone, spiral));
    append(f, ringFaces(b.x, b.y, 0, 1.75, 1.75, 1.75, 1.75, 9.6, 9.95, 10, { 120, 110, 146 }));
    append(f, coneFaces(b.x, b.y, 1.7, 12.6, 3.2, 10, { 92, 64, 150 }));
    drawFaces(p, f, cam);
    // o cristal (octaedro) e duas pedras que orbitam
    const double cz = 17.2, cr = 0.7;
    const RGB crystal = { 176, 140, 255 };
    const V3 top = { b.x, b.y, cz + 1.3 }, bot = { b.x, b.y, cz - 1.3 };
    V3 eq[4];
    for (int k = 0; k < 4; k++) { const double a = k / 4.0 * M_PI * 2 + b.v * 2; eq[k] = { b.x + std::cos(a) * cr, b.y + std::sin(a) * cr, cz }; }
    Faces cf;
    for (int k = 0; k < 4; k++) { const V3 q = eq[(k + 1) % 4]; cf.push_back(face({ eq[k], q, top }, crystal, { b.x, b.y, cz })); cf.push_back(face({ q, eq[k], bot }, sc(crystal, 0.85), { b.x, b.y, cz })); }
    glowAt(p, cam, { b.x, b.y, cz }, 3.4, { 186, 150, 255 }, 0.5);
    for (int k = 0; k < 2; k++) { const double a = b.v * 6 + k * M_PI; append(cf, blobFaces(b.x + std::cos(a) * 2.4, b.y + std::sin(a) * 2.4, 15.8 + k * 0.8, 0.35, 0.3, 6, 3, { 120, 110, 140 }, 40 + k, 0.25)); }
    drawFaces(p, cf, cam);
}

// ── Árvore Sagrada: tronco colossal com raízes, galhos abertos e uma copa em camadas ──
void drawGreatTree(QPainter& p, const Art& b, const Cam& cam)
{
    const RGB bark = { 104, 78, 56 };
    Faces f = taperFaces(b.x, b.y, 2.6, 1.5, 0, 10, 10, bark, [](int) -> Deco {
        return [](QPainter& q, const PtFn& pt, double) { strokeLines(q, QColor(50, 34, 22, 102), 0.7); for (int i = 1; i < 4; i++) line(q, pt(i / 4.0, 0), pt(i / 4.0 + 0.05, 1)); };
    });
    for (int k = 0; k < 6; k++) { const double a = k / 6.0 * M_PI * 2 + b.v; append(f, beamFaces({ b.x + std::cos(a) * 1.6, b.y + std::sin(a) * 1.6, 1.4 }, { b.x + std::cos(a) * 4.2, b.y + std::sin(a) * 4.2, 0 }, 0.8, 0.7, sc(bark, 0.92))); }
    for (int k = 0; k < 5; k++) { const double a = k / 5.0 * M_PI * 2 + b.v * 3; append(f, beamFaces({ b.x, b.y, 8.4 }, { b.x + std::cos(a) * 5.4, b.y + std::sin(a) * 5.4, 12.2 }, 0.7, 0.7, bark)); }
    drawFaces(p, f, cam);
    const std::vector<RGB> leaf = { { 70, 120, 62 }, { 82, 136, 70 }, { 62, 108, 58 }, { 96, 146, 78 } };
    Rng R(seedOf(b, 21));
    Faces canopy = blobFaces(b.x, b.y, 14.6, 7.4, 3.8, 14, 6, leaf[0], 101, 0.14, leaf);
    for (int k = 0; k < 6; k++) { const double a = k / 6.0 * M_PI * 2 + R(), zz = 12.6 + R() * 2; append(canopy, blobFaces(b.x + std::cos(a) * 5.6, b.y + std::sin(a) * 5.6, zz, 3.4, 2.4, 10, 4, leaf[1], 200 + k, 0.16, leaf)); }
    append(canopy, blobFaces(b.x, b.y, 18.2, 4.4, 2.6, 12, 5, leaf[3], 303, 0.14, leaf));
    drawFaces(p, canopy, cam);
}

// ── círculo de pedras: monólitos em roda, alguns com a laje por cima, um caído, o altar no meio ──
void drawStones(QPainter& p, const Art& b, const Cam& cam)
{
    const RGB stone = { 156, 154, 146 };
    Rng R(seedOf(b, 31));
    const int n = 10;
    const double RR = 5.2;
    Faces f = boxFaces(b.x, b.y, b.yaw, 2.2, 1.2, 0, 0.6, { 140, 138, 130 });
    std::vector<std::array<double, 4>> tops;
    std::vector<bool> has;
    for (int k = 0; k < n; k++) {
        const double a = double(k) / n * M_PI * 2 + b.yaw, x = b.x + std::cos(a) * RR, y = b.y + std::sin(a) * RR;
        if (k == 6) { append(f, boxFaces(x + std::cos(a) * 1.4, y + std::sin(a) * 1.4, a, 2.9, 0.9, 0, 0.6, sc(stone, 0.9))); tops.push_back({}); has.push_back(false); continue; }
        const double h = 3.0 + R() * 0.9, shade = 0.9 + R() * 0.15;
        append(f, boxFaces(x, y, a + M_PI / 2, 1.0, 0.6, 0, h, sc(stone, shade), nullptr, [](int, double) -> Deco {
            return [](QPainter& q, const PtFn& pt, double) { strokeLines(q, QColor(70, 70, 64, 89), 0.6); line(q, pt(0.3, 0.1), pt(0.45, 0.8)); };
        }));
        tops.push_back({ x, y, h, a }); has.push_back(true);
    }
    for (int k = 0; k < n; k += 2) {
        const int j = (k + 1) % n;
        if (!has[k] || !has[j]) continue;
        const double z = std::min(tops[k][2], tops[j][2]);
        append(f, beamFaces({ tops[k][0], tops[k][1], z + 0.3 }, { tops[j][0], tops[j][1], z + 0.3 }, 0.62, 0.55, { 146, 144, 136 }));
    }
    drawFaces(p, f, cam);
}

// ── acampamento: tendas de lona em volta da fogueira, uma tenda grande e redonda ──
void drawCamp(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    Rng R(seedOf(b, 41));
    static const RGB canvas[4] = { { 214, 194, 150 }, { 176, 104, 78 }, { 196, 170, 120 }, { 150, 120, 90 } };
    Faces f;
    for (int k = 0; k < 4; k++) {
        const double a = k / 4.0 * M_PI * 2 + 0.4, d = 3.6, yaw = a + M_PI / 2;
        const P2 t = L.at(std::cos(a) * d, std::sin(a) * d);
        const RGB col = canvas[static_cast<int>(std::floor(R() * 4))];
        const double c = std::cos(yaw), s = std::sin(yaw);
        auto P = [&](double lx, double ly, double z) { return V3{ t[0] + c * lx - s * ly, t[1] + s * lx + c * ly, z }; };
        const double w = 1.3, l = 1.5, h = 1.7;
        const V3 C = { t[0], t[1], 0.5 };
        f.push_back(poly({ P(-l, -w, 0), P(l, -w, 0), P(l, 0, h), P(-l, 0, h) }, col, C));
        f.push_back(poly({ P(-l, w, 0), P(l, w, 0), P(l, 0, h), P(-l, 0, h) }, col, C));
        Face door = poly({ P(l, -w, 0), P(l, w, 0), P(l, 0, h) }, sc(col, 0.92), C, [](QPainter& q, const PtFn& pt, double) {
            q.fillPath(pathOf({ pt(0.35, 0), pt(0.5, 0.7), pt(0.65, 0) }), QColor(40, 28, 20, 204));
        });
        door.q = { 0, 1, 2, 2 };
        f.push_back(std::move(door));
        f.push_back(poly({ P(-l, -w, 0), P(-l, w, 0), P(-l, 0, h) }, sc(col, 0.92), C));
    }
    // a tenda grande, redonda, do chefe
    const P2 bt = L.at(-0.6, -6.2);
    append(f, taperFaces(bt[0], bt[1], 2.4, 2.4, 0, 1.4, 12, { 190, 160, 116 }));
    append(f, coneFaces(bt[0], bt[1], 2.6, 1.4, 1.9, 12, { 164, 92, 66 }));
    // fogueira: pedras em roda e as toras
    for (int k = 0; k < 7; k++) { const double a = k / 7.0 * M_PI * 2; const P2 q = L.at(std::cos(a) * 0.9, std::sin(a) * 0.9); append(f, blobFaces(q[0], q[1], 0, 0.25, 0.2, 5, 2, { 120, 116, 108 }, 50 + k, 0.2)); }
    append(f, beamFaces(L(-0.6, -0.3, 0.1), L(0.6, 0.3, 0.35), 0.22, 0.22, { 80, 56, 36 }));
    append(f, beamFaces(L(-0.5, 0.4, 0.1), L(0.5, -0.4, 0.35), 0.22, 0.22, { 80, 56, 36 }));
    drawFaces(p, f, cam);
    // o fogo (de dia também se vê a chama)
    const V3 f0 = L(0, 0, 0.3), f1 = L(0, 0, 1.6);
    const QPointF X = cam.proj(f0[0], f0[1], f0[2]), X2 = cam.proj(f1[0], f1[1], f1[2]);
    const double hh = std::max(3.0, std::hypot(X2.x() - X.x(), X2.y() - X.y()));
    QPainterPath fl;
    fl.moveTo(X.x() - hh * 0.35, X.y());
    fl.quadTo(X.x() - hh * 0.3, X.y() - hh * 0.6, X.x(), X.y() - hh);
    fl.quadTo(X.x() + hh * 0.3, X.y() - hh * 0.6, X.x() + hh * 0.35, X.y());
    fl.closeSubpath();
    p.fillPath(fl, QColor(255, 150, 50, 242));
    QPainterPath in;
    in.moveTo(X.x() - hh * 0.18, X.y());
    in.quadTo(X.x(), X.y() - hh * 0.75, X.x() + hh * 0.18, X.y());
    in.closeSubpath();
    p.fillPath(in, QColor(255, 230, 140, 242));
}

// ── castelos de outros povos (mesmos quatro tamanhos do castelo humano) ──
struct CastleOut { Faces faces; V3 flag{}; std::vector<V3> smoke; bool hasFire = false; V3 fire{}; };
// élfico: muralha baixa e clara, torres redondas finas com flechas altas, a torre do meio bem alta
CastleOut elvenCastle(const Art& b, const RGB& realmCol)
{
    const CastleCfg& cfg = kCastle[std::max(0, std::min(3, b.size))];
    const double R = cfg.R * 0.95;
    const RGB white = { 236, 234, 226 }, wallTop = { 214, 214, 206 }, roof = { realmCol[0] * 0.75 + 40, realmCol[1] * 0.75 + 40, realmCol[2] * 0.75 + 40 };
    const Loc L(b);
    const int n = 6;
    P2 pts[6];
    for (int k = 0; k < n; k++) { const double a = double(k) / n * M_PI * 2 + M_PI / 6; pts[k] = L.at(std::cos(a) * R, std::sin(a) * R); }
    CastleOut o;
    Faces& f = o.faces;
    const Deco arcs = [](QPainter& q, const PtFn& pt, double kk) {
        for (int i = 0; i < 4; i++) {
            const double s0 = (i + 0.5) / 4;
            QPainterPath path;
            path.moveTo(pt(s0 - 0.05, 0.25)); path.lineTo(pt(s0 - 0.05, 0.6)); path.quadTo(pt(s0, 0.85), pt(s0 + 0.05, 0.6)); path.lineTo(pt(s0 + 0.05, 0.25)); path.closeSubpath();
            q.fillPath(path, rgb({ 120, 150, 170 }, kk, 0.6));
        }
    };
    for (int k = 0; k < n; k++) {
        const P2& a = pts[k]; const P2& q = pts[(k + 1) % n];
        const double mx = (a[0] + q[0]) / 2, my = (a[1] + q[1]) / 2, len = std::hypot(q[0] - a[0], q[1] - a[1]);
        append(f, boxFaces(mx, my, std::atan2(q[1] - a[1], q[0] - a[0]), len, 0.7, 0, cfg.wallH * 0.75, white, &wallTop, [arcs](int side, double) { return side == 0 || side == 2 ? arcs : Deco(); }));
        const double tr = cfg.towerR * 0.7, th = cfg.towerH * 1.25;
        append(f, taperFaces(a[0], a[1], tr, tr * 0.85, 0, th, 10, white, [](int i) { return i % 3 == 0 ? slitDeco(1) : Deco(); }));
        append(f, ringFaces(a[0], a[1], 0, tr * 1.15, tr * 1.15, tr * 1.15, tr * 1.15, th, th + 0.3, 10, { 210, 210, 202 }));
        append(f, coneFaces(a[0], a[1], tr * 1.1, th + 0.3, th * 0.7, 10, roof));
    }
    const double kt = cfg.kt[1] * 1.5, kr = cfg.kt[0] * 1.3;
    append(f, taperFaces(b.x, b.y, kr * 1.5, kr, 0, kt * 0.45, 12, white));
    append(f, taperFaces(b.x, b.y, kr, kr * 0.8, kt * 0.45, kt, 12, white, [](int i) { return i % 4 == 0 ? slitDeco(1) : Deco(); }));
    append(f, ringFaces(b.x, b.y, 0, kr * 1.25, kr * 1.25, kr * 1.25, kr * 1.25, kt * 0.62, kt * 0.62 + 0.35, 12, wallTop));
    append(f, coneFaces(b.x, b.y, kr * 1.05, kt, kt * 0.55, 12, roof));
    o.flag = { b.x, b.y, kt + kt * 0.55 };
    return o;
}
// anão: entalhado num monte de pedra; bastiões quadrados e baixos com frisos dourados, portão monumental
CastleOut dwarfCastle(const Art& b)
{
    const CastleCfg& cfg = kCastle[std::max(0, std::min(3, b.size))];
    const double R = cfg.R;
    const Loc L(b);
    const RGB stone = { 150, 138, 124 }, gold = { 214, 174, 86 }, top = { 170, 158, 142 }, btop = { 168, 156, 140 };
    CastleOut o;
    const P2 m = L.at(-R * 0.45, 0);
    o.faces = moundFaces(m[0], m[1], R * 1.15, cfg.keepH * 1.25, std::floor(b.v * 1e5) + 61, { 128, 118, 106 });
    Faces& f = o.faces;
    const Deco trim = [gold](QPainter& q, const PtFn& pt, double) { strokeLines(q, rgb(gold, 1, 0.85), 1); line(q, pt(0, 0.86), pt(1, 0.86)); line(q, pt(0, 0.14), pt(1, 0.14)); };
    // fachada: muro largo na frente do monte, com o portão
    const P2 fc = L.at(R * 0.55, 0);
    const Deco gate = [trim, gold](QPainter& q, const PtFn& pt, double kk) {
        trim(q, pt, kk);
        quad(q, pt, 0.4, 0.6, 0, 0.66, rgb({ 40, 30, 22 }, kk));
        q.setPen(pen(rgb(gold, kk, 0.9), 1.2)); q.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(pt(0.38, 0)); path.lineTo(pt(0.38, 0.7)); path.lineTo(pt(0.5, 0.8)); path.lineTo(pt(0.62, 0.7)); path.lineTo(pt(0.62, 0));
        q.drawPath(path);
    };
    append(f, boxFaces(fc[0], fc[1], b.yaw + M_PI / 2, R * 1.5, 1.4, 0, cfg.wallH * 1.1, stone, &top, [gate, trim](int side, double) { return side == 0 ? gate : trim; }));
    // bastiões nas pontas da fachada e dois mais atrás
    const Deco slit = slitDeco(1);
    for (const auto& q : { std::array<double, 3>{ R * 0.55, -R * 0.8, 1 }, std::array<double, 3>{ R * 0.55, R * 0.8, 1 }, std::array<double, 3>{ -R * 0.1, -R * 1.0, 0.85 }, std::array<double, 3>{ -R * 0.1, R * 1.0, 0.85 } }) {
        const P2 bc = L.at(q[0], q[1]);
        const double w = cfg.towerR * 2.2 * q[2], h = cfg.towerH * 0.85 * q[2];
        append(f, boxFaces(bc[0], bc[1], b.yaw, w, w, 0, h, stone, &btop, [trim, slit](int, double) -> Deco { return [trim, slit](QPainter& pp, const PtFn& pt, double kk) { trim(pp, pt, kk); slit(pp, pt, kk); }; }));
        const P2 c[4] = { L.at(q[0] - w / 2, q[1] - w / 2), L.at(q[0] + w / 2, q[1] - w / 2), L.at(q[0] + w / 2, q[1] + w / 2), L.at(q[0] - w / 2, q[1] + w / 2) };
        for (int k = 0; k < 4; k++) merlons(f, c[k][0], c[k][1], c[(k + 1) % 4][0], c[(k + 1) % 4][1], h, 0.6, stone);
    }
    // chaminés das forjas saindo do monte
    const RGB chimTop = { 60, 52, 46 };
    for (const auto& q : { P2{ -R * 0.7, -R * 0.3 }, P2{ -R * 0.5, R * 0.4 } }) {
        const P2 c = L.at(q[0], q[1]);
        append(f, boxFaces(c[0], c[1], b.yaw, 1.0, 1.0, cfg.keepH * 0.6, cfg.keepH * 1.35, { 110, 100, 92 }, &chimTop));
        o.smoke.push_back(L(q[0], q[1], cfg.keepH * 1.4));
    }
    o.flag = L(R * 0.55, 0, cfg.wallH * 1.1 + 0.3);
    return o;
}
// orc: paliçada de toras pontudas, torres de vigia de madeira, o salão comprido no meio
CastleOut orcCastle(const Art& b)
{
    const CastleCfg& cfg = kCastle[std::max(0, std::min(3, b.size))];
    const double R = cfg.R * 1.05;
    const Loc L(b);
    const RGB log = { 112, 82, 54 };
    Rng R2(seedOf(b, 71));
    CastleOut o;
    Faces& f = o.faces;
    const int n = static_cast<int>(jsRound(R * 4.2));
    for (int k = 0; k < n; k++) {
        const double a = double(k) / n * M_PI * 2;
        if (std::fabs(std::fmod(a + M_PI, M_PI * 2) - M_PI) < 0.16) continue;            // o vão do portão
        const double x = b.x + std::cos(a + b.yaw) * R, y = b.y + std::sin(a + b.yaw) * R, h = cfg.wallH * (0.85 + R2() * 0.25), sh = 0.9 + R2() * 0.2;
        append(f, taperFaces(x, y, 0.42, 0.38, 0, h, 5, sc(log, sh)));
        append(f, coneFaces(x, y, 0.38, h, 0.7, 5, { 88, 64, 42 }));
    }
    // torres de vigia: quatro postes, plataforma, telhadinho
    auto tw = [&](double lx, double ly, double h) {
        const P2 t = L.at(lx, ly);
        const double s = 1.1;
        for (const auto& d : { P2{ -s, -s }, P2{ s, -s }, P2{ s, s }, P2{ -s, s } }) { const P2 q = L.at(lx + d[0], ly + d[1]); append(f, beamFaces({ q[0], q[1], 0 }, { q[0], q[1], h }, 0.3, 0.3, log)); }
        const RGB platTop = { 104, 78, 52 };
        append(f, boxFaces(t[0], t[1], b.yaw, 2.8, 2.8, h, h + 0.9, { 126, 94, 62 }, &platTop));
        append(f, coneFaces(t[0], t[1], 2.2, h + 1.6, 1.6, 4, { 96, 72, 48 }));
    };
    tw(R, -R * 0.35, cfg.towerH * 1.1); tw(R, R * 0.35, cfg.towerH * 1.1);
    if (cfg.mid) { tw(-R, 0, cfg.towerH); tw(0, R, cfg.towerH * 0.95); tw(0, -R, cfg.towerH * 0.95); }
    // salão comprido com telhado de palha escura
    const double hw = cfg.keep * 0.75, hd = cfg.keep * 0.4, wh = cfg.keepH * 0.35, rh = cfg.keepH * 0.45;
    const V3 A = L(-hw, -hd, 0), B = L(hw, -hd, 0), C2 = L(hw, hd, 0), D = L(-hw, hd, 0);
    auto up = [](const V3& q, double z) { return V3{ q[0], q[1], z }; };
    const V3 R1 = L(-hw, 0, wh + rh), Rb = L(hw, 0, wh + rh), ctr = { b.x, b.y, wh / 2 };
    const RGB wall = { 136, 102, 68 }, thatch = { 92, 74, 50 };
    f.push_back(poly({ A, B, up(B, wh), up(A, wh) }, wall, ctr));
    f.push_back(poly({ C2, D, up(D, wh), up(C2, wh) }, wall, ctr));
    Face door = poly({ B, C2, up(C2, wh), Rb, up(B, wh) }, wall, ctr, [](QPainter& q, const PtFn& pt, double kk) { quad(q, pt, 0.4, 0.6, 0, 0.5, rgb({ 50, 34, 22 }, kk)); });
    door.q = { 0, 1, 2, 4 };
    f.push_back(std::move(door));
    Face back = poly({ D, A, up(A, wh), R1, up(D, wh) }, wall, ctr);
    back.q = { 0, 1, 2, 4 };
    f.push_back(std::move(back));
    f.push_back(poly({ up(A, wh - 0.05), up(B, wh - 0.05), Rb, R1 }, thatch, ctr, roofDeco, 0.01));
    f.push_back(poly({ up(C2, wh - 0.05), up(D, wh - 0.05), R1, Rb }, thatch, ctr, roofDeco, 0.01));
    // chifres cruzados na ponta do telhado
    append(f, beamFaces(L(hw, 0, wh + rh), L(hw + 0.6, -0.8, wh + rh + 1.6), 0.25, 0.25, { 228, 220, 196 }));
    append(f, beamFaces(L(hw, 0, wh + rh), L(hw + 0.6, 0.8, wh + rh + 1.6), 0.25, 0.25, { 228, 220, 196 }));
    o.flag = L(R, -R * 0.35, cfg.towerH * 1.1 + 3.2);
    o.hasFire = true; o.fire = L(-hw - 2.4, 0, 0.3);
    return o;
}
void drawStyledCastle(QPainter& p, const Art& b, const Cam& cam, const RGB& realmCol)
{
    const CastleOut o = b.style == QLatin1String("elven") ? elvenCastle(b, realmCol) : b.style == QLatin1String("dwarf") ? dwarfCastle(b) : orcCastle(b);
    drawFaces(p, o.faces, cam);
    for (const V3& s : o.smoke) for (int i = 0; i < 3; i++) glowAt(p, cam, { s[0] + i * 0.6, s[1], s[2] + i * 1.3 }, 1.4 + i * 0.6, { 190, 186, 180 }, 0.35 - i * 0.08);
    if (o.hasFire) glowAt(p, cam, o.fire, 1.6, { 255, 150, 60 }, 0.8);
    flag(p, cam, o.flag[0], o.flag[1], o.flag[2], realmCol);
}

// ════════════════════════ leva 3 ════════════════════════

// caixa no sistema do artefato, passando cada canto por T (pra inclinar junto com o casco)
Faces boxT(const std::function<V3(double, double, double)>& T, double x0, double x1, double y0, double y1, double z0, double z1, const RGB& col)
{
    const P2 P[4] = { { x0, y0 }, { x1, y0 }, { x1, y1 }, { x0, y1 } };
    const V3 C = T((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2);
    Faces f;
    for (int k = 0; k < 4; k++) { const P2& a = P[k]; const P2& b = P[(k + 1) % 4]; f.push_back(face({ T(a[0], a[1], z0), T(b[0], b[1], z0), T(b[0], b[1], z1), T(a[0], a[1], z1) }, col, C)); }
    f.push_back(face({ T(P[0][0], P[0][1], z1), T(P[1][0], P[1][1], z1), T(P[2][0], P[2][1], z1), T(P[3][0], P[3][1], z1) }, sc(col, 1.15), C));
    return f;
}
// casco de navio: perfil de convés, quilha mais estreita; T transforma (inclinar, afundar); raise ergue as pontas
Faces hullFaces(const std::function<V3(double, double, double)>& T, const std::vector<P2>& out, double DZ, const std::function<double(double)>& raise,
                const RGB& col, const RGB& deckCol, const std::function<Deco(int, int)>& deco)
{
    auto zr = [&](double x) { return raise ? raise(x) : 0.0; };
    std::vector<V3> deckL, deckR, keelL, keelR;
    for (const P2& q : out) {
        deckL.push_back(T(q[0], -q[1], DZ + zr(q[0]))); deckR.push_back(T(q[0], q[1], DZ + zr(q[0])));
        keelL.push_back(T(q[0] * 0.94, -q[1] * 0.42, zr(q[0]) * 0.8)); keelR.push_back(T(q[0] * 0.94, q[1] * 0.42, zr(q[0]) * 0.8));
    }
    const V3 C0 = T(0, 0, DZ * 0.4);
    Faces f;
    for (size_t i = 0; i + 1 < out.size(); i++) {
        f.push_back(face({ keelL[i], keelL[i + 1], deckL[i + 1], deckL[i] }, col, C0, deco ? deco(static_cast<int>(i), -1) : Deco()));
        f.push_back(face({ keelR[i], keelR[i + 1], deckR[i + 1], deckR[i] }, col, C0, deco ? deco(static_cast<int>(i), 1) : Deco()));
    }
    if (out[0][1] > 0.05) f.push_back(face({ keelL[0], keelR[0], deckR[0], deckL[0] }, col, C0));
    // o convés começa do meio: com as pontas de largura zero, começar por elas dá normal nula (e o convés some)
    std::vector<V3> ring = deckL;
    for (size_t i = out.size(); i-- > 0;) ring.push_back(deckR[i]);
    const size_t m = out.size() / 2;
    std::rotate(ring.begin(), ring.begin() + m, ring.end());
    f.push_back(face(ring, deckCol, T(0, 0, -5), Deco(), -0.01));
    return f;
}
Deco planks()
{
    return [](QPainter& q, const PtFn& pt, double) { strokeLines(q, QColor(40, 24, 14, 102), 0.6); line(q, pt(0, 0.5), pt(1, 0.5)); line(q, pt(0, 0.78), pt(1, 0.78)); };
}

// ── dragão: empoleirado numa rocha, asas meio abertas, pescoço erguido e a cauda enrolada na pedra ──
void drawDragon(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    static const RGB DRAGON[4][2] = { { { 164, 48, 40 }, { 222, 170, 110 } }, { { 64, 118, 72 }, { 196, 196, 120 } }, { { 60, 58, 70 }, { 150, 120, 110 } }, { { 192, 146, 58 }, { 236, 214, 150 } } };
    const int vi = static_cast<int>(std::floor(b.v * 4)) % 4;
    const RGB scale = DRAGON[vi][0], belly = DRAGON[vi][1], dark = sc(scale, 0.72), horn = { 226, 214, 186 };
    const double RZ = 2.6;
    const P2 rc = L.at(-0.4, 0);
    Faces f = above(blobFaces(rc[0], rc[1], 0, 4.4, RZ, 12, 5, { 120, 112, 102 }, seedOf(b, 3), 0.18, { { 120, 112, 102 }, { 108, 100, 92 }, { 132, 124, 112 } }), 0);
    // cauda: sai das ancas, desce pela rocha e dá a volta
    append(f, tube({ L(-2.4, 0, 3.9), L(-3.6, 0.5, 3.3), L(-4.6, 1.6, 2.6), L(-4.7, 3.0, 2.2), L(-3.8, 4.1, 1.6), L(-2.4, 4.5, 1.0), L(-1.2, 4.3, 0.5) }, 0.85, 0.2, scale, &dark));
    f.push_back(tri(L(-1.2, 4.3, 0.5), L(-0.2, 4.0, 0.9), L(-0.4, 4.8, 0.2), dark, L(-1, 4.3, 0.5)));
    // corpo: peito e barriga, as coxas de trás e as patas
    append(f, ellip(L, -1.0, 0, 3.9, 1.9, 1.15, 1.15, 10, 5, scale, seedOf(b, 5), 0.06));
    append(f, ellip(L, 0.9, 0, 4.4, 1.6, 1.15, 1.35, 10, 5, scale, seedOf(b, 6), 0.06));
    append(f, ellip(L, 1.3, 0, 4.0, 1.1, 0.8, 0.9, 8, 4, belly, seedOf(b, 7), 0.05));
    for (int sg : { -1, 1 }) {
        append(f, ellip(L, -1.4, sg * 1.0, 3.6, 0.95, 0.5, 0.9, 8, 4, scale, seedOf(b, 8 + sg), 0.06));
        append(f, tube({ L(-1.2, sg * 1.15, 3.2), L(-0.9, sg * 1.25, 2.6), L(-0.5, sg * 1.3, RZ - 0.05) }, 0.5, 0.42, dark));
        append(f, tube({ L(1.3, sg * 0.75, 4.0), L(1.7, sg * 0.95, 3.1), L(2.1, sg * 1.0, RZ - 0.1) }, 0.48, 0.38, scale));
        const P2 q = L.at(2.3, sg * 1.0);
        append(f, boxFaces(q[0], q[1], b.yaw, 0.7, 0.45, RZ - 0.2, RZ + 0.05, dark));
    }
    // pescoço em S, crista e a cabeça
    append(f, tube({ L(1.9, 0, 4.9), L(2.6, 0, 6.0), L(2.9, 0, 7.2), L(3.4, 0, 8.1), L(4.1, 0, 8.5) }, 0.95, 0.6, scale, &dark));
    for (int i = 0; i < 4; i++) { const V3 q = L(2.2 + i * 0.5, 0, 5.6 + i * 0.85); f.push_back(tri(q, { q[0], q[1], q[2] + 0.6 }, L(1.9 + i * 0.5, 0, 5.3 + i * 0.85), dark, q)); }
    append(f, ellip(L, 4.6, 0, 8.5, 0.85, 0.5, 0.45, 8, 4, scale, seedOf(b, 11), 0.05));
    append(f, beamFaces(L(4.9, 0, 8.45), L(5.9, 0, 8.25), 0.5, 0.42, scale));
    append(f, beamFaces(L(4.8, 0, 8.1), L(5.7, 0, 7.75), 0.4, 0.18, belly));
    for (int sg : { -1, 1 }) append(f, tube({ L(4.3, sg * 0.3, 8.8), L(3.8, sg * 0.5, 9.4), L(3.2, sg * 0.6, 9.7) }, 0.2, 0.08, horn));
    drawFaces(p, f, cam);
    // as asas: braço até o pulso, três dedos, a membrana entre eles (por cima do corpo atrás delas)
    Faces wings;
    const RGB tint = { 70, 40, 20 };
    const RGB mem = { scale[0] * 0.62 + tint[0] * 0.38, scale[1] * 0.62 + tint[1] * 0.38, scale[2] * 0.62 + tint[2] * 0.38 };
    for (int sg : { -1, 1 }) {
        const V3 sh = L(0.8, sg * 0.9, 5.3), el = L(0.2, sg * 2.6, 7.4), wr = L(-0.4, sg * 3.4, 8.8);
        const V3 t1 = L(-1.4, sg * 6.6, 8.2), t2 = L(-3.0, sg * 6.0, 6.4), t3 = L(-3.8, sg * 4.0, 4.9), hip = L(-2.2, sg * 0.9, 4.2), C = L(-1, sg * 3, 6);
        wings.push_back(tri(wr, t1, t2, mem, C));
        wings.push_back(tri(wr, t2, t3, sc(mem, 0.92), C));
        wings.push_back(flatPoly({ sh, el, wr, t3, hip }, sc(mem, 0.85), C));
        append(wings, tube({ sh, el, wr }, 0.34, 0.26, dark));
        append(wings, tube({ wr, t1 }, 0.2, 0.1, dark)); append(wings, tube({ wr, t2 }, 0.18, 0.08, dark)); append(wings, tube({ wr, t3 }, 0.16, 0.08, dark));
        append(wings, beamFaces(wr, L(-0.2, sg * 3.5, 9.5), 0.14, 0.14, horn));
    }
    drawFaces(p, wings, cam);
    for (int sg : { -1, 1 }) glowAt(p, cam, L(4.95, sg * 0.28, 8.65), 0.22, { 255, 210, 90 }, 1, 3);
}

// ── ossada de dragão: a carcaça deitada, costelas em arco, crânio com chifres e as asas abertas no chão ──
void drawBones(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const RGB bone = { 232, 224, 204 }, bone2 = { 214, 204, 182 };
    auto zs = [](double x) { const double t = (x + 0.4) / 3.0; return 0.35 + 2.3 * std::exp(-t * t); };
    std::vector<V3> spine;
    for (double x = 5; x >= -5; x -= 1.25) spine.push_back(L(x, 0, zs(x)));
    for (int i = 1; i <= 5; i++) spine.push_back(L(-5 - i * 1.3, std::sin(i * 0.9) * 0.9, 0.25));
    Faces f = tube(spine, 0.42, 0.18, bone, &bone2);
    for (double x = 3; x >= -3; x -= 1.2) {
        const double z = zs(x);
        for (int sg : { -1, 1 }) append(f, tube({ L(x, 0, z), L(x + 0.1, sg * 1.5, z + 0.05), L(x + 0.25, sg * 2.4, z * 0.55), L(x + 0.4, sg * 2.6, 0.05) }, 0.26, 0.16, bone, &bone2));
    }
    // pescoço tombado e o crânio
    append(f, tube({ L(5, 0, zs(5)), L(6.2, 0.4, 0.7), L(7.4, 0.9, 0.55) }, 0.36, 0.3, bone, &bone2));
    append(f, above(ellip(L, 8.4, 1.1, 0.5, 1.2, 0.75, 0.62, 8, 4, bone, seedOf(b, 3), 0.06), 0));
    append(f, beamFaces(L(8.9, 1.2, 0.35), L(10.2, 1.4, 0.25), 0.55, 0.4, bone2));
    for (int sg : { -1, 1 }) append(f, tube({ L(7.9, 1.1 + sg * 0.45, 0.9), L(7.0, 1.1 + sg * 1.0, 1.7), L(6.4, 1.1 + sg * 1.1, 2.3) }, 0.22, 0.08, bone));
    // as asas: ossos longos no chão, abertos
    for (int sg : { -1, 1 }) {
        const V3 sh = L(1.6, sg * 1.6, 1.2), wr = L(0.6, sg * 6.0, 0.2);
        append(f, tube({ sh, L(1.2, sg * 3.8, 0.5), wr }, 0.3, 0.22, bone, &bone2));
        for (const P2& t : { P2{ -1.8, 8.4 }, P2{ -3.6, 7.0 }, P2{ -4.4, 4.6 } }) append(f, beamFaces(wr, L(t[0], sg * t[1], 0.08), 0.14, 0.12, bone2));
    }
    drawFaces(p, f, cam);
}

// ── serpente marinha: o pescoço saindo do mar, três corcovas e a cauda, nadadeiras vermelhas ──
void drawSeaSerpent(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    static const RGB skins[3] = { { 56, 118, 112 }, { 70, 96, 130 }, { 86, 120, 64 } };
    const RGB skin = skins[static_cast<int>(std::floor(b.v * 3)) % 3], dark = sc(skin, 0.74), fin = { 198, 90, 66 };
    static const double humps[3][3] = { { 0.8, 1.6, 0.9 }, { -2.8, 1.35, 0.8 }, { -5.9, 1.05, 0.62 } };
    for (const auto& h : humps) for (int sg : { -1, 1 }) foam(p, cam, L.at(h[0] + sg * h[1], 0), 0.9, 0.7, b.yaw, 0.7);
    foam(p, cam, L.at(3.0, 0), 1.1, 0.85, b.yaw, 0.8);
    foam(p, cam, L.at(-8.0, 0.3), 0.7, 0.5, b.yaw, 0.6);
    Faces f;
    for (const auto& h : humps) {
        const double xc = h[0], r = h[1], w = h[2];
        std::vector<V3> pts;
        for (int i = 0; i <= 8; i++) { const double a = i / 8.0 * M_PI; pts.push_back(L(xc + std::cos(a) * r, std::sin(a * 2) * 0.12, -0.35 + std::sin(a) * r * 1.15)); }
        append(f, tube(pts, w, w * 0.9, skin, &dark));
        const double top = -0.35 + r * 1.15 + w * 0.4;
        f.push_back(tri(L(xc + 0.55, 0, top - 0.15), L(xc - 0.5, 0, top - 0.15), L(xc - 0.15, 0, top + 0.85), fin, L(xc, 1, top)));
    }
    // o rabo: nadadeira em leque saindo d'água
    f.push_back(tri(L(-8.0, 0.3, 0), L(-9.2, 1.1, 1.5), L(-8.9, -0.4, 1.2), fin, L(-8, 1, 0)));
    // pescoço, cabeça, mandíbula aberta, babados
    append(f, tube({ L(3.0, 0, -0.35), L(3.3, 0, 1.0), L(3.5, 0, 2.3), L(3.8, 0, 3.4), L(4.4, 0, 4.0) }, 1.0, 0.72, skin, &dark));
    for (int i = 0; i < 3; i++) f.push_back(tri(L(3.1 + i * 0.3, 0, 1.4 + i * 1.0), L(2.9 + i * 0.3, 0, 1.0 + i * 1.0), L(2.6 + i * 0.3, 0, 1.6 + i * 1.0), fin, L(4, 1, 2)));
    append(f, ellip(L, 5.0, 0, 4.05, 0.95, 0.55, 0.48, 8, 4, skin, seedOf(b, 4), 0.05));
    append(f, beamFaces(L(4.6, 0, 3.65), L(5.9, 0, 3.15), 0.5, 0.2, { 214, 196, 150 }));
    for (int sg : { -1, 1 }) f.push_back(tri(L(4.5, sg * 0.45, 4.25), L(3.7, sg * 1.35, 4.9), L(3.9, sg * 0.9, 3.7), fin, L(4.5, 0, 4)));
    drawFaces(p, f, cam);
    for (int sg : { -1, 1 }) glowAt(p, cam, L(5.35, sg * 0.32, 4.3), 0.2, { 255, 220, 110 }, 1, 3);
}

// ── kraken: tentáculos enrolados saindo do mar em volta da cabeça, os olhos amarelos rente à água ──
void drawKraken(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    Rng R(seedOf(b, 1));
    static const RGB skins[3] = { { 150, 64, 96 }, { 176, 86, 64 }, { 96, 74, 120 } };
    const RGB skin = skins[static_cast<int>(std::floor(b.v * 3)) % 3];
    const RGB alt = { std::min(255.0, skin[0] * 1.14), std::min(255.0, skin[1] * 1.14), std::min(255.0, skin[2] * 1.14) };
    struct Arm { std::vector<V3> pts; P2 base; double w; };
    std::vector<Arm> arms;
    const int n = 7;
    for (int k = 0; k < n; k++) {
        const double a = double(k) / n * M_PI * 2 + R() * 0.4, r0 = 2.6 + R() * 0.8, Lt = 6.5 + R() * 2.5, curl = 4.0 + R() * 1.2, ox = std::cos(a), oy = std::sin(a);
        double u = r0, z = -0.4;
        const double h = 1.45 - R() * 0.15;
        const int N = 14;
        const double ds = Lt / N;
        Arm arm;
        arm.pts.push_back(L(ox * u, oy * u, z));
        for (int i = 1; i <= N; i++) { const double t = double(i) / N, hh = h - t * t * curl; u += std::cos(hh) * ds; z += std::sin(hh) * ds; arm.pts.push_back(L(ox * u, oy * u, std::max(-0.4, z))); }
        arm.base = L.at(ox * r0, oy * r0);
        arm.w = 1.0 + R() * 0.2;
        arms.push_back(std::move(arm));
    }
    for (const Arm& a : arms) foam(p, cam, a.base, 1.1, 1.1, 0, 0.75);
    foam(p, cam, { b.x, b.y }, 2.4, 2.4, 0, 0.8);
    Faces f = above(ellip(L, 0, 0, -0.3, 2.1, 2.0, 1.9, 10, 5, skin, seedOf(b, 2), 0.08), 0);
    for (const Arm& a : arms) append(f, tube(a.pts, a.w, 0.12, skin, &alt));
    drawFaces(p, f, cam);
    for (int sg : { -1, 1 }) glowAt(p, cam, L(1.7, sg * 0.8, 0.9), 0.32, { 255, 214, 80 }, 1, 3);
}

// ── drakkar: casco longo de pontas erguidas, cabeça de dragão na proa, escudos nas bordas, vela listrada ──
void drawLongship(QPainter& p, const Art& b, const Cam& cam, const RGB& realmCol)
{
    const Loc L(b);
    const double DZ = 1.3;
    const RGB wood = { 116, 80, 50 }, deckCol = { 150, 112, 74 }, mast = { 84, 60, 40 };
    const std::vector<P2> out = { { -6.0, 0 }, { -5.0, 0.8 }, { -3.0, 1.35 }, { 0, 1.5 }, { 3.0, 1.3 }, { 5.0, 0.75 }, { 6.0, 0 } };
    auto raise = [](double x) { return std::pow(std::max(0.0, std::fabs(x) - 3.4), 1.5) * 0.42; };
    foam(p, cam, { b.x, b.y }, 6.6, 1.9, b.yaw, 0.55);
    Faces f = hullFaces(L, out, DZ, raise, wood, deckCol, [](int, int) { return planks(); });
    // remos
    for (int i = 0; i < 5; i++) { const double x = -2.6 + i * 1.3; for (int sg : { -1, 1 }) append(f, beamFaces(L(x, sg * 1.35, DZ), L(x - 0.7, sg * 3.6, -0.1), 0.1, 0.1, { 92, 66, 44 })); }
    drawFaces(p, f, cam);
    // escudos na borda (dos dois lados), cores alternadas
    static const RGB shields[4] = { { 196, 52, 40 }, { 232, 206, 120 }, { 60, 92, 140 }, { 226, 222, 210 } };
    Faces sh;
    for (int i = 0; i < 6; i++) {
        const double x = -3.0 + i * 1.2, hw = 1.5 - std::fabs(x) * 0.05;
        for (int sg : { -1, 1 }) {
            std::vector<V3> ring;
            for (int k = 0; k < 8; k++) { const double t = k / 8.0 * M_PI * 2; ring.push_back(L(x + std::cos(t) * 0.45, sg * (hw + 0.06), DZ - 0.05 + std::sin(t) * 0.45)); }
            Face s = poly(ring, shields[(i + (sg > 0 ? 1 : 0)) % 4], L(x, 0, DZ), [](QPainter& q, const PtFn& pt, double) {
                q.setPen(Qt::NoPen); q.setBrush(QColor(70, 60, 50, 230)); q.drawEllipse(pt(0.5, 0.5), 1.2, 1.2);
            });
            s.q = { 0, 2, 4, 6 };
            sh.push_back(std::move(s));
        }
    }
    // proa de dragão e popa enrolada, mastro e verga
    append(sh, tube({ L(5.9, 0, DZ + raise(5.9)), L(6.3, 0, DZ + 2.6), L(6.5, 0, DZ + 3.5), L(7.0, 0, DZ + 3.9) }, 0.42, 0.34, wood));
    append(sh, ellip(L, 7.3, 0, DZ + 3.9, 0.55, 0.3, 0.3, 6, 3, wood, 5, 0.05));
    for (int sg : { -1, 1 }) append(sh, beamFaces(L(7.0, sg * 0.15, DZ + 4.1), L(6.6, sg * 0.3, DZ + 4.7), 0.1, 0.1, { 222, 196, 120 }));
    append(sh, tube({ L(-5.9, 0, DZ + raise(-5.9)), L(-6.3, 0, DZ + 2.6), L(-6.2, 0, DZ + 3.4), L(-5.7, 0, DZ + 3.6), L(-5.5, 0, DZ + 3.1) }, 0.4, 0.22, wood));
    append(sh, beamFaces(L(0.3, 0, DZ), L(0.3, 0, DZ + 7.2), 0.26, 0.26, mast));
    append(sh, beamFaces(L(0.4, -2.8, DZ + 6.6), L(0.4, 2.8, DZ + 6.6), 0.16, 0.16, mast));
    drawFaces(p, sh, cam);
    const RGB red = realmCol;
    Face sail = flatPoly({ L(0.55, -2.7, DZ + 2.2), L(0.95, 0, DZ + 2.0), L(0.55, 2.7, DZ + 2.2), L(0.5, 2.7, DZ + 6.5), L(0.75, 0, DZ + 6.5), L(0.5, -2.7, DZ + 6.5) },
                         { 238, 228, 206 }, L(-2, 0, DZ + 4), [red](QPainter& q, const PtFn& pt, double) { for (double s0 : { 0.0, 0.34, 0.67 }) quad(q, pt, s0, s0 + 0.17, 0, 1, rgb(red, 1, 0.9)); });
    sail.q = { 0, 2, 3, 5 };
    drawFaces(p, Faces{ std::move(sail) }, detailed(cam));
}

// ── naufrágio: o casco adernado e afundando de proa, mastro quebrado, o outro boiando com a vela rasgada ──
void drawWreck(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const double roll = 0.55, cr = std::cos(roll), sr = std::sin(roll);
    const auto T = [&](double lx, double ly, double lz) { return L(lx, ly * cr - lz * sr, ly * sr + lz * cr - 0.9 - lx * 0.14); };
    const std::vector<P2> out = { { -6, 1.7 }, { -3, 1.85 }, { 1, 1.8 }, { 3.8, 1.4 }, { 5.6, 0.7 }, { 6.6, 0 } };
    foam(p, cam, { b.x, b.y }, 6.4, 2.6, b.yaw, 0.6);
    const auto hole = [](int i, int sg) -> Deco {
        const Deco pl = planks();
        return [pl, i, sg](QPainter& q, const PtFn& pt, double kk) {
            pl(q, pt, kk);
            if (sg < 0 && (i == 1 || i == 3)) q.fillPath(pathOf({ pt(0.3, 0.3), pt(0.55, 0.2), pt(0.7, 0.55), pt(0.45, 0.75), pt(0.28, 0.55) }), rgb({ 26, 20, 16 }, kk));
        };
    };
    Faces f = above(hullFaces(T, out, 1.4, {}, { 82, 62, 44 }, { 120, 96, 70 }, hole), 0);
    append(f, above(boxT(T, -6.0, -3.2, -1.65, 1.65, 1.4, 2.9, { 96, 74, 52 }), 0));
    // mastro quebrado no convés e o outro caído n'água
    const RGB mast = { 70, 52, 36 };
    append(f, beamFaces(T(0.8, 0, 1.4), T(0.8, 0, 5.4), 0.3, 0.3, mast));
    f.push_back(tri(T(0.8, 0, 5.4), T(0.95, 0.1, 6.0), T(0.7, -0.1, 5.5), mast, T(0.8, 0, 5)));
    append(f, beamFaces(L(-2.0, 3.2, 0.12), L(-7.6, 5.4, 0.08), 0.3, 0.26, mast));
    f.push_back(flatPoly({ L(-3.0, 3.6, 0.06), L(-6.4, 4.9, 0.06), L(-6.0, 6.6, 0.04), L(-4.4, 6.4, 0.05), L(-3.2, 5.2, 0.05) }, { 214, 204, 182 }, L(-4, 5, -1)));
    // destroços boiando
    Rng R(seedOf(b, 9));
    for (int i = 0; i < 4; i++) { const double lx = 2 + R() * 5, ly = -2.5 - R() * 3, yaw = R() * 3, w = 0.7 + R() * 0.5; const P2 q = L.at(lx, ly); append(f, boxFaces(q[0], q[1], yaw, w, 0.5, 0, 0.25, { 110, 84, 58 })); }
    drawFaces(p, f, cam);
}

// ── pirâmide: tronco de pedra com degraus, o topo de ouro, a entrada na frente; duas menores atrás ──
Faces pyramidFaces(const Loc& L, double lx, double ly, double B, double H, const RGB& stone, const RGB& gold, bool door)
{
    const double t = 0.86;
    const P2 pp[4] = { { B, -B }, { B, B }, { -B, B }, { -B, -B } };
    const V3 C = L(lx, ly, H * 0.3);
    Faces f;
    for (int k = 0; k < 4; k++) {
        const P2& a = pp[k]; const P2& c = pp[(k + 1) % 4];
        const bool hasDoor = door && k == 0;
        f.push_back(face({ L(lx + a[0], ly + a[1], 0), L(lx + c[0], ly + c[1], 0), L(lx + c[0] * (1 - t), ly + c[1] * (1 - t), H * t), L(lx + a[0] * (1 - t), ly + a[1] * (1 - t), H * t) }, stone, C,
                         [hasDoor](QPainter& q, const PtFn& pt, double kk) {
                             strokeLines(q, QColor(140, 110, 70, 102), 0.6);
                             for (int i = 1; i < 9; i++) line(q, pt(0, i / 9.0), pt(1, i / 9.0));
                             if (hasDoor) quad(q, pt, 0.45, 0.55, 0, 0.14, rgb({ 40, 30, 20 }, kk));
                         }));
        f.push_back(face({ L(lx + a[0] * (1 - t), ly + a[1] * (1 - t), H * t), L(lx + c[0] * (1 - t), ly + c[1] * (1 - t), H * t), L(lx, ly, H) }, gold, C));
    }
    return f;
}
void drawPyramid(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const RGB stone = { 226, 200, 144 }, small = { 216, 190, 136 };
    Faces f = pyramidFaces(L, 0, 0, 5, 7.6, stone, { 238, 196, 86 }, true);
    append(f, pyramidFaces(L, -6.6, 5.6, 2.4, 3.7, small, small, false));
    append(f, pyramidFaces(L, -7.2, -5.0, 1.9, 2.9, small, small, false));
    // o caminho de pedra até a entrada
    f.push_back(flatPoly({ L(5, -0.7, 0.04), L(9, -0.7, 0.04), L(9, 0.7, 0.04), L(5, 0.7, 0.04) }, { 206, 184, 140 }, L(7, 0, -1)));
    drawFaces(p, f, cam);
}

// ── zigurate: templo em degraus, escadaria na frente, santuário no topo com dois braseiros ──
void drawZiggurat(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    static const RGB cols[5] = { { 184, 170, 136 }, { 172, 162, 130 }, { 184, 170, 136 }, { 172, 162, 130 }, { 184, 170, 136 } };
    const RGB moss = { 138, 152, 104 };
    const Deco band = [](QPainter& q, const PtFn& pt, double) { strokeLines(q, QColor(100, 88, 60, 115), 0.6); line(q, pt(0, 0.7), pt(1, 0.7)); for (int i = 1; i < 8; i++) line(q, pt(i / 8.0, 0.7), pt(i / 8.0, 1)); };
    Faces f;
    for (int k = 0; k < 5; k++) { const double s = 5.2 - k * 0.95; append(f, boxFaces(b.x, b.y, b.yaw, 2 * s, 2 * s, k * 1.4, (k + 1) * 1.4, cols[k], k % 2 ? &moss : &cols[k], [band](int, double) { return band; })); }
    // escadaria: rampa do chão ao topo, com os degraus riscados
    const double X0 = 6.8, X1 = 1.4, ZT = 7.0, hw = 0.9;
    f.push_back(poly({ L(X0, -hw, 0), L(X0, hw, 0), L(X1, hw, ZT), L(X1, -hw, ZT) }, { 198, 186, 152 }, L(0, 0, 0), [](QPainter& q, const PtFn& pt, double) {
        strokeLines(q, QColor(90, 76, 50, 153), 0.6); for (int i = 1; i < 14; i++) line(q, pt(0, i / 14.0), pt(1, i / 14.0));
    }, 0.06));
    for (int sg : { -1, 1 }) f.push_back(poly({ L(X0, sg * hw, 0), L(X1, sg * hw, ZT), L(X1, sg * hw, 0) }, { 176, 164, 132 }, L(4, 0, 2), {}, 0.06));
    // santuário
    append(f, boxFaces(b.x, b.y, b.yaw, 2.2, 2.4, ZT, ZT + 1.9, { 196, 176, 140 }, nullptr, [](int side, double) -> Deco {
        if (side != 1) return Deco();
        return [](QPainter& q, const PtFn& pt, double kk) { quad(q, pt, 0.36, 0.64, 0, 0.72, rgb({ 40, 28, 20 }, kk)); };
    }));
    const RGB roof = { 150, 72, 52 }, roofTop = { 168, 86, 60 };
    append(f, boxFaces(b.x, b.y, b.yaw, 2.9, 3.1, ZT + 1.9, ZT + 2.3, roof, &roofTop));
    append(f, boxFaces(b.x, b.y, b.yaw, 1.6, 1.8, ZT + 2.3, ZT + 2.8, roof, &roofTop));
    for (int sg : { -1, 1 }) { const P2 q = L.at(1.7, sg * 1.35); append(f, taperFaces(q[0], q[1], 0.32, 0.42, ZT, ZT + 0.6, 6, { 90, 80, 70 })); }
    drawFaces(p, f, cam);
    for (int sg : { -1, 1 }) { glowAt(p, cam, L(1.7, sg * 1.35, ZT + 0.9), 0.8, { 255, 150, 60 }, 0.95, 3); glowAt(p, cam, L(1.7, sg * 1.35, ZT + 0.75), 0.35, { 255, 230, 140 }, 1, 3); }
}

// ── pagode: cinco andares de paredes vermelhas, beirais curvos de pontas erguidas, agulha dourada ──
void drawPagoda(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    static const RGB roofs[3] = { { 58, 74, 72 }, { 64, 104, 86 }, { 70, 62, 60 } };
    const RGB wall = { 186, 64, 48 }, roof = roofs[static_cast<int>(std::floor(b.v * 3)) % 3], gold = { 226, 186, 90 };
    Faces f = boxFaces(b.x, b.y, b.yaw, 6, 6, 0, 0.6, { 170, 160, 146 });
    const Deco windows = [](QPainter& q, const PtFn& pt, double kk) { for (int j = 0; j < 3; j++) { const double s0 = 0.18 + j * 0.25; quad(q, pt, s0, s0 + 0.14, 0.3, 0.8, rgb({ 60, 30, 24 }, kk)); } };
    double z = 0.6;
    for (int i = 0; i < 5; i++) {
        const double w = 3.6 - i * 0.48, h = i == 0 ? 2.0 : 1.6, E = w / 2 + 0.8, I = w / 2 * 0.7, up = 0.42, rh = 0.7;
        append(f, boxFaces(b.x, b.y, b.yaw, w, w, z, z + h, wall, nullptr, [windows](int, double) { return windows; }));
        const double zt = z + h;
        const V3 C = L(0, 0, zt - 2);
        static const P2 cn[4] = { { 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 } };
        for (int k = 0; k < 4; k++) {
            const P2& a = cn[k]; const P2& c = cn[(k + 1) % 4];
            const double mx = (a[0] + c[0]) / 2, my = (a[1] + c[1]) / 2;
            const V3 A = L(a[0] * E, a[1] * E, zt + up - 0.15), M = L(mx * E * 0.98, my * E * 0.98, zt - 0.15), B = L(c[0] * E, c[1] * E, zt + up - 0.15);
            const V3 IA = L(a[0] * I, a[1] * I, zt + rh), IM = L(mx * I, my * I, zt + rh), IB = L(c[0] * I, c[1] * I, zt + rh);
            f.push_back(poly({ A, M, IM, IA }, roof, C, roofDeco, 0.01));
            f.push_back(poly({ M, B, IB, IM }, roof, C, roofDeco, 0.01));
        }
        z = zt + rh - 0.05;
    }
    append(f, taperFaces(b.x, b.y, 0.42, 0.42, z, z + 0.6, 8, roof));
    for (int i = 0; i < 4; i++) append(f, taperFaces(b.x, b.y, 0.3 - i * 0.04, 0.3 - i * 0.04, z + 0.8 + i * 0.6, z + 1.0 + i * 0.6, 8, gold));
    append(f, beamFaces({ b.x, b.y, z + 0.5 }, { b.x, b.y, z + 3.6 }, 0.12, 0.12, gold));
    drawFaces(p, f, cam);
}

// ── obelisco: agulha de pedra num pedestal, inscrições, a ponta de ouro ──
void drawObelisk(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const RGB stone = { 196, 178, 150 }, gold = { 236, 196, 90 };
    Faces f = boxFaces(b.x, b.y, b.yaw, 2.8, 2.8, 0, 0.7, { 176, 166, 150 });
    append(f, boxFaces(b.x, b.y, b.yaw, 2.0, 2.0, 0.7, 1.2, { 186, 176, 158 }));
    static const P2 pp[4] = { { 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 } };
    const double r0 = 0.72, r1 = 0.48, z0 = 1.2, z1 = 10.2;
    const Deco glyphs = [](QPainter& q, const PtFn& pt, double) { for (int i = 0; i < 9; i++) { const double t = 0.12 + i * 0.085; quad(q, pt, 0.38, 0.62, t, t + 0.035, QColor(110, 90, 64, 140)); } };
    for (int k = 0; k < 4; k++) {
        const P2& a = pp[k]; const P2& c = pp[(k + 1) % 4];
        f.push_back(face({ L(a[0] * r0, a[1] * r0, z0), L(c[0] * r0, c[1] * r0, z0), L(c[0] * r1, c[1] * r1, z1), L(a[0] * r1, a[1] * r1, z1) }, stone, L(0, 0, 5), glyphs));
        f.push_back(face({ L(a[0] * r1, a[1] * r1, z1), L(c[0] * r1, c[1] * r1, z1), L(0, 0, z1 + 1.1) }, gold, L(0, 0, 5)));
    }
    drawFaces(p, f, cam);
}

// ── cemitério: cerca baixa, lápides e cruzes, o mausoléu de colunas e uma árvore seca ──
void drawGraveyard(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    Rng R(seedOf(b, 1));
    const RGB stone = { 168, 166, 160 }, iron = { 54, 52, 56 }, stoneTop = { 190, 188, 182 };
    Faces f;
    // cerca: postes e duas travessas; o portão na frente
    const double HX = 5.4, HY = 4.2;
    const P2 ring[4] = { { HX, -HY }, { HX, HY }, { -HX, HY }, { -HX, -HY } };
    for (int k = 0; k < 4; k++) {
        const P2& a = ring[k]; const P2& c = ring[(k + 1) % 4];
        const int n = static_cast<int>(jsRound(std::hypot(c[0] - a[0], c[1] - a[1]) / 1.1));
        for (int i = 0; i <= n; i++) {
            const double x = a[0] + (c[0] - a[0]) * i / n, y = a[1] + (c[1] - a[1]) * i / n;
            if (k == 0 && std::fabs(y) < 1.0) continue;
            append(f, beamFaces(L(x, y, 0), L(x, y, 1.15), 0.12, 0.12, iron));
        }
        for (double z : { 0.45, 0.95 }) {
            if (k == 0) { append(f, beamFaces(L(a[0], a[1], z), L(a[0], -1.0, z), 0.07, 0.07, iron)); append(f, beamFaces(L(a[0], 1.0, z), L(c[0], c[1], z), 0.07, 0.07, iron)); }
            else append(f, beamFaces(L(a[0], a[1], z), L(c[0], c[1], z), 0.07, 0.07, iron));
        }
    }
    for (int sg : { -1, 1 }) { const P2 q = L.at(HX, sg * 1.1); append(f, boxFaces(q[0], q[1], b.yaw, 0.5, 0.5, 0, 1.6, stone)); }
    // lápides e cruzes
    for (int r = 0; r < 3; r++) for (int c = 0; c < 4; c++) {
        if (R() < 0.15) continue;
        const double x = 3.6 - r * 1.6, y = -3.2 + c * 1.25, yaw = b.yaw + (R() - 0.5) * 0.3, h = 0.8 + R() * 0.4;
        const P2 g = L.at(x, y);
        if (R() < 0.3) {
            append(f, beamFaces({ g[0], g[1], 0 }, { g[0], g[1], h + 0.5 }, 0.14, 0.14, stone));
            const double c0 = std::cos(yaw + M_PI / 2) * 0.35, s0 = std::sin(yaw + M_PI / 2) * 0.35;
            append(f, beamFaces({ g[0] - c0, g[1] - s0, h + 0.15 }, { g[0] + c0, g[1] + s0, h + 0.15 }, 0.13, 0.13, stone));
        } else append(f, boxFaces(g[0], g[1], yaw, 0.22, 0.75, 0, h, sc(stone, 0.88 + R() * 0.16), &stoneTop));
        f.push_back(flatPoly({ L(x + 0.2, y - 0.3, 0.03), L(x + 1.0, y - 0.3, 0.03), L(x + 1.0, y + 0.3, 0.03), L(x + 0.2, y + 0.3, 0.03) }, { 96, 80, 62 }, L(x, y, -1)));
    }
    // mausoléu: caixa, frontão, duas colunas na frente e a porta
    const double mx = -2.8, my = 1.6, mw = 3.0, md = 2.6, mh = 2.3;
    const P2 mc = L.at(mx, my);
    append(f, boxFaces(mc[0], mc[1], b.yaw, mw, md, 0, mh, { 180, 176, 168 }, nullptr, [](int side, double) -> Deco {
        if (side != 1) return Deco();
        return [](QPainter& q, const PtFn& pt, double kk) { quad(q, pt, 0.36, 0.64, 0, 0.7, rgb({ 34, 30, 32 }, kk)); };
    }));
    const V3 g0 = L(mx + mw / 2 + 0.6, my - md / 2 - 0.2, mh), g1 = L(mx + mw / 2 + 0.6, my + md / 2 + 0.2, mh), g2 = L(mx + mw / 2 + 0.6, my, mh + 1.2);
    const V3 b0 = L(mx - mw / 2, my - md / 2 - 0.2, mh), b1 = L(mx - mw / 2, my + md / 2 + 0.2, mh), b2 = L(mx - mw / 2, my, mh + 1.2);
    const V3 C = L(mx, my, mh / 2);
    const RGB pedi = { 192, 188, 180 }, roofC = { 128, 120, 116 };
    f.push_back(face({ g0, g1, g2 }, pedi, C)); f.push_back(face({ b0, b1, b2 }, pedi, C));
    f.push_back(face({ g0, b0, b2, g2 }, roofC, C, roofDeco)); f.push_back(face({ g1, b1, b2, g2 }, roofC, C, roofDeco));
    for (int sg : { -1, 1 }) { const P2 q = L.at(mx + mw / 2 + 0.4, my + sg * 1.0); append(f, taperFaces(q[0], q[1], 0.2, 0.18, 0, mh, 6, { 206, 202, 194 })); }
    append(f, beamFaces(g0, g1, 0.3, 0.2, pedi));
    // árvore seca, retorcida
    const RGB tr = { 62, 54, 50 };
    const P2 t = L.at(-3.6, -2.6);
    append(f, tube({ { t[0], t[1], 0 }, { t[0] + 0.2, t[1], 1.6 }, { t[0] - 0.1, t[1] + 0.2, 3.0 }, { t[0] + 0.1, t[1], 4.0 } }, 0.42, 0.22, tr));
    for (const auto& br : { std::array<double, 4>{ 1.6, 0.4, 1.0, 2.4 }, std::array<double, 4>{ -1.4, -0.6, 1.3, 2.8 }, std::array<double, 4>{ 0.6, -1.4, 1.2, 3.4 }, std::array<double, 4>{ -0.5, 1.2, 0.9, 3.6 } })
        append(f, tube({ { t[0], t[1], br[3] }, { t[0] + br[0] * 0.6, t[1] + br[1] * 0.6, br[3] + br[2] * 0.7 }, { t[0] + br[0], t[1] + br[1], br[3] + br[2] } }, 0.18, 0.06, tr));
    drawFaces(p, f, cam);
}

// ── portal: anel de pedra em pé sobre degraus, o meio girando em luz arcana ──
void drawPortal(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const RGB stone = { 138, 132, 150 };
    const double ZC = 3.9, RR = 3.0, IR = 2.55;
    const int n = 18;
    Faces f = boxFaces(b.x, b.y, b.yaw, 2.6, 7.8, 0, 0.35, { 150, 146, 156 });
    append(f, boxFaces(b.x, b.y, b.yaw, 1.9, 6.6, 0.35, 0.75, { 160, 156, 166 }));
    for (int sg : { -1, 1 }) { const P2 q = L.at(1.6, sg * 4.6); append(f, boxFaces(q[0], q[1], b.yaw, 0.8, 0.6, 0, 2.2, sc(stone, 0.9))); }
    drawFaces(p, f, cam);
    // o redemoinho (desenhado antes do anel, que o emoldura)
    std::vector<QPointF> pts;
    for (int i = 0; i < 28; i++) { const double t = i / 28.0 * M_PI * 2; const V3 q = L(0, std::cos(t) * IR, ZC + std::sin(t) * IR); pts.push_back(cam.proj(q[0], q[1], q[2])); }
    const V3 c3 = L(0, 0, ZC);
    const QPointF c = cam.proj(c3[0], c3[1], c3[2]);
    double rad = 1;
    for (const QPointF& q : pts) rad = std::max(rad, std::hypot(q.x() - c.x(), q.y() - c.y()));
    QRadialGradient gr(c, rad);
    gr.setColorAt(0, QColor(236, 222, 255, 250)); gr.setColorAt(0.45, QColor(160, 120, 255, 235)); gr.setColorAt(1, QColor(70, 40, 170, 230));
    const QPainterPath disc = pathOf(pts);
    p.fillPath(disc, gr);
    p.save();
    p.setClipPath(disc, Qt::IntersectClip);
    p.setPen(pen(QColor(255, 255, 255, 115), std::max(0.8, rad * 0.05), Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    for (int arm = 0; arm < 3; arm++) {
        QPainterPath sp;
        for (int i = 0; i <= 30; i++) {
            const double t = i / 30.0, a = arm * M_PI * 2 / 3 + t * 4.2 + b.v * 6, r = t * IR;
            const V3 q = L(0, std::cos(a) * r, ZC + std::sin(a) * r);
            const QPointF X = cam.proj(q[0], q[1], q[2]);
            if (i) sp.lineTo(X); else sp.moveTo(X);
        }
        p.drawPath(sp);
    }
    p.restore();
    // o anel, em blocos, com uma pedra-chave clara no alto
    Faces ringF;
    for (int k = 0; k < n; k++) {
        const double a0 = double(k) / n * M_PI * 2, a1 = double(k + 1) / n * M_PI * 2;
        const V3 p0 = L(0, std::cos(a0) * RR, ZC + std::sin(a0) * RR), p1 = L(0, std::cos(a1) * RR, ZC + std::sin(a1) * RR);
        if (std::min(p0[2], p1[2]) < 0.6) continue;
        const bool key = std::fabs((a0 + a1) / 2 - M_PI / 2) < 0.2;
        append(ringF, beamFaces(p0, p1, 0.85, 0.85, key ? RGB{ 206, 196, 230 } : k % 2 ? stone : sc(stone, 0.92)));
    }
    drawFaces(p, ringF, cam);
    glowAt(p, cam, c3, 4.2, { 186, 150, 255 }, 0.3, 3);
}

// ── cabeças de pedra: uma fileira de bustos colossais numa plataforma, olhando pro mesmo lado ──
void drawHeads(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    Rng R(seedOf(b, 2));
    const RGB stone = { 120, 112, 104 }, platTop = { 150, 144, 132 };
    Faces f = boxFaces(b.x, b.y, b.yaw, 2.6, 10.4, 0, 0.8, { 140, 134, 124 }, &platTop);
    const Deco faceDeco = [](QPainter& q, const PtFn& pt, double kk) {
        const QColor ink = rgb({ 44, 40, 38 }, kk, 0.85);
        quad(q, pt, 0.12, 0.88, 0.68, 0.74, ink);                     // a testa pesada
        quad(q, pt, 0.2, 0.42, 0.56, 0.67, ink); quad(q, pt, 0.58, 0.8, 0.56, 0.67, ink);
        quad(q, pt, 0.3, 0.7, 0.16, 0.2, ink);                        // a boca
    };
    for (int i = 0; i < 4; i++) {
        const double y = -3.75 + i * 2.5, s = 0.92 + R() * 0.18, shade = 0.92 + R() * 0.14;
        const P2 h = L.at(0.1, y);
        const RGB col = sc(stone, shade);
        append(f, boxFaces(h[0], h[1], b.yaw, 1.15 * s, 1.5 * s, 0.8, 0.8 + 1.5 * s, col));
        append(f, boxFaces(h[0], h[1], b.yaw, 1.25 * s, 1.35 * s, 0.8 + 1.5 * s, 0.8 + 4.2 * s, col, nullptr, [faceDeco](int side, double) { return side == 1 ? faceDeco : Deco(); }));
        append(f, beamFaces(L(0.1 + 0.64 * s, y, 0.8 + 3.3 * s), L(0.1 + 1.0 * s, y, 0.8 + 2.4 * s), 0.38 * s, 0.32 * s, col));
        for (int sg : { -1, 1 }) { const P2 e = L.at(0.1, y + sg * 0.72 * s); append(f, boxFaces(e[0], e[1], b.yaw, 0.35 * s, 0.14 * s, 0.8 + 2.5 * s, 0.8 + 3.8 * s, col)); }
        if (i % 2 == 0) append(f, taperFaces(h[0], h[1], 0.62 * s, 0.56 * s, 0.8 + 4.2 * s, 0.8 + 4.9 * s, 8, { 150, 74, 52 }));
    }
    drawFaces(p, f, cam);
}

// ── ilha flutuante: rocha de ponta pra baixo no céu, grama em cima, torrinha, árvore e uma cascata ──
void drawFloatIsland(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const double ZB = 9.5;
    const RGB rock = { 132, 118, 104 };
    groundShadow(p, cam, b.x, b.y, 4.2, 0.18);
    std::vector<QPointF> fall;
    for (const V3& q : { L(4.1, -0.45, ZB + 0.3), L(4.1, 0.45, ZB + 0.3), L(4.5, 0.8, 0.6), L(4.5, -0.8, 0.6) }) fall.push_back(cam.proj(q[0], q[1], q[2]));
    Faces f = below(blobFaces(b.x, b.y, ZB, 4.5, 6.4, 12, 6, rock, seedOf(b, 1), 0.22, { rock, sc(rock, 0.88), { 112, 100, 92 } }), ZB);
    append(f, above(blobFaces(b.x, b.y, ZB - 0.05, 4.6, 0.7, 12, 4, { 104, 150, 72 }, seedOf(b, 2), 0.1, { { 104, 150, 72 }, { 92, 140, 66 }, { 116, 160, 80 } }), ZB - 0.05));
    Rng R(seedOf(b, 3));
    for (int k = 0; k < 3; k++) {
        const double a = R() * 6.28, d = 5.8 + R() * 1.4, zz = ZB - 1.5 + R() * 2.4, r = 0.5 + R() * 0.4;
        append(f, blobFaces(b.x + std::cos(a) * d, b.y + std::sin(a) * d, zz, r, 0.7, 6, 3, rock, seedOf(b, 10 + k), 0.25));
    }
    drawFaces(p, f, cam);
    // cascata: cai da borda e some em névoa
    QLinearGradient gr(fall[0], fall[3]);
    gr.setColorAt(0, QColor(200, 230, 255, 242)); gr.setColorAt(1, QColor(200, 230, 255, 0));
    p.fillPath(pathOf(fall), gr);
    // em cima: a torrinha e a árvore
    Faces top;
    const P2 t = L.at(-1.2, 1.0);
    append(top, taperFaces(t[0], t[1], 0.95, 0.85, ZB + 0.5, ZB + 4.4, 8, { 214, 206, 190 }, [](int i) -> Deco {
        if (i % 3) return Deco();
        return [](QPainter& q, const PtFn& pt, double kk) { quad(q, pt, 0.35, 0.65, 0.6, 0.75, rgb({ 50, 40, 34 }, kk)); };
    }));
    append(top, coneFaces(t[0], t[1], 1.15, ZB + 4.4, 2.0, 8, { 70, 96, 150 }));
    const P2 q = L.at(1.6, -1.2);
    append(top, beamFaces({ q[0], q[1], ZB + 0.5 }, { q[0], q[1], ZB + 2.2 }, 0.35, 0.35, { 96, 70, 48 }));
    append(top, blobFaces(q[0], q[1], ZB + 3.0, 1.5, 1.2, 8, 4, { 80, 136, 66 }, seedOf(b, 4), 0.15, { { 80, 136, 66 }, { 94, 148, 74 } }));
    drawFaces(p, top, cam);
    glowAt(p, cam, L(4.5, 0, 0.8), 1.6, { 230, 244, 255 }, 0.5);
}

// ── fazenda: celeiro vermelho de telhado quebrado, silo, a plantação cercada e fardos de feno ──
void drawFarm(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const RGB red = { 170, 54, 42 }, trim = { 236, 228, 214 }, roofC = { 84, 72, 68 };
    Faces f;
    // plantação (chão), cercada
    f.push_back(flatPoly({ L(-1.6, -3.2, 0.04), L(-1.6, 3.2, 0.04), L(-6.6, 3.2, 0.04), L(-6.6, -3.2, 0.04) }, { 212, 182, 96 }, L(-4, 0, -1), [](QPainter& q, const PtFn& pt, double) {
        strokeLines(q, QColor(150, 120, 50, 153), 0.8); for (int i = 1; i < 10; i++) line(q, pt(i / 10.0, 0.04), pt(i / 10.0, 0.96));
    }));
    const P2 fence[4] = { { -1.4, -3.4 }, { -1.4, 3.4 }, { -6.8, 3.4 }, { -6.8, -3.4 } };
    for (int k = 0; k < 4; k++) {
        const P2& a = fence[k]; const P2& c = fence[(k + 1) % 4];
        const int n = static_cast<int>(jsRound(std::hypot(c[0] - a[0], c[1] - a[1]) / 1.2));
        for (int i = 0; i <= n; i++) { const double x = a[0] + (c[0] - a[0]) * i / n, y = a[1] + (c[1] - a[1]) * i / n; append(f, beamFaces(L(x, y, 0), L(x, y, 0.8), 0.12, 0.12, { 140, 110, 78 })); }
        append(f, beamFaces(L(a[0], a[1], 0.6), L(c[0], c[1], 0.6), 0.08, 0.08, { 150, 120, 86 }));
    }
    // celeiro: paredes, oitões e o telhado em dois caimentos de cada lado
    const double bx = 1.6, by = -1.2, W = 2.2, D = 1.6, Z = 2.3;
    const P2 bc = L.at(bx, by);
    const V3 C = L(bx, by, 1.6);
    append(f, boxFaces(bc[0], bc[1], b.yaw, 2 * W, 2 * D, 0, Z, red, nullptr, [trim](int side, double) -> Deco {
        if (side != 1) return Deco();
        return [trim](QPainter& q, const PtFn& pt, double kk) {
            quad(q, pt, 0.3, 0.7, 0, 0.8, rgb({ 120, 36, 30 }, kk));
            q.setPen(pen(rgb(trim, kk), 1)); q.setBrush(Qt::NoBrush);
            q.drawPath(pathOf({ pt(0.3, 0), pt(0.7, 0), pt(0.7, 0.8), pt(0.3, 0.8) }));
            line(q, pt(0.3, 0), pt(0.7, 0.8)); line(q, pt(0.7, 0), pt(0.3, 0.8));
        };
    }));
    for (int sx : { -1, 1 }) {
        Face g = face({ L(bx + sx * W, by - D, Z), L(bx + sx * W, by - D * 0.62, Z + 1.0), L(bx + sx * W, by, Z + 1.6), L(bx + sx * W, by + D * 0.62, Z + 1.0), L(bx + sx * W, by + D, Z) }, red, C,
                      sx > 0 ? Deco([](QPainter& q, const PtFn& pt, double kk) { quad(q, pt, 0.4, 0.6, 0.3, 0.6, rgb({ 255, 210, 130 }, kk, 0.85)); }) : Deco());
        g.q = { 0, 4, 2, 2 };
        f.push_back(std::move(g));
    }
    for (int sg : { -1, 1 }) {
        f.push_back(face({ L(bx - W - 0.15, by + sg * (D + 0.15), Z - 0.05), L(bx + W + 0.15, by + sg * (D + 0.15), Z - 0.05), L(bx + W + 0.15, by + sg * D * 0.62, Z + 1.0), L(bx - W - 0.15, by + sg * D * 0.62, Z + 1.0) }, roofC, C, roofDeco, 0.01));
        f.push_back(face({ L(bx - W - 0.15, by + sg * D * 0.62, Z + 1.0), L(bx + W + 0.15, by + sg * D * 0.62, Z + 1.0), L(bx + W + 0.15, by, Z + 1.65), L(bx - W - 0.15, by, Z + 1.65) }, sc(roofC, 1.08), C, roofDeco, 0.01));
    }
    // silo
    const P2 s = L.at(-0.2, 2.2);
    append(f, taperFaces(s[0], s[1], 1.0, 1.0, 0, 5.0, 12, { 176, 180, 184 }, [](int) -> Deco {
        return [](QPainter& q, const PtFn& pt, double) { strokeLines(q, QColor(90, 90, 96, 128), 0.6); for (int i = 1; i < 5; i++) line(q, pt(0, i / 5.0), pt(1, i / 5.0)); };
    }));
    append(f, coneFaces(s[0], s[1], 1.08, 5.0, 0.9, 12, { 140, 146, 152 }));
    // fardos de feno
    for (const P2& h : { P2{ 3.6, 2.0 }, P2{ 4.4, 2.8 }, P2{ 3.4, 3.2 } }) {
        const P2 q = L.at(h[0], h[1]);
        append(f, taperFaces(q[0], q[1], 0.55, 0.55, 0, 0.75, 8, { 214, 186, 110 }));
        append(f, coneFaces(q[0], q[1], 0.55, 0.75, 0.12, 8, { 224, 198, 124 }));
    }
    drawFaces(p, f, cam);
}

// ── espada na pedra: lâmina cravada numa rocha, guarda e pomo dourados, uma luz suave ──
void drawSword(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const RGB gold = { 216, 176, 84 };
    glowAt(p, cam, L(0, 0, 5.6), 5.0, { 255, 236, 170 }, 0.22, 3);
    Faces f = above(blobFaces(b.x, b.y, 0, 2.7, 1.9, 10, 4, { 140, 136, 128 }, seedOf(b, 1), 0.2, { { 140, 136, 128 }, { 126, 124, 116 }, { 150, 146, 136 } }), 0);
    const P2 r2 = L.at(2.3, 1.4);
    append(f, above(blobFaces(r2[0], r2[1], 0, 1.0, 0.7, 6, 3, { 132, 128, 120 }, seedOf(b, 2), 0.25), 0));
    for (const P2& q : { P2{ 0.9, -1.1 }, P2{ -1.3, 1.0 }, P2{ 1.7, -0.3 } })
        f.push_back(flatPoly({ L(q[0] - 0.4, q[1], 1.3), L(q[0] + 0.4, q[1], 1.3), L(q[0], q[1] + 0.5, 1.6) }, { 96, 140, 70 }, L(0, 0, 0)));
    f.push_back(flatPoly({ L(-0.08, -0.42, 1.6), L(-0.08, 0.42, 1.6), L(0, 0.38, 8.6), L(0, -0.38, 8.6) }, { 206, 214, 222 }, L(-1, 0, 3), [](QPainter& q, const PtFn& pt, double) {
        strokeLines(q, QColor(120, 130, 140, 179), 0.7); line(q, pt(0.5, 0), pt(0.5, 1));
    }));
    append(f, beamFaces(L(0, -1.6, 8.65), L(0, 1.6, 8.65), 0.4, 0.4, gold));
    append(f, beamFaces(L(0, 0, 8.7), L(0, 0, 10.5), 0.34, 0.34, { 96, 60, 38 }));
    const P2 pm = L.at(0, 0);
    append(f, blobFaces(pm[0], pm[1], 10.8, 0.46, 0.42, 6, 3, gold, 9, 0.05));
    drawFaces(p, f, cam);
}

// luz de noite: [lx, ly, z, raio, tipo, força] no sistema do artefato
const QHash<QString, std::vector<ArtLight>>& lightTable()
{
    static const QHash<QString, std::vector<ArtLight>> t = {
        { "ship", { { -4.6, 0, 2.4, 2.6, "lantern", 0.9 }, { 3.6, 0, 2.2, 1.8, "lantern", 0.7 } } },
        { "colosseum", { { 10, 0, 5, 2.4, "torch", 0.8 }, { -10, 0, 5, 2.4, "torch", 0.8 }, { 0, 7.6, 5, 2.4, "torch", 0.8 }, { 0, -7.6, 5, 2.4, "torch", 0.8 } } },
        { "cave", { { 4.2, -2.3, 2.4, 2.2, "torch", 1 }, { 4.2, 2.3, 2.4, 2.2, "torch", 1 } } },
        { "mine", { { 3.3, -1.4, 2.0, 2.0, "lantern", 0.9 }, { 5.4, -3.2, 1.2, 1.8, "window", 0.8 } } },
        { "mage", { { 0, 0, 17.2, 7, "arcane", 1 }, { 0, 0, 7, 2.4, "arcane", 0.6 } } },
        { "greattree", { { 3, 2, 11, 3.2, "fairy", 0.7 }, { -3.5, 1, 13, 3, "fairy", 0.7 }, { 1, -4, 14, 3, "fairy", 0.7 }, { 0, 0, 17, 4, "fairy", 0.6 }, { -2, -2, 9, 2.6, "fairy", 0.6 } } },
        { "camp", { { 0, 0, 0.8, 4.6, "fire", 1 }, { -0.6, -6.2, 1.2, 1.8, "window", 0.6 } } },
        { "dragon", { { 5.0, 0, 8.6, 1.4, "eye", 0.9 } } },
        { "bones", { { 0, 0, 1.6, 3.4, "ghost", 0.35 } } },
        { "seaserpent", { { 5.3, 0, 4.3, 1.3, "eye", 0.9 } } },
        { "kraken", { { 1.7, 0, 0.9, 1.8, "eye", 0.9 } } },
        { "longship", { { -4.2, 0, 2.4, 1.8, "lantern", 0.8 }, { 3.6, 0, 2.2, 1.4, "lantern", 0.6 } } },
        { "wreck", { { -3.6, -0.6, 0.8, 1.4, "lantern", 0.45 } } },
        { "pyramid", { { 5.8, -0.9, 0.9, 1.8, "torch", 0.9 }, { 5.8, 0.9, 0.9, 1.8, "torch", 0.9 }, { 0, 0, 7.4, 2.4, "holy", 0.45 } } },
        { "ziggurat", { { 1.7, -1.35, 7.8, 2.6, "fire", 1 }, { 1.7, 1.35, 7.8, 2.6, "fire", 1 }, { 1.2, 0, 7.8, 1.4, "window", 0.7 } } },
        { "pagoda", { { 2.4, -2.4, 1.6, 1.4, "lantern", 0.9 }, { 2.4, 2.4, 1.6, 1.4, "lantern", 0.9 }, { 0, 0, 6, 1.8, "window", 0.7 }, { 0, 0, 10, 1.6, "window", 0.6 } } },
        { "graveyard", { { -1.0, 1.6, 1.4, 1.6, "lantern", 0.8 }, { 2.0, -1.2, 1.0, 3.6, "ghost", 0.5 } } },
        { "portal", { { 0, 0, 3.9, 6.5, "arcane", 1 } } },
        { "heads", { { 2.2, -5.4, 1.4, 1.8, "torch", 0.85 }, { 2.2, 5.4, 1.4, 1.8, "torch", 0.85 } } },
        { "floatisland", { { -1.2, 1.0, 12.5, 1.8, "window", 0.85 }, { 0, 0, 7.5, 4.4, "fairy", 0.55 } } },
        { "farm", { { 3.9, -1.2, 3.0, 1.6, "window", 0.85 } } },
        { "sword", { { 0, 0, 6.2, 4.4, "holy", 0.7 } } },
        // leva 4
        { "colossus", { { 0.05 * 24, 0.175 * 24, 3.6 + 1.2 * 24, 4.2, "fire", 1 }, { 3.6, 0, 1.8, 2.2, "torch", 0.8 } } },
        { "guardians", { { 1.6, -5.6, 2.8, 2.0, "torch", 0.8 }, { 1.6, 5.6, 2.8, 2.0, "torch", 0.8 } } },
        { "seatedgod", { { 4.0, -3.4, 2.8, 2.6, "fire", 1 }, { 4.0, 3.4, 2.8, 2.6, "fire", 1 } } },
        { "cathedral", { { 0, 0, 4.2, 4.6, "window", 0.85 }, { 5.3, 0, 6.4, 2.6, "window", 1 }, { -6.6, 0, 3.6, 2.4, "window", 0.7 } } },
        { "arch", { { 1.8, -2.3, 1.2, 1.8, "torch", 0.85 }, { 1.8, 2.3, 1.2, 1.8, "torch", 0.85 } } },
        { "market", { { 3.5, 2.5, 2.4, 1.8, "lantern", 0.9 }, { -3.2, -2.8, 2.4, 1.8, "lantern", 0.9 }, { -1.0, 4.2, 2.4, 1.8, "lantern", 0.85 }, { 2.2, -4.0, 2.4, 1.8, "lantern", 0.85 } } },
    };
    return t;
}
}

bool drawArt(QPainter& p, const Art& a, const Cam& cam, const RGB& realmCol)
{
    if (drawArt4(p, a, cam, realmCol)) return true;          // leva 4: castelos novos e os monumentos grandes
    const QString& k = a.kind;
    if (k == QLatin1String("castle")) {
        if (a.style.isEmpty() || a.style == QLatin1String("human")) return false;
        drawStyledCastle(p, a, cam, realmCol);
    }
    else if (k == QLatin1String("statue")) drawStatue(p, a, cam);
    else if (k == QLatin1String("ship")) drawShip(p, a, cam, realmCol);
    else if (k == QLatin1String("colosseum")) drawColosseum(p, a, cam);
    else if (k == QLatin1String("cave")) drawCave(p, a, cam);
    else if (k == QLatin1String("mine")) drawMine(p, a, cam);
    else if (k == QLatin1String("mage")) drawMageTower(p, a, cam);
    else if (k == QLatin1String("greattree")) drawGreatTree(p, a, cam);
    else if (k == QLatin1String("stones")) drawStones(p, a, cam);
    else if (k == QLatin1String("camp")) drawCamp(p, a, cam);
    else if (k == QLatin1String("dragon")) drawDragon(p, a, cam);
    else if (k == QLatin1String("bones")) drawBones(p, a, cam);
    else if (k == QLatin1String("seaserpent")) drawSeaSerpent(p, a, cam);
    else if (k == QLatin1String("kraken")) drawKraken(p, a, cam);
    else if (k == QLatin1String("longship")) drawLongship(p, a, cam, realmCol);
    else if (k == QLatin1String("wreck")) drawWreck(p, a, cam);
    else if (k == QLatin1String("pyramid")) drawPyramid(p, a, cam);
    else if (k == QLatin1String("ziggurat")) drawZiggurat(p, a, cam);
    else if (k == QLatin1String("pagoda")) drawPagoda(p, a, cam);
    else if (k == QLatin1String("obelisk")) drawObelisk(p, a, cam);
    else if (k == QLatin1String("graveyard")) drawGraveyard(p, a, cam);
    else if (k == QLatin1String("portal")) drawPortal(p, a, cam);
    else if (k == QLatin1String("heads")) drawHeads(p, a, cam);
    else if (k == QLatin1String("floatisland")) drawFloatIsland(p, a, cam);
    else if (k == QLatin1String("farm")) drawFarm(p, a, cam);
    else if (k == QLatin1String("sword")) drawSword(p, a, cam);
    else return false;
    return true;
}

const std::vector<ArtLight>& artLights(const QString& kind)
{
    static const std::vector<ArtLight> none;
    const auto& t = lightTable();
    const auto it = t.find(kind);
    return it == t.end() ? none : it.value();
}

} // namespace carto::spr
