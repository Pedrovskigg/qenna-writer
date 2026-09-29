#pragma once

#include "DialogueDetector.h"

#include <QHash>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QVector>

// Falas detectadas pelo DialogueDetector, em `[projectRoot]/dialogs.json`.
//
// Desde a 1.4 o dialogs.json continua uma lista (a versão anterior ainda o
// lê), com campos novos por fala (speech, origin, confidence, extra); a lista
// de ignoradas e os votos de gênero ficam em dialogs-meta.json. Fala sem
// "origin" (Mira 1, Qenna até a 0.18, ou regravada por uma versão antiga)
// entra como "legacy" e é resolvida no primeiro scan (ver applyChapterScan).
//
// Regras (auditoria de 2026-09-29, concepts/harness/dialogos):
//  - o scan de um capítulo é a verdade daquele capítulo: fala que saiu do
//    texto sai do arquivo; nada de fantasma contando nas estatísticas;
//  - identidade = cena + texto inteiro + ordem entre falas iguais (dois
//    "— Sim." no mesmo capítulo são duas falas);
//  - fala automática é recalculada a cada scan; fala MANUAL nunca;
//  - "Não é fala" (remove) entra numa lista de ignoradas e não volta;
//  - arquivo que não abre é copiado de lado antes de qualquer gravação.
class DialogueStore : public QObject {
    Q_OBJECT
public:
    struct Dialogue {
        QString id;
        QString text;          // parágrafo inteiro (o editor procura por ele)
        QString speech;        // só a fala, sem tag (diálogo x narração)
        QString characterId;   // vazio = sem locutor do elenco
        QString origin;        // "auto" | "manual" | "legacy"
        QString confidence;    // "certain" | "probable" | "extra" | "none"
        QString extraLabel;    // figurante: "o porteiro", "Éder"
        QString manuscriptId;
        QString chapterId;
        int     sceneIndex = -1; // -1 = capítulo sem cenas
        QString sourceLabel;     // rótulo pronto: "Capítulo 3 — Cena 2"
        qint64  createdAt = 0;

        bool isManual() const { return origin == QLatin1String("manual"); }
        bool isProbable() const { return confidence == QLatin1String("probable") && !isManual(); }
        bool isExtra() const { return characterId.isEmpty() && confidence == QLatin1String("extra"); }
        // Palavras de fala de verdade (sem a tag), com fallback pro texto.
        const QString& spokenText() const { return speech.isEmpty() ? text : speech; }
    };

    // Uma fala encontrada num scan, já com a cena/rótulo de onde veio.
    struct ScannedLine {
        QString text;
        QString speech;
        QString characterId;
        DialogueConfidence confidence = DialogueConfidence::None;
        QString extraLabel;
        int sceneIndex = -1;
        QString sourceLabel;
    };

    // sceneScope de applyChapterScan: o capítulo inteiro.
    static constexpr int kWholeChapter = -2;

    explicit DialogueStore(QObject* parent = nullptr);

    void setProjectRoot(const QString& root);
    bool load();
    bool save() const;

    const QVector<Dialogue>& dialogues() const { return m_dialogues; }
    QVector<Dialogue> dialoguesForCharacter(const QString& elementId) const;
    // Palavras FALADAS (sem tag) de um capítulo / de um personagem — mesma
    // regra de contagem do WordCounter, pra fechar com countChapter().
    int dialogueWordsForChapter(const QString& chapterId) const;
    int dialogueWordsForCharacter(const QString& elementId) const;

    // Aplica o resultado de um scan.
    //  sceneScope == kWholeChapter: `found` é o capítulo inteiro — o que não
    //    está mais no texto sai do arquivo (inclusive manual).
    //  sceneScope >= -1: só aquela cena foi lida (cena aberta sozinha). Fala
    //    de outra cena nunca é tocada; manual/legacy sem par fica até o
    //    próximo scan do capítulo inteiro.
    // cast: elenco atual — manual apontando pra personagem excluído volta a
    // ser automática; e é com ele que a migração do formato 1 reproduz o
    // motor antigo (DialogueDetector::legacyAttribution) pra separar o que
    // foi automático do que foi corrigido à mão.
    // legacyNarratorId: personagem marcado como narrador (o motor antigo
    // jogava pra ele toda linha com "eu"/"disse" — nunca é correção manual).
    void applyChapterScan(const QString& manuscriptId, const QString& chapterId,
                          const QVector<ScannedLine>& found, int sceneScope,
                          const DialogueDetector::Cast& cast,
                          const QString& legacyNarratorId = QString());

    // Capítulos que não existem mais: falas saem do arquivo.
    void pruneChapters(const QSet<QString>& validChapterIds);
    void removeChapter(const QString& chapterId);

    // Gênero aprendido pelo texto, capítulo a capítulo (ver
    // DialogueDetector::addGenderVotes). Retorna true se mudou (grave com save()).
    bool setChapterGenderVotes(const QString& chapterId, const DialogueDetector::GenderVotes& votes);
    DialogueDetector::GenderVotes genderVotes() const;

    // Aplica trocas de nome NO TEXTO das falas já salvas (pares antigo→novo).
    // O texto de uma fala é uma CÓPIA de um trecho do manuscrito: renomeado o
    // manuscrito, a cópia precisa acompanhar. Retorna quantas falas mudaram.
    int replaceInTexts(const QVector<QPair<QString, QString>>& terms);

    // Parecença entre duas versões de uma fala (prefixo + sufixo em comum
    // sobre o tamanho da maior): >= 0,6 = a mesma fala, editada.
    static double textSimilarity(const QString& a, const QString& b);

    // "Não é fala": some e entra na lista de ignoradas do capítulo.
    bool remove(const QString& id);
    // Correção/confirmação manual de locutor. O mesmo id confirma uma fala
    // provável. Manual nunca é sobrescrita por scan.
    bool setCharacter(const QString& id, const QString& newCharacterId);

signals:
    void changed();

private:
    QString sidecarPath() const;
    QString metaPath() const; // dialogs-meta.json: ignoradas + votos de gênero
    static QString ignoreKey(const QString& chapterId, const QString& text);

    QString m_root;
    QVector<Dialogue> m_dialogues;
    QSet<QString> m_ignored; // ignoreKey()
    QHash<QString, DialogueDetector::GenderVotes> m_genderVotes; // por capítulo
};
