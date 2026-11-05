#include "GalaxyCanvas.h"
#include <QPainterPath>
#include "GalaxyModels.h"

#include <QCoreApplication>
#include <QMouseEvent>
#include <QRandomGenerator>
#include <QWheelEvent>

#include <algorithm>

namespace galaxy {

namespace {
constexpr double MAX_TILT = 70 * PI_D / 180;
inline double sizeK(const QString& s) { return s == QLatin1String("p") ? 0.65 : s == QLatin1String("g") ? 2 : s == QLatin1String("c") ? 3 : 1; }
inline uint32_t randSeed() { return QRandomGenerator::global()->generate(); }
inline double randAngle() { return QRandomGenerator::global()->generateDouble() * TAU; }
inline double orbitSpeed(double rr, bool retro = false) { return SPEED_PLANET * std::pow(0.35 / rr, 1.5) * (retro ? -1 : 1); }
} // namespace

// ── entrada ──
QString Canvas::pickAt(double x, double y)
{
    QString best, onLabel;
    double bd = 1e18;
    for (const Hit& h : m_hits) {
        if (h.label) {
            if (h.key.startsWith(QLatin1Char('L')) && tool != Tool::Names) continue;
            if (onLabel.isEmpty() && h.box.contains(QPointF(x, y))) onLabel = h.key;
            continue;
        }
        const double d = std::hypot(h.x - x, h.y - y);
        if (d < h.r && d < bd) { bd = d; best = h.key; }
    }
    if (best.isEmpty() && !onLabel.isEmpty()) return onLabel;
    if (best.isEmpty()) for (const RingHit& q : m_ringHits) {
        // O anel orbital tem inclinação própria na tela: desfaz antes de medir.
        const double c = std::cos(q.roll), n = std::sin(q.roll), dx = x - q.x, dy = y - q.y, ax = dx * c + dy * n, ay = -dx * n + dy * c;
        const double d = std::hypot(ax, ay / std::max(0.2, q.cT));
        if (d >= q.r0 && d <= q.r1) return q.key;
    }
    if (!best.isEmpty()) return best;
    return !focusKey.isEmpty() || (tool != Tool::Explore && tool != Tool::Edit) ? QString() : regionAt(x, y);
}
void Canvas::mousePressEvent(QMouseEvent* e)
{
    const double mx = e->position().x(), my = e->position().y();
    // Como no Mapa: o botão direito gira e inclina; o do meio, ou Alt, sempre move; o esquerdo move em Mover e Explorar.
    const bool left = e->button() == Qt::LeftButton, alt = e->modifiers() & Qt::AltModifier;
    const bool orbit = e->button() == Qt::RightButton;
    const bool pan = e->button() == Qt::MiddleButton || (left && (alt || tool == Tool::Pan || tool == Tool::Explore || tool == Tool::Edit));
    emit pressed();
    setPeek(QString());
    auto d = std::make_unique<Drag>();
    d->x = mx; d->y = my; toWorld(mx, my, d->wx, d->wy); d->orbit = orbit; d->pan = pan; d->tilt = cam.tilt; d->yaw = cam.yaw; d->lx = mx; d->ly = my;
    if (!pan && !orbit && left) {
        if (tool == Tool::Build) movableAt(mx, my, *d);
        if (tool == Tool::Names) {
            for (const Hit& h : m_hits) if (h.label && h.key.startsWith(QLatin1Char('L')) && h.box.contains(QPointF(mx, my))) { for (FreeLabel& l : doc->labels) if (l.key == h.key) d->label = &l; break; }
        }
        d->paint = tool == Tool::Realm && realmMode != RealmMode::Seed;
    }
    if (pan && left && !alt) for (const LiveHit& h : m_liveHits) if (std::hypot(h.x - mx, h.y - my) < h.r) { d->sphere = h.b; d->piece = h.build; break; }
    m_drag = std::move(d);
    if (m_drag->paint) realmPaintAt(mx, my);
    m_fly.reset(); m_tiltAnim.reset();
    m_dirty = true;
}
void Canvas::mouseMoveEvent(QMouseEvent* e)
{
    const double mx = e->position().x(), my = e->position().y();
    m_pointer = QPointF(mx, my); m_hasPointer = true;
    if (tool == Tool::Realm && realmMode != RealmMode::Seed) m_dirty = true;
    if (tool == Tool::Ruler && !rulerPts.empty() && !rulerDone) m_dirty = true;
    if (m_drag) {
        Drag& d = *m_drag;
        const double dx = mx - d.x, dy = my - d.y;
        if (std::hypot(dx, dy) >= (d.orbit ? 5 : 3)) { d.moved = true; if (d.pan || d.orbit || d.sphere || d.piece) setCursor(Qt::ClosedHandCursor); }
        if (!d.moved) return;
        if (!d.moveKey.isEmpty()) { moveDrag(mx, my); m_dirty = true; return; }
        if (d.label) { toWorld(mx, my, d.label->x, d.label->y); m_dirty = true; return; }
        if (d.paint) { realmPaintAt(mx, my); m_dirty = true; return; }
        if (d.sphere) {
            Body* b = d.sphere;
            b->spinU += (mx - d.lx) * 0.01;
            b->pitchU = std::max(-1.3, std::min(1.3, b->pitchU + (my - d.ly) * 0.007));
            b->spunByUser = true;
            d.lx = mx; d.ly = my;
        } else if (d.piece) {
            d.piece->spinU += (mx - d.lx) * 0.01;
            d.piece->pitchU = std::max(-1.2, std::min(1.2, d.piece->pitchU + (my - d.ly) * 0.007));
            d.lx = mx; d.ly = my;
        } else if (d.orbit) {
            cam.yaw = d.yaw - dx * 0.006;
            cam.tilt = std::max(0.0, std::min(MAX_TILT, d.tilt + dy * 0.25 * PI_D / 180));
            emit cameraMoved();
        } else if (d.pan) {
            cam.follow.clear();
            double w1x, w1y;
            toWorld(mx, my, w1x, w1y);
            cam.x += d.wx - w1x; cam.y += d.wy - w1y;
            emit cameraMoved();
        }
        m_dirty = true;
        return;
    }
    if (placing) { updateGhost(mx, my); return; }
    // Em Mover o mapa é só mapa: nada acende nem abre.
    const QString h = tool == Tool::Pan || tool == Tool::Ruler ? QString() : pickAt(mx, my);
    if (h != m_hover) { m_hover = h; m_dirty = true; }
    // A prévia não mexe no mapa: em Mover nada acende, mas o card aparece.
    setPeek(tool == Tool::Pan ? pickAt(mx, my) : tool == Tool::Explore || tool == Tool::Edit ? h : QString());
    bool overSphere = false;
    if (tool == Tool::Pan || tool == Tool::Explore || tool == Tool::Edit) for (const LiveHit& q : m_liveHits) if (std::hypot(q.x - mx, q.y - my) < q.r) { overSphere = true; break; }
    setCursor(overSphere ? Qt::OpenHandCursor : !h.isEmpty() || !linkAt(mx, my).isEmpty() ? Qt::PointingHandCursor : Qt::ArrowCursor);
    emit tipChanged(h, e->position());
}
void Canvas::mouseReleaseEvent(QMouseEvent* e)
{
    const double mx = e->position().x(), my = e->position().y();
    if (!m_drag) return;
    std::unique_ptr<Drag> dp = std::move(m_drag);
    Drag& d = *dp;
    const bool was = d.moved;
    unsetCursor();
    if (d.paint) { d.painted.swap(m_painted); realmPaintEnd(); if (was) return; }
    if (was && !d.moveKey.isEmpty()) { m_drag = std::move(dp); moveDrop(); m_drag.reset(); return; }
    if (was) { if (d.label) emit docEdited(); return; }
    if (e->button() == Qt::RightButton) { emit requestRing(QPointF(mx, my), QString()); return; }
    if (e->button() != Qt::LeftButton || (d.pan && tool == Tool::Pan)) return;
    if (tool == Tool::Build) { buildClick(mx, my); return; }
    if (tool == Tool::Realm) { if (realmMode == RealmMode::Seed) realmClick(mx, my, e->modifiers() & Qt::ShiftModifier); return; }
    if (tool == Tool::Names) { const QString k = pickAt(mx, my); emit nameClickRequested(mx, my, k, !k.isEmpty() && k.startsWith(QLatin1Char('L'))); return; }
    if (tool == Tool::Ruler) { rulerClick(mx, my); return; }
    if (pickingLink) { const QString h = pickAt(mx, my); if (!h.isEmpty()) emit linkPicked(h); return; }
    if (const QString id = linkAt(mx, my); !id.isEmpty()) { emit linkClicked(id); return; }
    const QString h = pickAt(mx, my);
    if (!h.isEmpty()) { select(h, true); emit clickedPlace(h); }
    else if (!focusKey.isEmpty() && !inFocusArea(mx, my)) { setFocus2(QString()); emit selectionChanged(sel); }
}
// ── conexões ──
bool Canvas::anchorOf(const QString& key, double& x, double& y, double& h) const
{
    const Ref o = doc->resolve(key);
    if (o.kind == RefKind::System) { x = o.sys->x; y = o.sys->y; h = o.sys->layH; return true; }
    if (o.kind != RefKind::Region || o.region->systems.isEmpty()) return false;
    double x0 = 9, x1 = -9, y0 = 9, y1 = -9;
    for (int id : o.region->systems) { const System& s = *doc->gal->systems[size_t(id)]; x0 = std::min(x0, s.x); x1 = std::max(x1, s.x); y0 = std::min(y0, s.y); y1 = std::max(y1, s.y); }
    x = (x0 + x1) / 2; y = (y0 + y1) / 2; h = 0;
    return true;
}
void Canvas::setLinks(std::vector<LinkView> links)
{
    m_links = std::move(links);
    m_linkHits.clear();
    m_dirty = true;
    update();
}
QString Canvas::linkAt(double x, double y) const
{
    if (tool != Tool::Explore && tool != Tool::Edit && tool != Tool::Pan) return QString();
    for (const auto& [box, id] : m_linkHits) if (box.contains(QPointF(x, y))) return id;
    return QString();
}
void Canvas::drawLinks(QPainter& p)
{
    m_linkHits.clear();
    if (m_links.empty()) return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    QFont lf = font(); lf.setPixelSize(12); lf.setWeight(QFont::DemiBold);
    const QFontMetricsF lfm(lf);
    p.setFont(lf);
    for (const LinkView& l : m_links) {
        double ax, ay, ah, bx, by, bh, sax, say, sbx, sby;
        if (!anchorOf(l.a, ax, ay, ah) || !anchorOf(l.b, bx, by, bh)) continue;
        proj(ax, ay, ah, sax, say);
        proj(bx, by, bh, sbx, sby);
        const QPointF A(sax, say), B(sbx, sby), mid = (A + B) / 2;
        const double d = std::hypot(sbx - sax, sby - say);
        const QPointF ctrl(mid.x(), mid.y() - std::min(140.0, d * 0.28));
        QPainterPath arc(A);
        arc.quadTo(ctrl, B);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(10, 12, 14, 153), 6, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(arc);
        p.setPen(QPen(l.color, 2.6, l.solid ? Qt::SolidLine : Qt::DashLine, Qt::RoundCap));
        p.drawPath(arc);
        for (const QPointF& E : { A, B }) {
            p.setPen(QPen(QColor(252, 248, 238), 2));
            p.setBrush(l.color);
            p.drawEllipse(E, 5, 5);
        }
        const QPointF at = 0.25 * A + 0.5 * ctrl + 0.25 * B;
        const QString txt = lfm.elidedText(l.label, Qt::ElideRight, 260);
        const double w = lfm.horizontalAdvance(txt) + 28;
        const QRectF box(at.x() - w / 2, at.y() - 13, w, 26);
        p.setPen(QPen(l.color, 2));
        p.setBrush(QColor(252, 248, 238, 245));
        p.drawRoundedRect(box, 13, 13);
        p.setPen(QColor(30, 26, 22));
        p.drawText(box, Qt::AlignCenter, txt);
        m_linkHits.push_back({ box, l.id });
    }
    p.restore();
}
// Leva o mapa até os dois lados caberem na faixa livre entre top e bottom.
void Canvas::frameLink(const QString& a, const QString& b, double top, double bottom)
{
    double ax, ay, ah, bx, by, bh;
    if (!anchorOf(a, ax, ay, ah) || !anchorOf(b, bx, by, bh)) return;
    const double cy = std::cos(cam.yaw), sy = std::sin(cam.yaw), cT = std::max(0.2, std::cos(cam.tilt));
    const double dx = bx - ax, dy = by - ay, X = std::fabs(dx * cy - dy * sy), Y = std::fabs(dx * sy + dy * cy) * cT;
    const double band = std::max(80.0, bottom - top);
    // o arco sobe acima dos dois pontos: a altura da faixa não pode ir toda pra distância
    const double z = std::clamp(std::min({ cam.z, (width() - 240) / std::max(1e-4, X), (band * 0.55) / std::max(1e-4, Y) }), zMin(), zMax());
    const double bandY = (top + bottom) / 2 + band * 0.12, Yr = (viewCenter().y() - bandY) / (cT * z);
    flyTo((ax + bx) / 2 - Yr * sy, (ay + by) / 2 - Yr * cy, z);
}
void Canvas::mouseDoubleClickEvent(QMouseEvent*)
{
    if (tool == Tool::Ruler && rulerPts.size() > 1) { rulerDone = true; m_dirty = true; }
}
void Canvas::leaveEvent(QEvent*)
{
    m_hasPointer = false;
    emit tipChanged(QString(), QPointF());
    setPeek(QString());
}
void Canvas::wheelEvent(QWheelEvent* e)
{
    e->accept();
    setPeek(QString());
    m_fly.reset();
    m_lastWheelT = m_clock.elapsed();
    const double delta = e->angleDelta().y() != 0 ? e->angleDelta().y() : e->angleDelta().x();
    // Shift + roda gira o mapa, como no Mapa.
    if (e->modifiers() & Qt::ShiftModifier) { cam.yaw += delta * 0.0025; m_tiltAnim.reset(); m_dirty = true; emit cameraMoved(); return; }
    const double mx = e->position().x(), my = e->position().y();
    double wx, wy;
    toWorld(mx, my, wx, wy);
    cam.z = std::min(zMax(), std::max(zMin(), cam.z * std::pow(1.0018, delta / 1.2)));
    // Seguindo um planeta, o zoom é no centro (senão ele sairia de vista).
    if (cam.follow.isEmpty()) { double wx2, wy2; toWorld(mx, my, wx2, wy2); cam.x += wx - wx2; cam.y += wy - wy2; }
    m_dirty = true;
    emit cameraMoved();
}

// ── desfazer ──
void Canvas::pushUndo(const QString& label, std::function<void()> undoFn, std::function<void()> redoFn)
{
    undoStack.push_back({ std::move(undoFn), std::move(redoFn), label });
    redoStack.clear();
    if (undoStack.size() > 60) undoStack.erase(undoStack.begin());
    emit toast(label, true);
    emit historyChanged();
    emit docEdited();
}
void Canvas::afterHistory()
{
    m_habCache.clear();
    m_tr.key.clear(); m_tr.sys.clear(); m_fx.key.clear(); m_prims.clear();
    if (!doc->resolve(sel)) sel = QStringLiteral("GAL");
    m_dirty = true;
    emit toast(QString(), false);
    emit selectionChanged(sel);
    emit historyChanged();
    emit docEdited();
    update();
}
void Canvas::undo()
{
    if (undoStack.empty()) return;
    UndoEntry e = std::move(undoStack.back());
    undoStack.pop_back();
    e.undo();
    if (e.redo) redoStack.push_back(std::move(e));
    afterHistory();
}
void Canvas::redo()
{
    if (redoStack.empty()) return;
    UndoEntry e = std::move(redoStack.back());
    redoStack.pop_back();
    e.redo();
    undoStack.push_back(std::move(e));
    afterHistory();
}

// ── colocar peças ──
void Canvas::armItem(const PlaceItem* it)
{
    placing = it ? std::make_unique<PlaceItem>(*it) : nullptr;
    m_hasGhost = false;
    setCursor(it ? Qt::CrossCursor : Qt::ArrowCursor);
    emit hintChanged();
    m_dirty = true;
}
// Onde a peça cairia com o ponteiro aqui.
Snap Canvas::snapFor(const PlaceItem& it, double mx, double my)
{
    Snap G;
    G.sx = mx; G.sy = my;
    const double z = cam.z, cT = std::max(0.2, std::cos(cam.tilt));
    const StructDef* def = it.kind == ItemKind::Struct ? structOf(it.id) : nullptr;
    // De uma diferença na tela pro ângulo na órbita: desfaz a inclinação e o giro da câmera.
    const auto worldAng = [&](double dx, double dy) { return std::atan2(-dy / cT, dx) - cam.yaw; };
    if (it.kind == ItemKind::Star) {
        double wx, wy;
        toWorld(mx, my, wx, wy);
        if (std::hypot(wx, wy) > EXT * 1.05) { G.msg = tr("Longe demais da galáxia."); return G; }
        for (const auto& s : doc->gal->systems) if (!s->gone && std::hypot(s->x - wx, s->y - wy) < 0.022) { G.msg = tr("Perto demais de outro sistema."); return G; }
        G.ok = true; G.wx = wx; G.wy = wy;
        return G;
    }
    // O sistema aberto embaixo do ponteiro.
    System* s = nullptr;
    double bd = 1e18, bx = 0, by = 0, bpx = 0;
    for (const auto& q : doc->gal->systems) {
        if (q->gone) continue;
        const double Rpx = q->layR * z;
        if (Rpx < 36) continue;
        double x, y;
        proj(q->x, q->y, q->layH, x, y);
        const double d = std::hypot(x - mx, (y - my) / cT);
        if (d < Rpx * 1.25 && d < bd) { bd = d; s = q.get(); bx = x; by = y; bpx = Rpx; }
    }
    if (!s) {
        if (def && def->deep) {
            toWorld(mx, my, G.wx, G.wy);
            G.ok = true; G.deep = true; G.msg = tr("No espaço aberto, fora de qualquer sistema.");
            return G;
        }
        const QString h = pickAt(mx, my);
        G.msg = h.startsWith(QLatin1Char('S')) ? tr("Clique pra chegar perto de %1.").arg(doc->labelOf(h)) : tr("Aproxime até um sistema abrir.");
        return G;
    }
    if (s->isCore) { G.msg = tr("O núcleo da galáxia não aceita construção."); return G; }
    // Perto de um planeta?
    Body* hostP = nullptr;
    double hd = 1e18, hpx = 0, hpy = 0, hpr = 0;
    const bool wantsPlanet = it.kind == ItemKind::Moon || (def && def->host != Host::Star);
    if (wantsPlanet) for (Body* p : planetsOf(*s)) {
        double px, py;
        proj(p->layX, p->layY, s->layH, px, py);
        const double pr = std::max(2.2, p->layR * z), d = std::hypot(px - mx, (py - my) / cT);
        if (d < std::max(pr * 2.4, 24.0) && d < hd) { hd = d; hostP = p; hpx = px; hpy = py; hpr = pr; }
    }
    const auto builds = doc->buildsIn(*s);
    if (it.kind == ItemKind::Moon || (def && def->host == Host::Planet)) {
        G.sys = s;
        if (!hostP) { G.msg = tr("Precisa ser em cima de um planeta."); return G; }
        const int uid = hostP->uid ? hostP->uid : hostP->idx;
        if (def && def->one) for (Build* b : builds) if (b->type == it.id && b->host == uid) { G.msg = tr("%1 já tem um.").arg(doc->labelOf(keyP(*s, *hostP))); return G; }
        if (it.kind == ItemKind::Moon && isArtificial(hostP->type)) { G.msg = tr("Mundo construído não prende lua."); return G; }
        G.ok = true; G.host = hostP; G.hx = hpx; G.hy = hpy; G.hr = hpr; G.ang = worldAng(mx - hpx, my - hpy); G.sang = std::atan2(my - hpy, mx - hpx);
        G.msg = tr("Em volta de %1.").arg(doc->labelOf(keyP(*s, *hostP)));
        return G;
    }
    if (def && def->one) for (Build* b : builds) if (b->type == it.id && b->host < 0) { G.msg = tr("%1 já tem um.").arg(doc->labelOf(keyS(*s))); return G; }
    G.sys = s;
    if (hostP && def && def->host == Host::Any) {
        G.ok = true; G.host = hostP; G.hx = hpx; G.hy = hpy; G.hr = hpr; G.rp = std::max(1.5, std::min(5.0, hd / hpr)); G.ang = worldAng(mx - hpx, my - hpy);
        G.msg = tr("Em volta de %1.").arg(doc->labelOf(keyP(*s, *hostP)));
        return G;
    }
    G.ok = true; G.cx = bx; G.cy = by; G.rr = std::max(0.2, std::min(1.2, bd / bpx)); G.ang = worldAng(mx - bx, my - by);
    G.msg = tr("Em órbita de %1.").arg(doc->labelOf(keyS(*s)));
    return G;
}
void Canvas::updateGhost(double mx, double my)
{
    if (!placing) return;
    m_ghost = snapFor(*placing, mx, my);
    m_ghost.mx = mx; m_ghost.my = my;
    m_hasGhost = true;
    emit hintChanged();
    m_dirty = true;
}
// Os corpos de exemplo saem do mesmo gerador dos planetas de verdade.
Body* Canvas::previewBody(const PlaceItem& it)
{
    auto found = m_previews.find(it.id);
    if (found != m_previews.end()) return found->second.body;
    System* base = nullptr;
    for (const auto& s : doc->gal->systems) if (!s->isCore && s->hasHz && s->stars[0].kind == StarKind::G) { base = s.get(); break; }
    if (!base) for (const auto& s : doc->gal->systems) if (!s->isCore && s->hasHz) { base = s.get(); break; }
    if (!base) base = doc->gal->systems[std::min<size_t>(1, doc->gal->systems.size() - 1)].get();
    auto tmp = std::make_shared<System>();
    tmp->id = base->id; tmp->seed = base->seed; tmp->stars = base->stars; tmp->M = base->M; tmp->L = base->L; tmp->age = base->age; tmp->hasHz = base->hasHz; tmp->hz[0] = base->hz[0]; tmp->hz[1] = base->hz[1]; tmp->frost = base->frost; tmp->frostEff = base->frostEff; tmp->hasFrost = base->hasFrost; tmp->kind = base->kind;
    uint32_t seed = 7;
    for (char c : it.id) seed = seed * 31 + uint8_t(c);
    Type t = Type::Arido;
    typeFromKey(it.id, t);
    Body* b = nullptr;
    if (it.kind == ItemKind::Moon) { Body* p = addPlanet(*tmp, Type::Arido, 1, seed + 1); b = addMoon(*tmp, *p, t, seed); }
    else b = addPlanet(*tmp, t, tmp->hasHz ? (tmp->hz[0] + tmp->hz[1]) / 2 : 1, seed);
    m_previews[it.id] = { tmp, b };
    return b;
}
void Canvas::drawGhost(QPainter& p, double z, double cT)
{
    if (!placing || !m_hasGhost) return;
    const Snap& G = m_ghost;
    const PlaceItem& it = *placing;
    System* s = G.sys;
    const double sa = G.ang + cam.yaw, simT = doc->simT, scale = sizeK(buildSize);
    p.save();
    if (!G.ok) {
        QPen pen(QColor(255, 140, 120, 204)); pen.setWidthF(1.3); pen.setDashPattern({ 3 / 1.3, 4 / 1.3 });
        p.setPen(pen); p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(G.mx, G.my), 10, 10);
        p.restore();
        return;
    }
    if (it.kind == ItemKind::Star) {
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.drawImage(QRectF(G.mx - 16, G.my - 16, 32, 32), glow({ 255, 226, 170 }));
        p.restore();
        return;
    }
    if (s && !G.host && G.rr != 0) {
        QPen pen(QColor(140, 200, 255, 178)); pen.setWidthF(1); pen.setDashPattern({ 4, 5 });
        p.setPen(pen); p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(G.cx, G.cy), G.rr * s->layR * z, std::max(0.5, G.rr * s->layR * z * cT));
    }
    const StructDef* def = it.kind == ItemKind::Struct ? structOf(it.id) : nullptr;
    if (def && def->mega && s) {
        double x, y;
        proj(s->x, s->y, s->layH, x, y);
        if (it.id == "anelOrbital") {
            const double Rr = orbRingR(G.hr, scale), La = orbRingLight(*s, *G.host);
            drawOrbitalRing(p, G.hx, G.hy, Rr, ringPitch(), 91, simT, 'b', 0.6, m_dpr, G.hr, La);
            drawOrbitalRing(p, G.hx, G.hy, Rr, ringPitch(), 91, simT, 'f', 0.75, m_dpr, G.hr, La);
        } else if (it.id == "elevador") {
            drawElevator(p, G.hx, G.hy, G.hr, G.sang, G.hr * 2.2 * scale, cT, simT, 0.7, m_dpr, QStringLiteral("ghost"));
        } else {
            Build fake;
            fake.key = QStringLiteral("ghost"); fake.type = it.id; fake.rr = G.rr != 0 ? G.rr : 0.6; fake.scale = scale; fake.seed = 91;
            const size_t n = m_hits.size();
            drawStarMega(p, fake, *s, x, y, z, cT, 0.65, 'b');
            drawStarMega(p, fake, *s, x, y, z, cT, 0.65, 'f');
            m_hits.resize(n);
        }
    } else if (def) {
        const double px = G.deep ? G.sx : G.host ? G.hx + std::cos(sa) * (G.rp != 0 ? G.rp : 1.9) * G.hr : G.cx + std::cos(sa) * G.rr * s->layR * z;
        const double py = G.deep ? G.sy : G.host ? G.hy - std::sin(sa) * (G.rp != 0 ? G.rp : 1.9) * G.hr * cT : G.cy - std::sin(sa) * G.rr * s->layR * z * cT;
        const double size = def->size != 0 ? def->size : 0.02;
        const double r = G.deep ? size * 0.75 * scale * z : G.host ? std::max(G.hr * 0.16, size * s->layR * z * 0.65) * scale : size * 1.4 * s->layR * z * scale;
        drawModel(p, it.id, 91, px, py, std::max(3.0, r), 0.25 + cam.yaw, 0.78 + cam.tilt * 0.7, -0.7, -0.7, 0.75, m_dpr, QStringLiteral("ghost:") + QString::fromStdString(it.id));
    } else if (it.kind == ItemKind::Planet || it.kind == ItemKind::Moon) {
        Body* b = previewBody(it);
        const double px = G.host ? G.hx + std::cos(sa) * G.hr * 1.8 : G.cx + std::cos(sa) * G.rr * s->layR * z;
        const double py = G.host ? G.hy - std::sin(sa) * G.hr * 1.8 * cT : G.cy - std::sin(sa) * G.rr * s->layR * z * cT;
        const double r = G.host ? std::max(3.0, G.hr * 0.3) : std::max(4.0, s->layR * 0.05 * z);
        int D = 256;
        for (int v : { 16, 32, 64, 128, 256 }) if (v >= r * 2) { D = v; break; }
        p.setOpacity(0.75);
        if (const QImage* img = thumb(*b, D)) p.drawImage(QRectF(px - r, py - r, r * 2, r * 2), *img);
    } else if (it.kind == ItemKind::Belt) {
        const double r0 = G.rr * s->layR * z * 0.88, r1 = G.rr * s->layR * z * 1.12;
        QPen pen(it.id == "asteroides" ? QColor(205, 190, 170, 102) : QColor(190, 222, 242, 102)); pen.setWidthF(r1 - r0);
        p.setPen(pen); p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(G.cx, G.cy), (r0 + r1) / 2, std::max(0.5, (r0 + r1) / 2 * cT));
    } else if (it.kind == ItemKind::Comet) {
        const double px = G.cx + std::cos(sa) * G.rr * s->layR * z, py = G.cy - std::sin(sa) * G.rr * s->layR * z * cT;
        p.setPen(Qt::NoPen); p.setBrush(QColor(0xea, 0xf4, 0xff));
        p.drawEllipse(QPointF(px, py), 3, 3);
    }
    p.restore();
}
void Canvas::placeClick(double mx, double my)
{
    const PlaceItem it = *placing;
    const Snap G = snapFor(it, mx, my);
    if (!G.ok) {
        const QString h = pickAt(mx, my);
        if (h.startsWith(QLatin1Char('S'))) {
            double x, y, z2;
            frameOfKey(h, x, y, z2);
            flyTo(x, y, std::min(zMax(), std::max(zMin(), z2)), 950, QString(), heightOf(h));
        } else emit toast(G.msg, false);
        return;
    }
    System* s = G.sys;
    const double scale = sizeK(buildSize), simT = doc->simT;
    QString key, label = it.name;
    std::function<void()> undoFn, again;
    Doc* d = doc;
    if (it.kind == ItemKind::Star) {
        Galaxy& g = *doc->gal;
        const uint32_t seed = randSeed();
        auto sys = std::make_unique<System>();
        sys->id = int(g.systems.size()); sys->x = G.wx; sys->y = G.wy; sys->seed = seed; sys->young = 0.3; sys->rCore = std::hypot(G.wx, G.wy);
        sys->stars = { starOf(StarKind::G, 0, seed) }; sys->kind = StarKind::G; sys->amax = 0.05; sys->age = sys->stars[0].age; sys->edited = true; sys->hand = true;
        refreshSystem(*sys);
        System* raw = sys.get();
        g.systems.push_back(std::move(sys));
        doc->layoutSystems();
        key = keyS(*raw);
        undoFn = [d, raw] { raw->gone = true; d->layoutSystems(); };
        again = [d, raw] { raw->gone = false; d->layoutSystems(); };
        label = tr("Nova estrela");
    } else if (it.kind == ItemKind::Planet) {
        Type t = Type::Arido;
        typeFromKey(it.id, t);
        Body* p = addPlanet(*s, t, doc->fromR(*s, G.rr * s->layR), randSeed());
        if (isArtificial(t)) {
            p->radius = buildSize == QLatin1String("p") ? 0.4 : buildSize == QLatin1String("g") ? 6 : buildSize == QLatin1String("c") ? 10 : 1.1;
            p->mass = std::pow(p->radius, 3); p->gravity = p->mass / (p->radius * p->radius); p->density = (5.51 * p->mass) / std::pow(p->radius, 3);
        }
        s->edited = true;
        doc->layoutSystem(*s);
        p->layA0 = G.ang - p->layW * simT;
        doc->place(*s);
        key = keyP(*s, *p);
        auto held = std::make_shared<std::unique_ptr<Body>>();
        undoFn = [d, s, p, held] { *held = removeBody(*s, p); d->layoutSystem(*s); };
        again = [d, s, held] { restoreBody(*s, std::move(*held), nullptr); d->layoutSystem(*s); };
    } else if (it.kind == ItemKind::Moon) {
        Type t = Type::Rochosa;
        typeFromKey(it.id, t);
        Body* host = G.host;
        Body* m = addMoon(*s, *host, t, randSeed());
        s->edited = true;
        doc->layoutSystem(*s);
        m->layA0 = G.ang - m->layW * simT;
        doc->place(*s);
        key = keyM(*s, *host, *m);
        auto held = std::make_shared<std::unique_ptr<Body>>();
        undoFn = [d, s, m, held] { *held = removeBody(*s, m); d->layoutSystem(*s); };
        again = [d, s, host, held] { restoreBody(*s, std::move(*held), host); d->layoutSystem(*s); };
    } else if (it.kind == ItemKind::Comet) {
        const double Q = std::max(doc->fromR(*s, G.rr * s->layR), 0.2), q = Q * 0.1, ca = (q + Q) / 2, e = (Q - q) / (Q + q);
        int idx = 0;
        for (const auto& b : s->bodies) if (b->body == BodyKind::Cometa) idx = std::max(idx, b->idx);
        idx++;
        auto cm = std::make_unique<Body>();
        cm->body = BodyKind::Cometa; cm->type = Type::Cometa; cm->a = ca; cm->e = e; cm->w = G.ang - PI_D; cm->M0 = PI_D - simT * SPEED_COMET;
        cm->P = 365.25 * std::sqrt((ca * ca * ca) / (s->M != 0 ? s->M : 1)); cm->radius = 0.002; cm->idx = idx; cm->look.seed = randSeed();
        Body* raw = cm.get();
        s->bodies.push_back(std::move(cm));
        s->edited = true;
        key = QStringLiteral("C%1.%2").arg(s->id).arg(idx);
        auto held = std::make_shared<std::unique_ptr<Body>>();
        undoFn = [s, raw, held] { *held = removeBody(*s, raw); };
        again = [s, held] { restoreBody(*s, std::move(*held), nullptr); };
    } else if (it.kind == ItemKind::Belt) {
        const double a = doc->fromR(*s, G.rr * s->layR);
        int uid = 0;
        for (size_t i = 0; i < s->belts.size(); i++) uid = std::max(uid, s->belts[i].uid ? s->belts[i].uid : int(i) + 1);
        uid++;
        Belt belt;
        belt.kuiper = it.id == "kuiper"; belt.a0 = a * 0.82; belt.a1 = a * 1.2; belt.n = belt.kuiper ? 1100 : 900; belt.uid = uid;
        s->belts.push_back(belt);
        s->edited = true;
        key = QStringLiteral("K%1.%2").arg(s->id).arg(uid);
        undoFn = [s, uid] { s->belts.erase(std::remove_if(s->belts.begin(), s->belts.end(), [uid](const Belt& b) { return b.uid == uid; }), s->belts.end()); };
        again = [s, belt] { s->belts.push_back(belt); };
    } else {
        const StructDef* def = structOf(it.id);
        auto b = std::make_unique<Build>();
        b->key = QStringLiteral("B%1").arg(doc->seq++); b->type = it.id; b->seed = randSeed(); b->scale = def->sizes ? scale : 1; b->size = def->sizes ? buildSize : QString(); b->rot0 = randAngle();
        b->sys = s ? s->id : -1; b->host = G.host ? (G.host->uid ? G.host->uid : G.host->idx) : -1; b->deep = G.deep; b->deepX = G.wx; b->deepY = G.wy;
        if (G.host) {
            b->rp = G.rp != 0 ? G.rp : 1.9; b->w = SPEED_MOON / (1 + b->rp * 0.3); b->a0 = it.id == "elevador" ? G.sang - simT * 0.02 : G.ang - b->w * simT;
        } else if (s) {
            b->rr = def->mega && it.id != "mundoAnel" ? 0 : G.rr;
            if (!def->mega) { b->w = it.id == "naveMundo" || it.id == "portal" ? orbitSpeed(G.rr) * 0.4 : orbitSpeed(G.rr); b->a0 = G.ang - b->w * simT; }
        }
        Build* raw = doc->addBuild(std::move(b));
        key = raw->key;
        auto held = std::make_shared<std::unique_ptr<Build>>();
        undoFn = [d, raw, held] { *held = d->dropBuild(raw); };
        again = [d, held] { d->addBuild(std::move(*held)); };
    }
    m_tr.key.clear(); m_tr.sys.clear(); m_fx.key.clear();
    pushUndo(tr("%1 no lugar.").arg(label), undoFn, again);
    select(key, false);
    updateGhost(mx, my);
}
bool Canvas::removable(const QString& k) const
{
    const Ref o = doc->resolve(k);
    return o.kind == RefKind::Build || o.kind == RefKind::Planet || o.kind == RefKind::Moon || o.kind == RefKind::Comet || o.kind == RefKind::Belt || (o.kind == RefKind::System && o.sys->hand);
}
void Canvas::buildClick(double mx, double my)
{
    const QString h = pickAt(mx, my);
    const Ref o = doc->resolve(h);
    if (o.kind == RefKind::Build) { select(h, false); return; }
    if (placing) { placeClick(mx, my); return; }
    if (o && removable(h)) { select(h, false); return; }
    if (removable(sel)) select(doc->parentOf(sel), false);
}
// Mover: peças em órbita mudam de órbita (e até de dono); planetas mudam de posição na fila do sistema.
void Canvas::movableAt(double mx, double my, Drag& d)
{
    const QString h = pickAt(mx, my);
    if (h.isEmpty()) return;
    const Ref o = doc->resolve(h);
    if (!o) return;
    if (o.kind == RefKind::Planet && !placing) { d.moveKey = h; d.movePlanet = o.body; d.moveSys = o.sys; return; }
    if (o.kind == RefKind::Build) {
        const StructDef* def = structOf(o.build->type);
        if (!def->mega) { d.moveKey = h; d.moveBuild = o.build; d.moveSys = o.sys; d.was = *o.build; }
        else if (o.build->type == "mundoAnel") { d.moveKey = h; d.moveBuild = o.build; d.moveSys = o.sys; d.was = *o.build; d.radial = true; }
    }
}
void Canvas::moveDrag(double mx, double my)
{
    Drag& d = *m_drag;
    if (d.movePlanet) {
        double x, y;
        proj(d.moveSys->x, d.moveSys->y, d.moveSys->layH, x, y);
        const double cT = std::max(0.2, std::cos(cam.tilt));
        d.moveRr = std::max(0.2, std::min(1.25, std::hypot(mx - x, (my - y) / cT) / (d.moveSys->layR * cam.z)));
        d.moveAng = std::atan2(-(my - y) / cT, mx - x) - cam.yaw;
        d.hasGhostOrb = true; d.gx = x; d.gy = y; d.gr = d.moveRr * d.moveSys->layR * cam.z; d.gc = cT;
        return;
    }
    if (d.radial) {
        double x, y;
        proj(d.moveSys->x, d.moveSys->y, d.moveSys->layH, x, y);
        const double cT = std::max(0.2, std::cos(cam.tilt));
        d.moveBuild->rr = std::max(0.25, std::min(1.2, std::hypot(mx - x, (my - y) / cT) / (d.moveSys->layR * cam.z)));
        return;
    }
    PlaceItem it;
    it.id = d.moveBuild->type; it.kind = ItemKind::Struct;
    const Snap G = snapFor(it, mx, my);
    if (G.ok) applySnap(*d.moveBuild, G);
}
void Canvas::applySnap(Build& b, const Snap& G)
{
    const double simT = doc->simT;
    b.sys = G.sys ? G.sys->id : -1;
    b.deep = G.deep;
    if (G.deep) { b.deepX = G.wx; b.deepY = G.wy; }
    b.host = G.host ? (G.host->uid ? G.host->uid : G.host->idx) : -1;
    if (G.host) { b.rp = G.rp != 0 ? G.rp : 1.9; b.w = SPEED_MOON / (1 + b.rp * 0.3); b.a0 = G.ang - b.w * simT; b.rr = 0; }
    else if (G.sys) { b.rr = G.rr; b.w = orbitSpeed(G.rr); b.a0 = G.ang - b.w * simT; b.rp = 0; }
    m_tr.key.clear(); m_tr.sys.clear();
}
void Canvas::moveDrop()
{
    Drag& d = *m_drag;
    Doc* dc = doc;
    if (d.movePlanet && d.moveRr != 0) {
        System* s = d.moveSys;
        Body* p = d.movePlanet;
        const double a0 = p->a, angWas = p->layA0 + p->layW * doc->simT, simT = doc->simT;
        p->a = doc->fromR(*s, d.moveRr * s->layR);
        reorder(*s); refreshSystem(*s); s->edited = true;
        doc->layoutSystem(*s);
        p->layA0 = d.moveAng - p->layW * simT;
        doc->place(*s);
        pushUndo(tr("%1 mudou de órbita.").arg(doc->labelOf(d.moveKey)), [dc, s, p, a0, angWas] {
            p->a = a0; reorder(*s); refreshSystem(*s); dc->layoutSystem(*s); p->layA0 = angWas - p->layW * dc->simT;
        });
    } else if (d.moveBuild) {
        Build* b = d.moveBuild;
        const Build was = d.was;
        pushUndo(tr("%1 mudou de órbita.").arg(doc->labelOf(d.moveKey)), [b, was] { *b = was; });
    }
    d.hasGhostOrb = false;
    emit selectionChanged(sel);
    m_dirty = true;
}
// Nada some de vez: remover sempre tem Desfazer, e nome e documentos ficam guardados.
void Canvas::removeThing(const QString& k)
{
    const Ref o = doc->resolve(k);
    if (!o) return;
    const QString name = doc->labelOf(k), parent = doc->parentOf(k);
    std::function<void()> back, again;
    Doc* d = doc;
    if (o.kind == RefKind::Build) {
        Build* raw = o.build;
        auto held = std::make_shared<std::unique_ptr<Build>>(d->dropBuild(raw));
        back = [d, held] { d->addBuild(std::move(*held)); };
        again = [d, raw, held] { *held = d->dropBuild(raw); };
    } else if (o.kind == RefKind::Planet) {
        System* s = o.sys;
        Body* p = o.body;
        const int uid = p->uid ? p->uid : p->idx;
        struct Held { std::unique_ptr<Body> body; std::vector<Build*> raw; std::vector<std::unique_ptr<Build>> hosted; };
        auto held = std::make_shared<Held>();
        const auto drop = [d, s, p, uid, held] {
            held->raw.clear();
            for (Build* b : d->buildsIn(*s)) if (b->host == uid) held->raw.push_back(b);
            for (Build* b : held->raw) held->hosted.push_back(d->dropBuild(b));
            held->body = removeBody(*s, p);
            d->layoutSystem(*s);
        };
        drop();
        s->edited = true;
        back = [d, s, held] { restoreBody(*s, std::move(held->body), nullptr); for (auto& b : held->hosted) d->addBuild(std::move(b)); held->hosted.clear(); d->layoutSystem(*s); };
        again = drop;
    } else if (o.kind == RefKind::Moon) {
        System* s = o.sys;
        Body* m = o.moon;
        Body* p = o.body;
        auto held = std::make_shared<std::unique_ptr<Body>>(removeBody(*s, m));
        s->edited = true;
        doc->layoutSystem(*s);
        back = [d, s, p, held] { restoreBody(*s, std::move(*held), p); d->layoutSystem(*s); };
        again = [d, s, m, held] { *held = removeBody(*s, m); d->layoutSystem(*s); };
    } else if (o.kind == RefKind::Comet) {
        System* s = o.sys;
        Body* c = o.comet;
        auto held = std::make_shared<std::unique_ptr<Body>>(removeBody(*s, c));
        s->edited = true;
        back = [s, held] { restoreBody(*s, std::move(*held), nullptr); };
        again = [s, c, held] { *held = removeBody(*s, c); };
    } else if (o.kind == RefKind::Belt) {
        System* s = o.sys;
        const size_t i = size_t(o.belt - s->belts.data());
        const Belt belt = *o.belt;
        s->belts.erase(s->belts.begin() + long(i));
        s->edited = true;
        back = [s, i, belt] { s->belts.insert(s->belts.begin() + long(std::min(i, s->belts.size())), belt); };
        again = [s, i] { if (i < s->belts.size()) s->belts.erase(s->belts.begin() + long(i)); };
    } else if (o.kind == RefKind::System) {
        System* s = o.sys;
        s->gone = true;
        doc->layoutSystems();
        back = [d, s] { s->gone = false; d->layoutSystems(); };
        again = [d, s] { s->gone = true; d->layoutSystems(); };
    } else return;
    m_tr.key.clear(); m_tr.sys.clear(); m_fx.key.clear();
    pushUndo(tr("%1 saiu do mapa.").arg(name), back, again);
    select(parent, false);
}

// ── territórios ──
void Canvas::assignSystems(const std::vector<System*>& list, Region* r, const QString& label)
{
    struct Was { System* s; Region* o; };
    auto was = std::make_shared<std::vector<Was>>();
    for (System* s : list) { Region* o = doc->regionOfSys(*s); if (o != r) was->push_back({ s, o }); }
    if (was->empty()) return;
    const auto put = [was, r] { for (const Was& w : *was) { if (w.o) w.o->systems.remove(w.s->id); if (r) r->systems.insert(w.s->id); } };
    const auto back = [was, r] { for (const Was& w : *was) { if (r) r->systems.remove(w.s->id); if (w.o) w.o->systems.insert(w.s->id); } };
    put();
    m_tr.key.clear(); m_fx.key.clear(); m_dirty = true;
    pushUndo(label, back, put);
    emit toolDockChanged();
}
void Canvas::realmClick(double mx, double my, bool shift)
{
    const QString h = pickAt(mx, my);
    const Ref o = doc->resolve(h);
    if (!o || !o.sys || o.sys->isCore) return;
    if (shift) {
        Region* old = doc->regionOfSys(*o.sys);
        if (old) assignSystems({ o.sys }, nullptr, tr("%1 saiu de %2.").arg(doc->labelOf(keyS(*o.sys)), old->name));
        return;
    }
    Region* r = nullptr;
    for (const auto& q : doc->regions) if (q->key == realmActive) r = q.get();
    if (!r) { emit toast(tr("Crie um território primeiro."), false); return; }
    const double lim = realmReach >= 400 ? 1e9 : realmReach / 1000.0;
    std::vector<System*> list { o.sys };
    if (realmReach > 20) for (const auto& s : doc->gal->systems) {
        if (s.get() != o.sys && !s->gone && !s->isCore && !doc->regionOfSys(*s) && std::hypot(s->x - o.sys->x, s->y - o.sys->y) < lim) list.push_back(s.get());
    }
    assignSystems(list, r, list.size() > 1 ? tr("%1 sistemas agora são de %2.").arg(list.size()).arg(r->name) : tr("%1 agora é de %2.").arg(doc->labelOf(keyS(*o.sys)), r->name));
}
void Canvas::realmPaintAt(double mx, double my)
{
    Region* r = nullptr;
    if (realmMode != RealmMode::Neutral) for (const auto& q : doc->regions) if (q->key == realmActive) r = q.get();
    if (realmMode == RealmMode::Paint && !r) return;
    auto& seen = m_drag ? m_drag->painted : m_painted;
    for (const auto& s : doc->gal->systems) {
        if (s->gone || s->isCore || seen.contains(s->id)) continue;
        double x, y;
        proj(s->x, s->y, s->layH, x, y);
        if (std::hypot(x - mx, y - my) > realmBrush) continue;
        Region* o = doc->regionOfSys(*s);
        if (o == r) continue;
        seen.insert(s->id, o ? o->key : QString());
        if (o) o->systems.remove(s->id);
        if (r) r->systems.insert(s->id);
    }
}
void Canvas::realmPaintEnd()
{
    if (m_painted.isEmpty()) return;
    Region* r = nullptr;
    if (realmMode != RealmMode::Neutral) for (const auto& q : doc->regions) if (q->key == realmActive) r = q.get();
    const QHash<int, QString> list = m_painted;
    m_painted.clear();
    Doc* d = doc;
    const auto regionBy = [d](const QString& k) -> Region* { for (const auto& q : d->regions) if (q->key == k) return q.get(); return nullptr; };
    const auto put = [list, r, regionBy] { for (auto it = list.begin(); it != list.end(); ++it) { if (Region* o = regionBy(it.value())) o->systems.remove(it.key()); if (r) r->systems.insert(it.key()); } };
    const auto back = [list, r, regionBy] { for (auto it = list.begin(); it != list.end(); ++it) { if (r) r->systems.remove(it.key()); if (Region* o = regionBy(it.value())) o->systems.insert(it.key()); } };
    m_tr.key.clear(); m_fx.key.clear();
    pushUndo(r ? tr("%1 sistemas agora são de %2.").arg(list.size()).arg(r->name) : tr("%1 sistemas sem dono.").arg(list.size()), back, put);
    emit toolDockChanged();
    m_dirty = true;
}
void Canvas::rulerClick(double mx, double my)
{
    if (rulerDone) { rulerPts.clear(); rulerDone = false; }
    double wx, wy;
    toWorld(mx, my, wx, wy);
    rulerPts.push_back(QPointF(wx, wy));
    m_dirty = true;
}

} // namespace galaxy
