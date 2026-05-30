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
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

// Mesma lógica do TimelineEventItem — geometria correta por forma
QPainterPath shapePath(const QString& shape, const QRectF& r)
{
    QPainterPath p;
    if (shape == QStringLiteral("circle")) {
        const qreal d = qMin(r.width(), r.height());
        p.addEllipse(QRectF(r.center().x()-d/2, r.center().y()-d/2, d, d));
    } else if (shape == QStringLiteral("triangle")) {
        const qreal side = qMin(r.width(), r.height() * 2.0 / 1.7320508);
        const qreal h    = side * 1.7320508 / 2.0;
        const qreal cx = r.center().x(), top = r.center().y() - h/2, bot = top + h;
        p.moveTo(cx, top); p.lineTo(cx+side/2, bot); p.lineTo(cx-side/2, bot);
        p.closeSubpath();
    } else if (shape == QStringLiteral("diamond")) {
        p.moveTo(r.center().x(), r.top());
        p.lineTo(r.right(), r.center().y());
        p.lineTo(r.center().x(), r.bottom());
        p.lineTo(r.left(), r.center().y());
        p.closeSubpath();
    } else if (shape == QStringLiteral("hexagon")) {
        const qreal rad = qMin(r.width(), r.height()) / 2.0;
        const qreal cx = r.center().x(), cy = r.center().y();
        for (int i = 0; i < 6; ++i) {
            const double a = kPi/180.0 * (60.0*i);
            const qreal x = cx + rad*std::cos(a), y = cy + rad*std::sin(a);
            if (i == 0) p.moveTo(x, y); else p.lineTo(x, y);
        }
        p.closeSubpath();
    } else if (shape == QStringLiteral("pentagon")) {
        const qreal rad = qMin(r.width(), r.height()) / 2.0;
        const qreal cx = r.center().x(), cy = r.center().y();
        for (int i = 0; i < 5; ++i) {
            const double a = kPi/180.0 * (72.0*i - 90.0);
            const qreal x = cx + rad*std::cos(a), y = cy + rad*std::sin(a);
            if (i == 0) p.moveTo(x, y); else p.lineTo(x, y);
        }
        p.closeSubpath();
    } else if (shape == QStringLiteral("star")) {
        const qreal ro = qMin(r.width(), r.height()) / 2.0, ri = ro * 0.40;
        const qreal cx = r.center().x(), cy = r.center().y();
        for (int i = 0; i < 10; ++i) {
            const double a = kPi/180.0 * (36.0*i - 90.0);
            const qreal rr = (i%2==0) ? ro : ri;
            const qreal x = cx + rr*std::cos(a), y = cy + rr*std::sin(a);
            if (i == 0) p.moveTo(x, y); else p.lineTo(x, y);
        }
        p.closeSubpath();
    } else if (shape == QStringLiteral("arrow")) {
        const qreal l = r.left(), ri = r.right(), cy = r.center().y();
        const qreal sT = r.top()+r.height()*0.28, sB = r.bottom()-r.height()*0.28;
        const qreal sR = r.left()+r.width()*0.62;
        p.moveTo(l,sT); p.lineTo(sR,sT); p.lineTo(sR,r.top());
        p.lineTo(ri,cy); p.lineTo(sR,r.bottom()); p.lineTo(sR,sB); p.lineTo(l,sB);
        p.closeSubpath();
    } else {
        p.addRoundedRect(r, 5, 5);
    }
    return p;
}

QPixmap shapePixmap(const QString& shape, const QColor& fill, int sz = 36)
{
    QPixmap px(sz, sz);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(2, 2, sz-4, sz-4);

    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0, fill.lighter(130));
    g.setColorAt(1, fill.darker(125));
    p.setBrush(g);
    p.setPen(QPen(fill.darker(160), 1.2));
    p.drawPath(shapePath(shape, r));
    return px;
}

QPixmap colorPixmap(const QColor& c, int sz = 18)
{
    QPixmap px(sz, sz);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(c.isValid() ? c : QColor(110,110,110));
    p.setPen(p.brush().color().darker(130));
    p.drawRoundedRect(1, 1, sz-2, sz-2, 3, 3);
    return px;
}

struct ShapeDef { QString id; QString label; };
const QList<ShapeDef> kShapes = {
    { QStringLiteral("square"),   QStringLiteral("Retângulo")  },
    { QStringLiteral("circle"),   QStringLiteral("Círculo")    },
    { QStringLiteral("diamond"),  QStringLiteral("Losango")    },
    { QStringLiteral("triangle"), QStringLiteral("Triângulo")  },
    { QStringLiteral("hexagon"),  QStringLiteral("Hexágono")   },
    { QStringLiteral("pentagon"), QStringLiteral("Pentágono")  },
    { QStringLiteral("star"),     QStringLiteral("Estrela")    },
    { QStringLiteral("arrow"),    QStringLiteral("Seta")       },
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

TimelineEventPopup::TimelineEventPopup(const QList<TimelineDef>& timelines,
                                       QWidget* parent)
    : QDialog(parent, Qt::Dialog), m_timelines(timelines)
{
    setWindowTitle(tr("Evento da linha do tempo"));
    setMinimumWidth(360);
    buildUi(timelines);
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &TimelineEventPopup::applyTheme);
}

void TimelineEventPopup::buildUi(const QList<TimelineDef>& timelines)
{
    auto* root = new QVBoxLayout(this);
    root->setSpacing(12);
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

    m_desc = new QLineEdit(this);
    m_desc->setPlaceholderText(tr("Descrição breve (opcional)"));
    form->addRow(tr("Descrição:"), m_desc);

    if (!timelines.isEmpty()) {
        m_timelineCombo = new QComboBox(this);
        for (const auto& t : timelines)
            m_timelineCombo->addItem(t.name, t.id);
        form->addRow(tr("Timeline:"), m_timelineCombo);
    }

    root->addLayout(form);

    // ── Forma ────────────────────────────────────────────────────────────────
    auto* shapeLabel = new QLabel(tr("Forma:"), this);
    shapeLabel->setObjectName(QStringLiteral("tlPopupSectionLabel"));
    root->addWidget(shapeLabel);

    // Grid de formas: 4 colunas
    auto* shapeGrid = new QWidget(this);
    auto* sg = new QHBoxLayout(shapeGrid);
    sg->setSpacing(6);
    sg->setContentsMargins(0, 0, 0, 0);

    const QColor previewFill(QStringLiteral("#6c8ebf"));
    for (const auto& sd : kShapes) {
        auto* btn = new QToolButton(shapeGrid);
        btn->setCheckable(true);
        btn->setToolTip(sd.label);
        btn->setIcon(QIcon(shapePixmap(sd.id, previewFill, 32)));
        btn->setIconSize(QSize(32, 32));
        btn->setFixedSize(44, 44);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setObjectName(QStringLiteral("shapePickBtn"));
        btn->setProperty("shapeId", sd.id);
        sg->addWidget(btn);
        m_shapeBtns.append(btn);

        connect(btn, &QToolButton::clicked, this, [this, id = sd.id]() {
            selectShape(id);
        });
    }
    sg->addStretch();
    root->addWidget(shapeGrid);

    selectShape(m_shape);

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

void TimelineEventPopup::selectShape(const QString& shape)
{
    m_shape = shape;
    for (auto* btn : m_shapeBtns)
        btn->setChecked(btn->property("shapeId").toString() == shape);
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
    if (m_desc)   m_desc->setText(e.description);
    m_color = e.color;
    updateColorBtn();
    selectShape(e.shape.isEmpty() ? QStringLiteral("square") : e.shape);

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
    e.title       = m_title  ? m_title->text().trimmed()  : QString();
    e.timeMarker  = m_marker ? m_marker->text().trimmed() : QString();
    e.description = m_desc   ? m_desc->text().trimmed()   : QString();
    e.shape       = m_shape;
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
        QLineEdit {
            background: %4; color: %2;
            border: 1px solid %5; border-radius: 5px;
            padding: 4px 8px; font-size: 13px;
        }
        QLineEdit:focus { border-color: %6; }
        QComboBox {
            background: %4; color: %2;
            border: 1px solid %5; border-radius: 5px;
            padding: 4px 8px; font-size: 13px;
        }
        QToolButton#shapePickBtn {
            background: %4;
            border: 1px solid %5;
            border-radius: 8px;
        }
        QToolButton#shapePickBtn:checked {
            border: 2px solid %6;
            background: %7;
        }
        QToolButton#shapePickBtn:hover { background: %7; }
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
