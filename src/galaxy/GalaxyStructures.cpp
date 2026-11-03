#include "GalaxyStructures.h"
#include "GalaxyModels.h"

#include <QCoreApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRadialGradient>

#include <algorithm>
#include <unordered_map>

namespace galaxy {

namespace {
class SRng {
public:
    explicit SRng(uint32_t seed) : a(seed) {}
    double operator()()
    {
        a = a + 0x6d2b79f5u;
        uint32_t t = a;
        t = (t ^ (t >> 15)) * (t | 1u);
        t ^= t + (t ^ (t >> 7)) * (t | 61u);
        return double(t ^ (t >> 14)) / 4294967296.0;
    }
private:
    uint32_t a;
};
inline QColor rgba(double r, double g, double b, double a = 1)
{
    const auto c = [](double v) { return int(v < 0 ? 0 : v > 255 ? 255 : std::floor(v + 0.5)); };
    return QColor(c(r), c(g), c(b), int(clamp(a, 0, 1) * 255 + 0.5));
}
QRadialGradient gradLamp(double x, double y, double s, int r, int g, int b, double a)
{
    QRadialGradient q(x, y, s);
    q.setColorAt(0, rgba(255, 255, 255, a));
    q.setColorAt(0.3, rgba(r, g, b, a * 0.8));
    q.setColorAt(1, rgba(r, g, b, 0));
    return q;
}
// o ctx.ellipse do canvas (ângulos no sentido da tela) num arco do Qt
void arcTo(QPainter& p, double cx, double cy, double rx, double ry, double a0, double a1)
{
    const double D = 180.0 / PI_D;
    p.drawArc(QRectF(cx - rx, cy - ry, rx * 2, ry * 2), int(std::floor(-a0 * D * 16 + 0.5)), int(std::floor(-(a1 - a0) * D * 16 + 0.5)));
}
} // namespace

const std::map<std::string, StructDef>& structs()
{
    static const std::map<std::string, StructDef> t = [] {
        std::map<std::string, StructDef> m;
        const auto add = [&](const char* id, const char* name, const char* pre, const char* desc, double size, Host host, double spin, bool deep = false, bool sizes = true) { StructDef d {}; d.name = name; d.pre = pre; d.desc = desc; d.size = size; d.host = host; d.spin = spin; d.deep = deep; d.sizes = sizes; m[id] = d; };
        const auto mega = [&](const char* id, const char* name, const char* pre, const char* desc, Host host, bool one, bool orbit = false) { StructDef d {}; d.name = name; d.pre = pre; d.desc = desc; d.host = host; d.mega = true; d.one = one; d.orbit = orbit; m[id] = d; };
        add("sat", QT_TRANSLATE_NOOP("galaxy::Catalog", "Satélite"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Satélite"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Sonda ou satélite pequeno. Comunicação, mapeamento, espionagem."), 0.0085, Host::Any, 0.02, false, false);
        add("farol", QT_TRANSLATE_NOOP("galaxy::Catalog", "Farol de navegação"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Farol"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Sinaliza a rota pras naves que chegam. Pisca, e dá pra ver de longe."), 0.012, Host::Any, 0, true, false);
        add("posto", QT_TRANSLATE_NOOP("galaxy::Catalog", "Posto orbital"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Posto"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Estação pequena: uma tripulação, algumas docas, nada de luxo."), 0.014, Host::Any, 0.015);
        add("mina", QT_TRANSLATE_NOOP("galaxy::Catalog", "Mina de asteroide"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Mina"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Um asteroide inteiro virado em canteiro de extração."), 0.019, Host::Any, 0.008);
        add("telescopio", QT_TRANSLATE_NOOP("galaxy::Catalog", "Observatório"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Observatório"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Um prato enorme apontado pro resto da galáxia."), 0.019, Host::Any, 0.004);
        add("forte", QT_TRANSLATE_NOOP("galaxy::Catalog", "Plataforma de defesa"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Plataforma"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Casco blindado e seis torres de canhão."), 0.02, Host::Any, 0.01);
        add("roda", QT_TRANSLATE_NOOP("galaxy::Catalog", "Estação-roda"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Estação"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Gira pra criar gravidade. O clássico: comércio, trânsito, gente de todo canto."), 0.024, Host::Any, 0.09);
        add("base", QT_TRANSLATE_NOOP("galaxy::Catalog", "Base modular"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Base"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Espinha de treliça com módulos e asas solares. Cresce conforme a necessidade."), 0.028, Host::Any, 0.01, true);
        add("frota", QT_TRANSLATE_NOOP("galaxy::Catalog", "Frota ancorada"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Frota"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Naves em formação, esperando ordens."), 0.026, Host::Any, 0.004, true);
        add("estaleiro", QT_TRANSLATE_NOOP("galaxy::Catalog", "Estaleiro"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Estaleiro"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Berço de construção, com uma nave pela metade dentro."), 0.03, Host::Any, 0.006);
        add("cilindro", QT_TRANSLATE_NOOP("galaxy::Catalog", "Colônia cilíndrica"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Colônia"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Um cilindro que gira, com terra, mar e cidades do lado de dentro."), 0.028, Host::Any, 0.006);
        add("cidade", QT_TRANSLATE_NOOP("galaxy::Catalog", "Cidade orbital"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Cidade"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Milhões de pessoas num disco, sem nunca pisar num planeta."), 0.034, Host::Any, 0.018);
        add("portal", QT_TRANSLATE_NOOP("galaxy::Catalog", "Portal de salto"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Portal"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Anel que abre caminho até outro sistema. Com o Tráfego ligado, as naves saltam por ele."), 0.034, Host::Star, 0.008, true);
        add("naveMundo", QT_TRANSLATE_NOOP("galaxy::Catalog", "Nave-mundo"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Nave-mundo"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Uma cidade inteira que nunca chega: gerações nascem e morrem a bordo."), 0.05, Host::Star, 0, true);
        add("halo", QT_TRANSLATE_NOOP("galaxy::Catalog", "Anel-habitat"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Anel"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Um anel com terra e mar do lado de dentro, do tamanho de uma lua."), 0.06, Host::Star, 0.012);
        mega("anelOrbital", QT_TRANSLATE_NOOP("galaxy::Catalog", "Anel orbital"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Anel orbital"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Um anel construído em volta de um planeta inteiro, com estações presas nele."), Host::Planet, true);
        mega("elevador", QT_TRANSLATE_NOOP("galaxy::Catalog", "Elevador espacial"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Elevador"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Um cabo do chão até o contrapeso, em órbita."), Host::Planet, false);
        mega("mundoAnel", QT_TRANSLATE_NOOP("galaxy::Catalog", "Mundo-anel"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Mundo-anel"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Um anel em volta da estrela, com mais terra que mil planetas."), Host::Star, true, true);
        mega("dyson", QT_TRANSLATE_NOOP("galaxy::Catalog", "Esfera de Dyson"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Esfera"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Placas fechando a estrela, em obra há séculos. O que passa de luz é o que sobrou."), Host::Star, true);
        mega("enxame", QT_TRANSLATE_NOOP("galaxy::Catalog", "Enxame de Dyson"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Enxame"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Milhares de espelhos em órbita, colhendo a luz da estrela."), Host::Star, true);
        mega("sifao", QT_TRANSLATE_NOOP("galaxy::Catalog", "Sifão estelar"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Sifão"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Três coletores puxando plasma direto da estrela."), Host::Star, true);
        return m;
    }();
    return t;
}
const StructDef* structOf(const std::string& id) { const auto& s = structs(); auto it = s.find(id); return it == s.end() ? nullptr : &it->second; }

const BodyDef* bodyDef(const std::string& id)
{
    static const std::map<std::string, BodyDef> t {
        { "temperado", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Temperado"), nullptr } }, { "oceano", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Oceânico"), nullptr } }, { "deserto", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Desértico"), nullptr } }, { "arido", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Rochoso árido"), nullptr } }, { "gelado", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Gelado"), nullptr } }, { "lava", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Lava"), nullptr } }, { "toxico", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Estufa tóxica"), nullptr } }, { "anao", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Planeta anão"), nullptr } },
        { "gigante", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Gigante gasoso"), nullptr } }, { "quente", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Júpiter quente"), nullptr } }, { "gelo", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Gigante de gelo"), nullptr } }, { "subnetuno", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Subnetuno"), nullptr } },
        { "fortaleza", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Lua de guerra"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Uma estação do tamanho de uma lua, com um canhão num prato gigante. Um planeta que alguém construiu pra destruir outros.") } },
        { "ecumeno", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Planeta-cidade"), QT_TRANSLATE_NOOP("galaxy::Catalog", "A cidade cobriu tudo. De noite, o planeta inteiro acende.") } },
        { "casco", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Planeta-casco"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Um planeta artificial, placa por placa, com energia correndo nas costuras.") } },
        { "forja", { QT_TRANSLATE_NOOP("galaxy::Catalog", "Mundo-forja"), QT_TRANSLATE_NOOP("galaxy::Catalog", "Um planeta transformado em fábrica. Metal fundido nas rachaduras.") } },
    };
    auto it = t.find(id);
    return it == t.end() ? nullptr : &it->second;
}

const std::vector<CatalogCat>& catalog()
{
    static const std::vector<CatalogCat> c = [] {
        std::vector<CatalogCat> out;
        CatalogCat a { "planetas", QT_TRANSLATE_NOOP("galaxy::Catalog", "Astros"), "planet", {} };
        a.items.push_back({ "estrela", ItemKind::Star, QT_TRANSLATE_NOOP("galaxy::Catalog", "Nova estrela"), false, true });
        for (const char* id : { "temperado", "oceano", "deserto", "arido", "gelado", "lava", "toxico", "anao", "gigante", "quente", "gelo", "subnetuno" }) a.items.push_back({ id, ItemKind::Planet, bodyDef(id)->name });
        out.push_back(a);
        CatalogCat b { "pequenos", QT_TRANSLATE_NOOP("galaxy::Catalog", "Luas e cometas"), "moon", {} };
        b.items.push_back({ "rochosa", ItemKind::Moon, QT_TRANSLATE_NOOP("galaxy::Catalog", "Lua rochosa") }); b.items.push_back({ "gelada", ItemKind::Moon, QT_TRANSLATE_NOOP("galaxy::Catalog", "Lua gelada") }); b.items.push_back({ "vulcanica", ItemKind::Moon, QT_TRANSLATE_NOOP("galaxy::Catalog", "Lua vulcânica") }); b.items.push_back({ "temperada", ItemKind::Moon, QT_TRANSLATE_NOOP("galaxy::Catalog", "Lua habitável") });
        b.items.push_back({ "cometa", ItemKind::Comet, QT_TRANSLATE_NOOP("galaxy::Catalog", "Cometa") }); b.items.push_back({ "asteroides", ItemKind::Belt, QT_TRANSLATE_NOOP("galaxy::Catalog", "Cinturão") }); b.items.push_back({ "kuiper", ItemKind::Belt, QT_TRANSLATE_NOOP("galaxy::Catalog", "Disco gelado") });
        out.push_back(b);
        CatalogCat s { "estacoes", QT_TRANSLATE_NOOP("galaxy::Catalog", "Estações"), "station", {} };
        for (const char* id : { "sat", "farol", "posto", "mina", "telescopio", "forte", "roda", "base", "frota", "estaleiro", "cilindro", "cidade" }) s.items.push_back({ id, ItemKind::Struct, structOf(id)->name });
        out.push_back(s);
        CatalogCat w { "mundos", QT_TRANSLATE_NOOP("galaxy::Catalog", "Mundos"), "hull", {} };
        for (const char* id : { "fortaleza", "ecumeno", "casco", "forja" }) w.items.push_back({ id, ItemKind::Planet, bodyDef(id)->name, true });
        out.push_back(w);
        CatalogCat g { "mega", QT_TRANSLATE_NOOP("galaxy::Catalog", "Megaestruturas"), "ring", {} };
        for (const char* id : { "portal", "naveMundo", "halo", "anelOrbital", "elevador", "mundoAnel", "dyson", "enxame", "sifao" }) g.items.push_back({ id, ItemKind::Struct, structOf(id)->name });
        out.push_back(g);
        return out;
    }();
    return c;
#undef T
}

void drawAnimated(QPainter& p, const std::string& type, uint32_t seed, double x, double y, double r, double rot, double pitch, double t, double alpha, double flash)
{
    if (r < 3) return;
    const double P = clampPitch(pitch), rad = radOf(type, seed);
    const auto at = [&](double px, double py, double pz, double& ox, double& oy) { double a, b, c; localToScreen(px, py, pz, rot, P, rad, a, b, c); ox = a * r; oy = b * r; };
    p.save();
    p.translate(x, y);
    p.setCompositionMode(QPainter::CompositionMode_Plus);
    p.setOpacity(alpha);
    p.setPen(Qt::NoPen);
    if (type == "portal") {
        const double rIn = (r * 0.69) / rad, pw = 0.55 + 0.1 * std::sin(t * 0.8) + flash * 0.45, sq = std::max(0.03, std::fabs(std::cos(P)));
        p.scale(1, sq);
        QRadialGradient q(0, 0, rIn);
        q.setColorAt(0, rgba(190, 240, 255, 0.5 * pw + flash * 0.5)); q.setColorAt(0.55, rgba(70, 160, 255, 0.22 * pw)); q.setColorAt(0.9, rgba(70, 190, 255, 0.4 * pw)); q.setColorAt(1, rgba(90, 200, 255, 0));
        p.setBrush(q);
        p.drawEllipse(QPointF(0, 0), rIn, rIn);
        if (r > 14) {
            QPen pen(rgba(190, 235, 255, 0.3 + flash * 0.5)); pen.setWidthF(std::max(0.6, r * 0.012));
            p.setPen(pen); p.setBrush(Qt::NoBrush);
            for (int i = 0; i < 4; i++) {
                const double a = rot + t * 0.22 * (i % 2 ? -1 : 1) + i * 1.57;
                QPolygonF poly;
                for (int u = 0; u <= 22; u++) { const double k = u / 22.0, q2 = rIn * (0.94 - k * 0.92), th = a + k * 3.2; poly << QPointF(std::cos(th) * q2, std::sin(th) * q2); }
                p.drawPolyline(poly);
            }
        }
    } else if (type == "farol") {
        const double b = std::pow(0.5 + 0.5 * std::sin(t * 1.6), 6), s = r * (0.25 + b * 0.8);
        double px, py; at(0, 0, 0.7, px, py);
        p.setBrush(gradLamp(px, py, s, 255, 120, 90, 0.25 + b * 0.75));
        p.drawEllipse(QPointF(px, py), s, s);
    } else if (type == "estaleiro") {
        for (int i = 0; i < 5; i++) {
            const double b = std::pow(0.5 + 0.5 * std::sin(t * (2.1 + i * 0.63) + i * 2.4), 14);
            if (b < 0.02) continue;
            double px, py; at(-0.26 + i * 0.13, i % 2 ? -0.19 : 0.19, i % 3 ? 0.1 : -0.1, px, py);
            const double s = r * 0.09;
            p.setBrush(gradLamp(px, py, s, 180, 225, 255, b));
            p.drawEllipse(QPointF(px, py), s, s);
        }
    } else if (type == "naveMundo") {
        static const double E[3][2] = { { 0.11, 0 }, { -0.055, 0.095 }, { -0.055, -0.095 } };
        for (const auto& e : E) {
            double px, py, ex, ey; at(-1, e[0], e[1], px, py); at(-1.42, e[0], e[1], ex, ey);
            const double a = 0.7 + 0.15 * std::sin(t * 2 + e[0] * 9);
            QLinearGradient q(px, py, ex, ey); q.setColorAt(0, rgba(140, 215, 255, a)); q.setColorAt(1, rgba(90, 150, 255, 0));
            QPen pen(QBrush(q), std::max(1.0, r * 0.075)); pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen); p.drawLine(QPointF(px, py), QPointF(ex, ey));
        }
    } else if (type == "mina") {
        const double b = std::pow(0.5 + 0.5 * std::sin(t * 3.1), 10);
        if (b > 0.02) { double px, py; at(0.26, 0.04, 0.62, px, py); const double s = r * 0.26; p.setBrush(gradLamp(px, py, s, 255, 170, 90, b * 0.7)); p.drawEllipse(QPointF(px, py), s, s); }
    }
    p.restore();
}

namespace {
struct Panel { double x, y, z, k; };
const std::vector<Panel>& dysonPanels(uint32_t seed)
{
    static std::unordered_map<uint32_t, std::vector<Panel>> cache;
    auto it = cache.find(seed);
    if (it != cache.end()) return it->second;
    SRng r(seed);
    const int n = 210;
    std::vector<Panel> out;
    // A obra cresce em faixas: uns cinturões já fechados, o resto em pedaços.
    for (int i = 0; i < n; i++) {
        const double z = 1 - (2 * (i + 0.5)) / n, q = std::sqrt(1 - z * z), a = i * 2.399963;
        const double band = std::fabs(z) < 0.3 ? 0.9 : std::fabs(z) < 0.62 ? 0.5 : 0.22;
        if (r() < band) { const double k = 0.7 + r() * 0.3; out.push_back({ q * std::cos(a), z, q * std::sin(a), k }); }
    }
    return cache.emplace(seed, std::move(out)).first->second;
}
} // namespace

void drawDyson(QPainter& p, double x, double y, double R, double cT, uint32_t seed, double t, char part, double alpha)
{
    const auto& list = dysonPanels(seed);
    const double ca = std::cos(t * 0.02), sa = std::sin(t * 0.02), S = R * 0.145, sT = std::sqrt(std::max(0.0, 1 - cT * cT));
    p.save();
    p.setOpacity(alpha);
    for (const Panel& q : list) {
        const double X = q.x * ca + q.z * sa, Z0 = -q.x * sa + q.z * ca, Y = q.y * cT - Z0 * sT, Z = q.y * sT + Z0 * cT;
        const bool front = Z >= 0;
        if ((part == 'f') != front) continue;
        const double f = std::max(0.12, std::fabs(Z)), sx = x + X * R, sy = y - Y * R, rad = S * q.k;
        // Hexágono achatado na direção em que a placa olha.
        const double ang = std::atan2(Y, X);
        p.save();
        p.translate(sx, sy);
        p.rotate(-ang * 180 / PI_D);
        p.scale(std::max(0.2, f), 1);
        QPolygonF hex;
        for (int i = 0; i < 6; i++) { const double a = (i / 6.0) * TAU + 0.26; hex << QPointF(std::cos(a) * rad, std::sin(a) * rad); }
        if (front) {
            const double v = 60 + 90 * Z * Z;
            QPen pen(rgba(190, 220, 255, 0.45)); pen.setWidthF(std::max(0.5, rad * 0.05));
            p.setPen(pen); p.setBrush(rgba(v, v * 1.04, v * 1.16, 0.94));
        } else {
            // Por dentro, a placa recebe a estrela em cheio.
            p.setPen(Qt::NoPen); p.setBrush(rgba(255, 205 + 30 * q.k, 150 + 40 * q.k, 0.2 + 0.32 * f));
        }
        p.drawPolygon(hex);
        p.restore();
    }
    p.restore();
}
void drawSwarm(QPainter& p, double x, double y, double R, double cT, uint32_t seed, double t, double alpha)
{
    SRng r(seed);
    p.save();
    p.setOpacity(alpha);
    p.setCompositionMode(QPainter::CompositionMode_Plus);
    const double sT = std::sqrt(std::max(0.0, 1 - cT * cT));
    for (int ringI = 0; ringI < 6; ringI++) {
        const double inc = -1.1 + (ringI * 2.2) / 5 + (r() - 0.5) * 0.2, node = r() * TAU, rad = R * (0.72 + r() * 0.34);
        const int n = 170 + int(std::floor(r() * 80));
        const double sp = 0.012 / (0.6 + ringI * 0.15), ci = std::cos(inc), si = std::sin(inc), cn = std::cos(node), sn = std::sin(node);
        for (int i = 0; i < n; i++) {
            const double a = (double(i) / n) * TAU + t * sp + r() * 0.04, c = std::cos(a), s = std::sin(a);
            const double ox = c * cn - s * ci * sn, oy = c * sn + s * ci * cn, oz = s * si;
            const double flick = 0.4 + 0.6 * std::pow(0.5 + 0.5 * std::sin(t * 0.5 + i * 2.7 + ringI), 4);
            const double sz = std::max(1.3, R * 0.012);
            p.fillRect(QRectF(x + ox * rad - sz / 2, y - (oy * cT + oz * sT) * rad - sz / 2, sz, sz), rgba(255, 225 - oz * 30, 170 - oz * 60, flick * (oz < 0 ? 0.55 : 0.95)));
        }
    }
    p.restore();
}
namespace {
struct BandSeg { int i; double Y; QPolygonF q; };
// Uma faixa em volta do eixo: cada trecho é um quadrilátero 3D, e só a metade pedida é desenhada.
std::vector<BandSeg> bandSegments(double R, double H, double P, double a0, int n, char part)
{
    const double cp = std::cos(P), sp = std::sin(P);
    const bool back = part == 'b';
    std::vector<BandSeg> out;
    for (int i = 0; i < n; i++) {
        const double a = a0 + (double(i) / n) * TAU, b = a0 + ((i + 1.04) / n) * TAU, mid = (a + b) / 2, Y = std::sin(mid);
        if ((Y >= 0) != back) continue;
        const auto pt = [&](double q, double z) { const double Yq = std::sin(q) * R; return QPointF(std::cos(q) * R, -(Yq * cp + z * sp)); };
        QPolygonF poly; poly << pt(a, H) << pt(b, H) << pt(b, -H) << pt(a, -H);
        out.push_back({ i, Y, poly });
    }
    std::sort(out.begin(), out.end(), [&](const BandSeg& u, const BandSeg& v) { return back ? v.Y < u.Y : u.Y < v.Y; });
    return out;
}
inline void fillQuad(QPainter& p, const QPolygonF& q, double x, double y, const QColor& c)
{
    QPen pen(c); pen.setWidthF(0.7);
    p.setPen(pen); p.setBrush(c);
    p.drawPolygon(q.translated(x, y));
}
} // namespace
// Mundo-anel: terra por dentro, casco por fora, muralhas nas bordas. P é a inclinação com que a gente vê o anel.
void drawRingworld(QPainter& p, double x, double y, double R, double H, double P, uint32_t seed, double t, char part, double alpha)
{
    SRng r(seed);
    const int n = 160;
    const double a0 = t * 0.004;
    std::vector<std::array<int, 3>> col;
    double q = r();
    int left = 0;
    for (int i = 0; i < n; i++) {
        if (left-- <= 0) { q = r(); left = 2 + int(std::floor(r() * 9)); }
        col.push_back(q < 0.46 ? std::array<int, 3> { 62, 134, 74 } : q < 0.72 ? std::array<int, 3> { 46, 110, 170 } : q < 0.9 ? std::array<int, 3> { 160, 136, 90 } : std::array<int, 3> { 232, 238, 242 });
    }
    p.save();
    p.setOpacity(alpha);
    const double cp = std::cos(P), sp = std::sin(P);
    for (const BandSeg& s : bandSegments(R, H, P, a0, n, part)) {
        const double mid = a0 + (double(s.i) / n) * TAU;
        if (part == 'b') {
            // Quadrados de sombra fazem a noite em quem está no anel.
            const double night = std::pow(0.5 + 0.5 * std::cos(mid * 8 - a0 * 12), 2), k = 0.98 - night * 0.82;
            const auto& c = col[size_t(s.i)];
            fillQuad(p, s.q, x, y, rgba(c[0] * k, c[1] * k, c[2] * k));
        } else {
            const double v = 44 + 18 * (s.i % 4 == 0 ? 1 : 0) + 22 * std::max(0.0, -std::cos(mid) * 0.7);
            fillQuad(p, s.q, x, y, rgba(v, v + 3, v + 9));
        }
    }
    // As muralhas de cima e de baixo, na metade que está sendo desenhada.
    const double a1 = part == 'b' ? 0 : PI_D, a2 = part == 'b' ? PI_D : TAU;
    p.setBrush(Qt::NoBrush);
    for (double z : { H, -H }) {
        QPen pen(z > 0 ? QColor(0xd4, 0xdb, 0xe5) : QColor(0x8a, 0x94, 0xa2)); pen.setWidthF(std::max(1.0, H * 0.1));
        p.setPen(pen);
        arcTo(p, x, y - z * sp, R, R * cp, -a2, -a1);
    }
    if (part == 'f') {
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        const QColor c = rgba(255, 214, 150, 0.75);
        const double w = std::max(1.0, H * 0.05);
        for (int i = 0; i < 120; i++) {
            const double a = a0 + (i / 120.0) * TAU + 0.013;
            if (std::sin(a) >= 0) continue;
            const double z = H * (((i * 37) % 11) / 5.5 - 1) * 0.72;
            p.fillRect(QRectF(x + std::cos(a) * R - w / 2, y - (std::sin(a) * R * cp + z * sp) - w / 2, w, w), c);
        }
    }
    p.restore();
}
// Anel orbital: uma estrutura de verdade em volta do planeta, com seção retangular (convés, parede de dentro e de fora),
// trilho no meio do convés, trens andando, cabos descendo até o chão e estações presas nele.
// lightAng: de onde vem a estrela, no plano do anel. A parte que cai na sombra do planeta apaga e acende as janelas.
void drawOrbitalRing(QPainter& p, double x, double y, double R, double P, uint32_t seed, double t, char part, double alpha, double dpr, double pr, double lightAng)
{
    const double RING_ROLL_DEG = -0.32 * 180 / PI_D;
    const double W = R * 0.075, H = R * 0.03, cp = std::cos(P), sp = std::sin(P), spin = t * 0.012;
    const double Lx = std::cos(lightAng), Ly = std::sin(lightAng);
    p.save();
    p.setOpacity(alpha);
    p.translate(x, y);
    if (part == 's') {
        // A sombra fina do anel no planeta, do lado contrário ao da estrela.
        QPainterPath clip; clip.addEllipse(QPointF(0, 0), pr, pr);
        p.setClipPath(clip, Qt::IntersectClip);
        p.rotate(RING_ROLL_DEG);
        p.translate(-Lx * pr * 0.1, Ly * pr * 0.1 * cp);
        p.setBrush(Qt::NoBrush);
        for (const auto& la : { std::array<double, 2> { W * 1.15, 0.07 }, std::array<double, 2> { W * 0.55, 0.13 } }) { QPen pen(rgba(0, 2, 6, la[1])); pen.setWidthF(la[0]); p.setPen(pen); p.drawEllipse(QPointF(0, 0), R * 0.97, R * 0.97 * cp); }
        p.restore();
        return;
    }
    p.rotate(RING_ROLL_DEG);
    const bool back = part == 'b';
    const auto pt = [&](double a, double r, double z) { return QPointF(std::cos(a) * r, -(std::sin(a) * r * cp + z * sp)); };
    const auto inShadow = [&](double a) { const double X = std::cos(a) * R, Y = std::sin(a) * R; return X * Lx + Y * Ly >= 0 ? 0.0 : 1 - smooth(pr * 0.86, pr * 1.06, std::fabs(X * Ly - Y * Lx)); };
    const auto tone = [](double v, double k) { const int q = int(std::floor(v * 255 + 0.5)), b2 = int(std::floor(std::min(255.0, v * 255 * (1.05 + k)) + 0.5)); return QColor(std::min(255, q), std::min(255, int(std::floor(q * 1.02 + 0.5))), b2); };
    const int n = std::max(72, std::min(260, int(std::floor(R * 0.8 + 0.5))));
    struct Seg { int i; double mid, Y; };
    std::vector<Seg> segs;
    for (int i = 0; i < n; i++) { const double mid = spin + ((i + 0.5) / n) * TAU, Y = std::sin(mid); if ((Y >= 0) == back) segs.push_back({ i, mid, Y }); }
    std::sort(segs.begin(), segs.end(), [&](const Seg& u, const Seg& v) { return back ? v.Y < u.Y : u.Y < v.Y; });
    const auto quad = [&](double a0, double a1, double r0, double z0, double r1, double z1, const QColor& c) { QPolygonF q; q << pt(a0, r0, z0) << pt(a1, r0, z0) << pt(a1, r1, z1) << pt(a0, r1, z1); QPen pen(c); pen.setWidthF(0.6); p.setPen(pen); p.setBrush(c); p.drawPolygon(q); };
    const double rI = R - W / 2, rO = R + W / 2, Hh = H / 2;
    const bool big = R > 70;
    for (const Seg& s : segs) {
        const double a0 = spin + (double(s.i) / n) * TAU, a1 = spin + ((s.i + 1.02) / n) * TAU, sh = inShadow(s.mid), lit = 1 - sh * 0.92;
        // Painéis: uma junta escura de vez em quando, e um casco que não é igual de ponta a ponta.
        const bool joint = s.i % 12 == 0;
        const double wob = (double((uint32_t(s.i + 13) * 2654435761u) >> 24) / 255 - 0.5) * 0.05;
        const double wallK = back ? std::max(0.0, -(std::cos(s.mid) * Lx + std::sin(s.mid) * Ly)) : std::max(0.0, std::cos(s.mid) * Lx + std::sin(s.mid) * Ly);
        const double wall = (0.07 + 0.44 * wallK * lit) * (joint ? 0.72 : 1) + wob * lit, deck = (0.12 + 0.46 * lit) * (joint ? 0.88 : 1) + wob * lit;
        if (back) quad(a0, a1, rI, -Hh, rI, Hh, tone(std::max(0.03, wall), 0.08));
        else quad(a0, a1, rO, -Hh, rO, Hh, tone(std::max(0.03, wall), 0.08));
        // O convés em três faixas: as bordas claras e o trilho escuro no meio.
        const double q = R * 0.0085;
        quad(a0, a1, rI, Hh, R - q, Hh, tone(std::max(0.03, deck), 0.04));
        quad(a0, a1, R - q, Hh, R + q, Hh, tone(std::max(0.02, deck * 0.56), 0.14));
        quad(a0, a1, R + q, Hh, rO, Hh, tone(std::max(0.03, deck * 0.86), 0.04));
        // Módulos no convés, de tamanhos diferentes, nos dois lados do trilho.
        const uint32_t hs = uint32_t(s.i + 7) * 2654435761u;
        if (big && (hs & 7) == 0) {
            const bool out = hs & 8;
            const double len = ((a1 - a0) * (1 + ((hs >> 8) & 3))) * 0.75, r0 = out ? R + q * 1.8 : rI + W * 0.07, r1 = out ? rO - W * 0.07 : R - q * 1.8;
            quad(a0, a0 + len, r0, Hh, r1, Hh, tone(std::min(0.92, deck * (((hs >> 12) & 1) ? 1.3 : 0.68)), 0.05));
        }
    }
    // Quinas claras, que desenham o volume.
    const double arc0 = back ? -PI_D : 0, arc1 = back ? 0 : PI_D;
    p.setBrush(Qt::NoBrush);
    {
        QPen a(rgba(226, 234, 244, 0.6)); a.setWidthF(std::max(0.5, H * 0.9 * 0.1)); a.setCapStyle(Qt::FlatCap); p.setPen(a); arcTo(p, 0, -Hh * sp, rO, rO * cp, arc0, arc1);
        QPen b(rgba(226, 234, 244, 0.42)); b.setWidthF(std::max(0.5, H * 0.8 * 0.1)); b.setCapStyle(Qt::FlatCap); p.setPen(b); arcTo(p, 0, -Hh * sp, rI, rI * cp, arc0, arc1);
    }
    // Janelas: pontinhos finos na parede, que só aparecem no escuro; trens correndo no trilho.
    p.setCompositionMode(QPainter::CompositionMode_Plus);
    if (big) {
        const int m = std::min(560, int(std::floor(R * 1.5 + 0.5)));
        const double sz = std::max(0.8, H * 0.09), rw = back ? rI : rO;
        for (int i = 0; i < m; i++) {
            const uint32_t h = (uint32_t(i) + seed) * 2654435761u;
            const double a = spin + (double(i) / m) * TAU + ((h >> 8) & 255) / 255.0 * 0.012, Y = std::sin(a);
            if ((Y >= 0) != back || (h & 7) > 3) continue;
            const double dark = inShadow(a), glow = 0.14 + 0.82 * dark;
            const double z = (((h >> 16) & 255) / 255.0 - 0.5) * H * 0.72;
            const QPointF q = pt(a, rw, z);
            p.fillRect(QRectF(q.x() - sz / 2, q.y() - sz / 2, sz, sz), ((h >> 3) & 1) ? rgba(255, 214, 150, glow) : rgba(190, 226, 255, glow * 0.8));
        }
        const int m2 = std::min(900, int(std::floor(R * 2.4 + 0.5)));
        const double sz2 = std::max(0.7, W * 0.035);
        for (int i = 0; i < m2; i++) {
            const uint32_t h = (uint32_t(i) * 3u + seed + 11u) * 2654435761u;
            const double a = spin + (double(i) / m2) * TAU, Y = std::sin(a);
            if ((Y >= 0) != back || (h & 3) == 0) continue;
            const double dark = inShadow(a);
            if (dark < 0.04) continue;
            const double off = W * ((((h >> 5) & 15) / 15.0) * 0.32 + 0.05);
            const QPointF q = pt(a, ((h >> 4) & 1) ? rO - off : rI + off, Hh);
            p.fillRect(QRectF(q.x() - sz2 / 2, q.y() - sz2 / 2, sz2, sz2), rgba(255, 212, 150, dark * 0.85));
        }
    }
    // Trens: riscos curtos azulados no trilho.
    for (int i = 0; i < 5; i++) {
        const double a = spin * (i % 2 ? -5 : 5) + (i / 5.0) * TAU + seed, len = 0.016, Y = std::sin(a);
        if ((Y >= 0) != back) continue;
        QPen pen(rgba(190, 228, 255, 0.8)); pen.setWidthF(std::max(0.7, R * 0.0055));
        p.setPen(pen);
        QPolygonF poly;
        for (int u = 0; u <= 4; u++) poly << pt(a + (u / 4.0) * len, R, Hh);
        p.drawPolyline(poly);
    }
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    // Quatro estações, cada uma com o cabo descendo até o chão do planeta.
    for (int i = 0; i < 4; i++) {
        const double a = spin + (i / 4.0) * TAU + (seed % 100) * 0.0628, Y = std::sin(a);
        if ((Y >= 0) != back) continue;
        const QPointF hub = pt(a, R - W * 0.6, 0), foot = pt(a, pr * 0.99, 0);
        const double dark = inShadow(a);
        QPen pen(rgba(190, 204, 222, 0.8)); pen.setWidthF(std::max(0.6, R * 0.004));
        p.setPen(pen);
        for (double off : { -0.012, 0.012 }) p.drawLine(pt(a + off, pr * 0.99, 0), pt(a + off * 0.5, R - W * 0.6, 0));
        const double k = 0.5 + 0.5 * std::sin(t * 0.13 + i * 1.9), cx = foot.x() + (hub.x() - foot.x()) * k, cy = foot.y() + (hub.y() - foot.y()) * k, cs = std::max(1.0, R * 0.008);
        p.fillRect(QRectF(cx - cs, cy - cs, cs * 2, cs * 2), rgba(255, 226, 170, 0.95));
        const double s = std::max(3.0, W * 1.25), vx = Lx, vy = -Ly * cp;
        double vn = std::hypot(vx, vy);
        if (vn == 0) vn = 1;
        drawModel(p, "anelNo", 5, std::cos(a) * R, pt(a, R, Hh * 0.4).y(), s, a, P, vx / vn * (1 - dark * 0.5), vy / vn * (1 - dark * 0.5), alpha, dpr, QStringLiteral("ring:%1:%2").arg(seed).arg(i));
    }
    p.restore();
}
// Elevador: o cabo sai do chão, sobe até o contrapeso, e um carro anda nele.
void drawElevator(QPainter& p, double x, double y, double pr, double ang, double H, double cT, double t, double alpha, double dpr, const QString& id)
{
    const double ca = std::cos(ang), sa = std::sin(ang), x0 = x + ca * pr * 0.97, y0 = y + sa * pr * 0.97, x1 = x + ca * (pr + H), y1 = y + sa * (pr * 0.97 + H * cT), W = std::max(0.8, H * 0.012);
    p.save();
    p.setOpacity(alpha);
    QPen pen(rgba(205, 215, 228, 0.85)); pen.setWidthF(W);
    p.setPen(pen);
    p.drawLine(QPointF(x0, y0), QPointF(x1, y1));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x9a, 0xa4, 0xb2));
    const double fr = std::max(1.5, pr * 0.05);
    p.drawEllipse(QPointF(x0, y0), fr, fr);
    const double s = std::max(2.5, H * 0.09);
    drawModel(p, "contrapeso", 5, x1, y1, s, ang, std::acos(std::max(0.05, cT)) + 0.5, -ca, -sa, alpha, dpr, QStringLiteral("elev:%1").arg(id));
    const double k = 0.5 + 0.5 * std::sin(t * 0.09), xc = x0 + (x1 - x0) * k, yc = y0 + (y1 - y0) * k;
    p.setCompositionMode(QPainter::CompositionMode_Plus);
    p.setBrush(gradLamp(xc, yc, s * 0.5, 255, 214, 150, 1));
    p.drawEllipse(QPointF(xc, yc), s * 0.5, s * 0.5);
    p.restore();
}
// Sifão estelar: três coletores 3D puxando plasma da estrela em fios.
void drawSiphon(QPainter& p, double x, double y, double R, double cT, uint32_t seed, double t, const std::array<int, 3>& starC, double alpha, double dpr, double yaw)
{
    p.save();
    p.setOpacity(alpha);
    const double base = (seed % 628) / 100.0 + t * 0.003, P = std::acos(std::max(0.05, cT)) + 0.6;
    for (int i = 0; i < 3; i++) {
        const double a = base + (i / 3.0) * TAU, ca = std::cos(a), sa = std::sin(a), ex = x + ca * R * 0.86, ey = y + sa * R * 0.86 * cT, sx = x + ca * R * 0.16, sy = y + sa * R * 0.16 * cT;
        const double nx = -sa * R * 0.16, ny = ca * R * 0.16 * cT, mx = (sx + ex) / 2 + nx, my = (sy + ey) / 2 + ny;
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        QLinearGradient q(sx, sy, ex, ey);
        q.setColorAt(0, rgba(starC[0], starC[1], starC[2], 0.85)); q.setColorAt(1, rgba(starC[0], starC[1], starC[2], 0.2));
        p.setBrush(Qt::NoBrush);
        for (int f = 0; f < 3; f++) {
            QPen pen(QBrush(q), std::max(0.7, R * (0.018 - f * 0.005))); pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            QPainterPath path(QPointF(sx, sy));
            path.quadTo(mx + nx * f * 0.14, my + ny * f * 0.14, ex, ey);
            p.drawPath(path);
        }
        for (int k = 0; k < 4; k++) {
            const double u = std::fmod(t * 0.035 + k / 4.0 + i * 0.13, 1.0), v = 1 - u, px = v * v * sx + 2 * v * u * mx + u * u * ex, py = v * v * sy + 2 * v * u * my + u * u * ey, s = std::max(1.0, R * 0.012);
            p.fillRect(QRectF(px - s / 2, py - s / 2, s, s), rgba(255, 245, 225, 0.95));
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        const double s = R * 0.17;
        if (s > 2) drawModel(p, "coletor", 5, x + ca * R, y + sa * R * cT, s, -a + yaw, P, -ca, -sa, alpha, dpr, QStringLiteral("sifao:%1:%2").arg(seed).arg(i));
    }
    p.restore();
}

} // namespace galaxy
