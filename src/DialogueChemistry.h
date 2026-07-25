#pragma once

#include "DialogueStore.h"

#include <QString>
#include <QVector>

// Coocorrência entre personagens a partir dos diálogos já detectados —
// motor de dados por trás da "Química" da área de Estatísticas. Deliberadamente
// uma lista ranqueada por par, não um grafo visual (ver decisão registrada na
// memória do projeto stats-panel-feature: "Mapa de Relações" como grafo foi
// rejeitado por poluição visual; isso só aparece dentro do drill-down de UM
// personagem por vez).
namespace DialogueChemistry {

struct PairStats {
    QString otherElementId;
    int scenesTogether = 0;
    int chaptersTogether = 0;
    int crossDialogues = 0; // falas de qualquer um dos dois, nas cenas em comum
};

// Duas falas "contam como juntas" quando caem no mesmo grupo (chapterId,
// sceneIndex) — mesma granularidade do filtro "também fala nessa cena" do
// Pensário (PensarioPanel::rebuildDialoguePresenceChips). Falas sem locutor
// (characterId vazio) não contam pra ninguém.
QVector<PairStats> chemistryForCharacter(const QVector<DialogueStore::Dialogue>& all,
                                          const QString& elementId);

// Falas de `a` e de `b` dentro das cenas onde os dois têm fala salva,
// ordenadas por criação — usado pelo popup "diálogos entre os dois".
QVector<DialogueStore::Dialogue> sharedSceneDialogues(const QVector<DialogueStore::Dialogue>& all,
                                                       const QString& a, const QString& b);

} // namespace DialogueChemistry
