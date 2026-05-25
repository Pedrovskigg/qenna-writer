#include "TopToolbar.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPair>
#include <QSignalBlocker>
#include <QToolButton>
#include <QWidgetAction>

#include "FontPickerPopup.h"
#include "IconUtils.h"
#include "Theme.h"

namespace {

// Cores dos ícones vêm do tema; reavaliadas em cada loadIcon, pra que reload
// após troca de tema reflita as cores atuais (e nunca fiquem invisíveis).
QString iconNormalColor()   { return Theme::textMuted(); }
QString iconHoverColor()    { return Theme::textPrimary(); }
QString iconSelectedColor() { return Theme::textBright(); }

constexpr int kBarHeight = 48;
constexpr int kIconButtonSize = 32;
constexpr int kIconSize = 20;

QToolButton *makeFlatButton(QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setToolButtonStyle(Qt::ToolButtonTextOnly);
    b->setAutoRaise(true);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QToolButton *makeIconButton(QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setToolButtonStyle(Qt::ToolButtonIconOnly);
    b->setAutoRaise(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(kIconButtonSize, kIconButtonSize);
    b->setIconSize(QSize(kIconSize, kIconSize));
    return b;
}

QIcon loadIcon(const QString &name)
{
    return IconUtils::loadToolbarIcon(
        QStringLiteral(":/icons/%1").arg(name),
        QColor(iconNormalColor()),
        QColor(iconHoverColor()),
        QColor(iconSelectedColor()));
}

QFrame *makeVSeparator(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setObjectName(QStringLiteral("ttbVSep"));
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Plain);
    line->setFixedSize(1, 22);
    return line;
}

int fontButtonPointSize(int editorPt)
{
    return qBound(10, editorPt * 2 / 3, 18);
}

}

TopToolbar::TopToolbar(QWidget *parent)
    : QWidget(parent)
    , newProjectButton(makeIconButton(this))
    , openProjectButton(makeIconButton(this))
    , saveProjectButton(makeIconButton(this))
    , boldButton(makeIconButton(this))
    , italicButton(makeIconButton(this))
    , underlineButton(makeIconButton(this))
    , strikethroughButton(makeIconButton(this))
    , glossaryButton(makeIconButton(this))
    , readModeButton(makeIconButton(this))
    , focusButton(makeIconButton(this))
    , searchButton(makeIconButton(this))
    , fontButton(makeFlatButton(this))
    , sizeButton(makeFlatButton(this))
    , lineHeightButton(makeFlatButton(this))
    , indentButton(makeFlatButton(this))
    , imageButton(makeIconButton(this))
    , reminderButton(makeIconButton(this))
    , immersiveSoundButton(makeIconButton(this))
    , themePanelButton(makeIconButton(this))
    , settingsButton(makeIconButton(this))
    , fullscreenButton(makeIconButton(this))
    , refMenuButton(makeIconButton(this))
    , docTitleLabel(new QLabel(this))
    , fontPicker(nullptr)
    , sizeStepperValueLabel(nullptr)
    , currentFontFamily(QStringLiteral("Alegreya"))
    , currentFontSize(16)
    , currentLineHeightPercent(170)
{
    setObjectName(QStringLiteral("topToolbar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(kBarHeight);

    focusOffIcon = loadIcon(QStringLiteral("focusmode-off.svg"));
    focusOnIcon  = loadIcon(QStringLiteral("focusmode-on.svg"));

    auto bindIcon = [this](QToolButton* b, const QString& name) {
        b->setIcon(loadIcon(name));
        iconBindings.append({b, name});
    };

    // ---------------- Grupo A: Projeto ----------------
    newProjectButton->setObjectName(QStringLiteral("ttbProject"));
    bindIcon(newProjectButton, QStringLiteral("newproject.svg"));
    newProjectButton->setToolTip(tr("Novo projeto"));
    connect(newProjectButton, &QToolButton::clicked, this, &TopToolbar::newProjectRequested);

    openProjectButton->setObjectName(QStringLiteral("ttbProject"));
    bindIcon(openProjectButton, QStringLiteral("loadproject.svg"));
    openProjectButton->setToolTip(tr("Abrir projeto"));
    connect(openProjectButton, &QToolButton::clicked, this, &TopToolbar::openProjectRequested);

    saveProjectButton->setObjectName(QStringLiteral("ttbProject"));
    bindIcon(saveProjectButton, QStringLiteral("save-project.svg"));
    saveProjectButton->setToolTip(tr("Salvar projeto (Ctrl+S)"));
    connect(saveProjectButton, &QToolButton::clicked, this, &TopToolbar::saveProjectRequested);

    // ---------------- Grupo B: Formatação inline ----------------
    boldButton->setObjectName(QStringLiteral("ttbInline"));
    bindIcon(boldButton, QStringLiteral("bold.svg"));
    boldButton->setCheckable(true);
    boldButton->setToolTip(tr("Negrito (Ctrl+B)"));
    connect(boldButton, &QToolButton::toggled, this, &TopToolbar::boldToggled);

    italicButton->setObjectName(QStringLiteral("ttbInline"));
    bindIcon(italicButton, QStringLiteral("italic.svg"));
    italicButton->setCheckable(true);
    italicButton->setToolTip(tr("Itálico (Ctrl+I)"));
    connect(italicButton, &QToolButton::toggled, this, &TopToolbar::italicToggled);

    underlineButton->setObjectName(QStringLiteral("ttbInline"));
    bindIcon(underlineButton, QStringLiteral("underline.svg"));
    underlineButton->setCheckable(true);
    underlineButton->setToolTip(tr("Sublinhado (Ctrl+U)"));
    connect(underlineButton, &QToolButton::toggled, this, &TopToolbar::underlineToggled);

    strikethroughButton->setObjectName(QStringLiteral("ttbInline"));
    bindIcon(strikethroughButton, QStringLiteral("strikethrough.svg"));
    strikethroughButton->setCheckable(true);
    strikethroughButton->setToolTip(tr("Tachado (Ctrl+Shift+S)"));
    connect(strikethroughButton, &QToolButton::toggled, this, &TopToolbar::strikethroughToggled);

    // ---------------- Grupo C: Ferramentas ----------------
    glossaryButton->setObjectName(QStringLiteral("ttbTool"));
    bindIcon(glossaryButton, QStringLiteral("glossary.svg"));
    glossaryButton->setToolTip(tr("Glossário"));
    connect(glossaryButton, &QToolButton::clicked, this, &TopToolbar::glossaryRequested);

    readModeButton->setObjectName(QStringLiteral("ttbTool"));
    bindIcon(readModeButton, QStringLiteral("readmode.svg"));
    readModeButton->setCheckable(true);
    readModeButton->setToolTip(tr("Modo leitura"));
    connect(readModeButton, &QToolButton::toggled, this, &TopToolbar::readModeToggled);

    focusButton->setObjectName(QStringLiteral("ttbTool"));
    focusButton->setIcon(focusOffIcon);
    focusButton->setCheckable(true);
    focusButton->setToolTip(tr("Modo foco"));
    connect(focusButton, &QToolButton::toggled, this, [this](bool on) {
        focusCheckedCache = on;
        focusButton->setIcon(on ? focusOnIcon : focusOffIcon);
        emit focusModeToggled(on);
    });

    searchButton->setObjectName(QStringLiteral("ttbTool"));
    bindIcon(searchButton, QStringLiteral("search.svg"));
    searchButton->setToolTip(tr("Buscar"));
    connect(searchButton, &QToolButton::clicked, this, &TopToolbar::searchRequested);

    // ---------------- Grupo D: Tipografia ----------------
    fontButton->setObjectName(QStringLiteral("ttbFont"));
    fontButton->setText(currentFontFamily);
    applyFontButtonStyle();

    fontPicker = new FontPickerPopup(this);
    connect(fontPicker, &FontPickerPopup::fontSelected, this, [this](const QString &family) {
        currentFontFamily = family;
        fontButton->setText(family);
        applyFontButtonStyle();
        emit fontFamilyChanged(family);
    });
    connect(fontButton, &QToolButton::clicked, this, [this]() {
        if (!fontPicker) return;
        fontPicker->setFontFamilies(fontFamilies, currentFontFamily);
        const QPoint anchor = fontButton->mapToGlobal(QPoint(0, fontButton->height()));
        fontPicker->showAtBelow(anchor);
    });

    const QSize iconSize(kIconSize, kIconSize);

    sizeButton->setObjectName(QStringLiteral("ttbSize"));
    sizeButton->setPopupMode(QToolButton::InstantPopup);
    sizeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    bindIcon(sizeButton, QStringLiteral("font-size.svg"));
    sizeButton->setIconSize(iconSize);
    sizeButton->setText(QString::number(currentFontSize));
    sizeButton->setToolTip(tr("Tamanho da fonte"));

    lineHeightButton->setObjectName(QStringLiteral("ttbLineHeight"));
    lineHeightButton->setPopupMode(QToolButton::InstantPopup);
    lineHeightButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    bindIcon(lineHeightButton, QStringLiteral("text-spacing.svg"));
    lineHeightButton->setIconSize(iconSize);
    lineHeightButton->setText(QString::number(currentLineHeightPercent / 100.0, 'f', 1));
    lineHeightButton->setToolTip(tr("Espaçamento"));

    indentButton->setObjectName(QStringLiteral("ttbIndent"));
    indentButton->setText(QStringLiteral("¶"));
    indentButton->setCheckable(true);
    indentButton->setChecked(true);
    indentButton->setToolTip(tr("Identação de parágrafo"));
    connect(indentButton, &QToolButton::toggled, this, &TopToolbar::firstLineIndentToggled);

    // ---------------- Grupo E: Mídia ----------------
    imageButton->setObjectName(QStringLiteral("ttbMedia"));
    bindIcon(imageButton, QStringLiteral("add-image.svg"));
    imageButton->setToolTip(tr("Adicionar imagem"));
    connect(imageButton, &QToolButton::clicked, this, &TopToolbar::addImageRequested);

    reminderButton->setObjectName(QStringLiteral("ttbMedia"));
    bindIcon(reminderButton, QStringLiteral("reminder.svg"));
    reminderButton->setToolTip(tr("Lembretes"));
    connect(reminderButton, &QToolButton::clicked, this, &TopToolbar::reminderRequested);

    immersiveSoundButton->setObjectName(QStringLiteral("ttbMedia"));
    bindIcon(immersiveSoundButton, QStringLiteral("immersive-sound.svg"));
    immersiveSoundButton->setToolTip(tr("Som imersivo"));
    connect(immersiveSoundButton, &QToolButton::clicked, this, &TopToolbar::immersiveSoundRequested);

    // ---------------- Grupo F: Sistema ----------------
    themePanelButton->setObjectName(QStringLiteral("ttbSystem"));
    bindIcon(themePanelButton, QStringLiteral("theme-panel.svg"));
    themePanelButton->setToolTip(tr("Temas"));
    connect(themePanelButton, &QToolButton::clicked, this, &TopToolbar::themePanelRequested);

    settingsButton->setObjectName(QStringLiteral("ttbSystem"));
    bindIcon(settingsButton, QStringLiteral("settings.svg"));
    settingsButton->setToolTip(tr("Configurações"));
    connect(settingsButton, &QToolButton::clicked, this, &TopToolbar::settingsRequested);

    fullscreenButton->setObjectName(QStringLiteral("ttbSystem"));
    bindIcon(fullscreenButton, QStringLiteral("fullscreen.svg"));
    fullscreenButton->setCheckable(true);
    fullscreenButton->setToolTip(tr("Tela cheia"));
    connect(fullscreenButton, &QToolButton::toggled, this, &TopToolbar::fullscreenToggled);

    refMenuButton->setObjectName(QStringLiteral("ttbSystem"));
    bindIcon(refMenuButton, QStringLiteral("refmenu.svg"));
    refMenuButton->setToolTip(tr("Painel de Referência"));
    connect(refMenuButton, &QToolButton::clicked, this, &TopToolbar::refMenuToggleRequested);

    // ---------------- Título do documento (centro) ----------------
    docTitleLabel->setObjectName(QStringLiteral("ttbDocTitle"));
    docTitleLabel->setAlignment(Qt::AlignCenter);
    docTitleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    docTitleLabel->setText(QString());
    // Sai do fluxo do layout: posicionado manualmente sobre o centro real da
    // TopToolbar, para não deslocar quando um lado fica mais largo que o outro.
    docTitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    // ---------------- Layout ----------------
    // Esquerda: Projeto (new/open/save) + Editor (font/size/lineHeight/indent/B/I)
    // Centro: título do documento
    // Direita: Ferramentas + Mídia + Sistema + RefMenu
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 4, 14, 4);
    layout->setSpacing(6);

    // --- Esquerda: Projeto ---
    layout->addWidget(newProjectButton);
    layout->addWidget(openProjectButton);
    layout->addWidget(saveProjectButton);
    layout->addWidget(makeVSeparator(this));

    // --- Esquerda: Editor (tipografia + inline) ---
    layout->addWidget(fontButton);
    layout->addWidget(sizeButton);
    layout->addWidget(lineHeightButton);
    layout->addWidget(indentButton);
    layout->addWidget(boldButton);
    layout->addWidget(italicButton);
    layout->addWidget(underlineButton);
    layout->addWidget(strikethroughButton);

    // --- Centro: stretch reservado (título é posicionado manualmente) ---
    layout->addStretch(1);

    // --- Direita: Ferramentas ---
    layout->addWidget(glossaryButton);
    layout->addWidget(readModeButton);
    layout->addWidget(focusButton);
    layout->addWidget(searchButton);
    layout->addWidget(makeVSeparator(this));

    // --- Direita: Mídia ---
    layout->addWidget(imageButton);
    layout->addWidget(reminderButton);
    layout->addWidget(immersiveSoundButton);
    layout->addWidget(makeVSeparator(this));

    // --- Direita: Sistema ---
    layout->addWidget(themePanelButton);
    layout->addWidget(settingsButton);
    layout->addWidget(fullscreenButton);
    layout->addWidget(makeVSeparator(this));
    layout->addWidget(refMenuButton);

    buildSizeMenu();
    buildSpacingMenu();
    applyRootStyle();

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &TopToolbar::applyTheme);
}

void TopToolbar::applyRootStyle()
{
    // Background da toolbar segue o app (sem caixa), botões transparentes com
    // hover sutil. As cores dos ícones já vêm tintadas via loadIcon().
    setStyleSheet(QStringLiteral(R"(
        QWidget#topToolbar {
            background: %1;
        }
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 6px;
            color: %2;
        }
        QToolButton:hover {
            background: %3;
            border-color: %4;
            color: %5;
        }
        QToolButton:checked {
            background: %6;
            border-color: %5;
            color: %5;
        }
        QToolButton::menu-indicator { image: none; width: 0; }
        QFrame#ttbVSep {
            color: %4;
            background: %4;
        }
    )").arg(
        Theme::appBackground(),    // 1 — fundo
        Theme::textPrimary(),      // 2 — texto dos botões em estado normal
        Theme::hoverOverlay(),     // 3 — hover bg
        Theme::subtleBorder(),     // 4 — borda hover / separador
        Theme::textBright(),       // 5 — texto hover/checked
        Theme::pressedOverlay()    // 6 — checked bg
    ));

    if (docTitleLabel) {
        docTitleLabel->setStyleSheet(QStringLiteral(
            "QLabel#ttbDocTitle {"
            "  color: %1;"
            "  background: transparent;"
            "  font-family: 'Lora','Crimson Text',serif;"
            "  font-size: 18px;"
            "  font-weight: 700;"
            "}").arg(Theme::textBright()));
    }
}

void TopToolbar::reloadIcons()
{
    focusOffIcon = loadIcon(QStringLiteral("focusmode-off.svg"));
    focusOnIcon  = loadIcon(QStringLiteral("focusmode-on.svg"));
    if (focusButton) {
        focusButton->setIcon(focusCheckedCache ? focusOnIcon : focusOffIcon);
    }
    for (const auto& pair : iconBindings) {
        if (pair.first) pair.first->setIcon(loadIcon(pair.second));
    }
}

void TopToolbar::applyTheme()
{
    applyRootStyle();
    reloadIcons();
}

void TopToolbar::setDocumentTitle(const QString &title)
{
    if (!docTitleLabel) return;
    docTitleLabel->setText(title);
    docTitleLabel->adjustSize();
    positionDocTitle();
    docTitleLabel->raise();
}

void TopToolbar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    positionDocTitle();
}

void TopToolbar::setTitleAnchorX(int x)
{
    titleAnchorX = x;
    positionDocTitle();
}

QRect TopToolbar::immersiveSoundButtonGlobalRect() const
{
    if (!immersiveSoundButton) return QRect();
    const QPoint topLeft = immersiveSoundButton->mapToGlobal(QPoint(0, 0));
    return QRect(topLeft, immersiveSoundButton->size());
}

QRect TopToolbar::glossaryButtonGlobalRect() const
{
    if (!glossaryButton) return QRect();
    const QPoint topLeft = glossaryButton->mapToGlobal(QPoint(0, 0));
    return QRect(topLeft, glossaryButton->size());
}

void TopToolbar::positionDocTitle()
{
    if (!docTitleLabel) return;
    const int centerX = (titleAnchorX >= 0) ? titleAnchorX : (width() / 2);
    const int x = centerX - docTitleLabel->width() / 2;
    const int y = (height() - docTitleLabel->height()) / 2;
    docTitleLabel->move(x, y);
}

void TopToolbar::setFontFamilies(const QStringList &families, const QString &current)
{
    fontFamilies = families;
    currentFontFamily = current;
    fontButton->setText(currentFontFamily);
    applyFontButtonStyle();
}

void TopToolbar::setFontSize(int pt)
{
    currentFontSize = pt;
    updateSizeMenuState();
}

void TopToolbar::setLineHeightPercent(int percent)
{
    currentLineHeightPercent = percent;
    lineHeightButton->setText(QString::number(percent / 100.0, 'f', 1));
    updateSpacingMenuChecks();
}

void TopToolbar::setFirstLineIndentEnabled(bool enabled)
{
    QSignalBlocker block(indentButton);
    indentButton->setChecked(enabled);
}

void TopToolbar::setBoldChecked(bool checked)
{
    QSignalBlocker block(boldButton);
    boldButton->setChecked(checked);
}

void TopToolbar::setItalicChecked(bool checked)
{
    QSignalBlocker block(italicButton);
    italicButton->setChecked(checked);
}

void TopToolbar::setUnderlineChecked(bool checked)
{
    QSignalBlocker block(underlineButton);
    underlineButton->setChecked(checked);
}

void TopToolbar::setStrikethroughChecked(bool checked)
{
    QSignalBlocker block(strikethroughButton);
    strikethroughButton->setChecked(checked);
}

void TopToolbar::setFocusModeChecked(bool checked)
{
    if (!focusButton) return;
    QSignalBlocker block(focusButton);
    focusButton->setChecked(checked);
    focusCheckedCache = checked;
    focusButton->setIcon(checked ? focusOnIcon : focusOffIcon);
}

void TopToolbar::setParagraphSpacingBefore(int px)
{
    currentParaSpaceBefore = qBound(0, px, 32);
    if (paraBeforeValueLabel) {
        paraBeforeValueLabel->setText(QStringLiteral("%1 px").arg(currentParaSpaceBefore));
    }
}

void TopToolbar::setParagraphSpacingAfter(int px)
{
    currentParaSpaceAfter = qBound(0, px, 32);
    if (paraAfterValueLabel) {
        paraAfterValueLabel->setText(QStringLiteral("%1 px").arg(currentParaSpaceAfter));
    }
}

void TopToolbar::applyParaSpaceBefore(int px)
{
    const int next = qBound(0, px, 32);
    if (next == currentParaSpaceBefore) return;
    currentParaSpaceBefore = next;
    if (paraBeforeValueLabel) paraBeforeValueLabel->setText(QStringLiteral("%1 px").arg(next));
    emit paragraphSpacingBeforeChanged(next);
}

void TopToolbar::applyParaSpaceAfter(int px)
{
    const int next = qBound(0, px, 32);
    if (next == currentParaSpaceAfter) return;
    currentParaSpaceAfter = next;
    if (paraAfterValueLabel) paraAfterValueLabel->setText(QStringLiteral("%1 px").arg(next));
    emit paragraphSpacingAfterChanged(next);
}

void TopToolbar::buildSizeMenu()
{
    auto *menu = new QMenu(sizeButton);
    menu->setObjectName(QStringLiteral("ttbSizeMenu"));

    auto *stepperWidget = new QWidget(menu);
    stepperWidget->setObjectName(QStringLiteral("ttbSizeStepper"));
    auto *stepperLayout = new QHBoxLayout(stepperWidget);
    stepperLayout->setContentsMargins(10, 6, 10, 6);
    stepperLayout->setSpacing(6);

    auto *minus = new QToolButton(stepperWidget);
    minus->setObjectName(QStringLiteral("ttbSizeStep"));
    minus->setText(QStringLiteral("−"));
    minus->setFixedSize(28, 28);
    minus->setCursor(Qt::PointingHandCursor);
    minus->setAutoRaise(true);

    sizeStepperValueLabel = new QLabel(stepperWidget);
    sizeStepperValueLabel->setObjectName(QStringLiteral("ttbSizeValue"));
    sizeStepperValueLabel->setText(QString::number(currentFontSize));
    sizeStepperValueLabel->setAlignment(Qt::AlignCenter);
    sizeStepperValueLabel->setFixedWidth(40);

    auto *plus = new QToolButton(stepperWidget);
    plus->setObjectName(QStringLiteral("ttbSizeStep"));
    plus->setText(QStringLiteral("+"));
    plus->setFixedSize(28, 28);
    plus->setCursor(Qt::PointingHandCursor);
    plus->setAutoRaise(true);

    stepperLayout->addWidget(minus);
    stepperLayout->addWidget(sizeStepperValueLabel);
    stepperLayout->addWidget(plus);

    connect(minus, &QToolButton::clicked, this, [this]() { applySize(currentFontSize - 2); });
    connect(plus, &QToolButton::clicked, this, [this]() { applySize(currentFontSize + 2); });

    auto *stepperAction = new QWidgetAction(menu);
    stepperAction->setDefaultWidget(stepperWidget);
    menu->addAction(stepperAction);

    menu->addSeparator();

    sizePresetActions.clear();
    const QList<int> presets = {12, 14, 16, 18, 20, 24, 28};
    for (int s : presets) {
        QAction *a = menu->addAction(QString::number(s) + tr(" pt"));
        a->setCheckable(true);
        a->setChecked(s == currentFontSize);
        a->setData(s);
        connect(a, &QAction::triggered, this, [this, s]() { applySize(s); });
        sizePresetActions.append(a);
    }

    sizeButton->setMenu(menu);
}

void TopToolbar::updateSizeMenuState()
{
    sizeButton->setText(QString::number(currentFontSize));
    applyFontButtonStyle();
    if (sizeStepperValueLabel) {
        sizeStepperValueLabel->setText(QString::number(currentFontSize));
    }
    for (QAction *a : std::as_const(sizePresetActions)) {
        a->setChecked(a->data().toInt() == currentFontSize);
    }
}

void TopToolbar::applyFontButtonStyle()
{
    QFont f(currentFontFamily, fontButtonPointSize(currentFontSize));
    f.setBold(true);
    fontButton->setFont(f);
}

void TopToolbar::buildSpacingMenu()
{
    auto *menu = new QMenu(lineHeightButton);
    menu->setObjectName(QStringLiteral("ttbSpacingMenu"));

    // ---- Seção 1: Espaçamento entre linhas (presets) ----
    auto *headerLine = menu->addAction(tr("Entre linhas"));
    headerLine->setEnabled(false);

    const QList<QPair<int, QString>> spacings = {
        { 100, tr("Simples (1.0)") },
        { 115, tr("Justo (1.15)") },
        { 130, tr("Compacto (1.3)") },
        { 150, tr("Confortável (1.5)") },
        { 170, tr("Padrão (1.7)") },
        { 190, tr("Amplo (1.9)") },
        { 220, tr("Espaçoso (2.2)") },
    };

    for (const auto &sp : spacings) {
        const int percent = sp.first;
        QAction *a = menu->addAction(sp.second);
        a->setCheckable(true);
        a->setChecked(percent == currentLineHeightPercent);
        a->setData(percent);
        a->setProperty("ttbRole", QStringLiteral("lineHeight"));
        connect(a, &QAction::triggered, this, [this, percent]() {
            currentLineHeightPercent = percent;
            lineHeightButton->setText(QString::number(percent / 100.0, 'f', 1));
            emit lineHeightChanged(percent);
            updateSpacingMenuChecks();
        });
    }

    menu->addSeparator();

    // ---- Seção 2: Espaçamento ANTES do parágrafo ----
    auto *headerBefore = menu->addAction(tr("Antes do parágrafo"));
    headerBefore->setEnabled(false);

    auto *stepperBefore = new QWidget(menu);
    auto *layoutBefore = new QHBoxLayout(stepperBefore);
    layoutBefore->setContentsMargins(10, 4, 10, 4);
    layoutBefore->setSpacing(6);

    auto *minusBefore = new QToolButton(stepperBefore);
    minusBefore->setObjectName(QStringLiteral("ttbSizeStep"));
    minusBefore->setText(QStringLiteral("−"));
    minusBefore->setFixedSize(28, 28);
    minusBefore->setCursor(Qt::PointingHandCursor);
    minusBefore->setAutoRaise(true);

    paraBeforeValueLabel = new QLabel(stepperBefore);
    paraBeforeValueLabel->setObjectName(QStringLiteral("ttbSizeValue"));
    paraBeforeValueLabel->setText(QStringLiteral("%1 px").arg(currentParaSpaceBefore));
    paraBeforeValueLabel->setAlignment(Qt::AlignCenter);
    paraBeforeValueLabel->setFixedWidth(56);

    auto *plusBefore = new QToolButton(stepperBefore);
    plusBefore->setObjectName(QStringLiteral("ttbSizeStep"));
    plusBefore->setText(QStringLiteral("+"));
    plusBefore->setFixedSize(28, 28);
    plusBefore->setCursor(Qt::PointingHandCursor);
    plusBefore->setAutoRaise(true);

    layoutBefore->addWidget(minusBefore);
    layoutBefore->addWidget(paraBeforeValueLabel);
    layoutBefore->addWidget(plusBefore);

    connect(minusBefore, &QToolButton::clicked, this, [this]() { applyParaSpaceBefore(currentParaSpaceBefore - 2); });
    connect(plusBefore, &QToolButton::clicked, this, [this]() { applyParaSpaceBefore(currentParaSpaceBefore + 2); });

    auto *actionBefore = new QWidgetAction(menu);
    actionBefore->setDefaultWidget(stepperBefore);
    menu->addAction(actionBefore);

    menu->addSeparator();

    // ---- Seção 3: Espaçamento DEPOIS do parágrafo ----
    auto *headerAfter = menu->addAction(tr("Depois do parágrafo"));
    headerAfter->setEnabled(false);

    auto *stepperAfter = new QWidget(menu);
    auto *layoutAfter = new QHBoxLayout(stepperAfter);
    layoutAfter->setContentsMargins(10, 4, 10, 4);
    layoutAfter->setSpacing(6);

    auto *minusAfter = new QToolButton(stepperAfter);
    minusAfter->setObjectName(QStringLiteral("ttbSizeStep"));
    minusAfter->setText(QStringLiteral("−"));
    minusAfter->setFixedSize(28, 28);
    minusAfter->setCursor(Qt::PointingHandCursor);
    minusAfter->setAutoRaise(true);

    paraAfterValueLabel = new QLabel(stepperAfter);
    paraAfterValueLabel->setObjectName(QStringLiteral("ttbSizeValue"));
    paraAfterValueLabel->setText(QStringLiteral("%1 px").arg(currentParaSpaceAfter));
    paraAfterValueLabel->setAlignment(Qt::AlignCenter);
    paraAfterValueLabel->setFixedWidth(56);

    auto *plusAfter = new QToolButton(stepperAfter);
    plusAfter->setObjectName(QStringLiteral("ttbSizeStep"));
    plusAfter->setText(QStringLiteral("+"));
    plusAfter->setFixedSize(28, 28);
    plusAfter->setCursor(Qt::PointingHandCursor);
    plusAfter->setAutoRaise(true);

    layoutAfter->addWidget(minusAfter);
    layoutAfter->addWidget(paraAfterValueLabel);
    layoutAfter->addWidget(plusAfter);

    connect(minusAfter, &QToolButton::clicked, this, [this]() { applyParaSpaceAfter(currentParaSpaceAfter - 2); });
    connect(plusAfter, &QToolButton::clicked, this, [this]() { applyParaSpaceAfter(currentParaSpaceAfter + 2); });

    auto *actionAfter = new QWidgetAction(menu);
    actionAfter->setDefaultWidget(stepperAfter);
    menu->addAction(actionAfter);

    lineHeightButton->setMenu(menu);
}

void TopToolbar::updateSpacingMenuChecks()
{
    QMenu *menu = lineHeightButton ? lineHeightButton->menu() : nullptr;
    if (!menu) return;
    for (QAction *a : menu->actions()) {
        if (a->property("ttbRole").toString() == QLatin1String("lineHeight")) {
            a->setChecked(a->data().toInt() == currentLineHeightPercent);
        }
    }
}

void TopToolbar::applySize(int pt)
{
    const int minSize = 10;
    const int maxSize = 48;
    int next = qBound(minSize, pt, maxSize);
    if (next == currentFontSize) return;
    currentFontSize = next;
    updateSizeMenuState();
    emit fontSizeChanged(next);
}
