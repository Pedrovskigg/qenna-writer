#include "GalaxyPresets.h"

#include <QCoreApplication>
#include <QLatin1String>

#include <cstring>

namespace galaxy {

namespace {
Tune baseTune(double ly, const char* stars) { Tune t; t.has = true; t.ly = ly; t.starsText = stars; return t; }
std::vector<Preset> build()
{
    std::vector<Preset> out;
    const char* const near = QT_TRANSLATE_NOOP("galaxy::Preset", "Nossa vizinhança");
    const char* const famous = QT_TRANSLATE_NOOP("galaxy::Preset", "Famosas");
    {
        Tune t = baseTune(100000, "entre 100 e 400 bilhões");
        t.hasAngle = true; t.angle = 0.5; t.hasK = true; t.k = 2; t.hasWarp = true; t.warp = 0.35;
        out.push_back({ "lactea", near, QT_TRANSLATE_NOOP("galaxy::Preset", "Via Láctea"), QT_TRANSLATE_NOOP("galaxy::Preset", "A nossa. Espiral barrada, com os quatro grandes braços nomeados e o Sistema Solar no lugar dele."), Style::Barrada, 4, Size::Grande, 200, 1977, t, "Sagittarius A*", true, {} });
    }
    {
        Tune t = baseTune(220000, "cerca de 1 trilhão");
        t.hasAngle = true; t.angle = 1.1; t.hasK = true; t.k = 3.1; t.hasWarp = true; t.warp = 0.5; t.bulgeK = 1.6; t.ringAdd = 0.75; t.hasRingR = true; t.ringR = 0.42; t.ringW = 0.05;
        t.blobs = { { 0.1, -0.28, 0.035, 1.5, false }, { -0.42, 0.62, 0.075, 0.55, false } };
        out.push_back({ "andromeda", near, QT_TRANSLATE_NOOP("galaxy::Preset", "Andrômeda"), QT_TRANSLATE_NOOP("galaxy::Preset", "A vizinha gigante, M31. Braços bem fechados, o Anel de Fogo e as companheiras M32 e M110."), Style::Espiral, 2, Size::Imensa, 300, 31, t, nullptr, false, { { QT_TRANSLATE_NOOP("galaxy::Preset", "Anel de Fogo"), 0.31, 0.36 }, { "M32", 0.1, -0.34 }, { "M110", -0.42, 0.72 } } });
    }
    {
        Tune t = baseTune(60000, "cerca de 40 bilhões");
        t.hasAngle = true; t.angle = 2.2; t.hasK = true; t.k = 1.3; t.hasWarp = true; t.warp = 0.8; t.bulgeK = 0.45;
        out.push_back({ "triangulo", near, QT_TRANSLATE_NOOP("galaxy::Preset", "Triângulo"), QT_TRANSLATE_NOOP("galaxy::Preset", "M33, a terceira maior do Grupo Local. Espiral solta, sem bojo forte, cheia de berçários de estrelas."), Style::Espiral, 2, Size::Media, 90, 33, t, nullptr, false, {} });
    }
    {
        Tune t = baseTune(32000, "cerca de 30 bilhões");
        t.hasOx = true; t.ox = -0.05; t.hasOy = true; t.oy = 0.03; t.hasBarI = true; t.barA = 0.35; t.barK = 1.3;
        t.blobs = { { 0.28, 0.3, 0.06, 2.2, true } };
        out.push_back({ "gnuvem", near, QT_TRANSLATE_NOOP("galaxy::Preset", "Grande Nuvem de Magalhães"), QT_TRANSLATE_NOOP("galaxy::Preset", "Satélite da Via Láctea. Irregular, com uma barra torta e a Nebulosa da Tarântula brilhando."), Style::Irregular, 4, Size::Pequena, 60, 1519, t, nullptr, false, { { QT_TRANSLATE_NOOP("galaxy::Preset", "Nebulosa da Tarântula"), 0.28, 0.42 } } });
    }
    {
        Tune t = baseTune(19000, "cerca de 3 bilhões");
        t.hasOx = true; t.ox = 0.08; t.hasOy = true; t.oy = -0.08; t.hasBarI = true; t.barA = 2.3; t.barK = 0.8;
        out.push_back({ "pnuvem", near, QT_TRANSLATE_NOOP("galaxy::Preset", "Pequena Nuvem de Magalhães"), QT_TRANSLATE_NOOP("galaxy::Preset", "A irmã menor da Grande Nuvem. Uma nuvem esticada de estrelas, deformada pela vizinhança."), Style::Irregular, 4, Size::Pequena, 40, 1520, t, nullptr, false, {} });
    }
    {
        Tune t = baseTune(76000, "cerca de 100 bilhões");
        t.hasAngle = true; t.angle = 0.2; t.hasK = true; t.k = 1.25; t.hasWarp = true; t.warp = 0.25;
        t.blobs = { { 0.2, 0.82, 0.09, 1.5, false } };
        out.push_back({ "redemoinho", famous, QT_TRANSLATE_NOOP("galaxy::Preset", "Redemoinho"), QT_TRANSLATE_NOOP("galaxy::Preset", "M51. Os dois braços mais perfeitos do céu, com a companheira NGC 5195 na ponta de um deles."), Style::Espiral, 2, Size::Media, 120, 51, t, nullptr, false, { { "NGC 5195", 0.2, 0.98 } } });
    }
    {
        Tune t = baseTune(170000, "cerca de 1 trilhão");
        t.hasAngle = true; t.angle = 0.9; t.hasK = true; t.k = 1.05; t.hasWarp = true; t.warp = 1.1; t.bulgeK = 0.7;
        out.push_back({ "catavento", famous, QT_TRANSLATE_NOOP("galaxy::Preset", "Cata-vento"), QT_TRANSLATE_NOOP("galaxy::Preset", "M101. Enorme, vista de frente, com muitos braços abertos e meio tortos."), Style::Espiral, 5, Size::Imensa, 300, 101, t, nullptr, false, {} });
    }
    {
        Tune t = baseTune(50000, "cerca de 100 bilhões");
        t.hasRingR = true; t.ringR = 0.7; t.ringW = 0.065; t.coreK = 1; t.coreR = 0.2; t.dustK = 3;
        out.push_back({ "sombrero", famous, QT_TRANSLATE_NOOP("galaxy::Preset", "Sombrero"), QT_TRANSLATE_NOOP("galaxy::Preset", "M104. Um bojo gigante e brilhante dentro de um anel largo de poeira escura."), Style::Anel, 4, Size::Media, 90, 104, t, nullptr, false, {} });
    }
    {
        Tune t = baseTune(130000, "alguns trilhões");
        t.hasAngle = true; t.angle = 0.6; t.hasQ = true; t.q = 0.94; t.hasJet = true; t.jetA = 0.55; t.jetLen = 0.22;
        out.push_back({ "m87", famous, QT_TRANSLATE_NOOP("galaxy::Preset", "M87"), QT_TRANSLATE_NOOP("galaxy::Preset", "A elíptica gigante de Virgem, quase redonda, com o buraco negro da primeira foto e o jato saindo dele."), Style::Eliptica, 4, Size::Imensa, 220, 87, t, "M87*", false, { { QT_TRANSLATE_NOOP("galaxy::Preset", "Jato"), 0.32, 0.13 } } });
    }
    {
        Tune t = baseTune(150000, "dezenas de bilhões");
        t.hasRingR = true; t.ringR = 0.78; t.ringW = 0.055; t.ring2R = 0.2; t.spokes = 14;
        out.push_back({ "roda", famous, QT_TRANSLATE_NOOP("galaxy::Preset", "Roda de Carro"), QT_TRANSLATE_NOOP("galaxy::Preset", "Uma galáxia menor atravessou esta e abriu uma onda de estrelas: anel, raios e miolo."), Style::Anel, 4, Size::Grande, 130, 1941, t, nullptr, false, {} });
    }
    return out;
}
} // namespace

const std::vector<Preset>& presets() { static const std::vector<Preset> p = build(); return p; }
const Preset* presetById(const QString& id)
{
    for (const Preset& p : presets()) if (id == QLatin1String(p.id)) return &p;
    return nullptr;
}

// Sistema Solar: Terra = 1 em massa e raio; distância em UA, dia em horas, temperatura em kelvin.
const std::vector<SolarPlanet>& solarPlanets()
{
    static const std::vector<SolarPlanet> s {
        { QT_TRANSLATE_NOOP("galaxy::Preset", "Mercúrio"), Type::Arido, 0.387, 0.055, 0.383, 1408, 440, 0.03, 0.206, false, "Quase nenhuma", 0, LookStyle::None, 0, false, {} },
        { QT_TRANSLATE_NOOP("galaxy::Preset", "Vênus"), Type::Toxico, 0.723, 0.815, 0.949, 5832, 737, 2.6, 0.007, true, "Dióxido de carbono denso, nuvens ácidas", 0, LookStyle::None, 0, false, {} },
        { QT_TRANSLATE_NOOP("galaxy::Preset", "Terra"), Type::Temperado, 1, 1, 1, 23.93, 288, 23.44, 0.017, false, "Nitrogênio e oxigênio", 0.71, LookStyle::Continental, 8.1e9, false, { { QT_TRANSLATE_NOOP("galaxy::Preset", "Lua"), Type::Rochosa, 384400, 1737, 27.32, 0.165 } } },
        { QT_TRANSLATE_NOOP("galaxy::Preset", "Marte"), Type::Deserto, 1.524, 0.107, 0.532, 24.62, 210, 25.19, 0.093, false, "CO₂, muito fina", 0, LookStyle::None, 0, false, { { QT_TRANSLATE_NOOP("galaxy::Preset", "Fobos"), Type::Rochosa, 9376, 11, 0.32, 0.0006 }, { QT_TRANSLATE_NOOP("galaxy::Preset", "Deimos"), Type::Rochosa, 23463, 6, 1.26, 0.0003 } } },
        { QT_TRANSLATE_NOOP("galaxy::Preset", "Júpiter"), Type::Gigante, 5.203, 317.8, 11.21, 9.93, 165, 3.13, 0.049, false, "Hidrogênio e hélio", 0, LookStyle::None, 0, false, { { QT_TRANSLATE_NOOP("galaxy::Preset", "Io"), Type::Vulcanica, 421700, 1822, 1.77, 0.183 }, { QT_TRANSLATE_NOOP("galaxy::Preset", "Europa"), Type::Gelada, 671100, 1561, 3.55, 0.134 }, { QT_TRANSLATE_NOOP("galaxy::Preset", "Ganimedes"), Type::Gelada, 1070400, 2634, 7.15, 0.146 }, { QT_TRANSLATE_NOOP("galaxy::Preset", "Calisto"), Type::Gelada, 1882700, 2410, 16.69, 0.126 } } },
        { QT_TRANSLATE_NOOP("galaxy::Preset", "Saturno"), Type::Gigante, 9.537, 95.2, 9.45, 10.7, 134, 26.73, 0.054, false, "Hidrogênio e hélio", 0, LookStyle::None, 0, true, { { QT_TRANSLATE_NOOP("galaxy::Preset", "Encélado"), Type::Gelada, 238000, 252, 1.37, 0.011 }, { QT_TRANSLATE_NOOP("galaxy::Preset", "Titã"), Type::Rochosa, 1221900, 2575, 15.95, 0.138 } } },
        { QT_TRANSLATE_NOOP("galaxy::Preset", "Urano"), Type::GeloG, 19.19, 14.5, 4.01, 17.24, 76, 82.2, 0.047, true, "Hidrogênio, hélio e metano", 0, LookStyle::None, 0, false, { { QT_TRANSLATE_NOOP("galaxy::Preset", "Titânia"), Type::Gelada, 436300, 789, 8.71, 0.037 } } },
        { QT_TRANSLATE_NOOP("galaxy::Preset", "Netuno"), Type::GeloG, 30.07, 17.1, 3.88, 16.11, 72, 28.32, 0.009, false, "Hidrogênio, hélio e metano", 0, LookStyle::None, 0, false, { { QT_TRANSLATE_NOOP("galaxy::Preset", "Tritão"), Type::Gelada, 354800, 1353, 5.88, 0.08 } } },
        { QT_TRANSLATE_NOOP("galaxy::Preset", "Plutão"), Type::Anao, 39.48, 0.0022, 0.186, 153.3, 44, 57.5, 0.249, false, "Fina, nitrogênio", 0, LookStyle::None, 0, false, { { QT_TRANSLATE_NOOP("galaxy::Preset", "Caronte"), Type::Gelada, 19600, 606, 6.39, 0.029 } } },
    };
    return s;
}

void applySolarLook(const char* name, Look& L)
{
    const auto is = [&](const char* n) { return std::strcmp(name, n) == 0; };
    if (is("Mercúrio")) { L.tint = { 0.55, 0.52, 0.5 }; }
    else if (is("Vênus")) { L.tint = { 0.9, 0.8, 0.56 }; L.pal = { { 0, { 0.74, 0.62, 0.42 } }, { 0.5, { 0.9, 0.82, 0.62 } }, { 1, { 0.72, 0.6, 0.4 } } }; L.bandsF = 4; L.turb = 0.5; L.storms.clear(); }
    else if (is("Terra")) { L.shallow = { 0.08, 0.32, 0.52 }; L.deep = { 0.02, 0.09, 0.28 }; L.veg = { 0.14, 0.3, 0.09 }; L.vegKeyV = Veg::Verde; L.dry = { 0.66, 0.54, 0.36 }; L.dryBias = -0.28; L.dryBase = -0.28; L.hasDryBase = true; L.belt = 0.16; L.cap = 1.25; L.snowAt = 0.55; L.cloud = 0.1; L.cloudCol = { 1.02, 1.02, 1.03 }; L.ice = { 0.93, 0.95, 0.98 }; }
    else if (is("Marte")) { L.pal = { { 0, { 0.42, 0.2, 0.1 } }, { 0.5, { 0.72, 0.38, 0.2 } }, { 1, { 0.84, 0.55, 0.32 } } }; L.cap = 1.2; L.canyon = true; }
    else if (is("Júpiter")) { L.pal = { { 0, { 0.56, 0.4, 0.28 } }, { 0.3, { 0.86, 0.72, 0.54 } }, { 0.5, { 0.94, 0.88, 0.76 } }, { 0.72, { 0.72, 0.52, 0.36 } }, { 1, { 0.9, 0.8, 0.66 } } }; L.bandsF = 12; L.turb = 1.1; L.storms = { { -0.36, 0.6, 0.3, 0.1, { 0.78, 0.36, 0.22 } } }; }
    else if (is("Saturno")) { L.pal = { { 0, { 0.72, 0.62, 0.44 } }, { 0.5, { 0.92, 0.84, 0.64 } }, { 1, { 0.8, 0.7, 0.5 } } }; L.bandsF = 9; L.turb = 0.5; L.storms.clear(); }
    else if (is("Urano")) { L.pal = { { 0, { 0.62, 0.84, 0.86 } }, { 0.5, { 0.74, 0.92, 0.93 } }, { 1, { 0.62, 0.84, 0.86 } } }; L.bandsF = 2; L.turb = 0.2; L.storms.clear(); }
    else if (is("Netuno")) { L.pal = { { 0, { 0.16, 0.3, 0.72 } }, { 0.5, { 0.28, 0.46, 0.88 } }, { 1, { 0.16, 0.3, 0.72 } } }; L.bandsF = 4; L.turb = 0.5; L.storms = { { -0.34, 0.9, 0.2, 0.07, { 0.1, 0.16, 0.42 } } }; }
    else if (is("Plutão")) { L.tint = { 0.82, 0.72, 0.6 }; }
}

} // namespace galaxy
