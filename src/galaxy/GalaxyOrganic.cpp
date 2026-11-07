#include "GalaxyOrganic.h"

#include "GalaxyCanvas.h"   // inRows
#include "GalaxyDoc.h"

#include <QHash>
#include <QMutex>

#include <algorithm>
#include <cmath>

namespace galaxy {

namespace {
// as mesmas medidas do mapa (GalaxyCanvasRegions)
constexpr double RAD = 0.06, NECK = 0.034, MELT = 0.05, LINK = 0.42;
constexpr double STAMP_MELT = 0.1;   // a forma pintada funde mais macio: as pinceladas somem numa massa só
constexpr double ORG = 0.04;         // o máximo que o recorte mexe na fronteira

inline double sminK(double a, double b, double m)
{
    const double h = std::max(m - std::fabs(a - b), 0.0) / m;
    return std::min(a, b) - h * h * m * 0.25;
}
// ruído de valor e fbm: o recorte de litoral
inline double vnoise(double x, double y)
{
    const double fx = std::floor(x), fy = std::floor(y), xf = x - fx, yf = y - fy;
    const int xi = int(fx), yi = int(fy);
    const auto hh = [](int i, int j) {
        uint32_t n = uint32_t(i) * 374761393u + uint32_t(j) * 668265263u;
        n = (n ^ (n >> 13)) * 1274126177u;
        return double(n ^ (n >> 16)) / 4294967296.0;
    };
    const double u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf);
    const double a = hh(xi, yi), b = hh(xi + 1, yi), c = hh(xi, yi + 1), d = hh(xi + 1, yi + 1);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
inline double fbm(double x, double y, int oct)
{
    double s = 0, a = 0.5, f = 1;
    for (int o = 0; o < oct; o++) { s += a * (vnoise(x * f, y * f) - 0.5); f *= 2.03; a *= 0.5; }
    return s;
}
// A galáxia desfocada (72 px): a fronteira segue os braços sem tremer a cada estrela.
struct Soft { qint64 key = -1; int N = 0; std::vector<float> v; };
Soft g_soft;
QMutex g_softLock;
const Soft& softOf(const Doc& d)
{
    QMutexLocker lock(&g_softLock);
    const qint64 key = d.galImg.cacheKey();
    if (g_soft.key == key && g_soft.N) return g_soft;
    g_soft.key = key;
    const int N = 72;
    g_soft.N = N;
    g_soft.v.assign(size_t(N * N), 0.f);
    if (d.galImg.isNull()) return g_soft;
    const QImage s = d.galImg.scaled(N, N, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);
    std::vector<float> a(size_t(N * N)), b(size_t(N * N));
    for (int y = 0; y < N; y++) {
        const QRgb* row = reinterpret_cast<const QRgb*>(s.constScanLine(y));
        for (int x = 0; x < N; x++) a[size_t(y * N + x)] = float((qRed(row[x]) + qGreen(row[x]) + qBlue(row[x])) / 765.0);
    }
    // desfoque leve (duas passadas em caixa de 3)
    for (int pass = 0; pass < 2; pass++) {
        for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) { float t = 0; int n = 0; for (int i = -1; i <= 1; i++) { const int xx = x + i; if (xx < 0 || xx >= N) continue; t += a[size_t(y * N + xx)]; n++; } b[size_t(y * N + x)] = t / float(n); }
        for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) { float t = 0; int n = 0; for (int i = -1; i <= 1; i++) { const int yy = y + i; if (yy < 0 || yy >= N) continue; t += b[size_t(yy * N + x)]; n++; } a[size_t(y * N + x)] = t / float(n); }
    }
    g_soft.v = a;
    return g_soft;
}
double softAt(const Soft& S, double x, double y)
{
    if (!S.N) return 0;
    const int N = S.N;
    const double fx = std::clamp((x + EXT) / (2 * EXT) * (N - 1), 0.0, N - 1.001), fy = std::clamp((EXT - y) / (2 * EXT) * (N - 1), 0.0, N - 1.001);
    const int i = int(fx), j = int(fy);
    const double a = fx - i, b = fy - j;
    const float* v = S.v.data();
    return v[j * N + i] * (1 - a) * (1 - b) + v[j * N + i + 1] * a * (1 - b) + v[(j + 1) * N + i] * (1 - a) * b + v[(j + 1) * N + i + 1] * a * b;
}
// o quanto a fronteira entra ou sai naquele ponto (negativo = o território avança).
// Lê a galáxia desfocada da última galáxia preparada (softOf é chamado antes por quem monta a forma).
inline double organicAt(double x, double y, double sd)
{
    const double X = x * 7.83 + sd, Y = y * 7.83 - sd;
    const double wx = fbm(X + 3.1, Y + 7.7, 2), wy = fbm(X - 5.3, Y + 1.9, 2);
    const double soft = g_soft.N ? softAt(g_soft, x, y) : 0.2;
    return fbm(X + wx * 2.4, Y + wy * 2.4, 3) * 0.063 - (soft - 0.2) * 0.0575;
}
double seedOf(const QString& key) { return double(qHash(key) % 997u) * 0.731; }
void linksFor(TerrField& F)
{
    const auto& pts = F.pts;
    if (pts.size() < 2) return;
    std::vector<size_t> left, inside { 0 };
    for (size_t i = 1; i < pts.size(); i++) left.push_back(i);
    while (!left.empty()) {
        size_t bi = 0, bo = inside[0];
        double bd = 1e18;
        for (size_t i = 0; i < left.size(); i++) for (size_t o : inside) {
            const double d = std::hypot(pts[o][0] - pts[left[i]][0], pts[o][1] - pts[left[i]][1]);
            if (d < bd) { bd = d; bi = i; bo = o; }
        }
        const size_t p = left[bi];
        left.erase(left.begin() + long(bi));
        inside.push_back(p);
        if (bd < LINK) { const double dx = pts[bo][0] - pts[p][0], dy = pts[bo][1] - pts[p][1]; F.links.push_back({ pts[p][0], pts[p][1], dx, dy, std::max(1e-12, dx * dx + dy * dy) }); }
    }
}
} // namespace

TerrField terrField(const Doc& d, const Region& r, bool withSolo)
{
    TerrField F;
    QList<int> ids = r.systems.values();
    std::sort(ids.begin(), ids.end());
    for (int id : ids) {
        if (id < 0 || id >= int(d.gal->systems.size())) continue;
        const System& s = *d.gal->systems[size_t(id)];
        if (s.isCore) continue;
        if (s.gone && !(withSolo && d.solo.contains(s.id))) continue;
        F.pts.push_back({ s.x, s.y });
    }
    F.stamps = r.stamps;
    F.sd = seedOf(r.key);
    linksFor(F);
    softOf(d);
    return F;
}
TerrField stampField(const Region& r)
{
    TerrField F;
    F.stamps = r.stamps;
    F.sd = seedOf(r.key);
    return F;
}
double terrDist(const TerrField& F, double x, double y)
{
    double d = 1e18;
    for (const auto& p : F.pts) d = sminK(d, std::hypot(x - p[0], y - p[1]) - RAD, MELT);
    for (const auto& s : F.links) {
        const double a = x - s.x, b = y - s.y, k = std::clamp((a * s.dx + b * s.dy) / s.l, 0.0, 1.0);
        d = sminK(d, std::hypot(a - s.dx * k, b - s.dy * k) - NECK, MELT);
    }
    for (const auto& s : F.stamps) d = sminK(d, std::hypot(x - s[0], y - s[1]) - s[2], STAMP_MELT);
    if (!F.organic || d > ORG * 2.5) return d;
    return d + organicAt(x, y, F.sd);
}
bool terrEmpty(const TerrField& F) { return F.pts.empty() && F.stamps.empty(); }
bool terrBox(const TerrField& F, double& x0, double& y0, double& x1, double& y1)
{
    x0 = y0 = 9; x1 = y1 = -9;
    const auto grow = [&](double x, double y, double r) { x0 = std::min(x0, x - r); y0 = std::min(y0, y - r); x1 = std::max(x1, x + r); y1 = std::max(y1, y + r); };
    for (const auto& p : F.pts) grow(p[0], p[1], RAD + MELT + ORG);
    for (const auto& s : F.stamps) grow(s[0], s[1], s[2] + STAMP_MELT * 0.5 + ORG);
    return x1 > x0;
}
bool terrCenter(const TerrField& F, double& x, double& y)
{
    double sx = 0, sy = 0, w = 0;
    for (const auto& p : F.pts) { sx += p[0] * RAD; sy += p[1] * RAD; w += RAD; }
    for (const auto& s : F.stamps) { sx += s[0] * s[2]; sy += s[1] * s[2]; w += s[2]; }
    if (w <= 0) return false;
    x = sx / w; y = sy / w;
    return true;
}
double galSoft(const Doc& d, double x, double y) { return softAt(softOf(d), x, y); }

TerrImage terrImage(const Doc& d, const TerrField& F, const QColor& color, const double A[6], int W, int H, double zpx, double dpr, bool busy)
{
    TerrImage out;
    double bx0, by0, bx1, by1;
    if (!terrBox(F, bx0, by0, bx1, by1)) return out;
    const Soft& S = softOf(d);
    const int G = std::max(2, int(std::lround((busy ? 8 : 4) * dpr)));
    const double GLOW = 22 * dpr, RIM = std::clamp(0.03 * zpx / dpr, 10.0, 90.0) * dpr;
    const double ox = A[0], oy = A[1], ax = A[2] - ox, ay = A[3] - oy, bx = A[4] - ox, by = A[5] - oy;
    double x0 = W, y0 = H, x1 = 0, y1 = 0;
    for (double wx : { bx0, bx1 }) for (double wy : { by0, by1 }) {
        const double sx = ox + ax * wx + bx * wy, sy = oy + ay * wx + by * wy;
        x0 = std::min(x0, sx); y0 = std::min(y0, sy); x1 = std::max(x1, sx); y1 = std::max(y1, sy);
    }
    const int ix0 = std::max(0, int(std::floor(x0 - GLOW - G))), iy0 = std::max(0, int(std::floor(y0 - GLOW - G)));
    const int ix1 = std::min(W, int(std::ceil(x1 + GLOW + G))), iy1 = std::min(H, int(std::ceil(y1 + GLOW + G)));
    if (ix1 <= ix0 || iy1 <= iy0) return out;
    const int w = ix1 - ix0, h = iy1 - iy0, gw = (w + G - 1) / G, gh = (h + G - 1) / G, row = gw + 1;
    const double det = ax * by - bx * ay;
    if (std::fabs(det) < 1e-12) return out;
    const double ia = by / det, ib = -bx / det, ic = -ay / det, id = ax / det;
    std::vector<float> fd(size_t(row) * size_t(gh + 1), 1e9f);
    (void)S;
    // Em blocos de 8×8 pontos da grade: cada bloco só faz conta com as peças que chegam perto dele (com centenas de
    // pinceladas, isso é o que deixa o pincel leve). Peça longe não muda nada que apareça.
    const int TB = 8, tw = (gw + 1 + TB - 1) / TB, th = (gh + 1 + TB - 1) / TB;
    const double reach = (GLOW + G * 3.0) / zpx + 2 * STAMP_MELT + ORG * 2;
    inRows(th, [&](int ta, int tb) {
        TerrField sub;
        sub.sd = F.sd; sub.organic = F.organic;
        for (int tj = ta; tj < tb; tj++) for (int ti = 0; ti < tw; ti++) {
            const int i0 = ti * TB, j0 = tj * TB, i1 = std::min(gw, i0 + TB - 1), j1 = std::min(gh, j0 + TB - 1);
            double wx0 = 1e9, wy0 = 1e9, wx1 = -1e9, wy1 = -1e9;
            for (int ci : { i0, i1 }) for (int cj : { j0, j1 }) {
                const double X = ix0 + ci * G - ox, Y = iy0 + cj * G - oy, wx = ia * X + ib * Y, wy = ic * X + id * Y;
                wx0 = std::min(wx0, wx); wx1 = std::max(wx1, wx); wy0 = std::min(wy0, wy); wy1 = std::max(wy1, wy);
            }
            const auto gap = [&](double x, double y) { const double dx = std::max({ wx0 - x, 0.0, x - wx1 }), dy = std::max({ wy0 - y, 0.0, y - wy1 }); return std::hypot(dx, dy); };
            sub.pts.clear(); sub.links.clear(); sub.stamps.clear();
            for (const auto& p : F.pts) if (gap(p[0], p[1]) < RAD + reach) sub.pts.push_back(p);
            for (const auto& l : F.links) {
                const double lx0 = std::min(l.x, l.x + l.dx), lx1 = std::max(l.x, l.x + l.dx), ly0 = std::min(l.y, l.y + l.dy), ly1 = std::max(l.y, l.y + l.dy);
                if (lx1 + NECK + reach >= wx0 && lx0 - NECK - reach <= wx1 && ly1 + NECK + reach >= wy0 && ly0 - NECK - reach <= wy1) sub.links.push_back(l);
            }
            for (const auto& st : F.stamps) if (gap(st[0], st[1]) < st[2] + reach) sub.stamps.push_back(st);
            if (sub.pts.empty() && sub.links.empty() && sub.stamps.empty()) continue;
            for (int j = j0; j <= j1; j++) for (int i = i0; i <= i1; i++) {
                const double X = ix0 + i * G - ox, Y = iy0 + j * G - oy;
                fd[size_t(j * row + i)] = float(terrDist(sub, ia * X + ib * Y, ic * X + id * Y) * zpx);
            }
        }
    });
    const int cr = color.red(), cg = color.green(), cb = color.blue();
    const int lutN = int(std::ceil((RIM + GLOW) * 4)) + 1;
    std::vector<uint32_t> lut(static_cast<size_t>(lutN));
    for (int n = 0; n < lutN; n++) {
        const double q = n / 4.0 - RIM, line = 1 - smooth(0.55 * dpr, 1.35 * dpr, std::fabs(q));
        const double fill = q < 0 ? 0.03 + 0.2 * std::pow(1 - smooth(0, RIM, -q), 2) : 0.1 * (1 - smooth(0, GLOW, q));
        const int al = int(std::floor(std::max(fill, line * 0.9) * 255 + 0.5));
        const double K = line * 0.3;
        lut[size_t(n)] = qPremultiply(qRgba(int(cr + (255 - cr) * K + 0.5), int(cg + (255 - cg) * K + 0.5), int(cb + (255 - cb) * K + 0.5), al));
    }
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(0u);
    uchar* bits = img.bits();
    const qsizetype bpl = img.bytesPerLine();
    const uint32_t core = lut[0];
    inRows(gh, [&](int ra, int rb) {
        for (int j = ra; j < rb; j++) for (int i = 0; i < gw; i++) {
            const int k = j * row + i;
            const double d00 = fd[size_t(k)], d10 = fd[size_t(k + 1)], d01 = fd[size_t(k + row)], d11 = fd[size_t(k + row + 1)];
            if (std::min({ d00, d10, d01, d11 }) >= GLOW) continue;
            const int xs = i * G, ys = j * G, xe = std::min(w, xs + G), ye = std::min(h, ys + G);
            const bool solid = std::max({ d00, d10, d01, d11 }) <= -RIM;
            for (int y = ys; y < ye; y++) {
                uint32_t* px = reinterpret_cast<uint32_t*>(bits + bpl * y);
                if (solid) { for (int x = xs; x < xe; x++) px[x] = core; continue; }
                const double v = double(y - ys) / G, dl = d00 + (d01 - d00) * v, dr = d10 + (d11 - d10) * v;
                for (int x = xs; x < xe; x++) {
                    const double q = dl + (dr - dl) * (double(x - xs) / G);
                    if (q >= GLOW) continue;
                    px[x] = lut[size_t(q <= -RIM ? 0 : std::min(lutN - 1, int(std::floor((q + RIM) * 4 + 0.5))))];
                }
            }
        }
    });
    out.img = img;
    out.x = ix0; out.y = iy0;
    out.empty = false;
    return out;
}

bool spreadSpot(const Doc& d, const Region* r, double& x, double& y, uint32_t seed, const QSet<int>& ignore)
{
    const Galaxy& g = *d.gal;
    const Soft& S = softOf(d);
    Rng rng(seed);
    // quem já está no mapa, mundos soltos inclusive
    std::vector<std::array<double, 2>> others;
    std::vector<int> members;
    for (const auto& s : g.systems) {
        if (s->isCore || ignore.contains(s->id)) continue;
        const bool shown = !s->gone || d.solo.contains(s->id);
        if (!shown) continue;
        others.push_back({ s->x, s->y });
        if (r && r->systems.contains(s->id)) members.push_back(s->id);
    }
    const auto nearest = [&](double px, double py, double cap) { double m = cap; for (const auto& o : others) m = std::min(m, std::hypot(o[0] - px, o[1] - py)); return m; };
    std::vector<const System*> spare;
    for (const auto& s : g.systems) if (d.veiled.contains(s->id)) spare.push_back(s.get());
    const auto fromSpare = [&](double& px, double& py) {
        if (!spare.empty()) { const System* s = spare[size_t(rng() * double(spare.size())) % spare.size()]; px = s->x + (rng() - 0.5) * 0.012; py = s->y + (rng() - 0.5) * 0.012; }
        else { const double a = rng() * TAU, dd = 0.2 + std::sqrt(rng()) * 0.7; px = std::cos(a) * dd; py = std::sin(a) * dd; }
    };
    // os outros territórios (pra não nascer dentro deles)
    std::vector<TerrField> fields;
    for (const auto& o : d.regions) { if (o.get() == r) continue; TerrField F = terrField(d, *o); if (!terrEmpty(F)) fields.push_back(std::move(F)); }
    const auto otherDist = [&](double px, double py) { double m = 0.5; for (const auto& F : fields) m = std::min(m, terrDist(F, px, py)); return m; };

    double bestX = 0, bestY = 0, bv = -1e9;
    const auto consider = [&](double px, double py, double v) { if (std::hypot(px, py) > 1.05) v -= 10; if (v > bv) { bv = v; bestX = px; bestY = py; } };
    if (r && !r->stamps.empty()) {
        // pela forma pintada inteira: o mais longe dos outros, só entre pontos com folga da borda
        const TerrField SF = stampField(*r);
        double x0 = 9, y0 = 9, x1 = -9, y1 = -9;
        for (const auto& s : r->stamps) { x0 = std::min(x0, s[0] - s[2]); y0 = std::min(y0, s[1] - s[2]); x1 = std::max(x1, s[0] + s[2]); y1 = std::max(y1, s[1] + s[2]); }
        for (int n = 0; n < 72; n++) {
            double px = r->stamps[0][0], py = r->stamps[0][1];
            for (int k = 0; k < 40; k++) {
                const double qx = x0 + rng() * (x1 - x0), qy = y0 + rng() * (y1 - y0);
                bool in = false;
                for (const auto& s : r->stamps) if (std::hypot(s[0] - qx, s[1] - qy) < s[2] * 0.72) { in = true; break; }
                if (in) { px = qx; py = qy; break; }
            }
            const double depth = -terrDist(SF, px, py);
            consider(px, py, depth < 0.0575 ? depth - 1 : nearest(px, py, 0.69) + softAt(S, px, py) * 0.04 + std::min(depth, 0.092) * 0.3);
        }
    } else if (r && !members.empty()) {
        // sem forma pintada: em volta de quem já está lá, espalhando
        double cx = 0, cy = 0;
        for (int id : members) { cx += g.systems[size_t(id)]->x; cy += g.systems[size_t(id)]->y; }
        cx /= double(members.size()); cy /= double(members.size());
        const double R = (0.05 + 0.035 * std::sqrt(double(members.size()) + 1)) * 1.15;
        for (int n = 0; n < 72; n++) {
            const double a = rng() * TAU, dd = std::sqrt(rng()) * R, px = cx + std::cos(a) * dd, py = cy + std::sin(a) * dd;
            consider(px, py, nearest(px, py, 0.69) + softAt(S, px, py) * 0.04 - (otherDist(px, py) < 0.04 ? 3 : 0));
        }
    } else if (r) {
        // o primeiro de um território sem forma: num lugar com estrelas, longe dos outros territórios
        for (int n = 0; n < 60; n++) {
            double px, py;
            fromSpare(px, py);
            consider(px, py, std::min(otherDist(px, py), 0.5) + softAt(S, px, py) * 0.3 + std::min(nearest(px, py, 0.2), 0.2));
        }
    } else {
        // solto: longe dos outros e fora de qualquer território
        for (int n = 0; n < 72; n++) {
            double px, py;
            fromSpare(px, py);
            consider(px, py, nearest(px, py, 0.184) * 5 + softAt(S, px, py) * 0.5 - (otherDist(px, py) < 0.04 ? 3 : 0));
        }
    }
    x = bestX; y = bestY;
    return true;
}

} // namespace galaxy
