#include "ThemesPanel.h"

#include "ThemeEditorDialog.h"
#include "ThemePreviewWidget.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTimeEdit>
#include <QVBoxLayout>

namespace {

constexpr int kCardW = 230;
constexpr int kCardH = 170;
constexpr int kCardSpacing = 14;

// Card visual de um tema: preview + nome + (em uso) ring.
class ThemeCard : public QFrame {
public:
    ThemeCard(const Theme::MiraTheme& theme, bool inUse, bool dayRole, bool nightRole,
              bool favorite, QWidget* parent = nullptr)
        : QFrame(parent), m_id(theme.id), m_inUse(inUse), m_selected(false), m_favorite(favorite)
    {
        setObjectName(QStringLiteral("themeCard"));
        setFrameShape(QFrame::NoFrame);
        setFixedSize(kCardW, kCardH);
        setCursor(Qt::PointingHandCursor);

        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(8, 8, 8, 8);
        lay->setSpacing(6);

        m_preview = new ThemePreviewWidget(this);
        m_preview->setTheme(theme);
        m_preview->setMinimumHeight(110);
        m_preview->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        lay->addWidget(m_preview, 1);

        // Coração de favorito — fora do layout, posicionado à mão no canto
        // superior direito do card (sobre o preview) pra não competir com o
        // nameRow, que já tem nome + badges de papel/uso.
        m_favoriteBtn = new QLabel(this);
        m_favoriteBtn->setObjectName(QStringLiteral("themeCardFavorite"));
        m_favoriteBtn->setCursor(Qt::PointingHandCursor);
        m_favoriteBtn->setAlignment(Qt::AlignCenter);
        m_favoriteBtn->setFixedSize(24, 24);
        m_favoriteBtn->move(kCardW - 8 - 24, 8);
        m_favoriteBtn->setToolTip(QCoreApplication::translate("ThemeCard", "Favoritar"));
        m_favoriteBtn->setAttribute(Qt::WA_Hover, true);
        m_favoriteBtn->raise();

        auto* nameRow = new QHBoxLayout;
        nameRow->setContentsMargins(0, 0, 0, 0);
        nameRow->setSpacing(6);
        m_nameLabel = new QLabel(theme.name, this);
        m_nameLabel->setObjectName(QStringLiteral("themeCardName"));
        m_nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        nameRow->addWidget(m_nameLabel, 1);
        if (dayRole) {
            auto* badge = new QLabel(QStringLiteral("☀"), this);
            badge->setObjectName(QStringLiteral("themeCardRoleBadge"));
            badge->setToolTip(QCoreApplication::translate("ThemeCard", "Tema diurno"));
            badge->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            nameRow->addWidget(badge);
        }
        if (nightRole) {
            auto* badge = new QLabel(QStringLiteral("☽"), this);
            badge->setObjectName(QStringLiteral("themeCardRoleBadge"));
            badge->setToolTip(QCoreApplication::translate("ThemeCard", "Tema noturno"));
            badge->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            nameRow->addWidget(badge);
        }
        if (inUse) {
            auto* badge = new QLabel(QCoreApplication::translate("ThemeCard", "em uso"), this);
            badge->setObjectName(QStringLiteral("themeCardBadge"));
            badge->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            nameRow->addWidget(badge);
        }
        lay->addLayout(nameRow);

        refreshStyle();
    }

    QString themeId() const { return m_id; }
    bool inUse() const { return m_inUse; }

    void setSelected(bool s) {
        if (m_selected == s) return;
        m_selected = s;
        refreshStyle();
    }
    bool isSelected() const { return m_selected; }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            if (m_favoriteBtn->geometry().contains(event->pos())) {
                emit_favoriteToggled();
                return;
            }
            emit_clicked();
        }
        QFrame::mousePressEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            emit_doubleClicked();
        }
        QFrame::mouseDoubleClickEvent(event);
    }

private:
    // Q_SIGNALS via parent: usamos parent dispatch ao invés de signals (QFrame não tem MOC aqui).
    // Pra simplicidade, usamos QEvent::Type custom. Mas é mais simples: callback via parent.
    // Vou usar property + custom event no parent.
    void emit_clicked();
    void emit_doubleClicked();
    void emit_favoriteToggled();

    void refreshStyle()
    {
        QString border = m_selected
            ? Theme::accentDefault()
            : (m_inUse ? Theme::accentSuccess() : Theme::panelBorder());
        const int bw = (m_selected || m_inUse) ? 2 : 1;
        setStyleSheet(QStringLiteral(
            "#themeCard { background: %1; border: %2px solid %3; border-radius: 8px; }"
            "#themeCard:hover { border-color: %4; }"
            "#themeCardName { color: %5; font-size: 12px; font-weight: 500; background: transparent; }"
            "#themeCardBadge { color: %6; font-size: 10px; background: %7; "
            "  padding: 2px 6px; border-radius: 8px; font-weight: 600; }"
            "#themeCardRoleBadge { color: %4; font-size: 13px; background: transparent; }"
            "#themeCardFavorite { color: %8; font-size: 15px; background: rgba(0,0,0,0.35); border-radius: 12px; }"
            "#themeCardFavorite:hover { color: %9; }"
        ).arg(
            Theme::panelBackground(),
            QString::number(bw),
            border,
            Theme::accentDefault(),
            Theme::textPrimary(),
            Theme::accentSuccess(),
            Theme::accentSuccessSoft(),
            m_favorite ? QStringLiteral("#ff5a7a") : QStringLiteral("rgba(255,255,255,0.65)"),
            QStringLiteral("#ff5a7a")
        ));
        m_favoriteBtn->setText(m_favorite ? QStringLiteral("♥") : QStringLiteral("♡"));
    }

    QString m_id;
    bool m_inUse;
    bool m_selected;
    bool m_favorite;
    ThemePreviewWidget* m_preview;
    QLabel* m_nameLabel;
    QLabel* m_favoriteBtn;
};

// Eventos custom pra propagar clicks do card ao painel pai (evita Q_OBJECT no card).
constexpr QEvent::Type kCardClickEvent = static_cast<QEvent::Type>(QEvent::User + 1101);
constexpr QEvent::Type kCardDoubleClickEvent = static_cast<QEvent::Type>(QEvent::User + 1102);
constexpr QEvent::Type kCardFavoriteEvent = static_cast<QEvent::Type>(QEvent::User + 1103);

class CardEvent : public QEvent {
public:
    CardEvent(QEvent::Type t, const QString& id) : QEvent(t), themeId(id) {}
    QString themeId;
};

void ThemeCard::emit_clicked()
{
    QApplication::postEvent(parent(), new CardEvent(kCardClickEvent, m_id));
}
void ThemeCard::emit_doubleClicked()
{
    QApplication::postEvent(parent(), new CardEvent(kCardDoubleClickEvent, m_id));
}
void ThemeCard::emit_favoriteToggled()
{
    QApplication::postEvent(parent(), new CardEvent(kCardFavoriteEvent, m_id));
}

// Container do grid que captura os custom events dos cards e despacha ao painel.
class CardGrid : public QWidget {
public:
    using ClickHandler = std::function<void(const QString&)>;
    using DoubleClickHandler = std::function<void(const QString&)>;
    using FavoriteHandler = std::function<void(const QString&)>;

    CardGrid(QWidget* parent = nullptr) : QWidget(parent) {}
    void setHandlers(ClickHandler c, DoubleClickHandler d, FavoriteHandler f) {
        m_click = std::move(c);
        m_double = std::move(d);
        m_favorite = std::move(f);
    }

protected:
    bool event(QEvent* e) override {
        if (e->type() == kCardClickEvent) {
            auto* ce = static_cast<CardEvent*>(e);
            if (m_click) m_click(ce->themeId);
            return true;
        }
        if (e->type() == kCardDoubleClickEvent) {
            auto* ce = static_cast<CardEvent*>(e);
            if (m_double) m_double(ce->themeId);
            return true;
        }
        if (e->type() == kCardFavoriteEvent) {
            auto* ce = static_cast<CardEvent*>(e);
            if (m_favorite) m_favorite(ce->themeId);
            return true;
        }
        return QWidget::event(e);
    }

private:
    ClickHandler m_click;
    DoubleClickHandler m_double;
    FavoriteHandler m_favorite;
};

} // namespace

ThemesPanel::ThemesPanel(QWidget* parent)
    : QDialog(parent)
    , m_tabs(nullptr)
    , m_bundledScroll(nullptr)
    , m_customScroll(nullptr)
    , m_bundledGrid(nullptr)
    , m_customGrid(nullptr)
    , m_applyButton(nullptr)
    , m_newButton(nullptr)
    , m_duplicateButton(nullptr)
    , m_editButton(nullptr)
    , m_deleteButton(nullptr)
    , m_closeButton(nullptr)
    , m_selectionInfo(nullptr)
    , m_searchEdit(nullptr)
    , m_autoSwitchCheck(nullptr)
    , m_autoSwitchBody(nullptr)
    , m_dayRoleCheck(nullptr)
    , m_nightRoleCheck(nullptr)
    , m_dayThemeLabel(nullptr)
    , m_nightThemeLabel(nullptr)
    , m_dayStartEdit(nullptr)
    , m_nightStartEdit(nullptr)
{
    setObjectName(QStringLiteral("themesPanel"));
    setWindowTitle(tr("Temas"));
    setModal(false);
    resize(720, 580);

    buildUi();
    applyDialogStyle();

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &ThemesPanel::onThemeChanged);
    connect(Theme::Manager::instance(), &Theme::Manager::customThemesChanged,
            this, &ThemesPanel::onCustomThemesChanged);
    connect(Theme::Manager::instance(), &Theme::Manager::autoSwitchConfigChanged,
            this, &ThemesPanel::onAutoSwitchConfigChanged);
    connect(Theme::Manager::instance(), &Theme::Manager::favoritesChanged,
            this, &ThemesPanel::onFavoritesChanged);

    // Seleciona o tema em uso por padrão.
    const QString currentId = Theme::Manager::instance()->current().id;
    m_selectedId = currentId;
    rebuildGrids();
    updateButtonsState();
    m_autoSwitchCheck->setChecked(Theme::Manager::instance()->autoSwitchConfig().enabled);
    m_autoSwitchBody->setVisible(m_autoSwitchCheck->isChecked());
    refreshAutoSwitchUi();
    if (Theme::Manager::instance()->isCustom(currentId)) {
        m_tabs->setCurrentIndex(TabCustom);
    }
}

void ThemesPanel::buildUi()
{
    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(16, 16, 16, 16);
    main->setSpacing(12);

    main->addWidget(buildSearchRow());

    // ---- Tabs com botões de ação como cornerWidget (top-right) ----
    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("themesPanelTabs"));

    auto buildScrollWithGrid = [this](QScrollArea*& scroll, QWidget*& grid) {
        scroll = new QScrollArea(m_tabs);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto* gridContainer = new CardGrid(scroll);
        gridContainer->setHandlers(
            [this](const QString& id) { selectId(id); },
            [this](const QString& id) { selectId(id); onApplyClicked(); },
            [](const QString& id) {
                auto* mgr = Theme::Manager::instance();
                mgr->setFavorite(id, !mgr->isFavorite(id));
            }
        );
        grid = gridContainer;
        scroll->setWidget(grid);
    };
    buildScrollWithGrid(m_bundledScroll, m_bundledGrid);
    buildScrollWithGrid(m_customScroll, m_customGrid);

    // Aba Padrão = barra de filtros de categoria + grid scrollável.
    auto* bundledPage = new QWidget(m_tabs);
    auto* bundledLay = new QVBoxLayout(bundledPage);
    bundledLay->setContentsMargins(0, 0, 0, 0);
    bundledLay->setSpacing(0);
    bundledLay->addWidget(buildCategoryFilterRow(bundledPage));
    bundledLay->addWidget(m_bundledScroll, 1);
    m_tabs->addTab(bundledPage, tr("Padrão"));
    m_tabs->addTab(m_customScroll, tr("Personalizado"));
    connect(m_tabs, &QTabWidget::currentChanged, this, &ThemesPanel::onTabChanged);

    // Corner widget: barra horizontal de ações alinhada com as abas.
    auto* corner = new QWidget(m_tabs);
    auto* cornerLayout = new QHBoxLayout(corner);
    cornerLayout->setContentsMargins(0, 4, 6, 0);
    cornerLayout->setSpacing(6);
    m_newButton = new QPushButton(tr("Novo"), corner);
    m_newButton->setObjectName(QStringLiteral("themesPanelActionBtn"));
    m_duplicateButton = new QPushButton(tr("Duplicar"), corner);
    m_duplicateButton->setObjectName(QStringLiteral("themesPanelActionBtn"));
    m_editButton = new QPushButton(tr("Editar"), corner);
    m_editButton->setObjectName(QStringLiteral("themesPanelActionBtn"));
    m_deleteButton = new QPushButton(tr("Excluir"), corner);
    m_deleteButton->setObjectName(QStringLiteral("themesPanelActionBtn"));
    cornerLayout->addWidget(m_newButton);
    cornerLayout->addWidget(m_duplicateButton);
    cornerLayout->addWidget(m_editButton);
    cornerLayout->addWidget(m_deleteButton);
    m_tabs->setCornerWidget(corner, Qt::TopRightCorner);

    main->addWidget(m_tabs, 1);

    main->addWidget(buildAutoSwitchRow());

    // ---- Footer: info da seleção (esquerda) + ações finais (direita) ----
    auto* footer = new QHBoxLayout;
    footer->setSpacing(12);
    m_selectionInfo = new QLabel(this);
    m_selectionInfo->setObjectName(QStringLiteral("themesPanelSelectionInfo"));
    m_selectionInfo->setWordWrap(true);
    footer->addWidget(m_selectionInfo, 1);

    m_closeButton = new QPushButton(tr("Fechar"), this);
    m_closeButton->setObjectName(QStringLiteral("themesPanelCloseBtn"));
    m_applyButton = new QPushButton(tr("Aplicar"), this);
    m_applyButton->setObjectName(QStringLiteral("themesPanelApplyBtn"));
    m_applyButton->setDefault(true);
    footer->addWidget(m_closeButton);
    footer->addWidget(m_applyButton);
    main->addLayout(footer);

    connect(m_applyButton, &QPushButton::clicked, this, &ThemesPanel::onApplyClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_newButton, &QPushButton::clicked, this, &ThemesPanel::onNewClicked);
    connect(m_duplicateButton, &QPushButton::clicked, this, &ThemesPanel::onDuplicateClicked);
    connect(m_editButton, &QPushButton::clicked, this, &ThemesPanel::onEditClicked);
    connect(m_deleteButton, &QPushButton::clicked, this, &ThemesPanel::onDeleteClicked);
}

QWidget* ThemesPanel::buildAutoSwitchRow()
{
    auto* wrap = new QWidget(this);
    wrap->setObjectName(QStringLiteral("autoSwitchWrap"));
    auto* wrapLay = new QVBoxLayout(wrap);
    wrapLay->setContentsMargins(0, 0, 0, 0);
    wrapLay->setSpacing(8);

    m_autoSwitchCheck = new QCheckBox(tr("Troca automática por horário"), wrap);
    m_autoSwitchCheck->setObjectName(QStringLiteral("autoSwitchMasterCheck"));
    m_autoSwitchCheck->setToolTip(
        tr("Alterna sozinho entre um tema diurno e um noturno, nos horários que você definir."));
    wrapLay->addWidget(m_autoSwitchCheck);

    m_autoSwitchBody = new QWidget(wrap);
    auto* bodyLay = new QHBoxLayout(m_autoSwitchBody);
    bodyLay->setContentsMargins(20, 0, 0, 0);
    bodyLay->setSpacing(20);

    // Coluna esquerda: atribuir o tema selecionado no grid ao papel diurno/noturno.
    auto* rolesCol = new QVBoxLayout;
    rolesCol->setSpacing(4);
    auto* dayRow = new QHBoxLayout;
    m_dayRoleCheck = new QCheckBox(tr("Diurno"), m_autoSwitchBody);
    m_dayThemeLabel = new QLabel(m_autoSwitchBody);
    m_dayThemeLabel->setObjectName(QStringLiteral("autoSwitchRoleLabel"));
    dayRow->addWidget(m_dayRoleCheck);
    dayRow->addWidget(m_dayThemeLabel, 1);
    rolesCol->addLayout(dayRow);
    auto* nightRow = new QHBoxLayout;
    m_nightRoleCheck = new QCheckBox(tr("Noturno"), m_autoSwitchBody);
    m_nightThemeLabel = new QLabel(m_autoSwitchBody);
    m_nightThemeLabel->setObjectName(QStringLiteral("autoSwitchRoleLabel"));
    nightRow->addWidget(m_nightRoleCheck);
    nightRow->addWidget(m_nightThemeLabel, 1);
    rolesCol->addLayout(nightRow);
    rolesCol->addWidget(new QLabel(tr("Selecione um tema no grid acima e marque aqui pra defini-lo."), m_autoSwitchBody));
    bodyLay->addLayout(rolesCol, 1);

    // Coluna direita: horários de troca.
    auto* timesCol = new QVBoxLayout;
    timesCol->setSpacing(4);
    auto* dayTimeRow = new QHBoxLayout;
    dayTimeRow->addWidget(new QLabel(tr("Trocar pro diurno às"), m_autoSwitchBody));
    m_dayStartEdit = new QTimeEdit(m_autoSwitchBody);
    m_dayStartEdit->setDisplayFormat(QStringLiteral("HH:mm"));
    dayTimeRow->addWidget(m_dayStartEdit);
    timesCol->addLayout(dayTimeRow);
    auto* nightTimeRow = new QHBoxLayout;
    nightTimeRow->addWidget(new QLabel(tr("Trocar pro noturno às"), m_autoSwitchBody));
    m_nightStartEdit = new QTimeEdit(m_autoSwitchBody);
    m_nightStartEdit->setDisplayFormat(QStringLiteral("HH:mm"));
    nightTimeRow->addWidget(m_nightStartEdit);
    timesCol->addLayout(nightTimeRow);
    bodyLay->addLayout(timesCol);

    wrapLay->addWidget(m_autoSwitchBody);

    connect(m_autoSwitchCheck, &QCheckBox::toggled, this, &ThemesPanel::onAutoSwitchToggled);
    connect(m_dayRoleCheck, &QCheckBox::toggled, this, &ThemesPanel::onDayRoleToggled);
    connect(m_nightRoleCheck, &QCheckBox::toggled, this, &ThemesPanel::onNightRoleToggled);
    connect(m_dayStartEdit, &QTimeEdit::timeChanged, this, &ThemesPanel::onAutoSwitchTimeChanged);
    connect(m_nightStartEdit, &QTimeEdit::timeChanged, this, &ThemesPanel::onAutoSwitchTimeChanged);

    return wrap;
}

ThemesPanel::Tab ThemesPanel::activeTab() const
{
    return static_cast<Tab>(m_tabs->currentIndex());
}

bool ThemesPanel::selectedIsCustom() const
{
    return Theme::Manager::instance()->isCustom(m_selectedId);
}

void ThemesPanel::onTabChanged(int /*index*/)
{
    // Quando muda de aba, reseta a seleção pro tema em uso se ele for da nova aba,
    // senão pro primeiro da aba.
    auto* mgr = Theme::Manager::instance();
    const QString currentId = mgr->current().id;
    if (activeTab() == TabBundled) {
        const auto bundled = mgr->bundledThemes();
        bool currentIsBundled = false;
        for (const auto& t : bundled) if (t.id == currentId) { currentIsBundled = true; break; }
        m_selectedId = currentIsBundled ? currentId : (bundled.isEmpty() ? QString() : bundled.first().id);
    } else {
        const auto custom = mgr->customThemes();
        bool currentIsCustom = false;
        for (const auto& t : custom) if (t.id == currentId) { currentIsCustom = true; break; }
        m_selectedId = currentIsCustom ? currentId : (custom.isEmpty() ? QString() : custom.first().id);
    }
    rebuildGrids();
    updateButtonsState();
    refreshAutoSwitchUi();
}

void ThemesPanel::selectId(const QString& id)
{
    if (m_selectedId == id) return;
    m_selectedId = id;
    // Atualiza visual usando a lista de cards mantida em rebuildOneGrid.
    for (auto& ptr : m_cards) {
        if (auto* frame = ptr.data()) {
            auto* card = static_cast<ThemeCard*>(frame);
            card->setSelected(card->themeId() == m_selectedId);
        }
    }
    updateButtonsState();
    refreshAutoSwitchUi();
}

void ThemesPanel::updateButtonsState()
{
    auto* mgr = Theme::Manager::instance();
    const bool hasSel = !m_selectedId.isEmpty();
    const bool isCustom = hasSel && mgr->isCustom(m_selectedId);
    const bool isCurrent = hasSel && (m_selectedId == mgr->current().id);

    m_applyButton->setEnabled(hasSel && !isCurrent);
    m_duplicateButton->setEnabled(hasSel);
    m_editButton->setEnabled(isCustom);
    m_deleteButton->setEnabled(isCustom);

    // Info textual — inline no footer
    if (hasSel) {
        const Theme::MiraTheme* th = nullptr;
        for (const auto& t : mgr->available()) if (t.id == m_selectedId) { th = &t; break; }
        if (th) {
            const QString sep = QStringLiteral(" <span style='color:%1'>·</span> ").arg(Theme::textMuted());
            QString msg = QStringLiteral("<b>%1</b>").arg(th->name);
            if (isCurrent) {
                msg += sep + QStringLiteral("<span style='color:%1'>%2</span>")
                    .arg(Theme::accentSuccess(), tr("Em uso"));
            } else {
                msg += sep + QStringLiteral("<span style='color:%1'>%2</span>")
                    .arg(Theme::textMuted(), isCustom ? tr("Personalizado") : tr("Padrão"));
            }
            if (!th->backgroundImage.isEmpty()) {
                msg += sep + QStringLiteral("<span style='color:%1'>%2</span>")
                    .arg(Theme::textMuted(), tr("Com imagem"));
            }
            m_selectionInfo->setText(msg);
        }
    } else {
        m_selectionInfo->setText(QStringLiteral("<i style='color:%1'>%2</i>")
            .arg(Theme::textMuted(), tr("Nenhum tema selecionado")));
    }
}

QWidget* ThemesPanel::buildSearchRow()
{
    auto* row = new QWidget(this);
    row->setObjectName(QStringLiteral("themesSearchRow"));
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_searchEdit = new QLineEdit(row);
    m_searchEdit->setObjectName(QStringLiteral("themesSearchEdit"));
    m_searchEdit->setPlaceholderText(tr("Buscar tema por nome…"));
    m_searchEdit->setClearButtonEnabled(true);
    lay->addWidget(m_searchEdit);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &ThemesPanel::onSearchTextChanged);

    return row;
}

QWidget* ThemesPanel::buildCategoryFilterRow(QWidget* parent)
{
    auto* row = new QWidget(parent);
    row->setObjectName(QStringLiteral("themesFilterRow"));
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(10, 8, 10, 4);
    lay->setSpacing(6);

    auto* group = new QButtonGroup(row);
    group->setExclusive(true);

    struct Chip { QString label; QString value; };
    const QList<Chip> chips = {
        { tr("Todos"),          QStringLiteral("all") },
        { tr("♥ Favoritos"),    QStringLiteral("favorites") },
        { tr("Claros"),         QStringLiteral("light") },
        { tr("Amarelados"),     QStringLiteral("warm") },
        { tr("Escuros"),        QStringLiteral("dark") },
        { tr("Coloridos"),      QStringLiteral("colorful") },
        { tr("Estampados"),     QStringLiteral("estampados") },
    };
    for (const Chip& c : chips) {
        auto* btn = new QPushButton(c.label, row);
        btn->setObjectName(QStringLiteral("themesFilterChip"));
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setChecked(c.value == m_categoryFilter);
        const QString value = c.value;
        connect(btn, &QPushButton::clicked, this, [this, value]() {
            if (m_categoryFilter == value) return;
            m_categoryFilter = value;
            rebuildGrids();
        });
        group->addButton(btn);
        lay->addWidget(btn);
    }
    lay->addStretch(1);
    return row;
}

void ThemesPanel::rebuildGrids()
{
    auto* mgr = Theme::Manager::instance();
    m_cards.clear();
    // Aba Padrão respeita o filtro de categoria.
    QList<Theme::MiraTheme> bundled = mgr->bundledThemes();
    if (m_categoryFilter == QStringLiteral("favorites")) {
        QList<Theme::MiraTheme> filtered;
        for (const auto& t : bundled)
            if (mgr->isFavorite(t.id)) filtered.append(t);
        bundled = filtered;
    } else if (m_categoryFilter != QStringLiteral("all")) {
        QList<Theme::MiraTheme> filtered;
        for (const auto& t : bundled)
            if (t.category == m_categoryFilter) filtered.append(t);
        bundled = filtered;
    }
    QList<Theme::MiraTheme> custom = mgr->customThemes();
    // Busca por nome — aplica nas duas abas, além do filtro de categoria.
    if (!m_searchText.isEmpty()) {
        QList<Theme::MiraTheme> filtered;
        for (const auto& t : bundled)
            if (t.name.toLower().contains(m_searchText)) filtered.append(t);
        bundled = filtered;
        filtered.clear();
        for (const auto& t : custom)
            if (t.name.toLower().contains(m_searchText)) filtered.append(t);
        custom = filtered;
    }
    rebuildOneGrid(m_bundledGrid, bundled, false);
    rebuildOneGrid(m_customGrid, custom, true);
    updateButtonsState();
}

void ThemesPanel::rebuildOneGrid(QWidget* gridContainer, const QList<Theme::MiraTheme>& themes, bool custom)
{
    if (!gridContainer) return;
    // Limpa filhos.
    if (gridContainer->layout()) {
        QLayoutItem* item;
        while ((item = gridContainer->layout()->takeAt(0))) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        delete gridContainer->layout();
    }

    auto* gl = new QGridLayout(gridContainer);
    gl->setContentsMargins(8, 12, 8, 8);
    gl->setSpacing(kCardSpacing);
    gl->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    if (themes.isEmpty()) {
        QString message;
        if (!m_searchText.isEmpty()) {
            message = tr("Nenhum tema encontrado.");
        } else if (!custom && m_categoryFilter == QStringLiteral("favorites")) {
            message = tr("Nenhum favorito ainda.\nClique no coração de um tema pra marcá-lo.");
        } else if (custom) {
            message = tr("Nenhum tema personalizado ainda.\nUse \"Duplicar\" em um tema padrão pra começar.");
        } else {
            message = tr("Nenhum tema nessa categoria.");
        }
        auto* empty = new QLabel(message, gridContainer);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(Theme::textMuted()));
        gl->addWidget(empty, 0, 0);
        return;
    }

    const auto* mgr = Theme::Manager::instance();
    const QString currentId = mgr->current().id;
    const auto& autoSwitch = mgr->autoSwitchConfig();
    // 3 colunas por linha — vai esticar conforme o espaço.
    const int cols = 3;
    for (int i = 0; i < themes.size(); ++i) {
        const auto& t = themes.at(i);
        const bool dayRole = !autoSwitch.dayThemeId.isEmpty() && t.id == autoSwitch.dayThemeId;
        const bool nightRole = !autoSwitch.nightThemeId.isEmpty() && t.id == autoSwitch.nightThemeId;
        auto* card = new ThemeCard(t, t.id == currentId, dayRole, nightRole,
                                    mgr->isFavorite(t.id), gridContainer);
        card->setSelected(t.id == m_selectedId);
        gl->addWidget(card, i / cols, i % cols);
        m_cards.append(QPointer<QFrame>(card));
    }
    // Stretch nas células vazias da última linha pra cards não esticarem.
    gl->setColumnStretch(cols, 1);
    gl->setRowStretch((themes.size() / cols) + 1, 1);
}

void ThemesPanel::onApplyClicked()
{
    if (m_selectedId.isEmpty()) return;
    Theme::Manager::instance()->setCurrent(m_selectedId);
    // onThemeChanged vai rebuildar os grids.
}

void ThemesPanel::onNewClicked()
{
    // Cria custom em branco (baseado no Grafite/bundled[0]).
    auto* mgr = Theme::Manager::instance();
    Theme::MiraTheme base = mgr->bundledThemes().value(0);
    base.bundled = false;
    base.id = mgr->uniqueCustomId();
    base.name = tr("Novo tema");

    ThemeEditorDialog dlg(base, this);
    if (dlg.exec() == QDialog::Accepted) {
        const QString id = mgr->upsertCustom(dlg.theme());
        m_tabs->setCurrentIndex(TabCustom);
        m_selectedId = id;
        rebuildGrids();
    }
}

void ThemesPanel::onDuplicateClicked()
{
    if (m_selectedId.isEmpty()) return;
    auto* mgr = Theme::Manager::instance();
    const Theme::MiraTheme* found = nullptr;
    for (const auto& t : mgr->available()) if (t.id == m_selectedId) { found = &t; break; }
    if (!found) return;

    Theme::MiraTheme copy = *found;
    copy.bundled = false;
    copy.id = mgr->uniqueCustomId();
    copy.name = tr("%1 (cópia)").arg(found->name);

    ThemeEditorDialog dlg(copy, this);
    if (dlg.exec() == QDialog::Accepted) {
        const QString id = mgr->upsertCustom(dlg.theme());
        m_tabs->setCurrentIndex(TabCustom);
        m_selectedId = id;
        rebuildGrids();
    }
}

void ThemesPanel::onEditClicked()
{
    if (!selectedIsCustom()) return;
    auto* mgr = Theme::Manager::instance();
    const Theme::MiraTheme* found = nullptr;
    for (const auto& t : mgr->available()) if (t.id == m_selectedId) { found = &t; break; }
    if (!found) return;

    ThemeEditorDialog dlg(*found, this);
    if (dlg.exec() == QDialog::Accepted) {
        const QString id = mgr->upsertCustom(dlg.theme());
        m_selectedId = id;
        rebuildGrids();
    }
}

void ThemesPanel::onDeleteClicked()
{
    if (!selectedIsCustom()) return;
    const auto answer = QMessageBox::question(
        this, tr("Excluir tema"),
        tr("Excluir este tema personalizado? Esta ação não pode ser desfeita."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    Theme::Manager::instance()->removeCustom(m_selectedId);
    m_selectedId.clear();
    rebuildGrids();
}

void ThemesPanel::onThemeChanged()
{
    rebuildGrids();
    applyDialogStyle();
}

void ThemesPanel::onCustomThemesChanged()
{
    rebuildGrids();
}

void ThemesPanel::onSearchTextChanged(const QString& text)
{
    m_searchText = text.trimmed().toLower();
    rebuildGrids();
}

void ThemesPanel::onFavoritesChanged()
{
    rebuildGrids();
}

void ThemesPanel::onAutoSwitchToggled(bool checked)
{
    if (m_syncingAutoSwitchUi) return;
    m_autoSwitchBody->setVisible(checked);

    auto* mgr = Theme::Manager::instance();
    Theme::AutoSwitchConfig cfg = mgr->autoSwitchConfig();
    if (!checked) {
        cfg.enabled = false;
        mgr->setAutoSwitchConfig(cfg);
        return;
    }
    // Só liga de verdade quando os dois papéis já estão atribuídos — senão o
    // checkbox fica marcado (corpo expandido) esperando a atribuição.
    if (!cfg.dayThemeId.isEmpty() && !cfg.nightThemeId.isEmpty()) {
        cfg.enabled = true;
        mgr->setAutoSwitchConfig(cfg);
    }
}

void ThemesPanel::onDayRoleToggled(bool checked)
{
    if (m_syncingAutoSwitchUi || m_selectedId.isEmpty()) return;
    auto* mgr = Theme::Manager::instance();
    Theme::AutoSwitchConfig cfg = mgr->autoSwitchConfig();
    if (checked) {
        cfg.dayThemeId = m_selectedId;
        if (cfg.nightThemeId == m_selectedId) cfg.nightThemeId.clear();
    } else if (cfg.dayThemeId == m_selectedId) {
        cfg.dayThemeId.clear();
    }
    cfg.enabled = m_autoSwitchCheck->isChecked()
        && !cfg.dayThemeId.isEmpty() && !cfg.nightThemeId.isEmpty();
    mgr->setAutoSwitchConfig(cfg);
}

void ThemesPanel::onNightRoleToggled(bool checked)
{
    if (m_syncingAutoSwitchUi || m_selectedId.isEmpty()) return;
    auto* mgr = Theme::Manager::instance();
    Theme::AutoSwitchConfig cfg = mgr->autoSwitchConfig();
    if (checked) {
        cfg.nightThemeId = m_selectedId;
        if (cfg.dayThemeId == m_selectedId) cfg.dayThemeId.clear();
    } else if (cfg.nightThemeId == m_selectedId) {
        cfg.nightThemeId.clear();
    }
    cfg.enabled = m_autoSwitchCheck->isChecked()
        && !cfg.dayThemeId.isEmpty() && !cfg.nightThemeId.isEmpty();
    mgr->setAutoSwitchConfig(cfg);
}

void ThemesPanel::onAutoSwitchTimeChanged()
{
    if (m_syncingAutoSwitchUi) return;
    auto* mgr = Theme::Manager::instance();
    Theme::AutoSwitchConfig cfg = mgr->autoSwitchConfig();
    cfg.dayStart = m_dayStartEdit->time();
    cfg.nightStart = m_nightStartEdit->time();
    mgr->setAutoSwitchConfig(cfg);
}

void ThemesPanel::onAutoSwitchConfigChanged()
{
    const auto& cfg = Theme::Manager::instance()->autoSwitchConfig();
    // Desligamento externo de verdade (override manual de tema ou exclusão do
    // tema de um papel) — os dois ids continuam preenchidos mas enabled caiu
    // sozinho. Atribuição incompleta de papéis (algum id vazio, no meio do
    // fluxo de configuração) não deve recolher o painel.
    if (!cfg.enabled && m_autoSwitchCheck->isChecked()
        && !cfg.dayThemeId.isEmpty() && !cfg.nightThemeId.isEmpty()) {
        m_syncingAutoSwitchUi = true;
        m_autoSwitchCheck->setChecked(false);
        m_syncingAutoSwitchUi = false;
    }
    m_autoSwitchBody->setVisible(m_autoSwitchCheck->isChecked());
    refreshAutoSwitchUi();
    rebuildGrids();
}

void ThemesPanel::refreshAutoSwitchUi()
{
    if (!m_autoSwitchCheck) return;
    const auto* mgr = Theme::Manager::instance();
    const auto& cfg = mgr->autoSwitchConfig();

    m_syncingAutoSwitchUi = true;

    const Theme::MiraTheme* dayTheme = nullptr;
    const Theme::MiraTheme* nightTheme = nullptr;
    for (const auto& t : mgr->available()) {
        if (t.id == cfg.dayThemeId) dayTheme = &t;
        if (t.id == cfg.nightThemeId) nightTheme = &t;
    }
    m_dayRoleCheck->setChecked(!m_selectedId.isEmpty() && m_selectedId == cfg.dayThemeId);
    m_nightRoleCheck->setChecked(!m_selectedId.isEmpty() && m_selectedId == cfg.nightThemeId);
    m_dayThemeLabel->setText(dayTheme
        ? tr("— %1").arg(dayTheme->name)
        : tr("— nenhum tema definido"));
    m_nightThemeLabel->setText(nightTheme
        ? tr("— %1").arg(nightTheme->name)
        : tr("— nenhum tema definido"));
    m_dayStartEdit->setTime(cfg.dayStart);
    m_nightStartEdit->setTime(cfg.nightStart);

    m_syncingAutoSwitchUi = false;
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
        #themesPanelSelectionInfo {
            font-size: 12px;
            padding: 0 4px;
        }
        #themesPanelTabs::pane {
            border: 1px solid %6;
            border-radius: 8px;
            background: %5;
            top: -1px;
        }
        #themesPanelTabs::tab-bar {
            left: 8px;
        }
        #themesPanelTabs QTabBar::tab {
            background: transparent;
            color: %4;
            padding: 8px 18px;
            border: 1px solid transparent;
            border-bottom: none;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            font-size: 12px;
            margin-right: 4px;
        }
        #themesPanelTabs QTabBar::tab:selected {
            background: %5;
            color: %3;
            border-color: %6;
        }
        #themesPanelTabs QTabBar::tab:hover:!selected {
            color: %2;
        }
        QScrollArea {
            background: transparent;
            border: none;
        }
        QScrollArea > QWidget > QWidget {
            background: transparent;
        }
        QPushButton#themesPanelActionBtn {
            background: %5;
            color: %2;
            border: 1px solid %6;
            padding: 5px 12px;
            border-radius: 5px;
            font-size: 11px;
        }
        QPushButton#themesPanelActionBtn:hover {
            background: %7;
            color: %3;
            border-color: %9;
        }
        QPushButton#themesPanelActionBtn:disabled {
            color: %4;
            background: transparent;
            border-color: %8;
        }
        QPushButton#themesPanelCloseBtn, QPushButton#themesPanelApplyBtn {
            background: %5;
            color: %2;
            border: 1px solid %6;
            padding: 7px 22px;
            border-radius: 6px;
            font-size: 12px;
        }
        QPushButton#themesPanelApplyBtn {
            background: %9;
            color: %3;
            border-color: %9;
        }
        QPushButton#themesPanelApplyBtn:hover {
            background: %9;
        }
        QPushButton#themesPanelApplyBtn:disabled {
            background: transparent;
            color: %4;
            border-color: %8;
        }
        QPushButton#themesPanelCloseBtn:hover {
            background: %7;
            color: %3;
        }
        #themesFilterRow { background: transparent; }
        QPushButton#themesFilterChip {
            background: transparent;
            color: %4;
            border: 1px solid %6;
            padding: 4px 12px;
            border-radius: 12px;
            font-size: 11px;
        }
        QPushButton#themesFilterChip:hover { color: %3; border-color: %9; }
        QPushButton#themesFilterChip:checked {
            background: %9;
            color: %3;
            border-color: %9;
        }
        #autoSwitchWrap QCheckBox {
            color: %2;
            font-size: 12px;
            spacing: 6px;
        }
        #autoSwitchMasterCheck { font-weight: 600; }
        #autoSwitchRoleLabel { color: %4; font-size: 11px; }
        #autoSwitchWrap QTimeEdit {
            background: %5;
            color: %2;
            border: 1px solid %6;
            border-radius: 4px;
            padding: 2px 6px;
            font-size: 12px;
        }
        QLineEdit#themesSearchEdit {
            background: %5;
            color: %2;
            border: 1px solid %6;
            border-radius: 8px;
            padding: 6px 10px;
            font-size: 12px;
        }
        QLineEdit#themesSearchEdit:focus { border-color: %9; }
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
