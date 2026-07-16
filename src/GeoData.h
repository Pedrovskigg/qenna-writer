#pragma once

#include <QHash>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>

// Leitor dos dados Natural Earth embutidos (:/geo/*.geojson). Carrega uma única
// vez (singleton preguiçoso). Coordenadas geográficas: x = longitude, y = latitude.
class GeoData {
public:
    struct Country {
        QString name;             // português quando há (NAME_PT), senão inglês
        QString nameEn;
        QString iso;              // ISO_A2 (liga ao CountryInfo)
        QVector<QPolygonF> rings; // anéis externos (lon,lat); ilhas = vários
        QRectF bounds;            // bounding box em lon/lat
        double labelLon = 0.0;    // ponto ideal do rótulo (LABEL_X/Y do Natural Earth)
        double labelLat = 0.0;
    };
    // Ficha de país (GeoNames countryInfo).
    struct CountryInfo {
        QString capital;
        qint64 population = 0;
        double area = 0.0;        // km²
        QString continent;        // código (SA, EU…)
        QString currency;         // nome da moeda
        QString languages;        // códigos (pt-BR,es…)
    };
    struct State {
        QString name;             // português quando há (name_pt)
        QString country;          // país (admin)
        QVector<QPolygonF> rings;
        QRectF bounds;
    };
    struct Place {
        QString name;
        QString asciiName;        // sem acento (busca)
        double lon = 0.0;
        double lat = 0.0;
        qint64 population = 0;
        QString fcode;            // PPLC = capital nacional, PPLA = capital estadual
        QString countryCode;      // sigla ISO (BR, US…)
        QString state;            // nome do estado/província
        int elevation = 0;        // metros
        QString timezone;         // ex America/Sao_Paulo
        QString altNames;         // nomes alternativos (busca multilíngue)

        bool isCapital() const { return fcode == QStringLiteral("PPLC"); }
        bool isStateCapital() const { return fcode == QStringLiteral("PPLA"); }
    };

    static const GeoData& instance();

    const QVector<Country>& countries() const { return m_countries; }
    const QVector<State>& states() const { return m_states; }
    const QVector<Place>& places() const { return m_places; }
    const CountryInfo* countryInfo(const QString& iso) const;

private:
    GeoData();
    void load();

    QVector<Country> m_countries;
    QVector<State> m_states;
    QVector<Place> m_places;
    QHash<QString, CountryInfo> m_countryInfo;
};
