#pragma once

#include <QObject>
#include <QString>
#include <QVector>

// Memórias do projeto: trechos que o usuário seleciona no editor e salva para
// consulta. Persiste em `[projectRoot]/memories.json` (sidecar separado do
// project.mira.json, igual ao glossary.json e elements.json). Espelha o shape
// do Mira 1: cada memória guarda o texto, o destino (projeto ou um personagem)
// e a fonte de onde foi tirada (capítulo/cena/gaveta) + um rótulo já montado.
class MemoriesStore : public QObject {
    Q_OBJECT
public:
    struct Memory {
        QString id;
        QString name;          // título opcional dado pelo usuário
        QString text;          // o trecho selecionado
        QString targetType;    // "project" | "character"
        QString elementId;     // id do personagem (se targetType == "character")
        QString sourceType;    // "chapter" | "scene" | "drawer" | ""
        QString sourceLabel;   // rótulo pronto: "Capítulo 3 — Cena 2", etc.
        QString chapterId;
        int     sceneIndex = -1;
        QString manuscriptId;
        QString drawerKey;
        QString itemId;
        qint64  createdAt = 0; // ms desde epoch
    };

    explicit MemoriesStore(QObject* parent = nullptr);

    void setProjectRoot(const QString& root);
    bool load();
    bool save() const;

    const QVector<Memory>& memories() const { return m_memories; }
    // Memórias do projeto (targetType != "character"), mais recentes primeiro.
    QVector<Memory> projectMemories() const;
    // Memórias de um personagem específico, mais recentes primeiro.
    QVector<Memory> characterMemories(const QString& elementId) const;

    QString add(const Memory& mem); // gera id/createdAt se vazios; retorna id
    bool remove(const QString& id);

signals:
    void changed();

private:
    QString sidecarPath() const;

    QString m_root;
    QVector<Memory> m_memories;
};
