#include "UpdateBanner.h"

#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kBadge = 44;

// Círculo na cor de destaque com a seta de download (ou o check) dentro.
QPixmap badgePixmap(const QColor& accent, const QColor& ink, qreal dpr, bool check)
{
    QPixmap pm(QSize(kBadge, kBadge) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(accent);
    p.drawEllipse(QRectF(0, 0, kBadge, kBadge));
    QPen pen(ink, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    const qreal c = kBadge / 2.0;
    if (check) {
        QPainterPath tick;
        tick.moveTo(c - 9, c + 0.5);
        tick.lineTo(c - 2.5, c + 7);
        tick.lineTo(c + 10, c - 6.5);
        p.drawPath(tick);
        return pm;
    }
    p.drawLine(QPointF(c, 12), QPointF(c, 27));
    QPainterPath head;
    head.moveTo(c - 6.5, 21);
    head.lineTo(c, 27.5);
    head.lineTo(c + 6.5, 21);
    p.drawPath(head);
    p.drawLine(QPointF(c - 9, 32), QPointF(c + 9, 32));
    return pm;
}

QString idleText()
{
    return UpdateBanner::tr("Baixa agora e abre o instalador. O Qenna fecha pra instalar; seus projetos ficam onde estão.");
}
}

UpdateBanner::UpdateBanner(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("updateBanner"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(18, 14, 16, 14);
    outer->setSpacing(10);

    auto* row = new QHBoxLayout();
    row->setSpacing(14);
    m_badge = new QLabel(this);
    m_badge->setFixedSize(kBadge, kBadge);
    row->addWidget(m_badge, 0, Qt::AlignTop);

    auto* text = new QVBoxLayout();
    text->setSpacing(3);
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("ubTitle"));
    m_title->setWordWrap(true);
    text->addWidget(m_title);
    m_sub = new QLabel(this);
    m_sub->setObjectName(QStringLiteral("ubSub"));
    m_sub->setWordWrap(true);
    text->addWidget(m_sub);
    m_progress = new QProgressBar(this);
    m_progress->setObjectName(QStringLiteral("ubProgress"));
    m_progress->setRange(0, 100);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(6);
    m_progress->hide();
    text->addSpacing(2);
    text->addWidget(m_progress);
    m_notesBtn = new QToolButton(this);
    m_notesBtn->setObjectName(QStringLiteral("ubNotesBtn"));
    m_notesBtn->setCursor(Qt::PointingHandCursor);
    m_notesBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(m_notesBtn, &QToolButton::clicked, this, [this]() { setNotesOpen(!m_notesScroll->isVisible()); });
    text->addWidget(m_notesBtn, 0, Qt::AlignLeft);
    row->addLayout(text, 1);

    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(8);
    m_later = new QPushButton(tr("Depois"), this);
    m_later->setObjectName(QStringLiteral("ubGhost"));
    m_later->setCursor(Qt::PointingHandCursor);
    connect(m_later, &QPushButton::clicked, this, [this]() { hide(); emit dismissed(); });
    buttons->addWidget(m_later);
    m_cancel = new QPushButton(tr("Cancelar"), this);
    m_cancel->setObjectName(QStringLiteral("ubGhost"));
    m_cancel->setCursor(Qt::PointingHandCursor);
    m_cancel->hide();
    connect(m_cancel, &QPushButton::clicked, this, &UpdateBanner::cancelRequested);
    buttons->addWidget(m_cancel);
    m_action = new QPushButton(this);
    m_action->setObjectName(QStringLiteral("ubAction"));
    m_action->setCursor(Qt::PointingHandCursor);
    connect(m_action, &QPushButton::clicked, this, [this]() {
        if (m_info) { hide(); emit dismissed(); return; }
        emit downloadRequested();
    });
    buttons->addWidget(m_action);
    row->addLayout(buttons);
    row->setAlignment(buttons, Qt::AlignTop);
    outer->addLayout(row);

    m_notes = new QLabel(this);
    m_notes->setObjectName(QStringLiteral("ubNotes"));
    m_notes->setWordWrap(true);
    m_notes->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_notes->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_notesScroll = new QScrollArea(this);
    m_notesScroll->setObjectName(QStringLiteral("ubNotesScroll"));
    m_notesScroll->setWidget(m_notes);
    m_notesScroll->setWidgetResizable(true);
    m_notesScroll->setFrameShape(QFrame::NoFrame);
    m_notesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_notesScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    // Do tamanho das notas, até um teto; passou disso, rola.
    m_notesScroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    m_notesScroll->setMaximumHeight(220);
    m_notesScroll->hide();
    auto* notesRow = new QHBoxLayout();
    notesRow->setContentsMargins(kBadge + 14, 0, 0, 0);
    notesRow->addWidget(m_notesScroll);
    outer->addLayout(notesRow);

    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, &UpdateBanner::applyTheme);
    hide();
}

void UpdateBanner::showAvailable(const QString& version, const QString& notes, bool incomplete, bool ready)
{
    m_info = false;
    m_incomplete = incomplete;
    m_ready = ready;
    applyTheme();
    m_title->setText(incomplete ? tr("A atualização pra %1 não terminou").arg(version)
                                : tr("Tem versão nova do Qenna Writer: %1").arg(version));
    m_sub->setText(modeText());
    m_notes->setText(notes);
    m_notesBtn->setVisible(!notes.trimmed().isEmpty());
    setNotesOpen(false);
    resetIdle();
    show();
}

void UpdateBanner::showInfo(const QString& title, const QString& text)
{
    m_info = true;
    m_downloading = false;
    m_title->setText(title);
    m_sub->setText(text);
    m_sub->show();
    m_notesBtn->hide();
    setNotesOpen(false);
    m_progress->hide();
    m_cancel->hide();
    m_later->hide();
    m_action->setText(tr("Ok"));
    m_action->setEnabled(true);
    applyTheme();
    show();
}

void UpdateBanner::setDownloading(int percent)
{
    m_downloading = true;
    m_sub->setText(tr("Baixando a atualização… %1%").arg(qBound(0, percent, 100)));
    m_sub->setProperty("error", false);
    m_sub->style()->unpolish(m_sub);
    m_sub->style()->polish(m_sub);
    m_progress->setValue(percent);
    m_progress->show();
    m_later->hide();
    m_cancel->show();
    m_action->setText(tr("Baixando…"));
    m_action->setEnabled(false);
}

void UpdateBanner::showError(const QString& message)
{
    m_downloading = false;
    m_sub->setText(message);
    m_sub->setProperty("error", true);
    m_sub->style()->unpolish(m_sub);
    m_sub->style()->polish(m_sub);
    m_progress->hide();
    m_cancel->hide();
    m_later->show();
    m_action->setText(tr("Tentar novamente"));
    m_action->setEnabled(true);
}

void UpdateBanner::resetIdle()
{
    if (m_info) return;
    m_downloading = false;
    m_sub->setProperty("error", false);
    m_sub->style()->unpolish(m_sub);
    m_sub->style()->polish(m_sub);
    m_sub->setText(modeText());
    m_progress->setValue(0);
    m_progress->hide();
    m_cancel->hide();
    m_later->show();
    m_action->setText(m_incomplete ? (m_ready ? tr("Concluir instalação") : tr("Baixar e concluir"))
                                   : (m_ready ? tr("Instalar") : tr("Baixar e instalar")));
    m_action->setEnabled(true);
}

QString UpdateBanner::modeText() const
{
    if (m_incomplete)
        return tr("A instalação parou no meio e o Qenna ficou com uma parte dos arquivos da versão nova. "
                  "Conclua pra ter a versão inteira; seus projetos ficam onde estão.");
    if (m_ready)
        return tr("O instalador já está baixado. O Qenna fecha pra instalar; seus projetos ficam onde estão.");
    return idleText();
}

void UpdateBanner::setNotesOpen(bool open)
{
    m_notesScroll->setVisible(open);
    m_notesBtn->setText(open ? tr("Esconder novidades ▴") : tr("Ver novidades ▾"));
}

void UpdateBanner::applyTheme()
{
    const QColor accent = Theme::toColor(Theme::accentDefault());
    const QColor bg = Theme::toColor(Theme::panelBackground());
    // Fundo = painel puxado pro destaque; borda = destaque meio apagado.
    auto mix = [](const QColor& a, const QColor& b, qreal t) {
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                                a.greenF() + (b.greenF() - a.greenF()) * t,
                                a.blueF() + (b.blueF() - a.blueF()) * t);
    };
    const QColor tint = mix(bg, accent, 0.12);
    const QColor border = mix(bg, accent, 0.45);
    // Texto do botão: branco ou quase preto, o que ler melhor no destaque.
    const QColor ink = accent.lightnessF() > 0.62 ? QColor(QStringLiteral("#15171c")) : QColor(QStringLiteral("#f7f8fa"));
    m_badge->setPixmap(badgePixmap(accent, ink, devicePixelRatioF(), m_info));
    // Texto de apoio: o do painel um pouco apagado — o "muted" do tema some
    // no fundo tingido.
    const QColor body = mix(Theme::toColor(Theme::textPrimary()), tint, 0.22);

    setStyleSheet(Theme::qss(QStringLiteral(R"(
        QFrame#updateBanner { background: %1; border: 1px solid %2; border-radius: @radius-panel; }
        QLabel#ubTitle { color: %3; font-size: 16px; font-weight: 600; background: transparent; }
        QLabel#ubSub { color: %4; font-size: 12px; background: transparent; }
        QLabel#ubSub[error="true"] { color: %8; font-weight: 600; }
        QLabel#ubNotes { color: %4; font-size: 12px; background: transparent; }
        QScrollArea#ubNotesScroll { background: transparent; border: none; }
        QToolButton#ubNotesBtn { background: transparent; border: none; color: %5; font-size: 12px; padding: 2px 0px; }
        QToolButton#ubNotesBtn:hover { color: %3; }
        QPushButton#ubAction {
            background: %5; color: %6; border: 1px solid %5; border-radius: @radius-control;
            padding: 8px 18px; font-size: 13px; font-weight: 600;
        }
        QPushButton#ubAction:hover { border-color: %3; }
        QPushButton#ubAction:disabled { background: %2; border-color: %2; }
        QPushButton#ubGhost {
            background: transparent; color: %4; border: 1px solid %2; border-radius: @radius-control;
            padding: 8px 14px; font-size: 13px;
        }
        QPushButton#ubGhost:hover { color: %3; background: %7; }
        QProgressBar#ubProgress { background: %2; border: none; border-radius: 3px; }
        QProgressBar#ubProgress::chunk { background: %5; border-radius: 3px; }
    )")).arg(tint.name(), border.name(), Theme::textBright(), body.name(),
             accent.name(), ink.name(), Theme::hoverOverlay(), Theme::accentDanger()));
}
