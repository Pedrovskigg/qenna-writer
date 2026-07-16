#include "PresencePopup.h"

#include "Theme.h"

#include <QByteArray>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

PresencePopup::PresencePopup(ElementsStore* store, QWidget* parent)
    : QFrame(parent)
    , m_store(store)
{
    setObjectName(QStringLiteral("presencePopup"));
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    buildUi();
    hide();
    connect(m_store, &ElementsStore::changed, this, [this]() {
        if (isVisible()) updateContent();
    });
}

void PresencePopup::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(14, 12, 14, 12);
    outer->setSpacing(8);

    // Linha superior: foto + nome
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(10);

    m_photoLabel = new QLabel(this);
    m_photoLabel->setObjectName(QStringLiteral("ppPhoto"));
    m_photoLabel->setFixedSize(40, 40);
    m_photoLabel->setAlignment(Qt::AlignCenter);
    topRow->addWidget(m_photoLabel);

    auto* textCol = new QVBoxLayout();
    textCol->setSpacing(2);
    m_captionLabel = new QLabel(tr("Personagem detectado"), this);
    m_captionLabel->setObjectName(QStringLiteral("ppCaption"));
    m_nameLabel = new QLabel(this);
    m_nameLabel->setObjectName(QStringLiteral("ppName"));
    textCol->addWidget(m_captionLabel);
    textCol->addWidget(m_nameLabel);
    textCol->addStretch();
    topRow->addLayout(textCol);
    topRow->addStretch();
    outer->addLayout(topRow);

    // Botões de ação
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);

    m_markBtn   = new QPushButton(tr("Marcar"),  this);
    m_alwaysBtn = new QPushButton(tr("Sempre"),  this);
    m_ignoreBtn = new QPushButton(tr("Ignorar"), this);
    m_neverBtn  = new QPushButton(tr("Nunca"),   this);

    for (auto* b : { m_markBtn, m_alwaysBtn, m_ignoreBtn, m_neverBtn }) {
        b->setObjectName(QStringLiteral("ppBtn"));
        b->setCursor(Qt::PointingHandCursor);
        btnRow->addWidget(b);
    }
    outer->addLayout(btnRow);

    connect(m_markBtn, &QPushButton::clicked, this, [this]() {
        if (m_current < 0 || m_current >= m_queue.size()) return;
        const auto& det = m_queue[m_current];
        emit markRequested(det.elementId, det.docKey);
        advance();
    });
    connect(m_alwaysBtn, &QPushButton::clicked, this, [this]() {
        if (m_current < 0 || m_current >= m_queue.size()) return;
        const auto& det = m_queue[m_current];
        emit markRequested(det.elementId, det.docKey);  // marca o atual também
        emit markAllActivated();                         // ativa markAll global
        // Descarta a fila — tudo será auto-marcado daqui em diante
        m_queue.clear();
        hide();
        m_current = -1;
    });
    connect(m_ignoreBtn, &QPushButton::clicked, this, [this]() {
        if (m_current < 0 || m_current >= m_queue.size()) return;
        const auto& det = m_queue[m_current];
        emit ignoredNow(det.elementId, det.docKey);
        advance();
    });
    connect(m_neverBtn, &QPushButton::clicked, this, [this]() {
        if (m_current < 0 || m_current >= m_queue.size()) return;
        const auto& det = m_queue[m_current];
        emit neverRequested(det.elementId, det.docKey);
        advance();
    });

    applyTheme();
}

void PresencePopup::applyTheme()
{
    setStyleSheet(QStringLiteral(R"(
        QFrame#presencePopup {
            background: %1;
            border: 1px solid %2;
            border-radius: 8px;
        }
        QLabel#ppCaption { color: %3; font-size: 10px; }
        QLabel#ppName    { color: %4; font-size: 13px; font-weight: 600; }
        QLabel#ppPhoto   {
            background: %5; border: 1px solid %2;
            border-radius: 4px; color: %3; font-size: 9px;
        }
        QPushButton#ppBtn {
            background: %6; color: %4;
            border: 1px solid %2; border-radius: 4px;
            padding: 4px 10px; font-size: 11px;
        }
        QPushButton#ppBtn:hover { background: %7; }
    )").arg(Theme::panelBackground(),
            Theme::panelBorder(),
            Theme::textMuted(),
            Theme::textPrimary(),
            Theme::inputBackground(),
            Theme::hoverOverlay(),
            Theme::hoverStrong()));
}

void PresencePopup::enqueue(const QList<CharacterDetection>& detections)
{
    const bool wasIdle = m_queue.isEmpty() || m_current < 0;
    for (const auto& det : detections) {
        // Não enfileira duplicatas
        bool already = false;
        for (const auto& q : m_queue) {
            if (q.elementId == det.elementId && q.docKey == det.docKey) { already = true; break; }
        }
        if (!already) m_queue.append(det);
    }
    if (wasIdle) showNext();
}

void PresencePopup::showNext()
{
    if (m_queue.isEmpty()) { hide(); m_current = -1; return; }
    m_current = 0;
    updateContent();
    show();
    raise();
}

void PresencePopup::advance()
{
    m_queue.removeFirst();
    m_current = 0;
    if (m_queue.isEmpty()) { hide(); m_current = -1; return; }
    updateContent();
}

void PresencePopup::updateContent()
{
    if (m_current < 0 || m_current >= m_queue.size()) return;
    const CharacterDetection& det = m_queue[m_current];
    const Element* elem = m_store->findElement(det.elementId);

    const QString name = elem ? elem->name : det.elementId;
    m_nameLabel->setText(name);

    if (elem && !elem->image.isEmpty()) {
        const int comma = elem->image.indexOf(QLatin1Char(','));
        const QByteArray raw = QByteArray::fromBase64(elem->image.mid(comma + 1).toLatin1());
        QPixmap pm;
        pm.loadFromData(raw);
        if (!pm.isNull()) {
            m_photoLabel->setPixmap(pm.scaled(40, 40, Qt::KeepAspectRatioByExpanding,
                                              Qt::SmoothTransformation));
            m_photoLabel->setText(QString());
        } else {
            m_photoLabel->setPixmap(QPixmap());
            m_photoLabel->setText(name.left(2).toUpper());
        }
    } else {
        m_photoLabel->setPixmap(QPixmap());
        m_photoLabel->setText(elem ? name.left(2).toUpper() : QStringLiteral("?"));
    }

    adjustSize();
}
