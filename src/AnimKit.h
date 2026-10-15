#pragma once

// Ferramentas das levas 2 a 4 dos temas Animados (portadas do Weather Lab,
// leva2*.js, leva3*.js, leva4*.js e people.js). Só os arquivos novos incluem.

#include "AnimBackgroundEngines.h"

#include <QLineF>
#include <QPainterPath>
#include <QPen>
#include <QVector>

#include <vector>

namespace AnimBg {
namespace kit {

using namespace fx;

inline double lerp(double a, double b, double k) { return a + (b - a) * k; }
QPen pen(const QColor& c, double w);                          // ponta e junção redondas
void disc(QPainter& p, double x, double y, double r, const QColor& c);
void glow(QPainter& p, double x, double y, double r, const QColor& c, double a);   // brilho radial (já em Plus, se o chamador quiser)

// Perfil irregular (deslocamento do ponto médio): at(u), u em 0..1, mais ou menos entre -1 e 1.
struct Profile {
    std::vector<double> pts;
    double at(double u) const;
};
Profile profile(int it = 8, double rough = 0.55);
void fillProfile(QPainter& p, const Ctx& c, const Profile& f, double base, double amp, const QColor& col, double step = 6);
// Céu de estrelas paradas, numa imagem do tamanho da mesa.
QPixmap starLayer(const Ctx& c, int n, double yMax, double aMax = 0.6);
const QPixmap& smokeDot();

// As cenas foram desenhadas no Lab com a mesa da esquerda entre x 45 e 410:
// zx() leva essa coordenada pro pedaço de mesa à vista; rx() ancora na direita.
double zx(const Ctx& c, double labX);
inline double rx(const Ctx& c, double labX) { return labX - 1440 + c.W; }

// Cache que se refaz quando a resolução física muda (ou quando a chave muda).
struct Cache {
    QPixmap pm;
    double px = 0;
    QString key;
    bool valid(const Ctx& c, const QString& k = QString()) const { return !pm.isNull() && qAbs(px - c.px) < 0.01 && key == k; }
    void set(const Ctx& c, const QPixmap& p, const QString& k = QString()) { pm = p; px = c.px; key = k; }
    void reset() { pm = QPixmap(); }
};

// Muitas linhas da mesma cor com transparências diferentes: agrupa por nível
// de alfa e desenha cada grupo de uma vez (drawLines é bem mais rápido).
struct LineBatch {
    static constexpr int N = 10;
    QVector<QLineF> b[N];
    void add(double x0, double y0, double x1, double y1, double a)
    {
        const int i = qBound(0, int(a * N), N - 1);
        if (a > 0.004) b[i].append(QLineF(x0, y0, x1, y1));
    }
    void flush(QPainter& p, const QColor& col, double w)
    {
        for (int i = 0; i < N; ++i) {
            if (b[i].isEmpty()) continue;
            p.setPen(pen(alpha(col, (i + 0.5) / N), w));
            p.drawLines(b[i]);
            b[i].clear();
        }
    }
};

// ---- gente: silhueta com proporção (não boneco de placa) ----
struct Fig {
    enum Pose { Stand, Walk, Sit, Bust } pose = Stand;
    enum Hair { Short, Long, Bun, Pony, Curly, Cap, Hat } hair = Short;
    enum Arm { Down, Up, Umbrella, Cup, Hold, Counter, Oar } arm = Down;
    double ph = 0, lift = 0, st = 0, build = 1, lean = 0, tilt = 0;
    int side = 1;
    bool skirt = false;
    QColor col = QColor(10, 10, 14);
    QColor stripes, band;   // inválidas = sem listra / sem fita
};
// x,y = pé no chão (no Sit, o assento; no Bust, a base do recorte); h = altura em pé.
void figure(QPainter& p, double x, double y, double h, const Fig& o);
QPointF figureHand(double x, double y, double h, const Fig& o);
Fig::Hair hairAt(int i);

} // namespace kit

// Fábricas das levas novas
std::unique_ptr<Layer> makeLeva2a(const QString& scene);
std::unique_ptr<Layer> makeLeva2b(const QString& scene);
std::unique_ptr<Layer> makeLeva3(const QString& scene);
std::unique_ptr<Layer> makeLeva4(const QString& scene);
std::unique_ptr<Layer> makeLeva5(const QString& scene);
// Cidade com volume (Paulista, Night Train, Harbor), em AnimCityDepth.cpp
std::unique_ptr<Layer> makeCityDepth(bool towers, bool helis, bool train, bool water);
// Neon District com volume e hologramas, em AnimNeonDistrict.cpp
std::unique_ptr<Layer> makeNeonDistrict();
// Natureza com volume (farol, charneca, campina, céu estrelado), em AnimNature.cpp
std::unique_ptr<Layer> makeNature(const QString& scene);

} // namespace AnimBg
