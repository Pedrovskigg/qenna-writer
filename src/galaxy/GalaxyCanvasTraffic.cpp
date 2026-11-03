#include "GalaxyCanvas.h"
#include "GalaxyModels.h"

#include <QLinearGradient>

#include <algorithm>

namespace galaxy {

namespace {
double hash01(double a0, double b0)
{
    const uint32_t a = uint32_t(int64_t(a0)), b = uint32_t(int64_t(b0));
    uint32_t h = (a ^ 0x9e3779b1u) * 0x85ebca6bu ^ (b + 0x165667b1u) * 0xc2b2ae35u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 13;
    return h / 4294967296.0;
}
// Tamanho relativo ao sistema e tempo de viagem de cada classe de nave.
struct ShipClass { double size, T; std::vector<int> kinds; };
const ShipClass SHIP_CLASSES[3] = { { 0.0085, 20, { 0, 0, 4 } }, { 0.014, 32, { 1, 3, 4 } }, { 0.028, 48, { 1, 2, 2, 3 } } };
// Onde a nave pode parar: mundo habitável, lua, mundo construído e estação. Gigante gasoso e lava, nunca, mesmo com gente.
inline bool noLanding(Type t) { return t == Type::Gigante || t == Type::Quente || t == Type::GeloG || t == Type::Subnetuno || t == Type::Lava || t == Type::Vulcanica; }
inline bool canLand(const Body& b) { return !noLanding(b.type) && (isLiving(b.type) || isArtificial(b.type) || b.look.pop > 0); }
Rgb colorOf(const QString& s, double k = 1) { const QColor c(s); return { int(c.red() * k), int(c.green() * k), int(c.blue() * k) }; }
QColor rgbC(const Rgb& c, double a = 1) { return QColor(std::clamp(c[0], 0, 255), std::clamp(c[1], 0, 255), std::clamp(c[2], 0, 255), int(clamp(a, 0, 1) * 255 + 0.5)); }
} // namespace

std::vector<Build*> Canvas::portalsOf(const System& s) const
{
    std::vector<Build*> out;
    for (Build* b : doc->buildsIn(s)) if (b->type == "portal" && b->host < 0) out.push_back(b);
    return out;
}
std::vector<Canvas::Stop> Canvas::stopsOf(System& s) const
{
    std::vector<Stop> out;
    for (Body* p : planetsOf(s)) {
        if (canLand(*p)) { Stop st; st.p = p; out.push_back(st); }
        // A lua vazia é parada de pouco movimento: o grosso das naves vai pra onde tem gente.
        for (const auto& m : p->moons) if (!noLanding(m->type)) { Stop st; st.p = m.get(); st.host = p; st.minor = !canLand(*m); out.push_back(st); }
    }
    for (Build* b : doc->buildsIn(s)) if (!structOf(b->type)->mega && b->type != "portal" && b->type != "sat" && b->type != "farol") { Stop st; st.b = b; out.push_back(st); }
    return out;
}
int Canvas::trafficScore(System& s) const
{
    if (s.isCore) return 0;
    const auto stops = stopsOf(s);
    const auto gates = portalsOf(s);
    if (stops.empty() && gates.empty()) return 0;
    int n = livingCount(s) * 2 + int(doc->buildsIn(s).size()) * 2 + (doc->touched(keyS(s)) ? 1 : 0) + (doc->regionOfSys(s) ? 1 : 0);
    for (Body* p : planetsOf(s)) if (p->look.pop > 0) n++;
    return n;
}
void Canvas::trafficModel()
{
    const QString key = QStringLiteral("%1,%2,%3,%4,%5,%6").arg(doc->builds.size()).arg(doc->gal->systems.size()).arg(trafficLevel).arg(doc->regions.size()).arg(doc->userNames.size()).arg(doc->docs.size());
    if (m_tr.key == key) return;
    m_tr.key = key; m_tr.routes.clear(); m_tr.sys.clear();
    std::vector<System*> nodes;
    for (const auto& s : doc->gal->systems) if (!s->gone && trafficScore(*s) > 0) nodes.push_back(s.get());
    QSet<qint64> seen;
    std::vector<Region*> regionsOf(nodes.size());
    for (size_t i = 0; i < nodes.size(); i++) regionsOf[i] = doc->regionOfSys(*nodes[i]);
    std::vector<std::pair<double, size_t>> near;
    for (size_t si = 0; si < nodes.size(); si++) {
        System* s = nodes[si];
        Region* ra = regionsOf[si];
        near.clear();
        for (size_t oi = 0; oi < nodes.size(); oi++) if (oi != si) near.push_back({ std::hypot(nodes[oi]->x - s->x, nodes[oi]->y - s->y) * (regionsOf[oi] && regionsOf[oi] == ra ? 0.5 : 1), oi });
        const size_t top = std::min<size_t>(2, near.size());
        std::partial_sort(near.begin(), near.begin() + long(top), near.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (size_t i = 0; i < top; i++) {
            if (near[i].first > 0.5) continue;
            System* o = nodes[near[i].second];
            const qint64 id = s->id < o->id ? (qint64(s->id) << 32 | o->id) : (qint64(o->id) << 32 | s->id);
            if (seen.contains(id)) continue;
            seen.insert(id);
            Region* rb = regionsOf[near[i].second];
            m_tr.routes.push_back({ s, o, s->id * 977 + o->id, ra && ra == rb ? ra->color : QString(), !portalsOf(*s).empty() && !portalsOf(*o).empty() });
        }
    }
}
// As naves de um sistema: cada uma emenda viagens entre os pontos com movimento.
const std::vector<Canvas::Ship>& Canvas::shipsOf(System& s)
{
    const int key = int(doc->buildsIn(s).size() + planetsOf(s).size());
    auto it = m_tr.sys.find(s.id);
    if (it != m_tr.sys.end() && it->second.first == key) return it->second.second;
    const int score = !stopsOf(s).empty() ? trafficScore(s) : 0;
    static const double LEVEL[4] = { 0, 0.55, 1, 1.8 };
    const int n = score ? std::min(70, int(std::floor((14 + std::sqrt(double(score)) * 8) * LEVEL[std::clamp(trafficLevel, 0, 3)] + 0.5))) : 0;
    std::vector<Ship> ships;
    for (int i = 0; i < n; i++) {
        // Muita nave miúda, um pouco de média, e de vez em quando uma grande no meio.
        const double q = hash01(s.seed, i), v = hash01(double(s.seed) + 3, i);
        const ShipClass& cl = q < 0.62 ? SHIP_CLASSES[0] : q < 0.9 ? SHIP_CLASSES[1] : SHIP_CLASSES[2];
        const int kind = cl.kinds[size_t(std::floor(v * double(cl.kinds.size())))];
        ships.push_back({ i, kind, cl.size * (0.8 + hash01(double(s.seed) + 7, i) * 0.45), cl.T * (0.8 + hash01(s.seed, i + 40) * 0.5), hash01(s.seed, i + 80) * 1000 });
    }
    auto& e = m_tr.sys[s.id];
    e.first = key;
    e.second = std::move(ships);
    return e.second;
}
void Canvas::drawJump(QPainter& p, double px, double py, double dx, double dy, double k, const std::array<int, 3>& c, double size)
{
    p.save();
    if (m_sk) {
        // No papel, o salto é uma estrelinha de pena.
        const double L = size * (2 + k * 6);
        p.setOpacity(p.opacity() * std::min(1.0, k * 1.6));
        QPen pen(m_sk->mono ? m_sk->inkC() : m_sk->selC()); pen.setWidthF(1);
        p.setPen(pen);
        p.drawLine(QPointF(px - dx * L, py - dy * L), QPointF(px + dx * L, py + dy * L));
        p.drawLine(QPointF(px + dy * L * 0.35, py - dx * L * 0.35), QPointF(px - dy * L * 0.35, py + dx * L * 0.35));
        p.restore();
        return;
    }
    // O clarão do salto: um risco esticado na direção da viagem e uma explosão de luz.
    p.setCompositionMode(QPainter::CompositionMode_Plus);
    const double L = size * (2 + k * 10), A = std::min(1.0, k * 1.6);
    QLinearGradient q(px - dx * L, py - dy * L, px + dx * L, py + dy * L);
    q.setColorAt(0, rgbC(c, 0)); q.setColorAt(0.5, QColor(255, 255, 255, int(A * 255))); q.setColorAt(1, rgbC(c, 0));
    QPen pen(QBrush(q), std::max(1.0, size * 0.22));
    p.setPen(pen);
    p.drawLine(QPointF(px - dx * L, py - dy * L), QPointF(px + dx * L, py + dy * L));
    const double R = size * (1 + k * 2.2);
    p.setOpacity(A);
    p.drawImage(QRectF(px - R, py - R, R * 2, R * 2), glow(c));
    p.restore();
}
void Canvas::drawTraffic(QPainter& p, Region* fr, double z, double cT, double zoomK)
{
    trafficModel();
    drawLanes(p, fr, z, zoomK);
    for (System* s : m_openSystems) if (s->layR * z > 60) drawSystemShips(p, *s, z, cT);
}
// De longe: as rotas entre os sistemas, com pulsos de luz, que são as naves em salto.
void Canvas::drawLanes(QPainter& p, Region* fr, double, double zoomK)
{
    const double A = 1 - smooth(9, 26, zoomK);
    if (A < 0.02) return;
    const double simT = doc->simT;
    const int W = width(), H = height();
    p.save();
    for (const Route& R : m_tr.routes) {
        if (fr && !(fr->systems.contains(R.a->id) && fr->systems.contains(R.b->id))) continue;
        double ax, ay, bx, by;
        proj(R.a->x, R.a->y, R.a->layH, ax, ay);
        proj(R.b->x, R.b->y, R.b->layH, bx, by);
        if (std::max(ax, bx) < -40 || std::min(ax, bx) > W + 40 || std::max(ay, by) < -40 || std::min(ay, by) > H + 40) continue;
        const Rgb c = m_sk ? (m_sk->mono || R.color.isEmpty() ? m_sk->ink : colorOf(R.color, 0.7)) : !R.color.isEmpty() ? colorOf(R.color) : Rgb { 150, 200, 255 };
        // Um contorno escuro por baixo faz a rota aparecer até em cima dos braços da galáxia.
        if (!m_sk) {
            p.setOpacity(A * 0.5);
            QPen under(QColor(3, 5, 10)); under.setWidthF(R.gate ? 3.4 : 2.6); under.setCapStyle(Qt::RoundCap);
            p.setPen(under);
            p.drawLine(QPointF(ax, ay), QPointF(bx, by));
        }
        p.setOpacity(A * (R.gate ? 0.9 : 0.62));
        QPen pen(rgbC(c)); pen.setWidthF(R.gate ? 1.5 : 1); pen.setCapStyle(Qt::RoundCap);
        if (!R.gate) pen.setDashPattern({ 1.5, 5 });
        p.setPen(pen);
        p.drawLine(QPointF(ax, ay), QPointF(bx, by));
        const double dx = bx - ax, dy = by - ay;
        double len = std::hypot(dx, dy);
        if (len == 0) len = 1;
        const double ux = dx / len, uy = dy / len;
        for (int i = 0; i < trafficLevel; i++) {
            // Cada nave passa uma parte do ciclo atravessando a rota; no resto, está em outro lugar.
            const double T = 26 + hash01(R.id, i) * 26, u0 = std::fmod(simT / T + hash01(R.id, i + 9), 1.0), span = 0.34;
            const bool back = i % 2 == 1;
            if (u0 > span) continue;
            const double k = u0 / span, ease = k * k * (3 - 2 * k), u = back ? 1 - ease : ease, px = ax + dx * u, py = ay + dy * u, dir = back ? -1 : 1;
            const double tail = std::min(len * 0.3, 46.0);
            p.save();
            if (!m_sk) p.setCompositionMode(QPainter::CompositionMode_Plus);
            p.setOpacity(A);
            QLinearGradient q(px, py, px - ux * dir * tail, py - uy * dir * tail);
            q.setColorAt(0, m_sk ? rgbC(c, 1) : QColor(255, 255, 255, 242)); q.setColorAt(0.35, rgbC(c, 0.6)); q.setColorAt(1, rgbC(c, 0));
            QPen tp(QBrush(q), 2.2); tp.setCapStyle(Qt::RoundCap);
            p.setPen(tp);
            p.drawLine(QPointF(px, py), QPointF(px - ux * dir * tail, py - uy * dir * tail));
            if (m_sk) { p.setPen(Qt::NoPen); p.setBrush(rgbC(c, 1)); p.drawEllipse(QPointF(px, py), 2, 2); }
            else p.drawImage(QRectF(px - 6, py - 6, 12, 12), glow(c));
            p.restore();
            p.setOpacity(A);
            if (k < 0.1) drawJump(p, back ? bx : ax, back ? by : ay, ux, uy, 1 - k / 0.1, c, 4);
            if (k > 0.9) drawJump(p, back ? ax : bx, back ? ay : by, ux, uy, (k - 0.9) / 0.1, c, 4);
        }
    }
    p.restore();
}
// De perto: naves de verdade indo de um ponto a outro, entrando e saindo em salto.
void Canvas::drawSystemShips(QPainter& p, System& s, double z, double)
{
    const std::vector<Ship>& ships = shipsOf(s);
    if (ships.empty()) return;
    const std::vector<Stop> stops = stopsOf(s);
    const std::vector<Build*> gates = portalsOf(s);
    const Region* rg = doc->regionOfSys(s);
    const Rgb tint = m_sk ? (m_sk->mono || !rg ? m_sk->ink : colorOf(rg->color, 0.7)) : rg ? colorOf(rg->color) : Rgb { 150, 205, 255 };
    for (Build* g : gates) g->flash *= 0.92;
    const double A0 = smooth(60, 120, s.layR * z), simT = doc->simT;
    std::vector<const Stop*> mainS, minorS;
    for (const Stop& st : stops) (st.minor ? minorS : mainS).push_back(&st);
    const auto planets = planetsOf(s);
    // A lua só aparece de perto: até lá, a nave dela também não, pra não parecer que foi pro planeta.
    const auto seenK = [&](const Stop* st) { return st && st->host ? smooth(9, 22, std::max(2.2, st->host->layR * z)) : 1.0; };
    struct Way { const Stop* st = nullptr; double x = 0, y = 0; Build* gate = nullptr; bool jump = false; };
    // O ponto de salto: a saída pela borda do sistema, ou o portal, se tiver.
    const auto jumpOf = [&](double leg, int i) {
        Way w;
        if (!gates.empty()) {
            Build* g = gates[size_t(std::floor(hash01(leg, i + 3) * double(gates.size())))];
            buildPos(*g, &s, nullptr, w.x, w.y);
            w.gate = g;
            return w;
        }
        const double a = hash01(leg + 5, i) * TAU;
        w.x = s.x + std::cos(a) * s.layR * 1.02; w.y = s.y + std::sin(a) * s.layR * 1.02; w.jump = true;
        return w;
    };
    const auto pick2 = [&](double leg, int i) {
        const double h = hash01(leg, i + int(s.seed % 97));
        if (h < 0.3 || stops.empty()) return jumpOf(leg, i);
        const auto& pool = !minorS.empty() && (mainS.empty() || hash01(leg, i + 11) < 0.15) ? minorS : mainS;
        Way w;
        w.st = pool[size_t(std::floor(hash01(leg, i + 7) * double(pool.size()))) % pool.size()];
        return w;
    };
    const auto posOf = [&](const Way& w, double& x, double& y, double& r) {
        r = 0;
        if (!w.st) { x = w.x; y = w.y; return; }
        if (w.st->p) { x = w.st->p->layX; y = w.st->p->layY; r = w.st->p->layR; return; }
        Body* host = nullptr;
        if (w.st->b->host >= 0) for (Body* q : planets) if ((q->uid ? q->uid : q->idx) == w.st->b->host) host = q;
        buildPos(*w.st->b, &s, host, x, y);
        r = buildRadiusOf(*w.st->b, &s, host);
    };
    const int W = width(), H = height();
    p.save();
    for (const Ship& sh : ships) {
        const double tt = (simT + sh.ph) / sh.T, leg = std::floor(tt), f = tt - leg;
        const Way from = pick2(leg, sh.i), to = pick2(leg + 1, sh.i);
        if (from.st && from.st == to.st) continue;
        const double A = A0 * std::min(seenK(from.st), seenK(to.st));
        if (A <= 0) continue;
        // Parte do ciclo a nave fica parada na doca (não aparece); o resto é viagem.
        const double f0 = 0.12, f1 = 0.88;
        if (f < f0 || f > f1) continue;
        double ax, ay, ar, bx, by, br;
        posOf(from, ax, ay, ar);
        posOf(to, bx, by, br);
        const auto pos = [&](double q, double& ox, double& oy) {
            const double e = q * q * (3 - 2 * q), dx = bx - ax, dy = by - ay;
            double d = std::hypot(dx, dy);
            if (d == 0) d = 1;
            const double bend = d * 0.16 * (sh.i % 2 ? 1 : -1) * std::sin(PI_D * q);
            const double sx = ax + (dx / d) * ar, sy = ay + (dy / d) * ar, ex = bx - (dx / d) * br, ey = by - (dy / d) * br;
            proj(sx + (ex - sx) * e - (dy / d) * bend, sy + (ey - sy) * e + (dx / d) * bend, s.layH, ox, oy);
        };
        const double q = (f - f0) / (f1 - f0);
        double px, py, qx, qy;
        pos(q, px, py);
        pos(std::min(1.0, q + 0.012), qx, qy);
        const double vx = qx - px, vy = qy - py;
        double vn = std::hypot(vx, vy);
        if (vn == 0) vn = 1;
        const double ux = vx / vn, uy = vy / vn;
        if (px < -30 || py < -30 || px > W + 30 || py > H + 30) continue;
        const double size = std::max(1.6, std::min(40.0, s.layR * z * sh.size));
        // O rastro do motor.
        double tx, ty;
        pos(std::max(0.0, q - (size < 6 ? 0.025 : 0.05)), tx, ty);
        p.save();
        p.setOpacity(A);
        if (!m_sk) p.setCompositionMode(QPainter::CompositionMode_Plus);
        QLinearGradient gr(px, py, tx, ty);
        gr.setColorAt(0, rgbC(tint, 0.75)); gr.setColorAt(1, rgbC(tint, 0));
        p.setPen(QPen(QBrush(gr), std::max(0.7, size * 0.16)));
        p.drawLine(QPointF(px, py), QPointF(tx, ty));
        p.restore();
        if (size < 3.2) {
            const double w = size < 2.2 ? 1 : 1.6;
            p.setOpacity(A);
            p.fillRect(QRectF(px - w / 2, py - w / 2, w, w), m_sk ? m_sk->inkC() : QColor(0xee, 0xf4, 0xfb));
            p.setOpacity(1);
        } else {
            // A nave é modelo também: aponta pra onde vai e acompanha a inclinação do mapa.
            const double want = size * 2 * std::min(m_dpr, 1.5);
            int D = 128;
            for (int v : { 24, 32, 48, 64, 96, 128 }) if (v >= want) { D = v; break; }
            const double P = 0.55 + cam.tilt * 0.7, heading = std::atan2(-uy / std::max(0.2, std::cos(clampPitch(P))), ux);
            p.save();
            p.setOpacity(A);
            p.drawImage(QRectF(px - size, py - size, size * 2, size * 2), shipImage(sh.kind, D, heading, P));
            if (!m_sk) p.setCompositionMode(QPainter::CompositionMode_Plus);
            p.setPen(Qt::NoPen); p.setBrush(rgbC(tint, 0.95));
            const double er = std::max(0.8, size * 0.1);
            p.drawEllipse(QPointF(px - ux * size * 0.9, py - uy * size * 0.9), er, er);
            p.restore();
        }
        // Chegou ou saiu por um salto.
        const Rgb jumpC = from.gate || to.gate ? Rgb { 130, 210, 255 } : tint;
        p.setOpacity(1);
        if ((from.jump || from.gate) && q < 0.09) { drawJump(p, px, py, ux, uy, 1 - q / 0.09, jumpC, size * 1.1); if (from.gate) from.gate->flash = std::max(from.gate->flash, 1 - q / 0.09); }
        if ((to.jump || to.gate) && q > 0.91) { drawJump(p, px, py, ux, uy, (q - 0.91) / 0.09, jumpC, size * 1.1); if (to.gate) to.gate->flash = std::max(to.gate->flash, (q - 0.91) / 0.09); }
    }
    p.restore();
}

} // namespace galaxy
