#pragma once

#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

// Adesivos da Lousa: a cartela que vem com a Qenna (marcações de quadro de
// investigação, carimbos e objetos) e os PNGs que a pessoa já usou, em
// qualquer projeto. Leva 2 do rework da Lousa (2026-10-07).
namespace LousaStickers {

// Ids da cartela, na ordem em que aparecem.
QStringList markIds();     // círculo, seta, X, ?, sublinhado, fita
QStringList objectIds();   // carimbos, digital, lupa, clipe, café

// O desenho pronto pra virar adesivo (fundo transparente), com o lado
// maior em `longestPx`.
QImage render(const QString& id, int longestPx = 360);
// Nome pra dica do botão ("Círculo de caneta").
QString label(const QString& id);
// Contorno com que o adesivo da cartela entra: tinta e caneta entram sem
// recorte branco; objeto entra com sombra.
QString defaultOutline(const QString& id);
// Tamanho no quadro (lado maior), em px.
qreal defaultSize(const QString& id);

// Usados antes: guardados fora do projeto, pra servir em qualquer um.
void remember(const QImage& image);
struct Recent { QString path; QImage image; };
QList<Recent> recent();

// A imagem tem algum pixel transparente? (então vira adesivo, não card de imagem)
bool hasTransparency(const QImage& image);

} // namespace LousaStickers
