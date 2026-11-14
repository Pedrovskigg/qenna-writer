#ifndef TOPTOOLBAR_H
#define TOPTOOLBAR_H

#include <QHash>
#include <QLabel>
#include <QList>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <functional>
class QToolButton;
class QLineEdit;
class QFrame;
class QMenu;
class QAction;
class QBoxLayout;
class QSpacerItem;
class FontPickerPopup;
class ToolbarGroupWidget;
class CarouselViewport;
class QStackedWidget;
class QWidget;

class TopToolbar : public QWidget
{
    Q_OBJECT

public:
    enum class AlignScope { ThisDoc, AllDocs, Manuscript, Drawers };
    Q_ENUM(AlignScope)
    // Estilo da barra (12/11/2026). Clássica = a de sempre, desde o Mira
    // Writing em JSX. As outras três são a faixa estilo Word: completa,
    // simplificada (uma linha) e só as abas. Escolha em Configurações e no
    // botão de modo da própria faixa; salva em ui/topToolbarStyle.
    // 13/11/2026, saídas do lab Escala Word–Qenna: Simplificada com sotaque
    // (o Q no lugar do Arquivo, a linha de comandos num cartão) e Fichário
    // (abas de fichário que se emendam na faixa). As duas têm uma linha só.
    enum class BarStyle { Classic = 0, RibbonFull = 1, RibbonSimple = 2, RibbonTabs = 3,
                          RibbonAccent = 4, RibbonBinder = 5, RibbonReel = 6 };
    Q_ENUM(BarStyle)

    // side: Qt::TopEdge (padrão, horizontal) ou Qt::RightEdge (vertical, na
    // lateral direita). Fixo pra vida do objeto — trocar de lado é decisão de
    // configuração (pede reiniciar o app, mesmo padrão do seletor de idioma
    // na tela inicial), não uma troca ao vivo de layout.
    explicit TopToolbar(QWidget *parent = nullptr, Qt::Edge side = Qt::TopEdge);

    Qt::Edge barSide() const { return m_barSide; }
    bool isVertical() const { return m_barSide == Qt::LeftEdge || m_barSide == Qt::RightEdge; }
    // Espessura fixa da barra (48px) — largura quando vertical, altura quando
    // horizontal. MainWindow usa isso pra reservar espaço de layout.
    int thickness() const;

    BarStyle barStyle() const { return m_barStyle; }
    void setBarStyle(BarStyle style);
    // A faixa só existe com a barra em cima; na lateral vale a clássica.
    bool ribbonActive() const { return m_barStyle != BarStyle::Classic && !isVertical(); }
    // Faixa de uma linha só (simplificada e as duas com cara de Qenna).
    bool ribbonOneLine() const { return m_barStyle == BarStyle::RibbonSimple || m_barStyle == BarStyle::RibbonAccent
                                        || m_barStyle == BarStyle::RibbonBinder || m_barStyle == BarStyle::RibbonReel; }
    // Largura que a faixa quer ter (a da folha). A MainWindow informa.
    void setRibbonTargetWidth(int px);
    // Posição da faixa: de ponta a ponta (teto da janela, emendada na LeftBar)
    // ou flutuante (da largura da folha, presa em cima dela). Salva em
    // ui/topToolbarRibbonFloating.
    bool ribbonFloating() const { return m_ribbonFloating; }
    void setRibbonFloating(bool on);
    // De ponta a ponta de fato (faixa ativa e não flutuante).
    // Posição da barra (13/11): de ponta a ponta ou flutuante vale pra todos os
    // estilos, a clássica e a Giratória inclusive. Com a barra na lateral não vale.
    bool ribbonEdgeToEdge() const { return ribbonActive() && !m_ribbonFloating; }
    bool classicEdgeToEdge() const { return !ribbonActive() && !m_ribbonFloating && !isVertical(); }
    bool barEdgeToEdge() const { return ribbonEdgeToEdge() || classicEdgeToEdge(); }
    // Largura da clássica: 0 = a do conteúdo (a de sempre). Com largura, os
    // botões ficam centralizados em centerX (coordenada da barra; -1 = meio) e
    // o carrossel gira se não couberem.
    void setClassicSpan(int width, int centerX);
    // Rolo (13/11): flutua como a clássica, da largura do próprio conteúdo e
    // centralizado na folha, valha o que valer a Posição.
    bool ribbonHugsContent() const { return ribbonActive() && m_barStyle == BarStyle::RibbonReel && m_ribbonFloating; }
    // Recuo do conteúdo da faixa pra começar alinhado com a borda da LeftBar
    // (o canto em cima dela fica vazio, na cor da faixa).
    void setRibbonSideInsets(int left, int right);
    // Centro da folha, em coordenadas da barra (Fichário de ponta a ponta:
    // abas e botões se centralizam com a página). A MainWindow informa.
    void setRibbonPageCenter(int x);
    // Teto do comprimento da folha (o mesmo das Configurações: a altura útil da
    // janela). No máximo, "Tela cheia". A MainWindow fornece.
    std::function<int()> pageHeightMaximum;

    void setFontFamilies(const QStringList &families, const QString &current);
    void setFontSize(qreal pt);
    void setLineHeightPercent(int percent);
    void setFirstLineIndentEnabled(bool enabled);
    void setParagraphSpacingBefore(int px);
    void setParagraphSpacingAfter(int px);
    void setBoldChecked(bool checked);
    void setItalicChecked(bool checked);
    void setUnderlineChecked(bool checked);
    void setStrikethroughChecked(bool checked);
    void setReadModeChecked(bool checked);
    void setFocusModeChecked(bool checked);
    void setFullscreenChecked(bool checked);
    // `subtitle` opcional: quando presente, mostra `title` maior em cima e
    // `subtitle` menor embaixo (ex.: capítulo em cima, "Cena x" embaixo).
    // Botão discreto ao lado do subtítulo — só faz sentido quando o
    // subtítulo é uma cena de verdade (viewMode SceneDoc), abre o popup de
    // variações. Escondido por padrão.
    // x em coords locais da TopToolbar; passe -1 para retomar o centro geométrico.

    QRect immersiveSoundButtonGlobalRect() const;
    QRect readAloudButtonGlobalRect() const;
    QRect repetitionsButtonGlobalRect() const;
    QRect reminderButtonGlobalRect() const;
    QRect helpButtonGlobalRect() const;

    // Troca o lado da barra AO VIVO (antes exigia reiniciar o app). Reconfigura
    // orientacao, layout, folha de estilo e a natureza dos botoes de tipografia
    // — no topo o de fonte e um botao de TEXTO com o nome da fonte, na lateral e
    // um icone quadrado. Quem estiver ouvindo barSideChanged reage ao resto
    // (holder, faixa de titulo, insets dos paineis).
    void setBarSide(Qt::Edge side);

    void setReminderBadge(bool active);
    void pulsePensarioBadge();
    // RightBar ligada (13/11/2026): Criador de Mundos, Referência, Pensário,
    // Lembretes, Estatísticas e a assistente moram nela e saem daqui (da
    // clássica e das abas). A organização salva da clássica não muda.
    void setProjectButtonsInRightBar(bool on);
    bool projectButtonsInRightBar() const { return !m_rightBarButtons.isEmpty(); }
    void setCurrentAlignment(Qt::Alignment alignment);

protected:
    bool event(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    // Clicar em qualquer lugar que nao seja um icone arrastavel SAI do modo de
    // reorganizacao (mesma logica de "tocar fora" do celular). A entrada e o
    // clique-e-segurar num icone, em ToolbarGroupWidget.
    void mousePressEvent(QMouseEvent *event) override;
    // Clique fora da barra inteira: mesmo efeito, capturado no nivel do app
    // enquanto o modo de edicao esta ligado.
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void barSideChanged(Qt::Edge side);
    void barStyleChanged(TopToolbar::BarStyle style);
    void ribbonPlacementChanged(bool floating);
    // Grupo Página da faixa: liga/desliga o modo de página (a MainWindow aplica).
    void pageModeToggled(bool on);
    void fontFamilyChanged(const QString &family);
    void fontSizeChanged(qreal pt);
    void lineHeightChanged(int percent);
    void firstLineIndentToggled(bool enabled);
    void paragraphSpacingBeforeChanged(int px);
    void paragraphSpacingAfterChanged(int px);
    void addImageRequested();
    void focusModeToggled(bool enabled);
    void mainMenuRequested();
    void newProjectRequested();
    void openProjectRequested();
    void saveProjectRequested();
    void exportRequested();
    void refMenuToggleRequested();
    void pensarioToggleRequested();
    void helpRequested();
    void construtorToggleRequested();
    void miraToggleRequested();
    void boldToggled(bool enabled);
    void italicToggled(bool enabled);
    void underlineToggled(bool enabled);
    void strikethroughToggled(bool enabled);
    void alignmentRequested(Qt::Alignment alignment, TopToolbar::AlignScope scope);
    // Abre o painel de Estatísticas (personagens/manuscrito).
    void statisticsRequested();
    // Lê em voz alta: o trecho selecionado, ou do cursor até o fim se não
    // houver seleção. Quem decide é a MainWindow, que enxerga o editor.
    void readAloudRequested();
    // Detector de Repetições ligado/desligado — é um interruptor, não um
    // painel: ligado, o texto passa a grifar o que se repete.
    void repetitionsToggled(bool enabled);
    // Placeholders — ainda sem implementação
    void readModeToggled(bool enabled);
    void searchRequested();
    void reminderRequested();
    void immersiveSoundRequested();
    void settingsRequested();
    void fullscreenToggled(bool enabled);
    void themePanelRequested();

private:
    QToolButton *homeButton;
    QToolButton *newProjectButton;
    QToolButton *openProjectButton;
    QToolButton *saveProjectButton;
    QToolButton *exportButton;
    QToolButton *boldButton;
    QToolButton *italicButton;
    QToolButton *underlineButton;
    QToolButton *strikethroughButton;
    QToolButton *statisticsButton;
    QToolButton *readAloudButton;
    QToolButton *repetitionsButton;
    QToolButton *readModeButton;
    QToolButton *focusButton;
    QToolButton *searchButton;
    QToolButton *fontButton;
    QToolButton *sizeButton;
    QToolButton *lineHeightButton;
    QToolButton *indentButton;
    QToolButton *alignButton;
    QToolButton *imageButton;
    QToolButton *reminderButton;
    QToolButton *immersiveSoundButton;
    QToolButton *themePanelButton;
    QToolButton *settingsButton;
    QToolButton *fullscreenButton;
    QToolButton *refMenuButton;
    QToolButton *pensarioButton;
    QToolButton *helpButton;
    QToolButton *construtorButton;
    QToolButton *miraButton;

    QIcon focusOffIcon;
    QIcon focusOnIcon;
    QIcon readModeOffIcon;
    QIcon readModeOnIcon;

    FontPickerPopup *fontPicker;

    QLineEdit *sizeStepperEdit = nullptr;
    QList<QAction*> sizePresetActions;

    QStringList fontFamilies;
    QString currentFontFamily;
    qreal currentFontSize;
    int currentLineHeightPercent;
    int currentParaSpaceBefore = 0;
    int currentParaSpaceAfter = 0;
    QLabel *paraBeforeValueLabel = nullptr;
    QLabel *paraAfterValueLabel = nullptr;

    void buildSizeMenu();
    void buildSpacingMenu();
    void buildAlignMenu();
    void updateAlignButtonIcon();
    void refreshLayout();
    void updateSizeMenuState();
    void updateSpacingMenuChecks();
    // Texto vira tooltip quando vertical (ícone-only) — "17.5" ou "1.3" não
    // se assenta legivelmente embaixo de um ícone de 32-48px numa coluna
    // estreita, é mais robusto mostrar só sob demanda do que tentar encaixar.
    void updateSizeButtonLabel();
    void updateLineHeightButtonLabel();
    void applySize(qreal pt);
    void commitSizeEditor();
    static QString sizeText(qreal pt);
    void applyParaSpaceBefore(int px);
    void applyParaSpaceAfter(int px);
    void applyFontButtonStyle();
    // Fonte/tamanho/espacamento mudam de natureza conforme o lado da barra.
    // Extraido do construtor pra poder rodar de novo numa troca ao vivo.
    void applyTypographyButtonMode();
    // Ver definição em TopToolbar.cpp: anula o min-width que o QSS global impõe
    // aos botões de tipografia, inválido quando a barra está na vertical.
    QString verticalGeometryReset() const;
    void applyTheme();
    void applyRootStyle();
    void reloadIcons();
    void applyUiScale();
    int currentIconPx() const;

    // Carrossel (12/11/2026, ideia do Pe): quando a janela não tem largura
    // pra todos os botões, a fileira gira em vez de esconder botão num menu
    // "⋯". m_content é onde os grupos moram; o carrossel recorta e gira.
    QWidget* m_content = nullptr;
    CarouselViewport* m_carousel = nullptr;
    int m_classicSpanW = 0;
    int m_classicCenterX = -1;
    bool m_classicSpanApplied = false;
    void applyClassicSpan();
    // Sem escolha salva, cada estilo nasce como sempre foi: a clássica e a
    // Giratória flutuando, as abas de ponta a ponta.
    static bool defaultFloatingFor(BarStyle style) { return style == BarStyle::Classic || style == BarStyle::RibbonReel; }
    QBoxLayout* m_outerLayout = nullptr;
    // Traz o botão pro miolo antes de ancorar um painel nele.
    void revealButton(QToolButton* btn) const;

    QList<QPair<QToolButton*, QString>> iconBindings;
    QList<QToolButton*> m_squareButtons; // botões de tamanho padrão (ver applyUiScale)
    QList<QFrame*> m_separators;
    bool focusCheckedCache = false;
    bool readModeOn = false;

    Qt::Alignment m_currentAlignment = Qt::AlignLeft;
    AlignScope    m_alignScope       = AlignScope::ThisDoc;
    QToolButton*  m_alignBtnLeft     = nullptr;
    QToolButton*  m_alignBtnCenter   = nullptr;
    QToolButton*  m_alignBtnRight    = nullptr;
    QToolButton*  m_alignBtnJustify  = nullptr;

    QLabel *reminderBadge = nullptr;
    QLabel *makeBadge(const QString &objectName);
    void positionBadge(QLabel *badge, QToolButton *button);
    void positionModeBadges();
    void positionReminderBadge();

    QLabel *pensarioBadge = nullptr;
    // Luzinhas de MODO ativo (editor focado, modo foco). Mesma linguagem visual
    // dos badges de aviso acima, mas acesas de forma continua: elas sinalizam um
    // estado ligado, nao um evento que acabou de acontecer.
    QLabel *readModeBadge = nullptr;
    QLabel *focusModeBadge = nullptr;
    QLabel *indentBadge = nullptr;
    void positionPensarioBadge();

    Qt::Edge m_barSide = Qt::TopEdge;

    // ---------------- Faixa estilo Word (TopToolbarRibbon.cpp) ----------------
    // Os botões são OS MESMOS da clássica (mesmos menus e sinais); a faixa só
    // muda a casa deles. Voltar pra clássica devolve tudo pros grupos.
    BarStyle m_barStyle = BarStyle::Classic;
    BarStyle m_lastRibbonStyle = BarStyle::RibbonSimple;
    QWidget* m_ribbon = nullptr;
    QWidget* m_ribbonTabsRow = nullptr;
    QToolButton* m_ribbonFile = nullptr;
    QSpacerItem* m_ribbonQGap = nullptr;    // Fichário: vão entre o Q e a primeira aba
    QList<QToolButton*> m_ribbonTabs;
    int m_ribbonTab = 0;
    QToolButton* m_ribbonSave = nullptr;
    QToolButton* m_ribbonModeBtn = nullptr;
    QToolButton* m_ribbonCollapse = nullptr;
    QWidget* m_ribbonBody = nullptr;
    CarouselViewport* m_ribbonCarousel = nullptr;
    QStackedWidget* m_ribbonStack = nullptr;
    QWidget* m_ribbonParking = nullptr;     // botões que a faixa não mostra (vivos, escondidos)
    QToolButton* m_ribbonAlign[4] = { nullptr, nullptr, nullptr, nullptr };
    QToolButton* m_ribbonScope = nullptr;
    QToolButton* m_pageButton = nullptr;
    bool m_ribbonPeek = false;              // só abas: a faixa desceu por cima do texto
    bool m_peekFilter = false;
    int m_ribbonTargetWidth = 0;
    bool m_ribbonFloating = false;
    // Fichário com Rolo (13/11, lab Escala, Tambor 3): sem abas; um rolo de
    // nomes na ponta, como um cadeado de segredo, troca a seção, e a faixa
    // vira junto como um tambor. As ferramentas de cada seção ficam paradas.
    bool m_ribbonReelMode = false;
    QWidget* m_ribbonReel = nullptr;
    QWidget* m_ribbonFlip = nullptr;
    qint64 m_lastReelWheel = 0;
    void stepRibbonSection(int delta);
    bool wheelRibbonSection(int angleDeltaY);
    int m_ribbonInsetL = 0;
    int m_ribbonInsetR = 0;
    int m_ribbonPageCenter = 0;
    QHash<QToolButton*, int> m_ribbonTabOf;
    QHash<QToolButton*, QString> m_classicText;
    QHash<QToolButton*, Qt::ToolButtonStyle> m_classicStyle;
    void buildRibbonChrome();
    void applyBarStyle();
    void rebuildRibbonPages();
    void setRibbonTab(int idx);
    void setRibbonPeek(bool on);
    void showBackstage();
    void showPagePopup();
    void styleRibbonButton(QToolButton* b, int role, const QString& label);
    void restoreClassicButtons();
    void syncRibbonAlign();
    void syncRibbonModeMenu();
    int ribbonHeight(bool withPeek) const;
    void applyRibbonMargins();
    void applyBinderIndent();
    QRect anchorRect(QToolButton* b) const;
    QString ribbonQss() const;
    QString ribbonRootQss() const;
    QString ribbonLookQss() const;
    static QIcon toolbarIcon(const QString& name, int px);

    // ---------------- Reorganização da barra (grupos arrastáveis) ----------------
    // Cada grupo (Projeto/Editor/Ferramentas/Mídia/Worldbuilding/Sistema) é uma
    // unidade de arrastar (ToolbarGroupWidget) — a ordem entre eles persiste em
    // QSettings.
    QBoxLayout* m_mainLayout = nullptr;
    QHash<QString, ToolbarGroupWidget*> m_groupWidgets;
    QStringList m_groupOrder;
    // Id estável -> botão. O id é o que vai pro QSettings, então NUNCA pode
    // virar índice nem depender da ordem de construção.
    QHash<QString, QToolButton*> m_buttonsById;
    // Composição atual de cada grupo (groupId -> ids de botão, em ordem).
    QHash<QString, QStringList> m_groupButtons;
    bool m_editMode = false;
    QSet<QToolButton*> m_rightBarButtons;   // vazio = todos aqui

    void buildGroups();
    void rebuildGroupLayout();
    QStringList defaultGroupOrder() const;
    QStringList loadGroupOrder() const;
    void saveGroupOrder() const;
    QHash<QString, QStringList> defaultButtonLayout() const;
    QHash<QString, QStringList> loadButtonLayout() const;
    void saveButtonLayout() const;
    void setEditMode(bool on);
    void onGroupDropped(const QString& draggedId, const QString& targetId);
    void onButtonDropped(const QString& buttonId, const QString& targetGroupId, int index);
};

#endif
