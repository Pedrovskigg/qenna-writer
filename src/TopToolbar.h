#ifndef TOPTOOLBAR_H
#define TOPTOOLBAR_H

#include <QHash>
#include <QLabel>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QWidget>
class QToolButton;
class QLineEdit;
class QFrame;
class QMenu;
class QAction;
class QBoxLayout;
class FontPickerPopup;
class ToolbarGroupWidget;
class QWidget;

class TopToolbar : public QWidget
{
    Q_OBJECT

public:
    enum class AlignScope { ThisDoc, AllDocs, Manuscript, Drawers };
    Q_ENUM(AlignScope)

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
    void setFocusModeChecked(bool checked);
    void setFullscreenChecked(bool checked);
    // `subtitle` opcional: quando presente, mostra `title` maior em cima e
    // `subtitle` menor embaixo (ex.: capítulo em cima, "Cena x" embaixo).
    void setDocumentTitle(const QString &title, const QString &subtitle = QString());
    // Botão discreto ao lado do subtítulo — só faz sentido quando o
    // subtítulo é uma cena de verdade (viewMode SceneDoc), abre o popup de
    // variações. Escondido por padrão.
    void setSceneVarButtonVisible(bool visible);
    QRect sceneVarButtonGlobalRect() const;
    // x em coords locais da TopToolbar; passe -1 para retomar o centro geométrico.
    void setTitleAnchorX(int x);

    QRect immersiveSoundButtonGlobalRect() const;
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
    void setCurrentAlignment(Qt::Alignment alignment);

protected:
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
    void sceneVarRequested();
    void boldToggled(bool enabled);
    void italicToggled(bool enabled);
    void underlineToggled(bool enabled);
    void strikethroughToggled(bool enabled);
    void alignmentRequested(Qt::Alignment alignment, TopToolbar::AlignScope scope);
    // Abre o painel de Estatísticas (personagens/manuscrito).
    void statisticsRequested();
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
    QLabel *docTitleLabel;
    QLabel *docSubtitleLabel; // "Cena x" embaixo do título, quando aplicável
    QToolButton *sceneVarButton; // abre popup de variações da cena atual

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
    int titleAnchorX = -1;
    // Texto completo (não-elidido); positionDocTitle() re-elide a cada reposicionamento
    // conforme o espaço livre entre os grupos de botões muda.
    QString m_rawTitle;
    QString m_rawSubtitle;
    // O que o CALLER pediu (setDocumentTitle/setSceneVarButtonVisible) — não o
    // estado atual do widget, que positionDocTitle() pode esconder por falta
    // de espaço. Sem essa distinção, o título nunca mais voltaria a aparecer
    // depois de sumir uma vez numa janela estreita.
    bool m_subtitleWanted = false;
    bool m_sceneVarWanted = false;
    QLabel *paraBeforeValueLabel = nullptr;
    QLabel *paraAfterValueLabel = nullptr;

    void buildSizeMenu();
    void buildSpacingMenu();
    void buildAlignMenu();
    void updateAlignButtonIcon();
    void positionDocTitle();
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

    // Menu de overflow — botões dispensáveis (tema, tela cheia, som imersivo,
    // ferramentas de worldbuilding etc.) somem pra dentro de "⋯" quando a
    // janela é estreita demais pra caber tudo, do menos essencial pro mais
    // essencial (índice 0 primeiro), e voltam na ordem inversa conforme sobra
    // espaço de novo. Ver updateOverflow().
    void buildOverflowMenu();
    void updateOverflow();
    void collapseToOverflow(QToolButton* btn);
    void restoreFromOverflow(QToolButton* btn);
    QToolButton* overflowButton = nullptr;
    QMenu* m_overflowMenu = nullptr;
    QList<QToolButton*> m_collapsePriority;
    QHash<QToolButton*, QAction*> m_overflowActions;

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
    void positionReminderBadge();

    QLabel *pensarioBadge = nullptr;
    void positionPensarioBadge();

    Qt::Edge m_barSide = Qt::TopEdge;

    // ---------------- Reorganização da barra (grupos arrastáveis) ----------------
    // Cada grupo (Projeto/Editor/Ferramentas/Mídia/Worldbuilding/Sistema) é uma
    // unidade de arrastar (ToolbarGroupWidget) — a ordem entre eles persiste em
    // QSettings. overflowButton fica DE FORA do sistema de
    // grupos, sempre fixos (início/fim), igual antes.
    QBoxLayout* m_mainLayout = nullptr;
    QHash<QString, ToolbarGroupWidget*> m_groupWidgets;
    QStringList m_groupOrder;
    // Id estável -> botão. O id é o que vai pro QSettings, então NUNCA pode
    // virar índice nem depender da ordem de construção.
    QHash<QString, QToolButton*> m_buttonsById;
    // Composição atual de cada grupo (groupId -> ids de botão, em ordem).
    QHash<QString, QStringList> m_groupButtons;
    bool m_editMode = false;

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
