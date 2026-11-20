// Busca na própria barra (19/11/2026, ideia do Pe). Nas faixas de uma linha
// (a Giratória e as irmãs) sobra um espaço enorme depois do Buscar; clicar nele
// abre ali mesmo o campo. Escreveu e deu Enter: procura no documento aberto
// (a mesma busca do Ctrl+F, com as ocorrências marcadas na folha, o contador e
// as setinhas). A caixinha "Livro inteiro", no fim do campo, troca pra busca do
// projeto (a do Ctrl+Shift+F), com os resultados pendurados embaixo do campo.
// Quem faz as buscas é a MainWindow; aqui mora só o campo.

#include "TopToolbar.h"

#include "Theme.h"
#include "UiScale.h"

#include <QApplication>
#include <QCheckBox>
#include <QFrame>
#include <QStyle>
#include <functional>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QToolButton>

#include "CarouselViewport.h"

namespace {

int fsc(int v) { return qMax(1, qRound(v * UiScale::scale())); }

// Campo que manda as teclas de navegação pra barra em vez de engolir.
class InlineFindEdit : public QLineEdit
{
public:
    using QLineEdit::QLineEdit;
    std::function<bool(QKeyEvent*)> onKey;

protected:
    void keyPressEvent(QKeyEvent* e) override
    {
        if (onKey && onKey(e)) { e->accept(); return; }
        QLineEdit::keyPressEvent(e);
    }
};

// Clique fora do campo (e fora da lista do "Livro inteiro") fecha a busca.
class InlineFindOutsideClick : public QObject
{
public:
    using QObject::QObject;
    std::function<void(QObject*)> onPress;

protected:
    bool eventFilter(QObject* watched, QEvent* e) override
    {
        if (onPress && e->type() == QEvent::MouseButtonPress) onPress(watched);
        return false;
    }
};

} // namespace

void TopToolbar::buildInlineFind()
{
    m_inlineFind = new QFrame(m_ribbonParking);
    m_inlineFind->setObjectName(QStringLiteral("ribbonFind"));
    m_inlineFind->setAttribute(Qt::WA_StyledBackground, true);
    m_inlineFind->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_inlineFind->hide();
    auto* lay = new QHBoxLayout(m_inlineFind);
    lay->setSpacing(fsc(4));

    // A lupa mora onde estava o Buscar: clicar de novo fecha (pedido do Pe).
    m_inlineFindIcon = new QToolButton(m_inlineFind);
    m_inlineFindIcon->setObjectName(QStringLiteral("ribbonFindBtn"));
    m_inlineFindIcon->setCursor(Qt::PointingHandCursor);
    m_inlineFindIcon->setAutoRaise(true);
    m_inlineFindIcon->setFocusPolicy(Qt::NoFocus);
    m_inlineFindIcon->setToolTip(QCoreApplication::translate("FindBar", "Fechar (Esc)"));
    lay->addWidget(m_inlineFindIcon, 0, Qt::AlignVCenter);

    auto* edit = new InlineFindEdit(m_inlineFind);
    edit->setObjectName(QStringLiteral("ribbonFindEdit"));
    edit->setFrame(false);
    m_inlineFindEdit = edit;
    lay->addWidget(edit, 1);

    m_inlineFindCount = new QLabel(m_inlineFind);
    m_inlineFindCount->setObjectName(QStringLiteral("ribbonFindCount"));
    lay->addWidget(m_inlineFindCount, 0, Qt::AlignVCenter);

    auto mini = [this, lay](const QString& text, const QString& tip) {
        auto* b = new QToolButton(m_inlineFind);
        b->setObjectName(QStringLiteral("ribbonFindBtn"));
        b->setText(text);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setAutoRaise(true);
        lay->addWidget(b, 0, Qt::AlignVCenter);
        return b;
    };
    m_inlineFindPrev = mini(QStringLiteral("‹"), QCoreApplication::translate("FindBar", "Anterior (Shift+Enter)"));
    m_inlineFindNext = mini(QStringLiteral("›"), QCoreApplication::translate("FindBar", "Próximo (Enter)"));

    auto* sep = new QFrame(m_inlineFind);
    sep->setObjectName(QStringLiteral("ribbonFindSep"));
    sep->setFixedWidth(1);
    m_inlineFindSep = sep;
    lay->addWidget(sep, 0, Qt::AlignVCenter);

    m_inlineFindBook = new QCheckBox(tr("Livro inteiro"), m_inlineFind);
    m_inlineFindBook->setObjectName(QStringLiteral("ribbonFindBook"));
    m_inlineFindBook->setCursor(Qt::PointingHandCursor);
    m_inlineFindBook->setFocusPolicy(Qt::NoFocus);   // o foco fica no campo
    m_inlineFindBook->setToolTip(tr("Procura em todos os capítulos, cenas e gavetas do projeto (Ctrl+Shift+F)"));
    lay->addWidget(m_inlineFindBook, 0, Qt::AlignVCenter);

    m_inlineFindClose = mini(QString(), QCoreApplication::translate("FindBar", "Fechar (Esc)"));

    edit->onKey = [this](QKeyEvent* e) {
        const bool book = m_inlineFindBook->isChecked();
        switch (e->key()) {
        case Qt::Key_Escape:
            closeInlineFind();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (book) emit inlineFindNavKey(Qt::Key_Return);
            else emit inlineFindStep((e->modifiers() & Qt::ShiftModifier) ? -1 : 1);
            return true;
        case Qt::Key_Up:
        case Qt::Key_Down:
            if (!book) return false;
            emit inlineFindNavKey(e->key());
            return true;
        default:
            return false;
        }
    };
    connect(edit, &QLineEdit::textChanged, this, [this](const QString& text) {
        refreshInlineFindParts();
        emit inlineFindChanged(text, m_inlineFindBook->isChecked());
    });
    connect(m_inlineFindPrev, &QToolButton::clicked, this, [this]() { emit inlineFindStep(-1); m_inlineFindEdit->setFocus(); });
    connect(m_inlineFindNext, &QToolButton::clicked, this, [this]() { emit inlineFindStep(1); m_inlineFindEdit->setFocus(); });
    connect(m_inlineFindClose, &QToolButton::clicked, this, [this]() { closeInlineFind(); });
    connect(m_inlineFindIcon, &QToolButton::clicked, this, [this]() { closeInlineFind(); });

    auto* outside = new InlineFindOutsideClick(this);
    outside->onPress = [this](QObject* watched) {
        if (!m_inlineFindOpen) return;
        auto* w = qobject_cast<QWidget*>(watched);
        if (!w) return;
        if (w == m_inlineFind || m_inlineFind->isAncestorOf(w)) return;
        if (m_inlineFindCompanion && (w == m_inlineFindCompanion || m_inlineFindCompanion->isAncestorOf(w))) return;
        // Menus e popups (o da fonte, o do Q) não contam como "fora".
        if (w->window()->windowFlags() & Qt::Popup) return;
        closeInlineFind();
    };
    m_inlineFindOutside = outside;
    connect(m_inlineFindBook, &QCheckBox::toggled, this, [this](bool on) {
        refreshInlineFindParts();
        m_inlineFindEdit->setFocus();
        emit inlineFindChanged(m_inlineFindEdit->text(), on);
    });

    refreshInlineFindParts();
}

void TopToolbar::refreshInlineFindParts()
{
    if (!m_inlineFind) return;
    const bool book = m_inlineFindBook->isChecked();
    const bool hasText = !m_inlineFindEdit->text().isEmpty();
    m_inlineFindEdit->setPlaceholderText(book ? tr("Buscar no livro inteiro…") : tr("Buscar neste documento…"));
    // Contador e setinhas só fazem sentido no documento (no livro, a lista embaixo
    // é que anda com ↑ ↓).
    m_inlineFindCount->setVisible(!book && hasText);
    m_inlineFindPrev->setVisible(!book && hasText);
    m_inlineFindNext->setVisible(!book && hasText);
    // Barra apertada (o carrossel girando): o campo aparece inteiro, caixinha
    // inclusa, depois que o layout assentar.
    if (m_inlineFindOpen && m_ribbonCarousel)
        QTimer::singleShot(40, this, [this]() {
            if (m_inlineFindOpen && m_ribbonCarousel && m_ribbonCarousel->isOverflowing())
                m_ribbonCarousel->ensureVisible(m_inlineFind, false);
        });
}

void TopToolbar::styleInlineFind()
{
    if (!m_inlineFind) return;
    const int h = fsc(28);
    m_inlineFind->setFixedHeight(h);
    m_inlineFind->setMinimumWidth(fsc(240));
    m_inlineFindEdit->setMinimumWidth(fsc(70));
    if (auto* lay = m_inlineFind->layout()) lay->setContentsMargins(fsc(8), 0, fsc(3), 0);
    const int ipx = fsc(14);
    m_inlineFindIcon->setIcon(toolbarIcon(QStringLiteral("search.svg"), ipx));
    m_inlineFindIcon->setIconSize(QSize(ipx, ipx));
    m_inlineFindClose->setIcon(toolbarIcon(QStringLiteral("close.svg"), fsc(12)));
    m_inlineFindClose->setIconSize(QSize(fsc(12), fsc(12)));
    for (QToolButton* b : { m_inlineFindIcon, m_inlineFindPrev, m_inlineFindNext, m_inlineFindClose })
        b->setFixedSize(fsc(22), fsc(22));
    m_inlineFindSep->setFixedHeight(fsc(16));
    // Fichário e Giratória: caixas sem contorno, como as de fonte.
    const bool binder = m_barStyle == BarStyle::RibbonBinder || m_barStyle == BarStyle::RibbonReel;
    m_inlineFind->setProperty("comboLook", binder ? QVariant(QStringLiteral("binder")) : QVariant());
    m_inlineFind->style()->unpolish(m_inlineFind);
    m_inlineFind->style()->polish(m_inlineFind);
}

QString TopToolbar::inlineFindQss() const
{
    return Theme::qss(QStringLiteral(R"(
        QFrame#ribbonFind { background: %1; border: 1px solid %2; border-radius: @radius-control; }
        QFrame#ribbonFind[comboLook="binder"] { border-color: transparent; }
        QLineEdit#ribbonFindEdit { background: transparent; border: none; padding: 0px 2px; color: %3;
                                   font-size: 12px; selection-background-color: %4; }
        QLabel#ribbonFindCount { background: transparent; color: %5; font-size: 11px; padding: 0px 2px; }
        QFrame#ribbonFindSep { background: %7; border: none; }
        QToolButton#ribbonFindBtn { background: transparent; border: none; border-radius: @radius-control;
                                    color: %3; font-size: 15px; padding: 0px; min-width: 0px; }
        QToolButton#ribbonFindBtn:hover { background: %6; }
        QCheckBox#ribbonFindBook { background: transparent; color: %5; font-size: 11px; spacing: 5px; padding: 0px 4px; }
        QCheckBox#ribbonFindBook:hover, QCheckBox#ribbonFindBook:checked { color: %3; }
        QCheckBox#ribbonFindBook::indicator:unchecked { border: 1px solid %5; border-radius: 3px; background: transparent; }
        QCheckBox#ribbonFindBook::indicator:checked { border: 1px solid %4; border-radius: 3px; }
    )")).arg(Theme::inputBackground(), Theme::panelBorder(), Theme::textPrimary(),
            Theme::accentDefault(), Theme::textMuted(), Theme::hoverOverlay(), Theme::subtleBorder());
}

bool TopToolbar::canInlineFind() const
{
    return ribbonActive() && ribbonOneLine() && isVisible() && m_inlineFind && searchButton
        && m_inlineFind->parentWidget() != m_ribbonParking
        && !m_rightBarButtons.contains(searchButton);
}

bool TopToolbar::openInlineFind(bool wholeBook)
{
    if (!canInlineFind()) return false;
    if (m_ribbonTab != 0) setRibbonTab(0);   // o Buscar mora no Editor
    m_inlineFindBook->blockSignals(true);
    m_inlineFindBook->setChecked(wholeBook);
    m_inlineFindBook->blockSignals(false);
    refreshInlineFindParts();
    if (!m_inlineFindOpen && m_inlineFindOutside) qApp->installEventFilter(m_inlineFindOutside);
    m_inlineFindOpen = true;
    searchButton->hide();
    m_inlineFind->show();
    refreshInlineFindParts();   // de novo, já aberto: leva o carrossel até o campo
    m_inlineFindEdit->setFocus();
    m_inlineFindEdit->selectAll();
    // Reaberto com texto, as ocorrências voltam na hora.
    emit inlineFindChanged(m_inlineFindEdit->text(), wholeBook);
    return true;
}

void TopToolbar::closeInlineFind()
{
    if (!m_inlineFindOpen) return;
    m_inlineFindOpen = false;
    if (m_inlineFindOutside) qApp->removeEventFilter(m_inlineFindOutside);
    if (m_inlineFind) m_inlineFind->hide();
    if (searchButton) searchButton->show();
    emit inlineFindClosed();
}

bool TopToolbar::inlineFindWholeBook() const
{
    return m_inlineFindBook && m_inlineFindBook->isChecked();
}

void TopToolbar::setInlineFindWholeBook(bool on)
{
    if (m_inlineFindBook) m_inlineFindBook->setChecked(on);   // toggled refaz a busca
}

void TopToolbar::setInlineFindText(const QString& text)
{
    if (m_inlineFindEdit && m_inlineFindEdit->text() != text) {
        m_inlineFindEdit->setText(text);
        m_inlineFindEdit->end(false);
    }
}

void TopToolbar::setInlineFindCount(int active, int total)
{
    if (!m_inlineFindCount) return;
    m_inlineFindCount->setText(QStringLiteral("%1/%2").arg(active).arg(total));
}

QRect TopToolbar::inlineFindGlobalRect() const
{
    if (!m_inlineFind || !m_inlineFind->isVisible()) return QRect();
    return QRect(m_inlineFind->mapToGlobal(QPoint(0, 0)), m_inlineFind->size());
}

void TopToolbar::setInlineFindCompanion(QWidget* w)
{
    m_inlineFindCompanion = w;
}
