#ifndef TOPTOOLBAR_H
#define TOPTOOLBAR_H

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QToolButton;
class FontPickerPopup;
class QWidget;

class TopToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit TopToolbar(QWidget *parent = nullptr);

    void setFontFamilies(const QStringList &families, const QString &current);
    void setFontSize(int pt);
    void setLineHeightPercent(int percent);
    void setFirstLineIndentEnabled(bool enabled);
    void setParagraphSpacingBefore(int px);
    void setParagraphSpacingAfter(int px);
    void setBoldChecked(bool checked);
    void setItalicChecked(bool checked);
    void setUnderlineChecked(bool checked);
    void setStrikethroughChecked(bool checked);
    void setFocusModeChecked(bool checked);
    void setDocumentTitle(const QString &title);
    // x em coords locais da TopToolbar; passe -1 para retomar o centro geométrico.
    void setTitleAnchorX(int x);

    QRect immersiveSoundButtonGlobalRect() const;
    QRect glossaryButtonGlobalRect() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

signals:
    void fontFamilyChanged(const QString &family);
    void fontSizeChanged(int pt);
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
    void refMenuToggleRequested();
    void boldToggled(bool enabled);
    void italicToggled(bool enabled);
    void underlineToggled(bool enabled);
    void strikethroughToggled(bool enabled);
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
    QToolButton *imageButton;
    QToolButton *reminderButton;
    QToolButton *immersiveSoundButton;
    QToolButton *themePanelButton;
    QToolButton *settingsButton;
    QToolButton *fullscreenButton;
    QToolButton *refMenuButton;
    QLabel *docTitleLabel;

    QIcon focusOffIcon;
    QIcon focusOnIcon;

    FontPickerPopup *fontPicker;

    QLabel *sizeStepperValueLabel;
    QList<QAction*> sizePresetActions;

    QStringList fontFamilies;
    QString currentFontFamily;
    int currentFontSize;
    int currentLineHeightPercent;
    int currentParaSpaceBefore = 0;
    int currentParaSpaceAfter = 0;
    int titleAnchorX = -1;
    QLabel *paraBeforeValueLabel = nullptr;
    QLabel *paraAfterValueLabel = nullptr;

    void buildSizeMenu();
    void buildSpacingMenu();
    void positionDocTitle();
    void updateSizeMenuState();
    void updateSpacingMenuChecks();
    void applySize(int pt);
    void applyParaSpaceBefore(int px);
    void applyParaSpaceAfter(int px);
    void applyFontButtonStyle();
    void applyTheme();
    void applyRootStyle();
    void reloadIcons();

    QList<QPair<QToolButton*, QString>> iconBindings;
    bool focusCheckedCache = false;
};

#endif
