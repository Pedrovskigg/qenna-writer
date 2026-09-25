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
// O mesmo movimento saindo de qualquer borda (Pensário e Estatísticas pela
// direita, RefMenu e Som imersivo pela barra de ferramentas). Serve também
// pra janela solta (Qt::Tool).
void installFromEdge(QWidget* panel, std::function<Qt::Edge()> edge, std::function<void()> onOpening);
// Cascata automática: acha a lista principal do painel (o maior QScrollArea)
// e faz as linhas dela entrarem a partir de `from`.
void autoCascade(QWidget* panel, Qt::Edge from, int delayMs = 70);
// Popover que aparece e some o tempo todo (menu de seleção): entra com fade
// e um deslize curto a partir de `from`, sem mola; ao sumir, some na hora.
void installPopover(QWidget* w, Qt::Edge from);

// Cascata: cada linha entra deslizando da barra, 22ms depois da anterior.
// Só as 14 primeiras animam; o resto já está lá.
void cascade(const QList<QWidget*>& rows, int delayMs);
void cascade(const QList<QWidget*>& rows, int delayMs, Qt::Edge from);

// Troca de conteúdo (estilo, gaveta, livro): o conteúdo velho sai deslizando
// num fantasma por cima de `area`. Chamar ANTES de refazer a lista.
void swapOut(QWidget* area);

// Janela solta (ficha no hover): entra com fade e deslize curto. Chamar logo
// depois do show(), só quando ela acabou de aparecer.
void popIn(QWidget* window);

// Menu (⋯, botão direito): entra com fade. Chamar antes do exec().
void animateMenu(QMenu* menu);

// Toda janela que abre no app (diálogos de criar memória, marcador,
// glossário, evento, Configurações, Temas, menus…) entra com fade e um
// deslize curto de baixo pra cima. Quem já tem movimento próprio fica de
// fora (propriedade "qennaMotionHandled" ou a lista em PanelMotion.cpp).
void installGlobalWindowMotion();

// Tempo e curva da cascata, pra quem desenha os próprios itens (Retratos,
// Polaroid, Crachás, Galeria): progresso 0…1 do item `order` depois de
// `elapsedMs` desde o início.
qreal cascadeProgress(int order, qint64 elapsedMs, int delayMs);
int cascadeTotalMs(int delayMs);
constexpr qreal kCascadeShift = 10.0;

}
