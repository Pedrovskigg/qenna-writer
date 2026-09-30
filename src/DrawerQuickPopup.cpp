#include "DrawerQuickPopup.h"

#include "DrawerCreateDialog.h"
#include "ElementsStore.h"
#include "IconUtils.h"
#include "QuickCoverWidgets.h"
#include "SheetDialog.h"
#include "Theme.h"
#include "TimelineTracksTypes.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Tracks;

namespace {
constexpr int kWidth = 212;
constexpr int kBadge = 56;
constexpr int kShownIcons = 9;   // à vista; o resto atrás do "+N"
QString css(const QColor& c) { return c.name(QColor::HexArgb); }
}

// "Coluna": estreito e em pé, no formato da LeftBar. Ícone grande em cima,
// nome embaixo, os ícones mais usados à vista, cor e tipo no pé.
DrawerQuickPopup::DrawerQuickPopup(ElementsStore* store, QWidget* window)
    : QFrame(window), m_store(store), m_window(window)
{
    setObjectName(QStringLiteral("drawerQuick"));
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(14, 14, 14, 10);
    v->setSpacing(10);
    // o cartão acompanha o conteúdo (grade e tipo abrem e fecham); a largura
    // vem das peças de largura fixa lá dentro
    v->setSizeConstraint(QLayout::SetFixedSize);

    m_badge = new QToolButton(this);
    m_badge->setFixedSize(kBadge, kBadge);
    m_badge->setFocusPolicy(Qt::NoFocus);
    m_badge->setAutoRaise(true);
    m_badge->setAttribute(Qt::WA_TransparentForMouseEvents);
    v->addWidget(m_badge, 0, Qt::AlignHCenter);

    m_name = new QLineEdit(this);
    m_name->setObjectName(QStringLiteral("dqName"));
    m_name->setFont(serifFont(17));
    m_name->setFixedSize(kWidth - 28, 32);
    m_name->setAlignment(Qt::AlignCenter);
    m_name->setPlaceholderText(tr("Nome da gaveta"));
    connect(m_name, &QLineEdit::textChanged, this, [this](const QString& t) {
        if (!m_iconManual) setIcon(DrawerCreateDialog::autoIconFromTitle(t), false);
    });
    connect(m_name, &QLineEdit::returnPressed, this, &DrawerQuickPopup::confirm);
    v->addWidget(m_name);

    // os ícones: os mais usados à vista, o resto no "+N"
    m_iconGrid = new QWidget(this);
    m_iconGrid->setFixedWidth(kWidth - 28);
    m_iconLayout = new QGridLayout(m_iconGrid);
    m_iconLayout->setContentsMargins(0, 0, 0, 0);
    m_iconLayout->setSpacing(3);
    const QStringList ids = DrawerCreateDialog::drawerIconCatalog();
    for (const QString& id : ids) {
        auto* b = new QToolButton(m_iconGrid);
        b->setObjectName(QStringLiteral("dqIcon"));
        b->setCheckable(true);
        b->setFixedSize(34, 30);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setToolTip(DrawerCreateDialog::iconLabel(id));
        b->setProperty("iconId", id);
        b->setIconSize(QSize(17, 17));
        connect(b, &QToolButton::clicked, this, [this, id]() {
            setIcon(id, true);
            m_name->setFocus();
        });
        m_iconBtns << b;
    }
    m_moreBtn = new QToolButton(m_iconGrid);
    m_moreBtn->setObjectName(QStringLiteral("dqMore"));
    m_moreBtn->setFixedSize(34, 30);
    m_moreBtn->setCursor(Qt::PointingHandCursor);
    m_moreBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_moreBtn, &QToolButton::clicked, this, [this]() {
        m_allIcons = !m_allIcons;
        layoutIcons();
        m_name->setFocus();
    });
    v->addWidget(m_iconGrid);

    // cor e tipo, numa linha
    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    m_wheel = new ColorWheelButton(this);
    m_wheel->setPickerTitle(tr("Cor da gaveta"));
    m_wheel->setToolTip(tr("Cor da gaveta"));
    connect(m_wheel, &ColorWheelButton::colorPicked, this, [this]() {
        refreshBadge();
        m_name->setFocus();
    });
    row->addWidget(m_wheel);
    row->addStretch(1);
    m_typeLink = new QToolButton(this);
    m_typeLink->setObjectName(QStringLiteral("dqLink"));
    m_typeLink->setCursor(Qt::PointingHandCursor);
    m_typeLink->setFocusPolicy(Qt::NoFocus);
    m_typeLink->setToolTip(tr("Automático deixa o Qenna decidir pelo nome da gaveta."));
    connect(m_typeLink, &QToolButton::clicked, this, [this]() {
        m_typeBox->setVisible(!m_typeBox->isVisible());
        adjustSize();
    });
    row->addWidget(m_typeLink);
    v->addLayout(row);

    // o tipo de elemento, só quando pedem
    m_typeBox = new QWidget(this);
    m_typeBox->setFixedWidth(kWidth - 28);
    auto* tv = new QVBoxLayout(m_typeBox);
    tv->setContentsMargins(0, 0, 0, 0);
    QStringList labels = { tr("Automático") };
    m_typeIds = QStringList{ QString() };
    if (m_store)
        for (const auto& t : m_store->elementTypes()) { labels << t.label; m_typeIds << t.id; }
    m_type = new SheetChoice(labels, m_typeBox, Qt::AlignLeft, kWidth - 28);
    m_type->setCurrentIndex(0);
    connect(m_type, &SheetChoice::activated, this, [this]() { refreshTypeLink(); m_name->setFocus(); });
    tv->addWidget(m_type);
    m_typeBox->hide();
    v->addWidget(m_typeBox);

    // pé: a dica do Enter, centralizada sob um fio
    auto* rule = new QFrame(this);
    rule->setObjectName(QStringLiteral("dqRule"));
    rule->setFixedHeight(1);
    v->addWidget(rule);
    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("dqDim"));
    m_hint->setFont(uiFont(10.5));
    m_hint->setAlignment(Qt::AlignCenter);
    v->addWidget(m_hint);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(0, 0, 0, 120));
    setGraphicsEffect(shadow);

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, [this]() { applyTheme(); });
    applyTheme();
    hide();
}

void DrawerQuickPopup::applyTheme()
{
    const Palette pal = Palette::onPage();
    for (QToolButton* b : std::as_const(m_iconBtns)) {
        const QString id = b->property("iconId").toString();
        b->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/%1.svg").arg(id),
                                              pal.ink, pal.ink, pal.ink, QSize(16, 16)));
    }
    setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#drawerQuick { background: %1; border: 1px solid %2; border-radius: 10px; }"
        "QLineEdit#dqName { background: transparent; border: none; border-bottom: 1px solid %3; color: %4;"
        "  padding: 0 2px; selection-background-color: %5; }"
        "QLineEdit#dqName:focus { border-bottom-color: %6; }"
        "QLabel#dqDim { color: %7; background: transparent; }"
        "QToolButton#dqLink { background: transparent; border: none; color: %7; font-size: 11px; padding: 0; }"
        "QToolButton#dqLink:hover { color: %4; }"
        "QToolButton#dqIcon { background: transparent; border: 1px solid transparent; border-radius: 6px; }"
        "QToolButton#dqIcon:hover { background: %8; }"
        "QToolButton#dqIcon:checked { border-color: %6; background: %9; }"
        "QToolButton#dqMore { background: transparent; border: none; border-radius: 6px; color: %7; font-size: 11px; }"
        "QToolButton#dqMore:hover { background: %8; color: %4; }"
        "QFrame#dqRule { background: %2; border: none; }"
        // pílulas / lista do tipo (SheetChoice), no desenho das folhas
        "QToolButton#sheetType { background: transparent; border: 1px solid %2; border-radius: 13px; color: %7; padding: 0 9px; }"
        "QToolButton#sheetType:hover { color: %4; border-color: %3; }"
        "QToolButton#sheetType:checked { color: %4; border-color: %6; background: %9; }"
        "QPushButton#sheetPick { background: %10; border: 1px solid %2; border-radius: 6px; text-align: left; }"
        "QLabel#sheetPickText { background: transparent; color: %4; }"
        "QFrame#sheetChoiceList { background: %10; border: 1px solid %2; border-radius: 8px; }"
        "QPushButton#sheetChoiceCell { background: transparent; border: 1px solid transparent; border-radius: 6px;"
        "  color: %11; text-align: left; padding: 0 8px; font-size: 12.5px; }"
        "QPushButton#sheetChoiceCell:hover { background: %8; color: %4; }"
        "QPushButton#sheetChoiceCell[sel=\"true\"] { background: %9; border-color: %6; color: %4; }")
        .arg(pal.page.name(), pal.border.name(), css(alpha(pal.ink, 0.30)), pal.bright.name(),
             css(alpha(pal.accent, 0.35)), pal.accent.name(), pal.dim.name(), css(alpha(pal.ink, 0.08)),
             css(alpha(pal.accent, 0.16)))
        .arg(mix(pal.app, pal.page, 0.55).name(), pal.ink.name())));
    refreshBadge();
}

void DrawerQuickPopup::setIcon(const QString& id, bool manual)
{
    m_icon = id;
    if (manual) m_iconManual = true;
    for (QToolButton* b : std::as_const(m_iconBtns)) b->setChecked(b->property("iconId").toString() == id);
    layoutIcons();
    refreshBadge();
}

void DrawerQuickPopup::refreshBadge()
{
    const QColor col = m_wheel ? m_wheel->color() : QColor(QStringLiteral("#2b79ff"));
    const qreal dpr = devicePixelRatioF();
    QPixmap pm(QSize(kBadge, kBadge) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QColor bg = col; bg.setAlphaF(0.18f);
    QColor bd = col; bd.setAlphaF(0.55f);
    p.setPen(QPen(bd, 1));
    p.setBrush(bg);
    p.drawRoundedRect(QRectF(0.5, 0.5, kBadge - 1, kBadge - 1), 12, 12);
    IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/%1.svg").arg(m_icon), col, col, col, QSize(26, 26))
        .paint(&p, QRect((kBadge - 26) / 2, (kBadge - 26) / 2, 26, 26));
    p.end();
    m_badge->setIcon(QIcon(pm));
    m_badge->setIconSize(QSize(kBadge, kBadge));
}

void DrawerQuickPopup::layoutIcons()
{
    if (!m_iconLayout) return;
    while (m_iconLayout->count()) m_iconLayout->takeAt(0);
    // à vista: os primeiros do catálogo; o escolhido entra no lugar do último
    // se não estiver entre eles
    QList<QToolButton*> shown;
    if (m_allIcons) {
        shown = m_iconBtns;
    } else {
        shown = m_iconBtns.mid(0, kShownIcons);
        QToolButton* cur = nullptr;
        for (QToolButton* b : std::as_const(m_iconBtns)) if (b->isChecked()) cur = b;
        if (cur && !shown.contains(cur) && !shown.isEmpty()) shown.last() = cur;
    }
    for (QToolButton* b : std::as_const(m_iconBtns)) b->setVisible(shown.contains(b));
    int n = 0;
    for (QToolButton* b : std::as_const(shown)) { m_iconLayout->addWidget(b, n / 5, n % 5); ++n; }
    m_moreBtn->setText(m_allIcons ? tr("menos") : QStringLiteral("+%1").arg(m_iconBtns.size() - shown.size()));
    m_moreBtn->setToolTip(m_allIcons ? tr("Mostrar só os mais usados") : tr("Todos os ícones"));
    m_iconLayout->addWidget(m_moreBtn, n / 5, n % 5);
    adjustSize();
}

void DrawerQuickPopup::refreshTypeLink()
{
    const int i = m_type->currentIndex();
    QString name = tr("Automático");
    if (i > 0 && m_store)
        for (const auto& t : m_store->elementTypes()) if (t.id == m_typeIds.value(i)) name = t.label;
    m_typeLink->setText(tr("tipo: %1 ▾").arg(i == 0 ? tr("Auto") : name));
}

void DrawerQuickPopup::openCreate(const QRect& anchorGlobal, const QString& color)
{
    m_edit = false;
    m_iconManual = false;
    m_name->clear();
    m_wheel->setColor(QColor(color));
    m_type->setCurrentIndex(0);
    setIcon(QStringLiteral("drawer"), false);
    m_hint->setText(tr("Enter cria"));
    open(anchorGlobal);
}

void DrawerQuickPopup::openEdit(const QRect& anchorGlobal, const QString& title, const QString& icon,
                                const QString& color, const QString& elementTypeId)
{
    m_edit = true;
    m_iconManual = true;   // já tem ícone: renomear não troca
    m_name->setText(title);
    m_wheel->setColor(QColor(color));
    m_type->setCurrentIndex(qMax(0, int(m_typeIds.indexOf(elementTypeId))));
    setIcon(icon.isEmpty() ? QStringLiteral("drawer") : icon, true);
    m_hint->setText(tr("Enter salva"));
    open(anchorGlobal);
}

void DrawerQuickPopup::open(const QRect& anchorGlobal)
{
    m_prevFocus = QApplication::focusWidget();
    m_allIcons = false;
    m_typeBox->hide();
    layoutIcons();
    refreshTypeLink();
    adjustSize();
    // ao lado do que abriu, centrado na altura dele; sempre dentro da janela
    QPoint at = m_window->mapFromGlobal(QPoint(anchorGlobal.right() + 12, anchorGlobal.center().y() - height() / 2));
    at.setX(qBound(8, at.x(), m_window->width() - width() - 8));
    at.setY(qBound(8, at.y(), m_window->height() - height() - 8));
    move(at);
    show();
    raise();
    m_name->setFocus();
    m_name->selectAll();
    qApp->installEventFilter(this);
}

void DrawerQuickPopup::dismiss()
{
    qApp->removeEventFilter(this);
    hide();
    if (m_prevFocus) m_prevFocus->setFocus();
}

void DrawerQuickPopup::confirm()
{
    const QString title = m_name->text().trimmed();
    if (title.isEmpty()) return;
    const QString type = m_typeIds.value(m_type->currentIndex());
    const QString color = m_wheel->color().name(QColor::HexRgb);
    const QString icon = m_icon;
    dismiss();
    emit confirmed(title, icon, color, type);
}

void DrawerQuickPopup::resizeEvent(QResizeEvent* e)
{
    QFrame::resizeEvent(e);
    // cresceu (grade ou tipo abertos) perto do pé da janela: sobe o que precisar
    const int maxY = m_window->height() - height() - 8;
    if (y() > maxY) move(x(), qMax(8, maxY));
}

void DrawerQuickPopup::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) { dismiss(); return; }
    QFrame::keyPressEvent(e);
}

bool DrawerQuickPopup::eventFilter(QObject* o, QEvent* e)
{
    if (!isVisible()) return false;
    if (e->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape) {
        auto* w = qobject_cast<QWidget*>(o);
        if (w && isAncestorOf(w)) { dismiss(); return true; }
    }
    // clique fora (na própria janela) fecha; o seletor de cor é outra janela
    // e não conta
    if (e->type() == QEvent::MouseButtonPress) {
        auto* w = qobject_cast<QWidget*>(o);
        if (w && w->window() == m_window && w != this && !isAncestorOf(w)) {
            const QPoint gp = static_cast<QMouseEvent*>(e)->globalPosition().toPoint();
            if (!rect().contains(mapFromGlobal(gp))) dismiss();
        }
    }
    return false;
}
