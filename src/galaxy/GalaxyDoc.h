#pragma once

// A galáxia aberta: o que o gerador fez, o que o autor pôs por cima (nomes, territórios, grupos,
// construções, rótulos) e o esquema de cada sistema no mapa. Guardada num arquivo .qgal por mapa:
// a receita mais a diferença; ao abrir, o gerador refaz o resto.

#include "GalaxyCore.h"
#include "GalaxyNames.h"

#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <vector>

namespace galaxy {

struct Region {
    QString key, name, color;
    QSet<int> systems;
    // Autopopular: os mundos destas classes ganham gente e construções ao entrar no território (bit n = classe n).
    bool autoPop = false;
    int popMask = (1 << 1) | (1 << 2);
    // Criação Livre: a forma pintada à mão, em carimbos (x, y e raio no espaço do mundo)
    std::vector<std::array<double, 3>> stamps;
};
// O território tem forma no mapa: algum sistema à vista ou a forma pintada à mão.
inline bool regionShown(const Region& r, const Galaxy& g)
{
    if (!r.stamps.empty()) return true;
    for (int id : r.systems) if (id >= 0 && id < int(g.systems.size()) && !g.systems[size_t(id)]->gone) return true;
    return false;
}
struct Group { QString key, name, kind, parent, form; };   // kind: "Lugar" ou "Social"; form: Cidade, Região, Palácio...
// O documento mora no projeto; aqui fica só a ligação com o lugar do mapa.
struct DocLink {
    int id = 0;
    QString owner, title, text, html;
    bool gaveta = false;
    QString drawer;      // gaveta
    QString itemId;      // item da gaveta, no projeto
    QString placeId, nodeId;   // nó no Criador de Mundos
    int parent = 0;      // Criação Livre: documento dentro de outro documento (o id dele); 0 = direto no lugar
};
struct Build {
    QString key;
    std::string type;
    uint32_t seed = 0;
    double scale = 1;
    QString size;
    double rot0 = 0, spinU = 0, pitchU = 0, a0 = 0, w = 0, rr = 0, rp = 0;
    int sys = -1;    // -1: solta no espaço
    int host = -1;   // -1: em volta da estrela
    bool deep = false;
    double deepX = 0, deepY = 0;
    double flash = 0;
};
struct FreeLabel { QString key; double x = 0, y = 0; QString text; };

enum class RefKind { None, Galaxy, Region, Group, Build, System, Comet, Belt, Planet, Moon };
struct Ref {
    RefKind kind = RefKind::None;
    System* sys = nullptr;
    Body* body = nullptr;
    Body* moon = nullptr;
    Body* comet = nullptr;
    Belt* belt = nullptr;
    Region* region = nullptr;
    Group* group = nullptr;
    Build* build = nullptr;
    Body* host = nullptr;
    explicit operator bool() const { return kind != RefKind::None; }
};

struct GalOpts {
    uint32_t seed = 4207;
    Style style = Style::Espiral;
    Size size = Size::Media;
    int arms = 4;
    double life = 0.3;
    int count = 0;
    QString preset;
    std::vector<double> mix;   // quanto de cada classe, de 1 a 6; vazio = o que o sorteio der
    bool organic = false;      // Semeação Orgânica: os sistemas aparecem conforme os territórios crescem
    bool free = false;         // Criação Livre: a galáxia nasce vazia e só existe o que o autor cria
};

constexpr double SPEED_PLANET = TAU / 240, SPEED_MOON = TAU / 90, SPEED_SPIN = TAU / 60, SPEED_BINARY = TAU / 120, SPEED_PULSAR = TAU / 24, SPEED_BELT = TAU / 900, SPEED_COMET = TAU / 700, SPEED_SSTAR = TAU / 300;

class Doc {
public:
    GalOpts opts;
    std::unique_ptr<Galaxy> gal;
    QImage galImg;
    std::unique_ptr<NameGen> names;
    QHash<QString, QString> genNames;
    // o que o autor pôs
    QHash<QString, QString> userNames;
    std::vector<std::unique_ptr<Region>> regions;
    std::vector<std::unique_ptr<Group>> groups;
    std::vector<std::unique_ptr<DocLink>> docs;
    std::vector<std::unique_ptr<Build>> builds;
    std::vector<FreeLabel> labels;
    QSet<QString> hidden;
    QHash<QString, QString> placeIds;  // lugar da galáxia → Lugar do Criador de Mundos (conexões e personagens)
    int seq = 1;
    std::vector<int> addedBatches;     // "Mais sistemas": cada chamada, pra refazer igual
    uint32_t rev = 0;                  // sobe a cada mudança que os caches precisam ver
    // Semeação Orgânica: a galáxia existe inteira, mas estes sistemas ainda não apareceram. Eles usam a mesma marca
    // do sistema removido (gone), que o mapa, a busca e as contas já respeitam; este conjunto é o que os diferencia.
    QSet<int> veiled;
    QSet<int> autoSys;                 // sistemas cuja única edição veio do programa: o Autopopular ainda pode mexer
    bool isVeiled(const System& s) const { return veiled.contains(s.id); }
    void veil(System& s) { veiled.insert(s.id); s.gone = true; rev++; }
    void reveal(System& s) { if (veiled.remove(s.id)) { s.gone = false; rev++; } }
    void setOrganic(bool on);
    void restoreVeil(bool organic, const QSet<int>& ids);
    QSet<int> organicKeep() const;     // o que a história já tocou: nunca é escondido
    double simT = 0;

    // Criação Livre
    // Mundo solto: mora num sistema feito à mão só pra ele, que o mapa não mostra até o autor dar um lugar pro mundo.
    QSet<int> solo;
    bool isSolo(const System& s) const { return solo.contains(s.id); }
    QSet<QString> ghosts;              // o que o Gerar sugeriu e ainda espera o sim (tracejado)
    QSet<QString> capitals;            // mundo: capital do território; lugar ou construção: capital de onde está
    QHash<QString, QHash<QString, QString>> fichas, fichasGen;   // a ficha técnica que o autor escreve; a gerada, até ele aceitar
    QHash<QString, QString> linkTypes; // vínculo do Criador → tipo da conexão (Aliança, Rota...)
    System* addHandSystem(double x, double y, StarKind kind, uint32_t seed);
    bool freeSpot(const Region* r, double& x, double& y, uint32_t seed) const;   // onde nasce um sistema novo
    // O mundo mudou de sistema: tudo que apontava pra chave velha (nome, documentos, lugares, peças) passa pra nova.
    void rekey(const QString& from, const QString& to);

    // Gera de novo pela receita. keepUser = false limpa o que o autor pôs.
    void generate(const GalOpts& o);
    static QImage galaxyImage(const Galaxy& g, int T);

    Ref resolve(const QString& key) const;
    QString labelOf(const QString& key);
    QString genName(const QString& key);
    QString parentOf(const QString& key) const;
    QStringList pathOf(const QString& key) const;
    std::vector<DocLink*> docsOf(const QString& k) const;
    std::vector<Group*> groupsOf(const QString& k) const;
    Region* regionOfSys(const System& s) const;
    bool touched(const QString& k) const;
    std::vector<Build*> buildsIn(const System& s) const;
    Build* addBuild(std::unique_ptr<Build> b);
    std::unique_ptr<Build> dropBuild(Build* b);
    Region* newRegion(const QString& name, const QString& color, const std::vector<System*>& systems);
    Group* newGroup(const QString& parent, const QString& name, const QString& kind);

    // o esquema
    void layoutSystems();
    void layoutSystem(System& s);
    void place(System& s) const;
    double toR(const System& s, double au) const;
    double fromR(const System& s, double rr) const;

    // arquivo
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& o);

    // a galáxia de exemplo do Lab não vem pro app: galáxia pronta entra pelo criador
    void applyPreset();
};

std::vector<Body*> planetsOf(const System& s);
int livingCount(const System& s);
inline QString keyS(const System& s) { return QStringLiteral("S%1").arg(s.id); }
inline QString keyP(const System& s, const Body& p) { return QStringLiteral("P%1.%2").arg(s.id).arg(p.uid ? p.uid : p.idx); }
inline QString keyM(const System& s, const Body& p, const Body& m) { return QStringLiteral("M%1.%2.%3").arg(s.id).arg(p.uid ? p.uid : p.idx).arg(m.uid ? m.uid : m.idx); }
std::array<int, 3> sysColor(const System& s);

} // namespace galaxy
