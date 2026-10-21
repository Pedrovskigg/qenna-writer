#include "CartoDoc.h"
#include "CartoProf.h"

#include <QJsonArray>

namespace carto {

QJsonObject Doc::layersToJson()
{
    CARTO_PROF("camadas → json");
    QJsonObject o;
    // grava o que o usuário fez (territórios e construções) sem refazer as cidades sugeridas:
    // elas saem do terreno de novo ao abrir
    if (info) {
        o.insert("realms", realmsToJson(*info));
        o.insert("user", info->user.toJson());
    }
    o.insert("labels", labels.toJson());
    QJsonArray rv;
    for (const River& r : rivers) {
        QJsonArray pts;
        for (const Pt& p : r.pts) pts.append(QJsonArray{ std::round(p[0] * 100) / 100, std::round(p[1] * 100) / 100 });
        rv.append(QJsonObject{ { "id", r.id }, { "pts", pts } });
    }
    o.insert("rivers", rv);
    o.insert("riverSeq", riverSeq);
    if (!title.isEmpty()) o.insert("title", title);
    return o;
}

void Doc::layersFromJson(const QJsonObject& o)
{
    CARTO_PROF("json → camadas");
    labels = LabelLayer::fromJson(o.value("labels").toObject());
    rivers.clear();
    for (const QJsonValue& v : o.value("rivers").toArray()) {
        const QJsonObject r = v.toObject();
        River river;
        river.id = r.value("id").toInt();
        for (const QJsonValue& q : r.value("pts").toArray()) { const QJsonArray a = q.toArray(); river.pts.push_back({ a.at(0).toDouble(), a.at(1).toDouble() }); }
        rivers.push_back(river);
    }
    riverSeq = o.value("riverSeq").toInt();
    title = o.value("title").toString();
    Info& in = ensureInfo();
    if (o.contains("user")) in.user = UserLayer::fromJson(o.value("user").toObject());
    if (o.contains("realms")) realmsFromJson(in, o.value("realms").toObject());
    recomputeRealms(world, in);
}

} // namespace carto
