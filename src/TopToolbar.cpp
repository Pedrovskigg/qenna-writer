#include "TopToolbar.h"

#include "MiraPersonality.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QDoubleValidator>
#include <QEasingCurve>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPair>
#include <QPropertyAnimation>
#include <QRadioButton>
#include <QSet>
#include <QSettings>
#include <QScopeGuard>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include "FontPickerPopup.h"
#include "IconUtils.h"
#include "Theme.h"
#include "ToolbarGroupWidget.h"
#include "UiScale.h"

namespace {

// Cores dos ícones vêm do tema; reavaliadas em cada loadIcon, pra que reload
// após troca de tema reflita as cores atuais (e nunca fiquem invisíveis).
QString iconNormalColor()   { return Theme::textMuted(); }
QString iconHoverColor()    { return Theme::textPrimary(); }
QString iconSelectedColor() { return Theme::textBright(); }

// Igual à LeftBar (LeftBar.cpp: kBarWidthBase=60, floor=40) — antes eram
// 48/28, então LeftBar e TopToolbar cresciam/encolhiam em proporções
// diferentes ao mexer no slider de escala de UI, ficando visivelmente
// desencontradas (uma sempre maior que a outra). Ver TopToolbar::applyUiScale.
constexpr int kBarHeight = 60;
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

QIcon loadIcon(const QString &name, int px = kIconSize)
{
    return IconUtils::loadToolbarIcon(
        QStringLiteral(":/icons/%1").arg(name),
        QColor(iconNormalColor()),
        QColor(iconHoverColor()),
        QColor(iconSelectedColor()),
        QSize(px, px));
}

// vertical=true monta o separador pra uma barra vertical (linha horizontal,
// "HLine") — o nome do objeto (ttbVSep) fica genérico de propósito, é só
// estilizado por QSS uma vez só.
QFrame *makeVSeparator(QWidget *parent, bool vertical = false)
{
    auto *line = new QFrame(parent);
    line->setObjectName(QStringLiteral("ttbVSep"));
    line->setFrameShape(vertical ? QFrame::HLine : QFrame::VLine);
    line->setFrameShadow(QFrame::Plain);
    line->setFixedSize(vertical ? QSize(22, 1) : QSize(1, 22));
    return line;
}

int fontButtonPointSize(qreal editorPt)
{
    return qBound(10, qRound(editorPt * 2.0 / 3.0), 18);
}

}

int TopToolbar::thickness() const
{
    // Não usa a constante kBarHeight direto — a espessura real muda com a
    // escala de UI (ver applyUiScale()); ler a dimensão fixada de verdade
    // evita as duas ficarem dessincronizadas.
    return isVertical() ? width() : height();
}

// "14" para inteiros, "14.5" para meios-pontos. Sem ".0" pendurado.
QString TopToolbar::sizeText(qreal pt)
{
    if (qFuzzyCompare(pt, qRound(pt)))
        return QString::number(qRound(pt));
    return QString::number(pt, 'f', 1);
}

TopToolbar::TopToolbar(QWidget *parent, Qt::Edge side)
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
    , statisticsButton(makeIconButton(this))
    , readAloudButton(makeIconButton(this))
    , repetitionsButton(makeIconButton(this))
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
    , miraButton(makeIconButton(this))
    , fontPicker(nullptr)
    , sizeStepperEdit(nullptr)
    , currentFontFamily(QStringLiteral("Alegreya"))
    , currentFontSize(16)
    , currentLineHeightPercent(115)
    , m_barSide(side)
{
    setObjectName(QStringLiteral("topToolbar"));
    setAttribute(Qt::WA_StyledBackground, true);
    if (isVertical()) setFixedWidth(kBarHeight);
    else setFixedHeight(kBarHeight);

    reminderBadge   = makeBadge(QStringLiteral("reminderBadge"));
    pensarioBadge   = makeBadge(QStringLiteral("pensarioBadge"));
    readModeBadge   = makeBadge(QStringLiteral("modeBadge"));
    focusModeBadge  = makeBadge(QStringLiteral("modeBadge"));
    indentBadge     = makeBadge(QStringLiteral("modeBadge"));

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
    homeButton->setToolTip(tr("Voltar ao menu principal (F12)"));
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
    statisticsButton->setObjectName(QStringLiteral("ttbTool"));
    bindIcon(statisticsButton, QStringLiteral("stats-chart.svg"));
    statisticsButton->setToolTip(tr("Estatísticas"));
    connect(statisticsButton, &QToolButton::clicked, this, &TopToolbar::statisticsRequested);

    readAloudButton->setObjectName(QStringLiteral("ttbTool"));
    bindIcon(readAloudButton, QStringLiteral("read-aloud.svg"));
    readAloudButton->setToolTip(tr("Ler em voz alta a seleção"));
    connect(readAloudButton, &QToolButton::clicked, this, &TopToolbar::readAloudRequested);

    repetitionsButton->setObjectName(QStringLiteral("ttbTool"));
    bindIcon(repetitionsButton, QStringLiteral("repetitions.svg"));
    repetitionsButton->setToolTip(tr("Grifar repetições"));
    repetitionsButton->setCheckable(true);
    connect(repetitionsButton, &QToolButton::toggled, this, &TopToolbar::repetitionsToggled);

    // Editor focado (distraction-free). Checkable de proposito: o destaque de
    // "ligado" na barra e o unico indicativo de que o modo esta ativo, ja que
    // ele esconde justamente a UI que mostraria isso. Mesmo tratamento do
    // Modo Foco.
    readModeButton->setObjectName(QStringLiteral("ttbMode"));
    readModeButton->setIcon(readModeOffIcon);
    // Nome no text() e descricao no toolTip(): o botao e icone-only, entao o
    // text() nao e desenhado — ele serve pro rotulo no menu "..." de overflow
    // (ver buildOverflowMenu), onde a descricao inteira nao caberia.
    readModeButton->setText(tr("Editor Focado (Modo clássico)"));
    readModeButton->setToolTip(tr("Editor Focado (Modo clássico)\n"
                                  "Modo editor focado com barras que recuam. Igual na "
                                  "primeira versão clássica do app (Mira Writing)."));
    readModeButton->setCheckable(true);
    connect(readModeButton, &QToolButton::toggled, this, [this](bool on) {
        readModeOn = on;
        readModeButton->setIcon(readModeOn ? readModeOnIcon : readModeOffIcon);
        positionModeBadges();
        emit readModeToggled(readModeOn);
    });

    focusButton->setObjectName(QStringLiteral("ttbMode"));
    focusButton->setIcon(focusOffIcon);
    focusButton->setCheckable(true);
    focusButton->setToolTip(tr("Modo foco (Ctrl+F10)"));
    connect(focusButton, &QToolButton::toggled, this, [this](bool on) {
        focusCheckedCache = on;
        focusButton->setIcon(on ? focusOnIcon : focusOffIcon);
        positionModeBadges();
        emit focusModeToggled(on);
    });

    searchButton->setObjectName(QStringLiteral("ttbTool"));
    bindIcon(searchButton, QStringLiteral("search.svg"));
    searchButton->setToolTip(tr("Buscar (Ctrl+Shift+F)"));
    connect(searchButton, &QToolButton::clicked, this, &TopToolbar::searchRequested);

    // ---------------- Grupo D: Tipografia ----------------
    fontButton->setObjectName(QStringLiteral("ttbFont"));
    sizeButton->setObjectName(QStringLiteral("ttbSize"));
    lineHeightButton->setObjectName(QStringLiteral("ttbLineHeight"));

    // O ícone é vinculado SEMPRE, mesmo no modo horizontal onde o botão mostra
    // texto: sem isso, trocar pra lateral ao vivo acharia o binding inexistente
    // e o botão nasceria sem ícone.
    bindIcon(fontButton, QStringLiteral("change-font.svg"));
    bindIcon(sizeButton, QStringLiteral("font-size.svg"));
    bindIcon(lineHeightButton, QStringLiteral("text-spacing.svg"));

    sizeButton->setPopupMode(QToolButton::InstantPopup);
    lineHeightButton->setPopupMode(QToolButton::InstantPopup);

    applyTypographyButtonMode();

    fontPicker = new FontPickerPopup(this);
    connect(fontPicker, &FontPickerPopup::fontSelected, this, [this](const QString &family) {
        currentFontFamily = family;
        if (isVertical()) fontButton->setToolTip(family);
        else { fontButton->setText(family); applyFontButtonStyle(); }
        emit fontFamilyChanged(family);
    });
    connect(fontButton, &QToolButton::clicked, this, [this]() {
        if (!fontPicker) return;
        fontPicker->setFontFamilies(fontFamilies, currentFontFamily);
        const QRect anchor(fontButton->mapToGlobal(QPoint(0, 0)), fontButton->size());
        fontPicker->showNear(anchor, m_barSide);
    });

    indentButton->setObjectName(QStringLiteral("ttbIndent"));
    indentButton->setText(QStringLiteral("¶"));
    // Único botão do grupo Editor sem tamanho fixo/ícone de verdade — ficava
    // do tamanho natural do glifo "¶", inconsistente com os vizinhos
    // quadrados (mais visível agora que os grupos têm política de tamanho
    // rígida). Adicionado a m_squareButtons abaixo pra escalar igual aos outros.
    indentButton->setFixedSize(kIconButtonSize, kIconButtonSize);
    indentButton->setCheckable(true);
    indentButton->setChecked(true);
    indentButton->setToolTip(tr("Identação de parágrafo"));
    connect(indentButton, &QToolButton::toggled, this, [this](bool on) {
        positionModeBadges();
        emit firstLineIndentToggled(on);
    });

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
    reminderButton->setToolTip(tr("Lembretes (F7)"));
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
    fullscreenButton->setToolTip(tr("Tela cheia (F11)"));
    connect(fullscreenButton, &QToolButton::toggled, this, &TopToolbar::fullscreenToggled);

    refMenuButton->setObjectName(QStringLiteral("ttbSystem"));
    bindIcon(refMenuButton, QStringLiteral("refmenu.svg"));
    refMenuButton->setToolTip(tr("Painel de Referência (F6)"));
    connect(refMenuButton, &QToolButton::clicked, this, &TopToolbar::refMenuToggleRequested);

    pensarioButton->setObjectName(QStringLiteral("ttbSystem"));
    bindIcon(pensarioButton, QStringLiteral("pensario.svg"));
    pensarioButton->setToolTip(tr("Pensário (F4)"));
    connect(pensarioButton, &QToolButton::clicked, this, &TopToolbar::pensarioToggleRequested);

    construtorButton->setObjectName(QStringLiteral("ttbSystem"));
    bindIcon(construtorButton, QStringLiteral("construtor.svg"));
    construtorButton->setToolTip(tr("Construtor"));
    connect(construtorButton, &QToolButton::clicked, this, &TopToolbar::construtorToggleRequested);

    miraButton->setObjectName(QStringLiteral("ttbSystem"));
    bindIcon(miraButton, QStringLiteral("elements/star.svg"));
    miraButton->setToolTip(tr("%1 — chat com a assistente de IA (F9)").arg(miraAssistantName()));
    connect(miraButton, &QToolButton::clicked, this, &TopToolbar::miraToggleRequested);

    // ---------------- Layout ----------------
    // Início (esquerda quando horizontal / topo quando vertical): Projeto
    // (new/open/save) + Editor (font/size/lineHeight/indent/B/I)
    // Meio: título do documento (só horizontal — ver positionDocTitle)
    // Fim: Ferramentas + Mídia + Worldbuilding (Construtor/Pensário/RefMenu) + Sistema
    //
    // Cada bloco acima virou um ToolbarGroupWidget arrastável (ver
    // buildGroups()) — a ORDEM entre eles é o que fica configurável (modo de
    // edição, ligado por duplo clique na barra). m_mainLayout é membro (não variável
    // local) porque rebuildGroupLayout() precisa reconstruí-lo sempre que a
    // ordem muda, não só uma vez na construção.
    const bool vertical = isVertical();
    m_mainLayout = new QBoxLayout(vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight, this);
    m_mainLayout->setContentsMargins(vertical ? 4 : 14, vertical ? 14 : 4, vertical ? 4 : 14, vertical ? 14 : 4);
    m_mainLayout->setSpacing(6);

    buildGroups();

    // Botão de overflow ("⋯") — só aparece quando updateOverflow() precisa
    // esconder algum botão dispensável por falta de espaço. Fica no fim da
    // barra, sempre com o menu vazio até que isso aconteça. Fora do sistema
    // de grupos (não arrasta, sempre por último).
    overflowButton = new QToolButton(this);
    overflowButton->setObjectName(QStringLiteral("ttbSystem"));
    overflowButton->setAutoRaise(true);
    overflowButton->setCursor(Qt::PointingHandCursor);
    overflowButton->setFixedSize(kIconButtonSize, kIconButtonSize);
    overflowButton->setText(QStringLiteral("⋯"));
    overflowButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    overflowButton->setToolTip(tr("Mais opções"));
    overflowButton->setPopupMode(QToolButton::InstantPopup);
    overflowButton->setVisible(false);
    m_overflowMenu = new QMenu(overflowButton);
    overflowButton->setMenu(m_overflowMenu);

    m_groupOrder = loadGroupOrder();
    m_groupButtons = loadButtonLayout();
    rebuildGroupLayout();

    buildSizeMenu();
    buildSpacingMenu();
    buildAlignMenu();
    buildOverflowMenu();
    applyRootStyle();
    for (ToolbarGroupWidget* g : std::as_const(m_groupWidgets)) {
        if (g) g->applyTheme(QColor(Theme::textMuted()), QColor(Theme::accentDefault()));
    }

    // Botões "quadrados" padrão (tamanho kIconButtonSize / ícone kIconSize) —
    // os únicos com dimensão especial (sizeButton, lineHeightButton)
    // ficam de fora e são tratados à parte em applyUiScale().
    m_squareButtons = {
        homeButton, newProjectButton, openProjectButton, saveProjectButton,
        exportButton, helpButton, boldButton, italicButton, underlineButton,
        strikethroughButton, statisticsButton, readAloudButton, repetitionsButton,
        readModeButton, focusButton,
        searchButton, alignButton, imageButton, reminderButton,
        immersiveSoundButton, themePanelButton, settingsButton, fullscreenButton,
        refMenuButton, pensarioButton, construtorButton, miraButton,
        overflowButton, indentButton,
    };

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &TopToolbar::applyTheme);
    connect(UiScale::Manager::instance(), &UiScale::Manager::scaleChanged,
            this, &TopToolbar::applyUiScale);
    applyUiScale(); // aplica a escala persistida (icones recarregados no tamanho certo)
}

void TopToolbar::buildGroups()
{
    const Qt::Orientation orient = isVertical() ? Qt::Vertical : Qt::Horizontal;
    for (const QString& id : defaultGroupOrder()) {
        auto* g = new ToolbarGroupWidget(id, orient, this);
        connect(g, &ToolbarGroupWidget::groupDroppedOn, this, &TopToolbar::onGroupDropped);
        connect(g, &ToolbarGroupWidget::buttonDroppedOn, this, &TopToolbar::onButtonDropped);
        connect(g, &ToolbarGroupWidget::toggleEditModeRequested,
                this, [this]() { setEditMode(!m_editMode); });
        m_groupWidgets.insert(id, g);
    }

    // Ids ESTAVEIS: e isso que vai pro QSettings. Renomear um id aqui invalida
    // a organizacao salva do usuario (cai no padrao, ver loadButtonLayout) —
    // entao trate como formato de arquivo, nao como detalhe interno.
    m_buttonsById = {
        { QStringLiteral("home"),           homeButton },
        { QStringLiteral("newProject"),     newProjectButton },
        { QStringLiteral("openProject"),    openProjectButton },
        { QStringLiteral("saveProject"),    saveProjectButton },
        { QStringLiteral("export"),         exportButton },
        { QStringLiteral("help"),           helpButton },
        { QStringLiteral("font"),           fontButton },
        { QStringLiteral("size"),           sizeButton },
        { QStringLiteral("lineHeight"),     lineHeightButton },
        { QStringLiteral("indent"),         indentButton },
        { QStringLiteral("align"),          alignButton },
        { QStringLiteral("bold"),           boldButton },
        { QStringLiteral("italic"),         italicButton },
        { QStringLiteral("underline"),      underlineButton },
        { QStringLiteral("strikethrough"),  strikethroughButton },
        { QStringLiteral("image"),          imageButton },
        { QStringLiteral("readMode"),       readModeButton },
        { QStringLiteral("focus"),          focusButton },
        { QStringLiteral("search"),         searchButton },
        { QStringLiteral("reminder"),       reminderButton },
        { QStringLiteral("immersiveSound"), immersiveSoundButton },
        { QStringLiteral("construtor"),     construtorButton },
        { QStringLiteral("pensario"),       pensarioButton },
        { QStringLiteral("refMenu"),        refMenuButton },
        { QStringLiteral("statistics"),     statisticsButton },
        { QStringLiteral("readAloud"),      readAloudButton },
        { QStringLiteral("repetitions"),    repetitionsButton },
        { QStringLiteral("mira"),           miraButton },
        { QStringLiteral("themePanel"),     themePanelButton },
        { QStringLiteral("settings"),       settingsButton },
        { QStringLiteral("fullscreen"),     fullscreenButton },
    };
}

QHash<QString, QStringList> TopToolbar::defaultButtonLayout() const
{
    return {
        { QStringLiteral("project"), { QStringLiteral("home"), QStringLiteral("newProject"),
                                       QStringLiteral("openProject"), QStringLiteral("saveProject"),
                                       QStringLiteral("export"), QStringLiteral("help") } },
        { QStringLiteral("editor"),  { QStringLiteral("font"), QStringLiteral("size"),
                                       QStringLiteral("lineHeight"), QStringLiteral("indent"),
                                       QStringLiteral("align"), QStringLiteral("bold"),
                                       QStringLiteral("italic"), QStringLiteral("underline"),
                                       QStringLiteral("strikethrough"), QStringLiteral("image") } },
        { QStringLiteral("tools"),   { QStringLiteral("readMode"), QStringLiteral("focus"),
                                       QStringLiteral("search"), QStringLiteral("readAloud"),
                                       QStringLiteral("repetitions") } },
        { QStringLiteral("media"),   { QStringLiteral("reminder"), QStringLiteral("immersiveSound") } },
        { QStringLiteral("worldbuilding"), { QStringLiteral("construtor"), QStringLiteral("pensario"),
                                             QStringLiteral("refMenu"), QStringLiteral("statistics"),
                                             QStringLiteral("mira") } },
        { QStringLiteral("system"),  { QStringLiteral("themePanel"), QStringLiteral("settings"),
                                       QStringLiteral("fullscreen") } },
    };
}

QStringList TopToolbar::defaultGroupOrder() const
{
    return { QStringLiteral("project"), QStringLiteral("editor"), QStringLiteral("tools"),
             QStringLiteral("media"), QStringLiteral("worldbuilding"), QStringLiteral("system") };
}

QStringList TopToolbar::loadGroupOrder() const
{
    const QStringList def = defaultGroupOrder();
    const QStringList saved = QSettings().value(QStringLiteral("ui/topToolbarGroupOrder")).toStringList();
    // Settings corrompida, de versao antiga sem esse valor, ou de uma versao
    // futura com grupos diferentes — ignora e volta pro padrao em vez de
    // arriscar desenhar a barra incompleta ou travar em algum lugar.
    const QSet<QString> savedSet(saved.begin(), saved.end());
    const QSet<QString> defSet(def.begin(), def.end());
    if (saved.size() == def.size() && savedSet == defSet) return saved;
    return def;
}

void TopToolbar::saveGroupOrder() const
{
    QSettings().setValue(QStringLiteral("ui/topToolbarGroupOrder"), m_groupOrder);
}

// Serializado como uma QStringList de "grupo=id1,id2,id3" — um valor so no
// registro, e legivel a olho nu se precisar depurar.
void TopToolbar::saveButtonLayout() const
{
    QStringList out;
    for (const QString& gid : m_groupOrder)
        out << QStringLiteral("%1=%2").arg(gid, m_groupButtons.value(gid).join(QLatin1Char(',')));
    QSettings().setValue(QStringLiteral("ui/topToolbarButtonLayout"), out);
}

QHash<QString, QStringList> TopToolbar::loadButtonLayout() const
{
    const QHash<QString, QStringList> def = defaultButtonLayout();
    const QStringList saved = QSettings().value(QStringLiteral("ui/topToolbarButtonLayout")).toStringList();
    if (saved.isEmpty()) return def;

    QHash<QString, QStringList> parsed;
    QSet<QString> seen;
    for (const QString& entry : saved) {
        const int eq = entry.indexOf(QLatin1Char('='));
        if (eq <= 0) return def; // formato estranho: nao tenta adivinhar
        const QString gid = entry.left(eq);
        if (!m_groupWidgets.contains(gid) || parsed.contains(gid)) return def;
        QStringList ids;
        const QString rhs = entry.mid(eq + 1);
        if (!rhs.isEmpty()) ids = rhs.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString& id : std::as_const(ids)) {
            // Id desconhecido (versao antiga/futura) ou repetido em dois grupos
            // deixaria um botao orfao ou duplicado na barra — melhor recomecar.
            if (!m_buttonsById.contains(id) || seen.contains(id)) return def;
            seen.insert(id);
        }
        parsed.insert(gid, ids);
    }
    // Todo botao conhecido precisa estar em exatamente um grupo. Quando uma
    // versao nova do app ganha um botao, o layout salvo nao o conhece —
    // antes isso jogava a organizacao inteira fora e voltava pro padrao.
    // Perder a barra que o usuario arrumou a mao toda vez que nasce uma
    // feature e caro demais: agora o botao que falta e ENCAIXADO no grupo
    // onde o padrao o coloca, e o resto da organizacao fica de pe.
    if (seen.size() != m_buttonsById.size()) {
        for (auto it = def.constBegin(); it != def.constEnd(); ++it) {
            for (const QString& id : it.value()) {
                if (seen.contains(id) || !m_buttonsById.contains(id)) continue;
                parsed[it.key()].append(id);
                seen.insert(id);
            }
        }
        // Botao conhecido que nem o padrao posiciona (descuido de quem
        // adicionou): ai sim recomeca, senao ele ficaria orfao e invisivel.
        if (seen.size() != m_buttonsById.size()) return def;
    }
    for (const QString& gid : m_groupWidgets.keys())
        if (!parsed.contains(gid)) parsed.insert(gid, QStringList());
    return parsed;
}

void TopToolbar::rebuildGroupLayout()
{
    if (!m_mainLayout) return;

    QLayoutItem* item;
    while ((item = m_mainLayout->takeAt(0)) != nullptr) delete item; // nao deleta os widgets
    for (QFrame* sep : std::as_const(m_separators)) if (sep) sep->deleteLater();
    m_separators.clear();

    // Repovoa cada grupo a partir da composicao atual (que o usuario pode ter
    // mudado arrastando botoes), nao da divisao fixa de codigo.
    for (const QString& gid : std::as_const(m_groupOrder)) {
        ToolbarGroupWidget* g = m_groupWidgets.value(gid);
        if (!g) continue;
        g->clearButtons();
        for (const QString& bid : m_groupButtons.value(gid)) {
            if (QToolButton* b = m_buttonsById.value(bid)) g->addButton(b, bid);
        }
        g->refreshEmptyPlaceholder();
    }

    const bool vertical = isVertical();
    // Grupos vazios so ocupam lugar (e so recebem drop) durante o modo de
    // edicao; fora dele sao invisiveis, e um separador ao lado de nada fica
    // parecendo sujeira na barra.
    QStringList visible;
    for (const QString& gid : std::as_const(m_groupOrder)) {
        ToolbarGroupWidget* g = m_groupWidgets.value(gid);
        if (!g) continue;
        if (g->isEmpty() && !m_editMode) { g->hide(); continue; }
        g->show();
        visible << gid;
    }

    for (int i = 0; i < visible.size(); ++i) {
        ToolbarGroupWidget* g = m_groupWidgets.value(visible.at(i));
        m_mainLayout->addWidget(g);
        if (i == visible.size() - 1) continue;

        // Horizontal: so grupos e separadores, sem espaco elastico. A barra
        // agora tem a largura do CONTEUDO (ver sizePolicy abaixo e
        // MainWindow::layoutToolbarHolder), entao nao existe sobra pra
        // distribuir — o antigo stretch no meio era o vao reservado pro titulo
        // do documento, que mudou de casa (ver DocHeaderBar) e virou buraco.
        //
        // A vertical fica como estava (um stretch depois do 2o grupo, o resto
        // empilhado): la a barra ocupa a altura toda da janela de qualquer
        // jeito, e mudar isso seria mexer no que ninguem pediu.
        auto* sep = makeVSeparator(this, vertical);
        m_separators.append(sep);
        m_mainLayout->addWidget(sep);
        if (vertical && i == 1) m_mainLayout->addStretch(1);
    }
    if (overflowButton) m_mainLayout->addWidget(overflowButton);

    // Mesmo raciocinio de centralizacao de sempre, agora nos GRUPOS (que ja
    // centralizam seus proprios botoes por dentro, ver ToolbarGroupWidget)
    // em vez de botao por botao.
    const Qt::Alignment crossAxisAlign = vertical ? Qt::AlignHCenter : Qt::AlignVCenter;
    for (int i = 0; i < m_mainLayout->count(); ++i) {
        if (QWidget* w = m_mainLayout->itemAt(i)->widget())
            m_mainLayout->setAlignment(w, crossAxisAlign);
    }
    // Separadores nascem com o tamanho base; sem isto ficam do tamanho errado
    // quando a escala de UI nao e 100% (rebuild acontece depois do applyUiScale).
    const int sepLen = qMax(12, qRound(22 * UiScale::scale()));
    for (QFrame* sep : std::as_const(m_separators)) {
        if (vertical) sep->setFixedSize(sepLen, 1);
        else          sep->setFixedSize(1, sepLen);
    }
}

void TopToolbar::setEditMode(bool on)
{
    if (m_editMode == on) return;
    m_editMode = on;
    // Só enquanto edita: um filtro de aplicação permanente veria TODO evento do
    // app pra nada, e este aqui existe só pra perceber o clique que encerra.
    if (on) qApp->installEventFilter(this);
    else    qApp->removeEventFilter(this);
    for (ToolbarGroupWidget* g : std::as_const(m_groupWidgets)) {
        if (g) g->setDraggable(on);
    }
    // Um grupo que ficou sem nenhum botao so aparece durante a edicao — e e
    // justamente durante a edicao que ele precisa aparecer, senao nao ha onde
    // soltar um botao pra devolve-lo. Ver rebuildGroupLayout().
    rebuildGroupLayout();
}

void TopToolbar::onButtonDropped(const QString& buttonId, const QString& targetGroupId, int index)
{
    if (!m_buttonsById.contains(buttonId) || !m_groupButtons.contains(targetGroupId)) return;

    QString sourceGroupId;
    int sourceIndex = -1;
    for (auto it = m_groupButtons.constBegin(); it != m_groupButtons.constEnd(); ++it) {
        const int i = it.value().indexOf(buttonId);
        if (i >= 0) { sourceGroupId = it.key(); sourceIndex = i; break; }
    }
    if (sourceGroupId.isEmpty()) return;

    int target = qBound(0, index, m_groupButtons.value(targetGroupId).size());
    if (sourceGroupId == targetGroupId) {
        // Tirar o botao da lista desloca pra tras tudo que vem depois dele, e o
        // indice de insercao foi calculado ANTES dessa remocao.
        if (sourceIndex < target) --target;
        if (target == sourceIndex) return; // soltou onde ja estava
    }

    m_groupButtons[sourceGroupId].removeAt(sourceIndex);
    QStringList& targetList = m_groupButtons[targetGroupId];
    targetList.insert(qBound(0, target, targetList.size()), buttonId);

    saveButtonLayout();
    rebuildGroupLayout();
}

void TopToolbar::onGroupDropped(const QString& draggedId, const QString& targetId)
{
    if (draggedId == targetId) return;
    const int fromIdx = m_groupOrder.indexOf(draggedId);
    if (fromIdx < 0 || !m_groupOrder.contains(targetId)) return;
    m_groupOrder.removeAt(fromIdx);
    const int toIdx = m_groupOrder.indexOf(targetId); // recalculado após o remove acima
    m_groupOrder.insert(toIdx, draggedId);
    saveGroupOrder();
    saveButtonLayout(); // serializado na ordem dos grupos — precisa acompanhar
    rebuildGroupLayout();
}

void TopToolbar::buildOverflowMenu()
{
    // Ordem de colapso: índice 0 é o primeiro a sumir pra dentro de "⋯" quando
    // falta espaço; os últimos (miraButton, construtorButton...) só colapsam
    // em janelas realmente minúsculas. Escrita/formatação (grupo da esquerda)
    // e Projeto/Configurações nunca entram aqui — ficam sempre visíveis.
    m_collapsePriority = {
        immersiveSoundButton,
        fullscreenButton,
        themePanelButton,
        reminderButton,
        readModeButton,
        searchButton,
        focusButton,
        statisticsButton,
        refMenuButton,
        pensarioButton,
        construtorButton,
        miraButton,
    };

    for (QToolButton *btn : std::as_const(m_collapsePriority)) {
        if (!btn) continue;
        // text() quando existe: alguns botoes tem tooltip de varias linhas com
        // descricao, e a descricao inteira nao cabe num item de menu.
        const QString label = btn->text().isEmpty() ? btn->toolTip() : btn->text();
        auto *action = new QAction(btn->icon(), label, m_overflowMenu);
        action->setCheckable(btn->isCheckable());
        connect(action, &QAction::triggered, this, [btn]() { btn->click(); });
        m_overflowActions.insert(btn, action);
    }

    // Sincroniza o "check" dos itens colapsáveis checkable (focusButton,
    // fullscreenButton) toda vez que o menu abre — o QAction proxy não recebe
    // o toggled do botão real automaticamente.
    connect(m_overflowMenu, &QMenu::aboutToShow, this, [this]() {
        for (auto it = m_overflowActions.constBegin(); it != m_overflowActions.constEnd(); ++it) {
            if (it.value()->isCheckable()) it.value()->setChecked(it.key()->isChecked());
        }
    });
}

int TopToolbar::currentIconPx() const
{
    return qMax(10, qRound(kIconSize * UiScale::scale()));
}


void TopToolbar::applyUiScale()
{
    const qreal s = UiScale::scale();
    const int barH = qMax(40, qRound(kBarHeight * s));
    const int btnSize = qMax(18, qRound(kIconButtonSize * s));
    const int icoPx = currentIconPx();

    if (isVertical()) {
        setFixedWidth(barH);
    } else {
        setFixedHeight(barH);
        // Maximum na horizontal: a barra nunca passa da largura do proprio
        // conteudo (fica uma faixa centralizada no topo, em vez de esticar de
        // ponta a ponta e deixar um vazio enorme no meio), mas ainda PODE ser
        // espremida numa janela estreita — e e isso que faz o updateOverflow()
        // perceber que precisa colapsar botoes pro menu "...".
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    }

    // O polish do QStyleSheetStyle é PREGUIÇOSO (roda no primeiro layout) e é
    // ele quem traduz `min-width` do QSS em setMinimumWidth() no widget. Se
    // deixarmos acontecer sozinho, ele pousa DEPOIS dos setFixedSize() abaixo e
    // rouba o mínimo deles — era exatamente esse o bug da barra vertical (ver
    // verticalGeometryReset()). Forçar aqui inverte a ordem: o QSS aplica o que
    // tem pra aplicar primeiro, e o tamanho fixo abaixo é a última palavra.
    for (QToolButton *b : { fontButton, sizeButton, lineHeightButton, indentButton }) {
        if (b) b->ensurePolished();
    }

    for (QToolButton *b : std::as_const(m_squareButtons)) {
        if (!b) continue;
        b->setFixedSize(btnSize, btnSize);
        b->setIconSize(QSize(icoPx, icoPx));
    }
    // fontButton não está em m_squareButtons (no horizontal ele é um botão de
    // largura variável com o nome da fonte) — só precisa escalar como um
    // quadrado igual aos outros quando é ícone-only (vertical).
    if (fontButton && isVertical()) {
        fontButton->setFixedSize(btnSize, btnSize);
        fontButton->setIconSize(QSize(icoPx, icoPx));
    }
    if (sizeButton) {
        sizeButton->setIconSize(QSize(icoPx, icoPx));
        if (isVertical()) sizeButton->setFixedSize(btnSize, btnSize);
        else sizeButton->setFixedSize(qMax(18, qRound(26 * s)), btnSize);
    }
    if (lineHeightButton) {
        lineHeightButton->setIconSize(QSize(icoPx, icoPx));
        if (isVertical()) lineHeightButton->setFixedSize(btnSize, btnSize);
        else lineHeightButton->setFixedSize(qMax(18, qRound(26 * s)), btnSize);
    }
    const int sepLen = qMax(12, qRound(22 * s));
    for (QFrame *sep : std::as_const(m_separators)) {
        if (!sep) continue;
        if (isVertical()) sep->setFixedSize(sepLen, 1);
        else sep->setFixedSize(1, sepLen);
    }

    reloadIcons(); // re-renderiza os SVGs no tamanho novo (evita esticar bitmap)

    for (ToolbarGroupWidget *g : std::as_const(m_groupWidgets)) {
        if (g && g->layout()) g->layout()->activate();
    }
    if (layout()) layout()->activate();

    refreshLayoutAndOverflow();
}

// Fonte/tamanho/espaçamento são os únicos botões que mudam de NATUREZA com o
// lado da barra, e é por isso que trocar de lado já exigiu reiniciar o app:
// no topo o de fonte é um botão de TEXTO que mostra o nome da fonte escrito na
// própria fonte (pré-visualização), e o de tamanho mostra o valor ao lado do
// ícone; na lateral não cabe texto numa coluna de ~48px, então os três viram
// quadrados ícone-only e o valor vai pro tooltip.
//
// Tudo aqui é idempotente de propósito: roda no construtor e de novo a cada
// setBarSide().
// Troca o lado da barra AO VIVO. Antes isso exigia relançar o app inteiro, o
// que jogava o usuário de volta na tela inicial.
//
// A ordem aqui importa e não é arbitrária:
//   1. natureza dos botões (texto <-> ícone) ANTES de qualquer medida;
//   2. direção do layout e orientação dos grupos;
//   3. rebuild (é ele que recria os separadores na forma certa e reaplica o
//      alinhamento de cada botão, que é definido em ToolbarGroupWidget::addButton);
//   4. folha de estilo nova;
//   5. re-polir os botões de tipografia — ver comentário abaixo;
//   6. tamanhos.
void TopToolbar::setBarSide(Qt::Edge side)
{
    if (m_barSide == side) return;
    m_barSide = side;
    const bool vertical = isVertical();

    // O modo de reorganização não sobrevive à troca: alças e orientação são
    // refeitas embaixo dele.
    if (m_editMode) setEditMode(false);

    applyTypographyButtonMode();

    if (m_mainLayout) {
        m_mainLayout->setDirection(vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        m_mainLayout->setContentsMargins(vertical ? 4 : 14, vertical ? 14 : 4,
                                         vertical ? 4 : 14, vertical ? 14 : 4);
    }
    for (ToolbarGroupWidget *g : std::as_const(m_groupWidgets)) {
        if (g) g->setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
    }
    rebuildGroupLayout();

    // A barra tinha altura OU largura fixa; setFixedWidth/Height mexe em min E
    // max, então o eixo que deixou de ser fixo ficaria preso no valor antigo.
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    applyRootStyle();

    // As restrições de geometria do QSS (`min-width`) só entram no widget
    // durante o polish, que já aconteceu há muito tempo — sem forçar aqui, o
    // botão de fonte voltaria pro modo horizontal SEM o min-width de 130px que
    // ele precisa pra caber o nome da fonte, e no caminho inverso levaria junto
    // a restrição do modo antigo. Zerar antes é o que faz o QSS novo valer de
    // fato; ver verticalGeometryReset() pro mecanismo completo.
    for (QToolButton *b : { fontButton, sizeButton, lineHeightButton, indentButton }) {
        if (!b) continue;
        b->setMinimumSize(0, 0);
        b->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        style()->unpolish(b);
        style()->polish(b);
    }

    applyUiScale();     // re-fixa tamanhos e o eixo certo da barra
    updateAlignButtonIcon();
    refreshLayoutAndOverflow();

    emit barSideChanged(m_barSide);
}

void TopToolbar::applyTypographyButtonMode()
{
    const bool vertical = isVertical();

    if (fontButton) {
        if (vertical) {
            fontButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
            fontButton->setText(QString());
            fontButton->setToolTip(currentFontFamily);
        } else {
            // Solta o tamanho fixo que o modo lateral impôs — no topo a largura
            // é livre, ela acompanha o nome da fonte.
            fontButton->setMinimumSize(0, 0);
            fontButton->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            fontButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
            fontButton->setToolTip(QString());
            fontButton->setText(currentFontFamily);
            applyFontButtonStyle();
        }
    }

    const Qt::ToolButtonStyle sizeStyle = vertical
        ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon;
    if (sizeButton) {
        sizeButton->setToolButtonStyle(sizeStyle);
        if (!vertical) sizeButton->setToolTip(tr("Tamanho da fonte"));
    }
    if (lineHeightButton) {
        lineHeightButton->setToolButtonStyle(sizeStyle);
        if (!vertical) lineHeightButton->setToolTip(tr("Espaçamento"));
    }
    // Reescrevem texto/tooltip conforme o modo (ver as duas funções).
    updateSizeButtonLabel();
    updateLineHeightButtonLabel();
}

// Neutraliza, SÓ no modo vertical, as regras de geometria que o QSS global
// (Theme::globalStyleSheet, aplicado no qApp) impõe aos botões de tipografia.
//
// Aquelas regras foram escritas para a barra HORIZONTAL, onde #ttbFont exibe o
// NOME da fonte como texto e por isso pede min-width: 130px. Na barra vertical
// os mesmos botões viram quadrados ícone-only de tamanho fixo (applyUiScale) e
// esse mínimo passa a ser veneno:
//
//   QStyleSheetStyle aplica `min-width` chamando setMinimumWidth() no próprio
//   widget, durante o polish. Polish é PREGUIÇOSO — acontece no primeiro
//   layout, ou seja DEPOIS do setFixedSize() do construtor. O mínimo do QSS
//   sobrescreve o tamanho fixo e, como setGeometry_sys() faz qMax(w, minw)
//   depois de qMin(w, maxw), o MÍNIMO GANHA DO MÁXIMO. No cold start o
//   #ttbFont nascia deitado com 148px (130 + padding 16 + borda 2) dentro de
//   uma coluna de 48px — sumia da barra; #ttbSize/#ttbLineHeight nasciam com
//   64px (60 + 2 + 2) e o ícone ficava centralizado nessa caixa larga demais,
//   parecendo "descentralizado". Mexer no slider de escala de UI destravava
//   porque um segundo setFixedSize(), já com o widget polido, gruda.
//
// A folha de um ancestral (esta) vence a do QApplication independente de
// especificidade — por isso basta redeclarar aqui. Modo horizontal fica
// intocado.
//
// O `padding` é redeclarado junto, e de propósito IGUAL ao da regra genérica
// `#topToolbar QToolButton` (4px 6px): cada um desses quatro tinha um padding
// próprio pensado pro modo horizontal (`4px 8px` no #ttbFont, `4px 0` no
// #ttbIndent...), e como o padding encolhe a área de conteúdo, ele acaba
// decidindo o tamanho FINAL do ícone — o `setIconSize` só vale até onde a
// caixa deixa. Padding diferente = ícone renderizado em tamanho diferente do
// resto da barra, que foi exatamente a queixa de "ficou apertado" (mais tinta
// na mesma caixa = menos ar entre os botões). Uniformizando o padding, os 30
// botões renderizam no mesmo tamanho.
QString TopToolbar::verticalGeometryReset() const
{
    if (!isVertical()) return QString();
    return QStringLiteral(R"(
        QToolButton#ttbFont, QToolButton#ttbSize,
        QToolButton#ttbLineHeight, QToolButton#ttbIndent {
            min-width: 0px;
            padding: 4px 6px;
        }
    )");
}

void TopToolbar::applyRootStyle()
{
    // Mesmo estilo "painel" da LeftBar/painéis flutuantes (panelBackground +
    // borda + raio configurável no Editor de Temas) — antes era um
    // background liso (appBackground, sem borda/raio), inconsistente com o
    // resto do chrome do app. Botões continuam transparentes com hover sutil;
    // as cores dos ícones já vêm tintadas via loadIcon().
    setStyleSheet(QStringLiteral(R"(
        QWidget#topToolbar {
            background: %1;
            border: 1px solid %7;
            border-radius: %8;
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
        /* Os botoes de MODO nao usam o realce de "checked": esse destaque e a
           linguagem do negrito/italico (estado de formatacao). Um modo do app e
           sinalizado pela luzinha no canto — ver positionModeBadges(). */
        QToolButton#ttbMode:checked, QToolButton#ttbIndent:checked {
            background: transparent;
            border-color: transparent;
            color: %2;
        }
        QToolButton#ttbSize, QToolButton#ttbLineHeight {
            padding: 0px 1px;
            spacing: 1px;
            font-size: %9px;
            color: %10;
        }
        QToolButton#ttbSize:hover, QToolButton#ttbLineHeight:hover {
            color: %5;
        }
        QFrame#ttbVSep {
            color: %4;
            background: %4;
        }
        %11
    )").arg(
        Theme::panelBackground(),    // 1 — fundo
        Theme::textPrimary(),        // 2 — texto dos botões em estado normal
        Theme::hoverOverlay(),       // 3 — hover bg
        Theme::subtleBorder(),       // 4 — borda hover / separador
        Theme::textBright(),         // 5 — texto hover/checked
        Theme::pressedOverlay(),     // 6 — checked bg
        Theme::panelBorder(),        // 7 — borda do corpo da barra
        Theme::panelBorderRadius(),  // 8 — raio configurável (Editor de Temas)
        QString::number(isVertical() ? 10 : 11), // 9 — texto do tamanho/espaçamento
        Theme::textMuted(),          // 10 — cor discreta desse mesmo texto
        verticalGeometryReset()      // 11 — anula min-width do QSS global (ver abaixo)
    ));

    if (reminderBadge) {
        reminderBadge->setStyleSheet(QStringLiteral(
            "QLabel#reminderBadge {"
            "  background: %1; border-radius: 4px;"
            "}").arg(Theme::accentDanger()));
    }

    for (QLabel *badge : { readModeBadge, focusModeBadge, indentBadge }) {
        if (!badge) continue;
        // Ponto aceso com halo: o halo e o que faz parecer luz e nao sujeira.
        badge->setStyleSheet(QStringLiteral(
            "QLabel#modeBadge {"
            "  background: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 4px;"
            "}").arg(Theme::accentDefault(), Theme::panelBackground()));
    }

    if (pensarioBadge) {
        pensarioBadge->setStyleSheet(QStringLiteral(
            "QLabel#pensarioBadge {"
            "  background: %1; border-radius: 4px;"
            "}").arg(Theme::accentSuccess()));
    }
}

void TopToolbar::reloadIcons()
{
    const int px = currentIconPx();
    focusOffIcon = loadIcon(QStringLiteral("focusmode-off.svg"), px);
    focusOnIcon  = loadIcon(QStringLiteral("focusmode-on.svg"), px);
    if (focusButton) {
        focusButton->setIcon(focusCheckedCache ? focusOnIcon : focusOffIcon);
    }
    readModeOffIcon = loadIcon(QStringLiteral("focusededitor-off.svg"), px);
    readModeOnIcon  = loadIcon(QStringLiteral("focusededitor-on.svg"), px);
    if (readModeButton) {
        readModeButton->setIcon(readModeOn ? readModeOnIcon : readModeOffIcon);
    }
    for (const auto& pair : iconBindings) {
        if (!pair.first) continue;
        // Respeita o iconSize() JÁ configurado no próprio botão — não assume
        // `px` (padrão da barra) pra todo mundo — sobrescrever com `px`
        // recarregava o ícone renderizado no tamanho errado, esticado pro
        // tamanho real do widget (ficava borrado/sumido/pequeno demais).
        const int currentPx = pair.first->iconSize().width();
        const int iconPx = currentPx > 0 ? currentPx : px;
        pair.first->setIcon(loadIcon(pair.second, iconPx));
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
    if (alignButton) alignButton->setIcon(loadIcon(alignIconName(m_currentAlignment), currentIconPx()));
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
    // applyUiScale() no lugar de reloadIcons() direto: ele já recarrega os
    // ícones E re-afirma os tamanhos fixos. Necessário porque setStyleSheet()
    // faz o Qt re-polir os botões, e o polish reaplica a geometria do QSS por
    // cima do que fixamos (mesmo mecanismo do bug da barra vertical).
    applyUiScale();
    for (ToolbarGroupWidget* g : std::as_const(m_groupWidgets)) {
        if (g) g->applyTheme(QColor(Theme::textMuted()), QColor(Theme::accentDefault()));
    }
}

void TopToolbar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    refreshLayoutAndOverflow();
    positionReminderBadge();
    positionPensarioBadge();
    positionModeBadges();
}

// Sair do modo de reorganização é clicar em qualquer coisa que não seja um
// ícone arrastável — a mesma ideia de "tocar fora" que todo celular usa.
// Precisar segurar de novo pra sair era contraintuitivo.
//
// Este handler cobre as partes vazias da própria barra (margens, frestas entre
// grupos, separadores, área do título). Um clique na área vazia de um GRUPO
// também cai aqui: o ToolbarGroupWidget ignora o press quando ele não é na
// alça, e evento ignorado sobe pro pai. Clique FORA da barra inteira é o outro
// caminho, no eventFilter abaixo.
void TopToolbar::mousePressEvent(QMouseEvent *event)
{
    if (m_editMode && event->button() == Qt::LeftButton) {
        setEditMode(false);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

// Filtro no nível do QApplication, instalado SÓ enquanto o modo de edição está
// ligado (ver setEditMode) — é o que permite perceber um clique em qualquer
// outro lugar do app: no texto, na LeftBar, num painel.
//
// O evento não é consumido de propósito: quem clicou no meio do texto quer o
// cursor ali, não só sair do modo de edição. Sair é efeito colateral do
// clique, não substituto dele.
bool TopToolbar::eventFilter(QObject *watched, QEvent *event)
{
    if (m_editMode && event->type() == QEvent::MouseButtonPress) {
        // Um filtro de aplicação vê eventos de QWindow também, não só de
        // widget; só os de widget interessam aqui.
        auto *w = qobject_cast<QWidget*>(watched);
        if (w && w != this && !isAncestorOf(w)) setEditMode(false);
    }
    return QWidget::eventFilter(watched, event);
}

QRect TopToolbar::immersiveSoundButtonGlobalRect() const
{
    if (!immersiveSoundButton) return QRect();
    const QPoint topLeft = immersiveSoundButton->mapToGlobal(QPoint(0, 0));
    return QRect(topLeft, immersiveSoundButton->size());
}

QRect TopToolbar::readAloudButtonGlobalRect() const
{
    if (!readAloudButton) return QRect();
    const QPoint topLeft = readAloudButton->mapToGlobal(QPoint(0, 0));
    return QRect(topLeft, readAloudButton->size());
}

QRect TopToolbar::repetitionsButtonGlobalRect() const
{
    if (!repetitionsButton) return QRect();
    return QRect(repetitionsButton->mapToGlobal(QPoint(0, 0)), repetitionsButton->size());
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

QLabel *TopToolbar::makeBadge(const QString &objectName)
{
    auto *b = new QLabel(this);
    b->setObjectName(objectName);
    b->setFixedSize(9, 9);
    b->setAttribute(Qt::WA_TransparentForMouseEvents);
    b->setVisible(false);
    return b;
}

void TopToolbar::positionBadge(QLabel *badge, QToolButton *button)
{
    if (!badge || !button || !badge->isVisible()) return;
    // mapTo(this, ...) e NAO geometry(): desde que os botoes passaram a morar
    // dentro de ToolbarGroupWidget, geometry() e relativa ao GRUPO, enquanto o
    // badge continua filho da barra. Comparar os dois punha a luzinha num lugar
    // qualquer — mesmo motivo do mapTo em positionDocTitle na epoca.
    const QPoint tl = button->mapTo(this, QPoint(0, 0));
    badge->move(tl.x() + button->width() - 10, tl.y() + 2);
    badge->raise();
}

void TopToolbar::positionReminderBadge()
{
    positionBadge(reminderBadge, reminderButton);
}

void TopToolbar::positionPensarioBadge()
{
    positionBadge(pensarioBadge, pensarioButton);
}

// Acesas enquanto o modo estiver ligado. O modo focado esconde justamente a UI
// que mostraria que ele esta ativo, entao a luzinha e o unico sinal que sobra.
void TopToolbar::positionModeBadges()
{
    if (readModeBadge) {
        readModeBadge->setVisible(readModeOn && readModeButton && readModeButton->isVisible());
        positionBadge(readModeBadge, readModeButton);
    }
    if (focusModeBadge) {
        focusModeBadge->setVisible(focusCheckedCache && focusButton && focusButton->isVisible());
        positionBadge(focusModeBadge, focusButton);
    }
    // Identacao de primeira linha entra aqui e nao no grupo do negrito/italico:
    // ela e uma preferencia do documento inteiro, ligada ou desligada, e nao uma
    // formatacao aplicada ao trecho selecionado.
    if (indentBadge) {
        indentBadge->setVisible(indentButton && indentButton->isChecked() && indentButton->isVisible());
        positionBadge(indentBadge, indentButton);
    }
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

// O título do documento saiu daqui: ele agora mora numa faixa fixa no topo da
// folha do editor (DocHeaderBar), nos DOIS modos de barra. Ele dependia de um
// espaço reservado entre grupos específicos e de medir a distância entre dois
// botões nomeados — nada disso sobrevive a grupos e botões que o usuário pode
// arrastar pra onde quiser. Um lugar só pro título, e o que já funcionava.
//
// O que sobrou aqui é a faxina de layout que o resto do arquivo depende:
// activate() é o que avisa o QBoxLayout pra recalcular posições depois que
// applyUiScale() redimensiona botões — sem isso os vizinhos de baixo ficam com
// geometria antiga, sobrepondo quem está acima.
void TopToolbar::refreshLayoutAndOverflow()
{
    if (auto *lay = layout()) lay->activate();
    updateOverflow();
}

void TopToolbar::collapseToOverflow(QToolButton *btn)
{
    if (!btn || !btn->isVisible()) return;
    btn->hide(); // QLayout ignora widgets escondidos — libera o espaço sozinho
    QAction *action = m_overflowActions.value(btn);
    if (action && !m_overflowMenu->actions().contains(action))
        m_overflowMenu->addAction(action);
}

void TopToolbar::restoreFromOverflow(QToolButton *btn)
{
    if (!btn || btn->isVisible()) return;
    btn->show();
    QAction *action = m_overflowActions.value(btn);
    if (action) m_overflowMenu->removeAction(action);
}

// Colapsa botões dispensáveis pra dentro do menu "⋯" quando a barra horizontal
// não tem largura pra todos.
//
// O critério antes era "sobrar 180px entre o imageButton e o readModeButton",
// porque esse vão era onde o título do documento morava. Duas coisas mataram
// isso: o título saiu da barra (ver refreshLayoutAndOverflow) e os botões
// passaram a ser arrastáveis — medir a distância entre dois botões nomeados
// deixou de significar qualquer coisa quando o usuário pode trocá-los de lugar.
// Agora a pergunta é direta: o conteúdo cabe na barra?
// Largura de referência ESTÁVEL pra decidir o overflow: a da janela, não a da
// própria barra.
//
// Desde que a barra horizontal passou a ter a largura do conteúdo
// (QSizePolicy::Maximum), medir contra `width()` virou realimentação: colapsar
// um botão encolhe o conteúdo, o que encolhe a barra, o que faz o conteúdo
// "caber" de novo, o que manda restaurar o botão, o que cresce... e o
// resizeEvent fecha o ciclo. Na prática a barra piscava sem parar, com os
// botões pulando sozinhos.
int TopToolbar::availableBarWidth() const
{
    // 24 = margens laterais do holder (12 de cada lado, ver MainWindow).
    constexpr int kHolderSideMargins = 24;
    const QWidget *win = window();
    return qMax(0, (win ? win->width() : width()) - kHolderSideMargins);
}

void TopToolbar::updateOverflow()
{
    if (!overflowButton || !m_mainLayout || m_collapsePriority.isEmpty()) return;
    // A conta é toda em X — numa barra vertical o eixo apertado é o Y, e lá o
    // pior caso é cortar embaixo, sem corromper nada. Ver ToolbarGroupWidget.
    if (isVertical()) return;
    // Mostrar/esconder botão redimensiona a barra, o que dispara resizeEvent, que
    // chama isto de novo — sem a trava, uma volta vira uma cascata.
    if (m_inOverflowUpdate) return;
    m_inOverflowUpdate = true;
    const auto releaseGuard = qScopeGuard([this]() { m_inOverflowUpdate = false; });

    auto tooWide = [this]() {
        m_mainLayout->invalidate();
        m_mainLayout->activate();
        return m_mainLayout->minimumSize().width() > availableBarWidth();
    };

    // Encolhe: do menos essencial pro mais essencial, um por vez, até caber.
    for (QToolButton *btn : std::as_const(m_collapsePriority)) {
        if (!tooWide()) break;
        collapseToOverflow(btn);
    }

    // Cresce: tenta devolver o mais essencial dos colapsados primeiro; se não
    // coube, desfaz e para — os que sobraram são ainda menos essenciais.
    for (int i = m_collapsePriority.size() - 1; i >= 0; --i) {
        QToolButton *btn = m_collapsePriority.at(i);
        if (!btn || btn->isVisible()) continue;
        restoreFromOverflow(btn);
        if (tooWide()) {
            collapseToOverflow(btn);
            break;
        }
    }

    overflowButton->setVisible(!m_overflowMenu->actions().isEmpty());
}

void TopToolbar::setFontFamilies(const QStringList &families, const QString &current)
{
    fontFamilies = families;
    currentFontFamily = current;
    if (isVertical()) fontButton->setToolTip(currentFontFamily);
    else { fontButton->setText(currentFontFamily); applyFontButtonStyle(); }
}

void TopToolbar::setFontSize(qreal pt)
{
    currentFontSize = pt;
    updateSizeMenuState();
}

void TopToolbar::setLineHeightPercent(int percent)
{
    currentLineHeightPercent = percent;
    updateLineHeightButtonLabel();
    updateSpacingMenuChecks();
}

void TopToolbar::setFirstLineIndentEnabled(bool enabled)
{
    QSignalBlocker block(indentButton);
    indentButton->setChecked(enabled);
    positionModeBadges();
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

// Sincroniza o botao sem reemitir o sinal — usado na restauracao do estado
// salvo, onde quem manda e o QSettings e nao um clique do usuario.
void TopToolbar::setReadModeChecked(bool checked)
{
    if (!readModeButton) return;
    QSignalBlocker block(readModeButton);
    readModeButton->setChecked(checked);
    readModeOn = checked;
    readModeButton->setIcon(checked ? readModeOnIcon : readModeOffIcon);
    positionModeBadges();
}

void TopToolbar::setFocusModeChecked(bool checked)
{
    if (!focusButton) return;
    QSignalBlocker block(focusButton);
    focusButton->setChecked(checked);
    focusCheckedCache = checked;
    focusButton->setIcon(checked ? focusOnIcon : focusOffIcon);
    positionModeBadges();
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
    updateSizeButtonLabel();
    applyFontButtonStyle();
    if (sizeStepperEdit) {
        QSignalBlocker block(sizeStepperEdit);
        sizeStepperEdit->setText(sizeText(currentFontSize));
    }
    for (QAction *a : std::as_const(sizePresetActions)) {
        a->setChecked(qFuzzyCompare(currentFontSize, qreal(a->data().toInt())));
    }
}

void TopToolbar::updateSizeButtonLabel()
{
    if (!sizeButton) return;
    const QString txt = sizeText(currentFontSize);
    // Limpar a representação do outro modo não é zelo: numa troca ao vivo o
    // texto antigo sobreviveria e o botão ícone-only ficaria com um "17.5"
    // fantasma influenciando o sizeHint.
    if (isVertical()) {
        sizeButton->setText(QString());
        sizeButton->setToolTip(tr("Tamanho da fonte (%1)").arg(txt));
    } else {
        sizeButton->setText(txt);
    }
}

void TopToolbar::updateLineHeightButtonLabel()
{
    if (!lineHeightButton) return;
    const QString txt = QString::number(currentLineHeightPercent / 100.0, 'f', 1);
    if (isVertical()) {
        lineHeightButton->setText(QString());
        lineHeightButton->setToolTip(tr("Espaçamento (%1)").arg(txt));
    } else {
        lineHeightButton->setText(txt);
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
            updateLineHeightButtonLabel();
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
