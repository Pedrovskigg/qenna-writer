#include "GeoData.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

const GeoData& GeoData::instance()
{
    static GeoData inst;
    return inst;
}

GeoData::GeoData()
{
    load();
}

void GeoData::load()
{
    // Extrai os anéis externos da geometria (Polygon/MultiPolygon).
    auto extractRings = [](const QJsonObject& geom) {
        QVector<QPolygonF> rings;
        const QString type = geom.value(QStringLiteral("type")).toString();
        const QJsonArray coords = geom.value(QStringLiteral("coordinates")).toArray();
        auto addRing = [&](const QJsonArray& ring) {
            QPolygonF poly;
            poly.reserve(ring.size());
            for (const QJsonValue& pv : ring) {
                const QJsonArray p = pv.toArray();
                if (p.size() >= 2)
                    poly << QPointF(p.at(0).toDouble(), p.at(1).toDouble());
            }
            if (poly.size() >= 2) rings.append(poly);
        };
        if (type == QStringLiteral("Polygon")) {
            if (!coords.isEmpty()) addRing(coords.at(0).toArray());
        } else if (type == QStringLiteral("MultiPolygon")) {
            for (const QJsonValue& polyV : coords) {
                const QJsonArray poly = polyV.toArray();
                if (!poly.isEmpty()) addRing(poly.at(0).toArray());
            }
        }
        return rings;
    };
    auto boundsOf = [](const QVector<QPolygonF>& rings) {
        QRectF b; bool first = true;
        for (const QPolygonF& r : rings) {
            b = first ? r.boundingRect() : b.united(r.boundingRect());
            first = false;
        }
        return b;
    };

    QSettings qs;
    const QString uiLang = qs.value(QStringLiteral("app/language")).toString();

    // ---------------- Países (fronteiras) ----------------
    QFile cf(QStringLiteral(":/geo/countries.geojson"));
    if (cf.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(cf.readAll()).object();
        for (const QJsonValue& fv : root.value(QStringLiteral("features")).toArray()) {
            const QJsonObject f = fv.toObject();
            const QJsonObject props = f.value(QStringLiteral("properties")).toObject();
            Country c;
            c.nameEn = props.value(QStringLiteral("NAME")).toString();
            if (uiLang == QStringLiteral("en")) {
                c.name = c.nameEn;
            } else if (uiLang == QStringLiteral("es")) {
                const QString es = props.value(QStringLiteral("NAME_ES")).toString();
                c.name = es.isEmpty() ? c.nameEn : es;
            } else {
                const QString pt = props.value(QStringLiteral("NAME_PT")).toString();
                c.name = pt.isEmpty() ? c.nameEn : pt;
            }
            c.iso = props.value(QStringLiteral("ISO_A2")).toString();
            c.labelLon = props.value(QStringLiteral("LABEL_X")).toDouble();
            c.labelLat = props.value(QStringLiteral("LABEL_Y")).toDouble();
            c.rings = extractRings(f.value(QStringLiteral("geometry")).toObject());
            c.bounds = boundsOf(c.rings);
            if (!c.rings.isEmpty()) m_countries.append(c);
        }
    }

    // ---------------- Estados / províncias ----------------
    QFile sf(QStringLiteral(":/geo/states.geojson"));
    if (sf.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(sf.readAll()).object();
        for (const QJsonValue& fv : root.value(QStringLiteral("features")).toArray()) {
            const QJsonObject f = fv.toObject();
            const QJsonObject props = f.value(QStringLiteral("properties")).toObject();
            State st;
            const QString stateEn = props.value(QStringLiteral("name")).toString();
            if (uiLang == QStringLiteral("en")) {
                st.name = stateEn;
            } else if (uiLang == QStringLiteral("es")) {
                const QString es = props.value(QStringLiteral("name_es")).toString();
                st.name = es.isEmpty() ? stateEn : es;
            } else {
                const QString pt = props.value(QStringLiteral("name_pt")).toString();
                st.name = pt.isEmpty() ? stateEn : pt;
            }
            st.country = props.value(QStringLiteral("admin")).toString();
            st.rings = extractRings(f.value(QStringLiteral("geometry")).toObject());
            st.bounds = boundsOf(st.rings);
            if (!st.rings.isEmpty()) m_states.append(st);
        }
    }

    // ---------------- Cidades / municípios (GeoNames, TSV) ----------------
    // Colunas: name, asciiname, lat, lon, pop, fcode, cc, estado, altitude,
    //          fuso, alternatenames.
    QFile pf(QStringLiteral(":/geo/cities.tsv"));
    if (pf.open(QIODevice::ReadOnly)) {
        QString text = QString::fromUtf8(pf.readAll());
        text.remove(QLatin1Char('\r')); // normaliza CRLF do arquivo gerado no Windows
        const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        m_places.reserve(lines.size());
        for (const QString& line : lines) {
            const QStringList c = line.split(QLatin1Char('\t'));
            if (c.size() < 10) continue;
            Place p;
            p.name = c.at(0);
            p.asciiName = c.at(1);
            p.lat = c.at(2).toDouble();
            p.lon = c.at(3).toDouble();
            p.population = c.at(4).toLongLong();
            p.fcode = c.at(5);
            p.countryCode = c.at(6);
            p.state = c.at(7);
            p.elevation = c.at(8).toInt();
            p.timezone = c.at(9);
            if (c.size() >= 11) p.altNames = c.at(10);
            if (!p.name.isEmpty()) m_places.append(p);
        }
    }

    // ---------------- Ficha de países (GeoNames countryInfo) ----------------
    // Colunas: iso, capital, pop, area, continente, moeda, idiomas.
    QFile inf(QStringLiteral(":/geo/countries_info.tsv"));
    if (inf.open(QIODevice::ReadOnly)) {
        QString text = QString::fromUtf8(inf.readAll());
        text.remove(QLatin1Char('\r')); // normaliza CRLF
        for (const QString& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QStringList c = line.split(QLatin1Char('\t'));
            if (c.size() < 7) continue;
            CountryInfo ci;
            ci.capital = c.at(1);
            ci.population = c.at(2).toLongLong();
            ci.area = c.at(3).toDouble();
            ci.continent = c.at(4);
            ci.currency = c.at(5);
            ci.languages = c.at(6);
            m_countryInfo.insert(c.at(0), ci);
        }
    }
}

const GeoData::CountryInfo* GeoData::countryInfo(const QString& iso) const
{
    auto it = m_countryInfo.constFind(iso);
    return (it != m_countryInfo.constEnd()) ? &it.value() : nullptr;
}
