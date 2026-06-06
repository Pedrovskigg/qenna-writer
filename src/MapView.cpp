#include "MapView.h"

#include "GeoData.h"
#include "MapPinsStore.h"
#include "Theme.h"

#include <QDateTime>
#include <QHash>
#include <QLineF>
#include <QLocale>
#include <QTimeZone>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QEasingCurve>
#include <QImage>
#include <QSettings>
#include <QShowEvent>
#include <QVariantAnimation>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {
struct Continent { const char* name; double lon; double lat; };
const Continent kContinents[] = {
    { "América do Norte", -100.0, 48.0 },
    { "América do Sul",    -60.0, -12.0 },
    { "Europa",             14.0,  52.0 },
    { "África",             20.0,   2.0 },
    { "Ásia",               90.0,  47.0 },
    { "Oceania",           140.0, -25.0 },
    { "Antártida",           0.0, -82.0 },
};

QString fmtNum(qint64 n)
{
    return QLocale(QLocale::Portuguese, QLocale::Brazil).toString(n);
}

// Interpola entre duas cores (t de 0 a 1) — pra derivar terra/fronteira do tema
// com contraste garantido.
QColor mix(const QColor& a, const QColor& b, double t)
{
    return QColor(int(a.red()   + (b.red()   - a.red())   * t),
                  int(a.green() + (b.green() - a.green()) * t),
                  int(a.blue()  + (b.blue()  - a.blue())  * t));
}

// Cor da terra por faixa climática (latitude). Mesma paleta do gradiente do mapa
// plano, pra que o globo combine com ele. No plano é um QLinearGradient vertical;
// no globo a latitude não é linear na tela, então colorimos por anel.
QColor climateColor(double lat)
{
    struct Stop { double lat; QColor c; };
    static const Stop stops[] = {
        {  90.0, QColor(0xc4, 0xcc, 0xd1) }, // polar (gelo)
        {  72.0, QColor(0xa6, 0xae, 0xa4) }, // tundra
        {  58.0, QColor(0x66, 0x7a, 0x5d) }, // boreal escuro
        {  44.0, QColor(0x84, 0x9b, 0x55) }, // temperado
        {  33.0, QColor(0xa3, 0x9a, 0x58) }, // estepe
        {  24.0, QColor(0xc2, 0xa6, 0x62) }, // árido / bege
        {  15.0, QColor(0x8d, 0x8c, 0x4a) }, // savana
        {   6.0, QColor(0x57, 0x88, 0x42) }, // tropical
        {   0.0, QColor(0x46, 0x78, 0x39) }, // equatorial escuro
        {  -6.0, QColor(0x57, 0x88, 0x42) },
        { -15.0, QColor(0x8d, 0x8c, 0x4a) },
        { -24.0, QColor(0xc2, 0xa6, 0x62) },
        { -33.0, QColor(0xa3, 0x9a, 0x58) },
        { -44.0, QColor(0x84, 0x9b, 0x55) },
        { -58.0, QColor(0x66, 0x7a, 0x5d) },
        { -72.0, QColor(0xa6, 0xae, 0xa4) },
        { -90.0, QColor(0xc4, 0xcc, 0xd1) },
    };
    const int n = int(std::size(stops));
    if (lat >= stops[0].lat)     return stops[0].c;
    if (lat <= stops[n - 1].lat) return stops[n - 1].c;
    for (int i = 0; i < n - 1; ++i) {
        if (lat <= stops[i].lat && lat >= stops[i + 1].lat) {
            const double t = (stops[i].lat - lat) / (stops[i].lat - stops[i + 1].lat);
            return mix(stops[i].c, stops[i + 1].c, t);
        }
    }
    return stops[0].c;
}

constexpr double kPi = 3.14159265358979323846;

// Distância em km entre dois pontos geográficos (Haversine).
double haversineKm(double lat1, double lon1, double lat2, double lon2)
{
    constexpr double R = 6371.0, D = kPi / 180.0;
    const double dLat = (lat2 - lat1) * D, dLon = (lon2 - lon1) * D;
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2)
        + std::cos(lat1 * D) * std::cos(lat2 * D) * std::sin(dLon / 2) * std::sin(dLon / 2);
    return R * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
}

// Offset UTC (segundos) de um ponto: usa o fuso da cidade mais próxima; se não
// houver cidade por perto (oceano), estima pela longitude.
int utcOffsetSec(double lon, double lat)
{
    const GeoData& geo = GeoData::instance();
    const GeoData::Place* best = nullptr;
    double bestD = 1e18;
    for (const GeoData::Place& p : geo.places()) {
        const double d = (p.lon - lon) * (p.lon - lon) + (p.lat - lat) * (p.lat - lat);
        if (d < bestD) { bestD = d; best = &p; }
    }
    if (best && bestD < 25.0 && !best->timezone.isEmpty()) {
        const QTimeZone tz(best->timezone.toUtf8());
        if (tz.isValid()) return tz.offsetFromUtc(QDateTime::currentDateTimeUtc());
    }
    return int(std::round(lon / 15.0)) * 3600;
}

QString continentName(const QString& code)
{
    if (code == QLatin1String("SA")) return QStringLiteral("América do Sul");
    if (code == QLatin1String("NA")) return QStringLiteral("América do Norte");
    if (code == QLatin1String("EU")) return QStringLiteral("Europa");
    if (code == QLatin1String("AS")) return QStringLiteral("Ásia");
    if (code == QLatin1String("AF")) return QStringLiteral("África");
    if (code == QLatin1String("OC")) return QStringLiteral("Oceania");
    if (code == QLatin1String("AN")) return QStringLiteral("Antártida");
    return code;
}

QString languageNames(const QString& codes)
{
    static const QHash<QString, QString> names = {
        {"af","Africâner"},{"sq","Albanês"},{"am","Amárico"},{"ar","Árabe"},
        {"hy","Armênio"},{"az","Azerbaijano"},{"eu","Basco"},{"be","Bielorrusso"},
        {"bn","Bengali"},{"bs","Bósnio"},{"bg","Búlgaro"},{"my","Birmanês"},
        {"ca","Catalão"},{"zh","Chinês"},{"hr","Croata"},{"cs","Tcheco"},
        {"da","Dinamarquês"},{"nl","Holandês"},{"en","Inglês"},{"et","Estoniano"},
        {"fi","Finlandês"},{"fr","Francês"},{"gl","Galego"},{"ka","Georgiano"},
        {"de","Alemão"},{"el","Grego"},{"gu","Guzerate"},{"ha","Hauçá"},
        {"he","Hebraico"},{"hi","Híndi"},{"hu","Húngaro"},{"is","Islandês"},
        {"ig","Igbo"},{"id","Indonésio"},{"ga","Irlandês"},{"it","Italiano"},
        {"ja","Japonês"},{"kk","Cazaque"},{"km","Khmer"},{"ko","Coreano"},
        {"ku","Curdo"},{"ky","Quirguiz"},{"lo","Laosiano"},{"lv","Letão"},
        {"lt","Lituano"},{"lb","Luxemburguês"},{"mk","Macedônio"},{"mg","Malgaxe"},
        {"ms","Malaio"},{"ml","Malaiala"},{"mt","Maltês"},{"mn","Mongol"},
        {"ne","Nepali"},{"no","Norueguês"},{"nb","Norueguês"},{"nn","Norueguês"},
        {"fa","Persa"},{"pl","Polonês"},{"pt","Português"},{"pa","Panjabi"},
        {"ro","Romeno"},{"ru","Russo"},{"sr","Sérvio"},{"si","Cingalês"},
        {"sk","Eslovaco"},{"sl","Esloveno"},{"so","Somali"},{"es","Espanhol"},
        {"sw","Suaíli"},{"sv","Sueco"},{"tg","Tadjique"},{"ta","Tâmil"},
        {"te","Télugo"},{"th","Tailandês"},{"bo","Tibetano"},{"ti","Tigrínia"},
        {"tr","Turco"},{"tk","Turcomeno"},{"uk","Ucraniano"},{"ur","Urdu"},
        {"uz","Uzbeque"},{"vi","Vietnamita"},{"cy","Galês"},{"yo","Iorubá"},
        {"zu","Zulu"},{"rw","Quiniaruanda"},{"sn","Shona"},{"xh","Xhosa"},
        {"st","Soto"},{"tn","Tswana"},{"wo","Uólofe"},{"ak","Akan"},
        {"lg","Luganda"},{"ny","Chichewa"},{"dz","Dzonga"},{"ps","Pashto"},
    };
    QStringList out;
    const QStringList parts = codes.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (int i = 0; i < parts.size(); ++i) {
        const QString raw = parts.at(i);
        // Só o idioma principal (1º) e os oficiais regionais (com sufixo de país,
        // ex pt-BR, de-CH). Idiomas soltos (es, en, fr no Brasil) são ruído.
        if (i != 0 && !raw.contains(QLatin1Char('-'))) continue;
        const QString base = raw.section(QLatin1Char('-'), 0, 0).toLower();
        const QString nm = names.value(base);
        if (nm.isEmpty()) continue;          // desconhecido: omite (sem sigla)
        if (!out.contains(nm)) out << nm;
        if (out.size() >= 4) break;
    }
    return out.join(QStringLiteral(", "));
}
}

MapView::MapView(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
    setMouseTracking(true); // hover ao vivo
    setCursor(Qt::OpenHandCursor);
    m_textured = QSettings().value(QStringLiteral("map/textured"), true).toBool();
    m_globe = QSettings().value(QStringLiteral("map/globe"), false).toBool();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, qOverload<>(&QWidget::update));
}

QPointF MapView::project(double lon, double lat) const
{
    // Equiretangular: x cresce com a longitude; y invertido (lat+ = cima).
    return QPointF(lon * m_scale + m_offset.x(),
                   -lat * m_scale + m_offset.y());
}

QPointF MapView::screenToLonLat(const QPointF& s) const
{
    if (m_scale <= 0.0) return QPointF();
    return QPointF((s.x() - m_offset.x()) / m_scale,
                   (m_offset.y() - s.y()) / m_scale);
}

// Normaliza a longitude pra [-180,180): ao girar o globo, m_globeLon0 (e o lon
// devolvido por invGlobe) sai dessa faixa, e o teste de país/estado falha sem isso.
static QPointF normLon(const QPointF& ll)
{
    double lon = std::fmod(ll.x() + 180.0, 360.0);
    if (lon < 0.0) lon += 360.0;
    return QPointF(lon - 180.0, ll.y());
}

int MapView::countryAt(const QPointF& llIn) const
{
    const QPointF ll = normLon(llIn);
    const QVector<GeoData::Country>& cs = GeoData::instance().countries();
    for (int i = 0; i < cs.size(); ++i) {
        if (!cs.at(i).bounds.contains(ll)) continue;
        for (const QPolygonF& ring : cs.at(i).rings)
            if (ring.containsPoint(ll, Qt::OddEvenFill)) return i;
    }
    return -1;
}

int MapView::stateAt(const QPointF& llIn) const
{
    const QPointF ll = normLon(llIn);
    const QVector<GeoData::State>& ss = GeoData::instance().states();
    for (int i = 0; i < ss.size(); ++i) {
        if (!ss.at(i).bounds.contains(ll)) continue;
        for (const QPolygonF& ring : ss.at(i).rings)
            if (ring.containsPoint(ll, Qt::OddEvenFill)) return i;
    }
    return -1;
}

void MapView::leaveEvent(QEvent*)
{
    if (m_hoverCountry >= 0 || m_hoverState >= 0) {
        m_hoverCountry = -1;
        m_hoverState = -1;
        update();
    }
}

void MapView::hitTest(const QPoint& pos)
{
    const QPointF ll = screenToLonLat(QPointF(pos));
    const double r = (m_fitScale > 0.0) ? m_scale / m_fitScale : 1.0;
    pickFeatureAt(ll, pos, r, [this](double lon, double lat, bool* vis) {
        if (vis) *vis = true;
        return project(lon, lat);
    });
}

void MapView::pickFeatureAt(const QPointF& ll, const QPoint& pos, double r,
                            const std::function<QPointF(double, double, bool*)>& proj)
{
    const GeoData& geo = GeoData::instance();

    // 1) Cidade próxima do clique (mesma visibilidade do desenho).
    const qint64 thr =
          (r < 2.0)  ? Q_INT64_C(9000000000)
        : (r < 4.5)  ? Q_INT64_C(3000000)
        : (r < 9.0)  ? Q_INT64_C(700000)
        : (r < 16.0) ? Q_INT64_C(150000)
        : (r < 28.0) ? Q_INT64_C(30000)
                     : Q_INT64_C(0);
    // Raio curto: só pega cidade se o clique for quase em cima do ponto.
    // Assim, clicar numa área livre do estado seleciona o estado.
    const GeoData::Place* bestCity = nullptr;
    double bestD = 8.0;
    for (const GeoData::Place& pl : geo.places()) {
        const bool show = pl.population >= thr
            || (pl.isCapital() && r >= 2.0)
            || (pl.isStateCapital() && r >= 4.5);
        if (!show) continue;
        bool vis = false;
        const QPointF s = proj(pl.lon, pl.lat, &vis);
        if (!vis) continue; // no globo, cidade no lado de trás não conta
        const double d = QLineF(s, QPointF(pos)).length();
        if (d < bestD) { bestD = d; bestCity = &pl; }
    }
    if (bestCity) {
        QString country = bestCity->countryCode;
        for (const GeoData::Country& c : geo.countries())
            if (c.iso == bestCity->countryCode) { country = c.name; break; }
        QString sub = tr("Cidade");
        if (bestCity->isCapital()) sub = tr("Capital nacional");
        else if (bestCity->isStateCapital()) sub = tr("Capital estadual");
        QStringList det;
        if (!bestCity->state.isEmpty()) det << tr("Estado: %1").arg(bestCity->state);
        if (!country.isEmpty()) det << tr("País: %1").arg(country);
        if (bestCity->population > 0) det << tr("População: %1").arg(fmtNum(bestCity->population));
        det << tr("Altitude: %1 m").arg(bestCity->elevation);
        if (!bestCity->timezone.isEmpty()) {
            det << tr("Fuso: %1").arg(bestCity->timezone);
            const QTimeZone tz(bestCity->timezone.toUtf8());
            if (tz.isValid()) {
                const QString hora = QDateTime::currentDateTime().toTimeZone(tz).toString(QStringLiteral("HH:mm"));
                det << tr("Hora local: %1").arg(hora);
            }
        }
        emit featureClicked(bestCity->name, sub, det, bestCity->countryCode);
        return;
    }

    // 2) Estado (quando há zoom de estado).
    if (r >= 4.5) {
        for (const GeoData::State& st : geo.states()) {
            if (!st.bounds.contains(ll)) continue;
            bool inside = false;
            for (const QPolygonF& ring : st.rings)
                if (ring.containsPoint(ll, Qt::OddEvenFill)) { inside = true; break; }
            if (!inside) continue;
            QString iso;
            for (const GeoData::Country& c : geo.countries())
                if (c.nameEn == st.country) { iso = c.iso; break; }
            emit featureClicked(st.name, tr("Estado"), { tr("País: %1").arg(st.country) }, iso);
            return;
        }
    }

    // 3) País (ficha completa).
    const int ci = countryAt(ll);
    if (ci >= 0) emitCountryFicha(ci);
}

void MapView::emitCountryFicha(int idx)
{
    const GeoData& geo = GeoData::instance();
    if (idx < 0 || idx >= geo.countries().size()) return;
    const GeoData::Country& c = geo.countries().at(idx);
    QStringList det;
    if (const GeoData::CountryInfo* ci = geo.countryInfo(c.iso)) {
        if (!ci->capital.isEmpty()) det << tr("Capital: %1").arg(ci->capital);
        if (ci->population > 0) det << tr("População: %1").arg(fmtNum(ci->population));
        if (!ci->languages.isEmpty()) det << tr("Idiomas: %1").arg(languageNames(ci->languages));
        if (!ci->currency.isEmpty()) det << tr("Moeda: %1").arg(ci->currency);
        if (!ci->continent.isEmpty()) det << tr("Continente: %1").arg(continentName(ci->continent));
        if (ci->area > 0) det << tr("Área: %1 km²").arg(fmtNum(static_cast<qint64>(ci->area)));
    }
    emit featureClicked(c.name, tr("País"), det, c.iso);
}

void MapView::fitToWidget()
{
    if (width() <= 0 || height() <= 0) return;
    m_fitScale = width() / 360.0;       // mundo (360°) cabe na largura
    m_scale = m_fitScale;
    m_offset = QPointF(width() / 2.0, height() / 2.0); // lon0/lat0 no centro
    m_fitted = true;
}

void MapView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (!m_fitted) fitToWidget();
}

// Converte uma escala do mapa plano (pixels/grau) no zoom equivalente do globo,
// pra busca/navegação levarem ao mesmo nível de detalhe nos dois modos.
double MapView::scaleToGlobeZoom(double scale) const
{
    const double k = qMin(width(), height()) * 0.42 * kPi / 180.0; // px/grau a zoom=1
    return (k > 0.0) ? std::clamp(scale / k, 0.6, 12.0) : m_globeZoom;
}

void MapView::startGlobeFly(double lon, double lat, double zoom)
{
    lat = std::clamp(lat, -89.0, 89.0);
    // Caminho mais curto em longitude (trata o antimeridiano: 170 -> -170 = +20).
    double dLon = lon - m_globeLon0;
    while (dLon >  180.0) dLon -= 360.0;
    while (dLon < -180.0) dLon += 360.0;
    m_flyLonFrom = m_globeLon0; m_flyLonDelta = dLon;
    m_flyLatFrom = m_globeLat0; m_flyLatTo   = lat;
    m_flyZoomFrom = m_globeZoom; m_flyZoomTo = zoom;

    if (!m_flyAnim) {
        m_flyAnim = new QVariantAnimation(this);
        m_flyAnim->setStartValue(0.0);
        m_flyAnim->setEndValue(1.0);
        m_flyAnim->setEasingCurve(QEasingCurve::InOutCubic);
        connect(m_flyAnim, &QVariantAnimation::valueChanged, this,
                [this](const QVariant& v) {
                    const double t = v.toDouble();
                    m_globeLon0 = m_flyLonFrom + m_flyLonDelta * t;
                    m_globeLat0 = m_flyLatFrom + (m_flyLatTo - m_flyLatFrom) * t;
                    m_globeZoom = m_flyZoomFrom + (m_flyZoomTo - m_flyZoomFrom) * t;
                    update();
                });
    }
    m_flyAnim->stop();
    // Duração proporcional ao quanto vai girar (perto = rápido, longe = ~1.1s).
    const double travel = std::abs(dLon) + std::abs(lat - m_flyLatFrom);
    m_flyAnim->setDuration(int(420 + std::min(1.0, travel / 220.0) * 680));
    m_flyAnim->start();
}

void MapView::flyToBounds(const QRectF& b)
{
    if (!m_fitted) fitToWidget();
    if (b.isNull() || b.width() <= 0.0 || b.height() <= 0.0) return;
    const double sx = width()  / b.width();
    const double sy = height() / b.height();
    double sc = std::min(sx, sy) * 0.75; // folga nas bordas
    sc = std::clamp(sc, m_fitScale, m_fitScale * 40.0);
    const QPointF c = b.center();
    if (m_globe) { // gira (animado) até a região e ajusta o zoom equivalente
        startGlobeFly(c.x(), c.y(), scaleToGlobeZoom(sc));
        return;
    }
    m_scale = sc;
    m_offset = QPointF(width() / 2.0 - c.x() * m_scale,
                       height() / 2.0 + c.y() * m_scale);
    update();
}

void MapView::flyToPoint(double lon, double lat)
{
    if (!m_fitted) fitToWidget();
    if (m_globe) {
        startGlobeFly(lon, lat, scaleToGlobeZoom(m_fitScale * 12.0));
        return;
    }
    m_scale = std::clamp(m_fitScale * 12.0, m_fitScale, m_fitScale * 40.0);
    m_offset = QPointF(width() / 2.0 - lon * m_scale,
                       height() / 2.0 + lat * m_scale);
    update();
}

void MapView::setRulerMode(bool on)
{
    m_ruler = on;
    m_rulerCount = 0;
    if (on) m_addPin = false;
    setCursor(on ? Qt::CrossCursor : Qt::OpenHandCursor);
    update();
}

void MapView::setPinsStore(MapPinsStore* s)
{
    m_pins = s;
    if (m_pins)
        connect(m_pins, &MapPinsStore::pinsChanged, this, qOverload<>(&QWidget::update));
    update();
}

void MapView::setAddPinMode(bool on)
{
    m_addPin = on;
    if (on) m_ruler = false;
    setCursor(on ? Qt::CrossCursor : Qt::OpenHandCursor);
    update();
}

void MapView::setTextured(bool on)
{
    m_textured = on;
    QSettings().setValue(QStringLiteral("map/textured"), on);
    update();
}

void MapView::setGlobeMode(bool on)
{
    m_globe = on;
    QSettings().setValue(QStringLiteral("map/globe"), on);
    setCursor(Qt::OpenHandCursor);
    update();
}

double MapView::globeRadius() const
{
    return qMin(width(), height()) * 0.42 * m_globeZoom;
}

// "r" do globo equivalente ao zoom do mapa plano: pixels por grau no centro do
// disco / escala de enquadramento. Assim as mesmas faixas de detalhe (cidades,
// estados, rótulos) valem pros dois modos.
double MapView::globeDetailRatio() const
{
    const double pxPerDeg = globeRadius() * kPi / 180.0;
    return (m_fitScale > 0.0) ? pxPerDeg / m_fitScale : m_globeZoom;
}

const QImage& MapView::relief() const
{
    if (!m_reliefTried) {
        m_reliefTried = true;
        QImage img(QStringLiteral(":/geo/relief.png"));
        if (!img.isNull())
            m_relief = img.convertToFormat(QImage::Format_Grayscale8);
    }
    return m_relief;
}

const QImage& MapView::oceanTex() const
{
    if (!m_oceanTried) {
        m_oceanTried = true;
        QImage img(QStringLiteral(":/geo/ocean.png"));
        if (!img.isNull())
            m_oceanTex = img.convertToFormat(QImage::Format_ARGB32);
    }
    return m_oceanTex;
}

// Projeta a textura de relevo equiretangular no disco do globo (projeção
// ortográfica inversa por pixel). Resultado em cinza: 128 = plano, claro =
// encosta ao sol, escuro = sombra. Cacheado: só refaz quando a câmera muda.
void MapView::buildGlobeShade()
{
    const QImage& rel = relief();
    const QImage& ocn = oceanTex();
    if (rel.isNull() && ocn.isNull()) return;
    if (m_shadeLon0 == m_globeLon0 && m_shadeLat0 == m_globeLat0
        && m_shadeZoom == m_globeZoom && m_shadeSize == size())
        return; // cache válido
    m_shadeLon0 = m_globeLon0; m_shadeLat0 = m_globeLat0;
    m_shadeZoom = m_globeZoom; m_shadeSize = size();

    // Conforme aproxima, o relevo desbota (ajuda a leitura), mas mantém um piso
    // mínimo: nunca fica 100% plano. Cheio até r=3, no piso a partir de r=8.
    const double fade = std::clamp((8.0 - globeDetailRatio()) / 5.0, 0.0, 1.0);
    const double strength = 0.12 + 0.88 * fade;

    // Resolução adaptativa: a projeção por pixel é o gargalo. No zoom out o disco
    // é pequeno → renderiza em cheia (nítido e barato). Conforme aproxima e o
    // disco cresce, reduz a escala pra travar o custo (e aí o relevo já está no
    // piso, então o leve borrão do upscale passa despercebido).
    const double SS = std::clamp(520.0 / globeRadius(), 0.15, 1.0);
    const int W = std::max(1, int(width()  * SS));
    const int H = std::max(1, int(height() * SS));
    QImage shade(W, H, QImage::Format_RGB32);
    shade.fill(qRgb(128, 128, 128));           // neutro: não altera no Overlay
    QImage ocean(W, H, QImage::Format_ARGB32);
    ocean.fill(Qt::transparent);               // fora do oceano: nada

    const double R = globeRadius() * SS;
    const double cx = W / 2.0, cy = H / 2.0;
    const double D = kPi / 180.0, la0 = m_globeLat0 * D;
    const double sLa0 = std::sin(la0), cLa0 = std::cos(la0);
    const int tw = !rel.isNull() ? rel.width()  : ocn.width();
    const int th = !rel.isNull() ? rel.height() : ocn.height();

    const int x0 = std::max(0, int(cx - R)), x1 = std::min(W - 1, int(cx + R));
    const int y0 = std::max(0, int(cy - R)), y1 = std::min(H - 1, int(cy + R));
    for (int py = y0; py <= y1; ++py) {
        QRgb* sline = reinterpret_cast<QRgb*>(shade.scanLine(py));
        QRgb* oline = reinterpret_cast<QRgb*>(ocean.scanLine(py));
        const double yp = cy - py; // y pra cima
        for (int px = x0; px <= x1; ++px) {
            const double xp = px - cx;
            const double rho = std::hypot(xp, yp);
            if (rho > R) continue; // fora do disco
            // sin(asin(rho/R)) = rho/R e cos = sqrt(1-(rho/R)^2): elimina 3 trig.
            const double sinc = rho / R;
            const double cosc = std::sqrt(std::max(0.0, 1.0 - sinc * sinc));
            const double lat = std::asin(std::clamp(
                cosc * sLa0 + (rho > 1e-9 ? yp * sinc * cLa0 / rho : 0.0),
                -1.0, 1.0)) / D;
            const double lon = m_globeLon0
                + std::atan2(xp * sinc, rho * cLa0 * cosc - yp * sLa0 * sinc) / D;
            double u = (lon + 180.0) / 360.0;
            u -= std::floor(u); // wrap longitude
            const double v = std::clamp((90.0 - lat) / 180.0, 0.0, 1.0);
            // Amostragem bilinear compartilhada (relevo e oceano têm mesma grade).
            const double fx = u * tw - 0.5, fy = v * th - 0.5;
            const int ix = int(std::floor(fx)), iy = int(std::floor(fy));
            const double tx = fx - ix, ty = fy - iy;
            const int ix0 = ((ix % tw) + tw) % tw, ix1 = (ix0 + 1) % tw;
            const int iy0 = std::clamp(iy, 0, th - 1), iy1 = std::clamp(iy + 1, 0, th - 1);
            if (!rel.isNull()) {
                const uchar* r0 = rel.constScanLine(iy0);
                const uchar* r1 = rel.constScanLine(iy1);
                const double top = r0[ix0] * (1.0 - tx) + r0[ix1] * tx;
                const double bot = r1[ix0] * (1.0 - tx) + r1[ix1] * tx;
                const int g = int(top * (1.0 - ty) + bot * ty + 0.5);
                const int gm = std::clamp(int(128.0 + (g - 128.0) * strength + 0.5), 0, 255);
                sline[px] = qRgb(gm, gm, gm);
            }
            if (!ocn.isNull()) {
                const QRgb* q0 = reinterpret_cast<const QRgb*>(ocn.constScanLine(iy0));
                const QRgb* q1 = reinterpret_cast<const QRgb*>(ocn.constScanLine(iy1));
                const QRgb a = q0[ix0], b = q0[ix1], c = q1[ix0], d = q1[ix1];
                auto bil = [&](int c00, int c10, int c01, int c11) {
                    const double t = c00 * (1.0 - tx) + c10 * tx;
                    const double bb = c01 * (1.0 - tx) + c11 * tx;
                    return int(t * (1.0 - ty) + bb * ty + 0.5);
                };
                const int A = bil(qAlpha(a), qAlpha(b), qAlpha(c), qAlpha(d));
                oline[px] = qRgba(bil(qRed(a), qRed(b), qRed(c), qRed(d)),
                                  bil(qGreen(a), qGreen(b), qGreen(c), qGreen(d)),
                                  bil(qBlue(a), qBlue(b), qBlue(c), qBlue(d)), A);
                // "Grava" o mar como neutro no relevo: dispensa recortar à terra.
                if (!rel.isNull() && A > 127) sline[px] = qRgb(128, 128, 128);
            }
        }
    }
    m_globeShade = shade;
    m_globeOcean = ocean;
}

QPointF MapView::projectGlobe(double lon, double lat, bool* visible) const
{
    const double D = kPi / 180.0;
    const double R = globeRadius();
    const double la = lat * D, la0 = m_globeLat0 * D, lo = (lon - m_globeLon0) * D;
    const double cosc = std::sin(la0) * std::sin(la)
                      + std::cos(la0) * std::cos(la) * std::cos(lo);
    if (visible) *visible = (cosc >= 0.0);
    const double x = R * std::cos(la) * std::sin(lo);
    const double y = R * (std::cos(la0) * std::sin(la) - std::sin(la0) * std::cos(la) * std::cos(lo));
    return QPointF(width() / 2.0 + x, height() / 2.0 - y);
}

QPointF MapView::invGlobe(const QPointF& screen, bool* ok) const
{
    const double D = kPi / 180.0;
    const double R = globeRadius();
    const double x = screen.x() - width() / 2.0;
    const double y = -(screen.y() - height() / 2.0);
    const double rho = std::sqrt(x * x + y * y);
    if (rho > R) { if (ok) *ok = false; return QPointF(); }
    if (ok) *ok = true;
    if (rho < 1e-6) return QPointF(m_globeLon0, m_globeLat0);
    const double cc = std::asin(std::clamp(rho / R, -1.0, 1.0));
    const double la0 = m_globeLat0 * D;
    const double lat = std::asin(std::clamp(
        std::cos(cc) * std::sin(la0) + y * std::sin(cc) * std::cos(la0) / rho, -1.0, 1.0)) / D;
    const double lon = m_globeLon0 + std::atan2(
        x * std::sin(cc), rho * std::cos(la0) * std::cos(cc) - y * std::sin(la0) * std::sin(cc)) / D;
    return QPointF(lon, lat);
}

void MapView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!m_fitted) fitToWidget();
}

void MapView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_globe) { paintGlobe(p); return; }

    const bool tex = m_textured;
    const QColor appBg(Theme::appBackground());
    const QColor textP(Theme::textPrimary());
    // Texturizado: esquema físico próprio (independe do tema). Simples: tema.
    const QColor water      = tex ? QColor(0x16, 0x33, 0x60) : appBg;
    const QColor stroke     = tex ? QColor(0x3c, 0x3c, 0x3c) : mix(appBg, textP, 0.92);
    const QColor capitalDot = tex ? QColor(0xb0, 0x3a, 0x2e) : QColor(Theme::accentDefault());
    const QColor cityDot     = tex ? QColor(0x55, 0x55, 0x55) : QColor(Theme::textMuted());
    const QColor stateCapDot = tex ? QColor(0x22, 0x22, 0x22) : QColor(Theme::textBright());
    // Etiqueta de mapa físico: texto claro com contorno escuro lê melhor sobre o
    // terreno verde/colorido do que texto escuro com halo branco.
    const QColor labelColor  = tex ? QColor(0xf2, 0xf2, 0xf2) : textP;
    const QColor bigLabel    = tex ? QColor(0xff, 0xff, 0xff) : QColor(Theme::textBright());
    const QColor haloColor   = tex ? QColor(0x10, 0x10, 0x10) : water;

    p.fillRect(rect(), water);

    const QColor stateStroke = tex ? QColor(0x5a, 0x5a, 0x5a) : mix(appBg, textP, 0.74);
    const GeoData& geo = GeoData::instance();

    // Desenha texto com um leve contorno (halo) pra os nomes se destacarem do
    // fundo em vez de se perderem na terra.
    // Contorno completo (8 direções) pra o nome se destacar do terreno em
    // qualquer cor — a cruz de 4 deixava cantos vazando e baixo contraste.
    static const QPointF kHaloOff[] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };
    auto haloText = [&](const QPointF& at, const QString& t, const QColor& fg) {
        p.setPen(haloColor);
        for (const QPointF& o : kHaloOff) p.drawText(at + o, t);
        p.setPen(fg);
        p.drawText(at, t);
    };
    const QRect view = rect().adjusted(-40, -40, 40, 40);

    // Nível de detalhe pela razão de zoom em relação ao enquadramento.
    const double r = (m_fitScale > 0.0) ? m_scale / m_fitScale : 1.0;
    const bool lvlContinents = r < 2.0;
    const bool lvlCountries  = (r >= 2.0 && r < 4.5);
    const bool lvlStates     = (r >= 4.5 && r < 9.0);
    const bool lvlCities     = r >= 9.0;

    auto boundsOnScreen = [&](const QRectF& geo) {
        const QPointF a = project(geo.left(), geo.top());
        const QPointF b = project(geo.right(), geo.bottom());
        return QRectF(a, b).normalized();
    };

    // Terra colorida por faixa climática (gradiente latitudinal) + fronteiras.
    // A projeção é linear na latitude, então um gradiente vertical em coords de
    // tela já corresponde às faixas de clima — países grandes mostram a variação.
    {
        QBrush landBrush;
        if (tex) {
            const double yTop = project(0.0, 90.0).y();
            const double yBot = project(0.0, -90.0).y();
            QLinearGradient climate(QPointF(0, yTop), QPointF(0, yBot));
            auto fr = [](double lat) { return (90.0 - lat) / 180.0; };
            for (double lat = 90.0; lat >= -90.0; lat -= 10.0)
                climate.setColorAt(fr(lat), climateColor(lat));
            landBrush = QBrush(climate);
        } else {
            landBrush = mix(appBg, textP, 0.26); // terra única do tema
        }
        p.setBrush(landBrush);
        p.setPen(QPen(stroke, 1.4));
        for (const GeoData::Country& c : geo.countries()) {
            for (const QPolygonF& ring : c.rings) {
                QPolygonF screen;
                screen.reserve(ring.size());
                for (const QPointF& pt : ring)
                    screen << project(pt.x(), pt.y());
                p.drawPolygon(screen);
            }
        }
    }

    // Divisões internas (estados): contorno TRACEJADO — destaca-se da borda do
    // país e fica nítido. Aparecem já num zoom moderado.
    if (r >= 3.0) {
        p.setBrush(Qt::NoBrush);
        QPen statePen(stateStroke, 1.3, Qt::DashLine);
        statePen.setDashPattern({3.0, 2.5}); // tracejado denso, bem visível
        p.setPen(statePen);
        for (const GeoData::State& st : geo.states()) {
            if (!boundsOnScreen(st.bounds).intersects(view)) continue;
            for (const QPolygonF& ring : st.rings) {
                QPolygonF screen;
                screen.reserve(ring.size());
                for (const QPointF& pt : ring)
                    screen << project(pt.x(), pt.y());
                p.drawPolygon(screen);
            }
        }
    }

    // Destaque do hover (estado em zoom, senão país sob o cursor).
    {
        auto drawHi = [&](const QVector<QPolygonF>& rings) {
            p.setBrush(QColor(255, 255, 255, 48));
            p.setPen(QPen(QColor(Theme::accentDefault()), 2.0));
            for (const QPolygonF& ring : rings) {
                QPolygonF sc;
                sc.reserve(ring.size());
                for (const QPointF& pt : ring) sc << project(pt.x(), pt.y());
                p.drawPolygon(sc);
            }
        };
        if (m_hoverState >= 0 && m_hoverState < geo.states().size())
            drawHi(geo.states().at(m_hoverState).rings);
        else if (m_hoverCountry >= 0 && m_hoverCountry < geo.countries().size())
            drawHi(geo.countries().at(m_hoverCountry).rings);
    }

    // Cidades/municípios: surgem por população conforme o zoom (capitais têm
    // prioridade). Quanto mais perto, menor a cidade que aparece.
    {
        const qint64 thr =
              (r < 2.0)  ? Q_INT64_C(9000000000)
            : (r < 4.5)  ? Q_INT64_C(3000000)
            : (r < 9.0)  ? Q_INT64_C(700000)
            : (r < 16.0) ? Q_INT64_C(150000)
            : (r < 28.0) ? Q_INT64_C(30000)
                         : Q_INT64_C(0);
        const bool cityLabels = r >= 6.0;

        // Coleta as visíveis (filtro + culling).
        struct Vis { QPointF s; const GeoData::Place* p; };
        QVector<Vis> vis;
        for (const GeoData::Place& place : geo.places()) {
            const bool show = place.population >= thr
                || (place.isCapital() && r >= 2.0)
                || (place.isStateCapital() && r >= 4.5);
            if (!show) continue;
            const QPointF s = project(place.lon, place.lat);
            if (!view.contains(s.toPoint())) continue;
            vis.append({ s, &place });
        }

        // Pontos de todas as visíveis.
        for (const Vis& v : vis) {
            p.setPen(Qt::NoPen);
            if (v.p->isCapital()) {
                p.setBrush(capitalDot);
                p.drawEllipse(v.s, 3.5, 3.5);
            } else if (v.p->isStateCapital()) {
                p.setBrush(stateCapDot);
                p.drawEllipse(v.s, 2.8, 2.8);
            } else {
                p.setBrush(cityDot);
                p.drawEllipse(v.s, 2.0, 2.0);
            }
        }

        // Rótulos com anti-sobreposição: prioriza capitais e maiores; só escreve
        // o nome se não colidir com um já escrito.
        if (cityLabels) {
            auto importance = [](const GeoData::Place* pl) -> qint64 {
                qint64 k = pl->population;
                if (pl->isCapital())           k += Q_INT64_C(1000000000000);
                else if (pl->isStateCapital()) k += Q_INT64_C(1000000000);
                return k;
            };
            std::sort(vis.begin(), vis.end(), [&](const Vis& a, const Vis& b) {
                return importance(a.p) > importance(b.p);
            });
            QFont cf = font();
            cf.setPointSize(8);
            QFont cfBold = cf;
            cfBold.setPointSize(9);
            cfBold.setBold(true);
            p.setFont(cf);
            const QFontMetrics fm(cf);
            QVector<QRectF> occupied;
            occupied.reserve(256);
            for (const Vis& v : vis) {
                const QString& nm = v.p->name;
                const QRectF lr(v.s.x() + 6, v.s.y() - fm.height() + 4,
                                fm.horizontalAdvance(nm) + 5, fm.height());
                bool collide = false;
                for (const QRectF& o : occupied)
                    if (o.intersects(lr)) { collide = true; break; }
                if (collide) continue;
                occupied.append(lr);
                p.setFont(v.p->isCapital() ? cfBold : cf);
                haloText(v.s + QPointF(6, 4), nm, v.p->isCapital() ? bigLabel : labelColor);
            }
        }
    }

    // Rótulos de região (continente / país / estado).
    QFont f = font();
    if (lvlContinents) {
        f.setPointSize(13);
        f.setBold(true);
        p.setFont(f);
        const QFontMetrics fm(f);
        for (const Continent& c : kContinents) {
            const QPointF s = project(c.lon, c.lat);
            if (!view.contains(s.toPoint())) continue;
            const QString name = QString::fromUtf8(c.name);
            haloText(s - QPointF(fm.horizontalAdvance(name) / 2.0, 0), name, bigLabel);
        }
    } else if (lvlCountries) {
        f.setPointSize(9);
        f.setBold(true);
        p.setFont(f);
        const QFontMetrics fm(f);
        // Ponto do rótulo: LABEL_X/Y do Natural Earth (sempre dentro do país,
        // trata o antimeridiano); fallback no centro do bounding box.
        auto labelPos = [](const GeoData::Country& c) {
            if (c.labelLon != 0.0 || c.labelLat != 0.0)
                return QPointF(c.labelLon, c.labelLat);
            return c.bounds.center();
        };
        // Maiores primeiro têm prioridade na anti-sobreposição.
        QVector<const GeoData::Country*> cands;
        for (const GeoData::Country& c : geo.countries()) {
            if (!view.contains(project(labelPos(c).x(), labelPos(c).y()).toPoint())) continue;
            cands.append(&c);
        }
        std::sort(cands.begin(), cands.end(), [](const GeoData::Country* a, const GeoData::Country* b) {
            return a->bounds.width() * a->bounds.height() > b->bounds.width() * b->bounds.height();
        });
        QVector<QRectF> occ;
        for (const GeoData::Country* c : cands) {
            const QPointF s = project(labelPos(*c).x(), labelPos(*c).y());
            const QPointF at = s - QPointF(fm.horizontalAdvance(c->name) / 2.0, 0);
            const QRectF lr(at.x(), at.y() - fm.height() + 4,
                            fm.horizontalAdvance(c->name) + 4, fm.height());
            bool col = false;
            for (const QRectF& o : occ) if (o.intersects(lr)) { col = true; break; }
            if (col) continue;
            occ.append(lr);
            haloText(at, c->name, bigLabel);
        }
    } else if (lvlStates) {
        f.setPointSize(8);
        f.setBold(true);
        p.setFont(f);
        const QFontMetrics fm(f);
        for (const GeoData::State& st : geo.states()) {
            if (st.bounds.width() * m_scale < 45.0) continue;
            const QPointF s = project(st.bounds.center().x(), st.bounds.center().y());
            if (!view.contains(s.toPoint())) continue;
            haloText(s - QPointF(fm.horizontalAdvance(st.name) / 2.0, 0), st.name, labelColor);
        }
    }

    // Régua: marcadores e linha entre os dois pontos.
    if (m_ruler && m_rulerCount >= 1) {
        const QColor rc(Theme::accentDefault());
        const QPointF a = project(m_rulerA.x(), m_rulerA.y());
        p.setPen(Qt::NoPen);
        p.setBrush(rc);
        p.drawEllipse(a, 4.0, 4.0);
        if (m_rulerCount == 2) {
            const QPointF b = project(m_rulerB.x(), m_rulerB.y());
            p.drawEllipse(b, 4.0, 4.0);
            QPen lp(rc, 1.6, Qt::DashLine);
            p.setPen(lp);
            p.setBrush(Qt::NoBrush);
            p.drawLine(a, b);
        }
    }

    // Pins de referência do projeto (sempre por cima).
    if (m_pins) {
        QFont pf = font();
        pf.setPointSize(9);
        pf.setBold(true);
        p.setFont(pf);
        const QColor pinColor(Theme::accentDefault());
        for (const MapPinsStore::Pin& pin : m_pins->pins()) {
            const QPointF s = project(pin.lon, pin.lat);
            if (!view.contains(s.toPoint())) continue;
            // Gota: círculo com um vértice apontando pra baixo.
            p.setPen(QPen(water, 1.0));
            p.setBrush(pinColor);
            p.drawEllipse(s + QPointF(0, -6), 6.0, 6.0);
            QPolygonF tip;
            tip << s + QPointF(-3.5, -3) << s + QPointF(3.5, -3) << s;
            p.drawPolygon(tip);
            p.setPen(Qt::NoPen);
            p.setBrush(water);
            p.drawEllipse(s + QPointF(0, -6), 2.0, 2.0);
            if (!pin.label.isEmpty())
                haloText(s + QPointF(9, -4), pin.label, bigLabel);
        }
    }

    // Tooltip do hover (nome do que está sob o cursor).
    if (m_hoverState >= 0 || m_hoverCountry >= 0) {
        QString name;
        if (m_hoverState >= 0 && m_hoverState < geo.states().size())
            name = geo.states().at(m_hoverState).name;
        else if (m_hoverCountry >= 0 && m_hoverCountry < geo.countries().size())
            name = geo.countries().at(m_hoverCountry).name;
        if (!name.isEmpty()) {
            QFont tf = font();
            tf.setPointSize(9);
            tf.setBold(true);
            p.setFont(tf);
            const QFontMetrics fm(tf);
            const qreal tw = fm.horizontalAdvance(name) + 14;
            const qreal th = fm.height() + 6;
            QRectF box(m_hoverPos.x() + 14, m_hoverPos.y() - th - 6, tw, th);
            if (box.right() > width() - 2) box.moveLeft(m_hoverPos.x() - tw - 8);
            if (box.top() < 2) box.moveTop(m_hoverPos.y() + 10);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(18, 18, 20, 215));
            p.drawRoundedRect(box, 5, 5);
            p.setPen(QColor(0xff, 0xff, 0xff));
            p.drawText(box, Qt::AlignCenter, name);
        }
    }
}

void MapView::paintGlobe(QPainter& p)
{
    p.setRenderHint(QPainter::SmoothPixmapTransform, true); // upscale suave do relevo
    const bool tex = m_textured;
    const QColor appBg(Theme::appBackground());
    const QColor textP(Theme::textPrimary());
    const QColor space  = tex ? QColor(0x0a, 0x0f, 0x18) : mix(appBg, QColor(0, 0, 0), 0.35);
    const QColor ocean  = tex ? QColor(0x16, 0x33, 0x60) : appBg;
    const QColor land        = tex ? QColor(0x6e, 0x9a, 0x54) : mix(appBg, textP, 0.26);
    const QColor stroke      = tex ? QColor(0x3c, 0x3c, 0x3c) : mix(appBg, textP, 0.92);
    const QColor stateStroke = tex ? QColor(0x5a, 0x5a, 0x5a) : mix(appBg, textP, 0.74);
    const QColor capitalDot  = tex ? QColor(0xb0, 0x3a, 0x2e) : QColor(Theme::accentDefault());
    const QColor cityDot     = tex ? QColor(0x55, 0x55, 0x55) : QColor(Theme::textMuted());
    const QColor stateCapDot = tex ? QColor(0x22, 0x22, 0x22) : QColor(Theme::textBright());
    const QColor labelColor  = tex ? QColor(0xf2, 0xf2, 0xf2) : textP;
    const QColor bigLabel    = tex ? QColor(0xff, 0xff, 0xff) : QColor(Theme::textBright());
    const QColor haloColor   = tex ? QColor(0x10, 0x10, 0x10) : ocean;
    const QColor pinColor(Theme::accentDefault());

    p.fillRect(rect(), space);

    const QPointF cen(width() / 2.0, height() / 2.0);
    const double R = globeRadius();

    // Estrelas de fundo (só texturizado): posições determinísticas por hash pra
    // não tremularem entre frames. Ficam atrás do disco, que é opaco.
    if (tex) {
        auto hash = [](quint32 n) {
            n = (n ^ 61u) ^ (n >> 16); n *= 9u; n ^= n >> 4;
            n *= 0x27d4eb2du; n ^= n >> 15; return n;
        };
        const int W = width(), H = height();
        const int count = qMax(60, (W * H) / 7000);
        p.setPen(Qt::NoPen);
        for (int i = 0; i < count; ++i) {
            const quint32 h1 = hash(quint32(i) * 2654435761u);
            const quint32 h2 = hash(h1 ^ 0x9e3779b9u);
            const double sx = double(h1 % 100000) / 100000.0 * W;
            const double sy = double(h2 % 100000) / 100000.0 * H;
            const double br = double(hash(h2) % 100) / 100.0;
            const double rad = (br > 0.92) ? 1.5 : (br > 0.7 ? 1.0 : 0.6);
            p.setBrush(QColor(255, 255, 255, int(40 + br * 160)));
            p.drawEllipse(QPointF(sx, sy), rad, rad);
        }
    }

    p.setPen(QPen(mix(ocean, QColor(0, 0, 0), 0.4), 1.5));
    p.setBrush(ocean);
    p.drawEllipse(cen, R, R);

    const GeoData& geo = GeoData::instance();

    // Recorta cada anel contra o hemisfério visível (plano z=0 da câmera) e
    // projeta. Grampear os vértices "de trás" na borda criava leques/triângulos
    // gigantes; o recorte de verdade segue a silhueta do globo, sem fragmentos.
    struct V3 { double x, y, z; };
    const double D = kPi / 180.0;
    const double la0 = m_globeLat0 * D;
    const double sLa0 = std::sin(la0), cLa0 = std::cos(la0);
    auto to3D = [&](const QPointF& ll) -> V3 {
        const double la = ll.y() * D, lo = (ll.x() - m_globeLon0) * D;
        const double cLa = std::cos(la), sLa = std::sin(la), cLo = std::cos(lo);
        return V3{ cLa * std::sin(lo),                       // x = direita na tela
                   cLa0 * sLa - sLa0 * cLa * cLo,            // y = cima na tela
                   sLa0 * sLa + cLa0 * cLa * cLo };          // z = em direção à câmera
    };
    auto toScreen = [&](const V3& v) {
        return QPointF(cen.x() + R * v.x, cen.y() - R * v.y);
    };
    auto ringToPoly = [&](const QPolygonF& ring) {
        QPolygonF poly;
        const int n = ring.size();
        if (n == 0) return poly;
        QVector<V3> in;
        in.reserve(n);
        for (const QPointF& pt : ring) in.push_back(to3D(pt));
        // Sutherland-Hodgman contra z >= 0; o cruzamento do horizonte é
        // reprojetado na borda do disco (normalizado pra cair na circunferência).
        for (int i = 0; i < n; ++i) {
            const V3& A = in[i];
            const V3& B = in[(i + 1) % n];
            const bool aIn = A.z >= 0.0, bIn = B.z >= 0.0;
            if (aIn) poly << toScreen(A);
            if (aIn != bIn) {
                const double t = A.z / (A.z - B.z);
                V3 I{ A.x + (B.x - A.x) * t, A.y + (B.y - A.y) * t, 0.0 };
                const double l = std::hypot(I.x, I.y);
                if (l > 1e-9) { I.x /= l; I.y /= l; }
                poly << toScreen(I);
            }
        }
        return poly;
    };
    static const QPointF kHaloOff[] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };
    auto haloText = [&](const QPointF& at, const QString& t, const QColor& fg) {
        p.setPen(haloColor);
        for (const QPointF& o : kHaloOff) p.drawText(at + o, t);
        p.setPen(fg); p.drawText(at, t);
    };
    // Um anel só é "da frente" se todos os vértices estão no hemisfério visível —
    // evita cordas feias na borda ao traçar contornos (estados) sem preenchimento.
    auto frontRing = [&](const QPolygonF& ring) {
        for (const QPointF& pt : ring) {
            bool vis;
            projectGlobe(pt.x(), pt.y(), &vis);
            if (!vis) return false;
        }
        return true;
    };

    const double r = globeDetailRatio();
    const bool lvlContinents = r < 2.0;
    const bool lvlCountries  = (r >= 2.0 && r < 4.5);
    const bool lvlStates     = (r >= 4.5 && r < 9.0);
    const double pxPerDeg = R * kPi / 180.0;

    QPainterPath clip;
    clip.addEllipse(cen, R, R);
    p.setClipPath(clip);

    // Oceano com profundidade: cor por distância à costa (raso turquesa -> fundo
    // azul-marinho), projetada sobre o disco. A terra é transparente na textura,
    // então só pinta o mar; a terra vetorial vem por cima logo abaixo.
    if (tex) {
        buildGlobeShade(); // gera relevo (m_globeShade) e oceano (m_globeOcean)
        if (!m_globeOcean.isNull())
            p.drawImage(rect(), m_globeOcean); // escala da meia-res pra tela cheia
    }

    // Terra: gradiente climático contínuo (textura) ou cor única do tema. Um único
    // brush pra todos os países evita o "mosaico" de cores que surgia ao colorir
    // cada anel sozinho (arquipélagos e países pequenos viravam quadradinhos
    // coloridos, e continentes grandes ficavam chapados).
    QBrush landBrush = land;
    if (tex) {
        // Faixas posicionadas pela projeção do meridiano central, pra acompanharem
        // a curvatura da esfera (gelo nos polos -> verde no equador).
        auto yOf = [&](double lat) {
            bool v;
            return projectGlobe(m_globeLon0, lat, &v).y();
        };
        const double yN = yOf(90.0), yS = yOf(-90.0);
        const double span = yS - yN;
        QLinearGradient g(QPointF(0, yN), QPointF(0, yS));
        double lastT = -1.0;
        for (double lat = 90.0; lat >= -90.0; lat -= 15.0) {
            const double t = (span != 0.0)
                ? std::clamp((yOf(lat) - yN) / span, 0.0, 1.0) : 0.0;
            if (t <= lastT) continue; // pula faixas que "voltam" no polo de trás
            g.setColorAt(t, climateColor(lat));
            lastT = t;
        }
        landBrush = QBrush(g);
    }
    QPainterPath landPath; // contorno da terra, pra recortar as manchas de cor
    p.setBrush(landBrush);
    p.setPen(QPen(stroke, 1.0));
    for (const GeoData::Country& c : geo.countries())
        for (const QPolygonF& ring : c.rings) {
            const QPolygonF sp = ringToPoly(ring);
            p.drawPolygon(sp);
            if (tex && sp.size() >= 3) {
                const QRectF bb = sp.boundingRect();
                if (bb.width() * bb.height() > 60.0) landPath.addPolygon(sp);
            }
        }

    // Variação de cor do terreno: manchas orgânicas (verde escuro/claro, bege,
    // marrom) sobre o gradiente base, recortadas à terra. Acompanham a rotação e
    // ficam visíveis em qualquer zoom (não desbotam como as nuvens).
    if (tex && !landPath.isEmpty()) {
        static const QColor kTerr[] = {
            QColor(0x3b, 0x55, 0x2e), QColor(0x9c, 0xb2, 0x66),
            QColor(0xbe, 0xa2, 0x62), QColor(0x84, 0x64, 0x3c),
            QColor(0x6d, 0x7e, 0x3a), QColor(0x53, 0x73, 0x3e),
            QColor(0xcb, 0xb6, 0x80), QColor(0x6f, 0x4f, 0x33),
        };
        const int nTerr = int(std::size(kTerr));
        quint32 ss = 0x9e3779b1u;
        auto rs = [&]() {
            ss = ss * 1664525u + 1013904223u;
            return double(ss >> 8) / double(0x01000000u);
        };
        p.save();
        p.setClipPath(landPath, Qt::IntersectClip);
        p.setPen(Qt::NoPen);
        const int splotches = 90;
        for (int s = 0; s < splotches; ++s) {
            const double slat = -78.0 + rs() * 156.0;
            const double slon = -180.0 + rs() * 360.0;
            const int ci = int(rs() * nTerr) % nTerr;
            const double sa = 0.16 + rs() * 0.26;
            const double rr = rs();
            const double fl = 0.55 + rs() * 0.5;
            const V3 cn = to3D(QPointF(slon, slat));
            if (cn.z <= 0.05) continue; // atrás do globo
            const int alpha = int(std::clamp(sa * cn.z, 0.0, 1.0) * 255);
            if (alpha < 4) continue;
            const double rpx = (5.0 + rr * 16.0) * pxPerDeg * (0.5 + 0.5 * cn.z);
            const QColor col = kTerr[ci];
            p.save();
            p.translate(toScreen(cn));
            p.scale(1.0, fl);
            QRadialGradient rg(QPointF(0, 0), rpx);
            rg.setColorAt(0.0, QColor(col.red(), col.green(), col.blue(), alpha));
            rg.setColorAt(0.7, QColor(col.red(), col.green(), col.blue(), int(alpha * 0.45)));
            rg.setColorAt(1.0, QColor(col.red(), col.green(), col.blue(), 0));
            p.setBrush(rg);
            p.drawEllipse(QPointF(0, 0), rpx, rpx);
            p.restore();
        }
        p.restore();
    }

    // Relevo (hillshade do GEBCO), modo Overlay (cinza 128 = neutro): realça
    // encostas ao sol e sombreia vales. O mar já está gravado como neutro na
    // textura, então desenha no disco inteiro sem precisar recortar à terra.
    if (tex && !m_globeShade.isNull()) {
        p.save();
        p.setCompositionMode(QPainter::CompositionMode_Overlay);
        p.drawImage(rect(), m_globeShade);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.restore();
    }

    // Divisões internas (estados): contorno tracejado, igual ao mapa plano.
    if (r >= 3.0) {
        p.setBrush(Qt::NoBrush);
        QPen statePen(stateStroke, 1.1, Qt::DashLine);
        statePen.setDashPattern({3.0, 2.5});
        p.setPen(statePen);
        for (const GeoData::State& st : geo.states())
            for (const QPolygonF& ring : st.rings)
                if (frontRing(ring)) p.drawPolygon(ringToPoly(ring));
    }

    // Destaque do hover (estado em zoom, senão país sob o cursor). Discreto no
    // globo: leve clareamento + um fio fino, pra não brigar com a textura.
    {
        auto drawHi = [&](const QVector<QPolygonF>& rings) {
            p.setBrush(QColor(255, 255, 255, 26));
            p.setPen(QPen(QColor(255, 255, 255, 120), 1.2));
            for (const QPolygonF& ring : rings)
                p.drawPolygon(ringToPoly(ring));
        };
        if (m_hoverState >= 0 && m_hoverState < geo.states().size())
            drawHi(geo.states().at(m_hoverState).rings);
        else if (m_hoverCountry >= 0 && m_hoverCountry < geo.countries().size())
            drawHi(geo.countries().at(m_hoverCountry).rings);
    }

    // ── Atmosfera, dia/noite e nuvens (planeta visto de longe) ──────────────
    // Atmosfera/dia-noite somem cedo (atmoFade); as nuvens persistem mais no
    // zoom (cloudFade), devolvendo o mapa funcional limpo só lá no detalhe.
    const double atmoFade  = std::clamp((3.0 - r) / 1.5, 0.0, 1.0);
    const double cloudFade = std::clamp((6.0 - r) / 3.5, 0.0, 1.0);
    if (tex && (atmoFade > 0.01 || cloudFade > 0.01)) {
        // Ponto subsolar pela hora UTC atual (sol "de verdade"); usado pelo
        // dia/noite e pela iluminação das nuvens.
        const QDateTime now = QDateTime::currentDateTimeUtc();
        const double secs = now.time().msecsSinceStartOfDay() / 1000.0;
        const double sunLon = 180.0 - secs / 86400.0 * 360.0;
        const int doy = now.date().dayOfYear();
        const double sunLat = 23.44 * std::sin(2.0 * kPi * (doy - 80) / 365.0);
        const V3 sun = to3D(QPointF(sunLon, sunLat));

        if (atmoFade > 0.01) {
            // Volume esférico: leve escurecimento na borda do disco.
            QRadialGradient sphere(cen, R);
            sphere.setColorAt(0.0,  QColor(0, 0, 0, 0));
            sphere.setColorAt(0.78, QColor(0, 0, 0, 0));
            sphere.setColorAt(1.0,  QColor(0, 0, 0, int(70 * atmoFade)));
            p.setPen(Qt::NoPen);
            p.setBrush(sphere);
            p.drawEllipse(cen, R, R);

            // Dia/noite.
            const QColor night(2, 6, 18, int(205 * atmoFade));
            const double dl = std::hypot(sun.x, sun.y);
            if (dl < 1e-3) {
                if (sun.z < 0) { p.setBrush(night); p.drawEllipse(cen, R, R); }
            } else {
                const QPointF dir(sun.x / dl, -sun.y / dl); // lado iluminado
                const QPointF bright = cen + dir * R;
                const QPointF dark   = cen - dir * R;
                const double term = std::clamp(0.5 + sun.z * 0.5, 0.06, 0.94);
                const QColor clear(night.red(), night.green(), night.blue(), 0);
                QLinearGradient g(bright, dark);
                g.setColorAt(0.0, clear);
                g.setColorAt(std::clamp(term - 0.14, 0.0, 1.0), clear);
                g.setColorAt(std::clamp(term + 0.16, 0.0, 1.0), night);
                g.setColorAt(1.0, night);
                p.setBrush(g);
                p.drawEllipse(cen, R, R);
            }

            // Brilho atmosférico (rim light) na borda do disco, concentrado na
            // direção do sol — passa por fora do disco, então sem o recorte.
            if (dl >= 1e-3) {
                p.save();
                p.setClipping(false);
                const double sunAngle = std::atan2(sun.y, sun.x) * 180.0 / kPi;
                auto rimGrad = [&](int peak) {
                    QConicalGradient cg(cen, sunAngle);
                    cg.setColorAt(0.00, QColor(168, 206, 255, peak));
                    cg.setColorAt(0.17, QColor(168, 206, 255, int(peak * 0.22)));
                    cg.setColorAt(0.50, QColor(168, 206, 255, 0));
                    cg.setColorAt(0.83, QColor(168, 206, 255, int(peak * 0.22)));
                    cg.setColorAt(1.00, QColor(168, 206, 255, peak));
                    return cg;
                };
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(QBrush(rimGrad(int(70 * atmoFade))), 7.0));   // glow externo
                p.drawEllipse(cen, R + 1.5, R + 1.5);
                p.setPen(QPen(QBrush(rimGrad(int(185 * atmoFade))), 2.2));  // núcleo
                p.drawEllipse(cen, R, R);
                p.restore();
            }
        }

        // Nuvens: clusters de blobs achatados (estratiformes), portadas do
        // MapClouds do Mira 1. Geradas em lon/lat (giram com a Terra), desbotam
        // na borda (z), no lado noturno (iluminação) e ao dar zoom (cloudFade).
        if (cloudFade > 0.01) {
            quint32 seed = 0x1234567u;
            auto rnd = [&]() {
                seed = seed * 1664525u + 1013904223u;
                return double(seed >> 8) / double(0x01000000u);
            };
            const int clusters = 30;
            for (int c = 0; c < clusters; ++c) {
                const double clat = -68.0 + rnd() * 136.0;
                const double clon = -180.0 + rnd() * 360.0;
                const double baseA = 0.24 + rnd() * 0.42;       // opacidade variada
                const int blobs = 5 + int(rnd() * 7);
                const double clSize = 0.65 + rnd() * 1.5;       // tamanho do cluster
                const double flat = 0.4 + rnd() * 0.35;         // achatamento
                const double cosLat = std::max(0.2, std::cos(clat * kPi / 180.0));
                for (int b = 0; b < blobs; ++b) {
                    // Consome todos os rnd() antes de qualquer 'continue', pra a
                    // sequência (e as nuvens) não mudarem ao girar o globo.
                    const double dlon = (b == 0 ? 0.0 : (rnd() - 0.5) * 42.0);
                    const double dlat = (rnd() - 0.5) * 11.0;
                    const double rr   = rnd();
                    const V3 cn = to3D(QPointF(clon + dlon / cosLat, clat + dlat));
                    if (cn.z <= 0.05) continue; // atrás do globo
                    const double illum = cn.x * sun.x + cn.y * sun.y + cn.z * sun.z;
                    const double dayF = std::clamp(illum * 1.4 + 0.35, 0.0, 1.0);
                    const double a = baseA * cn.z * cloudFade * dayF;
                    if (a < 0.02) continue;
                    const double rpx = (3.0 + rr * 9.0) * clSize * pxPerDeg
                                     * (0.55 + 0.45 * cn.z);
                    p.save();
                    p.translate(toScreen(cn));
                    p.scale(1.0, flat);
                    QRadialGradient rg(QPointF(0, 0), rpx);
                    rg.setColorAt(0.0,  QColor(246, 244, 238, int(std::min(255.0, 235 * a))));
                    rg.setColorAt(0.55, QColor(246, 244, 238, int(160 * a)));
                    rg.setColorAt(1.0,  QColor(246, 244, 238, 0));
                    p.setPen(Qt::NoPen);
                    p.setBrush(rg);
                    p.drawEllipse(QPointF(0, 0), rpx, rpx);
                    p.restore();
                }
            }
        }
    }

    p.setClipping(false);

    // Cidades/municípios: surgem por população conforme o zoom (capitais primeiro).
    {
        const qint64 thr =
              (r < 2.0)  ? Q_INT64_C(9000000000)
            : (r < 4.5)  ? Q_INT64_C(3000000)
            : (r < 9.0)  ? Q_INT64_C(700000)
            : (r < 16.0) ? Q_INT64_C(150000)
            : (r < 28.0) ? Q_INT64_C(30000)
                         : Q_INT64_C(0);
        const bool cityLabels = r >= 6.0;

        struct Vis { QPointF s; const GeoData::Place* p; };
        QVector<Vis> vis;
        for (const GeoData::Place& place : geo.places()) {
            const bool show = place.population >= thr
                || (place.isCapital() && r >= 2.0)
                || (place.isStateCapital() && r >= 4.5);
            if (!show) continue;
            bool vv;
            const QPointF s = projectGlobe(place.lon, place.lat, &vv);
            if (!vv) continue; // lado de trás do globo
            vis.append({ s, &place });
        }

        for (const Vis& v : vis) {
            p.setPen(Qt::NoPen);
            if (v.p->isCapital()) {
                p.setBrush(capitalDot);
                p.drawEllipse(v.s, 3.5, 3.5);
            } else if (v.p->isStateCapital()) {
                p.setBrush(stateCapDot);
                p.drawEllipse(v.s, 2.8, 2.8);
            } else {
                p.setBrush(cityDot);
                p.drawEllipse(v.s, 2.0, 2.0);
            }
        }

        if (cityLabels) {
            auto importance = [](const GeoData::Place* pl) -> qint64 {
                qint64 k = pl->population;
                if (pl->isCapital())           k += Q_INT64_C(1000000000000);
                else if (pl->isStateCapital()) k += Q_INT64_C(1000000000);
                return k;
            };
            std::sort(vis.begin(), vis.end(), [&](const Vis& a, const Vis& b) {
                return importance(a.p) > importance(b.p);
            });
            QFont cf = font();
            cf.setPointSize(8);
            QFont cfBold = cf;
            cfBold.setPointSize(9);
            cfBold.setBold(true);
            p.setFont(cf);
            const QFontMetrics fm(cf);
            QVector<QRectF> occupied;
            occupied.reserve(256);
            for (const Vis& v : vis) {
                const QString& nm = v.p->name;
                const QRectF lr(v.s.x() + 6, v.s.y() - fm.height() + 4,
                                fm.horizontalAdvance(nm) + 5, fm.height());
                bool collide = false;
                for (const QRectF& o : occupied)
                    if (o.intersects(lr)) { collide = true; break; }
                if (collide) continue;
                occupied.append(lr);
                p.setFont(v.p->isCapital() ? cfBold : cf);
                haloText(v.s + QPointF(6, 4), nm, v.p->isCapital() ? bigLabel : labelColor);
            }
        }
    }

    // Rótulos de região por nível de zoom (continente -> país -> estado).
    QFont f = font();
    if (lvlContinents) {
        f.setPointSize(13);
        f.setBold(true);
        p.setFont(f);
        const QFontMetrics fm(f);
        for (const Continent& c : kContinents) {
            bool vis;
            const QPointF s = projectGlobe(c.lon, c.lat, &vis);
            if (!vis) continue;
            const QString name = QString::fromUtf8(c.name);
            haloText(s - QPointF(fm.horizontalAdvance(name) / 2.0, 0), name, bigLabel);
        }
    } else if (lvlCountries) {
        f.setPointSize(9);
        f.setBold(true);
        p.setFont(f);
        const QFontMetrics fm(f);
        QVector<const GeoData::Country*> cands;
        for (const GeoData::Country& c : geo.countries()) cands.append(&c);
        std::sort(cands.begin(), cands.end(), [](const GeoData::Country* a, const GeoData::Country* b) {
            return a->bounds.width() * a->bounds.height() > b->bounds.width() * b->bounds.height();
        });
        QVector<QRectF> occ;
        for (const GeoData::Country* c : cands) {
            const double lon = (c->labelLon != 0.0 || c->labelLat != 0.0) ? c->labelLon : c->bounds.center().x();
            const double lat = (c->labelLon != 0.0 || c->labelLat != 0.0) ? c->labelLat : c->bounds.center().y();
            bool vis;
            const QPointF s = projectGlobe(lon, lat, &vis);
            if (!vis) continue;
            const QPointF at = s - QPointF(fm.horizontalAdvance(c->name) / 2.0, 0);
            const QRectF lr(at.x(), at.y() - fm.height() + 4, fm.horizontalAdvance(c->name) + 4, fm.height());
            bool col = false;
            for (const QRectF& o : occ) if (o.intersects(lr)) { col = true; break; }
            if (col) continue;
            occ.append(lr);
            haloText(at, c->name, bigLabel);
        }
    } else if (lvlStates) {
        f.setPointSize(8);
        f.setBold(true);
        p.setFont(f);
        const QFontMetrics fm(f);
        for (const GeoData::State& st : geo.states()) {
            if (st.bounds.width() * pxPerDeg < 45.0) continue;
            bool vis;
            const QPointF s = projectGlobe(st.bounds.center().x(), st.bounds.center().y(), &vis);
            if (!vis) continue;
            haloText(s - QPointF(fm.horizontalAdvance(st.name) / 2.0, 0), st.name, labelColor);
        }
    }

    if (m_pins) {
        QFont pf = font();
        pf.setPointSize(9);
        pf.setBold(true);
        p.setFont(pf);
        for (const MapPinsStore::Pin& pin : m_pins->pins()) {
            bool vis;
            const QPointF s = projectGlobe(pin.lon, pin.lat, &vis);
            if (!vis) continue;
            p.setPen(QPen(haloColor, 1.0));
            p.setBrush(pinColor);
            p.drawEllipse(s + QPointF(0, -6), 6, 6);
            QPolygonF tip;
            tip << s + QPointF(-3.5, -3) << s + QPointF(3.5, -3) << s;
            p.drawPolygon(tip);
            p.setPen(Qt::NoPen);
            p.setBrush(haloColor);
            p.drawEllipse(s + QPointF(0, -6), 2, 2);
            if (!pin.label.isEmpty()) haloText(s + QPointF(9, -4), pin.label, labelColor);
        }
    }

    if (m_hoverState >= 0 || m_hoverCountry >= 0) {
        QString name;
        if (m_hoverState >= 0 && m_hoverState < geo.states().size())
            name = geo.states().at(m_hoverState).name;
        else if (m_hoverCountry >= 0 && m_hoverCountry < geo.countries().size())
            name = geo.countries().at(m_hoverCountry).name;
        if (!name.isEmpty()) {
            QFont tf = font();
            tf.setPointSize(9);
            tf.setBold(true);
            p.setFont(tf);
            const QFontMetrics tfm(tf);
            const qreal tw = tfm.horizontalAdvance(name) + 14, th = tfm.height() + 6;
            QRectF box(m_hoverPos.x() + 14, m_hoverPos.y() - th - 6, tw, th);
            if (box.right() > width() - 2) box.moveLeft(m_hoverPos.x() - tw - 8);
            if (box.top() < 2) box.moveTop(m_hoverPos.y() + 10);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(18, 18, 20, 215));
            p.drawRoundedRect(box, 5, 5);
            p.setPen(QColor(0xff, 0xff, 0xff));
            p.drawText(box, Qt::AlignCenter, name);
        }
    }
}

void MapView::wheelEvent(QWheelEvent* event)
{
    if (m_globe) {
        if (m_flyAnim) m_flyAnim->stop(); // zoom manual cancela o "voo"
        const double f = (event->angleDelta().y() > 0) ? 1.12 : 1.0 / 1.12;
        // Teto alto o bastante pra alcançar as cidades menores (r >= 28 no
        // globeDetailRatio, ~zoom 11), igual ao alcance do mapa plano.
        m_globeZoom = std::clamp(m_globeZoom * f, 0.6, 12.0);
        update();
        return;
    }

    const double factor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
    const QPointF cursor = event->position();
    const QPointF worldUnder = (cursor - m_offset) / m_scale;

    double newScale = m_scale * factor;
    // Mínimo = enquadramento (sem zoom out exagerado); máximo = ~40x.
    newScale = std::clamp(newScale, m_fitScale, m_fitScale * 40.0);
    m_offset = cursor - worldUnder * newScale;
    m_scale = newScale;
    update();
}

void MapView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_flyAnim) m_flyAnim->stop(); // arrastar cancela o "voo"
        m_dragging = true;
        m_lastMouse = event->pos();
        m_pressPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void MapView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        if (m_globe) {
            const double k = 0.25 / m_globeZoom;
            m_globeLon0 -= (event->pos().x() - m_lastMouse.x()) * k;
            while (m_globeLon0 > 180.0)  m_globeLon0 -= 360.0; // mantém em faixa
            while (m_globeLon0 < -180.0) m_globeLon0 += 360.0;
            m_globeLat0 = std::clamp(m_globeLat0 + (event->pos().y() - m_lastMouse.y()) * k, -89.0, 89.0);
            m_lastMouse = event->pos();
            update();
        } else {
            m_offset += QPointF(event->pos() - m_lastMouse);
            m_lastMouse = event->pos();
            update();
        }
        return;
    }

    // Hover no globo: estado (em zoom) ou país sob o cursor, via projeção inversa.
    if (m_globe) {
        bool ok;
        const QPointF gl = invGlobe(QPointF(event->pos()), &ok);
        const double r = globeDetailRatio();
        const int hs = (ok && r >= 4.5) ? stateAt(gl) : -1;
        const int hc = (ok && hs < 0) ? countryAt(gl) : -1;
        const bool had = (m_hoverState >= 0 || m_hoverCountry >= 0);
        m_hoverState = hs;
        m_hoverCountry = hc;
        m_hoverPos = event->pos();
        if (had || hs >= 0 || hc >= 0) update();
        return;
    }

    // Hover ao vivo: destaca o estado (em zoom) ou o país sob o cursor.
    const QPointF ll = screenToLonLat(QPointF(event->pos()));
    const double r = (m_fitScale > 0.0) ? m_scale / m_fitScale : 1.0;
    const int hs = (r >= 4.5) ? stateAt(ll) : -1;
    const int hc = (hs < 0) ? countryAt(ll) : -1;
    const bool had = (m_hoverState >= 0 || m_hoverCountry >= 0);
    m_hoverState = hs;
    m_hoverCountry = hc;
    m_hoverPos = event->pos();
    if (had || hs >= 0 || hc >= 0) update();
}

void MapView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor((m_ruler || m_addPin) ? Qt::CrossCursor : Qt::OpenHandCursor);
        if ((event->pos() - m_pressPos).manhattanLength() >= 5) return; // foi arrasto

        // No globo: pin existente > fixar pin > país.
        if (m_globe) {
            bool ok;
            const QPointF gl = invGlobe(QPointF(event->pos()), &ok);
            if (!ok) return;
            if (m_addPin) { emit pinRequestedAt(gl.x(), gl.y()); return; }
            if (m_pins) {
                const MapPinsStore::Pin* best = nullptr;
                double bd = 12.0;
                for (const MapPinsStore::Pin& pin : m_pins->pins()) {
                    bool vis;
                    const QPointF s = projectGlobe(pin.lon, pin.lat, &vis);
                    if (!vis) continue;
                    const double d = QLineF(s, QPointF(event->pos())).length();
                    if (d < bd) { bd = d; best = &pin; }
                }
                if (best) { emit pinClicked(best->id); return; }
            }
            // Cidade > estado > país, igual ao mapa plano.
            pickFeatureAt(gl, event->pos(), globeDetailRatio(),
                          [this](double lon, double lat, bool* vis) {
                              return projectGlobe(lon, lat, vis);
                          });
            return;
        }

        // Modo "fixar pin": clique cria um pin no ponto.
        if (m_addPin) {
            const QPointF p = screenToLonLat(QPointF(event->pos()));
            emit pinRequestedAt(p.x(), p.y());
            return;
        }

        if (!m_ruler) {
            // Pin existente tem prioridade sobre a feição geográfica.
            if (m_pins) {
                const MapPinsStore::Pin* best = nullptr;
                double bd = 12.0;
                for (const MapPinsStore::Pin& pin : m_pins->pins()) {
                    const double d = QLineF(project(pin.lon, pin.lat),
                                            QPointF(event->pos())).length();
                    if (d < bd) { bd = d; best = &pin; }
                }
                if (best) { emit pinClicked(best->id); return; }
            }
            hitTest(event->pos()); // clique normal: identifica a feição
            return;
        }

        // Modo régua: marca A, depois B, e mede.
        const QPointF ll = screenToLonLat(QPointF(event->pos()));
        if (m_rulerCount == 0 || m_rulerCount == 2) {
            m_rulerA = ll;
            m_rulerCount = 1;
            update();
        } else {
            m_rulerB = ll;
            m_rulerCount = 2;
            const double km = haversineKm(m_rulerA.y(), m_rulerA.x(), m_rulerB.y(), m_rulerB.x());
            const double dh = (utcOffsetSec(m_rulerB.x(), m_rulerB.y())
                             - utcOffsetSec(m_rulerA.x(), m_rulerA.y())) / 3600.0;
            QStringList det;
            det << tr("Distância: %1 km").arg(fmtNum(qRound64(km)));
            if (qAbs(dh) < 0.01)
                det << tr("Mesmo fuso horário");
            else
                det << tr("Diferença de fuso: %1%2 h")
                        .arg(dh > 0 ? QStringLiteral("+") : QString())
                        .arg(QString::number(dh, 'g', 3));
            emit featureClicked(tr("Medição"), tr("Régua"), det, QString());
            update();
        }
    }
}
