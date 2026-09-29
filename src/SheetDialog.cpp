#include "SheetDialog.h"

#include "Theme.h"
#include "TimelineTracksTypes.h"

#include <QApplication>
#include <QButtonGroup>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QStyle>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Tracks;

namespace {
constexpr int kShadow = 22;   // margem transparente em volta, pra sombra caber

QString css(const QColor& c) { return c.name(QColor::HexArgb); }

QIcon closeIcon(const QColor& c)
{
    QPixmap pm(28, 28);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c, 1.6, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(4, 4), QPointF(10, 10));
    p.drawLine(QPointF(10, 4), QPointF(4, 10));
    return QIcon(pm);
}
} // namespace

SheetDialog::SheetDialog(QWidget* parent, int width)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(kShadow, kShadow, kShadow, kShadow);
    // a folha acompanha o conteúdo nos dois sentidos: cresce quando algo abre
    // (lista de papéis, "+ outro POV") e ENCOLHE quando fecha. Só adjustSize()
    // no clique não encolhia: o layout ainda não tinha registrado o hide().
    outer->setSizeConstraint(QLayout::SetFixedSize);
    m_card = new QFrame(this);
    m_card->setObjectName(QStringLiteral("sheetCard"));
    m_card->setFixedWidth(width);
    outer->addWidget(m_card);
    auto* shadow = new QGraphicsDropShadowEffect(m_card);
    shadow->setBlurRadius(42);
    shadow->setOffset(0, 12);
    shadow->setColor(QColor(0, 0, 0, 110));
    m_card->setGraphicsEffect(shadow);

    m_root = new QVBoxLayout(m_card);
    m_root->setContentsMargins(0, 0, 0, 0);
    m_root->setSpacing(0);

    auto* top = new QHBoxLayout;
    top->setContentsMargins(18, 14, 12, 0);
    m_eyebrow = sectionLabel(QString(), m_card);
    top->addWidget(m_eyebrow);
    top->addStretch(1);
    m_close = new QToolButton(m_card);
    m_close->setObjectName(QStringLiteral("sheetX"));
    m_close->setFixedSize(28, 28);
    m_close->setCursor(Qt::PointingHandCursor);
    m_close->setToolTip(tr("Cancelar (Esc)"));
    m_close->setFocusPolicy(Qt::NoFocus);
    connect(m_close, &QToolButton::clicked, this, &QDialog::reject);
    top->addWidget(m_close);
    m_root->addLayout(top);
    m_top = top;

    m_body = new QVBoxLayout;
    m_body->setContentsMargins(26, 14, 26, 8);
    m_body->setSpacing(12);
    m_root->addLayout(m_body);
}

void SheetDialog::replaceHeader(QWidget* header)
{
    m_eyebrow->hide();
    m_close->hide();
    m_top->setContentsMargins(0, 0, 0, 0);
    m_root->insertWidget(0, header);
}

void SheetDialog::setEyebrow(const QString& text)
{
    m_eyebrow->setText(text.toUpper());
    setWindowTitle(text);
}

QLabel* SheetDialog::sectionLabel(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text.toUpper(), parent);
    l->setObjectName(QStringLiteral("sheetSection"));
    QFont f = uiFont(9.5, QFont::DemiBold);
    f.setLetterSpacing(QFont::PercentageSpacing, 113);
    l->setFont(f);
    return l;
}

QLineEdit* SheetDialog::titleEdit(QWidget* parent, bool centered, qreal px)
{
    auto* e = new QLineEdit(parent);
    e->setObjectName(QStringLiteral("sheetTitle"));
    if (centered) e->setAlignment(Qt::AlignCenter);
    e->setFont(serifFont(px));
    e->setFixedHeight(qRound(px * 1.75));
    return e;
}

QLineEdit* SheetDialog::field(QWidget* parent, bool mono)
{
    auto* e = new QLineEdit(parent);
    e->setObjectName(QStringLiteral("sheetFld"));
    e->setFont(mono ? monoFont(12.5) : uiFont(12.5));
    e->setFixedHeight(32);
    return e;
}

QTextEdit* SheetDialog::textArea(QWidget* parent, int height)
{
    auto* t = new QTextEdit(parent);
    t->setObjectName(QStringLiteral("sheetTxa"));
    t->setAcceptRichText(false);
    t->setTabChangesFocus(true);
    t->setFont(serifFont(13.5, QFont::Normal));
    t->document()->setDocumentMargin(4);
    t->setFixedHeight(height);
    return t;
}

QToolButton* SheetDialog::pill(const QString& text, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setObjectName(QStringLiteral("sheetType"));
    b->setText(text);
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    b->setFixedHeight(26);
    b->setFont(uiFont(11.5));
    b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return b;
}

QIcon SheetDialog::chevronIcon(const QColor& c)
{
    QPixmap pm(28, 28);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath path;
    path.moveTo(3.5, 5.5); path.lineTo(7, 9); path.lineTo(10.5, 5.5);
    p.drawPath(path);
    return QIcon(pm);
}

QPushButton* SheetDialog::addFooter(const QString& okText, const QString& enterHint)
{
    auto* sep = new QFrame(m_card);
    sep->setObjectName(QStringLiteral("sheetRule"));
    sep->setFixedHeight(1);
    m_root->addSpacing(8);
    m_root->addWidget(sep);
    auto* foot = new QHBoxLayout;
    foot->setContentsMargins(16, 12, 16, 14);
    foot->setSpacing(8);
    if (!enterHint.isEmpty()) {
        auto* kbd = new QLabel(QStringLiteral("Enter"), m_card);
        kbd->setObjectName(QStringLiteral("sheetKbd"));
        kbd->setFont(monoFont(10.5));
        kbd->setFixedHeight(18);
        m_hint = new QLabel(enterHint, m_card);
        m_hint->setObjectName(QStringLiteral("sheetDim"));
        m_hint->setFont(uiFont(11.5));
        foot->addWidget(kbd, 0, Qt::AlignVCenter);
        foot->addWidget(m_hint, 0, Qt::AlignVCenter);
    }
    foot->addStretch(1);
    auto* cancel = new QPushButton(tr("Cancelar"), m_card);
    cancel->setObjectName(QStringLiteral("sheetBtn"));
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setFixedHeight(30);
    cancel->setAutoDefault(false);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    m_okBtn = new QPushButton(okText, m_card);
    m_okBtn->setObjectName(QStringLiteral("sheetPri"));
    m_okBtn->setCursor(Qt::PointingHandCursor);
    m_okBtn->setFixedHeight(30);
    m_okBtn->setAutoDefault(false);
    connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);
    foot->addWidget(cancel);
    foot->addWidget(m_okBtn);
    m_root->addLayout(foot);
    m_foot = foot;
    return m_okBtn;
}

void SheetDialog::setEnterHint(const QString& text)
{
    if (m_hint) m_hint->setText(text);
}

void SheetDialog::confirmOnEnter(QWidget* w)
{
    if (w) w->installEventFilter(this);
}

bool SheetDialog::eventFilter(QObject* o, QEvent* e)
{
    if (e->type() == QEvent::KeyPress) {
        auto* k = static_cast<QKeyEvent*>(e);
        const bool enter = k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter;
        const bool multi = qobject_cast<QTextEdit*>(o) != nullptr;
        if (enter && (!multi || (k->modifiers() & Qt::ControlModifier))) {
            if (m_okBtn && m_okBtn->isEnabled()) m_okBtn->click();
            return true;
        }
    }
    return QDialog::eventFilter(o, e);
}

int SheetDialog::exec()
{
    QWidget* win = parentWidget() ? parentWidget()->window() : QApplication::activeWindow();
    QPointer<QWidget> scrim;
    if (win && win->isVisible()) {
        scrim = new QWidget(win);
        scrim->setObjectName(QStringLiteral("qennaSheetScrim"));
        scrim->setAttribute(Qt::WA_StyledBackground);
        scrim->setStyleSheet(QStringLiteral("QWidget#qennaSheetScrim { background: rgba(0,0,0,0.38); }"));
        scrim->setGeometry(win->rect());
        scrim->show();
        scrim->raise();
    }
    fitToContent();
    if (win) move(win->geometry().center() - QPoint(width() / 2, height() / 2));
    const int r = QDialog::exec();
    if (scrim) scrim->deleteLater();
    return r;
}

void SheetDialog::openBeside(QWidget* anchor)
{
    setModal(false);
    // quem estava escrevendo continua escrevendo: a folha só pega o teclado
    // quando alguém clica nela
    setAttribute(Qt::WA_ShowWithoutActivating);
    fitToContent();
    QWidget* win = parentWidget() ? parentWidget()->window() : nullptr;
    if (anchor && anchor->isVisible()) {
        // a borda da folha (não a da sombra) a 12px do painel, topo alinhado com ele
        const QPoint tr = anchor->mapToGlobal(QPoint(anchor->width(), 0));
        move(tr.x() + 12 - kShadow, tr.y() - kShadow);
    } else if (win) {
        move(win->geometry().center() - QPoint(width() / 2, height() / 2));
    }
    keepOnScreen();
    show();
}

void SheetDialog::openInside(QWidget* area)
{
    setModal(false);
    setAttribute(Qt::WA_ShowWithoutActivating);
    fitToContent();
    if (area && area->isVisible()) {
        const QPoint tl = area->mapToGlobal(QPoint(0, 0));
        move(tl.x() + 12 - kShadow, tl.y() + 12 - kShadow);
    } else if (QWidget* win = parentWidget() ? parentWidget()->window() : nullptr) {
        move(win->geometry().center() - QPoint(width() / 2, height() / 2));
    }
    keepOnScreen();
    show();
}

void SheetDialog::fitToContent()
{
    // Recalcula o tamanho ANTES de posicionar: quem mudou de altura antes de
    // abrir (o pôster do personagem, que já abre com a foto no modo editar)
    // ainda não passou pelo layout, e a folha era posicionada com a altura
    // velha e depois crescia só pra baixo.
    // (a folha interna só repassa a altura nova no LayoutRequest, que fica na
    // fila até alguém processar: invalidar o layout de fora não basta)
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    m_root->activate();
    if (layout()) { layout()->invalidate(); layout()->activate(); }
    adjustSize();
}

void SheetDialog::keepOnScreen()
{
    QScreen* scr = screen();
    if (!scr) return;
    const QRect avail = scr->availableGeometry();
    QRect g = frameGeometry();
    // a sombra pode sair da tela; a folha, não
    if (g.bottom() - kShadow > avail.bottom() - 8) g.moveBottom(avail.bottom() - 8 + kShadow);
    if (g.top() + kShadow < avail.top() + 8) g.moveTop(avail.top() + 8 - kShadow);
    // ao lado de um painel encostado na direita da tela, não sai pela borda
    if (g.right() - kShadow > avail.right() - 8) g.moveRight(avail.right() - 8 + kShadow);
    if (g.left() + kShadow < avail.left() + 8) g.moveLeft(avail.left() + 8 - kShadow);
    if (g.topLeft() != frameGeometry().topLeft()) move(g.topLeft());
}

void SheetDialog::resizeEvent(QResizeEvent* e)
{
    QDialog::resizeEvent(e);
    // aberta ao lado de um painel, ela cresce pra baixo (lista de papéis, foto):
    // não deixa o rodapé sumir embaixo da barra de tarefas
    if (!isModal() && isVisible()) keepOnScreen();
}

void SheetDialog::setPlaceholderColors()
{
    const Palette pal = Palette::current();
    const QColor ph = mix(pal.ink, pal.page, 0.42);
    for (QWidget* w : findChildren<QWidget*>()) {
        if (!qobject_cast<QLineEdit*>(w) && !qobject_cast<QTextEdit*>(w)) continue;
        QPalette p = w->palette();
        p.setColor(QPalette::PlaceholderText, ph);
        w->setPalette(p);
    }
}

void SheetDialog::applySheetTheme(const QString& extraQss)
{
    const Palette pal = Palette::current();
    const QColor fieldBg = mix(pal.app, pal.page, 0.55);
    m_close->setIcon(closeIcon(pal.dim));
    setStyleSheet(Theme::qss(QStringLiteral(R"(
        QFrame#sheetCard { background: %1; border: 1px solid %2; border-radius: 10px; }
        QLabel#sheetSection { color: %3; background: transparent; }
        QLabel#sheetDim { color: %3; background: transparent; }
        QFrame#sheetRule { background: %4; border: none; }
        QToolButton#sheetX { background: transparent; border: none; border-radius: @radius-control; }
        QToolButton#sheetX:hover { background: %5; }
        QToolButton#sheetType { background: transparent; border: 1px solid %2; border-radius: 13px; color: %3; padding: 0 9px; }
        QToolButton#sheetType:hover { color: %6; border-color: %7; }
        QToolButton#sheetType:checked { color: %6; border-color: %8; background: %9; }
        QLineEdit#sheetTitle { background: transparent; border: none; border-bottom: 1px solid transparent;
                               color: %6; selection-background-color: %10; padding: 0; }
        QLineEdit#sheetTitle:focus { border-bottom: 1px solid %8; }
        QLineEdit#sheetFld { background: %11; border: 1px solid %2; border-radius: @radius-control; color: %6;
                             padding: 0 10px; selection-background-color: %10; }
        QLineEdit#sheetFld:focus { border-color: %12; }
        QTextEdit#sheetTxa { background: %11; border: 1px solid %2; border-radius: @radius-control; color: %13;
                             padding: 2px 4px; selection-background-color: %10; }
        QTextEdit#sheetTxa:focus { border-color: %12; }
        QLabel#sheetLei { color: %3; background: transparent; }
        QLabel#sheetLei[kind="ok"] { color: %14; }
        QLabel#sheetLei[kind="bad"] { color: %15; }
        QToolButton#sheetQuick { background: %16; color: %12; border: none; border-radius: 11px; padding: 0 8px; }
        QToolButton#sheetQuick:hover { background: %17; }
        QToolButton#sheetAdd { background: transparent; border: 1px dashed %7; border-radius: 13px; color: %3;
                               padding: 0 11px; font-size: 12px; }
        QToolButton#sheetAdd:hover { color: %6; border-color: %12; }
        QToolButton#sheetLink { background: transparent; border: none; color: %3; font-size: 11.5px; padding: 0; }
        QToolButton#sheetLink:hover { color: %6; text-decoration: underline; }
        QCheckBox#sheetChk { color: %13; font-size: 12.5px; spacing: 8px; background: transparent; }
        QLabel#sheetKbd { color: %13; background: transparent; border: 1px solid %2; border-radius: 3px; padding: 0 5px; }
        QPushButton#sheetBtn { background: transparent; border: 1px solid %2; border-radius: @radius-control;
                               color: %13; font-size: 12.5px; padding: 0 13px; }
        QPushButton#sheetBtn:hover { border-color: %7; }
        QPushButton#sheetPri { background: %12; border: none; border-radius: @radius-control;
                               color: %18; font-size: 12.5px; font-weight: 600; padding: 0 16px; }
        QPushButton#sheetPri:hover { background: %19; }
        QPushButton#sheetPri:disabled { background: %7; color: %1; }
        QPushButton#sheetPick { background: %11; border: 1px solid %2; border-radius: 6px; text-align: left; }
        QPushButton#sheetPick:hover { border-color: %7; }
        QLabel#sheetPickText { background: transparent; color: %6; }
        QFrame#sheetChoiceList { background: %11; border: 1px solid %2; border-radius: 8px; }
        QPushButton#sheetChoiceCell { background: transparent; border: 1px solid transparent; border-radius: 6px;
                                      color: %13; text-align: left; padding: 0 8px; font-size: 12.5px; }
        QPushButton#sheetChoiceCell:hover { background: %5; color: %6; }
        QPushButton#sheetChoiceCell[sel="true"] { background: %9; border-color: %8; color: %6; }
    )")).arg(pal.page.name(),                              // 1
             pal.border.name(),                            // 2
             pal.dim.name(),                               // 3
             css(alpha(pal.ink, 0.14)),                    // 4
             css(alpha(pal.ink, 0.08)),                    // 5
             pal.bright.name(),                            // 6
             css(alpha(pal.ink, 0.40)),                    // 7
             css(alpha(pal.accent, 0.65)),                 // 8
             css(alpha(pal.accent, 0.15)))                 // 9
         .arg(css(alpha(pal.accent, 0.35)),                // 10
              fieldBg.name(),                              // 11
              pal.accent.name(),                           // 12
              pal.ink.name(),                              // 13
              pal.success.name(),                          // 14
              pal.warning.name(),                          // 15
              css(alpha(pal.accent, 0.11)),                // 16
              css(alpha(pal.accent, 0.22)),                // 17
              pal.app.name(),                              // 18
              mix(pal.accent, pal.bright, 0.85).name())    // 19
        + extraQss);
    setPlaceholderColors();
}

// ── SheetChoice ──────────────────────────────────────────────────────────────

SheetChoice::SheetChoice(const QStringList& labels, QWidget* parent, Qt::Alignment pillsAlign, int availWidth)
    : QWidget(parent), m_labels(labels)
{
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);

    if (labels.size() <= kMaxPills) {
        m_group = new QButtonGroup(this);
        m_group->setExclusive(true);
        QHBoxLayout* row = nullptr;
        int used = 0;
        auto closeRow = [&]() {
            if (row && (pillsAlign & (Qt::AlignRight | Qt::AlignHCenter))) row->insertStretch(0, 1);
            if (row && !(pillsAlign & Qt::AlignRight)) row->addStretch(1);
        };
        for (int i = 0; i < labels.size(); ++i) {
            QToolButton* b = SheetDialog::pill(labels[i], this);
            const int w = b->fontMetrics().horizontalAdvance(labels[i]) + 30;   // padding 9+9, borda e a folga do estilo
            if (!row || (availWidth > 0 && used + w > availWidth)) {
                closeRow();
                row = new QHBoxLayout;
                row->setSpacing(5);
                v->addLayout(row);
                used = 0;
            }
            row->addWidget(b);
            used += w + 5;
            m_group->addButton(b, i);
        }
        closeRow();
        connect(m_group, &QButtonGroup::idClicked, this, [this](int id) {
            m_index = id;
            emit activated(id);
        });
        return;
    }

    // a linha: o escolhido + a setinha
    m_line = new QPushButton(this);
    m_line->setObjectName(QStringLiteral("sheetPick"));
    m_line->setCursor(Qt::PointingHandCursor);
    m_line->setFixedHeight(30);
    m_line->setAutoDefault(false);
    m_line->setFocusPolicy(Qt::NoFocus);
    {
        auto* h = new QHBoxLayout(m_line);
        h->setContentsMargins(10, 0, 8, 0);
        m_lineText = new QLabel(m_line);
        m_lineText->setObjectName(QStringLiteral("sheetPickText"));
        m_lineText->setFont(Tracks::uiFont(12.5));
        m_lineText->setAttribute(Qt::WA_TransparentForMouseEvents);
        h->addWidget(m_lineText, 1);
        auto* chev = new QLabel(m_line);
        chev->setPixmap(SheetDialog::chevronIcon(Tracks::Palette::current().dim).pixmap(12, 12));
        chev->setAttribute(Qt::WA_TransparentForMouseEvents);
        h->addWidget(chev);
    }
    v->addWidget(m_line);
    connect(m_line, &QPushButton::clicked, this, [this]() { toggleList(!m_list->isVisible()); });

    // a lista, na própria folha: duas colunas
    m_list = new QFrame(this);
    m_list->setObjectName(QStringLiteral("sheetChoiceList"));
    auto* grid = new QGridLayout(m_list);
    grid->setContentsMargins(3, 3, 3, 3);
    grid->setSpacing(2);
    const int rows = (int(labels.size()) + 1) / 2;
    for (int i = 0; i < labels.size(); ++i) {
        auto* cell = new QPushButton(m_list);
        cell->setObjectName(QStringLiteral("sheetChoiceCell"));
        cell->setCursor(Qt::PointingHandCursor);
        cell->setAutoDefault(false);
        cell->setFocusPolicy(Qt::NoFocus);
        cell->setFixedHeight(28);
        cell->setMinimumWidth(0);
        cell->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        cell->setText(labels[i]);
        cell->setToolTip(labels[i]);
        // coluna por coluna (de cima pra baixo), como se lê uma lista
        grid->addWidget(cell, i % rows, i / rows);
        m_cells << cell;
        connect(cell, &QPushButton::clicked, this, [this, i]() {
            m_index = i;
            refresh();
            toggleList(false);
            emit activated(i);
        });
    }
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    m_list->hide();
    v->addWidget(m_list);
    refresh();
}

void SheetChoice::setCurrentIndex(int i)
{
    if (i < 0 || i >= m_labels.size()) return;
    m_index = i;
    if (m_group) {
        if (QAbstractButton* b = m_group->button(i)) b->setChecked(true);
        return;
    }
    refresh();
}

void SheetChoice::setAlwaysOpen()
{
    if (!m_line) return;
    m_line->hide();
    m_list->show();
}

void SheetChoice::refresh()
{
    if (!m_line) return;
    m_lineText->setText(m_index >= 0 ? m_labels[m_index] : QString());
    for (int i = 0; i < m_cells.size(); ++i) {
        QPushButton* cell = m_cells[i];
        const bool on = i == m_index;
        if (cell->property("sel").toBool() == on) continue;
        cell->setProperty("sel", on);
        cell->style()->unpolish(cell);
        cell->style()->polish(cell);
    }
}

void SheetChoice::toggleList(bool open)
{
    if (!m_list || !m_line->isVisible()) return;
    m_list->setVisible(open);
    // a folha acompanha (SetFixedSize no layout de fora); adjustSize garante
    if (QWidget* w = window()) w->adjustSize();
}
