#include "ImageOverlay.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QToolButton>

namespace {

QToolButton *makeOverlayButton(QWidget *parent, const QString &text, const QString &tooltip)
{
    auto *b = new QToolButton(parent);
    b->setText(text);
    b->setToolTip(tooltip);
    b->setAutoRaise(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setCheckable(false);
    return b;
}

QFrame *makeOverlaySeparator(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setFrameShape(QFrame::VLine);
    line->setObjectName(QStringLiteral("imgOverlaySep"));
    line->setFixedHeight(16);
    return line;
}

}

ImageOverlay::ImageOverlay(QWidget *parent, bool showAlignment)
    : QWidget(parent)
    , leftBtn(makeOverlayButton(this, QStringLiteral("⇤"), tr("Alinhar à esquerda")))
    , centerBtn(makeOverlayButton(this, QStringLiteral("≡"), tr("Centralizar")))
    , rightBtn(makeOverlayButton(this, QStringLiteral("⇥"), tr("Alinhar à direita")))
    , minusBtn(makeOverlayButton(this, QStringLiteral("−"), tr("Diminuir")))
    , widthLabel(new QLabel(this))
    , plusBtn(makeOverlayButton(this, QStringLiteral("+"), tr("Aumentar")))
{
    setObjectName(QStringLiteral("imageOverlay"));
    setAttribute(Qt::WA_StyledBackground, true);

    leftBtn->setObjectName(QStringLiteral("imgOverlayBtn"));
    centerBtn->setObjectName(QStringLiteral("imgOverlayBtn"));
    rightBtn->setObjectName(QStringLiteral("imgOverlayBtn"));
    minusBtn->setObjectName(QStringLiteral("imgOverlayBtn"));
    plusBtn->setObjectName(QStringLiteral("imgOverlayBtn"));

    widthLabel->setObjectName(QStringLiteral("imgOverlayWidth"));
    widthLabel->setAlignment(Qt::AlignCenter);
    widthLabel->setFixedWidth(56);
    widthLabel->setText(QStringLiteral("320 px"));

    auto *alignSep = makeOverlaySeparator(this);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(4);
    layout->addWidget(leftBtn);
    layout->addWidget(centerBtn);
    layout->addWidget(rightBtn);
    layout->addWidget(alignSep);
    layout->addWidget(minusBtn);
    layout->addWidget(widthLabel);
    layout->addWidget(plusBtn);

    // Sem alinhamento/flutuação em alguns contextos (ex.: Construtor) — só
    // o controle de tamanho importa lá, então some com esse grupo de botões.
    if (!showAlignment) {
        leftBtn->hide();
        centerBtn->hide();
        rightBtn->hide();
        alignSep->hide();
    }

    connect(leftBtn, &QToolButton::clicked, this, [this]() { emit alignmentRequested(Left); });
    connect(centerBtn, &QToolButton::clicked, this, [this]() { emit alignmentRequested(Center); });
    connect(rightBtn, &QToolButton::clicked, this, [this]() { emit alignmentRequested(Right); });
    connect(minusBtn, &QToolButton::clicked, this, [this]() { emit widthChangeRequested(-40); });
    connect(plusBtn, &QToolButton::clicked, this, [this]() { emit widthChangeRequested(+40); });

    hide();
}

void ImageOverlay::setCurrentAlignment(Alignment alignment)
{
    updateAlignmentButtons(alignment);
}

void ImageOverlay::setCurrentWidth(int width)
{
    widthLabel->setText(QStringLiteral("%1 px").arg(width));
}

void ImageOverlay::updateAlignmentButtons(Alignment current)
{
    leftBtn->setProperty("active", current == Left);
    centerBtn->setProperty("active", current == Center);
    rightBtn->setProperty("active", current == Right);
    for (QToolButton *b : {leftBtn, centerBtn, rightBtn}) {
        b->style()->unpolish(b);
        b->style()->polish(b);
    }
}
