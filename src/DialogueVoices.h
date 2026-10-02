#pragma once

#include <QColor>
#include <QHash>
#include <QPixmap>
#include <QString>

#include "DialogueStore.h"

// Cor de cada voz (quem fala) e rosto pronto pra mostrar — compartilhado entre
// a aba Diálogos do Pensário e o aviso do Granna no DocHeader, pra o mesmo
// personagem ter a mesma cor nos dois lugares.
namespace DialogueVoices {

// Cor por POSIÇÃO numa paleta de famílias bem separadas (laranja, azul, rosa,
// verde, roxo, amarelo, vermelho, turquesa): matiz por hash deixava dois
// verdes lado a lado. Da 9ª voz em diante a paleta repete num tom mais
// claro/escuro. index < 0 = cor neutra do tema.
QColor color(int index);

// Posição de cada personagem na paleta: quem mais fala no manuscrito primeiro
// (empate pelo id), a mesma ordem da aba Diálogos.
QHash<QString, int> rankForManuscript(const QVector<DialogueStore::Dialogue>& all,
                                      const QString& manuscriptId);

// Rosto circular em `size` pixels lógicos (já com o DPR da tela): a foto do
// personagem se houver; senão, a inicial num círculo na cor da voz.
QPixmap face(const QString& imageDataUrl, const QString& name, const QColor& voice, int size);

} // namespace DialogueVoices
