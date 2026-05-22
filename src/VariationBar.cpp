#include "VariationBar.h"
#include "EditorHost.h"
#include "ProjectModel.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QToolButton>
#include <QLineEdit>

namespace {
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
            border-radius: 6px;
            padding: 3px 10px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 12px;
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
            border-radius: 6px;
            padding: 3px 8px;
            font-size: 13px;
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
           QStringLiteral("#3a3a42"));
}
}

VariationBar::VariationBar(ProjectModel* model, EditorHost* host, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_host(host)
    , m_buttonsLayout(nullptr)
    , m_label(nullptr)
    , m_newBtn(nullptr)
    , m_primaryBtn(nullptr)
    , m_deleteBtn(nullptr)
{
    setObjectName(QStringLiteral("variationBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    applyRootStyle();

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(10, 6, 10, 6);
    root->setSpacing(8);

    m_label = new QLabel(tr("Variações"), this);
    m_label->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Lora','Crimson Text',serif; font-size: 12px; font-weight: 600;")
        .arg(Theme::textMuted()));
    root->addWidget(m_label);

    m_buttonsLayout = new QHBoxLayout();
    m_buttonsLayout->setContentsMargins(0, 0, 0, 0);
    m_buttonsLayout->setSpacing(4);
    root->addLayout(m_buttonsLayout, /*stretch=*/1);

    m_newBtn = new QToolButton(this);
    m_newBtn->setText(QStringLiteral("+ nova"));
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
    root->addWidget(m_newBtn);

    m_primaryBtn = new QToolButton(this);
    m_primaryBtn->setText(QStringLiteral("★ primária"));
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
    root->addWidget(m_primaryBtn);

    m_deleteBtn = new QToolButton(this);
    m_deleteBtn->setText(QStringLiteral("✕ apagar"));
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
    root->addWidget(m_deleteBtn);

    if (m_model) {
        connect(m_model, &ProjectModel::chaptersChanged, this, &VariationBar::refresh);
    }
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &VariationBar::applyTheme);
    hide();
}

void VariationBar::applyRootStyle() {
    setStyleSheet(QStringLiteral(R"(
        #variationBar {
            background: %1;
            border: 1px solid %2;
            border-radius: 8px;
        }
    )").arg(Theme::panelBackground(), Theme::panelBorder()));
}

void VariationBar::applyTheme() {
    applyRootStyle();
    if (m_label) {
        m_label->setStyleSheet(QStringLiteral(
            "color: %1; font-family: 'Lora','Crimson Text',serif; font-size: 12px; font-weight: 600;")
            .arg(Theme::textMuted()));
    }
    if (m_newBtn) m_newBtn->setStyleSheet(iconBtnQss());
    if (m_primaryBtn) m_primaryBtn->setStyleSheet(iconBtnQss());
    if (m_deleteBtn) m_deleteBtn->setStyleSheet(iconBtnQss());
    if (isVisible()) rebuildButtons();
}

void VariationBar::refresh() {
    if (!m_host || !m_model) { hide(); return; }
    const auto vm = m_host->viewMode();
    if (vm.type != EditorHost::SceneDoc) { hide(); return; }
    const Scene* scene = m_model->findScene(vm.chapterId, vm.sceneIndex);
    if (!scene) { hide(); return; }

    show();
    rebuildButtons();
}

void VariationBar::rebuildButtons() {
    // Limpa botões antigos.
    while (m_buttonsLayout->count() > 0) {
        QLayoutItem* item = m_buttonsLayout->takeAt(0);
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
        m_buttonsLayout->addWidget(btn);
    }
    m_buttonsLayout->addStretch();
}

void VariationBar::setEmptyState(bool empty) {
    m_primaryBtn->setEnabled(!empty);
    m_deleteBtn->setEnabled(!empty);
}
