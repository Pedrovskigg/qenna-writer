#include "ColorPopover.h"

#include "Theme.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCursor>
#include <QEventLoop>
#include <QFrame>
#include <QStyle>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <algorithm>
#include <cmath>

using ColorPick::Choice;

namespace {
constexpr int kShadow = 16;   // margem transparente pra sombra
constexpr int kWidth  = 268;  // largura do balão
constexpr int kSw     = 24;   // diâmetro de cada amostra

const char* const kPalette[] = {
    "#e06c75", "#e5875f", "#e0a458", "#d8c26a", "#b5c96a", "#7fbf8f", "#5fb3a8", "#5aa9d6",
    "#6c8ebf", "#8f8fe0", "#b48ade", "#d58bc4", "#c0553f", "#3f8f7a", "#3e6fb0", "#9aa0ad" };

QColor col(const QString& s) { return QColor(s); }
QColor withAlpha(QColor c, qreal a) { c.setAlphaF(float(a)); return c; }

QFont uiFont(qreal px, int weight = QFont::Normal)
{
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(qRound(px));
    f.setWeight(QFont::Weight(weight));
    return f;
}
QFont monoFont(qreal px)
{
    static const QString fam = QFontDatabase::families().contains(QStringLiteral("IBM Plex Mono"))
        ? QStringLiteral("IBM Plex Mono") : QStringLiteral("Consolas");
    QFont f(fam);
    f.setPixelSize(qRound(px));
    return f;
}

void paintChecker(QPainter& p, const QRectF& r)
{
    p.save();
    p.setClipRect(r, Qt::IntersectClip); // soma ao recorte redondo de quem chama
    const int s = 5;
    for (int y = int(r.top()); y < r.bottom(); y += s)
        for (int x = int(r.left()); x < r.right(); x += s)
            p.fillRect(QRect(x, y, s, s), ((x / s + y / s) % 2) ? QColor(200, 200, 200) : QColor(245, 245, 245));
    p.restore();
}

QString themeName(const QString& key)
{
    if (key == QLatin1String("accent"))  return QCoreApplication::translate("ColorPopover", "Destaque");
    if (key == QLatin1String("warning")) return QCoreApplication::translate("ColorPopover", "Aviso");
    if (key == QLatin1String("info"))    return QCoreApplication::translate("ColorPopover", "Informação");
    if (key == QLatin1String("success")) return QCoreApplication::translate("ColorPopover", "Sucesso");
    return QCoreApplication::translate("ColorPopover", "Perigo");
}
} // namespace

QColor ColorPick::themeColor(const QString& key)
{
    if (key == QLatin1String("accent"))  return col(Theme::accentDefault());
    if (key == QLatin1String("warning")) return col(Theme::accentWarning());
    if (key == QLatin1String("info"))    return col(Theme::accentInfo());
    if (key == QLatin1String("success")) return col(Theme::accentSuccess());
    if (key == QLatin1String("danger"))  return col(Theme::accentDanger());
    return {};
}
QStringList ColorPick::themeKeys()
{
    return { QStringLiteral("accent"), QStringLiteral("warning"), QStringLiteral("info"),
             QStringLiteral("success"), QStringLiteral("danger") };
}

// ── amostra redonda ──────────────────────────────────────────────────────────
class SwatchButton : public QAbstractButton {
public:
    SwatchButton(const Choice& c, QWidget* parent) : QAbstractButton(parent), m_choice(c)
    {
        setFixedSize(kSw + 6, kSw + 6);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        setToolTip(c.isTheme() ? themeName(c.themeKey) : c.color.name());
    }
    Choice choice() const
    {
        Choice c = m_choice;
        if (c.isTheme()) c.color = ColorPick::themeColor(c.themeKey);
        return c;
    }
    void setSelected(bool s) { if (m_sel != s) { m_sel = s; update(); } }
    const Choice& raw() const { return m_choice; }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QPointF c = QRectF(rect()).center();
        const qreal r = kSw / 2.0 * (underMouse() ? 1.14 : 1.0);
        if (m_sel) {
            p.setPen(QPen(col(Theme::textBright()), 2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, kSw / 2.0 + 3.2, kSw / 2.0 + 3.2);
        }
        p.setPen(m_choice.isTheme() ? QPen(QColor(0, 0, 0, 46), 2) : QPen(Qt::NoPen));
        p.setBrush(choice().color);
        p.drawEllipse(c, r, r);
    }
private:
    Choice m_choice;
    bool m_sel = false;
};

// ── quadrado de saturação × brilho ───────────────────────────────────────────
class SVSquare : public QWidget {
public:
    explicit SVSquare(QWidget* parent) : QWidget(parent) { setFixedHeight(118); setCursor(Qt::CrossCursor); }
    std::function<void()> onChange;
    qreal h = 0, s = 0, v = 0;
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip; clip.addRoundedRect(QRectF(rect()), Theme::controlRadius(), Theme::controlRadius());
        p.setClipPath(clip);
        p.fillRect(rect(), QColor::fromHsvF(float(h / 360.0), 1, 1));
        QLinearGradient gw(0, 0, width(), 0); gw.setColorAt(0, Qt::white); gw.setColorAt(1, QColor(255, 255, 255, 0));
        p.fillRect(rect(), gw);
        QLinearGradient gb(0, 0, 0, height()); gb.setColorAt(0, QColor(0, 0, 0, 0)); gb.setColorAt(1, Qt::black);
        p.fillRect(rect(), gb);
        p.setClipping(false);
        const QPointF t(s * width(), (1 - v) * height());
        p.setPen(QPen(QColor(0, 0, 0, 100), 1));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(t, 8, 8);
        p.setPen(QPen(Qt::white, 2));
        p.drawEllipse(t, 6.5, 6.5);
    }
    void mousePressEvent(QMouseEvent* e) override { pick(e->position()); }
    void mouseMoveEvent(QMouseEvent* e) override { if (e->buttons() & Qt::LeftButton) pick(e->position()); }
private:
    void pick(const QPointF& pt)
    {
        s = std::clamp(pt.x() / width(), 0.0, 1.0);
        v = 1.0 - std::clamp(pt.y() / height(), 0.0, 1.0);
        update();
        if (onChange) onChange();
    }
};

// ── barra de matiz ───────────────────────────────────────────────────────────
class HueSlider : public QWidget {
public:
    explicit HueSlider(QWidget* parent) : QWidget(parent) { setFixedHeight(16); setCursor(Qt::PointingHandCursor); }
    std::function<void()> onChange;
    qreal h = 0;
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF bar(0, 2, width(), 12);
        QLinearGradient g(bar.topLeft(), bar.topRight());
        for (int k = 0; k <= 6; ++k) g.setColorAt(k / 6.0, QColor::fromHsvF(float(std::min(k / 6.0, 0.9999)), 1, 1));
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawRoundedRect(bar, 6, 6);
        const QPointF t(std::clamp(h / 360.0 * width(), 7.0, width() - 7.0), bar.center().y());
        p.setPen(QPen(QColor(0, 0, 0, 100), 1)); p.setBrush(Qt::NoBrush); p.drawEllipse(t, 8, 8);
        p.setPen(QPen(Qt::white, 2)); p.drawEllipse(t, 6.5, 6.5);
    }
    void mousePressEvent(QMouseEvent* e) override { pick(e->position().x()); }
    void mouseMoveEvent(QMouseEvent* e) override { if (e->buttons() & Qt::LeftButton) pick(e->position().x()); }
private:
    void pick(qreal x) { h = std::clamp(x / width() * 360.0, 0.0, 359.9); update(); if (onChange) onChange(); }
};

// ── opacidade (só quando a tela pede) ────────────────────────────────────────
class AlphaSlider : public QWidget {
public:
    explicit AlphaSlider(QWidget* parent) : QWidget(parent) { setFixedHeight(16); setCursor(Qt::PointingHandCursor); }
    std::function<void()> onChange;
    qreal a = 1;
    QColor base;
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF bar(0, 2, width(), 12);
        QPainterPath clip; clip.addRoundedRect(bar, 6, 6);
        p.setClipPath(clip);
        paintChecker(p, bar);
        QLinearGradient g(bar.topLeft(), bar.topRight());
        g.setColorAt(0, withAlpha(base, 0)); g.setColorAt(1, withAlpha(base, 1));
        p.fillRect(bar, g);
        p.setClipping(false);
        const QPointF t(std::clamp(a * width(), 7.0, width() - 7.0), bar.center().y());
        p.setPen(QPen(QColor(0, 0, 0, 100), 1)); p.setBrush(Qt::NoBrush); p.drawEllipse(t, 8, 8);
        p.setPen(QPen(Qt::white, 2)); p.drawEllipse(t, 6.5, 6.5);
    }
    void mousePressEvent(QMouseEvent* e) override { pick(e->position().x()); }
    void mouseMoveEvent(QMouseEvent* e) override { if (e->buttons() & Qt::LeftButton) pick(e->position().x()); }
private:
    void pick(qreal x) { a = std::clamp(x / width(), 0.0, 1.0); update(); if (onChange) onChange(); }
};

// ── amostra grande do cabeçalho ──────────────────────────────────────────────
class CurrentSwatch : public QWidget {
public:
    explicit CurrentSwatch(QWidget* parent) : QWidget(parent) { setFixedSize(26, 26); }
    QColor c;
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath path; path.addEllipse(QRectF(rect()));
        p.setClipPath(path);
        if (c.alpha() < 255) paintChecker(p, rect());
        p.fillRect(rect(), c);
        p.setClipping(false);
        p.setPen(QPen(QColor(255, 255, 255, 22), 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(rect()).adjusted(1, 1, -1, -1));
    }
};

// ═════════════════════════════════════════════════════════════════════════════

ColorPopover::ColorPopover(const Choice& current, const Options& opt, QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , m_initial(current), m_current(current), m_opt(opt)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    if (m_current.isTheme()) m_current.color = ColorPick::themeColor(m_current.themeKey);
    if (!m_current.color.isValid()) m_current.color = QColor(QStringLiteral("#6c8ebf"));
    m_initial = m_current;
    build();
}

void ColorPopover::build()
{
    const QColor panel = col(Theme::panelBackground()), bright = col(Theme::textBright()),
                 ink = col(Theme::textPrimary()), border = col(Theme::panelBorder()),
                 accent = col(Theme::accentDefault());
    const QColor dim = QColor::fromRgbF(ink.redF() * 0.64f + panel.redF() * 0.36f,
                                        ink.greenF() * 0.64f + panel.greenF() * 0.36f,
                                        ink.blueF() * 0.64f + panel.blueF() * 0.36f);
    setStyleSheet(Theme::qss(QStringLiteral(R"(
        QLabel { background: transparent; }
        QLabel#cpTitle { color: %1; }
        QLabel#cpSub, QLabel#cpSection { color: %2; }
        QLabel#cpNote { color: %3; }
        QToolButton#cpDisc { background: transparent; border: none; color: %4; font-size: 12px;
                             padding: 4px 2px; text-align: left; }
        QToolButton#cpDisc:hover { color: %1; }
        QLineEdit#cpHex { background: rgba(0,0,0,0.2); color: %1; border: 1px solid %5;
                          border-radius: @radius-control; padding: 0 9px; min-height: 26px; }
        QLineEdit#cpHex:focus { border-color: %3; }
        QLineEdit#cpHex[bad="true"] { border-color: %6; }
        QPushButton#cpReset { background: transparent; border: none; color: %2; font-size: 12px; padding: 0; }
        QPushButton#cpReset:hover { color: %1; }
    )")).arg(bright.name(), dim.name(), accent.name(), ink.name(), border.name(),
             col(Theme::accentDanger()).name()));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(kShadow, kShadow - 6, kShadow, kShadow + 4);
    m_panel = new QWidget(this);
    m_panel->setFixedWidth(kWidth);
    outer->addWidget(m_panel);
    auto* v = new QVBoxLayout(m_panel);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(0);

    // cabeçalho
    auto* hd = new QHBoxLayout;
    hd->setSpacing(9);
    auto* cs = new CurrentSwatch(m_panel);
    m_curSwatch = cs;
    hd->addWidget(cs);
    auto* ht = new QVBoxLayout;
    ht->setSpacing(1);
    m_title = new QLabel(m_opt.title.isEmpty() ? tr("Cor") : m_opt.title, m_panel);
    m_title->setObjectName(QStringLiteral("cpTitle"));
    m_title->setFont(uiFont(12.5, QFont::DemiBold));
    m_sub = new QLabel(m_panel);
    m_sub->setObjectName(QStringLiteral("cpSub"));
    m_sub->setFont(monoFont(11));
    ht->addWidget(m_title);
    ht->addWidget(m_sub);
    hd->addLayout(ht, 1);
    v->addLayout(hd);
    v->addSpacing(12);

    auto section = [&](const QString& text, QLabel** note = nullptr) {
        auto* row = new QHBoxLayout;
        auto* l = new QLabel(text.toUpper(), m_panel);
        l->setObjectName(QStringLiteral("cpSection"));
        QFont f = uiFont(9.5, QFont::DemiBold);
        f.setLetterSpacing(QFont::PercentageSpacing, 113);
        l->setFont(f);
        row->addWidget(l);
        row->addStretch();
        if (note) {
            *note = new QLabel(m_panel);
            (*note)->setObjectName(QStringLiteral("cpNote"));
            (*note)->setFont(uiFont(10.5, QFont::Medium));
            row->addWidget(*note);
        }
        v->addLayout(row);
        v->addSpacing(6);
    };

    section(tr("Do tema"), &m_themeNote);
    m_themeRow = new QWidget(m_panel);
    m_themeRow->installEventFilter(this);
    v->addWidget(m_themeRow);
    v->addSpacing(10);
    section(tr("Paleta"));
    m_paletteGrid = new QWidget(m_panel);
    m_paletteGrid->installEventFilter(this);
    v->addWidget(m_paletteGrid);

    m_recentSec = new QWidget(m_panel);
    auto* rv = new QVBoxLayout(m_recentSec);
    rv->setContentsMargins(0, 10, 0, 0);
    rv->setSpacing(6);
    {
        auto* l = new QLabel(tr("Recentes").toUpper(), m_recentSec);
        l->setObjectName(QStringLiteral("cpSection"));
        QFont f = uiFont(9.5, QFont::DemiBold);
        f.setLetterSpacing(QFont::PercentageSpacing, 113);
        l->setFont(f);
        rv->addWidget(l);
        m_recentRow = new QWidget(m_recentSec);
        m_recentRow->installEventFilter(this);
        rv->addWidget(m_recentRow);
    }
    v->addWidget(m_recentSec);

    // personalizada (recolhida)
    v->addSpacing(8);
    m_disc = new QToolButton(m_panel);
    m_disc->setObjectName(QStringLiteral("cpDisc"));
    m_disc->setText(tr("Personalizada"));
    m_disc->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_disc->setCursor(Qt::PointingHandCursor);
    m_disc->setCheckable(true);
    v->addWidget(m_disc, 0, Qt::AlignLeft);
    auto discIcon = [ink](bool open) {
        QPixmap pm(QSize(10, 10) * 2); pm.setDevicePixelRatio(2); pm.fill(Qt::transparent);
        QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(ink, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath path;
        if (open) { path.moveTo(1.5, 3); path.lineTo(5, 7); path.lineTo(8.5, 3); }
        else      { path.moveTo(3, 1.5); path.lineTo(7, 5); path.lineTo(3, 8.5); }
        p.drawPath(path);
        return QIcon(pm);
    };
    m_disc->setIcon(discIcon(false));
    connect(m_disc, &QToolButton::toggled, this, [this, discIcon](bool on) {
        m_disc->setIcon(discIcon(on));
        setCustomOpen(on);
    });

    m_custom = new QWidget(m_panel);
    auto* cv = new QVBoxLayout(m_custom);
    cv->setContentsMargins(0, 8, 0, 0);
    cv->setSpacing(9);
    m_sv = new SVSquare(m_custom);
    m_hue = new HueSlider(m_custom);
    cv->addWidget(m_sv);
    cv->addWidget(m_hue);
    if (m_opt.alpha) {
        m_alpha = new AlphaSlider(m_custom);
        cv->addWidget(m_alpha);
    }
    m_hex = new QLineEdit(m_custom);
    m_hex->setObjectName(QStringLiteral("cpHex"));
    m_hex->setFont(monoFont(12));
    m_hex->setMaxLength(9);
    cv->addWidget(m_hex);
    v->addWidget(m_custom);
    m_custom->hide();

    auto customChanged = [this]() {
        QColor c = QColor::fromHsvF(float(m_hue->h / 360.0), float(m_sv->s), float(m_sv->v));
        if (m_alpha) { c.setAlphaF(float(m_alpha->a)); m_alpha->base = c; m_alpha->update(); }
        m_current = Choice{ c, QString() };
        m_sv->h = m_hue->h;
        m_sv->update();
        { QSignalBlocker b(m_hex); m_hex->setText(c.alpha() < 255 ? c.name(QColor::HexArgb) : c.name()); }
        m_hex->setProperty("bad", false); m_hex->style()->unpolish(m_hex); m_hex->style()->polish(m_hex);
        updateSelection();
        preview(m_current);
    };
    m_sv->onChange = customChanged;
    m_hue->onChange = customChanged;
    if (m_alpha) m_alpha->onChange = customChanged;
    connect(m_hex, &QLineEdit::textEdited, this, [this](const QString& t) {
        QString s = t.trimmed();
        if (!s.startsWith(QLatin1Char('#'))) s.prepend(QLatin1Char('#'));
        static const QRegularExpression re(QStringLiteral("^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$"));
        const bool ok = re.match(s).hasMatch();
        m_hex->setProperty("bad", !ok && t.size() >= 6);
        m_hex->style()->unpolish(m_hex); m_hex->style()->polish(m_hex);
        if (!ok) return;
        m_current = Choice{ QColor(s), QString() };
        syncCustomFromCurrent();
        updateSelection();
        preview(m_current);
    });

    // rodapé
    v->addSpacing(12);
    auto* sep = new QFrame(m_panel);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QStringLiteral("background:%1;").arg(withAlpha(ink, 0.12).name(QColor::HexArgb)));
    v->addWidget(sep);
    v->addSpacing(10);
    auto* ft = new QHBoxLayout;
    if (m_opt.resetAvailable) {
        auto* reset = new QPushButton(tr("Restaurar padrão"), m_panel);
        reset->setObjectName(QStringLiteral("cpReset"));
        reset->setCursor(Qt::PointingHandCursor);
        reset->setFlat(true);
        connect(reset, &QPushButton::clicked, this, [this]() { finish(Reset); });
        ft->addWidget(reset);
    }
    ft->addStretch();
    auto* esc = new QLabel(m_panel);
    esc->setTextFormat(Qt::RichText);
    esc->setFont(monoFont(10.5));
    esc->setText(QStringLiteral("<span style='color:%1;border:1px solid %2'>&nbsp;Esc&nbsp;</span>"
                                "<span style='color:%3'> %4</span>")
                     .arg(ink.name(), border.name(), dim.name(), tr("desfaz")));
    ft->addWidget(esc);
    v->addLayout(ft);

    rebuildSwatches();
    syncCustomFromCurrent();
    refreshHeader(m_current);
}

void ColorPopover::rebuildSwatches()
{
    auto fill = [this](QWidget* host, const QList<Choice>& items, int cols) {
        if (host->layout()) {
            QLayoutItem* it;
            while ((it = host->layout()->takeAt(0))) { delete it->widget(); delete it; }
            delete host->layout();
        }
        auto* g = new QGridLayout(host);
        g->setContentsMargins(0, 0, 0, 0);
        g->setHorizontalSpacing(0);
        g->setVerticalSpacing(0);
        g->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        for (int i = 0; i < items.size(); ++i) {
            auto* b = new SwatchButton(items[i], host);
            b->setSelected(isSelected(items[i]));
            const Choice c = b->choice();
            b->installEventFilter(this);
            b->setProperty("cpIndex", i);
            connect(b, &QAbstractButton::clicked, this, [this, c]() {
                Choice pick = c;
                if (!m_opt.themeBinding) pick.themeKey.clear();
                if (m_alpha && m_current.color.alpha() < 255) pick.color.setAlpha(m_current.color.alpha());
                m_current = pick;
                finish(Accepted);
            });
            g->addWidget(b, i / cols, i % cols);
        }
    };
    QList<Choice> theme;
    for (const QString& k : ColorPick::themeKeys()) theme << Choice{ ColorPick::themeColor(k), k };
    fill(m_themeRow, theme, 8);
    QList<Choice> pal;
    for (const char* h : kPalette) pal << Choice{ QColor(QString::fromLatin1(h)), QString() };
    fill(m_paletteGrid, pal, 8);
    QList<Choice> rec;
    for (const QString& h : QSettings().value(QStringLiteral("colorPicker/recent")).toStringList())
        if (QColor(h).isValid()) rec << Choice{ QColor(h), QString() };
    m_recentSec->setVisible(!rec.isEmpty());
    fill(m_recentRow, rec, 8);
    m_themeNote->setText(m_opt.themeBinding && m_current.isTheme() ? tr("acompanha o tema") : QString());
}

bool ColorPopover::isSelected(const Choice& item) const
{
    if (item.isTheme()) {
        if (m_opt.themeBinding) return m_current.isTheme() && m_current.themeKey == item.themeKey;
        return !m_current.isTheme() && ColorPick::themeColor(item.themeKey) == m_current.color;
    }
    return !m_current.isTheme() && m_current.color.alpha() == 255
        && item.color.name() == m_current.color.name();
}

void ColorPopover::updateSelection()
{
    for (auto* ab : m_panel->findChildren<QAbstractButton*>())
        if (auto* b = dynamic_cast<SwatchButton*>(ab)) b->setSelected(isSelected(b->raw()));
    m_themeNote->setText(m_opt.themeBinding && m_current.isTheme() ? tr("acompanha o tema") : QString());
}

bool ColorPopover::eventFilter(QObject* obj, QEvent* ev)
{
    if (auto* b = dynamic_cast<SwatchButton*>(obj)) {
        if (ev->type() == QEvent::Enter) {
            Choice c = b->choice();
            if (!m_opt.themeBinding) c.themeKey.clear();
            preview(c);
        }
    } else if ((obj == m_themeRow || obj == m_paletteGrid || obj == m_recentRow) && ev->type() == QEvent::Leave) {
        preview(m_current);
    }
    return QWidget::eventFilter(obj, ev);
}

void ColorPopover::syncCustomFromCurrent()
{
    float h, s, v, a;
    m_current.color.getHsvF(&h, &s, &v, &a);
    if (h < 0) h = float(m_hue->h / 360.0); // cinza: mantém o matiz de antes
    m_hue->h = h * 360.0; m_sv->h = m_hue->h; m_sv->s = s; m_sv->v = v;
    if (m_alpha) { m_alpha->a = a; m_alpha->base = m_current.color; m_alpha->update(); }
    m_sv->update(); m_hue->update();
    if (!m_hex->hasFocus()) {
        QSignalBlocker b(m_hex);
        m_hex->setText(m_current.color.alpha() < 255 ? m_current.color.name(QColor::HexArgb) : m_current.color.name());
    }
}

void ColorPopover::setCustomOpen(bool open)
{
    m_custom->setVisible(open);
    if (open) syncCustomFromCurrent();
    adjustSize();
    keepOnScreen();
}

void ColorPopover::refreshHeader(const Choice& shown)
{
    static_cast<CurrentSwatch*>(m_curSwatch)->c = shown.color;
    m_curSwatch->update();
    if (shown.isTheme() && m_opt.themeBinding)
        m_sub->setText(tr("cor do tema · %1").arg(themeName(shown.themeKey).toLower()));
    else
        m_sub->setText(shown.color.alpha() < 255 ? shown.color.name(QColor::HexArgb) : shown.color.name());
}

void ColorPopover::preview(const Choice& c)
{
    refreshHeader(c);
    emit previewed(c);
}

void ColorPopover::finish(Result r)
{
    if (m_finished) return;
    m_finished = true;
    const Choice out = r == Cancelled ? m_initial : m_current;
    if (r == Accepted && !out.isTheme()) rememberRecent(out.color);
    emit finished(out, r);
    close();
}

void ColorPopover::rememberRecent(const QColor& c)
{
    if (!c.isValid()) return;
    QSettings qs;
    QStringList list = qs.value(QStringLiteral("colorPicker/recent")).toStringList();
    const QString key = c.alpha() < 255 ? c.name(QColor::HexArgb) : c.name();
    list.removeAll(key);
    list.prepend(key);
    while (list.size() > 8) list.removeLast();
    qs.setValue(QStringLiteral("colorPicker/recent"), list);
}

void ColorPopover::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) { finish(Cancelled); return; }
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)) { finish(m_current == m_initial ? Cancelled : Accepted); return; }
    QWidget::keyPressEvent(e);
}

void ColorPopover::hideEvent(QHideEvent* e)
{
    // clicar fora fecha o popup: confirma o que estiver escolhido
    if (!m_finished) {
        m_finished = true;
        const bool changed = m_current != m_initial;
        if (changed && !m_current.isTheme()) rememberRecent(m_current.color);
        emit finished(changed ? m_current : m_initial, changed ? Accepted : Cancelled);
    }
    QWidget::hideEvent(e);
}

void ColorPopover::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF panel = QRectF(m_panel->geometry());
    const qreal rad = Theme::panelRadius();
    // sombra macia (camadas)
    for (int i = 0; i < 12; ++i) {
        const qreal g = 1.5 * i;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, int(10 - i * 0.7)));
        p.drawRoundedRect(panel.adjusted(-g, -g + 6, g, g + 8), rad + g, rad + g);
    }
    p.setPen(QPen(col(Theme::panelBorder()), 1));
    p.setBrush(col(Theme::panelBackground()));
    p.drawRoundedRect(panel.adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
}

void ColorPopover::keepOnScreen()
{
    QScreen* scr = QGuiApplication::screenAt(m_anchor);
    if (!scr) scr = QGuiApplication::primaryScreen();
    const QRect avail = scr->availableGeometry();
    QPoint tl(m_anchor.x() - kShadow + 6, m_anchor.y() - kShadow + 16);
    if (tl.x() + width() > avail.right()) tl.setX(avail.right() - width());
    if (tl.y() + height() > avail.bottom()) tl.setY(std::max(avail.top(), m_anchor.y() - height() + kShadow - 8));
    if (tl.x() < avail.left()) tl.setX(avail.left());
    if (tl.y() < avail.top()) tl.setY(avail.top());
    move(tl);
}

void ColorPopover::popupAt(const QPoint& globalAnchor)
{
    m_anchor = globalAnchor;
    adjustSize();
    keepOnScreen();
    setWindowOpacity(0.0);
    show();
    raise();
    activateWindow();
    auto* a = new QVariantAnimation(this);
    a->setDuration(140);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setEasingCurve(QEasingCurve::OutCubic);
    connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { setWindowOpacity(v.toDouble()); });
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

QColor ColorPopover::getColor(const QColor& initial, QWidget* parent, const QString& title,
                              QColorDialog::ColorDialogOptions options)
{
    Options opt;
    opt.title = title;
    opt.alpha = options.testFlag(QColorDialog::ShowAlphaChannel);
    Choice start{ initial.isValid() ? initial : QColor(QStringLiteral("#6c8ebf")), QString() };
    auto* pop = new ColorPopover(start, opt, parent);
    QColor result;
    QEventLoop loop;
    connect(pop, &ColorPopover::finished, &loop, [&](const Choice& c, Result r) {
        if (r == Accepted) {
            result = c.color;
            if (!opt.alpha) result.setAlpha(255);
        }
        loop.quit();
    });
    pop->popupAt(QCursor::pos());
    loop.exec();
    return result;
}
