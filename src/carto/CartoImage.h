#pragma once

// Mapa a partir de uma imagem pronta (15/11/2026, prometido à Lady Oscar): a
// imagem vira o chão da mesa (gira e inclina com ela) e o mar é achado pela
// cor da borda. Por baixo, o mundo é plano: terra onde a imagem é terra, mar
// onde é mar. Assim os Territórios crescem só na terra e o Explorar, as
// construções e os nomes funcionam em cima da imagem.

#include "CartoCore.h"

#include <QImage>
#include <QSize>

namespace carto {

constexpr int kImageMaxSide = 8192;   // imagem maior que isso é reduzida ao abrir

// o tamanho do mundo pra uma imagem: a mesma área do tamanho escolhido, na proporção da imagem
QSize imageWorldSize(QSize img, int size);
// o tamanho sugerido: o maior que a imagem cobre sem esticar (no mínimo a Região)
int suggestedImageSize(QSize img);
// a imagem no tamanho do mundo (RGBA8888), pronta pra virar chão
QImage fitImageToWorld(const QImage& src, int size);
// a máscara do mar (1 = mar), na resolução da imagem. sea: 0 = sem mar · 1–100 = tolerância da cor
std::vector<uint8_t> seaMask(const QImage& img, int sea);
// o mundo inteiro a partir da imagem já no tamanho do mundo
World imageWorld(const QImage& fitted, int sea, int seed, int size, const QString& climate);
// a imagem do chão de volta (pro Regenerar partir dela)
QImage backdropImage(const World& w);
// prévia: a imagem com o mar coberto de azul translúcido
QImage seaPreview(const QImage& fitted, const std::vector<uint8_t>& sea, int targetW);

} // namespace carto
