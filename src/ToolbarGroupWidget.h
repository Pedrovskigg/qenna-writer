#pragma once

#include <QHash>
#include <QList>
#include <QPair>
#include <QPoint>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>

class QBoxLayout;
class QLabel;
class QTimer;
class QMouseEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;
class QPaintEvent;

// Um grupo de botões da TopToolbar (ex.: "Projeto", "Tipografia") tratado como
// uma unidade de arrastar — existe pra permitir reorganizar a barra (ver
// TopToolbar::rebuildGroupLayout). Fora do "modo de edição" da barra
// (setDraggable(false), padrão), é só um container transparente sem nenhum
// comportamento extra: os botões dentro dele funcionam exatamente como se
// estivessem soltos direto no layout da toolbar.
//
// Em modo de edição ele arrasta duas coisas diferentes:
//  - o GRUPO inteiro, pegando pela alça (ícone de "grip") que aparece na borda
//    inicial;
//  - um BOTÃO individual, pegando em qualquer lugar do próprio botão — que
//    pode ser solto em outra posição do mesmo grupo ou em outro grupo.
//
// O press num botão não chega aqui sozinho (o QToolButton consome), por isso
// instalamos um event filter nos filhos. Em modo de edição ele habilita o
// arrasto e impede que rearranjar dispare a ação do botão sem querer; FORA do
// modo de edição ele fica transparente (deixa o botão funcionar normalmente) e
// só cronometra o clique-e-segurar que LIGA o modo — o mesmo gesto de
// reorganizar ícone que todo mundo conhece de celular.
class ToolbarGroupWidget : public QWidget {
    Q_OBJECT
public:
    // orientation: Qt::Horizontal (barra no topo) ou Qt::Vertical (lateral) —
    // decide a direção do layout interno, de qual lado fica a alça e em qual
    // eixo o indicador de inserção é calculado.
    ToolbarGroupWidget(const QString& groupId, Qt::Orientation orientation, QWidget* parent = nullptr);

    QString groupId() const { return m_groupId; }

    // O id é o mesmo usado na persistência (ver TopToolbar::saveButtonLayout) —
    // precisa ser estável entre versões, não pode ser índice.
    void addButton(QWidget* button, const QString& buttonId);
    // Tira os botões do layout SEM deletá-los (eles pertencem à TopToolbar) —
    // usado no rebuild quando a ordem muda. A alça continua no lugar.
    void clearButtons();
    bool isEmpty() const { return m_buttons.isEmpty(); }

    // Trocar o lado da barra ao vivo troca a orientacao dos grupos junto. Tem
    // que ser chamado ANTES do rebuild da TopToolbar: o alinhamento de cada
    // botao no layout e definido em addButton(), entao so o rebuild o corrige.
    void setOrientation(Qt::Orientation orientation);
    void setDraggable(bool on);
    bool isDraggable() const { return m_draggable; }
    // Reserva area de drop quando o grupo esta vazio E em modo de edicao.
    // Precisa ser chamado DEPOIS de repovoar o grupo (ver
    // TopToolbar::rebuildGroupLayout): um grupo que so ficou vazio por causa do
    // ultimo drop nao pode colapsar pra zero, senao nao ha onde soltar um botao
    // pra devolve-lo — vira uma armadilha de mao unica.
    void refreshEmptyPlaceholder();
    // Espaço entre os botões do grupo. A LeftBar usa um passo menor que a
    // TopToolbar e, sem isto, os ícones dela ficariam mais afastados dentro do
    // grupo do que sempre foram.
    void setSpacing(int px);

    void applyTheme(const QColor& handleColor, const QColor& dropHighlight);

signals:
    // Emitido no widget-ALVO do drop, quando outro grupo é largado em cima
    // dele — a TopToolbar decide a nova ordem e reconstrói o layout.
    void groupDroppedOn(const QString& draggedGroupId, const QString& targetGroupId);
    // Idem para um botão solto neste grupo. `index` é a posição de inserção já
    // calculada a partir de onde o mouse soltou.
    void buttonDroppedOn(const QString& draggedButtonId, const QString& targetGroupId, int index);
    // Duplo clique em cima de um BOTAO durante o modo de edicao. O filtro de
    // eventos engole cliques nos botoes (senao rearranjar dispararia a acao),
    // entao sem isto o duplo clique so sairia do modo de edicao se acertasse
    // uma fresta entre botoes — e a barra vertical quase nao tem fresta.
    // Pedido de ligar/desligar o modo de edição vindo do clique-e-segurar em
    // cima de um BOTÃO.
    void toggleEditModeRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void beginDrag();
    void resetPress();
    // Tooltip do botão + a dica do gesto, montada na hora de exibir.
    QString composedToolTip(const QWidget* button) const;
    void startLongPress(QWidget* button);
    void cancelLongPress();
    void onLongPressElapsed();
    // Posição de inserção (0..n) a partir de um ponto em coordenadas locais.
    int insertionIndexAt(const QPoint& pos) const;
    QRect insertionMarkerRect(int index) const;
    QString buttonIdOf(const QWidget* w) const;
    QWidget* buttonWidget(const QString& id) const;
    // Direção principal do layout: x quando horizontal, y quando vertical.
    int alongAxis(const QPoint& p) const;

    QString m_groupId;
    Qt::Orientation m_orientation;
    QBoxLayout* m_layout = nullptr;
    QLabel* m_handle = nullptr;
    bool m_draggable = false;
    bool m_groupDropHighlight = false;
    // -1 = nenhum indicador de inserção de botão desenhado.
    int m_dropIndex = -1;

    // Press em andamento: `m_pressActive` existe porque QPoint(0,0).isNull() é
    // true — um press exatamente no canto do widget não pode ser confundido
    // com "sem press".
    bool m_pressActive = false;
    QPoint m_dragStartPos;
    // Vazio = o press foi na alça, então o arrasto é do grupo inteiro.
    QString m_pressedButtonId;

    // Clique-e-segurar. `m_longPressFired` existe pra engolir o release que
    // vem depois: sem isso, soltar o botão depois de segurar dispararia a ação
    // dele (negrito, abrir painel...) junto com a entrada no modo de edição.
    QTimer* m_longPressTimer = nullptr;
    QWidget* m_longPressButton = nullptr;
    bool m_longPressFired = false;

    QList<QPair<QString, QWidget*>> m_buttons;
    QColor m_handleColor;
    QColor m_dropHighlightColor;
};

// Composição de uma barra reorganizável — a ordem dos grupos e quais botões
// cada um contém — e sua persistência no QSettings. Compartilhada pela
// TopToolbar e pela LeftBar: as duas regras de validação têm que ser a mesma,
// senão uma barra passa a perder a organização do usuário em casos em que a
// outra não perde.
namespace ToolbarLayout {

// Ordem salva em `key`, ou `def` se ela não tiver exatamente os mesmos grupos.
QStringList loadGroupOrder(const QString& key, const QStringList& def);
void saveGroupOrder(const QString& key, const QStringList& order);

// Serializado como uma QStringList de "grupo=id1,id2,id3", na ordem dos grupos.
void saveButtonLayout(const QString& key, const QStringList& order,
                      const QHash<QString, QStringList>& layout);
// `groupIds`/`buttonIds`: tudo que a barra conhece hoje. Botão novo que o
// layout salvo não tem é encaixado onde `def` o coloca; qualquer outra
// inconsistência (id desconhecido, repetido, formato estranho) volta pra `def`.
QHash<QString, QStringList> loadButtonLayout(const QString& key,
                                             const QHash<QString, QStringList>& def,
                                             const QSet<QString>& groupIds,
                                             const QSet<QString>& buttonIds);

// Aplicam um drop à composição. Devolvem false quando nada mudou (soltou onde
// já estava, id desconhecido) — aí não há o que salvar nem reconstruir.
bool moveButton(QHash<QString, QStringList>& layout, const QString& buttonId,
                const QString& targetGroupId, int index);
bool moveGroup(QStringList& order, const QString& draggedId, const QString& targetId);

} // namespace ToolbarLayout
