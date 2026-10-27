#pragma once

// Cena por cima do mapa (porte do scene.js do Map Lab): plantações, estradas,
// sprites (vegetação, casas, castelos, pontes, barcos), as luzes da noite e os
// nomes. Cada coisa pisa no chão pintado: o pintor guarda por pixel quanto o
// chão subiu e de que linha ele veio, então o que está atrás do morro some.

#include "CartoDoc.h"
#include "CartoSprites.h"

#include <QPointF>
#include <QRectF>
#include <QPainterPath>

#include <optional>

class QPainter;

namespace carto {

struct SceneState {
    Doc* doc = nullptr;
    const Geom* g = nullptr;      // a pintura atual (pode ser a prévia)
    const Aux* aux = nullptr;
    double ang = 0, pitch = 40, zoom = 1, panX = 0, panY = 0, fitZoom = 1;
    QString mode = QStringLiteral("full");
    double hour = kHourDefault;
    bool sprites = true;
    int vw = 0, vh = 0;
    // nomes
    bool namesAuto = true;
    QString density = QStringLiteral("normal");
    bool namesTool = false;
    QString selName;
    std::vector<QRectF> avoid;    // lugares ocupados (a Cartela no PNG): nome não senta ali
    double cull = 60;             // folga fora da tela onde ainda se desenha (a camada guardada desenha além da tela)
    bool quick = false;           // a mão está mexendo a vista: vale o carimbo de letra mais perto (o certo vem quando parar)
    // camada do chão em azulejos: planKey identifica a vista sem o arrasto (onde cada coisa cai na tela
    // fica guardado por ela) e clip é o azulejo, em coordenadas da tela de st: só entra o que encosta nele
    QString planKey;
    QString roadKey;              // o traçado das estradas na tela (não muda quando só um castelo entra)
    QRectF clip;
    bool info() const { return mode != QLatin1String("terrain") && doc && doc->info; }
};

struct Ground { double px, py, L; bool vis; double X, Y; };

class Scene {
public:
    std::optional<Ground> ground(const SceneState& st, double qx, double qy, double zTop = 0) const;
    spr::Cam cam(const SceneState& st, double L) const;                       // sem escala do mundo
    // encolhe em volta do centro do objeto (escala do mundo × sc, o tamanho dele); withGround: a câmera sabe a altura do chão (gz)
    spr::Cam camAt(const SceneState& st, double L, double ox, double oy, double sc = 1, bool withGround = false) const;
    // o morro na frente tampa a construção: a parte da tela onde ela ainda aparece (nada = nada tampa).
    // footR: raio do pé no chão; topH: altura (as duas no mundo); onRelief: assenta no relevo (Penhasco)
    std::optional<QPainterPath> unhidden(const SceneState& st, double x, double y, double footR, double topH, bool onRelief = false) const;

    void drawFields(QPainter& p, const SceneState& st);
    void drawRoads(QPainter& p, const SceneState& st);
    void drawSprites(QPainter& p, const SceneState& st);
    void drawLights(QPainter& p, const SceneState& st, double night);
    void drawLabels(QPainter& p, const SceneState& st);
    // vistas de arte (CartoSceneDeco.cpp): por baixo do terreno, só aparece no mar (linhas de rumo, ondinhas e monstro da
    // Carta; a grade da Planta); por cima, as rosas dos ventos da Carta e a borda gasta do papel
    void drawUnder(QPainter& p, const SceneState& st);
    void drawOver(QPainter& p, const SceneState& st);

    // nome sob o cursor (o desenhado por último ganha)
    const Label* labelAt(QPointF at) const;
    const Label* nameByRef(const QString& ref) const;
    std::vector<Label>& names() { return m_lastNames; }
    void invalidate() { m_itemsKey.clear(); m_lightsKey.clear(); }
    bool labelsPending() const { return m_labelsNear; }   // algum nome saiu com carimbo de outro tamanho

private:
    bool m_labelsNear = false;
    struct Item { int k; int idx; int sub; double d; };
    const std::vector<Item>& items(const SceneState& st);
    const std::vector<Path>& userRoadBridges(const SceneState& st);
    struct SpriteIndex;
    struct RoadPlan;
    const SpriteIndex& spriteIndex(const SceneState& st, const std::vector<Item>& its);
    std::shared_ptr<SpriteIndex> m_sprIdx;
    std::shared_ptr<RoadPlan> m_roadPlan;
    void drawAtlasWalls(QPainter& p, const SceneState& st, const UserLayer& U);

    struct SeaDeco { QString key; std::vector<std::array<double, 3>> roses, waves, beasts; };
    SeaDeco m_deco;
    const SeaDeco& seaSpots(const SceneState& st);
    QString m_itemsKey;
    std::vector<Item> m_items;
    QString m_rbKey;
    std::vector<Path> m_roadBridges;
    struct Light { double x, y, z, r; QString kind; double a; };
    QString m_lightsKey;
    std::vector<Light> m_lights;
    struct Laid { Label lab; std::vector<QRectF> boxes; QRectF bbox; };
    std::vector<Label> m_lastNames;
    std::vector<Laid> m_lastLayout;
};

} // namespace carto
