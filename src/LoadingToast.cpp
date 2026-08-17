#include "LoadingToast.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMovie>

LoadingToast::LoadingToast(QWidget* hostWindow, const QString& text)
    : QWidget(hostWindow)
{
    setObjectName(QStringLiteral("loadingToast"));
    // Sem isso, QWidget puro não pinta background/border de QSS — mesmo
    // padrão já usado em dezenas de outros widgets compostos do app (ver
    // AIChatPanel, PensarioPanel, RefMenuPanel etc.).
    setAttribute(Qt::WA_StyledBackground, true);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(20, 12, 20, 12);
    lay->setSpacing(10);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setObjectName(QStringLiteral("loadingToastImage"));
    m_imageLabel->hide();
    lay->addWidget(m_imageLabel);

    m_textLabel = new QLabel(text.isEmpty() ? tr("Carregando…") : text, this);
    m_textLabel->setObjectName(QStringLiteral("loadingToastText"));
    lay->addWidget(m_textLabel);

    setStyleSheet(QStringLiteral(
        "QWidget#loadingToast { background: %1; border: 1px solid %2; border-radius: 8px; }"
        "QLabel#loadingToastText { color: %3; background: transparent; border: none;"
        " font-family: 'Lora','Crimson Text',serif; font-size: 13px; }")
        .arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textBright()));

    adjustSize();
    reposition();
}

void LoadingToast::setText(const QString& text)
{
    m_textLabel->setText(text);
    adjustSize();
    reposition();
}

void LoadingToast::setAnimation(QMovie* movie)
{
    m_imageLabel->setMovie(movie);
    m_imageLabel->show();
    if (movie) movie->start();
    adjustSize();
    reposition();
}

void LoadingToast::reposition()
{
    QWidget* host = parentWidget();
    if (!host) return;
    const QPoint center = host->rect().center();
    move(center.x() - width() / 2, center.y() - height() / 2);
}
