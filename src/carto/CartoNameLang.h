#pragma once

// Os nomes gerados (mares, serras, florestas...) saem no idioma do app: cada
// língua tem seus modelos e suas palavras, com gênero e número concordando
// ("Serra Rubra", "Sierra Bermeja", "Catena Vermiglia", "Chaîne Vermeille").
// O idioma é o da tradução carregada (a string "pt" traduzida pra en/es/it/fr).

#include <QHash>
#include <QString>
#include <QStringList>

namespace carto {

struct NameLang {
    QString code;                           // pt · en · es · it · fr
    QHash<QString, QStringList> templates;  // por tipo (ocean, sea, rangeL...)
    QHash<QString, QStringList> words;      // m f mp fp Nm Nf Nmp Nfp
};

const NameLang& nameLang();
QString nameLanguage();                     // código do idioma dos nomes gerados
QString polishName(const QString& s);       // acertos da língua (francês: "de Astal" → "d'Astal")

} // namespace carto
