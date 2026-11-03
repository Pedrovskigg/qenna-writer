#pragma once

// O que a civilização constrói no espaço: o catálogo do Construir e os colossos do tamanho
// de uma órbita (porte de structures.js). Os modelos das peças menores ficam em GalaxyModels.

#include "GalaxyCore.h"

#include <QColor>
#include <QString>

#include <map>
#include <string>
#include <vector>

class QPainter;

namespace galaxy {

enum class Host { Any, Star, Planet };

struct StructDef {
    const char* name;
    const char* pre;
    const char* desc;
    double size = 0;
    Host host = Host::Any;
    double spin = 0;
    bool deep = false, sizes = true, mega = false, one = false, orbit = false;
};
const std::map<std::string, StructDef>& structs();
const StructDef* structOf(const std::string& id);

struct BodyDef { const char* name; const char* desc; };
const BodyDef* bodyDef(const std::string& id);

enum class ItemKind { Star, Planet, Moon, Comet, Belt, Struct };
struct CatalogItem { std::string id; ItemKind kind; const char* name; bool art = false; bool deepOnly = false; };
struct CatalogCat { std::string id; const char* name; std::string icon; std::vector<CatalogItem> items; };
const std::vector<CatalogCat>& catalog();

// O que se mexe em cima do modelo: a energia do portal, o farol piscando, as fagulhas da obra, os motores.
void drawAnimated(QPainter& p, const std::string& type, uint32_t seed, double x, double y, double r, double rot, double pitch, double t, double alpha = 1, double flash = 0);
// part: 'b' antes da estrela, 'f' depois.
void drawDyson(QPainter& p, double x, double y, double R, double cT, uint32_t seed, double t, char part, double alpha = 1);
void drawSwarm(QPainter& p, double x, double y, double R, double cT, uint32_t seed, double t, double alpha = 1);
void drawRingworld(QPainter& p, double x, double y, double R, double H, double P, uint32_t seed, double t, char part, double alpha = 1);
// part: 'b' antes do planeta, 's' a sombra no chão do planeta, 'f' depois dele.
void drawOrbitalRing(QPainter& p, double x, double y, double R, double P, uint32_t seed, double t, char part, double alpha, double dpr, double pr, double lightAng);
void drawElevator(QPainter& p, double x, double y, double pr, double ang, double H, double cT, double t, double alpha, double dpr, const QString& id);
void drawSiphon(QPainter& p, double x, double y, double R, double cT, uint32_t seed, double t, const std::array<int, 3>& starC, double alpha, double dpr, double yaw);

} // namespace galaxy
