// As vistas nascidas do espaço: cada uma lê uma coisa que a galáxia tem e o papel não mostra (luz, massa, rotas, domínios).
// As pinturas moram na camada do fundo, refeita quando a câmera mexe (porte de spaceviews.js).

#include "GalaxyCanvas.h"

#include <QCoreApplication>
#include <QLinearGradient>
#include <QLocale>
#include <QPainterPath>
#include <QRadialGradient>

#include <set>

namespace galaxy {

namespace {
QColor cssOf(const Rgb& c, double a = 1) { return QColor(std::clamp(c[0], 0, 255), std::clamp(c[1], 0, 255), std::clamp(c[2], 0, 255), int(clamp(a, 0, 1) * 255 + 0.5)); }
Rgb mixRgb(const Rgb& a, const Rgb& b, double t) { return { int(a[0] + (b[0] - a[0]) * t), int(a[1] + (b[1] - a[1]) * t), int(a[2] + (b[2] - a[2]) * t) }; }
uint32_t hashOf(int a, int b)
{
    int32_t h = int32_t(uint32_t(int32_t(uint32_t(a) * 73856093u) ^ int32_t(uint32_t(b) * 19349663u)) * 0x5bd1e995u);
    h ^= int32_t(uint32_t(h) >> 13);
    return uint32_t(h);
}
const char* const LINE_COLORS[12] = { "#dc241f", "#0019a8", "#00843d", "#e87722", "#9b0058", "#00a3a1", "#7a4fb5", "#c9a100", "#e86aa6", "#4a7fb5", "#7b5a36", "#6a9a1f" };
inline uint64_t edgeKey(int a, int b) { return a < b ? uint64_t(a) * 4096 + uint64_t(b) : uint64_t(b) * 4096 + uint64_t(a); }
}

// ── as rotas: uma árvore que liga a galáxia, quebrada em linhas compridas ──
const Canvas::Lanes& Canvas::lanes()
{
    const Galaxy* gal = doc->gal.get();
    int alive = 0;
    for (const auto& s : gal->systems) if (!s->gone) alive++;
    Lanes& L = m_lanes;
    if (L.gal == gal && L.n == alive) return L;
    L = Lanes();
    L.gal = gal; L.n = alive;
    for (const auto& s : gal->systems) if (!s->gone) L.list.push_back(s.get());
    const int n = alive;
    if (!n) return L;
    std::vector<char> inTree(size_t(n), 0);
    std::vector<double> best(size_t(n), 1e300);
    std::vector<int> from(size_t(n), -1);
    std::vector<std::vector<int>> adj(static_cast<size_t>(n));
    best[0] = 0;
    for (int k = 0; k < n; k++) {
        int u = -1;
        double m = 1e300;
        for (int i = 0; i < n; i++) if (!inTree[size_t(i)] && best[size_t(i)] < m) { m = best[size_t(i)]; u = i; }
        if (u < 0) break;
        inTree[size_t(u)] = 1;
        if (from[size_t(u)] >= 0) { L.edges.push_back({ from[size_t(u)], u, std::sqrt(m) }); adj[size_t(u)].push_back(from[size_t(u)]); adj[size_t(from[size_t(u)])].push_back(u); }
        for (int i = 0; i < n; i++) if (!inTree[size_t(i)]) {
            const double dx = L.list[size_t(i)]->x - L.list[size_t(u)]->x, dy = L.list[size_t(i)]->y - L.list[size_t(u)]->y, d = dx * dx + dy * dy;
            if (d < best[size_t(i)]) { best[size_t(i)] = d; from[size_t(i)] = u; }
        }
    }
    // Atalhos: dois vizinhos perto que a árvore obriga a dar uma volta enorme.
    {
        std::vector<double> sorted;
        for (const auto& e : L.edges) sorted.push_back(e.len);
        std::sort(sorted.begin(), sorted.end());
        L.med = sorted.empty() ? 0.1 : sorted[sorted.size() >> 1];
    }
    std::vector<std::vector<int>> full = adj;
    const auto farApart = [&](int a, int b) {
        std::set<int> seen { a };
        std::vector<int> now { a };
        for (int h = 0; h < 5; h++) {
            std::vector<int> next;
            for (int u : now) for (int w : full[size_t(u)]) { if (w == b) return false; if (seen.insert(w).second) next.push_back(w); }
            now = std::move(next);
        }
        return true;
    };
    for (int i = 0; i < n && double(L.shorts.size()) < n / 7.0; i++) {
        int bj = -1;
        double bd = L.med * 1.5;
        for (int j = i + 1; j < n; j++) {
            const double d = std::hypot(L.list[size_t(i)]->x - L.list[size_t(j)]->x, L.list[size_t(i)]->y - L.list[size_t(j)]->y);
            if (d < bd && farApart(i, j)) { bd = d; bj = j; }
        }
        if (bj >= 0) { L.shorts.push_back({ i, bj, bd }); full[size_t(i)].push_back(bj); full[size_t(bj)].push_back(i); }
    }
    // Quebra a árvore: a maior linha primeiro, depois a maior do que sobrou.
    std::set<uint64_t> used;
    const auto lenOf = [&](int a, int b) { return std::hypot(L.list[size_t(a)]->x - L.list[size_t(b)]->x, L.list[size_t(a)]->y - L.list[size_t(b)]->y); };
    struct Walk { int far; std::vector<double> dist; std::vector<int> prev; };
    const auto walk = [&](int start) {
        Walk W { start, std::vector<double>(size_t(n), -1), std::vector<int>(size_t(n), -1) };
        W.dist[size_t(start)] = 0;
        std::vector<int> stack { start };
        while (!stack.empty()) {
            const int u = stack.back(); stack.pop_back();
            for (int w : adj[size_t(u)]) if (!used.count(edgeKey(u, w)) && W.dist[size_t(w)] < 0) {
                W.dist[size_t(w)] = W.dist[size_t(u)] + lenOf(u, w); W.prev[size_t(w)] = u; stack.push_back(w);
                if (W.dist[size_t(w)] > W.dist[size_t(W.far)]) W.far = w;
            }
        }
        return W;
    };
    for (int round = 0; round < 60; round++) {
        std::vector<char> seen(size_t(n), 0);
        std::vector<int> top;
        double topLen = 0;
        for (int i = 0; i < n; i++) {
            if (seen[size_t(i)]) continue;
            bool open = false;
            for (int w : adj[size_t(i)]) if (!used.count(edgeKey(i, w))) { open = true; break; }
            if (!open) continue;
            const Walk A = walk(i);
            for (int u = 0; u < n; u++) if (A.dist[size_t(u)] >= 0) seen[size_t(u)] = 1;
            const Walk B = walk(A.far);
            const double len = B.dist[size_t(B.far)];
            if (len > topLen) { topLen = len; top.clear(); for (int u = B.far; u >= 0; u = B.prev[size_t(u)]) top.push_back(u); }
        }
        if (top.size() < 3) break;
        for (size_t i = 0; i + 1 < top.size(); i++) used.insert(edgeKey(top[i], top[i + 1]));
        L.lines.push_back(std::move(top));
    }
    for (const auto& e : L.edges) if (!used.count(edgeKey(e.a, e.b))) L.twigs.push_back(e);
    for (const auto& l : L.lines) for (int i : l) L.lineCount[L.list[size_t(i)]->id]++;
    // Constelações: as ligações curtas, em grupos pequenos.
    {
        std::vector<int> dad(static_cast<size_t>(n)), size(static_cast<size_t>(n), 1);
        for (int i = 0; i < n; i++) dad[size_t(i)] = i;
        const auto root = [&](int i) { while (dad[size_t(i)] != i) { dad[size_t(i)] = dad[size_t(dad[size_t(i)])]; i = dad[size_t(i)]; } return i; };
        std::vector<Lanes::Edge> sorted = L.edges;
        std::stable_sort(sorted.begin(), sorted.end(), [](const Lanes::Edge& x, const Lanes::Edge& y) { return x.len < y.len; });
        for (const auto& e : sorted) {
            const int a = root(e.a), b = root(e.b);
            if (a != b && size[size_t(a)] + size[size_t(b)] <= 6) { dad[size_t(a)] = b; size[size_t(b)] += size[size_t(a)]; L.stars.push_back(e); }
        }
    }
    return L;
}

QString Canvas::spaceRegionKey() const
{
    QString k = QString::number(doc->gal->systems.size());
    for (const auto& r : doc->regions) {
        int sum = int(r->systems.size());
        for (int id : r->systems) sum += id * 31;
        k += QLatin1Char('|') + r->color + QLatin1Char(':') + QString::number(sum);
    }
    return k;
}

void Canvas::paintSpace(QPainter& g, double a, double, double cc, double, double, double, double zoomK)
{
    g.save();
    switch (m_sk->paint) {
    case SpacePaint::Holo: paintHolo(g, zoomK); break;
    case SpacePaint::Routes: paintRoutes(g, zoomK); break;
    case SpacePaint::Domains: paintDomains(g, zoomK); break;
    case SpacePaint::Gravity: paintGravity(g, a, cc, zoomK); break;
    case SpacePaint::Harmony: paintHarmony(g, zoomK); break;
    case SpacePaint::Thermal: paintThermal(g); break;
    case SpacePaint::Plate: paintPlate(g); break;
    default: break;
    }
    g.restore();
}

// ── Holograma ──
void Canvas::paintHolo(QPainter& g, double zoomK)
{
    const int W = width(), H = height();
    const double k = 1 - smooth(5, 18, zoomK), ly = doc->gal->ly / 2;
    const auto P = [&](double x, double y) { double ox, oy; proj(x, y, 0, ox, oy); return QPointF(ox, oy); };
    const auto circle = [&](double R) { QPolygonF c; for (int i = 0; i <= 120; i++) { const double t = (i * TAU) / 120; c << P(std::cos(t) * R, std::sin(t) * R); } return c; };
    if (k > 0.02) {
        QFont f(QStringLiteral("IBM Plex Mono")); f.setPixelSize(10); g.setFont(f);
        for (const double R : { 0.25, 0.5, 0.75, 1.0 }) {
            QPen pen(m_sk->inkC((R == 1 ? 0.5 : 0.22) * k)); pen.setWidthF(0.8);
            g.setPen(pen); g.setBrush(Qt::NoBrush);
            g.drawPolyline(circle(R));
            const QPointF q = P(0, -R);
            g.setPen(m_sk->inkC(0.6 * k));
            const QString t = QCoreApplication::translate("galaxy::Canvas", "%1 a.l.").arg(QLocale().toString(std::round((R * ly) / 100) * 100, 'f', 0));
            g.drawText(QRectF(q.x() - 60, q.y() + 3, 120, 14), Qt::AlignCenter, t);
        }
        QPen tick(m_sk->inkC(0.5 * k)); tick.setWidthF(0.8);
        g.setPen(tick);
        QVector<QLineF> ticks;
        for (int i = 0; i < 72; i++) { const double t = (i * TAU) / 72, R1 = i % 6 == 0 ? 1.04 : 1.018; ticks.push_back(QLineF(P(std::cos(t), std::sin(t)), P(std::cos(t) * R1, std::sin(t) * R1))); }
        g.drawLines(ticks);
        QPen cross(m_sk->inkC(0.1 * k)); cross.setWidthF(0.8);
        g.setPen(cross);
        for (const double t : { 0.0, PI_D / 2, PI_D / 4, -PI_D / 4 }) g.drawLine(P(-std::cos(t), -std::sin(t)), P(std::cos(t), std::sin(t)));
    }
    // Linhas de varredura, presas à tela.
    g.setRenderHint(QPainter::Antialiasing, false);
    for (int y = 0; y < H; y += 3) g.fillRect(QRectF(0, y, W, 1), QColor(0, 4, 10, 51));
    g.setRenderHint(QPainter::Antialiasing, true);
    const double R = std::hypot(W, H) * 0.58, t0 = (std::min(W, H) * 0.4) / R;
    QRadialGradient gr(W / 2.0, H / 2.0, R);
    gr.setColorAt(0, QColor(0, 10, 20, 0)); gr.setColorAt(t0, QColor(0, 10, 20, 0)); gr.setColorAt(1, QColor(0, 10, 20, 140));
    g.fillRect(QRectF(0, 0, W, H), gr);
}

// ── Rotas ──
void Canvas::paintRoutes(QPainter& g, double zoomK)
{
    const double k = 1 - smooth(9, 30, zoomK);
    if (k < 0.02) return;
    const Lanes& L = lanes();
    const double wide = std::min(5.5, 2.8 + zoomK * 0.45);
    const auto P = [&](double x, double y, double h) { double ox, oy; proj(x, y, h, ox, oy); return QPointF(ox, oy); };
    // Cada trecho anda reto e depois em 45°, como num mapa de metrô. A estação fica onde o sistema está.
    const auto leg = [&](QPolygonF& out, const System& a, const System& b) {
        const double dx = b.x - a.x, dy = b.y - a.y, m = std::min(std::fabs(dx), std::fabs(dy));
        const double ux = (dx > 0 ? 1 : dx < 0 ? -1 : 0) * m, uy = (dy > 0 ? 1 : dy < 0 ? -1 : 0) * m;
        const bool first = hashOf(a.id, b.id) & 1;
        const double kx = first ? a.x + ux : b.x - ux, ky = first ? a.y + uy : b.y - uy;
        out << P(kx, ky, (a.layH + b.layH) / 2) << P(b.x, b.y, b.layH);
    };
    const auto seg = [&](const Lanes::Edge& e) { const System& a = *L.list[size_t(e.a)]; const System& b = *L.list[size_t(e.b)]; QPolygonF q; q << P(a.x, a.y, a.layH); leg(q, a, b); return q; };
    g.setOpacity(k);
    g.setBrush(Qt::NoBrush);
    {
        QPen pen(m_sk->inkC(0.28)); pen.setWidthF(1); pen.setDashPattern({ 3, 4 }); pen.setJoinStyle(Qt::RoundJoin); pen.setCapStyle(Qt::RoundCap);
        g.setPen(pen);
        for (const auto& e : L.shorts) g.drawPolyline(seg(e));
    }
    {
        QPen pen(m_sk->inkC(0.4)); pen.setWidthF(1.5); pen.setJoinStyle(Qt::RoundJoin); pen.setCapStyle(Qt::RoundCap);
        g.setPen(pen);
        for (const auto& e : L.twigs) g.drawPolyline(seg(e));
    }
    for (size_t n = 0; n < L.lines.size(); n++) {
        const std::vector<int>& path = L.lines[n];
        QPolygonF line;
        const System& s0 = *L.list[size_t(path[0])];
        line << P(s0.x, s0.y, s0.layH);
        for (size_t i = 0; i + 1 < path.size(); i++) leg(line, *L.list[size_t(path[i])], *L.list[size_t(path[i + 1])]);
        QPen edge(m_sk->paperC()); edge.setWidthF(wide + 2.2); edge.setJoinStyle(Qt::RoundJoin); edge.setCapStyle(Qt::RoundCap);
        g.setPen(edge); g.drawPolyline(line);
        QPen pen(QColor(QString::fromLatin1(LINE_COLORS[n % 12]))); pen.setWidthF(wide); pen.setJoinStyle(Qt::RoundJoin); pen.setCapStyle(Qt::RoundCap);
        g.setPen(pen); g.drawPolyline(line);
    }
    g.setOpacity(1);
}

// ── Domínios ──
const Canvas::DomainCells& Canvas::domainCells()
{
    const QString key = spaceRegionKey();
    if (m_domain.key == key && !m_domain.img.isNull()) return m_domain;
    m_domainMasks.clear();
    const Lanes& L = lanes();
    const double E = EXT, cap = std::max(0.05, L.med * 2.7);
    const int S = 900, nb = int(std::ceil((2 * E) / cap));
    std::vector<std::vector<int>> cells(size_t(nb) * size_t(nb));
    const auto cellOf = [&](double x, double y) { return std::clamp(int(std::floor((y + E) / cap)), 0, nb - 1) * nb + std::clamp(int(std::floor((x + E) / cap)), 0, nb - 1); };
    for (size_t i = 0; i < L.list.size(); i++) cells[size_t(cellOf(L.list[i]->x, L.list[i]->y))].push_back(int(i));
    std::vector<int> owner(L.list.size(), -1);
    QHash<int, int> byId;
    for (size_t i = 0; i < L.list.size(); i++) byId.insert(L.list[i]->id, int(i));
    std::vector<Rgb> rgb;
    for (size_t ri = 0; ri < doc->regions.size(); ri++) {
        const Region& r = *doc->regions[ri];
        for (int id : r.systems) { auto it = byId.constFind(id); if (it != byId.constEnd()) owner[size_t(*it)] = int(ri); }
        const QColor c(r.color);
        rgb.push_back({ c.red(), c.green(), c.blue() });
    }
    std::vector<int> ids(size_t(S) * size_t(S), -1);
    const double caps = cap * cap;
    for (int j = 0; j < S; j++) {
        const double y = E - ((j + 0.5) / S) * 2 * E;
        const int cy = int(std::floor((y + E) / cap));
        for (int i = 0; i < S; i++) {
            const double x = -E + ((i + 0.5) / S) * 2 * E;
            const int cx = int(std::floor((x + E) / cap));
            double best = caps;
            int id = -1;
            for (int b = std::max(0, cy - 1); b <= std::min(nb - 1, cy + 1); b++) for (int a = std::max(0, cx - 1); a <= std::min(nb - 1, cx + 1); a++) {
                for (int q : cells[size_t(b * nb + a)]) {
                    const double dx = L.list[size_t(q)]->x - x, dy = L.list[size_t(q)]->y - y, dd = dx * dx + dy * dy;
                    if (dd < best) { best = dd; id = q; }
                }
            }
            ids[size_t(j * S + i)] = id;
        }
    }
    QImage img(S, S, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    const Rgb FREE { 40, 50, 68 };
    for (int j = 1; j < S - 1; j++) {
        QRgb* row = reinterpret_cast<QRgb*>(img.scanLine(j));
        for (int i = 1; i < S - 1; i++) {
            const int o = j * S + i, id = ids[size_t(o)];
            if (id < 0) continue;
            const int own = owner[size_t(id)], tone = int(double((hashOf(id, 11) & 15)) - 7) * 9 / 10;
            const Rgb b0 = own >= 0 ? mixRgb(rgb[size_t(own)], { 14, 18, 28 }, 0.28) : FREE;
            const Rgb base { b0[0] + tone, b0[1] + tone, b0[2] + tone };
            Rgb col = base;
            for (const int nx : { o - 1, o + 1, o - S, o + S, o - 2, o - 2 * S }) {
                if (nx < 0) continue;
                const int q = ids[size_t(nx)];
                if (q == id) continue;
                const int oq = q < 0 ? -2 : owner[size_t(q)];
                if (oq != own) {
                    if (own >= 0) { col = mixRgb(rgb[size_t(own)], { 255, 255, 255 }, 0.35); break; }
                    if (q < 0) col = mixRgb(FREE, { 120, 134, 160 }, 0.5);
                } else if (nx == o - 1 || nx == o - S) col = mixRgb(base, { 8, 11, 18 }, 0.82);
            }
            row[i] = qRgb(std::clamp(col[0], 0, 255), std::clamp(col[1], 0, 255), std::clamp(col[2], 0, 255));
        }
    }
    m_domain.key = key; m_domain.img = img; m_domain.ids = std::move(ids); m_domain.owner = std::move(owner); m_domain.S = S;
    return m_domain;
}
// As casas de um império, pra recortar a parte nítida do foco.
QImage Canvas::domainMask(const Region& r)
{
    const DomainCells& D = domainCells();
    auto it = m_domainMasks.constFind(r.key);
    if (it != m_domainMasks.constEnd()) return *it;
    int ri = -1;
    for (size_t i = 0; i < doc->regions.size(); i++) if (doc->regions[i].get() == &r) ri = int(i);
    QImage out(D.S, D.S, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    for (int j = 0; j < D.S; j++) {
        QRgb* row = reinterpret_cast<QRgb*>(out.scanLine(j));
        for (int i = 0; i < D.S; i++) {
            const int id = D.ids[size_t(j * D.S + i)];
            if (id >= 0 && D.owner[size_t(id)] == ri) row[i] = 0xffffffffu;
        }
    }
    m_domainMasks.insert(r.key, out);
    return out;
}
void Canvas::paintDomains(QPainter& g, double zoomK)
{
    const double k = 1 - smooth(6, 26, zoomK) * 0.92, E = EXT;
    double p0x, p0y, p1x, p1y, p2x, p2y;
    proj(-E, E, 0, p0x, p0y); proj(E, E, 0, p1x, p1y); proj(-E, -E, 0, p2x, p2y);
    const QImage& cv = domainCells().img;
    const double T = cv.width();
    g.save();
    g.setOpacity(k);
    g.setTransform(QTransform(m_dpr * (p1x - p0x) / T, m_dpr * (p1y - p0y) / T, m_dpr * (p2x - p0x) / T, m_dpr * (p2y - p0y) / T, m_dpr * p0x, m_dpr * p0y));
    g.drawImage(0, 0, cv);
    g.restore();
    const double kL = 1 - smooth(10, 30, zoomK);
    if (kL > 0.02) {
        const Lanes& L = lanes();
        g.setOpacity(kL);
        QPen pen(QColor(214, 224, 240, 77)); pen.setWidthF(0.9);
        g.setPen(pen);
        QVector<QLineF> lines;
        const auto add = [&](const Lanes::Edge& e) {
            const System& a = *L.list[size_t(e.a)]; const System& b = *L.list[size_t(e.b)];
            double ax, ay, bx, by;
            proj(a.x, a.y, a.layH, ax, ay); proj(b.x, b.y, b.layH, bx, by);
            lines.push_back(QLineF(ax, ay, bx, by));
        };
        for (const auto& e : L.edges) add(e);
        for (const auto& e : L.shorts) add(e);
        g.drawLines(lines);
        g.setOpacity(1);
    }
}

// ── Gravidade ──
// A malha é puxada por dois pesos: a massa espalhada dos braços e o poço de cada sistema.
void Canvas::paintGravity(QPainter& g, double a, double cc, double zoomK)
{
    const int W = width(), H = height();
    const double px = std::hypot(a, cc), target = 14;
    double st = 0.25;
    while (st * px > target * 1.5) st /= 2;
    while (st * px < target * 0.75) st *= 2;
    double cx, cy;
    toWorld(W / 2.0, H / 2.0, cx, cy);
    const double R = (std::hypot(W, H) / px) * (cam.tilt > 0.2 ? 1.2 : 0.62);
    const int N = std::min(85, int(std::ceil(R / st))), x0 = int(std::floor(cx / st + 0.5)) - N, y0 = int(std::floor(cy / st + 0.5)) - N, M = 2 * N + 1;
    const size_t count = size_t(M) * size_t(M);
    std::vector<float> DX(count, 0), DY(count, 0), DIP(count, 0), WELL(count, 0), T(count, 0);
    const Field& F = m_art.field;
    const double E = EXT, far_ = 1 - smooth(4, 16, zoomK);
    const auto at = [&](double x, double y) { return std::fabs(x) > E || std::fabs(y) > E ? 0.0 : sampleField(F, (x + E) / (2 * E), (E - y) / (2 * E)); };
    if (far_ > 0.01) {
        size_t o = 0;
        for (int j = 0; j < M; j++) for (int i = 0; i < M; i++, o++) {
            const double x = (x0 + i) * st, y = (y0 + j) * st, t = at(x, y), q = st * 1.5;
            T[o] = float(t);
            DX[o] = float((at(x + q, y) - at(x - q, y)) * st * 2.2 * far_); DY[o] = float((at(x, y + q) - at(x, y - q)) * st * 2.2 * far_);
            DIP[o] = float(std::pow(t, 0.8) * 0.2 * far_);
        }
    }
    for (const auto& sp : doc->gal->systems) {
        const System& s = *sp;
        if (s.gone) continue;
        const double sx = (s.x - x0 * st) / st, sy = (s.y - y0 * st) / st;
        if (sx < -8 || sy < -8 || sx > M + 8 || sy > M + 8) continue;
        const double m = s.isCore ? 3 : clamp(0.5 + 0.45 * std::log10(1 + (s.M != 0 ? s.M : 1) * 3), 0.4, 1.5);
        const double eps = std::max(st * 1.9, s.isCore ? 0.06 : std::min((s.layR != 0 ? s.layR : 0.02) * 0.32, 0.03));
        const int reach = std::min(40, int(std::ceil((eps * 4.5) / st)));
        for (int j = std::max(0, int(std::floor(sy)) - reach); j <= std::min(M - 1, int(std::ceil(sy)) + reach); j++) for (int i = std::max(0, int(std::floor(sx)) - reach); i <= std::min(M - 1, int(std::ceil(sx)) + reach); i++) {
            const double rx = (sx - i) * st, ry = (sy - j) * st, rr = rx * rx + ry * ry, q = 1 / (rr + eps * eps), fade = 1 - smooth(eps * 2.5, eps * 4.5, std::sqrt(rr));
            const double amp = m * eps * eps * q * fade;
            const size_t o = size_t(j) * size_t(M) + size_t(i);
            DX[o] += float((rx / eps) * amp * st * 1.2); DY[o] += float((ry / eps) * amp * st * 1.2);
            const double dip = (m * fade) / std::sqrt(1 + rr / (eps * eps));
            DIP[o] = std::max(DIP[o], float(dip * eps * 1.5)); WELL[o] = std::max(WELL[o], float(dip));
        }
    }
    std::vector<float> X(count), Y(count);
    std::vector<uint8_t> B(count);
    {
        const double lim = st * 0.62;
        size_t o = 0;
        for (int j = 0; j < M; j++) for (int i = 0; i < M; i++, o++) {
            double qx, qy;
            proj((x0 + i) * st + clamp(DX[o], -lim, lim), (y0 + j) * st + clamp(DY[o], -lim, lim), -std::min(double(DIP[o]), 0.34), qx, qy);
            X[o] = float(qx); Y[o] = float(qy);
            B[o] = uint8_t(std::min(4, int(std::floor(std::pow(double(T[o]), 0.8) * far_ * 4.6 + WELL[o] * 1.7))));
        }
    }
    const double lw[5] = { 0.5, 0.65, 0.8, 1, 1.2 }, al[5] = { 0.1, 0.24, 0.42, 0.66, 0.92 };
    QVector<QLineF> lines[5];
    {
        size_t o = 0;
        const size_t MM = size_t(M);
        for (int j = 0; j < M; j++) for (int i = 0; i < M; i++, o++) {
            if (i + 1 < M) lines[std::max(B[o], B[o + 1])].push_back(QLineF(X[o], Y[o], X[o + 1], Y[o + 1]));
            if (j + 1 < M) lines[std::max(B[o], B[o + MM])].push_back(QLineF(X[o], Y[o], X[o + MM], Y[o + MM]));
        }
    }
    for (int lv = 0; lv < 5; lv++) {
        const Rgb c = lv >= 3 ? mixRgb(m_sk->ink, { 255, 255, 255 }, lv == 4 ? 0.65 : 0.25) : m_sk->ink;
        QPen pen(cssOf(c, al[lv])); pen.setWidthF(lw[lv]);
        g.setPen(pen);
        g.drawLines(lines[lv]);
    }
}

// ── Harmonia ──
void Canvas::paintHarmony(QPainter& g, double zoomK)
{
    const double k = 1 - smooth(5, 18, zoomK);
    const auto P = [&](double x, double y) { double ox, oy; proj(x, y, 0, ox, oy); return QPointF(ox, oy); };
    const auto circle = [&](double R) { QPolygonF c; for (int i = 0; i <= 120; i++) { const double t = (i * TAU) / 120; c << P(std::cos(t) * R, std::sin(t) * R); } return c; };
    const auto spoke = [&](QVector<QLineF>& out, double t, double r0, double r1) { out.push_back(QLineF(P(std::cos(t) * r0, std::sin(t) * r0), P(std::cos(t) * r1, std::sin(t) * r1))); };
    g.setBrush(Qt::NoBrush);
    if (k > 0.02) {
        QPen thin(m_sk->inkC(0.16 * k)); thin.setWidthF(0.7);
        g.setPen(thin);
        for (const double R : { 0.25, 0.5, 0.75 }) g.drawPolyline(circle(R));
        QVector<QLineF> sp;
        for (int i = 0; i < 12; i++) spoke(sp, (i * TAU) / 12, 0.08, 1.06);
        g.drawLines(sp);
        // A borda graduada do atlas: dois aros e os graus entre eles.
        QPen ring(m_sk->inkC(0.8 * k)); ring.setWidthF(1.2);
        g.setPen(ring);
        g.drawPolyline(circle(1.06)); g.drawPolyline(circle(1.17));
        ring.setWidthF(0.6); g.setPen(ring);
        g.drawPolyline(circle(1.075));
        QPen tick(m_sk->inkC(0.65 * k)); tick.setWidthF(0.7);
        g.setPen(tick);
        QVector<QLineF> ticks;
        for (int i = 0; i < 360; i += 2) spoke(ticks, (i * TAU) / 360, 1.06, i % 30 == 0 ? 1.17 : i % 10 == 0 ? 1.1 : 1.075);
        g.drawLines(ticks);
        static const char* const ROMAN[12] = { "XII", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI" };
        QFont f(QStringLiteral("IM FELL English SC")); f.setPixelSize(int(clamp(zoomK * 15, 11, 18)));
        g.setFont(f);
        g.setPen(m_sk->inkC(0.92 * k));
        for (int i = 0; i < 12; i++) {
            const double t = ((i + 0.5) * TAU) / 12;
            const QPointF q = P(std::cos(t) * 1.135, std::sin(t) * 1.135);
            g.drawText(QRectF(q.x() - 30, q.y() - 12, 60, 24), Qt::AlignCenter, QString::fromLatin1(ROMAN[i]));
        }
    }
    // As constelações: os sistemas vizinhos, ligados por um traço fino.
    const double kc = 1 - smooth(7, 22, zoomK);
    if (kc > 0.02) {
        const Lanes& L = lanes();
        QPen pen(m_sk->inkC(0.5 * kc)); pen.setWidthF(0.75); pen.setDashPattern({ 2, 3.3 });
        g.setPen(pen);
        QVector<QLineF> lines;
        for (const auto& e : L.stars) {
            const System& a = *L.list[size_t(e.a)]; const System& b = *L.list[size_t(e.b)];
            double ax, ay, bx, by;
            proj(a.x, a.y, a.layH, ax, ay); proj(b.x, b.y, b.layH, bx, by);
            lines.push_back(QLineF(ax, ay, bx, by));
        }
        g.drawLines(lines);
    }
}

// ── Infravermelho: a régua da temperatura ──
void Canvas::paintThermal(QPainter& g)
{
    const double w = 170, x = width() / 2.0 - w / 2, y = 20;
    QLinearGradient gr(x, 0, x + w, 0);
    for (int i = 0; i <= 8; i++) gr.setColorAt(i / 8.0, cssOf(m_sk->gradAt(i / 8.0)));
    g.fillRect(QRectF(x, y, w, 7), gr);
    QPen edge(QColor(255, 255, 255, 102)); edge.setWidthF(0.7);
    g.setPen(edge); g.setBrush(Qt::NoBrush);
    g.drawRect(QRectF(x, y, w, 7));
    QFont f(QStringLiteral("IBM Plex Mono")); f.setPixelSize(10);
    g.setFont(f);
    g.setPen(QColor(255, 240, 220, 217));
    const QRectF row(x, y + 9, w, 14);
    g.drawText(row, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("−230 °C"));
    g.drawText(row, Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("+930 °C"));
    g.drawText(row, Qt::AlignHCenter | Qt::AlignVCenter, QCoreApplication::translate("galaxy::Canvas", "MUNDOS"));
}

// ── Chapa de vidro ──
void Canvas::paintPlate(QPainter& g)
{
    const int W = width(), H = height();
    // A numeração escrita à mão na beira da chapa e as marcas de registro.
    int alive = 0;
    for (const auto& s : doc->gal->systems) if (!s->gone) alive++;
    QFont f(QStringLiteral("Caveat")); f.setPixelSize(26);
    g.setFont(f);
    g.setPen(QColor(32, 44, 110, 219));
    g.drawText(QRectF(0, 12, W - 86, 36), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("galaxy::Canvas", "chapa nº %1").arg(doc->gal->seed));
    f.setPixelSize(19);
    g.setFont(f);
    g.drawText(QRectF(0, 44, W - 86, 28), Qt::AlignRight | Qt::AlignVCenter, QCoreApplication::translate("galaxy::Canvas", "%1 sistemas contados").arg(QLocale().toString(alive)));
    QPen pen(QColor(20, 22, 28, 140)); pen.setWidthF(1);
    g.setPen(pen);
    const double marks[4][2] = { { -1.1, 1.1 }, { 1.1, 1.1 }, { -1.1, -1.1 }, { 1.1, -1.1 } };
    for (const auto& m : marks) {
        double x, y;
        proj(m[0], m[1], 0, x, y);
        g.drawLine(QPointF(x - 9, y), QPointF(x + 9, y));
        g.drawLine(QPointF(x, y - 9), QPointF(x, y + 9));
    }
    const double R = std::hypot(W, H) * 0.58, t0 = (std::min(W, H) * 0.46) / R;
    QRadialGradient gr(W / 2.0, H / 2.0, R);
    gr.setColorAt(0, QColor(70, 74, 70, 0)); gr.setColorAt(t0, QColor(70, 74, 70, 0)); gr.setColorAt(1, QColor(70, 74, 70, 77));
    g.fillRect(QRectF(0, 0, W, H), gr);
}

// ── a estrela de longe, em cada vista ──
const QImage& Canvas::flare(const Rgb& c)
{
    const QString k = QStringLiteral("%1,%2,%3").arg(c[0]).arg(c[1]).arg(c[2]);
    auto it = m_flares.find(k);
    if (it != m_flares.end()) return *it;
    QImage f(64, 64, QImage::Format_ARGB32_Premultiplied);
    f.fill(Qt::transparent);
    {
        QPainter q(&f);
        QRadialGradient gr(32, 32, 32);
        gr.setColorAt(0, QColor(255, 255, 255, 255)); gr.setColorAt(0.16, cssOf(mixRgb(c, { 255, 255, 255 }, 0.6), 0.95)); gr.setColorAt(0.42, cssOf(c, 0.34)); gr.setColorAt(1, cssOf(c, 0));
        q.fillRect(f.rect(), gr);
    }
    return *m_flares.insert(k, f);
}
void Canvas::drawSpaceStar(QPainter& p, const System& s, double x, double y)
{
    const Star& st = s.stars[0];
    const StarKind kind = st.kind;
    const bool living = livingCount(s) > 0 && !s.isCore, hole = s.isCore || kind == StarKind::BuracoNegro;
    const double big = 1.3 + std::min(1.9, (st.radius != 0 ? st.radius : 1) * 0.42);
    const QColor ink = m_sk->inkC();
    const auto glowAt = [&](const Rgb& c, double R) {
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.drawImage(QRectF(x - R, y - R, R * 2, R * 2), flare(c));
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    };
    const auto halo = [&](const QColor& c, double w, double R) { QPen pen(c); pen.setWidthF(w); p.setPen(pen); p.setBrush(Qt::NoBrush); p.drawEllipse(QPointF(x, y), R, R); };
    const auto dot = [&](const QColor& c, double R) { p.setPen(Qt::NoPen); p.setBrush(c); p.drawEllipse(QPointF(x, y), R, R); };
    switch (m_sk->sign) {
    case StarSign::Holo: {
        glowAt(m_sk->ink, s.isCore ? 20 : 6 + big * 2);
        if (hole) halo(ink, 1, s.isCore ? 8 : 4.5);
        if (living) {
            const double q = 6.5, c = 2.6;
            QPen pen(m_sk->hzC()); pen.setWidthF(1);
            p.setPen(pen); p.setBrush(Qt::NoBrush);
            const double corners[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
            for (const auto& k : corners) { QPolygonF l; l << QPointF(x + k[0] * q, y + k[1] * (q - c)) << QPointF(x + k[0] * q, y + k[1] * q) << QPointF(x + k[0] * (q - c), y + k[1] * q); p.drawPolyline(l); }
        }
        break;
    }
    case StarSign::Station: {
        const int lines = lanes().lineCount.value(s.id);
        const double R = s.isCore ? 7 : lines > 1 ? 4.6 : 3;
        QPen pen(ink); pen.setWidthF(lines > 1 || s.isCore ? 2 : 1.5);
        p.setPen(pen); p.setBrush(Qt::white);
        p.drawEllipse(QPointF(x, y), R, R);
        if (living) dot(m_sk->hzC(), std::max(1.4, R * 0.5));
        if (hole) dot(ink, R * 0.55);
        break;
    }
    case StarSign::Domain:
        dot(QColor(8, 10, 16, 230), big + 1.4);
        dot(living ? QColor(255, 255, 255) : QColor(0xc6, 0xd0, 0xe0), s.isCore ? 4 : living ? 2.3 : 1.5);
        if (living) halo(Qt::white, 1, 4.8);
        break;
    case StarSign::Grav:
        glowAt(mixRgb(m_sk->ink, { 255, 255, 255 }, 0.4), s.isCore ? 16 : 4 + big * 1.6);
        if (living) halo(m_sk->hzC(), 1, 5.2);
        break;
    case StarSign::Engraved: {
        const double R = s.isCore ? 9 : 2.6 + big * 1.15;
        QPolygonF poly;
        for (int i = 0; i < 16; i++) {
            const double t = (i * TAU) / 16 - TAU / 4, q = i % 4 == 0 ? R : i % 2 == 0 ? R * 0.62 : R * 0.26;
            poly << QPointF(x + std::cos(t) * q, y + std::sin(t) * q);
        }
        p.setPen(Qt::NoPen); p.setBrush(ink);
        p.drawPolygon(poly);
        if (hole) halo(ink, 1, R * 1.2);
        if (living) halo(m_sk->hzC(), 1, R + 2.8);
        break;
    }
    case StarSign::Thermal: {
        const double t = clamp(std::log((st.temp != 0 ? st.temp : 5000) / 2500) / std::log(16.0), 0.45, 1);
        glowAt(m_sk->gradAt(t), s.isCore ? 22 : 5 + big * 2.2);
        if (living) halo(m_sk->hzC(), 1, 5.6);
        break;
    }
    case StarSign::Xray: {
        const bool violent = hole || kind == StarKind::Neutrons, hot = kind == StarKind::O || kind == StarKind::B;
        if (violent || hot) {
            glowAt(hot ? Rgb { 80, 150, 255 } : Rgb { 120, 190, 255 }, s.isCore ? 46 : violent ? 20 : 10);
            if (violent) {
                // O jato sai pelos dois polos, sempre na mesma direção do sistema.
                const double a = (double(s.seed % 628)) / 100, L = s.isCore ? 74 : 26, dx = std::cos(a) * L, dy = std::sin(a) * L;
                QLinearGradient gr(x - dx, y - dy, x + dx, y + dy);
                gr.setColorAt(0, QColor(120, 190, 255, 0)); gr.setColorAt(0.5, QColor(235, 248, 255, 242)); gr.setColorAt(1, QColor(120, 190, 255, 0));
                QPen pen(QBrush(gr), s.isCore ? 2.4 : 1.3);
                p.setCompositionMode(QPainter::CompositionMode_Plus);
                p.setPen(pen);
                p.drawLine(QPointF(x - dx, y - dy), QPointF(x + dx, y + dy));
                p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            }
        } else p.fillRect(QRectF(x - 0.8, y - 0.8, 1.6, 1.6), QColor(112, 158, 214, 128));
        if (living) halo(QColor(190, 226, 255, 140), 0.9, 4.6);
        break;
    }
    case StarSign::Plate:
        dot(QColor(0x12, 0x14, 0x18), s.isCore ? 6 : big * 0.82);
        if (living) {
            // Círculo a tinta, feito à mão: nunca fecha redondo.
            const uint32_t h = hashOf(s.id, 7);
            const double R = 6 + (h & 3), off = ((h >> 4) & 255) / 40.0;
            QPolygonF c;
            for (int i = 0; i <= 26; i++) {
                const double t = off + (i / 24.0) * TAU, w = R * (1 + 0.1 * std::sin(t * 2 + off) + 0.05 * std::sin(t * 5));
                c << QPointF(x + std::cos(t) * w, y + std::sin(t) * w * 0.92);
            }
            QPen pen(QColor(32, 44, 110, 204)); pen.setWidthF(1.1);
            p.setPen(pen); p.setBrush(Qt::NoBrush);
            p.drawPolyline(c);
        }
        break;
    default: break;
    }
}

} // namespace galaxy
