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
class FontPickerPopup;
class QWidget;

class TopToolbar : public QWidget
{
    Q_OBJECT

public:
    enum class AlignScope { ThisDoc, AllDocs, Manuscript, Drawers };
    Q_ENUM(AlignScope)

    explicit TopToolbar(QWidget *parent = nullptr);

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

    void setReminderBadge(bool active);
    void pulsePensarioBadge();
    void setCurrentAlignment(Qt::Alignment alignment);

protected:
    void resizeEvent(QResizeEvent *event) override;

signals:
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
    void applySize(qreal pt);
    void commitSizeEditor();
    static QString sizeText(qreal pt);
    void applyParaSpaceBefore(int px);
    void applyParaSpaceAfter(int px);
    void applyFontButtonStyle();
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
};

#endif
