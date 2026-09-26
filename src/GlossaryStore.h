#pragma once

#include <QObject>
#include <QString>
#include <QSet>
#include <QStringList>
#include <QVector>

// Glossário do projeto: dicionário canônico de termos da obra. Persiste em
// `[projectRoot]/glossary.json`. Alimenta o spell-check (termos do glossário
// nunca viram erro) e, mais à frente, sai como apêndice nas exportações.
class GlossaryStore : public QObject {
    Q_OBJECT
public:
    struct Entry {
        QString id;
        QString term;
        QString definition;
        qint64 addedAt = 0; // ms desde epoch
        QString category;   // tipo livre e opcional ("sigla", "grupo", "lugar"…)
        QStringList aliases; // outras grafias: "CAL" pro "Comando Alto Leste"
        // Termo + outras grafias, sem vazios.
        QStringList spellings() const;
    };

    explicit GlossaryStore(QObject* parent = nullptr);

    void setProjectRoot(const QString& root);
    bool load();
    bool save() const;

    const QVector<Entry>& entries() const { return m_entries; }
    QSet<QString> terms() const; // lowercase, p/ spell-check (inclui outras grafias e cada palavra delas)

    QString add(const QString& term, const QString& definition,
                const QString& category = QString(), const QStringList& aliases = {});
    bool update(const QString& id, const QString& term, const QString& definition);
    bool update(const QString& id, const QString& term, const QString& definition,
                const QString& category, const QStringList& aliases);
    bool remove(const QString& id);
    Entry findById(const QString& id) const;
    bool hasTerm(const QString& term) const;

signals:
    void changed();

private:
    QString sidecarPath() const;
    void sortEntries();

    QString m_root;
    QVector<Entry> m_entries;
};
