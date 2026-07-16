#include "NameGenerator.h"

#include <QRandomGenerator>
#include <QSet>

namespace {

constexpr int kMaxOrder = 3;
const QChar kEnd = QChar('\n'); // terminador (nunca aparece num nome)

// ---------------- Datasets de treino (sementes do Markov) ----------------
// Pequenos de propósito: o Markov generaliza o "som" do estilo. Nomes não têm
// copyright; o que vale é a sonoridade.

const QStringList& elvish() {
    static const QStringList d = {
        "Aelar","Aeris","Caelar","Caelyn","Elenya","Elaria","Faelar","Faelyn",
        "Galadel","Ithilien","Lariel","Lorien","Myriel","Nimue","Sylvar","Thalas",
        "Thandor","Vaelora","Yllae","Aerendil","Elandor","Naeris","Sariel","Taeral",
        "Variel","Aelithil","Celebor","Eowyn","Finrod","Luthien" };
    return d;
}
const QStringList& nordic() {
    static const QStringList d = {
        "Bjorn","Sigrid","Ragnar","Astrid","Eirik","Gunnar","Helga","Ingrid",
        "Leif","Sven","Thora","Ulfar","Frida","Hakon","Sigurd","Yrsa","Brynja",
        "Torvald","Greta","Knut","Solveig","Vidar","Eldgrim","Halvard","Runa",
        "Gudrun","Harald","Ivar","Njal","Skadi" };
    return d;
}
const QStringList& latin() {
    static const QStringList d = {
        "Marcus","Lucius","Livia","Cassia","Tiberius","Aurelia","Octavia","Valerius",
        "Cornelius","Drusilla","Fabius","Julia","Quintus","Severus","Antonia","Gaius",
        "Flavia","Maximus","Lucretia","Cassius","Decimus","Vespasia","Aelia","Crispus",
        "Sabina","Tullius","Marcella","Albinus","Caelina","Servius" };
    return d;
}
const QStringList& slavic() {
    static const QStringList d = {
        "Mirko","Vesna","Dragan","Zora","Bogdan","Lada","Radomir","Snezana","Vuk",
        "Milena","Branko","Jasna","Stanislav","Dunja","Zoran","Vlatka","Borislav",
        "Nadia","Tomislav","Kresimir","Anja","Goran","Ljuba","Predrag","Slavica",
        "Miroslav","Ivana","Dragica","Velimir","Zlatan" };
    return d;
}
const QStringList& dark() {
    static const QStringList d = {
        "Malachar","Vorath","Nyxara","Drakthar","Mortheus","Velzaroth","Karneth",
        "Sythara","Azrath","Belmoth","Cindrel","Gravorn","Xulthar","Zarael","Morwen",
        "Nethrax","Vaelroth","Skarn","Thessil","Ombrath","Crevan","Dolmar","Erebos",
        "Vexara","Ravok","Malgrim","Tharivol","Zedrik","Korvath","Nephzar" };
    return d;
}
const QStringList& placeFantasy() {
    static const QStringList d = {
        "Eldoria","Valmoor","Brighthold","Ravenmark","Stormgard","Thornwood","Greycliff",
        "Ashfall","Silvermere","Dawnhold","Frosthelm","Duskmoor","Ironvale","Mistral",
        "Highmoor","Blackwater","Goldcrest","Stonereach","Wyrmrest","Emberfall","Westmarch",
        "Oakhaven","Shadowfen","Brackenmoor","Sunhollow","Northwatch","Grimhold","Lakeshire" };
    return d;
}
const QStringList& placeNordic() {
    static const QStringList d = {
        "Trondheim","Bergvik","Asgard","Eldnvik","Skarholm","Vidaheim","Frosthavn",
        "Ulfgard","Norvik","Hadenfjord","Ravnsfell","Stormvik","Brynholm","Eikgard",
        "Sundholm","Valdheim","Hjarvik","Drakfjord","Isgard","Solvik","Thornvik" };
    return d;
}

const QStringList& female() {
    static const QStringList d = {
        // Lusófonos
        "Sandra","Mariana","Juliana","Adriana","Helena","Beatriz","Camila","Larissa",
        "Fernanda","Gabriela","Isabela","Carolina","Leticia","Patricia","Vanessa","Bianca",
        "Renata","Daniela","Luana","Marina","Amanda","Carla","Aline","Viviane",
        // Anglo
        "Sarah","Emily","Olivia","Emma","Charlotte","Grace","Hannah","Chloe","Victoria",
        "Samantha","Jessica","Lauren","Natalie","Abigail","Sophie","Audrey","Eleanor",
        // Italianos
        "Alessandra","Cassandra","Francesca","Giulia","Chiara","Valentina","Martina",
        "Elisa","Federica","Bianca","Serena",
        // Franceses
        "Camille","Amelie","Juliette","Celine","Manon","Elodie","Margot","Sylvie","Noemie",
        // Espanhóis
        "Lucia","Sofia","Carmen","Elena","Isabel","Paloma","Ines","Pilar","Rocio",
        // Germânicos / nórdicos
        "Greta","Ingrid","Sabine","Brigitte","Astrid","Sigrid","Freya","Solveig","Heidi",
        // Eslavos
        "Natasha","Olga","Svetlana","Katarina","Milena","Vesna","Anja","Ludmila",
        // Gregos / outros
        "Athena","Daphne","Penelope","Thalia","Calliope","Layla","Yasmin","Amira","Nadia",
        "Alexandra","Lisandra","Aurora" };
    return d;
}
const QStringList& male() {
    static const QStringList d = {
        // Lusófonos
        "Lucas","Gabriel","Mateus","Rafael","Daniel","Pedro","Felipe","Bruno","Thiago",
        "Rodrigo","Gustavo","Leonardo","Vinicius","Eduardo","Marcelo","Ricardo","Fernando",
        "Carlos","Henrique","Caio","Murilo","Davi","Andre","Leandro",
        // Anglo
        "James","William","Oliver","Henry","Jack","Thomas","George","Edward","Charles",
        "Benjamin","Samuel","Nathan","Oscar","Harry","Dylan","Theodore","Walter",
        // Italianos
        "Marco","Luca","Matteo","Lorenzo","Giovanni","Alessandro","Francesco","Antonio",
        "Riccardo","Stefano","Enzo",
        // Franceses
        "Pierre","Julien","Antoine","Nicolas","Mathis","Hugo","Louis","Etienne","Olivier",
        // Espanhóis
        "Diego","Javier","Alejandro","Pablo","Mateo","Sergio","Manuel","Andres",
        // Germânicos / nórdicos
        "Hans","Klaus","Friedrich","Stefan","Felix","Otto","Wolfgang","Bjorn","Erik",
        "Lars","Sven","Magnus","Olaf","Gunnar","Axel","Finn",
        // Eslavos
        "Dimitri","Ivan","Boris","Sergei","Vladimir","Mikhail","Nikolai","Pavel",
        // Gregos / outros
        "Nikos","Stavros","Theo","Alexios","Omar","Khalid","Yusuf","Karim","Hassan",
        "Alexandre","Miguel" };
    return d;
}

const QStringList& datasetFor(const QString& id) {
    static const QStringList empty;
    if (id == QStringLiteral("female"))      return female();
    if (id == QStringLiteral("male"))        return male();
    if (id == QStringLiteral("elvish"))      return elvish();
    if (id == QStringLiteral("nordic"))      return nordic();
    if (id == QStringLiteral("latin"))       return latin();
    if (id == QStringLiteral("slavic"))      return slavic();
    if (id == QStringLiteral("dark"))        return dark();
    if (id == QStringLiteral("place_fantasy")) return placeFantasy();
    if (id == QStringLiteral("place_nordic"))  return placeNordic();
    return empty;
}

QString capitalize(const QString& s) {
    if (s.isEmpty()) return s;
    return s.left(1).toUpper() + s.mid(1);
}

QString pickStr(const QStringList& v) {
    return v.at(QRandomGenerator::global()->bounded(v.size()));
}

} // namespace

QVector<NameGenerator::Style> NameGenerator::characterStyles() {
    return {
        {QStringLiteral("real"),   QStringLiteral("Pessoa")},
        {QStringLiteral("elvish"), QStringLiteral("Élfico")},
        {QStringLiteral("nordic"), QStringLiteral("Nórdico")},
        {QStringLiteral("latin"),  QStringLiteral("Latino")},
        {QStringLiteral("slavic"), QStringLiteral("Eslavo")},
        {QStringLiteral("dark"),   QStringLiteral("Sombrio")},
    };
}

QVector<NameGenerator::Style> NameGenerator::placeStyles() {
    return {
        {QStringLiteral("place_fantasy"), QStringLiteral("Fantasia")},
        {QStringLiteral("place_nordic"),  QStringLiteral("Nórdico")},
    };
}

void NameGenerator::ensureTrained(const QString& styleId) {
    if (m_models.contains(styleId)) return;

    QVector<QHash<QString, QString>> models(kMaxOrder); // índice = ordem-1
    const QStringList& data = datasetFor(styleId);
    for (const QString& raw : data) {
        const QString w = raw.toLower();
        if (w.isEmpty()) continue;
        for (int o = 1; o <= kMaxOrder; ++o) {
            const QString padded = QString(o, QChar('^')) + w + kEnd;
            for (int i = o; i < padded.size(); ++i) {
                const QString ctx = padded.mid(i - o, o);
                models[o - 1][ctx].append(padded.at(i));
            }
        }
    }
    m_models.insert(styleId, models);
}

QString NameGenerator::markovOne(const QString& styleId, const QString& seed) {
    const auto it = m_models.constFind(styleId);
    if (it == m_models.constEnd()) return QString();
    const QVector<QHash<QString, QString>>& models = it.value();

    QString out = seed; // início plantado (já em minúsculas pelo chamador)
    for (int guard = 0; guard < 40; ++guard) {
        const QString full = QString(kMaxOrder, QChar('^')) + out;
        QChar next;
        bool found = false;
        // Back-off: tenta a maior ordem disponível pro contexto atual.
        for (int o = kMaxOrder; o >= 1; --o) {
            const QString ctx = full.right(o);
            const auto mit = models[o - 1].constFind(ctx);
            if (mit != models[o - 1].constEnd() && !mit.value().isEmpty()) {
                const QString& bag = mit.value();
                next = bag.at(QRandomGenerator::global()->bounded(bag.size()));
                found = true;
                break;
            }
        }
        if (!found || next == kEnd) break;
        out.append(next);
        if (out.size() >= 12) break;
    }
    return out;
}

QStringList NameGenerator::generate(const QString& styleId, int count,
                                    const QString& prefix, const QString& suffix) {
    ensureTrained(styleId);

    const QString pfx = prefix.trimmed().toLower();
    const QString sfx = suffix.trimmed().toLower();

    // Conjunto do dataset (lower) pra evitar "copiar" um nome de treino.
    QSet<QString> seeds;
    for (const QString& s : datasetFor(styleId)) seeds.insert(s.toLower());

    QStringList result;
    QSet<QString> seen;
    int attempts = 0;
    const int maxAttempts = count * 80 + 400;
    while (result.size() < count && attempts++ < maxAttempts) {
        QString w;
        if (sfx.isEmpty()) {
            w = markovOne(styleId, pfx);
            if (w.size() < 3 || w.size() > 14) continue;
            if (seeds.contains(w)) continue; // não "copiar" um nome de treino
        } else {
            // Garante o final: gera um começo no estilo (respeitando o prefixo,
            // se houver) e cola o sufixo pedido. Sempre produz. A faixa larga de
            // comprimento do começo (2..7) é o que dá variedade aos resultados.
            QString body = markovOne(styleId, pfx);
            const int minBody = qMax(pfx.size(), 2);
            const int target = qMax(minBody, 2 + QRandomGenerator::global()->bounded(6)); // 2..7
            if (body.size() > target) body = body.left(target);
            if (body.size() < 2) continue;
            w = body + sfx;
            if (w.size() > 18) continue;
        }
        if (seen.contains(w)) continue;
        seen.insert(w);
        result.append(capitalize(w));
    }
    return result;
}

QStringList NameGenerator::generateWeapons(int count) {
    // "Kit PT-BR" de armas. Cada palavra carrega gênero/número pra concordância
    // (adjetivo + artigo contraído do/da/dos/das). Pra outro idioma no futuro,
    // basta um kit equivalente com seus moldes (a estrutura gramatical muda).
    struct Word { QString w; bool fem; bool plural; };
    struct Adj  { QString masc; QString fem; };

    static const QVector<Word> armas = {
        {QStringLiteral("Lâmina"), true, false},  {QStringLiteral("Espada"), true, false},
        {QStringLiteral("Foice"), true, false},   {QStringLiteral("Machado"), false, false},
        {QStringLiteral("Lança"), true, false},   {QStringLiteral("Adaga"), true, false},
        {QStringLiteral("Martelo"), false, false},{QStringLiteral("Cetro"), false, false},
        {QStringLiteral("Arco"), false, false},   {QStringLiteral("Gládio"), false, false} };
    static const QVector<Adj> adjs = {
        {QStringLiteral("Sombrio"), QStringLiteral("Sombria")},
        {QStringLiteral("Flamejante"), QStringLiteral("Flamejante")}, // invariável
        {QStringLiteral("Gélido"), QStringLiteral("Gélida")},
        {QStringLiteral("Maldito"), QStringLiteral("Maldita")},
        {QStringLiteral("Sagrado"), QStringLiteral("Sagrada")},
        {QStringLiteral("Eterno"), QStringLiteral("Eterna")},
        {QStringLiteral("Silencioso"), QStringLiteral("Silenciosa")},
        {QStringLiteral("Sangrento"), QStringLiteral("Sangrenta")},
        {QStringLiteral("Ancestral"), QStringLiteral("Ancestral")} }; // invariável
    static const QVector<Word> abstratos = {
        {QStringLiteral("Crepúsculo"), false, false}, {QStringLiteral("Alvorecer"), false, false},
        {QStringLiteral("Vazio"), false, false},      {QStringLiteral("Tormento"), false, false},
        {QStringLiteral("Silêncio"), false, false},   {QStringLiteral("Trovão"), false, false},
        {QStringLiteral("Esquecimento"), false, false},{QStringLiteral("Juízo"), false, false},
        {QStringLiteral("Abismo"), false, false},     {QStringLiteral("Inverno"), false, false},
        {QStringLiteral("Tempestade"), true, false},  {QStringLiteral("Penumbra"), true, false},
        {QStringLiteral("Ruína"), true, false},       {QStringLiteral("Aurora"), true, false} };
    static const QVector<Word> alvos = {
        {QStringLiteral("Almas"), true, true},   {QStringLiteral("Sangue"), false, false},
        {QStringLiteral("Mundos"), false, true}, {QStringLiteral("Reis"), false, true},
        {QStringLiteral("Deuses"), false, true}, {QStringLiteral("Sombras"), true, true},
        {QStringLiteral("Céus"), false, true},   {QStringLiteral("Titãs"), false, true} };
    static const QStringList verbos = {
        QStringLiteral("Fende"), QStringLiteral("Bebe"), QStringLiteral("Ceifa"),
        QStringLiteral("Parte"), QStringLiteral("Devora"), QStringLiteral("Rasga"),
        QStringLiteral("Queima") };

    // Artigo contraído "de + artigo" concordando com gênero e número.
    auto contr = [](const Word& w) {
        if (w.plural) return w.fem ? QStringLiteral("das") : QStringLiteral("dos");
        return w.fem ? QStringLiteral("da") : QStringLiteral("do");
    };
    auto pickW = [](const QVector<Word>& v) -> const Word& {
        return v.at(QRandomGenerator::global()->bounded(v.size()));
    };

    QStringList result;
    QSet<QString> seen;
    int attempts = 0;
    const int maxAttempts = count * 40 + 100;
    while (result.size() < count && attempts++ < maxAttempts) {
        QString name;
        switch (QRandomGenerator::global()->bounded(4)) {
        case 0: { // Lâmina Sombria / Machado Sombrio (adjetivo concorda com a arma)
            const Word& a = pickW(armas);
            const Adj& adj = adjs.at(QRandomGenerator::global()->bounded(adjs.size()));
            name = a.w + QStringLiteral(" ") + (a.fem ? adj.fem : adj.masc);
            break; }
        case 1: { // Foice do Crepúsculo / Lança da Tempestade
            const Word& a = pickW(armas);
            const Word& ab = pickW(abstratos);
            name = a.w + QStringLiteral(" ") + contr(ab) + QStringLiteral(" ") + ab.w;
            break; }
        case 2: // Fende-Almas (sem artigo, sem concordância)
            name = pickStr(verbos) + QStringLiteral("-") + pickW(alvos).w;
            break;
        default: { // Adaga das Almas / Espada dos Deuses / Lança do Sangue
            const Word& a = pickW(armas);
            const Word& t = pickW(alvos);
            name = a.w + QStringLiteral(" ") + contr(t) + QStringLiteral(" ") + t.w;
            break; }
        }
        if (seen.contains(name)) continue;
        seen.insert(name);
        result.append(name);
    }
    return result;
}
