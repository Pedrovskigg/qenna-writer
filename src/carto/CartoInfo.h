#pragma once

// Camada de informações do mapa (porte do Map Lab, core.js): reinos, cidades e
// estradas sugeridos pelo terreno, territórios editáveis, o que o usuário
// constrói (castelos, vilas, muralhas, estradas, pontes) e a vegetação.
// O terreno decide: o território cresce pelo custo de caminhar (serra custa
// caro, então a fronteira cai nas cristas) e a estrada procura o vale.

#include "CartoPaint.h"

#include <QJsonObject>
#include <QString>

#include <array>
#include <memory>
#include <set>
#include <vector>

namespace carto {

using Pt = std::array<double, 2>;
using Path = std::vector<Pt>;

// grade de custo de caminhar (1 célula = `step` px do mundo)
struct CostGrid {
    int gw = 0, gh = 0;
    std::vector<float> cost;
    std::vector<uint8_t> land;
};
struct Tree {
    std::vector<double> dist;
    std::vector<int> prev;
};
Tree dijkstra(int gw, int gh, const std::vector<float>& cost, const std::vector<int>& src);
int infoStep(const World& w);
double heightAt(const World& w, double x, double y);   // altura no px mais perto
Path smoothPath(Path p);                               // Chaikin, 2 passadas
Path resample(const Path& pts, double step);

// ── o que o terreno sugere (cidades e estradas de teste) ──
struct Town { double x = 0, y = 0; int realm = -1; bool capital = false; QString name; };
struct Building {
    bool castle = false;
    int size = 1;                 // castelo: 0 pequeno · 1 médio · 2 grande · 3 colossal
    double x = 0, y = 0, yaw = 0, v = 0;
    int realm = -1, town = -1;
    int floors = 1;
    bool timber = false;
    mutable int snowy = -1;       // −1 = ainda não olhou o clima
};
struct Field { double x = 0, y = 0, w = 0, d = 0, yaw = 0; int crop = 0; };
struct Dock { double x0 = 0, y0 = 0, x1 = 0, y1 = 0; };
struct Boat { double x = 0, y = 0, yaw = 0, v = 0; };

// ── territórios editáveis ──
constexpr double kReachAll = 1e8, kFar = 1e9;
extern const RGB kRealmPalette[8];
struct RealmDef {
    QString name;
    RGB color{};
    std::vector<Pt> seeds;
    double reach = kReachAll;
    std::set<int> paint;          // células pintadas (também viram semente)
    QString groupId, placeId;     // Explorar: o grupo de documentos e o Lugar do Criador de Mundos
};
struct Realm { QString name; RGB color{}; double x = 0, y = 0; int area = 0; };
struct RealmState { std::vector<RealmDef> defs; std::vector<uint8_t> barrier; };

// ── o que o usuário constrói ──
struct Art {
    QString id, kind;             // castle · tower · lighthouse · windmill · temple · ruins · e os de artSpecs()
    double x = 0, y = 0, yaw = 0, v = 0;
    int size = 0;
    QString style;                // castelo: "" (humano) · elven · dwarf · orc
    QString name;
    int realm = -1;
};
struct House { double x = 0, y = 0, yaw = 0, v = 0; int floors = 1; bool timber = false; mutable int snowy = -1; };
struct Wall { Path pts; };
struct Road { Path pts; QString kind = QStringLiteral("road"); };   // trail · road · royal
struct UserLayer {
    std::vector<Art> arts;
    std::vector<House> houses;
    std::vector<Wall> walls;
    std::vector<Field> fields;
    std::vector<Path> bridges;
    std::vector<Road> roads;
    bool showGen = true;          // mostra as cidades e estradas sugeridas
    int seq = 0, ver = 0;
    double scale = 1;
    QJsonObject toJson() const;
    static UserLayer fromJson(const QJsonObject& o);
};
QString artName(const QString& kind);   // "Castelo", "Torre"...
// artefatos das levas novas (estátua em diante): tamanho, onde vão e pra onde nascem virados
struct ArtSpec {
    const char* kind;
    double r, h;                  // raio no chão e altura (px do mundo, escala 1)
    bool water, named;            // só no mar · (named: sem uso desde 23/10, nada nasce com nome)
    char face;                    // 'f' de frente pra quem olha · 't' três quartos de frente · 's' de lado · 0 livre
};
const std::vector<ArtSpec>& artSpecs();
const ArtSpec* artSpec(const QString& kind);
double faceYaw(const ArtSpec& s, double v, double viewAng, double yaw);   // o giro de nascença, virado pra câmera (livre: fica yaw)

class Info : public RealmPaint {
public:
    int step = 4, gw = 0, gh = 0;
    std::vector<Town> towns;
    std::vector<Path> roads;
    std::vector<Realm> realms;
    std::vector<Building> buildings;
    std::vector<Path> bridges;
    std::vector<Field> fields;
    std::vector<Dock> docks;
    std::vector<Boat> boats;
    CostGrid realmGrid, roadGrid;
    std::vector<uint8_t> barrier;
    std::vector<RealmDef> defs;
    std::vector<std::vector<double>> dist;
    std::vector<double> reach;
    std::vector<int16_t> owner;
    UserLayer user;

    RealmSample realmAt(double qx, double qy) const override;
    RGB realmColor(int id) const override;
    int cellOf(double x, double y) const;
    RealmState realmState() const { return { defs, barrier }; }
};

// count = quantos reinos sugerir (3; 5 em mapa grande)
std::unique_ptr<Info> buildInfo(World& w, int count);
void recomputeRealms(const World& w, Info& info);
void refreshOwners(Info& info);   // só quem é dono de cada cidade, castelo e artefato (sem refazer os reinos)
void setRealmState(const World& w, Info& info, const RealmState& st);
// mode: claim (pro território `realm`) ou neutral (barreira)
bool paintTerritory(const World& w, Info& info, double x, double y, double r, bool neutral, int realm);
QJsonObject realmsToJson(const Info& info);
void realmsFromJson(Info& info, const QJsonObject& o);

// construir
Art& addArt(const World& w, Info& info, const QString& kind, double x, double y, int size);
bool paintVillage(const World& w, Info& info, double x, double y, double r);
bool paintFieldsUser(const World& w, Info& info, double x, double y, double r);
bool addWall(Info& info, const Path& pts);
bool addRoad(Info& info, const Path& pts, const QString& kind, bool smooth);
bool addBridge(Info& info, Pt a, Pt b);
bool eraseUser(Info& info, double x, double y, double r);
double artRadius(const Art& a);         // raio no chão (px do mundo, escala 1)

// estrada "pelo vale": Dijkstra a partir de um ponto; pathTo devolve o caminho até outro
class Router {
public:
    Router(const World& w, Info& info, double x, double y);
    bool ok() const { return !m_tree.dist.empty(); }
    double x() const { return m_x; }
    double y() const { return m_y; }
    bool pathTo(double tx, double ty, Path& out) const;
    const Info* info() const { return m_info; }
private:
    int nearLand(double px, double py) const;
    int cellOf(double px, double py) const;
    const Info* m_info = nullptr;
    int m_gw = 0, m_gh = 0, m_step = 4, m_src = -1;
    std::vector<uint8_t> m_land;
    Tree m_tree;
    double m_x = 0, m_y = 0;
};
// todas as estradas do usuário com trecho na água viram ponte (de margem a margem)
std::vector<Path> roadBridges(const World& w, const UserLayer& U);

// ── vegetação ──
enum FloraType { Broad = 0, Pine, SnowPine, Palm, Cactus, Shrub, Rock, Reed, Dead };
struct Flora { double x, y; int type; double v, s; int b; };
std::vector<Flora> buildFlora(World& w);
// depois de uma pincelada: cada planta só depende do terreno do próprio lugar, então basta refazer as do retângulo
void updateFloraBox(World& w, std::vector<Flora>& flora, int x0, int y0, int x1, int y1);

} // namespace carto
