#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

// Gerador de nomes do Pensário — 100% offline, sem dependências.
// - Markov char-level multi-ordem (back-off 3→2→1) treinado em pequenos
//   datasets por estilo cultural; inventa nomes "no jeitão" do estilo.
// - Padrões/templates para nomes estruturados (armas e títulos).
class NameGenerator {
public:
    struct Style { QString id; QString label; };

    NameGenerator() = default;

    // Estilos disponíveis (id + rótulo) por finalidade.
    static QVector<Style> characterStyles();
    static QVector<Style> placeStyles();

    // Gera `count` nomes Markov no estilo dado (personagens/lugares).
    // prefix: força o início do nome (plantado no gerador). suffix: filtra os
    // que terminam assim (por rejeição). Ambos opcionais, case-insensitive.
    QStringList generate(const QString& styleId, int count,
                         const QString& prefix = QString(),
                         const QString& suffix = QString());

    // Gera `count` nomes de arma/título por padrões.
    QStringList generateWeapons(int count);

private:
    void ensureTrained(const QString& styleId);
    QString markovOne(const QString& styleId, const QString& seed = QString());

    // styleId -> [modelo ordem1, ordem2, ordem3]; cada modelo: contexto -> "bag"
    // de próximos caracteres (cada char repetido = peso).
    QHash<QString, QVector<QHash<QString, QString>>> m_models;
};
