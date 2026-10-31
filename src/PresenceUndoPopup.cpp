#include "PresenceUndoPopup.h"

#include "Theme.h"
#include "UiScale.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

namespace {

qreal sc() { return UiScale::scale(); }

// Uma linha clicável: a ação em cima, a explicação embaixo, mais apagada.
QPushButton* actionRow(const QString& title, const QString& hint, QWidget* parent)
{
    auto* b = new QPushButton(parent);
    b->setObjectName(QStringLiteral("presenceUndoItem"));
    b->setCursor(Qt::PointingHandCursor);
    auto* v = new QVBoxLayout(b);
    v->setContentsMargins(qRound(8 * sc()), qRound(7 * sc()), qRound(8 * sc()), qRound(7 * sc()));
    v->setSpacing(qRound(2 * sc()));
    auto* t = new QLabel(title, b);
    t->setObjectName(QStringLiteral("presenceUndoTitle"));
    auto* h = new QLabel(hint, b);
    h->setObjectName(QStringLiteral("presenceUndoHint"));
    h->setWordWrap(true);
    for (QLabel* l : { t, h }) l->setAttribute(Qt::WA_TransparentForMouseEvents);
    v->addWidget(t);
    v->addWidget(h);
    b->setMinimumHeight(v->sizeHint().height());
    return b;
}

} // namespace

PresenceUndoPopup::PresenceUndoPopup(const QPixmap& face, const QString& name, QWidget* parent)
    : QFrame(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("presenceUndo"));
    const qreal s = sc();
    setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#presenceUndo { background: %1; border: 1px solid %2; border-radius: @radius-panel; }"
        "QLabel { background: transparent; }"
        "QLabel#presenceUndoName { color: %4; font-weight: 700; }"
        "QLabel#presenceUndoSub, QLabel#presenceUndoHint { color: %3; }"
        "QLabel#presenceUndoTitle { color: %4; }"
        "QPushButton#presenceUndoItem { background: transparent; border: none; border-radius: @radius-control; text-align: left; }"
        "QPushButton#presenceUndoItem:hover, QPushButton#presenceUndoItem:focus { background: %5; }")
        .arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textMuted(), Theme::textBright(),
             Theme::hoverOverlay())));

    const int w = qRound(260 * s);
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(qRound(12 * s), qRound(12 * s), qRound(12 * s), qRound(8 * s));
    lay->setSpacing(qRound(4 * s));

    auto* who = new QHBoxLayout;
    who->setSpacing(qRound(10 * s));
    auto* ph = new QLabel(this);
    ph->setPixmap(face);
    who->addWidget(ph, 0, Qt::AlignVCenter);
    auto* names = new QVBoxLayout;
    names->setSpacing(qRound(2 * s));
    auto* nm = new QLabel(name, this);
    nm->setObjectName(QStringLiteral("presenceUndoName"));
    QFont nf = nm->font();
    nf.setPixelSize(qMax(10, qRound(14 * s)));
    nm->setFont(nf);
    auto* sub = new QLabel(tr("presente neste capítulo"), this);
    sub->setObjectName(QStringLiteral("presenceUndoSub"));
    QFont sf = sub->font();
    sf.setPixelSize(qMax(9, qRound(11.5 * s)));
    sub->setFont(sf);
    names->addWidget(nm);
    names->addWidget(sub);
    who->addLayout(names, 1);
    lay->addLayout(who);
    lay->addSpacing(qRound(6 * s));

    QPushButton* notHere = actionRow(tr("Não está aqui"), tr("Tira a presença só deste capítulo."), this);
    QPushButton* never = actionRow(tr("Nunca detectar"), tr("O detector deixa de procurar %1.").arg(name), this);
    for (QPushButton* b : { notHere, never }) {
        b->setFixedWidth(w);
        for (QLabel* l : b->findChildren<QLabel*>()) {
            QFont f = l->font();
            f.setPixelSize(qMax(9, qRound((l->objectName() == QLatin1String("presenceUndoTitle") ? 12.5 : 11) * s)));
            l->setFont(f);
        }
        lay->addWidget(b);
    }
    m_first = notHere;
    connect(notHere, &QPushButton::clicked, this, [this]() { emit notHereChosen(); close(); });
    connect(never, &QPushButton::clicked, this, [this]() { emit neverChosen(); close(); });
}

void PresenceUndoPopup::popupBelow(const QRect& anchorGlobal)
{
    adjustSize();
    QPoint pos(anchorGlobal.center().x() - width() / 2, anchorGlobal.bottom() + qRound(6 * sc()));
    if (QScreen* screen = QGuiApplication::screenAt(anchorGlobal.center())) {
        const QRect avail = screen->availableGeometry();
        if (pos.x() + width() > avail.right()) pos.setX(avail.right() - width());
        if (pos.x() < avail.left()) pos.setX(avail.left());
        if (pos.y() + height() > avail.bottom()) pos.setY(anchorGlobal.top() - height() - qRound(6 * sc()));
    }
    move(pos);
    show();
    if (m_first) m_first->setFocus(Qt::PopupFocusReason);
}

void PresenceUndoPopup::hideEvent(QHideEvent* event)
{
    QFrame::hideEvent(event);
    emit closed();
}
