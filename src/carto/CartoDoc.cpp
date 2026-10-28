#include "CartoDoc.h"
#include "CartoProf.h"

#include <QByteArray>
#include <QJsonArray>

#include <cstring>

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
    // fossos cavados pelos castelos: o terreno de antes e o que ficou (pra devolver quando o castelo sair)
    if (!world.moats.empty()) {
        QJsonArray ms;
        auto pack = [](const std::vector<float>& v) { return QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(v.data()), static_cast<int>(v.size() * sizeof(float))).toBase64()); };
        for (const MoatCut& m : world.moats)
            ms.append(QJsonObject{ { "id", m.id }, { "key", m.key }, { "x0", m.x0 }, { "y0", m.y0 }, { "x1", m.x1 }, { "y1", m.y1 }, { "before", pack(m.before) }, { "cut", pack(m.cut) } });
        o.insert("moats", ms);
    }
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
    world.moats.clear();
    for (const QJsonValue& v : o.value("moats").toArray()) {
        const QJsonObject m = v.toObject();
        MoatCut c;
        c.id = m.value("id").toString(); c.key = m.value("key").toString();
        c.x0 = m.value("x0").toInt(); c.y0 = m.value("y0").toInt(); c.x1 = m.value("x1").toInt(); c.y1 = m.value("y1").toInt();
        auto unpack = [](const QJsonValue& s) { const QByteArray b = QByteArray::fromBase64(s.toString().toLatin1()); std::vector<float> out(b.size() / sizeof(float)); memcpy(out.data(), b.constData(), out.size() * sizeof(float)); return out; };
        c.before = unpack(m.value("before")); c.cut = unpack(m.value("cut"));
        const size_t n = static_cast<size_t>(std::max(0, c.x1 - c.x0)) * std::max(0, c.y1 - c.y0);
        if (c.before.size() == n && c.cut.size() == n && c.x1 <= world.W && c.y1 <= world.H) world.moats.push_back(std::move(c));
    }
    Info& in = ensureInfo();
    if (o.contains("user")) in.user = UserLayer::fromJson(o.value("user").toObject());
    if (o.contains("realms")) realmsFromJson(in, o.value("realms").toObject());
    recomputeRealms(world, in);
}

} // namespace carto
