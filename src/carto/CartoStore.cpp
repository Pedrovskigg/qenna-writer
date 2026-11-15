#include "CartoStore.h"
#include "CartoImage.h"
#include "CartoProf.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>

namespace carto {

namespace {
constexpr quint32 kMagic = 0x514D4150;   // "QMAP"
constexpr quint32 kVersion = 1;
// a diferença vira inteiro (zero onde não mexeu, o que comprime quase a nada)
constexpr double kQuantH = 4096.0, kQuantMK = 65536.0;

QByteArray packDelta(const std::vector<float>& cur, const std::vector<float>& base, double q, bool* any)
{
    QByteArray raw(static_cast<int>(cur.size() * sizeof(qint32)), Qt::Uninitialized);
    auto* d = reinterpret_cast<qint32*>(raw.data());
    *any = false;
    for (size_t i = 0; i < cur.size(); i++) {
        if (cur[i] == base[i]) { d[i] = 0; continue; }
        const double v = std::floor((static_cast<double>(cur[i]) - base[i]) * q + 0.5);
        d[i] = static_cast<qint32>(clampd(v, -2147483647.0, 2147483647.0));
        if (d[i] == 0) d[i] = cur[i] > base[i] ? 1 : -1;   // mudou de verdade: não deixa a diferença sumir no arredondamento
        *any = true;
    }
    return qCompress(raw, 6);
}
void unpackDelta(std::vector<float>& base, const QByteArray& packed, double q)
{
    const QByteArray raw = qUncompress(packed);
    if (raw.size() != static_cast<int>(base.size() * sizeof(qint32))) return;
    const auto* d = reinterpret_cast<const qint32*>(raw.constData());
    for (size_t i = 0; i < base.size(); i++) if (d[i]) base[i] = static_cast<float>(base[i] + d[i] / q);
}
}

QStringList imageFilesOf(const QString& mapPath)
{
    const QFileInfo fi(mapPath);
    QStringList out;
    for (const QFileInfo& f : QDir(fi.absolutePath()).entryInfoList({ fi.completeBaseName() + QStringLiteral("-*.png") }, QDir::Files))
        out << f.absoluteFilePath();
    return out;
}
QString imageFileFor(const QString& mapPath, const QString& key)
{
    const QFileInfo fi(mapPath);
    return QDir(fi.absolutePath()).filePath(fi.completeBaseName() + QLatin1Char('-') + key + QStringLiteral(".png"));
}

World rebuildBase(int seed, int water, const QString& type, int size, const QString& climate)
{
    CARTO_PROF("refazer mundo da semente");
    return type == QLatin1String("empty") ? emptyWorld(seed, size, climate) : generate(seed, water, type, size, climate);
}

void Store::setProjectRoot(const QString& root)
{
    m_root = root;
    loadManifest();
}
QString Store::manifestPath() const { return QDir::cleanPath(m_root + QStringLiteral("/mapas.json")); }
QString Store::pathOf(const QString& id) const
{
    const int i = indexOf(id);
    return i >= 0 ? QDir::cleanPath(m_root + QLatin1Char('/') + m_maps[i].file) : QString();
}
int Store::indexOf(const QString& id) const
{
    for (int i = 0; i < m_maps.size(); i++) if (m_maps[i].id == id) return i;
    return -1;
}

void Store::loadManifest()
{
    m_maps.clear();
    m_activeId.clear();
    if (m_root.isEmpty()) return;
    QFile f(manifestPath());
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    for (const QJsonValue& v : root.value(QStringLiteral("maps")).toArray()) {
        const QJsonObject o = v.toObject();
        MapMeta m{ o.value(QStringLiteral("id")).toString(), o.value(QStringLiteral("name")).toString(), o.value(QStringLiteral("file")).toString(), o.value(QStringLiteral("kind")).toString() };
        if (!m.id.isEmpty() && !m.file.isEmpty()) m_maps.append(m);
    }
    m_activeId = root.value(QStringLiteral("activeMapId")).toString();
    if (indexOf(m_activeId) < 0 && !m_maps.isEmpty()) m_activeId = m_maps.first().id;
}
void Store::saveManifest() const
{
    if (m_root.isEmpty()) return;
    QJsonArray arr;
    for (const MapMeta& m : m_maps) {
        QJsonObject o{ { QStringLiteral("id"), m.id }, { QStringLiteral("name"), m.name }, { QStringLiteral("file"), m.file } };
        if (!m.kind.isEmpty()) o.insert(QStringLiteral("kind"), m.kind);
        arr.append(o);
    }
    const QJsonObject root{ { QStringLiteral("maps"), arr }, { QStringLiteral("activeMapId"), m_activeId } };
    QSaveFile f(manifestPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.commit();
}
MapMeta Store::createMap(const QString& name, const QString& kind)
{
    MapMeta m;
    m.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m.name = name;
    m.kind = kind;
    m.file = (kind == QLatin1String("galaxy") ? QStringLiteral("mapas/%1.qgal") : QStringLiteral("mapas/%1.qmap")).arg(m.id);
    QDir(m_root).mkpath(QStringLiteral("mapas"));
    m_maps.append(m);
    m_activeId = m.id;
    saveManifest();
    return m;
}
void Store::renameMap(const QString& id, const QString& name)
{
    const int i = indexOf(id);
    if (i < 0) return;
    m_maps[i].name = name;
    saveManifest();
}
void Store::removeMap(const QString& id)
{
    const int i = indexOf(id);
    if (i < 0) return;
    const QString path = pathOf(id);
    QFile::remove(path);
    for (const QString& f : imageFilesOf(path)) QFile::remove(f);   // as imagens do mapa de imagem
    m_maps.removeAt(i);
    if (m_activeId == id) m_activeId = m_maps.isEmpty() ? QString() : m_maps.first().id;
    saveManifest();
}

SaveJob Store::prepareSave(const QString& id, const World& w, const MapViewState& view) const
{
    SaveJob j;
    j.path = pathOf(id);
    j.seed = w.seed; j.water = w.water; j.type = w.type; j.size = w.size; j.climate = w.climate;
    j.ref = w.ref; j.scale = w.scale; j.W = w.W; j.H = w.H;
    j.h = w.h; j.m = w.m; j.k = w.k;
    j.view = view;
    if (isImageWorld(w)) j.backdrop = w.backdrop;
    return j;
}

bool Store::saveMap(const QString& id, const World& w, const MapViewState& view) const
{
    return writeSave(prepareSave(id, w, view));
}

bool writeSave(const SaveJob& j)
{
    CARTO_PROF("salvar arquivo");
    const QString& path = j.path;
    const MapViewState& view = j.view;
    if (path.isEmpty() || j.W <= 0) return false;
    QDir().mkpath(QFileInfo(path).absolutePath());
    // mapa de imagem: o mundo sai da imagem (os pincéis de relevo não mexem nele), então não há diferença pra guardar
    const bool image = j.type == QLatin1String("image");
    QString imageName;
    if (image && j.backdrop && !j.backdrop->levels.empty()) {
        const QString file = imageFileFor(path, j.backdrop->key);
        if (!QFileInfo::exists(file)) {
            const Backdrop::Level& l = j.backdrop->levels.front();
            QImage(l.rgba.data(), l.W, l.H, l.W * 4, QImage::Format_RGBA8888).save(file, "PNG");
        }
        for (const QString& f : imageFilesOf(path)) if (QFileInfo(f).fileName() != QFileInfo(file).fileName()) QFile::remove(f);   // a imagem antiga (o Regenerar trocou)
        imageName = QFileInfo(file).fileName();
    }
    const World base = image ? World() : rebuildBase(j.seed, j.water, j.type, j.size, j.climate);
    QJsonObject meta{
        { QStringLiteral("seed"), j.seed }, { QStringLiteral("water"), j.water }, { QStringLiteral("type"), j.type },
        { QStringLiteral("size"), j.size }, { QStringLiteral("climate"), j.climate }, { QStringLiteral("ref"), j.ref },
        { QStringLiteral("scale"), j.scale },
        { QStringLiteral("view"), QJsonObject{ { QStringLiteral("ang"), view.ang }, { QStringLiteral("pitch"), view.pitch },
                                                { QStringLiteral("mode"), view.mode }, { QStringLiteral("style"), view.style },
                                                { QStringLiteral("hour"), view.hour } } },
    };
    if (!view.extra.isEmpty()) meta.insert(QStringLiteral("layers"), view.extra);
    if (!imageName.isEmpty()) meta.insert(QStringLiteral("image"), imageName);
    QByteArray body;
    {
        QDataStream ds(&body, QIODevice::WriteOnly);
        ds.setVersion(QDataStream::Qt_6_5);
        ds << kMagic << kVersion << QJsonDocument(meta).toJson(QJsonDocument::Compact);
        const bool sameSize = !image && base.W == j.W && base.H == j.H;
        for (int f = 0; f < 3; f++) {
            const std::vector<float>& cur = f == 0 ? j.h : f == 1 ? j.m : j.k;
            const std::vector<float>& bas = f == 0 ? base.h : f == 1 ? base.m : base.k;
            bool any = false;
            QByteArray packed;
            if (sameSize) packed = packDelta(cur, bas, f == 0 ? kQuantH : kQuantMK, &any);
            ds << static_cast<quint8>(any ? 1 : 0);
            if (any) ds << packed;
        }
    }
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    out.write(body);
    return out.commit();
}

bool Store::loadMap(const QString& id, World& w, MapViewState& view) const
{
    CARTO_PROF("abrir arquivo");
    QFile f(pathOf(id));
    if (!f.open(QIODevice::ReadOnly)) return false;
    QDataStream ds(&f);
    ds.setVersion(QDataStream::Qt_6_5);
    quint32 magic = 0, version = 0;
    QByteArray json;
    ds >> magic >> version >> json;
    if (magic != kMagic || version > kVersion) return false;
    const QJsonObject meta = QJsonDocument::fromJson(json).object();
    if (meta.value(QStringLiteral("type")).toString() == QLatin1String("image")) {
        // mapa de imagem: o mundo sai de novo da imagem guardada; sem ela, vira um mar vazio pra desenhar
        const QString name = meta.value(QStringLiteral("image")).toString();
        const QImage img = name.isEmpty() ? QImage() : QImage(QDir(QFileInfo(f.fileName()).absolutePath()).filePath(name));
        const int size = meta.value(QStringLiteral("size")).toInt(1);
        const QString climate = meta.value(QStringLiteral("climate")).toString(QStringLiteral("mixed"));
        if (!img.isNull()) {
            w = imageWorld(img.convertToFormat(QImage::Format_RGBA8888), meta.value(QStringLiteral("water")).toInt(50),
                           meta.value(QStringLiteral("seed")).toInt(), size, climate);
            if (w.backdrop) {
                auto bd = std::make_shared<Backdrop>(*w.backdrop);
                bd->key = QFileInfo(name).completeBaseName().section(QLatin1Char('-'), -1);   // o mesmo arquivo: não regrava
                w.backdrop = bd;
            }
        } else w = emptyWorld(meta.value(QStringLiteral("seed")).toInt(), size, climate);
    } else
    w = rebuildBase(meta.value(QStringLiteral("seed")).toInt(), meta.value(QStringLiteral("water")).toInt(60),
                    meta.value(QStringLiteral("type")).toString(QStringLiteral("continent")), meta.value(QStringLiteral("size")).toInt(1),
                    meta.value(QStringLiteral("climate")).toString(QStringLiteral("mixed")));
    for (int fi = 0; fi < 3; fi++) {
        if (isImageWorld(w)) break;
        quint8 present = 0;
        ds >> present;
        if (!present) continue;
        QByteArray packed;
        ds >> packed;
        unpackDelta(fi == 0 ? w.h : fi == 1 ? w.m : w.k, packed, fi == 0 ? kQuantH : kQuantMK);
    }
    if (!isImageWorld(w)) w.ref = meta.value(QStringLiteral("ref")).toDouble(w.ref);
    w.scale = meta.value(QStringLiteral("scale")).toDouble(1.0);
    const QJsonObject v = meta.value(QStringLiteral("view")).toObject();
    view.ang = v.value(QStringLiteral("ang")).toDouble(0);
    view.pitch = v.value(QStringLiteral("pitch")).toDouble(40);
    view.mode = v.value(QStringLiteral("mode")).toString(QStringLiteral("full"));
    view.style = v.value(QStringLiteral("style")).toString(QStringLiteral("rich"));
    view.hour = v.value(QStringLiteral("hour")).toDouble(14.4);
    view.extra = meta.value(QStringLiteral("layers")).toObject();
    return ds.status() == QDataStream::Ok;
}

} // namespace carto
