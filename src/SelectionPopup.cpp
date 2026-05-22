#include "SelectionPopup.h"

#include "IconUtils.h"
#include "Theme.h"

#include <QEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextEdit>
#include <QToolButton>

namespace {

constexpr int kIconSize = 18;
constexpr int kButtonSize = 28;
constexpr int kGapAboveSelection = 10;

QToolButton* makeBtn(QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setObjectName(QStringLiteral("selPopupBtn"));
    b->setIconSize(QSize(kIconSize, kIconSize));
    b->setFixedSize(kButtonSize, kButtonSize);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    b->setAutoRaise(true);
    return b;
}

}

SelectionPopup::SelectionPopup(QTextEdit *editor, QWidget *parent)
    : QFrame(parent)
    , m_editor(editor)
    , m_layout(nullptr)
{
    setObjectName(QStringLiteral("selectionPopup"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFocusPolicy(Qt::NoFocus);

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(6, 4, 6, 4);
    m_layout->setSpacing(2);

    applyRootStyle();

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(18);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 2);
    setGraphicsEffect(shadow);

    hide();

    if (m_editor) {
        m_editor->viewport()->installEventFilter(this);
        m_editor->installEventFilter(this);
        connect(m_editor, &QTextEdit::selectionChanged,
                this, &SelectionPopup::onSelectionChanged);
        connect(m_editor, &QTextEdit::cursorPositionChanged,
                this, &SelectionPopup::onSelectionChanged);
        if (auto *bar = m_editor->verticalScrollBar()) {
            connect(bar, &QScrollBar::valueChanged,
                    this, &SelectionPopup::onScrollChanged);
        }
        if (auto *bar = m_editor->horizontalScrollBar()) {
            connect(bar, &QScrollBar::valueChanged,
                    this, &SelectionPopup::onScrollChanged);
        }
    }

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &SelectionPopup::applyTheme);
}

void SelectionPopup::applyRootStyle()
{
    setStyleSheet(QStringLiteral(
        "QFrame#selectionPopup {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
        "QToolButton#selPopupBtn {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 6px;"
        "}"
        "QToolButton#selPopupBtn:hover { background: %3; }"
        "QToolButton#selPopupBtn:checked { background: %4; }"
    ).arg(Theme::panelBackground(),
          Theme::panelBorder(),
          Theme::hoverOverlay(),
          Theme::pressedOverlay()));
}

void SelectionPopup::applyTheme()
{
    applyRootStyle();
    // Ícones dos botões já existentes seguem com a cor antiga até a próxima
    // criação — o ganho visual aqui é o background/border do popup, que é
    // o que mais salta aos olhos. MVP aceita.
}

QToolButton* SelectionPopup::addAction(const QString &iconAlias,
                                       const QString &tooltip,
                                       Callback cb)
{
    auto *b = makeBtn(this);
    const QString resPath = QStringLiteral(":/icons/%1").arg(iconAlias);
    b->setIcon(IconUtils::loadToolbarIcon(resPath,
                                          QColor(Theme::textPrimary()),
                                          QColor(Theme::textBright()),
                                          QColor(Theme::accentDefault()),
                                          QSize(kIconSize, kIconSize)));
    b->setToolTip(tooltip);
    if (cb) {
        connect(b, &QToolButton::clicked, this, [cb]() { cb(); });
    }
    m_layout->addWidget(b);
    adjustSize();
    return b;
}

void SelectionPopup::addSeparator()
{
    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::VLine);
    line->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::subtleBorder()));
    line->setFixedSize(1, kButtonSize - 8);
    m_layout->addWidget(line);
}

bool SelectionPopup::hasSelection() const
{
    return m_editor && m_editor->textCursor().hasSelection();
}

bool SelectionPopup::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_editor) return QFrame::eventFilter(watched, event);

    const QEvent::Type t = event->type();
    if (watched == m_editor->viewport()) {
        if (t == QEvent::MouseButtonPress) {
            m_dragging = true;
            hide();
        } else if (t == QEvent::MouseButtonRelease) {
            m_dragging = false;
            onSelectionChanged();
        }
    } else if (watched == m_editor) {
        if (t == QEvent::FocusOut) {
            hide();
        } else if (t == QEvent::Hide) {
            hide();
        }
    }
    return QFrame::eventFilter(watched, event);
}

void SelectionPopup::onSelectionChanged()
{
    if (m_dragging) return;
    if (!hasSelection()) {
        hide();
        return;
    }
    updatePosition();
    if (!isVisible()) show();
}

void SelectionPopup::onScrollChanged()
{
    if (!isVisible()) return;
    if (!hasSelection()) {
        hide();
        return;
    }
    updatePosition();
}

void SelectionPopup::updatePosition()
{
    if (!m_editor) return;

    QTextCursor cur = m_editor->textCursor();
    if (!cur.hasSelection()) return;

    QTextCursor a(m_editor->document());
    a.setPosition(cur.selectionStart());
    QTextCursor b(m_editor->document());
    b.setPosition(cur.selectionEnd());

    const QRect ra = m_editor->cursorRect(a);
    const QRect rb = m_editor->cursorRect(b);
    QRect bounds = ra.united(rb);

    const QRect viewportRect = m_editor->viewport()->rect();
    // Não posiciona se a seleção saiu da janela visível.
    if (!viewportRect.intersects(bounds)) {
        hide();
        return;
    }

    adjustSize();
    const QSize ps = size();
    const QPoint topMid(bounds.left() + bounds.width() / 2,
                        bounds.top() - kGapAboveSelection);
    QPoint global = m_editor->viewport()->mapToGlobal(topMid);
    global.rx() -= ps.width() / 2;
    global.ry() -= ps.height();

    // Se for sair em cima, posiciona abaixo da seleção.
    const QPoint topOfViewportGlobal = m_editor->viewport()->mapToGlobal(QPoint(0, 0));
    if (global.y() < topOfViewportGlobal.y()) {
        global.setY(m_editor->viewport()
                        ->mapToGlobal(QPoint(0, bounds.bottom() + kGapAboveSelection))
                        .y());
    }
    move(global);
}
