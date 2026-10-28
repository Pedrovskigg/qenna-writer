#include "CartoNameLang.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace carto {

QString nameLanguage()
{
    const QString c = QCoreApplication::translate("carto", "pt", "language code of the generated place names on the map: pt, en, es, it or fr");
    return c == QLatin1String("en") || c == QLatin1String("es") || c == QLatin1String("it") || c == QLatin1String("fr") ? c : QStringLiteral("pt");
}

namespace {
QStringList L(std::initializer_list<const char*> l) { QStringList out; for (const char* s : l) out << QString::fromUtf8(s); return out; }

NameLang makePt()
{
    NameLang n;
    n.code = QStringLiteral("pt");
    n.words = {
        { "m", L({ "Cinzento", "Sombrio", "Branco", "Velho", "Rubro", "Gelado", "Esquecido", "Negro", "Dourado", "Silente" }) },
        { "f", L({ "Cinzenta", "Sombria", "Branca", "Velha", "Rubra", "Gelada", "Esquecida", "Negra", "Dourada", "Silente" }) },
        { "mp", L({ "Cinzentos", "Sombrios", "Brancos", "Velhos", "Rubros", "Gelados", "Esquecidos", "Negros", "Dourados", "Silentes" }) },
        { "fp", L({ "Cinzentas", "Sombrias", "Brancas", "Velhas", "Rubras", "Geladas", "Esquecidas", "Negras", "Douradas", "Silentes" }) },
        { "Nm", L({ "Vento Frio", "Lobo", "Corvo", "Trovão", "Gigante", "Rei Morto", "Falcão", "Silêncio", "Dragão", "Eco" }) },
        { "Nf", L({ "Coroa", "Aurora", "Névoa", "Serpente", "Lança", "Bruma", "Tormenta", "Águia", "Sentinela", "Cinza" }) },
        { "Nmp", L({ "Lobos", "Corvos", "Ecos", "Reis", "Ossos", "Naufrágios", "Gigantes" }) },
        { "Nfp", L({ "Pedras", "Garças", "Brumas", "Sombras", "Cinzas", "Sereias", "Tormentas" }) },
    };
    n.templates = {
        { "ocean", L({ "Oceano de {R}", "Oceano {m}", "Mar Sem Fim", "Oceano das {Nfp}", "Grande Mar de {R}", "Oceano de {R}" }) },
        { "sea", L({ "Mar de {R}", "Mar {m}", "Mar das {Nfp}", "Mar dos {Nmp}", "Mar de {R}" }) },
        { "bay", L({ "Baía de {R}", "Golfo de {R}", "Enseada de {R}", "Baía das {Nfp}", "Golfo {m}", "Baía do {Nm}" }) },
        { "lake", L({ "Lago {R}", "Lago de {R}", "Lago {m}", "Lagoa de {R}", "Lago das {Nfp}" }) },
        { "rangeL", L({ "Cordilheira de {R}", "Cordilheira {f}", "Espinha do {Nm}", "Grande Serra de {R}", "Cordilheira de {R}" }) },
        { "range", L({ "Serra de {R}", "Serra {f}", "Serra do {Nm}", "Serra da {Nf}", "Serra de {R}" }) },
        { "rangeS", L({ "Montes {mp}", "Montes de {R}", "Maciço de {R}", "Picos de {R}", "Montes do {Nm}" }) },
        { "peak", L({ "Pico {R}", "Monte {R}", "Pico do {Nm}", "Pico da {Nf}", "Monte {m}" }) },
        { "forest", L({ "Floresta de {R}", "Bosque de {R}", "Mata {f}", "Floresta {f}", "Bosque dos {Nmp}", "Floresta de {R}" }) },
        { "desert", L({ "Deserto de {R}", "Areias de {R}", "Ermos de {R}", "Deserto {m}", "Dunas de {R}", "Terras Queimadas" }) },
        { "marsh", L({ "Pântano de {R}", "Charcos de {R}", "Brejo {m}", "Alagados de {R}", "Pântano das {Nfp}" }) },
        { "ice", L({ "Tundra de {R}", "Ermos Gélidos", "Geleira de {R}", "Terras Brancas de {R}", "Planalto de {R}" }) },
        { "plains", L({ "Campos de {R}", "Planícies de {R}", "Vale de {R}", "Campinas {fp}" }) },
        { "island", L({ "Ilha de {R}", "Ilha {f}", "Ilha do {Nm}", "Ilha da {Nf}", "Ilha de {R}" }) },
        { "isles", L({ "Ilhas {fp}", "Arquipélago de {R}", "Ilhas de {R}", "Ilhas das {Nfp}" }) },
        { "river", L({ "Rio {R}", "Rio {m}", "Rio das {Nfp}", "Rio do {Nm}", "Ribeira de {R}" }) },
    };
    return n;
}

NameLang makeEn()
{
    NameLang n;
    n.code = QStringLiteral("en");
    const QStringList adj = L({ "Grey", "Shadowed", "White", "Old", "Crimson", "Frozen", "Forgotten", "Black", "Golden", "Silent" });
    n.words = {
        { "m", adj }, { "f", adj }, { "mp", adj }, { "fp", adj },
        { "Nm", L({ "Cold Wind", "Wolf", "Raven", "Thunder", "Giant", "Dead King", "Falcon", "Silence", "Dragon", "Echo" }) },
        { "Nf", L({ "Crown", "Dawn", "Mist", "Serpent", "Spear", "Haze", "Storm", "Eagle", "Sentinel", "Ash" }) },
        { "Nmp", L({ "Wolves", "Ravens", "Echoes", "Kings", "Bones", "Shipwrecks", "Giants" }) },
        { "Nfp", L({ "Stones", "Herons", "Mists", "Shadows", "Ashes", "Sirens", "Storms" }) },
    };
    n.templates = {
        { "ocean", L({ "{R} Ocean", "The {m} Ocean", "The Endless Sea", "Ocean of {Nfp}", "Great Sea of {R}", "{R} Ocean" }) },
        { "sea", L({ "Sea of {R}", "The {m} Sea", "Sea of {Nfp}", "Sea of {Nmp}", "{R} Sea" }) },
        { "bay", L({ "Bay of {R}", "Gulf of {R}", "{R} Cove", "Bay of {Nfp}", "The {m} Gulf", "{Nm} Bay" }) },
        { "lake", L({ "Lake {R}", "{R} Lake", "The {m} Lake", "{R} Lagoon", "Lake of {Nfp}" }) },
        { "rangeL", L({ "{R} Range", "The {f} Range", "Spine of the {Nm}", "Great {R} Mountains", "{R} Range" }) },
        { "range", L({ "{R} Ridge", "The {f} Ridge", "{Nm} Ridge", "{Nf} Ridge", "{R} Mountains" }) },
        { "rangeS", L({ "The {mp} Peaks", "{R} Hills", "{R} Massif", "{R} Peaks", "{Nm} Hills" }) },
        { "peak", L({ "Mount {R}", "{R} Peak", "{Nm} Peak", "{Nf} Peak", "Mount {m}" }) },
        { "forest", L({ "{R} Forest", "{R} Woods", "The {f} Wood", "The {f} Forest", "Wood of {Nmp}", "{R} Forest" }) },
        { "desert", L({ "{R} Desert", "Sands of {R}", "{R} Wastes", "The {m} Desert", "{R} Dunes", "The Burnt Lands" }) },
        { "marsh", L({ "{R} Marsh", "{R} Fens", "The {m} Bog", "{R} Mires", "Marsh of {Nfp}" }) },
        { "ice", L({ "{R} Tundra", "The Frozen Wastes", "{R} Glacier", "White Lands of {R}", "{R} Plateau" }) },
        { "plains", L({ "Fields of {R}", "{R} Plains", "Vale of {R}", "The {fp} Meadows" }) },
        { "island", L({ "Isle of {R}", "The {f} Isle", "{Nm} Island", "{Nf} Island", "{R} Island" }) },
        { "isles", L({ "The {fp} Isles", "{R} Archipelago", "Isles of {R}", "Isles of {Nfp}" }) },
        { "river", L({ "River {R}", "The {m} River", "River of {Nfp}", "{Nm} River", "{R} Stream" }) },
    };
    return n;
}

NameLang makeEs()
{
    NameLang n;
    n.code = QStringLiteral("es");
    n.words = {
        { "m", L({ "Gris", "Sombrío", "Blanco", "Viejo", "Bermejo", "Helado", "Olvidado", "Negro", "Dorado", "Silente" }) },
        { "f", L({ "Gris", "Sombría", "Blanca", "Vieja", "Bermeja", "Helada", "Olvidada", "Negra", "Dorada", "Silente" }) },
        { "mp", L({ "Grises", "Sombríos", "Blancos", "Viejos", "Bermejos", "Helados", "Olvidados", "Negros", "Dorados", "Silentes" }) },
        { "fp", L({ "Grises", "Sombrías", "Blancas", "Viejas", "Bermejas", "Heladas", "Olvidadas", "Negras", "Doradas", "Silentes" }) },
        { "Nm", L({ "Viento Frío", "Lobo", "Cuervo", "Trueno", "Gigante", "Rey Muerto", "Halcón", "Silencio", "Dragón", "Eco" }) },
        { "Nf", L({ "Corona", "Aurora", "Niebla", "Serpiente", "Lanza", "Bruma", "Tormenta", "Águila", "Centinela", "Ceniza" }) },
        { "Nmp", L({ "Lobos", "Cuervos", "Ecos", "Reyes", "Huesos", "Naufragios", "Gigantes" }) },
        { "Nfp", L({ "Piedras", "Garzas", "Brumas", "Sombras", "Cenizas", "Sirenas", "Tormentas" }) },
    };
    n.templates = {
        { "ocean", L({ "Océano de {R}", "Océano {m}", "Mar sin Fin", "Océano de las {Nfp}", "Gran Mar de {R}", "Océano de {R}" }) },
        { "sea", L({ "Mar de {R}", "Mar {m}", "Mar de las {Nfp}", "Mar de los {Nmp}", "Mar de {R}" }) },
        { "bay", L({ "Bahía de {R}", "Golfo de {R}", "Ensenada de {R}", "Bahía de las {Nfp}", "Golfo {m}", "Bahía del {Nm}" }) },
        { "lake", L({ "Lago {R}", "Lago de {R}", "Lago {m}", "Laguna de {R}", "Lago de las {Nfp}" }) },
        { "rangeL", L({ "Cordillera de {R}", "Cordillera {f}", "Espinazo del {Nm}", "Gran Sierra de {R}", "Cordillera de {R}" }) },
        { "range", L({ "Sierra de {R}", "Sierra {f}", "Sierra del {Nm}", "Sierra de la {Nf}", "Sierra de {R}" }) },
        { "rangeS", L({ "Montes {mp}", "Montes de {R}", "Macizo de {R}", "Picos de {R}", "Montes del {Nm}" }) },
        { "peak", L({ "Pico {R}", "Monte {R}", "Pico del {Nm}", "Pico de la {Nf}", "Monte {m}" }) },
        { "forest", L({ "Bosque de {R}", "Arboleda de {R}", "Selva {f}", "Floresta {f}", "Bosque de los {Nmp}", "Bosque de {R}" }) },
        { "desert", L({ "Desierto de {R}", "Arenas de {R}", "Yermos de {R}", "Desierto {m}", "Dunas de {R}", "Tierras Quemadas" }) },
        { "marsh", L({ "Pantano de {R}", "Ciénagas de {R}", "Marjal {m}", "Marismas de {R}", "Pantano de las {Nfp}" }) },
        { "ice", L({ "Tundra de {R}", "Yermos Helados", "Glaciar de {R}", "Tierras Blancas de {R}", "Meseta de {R}" }) },
        { "plains", L({ "Campos de {R}", "Llanuras de {R}", "Valle de {R}", "Praderas {fp}" }) },
        { "island", L({ "Isla de {R}", "Isla {f}", "Isla del {Nm}", "Isla de la {Nf}", "Isla de {R}" }) },
        { "isles", L({ "Islas {fp}", "Archipiélago de {R}", "Islas de {R}", "Islas de las {Nfp}" }) },
        { "river", L({ "Río {R}", "Río {m}", "Río de las {Nfp}", "Río del {Nm}", "Arroyo de {R}" }) },
    };
    return n;
}

NameLang makeIt()
{
    NameLang n;
    n.code = QStringLiteral("it");
    // nomes que começam com vogal ficam de fora: "della Alba" pediria elisão
    n.words = {
        { "m", L({ "Grigio", "Cupo", "Bianco", "Vecchio", "Vermiglio", "Gelido", "Dimenticato", "Nero", "Dorato", "Silente" }) },
        { "f", L({ "Grigia", "Cupa", "Bianca", "Vecchia", "Vermiglia", "Gelida", "Dimenticata", "Nera", "Dorata", "Silente" }) },
        { "mp", L({ "Grigi", "Cupi", "Bianchi", "Vecchi", "Vermigli", "Gelidi", "Dimenticati", "Neri", "Dorati", "Silenti" }) },
        { "fp", L({ "Grigie", "Cupe", "Bianche", "Vecchie", "Vermiglie", "Gelide", "Dimenticate", "Nere", "Dorate", "Silenti" }) },
        { "Nm", L({ "Vento Freddo", "Lupo", "Corvo", "Tuono", "Gigante", "Re Morto", "Falco", "Silenzio", "Drago", "Grifone" }) },
        { "Nf", L({ "Corona", "Brina", "Nebbia", "Serpe", "Lancia", "Bruma", "Tempesta", "Falce", "Sentinella", "Cenere" }) },
        { "Nmp", L({ "Lupi", "Corvi", "Falchi", "Re", "Draghi", "Naufragi", "Giganti" }) },
        { "Nfp", L({ "Pietre", "Gru", "Brume", "Ombre", "Ceneri", "Sirene", "Tempeste" }) },
    };
    n.templates = {
        { "ocean", L({ "Oceano di {R}", "Oceano {m}", "Mare Senza Fine", "Oceano delle {Nfp}", "Grande Mare di {R}", "Oceano di {R}" }) },
        { "sea", L({ "Mare di {R}", "Mare {m}", "Mare delle {Nfp}", "Mare dei {Nmp}", "Mare di {R}" }) },
        { "bay", L({ "Baia di {R}", "Golfo di {R}", "Insenatura di {R}", "Baia delle {Nfp}", "Golfo {m}", "Baia del {Nm}" }) },
        { "lake", L({ "Lago {R}", "Lago di {R}", "Lago {m}", "Laguna di {R}", "Lago delle {Nfp}" }) },
        { "rangeL", L({ "Cordigliera di {R}", "Cordigliera {f}", "Spina del {Nm}", "Grande Catena di {R}", "Cordigliera di {R}" }) },
        { "range", L({ "Catena di {R}", "Catena {f}", "Catena del {Nm}", "Catena della {Nf}", "Catena di {R}" }) },
        { "rangeS", L({ "Monti {mp}", "Monti di {R}", "Massiccio di {R}", "Picchi di {R}", "Monti del {Nm}" }) },
        { "peak", L({ "Picco {R}", "Monte {R}", "Picco del {Nm}", "Picco della {Nf}", "Monte {m}" }) },
        { "forest", L({ "Foresta di {R}", "Bosco di {R}", "Selva {f}", "Foresta {f}", "Bosco dei {Nmp}", "Foresta di {R}" }) },
        { "desert", L({ "Deserto di {R}", "Sabbie di {R}", "Lande di {R}", "Deserto {m}", "Dune di {R}", "Terre Bruciate" }) },
        { "marsh", L({ "Palude di {R}", "Acquitrini di {R}", "Pantano {m}", "Paludi di {R}", "Palude delle {Nfp}" }) },
        { "ice", L({ "Tundra di {R}", "Lande Gelide", "Ghiacciaio di {R}", "Terre Bianche di {R}", "Altopiano di {R}" }) },
        { "plains", L({ "Campi di {R}", "Pianure di {R}", "Valle di {R}", "Praterie {fp}" }) },
        { "island", L({ "Isola di {R}", "Isola {f}", "Isola del {Nm}", "Isola della {Nf}", "Isola di {R}" }) },
        { "isles", L({ "Isole {fp}", "Arcipelago di {R}", "Isole di {R}", "Isole delle {Nfp}" }) },
        { "river", L({ "Fiume {R}", "Fiume {m}", "Fiume delle {Nfp}", "Fiume del {Nm}", "Torrente di {R}" }) },
    };
    return n;
}

NameLang makeFr()
{
    NameLang n;
    n.code = QStringLiteral("fr");
    // "du"/"de la" pedem nome que começa com consoante (senão: "de l'", "de l'")
    n.words = {
        { "m", L({ "Gris", "Sombre", "Blanc", "Vieux", "Vermeil", "Glacé", "Oublié", "Noir", "Doré", "Silencieux" }) },
        { "f", L({ "Grise", "Sombre", "Blanche", "Vieille", "Vermeille", "Glacée", "Oubliée", "Noire", "Dorée", "Silencieuse" }) },
        { "mp", L({ "Gris", "Sombres", "Blancs", "Vieux", "Vermeils", "Glacés", "Oubliés", "Noirs", "Dorés", "Silencieux" }) },
        { "fp", L({ "Grises", "Sombres", "Blanches", "Vieilles", "Vermeilles", "Glacées", "Oubliées", "Noires", "Dorées", "Silencieuses" }) },
        { "Nm", L({ "Vent Froid", "Loup", "Corbeau", "Tonnerre", "Géant", "Roi Mort", "Faucon", "Silence", "Dragon", "Griffon" }) },
        { "Nf", L({ "Couronne", "Brume", "Lance", "Tempête", "Sentinelle", "Cendre", "Vigie", "Faux", "Flamme", "Chouette" }) },
        { "Nmp", L({ "Loups", "Corbeaux", "Échos", "Rois", "Os", "Naufrages", "Géants" }) },
        { "Nfp", L({ "Pierres", "Grues", "Brumes", "Ombres", "Cendres", "Sirènes", "Tempêtes" }) },
    };
    n.templates = {
        { "ocean", L({ "Océan de {R}", "Océan {m}", "Mer Sans Fin", "Océan des {Nfp}", "Grande Mer de {R}", "Océan de {R}" }) },
        { "sea", L({ "Mer de {R}", "Mer {f}", "Mer des {Nfp}", "Mer des {Nmp}", "Mer de {R}" }) },
        { "bay", L({ "Baie de {R}", "Golfe de {R}", "Anse de {R}", "Baie des {Nfp}", "Golfe {m}", "Baie du {Nm}" }) },
        { "lake", L({ "Lac {R}", "Lac de {R}", "Lac {m}", "Lagune de {R}", "Lac des {Nfp}" }) },
        { "rangeL", L({ "Cordillère de {R}", "Cordillère {f}", "Échine du {Nm}", "Grande Chaîne de {R}", "Cordillère de {R}" }) },
        { "range", L({ "Chaîne de {R}", "Chaîne {f}", "Chaîne du {Nm}", "Chaîne de la {Nf}", "Chaîne de {R}" }) },
        { "rangeS", L({ "Monts {mp}", "Monts de {R}", "Massif de {R}", "Pics de {R}", "Monts du {Nm}" }) },
        { "peak", L({ "Pic {R}", "Mont {R}", "Pic du {Nm}", "Pic de la {Nf}", "Mont {m}" }) },
        { "forest", L({ "Forêt de {R}", "Bois de {R}", "Sylve {f}", "Forêt {f}", "Bois des {Nmp}", "Forêt de {R}" }) },
        { "desert", L({ "Désert de {R}", "Sables de {R}", "Landes de {R}", "Désert {m}", "Dunes de {R}", "Terres Brûlées" }) },
        { "marsh", L({ "Marais de {R}", "Marécages de {R}", "Marais {m}", "Tourbières de {R}", "Marais des {Nfp}" }) },
        { "ice", L({ "Toundra de {R}", "Landes Glacées", "Glacier de {R}", "Terres Blanches de {R}", "Plateau de {R}" }) },
        { "plains", L({ "Champs de {R}", "Plaines de {R}", "Vallée de {R}", "Prairies {fp}" }) },
        { "island", L({ "Île de {R}", "Île {f}", "Île du {Nm}", "Île de la {Nf}", "Île de {R}" }) },
        { "isles", L({ "Îles {fp}", "Archipel de {R}", "Îles de {R}", "Îles des {Nfp}" }) },
        { "river", L({ "Fleuve {R}", "Fleuve {m}", "Rivière des {Nfp}", "Fleuve du {Nm}", "Ruisseau de {R}" }) },
    };
    return n;
}
}

const NameLang& nameLang()
{
    static QHash<QString, NameLang> cache;
    const QString code = nameLanguage();
    auto it = cache.find(code);
    if (it != cache.end()) return it.value();
    NameLang n = code == QLatin1String("en") ? makeEn() : code == QLatin1String("es") ? makeEs() : code == QLatin1String("it") ? makeIt()
               : code == QLatin1String("fr") ? makeFr() : makePt();
    return cache.insert(code, n).value();
}

QString polishName(const QString& s)
{
    if (nameLanguage() != QLatin1String("fr")) return s;
    // francês: "de" antes de vogal vira "d'" (Mer d'Astal, Terres d'Ithane)
    static const QRegularExpression de(QStringLiteral("\\bde ([AEIOUYÉÈÊÎÔaeiouyéèêîô])"));
    QString out = s;
    out.replace(de, QStringLiteral("d'\\1"));
    return out;
}

} // namespace carto
