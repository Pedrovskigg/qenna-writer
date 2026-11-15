#include "CartoImage.h"
#include "CartoProf.h"

#include <QColor>
#include <QPainter>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>

namespace carto {

namespace {
// cor no espaço Lab (o do olho): o cinza-azulado do mar e o cinza-pardo da montanha ficam longe, o raso e o
// fundo do mesmo mar ficam perto (no RGB os dois pares davam quase a mesma distância)
struct Lab { float L, a, b; };
Lab toLab(int r, int g, int b)
{
    static const std::array<float, 256> lin = [] {
        std::array<float, 256> t{};
        for (int i = 0; i < 256; i++) { const double c = i / 255.0; t[i] = static_cast<float>(c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4)); }
        return t;
    }();
    const double R = lin[r], G = lin[g], B = lin[b];
    const double X = (0.4124 * R + 0.3576 * G + 0.1805 * B) / 0.95047, Y = 0.2126 * R + 0.7152 * G + 0.0722 * B, Z = (0.0193 * R + 0.1192 * G + 0.9505 * B) / 1.08883;
    auto f = [](double t) { return t > 0.008856 ? std::cbrt(t) : 7.787 * t + 16.0 / 116; };
    const double fx = f(X), fy = f(Y), fz = f(Z);
    return { static_cast<float>(116 * fy - 16), static_cast<float>(500 * (fx - fy)), static_cast<float>(200 * (fy - fz)) };
}
inline double labDist(const Lab& p, const Lab& q) { return std::sqrt((p.L - q.L) * (p.L - q.L) + (p.a - q.a) * (p.a - q.a) + (p.b - q.b) * (p.b - q.b)); }

// borrão de caixa por canal (tira a textura de onda e o grão do papel antes de comparar cor)
std::vector<uint8_t> blurRGB(const uchar* src, int W, int H, int stride, int r)
{
    const size_t N = static_cast<size_t>(W) * H;
    std::vector<uint8_t> a(N * 3), b(N * 3);
    for (int y = 0; y < H; y++) {
        const uchar* row = src + static_cast<size_t>(y) * stride;
        for (int x = 0; x < W; x++) for (int c = 0; c < 3; c++) a[(static_cast<size_t>(y) * W + x) * 3 + c] = row[x * 4 + c];
    }
    if (r <= 0) return a;
    const int n = 2 * r + 1;
    for (int y = 0; y < H; y++) for (int c = 0; c < 3; c++) {
        int acc = 0;
        auto at = [&](int x) { return a[(static_cast<size_t>(y) * W + std::clamp(x, 0, W - 1)) * 3 + c]; };
        for (int k = -r; k <= r; k++) acc += at(k);
        for (int x = 0; x < W; x++) {
            b[(static_cast<size_t>(y) * W + x) * 3 + c] = static_cast<uint8_t>(acc / n);
            acc += at(x + r + 1) - at(x - r);
        }
    }
    for (int x = 0; x < W; x++) for (int c = 0; c < 3; c++) {
        int acc = 0;
        auto at = [&](int y) { return b[(static_cast<size_t>(std::clamp(y, 0, H - 1)) * W + x) * 3 + c]; };
        for (int k = -r; k <= r; k++) acc += at(k);
        for (int y = 0; y < H; y++) {
            a[(static_cast<size_t>(y) * W + x) * 3 + c] = static_cast<uint8_t>(acc / n);
            acc += at(y + r + 1) - at(y - r);
        }
    }
    return a;
}

// pedaços ligados (4 vizinhos) de quem passa no teste; devolve o tamanho de cada pedaço por pixel
template <class F>
void components(int W, int H, F inSet, const std::function<void(const std::vector<int>&)>& each)
{
    const size_t N = static_cast<size_t>(W) * H;
    std::vector<uint8_t> seen(N, 0);
    std::vector<int> stack, cells;
    for (size_t s0 = 0; s0 < N; s0++) {
        if (seen[s0] || !inSet(static_cast<int>(s0))) continue;
        stack.assign(1, static_cast<int>(s0));
        seen[s0] = 1;
        cells.clear();
        while (!stack.empty()) {
            const int i = stack.back(); stack.pop_back();
            cells.push_back(i);
            const int x = i % W, y = i / W;
            const int nb[4] = { x > 0 ? i - 1 : -1, x < W - 1 ? i + 1 : -1, y > 0 ? i - W : -1, y < H - 1 ? i + W : -1 };
            for (int j : nb) if (j >= 0 && !seen[j] && inSet(j)) { seen[j] = 1; stack.push_back(j); }
        }
        each(cells);
    }
}

std::shared_ptr<Backdrop> makeBackdrop(const QImage& fitted)
{
    auto bd = std::make_shared<Backdrop>();
    QImage cur = fitted.convertToFormat(QImage::Format_RGBA8888);
    for (int L = 0; L < 4; L++) {
        Backdrop::Level lv;
        lv.W = cur.width(); lv.H = cur.height();
        lv.rgba.resize(static_cast<size_t>(lv.W) * lv.H * 4);
        for (int y = 0; y < lv.H; y++) std::copy_n(cur.constScanLine(y), lv.W * 4, lv.rgba.data() + static_cast<size_t>(y) * lv.W * 4);
        bd->levels.push_back(std::move(lv));
        if (std::max(cur.width(), cur.height()) < 512) break;
        cur = cur.scaled(std::max(1, cur.width() / 2), std::max(1, cur.height() / 2), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    // a cor da borda: a média da moldura de fora (é o que a mesa mostra em volta da imagem)
    const Backdrop::Level& l0 = bd->levels.front();
    const int band = std::max(1, std::min(l0.W, l0.H) / 100);
    double s[3] = { 0, 0, 0 }; long long n = 0;
    for (int y = 0; y < l0.H; y++) for (int x = 0; x < l0.W; x++) {
        if (x >= band && x < l0.W - band && y >= band && y < l0.H - band) { x = l0.W - band - 1; continue; }
        const uint8_t* p = l0.rgba.data() + (static_cast<size_t>(y) * l0.W + x) * 4;
        s[0] += p[0]; s[1] += p[1]; s[2] += p[2]; n++;
    }
    if (n > 0) for (int c = 0; c < 3; c++) bd->edge[c] = static_cast<uint8_t>(std::lround(s[c] / n));
    bd->key = QUuid::createUuid().toString(QUuid::Id128).left(10);
    return bd;
}
}

QSize imageWorldSize(QSize img, int size)
{
    const SizePreset& sp = sizePreset(size);
    const double area = double((sp.gw - 1) * 5 + 1) * ((sp.gh - 1) * 5 + 1);
    const double a = img.height() > 0 ? std::clamp(double(img.width()) / img.height(), 0.25, 4.0) : 4.0 / 3;
    const int W = std::max(64, static_cast<int>(std::lround(std::sqrt(area * a))));
    const int H = std::max(64, static_cast<int>(std::lround(area / W)));
    // a imagem que já é o chão de um mapa (Regenerar) volta no mesmo tamanho, sem 1 px de diferença do arredondamento
    if (std::abs(img.width() - W) <= 2 && std::abs(img.height() - H) <= 2) return img;
    return { W, H };
}

int suggestedImageSize(QSize img)
{
    int best = 1;
    for (int s = 1; s < 6; s++) if (imageWorldSize(img, s).width() <= img.width() * 1.05) best = s;
    return best;
}

QImage fitImageToWorld(const QImage& src, int size)
{
    if (src.isNull()) return QImage();
    const QSize ws = imageWorldSize(src.size(), size);
    QImage flat(src.size(), QImage::Format_RGBA8888);
    flat.fill(QColor(244, 236, 216));   // parte transparente da imagem vira papel
    {
        QPainter p(&flat);
        p.drawImage(0, 0, src);
    }
    return flat.scaled(ws, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);
}

std::vector<uint8_t> seaMask(const QImage& imgIn, int sea)
{
    CARTO_PROF("mar da imagem");
    const QImage img = imgIn.format() == QImage::Format_RGBA8888 ? imgIn : imgIn.convertToFormat(QImage::Format_RGBA8888);
    const int W = img.width(), H = img.height();
    const size_t N = static_cast<size_t>(W) * H;
    std::vector<uint8_t> out(N, 0);
    if (sea <= 0 || W < 8 || H < 8) return out;
    const std::vector<uint8_t> c = blurRGB(img.constBits(), W, H, img.bytesPerLine(), std::max(1, std::min(W, H) / 400));
    auto px = [&](size_t i, int k) { return int(c[i * 3 + k]); };

    // a cor do mar: a mais comum na moldura de fora (16 tons por canal)
    const int band = std::max(2, std::min(W, H) / 50);
    std::vector<int> hist(4096, 0);
    std::vector<size_t> border;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        if (x >= band && x < W - band && y >= band && y < H - band) { x = W - band - 1; continue; }
        const size_t i = static_cast<size_t>(y) * W + x;
        border.push_back(i);
        hist[(px(i, 0) >> 4) << 8 | (px(i, 1) >> 4) << 4 | (px(i, 2) >> 4)]++;
    }
    const int mode = int(std::max_element(hist.begin(), hist.end()) - hist.begin());
    double sr = 0, sg = 0, sb = 0; int sn = 0;
    for (size_t i : border) if (((px(i, 0) >> 4) << 8 | (px(i, 1) >> 4) << 4 | (px(i, 2) >> 4)) == mode) { sr += px(i, 0); sg += px(i, 1); sb += px(i, 2); sn++; }
    if (sn == 0) return out;
    const Lab seed = toLab(int(std::lround(sr / sn)), int(std::lround(sg / sn)), int(std::lround(sb / sn)));
    const double T = 3 + sea * 0.22;   // 50 = 14 (ΔE): o raso e o fundo do mesmo mar entram, a montanha cinza e a areia não
    auto dSeed = [&](size_t i) { return labDist(toLab(px(i, 0), px(i, 1), px(i, 2)), seed); };

    // a moldura precisa ser em boa parte dessa cor; senão a imagem não tem mar na borda (mapa de uma terra só)
    int close = 0;
    for (size_t i : border) if (dSeed(i) < T) close++;
    const bool borderSea = close >= int(border.size() * 0.2);

    // 1. o mar que encosta na borda: cresce pela cor (degradê de raso a fundo cabe na tolerância; seguir degradê
    //    sem limite vazava pra terra clara de cor parecida, como a do Atlas)
    std::vector<float> d(N);
    for (size_t i = 0; i < N; i++) d[i] = static_cast<float>(dSeed(i));
    if (borderSea) {
        std::deque<int> q;
        for (size_t i : border) if (d[i] < T && !out[i]) { out[i] = 1; q.push_back(static_cast<int>(i)); }
        while (!q.empty()) {
            const int i = q.front(); q.pop_front();
            const int x = i % W, y = i / W;
            const int nb[4] = { x > 0 ? i - 1 : -1, x < W - 1 ? i + 1 : -1, y > 0 ? i - W : -1, y < H - 1 ? i + W : -1 };
            for (int j : nb) {
                if (j < 0 || out[j]) continue;
                if (d[j] < T) { out[j] = 1; q.push_back(j); }
            }
        }
    }
    // 2. lagos e mares de dentro: da mesma cor e grandes o bastante (letra e ícone não viram lago)
    const int minLake = std::max(24, static_cast<int>(N * 0.00025));
    components(W, H, [&](int i) { return !out[i] && d[i] < T * 0.7; }, [&](const std::vector<int>& cells) {
        if (static_cast<int>(cells.size()) >= minLake) for (int i : cells) out[i] = 1;
    });
    // 3. limpeza: pingo de terra no meio do mar vira mar, e risco fino também (letra do nome do oceano, rosa dos
    //    ventos, ondinha desenhada): terra de verdade tem miolo, longe do mar; letra é só traço
    const int minBit = std::max(10, static_cast<int>(N * 0.00004));
    const int maxThinArea = std::max(400, static_cast<int>(N * 0.002));
    const float thin = static_cast<float>(std::max(2.5, std::min(W, H) / 200.0));
    std::vector<float> inland(N);   // distância até o mar (chanfro 3-4, em px)
    for (size_t i = 0; i < N; i++) inland[i] = out[i] ? 0.0f : 1e9f;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        float& v = inland[static_cast<size_t>(y) * W + x];
        if (v == 0) continue;
        if (x > 0) v = std::min(v, inland[static_cast<size_t>(y) * W + x - 1] + 1.0f);
        if (y > 0) {
            v = std::min(v, inland[static_cast<size_t>(y - 1) * W + x] + 1.0f);
            if (x > 0) v = std::min(v, inland[static_cast<size_t>(y - 1) * W + x - 1] + 1.414f);
            if (x < W - 1) v = std::min(v, inland[static_cast<size_t>(y - 1) * W + x + 1] + 1.414f);
        }
    }
    for (int y = H - 1; y >= 0; y--) for (int x = W - 1; x >= 0; x--) {
        float& v = inland[static_cast<size_t>(y) * W + x];
        if (v == 0) continue;
        if (x < W - 1) v = std::min(v, inland[static_cast<size_t>(y) * W + x + 1] + 1.0f);
        if (y < H - 1) {
            v = std::min(v, inland[static_cast<size_t>(y + 1) * W + x] + 1.0f);
            if (x < W - 1) v = std::min(v, inland[static_cast<size_t>(y + 1) * W + x + 1] + 1.414f);
            if (x > 0) v = std::min(v, inland[static_cast<size_t>(y + 1) * W + x - 1] + 1.414f);
        }
    }
    const bool anySea = std::any_of(out.begin(), out.end(), [](uint8_t v) { return v != 0; });
    if (anySea) components(W, H, [&](int i) { return !out[i]; }, [&](const std::vector<int>& cells) {
        const int n = static_cast<int>(cells.size());
        bool drop = n < minBit;
        if (!drop && n < maxThinArea) {
            float core = 0;
            for (int i : cells) core = std::max(core, inland[static_cast<size_t>(i)]);
            drop = core < thin;
        }
        if (drop) for (int i : cells) out[i] = 1;
    });
    components(W, H, [&](int i) { return out[i] != 0; }, [&](const std::vector<int>& cells) {
        if (static_cast<int>(cells.size()) < minBit) for (int i : cells) out[i] = 0;
    });
    return out;
}

World imageWorld(const QImage& fitted, int sea, int seed, int size, const QString& climate)
{
    CARTO_PROF("mundo da imagem");
    World w;
    w.W = fitted.width(); w.H = fitted.height();
    const size_t N = static_cast<size_t>(w.W) * w.H;
    const std::vector<uint8_t> mask = seaMask(fitted, sea);
    // chão plano: terra um tico acima do mar, mar um tico abaixo (a costa cruza o zero suave, sem serrilhado)
    std::vector<float> s(N), ones(N, 1.0f);
    for (size_t i = 0; i < N; i++) s[i] = mask[i] ? 1.0f : 0.0f;
    boxBlurWeighted({ &s }, ones, w.W, w.H, 1);
    w.h.resize(N);
    for (size_t i = 0; i < N; i++) w.h[i] = (0.5f - s[i]) * 0.16f;
    w.m.assign(N, 0.5f);
    w.k.assign(N, 0.0f);
    w.seed = seed; w.water = sea; w.type = QStringLiteral("image"); w.size = size; w.climate = climate;
    w.ref = 1.0;
    w.backdrop = makeBackdrop(fitted);
    return w;
}

QImage backdropImage(const World& w)
{
    if (!w.backdrop || w.backdrop->levels.empty()) return QImage();
    const Backdrop::Level& l = w.backdrop->levels.front();
    return QImage(l.rgba.data(), l.W, l.H, l.W * 4, QImage::Format_RGBA8888).copy();
}

QImage seaPreview(const QImage& fitted, const std::vector<uint8_t>& sea, int targetW)
{
    if (fitted.isNull()) return QImage();
    const int tw = std::max(32, std::min(targetW, fitted.width()));
    const int th = std::max(24, static_cast<int>(std::lround(double(tw) * fitted.height() / fitted.width())));
    QImage out = fitted.scaled(tw, th, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);
    if (sea.size() != static_cast<size_t>(fitted.width()) * fitted.height()) return out;
    // mar: azul translúcido liso (listra dá enjoo), com a costa marcada
    const int W = fitted.width(), H = fitted.height();
    auto seaAt = [&](int x, int y) {
        const int sx = std::clamp(static_cast<int>((x + 0.5) * W / tw), 0, W - 1), sy = std::clamp(static_cast<int>((y + 0.5) * H / th), 0, H - 1);
        return sea[static_cast<size_t>(sy) * W + sx] != 0;
    };
    std::vector<uint8_t> m(static_cast<size_t>(tw) * th);
    for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) m[static_cast<size_t>(y) * tw + x] = seaAt(x, y);
    for (int y = 0; y < th; y++) {
        uchar* row = out.scanLine(y);
        for (int x = 0; x < tw; x++) {
            const bool s = m[static_cast<size_t>(y) * tw + x];
            const bool edge = s && ((x > 0 && !m[static_cast<size_t>(y) * tw + x - 1]) || (x < tw - 1 && !m[static_cast<size_t>(y) * tw + x + 1])
                                    || (y > 0 && !m[static_cast<size_t>(y - 1) * tw + x]) || (y < th - 1 && !m[static_cast<size_t>(y + 1) * tw + x]));
            if (!s) continue;
            const double a = edge ? 0.9 : 0.5;
            const int cr = edge ? 255 : 40, cg = edge ? 255 : 120, cb = edge ? 255 : 220;
            row[x * 4] = static_cast<uchar>(row[x * 4] * (1 - a) + cr * a);
            row[x * 4 + 1] = static_cast<uchar>(row[x * 4 + 1] * (1 - a) + cg * a);
            row[x * 4 + 2] = static_cast<uchar>(row[x * 4 + 2] * (1 - a) + cb * a);
        }
    }
    return out;
}

} // namespace carto
