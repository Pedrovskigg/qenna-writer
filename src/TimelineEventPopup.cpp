#include "TimelineEventPopup.h"

#include "Theme.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QPixmap colorPixmap(const QColor& c, int sz = 18)
{
    QPixmap px(sz, sz);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(c.isValid() ? c : QColor(110, 110, 110));
    p.setPen(p.brush().color().darker(130));
    p.drawRoundedRect(1, 1, sz - 2, sz - 2, 3, 3);
    return px;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

TimelineEventPopup::TimelineEventPopup(const QList<TimelineDef>& timelines,
                                       QWidget* parent)
    : QDialog(parent, Qt::Dialog), m_timelines(timelines)
{
    setWindowTitle(tr("Evento da linha do tempo"));
    setMinimumWidth(380);
    buildUi(timelines);
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &TimelineEventPopup::applyTheme);
}

void TimelineEventPopup::buildUi(const QList<TimelineDef>& timelines)
{
    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);
    root->setContentsMargins(16, 16, 16, 16);

    auto* form = new QFormLayout;
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_title = new QLineEdit(this);
    m_title->setPlaceholderText(tr("Nome do evento"));
    form->addRow(tr("Título:"), m_title);

    m_marker = new QLineEdit(this);
    m_marker->setPlaceholderText(tr("Ex: Dia 5, 15/07, Ano III..."));
    form->addRow(tr("Marcador:"), m_marker);

    root->addLayout(form);

    // ── Descrição ────────────────────────────────────────────────────────────
    auto* descLabel = new QLabel(tr("Descrição:"), this);
    descLabel->setObjectName(QStringLiteral("tlPopupSectionLabel"));
    root->addWidget(descLabel);

    m_desc = new QPlainTextEdit(this);
    m_desc->setPlaceholderText(tr("Detalhes do evento (visível no modo expandido)"));
    m_desc->setMinimumHeight(80);
    m_desc->setMaximumHeight(160);
    root->addWidget(m_desc);

    // ── Timeline ─────────────────────────────────────────────────────────────
    if (!timelines.isEmpty()) {
        auto* timelineLabel = new QLabel(tr("Timeline:"), this);
        timelineLabel->setObjectName(QStringLiteral("tlPopupSectionLabel"));
        root->addWidget(timelineLabel);

        m_timelineCombo = new QComboBox(this);
        for (const auto& t : timelines)
            m_timelineCombo->addItem(t.name, t.id);
        root->addWidget(m_timelineCombo);
    }

    // ── Cor ──────────────────────────────────────────────────────────────────
    auto* colorRow = new QHBoxLayout;
    colorRow->setSpacing(8);
    auto* colorLabel = new QLabel(tr("Cor:"), this);
    colorLabel->setObjectName(QStringLiteral("tlPopupSectionLabel"));

    m_colorBtn = new QToolButton(this);
    m_colorBtn->setObjectName(QStringLiteral("colorPickBtn"));
    m_colorBtn->setFixedHeight(28);
    m_colorBtn->setCursor(Qt::PointingHandCursor);
    updateColorBtn();

    auto* btnClear = new QToolButton(this);
    btnClear->setText(tr("Herdar"));
    btnClear->setToolTip(tr("Usar cor da timeline"));
    btnClear->setFixedHeight(28);
    btnClear->setCursor(Qt::PointingHandCursor);
    btnClear->setObjectName(QStringLiteral("tlSmallBtn"));

    connect(m_colorBtn, &QToolButton::clicked, this, &TimelineEventPopup::pickColor);
    connect(btnClear,   &QToolButton::clicked, this, [this]() {
        m_color = QColor();
        updateColorBtn();
    });

    colorRow->addWidget(colorLabel);
    colorRow->addWidget(m_colorBtn);
    colorRow->addWidget(btnClear);
    colorRow->addStretch();
    root->addLayout(colorRow);
    root->addStretch();

    // ── Botões ───────────────────────────────────────────────────────────────
    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btns->button(QDialogButtonBox::Ok)->setText(tr("Confirmar"));
    btns->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(btns);
}

void TimelineEventPopup::pickColor()
{
    const QColor init = m_color.isValid() ? m_color : QColor(QStringLiteral("#6c8ebf"));
    const QColor c = QColorDialog::getColor(init, this, tr("Cor do evento"));
    if (c.isValid()) { m_color = c; updateColorBtn(); }
}

void TimelineEventPopup::updateColorBtn()
{
    m_colorBtn->setIcon(QIcon(colorPixmap(m_color, 16)));
    m_colorBtn->setText(m_color.isValid() ? m_color.name() : tr("  Herdar da timeline"));
    m_colorBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
}

void TimelineEventPopup::setEventData(const TimelineEvent& e)
{
    m_eventId = e.id;
    if (m_title)  m_title->setText(e.title);
    if (m_marker) m_marker->setText(e.timeMarker);
    if (m_desc)   m_desc->setPlainText(e.description);
    m_color = e.color;
    updateColorBtn();

    if (m_timelineCombo) {
        for (int i = 0; i < m_timelineCombo->count(); ++i) {
            if (m_timelineCombo->itemData(i).toString() == e.timelineId) {
                m_timelineCombo->setCurrentIndex(i);
                break;
            }
        }
    }
}

TimelineEvent TimelineEventPopup::eventData() const
{
    TimelineEvent e;
    e.id          = m_eventId;
    e.title       = m_title  ? m_title->text().trimmed()         : QString();
    e.timeMarker  = m_marker ? m_marker->text().trimmed()        : QString();
    e.description = m_desc   ? m_desc->toPlainText().trimmed()   : QString();
    e.color       = m_color;
    if (m_timelineCombo)
        e.timelineId = m_timelineCombo->currentData().toString();
    return e;
}

void TimelineEventPopup::applyTheme()
{
    setStyleSheet(QStringLiteral(R"(
        QDialog { background: %1; }
        QLabel  { color: %2; font-size: 13px; }
        QLabel#tlPopupSectionLabel { color: %3; font-size: 11px; font-weight: 600;
                                     text-transform: uppercase; letter-spacing: 1px; }
        QLineEdit, QPlainTextEdit {
            background: %4; color: %2;
            border: 1px solid %5; border-radius: 5px;
            padding: 4px 8px; font-size: 13px;
        }
        QLineEdit:focus, QPlainTextEdit:focus { border-color: %6; }
        QComboBox {
            background: %4; color: %2;
            border: 1px solid %5; border-radius: 5px;
            padding: 4px 8px; font-size: 13px;
        }
        QToolButton#colorPickBtn, QToolButton#tlSmallBtn {
            background: %4; color: %2;
            border: 1px solid %5; border-radius: 5px;
            padding: 2px 8px; font-size: 12px;
        }
        QToolButton#colorPickBtn:hover, QToolButton#tlSmallBtn:hover {
            background: %7; border-color: %6;
        }
        QDialogButtonBox QPushButton {
            background: %4; color: %2;
            border: 1px solid %5; border-radius: 5px;
            padding: 5px 16px; font-size: 13px; min-width: 80px;
        }
        QDialogButtonBox QPushButton:hover { background: %7; border-color: %6; }
        QDialogButtonBox QPushButton:default { border-color: %6; color: %8; }
    )").arg(Theme::panelBackground(),  // 1
            Theme::textPrimary(),      // 2
            Theme::textMuted(),        // 3
            Theme::inputBackground(),  // 4
            Theme::subtleBorder(),     // 5
            Theme::accentDefault(),    // 6
            Theme::hoverOverlay(),     // 7
            Theme::textBright()));     // 8
}
