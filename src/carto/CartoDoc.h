#pragma once

// O mapa inteiro: o mundo (relevo, umidade, frio) e as camadas do usuário por
// cima dele (territórios, construções, nomes, rios pintados, título). As
// cidades e estradas sugeridas e a vegetação são refeitas do terreno quando
// ele muda; o que o usuário fez é preservado.

#include "CartoInfo.h"
#include "CartoNames.h"

#include <QJsonObject>

#include <algorithm>
#include <memory>

namespace carto {

struct Doc {
    World world;
    std::unique_ptr<Info> info;
    int infoVer = -1;
    LabelLayer labels;
    std::vector<River> rivers;
    int riverSeq = 0;
    QString title;              // vazio = o sugerido ("Terras de X")
    QString groupId;            // Explorar: o grupo do mapa inteiro (a raiz da árvore; os territórios moram dentro)
    int ver = 0;                // sobe a cada mudança no terreno
    // caches
    std::vector<Flora> flora;
    int floraVer = -1;
    double floraScale = 0;
    std::vector<NameFeature> names;
    QString namesKey;

    void touch() { ver++; }
    QString displayTitle() const { return title.isEmpty() ? mapTitle(world) : title; }

    // camada de informações em dia com o terreno (cidades e estradas sugeridas
    // refeitas; territórios e construções preservados)
    Info& ensureInfo()
    {
        if (info && infoVer == ver) return *info;
        std::unique_ptr<Info> fresh = buildInfo(world, double(world.W) * world.H > 3.5e6 ? 5 : 3);
        if (info) {
            fresh->user = std::move(info->user);
            if (info->barrier.size() == fresh->barrier.size()) { fresh->defs = std::move(info->defs); fresh->barrier = std::move(info->barrier); }
            recomputeRealms(world, *fresh);
        }
        info = std::move(fresh);
        infoVer = ver;
        return *info;
    }
    const std::vector<Flora>& ensureFlora()
    {
        if (floraVer != ver || floraScale != worldScale(world)) { flora = buildFlora(world); floraVer = ver; floraScale = worldScale(world); }
        return flora;
    }
    const std::vector<NameFeature>& ensureNames(const QString& density)
    {
        const QString key = QStringLiteral("%1|%2|%3").arg(ver).arg(density).arg(rivers.size());
        if (namesKey != key) { names = buildNames(world, rivers, density); namesKey = key; }
        return names;
    }

    // mapa só de terreno: sem reinos, cidades, estradas e nomes sugeridos (o usuário cria os dele)
    void clearSuggestions()
    {
        Info& in = ensureInfo();
        in.defs.clear();
        std::fill(in.barrier.begin(), in.barrier.end(), uint8_t(0));
        in.user.showGen = false;
        recomputeRealms(world, in);
        labels.autoOn = false;
    }
    // há algo pra vista Completo mostrar: território, construção, nome ou sugestão ligada
    bool hasContent() const
    {
        if (labels.autoOn || !labels.custom.empty()) return true;
        if (!info) return false;
        const UserLayer& U = info->user;
        return U.showGen || !info->defs.empty() || !U.arts.empty() || !U.houses.empty() || !U.walls.empty()
            || !U.fields.empty() || !U.bridges.empty() || !U.roads.empty();
    }

    // ── camadas pra gravar e pra desfazer ──
    struct Layers {
        RealmState realms;
        bool hasRealms = false;
        UserLayer user;
        LabelLayer labels;
        std::vector<River> rivers;
        int riverSeq = 0;
        QString title;
    };
    Layers layers() const
    {
        Layers L;
        if (info) { L.realms = info->realmState(); L.hasRealms = true; L.user = info->user; }
        L.labels = labels; L.rivers = rivers; L.riverSeq = riverSeq; L.title = title;
        return L;
    }
    void setLayers(const Layers& L)
    {
        labels = L.labels; rivers = L.rivers; riverSeq = L.riverSeq; title = L.title;
        Info& in = ensureInfo();
        in.user = L.user;
        if (L.hasRealms) setRealmState(world, in, L.realms);
        else recomputeRealms(world, in);
    }

    QJsonObject layersToJson();
    void layersFromJson(const QJsonObject& o);
};

} // namespace carto
