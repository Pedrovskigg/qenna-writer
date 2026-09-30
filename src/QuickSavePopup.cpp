#include "QuickSavePopup.h"

#include "IconUtils.h"
#include "Theme.h"
#include "TimelineTracksTypes.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Tracks;

namespace {
constexpr int kWidth = 240;
constexpr int kRowH = 28;
constexpr int kMaxRows = 9;   // mais que isso a lista rola
}

QuickSavePopup::QuickSavePopup(QTextEdit* editor, QWidget* parent)
    : QFrame(parent)
    , m_editor(editor)
{
    setObjectName(QStringLiteral("quickSavePopup"));
    // mesma receita do menu de seleção: flutua, não ativa, não pega o teclado
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::NoDropShadowWindowHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    // é de passagem: com o app minimizado e restaurado, não volta sozinho
    setProperty("qennaNoRestore", true);
    setFixedWidth(kWidth);

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(6, 8, 6, 6);
    v->setSpacing(4);

    m_header = new QLabel(this);
    m_header->setObjectName(QStringLiteral("qsHeader"));
    m_header->setFont(uiFont(10.5));
    m_header->setContentsMargins(6, 0, 6, 2);
    m_header->setToolTip(tr("Clique duas vezes pra dar outro nome"));
    m_header->installEventFilter(this);
    v->addWidget(m_header);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName(QStringLiteral("qsName"));
    m_nameEdit->setFont(serifFont(13.5, QFont::Normal));
    m_nameEdit->setFixedHeight(28);
    m_nameEdit->installEventFilter(this);
    m_nameEdit->hide();
    v->addWidget(m_nameEdit);

    m_rows = new QScrollArea(this);
    m_rows->setFrameShape(QFrame::NoFrame);
    m_rows->setWidgetResizable(true);
    m_rows->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_rows->setFocusPolicy(Qt::NoFocus);
    m_rows->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    auto* inner = new QWidget(m_rows);
    inner->setObjectName(QStringLiteral("qsInner"));
    inner->setStyleSheet(QStringLiteral("QWidget#qsInner { background: transparent; }"));
    m_rowsLay = new QVBoxLayout(inner);
    m_rowsLay->setContentsMargins(0, 0, 0, 0);
    m_rowsLay->setSpacing(1);
    m_rowsLay->setAlignment(Qt::AlignTop);
    m_rows->setWidget(inner);
    // o viewport não herda o transparent (ver qt-viewport-transparent-bg-bug)
    m_rows->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    v->addWidget(m_rows);

    m_footer = new QLabel(this);
    m_footer->setObjectName(QStringLiteral("qsFooter"));
    m_footer->setFont(uiFont(10.5));
    m_footer->setTextFormat(Qt::RichText);
    m_footer->setContentsMargins(8, 6, 8, 1);
    m_footer->hide();
    v->addWidget(m_footer);

    m_doneRow = new QWidget(this);
    auto* dh = new QHBoxLayout(m_doneRow);
    dh->setContentsMargins(8, 3, 6, 3);
    dh->setSpacing(8);
    m_done = new QLabel(m_doneRow);
    m_done->setObjectName(QStringLiteral("qsDone"));
    m_done->setFont(uiFont(12));
    m_done->setWordWrap(true);
    dh->addWidget(m_done, 1);
    m_doneAction = new QToolButton(m_doneRow);
    m_doneAction->setObjectName(QStringLiteral("qsAction"));
    m_doneAction->setCursor(Qt::PointingHandCursor);
    m_doneAction->setFocusPolicy(Qt::NoFocus);
    connect(m_doneAction, &QToolButton::clicked, this, [this]() {
        auto act = m_action;
        hide();
        if (act) act();
    });
    dh->addWidget(m_doneAction, 0, Qt::AlignVCenter);
    m_doneRow->hide();
    v->addWidget(m_doneRow);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(18);
    shadow->setColor(QColor(0, 0, 0, 160));
    shadow->setOffset(0, 2);
    setGraphicsEffect(shadow);

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &QWidget::hide);

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, [this]() { applyTheme(); });
    applyTheme();
    hide();
}

void QuickSavePopup::applyTheme()
{
    const Palette pal = Palette::onPage();
    setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#quickSavePopup { background: %1; border: 1px solid %2; border-radius: @radius-panel; }"
        "QLabel#qsHeader, QLabel#qsFooter { color: %3; background: transparent; }"
        "QLabel#qsFooter { border-top: 1px solid %8; }"
        "QLabel#qsDone { color: %4; background: transparent; }"
        "QLineEdit#qsName { background: %9; border: 1px solid %10; border-radius: @radius-control;"
        "  color: %4; padding: 0 8px; selection-background-color: %11; }"
        "QToolButton#qsAction { background: transparent; border: none; color: %10; font-size: 11.5px; padding: 0 2px; }"
        "QToolButton#qsAction:hover { text-decoration: underline; }"
        "QPushButton#qsRow { background: transparent; border: none; border-radius: @radius-control;"
        "  color: %5; text-align: left; padding: 0 8px; font-size: 12.5px; }"
        "QPushButton#qsRow:hover { background: %6; color: %4; }"
        "QPushButton#qsRow:pressed { background: %7; }"
        "QLabel#qsHint { color: %3; background: transparent; }")
        .arg(Theme::panelBackground(), Theme::panelBorder(), pal.dim.name(), pal.bright.name(),
             pal.ink.name(), Theme::hoverOverlay(), Theme::pressedOverlay(),
             alpha(pal.ink, 0.12).name(QColor::HexArgb), mix(pal.app, pal.page, 0.55).name())
        .arg(pal.accent.name(), alpha(pal.accent, 0.35).name(QColor::HexArgb))));
}

void QuickSavePopup::clearRows()
{
    while (QLayoutItem* it = m_rowsLay->takeAt(0)) {
        if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
        delete it;
    }
}

void QuickSavePopup::refreshHeader()
{
    const QString shown = m_header->fontMetrics().elidedText(m_name, Qt::ElideRight, kWidth - 120);
    m_header->setText(m_headerFmt.arg(shown));
}

void QuickSavePopup::presentAt(const QPoint& globalAnchor, const QString& headerFmt, const QString& name,
                               const QVector<Target>& targets, const QString& footer)
{
    m_hideTimer->stop();
    setTakesFocus(false);
    clearRows();
    m_headerFmt = headerFmt;
    m_name = name;
    m_nameEdit->hide();
    m_doneRow->hide();
    m_header->show();
    m_rows->show();
    refreshHeader();
    m_footer->setText(footer);
    m_footer->setVisible(!footer.isEmpty());

    QWidget* inner = m_rows->widget();
    for (const Target& t : targets) {
        auto* b = new QPushButton(inner);
        b->setObjectName(QStringLiteral("qsRow"));
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setAutoDefault(false);
        b->setFixedHeight(kRowH);
        b->setText(t.title.isEmpty() ? tr("(sem nome)") : t.title);
        const QColor c(t.color.isEmpty() ? Palette::onPage().dim : QColor(t.color));
        QIcon icon;
        if (!t.iconId.isEmpty())
            icon = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/%1.svg").arg(t.iconId), c, c, c, QSize(15, 15));
        if (icon.isNull()) {
            // sem ícone: bolinha na cor
            QPixmap pm(QSize(15, 15) * 2);
            pm.setDevicePixelRatio(2.0);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawEllipse(QRectF(3.5, 3.5, 8, 8));
            icon = QIcon(pm);
        }
        b->setIcon(icon);
        b->setIconSize(QSize(15, 15));
        if (!t.hint.isEmpty()) {
            auto* h = new QHBoxLayout(b);
            h->setContentsMargins(0, 0, 8, 0);
            h->addStretch(1);
            auto* lbl = new QLabel(t.hint, b);
            lbl->setObjectName(QStringLiteral("qsHint"));
            lbl->setFont(uiFont(10.5));
            lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            h->addWidget(lbl);
        }
        const QString key = t.key;
        connect(b, &QPushButton::clicked, this, [this, key]() {
            if (m_nameEdit->isVisible()) finishRename(true);
            emit chosen(key);
        });
        m_rowsLay->addWidget(b);
    }
    m_rows->setFixedHeight(qMin(int(targets.size()), kMaxRows) * (kRowH + 1));

    adjustSize();
    move(globalAnchor);
    keepOnScreen();
    show();
    raise();
    qApp->installEventFilter(this);
}

void QuickSavePopup::flash(const QString& text, const QString& actionText, std::function<void()> action)
{
    if (m_nameEdit->isVisible()) finishRename(true);
    m_header->hide();
    m_rows->hide();
    m_footer->hide();
    m_done->setText(text);
    m_action = std::move(action);
    m_doneAction->setText(actionText);
    m_doneAction->setVisible(!actionText.isEmpty() && m_action);
    m_doneRow->show();
    adjustSize();
    // devolve o teclado pro texto (se o nome tinha pegado)
    setTakesFocus(false);
    m_hideTimer->start(actionText.isEmpty() ? 1300 : 2600);
}

void QuickSavePopup::setTakesFocus(bool on)
{
    if (on == m_tookFocus) return;
    m_tookFocus = on;
    const bool visible = isVisible();
    const QPoint at = pos();
    // trocar a flag recria a janela nativa (passa por hide/show): o hideEvent
    // dessa troca não é fechamento
    m_reflagging = true;
    setWindowFlag(Qt::WindowDoesNotAcceptFocus, !on);
    setAttribute(Qt::WA_ShowWithoutActivating, !on);
    if (visible) {
        move(at);
        show();
        raise();
        qApp->installEventFilter(this);
    }
    m_reflagging = false;
    if (on) {
        activateWindow();
    } else if (m_editor) {
        m_editor->window()->activateWindow();
        m_editor->setFocus();
    }
}

void QuickSavePopup::startRename()
{
    m_header->hide();
    m_nameEdit->setText(m_name);
    m_nameEdit->show();
    adjustSize();
    setTakesFocus(true);
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

void QuickSavePopup::finishRename(bool keep)
{
    if (keep && !m_nameEdit->text().trimmed().isEmpty()) m_name = m_nameEdit->text().trimmed();
    m_nameEdit->hide();
    refreshHeader();
    m_header->show();
    adjustSize();
}

void QuickSavePopup::keepOnScreen()
{
    QScreen* scr = QGuiApplication::screenAt(pos());
    if (!scr) scr = screen();
    if (!scr) return;
    const QRect avail = scr->availableGeometry();
    QRect g = frameGeometry();
    if (g.right() > avail.right() - 8) g.moveRight(avail.right() - 8);
    if (g.bottom() > avail.bottom() - 8) g.moveBottom(avail.bottom() - 8);
    move(g.topLeft());
}

bool QuickSavePopup::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_header && event->type() == QEvent::MouseButtonDblClick) {
        startRename();
        return true;
    }
    if (watched == m_nameEdit && event->type() == QEvent::KeyPress) {
        auto* k = static_cast<QKeyEvent*>(event);
        if (k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter) {
            finishRename(true);
            return true;
        }
        if (k->key() == Qt::Key_Escape) {
            finishRename(false);
            return true;
        }
        return false;
    }
    if (!isVisible()) {
        qApp->removeEventFilter(this);
        return false;
    }
    switch (event->type()) {
    case QEvent::KeyPress: {
        if (m_nameEdit->isVisible()) break;   // digitando o nome
        // o teclado continua no editor: Esc fecha; voltar a digitar também
        auto* k = static_cast<QKeyEvent*>(event);
        if (k->key() == Qt::Key_Escape) {
            hide();
            return true;
        }
        const bool modifierOnly = k->key() == Qt::Key_Shift || k->key() == Qt::Key_Control
                               || k->key() == Qt::Key_Alt || k->key() == Qt::Key_Meta;
        if (!modifierOnly && !m_doneRow->isVisible()) hide();
        break;
    }
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (!frameGeometry().contains(me->globalPosition().toPoint())) hide();
        break;
    }
    case QEvent::ApplicationDeactivate:
        hide();
        break;
    default:
        break;
    }
    return false;
}

void QuickSavePopup::hideEvent(QHideEvent* e)
{
    QFrame::hideEvent(e);
    if (m_reflagging) return;
    m_hideTimer->stop();
    qApp->removeEventFilter(this);
    // fechou com o nome em edição: volta a ser o popup sem foco e devolve o
    // teclado pro texto
    setTakesFocus(false);
}
