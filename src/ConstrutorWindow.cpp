#include "ConstrutorWindow.h"
#include "TerritorioStore.h"
#include "Theme.h"
#include "WorldContentEditor.h"

#include <QApplication>
#include <QColor>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QStyledItemDelegate>

// Delegate de dois níveis para a lista de sistemas:
//   Linha 1 — nome do sistema (13 px)
//   Linha 2 — "Categoria | Waypoint" (11 px, muted)
class SystemItemDelegate final : public QStyledItemDelegate {
public:
    QString colorPrimary  = QStringLiteral("#e0e0e0");
    QString colorMuted    = QStringLiteral("#888888");
    QString colorSelected = QStringLiteral("#f5f5f5");

    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return { 160, 50 };
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem o = opt;
        initStyleOption(&o, index);
        o.text.clear();
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &o, p, o.widget);

        const bool sel     = o.state & QStyle::State_Selected;
        const QString name = index.data(Qt::DisplayRole).toString();
        const QString sub  = index.data(Qt::UserRole + 2).toString();

        p->save();
        const int midY = o.rect.center().y();
        const QRect lr(o.rect.left() + 10, 0, o.rect.width() - 20, 0);

        QFont nf = o.font;
        nf.setPixelSize(13);
        p->setFont(nf);
        p->setPen(QColor(sel ? colorSelected : colorPrimary));
        p->drawText(QRect(lr.left(), midY - 18, lr.width(), 18),
                    Qt::AlignLeft | Qt::AlignVCenter, name);

        if (!sub.isEmpty()) {
            QFont sf = o.font;
            sf.setPixelSize(11);
            p->setFont(sf);
            p->setPen(QColor(sel ? colorPrimary : colorMuted));
            p->drawText(QRect(lr.left(), midY + 2, lr.width(), 16),
                        Qt::AlignLeft | Qt::AlignVCenter, sub);
        }
        p->restore();
    }
};

// ── Theme ──────────────────────────────────────────────────────────────────────

void ConstrutorWindow::applyTheme()
{
    const QString panelBg   = Theme::panelBackground();
    const QString border    = Theme::panelBorder();
    const QString subtle    = Theme::subtleBorder();
    const QString txtPrim   = Theme::textPrimary();
    const QString txtMuted  = Theme::textMuted();
    const QString txtBright = Theme::textBright();
    const QString hover     = Theme::hoverOverlay();
    const QString accentSf  = Theme::accentInfoSoft();
    const QString accentBd  = Theme::accentInfoBorderSoft();
    const QString accentDef = Theme::accentDefault();
    const QString dangerSf  = Theme::accentDangerSoft();
    const QString danger    = Theme::accentDanger();
    const QString disabled  = Theme::disabledText();
    const QString editorBg  = Theme::editorBackground();
    const QString success   = Theme::accentSuccess();
    const QString warning   = Theme::accentWarning();

    setStyleSheet(QStringLiteral(R"(
        ConstrutorWindow { background: %1; }

        QWidget#ctrLeftHeader { background: %1; border-bottom: 1px solid %4; }
        QLabel#ctrLeftTitle {
            color: %6;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 1px;
        }

        QLineEdit#ctrSearchEdit {
            background: %2;
            color: %5;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 5px 8px;
            font-size: 12px;
        }
        QLineEdit#ctrSearchEdit:focus { border-color: %10; }

        QListWidget#ctrSearchResults {
            background: transparent;
            color: %5;
            border: none;
            font-size: 12px;
            outline: none;
        }
        QListWidget#ctrSearchResults::item { padding: 6px 8px; border-radius: 6px; }
        QListWidget#ctrSearchResults::item:hover { background: %8; color: %7; }
        QListWidget#ctrSearchResults::item:selected { background: %9; color: %7; }

        QListWidget#ctrSystemsList {
            background: transparent;
            color: %5;
            border: none;
            font-size: 13px;
            outline: none;
        }
        QListWidget#ctrSystemsList::item { padding: 4px 8px; border-radius: 6px; }
        QListWidget#ctrSystemsList::item:hover { background: %8; color: %7; }
        QListWidget#ctrSystemsList::item:selected { background: %9; color: %7; }

        QPushButton#ctrNewSystem {
            background: %9;
            color: %7;
            border: 1px solid %10;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 12px;
            font-weight: 600;
        }
        QPushButton#ctrNewSystem:hover { background: %10; }

        QWidget#ctrSysDetail { background: %1; }
        QFrame#ctrHSep { background: %4; border: none; max-height: 1px; }

        QLineEdit#ctrSysName {
            background: %2;
            color: %7;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 5px 8px;
            font-size: 13px;
            font-weight: 600;
        }
        QLineEdit#ctrSysName:focus { border-color: %10; }

        QLabel#ctrCatBadge {
            background: %9;
            color: %7;
            font-size: 10px;
            font-weight: 700;
            padding: 2px 8px;
            border: 1px solid %10;
            border-radius: 9px;
        }

        QPushButton#ctrDeleteSys, QPushButton#ctrMentionsToggle {
            background: transparent;
            color: %6;
            border: 1px solid %3;
            border-radius: 6px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton#ctrDeleteSys:hover, QPushButton#ctrMentionsToggle:hover { background: %12; color: %13; border-color: %13; }
        QPushButton#ctrMentionsToggle:checked { background: %9; color: %7; border-color: %10; }

        QLabel#ctrWaypointName { color: %7; font-size: 12px; font-weight: 700; }
        QLabel#ctrWaypointEdge { color: %6; font-size: 10px; }
        QSlider#ctrSlider::groove:horizontal { height: 4px; background: %3; border-radius: 2px; }
        QSlider#ctrSlider::handle:horizontal {
            background: %11; border: none;
            width: 14px; height: 14px; margin: -5px 0; border-radius: 7px;
        }
        QSlider#ctrSlider::sub-page:horizontal { background: %11; border-radius: 2px; }
        QLabel#ctrWaypointTip {
            color: %6; font-size: 10px; font-style: italic; padding: 2px 0 0 0;
        }

        QLabel#ctrFavorsTitle {
            color: %16; font-size: 9px; font-weight: 700; letter-spacing: 1px;
        }
        QLabel#ctrDemandsTitle {
            color: %17; font-size: 9px; font-weight: 700; letter-spacing: 1px;
        }
        QLabel#ctrFavorsList, QLabel#ctrDemandsList {
            color: %6; font-size: 11px; padding: 2px 0;
        }
        QPushButton#ctrTradeoffExpand {
            background: transparent; color: %6; border: 1px solid %3;
            border-radius: 9px; font-size: 10px; font-weight: 700;
        }
        QPushButton#ctrTradeoffExpand:hover { background: %8; color: %7; border-color: %4; }
        QPushButton#ctrTradeoffExpand:checked { background: %9; color: %7; border-color: %10; }

        QPushButton#ctrAddRule, QPushButton#ctrAddSection {
            background: %9; color: %7; border: 1px solid %10;
            border-radius: 6px; padding: 4px 10px; font-size: 11px;
        }
        QPushButton#ctrAddRule:hover, QPushButton#ctrAddSection:hover { background: %10; }

        QPushButton#ctrDeleteNode {
            background: transparent; color: %6; border: 1px solid %3;
            border-radius: 6px; font-size: 13px; font-weight: 600;
        }
        QPushButton#ctrDeleteNode:hover { background: %12; color: %13; border-color: %13; }
        QPushButton#ctrDeleteNode:disabled { color: %14; border-color: %3; }

        QTreeWidget#ctrTree {
            background: %1; color: %5; border: none;
            font-size: 13px; outline: none;
        }
        QTreeWidget#ctrTree::item { padding: 4px 6px; border-radius: 4px; }
        QTreeWidget#ctrTree::item:hover { background: %8; color: %7; }
        QTreeWidget#ctrTree::item:selected { background: %9; color: %7; }

        QWidget#ctrMentionsPanel {
            background: %2; border: 1px solid %3; border-radius: 10px;
        }
        QWidget#ctrMentionsPanel QScrollArea { background: transparent; border: none; }
        QWidget#ctrMentionsPanel QScrollArea > QWidget { background: transparent; }
        QLabel#ctrMentionsTitle {
            color: %6; font-size: 10px; font-weight: 700; letter-spacing: 1px;
        }
        QToolButton#ctrMentionsCloseBtn {
            color: %6; border: none; background: transparent; font-size: 14px;
        }
        QToolButton#ctrMentionsCloseBtn:hover { color: %7; }
        QLabel#ctrMentionsEmpty { color: %6; font-size: 11px; font-style: italic; }
        QFrame#ctrMentionCard {
            background: %15; border: 1px solid %3; border-radius: 6px;
        }
        QFrame#ctrMentionCard:hover { border-color: %10; }
        QLabel#ctrMentionCardSource { color: %6; font-size: 10px; }
        QLabel#ctrMentionCardQuote { color: %5; font-size: 12px; }
        QToolButton#ctrMentionDelete {
            color: %6; border: none; background: transparent; font-size: 13px;
        }
        QToolButton#ctrMentionDelete:hover { color: %13; }

        QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }
        QScrollBar::handle:vertical { background: %3; border-radius: 3px; min-height: 20px; }
        QScrollBar::handle:vertical:hover { background: %6; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )").arg(panelBg, panelBg, border, subtle, txtPrim)   // %1-5
       .arg(txtMuted, txtBright, hover, accentSf)      // %6-9
       .arg(accentBd, accentDef, dangerSf, danger)     // %10-13
       .arg(disabled, editorBg)                        // %14-15
       .arg(success, warning));                        // %16-17

    if (m_sysDelegate) {
        m_sysDelegate->colorPrimary  = txtPrim;
        m_sysDelegate->colorMuted    = txtMuted;
        m_sysDelegate->colorSelected = txtBright;
        if (m_systemsList) m_systemsList->update();
    }
}

#include <QAction>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <functional>

namespace {
constexpr int kNodeIdRole   = Qt::UserRole;
constexpr int kNodeTypeRole = Qt::UserRole + 1;

// Busca recursiva por nome — alimenta a lista de resultados da busca global
// (breadcrumb "Sistema ▸ Nó ▸ Subnó" pra desambiguar nomes repetidos entre
// sistemas diferentes).
void collectNodeMatches(const QString& systemId, const QString& breadcrumb,
                         const QList<ConstrutorStore::Node>& nodes,
                         const QString& needle, QListWidget* results)
{
    for (const auto& n : nodes) {
        const QString path = breadcrumb + QStringLiteral(" ▸ ") + n.name;
        if (n.name.contains(needle, Qt::CaseInsensitive)) {
            auto* item = new QListWidgetItem(path, results);
            item->setData(Qt::UserRole, systemId);
            item->setData(Qt::UserRole + 1, n.id);
        }
        collectNodeMatches(systemId, path, n.children, needle, results);
    }
}
}

ConstrutorWindow::ConstrutorWindow(ConstrutorStore* store, WorldContentEditor* editor, QWidget* parent)
    : QWidget(parent)
    , m_editor(editor)
{
    buildUi();
    showNoSystemOpenState();
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &ConstrutorWindow::applyTheme);

    setStore(store);
}

void ConstrutorWindow::setEditorActive(bool active)
{
    if (m_editorActive == active) return;
    m_editorActive = active;
    if (!active) {
        // Perdeu a posse do editor compartilhado (Território assumiu) —
        // limpa a própria seleção pra não ficar mostrando destaque num
        // sistema/nó cujo conteúdo não é mais o que está no editor. Clicar
        // de novo (mesmo no mesmo item) volta a disparar o sinal de seleção
        // e reclama o editor de volta.
        m_rebuilding = true;
        if (m_systemsList) m_systemsList->setCurrentItem(nullptr);
        m_rebuilding = false;
        m_currentSystemId.clear();
        m_currentNodeId.clear();
        if (m_sysDetail) m_sysDetail->setVisible(false);
        if (m_tree) m_tree->setVisible(false);
        if (m_deleteNodeBtn) m_deleteNodeBtn->setEnabled(false);
        rebuildMentionsPanel();
    }
}

void ConstrutorWindow::flushPendingContent()
{
    if (m_editorActive) saveCurrentNodeContent();
}

void ConstrutorWindow::claimEditor()
{
    if (!m_editorActive) {
        m_editorActive = true;
        emit editorOwnershipRequested();
    }
}

void ConstrutorWindow::setTerritorioStore(TerritorioStore* store)
{
    // Igualdade de ponteiro só controla o (dis)connect do sinal — o mesmo
    // ponteiro é reaproveitado entre projetos (setProjectRoot()+load() não
    // emite changed()), então SEMPRE reconstrói o menu, nunca pula por
    // igualdade sozinha (mesma classe de bug já corrigida em setStore()).
    if (m_territorioStore != store) {
        if (m_territorioStore) disconnect(m_territorioStore, nullptr, this, nullptr);
        m_territorioStore = store;
        if (m_territorioStore)
            connect(m_territorioStore, &TerritorioStore::changed, this, &ConstrutorWindow::rebuildTerritoryMenu);
    }
    rebuildTerritoryMenu();
}

void ConstrutorWindow::rebuildTerritoryMenu()
{
    if (!m_territoryBtn) return;
    if (!m_territorioStore || m_currentSystemId.isEmpty() || !m_store) {
        m_territoryBtn->setVisible(false);
        return;
    }
    const ConstrutorStore::System* sys = m_store->system(m_currentSystemId);
    if (!sys) { m_territoryBtn->setVisible(false); return; }

    m_territoryBtn->setVisible(true);
    if (sys->territoryIds.isEmpty()) {
        m_territoryBtn->setText(tr("🌐 Território: Global"));
    } else {
        QStringList names;
        for (const auto& tid : sys->territoryIds) {
            const auto* t = m_territorioStore->territorio(tid);
            if (t) names << t->name;
        }
        m_territoryBtn->setText(tr("🌐 Território: %1").arg(names.join(QStringLiteral(", "))));
    }

    if (QMenu* old = m_territoryBtn->menu()) old->deleteLater();
    auto* menu = new QMenu(m_territoryBtn);
    const QString systemId = m_currentSystemId;
    for (const auto& t : m_territorioStore->territorios()) {
        QAction* a = menu->addAction(t.name);
        a->setCheckable(true);
        a->setChecked(sys->territoryIds.contains(t.id));
        const QString tid = t.id;
        connect(a, &QAction::toggled, this, [this, systemId, tid](bool checked) {
            if (!m_store) return;
            const ConstrutorStore::System* s = m_store->system(systemId);
            if (!s) return;
            QStringList ids = s->territoryIds;
            if (checked && !ids.contains(tid)) ids << tid;
            else if (!checked) ids.removeAll(tid);
            m_store->updateSystemTerritories(systemId, ids);
        });
    }
    if (m_territorioStore->territorios().isEmpty()) {
        QAction* empty = menu->addAction(tr("Nenhum território criado ainda"));
        empty->setEnabled(false);
    }
    m_territoryBtn->setMenu(menu);
}

void ConstrutorWindow::setStore(ConstrutorStore* store)
{
    if (m_store != store) {
        if (m_store)
            disconnect(m_store, &ConstrutorStore::changed, this, &ConstrutorWindow::onStoreChanged);
        m_store = store;
        if (m_store)
            connect(m_store, &ConstrutorStore::changed, this, &ConstrutorWindow::onStoreChanged);
    }
    if (m_searchEdit) m_searchEdit->clear();
    // Reconstrói sempre: o mesmo ponteiro de store é reaproveitado entre
    // projetos (só o conteúdo interno muda via setProjectRoot()+load()),
    // então a igualdade de ponteiro sozinha não pode ser usada pra pular
    // o rebuild.
    if (m_store) rebuildSystemsList();
}

void ConstrutorWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_mentionsPanel && m_mentionsPanel->isVisible()) anchorMentionsPanel();
}

bool ConstrutorWindow::eventFilter(QObject* watched, QEvent* event)
{
    // Card de menção: clicar abre a origem no editor (sem menu — só há uma
    // ação possível, o botão × de excluir já é um widget filho próprio).
    if (event->type() == QEvent::MouseButtonRelease) {
        if (auto* w = qobject_cast<QWidget*>(watched)) {
            const QVariant idProp = w->property("mentionId");
            if (idProp.isValid()) {
                auto* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton && w->rect().contains(me->position().toPoint())) {
                    const ConstrutorStore::System* sys =
                        m_store ? m_store->system(m_currentSystemId) : nullptr;
                    if (sys) {
                        const QString mentionId = idProp.toString();
                        for (const auto& m : sys->mentions) {
                            if (m.id == mentionId) { emit openMentionInEditorRequested(m); break; }
                        }
                    }
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ── UI ────────────────────────────────────────────────────────────────────────

void ConstrutorWindow::buildUi()
{
    auto* leftLay = new QVBoxLayout(this);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(0);

    // Header
    auto* leftHeader = new QWidget(this);
    leftHeader->setObjectName(QStringLiteral("ctrLeftHeader"));
    auto* lhLay = new QHBoxLayout(leftHeader);
    lhLay->setContentsMargins(12, 10, 12, 6);
    auto* leftTitle = new QLabel(tr("SISTEMAS"), leftHeader);
    leftTitle->setObjectName(QStringLiteral("ctrLeftTitle"));
    lhLay->addWidget(leftTitle);
    lhLay->addStretch();
    // Mostrar/ocultar "Menções no projeto" — antes ficava na toolbar do
    // editor próprio; agora que o editor é compartilhado, mora aqui.
    m_mentionsToggleBtn = new QPushButton(QStringLiteral("@"), leftHeader);
    m_mentionsToggleBtn->setObjectName(QStringLiteral("ctrMentionsToggle"));
    m_mentionsToggleBtn->setCursor(Qt::PointingHandCursor);
    m_mentionsToggleBtn->setCheckable(true);
    m_mentionsToggleBtn->setFixedSize(24, 24);
    m_mentionsToggleBtn->setToolTip(tr("Mostrar/ocultar menções no projeto"));
    connect(m_mentionsToggleBtn, &QPushButton::toggled, this, [this](bool checked) {
        if (!m_mentionsPanel) return;
        m_mentionsPanel->setVisible(checked);
        if (checked) anchorMentionsPanel();
    });
    lhLay->addWidget(m_mentionsToggleBtn);
    leftLay->addWidget(leftHeader);

    // Busca global entre sistemas e nós (por nome)
    auto* searchWrap = new QWidget(this);
    auto* searchLay  = new QHBoxLayout(searchWrap);
    searchLay->setContentsMargins(12, 0, 12, 8);
    m_searchEdit = new QLineEdit(searchWrap);
    m_searchEdit->setObjectName(QStringLiteral("ctrSearchEdit"));
    m_searchEdit->setPlaceholderText(tr("Buscar sistema ou nó…"));
    m_searchEdit->setClearButtonEnabled(true);
    searchLay->addWidget(m_searchEdit);
    leftLay->addWidget(searchWrap);

    m_searchResultsList = new QListWidget(this);
    m_searchResultsList->setObjectName(QStringLiteral("ctrSearchResults"));
    m_searchResultsList->setFrameShape(QFrame::NoFrame);
    m_searchResultsList->setVisible(false);
    leftLay->addWidget(m_searchResultsList, 1);

    // Lista de sistemas
    m_systemsList = new QListWidget(this);
    m_systemsList->setObjectName(QStringLiteral("ctrSystemsList"));
    m_systemsList->setFrameShape(QFrame::NoFrame);
    m_systemsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_systemsList->setMaximumHeight(210);
    m_sysDelegate = new SystemItemDelegate(m_systemsList);
    m_systemsList->setItemDelegate(m_sysDelegate);
    leftLay->addWidget(m_systemsList);

    // Botão novo sistema
    auto* newSysWrap = new QWidget(this);
    auto* newSysLay  = new QHBoxLayout(newSysWrap);
    newSysLay->setContentsMargins(12, 4, 12, 8);
    m_newSystemBtn = new QPushButton(tr("+ Novo sistema"), newSysWrap);
    m_newSystemBtn->setObjectName(QStringLiteral("ctrNewSystem"));
    m_newSystemBtn->setCursor(Qt::PointingHandCursor);
    newSysLay->addWidget(m_newSystemBtn);
    leftLay->addWidget(newSysWrap);

    // ── Detalhe do sistema selecionado ────────────────────────────────────────
    m_sysDetail = new QWidget(this);
    m_sysDetail->setObjectName(QStringLiteral("ctrSysDetail"));
    m_sysDetail->setVisible(false);
    auto* sdLay = new QVBoxLayout(m_sysDetail);
    sdLay->setContentsMargins(12, 4, 12, 8);
    sdLay->setSpacing(6);

    auto makeHSep = [&](QWidget* parent) {
        auto* s = new QFrame(parent);
        s->setFrameShape(QFrame::HLine);
        s->setObjectName(QStringLiteral("ctrHSep"));
        return s;
    };
    sdLay->addWidget(makeHSep(m_sysDetail));

    // Nome + excluir sistema
    auto* nameRow = new QHBoxLayout();
    nameRow->setSpacing(6);
    m_systemNameEdit = new QLineEdit(m_sysDetail);
    m_systemNameEdit->setObjectName(QStringLiteral("ctrSysName"));
    m_systemNameEdit->setPlaceholderText(tr("Nome do sistema"));
    nameRow->addWidget(m_systemNameEdit, 1);
    m_deleteSystemBtn = new QPushButton(QStringLiteral("✕"), m_sysDetail);
    m_deleteSystemBtn->setObjectName(QStringLiteral("ctrDeleteSys"));
    m_deleteSystemBtn->setCursor(Qt::PointingHandCursor);
    m_deleteSystemBtn->setToolTip(tr("Excluir sistema"));
    m_deleteSystemBtn->setFixedSize(26, 26);
    nameRow->addWidget(m_deleteSystemBtn);
    sdLay->addLayout(nameRow);

    m_categoryLabel = new QLabel(m_sysDetail);
    m_categoryLabel->setObjectName(QStringLiteral("ctrCatBadge"));
    sdLay->addWidget(m_categoryLabel);

    // Tag de Território(s) — só aparece se o Criador de Mundos injetar uma
    // TerritorioStore (setTerritorioStore()). Sem isso, sistema é sempre
    // global (comportamento anterior, inalterado).
    m_territoryBtn = new QPushButton(m_sysDetail);
    m_territoryBtn->setObjectName(QStringLiteral("ctrCatBadge"));
    m_territoryBtn->setCursor(Qt::PointingHandCursor);
    m_territoryBtn->setFlat(true);
    m_territoryBtn->setVisible(false);
    sdLay->addWidget(m_territoryBtn);

    sdLay->addWidget(makeHSep(m_sysDetail));

    // Slider
    auto* sliderRow = new QHBoxLayout();
    sliderRow->setSpacing(4);
    m_waypointFirst = new QLabel(m_sysDetail);
    m_waypointFirst->setObjectName(QStringLiteral("ctrWaypointEdge"));
    sliderRow->addWidget(m_waypointFirst);
    m_slider = new QSlider(Qt::Horizontal, m_sysDetail);
    m_slider->setObjectName(QStringLiteral("ctrSlider"));
    m_slider->setSingleStep(1);
    m_slider->setPageStep(1);
    sliderRow->addWidget(m_slider, 1);
    m_waypointLast = new QLabel(m_sysDetail);
    m_waypointLast->setObjectName(QStringLiteral("ctrWaypointEdge"));
    sliderRow->addWidget(m_waypointLast);
    sdLay->addLayout(sliderRow);

    m_waypointName = new QLabel(m_sysDetail);
    m_waypointName->setObjectName(QStringLiteral("ctrWaypointName"));
    m_waypointName->setAlignment(Qt::AlignCenter);
    sdLay->addWidget(m_waypointName);

    m_waypointTip = new QLabel(m_sysDetail);
    m_waypointTip->setObjectName(QStringLiteral("ctrWaypointTip"));
    m_waypointTip->setAlignment(Qt::AlignCenter);
    m_waypointTip->setWordWrap(true);
    sdLay->addWidget(m_waypointTip);

    // Favorece / Exige — balança de trade-offs do waypoint atual
    auto* tradeoffHeader = new QHBoxLayout();
    tradeoffHeader->setSpacing(4);
    auto* favorsTitle = new QLabel(tr("FAVORECE"), m_sysDetail);
    favorsTitle->setObjectName(QStringLiteral("ctrFavorsTitle"));
    tradeoffHeader->addWidget(favorsTitle);
    tradeoffHeader->addStretch();
    m_tradeoffExpandBtn = new QPushButton(QStringLiteral("?"), m_sysDetail);
    m_tradeoffExpandBtn->setObjectName(QStringLiteral("ctrTradeoffExpand"));
    m_tradeoffExpandBtn->setCursor(Qt::PointingHandCursor);
    m_tradeoffExpandBtn->setCheckable(true);
    m_tradeoffExpandBtn->setFixedSize(18, 18);
    m_tradeoffExpandBtn->setToolTip(tr("Mostrar todos os pontos"));
    tradeoffHeader->addWidget(m_tradeoffExpandBtn);
    tradeoffHeader->addStretch();
    auto* demandsTitle = new QLabel(tr("EXIGE"), m_sysDetail);
    demandsTitle->setObjectName(QStringLiteral("ctrDemandsTitle"));
    tradeoffHeader->addWidget(demandsTitle);
    sdLay->addLayout(tradeoffHeader);

    auto* tradeoffRow = new QHBoxLayout();
    tradeoffRow->setSpacing(10);
    m_favorsList = new QLabel(m_sysDetail);
    m_favorsList->setObjectName(QStringLiteral("ctrFavorsList"));
    m_favorsList->setWordWrap(true);
    m_favorsList->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    tradeoffRow->addWidget(m_favorsList, 1);
    m_demandsList = new QLabel(m_sysDetail);
    m_demandsList->setObjectName(QStringLiteral("ctrDemandsList"));
    m_demandsList->setWordWrap(true);
    m_demandsList->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    tradeoffRow->addWidget(m_demandsList, 1);
    sdLay->addLayout(tradeoffRow);

    connect(m_tradeoffExpandBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_tradeoffExpanded = checked;
        updateSliderDisplay(m_slider->value());
    });

    sdLay->addWidget(makeHSep(m_sysDetail));

    // Botões de nós
    auto* nodeBtns = new QHBoxLayout();
    nodeBtns->setSpacing(4);
    m_addRuleBtn = new QPushButton(tr("+ Regra"), m_sysDetail);
    m_addRuleBtn->setObjectName(QStringLiteral("ctrAddRule"));
    m_addRuleBtn->setCursor(Qt::PointingHandCursor);
    m_addRuleBtn->setToolTip(tr("Adicionar regra (mecânica/lei)"));
    nodeBtns->addWidget(m_addRuleBtn);
    m_addSectionBtn = new QPushButton(tr("+ Seção"), m_sysDetail);
    m_addSectionBtn->setObjectName(QStringLiteral("ctrAddSection"));
    m_addSectionBtn->setCursor(Qt::PointingHandCursor);
    m_addSectionBtn->setToolTip(tr("Adicionar seção (informação/lore)"));
    nodeBtns->addWidget(m_addSectionBtn);
    nodeBtns->addStretch();
    m_deleteNodeBtn = new QPushButton(QStringLiteral("✕"), m_sysDetail);
    m_deleteNodeBtn->setObjectName(QStringLiteral("ctrDeleteNode"));
    m_deleteNodeBtn->setCursor(Qt::PointingHandCursor);
    m_deleteNodeBtn->setEnabled(false);
    m_deleteNodeBtn->setToolTip(tr("Excluir nó selecionado"));
    m_deleteNodeBtn->setFixedSize(26, 26);
    nodeBtns->addWidget(m_deleteNodeBtn);
    sdLay->addLayout(nodeBtns);

    leftLay->addWidget(m_sysDetail);

    // Árvore de nós (ocupa o resto do painel)
    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("ctrTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_tree->setVisible(false);
    leftLay->addWidget(m_tree, 1);

    // ── Seção "Menções no projeto" — overlay flutuante no canto superior
    // direito DESTE widget (não entra no layout principal), toggle "@" no
    // cabeçalho acima.
    m_mentionsPanel = new QWidget(this);
    m_mentionsPanel->setObjectName(QStringLiteral("ctrMentionsPanel"));
    m_mentionsPanel->setAttribute(Qt::WA_StyledBackground, true);
    m_mentionsPanel->hide();
    auto* mentionsPanelLay = new QVBoxLayout(m_mentionsPanel);
    mentionsPanelLay->setContentsMargins(14, 10, 14, 10);
    mentionsPanelLay->setSpacing(6);

    auto* mentionsHeaderRow = new QHBoxLayout();
    m_mentionsTitleLabel = new QLabel(m_mentionsPanel);
    m_mentionsTitleLabel->setObjectName(QStringLiteral("ctrMentionsTitle"));
    mentionsHeaderRow->addWidget(m_mentionsTitleLabel, 1);
    m_mentionsCloseBtn = new QToolButton(m_mentionsPanel);
    m_mentionsCloseBtn->setObjectName(QStringLiteral("ctrMentionsCloseBtn"));
    m_mentionsCloseBtn->setText(QStringLiteral("×"));
    m_mentionsCloseBtn->setCursor(Qt::PointingHandCursor);
    connect(m_mentionsCloseBtn, &QToolButton::clicked, this, [this]() {
        if (m_mentionsToggleBtn) m_mentionsToggleBtn->setChecked(false);
    });
    mentionsHeaderRow->addWidget(m_mentionsCloseBtn, 0, Qt::AlignTop);
    mentionsPanelLay->addLayout(mentionsHeaderRow);

    m_mentionsScroll = new QScrollArea(m_mentionsPanel);
    m_mentionsScroll->setObjectName(QStringLiteral("ctrMentionsScroll"));
    m_mentionsScroll->setFrameShape(QFrame::NoFrame);
    m_mentionsScroll->setWidgetResizable(true);
    m_mentionsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_mentionsColumn = new QWidget(m_mentionsScroll);
    m_mentionsLay = new QVBoxLayout(m_mentionsColumn);
    m_mentionsLay->setContentsMargins(0, 0, 0, 0);
    m_mentionsLay->setSpacing(6);
    m_mentionsScroll->setWidget(m_mentionsColumn);
    mentionsPanelLay->addWidget(m_mentionsScroll, 1);

    // ── Conexões ─────────────────────────────────────────────────────────────
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &ConstrutorWindow::onSearchTextChanged);
    connect(m_searchResultsList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        const QString systemId = item->data(Qt::UserRole).toString();
        const QString nodeId   = item->data(Qt::UserRole + 1).toString();
        m_searchEdit->blockSignals(true);
        m_searchEdit->clear();
        m_searchEdit->blockSignals(false);
        m_searchResultsList->setVisible(false);
        m_searchResultsList->clear();
        m_systemsList->setVisible(true);
        selectSystemAndNode(systemId, nodeId);
        if (!m_currentSystemId.isEmpty()) {
            m_sysDetail->setVisible(true);
            m_tree->setVisible(true);
        }
    });

    connect(m_systemsList, &QListWidget::currentRowChanged,
            this, &ConstrutorWindow::onSystemSelected);
    connect(m_newSystemBtn,    &QPushButton::clicked, this, &ConstrutorWindow::onNewSystem);
    connect(m_deleteSystemBtn, &QPushButton::clicked, this, &ConstrutorWindow::onDeleteSystem);
    connect(m_systemNameEdit, &QLineEdit::editingFinished,
            this, [this]() { onSystemNameEdited(m_systemNameEdit->text()); });
    connect(m_slider, &QSlider::valueChanged, this, &ConstrutorWindow::onSliderChanged);

    connect(m_addRuleBtn,    &QPushButton::clicked, this, &ConstrutorWindow::onAddRule);
    connect(m_addSectionBtn, &QPushButton::clicked, this, &ConstrutorWindow::onAddSection);
    connect(m_deleteNodeBtn, &QPushButton::clicked, this, &ConstrutorWindow::onDeleteNode);
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &ConstrutorWindow::onTreeSelectionChanged);
    connect(m_tree, &QTreeWidget::itemChanged,
            this, &ConstrutorWindow::onTreeItemChanged);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &ConstrutorWindow::onTreeContextMenu);

    if (m_editor)
        connect(m_editor, &WorldContentEditor::contentChanged,
                this, &ConstrutorWindow::onEditorContentChanged);
}

// ── Lista de sistemas ──────────────────────────────────────────────────────────

void ConstrutorWindow::rebuildSystemsList()
{
    if (!m_store) return;

    const QString currentId = selectedSystemId();
    m_rebuilding = true;
    m_systemsList->blockSignals(true);
    m_systemsList->clear();

    for (const auto& sys : m_store->systems()) {
        // Filtro por Território (Criador de Mundos): vazio = mostra tudo;
        // sistema sem tag nenhuma = global, sempre aparece; sistema tagueado
        // só aparece pro(s) território(s) marcado(s).
        if (!m_territoryFilter.isEmpty() && !sys.territoryIds.isEmpty()
            && !sys.territoryIds.contains(m_territoryFilter))
            continue;

        auto* item = new QListWidgetItem(sys.name, m_systemsList);
        item->setData(Qt::UserRole, sys.id);

        // Subtexto: "Categoria | Waypoint"
        const ConstrutorStore::Category* cat = ConstrutorStore::categoryById(sys.categoryId);
        QString sub;
        if (cat) {
            sub = cat->displayName;
            if (!cat->waypoints.isEmpty()) {
                const int idx = qBound(0, sys.sliderIndex, cat->waypoints.size() - 1);
                sub += QStringLiteral(" | ") + cat->waypoints[idx].label;
            }
        }
        item->setData(Qt::UserRole + 2, sub);

        if (sys.id == currentId)
            m_systemsList->setCurrentItem(item);
    }

    m_systemsList->blockSignals(false);
    m_rebuilding = false;

    if (m_systemsList->currentItem() == nullptr && !m_currentSystemId.isEmpty()) {
        m_currentSystemId.clear();
        m_currentNodeId.clear();
        m_sysDetail->setVisible(false);
        m_tree->setVisible(false);
        showNoSystemOpenState();
    }
}

void ConstrutorWindow::setTerritoryFilter(const QString& territorioId)
{
    if (m_territoryFilter == territorioId) return;
    m_territoryFilter = territorioId;
    rebuildSystemsList();
}

// ── Carrega sistema selecionado ───────────────────────────────────────────────

void ConstrutorWindow::onSystemSelected()
{
    if (m_rebuilding) return;

    const QString id = selectedSystemId();
    if (id.isEmpty()) {
        if (m_editorActive) saveCurrentNodeContent();
        m_currentSystemId.clear();
        m_currentNodeId.clear();
        m_sysDetail->setVisible(false);
        m_tree->setVisible(false);
        showNoSystemOpenState();
        return;
    }
    if (id == m_currentSystemId) return;
    if (m_editorActive) saveCurrentNodeContent();
    loadSystem(id);
}

void ConstrutorWindow::loadSystem(const QString& id)
{
    const ConstrutorStore::System* sys = m_store ? m_store->system(id) : nullptr;
    if (!sys) return;

    claimEditor();

    m_currentSystemId = id;
    m_currentNodeId.clear();
    m_sysDetail->setVisible(true);
    m_tree->setVisible(true);

    // Nome
    m_systemNameEdit->blockSignals(true);
    m_systemNameEdit->setText(sys->name);
    m_systemNameEdit->blockSignals(false);

    // Categoria
    const ConstrutorStore::Category* cat = ConstrutorStore::categoryById(sys->categoryId);
    m_categoryLabel->setText(cat ? cat->displayName : sys->categoryId);

    rebuildTerritoryMenu();

    // Slider
    if (cat && !cat->waypoints.isEmpty()) {
        m_slider->blockSignals(true);
        m_slider->setRange(0, cat->waypoints.size() - 1);
        m_slider->setTickInterval(1);
        m_slider->setValue(qBound(0, sys->sliderIndex, cat->waypoints.size() - 1));
        m_waypointFirst->setText(cat->waypoints.first().label);
        m_waypointLast->setText(cat->waypoints.last().label);
        m_slider->blockSignals(false);
        updateSliderDisplay(m_slider->value());
    }

    // Árvore
    rebuildTree();

    // Editor — ainda sem nó selecionado: mostra o resumo/parecer do sistema.
    if (m_editor) {
        m_editor->setPlaceholderText(
            tr("Escreva um resumo, parecer ou introdução deste sistema…"));
        m_editor->setContent(sys->content);
        m_editor->setStatusText(sys->updatedAt > 0
            ? tr("Editado em %1").arg(QDateTime::fromMSecsSinceEpoch(sys->updatedAt)
                  .toString(QStringLiteral("dd/MM/yyyy HH:mm")))
            : QString());
    }
    m_deleteNodeBtn->setEnabled(false);
    rebuildMentionsPanel();
}

// ── Árvore de nós ─────────────────────────────────────────────────────────────

void ConstrutorWindow::rebuildTree()
{
    const ConstrutorStore::System* sys =
        (m_store && !m_currentSystemId.isEmpty())
            ? m_store->system(m_currentSystemId)
            : nullptr;
    if (!sys) return;

    m_rebuilding = true;
    m_tree->blockSignals(true);
    m_tree->clear();

    for (const auto& node : sys->nodes)
        populateTreeNode(nullptr, node);

    m_tree->expandAll();

    // Restaura seleção — ainda com m_rebuilding=true (igual rebuildSystemsList):
    // setCurrentItem dispara itemSelectionChanged mesmo fora de blockSignals,
    // e sem essa guarda o reseletor reaciona onTreeSelectionChanged, que
    // recarrega o conteúdo do nó (setHtml + moveCursor(Start)) no meio da
    // digitação, resetando o cursor pro início do documento.
    if (!m_currentNodeId.isEmpty()) {
        const auto items = m_tree->findItems(QString(), Qt::MatchContains | Qt::MatchRecursive);
        for (auto* item : items) {
            if (item->data(0, kNodeIdRole).toString() == m_currentNodeId) {
                m_tree->setCurrentItem(item);
                break;
            }
        }
    }

    m_tree->blockSignals(false);
    m_rebuilding = false;
}

void ConstrutorWindow::populateTreeNode(QTreeWidgetItem* parent,
                                         const ConstrutorStore::Node& node)
{
    QTreeWidgetItem* item;
    if (parent)
        item = new QTreeWidgetItem(parent);
    else
        item = new QTreeWidgetItem(m_tree);

    const bool isRule = (node.type == ConstrutorStore::NodeType::Rule);
    item->setText(0, (isRule ? QStringLiteral("📐 ") : QStringLiteral("📄 ")) + node.name);
    item->setData(0, kNodeIdRole,   node.id);
    item->setData(0, kNodeTypeRole, static_cast<int>(node.type));
    item->setFlags(item->flags() | Qt::ItemIsEditable);

    for (const auto& child : node.children)
        populateTreeNode(item, child);
}

void ConstrutorWindow::showNoSystemOpenState()
{
    if (m_editor && m_editorActive) {
        m_editor->setEditorEnabled(false);
        m_editor->setPlaceholderText(
            tr("Nenhum sistema aberto. Selecione um sistema à esquerda ou crie um novo para começar."));
        m_editor->setStatusText(QString());
    }
    rebuildMentionsPanel();
}

void ConstrutorWindow::rebuildMentionsPanel()
{
    if (!m_mentionsPanel) return;

    // takeAt(0) tira o item do layout na hora (deleteLater só adia a
    // destruição do widget em si) — senão os cards antigos ficam visíveis
    // sobrepostos aos novos até o próximo tick do event loop.
    while (QLayoutItem* item = m_mentionsLay->takeAt(0)) {
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }

    const ConstrutorStore::System* sys =
        (m_store && !m_currentSystemId.isEmpty()) ? m_store->system(m_currentSystemId) : nullptr;
    if (!sys) {
        m_mentionsTitleLabel->setText(tr("Menções no projeto"));
        auto* empty = new QLabel(tr("Abra um sistema para ver as menções."), m_mentionsColumn);
        empty->setObjectName(QStringLiteral("ctrMentionsEmpty"));
        empty->setWordWrap(true);
        m_mentionsLay->addWidget(empty);
        m_mentionsLay->addStretch();
        return;
    }

    QList<ConstrutorStore::Mention> shown;
    for (const auto& m : sys->mentions) {
        const bool matches = m_currentNodeId.isEmpty()
            ? m.nodeId.isEmpty()
            : (m.nodeId == m_currentNodeId);
        if (matches) shown.append(m);
    }

    m_mentionsTitleLabel->setText(tr("Menções no projeto (%1)").arg(shown.size()));
    if (shown.isEmpty()) {
        auto* empty = new QLabel(tr("Nenhuma menção salva ainda para este item."), m_mentionsColumn);
        empty->setObjectName(QStringLiteral("ctrMentionsEmpty"));
        empty->setWordWrap(true);
        m_mentionsLay->addWidget(empty);
    } else {
        for (const auto& m : shown)
            m_mentionsLay->addWidget(buildMentionCard(m, m_mentionsColumn));
    }
    m_mentionsLay->addStretch();
}

QWidget* ConstrutorWindow::buildMentionCard(const ConstrutorStore::Mention& mention, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("ctrMentionCard"));
    card->setCursor(Qt::PointingHandCursor);
    card->setProperty("mentionId", mention.id);
    card->installEventFilter(this);

    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(8, 6, 8, 6);
    lay->setSpacing(3);

    auto* topRow = new QHBoxLayout();
    auto* sourceLbl = new QLabel(mention.sourceLabel, card);
    sourceLbl->setObjectName(QStringLiteral("ctrMentionCardSource"));
    sourceLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    topRow->addWidget(sourceLbl, 1);

    auto* delBtn = new QToolButton(card);
    delBtn->setObjectName(QStringLiteral("ctrMentionDelete"));
    delBtn->setText(QStringLiteral("×"));
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setToolTip(tr("Excluir menção"));
    const QString systemId = m_currentSystemId;
    const QString mentionId = mention.id;
    connect(delBtn, &QToolButton::clicked, this, [this, systemId, mentionId]() {
        if (m_store) m_store->removeMention(systemId, mentionId);
    });
    topRow->addWidget(delBtn, 0, Qt::AlignTop);
    lay->addLayout(topRow);

    auto* textLbl = new QLabel(QStringLiteral("“%1”").arg(mention.text.trimmed()), card);
    textLbl->setObjectName(QStringLiteral("ctrMentionCardQuote"));
    textLbl->setWordWrap(true);
    textLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    lay->addWidget(textLbl);

    return card;
}

void ConstrutorWindow::anchorMentionsPanel()
{
    if (!m_mentionsPanel) return;
    constexpr int kMentionsPanelWidth = 280;
    constexpr int kMargin = 10;
    const int h = qMax(200, height() - kMargin * 2);
    m_mentionsPanel->resize(kMentionsPanelWidth, h);
    m_mentionsPanel->move(width() - m_mentionsPanel->width() - kMargin, kMargin);
    m_mentionsPanel->raise();
}

void ConstrutorWindow::onTreeSelectionChanged()
{
    if (m_rebuilding) return;
    if (m_editorActive) saveCurrentNodeContent();

    const QString nodeId = selectedNodeId();
    m_currentNodeId = nodeId;

    const ConstrutorStore::System* sys = m_store ? m_store->system(m_currentSystemId) : nullptr;

    if (nodeId.isEmpty()) {
        // Sem nó selecionado: volta pro resumo/parecer geral do sistema.
        m_deleteNodeBtn->setEnabled(false);
        claimEditor();
        if (!sys) {
            if (m_editor) { m_editor->setEditorEnabled(false); m_editor->setStatusText(QString()); }
            rebuildMentionsPanel();
            return;
        }
        if (m_editor) {
            m_editor->setPlaceholderText(
                tr("Escreva um resumo, parecer ou introdução deste sistema…"));
            m_editor->setContent(sys->content);
            m_editor->setStatusText(sys->updatedAt > 0
                ? tr("Editado em %1").arg(QDateTime::fromMSecsSinceEpoch(sys->updatedAt)
                      .toString(QStringLiteral("dd/MM/yyyy HH:mm")))
                : QString());
        }
        rebuildMentionsPanel();
        return;
    }

    m_deleteNodeBtn->setEnabled(true);
    if (!sys) return;

    std::function<const ConstrutorStore::Node*(const QList<ConstrutorStore::Node>&, const QString&)>
        findConst = [&](const QList<ConstrutorStore::Node>& nodes,
                        const QString& id) -> const ConstrutorStore::Node* {
        for (const auto& n : nodes) {
            if (n.id == id) return &n;
            const auto* c = findConst(n.children, id);
            if (c) return c;
        }
        return nullptr;
    };

    const ConstrutorStore::Node* node = findConst(sys->nodes, nodeId);
    if (!node) return;

    claimEditor();
    if (m_editor) {
        m_editor->setPlaceholderText(tr("Escreva aqui…"));
        m_editor->setContent(node->content);
        m_editor->setStatusText(node->updatedAt > 0
            ? tr("Editado em %1").arg(QDateTime::fromMSecsSinceEpoch(node->updatedAt)
                  .toString(QStringLiteral("dd/MM/yyyy HH:mm")))
            : QString());
    }
    rebuildMentionsPanel();
}

void ConstrutorWindow::onTreeItemChanged(QTreeWidgetItem* item, int column)
{
    if (m_rebuilding || !item || column != 0) return;
    if (!m_store) return;

    // Remove o prefixo de ícone para extrair apenas o nome
    QString text = item->text(0);
    if (text.startsWith(QStringLiteral("📐 ")) || text.startsWith(QStringLiteral("📄 ")))
        text = text.mid(3);

    const QString nodeId = item->data(0, kNodeIdRole).toString();
    if (nodeId.isEmpty() || text.isEmpty()) {
        rebuildTree();
        return;
    }

    // Busca conteúdo atual para preservar
    const ConstrutorStore::System* sys = m_store->system(m_currentSystemId);
    if (!sys) return;
    std::function<const ConstrutorStore::Node*(const QList<ConstrutorStore::Node>&)>
        findConst = [&](const QList<ConstrutorStore::Node>& nodes)
            -> const ConstrutorStore::Node* {
        for (const auto& n : nodes) {
            if (n.id == nodeId) return &n;
            const auto* c = findConst(n.children);
            if (c) return c;
        }
        return nullptr;
    };
    const ConstrutorStore::Node* node = findConst(sys->nodes);
    const QString content = node ? node->content : QString();

    m_store->updateNode(m_currentSystemId, nodeId, text, content);
}

// ── Slider ────────────────────────────────────────────────────────────────────

void ConstrutorWindow::updateSliderDisplay(int index)
{
    const ConstrutorStore::System* sys = m_store ? m_store->system(m_currentSystemId) : nullptr;
    if (!sys) return;
    const ConstrutorStore::Category* cat = ConstrutorStore::categoryById(sys->categoryId);
    if (!cat || cat->waypoints.isEmpty()) return;

    const int i = qBound(0, index, cat->waypoints.size() - 1);
    const ConstrutorStore::CategoryWaypoint& wp = cat->waypoints[i];
    m_waypointName->setText(wp.label);
    m_waypointTip->setText(wp.tooltip);

    const int shown = m_tradeoffExpanded ? 10 : 3;
    auto bulletText = [shown](const QStringList& items) {
        QString html;
        for (int j = 0; j < items.size() && j < shown; ++j)
            html += QStringLiteral("<p style='margin:0 0 8px 0;'>• %1</p>").arg(items[j].toHtmlEscaped());
        return html;
    };
    if (m_favorsList)  m_favorsList->setText(bulletText(wp.favors));
    if (m_demandsList) m_demandsList->setText(bulletText(wp.demands));
    if (m_tradeoffExpandBtn) m_tradeoffExpandBtn->setText(m_tradeoffExpanded ? QStringLiteral("−") : QStringLiteral("?"));
}

void ConstrutorWindow::onSliderChanged(int index)
{
    updateSliderDisplay(index);
    if (!m_store || m_currentSystemId.isEmpty()) return;
    const ConstrutorStore::System* sys = m_store->system(m_currentSystemId);
    if (!sys) return;
    m_store->updateSystem(m_currentSystemId, sys->name, index);
}

// ── Edição de nome do sistema ─────────────────────────────────────────────────

void ConstrutorWindow::onSystemNameEdited(const QString& name)
{
    if (!m_store || m_currentSystemId.isEmpty() || name.trimmed().isEmpty()) return;
    const ConstrutorStore::System* sys = m_store->system(m_currentSystemId);
    if (!sys || sys->name == name.trimmed()) return;
    m_store->updateSystem(m_currentSystemId, name.trimmed(), sys->sliderIndex);

    // Atualiza item na lista
    const auto items = m_systemsList->findItems(QString(), Qt::MatchContains);
    for (auto* item : items) {
        if (item->data(Qt::UserRole).toString() == m_currentSystemId) {
            m_rebuilding = true;
            item->setText(name.trimmed());
            m_rebuilding = false;
            break;
        }
    }
}

// ── Conteúdo do nó ────────────────────────────────────────────────────────────

void ConstrutorWindow::onEditorContentChanged()
{
    if (!m_editorActive) return;
    saveCurrentNodeContent();
}

void ConstrutorWindow::saveCurrentNodeContent()
{
    if (!m_store || m_currentSystemId.isEmpty() || !m_editor) return;

    const ConstrutorStore::System* sys = m_store->system(m_currentSystemId);
    if (!sys) return;

    const QString newContent = m_editor->content();

    if (m_currentNodeId.isEmpty()) {
        // Sem nó selecionado: o texto pertence ao resumo do sistema.
        if (sys->content == newContent) return;
        m_store->updateSystemContent(m_currentSystemId, newContent);
        return;
    }

    std::function<const ConstrutorStore::Node*(const QList<ConstrutorStore::Node>&)>
        findConst = [&](const QList<ConstrutorStore::Node>& nodes)
            -> const ConstrutorStore::Node* {
        for (const auto& n : nodes) {
            if (n.id == m_currentNodeId) return &n;
            const auto* c = findConst(n.children);
            if (c) return c;
        }
        return nullptr;
    };

    const ConstrutorStore::Node* node = findConst(sys->nodes);
    if (!node) return;

    if (node->content == newContent) return;
    m_store->updateNode(m_currentSystemId, m_currentNodeId, node->name, newContent);
}

// ── Criação de sistema ────────────────────────────────────────────────────────

void ConstrutorWindow::onNewSystem()
{
    if (!m_store) return;

    // Diálogo único: nome + categoria na mesma caixa
    const QList<ConstrutorStore::Category>& cats = ConstrutorStore::categories();

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Novo sistema"));

    auto* form = new QFormLayout();
    auto* nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(tr("Nome do sistema"));
    form->addRow(tr("Nome:"), nameEdit);

    auto* catCombo = new QComboBox(&dlg);
    for (const auto& c : cats)
        catCombo->addItem(c.displayName, c.id);
    form->addRow(tr("Categoria:"), catCombo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QPushButton* okBtn = buttons->button(QDialogButtonBox::Ok);
    okBtn->setEnabled(false);
    connect(nameEdit, &QLineEdit::textChanged, &dlg, [okBtn](const QString& t) {
        okBtn->setEnabled(!t.trimmed().isEmpty());
    });
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* layout = new QVBoxLayout(&dlg);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return;

    const QString name = nameEdit->text().trimmed();
    const QString categoryId = catCombo->currentData().toString();
    if (name.isEmpty()) return;

    const QString id = m_store->addSystem(name, categoryId, 0);

    // Seleciona o novo sistema na lista
    for (int i = 0; i < m_systemsList->count(); ++i) {
        if (m_systemsList->item(i)->data(Qt::UserRole).toString() == id) {
            m_systemsList->setCurrentRow(i);
            break;
        }
    }
}

void ConstrutorWindow::onDeleteSystem()
{
    if (!m_store || m_currentSystemId.isEmpty()) return;
    const ConstrutorStore::System* sys = m_store->system(m_currentSystemId);
    if (!sys) return;

    const auto r = QMessageBox::question(
        this, tr("Excluir sistema"),
        tr("Excluir o sistema \"%1\" e todos os seus nós?\n\nEssa ação não pode ser desfeita.")
            .arg(sys->name),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (r != QMessageBox::Yes) return;

    m_currentSystemId.clear();
    m_currentNodeId.clear();
    m_sysDetail->setVisible(false);
    m_tree->setVisible(false);
    showNoSystemOpenState();
    m_store->removeSystem(sys->id);
}

// ── Criação de nós ────────────────────────────────────────────────────────────

void ConstrutorWindow::onAddRule()    { onAddChild(ConstrutorStore::NodeType::Rule); }
void ConstrutorWindow::onAddSection() { onAddChild(ConstrutorStore::NodeType::Section); }

void ConstrutorWindow::onAddChild(ConstrutorStore::NodeType type)
{
    if (!m_store || m_currentSystemId.isEmpty()) return;

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, type == ConstrutorStore::NodeType::Rule ? tr("Nova regra") : tr("Nova seção"),
        tr("Nome:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    // Se houver nó selecionado, adiciona como filho; senão, como raiz
    const QString parentId = selectedNodeId();
    const QString newId = m_store->addNode(m_currentSystemId, parentId, type, name.trimmed());

    // Seleciona o nó recém-criado
    m_currentNodeId = newId;
    rebuildTree();
}

void ConstrutorWindow::onDeleteNode()
{
    if (!m_store || m_currentSystemId.isEmpty() || m_currentNodeId.isEmpty()) return;
    const auto r = QMessageBox::question(
        this, tr("Excluir nó"),
        tr("Excluir este nó e todos os seus filhos?\n\nEssa ação não pode ser desfeita."),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (r != QMessageBox::Yes) return;

    const QString nodeId = m_currentNodeId;
    m_currentNodeId.clear();
    m_deleteNodeBtn->setEnabled(false);

    // Volta pro resumo do sistema, já que nenhum nó fica selecionado.
    const ConstrutorStore::System* sys = m_store->system(m_currentSystemId);
    if (m_editor) {
        if (sys) {
            m_editor->setPlaceholderText(
                tr("Escreva um resumo, parecer ou introdução deste sistema…"));
            m_editor->setContent(sys->content);
        } else {
            m_editor->setEditorEnabled(false);
        }
    }
    // removeNode() já dispara ConstrutorStore::changed() → onStoreChanged()
    // → rebuildMentionsPanel() (agora filtrando por nível-sistema, já que
    // m_currentNodeId foi limpo acima) — sem precisar chamar de novo aqui.
    m_store->removeNode(m_currentSystemId, nodeId);
}

// ── Menu de contexto da árvore ────────────────────────────────────────────────

void ConstrutorWindow::onTreeContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (!item) return;

    const QString nodeId = item->data(0, kNodeIdRole).toString();
    if (nodeId.isEmpty()) return;

    QMenu menu(this);
    menu.addAction(tr("Adicionar Regra filha"), this,
                   [this]() { onAddChild(ConstrutorStore::NodeType::Rule); });
    menu.addAction(tr("Adicionar Seção filha"), this,
                   [this]() { onAddChild(ConstrutorStore::NodeType::Section); });
    menu.addSeparator();
    menu.addAction(tr("Excluir"), this, &ConstrutorWindow::onDeleteNode);
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

// ── Store changed ─────────────────────────────────────────────────────────────

void ConstrutorWindow::onStoreChanged()
{
    rebuildSystemsList();
    if (!m_currentSystemId.isEmpty()) {
        const ConstrutorStore::System* sys = m_store ? m_store->system(m_currentSystemId) : nullptr;
        if (!sys) {
            m_currentSystemId.clear();
            m_sysDetail->setVisible(false);
            m_tree->setVisible(false);
            if (m_territoryBtn) m_territoryBtn->setVisible(false);
        } else {
            rebuildTree();
            rebuildTerritoryMenu();
        }
    }
    rebuildMentionsPanel();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString ConstrutorWindow::selectedSystemId() const
{
    const QListWidgetItem* item = m_systemsList ? m_systemsList->currentItem() : nullptr;
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QString ConstrutorWindow::selectedNodeId() const
{
    const QTreeWidgetItem* item = m_tree ? m_tree->currentItem() : nullptr;
    return item ? item->data(0, kNodeIdRole).toString() : QString();
}

// ── Busca global entre sistemas e nós ─────────────────────────────────────────

void ConstrutorWindow::onSearchTextChanged(const QString& text)
{
    const QString needle = text.trimmed();
    if (needle.isEmpty()) {
        m_searchResultsList->setVisible(false);
        m_searchResultsList->clear();
        m_systemsList->setVisible(true);
        m_sysDetail->setVisible(!m_currentSystemId.isEmpty());
        m_tree->setVisible(!m_currentSystemId.isEmpty());
        return;
    }
    if (!m_store) return;

    m_systemsList->setVisible(false);
    m_sysDetail->setVisible(false);
    m_tree->setVisible(false);
    m_searchResultsList->clear();
    m_searchResultsList->setVisible(true);

    for (const auto& sys : m_store->systems()) {
        if (sys.name.contains(needle, Qt::CaseInsensitive)) {
            auto* item = new QListWidgetItem(sys.name, m_searchResultsList);
            item->setData(Qt::UserRole, sys.id);
            item->setData(Qt::UserRole + 1, QString());
        }
        collectNodeMatches(sys.id, sys.name, sys.nodes, needle, m_searchResultsList);
    }
}

void ConstrutorWindow::openNode(const QString& systemId, const QString& nodeId)
{
    if (m_searchEdit) m_searchEdit->clear(); // restaura painel normal (onSearchTextChanged)
    selectSystemAndNode(systemId, nodeId);
    if (!m_currentSystemId.isEmpty()) {
        m_sysDetail->setVisible(true);
        m_tree->setVisible(true);
    }
}

void ConstrutorWindow::selectSystemAndNode(const QString& systemId, const QString& nodeId)
{
    for (int i = 0; i < m_systemsList->count(); ++i) {
        if (m_systemsList->item(i)->data(Qt::UserRole).toString() == systemId) {
            m_systemsList->setCurrentRow(i);
            break;
        }
    }
    if (nodeId.isEmpty()) return;
    const auto items = m_tree->findItems(QString(), Qt::MatchContains | Qt::MatchRecursive);
    for (auto* item : items) {
        if (item->data(0, kNodeIdRole).toString() == nodeId) {
            m_tree->setCurrentItem(item);
            break;
        }
    }
}
