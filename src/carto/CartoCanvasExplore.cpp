// Explorar na mesa: contorno do território (passando o mouse e escolhido),
// a contagem de documentos embaixo do nome e os arcos das conexões, com o
// nome no meio (clicável). Quem decide o que mostrar é o painel (ExploreView).
#include "CartoCanvas.h"

#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <cmath>

namespace carto {

int Canvas::realmAtScreen(QPointF s) const
{
    if (!m_doc || !m_doc->info || !m_world || !infoOn()) return -1;
    const Info& in = *m_doc->info;
    const auto q = screenToWorld(s);
    const int gx = static_cast<int>(std::lround((q[0] - 2) / in.step)), gy = static_cast<int>(std::lround((q[1] - 2) / in.step));
    if (gx < 0 || gy < 0 || gx >= in.gw || gy >= in.gh) return -1;
    const size_t c = static_cast<size_t>(gy) * in.gw + gx;
    if (c >= in.realmGrid.land.size() || !in.realmGrid.land[c]) return -1;   // no mar não tem território
    const int id = in.realmAt(q[0], q[1]).id;
    return id >= 0 && id < static_cast<int>(in.defs.size()) ? id : -1;
}

std::optional<QPointF> Canvas::realmCenterOnScreen(int r) const
{
    if (!m_doc || !m_doc->info || !m_base || r < 0 || r >= static_cast<int>(m_doc->info->realms.size())) return std::nullopt;
    const Realm& R = m_doc->info->realms[r];
    if (!R.area) return std::nullopt;
    const auto g = m_scene.ground(sceneState(), R.x, R.y);
    if (!g) return std::nullopt;
    return QPointF(g->X, g->Y);
}

std::optional<QPointF> Canvas::worldOnScreen(double x, double y) const
{
    if (!m_base) return std::nullopt;
    const auto g = m_scene.ground(sceneState(), x, y);
    if (!g) return std::nullopt;
    return QPointF(g->X, g->Y);
}

void Canvas::panBy(double dx, double dy)
{
    m_panX += dx; m_panY += dy;
    update();
    emit viewChanged();
}

// ── a viagem do Explorar: zoom e deslocamento andam juntos, guiados por um ponto ──
void Canvas::flyTo(double wx, double wy, double zoomFit, QPointF at)
{
    if (!m_world || width() < 10) return;
    double fz, fx, fy;
    fitTarget(fz, fx, fy);
    m_fitZoom = fz;
    const double z = std::max(fz * 0.35, std::min(std::max(fz * 14, 7.0), fz * zoomFit));
    const auto n = worldToPlane(*m_world, m_ang, wx, wy);
    // o relevo levanta o ponto na tela (proporcional ao zoom): mede agora e desconta no alvo
    QPointF lift(0, 0);
    if (const auto g = worldOnScreen(wx, wy)) lift = (*g - QPointF(m_panX + m_zoom * n[0], m_panY + m_zoom * sq() * n[1])) / m_zoom;
    flyStart(z, at.x() - z * (n[0] + lift.x()), at.y() - z * (n[1] * sq() + lift.y()), n[0], n[1]);
}

void Canvas::flyFit()
{
    if (!m_world || width() < 10) return;
    double z, px, py;
    fitTarget(z, px, py);
    m_fitZoom = z;
    const auto n = worldToPlane(*m_world, m_ang, m_world->W / 2.0, m_world->H / 2.0);
    flyStart(z, px, py, n[0], n[1]);
}

void Canvas::flyStart(double z, double px, double py, double refX, double refY)
{
    if (!m_flyTimer) {
        m_flyTimer = new QTimer(this);
        m_flyTimer->setInterval(15);
        connect(m_flyTimer, &QTimer::timeout, this, [this]() {
            const double t = std::min(1.0, m_flyClock.elapsed() / 420.0);
            const double e = 1 - std::pow(1 - t, 3);
            const double s = sq();
            // o ponto-guia anda em linha reta na tela enquanto o zoom muda em escala de log
            const double z0 = m_fly[0], z1 = m_fly[3];
            const double z = std::exp(std::log(z0) + (std::log(z1) - std::log(z0)) * e);
            const double sx0 = m_fly[1] + z0 * m_fly[6], sy0 = m_fly[2] + z0 * s * m_fly[7];
            const double sx1 = m_fly[4] + z1 * m_fly[6], sy1 = m_fly[5] + z1 * s * m_fly[7];
            const double sx = sx0 + (sx1 - sx0) * e, sy = sy0 + (sy1 - sy0) * e;
            m_zoom = z;
            m_panX = sx - z * m_fly[6];
            m_panY = sy - z * s * m_fly[7];
            m_lastMove.restart();
            update();
            emit viewChanged();
            if (t >= 1) { m_flyTimer->stop(); m_settle.start(); }
        });
    }
    if (std::abs(z - m_zoom) < 1e-6 && std::hypot(px - m_panX, py - m_panY) < 0.5) return;
    m_fly[0] = m_zoom; m_fly[1] = m_panX; m_fly[2] = m_panY;
    m_fly[3] = z; m_fly[4] = px; m_fly[5] = py;
    m_fly[6] = refX; m_fly[7] = refY;
    m_flyClock.restart();
    m_flyTimer->start();
}

QString Canvas::exploreLinkAt(QPointF s) const
{
    for (const auto& h : m_xhits) if (h.first.contains(s)) return h.second;
    return QString();
}

void Canvas::exploreMove(QPointF pos)
{
    const QString link = exploreLinkAt(pos);
    const int r = link.isEmpty() ? realmAtScreen(pos) : -1;
    setCursor(r >= 0 || !link.isEmpty() ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (r != m_xhovRealm || link != m_xhovLink) { m_xhovRealm = r; m_xhovLink = link; }
    emit exploreHover(r, link, pos.toPoint());
}

// contorno por marching squares na grade de donos (só terra); guardado por território
const std::vector<float>& Canvas::exploreSegs(int slot, int r)
{
    XOutline& o = m_xoutline[slot];
    const Info& in = *m_doc->info;
    if (o.r == r && o.info == &in && o.infoVer == m_doc->infoVer && o.rev == m_sceneRev) return o.segs;
    o.r = r; o.info = &in; o.infoVer = m_doc->infoVer; o.rev = m_sceneRev;
    o.segs.clear();
    const int gw = in.gw, gh = in.gh;
    const double S = in.step;
    const auto& land = in.realmGrid.land;
    const auto& owner = in.owner;
    if (owner.size() < static_cast<size_t>(gw) * gh || land.size() < static_cast<size_t>(gw) * gh) return o.segs;
    auto on = [&](int x, int y) -> int {
        if (x < 0 || y < 0 || x >= gw || y >= gh) return 0;
        const size_t i = static_cast<size_t>(y) * gw + x;
        return land[i] && owner[i] == r ? 1 : 0;
    };
    auto add = [&](double ax, double ay, double bx, double by) {
        o.segs.push_back(static_cast<float>(ax * S + 2)); o.segs.push_back(static_cast<float>(ay * S + 2));
        o.segs.push_back(static_cast<float>(bx * S + 2)); o.segs.push_back(static_cast<float>(by * S + 2));
    };
    for (int y = -1; y < gh; y++) for (int x = -1; x < gw; x++) {
        const int c = on(x, y) | (on(x + 1, y) << 1) | (on(x + 1, y + 1) << 2) | (on(x, y + 1) << 3);
        if (c == 0 || c == 15) continue;
        const double tx = x + 0.5, ty = y, rx = x + 1, ry = y + 0.5, bx = x + 0.5, by = y + 1, lx = x, ly = y + 0.5;
        switch (c) {
        case 1: case 14: add(lx, ly, tx, ty); break;
        case 2: case 13: add(tx, ty, rx, ry); break;
        case 3: case 12: add(lx, ly, rx, ry); break;
        case 4: case 11: add(rx, ry, bx, by); break;
        case 6: case 9: add(tx, ty, bx, by); break;
        case 7: case 8: add(lx, ly, bx, by); break;
        case 5: add(lx, ly, tx, ty); add(rx, ry, bx, by); break;
        case 10: add(tx, ty, rx, ry); add(bx, by, lx, ly); break;
        default: break;
        }
    }
    return o.segs;
}

// Os contornos vão ANTES dos nomes do mapa (paintMap): por cima, passavam em
// cima do nome das cidades e dos lugares na beira do território.
void Canvas::drawExploreOutlines(QPainter& p, const SceneState& st)
{
    if (!m_doc || !m_doc->info || !infoOn()) return;
    const Info& in = *m_doc->info;
    const int nDefs = static_cast<int>(in.defs.size());
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    // "Atua em": os territórios por onde o grupo escolhido se espalha, pontilhados na cor dele
    for (size_t k = 0; k < m_explore.acts.size() && k < 4; k++) {
        const int r = m_explore.acts[k];
        if (r < 0 || r >= nDefs || r == m_explore.sel) continue;
        const std::vector<float>& segs = exploreSegs(2 + static_cast<int>(k), r);
        QPainterPath path;
        for (size_t i = 0; i + 3 < segs.size(); i += 4) {
            const auto a = m_scene.ground(st, segs[i], segs[i + 1]), b = m_scene.ground(st, segs[i + 2], segs[i + 3]);
            if (!a || !b) continue;
            path.moveTo(a->X, a->Y); path.lineTo(b->X, b->Y);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(10, 12, 14, 120), 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);
        p.setPen(QPen(m_explore.actsColor.isValid() ? m_explore.actsColor.lighter(130) : QColor(240, 236, 226), 2.2, Qt::DotLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);
    }
    // contornos: o do mouse tracejado na cor do território, o escolhido cheio e claro
    const int rs[2] = { m_explore.hov == m_explore.sel ? -1 : m_explore.hov, m_explore.sel };
    for (int k = 0; k < 2; k++) {
        const int r = rs[k];
        if (r < 0 || r >= nDefs) continue;
        const std::vector<float>& segs = exploreSegs(k, r);
        QPainterPath path;
        for (size_t i = 0; i + 3 < segs.size(); i += 4) {
            const auto a = m_scene.ground(st, segs[i], segs[i + 1]), b = m_scene.ground(st, segs[i + 2], segs[i + 3]);
            if (!a || !b) continue;
            path.moveTo(a->X, a->Y); path.lineTo(b->X, b->Y);
        }
        const bool chosen = k == 1;
        const RGB c = in.defs[r].color;
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(10, 12, 14, 140), chosen ? 6 : 4.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);
        QPen pen(chosen ? QColor(252, 248, 238) : QColor(static_cast<int>(std::min(255.0, c[0] + 70)), static_cast<int>(std::min(255.0, c[1] + 70)), static_cast<int>(std::min(255.0, c[2] + 70)), 242),
                 chosen ? 2.4 : 1.7, chosen ? Qt::SolidLine : Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.drawPath(path);
    }
    p.restore();
}

void Canvas::drawExplore(QPainter& p, const SceneState& st)
{
    m_xhits.clear();
    if (!m_doc || !m_doc->info || !infoOn()) return;
    const Info& in = *m_doc->info;
    const int nDefs = static_cast<int>(in.defs.size());
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    // a contagem embaixo do nome de cada território
    QFont f = font(); f.setPixelSize(11); f.setWeight(QFont::DemiBold);
    p.setFont(f);
    const QFontMetricsF fm(f);
    for (int r = 0; r < nDefs; r++) {
        const auto g = realmCenterOnScreen(r);
        if (!g) continue;
        const int n = r < static_cast<int>(m_explore.counts.size()) ? m_explore.counts[r] : 0;
        if (n <= 0) continue;   // território sem documento não ganha pílula
        const QString txt = tr("%n documento(s)", "", n);
        const double w = fm.horizontalAdvance(txt) + 18, y = g->y() + 24;
        const QRectF box(g->x() - w / 2, y - 10, w, 20);
        const bool chosen = r == m_explore.sel;
        p.setPen(Qt::NoPen);
        p.setBrush(chosen ? QColor(252, 248, 238, 242) : QColor(14, 16, 20, 184));
        p.drawRoundedRect(box, 10, 10);
        p.setPen(chosen ? QColor(30, 26, 22) : QColor(240, 236, 226));
        p.drawText(box, Qt::AlignCenter, txt);
    }
    // construções batizadas: pontos clicáveis (um Lugar dentro do território); a escolhida ganha um anel
    for (const QString& id : m_explore.arts) {
        const Art* art = nullptr;
        for (const Art& a : in.user.arts) if (a.id == id) { art = &a; break; }
        if (!art) continue;
        const auto g = m_scene.ground(st, art->x, art->y);
        if (!g) continue;
        const QPointF c(g->X, g->Y);
        const bool chosen = id == m_explore.selArt, hov = m_xhovLink == QStringLiteral("art:") + id;
        if (chosen || hov) {
            const double rx = 22 * std::max(0.8, std::min(1.6, art->sc)), ry = rx * std::max(0.35, std::sin(m_pitch * M_PI / 180));
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(10, 12, 14, 150), chosen ? 5 : 4));
            p.drawEllipse(c, rx, ry);
            p.setPen(QPen(QColor(252, 248, 238), chosen ? 2.4 : 1.6, chosen ? Qt::SolidLine : Qt::DashLine));
            p.drawEllipse(c, rx, ry);
        }
        m_xhits.push_back({ QRectF(c.x() - 16, c.y() - 34, 32, 42), QStringLiteral("art:") + id });
    }
    // conexões: arco entre os centros, o nome no meio
    QFont lf = font(); lf.setPixelSize(12); lf.setWeight(QFont::DemiBold);
    const QFontMetricsF lfm(lf);
    for (const ExploreLinkView& l : m_explore.links) {
        const auto A = realmCenterOnScreen(l.a), B = realmCenterOnScreen(l.b);
        if (!A || !B) continue;
        const QPointF mid = (*A + *B) / 2;
        const double d = std::hypot(B->x() - A->x(), B->y() - A->y());
        const QPointF ctrl(mid.x(), mid.y() - std::min(140.0, d * 0.28));
        QPainterPath arc(*A);
        arc.quadTo(ctrl, *B);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(10, 12, 14, 153), 6, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(arc);
        p.setPen(QPen(l.color, 2.6, m_explore.duo ? Qt::SolidLine : Qt::DashLine, Qt::RoundCap));
        p.drawPath(arc);
        for (const QPointF& E : { *A, *B }) {
            p.setPen(QPen(QColor(252, 248, 238), 2));
            p.setBrush(l.color);
            p.drawEllipse(E, 5, 5);
        }
        const QPointF at = 0.25 * *A + 0.5 * ctrl + 0.25 * *B;
        const QString txt = lfm.elidedText(l.label, Qt::ElideRight, 260);
        const double w = lfm.horizontalAdvance(txt) + 28;
        const QRectF box(at.x() - w / 2, at.y() - 13, w, 26);
        p.setFont(lf);
        p.setPen(QPen(l.color, 2));
        p.setBrush(QColor(252, 248, 238, 245));
        p.drawRoundedRect(box, 13, 13);
        p.setPen(QColor(30, 26, 22));
        p.drawText(box, Qt::AlignCenter, txt);
        m_xhits.push_back({ box, l.id });
    }
    p.restore();
}

} // namespace carto
