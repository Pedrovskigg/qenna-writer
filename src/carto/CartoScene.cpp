#include "CartoScene.h"
#include "CartoProf.h"

#include "CartoEdit.h"

#include <QFontMetricsF>
#include <QHash>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <memory>
#include <map>
#include <unordered_map>

namespace carto {

namespace {
const RGB kNeutral = { 150, 146, 140 };   // cidade em terra sem dono
double artHeight(const QString& kind)
{
    if (kind == QLatin1String("castle")) return 26;
    if (kind == QLatin1String("tower")) return 10;
    if (kind == QLatin1String("lighthouse")) return 12;
    if (kind == QLatin1String("windmill")) return 9;
    if (kind == QLatin1String("temple")) return 6;
    if (kind == QLatin1String("ruins")) return 4;
    if (const ArtSpec* s = artSpec(kind)) return s->h;
    return 6;
}
inline bool isFull(const SceneState& st) { return st.mode == QLatin1String("full"); }
inline bool isAtlas(const SceneState& st) { return st.mode == QLatin1String("atlas"); }
inline bool isInk(const SceneState& st) { return st.mode == QLatin1String("ink"); }
QColor rgba(double r, double g, double b, double a = 1)
{
    return QColor(static_cast<int>(jsRound(clampd(r, 0, 255))), static_cast<int>(jsRound(clampd(g, 0, 255))),
                  static_cast<int>(jsRound(clampd(b, 0, 255))), static_cast<int>(jsRound(clampd(a, 0, 1) * 255)));
}
QColor rgbOf(const RGB& c) { return rgba(c[0], c[1], c[2]); }
RGB realmCol(const Info& info, int r) { return r >= 0 && r < static_cast<int>(info.realms.size()) ? info.realms[r].color : kNeutral; }

// fonte dos nomes: IM Fell (versalete pros nomes de terra, itálico pros de água)
QFont fell(bool caps, bool italic, double px)
{
    QFont f;
    f.setFamilies({ caps && !italic ? QStringLiteral("IM FELL English SC") : QStringLiteral("IM FELL English"), QStringLiteral("Georgia"), QStringLiteral("serif") });
    f.setItalic(italic);
    f.setPointSizeF(std::max(1.0, px * 0.75));
    f.setHintingPreference(QFont::PreferNoHinting);
    return f;
}
QPen strokePen(const QColor& c, double w)
{
    QPen pen(c, w);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    return pen;
}
// fonte, medidas e o desenho de cada letra guardados (montar fonte e contornar
// letra a cada quadro custava mais que o mapa inteiro)
struct FontEntry {
    QFont font;
    QFontMetricsF fm;
    double mid;                                  // baseline "middle" do canvas
    QHash<QChar, double> adv;
    QHash<QChar, QPainterPath> glyph;            // letra centrada na origem
    QHash<QString, QPainterPath> text;           // texto inteiro, começando na origem
    explicit FontEntry(const QFont& f) : font(f), fm(f), mid((fm.ascent() - fm.descent()) / 2) {}
    double advance(QChar c)
    {
        auto it = adv.find(c);
        if (it != adv.end()) return it.value();
        return adv.insert(c, fm.horizontalAdvance(c)).value();
    }
    const QPainterPath& glyphPath(QChar c)
    {
        auto it = glyph.find(c);
        if (it != glyph.end()) return it.value();
        QPainterPath gp;
        gp.addText(QPointF(-advance(c) / 2, mid), font, QString(c));
        return glyph.insert(c, gp).value();
    }
    const QPainterPath& textPath(const QString& t)
    {
        auto it = text.find(t);
        if (it != text.end()) return it.value();
        QPainterPath tp;
        tp.addText(QPointF(0, mid), font, t);
        return text.insert(t, tp).value();
    }
};
FontEntry& fontAt(bool caps, bool italic, double px, double ls = 0)
{
    static std::unordered_map<qint64, std::unique_ptr<FontEntry>> cache;
    const qint64 key = (static_cast<qint64>(jsRound(px * 10)) << 24) ^ (static_cast<qint64>(jsRound(ls * 10)) << 4) ^ (caps ? 1 : 0) ^ (italic ? 2 : 0);
    auto it = cache.find(key);
    if (it != cache.end()) return *it->second;
    if (cache.size() > 400) cache.clear();
    QFont f = fell(caps, italic, px);
    if (ls) f.setLetterSpacing(QFont::AbsoluteSpacing, ls);
    return *cache.emplace(key, std::make_unique<FontEntry>(f)).first->second;
}
// Contornar letra a cada quadro custava mais que o mapa inteiro: cada letra (ou
// nome de cidade) vira dois carimbos guardados, o halo e o miolo, desenhados em
// duas passadas (o halo de uma letra não come a vizinha). O tamanho anda em
// degraus de 4% e o carimbo estica o resto, pra o zoom não refazer tudo.
struct TextSprite { QImage halo, fill; QRectF rect; };
double quantSize(double fs) { return std::pow(1.04, jsRound(std::log(std::max(1.0, fs)) / std::log(1.04))); }
// família = tudo menos o tamanho; cada família guarda os tamanhos já carimbados
struct SpriteCache { QHash<QString, std::map<int, TextSprite>> fam; int count = 0; };
SpriteCache& spriteCache() { static SpriteCache c; return c; }
// o carimbo desse tamanho; com `near`, aceita o tamanho guardado mais perto (e diz qual)
const TextSprite* findSprite(const QString& fam, double fq, double* near)
{
    SpriteCache& C = spriteCache();
    auto it = C.fam.find(fam);
    if (it == C.fam.end() || it->empty()) return nullptr;
    const int k = static_cast<int>(jsRound(fq * 1000));
    auto e = it->find(k);
    if (e != it->end()) { if (near) *near = fq; return &e->second; }
    if (!near) return nullptr;
    auto hi = it->lower_bound(k);
    auto best = hi;
    if (hi == it->end()) best = std::prev(hi);
    else if (hi != it->begin()) {
        const auto lo = std::prev(hi);
        best = std::fabs(std::log(hi->first / double(k))) < std::fabs(std::log(lo->first / double(k))) ? hi : lo;
    }
    *near = best->first / 1000.0;
    return &best->second;
}
const TextSprite& textSprite(const QString& fam, double fq, const QPainterPath& path, const QColor& stroke, double lw, bool knock, double knockW1, double knockW2,
                             const QColor& fill, double dpr)
{
    SpriteCache& C = spriteCache();
    const int key = static_cast<int>(jsRound(fq * 1000));
    {
        auto it = C.fam.find(fam);
        if (it != C.fam.end()) { auto e = it->find(key); if (e != it->end()) return e->second; }
    }
    if (C.count > 4000) { C.fam.clear(); C.count = 0; }
    const double pad = (knock ? std::max(knockW1, knockW2) : lw) / 2 + 2;
    const QRectF r = path.boundingRect().adjusted(-pad, -pad, pad, pad);
    CARTO_PROF("nomes: carimbo novo");
    TextSprite sp;
    sp.rect = r;
    const QSize px(std::max(1, static_cast<int>(std::ceil(r.width() * dpr))), std::max(1, static_cast<int>(std::ceil(r.height() * dpr))));
    sp.halo = QImage(px, QImage::Format_ARGB32_Premultiplied);
    sp.fill = QImage(px, QImage::Format_ARGB32_Premultiplied);
    sp.halo.fill(Qt::transparent);
    sp.fill.fill(Qt::transparent);
    {
        QPainter q(&sp.halo);
        q.setRenderHint(QPainter::Antialiasing);
        q.scale(dpr, dpr);
        q.translate(-r.topLeft());
        if (knock) {
            // Tinta: anel largo e suave (o traço do mapa vai sumindo), depois o miolo de papel puro
            q.strokePath(path, strokePen(QColor(240, 231, 208, 153), knockW1));
            q.strokePath(path, strokePen(QColor(240, 231, 208), knockW2));
        } else q.strokePath(path, strokePen(stroke, lw));
    }
    {
        QPainter q(&sp.fill);
        q.setRenderHint(QPainter::Antialiasing);
        q.scale(dpr, dpr);
        q.translate(-r.topLeft());
        q.fillPath(path, fill);
    }
    C.count++;
    return C.fam[fam][key] = sp;
}
QString colorKey(const QColor& c) { return QString::number(c.rgba(), 16); }
}

// ── chão ──
std::optional<Ground> Scene::ground(const SceneState& st, double qx, double qy, double zTop) const
{
    if (!st.g || !st.aux || st.aux->lift.empty() || !st.doc) return std::nullopt;
    const Geom& m = *st.g;
    const World& w = st.doc->world;
    const double s = m.step;
    const double ca = std::cos(st.ang), sa = std::sin(st.ang), dx = qx - w.W / 2.0, dy = qy - w.H / 2.0;
    const double px = ca * dx - sa * dy, py = sa * dx + ca * dy;
    const int u = static_cast<int>(jsRound(px / s + m.cu)), v = static_cast<int>(jsRound(py / s + m.cv));
    if (u < 0 || v < 0 || u >= m.W || v >= m.H) return std::nullopt;
    const double Lv = st.aux->lift[static_cast<size_t>(v) * m.W + u];
    const int top = static_cast<int>(std::floor(v - Lv));
    const int d = top >= 0 ? st.aux->depth[static_cast<size_t>(top) * m.W + u] : -1;
    bool vis = d <= v + 2;
    if (!vis && zTop > 0) {
        const double co = std::cos(st.pitch * M_PI / 180);
        const int tt = static_cast<int>(std::floor(v - Lv - co * zTop / (m.sq * s)));
        const int dt = tt >= 0 ? st.aux->depth[static_cast<size_t>(tt) * m.W + u] : -1;
        vis = dt <= v + 2;
    }
    const double L = Lv * s;
    return Ground{ px, py, L, vis, st.panX + st.zoom * px, st.panY + st.zoom * m.sq * (py - L) };
}

spr::Cam Scene::cam(const SceneState& st, double L) const
{
    spr::Cam c;
    const double th = st.pitch * M_PI / 180;
    c.ang = st.ang; c.co = std::cos(th); c.si = std::sin(th);
    const double det = st.zoom / st.fitZoom;
    c.edges = det > 1.6; c.detail = det > 1.35;
    const double ca = std::cos(st.ang), sa = std::sin(st.ang), sq = st.g ? st.g->sq : 1, co = c.co;
    const double cx = st.doc->world.W / 2.0, cy = st.doc->world.H / 2.0, zoom = st.zoom, panX = st.panX, panY = st.panY;
    c.proj = [=](double x, double y, double z) {
        const double dx = x - cx, dy = y - cy;
        const double px = ca * dx - sa * dy, py = sa * dx + ca * dy;
        return QPointF(panX + zoom * px, panY + zoom * (sq * (py - L) - co * z));
    };
    return c;
}

spr::Cam Scene::camAt(const SceneState& st, double L, double ox, double oy) const
{
    const double k = worldScale(st.doc->world);
    if (k == 1) return cam(st, L);
    spr::Cam c = cam(st, L);
    const double det = st.zoom * k / st.fitZoom;
    c.edges = det > 1.6; c.detail = det > 1.35;
    const double ca = std::cos(st.ang), sa = std::sin(st.ang), sq = st.g ? st.g->sq : 1, co = c.co;
    const double cx = st.doc->world.W / 2.0, cy = st.doc->world.H / 2.0, zoom = st.zoom, panX = st.panX, panY = st.panY;
    c.proj = [=](double x, double y, double z) {
        const double dx = ox + (x - ox) * k - cx, dy = oy + (y - oy) * k - cy;
        const double px = ca * dx - sa * dy, py = sa * dx + ca * dy;
        return QPointF(panX + zoom * px, panY + zoom * (sq * (py - L) - co * z * k));
    };
    return c;
}

// ── a lista de tudo que fica de pé, em ordem de profundidade ──
const std::vector<Path>& Scene::userRoadBridges(const SceneState& st)
{
    const Info* info = st.doc->info.get();
    const QString key = info ? QStringLiteral("%1|%2|%3").arg(st.doc->ver).arg(st.doc->infoVer).arg(info->user.ver) : QString();
    if (key != m_rbKey) {
        m_rbKey = key;
        m_roadBridges = info ? roadBridges(st.doc->world, info->user) : std::vector<Path>();
    }
    return m_roadBridges;
}

const std::vector<Scene::Item>& Scene::items(const SceneState& st)
{
    CARTO_PROF("cena: lista de itens");
    Doc& doc = *st.doc;
    World& w = doc.world;
    const std::vector<Flora>& flora = doc.ensureFlora();
    Info* info = st.mode != QLatin1String("terrain") ? doc.info.get() : nullptr;
    const UserLayer* U = info ? &info->user : nullptr;
    const bool gen = !U || U->showGen;
    const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7").arg(st.ang, 0, 'g', 12).arg(doc.ver).arg(st.mode).arg(info ? doc.infoVer : -1)
                            .arg(U ? QStringLiteral("%1:%2").arg(U->ver).arg(U->showGen) : QString()).arg(worldScale(w)).arg(doc.floraVer);
    if (key == m_itemsKey) return m_items;
    m_itemsKey = key;
    m_items.clear();
    // em vista com informação, abre clareira nas cidades e ao longo das estradas
    std::vector<uint8_t> blocked;
    const int G = 3, gw = (w.W + G - 1) / G, gh = (w.H + G - 1) / G;
    if (info) {
        blocked.assign(static_cast<size_t>(gw) * gh, 0);
        auto mark = [&](double x, double y, int r) {
            const int bx = static_cast<int>(std::floor(x / G)), by = static_cast<int>(std::floor(y / G));
            for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
                const int gx = bx + dx, gy = by + dy;
                if (gx >= 0 && gy >= 0 && gx < gw && gy < gh) blocked[static_cast<size_t>(gy) * gw + gx] = 1;
            }
        };
        if (gen) {
            for (const Path& rd : info->roads) for (const Pt& p : rd) mark(p[0], p[1], 1);
            for (const Town& t : info->towns) mark(t.x, t.y, t.capital ? 7 : 4);
            for (const Field& f : info->fields) mark(f.x, f.y, static_cast<int>(std::ceil(std::max(f.w, f.d) / 2 / G)) + 1);
        }
        const double kk = worldScale(w);
        for (const House& h : U->houses) mark(h.x, h.y, 1);
        for (const Art& a : U->arts)
            mark(a.x, a.y, a.kind == QLatin1String("castle") ? static_cast<int>(std::ceil((spr::kCastle[std::max(0, std::min(3, a.size))].R + 2) * kk / G))
                           : (a.kind == QLatin1String("temple") || a.kind == QLatin1String("ruins")) ? static_cast<int>(std::ceil(3 * kk))
                           : artSpec(a.kind) ? static_cast<int>(std::ceil((artSpec(a.kind)->r + 1) * kk / G)) : 1);
        for (const Field& f : U->fields) mark(f.x, f.y, static_cast<int>(std::ceil(std::max(f.w, f.d) / 2 / G)) + 1);
        for (const Wall& wl : U->walls) for (const Pt& p : wl.pts) mark(p[0], p[1], 1);
        for (const Road& rd : U->roads) for (size_t i = 0; i < rd.pts.size(); i += 2) mark(rd.pts[i][0], rd.pts[i][1], rd.kind == QLatin1String("trail") ? 0 : 1);
    }
    const double ca = std::cos(st.ang), sa = std::sin(st.ang), cx = w.W / 2.0, cy = w.H / 2.0;
    auto depthOf = [&](double x, double y) { return sa * (x - cx) + ca * (y - cy); };
    for (size_t i = 0; i < flora.size(); i++) {
        const Flora& f = flora[i];
        if (!blocked.empty() && blocked[static_cast<size_t>(std::floor(f.y / G)) * gw + static_cast<size_t>(std::floor(f.x / G))]) continue;
        m_items.push_back({ 0, static_cast<int>(i), 0, depthOf(f.x, f.y) });
    }
    if (info && isFull(st)) {
        for (size_t i = 0; i < U->houses.size(); i++) {
            const House& h = U->houses[i];
            if (h.snowy < 0) { const ClimateSample c = climateAt(w, h.x, h.y); h.snowy = c.valid && c.id == 0 ? 1 : 0; }
            m_items.push_back({ 1, static_cast<int>(i), 1, depthOf(h.x, h.y) });
        }
        for (size_t i = 0; i < U->arts.size(); i++) m_items.push_back({ 6, static_cast<int>(i), 0, depthOf(U->arts[i].x, U->arts[i].y) });
        for (size_t wi = 0; wi < U->walls.size(); wi++) {
            const Path& p = U->walls[wi].pts;
            for (size_t i = 0; i + 1 < p.size(); i++) m_items.push_back({ 7, static_cast<int>(wi), static_cast<int>(i), depthOf((p[i][0] + p[i + 1][0]) / 2, (p[i][1] + p[i + 1][1]) / 2) });
            for (size_t i = 0; i < p.size(); i += 6) m_items.push_back({ 8, static_cast<int>(wi), static_cast<int>(i), depthOf(p[i][0], p[i][1]) + 0.01 });
            m_items.push_back({ 8, static_cast<int>(wi), static_cast<int>(p.size() - 1), depthOf(p.back()[0], p.back()[1]) + 0.01 });
        }
        for (size_t i = 0; i < U->bridges.size(); i++) { const Pt& mid = U->bridges[i][U->bridges[i].size() / 2]; m_items.push_back({ 3, static_cast<int>(i), 0, depthOf(mid[0], mid[1]) }); }
        const std::vector<Path>& rb = userRoadBridges(st);
        for (size_t i = 0; i < rb.size(); i++) { const Pt& mid = rb[i][rb[i].size() / 2]; m_items.push_back({ 3, static_cast<int>(i), 1, depthOf(mid[0], mid[1]) }); }
    }
    if (info && isFull(st) && gen) {
        for (size_t i = 0; i < info->buildings.size(); i++) {
            const Building& b = info->buildings[i];
            if (b.snowy < 0) { const ClimateSample c = climateAt(w, b.x, b.y); b.snowy = c.valid && c.id == 0 ? 1 : 0; }
            m_items.push_back({ b.castle ? 2 : 1, static_cast<int>(i), 0, depthOf(b.x, b.y) });
        }
        for (size_t i = 0; i < info->bridges.size(); i++) { const Pt& mid = info->bridges[i][info->bridges[i].size() / 2]; m_items.push_back({ 3, static_cast<int>(i), 2, depthOf(mid[0], mid[1]) }); }
        for (size_t i = 0; i < info->docks.size(); i++) { const Dock& d = info->docks[i]; m_items.push_back({ 4, static_cast<int>(i), 0, depthOf((d.x0 + d.x1) / 2, (d.y0 + d.y1) / 2) }); }
        for (size_t i = 0; i < info->boats.size(); i++) m_items.push_back({ 5, static_cast<int>(i), 0, depthOf(info->boats[i].x, info->boats[i].y) });
    }
    std::stable_sort(m_items.begin(), m_items.end(), [](const Item& a, const Item& b) { return a.d < b.d; });
    return m_items;
}

// ── plantações (chão, antes das estradas) ──
void Scene::drawFields(QPainter& p, const SceneState& st)
{
    CARTO_PROF("cena: plantações");
    if (!st.info() || isInk(st)) return;
    const Info& info = *st.doc->info;
    const UserLayer& U = info.user;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    auto one = [&](const Field& f) {
        const auto g = ground(st, f.x, f.y);
        if (!g || !g->vis) return;
        if (st.clip.isValid()) {
            const double m = std::max(f.w, f.d) * st.zoom + 16;
            if (!st.clip.adjusted(-m, -m, m, m).contains(g->X, g->Y)) return;
        }
        spr::drawField(p, f, cam(st, g->L));      // o tamanho da plantação já nasce na escala
    };
    if (U.showGen) for (const Field& f : info.fields) one(f);
    for (const Field& f : U.fields) one(f);
    p.restore();
}

// ── estradas: quebram onde o relevo da frente as esconde ──
// Largura no mundo (w, px) com um mínimo na tela (min): de longe viram símbolo,
// de perto viram estrada de verdade (sulco no meio, calçamento na estrada real).
namespace {
struct RoadSty { double w, min; QColor edge, fill, aEdge, aFill; };
const RoadSty& roadSty(const QString& kind)
{
    static const RoadSty trail{ 0.55, 1.0, QColor(40, 30, 20, 61), QColor(202, 178, 134), QColor(244, 236, 214, 179), QColor(150, 92, 60) };
    static const RoadSty road{ 0.95, 1.5, QColor(30, 22, 14, 107), QColor(214, 190, 146), QColor(244, 236, 214, 217), QColor(132, 64, 42) };
    static const RoadSty royal{ 1.4, 2.1, QColor(52, 46, 40, 158), QColor(208, 202, 190), QColor(248, 242, 228, 235), QColor(104, 46, 34) };
    return kind == QLatin1String("trail") ? trail : kind == QLatin1String("royal") ? royal : road;
}
}

void Scene::drawAtlasWalls(QPainter& p, const SceneState& st, const UserLayer& U)
{
    const double k = clampd(st.zoom / st.fitZoom, 0.8, 2.4);
    for (const Wall& wl : U.walls) {
        QPainterPath path;
        bool started = false;
        for (const Pt& q : wl.pts) {
            const auto g = ground(st, q[0], q[1]);
            if (!g) continue;
            started ? path.lineTo(g->X, g->Y) : path.moveTo(g->X, g->Y);
            started = true;
        }
        QPen pen(QColor(60, 50, 40, 230), 2.6 * k);
        pen.setDashPattern({ 2.2 / 2.6, 1.6 / 2.6 });
        p.strokePath(path, pen);
    }
}

struct Scene::RoadPlan {
    QString key;
    struct Run { QPainterPath path; int n = 0; const RoadSty* s; QString kind; double inner; QRectF bb; };
    std::vector<Run> runs;      // em coordenadas da tela sem o arrasto
};

void Scene::drawRoads(QPainter& p, const SceneState& st)
{
    CARTO_PROF("cena: estradas");
    if (!st.info()) return;
    const Info& info = *st.doc->info;
    const UserLayer& U = info.user;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    if (isAtlas(st) || isInk(st)) drawAtlasWalls(p, st, U);
    const bool atlas = isAtlas(st), inkM = isInk(st), flat = atlas || inkM;
    const double kz = clampd(st.zoom / st.fitZoom, 0.8, 2.4);
    const double ks = worldScale(st.doc->world);
    const bool near = st.zoom * ks / st.fitZoom > 1.35;
    // o traçado na tela só muda com a vista (o arrasto só desloca): os azulejos reaproveitam
    RoadPlan local;
    RoadPlan* plan = &local;
    const QString& rkey = st.roadKey.isEmpty() ? st.planKey : st.roadKey;
    if (!rkey.isEmpty() && m_roadPlan && m_roadPlan->key == rkey) plan = m_roadPlan.get();
    else {
        if (!rkey.isEmpty()) { m_roadPlan = std::make_shared<RoadPlan>(); plan = m_roadPlan.get(); plan->key = rkey; }
        struct R { const Path* pts; QString kind; };
        std::vector<R> all;
        if (U.showGen) for (const Path& rd : info.roads) all.push_back({ &rd, QStringLiteral("road") });
        for (const Road& rd : U.roads) all.push_back({ &rd.pts, rd.kind });
        for (const R& rd : all) {
            const RoadSty& S0 = roadSty(rd.kind);
            const double inner = std::max(S0.min * kz, st.zoom * ks * S0.w);
            RoadPlan::Run* cur = nullptr;
            for (const Pt& q : *rd.pts) {
                const auto g = ground(st, q[0], q[1], 4);     // folga: atrás de crista baixinha a estrada não some (atrás de serra, some)
                if (!g || !g->vis) { cur = nullptr; continue; }
                if (!cur) { plan->runs.push_back({ QPainterPath(), 0, &S0, rd.kind, inner, QRectF() }); cur = &plan->runs.back(); }
                const double X = g->X - st.panX, Y = g->Y - st.panY;
                if (cur->n++) cur->path.lineTo(X, Y); else cur->path.moveTo(X, Y);
            }
        }
        for (RoadPlan::Run& r : plan->runs) r.bb = r.path.boundingRect();
    }
    if (plan->runs.empty()) { p.restore(); return; }
    p.translate(st.panX, st.panY);
    const QRectF clipA = st.clip.isValid() ? st.clip.translated(-st.panX, -st.panY) : QRectF();
    std::vector<const RoadPlan::Run*> runs;
    for (const RoadPlan::Run& r : plan->runs) {
        const double m = r.inner * 1.6 + 8 * kz;
        if (clipA.isValid() && !r.bb.adjusted(-m, -m, m, m).intersects(clipA)) continue;
        runs.push_back(&r);
    }
    auto mk = [](const QColor& c, double w) { QPen pen(c, w); pen.setCapStyle(Qt::RoundCap); pen.setJoinStyle(Qt::RoundJoin); return pen; };
    // borda de todas primeiro, depois o miolo: cruzamento sai limpo
    for (const RoadPlan::Run* rp : runs) {
        const RoadPlan::Run& r = *rp;
        if (r.n < 2) continue;
        const double lw = inkM ? (r.kind == QLatin1String("royal") ? 5.2 : 3.4) * kz : r.inner + std::max(1.6 * std::min(kz, 1.4), r.inner * 0.35);
        p.strokePath(r.path, mk(inkM ? QColor(240, 231, 208, 217) : atlas ? r.s->aEdge : r.s->edge, lw));
    }
    for (const RoadPlan::Run* rp : runs) {
        const RoadPlan::Run& r = *rp;
        if (r.n < 2) continue;
        const bool trail = r.kind == QLatin1String("trail"), royal = r.kind == QLatin1String("royal");
        const double lw = inkM ? (royal ? 3.2 : trail ? 0.9 : 1.2) * kz : atlas ? (royal ? 2.4 : trail ? 1.1 : 1.5) * kz : r.inner;
        QPen pen = mk(inkM ? QColor(40, 33, 28) : atlas ? r.s->aFill : r.s->fill, lw);
        // trilha é sempre tracejada; a estrada, só de longe (o símbolo de sempre); na Tinta, traço de pena
        QVector<qreal> dash;
        if (inkM) { if (trail) dash = { 1 * kz, 2.6 * kz }; else if (!royal) dash = { 5 * kz, 3 * kz }; }
        else if (trail) dash = { 2.4 * kz, 2.2 * kz };
        else if (!royal && !near && !atlas) dash = { 5 * kz, 3 * kz };
        if (!dash.isEmpty()) { for (qreal& d : dash) d /= std::max(0.01, lw); pen.setDashPattern(dash); pen.setCapStyle(Qt::RoundCap); }
        p.strokePath(r.path, pen);
        if (inkM && royal) p.strokePath(r.path, mk(QColor(240, 231, 208), 1.3 * kz));   // estrada real: linha dupla
    }
    if (near && !flat) {
        for (const RoadPlan::Run* rp : runs) {
            const RoadPlan::Run& r = *rp;
            if (r.n < 2 || r.kind == QLatin1String("trail")) continue;
            if (r.kind == QLatin1String("royal")) p.strokePath(r.path, mk(QColor(250, 246, 236, 82), r.inner * 0.42));   // calçada abaulada: o meio mais claro
            else p.strokePath(r.path, mk(QColor(150, 118, 80, 128), std::max(0.6, r.inner * 0.22)));             // sulco das carroças no meio
        }
    }
    p.restore();
}

// ── sprites ──
// Onde cada coisa de pé cai na tela (sem o arrasto), em células de 128 px: o azulejo
// pega só o que encosta nele, na mesma ordem de profundidade da lista.
struct Scene::SpriteIndex { QString key; QHash<qint64, std::vector<int>> cells; };
namespace { constexpr int kCell = 128; }

const Scene::SpriteIndex& Scene::spriteIndex(const SceneState& st, const std::vector<Item>& its)
{
    const QString key = st.planKey + QLatin1Char('#') + m_itemsKey;
    if (m_sprIdx && m_sprIdx->key == key) return *m_sprIdx;
    CARTO_PROF("sprites: índice");
    m_sprIdx = std::make_shared<SpriteIndex>();
    SpriteIndex& ix = *m_sprIdx;
    ix.key = key;
    Doc& doc = *st.doc;
    const std::vector<Flora>& flora = doc.ensureFlora();
    const Info* info = st.info() ? doc.info.get() : nullptr;
    const double ks = worldScale(doc.world), k = clampd(st.zoom / st.fitZoom, 0.8, 1.8);
    const double big = (40 * ks + 12) * st.zoom + 24;     // castelo colossal com bandeira cabe aqui
    int pos = 0;
    auto put = [&](double x0, double y0, double x1, double y1) {
        x0 -= st.panX; x1 -= st.panX; y0 -= st.panY; y1 -= st.panY;
        for (int gy = static_cast<int>(std::floor(y0 / kCell)); gy <= static_cast<int>(std::floor(y1 / kCell)); gy++)
            for (int gx = static_cast<int>(std::floor(x0 / kCell)); gx <= static_cast<int>(std::floor(x1 / kCell)); gx++)
                ix.cells[(static_cast<qint64>(gx) << 32) ^ static_cast<quint32>(gy)].push_back(pos);
    };
    auto around = [&](const std::optional<Ground>& g, double r) { if (g) put(g->X - r, g->Y - 1.6 * r, g->X + r, g->Y + r); };
    auto span = [&](std::initializer_list<std::optional<Ground>> gs, double r) {
        double x0 = INFINITY, y0 = INFINITY, x1 = -INFINITY, y1 = -INFINITY;
        for (const auto& g : gs) if (g) { x0 = std::min(x0, g->X); y0 = std::min(y0, g->Y); x1 = std::max(x1, g->X); y1 = std::max(y1, g->Y); }
        if (x0 <= x1) put(x0 - r, y0 - 1.6 * r, x1 + r, y1 + r);
    };
    for (; pos < static_cast<int>(its.size()); pos++) {
        const Item& it = its[pos];
        if (it.k == 0) {
            const Flora& o = flora[it.idx];
            const auto g = ground(st, o.x, o.y, 7);
            if (!g || !g->vis) continue;
            const double r = std::max(st.zoom * o.s * ks * 1.6, 26 * k * std::sqrt(ks)) + 4;
            put(g->X - r, g->Y - 2.4 * r, g->X + r, g->Y + r);
            continue;
        }
        if (!info) continue;
        const UserLayer& U = info->user;
        if (it.k == 6) { const Art& o = U.arts[it.idx]; around(ground(st, o.x, o.y), big); }
        else if (it.k == 7) {
            const Path& pts = U.walls[it.idx].pts;
            span({ ground(st, pts[it.sub][0], pts[it.sub][1]), ground(st, pts[it.sub + 1][0], pts[it.sub + 1][1]) }, big * 0.6);
        } else if (it.k == 8) { const Pt o = U.walls[it.idx].pts[it.sub]; around(ground(st, o[0], o[1]), big * 0.6); }
        else if (it.k == 4) { const Dock& o = info->docks[it.idx]; span({ ground(st, o.x0, o.y0), ground(st, o.x1, o.y1) }, big * 0.6); }
        else if (it.k == 5) { const Boat& o = info->boats[it.idx]; around(ground(st, o.x, o.y), big * 0.6); }
        else if (it.k == 3) {
            const Path& br = it.sub == 0 ? U.bridges[it.idx] : it.sub == 1 ? userRoadBridges(st)[it.idx] : info->bridges[it.idx];
            double x0 = INFINITY, y0 = INFINITY, x1 = -INFINITY, y1 = -INFINITY;
            for (const Pt& q : br) if (const auto g = ground(st, q[0], q[1])) { x0 = std::min(x0, g->X); y0 = std::min(y0, g->Y); x1 = std::max(x1, g->X); y1 = std::max(y1, g->Y); }
            if (x0 <= x1) put(x0 - big * 0.6, y0 - big, x1 + big * 0.6, y1 + big * 0.6);
        } else if (it.k == 2) { const Building& o = info->buildings[it.idx]; around(ground(st, o.x, o.y), big); }
        else if (it.k == 1) {
            const bool user = it.sub == 1;
            around(ground(st, user ? U.houses[it.idx].x : info->buildings[it.idx].x, user ? U.houses[it.idx].y : info->buildings[it.idx].y), big * 0.5);
        }
    }
    return ix;
}

void Scene::drawSprites(QPainter& p, const SceneState& st)
{
    CARTO_PROF("cena: sprites");
    if (!st.sprites || !st.aux || st.aux->lift.empty() || !st.doc) return;
    Doc& doc = *st.doc;
    const std::vector<Flora>& flora = doc.ensureFlora();
    const bool atlas = isAtlas(st), inkM = isInk(st);
    const Info* info = st.info() ? doc.info.get() : nullptr;
    const double k = clampd(st.zoom / st.fitZoom, 0.8, 1.8);
    const bool clipped = st.clip.isValid() && !st.planKey.isEmpty();
    const double W = st.vw, H = st.vh, pad = clipped ? 1e9 : st.cull;   // no azulejo quem escolhe é o índice
    const double ks = worldScale(doc.world);
    // de muito longe, uma fração das árvores basta (milhares de pontinhos de 2 px não se distinguem)
    const double zk = st.zoom * std::sqrt(ks);
    const int stride = zk < 0.6 ? static_cast<int>(std::ceil(std::pow(0.6 / zk, 2))) : 1;
    const SpriteSun sun = spriteSun(st.hour);
    spr::setSun(sun.L);
    auto out = [&](const Ground& g) { return g.X < -pad || g.Y < -pad || g.X > W + pad || g.Y > H + pad; };
    const double dpr = p.device() ? p.device()->devicePixelRatioF() : 1.0;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const std::vector<Item>& its = items(st);
    std::vector<int> sel;
    if (clipped) {
        const SpriteIndex& ix = spriteIndex(st, its);
        const QRectF c = st.clip.translated(-st.panX, -st.panY);
        for (int gy = static_cast<int>(std::floor(c.top() / kCell)); gy <= static_cast<int>(std::floor(c.bottom() / kCell)); gy++)
            for (int gx = static_cast<int>(std::floor(c.left() / kCell)); gx <= static_cast<int>(std::floor(c.right() / kCell)); gx++) {
                auto cell = ix.cells.find((static_cast<qint64>(gx) << 32) ^ static_cast<quint32>(gy));
                if (cell != ix.cells.end()) sel.insert(sel.end(), cell->begin(), cell->end());
            }
        std::sort(sel.begin(), sel.end());
        sel.erase(std::unique(sel.begin(), sel.end()), sel.end());
    }
    const int N = clipped ? static_cast<int>(sel.size()) : static_cast<int>(its.size());
    for (int qi = 0; qi < N; qi++) {
        const Item& it = its[clipped ? sel[qi] : qi];
        if (it.k == 0) {
            const Flora& o = flora[it.idx];
            // quem fica de fora de longe é sorteado pelo próprio índice (o mesmo em todo azulejo)
            const quint32 hsh = (static_cast<quint32>(o.v * 16777215.0) * 2654435761u) >> 8;
            if (stride > 1 && (hsh % stride)) continue;
            if (atlas && ((hsh / stride) % 3)) continue;        // Atlas: um símbolo a cada três
            if (inkM) {
                // mapa de livro: menos símbolos e maiores; de perto, todos. Arbusto e pedra solta não entram
                if (o.type == Shrub || o.type == Rock) continue;
                if (o.b == 2 && static_cast<int>(std::floor(o.v * 613)) % 4) continue;   // árvore solta na planície: uma em quatro
                const double zr = st.zoom / st.fitZoom;
                const int inkStride = zr < 1.6 ? 3 : zr < 2.6 ? 2 : 1;
                if (inkStride > 1 && static_cast<int>(std::floor(o.v * 997)) % inkStride) continue;
                const auto g = ground(st, o.x, o.y, 7);
                if (!g || !g->vis || out(*g)) continue;
                spr::inkGlyph(p, o.type, g->X, g->Y, k * 1.35 * std::sqrt(ks), o.v);
                continue;
            }
            const auto g = ground(st, o.x, o.y, 7);
            if (!g || !g->vis || out(*g)) continue;
            if (atlas) { spr::atlasGlyph(p, o.type, g->X, g->Y, k); continue; }
            const double ppw = st.zoom * o.s * ks;
            const spr::FloraImg& img = spr::floraImage(o.type, o.v, st.pitch, ppw * dpr, sun.mirror);
            const double f = ppw / (2 * img.ppw);
            p.drawImage(QRectF(g->X - img.ax * f, g->Y - img.ay * f, img.img.width() * f, img.img.height() * f), img.img);
            continue;
        }
        if (!info) continue;
        const UserLayer& U = info->user;
        if (it.k == 6) {
            const Art& o = U.arts[it.idx];
            const auto g = ground(st, o.x, o.y, artHeight(o.kind));
            if (!g || !g->vis || out(*g)) continue;
            const spr::Cam c = camAt(st, g->L, o.x, o.y);
            const RGB rc = realmCol(*info, o.realm);
            if (spr::drawArt(p, o, c, rc)) {}
            else if (o.kind == QLatin1String("castle")) spr::drawCastle(p, o.x, o.y, o.yaw, o.size, c, rc);
            else if (o.kind == QLatin1String("tower")) spr::drawTowerArt(p, o, c, rc);
            else if (o.kind == QLatin1String("lighthouse")) spr::drawLighthouse(p, o, c);
            else if (o.kind == QLatin1String("windmill")) spr::drawWindmill(p, o, c);
            else if (o.kind == QLatin1String("temple")) spr::drawTemple(p, o, c);
            else if (o.kind == QLatin1String("ruins")) spr::drawRuins(p, o, c);
        } else if (it.k == 7) {
            // a muralha inclina de uma ponta à outra acompanhando o chão (trechos colados, sem degrau)
            const Path& pts = U.walls[it.idx].pts;
            const Pt a = pts[it.sub], b = pts[it.sub + 1];
            const auto ga = ground(st, a[0], a[1], 4.2), gb = ground(st, b[0], b[1], 4.2);
            if (!ga || !gb || (!ga->vis && !gb->vis) || out(*ga)) continue;
            spr::Cam c = cam(st, 0);
            const double dx = b[0] - a[0], dy = b[1] - a[1], l2 = (dx * dx + dy * dy) ? dx * dx + dy * dy : 1;
            const auto base = c.proj;
            const double La = ga->L, Lb = gb->L, zs = st.zoom * st.g->sq;
            c.proj = [=](double x, double y, double z) {
                const double t = std::max(0.0, std::min(1.0, ((x - a[0]) * dx + (y - a[1]) * dy) / l2)), L = La + (Lb - La) * t;
                const QPointF q = base(x, y, z);
                return QPointF(q.x(), q.y() - zs * L);
            };
            spr::drawWallSeg(p, a, b, c, ks);
        } else if (it.k == 8) {
            const Pt o = U.walls[it.idx].pts[it.sub];
            const auto g = ground(st, o[0], o[1], 5);
            if (!g || !g->vis || out(*g)) continue;
            spr::drawWallTower(p, o, cam(st, g->L), ks);
        } else if (it.k == 4) {
            const Dock& o = info->docks[it.idx];
            const auto g = ground(st, (o.x0 + o.x1) / 2, (o.y0 + o.y1) / 2);
            if (!g || !g->vis || out(*g)) continue;
            spr::drawDock(p, o, cam(st, 0));
        } else if (it.k == 5) {
            const Boat& o = info->boats[it.idx];
            const auto g = ground(st, o.x, o.y);
            if (!g || !g->vis || out(*g)) continue;
            spr::drawBoat(p, o, camAt(st, 0, o.x, o.y));
        } else if (it.k == 3) {
            const Path& br = it.sub == 0 ? U.bridges[it.idx] : it.sub == 1 ? userRoadBridges(st)[it.idx] : info->bridges[it.idx];
            const Pt mid = br[br.size() / 2];
            const auto g = ground(st, mid[0], mid[1]);
            if (!g || !g->vis) continue;
            spr::drawBridge(p, br, cam(st, 0));
        } else if (it.k == 2) {
            const Building& o = info->buildings[it.idx];
            const auto g = ground(st, o.x, o.y, 12);
            if (!g || !g->vis || out(*g)) continue;
            spr::drawCastle(p, o.x, o.y, o.yaw, o.size, camAt(st, g->L, o.x, o.y), realmCol(*info, o.realm));
        } else if (it.k == 1) {
            const bool user = it.sub == 1;
            const double x = user ? U.houses[it.idx].x : info->buildings[it.idx].x, y = user ? U.houses[it.idx].y : info->buildings[it.idx].y;
            const auto g = ground(st, x, y, 4);
            if (!g || !g->vis || out(*g)) continue;
            if (user) { const House& h = U.houses[it.idx]; spr::drawHouse(p, h.x, h.y, h.yaw, h.v, h.floors, h.timber, camAt(st, g->L, h.x, h.y), h.snowy == 1); }
            else { const Building& h = info->buildings[it.idx]; spr::drawHouse(p, h.x, h.y, h.yaw, h.v, h.floors, h.timber, camAt(st, g->L, h.x, h.y), h.snowy == 1); }
        }
    }
    p.restore();
}

// ── luzes da noite: janela, tocha, lanterna e farol ──
void Scene::drawLights(QPainter& p, const SceneState& st, double night)
{
    CARTO_PROF("cena: luzes");
    if (!st.sprites || !st.info() || !isFull(st)) return;
    Doc& doc = *st.doc;
    const Info& info = *doc.info;
    const UserLayer& U = info.user;
    const bool gen = U.showGen;
    const QString key = QStringLiteral("%1|%2|%3:%4|%5").arg(doc.ver).arg(doc.infoVer).arg(U.ver).arg(U.showGen).arg(worldScale(doc.world));
    if (key != m_lightsKey) {
        m_lightsKey = key;
        m_lights.clear();
        const double k = worldScale(doc.world);
        auto add = [&](double x, double y, double z, double r, const char* kind, double a) { m_lights.push_back({ x, y, z, r, QString::fromLatin1(kind), a }); };
        auto fr = [](double v) { return v - std::floor(v); };
        auto house = [&](double x, double y, double v, int floors) {
            if (fr(v * 7.31) < 0.25) return;
            add(x, y, (floors == 2 ? 2.8 : 1.7) * k, 2.6 * k, "window", 0.75 + 0.25 * fr(v * 3.7));
        };
        auto castle = [&](double x, double y, double yaw, int size) {
            const spr::CastleCfg& D = spr::kCastle[std::max(0, std::min(3, size))];
            const double R = D.R * k;
            static const int kTorches[4] = { 4, 6, 9, 14 };
            const int n = kTorches[std::max(0, std::min(3, size))];
            for (int i = 0; i < n; i++) {
                const double a = yaw + (i + 0.5) / n * M_PI * 2;
                add(x + std::cos(a) * R, y + std::sin(a) * R, (D.wallH + 0.6) * k, 2.4 * k, "torch", 0.9);
            }
            add(x, y, D.keepH * 0.7 * k, 3.4 * k, "window", 1);
            if (D.hasInner) for (int i = 0; i < 4; i++) {
                const double a = yaw + i / 4.0 * M_PI * 2;
                add(x + std::cos(a) * D.iR * k, y + std::sin(a) * D.iR * k, (D.iWallH + 0.6) * k, 2.2 * k, "torch", 0.85);
            }
        };
        if (gen) {
            for (const Building& b : info.buildings) { if (b.castle) castle(b.x, b.y, b.yaw, b.size); else house(b.x, b.y, b.v, b.floors); }
            for (const Dock& d : info.docks) add(d.x1, d.y1, 1.4, 1.9 * k, "lantern", 0.9);
            for (const Boat& b : info.boats) add(b.x, b.y, 2.4 * k, 1.3 * k, "lantern", 0.6);
            for (const Path& br : info.bridges) { add(br[0][0], br[0][1], 1.6, 1.6 * k, "lantern", 0.8); add(br.back()[0], br.back()[1], 1.6, 1.6 * k, "lantern", 0.8); }
        }
        for (const House& h : U.houses) house(h.x, h.y, h.v, h.floors);
        for (const Art& a : U.arts) {
            if (a.kind == QLatin1String("castle")) castle(a.x, a.y, a.yaw, a.size);
            else if (a.kind == QLatin1String("tower")) add(a.x, a.y, 9.6 * k, 2.6 * k, "torch", 1);
            else if (a.kind == QLatin1String("lighthouse")) add(a.x, a.y, 11.6 * k, 5.5 * k, "beacon", 1);
            else if (a.kind == QLatin1String("windmill")) add(a.x, a.y, 1.8 * k, 2.2 * k, "window", 0.8);
            else if (a.kind == QLatin1String("temple")) for (int sg : { -1, 1 }) add(a.x + std::cos(a.yaw) * 3.4 * k * sg, a.y + std::sin(a.yaw) * 3.4 * k * sg, 1.4 * k, 2.2 * k, "torch", 0.95);
            else {
                const double ca = std::cos(a.yaw), sa = std::sin(a.yaw);
                for (const spr::ArtLight& l : spr::artLights(a.kind))
                    add(a.x + (ca * l.lx - sa * l.ly) * k, a.y + (sa * l.lx + ca * l.ly) * k, l.z * k, l.r * k, l.kind, l.force);
            }
        }
        for (const Wall& wl : U.walls) for (size_t i = 0; i < wl.pts.size(); i += 6) add(wl.pts[i][0], wl.pts[i][1], 4.8 * k, 1.9 * k, "torch", 0.85);
        for (const Road& rd : U.roads) if (rd.kind == QLatin1String("royal"))
            for (size_t i = 8; i < rd.pts.size(); i += std::max<size_t>(6, static_cast<size_t>(jsRound(16 * k)))) add(rd.pts[i][0], rd.pts[i][1], 2.2 * k, 1.7 * k, "lantern", 0.7);
        for (const Path& br : roadBridges(doc.world, U)) { add(br[0][0], br[0][1], 1.6, 1.6 * k, "lantern", 0.8); add(br.back()[0], br.back()[1], 1.6, 1.6 * k, "lantern", 0.8); }
    }
    const double co = std::cos(st.pitch * M_PI / 180), pad = 40;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setCompositionMode(QPainter::CompositionMode_Plus);
    for (const Light& l : m_lights) {
        const auto g = ground(st, l.x, l.y, l.z);
        if (!g || !g->vis) continue;
        const double X = g->X, Y = g->Y - st.zoom * co * l.z;
        if (X < -pad || Y < -pad || X > st.vw + pad || Y > st.vh + pad) continue;
        const bool beacon = l.kind == QLatin1String("beacon");
        const double r = std::max(beacon ? 7.0 : 3.2, st.zoom * l.r * 1.7);
        p.setOpacity(std::min(1.0, night * l.a));
        p.drawImage(QRectF(X - r, Y - r, r * 2, r * 2), spr::glowImage(l.kind));
        if (beacon) {
            // facho do farol girando com as horas
            const double a = st.hour * M_PI * 2 * 1.5, len = std::max(40.0, st.zoom * 46 * worldScale(doc.world)), sq = st.g->sq;
            const double dx = std::cos(a), dy = std::sin(a) * sq, nx = -dy * 0.16, ny = dx * 0.16;
            QLinearGradient gr(QPointF(X, Y), QPointF(X + dx * len, Y + dy * len));
            gr.setColorAt(0, QColor(255, 244, 210, 107));
            gr.setColorAt(1, QColor(255, 244, 210, 0));
            QPainterPath tri;
            tri.moveTo(X, Y); tri.lineTo(X + (dx + nx) * len, Y + (dy + ny) * len); tri.lineTo(X + (dx - nx) * len, Y + (dy - ny) * len); tri.closeSubpath();
            p.fillPath(tri, gr);
        }
    }
    p.restore();
}

// ── nomes do relevo + reinos + cidades ──
namespace {
// Estilo de cada tipo. base: tamanho pelo tamanho do acidente na tela; caps: versalete;
// it: itálico; sp: espaço entre letras (× tamanho); arc: curvinha do nome de área; line: segue a crista ou o rio
struct NSty { bool caps, it; double base, min, max, sp; bool water; double arc; bool line; };
const NSty* nameStyle(const QString& kind)
{
    static const NSty AREA{ false, true, 0.85, 12, 22, 0.16, false, 0.04, false };
    static const QHash<QString, NSty> S = {
        { "ocean", { true, true, 1.35, 17, 42, 0.55, true, 0.07, false } },
        { "sea", { true, true, 1.15, 14, 30, 0.4, true, 0.06, false } },
        { "bay", { false, true, 0.95, 12, 19, 0.1, true, 0.04, false } },
        { "lake", { false, true, 0.9, 11, 17, 0.06, true, 0.03, false } },
        { "river", { false, true, 0, 11, 15, 0.08, true, 0, true } },
        { "range", { true, false, 0.95, 12, 24, 0.32, false, 0, true } },
        { "peak", { false, true, 0, 11, 13, 0.02, false, 0, false } },
        { "forest", AREA }, { "desert", AREA }, { "ice", AREA }, { "marsh", AREA }, { "plains", AREA },
        { "island", { false, false, 0.85, 12, 19, 0.08, false, 0.03, false } },
        { "isles", { false, true, 0.8, 12, 19, 0.14, false, 0.04, false } },
        { "cland", { true, false, 1.0, 11, 48, 0.3, false, 0.03, true } },
        { "cwater", { false, true, 1.0, 11, 48, 0.2, true, 0.04, true } },
    };
    auto it = S.find(kind);
    return it == S.end() ? nullptr : &it.value();
}
// [preenchimento, contorno] por vista
std::pair<QColor, QColor> namePalette(const QString& mode, bool water)
{
    if (mode == QLatin1String("ink")) return water ? std::make_pair(QColor(40, 33, 28, 214), QColor(240, 231, 208, 230)) : std::make_pair(QColor(40, 33, 28), QColor(240, 231, 208, 235));
    if (mode == QLatin1String("atlas")) return water ? std::make_pair(QColor(52, 92, 118), QColor(214, 232, 236, 217)) : std::make_pair(QColor(78, 58, 42), QColor(246, 240, 224, 217));
    return water ? std::make_pair(QColor(208, 230, 242, 240), QColor(8, 26, 40, 153)) : std::make_pair(QColor(252, 246, 232, 245), QColor(18, 16, 14, 158));
}
// grade de colisão: um nome só entra onde não encosta em outro
class HitGrid {
public:
    bool hits(const std::vector<QRectF>& boxes) const
    {
        for (const QRectF& b : boxes) {
            bool hit = false;
            each(b, [&](qint64 key) {
                auto it = m_cells.find(key);
                if (it == m_cells.end()) return false;
                for (const QRectF& o : it.value()) if (b.left() < o.right() && b.right() > o.left() && b.top() < o.bottom() && b.bottom() > o.top()) { hit = true; return true; }
                return false;
            });
            if (hit) return true;
        }
        return false;
    }
    void add(const std::vector<QRectF>& boxes) { for (const QRectF& b : boxes) each(b, [&](qint64 key) { m_cells[key].push_back(b); return false; }); }
private:
    template <typename F> static void each(const QRectF& b, F fn)
    {
        const int CS = 48;
        for (int gx = static_cast<int>(std::floor(b.left() / CS)); gx <= static_cast<int>(std::floor(b.right() / CS)); gx++)
            for (int gy = static_cast<int>(std::floor(b.top() / CS)); gy <= static_cast<int>(std::floor(b.bottom() / CS)); gy++)
                if (fn((static_cast<qint64>(gx) << 32) ^ static_cast<quint32>(gy))) return;
    }
    QHash<qint64, std::vector<QRectF>> m_cells;
};
QRectF box(double x0, double y0, double x1, double y1) { return QRectF(QPointF(x0, y0), QPointF(x1, y1)); }

// parábola que passa pelo meio dos pontos (o nome curva de leve, sem tremer com cada pico)
struct Arc { double mx, my, dx, dy, nx, ny, c0 = 0, c1 = 0, c2 = 0, ac = 0, len = 0; };
Arc fitArc(const std::vector<QPointF>& P)
{
    const int n = static_cast<int>(P.size());
    double mx = 0, my = 0;
    for (const QPointF& p : P) { mx += p.x(); my += p.y(); }
    mx /= n; my /= n;
    double xx = 0, xy = 0, yy = 0;
    for (const QPointF& p : P) { const double dx = p.x() - mx, dy = p.y() - my; xx += dx * dx; xy += dx * dy; yy += dy * dy; }
    const double an = 0.5 * std::atan2(2 * xy, xx - yy);
    double d0 = std::cos(an), d1 = std::sin(an);
    if (d0 < 0) { d0 = -d0; d1 = -d1; }
    if (std::fabs(d0) < 0.26 && d1 > 0) { d0 = -d0; d1 = -d1; }      // quase vertical: lê de baixo pra cima
    const double n0 = d1, n1 = -d0;                                     // "pra cima" do texto
    std::vector<double> A(n), B(n);
    for (int i = 0; i < n; i++) { A[i] = (P[i].x() - mx) * d0 + (P[i].y() - my) * d1; B[i] = (P[i].x() - mx) * n0 + (P[i].y() - my) * n1; }
    const double amin = *std::min_element(A.begin(), A.end()), amax = *std::max_element(A.begin(), A.end()), half = std::max(1.0, (amax - amin) / 2);
    double c0 = 0, c1 = 0, c2 = 0;
    if (n >= 3) {
        double s0 = n, s1 = 0, s2 = 0, s3 = 0, s4 = 0, t0 = 0, t1 = 0, t2 = 0;
        for (int i = 0; i < n; i++) { const double a = A[i], b = B[i], a2 = a * a; s1 += a; s2 += a2; s3 += a2 * a; s4 += a2 * a2; t0 += b; t1 += b * a; t2 += b * a2; }
        auto det3 = [](const double m[9]) { return m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) + m[2] * (m[3] * m[7] - m[4] * m[6]); };
        const double Mx[9] = { s0, s1, s2, s1, s2, s3, s2, s3, s4 };
        const double D = det3(Mx);
        if (std::fabs(D) > 1e-9) {
            const double m0[9] = { t0, s1, s2, t1, s2, s3, t2, s3, s4 }, m1[9] = { s0, t0, s2, s1, t1, s3, s2, t2, s4 }, m2[9] = { s0, s1, t0, s1, s2, t1, s2, s3, t2 };
            c0 = det3(m0) / D; c1 = det3(m1) / D; c2 = det3(m2) / D;
        }
    } else if (n == 2) c0 = (B[0] + B[1]) / 2;
    const double cmax = 0.12 / half;
    c2 = std::max(-cmax, std::min(cmax, c2));
    c1 = std::max(-0.35, std::min(0.35, c1));
    Arc r{ mx, my, d0, d1, n0, n1 };
    r.c0 = c0; r.c1 = c1; r.c2 = c2; r.ac = (amin + amax) / 2; r.len = amax - amin;
    return r;
}
// trecho do meio de uma linha com o comprimento pedido (rio comprido: o nome pega um pedaço)
std::vector<QPointF> midRun(const std::vector<QPointF>& P, double L)
{
    std::vector<double> cum{ 0 };
    for (size_t i = 1; i < P.size(); i++) cum.push_back(cum[i - 1] + std::hypot(P[i].x() - P[i - 1].x(), P[i].y() - P[i - 1].y()));
    const double T = cum.back();
    if (T <= L) return P;
    const double a = (T - L) / 2, b = a + L;
    std::vector<QPointF> out;
    for (size_t i = 0; i < P.size(); i++) if (cum[i] >= a && cum[i] <= b) out.push_back(P[i]);
    return out;
}
struct Glyph { QString ch; double X, Y, rot; };
struct Layout { std::vector<Glyph> glyphs; std::vector<QRectF> boxes; QRectF bbox; FontEntry* fe = nullptr; double fs = 12; bool caps = false, it = false; bool marker = false; QPointF mk; };
}

static std::optional<Layout> layoutName(const Scene& sc, const SceneState& st, const Label& lab, const NSty& sty, double fs)
{
    const QString txt = sty.caps ? lab.text.toUpper() : lab.text;
    if (txt.trimmed().isEmpty()) return std::nullopt;
    FontEntry* fe = &fontAt(sty.caps, sty.it, fs);
    double sp = sty.sp * fs;
    std::vector<double> ws;
    double tw = 0;
    for (QChar ch : txt) { ws.push_back(fe->advance(ch) + sp); tw += ws.back(); }
    tw -= sp;
    // nome de área maior que a área: aperta o espaço entre letras e encolhe (até o mínimo do tipo)
    const double room = 2.3 * lab.r * st.zoom * lab.size;
    if (!sty.line && lab.autoName && lab.kind != QLatin1String("peak") && tw > room) {
        const double k = std::max(sty.min / fs, room / tw);
        fs = jsRound(fs * k * 10) / 10;
        fe = &fontAt(sty.caps, sty.it, fs);
        sp = sty.sp * fs * std::max(0.35, k);
        ws.clear(); tw = 0;
        for (QChar ch : txt) { ws.push_back(fe->advance(ch) + sp); tw += ws.back(); }
        tw -= sp;
    }
    std::optional<Arc> arc;
    Layout L;
    if (sty.line && lab.pts.size() >= 2) {
        std::vector<QPointF> P;
        for (const Pt& q : lab.pts) { const auto g = sc.ground(st, q[0], q[1]); if (g) P.push_back(QPointF(g->X, g->Y)); }
        if (lab.kind == QLatin1String("river") && P.size() > 3) P = midRun(P, tw * 1.3);
        if (P.size() >= 2) arc = fitArc(P);
        if (arc && lab.kind == QLatin1String("river")) arc->c0 += fs * 0.85;          // rio: o nome corre do lado, não em cima da água
    }
    if (!arc) {
        const auto g = sc.ground(st, lab.x, lab.y);
        if (!g) return std::nullopt;
        const double sag = sty.arc * tw;
        if (lab.kind == QLatin1String("peak")) {
            L.marker = true; L.mk = QPointF(g->X, g->Y);
            arc = Arc{ g->X + 8 + tw / 2, g->Y - 3, 1, 0, 0, -1 };
        } else {
            arc = Arc{ g->X, g->Y, 1, 0, 0, -1 };
            arc->c2 = tw > 0 ? -4 * sag / (tw * tw) : 0;
        }
    }
    const Arc& A = *arc;
    auto slope = [&](double a) { return A.c1 + 2 * A.c2 * a; };
    double a = A.ac - tw / 2 / std::sqrt(1 + std::pow(slope(A.ac), 2));
    double x0 = INFINITY, y0 = INFINITY, x1 = -INFINITY, y1 = -INFINITY;
    for (int k = 0; k < txt.size(); k++) {
        const double w = ws[k];
        const double da = w / std::sqrt(1 + std::pow(slope(a + w / 2), 2));
        const double am = a + (da - (k == txt.size() - 1 ? 0 : sp / std::sqrt(1 + std::pow(slope(a), 2)))) / 2;
        const double b = A.c0 + A.c1 * am + A.c2 * am * am, sl = slope(am);
        const double X = A.mx + A.dx * am + A.nx * b, Y = A.my + A.dy * am + A.ny * b;
        L.glyphs.push_back({ QString(txt[k]), X, Y, std::atan2(A.dy + A.ny * sl, A.dx + A.nx * sl) });
        if (txt[k] != QLatin1Char(' ')) {
            const double r = std::max(w - sp, fs * 0.5) * 0.58, rv = fs * 0.55;
            L.boxes.push_back(box(X - r, Y - rv, X + r, Y + rv));
            x0 = std::min(x0, X - r); y0 = std::min(y0, Y - rv); x1 = std::max(x1, X + r); y1 = std::max(y1, Y + rv);
        }
        a += da;
    }
    if (L.marker) { L.boxes.push_back(box(L.mk.x() - 5, L.mk.y() - 7, L.mk.x() + 5, L.mk.y() + 2)); x0 = std::min(x0, L.mk.x() - 5); }
    if (L.boxes.empty()) return std::nullopt;
    L.bbox = box(x0, y0, x1, y1);
    L.fe = fe; L.fs = fs; L.caps = sty.caps; L.it = sty.it;
    return L;
}

// knock (Tinta): clareira de papel em volta das letras, como no mapa impresso; o traço do mapa some embaixo
static bool drawName(QPainter& p, const Layout& L, const std::pair<QColor, QColor>& pal, double alpha, bool knock, bool quick)
{
    bool near = false;
    p.save();
    p.setOpacity(alpha);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    if (L.marker) {
        QPainterPath m;
        m.moveTo(L.mk.x(), L.mk.y() - 6); m.lineTo(L.mk.x() + 4.2, L.mk.y() + 1); m.lineTo(L.mk.x() - 4.2, L.mk.y() + 1); m.closeSubpath();
        p.strokePath(m, strokePen(pal.second, 2.6));
        p.fillPath(m, pal.first);
    }
    const double dpr = p.device() ? p.device()->devicePixelRatioF() : 1.0;
    const double fq = quantSize(L.fs);
    const double lw = std::max(2.6, fq * 0.24), k1 = std::max(9.0, fq * 0.95), k2 = std::max(5.0, fq * 0.5);
    const QString base = QStringLiteral("g|%1|%2|%3|%4|%5|").arg(L.caps).arg(L.it).arg(colorKey(pal.second), colorKey(pal.first)).arg(knock).arg(dpr);
    const QTransform t0 = p.worldTransform();
    for (int pass = 0; pass < 2; pass++) {
        for (const Glyph& g : L.glyphs) {
            if (g.ch == QLatin1String(" ")) continue;
            const QChar c = g.ch.at(0);
            const QString fam = base + c;
            double fu = fq;
            const TextSprite* spt = findSprite(fam, fq, quick ? &fu : nullptr);
            if (!spt) { fu = fq; spt = &textSprite(fam, fq, fontAt(L.caps, L.it, fq).glyphPath(c), pal.second, lw, knock, k1, k2, pal.first, dpr); }
            if (fu != fq) near = true;
            const double sc = L.fs / fu;
            QTransform t;
            t.translate(g.X, g.Y);
            t.rotateRadians(g.rot);
            t.scale(sc, sc);
            p.setWorldTransform(t * t0);
            p.drawImage(spt->rect, pass == 0 ? spt->halo : spt->fill);
        }
    }
    p.setWorldTransform(t0);
    p.restore();
    return near;
}

static void atlasCastle(QPainter& p, double x, double y, double k, const RGB& rc)
{
    const double s = 7 * k;
    QPainterPath c;
    c.moveTo(x - s, y); c.lineTo(x - s, y - s * 1.1); c.lineTo(x - s * 0.6, y - s * 1.1); c.lineTo(x - s * 0.6, y - s * 0.8);
    c.lineTo(x - s * 0.3, y - s * 0.8); c.lineTo(x - s * 0.3, y - s * 1.7); c.lineTo(x + s * 0.3, y - s * 1.7); c.lineTo(x + s * 0.3, y - s * 0.8);
    c.lineTo(x + s * 0.6, y - s * 0.8); c.lineTo(x + s * 0.6, y - s * 1.1); c.lineTo(x + s, y - s * 1.1); c.lineTo(x + s, y); c.closeSubpath();
    p.setBrush(QColor(248, 242, 228));
    p.setPen(QPen(QColor(36, 28, 22), 1.5));
    p.drawPath(c);
    p.drawLine(QPointF(x, y - s * 1.7), QPointF(x, y - s * 2.5));
    QPainterPath f;
    f.moveTo(x, y - s * 2.5); f.lineTo(x + s * 0.8, y - s * 2.25); f.lineTo(x, y - s * 2.0); f.closeSubpath();
    p.fillPath(f, rgbOf(rc));
}

void Scene::drawLabels(QPainter& p, const SceneState& st)
{
    CARTO_PROF("cena: nomes");
    m_labelsNear = false;
    if (!st.info()) return;
    Doc& doc = *st.doc;
    Info& info = *doc.info;
    const bool atlas = isAtlas(st), inkM = isInk(st), flat = atlas || inkM;
    const double k = clampd(st.zoom / st.fitZoom, 0.8, 2.4);
    HitGrid hash;
    hash.add(st.avoid);
    struct Job { FontEntry* font; double ls; QString txt; double x, y; bool center; double lw; QColor stroke, fill; double fs; bool caps = false, it = false; };
    std::vector<Job> jobs;
    auto textBox = [](double x, double y, double w, double fs, bool center) {
        return center ? box(x - w / 2, y - fs * 0.6, x + w / 2, y + fs * 0.6) : box(x, y - fs * 0.6, x + w, y + fs * 0.6);
    };
    const QColor halo = flat ? (inkM ? QColor(240, 231, 208, 235) : QColor(248, 242, 228, 235)) : QColor(14, 16, 20, 179);
    const QColor ink = flat ? (inkM ? QColor(40, 33, 28) : QColor(44, 32, 24)) : QColor(246, 240, 228);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    // reinos (centralizados, com letras espaçadas)
    for (const Realm& r : info.realms) {
        if (!r.area) continue;
        const auto g = ground(st, r.x, r.y);
        if (!g) continue;
        const double size = std::max(13.0, std::min(34.0, std::sqrt(double(r.area)) * 0.42 * k));
        const QString txt = r.name.toUpper();
        const double ls = jsRound(size * 0.28);
        FontEntry& font = fontAt(true, false, size, ls);
        const double w = font.fm.horizontalAdvance(txt);
        const RGB c = r.color;
        const QColor fill = inkM ? QColor(40, 33, 28, 230) : atlas ? rgba(c[0] * 0.55, c[1] * 0.55, c[2] * 0.55, 0.85)
                                                                     : rgba(std::min(255.0, c[0] + 60), std::min(255.0, c[1] + 60), std::min(255.0, c[2] + 60), 0.9);
        jobs.push_back({ &font, ls, txt, g->X, g->Y, true, 4, flat ? (inkM ? QColor(240, 231, 208, 204) : QColor(246, 240, 224, 191)) : QColor(10, 14, 18, 128), fill, size, true, false });
        hash.add({ textBox(g->X, g->Y, w, size, true) });
    }
    UserLayer& U = info.user;
    if (flat) for (const House& h : U.houses) {
        const auto g = ground(st, h.x, h.y);
        if (!g) continue;
        const double sz = 2.2 * k;
        p.save();
        p.translate(g->X, g->Y);
        p.rotate((h.yaw + st.ang) * 180 / M_PI);
        p.setBrush(inkM ? QColor(240, 231, 208) : QColor(120, 70, 50));
        p.setPen(QPen(inkM ? QColor(40, 33, 28) : QColor(44, 30, 22), 0.8));
        p.drawRect(QRectF(-sz, -sz * 0.7, sz * 2, sz * 1.4));
        p.restore();
    }
    auto flagOf = [&](const RGB& rc) { return inkM ? RGB{ 40, 33, 28 } : rc; };
    for (const Art& a : U.arts) {
        const auto g = ground(st, a.x, a.y);
        if (!g) continue;
        const RGB rc = realmCol(info, a.realm);
        if (flat) {
            if (a.kind == QLatin1String("castle")) atlasCastle(p, g->X, g->Y, k, flagOf(rc));
            else {
                QPainterPath d;
                d.moveTo(g->X, g->Y - 5 * k); d.lineTo(g->X + 3.4 * k, g->Y); d.lineTo(g->X, g->Y + 3 * k); d.lineTo(g->X - 3.4 * k, g->Y); d.closeSubpath();
                p.setBrush(QColor(250, 246, 234)); p.setPen(QPen(QColor(40, 30, 22), 1.4));
                p.drawPath(d);
            }
            hash.add({ box(g->X - 6 * k, g->Y - 12 * k, g->X + 6 * k, g->Y + 4 * k) });
        }
        if (a.name.isEmpty() || (!g->vis && !flat)) continue;
        const bool castle = a.kind == QLatin1String("castle");
        const double fs = (castle ? 14 : 12) * std::min(1.5, k);
        FontEntry& font = fontAt(castle, !castle, fs);
        const double lx = g->X + (flat ? 9 : 16) * k, ly = g->Y - (flat ? 4 : 8) * k;
        const double w = font.fm.horizontalAdvance(a.name);
        jobs.push_back({ &font, 0, a.name, lx, ly, false, 3.5, halo, ink, fs, castle, !castle });
        hash.add({ textBox(lx, ly, w, fs, false) });
    }
    if (U.showGen) for (const Town& tw : info.towns) {
        const auto g = ground(st, tw.x, tw.y);
        if (!g) continue;
        const RGB rc = realmCol(info, tw.realm);
        if (flat) {
            if (tw.capital) atlasCastle(p, g->X, g->Y, k, flagOf(rc));
            else { p.setBrush(QColor(250, 246, 234)); p.setPen(QPen(QColor(40, 30, 22), 1.6)); p.drawEllipse(QPointF(g->X, g->Y), 3.2 * k, 3.2 * k); }
            hash.add({ box(g->X - 6 * k, g->Y - 12 * k, g->X + 6 * k, g->Y + 4 * k) });
        }
        if (!g->vis && !flat) continue;
        const double fs = (tw.capital ? 14 : 12) * std::min(1.5, k);
        FontEntry& font = fontAt(tw.capital, !tw.capital, fs);
        const double off = flat ? (tw.capital ? 11 : 6) : (tw.capital ? 16 : 10);
        const double lx = g->X + off * k, ly = g->Y - (flat ? (tw.capital ? 6 : 1) : 5) * k;
        const double w = font.fm.horizontalAdvance(tw.name);
        jobs.push_back({ &font, 0, tw.name, lx, ly, false, 3.5, halo, ink, fs, tw.capital, !tw.capital });
        hash.add({ textBox(lx, ly, w, fs, false) });
    }

    // nomes do relevo: do mais importante pro menos; quem não cabe sem encostar fica de fora
    const std::vector<NameFeature>& F = doc.ensureNames(st.density);
    m_lastNames = resolveNames(F, doc.labels, st.namesAuto);
    m_lastLayout.clear();
    // nome que o usuário mexeu entra primeiro e nunca cede (o rio pintado também não)
    std::vector<int> order(m_lastNames.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = static_cast<int>(i);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        const Label& p1 = m_lastNames[a]; const Label& q1 = m_lastNames[b];
        if (p1.forced() != q1.forced()) return p1.forced();
        if (p1.prio != q1.prio) return p1.prio > q1.prio;
        return p1.r > q1.r;
    });
    const double zk = std::sqrt(st.zoom / st.fitZoom);
    const double sq = st.g ? st.g->sq : 1;
    static const double NUDGE[10][2] = { { 0.4, 0 }, { -0.4, 0 }, { 0, 0.4 }, { 0, -0.4 }, { 0.3, 0.3 }, { -0.3, 0.3 }, { 0.3, -0.3 }, { -0.3, -0.3 }, { 0.65, 0 }, { -0.65, 0 } };
    for (int oi : order) {
        const Label& lab = m_lastNames[oi];
        const NSty* sty = nameStyle(lab.kind);
        if (!sty) continue;
        double fs;
        if (!sty->base) fs = std::max(sty->min, std::min(sty->max, sty->min * std::min(1.35, zk)));
        else {
            const double raw = sty->base * std::sqrt(lab.r * st.zoom);
            if (!lab.forced() && lab.kind != QLatin1String("ocean") && raw < sty->min * 0.62 && !(st.namesTool && st.selName == lab.ref)) continue;   // pequeno demais nesse zoom
            fs = std::max(sty->min, std::min(sty->max, raw));
        }
        fs = jsRound(fs * lab.size * 10) / 10;
        const bool ghost = lab.hidden;
        if (ghost && !st.namesTool) continue;
        const bool sel = st.namesTool && st.selName == lab.ref;
        // letra de nome de água caindo na terra também conta como esbarrão (o rio corre na margem de propósito)
        auto wet = [&](const Layout& T) {
            if (!sty->water || lab.kind == QLatin1String("river")) return true;
            for (const Glyph& g : T.glyphs) {
                if (g.ch == QLatin1String(" ")) continue;
                const auto q = planeToWorld(doc.world, st.ang, (g.X - st.panX) / st.zoom, (g.Y - st.panY) / (st.zoom * sq));
                if (heightAt(doc.world, q[0], q[1]) > 0) return false;
            }
            return true;
        };
        auto bad = [&](const Layout& T) { return hash.hits(T.boxes) || !wet(T); };
        std::optional<Layout> L = layoutName(*this, st, lab, *sty, fs);
        if (!L) continue;
        if (!ghost && !lab.forced() && !sel && bad(*L)) {
            // esbarrou: tenta um pouco pro lado, ainda dentro do acidente (nome de água só em cima da água)
            L.reset();
            if (!sty->line && lab.kind != QLatin1String("peak")) for (const auto& nd : NUDGE) {
                const double x = lab.x + nd[0] * lab.r, y = lab.y + nd[1] * lab.r;
                if (sty->water && heightAt(doc.world, x, y) > 0) continue;
                if (!sty->water && heightAt(doc.world, x, y) <= 0) continue;
                Label moved = lab; moved.x = x; moved.y = y;
                std::optional<Layout> T = layoutName(*this, st, moved, *sty, fs);
                if (T && !bad(*T)) { L = T; break; }
            }
            if (!L) continue;
        }
        if (!ghost) hash.add(L->boxes);
        if (drawName(p, *L, namePalette(st.mode, sty->water), ghost ? 0.32 : 1, inkM && !ghost, st.quick)) m_labelsNear = true;
        m_lastLayout.push_back({ lab, L->boxes, L->bbox });
        if (sel) {
            const QRectF b = L->bbox.adjusted(-4, -3, 4, 3);
            QPen pen(flat ? QColor(40, 33, 28, 217) : QColor(250, 236, 200, 242), 1.5);
            pen.setDashPattern({ 5 / 1.5, 4 / 1.5 });
            p.setPen(pen); p.setBrush(Qt::NoBrush);
            p.drawRect(b);
        }
    }

    // reinos, cidades e artefatos por cima
    const double dpr = p.device() ? p.device()->devicePixelRatioF() : 1.0;
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QTransform t0 = p.worldTransform();
    for (int pass = 0; pass < 2; pass++) {
        for (const Job& j : jobs) {
            // Tinta: a mesma clareira de papel dos nomes do relevo
            const double fq = quantSize(j.fs);
            const QString fam = QStringLiteral("t|%1|%2|%3|%4|%5|%6|%7|%8").arg(j.caps).arg(j.it).arg(j.ls ? 1 : 0).arg(colorKey(j.stroke), colorKey(j.fill))
                                    .arg(inkM).arg(dpr).arg(j.txt);
            double fu = fq;
            const TextSprite* spt = findSprite(fam, fq, st.quick ? &fu : nullptr);
            if (!spt) {
                fu = fq;
                FontEntry& fe = fontAt(j.caps, j.it, fq, j.ls ? jsRound(fq * 0.28) : 0);
                spt = &textSprite(fam, fq, fe.textPath(j.txt), j.stroke, j.lw, inkM, std::max(9.0, fq * 0.95), std::max(5.0, fq * 0.5), j.fill, dpr);
            }
            if (fu != fq) m_labelsNear = true;
            const double sc = j.fs / fu;
            const double w = fontAt(j.caps, j.it, fu, j.ls ? jsRound(fu * 0.28) : 0).fm.horizontalAdvance(j.txt);
            QTransform t;
            t.translate(j.x, j.y);
            t.scale(sc, sc);
            if (j.center) t.translate(-w / 2, 0);
            p.setWorldTransform(t * t0);
            p.drawImage(spt->rect, pass == 0 ? spt->halo : spt->fill);
        }
    }
    p.setWorldTransform(t0);
    p.restore();
}

const Label* Scene::labelAt(QPointF at) const
{
    for (auto it = m_lastLayout.rbegin(); it != m_lastLayout.rend(); ++it)
        for (const QRectF& b : it->boxes)
            if (at.x() >= b.left() - 3 && at.x() <= b.right() + 3 && at.y() >= b.top() - 3 && at.y() <= b.bottom() + 3) return &it->lab;
    return nullptr;
}

const Label* Scene::nameByRef(const QString& ref) const
{
    for (const Label& l : m_lastNames) if (l.ref == ref) return &l;
    return nullptr;
}

} // namespace carto
