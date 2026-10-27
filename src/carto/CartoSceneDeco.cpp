// Enfeites das vistas de arte (porte do scene.js do Map Lab, v35).
// Por baixo do terreno: o mapa pinta a terra opaca e deixa o mar transparente, então o que é
// desenhado ANTES dele só aparece no mar — na Carta náutica, as linhas de rumo, as ondinhas de pena
// e a serpente marinha; na Planta de guerra, a grade do papel quadriculado.
// Por cima de tudo (antes dos nomes): as rosas dos ventos da Carta e a borda gasta do Pergaminho.

#include "CartoScene.h"
#include "CartoEdit.h"

#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <cmath>

namespace carto {

namespace {
// ponto do mundo no nível do mar → tela
QPointF seaXY(const SceneState& st, double x, double y)
{
    const auto p = worldToPlane(st.doc->world, st.ang, x, y);
    const double sq = st.g ? st.g->sq : 1;
    return QPointF(st.panX + st.zoom * p[0], st.panY + st.zoom * sq * p[1]);
}
}

// lugares de mar aberto, sorteados pela semente: rosas dos ventos, ondinhas e monstros
const Scene::SeaDeco& Scene::seaSpots(const SceneState& st)
{
    const World& w = st.doc->world;
    const QString key = QStringLiteral("%1|%2|%3").arg(st.doc->ver).arg(w.seed).arg(w.W);
    if (m_deco.key == key) return m_deco;
    m_deco = SeaDeco();
    m_deco.key = key;
    const double M = std::min(w.W, w.H);
    auto H = [&](double x, double y) { return heightAt(w, x, y); };
    // mar aberto: nada de terra em três anéis em volta
    auto clear = [&](double x, double y, double r) {
        for (int a = 0; a < 16; a++) for (double f : { 0.35, 0.7, 1.0 })
            if (H(x + std::cos(a / 16.0 * 6.283) * r * f, y + std::sin(a / 16.0 * 6.283) * r * f) > 0) return false;
        return true;
    };
    double t = std::fmod(std::fabs(double(w.seed)) * 9301 + 49297, 233280);
    auto rnd = [&]() { t = std::fmod(t * 9301 + 49297, 233280); return t / 233280; };
    auto& roses = m_deco.roses; auto& waves = m_deco.waves; auto& beasts = m_deco.beasts;
    for (int tries = 0; tries < 400 && roses.size() < 3; tries++) {
        const double x = M * 0.08 + rnd() * (w.W - M * 0.16), y = M * 0.08 + rnd() * (w.H - M * 0.16);
        if (H(x, y) > 0 || !clear(x, y, M * 0.07)) continue;
        bool nearRose = false;
        for (const auto& r : roses) if (std::hypot(r[0] - x, r[1] - y) < M * 0.4) nearRose = true;
        if (nearRose) continue;
        roses.push_back({ x, y, M * 0.045 });
    }
    if (roses.empty()) roses.push_back({ w.W * 0.12, w.H * 0.12, M * 0.045 });
    for (int tries = 0; tries < 1400 && waves.size() < 70; tries++) {
        const double x = rnd() * w.W, y = rnd() * w.H;
        if (H(x, y) > 0 || !clear(x, y, M * 0.025)) continue;
        waves.push_back({ x, y, 0.8 + rnd() * 0.5 });
    }
    for (int tries = 0; tries < 400 && beasts.size() < 2; tries++) {
        const double x = M * 0.1 + rnd() * (w.W - M * 0.2), y = M * 0.1 + rnd() * (w.H - M * 0.2);
        if (H(x, y) > 0 || !clear(x, y, M * 0.08)) continue;
        bool bad = false;
        for (const auto& r : roses) if (std::hypot(r[0] - x, r[1] - y) < M * 0.15) bad = true;
        for (const auto& b : beasts) if (std::hypot(b[0] - x, b[1] - y) < M * 0.35) bad = true;
        if (bad) continue;
        beasts.push_back({ x, y, rnd() < 0.5 ? -1.0 : 1.0 });
    }
    return m_deco;
}

void Scene::drawUnder(QPainter& p, const SceneState& st)
{
    if (!st.doc || (st.mode != QLatin1String("chart") && st.mode != QLatin1String("blue"))) return;
    const World& w = st.doc->world;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    if (st.mode == QLatin1String("blue")) {
        // grade a cada 20 px do mundo (a mesma que o terreno risca na terra) e uma mestra a cada 100
        const double G = 20, x0 = -std::ceil(w.W * 0.3 / G) * G, x1 = w.W * 1.3, y0 = -std::ceil(w.H * 0.3 / G) * G, y1 = w.H * 1.3;
        for (int pass = 0; pass < 2; pass++) {
            QPainterPath grid;
            for (double x = x0; x <= x1; x += G) { if ((std::lround(x / G) % 5 == 0) != bool(pass)) continue; grid.moveTo(seaXY(st, x, y0)); grid.lineTo(seaXY(st, x, y1)); }
            for (double y = y0; y <= y1; y += G) { if ((std::lround(y / G) % 5 == 0) != bool(pass)) continue; grid.moveTo(seaXY(st, x0, y)); grid.lineTo(seaXY(st, x1, y)); }
            p.setPen(QPen(pass ? QColor(232, 240, 250, 82) : QColor(232, 240, 250, 36), pass ? 1.1 : 0.7));
            p.setBrush(Qt::NoBrush);
            p.drawPath(grid);
        }
        p.restore();
        return;
    }
    const SeaDeco& D = seaSpots(st);
    const double far = std::hypot(w.W, w.H);
    // linhas de rumo: 32 a partir de cada rosa; os 8 ventos em tinta, os meios-ventos em verde, as quartas em vermelho
    for (const auto& r : D.roses) for (int i = 0; i < 32; i++) {
        const double a = i / 32.0 * M_PI * 2;
        const QColor c = i % 4 == 0 ? QColor(62, 48, 36, 107) : i % 2 == 0 ? QColor(52, 110, 70, 102) : QColor(164, 52, 40, 92);
        p.setPen(QPen(c, i % 4 == 0 ? 0.9 : 0.7));
        p.drawLine(seaXY(st, r[0], r[1]), seaXY(st, r[0] + std::cos(a) * far, r[1] + std::sin(a) * far));
    }
    // ondinhas: três arquinhos de pena
    const double kz = std::max(0.7, std::min(2.2, st.zoom / st.fitZoom));
    QPen wp(QColor(62, 48, 36, 128), 0.9); wp.setCapStyle(Qt::RoundCap);
    p.setPen(wp); p.setBrush(Qt::NoBrush);
    for (const auto& v : D.waves) {
        const QPointF X = seaXY(st, v[0], v[1]);
        if (X.x() < -20 || X.y() < -20 || X.x() > st.vw + 20 || X.y() > st.vh + 20) continue;
        const double s = 3.2 * kz * v[2];
        QPainterPath wv;
        for (int j = 0; j < 3; j++) { const double cx = X.x() + (j - 1) * s * 1.6; wv.moveTo(cx - s * 0.8, X.y()); wv.quadTo(cx, X.y() - s * 0.75, cx + s * 0.8, X.y()); }
        p.drawPath(wv);
    }
    // monstro marinho: serpente de três corcovas saindo da água, com a cabeça e a barbatana
    for (const auto& b : D.beasts) {
        const QPointF X = seaXY(st, b[0], b[1]);
        const double s = 9 * kz;
        if (X.x() < -80 || X.y() < -80 || X.x() > st.vw + 80 || X.y() > st.vh + 80) continue;
        p.save();
        p.translate(X); p.scale(b[2], 1);
        QPen ink(QColor(62, 48, 36), 1.1); ink.setJoinStyle(Qt::RoundJoin);
        p.setPen(ink); p.setBrush(QColor(238, 226, 196, 242));
        for (int j = 0; j < 3; j++) {
            const double x0 = -s * 3 + j * s * 1.9, hgt = s * (0.95 - j * 0.12);
            QPainterPath hump;
            hump.moveTo(x0, 0); hump.cubicTo(x0, -hgt * 1.3, x0 + s * 1.2, -hgt * 1.3, x0 + s * 1.2, 0); hump.lineTo(x0 + s * 0.85, 0);
            hump.cubicTo(x0 + s * 0.85, -hgt * 0.8, x0 + s * 0.35, -hgt * 0.8, x0 + s * 0.35, 0); hump.closeSubpath();
            p.drawPath(hump);
            QPainterPath fin; fin.moveTo(x0 + s * 0.45, -hgt * 1.0); fin.lineTo(x0 + s * 0.6, -hgt * 1.45); fin.lineTo(x0 + s * 0.8, -hgt * 0.98);
            p.setBrush(Qt::NoBrush); p.drawPath(fin); p.setBrush(QColor(238, 226, 196, 242));
        }
        // pescoço e cabeça na frente
        QPainterPath neck;
        neck.moveTo(s * 2.7, 0); neck.cubicTo(s * 2.6, -s * 1.6, s * 3.4, -s * 2.4, s * 4.2, -s * 2.0); neck.lineTo(s * 4.6, -s * 1.5);
        neck.lineTo(s * 3.8, -s * 1.45); neck.cubicTo(s * 3.3, -s * 1.3, s * 3.2, -s * 0.6, s * 3.3, 0); neck.closeSubpath();
        p.drawPath(neck);
        p.setPen(Qt::NoPen); p.setBrush(QColor(62, 48, 36));
        p.drawEllipse(QPointF(s * 4.05, -s * 1.85), s * 0.12, s * 0.12);
        // a água em volta
        p.setPen(ink); p.drawLine(QPointF(-s * 3.6, 0.5), QPointF(s * 3.9, 0.5));
        p.restore();
    }
    p.restore();
}

void Scene::drawOver(QPainter& p, const SceneState& st)
{
    if (!st.doc) return;
    const double W = st.vw, H = st.vh;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    if (st.mode == QLatin1String("chart")) {
        const SeaDeco& D = seaSpots(st);
        const QColor ink(62, 48, 36), paper(246, 238, 216);
        for (const auto& r : D.roses) {
            const QPointF X = seaXY(st, r[0], r[1]);
            const double R = std::max(26.0, std::min(90.0, r[2] * st.zoom));
            if (X.x() < -R || X.y() < -R || X.x() > W + R || X.y() > H + R) continue;
            p.save();
            p.translate(X); p.rotate(st.ang * 180 / M_PI);
            p.setPen(QPen(ink, 1));
            p.setBrush(QColor(238, 226, 196, 230)); p.drawEllipse(QPointF(0, 0), R * 0.62, R * 0.62);
            p.setBrush(Qt::NoBrush); p.drawEllipse(QPointF(0, 0), R * 0.56, R * 0.56);
            // 16 pontas: as 4 grandes, as 4 médias, as 8 pequenas; metade de cada ponta escura (luz e sombra)
            auto tip = [&](double a, double len, double wid, const QColor& dark) {
                const double c = std::cos(a), s = std::sin(a), px = -s * wid, py = c * wid;
                QPainterPath h1; h1.moveTo(0, 0); h1.lineTo(c * len, s * len); h1.lineTo(px, py); h1.closeSubpath();
                QPainterPath h2; h2.moveTo(0, 0); h2.lineTo(c * len, s * len); h2.lineTo(-px, -py); h2.closeSubpath();
                p.setBrush(dark); p.drawPath(h1);
                p.setBrush(paper); p.drawPath(h2);
            };
            for (int i = 0; i < 8; i++) tip((i + 0.5) / 8 * M_PI * 2 - M_PI / 2, R * 0.5, R * 0.05, QColor(164, 52, 40));
            for (int i = 0; i < 4; i++) tip((i + 0.5) / 4 * M_PI * 2 - M_PI / 2, R * 0.72, R * 0.08, QColor(52, 110, 70));
            for (int i = 0; i < 4; i++) tip(i / 4.0 * M_PI * 2 - M_PI / 2, R, R * 0.11, ink);
            // flor-de-lis no norte
            QPainterPath lis;
            lis.moveTo(0, -R * 1.02); lis.quadTo(R * 0.12, -R * 1.18, 0, -R * 1.3); lis.quadTo(-R * 0.12, -R * 1.18, 0, -R * 1.02);
            p.setBrush(QColor(164, 52, 40)); p.drawPath(lis);
            p.setPen(Qt::NoPen); p.setBrush(ink); p.drawEllipse(QPointF(0, 0), R * 0.06, R * 0.06);
            p.restore();
        }
    }
    if (st.mode == QLatin1String("parch") || st.mode == QLatin1String("chart")) {
        // borda do papel envelhecida: queimado leve nos cantos da tela
        QRadialGradient g(QPointF(W / 2, H / 2), std::hypot(W, H) * 0.62);
        const double inner = std::min(W, H) * 0.45 / (std::hypot(W, H) * 0.62);
        g.setColorAt(0, QColor(110, 70, 30, 0));
        g.setColorAt(std::min(0.95, inner), QColor(110, 70, 30, 0));
        g.setColorAt(1, st.mode == QLatin1String("parch") ? QColor(110, 70, 30, 77) : QColor(110, 80, 40, 46));
        p.fillRect(QRectF(0, 0, W, H), g);
    }
    p.restore();
}

} // namespace carto
