#pragma once

// Criação Livre: a forma orgânica dos territórios e onde nasce cada sistema.
// A forma é a mesma do mapa (bolha em volta de cada sistema, corredores pela árvore mínima e a forma pintada, tudo
// fundido com suavidade), com um recorte de litoral por cima: ruído com dobra e a galáxia desfocada empurrando a
// fronteira pros braços e tirando do vazio. Quem usa: o minimapa, o pincel de território e a "Forma no mapa".
// O mapa principal do Galaxy Maps continua com as bordas de sempre (GalaxyCanvasRegions).

#include <QColor>
#include <QImage>
#include <QSet>

#include <array>
#include <cstdint>
#include <vector>

namespace galaxy {

class Doc;
struct Region;

struct TerrField {
    struct Link { double x, y, dx, dy, l; };
    std::vector<std::array<double, 2>> pts;      // os sistemas (e mundos soltos) do território
    std::vector<Link> links;                      // corredores entre eles
    std::vector<std::array<double, 3>> stamps;    // a forma pintada
    double sd = 0;                                // semente do recorte (cada território tem o seu)
    bool organic = true;
};

// A forma de um território. withSolo: os mundos soltos dele também dão forma (no minimapa eles aparecem).
TerrField terrField(const Doc& d, const Region& r, bool withSolo = true);
// Só a forma pintada (pra medir a folga da borda ao espalhar os sistemas).
TerrField stampField(const Region& r);
// Distância até a fronteira, no espaço do mundo (negativa dentro).
double terrDist(const TerrField& F, double x, double y);
bool terrBox(const TerrField& F, double& x0, double& y0, double& x1, double& y1);
bool terrCenter(const TerrField& F, double& x, double& y);
bool terrEmpty(const TerrField& F);
// O brilho da galáxia desfocada (0 a 1) nesse ponto do mundo.
double galSoft(const Doc& d, double x, double y);

// O território na tela: linha fina da fronteira, cor forte na borda que some pro miolo e um brilho leve por fora.
// A: onde caem na tela (pixels da imagem) os pontos (0,0), (1,0) e (0,1) do chão da galáxia: {ox, oy, exx, exy, eyx, eyy}.
// zpx: pixels por unidade do mundo. busy: câmera andando (grade mais larga, mais barato).
struct TerrImage { QImage img; int x = 0, y = 0; bool empty = true; };
TerrImage terrImage(const Doc& d, const TerrField& F, const QColor& color, const double A[6], int W, int H, double zpx, double dpr, bool busy);

// Onde nasce um sistema novo. No território: espalhado pela forma inteira, no ponto mais longe de quem já está lá,
// com folga da borda. Solto: num lugar com estrelas, longe dos outros e fora de qualquer território.
// ignore: sistemas que não contam como vizinhos (os que estão sendo redistribuídos).
bool spreadSpot(const Doc& d, const Region* r, double& x, double& y, uint32_t seed, const QSet<int>& ignore = {});

} // namespace galaxy
