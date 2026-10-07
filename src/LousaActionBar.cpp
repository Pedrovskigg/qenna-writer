#include "LousaActionBar.h"

#include "IconUtils.h"
#include "Theme.h"

#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QPainter>
#include <QToolButton>

namespace {
QIcon swatchIcon(const QColor& c)
{
    QPixmap px(36, 36);
    px.setDevicePixelRatio(2.0);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(Theme::textMuted()), 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(9, 9), 8, 8);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawEllipse(QPointF(9, 9), 6, 6);
    return QIcon(px);
}
} // namespace

LousaActionBar::LousaActionBar(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("lousaActionBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    m_lay = new QHBoxLayout(this);
    m_lay->setContentsMargins(4, 4, 4, 4);
    m_lay->setSpacing(1);
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(22);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(0, 0, 0, 110));
    setGraphicsEffect(shadow);
    applyTheme();
    hide();
}

void LousaActionBar::applyTheme()
{
    setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#lousaActionBar { background: %1; border: 1px solid %2; border-radius: @radius-panel; }"
        "QToolButton#lousaActionBtn { background: transparent; border: none; border-radius: @radius-control;"
        "  color: %3; font-size: 12px; padding: 4px 8px; }"
        "QToolButton#lousaActionBtn:hover { background: %4; color: %5; }"
        "QToolButton#lousaActionBtn:checked { background: %4; color: %6; }"
        "QToolButton#lousaActionBtn[danger=\"true\"] { color: %7; }"
        "QFrame#lousaActionSep { background: %2; }"
    ).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(),
          Theme::hoverStrong(), Theme::textBright(), Theme::accentDefault(),
          Theme::accentDanger())));
    if (!m_buttons.isEmpty()) setButtons(m_buttons);
}

void LousaActionBar::setButtons(const QVector<Btn>& buttons)
{
    m_buttons = buttons;
    while (QLayoutItem* it = m_lay->takeAt(0)) {
        if (QWidget* w = it->widget()) w->deleteLater();
        delete it;
    }
    for (const Btn& b : buttons) {
        if (b.separatorBefore) {
            auto* sep = new QFrame(this);
            sep->setObjectName(QStringLiteral("lousaActionSep"));
            sep->setFixedSize(1, 18);
            m_lay->addSpacing(3);
            m_lay->addWidget(sep, 0, Qt::AlignVCenter);
            sep->show();
            m_lay->addSpacing(3);
        }
        auto* tb = new QToolButton(this);
        tb->setObjectName(QStringLiteral("lousaActionBtn"));
        tb->setCursor(Qt::PointingHandCursor);
        tb->setFixedHeight(30);
        tb->setIconSize(QSize(16, 16));
        if (b.danger) tb->setProperty("danger", true);
        if (b.swatch.isValid()) {
            tb->setIcon(swatchIcon(b.swatch));
            tb->setIconSize(QSize(18, 18));
        } else if (!b.icon.isEmpty()) {
            tb->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/lousa/%1.svg").arg(b.icon),
                b.danger ? QColor(Theme::accentDanger()) : QColor(Theme::textPrimary()),
                QColor(Theme::textBright()), QColor(Theme::accentDefault())));
        }
        if (!b.text.isEmpty()) {
            tb->setText(b.text);
            tb->setToolButtonStyle(b.icon.isEmpty() && !b.swatch.isValid()
                                   ? Qt::ToolButtonTextOnly : Qt::ToolButtonTextBesideIcon);
        } else {
            tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
            tb->setFixedWidth(32);
        }
        tb->setToolTip(b.tip.isEmpty() ? b.text : b.tip);
        tb->setCheckable(b.checkable);
        tb->setChecked(b.checked);
        const QString id = b.id;
        connect(tb, &QToolButton::clicked, this, [this, tb, id]() {
            emit triggered(id, tb->mapToGlobal(QPoint(0, tb->height() + 4)));
        });
        m_lay->addWidget(tb);
        tb->show();   // filho novo de widget já visível nasce escondido
    }
    m_lay->activate();
    adjustSize();
}
