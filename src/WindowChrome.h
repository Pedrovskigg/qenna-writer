#pragma once
// A barra de título das janelas no tema do Qenna (Windows 11: cor da barra,
// do texto e da borda; claro/escuro nos botões de minimizar/fechar). Antes só
// o editor pintava a dele; as outras janelas (Lousa, Timeline, Enciclopédia,
// menu principal, avisos) ficavam no azul do sistema.
//
// install() cuida de todas: pinta cada janela com barra quando ela aparece e
// repinta as abertas quando o tema muda. A janela principal fica sem borda
// visível (a barra emenda no fundo); as que flutuam por cima ganham a borda
// na cor dos painéis.

class QWidget;

namespace WindowChrome {

void install();
void apply(QWidget* window);

} // namespace WindowChrome
