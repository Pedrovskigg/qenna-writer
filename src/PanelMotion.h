#pragma once
// Movimento "Gaveta" dos painéis que abrem da LeftBar (gavetas e Manuscritos).
//
// A gaveta sai de trás da barra (máscara crescendo a partir da borda da barra
// + um deslize curto com um pouco de mola) e, ao fechar, volta pra ela. As
// primeiras linhas entram em cascata. Tudo desliga com Configurações ›
// Interface › Animações.
//
// Abrir usa o próprio painel (máscara + posição); fechar usa um "fantasma"
// (grab() do painel), porque no Hide o painel já sumiu — assim nenhum dos
// lugares que chamam hide() precisa mudar.

#include <QList>
#include <QPointer>
#include <QWidget>
#include <functional>

class QMenu;

namespace PanelMotion {

bool enabled();
void setEnabled(bool on);

// Lado da barra (true = LeftBar à direita, o painel cresce pra esquerda).
void setBarOnRightProvider(std::function<bool()> provider);
bool barOnRight();

// Liga o abrir/fechar da gaveta no painel. onOpening roda logo depois que a
// abertura começa (a cascata das linhas).
void installDrawer(QWidget* panel, std::function<void()> onOpening);

// Cascata: cada linha entra deslizando da barra, 22ms depois da anterior.
// Só as 14 primeiras animam; o resto já está lá.
void cascade(const QList<QWidget*>& rows, int delayMs);

// Troca de conteúdo (estilo, gaveta, livro): o conteúdo velho sai deslizando
// num fantasma por cima de `area`. Chamar ANTES de refazer a lista.
void swapOut(QWidget* area);

// Janela solta (ficha no hover): entra com fade e deslize curto. Chamar logo
// depois do show(), só quando ela acabou de aparecer.
void popIn(QWidget* window);

// Menu (⋯, botão direito): entra com fade. Chamar antes do exec().
void animateMenu(QMenu* menu);

// Tempo e curva da cascata, pra quem desenha os próprios itens (Retratos,
// Polaroid, Crachás, Galeria): progresso 0…1 do item `order` depois de
// `elapsedMs` desde o início.
qreal cascadeProgress(int order, qint64 elapsedMs, int delayMs);
int cascadeTotalMs(int delayMs);
constexpr qreal kCascadeShift = 10.0;

}
