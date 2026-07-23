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
        QString characterId;   // id do Element que fala; vazio = sem locutor
                                // atribuído ainda (estado válido, não um dado
                                // corrompido — ver PensarioPanel, "Diálogos
                                // sem atribuição")
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
    // Soma de palavras de todas as falas atribuídas a um capítulo — usa a
    // MESMA regra de contagem do WordCounter (letra/número contíguos), pra
    // ser comparável com WordCounter::countChapter() e montar a proporção
    // diálogo/narração.
    int dialogueWordsForChapter(const QString& chapterId) const;

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
    // Correção manual de locutor (menu de contexto no card) — o detector é
    // bom, mas heurística de proximidade erra quando o nome citado no texto
    // é de quem está sendo FALADO SOBRE, não de quem fala (ex.: "— Quase
    // nada. — ele admitiu. — Miya Morikawa dos Santos..." atribui pra Miya
    // por proximidade, mas quem fala é outro personagem). Sobrevive a
    // rescans futuros: upsertScanResults só toca characterId em falas NOVAS
    // (texto mudou o bastante pra não bater o hash), nunca em já casadas.
    bool setCharacter(const QString& id, const QString& newCharacterId);

signals:
    void changed();

private:
    QString sidecarPath() const;

    QString m_root;
    QVector<Dialogue> m_dialogues;
};
