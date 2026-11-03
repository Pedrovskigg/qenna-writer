#include "GalaxyCanvas.h"

#include <algorithm>

namespace galaxy {

// Territórios: mancha de cada região, mais forte de longe, quase sumindo dentro dos sistemas.
namespace {
constexpr double REGION_RAD = 0.06, REGION_LINK = 0.42, REGION_NECK = 0.034, REGION_MELT = 0.05;
constexpr int TERR_GRID = 4;
constexpr double TERR_GLOW = 22;
inline double smin(double a, double b)
{
    const double h = std::max(REGION_MELT - std::fabs(a - b), 0.0) / REGION_MELT;
    return std::min(a, b) - h * h * REGION_MELT * 0.25;
}
int signatureOf(const Region& r)
{
    uint32_t sig = uint32_t(r.systems.size());
    for (int id : r.systems) sig += uint32_t(id + 1) * 2654435761u;
    return int(sig);
}
// Desfoque em caixa, três passadas: chega perto de um blur de verdade e é barato na imagem de ¼.
void boxBlur(QImage& img, int r)
{
    const int W = img.width(), H = img.height(), n = 2 * r + 1;
    std::vector<QRgb> tmp(size_t(std::max(W, H)));
    for (int pass = 0; pass < 3; pass++) {
        for (int y = 0; y < H; y++) {
            QRgb* row = reinterpret_cast<QRgb*>(img.scanLine(y));
            int a = 0, rr = 0, g = 0, b = 0;
            for (int i = -r; i <= r; i++) { const QRgb q = row[std::clamp(i, 0, W - 1)]; a += qAlpha(q); rr += qRed(q); g += qGreen(q); b += qBlue(q); }
            for (int x = 0; x < W; x++) {
                tmp[size_t(x)] = qRgba(rr / n, g / n, b / n, a / n);
                const QRgb o = row[std::max(0, x - r)], in = row[std::min(W - 1, x + r + 1)];
                a += qAlpha(in) - qAlpha(o); rr += qRed(in) - qRed(o); g += qGreen(in) - qGreen(o); b += qBlue(in) - qBlue(o);
            }
            std::copy(tmp.begin(), tmp.begin() + W, row);
        }
        const qsizetype bpl = img.bytesPerLine();
        uchar* bits = img.bits();
        for (int x = 0; x < W; x++) {
            const auto at = [&](int y) -> QRgb& { return reinterpret_cast<QRgb*>(bits + bpl * y)[x]; };
            int a = 0, rr = 0, g = 0, b = 0;
            for (int i = -r; i <= r; i++) { const QRgb q = at(std::clamp(i, 0, H - 1)); a += qAlpha(q); rr += qRed(q); g += qGreen(q); b += qBlue(q); }
            for (int y = 0; y < H; y++) {
                tmp[size_t(y)] = qRgba(rr / n, g / n, b / n, a / n);
                const QRgb o = at(std::max(0, y - r)), in = at(std::min(H - 1, y + r + 1));
                a += qAlpha(in) - qAlpha(o); rr += qRed(in) - qRed(o); g += qGreen(in) - qGreen(o); b += qBlue(in) - qBlue(o);
            }
            for (int y = 0; y < H; y++) at(y) = tmp[size_t(y)];
        }
    }
}
// Redução pela média: sem o chuvisco de pular pixels.
QImage shrink(const QImage& src, int sw, int sh)
{
    QImage out(sw, sh, QImage::Format_ARGB32_Premultiplied);
    const int W = src.width(), H = src.height();
    for (int y = 0; y < sh; y++) {
        const int y0 = (y * H) / sh, y1 = std::max(y0 + 1, ((y + 1) * H) / sh);
        QRgb* d = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < sw; x++) {
            const int x0 = (x * W) / sw, x1 = std::max(x0 + 1, ((x + 1) * W) / sw);
            int a = 0, r = 0, g = 0, b = 0, n = 0;
            for (int j = y0; j < y1; j++) {
                const QRgb* row = reinterpret_cast<const QRgb*>(src.constScanLine(j));
                for (int i = x0; i < x1; i++) { const QRgb q = row[i]; a += qAlpha(q); r += qRed(q); g += qGreen(q); b += qBlue(q); n++; }
            }
            d[x] = qRgba(r / n, g / n, b / n, a / n);
        }
    }
    return out;
}
} // namespace

// A região liga os sistemas pelo caminho mais curto (árvore mínima); quem está longe demais vira exclave.
// A forma é um campo de distância até a fronteira (negativo dentro): a bolha de cada sistema e os corredores entre
// eles, fundidos com suavidade. Pintura, clique e foco usam a mesma forma.
const Canvas::RegionPrims& Canvas::regionPrims(const Region& r)
{
    const int sig = signatureOf(r);
    auto it = m_prims.find(r.key);
    if (it != m_prims.end() && it->n == sig) return it.value();
    RegionPrims P;
    P.n = sig;
    QList<int> ids = r.systems.values();
    std::sort(ids.begin(), ids.end());
    for (int id : ids) P.pts.push_back(doc->gal->systems[size_t(id)].get());
    std::vector<System*> left(P.pts.begin() + (P.pts.empty() ? 0 : 1), P.pts.end()), inside;
    if (!P.pts.empty()) inside.push_back(P.pts[0]);
    while (!left.empty()) {
        size_t bi = 0;
        System* bo = inside[0];
        double bd = 1e18;
        for (size_t i = 0; i < left.size(); i++) for (System* o : inside) {
            const double d = std::hypot(o->x - left[i]->x, o->y - left[i]->y);
            if (d < bd) { bd = d; bi = i; bo = o; }
        }
        System* p = left[bi];
        left.erase(left.begin() + long(bi));
        inside.push_back(p);
        if (bd < REGION_LINK) { const double dx = bo->x - p->x, dy = bo->y - p->y; P.links.push_back({ p->x, p->y, dx, dy, dx * dx + dy * dy }); }
    }
    return m_prims.insert(r.key, std::move(P)).value();
}
double Canvas::regionDist(const RegionPrims& P, double x, double y) const
{
    double d = 1e18;
    for (const System* p : P.pts) { const double a = x - p->x, b = y - p->y; d = smin(d, std::sqrt(a * a + b * b) - REGION_RAD); }
    for (const auto& s : P.links) {
        const double a = x - s.x, b = y - s.y, u = std::max(0.0, std::min(1.0, (a * s.dx + b * s.dy) / s.l)), e = a - s.dx * u, f = b - s.dy * u;
        d = smin(d, std::sqrt(e * e + f * f) - REGION_NECK);
    }
    return d;
}
// A região embaixo do ponteiro: o território inteiro é clicável, não só o nome.
QString Canvas::regionAt(double sx, double sy)
{
    if (!layTerr() || cam.z / fitZ() > 18) return QString();
    double wx, wy;
    toWorld(sx, sy, wx, wy);
    for (const auto& r : doc->regions) if (!r->systems.isEmpty() && regionDist(regionPrims(*r), wx, wy) < 0) return r->key;
    return QString();
}
// O território na tela: linha fina da fronteira, cor forte na borda que some pro miolo e um brilho leve por fora.
// O campo é calculado numa grade de 4 px e interpolado; o resultado fica guardado enquanto a câmera não mexe.
Canvas::TerrLayer& Canvas::territoryLayer(const Region& r)
{
    const bool withMask = m_focusShown == r.key;
    const QPointF vc = viewCenter();
    const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9").arg(cam.x, 0, 'g', 12).arg(cam.y, 0, 'g', 12).arg(cam.z, 0, 'g', 12).arg(cam.h, 0, 'g', 10).arg(cam.tilt, 0, 'g', 10).arg(cam.yaw, 0, 'g', 10).arg(width()).arg(height()).arg(QStringLiteral("%1,%2,%3,%4,%5,%6").arg(vc.x()).arg(vc.y()).arg(r.color).arg(signatureOf(r)).arg(int(withMask)).arg(m_sk ? m_sk->id : QString()));
    TerrLayer& e = m_terr[r.key];
    if (e.key == key) return e;
    e.key = key;
    const RegionPrims& P = regionPrims(r);
    const double z = cam.z, cT = std::cos(cam.tilt);
    const int G = TERR_GRID, W = width(), H = height();
    const double GLOW = TERR_GLOW;
    const double padX = (REGION_RAD + REGION_MELT) * z + GLOW + G, padY = (REGION_RAD + REGION_MELT) * z * std::max(cT, 0.4) + GLOW + G;
    double fx0 = W, fy0 = H, fx1 = 0, fy1 = 0;
    for (const System* s : P.pts) { double x, y; proj(s->x, s->y, 0, x, y); fx0 = std::min(fx0, x - padX); fy0 = std::min(fy0, y - padY); fx1 = std::max(fx1, x + padX); fy1 = std::max(fy1, y + padY); }
    const int x0 = std::max(0, int(std::floor(fx0))), y0 = std::max(0, int(std::floor(fy0))), x1 = std::min(W, int(std::ceil(fx1))), y1 = std::min(H, int(std::ceil(fy1)));
    e.empty = x1 <= x0 || y1 <= y0;
    if (e.empty) return e;
    const int w = x1 - x0, h = y1 - y0, gw = (w + G - 1) / G, gh = (h + G - 1) / G;
    // Da tela pro mundo: a inversa exata da projeção do chão da galáxia.
    double ox, oy, ex0, ex1, ey0, ey1;
    proj(0, 0, 0, ox, oy); proj(1, 0, 0, ex0, ex1); proj(0, 1, 0, ey0, ey1);
    const double a = ex0 - ox, b = ey0 - ox, c = ex1 - oy, d = ey1 - oy, det = a * d - b * c;
    const double ia = d / det, ib = -b / det, ic = -c / det, id = a / det;
    const int row = gw + 1;
    e.field.resize(size_t(row) * size_t(gh + 1));
    float* fd = e.field.data();
    for (int j = 0; j <= gh; j++) for (int i = 0; i <= gw; i++) {
        const double X = x0 + i * G - ox, Y = y0 + j * G - oy;
        fd[j * row + i] = float(regionDist(P, ia * X + ib * Y, ic * X + id * Y) * z);
    }
    // Tabela de cor pela distância, em quartos de pixel.
    const double RIM = std::max(10.0, std::min(90.0, 0.03 * z));
    const QColor rc = regionInk(r);
    const int cr = rc.red(), cg = rc.green(), cb = rc.blue();
    const int lutN = int(std::ceil((RIM + GLOW) * 4)) + 1;
    std::vector<uint32_t> lut(static_cast<size_t>(lutN)), lutM(static_cast<size_t>(lutN));
    for (int n = 0; n < lutN; n++) {
        const double q = n / 4.0 - RIM, line = 1 - smooth(0.55, 1.35, std::fabs(q));
        const double fill = q < 0 ? 0.03 + 0.2 * std::pow(1 - smooth(0, RIM, -q), 2) : 0.1 * (1 - smooth(0, GLOW, q));
        const int A = int(std::floor(std::max(m_sk && m_sk->mono ? fill * 0.55 : fill, line * 0.9) * 255 + 0.5));
        const double K = m_sk ? 0 : line * 0.3;
        lut[size_t(n)] = uint32_t(A) << 24 | uint32_t(cb + (255 - cb) * K + 0.5) << 16 | uint32_t(cg + (255 - cg) * K + 0.5) << 8 | uint32_t(cr + (255 - cr) * K + 0.5);
        lutM[size_t(n)] = uint32_t((1 - smooth(-1.5, 1.5, q)) * 255 + 0.5) << 24 | 0xffffffu;
    }
    QImage img(w, h, QImage::Format_RGBA8888), imgM;
    img.fill(Qt::transparent);
    if (withMask) { imgM = QImage(w, h, QImage::Format_RGBA8888); imgM.fill(Qt::transparent); }
    const uint32_t core = lut[0], coreM = lutM[0];
    for (int j = 0; j < gh; j++) for (int i = 0; i < gw; i++) {
        const int k = j * row + i;
        const double d00 = fd[k], d10 = fd[k + 1], d01 = fd[k + row], d11 = fd[k + row + 1];
        const double lo = std::min({ d00, d10, d01, d11 }), hi = std::max({ d00, d10, d01, d11 });
        if (lo >= GLOW) continue;   // fora de tudo
        const int xs = i * G, ys = j * G, xe = std::min(w, xs + G), ye = std::min(h, ys + G);
        for (int y = ys; y < ye; y++) {
            uint32_t* px = reinterpret_cast<uint32_t*>(img.scanLine(y));
            uint32_t* pxM = withMask ? reinterpret_cast<uint32_t*>(imgM.scanLine(y)) : nullptr;
            if (hi <= -RIM) {   // miolo: cor única
                for (int x = xs; x < xe; x++) { px[x] = core; if (pxM) pxM[x] = coreM; }
                continue;
            }
            const double v = double(y - ys) / G, dl = d00 + (d01 - d00) * v, dr = d10 + (d11 - d10) * v;
            for (int x = xs; x < xe; x++) {
                const double q = dl + (dr - dl) * (double(x - xs) / G);
                if (q >= GLOW) continue;
                const int n = q <= -RIM ? 0 : int(std::floor((q + RIM) * 4 + 0.5));
                px[x] = lut[size_t(n)];
                if (pxM) pxM[x] = lutM[size_t(n)];
            }
        }
    }
    e.img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    e.mask = withMask ? imgM.convertToFormat(QImage::Format_ARGB32_Premultiplied) : QImage();
    e.x = x0; e.y = y0; e.w = w; e.h = h;
    return e;
}
void Canvas::drawRegions(QPainter& p, double zoomK, const std::vector<Region*>& list, double mul)
{
    const double k = 1 - smooth(8, 60, zoomK);
    if (k < 0.02) return;
    p.save();
    for (Region* r : list) {
        if (r->systems.isEmpty()) continue;
        TerrLayer& e = territoryLayer(*r);
        if (e.empty) continue;
        p.setOpacity(std::min(1.0, k * mul * (m_selPath.contains(r->key) || m_hover == r->key ? 1 : 0.72) * (view == QLatin1String("atlas") ? 1.5 : 1)));
        p.drawImage(QRectF(e.x, e.y, e.w, e.h), e.img);
    }
    p.restore();
}
// O Modo Foco: o que está fora do território fica desfocado e calmo; o de dentro, nítido, recortado pela fronteira.
void Canvas::paintFocus(QPainter& p, Region& r, double zoomK, double cT, std::vector<Label>& labels)
{
    const double f = smooth(0, 1, m_focusAmt);
    const QPointF vc = viewCenter();
    const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9").arg(cam.x, 0, 'g', 12).arg(cam.y, 0, 'g', 12).arg(cam.z, 0, 'g', 12).arg(cam.h, 0, 'g', 10).arg(cam.tilt, 0, 'g', 10).arg(cam.yaw, 0, 'g', 10).arg(vc.x()).arg(vc.y()).arg(QStringLiteral("%1,%2,%3,%4,%5").arg(r.key).arg(int(layTerr())).arg(int(doc->builds.size())).arg(m_frame.width()).arg(m_frame.height()) + (m_sk ? m_sk->id : QString()));
    if (m_focusAmt < 1 || key != m_fx.key) {
        drawBackdrop(p, zoomK);
        const QImage sharp = static_cast<const QImage&>(m_frame).copy();
        if (layTerr()) {
            std::vector<Region*> others;
            for (const auto& o : doc->regions) if (o.get() != &r) others.push_back(o.get());
            drawRegions(p, zoomK, others);
        }
        // Os sistemas de fora entram só no desfoque: não ganham esfera viva nem clique.
        m_liveBudget = 0;
        std::vector<Label> outside;
        for (const auto& s : doc->gal->systems) if (!s->gone && !r.systems.contains(s->id)) drawSystemNode(p, *s, outside, cam.z, cT, 0);
        drawDeepBuilds(p, outside, cam.z, cT);
        m_hits.clear(); m_liveHits.clear(); m_ringHits.clear(); m_liveBudget = 2;
        if (m_focusAmt < 0.98) for (Label& l : outside) { l.alpha = (l.hasAlpha ? l.alpha : 1) * (1 - m_focusAmt); l.hasAlpha = true; labels.push_back(l); }
        const int sw = int(std::ceil(width() * 0.25)), sh = int(std::ceil(height() * 0.25));
        m_fx.blur = shrink(m_frame, std::max(1, sw), std::max(1, sh));
        boxBlur(m_fx.blur, 2);
        // A parte nítida é recortada pela própria fronteira, com o fundo mais calmo pros sistemas chamarem o olho.
        TerrLayer& terr = territoryLayer(r);
        QImage patch = sharp.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        patch.setDevicePixelRatio(1);
        {
            QPainter pg(&patch);
            if (terr.empty) { pg.setCompositionMode(QPainter::CompositionMode_Clear); pg.fillRect(patch.rect(), Qt::transparent); }
            else {
                // Sem máscara fora do retângulo do território, tudo some lá também.
                pg.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                QImage full(patch.size(), QImage::Format_ARGB32_Premultiplied);
                full.fill(Qt::transparent);
                {
                    QPainter q(&full);
                    q.setRenderHint(QPainter::SmoothPixmapTransform, true);
                    q.scale(m_dpr, m_dpr);
                    q.drawImage(QRectF(terr.x, terr.y, terr.w, terr.h), terr.mask);
                }
                pg.drawImage(0, 0, full);
            }
            pg.setCompositionMode(QPainter::CompositionMode_SourceAtop);
            QColor calm = m_sk ? m_sk->paperC() : QColor(3, 5, 10);
            calm.setAlphaF(m_sk ? 0.2f : 0.4f);
            pg.fillRect(patch.rect(), calm);
        }
        m_fx.patch = patch;
        m_fx.key = key;
    }
    const QRectF all(0, 0, width(), height());
    p.save();
    p.setOpacity(f);
    p.drawImage(all, m_fx.blur);
    p.setOpacity(1);
    QColor veil = m_sk ? m_sk->paperC() : QColor(3, 5, 10);
    veil.setAlphaF(float((m_sk ? 0.5 : 0.42) * f));
    p.fillRect(all, veil);
    p.setOpacity(f);
    p.drawImage(all, m_fx.patch);
    p.restore();
    if (layTerr()) drawRegions(p, zoomK, { &r }, f);
}
// Clique no vazio: dentro da área não faz nada; fora dela, sai do foco.
bool Canvas::inFocusArea(double sx, double sy)
{
    Region* r = nullptr;
    for (const auto& o : doc->regions) if (o->key == focusKey) r = o.get();
    if (!r) return true;
    double wx, wy;
    toWorld(sx, sy, wx, wy);
    if (regionDist(regionPrims(*r), wx, wy) < 0) return true;
    // Com o mapa inclinado e de perto, o sistema flutua longe da mancha do chão.
    for (int id : r->systems) {
        const System& s = *doc->gal->systems[size_t(id)];
        double x, y;
        proj(s.x, s.y, s.layH, x, y);
        if (std::hypot(x - sx, y - sy) < s.layR * cam.z * 1.15) return true;
    }
    return false;
}

} // namespace galaxy
