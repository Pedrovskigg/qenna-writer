// Catálogo do Construir (o modo construção do The Sims): a faixa de baixo, as
// cartinhas com miniatura de verdade e a pílula em cima do selecionado.

#include "CartoBuild.h"

#include "CartoCanvas.h"
#include "CartoIcons.h"
#include "CartoPanel.h"
#include "CartoSprites.h"
#include "../Theme.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QHelpEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace carto {

namespace {
constexpr int kCardW = 86, kCardH = 104, kThumbW = 76, kThumbH = 62;

// rótulo curto da cartinha (o nome inteiro fica na dica, no título e no selecionado)
QString shortLabel(const QString& k)
{
    if (k == QLatin1String("mage")) return Panel::tr("Mago");
    if (k == QLatin1String("greattree")) return Panel::tr("Árvore");
    if (k == QLatin1String("stones")) return Panel::tr("Pedras");
    if (k == QLatin1String("floatisland")) return Panel::tr("Ilha");
    if (k == QLatin1String("sword")) return Panel::tr("Espada");
    if (k == QLatin1String("heads")) return Panel::tr("Cabeças");
    if (k == QLatin1String("seaserpent")) return Panel::tr("Serpente");
    if (k == QLatin1String("camp")) return Panel::tr("Tendas");
    if (k == QLatin1String("seatedgod")) return Panel::tr("Deus");
    if (k == QLatin1String("arch")) return Panel::tr("Arco");
    return buildKindName(k);
}

QString tipOf(const QString& k)
{
    static const QHash<QString, const char*> tips = {
        { "castle", QT_TRANSLATE_NOOP("carto::Panel", "Castelo: clique pra colocar") }, { "tower", QT_TRANSLATE_NOOP("carto::Panel", "Torre de vigia") },
        { "lighthouse", QT_TRANSLATE_NOOP("carto::Panel", "Farol (na costa fica bonito)") }, { "windmill", QT_TRANSLATE_NOOP("carto::Panel", "Moinho") },
        { "temple", QT_TRANSLATE_NOOP("carto::Panel", "Templo") }, { "ruins", QT_TRANSLATE_NOOP("carto::Panel", "Ruínas") },
        { "bridge", QT_TRANSLATE_NOOP("carto::Panel", "Ponte: clique numa margem e depois na outra") },
        { "village", QT_TRANSLATE_NOOP("carto::Panel", "Pincel de vila: casas brotam em terra firme") }, { "fields", QT_TRANSLATE_NOOP("carto::Panel", "Pincel de plantação") },
        { "wall", QT_TRANSLATE_NOOP("carto::Panel", "Muralha: desenhe o traço; ela sobe e desce com o chão") },
        { "road", QT_TRANSLATE_NOOP("carto::Panel", "Estrada: ligue dois pontos (ela acha o caminho pelo vale) ou desenhe à mão") },
        { "erase", QT_TRANSLATE_NOOP("carto::Panel", "Apaga o que você construiu") },
        { "statue", QT_TRANSLATE_NOOP("carto::Panel", "Estátua de bronze sobre pedestal") }, { "ship", QT_TRANSLATE_NOOP("carto::Panel", "Navio de três mastros: clique no mar") },
        { "colosseum", QT_TRANSLATE_NOOP("carto::Panel", "Coliseu de pedra com arena") }, { "cave", QT_TRANSLATE_NOOP("carto::Panel", "Entrada de caverna numa rocha") },
        { "mine", QT_TRANSLATE_NOOP("carto::Panel", "Mina escorada, com trilhos e vagonete") }, { "mage", QT_TRANSLATE_NOOP("carto::Panel", "Torre do mago com cristal flutuante") },
        { "greattree", QT_TRANSLATE_NOOP("carto::Panel", "Árvore Sagrada (enorme)") }, { "stones", QT_TRANSLATE_NOOP("carto::Panel", "Círculo de pedras") },
        { "camp", QT_TRANSLATE_NOOP("carto::Panel", "Acampamento com tendas e fogueira") },
        { "dragon", QT_TRANSLATE_NOOP("carto::Panel", "Dragão empoleirado numa rocha") }, { "bones", QT_TRANSLATE_NOOP("carto::Panel", "Ossada de dragão no chão") },
        { "portal", QT_TRANSLATE_NOOP("carto::Panel", "Portal de pedra com luz arcana") }, { "floatisland", QT_TRANSLATE_NOOP("carto::Panel", "Ilha flutuante com torre e cascata") },
        { "sword", QT_TRANSLATE_NOOP("carto::Panel", "Espada cravada na pedra") }, { "heads", QT_TRANSLATE_NOOP("carto::Panel", "Cabeças de pedra colossais") },
        { "longship", QT_TRANSLATE_NOOP("carto::Panel", "Drakkar viking: clique no mar") }, { "wreck", QT_TRANSLATE_NOOP("carto::Panel", "Naufrágio afundando: clique no mar") },
        { "seaserpent", QT_TRANSLATE_NOOP("carto::Panel", "Serpente marinha: clique no mar") }, { "kraken", QT_TRANSLATE_NOOP("carto::Panel", "Kraken: clique no mar") },
        { "pyramid", QT_TRANSLATE_NOOP("carto::Panel", "Pirâmide com o topo de ouro") }, { "ziggurat", QT_TRANSLATE_NOOP("carto::Panel", "Templo em degraus com escadaria") },
        { "pagoda", QT_TRANSLATE_NOOP("carto::Panel", "Pagode de cinco andares") }, { "obelisk", QT_TRANSLATE_NOOP("carto::Panel", "Obelisco de pedra com ponta de ouro") },
        { "graveyard", QT_TRANSLATE_NOOP("carto::Panel", "Cemitério com mausoléu") }, { "farm", QT_TRANSLATE_NOOP("carto::Panel", "Fazenda: celeiro, silo e plantação") },
        { "colossus", QT_TRANSLATE_NOOP("carto::Panel", "Colosso de bronze com a tocha erguida") }, { "guardians", QT_TRANSLATE_NOOP("carto::Panel", "Guardiões: dois reis de pedra com a mão erguida") },
        { "seatedgod", QT_TRANSLATE_NOOP("carto::Panel", "Deus Sentado no trono, com braseiros") }, { "cathedral", QT_TRANSLATE_NOOP("carto::Panel", "Catedral com rosácea, torres e arcobotantes") },
        { "arch", QT_TRANSLATE_NOOP("carto::Panel", "Arco do Triunfo") }, { "market", QT_TRANSLATE_NOOP("carto::Panel", "Mercado: barracas listradas e chafariz") },
        { "aqueduct", QT_TRANSLATE_NOOP("carto::Panel", "Trecho de aqueduto com duas fileiras de arcos") },
        { "city", QT_TRANSLATE_NOOP("carto::Panel", "Pincel de cidade: sobrados de dois e três andares") },
        { "forest", QT_TRANSLATE_NOOP("carto::Panel", "Pincel de floresta") }, { "rocks", QT_TRANSLATE_NOOP("carto::Panel", "Pincel de rochedos") },
    };
    const char* t = tips.value(k, nullptr);
    return t ? Panel::tr(t) : buildKindName(k);
}

// os povos do castelo, na ordem da categoria Castelos (cada um vira uma cartinha: castle:<estilo>)
struct StyleSw { const char* v; const char* label; const char* tip; QRgb col; };
const StyleSw kStyles[12] = {
    { "human", QT_TRANSLATE_NOOP("carto::Panel", "Humano"), QT_TRANSLATE_NOOP("carto::Panel", "Muralha de pedra, torres de flecha e a menagem"), 0xffa39a8c },
    { "norman", QT_TRANSLATE_NOOP("carto::Panel", "Normando"), QT_TRANSLATE_NOOP("carto::Panel", "Torres quadradas e a menagem maciça com torreões"), 0xffb8ad98 },
    { "moat", QT_TRANSLATE_NOOP("carto::Panel", "Com fosso"), QT_TRANSLATE_NOOP("carto::Panel", "Cava o fosso no terreno em volta; ponte levadiça"), 0xff7aa0b4 },
    { "chateau", QT_TRANSLATE_NOOP("carto::Panel", "Château"), QT_TRANSLATE_NOOP("carto::Panel", "Palácio branco, telhado de ardósia, torres de cone e jardim"), 0xffe8e2d4 },
    { "crag", QT_TRANSLATE_NOOP("carto::Panel", "Penhasco"), QT_TRANSLATE_NOOP("carto::Panel", "Assenta no relevo: muralha e torres acompanham o morro"), 0xff8a8070 },
    { "elven", QT_TRANSLATE_NOOP("carto::Panel", "Élfico"), QT_TRANSLATE_NOOP("carto::Panel", "Muralha clara, torres finas e uma torre altíssima no meio"), 0xffdfe8e2 },
    { "dwarf", QT_TRANSLATE_NOOP("carto::Panel", "Anão"), QT_TRANSLATE_NOOP("carto::Panel", "Fortaleza escavada no monte, com portão dourado e forjas"), 0xffc9963c },
    { "orc", QT_TRANSLATE_NOOP("carto::Panel", "Orc"), QT_TRANSLATE_NOOP("carto::Panel", "Paliçada de toras, torres de vigia e o salão comprido"), 0xff7a5434 },
    { "oriental", QT_TRANSLATE_NOOP("carto::Panel", "Oriental"), QT_TRANSLATE_NOOP("carto::Panel", "Muralha branca, guaritas e a torre de vários telhados"), 0xffeeeae0 },
    { "desert", QT_TRANSLATE_NOOP("carto::Panel", "Deserto"), QT_TRANSLATE_NOOP("carto::Panel", "Adobe, cúpulas, a cebola dourada e os minaretes"), 0xffe2c290 },
    { "dark", QT_TRANSLATE_NOOP("carto::Panel", "Sombrio"), QT_TRANSLATE_NOOP("carto::Panel", "Pedra negra, torres altíssimas e janelas verdes acesas"), 0xff403c46 },
    { "ice", QT_TRANSLATE_NOOP("carto::Panel", "Gelo"), QT_TRANSLATE_NOOP("carto::Panel", "Torres de cristal e um palácio de cristais tortos"), 0xffc0e4f4 },
};
const StyleSw* styleSw(const QString& v) { for (const StyleSw& s : kStyles) if (v == QLatin1String(s.v)) return &s; return nullptr; }
// item do catálogo → o tipo de verdade e o estilo (castle:<estilo>)
QString baseKind(const QString& item) { const int i = item.indexOf(QLatin1Char(':')); return i < 0 ? item : item.left(i); }
QString styleOfItem(const QString& item) { const int i = item.indexOf(QLatin1Char(':')); return i < 0 ? QString() : item.mid(i + 1); }
bool isPaintBrush(const QString& k)
{
    return k == QLatin1String("village") || k == QLatin1String("city") || k == QLatin1String("fields") || k == QLatin1String("forest") || k == QLatin1String("rocks")
        || k == QLatin1String("erase");
}
bool isBrushKind(const QString& k) { return isPaintBrush(k) || k == QLatin1String("wall") || k == QLatin1String("road"); }

// a cartinha: miniatura, nome e (no castelo) as amostrinhas de estilo
class Card : public QAbstractButton {
public:
    QString kind, style;
    QPixmap pix;
    bool castle = false;
    std::function<void(const QString&)> onStyle;
    explicit Card(QWidget* parent) : QAbstractButton(parent)
    {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
        setFixedSize(kCardW, kCardH);
    }
    QSize sizeHint() const override { return QSize(kCardW, kCardH); }

protected:
    QRectF swatch(int i) const { const double w = 4 * 10 + 3 * 6; return QRectF((width() - w) / 2 + i * 16, 88, 10, 10); }
    int swatchAt(QPointF p) const { for (int i = 0; i < 4; i++) if (swatch(i).adjusted(-3, -3, 3, 3).contains(p)) return i; return -1; }
    bool event(QEvent* e) override
    {
        if (e->type() == QEvent::ToolTip && castle) {
            const int i = swatchAt(static_cast<QHelpEvent*>(e)->pos());
            setToolTip(i >= 0 ? Panel::tr(kStyles[i].label) + QStringLiteral(": ") + Panel::tr(kStyles[i].tip) : tipOf(kind));
        }
        return QAbstractButton::event(e);
    }
    void mousePressEvent(QMouseEvent* e) override
    {
        if (castle && e->button() == Qt::LeftButton) {
            const int i = swatchAt(e->position());
            if (i >= 0) { if (onStyle) onStyle(QString::fromLatin1(kStyles[i].v)); e->accept(); return; }
        }
        QAbstractButton::mousePressEvent(e);
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor accent = Theme::toColor(Theme::accentDefault()), muted = Theme::toColor(Theme::textMuted()), ink = Theme::toColor(Theme::textPrimary());
        const QColor panel = Theme::toColor(Theme::panelBackground()), well = Theme::toColor(Theme::hoverOverlay()), line = Theme::toColor(Theme::panelBorder());
        const bool hover = underMouse();
        if (isChecked()) {
            QColor soft = accent; soft.setAlphaF(0.18);
            p.setBrush(soft); p.setPen(QPen(accent, 1));
            p.drawRoundedRect(QRectF(0.5, 0.5, width() - 1, height() - 1), 10, 10);
        }
        const QRectF th((width() - kThumbW) / 2.0, 4, kThumbW, kThumbH);
        p.setBrush(well); p.setPen(hover && !isChecked() ? QPen(line, 1) : Qt::NoPen);
        p.drawRoundedRect(th, 8, 8);
        if (!pix.isNull()) {
            const QSizeF ps = pix.deviceIndependentSize();
            p.drawPixmap(QPointF(th.center().x() - ps.width() / 2, th.center().y() - ps.height() / 2), pix);
        }
        QFont f = font(); f.setPixelSize(11); f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(isChecked() ? accent : hover ? ink : muted);
        const QRectF tr(2, 69, width() - 4, 16);
        p.drawText(tr, Qt::AlignCenter, p.fontMetrics().elidedText(text(), Qt::ElideRight, int(tr.width())));
        if (castle) {
            for (int i = 0; i < 4; i++) {
                const QRectF r = swatch(i);
                if (style == QLatin1String(kStyles[i].v)) {
                    p.setPen(Qt::NoPen); p.setBrush(accent); p.drawEllipse(r.adjusted(-3.5, -3.5, 3.5, 3.5));
                    p.setBrush(panel); p.drawEllipse(r.adjusted(-2, -2, 2, 2));
                }
                p.setBrush(QColor::fromRgb(kStyles[i].col)); p.setPen(QPen(QColor(0, 0, 0, 77), 1));
                p.drawEllipse(r);
            }
        }
    }
};

// ── miniaturas: o mesmo desenho do mapa numa câmera de vitrine (três quartos, 40°) ──
constexpr double kVA = 0.62, kVP = 40 * M_PI / 180;
const RGB kLand{ 124, 146, 88 }, kSea{ 70, 118, 146 }, kFlag{ 196, 72, 58 };

spr::Cam vitrine(double s, double ox, double oy)
{
    spr::Cam c;
    c.ang = kVA; c.co = std::cos(kVP); c.si = std::sin(kVP); c.edges = true; c.detail = true;
    const double ca = std::cos(kVA), sa = std::sin(kVA), co = c.co, si = c.si;
    c.proj = [=](double x, double y, double z) { return QPointF(ox + s * (ca * x - sa * y), oy + s * (si * (sa * x + ca * y) - co * z)); };
    return c;
}
QColor rgbc(const RGB& c, double k = 1) { return QColor(int(c[0] * k), int(c[1] * k), int(c[2] * k)); }
void disc(QPainter& p, const spr::Cam& cam, double r, const RGB& col)
{
    QPolygonF poly;
    for (int i = 0; i < 48; i++) { const double a = i / 48.0 * 2 * M_PI; poly << cam.proj(std::cos(a) * r, std::sin(a) * r, 0); }
    p.setBrush(rgbc(col)); p.setPen(QPen(rgbc(col, 0.7), 1));
    p.drawPolygon(poly);
}
double yawFor(const QString& kind)
{
    // como nasce no mapa: de frente, três quartos ou de lado pra quem olha
    const double toEye = std::atan2(std::cos(kVA), std::sin(kVA));
    if (const ArtSpec* s = artSpec(kind)) {
        if (s->face == 'f') return toEye - 0.1;
        if (s->face == 't') return toEye - 0.8;
        if (s->face == 's') return toEye + M_PI / 2 - 0.65;
    }
    return toEye - 0.15;
}
void paintItem(QPainter& p, const QString& kind, const QString& style, const spr::Cam& cam, double s)
{
    p.setRenderHint(QPainter::Antialiasing);
    if (kind == QLatin1String("village")) {
        disc(p, cam, 9, kLand);
        struct H { double x, y, yaw, v; int floors; };
        std::vector<H> hs = { { -4, -3.5, 0.3, 0.2, 1 }, { 3.5, -3, 1.3, 0.7, 2 }, { -3.5, 3, 0.9, 0.45, 1 }, { 4, 3.5, 2.1, 0.9, 1 } };
        const double ca = std::cos(kVA), sa = std::sin(kVA);
        std::sort(hs.begin(), hs.end(), [&](const H& a, const H& b) { return sa * a.x + ca * a.y < sa * b.x + ca * b.y; });
        for (const H& h : hs) spr::drawHouse(p, h.x, h.y, h.yaw, h.v, h.floors, h.v > 0.6, cam, false);
        return;
    }
    if (kind == QLatin1String("fields")) {
        disc(p, cam, 9, kLand);
        spr::drawField(p, Field{ -2.8, -2.4, 8, 4.6, 0.25, 0 }, cam);
        spr::drawField(p, Field{ 3, 3, 7, 4.2, 0.25, 1 }, cam);
        return;
    }
    if (kind == QLatin1String("city")) {
        disc(p, cam, 9, kLand);
        struct H { double x, y, v; int floors; bool bell; };
        std::vector<H> hs = { { -3.6, -3.2, 0.15, 3, false }, { 0.2, -3.6, 0.55, 2, false }, { 3.8, -3.0, 0.8, 3, false }, { -3.4, 1.4, 0.35, 2, false }, { 0.4, 1.0, 0.7, 3, true }, { 4.0, 1.6, 0.95, 2, false } };
        const double ca = std::cos(kVA), sa = std::sin(kVA);
        std::sort(hs.begin(), hs.end(), [&](const H& a, const H& b) { return sa * a.x + ca * a.y < sa * b.x + ca * b.y; });
        for (const H& h : hs) { House hh; hh.x = h.x; hh.y = h.y; hh.yaw = 0.2; hh.v = h.v; hh.floors = h.floors; hh.city = true; hh.bell = h.bell; spr::drawCityHouse(p, hh, cam, false); }
        return;
    }
    if (kind == QLatin1String("forest") || kind == QLatin1String("rocks")) {
        disc(p, cam, 9, kLand);
        struct T { double x, y; int type; double v, sc; };
        std::vector<T> ts = kind == QLatin1String("forest")
            ? std::vector<T>{ { -5, -3, Pine, 0.2, 1.1 }, { -1.5, -5, Broad, 0.6, 1.2 }, { 2.5, -4, Pine, 0.4, 1 }, { 5.5, -1, Broad, 0.1, 1.1 }, { -4.5, 1.5, Broad, 0.8, 1.15 },
                              { -0.5, 0, Pine, 0.7, 1.25 }, { 3, 2.5, Broad, 0.3, 1.05 }, { -2, 4.5, Pine, 0.5, 1 }, { 1.5, 5.5, Broad, 0.9, 1 } }
            : std::vector<T>{ { -4, -2, Rock, 0.2, 2.4 }, { 1.5, -3.5, Rock, 0.6, 1.8 }, { 4.5, 1, Rock, 0.4, 2.6 }, { -1.5, 2.5, Rock, 0.8, 2.0 }, { -5, 4, Rock, 0.5, 1.4 } };
        const double ca = std::cos(kVA), sa = std::sin(kVA);
        std::sort(ts.begin(), ts.end(), [&](const T& a, const T& b) { return sa * a.x + ca * a.y < sa * b.x + ca * b.y; });
        const double dpr = p.device() ? p.device()->devicePixelRatioF() : 1.0;
        for (const T& t : ts) {
            const double ppw = s * t.sc;
            const spr::FloraImg& img = spr::floraImage(t.type, t.v, 40, ppw * dpr, false);
            const double f = ppw / (2 * img.ppw);
            const QPointF X = cam.proj(t.x, t.y, 0);
            p.drawImage(QRectF(X.x() - img.ax * f, X.y() - img.ay * f, img.img.width() * f, img.img.height() * f), img.img);
        }
        return;
    }
    if (kind == QLatin1String("wall")) {
        disc(p, cam, 9, kLand);
        spr::drawWallSeg(p, Pt{ -7, 3 }, Pt{ 0, -2.5 }, cam, 1); spr::drawWallTower(p, Pt{ -7, 3 }, cam, 1);
        spr::drawWallSeg(p, Pt{ 0, -2.5 }, Pt{ 7, 1.5 }, cam, 1); spr::drawWallTower(p, Pt{ 0, -2.5 }, cam, 1); spr::drawWallTower(p, Pt{ 7, 1.5 }, cam, 1);
        return;
    }
    if (kind == QLatin1String("road")) {
        disc(p, cam, 9, kLand);
        QPainterPath path;
        const double pts[5][2] = { { -9, 3.5 }, { -5, 0.5 }, { -1, 1.5 }, { 3, -1 }, { 9, -3.5 } };
        for (int i = 0; i < 5; i++) { const QPointF q = cam.proj(pts[i][0], pts[i][1], 0); if (i) path.lineTo(q); else path.moveTo(q); }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(128, 100, 68), s * 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.drawPath(path);
        p.setPen(QPen(QColor(184, 154, 110), s * 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.drawPath(path);
        QPen mid(QColor(120, 92, 60, 128), std::max(1.0, s * 0.25)); mid.setDashPattern({ 4, 4 }); p.setPen(mid); p.drawPath(path);
        return;
    }
    if (kind == QLatin1String("bridge")) {
        disc(p, cam, 9, kLand);
        QPolygonF river;
        for (const auto& q : { Pt{ -2.6, -8.6 }, Pt{ 2.6, -8.6 }, Pt{ 2.6, 8.6 }, Pt{ -2.6, 8.6 } }) river << cam.proj(q[0], q[1], 0);
        p.setBrush(rgbc(kSea)); p.setPen(Qt::NoPen); p.drawPolygon(river);
        spr::drawBridge(p, Path{ Pt{ -5.5, 0 }, Pt{ 5.5, 0 } }, cam);
        return;
    }
    Art o;
    o.kind = kind; o.yaw = yawFor(kind); o.v = 0.42;
    o.size = kind == QLatin1String("castle") ? 1 : 0;
    if (kind == QLatin1String("castle") && style != QLatin1String("human")) o.style = style;
    const ArtSpec* spec = artSpec(kind);
    double ext = 3;
    if (kind == QLatin1String("castle")) ext = spr::kCastle[1].R * 1.25 + 1;
    else if (kind == QLatin1String("tower")) ext = 2.6;
    else if (kind == QLatin1String("lighthouse")) ext = 2.4;
    else if (kind == QLatin1String("windmill")) ext = 4;
    else if (kind == QLatin1String("temple")) ext = 4.6;
    else if (kind == QLatin1String("ruins")) ext = 5.6;
    else if (spec) ext = spec->r;
    disc(p, cam, std::max(4.0, ext * 1.15), spec && spec->water ? kSea : kLand);
    if (spr::drawArt4(p, o, cam, kFlag) || spr::drawArt(p, o, cam, kFlag)) {}
    else if (kind == QLatin1String("castle")) spr::drawCastle(p, 0, 0, o.yaw, 1, cam, kFlag);
    else if (kind == QLatin1String("tower")) spr::drawTowerArt(p, o, cam, kFlag);
    else if (kind == QLatin1String("lighthouse")) spr::drawLighthouse(p, o, cam);
    else if (kind == QLatin1String("windmill")) spr::drawWindmill(p, o, cam);
    else if (kind == QLatin1String("temple")) spr::drawTemple(p, o, cam);
    else if (kind == QLatin1String("ruins")) spr::drawRuins(p, o, cam);
}
} // namespace

QPixmap BuildBar::thumb(const QString& kind, const QString& style) const
{
    const double dpr = std::min(2.0, m_host->devicePixelRatioF());
    const QString key = kind + QLatin1Char('|') + style + QLatin1Char('|') + QString::number(dpr);
    if (m_thumbs.contains(key)) return m_thumbs.value(key);
    spr::resetSun();                       // luz de dia (o próximo quadro do mapa devolve a luz da hora)
    // passada 1: desenha grande pra medir; passada 2: na escala que cabe na cartinha
    constexpr int N = 520;
    constexpr double s0 = 6, ox0 = 260, oy0 = 330;
    QImage big(N, N, QImage::Format_ARGB32_Premultiplied);
    big.fill(Qt::transparent);
    { QPainter p(&big); paintItem(p, kind, style, vitrine(s0, ox0, oy0), s0); }
    int x0 = N, y0 = N, x1 = 0, y1 = 0;
    for (int y = 0; y < N; y += 2) {
        const QRgb* row = reinterpret_cast<const QRgb*>(big.constScanLine(y));
        for (int x = 0; x < N; x += 2) if (qAlpha(row[x]) > 10) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
    }
    QPixmap out;
    if (x1 > x0) {
        const int W = qRound(kThumbW * dpr), H = qRound(kThumbH * dpr);
        const double pad = 5 * dpr, s = s0 * std::min((W - 2 * pad) / (x1 - x0 + 2), (H - 2 * pad) / (y1 - y0 + 2));
        const double cx = (x0 + x1) / 2.0, cy = (y0 + y1) / 2.0;
        QImage img(W, H, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        { QPainter p(&img); paintItem(p, kind, style, vitrine(s, W / 2.0 - (cx - ox0) * s / s0, H / 2.0 - (cy - oy0) * s / s0), s); }
        img.setDevicePixelRatio(dpr);
        out = QPixmap::fromImage(img);
    }
    m_thumbs.insert(key, out);
    return out;
}

BuildBar::BuildBar(QWidget* host, Canvas* canvas, Doc* doc) : QObject(host), m_host(host), m_canvas(canvas), m_doc(doc)
{
    m_cats = {
        // v32: os castelos ganharam categoria própria, uma cartinha por povo
        { QStringLiteral("cas"), Panel::tr("Castelos"), QStringLiteral("castle"),
          { "castle:human", "castle:norman", "castle:moat", "castle:chateau", "castle:crag", "castle:elven", "castle:dwarf", "castle:orc",
            "castle:oriental", "castle:desert", "castle:dark", "castle:ice" } },
        { QStringLiteral("uni"), Panel::tr("Únicos"), QStringLiteral("tower"), { "tower", "lighthouse", "windmill", "temple", "ruins", "bridge" } },
        { QStringLiteral("brs"), Panel::tr("Pincéis"), QStringLiteral("house"), { "village", "city", "fields", "forest", "rocks", "wall", "road", "erase" } },
        { QStringLiteral("more"), Panel::tr("Mais"), QStringLiteral("a-statue"),
          { "statue", "cathedral", "market", "aqueduct", "colosseum", "cave", "mine", "mage", "greattree", "stones", "camp" } },
        { QStringLiteral("leg"), Panel::tr("Lendas"), QStringLiteral("a-dragon"), { "dragon", "bones", "portal", "floatisland", "sword", "heads" } },
        { QStringLiteral("sea"), Panel::tr("Mar"), QStringLiteral("a-longship"), { "ship", "longship", "wreck", "seaserpent", "kraken" } },
        { QStringLiteral("mon"), Panel::tr("Monumentos"), QStringLiteral("a-pyramid"),
          { "colossus", "guardians", "seatedgod", "arch", "pyramid", "ziggurat", "pagoda", "obelisk", "graveyard", "farm" } },
    };
    auto btn = [this](QWidget* parent, const QString& iconName, const QString& tip, const QString& obj = QStringLiteral("cartoBtn")) {
        auto* b = new QToolButton(parent);
        b->setObjectName(obj);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setIconSize(QSize(18, 18));
        b->setFixedSize(30, 30);
        m_icons.append({ b, iconName });
        return b;
    };
    auto mini = [this](QWidget* parent, QHBoxLayout* l, QHash<QString, QToolButton*>& out, const QStringList& values, const QStringList& labels,
                       const QStringList& tips, const std::function<void(const QString&)>& onPick) {
        auto* seg = new QFrame(parent);
        seg->setObjectName(QStringLiteral("cartoMiniSeg"));
        auto* h = new QHBoxLayout(seg);
        h->setContentsMargins(2, 2, 2, 2);
        h->setSpacing(2);
        for (int i = 0; i < values.size(); i++) {
            auto* b = new QToolButton(seg);
            b->setObjectName(QStringLiteral("cartoMini"));
            b->setText(labels.value(i));
            b->setToolTip(tips.value(i));
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            const QString v = values[i];
            connect(b, &QToolButton::clicked, this, [onPick, v]() { onPick(v); });
            out.insert(v, b);
            h->addWidget(b);
        }
        l->addWidget(seg);
    };

    // ── a faixa ──
    m_bar = new QFrame(host);
    m_bar->setObjectName(QStringLiteral("cartoBuildBar"));
    m_bar->setProperty("cartoFloat", true);
    m_bar->setAttribute(Qt::WA_StyledBackground, true);
    m_bar->setFixedHeight(kHeight);
    auto* bl = new QHBoxLayout(m_bar);
    bl->setContentsMargins(1, 1, 1, 1);
    bl->setSpacing(0);
    m_catCol = new QWidget(m_bar);
    m_catCol->setObjectName(QStringLiteral("cartoCatCol"));
    m_catCol->setAttribute(Qt::WA_StyledBackground, true);
    m_catCol->setFixedWidth(132);
    auto* cl = new QVBoxLayout(m_catCol);
    cl->setContentsMargins(6, 6, 6, 6);
    cl->setSpacing(1);
    cl->addStretch();
    for (const Cat& c : m_cats) {
        auto* b = new QPushButton(c.name, m_catCol);       // QPushButton: o texto encosta à esquerda
        b->setObjectName(QStringLiteral("cartoCat"));
        b->setToolTip(c.name);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setIconSize(QSize(15, 15));
        b->setFixedHeight(19);          // sete categorias na faixa de 158 px
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        const QString id = c.id;
        connect(b, &QPushButton::clicked, this, [this, id]() { pickCat(id); });
        m_icons.append({ b, c.icon });
        m_catBtns.insert(id, b);
        cl->addWidget(b);
    }
    cl->addStretch();
    bl->addWidget(m_catCol);
    auto* vsep = new QFrame(m_bar);
    vsep->setObjectName(QStringLiteral("cartoSep"));
    vsep->setFixedWidth(1);
    bl->addWidget(vsep);
    auto* main = new QWidget(m_bar);
    auto* ml = new QVBoxLayout(main);
    ml->setContentsMargins(0, 0, 0, 0);
    ml->setSpacing(0);
    auto* head = new QWidget(main);
    head->setFixedHeight(38);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(14, 0, 6, 0);
    hl->setSpacing(8);
    m_title = new QLabel(head);
    m_title->setObjectName(QStringLiteral("cartoBarTitle"));
    hl->addWidget(m_title);
    hl->addStretch(1);
    // contexto: só o que vale pro item escolhido
    m_castleRow = new QWidget(head);
    auto* cr = new QHBoxLayout(m_castleRow);
    cr->setContentsMargins(0, 0, 0, 0);
    cr->setSpacing(8);
    m_sizeCap = new QLabel(Panel::tr("PLANTA"), m_castleRow);
    m_sizeCap->setObjectName(QStringLiteral("cartoCaption"));
    cr->addWidget(m_sizeCap);
    mini(m_castleRow, cr, m_sizeBtns, { QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3") },
         { Panel::tr("Pequeno"), Panel::tr("Médio"), Panel::tr("Grande"), Panel::tr("Colossal") },
         { Panel::tr("Pequeno"), Panel::tr("Médio"), Panel::tr("Grande"), Panel::tr("Colossal") }, [this](const QString& v) {
             const int size = v.toInt();
             if (Art* a = m_canvas->selectedArt(); a && a->kind == QLatin1String("castle")) {
                 const QString id = a->id;
                 m_canvas->userEdit([id, size](Info& in) { for (Art& x : in.user.arts) if (x.id == id) { x.size = size; return true; } return false; });
             } else m_canvas->opts().castleSize = size;
             if (changed) changed();
         });
    // o povo do castelo selecionado (pra trocar sem recolocar)
    m_styleBox = new QComboBox(m_castleRow);
    m_styleBox->setObjectName(QStringLiteral("cartoField"));
    m_styleBox->setFocusPolicy(Qt::NoFocus);
    for (const StyleSw& s : kStyles) m_styleBox->addItem(Panel::tr(s.label), QString::fromLatin1(s.v));
    connect(m_styleBox, &QComboBox::activated, this, [this](int i) { pickStyle(m_styleBox->itemData(i).toString()); });
    cr->addWidget(m_styleBox);
    hl->addWidget(m_castleRow);
    // muralha: pedra ou paliçada
    m_wallRow = new QWidget(head);
    auto* wr = new QHBoxLayout(m_wallRow);
    wr->setContentsMargins(0, 0, 0, 0);
    mini(m_wallRow, wr, m_wallBtns, { QString(), QStringLiteral("wood") }, { Panel::tr("Pedra"), Panel::tr("Paliçada") },
         { Panel::tr("Muralha de pedra com ameias e torres"), Panel::tr("Toras pontudas e torres de vigia de madeira") },
         [this](const QString& v) { m_canvas->opts().wallKind = v; if (changed) changed(); });
    hl->addWidget(m_wallRow);
    // floresta: a árvore
    m_treeRow = new QWidget(head);
    auto* tr2 = new QHBoxLayout(m_treeRow);
    tr2->setContentsMargins(0, 0, 0, 0);
    mini(m_treeRow, tr2, m_treeBtns, { QStringLiteral("auto"), QStringLiteral("broad"), QStringLiteral("pine"), QStringLiteral("palm") },
         { Panel::tr("Do clima"), Panel::tr("Folhosa"), Panel::tr("Pinheiro"), Panel::tr("Palmeira") },
         { Panel::tr("A árvore que o lugar daria (pinheiro no frio, cacto no deserto, junco no brejo)"), Panel::tr("Copa redonda"), Panel::tr("Pinheiro"), Panel::tr("Palmeira") },
         [this](const QString& v) { m_canvas->opts().treeMode = v; if (changed) changed(); });
    hl->addWidget(m_treeRow);
    m_roadRow = new QWidget(head);
    auto* rr = new QHBoxLayout(m_roadRow);
    rr->setContentsMargins(0, 0, 0, 0);
    rr->setSpacing(8);
    mini(m_roadRow, rr, m_rmodeBtns, { QStringLiteral("route"), QStringLiteral("hand") }, { Panel::tr("Pelo vale"), Panel::tr("À mão") },
         { Panel::tr("Clique no começo e no fim: ela acha o caminho pelo vale"), Panel::tr("Desenhe o traço") },
         [this](const QString& v) { m_canvas->opts().roadMode = v; m_canvas->optsChanged(); if (changed) changed(); });
    mini(m_roadRow, rr, m_rkindBtns, { QStringLiteral("trail"), QStringLiteral("road"), QStringLiteral("royal") }, { Panel::tr("Trilha"), Panel::tr("Estrada"), Panel::tr("Real") },
         { Panel::tr("Caminho de terra estreito"), Panel::tr("Estrada de terra"), Panel::tr("Estrada real, calçada de pedra, com lanternas à noite") },
         [this](const QString& v) { m_canvas->opts().roadKind = v; m_canvas->optsChanged(); if (changed) changed(); });
    hl->addWidget(m_roadRow);
    m_brushRow = new QWidget(head);
    auto* br = new QHBoxLayout(m_brushRow);
    br->setContentsMargins(0, 0, 0, 0);
    br->setSpacing(8);
    m_brushCap = new QLabel(Panel::tr("PINCEL"), m_brushRow);
    m_brushCap->setObjectName(QStringLiteral("cartoCaption"));
    br->addWidget(m_brushCap);
    m_brush = new QSlider(Qt::Horizontal, m_brushRow);
    m_brush->setRange(4, 60);
    m_brush->setFixedWidth(110);
    m_brush->setFocusPolicy(Qt::NoFocus);
    br->addWidget(m_brush);
    m_brushValue = new QLabel(m_brushRow);
    m_brushValue->setObjectName(QStringLiteral("cartoValue"));
    m_brushValue->setMinimumWidth(18);
    br->addWidget(m_brushValue);
    connect(m_brush, &QSlider::valueChanged, this, [this](int v) { m_canvas->opts().buildBrush = v; m_brushValue->setText(QString::number(v)); m_canvas->update(); });
    hl->addWidget(m_brushRow);
    m_selectBtn = btn(head, QStringLiteral("cursor"), Panel::tr("Selecione um artefato pra arrastar, girar, renomear ou apagar"));
    m_selectBtn->setCheckable(true);
    connect(m_selectBtn, &QToolButton::clicked, this, [this]() { pickKind(QStringLiteral("select")); });
    hl->addWidget(m_selectBtn);
    m_gearBtn = btn(head, QStringLiteral("gear"), Panel::tr("Escala do mundo e cidades sugeridas"));
    m_gearBtn->setCheckable(true);
    connect(m_gearBtn, &QToolButton::clicked, this, [this]() { m_gear->setVisible(!m_gear->isVisible()); sync(); });
    hl->addWidget(m_gearBtn);
    ml->addWidget(head);
    m_scroll = new QScrollArea(main);
    m_scroll->setObjectName(QStringLiteral("cartoCardScroll"));
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setFocusPolicy(Qt::NoFocus);
    m_rowBody = new QWidget();
    m_rowBody->setObjectName(QStringLiteral("cartoCardRow"));
    m_rowLay = new QHBoxLayout(m_rowBody);
    m_rowLay->setContentsMargins(10, 0, 10, 4);
    m_rowLay->setSpacing(4);
    m_rowLay->addStretch(1);
    m_scroll->setWidget(m_rowBody);
    m_scroll->viewport()->installEventFilter(this);
    ml->addWidget(m_scroll, 1);
    bl->addWidget(main, 1);
    m_bar->hide();

    // ── engrenagem ──
    m_gear = new QFrame(host);
    m_gear->setProperty("cartoFloat", true);
    m_gear->setAttribute(Qt::WA_StyledBackground, true);
    m_gear->setFixedWidth(344);
    auto* gl = new QVBoxLayout(m_gear);
    gl->setContentsMargins(12, 12, 12, 12);
    gl->setSpacing(8);
    auto* gcap = new QLabel(Panel::tr("ESCALA DO MUNDO"), m_gear);
    gcap->setObjectName(QStringLiteral("cartoCaption"));
    gl->addWidget(gcap);
    auto* sr = new QHBoxLayout();
    sr->setSpacing(4);
    const QStringList sv = { QStringLiteral("1"), QStringLiteral("0.7"), QStringLiteral("0.5") };
    const QStringList sl = { Panel::tr("Normal"), Panel::tr("Menor"), Panel::tr("Mínima") };
    const QStringList st = { Panel::tr("Tudo que é construído ou plantado no tamanho de sempre"), Panel::tr("Construções e árvores menores: o mesmo continente parece um mundo maior"),
                             Panel::tr("Ainda menores: o continente vira um império") };
    for (int i = 0; i < 3; i++) {
        auto* b = new QToolButton(m_gear);
        b->setObjectName(QStringLiteral("cartoSeg"));
        b->setText(sl[i]); b->setToolTip(st[i]);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        const double v = sv[i].toDouble();
        connect(b, &QToolButton::clicked, this, [this, v]() { if (setScale) setScale(v); });
        m_scaleBtns.insert(sv[i], b);
        sr->addWidget(b);
    }
    gl->addLayout(sr);
    m_showGen = new QToolButton(m_gear);
    m_showGen->setObjectName(QStringLiteral("cartoCheck"));
    m_showGen->setCheckable(true);
    m_showGen->setCursor(Qt::PointingHandCursor);
    m_showGen->setFocusPolicy(Qt::NoFocus);
    m_showGen->setText(Panel::tr("Mostrar cidades e estradas sugeridas pelo terreno"));
    m_showGen->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_showGen->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_showGen, &QToolButton::clicked, this, [this](bool on) { m_canvas->userEdit([on](Info& in) { in.user.showGen = on; return true; }); if (changed) changed(); });
    gl->addWidget(m_showGen);
    m_gear->hide();

    // ── pílula em cima do selecionado ──
    m_sel = new QFrame(host);
    m_sel->setObjectName(QStringLiteral("cartoSelPill"));
    m_sel->setProperty("cartoFloat", true);
    m_sel->setAttribute(Qt::WA_StyledBackground, true);
    auto* pl = new QHBoxLayout(m_sel);
    pl->setContentsMargins(10, 3, 4, 3);
    pl->setSpacing(2);
    m_selName = new QLineEdit(m_sel);
    m_selName->setObjectName(QStringLiteral("cartoSelName"));
    m_selName->setFixedWidth(140);
    connect(m_selName, &QLineEdit::editingFinished, this, [this]() {
        Art* a = m_canvas->selectedArt();
        if (!a) return;
        const QString v = m_selName->text().trimmed(), id = a->id;
        if (v == a->name) return;
        m_canvas->userEdit([id, v](Info& in) { for (Art& x : in.user.arts) if (x.id == id) { x.name = v; return true; } return false; });
        m_selName->clearFocus();
    });
    pl->addWidget(m_selName);
    auto* rl = btn(m_sel, QStringLiteral("rotl"), Panel::tr("Girar (Q)"));
    auto* rrb = btn(m_sel, QStringLiteral("rotr"), Panel::tr("Girar (E)"));
    connect(rl, &QToolButton::clicked, this, [this]() { if (rotate) rotate(-M_PI / 12); });
    connect(rrb, &QToolButton::clicked, this, [this]() { if (rotate) rotate(M_PI / 12); });
    pl->addWidget(rl); pl->addWidget(rrb);
    auto* psep = new QFrame(m_sel);
    psep->setObjectName(QStringLiteral("cartoSep"));
    psep->setFixedSize(1, 18);
    pl->addWidget(psep, 0, Qt::AlignVCenter);
    // tamanho do selecionado (40% a 400%): o slider mexe ao vivo e o desfazer guarda o arrasto inteiro como um passo
    m_sizeIcon = new QLabel(m_sel);
    m_sizeIcon->setToolTip(Panel::tr("Tamanho (teclas [ e ]; duplo clique volta pra 100%)"));
    pl->addWidget(m_sizeIcon);
    m_size = new QSlider(Qt::Horizontal, m_sel);
    m_size->setRange(40, 400);
    m_size->setSingleStep(5);
    m_size->setPageStep(20);
    m_size->setFixedWidth(96);
    m_size->setFocusPolicy(Qt::NoFocus);
    m_size->setToolTip(m_sizeIcon->toolTip());
    m_size->installEventFilter(this);
    pl->addWidget(m_size);
    m_sizeValue = new QLabel(m_sel);
    m_sizeValue->setObjectName(QStringLiteral("cartoValue"));
    m_sizeValue->setMinimumWidth(34);
    pl->addWidget(m_sizeValue);
    connect(m_size, &QSlider::sliderPressed, this, [this]() { m_sizeBefore = std::make_shared<Doc::Layers>(m_doc->layers()); });
    connect(m_size, &QSlider::valueChanged, this, [this](int v) {
        m_sizeValue->setText(QStringLiteral("%1%").arg(v));
        Art* a = m_canvas->selectedArt();
        if (!a || !m_doc->info) return;
        const double sc = v / 100.0;
        if (std::fabs(a->sc - sc) < 1e-9) return;
        if (!m_size->isSliderDown()) { setSize(sc); return; }     // teclado/roda: um passo de desfazer
        a->sc = sc;
        m_doc->info->user.ver++;
        m_canvas->layersLive();
    });
    connect(m_size, &QSlider::sliderReleased, this, [this]() {
        if (m_sizeBefore) m_canvas->commitLayers(std::move(m_sizeBefore));
        m_sizeBefore.reset();
    });
    auto* psep2 = new QFrame(m_sel);
    psep2->setObjectName(QStringLiteral("cartoSep"));
    psep2->setFixedSize(1, 18);
    pl->addWidget(psep2, 0, Qt::AlignVCenter);
    auto* del = btn(m_sel, QStringLiteral("trash"), Panel::tr("Apagar (Delete)"));
    connect(del, &QToolButton::clicked, this, [this]() { if (remove) remove(); });
    pl->addWidget(del);
    m_sel->hide();
}

bool BuildBar::eventFilter(QObject* o, QEvent* e)
{
    // a roda do mouse anda a fileira pro lado
    if (o == m_scroll->viewport() && e->type() == QEvent::Wheel) {
        auto* we = static_cast<QWheelEvent*>(e);
        const int d = we->angleDelta().y() ? we->angleDelta().y() : we->angleDelta().x();
        m_scroll->horizontalScrollBar()->setValue(m_scroll->horizontalScrollBar()->value() - d);
        return true;
    }
    if (o == m_size && e->type() == QEvent::MouseButtonDblClick) { setSize(1); return true; }
    return QObject::eventFilter(o, e);
}

void BuildBar::setActive(bool on)
{
    m_active = on;
    m_bar->setVisible(on);
    if (!on) { m_gear->hide(); m_sel->hide(); }
    else { m_lastKind.clear(); sync(); }
}

QRect BuildBar::geometry() const { return m_bar->geometry(); }
bool BuildBar::owns(const QWidget* w) const { return w && (w == m_selName || m_sel->isAncestorOf(w)); }
bool BuildBar::escape()
{
    if (!m_gear->isVisible()) return false;
    m_gear->hide();
    sync();
    return true;
}

QString BuildBar::castleStyle() const
{
    Art* a = m_canvas->selectedArt();
    const QString s = a && a->kind == QLatin1String("castle") ? a->style : m_canvas->opts().castleStyle;
    return s.isEmpty() ? QStringLiteral("human") : s;
}

void BuildBar::pickCat(const QString& id)
{
    m_cat = id;
    sync();
}

void BuildBar::scaleSelected(double factor)
{
    if (const Art* a = m_canvas->selectedArt()) setSize(a->sc * factor);
}

void BuildBar::setSize(double sc)
{
    Art* a = m_canvas->selectedArt();
    if (!a) return;
    sc = std::max(0.4, std::min(4.0, std::round(sc * 100) / 100));
    const QString id = a->id;
    m_canvas->userEdit([id, sc](Info& in) { for (Art& x : in.user.arts) if (x.id == id) { if (std::fabs(x.sc - sc) < 1e-9) return false; x.sc = sc; return true; } return false; });
    if (changed) changed();
}

void BuildBar::pickKind(const QString& item)
{
    // como no Sims: pegar outro item do catálogo solta o que estava selecionado
    if (item != QLatin1String("select") && !m_canvas->selArt().isEmpty()) m_canvas->setSelArt(QString());
    const QString kind = baseKind(item);
    if (item.contains(QLatin1Char(':'))) { const QString st = styleOfItem(item); m_canvas->opts().castleStyle = st == QLatin1String("human") ? QString() : st; }
    m_canvas->opts().buildKind = kind;
    m_canvas->optsChanged();
    if (changed) changed();
}

void BuildBar::pickStyle(const QString& style)
{
    const QString v = style == QLatin1String("human") ? QString() : style;
    if (Art* a = m_canvas->selectedArt(); a && a->kind == QLatin1String("castle")) {
        const QString id = a->id;
        m_canvas->userEdit([id, v](Info& in) { for (Art& x : in.user.arts) if (x.id == id) { x.style = v; return true; } return false; });
    } else {
        m_canvas->opts().castleStyle = v;
        if (m_canvas->opts().buildKind != QLatin1String("castle")) { m_canvas->opts().buildKind = QStringLiteral("castle"); m_canvas->optsChanged(); }
    }
    if (changed) changed();
}

void BuildBar::renderRow()
{
    m_rowCat = m_cat;
    for (QWidget* c : m_cards) c->deleteLater();
    m_cards.clear();
    const Cat* cat = nullptr;
    for (const Cat& c : m_cats) if (c.id == m_cat) cat = &c;
    if (!cat) return;
    const QColor muted = Theme::toColor(Theme::textMuted());
    for (const QString& k : cat->items) {
        auto* card = new Card(m_rowBody);
        card->kind = k;
        const QString st = styleOfItem(k);
        const StyleSw* sw = styleSw(st);
        card->setText(sw ? Panel::tr(sw->label) : shortLabel(k));
        card->setToolTip(sw ? Panel::tr(sw->label) + QStringLiteral(": ") + Panel::tr(sw->tip) : tipOf(k));
        card->castle = false;              // (as amostrinhas de estilo viraram cartinhas próprias)
        card->style = st;
        card->pix = k == QLatin1String("erase") ? iconPixmap(QStringLiteral("erase"), muted, 30) : thumb(baseKind(k), st);
        card->onStyle = [this](const QString& s) { pickStyle(s); };
        connect(card, &QAbstractButton::clicked, this, [this, k]() { pickKind(k); });
        m_rowLay->insertWidget(m_rowLay->count() - 1, card);
        m_cards.append(card);
    }
    m_scroll->horizontalScrollBar()->setValue(0);
}

void BuildBar::setNarrow(bool on)
{
    // faixa estreita: categorias só com ícone (o nome fica no título da faixa) e rótulos curtos
    if (m_narrow == on) return;
    m_narrow = on;
    m_catCol->setFixedWidth(on ? 46 : 132);
    for (const Cat& c : m_cats) if (QPushButton* b = m_catBtns.value(c.id)) b->setText(on ? QString() : c.name);
    const QStringList full = { Panel::tr("Pequeno"), Panel::tr("Médio"), Panel::tr("Grande"), Panel::tr("Colossal") };
    const QStringList brief = { Panel::tr("Peq."), Panel::tr("Méd."), Panel::tr("Gde."), Panel::tr("Col.") };
    for (int i = 0; i < 4; i++) if (QToolButton* b = m_sizeBtns.value(QString::number(i))) b->setText(on ? brief[i] : full[i]);
    if (QToolButton* b = m_rmodeBtns.value(QStringLiteral("route"))) b->setText(on ? Panel::tr("Vale") : Panel::tr("Pelo vale"));
    m_sizeCap->setVisible(!on);
    m_brushCap->setVisible(!on);
}

void BuildBar::syncCtx()
{
    const ToolOpts& o = m_canvas->opts();
    const QString& k = o.buildKind;
    const Art* sel = m_canvas->selectedArt();
    const bool selCastle = sel && sel->kind == QLatin1String("castle");
    m_castleRow->setVisible(k == QLatin1String("castle") || (selCastle && !isBrushKind(k)));
    m_styleBox->setVisible(selCastle);
    if (selCastle) { const int i = m_styleBox->findData(sel->style.isEmpty() ? QStringLiteral("human") : sel->style); if (i >= 0 && m_styleBox->currentIndex() != i) m_styleBox->setCurrentIndex(i); }
    m_wallRow->setVisible(k == QLatin1String("wall"));
    for (auto it = m_wallBtns.begin(); it != m_wallBtns.end(); ++it) it.value()->setChecked(it.key() == o.wallKind);
    m_treeRow->setVisible(k == QLatin1String("forest"));
    for (auto it = m_treeBtns.begin(); it != m_treeBtns.end(); ++it) it.value()->setChecked(it.key() == o.treeMode);
    const int cs = selCastle ? sel->size : o.castleSize;
    for (auto it = m_sizeBtns.begin(); it != m_sizeBtns.end(); ++it) it.value()->setChecked(it.key().toInt() == cs);
    m_roadRow->setVisible(k == QLatin1String("road"));
    for (auto it = m_rmodeBtns.begin(); it != m_rmodeBtns.end(); ++it) it.value()->setChecked(it.key() == o.roadMode);
    for (auto it = m_rkindBtns.begin(); it != m_rkindBtns.end(); ++it) it.value()->setChecked(it.key() == o.roadKind);
    m_brushRow->setVisible(isPaintBrush(k));
    if (m_brush->value() != qRound(o.buildBrush)) { m_brush->blockSignals(true); m_brush->setValue(qRound(o.buildBrush)); m_brush->blockSignals(false); }
    m_brushValue->setText(QString::number(qRound(o.buildBrush)));
}

void BuildBar::sync()
{
    if (!m_active) return;
    const QString& kind = m_canvas->opts().buildKind;
    if (kind != m_lastKind) {                       // escolheu outro item: a faixa vai pra categoria dele
        m_lastKind = kind;
        for (const Cat& c : m_cats) for (const QString& it : c.items) if (baseKind(it) == kind) m_cat = c.id;
    }
    for (auto it = m_catBtns.begin(); it != m_catBtns.end(); ++it) it.value()->setChecked(it.key() == m_cat);
    for (const Cat& c : m_cats) if (c.id == m_cat) m_title->setText(c.name);
    if (m_rowCat != m_cat) renderRow();
    const QString st = m_canvas->opts().castleStyle.isEmpty() ? QStringLiteral("human") : m_canvas->opts().castleStyle;
    for (QWidget* w : m_cards) {
        auto* c = static_cast<Card*>(w);
        c->setChecked(c->style.isEmpty() ? c->kind == kind : kind == QLatin1String("castle") && c->style == st);
        c->update();
    }
    syncCtx();
    m_selectBtn->setChecked(kind == QLatin1String("select"));
    m_gearBtn->setChecked(m_gear->isVisible());
    const double sc = worldScale(m_doc->world);
    for (auto it = m_scaleBtns.begin(); it != m_scaleBtns.end(); ++it) it.value()->setChecked(std::fabs(it.key().toDouble() - sc) < 1e-6);
    m_showGen->setChecked(m_doc->info ? m_doc->info->user.showGen : true);
    m_showGen->setIcon(icon(m_showGen->isChecked() ? QStringLiteral("check") : QStringLiteral("box"), Theme::toColor(Theme::textMuted()), Theme::toColor(Theme::accentDefault())));
    if (const Art* a = m_canvas->selectedArt()) {
        if (!m_selName->hasFocus() || a->id != m_selId) m_selName->setText(a->name);
        m_selName->setPlaceholderText(Panel::tr("%1 · sem nome").arg(artName(a->kind)));
        m_selId = a->id;
        if (!m_size->isSliderDown()) { m_size->blockSignals(true); m_size->setValue(qRound(a->sc * 100)); m_size->blockSignals(false); }
        m_sizeValue->setText(QStringLiteral("%1%").arg(qRound(a->sc * 100)));
    } else m_selId.clear();
    if (m_gear->isVisible()) {
        m_gear->adjustSize();
        const QPoint g = m_gearBtn->mapTo(m_host, QPoint(m_gearBtn->width(), 0));
        m_gear->move(std::max(8, std::min(m_host->width() - m_gear->width() - 8, g.x() - m_gear->width())), g.y() - m_gear->height() - 8);
        m_gear->raise();
    }
    placeSel();
}

void BuildBar::place(int x, int y, int w)
{
    setNarrow(w < 660);
    m_bar->setGeometry(x, y, w, kHeight);
    m_bar->raise();
    if (m_gear->isVisible()) sync();
}

void BuildBar::placeSel()
{
    // depois de cada quadro da mesa: junta os pedidos e move uma vez só
    if (m_placeQueued) return;
    m_placeQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_placeQueued = false;
        const auto r = m_active ? m_canvas->selectedArtRect() : std::nullopt;
        if (!r) { m_sel->hide(); return; }
        m_sel->adjustSize();
        const int w = m_sel->width(), h = m_sel->height();
        const int x = std::max(8, std::min(m_host->width() - w - 8, qRound(r->center().x() - w / 2.0)));
        const int y = std::max(60, qRound(r->top()) - h - 10);
        if (m_sel->pos() != QPoint(x, y)) m_sel->move(x, y);
        if (!m_sel->isVisible()) { m_sel->show(); m_sel->raise(); }
    });
}

void BuildBar::applyTheme()
{
    const QColor muted = Theme::toColor(Theme::textMuted()), a = Theme::toColor(Theme::accentDefault());
    for (const auto& [b, name] : m_icons) b->setIcon(icon(name, muted, a));
    m_sizeIcon->setPixmap(iconPixmap(QStringLiteral("expand"), muted, 15));
    m_thumbs.clear();                     // a resolução da tela pode ter mudado; o desenho é o mesmo
    m_rowCat.clear();
    sync();
}

} // namespace carto
