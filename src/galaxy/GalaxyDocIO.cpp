// O arquivo .qgal: a receita da galáxia mais a diferença. Ao abrir, o gerador refaz tudo pela semente;
// só o que o autor mexeu vem do arquivo. Sistema editado vai inteiro, com cada corpo e o visual dele.

#include "GalaxyDoc.h"
#include "GalaxyPresets.h"

#include <QJsonArray>
#include <QJsonObject>

namespace galaxy {

namespace {
QJsonArray v3ToJson(const V3& v) { return QJsonArray { v[0], v[1], v[2] }; }
V3 v3From(const QJsonValue& j)
{
    const QJsonArray a = j.toArray();
    return a.size() == 3 ? V3 { a[0].toDouble(), a[1].toDouble(), a[2].toDouble() } : V3 {};
}
QJsonArray rgbToJson(const std::array<int, 3>& v) { return QJsonArray { v[0], v[1], v[2] }; }
std::array<int, 3> rgbFrom(const QJsonValue& j)
{
    const QJsonArray a = j.toArray();
    return a.size() == 3 ? std::array<int, 3> { a[0].toInt(), a[1].toInt(), a[2].toInt() } : std::array<int, 3> { 255, 255, 255 };
}
QString str(const std::string& s) { return QString::fromStdString(s); }
std::string stdStr(const QJsonValue& j) { return j.toString().toStdString(); }

QJsonObject lookToJson(const Look& L)
{
    QJsonObject o;
    o["seed"] = double(L.seed); o["pop"] = L.pop; o["style"] = QString::fromLatin1(lookStyleKey(L.style));
    o["artificial"] = L.artificial; o["tone"] = L.tone; o["hasDish"] = L.hasDish; o["dish"] = v3ToJson(L.dish);
    QJsonArray pal;
    for (const PalStop& s : L.pal) pal.push_back(QJsonArray { s.t, s.c[0], s.c[1], s.c[2] });
    o["pal"] = pal;
    o["bandsF"] = L.bandsF; o["turb"] = L.turb; o["cap"] = L.cap; o["canyon"] = L.canyon;
    QJsonArray storms;
    for (const Storm& s : L.storms) storms.push_back(QJsonArray { s.lat, s.lon, s.w, s.h, s.c[0], s.c[1], s.c[2] });
    o["storms"] = storms;
    o["hasFreq"] = L.hasFreq; o["freq"] = L.freq; o["sea"] = L.sea; o["dryBias"] = L.dryBias; o["dryBase"] = L.dryBase; o["belt"] = L.belt; o["snowAt"] = L.snowAt; o["cloud"] = L.cloud;
    o["hasDryBase"] = L.hasDryBase; o["swirl"] = L.swirl;
    o["shallow"] = v3ToJson(L.shallow); o["deep"] = v3ToJson(L.deep); o["veg"] = v3ToJson(L.veg); o["dry"] = v3ToJson(L.dry); o["rock"] = v3ToJson(L.rock); o["cloudCol"] = v3ToJson(L.cloudCol); o["ice"] = v3ToJson(L.ice); o["murk"] = v3ToJson(L.murk);
    o["vegKey"] = QString::fromLatin1(vegKey(L.vegKeyV)); o["hasSub"] = L.hasSub; o["sub"] = L.sub;
    o["hasRock"] = L.hasRock; o["tint"] = v3ToJson(L.tint); o["line"] = v3ToJson(L.line); o["lo"] = v3ToJson(L.lo); o["hi"] = v3ToJson(L.hi); o["ac"] = v3ToJson(L.ac);
    o["craters"] = L.craters; o["nCr"] = L.nCr; o["cseed"] = L.cseed; o["bandsI"] = L.bandsI;
    o["k1"] = L.k1; o["thr"] = L.thr; o["cells"] = L.cells; o["fresh"] = L.fresh; o["frost"] = L.frost; o["plainR"] = L.plainR; o["cellF"] = L.cellF;
    QJsonArray volc;
    for (const auto& v : L.volc) volc.push_back(QJsonArray { v[0], v[1], v[2], v[3] });
    o["volc"] = volc;
    QJsonArray bc;
    for (const V3& c : L.bandCols) bc.push_back(v3ToJson(c));
    o["bandCols"] = bc;
    o["plain"] = v3ToJson(L.plain); o["crack"] = L.crack; o["pool"] = L.pool;
    return o;
}
void lookFromJson(const QJsonObject& o, Look& L)
{
    L.seed = uint32_t(o["seed"].toDouble()); L.pop = o["pop"].toDouble();
    lookStyleFromKey(stdStr(o["style"]), L.style);
    L.artificial = o["artificial"].toBool(); L.tone = o["tone"].toInt(); L.hasDish = o["hasDish"].toBool(); L.dish = v3From(o["dish"]);
    L.pal.clear();
    for (const QJsonValue& v : o["pal"].toArray()) { const QJsonArray a = v.toArray(); if (a.size() == 4) L.pal.push_back({ a[0].toDouble(), V3 { a[1].toDouble(), a[2].toDouble(), a[3].toDouble() } }); }
    L.bandsF = o["bandsF"].toDouble(); L.turb = o["turb"].toDouble(); L.cap = o["cap"].toDouble(); L.canyon = o["canyon"].toBool();
    L.storms.clear();
    for (const QJsonValue& v : o["storms"].toArray()) { const QJsonArray a = v.toArray(); if (a.size() == 7) L.storms.push_back({ a[0].toDouble(), a[1].toDouble(), a[2].toDouble(), a[3].toDouble(), V3 { a[4].toDouble(), a[5].toDouble(), a[6].toDouble() } }); }
    L.hasFreq = o["hasFreq"].toBool(); L.freq = o["freq"].toDouble(); L.sea = o["sea"].toDouble(); L.dryBias = o["dryBias"].toDouble(); L.dryBase = o["dryBase"].toDouble(); L.belt = o["belt"].toDouble(); L.snowAt = o["snowAt"].toDouble(); L.cloud = o["cloud"].toDouble();
    L.hasDryBase = o["hasDryBase"].toBool(); L.swirl = o["swirl"].toBool();
    L.shallow = v3From(o["shallow"]); L.deep = v3From(o["deep"]); L.veg = v3From(o["veg"]); L.dry = v3From(o["dry"]); L.rock = v3From(o["rock"]); L.cloudCol = v3From(o["cloudCol"]); L.ice = v3From(o["ice"]); L.murk = v3From(o["murk"]);
    const std::string vk = stdStr(o["vegKey"]);
    for (Veg v : { Veg::None, Veg::Verde, Veg::Selva, Veg::Oliva, Veg::Carmim, Veg::Roxa, Veg::Azulada, Veg::Outono, Veg::Escura }) if (vk == vegKey(v)) L.vegKeyV = v;
    L.hasSub = o["hasSub"].toBool(); L.sub = o["sub"].toDouble();
    L.hasRock = o["hasRock"].toBool(); L.tint = v3From(o["tint"]); L.line = v3From(o["line"]); L.lo = v3From(o["lo"]); L.hi = v3From(o["hi"]); L.ac = v3From(o["ac"]);
    L.craters = o["craters"].toInt(); L.nCr = o["nCr"].toInt(); L.cseed = o["cseed"].toInt(); L.bandsI = o["bandsI"].toInt();
    L.k1 = o["k1"].toDouble(); L.thr = o["thr"].toDouble(); L.cells = o["cells"].toDouble(); L.fresh = o["fresh"].toDouble(); L.frost = o["frost"].toDouble(); L.plainR = o["plainR"].toDouble(); L.cellF = o["cellF"].toDouble();
    L.volc.clear();
    for (const QJsonValue& v : o["volc"].toArray()) { const QJsonArray a = v.toArray(); if (a.size() == 4) L.volc.push_back({ a[0].toDouble(), a[1].toDouble(), a[2].toDouble(), a[3].toDouble() }); }
    L.bandCols.clear();
    for (const QJsonValue& v : o["bandCols"].toArray()) L.bandCols.push_back(v3From(v));
    L.plain = v3From(o["plain"]); L.crack = o["crack"].toDouble(); L.pool = o["pool"].toDouble();
}
QJsonObject starToJson(const Star& s)
{
    QJsonObject o;
    o["kind"] = QString::fromLatin1(starKindKey(s.kind)); o["label"] = str(s.label); o["spec"] = str(s.spec);
    o["mass"] = s.mass; o["radius"] = s.radius; o["lum"] = s.lum; o["temp"] = s.temp; o["age"] = s.age; o["radiusKm"] = s.radiusKm; o["period"] = s.period;
    o["color"] = rgbToJson(s.color);
    o["nebula"] = s.nebula; o["pulsar"] = s.pulsar; o["remnant"] = s.remnant; o["disk"] = s.disk;
    return o;
}
Star starFrom(const QJsonObject& o)
{
    Star s;
    starKindFromKey(stdStr(o["kind"]), s.kind);
    s.label = stdStr(o["label"]); s.spec = stdStr(o["spec"]);
    s.mass = o["mass"].toDouble(); s.radius = o["radius"].toDouble(); s.lum = o["lum"].toDouble(); s.temp = o["temp"].toDouble(); s.age = o["age"].toDouble(); s.radiusKm = o["radiusKm"].toDouble(); s.period = o["period"].toDouble();
    s.color = rgbFrom(o["color"]);
    s.nebula = o["nebula"].toBool(); s.pulsar = o["pulsar"].toBool(); s.remnant = o["remnant"].toBool(); s.disk = o["disk"].toBool();
    return s;
}
int bodyKindInt(BodyKind k) { return int(k); }
int clsInt(Cls c) { return int(c); }
int hzInt(Hz h) { return int(h); }
QJsonObject bodyToJson(const Body& b)
{
    QJsonObject o;
    o["body"] = bodyKindInt(b.body); o["type"] = QString::fromLatin1(typeKey(b.type)); o["cls"] = clsInt(b.cls); o["idx"] = b.idx; o["uid"] = b.uid;
    o["a"] = b.a; o["e"] = b.e; o["w"] = b.w; o["M0"] = b.M0; o["P"] = b.P; o["mass"] = b.mass; o["radius"] = b.radius; o["gravity"] = b.gravity; o["density"] = b.density; o["albedo"] = b.albedo;
    o["teq"] = b.teq; o["temp"] = b.temp; o["dayH"] = b.dayH; o["tilt"] = b.tilt; o["az"] = b.az;
    o["atm"] = str(b.atm);
    o["water"] = b.water; o["locked"] = b.locked; o["retro"] = b.retro; o["wantHabMoon"] = b.wantHabMoon; o["hab"] = b.hab; o["hz"] = hzInt(b.hz);
    o["look"] = lookToJson(b.look);
    o["frame"] = QJsonArray { v3ToJson(b.frame.ax), v3ToJson(b.frame.e1), v3ToJson(b.frame.e2) };
    if (b.rings) {
        QJsonObject r;
        r["inner"] = b.rings->inner; r["outer"] = b.rings->outer; r["alpha"] = b.rings->alpha; r["tint"] = v3ToJson(b.rings->tint);
        QJsonArray bands;
        for (float f : b.rings->bands) bands.push_back(double(f));
        r["bands"] = bands;
        o["rings"] = r;
    }
    if (!b.moons.empty()) { QJsonArray ms; for (const auto& m : b.moons) ms.push_back(bodyToJson(*m)); o["moons"] = ms; }
    o["dist"] = b.dist; o["radiusKm"] = b.radiusKm; o["massKg"] = b.massKg;
    if (b.star) o["star"] = starToJson(*b.star);
    o["people"] = b.people;
    if (b.air) o["air"] = b.air;
    if (b.habSet) o["habSet"] = b.habSet;
    o["layA0"] = b.layA0; o["layW"] = b.layW; o["layHasW"] = b.layHasW; o["layHasA0"] = b.layHasA0;
    o["spinU"] = b.spinU; o["pitchU"] = b.pitchU; o["spun"] = b.spunByUser;
    return o;
}
std::unique_ptr<Body> bodyFrom(const QJsonObject& o)
{
    auto b = std::make_unique<Body>();
    b->body = BodyKind(o["body"].toInt()); typeFromKey(stdStr(o["type"]), b->type); b->cls = Cls(o["cls"].toInt()); b->idx = o["idx"].toInt(); b->uid = o["uid"].toInt();
    b->a = o["a"].toDouble(); b->e = o["e"].toDouble(); b->w = o["w"].toDouble(); b->M0 = o["M0"].toDouble(); b->P = o["P"].toDouble(); b->mass = o["mass"].toDouble(); b->radius = o["radius"].toDouble(); b->gravity = o["gravity"].toDouble(); b->density = o["density"].toDouble(); b->albedo = o["albedo"].toDouble();
    b->teq = o["teq"].toDouble(); b->temp = o["temp"].toDouble(); b->dayH = o["dayH"].toDouble(); b->tilt = o["tilt"].toDouble(); b->az = o["az"].toDouble();
    b->atm = stdStr(o["atm"]);
    b->water = o["water"].toBool(); b->locked = o["locked"].toBool(); b->retro = o["retro"].toBool(); b->wantHabMoon = o["wantHabMoon"].toBool(); b->hab = o["hab"].toBool(); b->hz = Hz(o["hz"].toInt());
    lookFromJson(o["look"].toObject(), b->look);
    const QJsonArray fr = o["frame"].toArray();
    if (fr.size() == 3) b->frame = Frame { v3From(fr[0]), v3From(fr[1]), v3From(fr[2]) };
    if (o.contains("rings")) {
        const QJsonObject r = o["rings"].toObject();
        b->rings = std::make_unique<Rings>();
        b->rings->inner = r["inner"].toDouble(); b->rings->outer = r["outer"].toDouble(); b->rings->alpha = r["alpha"].toDouble(); b->rings->tint = v3From(r["tint"]);
        const QJsonArray bands = r["bands"].toArray();
        for (int i = 0; i < std::min(48, int(bands.size())); i++) b->rings->bands[size_t(i)] = float(bands[i].toDouble());
    }
    for (const QJsonValue& m : o["moons"].toArray()) b->moons.push_back(bodyFrom(m.toObject()));
    b->dist = o["dist"].toDouble(); b->radiusKm = o["radiusKm"].toDouble(); b->massKg = o["massKg"].toDouble();
    if (o.contains("star")) b->star = std::make_unique<Star>(starFrom(o["star"].toObject()));
    b->people = o["people"].toDouble();
    b->air = o["air"].toInt(); b->habSet = o["habSet"].toInt();
    b->layA0 = o["layA0"].toDouble(); b->layW = o["layW"].toDouble(); b->layHasW = o["layHasW"].toBool(); b->layHasA0 = o["layHasA0"].toBool();
    b->spinU = o["spinU"].toDouble(); b->pitchU = o["pitchU"].toDouble(0.3); b->spunByUser = o["spun"].toBool();
    return b;
}
QJsonObject systemToJson(const System& s)
{
    QJsonObject o;
    o["id"] = s.id; o["x"] = s.x; o["y"] = s.y; o["z"] = s.z; o["seed"] = double(s.seed); o["isCore"] = s.isCore; o["hand"] = s.hand;
    o["young"] = s.young; o["rCore"] = s.rCore; o["kind"] = QString::fromLatin1(starKindKey(s.kind));
    QJsonArray stars;
    for (const Star& st : s.stars) stars.push_back(starToJson(st));
    o["stars"] = stars;
    o["sep"] = s.sep; o["binP"] = s.binP; o["M"] = s.M; o["L"] = s.L; o["age"] = s.age; o["life"] = s.life; o["label"] = str(s.label);
    o["hasHz"] = s.hasHz; o["hasFrost"] = s.hasFrost; o["hz0"] = s.hz[0]; o["hz1"] = s.hz[1]; o["frost"] = s.frost; o["frostEff"] = s.frostEff; o["amax"] = s.amax;
    QJsonArray bodies;
    for (const auto& b : s.bodies) bodies.push_back(bodyToJson(*b));
    o["bodies"] = bodies;
    QJsonArray belts;
    for (const Belt& b : s.belts) belts.push_back(QJsonArray { b.kuiper ? 1 : 0, b.a0, b.a1, b.n, b.uid });
    o["belts"] = belts;
    o["planetCount"] = s.planetCount;
    return o;
}
void systemFrom(const QJsonObject& o, System& s)
{
    s.x = o["x"].toDouble(); s.y = o["y"].toDouble(); s.z = o["z"].toDouble(); s.seed = uint32_t(o["seed"].toDouble()); s.isCore = o["isCore"].toBool(); s.hand = o["hand"].toBool();
    s.young = o["young"].toDouble(); s.rCore = o["rCore"].toDouble(); starKindFromKey(stdStr(o["kind"]), s.kind);
    s.stars.clear();
    for (const QJsonValue& v : o["stars"].toArray()) s.stars.push_back(starFrom(v.toObject()));
    s.sep = o["sep"].toDouble(); s.binP = o["binP"].toDouble(); s.M = o["M"].toDouble(); s.L = o["L"].toDouble(); s.age = o["age"].toDouble(); s.life = o["life"].toDouble(); s.label = stdStr(o["label"]);
    s.hasHz = o["hasHz"].toBool(); s.hasFrost = o["hasFrost"].toBool(); s.hz[0] = o["hz0"].toDouble(); s.hz[1] = o["hz1"].toDouble(); s.frost = o["frost"].toDouble(); s.frostEff = o["frostEff"].toDouble(); s.amax = o["amax"].toDouble();
    s.bodies.clear();
    for (const QJsonValue& v : o["bodies"].toArray()) s.bodies.push_back(bodyFrom(v.toObject()));
    s.belts.clear();
    for (const QJsonValue& v : o["belts"].toArray()) { const QJsonArray a = v.toArray(); if (a.size() == 5) { Belt b; b.kuiper = a[0].toInt() != 0; b.a0 = a[1].toDouble(); b.a1 = a[2].toDouble(); b.n = a[3].toInt(); b.uid = a[4].toInt(); s.belts.push_back(b); } }
    s.planetCount = o["planetCount"].toInt();
    s.edited = true;
}
QJsonObject buildToJson(const Build& b)
{
    QJsonObject o;
    o["key"] = b.key; o["type"] = str(b.type); o["seed"] = double(b.seed); o["scale"] = b.scale; o["size"] = b.size;
    o["rot0"] = b.rot0; o["spinU"] = b.spinU; o["pitchU"] = b.pitchU; o["a0"] = b.a0; o["w"] = b.w; o["rr"] = b.rr; o["rp"] = b.rp;
    o["sys"] = b.sys; o["host"] = b.host; o["deep"] = b.deep; o["deepX"] = b.deepX; o["deepY"] = b.deepY;
    return o;
}
std::unique_ptr<Build> buildFrom(const QJsonObject& o)
{
    auto b = std::make_unique<Build>();
    b->key = o["key"].toString(); b->type = stdStr(o["type"]); b->seed = uint32_t(o["seed"].toDouble()); b->scale = o["scale"].toDouble(1); b->size = o["size"].toString();
    b->rot0 = o["rot0"].toDouble(); b->spinU = o["spinU"].toDouble(); b->pitchU = o["pitchU"].toDouble(); b->a0 = o["a0"].toDouble(); b->w = o["w"].toDouble(); b->rr = o["rr"].toDouble(); b->rp = o["rp"].toDouble();
    b->sys = o["sys"].toInt(-1); b->host = o["host"].toInt(-1); b->deep = o["deep"].toBool(); b->deepX = o["deepX"].toDouble(); b->deepY = o["deepY"].toDouble();
    return b;
}
} // namespace

QJsonObject Doc::toJson() const
{
    QJsonObject o;
    o["v"] = 1;
    QJsonObject rec;
    rec["seed"] = double(opts.seed); rec["style"] = QString::fromLatin1(styleKey(opts.style)); rec["size"] = QString::fromLatin1(sizeKey(opts.size));
    rec["arms"] = opts.arms; rec["life"] = opts.life; rec["count"] = opts.count; rec["preset"] = opts.preset;
    if (opts.free) rec["free"] = true;
    if (opts.mix.size() == 7) { QJsonArray mx; for (double v : opts.mix) mx.push_back(v); rec["mix"] = mx; }
    o["recipe"] = rec;
    QJsonArray batches;
    for (int n : addedBatches) batches.push_back(n);
    o["batches"] = batches;
    QJsonObject nm;
    for (auto it = userNames.begin(); it != userNames.end(); ++it) nm[it.key()] = it.value();
    o["names"] = nm;
    QJsonArray hid;
    for (const QString& k : hidden) hid.push_back(k);
    o["hidden"] = hid;
    o["seq"] = seq;
    o["simT"] = simT;
    QJsonArray regs;
    for (const auto& r : regions) {
        QJsonObject q;
        q["key"] = r->key; q["name"] = r->name; q["color"] = r->color;
        if (r->autoPop) { q["autoPop"] = true; q["popMask"] = r->popMask; }
        if (!r->stamps.empty()) { QJsonArray st; for (const auto& x : r->stamps) st.push_back(QJsonArray { x[0], x[1], x[2] }); q["stamps"] = st; }
        QJsonArray sy;
        for (int id : r->systems) sy.push_back(id);
        q["systems"] = sy;
        regs.push_back(q);
    }
    o["regions"] = regs;
    QJsonArray grs;
    for (const auto& g : groups) { QJsonObject q; q["key"] = g->key; q["name"] = g->name; q["kind"] = g->kind; q["parent"] = g->parent; if (!g->form.isEmpty()) q["form"] = g->form; grs.push_back(q); }
    o["groups"] = grs;
    QJsonArray dcs;
    for (const auto& d : docs) {
        QJsonObject q;
        q["id"] = d->id; q["owner"] = d->owner; q["title"] = d->title; q["text"] = d->text; q["html"] = d->html; q["gaveta"] = d->gaveta; q["drawer"] = d->drawer; q["itemId"] = d->itemId; q["placeId"] = d->placeId; q["nodeId"] = d->nodeId;
        if (d->parent) q["parent"] = d->parent;
        dcs.push_back(q);
    }
    o["docs"] = dcs;
    QJsonArray bds;
    for (const auto& b : builds) bds.push_back(buildToJson(*b));
    o["builds"] = bds;
    QJsonArray lbs;
    for (const FreeLabel& l : labels) { QJsonObject q; q["key"] = l.key; q["x"] = l.x; q["y"] = l.y; q["text"] = l.text; lbs.push_back(q); }
    o["labels"] = lbs;
    QJsonObject plc;
    for (auto it = placeIds.constBegin(); it != placeIds.constEnd(); ++it) plc[it.key()] = it.value();
    if (!plc.isEmpty()) o["places"] = plc;
    QJsonArray edited, gone;
    for (const auto& s : gal->systems) {
        if (s->gone && !veiled.contains(s->id)) gone.push_back(s->id);
        if (s->edited || s->hand) edited.push_back(systemToJson(*s));
    }
    o["edited"] = edited;
    o["gone"] = gone;
    if (opts.organic || opts.free) {
        if (opts.organic) o["organic"] = true;
        QJsonArray vs;
        for (int id : veiled) vs.push_back(id);
        o["veiled"] = vs;
    }
    // Criação Livre
    const auto keys = [](const QSet<QString>& set) { QJsonArray a; for (const QString& k : set) a.push_back(k); return a; };
    const auto table = [](const QHash<QString, QHash<QString, QString>>& h) {
        QJsonObject out;
        for (auto it = h.constBegin(); it != h.constEnd(); ++it) { QJsonObject f; for (auto j = it.value().constBegin(); j != it.value().constEnd(); ++j) f[j.key()] = j.value(); out[it.key()] = f; }
        return out;
    };
    if (!solo.isEmpty()) { QJsonArray a; for (int id : solo) a.push_back(id); o["solo"] = a; }
    if (!ghosts.isEmpty()) o["ghosts"] = keys(ghosts);
    if (!capitals.isEmpty()) o["capitals"] = keys(capitals);
    if (!fichas.isEmpty()) o["fichas"] = table(fichas);
    if (!fichasGen.isEmpty()) o["fichasGen"] = table(fichasGen);
    if (!linkTypes.isEmpty()) { QJsonObject lt; for (auto it = linkTypes.constBegin(); it != linkTypes.constEnd(); ++it) lt[it.key()] = it.value(); o["linkTypes"] = lt; }
    if (!autoSys.isEmpty()) {
        QJsonArray as;
        for (int id : autoSys) as.push_back(id);
        o["autoSys"] = as;
    }
    return o;
}
bool Doc::fromJson(const QJsonObject& o)
{
    if (!o.contains("recipe")) return false;
    const QJsonObject rec = o["recipe"].toObject();
    GalOpts g;
    g.seed = uint32_t(rec["seed"].toDouble());
    const std::string st = stdStr(rec["style"]), sz = stdStr(rec["size"]);
    for (Style s : { Style::Espiral, Style::Barrada, Style::Eliptica, Style::Irregular, Style::Anel }) if (st == styleKey(s)) g.style = s;
    for (Size s : { Size::Pequena, Size::Media, Size::Grande, Size::Imensa }) if (sz == sizeKey(s)) g.size = s;
    g.arms = rec["arms"].toInt(4); g.life = rec["life"].toDouble(0.3); g.count = rec["count"].toInt(); g.preset = rec["preset"].toString();
    g.free = rec["free"].toBool();
    if (rec.contains("mix")) { const QJsonArray mx = rec["mix"].toArray(); if (mx.size() == 7) for (const QJsonValue& v : mx) g.mix.push_back(v.toDouble()); }
    generate(g);
    // A pronta já pôs os nomes e rótulos dela: o que vale agora é o que está guardado.
    userNames.clear(); labels.clear();
    placeIds.clear();
    { const QJsonObject plc = o["places"].toObject(); for (auto it = plc.constBegin(); it != plc.constEnd(); ++it) placeIds.insert(it.key(), it.value().toString()); }
    const QJsonArray batches = o["batches"].toArray();
    for (const QJsonValue& v : batches) { const int n = v.toInt(); addSystems(*gal, n); addedBatches.push_back(n); }
    const QJsonObject nm = o["names"].toObject();
    for (auto it = nm.begin(); it != nm.end(); ++it) userNames.insert(it.key(), it.value().toString());
    for (const QJsonValue& v : o["hidden"].toArray()) hidden.insert(v.toString());
    seq = o["seq"].toInt(1);
    simT = o["simT"].toDouble();
    for (const QJsonValue& v : o["regions"].toArray()) {
        const QJsonObject q = v.toObject();
        auto r = std::make_unique<Region>();
        r->key = q["key"].toString(); r->name = q["name"].toString(); r->color = q["color"].toString();
        r->autoPop = q["autoPop"].toBool(); r->popMask = q["popMask"].toInt(r->popMask);
        for (const QJsonValue& id : q["systems"].toArray()) r->systems.insert(id.toInt());
        for (const QJsonValue& x : q["stamps"].toArray()) { const QJsonArray a = x.toArray(); if (a.size() == 3) r->stamps.push_back({ a[0].toDouble(), a[1].toDouble(), a[2].toDouble() }); }
        regions.push_back(std::move(r));
    }
    for (const QJsonValue& v : o["groups"].toArray()) {
        const QJsonObject q = v.toObject();
        auto r = std::make_unique<Group>();
        r->key = q["key"].toString(); r->name = q["name"].toString(); r->kind = q["kind"].toString(); r->parent = q["parent"].toString(); r->form = q["form"].toString();
        groups.push_back(std::move(r));
    }
    for (const QJsonValue& v : o["docs"].toArray()) {
        const QJsonObject q = v.toObject();
        auto d = std::make_unique<DocLink>();
        d->id = q["id"].toInt(); d->owner = q["owner"].toString(); d->title = q["title"].toString(); d->text = q["text"].toString(); d->html = q["html"].toString(); d->gaveta = q["gaveta"].toBool(); d->drawer = q["drawer"].toString(); d->itemId = q["itemId"].toString(); d->placeId = q["placeId"].toString(); d->nodeId = q["nodeId"].toString(); d->parent = q["parent"].toInt();
        docs.push_back(std::move(d));
    }
    for (const QJsonValue& v : o["builds"].toArray()) builds.push_back(buildFrom(v.toObject()));
    for (const QJsonValue& v : o["labels"].toArray()) { const QJsonObject q = v.toObject(); labels.push_back({ q["key"].toString(), q["x"].toDouble(), q["y"].toDouble(), q["text"].toString() }); }
    // Sistema editado vem inteiro do arquivo; sistema posto à mão entra na mesma posição da lista.
    for (const QJsonValue& v : o["edited"].toArray()) {
        const QJsonObject q = v.toObject();
        const int id = q["id"].toInt(-1);
        if (id < 0) continue;
        while (int(gal->systems.size()) <= id) {
            auto s = std::make_unique<System>();
            s->id = int(gal->systems.size()); s->gone = true;
            gal->systems.push_back(std::move(s));
        }
        System& s = *gal->systems[size_t(id)];
        s.gone = false;
        systemFrom(q, s);
    }
    for (const QJsonValue& v : o["gone"].toArray()) { const int id = v.toInt(-1); if (id >= 0 && id < int(gal->systems.size())) gal->systems[size_t(id)]->gone = true; }
    opts.organic = o["organic"].toBool();
    for (const QJsonValue& v : o["veiled"].toArray()) { const int id = v.toInt(-1); if (id >= 0 && id < int(gal->systems.size())) veil(*gal->systems[size_t(id)]); }
    for (const QJsonValue& v : o["autoSys"].toArray()) autoSys.insert(v.toInt(-1));
    for (const QJsonValue& v : o["solo"].toArray()) { const int id = v.toInt(-1); if (id >= 0 && id < int(gal->systems.size())) { solo.insert(id); gal->systems[size_t(id)]->gone = true; } }
    for (const QJsonValue& v : o["ghosts"].toArray()) ghosts.insert(v.toString());
    for (const QJsonValue& v : o["capitals"].toArray()) capitals.insert(v.toString());
    const auto readTable = [](const QJsonObject& t, QHash<QString, QHash<QString, QString>>& out) {
        for (auto it = t.constBegin(); it != t.constEnd(); ++it) { QHash<QString, QString> f; const QJsonObject q = it.value().toObject(); for (auto j = q.constBegin(); j != q.constEnd(); ++j) f.insert(j.key(), j.value().toString()); out.insert(it.key(), f); }
    };
    readTable(o["fichas"].toObject(), fichas);
    readTable(o["fichasGen"].toObject(), fichasGen);
    { const QJsonObject lt = o["linkTypes"].toObject(); for (auto it = lt.constBegin(); it != lt.constEnd(); ++it) linkTypes.insert(it.key(), it.value().toString()); }
    layoutSystems();
    rev++;
    return true;
}

} // namespace galaxy
