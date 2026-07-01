#include "ConstrutorWindow.h"
#include "EditorLayout.h"
#include "IconUtils.h"
#include "ImageOverlay.h"
#include "Theme.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QTextObjectInterface>

namespace {

// Substitui o handler padrão de imagem do Qt pra desenhar com
// SmoothPixmapTransform (sem isso, redimensionar via overlay deixa a imagem
// serrilhada). Espelha SmoothImageHandler de SpellEditor.cpp, mas lê o
// tamanho natural do recurso já em cache (base64 embutido) em vez de
// reabrir um arquivo em disco — o Construtor não referencia caminho externo.
class ConstrutorImageHandler : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)
public:
    explicit ConstrutorImageHandler(QObject* parent = nullptr) : QObject(parent) {}

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
#include "ConstrutorWindow.moc"
}

namespace {
// Parseia uma cor no formato "rgba(r,g,b,a)" com alpha 0-255 (mesmo formato
// usado pelo MiraTheme; espelha a função homônima de MainWindow.cpp).
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
}

// Delegate de dois níveis para a lista de sistemas:
//   Linha 1 — nome do sistema (13 px)
//   Linha 2 — "Categoria | Waypoint" (11 px, muted)
class SystemItemDelegate final : public QStyledItemDelegate {
public:
    QString colorPrimary  = QStringLiteral("#e0e0e0");
    QString colorMuted    = QStringLiteral("#888888");
    QString colorSelected = QStringLiteral("#f5f5f5");

    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return { 160, 50 };
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem o = opt;
        initStyleOption(&o, index);
        o.text.clear();
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &o, p, o.widget);

        const bool sel     = o.state & QStyle::State_Selected;
        const QString name = index.data(Qt::DisplayRole).toString();
        const QString sub  = index.data(Qt::UserRole + 2).toString();

        p->save();
        const int midY = o.rect.center().y();
        const QRect lr(o.rect.left() + 10, 0, o.rect.width() - 20, 0);

        QFont nf = o.font;
        nf.setPixelSize(13);
        p->setFont(nf);
        p->setPen(QColor(sel ? colorSelected : colorPrimary));
        p->drawText(QRect(lr.left(), midY - 18, lr.width(), 18),
                    Qt::AlignLeft | Qt::AlignVCenter, name);

        if (!sub.isEmpty()) {
            QFont sf = o.font;
            sf.setPixelSize(11);
            p->setFont(sf);
            p->setPen(QColor(sel ? colorPrimary : colorMuted));
            p->drawText(QRect(lr.left(), midY + 2, lr.width(), 16),
                        Qt::AlignLeft | Qt::AlignVCenter, sub);
        }
        p->restore();
    }
};

// ── Theme ──────────────────────────────────────────────────────────────────────

void ConstrutorWindow::applyTheme()
{
    // Mesma paleta do RefMenuPanel: superfícies grandes usam panelBackground
    // (escuro), nunca inputBackground (cor saturada pensada pra outro contexto
    // — estourava contraste em blocos grandes).
    const QString appBg     = Theme::appBackground();
    const QString panelBg   = Theme::panelBackground();
    const QString border    = Theme::panelBorder();
    const QString subtle    = Theme::subtleBorder();
    const QString txtPrim   = Theme::textPrimary();
    const QString txtMuted  = Theme::textMuted();
    const QString txtBright = Theme::textBright();
    const QString hover     = Theme::hoverOverlay();
    const QString accentSf  = Theme::accentInfoSoft();
    const QString accentBd  = Theme::accentInfoBorderSoft();
    const QString accentDef = Theme::accentDefault();
    const QString dangerSf  = Theme::accentDangerSoft();
    const QString danger    = Theme::accentDanger();
    const QString disabled  = Theme::disabledText();
    const QString editorBg  = Theme::editorBackground();
    const QString success   = Theme::accentSuccess();
    const QString warning   = Theme::accentWarning();
    const QColor  editorTxt = QColor(Theme::editorTextColor());

    setStyleSheet(QStringLiteral(R"(
        ConstrutorWindow { background: %1; }

        /* ── Painel esquerdo ────────────────────────────── */
        QWidget#ctrLeft { background: %2; }
        QWidget#ctrLeftHeader { background: %2; border-bottom: 1px solid %4; }
        QLabel#ctrLeftTitle {
            color: %6;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 1px;
        }
        QFrame#ctrVSep { background: %3; border: none; max-width: 1px; }

        /* ── Lista de sistemas ──────────────────────────── */
        QListWidget#ctrSystemsList {
            background: transparent;
            color: %5;
            border: none;
            font-size: 13px;
            outline: none;
        }
        QListWidget#ctrSystemsList::item { padding: 4px 8px; border-radius: 6px; }
        QListWidget#ctrSystemsList::item:hover { background: %8; color: %7; }
        QListWidget#ctrSystemsList::item:selected { background: %9; color: %7; }

        QPushButton#ctrNewSystem {
            background: %9;
            color: %7;
            border: 1px solid %10;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 12px;
            font-weight: 600;
        }
        QPushButton#ctrNewSystem:hover { background: %10; }

        /* ── Detalhe do sistema ─────────────────────────── */
        QWidget#ctrSysDetail { background: %2; }
        QFrame#ctrHSep { background: %4; border: none; max-height: 1px; }

        QLineEdit#ctrSysName {
            background: %2;
            color: %7;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 5px 8px;
            font-size: 13px;
            font-weight: 600;
        }
        QLineEdit#ctrSysName:focus { border-color: %10; }

        QLabel#ctrCatBadge {
            background: %9;
            color: %7;
            font-size: 10px;
            font-weight: 700;
            padding: 2px 8px;
            border: 1px solid %10;
            border-radius: 9px;
        }

        QPushButton#ctrDeleteSys {
            background: transparent;
            color: %6;
            border: 1px solid %3;
            border-radius: 6px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton#ctrDeleteSys:hover { background: %12; color: %13; border-color: %13; }

        QLabel#ctrWaypointName { color: %7; font-size: 12px; font-weight: 700; }
        QLabel#ctrWaypointEdge { color: %6; font-size: 10px; }
        QSlider#ctrSlider::groove:horizontal { height: 4px; background: %3; border-radius: 2px; }
        QSlider#ctrSlider::handle:horizontal {
            background: %11; border: none;
            width: 14px; height: 14px; margin: -5px 0; border-radius: 7px;
        }
        QSlider#ctrSlider::sub-page:horizontal { background: %11; border-radius: 2px; }
        QLabel#ctrWaypointTip {
            color: %6; font-size: 10px; font-style: italic; padding: 2px 0 0 0;
        }

        QLabel#ctrFavorsTitle {
            color: %16; font-size: 9px; font-weight: 700; letter-spacing: 1px;
        }
        QLabel#ctrDemandsTitle {
            color: %17; font-size: 9px; font-weight: 700; letter-spacing: 1px;
        }
        QLabel#ctrFavorsList, QLabel#ctrDemandsList {
            color: %6; font-size: 11px; padding: 2px 0;
        }
        QPushButton#ctrTradeoffExpand {
            background: transparent; color: %6; border: 1px solid %3;
            border-radius: 9px; font-size: 10px; font-weight: 700;
        }
        QPushButton#ctrTradeoffExpand:hover { background: %8; color: %7; border-color: %4; }
        QPushButton#ctrTradeoffExpand:checked { background: %9; color: %7; border-color: %10; }

        QPushButton#ctrAddRule, QPushButton#ctrAddSection {
            background: %9; color: %7; border: 1px solid %10;
            border-radius: 6px; padding: 4px 10px; font-size: 11px;
        }
        QPushButton#ctrAddRule:hover, QPushButton#ctrAddSection:hover { background: %10; }

        QPushButton#ctrDeleteNode {
            background: transparent; color: %6; border: 1px solid %3;
            border-radius: 6px; font-size: 13px; font-weight: 600;
        }
        QPushButton#ctrDeleteNode:hover { background: %12; color: %13; border-color: %13; }
        QPushButton#ctrDeleteNode:disabled { color: %14; border-color: %3; }

        /* ── Árvore de nós ──────────────────────────────── */
        QTreeWidget#ctrTree {
            background: %2; color: %5; border: none;
            font-size: 13px; outline: none;
        }
        QTreeWidget#ctrTree::item { padding: 4px 6px; border-radius: 4px; }
        QTreeWidget#ctrTree::item:hover { background: %8; color: %7; }
        QTreeWidget#ctrTree::item:selected { background: %9; color: %7; }

        /* ── Toolbar de formatação ───────────────────────── */
        QWidget#ctrToolbar {
            background: %2;
            border-bottom: 1px solid %4;
        }
        QPushButton#ctrFmtBtn {
            background: transparent;
            color: %5;
            border: 1px solid transparent;
            border-radius: 5px;
            font-size: 13px;
        }
        QPushButton#ctrFmtBtn:hover { background: %8; border-color: %4; color: %7; }
        QPushButton#ctrFmtBtn:checked {
            background: %9; border-color: %10; color: %7;
        }
        QPushButton#ctrFmtBtn:pressed { background: %10; }
        QPushButton#ctrFmtBtn::menu-indicator { image: none; width: 0; }
        QFrame#ctrToolbarSep { background: %4; border: none; max-width: 1px; margin: 2px 3px; }
        QFontComboBox#ctrFontCombo, QComboBox#ctrSizeCombo {
            background: %2; color: %5;
            border: 1px solid %3; border-radius: 5px;
            padding: 2px 4px; font-size: 12px;
            selection-background-color: %9;
        }
        QFontComboBox#ctrFontCombo:focus, QComboBox#ctrSizeCombo:focus { border-color: %10; }
        QComboBox#ctrSizeCombo::drop-down { width: 14px; border: none; }
        QMenu#ctrSpacingMenu {
            background: %2; color: %5;
            border: 1px solid %3; border-radius: 8px;
            padding: 6px;
        }
        QMenu#ctrSpacingMenu::item {
            padding: 5px 10px; border-radius: 5px; font-size: 12px;
        }
        QMenu#ctrSpacingMenu::item:selected { background: %8; color: %7; }
        QMenu#ctrSpacingMenu::item:disabled {
            color: %6; font-size: 10px; font-weight: 700; letter-spacing: 0.5px;
        }
        QMenu#ctrSpacingMenu::separator { height: 1px; background: %4; margin: 4px 4px; }

        /* ── "Página" do editor (folha centralizada) ────── */
        QScrollArea#ctrPageScroll { background: transparent; border: none; }
        QWidget#ctrPageColumn { background: %15; }
        QTextEdit#ctrContentEdit {
            background: %15;
            border: none;
            selection-background-color: %9;
        }
        QTextEdit#ctrContentEdit:disabled { color: %14; }

        /* ── Scrollbars ─────────────────────────────────── */
        QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }
        QScrollBar::handle:vertical { background: %3; border-radius: 3px; min-height: 20px; }
        QScrollBar::handle:vertical:hover { background: %6; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )").arg(appBg, panelBg, border, subtle, txtPrim)   // %1-5
       .arg(txtMuted, txtBright, hover, accentSf)      // %6-9
       .arg(accentBd, accentDef, dangerSf, danger)     // %10-13
       .arg(disabled, editorBg)                        // %14-15
       .arg(success, warning));                        // %16-17

    // Cor de texto via QPalette (não via CSS `color`) — espelha o editor
    // principal: CSS sobrepõe palette e quebraria o dim do Focus Mode.
    m_baseTextColor = editorTxt;
    if (m_contentEdit) {
        QPalette p = m_contentEdit->palette();
        p.setColor(QPalette::Base, QColor(editorBg));
        m_contentEdit->setPalette(p);
    }
    applyFocusTextColor();

    // Sombra da página — mesmo efeito/parâmetros do editor principal.
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

    if (m_sysDelegate) {
        m_sysDelegate->colorPrimary  = txtPrim;
        m_sysDelegate->colorMuted    = txtMuted;
        m_sysDelegate->colorSelected = txtBright;
        if (m_systemsList) m_systemsList->update();
    }

    // Ícones SVG de alinhamento — mesmos da TopToolbar, com cores do tema atual
    auto loadCtrIcon = [&](const QString& name) {
        return IconUtils::loadToolbarIcon(
            QStringLiteral(":/icons/") + name,
            QColor(txtMuted), QColor(txtPrim), QColor(txtBright),
            QSize(16, 16));
    };
    if (m_alignLeftBtn)   m_alignLeftBtn->setIcon(loadCtrIcon(QStringLiteral("align-left.svg")));
    if (m_alignCenterBtn) m_alignCenterBtn->setIcon(loadCtrIcon(QStringLiteral("align-center.svg")));
    if (m_alignRightBtn)  m_alignRightBtn->setIcon(loadCtrIcon(QStringLiteral("align-right.svg")));
    if (m_spacingBtn)     m_spacingBtn->setIcon(loadCtrIcon(QStringLiteral("text-spacing.svg")));
    if (m_insertImageBtn) m_insertImageBtn->setIcon(loadCtrIcon(QStringLiteral("add-image.svg")));

    // Modo foco — ícones on/off, mesmos arquivos da toolbar principal.
    m_focusOffIcon = loadCtrIcon(QStringLiteral("focusmode-off.svg"));
    m_focusOnIcon  = loadCtrIcon(QStringLiteral("focusmode-on.svg"));
    if (m_focusBtn) m_focusBtn->setIcon(m_focusModeEnabled ? m_focusOnIcon : m_focusOffIcon);
}


#include <QAction>
#include <QBuffer>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSlider>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidgetAction>

namespace {
constexpr int kNodeIdRole   = Qt::UserRole;
constexpr int kNodeTypeRole = Qt::UserRole + 1;
constexpr int kSaveDelay    = 600; // ms debounce do editor de conteúdo
}

ConstrutorWindow::ConstrutorWindow(ConstrutorStore* store, QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(tr("Construtor"));
    setMinimumSize(750, 520);
    resize(900, 620);
    buildUi();
    applyPageLayout();
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &ConstrutorWindow::applyTheme);
    connect(EditorLayout::Manager::instance(), &EditorLayout::Manager::layoutChanged,
            this, &ConstrutorWindow::applyPageLayout);

    // Restaura preferências persistidas (mesmo padrão de "editor/focusMode"
    // do editor principal — ver MainWindow::MainWindow / ::setFocusMode).
    m_firstLineIndentEnabled = QSettings()
        .value(QStringLiteral("construtor/firstLineIndent"), false).toBool();
    if (m_indentBtn) {
        QSignalBlocker block(m_indentBtn);
        m_indentBtn->setChecked(m_firstLineIndentEnabled);
    }
    m_lineHeightPercent = QSettings()
        .value(QStringLiteral("construtor/lineHeightPercent"), m_lineHeightPercent).toInt();
    m_paraSpaceBefore = QSettings()
        .value(QStringLiteral("construtor/paraSpaceBefore"), m_paraSpaceBefore).toInt();
    m_paraSpaceAfter = QSettings()
        .value(QStringLiteral("construtor/paraSpaceAfter"), m_paraSpaceAfter).toInt();
    updateLineHeightMenuChecks();
    if (m_paraBeforeLabel) m_paraBeforeLabel->setText(QStringLiteral("%1 px").arg(m_paraSpaceBefore));
    if (m_paraAfterLabel)  m_paraAfterLabel->setText(QStringLiteral("%1 px").arg(m_paraSpaceAfter));

    const bool savedFocus = QSettings()
        .value(QStringLiteral("construtor/focusMode"), false).toBool();
    if (savedFocus) setFocusMode(true);

    setStore(store);
}

void ConstrutorWindow::applyPageLayout()
{
    if (!m_pageColumn || !m_contentEdit) return;
    const int hm = EditorLayout::horizontalMargin();
    const int vm = EditorLayout::verticalMargin();
    m_pageColumn->setFixedWidth(EditorLayout::pageWidth());
    if (auto* lay = qobject_cast<QVBoxLayout*>(m_pageColumn->layout()))
        lay->setContentsMargins(0, vm, 0, vm);
    m_contentEdit->document()->setDocumentMargin(hm);
}

void ConstrutorWindow::setFocusMode(bool enabled)
{
    if (m_focusModeEnabled == enabled) return;
    m_focusModeEnabled = enabled;
    QSettings().setValue(QStringLiteral("construtor/focusMode"), enabled);

    if (m_focusBtn) {
        QSignalBlocker block(m_focusBtn);
        m_focusBtn->setChecked(enabled);
        m_focusBtn->setIcon(enabled ? m_focusOnIcon : m_focusOffIcon);
    }

    if (m_contentEdit) {
        if (enabled) {
            connect(m_contentEdit, &QTextEdit::cursorPositionChanged,
                    this, &ConstrutorWindow::updateFocusedBlock);
            connect(m_contentEdit, &QTextEdit::textChanged,
                    this, &ConstrutorWindow::updateFocusedBlock);
            connect(m_contentEdit, &QTextEdit::selectionChanged,
                    this, &ConstrutorWindow::updateFocusedBlock);
        } else {
            disconnect(m_contentEdit, &QTextEdit::cursorPositionChanged,
                       this, &ConstrutorWindow::updateFocusedBlock);
            disconnect(m_contentEdit, &QTextEdit::textChanged,
                       this, &ConstrutorWindow::updateFocusedBlock);
            disconnect(m_contentEdit, &QTextEdit::selectionChanged,
                       this, &ConstrutorWindow::updateFocusedBlock);
            m_contentEdit->setExtraSelections({});
        }
    }

    applyFocusTextColor();
    if (enabled) updateFocusedBlock();
}

void ConstrutorWindow::applyFocusTextColor()
{
    if (!m_contentEdit || !m_baseTextColor.isValid()) return;

    QColor textColor = m_baseTextColor;
    textColor.setAlpha(m_focusModeEnabled ? 100 : 255);

    QPalette p = m_contentEdit->palette();
    p.setColor(QPalette::Text, textColor);
    m_contentEdit->setPalette(p);

    // Aplica a cor no charFormat de todo o documento — palette sozinha não
    // alcança texto que já tem foreground explícito (ex.: colado de fora).
    const bool wasModified = m_contentEdit->document()->isModified();
    QTextCursor cursor(m_contentEdit->document());
    cursor.select(QTextCursor::Document);
    QTextCharFormat fmt;
    fmt.setForeground(textColor);
    cursor.mergeCharFormat(fmt);
    m_contentEdit->document()->setModified(wasModified);
}

void ConstrutorWindow::updateFocusedBlock()
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

void ConstrutorWindow::setStore(ConstrutorStore* store)
{
    if (m_store == store) return;
    if (m_store)
        disconnect(m_store, &ConstrutorStore::changed, this, &ConstrutorWindow::onStoreChanged);
    m_store = store;
    if (m_store) {
        connect(m_store, &ConstrutorStore::changed, this, &ConstrutorWindow::onStoreChanged);
        rebuildSystemsList();
    }
}

void ConstrutorWindow::closeEvent(QCloseEvent* event)
{
    saveCurrentNodeContent();
    event->accept();
}

bool ConstrutorWindow::eventFilter(QObject* watched, QEvent* event)
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

// ── UI ────────────────────────────────────────────────────────────────────────

void ConstrutorWindow::buildUi()
{
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(kSaveDelay);
    connect(m_saveTimer, &QTimer::timeout, this, &ConstrutorWindow::saveCurrentNodeContent);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Painel esquerdo colapsável ────────────────────────────────────────────
    m_leftPanel = new QWidget(this);
    m_leftPanel->setFixedWidth(258);
    m_leftPanel->setObjectName(QStringLiteral("ctrLeft"));
    auto* leftLay = new QVBoxLayout(m_leftPanel);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(0);

    // Header
    auto* leftHeader = new QWidget(m_leftPanel);
    leftHeader->setObjectName(QStringLiteral("ctrLeftHeader"));
    auto* lhLay = new QHBoxLayout(leftHeader);
    lhLay->setContentsMargins(12, 10, 12, 6);
    auto* leftTitle = new QLabel(tr("SISTEMAS"), leftHeader);
    leftTitle->setObjectName(QStringLiteral("ctrLeftTitle"));
    lhLay->addWidget(leftTitle);
    lhLay->addStretch();
    leftLay->addWidget(leftHeader);

    // Lista de sistemas
    m_systemsList = new QListWidget(m_leftPanel);
    m_systemsList->setObjectName(QStringLiteral("ctrSystemsList"));
    m_systemsList->setFrameShape(QFrame::NoFrame);
    m_systemsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_systemsList->setMaximumHeight(210);
    m_sysDelegate = new SystemItemDelegate(m_systemsList);
    m_systemsList->setItemDelegate(m_sysDelegate);
    leftLay->addWidget(m_systemsList);

    // Botão novo sistema
    auto* newSysWrap = new QWidget(m_leftPanel);
    auto* newSysLay  = new QHBoxLayout(newSysWrap);
    newSysLay->setContentsMargins(12, 4, 12, 8);
    m_newSystemBtn = new QPushButton(tr("+ Novo sistema"), newSysWrap);
    m_newSystemBtn->setObjectName(QStringLiteral("ctrNewSystem"));
    m_newSystemBtn->setCursor(Qt::PointingHandCursor);
    newSysLay->addWidget(m_newSystemBtn);
    leftLay->addWidget(newSysWrap);

    // ── Detalhe do sistema selecionado ────────────────────────────────────────
    m_sysDetail = new QWidget(m_leftPanel);
    m_sysDetail->setObjectName(QStringLiteral("ctrSysDetail"));
    m_sysDetail->setVisible(false);
    auto* sdLay = new QVBoxLayout(m_sysDetail);
    sdLay->setContentsMargins(12, 4, 12, 8);
    sdLay->setSpacing(6);

    auto makeHSep = [&](QWidget* parent) {
        auto* s = new QFrame(parent);
        s->setFrameShape(QFrame::HLine);
        s->setObjectName(QStringLiteral("ctrHSep"));
        return s;
    };
    sdLay->addWidget(makeHSep(m_sysDetail));

    // Nome + excluir sistema
    auto* nameRow = new QHBoxLayout();
    nameRow->setSpacing(6);
    m_systemNameEdit = new QLineEdit(m_sysDetail);
    m_systemNameEdit->setObjectName(QStringLiteral("ctrSysName"));
    m_systemNameEdit->setPlaceholderText(tr("Nome do sistema"));
    nameRow->addWidget(m_systemNameEdit, 1);
    m_deleteSystemBtn = new QPushButton(QStringLiteral("✕"), m_sysDetail);
    m_deleteSystemBtn->setObjectName(QStringLiteral("ctrDeleteSys"));
    m_deleteSystemBtn->setCursor(Qt::PointingHandCursor);
    m_deleteSystemBtn->setToolTip(tr("Excluir sistema"));
    m_deleteSystemBtn->setFixedSize(26, 26);
    nameRow->addWidget(m_deleteSystemBtn);
    sdLay->addLayout(nameRow);

    m_categoryLabel = new QLabel(m_sysDetail);
    m_categoryLabel->setObjectName(QStringLiteral("ctrCatBadge"));
    sdLay->addWidget(m_categoryLabel);

    sdLay->addWidget(makeHSep(m_sysDetail));

    // Slider
    auto* sliderRow = new QHBoxLayout();
    sliderRow->setSpacing(4);
    m_waypointFirst = new QLabel(m_sysDetail);
    m_waypointFirst->setObjectName(QStringLiteral("ctrWaypointEdge"));
    sliderRow->addWidget(m_waypointFirst);
    m_slider = new QSlider(Qt::Horizontal, m_sysDetail);
    m_slider->setObjectName(QStringLiteral("ctrSlider"));
    m_slider->setSingleStep(1);
    m_slider->setPageStep(1);
    sliderRow->addWidget(m_slider, 1);
    m_waypointLast = new QLabel(m_sysDetail);
    m_waypointLast->setObjectName(QStringLiteral("ctrWaypointEdge"));
    sliderRow->addWidget(m_waypointLast);
    sdLay->addLayout(sliderRow);

    m_waypointName = new QLabel(m_sysDetail);
    m_waypointName->setObjectName(QStringLiteral("ctrWaypointName"));
    m_waypointName->setAlignment(Qt::AlignCenter);
    sdLay->addWidget(m_waypointName);

    m_waypointTip = new QLabel(m_sysDetail);
    m_waypointTip->setObjectName(QStringLiteral("ctrWaypointTip"));
    m_waypointTip->setAlignment(Qt::AlignCenter);
    m_waypointTip->setWordWrap(true);
    sdLay->addWidget(m_waypointTip);

    // Favorece / Exige — balança de trade-offs do waypoint atual
    auto* tradeoffHeader = new QHBoxLayout();
    tradeoffHeader->setSpacing(4);
    auto* favorsTitle = new QLabel(tr("FAVORECE"), m_sysDetail);
    favorsTitle->setObjectName(QStringLiteral("ctrFavorsTitle"));
    tradeoffHeader->addWidget(favorsTitle);
    tradeoffHeader->addStretch();
    m_tradeoffExpandBtn = new QPushButton(QStringLiteral("?"), m_sysDetail);
    m_tradeoffExpandBtn->setObjectName(QStringLiteral("ctrTradeoffExpand"));
    m_tradeoffExpandBtn->setCursor(Qt::PointingHandCursor);
    m_tradeoffExpandBtn->setCheckable(true);
    m_tradeoffExpandBtn->setFixedSize(18, 18);
    m_tradeoffExpandBtn->setToolTip(tr("Mostrar todos os pontos"));
    tradeoffHeader->addWidget(m_tradeoffExpandBtn);
    tradeoffHeader->addStretch();
    auto* demandsTitle = new QLabel(tr("EXIGE"), m_sysDetail);
    demandsTitle->setObjectName(QStringLiteral("ctrDemandsTitle"));
    tradeoffHeader->addWidget(demandsTitle);
    sdLay->addLayout(tradeoffHeader);

    auto* tradeoffRow = new QHBoxLayout();
    tradeoffRow->setSpacing(10);
    m_favorsList = new QLabel(m_sysDetail);
    m_favorsList->setObjectName(QStringLiteral("ctrFavorsList"));
    m_favorsList->setWordWrap(true);
    m_favorsList->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    tradeoffRow->addWidget(m_favorsList, 1);
    m_demandsList = new QLabel(m_sysDetail);
    m_demandsList->setObjectName(QStringLiteral("ctrDemandsList"));
    m_demandsList->setWordWrap(true);
    m_demandsList->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    tradeoffRow->addWidget(m_demandsList, 1);
    sdLay->addLayout(tradeoffRow);

    connect(m_tradeoffExpandBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_tradeoffExpanded = checked;
        updateSliderDisplay(m_slider->value());
    });

    sdLay->addWidget(makeHSep(m_sysDetail));

    // Botões de nós
    auto* nodeBtns = new QHBoxLayout();
    nodeBtns->setSpacing(4);
    m_addRuleBtn = new QPushButton(tr("+ Regra"), m_sysDetail);
    m_addRuleBtn->setObjectName(QStringLiteral("ctrAddRule"));
    m_addRuleBtn->setCursor(Qt::PointingHandCursor);
    m_addRuleBtn->setToolTip(tr("Adicionar regra (mecânica/lei)"));
    nodeBtns->addWidget(m_addRuleBtn);
    m_addSectionBtn = new QPushButton(tr("+ Seção"), m_sysDetail);
    m_addSectionBtn->setObjectName(QStringLiteral("ctrAddSection"));
    m_addSectionBtn->setCursor(Qt::PointingHandCursor);
    m_addSectionBtn->setToolTip(tr("Adicionar seção (informação/lore)"));
    nodeBtns->addWidget(m_addSectionBtn);
    nodeBtns->addStretch();
    m_deleteNodeBtn = new QPushButton(QStringLiteral("✕"), m_sysDetail);
    m_deleteNodeBtn->setObjectName(QStringLiteral("ctrDeleteNode"));
    m_deleteNodeBtn->setCursor(Qt::PointingHandCursor);
    m_deleteNodeBtn->setEnabled(false);
    m_deleteNodeBtn->setToolTip(tr("Excluir nó selecionado"));
    m_deleteNodeBtn->setFixedSize(26, 26);
    nodeBtns->addWidget(m_deleteNodeBtn);
    sdLay->addLayout(nodeBtns);

    leftLay->addWidget(m_sysDetail);

    // Árvore de nós (ocupa o resto do painel)
    m_tree = new QTreeWidget(m_leftPanel);
    m_tree->setObjectName(QStringLiteral("ctrTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_tree->setVisible(false);
    leftLay->addWidget(m_tree, 1);

    root->addWidget(m_leftPanel);

    // Separador vertical
    m_vsep = new QFrame(this);
    m_vsep->setFrameShape(QFrame::VLine);
    m_vsep->setObjectName(QStringLiteral("ctrVSep"));
    root->addWidget(m_vsep);

    // ── Lado direito: toolbar + editor ───────────────────────────────────────
    auto* rightWidget = new QWidget(this);
    auto* rightLay    = new QVBoxLayout(rightWidget);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(0);

    // Toolbar de formatação
    auto* toolbar = new QWidget(rightWidget);
    toolbar->setObjectName(QStringLiteral("ctrToolbar"));
    auto* tbLay = new QHBoxLayout(toolbar);
    tbLay->setContentsMargins(8, 4, 8, 4);
    tbLay->setSpacing(2);

    auto makeSep = [&](QWidget* parent) {
        auto* s = new QFrame(parent);
        s->setFrameShape(QFrame::VLine);
        s->setObjectName(QStringLiteral("ctrToolbarSep"));
        return s;
    };
    auto makeFmtBtn = [&](const QString& text, QWidget* parent, bool checkable = false) {
        auto* btn = new QPushButton(text, parent);
        btn->setObjectName(QStringLiteral("ctrFmtBtn"));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setCheckable(checkable);
        btn->setFixedSize(28, 26);
        return btn;
    };

    // Toggle do painel esquerdo
    m_togglePanelBtn = makeFmtBtn(QStringLiteral("☰"), toolbar, true);
    m_togglePanelBtn->setChecked(true);
    m_togglePanelBtn->setFixedSize(30, 26);
    m_togglePanelBtn->setToolTip(tr("Mostrar/ocultar sistemas"));
    tbLay->addWidget(m_togglePanelBtn);
    tbLay->addWidget(makeSep(toolbar));

    // Negrito / Itálico / Sublinhado / Tachado
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

    // Fonte e tamanho
    m_fontCombo = new QFontComboBox(toolbar);
    m_fontCombo->setObjectName(QStringLiteral("ctrFontCombo"));
    m_fontCombo->setMaximumWidth(160);
    tbLay->addWidget(m_fontCombo);

    m_sizeCombo = new QComboBox(toolbar);
    m_sizeCombo->setObjectName(QStringLiteral("ctrSizeCombo"));
    m_sizeCombo->setEditable(true);
    m_sizeCombo->setMinimumWidth(64);
    m_sizeCombo->setMaximumWidth(72);
    for (int s : {8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 22, 24, 28, 32, 36, 48, 72})
        m_sizeCombo->addItem(QString::number(s));
    m_sizeCombo->setCurrentText(QStringLiteral("16"));
    tbLay->addWidget(m_sizeCombo);

    // Espaçamento (entrelinha + margens de parágrafo) — mesma lógica do editor principal
    m_spacingBtn = makeFmtBtn(QString(), toolbar);
    m_spacingBtn->setToolTip(tr("Espaçamento de linhas e parágrafos"));
    m_spacingBtn->setIconSize(QSize(16, 16));
    buildSpacingMenu();
    tbLay->addWidget(m_spacingBtn);

    tbLay->addWidget(makeSep(toolbar));

    // Alinhamento — ícones SVG carregados via applyTheme()
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

    // Modo foco — mesmo ícone/mecânica do botão "Modo foco" da toolbar
    // principal (TopToolbar::focusButton): checkable, ícone troca on/off.
    m_focusBtn = makeFmtBtn(QString(), toolbar, true);
    m_focusBtn->setToolTip(tr("Modo foco"));
    m_focusBtn->setIconSize(QSize(16, 16));
    tbLay->addWidget(m_focusBtn);

    m_insertImageBtn = makeFmtBtn(QString(), toolbar);
    m_insertImageBtn->setToolTip(tr("Inserir imagem"));
    m_insertImageBtn->setIconSize(QSize(16, 16));
    tbLay->addWidget(m_insertImageBtn);
    tbLay->addStretch();

    rightLay->addWidget(toolbar);

    // Editor rich text — vive dentro de uma "página" (largura/margens vindas
    // de EditorLayout::Manager, a mesma config global do editor principal),
    // centralizada numa QScrollArea — ver applyPageLayout().
    m_contentEdit = new QTextEdit();
    m_contentEdit->setObjectName(QStringLiteral("ctrContentEdit"));
    m_contentEdit->setFrameShape(QFrame::NoFrame);
    m_contentEdit->setPlaceholderText(tr("Escreva aqui…"));
    m_contentEdit->setEnabled(false);
    m_contentEdit->setAcceptRichText(true);
    m_contentEdit->document()->setDefaultStyleSheet(
        QStringLiteral("p { margin: 0 0 10px 0; line-height: 1.7; }"));
    { QFont f; f.setPointSize(16); m_contentEdit->document()->setDefaultFont(f); }
    m_contentEdit->document()->documentLayout()->registerHandler(
        QTextFormat::ImageObject, new ConstrutorImageHandler(m_contentEdit));

    // Overlay de tamanho de imagem — clica na imagem, mostra controles de
    // +/- largura. Sem alinhamento (showAlignment=false): o Construtor não
    // tem float/posicionamento de imagem, só inline centralizado.
    m_imageOverlay = new ImageOverlay(m_contentEdit->viewport(), /*showAlignment=*/false);
    m_imageOverlay->hide();
    connect(m_imageOverlay, &ImageOverlay::widthChangeRequested,
            this, &ConstrutorWindow::changeSelectedImageWidth);
    m_contentEdit->viewport()->installEventFilter(this);
    connect(m_contentEdit->verticalScrollBar(), &QAbstractSlider::valueChanged,
            this, &ConstrutorWindow::hideOverlay);

    m_pageColumn = new QWidget();
    m_pageColumn->setObjectName(QStringLiteral("ctrPageColumn"));
    auto* pageLay = new QVBoxLayout(m_pageColumn);
    pageLay->addWidget(m_contentEdit, 1);

    m_pageScroll = new QScrollArea(rightWidget);
    m_pageScroll->setObjectName(QStringLiteral("ctrPageScroll"));
    m_pageScroll->setFrameShape(QFrame::NoFrame);
    m_pageScroll->setWidgetResizable(true);
    m_pageScroll->setAlignment(Qt::AlignHCenter);
    m_pageScroll->setWidget(m_pageColumn);
    rightLay->addWidget(m_pageScroll, 1);

    root->addWidget(rightWidget, 1);

    // ── Conexões ─────────────────────────────────────────────────────────────
    connect(m_togglePanelBtn, &QPushButton::toggled,
            this, &ConstrutorWindow::onTogglePanel);
    connect(m_focusBtn, &QPushButton::toggled, this, &ConstrutorWindow::setFocusMode);

    connect(m_systemsList, &QListWidget::currentRowChanged,
            this, &ConstrutorWindow::onSystemSelected);
    connect(m_newSystemBtn,    &QPushButton::clicked, this, &ConstrutorWindow::onNewSystem);
    connect(m_deleteSystemBtn, &QPushButton::clicked, this, &ConstrutorWindow::onDeleteSystem);
    connect(m_systemNameEdit, &QLineEdit::editingFinished,
            this, [this]() { onSystemNameEdited(m_systemNameEdit->text()); });
    connect(m_slider, &QSlider::valueChanged, this, &ConstrutorWindow::onSliderChanged);

    connect(m_addRuleBtn,    &QPushButton::clicked, this, &ConstrutorWindow::onAddRule);
    connect(m_addSectionBtn, &QPushButton::clicked, this, &ConstrutorWindow::onAddSection);
    connect(m_deleteNodeBtn, &QPushButton::clicked, this, &ConstrutorWindow::onDeleteNode);
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &ConstrutorWindow::onTreeSelectionChanged);
    connect(m_tree, &QTreeWidget::itemChanged,
            this, &ConstrutorWindow::onTreeItemChanged);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &ConstrutorWindow::onTreeContextMenu);

    connect(m_contentEdit, &QTextEdit::textChanged,
            this, &ConstrutorWindow::onNodeContentChanged);
    connect(m_contentEdit, &QTextEdit::currentCharFormatChanged,
            this, &ConstrutorWindow::onCurrentCharFormatChanged);
    connect(m_contentEdit, &QTextEdit::cursorPositionChanged, this, [this]() {
        if (!m_updatingFmt && m_contentEdit->isEnabled())
            updateToolbarState(m_contentEdit->currentCharFormat());
    });

    // Formatação de caractere
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
    // Indentação, fonte, tamanho, alinhamento e espaçamento são GLOBAIS —
    // aplicam ao documento inteiro do nó atual, não apenas à seleção/parágrafo
    // corrente (mesmo padrão do editor principal: MainWindow::applyEditorStyle()).
    connect(m_indentBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_updatingFmt) return;
        m_firstLineIndentEnabled = on;
        QSettings().setValue(QStringLiteral("construtor/firstLineIndent"), on);
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
            this, &ConstrutorWindow::onInsertImage);
}

// ── Lista de sistemas ──────────────────────────────────────────────────────────

void ConstrutorWindow::rebuildSystemsList()
{
    if (!m_store) return;

    const QString currentId = selectedSystemId();
    m_rebuilding = true;
    m_systemsList->blockSignals(true);
    m_systemsList->clear();

    for (const auto& sys : m_store->systems()) {
        auto* item = new QListWidgetItem(sys.name, m_systemsList);
        item->setData(Qt::UserRole, sys.id);

        // Subtexto: "Categoria | Waypoint"
        const ConstrutorStore::Category* cat = ConstrutorStore::categoryById(sys.categoryId);
        QString sub;
        if (cat) {
            sub = cat->displayName;
            if (!cat->waypoints.isEmpty()) {
                const int idx = qBound(0, sys.sliderIndex, cat->waypoints.size() - 1);
                sub += QStringLiteral(" | ") + cat->waypoints[idx].label;
            }
        }
        item->setData(Qt::UserRole + 2, sub);

        if (sys.id == currentId)
            m_systemsList->setCurrentItem(item);
    }

    m_systemsList->blockSignals(false);
    m_rebuilding = false;

    if (m_systemsList->currentItem() == nullptr && !m_currentSystemId.isEmpty()) {
        m_currentSystemId.clear();
        m_sysDetail->setVisible(false);
        m_tree->setVisible(false);
    }
}

// ── Carrega sistema selecionado ───────────────────────────────────────────────

void ConstrutorWindow::onSystemSelected()
{
    if (m_rebuilding) return;
    saveCurrentNodeContent();

    const QString id = selectedSystemId();
    if (id.isEmpty()) {
        m_currentSystemId.clear();
        m_currentNodeId.clear();
        m_sysDetail->setVisible(false);
        m_tree->setVisible(false);
        return;
    }
    if (id == m_currentSystemId) return;
    loadSystem(id);
}

void ConstrutorWindow::loadSystem(const QString& id)
{
    const ConstrutorStore::System* sys = m_store ? m_store->system(id) : nullptr;
    if (!sys) return;

    m_currentSystemId = id;
    m_currentNodeId.clear();
    m_sysDetail->setVisible(true);
    m_tree->setVisible(true);

    // Nome
    m_systemNameEdit->blockSignals(true);
    m_systemNameEdit->setText(sys->name);
    m_systemNameEdit->blockSignals(false);

    // Categoria
    const ConstrutorStore::Category* cat = ConstrutorStore::categoryById(sys->categoryId);
    m_categoryLabel->setText(cat ? cat->displayName : sys->categoryId);

    // Slider
    if (cat && !cat->waypoints.isEmpty()) {
        m_slider->blockSignals(true);
        m_slider->setRange(0, cat->waypoints.size() - 1);
        m_slider->setTickInterval(1);
        m_slider->setValue(qBound(0, sys->sliderIndex, cat->waypoints.size() - 1));
        m_waypointFirst->setText(cat->waypoints.first().label);
        m_waypointLast->setText(cat->waypoints.last().label);
        m_slider->blockSignals(false);
        updateSliderDisplay(m_slider->value());
    }

    // Árvore
    rebuildTree();

    // Editor
    hideOverlay();
    m_contentEdit->setEnabled(false);
    m_contentEdit->clear();
    m_deleteNodeBtn->setEnabled(false);
}

// ── Árvore de nós ─────────────────────────────────────────────────────────────

void ConstrutorWindow::rebuildTree()
{
    const ConstrutorStore::System* sys =
        (m_store && !m_currentSystemId.isEmpty())
            ? m_store->system(m_currentSystemId)
            : nullptr;
    if (!sys) return;

    m_rebuilding = true;
    m_tree->blockSignals(true);
    m_tree->clear();

    for (const auto& node : sys->nodes)
        populateTreeNode(nullptr, node);

    m_tree->expandAll();

    // Restaura seleção — ainda com m_rebuilding=true (igual rebuildSystemsList):
    // setCurrentItem dispara itemSelectionChanged mesmo fora de blockSignals,
    // e sem essa guarda o reseletor reaciona onTreeSelectionChanged, que
    // recarrega o conteúdo do nó (setHtml + moveCursor(Start)) no meio da
    // digitação, resetando o cursor pro início do documento.
    if (!m_currentNodeId.isEmpty()) {
        const auto items = m_tree->findItems(QString(), Qt::MatchContains | Qt::MatchRecursive);
        for (auto* item : items) {
            if (item->data(0, kNodeIdRole).toString() == m_currentNodeId) {
                m_tree->setCurrentItem(item);
                break;
            }
        }
    }

    m_tree->blockSignals(false);
    m_rebuilding = false;
}

void ConstrutorWindow::populateTreeNode(QTreeWidgetItem* parent,
                                         const ConstrutorStore::Node& node)
{
    QTreeWidgetItem* item;
    if (parent)
        item = new QTreeWidgetItem(parent);
    else
        item = new QTreeWidgetItem(m_tree);

    const bool isRule = (node.type == ConstrutorStore::NodeType::Rule);
    item->setText(0, (isRule ? QStringLiteral("📐 ") : QStringLiteral("📄 ")) + node.name);
    item->setData(0, kNodeIdRole,   node.id);
    item->setData(0, kNodeTypeRole, static_cast<int>(node.type));
    item->setFlags(item->flags() | Qt::ItemIsEditable);

    for (const auto& child : node.children)
        populateTreeNode(item, child);
}

void ConstrutorWindow::onTreeSelectionChanged()
{
    if (m_rebuilding) return;
    saveCurrentNodeContent();

    const QString nodeId = selectedNodeId();
    m_currentNodeId = nodeId;

    if (nodeId.isEmpty()) {
        hideOverlay();
        m_contentEdit->setEnabled(false);
        m_contentEdit->clear();
        m_deleteNodeBtn->setEnabled(false);
        return;
    }

    m_deleteNodeBtn->setEnabled(true);

    const ConstrutorStore::System* sys = m_store ? m_store->system(m_currentSystemId) : nullptr;
    if (!sys) return;

    // Busca o nó na store para carregar o conteúdo
    // (findNode é privado, mas podemos usar updateNode com dados iguais para obter)
    // Aqui fazemos a busca diretamente via store const
    // Usamos um helper local recursivo
    std::function<const ConstrutorStore::Node*(const QList<ConstrutorStore::Node>&, const QString&)>
        findConst = [&](const QList<ConstrutorStore::Node>& nodes,
                        const QString& id) -> const ConstrutorStore::Node* {
        for (const auto& n : nodes) {
            if (n.id == id) return &n;
            const auto* c = findConst(n.children, id);
            if (c) return c;
        }
        return nullptr;
    };

    const ConstrutorStore::Node* node = findConst(sys->nodes, nodeId);
    if (!node) return;

    hideOverlay();
    m_selectedImageCursor = QTextCursor();
    m_contentEdit->blockSignals(true);
    m_contentEdit->setEnabled(true);
    if (node->content.startsWith(QLatin1String("<!DOCTYPE")))
        m_contentEdit->setHtml(node->content);
    else
        m_contentEdit->setPlainText(node->content);
    m_contentEdit->moveCursor(QTextCursor::Start);

    // Normaliza indentação, entrelinha e espaçamento de parágrafo pelas
    // preferências globais persistidas (igual ao editor principal: são
    // globais, reaplicadas em todo o documento a cada carregamento, não
    // por parágrafo/documento individual).
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

    // Conteúdo novo entra sem o dim/foreground do Focus Mode (blockSignals
    // suprimiu o cursorPositionChanged) — reaplica explicitamente.
    applyFocusTextColor();
    updateFocusedBlock();

    updateToolbarState(m_contentEdit->currentCharFormat());
}

void ConstrutorWindow::onTreeItemChanged(QTreeWidgetItem* item, int column)
{
    if (m_rebuilding || !item || column != 0) return;
    if (!m_store) return;

    // Remove o prefixo de ícone para extrair apenas o nome
    QString text = item->text(0);
    if (text.startsWith(QStringLiteral("📐 ")) || text.startsWith(QStringLiteral("📄 ")))
        text = text.mid(3);

    const QString nodeId = item->data(0, kNodeIdRole).toString();
    if (nodeId.isEmpty() || text.isEmpty()) {
        rebuildTree();
        return;
    }

    // Busca conteúdo atual para preservar
    const ConstrutorStore::System* sys = m_store->system(m_currentSystemId);
    if (!sys) return;
    std::function<const ConstrutorStore::Node*(const QList<ConstrutorStore::Node>&)>
        findConst = [&](const QList<ConstrutorStore::Node>& nodes)
            -> const ConstrutorStore::Node* {
        for (const auto& n : nodes) {
            if (n.id == nodeId) return &n;
            const auto* c = findConst(n.children);
            if (c) return c;
        }
        return nullptr;
    };
    const ConstrutorStore::Node* node = findConst(sys->nodes);
    const QString content = node ? node->content : QString();

    m_store->updateNode(m_currentSystemId, nodeId, text, content);
}

// ── Slider ────────────────────────────────────────────────────────────────────

void ConstrutorWindow::updateSliderDisplay(int index)
{
    const ConstrutorStore::System* sys = m_store ? m_store->system(m_currentSystemId) : nullptr;
    if (!sys) return;
    const ConstrutorStore::Category* cat = ConstrutorStore::categoryById(sys->categoryId);
    if (!cat || cat->waypoints.isEmpty()) return;

    const int i = qBound(0, index, cat->waypoints.size() - 1);
    const ConstrutorStore::CategoryWaypoint& wp = cat->waypoints[i];
    m_waypointName->setText(wp.label);
    m_waypointTip->setText(wp.tooltip);

    const int shown = m_tradeoffExpanded ? 10 : 3;
    auto bulletText = [shown](const QStringList& items) {
        QString html;
        for (int j = 0; j < items.size() && j < shown; ++j)
            html += QStringLiteral("<p style='margin:0 0 8px 0;'>• %1</p>").arg(items[j].toHtmlEscaped());
        return html;
    };
    if (m_favorsList)  m_favorsList->setText(bulletText(wp.favors));
    if (m_demandsList) m_demandsList->setText(bulletText(wp.demands));
    if (m_tradeoffExpandBtn) m_tradeoffExpandBtn->setText(m_tradeoffExpanded ? QStringLiteral("−") : QStringLiteral("?"));
}

void ConstrutorWindow::onSliderChanged(int index)
{
    updateSliderDisplay(index);
    if (!m_store || m_currentSystemId.isEmpty()) return;
    const ConstrutorStore::System* sys = m_store->system(m_currentSystemId);
    if (!sys) return;
    m_store->updateSystem(m_currentSystemId, sys->name, index);
}

// ── Edição de nome do sistema ─────────────────────────────────────────────────

void ConstrutorWindow::onSystemNameEdited(const QString& name)
{
    if (!m_store || m_currentSystemId.isEmpty() || name.trimmed().isEmpty()) return;
    const ConstrutorStore::System* sys = m_store->system(m_currentSystemId);
    if (!sys || sys->name == name.trimmed()) return;
    m_store->updateSystem(m_currentSystemId, name.trimmed(), sys->sliderIndex);

    // Atualiza item na lista
    const auto items = m_systemsList->findItems(QString(), Qt::MatchContains);
    for (auto* item : items) {
        if (item->data(Qt::UserRole).toString() == m_currentSystemId) {
            m_rebuilding = true;
            item->setText(name.trimmed());
            m_rebuilding = false;
            break;
        }
    }
}

// ── Conteúdo do nó ────────────────────────────────────────────────────────────

void ConstrutorWindow::onNodeContentChanged()
{
    m_saveTimer->start();
}

void ConstrutorWindow::saveCurrentNodeContent()
{
    m_saveTimer->stop();
    if (!m_store || m_currentSystemId.isEmpty() || m_currentNodeId.isEmpty()) return;
    if (!m_contentEdit->isEnabled()) return;

    const ConstrutorStore::System* sys = m_store->system(m_currentSystemId);
    if (!sys) return;

    std::function<const ConstrutorStore::Node*(const QList<ConstrutorStore::Node>&)>
        findConst = [&](const QList<ConstrutorStore::Node>& nodes)
            -> const ConstrutorStore::Node* {
        for (const auto& n : nodes) {
            if (n.id == m_currentNodeId) return &n;
            const auto* c = findConst(n.children);
            if (c) return c;
        }
        return nullptr;
    };

    const ConstrutorStore::Node* node = findConst(sys->nodes);
    if (!node) return;

    const QString newContent = m_contentEdit->toHtml();
    if (node->content == newContent) return;
    m_store->updateNode(m_currentSystemId, m_currentNodeId, node->name, newContent);
}

// ── Criação de sistema ────────────────────────────────────────────────────────

void ConstrutorWindow::onNewSystem()
{
    if (!m_store) return;

    // Diálogo único: nome + categoria na mesma caixa
    const QList<ConstrutorStore::Category>& cats = ConstrutorStore::categories();

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Novo sistema"));

    auto* form = new QFormLayout();
    auto* nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(tr("Nome do sistema"));
    form->addRow(tr("Nome:"), nameEdit);

    auto* catCombo = new QComboBox(&dlg);
    for (const auto& c : cats)
        catCombo->addItem(c.displayName, c.id);
    form->addRow(tr("Categoria:"), catCombo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QPushButton* okBtn = buttons->button(QDialogButtonBox::Ok);
    okBtn->setEnabled(false);
    connect(nameEdit, &QLineEdit::textChanged, &dlg, [okBtn](const QString& t) {
        okBtn->setEnabled(!t.trimmed().isEmpty());
    });
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* layout = new QVBoxLayout(&dlg);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return;

    const QString name = nameEdit->text().trimmed();
    const QString categoryId = catCombo->currentData().toString();
    if (name.isEmpty()) return;

    const QString id = m_store->addSystem(name, categoryId, 0);

    // Seleciona o novo sistema na lista
    for (int i = 0; i < m_systemsList->count(); ++i) {
        if (m_systemsList->item(i)->data(Qt::UserRole).toString() == id) {
            m_systemsList->setCurrentRow(i);
            break;
        }
    }
}

void ConstrutorWindow::onDeleteSystem()
{
    if (!m_store || m_currentSystemId.isEmpty()) return;
    const ConstrutorStore::System* sys = m_store->system(m_currentSystemId);
    if (!sys) return;

    const auto r = QMessageBox::question(
        this, tr("Excluir sistema"),
        tr("Excluir o sistema \"%1\" e todos os seus nós?\n\nEssa ação não pode ser desfeita.")
            .arg(sys->name),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (r != QMessageBox::Yes) return;

    m_currentSystemId.clear();
    m_currentNodeId.clear();
    m_sysDetail->setVisible(false);
    m_tree->setVisible(false);
    m_store->removeSystem(sys->id);
}

// ── Criação de nós ────────────────────────────────────────────────────────────

void ConstrutorWindow::onAddRule()    { onAddChild(ConstrutorStore::NodeType::Rule); }
void ConstrutorWindow::onAddSection() { onAddChild(ConstrutorStore::NodeType::Section); }

void ConstrutorWindow::onAddChild(ConstrutorStore::NodeType type)
{
    if (!m_store || m_currentSystemId.isEmpty()) return;

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, type == ConstrutorStore::NodeType::Rule ? tr("Nova regra") : tr("Nova seção"),
        tr("Nome:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    // Se houver nó selecionado, adiciona como filho; senão, como raiz
    const QString parentId = selectedNodeId();
    const QString newId = m_store->addNode(m_currentSystemId, parentId, type, name.trimmed());

    // Seleciona o nó recém-criado
    m_currentNodeId = newId;
    rebuildTree();
}

void ConstrutorWindow::onDeleteNode()
{
    if (!m_store || m_currentSystemId.isEmpty() || m_currentNodeId.isEmpty()) return;
    const auto r = QMessageBox::question(
        this, tr("Excluir nó"),
        tr("Excluir este nó e todos os seus filhos?\n\nEssa ação não pode ser desfeita."),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (r != QMessageBox::Yes) return;

    m_saveTimer->stop();
    const QString nodeId = m_currentNodeId;
    m_currentNodeId.clear();
    hideOverlay();
    m_contentEdit->setEnabled(false);
    m_contentEdit->clear();
    m_deleteNodeBtn->setEnabled(false);
    m_store->removeNode(m_currentSystemId, nodeId);
}

// ── Menu de contexto da árvore ────────────────────────────────────────────────

void ConstrutorWindow::onTreeContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (!item) return;

    const QString nodeId = item->data(0, kNodeIdRole).toString();
    if (nodeId.isEmpty()) return;

    QMenu menu(this);
    menu.addAction(tr("Adicionar Regra filha"), this,
                   [this]() { onAddChild(ConstrutorStore::NodeType::Rule); });
    menu.addAction(tr("Adicionar Seção filha"), this,
                   [this]() { onAddChild(ConstrutorStore::NodeType::Section); });
    menu.addSeparator();
    menu.addAction(tr("Excluir"), this, &ConstrutorWindow::onDeleteNode);
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

// ── Store changed ─────────────────────────────────────────────────────────────

void ConstrutorWindow::onStoreChanged()
{
    rebuildSystemsList();
    if (!m_currentSystemId.isEmpty()) {
        const ConstrutorStore::System* sys = m_store ? m_store->system(m_currentSystemId) : nullptr;
        if (!sys) {
            m_currentSystemId.clear();
            m_sysDetail->setVisible(false);
            m_tree->setVisible(false);
        } else {
            rebuildTree();
        }
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString ConstrutorWindow::selectedSystemId() const
{
    const QListWidgetItem* item = m_systemsList ? m_systemsList->currentItem() : nullptr;
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QString ConstrutorWindow::selectedNodeId() const
{
    const QTreeWidgetItem* item = m_tree ? m_tree->currentItem() : nullptr;
    return item ? item->data(0, kNodeIdRole).toString() : QString();
}

// ── Toggle do painel esquerdo ─────────────────────────────────────────────────

void ConstrutorWindow::onTogglePanel()
{
    const bool visible = m_togglePanelBtn->isChecked();
    m_leftPanel->setVisible(visible);
    m_vsep->setVisible(visible);
}

// ── Sincroniza toolbar com o formato atual do cursor ──────────────────────────

void ConstrutorWindow::updateToolbarState(const QTextCharFormat& fmt)
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

    // Entrelinha/espaçamento de parágrafo são preferências globais (como o
    // indent) — não derivam do parágrafo atual, só refletem o valor salvo.
    if (m_paraBeforeLabel) m_paraBeforeLabel->setText(QStringLiteral("%1 px").arg(m_paraSpaceBefore));
    if (m_paraAfterLabel)  m_paraAfterLabel->setText(QStringLiteral("%1 px").arg(m_paraSpaceAfter));
    updateLineHeightMenuChecks();

    m_updatingFmt = false;
}

void ConstrutorWindow::onCurrentCharFormatChanged(const QTextCharFormat& fmt)
{
    if (!m_updatingFmt && m_contentEdit->isEnabled())
        updateToolbarState(fmt);
}

// ── Formatação global (documento inteiro) ───────────────────────────────────────

void ConstrutorWindow::applyGlobalAlignment(Qt::Alignment align)
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

void ConstrutorWindow::applyLineHeight(int percent)
{
    m_lineHeightPercent = percent;
    QSettings().setValue(QStringLiteral("construtor/lineHeightPercent"), percent);
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

void ConstrutorWindow::applyParaSpaceBefore(int px)
{
    m_paraSpaceBefore = qBound(0, px, 64);
    QSettings().setValue(QStringLiteral("construtor/paraSpaceBefore"), m_paraSpaceBefore);
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

void ConstrutorWindow::applyParaSpaceAfter(int px)
{
    m_paraSpaceAfter = qBound(0, px, 64);
    QSettings().setValue(QStringLiteral("construtor/paraSpaceAfter"), m_paraSpaceAfter);
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

void ConstrutorWindow::updateLineHeightMenuChecks()
{
    for (QAction* a : std::as_const(m_lineHeightActions))
        a->setChecked(a->data().toInt() == m_lineHeightPercent);
}

void ConstrutorWindow::buildSpacingMenu()
{
    auto* menu = new QMenu(m_spacingBtn);
    menu->setObjectName(QStringLiteral("ctrSpacingMenu"));

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

void ConstrutorWindow::onInsertImage()
{
    if (!m_contentEdit->isEnabled()) return;

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Inserir imagem"), QString(),
        tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp *.gif)"));
    if (path.isEmpty()) return;

    QImage img(path);
    if (img.isNull()) return;

    // Embute numa resolução generosa (até 1200px — o teto do overlay de
    // tamanho abaixo) em vez de pedir a largura antes de inserir. O usuário
    // ajusta o tamanho depois clicando na imagem, igual ao editor principal.
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

bool ConstrutorWindow::findImageAt(const QPoint& viewportPos, QTextCursor& imageCursor) const
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

void ConstrutorWindow::showOverlayForImage(const QTextCursor& imageCursor)
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

void ConstrutorWindow::hideOverlay()
{
    if (m_imageOverlay) m_imageOverlay->hide();
}

void ConstrutorWindow::changeSelectedImageWidth(int delta)
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
