#include "GlossaryPanel.h"

#include "GlossaryStore.h"
#include "Theme.h"

#include <QApplication>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kPanelWidth = 540;
constexpr int kPanelHeight = 420;
constexpr int kGapBelowAnchor = 6;
constexpr int kSearchMinTerms = 3;
}

GlossaryPanel::GlossaryPanel(GlossaryStore* store, QWidget* parent)
    : QFrame(parent)
    , m_store(store)
{
    setObjectName(QStringLiteral("glossaryPanel"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setFocusPolicy(Qt::ClickFocus);

    buildUi();
    applyTheme();

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(22);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);

    if (m_store) {
        connect(m_store, &GlossaryStore::changed,
                this, &GlossaryPanel::onStoreChanged);
    }
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &GlossaryPanel::applyTheme);

    rebuildList();
    updateRightPane();

    hide();
    qApp->installEventFilter(this);
}

void GlossaryPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(8);

    auto* topRow = new QHBoxLayout();
    m_header = new QLabel(tr("Glossário"), this);
    m_header->setObjectName(QStringLiteral("glsHeader"));
    topRow->addWidget(m_header);
    topRow->addStretch(1);
    m_addBtn = new QPushButton(tr("+ Novo termo"), this);
    m_addBtn->setObjectName(QStringLiteral("glsAddBtn"));
    m_addBtn->setCursor(Qt::PointingHandCursor);
    connect(m_addBtn, &QPushButton::clicked, this, &GlossaryPanel::onAddClicked);
    topRow->addWidget(m_addBtn);
    root->addLayout(topRow);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("glsSearch"));
    m_search->setPlaceholderText(tr("Buscar termo..."));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, this, &GlossaryPanel::onSearchTextChanged);
    root->addWidget(m_search);

    auto* split = new QHBoxLayout();
    split->setSpacing(10);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("glsList"));
    m_list->setFixedWidth(190);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(m_list, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem*, QListWidgetItem*) { onItemSelected(); });
    split->addWidget(m_list);

    auto* right = new QVBoxLayout();
    right->setSpacing(6);
    auto* termLabel = new QLabel(tr("Termo"), this);
    termLabel->setObjectName(QStringLiteral("glsFieldLabel"));
    right->addWidget(termLabel);
    m_termEdit = new QLineEdit(this);
    m_termEdit->setObjectName(QStringLiteral("glsTermEdit"));
    connect(m_termEdit, &QLineEdit::editingFinished, this, &GlossaryPanel::onTermEdited);
    right->addWidget(m_termEdit);

    auto* defLabel = new QLabel(tr("Definição"), this);
    defLabel->setObjectName(QStringLiteral("glsFieldLabel"));
    right->addWidget(defLabel);
    m_defEdit = new QTextEdit(this);
    m_defEdit->setObjectName(QStringLiteral("glsDefEdit"));
    m_defEdit->setAcceptRichText(false);
    m_defEdit->setPlaceholderText(tr("Definição opcional..."));
    // Salva ao perder foco (sem flush a cada tecla).
    m_defEdit->installEventFilter(this);
    right->addWidget(m_defEdit, 1);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    m_removeBtn = new QPushButton(tr("Remover"), this);
    m_removeBtn->setObjectName(QStringLiteral("glsRemoveBtn"));
    m_removeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_removeBtn, &QPushButton::clicked, this, &GlossaryPanel::onRemoveClicked);
    btnRow->addWidget(m_removeBtn);
    right->addLayout(btnRow);

    split->addLayout(right, 1);
    root->addLayout(split, 1);

    setFixedSize(kPanelWidth, kPanelHeight);
}

void GlossaryPanel::applyTheme()
{
    setStyleSheet(QStringLiteral(
        "QFrame#glossaryPanel {"
        "  background: %1; border: 1px solid %2; border-radius: 10px;"
        "}"
        "QLabel#glsHeader { color: %3; font-size: 14px; font-weight: 600; }"
        "QLabel#glsFieldLabel { color: %4; font-size: 11px; }"
        "QPushButton#glsAddBtn, QPushButton#glsRemoveBtn {"
        "  background: transparent; color: %3;"
        "  border: 1px solid %2; border-radius: 6px;"
        "  padding: 4px 10px; font-size: 11px;"
        "}"
        "QPushButton#glsAddBtn:hover, QPushButton#glsRemoveBtn:hover { background: %5; }"
        "QPushButton#glsRemoveBtn { color: #E57373; border-color: #80E57373; }"
        "QLineEdit, QTextEdit {"
        "  background: %6; color: %3;"
        "  border: 1px solid %2; border-radius: 4px;"
        "  padding: 4px; font-size: 12px;"
        "}"
        "QListWidget#glsList {"
        "  background: %6; color: %3;"
        "  border: 1px solid %2; border-radius: 6px; padding: 4px; outline: 0;"
        "}"
        "QListWidget#glsList::item { padding: 6px 8px; border-radius: 4px; }"
        "QListWidget#glsList::item:hover { background: %5; }"
        "QListWidget#glsList::item:selected { background: %7; color: %3; }"
    ).arg(Theme::panelBackground(),
          Theme::panelBorder(),
          Theme::textPrimary(),
          Theme::textMuted(),
          Theme::hoverOverlay(),
          Theme::editorBackground(),
          Theme::pressedOverlay()));
}

void GlossaryPanel::rebuildList()
{
    if (!m_list || !m_store) return;
    m_syncing = true;
    m_list->clear();

    const QString needle = m_search ? m_search->text().trimmed().toLower() : QString();
    const bool useFilter = needle.size() >= kSearchMinTerms;

    for (const GlossaryStore::Entry& e : m_store->entries()) {
        if (useFilter && !e.term.toLower().contains(needle)) continue;
        auto* item = new QListWidgetItem(e.term, m_list);
        item->setData(Qt::UserRole, e.id);
        if (!e.definition.isEmpty()) item->setToolTip(e.definition);
    }
    selectId(m_selectedId);
    m_syncing = false;
    if (!m_list->currentItem() && m_list->count() > 0) m_list->setCurrentRow(0);
}

void GlossaryPanel::selectId(const QString& id)
{
    if (!m_list) return;
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toString() == id) {
            m_list->setCurrentRow(i);
            return;
        }
    }
}

void GlossaryPanel::onStoreChanged()
{
    rebuildList();
    updateRightPane();
}

void GlossaryPanel::onSearchTextChanged(const QString& /*text*/)
{
    rebuildList();
}

void GlossaryPanel::onItemSelected()
{
    if (m_syncing) return;
    auto* item = m_list ? m_list->currentItem() : nullptr;
    m_selectedId = item ? item->data(Qt::UserRole).toString() : QString();
    updateRightPane();
}

void GlossaryPanel::updateRightPane()
{
    const bool hasSel = !m_selectedId.isEmpty() && m_store;
    GlossaryStore::Entry e = hasSel ? m_store->findById(m_selectedId) : GlossaryStore::Entry{};

    m_syncing = true;
    if (m_termEdit) m_termEdit->setText(e.term);
    if (m_defEdit) m_defEdit->setPlainText(e.definition);
    m_syncing = false;

    if (m_termEdit) m_termEdit->setEnabled(hasSel);
    if (m_defEdit) m_defEdit->setEnabled(hasSel);
    if (m_removeBtn) m_removeBtn->setEnabled(hasSel);
}

void GlossaryPanel::onTermEdited()
{
    if (m_syncing || m_selectedId.isEmpty() || !m_store || !m_termEdit) return;
    const QString next = m_termEdit->text().trimmed();
    GlossaryStore::Entry e = m_store->findById(m_selectedId);
    if (next.isEmpty() || next == e.term) return;
    m_store->update(m_selectedId, next, e.definition);
}

void GlossaryPanel::onDefinitionEdited()
{
    if (m_syncing || m_selectedId.isEmpty() || !m_store || !m_defEdit) return;
    GlossaryStore::Entry e = m_store->findById(m_selectedId);
    const QString next = m_defEdit->toPlainText();
    if (next == e.definition) return;
    m_store->update(m_selectedId, e.term, next);
}

void GlossaryPanel::onRemoveClicked()
{
    if (m_selectedId.isEmpty() || !m_store) return;
    m_store->remove(m_selectedId);
    m_selectedId.clear();
}

void GlossaryPanel::onAddClicked()
{
    if (!m_store) return;
    const QString id = m_store->add(tr("Novo termo"), QString());
    m_selectedId = id;
    rebuildList();
    selectId(id);
    updateRightPane();
    if (m_termEdit) {
        m_termEdit->setFocus();
        m_termEdit->selectAll();
    }
}

void GlossaryPanel::showNear(const QRect& anchorGlobal)
{
    adjustSize();
    QPoint pos(anchorGlobal.left(), anchorGlobal.bottom() + kGapBelowAnchor);
    const QScreen* screen = QGuiApplication::screenAt(anchorGlobal.center());
    if (screen) {
        const QRect avail = screen->availableGeometry();
        if (pos.x() + width() > avail.right()) pos.setX(anchorGlobal.right() - width());
        if (pos.x() < avail.left()) pos.setX(avail.left() + 4);
        if (pos.y() + height() > avail.bottom()) {
            pos.setY(anchorGlobal.top() - height() - kGapBelowAnchor);
        }
    }
    move(pos);
    show();
    raise();
    activateWindow();
}

bool GlossaryPanel::eventFilter(QObject* watched, QEvent* event)
{
    // Salva definição quando perde foco do textarea.
    if (watched == m_defEdit && event->type() == QEvent::FocusOut) {
        onDefinitionEdited();
        return false;
    }
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
        // Salva definição antes de esconder (caso o user clicou fora ainda digitando).
        onDefinitionEdited();
        hide();
    }
    return false;
}
