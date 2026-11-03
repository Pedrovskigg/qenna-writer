#include "GalaxyBlackHole.h"
#include "GalaxyCore.h"

#include <QCoreApplication>
#include <QImage>
#include <QPainter>
#include <QPointer>
#include <QThreadPool>
#include <QWidget>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <random>
#include <vector>

namespace galaxy {

namespace {

constexpr double PI_V = 3.14159265358979323846;
constexpr double SHADOW_B = 5.196152422706632;  // o raio da sombra, em massas: 3√3
constexpr double DISC_IN = 4.6, DISC_OUT = 22.0;
constexpr int ROWS = 128, COLS = 2048;
constexpr int INC_STEPS = 60;  // de 1,5 em 1,5 grau

// A textura do gás. Duas cópias giram em velocidades diferentes e a mistura delas muda com o raio:
// o miolo corre mais que a borda, e os fios não se enrolam nem racham com o tempo.
const uint8_t* gasTexture()
{
    static std::vector<uint8_t> tab = [] {
        std::vector<uint8_t> out(size_t(ROWS) * COLS);
        std::mt19937 rng(7741);
        std::uniform_real_distribution<double> U(0, 1);
        std::vector<double> sum(out.size(), 0.0);
        double amp = 1, total = 0;
        // Fios compridos: pouca variação ao longo da volta, bastante de um raio pro outro.
        for (const auto& [f, rowsF] : { std::pair<int, int>{ 3, 7 }, { 6, 15 }, { 13, 31 }, { 27, 61 } }) {
            std::vector<double> grid(size_t(f) * rowsF);
            for (double& v : grid) v = U(rng);
            for (int r = 0; r < ROWS; r++) {
                const double fy = double(r) / ROWS * rowsF;
                const int y0 = int(fy) % rowsF, y1 = (y0 + 1) % rowsF;
                const double ty = fy - int(fy), sy = ty * ty * (3 - 2 * ty);
                for (int c = 0; c < COLS; c++) {
                    const double fx = double(c) / COLS * f;
                    const int x0 = int(fx) % f, x1 = (x0 + 1) % f;
                    const double tx = fx - int(fx), sx = tx * tx * (3 - 2 * tx);
                    const double top = grid[size_t(y0) * f + x0] * (1 - sx) + grid[size_t(y0) * f + x1] * sx;
                    const double bot = grid[size_t(y1) * f + x0] * (1 - sx) + grid[size_t(y1) * f + x1] * sx;
                    sum[size_t(r) * COLS + c] += amp * (top * (1 - sy) + bot * sy);
                }
            }
            total += amp;
            amp *= 0.68;
        }
        for (size_t i = 0; i < out.size(); i++) {
            const double n = 0.6 + 0.95 * std::pow(std::clamp((sum[i] / total - 0.24) / 0.52, 0.0, 1.0), 1.4);
            out[i] = uint8_t(std::clamp(n * 128, 0.0, 255.0));
        }
        return out;
    }();
    return tab.data();
}

// Um pedaço de gás que aparece num ponto da imagem.
struct GasPx {
    uint32_t pos;
    uint16_t th;
    uint16_t r, g, b;
    uint8_t row;
};

// Tudo que depende só do ângulo de visão e do tamanho: guardado, cada quadro só gira a textura.
struct Sheet {
    int size = 0, inc = 0;
    QImage base, img;
    std::vector<GasPx> gas;
    double shownT = -1e18;
    uint64_t used = 0;
};

double smooth01(double t) { t = std::clamp(t, 0.0, 1.0); return t * t * (3 - 2 * t); }

// Quanto a luz se curva ao passar a uma distância b do buraco negro.
double deflection(double b)
{
    const double q = SHADOW_B / b;
    return -std::log(std::max(1e-6, 1 - q)) * (0.77 + 0.23 * q * q * q * q) - 0.4 * std::pow(q, 6);
}

void gasColor(double temp, double out[3])
{
    static const double ramp[4][3] = { { 196, 66, 18 }, { 244, 122, 42 }, { 255, 182, 92 }, { 255, 232, 190 } };
    const double f = std::clamp(temp, 0.0, 1.0) * 3;
    const int i = std::min(2, int(f));
    const double t = f - i;
    for (int c = 0; c < 3; c++) out[c] = ramp[i][c] * (1 - t) + ramp[i + 1][c] * t;
}

std::unique_ptr<Sheet> buildSheet(int size, int inc)
{
    auto sh = std::make_unique<Sheet>();
    sh->size = size; sh->inc = inc;
    sh->base = QImage(size, size, QImage::Format_ARGB32_Premultiplied);
    sh->base.fill(Qt::transparent);
    sh->gas.reserve(size_t(size) * size / 2);
    const double angle = (double(inc) + 0.5) * 89.5 / INC_STEPS * PI_V / 180;
    const double sinI = std::sin(angle), cosI = std::cos(angle);
    const double half = size / 2.0, E = BLACK_HOLE_EXTENT, edge = 1.5 * E / half;
    const double logIn = std::log(DISC_IN), logSpan = std::log(DISC_OUT) - logIn;

    for (int y = 0; y < size; y++) {
        auto* line = reinterpret_cast<uint32_t*>(sh->base.scanLine(y));
        for (int x = 0; x < size; x++) {
            const double X = (x + 0.5 - half) / half * E, Y = -(y + 0.5 - half) / half * E;
            const double rho = std::hypot(X, Y);
            if (rho >= E) continue;
            const double safe = std::max(1e-6, rho), cosP = X / safe, sinP = Y / safe;
            const double b = rho * SHADOW_B;
            const double outer = 1 - smooth01((rho - E * 0.7) / (E * 0.3));

            // O que não muda: a sombra, o anel fino da luz presa e o brilho em volta.
            const double dist = std::max(0.0, rho - 1);
            const double side = 1 - 0.42 * cosP * sinI;
            const double ring = (1.5 * std::exp(-dist / 0.016) + 0.34 * std::exp(-dist / 0.13)) * side;
            const double shadow = std::clamp((1 + edge * 0.5 - rho) / edge, 0.0, 1.0);
            const double lit = 1 - shadow;
            const double halo = 0.15 * std::exp(-dist / 1.25) * outer * lit;
            const double glow = lit * (1 - std::exp(-ring * 1.7));
            const double rr = glow * 255 + halo * 214;
            const double gg = glow * 228 + halo * 86;
            const double bb = glow * 182 + halo * 30;
            const int ir = std::min(255, int(rr)), ig = std::min(255, int(gg)), ib = std::min(255, int(bb));
            const int ia = std::max({ int(shadow * 255), ir, ig, ib });
            line[x] = (uint32_t(ia) << 24) | (uint32_t(ir) << 16) | (uint32_t(ig) << 8) | uint32_t(ib);

            // O gás: cada raio de luz cruza o plano do disco no máximo duas vezes.
            const double A = cosI, B = sinP * sinI;
            const double psi0 = std::atan2(A, -B);
            double r0 = 0, sweep = 0;
            const bool escapes = b > SHADOW_B * 1.0005;
            if (escapes) {
                r0 = 2 * b / std::sqrt(3.0) * std::cos(std::acos(-SHADOW_B / b) / 3);
                sweep = PI_V + deflection(b);
            }
            for (int k = 0; k < 2; k++) {
                const double psi = psi0 + k * PI_V;
                double r, tip = 1;
                if (escapes) {
                    if (psi >= sweep) break;
                    r = r0 / std::max(1e-5, std::sin(psi * PI_V / sweep));
                } else {
                    if (k > 0 || psi >= PI_V / 2) break;
                    r = b / std::max(1e-5, std::sin(psi));
                    if (r < 2.2) break;
                    tip = smooth01((PI_V / 2 - psi) / 0.3);  // a ponta da faixa some antes de chegar na borda da sombra
                }
                if (r < DISC_IN * 0.92 || r > DISC_OUT) continue;
                const double sinS = std::sin(psi), cosS = std::cos(psi);
                const double dX = sinS * cosP, dY = sinS * sinP * cosI - cosS * sinI;
                const double th = std::atan2(dY, dX);
                const double beta = std::sqrt(1 / r) * 0.78;
                const double g = std::sqrt(std::max(0.05, 1 - 3 / r)) / (1 + beta * sinI * std::cos(th));
                const double inner = smooth01((r - DISC_IN * 0.92) / (DISC_IN * 0.22));
                const double far = 1 - smooth01((r - DISC_OUT * 0.35) / (DISC_OUT * 0.65));
                const double I = tip * inner * far * std::pow(DISC_IN / r, 1.35) * g * g * 4.2 * (k ? 0.72 : 1.0);
                if (I < 0.004) continue;
                double col[3];
                gasColor(std::pow(DISC_IN / r, 0.8) * g * 1.25, col);
                const double tone = 1 - std::exp(-I);
                GasPx px;
                px.pos = uint32_t(y) * uint32_t(size) + uint32_t(x);
                px.th = uint16_t(int((th + PI_V) / (2 * PI_V) * COLS) & (COLS - 1));
                px.row = uint8_t(std::clamp(int((std::log(std::max(r, DISC_IN)) - logIn) / logSpan * (ROWS - 1)), 0, ROWS - 1));
                px.r = uint16_t(col[0] * tone * 16); px.g = uint16_t(col[1] * tone * 16); px.b = uint16_t(col[2] * tone * 16);
                sh->gas.push_back(px);
            }
        }
    }
    sh->gas.shrink_to_fit();
    sh->img = QImage(size, size, QImage::Format_ARGB32_Premultiplied);
    return sh;
}

const QImage& frameOf(Sheet& sh, double simT)
{
    if (sh.shownT == simT) return sh.img;
    sh.shownT = simT;
    std::memcpy(sh.img.bits(), sh.base.constBits(), size_t(sh.base.sizeInBytes()));
    const uint8_t* tex = gasTexture();
    const auto offsetOf = [](double speed, double t) { const double turn = t * speed / (2 * PI_V); return int(std::floor((turn - std::floor(turn)) * COLS)); };
    const int fast = offsetOf(0.62, simT), slow = offsetOf(0.17, simT) + COLS / 3;
    static const std::array<uint32_t, ROWS> weight = [] {
        std::array<uint32_t, ROWS> w {};
        for (int r = 0; r < ROWS; r++) w[size_t(r)] = uint32_t((1 - smooth01(double(r) / (ROWS - 1) * 1.15)) * 256);
        return w;
    }();
    auto* out = reinterpret_cast<uint32_t*>(sh.img.bits());
    for (const GasPx& g : sh.gas) {
        const uint8_t* line = tex + size_t(g.row) * COLS;
        const uint32_t w = weight[g.row];
        const uint32_t n = (line[(g.th - fast) & (COLS - 1)] * w + line[(g.th - slow) & (COLS - 1)] * (256 - w)) >> 8;
        const uint32_t v = out[g.pos];
        const uint32_t R = std::min<uint32_t>(255, ((v >> 16) & 255) + ((g.r * n) >> 11));
        const uint32_t G = std::min<uint32_t>(255, ((v >> 8) & 255) + ((g.g * n) >> 11));
        const uint32_t B = std::min<uint32_t>(255, (v & 255) + ((g.b * n) >> 11));
        const uint32_t Al = std::max({ v >> 24, R, G, B });
        out[g.pos] = (Al << 24) | (R << 16) | (G << 8) | B;
    }
    return sh.img;
}

// A folha certa, se já existe. Se não existe, ela nasce em segundo plano e, enquanto isso, a tela usa a mais parecida:
// de perto, montar uma folha leva mais que um quadro, e inclinar o mapa não pode engasgar.
// Sem widget pra avisar (exportar a imagem, o retrato do painel), monta na hora.
Sheet& sheetFor(int size, int inc, QWidget* wake)
{
    static std::vector<std::unique_ptr<Sheet>> cache;
    static uint64_t tick = 0;
    static std::mutex readyLock;
    static std::vector<std::unique_ptr<Sheet>> ready;
    static std::atomic<bool> working { false };
    tick++;
    const auto adopt = [&](std::unique_ptr<Sheet> sh) -> Sheet& {
        if (cache.size() >= 6) cache.erase(std::min_element(cache.begin(), cache.end(), [](const auto& l, const auto& r) { return l->used < r->used; }));
        sh->used = tick;
        cache.push_back(std::move(sh));
        return *cache.back();
    };
    {
        const std::lock_guard<std::mutex> lock(readyLock);
        for (auto& sh : ready) adopt(std::move(sh));
        ready.clear();
    }
    for (auto& sh : cache) if (sh->size == size && sh->inc == inc) { sh->used = tick; return *sh; }
    if (!wake) return adopt(buildSheet(size, inc));
    if (!working.exchange(true)) {
        QThreadPool::globalInstance()->start([size, inc, w = QPointer<QWidget>(wake)]() {
            auto sh = buildSheet(size, inc);
            {
                const std::lock_guard<std::mutex> lock(readyLock);
                ready.push_back(std::move(sh));
            }
            working = false;
            if (QCoreApplication* app = QCoreApplication::instance()) QMetaObject::invokeMethod(app, [w]() { if (w) w->update(); }, Qt::QueuedConnection);
        });
    }
    Sheet* best = nullptr;
    int bestScore = 0;
    for (auto& sh : cache) {
        const int score = std::abs(sh->inc - inc) + (sh->size == size ? 0 : sh->size > size ? 10 : 20);
        if (!best || score < bestScore) { best = sh.get(); bestScore = score; }
    }
    if (best) { best->used = tick; return *best; }
    return adopt(buildSheet(96, inc));
}

} // namespace

void blackHoleAxis(const System& s, double& a, double& b)
{
    if (s.isCore) { a = 78 * PI_V / 180; b = 98 * PI_V / 180; return; }
    // Cada um deitado do seu jeito, sempre o mesmo pra aquele sistema.
    const double h = std::fabs(std::sin(s.x * 12.9898 + s.y * 78.233) * 43758.5453);
    const double f = h - std::floor(h);
    a = (64 + 20 * f) * PI_V / 180;
    b = f * 977 * PI_V / 180;
}

void paintBlackHole(QPainter& p, QPointF at, double shadowPx, double a, double b, double tilt, double yaw, double simT, QWidget* wake)
{
    // O eixo do disco visto pela câmera do mapa: dá o ângulo de visão e o giro na tela.
    const double wx = std::sin(a) * std::cos(b), wy = std::sin(a) * std::sin(b), wz = std::cos(a);
    const double cy = std::cos(yaw), sy = std::sin(yaw), cT = std::cos(tilt), sT = std::sin(tilt);
    const double nx = wx * cy - wy * sy, ny0 = wx * sy + wy * cy;
    double sx = nx, sUp = ny0 * cT + wz * sT, sZ = -ny0 * sT + wz * cT;
    if (sZ < 0) { sx = -sx; sUp = -sUp; sZ = -sZ; }
    const int inc = std::clamp(int(std::acos(std::min(1.0, sZ)) * 180 / PI_V / 89.5 * INC_STEPS), 0, INC_STEPS - 1);
    const double roll = std::atan2(sx, sUp) * 180 / PI_V;

    const double half = BLACK_HOLE_EXTENT * shadowPx;
    int size = 640;
    for (const int s : { 96, 160, 256, 384, 512 }) if (s >= half * 2) { size = s; break; }
    const QImage& img = frameOf(sheetFor(size, inc, wake), simT);

    p.save();
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setPen(Qt::NoPen);
    p.translate(at);
    p.rotate(roll);
    p.drawImage(QRectF(-half, -half, half * 2, half * 2), img);
    p.restore();
}

} // namespace galaxy
