#pragma once

#include <QColor>
#include <QRect>

class QImage;
class QWidget;

// Vidro nos painéis (Criador de Temas, 2026-09): com o tema pedindo desfoque
// (Theme::panelBlur) e painéis translúcidos (panelOpacity < 100), o que aparece
// atrás de cada painel é o fundo da janela e a folha DESFOCADOS — o conteúdo
// do painel continua inteiro, só o fundo dele vira vidro.
//
// O Qt Widgets não tem backdrop-filter, então:
//  1. o fundo (a imagem do tema) e a folha são desfocados UMA vez, numa imagem
//     reduzida, e só refeitos quando a janela, a folha ou o tema mudam;
//  2. cada painel ligado ganha uma "placa" irmã, logo abaixo dele no z-order e
//     com a mesma geometria, que pinta o pedaço desfocado que fica atrás dele.
//     O fundo translúcido do painel (QSS com rgba) compõe por cima da placa.
//
// Por que placa irmã e não pintar dentro do painel: o fundo de QSS de um widget
// é pintado ANTES do evento de paint chegar a qualquer filtro, então pintar o
// desfoque "por dentro" cobriria a cor translúcida do próprio painel.
//
// Limites conhecidos (ditos ao usuário): um painel sobre outro não desfoca o de
// trás, e menus/popups (janelas separadas) ficam de fora.
namespace PanelGlass {

// A janela e o widget de fundo (a imagem do tema). Chamar uma vez.
void setWindow(QWidget* window, QWidget* background);

// Retângulo da folha, em coordenadas da janela, e a cor dela (já com a
// opacidade da página). O MainWindow chama sempre que a folha muda de lugar.
void setPage(const QRect& rectInWindow, const QColor& fill);

// Liga o vidro a um painel (filho da janela, não janela própria).
void attach(QWidget* panel);

// Refaz o desfoque (tema trocou, janela mudou de tamanho...). Barato de chamar
// em sequência: agrupa num timer curto.
void invalidate();

// Desfoque de caixa em três passadas (≈ gaussiano), no lugar. Usado também pela
// prévia pintada do Criador de Temas, pra o vidro lá sair igual ao do app.
void boxBlur(QImage& img, int radius);

} // namespace PanelGlass
