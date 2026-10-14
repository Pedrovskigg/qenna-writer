#pragma once

// Peças internas do AnimBackground: o contexto da cena, a paleta tirada do
// tema e as ferramentas de desenho que todas as camadas usam. Só
// AnimBackground.cpp e AnimBackgroundCity.cpp incluem isto.

#include "AnimBackground.h"

#include <QColor>
#include <QPainter>
#include <QPixmap>
#include <QPointF>
#include <QRandomGenerator>
#include <QString>

#include <cmath>
#include <memory>

namespace AnimBg {

// Cores do tema que a mesa usa.
struct Pal {
    bool dark = true;
    QColor app, page, pageInk, ink, acc, green;
    QColor part;  // cor das partículas (chuva, névoa no pé da mesa)
};

struct Ctx {
    double W = 1440;            // largura da cena (a altura é sempre kSceneH)
    double H = kSceneH;
    Zones zones;
    int intensity = 1;
    QString tod = QStringLiteral("theme");
    bool neon = false;          // chuva pega as cores do neon
    bool hasNeonCols = false;   // cyberpunk troca as cores do neon pelas da vinheta
    QColor neonA, neonB;
    double px = 1;              // pixels físicos por unidade da cena
    Pal pal;                    // a paleta, pra quem precisa dela já no init()
};

class Layer {
public:
    virtual ~Layer() = default;
    virtual void init(Ctx& c) = 0;
    virtual void step(double dt, Ctx& c) = 0;
    virtual void paint(QPainter& p, const Pal& pal, Ctx& c) = 0;
    // A janela mudou de tamanho: por padrão monta de novo.
    virtual void resized(Ctx& c) { init(c); }
};

namespace fx {

inline double rnd() { return QRandomGenerator::global()->generateDouble(); }
inline double R(double a, double b) { return a + rnd() * (b - a); }
inline int RI(int n) { return n <= 0 ? 0 : int(QRandomGenerator::global()->bounded(n)); }
template <class T, size_t N> const T& pick(const T (&a)[N]) { return a[RI(int(N))]; }

QColor hex(const char* s);
QColor mix(const QColor& a, const QColor& b, double k);
QColor alpha(QColor c, double a);
double luma(const QColor& c);
double smooth(double a, double b, double x);
double INT(const Ctx& c);                       // 0.45 / 1 / 1.8 conforme a intensidade
double LV(const Pal& p, const Ctx& c);          // claridade: 0 noite, 1 dia
double LVopen(const Pal& p, const Ctx& c);      // igual, mas "do tema" conta como noite
// Densidade das partículas acompanha a largura da mesa.
inline int count(double base, const Ctx& c) { return qMax(1, int(std::lround(base * INT(c) * c.W / 1440.0))); }

void vfill(QPainter& p, const QRectF& r, const QColor& a, const QColor& b, double y0, double y1);
void sky(QPainter& p, const Ctx& c, const QColor& top, const QColor& bottom);
// Céu conforme a hora escolhida; em "do tema" pinta top/bottom (ou nada, se inválidos).
void skyTOD(QPainter& p, const Pal& pal, const Ctx& c, const QColor& top = QColor(), const QColor& bottom = QColor());

// Cache em resolução física, desenhado em coordenadas da cena.
QPixmap layer(const Ctx& c, double w, double h);
QPixmap softDot(const QColor& col, double inner = 0.35);
inline void blit(QPainter& p, const QPixmap& pm, double x, double y, double w, double h)
{
    p.drawPixmap(QRectF(x, y, w, h), pm, QRectF(QPointF(0, 0), QSizeF(pm.size())));
}
// Ponto ao acaso num pedaço de mesa à vista.
QPointF visPoint(const Ctx& c);
// x de uma âncora (vela, farol, sol) dentro do pedaço esquerdo à vista.
double anchorX(const Ctx& c, double frac);
QColor hsl(double h, double s, double l, double a = 1.0);

} // namespace fx

// Fábricas (AnimBackgroundCity.cpp)
std::unique_ptr<Layer> makeCity();
std::unique_ptr<Layer> makeCityX(bool towers, bool helis, bool train, bool water);
std::unique_ptr<Layer> makeTown();
std::unique_ptr<Layer> makeHillside();
std::unique_ptr<Layer> makeCyberpunk();
std::unique_ptr<Layer> makeRetroGrid();
std::unique_ptr<Layer> makeNeonSign();

} // namespace AnimBg
