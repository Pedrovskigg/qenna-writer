#pragma once

// Modelos 3D das construções e naves, e o renderizador em software que desenha eles
// (porte de models3d.js). Geometria real, com profundidade, sem GPU e sem biblioteca.

#include "GalaxyCore.h"

#include <QImage>
#include <QString>

#include <functional>

class QPainter;

namespace galaxy {

struct MeshData {
    std::vector<float> V;      // x y z nx ny nz u v
    std::vector<uint32_t> T;   // a b c material
    double rad = 1;
    int nv = 0, nt = 0;
};

// Rasteriza a malha num buffer RGBA S×S. rot = giro em volta do eixo z; pitch = 0 de cima, π/2 de lado.
void rasterize(const MeshData& mesh, int S, double rot, double pitch, const V3& L, uint8_t* out, uint32_t seed);
const MeshData& meshOf(const std::string& type, uint32_t seed, int lod);
bool hasModel(const std::string& type);

inline double clampPitch(double p) { return p < -1.45 ? -1.45 : p > 1.45 ? 1.45 : p; }
void beginFrame(double ms);
// A vista de arte troca a cor do modelo pelo traço dela, já no tamanho final (buffer RGBA D×D).
void setTone(std::function<void(uint8_t*, int, int)> fn, const QString& id);
// key: quem é a peça no mapa (cada uma tem o próprio quadro guardado).
void drawModel(QPainter& p, const std::string& type, uint32_t seed, double x, double y, double r, double rot, double pitch, double lx, double ly, double alpha, double dpr, const QString& key = QString());
// Onde um ponto do modelo cai na tela, pra pôr brilho animado em cima dele.
void localToScreen(double px, double py, double pz, double rot, double pitch, double rad, double& ox, double& oy, double& oz);
double radOf(const std::string& type, uint32_t seed);
// A nave do tráfego: já vem desenhada pra direção em que está indo.
const QImage& shipImage(int kind, int D, double heading, double pitch);
// Um quadro avulso, pra miniatura e pro 3D do painel.
QImage renderModel(const std::string& type, uint32_t seed, int D, double rot, double pitch, const V3& L);
void clearModelCaches();

} // namespace galaxy
