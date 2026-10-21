#pragma once

// Edição do terreno (porte do Map Lab): pincéis que puxam um campo rumo a um
// alvo (repetir converge em vez de empilhar bolha), a ferramenta Área, e a
// foto de um retângulo dos campos pra desfazer a pincelada sem clonar o mundo.

#include "CartoCore.h"

#include <array>
#include <optional>

namespace carto {

// plano da mesa: o mundo girado em volta do centro (p = R(ang)·(q − centro))
std::array<double, 2> planeToWorld(const World& w, double ang, double px, double py);
std::array<double, 2> worldToPlane(const World& w, double ang, double qx, double qy);

enum class Brush { Land, Sea, Mountain, Smooth, River, Wet, Dry, Cold };
struct DabState { double r = 36; double strength = 0.5; bool inverse = false; double riverLen = 0; };
// aplica uma carimbada em (x, y) do mundo; devolve a caixa alterada [x0, y0, x1, y1)
std::optional<std::array<int, 4>> applyDab(World& w, Brush tool, double x, double y, const DabState& st);
double riverOffset(const World& w, double len, double r);
// ruído das bordas dos pincéis (torce o círculo pra não sair carimbo redondo)
double brushNoise(const World& w, double x, double y, double salt);

// área: gera uma ilha num retângulo do plano e funde no mundo (terra nova sobe, a antiga fica)
struct PlaneRect { double x, y, w, h; };
bool addArea(World& w, const PlaneRect& rect, double ang, int seed);

struct Snapshot { int x0 = 0, y0 = 0, x1 = 0, y1 = 0; std::vector<float> h, m, k; };
Snapshot snapshotRect(const World& w, const std::vector<float>& h, const std::vector<float>& m, const std::vector<float>& k,
                      int x0, int y0, int x1, int y1);
void applySnapshot(World& w, const Snapshot& s);

} // namespace carto
