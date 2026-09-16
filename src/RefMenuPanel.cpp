#include "RefMenuPanel.h"

#include "ConstrutorStore.h"
#include "TerritorioStore.h"
#include "DocCache.h"
#include "DocPreview.h"
#include "EditorHost.h"
#include "ElementsStore.h"
#include "FindBar.h"
#include "AvatarUtils.h"
#include "IconUtils.h"
#include "MarkerStore.h"
#include "MemoriesStore.h"
#include "ProjectModel.h"
#include "RoleTiers.h"
#include "ProjectStorage.h"
#include "SceneUtils.h"
#include "Theme.h"

#include <QApplication>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QByteArray>
#include <QEvent>
#include <functional>
#include <QFile>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QSplitter>
#include <QSettings>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTimer>
#include <QUrl>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <algorithm>

namespace {
constexpr int kDefaultW = 380;
constexpr int kDefaultH = 720;
constexpr int kMinW = 280;
constexpr int kMinH = 360;
constexpr int kHandleW = 6;     // espessura das faixas de resize
constexpr int kCornerSz = 12;   // tamanho dos handles de canto
constexpr int kHeaderH = 38;

const char* kKeyGeom        = "ui/refMenuPanel/geometry";
const char* kKeySplit       = "ui/refMenuPanel/bodySplit";
const char* kKeyNavHidden   = "ui/refMenuPanel/navHidden";
const char* kKeyVisualMode  = "ui/refMenuPanel/visualMode";
const char* kKeyPinned      = "ui/refMenuPanel/pinned";
const char* kKeyFontPt      = "ui/refMenuPanel/previewFontPt";

// keys de seleção (UserRole nos QListWidgetItem):
//   ms:<msId>            → manuscrito
//   ch:<msId>:<chId>     → capítulo
//   sc:<msId>:<chId>:<i> → cena
//   it:<itemId>          → item de gaveta
//   fl:<folderId>        → folder (drilling)
}

// =========================================================================
// Construção e ciclo de vida
// =========================================================================

RefMenuPanel::RefMenuPanel(ProjectModel* model, EditorHost* host, DocCache* cache,
                           ElementsStore* elements, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
    , m_model(model)
    , m_host(host)
    , m_cache(cache)
    , m_elements(elements)
{
    setObjectName(QStringLiteral("refMenuPanel"));
    setWindowTitle(tr("Referência"));
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumSize(kMinW, kMinH);
    resize(kDefaultW, kDefaultH);

    buildUi();
    loadGeometryFromSettings();
    applyPreviewFont();
    applyNavVisibility();

    if (m_model) {
        connect(m_model, &ProjectModel::manuscriptsChanged, this, &RefMenuPanel::refresh);
        connect(m_model, &ProjectModel::chaptersChanged, this, &RefMenuPanel::refresh);
        connect(m_model, &ProjectModel::drawersChanged, this, &RefMenuPanel::refresh);
        connect(m_model, &ProjectModel::loaded, this, &RefMenuPanel::refresh);
    }
    if (m_elements) {
        connect(m_elements, &ElementsStore::changed, this, &RefMenuPanel::refresh);
    }

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &RefMenuPanel::applyTheme);

    hide();
}

void RefMenuPanel::applyTheme()
{
    // Reaplica a stylesheet principal (header, tabs, preview wrapper, handles)
    // e força um rebuild da nav/preview pra estilos inline serem regerados.
    applyMainStyleSheet();
    refresh();
}

void RefMenuPanel::setProjectRoot(const QString& root)
{
    // Fixados e recentes são por projeto: troca de projeto, troca a lista.
    const bool changed = (m_projectRoot != root);
    m_projectRoot = root;
    if (changed) {
        m_pinnedKeys.clear();
        m_recentKeys.clear();
        loadPinsAndRecents();
        m_navMode = NavMode::Home;
    }
}

// =========================================================================
// UI
// =========================================================================

void RefMenuPanel::buildUi()
{
    // Layout externo: sem margem — os handles dedicados cobrem as bordas.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_frame = new QWidget(this);
    m_frame->setObjectName(QStringLiteral("refFrame"));
    m_frame->setAttribute(Qt::WA_StyledBackground, true);
    outer->addWidget(m_frame, /*stretch=*/1);

    m_frameLay = new QVBoxLayout(m_frame);
    m_frameLay->setContentsMargins(0, 0, 0, 0);
    m_frameLay->setSpacing(0);

    // ---- Header ----
    m_header = new QWidget(m_frame);
    m_header->setObjectName(QStringLiteral("refHeader"));
    m_header->setAttribute(Qt::WA_StyledBackground, true);
    m_header->setFixedHeight(kHeaderH);
    auto* hLay = new QHBoxLayout(m_header);
    hLay->setContentsMargins(8, 4, 6, 4);
    hLay->setSpacing(6);

    m_dragHandle = new QToolButton(m_header);
    m_dragHandle->setObjectName(QStringLiteral("refDragHandle"));
    m_dragHandle->setText(QStringLiteral(":::"));
    m_dragHandle->setToolTip(tr("Arrastar (duplo clique pra resetar)"));
    m_dragHandle->setCursor(Qt::OpenHandCursor);
    m_dragHandle->installEventFilter(this);
    hLay->addWidget(m_dragHandle);

    m_title = new QLabel(tr("Referência"), m_header);
    m_title->setObjectName(QStringLiteral("refTitle"));
    hLay->addWidget(m_title);

    hLay->addStretch();

    m_toggleNavBtn = new QToolButton(m_header);
    m_toggleNavBtn->setObjectName(QStringLiteral("refTinyBtn"));
    m_toggleNavBtn->setText(QStringLiteral("▾"));
    m_toggleNavBtn->setToolTip(tr("Ocultar/Mostrar explorador"));
    m_toggleNavBtn->setCursor(Qt::PointingHandCursor);
    connect(m_toggleNavBtn, &QToolButton::clicked, this, &RefMenuPanel::onToggleNav);
    hLay->addWidget(m_toggleNavBtn);

    m_searchBtn = new QToolButton(m_header);
    m_searchBtn->setObjectName(QStringLiteral("refTinyBtn"));
    {
        QIcon ic = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/search.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14));
        if (!ic.isNull()) m_searchBtn->setIcon(ic);
    }
    m_searchBtn->setToolTip(tr("Pesquisar no RefMenu (Ctrl+Alt+F)"));
    m_searchBtn->setCursor(Qt::PointingHandCursor);
    m_searchBtn->setCheckable(true);
    connect(m_searchBtn, &QToolButton::clicked, this, &RefMenuPanel::onToggleSearch);
    hLay->addWidget(m_searchBtn);

    m_fontSizeBtn = new QToolButton(m_header);
    m_fontSizeBtn->setObjectName(QStringLiteral("refTinyBtn"));
    m_fontSizeBtn->setText(QStringLiteral("Aa"));
    m_fontSizeBtn->setToolTip(tr("Tamanho do texto do preview"));
    m_fontSizeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_fontSizeBtn, &QToolButton::clicked, this, &RefMenuPanel::onCycleFontSize);
    hLay->addWidget(m_fontSizeBtn);

    m_editBtn = new QToolButton(m_header);
    m_editBtn->setObjectName(QStringLiteral("refTinyBtn"));
    {
        QIcon ic = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/edit.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14));
        if (!ic.isNull()) m_editBtn->setIcon(ic);
    }
    m_editBtn->setCheckable(true);
    m_editBtn->setCursor(Qt::PointingHandCursor);
    m_editBtn->setToolTip(tr("Editar documento"));
    m_editBtn->setEnabled(false);
    connect(m_editBtn, &QToolButton::toggled, this, &RefMenuPanel::onToggleEdit);
    hLay->addWidget(m_editBtn);

    m_pinBtn = new QToolButton(m_header);
    m_pinBtn->setObjectName(QStringLiteral("refTinyBtn"));
    {
        QIcon ic = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/pin.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14));
        if (!ic.isNull()) m_pinBtn->setIcon(ic);
    }
    m_pinBtn->setToolTip(tr("Fixar"));
    m_pinBtn->setCheckable(true);
    m_pinBtn->setCursor(Qt::PointingHandCursor);
    connect(m_pinBtn, &QToolButton::clicked, this, &RefMenuPanel::onTogglePin);
    hLay->addWidget(m_pinBtn);

    m_closeBtn = new QToolButton(m_header);
    m_closeBtn->setObjectName(QStringLiteral("refTinyBtn"));
    m_closeBtn->setText(QStringLiteral("✕"));
    m_closeBtn->setToolTip(tr("Fechar"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_closeBtn, &QToolButton::clicked, this, &RefMenuPanel::onCloseClicked);
    hLay->addWidget(m_closeBtn);

    m_frameLay->addWidget(m_header);

    // ---- Busca (sempre visível) ----
    // Antes ficava atrás de um botão de lupa. Consultar É a função do painel,
    // então o campo é a primeira coisa abaixo do cabeçalho — não um recurso
    // escondido que só quem sabe do atalho encontra.
    m_searchRow = new QWidget(m_frame);
    m_searchRow->setObjectName(QStringLiteral("refSearchRow"));
    m_searchRow->setAttribute(Qt::WA_StyledBackground, true);
    {
        auto* sLay = new QHBoxLayout(m_searchRow);
        sLay->setContentsMargins(10, 8, 10, 6);
        sLay->setSpacing(6);
        m_searchInput = new QLineEdit(m_searchRow);
        m_searchInput->setObjectName(QStringLiteral("refSearchInput"));
        m_searchInput->setPlaceholderText(tr("Buscar em tudo…"));
        m_searchInput->setClearButtonEnabled(true);
        connect(m_searchInput, &QLineEdit::textChanged, this, &RefMenuPanel::onSearchQueryChanged);
        sLay->addWidget(m_searchInput, 1);
    }
    m_frameLay->addWidget(m_searchRow);

    // ---- Trilha ----
    // No lugar de "Manuscritos ▾" + "Gaveta ▾", que competiam pelo comando e
    // não diziam onde você estava. Some na tela inicial (não há caminho a
    // mostrar) e reaparece ao entrar numa seção ou abrir um documento.
    m_crumbsRow = new QWidget(m_frame);
    m_crumbsRow->setObjectName(QStringLiteral("refCrumbsRow"));
    m_crumbsRow->setAttribute(Qt::WA_StyledBackground, true);
    m_crumbsLay = new QHBoxLayout(m_crumbsRow);
    m_crumbsLay->setContentsMargins(10, 2, 8, 8);
    m_crumbsLay->setSpacing(4);

    // Modo visual e filtro de território seguem existindo, mas agora à direita
    // da trilha — aparecem só nas seções em que fazem sentido.
    m_viewModeBtn = new QToolButton(m_crumbsRow);
    m_viewModeBtn->setObjectName(QStringLiteral("refTabBtnSmall"));
    m_viewModeBtn->setText(QStringLiteral("≡"));
    m_viewModeBtn->setToolTip(tr("Alternar modo visual/lista"));
    m_viewModeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_viewModeBtn, &QToolButton::clicked, this, &RefMenuPanel::onToggleVisualMode);

    m_territorioFilterBtn = new QToolButton(m_crumbsRow);
    m_territorioFilterBtn->setObjectName(QStringLiteral("refTabBtnSmall"));
    m_territorioFilterBtn->setText(tr("Território: Todos"));
    m_territorioFilterBtn->setCursor(Qt::PointingHandCursor);
    m_territorioFilterBtn->setPopupMode(QToolButton::InstantPopup);
    m_territorioFilterMenu = new QMenu(m_territorioFilterBtn);
    connect(m_territorioFilterMenu, &QMenu::aboutToShow, this, &RefMenuPanel::rebuildTerritorioFilterMenu);
    m_territorioFilterBtn->setMenu(m_territorioFilterMenu);

    m_crumbsRow->setVisible(false);
    m_frameLay->addWidget(m_crumbsRow);


    // ---- Nav body ----
    m_navScroll = new QScrollArea(m_frame);
    m_navScroll->setObjectName(QStringLiteral("refNavScroll"));
    m_navScroll->setWidgetResizable(true);
    m_navScroll->setFrameShape(QFrame::NoFrame);
    m_navScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_navInner = new QWidget(m_navScroll);
    m_navInner->setObjectName(QStringLiteral("refNavInner"));
    m_navInner->setAttribute(Qt::WA_StyledBackground, true);
    m_navInnerLay = new QVBoxLayout(m_navInner);
    m_navInnerLay->setContentsMargins(10, 6, 10, 10);
    m_navInnerLay->setSpacing(6);
    m_navInnerLay->addStretch();
    m_navScroll->setWidget(m_navInner);
    // Navegação e preview vivem num divisor arrastável: a divisão entre "lista"
    // e "texto" é preferência do momento (às vezes você quer ver a árvore
    // inteira, às vezes quer ler). Sem teto fixo de altura — quem decide é a
    // alça, e a posição fica guardada.
    m_navScroll->setMinimumHeight(80);
    m_bodySplit = new QSplitter(Qt::Vertical, m_frame);
    m_bodySplit->setObjectName(QStringLiteral("refBodySplit"));
    m_bodySplit->setChildrenCollapsible(false);
    m_bodySplit->setHandleWidth(7);
    m_bodySplit->addWidget(m_navScroll);
    m_frameLay->addWidget(m_bodySplit, /*stretch=*/1);

    // ---- Preview ----
    m_previewWrap = new QWidget(m_frame);
    m_previewWrap->setObjectName(QStringLiteral("refPreviewWrap"));
    m_previewWrap->setAttribute(Qt::WA_StyledBackground, true);
    auto* pvLay = new QVBoxLayout(m_previewWrap);
    pvLay->setContentsMargins(12, 10, 12, 12);
    pvLay->setSpacing(6);

    m_previewTitle = new QLabel(m_previewWrap);
    m_previewTitle->setObjectName(QStringLiteral("refPreviewTitle"));
    m_previewTitle->setWordWrap(true);
    m_previewTitle->setVisible(false);
    pvLay->addWidget(m_previewTitle);

    m_previewRole = new QLabel(m_previewWrap);
    m_previewRole->setObjectName(QStringLiteral("refPreviewRole"));
    m_previewRole->setVisible(false);
    pvLay->addWidget(m_previewRole);

    m_previewImagesScroll = new QScrollArea(m_previewWrap);
    m_previewImagesScroll->setObjectName(QStringLiteral("refImagesScroll"));
    m_previewImagesScroll->setWidgetResizable(true);
    m_previewImagesScroll->setFrameShape(QFrame::NoFrame);
    m_previewImagesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_previewImagesScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_previewImagesScroll->setMaximumHeight(360);
    m_previewImagesScroll->setVisible(false);
    m_previewImagesHost = new QWidget(m_previewImagesScroll);
    m_previewImagesLay = new QVBoxLayout(m_previewImagesHost);
    m_previewImagesLay->setContentsMargins(0, 0, 0, 0);
    m_previewImagesLay->setSpacing(8);
    m_previewImagesScroll->setWidget(m_previewImagesHost);
    pvLay->addWidget(m_previewImagesScroll);

    m_preview = new QTextBrowser(m_previewWrap);
    m_preview->setObjectName(QStringLiteral("refPreview"));
    m_preview->setReadOnly(true);
    m_preview->setOpenExternalLinks(false);
    m_preview->setFrameShape(QFrame::NoFrame);
    pvLay->addWidget(m_preview, /*stretch=*/1);

    m_previewPlaceholder = new QLabel(tr("Selecione um documento acima pra visualizar aqui."), m_previewWrap);
    m_previewPlaceholder->setObjectName(QStringLiteral("refPreviewPlaceholder"));
    m_previewPlaceholder->setAlignment(Qt::AlignCenter);
    m_previewPlaceholder->setWordWrap(true);
    pvLay->addWidget(m_previewPlaceholder);

    m_preview->setVisible(false);

    m_bodySplit->addWidget(m_previewWrap);
    m_bodySplit->setStretchFactor(0, 0);   // a árvore mantém o tamanho pedido
    m_bodySplit->setStretchFactor(1, 1);   // o texto absorve o resto
    m_bodySplit->setSizes({ 260, 460 });   // padrão até o usuário arrastar

    // FindBar do preview (Alt+F). Não entra no layout — flutua sobre o
    // m_previewWrap, posicionada em positionPreviewFindBar().
    m_previewFind = new FindBar(this);
    m_previewFind->attachTo(m_preview);
    connect(m_previewFind, &FindBar::selectionsChanged, this, [this]() {
        if (m_preview) m_preview->setExtraSelections(m_previewFind->findSelections());
    });
    connect(m_previewFind, &FindBar::closed, this, [this]() {
        if (m_preview) m_preview->setExtraSelections({});
    });
    m_previewFind->hide();
    m_previewFind->raise();

    // ---- Resize handles (estilo DrawerListPanel) ----
    auto makeHandle = [this](const QString& name, Qt::CursorShape cur) {
        auto* h = new QWidget(this);
        h->setObjectName(name);
        h->setAttribute(Qt::WA_StyledBackground, true);
        h->setCursor(cur);
        h->installEventFilter(this);
        h->raise();
        return h;
    };
    m_hL  = makeHandle(QStringLiteral("refHandleL"),  Qt::SizeHorCursor);
    m_hR  = makeHandle(QStringLiteral("refHandleR"),  Qt::SizeHorCursor);
    m_hT  = makeHandle(QStringLiteral("refHandleT"),  Qt::SizeVerCursor);
    m_hB  = makeHandle(QStringLiteral("refHandleB"),  Qt::SizeVerCursor);
    m_hTL = makeHandle(QStringLiteral("refHandleTL"), Qt::SizeFDiagCursor);
    m_hTR = makeHandle(QStringLiteral("refHandleTR"), Qt::SizeBDiagCursor);
    m_hBL = makeHandle(QStringLiteral("refHandleBL"), Qt::SizeBDiagCursor);
    m_hBR = makeHandle(QStringLiteral("refHandleBR"), Qt::SizeFDiagCursor);
    layoutResizeHandles();

    applyMainStyleSheet();
}

void RefMenuPanel::applyMainStyleSheet()
{
    const QString panelBg   = Theme::panelBackground();
    const QString appBg     = Theme::appBackground();
    const QString border    = Theme::panelBorder();
    const QString subtle    = Theme::subtleBorder();
    const QString txtPrim   = Theme::textPrimary();
    const QString txtMuted  = Theme::textMuted();
    const QString txtBright = Theme::textBright();
    const QString hover     = Theme::hoverOverlay();
    const QString accentSf  = Theme::accentInfoSoft();
    const QString accentBd  = Theme::accentInfoBorderSoft();
    const QString disabled  = Theme::disabledText();

    setStyleSheet(Theme::qss(QStringLiteral(R"(
        QWidget#refMenuPanel { background: transparent; }
        QWidget#refFrame {
            background: %1;
            border: 1px solid %3;
            border-radius: @radius-panel;
        }
        QWidget#refHeader {
            background: %2;
            border-bottom: 1px solid %4;
            border-top-left-radius: @radius-panel;
            border-top-right-radius: @radius-panel;
        }
        QLabel#refTitle { color: %7; font-size: 13px; font-weight: 600; }
        QToolButton#refTinyBtn {
            background: transparent; color: %6;
            border: none; border-radius: @radius-control;
            min-width: 24px; min-height: 24px;
            padding: 0 4px;
            font-size: 13px;
        }
        QToolButton#refTinyBtn:hover { background: %8; color: %7; }
        QToolButton#refTinyBtn:checked { background: %9; color: %7; }
        QToolButton#refTinyBtn:disabled { color: %11; }
        QToolButton#refDragHandle {
            background: transparent; color: %6;
            border: none; padding: 0 6px; font-size: 14px;
        }
        QToolButton#refDragHandle:hover { color: %7; }
        QWidget#refSearchRow {
            background: %2;
            border-bottom: 1px solid %4;
        }
        QLineEdit#refSearchInput {
            background: %1;
            color: %7;
            border: 1px solid %4;
            border-radius: @radius-control;
            padding: 4px 8px;
            font-size: 12px;
        }
        QLineEdit#refSearchInput:focus { border-color: %10; }
        QWidget#refCrumbsRow {
            background: %2;
            border-bottom: 1px solid %4;
        }
        QToolButton#refCrumbLink {
            background: transparent;
            color: %10;
            border: 0;
            border-radius: @radius-item;
            padding: 2px 5px;
            font-size: 12px;
        }
        QToolButton#refCrumbLink:hover { background: %8; }
        QLabel#refCrumbSep  { color: %5; font-size: 12px; padding: 0 1px; }
        QLabel#refCrumbHere { color: %7; font-size: 12px; padding: 2px 3px; }
        QToolButton#refTabBtn, QToolButton#refTabBtnSmall {
            background: transparent;
            color: %5;
            border: 1px solid transparent;
            border-radius: @radius-control;
            padding: 4px 10px;
            font-size: 12px;
        }
        QToolButton#refTabBtnSmall {
            padding: 2px 7px;
            min-width: 24px;
            font-size: 13px;
        }
        QToolButton#refTabBtn:hover, QToolButton#refTabBtnSmall:hover {
            background: %8; color: %7;
        }
        QToolButton#refTabBtn:checked {
            background: %9;
            border-color: %10;
            color: %7;
        }
        QToolButton#refTabBtn:disabled { color: %11; }
        QScrollArea#refNavScroll { background: transparent; border: none; }
        /* Alca do divisor: uma linha discreta que acende no hover, pra
           anunciar que da' pra arrastar sem virar um elemento gritante. */
        QSplitter#refBodySplit::handle:vertical {
            background: %4;
            height: 1px;
            margin: 3px 10px;
        }
        QSplitter#refBodySplit::handle:vertical:hover,
        QSplitter#refBodySplit::handle:vertical:pressed {
            background: %10;
            height: 2px;
            margin: 2px 8px;
        }
        QWidget#refNavInner { background: transparent; }
        QWidget#refPreviewWrap {
            background: transparent;
            border-top: 1px solid %4;
        }
        QLabel#refPreviewTitle {
            color: %7; font-size: 14px; font-weight: 700;
        }
        QLabel#refPreviewRole {
            color: %5; font-size: 10px; font-weight: 700;
            letter-spacing: 0.5px;
        }
        QTextBrowser#refPreview {
            background: transparent;
            color: %5;
            padding: 0;
        }
        QLabel#refPreviewPlaceholder {
            color: %6; font-size: 11px; padding: 12px 0;
        }
        QScrollArea#refImagesScroll { background: transparent; border: none; }
        QWidget#refHandleL, QWidget#refHandleR, QWidget#refHandleT, QWidget#refHandleB,
        QWidget#refHandleTL, QWidget#refHandleTR, QWidget#refHandleBL, QWidget#refHandleBR {
            background: transparent;
        }
        QWidget#refHandleL:hover, QWidget#refHandleR:hover,
        QWidget#refHandleT:hover, QWidget#refHandleB:hover,
        QWidget#refHandleTL:hover, QWidget#refHandleTR:hover,
        QWidget#refHandleBL:hover, QWidget#refHandleBR:hover {
            background: %12;
        }
    )"))
        .arg(panelBg,   // 1
             appBg,     // 2
             border,    // 3
             subtle,    // 4
             txtPrim,   // 5
             txtMuted,  // 6
             txtBright, // 7
             hover,     // 8
             accentSf,  // 9
             accentBd,  // 10
             disabled)  // 11
        .arg(subtle));  // 12 (handles hover)
}

void RefMenuPanel::layoutResizeHandles()
{
    if (!m_hL || !m_hR || !m_hT || !m_hB) return;
    const int w = width();
    const int h = height();
    const int hw = kHandleW;
    const int cs = kCornerSz;

    // Faixas: cobrem a borda inteira menos os cantos
    m_hL->setGeometry(0,        cs,       hw,       h - 2 * cs);
    m_hR->setGeometry(w - hw,   cs,       hw,       h - 2 * cs);
    m_hT->setGeometry(cs,       0,        w - 2*cs, hw);
    m_hB->setGeometry(cs,       h - hw,   w - 2*cs, hw);
    // Cantos: quadrados acima das faixas
    m_hTL->setGeometry(0,        0,        cs, cs);
    m_hTR->setGeometry(w - cs,   0,        cs, cs);
    m_hBL->setGeometry(0,        h - cs,   cs, cs);
    m_hBR->setGeometry(w - cs,   h - cs,   cs, cs);

    for (QWidget* h : { m_hL, m_hR, m_hT, m_hB, m_hTL, m_hTR, m_hBL, m_hBR }) {
        h->raise();
    }
}

// =========================================================================
// Reconstrução dos painéis
// =========================================================================

void RefMenuPanel::refresh()
{
    rebuildCrumbs();
    rebuildNavBody();
    rebuildPreview();
}

void RefMenuPanel::rebuildCrumbs()
{
    if (!m_crumbsRow || !m_crumbsLay) return;

    // Esvazia a trilha sem destruir os dois botoes persistentes (modo visual e
    // filtro de territorio), que sao membros e voltam ao layout logo abaixo.
    while (QLayoutItem* it = m_crumbsLay->takeAt(0)) {
        QWidget* w = it->widget();
        if (w && w != m_viewModeBtn && w != m_territorioFilterBtn) w->deleteLater();
        delete it;
    }

    const bool hasDoc = !m_selectedKey.isEmpty();
    const bool searching = !m_searchQuery.isEmpty();
    const bool inSection = (m_navMode == NavMode::Section);

    // Na tela inicial nao ha caminho a mostrar.
    if (!inSection && !hasDoc && !searching) {
        m_crumbsRow->setVisible(false);
        return;
    }
    m_crumbsRow->setVisible(!m_navHidden);

    auto addLink = [this](const QString& text, std::function<void()> onClick) {
        auto* b = new QToolButton(m_crumbsRow);
        b->setObjectName(QStringLiteral("refCrumbLink"));
        b->setText(text);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QToolButton::clicked, this, [onClick]() { onClick(); });
        m_crumbsLay->addWidget(b);
    };
    auto addSep = [this]() {
        auto* s = new QLabel(QString::fromUtf8("›"), m_crumbsRow);
        s->setObjectName(QStringLiteral("refCrumbSep"));
        m_crumbsLay->addWidget(s);
    };
    auto addHere = [this](const QString& text) {
        auto* l = new QLabel(text, m_crumbsRow);
        l->setObjectName(QStringLiteral("refCrumbHere"));
        m_crumbsLay->addWidget(l);
    };

    addLink(tr("Tudo"), [this]() { goHome(); });

    if (searching) {
        addSep();
        addHere(tr("Resultados"));
    } else {
        const QString sec = (m_navMode == NavMode::Home) ? QString() : currentSectionLabel();
        if (!sec.isEmpty()) {
            addSep();
            if (hasDoc) {
                const SourceKind kind = m_sourceKind;
                const QString key = m_currentDrawerKey;
                addLink(sec, [this, kind, key]() { enterSection(kind, key); });
            } else {
                addHere(sec);
            }
        }
        if (hasDoc) {
            KeyInfo info;
            if (describeKey(m_selectedKey, &info)) {
                addSep();
                addHere(info.name);
            }
        }
    }

    m_crumbsLay->addStretch();

    // Modo visual: so numa gaveta com cards de foto.
    bool visualDrawer = false;
    if (m_sourceKind == SourceKind::Drawer && m_model) {
        for (const Drawer& d : m_model->drawers()) {
            if (d.key == m_currentDrawerKey) { visualDrawer = drawerIsVisual(&d); break; }
        }
    }
    if (m_viewModeBtn) {
        m_crumbsLay->addWidget(m_viewModeBtn);
        m_viewModeBtn->setVisible(inSection && m_sourceKind == SourceKind::Drawer && visualDrawer);
        m_viewModeBtn->setText(m_visualMode ? QStringLiteral("≡") : QStringLiteral("⊞"));
        m_viewModeBtn->setToolTip(m_visualMode ? tr("Modo lista") : tr("Modo visual"));
    }
    if (m_territorioFilterBtn) {
        m_crumbsLay->addWidget(m_territorioFilterBtn);
        m_territorioFilterBtn->setVisible(inSection && m_sourceKind == SourceKind::Drawer
            && m_territorioStore && !m_territorioStore->territorios().isEmpty());
    }
}

QString RefMenuPanel::currentSectionLabel() const
{
    switch (m_sourceKind) {
    case SourceKind::Manuscript: {
        if (m_model) {
            for (const auto& ms : m_model->manuscripts()) {
                if (ms.id == m_currentManuscriptId && !ms.title.isEmpty()) return ms.title;
            }
        }
        return tr("Manuscrito");
    }
    case SourceKind::WorldExplorer:        return tr("Mundos");
    case SourceKind::TimelinesPlaceholder: return tr("Timelines");
    case SourceKind::MarkersPlaceholder:   return tr("Grupos");
    case SourceKind::Drawer: {
        if (m_currentDrawerKey == QStringLiteral("__groups__")) return tr("Grupos");
        if (m_model) {
            for (const Drawer& d : m_model->drawers()) {
                if (d.key == m_currentDrawerKey && !d.title.isEmpty()) return d.title;
            }
        }
        return tr("Gaveta");
    }
    }
    return QString();
}

void RefMenuPanel::goHome()
{
    m_navMode = NavMode::Home;
    m_currentGroupId.clear();
    m_currentFolderId.clear();
    m_currentConstrutorSystemId.clear();
    m_currentLugarTerritorioId.clear();
    changeSelectedKey(QString());
    if (m_searchInput && !m_searchInput->text().isEmpty()) {
        QSignalBlocker b(m_searchInput);
        m_searchInput->clear();
        m_searchQuery.clear();
    }
    rebuildNavBody();
    rebuildPreview();
    rebuildCrumbs();
}

void RefMenuPanel::enterSection(SourceKind kind, const QString& key)
{
    m_navMode = NavMode::Section;
    m_sourceKind = kind;
    if (kind == SourceKind::Drawer) m_currentDrawerKey = key;
    else if (kind == SourceKind::Manuscript && !key.isEmpty()) m_currentManuscriptId = key;
    m_currentGroupId.clear();
    m_currentFolderId.clear();
    m_currentConstrutorSystemId.clear();
    m_currentLugarTerritorioId.clear();
    changeSelectedKey(QString());
    rebuildNavBody();
    rebuildPreview();
    rebuildCrumbs();
}

void RefMenuPanel::rebuildTerritorioFilterMenu()
{
    if (!m_territorioFilterMenu || !m_territorioStore) return;
    m_territorioFilterMenu->clear();

    QAction* all = m_territorioFilterMenu->addAction(tr("Todos"));
    connect(all, &QAction::triggered, this, [this]() {
        m_territorioFilterId.clear();
        if (m_territorioFilterBtn) m_territorioFilterBtn->setText(tr("Território: Todos"));
        rebuildNavBody();
    });
    m_territorioFilterMenu->addSeparator();

    QList<TerritorioStore::Territorio> territorios = m_territorioStore->territorios();
    std::sort(territorios.begin(), territorios.end(),
              [](const TerritorioStore::Territorio& a, const TerritorioStore::Territorio& b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    for (const auto& t : territorios) {
        const QString name = t.name.isEmpty() ? tr("(sem nome)") : t.name;
        QAction* a = m_territorioFilterMenu->addAction(name);
        a->setCheckable(true);
        a->setChecked(t.id == m_territorioFilterId);
        const QString id = t.id;
        connect(a, &QAction::triggered, this, [this, id, name]() {
            m_territorioFilterId = id;
            if (m_territorioFilterBtn) m_territorioFilterBtn->setText(tr("Território: %1").arg(name));
            rebuildNavBody();
        });
    }
}

void RefMenuPanel::rebuildNavBody()
{
    if (!m_navInner || !m_navInnerLay) return;
    // Limpa filhos do layout (mantendo o stretch no final).
    while (m_navInnerLay->count() > 0) {
        QLayoutItem* item = m_navInnerLay->takeAt(0);
        if (auto* w = item->widget()) w->deleteLater();
        delete item;
    }

    // Com busca ativa, varre tudo (manuscritos, capítulos+cenas, gavetas)
    // independente do source/drawer aberto. Sem busca, segue o source normal.
    if (!m_searchQuery.isEmpty()) {
        buildSearchAllView();
        m_navInnerLay->addStretch();
        return;
    }

    // Tela inicial: o que você provavelmente quer, antes de qualquer navegação.
    if (m_navMode == NavMode::Home) {
        buildHomeView();
        m_navInnerLay->addStretch();
        return;
    }

    if (m_sourceKind == SourceKind::Manuscript) {
        buildManuscriptsView();
    } else if (m_sourceKind == SourceKind::Drawer) {
        buildDrawerView();
    } else if (m_sourceKind == SourceKind::MarkersPlaceholder) {
        buildGroupsView();
    } else if (m_sourceKind == SourceKind::TimelinesPlaceholder) {
        buildPlaceholderView(tr("Timelines"), tr("Em breve. Vai listar as linhas do tempo."));
    } else if (m_sourceKind == SourceKind::WorldExplorer) {
        buildWorldExplorerView();
    }

    m_navInnerLay->addStretch();
}

const ConstrutorStore::Node* RefMenuPanel::findConstrutorNode(
    const ConstrutorStore::System* sys, const QString& nodeId) const
{
    if (!sys) return nullptr;
    std::function<const ConstrutorStore::Node*(const QList<ConstrutorStore::Node>&)> find;
    find = [&](const QList<ConstrutorStore::Node>& nodes) -> const ConstrutorStore::Node* {
        for (const auto& n : nodes) {
            if (n.id == nodeId) return &n;
            if (const auto* c = find(n.children)) return c;
        }
        return nullptr;
    };
    return find(sys->nodes);
}

const TerritorioStore::Node* RefMenuPanel::findTerritorioNode(
    const TerritorioStore::Territorio* ter, const QString& nodeId) const
{
    if (!ter) return nullptr;
    std::function<const TerritorioStore::Node*(const QList<TerritorioStore::Node>&)> find;
    find = [&](const QList<TerritorioStore::Node>& nodes) -> const TerritorioStore::Node* {
        for (const auto& n : nodes) {
            if (n.id == nodeId) return &n;
            if (const auto* c = find(n.children)) return c;
        }
        return nullptr;
    };
    return find(ter->nodes);
}

static QString refListStyleSheet()
{
    return Theme::qss(QStringLiteral(R"(
        QListWidget {
            background: %1; color: %2;
            border: 1px solid %3; border-radius: @radius-control;
            outline: none; padding: 4px;
        }
        QListWidget::item { padding: 6px 8px; border-radius: @radius-item; }
        QListWidget::item:hover { background: %4; color: %5; }
        QListWidget::item:selected { background: %6; color: %5; }
    )")).arg(Theme::appBackground(),
           Theme::textPrimary(),
           Theme::panelBorder(),
           Theme::hoverOverlay(),
           Theme::textBright(),
           Theme::accentInfoSoft());
}

// "Explorador de Mundos" — fusão de Território (Lugares) e Construtor
// (Sistemas) numa view só. Na raiz mostra as duas seções lado a lado; ao
// entrar num território/sistema, mostra só os nós daquele um (mesmo
// comportamento de antes, só realocado pra dentro de uma função só).
void RefMenuPanel::buildWorldExplorerView()
{
    if (!m_navInner || !m_navInnerLay) return;
    const bool hasTerritorios = m_territorioStore && !m_territorioStore->territorios().isEmpty();
    const bool hasSistemas = m_construtorStore && !m_construtorStore->systems().isEmpty();
    if (!hasTerritorios && !hasSistemas) {
        buildPlaceholderView(tr("Explorador de Mundos"),
            tr("Nenhum território ou sistema criado ainda. Use o botão do Construtor na barra superior pra abrir o Criador de Mundos."));
        return;
    }

    const ConstrutorStore::System* openSys = m_currentConstrutorSystemId.isEmpty()
        ? nullptr : m_construtorStore->system(m_currentConstrutorSystemId);
    if (!openSys) m_currentConstrutorSystemId.clear();
    const TerritorioStore::Territorio* openTer = m_currentLugarTerritorioId.isEmpty()
        ? nullptr : m_territorioStore->territorio(m_currentLugarTerritorioId);
    if (!openTer) m_currentLugarTerritorioId.clear();

    auto sectionTitle = [this](const QString& text) {
        auto* lab = new QLabel(text, m_navInner);
        lab->setStyleSheet(QStringLiteral("color:%1; font-size:10px; font-weight:700; letter-spacing:1px; padding:6px 6px 2px;")
                               .arg(Theme::textMuted()));
        m_navInnerLay->addWidget(lab);
    };
    auto makeList = [this]() {
        auto* lw = new QListWidget(m_navInner);
        lw->setSelectionMode(QAbstractItemView::SingleSelection);
        lw->setFrameShape(QFrame::NoFrame);
        lw->setIconSize(QSize(22, 22));
        lw->setStyleSheet(refListStyleSheet());
        return lw;
    };
    auto backRow = [this](const QPixmap& avatar, const QString& name, std::function<void()> onBack) {
        auto* row = new QWidget(m_navInner);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 4);
        rowLay->setSpacing(6);
        auto* back = new QToolButton(row);
        back->setObjectName(QStringLiteral("refTinyBtn"));
        back->setText(QStringLiteral("←"));
        back->setCursor(Qt::PointingHandCursor);
        connect(back, &QToolButton::clicked, this, [onBack]() { onBack(); });
        rowLay->addWidget(back);
        if (!avatar.isNull()) {
            auto* av = new QLabel(row);
            av->setPixmap(avatar);
            av->setFixedSize(avatar.size());
            rowLay->addWidget(av);
        }
        auto* lab = new QLabel(name, row);
        lab->setStyleSheet(QStringLiteral("color:%1; font-size:12px; font-weight:600;").arg(Theme::textPrimary()));
        rowLay->addWidget(lab);
        rowLay->addStretch();
        m_navInnerLay->addWidget(row);
    };

    if (openSys) {
        backRow(QPixmap(), openSys->name, [this]() {
            m_currentConstrutorSystemId.clear();
            rebuildNavBody();
        });

        auto* list = makeList();
        std::function<void(const QString&, const QList<ConstrutorStore::Node>&)> walk;
        walk = [&](const QString& breadcrumb, const QList<ConstrutorStore::Node>& nodes) {
            for (const auto& n : nodes) {
                const QString path = breadcrumb.isEmpty() ? n.name : breadcrumb + QStringLiteral(" ▸ ") + n.name;
                if (matchesSearch(path)) {
                    const QString key = QStringLiteral("ctr:%1:%2").arg(openSys->id, n.id);
                    auto* li = new QListWidgetItem(path);
                    li->setData(Qt::UserRole, key);
                    list->addItem(li);
                    if (key == m_selectedKey) li->setSelected(true);
                }
                walk(path, n.children);
            }
        };
        walk(QString(), openSys->nodes);
        connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            setSelected(it->data(Qt::UserRole).toString());
        });

        if (list->count() == 0) {
            delete list;
            auto* empty = new QLabel(tr("Este sistema ainda não tem regras/seções."), m_navInner);
            empty->setStyleSheet(QStringLiteral("color:%1; font-size:11px; padding:8px;").arg(Theme::textMuted()));
            empty->setAlignment(Qt::AlignCenter);
            m_navInnerLay->addWidget(empty);
            return;
        }
        m_navInnerLay->addWidget(list);
        return;
    }

    if (openTer) {
        const QPixmap avatar = AvatarUtils::circularAvatar(openTer->avatarDataUrl, openTer->name, openTer->id, 22);
        backRow(avatar, openTer->name, [this]() {
            m_currentLugarTerritorioId.clear();
            rebuildNavBody();
        });

        auto* list = makeList();
        std::function<void(const QString&, const QList<TerritorioStore::Node>&)> walk;
        walk = [&](const QString& breadcrumb, const QList<TerritorioStore::Node>& nodes) {
            for (const auto& n : nodes) {
                const QString path = breadcrumb.isEmpty() ? n.name : breadcrumb + QStringLiteral(" ▸ ") + n.name;
                if (matchesSearch(path)) {
                    const QString key = QStringLiteral("lug:%1:%2").arg(openTer->id, n.id);
                    auto* li = new QListWidgetItem(path);
                    li->setData(Qt::UserRole, key);
                    list->addItem(li);
                    if (key == m_selectedKey) li->setSelected(true);
                }
                walk(path, n.children);
            }
        };
        walk(QString(), openTer->nodes);
        connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            setSelected(it->data(Qt::UserRole).toString());
        });

        if (list->count() == 0) {
            delete list;
            auto* empty = new QLabel(tr("Este território ainda não tem pastas/documentos."), m_navInner);
            empty->setStyleSheet(QStringLiteral("color:%1; font-size:11px; padding:8px;").arg(Theme::textMuted()));
            empty->setAlignment(Qt::AlignCenter);
            m_navInnerLay->addWidget(empty);
            return;
        }
        m_navInnerLay->addWidget(list);
        return;
    }

    // Raiz: territórios (avatar real, mesmo AvatarUtils::circularAvatar da
    // StatsPanel) e sistemas (badge de vínculo), como cards — mesmo padrão
    // visual de "refVisualCard" já usado no modo visual da Drawer
    // (buildDrawerView), não a lista de texto simples de antes.
    std::function<int(const QList<TerritorioStore::Node>&)> countTerNodes;
    countTerNodes = [&](const QList<TerritorioStore::Node>& nodes) -> int {
        int n = nodes.size();
        for (const auto& node : nodes) n += countTerNodes(node.children);
        return n;
    };
    std::function<int(const QList<ConstrutorStore::Node>&)> countSysNodes;
    countSysNodes = [&](const QList<ConstrutorStore::Node>& nodes) -> int {
        int n = nodes.size();
        for (const auto& node : nodes) n += countSysNodes(node.children);
        return n;
    };

    auto makeCard = [this](const QPixmap& avatar, const QString& title, const QString& meta,
                            const QStringList& badges, std::function<void()> onClick) {
        auto* card = new QToolButton(m_navInner);
        card->setObjectName(QStringLiteral("refVisualCard"));
        card->setCursor(Qt::PointingHandCursor);
        card->setMinimumHeight(58);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        card->setStyleSheet(Theme::qss(QStringLiteral(R"(
            QToolButton#refVisualCard {
                background: %1;
                border: 1px solid %2;
                border-radius: @radius-control;
                text-align: left;
                padding: 0;
            }
            QToolButton#refVisualCard:hover { background: %3; border-color: %4; }
        )")).arg(Theme::appBackground(), Theme::panelBorder(), Theme::hoverOverlay(), Theme::borderStrong()));

        auto* inner = new QWidget(card);
        inner->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* inLay = new QHBoxLayout(inner);
        inLay->setContentsMargins(10, 8, 10, 8);
        inLay->setSpacing(10);

        auto* avLabel = new QLabel(inner);
        avLabel->setFixedSize(32, 32);
        avLabel->setPixmap(avatar);
        inLay->addWidget(avLabel);

        auto* info = new QWidget(inner);
        auto* infoLay = new QVBoxLayout(info);
        infoLay->setContentsMargins(0, 0, 0, 0);
        infoLay->setSpacing(3);
        auto* nameLab = new QLabel(title, info);
        nameLab->setStyleSheet(QStringLiteral("color:%1; font-size:13px; font-weight:600;").arg(Theme::textBright()));
        infoLay->addWidget(nameLab);
        if (!meta.isEmpty()) {
            auto* metaLab = new QLabel(meta, info);
            metaLab->setStyleSheet(QStringLiteral("color:%1; font-size:11px;").arg(Theme::textMuted()));
            infoLay->addWidget(metaLab);
        }
        if (!badges.isEmpty()) {
            auto* badgeRow = new QWidget(info);
            auto* badgeLay = new QHBoxLayout(badgeRow);
            badgeLay->setContentsMargins(0, 2, 0, 0);
            badgeLay->setSpacing(4);
            for (const auto& b : badges) {
                auto* pill = new QLabel(QStringLiteral(" 🔗 %1 ").arg(b), badgeRow);
                pill->setStyleSheet(Theme::qss(QStringLiteral(
                    "background:%1; color:%2; border-radius: @radius-control; padding:1px 6px; font-size:10px; font-weight:600;")
                    .arg(Theme::hoverOverlay(), Theme::accentDefault())));
                badgeLay->addWidget(pill);
            }
            badgeLay->addStretch();
            infoLay->addWidget(badgeRow);
        }
        inLay->addWidget(info, /*stretch=*/1);

        auto* chev = new QLabel(QStringLiteral("›"), inner);
        chev->setStyleSheet(QStringLiteral("color:%1; font-size:18px;").arg(Theme::textMuted()));
        inLay->addWidget(chev);

        auto* cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(0, 0, 0, 0);
        cardLay->addWidget(inner);

        connect(card, &QToolButton::clicked, this, [onClick]() { onClick(); });
        return card;
    };

    bool anyShown = false;

    if (hasTerritorios) {
        QList<TerritorioStore::Territorio> hits;
        for (const auto& t : m_territorioStore->territorios())
            if (matchesSearch(t.name)) hits.append(t);
        if (!hits.isEmpty()) {
            anyShown = true;
            sectionTitle(tr("TERRITÓRIOS"));
            auto* col = new QWidget(m_navInner);
            auto* colLay = new QVBoxLayout(col);
            colLay->setContentsMargins(0, 0, 0, 0);
            colLay->setSpacing(6);
            for (const auto& t : hits) {
                int linkCount = 0;
                for (const auto& l : m_territorioStore->links())
                    if (l.fromTerritorioId == t.id || l.toTerritorioId == t.id) ++linkCount;
                QString meta = tr("%n documento(s)", nullptr, countTerNodes(t.nodes));
                if (linkCount > 0) meta += tr(" · %n vínculo(s)", nullptr, linkCount);
                const QPixmap avatar = AvatarUtils::circularAvatar(t.avatarDataUrl, t.name, t.id, 32);
                const QString territorioId = t.id;
                colLay->addWidget(makeCard(avatar, t.name, meta, {}, [this, territorioId]() {
                    m_currentConstrutorSystemId.clear();
                    m_currentLugarTerritorioId = territorioId;
                    setSelected(QStringLiteral("lug:%1").arg(territorioId));
                    rebuildNavBody();
                }));
            }
            m_navInnerLay->addWidget(col);
        }
    }

    if (hasSistemas) {
        QList<ConstrutorStore::System> hits;
        for (const auto& s : m_construtorStore->systems())
            if (matchesSearch(s.name)) hits.append(s);
        if (!hits.isEmpty()) {
            anyShown = true;
            sectionTitle(tr("SISTEMAS"));
            const QIcon icSys = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/cube.svg"),
                QColor(Theme::accentDefault()), QColor(Theme::accentDefault()), QColor(Theme::textBright()),
                QSize(16, 16));
            QPixmap sysAvatar(32, 32);
            sysAvatar.fill(Qt::transparent);
            {
                QPainter p(&sysAvatar);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setBrush(QColor(58, 140, 122, 40));
                p.setPen(Qt::NoPen);
                p.drawEllipse(0, 0, 32, 32);
                if (!icSys.isNull()) icSys.paint(&p, 8, 8, 16, 16);
            }
            auto* col = new QWidget(m_navInner);
            auto* colLay = new QVBoxLayout(col);
            colLay->setContentsMargins(0, 0, 0, 0);
            colLay->setSpacing(6);
            for (const auto& s : hits) {
                QString meta;
                if (const auto* cat = ConstrutorStore::categoryById(s.categoryId)) meta = cat->displayName;
                const int nodeCount = countSysNodes(s.nodes);
                if (nodeCount > 0) {
                    if (!meta.isEmpty()) meta += QStringLiteral(" · ");
                    meta += tr("%n regra(s)/seção(ões)", nullptr, nodeCount);
                }
                QStringList badges;
                if (m_territorioStore) {
                    for (const auto& tid : s.territoryIds)
                        if (const auto* t = m_territorioStore->territorio(tid)) badges << t->name;
                }
                const QString systemId = s.id;
                colLay->addWidget(makeCard(sysAvatar, s.name, meta, badges, [this, systemId]() {
                    m_currentLugarTerritorioId.clear();
                    m_currentConstrutorSystemId = systemId;
                    setSelected(QStringLiteral("ctr:%1").arg(systemId));
                    rebuildNavBody();
                }));
            }
            m_navInnerLay->addWidget(col);
        }
    }

    if (!anyShown) {
        auto* empty = new QLabel(tr("Nada encontrado."), m_navInner);
        empty->setStyleSheet(QStringLiteral("color:%1; font-size:11px; padding:8px;").arg(Theme::textMuted()));
        empty->setAlignment(Qt::AlignCenter);
        m_navInnerLay->addWidget(empty);
    }
}

void RefMenuPanel::buildGroupsView()
{
    if (!m_model || !m_navInner || !m_navInnerLay) return;

    // Limpa seleção de grupo se foi removido
    if (!m_currentGroupId.isEmpty() && !m_model->findGroup(m_currentGroupId))
        m_currentGroupId.clear();

    // ---- Seção: picker de grupo ----
    auto* grpTitle = new QLabel(tr("GRUPOS"), m_navInner);
    grpTitle->setStyleSheet(QStringLiteral(
        "color:%1; font-size:10px; font-weight:700; letter-spacing:1px; padding:6px 6px 2px;")
        .arg(Theme::textMuted()));
    m_navInnerLay->addWidget(grpTitle);

    const auto& groups = m_model->groups();

    if (groups.isEmpty()) {
        auto* hint = new QLabel(
            tr("Nenhum grupo criado. Clique com o botão direito num documento de gaveta e escolha \"Adicionar ao grupo\"."),
            m_navInner);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral(
            "color:%1; font-size:11px; padding:8px 6px; font-style:italic;").arg(Theme::textMuted()));
        m_navInnerLay->addWidget(hint);
        return;
    }

    auto* grpList = new QListWidget(m_navInner);
    grpList->setObjectName(QStringLiteral("refListGrp"));
    grpList->setSelectionMode(QAbstractItemView::SingleSelection);
    grpList->setFrameShape(QFrame::NoFrame);
    grpList->setUniformItemSizes(true);
    grpList->setStyleSheet(Theme::qss(QStringLiteral(R"(
        QListWidget {
            background: %1; color: %2;
            border: 1px solid %3; border-radius: @radius-control;
            outline: none; padding: 4px;
        }
        QListWidget::item { padding: 6px 8px; border-radius: @radius-item; }
        QListWidget::item:hover    { background: %4; color: %5; }
        QListWidget::item:selected { background: %6; color: %5; }
    )")).arg(Theme::appBackground(), Theme::textPrimary(), Theme::panelBorder(),
            Theme::hoverOverlay(), Theme::textBright(), Theme::accentInfoSoft()));

    for (const auto& g : groups) {
        QPixmap dot(10, 10);
        dot.fill(Qt::transparent);
        QPainter dotP(&dot);
        dotP.setRenderHint(QPainter::Antialiasing);
        dotP.setBrush(QColor(g.color));
        dotP.setPen(Qt::NoPen);
        dotP.drawEllipse(1, 1, 8, 8);
        dotP.end();

        auto* item = new QListWidgetItem(g.title);
        item->setIcon(QIcon(dot));
        item->setData(Qt::UserRole, g.id);
        grpList->addItem(item);
        if (g.id == m_currentGroupId)
            item->setSelected(true);
    }
    grpList->setFixedHeight(qMin(140, 6 + 30 * qMax(1, groups.size()) + 4));

    connect(grpList, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        if (!it) return;
        const QString id = it->data(Qt::UserRole).toString();
        m_currentGroupId = (m_currentGroupId == id) ? QString() : id;
        changeSelectedKey(QString());
        rebuildNavBody();
        rebuildPreview();
    });
    m_navInnerLay->addWidget(grpList);

    if (m_currentGroupId.isEmpty()) return;

    // ---- Seção: itens do grupo ----
    auto* itemsTitle = new QLabel(tr("DOCUMENTOS NO GRUPO"), m_navInner);
    itemsTitle->setStyleSheet(QStringLiteral(
        "color:%1; font-size:10px; font-weight:700; letter-spacing:1px; padding:10px 6px 2px;")
        .arg(Theme::textMuted()));
    m_navInnerLay->addWidget(itemsTitle);

    auto* itemList = new QListWidget(m_navInner);
    itemList->setObjectName(QStringLiteral("refListGrpItems"));
    itemList->setSelectionMode(QAbstractItemView::SingleSelection);
    itemList->setFrameShape(QFrame::NoFrame);
    itemList->setStyleSheet(Theme::qss(QStringLiteral(R"(
        QListWidget {
            background: %1; color: %2;
            border: 1px solid %3; border-radius: @radius-control;
            outline: none; padding: 4px;
        }
        QListWidget::item { padding: 6px 8px; border-radius: @radius-item; }
        QListWidget::item:hover    { background: %4; color: %5; }
        QListWidget::item:selected { background: %6; color: %5; }
    )")).arg(Theme::appBackground(), Theme::textPrimary(), Theme::panelBorder(),
            Theme::hoverOverlay(), Theme::textBright(), Theme::accentInfoSoft()));

    int shown = 0;
    for (const auto& drawer : m_model->drawers()) {
        for (const auto& di : drawer.items) {
            if (di.markerId != m_currentGroupId) continue;
            if (!m_searchQuery.isEmpty() && !matchesSearch(di.title)) continue;
            const QString label = di.title.isEmpty() ? tr("(sem nome)") : di.title;
            const QString full  = QStringLiteral("%1  —  %2")
                .arg(label, drawer.title.isEmpty() ? tr("gaveta") : drawer.title);
            auto* it = new QListWidgetItem(full);
            it->setData(Qt::UserRole, QStringLiteral("it:%1").arg(di.id));
            if (QStringLiteral("it:%1").arg(di.id) == m_selectedKey)
                it->setSelected(true);
            itemList->addItem(it);
            ++shown;
        }
    }

    if (shown == 0) {
        auto* noItems = new QLabel(tr("Nenhum documento neste grupo"), m_navInner);
        noItems->setStyleSheet(QStringLiteral(
            "color:%1; font-size:11px; padding:6px; font-style:italic;").arg(Theme::textMuted()));
        m_navInnerLay->addWidget(noItems);
    } else {
        itemList->setFixedHeight(qMin(200, 6 + 30 * shown + 4));
        connect(itemList, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            const QString key = it->data(Qt::UserRole).toString();
            setSelected(key);
            rebuildPreview();
        });
        m_navInnerLay->addWidget(itemList);
    }
}

namespace {

// Carrega miniatura a partir de data URL base64 (Element::image) ou de um
// caminho local. Devolve pixmap nulo quando não há imagem — a linha então cai
// na bolinha discreta, mantendo o texto alinhado com as linhas que têm foto.
QPixmap refThumbFrom(const QString& src)
{
    if (src.isEmpty()) return QPixmap();
    QPixmap pm;
    const int comma = src.indexOf(QLatin1Char(','));
    if (src.startsWith(QLatin1String("data:")) && comma > 0) {
        const QByteArray raw = QByteArray::fromBase64(src.mid(comma + 1).toLatin1());
        pm.loadFromData(raw);
    } else {
        pm.load(src);
    }
    return pm;
}

QString refPinsBase(const QString& projectRoot)
{
    return QStringLiteral("ui/refMenuPanel/%1/").arg(QString::fromLatin1(
        QCryptographicHash::hash(QDir::cleanPath(projectRoot).toUtf8(),
                                 QCryptographicHash::Sha1).toHex()));
}

} // namespace

void RefMenuPanel::loadPinsAndRecents()
{
    if (m_projectRoot.isEmpty()) return;
    QSettings s;
    const QString base = refPinsBase(m_projectRoot);
    m_pinnedKeys = s.value(base + QStringLiteral("pinned")).toStringList();
    m_recentKeys = s.value(base + QStringLiteral("recent")).toStringList();
}

void RefMenuPanel::savePinsAndRecents() const
{
    if (m_projectRoot.isEmpty()) return;
    QSettings s;
    const QString base = refPinsBase(m_projectRoot);
    s.setValue(base + QStringLiteral("pinned"), m_pinnedKeys);
    s.setValue(base + QStringLiteral("recent"), m_recentKeys);
}

bool RefMenuPanel::isPinned(const QString& key) const
{
    return m_pinnedKeys.contains(key);
}

void RefMenuPanel::togglePin(const QString& key)
{
    if (key.isEmpty()) return;
    if (m_pinnedKeys.removeAll(key) == 0) m_pinnedKeys.prepend(key);
    savePinsAndRecents();
    if (m_navMode == NavMode::Home && m_searchQuery.isEmpty()) rebuildNavBody();
}

void RefMenuPanel::addRecent(const QString& key)
{
    if (key.isEmpty()) return;
    m_recentKeys.removeAll(key);
    m_recentKeys.prepend(key);
    // Oito cobre uma sessão de escrita sem virar uma segunda lista de tudo.
    while (m_recentKeys.size() > 8) m_recentKeys.removeLast();
    savePinsAndRecents();
}

void RefMenuPanel::setCurrentDocKey(const QString& docKey)
{
    if (m_currentDocKey == docKey) return;
    m_currentDocKey = docKey;
    // Só redesenha se "Nesta cena" estiver à vista.
    if (isVisible() && m_navMode == NavMode::Home && m_searchQuery.isEmpty()) rebuildNavBody();
}

QString RefMenuPanel::editorDocKey() const
{
    // O painel já tem o EditorHost, então descobre sozinho qual documento está
    // aberto — não depende do MainWindow avisar a cada troca de cena. Mesma
    // chave que o ElementsStore usa pra guardar quem está presente.
    if (!m_host || !m_model) return QString();
    const auto vm = m_host->viewMode();
    if (vm.type == EditorHost::SceneDoc) {
        const Chapter* ch = m_model->findChapter(vm.chapterId);
        if (!ch || vm.sceneIndex < 0 || vm.sceneIndex >= ch->scenes.size()) return QString();
        return ElementsStore::elementDocKeyForScene(vm.manuscriptId, vm.chapterId,
                                                    ch->scenes[vm.sceneIndex].id);
    }
    if (vm.type == EditorHost::ChapterDoc) {
        return ElementsStore::elementDocKeyForChapter(vm.manuscriptId, vm.chapterId);
    }
    return QString();
}

bool RefMenuPanel::describeKey(const QString& key, KeyInfo* out) const
{
    if (key.isEmpty() || !out || !m_model) return false;
    const QStringList p = key.split(QLatin1Char(':'));

    if (key.startsWith(QLatin1String("it:")) && p.size() >= 2) {
        for (const Drawer& d : m_model->drawers()) {
            for (const DrawerItem& it : d.items) {
                if (it.id != p.at(1)) continue;
                out->name = it.title.isEmpty() ? tr("(sem título)") : it.title;
                out->sectionLabel = d.title;
                out->meta = d.title;
                out->role = roleOrLabelForItem(it);
                out->imagePath = imageForItem(it);
                return true;
            }
        }
        return false;
    }

    if (key.startsWith(QLatin1String("ch:")) && p.size() >= 3) {
        for (const Chapter& c : m_model->chapters()) {
            if (c.id != p.at(2)) continue;
            out->name = c.title.isEmpty() ? tr("(sem título)") : c.title;
            out->sectionLabel = tr("Manuscrito");
            out->meta = tr("capítulo");
            return true;
        }
        return false;
    }

    if (key.startsWith(QLatin1String("sc:")) && p.size() >= 4) {
        for (const Chapter& c : m_model->chapters()) {
            if (c.id != p.at(2)) continue;
            const int idx = p.at(3).toInt();
            if (idx < 0 || idx >= c.scenes.size()) return false;
            const QString st = c.scenes.at(idx).title;
            out->name = st.isEmpty() ? tr("Cena %1").arg(idx + 1) : st;
            out->sectionLabel = tr("Manuscrito");
            out->meta = c.title.isEmpty() ? tr("capítulo") : c.title;
            return true;
        }
        return false;
    }

    if (key.startsWith(QLatin1String("ctr:")) && m_construtorStore && p.size() >= 2) {
        for (const auto& sys : m_construtorStore->systems()) {
            if (sys.id != p.at(1)) continue;
            if (p.size() >= 3) {
                const ConstrutorStore::Node* n = findConstrutorNode(&sys, p.at(2));
                if (!n) return false;
                out->name = n->name;
                out->meta = sys.name;
            } else {
                out->name = sys.name;
                out->meta = tr("sistema");
            }
            out->sectionLabel = tr("Mundos");
            return true;
        }
        return false;
    }

    if (key.startsWith(QLatin1String("lug:")) && m_territorioStore && p.size() >= 2) {
        for (const auto& ter : m_territorioStore->territorios()) {
            if (ter.id != p.at(1)) continue;
            if (p.size() >= 3) {
                const TerritorioStore::Node* n = findTerritorioNode(&ter, p.at(2));
                if (!n) return false;
                out->name = n->name;
                out->meta = ter.name;
            } else {
                out->name = ter.name.isEmpty() ? tr("(sem nome)") : ter.name;
                out->meta = tr("território");
            }
            out->sectionLabel = tr("Mundos");
            return true;
        }
        return false;
    }

    return false;
}

QWidget* RefMenuPanel::makeNavRow(const QString& key, const KeyInfo& info, bool withPin,
                                  int fontPx)
{
    auto* row = new QWidget(m_navInner);
    row->setObjectName(QStringLiteral("refNavRow"));
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setCursor(Qt::PointingHandCursor);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(6, 5, 6, 5);
    lay->setSpacing(9);

    const QPixmap pm = refThumbFrom(info.imagePath);
    auto* thumb = new QLabel(row);
    thumb->setFixedSize(26, 26);
    if (!pm.isNull()) {
        thumb->setPixmap(pm.scaled(26, 26, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        thumb->setStyleSheet(Theme::qss(QStringLiteral("border-radius: @radius-item;")));
    } else {
        thumb->setAlignment(Qt::AlignCenter);
        thumb->setText(QStringLiteral("•"));
        thumb->setStyleSheet(QStringLiteral("color:%1; font-size:16px;").arg(Theme::textMuted()));
    }
    lay->addWidget(thumb);

    auto* textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(1);
    auto* name = new QLabel(info.name, row);
    name->setStyleSheet(QStringLiteral("color:%1; font-size:%2px;")
        .arg(Theme::textPrimary()).arg(fontPx));
    textCol->addWidget(name);
    // O papel (PROTAGONISTA etc.) fica sob o nome, como nos cards da gaveta.
    const QString sub = !info.role.isEmpty() ? info.role : info.meta;
    if (!sub.isEmpty()) {
        auto* lbl = new QLabel(sub, row);
        if (info.role.isEmpty()) {
            lbl->setStyleSheet(QStringLiteral("color:%1; font-size:10px;").arg(Theme::textMuted()));
        } else {
            lbl->setStyleSheet(QStringLiteral(
                "color:%1; font-size:10px; font-weight:700; letter-spacing:.5px;")
                .arg(Theme::accentInfo()));
        }
        textCol->addWidget(lbl);
    }
    lay->addLayout(textCol, 1);

    if (withPin) {
        auto* pin = new QToolButton(row);
        pin->setObjectName(QStringLiteral("refPinBtn"));
        const bool on = isPinned(key);
        pin->setText(on ? QStringLiteral("★") : QStringLiteral("☆"));
        pin->setToolTip(on ? tr("Desafixar") : tr("Fixar no topo"));
        pin->setCursor(Qt::PointingHandCursor);
        pin->setStyleSheet(QStringLiteral(
            "QToolButton{border:0;background:transparent;color:%1;font-size:13px;padding:2px 4px;}")
            .arg(on ? Theme::accentWarning() : Theme::textMuted()));
        connect(pin, &QToolButton::clicked, this, [this, key]() { togglePin(key); });
        lay->addWidget(pin);
    }

    row->setStyleSheet(Theme::qss(QStringLiteral(
        "QWidget#refNavRow { border-radius: @radius-item; }"
        "QWidget#refNavRow:hover { background: %1; }")).arg(Theme::hoverOverlay()));

    row->setProperty("refRowKey", key);
    row->installEventFilter(this);
    return row;
}

void RefMenuPanel::buildHomeView()
{
    if (!m_navInner || !m_navInnerLay) return;

    auto sectionTitle = [this](const QString& text) {
        auto* l = new QLabel(text, m_navInner);
        l->setStyleSheet(QStringLiteral(
            "color:%1; font-size:10px; font-weight:700; letter-spacing:1.2px; padding:10px 6px 2px;")
            .arg(Theme::textMuted()));
        m_navInnerLay->addWidget(l);
    };

    // ---- Nesta cena ----
    // Leitura pura do ElementsStore, que já mantém quem está presente em cada
    // documento. Nada é recalculado aqui.
    const QString sceneKey = m_currentDocKey.isEmpty() ? editorDocKey() : m_currentDocKey;
    if (m_elements && !sceneKey.isEmpty()) {
        const QStringList ids = m_elements->docElementIds(sceneKey);
        QList<Element> present;
        for (const QString& id : ids) {
            for (const Element& e : m_elements->elements()) {
                if (e.id == id) { present.append(e); break; }
            }
        }
        if (!present.isEmpty()) {
            sectionTitle(tr("NESTA CENA"));
            for (const Element& e : present) {
                // A ficha do elemento é um item de gaveta que aponta pra ele
                // (DrawerItem::elementId). Sem ficha, a linha ainda aparece —
                // saber quem está na cena já vale — só não abre nada.
                QString key;
                if (m_model) {
                    for (const Drawer& d : m_model->drawers()) {
                        for (const DrawerItem& it : d.items) {
                            if (it.elementId == e.id) { key = QStringLiteral("it:%1").arg(it.id); break; }
                        }
                        if (!key.isEmpty()) break;
                    }
                }
                KeyInfo info;
                info.name = e.name;
                info.role = e.role;
                info.imagePath = e.image;
                m_navInnerLay->addWidget(makeNavRow(key, info, !key.isEmpty()));
            }
        }
    }

    // ---- Fixados ----
    if (!m_pinnedKeys.isEmpty()) {
        QStringList alive;
        QList<QWidget*> rows;
        for (const QString& k : m_pinnedKeys) {
            KeyInfo info;
            if (!describeKey(k, &info)) continue;   // item apagado: some sem alarde
            alive << k;
            rows << makeNavRow(k, info, true);
        }
        if (alive.size() != m_pinnedKeys.size()) {
            m_pinnedKeys = alive;
            savePinsAndRecents();
        }
        if (!rows.isEmpty()) {
            sectionTitle(tr("FIXADOS"));
            for (QWidget* w : rows) m_navInnerLay->addWidget(w);
        }
    }

    // ---- Recentes ----
    if (!m_recentKeys.isEmpty()) {
        QStringList alive;
        QList<QWidget*> rows;
        for (const QString& k : m_recentKeys) {
            KeyInfo info;
            if (!describeKey(k, &info)) continue;
            alive << k;
            if (m_pinnedKeys.contains(k)) continue;     // já apareceu em Fixados
            if (rows.size() < 4) rows << makeNavRow(k, info, true);
        }
        if (alive.size() != m_recentKeys.size()) {
            m_recentKeys = alive;
            savePinsAndRecents();
        }
        if (!rows.isEmpty()) {
            sectionTitle(tr("RECENTES"));
            for (QWidget* w : rows) m_navInnerLay->addWidget(w);
        }
    }

    // ---- Percorrer ----
    // As fontes viram lista visível, com contagem — antes estavam atrás de dois
    // menus suspensos que não diziam o que havia dentro. Clicar abre a fonte
    // ali mesmo, para baixo, em vez de trocar a tela inteira: o resto da
    // navegação (Nesta cena, Fixados, Recentes) continua à vista.
    sectionTitle(tr("PERCORRER"));

    // Linha de fonte/pasta: mesma estética das outras, mais uma seta que gira.
    // `depth` recua a linha pra mostrar o aninhamento.
    // thumbSrc: capa do manuscrito (data URL) ou imagem da gaveta.
    // iconId: icone do catalogo de gavetas (:/icons/elements/<id>.svg).
    auto addBranch = [this](const QString& label, const QString& meta,
                            const QString& expandKey, int depth,
                            const QString& thumbSrc = QString(),
                            const QString& iconId = QString(),
                            const QString& docKey = QString()) {
        const bool open = m_expanded.contains(expandKey);
        auto* row = new QWidget(m_navInner);
        row->setObjectName(QStringLiteral("refNavRow"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setCursor(Qt::PointingHandCursor);
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(6 + depth * 14, 7, 8, 7);
        lay->setSpacing(8);

        // Com docKey, a linha tem duas acoes: a seta abre/fecha os filhos e o
        // resto abre o documento inteiro. Sem isso, um capitulo com cenas so
        // poderia ser lido cena a cena — nunca do comeco ao fim.
        if (!docKey.isEmpty()) {
            auto* chev = new QToolButton(row);
            chev->setObjectName(QStringLiteral("refChevBtn"));
            chev->setText(open ? QString::fromUtf8("⌄") : QString::fromUtf8("›"));
            chev->setCursor(Qt::PointingHandCursor);
            chev->setToolTip(open ? tr("Recolher cenas") : tr("Mostrar cenas"));
            chev->setStyleSheet(QStringLiteral(
                "QToolButton{border:0;background:transparent;color:%1;font-size:13px;"
                "min-width:14px;padding:0 1px;}").arg(Theme::textMuted()));
            connect(chev, &QToolButton::clicked, this, [this, expandKey]() {
                if (m_expanded.contains(expandKey)) m_expanded.remove(expandKey);
                else m_expanded.insert(expandKey);
                rebuildNavBody();
            });
            lay->addWidget(chev);
        } else {
            auto* chev = new QLabel(open ? QString::fromUtf8("⌄") : QString::fromUtf8("›"), row);
            chev->setFixedWidth(12);
            chev->setAlignment(Qt::AlignCenter);
            chev->setStyleSheet(QStringLiteral("color:%1; font-size:13px;").arg(Theme::textMuted()));
            lay->addWidget(chev);
        }

        // Miniatura da capa (manuscrito) ou icone (gaveta), quando houver —
        // e o que separa a linha-mae das folhas de olho, junto com o negrito.
        const QPixmap tp = refThumbFrom(thumbSrc);
        if (!tp.isNull()) {
            auto* th = new QLabel(row);
            th->setFixedSize(20, 26);
            th->setPixmap(tp.scaled(20, 26, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            th->setStyleSheet(Theme::qss(QStringLiteral("border-radius: @radius-item;")));
            lay->addWidget(th);
        } else if (!iconId.isEmpty()) {
            QIcon ic = IconUtils::loadToolbarIcon(
                QStringLiteral(":/icons/elements/%1.svg").arg(iconId),
                QColor(Theme::textMuted()), QColor(Theme::textBright()),
                QColor(Theme::textBright()), QSize(15, 15));
            if (!ic.isNull()) {
                auto* th = new QLabel(row);
                th->setFixedSize(18, 18);
                th->setPixmap(ic.pixmap(15, 15));
                th->setAlignment(Qt::AlignCenter);
                lay->addWidget(th);
            }
        }

        auto* name = new QLabel(label, row);
        name->setStyleSheet(QStringLiteral("color:%1; font-size:13px; font-weight:600;")
            .arg(open ? Theme::textBright() : Theme::textPrimary()));
        lay->addWidget(name, 1);

        if (!meta.isEmpty()) {
            auto* cnt = new QLabel(meta, row);
            cnt->setStyleSheet(QStringLiteral("color:%1; font-size:11px;").arg(Theme::textMuted()));
            lay->addWidget(cnt);
        }

        row->setStyleSheet(Theme::qss(QStringLiteral(
            "QWidget#refNavRow { border-radius: @radius-item; }"
            "QWidget#refNavRow:hover { background: %1; }")).arg(Theme::hoverOverlay()));
        if (docKey.isEmpty()) row->setProperty("refExpandKey", expandKey);
        else                  row->setProperty("refRowKey", docKey);
        row->installEventFilter(this);
        m_navInnerLay->addWidget(row);
        return open;
    };

    // Folha da árvore: um documento de verdade, que abre no preview.
    auto addLeaf = [this](const QString& key, const KeyInfo& info, int depth) {
        QWidget* w = makeNavRow(key, info, true, 12);   // um degrau abaixo da mae
        if (auto* lay = qobject_cast<QHBoxLayout*>(w->layout())) {
            const QMargins m = lay->contentsMargins();
            lay->setContentsMargins(m.left() + depth * 14, m.top(), m.right(), m.bottom());
        }
        m_navInnerLay->addWidget(w);
    };

    if (m_model) {
        for (const auto& ms : m_model->manuscripts()) {
            QList<Chapter> chs;
            for (const Chapter& c : m_model->chapters())
                if (c.manuscriptId == ms.id) chs.append(c);
            std::sort(chs.begin(), chs.end(),
                      [](const Chapter& a, const Chapter& b) { return a.order < b.order; });

            const bool open = addBranch(ms.title.isEmpty() ? tr("Manuscrito") : ms.title,
                                        tr("%n capítulo(s)", nullptr, int(chs.size())),
                                        QStringLiteral("ms:%1").arg(ms.id), 0,
                                        ms.coverDataUrl);
            if (!open) continue;

            for (const Chapter& c : chs) {
                const QString chKey = QStringLiteral("ch:%1:%2").arg(ms.id, c.id);
                const QString chName = c.title.isEmpty() ? tr("(sem título)") : c.title;

                // Capítulo com mais de uma cena vira galho; com uma só, é folha.
                if (c.scenes.size() > 1) {
                    // Passa a chave do capitulo: da' pra ler o capitulo inteiro
                    // clicando no nome, e abrir as cenas pela seta.
                    const bool chOpen = addBranch(chName,
                                                  tr("%n cena(s)", nullptr, int(c.scenes.size())),
                                                  QStringLiteral("chx:%1").arg(c.id), 1,
                                                  QString(), QString(), chKey);
                    if (chOpen) {
                        for (int i = 0; i < c.scenes.size(); ++i) {
                            const QString st = c.scenes.at(i).title;
                            KeyInfo si;
                            si.name = st.isEmpty() ? tr("Cena %1").arg(i + 1) : st;
                            addLeaf(QStringLiteral("sc:%1:%2:%3").arg(ms.id, c.id).arg(i), si, 2);
                        }
                    }
                } else {
                    KeyInfo ci;
                    ci.name = chName;
                    addLeaf(chKey, ci, 1);
                }
            }
        }

        for (const Drawer& d : m_model->drawers()) {
            const QString iconId = !d.drawerIcon.isEmpty() ? d.drawerIcon : d.drawerElementIcon;
            const bool open = addBranch(d.title.isEmpty() ? tr("Gaveta") : d.title,
                                        tr("%n item(ns)", nullptr, int(d.items.size())),
                                        QStringLiteral("dr:%1").arg(d.key), 0,
                                        QString(), iconId);
            if (!open) continue;
            for (const DrawerItem& it : d.items) {
                KeyInfo info;
                info.name = it.title.isEmpty() ? tr("(sem título)") : it.title;
                info.role = roleOrLabelForItem(it);
                info.imagePath = imageForItem(it);
                addLeaf(QStringLiteral("it:%1").arg(it.id), info, 1);
            }
        }
    }

    // Mundos: territórios e sistemas juntos, como no Explorador.
    {
        int worlds = 0;
        if (m_territorioStore) worlds += int(m_territorioStore->territorios().size());
        if (m_construtorStore) worlds += int(m_construtorStore->systems().size());
        const bool open = addBranch(tr("Mundos"), tr("%n item(ns)", nullptr, worlds),
                                    QStringLiteral("world"), 0);
        if (open) {
            if (m_territorioStore) {
                for (const auto& ter : m_territorioStore->territorios()) {
                    KeyInfo info;
                    info.name = ter.name.isEmpty() ? tr("(sem nome)") : ter.name;
                    info.meta = tr("território");
                    addLeaf(QStringLiteral("lug:%1").arg(ter.id), info, 1);
                }
            }
            if (m_construtorStore) {
                for (const auto& sys : m_construtorStore->systems()) {
                    KeyInfo info;
                    info.name = sys.name;
                    info.meta = tr("sistema");
                    addLeaf(QStringLiteral("ctr:%1").arg(sys.id), info, 1);
                }
            }
        }
    }
}

void RefMenuPanel::buildManuscriptsView()
{
    if (!m_model || !m_navInner || !m_navInnerLay) return;

    // ---- Seção Capítulos (do manuscrito escolhido via picker) ----
    auto* chTitle = new QLabel(tr("CAPÍTULOS"), m_navInner);
    chTitle->setStyleSheet(QStringLiteral("color:%1; font-size:10px; font-weight:700; letter-spacing:1px; padding:6px 6px 2px;")
                               .arg(Theme::textMuted()));
    m_navInnerLay->addWidget(chTitle);

    auto* chList = new QListWidget(m_navInner);
    chList->setObjectName(QStringLiteral("refListCh"));
    chList->setSelectionMode(QAbstractItemView::SingleSelection);
    chList->setFrameShape(QFrame::NoFrame);
    chList->setIconSize(QSize(14, 14));
    chList->setStyleSheet(Theme::qss(QStringLiteral(R"(
        QListWidget {
            background: %1; color: %2;
            border: 1px solid %3; border-radius: @radius-control;
            outline: none; padding: 4px;
        }
        QListWidget::item { padding: 6px 8px; border-radius: @radius-item; }
        QListWidget::item:hover { background: %4; color: %5; }
        QListWidget::item:selected { background: %6; color: %5; }
    )")).arg(Theme::appBackground(),
           Theme::textPrimary(),
           Theme::panelBorder(),
           Theme::hoverOverlay(),
           Theme::textBright(),
           Theme::accentInfoSoft()));
    QIcon icCh = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/chapter.svg"),
        QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
        QSize(14, 14));

    QList<Chapter> chs;
    QString msId = m_currentManuscriptId;
    if (msId.isEmpty() && !m_model->manuscripts().isEmpty()) {
        msId = m_model->manuscripts().first().id;
        m_currentManuscriptId = msId;
    }
    for (const auto& c : m_model->chapters()) {
        if (c.manuscriptId == msId) chs.append(c);
    }
    std::sort(chs.begin(), chs.end(), [](const Chapter& a, const Chapter& b) {
        return a.order < b.order;
    });
    for (const auto& c : chs) {
        const QString chTitle = c.title.isEmpty() ? tr("(sem título)") : c.title;
        const bool chTitleHit = matchesSearch(chTitle);

        // Coleta cenas que batem na busca (se houver query).
        QList<int> matchedScenes;
        if (c.scenes.size() > 1) {
            for (int i = 0; i < c.scenes.size(); ++i) {
                const auto& sc = c.scenes.at(i);
                const QString scTitle = sc.title.isEmpty() ? tr("Cena %1").arg(i + 1) : sc.title;
                if (matchesSearch(scTitle)) matchedScenes.append(i);
            }
        }

        // Pula capítulo se busca ativa e nem ele nem nenhuma cena batem.
        if (!m_searchQuery.isEmpty() && !chTitleHit && matchedScenes.isEmpty()) {
            continue;
        }

        auto* it = new QListWidgetItem(chTitle);
        if (!icCh.isNull()) it->setIcon(icCh);
        const QString key = QStringLiteral("ch:%1:%2").arg(msId, c.id);
        it->setData(Qt::UserRole, key);
        chList->addItem(it);
        if (key == m_selectedKey) it->setSelected(true);

        if (c.scenes.size() > 1) {
            for (int i = 0; i < c.scenes.size(); ++i) {
                const auto& sc = c.scenes.at(i);
                const QString scTitle = sc.title.isEmpty() ? tr("Cena %1").arg(i + 1) : sc.title;
                // Com busca ativa: só cenas que batem; sem busca: todas.
                if (!m_searchQuery.isEmpty() && !matchedScenes.contains(i)) continue;
                auto* si = new QListWidgetItem(QStringLiteral("       ") + scTitle);
                QFont f = si->font(); f.setItalic(true); si->setFont(f);
                const QString skey = QStringLiteral("sc:%1:%2:%3").arg(msId, c.id).arg(i);
                si->setData(Qt::UserRole, skey);
                chList->addItem(si);
                if (skey == m_selectedKey) si->setSelected(true);
            }
        }
    }
    if (chList->count() == 0) {
        chList->addItem(chs.isEmpty() ? tr("(nenhum capítulo)") : tr("(nenhum resultado)"));
        chList->item(0)->setFlags(Qt::NoItemFlags);
    }
    connect(chList, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        if (!it) return;
        const QString key = it->data(Qt::UserRole).toString();
        if (key.isEmpty()) return;
        setSelected(key);
    });
    m_navInnerLay->addWidget(chList);
}

void RefMenuPanel::buildDrawerView()
{
    if (!m_model || !m_navInner || !m_navInnerLay) return;
    const Drawer* d = m_model->findDrawer(m_currentDrawerKey);
    if (!d) {
        buildPlaceholderView(tr("Gaveta"), tr("Selecione uma gaveta acima."));
        return;
    }

    // Breadcrumb se em pasta
    if (!m_currentFolderId.isEmpty()) {
        const Folder* folder = nullptr;
        for (const auto& f : d->folders) if (f.id == m_currentFolderId) { folder = &f; break; }
        auto* row = new QWidget(m_navInner);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 4);
        rowLay->setSpacing(6);
        auto* back = new QToolButton(row);
        back->setObjectName(QStringLiteral("refTinyBtn"));
        back->setText(QStringLiteral("←"));
        back->setCursor(Qt::PointingHandCursor);
        connect(back, &QToolButton::clicked, this, [this]() {
            m_currentFolderId.clear();
            rebuildNavBody();
        });
        rowLay->addWidget(back);
        auto* lab = new QLabel(folder ? folder->title : tr("Pasta"), row);
        lab->setStyleSheet(QStringLiteral("color:%1; font-size:12px; font-weight:600;").arg(Theme::textPrimary()));
        rowLay->addWidget(lab);
        rowLay->addStretch();
        m_navInnerLay->addWidget(row);
    }

    // Folders strip — pastas em chips horizontais (estilo Mira 1 drawer strip)
    QList<Folder> childFolders;
    for (const auto& f : d->folders) {
        if ((f.parentId.isEmpty() && m_currentFolderId.isEmpty())
            || (f.parentId == m_currentFolderId && !m_currentFolderId.isEmpty())) {
            childFolders.append(f);
        }
    }
    if (!childFolders.isEmpty()) {
        auto* foldersScroll = new QScrollArea(m_navInner);
        foldersScroll->setWidgetResizable(true);
        foldersScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        foldersScroll->setFrameShape(QFrame::NoFrame);
        foldersScroll->setFixedHeight(36);
        foldersScroll->setStyleSheet(QStringLiteral("background:transparent;"));
        auto* host = new QWidget(foldersScroll);
        auto* hLay = new QHBoxLayout(host);
        hLay->setContentsMargins(0, 0, 0, 0);
        hLay->setSpacing(6);
        QIcon icFolder = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/folder.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(12, 12));
        for (const auto& f : childFolders) {
            auto* chip = new QToolButton(host);
            chip->setObjectName(QStringLiteral("refFolderChip"));
            chip->setText(f.title.isEmpty() ? tr("Pasta") : f.title);
            if (!icFolder.isNull()) {
                chip->setIcon(icFolder);
                chip->setIconSize(QSize(12, 12));
                chip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            }
            chip->setCursor(Qt::PointingHandCursor);
            chip->setStyleSheet(Theme::qss(QStringLiteral(R"(
                QToolButton {
                    background: %1; color: %2;
                    border: 1px solid %3; border-radius: @radius-control;
                    padding: 4px 10px; font-size: 11px;
                }
                QToolButton:hover { background: %4; color: %5; }
            )")).arg(Theme::appBackground(),
                   Theme::textPrimary(),
                   Theme::panelBorder(),
                   Theme::hoverOverlay(),
                   Theme::textBright()));
            const QString folderId = f.id;
            connect(chip, &QToolButton::clicked, this, [this, folderId]() {
                m_currentFolderId = folderId;
                rebuildNavBody();
            });
            hLay->addWidget(chip);
        }
        hLay->addStretch();
        foldersScroll->setWidget(host);
        m_navInnerLay->addWidget(foldersScroll);
    }

    // Items da pasta atual (ou raiz) — com filtro de busca
    QList<DrawerItem> items;
    for (const auto& it : d->items) {
        if (it.folderId != m_currentFolderId) continue;
        const QString title = it.title.isEmpty() ? tr("(sem título)") : it.title;
        if (!matchesSearch(title)) continue;
        items.append(it);
    }

    const bool visualDrawer = drawerIsVisual(d);
    const bool useVisual = visualDrawer && m_visualMode;

    if (items.isEmpty()) {
        auto* empty = new QLabel(
            m_searchQuery.isEmpty() ? tr("Sem itens nesta gaveta.") : tr("Nada encontrado."),
            m_navInner);
        empty->setStyleSheet(QStringLiteral("color:%1; font-size:11px; padding:8px;").arg(Theme::textMuted()));
        empty->setAlignment(Qt::AlignCenter);
        m_navInnerLay->addWidget(empty);
        return;
    }

    if (useVisual) {
        auto* gridHost = new QWidget(m_navInner);
        auto* gridLay = new QGridLayout(gridHost);
        gridLay->setContentsMargins(0, 0, 0, 0);
        gridLay->setSpacing(8);
        int row = 0;
        const QString accent = d->color.isEmpty() ? Theme::accentDefault() : d->color;
        for (const auto& it : items) {
            const QString key = QStringLiteral("it:%1").arg(it.id);
            auto* card = new QToolButton(gridHost);
            card->setObjectName(QStringLiteral("refVisualCard"));
            card->setCheckable(true);
            card->setAutoExclusive(false);
            card->setChecked(key == m_selectedKey);
            card->setCursor(Qt::PointingHandCursor);
            card->setMinimumHeight(72);
            card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            card->setStyleSheet(Theme::qss(QStringLiteral(R"(
                QToolButton#refVisualCard {
                    background: %2;
                    border: 1px solid %3;
                    border-radius: @radius-control;
                    text-align: left;
                    padding: 0;
                }
                QToolButton#refVisualCard:hover { background: %4; border-color: %5; }
                QToolButton#refVisualCard:checked { border-color: %1; background: %6; }
            )")).arg(accent,
                   Theme::appBackground(),
                   Theme::panelBorder(),
                   Theme::hoverOverlay(),
                   Theme::borderStrong(),
                   Theme::accentInfoSoft()));

            // Layout interno (foto à esquerda, infos à direita)
            auto* inner = new QWidget(card);
            inner->setAttribute(Qt::WA_TransparentForMouseEvents);
            auto* inLay = new QHBoxLayout(inner);
            inLay->setContentsMargins(8, 8, 10, 8);
            inLay->setSpacing(10);

            auto* photo = new QLabel(inner);
            photo->setFixedSize(56, 56);
            photo->setStyleSheet(Theme::qss(QStringLiteral("background:%1; border-radius: @radius-control;")).arg(Theme::inputBackground()));
            photo->setAlignment(Qt::AlignCenter);
            const QString img = imageForItem(it);
            QPixmap pm;
            if (!img.isEmpty()) {
                if (img.startsWith(QStringLiteral("data:"))) {
                    const int comma = img.indexOf(',');
                    if (comma > 0) {
                        QByteArray b64 = img.mid(comma + 1).toUtf8();
                        QByteArray bin = QByteArray::fromBase64(b64);
                        pm.loadFromData(bin);
                    }
                } else if (!m_projectRoot.isEmpty()) {
                    const QString path = ProjectStorage::joinPath(m_projectRoot, img);
                    pm.load(path);
                }
            }
            if (!pm.isNull()) {
                photo->setPixmap(pm.scaled(56, 56, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
                photo->setScaledContents(false);
            } else {
                photo->setText(QStringLiteral("?"));
                photo->setStyleSheet(Theme::qss(QStringLiteral("background:%1; border-radius: @radius-control; color:%2; font-size:22px; font-weight:700;"))
                                         .arg(Theme::inputBackground(), Theme::disabledText()));
            }
            inLay->addWidget(photo);

            auto* info = new QWidget(inner);
            auto* infoLay = new QVBoxLayout(info);
            infoLay->setContentsMargins(0, 4, 0, 4);
            infoLay->setSpacing(2);
            auto* name = new QLabel(it.title.isEmpty() ? tr("(sem título)") : it.title, info);
            name->setStyleSheet(QStringLiteral("color:%1; font-size:13px; font-weight:600;").arg(Theme::textBright()));
            infoLay->addWidget(name);
            const QString roleText = roleOrLabelForItem(it);
            if (!roleText.isEmpty()) {
                auto* roleLab = new QLabel(roleText.toUpper(), info);
                roleLab->setStyleSheet(QStringLiteral("color:%1; font-size:9px; font-weight:700; letter-spacing:1px;").arg(accent));
                infoLay->addWidget(roleLab);
            }
            infoLay->addStretch();
            inLay->addWidget(info, /*stretch=*/1);

            // monta o "ícone" do toolbutton como o widget interno via setLayout no card
            auto* cardLay = new QVBoxLayout(card);
            cardLay->setContentsMargins(0, 0, 0, 0);
            cardLay->addWidget(inner);

            connect(card, &QToolButton::clicked, this, [this, key]() { setSelected(key); });

            // Filtro por território (M7): esmaece (não esconde) quem não bate.
            if (!m_territorioFilterId.isEmpty()
                && it.origemTerritorioId != m_territorioFilterId
                && it.localAtualTerritorioId != m_territorioFilterId) {
                static constexpr qreal kTerritorioDimOpacity = 0.16; // mesmo valor de TimelineScene
                auto* effect = new QGraphicsOpacityEffect(card);
                effect->setOpacity(kTerritorioDimOpacity);
                card->setGraphicsEffect(effect);
            }

            gridLay->addWidget(card, row++, 0);
        }
        m_navInnerLay->addWidget(gridHost);
    } else {
        auto* list = new QListWidget(m_navInner);
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setFrameShape(QFrame::NoFrame);
        list->setIconSize(QSize(14, 14));
        list->setStyleSheet(Theme::qss(QStringLiteral(R"(
            QListWidget {
                background: %1; color: %2;
                border: 1px solid %3; border-radius: @radius-control;
                outline: none; padding: 4px;
            }
            QListWidget::item { padding: 6px 8px; border-radius: @radius-item; }
            QListWidget::item:hover { background: %4; color: %5; }
            QListWidget::item:selected { background: %6; color: %5; }
        )")).arg(Theme::appBackground(),
               Theme::textPrimary(),
               Theme::panelBorder(),
               Theme::hoverOverlay(),
               Theme::textBright(),
               Theme::accentInfoSoft()));
        QIcon icItemDefault = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/file.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14));
        for (const auto& it : items) {
            const QString key = QStringLiteral("it:%1").arg(it.id);
            QString text = it.title.isEmpty() ? tr("(sem título)") : it.title;
            const QString roleText = roleOrLabelForItem(it);
            if (!roleText.isEmpty()) text.append(QStringLiteral("   ·  ") + roleText);
            auto* li = new QListWidgetItem(text);
            // Ícone: prefere o elementIcon do item (se mapeia uma gaveta visual);
            // senão cai pro default file.svg.
            QIcon used;
            if (!it.elementIcon.isEmpty()) {
                QIcon ic = IconUtils::loadToolbarIcon(
                    QStringLiteral(":/icons/elements/%1.svg").arg(it.elementIcon),
                    QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
                    QSize(14, 14));
                if (!ic.isNull()) used = ic;
            }
            if (used.isNull()) used = icItemDefault;
            if (!used.isNull()) li->setIcon(used);
            li->setData(Qt::UserRole, key);
            list->addItem(li);
            if (key == m_selectedKey) li->setSelected(true);
        }
        connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            setSelected(it->data(Qt::UserRole).toString());
        });
        m_navInnerLay->addWidget(list);
    }
}

void RefMenuPanel::buildSearchAllView()
{
    if (!m_model || !m_navInner || !m_navInnerLay) return;

    auto sectionTitle = [this](const QString& text) {
        auto* lab = new QLabel(text, m_navInner);
        lab->setStyleSheet(QStringLiteral("color:%1; font-size:10px; font-weight:700; letter-spacing:1px; padding:8px 6px 2px;")
                               .arg(Theme::textMuted()));
        m_navInnerLay->addWidget(lab);
    };

    auto makeList = [this]() {
        auto* lw = new QListWidget(m_navInner);
        lw->setFrameShape(QFrame::NoFrame);
        lw->setIconSize(QSize(14, 14));
        lw->setStyleSheet(Theme::qss(QStringLiteral(R"(
            QListWidget {
                background: %1; color: %2;
                border: 1px solid %3; border-radius: @radius-control;
                outline: none; padding: 4px;
            }
            QListWidget::item { padding: 6px 8px; border-radius: @radius-item; }
            QListWidget::item:hover { background: %4; color: %5; }
            QListWidget::item:selected { background: %6; color: %5; }
        )")).arg(Theme::appBackground(),
               Theme::textPrimary(),
               Theme::panelBorder(),
               Theme::hoverOverlay(),
               Theme::textBright(),
               Theme::accentInfoSoft()));
        return lw;
    };

    // ===== Manuscritos =====
    QList<Manuscript> msHits;
    for (const auto& m : m_model->manuscripts()) {
        if (matchesSearch(m.title)) msHits.append(m);
    }
    if (!msHits.isEmpty()) {
        sectionTitle(tr("MANUSCRITOS"));
        auto* lw = makeList();
        const QIcon ic = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/manuscript.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14));
        for (const auto& m : msHits) {
            auto* it = new QListWidgetItem(m.title.isEmpty() ? tr("Manuscrito") : m.title);
            if (!ic.isNull()) it->setIcon(ic);
            it->setData(Qt::UserRole, QStringLiteral("ms:%1").arg(m.id));
            lw->addItem(it);
        }
        lw->setFixedHeight(qMin(160, 6 + 30 * msHits.size() + 4));
        connect(lw, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            const QString key = it->data(Qt::UserRole).toString();
            if (!key.startsWith(QStringLiteral("ms:"))) return;
            m_currentManuscriptId = key.mid(3);
            m_sourceKind = SourceKind::Manuscript;
            changeSelectedKey(QString());
            // sair do modo de busca pra revelar o manuscrito escolhido
            m_searchQuery.clear();
            if (m_searchInput) m_searchInput->clear();
            rebuildCrumbs();
            rebuildNavBody();
            rebuildPreview();
        });
        m_navInnerLay->addWidget(lw);
    }

    // ===== Capítulos + Cenas =====
    struct ChHit {
        QString msId;
        QString chId;
        QString title;
        QList<QPair<int, QString>> sceneHits; // (sceneIndex, sceneTitle)
    };
    QList<ChHit> chHits;
    for (const auto& c : m_model->chapters()) {
        const QString chTitle = c.title.isEmpty() ? tr("(sem título)") : c.title;
        const bool chHit = matchesSearch(chTitle);
        QList<QPair<int, QString>> sceneHits;
        if (c.scenes.size() > 1) {
            for (int i = 0; i < c.scenes.size(); ++i) {
                const auto& sc = c.scenes.at(i);
                const QString scTitle = sc.title.isEmpty() ? tr("Cena %1").arg(i + 1) : sc.title;
                if (matchesSearch(scTitle)) sceneHits.append({ i, scTitle });
            }
        }
        if (chHit || !sceneHits.isEmpty()) {
            ChHit h;
            h.msId = c.manuscriptId;
            h.chId = c.id;
            h.title = chTitle;
            h.sceneHits = sceneHits;
            chHits.append(h);
        }
    }
    if (!chHits.isEmpty()) {
        sectionTitle(tr("CAPÍTULOS"));
        auto* lw = makeList();
        const QIcon icCh = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/chapter.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14));
        for (const auto& h : chHits) {
            auto* it = new QListWidgetItem(h.title);
            if (!icCh.isNull()) it->setIcon(icCh);
            const QString key = QStringLiteral("ch:%1:%2").arg(h.msId, h.chId);
            it->setData(Qt::UserRole, key);
            lw->addItem(it);
            for (const auto& p : h.sceneHits) {
                auto* si = new QListWidgetItem(QStringLiteral("       ") + p.second);
                QFont f = si->font(); f.setItalic(true); si->setFont(f);
                const QString skey = QStringLiteral("sc:%1:%2:%3").arg(h.msId, h.chId).arg(p.first);
                si->setData(Qt::UserRole, skey);
                lw->addItem(si);
            }
        }
        connect(lw, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            const QString key = it->data(Qt::UserRole).toString();
            if (key.isEmpty()) return;
            // Atualiza source/manuscriptId pra coerência caso o usuário tire a busca depois.
            if (key.startsWith(QStringLiteral("ch:")) || key.startsWith(QStringLiteral("sc:"))) {
                const QStringList parts = key.split(QLatin1Char(':'));
                if (parts.size() >= 3) {
                    m_currentManuscriptId = parts.at(1);
                    m_sourceKind = SourceKind::Manuscript;
                }
            }
            setSelected(key);
        });
        m_navInnerLay->addWidget(lw);
    }

    // ===== Gavetas =====
    bool anyDrawerHit = false;
    for (const auto& d : m_model->drawers()) {
        QList<DrawerItem> hits;
        for (const auto& it : d.items) {
            const QString title = it.title.isEmpty() ? tr("(sem título)") : it.title;
            if (matchesSearch(title)) hits.append(it);
        }
        if (hits.isEmpty()) continue;
        anyDrawerHit = true;

        const QString label = d.title.isEmpty() ? tr("Gaveta") : d.title;
        sectionTitle(label.toUpper());
        auto* lw = makeList();
        const QString iconId = d.drawerElementIcon.isEmpty() ? d.drawerIcon : d.drawerElementIcon;
        QIcon ic;
        if (!iconId.isEmpty()) {
            ic = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/%1.svg").arg(iconId),
                QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
                QSize(14, 14));
        }
        for (const auto& it : hits) {
            auto* row = new QListWidgetItem(it.title.isEmpty() ? tr("(sem título)") : it.title);
            if (!ic.isNull()) row->setIcon(ic);
            row->setData(Qt::UserRole, QStringLiteral("it:%1").arg(it.id));
            // chave secundária pra navegar pra drawer correta ao clicar
            row->setData(Qt::UserRole + 1, d.key);
            lw->addItem(row);
        }
        const QString drawerKey = d.key;
        connect(lw, &QListWidget::itemClicked, this, [this, drawerKey](QListWidgetItem* it) {
            if (!it) return;
            const QString key = it->data(Qt::UserRole).toString();
            if (!key.startsWith(QStringLiteral("it:"))) return;
            // Move pro modo Drawer correto, mas mantém o resultado destacado.
            m_currentDrawerKey = drawerKey;
            m_sourceKind = SourceKind::Drawer;
            setSelected(key);
        });
        m_navInnerLay->addWidget(lw);
    }

    // ===== Territórios =====
    struct TerHit { QString path; QString key; QString territorioId; };
    QList<TerHit> terHits;
    if (m_territorioStore) {
        for (const auto& t : m_territorioStore->territorios()) {
            if (matchesSearch(t.name))
                terHits.append({ t.name, QStringLiteral("lug:%1").arg(t.id), t.id });
            std::function<void(const QString&, const QList<TerritorioStore::Node>&)> walk;
            walk = [&](const QString& breadcrumb, const QList<TerritorioStore::Node>& nodes) {
                for (const auto& n : nodes) {
                    const QString path = breadcrumb + QStringLiteral(" ▸ ") + n.name;
                    if (matchesSearch(path))
                        terHits.append({ path, QStringLiteral("lug:%1:%2").arg(t.id, n.id), t.id });
                    walk(path, n.children);
                }
            };
            walk(t.name, t.nodes);
        }
    }
    if (!terHits.isEmpty()) {
        sectionTitle(tr("TERRITÓRIOS"));
        auto* lw = makeList();
        for (const auto& h : terHits) {
            const TerritorioStore::Territorio* t = m_territorioStore->territorio(h.territorioId);
            const QIcon ic(AvatarUtils::circularAvatar(
                t ? t->avatarDataUrl : QString(), t ? t->name : h.territorioId, h.territorioId, 14));
            auto* it = new QListWidgetItem(ic, h.path);
            it->setData(Qt::UserRole, h.key);
            lw->addItem(it);
        }
        lw->setFixedHeight(qMin(160, 6 + 30 * terHits.size() + 4));
        connect(lw, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            const QString key = it->data(Qt::UserRole).toString();
            const QStringList parts = key.split(QLatin1Char(':'));
            if (parts.size() < 2) return;
            m_currentConstrutorSystemId.clear(); // garante que a view abra em território, não sistema
            m_currentLugarTerritorioId = parts.at(1);
            m_sourceKind = SourceKind::WorldExplorer;
            m_searchQuery.clear();
            if (m_searchInput) m_searchInput->clear();
            setSelected(key); // seta m_selectedKey + preview, antes do rebuild abaixo
            rebuildCrumbs();
            rebuildNavBody(); // já entra com o nó certo destacado na lista
        });
        m_navInnerLay->addWidget(lw);
    }

    // ===== Sistemas =====
    struct SysHit { QString path; QString key; };
    QList<SysHit> sysHits;
    if (m_construtorStore) {
        for (const auto& s : m_construtorStore->systems()) {
            if (matchesSearch(s.name))
                sysHits.append({ s.name, QStringLiteral("ctr:%1").arg(s.id) });
            std::function<void(const QString&, const QList<ConstrutorStore::Node>&)> walk;
            walk = [&](const QString& breadcrumb, const QList<ConstrutorStore::Node>& nodes) {
                for (const auto& n : nodes) {
                    const QString path = breadcrumb + QStringLiteral(" ▸ ") + n.name;
                    if (matchesSearch(path))
                        sysHits.append({ path, QStringLiteral("ctr:%1:%2").arg(s.id, n.id) });
                    walk(path, n.children);
                }
            };
            walk(s.name, s.nodes);
        }
    }
    if (!sysHits.isEmpty()) {
        sectionTitle(tr("SISTEMAS"));
        auto* lw = makeList();
        const QIcon icSys = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/cube.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
            QSize(14, 14));
        for (const auto& h : sysHits) {
            auto* it = new QListWidgetItem(h.path);
            if (!icSys.isNull()) it->setIcon(icSys);
            it->setData(Qt::UserRole, h.key);
            lw->addItem(it);
        }
        lw->setFixedHeight(qMin(160, 6 + 30 * sysHits.size() + 4));
        connect(lw, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
            if (!it) return;
            const QString key = it->data(Qt::UserRole).toString();
            const QStringList parts = key.split(QLatin1Char(':'));
            if (parts.size() < 2) return;
            m_currentLugarTerritorioId.clear(); // garante que a view abra em sistema, não território
            m_currentConstrutorSystemId = parts.at(1);
            m_sourceKind = SourceKind::WorldExplorer;
            m_searchQuery.clear();
            if (m_searchInput) m_searchInput->clear();
            setSelected(key); // seta m_selectedKey + preview, antes do rebuild abaixo
            rebuildCrumbs();
            rebuildNavBody(); // já entra com o nó certo destacado na lista
        });
        m_navInnerLay->addWidget(lw);
    }

    if (msHits.isEmpty() && chHits.isEmpty() && !anyDrawerHit
        && terHits.isEmpty() && sysHits.isEmpty()) {
        auto* empty = new QLabel(tr("Nada encontrado."), m_navInner);
        empty->setStyleSheet(QStringLiteral("color:%1; font-size:12px; padding:16px;").arg(Theme::textMuted()));
        empty->setAlignment(Qt::AlignCenter);
        m_navInnerLay->addWidget(empty);
    }
}

void RefMenuPanel::buildPlaceholderView(const QString& title, const QString& subtitle)
{
    auto* card = new QFrame(m_navInner);
    card->setStyleSheet(Theme::qss(QStringLiteral(R"(
        QFrame {
            background: %1;
            border: 1px dashed %2;
            border-radius: @radius-panel;
        }
    )")).arg(Theme::appBackground(), Theme::panelBorder()));
    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(6);
    auto* t = new QLabel(title, card);
    t->setStyleSheet(QStringLiteral("color:%1; font-size:13px; font-weight:700;").arg(Theme::textBright()));
    t->setAlignment(Qt::AlignCenter);
    lay->addWidget(t);
    auto* s = new QLabel(subtitle, card);
    s->setStyleSheet(QStringLiteral("color:%1; font-size:11px;").arg(Theme::textMuted()));
    s->setAlignment(Qt::AlignCenter);
    s->setWordWrap(true);
    lay->addWidget(s);
    m_navInnerLay->addWidget(card);
}

namespace {
// Trecho de busca a partir do texto da memória: primeira linha não-vazia,
// limitada a ~60 chars (QTextEdit::find casa só dentro de uma linha).
QString memSearchQuery(const QString& text)
{
    QString t = text;
    t.replace(QChar(0x2029), QChar('\n'));
    const QStringList lines = t.split(QChar('\n'), Qt::SkipEmptyParts);
    QString q = lines.isEmpty() ? t.trimmed() : lines.first().trimmed();
    if (q.size() > 60) q = q.left(60);
    return q;
}
} // namespace

void RefMenuPanel::openMemoryInRef(const MemoriesStore::Memory& mem)
{
    const QString query = memSearchQuery(mem.text);

    if (mem.sourceType == QStringLiteral("scene") && !mem.chapterId.isEmpty()) {
        enterManuscriptMode(mem.manuscriptId);
        setSelected(QStringLiteral("sc:%1:%2:%3")
                        .arg(mem.manuscriptId, mem.chapterId).arg(mem.sceneIndex));
    } else if (mem.sourceType == QStringLiteral("chapter") && !mem.chapterId.isEmpty()) {
        enterManuscriptMode(mem.manuscriptId);
        setSelected(QStringLiteral("ch:%1:%2").arg(mem.manuscriptId, mem.chapterId));
    } else if (mem.sourceType == QStringLiteral("drawer") && !mem.itemId.isEmpty()) {
        QString drawerKey;
        if (m_model) m_model->findDrawerItem(mem.itemId, &drawerKey);
        if (!drawerKey.isEmpty()) {
            enterDrawerMode(drawerKey);
            setSelected(QStringLiteral("it:%1").arg(mem.itemId));
        }
    } else {
        return;
    }

    if (!isVisible()) openPanel();
    else { show(); raise(); }
    highlightInPreview(query);
}

void RefMenuPanel::highlightInPreview(const QString& query)
{
    if (!m_preview || query.isEmpty()) return;
    // "Ctrl+F" automático: leva o cursor ao início e seleciona o 1º casamento,
    // o que rola até ele e o deixa marcado pela seleção.
    m_preview->moveCursor(QTextCursor::Start);
    if (m_preview->find(query)) {
        m_preview->ensureCursorVisible();
    }
}

// =========================================================================
// Preview
// =========================================================================

void RefMenuPanel::rebuildPreview()
{
    if (!m_preview || !m_previewTitle || !m_previewRole) return;

    // Antes de trocar o conteúdo do preview, salva/encerra edição em
    // andamento (se houver) — senão o texto editado se perde no setHtml.
    updateEditAvailability();

    // limpa imagens anteriores
    while (m_previewImagesLay && m_previewImagesLay->count() > 0) {
        QLayoutItem* item = m_previewImagesLay->takeAt(0);
        if (auto* w = item->widget()) w->deleteLater();
        delete item;
    }
    m_previewImagePixmaps.clear();
    m_previewImageLabels.clear();
    m_previewImagesScroll->setVisible(false);

    if (m_selectedKey.isEmpty()) {
        m_previewTitle->setVisible(false);
        m_previewRole->setVisible(false);
        m_preview->setVisible(false);
        m_previewPlaceholder->setVisible(true);
        return;
    }
    m_previewPlaceholder->setVisible(false);
    m_preview->setVisible(true);

    // Resolve título e role
    QString title;
    QString role;
    if (m_selectedKey.startsWith(QStringLiteral("ch:"))) {
        const QStringList parts = m_selectedKey.split(QLatin1Char(':'));
        if (parts.size() >= 3 && m_model) {
            const Chapter* ch = m_model->findChapter(parts.at(2));
            if (ch) title = ch->title;
        }
    } else if (m_selectedKey.startsWith(QStringLiteral("sc:"))) {
        const QStringList parts = m_selectedKey.split(QLatin1Char(':'));
        if (parts.size() >= 4 && m_model) {
            const Chapter* ch = m_model->findChapter(parts.at(2));
            int idx = parts.at(3).toInt();
            if (ch && idx >= 0 && idx < ch->scenes.size()) {
                const Scene& sc = ch->scenes.at(idx);
                title = sc.title.isEmpty() ? tr("Cena %1").arg(idx + 1) : sc.title;
            }
        }
    } else if (m_selectedKey.startsWith(QStringLiteral("it:"))) {
        const QString itemId = m_selectedKey.mid(3);
        const DrawerItem* it = m_model ? m_model->findDrawerItem(itemId) : nullptr;
        if (it) {
            title = it->title;
            role = roleOrLabelForItem(*it);
        }
    } else if (m_selectedKey.startsWith(QStringLiteral("ctr:"))) {
        const QStringList parts = m_selectedKey.split(QLatin1Char(':'));
        const ConstrutorStore::System* sys =
            (m_construtorStore && parts.size() >= 2) ? m_construtorStore->system(parts.at(1)) : nullptr;
        if (sys && parts.size() >= 3) {
            if (const ConstrutorStore::Node* node = findConstrutorNode(sys, parts.at(2))) {
                title = node->name;
                role = tr("%1 · %2").arg(
                    node->type == ConstrutorStore::NodeType::Rule ? tr("Regra") : tr("Seção"), sys->name);
            }
        } else if (sys) {
            title = sys->name;
            role = tr("Sistema · Explorador de Mundos");
        }
    } else if (m_selectedKey.startsWith(QStringLiteral("lug:"))) {
        const QStringList parts = m_selectedKey.split(QLatin1Char(':'));
        const TerritorioStore::Territorio* ter =
            (m_territorioStore && parts.size() >= 2) ? m_territorioStore->territorio(parts.at(1)) : nullptr;
        if (ter && parts.size() >= 3) {
            if (const TerritorioStore::Node* node = findTerritorioNode(ter, parts.at(2))) {
                title = node->name;
                role = tr("%1 · %2").arg(
                    node->type == TerritorioStore::NodeType::Doc ? tr("Documento") : tr("Pasta"), ter->name);
            }
        } else if (ter) {
            title = ter->name;
            role = tr("Território · Explorador de Mundos");
        }
    }

    m_previewTitle->setText(title.isEmpty() ? tr("(sem título)") : title);
    m_previewTitle->setVisible(true);
    if (!role.isEmpty()) {
        m_previewRole->setText(role.toUpper());
        m_previewRole->setVisible(true);
    } else {
        m_previewRole->setVisible(false);
    }

    // HTML
    const QString html = resolveDocHtml(m_selectedKey);
    QStringList images;
    QString rest;
    extractImagesFromHtml(html, &images, &rest);

    // Imagens no topo
    if (!images.isEmpty() && m_previewImagesLay) {
        for (const QString& imgHtml : images) {
            QRegularExpression re(QStringLiteral("src=\"([^\"]*)\""));
            QRegularExpressionMatch m = re.match(imgHtml);
            QString src = m.hasMatch() ? m.captured(1) : QString();
            QString resolved = resolveImageSrc(src);
            QPixmap pm;
            if (resolved.startsWith(QStringLiteral("data:"))) {
                const int comma = resolved.indexOf(',');
                if (comma > 0) {
                    QByteArray bin = QByteArray::fromBase64(resolved.mid(comma + 1).toUtf8());
                    pm.loadFromData(bin);
                }
            } else if (!resolved.isEmpty()) {
                pm.load(resolved);
            }
            if (pm.isNull()) continue;
            auto* lab = new QLabel(m_previewImagesHost);
            lab->setAlignment(Qt::AlignCenter);
            lab->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            m_previewImagePixmaps.append(pm);
            m_previewImageLabels.append(lab);
            m_previewImagesLay->addWidget(lab);
        }
        if (!m_previewImageLabels.isEmpty()) {
            m_previewImagesScroll->setVisible(true);
            rescalePreviewImages();
        }
    }

    // Conteúdo HTML
    if (rest.trimmed().isEmpty()) {
        m_preview->setHtml(QStringLiteral("<p style='color:%2;'>%1</p>")
                               .arg(tr("(documento vazio)"), Theme::textMuted()));
    } else {
        m_preview->setHtml(rest);
    }

    // O HTML salvo vem com `color:` inline do tema da hora em que foi escrito
    // (charformat embutido pelo toHtml). Sobrescreve com a cor do tema atual
    // pra não ficar branco no tema claro. Onde houver background de marker,
    // foreground vai por contraste WCAG (mesma lógica do editor).
    {
        const QColor textColor(Theme::textPrimary());
        QTextDocument* doc = m_preview->document();
        for (QTextBlock block = doc->firstBlock(); block.isValid(); block = block.next()) {
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const QTextFragment frag = it.fragment();
                if (!frag.isValid() || frag.length() == 0) continue;
                const QTextCharFormat existing = frag.charFormat();
                QColor fg = textColor;
                const QBrush bgBrush = existing.background();
                if (bgBrush.style() != Qt::NoBrush && bgBrush.color().alpha() > 0) {
                    fg = MarkerStore::pickContrastingFg(bgBrush.color());
                }
                QTextCursor c(doc);
                c.setPosition(frag.position());
                c.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
                QTextCharFormat fmt;
                fmt.setForeground(fg);
                c.mergeCharFormat(fmt);
            }
        }
    }

    // Resolve src de imagens internas remanescentes para QTextBrowser (caso restem inline)
    // Simples: o QTextDocument resolve via setSearchPaths se houver projectRoot.
    if (!m_projectRoot.isEmpty()) {
        QStringList paths;
        paths << m_projectRoot << ProjectStorage::joinPath(m_projectRoot, QStringLiteral("content/images"));
        m_preview->setSearchPaths(paths);
    }

    // Doc novo herda o tamanho de fonte do ciclo Aa, vencendo o font-size
    // inline do HTML salvo.
    applyPreviewFont();
}

void RefMenuPanel::rescalePreviewImages()
{
    if (!m_previewImagesScroll || m_previewImagePixmaps.isEmpty()) return;

    const int w = m_previewImagesScroll->viewport()->width();
    if (w <= 0) return;

    int totalH = 0;
    for (int i = 0; i < m_previewImagePixmaps.size() && i < m_previewImageLabels.size(); ++i) {
        const QPixmap& pm = m_previewImagePixmaps.at(i);
        QLabel* lab = m_previewImageLabels.at(i);
        if (!lab || pm.isNull()) continue;
        const int h = pm.height() * w / qMax(1, pm.width());
        lab->setFixedHeight(h);
        lab->setPixmap(pm.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        if (totalH > 0) totalH += m_previewImagesLay->spacing();
        totalH += h;
    }

    // A QScrollArea não cresce com o conteúdo por conta própria (widgetResizable
    // só controla a largura/altura do widget interno). Ajusta a altura da área
    // pra caber a(s) imagem(ns) inteira(s), até o teto de 360px (acima disso,
    // scroll vertical).
    m_previewImagesScroll->setFixedHeight(qBound(1, totalH, 360));
}

void RefMenuPanel::setEditorFontFamily(const QString& family)
{
    if (m_editorFontFamily == family) return;
    m_editorFontFamily = family;
    applyPreviewFont();
}

void RefMenuPanel::applyPreviewFont()
{
    if (!m_preview) return;
    QFont f = m_preview->font();
    f.setPointSize(m_previewFontPt);
    if (!m_editorFontFamily.isEmpty()) f.setFamily(m_editorFontFamily);
    m_preview->setFont(f);

    // HTML salvo do Mira 1 vem com font-size inline em cada bloco (toHtml do
    // QTextDocument embute charformat). Sem isso, setFont é silenciosamente
    // derrotado. mergeCharFormat só toca charFormat — não mexe em indent,
    // line-height nem espaçamento de parágrafo (esses ficam em blockFormat).
    QTextCursor cur(m_preview->document());
    cur.select(QTextCursor::Document);
    QTextCharFormat fmt;
    fmt.setFontPointSize(m_previewFontPt);
    cur.mergeCharFormat(fmt);

    // título escala junto
    if (m_previewTitle) {
        QFont tf = m_previewTitle->font();
        tf.setPointSize(m_previewFontPt + 2);
        tf.setBold(true);
        m_previewTitle->setFont(tf);
    }
}

QString RefMenuPanel::resolveDocHtml(const QString& key) const
{
    if (!m_model) return QString();

    if (key.startsWith(QStringLiteral("ch:"))) {
        const QStringList parts = key.split(QLatin1Char(':'));
        if (parts.size() < 3) return QString();
        const QString msId = parts.at(1);
        const QString chId = parts.at(2);
        const QString cacheKey = DocCache::chapterKey(msId, chId);
        if (m_cache && m_cache->has(cacheKey)) return m_cache->get(cacheKey);
        const Chapter* ch = m_model->findChapter(chId);
        if (!ch || ch->file.isEmpty() || m_projectRoot.isEmpty()) return QString();
        bool ok = false;
        return ProjectStorage::readChapter(m_projectRoot, ch->file, &ok);
    }
    if (key.startsWith(QStringLiteral("sc:"))) {
        const QStringList parts = key.split(QLatin1Char(':'));
        if (parts.size() < 4) return QString();
        const QString msId = parts.at(1);
        const QString chId = parts.at(2);
        const int sceneIdx = parts.at(3).toInt();
        const QString cacheKey = DocCache::chapterKey(msId, chId);
        QString chapterHtml;
        if (m_cache && m_cache->has(cacheKey)) {
            chapterHtml = m_cache->get(cacheKey);
        } else if (const Chapter* ch = m_model->findChapter(chId); ch && !m_projectRoot.isEmpty()) {
            bool ok = false;
            chapterHtml = ProjectStorage::readChapter(m_projectRoot, ch->file, &ok);
        }
        return SceneUtils::getSceneHtml(chapterHtml, sceneIdx);
    }
    if (key.startsWith(QStringLiteral("it:"))) {
        const QString itemId = key.mid(3);
        const DrawerItem* item = m_model->findDrawerItem(itemId);
        return DocPreview::resolveDrawerItemHtml(item, m_elements, m_cache, m_projectRoot);
    }
    if (key.startsWith(QStringLiteral("ctr:"))) {
        const QStringList parts = key.split(QLatin1Char(':'));
        if (!m_construtorStore || parts.size() < 2) return QString();
        const ConstrutorStore::System* sys = m_construtorStore->system(parts.at(1));
        if (!sys) return QString();
        if (parts.size() < 3) return sys->content;
        const ConstrutorStore::Node* node = findConstrutorNode(sys, parts.at(2));
        return node ? node->content : QString();
    }
    if (key.startsWith(QStringLiteral("lug:"))) {
        const QStringList parts = key.split(QLatin1Char(':'));
        if (!m_territorioStore || parts.size() < 2) return QString();
        const TerritorioStore::Territorio* ter = m_territorioStore->territorio(parts.at(1));
        if (!ter) return QString();
        if (parts.size() < 3) return ter->content;
        const TerritorioStore::Node* node = findTerritorioNode(ter, parts.at(2));
        return node ? node->content : QString();
    }
    return QString();
}

void RefMenuPanel::extractImagesFromHtml(const QString& html, QStringList* imagesOut, QString* restOut) const
{
    if (restOut) restOut->clear();
    if (imagesOut) imagesOut->clear();
    if (html.isEmpty()) return;

    // Imagens flutuantes (inseridas via QTextFrame no editor principal) são
    // serializadas pelo toHtml() como uma <table> envolvendo o <img>. Extrai
    // primeiro esses wrappers completos, senão a <table> vazia sobra no HTML
    // e renderiza como um retângulo com borda no preview.
    QRegularExpression reTable(QStringLiteral("<table\\b[^>]*>(?:(?!</table>).)*?(<img\\b[^>]*?>)(?:(?!</table>).)*?</table>"),
                                QRegularExpression::CaseInsensitiveOption
                                | QRegularExpression::DotMatchesEverythingOption);

    QString working = html;
    int last = 0;
    QString out;
    auto itTable = reTable.globalMatch(working);
    while (itTable.hasNext()) {
        QRegularExpressionMatch m = itTable.next();
        if (m.capturedStart() > last) out.append(working.mid(last, m.capturedStart() - last));
        if (imagesOut) imagesOut->append(m.captured(1));
        last = m.capturedEnd();
    }
    if (last < working.size()) out.append(working.mid(last));
    working = out;

    // Segunda passada: imagens "soltas" (não envolvidas por <table>).
    QRegularExpression re(QStringLiteral("<img\\b[^>]*?>"),
                          QRegularExpression::CaseInsensitiveOption);
    last = 0;
    out.clear();
    auto it = re.globalMatch(working);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        if (m.capturedStart() > last) out.append(working.mid(last, m.capturedStart() - last));
        if (imagesOut) imagesOut->append(m.captured(0));
        last = m.capturedEnd();
    }
    if (last < working.size()) out.append(working.mid(last));
    if (restOut) *restOut = out;
}

QString RefMenuPanel::resolveImageSrc(const QString& src) const
{
    if (src.isEmpty()) return src;
    if (src.startsWith(QStringLiteral("data:"))) return src;
    // Imagens inseridas pelo editor principal usam URL absoluta file:///...
    QUrl url(src);
    if (url.isLocalFile()) return url.toLocalFile();
    if (m_projectRoot.isEmpty()) return src;
    QString s = src;
    if (s.startsWith(QStringLiteral("./"))) s = s.mid(2);
    return ProjectStorage::joinPath(m_projectRoot, s);
}

// =========================================================================
// Drawer picker menu
// =========================================================================



// =========================================================================
// Estado: modos
// =========================================================================

void RefMenuPanel::enterManuscriptMode(const QString& msId)
{
    // Entrar por fora (mencao, Pensario, atalho) tambem cai na arvore: abre o
    // galho certo em vez de trocar pra outra tela.
    m_navMode = NavMode::Home;
    m_sourceKind = SourceKind::Manuscript;
    if (!msId.isEmpty()) m_currentManuscriptId = msId;
    if (m_currentManuscriptId.isEmpty() && m_model && !m_model->manuscripts().isEmpty()) {
        m_currentManuscriptId = m_model->manuscripts().first().id;
    }
    if (!m_currentManuscriptId.isEmpty())
        m_expanded.insert(QStringLiteral("ms:%1").arg(m_currentManuscriptId));
    m_currentFolderId.clear();
    refresh();
}

void RefMenuPanel::enterDrawerMode(const QString& drawerKey)
{
    m_navMode = NavMode::Home;
    m_sourceKind = SourceKind::Drawer;
    m_currentDrawerKey = drawerKey;
    if (!drawerKey.isEmpty()) m_expanded.insert(QStringLiteral("dr:%1").arg(drawerKey));
    m_currentFolderId.clear();
    changeSelectedKey(QString());
    refresh();
}

void RefMenuPanel::enterPlaceholderMode(SourceKind kind)
{
    m_navMode = NavMode::Home;
    if (kind == SourceKind::WorldExplorer) m_expanded.insert(QStringLiteral("world"));
    m_sourceKind = kind;
    m_currentConstrutorSystemId.clear();
    m_currentLugarTerritorioId.clear();
    changeSelectedKey(QString());
    refresh();
}

void RefMenuPanel::changeSelectedKey(const QString& key)
{
    if (m_selectedKey == key) return;
    m_selectedKey = key;
    emit selectedKeyChanged(selectedCacheKey());
}

QString RefMenuPanel::selectedCacheKey() const
{
    if (m_selectedKey.isEmpty()) return QString();
    // "ch:<ms>:<ch>" → já é chave de cache
    if (m_selectedKey.startsWith(QStringLiteral("ch:"))) return m_selectedKey;
    // "sc:<ms>:<ch>:<idx>" → cena vive no cache do capítulo pai
    if (m_selectedKey.startsWith(QStringLiteral("sc:"))) {
        const QStringList p = m_selectedKey.split(QLatin1Char(':'));
        if (p.size() >= 3) return DocCache::chapterKey(p.at(1), p.at(2));
    }
    // "it:<id>" → já é chave de cache
    if (m_selectedKey.startsWith(QStringLiteral("it:"))) return m_selectedKey;
    return QString();
}

QString RefMenuPanel::editableCacheKey() const
{
    // Território/Sistema do Criador de Mundos — editável direto (persiste
    // em TerritorioStore/ConstrutorStore, não no DocCache), liberado sempre
    // que o alvo ainda existir. Nunca abre no editor principal, então o
    // guard de conflito abaixo não se aplica a esses dois prefixos.
    if (m_selectedKey.startsWith(QStringLiteral("ctr:"))) {
        const QStringList parts = m_selectedKey.split(QLatin1Char(':'));
        if (parts.size() < 2 || !m_construtorStore) return QString();
        return m_construtorStore->system(parts.at(1)) ? m_selectedKey : QString();
    }
    if (m_selectedKey.startsWith(QStringLiteral("lug:"))) {
        const QStringList parts = m_selectedKey.split(QLatin1Char(':'));
        if (parts.size() < 2 || !m_territorioStore) return QString();
        return m_territorioStore->territorio(parts.at(1)) ? m_selectedKey : QString();
    }

    // Só capítulos e itens de gaveta têm um HTML completo de 1:1 com a
    // chave de cache. Cenas individuais (parte de um capítulo) ficam de
    // fora por ora.
    if (!m_selectedKey.startsWith(QStringLiteral("ch:"))
        && !m_selectedKey.startsWith(QStringLiteral("it:"))) {
        return QString();
    }
    const QString key = selectedCacheKey();
    if (key.isEmpty()) return QString();
    // Evita conflito com o editor principal: se o mesmo doc está aberto lá,
    // edição fica bloqueada no RefMenu.
    if (m_host && m_host->activeKey() == key) return QString();
    return key;
}

void RefMenuPanel::updateEditAvailability()
{
    if (!m_editBtn) return;

    if (m_editing) {
        commitEdit();
        m_editing = false;
        m_editingKey.clear();
        if (m_preview) {
            m_preview->setReadOnly(true);
            m_preview->setStyleSheet(QString());
        }
        QSignalBlocker b(m_editBtn);
        m_editBtn->setChecked(false);
    }

    const QString key = editableCacheKey();
    m_editBtn->setEnabled(!key.isEmpty());

    if (!key.isEmpty()) {
        m_editBtn->setToolTip(tr("Editar documento"));
    } else if (m_selectedKey.startsWith(QStringLiteral("sc:"))) {
        m_editBtn->setToolTip(tr("Edição de cena individual em breve"));
    } else if (m_host && !m_selectedKey.isEmpty() && m_host->activeKey() == selectedCacheKey()) {
        m_editBtn->setToolTip(tr("Este documento está aberto no editor principal"));
    } else {
        m_editBtn->setToolTip(tr("Editar documento"));
    }
}

void RefMenuPanel::commitEdit()
{
    if (!m_editing || !m_preview || m_editingKey.isEmpty()) return;

    if (m_editingKey.startsWith(QStringLiteral("ctr:"))) {
        const QStringList parts = m_editingKey.split(QLatin1Char(':'));
        if (parts.size() < 2 || !m_construtorStore) return;
        const ConstrutorStore::System* sys = m_construtorStore->system(parts.at(1));
        if (!sys) return;
        const QString html = m_preview->toHtml();
        if (parts.size() >= 3) {
            if (const ConstrutorStore::Node* node = findConstrutorNode(sys, parts.at(2)))
                m_construtorStore->updateNode(sys->id, node->id, node->name, html);
        } else {
            m_construtorStore->updateSystemContent(sys->id, html);
        }
        return;
    }
    if (m_editingKey.startsWith(QStringLiteral("lug:"))) {
        const QStringList parts = m_editingKey.split(QLatin1Char(':'));
        if (parts.size() < 2 || !m_territorioStore) return;
        const TerritorioStore::Territorio* ter = m_territorioStore->territorio(parts.at(1));
        if (!ter) return;
        const QString html = m_preview->toHtml();
        if (parts.size() >= 3) {
            if (const TerritorioStore::Node* node = findTerritorioNode(ter, parts.at(2)))
                m_territorioStore->updateNode(ter->id, node->id, node->name, html);
        } else {
            m_territorioStore->updateTerritorioContent(ter->id, html);
        }
        return;
    }

    if (!m_cache) return;
    m_cache->set(m_editingKey, m_preview->toHtml(), /*markDirty=*/true);
}

void RefMenuPanel::onToggleEdit(bool on)
{
    if (!m_preview) return;

    if (on) {
        m_editingKey = editableCacheKey();
        if (m_editingKey.isEmpty()) {
            QSignalBlocker b(m_editBtn);
            m_editBtn->setChecked(false);
            return;
        }
        m_editing = true;
        m_preview->setReadOnly(false);
        m_preview->setStyleSheet(QStringLiteral("QTextBrowser#refPreview { background: %1; }")
                                      .arg(Theme::inputBackground()));
        m_preview->setFocus();
    } else {
        commitEdit();
        m_editing = false;
        m_editingKey.clear();
        m_preview->setReadOnly(true);
        m_preview->setStyleSheet(QString());
    }
}

void RefMenuPanel::setSelected(const QString& key)
{
    changeSelectedKey(key);
    rebuildPreview();
}

// =========================================================================
// Open / close / toggles
// =========================================================================

void RefMenuPanel::openPanel()
{
    if (m_sourceKind == SourceKind::Manuscript && m_currentManuscriptId.isEmpty() && m_model
        && !m_model->manuscripts().isEmpty()) {
        m_currentManuscriptId = m_model->manuscripts().first().id;
    }
    refresh();
    show();
    raise();
    nudgeClearOfChrome();
    emit geometryChanged();
}

// O RefMenu e uma janela livre com geometria propria salva (o usuario escolhe
// onde ela fica), entao NAO reposicionamos por gosto: so empurramos pra
// esquerda quando ela ficaria literalmente escondida atras da barra lateral.
// Sem isto, quem tinha o painel salvo no canto direito de antes da barra virar
// vertical passa a abri-lo debaixo dela.
void RefMenuPanel::nudgeClearOfChrome()
{
    if (m_rightInset <= 0) return;
    QWidget* p = parentWidget();
    if (!p || !p->window()) return;
    const QRect win = p->window()->geometry();
    constexpr int kGap = 12;
    const int limit = win.right() - m_rightInset - kGap;
    QRect g = geometry();
    if (g.right() <= limit) return; // ja esta livre
    g.moveRight(qMax(win.left() + kGap, limit));
    setGeometry(g);
    scheduleGeometrySave();
}

void RefMenuPanel::closePanel()
{
    if (m_editing) {
        commitEdit();
        m_editing = false;
        m_editingKey.clear();
        if (m_preview) {
            m_preview->setReadOnly(true);
            m_preview->setStyleSheet(QString());
        }
        if (m_editBtn) {
            QSignalBlocker b(m_editBtn);
            m_editBtn->setChecked(false);
        }
    }
    hide();
    emit geometryChanged();
}

void RefMenuPanel::togglePanel()
{
    if (isVisible()) closePanel();
    else openPanel();
}

void RefMenuPanel::openForDrawer(const QString& drawerKey, const QString& itemId)
{
    enterDrawerMode(drawerKey);
    if (!itemId.isEmpty()) {
        changeSelectedKey(QStringLiteral("it:%1").arg(itemId));
        rebuildPreview();
    }
    if (!isVisible()) openPanel();
    else { show(); raise(); }
}

void RefMenuPanel::openForChapter(const QString& manuscriptId, const QString& chapterId)
{
    enterManuscriptMode(manuscriptId);
    setSelected(QStringLiteral("ch:%1:%2").arg(manuscriptId, chapterId));
    if (!isVisible()) openPanel();
    else { show(); raise(); }
}

void RefMenuPanel::openForScene(const QString& manuscriptId, const QString& chapterId, int sceneIndex)
{
    enterManuscriptMode(manuscriptId);
    setSelected(QStringLiteral("sc:%1:%2:%3").arg(manuscriptId, chapterId).arg(sceneIndex));
    if (!isVisible()) openPanel();
    else { show(); raise(); }
}

void RefMenuPanel::onCloseClicked()
{
    closePanel();
}

void RefMenuPanel::onToggleNav()
{
    m_navHidden = !m_navHidden;
    applyNavVisibility();
    QSettings().setValue(QString::fromLatin1(kKeyNavHidden), m_navHidden);
}

void RefMenuPanel::applyNavVisibility()
{
    if (m_toggleNavBtn) {
        m_toggleNavBtn->setText(m_navHidden ? QStringLiteral("▸") : QStringLiteral("▾"));
        m_toggleNavBtn->setToolTip(m_navHidden ? tr("Mostrar explorador") : tr("Ocultar explorador"));
    }
    // Esconde tabs + nav. O preview ocupa todo o frame.
    if (m_crumbsRow) rebuildCrumbs();   // a trilha decide sozinha se aparece
    if (m_navScroll) m_navScroll->setVisible(!m_navHidden);
    if (!m_navHidden) rebuildNavBody();
}

void RefMenuPanel::onTogglePin()
{
    m_pinned = m_pinBtn->isChecked();
    QSettings().setValue(QString::fromLatin1(kKeyPinned), m_pinned);
}

void RefMenuPanel::onToggleSearch()
{
    if (!m_searchRow) return;
    const bool willShow = !m_searchRow->isVisible();
    m_searchRow->setVisible(willShow);
    if (m_searchBtn) m_searchBtn->setChecked(willShow);
    if (willShow) {
        if (m_searchInput) {
            m_searchInput->setFocus();
            m_searchInput->selectAll();
        }
    } else {
        if (m_searchInput) m_searchInput->clear();
        // textChanged limpa m_searchQuery via onSearchQueryChanged
    }
}

void RefMenuPanel::onSearchQueryChanged(const QString& q)
{
    m_searchQuery = q.trimmed();
    rebuildNavBody();
}

void RefMenuPanel::openSearch()
{
    if (!isVisible()) openPanel();
    if (m_searchRow && !m_searchRow->isVisible()) onToggleSearch();
    else if (m_searchInput) {
        m_searchInput->setFocus();
        m_searchInput->selectAll();
    }
}

void RefMenuPanel::closeSearch()
{
    if (m_searchRow && m_searchRow->isVisible()) onToggleSearch();
}

void RefMenuPanel::openPreviewFind()
{
    if (!isVisible()) openPanel();
    if (!m_previewFind) return;
    positionPreviewFindBar();
    m_previewFind->openBar();
    m_previewFind->raise();
}

void RefMenuPanel::positionPreviewFindBar()
{
    if (!m_previewFind || !m_previewWrap) return;
    m_previewFind->adjustSize();
    const int margin = 8;
    const int w = qMax(280, m_previewFind->sizeHint().width());
    const int h = m_previewFind->height();
    m_previewFind->resize(w, h);
    // Coordenadas do canto sup. direito do previewWrap relativas ao painel.
    const QPoint topRight = m_previewWrap->mapTo(this, QPoint(m_previewWrap->width(), 0));
    int x = topRight.x() - w - margin;
    int y = topRight.y() + margin;
    x = qMax(margin, x);
    m_previewFind->move(x, y);
}

bool RefMenuPanel::matchesSearch(const QString& text) const
{
    if (m_searchQuery.isEmpty()) return true;
    return text.contains(m_searchQuery, Qt::CaseInsensitive);
}


void RefMenuPanel::onCycleFontSize()
{
    // 11 → 13 → 15 → 17 → 11
    int next = m_previewFontPt + 2;
    if (next > 17) next = 11;
    m_previewFontPt = next;
    applyPreviewFont();
    QSettings().setValue(QString::fromLatin1(kKeyFontPt), m_previewFontPt);
}

void RefMenuPanel::onToggleVisualMode()
{
    m_visualMode = !m_visualMode;
    rebuildCrumbs();
    rebuildNavBody();
    QSettings().setValue(QString::fromLatin1(kKeyVisualMode), m_visualMode);
}

// =========================================================================
// Helpers (drawer/visual heuristic)
// =========================================================================

bool RefMenuPanel::drawerIsVisual(const Drawer* d) const
{
    if (!d) return false;
    const QString t = d->drawerElementType.toLower();
    if (t.isEmpty()) return false;
    QRegularExpression re(QStringLiteral("personagem|character|cen[aá]rios?|setting|objeto|object"),
                          QRegularExpression::CaseInsensitiveOption);
    return re.match(t).hasMatch();
}

QString RefMenuPanel::roleOrLabelForItem(const DrawerItem& it) const
{
    if (!it.role.isEmpty()) return RoleTiers::roleDisplayName(it.role);
    if (m_elements && !it.elementId.isEmpty()) {
        const Element* el = m_elements->findElement(it.elementId);
        if (el && !el->role.isEmpty()) return RoleTiers::roleDisplayName(el->role);
    }
    return QString();
}

QString RefMenuPanel::imageForItem(const DrawerItem& it) const
{
    if (!m_elements) return QString();
    if (it.elementId.isEmpty()) return QString();
    const Element* el = m_elements->findElement(it.elementId);
    if (!el) return QString();
    return el->image;
}

// =========================================================================
// Drag livre via ⠿ + Resize livre via handles dedicados (estilo Drawer)
// =========================================================================

void RefMenuPanel::moveEvent(QMoveEvent* event)
{
    QWidget::moveEvent(event);
    emit geometryChanged();
    scheduleGeometrySave();
}

void RefMenuPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutResizeHandles();
    if (m_previewFind && m_previewFind->isVisible()) positionPreviewFindBar();
    QTimer::singleShot(0, this, [this]() { rescalePreviewImages(); });
    emit geometryChanged();
    scheduleGeometrySave();
}

void RefMenuPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // Garante que se a geometria salva ficou fora de qualquer tela (monitor
    // desconectado, etc.), recolocamos dentro da tela mais próxima.
    if (const QScreen* scr = screen()) {
        QRect sg = scr->availableGeometry();
        QRect g = geometry();
        if (g.right() > sg.right()) g.moveRight(sg.right() - 4);
        if (g.bottom() > sg.bottom()) g.moveBottom(sg.bottom() - 4);
        if (g.left() < sg.left() + 4) g.moveLeft(sg.left() + 4);
        if (g.top() < sg.top() + 4) g.moveTop(sg.top() + 4);
        if (g != geometry()) setGeometry(g);
    }
}

// Event filter: handles de resize (8 widgets) + drag handle ⠿.
bool RefMenuPanel::eventFilter(QObject* watched, QEvent* event)
{
    // Linhas da tela inicial: abrir documento, ou entrar numa fonte.
    // Ficam marcadas por propriedade em vez de widget dedicado, pra que uma
    // linha nova em qualquer seção funcione sem fiação extra.
    if (event->type() == QEvent::MouseButtonRelease) {
        if (auto* w = qobject_cast<QWidget*>(watched)) {
            const QVariant rowKey = w->property("refRowKey");
            if (rowKey.isValid() && !rowKey.toString().isEmpty()) {
                const QString key = rowKey.toString();
                // NAO troca de modo: abrir um documento mantem a arvore na
                // tela, com os galhos como estavam. Trocar pra Section aqui era
                // o que fazia a navegacao cair na view antiga, de caixa bordada.
                setSelected(key);
                addRecent(key);
                rebuildCrumbs();
                return true;
            }
            // Galho da árvore: abre/fecha ali mesmo, sem trocar de tela.
            const QVariant expandVar = w->property("refExpandKey");
            if (expandVar.isValid() && !expandVar.toString().isEmpty()) {
                const QString ek = expandVar.toString();
                if (m_expanded.contains(ek)) m_expanded.remove(ek);
                else m_expanded.insert(ek);
                rebuildNavBody();
                return true;
            }
            const QVariant kindVar = w->property("refSectionKind");
            if (kindVar.isValid()) {
                enterSection(static_cast<SourceKind>(kindVar.toInt()),
                             w->property("refSectionKey").toString());
                return true;
            }
        }
    }

    // Mapa watched -> ResizeEdge.
    auto edgeOf = [this](QObject* o) -> ResizeEdge {
        if (o == m_hL)  return ResizeEdge::Left;
        if (o == m_hR)  return ResizeEdge::Right;
        if (o == m_hT)  return ResizeEdge::Top;
        if (o == m_hB)  return ResizeEdge::Bottom;
        if (o == m_hTL) return ResizeEdge::TL;
        if (o == m_hTR) return ResizeEdge::TR;
        if (o == m_hBL) return ResizeEdge::BL;
        if (o == m_hBR) return ResizeEdge::BR;
        return ResizeEdge::None;
    };
    ResizeEdge edge = edgeOf(watched);
    if (edge != ResizeEdge::None) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_resizing = true;
                m_activeEdge = edge;
                m_resizeStartGeom = geometry();
                m_resizeStartMouse = me->globalPosition().toPoint();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (m_resizing) {
                auto* me = static_cast<QMouseEvent*>(event);
                const QPoint g = me->globalPosition().toPoint();
                const int dx = g.x() - m_resizeStartMouse.x();
                const int dy = g.y() - m_resizeStartMouse.y();
                int newX = m_resizeStartGeom.x();
                int newY = m_resizeStartGeom.y();
                int newW = m_resizeStartGeom.width();
                int newH = m_resizeStartGeom.height();

                switch (m_activeEdge) {
                    case ResizeEdge::Left:
                        newW = m_resizeStartGeom.width() - dx;
                        newX = m_resizeStartGeom.x() + dx;
                        break;
                    case ResizeEdge::Right:
                        newW = m_resizeStartGeom.width() + dx;
                        break;
                    case ResizeEdge::Top:
                        newH = m_resizeStartGeom.height() - dy;
                        newY = m_resizeStartGeom.y() + dy;
                        break;
                    case ResizeEdge::Bottom:
                        newH = m_resizeStartGeom.height() + dy;
                        break;
                    case ResizeEdge::TL:
                        newW = m_resizeStartGeom.width() - dx;
                        newH = m_resizeStartGeom.height() - dy;
                        newX = m_resizeStartGeom.x() + dx;
                        newY = m_resizeStartGeom.y() + dy;
                        break;
                    case ResizeEdge::TR:
                        newW = m_resizeStartGeom.width() + dx;
                        newH = m_resizeStartGeom.height() - dy;
                        newY = m_resizeStartGeom.y() + dy;
                        break;
                    case ResizeEdge::BL:
                        newW = m_resizeStartGeom.width() - dx;
                        newH = m_resizeStartGeom.height() + dy;
                        newX = m_resizeStartGeom.x() + dx;
                        break;
                    case ResizeEdge::BR:
                        newW = m_resizeStartGeom.width() + dx;
                        newH = m_resizeStartGeom.height() + dy;
                        break;
                    default: break;
                }
                // Clamp pelos mínimos sem deixar a borda fixa pular.
                if (newW < kMinW) {
                    if (m_activeEdge == ResizeEdge::Left || m_activeEdge == ResizeEdge::TL || m_activeEdge == ResizeEdge::BL) {
                        newX = m_resizeStartGeom.right() - kMinW + 1;
                    }
                    newW = kMinW;
                }
                if (newH < kMinH) {
                    if (m_activeEdge == ResizeEdge::Top || m_activeEdge == ResizeEdge::TL || m_activeEdge == ResizeEdge::TR) {
                        newY = m_resizeStartGeom.bottom() - kMinH + 1;
                    }
                    newH = kMinH;
                }
                setGeometry(newX, newY, newW, newH);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_resizing) {
                m_resizing = false;
                m_activeEdge = ResizeEdge::None;
                scheduleGeometrySave();
                return true;
            }
        }
    }

    if (watched == m_dragHandle) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragOffset = me->globalPosition().toPoint() - pos();
                m_dragHandle->setCursor(Qt::ClosedHandCursor);
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (m_dragging) {
                auto* me = static_cast<QMouseEvent*>(event);
                const QPoint newPos = me->globalPosition().toPoint() - m_dragOffset;
                move(newPos);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_dragging) {
                m_dragging = false;
                m_dragHandle->setCursor(Qt::OpenHandCursor);
                scheduleGeometrySave();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            // duplo clique no drag handle → reseta posição/tamanho ao canto
            // direito da janela principal do app.
            if (auto* p = parentWidget()) {
                const QRect pg = p->window()->geometry();
                const int margin = 12;
                resize(kDefaultW, qMax(kMinH, pg.height() - margin * 2));
                move(pg.right() - width() - margin, pg.top() + margin);
                scheduleGeometrySave();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// =========================================================================
// Persistência QSettings
// =========================================================================

void RefMenuPanel::loadGeometryFromSettings()
{
    QSettings s;
    if (m_bodySplit) {
        const QByteArray st = s.value(QString::fromLatin1(kKeySplit)).toByteArray();
        if (!st.isEmpty()) m_bodySplit->restoreState(st);
    }
    QVariant geomVar = s.value(QString::fromLatin1(kKeyGeom));
    if (geomVar.isValid()) {
        const QByteArray ba = geomVar.toByteArray();
        if (!ba.isEmpty()) restoreGeometry(ba);
    }
    m_navHidden = s.value(QString::fromLatin1(kKeyNavHidden), false).toBool();
    m_visualMode = s.value(QString::fromLatin1(kKeyVisualMode), true).toBool();
    m_pinned = s.value(QString::fromLatin1(kKeyPinned), false).toBool();
    m_previewFontPt = qBound(9, s.value(QString::fromLatin1(kKeyFontPt), 13).toInt(), 24);

    if (m_pinBtn) {
        QSignalBlocker b(m_pinBtn);
        m_pinBtn->setChecked(m_pinned);
    }
    if (m_toggleNavBtn) {
        m_toggleNavBtn->setText(m_navHidden ? QStringLiteral("▸") : QStringLiteral("▾"));
    }
}

void RefMenuPanel::saveGeometryToSettings()
{
    QSettings s;
    s.setValue(QString::fromLatin1(kKeyGeom), saveGeometry());
    // A divisao entre arvore e texto e' preferencia: sobrevive ao fechar.
    if (m_bodySplit) s.setValue(QString::fromLatin1(kKeySplit), m_bodySplit->saveState());
}

void RefMenuPanel::scheduleGeometrySave()
{
    if (m_savePending) return;
    m_savePending = true;
    QTimer::singleShot(400, this, [this]() {
        m_savePending = false;
        saveGeometryToSettings();
    });
}
