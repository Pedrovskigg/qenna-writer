#include "AmbiencePanel.h"

#include "AmbienceManager.h"
#include "Theme.h"

#include <QApplication>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QScreen>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kPanelWidth = 320;
constexpr int kListHeight = 260;
constexpr int kGapBelowAnchor = 6;
}

AmbiencePanel::AmbiencePanel(AmbienceManager* manager, QWidget* parent)
    : QFrame(parent)
    , m_manager(manager)
{
    setObjectName(QStringLiteral("ambiencePanel"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setFocusPolicy(Qt::ClickFocus);

    buildUi();
    applyTheme();

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);

    if (m_manager) {
        connect(m_manager, &AmbienceManager::tracksChanged,
                this, &AmbiencePanel::onTracksChanged);
        connect(m_manager, &AmbienceManager::activeTrackChanged,
                this, &AmbiencePanel::onActiveTrackChanged);
        connect(m_manager, &AmbienceManager::playbackStateChanged,
                this, &AmbiencePanel::onPlaybackStateChanged);
        connect(m_manager, &AmbienceManager::volumeChanged,
                this, &AmbiencePanel::onVolumeChanged);
    }

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &AmbiencePanel::applyTheme);

    rebuildList();
    if (m_manager) {
        onVolumeChanged(m_manager->volumePercent());
        refreshPlayIcon(m_manager->isPlaying());
    }

    hide();
    qApp->installEventFilter(this);
}

void AmbiencePanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 12);
    root->setSpacing(8);

    auto* header = new QLabel(tr("Som ambiente"), this);
    header->setObjectName(QStringLiteral("ambHeader"));
    root->addWidget(header);

    m_pathLabel = new QLabel(this);
    m_pathLabel->setObjectName(QStringLiteral("ambPath"));
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_pathLabel);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("ambList"));
    m_list->setFixedHeight(kListHeight);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setUniformItemSizes(true);
    connect(m_list, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* cur, QListWidgetItem*) {
        if (m_syncingList || !m_manager || !cur) return;
        const QString id = cur->data(Qt::UserRole).toString();
        m_manager->setActiveTrack(id);
        if (!m_manager->isPlaying()) m_manager->play();
    });
    root->addWidget(m_list);

    auto* controls = new QHBoxLayout();
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setSpacing(8);

    m_playBtn = new QToolButton(this);
    m_playBtn->setObjectName(QStringLiteral("ambPlayBtn"));
    m_playBtn->setText(QStringLiteral("▶"));
    m_playBtn->setToolTip(tr("Reproduzir"));
    m_playBtn->setCursor(Qt::PointingHandCursor);
    m_playBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_playBtn, &QToolButton::clicked, this, [this]() {
        if (m_manager) m_manager->togglePlay();
    });
    controls->addWidget(m_playBtn);

    m_volume = new QSlider(Qt::Horizontal, this);
    m_volume->setObjectName(QStringLiteral("ambVolume"));
    m_volume->setRange(0, 100);
    m_volume->setSingleStep(1);
    m_volume->setPageStep(10);
    connect(m_volume, &QSlider::valueChanged, this, [this](int v) {
        if (m_manager) m_manager->setVolumePercent(v);
    });
    controls->addWidget(m_volume, 1);

    m_volumeLabel = new QLabel(QStringLiteral("0%"), this);
    m_volumeLabel->setObjectName(QStringLiteral("ambVolumeLabel"));
    m_volumeLabel->setFixedWidth(36);
    m_volumeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    controls->addWidget(m_volumeLabel);

    root->addLayout(controls);

    setFixedWidth(kPanelWidth);
}

void AmbiencePanel::applyTheme()
{
    setStyleSheet(QStringLiteral(
        "QFrame#ambiencePanel {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
        "QLabel#ambHeader { color: %3; font-size: 13px; font-weight: 600; }"
        "QLabel#ambPath { color: %4; font-size: 10px; }"
        "QLabel#ambVolumeLabel { color: %3; font-size: 11px; }"
        "QListWidget#ambList {"
        "  background: %5;"
        "  color: %3;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "  padding: 4px;"
        "  outline: 0;"
        "}"
        "QListWidget#ambList::item {"
        "  padding: 6px 8px;"
        "  border-radius: 4px;"
        "}"
        "QListWidget#ambList::item:hover { background: %6; }"
        "QListWidget#ambList::item:selected { background: %7; color: %3; }"
        "QToolButton#ambPlayBtn {"
        "  background: transparent;"
        "  color: %3;"
        "  border: 1px solid %2;"
        "  border-radius: 16px;"
        "  min-width: 32px; max-width: 32px;"
        "  min-height: 32px; max-height: 32px;"
        "  font-size: 14px;"
        "}"
        "QToolButton#ambPlayBtn:hover { background: %6; }"
        "QSlider#ambVolume::groove:horizontal {"
        "  background: %2; height: 4px; border-radius: 2px;"
        "}"
        "QSlider#ambVolume::sub-page:horizontal { background: %8; border-radius: 2px; }"
        "QSlider#ambVolume::handle:horizontal {"
        "  background: %3; width: 12px; margin: -5px 0; border-radius: 6px;"
        "}"
    ).arg(Theme::panelBackground(),
          Theme::panelBorder(),
          Theme::textPrimary(),
          Theme::textMuted(),
          Theme::editorBackground(),
          Theme::hoverOverlay(),
          Theme::pressedOverlay(),
          Theme::accentDefault()));
}

void AmbiencePanel::rebuildList()
{
    if (!m_list || !m_manager) return;
    m_syncingList = true;
    m_list->clear();
    if (m_manager->tracksDirectoryExists()) {
        m_pathLabel->setText(m_manager->tracksDirectory());
    } else {
        m_pathLabel->setText(tr("Pasta de sons não encontrada: %1")
                             .arg(m_manager->tracksDirectory()));
    }
    for (const AmbienceManager::Track& t : m_manager->tracks()) {
        auto* item = new QListWidgetItem(t.name, m_list);
        item->setData(Qt::UserRole, t.id);
        item->setToolTip(t.filePath);
    }
    selectIdInList(m_manager->activeTrackId());
    m_syncingList = false;
}

void AmbiencePanel::selectIdInList(const QString& id)
{
    if (!m_list) return;
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toString() == id) {
            m_list->setCurrentRow(i);
            return;
        }
    }
}

void AmbiencePanel::onTracksChanged()
{
    rebuildList();
}

void AmbiencePanel::onActiveTrackChanged(const QString& id)
{
    if (m_syncingList) return;
    m_syncingList = true;
    selectIdInList(id);
    m_syncingList = false;
}

void AmbiencePanel::onPlaybackStateChanged(bool playing)
{
    refreshPlayIcon(playing);
}

void AmbiencePanel::onVolumeChanged(int percent)
{
    if (m_volume && m_volume->value() != percent) {
        const QSignalBlocker blocker(m_volume);
        m_volume->setValue(percent);
    }
    if (m_volumeLabel) m_volumeLabel->setText(QStringLiteral("%1%").arg(percent));
}

void AmbiencePanel::refreshPlayIcon(bool playing)
{
    if (!m_playBtn) return;
    m_playBtn->setText(playing ? QStringLiteral("❚❚") : QStringLiteral("▶"));
    m_playBtn->setToolTip(playing ? tr("Pausar") : tr("Reproduzir"));
}

void AmbiencePanel::showNear(const QRect& anchorGlobal)
{
    if (m_manager) m_manager->refresh();
    adjustSize();
    const QSize ps = size();
    QPoint pos(anchorGlobal.left(), anchorGlobal.bottom() + kGapBelowAnchor);
    const QScreen* screen = QGuiApplication::screenAt(anchorGlobal.center());
    if (screen) {
        const QRect avail = screen->availableGeometry();
        if (pos.x() + ps.width() > avail.right()) {
            pos.setX(anchorGlobal.right() - ps.width());
        }
        if (pos.x() < avail.left()) pos.setX(avail.left() + 4);
        if (pos.y() + ps.height() > avail.bottom()) {
            pos.setY(anchorGlobal.top() - ps.height() - kGapBelowAnchor);
        }
    }
    move(pos);
    show();
    raise();
    activateWindow();
}

bool AmbiencePanel::eventFilter(QObject* /*watched*/, QEvent* event)
{
    if (!isVisible()) return false;
    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        const QPoint gp = me->globalPosition().toPoint();
        if (frameGeometry().contains(gp)) return false;
        QWidget* clicked = QApplication::widgetAt(gp);
        QWidget* w = clicked;
        while (w) {
            if (w == this) return false;
            w = w->parentWidget();
        }
        hide();
    }
    return false;
}
