#include "TopToolbar.h"

#include <QAction>
#include <QButtonGroup>
#include <QDoubleValidator>
#include <QEasingCurve>
#include <QFont>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QPair>
#include <QPropertyAnimation>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
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

int fontButtonPointSize(qreal editorPt)
{
    return qBound(10, qRound(editorPt * 2.0 / 3.0), 18);
}

}

// "14" para inteiros, "14.5" para meios-pontos. Sem ".0" pendurado.
QString TopToolbar::sizeText(qreal pt)
{
    if (qFuzzyCompare(pt, qRound(pt)))
        return QString::number(qRound(pt));
    return QString::number(pt, 'f', 1);
}

TopToolbar::TopToolbar(QWidget *parent)
    : QWidget(parent)
    , homeButton(makeIconButton(this))
    , newProjectButton(makeIconButton(this))
    , openProjectButton(makeIconButton(this))
    , saveProjectButton(makeIconButton(this))
    , exportButton(makeIconButton(this))
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
    , alignButton(makeIconButton(this))
    , imageButton(makeIconButton(this))
    , reminderButton(makeIconButton(this))
    , immersiveSoundButton(makeIconButton(this))
    , themePanelButton(makeIconButton(this))
    , settingsButton(makeIconButton(this))
    , fullscreenButton(makeIconButton(this))
    , refMenuButton(makeIconButton(this))
    , pensarioButton(makeIconButton(this))
    , helpButton(new QToolButton(this))
    , construtorButton(makeIconButton(this))
    , docTitleLabel(new QLabel(this))
    , docSubtitleLabel(new QLabel(this))
    , sceneVarButton(makeIconButton(this))
    , fontPicker(nullptr)
    , sizeStepperEdit(nullptr)
    , currentFontFamily(QStringLiteral("Alegreya"))
    , currentFontSize(16)
    , currentLineHeightPercent(115)
{
    setObjectName(QStringLiteral("topToolbar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(kBarHeight);

    reminderBadge = new QLabel(this);
    reminderBadge->setObjectName(QStringLiteral("reminderBadge"));
    reminderBadge->setFixedSize(9, 9);
    reminderBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    reminderBadge->setVisible(false);

    pensarioBadge = new QLabel(this);
    pensarioBadge->setObjectName(QStringLiteral("pensarioBadge"));
    pensarioBadge->setFixedSize(9, 9);
    pensarioBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    pensarioBadge->setVisible(false);

    focusOffIcon = loadIcon(QStringLiteral("focusmode-off.svg"));
    focusOnIcon  = loadIcon(QStringLiteral("focusmode-on.svg"));
    readModeOffIcon = loadIcon(QStringLiteral("focusededitor-off.svg"));
    readModeOnIcon  = loadIcon(QStringLiteral("focusededitor-on.svg"));

    auto bindIcon = [this](QToolButton* b, const QString& name) {
        b->setIcon(loadIcon(name));
        iconBindings.append({b, name});
    };

    // ---------------- Grupo A: Projeto ----------------
    homeButton->setObjectName(QStringLiteral("ttbProject"));
    bindIcon(homeButton, QStringLiteral("home.svg"));
    homeButton->setToolTip(tr("Voltar ao menu principal"));
    connect(homeButton, &QToolButton::clicked, this, &TopToolbar::mainMenuRequested);

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

    exportButton->setObjectName(QStringLiteral("ttbProject"));
    bindIcon(exportButton, QStringLiteral("download.svg"));
    exportButton->setToolTip(tr("Exportar projeto"));
    connect(exportButton, &QToolButton::clicked, this, &TopToolbar::exportRequested);

    helpButton->setObjectName(QStringLiteral("ttbProject"));
    helpButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    helpButton->setAutoRaise(true);
    helpButton->setCursor(Qt::PointingHandCursor);
    helpButton->setFixedSize(kIconButtonSize, kIconButtonSize);
    {
        QFont helpFont = helpButton->font();
        helpFont.setPointSize(13);
        helpFont.setBold(true);
        helpButton->setFont(helpFont);
    }
    helpButton->setText(QStringLiteral("?"));
    helpButton->setToolTip(tr("Ajuda"));
    connect(helpButton, &QToolButton::clicked, this, &TopToolbar::helpRequested);

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

    // Editor focado (distraction-free). Toggle de preferência: alterna o ícone
    // on/off mas NÃO mantém estado "checked" destacado na barra.
    readModeButton->setObjectName(QStringLiteral("ttbTool"));
    readModeButton->setIcon(readModeOffIcon);
    readModeButton->setToolTip(tr("Editor focado"));
    connect(readModeButton, &QToolButton::clicked, this, [this]() {
        readModeOn = !readModeOn;
        readModeButton->setIcon(readModeOn ? readModeOnIcon : readModeOffIcon);
        emit readModeToggled(readModeOn);
    });

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
    sizeButton->setIconSize(QSize(kIconSize, kIconSize));
    sizeButton->setText(sizeText(currentFontSize));
    sizeButton->setFixedSize(26, kIconButtonSize);
    sizeButton->setToolTip(tr("Tamanho da fonte"));

    lineHeightButton->setObjectName(QStringLiteral("ttbLineHeight"));
    lineHeightButton->setPopupMode(QToolButton::InstantPopup);
    lineHeightButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    bindIcon(lineHeightButton, QStringLiteral("text-spacing.svg"));
    lineHeightButton->setIconSize(QSize(kIconSize, kIconSize));
    lineHeightButton->setText(QString::number(currentLineHeightPercent / 100.0, 'f', 1));
    lineHeightButton->setFixedSize(26, kIconButtonSize);
    lineHeightButton->setToolTip(tr("Espaçamento"));

    indentButton->setObjectName(QStringLiteral("ttbIndent"));
    indentButton->setText(QStringLiteral("¶"));
    indentButton->setCheckable(true);
    indentButton->setChecked(true);
    indentButton->setToolTip(tr("Identação de parágrafo"));
    connect(indentButton, &QToolButton::toggled, this, &TopToolbar::firstLineIndentToggled);

    alignButton->setObjectName(QStringLiteral("ttbAlign"));
    alignButton->setPopupMode(QToolButton::InstantPopup);
    alignButton->setToolTip(tr("Alinhamento"));
    updateAlignButtonIcon();

    // ---------------- Grupo B (continuação): Imagem ----------------
    imageButton->setObjectName(QStringLiteral("ttbInline"));
    bindIcon(imageButton, QStringLiteral("add-image.svg"));
    imageButton->setToolTip(tr("Adicionar imagem"));
    connect(imageButton, &QToolButton::clicked, this, &TopToolbar::addImageRequested);

    // ---------------- Grupo E: Mídia ----------------
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

    pensarioButton->setObjectName(QStringLiteral("ttbSystem"));
    bindIcon(pensarioButton, QStringLiteral("pensario.svg"));
    pensarioButton->setToolTip(tr("Pensário"));
    connect(pensarioButton, &QToolButton::clicked, this, &TopToolbar::pensarioToggleRequested);

    construtorButton->setObjectName(QStringLiteral("ttbSystem"));
    bindIcon(construtorButton, QStringLiteral("construtor.svg"));
    construtorButton->setToolTip(tr("Construtor"));
    connect(construtorButton, &QToolButton::clicked, this, &TopToolbar::construtorToggleRequested);

    // ---------------- Título do documento (centro) ----------------
    docTitleLabel->setObjectName(QStringLiteral("ttbDocTitle"));
    docTitleLabel->setAlignment(Qt::AlignCenter);
    docTitleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    docTitleLabel->setText(QString());
    // Sai do fluxo do layout: posicionado manualmente sobre o centro real da
    // TopToolbar, para não deslocar quando um lado fica mais largo que o outro.
    docTitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    // Subtítulo opcional embaixo do título (ex.: "Cena x" sob o capítulo) —
    // mesmo tratamento de posicionamento manual, some quando vazio.
    docSubtitleLabel->setObjectName(QStringLiteral("ttbDocSubtitle"));
    docSubtitleLabel->setAlignment(Qt::AlignCenter);
    docSubtitleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    docSubtitleLabel->setText(QString());
    docSubtitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    docSubtitleLabel->setVisible(false);

    // Botão de variações — só aparece junto do subtítulo em modo SceneDoc.
    sceneVarButton->setObjectName(QStringLiteral("ttbSceneVar"));
    sceneVarButton->setFixedSize(20, 20);
    sceneVarButton->setIconSize(QSize(14, 14));
    bindIcon(sceneVarButton, QStringLiteral("scene-var.svg"));
    sceneVarButton->setToolTip(tr("Variações desta cena"));
    sceneVarButton->setVisible(false);
    connect(sceneVarButton, &QToolButton::clicked, this, &TopToolbar::sceneVarRequested);

    // ---------------- Layout ----------------
    // Esquerda: Projeto (new/open/save) + Editor (font/size/lineHeight/indent/B/I)
    // Centro: título do documento
    // Direita: Ferramentas + Mídia + Worldbuilding (Construtor/Pensário/RefMenu) + Sistema
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 4, 14, 4);
    layout->setSpacing(6);

    // --- Esquerda: Projeto ---
    layout->addWidget(homeButton);
    layout->addWidget(newProjectButton);
    layout->addWidget(openProjectButton);
    layout->addWidget(saveProjectButton);
    layout->addWidget(exportButton);
    layout->addWidget(helpButton);
    layout->addWidget(makeVSeparator(this));

    // --- Esquerda: Editor (tipografia + inline) ---
    layout->addWidget(fontButton);
    layout->addWidget(sizeButton);
    layout->addWidget(lineHeightButton);
    layout->addWidget(indentButton);
    layout->addWidget(alignButton);
    layout->addWidget(boldButton);
    layout->addWidget(italicButton);
    layout->addWidget(underlineButton);
    layout->addWidget(strikethroughButton);
    layout->addWidget(imageButton);

    // --- Centro: stretch reservado (título é posicionado manualmente) ---
    layout->addStretch(1);

    // --- Direita: Ferramentas ---
    layout->addWidget(glossaryButton);
    layout->addWidget(readModeButton);
    layout->addWidget(focusButton);
    layout->addWidget(searchButton);
    layout->addWidget(makeVSeparator(this));

    // --- Direita: Mídia ---
    layout->addWidget(reminderButton);
    layout->addWidget(immersiveSoundButton);
    layout->addWidget(makeVSeparator(this));

    // --- Direita: Ferramentas de worldbuilding ---
    layout->addWidget(construtorButton);
    layout->addWidget(pensarioButton);
    layout->addWidget(refMenuButton);
    layout->addWidget(makeVSeparator(this));

    // --- Direita: Sistema ---
    layout->addWidget(themePanelButton);
    layout->addWidget(settingsButton);
    layout->addWidget(fullscreenButton);

    buildSizeMenu();
    buildSpacingMenu();
    buildAlignMenu();
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
        QToolButton#ttbSize, QToolButton#ttbLineHeight {
            padding: 0px 1px;
            spacing: 1px;
            font-size: 11px;
        }
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

    if (reminderBadge) {
        reminderBadge->setStyleSheet(QStringLiteral(
            "QLabel#reminderBadge {"
            "  background: %1; border-radius: 4px;"
            "}").arg(Theme::accentDanger()));
    }

    if (pensarioBadge) {
        pensarioBadge->setStyleSheet(QStringLiteral(
            "QLabel#pensarioBadge {"
            "  background: %1; border-radius: 4px;"
            "}").arg(Theme::accentSuccess()));
    }

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

    if (docSubtitleLabel) {
        docSubtitleLabel->setStyleSheet(QStringLiteral(
            "QLabel#ttbDocSubtitle {"
            "  color: %1;"
            "  background: transparent;"
            "  font-family: 'Lora','Crimson Text',serif;"
            "  font-size: 12px;"
            "  font-weight: 500;"
            "}").arg(Theme::textMuted()));
    }

    if (sceneVarButton) {
        sceneVarButton->setStyleSheet(QStringLiteral(
            "QToolButton#ttbSceneVar {"
            "  background: transparent;"
            "  border: none;"
            "  border-radius: 4px;"
            "}"
            "QToolButton#ttbSceneVar:hover {"
            "  background: %1;"
            "}").arg(Theme::hoverOverlay()));
    }
}

void TopToolbar::reloadIcons()
{
    focusOffIcon = loadIcon(QStringLiteral("focusmode-off.svg"));
    focusOnIcon  = loadIcon(QStringLiteral("focusmode-on.svg"));
    if (focusButton) {
        focusButton->setIcon(focusCheckedCache ? focusOnIcon : focusOffIcon);
    }
    readModeOffIcon = loadIcon(QStringLiteral("focusededitor-off.svg"));
    readModeOnIcon  = loadIcon(QStringLiteral("focusededitor-on.svg"));
    if (readModeButton) {
        readModeButton->setIcon(readModeOn ? readModeOnIcon : readModeOffIcon);
    }
    for (const auto& pair : iconBindings) {
        if (pair.first) pair.first->setIcon(loadIcon(pair.second));
    }
    updateAlignButtonIcon();
}

// ---- Alinhamento ----

static QString alignIconName(Qt::Alignment a)
{
    if (a & Qt::AlignHCenter) return QStringLiteral("align-center.svg");
    if (a & Qt::AlignRight)   return QStringLiteral("align-right.svg");
    if (a & Qt::AlignJustify) return QStringLiteral("align-justify.svg");
    return QStringLiteral("align-left.svg");
}

void TopToolbar::updateAlignButtonIcon()
{
    if (alignButton) alignButton->setIcon(loadIcon(alignIconName(m_currentAlignment)));
}

void TopToolbar::setCurrentAlignment(Qt::Alignment alignment)
{
    m_currentAlignment = alignment;
    updateAlignButtonIcon();
    // Atualiza os botões dentro do popup.
    QToolButton* target = nullptr;
    if (alignment & Qt::AlignHCenter) target = m_alignBtnCenter;
    else if (alignment & Qt::AlignRight)   target = m_alignBtnRight;
    else if (alignment & Qt::AlignJustify) target = m_alignBtnJustify;
    else                                   target = m_alignBtnLeft;
    if (target) target->setChecked(true);
}

void TopToolbar::buildAlignMenu()
{
    // Popup customizado com botões de alinhamento + seletor de escopo.
    auto *menu = new QMenu(alignButton);
    menu->setObjectName(QStringLiteral("ttbAlignMenu"));

    auto *container = new QWidget;
    auto *vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(10, 8, 10, 10);
    vlay->setSpacing(6);

    // ---- Botões L/C/R/J ----
    auto *alignRow = new QHBoxLayout;
    alignRow->setSpacing(4);

    struct AlignOpt { Qt::Alignment align; QString icon; QString tip; };
    const QList<AlignOpt> opts = {
        { Qt::AlignLeft,    QStringLiteral("align-left.svg"),    tr("Esquerda")    },
        { Qt::AlignHCenter, QStringLiteral("align-center.svg"),  tr("Centro")      },
        { Qt::AlignRight,   QStringLiteral("align-right.svg"),   tr("Direita")     },
        { Qt::AlignJustify, QStringLiteral("align-justify.svg"), tr("Justificado") },
    };

    auto *alignGroup = new QButtonGroup(container);
    alignGroup->setExclusive(true);

    for (const AlignOpt& opt : opts) {
        auto *btn = new QToolButton(container);
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setAutoRaise(true);
        btn->setCheckable(true);
        btn->setFixedSize(28, 28);
        btn->setIconSize(QSize(16, 16));
        btn->setIcon(loadIcon(opt.icon));
        btn->setToolTip(opt.tip);
        btn->setCursor(Qt::PointingHandCursor);
        if (opt.align == m_currentAlignment) btn->setChecked(true);
        alignGroup->addButton(btn);
        alignRow->addWidget(btn);
        // Guarda referência para atualizar estado quando o doc muda.
        if (opt.align == Qt::AlignLeft)    m_alignBtnLeft    = btn;
        else if (opt.align == Qt::AlignHCenter) m_alignBtnCenter  = btn;
        else if (opt.align == Qt::AlignRight)   m_alignBtnRight   = btn;
        else if (opt.align == Qt::AlignJustify) m_alignBtnJustify = btn;

        const Qt::Alignment a = opt.align;
        connect(btn, &QToolButton::clicked, this, [this, menu, a]() {
            m_currentAlignment = a;
            updateAlignButtonIcon();
            emit alignmentRequested(a, m_alignScope);
            menu->close();
        });
    }
    alignRow->addStretch();
    vlay->addLayout(alignRow);

    // ---- Separador ----
    auto *sep = new QFrame(container);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Plain);
    vlay->addWidget(sep);

    // ---- Aplicar em ----
    auto *scopeLabel = new QLabel(tr("Aplicar em:"), container);
    scopeLabel->setObjectName(QStringLiteral("ttbAlignScopeLabel"));
    vlay->addWidget(scopeLabel);

    struct ScopeOpt { AlignScope scope; QString label; };
    const QList<ScopeOpt> scopes = {
        { AlignScope::ThisDoc,   tr("Esse documento")       },
        { AlignScope::AllDocs,   tr("Todos os documentos")  },
        { AlignScope::Manuscript,tr("Somente manuscrito")   },
        { AlignScope::Drawers,   tr("Somente gavetas")      },
    };

    auto *scopeGroup = new QButtonGroup(container);
    scopeGroup->setExclusive(true);
    for (const ScopeOpt& so : scopes) {
        auto *rb = new QRadioButton(so.label, container);
        rb->setObjectName(QStringLiteral("ttbAlignScope"));
        rb->setChecked(so.scope == m_alignScope);
        scopeGroup->addButton(rb);
        vlay->addWidget(rb);

        const AlignScope sc = so.scope;
        connect(rb, &QRadioButton::toggled, this, [this, sc](bool on) {
            if (on) m_alignScope = sc;
        });
    }

    auto *wa = new QWidgetAction(menu);
    wa->setDefaultWidget(container);
    menu->addAction(wa);

    menu->setStyleSheet(QStringLiteral(R"(
        QMenu#ttbAlignMenu { background: %1; border: 1px solid %2; border-radius: 8px; padding: 2px; }
        QToolButton { background: transparent; border: 1px solid transparent; border-radius: 4px; color: %3; }
        QToolButton:hover { background: %4; border-color: %5; }
        QToolButton:checked { background: %5; border-color: %5; }
        QLabel#ttbAlignScopeLabel { color: %6; font-size: 11px; font-weight: bold; margin-top: 2px; }
        QRadioButton#ttbAlignScope { color: %3; font-size: 12px; spacing: 8px; }
        QRadioButton#ttbAlignScope::indicator {
            width: 12px; height: 12px;
            border: 1px solid %2; border-radius: 6px; background: %7;
        }
        QRadioButton#ttbAlignScope::indicator:checked { background: %5; border-color: %5; }
        QFrame { color: %2; }
    )").arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(),
            Theme::hoverOverlay(), Theme::accentDefault(),
            Theme::textMuted(), Theme::inputBackground()));

    alignButton->setMenu(menu);
}

void TopToolbar::applyTheme()
{
    applyRootStyle();
    reloadIcons();
}

void TopToolbar::setDocumentTitle(const QString &title, const QString &subtitle)
{
    if (!docTitleLabel) return;
    docTitleLabel->setText(title);
    docTitleLabel->adjustSize();
    docTitleLabel->raise();

    if (docSubtitleLabel) {
        docSubtitleLabel->setText(subtitle);
        docSubtitleLabel->setVisible(!subtitle.isEmpty());
        docSubtitleLabel->adjustSize();
        docSubtitleLabel->raise();
    }

    positionDocTitle();
}

void TopToolbar::setSceneVarButtonVisible(bool visible)
{
    if (!sceneVarButton) return;
    sceneVarButton->setVisible(visible);
    sceneVarButton->raise();
    positionDocTitle();
}

QRect TopToolbar::sceneVarButtonGlobalRect() const
{
    if (!sceneVarButton) return QRect();
    const QPoint topLeft = sceneVarButton->mapToGlobal(QPoint(0, 0));
    return QRect(topLeft, sceneVarButton->size());
}

void TopToolbar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    positionDocTitle();
    positionReminderBadge();
    positionPensarioBadge();
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

QRect TopToolbar::reminderButtonGlobalRect() const
{
    if (!reminderButton) return QRect();
    const QPoint topLeft = reminderButton->mapToGlobal(QPoint(0, 0));
    return QRect(topLeft, reminderButton->size());
}

QRect TopToolbar::helpButtonGlobalRect() const
{
    if (!helpButton) return QRect();
    const QPoint topLeft = helpButton->mapToGlobal(QPoint(0, 0));
    return QRect(topLeft, helpButton->size());
}

void TopToolbar::setReminderBadge(bool active)
{
    if (reminderBadge) {
        reminderBadge->setVisible(active);
        positionReminderBadge();
    }
}

void TopToolbar::positionReminderBadge()
{
    if (!reminderBadge || !reminderButton || !reminderBadge->isVisible()) return;
    const QRect br = reminderButton->geometry();
    reminderBadge->move(br.right() - 10, br.top() + 2);
    reminderBadge->raise();
}

void TopToolbar::positionPensarioBadge()
{
    if (!pensarioBadge || !pensarioButton || !pensarioBadge->isVisible()) return;
    const QRect br = pensarioButton->geometry();
    pensarioBadge->move(br.right() - 10, br.top() + 2);
    pensarioBadge->raise();
}

// Pisca o badge do Pensário por alguns segundos e some sozinho — feedback de
// "salvou, e é aqui que fica", não um indicador persistente de não-lido.
void TopToolbar::pulsePensarioBadge()
{
    if (!pensarioBadge || !pensarioButton) return;

    pensarioBadge->setVisible(true);
    positionPensarioBadge();

    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(pensarioBadge->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(pensarioBadge);
        pensarioBadge->setGraphicsEffect(effect);
    }
    effect->setOpacity(1.0);

    auto* anim = new QPropertyAnimation(effect, "opacity", this);
    anim->setDuration(600);
    anim->setStartValue(1.0);
    anim->setKeyValueAt(0.5, 0.25);
    anim->setEndValue(1.0);
    anim->setLoopCount(4);
    anim->setEasingCurve(QEasingCurve::InOutQuad);
    connect(anim, &QPropertyAnimation::finished, this, [this]() {
        if (pensarioBadge) pensarioBadge->setVisible(false);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void TopToolbar::positionDocTitle()
{
    if (!docTitleLabel) return;
    const int centerX = (titleAnchorX >= 0) ? titleAnchorX : (width() / 2);
    const bool hasSubtitle = docSubtitleLabel && docSubtitleLabel->isVisible();

    if (!hasSubtitle) {
        const int x = centerX - docTitleLabel->width() / 2;
        const int y = (height() - docTitleLabel->height()) / 2;
        docTitleLabel->move(x, y);
        return;
    }

    // Título maior em cima, subtítulo menor embaixo — bloco de duas linhas
    // centralizado verticalmente na toolbar (ex.: capítulo + "Cena x").
    const int blockH = docTitleLabel->height() + docSubtitleLabel->height();
    const int topY = (height() - blockH) / 2;
    docTitleLabel->move(centerX - docTitleLabel->width() / 2, topY);

    // Botão de variações (quando visível) fica colado à direita do
    // subtítulo — a linha "subtítulo + botão" centraliza como um bloco só,
    // sem deslocar a linha do título de cima.
    const bool showVarBtn = sceneVarButton && sceneVarButton->isVisible();
    constexpr int kVarBtnGap = 4;
    const int subW = docSubtitleLabel->width();
    const int comboW = subW + (showVarBtn ? kVarBtnGap + sceneVarButton->width() : 0);
    const int subX = centerX - comboW / 2;
    const int subY = topY + docTitleLabel->height();
    docSubtitleLabel->move(subX, subY);
    if (showVarBtn) {
        const int subCenterY = subY + docSubtitleLabel->height() / 2;
        sceneVarButton->move(subX + subW + kVarBtnGap, subCenterY - sceneVarButton->height() / 2);
    }
}

void TopToolbar::setFontFamilies(const QStringList &families, const QString &current)
{
    fontFamilies = families;
    currentFontFamily = current;
    fontButton->setText(currentFontFamily);
    applyFontButtonStyle();
}

void TopToolbar::setFontSize(qreal pt)
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

void TopToolbar::setFullscreenChecked(bool checked)
{
    if (!fullscreenButton) return;
    QSignalBlocker block(fullscreenButton);
    fullscreenButton->setChecked(checked);
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

    sizeStepperEdit = new QLineEdit(stepperWidget);
    sizeStepperEdit->setObjectName(QStringLiteral("ttbSizeValue"));
    sizeStepperEdit->setText(sizeText(currentFontSize));
    sizeStepperEdit->setAlignment(Qt::AlignCenter);
    sizeStepperEdit->setFixedWidth(48);
    sizeStepperEdit->setFrame(false);
    sizeStepperEdit->setToolTip(tr("Digite o tamanho (10–48, aceita 0,5)"));
    // Aceita 10–48, com no máximo uma casa decimal (meios-pontos).
    auto *sizeValidator = new QDoubleValidator(10.0, 48.0, 1, sizeStepperEdit);
    sizeValidator->setNotation(QDoubleValidator::StandardNotation);
    sizeValidator->setLocale(QLocale::c());
    sizeStepperEdit->setValidator(sizeValidator);
    connect(sizeStepperEdit, &QLineEdit::returnPressed, this, &TopToolbar::commitSizeEditor);
    connect(sizeStepperEdit, &QLineEdit::editingFinished, this, &TopToolbar::commitSizeEditor);

    auto *plus = new QToolButton(stepperWidget);
    plus->setObjectName(QStringLiteral("ttbSizeStep"));
    plus->setText(QStringLiteral("+"));
    plus->setFixedSize(28, 28);
    plus->setCursor(Qt::PointingHandCursor);
    plus->setAutoRaise(true);

    stepperLayout->addWidget(minus);
    stepperLayout->addWidget(sizeStepperEdit);
    stepperLayout->addWidget(plus);

    connect(minus, &QToolButton::clicked, this, [this]() { applySize(currentFontSize - 0.5); });
    connect(plus, &QToolButton::clicked, this, [this]() { applySize(currentFontSize + 0.5); });

    auto *stepperAction = new QWidgetAction(menu);
    stepperAction->setDefaultWidget(stepperWidget);
    menu->addAction(stepperAction);

    menu->addSeparator();

    sizePresetActions.clear();
    const QList<int> presets = {12, 14, 16, 18, 20, 24, 28};
    for (int s : presets) {
        QAction *a = menu->addAction(QString::number(s) + tr(" pt"));
        a->setCheckable(true);
        a->setChecked(qFuzzyCompare(currentFontSize, qreal(s)));
        a->setData(s);
        connect(a, &QAction::triggered, this, [this, s]() { applySize(s); });
        sizePresetActions.append(a);
    }

    sizeButton->setMenu(menu);
}

void TopToolbar::commitSizeEditor()
{
    if (!sizeStepperEdit) return;
    const QString raw = sizeStepperEdit->text().trimmed().replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const qreal value = raw.toDouble(&ok);
    if (ok) applySize(value);
    // Normaliza o texto exibido (ex.: "14,5" → "14.5"; vazio → valor atual).
    sizeStepperEdit->setText(sizeText(currentFontSize));
}

void TopToolbar::updateSizeMenuState()
{
    sizeButton->setText(sizeText(currentFontSize));
    applyFontButtonStyle();
    if (sizeStepperEdit) {
        QSignalBlocker block(sizeStepperEdit);
        sizeStepperEdit->setText(sizeText(currentFontSize));
    }
    for (QAction *a : std::as_const(sizePresetActions)) {
        a->setChecked(qFuzzyCompare(currentFontSize, qreal(a->data().toInt())));
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

void TopToolbar::applySize(qreal pt)
{
    const qreal minSize = 10.0;
    const qreal maxSize = 48.0;
    // Encaixa em múltiplos de 0.5 — granularidade de meio-ponto.
    qreal next = qBound(minSize, qRound(pt * 2.0) / 2.0, maxSize);
    if (qFuzzyCompare(next, currentFontSize)) return;
    currentFontSize = next;
    updateSizeMenuState();
    emit fontSizeChanged(next);
}
