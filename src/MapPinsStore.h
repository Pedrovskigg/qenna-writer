#pragma once

#include <QObject>
#include <QString>
#include <QVector>

// Pins de referência do projeto no mapa-múndi. Cada pin é um lugar (real ou
// fictício) com rótulo, nota e, opcionalmente, vínculo a um elemento da gaveta
// (personagem etc.). Persistência via sidecar `map_pins.json` no root do projeto.
class MapPinsStore : public QObject {
    Q_OBJECT
public:
    struct Pin {
        QString id;
        double lon = 0.0;
        double lat = 0.0;
        QString label;
        QString note;
        QString linkId;     // id do elemento vinculado ("" = sem vínculo)
        QString linkLabel;  // nome do elemento (cache pra exibir)
    };

    explicit MapPinsStore(QObject* parent = nullptr);

    void setProjectRoot(const QString& root);
    bool load();
    bool save() const;

    const QVector<Pin>& pins() const { return m_pins; }

    QString addPin(const Pin& p);   // gera id, salva, devolve id
    bool updatePin(const Pin& p);   // por id
    bool removePin(const QString& id);

signals:
    void pinsChanged();

private:
    QString sidecarPath() const;

    QString m_root;
    QVector<Pin> m_pins;
};
