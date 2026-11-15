#pragma once

#include <QHash>
#include <QList>
#include <QPixmap>
#include <QPoint>
#include <QPointer>
#include <QString>
#include <QPair>
#include <QStringList>
#include <QWidget>

class QFrame;
class QEnterEvent;
class QTimer;
class QVariantAnimation;
class LeftBarLabels;
class LeftBarDrum;
class LeftBarNamed;
class LeftBarTag;
class LeftBarSceneCard;
class LeftBarThread;
struct LeftBarDrawerInfo;
class CarouselViewport;
class ToolbarGroupWidget;

class QVBoxLayout;
class QToolButton;
class QEvent;
class ProjectModel;
struct Drawer;

class LeftBar : public QWidget {
    Q_OBJECT
public:
    QToolButton* newDrawerButton() const { return m_newDrawerBtn; }   // onde a Nova gaveta brota
    enum FixedAction {
        Info,
        Whiteboard,
        Timeline,
        Manuscripts,
        Groups,
        Outline,
        WorldMap,   // o Mapa (gerador de mundos)
    };
    Q_ENUM(FixedAction)

    explicit LeftBar(ProjectModel* model, QWidget* parent = nullptr);

    // Estilos (14/11/2026, LeftBar Lab aprovado pelo Pe). A Clássica é a de
    // sempre e continua padrão; os outros mudam só o jeito das gavetas e dos
    // nomes. Os valores vão pro QSettings (ui/leftBarStyle): não renumerar.
    enum class BarStyle {
        Classic = 0,   // a de sempre
        Colors  = 1,   // Gavetas com cor: ladrilhos coloridos numa bandeja
        Named   = 2,   // Barra com nomes: larga, tudo escrito, gavetas em árvore
        Drum    = 3,   // Tambor de gavetas
        Scene   = 4,   // A barra que lê a cena
    };
    Q_ENUM(BarStyle)
    static constexpr int kLastStyle = 4;
    // Comprimento (ui/leftBarLength): do topo até o chão, ou só a altura do
    // que a barra tem, como a RightBar (a Plaquinha do lab).
    enum class BarLength { Full = 0, Plate = 1 };
    Q_ENUM(BarLength)

    static BarStyle savedStyle();
    static BarLength savedLength();
    BarStyle barStyle() const { return m_style; }
    void setBarStyle(BarStyle style);
    BarLength barLength() const { return m_length; }
    void setBarLength(BarLength length);

    // A barra que lê a cena: os elementos (Element::id) da folha aberta e o
    // título do trecho ("Nesta cena", "Neste capítulo"; vazio = nenhuma folha).
    void setSceneElements(const QStringList& elementIds, const QString& heading);
    QString sceneHeading() const { return m_sceneHeading; }
    // Itens (id, título) da gaveta que aparecem na folha aberta.
    QList<QPair<QString, QString>> sceneItems(const QString& drawerKey) const;
    // Onde a barra ocupa de verdade (a Barra com nomes larga abre por cima da
    // mesa): os painéis ao lado se posicionam por aqui. Coordenadas do pai.
    QRect outerGeometry() const;

    // A LeftBar so faz sentido colada numa lateral: deitada em cima ou embaixo,
    // o conteudo dela (gavetas, capitulos) atrapalharia o centro da tela. Por
    // isso `side` aceita apenas Qt::LeftEdge (padrao) ou Qt::RightEdge, e a
    // barra continua SEMPRE vertical — nada de orientacao aqui, diferente da
    // TopToolbar.
    Qt::Edge barSide() const { return m_barSide; }
    void setBarSide(Qt::Edge side);


    static int barWidth();

    bool isMirrored() const { return m_mirrored; }
    void setMirrored(bool mirrored);

    // Indica visualmente qual painel está aberto. Limpar (None / vazio) deixa
    // todos os botões desligados.
    void setActiveFixedAction(FixedAction action);
    void setActiveDrawer(const QString& drawerKey);
    void clearSelection();

    // Acesso pontual a um botão fixo (pra plugar hover/tooltips externos).
    QToolButton* fixedButton(FixedAction action) const;

    // Modo focado: esconde botões/separadores e torna o fundo transparente,
    // mantendo a largura (não sai do layout) pra não deslocar o editor. A
    // imagem de fundo do tema aparece atrás.
    void setChromeHidden(bool hidden);
    // Estado consultavel: o modo focado precisa saber se a barra ja esta
    // recuada antes de decidir revelar ou esconder de novo.
    bool chromeHidden() const { return m_chromeHidden; }

    // Etiquetas: com o mouse em cima, uma faixa sai da barra com o nome de
    // cada botão e a contagem das gavetas. Configurações › Interface liga/desliga.
    static bool labelsEnabled();
    // Mouse em cima da faixa de etiquetas aberta.
    bool labelsUnderMouse() const;
    static void setLabelsEnabled(bool on);

    // Persiana: o botão no pé da barra recolhe tudo pra baixo e deixa só uma
    // alcinha no pé, com o botão no mesmo lugar; clicar nela sobe de novo. A
    // largura não muda (a página não pula) e a escolha fica salva em
    // ui/leftBarCollapsed.
    bool isCollapsed() const { return m_collapsed; }
    void setCollapsed(bool collapsed, bool animated = true);
    // Recolhida, a barra precisa de Qt::AlignBottom no layout do pai (senão fica
    // centralizada na altura). Quem tira e repõe a barra no layout chama isto.
    void syncLayoutAlignment();
    // Emendada embaixo da faixa estilo Word (que vai de ponta a ponta): sem a
    // borda de cima nem a da beira da janela, as duas viram uma peça só em L.
    void setJoinedTop(bool on);

signals:
    void barSideChanged(Qt::Edge side);
    void fixedActionTriggered(LeftBar::FixedAction action);
    void drawerSelected(QString drawerKey);
    void newDrawerRequested();
    void drawerContextRequested(QString drawerKey, QPoint globalPos);
    void drawerItemActivated(QString drawerKey, QString itemId);   // a árvore da Barra com nomes
    void barStyleChanged(LeftBar::BarStyle style);
    void barLengthChanged(LeftBar::BarLength length);
    void widthChanged();   // a Barra com nomes abriu ou recolheu

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool event(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void rebuildDrawerButtons();
    void applyTheme();
    void applyUiScale();

private:
    Qt::Edge m_barSide = Qt::LeftEdge;
    bool m_chromeHidden = false;
    bool m_joinedTop = false;
    QString panelStyle(const QString& objectName = QStringLiteral("leftBar")) const;

    // ---- Estilos ----
    BarStyle m_style = BarStyle::Classic;
    BarLength m_length = BarLength::Full;
    bool m_namedNarrow = false;          // Barra com nomes recolhida = a de sempre
    qreal m_namedReveal = -1;            // animando a largura (0 = fina, 1 = larga)
    QPointer<QVariantAnimation> m_widthAnim;
    LeftBarDrum* m_drum = nullptr;
    LeftBarNamed* m_named = nullptr;
    QPointer<LeftBarTag> m_drumTag;
    QPointer<LeftBarSceneCard> m_sceneCard;
    QPointer<LeftBarThread> m_thread;
    QTimer* m_sceneCardTimer = nullptr;
    QPointer<QToolButton> m_sceneCardBtn;
    QStringList m_sceneElements;
    QString m_sceneHeading;
    bool namedWide() const { return m_style == BarStyle::Named && (!m_namedNarrow || m_namedReveal >= 0); }
    int targetWidth() const;
    void applyStyle();
    void placeNamed();
    void showStyleMenu(const QPoint& globalPos);
    void setNamedNarrow(bool narrow, bool animated);
    QList<LeftBarDrawerInfo> drawerInfos() const;
    void refreshNamed();
    void refreshDrum();
    void updateDrumTag(bool animate = false);
    void updateThread();
    void updateFloaters();
    void showSceneCard(QToolButton* btn);
    void hideSceneCard();
    bool drawerInScene(const QString& drawerKey, int* count = nullptr) const;
    bool sceneContext() const { return !m_sceneHeading.isEmpty(); }
    void styleDrawerButton(QToolButton* btn, const Drawer& d);
    void placeNewDrawerButton();
    QWidget* host() const { return parentWidget(); }
    // Placa: a altura do conteúdo; recolhida, a barra guarda a altura e só o
    // pé aparece (máscara), pra o botão da persiana não sair do lugar.
    bool isPlate() const { return m_length == BarLength::Plate; }
    int plateHeight() const;
    void updatePlateGeometry();
    int m_plateVisibleH = -1;            // -1 = inteira
    int m_plateCachedH = 0;              // altura da placa aberta (vale também recolhida)
    bool m_platePassQueued = false;
    QPointer<QWidget> m_filteredParent;  // o pai avisa quando muda de tamanho
    void setPlateVisible(int h);
    void remeasurePlate();
    QRect visibleBox() const;

    // Persiana
    bool contentShown() const { return !m_chromeHidden && !m_collapsed && !m_rolling; }
    bool classicShown() const { return contentShown() && !namedWide(); }
    void applyContentVisibility();
    void refreshCollapseButton();
    void finishRoll();
    int collapsedHeight() const;
    int expandedHeight() const;
    QToolButton* m_collapseBtn = nullptr;
    bool m_collapsed = false;
    bool m_rolling = false;          // animando: os botões somem e quem pinta é a foto
    QPixmap m_rollSnapshot;          // a barra aberta, parada no lugar durante a animação
    QPointer<QVariantAnimation> m_rollAnim;

    // Etiquetas
    void showLabels();
    void refreshLabels();
    void hideLabels(bool animated);
    void maybeHideLabels();
    void updateLabelsGeometry();
    LeftBarLabels* m_labels = nullptr;
    int m_labelsWidth = 140;
    int m_labelsHeight = 0;     // 0 = altura da barra
    QTimer* m_labelsTimer = nullptr;
    int drawerInsertIndexAt(const QPoint& posInBar) const;
    void updateDropIndicator(int targetIndex);
    void clearDropIndicator();
    void startDrawerDrag(QToolButton* btn, const QString& drawerKey);
    QToolButton* makeFixedButton(const QString& iconResource,
                                 const QString& placeholderLetter,
                                 const QString& tooltip,
                                 FixedAction action);
    QToolButton* makeDrawerButton(const QString& drawerKey,
                                  const QString& title,
                                  const QString& color,
                                  const QString& iconId);
    QToolButton* makeNewDrawerButton();
    QToolButton* m_newDrawerBtn = nullptr;
    QList<QFrame*> m_groupSeparators;

    // Botões fixos reorganizáveis — mesmo conceito e mesmo container da
    // TopToolbar (clique-e-segurar liga a edição, ver ToolbarGroupWidget).
    // As gavetas ficam de fora: são do PROJETO, já se reordenam com arrasto
    // direto e a ordem delas vai pro ProjectModel, não pro QSettings.
    void buildGroups();
    QHash<QString, QStringList> defaultButtonLayout() const;
    QStringList defaultGroupOrder() const;
    void rebuildGroupLayout();
    void setEditMode(bool on);
    void onGroupDropped(const QString& draggedId, const QString& targetId);
    void onButtonDropped(const QString& buttonId, const QString& targetGroupId, int index);
    QVBoxLayout* m_groupsLayout = nullptr;
    QHash<QString, ToolbarGroupWidget*> m_groupWidgets;
    QHash<QString, QToolButton*> m_buttonsById;
    QStringList m_groupOrder;
    QHash<QString, QStringList> m_groupButtons;
    bool m_editMode = false;
    void applyMirrorStyle();
    void refreshActiveStates();

    ProjectModel* m_model;
    QVBoxLayout* m_drawerLayout;
    // Carrossel das gavetas (12/11/2026, ideia do Pe): com gaveta demais pra
    // altura da barra, a lista gira. Os botões fixos, o "+" e a persiana
    // ficam parados; só a lista de gavetas mora aqui dentro.
    QWidget* m_drawerContent = nullptr;
    CarouselViewport* m_drawerCarousel = nullptr;
    // A gaveta aparece inteira no miolo do carrossel (as das pontas curvas
    // e as escondidas não ganham etiqueta).
    bool drawerInCarouselWindow(QToolButton* b) const;
    QVBoxLayout* m_rootLayout;
    bool m_mirrored = false;

    QHash<int, QToolButton*> m_fixedButtons;        // FixedAction → button
    QHash<QString, QToolButton*> m_drawerButtons;   // drawerKey → button
    int m_activeFixed = -1;                          // -1 = nenhum
    QString m_activeDrawer;                          // vazio = nenhum

    // Drag state
    QPoint m_dragStartPos;
    QString m_pressedDrawerKey;                      // drawerKey do botão sob press
    QWidget* m_dropIndicator = nullptr;
    int m_dropTargetIndex = -1;
};
