// Ferramentas das camadas da mesa (porte do index.html do Map Lab):
// Territórios (semear, pintar, neutro), Construir (artefatos únicos, ponte,
// vila, plantação, muralha, estrada, apagar) e Nomes (editar, mover, criar).
// Toda edição tira uma foto das camadas antes e avisa o painel (desfazer).

#include "CartoCanvas.h"

#include <QCoreApplication>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace carto {

namespace {
bool isUnique(const QString& k)
{
    return k == QLatin1String("castle") || k == QLatin1String("tower") || k == QLatin1String("lighthouse") || k == QLatin1String("windmill")
        || k == QLatin1String("temple") || k == QLatin1String("ruins");
}
QColor rgbC(const RGB& c, int a = 255) { return QColor(qRound(c[0]), qRound(c[1]), qRound(c[2]), a); }
QPen dashPen(const QColor& c, double w, double on, double off)
{
    QPen p(c, w);
    p.setDashPattern({ on / w, off / w });
    p.setCapStyle(Qt::FlatCap);
    return p;
}
}

// ── edições com desfazer ──
void Canvas::layersDone(std::shared_ptr<Doc::Layers> before, bool repaint)
{
    sceneChanged();
    emit layersEdited(std::move(before));
    if (repaint) requestFull();
    update();
}

void Canvas::realmEdit(const std::function<void(Info&)>& fn)
{
    if (!m_doc) return;
    waitIdle();
    auto before = std::make_shared<Doc::Layers>(m_doc->layers());
    Info& in = m_doc->ensureInfo();
    fn(in);
    if (m_opts.realmSel >= static_cast<int>(in.defs.size())) m_opts.realmSel = static_cast<int>(in.defs.size()) - 1;
    if (m_opts.realmSel < 0) m_opts.realmSel = 0;
    recomputeRealms(*m_world, in);
    layersDone(before, true);
    emit selectionChanged();
}

void Canvas::userEdit(const std::function<bool(Info&)>& fn)
{
    if (!m_doc) return;
    // construções não entram na pintura do terreno: a pintura de fundo pode seguir
    if (!m_doc->info || m_doc->infoVer != m_doc->ver) waitIdle();
    auto before = std::make_shared<Doc::Layers>(m_doc->layers());
    Info& in = m_doc->ensureInfo();
    if (!fn(in)) return;
    in.user.ver++;
    refreshOwners(in);                  // a bandeira pega a cor do território onde ficou
    layersDone(before, false);
    emit selectionChanged();
}

void Canvas::nameEdit(const std::function<bool(LabelLayer&)>& fn)
{
    if (!m_doc) return;
    auto before = std::make_shared<Doc::Layers>(m_doc->layers());
    if (!fn(m_doc->labels)) return;
    layersDone(before, false);
    emit selectionChanged();
}

void Canvas::optsChanged()
{
    m_bridgeStart.reset();
    m_roadFrom.reset();
    m_toolMsg.clear();
    sceneChanged();
    update();
}

void Canvas::setSelArt(const QString& id)
{
    if (m_selArt == id) return;
    m_selArt = id;
    emit selectionChanged();
    update();
}

Art* Canvas::selectedArt()
{
    if (m_selArt.isEmpty() || !m_doc || !m_doc->info) return nullptr;
    for (Art& a : m_doc->info->user.arts) if (a.id == m_selArt) return &a;
    return nullptr;
}

void Canvas::setSelName(const QString& ref)
{
    if (m_selName == ref) return;
    m_selName = ref;
    emit selectionChanged();
    update();
}

void Canvas::cancelAction()
{
    m_roadFrom.reset();
    m_bridgeStart.reset();
    m_toolMsg.clear();
    m_trail.clear();
    emit selectionChanged();
    update();
}

// ── mouse ──
bool Canvas::toolPress(QMouseEvent* e)
{
    if (!m_doc || !infoOn() || !m_base) return false;
    if (!m_doc->info || m_doc->infoVer != m_doc->ver) waitIdle();
    m_doc->ensureInfo();
    const QPointF pos = e->position();
    if (m_tool == Tool::Realm) realmDown(pos, e->modifiers() & Qt::ShiftModifier);
    else if (m_tool == Tool::Build) buildDown(pos);
    else if (m_tool == Tool::Names) namesDown(pos);
    return true;
}

bool Canvas::toolMove(QMouseEvent* e)
{
    if (!m_doc || !m_doc->info) return false;
    const QPointF pos = e->position();
    const auto q = screenToWorld(pos);
    Info& in = *m_doc->info;
    if (m_tool == Tool::Realm) {
        if (m_seedDrag) { m_seedDrag->x = q[0]; m_seedDrag->y = q[1]; update(); return true; }
        if (!m_rstroke) return false;
        const double lx = m_lastWorld[0], ly = m_lastWorld[1], d = std::hypot(q[0] - lx, q[1] - ly), sp = std::max(2.0, m_opts.realmBrush * 0.3);
        if (d < sp) return true;
        const int n = static_cast<int>(std::ceil(d / sp));
        const bool neutral = m_opts.realmTool == QLatin1String("neutral");
        for (int i = 1; i <= n; i++) paintTerritory(*m_world, in, lx + (q[0] - lx) * i / n, ly + (q[1] - ly) * i / n, m_opts.realmBrush, neutral, m_opts.realmSel);
        m_lastWorld = { q[0], q[1] };
        m_trail.push_back(pos);
        update();
        return true;
    }
    if (m_tool == Tool::Build) {
        if (m_artDrag) {
            for (Art& a : in.user.arts) if (a.id == m_artDrag->id) { a.x = q[0] + m_artDrag->ox; a.y = q[1] + m_artDrag->oy; }
            m_artDrag->moved = true;
            in.user.ver++;
            update();
            return true;
        }
        if (!m_bstroke) return false;
        const QString& k = m_opts.buildKind;
        const double lx = m_lastWorld[0], ly = m_lastWorld[1], d = std::hypot(q[0] - lx, q[1] - ly);
        const bool line = k == QLatin1String("wall") || k == QLatin1String("road");
        const double sp = line ? 2 : std::max(2.0, m_opts.buildBrush * 0.4);
        if (d < sp) return true;
        if (line) m_trailWorld.push_back({ q[0], q[1] });
        else { const int n = static_cast<int>(std::ceil(d / sp)); for (int i = 1; i <= n; i++) buildDab(lx + (q[0] - lx) * i / n, ly + (q[1] - ly) * i / n); }
        m_lastWorld = { q[0], q[1] };
        m_trail.push_back(pos);
        update();
        return true;
    }
    if (m_tool == Tool::Names) {
        if (m_nameDrag) {
            NameDrag& nd = *m_nameDrag;
            const double dx = q[0] - nd.x0, dy = q[1] - nd.y0;
            if (!nd.moved && std::hypot(dx, dy) * m_zoom < 3) return true;
            LabelLayer& L = m_doc->labels;
            if (nd.edit < 0 && nd.custom < 0) {
                const Label* lab = m_scene.nameByRef(nd.ref);
                if (!lab) return true;
                Label copy = *lab;
                if (copy.autoName) { nd.edit = editFor(L, m_doc->names, copy); if (nd.edit < 0) return true; nd.bx = L.edits[nd.edit].dx; nd.by = L.edits[nd.edit].dy; }
                else { nd.custom = copy.custom; if (nd.custom < 0) return true; nd.bx = L.custom[nd.custom].x; nd.by = L.custom[nd.custom].y; nd.pts = L.custom[nd.custom].pts; }
            }
            nd.moved = true;
            if (nd.edit >= 0) { L.edits[nd.edit].dx = nd.bx + dx; L.edits[nd.edit].dy = nd.by + dy; }
            else {
                NameCustom& c = L.custom[nd.custom];
                c.x = nd.bx + dx; c.y = nd.by + dy;
                c.pts = nd.pts;
                for (Pt& p : c.pts) { p[0] += dx; p[1] += dy; }
            }
            update();
            return true;
        }
        if (m_nameNew && !m_trail.empty()) {
            const QPointF t = m_trail.back();
            const double d = std::hypot(pos.x() - t.x(), pos.y() - t.y());
            if (d < 4) return true;
            m_nameNewLen += d;
            m_trail.push_back(pos);
            m_trailWorld.push_back({ q[0], q[1] });
            update();
            return true;
        }
    }
    return false;
}

bool Canvas::toolRelease(QMouseEvent*)
{
    if (!m_doc || !m_doc->info) return false;
    Info& in = *m_doc->info;
    if (m_tool == Tool::Realm) {
        if (m_seedDrag) {
            const SeedDrag sd = *m_seedDrag;
            m_seedDrag.reset();
            waitIdle();
            if (sd.r < static_cast<int>(in.defs.size()) && sd.k < static_cast<int>(in.defs[sd.r].seeds.size())) in.defs[sd.r].seeds[sd.k] = { sd.x, sd.y };
            recomputeRealms(*m_world, in);
            layersDone(std::move(m_editBefore), true);
            return true;
        }
        if (m_rstroke) {
            m_rstroke = false;
            m_trail.clear();
            waitIdle();
            recomputeRealms(*m_world, in);
            layersDone(std::move(m_editBefore), true);
            emit selectionChanged();
            return true;
        }
        return false;
    }
    if (m_tool == Tool::Build) {
        if (m_artDrag) {
            const ArtDrag ad = *m_artDrag;
            m_artDrag.reset();
            if (ad.moved) { refreshOwners(in); layersDone(std::move(m_editBefore), false); }
            return true;
        }
        if (m_bstroke) {
            m_bstroke = false;
            const QString& k = m_opts.buildKind;
            if (k == QLatin1String("wall")) addWall(in, m_trailWorld);
            if (k == QLatin1String("road")) addRoad(in, m_trailWorld, m_opts.roadKind, true);
            in.user.ver++;
            m_trail.clear();
            m_trailWorld.clear();
            layersDone(std::move(m_editBefore), false);
            return true;
        }
        return false;
    }
    if (m_tool == Tool::Names) {
        if (m_nameDrag) {
            const bool moved = m_nameDrag->moved;
            m_nameDrag.reset();
            if (moved) layersDone(std::move(m_editBefore), false);
            emit selectionChanged();
            return true;
        }
        if (m_nameNew) {
            m_nameNew = false;
            const Path pts = m_trailWorld;
            const double len = m_nameNewLen;
            m_trail.clear(); m_trailWorld.clear();
            if (pts.empty()) return true;
            QString made;
            nameEdit([&](LabelLayer& L) {
                L.seq++;
                NameCustom c;
                c.id = QStringLiteral("c%1").arg(L.seq);
                c.style = m_opts.nameStyle;
                c.text = c.style == QLatin1String("water") ? tr("Novo mar") : tr("Novo nome");
                c.x = pts[0][0]; c.y = pts[0][1];
                if (len > 30 && pts.size() >= 3) {
                    // arrastou: o nome segue o traço
                    const Path p = resample(smoothPath(pts), 4);
                    double l = 0;
                    for (size_t i = 1; i < p.size(); i++) l += std::hypot(p[i][0] - p[i - 1][0], p[i][1] - p[i - 1][1]);
                    c.pts = p; c.x = p[p.size() >> 1][0]; c.y = p[p.size() >> 1][1]; c.r = std::max(50.0, l / 2);
                }
                L.custom.push_back(c);
                made = c.id;
                return true;
            });
            m_newName = true;
            m_selName.clear();
            setSelName(made);
            return true;
        }
    }
    return false;
}

// ── territórios ──
std::optional<std::array<int, 2>> Canvas::seedNear(QPointF s) const
{
    if (!m_doc || !m_doc->info) return std::nullopt;
    const SceneState st = sceneState();
    std::optional<std::array<int, 2>> best;
    double bd = 13;
    const auto& defs = m_doc->info->defs;
    for (size_t r = 0; r < defs.size(); r++) for (size_t k = 0; k < defs[r].seeds.size(); k++) {
        const auto g = m_scene.ground(st, defs[r].seeds[k][0], defs[r].seeds[k][1]);
        if (!g) continue;
        const double dd = std::hypot(g->X - s.x(), g->Y - s.y() + 16);       // o pino fica 16 px acima do chão
        if (dd < bd) { bd = dd; best = std::array<int, 2>{ static_cast<int>(r), static_cast<int>(k) }; }
    }
    return best;
}

void Canvas::realmDown(QPointF pos, bool shift)
{
    Info& in = *m_doc->info;
    const auto q = screenToWorld(pos);
    auto newRealm = [&](std::optional<Pt> at) {
        realmEdit([&](Info& info) {
            std::vector<bool> used(8, false);
            for (const RealmDef& d : info.defs) for (int c = 0; c < 8; c++) if (d.color == kRealmPalette[c]) used[c] = true;
            int ci = -1;
            for (int c = 0; c < 8; c++) if (!used[c]) { ci = c; break; }
            RealmDef d;
            d.name = tr("Território %1").arg(info.defs.size() + 1);
            d.color = kRealmPalette[ci >= 0 ? ci : static_cast<int>(info.defs.size() % 8)];
            if (at) d.seeds.push_back(*at);
            d.reach = 120;
            info.defs.push_back(d);
            m_opts.realmSel = static_cast<int>(info.defs.size()) - 1;
        });
    };
    if (m_opts.realmTool == QLatin1String("seed")) {
        const auto hit = seedNear(pos);
        if (hit && shift) {
            m_opts.realmSel = (*hit)[0];
            realmEdit([&](Info& info) { auto& s = info.defs[(*hit)[0]].seeds; s.erase(s.begin() + (*hit)[1]); });
            return;
        }
        if (hit) {
            m_opts.realmSel = (*hit)[0];
            emit selectionChanged();
            m_editBefore = std::make_shared<Doc::Layers>(m_doc->layers());
            m_seedDrag = SeedDrag{ (*hit)[0], (*hit)[1], q[0], q[1] };
            return;
        }
        if (in.defs.empty()) { newRealm(Pt{ q[0], q[1] }); return; }
        realmEdit([&](Info& info) { info.defs[std::min<int>(m_opts.realmSel, info.defs.size() - 1)].seeds.push_back({ q[0], q[1] }); });
        return;
    }
    if (in.defs.empty() && m_opts.realmTool == QLatin1String("paint")) newRealm(std::nullopt);
    m_editBefore = std::make_shared<Doc::Layers>(m_doc->layers());
    m_rstroke = true;
    m_lastWorld = { q[0], q[1] };
    m_trail = { pos };
    paintTerritory(*m_world, *m_doc->info, q[0], q[1], m_opts.realmBrush, m_opts.realmTool == QLatin1String("neutral"), m_opts.realmSel);
}

// ── construir ──
std::array<double, 2> Canvas::artExtent(const Art& a) const
{
    const double k = worldScale(*m_world);
    if (a.kind == QLatin1String("castle")) {
        const spr::CastleCfg& c = spr::kCastle[std::max(0, std::min(3, a.size))];
        return { (c.R * 1.25 + 1) * k, (c.kt[1] + c.kt[2] + 2) * k };
    }
    if (a.kind == QLatin1String("tower")) return { 2.6 * k, 11 * k };
    if (a.kind == QLatin1String("lighthouse")) return { 2.4 * k, 13 * k };
    if (a.kind == QLatin1String("windmill")) return { 4 * k, 9 * k };
    if (a.kind == QLatin1String("temple")) return { 4.6 * k, 6.5 * k };
    if (a.kind == QLatin1String("ruins")) return { 5.6 * k, 4 * k };
    return { 3 * k, 6 * k };
}

Art* Canvas::artNear(QPointF s)
{
    if (!m_doc || !m_doc->info) return nullptr;
    const SceneState st = sceneState();
    const double co = std::cos(m_pitch * M_PI / 180), sqv = sq();
    Art* best = nullptr;
    double bestArea = INFINITY;
    for (Art& a : m_doc->info->user.arts) {
        const auto g = m_scene.ground(st, a.x, a.y);
        if (!g) continue;
        const auto [r, h] = artExtent(a);
        const double x0 = g->X - r * m_zoom, x1 = g->X + r * m_zoom, y0 = g->Y - (h * co + r * sqv) * m_zoom, y1 = g->Y + r * sqv * m_zoom;
        if (s.x() < x0 || s.x() > x1 || s.y() < y0 || s.y() > y1) continue;
        const double area = (x1 - x0) * (y1 - y0);
        if (area < bestArea) { bestArea = area; best = &a; }        // o menor que contém o clique (torre na frente do castelo)
    }
    return best;
}

// a ponta da estrada gruda no que estiver perto: artefato, cidade sugerida ou outra estrada
std::optional<Pt> Canvas::snapRoad(QPointF s)
{
    if (!m_doc || !m_doc->info) return std::nullopt;
    const SceneState st = sceneState();
    const Info& info = *m_doc->info;
    std::optional<Pt> best;
    double bd = 14 * 14;
    auto test = [&](double x, double y) {
        const auto g = m_scene.ground(st, x, y);
        if (!g) return;
        const double d = std::pow(g->X - s.x(), 2) + std::pow(g->Y - s.y(), 2);
        if (d < bd) { bd = d; best = Pt{ x, y }; }
    };
    for (const Art& a : info.user.arts) test(a.x, a.y);
    for (const Road& r : info.user.roads) for (size_t i = 0; i < r.pts.size(); i += 2) test(r.pts[i][0], r.pts[i][1]);
    if (info.user.showGen) {
        for (const Town& t : info.towns) test(t.x, t.y);
        for (const Path& r : info.roads) for (size_t i = 0; i < r.size(); i += 3) test(r[i][0], r[i][1]);
    }
    return best;
}

void Canvas::startRoadAt(const Pt& p)
{
    m_roadFrom = std::make_unique<Router>(*m_world, *m_doc->info, p[0], p[1]);
    m_roadFromInfoVer = m_doc->infoVer;
    if (!m_roadFrom->ok()) { m_roadFrom.reset(); m_toolMsg = tr("Aqui é mar aberto: comece a estrada em terra."); }
    else m_toolMsg.clear();
    emit selectionChanged();
}

void Canvas::buildDab(double x, double y)
{
    Info& in = *m_doc->info;
    const QString& k = m_opts.buildKind;
    if (k == QLatin1String("village")) paintVillage(*m_world, in, x, y, m_opts.buildBrush);
    else if (k == QLatin1String("fields")) paintFieldsUser(*m_world, in, x, y, m_opts.buildBrush * 1.4);
    else if (k == QLatin1String("erase")) eraseUser(in, x, y, m_opts.buildBrush);
}

void Canvas::buildDown(QPointF pos)
{
    const auto q = screenToWorld(pos);
    const QString& kind = m_opts.buildKind;
    Art* hit = artNear(pos);
    if (hit && (isUnique(kind) || kind == QLatin1String("bridge") || kind == QLatin1String("select"))) {
        setSelArt(hit->id);
        m_editBefore = std::make_shared<Doc::Layers>(m_doc->layers());
        m_artDrag = ArtDrag{ hit->id, hit->x - q[0], hit->y - q[1], false };
        return;
    }
    if (kind == QLatin1String("select")) { setSelArt(QString()); return; }
    if (kind == QLatin1String("road") && m_opts.roadMode == QLatin1String("route")) {
        const auto snap = snapRoad(pos);
        const Pt p = snap ? *snap : Pt{ q[0], q[1] };
        if (!m_roadFrom || m_roadFromInfoVer != m_doc->infoVer) { startRoadAt(p); return; }
        Path path;
        if (!m_roadFrom->pathTo(p[0], p[1], path)) { m_toolMsg = tr("Não tem caminho por terra até aí (outra ilha?). Esc recomeça."); emit selectionChanged(); return; }
        const QString rk = m_opts.roadKind;
        userEdit([&](Info& in) { return addRoad(in, path, rk, false); });
        startRoadAt(p);                            // continua dali (ligar cidade a cidade)
        return;
    }
    if (isUnique(kind)) {
        QString made;
        const int size = m_opts.castleSize;
        userEdit([&](Info& in) { made = addArt(*m_world, in, kind, q[0], q[1], size).id; return true; });
        setSelArt(made);
        return;
    }
    if (kind == QLatin1String("bridge")) {
        if (!m_bridgeStart) { m_bridgeStart = Pt{ q[0], q[1] }; return; }
        const Pt a = *m_bridgeStart;
        m_bridgeStart.reset();
        userEdit([&](Info& in) { return addBridge(in, a, Pt{ q[0], q[1] }); });
        return;
    }
    setSelArt(QString());
    m_editBefore = std::make_shared<Doc::Layers>(m_doc->layers());
    m_bstroke = true;
    m_lastWorld = { q[0], q[1] };
    m_trailWorld = { { q[0], q[1] } };
    m_trail = { pos };
    buildDab(q[0], q[1]);
}

// ── nomes ──
void Canvas::namesDown(QPointF pos)
{
    const auto q = screenToWorld(pos);
    if (const Label* hit = m_scene.labelAt(pos)) {
        setSelName(hit->ref);
        m_editBefore = std::make_shared<Doc::Layers>(m_doc->layers());
        m_nameDrag = NameDrag{ hit->ref, q[0], q[1], false, -1, -1, 0, 0, {} };
        return;
    }
    setSelName(QString());
    m_nameNew = true;
    m_nameNewLen = 0;
    m_trail = { pos };
    m_trailWorld = { { q[0], q[1] } };
}

// ── régua: distância em léguas e quilômetros, e quanto tempo leva ──
// 1 px do mundo = 0,6 km na escala Normal (Menor e Mínima esticam o mundo); 1 légua = 6 km
double Canvas::leaguePx() const { return m_zoom * 10 * (m_world ? worldScale(*m_world) : 1.0); }

void Canvas::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (m_tool == Tool::Ruler && e->button() == Qt::LeftButton) {
        m_rulerDone = m_ruler.size() >= 2;
        update();
        return;
    }
    QWidget::mouseDoubleClickEvent(e);
}

namespace {
QString travelTime(double km, double kmPerDay)
{
    const double days = km / kmPerDay;
    const QLocale loc;
    if (days < 1) {
        const int h = std::max(1, static_cast<int>(std::lround(days * 10)));   // dia de viagem: 10 horas
        return QCoreApplication::translate("carto", "%1 h").arg(h);
    }
    const double d = std::round(days * 2) / 2;
    if (d == 1) return QCoreApplication::translate("carto", "1 dia");
    return QCoreApplication::translate("carto", "%1 dias").arg(loc.toString(d, 'f', d == std::floor(d) ? 0 : 1));
}
}

void Canvas::drawRuler(QPainter& p, const SceneState& st)
{
    Path pts = m_ruler;
    if (!m_rulerDone && m_hasHover && !pts.empty()) { const auto q = screenToWorld(m_hover); pts.push_back({ q[0], q[1] }); }
    if (pts.empty()) return;
    // na tela: em cima do chão (o relevo ergue o ponto); fora da pintura, no plano
    auto screen = [&](const Pt& q) {
        if (const auto g = m_scene.ground(st, q[0], q[1])) return QPointF(g->X, g->Y);
        const auto pl = worldToPlane(*m_world, m_ang, q[0], q[1]);
        return QPointF(m_panX + m_zoom * pl[0], m_panY + m_zoom * sq() * pl[1]);
    };
    double len = 0, wet = 0, samples = 0;
    QPainterPath path;
    for (size_t i = 0; i < pts.size(); i++) {
        const QPointF s = screen(pts[i]);
        if (i) path.lineTo(s); else path.moveTo(s);
        if (!i) continue;
        const double d = std::hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]);
        len += d;
        for (double t = 0; t < d; t += 4) {
            const double f = t / d;
            if (heightAt(*m_world, pts[i - 1][0] + (pts[i][0] - pts[i - 1][0]) * f, pts[i - 1][1] + (pts[i][1] - pts[i - 1][1]) * f) <= 0) wet++;
            samples++;
        }
    }
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QPen under(QColor(20, 14, 8, 150), 5); under.setCapStyle(Qt::RoundCap); under.setJoinStyle(Qt::RoundJoin);
    p.strokePath(path, under);
    p.strokePath(path, dashPen(QColor(250, 236, 200, 245), 2, 7, 4));
    for (size_t i = 0; i < pts.size(); i++) {
        const QPointF s = screen(pts[i]);
        p.setPen(QPen(QColor(20, 14, 8, 200), 1.5));
        p.setBrush(i + 1 == pts.size() && !m_rulerDone ? QColor(250, 236, 200) : QColor(214, 180, 120));
        p.drawEllipse(s, 4.5, 4.5);
    }
    if (len > 0) {
        const QLocale loc;
        const double km = len * 0.6 / (m_world ? worldScale(*m_world) : 1.0), lg = km / 6;
        const QString l1 = tr("%1 léguas · %2 km").arg(loc.toString(lg, 'f', lg < 10 ? 1 : 0), loc.toString(km, 'f', km < 10 ? 1 : 0));
        const QString l2 = samples > 0 && wet / samples > 0.5 ? tr("de barco ~%1").arg(travelTime(km, 150))
                                                               : tr("a pé ~%1 · a cavalo ~%2").arg(travelTime(km, 30), travelTime(km, 60));
        QFont f = font();
        f.setPixelSize(12);
        f.setBold(true);
        QFont f2 = font();
        f2.setPixelSize(11);
        const QFontMetrics fm(f), fm2(f2);
        const int w = std::max(fm.horizontalAdvance(l1), fm2.horizontalAdvance(l2)) + 20, h = fm.height() + fm2.height() + 12;
        const QPointF end = screen(pts.back());
        QRectF box(end.x() + 14, end.y() - h - 8, w, h);
        if (box.right() > width() - 8) box.moveRight(end.x() - 14);
        if (box.top() < 8) box.moveTop(end.y() + 10);
        p.setPen(QPen(QColor(250, 236, 200, 90), 1));
        p.setBrush(QColor(22, 18, 14, 225));
        p.drawRoundedRect(box, 8, 8);
        p.setPen(QColor(250, 240, 220));
        p.setFont(f);
        p.drawText(QPointF(box.left() + 10, box.top() + 6 + fm.ascent()), l1);
        p.setPen(QColor(220, 206, 180));
        p.setFont(f2);
        p.drawText(QPointF(box.left() + 10, box.top() + 6 + fm.height() + fm2.ascent()), l2);
    }
    p.restore();
}

// ── o que as ferramentas desenham por cima ──
void Canvas::drawToolOverlay(QPainter& p, const SceneState& st)
{
    if (!m_doc || !m_doc->info || !infoOn()) return;
    const Info& info = *m_doc->info;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const bool hovering = m_hasHover && m_drag != Drag::Pan && m_drag != Drag::Turn;
    auto cursorRing = [&](double r, const QColor& c) {
        if (!hovering) return;
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 0, 0, 115), 3));
        p.drawEllipse(m_hover, r * m_zoom, r * m_zoom * sq());
        p.setPen(QPen(c, 1.6));
        p.drawEllipse(m_hover, r * m_zoom, r * m_zoom * sq());
    };
    if (m_tool == Tool::Realm) {
        // rastro da pincelada enquanto pinta (o território é recalculado ao soltar)
        if (m_rstroke && !info.defs.empty()) {
            const RGB c = m_opts.realmTool == QLatin1String("neutral") ? RGB{ 210, 210, 210 } : info.defs[std::min<int>(m_opts.realmSel, info.defs.size() - 1)].color;
            p.setPen(Qt::NoPen);
            p.setBrush(rgbC(c, 71));
            for (const QPointF& t : m_trail) p.drawEllipse(t, m_opts.realmBrush * m_zoom, m_opts.realmBrush * m_zoom * sq());
        }
        // sementes: pino na cor do território
        for (size_t r = 0; r < info.defs.size(); r++) for (size_t k = 0; k < info.defs[r].seeds.size(); k++) {
            const bool dragging = m_seedDrag && m_seedDrag->r == static_cast<int>(r) && m_seedDrag->k == static_cast<int>(k);
            const Pt sp = info.defs[r].seeds[k];
            const auto g = m_scene.ground(st, dragging ? m_seedDrag->x : sp[0], dragging ? m_seedDrag->y : sp[1]);
            if (!g) continue;
            const bool sel = static_cast<int>(r) == m_opts.realmSel;
            p.setPen(QPen(QColor(20, 16, 12, 217), 1.5));
            p.drawLine(QPointF(g->X, g->Y), QPointF(g->X, g->Y - 16));
            p.setBrush(rgbC(info.defs[r].color));
            p.setPen(QPen(sel ? QColor(250, 246, 236) : QColor(20, 16, 12, 217), sel ? 2.5 : 1.5));
            p.drawEllipse(QPointF(g->X, g->Y - 18), sel ? 7.0 : 5.5, sel ? 7.0 : 5.5);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 89));
            p.drawEllipse(QPointF(g->X, g->Y), 3, 1.4);
        }
        if (m_opts.realmTool != QLatin1String("seed")) {
            const RGB c = m_opts.realmTool == QLatin1String("neutral") || info.defs.empty() ? RGB{ 236, 232, 222 }
                                                                                            : info.defs[std::min<int>(m_opts.realmSel, info.defs.size() - 1)].color;
            cursorRing(m_opts.realmBrush, rgbC(c));
        }
    } else if (m_tool == Tool::Build) {
        const QString& kind = m_opts.buildKind;
        for (const Art& a : info.user.arts) if (a.id == m_selArt) {
            const auto g = m_scene.ground(st, a.x, a.y);
            const double rr = artExtent(a)[0];
            if (g) { p.setBrush(Qt::NoBrush); p.setPen(dashPen(QColor(250, 236, 200, 242), 2, 5, 4)); p.drawEllipse(QPointF(g->X, g->Y), rr * m_zoom, rr * m_zoom * sq()); }
        }
        if (m_bridgeStart) {
            const auto g = m_scene.ground(st, (*m_bridgeStart)[0], (*m_bridgeStart)[1]);
            if (g) {
                p.setBrush(QColor(214, 180, 120)); p.setPen(QPen(QColor(40, 30, 22), 1.5));
                p.drawEllipse(QPointF(g->X, g->Y), 5, 5);
                if (m_hasHover) { p.setPen(dashPen(QColor(240, 220, 180, 230), 1, 6, 4)); p.drawLine(QPointF(g->X, g->Y), m_hover); }
            }
        }
        if (kind == QLatin1String("road") && m_opts.roadMode == QLatin1String("route") && hovering) {
            // prévia: o caminho que a estrada vai fazer até o cursor
            const auto snap = snapRoad(m_hover);
            if (snap) { const auto g = m_scene.ground(st, (*snap)[0], (*snap)[1]); if (g) { p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor(250, 236, 200, 242), 1.6)); p.drawEllipse(QPointF(g->X, g->Y), 7, 7); } }
            if (m_roadFrom && m_roadFromInfoVer == m_doc->infoVer) {
                const auto w = screenToWorld(m_hover);
                const Pt to = snap ? *snap : Pt{ w[0], w[1] };
                Path path;
                const bool ok = m_roadFrom->pathTo(to[0], to[1], path);
                const auto g0 = m_scene.ground(st, m_roadFrom->x(), m_roadFrom->y());
                if (g0) { p.setBrush(QColor(214, 180, 120)); p.setPen(QPen(QColor(40, 30, 22), 1.5)); p.drawEllipse(QPointF(g0->X, g0->Y), 5, 5); }
                if (ok) {
                    QPainterPath pp; bool on = false;
                    for (const Pt& q : path) { const auto g = m_scene.ground(st, q[0], q[1]); if (!g) { on = false; continue; } if (on) pp.lineTo(g->X, g->Y); else { pp.moveTo(g->X, g->Y); on = true; } }
                    QPen under(QColor(20, 14, 8, 140), 5); under.setCapStyle(Qt::RoundCap); under.setJoinStyle(Qt::RoundJoin);
                    p.strokePath(pp, under);
                    p.strokePath(pp, dashPen(QColor(250, 236, 200, 242), 2, 6, 4));
                } else if (g0) {
                    p.setPen(dashPen(QColor(232, 140, 120, 204), 1.5, 3, 4));
                    p.drawLine(QPointF(g0->X, g0->Y), m_hover);
                }
            }
        }
        if (m_bstroke && (kind == QLatin1String("wall") || kind == QLatin1String("road")) && m_trail.size() > 1) {
            QPainterPath pp; pp.moveTo(m_trail[0]);
            for (size_t i = 1; i < m_trail.size(); i++) pp.lineTo(m_trail[i]);
            p.strokePath(pp, dashPen(QColor(240, 232, 214, 230), 3, 6, 4));
        }
        if (kind == QLatin1String("village") || kind == QLatin1String("fields") || kind == QLatin1String("erase"))
            cursorRing(m_opts.buildBrush * (kind == QLatin1String("fields") ? 1.4 : 1), kind == QLatin1String("erase") ? QColor(232, 140, 120) : QColor(240, 232, 214));
    } else if (m_tool == Tool::Names) {
        if (m_nameNew && m_trail.size() >= 2) {
            QPainterPath pp; pp.moveTo(m_trail[0]);
            for (size_t i = 1; i < m_trail.size(); i++) pp.lineTo(m_trail[i]);
            p.strokePath(pp, dashPen(m_mode == QLatin1String("full") ? QColor(240, 232, 214, 230) : QColor(40, 33, 28, 179), 2, 6, 4));
        }
    }
    p.restore();
}

} // namespace carto
