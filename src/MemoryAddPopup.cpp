#include "MemoryAddPopup.h"

#include "Theme.h"

#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

MemoryAddPopup::MemoryAddPopup(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("memAddPopup"));
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
            this, &MemoryAddPopup::applyTheme);

    hide();
    qApp->installEventFilter(this);
}

void MemoryAddPopup::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 12);
    root->setSpacing(8);

    m_header = new QLabel(tr("Adicionar à memória"), this);
    m_header->setObjectName(QStringLiteral("memAddHeader"));
    root->addWidget(m_header);

    m_sourceLabel = new QLabel(this);
    m_sourceLabel->setObjectName(QStringLiteral("memAddSource"));
    m_sourceLabel->setVisible(false);
    root->addWidget(m_sourceLabel);

    m_preview = new QLabel(this);
    m_preview->setObjectName(QStringLiteral("memAddPreview"));
    m_preview->setWordWrap(true);
    root->addWidget(m_preview);

    auto* nameLabel = new QLabel(tr("Nome (opcional)"), this);
    nameLabel->setObjectName(QStringLiteral("memAddFieldLabel"));
    root->addWidget(nameLabel);
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName(QStringLiteral("memAddName"));
    m_nameEdit->setPlaceholderText(tr("Ex: Klara rouba o carro"));
    root->addWidget(m_nameEdit);

    auto* destLabel = new QLabel(tr("Destino"), this);
    destLabel->setObjectName(QStringLiteral("memAddFieldLabel"));
    root->addWidget(destLabel);
    m_targetCombo = new QComboBox(this);
    m_targetCombo->setObjectName(QStringLiteral("memAddCombo"));
    m_targetCombo->addItem(tr("Projeto"), QStringLiteral("project"));
    m_targetCombo->addItem(tr("Personagem"), QStringLiteral("character"));
    connect(m_targetCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        refreshCharVisibility();
        refreshOkEnabled();
    });
    root->addWidget(m_targetCombo);

    m_charLabel = new QLabel(tr("Personagem"), this);
    m_charLabel->setObjectName(QStringLiteral("memAddFieldLabel"));
    root->addWidget(m_charLabel);
    m_charCombo = new QComboBox(this);
    m_charCombo->setObjectName(QStringLiteral("memAddCombo"));
    connect(m_charCombo, &QComboBox::currentIndexChanged, this, [this](int) { refreshOkEnabled(); });
    root->addWidget(m_charCombo);

    auto* row = new QHBoxLayout();
    row->addStretch(1);
    m_cancelBtn = new QPushButton(tr("Cancelar"), this);
    m_cancelBtn->setObjectName(QStringLiteral("memAddCancel"));
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        hide();
        emit cancelled();
    });
    row->addWidget(m_cancelBtn);

    m_okBtn = new QPushButton(tr("Salvar"), this);
    m_okBtn->setObjectName(QStringLiteral("memAddOk"));
    m_okBtn->setDefault(true);
    m_okBtn->setCursor(Qt::PointingHandCursor);
    connect(m_okBtn, &QPushButton::clicked, this, &MemoryAddPopup::emitConfirm);
    row->addWidget(m_okBtn);

    root->addLayout(row);

    setFixedWidth(340);
}

void MemoryAddPopup::applyTheme()
{
    setStyleSheet(QStringLiteral(
        "QFrame#memAddPopup {"
        "  background: %1; border: 1px solid %2; border-radius: 10px;"
        "}"
        "QLabel#memAddHeader { color: %3; font-size: 13px; font-weight: 600; }"
        "QLabel#memAddSource { color: %7; font-size: 11px; font-style: italic; }"
        "QLabel#memAddPreview {"
        "  color: %4; font-size: 12px;"
        "  background: %5; border: 1px solid %2; border-radius: 6px; padding: 6px 8px;"
        "}"
        "QLabel#memAddFieldLabel { color: %4; font-size: 11px; }"
        "QLineEdit, QComboBox {"
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
        "QPushButton#memAddOk { color: %3; border-color: %8; }"
        "QPushButton#memAddOk:hover { background: %6; }"
    ).arg(Theme::panelBackground(),   // 1
          Theme::panelBorder(),       // 2
          Theme::textPrimary(),       // 3
          Theme::textMuted(),         // 4
          Theme::editorBackground(),  // 5
          Theme::hoverOverlay(),      // 6
          Theme::textBright(),        // 7 (source label)
          Theme::accentDefault()));   // 8
}

void MemoryAddPopup::refreshCharVisibility()
{
    const bool isChar = m_targetCombo
        && m_targetCombo->currentData().toString() == QStringLiteral("character");
    if (m_charLabel) m_charLabel->setVisible(isChar);
    if (m_charCombo) m_charCombo->setVisible(isChar);
    adjustSize();
}

void MemoryAddPopup::refreshOkEnabled()
{
    if (!m_okBtn) return;
    const bool isChar = m_targetCombo
        && m_targetCombo->currentData().toString() == QStringLiteral("character");
    const bool charOk = !isChar || (m_charCombo && !m_charCombo->currentData().toString().isEmpty());
    m_okBtn->setEnabled(charOk);
}

void MemoryAddPopup::presentAt(const QPoint& globalAnchor,
                               const QString& selectedText,
                               const QString& sourceLabel,
                               const QVector<QPair<QString, QString>>& characters)
{
    if (m_nameEdit) {
        m_nameEdit->clear();
        m_nameEdit->setFocus();
    }
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
    if (m_charCombo) {
        m_charCombo->clear();
        if (characters.isEmpty()) {
            m_charCombo->addItem(tr("(nenhum personagem cadastrado)"), QString());
        } else {
            for (const auto& c : characters) m_charCombo->addItem(c.second, c.first);
        }
    }
    if (m_targetCombo) {
        // Sem personagens, trava no Projeto.
        m_targetCombo->setCurrentIndex(0);
        auto* model = m_targetCombo->model();
        Q_UNUSED(model);
    }
    refreshCharVisibility();
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

void MemoryAddPopup::emitConfirm()
{
    if (!m_okBtn || !m_okBtn->isEnabled()) return;
    const QString name = m_nameEdit ? m_nameEdit->text().trimmed() : QString();
    const QString targetType = m_targetCombo
        ? m_targetCombo->currentData().toString() : QStringLiteral("project");
    QString elementId;
    if (targetType == QStringLiteral("character") && m_charCombo)
        elementId = m_charCombo->currentData().toString();
    hide();
    emit confirmed(name, targetType, elementId);
}

bool MemoryAddPopup::eventFilter(QObject* /*watched*/, QEvent* event)
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
