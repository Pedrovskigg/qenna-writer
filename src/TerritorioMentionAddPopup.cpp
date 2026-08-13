#include "TerritorioMentionAddPopup.h"

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

TerritorioMentionAddPopup::TerritorioMentionAddPopup(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("lugMentionAddPopup"));
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
            this, &TerritorioMentionAddPopup::applyTheme);

    hide();
    qApp->installEventFilter(this);
}

void TerritorioMentionAddPopup::setTerritorioStore(TerritorioStore* store)
{
    m_store = store;
}

void TerritorioMentionAddPopup::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 12);
    root->setSpacing(8);

    m_header = new QLabel(tr("Salvar como menção ao Território"), this);
    m_header->setObjectName(QStringLiteral("lugMentAddHeader"));
    root->addWidget(m_header);

    m_sourceLabel = new QLabel(this);
    m_sourceLabel->setObjectName(QStringLiteral("lugMentAddSource"));
    m_sourceLabel->setVisible(false);
    root->addWidget(m_sourceLabel);

    m_preview = new QLabel(this);
    m_preview->setObjectName(QStringLiteral("lugMentAddPreview"));
    m_preview->setWordWrap(true);
    root->addWidget(m_preview);

    auto* terLabel = new QLabel(tr("Território"), this);
    terLabel->setObjectName(QStringLiteral("lugMentAddFieldLabel"));
    root->addWidget(terLabel);
    m_territorioCombo = new QComboBox(this);
    m_territorioCombo->setObjectName(QStringLiteral("lugMentAddCombo"));
    connect(m_territorioCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        rebuildNodeCombo();
        refreshOkEnabled();
    });
    root->addWidget(m_territorioCombo);

    auto* nodeLabel = new QLabel(tr("Pasta/Documento (opcional)"), this);
    nodeLabel->setObjectName(QStringLiteral("lugMentAddFieldLabel"));
    root->addWidget(nodeLabel);
    m_nodeCombo = new QComboBox(this);
    m_nodeCombo->setObjectName(QStringLiteral("lugMentAddCombo"));
    root->addWidget(m_nodeCombo);

    auto* catLabel = new QLabel(tr("Categoria (opcional)"), this);
    catLabel->setObjectName(QStringLiteral("lugMentAddFieldLabel"));
    root->addWidget(catLabel);
    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->setObjectName(QStringLiteral("lugMentAddCombo"));
    m_categoryCombo->setEditable(true);
    m_categoryCombo->addItem(QString());
    m_categoryCombo->addItems({ tr("Guerra"), tr("Batalha"), tr("Criatura"), tr("Aliança"),
                                tr("Evento histórico"), tr("Outro") });
    m_categoryCombo->setCurrentIndex(0);
    root->addWidget(m_categoryCombo);

    auto* row = new QHBoxLayout();
    row->addStretch(1);
    m_cancelBtn = new QPushButton(tr("Cancelar"), this);
    m_cancelBtn->setObjectName(QStringLiteral("lugMentAddCancel"));
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        hide();
        emit cancelled();
    });
    row->addWidget(m_cancelBtn);

    m_okBtn = new QPushButton(tr("Salvar"), this);
    m_okBtn->setObjectName(QStringLiteral("lugMentAddOk"));
    m_okBtn->setDefault(true);
    m_okBtn->setCursor(Qt::PointingHandCursor);
    connect(m_okBtn, &QPushButton::clicked, this, &TerritorioMentionAddPopup::emitConfirm);
    row->addWidget(m_okBtn);

    root->addLayout(row);

    setFixedWidth(340);
}

void TerritorioMentionAddPopup::applyTheme()
{
    setStyleSheet(QStringLiteral(
        "QFrame#lugMentionAddPopup {"
        "  background: %1; border: 1px solid %2; border-radius: 10px;"
        "}"
        "QLabel#lugMentAddHeader { color: %3; font-size: 13px; font-weight: 600; }"
        "QLabel#lugMentAddSource { color: %7; font-size: 11px; font-style: italic; }"
        "QLabel#lugMentAddPreview {"
        "  color: %4; font-size: 12px;"
        "  background: %5; border: 1px solid %2; border-radius: 6px; padding: 6px 8px;"
        "}"
        "QLabel#lugMentAddFieldLabel { color: %4; font-size: 11px; }"
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
        "QPushButton#lugMentAddOk { color: %3; border-color: %8; }"
        "QPushButton#lugMentAddOk:hover { background: %6; }"
    ).arg(Theme::panelBackground(),   // 1
          Theme::panelBorder(),       // 2
          Theme::textPrimary(),       // 3
          Theme::textMuted(),         // 4
          Theme::editorBackground(),  // 5
          Theme::hoverOverlay(),      // 6
          Theme::textBright(),        // 7 (source label)
          Theme::accentDefault()));   // 8
}

void TerritorioMentionAddPopup::rebuildNodeCombo()
{
    if (!m_nodeCombo) return;
    m_nodeCombo->clear();
    m_nodeCombo->addItem(tr("(território inteiro, sem item específico)"), QString());

    const QString territorioId = m_territorioCombo ? m_territorioCombo->currentData().toString() : QString();
    if (!m_store || territorioId.isEmpty()) return;
    const TerritorioStore::Territorio* ter = m_store->territorio(territorioId);
    if (!ter) return;

    std::function<void(const QString&, const QList<TerritorioStore::Node>&)> walk;
    walk = [&](const QString& breadcrumb, const QList<TerritorioStore::Node>& nodes) {
        for (const auto& n : nodes) {
            const QString path = breadcrumb.isEmpty()
                ? n.name : breadcrumb + QStringLiteral(" ▸ ") + n.name;
            m_nodeCombo->addItem(path, n.id);
            walk(path, n.children);
        }
    };
    walk(QString(), ter->nodes);
}

void TerritorioMentionAddPopup::refreshOkEnabled()
{
    if (!m_okBtn) return;
    const bool hasTerritorio = m_territorioCombo && !m_territorioCombo->currentData().toString().isEmpty();
    m_okBtn->setEnabled(hasTerritorio);
}

void TerritorioMentionAddPopup::presentAt(const QPoint& globalAnchor,
                                          const QString& selectedText,
                                          const QString& sourceLabel,
                                          const QVector<QPair<QString, QString>>& territorios)
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
    if (m_territorioCombo) {
        m_territorioCombo->clear();
        if (territorios.isEmpty()) {
            m_territorioCombo->addItem(tr("(nenhum território criado ainda)"), QString());
        } else {
            for (const auto& t : territorios) m_territorioCombo->addItem(t.second, t.first);
        }
    }
    if (m_categoryCombo) m_categoryCombo->setCurrentIndex(0);
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

void TerritorioMentionAddPopup::emitConfirm()
{
    if (!m_okBtn || !m_okBtn->isEnabled()) return;
    const QString territorioId = m_territorioCombo ? m_territorioCombo->currentData().toString() : QString();
    if (territorioId.isEmpty()) return;
    const QString nodeId = m_nodeCombo ? m_nodeCombo->currentData().toString() : QString();
    const QString category = m_categoryCombo ? m_categoryCombo->currentText().trimmed() : QString();
    hide();
    emit confirmed(territorioId, nodeId, category);
}

bool TerritorioMentionAddPopup::eventFilter(QObject* /*watched*/, QEvent* event)
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
