#include "GlossaryAddPopup.h"

#include "Theme.h"

#include <QApplication>
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
#include <QTextEdit>
#include <QVBoxLayout>

GlossaryAddPopup::GlossaryAddPopup(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("glsAddPopup"));
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
            this, &GlossaryAddPopup::applyTheme);

    hide();
    qApp->installEventFilter(this);
}

void GlossaryAddPopup::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 12);
    root->setSpacing(8);

    m_header = new QLabel(tr("Adicionar ao Glossário"), this);
    m_header->setObjectName(QStringLiteral("glsAddHeader"));
    root->addWidget(m_header);

    auto* termLabel = new QLabel(tr("Termo"), this);
    termLabel->setObjectName(QStringLiteral("glsAddFieldLabel"));
    root->addWidget(termLabel);
    m_termEdit = new QLineEdit(this);
    m_termEdit->setObjectName(QStringLiteral("glsAddTerm"));
    root->addWidget(m_termEdit);

    auto* defLabel = new QLabel(tr("Definição (opcional)"), this);
    defLabel->setObjectName(QStringLiteral("glsAddFieldLabel"));
    root->addWidget(defLabel);
    m_defEdit = new QTextEdit(this);
    m_defEdit->setObjectName(QStringLiteral("glsAddDef"));
    m_defEdit->setAcceptRichText(false);
    m_defEdit->setFixedHeight(80);
    root->addWidget(m_defEdit);

    auto* row = new QHBoxLayout();
    row->addStretch(1);
    m_cancelBtn = new QPushButton(tr("Cancelar"), this);
    m_cancelBtn->setObjectName(QStringLiteral("glsAddCancel"));
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        hide();
        emit cancelled();
    });
    row->addWidget(m_cancelBtn);

    m_okBtn = new QPushButton(tr("Adicionar"), this);
    m_okBtn->setObjectName(QStringLiteral("glsAddOk"));
    m_okBtn->setDefault(true);
    m_okBtn->setCursor(Qt::PointingHandCursor);
    connect(m_okBtn, &QPushButton::clicked, this, &GlossaryAddPopup::emitConfirm);
    row->addWidget(m_okBtn);

    root->addLayout(row);

    setFixedWidth(360);
}

void GlossaryAddPopup::applyTheme()
{
    setStyleSheet(QStringLiteral(
        "QFrame#glsAddPopup {"
        "  background: %1; border: 1px solid %2; border-radius: 10px;"
        "}"
        "QLabel#glsAddHeader { color: %3; font-size: 13px; font-weight: 600; }"
        "QLabel#glsAddFieldLabel { color: %4; font-size: 11px; }"
        "QLineEdit, QTextEdit {"
        "  background: %5; color: %3;"
        "  border: 1px solid %2; border-radius: 4px;"
        "  padding: 4px; font-size: 12px;"
        "}"
        "QPushButton {"
        "  background: transparent; color: %3;"
        "  border: 1px solid %2; border-radius: 6px;"
        "  padding: 4px 12px; font-size: 11px;"
        "}"
        "QPushButton:hover { background: %6; }"
        "QPushButton#glsAddOk { color: #7BC592; border-color: #807BC592; }"
        "QPushButton#glsAddOk:hover { background: rgba(120,200,140,0.18); }"
    ).arg(Theme::panelBackground(),
          Theme::panelBorder(),
          Theme::textPrimary(),
          Theme::textMuted(),
          Theme::editorBackground(),
          Theme::hoverOverlay()));
}

void GlossaryAddPopup::presentAt(const QPoint& globalAnchor, const QString& seedTerm)
{
    if (m_termEdit) {
        m_termEdit->setText(seedTerm.trimmed());
        m_termEdit->setFocus();
        m_termEdit->selectAll();
    }
    if (m_defEdit) m_defEdit->clear();

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

void GlossaryAddPopup::emitConfirm()
{
    const QString term = m_termEdit ? m_termEdit->text().trimmed() : QString();
    if (term.isEmpty()) {
        if (m_termEdit) m_termEdit->setFocus();
        return;
    }
    const QString def = m_defEdit ? m_defEdit->toPlainText() : QString();
    hide();
    emit confirmed(term, def);
}

bool GlossaryAddPopup::eventFilter(QObject* /*watched*/, QEvent* event)
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
        QWidget* clicked = QApplication::widgetAt(gp);
        QWidget* w = clicked;
        while (w) {
            if (w == this) return false;
            w = w->parentWidget();
        }
        hide();
        emit cancelled();
    }
    return false;
}
