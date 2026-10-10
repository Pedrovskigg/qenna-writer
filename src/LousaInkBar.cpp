#include "LousaInkBar.h"

#include "ColorPopover.h"
#include "IconUtils.h"
#include "LousaInk.h"
#include "Theme.h"

#include <QCoreApplication>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSettings>
#include <QSlider>
#include <QToolButton>

namespace {

struct PenDef { const char* tool; const char* icon; const char* label; const char* tip; };
const PenDef kPens[] = {
    { "pen",       "pen",         QT_TRANSLATE_NOOP("LousaInkBar", "Caneta"),      QT_TRANSLATE_NOOP("LousaInkBar", "Caneta: fina, segue a pressão") },
    { "marker",    "marker",      QT_TRANSLATE_NOOP("LousaInkBar", "Marcador"),    QT_TRANSLATE_NOOP("LousaInkBar", "Marcador: grosso e firme") },
    { "highlight", "highlighter", QT_TRANSLATE_NOOP("LousaInkBar", "Marca-texto"), QT_TRANSLATE_NOOP("LousaInkBar", "Marca-texto: transparente, por cima dos cards") },
    { "eraser",    "eraser",      QT_TRANSLATE_NOOP("LousaInkBar", "Borracha"),    QT_TRANSLATE_NOOP("LousaInkBar", "Borracha: apaga o traço inteiro (a ponta de trás da caneta também apaga)") },
};

const char* const kPalette[] = { "#2e2a24", "#ffffff", "#c0392b", "#d97757", "#e0b43a",
                                 "#4e9a5b", "#2a9d8f", "#3d6fc4", "#8a5cc2" };

QIcon penIcon(const QString& name)
{
    return IconUtils::loadToolbarIcon(QStringLiteral(":/icons/lousa/%1.svg").arg(name),
                                      QColor(Theme::textPrimary()), QColor(Theme::textBright()),
                                      QColor(Theme::accentDefault()));
}

} // namespace

LousaInkBar::LousaInkBar(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("lousaInkBar"));
    setAttribute(Qt::WA_StyledBackground, true);

    QSettings st;
    m_tool  = st.value(QStringLiteral("lousa/inkTool"), QStringLiteral("pen")).toString();
    if (m_tool == QStringLiteral("eraser")) m_tool = QStringLiteral("pen");   // abre sempre riscando
    m_color = QColor(st.value(QStringLiteral("lousa/inkColor"), QStringLiteral("#c0392b")).toString());
    if (!m_color.isValid()) m_color = QColor(QStringLiteral("#c0392b"));
    m_size  = qBound(1.0, st.value(QStringLiteral("lousa/inkSize"), 4.0).toDouble(), 14.0);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(6, 5, 8, 5);
    lay->setSpacing(2);
    auto sep = [this, lay]() {
        auto* s = new QFrame(this);
        s->setObjectName(QStringLiteral("lousaInkSep"));
        s->setFixedSize(1, 30);
        lay->addSpacing(5);
        lay->addWidget(s, 0, Qt::AlignVCenter);
        lay->addSpacing(5);
        m_seps << s;
    };

    for (const PenDef& d : kPens) {
        auto* b = new QToolButton(this);
        b->setObjectName(QStringLiteral("lousaInkTool"));
        b->setCursor(Qt::PointingHandCursor);
        b->setCheckable(true);
        b->setIconSize(QSize(20, 20));
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setFixedSize(QString::fromLatin1(d.tool) == QStringLiteral("highlight") ? 76 : 62, 48);
        b->setText(QCoreApplication::translate("LousaInkBar", d.label));
        b->setToolTip(QCoreApplication::translate("LousaInkBar", d.tip));
        const QString tool = QString::fromLatin1(d.tool);
        connect(b, &QToolButton::clicked, this, [this, tool]() { setTool(tool); });
        lay->addWidget(b);
        m_tools.append({ b, tool });
    }
    sep();

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(5);
    grid->setVerticalSpacing(5);
    int i = 0;
    for (const char* hex : kPalette) {
        auto* b = new QToolButton(this);
        b->setObjectName(QStringLiteral("lousaInkSwatch"));
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(18, 18);
        const QColor c(QString::fromLatin1(hex));
        connect(b, &QToolButton::clicked, this, [this, c]() {
            m_color = c;
            if (m_tool == QStringLiteral("eraser")) m_tool = QStringLiteral("pen");
            refreshTools(); refreshSwatches(); save(); emit toolChanged();
        });
        grid->addWidget(b, i / 5, i % 5);
        m_swatches.append({ b, c });
        ++i;
    }
    m_pick = new QToolButton(this);
    m_pick->setObjectName(QStringLiteral("lousaInkPick"));
    m_pick->setCursor(Qt::PointingHandCursor);
    m_pick->setFixedSize(18, 18);
    m_pick->setToolTip(tr("Outra cor"));
    connect(m_pick, &QToolButton::clicked, this, [this]() {
        const QColor nc = ColorPopover::getColor(m_color, this, tr("Cor da caneta"));
        if (!nc.isValid()) return;
        m_color = nc;
        if (m_tool == QStringLiteral("eraser")) m_tool = QStringLiteral("pen");
        refreshTools(); refreshSwatches(); save(); emit toolChanged();
    });
    grid->addWidget(m_pick, 1, 4);
    lay->addLayout(grid);
    sep();

    m_dot = new QLabel(this);
    m_dot->setFixedSize(22, 22);
    lay->addWidget(m_dot);
    m_sizeSlider = new QSlider(Qt::Horizontal, this);
    m_sizeSlider->setObjectName(QStringLiteral("lousaInkSlider"));
    m_sizeSlider->setRange(1, 14);
    m_sizeSlider->setValue(int(m_size));
    m_sizeSlider->setFixedWidth(76);
    m_sizeSlider->setToolTip(tr("Espessura"));
    connect(m_sizeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_size = v; refreshDot(); save(); emit toolChanged();
    });
    lay->addWidget(m_sizeSlider);
    sep();

    auto* stabLbl = new QLabel(tr("Estabilizador"), this);
    stabLbl->setObjectName(QStringLiteral("lousaInkLabel"));
    lay->addWidget(stabLbl);
    lay->addSpacing(4);
    m_stabSlider = new QSlider(Qt::Horizontal, this);
    m_stabSlider->setObjectName(QStringLiteral("lousaInkSlider"));
    m_stabSlider->setRange(0, 100);
    m_stabSlider->setValue(LousaInk::stabilizerSetting());
    m_stabSlider->setFixedWidth(76);
    const QString stabTip = tr("Quanto maior, mais o traço vem atrás da caneta, e mais liso sai. Vale pra Caneta e pro Esboço.");
    m_stabSlider->setToolTip(stabTip);
    stabLbl->setToolTip(stabTip);
    lay->addWidget(m_stabSlider);
    m_stabValue = new QLabel(this);
    m_stabValue->setObjectName(QStringLiteral("lousaInkLabel"));
    m_stabValue->setFixedWidth(34);
    m_stabValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_stabValue->setText(QStringLiteral("%1%").arg(m_stabSlider->value()));
    connect(m_stabSlider, &QSlider::valueChanged, this, [this](int v) {
        LousaInk::setStabilizerSetting(v);
        m_stabValue->setText(QStringLiteral("%1%").arg(v));
    });
    lay->addWidget(m_stabValue);
    sep();

    auto textBtn = [this, lay](const QString& text, const QString& tip, bool primary) {
        auto* b = new QToolButton(this);
        b->setObjectName(primary ? QStringLiteral("lousaInkDone") : QStringLiteral("lousaInkText"));
        b->setCursor(Qt::PointingHandCursor);
        b->setText(text);
        if (!tip.isEmpty()) b->setToolTip(tip);
        lay->addWidget(b);
        m_textBtns << b;
        return b;
    };
    connect(textBtn(tr("Desfazer"), tr("Desfazer (Ctrl+Z)"), false), &QToolButton::clicked, this, &LousaInkBar::undoRequested);
    connect(textBtn(tr("Limpar"), tr("Apaga todos os traços desta lousa (Ctrl+Z desfaz)"), false), &QToolButton::clicked, this, &LousaInkBar::clearRequested);
    lay->addSpacing(2);
    connect(textBtn(tr("Pronto"), tr("Sair da caneta (Esc)"), true), &QToolButton::clicked, this, &LousaInkBar::doneRequested);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 120));
    setGraphicsEffect(shadow);

    applyTheme();
    refreshTools();
    refreshSwatches();
    refreshDot();
    adjustSize();
}

void LousaInkBar::setTool(const QString& tool)
{
    m_tool = tool;
    refreshTools();
    save();
    emit toolChanged();
}

void LousaInkBar::save() const
{
    QSettings st;
    st.setValue(QStringLiteral("lousa/inkTool"), m_tool);
    st.setValue(QStringLiteral("lousa/inkColor"), m_color.name());
    st.setValue(QStringLiteral("lousa/inkSize"), m_size);
}

void LousaInkBar::refreshTools()
{
    for (auto& [b, t] : m_tools) { QSignalBlocker bl(b); b->setChecked(t == m_tool); }
}

void LousaInkBar::refreshSwatches()
{
    const QString ring = Theme::accentDefault();
    const QString bg = Theme::panelBackground();
    bool matched = false;
    for (auto& [b, c] : m_swatches) {
        const bool on = c.name() == m_color.name();
        matched |= on;
        b->setStyleSheet(QStringLiteral(
            "QToolButton { background: %1; border: 1.5px solid rgba(0,0,0,0.22); border-radius: 9px; }"
            "QToolButton:hover { border-color: %2; }").arg(c.name(), ring)
            + (on ? QStringLiteral("QToolButton { border: 2px solid %1; }").arg(ring) : QString()));
        Q_UNUSED(bg);
    }
    // O botão de outra cor mostra a cor escolhida quando ela não está na cartela.
    const QString fill = matched ? QStringLiteral("qconicalgradient(cx:0.5, cy:0.5, angle:0, stop:0 #e05555, stop:0.2 #e0b43a,"
                                                  " stop:0.4 #4e9a5b, stop:0.6 #3d6fc4, stop:0.8 #8a5cc2, stop:1 #e05555)")
                                 : m_color.name();
    m_pick->setStyleSheet(QStringLiteral(
        "QToolButton { background: %1; border: %2 solid %3; border-radius: 9px; }")
        .arg(fill, matched ? QStringLiteral("1.5px") : QStringLiteral("2px"),
             matched ? QStringLiteral("rgba(0,0,0,0.22)") : ring));
}

void LousaInkBar::refreshDot()
{
    const qreal dpr = devicePixelRatioF();
    QPixmap pm(QSize(22, 22) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(Theme::textPrimary()));
    const qreal d = qMax(3.0, m_size * 1.3);
    p.drawEllipse(QPointF(11, 11), d / 2, d / 2);
    p.end();
    m_dot->setPixmap(pm);
}

void LousaInkBar::applyTheme()
{
    const QColor a(Theme::accentDefault());
    const QString soft = QStringLiteral("rgba(%1,%2,%3,0.18)").arg(a.red()).arg(a.green()).arg(a.blue());
    setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#lousaInkBar { background: %1; border: 1px solid %2; border-radius: 14px; }"
        "QToolButton#lousaInkTool { background: transparent; border: none; border-radius: 9px;"
        "  color: %3; font-size: 11px; padding: 3px 0 0 0; }"
        "QToolButton#lousaInkTool:hover { background: %4; color: %5; }"
        "QToolButton#lousaInkTool:checked { background: %6; color: %5; }"
        "QToolButton#lousaInkText { background: transparent; border: none; border-radius: 7px; color: %3;"
        "  font-size: 12px; padding: 6px 9px; }"
        "QToolButton#lousaInkText:hover { background: %4; color: %5; }"
        "QToolButton#lousaInkDone { background: %7; border: none; border-radius: 7px; color: #ffffff;"
        "  font-size: 12px; font-weight: 600; padding: 6px 12px; }"
        "QToolButton#lousaInkDone:hover { background: %8; }"
        "QLabel#lousaInkLabel { color: %9; font-size: 11px; background: transparent; }"
        "QFrame#lousaInkSep { background: %2; }"
        "QSlider#lousaInkSlider::groove:horizontal { background: %2; height: 4px; border-radius: 2px; }"
        "QSlider#lousaInkSlider::sub-page:horizontal { background: %7; border-radius: 2px; }"
        "QSlider#lousaInkSlider::handle:horizontal { background: %7; width: 12px; margin: -5px 0; border-radius: 6px; }"
    ).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(),
          Theme::hoverStrong(), Theme::textBright(), soft, a.name(), a.darker(112).name(),
          Theme::textMuted())));
    int i = 0;
    for (const PenDef& d : kPens) m_tools[i++].first->setIcon(penIcon(QString::fromLatin1(d.icon)));
    if (m_dot) refreshDot();
    if (m_pick) refreshSwatches();
}
