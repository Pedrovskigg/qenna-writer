#include "TimelineBranchPopup.h"

#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>
#include <QVBoxLayout>

TimelineBranchPopup::TimelineBranchPopup(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("timelineBranchPopup"));
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    buildUi();
    hide();
}

void TimelineBranchPopup::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(14, 12, 14, 12);
    outer->setSpacing(8);

    m_captionLabel = new QLabel(tr("Ramificação ambígua"), this);
    m_captionLabel->setObjectName(QStringLiteral("tbpCaption"));
    outer->addWidget(m_captionLabel);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("tbpTitle"));
    m_titleLabel->setWordWrap(true);
    outer->addWidget(m_titleLabel);

    auto* hint = new QLabel(tr("Não deu pra decidir sozinho a qual linha isso pertence:"), this);
    hint->setObjectName(QStringLiteral("tbpHint"));
    hint->setWordWrap(true);
    outer->addWidget(hint);

    m_btnCol = new QVBoxLayout();
    m_btnCol->setSpacing(4);
    outer->addLayout(m_btnCol);

    applyTheme();
}

void TimelineBranchPopup::applyTheme()
{
    setStyleSheet(QStringLiteral(R"(
        QFrame#timelineBranchPopup {
            background: %1;
            border: 1px solid %2;
            border-radius: 8px;
        }
        QLabel#tbpCaption { color: %3; font-size: 10px; }
        QLabel#tbpTitle   { color: %4; font-size: 13px; font-weight: 600; }
        QLabel#tbpHint    { color: %3; font-size: 11px; }
        QPushButton#tbpCandidateBtn {
            background: %5; color: %4;
            border: 1px solid %2; border-radius: 4px;
            padding: 5px 10px; font-size: 11px; text-align: left;
        }
        QPushButton#tbpCandidateBtn:hover { background: %6; }
    )").arg(Theme::panelBackground(),
            Theme::panelBorder(),
            Theme::textMuted(),
            Theme::textPrimary(),
            Theme::inputBackground(),
            Theme::hoverOverlay()));
}

void TimelineBranchPopup::enqueue(const Ambiguity& item)
{
    const bool wasIdle = m_queue.isEmpty() || m_current < 0;
    for (const auto& q : m_queue) {
        if (q.evId == item.evId) return; // já enfileirado
    }
    m_queue.append(item);
    if (wasIdle) showNext();
}

void TimelineBranchPopup::showNext()
{
    if (m_queue.isEmpty()) { hide(); m_current = -1; return; }
    m_current = 0;
    updateContent();
    show();
    raise();
}

void TimelineBranchPopup::advance()
{
    m_queue.removeFirst();
    m_current = 0;
    if (m_queue.isEmpty()) { hide(); m_current = -1; return; }
    updateContent();
}

void TimelineBranchPopup::updateContent()
{
    if (m_current < 0 || m_current >= m_queue.size()) return;
    const Ambiguity& item = m_queue[m_current];
    m_titleLabel->setText(item.title);

    // Reconstrói os botões de candidato — a contagem varia por item.
    QLayoutItem* child;
    while ((child = m_btnCol->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    for (int i = 0; i < item.candidateIds.size(); ++i) {
        const QString candidateId = item.candidateIds[i];
        const QString label = i < item.candidateLabels.size() ? item.candidateLabels[i] : candidateId;
        auto* btn = new QPushButton(label, this);
        btn->setObjectName(QStringLiteral("tbpCandidateBtn"));
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, candidateId]() {
            if (m_current < 0 || m_current >= m_queue.size()) return;
            const QString evId = m_queue[m_current].evId;
            emit branchChosen(evId, candidateId);
            advance();
        });
        m_btnCol->addWidget(btn);
    }

    adjustSize();
}
