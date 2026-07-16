#include "FindBar.h"

#include "IconUtils.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>

namespace {
constexpr int kBarHeight = 36;
}

FindBar::FindBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("findBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(kBarHeight);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 4, 8, 4);
    lay->setSpacing(6);

    m_input = new QLineEdit(this);
    m_input->setObjectName(QStringLiteral("findBarInput"));
    m_input->setPlaceholderText(tr("Buscar no texto..."));
    m_input->setMinimumWidth(220);
    lay->addWidget(m_input, 1);

    m_count = new QLabel(QStringLiteral("0/0"), this);
    m_count->setObjectName(QStringLiteral("findBarCount"));
    m_count->setMinimumWidth(40);
    m_count->setAlignment(Qt::AlignCenter);
    lay->addWidget(m_count);

    auto buildBtn = [this](const QString& iconRes, const QString& fallback, const QString& tip) {
        auto* btn = new QToolButton(this);
        btn->setObjectName(QStringLiteral("findBarBtn"));
        btn->setAutoRaise(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tip);
        if (!iconRes.isEmpty()) {
            const QIcon ic = IconUtils::loadToolbarIcon(iconRes,
                QColor(Theme::textMuted()),
                QColor(Theme::textPrimary()),
                QColor(Theme::textPrimary()),
                QSize(14, 14));
            if (!ic.isNull()) btn->setIcon(ic);
        }
        if (btn->icon().isNull()) btn->setText(fallback);
        btn->setFixedSize(26, 26);
        return btn;
    };

    m_prev = buildBtn(QString(), QStringLiteral("‹"), tr("Anterior (Shift+Enter)"));
    m_next = buildBtn(QString(), QStringLiteral("›"), tr("Próximo (Enter)"));
    m_close = buildBtn(QStringLiteral(":/icons/close.svg"), QStringLiteral("✕"), tr("Fechar (Esc)"));

    lay->addWidget(m_prev);
    lay->addWidget(m_next);
    lay->addWidget(m_close);

    applyTheme();

    connect(m_input, &QLineEdit::textChanged, this, &FindBar::onQueryChanged);
    connect(m_input, &QLineEdit::returnPressed, this, &FindBar::onNext);
    connect(m_prev, &QToolButton::clicked, this, &FindBar::onPrev);
    connect(m_next, &QToolButton::clicked, this, &FindBar::onNext);
    connect(m_close, &QToolButton::clicked, this, &FindBar::onCloseClicked);

    hide();
}

void FindBar::attachTo(QTextEdit* editor)
{
    m_editor = editor;
}

void FindBar::openBar()
{
    if (!m_editor) return;
    show();
    raise();
    refreshMatches();
    m_input->setFocus();
    m_input->selectAll();
}

void FindBar::closeBar()
{
    m_matches.clear();
    m_activeIndex = 0;
    m_selections.clear();
    emit selectionsChanged();
    hide();
    emit closed();
}

void FindBar::applyTheme()
{
    setStyleSheet(QStringLiteral(R"(
        #findBar {
            background: %1;
            border: 1px solid %2;
            border-radius: 8px;
        }
        #findBarInput {
            background: %3;
            color: %4;
            border: 1px solid %2;
            border-radius: 6px;
            padding: 4px 8px;
            selection-background-color: %5;
        }
        #findBarInput:focus { border-color: %5; }
        #findBarCount {
            color: %6;
            font-size: 11px;
        }
        QToolButton#findBarBtn {
            border: none;
            background: transparent;
            color: %4;
            border-radius: 5px;
            font-size: 14px;
        }
        QToolButton#findBarBtn:hover { background: %7; }
    )")
        .arg(Theme::panelBackground())
        .arg(Theme::panelBorder())
        .arg(Theme::inputBackground())
        .arg(Theme::textPrimary())
        .arg(Theme::accentDefault())
        .arg(Theme::textMuted())
        .arg(Theme::hoverOverlay()));
}

void FindBar::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        event->accept();
        closeBar();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        event->accept();
        if (event->modifiers() & Qt::ShiftModifier) onPrev();
        else onNext();
        return;
    }
    QWidget::keyPressEvent(event);
}

void FindBar::onQueryChanged(const QString& /*q*/)
{
    refreshMatches();
}

void FindBar::onPrev()
{
    if (m_matches.isEmpty()) return;
    m_activeIndex = (m_activeIndex - 1 + m_matches.size()) % m_matches.size();
    updateCountLabel();
    rebuildSelections();
    scrollToMatch(m_activeIndex);
}

void FindBar::onNext()
{
    if (m_matches.isEmpty()) {
        refreshMatches();
        if (m_matches.isEmpty()) return;
    }
    m_activeIndex = (m_activeIndex + 1) % m_matches.size();
    updateCountLabel();
    rebuildSelections();
    scrollToMatch(m_activeIndex);
}

void FindBar::onCloseClicked()
{
    closeBar();
}

void FindBar::refreshMatches()
{
    m_matches.clear();
    m_activeIndex = 0;

    const QString q = m_input ? m_input->text() : QString();
    if (!m_editor || q.isEmpty()) {
        m_selections.clear();
        emit selectionsChanged();
        updateCountLabel();
        return;
    }

    QTextDocument* doc = m_editor->document();
    if (!doc) {
        updateCountLabel();
        return;
    }

    QTextCursor c(doc);
    while (true) {
        c = doc->find(q, c, QTextDocument::FindFlags());
        if (c.isNull()) break;
        const int start = c.selectionStart();
        const int end = c.selectionEnd();
        if (end <= start) break;
        m_matches.append({ start, end - start });
        // avança pra evitar loop em strings de comprimento zero
        c.setPosition(end);
    }

    rebuildSelections();
    updateCountLabel();
    if (!m_matches.isEmpty()) scrollToMatch(0);
}

void FindBar::rebuildSelections()
{
    m_selections.clear();
    if (!m_editor || m_matches.isEmpty()) {
        emit selectionsChanged();
        return;
    }

    QTextDocument* doc = m_editor->document();
    const QColor hl(Theme::accentDefault());
    QColor active = hl;
    active.setAlpha(200);
    QColor inactive = hl;
    inactive.setAlpha(90);

    for (int i = 0; i < m_matches.size(); ++i) {
        const auto& m = m_matches.at(i);
        QTextEdit::ExtraSelection sel;
        sel.cursor = QTextCursor(doc);
        sel.cursor.setPosition(m.first);
        sel.cursor.setPosition(m.first + m.second, QTextCursor::KeepAnchor);
        sel.format.setBackground(i == m_activeIndex ? active : inactive);
        m_selections.append(sel);
    }
    emit selectionsChanged();
}

void FindBar::scrollToMatch(int idx)
{
    if (!m_editor) return;
    if (idx < 0 || idx >= m_matches.size()) return;
    QTextCursor c(m_editor->document());
    c.setPosition(m_matches.at(idx).first);
    c.setPosition(m_matches.at(idx).first + m_matches.at(idx).second, QTextCursor::KeepAnchor);
    // Coloca o cursor na ocorrência ativa (também serve pro próximo "Continuar
    // edição daqui"). Não disputa o foco com o input.
    QTextCursor cur = m_editor->textCursor();
    cur.setPosition(c.selectionStart());
    cur.setPosition(c.selectionEnd(), QTextCursor::KeepAnchor);
    m_editor->setTextCursor(cur);
    m_editor->ensureCursorVisible();
    // Devolve o foco pro input (setTextCursor não rouba foco, mas defensivo).
    if (m_input) m_input->setFocus();
}

void FindBar::updateCountLabel()
{
    if (!m_count) return;
    if (m_matches.isEmpty()) m_count->setText(QStringLiteral("0/0"));
    else m_count->setText(QStringLiteral("%1/%2").arg(m_activeIndex + 1).arg(m_matches.size()));
}
