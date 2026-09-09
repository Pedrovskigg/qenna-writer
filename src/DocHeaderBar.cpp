#include "DocHeaderBar.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QToolButton>
#include <QVBoxLayout>

#include "IconUtils.h"
#include "Theme.h"
#include "UiScale.h"

namespace {
// Comporta o bloco de duas linhas (título + subtítulo) com folga.
constexpr int kHeightBase = 54;
constexpr int kHeightFloor = 40;
constexpr int kSideMargin = 16;
constexpr int kVarBtnGap = 4;
constexpr int kVarBtnBase = 20;
}

DocHeaderBar::DocHeaderBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("docHeaderBar"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(kSideMargin, 4, kSideMargin, 4);
    outer->setSpacing(0);
    outer->addStretch(1);

    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("docHeaderTitle"));
    m_title->setAlignment(Qt::AlignCenter);
    outer->addWidget(m_title, 0, Qt::AlignHCenter);

    // Subtítulo e botão de variação viajam juntos: no bloco da toolbar o botão
    // fica colado à direita do subtítulo, e a linha inteira centraliza como uma
    // coisa só, sem deslocar o título de cima.
    auto* subRow = new QHBoxLayout;
    subRow->setContentsMargins(0, 0, 0, 0);
    subRow->setSpacing(kVarBtnGap);

    m_subtitle = new QLabel(this);
    m_subtitle->setObjectName(QStringLiteral("docHeaderSubtitle"));
    m_subtitle->setAlignment(Qt::AlignCenter);
    subRow->addWidget(m_subtitle, 0, Qt::AlignVCenter);

    m_varButton = new QToolButton(this);
    m_varButton->setObjectName(QStringLiteral("docHeaderVar"));
    m_varButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_varButton->setAutoRaise(true);
    m_varButton->setCursor(Qt::PointingHandCursor);
    m_varButton->setToolTip(tr("Variações desta cena"));
    m_varButton->setVisible(false);
    connect(m_varButton, &QToolButton::clicked, this, &DocHeaderBar::sceneVarRequested);
    subRow->addWidget(m_varButton, 0, Qt::AlignVCenter);

    outer->addLayout(subRow);
    outer->setAlignment(subRow, Qt::AlignHCenter);
    outer->addStretch(1);

    // Self-contained: o MainWindow não precisa lembrar de repassar tema/escala.
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &DocHeaderBar::applyTheme);
    connect(UiScale::Manager::instance(), &UiScale::Manager::scaleChanged,
            this, &DocHeaderBar::applyUiScale);

    applyUiScale();
    applyTheme();
}

void DocHeaderBar::applyUiScale()
{
    const qreal s = UiScale::scale();
    setFixedHeight(qMax(kHeightFloor, qRound(kHeightBase * s)));
    const int varSize = qMax(14, qRound(kVarBtnBase * s));
    m_varButton->setFixedSize(varSize, varSize);
    const int icoPx = qMax(10, qRound(14 * s));
    m_varButton->setIconSize(QSize(icoPx, icoPx));
    applyTheme(); // ícone é re-renderizado no tamanho novo
    relayoutText();
}

void DocHeaderBar::applyTheme()
{
    // Mesma cor E mesma opacidade da folha (ver MainWindow::applyEditorStyle):
    // a faixa é irmã do editor no layout, então sem isto apareceria uma emenda
    // visível entre ela e o começo da página.
    const int opacity = qBound(0, Theme::editorOpacity(), 100);
    QColor bg(Theme::editorBackground());
    bg.setAlpha((opacity * 255) / 100);
    const QString bgCss = QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(bg.red()).arg(bg.green()).arg(bg.blue())
        .arg(QString::number(bg.alphaF(), 'f', 3));

    setStyleSheet(QStringLiteral(R"(
        QWidget#docHeaderBar { background: %1; }
        QLabel#docHeaderTitle {
            color: %2;
            background: transparent;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 15px;
            font-weight: 700;
        }
        QLabel#docHeaderSubtitle {
            color: %3;
            background: transparent;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 11px;
            font-weight: 500;
        }
        QToolButton#docHeaderVar {
            background: transparent;
            border: none;
            border-radius: 4px;
        }
        QToolButton#docHeaderVar:hover { background: %4; }
    )").arg(bgCss, Theme::textBright(), Theme::textMuted(), Theme::hoverOverlay()));

    m_varButton->setIcon(IconUtils::loadToolbarIcon(
        QStringLiteral(":/icons/scene-var.svg"),
        QColor(Theme::textMuted()), QColor(Theme::textPrimary()), QColor(Theme::textBright()),
        m_varButton->iconSize()));
}

void DocHeaderBar::setDocumentTitle(const QString& title, const QString& subtitle)
{
    m_rawTitle = title;
    m_rawSubtitle = subtitle;
    m_subtitleWanted = !subtitle.isEmpty();
    relayoutText();
}

void DocHeaderBar::setSceneVarButtonVisible(bool visible)
{
    m_varWanted = visible;
    relayoutText();
}

void DocHeaderBar::relayoutText()
{
    if (!m_title || !m_subtitle || !m_varButton) return;

    const int avail = qMax(40, width() - kSideMargin * 2);

    m_title->setVisible(!m_rawTitle.isEmpty());
    m_title->setText(QFontMetrics(m_title->font()).elidedText(m_rawTitle, Qt::ElideRight, avail));

    // Botão de variação só faz sentido junto do subtítulo — é a variação DAQUELA
    // cena. Mesma regra da toolbar.
    const bool showVar = m_subtitleWanted && m_varWanted;
    m_varButton->setVisible(showVar);
    m_subtitle->setVisible(m_subtitleWanted);
    if (m_subtitleWanted) {
        const int varW = showVar ? kVarBtnGap + m_varButton->width() : 0;
        m_subtitle->setText(QFontMetrics(m_subtitle->font())
                                .elidedText(m_rawSubtitle, Qt::ElideRight, qMax(20, avail - varW)));
    } else {
        m_subtitle->clear();
    }
}

QRect DocHeaderBar::sceneVarButtonGlobalRect() const
{
    if (!m_varButton || !m_varButton->isVisible()) return QRect();
    return QRect(m_varButton->mapToGlobal(QPoint(0, 0)), m_varButton->size());
}

void DocHeaderBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    relayoutText(); // a elipse depende da largura disponível
}
