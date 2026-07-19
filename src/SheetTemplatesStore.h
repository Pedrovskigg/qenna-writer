#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "ProjectModel.h"   // CharacterSheet (por valor)

// Modelo de ficha salvo pelo usuário: nome + estrutura completa (campos e
// valores) de uma CharacterSheet, pra reaproveitar ao criar novos personagens.
struct SheetTemplate {
    QString id;
    QString name;
    CharacterSheet sheet;
    qint64 createdAt = 0;
};

class SheetTemplatesStore : public QObject {
    Q_OBJECT
public:
    explicit SheetTemplatesStore(QObject* parent = nullptr);

    void setProjectRoot(const QString& root);
    bool load();
    bool save() const;

    const QVector<SheetTemplate>& all() const { return m_templates; }
    const SheetTemplate* findById(const QString& id) const;

    // Salva `sheet` como novo modelo nomeado `name`. Gera IDs novos pros campos
    // (cópia independente da ficha de origem).
    QString add(const QString& name, const CharacterSheet& sheet);
    bool remove(const QString& id);
    bool rename(const QString& id, const QString& newName);

signals:
    void changed();

private:
    QString sidecarPath() const;

    QString m_root;
    QVector<SheetTemplate> m_templates;
};
