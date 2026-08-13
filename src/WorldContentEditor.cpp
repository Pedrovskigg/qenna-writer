#include "WorldContentEditor.h"
#include "EditorLayout.h"
#include "IconUtils.h"
#include "ImageOverlay.h"
#include "Theme.h"

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QButtonGroup>
#include <QComboBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QImage>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStyle>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QTextObjectInterface>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidgetAction>

namespace {

// Substitui o handler padrão de imagem do Qt pra desenhar com
// SmoothPixmapTransform (sem isso, redimensionar via overlay deixa a imagem
// serrilhada). Espelha ConstrutorImageHandler de ConstrutorWindow.cpp.
class WorldImageHandler : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)
public:
    explicit WorldImageHandler(QObject* parent = nullptr) : QObject(parent) {}

    QSizeF intrinsicSize(QTextDocument* doc, int /*pos*/, const QTextFormat& format) override
    {
        const QTextImageFormat fmt = format.toImageFormat();
        const bool hasW = fmt.hasProperty(QTextFormat::ImageWidth);
        const bool hasH = fmt.hasProperty(QTextFormat::ImageHeight);
        if (hasW && hasH) return QSizeF(fmt.width(), fmt.height());

        const QSize nat = naturalSize(doc, fmt);
        if (hasW && nat.height() > 0)
            return QSizeF(fmt.width(), fmt.width() * nat.height() / nat.width());
        if (hasH && nat.width() > 0)
            return QSizeF(fmt.height() * nat.width() / nat.height(), fmt.height());
        return QSizeF(nat);
    }

    void drawObject(QPainter* painter, const QRectF& rect, QTextDocument* doc,
                     int /*pos*/, const QTextFormat& format) override
    {
        const QImage image = resourceImage(doc, format.toImageFormat());
        if (image.isNull()) return;
        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->drawImage(rect, image, image.rect());
        painter->restore();
    }

private:
    static QImage resourceImage(QTextDocument* doc, const QTextImageFormat& fmt)
    {
        const QVariant data = doc->resource(QTextDocument::ImageResource, QUrl(fmt.name()));
        if (data.userType() == QMetaType::QPixmap) return qvariant_cast<QPixmap>(data).toImage();
        if (data.userType() == QMetaType::QImage)  return qvariant_cast<QImage>(data);
        if (data.userType() == QMetaType::QByteArray) {
            QImage img;
            img.loadFromData(data.toByteArray());
            return img;
        }
        return {};
    }

    static QSize naturalSize(QTextDocument* doc, const QTextImageFormat& fmt)
    {
        const QSize sz = resourceImage(doc, fmt).size();
        return sz.isEmpty() ? QSize(100, 100) : sz;
    }
};
#include "WorldContentEditor.moc"

// Parseia "rgba(r,g,b,a)" — espelha a função homônima de MainWindow.cpp/ConstrutorWindow.cpp.
QColor parseColor(const QString& s)
{
    if (!s.startsWith(QLatin1String("rgba("))) return QColor(s);
    QString inner = s.mid(5);
    if (inner.endsWith(QChar(')'))) inner.chop(1);
    const QStringList parts = inner.split(QChar(','));
    if (parts.size() != 4) return QColor(s);
    bool ok = false;
    const int r = parts.at(0).trimmed().toInt(&ok); if (!ok) return QColor();
    const int g = parts.at(1).trimmed().toInt(&ok); if (!ok) return QColor();
    const int b = parts.at(2).trimmed().toInt(&ok); if (!ok) return QColor();
    const int a = parts.at(3).trimmed().toInt(&ok); if (!ok) return QColor();
    return QColor(r, g, b, a);
}

constexpr int kSaveDelay = 600; // ms debounce
constexpr auto kSettingsPrefix = "worldContentEditor/";

} // namespace

WorldContentEditor::WorldContentEditor(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    applyPageLayout();
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &WorldContentEditor::applyTheme);
    connect(EditorLayout::Manager::instance(), &EditorLayout::Manager::layoutChanged,
            this, &WorldContentEditor::applyPageLayout);

    m_firstLineIndentEnabled = QSettings()
        .value(QStringLiteral("%1firstLineIndent").arg(kSettingsPrefix), false).toBool();
    if (m_indentBtn) {
        QSignalBlocker block(m_indentBtn);
        m_indentBtn->setChecked(m_firstLineIndentEnabled);
    }
    m_lineHeightPercent = QSettings()
        .value(QStringLiteral("%1lineHeightPercent").arg(kSettingsPrefix), m_lineHeightPercent).toInt();
    m_paraSpaceBefore = QSettings()
        .value(QStringLiteral("%1paraSpaceBefore").arg(kSettingsPrefix), m_paraSpaceBefore).toInt();
    m_paraSpaceAfter = QSettings()
        .value(QStringLiteral("%1paraSpaceAfter").arg(kSettingsPrefix), m_paraSpaceAfter).toInt();
    updateLineHeightMenuChecks();
    if (m_paraBeforeLabel) m_paraBeforeLabel->setText(QStringLiteral("%1 px").arg(m_paraSpaceBefore));
    if (m_paraAfterLabel)  m_paraAfterLabel->setText(QStringLiteral("%1 px").arg(m_paraSpaceAfter));

    const bool savedFocus = QSettings()
        .value(QStringLiteral("%1focusMode").arg(kSettingsPrefix), false).toBool();
    if (savedFocus) setFocusModeEnabled(true);
}

// ── UI ────────────────────────────────────────────────────────────────────────

void WorldContentEditor::buildUi()
{
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(kSaveDelay);
    connect(m_saveTimer, &QTimer::timeout, this, &WorldContentEditor::contentChanged);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("wceToolbar"));
    auto* tbLay = new QHBoxLayout(toolbar);
    tbLay->setContentsMargins(8, 4, 8, 4);
    tbLay->setSpacing(2);

    auto makeSep = [&](QWidget* parent) {
        auto* s = new QFrame(parent);
        s->setFrameShape(QFrame::VLine);
        s->setObjectName(QStringLiteral("wceToolbarSep"));
        return s;
    };
    auto makeFmtBtn = [&](const QString& text, QWidget* parent, bool checkable = false) {
        auto* btn = new QPushButton(text, parent);
        btn->setObjectName(QStringLiteral("wceFmtBtn"));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setCheckable(checkable);
        btn->setFixedSize(28, 26);
        return btn;
    };

    m_boldBtn = makeFmtBtn(QStringLiteral("B"), toolbar, true);
    m_boldBtn->setToolTip(tr("Negrito (Ctrl+B)"));
    m_boldBtn->setShortcut(QKeySequence::Bold);
    { QFont f = m_boldBtn->font(); f.setBold(true); m_boldBtn->setFont(f); }
    tbLay->addWidget(m_boldBtn);

    m_italicBtn = makeFmtBtn(QStringLiteral("I"), toolbar, true);
    m_italicBtn->setToolTip(tr("Itálico (Ctrl+I)"));
    m_italicBtn->setShortcut(QKeySequence::Italic);
    { QFont f = m_italicBtn->font(); f.setItalic(true); m_italicBtn->setFont(f); }
    tbLay->addWidget(m_italicBtn);

    m_underlineBtn = makeFmtBtn(QStringLiteral("U"), toolbar, true);
    m_underlineBtn->setToolTip(tr("Sublinhado (Ctrl+U)"));
    m_underlineBtn->setShortcut(QKeySequence::Underline);
    { QFont f = m_underlineBtn->font(); f.setUnderline(true); m_underlineBtn->setFont(f); }
    tbLay->addWidget(m_underlineBtn);

    m_strikeBtn = makeFmtBtn(QStringLiteral("S"), toolbar, true);
    m_strikeBtn->setToolTip(tr("Tachado"));
    { QFont f = m_strikeBtn->font(); f.setStrikeOut(true); m_strikeBtn->setFont(f); }
    tbLay->addWidget(m_strikeBtn);

    m_indentBtn = makeFmtBtn(QStringLiteral("¶"), toolbar, true);
    m_indentBtn->setToolTip(tr("Indentar primeira linha do parágrafo"));
    tbLay->addWidget(m_indentBtn);

    tbLay->addWidget(makeSep(toolbar));

    m_fontCombo = new QFontComboBox(toolbar);
    m_fontCombo->setObjectName(QStringLiteral("wceFontCombo"));
    m_fontCombo->setMaximumWidth(160);
    tbLay->addWidget(m_fontCombo);

    m_sizeCombo = new QComboBox(toolbar);
    m_sizeCombo->setObjectName(QStringLiteral("wceSizeCombo"));
    m_sizeCombo->setEditable(true);
    m_sizeCombo->setMinimumWidth(64);
    m_sizeCombo->setMaximumWidth(72);
    for (int s : {8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 22, 24, 28, 32, 36, 48, 72})
        m_sizeCombo->addItem(QString::number(s));
    m_sizeCombo->setCurrentText(QStringLiteral("16"));
    tbLay->addWidget(m_sizeCombo);

    m_spacingBtn = makeFmtBtn(QString(), toolbar);
    m_spacingBtn->setToolTip(tr("Espaçamento de linhas e parágrafos"));
    m_spacingBtn->setIconSize(QSize(16, 16));
    buildSpacingMenu();
    tbLay->addWidget(m_spacingBtn);

    tbLay->addWidget(makeSep(toolbar));

    m_alignGroup = new QButtonGroup(this);
    m_alignGroup->setExclusive(true);
    m_alignLeftBtn   = makeFmtBtn(QString(), toolbar, true);
    m_alignLeftBtn->setToolTip(tr("Alinhar à esquerda"));
    m_alignLeftBtn->setIconSize(QSize(16, 16));
    m_alignCenterBtn = makeFmtBtn(QString(), toolbar, true);
    m_alignCenterBtn->setToolTip(tr("Centralizar"));
    m_alignCenterBtn->setIconSize(QSize(16, 16));
    m_alignRightBtn  = makeFmtBtn(QString(), toolbar, true);
    m_alignRightBtn->setToolTip(tr("Alinhar à direita"));
    m_alignRightBtn->setIconSize(QSize(16, 16));
    m_alignLeftBtn->setChecked(true);
    m_alignGroup->addButton(m_alignLeftBtn);
    m_alignGroup->addButton(m_alignCenterBtn);
    m_alignGroup->addButton(m_alignRightBtn);
    tbLay->addWidget(m_alignLeftBtn);
    tbLay->addWidget(m_alignCenterBtn);
    tbLay->addWidget(m_alignRightBtn);

    tbLay->addWidget(makeSep(toolbar));

    m_focusBtn = makeFmtBtn(QString(), toolbar, true);
    m_focusBtn->setToolTip(tr("Modo foco"));
    m_focusBtn->setIconSize(QSize(16, 16));
    tbLay->addWidget(m_focusBtn);

    m_insertImageBtn = makeFmtBtn(QString(), toolbar);
    m_insertImageBtn->setToolTip(tr("Inserir imagem"));
    m_insertImageBtn->setIconSize(QSize(16, 16));
    tbLay->addWidget(m_insertImageBtn);

    tbLay->addStretch();

    m_statusLabel = new QLabel(toolbar);
    m_statusLabel->setObjectName(QStringLiteral("wceStatusLabel"));
    tbLay->addWidget(m_statusLabel);

    root->addWidget(toolbar);

    m_contentEdit = new QTextEdit();
    m_contentEdit->setObjectName(QStringLiteral("wceContentEdit"));
    m_contentEdit->setFrameShape(QFrame::NoFrame);
    m_contentEdit->setEnabled(false);
    m_contentEdit->setAcceptRichText(true);
    m_contentEdit->document()->setDefaultStyleSheet(
        QStringLiteral("p { margin: 0 0 10px 0; line-height: 1.7; }"));
    { QFont f; f.setPointSize(16); m_contentEdit->document()->setDefaultFont(f); }
    m_contentEdit->document()->documentLayout()->registerHandler(
        QTextFormat::ImageObject, new WorldImageHandler(m_contentEdit));

    m_imageOverlay = new ImageOverlay(m_contentEdit->viewport(), /*showAlignment=*/false);
    m_imageOverlay->hide();
    connect(m_imageOverlay, &ImageOverlay::widthChangeRequested,
            this, &WorldContentEditor::changeSelectedImageWidth);
    m_contentEdit->viewport()->installEventFilter(this);
    connect(m_contentEdit->verticalScrollBar(), &QAbstractSlider::valueChanged,
            this, &WorldContentEditor::hideOverlay);

    m_pageColumn = new QWidget();
    m_pageColumn->setObjectName(QStringLiteral("wcePageColumn"));
    auto* pageLay = new QVBoxLayout(m_pageColumn);
    pageLay->addWidget(m_contentEdit, 1);

    m_pageScroll = new QScrollArea(this);
    m_pageScroll->setObjectName(QStringLiteral("wcePageScroll"));
    m_pageScroll->setFrameShape(QFrame::NoFrame);
    m_pageScroll->setWidgetResizable(true);
    m_pageScroll->setAlignment(Qt::AlignHCenter);
    m_pageScroll->setWidget(m_pageColumn);
    root->addWidget(m_pageScroll, 1);

    // Scrollbar externa, solta na borda direita do painel — a nativa do
    // QTextEdit fica colada na "página" (largura fixa, quase sempre bem mais
    // estreita que o painel inteiro), longe da borda de verdade.
    m_contentEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_externalScrollBar = new QScrollBar(Qt::Vertical, this);
    m_externalScrollBar->setObjectName(QStringLiteral("wceExternalScroll"));
    auto* innerSb = m_contentEdit->verticalScrollBar();
    m_externalScrollBar->setRange(innerSb->minimum(), innerSb->maximum());
    m_externalScrollBar->setPageStep(innerSb->pageStep());
    m_externalScrollBar->setSingleStep(innerSb->singleStep());
    m_externalScrollBar->setValue(innerSb->value());
    connect(innerSb, &QAbstractSlider::rangeChanged, this, [this, innerSb]() {
        m_externalScrollBar->setRange(innerSb->minimum(), innerSb->maximum());
        m_externalScrollBar->setPageStep(innerSb->pageStep());
        m_externalScrollBar->setSingleStep(innerSb->singleStep());
        positionExternalScrollBar();
    });
    connect(innerSb, &QAbstractSlider::valueChanged, m_externalScrollBar, &QScrollBar::setValue);
    connect(m_externalScrollBar, &QAbstractSlider::valueChanged, innerSb, &QScrollBar::setValue);

    connect(m_focusBtn, &QPushButton::toggled, this, &WorldContentEditor::setFocusModeEnabled);

    connect(m_contentEdit, &QTextEdit::textChanged,
            this, &WorldContentEditor::onTextChanged);
    connect(m_contentEdit, &QTextEdit::currentCharFormatChanged,
            this, &WorldContentEditor::onCurrentCharFormatChanged);
    connect(m_contentEdit, &QTextEdit::cursorPositionChanged, this, [this]() {
        if (!m_updatingFmt && m_contentEdit->isEnabled())
            updateToolbarState(m_contentEdit->currentCharFormat());
    });

    connect(m_boldBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_updatingFmt || !m_contentEdit->isEnabled()) return;
        QTextCharFormat fmt;
        fmt.setFontWeight(on ? QFont::Bold : QFont::Normal);
        m_contentEdit->mergeCurrentCharFormat(fmt);
    });
    connect(m_italicBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_updatingFmt || !m_contentEdit->isEnabled()) return;
        QTextCharFormat fmt;
        fmt.setFontItalic(on);
        m_contentEdit->mergeCurrentCharFormat(fmt);
    });
    connect(m_underlineBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_updatingFmt || !m_contentEdit->isEnabled()) return;
        QTextCharFormat fmt;
        fmt.setFontUnderline(on);
        m_contentEdit->mergeCurrentCharFormat(fmt);
    });
    connect(m_strikeBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_updatingFmt || !m_contentEdit->isEnabled()) return;
        QTextCharFormat fmt;
        fmt.setFontStrikeOut(on);
        m_contentEdit->mergeCurrentCharFormat(fmt);
    });
    connect(m_indentBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_updatingFmt) return;
        m_firstLineIndentEnabled = on;
        QSettings().setValue(QStringLiteral("%1firstLineIndent").arg(kSettingsPrefix), on);
        if (!m_contentEdit->isEnabled()) return;
        QTextCursor c(m_contentEdit->document());
        c.select(QTextCursor::Document);
        QTextBlockFormat bfmt;
        bfmt.setTextIndent(on ? 24.0 : 0.0);
        c.mergeBlockFormat(bfmt);
        QTextCursor cur = m_contentEdit->textCursor();
        cur.mergeBlockFormat(bfmt);
        m_contentEdit->setTextCursor(cur);
        m_contentEdit->setFocus();
    });
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont& f) {
        if (m_updatingFmt || !m_contentEdit->isEnabled()) return;
        QTextCursor c(m_contentEdit->document());
        c.select(QTextCursor::Document);
        QTextCharFormat fmt;
        fmt.setFontFamilies({f.family()});
        c.mergeCharFormat(fmt);
        m_contentEdit->mergeCurrentCharFormat(fmt);
        QFont docFont = m_contentEdit->document()->defaultFont();
        docFont.setFamily(f.family());
        m_contentEdit->document()->setDefaultFont(docFont);
        m_contentEdit->setFocus();
    });
    connect(m_sizeCombo, &QComboBox::currentTextChanged, this, [this](const QString& s) {
        if (m_updatingFmt || !m_contentEdit->isEnabled()) return;
        bool ok; const qreal sz = s.toDouble(&ok);
        if (!ok || sz <= 0) return;
        QTextCursor c(m_contentEdit->document());
        c.select(QTextCursor::Document);
        QTextCharFormat fmt;
        fmt.setFontPointSize(sz);
        c.mergeCharFormat(fmt);
        m_contentEdit->mergeCurrentCharFormat(fmt);
        QFont docFont = m_contentEdit->document()->defaultFont();
        docFont.setPointSize(static_cast<int>(sz));
        m_contentEdit->document()->setDefaultFont(docFont);
        m_contentEdit->setFocus();
    });
    connect(m_alignLeftBtn, &QPushButton::clicked, this, [this]() {
        if (m_contentEdit->isEnabled()) applyGlobalAlignment(Qt::AlignLeft);
    });
    connect(m_alignCenterBtn, &QPushButton::clicked, this, [this]() {
        if (m_contentEdit->isEnabled()) applyGlobalAlignment(Qt::AlignHCenter);
    });
    connect(m_alignRightBtn, &QPushButton::clicked, this, [this]() {
        if (m_contentEdit->isEnabled()) applyGlobalAlignment(Qt::AlignRight);
    });
    connect(m_insertImageBtn, &QPushButton::clicked,
            this, &WorldContentEditor::onInsertImage);
}

bool WorldContentEditor::eventFilter(QObject* watched, QEvent* event)
{
    if (m_contentEdit && watched == m_contentEdit->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            QTextCursor imageCursor;
            if (findImageAt(me->pos(), imageCursor)) {
                showOverlayForImage(imageCursor);
                return true;
            }
            hideOverlay();
        } else if (event->type() == QEvent::Resize) {
            hideOverlay();
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ── Theme / layout ────────────────────────────────────────────────────────────

void WorldContentEditor::applyTheme()
{
    const QString panelBg   = Theme::panelBackground();
    const QString border    = Theme::panelBorder();
    const QString txtPrim   = Theme::textPrimary();
    const QString txtMuted  = Theme::textMuted();
    const QString txtBright = Theme::textBright();
    const QString hover     = Theme::hoverOverlay();
    const QString accentSf  = Theme::accentInfoSoft();
    const QString accentBd  = Theme::accentInfoBorderSoft();
    const QString accentDef = Theme::accentDefault();
    const QString disabled  = Theme::disabledText();
    const QString editorBg  = Theme::editorBackground();
    const QColor  editorTxt = QColor(Theme::editorTextColor());

    setStyleSheet(QStringLiteral(R"(
        QWidget#wceToolbar { background: %1; border-bottom: 1px solid %2; }
        QPushButton#wceFmtBtn {
            background: transparent; color: %3;
            border: 1px solid transparent; border-radius: 5px; font-size: 13px;
        }
        QPushButton#wceFmtBtn:hover { background: %5; border-color: %2; color: %4; }
        QPushButton#wceFmtBtn:checked { background: %6; border-color: %7; color: %4; }
        QPushButton#wceFmtBtn:pressed { background: %7; }
        QPushButton#wceFmtBtn::menu-indicator { image: none; width: 0; }
        QFrame#wceToolbarSep { background: %2; border: none; max-width: 1px; margin: 2px 3px; }
        QFontComboBox#wceFontCombo, QComboBox#wceSizeCombo {
            background: %1; color: %3;
            border: 1px solid %8; border-radius: 5px;
            padding: 2px 4px; font-size: 12px;
            selection-background-color: %6;
        }
        QFontComboBox#wceFontCombo:focus, QComboBox#wceSizeCombo:focus { border-color: %7; }
        QComboBox#wceSizeCombo::drop-down { width: 14px; border: none; }
        QLabel#wceStatusLabel { color: %9; font-size: 10px; padding: 0 4px; }
        QMenu#wceSpacingMenu {
            background: %1; color: %3;
            border: 1px solid %8; border-radius: 8px; padding: 6px;
        }
        QMenu#wceSpacingMenu::item { padding: 5px 10px; border-radius: 5px; font-size: 12px; }
        QMenu#wceSpacingMenu::item:selected { background: %5; color: %4; }
        QMenu#wceSpacingMenu::item:disabled {
            color: %9; font-size: 10px; font-weight: 700; letter-spacing: 0.5px;
        }
        QMenu#wceSpacingMenu::separator { height: 1px; background: %2; margin: 4px 4px; }

        QScrollArea#wcePageScroll { background: transparent; border: none; }
        QWidget#wcePageColumn { background: %10; }
        QTextEdit#wceContentEdit { background: %10; border: none; selection-background-color: %6; }
        QTextEdit#wceContentEdit:disabled { color: %11; }

        QScrollBar#wceExternalScroll:vertical { background: transparent; width: 8px; margin: 0; }
        QScrollBar#wceExternalScroll::handle:vertical { background: %8; border-radius: 4px; min-height: 24px; }
        QScrollBar#wceExternalScroll::handle:vertical:hover { background: %9; }
        QScrollBar#wceExternalScroll::add-line:vertical, QScrollBar#wceExternalScroll::sub-line:vertical { height: 0; }
        QScrollBar#wceExternalScroll::add-page:vertical, QScrollBar#wceExternalScroll::sub-page:vertical { background: transparent; }
    )").arg(panelBg, border, txtPrim, txtBright)   // %1-4
       .arg(hover, accentSf, accentBd)              // %5-7
       .arg(Theme::subtleBorder(), txtMuted)        // %8-9
       .arg(editorBg, disabled));                   // %10-11

    m_baseTextColor = editorTxt;
    if (m_contentEdit) {
        QPalette p = m_contentEdit->palette();
        p.setColor(QPalette::Base, QColor(editorBg));
        m_contentEdit->setPalette(p);
    }
    applyFocusTextColor();

    if (m_pageColumn) {
        if (Theme::pageShadowEnabled()) {
            auto* effect = qobject_cast<QGraphicsDropShadowEffect*>(m_pageColumn->graphicsEffect());
            if (!effect) {
                effect = new QGraphicsDropShadowEffect(m_pageColumn);
                m_pageColumn->setGraphicsEffect(effect);
            }
            effect->setBlurRadius(Theme::pageShadowRadius());
            effect->setOffset(0, Theme::pageShadowOffset());
            effect->setColor(parseColor(Theme::pageShadowColor()));
        } else {
            m_pageColumn->setGraphicsEffect(nullptr);
        }
    }

    auto loadIcon = [&](const QString& name) {
        return IconUtils::loadToolbarIcon(
            QStringLiteral(":/icons/") + name,
            QColor(txtMuted), QColor(txtPrim), QColor(txtBright),
            QSize(16, 16));
    };
    if (m_alignLeftBtn)   m_alignLeftBtn->setIcon(loadIcon(QStringLiteral("align-left.svg")));
    if (m_alignCenterBtn) m_alignCenterBtn->setIcon(loadIcon(QStringLiteral("align-center.svg")));
    if (m_alignRightBtn)  m_alignRightBtn->setIcon(loadIcon(QStringLiteral("align-right.svg")));
    if (m_spacingBtn)     m_spacingBtn->setIcon(loadIcon(QStringLiteral("text-spacing.svg")));
    if (m_insertImageBtn) m_insertImageBtn->setIcon(loadIcon(QStringLiteral("add-image.svg")));

    m_focusOffIcon = loadIcon(QStringLiteral("focusmode-off.svg"));
    m_focusOnIcon  = loadIcon(QStringLiteral("focusmode-on.svg"));
    if (m_focusBtn) m_focusBtn->setIcon(m_focusModeEnabled ? m_focusOnIcon : m_focusOffIcon);
}

void WorldContentEditor::applyPageLayout()
{
    if (!m_pageColumn || !m_contentEdit) return;
    const int hm = EditorLayout::horizontalMargin();
    const int vm = EditorLayout::verticalMargin();
    m_pageColumn->setFixedWidth(EditorLayout::pageWidth());
    if (auto* lay = qobject_cast<QVBoxLayout*>(m_pageColumn->layout()))
        lay->setContentsMargins(0, vm, 0, vm);
    m_contentEdit->document()->setDocumentMargin(hm);
    positionExternalScrollBar();
}

void WorldContentEditor::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    positionExternalScrollBar();
}

void WorldContentEditor::positionExternalScrollBar()
{
    if (!m_externalScrollBar || !m_pageScroll) return;
    const int sbW = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    const QPoint vpOrigin = m_pageScroll->mapTo(this, QPoint(0, 0));
    constexpr int kMargin = 4;
    m_externalScrollBar->setGeometry(
        width() - sbW - kMargin,
        vpOrigin.y(),
        sbW,
        m_pageScroll->height()
    );
    m_externalScrollBar->raise();
}

// ── Conteúdo ──────────────────────────────────────────────────────────────────

void WorldContentEditor::setContent(const QString& content)
{
    hideOverlay();
    m_selectedImageCursor = QTextCursor();
    m_contentEdit->blockSignals(true);
    m_contentEdit->setEnabled(true);
    if (content.startsWith(QLatin1String("<!DOCTYPE")))
        m_contentEdit->setHtml(content);
    else
        m_contentEdit->setPlainText(content);
    m_contentEdit->moveCursor(QTextCursor::Start);

    {
        QTextCursor c(m_contentEdit->document());
        c.select(QTextCursor::Document);
        QTextBlockFormat bf;
        bf.setTextIndent(m_firstLineIndentEnabled ? 24.0 : 0.0);
        bf.setLineHeight(m_lineHeightPercent, QTextBlockFormat::ProportionalHeight);
        bf.setTopMargin(m_paraSpaceBefore);
        bf.setBottomMargin(m_paraSpaceAfter);
        c.mergeBlockFormat(bf);
    }

    m_contentEdit->blockSignals(false);

    applyFocusTextColor();
    updateFocusedBlock();
    updateToolbarState(m_contentEdit->currentCharFormat());
}

QString WorldContentEditor::content() const
{
    return m_contentEdit ? m_contentEdit->toHtml() : QString();
}

void WorldContentEditor::setEditorEnabled(bool enabled)
{
    if (!m_contentEdit) return;
    m_saveTimer->stop();
    if (!enabled) {
        hideOverlay();
        m_contentEdit->blockSignals(true);
        m_contentEdit->clear();
        m_contentEdit->blockSignals(false);
    }
    m_contentEdit->setEnabled(enabled);
}

void WorldContentEditor::setPlaceholderText(const QString& text)
{
    if (m_contentEdit) m_contentEdit->setPlaceholderText(text);
}

void WorldContentEditor::setStatusText(const QString& text)
{
    if (m_statusLabel) m_statusLabel->setText(text);
}

void WorldContentEditor::onTextChanged()
{
    m_saveTimer->start();
}

void WorldContentEditor::onCurrentCharFormatChanged(const QTextCharFormat& fmt)
{
    if (!m_updatingFmt && m_contentEdit->isEnabled())
        updateToolbarState(fmt);
}

// ── Modo Foco ─────────────────────────────────────────────────────────────────

void WorldContentEditor::setFocusModeEnabled(bool enabled)
{
    if (m_focusModeEnabled == enabled) return;
    m_focusModeEnabled = enabled;
    QSettings().setValue(QStringLiteral("%1focusMode").arg(kSettingsPrefix), enabled);

    if (m_focusBtn) {
        QSignalBlocker block(m_focusBtn);
        m_focusBtn->setChecked(enabled);
        m_focusBtn->setIcon(enabled ? m_focusOnIcon : m_focusOffIcon);
    }

    if (m_contentEdit) {
        if (enabled) {
            connect(m_contentEdit, &QTextEdit::cursorPositionChanged,
                    this, &WorldContentEditor::updateFocusedBlock);
            connect(m_contentEdit, &QTextEdit::textChanged,
                    this, &WorldContentEditor::updateFocusedBlock);
            connect(m_contentEdit, &QTextEdit::selectionChanged,
                    this, &WorldContentEditor::updateFocusedBlock);
        } else {
            disconnect(m_contentEdit, &QTextEdit::cursorPositionChanged,
                       this, &WorldContentEditor::updateFocusedBlock);
            disconnect(m_contentEdit, &QTextEdit::textChanged,
                       this, &WorldContentEditor::updateFocusedBlock);
            disconnect(m_contentEdit, &QTextEdit::selectionChanged,
                       this, &WorldContentEditor::updateFocusedBlock);
            m_contentEdit->setExtraSelections({});
        }
    }

    applyFocusTextColor();
    if (enabled) updateFocusedBlock();
}

void WorldContentEditor::applyFocusTextColor()
{
    if (!m_contentEdit || !m_baseTextColor.isValid()) return;

    QColor textColor = m_baseTextColor;
    textColor.setAlpha(m_focusModeEnabled ? 100 : 255);

    QPalette p = m_contentEdit->palette();
    p.setColor(QPalette::Text, textColor);
    m_contentEdit->setPalette(p);

    const bool wasModified = m_contentEdit->document()->isModified();
    QTextCursor cursor(m_contentEdit->document());
    cursor.select(QTextCursor::Document);
    QTextCharFormat fmt;
    fmt.setForeground(textColor);
    cursor.mergeCharFormat(fmt);
    m_contentEdit->document()->setModified(wasModified);
}

void WorldContentEditor::updateFocusedBlock()
{
    if (!m_focusModeEnabled || !m_contentEdit) return;

    QColor focused = m_baseTextColor;
    focused.setAlpha(255);

    QTextCursor blockCursor(m_contentEdit->textCursor().block());
    blockCursor.movePosition(QTextCursor::StartOfBlock);
    blockCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    QTextEdit::ExtraSelection sel;
    sel.cursor = blockCursor;
    sel.format.setForeground(focused);
    m_contentEdit->setExtraSelections({sel});
}

// ── Formatação global (documento inteiro) ────────────────────────────────────

void WorldContentEditor::updateToolbarState(const QTextCharFormat& fmt)
{
    m_updatingFmt = true;
    m_boldBtn->setChecked(fmt.fontWeight() >= QFont::Bold);
    m_italicBtn->setChecked(fmt.fontItalic());
    m_underlineBtn->setChecked(fmt.fontUnderline());
    m_strikeBtn->setChecked(fmt.fontStrikeOut());

    const QStringList families = fmt.fontFamilies().toStringList();
    if (!families.isEmpty())
        m_fontCombo->setCurrentFont(QFont(families.first()));
    if (fmt.fontPointSize() > 0)
        m_sizeCombo->setCurrentText(QString::number(static_cast<int>(fmt.fontPointSize())));

    const Qt::Alignment align = m_contentEdit->alignment();
    m_alignLeftBtn->setChecked(align & Qt::AlignLeft || align & Qt::AlignJustify);
    m_alignCenterBtn->setChecked(align & Qt::AlignHCenter);
    m_alignRightBtn->setChecked(align & Qt::AlignRight);

    m_indentBtn->setChecked(m_firstLineIndentEnabled);

    if (m_paraBeforeLabel) m_paraBeforeLabel->setText(QStringLiteral("%1 px").arg(m_paraSpaceBefore));
    if (m_paraAfterLabel)  m_paraAfterLabel->setText(QStringLiteral("%1 px").arg(m_paraSpaceAfter));
    updateLineHeightMenuChecks();

    m_updatingFmt = false;
}

void WorldContentEditor::applyGlobalAlignment(Qt::Alignment align)
{
    QTextCursor c(m_contentEdit->document());
    c.select(QTextCursor::Document);
    QTextBlockFormat bf;
    bf.setAlignment(align);
    c.mergeBlockFormat(bf);
    QTextCursor cur = m_contentEdit->textCursor();
    cur.mergeBlockFormat(bf);
    m_contentEdit->setTextCursor(cur);
    QTextOption opt = m_contentEdit->document()->defaultTextOption();
    opt.setAlignment(align);
    m_contentEdit->document()->setDefaultTextOption(opt);
}

void WorldContentEditor::applyLineHeight(int percent)
{
    m_lineHeightPercent = percent;
    QSettings().setValue(QStringLiteral("%1lineHeightPercent").arg(kSettingsPrefix), percent);
    updateLineHeightMenuChecks();
    if (!m_contentEdit->isEnabled()) return;
    QTextCursor c(m_contentEdit->document());
    c.select(QTextCursor::Document);
    QTextBlockFormat bf;
    bf.setLineHeight(percent, QTextBlockFormat::ProportionalHeight);
    c.mergeBlockFormat(bf);
    QTextCursor cur = m_contentEdit->textCursor();
    cur.mergeBlockFormat(bf);
    m_contentEdit->setTextCursor(cur);
}

void WorldContentEditor::applyParaSpaceBefore(int px)
{
    m_paraSpaceBefore = qBound(0, px, 64);
    QSettings().setValue(QStringLiteral("%1paraSpaceBefore").arg(kSettingsPrefix), m_paraSpaceBefore);
    if (m_paraBeforeLabel) m_paraBeforeLabel->setText(QStringLiteral("%1 px").arg(m_paraSpaceBefore));
    if (!m_contentEdit->isEnabled()) return;
    QTextCursor c(m_contentEdit->document());
    c.select(QTextCursor::Document);
    QTextBlockFormat bf;
    bf.setTopMargin(m_paraSpaceBefore);
    c.mergeBlockFormat(bf);
    QTextCursor cur = m_contentEdit->textCursor();
    cur.mergeBlockFormat(bf);
    m_contentEdit->setTextCursor(cur);
}

void WorldContentEditor::applyParaSpaceAfter(int px)
{
    m_paraSpaceAfter = qBound(0, px, 64);
    QSettings().setValue(QStringLiteral("%1paraSpaceAfter").arg(kSettingsPrefix), m_paraSpaceAfter);
    if (m_paraAfterLabel) m_paraAfterLabel->setText(QStringLiteral("%1 px").arg(m_paraSpaceAfter));
    if (!m_contentEdit->isEnabled()) return;
    QTextCursor c(m_contentEdit->document());
    c.select(QTextCursor::Document);
    QTextBlockFormat bf;
    bf.setBottomMargin(m_paraSpaceAfter);
    c.mergeBlockFormat(bf);
    QTextCursor cur = m_contentEdit->textCursor();
    cur.mergeBlockFormat(bf);
    m_contentEdit->setTextCursor(cur);
}

void WorldContentEditor::updateLineHeightMenuChecks()
{
    for (QAction* a : std::as_const(m_lineHeightActions))
        a->setChecked(a->data().toInt() == m_lineHeightPercent);
}

void WorldContentEditor::buildSpacingMenu()
{
    auto* menu = new QMenu(m_spacingBtn);
    menu->setObjectName(QStringLiteral("wceSpacingMenu"));

    auto* headerLines = menu->addAction(tr("ENTRE LINHAS"));
    headerLines->setEnabled(false);

    const QList<QPair<int, QString>> presets = {
        { 100, tr("Simples (1.0)") },
        { 115, tr("Justo (1.15)") },
        { 130, tr("Compacto (1.3)") },
        { 150, tr("Confortável (1.5)") },
        { 170, tr("Padrão (1.7)") },
        { 190, tr("Amplo (1.9)") },
        { 220, tr("Espaçoso (2.2)") },
    };
    m_lineHeightActions.clear();
    for (const auto& preset : presets) {
        const int percent = preset.first;
        QAction* a = menu->addAction(preset.second);
        a->setCheckable(true);
        a->setChecked(percent == m_lineHeightPercent);
        a->setData(percent);
        connect(a, &QAction::triggered, this, [this, percent]() { applyLineHeight(percent); });
        m_lineHeightActions.append(a);
    }

    menu->addSeparator();

    auto buildStepper = [&](const QString& title, QLabel*& labelOut, int initialPx,
                             const std::function<void(int)>& apply) {
        auto* header = menu->addAction(title);
        header->setEnabled(false);

        auto* row = new QWidget(menu);
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(10, 2, 10, 6);
        lay->setSpacing(6);

        auto* minusBtn = new QPushButton(QStringLiteral("−"), row);
        minusBtn->setFixedSize(26, 26);
        minusBtn->setCursor(Qt::PointingHandCursor);

        labelOut = new QLabel(QStringLiteral("%1 px").arg(initialPx), row);
        labelOut->setAlignment(Qt::AlignCenter);
        labelOut->setFixedWidth(50);

        auto* plusBtn = new QPushButton(QStringLiteral("+"), row);
        plusBtn->setFixedSize(26, 26);
        plusBtn->setCursor(Qt::PointingHandCursor);

        lay->addWidget(minusBtn);
        lay->addWidget(labelOut);
        lay->addWidget(plusBtn);

        connect(minusBtn, &QPushButton::clicked, this, [this, apply]() { apply(-2); });
        connect(plusBtn,  &QPushButton::clicked, this, [this, apply]() { apply(2); });

        auto* act = new QWidgetAction(menu);
        act->setDefaultWidget(row);
        menu->addAction(act);
    };

    buildStepper(tr("ANTES DO PARÁGRAFO"), m_paraBeforeLabel, m_paraSpaceBefore,
                 [this](int delta) { applyParaSpaceBefore(m_paraSpaceBefore + delta); });

    menu->addSeparator();

    buildStepper(tr("DEPOIS DO PARÁGRAFO"), m_paraAfterLabel, m_paraSpaceAfter,
                 [this](int delta) { applyParaSpaceAfter(m_paraSpaceAfter + delta); });

    m_spacingBtn->setMenu(menu);
}

// ── Inserção de imagem ────────────────────────────────────────────────────────

void WorldContentEditor::onInsertImage()
{
    if (!m_contentEdit->isEnabled()) return;

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Inserir imagem"), QString(),
        tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp *.gif)"));
    if (path.isEmpty()) return;

    QImage img(path);
    if (img.isNull()) return;

    constexpr int kMaxEmbedWidth = 1200;
    if (img.width() > kMaxEmbedWidth)
        img = img.scaledToWidth(kMaxEmbedWidth, Qt::SmoothTransformation);

    QByteArray data;
    QBuffer buf(&data);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    buf.close();

    const QString src = QStringLiteral("data:image/png;base64,") + QString::fromLatin1(data.toBase64());

    QTextImageFormat fmt;
    fmt.setName(src);
    fmt.setWidth(qMin(img.width(), 600));
    m_contentEdit->textCursor().insertImage(fmt);
}

// ── Overlay de tamanho de imagem ─────────────────────────────────────────────

bool WorldContentEditor::findImageAt(const QPoint& viewportPos, QTextCursor& imageCursor) const
{
    auto* layout = m_contentEdit->document()->documentLayout();
    const int scrollY = m_contentEdit->verticalScrollBar()->value();

    for (QTextBlock block = m_contentEdit->document()->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            QTextFragment frag = it.fragment();
            if (!frag.isValid() || !frag.charFormat().isImageFormat()) continue;

            QRect visRect = layout->blockBoundingRect(block).toRect();
            visRect.translate(0, -scrollY);

            if (visRect.adjusted(-4, -4, 4, 4).contains(viewportPos)) {
                imageCursor = QTextCursor(m_contentEdit->document());
                imageCursor.setPosition(frag.position());
                imageCursor.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
                return true;
            }
        }
    }
    return false;
}

void WorldContentEditor::showOverlayForImage(const QTextCursor& imageCursor)
{
    m_selectedImageCursor = imageCursor;

    const QTextImageFormat fmt = imageCursor.charFormat().toImageFormat();
    const int imgW = static_cast<int>(fmt.width() > 0 ? fmt.width() : 320);
    m_imageOverlay->setCurrentWidth(imgW);

    auto* layout = m_contentEdit->document()->documentLayout();
    QRect visRect = layout->blockBoundingRect(imageCursor.block()).toRect();
    visRect.translate(0, -m_contentEdit->verticalScrollBar()->value());

    m_imageOverlay->adjustSize();
    int x = visRect.center().x() - m_imageOverlay->width() / 2;
    int y = visRect.top() + 4;
    if (x + m_imageOverlay->width() > m_contentEdit->viewport()->width() - 4)
        x = m_contentEdit->viewport()->width() - m_imageOverlay->width() - 4;
    if (x < 4) x = 4;
    if (y < 4) y = 4;

    m_imageOverlay->move(x, y);
    m_imageOverlay->show();
    m_imageOverlay->raise();
}

void WorldContentEditor::hideOverlay()
{
    if (m_imageOverlay) m_imageOverlay->hide();
}

void WorldContentEditor::changeSelectedImageWidth(int delta)
{
    if (m_selectedImageCursor.isNull() || !m_selectedImageCursor.charFormat().isImageFormat()) return;

    QTextImageFormat fmt = m_selectedImageCursor.charFormat().toImageFormat();
    int newWidth = static_cast<int>(fmt.width() > 0 ? fmt.width() : 320) + delta;
    newWidth = qBound(60, newWidth, 1200);
    fmt.setWidth(newWidth);
    m_selectedImageCursor.setCharFormat(fmt);

    m_imageOverlay->setCurrentWidth(newWidth);
    showOverlayForImage(m_selectedImageCursor);
}
