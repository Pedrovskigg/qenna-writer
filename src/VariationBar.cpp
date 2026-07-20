#include "VariationBar.h"
#include "EditorHost.h"
#include "ProjectModel.h"
#include "Theme.h"

#include <QApplication>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kGapBelowAnchor = 8;

QString chipQss(bool active, bool primary) {
    const QString base = active ? Theme::panelBackground() : QStringLiteral("transparent");
    const QString borderColor = active ? Theme::textBright() : Theme::panelBorder();
    const QString color = active ? Theme::textBright() : Theme::textPrimary();
    Q_UNUSED(primary);
    return QStringLiteral(R"(
        QToolButton {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 5px;
            padding: 3px 8px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 11px;
            text-align: left;
        }
        QToolButton:hover {
            background: %4;
            color: %5;
            border-color: %5;
        }
    )").arg(base, color, borderColor, Theme::hoverOverlay(), Theme::textBright());
}

QString iconBtnQss() {
    return QStringLiteral(R"(
        QToolButton {
            background: transparent;
            color: %1;
            border: 1px solid transparent;
            border-radius: 5px;
            padding: 3px 6px;
            font-size: 11px;
        }
        QToolButton:hover {
            background: %2;
            color: %3;
            border-color: %4;
        }
        QToolButton:disabled {
            color: %5;
        }
    )").arg(Theme::textMuted(), Theme::hoverOverlay(),
           Theme::textBright(), Theme::subtleBorder(),
           Theme::disabledText());
}
}

VariationBar::VariationBar(ProjectModel* model, EditorHost* host, QWidget* parent)
    : QFrame(parent)
    , m_model(model)
    , m_host(host)
    , m_chipsColumn(nullptr)
    , m_actionsRow(nullptr)
    , m_newBtn(nullptr)
    , m_primaryBtn(nullptr)
    , m_deleteBtn(nullptr)
{
    setObjectName(QStringLiteral("variationBar"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);
    applyRootStyle();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(6);

    m_chipsColumn = new QVBoxLayout();
    m_chipsColumn->setContentsMargins(0, 0, 0, 0);
    m_chipsColumn->setSpacing(3);
    root->addLayout(m_chipsColumn);

    auto* sep = new QFrame(this);
    sep->setObjectName(QStringLiteral("varBarSep"));
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Plain);
    root->addWidget(sep);

    m_actionsRow = new QHBoxLayout();
    m_actionsRow->setContentsMargins(0, 0, 0, 0);
    m_actionsRow->setSpacing(4);
    root->addLayout(m_actionsRow);

    m_newBtn = new QToolButton(this);
    m_newBtn->setText(tr("+ nova"));
    m_newBtn->setToolTip(tr("Criar variação a partir do conteúdo atual"));
    m_newBtn->setCursor(Qt::PointingHandCursor);
    m_newBtn->setStyleSheet(iconBtnQss());
    connect(m_newBtn, &QToolButton::clicked, this, [this]() {
        if (!m_host) return;
        bool ok = false;
        const QString label = QInputDialog::getText(this, tr("Nova variação"),
            tr("Nome da variação:"), QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok) return;
        m_host->createVariationForCurrentScene(label);
    });
    m_actionsRow->addWidget(m_newBtn);

    m_primaryBtn = new QToolButton(this);
    m_primaryBtn->setText(tr("★ primária"));
    m_primaryBtn->setToolTip(tr("Marcar variação atual como primária"));
    m_primaryBtn->setCursor(Qt::PointingHandCursor);
    m_primaryBtn->setStyleSheet(iconBtnQss());
    connect(m_primaryBtn, &QToolButton::clicked, this, [this]() {
        if (!m_host || !m_model) return;
        const auto vm = m_host->viewMode();
        const Scene* scene = m_model->findScene(vm.chapterId, vm.sceneIndex);
        if (!scene || scene->activeVariationId.isEmpty()) return;
        m_model->setPrimaryVariation(vm.chapterId, vm.sceneIndex, scene->activeVariationId);
    });
    m_actionsRow->addWidget(m_primaryBtn);

    m_deleteBtn = new QToolButton(this);
    m_deleteBtn->setText(tr("✕ apagar"));
    m_deleteBtn->setToolTip(tr("Apagar variação atual"));
    m_deleteBtn->setCursor(Qt::PointingHandCursor);
    m_deleteBtn->setStyleSheet(iconBtnQss());
    connect(m_deleteBtn, &QToolButton::clicked, this, [this]() {
        if (!m_host || !m_model) return;
        const auto vm = m_host->viewMode();
        const Scene* scene = m_model->findScene(vm.chapterId, vm.sceneIndex);
        if (!scene || scene->activeVariationId.isEmpty()) return;
        if (scene->variations.size() <= 1) return;
        const auto reply = QMessageBox::question(this, tr("Apagar variação"),
            tr("Apagar a variação atual? Esta ação não pode ser desfeita."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        m_host->deleteVariationForCurrentScene(scene->activeVariationId);
    });
    m_actionsRow->addWidget(m_deleteBtn);

    if (m_model) {
        connect(m_model, &ProjectModel::chaptersChanged, this, &VariationBar::refresh);
    }
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &VariationBar::applyTheme);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(18);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 2);
    setGraphicsEffect(shadow);

    hide();
    qApp->installEventFilter(this);
}

void VariationBar::applyRootStyle() {
    setStyleSheet(QStringLiteral(R"(
        #variationBar {
            background: %1;
            border: 1px solid %2;
            border-radius: 8px;
        }
        QFrame#varBarSep { color: %2; }
    )").arg(Theme::panelBackground(), Theme::panelBorder()));
}

void VariationBar::applyTheme() {
    applyRootStyle();
    if (m_newBtn) m_newBtn->setStyleSheet(iconBtnQss());
    if (m_primaryBtn) m_primaryBtn->setStyleSheet(iconBtnQss());
    if (m_deleteBtn) m_deleteBtn->setStyleSheet(iconBtnQss());
    if (isVisible()) rebuildButtons();
}

void VariationBar::toggleNear(const QRect& anchorGlobal)
{
    if (isVisible()) { hide(); return; }
    if (!m_host || !m_model) return;
    const auto vm = m_host->viewMode();
    if (vm.type != EditorHost::SceneDoc) return;
    const Scene* scene = m_model->findScene(vm.chapterId, vm.sceneIndex);
    if (!scene) return;

    rebuildButtons();
    adjustSize();
    const QSize ps = size();
    QPoint pos(anchorGlobal.left() + anchorGlobal.width() / 2 - ps.width() / 2,
               anchorGlobal.bottom() + kGapBelowAnchor);
    const QScreen* screen = QGuiApplication::screenAt(pos);
    if (screen) {
        const QRect avail = screen->availableGeometry();
        if (pos.y() + ps.height() > avail.bottom())
            pos.setY(anchorGlobal.top() - ps.height() - kGapBelowAnchor);
        if (pos.x() < avail.left()) pos.setX(avail.left() + 4);
        if (pos.x() + ps.width() > avail.right()) pos.setX(avail.right() - ps.width() - 4);
    }
    move(pos);
    show();
    raise();
}

void VariationBar::refresh() {
    if (!m_host || !m_model) { hide(); return; }
    const auto vm = m_host->viewMode();
    if (vm.type != EditorHost::SceneDoc) { hide(); return; }
    const Scene* scene = m_model->findScene(vm.chapterId, vm.sceneIndex);
    if (!scene) { hide(); return; }
    if (isVisible()) rebuildButtons();
}

void VariationBar::rebuildButtons() {
    // Limpa chips antigos.
    while (m_chipsColumn->count() > 0) {
        QLayoutItem* item = m_chipsColumn->takeAt(0);
        if (auto* w = item->widget()) w->deleteLater();
        delete item;
    }

    const auto vm = m_host->viewMode();
    const Scene* scene = m_model->findScene(vm.chapterId, vm.sceneIndex);
    if (!scene) return;

    const QString activeId = scene->activeVariationId;
    setEmptyState(scene->variations.isEmpty());

    for (const auto& v : scene->variations) {
        auto* btn = new QToolButton(this);
        const QString label = v.label.isEmpty() ? tr("(sem nome)") : v.label;
        btn->setText(v.isPrimary ? QStringLiteral("★ %1").arg(label) : label);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(chipQss(v.id == activeId, v.isPrimary));
        const QString vid = v.id;
        connect(btn, &QToolButton::clicked, this, [this, vid]() {
            if (m_host) m_host->switchVariationForCurrentScene(vid);
        });
        m_chipsColumn->addWidget(btn);
    }
    adjustSize();
}

void VariationBar::setEmptyState(bool empty) {
    m_primaryBtn->setEnabled(!empty);
    m_deleteBtn->setEnabled(!empty);
}

bool VariationBar::eventFilter(QObject* watched, QEvent* event)
{
    if (!isVisible()) return QFrame::eventFilter(watched, event);

    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) {
            hide();
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        const QPoint gp = me->globalPosition().toPoint();
        if (!frameGeometry().contains(gp)) hide();
    }
    return QFrame::eventFilter(watched, event);
}
