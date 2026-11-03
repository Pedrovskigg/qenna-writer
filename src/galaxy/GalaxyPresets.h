#pragma once

// As galáxias prontas do criador (porte de presets.js). A forma e os números seguem as galáxias
// de verdade, vistas de cima; as estrelas e os mundos são gerados, menos o Sistema Solar, que vem com os dados reais.

#include "GalaxyCore.h"

#include <QString>

namespace galaxy {

struct PresetLabel { const char* text; double x, y; };
struct Preset {
    const char* id;
    const char* group;
    const char* name;
    const char* desc;
    Style style;
    int arms;
    Size size;
    int count;
    uint32_t seed;
    Tune tune;
    const char* core;     // nome do buraco negro do centro, ou nullptr
    bool solar;
    std::vector<PresetLabel> labels;
};
const std::vector<Preset>& presets();
const Preset* presetById(const QString& id);

struct SolarMoon { const char* name; Type type; double d, r, P, g; };
struct SolarPlanet {
    const char* name;
    Type type;
    double a, M, R, day, T, tilt, e;
    bool retro;
    const char* atm;
    double water;
    LookStyle style;
    double people;
    bool rings;
    std::vector<SolarMoon> moons;
};
const std::vector<SolarPlanet>& solarPlanets();
// As cores de cada um, por cima do que o gerador sortear. Textura ainda é procedural.
void applySolarLook(const char* name, Look& L);

} // namespace galaxy
