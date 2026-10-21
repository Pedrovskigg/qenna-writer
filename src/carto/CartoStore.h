#pragma once

// Mapas do projeto: manifesto `mapas.json` (como o `lousas.json`) e um arquivo
// por mapa. O arquivo guarda a receita (semente, tipo, tamanho, clima) e só a
// DIFERENÇA do que o usuário mudou: ao abrir, o gerador refaz o mundo e a
// diferença é somada. Mapa gerado e nunca pintado ocupa poucos KB.

#include "CartoCore.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace carto {

struct MapMeta {
    QString id;
    QString name;
    QString file;   // relativo à raiz do projeto, ex. "mapas/<id>.qmap"
};

// o que a janela lembra de cada mapa além do mundo
struct MapViewState {
    double ang = 0.0;
    double pitch = 40.0;
    QString mode = QStringLiteral("full");   // full · atlas · ink · terrain
    QString style = QStringLiteral("rich");  // rich · mira (visual do terreno)
    double hour = 14.4;
    QJsonObject extra;                       // camadas das próximas etapas (territórios, construções, nomes)
};

class Store {
public:
    void setProjectRoot(const QString& root);
    QString projectRoot() const { return m_root; }

    // manifesto
    void loadManifest();
    void saveManifest() const;
    const QVector<MapMeta>& maps() const { return m_maps; }
    QString activeId() const { return m_activeId; }
    void setActiveId(const QString& id) { m_activeId = id; saveManifest(); }
    int indexOf(const QString& id) const;
    MapMeta createMap(const QString& name);      // registra (o arquivo nasce no primeiro save)
    void renameMap(const QString& id, const QString& name);
    void removeMap(const QString& id);

    // um mapa
    bool loadMap(const QString& id, World& world, MapViewState& view) const;
    bool saveMap(const QString& id, const World& world, const MapViewState& view) const;

private:
    QString manifestPath() const;
    QString pathOf(const QString& id) const;
    QString m_root;
    QVector<MapMeta> m_maps;
    QString m_activeId;
};

// mundo novo pela receita (gerado ou mar vazio)
World rebuildBase(int seed, int water, const QString& type, int size, const QString& climate);

} // namespace carto
