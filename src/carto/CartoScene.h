#pragma once

// Cena por cima do mapa (porte do scene.js do Map Lab): plantações, estradas,
// sprites (vegetação, casas, castelos, pontes, barcos), as luzes da noite e os
// nomes. Cada coisa pisa no chão pintado: o pintor guarda por pixel quanto o
// chão subiu e de que linha ele veio, então o que está atrás do morro some.

#include "CartoDoc.h"
#include "CartoSprites.h"

#include <QPointF>
#include <QRectF>

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
    bool info() const { return mode != QLatin1String("terrain") && doc && doc->info; }
};

struct Ground { double px, py, L; bool vis; double X, Y; };

class Scene {
public:
    std::optional<Ground> ground(const SceneState& st, double qx, double qy, double zTop = 0) const;
    spr::Cam cam(const SceneState& st, double L) const;                       // sem escala do mundo
    spr::Cam camAt(const SceneState& st, double L, double ox, double oy) const; // encolhe em volta do centro do objeto

    void drawFields(QPainter& p, const SceneState& st);
    void drawRoads(QPainter& p, const SceneState& st);
    void drawSprites(QPainter& p, const SceneState& st);
    void drawLights(QPainter& p, const SceneState& st, double night);
    void drawLabels(QPainter& p, const SceneState& st);

    // nome sob o cursor (o desenhado por último ganha)
    const Label* labelAt(QPointF at) const;
    const Label* nameByRef(const QString& ref) const;
    std::vector<Label>& names() { return m_lastNames; }
    void invalidate() { m_itemsKey.clear(); m_lightsKey.clear(); }

private:
    struct Item { int k; int idx; int sub; double d; };
    const std::vector<Item>& items(const SceneState& st);
    const std::vector<Path>& userRoadBridges(const SceneState& st);
    void drawAtlasWalls(QPainter& p, const SceneState& st, const UserLayer& U);

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
