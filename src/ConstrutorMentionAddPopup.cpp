#include "ConstrutorMentionAddPopup.h"

#include "Theme.h"

#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

#include <functional>

ConstrutorMentionAddPopup::ConstrutorMentionAddPopup(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("ctrMentionAddPopup"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    setFocusPolicy(Qt::ClickFocus);

    buildUi();
    applyTheme();

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 3);
    setGraphicsEffect(shadow);

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &ConstrutorMentionAddPopup::applyTheme);

    hide();
    qApp->installEventFilter(this);
}

void ConstrutorMentionAddPopup::setConstrutorStore(ConstrutorStore* store)
{
    m_store = store;
}

void ConstrutorMentionAddPopup::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 12);
    root->setSpacing(8);

    m_header = new QLabel(tr("Salvar como menção ao sistema"), this);
    m_header->setObjectName(QStringLiteral("ctrMentAddHeader"));
    root->addWidget(m_header);

    m_sourceLabel = new QLabel(this);
    m_sourceLabel->setObjectName(QStringLiteral("ctrMentAddSource"));
    m_sourceLabel->setVisible(false);
    root->addWidget(m_sourceLabel);

    m_preview = new QLabel(this);
    m_preview->setObjectName(QStringLiteral("ctrMentAddPreview"));
    m_preview->setWordWrap(true);
    root->addWidget(m_preview);

    auto* sysLabel = new QLabel(tr("Sistema"), this);
    sysLabel->setObjectName(QStringLiteral("ctrMentAddFieldLabel"));
    root->addWidget(sysLabel);
    m_systemCombo = new QComboBox(this);
    m_systemCombo->setObjectName(QStringLiteral("ctrMentAddCombo"));
    connect(m_systemCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        rebuildNodeCombo();
        refreshOkEnabled();
    });
    root->addWidget(m_systemCombo);

    auto* nodeLabel = new QLabel(tr("Regra/Seção (opcional)"), this);
    nodeLabel->setObjectName(QStringLiteral("ctrMentAddFieldLabel"));
    root->addWidget(nodeLabel);
    m_nodeCombo = new QComboBox(this);
    m_nodeCombo->setObjectName(QStringLiteral("ctrMentAddCombo"));
    root->addWidget(m_nodeCombo);

    auto* row = new QHBoxLayout();
    row->addStretch(1);
    m_cancelBtn = new QPushButton(tr("Cancelar"), this);
    m_cancelBtn->setObjectName(QStringLiteral("ctrMentAddCancel"));
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        hide();
        emit cancelled();
    });
    row->addWidget(m_cancelBtn);

    m_okBtn = new QPushButton(tr("Salvar"), this);
    m_okBtn->setObjectName(QStringLiteral("ctrMentAddOk"));
    m_okBtn->setDefault(true);
    m_okBtn->setCursor(Qt::PointingHandCursor);
    connect(m_okBtn, &QPushButton::clicked, this, &ConstrutorMentionAddPopup::emitConfirm);
    row->addWidget(m_okBtn);

    root->addLayout(row);

    setFixedWidth(340);
}

void ConstrutorMentionAddPopup::applyTheme()
{
    setStyleSheet(QStringLiteral(
        "QFrame#ctrMentionAddPopup {"
        "  background: %1; border: 1px solid %2; border-radius: 10px;"
        "}"
        "QLabel#ctrMentAddHeader { color: %3; font-size: 13px; font-weight: 600; }"
        "QLabel#ctrMentAddSource { color: %7; font-size: 11px; font-style: italic; }"
        "QLabel#ctrMentAddPreview {"
        "  color: %4; font-size: 12px;"
        "  background: %5; border: 1px solid %2; border-radius: 6px; padding: 6px 8px;"
        "}"
        "QLabel#ctrMentAddFieldLabel { color: %4; font-size: 11px; }"
        "QComboBox {"
        "  background: %5; color: %3;"
        "  border: 1px solid %2; border-radius: 4px;"
        "  padding: 4px 6px; font-size: 12px;"
        "}"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView {"
        "  background: %1; color: %3; border: 1px solid %2;"
        "  selection-background-color: %6; selection-color: %3;"
        "}"
        "QPushButton {"
        "  background: transparent; color: %3;"
        "  border: 1px solid %2; border-radius: 6px;"
        "  padding: 4px 12px; font-size: 11px;"
        "}"
        "QPushButton:hover { background: %6; }"
        "QPushButton:disabled { color: %4; border-color: %2; }"
        "QPushButton#ctrMentAddOk { color: %3; border-color: %8; }"
        "QPushButton#ctrMentAddOk:hover { background: %6; }"
    ).arg(Theme::panelBackground(),   // 1
          Theme::panelBorder(),       // 2
          Theme::textPrimary(),       // 3
          Theme::textMuted(),         // 4
          Theme::editorBackground(),  // 5
          Theme::hoverOverlay(),      // 6
          Theme::textBright(),        // 7 (source label)
          Theme::accentDefault()));   // 8
}

void ConstrutorMentionAddPopup::rebuildNodeCombo()
{
    if (!m_nodeCombo) return;
    m_nodeCombo->clear();
    m_nodeCombo->addItem(tr("(sistema inteiro, sem nó específico)"), QString());

    const QString systemId = m_systemCombo ? m_systemCombo->currentData().toString() : QString();
    if (!m_store || systemId.isEmpty()) return;
    const ConstrutorStore::System* sys = m_store->system(systemId);
    if (!sys) return;

    // Mesmo algoritmo de achatamento com breadcrumb já usado em
    // MentionPopup::rebuildConstrutorSystemNodes — nós têm profundidade
    // arbitrária, não dá pra fazer um drill-down fixo tipo Capítulos→Cenas.
    std::function<void(const QString&, const QList<ConstrutorStore::Node>&)> walk;
    walk = [&](const QString& breadcrumb, const QList<ConstrutorStore::Node>& nodes) {
        for (const auto& n : nodes) {
            const QString path = breadcrumb.isEmpty()
                ? n.name : breadcrumb + QStringLiteral(" ▸ ") + n.name;
            m_nodeCombo->addItem(path, n.id);
            walk(path, n.children);
        }
    };
    walk(QString(), sys->nodes);
}

void ConstrutorMentionAddPopup::refreshOkEnabled()
{
    if (!m_okBtn) return;
    const bool hasSystem = m_systemCombo && !m_systemCombo->currentData().toString().isEmpty();
    m_okBtn->setEnabled(hasSystem);
}

void ConstrutorMentionAddPopup::presentAt(const QPoint& globalAnchor,
                                          const QString& selectedText,
                                          const QString& sourceLabel,
                                          const QVector<QPair<QString, QString>>& systems)
{
    if (m_sourceLabel) {
        if (sourceLabel.isEmpty()) {
            m_sourceLabel->setVisible(false);
        } else {
            m_sourceLabel->setText(tr("De: %1").arg(sourceLabel));
            m_sourceLabel->setVisible(true);
        }
    }
    if (m_preview) {
        QString clean = selectedText;
        clean.replace(QChar(0x2029), QChar('\n'));
        clean = clean.trimmed();
        if (clean.size() > 160) clean = clean.left(160) + QStringLiteral("…");
        m_preview->setText(QStringLiteral("“%1”").arg(clean));
    }
    if (m_systemCombo) {
        m_systemCombo->clear();
        if (systems.isEmpty()) {
            m_systemCombo->addItem(tr("(nenhum sistema criado no Construtor)"), QString());
        } else {
            for (const auto& s : systems) m_systemCombo->addItem(s.second, s.first);
        }
    }
    rebuildNodeCombo();
    refreshOkEnabled();

    adjustSize();
    QPoint pos = globalAnchor;
    const QScreen* screen = QGuiApplication::screenAt(pos);
    if (screen) {
        const QRect avail = screen->availableGeometry();
        if (pos.x() + width() > avail.right()) pos.setX(avail.right() - width() - 4);
        if (pos.y() + height() > avail.bottom()) pos.setY(avail.bottom() - height() - 4);
        if (pos.x() < avail.left()) pos.setX(avail.left() + 4);
        if (pos.y() < avail.top()) pos.setY(avail.top() + 4);
    }
    move(pos);
    show();
    raise();
    activateWindow();
}

void ConstrutorMentionAddPopup::emitConfirm()
{
    if (!m_okBtn || !m_okBtn->isEnabled()) return;
    const QString systemId = m_systemCombo ? m_systemCombo->currentData().toString() : QString();
    if (systemId.isEmpty()) return;
    const QString nodeId = m_nodeCombo ? m_nodeCombo->currentData().toString() : QString();
    hide();
    emit confirmed(systemId, nodeId);
}

bool ConstrutorMentionAddPopup::eventFilter(QObject* /*watched*/, QEvent* event)
{
    if (!isVisible()) return false;
    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) {
            hide();
            emit cancelled();
            return true;
        }
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
            && (ke->modifiers() & Qt::ControlModifier)) {
            emitConfirm();
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        const QPoint gp = me->globalPosition().toPoint();
        if (frameGeometry().contains(gp)) return false;
        QWidget* w = QApplication::widgetAt(gp);
        while (w) {
            if (w == this) return false;
            w = w->parentWidget();
        }
        hide();
        emit cancelled();
    }
    return false;
}
