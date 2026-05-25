#pragma once

#include <QList>
#include <QString>

// Catálogo de tipos de vínculo (presets) usados pelo BondPopup. Mantém M/F
// nos pares pra UI permitir alternar; os valores armazenados em
// CharacterBond.type são strings PT-BR já resolvidas (preset escolhido OU
// texto livre), exatamente como no Mira 1 ([[bonds-mira1-persistence]]).

struct BondTypeOption {
    QString masculine;
    QString feminine;
};

struct BondTypeGroup {
    QString name;
    QList<BondTypeOption> options;
};

namespace BondTypes {

// Cores presets do color picker (mesmas do Mira 1).
QList<QString> presetColors();

// Cor padrão de bond (azul claro).
QString defaultColor();

// Grupos de tipo preset (Família, Romântico, Social, Conflito, Poder).
QList<BondTypeGroup> presetGroups();

} // namespace BondTypes
