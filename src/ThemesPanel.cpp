#include "ThemesPanel.h"

#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

// Gera um pixmap pequeno com 4 swatches lado a lado representando o tema.
QPixmap makeSwatchPixmap(const Theme::MiraTheme& t, int w = 56, int h = 28)
{
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, false);
    const int cellW = w / 4;
    const QStringList colors = {
        t.appBackground, t.panelBackground, t.editorBackground, t.accentDefault,
    };
    for (int i = 0; i < 4; ++i) {
        const QRect r(i * cellW, 0, cellW, h);
        p.fillRect(r, QColor(colors.at(i)));
    }
    p.setPen(QColor(t.panelBorder));
    p.drawRect(0, 0, w - 1, h - 1);
    return pm;
}

} // namespace

ThemesPanel::ThemesPanel(QWidget* parent)
    : QDialog(parent)
    , m_list(new QListWidget(this))
    , m_previewName(new QLabel(this))
    , m_previewSwatch(new QLabel(this))
    , m_previewHint(new QLabel(this))
    , m_applyButton(new QPushButton(tr("Aplicar"), this))
{
    setObjectName(QStringLiteral("themesPanel"));
    setWindowTitle(tr("Temas"));
    setModal(false);
    resize(520, 360);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(16);

    // ---- Coluna esquerda: lista de temas ----
    m_list->setObjectName(QStringLiteral("themesPanelList"));
    m_list->setFixedWidth(200);
    m_list->setIconSize(QSize(64, 32));
    m_list->setSpacing(4);
    root->addWidget(m_list);

    // ---- Coluna direita: preview + ação ----
    auto* rightCol = new QVBoxLayout;
    rightCol->setSpacing(12);

    m_previewName->setObjectName(QStringLiteral("themesPanelPreviewName"));
    m_previewName->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_previewSwatch->setObjectName(QStringLiteral("themesPanelPreviewSwatch"));
    m_previewSwatch->setFixedHeight(64);
    m_previewSwatch->setAlignment(Qt::AlignCenter);

    m_previewHint->setObjectName(QStringLiteral("themesPanelPreviewHint"));
    m_previewHint->setWordWrap(true);
    m_previewHint->setText(tr(
        "Trocar de tema reaplica cores de painéis, fundo do editor e largura "
        "da página. As mudanças são imediatas e persistem entre sessões."));

    rightCol->addWidget(m_previewName);
    rightCol->addWidget(m_previewSwatch);
    rightCol->addWidget(m_previewHint);
    rightCol->addStretch();

    auto* actions = new QHBoxLayout;
    actions->addStretch();
    auto* closeBtn = new QPushButton(tr("Fechar"), this);
    closeBtn->setObjectName(QStringLiteral("themesPanelCloseBtn"));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    m_applyButton->setObjectName(QStringLiteral("themesPanelApplyBtn"));
    m_applyButton->setDefault(true);
    actions->addWidget(closeBtn);
    actions->addWidget(m_applyButton);
    rightCol->addLayout(actions);

    root->addLayout(rightCol, /*stretch=*/1);

    connect(m_list, &QListWidget::currentItemChanged,
            this, &ThemesPanel::onSelectionChanged);
    connect(m_applyButton, &QPushButton::clicked,
            this, &ThemesPanel::onApplyClicked);
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &ThemesPanel::onThemeChanged);

    rebuildList();
    applyDialogStyle();
}

void ThemesPanel::rebuildList()
{
    m_list->clear();
    const auto& themes = Theme::Manager::instance()->available();
    const QString currentId = Theme::Manager::instance()->current().id;
    for (const auto& t : themes) {
        auto* item = new QListWidgetItem(m_list);
        item->setText(t.name);
        item->setData(Qt::UserRole, t.id);
        item->setIcon(QIcon(makeSwatchPixmap(t)));
        if (t.id == currentId) {
            m_list->setCurrentItem(item);
            m_selectedId = t.id;
        }
    }
    if (!m_list->currentItem() && m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
    rebuildPreview();
}

void ThemesPanel::onSelectionChanged()
{
    auto* item = m_list->currentItem();
    if (!item) return;
    m_selectedId = item->data(Qt::UserRole).toString();
    rebuildPreview();
}

void ThemesPanel::rebuildPreview()
{
    const auto& themes = Theme::Manager::instance()->available();
    const Theme::MiraTheme* theme = nullptr;
    for (const auto& t : themes) {
        if (t.id == m_selectedId) { theme = &t; break; }
    }
    if (!theme) return;

    const QString currentId = Theme::Manager::instance()->current().id;
    const bool isCurrent = theme->id == currentId;

    m_previewName->setText(theme->name + (isCurrent ? tr("  (em uso)") : QString()));
    m_previewSwatch->setPixmap(makeSwatchPixmap(*theme, 360, 56));
    m_applyButton->setEnabled(!isCurrent);
}

void ThemesPanel::onApplyClicked()
{
    if (m_selectedId.isEmpty()) return;
    Theme::Manager::instance()->setCurrent(m_selectedId);
    // O sinal themeChanged já dispara rebuildPreview via onThemeChanged.
}

void ThemesPanel::onThemeChanged()
{
    rebuildList();
    applyDialogStyle();
}

void ThemesPanel::applyDialogStyle()
{
    setStyleSheet(QStringLiteral(R"(
        #themesPanel {
            background: %1;
        }
        #themesPanel QLabel {
            color: %2;
            font-size: 12px;
        }
        #themesPanelPreviewName {
            color: %3;
            font-size: 18px;
            font-weight: 600;
        }
        #themesPanelPreviewHint {
            color: %4;
            font-size: 11px;
        }
        #themesPanelList {
            background: %5;
            color: %2;
            border: 1px solid %6;
            border-radius: 8px;
            padding: 6px;
            outline: none;
        }
        #themesPanelList::item {
            padding: 8px 10px;
            border-radius: 6px;
            margin: 2px 0;
        }
        #themesPanelList::item:selected {
            background: %7;
            color: %3;
        }
        #themesPanelList::item:hover {
            background: %8;
        }
        QPushButton {
            background: %5;
            color: %2;
            border: 1px solid %6;
            padding: 6px 18px;
            border-radius: 6px;
            font-size: 12px;
        }
        QPushButton:hover {
            background: %7;
            color: %3;
        }
        QPushButton:disabled {
            color: %4;
            background: transparent;
            border-color: %8;
        }
        QPushButton:default {
            border-color: %9;
        }
    )").arg(
        Theme::appBackground(),     // 1
        Theme::textPrimary(),       // 2
        Theme::textBright(),        // 3
        Theme::textMuted(),         // 4
        Theme::panelBackground(),   // 5
        Theme::panelBorder(),       // 6
        Theme::hoverOverlay(),      // 7
        Theme::subtleBorder(),      // 8
        Theme::accentDefault()      // 9
    ));
}
