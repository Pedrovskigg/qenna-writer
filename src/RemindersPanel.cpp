#include "RemindersPanel.h"

#include "RemindersStore.h"
#include "Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QDate>
#include <QDateTime>
#include <QEvent>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QTime>
#include <QTimeEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kPanelWidth     = 360;
constexpr int kActiveListMaxH = 196;
constexpr int kHistListMaxH   = 140;
constexpr int kItemH          = 28;
constexpr int kGapBelowAnchor = 6;
}

RemindersPanel::RemindersPanel(RemindersStore* store, QWidget* parent)
    : QFrame(parent)
    , m_store(store)
{
    setObjectName(QStringLiteral("remindersPanel"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setFocusPolicy(Qt::ClickFocus);
    setFixedWidth(kPanelWidth);

    buildUi();
    applyTheme();

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(22);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);

    if (m_store)
        connect(m_store, &RemindersStore::changed, this, &RemindersPanel::onStoreChanged);

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &RemindersPanel::applyTheme);

    hide();
    qApp->installEventFilter(this);
}

void RemindersPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(7);

    // Header
    auto* header = new QLabel(tr("Lembretes"), this);
    header->setObjectName(QStringLiteral("remHeader"));
    root->addWidget(header);

    // Input row
    auto* inputRow = new QHBoxLayout();
    inputRow->setSpacing(6);
    m_input = new QLineEdit(this);
    m_input->setObjectName(QStringLiteral("remInput"));
    m_input->setPlaceholderText(tr("Novo lembrete..."));
    connect(m_input, &QLineEdit::returnPressed, this, &RemindersPanel::onAddClicked);
    inputRow->addWidget(m_input, 1);
    m_addBtn = new QPushButton(QStringLiteral("+"), this);
    m_addBtn->setObjectName(QStringLiteral("remAddBtn"));
    m_addBtn->setFixedSize(32, 32);
    m_addBtn->setCursor(Qt::PointingHandCursor);
    connect(m_addBtn, &QPushButton::clicked, this, &RemindersPanel::onAddClicked);
    inputRow->addWidget(m_addBtn);
    root->addLayout(inputRow);

    // Notify row
    auto* notifyRow = new QHBoxLayout();
    notifyRow->setSpacing(6);
    m_notifyCheck = new QCheckBox(tr("Notificar às"), this);
    m_notifyCheck->setObjectName(QStringLiteral("remNotifyCheck"));
    connect(m_notifyCheck, &QCheckBox::toggled, this, &RemindersPanel::onNotifyCheckToggled);
    notifyRow->addWidget(m_notifyCheck);
    m_timeEdit = new QTimeEdit(QTime::currentTime(), this);
    m_timeEdit->setObjectName(QStringLiteral("remTimeEdit"));
    m_timeEdit->setDisplayFormat(QStringLiteral("HH:mm"));
    m_timeEdit->setFixedWidth(68);
    m_timeEdit->setVisible(false);
    notifyRow->addWidget(m_timeEdit);
    notifyRow->addStretch(1);
    root->addLayout(notifyRow);

    // Separator
    auto* sep = new QFrame(this);
    sep->setObjectName(QStringLiteral("remSep"));
    sep->setFrameShape(QFrame::HLine);
    root->addWidget(sep);

    // Active list
    m_activeList = new QListWidget(this);
    m_activeList->setObjectName(QStringLiteral("remActiveList"));
    m_activeList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_activeList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_activeList->setMaximumHeight(kActiveListMaxH);
    m_activeList->setSelectionMode(QAbstractItemView::NoSelection);
    m_activeList->setFocusPolicy(Qt::NoFocus);
    connect(m_activeList, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (m_syncing) return;
        if (item && item->checkState() == Qt::Checked) {
            const QString id = item->data(Qt::UserRole).toString();
            QTimer::singleShot(0, this, [this, id]() { completeItem(id); });
        }
    });
    root->addWidget(m_activeList);

    // Empty placeholder
    m_emptyLabel = new QLabel(tr("Nenhum lembrete ativo"), this);
    m_emptyLabel->setObjectName(QStringLiteral("remEmpty"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setVisible(false);
    root->addWidget(m_emptyLabel);

    // History header row
    auto* histRow = new QHBoxLayout();
    histRow->setSpacing(4);
    m_historyToggle = new QToolButton(this);
    m_historyToggle->setObjectName(QStringLiteral("remHistToggle"));
    m_historyToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_historyToggle->setArrowType(Qt::RightArrow);
    m_historyToggle->setCheckable(false);
    m_historyToggle->setCursor(Qt::PointingHandCursor);
    connect(m_historyToggle, &QToolButton::clicked, this, &RemindersPanel::onHistoryToggleClicked);
    histRow->addWidget(m_historyToggle);
    histRow->addStretch(1);
    m_clearBtn = new QPushButton(tr("Limpar tudo"), this);
    m_clearBtn->setObjectName(QStringLiteral("remClearBtn"));
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setVisible(false);
    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        if (!m_store) return;
        m_store->clearCompleted();
        m_historyExpanded = false;
        updateHistoryHeader();
        adjustSize();
    });
    histRow->addWidget(m_clearBtn);
    root->addLayout(histRow);

    // History list
    m_historyList = new QListWidget(this);
    m_historyList->setObjectName(QStringLiteral("remHistList"));
    m_historyList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_historyList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_historyList->setMaximumHeight(kHistListMaxH);
    m_historyList->setSelectionMode(QAbstractItemView::NoSelection);
    m_historyList->setFocusPolicy(Qt::NoFocus);
    m_historyList->setToolTip(tr("Clique duplo para remover"));
    m_historyList->setVisible(false);
    connect(m_historyList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        const QString id = item->data(Qt::UserRole).toString();
        QTimer::singleShot(0, this, [this, id]() { removeItem(id); });
    });
    root->addWidget(m_historyList);
}

void RemindersPanel::rebuildActiveList()
{
    if (!m_activeList || !m_store) return;
    m_syncing = true;
    m_activeList->clear();

    const QVector<Reminder> actives = m_store->active();
    for (const Reminder& r : actives) {
        QString display = r.text;
        if (r.dueAt != 0) {
            const QString t = QDateTime::fromMSecsSinceEpoch(r.dueAt).toString(QStringLiteral("HH:mm"));
            display = QStringLiteral("%1  —  %2").arg(t, r.text);
        }
        auto* item = new QListWidgetItem(display, m_activeList);
        item->setData(Qt::UserRole, r.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setSizeHint(QSize(kPanelWidth - 28, kItemH));
    }

    const bool hasItems = m_activeList->count() > 0;
    m_activeList->setVisible(hasItems);
    m_emptyLabel->setVisible(!hasItems);

    if (hasItems) {
        const int h = qMin(m_activeList->count() * kItemH + 4, kActiveListMaxH);
        m_activeList->setFixedHeight(h);
    }
    m_syncing = false;
}

void RemindersPanel::rebuildHistoryList()
{
    if (!m_historyList || !m_store) return;
    m_historyList->clear();

    for (const Reminder& r : m_store->completed()) {
        auto* item = new QListWidgetItem(r.text, m_historyList);
        item->setData(Qt::UserRole, r.id);
        item->setSizeHint(QSize(kPanelWidth - 28, kItemH));
        QFont f = item->font();
        f.setStrikeOut(true);
        item->setFont(f);
    }

    const int n = m_historyList->count();
    if (n > 0) {
        const int h = qMin(n * kItemH + 4, kHistListMaxH);
        m_historyList->setFixedHeight(h);
    }
}

void RemindersPanel::updateHistoryHeader()
{
    if (!m_historyToggle || !m_store) return;
    const int n = int(m_store->completed().size());
    m_historyToggle->setText(tr("Concluídos (%1)").arg(n));
    m_historyToggle->setArrowType(m_historyExpanded ? Qt::DownArrow : Qt::RightArrow);
    const bool showHistory = m_historyExpanded && n > 0;
    if (m_clearBtn)    m_clearBtn->setVisible(showHistory);
    if (m_historyList) m_historyList->setVisible(showHistory);
}

void RemindersPanel::onStoreChanged()
{
    rebuildActiveList();
    if (m_historyExpanded) rebuildHistoryList();
    updateHistoryHeader();
    adjustSize();
}

void RemindersPanel::onAddClicked()
{
    if (!m_store || !m_input) return;
    const QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;

    qint64 dueAt = 0;
    bool   notify = false;
    if (m_notifyCheck && m_notifyCheck->isChecked() && m_timeEdit) {
        notify = true;
        QDateTime dt(QDate::currentDate(), m_timeEdit->time());
        if (dt <= QDateTime::currentDateTime()) dt = dt.addDays(1);
        dueAt = dt.toMSecsSinceEpoch();
    }

    m_store->add(text, dueAt, notify);
    m_input->clear();
    if (m_notifyCheck) m_notifyCheck->setChecked(false);
}

void RemindersPanel::onNotifyCheckToggled(bool checked)
{
    if (m_timeEdit) {
        m_timeEdit->setVisible(checked);
        if (checked) m_timeEdit->setTime(QTime::currentTime());
    }
    adjustSize();
}

void RemindersPanel::onHistoryToggleClicked()
{
    m_historyExpanded = !m_historyExpanded;
    if (m_historyExpanded) rebuildHistoryList();
    updateHistoryHeader();
    adjustSize();
}

void RemindersPanel::completeItem(const QString& id)
{
    if (m_store) m_store->complete(id);
}

void RemindersPanel::removeItem(const QString& id)
{
    if (m_store) m_store->remove(id);
}

void RemindersPanel::showNear(const QRect& anchorGlobal)
{
    if (m_store) {
        rebuildActiveList();
        if (m_historyExpanded) rebuildHistoryList();
        updateHistoryHeader();
    }
    adjustSize();

    QPoint pos(anchorGlobal.left(), anchorGlobal.bottom() + kGapBelowAnchor);
    const QScreen* screen = QGuiApplication::screenAt(anchorGlobal.center());
    if (screen) {
        const QRect avail = screen->availableGeometry();
        if (pos.x() + width() > avail.right())  pos.setX(anchorGlobal.right() - width());
        if (pos.x() < avail.left())              pos.setX(avail.left() + 4);
        if (pos.y() + height() > avail.bottom())
            pos.setY(anchorGlobal.top() - height() - kGapBelowAnchor);
    }
    move(pos);
    show();
    raise();
    activateWindow();
    if (m_input) m_input->setFocus();
}

bool RemindersPanel::eventFilter(QObject* /*watched*/, QEvent* event)
{
    if (!isVisible()) return false;
    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        const QPoint gp = me->globalPosition().toPoint();
        if (frameGeometry().contains(gp)) return false;
        QWidget* w = QApplication::widgetAt(gp);
        while (w) {
            if (w == this) return false;
            w = w->parentWidget();
        }
        hide();
    }
    return false;
}

void RemindersPanel::applyTheme()
{
    setStyleSheet(QStringLiteral(
        "QFrame#remindersPanel {"
        "  background: %1; border: 1px solid %2; border-radius: 10px;"
        "}"
        "QLabel#remHeader { color: %3; font-size: 14px; font-weight: 600; }"
        "QLabel#remEmpty  { color: %4; font-size: 12px; padding: 8px 0; }"
        "QLineEdit#remInput {"
        "  background: %5; color: %3; border: 1px solid %2;"
        "  border-radius: 6px; padding: 4px 8px; font-size: 12px;"
        "}"
        "QLineEdit#remInput:focus { border-color: %6; }"
        "QPushButton#remAddBtn {"
        "  background: %6; color: %7; border: none; border-radius: 6px;"
        "  font-size: 18px; font-weight: 300;"
        "}"
        "QPushButton#remAddBtn:hover  { background: %8; }"
        "QPushButton#remAddBtn:pressed { background: %6; }"
        "QCheckBox#remNotifyCheck { color: %4; font-size: 11px; }"
        "QTimeEdit#remTimeEdit {"
        "  background: %5; color: %3; border: 1px solid %2;"
        "  border-radius: 4px; padding: 2px 4px; font-size: 11px;"
        "}"
        "QFrame#remSep { color: %2; }"
        "QListWidget#remActiveList, QListWidget#remHistList {"
        "  background: transparent; border: none; outline: 0;"
        "  color: %3; font-size: 12px;"
        "}"
        "QListWidget#remActiveList::item, QListWidget#remHistList::item {"
        "  padding: 2px 4px; border-radius: 4px;"
        "}"
        "QListWidget#remActiveList::item:hover { background: %9; }"
        "QListWidget#remHistList { color: %4; }"
        "QListWidget#remHistList::item:hover { background: %9; }"
        "QToolButton#remHistToggle {"
        "  background: transparent; border: none;"
        "  color: %4; font-size: 11px; padding: 2px 0;"
        "}"
        "QToolButton#remHistToggle:hover { color: %3; }"
        "QPushButton#remClearBtn {"
        "  background: transparent; border: none;"
        "  color: %10; font-size: 11px; padding: 2px 4px;"
        "}"
        "QPushButton#remClearBtn:hover { color: %11; }"
    ).arg(Theme::panelBackground(),    // 1
         Theme::panelBorder(),         // 2
         Theme::textPrimary(),         // 3
         Theme::textMuted(),           // 4
         Theme::editorBackground(),    // 5
         Theme::accentDefault(),       // 6
         Theme::textBright(),          // 7
         Theme::hoverStrong(),         // 8
         Theme::hoverOverlay(),        // 9
         Theme::accentDanger(),        // 10
         Theme::accentDanger()));      // 11
}
