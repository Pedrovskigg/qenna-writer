#pragma once

// Sprites do mapa (porte do sprites.js do Map Lab): tudo é 3D simples projetado
// pela câmera do mapa (giro + inclinação).
//  · vegetação e pedras são redondas: só a inclinação importa, então cada desenho
//    é guardado pronto por (tipo, variação, inclinação, escala) e carimbado;
//  · construções, pontes, barcos e plantações têm frente e lado: sólidos com
//    faces, projetados e sombreados a cada quadro (giram junto com a mesa),
//    com detalhe desenhado em cima de cada face (janela, telha, ameia, sulco).

#include "CartoCore.h"
#include "CartoInfo.h"

#include <QImage>
#include <QPointF>
#include <QStringList>

#include <functional>

class QPainter;

namespace carto::spr {

// câmera de um objeto: proj(x, y, z do mundo) → tela; edges/detail pelo zoom
struct Cam {
    double ang = 0, co = 1, si = 0;
    bool edges = false, detail = false;
    std::function<QPointF(double, double, double)> proj;
    // altura do chão em (x, y), no sistema do sprite, em relação ao pé dele (o castelo no Penhasco assenta no relevo);
    // vazio = chão plano (miniatura do catálogo)
    std::function<double(double, double)> gz;
};

struct CastleCfg {
    double R, wallH; bool mid; double towerR, towerH, keep, keepH, kt[3]; bool turrets;
    bool hasInner; double iR, iWallH, iTowerR, iTowerH;
};
extern const CastleCfg kCastle[4];

void setSun(const std::array<double, 3>& L);   // luz das faces (vetor normalizado)
void resetSun();

struct FloraImg { QImage img; double ax = 0, ay = 0, ppw = 1; };
const FloraImg& floraImage(int type, double v, double pitchDeg, double ppw, bool mirror);
const QImage& glowImage(const QString& kind);   // window · torch · beacon · lantern · arcane · fairy · fire · ghost · holy · eye

void drawHouse(QPainter& p, double x, double y, double yaw, double v, int floors, bool timber, const Cam& cam, bool snowy);
void drawCastle(QPainter& p, double x, double y, double yaw, int size, const Cam& cam, const RGB& realmCol);
void drawBridge(QPainter& p, const Path& pts, const Cam& cam);
void drawDock(QPainter& p, const Dock& dk, const Cam& cam);
void drawBoat(QPainter& p, const Boat& b, const Cam& cam);
void drawField(QPainter& p, const Field& f, const Cam& cam);
void drawTowerArt(QPainter& p, const Art& a, const Cam& cam, const RGB& realmCol);
void drawLighthouse(QPainter& p, const Art& a, const Cam& cam);
void drawWindmill(QPainter& p, const Art& a, const Cam& cam);
void drawTemple(QPainter& p, const Art& a, const Cam& cam);
void drawRuins(QPainter& p, const Art& a, const Cam& cam);
void drawWallSeg(QPainter& p, const Pt& a, const Pt& b, const Cam& cam, double k);
void drawWallTower(QPainter& p, const Pt& at, const Cam& cam, double k);
void atlasGlyph(QPainter& p, int type, double X, double Y, double k);
// levas novas (CartoSpritesArts.cpp): estátua, navio, dragão... e os castelos élfico, anão e orc;
// devolve false se o artefato não é delas (aí vale o desenho de sempre)
bool drawArt(QPainter& p, const Art& a, const Cam& cam, const RGB& realmCol);
struct ArtLight { double lx, ly, z, r; const char* kind; double force; };
const std::vector<ArtLight>& artLights(const QString& kind);   // luzes de noite, no sistema do artefato (antes da escala)
// pal (opcional): a tinta e o papel da vista (Pergaminho, Carta, Topográfico, Aquarela, Planta); paperA = o quanto o recheio cobre
struct InkPal { RGB ink, paper; double paperA = 1, inkA = 1; };
void inkGlyph(QPainter& p, int type, double X, double Y, double k, double v, const InkPal* pal = nullptr);
// leva 4 (CartoSpritesArts4.cpp): casa de cidade (2 e 3 andares, campanário), paliçada, os castelos novos
// (humanos: normando, com fosso, château, penhasco; e oriental, deserto, sombrio, gelo) e os monumentos grandes
void drawCityHouse(QPainter& p, const House& h, const Cam& cam, bool snowy);
void drawPalisadeSeg(QPainter& p, const Pt& a, const Pt& b, const Cam& cam, double k);
void drawPalisadeTower(QPainter& p, const Pt& at, const Cam& cam, double k);
bool drawArt4(QPainter& p, const Art& a, const Cam& cam, const RGB& realmCol);
// o fosso do castelo "com fosso": meio-lados da ilha (Rx, Ry) e a largura da água (M), já na escala k do artefato
struct MoatGeom { double Rx, Ry, M; };
MoatGeom moatGeom(int size, double k);
// tochas da noite: o castelo do Penhasco acende em volta da muralha dele, não da de sempre (false = a de sempre)
bool castleLightRing(const Art& a, double& R, double& z);
// estilos de castelo, na ordem do catálogo
const QStringList& castleStyles();

} // namespace carto::spr
