#pragma once

#include "DialogueDetector.h"

#include <QObject>
#include <QString>
#include <QVector>

// Diálogos detectados automaticamente pelo DialogueDetector. Persiste em
// `[projectRoot]/dialogs.json` (sidecar separado, mesmo padrão de
// memories.json/elements.json). Espelha o shape do Mira 1 (dialogs.json):
// cada diálogo guarda o texto, quem falou e a origem (capítulo/cena).
class DialogueStore : public QObject {
    Q_OBJECT
public:
    struct Dialogue {
        QString id;
        QString text;
        QString characterId;   // id do Element que fala
        QString manuscriptId;
        QString chapterId;
        int     sceneIndex = -1; // -1 = capítulo sem cenas
        QString sourceLabel;     // rótulo pronto: "Capítulo 3 — Cena 2"
        qint64  createdAt = 0;
    };

    explicit DialogueStore(QObject* parent = nullptr);

    void setProjectRoot(const QString& root);
    bool load();
    bool save() const;

    const QVector<Dialogue>& dialogues() const { return m_dialogues; }
    QVector<Dialogue> dialoguesForCharacter(const QString& elementId) const;

    // Uma fala encontrada num scan, já com a cena/rótulo de onde veio.
    struct ScannedLine {
        QString text;
        QString characterId;
        int sceneIndex = -1;
        QString sourceLabel;
    };

    // Funde o resultado de um scan (DialogueDetector::scanConfidentDialogues,
    // já achatado pra TODOS os segmentos/cenas de um capítulo numa chamada só)
    // com o que já está salvo. Reconciliação é por CAPÍTULO inteiro (não por
    // cena exata) — identidade da fala é o hash do texto (100 primeiros
    // chars), não (chapterId, sceneIndex): isso evita duplicar tudo quando um
    // capítulo sem cenas ganha a primeira cena (sceneIndex muda de -1 pra 0
    // pra todo mundo, mas é a mesma fala, só de "endereço" atualizado).
    // Falas já salvas com o mesmo hash têm sceneIndex/sourceLabel corrigidos
    // se mudaram; texto editado é atualizado quando a correspondência com o
    // scan atual é inequívoca; falas novas são inseridas. Nada é removido
    // automaticamente — exclusão é sempre manual.
    void upsertScanResults(const QString& manuscriptId, const QString& chapterId,
                           const QVector<ScannedLine>& found);

    bool remove(const QString& id);

signals:
    void changed();

private:
    QString sidecarPath() const;

    QString m_root;
    QVector<Dialogue> m_dialogues;
};
