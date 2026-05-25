#include "MarkerPickPopup.h"

#include "Theme.h"

#include <QApplication>
#include <QColorDialog>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kSwatchSize = 20;
constexpr int kActionSize = 26;
constexpr int kGapAboveSelection = 10;
constexpr int kCommentWidth = 300;
constexpr int kCommentHeight = 90;

const QStringList kPalette = {
    "#FFD54F", // amarelo
    "#FF8A80", // rosa
    "#80D8FF", // azul claro
    "#A5D6A7", // verde
    "#CE93D8", // roxo
    "#FFAB91", // laranja
    "#EF9A9A", // vermelho suave
    "#80CBC4", // ciano
};

QString swatchStyle(const QString& color, bool selected)
{
    const QString border = selected
        ? QStringLiteral("2px solid %1").arg(Theme::textBright())
        : QStringLiteral("1px solid %1").arg(Theme::subtleBorder());
    return QStringLiteral(
        "QToolButton {"
        "  background: %1;"
        "  border: %2;"
        "  border-radius: 4px;"
        "  min-width: %3px; max-width: %3px;"
        "  min-height: %3px; max-height: %3px;"
        "}"
        "QToolButton:hover { border: 2px solid %4; }"
    ).arg(color, border, QString::number(kSwatchSize), Theme::textBright());
}

QString actionBtnStyle()
{
    return QStringLiteral(
        "QToolButton {"
        "  background: transparent;"
        "  border: 1px solid %1;"
        "  border-radius: 4px;"
        "  color: %2;"
        "  min-width: %3px; max-width: %3px;"
        "  min-height: %3px; max-height: %3px;"
        "  font-size: 13px;"
        "}"
        "QToolButton:hover { background: %4; border-color: %5; }"
        "QToolButton#confirmBtn { color: %6; border-color: %6; }"
        "QToolButton#confirmBtn:hover { background: rgba(120,200,140,0.18); }"
    ).arg(Theme::subtleBorder(),
          Theme::textPrimary(),
          QString::number(kActionSize),
          Theme::hoverOverlay(),
          Theme::textBright(),
          QStringLiteral("#7BC592"));
}

QString customBtnStyle(const QString& color)
{
    return QStringLiteral(
        "QToolButton {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "    stop:0 %1, stop:0.5 #ff8a65, stop:1 #ba68c8);"
        "  border: 1px solid %2;"
        "  border-radius: 4px;"
        "  min-width: %3px; max-width: %3px;"
        "  min-height: %3px; max-height: %3px;"
        "}"
        "QToolButton:hover { border: 2px solid %4; }"
    ).arg(color, Theme::subtleBorder(),
          QString::number(kSwatchSize), Theme::textBright());
}

}

MarkerPickPopup::MarkerPickPopup(QWidget* parent)
    : QFrame(parent)
    , m_color(kPalette.first())
{
    setObjectName(QStringLiteral("markerPickPopup"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);

    buildUi();
    applyTheme();

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(18);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 2);
    setGraphicsEffect(shadow);

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &MarkerPickPopup::applyTheme);

    hide();
    qApp->installEventFilter(this);
}

void MarkerPickPopup::buildUi()
{
    m_root = new QVBoxLayout(this);
    m_root->setContentsMargins(8, 6, 8, 6);
    m_root->setSpacing(6);

    m_paletteRow = new QHBoxLayout();
    m_paletteRow->setContentsMargins(0, 0, 0, 0);
    m_paletteRow->setSpacing(4);

    for (const QString& c : kPalette) {
        auto* sw = new QToolButton(this);
        sw->setFocusPolicy(Qt::NoFocus);
        sw->setCursor(Qt::PointingHandCursor);
        sw->setStyleSheet(swatchStyle(c, false));
        sw->setProperty("paletteColor", c);
        connect(sw, &QToolButton::clicked, this, [this, c]() {
            setColor(QColor(c));
        });
        m_swatches.append(sw);
        m_paletteRow->addWidget(sw);
    }

    m_customBtn = new QToolButton(this);
    m_customBtn->setFocusPolicy(Qt::NoFocus);
    m_customBtn->setCursor(Qt::PointingHandCursor);
    m_customBtn->setToolTip(tr("Cor personalizada"));
    m_customBtn->setStyleSheet(customBtnStyle(Theme::panelBackground()));
    connect(m_customBtn, &QToolButton::clicked, this, &MarkerPickPopup::openCustomColorDialog);
    m_paletteRow->addWidget(m_customBtn);

    m_actionsRow = new QHBoxLayout();
    m_actionsRow->setContentsMargins(0, 0, 0, 0);
    m_actionsRow->setSpacing(4);
    m_actionsRow->addStretch(1);

    m_confirmBtn = new QToolButton(this);
    m_confirmBtn->setObjectName(QStringLiteral("confirmBtn"));
    m_confirmBtn->setText(QStringLiteral("✓"));
    m_confirmBtn->setToolTip(tr("Aplicar"));
    m_confirmBtn->setCursor(Qt::PointingHandCursor);
    m_confirmBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_confirmBtn, &QToolButton::clicked, this, &MarkerPickPopup::emitConfirm);
    m_actionsRow->addWidget(m_confirmBtn);

    m_cancelBtn = new QToolButton(this);
    m_cancelBtn->setText(QStringLiteral("✕"));
    m_cancelBtn->setToolTip(tr("Cancelar"));
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_cancelBtn, &QToolButton::clicked, this, [this]() {
        hide();
        emit cancelled();
    });
    m_actionsRow->addWidget(m_cancelBtn);

    m_commentArea = new QWidget(this);
    auto* cLayout = new QVBoxLayout(m_commentArea);
    cLayout->setContentsMargins(0, 0, 0, 0);
    cLayout->setSpacing(0);
    m_commentEdit = new QTextEdit(m_commentArea);
    m_commentEdit->setPlaceholderText(tr("Escreva seu comentário..."));
    m_commentEdit->setFixedHeight(kCommentHeight);
    cLayout->addWidget(m_commentEdit);
    m_commentArea->setFixedWidth(kCommentWidth);

    // Montagem final do root layout — ordem: paleta, área de comentário
    // (hidden por default em ColorOnly), ações. setMode apenas toggla a
    // visibilidade do commentArea — NÃO reconstruimos o layout (delete em
    // sub-layouts crasha porque destrói os ponteiros).
    m_root->addLayout(m_paletteRow);
    m_root->addWidget(m_commentArea);
    m_root->addLayout(m_actionsRow);
    m_commentArea->setVisible(m_mode == WithComment);

    applySwatchSelection();
    adjustSize();
}

void MarkerPickPopup::rebuildLayout()
{
    // Apenas toggla visibilidade do commentArea — sem mexer no layout (delete
    // em sub-layouts cresceria mas faria dangling pointer).
    if (m_commentArea) m_commentArea->setVisible(m_mode == WithComment);
    adjustSize();
}

void MarkerPickPopup::applyTheme()
{
    setStyleSheet(QStringLiteral(
        "QFrame#markerPickPopup {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
        "QTextEdit {"
        "  background: %3;"
        "  color: %4;"
        "  border: 1px solid %5;"
        "  border-radius: 4px;"
        "  padding: 4px;"
        "  font-size: 12px;"
        "}"
    ).arg(Theme::panelBackground(),
          Theme::panelBorder(),
          Theme::editorBackground(),
          Theme::textPrimary(),
          Theme::subtleBorder()));

    if (m_customBtn) m_customBtn->setStyleSheet(customBtnStyle(Theme::panelBackground()));
    if (m_confirmBtn) m_confirmBtn->setStyleSheet(actionBtnStyle());
    if (m_cancelBtn) m_cancelBtn->setStyleSheet(actionBtnStyle());
    applySwatchSelection();
}

void MarkerPickPopup::applySwatchSelection()
{
    for (QToolButton* sw : m_swatches) {
        const QString c = sw->property("paletteColor").toString();
        const bool selected = QColor(c) == m_color;
        sw->setStyleSheet(swatchStyle(c, selected));
    }
}

void MarkerPickPopup::setMode(Mode m)
{
    if (m_mode == m) return;
    m_mode = m;
    if (m_commentArea) m_commentArea->setVisible(m_mode == WithComment);
    adjustSize();
}

void MarkerPickPopup::setColor(const QColor& c)
{
    if (!c.isValid()) return;
    m_color = c;
    applySwatchSelection();
}

void MarkerPickPopup::setComment(const QString& text)
{
    m_pendingComment = text;
    if (m_commentEdit) m_commentEdit->setPlainText(text);
}

QString MarkerPickPopup::comment() const
{
    if (m_commentEdit && m_mode == WithComment) return m_commentEdit->toPlainText();
    return m_pendingComment;
}

void MarkerPickPopup::openCustomColorDialog()
{
    const QColor c = QColorDialog::getColor(m_color, this, tr("Cor do marcador"));
    if (c.isValid()) setColor(c);
}

void MarkerPickPopup::emitConfirm()
{
    const QColor c = m_color;
    const QString cmt = comment();
    hide();
    emit confirmed(c, cmt);
}

void MarkerPickPopup::showAbove(const QRect& anchorGlobal)
{
    if (m_mode == WithComment && m_commentEdit) {
        m_commentEdit->setPlainText(m_pendingComment);
    }
    adjustSize();
    const QSize ps = size();
    QPoint pos(anchorGlobal.left() + anchorGlobal.width() / 2 - ps.width() / 2,
               anchorGlobal.top() - ps.height() - kGapAboveSelection);
    const QScreen* screen = QGuiApplication::screenAt(pos);
    if (screen) {
        const QRect avail = screen->availableGeometry();
        if (pos.y() < avail.top()) pos.setY(anchorGlobal.bottom() + kGapAboveSelection);
        if (pos.x() < avail.left()) pos.setX(avail.left() + 4);
        if (pos.x() + ps.width() > avail.right()) pos.setX(avail.right() - ps.width() - 4);
    }
    move(pos);
    show();
    if (m_mode == WithComment && m_commentEdit) {
        m_commentEdit->setFocus();
        QTextCursor c = m_commentEdit->textCursor();
        c.movePosition(QTextCursor::End);
        m_commentEdit->setTextCursor(c);
    }
}

void MarkerPickPopup::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
    QFrame::mousePressEvent(event);
}

void MarkerPickPopup::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        move(event->globalPosition().toPoint() - m_dragOffset);
    }
    QFrame::mouseMoveEvent(event);
}

void MarkerPickPopup::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) m_dragging = false;
    QFrame::mouseReleaseEvent(event);
}

bool MarkerPickPopup::eventFilter(QObject* watched, QEvent* event)
{
    if (!isVisible()) return QFrame::eventFilter(watched, event);

    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (m_mode == ColorOnly && (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)) {
            emitConfirm();
            return true;
        }
        if (ke->key() == Qt::Key_Escape) {
            hide();
            emit cancelled();
            return true;
        }
        if (m_mode == WithComment
            && (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
            && (ke->modifiers() & Qt::ControlModifier)) {
            emitConfirm();
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        const QPoint gp = me->globalPosition().toPoint();
        if (!frameGeometry().contains(gp)) {
            // Clique fora: cancela. Mas ignora cliques no QColorDialog filho.
            QWidget* clicked = QApplication::widgetAt(gp);
            QWidget* w = clicked;
            while (w) {
                if (w == this || w->parent() == this) break;
                if (qobject_cast<QColorDialog*>(w)) break;
                w = w->parentWidget();
            }
            if (!w) {
                hide();
                emit cancelled();
            }
        }
    }
    return QFrame::eventFilter(watched, event);
}
