#include "CartoGl.h"

#include "../CrashLogger.h"

#include <QCoreApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QSettings>
#include <QSurfaceFormat>

#include <algorithm>
#include <cmath>
#include <thread>

namespace carto {

namespace {

constexpr const char* kKeyOn = "carto/opengl";
constexpr const char* kKeySession = "carto/glSession";   // OpenGL em uso nesta sessão (some na saída normal)
constexpr const char* kKeyAutoOff = "carto/glAutoOff";   // desligado sozinho porque a sessão anterior caiu

bool g_loaded = false, g_on = false, g_tried = false;
GlSpin* g_inst = nullptr;
QString g_reason;

// a mesma conta de CartoPaint (styleLift): quanto o relevo sobe em cada vista
double liftOfStyle(Style s)
{
    switch (s) {
    case Style::Atlas: return kAtlasLift;
    case Style::Topo: return 0.6;
    case Style::Chart: case Style::Blue: return 0.75;
    default: return 1.0;
    }
}

// média móvel separável (bordas repetidas), raio r
void boxBlur(std::vector<float>& a, int W, int H, int r)
{
    if (r <= 0 || W <= 0 || H <= 0) return;
    std::vector<float> tmp(a.size());
    for (int y = 0; y < H; y++) {
        const float* src = a.data() + static_cast<size_t>(y) * W;
        float* dst = tmp.data() + static_cast<size_t>(y) * W;
        double acc = 0;
        for (int k = -r; k <= r; k++) acc += src[std::clamp(k, 0, W - 1)];
        for (int x = 0; x < W; x++) {
            dst[x] = static_cast<float>(acc / (2 * r + 1));
            acc += src[std::min(W - 1, x + r + 1)] - src[std::max(0, x - r)];
        }
    }
    for (int x = 0; x < W; x++) {
        double acc = 0;
        for (int k = -r; k <= r; k++) acc += tmp[static_cast<size_t>(std::clamp(k, 0, H - 1)) * W + x];
        for (int y = 0; y < H; y++) {
            a[static_cast<size_t>(y) * W + x] = static_cast<float>(acc / (2 * r + 1));
            acc += tmp[static_cast<size_t>(std::min(H - 1, y + r + 1)) * W + x] - tmp[static_cast<size_t>(std::max(0, y - r)) * W + x];
        }
    }
}

// erguida (com água em 0) em (x, y) do mundo, bilinear
inline double sampleLift(const GlTerrain& t, double x, double y)
{
    x = std::clamp(x, 0.0, t.W - 1.001); y = std::clamp(y, 0.0, t.H - 1.001);
    const int ix = static_cast<int>(x), iy = static_cast<int>(y);
    const double fx = x - ix, fy = y - iy;
    const size_t j = static_cast<size_t>(iy) * t.W + ix;
    auto a = [&](size_t k) { return std::max(0.0f, t.A[k]); };
    return a(j) * (1 - fx) * (1 - fy) + a(j + 1) * fx * (1 - fy) + a(j + t.W) * (1 - fx) * fy + a(j + t.W + 1) * fx * fy;
}

const char* kVert = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform vec3 uTex0;     // px do mundo por px da textura, cu0, cv0
uniform vec4 uView;     // cos e sen do giro, passo da vista, liftK / passo
uniform vec2 uOrigin;   // cu, cv da vista recortada
uniform vec2 uSize;     // tamanho do pedaço
uniform vec2 uDepth;    // linha de chão mais longe e mais perto
uniform vec2 uTexSize;
out vec2 vUv;
void main() {
    vec2 d = (aPos.xy - uTex0.yz) * uTex0.x;
    float px = uView.x * d.x - uView.y * d.y;
    float py = uView.y * d.x + uView.x * d.y;
    float u = px / uView.z + uOrigin.x;
    float v = py / uView.z + uOrigin.y;
    float r = v - max(aPos.z, 0.0) * uView.w;
    float z = clamp(1.0 - 2.0 * (v - uDepth.x) / (uDepth.y - uDepth.x), -1.0, 1.0);
    gl_Position = vec4((u + 0.5) / uSize.x * 2.0 - 1.0, (r + 0.5) / uSize.y * 2.0 - 1.0, z, 1.0);
    vUv = (aPos.xy + 0.5) / uTexSize;
}
)";

const char* kFrag = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uTex;
out vec4 frag;
void main() { frag = texture(uTex, vUv); }
)";

} // namespace

// ── preparo (fora da linha da tela) ──

std::shared_ptr<GlTerrain> prepareGlTerrain(World& w, Style style, bool info, const RealmPaint* realms,
                                            const std::vector<float>& lut, quint64 key, int maxTex,
                                            const std::atomic<bool>* cancel)
{
    if (w.W <= 1 || w.H <= 1 || w.backdrop) return nullptr;
    auto t = std::make_shared<GlTerrain>();
    t->key = key;
    // a pintura vista de cima (inclinação 90: sem relevo erguido), num passo que cabe na textura da placa
    const int lim = std::clamp(maxTex, 1024, 4096);
    View top;
    top.ang = 0; top.pitch = 90; top.style = style; top.info = info; top.step = 1;
    const Geom probe = viewGeom(w, top);
    top.step = std::max(1.0, std::ceil(std::max(probe.W, probe.H) / double(lim)));
    Render r = render(w, top, realms, true, 0, cancel);
    if (cancel && cancel->load()) return nullptr;
    const Geom& g = r.g;
    if (g.W <= 1 || g.H <= 1 || r.data.size() < static_cast<size_t>(g.W) * g.H * 4) return nullptr;
    std::vector<uint8_t> lit;
    const uint8_t* src = r.data.data();
    if (!lut.empty() && !r.aux.nrm.empty()) {
        lit.assign(r.data.size(), 0);
        relight(r.data.data(), r.aux.nrm.data(), r.aux.depth.data(), lit.data(), g.W, 0, 0, g.W, g.H, lut);
        src = lit.data();
    }
    t->tex = QImage(src, g.W, g.H, g.W * 4, QImage::Format_RGBA8888).copy();
    t->s0 = g.step; t->cu0 = g.cu; t->cv0 = g.cv; t->ccx = g.ccx; t->ccy = g.ccy;

    // erguida por px do mundo: a mesma conta do pintor, com a altura suavizada em ~3 px
    const int WW = w.W, WH = w.H;
    t->W = WW; t->H = WH;
    std::vector<float> hb = w.h;
    boxBlur(hb, WW, WH, 3);
    const double ref = w.ref > 0 ? w.ref : 1.0;
    const double extF0 = WH * 0.062 / 0.65 * 1.05 * liftOfStyle(style);
    t->A.resize(static_cast<size_t>(WW) * WH);
    for (size_t i = 0; i < t->A.size(); i++) {
        if (w.h[i] <= 0) { t->A[i] = -1.0f; continue; }
        const double ta = std::max(0.0, (hb[i] / ref - 0.12) / 0.88);
        t->A[i] = ta > 0 ? static_cast<float>(std::pow(ta, 1.3) * extF0) : 0.0f;
    }
    if (cancel && cancel->load()) return nullptr;

    // a malha: um vértice a cada `k` px da textura (até ~1024 por lado)
    const int k = std::max(1, static_cast<int>(std::ceil(std::max(g.W, g.H) / 1024.0)));
    const int gw = (g.W - 1 + k - 1) / k + 1, gh = (g.H - 1 + k - 1) / k + 1;
    t->verts.reserve(static_cast<size_t>(gw) * gh * 3);
    for (int j = 0; j < gh; j++) {
        const int v0 = std::min(j * k, g.H - 1);
        for (int i = 0; i < gw; i++) {
            const int u0 = std::min(i * k, g.W - 1);
            const double x = g.ccx + (u0 - g.cu) * g.step, y = g.ccy + (v0 - g.cv) * g.step;
            const bool inside = x >= 0 && y >= 0 && x <= WW - 1 && y <= WH - 1;
            t->verts.push_back(static_cast<float>(u0));
            t->verts.push_back(static_cast<float>(v0));
            t->verts.push_back(inside ? static_cast<float>(sampleLift(*t, x, y)) : 0.0f);
        }
    }
    t->idx.reserve(static_cast<size_t>(gw - 1) * (gh - 1) * 6);
    for (int j = 0; j + 1 < gh; j++)
        for (int i = 0; i + 1 < gw; i++) {
            const quint32 a = static_cast<quint32>(j * gw + i), b = a + 1, c = a + gw, d = c + 1;
            t->idx.insert(t->idx.end(), { a, c, b, b, c, d });
        }
    return t;
}

// ── o chão da vista, sem cor ──

void fastAux(const GlTerrain& t, const Geom& g, double liftK, Aux& aux)
{
    const int W = g.W, H = g.H;
    aux.depth.assign(static_cast<size_t>(W) * H, -1);
    aux.lift.assign(static_cast<size_t>(W) * H, 0.0f);
    aux.nrm.clear();
    if (W <= 0 || H <= 0 || t.A.empty()) return;
    const double s = g.step, ca = g.ca, sa = g.sa, kL = liftK / s;
    const int ext = static_cast<int>(std::ceil(g.extF * 1.1)) + 2;
    // Em blocos de colunas vizinhas, linha por linha do bloco: as escritas caem nas mesmas linhas de cache
    // (coluna por coluna, cada escrita era uma falta de cache) e as amostras do mundo ficam perto umas das outras.
    constexpr int B = 32;
    const float* A = t.A.data();
    const int TW = t.W, TH = t.H;
    auto cols = [&](int c0, int c1) {
        for (int b0 = c0; b0 < c1; b0 += B) {
            const int n = std::min(B, c1 - b0);
            int minTop[B];     // por coluna: linhas [minTop, H) já têm chão mais perto
            double px[B];
            for (int i = 0; i < n; i++) { minTop[i] = H; px[i] = (b0 + i - g.cu) * s; }
            // de perto pra longe: cada linha da tela recebe só o primeiro chão que a cobre (o mais perto)
            for (int v = H - 1 + ext; v >= 0; v--) {
                const double py = (v - g.cv) * s;
                for (int i = 0; i < n; i++) {
                    const double x = t.ccx + ca * px[i] + sa * py, y = t.ccy - sa * px[i] + ca * py;
                    if (x < 0 || y < 0 || x > TW - 1.001 || y > TH - 1.001) continue;
                    const int ix = static_cast<int>(x), iy = static_cast<int>(y);
                    const double fx = x - ix, fy = y - iy;
                    const size_t j = static_cast<size_t>(iy) * TW + ix;
                    const float a00 = A[j], a10 = A[j + 1], a01 = A[j + TW], a11 = A[j + TW + 1];
                    const float near = fy < 0.5 ? (fx < 0.5 ? a00 : a10) : (fx < 0.5 ? a01 : a11);
                    if (near < 0) continue;   // água
                    const double L = (std::max(0.0f, a00) * (1 - fx) * (1 - fy) + std::max(0.0f, a10) * fx * (1 - fy)
                                      + std::max(0.0f, a01) * (1 - fx) * fy + std::max(0.0f, a11) * fx * fy) * kL;
                    const int u = b0 + i;
                    if (v < H) aux.lift[static_cast<size_t>(v) * W + u] = static_cast<float>(L);
                    const int top = static_cast<int>(std::floor(v - L));
                    const int lo = std::max(top, 0), hi = std::min({ v, minTop[i] - 1, H - 1 });
                    if (lo > hi) continue;
                    for (int r = lo; r <= hi; r++) aux.depth[static_cast<size_t>(r) * W + u] = static_cast<int16_t>(v);
                    minTop[i] = lo;
                }
            }
        }
    };
    const int n = std::clamp(static_cast<int>(std::thread::hardware_concurrency()), 1, 16);
    if (n <= 1 || W < 64) { cols(0, W); return; }
    std::vector<std::thread> th;
    for (int i = 0; i < n; i++) th.emplace_back(cols, W * i / n, W * (i + 1) / n);
    for (auto& x : th) x.join();
}

// ── a opção e as travas ──

bool GlSpin::enabled()
{
    if (!g_loaded) { g_on = QSettings().value(QLatin1String(kKeyOn), false).toBool(); g_loaded = true; }
    return g_on;
}

void GlSpin::setEnabled(bool on)
{
    g_on = on; g_loaded = true;
    QSettings st;
    st.setValue(QLatin1String(kKeyOn), on);
    if (on) st.remove(QLatin1String(kKeyAutoOff));
}

void GlSpin::startupGuard()
{
    QSettings st;
    if (st.value(QLatin1String(kKeySession), false).toBool()) {
        st.remove(QLatin1String(kKeySession));
        if (st.value(QLatin1String(kKeyOn), false).toBool()) {
            st.setValue(QLatin1String(kKeyOn), false);
            st.setValue(QLatin1String(kKeyAutoOff), true);
            CrashLogger::log("OpenGL do Mapa desligado: a sessão anterior usou o OpenGL e não fechou direito");
        }
    }
    g_loaded = false;
    // saída normal: a sessão não conta como queda
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, []() { QSettings().remove(QLatin1String(kKeySession)); });
}

QString GlSpin::unavailableReason() { return g_reason; }

GlSpin* GlSpin::get()
{
    if (!enabled()) return nullptr;
    if (g_inst) return g_inst->m_broken ? nullptr : g_inst;
    if (g_tried) return nullptr;
    g_tried = true;
    auto* gl = new GlSpin();
    if (!gl->init()) { delete gl; return nullptr; }
    g_inst = gl;
    return g_inst;
}

void GlSpin::fail(const QString& why)
{
    m_broken = true;
    g_reason = why;
    CrashLogger::log(QStringLiteral("OpenGL do Mapa: ") + why + QStringLiteral(" (o giro volta ao processador)"));
}

bool GlSpin::init()
{
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    m_ctx = new QOpenGLContext();
    m_ctx->setFormat(fmt);
    if (!m_ctx->create()) { fail(QStringLiteral("sem contexto OpenGL")); return false; }
    const QSurfaceFormat got = m_ctx->format();
    if (m_ctx->isOpenGLES() || got.majorVersion() * 10 + got.minorVersion() < 33) {
        fail(QStringLiteral("a placa não tem OpenGL 3.3 (tem %1.%2)").arg(got.majorVersion()).arg(got.minorVersion()));
        return false;
    }
    m_surface = new QOffscreenSurface();
    m_surface->setFormat(got);
    m_surface->create();
    if (!m_surface->isValid() || !m_ctx->makeCurrent(m_surface)) { fail(QStringLiteral("não deu para usar a superfície fora da tela")); return false; }
    // daqui pra frente o OpenGL está em uso: se o app cair, a próxima abertura vem sem ele
    QSettings().setValue(QLatin1String(kKeySession), true);
    QOpenGLExtraFunctions* f = m_ctx->extraFunctions();
    auto compile = [&](GLenum type, const char* code) -> GLuint {
        const GLuint sh = f->glCreateShader(type);
        f->glShaderSource(sh, 1, &code, nullptr);
        f->glCompileShader(sh);
        GLint ok = 0;
        f->glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512] = {};
            f->glGetShaderInfoLog(sh, sizeof(log) - 1, nullptr, log);
            fail(QStringLiteral("shader não compilou: ") + QString::fromLatin1(log));
            return 0;
        }
        return sh;
    };
    const GLuint vs = compile(GL_VERTEX_SHADER, kVert), fs = compile(GL_FRAGMENT_SHADER, kFrag);
    if (!vs || !fs) { m_ctx->doneCurrent(); return false; }
    m_prog = f->glCreateProgram();
    f->glAttachShader(m_prog, vs);
    f->glAttachShader(m_prog, fs);
    f->glLinkProgram(m_prog);
    GLint linked = 0;
    f->glGetProgramiv(m_prog, GL_LINK_STATUS, &linked);
    f->glDeleteShader(vs);
    f->glDeleteShader(fs);
    if (!linked) { fail(QStringLiteral("shader não ligou")); m_ctx->doneCurrent(); return false; }
    GLint maxTex = 4096;
    f->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
    m_maxTex = std::max(1024, static_cast<int>(maxTex));
    GLuint ids[3] = {};
    f->glGenVertexArrays(1, ids);
    m_vao = ids[0];
    f->glGenBuffers(2, ids);
    m_vbo = ids[0]; m_ebo = ids[1];
    f->glGenTextures(1, ids);
    m_tex = ids[0];
    const bool ok = f->glGetError() == GL_NO_ERROR;
    m_ctx->doneCurrent();
    if (!ok) { fail(QStringLiteral("erro ao preparar a placa")); return false; }
    return true;
}

bool GlSpin::upload(const GlTerrain& t)
{
    if (m_broken || !m_ctx || t.tex.isNull() || t.idx.empty()) return false;
    if (t.tex.width() > m_maxTex || t.tex.height() > m_maxTex) return false;
    if (!m_ctx->makeCurrent(m_surface)) { fail(QStringLiteral("perdeu o contexto")); return false; }
    QOpenGLExtraFunctions* f = m_ctx->extraFunctions();
    f->glBindVertexArray(m_vao);
    f->glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    f->glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(t.verts.size() * sizeof(float)), t.verts.data(), GL_STATIC_DRAW);
    f->glEnableVertexAttribArray(0);
    f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    f->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    f->glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(t.idx.size() * sizeof(quint32)), t.idx.data(), GL_STATIC_DRAW);
    f->glBindVertexArray(0);
    f->glBindTexture(GL_TEXTURE_2D, m_tex);
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, t.tex.width(), t.tex.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, t.tex.constBits());
    f->glGenerateMipmap(GL_TEXTURE_2D);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    f->glBindTexture(GL_TEXTURE_2D, 0);
    const bool ok = f->glGetError() == GL_NO_ERROR;
    m_ctx->doneCurrent();
    if (!ok) { fail(QStringLiteral("erro ao mandar o mapa para a placa")); return false; }
    m_count = static_cast<qint64>(t.idx.size());
    m_texW = t.tex.width(); m_texH = t.tex.height();
    m_key = t.key;
    return true;
}

bool GlSpin::ensureFbo(int w, int h)
{
    if (m_fbo && w <= m_fboW && h <= m_fboH) return true;
    QOpenGLExtraFunctions* f = m_ctx->extraFunctions();
    const int W = (w + 255) / 256 * 256, H = (h + 255) / 256 * 256;
    if (m_fbo) {
        GLuint fb = m_fbo, rb[2] = { m_color, m_depth };
        f->glDeleteFramebuffers(1, &fb);
        f->glDeleteRenderbuffers(2, rb);
    }
    GLuint fb = 0, rb[2] = {};
    f->glGenFramebuffers(1, &fb);
    f->glGenRenderbuffers(2, rb);
    f->glBindRenderbuffer(GL_RENDERBUFFER, rb[0]);
    f->glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, W, H);
    f->glBindRenderbuffer(GL_RENDERBUFFER, rb[1]);
    f->glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, W, H);
    f->glBindFramebuffer(GL_FRAMEBUFFER, fb);
    f->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb[0]);
    f->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb[1]);
    const bool ok = f->glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    m_fbo = fb; m_color = rb[0]; m_depth = rb[1]; m_fboW = W; m_fboH = H;
    return ok;
}

bool GlSpin::render(const Geom& g, double liftK, double s0, double cu0, double cv0, double ccx, double ccy, QImage& out)
{
    Q_UNUSED(ccx); Q_UNUSED(ccy);
    if (m_broken || !m_ctx || !m_count || g.W <= 0 || g.H <= 0) return false;
    if (g.W > std::min(m_maxTex, 8192) || g.H > std::min(m_maxTex, 8192)) return false;
    if (!m_ctx->makeCurrent(m_surface)) { fail(QStringLiteral("perdeu o contexto")); return false; }
    QOpenGLExtraFunctions* f = m_ctx->extraFunctions();
    if (!ensureFbo(g.W, g.H)) { m_ctx->doneCurrent(); fail(QStringLiteral("framebuffer incompleto")); return false; }
    f->glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    f->glViewport(0, 0, g.W, g.H);
    f->glDisable(GL_BLEND);
    f->glDisable(GL_CULL_FACE);
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_LESS);
    f->glClearColor(0, 0, 0, 0);
    f->glClearDepthf(1.0f);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    f->glUseProgram(m_prog);
    auto loc = [&](const char* n) { return f->glGetUniformLocation(m_prog, n); };
    const double ext = g.extF * 1.2 + 8;
    f->glUniform3f(loc("uTex0"), float(s0), float(cu0), float(cv0));
    f->glUniform4f(loc("uView"), float(g.ca), float(g.sa), float(g.step), float(liftK / g.step));
    f->glUniform2f(loc("uOrigin"), float(g.cu), float(g.cv));
    f->glUniform2f(loc("uSize"), float(g.W), float(g.H));
    f->glUniform2f(loc("uDepth"), float(-ext), float(g.H + ext));
    f->glUniform2f(loc("uTexSize"), float(m_texW), float(m_texH));
    f->glUniform1i(loc("uTex"), 0);
    f->glActiveTexture(GL_TEXTURE0);
    f->glBindTexture(GL_TEXTURE_2D, m_tex);
    f->glBindVertexArray(m_vao);
    f->glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_count), GL_UNSIGNED_INT, nullptr);
    f->glBindVertexArray(0);
    // a linha 0 do framebuffer é a de cima da vista (o shader já desenha de ponta-cabeça pro OpenGL)
    out = QImage(g.W, g.H, QImage::Format_RGBA8888);
    f->glPixelStorei(GL_PACK_ALIGNMENT, 4);
    f->glReadPixels(0, 0, g.W, g.H, GL_RGBA, GL_UNSIGNED_BYTE, out.bits());
    f->glBindFramebuffer(GL_FRAMEBUFFER, 0);
    const bool ok = f->glGetError() == GL_NO_ERROR;
    m_ctx->doneCurrent();
    if (!ok) { fail(QStringLiteral("erro ao desenhar")); return false; }
    return true;
}

} // namespace carto
