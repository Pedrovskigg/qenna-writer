#ifndef TOPTOOLBAR_H
#define TOPTOOLBAR_H

#include <QLabel>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QWidget>
class QToolButton;
class QLineEdit;
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
    QRect glossaryButtonGlobalRect() const;
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
    void sceneVarRequested();
    void boldToggled(bool enabled);
    void italicToggled(bool enabled);
    void underlineToggled(bool enabled);
    void strikethroughToggled(bool enabled);
    void alignmentRequested(Qt::Alignment alignment, TopToolbar::AlignScope scope);
    // Placeholders — ainda sem implementação
    void glossaryRequested();
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
    QToolButton *glossaryButton;
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

    QList<QPair<QToolButton*, QString>> iconBindings;
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
