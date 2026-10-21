#pragma once

// Nomes do relevo (porte do Map Lab): o terreno sugere o que merece nome e onde
// o nome vai (oceano e mares no meio da água aberta, baías, lagos, serras pela
// linha da crista, florestas, desertos, pântanos, gelo, campos, ilhas,
// arquipélagos, picos e os rios pintados). O nome nasce da POSIÇÃO, então
// pintar longe não rebatiza nada. O usuário troca, move, esconde ou cria os seus.

#include "CartoInfo.h"

#include <QJsonObject>

#include <vector>

namespace carto {

struct River { int id = 0; Path pts; };

struct NameFeature {
    QString kind;      // ocean sea bay lake range peak forest desert ice marsh plains island isles river
    double x = 0, y = 0, r = 0;
    int prio = 0;
    Path pts;          // serra e rio: a linha que o nome segue
    QString tpl;       // serra: rangeL · range · rangeS
    int rid = -1;      // rio pintado
    QString name, id;
};

// edição de um nome sugerido (presa ao acidente do mesmo tipo mais perto)
struct NameEdit {
    QString id, kind;
    double x = 0, y = 0;
    QString name;      // vazio = o sugerido
    double dx = 0, dy = 0, size = 0;   // size 0 = 100%
    bool hidden = false;
};
// nome livre do usuário
struct NameCustom {
    QString id, text, style = QStringLiteral("land");   // land · water
    double size = 1, x = 0, y = 0, r = 90;
    Path pts;          // arrastou: o nome segue o traço
};
struct LabelLayer {
    std::vector<NameEdit> edits;
    std::vector<NameCustom> custom;
    int seq = 0;
    QJsonObject toJson() const;
    static LabelLayer fromJson(const QJsonObject& o);
    bool operator==(const LabelLayer& o) const;
};

// density: few · normal · many
std::vector<NameFeature> buildNames(World& w, const std::vector<River>& rivers, const QString& density);

// o que vai pra tela: sugestões (com as edições) + nomes livres
struct Label {
    QString ref;
    bool autoName = true;
    int feature = -1, edit = -1, custom = -1;
    QString kind, text;
    double x = 0, y = 0, r = 0, size = 1;
    Path pts;
    bool hidden = false;
    bool touched = false;  // o usuário trocou, moveu ou mudou o tamanho
    int prio = 0;
    bool forced() const { return !autoName || kind == QLatin1String("river") || touched; }   // nunca cede lugar
};
std::vector<Label> resolveNames(const std::vector<NameFeature>& F, const LabelLayer& L, bool autoOn);
int editFor(LabelLayer& L, const std::vector<NameFeature>& F, Label& lab);   // cria a edição se ainda não tem
QString nameKindLabel(const QString& kind);

} // namespace carto
