#pragma once

#include <QtGlobal>

// Valores-padrão (largura de página, tamanho de fonte) usados só quando o
// usuário/projeto ainda não tem uma preferência salva — ex.: primeiro projeto
// criado na máquina, ou projeto novo sem fontSize gravado no JSON. Adapta o
// "chute inicial" à resolução da tela primária, pra telas menores (notebooks)
// não nascerem com uma página larga demais, que o painel de contagem flutuante
// (ver WordCountPanel) acaba invadindo por cima do texto.
namespace ScreenDefaults {

int recommendedPageWidth();  // px — dentro de EditorLayout::minPageWidth()/maxPageWidth()
qreal recommendedFontSize(); // pt

} // namespace ScreenDefaults
