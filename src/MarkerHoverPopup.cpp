#include "MarkerHoverPopup.h"

#include "IconUtils.h"
#include "Theme.h"

#include <QEnterEvent>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kPopupWidth = 280;
constexpr int kCloseDelayMs = 250;
constexpr int kGapAboveSelection = 8;

}

MarkerHoverPopup::MarkerHoverPopup(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("markerHoverPopup"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::NoFocus);

    buildUi();
    applyTheme();

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(16);
    shadow->setColor(QColor(0, 0, 0, 160));
    shadow->setOffset(0, 2);
    setGraphicsEffect(shadow);

    m_closeTimer = new QTimer(this);
    m_closeTimer->setSingleShot(true);
    m_closeTimer->setInterval(kCloseDelayMs);
    connect(m_closeTimer, &QTimer::timeout, this, [this]() {
        hide();
        m_id.clear();
    });

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &MarkerHoverPopup::applyTheme);

    hide();
}

void MarkerHoverPopup::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(6);

    m_text = new QLabel(this);
    m_text->setWordWrap(true);
    m_text->setMaximumWidth(kPopupWidth - 24);
    m_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_text);

    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);
    row->addStretch(1);

    const QSize iconSz(14, 14);
    const QColor iconColor(Theme::textPrimary());
    const QColor iconHover(Theme::textBright());

    m_editBtn = new QToolButton(this);
    m_editBtn->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/edit.svg"),
                                                  iconColor, iconHover, iconHover, iconSz));
    m_editBtn->setIconSize(iconSz);
    m_editBtn->setToolTip(tr("Editar"));
    m_editBtn->setCursor(Qt::PointingHandCursor);
    m_editBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_editBtn, &QToolButton::clicked, this, [this]() {
        emit editRequested(m_id);
        hide();
        m_id.clear();
    });
    row->addWidget(m_editBtn);

    m_deleteBtn = new QToolButton(this);
    m_deleteBtn->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/trash.svg"),
                                                    iconColor, iconHover, iconHover, iconSz));
    m_deleteBtn->setIconSize(iconSz);
    m_deleteBtn->setToolTip(tr("Excluir marcador"));
    m_deleteBtn->setCursor(Qt::PointingHandCursor);
    m_deleteBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_deleteBtn, &QToolButton::clicked, this, [this]() {
        emit deleteRequested(m_id);
        hide();
        m_id.clear();
    });
    row->addWidget(m_deleteBtn);

    root->addLayout(row);

    setFixedWidth(kPopupWidth);
}

void MarkerHoverPopup::applyTheme()
{
    setStyleSheet(QStringLiteral(
        "QFrame#markerHoverPopup {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-left: 4px solid %3;"
        "  border-radius: 6px;"
        "}"
        "QLabel { color: %4; font-size: 12px; }"
        "QToolButton {"
        "  background: transparent;"
        "  border: 1px solid %5;"
        "  border-radius: 4px;"
        "  color: %4;"
        "  min-width: 24px; max-width: 24px;"
        "  min-height: 24px; max-height: 24px;"
        "}"
        "QToolButton:hover { background: %6; }"
    ).arg(Theme::panelBackground(),
          Theme::panelBorder(),
          m_color.isValid() ? m_color.name() : Theme::accentDefault(),
          Theme::textPrimary(),
          Theme::subtleBorder(),
          Theme::hoverOverlay()));
}

void MarkerHoverPopup::showFor(const QString& markerId,
                               const QColor& color,
                               const QString& comment,
                               const QRect& anchorGlobal)
{
    m_id = markerId;
    m_color = color;
    m_text->setText(comment);
    applyTheme();
    positionAbove(anchorGlobal);
    cancelClose();
    show();
}

void MarkerHoverPopup::positionAbove(const QRect& anchorGlobal)
{
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
}

void MarkerHoverPopup::scheduleClose()
{
    if (!isVisible()) return;
    m_closeTimer->start();
}

void MarkerHoverPopup::cancelClose()
{
    m_closeTimer->stop();
}

void MarkerHoverPopup::enterEvent(QEnterEvent* event)
{
    cancelClose();
    QFrame::enterEvent(event);
}

void MarkerHoverPopup::leaveEvent(QEvent* event)
{
    scheduleClose();
    QFrame::leaveEvent(event);
}
