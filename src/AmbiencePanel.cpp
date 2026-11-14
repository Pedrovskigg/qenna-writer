#include "AmbiencePanel.h"
#include "AmbienceArt.h"
#include "AnchorUtils.h"
#include "CarouselViewport.h"
#include "Theme.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QCursor>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>

using Mgr = AmbienceManager;

namespace {
constexpr int kPanelWidth = 316;
constexpr int kTileH = 78;
constexpr int kListHeight = 196;
constexpr int kGapBelowAnchor = 6;

QColor C(const QString& css) { return Theme::toColor(css); }
QColor ink() { return C(Theme::textPrimary()); }
QColor mut() { return C(Theme::textMuted()); }
QColor acc() { return C(Theme::accentDefault()); }
QColor onAcc() { return Theme::readableTextOn(acc()); }
QColor line() { return C(Theme::panelBorder()); }
QColor hov() { return C(Theme::hoverOverlay()); }

enum class Icon { None, Play, Pause, Back, Fwd, Folder, Timer, Close, Layers, Check, Plus };

void drawIcon(QPainter& p, const QRectF& b, Icon ic, const QColor& c)
{
    if (ic == Icon::None) return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(b.topLeft());
    p.scale(b.width() / 24.0, b.height() / 24.0);
    QPen pen(c, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    switch (ic) {
    case Icon::Play: {
        QPainterPath t(QPointF(7.5, 4.5));
        t.lineTo(19, 12);
        t.lineTo(7.5, 19.5);
        t.closeSubpath();
        p.fillPath(t, c);
        break;
    }
    case Icon::Pause:
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(QRectF(6, 4.5, 4, 15), 1, 1);
        p.drawRoundedRect(QRectF(14, 4.5, 4, 15), 1, 1);
        break;
    case Icon::Back: {
        const QPointF pts[] = {{14.5, 6}, {8.5, 12}, {14.5, 18}};
        p.drawPolyline(pts, 3);
        break;
    }
    case Icon::Fwd: {
        const QPointF pts[] = {{9.5, 6}, {15.5, 12}, {9.5, 18}};
        p.drawPolyline(pts, 3);
        break;
    }
    case Icon::Folder: {
        const QPointF pts[] = {{3.5, 6}, {9.5, 6}, {11.5, 8}, {20.5, 8}, {20.5, 18.5}, {3.5, 18.5}};
        p.drawPolygon(pts, 6);
        break;
    }
    case Icon::Timer:
        p.drawEllipse(QPointF(12, 13), 7.2, 7.2);
        p.drawLine(QPointF(12, 9.5), QPointF(12, 13));
        p.drawLine(QPointF(12, 13), QPointF(14.4, 14.8));
        p.drawLine(QPointF(9.5, 3.5), QPointF(14.5, 3.5));
        break;
    case Icon::Close:
        p.drawLine(QPointF(6.5, 6.5), QPointF(17.5, 17.5));
        p.drawLine(QPointF(17.5, 6.5), QPointF(6.5, 17.5));
        break;
    case Icon::Layers: {
        pen.setWidthF(1.6);
        p.setPen(pen);
        const QPointF top[] = {{12, 4}, {20.5, 8.5}, {12, 13}, {3.5, 8.5}};
        p.drawPolygon(top, 4);
        const QPointF m[] = {{3.5, 12.5}, {12, 17}, {20.5, 12.5}};
        p.drawPolyline(m, 3);
        const QPointF b2[] = {{3.5, 16}, {12, 20.5}, {20.5, 16}};
        p.drawPolyline(b2, 3);
        break;
    }
    case Icon::Check: {
        pen.setWidthF(2.8);
        p.setPen(pen);
        const QPointF pts[] = {{5.5, 12.5}, {9.7, 16.7}, {18.5, 7.3}};
        p.drawPolyline(pts, 3);
        break;
    }
    case Icon::Plus:
        pen.setWidthF(2.2);
        p.setPen(pen);
        p.drawLine(QPointF(12, 5), QPointF(12, 19));
        p.drawLine(QPointF(5, 12), QPointF(19, 12));
        break;
    case Icon::None:
        break;
    }
    p.restore();
}

// Equalizador de 4 barrinhas (14×12): mexe tocando, para no pause.
void drawEq(QPainter& p, const QRectF& r, const QColor& c, bool playing, double t)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    const double bw = 2.0, gap = (r.width() - 4 * bw) / 3.0;
    for (int i = 0; i < 4; ++i) {
        const double f = playing ? 0.2 + 0.8 * std::abs(std::sin(t * 3.4 + i * 1.3)) : 0.45;
        const double h = r.height() * f;
        p.drawRoundedRect(QRectF(r.left() + i * (bw + gap), r.bottom() - h, bw, h), 1, 1);
    }
    p.restore();
}

QFont px(const QFont& base, int size, bool bold = false)
{
    QFont f = base;
    f.setPixelSize(size);
    f.setWeight(bold ? QFont::DemiBold : QFont::Normal);
    return f;
}

void paintArtRounded(QPainter& p, const QRectF& r, double radius, const Mgr::Track& t, double time, bool ellipse = false)
{
    p.save();
    QPainterPath clip;
    if (ellipse) clip.addEllipse(r);
    else clip.addRoundedRect(r, radius, radius);
    p.setClipPath(clip, Qt::IntersectClip);
    AmbienceArt::paint(p, r, t, time);
    p.restore();
}

QString defaultRepId(Mgr::Category c)
{
    switch (c) {
    case Mgr::Nature: return QStringLiteral("Birds_In_Forest");
    case Mgr::RainSea: return QStringLiteral("Light_Rain");
    case Mgr::Places: return QStringLiteral("Urban_City");
    case Mgr::Calm: return QStringLiteral("LogicMoon_-_The_Lighthouse");
    case Mgr::Adventure: return QStringLiteral("Ebunny - Pirate Adventure");
    case Mgr::Suspense: return QStringLiteral("Beyond_The_Grave");
    case Mgr::Mine: break;
    }
    return QString();
}

QVector<const Mgr::Track*> activeIn(const Mgr* m, Mgr::Category c)
{
    QVector<const Mgr::Track*> out;
    for (const Mgr::MixItem& it : m->mix()) {
        const Mgr::Track* t = m->track(it.id);
        if (t && t->category == c) out.append(t);
    }
    return out;
}

QString soundsCount(int n)
{
    return n == 1 ? AmbiencePanel::tr("1 som") : AmbiencePanel::tr("%1 sons").arg(n);
}
} // namespace

namespace AmbienceUi {

class ElideLabel : public QLabel {
public:
    using QLabel::QLabel;
    QSize minimumSizeHint() const override { return QSize(0, QLabel::minimumSizeHint().height()); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setPen(ink());
        p.setFont(font());
        p.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter,
                   fontMetrics().elidedText(text(), Qt::ElideRight, width()));
    }
};

class Btn : public QAbstractButton {
public:
    enum Look { Flat, Accent, Outline, Pill, Link };
    Btn(Look look, Icon icon, const QString& text, QWidget* parent)
        : QAbstractButton(parent), m_look(look), m_icon(icon)
    {
        setText(text);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover, true);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    void setIconKind(Icon i) { m_icon = i; update(); }
    void setOn(bool on) { m_on = on; update(); }
    void setStrong(bool s) { m_strong = s; update(); }
    // Pílula com × no hover (mistura do usuário) e duplo clique.
    std::function<void()> onClose;
    std::function<void()> onDouble;
    QSize sizeHint() const override
    {
        if (m_look == Accent) return {34, 34};
        const QFontMetrics fm(textFont());
        const int tw = text().isEmpty() ? 0 : fm.horizontalAdvance(text());
        if (m_look == Flat) return {std::max(26, tw ? tw + 10 : 26), 26};
        const int icon = m_icon != Icon::None ? 14 : 0;
        const int pad = m_look == Link ? 5 : 10;
        const int close = onClose ? 12 : 0;
        return {pad * 2 + icon + (icon && tw ? 5 : 0) + tw + close, m_look == Pill ? 24 : 26};
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    QRectF closeRect() const { return QRectF(width() - 20, (height() - 12) / 2.0, 12, 12); }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (onClose && isDown() && closeRect().adjusted(-3, -4, 3, 4).contains(e->position())) {
            setDown(false);
            e->accept();
            auto cb = onClose;
            cb();
            return;
        }
        QAbstractButton::mouseReleaseEvent(e);
    }
    void mouseDoubleClickEvent(QMouseEvent* e) override
    {
        if (onDouble) { auto cb = onDouble; cb(); return; }
        QAbstractButton::mouseDoubleClickEvent(e);
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        QAbstractButton::mouseMoveEvent(e);
        if (onClose) update(); // o × acende quando o mouse chega nele
    }
    QFont textFont() const
    {
        switch (m_look) {
        case Outline: return px(font(), 12, true);
        case Pill: return px(font(), 11);
        case Flat: return px(font(), 11, true);
        case Link: return px(font(), m_strong ? 13 : 12, m_strong);
        case Accent: break;
        }
        return px(font(), 12);
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const bool h = underMouse();
        const QRectF r = rect();
        if (m_look == Accent) {
            QColor a = acc();
            if (h) a = a.lighter(110);
            p.setPen(Qt::NoPen);
            p.setBrush(a);
            p.drawEllipse(r.adjusted(0.5, 0.5, -0.5, -0.5));
            drawIcon(p, QRectF(r.center().x() - 8, r.center().y() - 8, 16, 16), m_icon, onAcc());
            return;
        }
        if (m_look == Flat) {
            if (h) { p.setPen(Qt::NoPen); p.setBrush(hov()); p.drawRoundedRect(r, 7, 7); }
            if (!text().isEmpty()) {
                p.setFont(textFont());
                p.setPen(m_on ? acc() : (h ? ink() : mut()));
                p.drawText(r, Qt::AlignCenter, text());
            } else {
                drawIcon(p, QRectF(r.center().x() - 8, r.center().y() - 8, 16, 16), m_icon,
                         m_on ? acc() : (h ? ink() : mut()));
            }
            return;
        }
        QColor fg;
        if (m_look == Outline || m_look == Pill) {
            const double rad = m_look == Pill ? r.height() / 2 : 8;
            if (m_on) {
                p.setPen(Qt::NoPen);
                p.setBrush(ink());
                fg = C(Theme::panelBackground());
                fg.setAlpha(255);
            } else {
                p.setPen(QPen(h ? mut() : line(), 1));
                p.setBrush(Qt::NoBrush);
                fg = (m_look == Outline || h) ? ink() : mut();
            }
            p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
        } else { // Link
            if (h) { p.setPen(Qt::NoPen); p.setBrush(hov()); p.drawRoundedRect(r, 6, 6); }
            fg = (m_strong || h) ? ink() : mut();
        }
        const int pad = m_look == Link ? 5 : 10;
        double x = pad;
        if (m_icon != Icon::None) {
            drawIcon(p, QRectF(x, r.center().y() - 7, 14, 14), m_icon, fg);
            x += 14 + (text().isEmpty() ? 0 : 5);
        }
        p.setFont(textFont());
        p.setPen(fg);
        p.drawText(QRectF(x, 0, r.width() - x, r.height()), Qt::AlignLeft | Qt::AlignVCenter, text());
        if (onClose && h) {
            const QRectF cr = closeRect();
            const bool overX = cr.adjusted(-3, -4, 3, 4).contains(mapFromGlobal(QCursor::pos()));
            drawIcon(p, cr, Icon::Close, overX ? acc() : fg);
        }
    }

private:
    Look m_look;
    Icon m_icon;
    bool m_on = false;
    bool m_strong = false;
};

class Tile : public QAbstractButton {
public:
    Tile(AmbiencePanel* panel, Mgr::Category cat, QWidget* parent)
        : QAbstractButton(parent), m_panel(panel), m_cat(cat)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover, true);
        setFixedHeight(kTileH);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    bool isActive() const { return !activeIn(m_panel->manager(), m_cat).isEmpty(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = rect();
        const Mgr* m = m_panel->manager();
        const auto act = activeIn(m, m_cat);
        QPainterPath clip;
        clip.addRoundedRect(r, 10, 10);
        p.setClipPath(clip);

        const Mgr::Track* rep = act.isEmpty() ? m->track(defaultRepId(m_cat)) : act.first();
        if (!rep) {
            const auto all = m->tracksIn(m_cat);
            if (!all.isEmpty()) rep = all.first();
        }
        if (rep) AmbienceArt::paint(p, r, *rep, act.isEmpty() ? 0.0 : m_panel->animTime());
        else p.fillRect(r, hov());

        QLinearGradient g(0, r.height() * 0.38, 0, r.height());
        g.setColorAt(0, QColor(0, 0, 0, 0));
        g.setColorAt(1, QColor(0, 0, 0, 170));
        p.fillRect(r, g);
        if (underMouse()) p.fillRect(r, QColor(255, 255, 255, 18));

        p.setPen(Qt::white);
        p.setFont(px(font(), 12, true));
        p.drawText(QRectF(8, r.height() - 34, r.width() - 14, 18), Qt::AlignLeft | Qt::AlignBottom,
                   Mgr::categoryName(m_cat));
        QString sub;
        if (act.isEmpty()) {
            sub = soundsCount(m->tracksIn(m_cat).size());
        } else {
            QStringList names;
            for (const auto* t : act) names << t->name;
            sub = names.join(QStringLiteral(", "));
        }
        const QFont sf = px(font(), 10);
        p.setFont(sf);
        p.setPen(QColor(255, 255, 255, 210));
        p.drawText(QRectF(8, r.height() - 17, r.width() - 14, 13), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(sf).elidedText(sub, Qt::ElideRight, int(r.width()) - 14));

        if (!act.isEmpty()) {
            const QString cnt = act.size() > 1 ? QString::number(act.size()) : QString();
            const QFont cf = px(font(), 10, true);
            const int cw = cnt.isEmpty() ? 0 : QFontMetrics(cf).horizontalAdvance(cnt) + 4;
            const QRectF badge(r.width() - 6 - (22 + cw), 6, 22 + cw, 18);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 120));
            p.drawRoundedRect(badge, 6, 6);
            drawEq(p, QRectF(badge.left() + 5, badge.top() + 3.5, 12, 11), Qt::white,
                   m->isPlaying(), m_panel->animTime());
            if (!cnt.isEmpty()) {
                p.setFont(cf);
                p.setPen(Qt::white);
                p.drawText(QRectF(badge.left() + 19, badge.top(), cw, badge.height()), Qt::AlignCenter, cnt);
            }
            p.setClipping(false);
            p.setPen(QPen(acc(), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 9, 9);
        }
    }

private:
    AmbiencePanel* m_panel;
    Mgr::Category m_cat;
};

// "Suas músicas": aparece só quando o usuário largou arquivo próprio na pasta.
class MineTile : public QAbstractButton {
public:
    MineTile(AmbiencePanel* panel, QWidget* parent) : QAbstractButton(parent), m_panel(panel)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover, true);
        setFixedHeight(36);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
        const Mgr* m = m_panel->manager();
        const auto act = activeIn(m, Mgr::Mine);
        const auto all = m->tracksIn(Mgr::Mine);
        p.setPen(QPen(act.isEmpty() ? line() : acc(), act.isEmpty() ? 1 : 2));
        p.setBrush(underMouse() ? hov() : QColor(Qt::transparent));
        p.drawRoundedRect(r, 10, 10);
        const Mgr::Track* rep = act.isEmpty() ? (all.isEmpty() ? nullptr : all.first()) : act.first();
        if (rep) paintArtRounded(p, QRectF(7, 7, 22, 22), 5, *rep, act.isEmpty() ? 0 : m_panel->animTime());
        p.setPen(ink());
        p.setFont(px(font(), 12, true));
        p.drawText(QRectF(38, 0, r.width() - 120, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   Mgr::categoryName(Mgr::Mine));
        double x = r.width() - 26;
        drawIcon(p, QRectF(x, r.center().y() - 7, 14, 14), Icon::Fwd, mut());
        p.setFont(px(font(), 11));
        p.setPen(mut());
        const QString cnt = soundsCount(all.size());
        const int cw = QFontMetrics(p.font()).horizontalAdvance(cnt);
        x -= cw + 6;
        p.drawText(QRectF(x, 0, cw, r.height()), Qt::AlignVCenter, cnt);
        if (!act.isEmpty())
            drawEq(p, QRectF(x - 20, r.center().y() - 6, 14, 12), acc(), m->isPlaying(), m_panel->animTime());
    }
private:
    AmbiencePanel* m_panel;
};

class Row : public QAbstractButton {
public:
    Row(AmbiencePanel* panel, const QString& id, QWidget* parent)
        : QAbstractButton(parent), m_panel(panel), m_id(id)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover, true);
        setFixedHeight(40);
    }
    const QString& id() const { return m_id; }
    bool isOn() const { return m_panel->manager()->inMix(m_id); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        const Mgr* m = m_panel->manager();
        const Mgr::Track* t = m->track(m_id);
        if (!t) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const bool on = m->inMix(m_id);
        const bool h = underMouse();
        const QRectF r = rect().adjusted(0, 1, 0, -1);
        if (on || h) {
            QColor bg = on ? acc() : hov();
            if (on) bg.setAlpha(h ? 52 : 38);
            p.setPen(Qt::NoPen);
            p.setBrush(bg);
            p.drawRoundedRect(r, 8, 8);
        }
        paintArtRounded(p, QRectF(6, r.center().y() - 14, 28, 28), 6, *t, on ? m_panel->animTime() : 0);

        const double d = 22;
        const QRectF tg(r.width() - 8 - d, r.center().y() - d / 2, d, d);
        if (on) {
            p.setPen(Qt::NoPen);
            p.setBrush(acc());
            p.drawEllipse(tg);
            drawIcon(p, tg.adjusted(5, 5, -5, -5), Icon::Check, onAcc());
        } else {
            p.setPen(QPen(h ? mut() : line(), 1));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(tg.adjusted(0.5, 0.5, -0.5, -0.5));
            drawIcon(p, tg.adjusted(6, 6, -6, -6), t->music ? Icon::Play : Icon::Plus, h ? ink() : mut());
        }
        double right = tg.left() - 8;
        if (on) {
            drawEq(p, QRectF(right - 14, r.center().y() - 6, 14, 12), acc(), m->isPlaying(), m_panel->animTime());
            right -= 22;
        }
        const double x = 44;
        const QFont tf = px(font(), 12, on);
        const QString title = QFontMetrics(tf).elidedText(t->name, Qt::ElideRight, int(right - x));
        p.setFont(tf);
        p.setPen(on ? acc() : ink());
        if (t->author.isEmpty()) {
            p.drawText(QRectF(x, r.top(), right - x, r.height()), Qt::AlignLeft | Qt::AlignVCenter, title);
        } else {
            p.drawText(QRectF(x, r.top() + 4, right - x, 17), Qt::AlignLeft | Qt::AlignVCenter, title);
            p.setFont(px(font(), 10));
            p.setPen(mut());
            p.drawText(QRectF(x, r.top() + 20, right - x, 14), Qt::AlignLeft | Qt::AlignVCenter, t->author);
        }
    }
private:
    AmbiencePanel* m_panel;
    QString m_id;
};

// A mistura empilhada na barra: bolinhas sobrepostas, a primeira embaixo.
class Stack : public QWidget {
public:
    Stack(AmbiencePanel* panel, QWidget* parent) : QWidget(parent), m_panel(panel)
    {
        setFixedHeight(28);
        refit();
    }
    void refit()
    {
        const int n = std::max<qsizetype>(1, m_panel->manager()->mix().size());
        setFixedWidth(28 + (n - 1) * 13);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const Mgr* m = m_panel->manager();
        const auto& mix = m->mix();
        if (mix.isEmpty()) {
            QPen pen(line(), 1, Qt::DashLine);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QRectF(1.5, 1.5, 25, 25));
            return;
        }
        for (int i = 0; i < mix.size(); ++i) {
            const Mgr::Track* t = m->track(mix[i].id);
            if (!t) continue;
            const QRectF cr(1 + i * 13, 1, 26, 26);
            // Anel com a cor do painel separa uma bolinha da outra.
            p.setPen(Qt::NoPen);
            QColor ring = C(Theme::panelBackground());
            ring.setAlpha(255);
            p.setBrush(ring);
            p.drawEllipse(cr.adjusted(-1, -1, 1, 1));
            paintArtRounded(p, cr, 13, *t, m_panel->animTime(), true);
        }
    }
private:
    AmbiencePanel* m_panel;
};

class ArtThumb : public QWidget {
public:
    ArtThumb(AmbiencePanel* panel, const QString& id, int size, QWidget* parent)
        : QWidget(parent), m_panel(panel), m_id(id)
    {
        setFixedSize(size, size);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        const Mgr::Track* t = m_panel->manager()->track(m_id);
        if (!t) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        paintArtRounded(p, rect(), 6, *t, m_panel->animTime());
    }
private:
    AmbiencePanel* m_panel;
    QString m_id;
};

} // namespace AmbienceUi

using namespace AmbienceUi;

AmbiencePanel::AmbiencePanel(AmbienceManager* manager, QWidget* parent)
    : QFrame(parent)
    , m_manager(manager)
{
    setObjectName(QStringLiteral("ambiencePanel"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setFocusPolicy(Qt::ClickFocus);

    m_anim = new QTimer(this);
    m_anim->setInterval(33);
    connect(m_anim, &QTimer::timeout, this, &AmbiencePanel::tick);
    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, this, [this]() { if (m_toast) m_toast->hide(); });

    buildUi();
    applyTheme();

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);

    if (m_manager) {
        connect(m_manager, &AmbienceManager::tracksChanged, this, [this]() {
            if (m_mineTile) m_mineTile->setVisible(!m_manager->tracksIn(Mgr::Mine).isEmpty());
            refreshTiles();
            if (m_view == View::Category) rebuildCategoryRows();
            if (m_view == View::Mesa) rebuildPresets();
            relayout();
        });
        connect(m_manager, &AmbienceManager::mixChanged, this, [this]() {
            refreshTiles();
            for (Row* r : m_rows) r->update();
            refreshBar();
            if (m_view == View::Mesa) { rebuildLayers(); refreshPresetState(); }
            updateAnimTimer();
        });
        connect(m_manager, &AmbienceManager::presetsChanged, this, [this]() {
            if (m_view == View::Mesa) { rebuildPresets(m_lastSavedName); relayout(); }
            m_lastSavedName.clear();
        });
        connect(m_manager, &AmbienceManager::playbackStateChanged, this, [this](bool) {
            refreshPlaying();
        });
        connect(m_manager, &AmbienceManager::volumeChanged, this, [this](int v) {
            if (m_master && m_master->value() != v) {
                const QSignalBlocker b(m_master);
                m_master->setValue(v);
            }
            if (m_masterNum) m_masterNum->setText(QString::number(v));
        });
        connect(m_manager, &AmbienceManager::sleepTimerChanged, this, [this](int) { refreshTimer(); });
    }

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &AmbiencePanel::applyTheme);

    setView(View::Grid);
    refreshBar();
    refreshPlaying();
    refreshTimer();

    hide();
    qApp->installEventFilter(this);
}

void AmbiencePanel::buildUi()
{
    m_root = new QVBoxLayout(this);
    m_root->setContentsMargins(12, 11, 12, 12);
    m_root->setSpacing(10);
    m_root->setSizeConstraint(QLayout::SetFixedSize);

    m_gridPage = buildGridPage();
    m_catPage = buildCategoryPage();
    m_mesaPage = buildMesaPage();
    m_root->addWidget(m_gridPage);
    m_root->addWidget(m_catPage);
    m_root->addWidget(m_mesaPage);
    m_root->addWidget(buildBar());

    m_toast = new QLabel(this);
    m_toast->setObjectName(QStringLiteral("ambToast"));
    m_toast->setAlignment(Qt::AlignCenter);
    m_toast->setTextFormat(Qt::RichText);
    m_toast->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    connect(m_toast, &QLabel::linkActivated, this, [this](const QString&) {
        auto undo = std::move(m_undo);
        m_undo = nullptr;
        m_toast->hide();
        if (undo) undo();
    });
    m_toast->hide();
}

static QWidget* fixedWidthPage(QWidget* parent)
{
    auto* w = new QWidget(parent);
    w->setFixedWidth(kPanelWidth - 24);
    return w;
}

QWidget* AmbiencePanel::buildGridPage()
{
    auto* page = fixedWidthPage(this);
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(10);

    auto* hd = new QHBoxLayout();
    hd->setContentsMargins(0, 0, 0, 0);
    auto* title = new QLabel(tr("Som ambiente"), page);
    title->setObjectName(QStringLiteral("ambHeader"));
    hd->addWidget(title);
    hd->addStretch(1);
    auto* folder = new Btn(Btn::Link, Icon::Folder, tr("Abrir pasta"), page);
    folder->setToolTip(m_manager ? m_manager->tracksDirectory() : QString());
    connect(folder, &QAbstractButton::clicked, this, [this, folder]() {
        if (!m_manager) return;
        folder->setToolTip(m_manager->tracksDirectory());
        if (m_manager->tracksDirectoryExists())
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_manager->tracksDirectory()));
        else
            showToast(tr("Pasta de sons não encontrada"));
    });
    hd->addWidget(folder);
    v->addLayout(hd);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(6);
    const Mgr::Category cats[] = {Mgr::Nature, Mgr::RainSea, Mgr::Places,
                                  Mgr::Calm, Mgr::Adventure, Mgr::Suspense};
    for (int i = 0; i < 6; ++i) {
        auto* tile = new Tile(this, cats[i], page);
        const Mgr::Category c = cats[i];
        connect(tile, &QAbstractButton::clicked, this, [this, c]() { setView(View::Category, c); });
        grid->addWidget(tile, i / 3, i % 3);
        m_tiles.append(tile);
    }
    v->addLayout(grid);

    m_mineTile = new MineTile(this, page);
    connect(m_mineTile, &QAbstractButton::clicked, this, [this]() { setView(View::Category, Mgr::Mine); });
    m_mineTile->setVisible(m_manager && !m_manager->tracksIn(Mgr::Mine).isEmpty());
    v->addWidget(m_mineTile);
    return page;
}

QWidget* AmbiencePanel::buildCategoryPage()
{
    auto* page = fixedWidthPage(this);
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);

    auto* hd = new QHBoxLayout();
    hd->setContentsMargins(0, 0, 0, 0);
    m_catBack = new Btn(Btn::Link, Icon::Back, QString(), page);
    m_catBack->setStrong(true);
    connect(m_catBack, &QAbstractButton::clicked, this, [this]() { setView(View::Grid); });
    hd->addWidget(m_catBack);
    hd->addStretch(1);
    m_catCount = new QLabel(page);
    m_catCount->setObjectName(QStringLiteral("ambSub"));
    hd->addWidget(m_catCount);
    v->addLayout(hd);

    m_catHint = new QLabel(page);
    m_catHint->setObjectName(QStringLiteral("ambHint"));
    m_catHint->setWordWrap(true);
    v->addWidget(m_catHint);

    m_catScroll = new QScrollArea(page);
    m_catScroll->setObjectName(QStringLiteral("ambScroll"));
    m_catScroll->setFixedHeight(kListHeight);
    m_catScroll->setWidgetResizable(true);
    m_catScroll->setFrameShape(QFrame::NoFrame);
    m_catScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_catScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    m_catList = new QWidget(m_catScroll);
    m_catList->setObjectName(QStringLiteral("ambList"));
    auto* lv = new QVBoxLayout(m_catList);
    lv->setContentsMargins(0, 0, 4, 0);
    lv->setSpacing(0);
    lv->addStretch(1);
    m_catScroll->setWidget(m_catList);
    v->addWidget(m_catScroll);
    return page;
}

QWidget* AmbiencePanel::buildMesaPage()
{
    auto* page = fixedWidthPage(this);
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    auto* hd = new QHBoxLayout();
    hd->setContentsMargins(0, 0, 0, 0);
    auto* back = new Btn(Btn::Link, Icon::Back, tr("Mesa"), page);
    back->setStrong(true);
    connect(back, &QAbstractButton::clicked, this, [this]() { setView(View::Grid); });
    hd->addWidget(back);
    hd->addStretch(1);
    m_timerBtn = new Btn(Btn::Flat, Icon::Timer, QString(), page);
    connect(m_timerBtn, &QAbstractButton::clicked, this, [this]() {
        if (m_manager) m_manager->cycleSleepTimer();
    });
    hd->addWidget(m_timerBtn);
    v->addLayout(hd);

    auto* caption = new QHBoxLayout();
    caption->setContentsMargins(0, 0, 0, 0);
    auto* prontas = new QLabel(tr("Misturas"), page);
    prontas->setObjectName(QStringLiteral("ambHint"));
    caption->addWidget(prontas);
    caption->addStretch(1);
    m_saveBtn = new Btn(Btn::Link, Icon::Plus, tr("Salvar esta mistura"), page);
    connect(m_saveBtn, &QAbstractButton::clicked, this, [this]() { startNaming(QString()); });
    caption->addWidget(m_saveBtn);
    v->addLayout(caption);

    m_nameRow = new QWidget(page);
    auto* nh = new QHBoxLayout(m_nameRow);
    nh->setContentsMargins(0, 0, 0, 0);
    nh->setSpacing(6);
    m_nameEdit = new QLineEdit(m_nameRow);
    m_nameEdit->setObjectName(QStringLiteral("ambName"));
    m_nameEdit->setMaxLength(40);
    m_nameEdit->setPlaceholderText(tr("Nome da mistura"));
    connect(m_nameEdit, &QLineEdit::returnPressed, this, [this]() { finishNaming(true); });
    nh->addWidget(m_nameEdit, 1);
    auto* ok = new Btn(Btn::Pill, Icon::None, tr("Salvar"), m_nameRow);
    connect(ok, &QAbstractButton::clicked, this, [this]() { finishNaming(true); });
    nh->addWidget(ok);
    auto* cancel = new Btn(Btn::Flat, Icon::Close, QString(), m_nameRow);
    cancel->setToolTip(tr("Cancelar"));
    connect(cancel, &QAbstractButton::clicked, this, [this]() { finishNaming(false); });
    nh->addWidget(cancel);
    m_nameRow->hide();
    v->addWidget(m_nameRow);

    m_presetBox = new QWidget(page);
    auto* ph = new QHBoxLayout(m_presetBox);
    ph->setContentsMargins(0, 0, 0, 0);
    ph->setSpacing(6);
    m_presetCarousel = new CarouselViewport(Qt::Horizontal, page);
    m_presetCarousel->setContent(m_presetBox);
    m_presetCarousel->setFixedWidth(kPanelWidth - 24);
    v->addWidget(m_presetCarousel);

    m_layersBox = new QWidget(page);
    auto* lv = new QVBoxLayout(m_layersBox);
    lv->setContentsMargins(0, 4, 0, 0);
    lv->setSpacing(6);
    v->addWidget(m_layersBox);

    m_mesaEmpty = new QLabel(tr("A mesa está vazia. Escolha uma mistura pronta ou abra um cartão."), page);
    m_mesaEmpty->setObjectName(QStringLiteral("ambEmpty"));
    m_mesaEmpty->setWordWrap(true);
    m_mesaEmpty->setAlignment(Qt::AlignCenter);
    v->addWidget(m_mesaEmpty);

    auto* mh = new QHBoxLayout();
    mh->setContentsMargins(0, 2, 0, 0);
    mh->setSpacing(8);
    auto* ml = new QLabel(tr("Volume geral"), page);
    ml->setObjectName(QStringLiteral("ambSub"));
    mh->addWidget(ml);
    m_master = new QSlider(Qt::Horizontal, page);
    m_master->setObjectName(QStringLiteral("ambVolume"));
    m_master->setRange(0, 100);
    m_master->setPageStep(10);
    m_master->setFocusPolicy(Qt::NoFocus);
    m_master->setValue(m_manager ? m_manager->volumePercent() : 45);
    connect(m_master, &QSlider::valueChanged, this, [this](int v) {
        if (m_manager) m_manager->setVolumePercent(v);
    });
    mh->addWidget(m_master, 1);
    m_masterNum = new QLabel(QString::number(m_master->value()), page);
    m_masterNum->setObjectName(QStringLiteral("ambSub"));
    m_masterNum->setFixedWidth(24);
    m_masterNum->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mh->addWidget(m_masterNum);
    v->addLayout(mh);
    return page;
}

QWidget* AmbiencePanel::buildBar()
{
    auto* bar = new QFrame(this);
    bar->setObjectName(QStringLiteral("ambBar"));
    bar->setFixedWidth(kPanelWidth - 24);
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(10, 7, 7, 7);
    h->setSpacing(8);
    m_stack = new Stack(this, bar);
    h->addWidget(m_stack);
    auto* meta = new QVBoxLayout();
    meta->setContentsMargins(0, 0, 0, 0);
    meta->setSpacing(0);
    m_barTitle = new ElideLabel(bar);
    m_barTitle->setObjectName(QStringLiteral("ambBarTitle"));
    meta->addWidget(m_barTitle);
    m_barSub = new QLabel(bar);
    m_barSub->setObjectName(QStringLiteral("ambSub"));
    meta->addWidget(m_barSub);
    h->addLayout(meta, 1);
    m_barPlay = new Btn(Btn::Accent, Icon::Play, QString(), bar);
    connect(m_barPlay, &QAbstractButton::clicked, this, [this]() {
        if (!m_manager) return;
        if (m_manager->mix().isEmpty()) {
            showToast(tr("Escolha um som primeiro."));
            return;
        }
        m_manager->togglePlay();
    });
    h->addWidget(m_barPlay);
    m_barMesa = new Btn(Btn::Outline, Icon::Layers, tr("Mesa"), bar);
    m_barMesa->setToolTip(tr("Volume de cada som, misturas prontas e desligar sozinho"));
    connect(m_barMesa, &QAbstractButton::clicked, this, [this]() {
        setView(m_view == View::Mesa ? View::Grid : View::Mesa);
    });
    h->addWidget(m_barMesa);
    return bar;
}

void AmbiencePanel::setView(View v, AmbienceManager::Category cat)
{
    if (v != View::Mesa && m_nameRow && m_nameRow->isVisible()) {
        m_renaming.clear();
        m_nameRow->hide();
    }
    m_view = v;
    m_cat = cat;
    m_gridPage->setVisible(v == View::Grid);
    m_catPage->setVisible(v == View::Category);
    m_mesaPage->setVisible(v == View::Mesa);
    if (m_barMesa) m_barMesa->setOn(v == View::Mesa);
    if (v == View::Category) rebuildCategoryRows();
    if (v == View::Mesa) { rebuildPresets(); rebuildLayers(); }
    if (v == View::Grid) refreshTiles();
    updateAnimTimer();
    relayout();
}

void AmbiencePanel::rebuildCategoryRows()
{
    if (!m_manager) return;
    auto* lv = static_cast<QVBoxLayout*>(m_catList->layout());
    for (Row* r : m_rows) r->deleteLater();
    m_rows.clear();
    const auto tracks = m_manager->tracksIn(m_cat);
    int music = 0;
    for (const auto* t : tracks) {
        if (t->music) ++music;
        auto* row = new Row(this, t->id, m_catList);
        const QString id = t->id;
        connect(row, &QAbstractButton::clicked, this, [this, id]() {
            if (m_manager->toggleInMix(id) == AmbienceManager::ToggleResult::Full)
                showToast(tr("Já tem 3 sons. Tire um na Mesa."));
        });
        lv->insertWidget(lv->count() - 1, row);
        m_rows.append(row);
    }
    m_catBack->setText(Mgr::categoryName(m_cat));
    m_catBack->updateGeometry();
    m_catCount->setText(soundsCount(tracks.size()));
    if (tracks.isEmpty())
        m_catHint->setText(tr("Nenhum som aqui. Coloque arquivos .mp3 ou .ogg na pasta de sons."));
    else if (music == tracks.size())
        m_catHint->setText(tr("Uma música por vez: escolher outra troca a que está tocando."));
    else if (music == 0)
        m_catHint->setText(tr("Clique pra somar à mistura. Até 3 sons junto com a música."));
    else
        m_catHint->setText(tr("Música troca a que está tocando; som soma à mistura."));
    m_catScroll->verticalScrollBar()->setValue(0);
}

void AmbiencePanel::rebuildPresets(const QString& focusName)
{
    if (!m_manager) return;
    auto* ph = static_cast<QHBoxLayout*>(m_presetBox->layout());
    while (QLayoutItem* it = ph->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    m_presetPills.clear();
    const auto presets = m_manager->presets();
    for (const auto& pr : presets) {
        auto* b = new Btn(Btn::Pill, Icon::None, pr.name, m_presetBox);
        const auto items = pr.items;
        connect(b, &QAbstractButton::clicked, this, [this, items]() { m_manager->setMix(items); });
        if (pr.user) {
            const QString name = pr.name;
            b->setToolTip(tr("Duplo clique renomeia; o × exclui"));
            b->onClose = [this, name]() {
                QTimer::singleShot(0, this, [this, name]() {
                    AmbienceManager::Preset removed;
                    const int at = m_manager->removeUserPreset(name, &removed);
                    if (at < 0) return;
                    showToast(tr("Mistura excluída"), [this, removed, at]() {
                        m_manager->restoreUserPreset(removed, at);
                    });
                });
            };
            b->onDouble = [this, name]() { startNaming(name); };
            b->setMouseTracking(true);
            b->updateGeometry();
        }
        ph->addWidget(b);
        QStringList ids;
        for (const auto& it : pr.items) ids << it.id;
        ids.sort();
        m_presetPills.append({b, ids});
    }
    m_presetBox->adjustSize();
    m_presetCarousel->setVisible(!presets.isEmpty());
    m_presetCarousel->updateGeometry();
    refreshPresetState();
    // Traz pro miolo a mistura recém-salva/renomeada (ou a que está tocando).
    QTimer::singleShot(0, this, [this, focusName]() {
        Btn* target = nullptr;
        for (const auto& pp : m_presetPills) {
            if (!focusName.isEmpty() && pp.first->text() == focusName) { target = pp.first; break; }
        }
        if (!target && focusName.isEmpty()) {
            QStringList cur;
            for (const auto& it : m_manager->mix()) cur << it.id;
            cur.sort();
            for (const auto& pp : m_presetPills)
                if (!cur.isEmpty() && pp.second == cur) { target = pp.first; break; }
        }
        if (target) m_presetCarousel->ensureVisible(target, false);
    });
}

// Acende a pílula da mistura que está tocando agora (mesmos sons, qualquer volume).
void AmbiencePanel::refreshPresetState()
{
    if (!m_manager) return;
    QStringList cur;
    for (const auto& it : m_manager->mix()) cur << it.id;
    cur.sort();
    for (auto& pp : m_presetPills) pp.first->setOn(!cur.isEmpty() && pp.second == cur);
    if (m_saveBtn) m_saveBtn->setVisible(!cur.isEmpty() && !m_nameRow->isVisible());
}

void AmbiencePanel::startNaming(const QString& renaming)
{
    if (!m_manager) return;
    if (renaming.isEmpty() && m_manager->mix().isEmpty()) return;
    m_renaming = renaming;
    m_nameEdit->setText(renaming.isEmpty() ? m_manager->suggestPresetName() : renaming);
    m_nameRow->show();
    m_saveBtn->hide();
    relayout();
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

void AmbiencePanel::finishNaming(bool accept)
{
    if (!m_nameRow->isVisible()) return;
    const QString name = m_nameEdit->text().trimmed();
    if (accept) {
        if (name.isEmpty()) return;
        if (!m_renaming.isEmpty()) {
            m_lastSavedName = name;
            if (name.compare(m_renaming, Qt::CaseSensitive) != 0
                && !m_manager->renameUserPreset(m_renaming, name)) {
                m_lastSavedName.clear();
                showToast(tr("Já existe uma mistura com esse nome."));
                return;
            }
        } else {
            const bool existed = m_manager->hasUserPreset(name);
            m_lastSavedName = name;
            m_manager->saveUserPreset(name);
            showToast(existed ? tr("Mistura atualizada") : tr("Mistura salva"));
        }
    }
    m_renaming.clear();
    m_nameRow->hide();
    refreshPresetState();
    relayout();
}

void AmbiencePanel::rebuildLayers()
{
    if (!m_manager) return;
    auto* lv = static_cast<QVBoxLayout*>(m_layersBox->layout());
    while (QLayoutItem* it = lv->takeAt(0)) {
        if (it->widget()) {
            it->widget()->hide();
            it->widget()->deleteLater();
        }
        delete it;
    }
    for (const Mgr::MixItem& m : m_manager->mix()) {
        const Mgr::Track* t = m_manager->track(m.id);
        if (!t) continue;
        auto* row = new QFrame(m_layersBox);
        row->setObjectName(QStringLiteral("ambLayer"));
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(6, 5, 3, 5);
        h->setSpacing(8);
        h->addWidget(new ArtThumb(this, m.id, 30, row));
        auto* meta = new QVBoxLayout();
        meta->setContentsMargins(0, 0, 0, 0);
        meta->setSpacing(0);
        auto* kind = new QLabel(QStringLiteral("%1 · %2")
                                    .arg(t->music ? tr("Música") : tr("Som"),
                                         Mgr::categoryName(t->category)).toUpper(), row);
        kind->setObjectName(QStringLiteral("ambKind"));
        meta->addWidget(kind);
        auto* name = new ElideLabel(t->name, row);
        name->setObjectName(QStringLiteral("ambLayerTitle"));
        meta->addWidget(name);
        h->addLayout(meta, 1);
        auto* slider = new QSlider(Qt::Horizontal, row);
        slider->setObjectName(QStringLiteral("ambVolume"));
        slider->setRange(0, 100);
        slider->setPageStep(10);
        slider->setFocusPolicy(Qt::NoFocus);
        slider->setFixedWidth(86);
        slider->setValue(m.volume);
        slider->setToolTip(tr("Volume deste som"));
        const QString id = m.id;
        connect(slider, &QSlider::valueChanged, this, [this, id](int v) { m_manager->setLayerVolume(id, v); });
        h->addWidget(slider);
        auto* x = new Btn(Btn::Flat, Icon::Close, QString(), row);
        x->setToolTip(tr("Tirar da mistura"));
        connect(x, &QAbstractButton::clicked, this, [this, id]() {
            QTimer::singleShot(0, this, [this, id]() { m_manager->removeFromMix(id); });
        });
        h->addWidget(x);
        lv->addWidget(row);
    }
    const bool empty = m_manager->mix().isEmpty();
    m_layersBox->setVisible(!empty);
    m_mesaEmpty->setVisible(empty);
    relayout();
}

void AmbiencePanel::refreshBar()
{
    if (!m_manager || !m_barTitle) return;
    const auto& mix = m_manager->mix();
    m_stack->refit();
    m_stack->update();
    if (mix.isEmpty()) {
        m_barTitle->setText(tr("Nada tocando"));
        m_barSub->setText(tr("Escolha um cartão"));
    } else {
        const Mgr::Track* first = m_manager->track(mix.first().id);
        m_barTitle->setText(first ? first->name : QString());
        if (mix.size() > 1)
            m_barSub->setText(tr("+ %1 na mistura").arg(mix.size() - 1));
        else
            m_barSub->setText(first ? Mgr::categoryName(first->category) : QString());
    }
    m_barTitle->update();
}

void AmbiencePanel::refreshTiles()
{
    for (Tile* t : m_tiles) t->update();
    if (m_mineTile) m_mineTile->update();
}

void AmbiencePanel::refreshTimer()
{
    if (!m_timerBtn || !m_manager) return;
    const int min = m_manager->sleepMinutes();
    m_timerBtn->setText(min > 0 ? QStringLiteral("%1′").arg(min) : QString());
    m_timerBtn->setOn(min > 0);
    m_timerBtn->setToolTip(min > 0 ? tr("Desliga sozinho em %1 min (clique pra mudar)").arg(min)
                                   : tr("Desligar sozinho"));
    m_timerBtn->updateGeometry();
}

void AmbiencePanel::refreshPlaying()
{
    const bool playing = m_manager && m_manager->isPlaying();
    if (m_barPlay) {
        m_barPlay->setIconKind(playing ? Icon::Pause : Icon::Play);
        m_barPlay->setToolTip(playing ? tr("Pausar") : tr("Reproduzir"));
    }
    updateAnimTimer();
    // Equalizadores param (ou voltam) na hora.
    refreshTiles();
    for (Row* r : m_rows) if (r->isOn()) r->update();
}

void AmbiencePanel::updateAnimTimer()
{
    const bool run = isVisible() && m_manager && m_manager->isPlaying() && !m_manager->mix().isEmpty();
    if (run && !m_anim->isActive()) {
        m_clock.restart();
        m_anim->start();
    } else if (!run && m_anim->isActive()) {
        m_anim->stop();
    }
}

void AmbiencePanel::tick()
{
    m_t += std::min(0.1, m_clock.restart() / 1000.0);
    if (m_view == View::Grid) {
        for (Tile* t : m_tiles) if (t->isActive()) t->update();
        if (m_mineTile && m_mineTile->isVisible()) m_mineTile->update();
    } else if (m_view == View::Category) {
        for (Row* r : m_rows) if (r->isOn()) r->update();
    } else {
        m_layersBox->update(); // repinta os filhos (as vinhetas das camadas) junto
    }
    m_stack->update();
}

void AmbiencePanel::relayout()
{
    if (m_root) m_root->activate();
    adjustSize();
    if (isVisible() && !m_anchor.isNull()) {
        const QScreen* screen = QGuiApplication::screenAt(m_anchor.center());
        if (screen)
            move(AnchorUtils::positionNear(m_anchor, size(), m_barSide,
                                           screen->availableGeometry(), kGapBelowAnchor));
    }
    // A sombra (QGraphicsEffect) pinta de cache e não percebe filho que
    // aparece/some: sem esse repaint atrasado a tela fica na tela velha
    // (mesmo bug das folhas, ver CardRepaintWatcher no SheetDialog).
    QTimer::singleShot(40, this, [this]() { update(); });
}

void AmbiencePanel::showToast(const QString& text, std::function<void()> undo)
{
    if (!m_toast) return;
    m_undo = std::move(undo);
    if (m_undo) {
        m_toast->setText(QStringLiteral("%1&nbsp;&nbsp;<a href=\"undo\" style=\"color:%2; text-decoration:none;\">%3</a>")
                             .arg(text.toHtmlEscaped(), Theme::accentDefault(), tr("Desfazer")));
    } else {
        m_toast->setText(text.toHtmlEscaped());
    }
    m_toast->adjustSize();
    const QWidget* bar = findChild<QFrame*>(QStringLiteral("ambBar"));
    const int y = bar ? bar->y() - m_toast->height() - 8 : height() - m_toast->height() - 70;
    m_toast->move((width() - m_toast->width()) / 2, y);
    m_toast->raise();
    m_toast->show();
    m_toastTimer->start(m_undo ? 5000 : 2200);
    QTimer::singleShot(40, this, [this]() { update(); });
}

void AmbiencePanel::applyTheme()
{
    setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#ambiencePanel {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: @radius-panel;"
        "}"
        "QLabel#ambHeader { color: %3; font-size: 13px; font-weight: 600; }"
        "QLabel#ambSub { color: %4; font-size: 11px; }"
        "QLabel#ambHint { color: %4; font-size: 11px; }"
        "QLabel#ambKind { color: %4; font-size: 9px; font-weight: 600; }"
        "QLabel#ambBarTitle { font-size: 12px; font-weight: 600; }"
        "QLabel#ambLayerTitle { font-size: 12px; }"
        "QLabel#ambEmpty { color: %4; font-size: 11px; border: 1px dashed %2;"
        "  border-radius: 10px; padding: 14px; }"
        "QLabel#ambToast { background: %3; color: %7; font-size: 11px; font-weight: 600;"
        "  padding: 6px 12px; border-radius: 8px; }"
        "QFrame#ambBar, QFrame#ambLayer { background: %5; border: 1px solid %2; border-radius: 12px; }"
        "QFrame#ambLayer { border-radius: 10px; }"
        "QWidget#ambList { background: transparent; }"
        "QLineEdit#ambName { background: %5; color: %3; border: 1px solid %2; border-radius: 8px;"
        "  padding: 3px 8px; font-size: 12px; selection-background-color: %6; margin: 0; }"
        "QLineEdit#ambName:focus { border-color: %6; }"
        "QScrollArea#ambScroll { background: transparent; border: 0; }"
        "QScrollArea#ambScroll QScrollBar:vertical { width: 6px; background: transparent; margin: 0; }"
        "QScrollArea#ambScroll QScrollBar::handle:vertical { background: %2; border-radius: 3px; min-height: 24px; }"
        "QScrollArea#ambScroll QScrollBar::add-line:vertical, QScrollArea#ambScroll QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollArea#ambScroll QScrollBar::add-page:vertical, QScrollArea#ambScroll QScrollBar::sub-page:vertical { background: transparent; }"
        "QSlider#ambVolume { background: transparent; }"
        "QSlider#ambVolume::groove:horizontal {"
        "  background: %2; height: 4px; border-radius: 2px;"
        "}"
        "QSlider#ambVolume::sub-page:horizontal { background: %6; border-radius: 2px; }"
        "QSlider#ambVolume::handle:horizontal {"
        "  background: %3; width: 12px; margin: -5px 0; border-radius: 6px;"
        "}"
    ).arg(Theme::panelBackground(),
          Theme::panelBorder(),
          Theme::textPrimary(),
          Theme::textMuted(),
          Theme::hoverOverlay(),
          Theme::accentDefault(),
          Theme::chromeBackground())));
    update();
}

void AmbiencePanel::showNear(const QRect& anchorGlobal, Qt::Edge barSide)
{
    m_anchor = anchorGlobal;
    m_barSide = barSide;
    if (m_manager) m_manager->refresh();
    setView(View::Grid);
    adjustSize();
    const QSize ps = size();
    QPoint pos(anchorGlobal.left(), anchorGlobal.bottom() + kGapBelowAnchor);
    const QScreen* screen = QGuiApplication::screenAt(anchorGlobal.center());
    if (screen) {
        pos = AnchorUtils::positionNear(anchorGlobal, ps, barSide,
                                         screen->availableGeometry(), kGapBelowAnchor);
    }
    move(pos);
    show();
    raise();
    activateWindow();
    updateAnimTimer();
}

void AmbiencePanel::hideEvent(QHideEvent* e)
{
    QFrame::hideEvent(e);
    m_anim->stop();
    if (m_toast) m_toast->hide();
}

bool AmbiencePanel::eventFilter(QObject* watched, QEvent* event)
{
    if (!isVisible()) return false;
    if (watched == m_nameEdit && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        finishNaming(false);
        return true;
    }
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
        hide();
    }
    return false;
}
