#include "LeftBarViews.h"

#include "IconUtils.h"
#include "Theme.h"
#include "UiScale.h"

#include <QContextMenuEvent>
#include <QDateTime>
#include <QEasingCurve>
#include <QFont>
#include <QFontMetricsF>
#include <QHelpEvent>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QToolTip>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>

namespace {

qreal sc() { return UiScale::scale(); }
int px(qreal v) { return qMax(1, qRound(v * sc())); }

// Mesmas medidas da LeftBar (botão 36, ícone 26 na escala 1).
int btnPx()  { return qMax(24, qRound(36 * sc())); }
int iconPx() { return qMax(14, qRound(26 * sc())); }

QColor withAlpha(QColor c, qreal a) { c.setAlphaF(qBound(0.0, a, 1.0)); return c; }

QFont uiFont(qreal size, QFont::Weight w = QFont::Normal) {
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(qMax(8, qRound(size * sc())));
    f.setWeight(w);
    return f;
}

QFont capsFont() {
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(qMax(7, qRound(8.5 * sc())));
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
    return f;
}

QString initials(const QString& name) {
    QString out;
    for (const QString& w : name.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (w.size() <= 2 && out.size() > 0) continue;
        out += w.left(1).toUpper();
        if (out.size() == 2) break;
    }
    return out.isEmpty() ? name.left(1).toUpper() : out;
}

QColor drawerColor(const QString& hex) {
    return QColor(hex.isEmpty() ? Theme::accentDefault() : hex);
}

} // namespace

QPixmap leftBarDrawerGlyph(const LeftBarDrawerInfo& d, int side, const QColor& color, qreal dpr) {
    if (!d.iconId.isEmpty()) {
        const QIcon ic = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/elements/%1.svg").arg(d.iconId),
                                                    color, color, color, QSize(side, side));
        if (!ic.isNull()) {
            const QPixmap pm = ic.pixmap(QSize(side, side), dpr);
            if (!pm.isNull()) return pm;
        }
    }
    QPixmap pm(QSize(side, side) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    QFont f(QStringLiteral("Lora"));
    f.setPixelSize(qMax(8, qRound(side * 0.66)));
    f.setWeight(QFont::Bold);
    p.setFont(f);
    p.setPen(color);
    const QString letter = d.title.isEmpty() ? QStringLiteral("?") : d.title.left(1).toUpper();
    p.drawText(QRect(0, 0, side, side), Qt::AlignCenter, letter);
    return pm;
}

// ============================================================ Tambor

LeftBarDrum::LeftBarDrum(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

int LeftBarDrum::slotH() const { return btnPx() + px(4); }

// O miolo fica logo embaixo do "+" (pedido do Pe, 14/11). Na primeira gaveta
// não há vizinha em cima, então ele cola no "+"; girando, desce um pouco pra
// caber uma vizinha em cima.
qreal LeftBarDrum::centerF() const {
    const qreal sh = slotH();
    return qMin(height() / 2.0, sh * (0.78 + 0.97 * qBound<qreal>(0, m_pos, 1)));
}

int LeftBarDrum::centerY() const { return qRound(centerF()); }

QSize LeftBarDrum::sizeHint() const { return QSize(btnPx(), slotH() * 5 + px(8)); }
QSize LeftBarDrum::minimumSizeHint() const { return QSize(btnPx(), slotH() * 3); }

void LeftBarDrum::setDrawers(const QList<LeftBarDrawerInfo>& drawers) {
    m_drawers = drawers;
    refreshLook();
    const int n = m_drawers.size();
    m_pos = n > 0 ? qBound<qreal>(0, qRound(m_pos), n - 1) : 0;
    m_lastCenter = -1;
    setPos(m_pos);
}

void LeftBarDrum::refreshLook() {
    m_glyphs.clear();
    const qreal dpr = devicePixelRatioF();
    for (const auto& d : std::as_const(m_drawers))
        m_glyphs.append(leftBarDrawerGlyph(d, iconPx(), drawerColor(d.color), dpr));
    updateGeometry();
    update();
}

void LeftBarDrum::setActiveKey(const QString& key) {
    if (m_activeKey == key) return;
    m_activeKey = key;
    update();
}

void LeftBarDrum::revealKey(const QString& key) {
    for (int i = 0; i < m_drawers.size(); ++i)
        if (m_drawers.at(i).key == key) { animateTo(i); return; }
}

QString LeftBarDrum::centerKey() const {
    const LeftBarDrawerInfo* d = centerInfo();
    return d ? d->key : QString();
}

const LeftBarDrawerInfo* LeftBarDrum::centerInfo() const {
    if (m_drawers.isEmpty()) return nullptr;
    return &m_drawers.at(qBound(0, qRound(m_pos), m_drawers.size() - 1));
}

QList<LeftBarDrum::Slot> LeftBarDrum::layoutSlots() const {
    QList<Slot> out;
    // O miolo fica perto do topo, logo embaixo do "+" (pedido do Pe, 14/11):
    // em cima cabe uma vizinha, embaixo as outras.
    const qreal h = height(), w = width(), sh = slotH();
    const qreal cy = centerF();
    const qreal Rup = sh * 1.9;
    const qreal Rdown = qMax<qreal>(sh * 0.6, qMin(h - cy - sh * 0.5, sh * 2.7));
    const qreal b = btnPx();
    for (int i = 0; i < m_drawers.size(); ++i) {
        const qreal d = i - m_pos;
        if (qAbs(d) > 2.9 || d < -1.35) continue;   // em cima só cabe uma
        const qreal a = qBound(-1.45, d * 0.5, 1.45);
        const qreal c = qCos(a);
        const qreal side = b * (0.62 + 0.38 * c);
        const qreal sy = side * qMax(0.4, c);
        const qreal y = cy + qSin(a) * (a < 0 ? Rup : Rdown);
        out.append({ i, QRectF(w / 2.0 - side / 2.0, y - sy / 2.0, side, sy), qPow(c, 2.2), qAbs(d) });
    }
    return out;
}

int LeftBarDrum::indexAt(const QPoint& p) const {
    QList<Slot> s = layoutSlots();
    std::sort(s.begin(), s.end(), [](const Slot& a, const Slot& b) { return a.dist < b.dist; });
    for (const Slot& sl : std::as_const(s))
        if (sl.opacity > 0.08 && sl.box.adjusted(-2, -2, 2, 2).contains(p)) return sl.index;
    return -1;
}

void LeftBarDrum::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal cy = centerF();
    const qreal gap = btnPx() / 2.0 + px(4);
    p.setPen(QPen(Theme::toColor(Theme::subtleBorder()), 1));
    p.drawLine(QPointF(px(5), cy - gap + 0.5), QPointF(width() - px(5), cy - gap + 0.5));
    p.drawLine(QPointF(px(5), cy + gap + 0.5), QPointF(width() - px(5), cy + gap + 0.5));

    QList<Slot> s = layoutSlots();
    std::sort(s.begin(), s.end(), [](const Slot& a, const Slot& b) { return a.dist > b.dist; });
    const qreal radius = qMax(4, px(8));
    for (const Slot& sl : std::as_const(s)) {
        const LeftBarDrawerInfo& d = m_drawers.at(sl.index);
        const QColor col = drawerColor(d.color);
        p.setOpacity(sl.opacity);
        const QRectF box = sl.box.adjusted(0.5, 0.5, -0.5, -0.5);
        const bool center = sl.dist < 0.5;
        const bool active = (d.key == m_activeKey);
        if (active) {
            p.setPen(QPen(col, 1));
            p.setBrush(Theme::toColor(Theme::pressedOverlay()));
            p.drawRoundedRect(box, radius, radius);
        } else if (center) {
            p.setPen(QPen(withAlpha(col, 0.55), 1));
            p.setBrush(withAlpha(col, 0.15));
            p.drawRoundedRect(box, radius, radius);
        } else if (sl.index == m_hover) {
            p.setPen(Qt::NoPen);
            p.setBrush(Theme::toColor(Theme::hoverOverlay()));
            p.drawRoundedRect(box, radius, radius);
        }
        if (sl.index < m_glyphs.size()) {
            const qreal k = qreal(iconPx()) / btnPx();
            const QSizeF gs(box.width() * k, box.height() * k);
            p.drawPixmap(QRectF(box.center() - QPointF(gs.width() / 2, gs.height() / 2), gs),
                         m_glyphs.at(sl.index), QRectF());
        }
    }
}

void LeftBarDrum::setPos(qreal pos) {
    m_pos = pos;
    update();
    emit moved();
    if (m_drawers.isEmpty()) return;
    const int c = qBound(0, qRound(m_pos), m_drawers.size() - 1);
    if (c != m_lastCenter) { m_lastCenter = c; emit centerChanged(); }
}

void LeftBarDrum::stopAnim() {
    if (m_anim) { QVariantAnimation* a = m_anim; m_anim.clear(); a->stop(); }
}

void LeftBarDrum::animateTo(int index) {
    if (m_drawers.isEmpty()) return;
    index = qBound(0, index, m_drawers.size() - 1);
    stopAnim();
    auto* a = new QVariantAnimation(this);
    a->setDuration(380);
    a->setStartValue(m_pos);
    a->setEndValue(qreal(index));
    QEasingCurve ec(QEasingCurve::OutBack);
    ec.setOvershoot(0.9);
    a->setEasingCurve(ec);
    connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { setPos(v.toReal()); });
    m_anim = a;
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

void LeftBarDrum::wheelEvent(QWheelEvent* e) {
    e->accept();
    if (m_drawers.isEmpty()) return;
    m_wheelAcc += e->angleDelta().y();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (qAbs(m_wheelAcc) < 60 || now - m_lastWheel < 170) return;
    const int step = m_wheelAcc > 0 ? -1 : 1;
    m_wheelAcc = 0;
    m_lastWheel = now;
    const int target = qBound(0, qRound(m_anim ? m_anim->endValue().toReal() : m_pos) + step, m_drawers.size() - 1);
    animateTo(target);
}

void LeftBarDrum::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }
    stopAnim();
    m_pressed = true;
    m_dragging = false;
    m_pressY = qRound(e->position().y());
    m_pressPos = m_pos;
    e->accept();
}

void LeftBarDrum::mouseMoveEvent(QMouseEvent* e) {
    if (m_pressed) {
        const int dy = qRound(e->position().y()) - m_pressY;
        if (!m_dragging && qAbs(dy) > 4) { m_dragging = true; setCursor(Qt::ClosedHandCursor); }
        if (m_dragging && !m_drawers.isEmpty())
            setPos(qBound<qreal>(-0.4, m_pressPos - qreal(dy) / slotH(), m_drawers.size() - 0.6));
        return;
    }
    const int h = indexAt(e->position().toPoint());
    if (h != m_hover) { m_hover = h; update(); }
}

void LeftBarDrum::mouseReleaseEvent(QMouseEvent* e) {
    if (!m_pressed || e->button() != Qt::LeftButton) { QWidget::mouseReleaseEvent(e); return; }
    m_pressed = false;
    setCursor(Qt::PointingHandCursor);
    if (m_dragging) { m_dragging = false; animateTo(qRound(m_pos)); return; }
    const int i = indexAt(e->position().toPoint());
    if (i < 0) return;
    if (i == qRound(m_pos)) emit activated(m_drawers.at(i).key);
    else animateTo(i);
}

void LeftBarDrum::contextMenuEvent(QContextMenuEvent* e) {
    const int i = indexAt(e->pos());
    if (i < 0) { e->ignore(); return; }
    emit contextRequested(m_drawers.at(i).key, e->globalPos());
}

bool LeftBarDrum::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        const int i = indexAt(he->pos());
        if (i >= 0) QToolTip::showText(he->globalPos(), m_drawers.at(i).title, this);
        else QToolTip::hideText();
        return true;
    }
    if (e->type() == QEvent::Leave && m_hover != -1) { m_hover = -1; update(); }
    return QWidget::event(e);
}

// ============================================================ Barra com nomes

namespace {
int namedRowH()  { return px(32); }
int namedItemH() { return px(27); }
int namedHeadH() { return px(28); }
}

int LeftBarNamed::designWidth() { return qMax(150, qRound(212 * sc())); }

LeftBarNamed::LeftBarNamed(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    const QStringList saved = QSettings().value(QStringLiteral("ui/leftBarNamedOpen")).toStringList();
    m_open = QSet<QString>(saved.begin(), saved.end());
}

void LeftBarNamed::saveOpen() {
    QSettings().setValue(QStringLiteral("ui/leftBarNamedOpen"), QStringList(m_open.begin(), m_open.end()));
}

int LeftBarNamed::headerH() const { return px(58); }
int LeftBarNamed::footerH() const { return px(44); }

QSize LeftBarNamed::sizeHint() const { return QSize(designWidth(), headerH() + contentH() + footerH()); }
QSize LeftBarNamed::minimumSizeHint() const { return QSize(0, headerH() + footerH() + namedRowH() * 3); }

void LeftBarNamed::setProject(const QString& title, const QString& subtitle, bool infoActive) {
    m_title = title;
    m_subtitle = subtitle;
    m_infoActive = infoActive;
    update();
}

void LeftBarNamed::setGroups(const QList<Group>& groups) {
    m_groups = groups;
    rebuildRows();
}

void LeftBarNamed::setDrawers(const QList<LeftBarDrawerInfo>& drawers, const QString& activeKey) {
    m_drawers = drawers;
    m_activeKey = activeKey;
    m_iconCache.clear();
    rebuildRows();
}

void LeftBarNamed::refreshLook() {
    m_iconCache.clear();
    rebuildRows();
}

void LeftBarNamed::rebuildRows() {
    m_rows.clear();
    int y = px(4);
    auto add = [&](Row r, int h) { r.y = y; r.h = h; y += h; m_rows.append(r); };
    for (const Group& g : std::as_const(m_groups)) {
        if (g.tools.isEmpty()) continue;
        Row h; h.kind = Head; h.text = g.title; add(h, namedHeadH());
        for (const Tool& t : g.tools) {
            Row r; r.kind = ToolRow; r.id = t.id; r.text = t.text; r.iconRes = t.iconRes; r.active = t.active;
            add(r, namedRowH());
        }
    }
    Row hd; hd.kind = Head; hd.text = tr("Gavetas"); add(hd, namedHeadH());
    for (int i = 0; i < m_drawers.size(); ++i) {
        const LeftBarDrawerInfo& d = m_drawers.at(i);
        Row r; r.kind = DrawerRow; r.key = d.key; r.text = d.title.isEmpty() ? tr("Gaveta") : d.title;
        r.color = d.color; r.count = d.count > 0 ? QString::number(d.count) : QString();
        r.active = (d.key == m_activeKey); r.open = m_open.contains(d.key) && !d.items.isEmpty(); r.drawer = i;
        add(r, namedRowH());
        if (r.open) {
            for (const auto& it : d.items) {
                Row c; c.kind = ItemRow; c.key = d.key; c.id = it.id; c.text = it.title; c.color = d.color; c.drawer = i;
                add(c, namedItemH());
            }
        }
    }
    Row nr; nr.kind = NewRow; nr.text = tr("Nova gaveta"); add(nr, namedRowH());
    clampScroll();
    updateGeometry();
    update();
}

int LeftBarNamed::contentH() const {
    return m_rows.isEmpty() ? 0 : m_rows.last().y + m_rows.last().h + px(8);
}

void LeftBarNamed::clampScroll() {
    m_scroll = qBound(0, m_scroll, qMax(0, contentH() - bodyH()));
}

void LeftBarNamed::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    clampScroll();
}

QRect LeftBarNamed::headerRect() const {
    return QRect(px(6), px(8), designWidth() - px(12), headerH() - px(12));
}

QRect LeftBarNamed::footerRect() const {
    return QRect(px(6), height() - footerH() + px(6), designWidth() - px(12), footerH() - px(12));
}

int LeftBarNamed::rowAt(const QPoint& p) const {
    if (headerRect().contains(p)) return -2;
    if (footerRect().contains(p)) return -3;
    if (p.y() < bodyTop() || p.y() >= bodyTop() + bodyH()) return -1;
    const int y = p.y() - bodyTop() + m_scroll;
    for (int i = 0; i < m_rows.size(); ++i) {
        const Row& r = m_rows.at(i);
        if (y >= r.y && y < r.y + r.h) return r.kind == Head ? -1 : i;
    }
    return -1;
}

// O botãozinho de abrir a gaveta no painel, na ponta direita da linha.
bool LeftBarNamed::openHit(const Row& r, const QPoint& p) const {
    if (r.kind != DrawerRow) return false;
    const int right = designWidth() - px(6) - px(22);
    return p.x() >= right - px(24) && p.x() < right;
}

QPixmap LeftBarNamed::toolIcon(const QString& res, bool active) {
    const QString key = res + (active ? QStringLiteral("|a") : QStringLiteral("|n"));
    auto it = m_iconCache.find(key);
    if (it != m_iconCache.end()) return it.value();
    const int s = px(20);
    const QColor c = Theme::toColor(active ? Theme::accentDefault() : Theme::textMuted());
    const QIcon ic = IconUtils::loadToolbarIcon(res, c, c, c, QSize(s, s));
    const QPixmap pm = ic.pixmap(QSize(s, s), devicePixelRatioF());
    m_iconCache.insert(key, pm);
    return pm;
}

QPixmap LeftBarNamed::drawerIcon(int drawer) {
    const QString key = QStringLiteral("dr|%1").arg(drawer);
    auto it = m_iconCache.find(key);
    if (it != m_iconCache.end()) return it.value();
    const LeftBarDrawerInfo& d = m_drawers.at(drawer);
    const QPixmap pm = leftBarDrawerGlyph(d, px(20), drawerColor(d.color), devicePixelRatioF());
    m_iconCache.insert(key, pm);
    return pm;
}

void LeftBarNamed::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const int W = designWidth();
    const qreal rr = px(8);
    const QColor bright = Theme::toColor(Theme::textBright());
    const QColor primary = Theme::toColor(Theme::textPrimary());
    const QColor muted = Theme::toColor(Theme::textMuted());
    const QColor accent = Theme::toColor(Theme::accentDefault());
    const QColor hover = Theme::toColor(Theme::hoverOverlay());
    const QColor pressed = Theme::toColor(Theme::pressedOverlay());

    // ---- Projeto ----
    {
        const QRect hr = headerRect();
        if (m_infoActive || m_hover == -2) {
            p.setPen(Qt::NoPen);
            p.setBrush(m_infoActive ? pressed : hover);
            p.drawRoundedRect(hr, rr, rr);
        }
        const int bs = px(26);
        const QRect badge(hr.left() + px(6), hr.center().y() - bs / 2, bs, bs);
        p.setPen(Qt::NoPen);
        p.setBrush(accent);
        p.drawRoundedRect(badge, px(7), px(7));
        QFont lf(QStringLiteral("Lora"));
        lf.setPixelSize(qMax(9, px(14)));
        lf.setWeight(QFont::Bold);
        p.setFont(lf);
        p.setPen(Qt::white);
        p.drawText(badge, Qt::AlignCenter, m_title.isEmpty() ? QStringLiteral("Q") : m_title.left(1).toUpper());
        const int tx = badge.right() + px(10);
        const int tw = hr.right() - tx - px(4);
        QFont tf(QStringLiteral("Lora"));
        tf.setPixelSize(qMax(10, px(14)));
        tf.setWeight(QFont::DemiBold);
        const QFont sf = uiFont(11.5);
        const int th = QFontMetrics(tf).height(), sh = QFontMetrics(sf).height();
        const int top = hr.center().y() - (th + sh) / 2;
        p.setFont(tf);
        p.setPen(bright);
        p.drawText(QRect(tx, top, tw, th), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(tf).elidedText(m_title.isEmpty() ? tr("Projeto") : m_title, Qt::ElideRight, tw));
        p.setFont(sf);
        p.setPen(muted);
        p.drawText(QRect(tx, top + th, tw, sh), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(sf).elidedText(m_subtitle, Qt::ElideRight, tw));
    }

    // ---- Miolo ----
    const QRect body(0, bodyTop(), W, bodyH());
    p.save();
    p.setClipRect(body);
    p.translate(0, bodyTop() - m_scroll);
    const QFont rowF = uiFont(13, QFont::Medium);
    const QFont itemF = uiFont(12.5);
    const QFont cntF = uiFont(11.5);
    const int x0 = px(6), x1 = W - px(6);
    for (int i = 0; i < m_rows.size(); ++i) {
        const Row& r = m_rows.at(i);
        if (r.y + r.h < m_scroll || r.y > m_scroll + body.height()) continue;
        const QRect box(x0, r.y + px(1), x1 - x0, r.h - px(2));
        if (r.kind == Head) {
            p.setFont(capsFont());
            p.setPen(withAlpha(muted, 0.9));
            p.drawText(QRect(px(14), r.y + px(8), W - px(28), r.h - px(8)), Qt::AlignLeft | Qt::AlignVCenter, r.text.toUpper());
            continue;
        }
        const bool hov = (i == m_hover);
        if (r.active || hov) {
            p.setPen(Qt::NoPen);
            p.setBrush(r.active ? pressed : hover);
            if (r.kind == ItemRow) p.drawRoundedRect(box.adjusted(px(22), 0, 0, 0), px(6), px(6));
            else p.drawRoundedRect(box, rr, rr);
        }
        const int ix = x0 + px(6);
        const int is = px(20);
        const QRect icoR(ix, box.center().y() - is / 2, is, is);
        if (r.kind == ToolRow) {
            p.drawPixmap(icoR, toolIcon(r.iconRes, r.active));
            p.setFont(rowF);
            p.setPen(r.active ? accent : (hov ? bright : primary));
            p.drawText(QRect(icoR.right() + px(10), box.top(), box.right() - icoR.right() - px(14), box.height()),
                       Qt::AlignLeft | Qt::AlignVCenter, r.text);
        } else if (r.kind == DrawerRow) {
            p.drawPixmap(icoR, drawerIcon(r.drawer));
            const LeftBarDrawerInfo& d = m_drawers.at(r.drawer);
            const int chevX = box.right() - px(18);
            // seta da árvore (só se a gaveta tem o que mostrar)
            if (!d.items.isEmpty()) {
                p.save();
                p.translate(chevX + px(6), box.center().y());
                if (r.open) p.rotate(90);
                QPainterPath tri;
                const qreal t = px(3.5);
                tri.moveTo(-t * 0.6, -t);
                tri.lineTo(t * 0.9, 0);
                tri.lineTo(-t * 0.6, t);
                tri.closeSubpath();
                p.setPen(Qt::NoPen);
                p.setBrush(muted);
                p.drawPath(tri);
                p.restore();
            }
            int textRight = chevX - px(4);
            if (hov) {
                // abrir a gaveta no painel
                const QRect ob(chevX - px(28), box.center().y() - px(11), px(22), px(22));
                if (m_hoverOpen) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(hover);
                    p.drawRoundedRect(ob, px(5), px(5));
                }
                p.drawPixmap(ob.adjusted(px(4), px(4), -px(4), -px(4)),
                             toolIcon(QStringLiteral(":/icons/panel-right.svg"), m_hoverOpen));
                textRight = ob.left() - px(4);
            } else if (!r.count.isEmpty()) {
                p.setFont(cntF);
                p.setPen(muted);
                const int cw = QFontMetrics(cntF).horizontalAdvance(r.count);
                p.drawText(QRect(chevX - px(6) - cw, box.top(), cw, box.height()), Qt::AlignRight | Qt::AlignVCenter, r.count);
                textRight = chevX - px(12) - cw;
            }
            p.setFont(rowF);
            p.setPen(r.active || hov ? bright : primary);
            const int tx = icoR.right() + px(10);
            p.drawText(QRect(tx, box.top(), textRight - tx, box.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(rowF).elidedText(r.text, Qt::ElideRight, qMax(0, textRight - tx)));
        } else if (r.kind == ItemRow) {
            const int gx = x0 + px(16);
            p.setPen(QPen(Theme::toColor(Theme::subtleBorder()), 1));
            p.drawLine(QPointF(gx + 0.5, r.y), QPointF(gx + 0.5, r.y + r.h));
            const int ds = px(6);
            p.setPen(Qt::NoPen);
            p.setBrush(withAlpha(drawerColor(r.color), 0.85));
            p.drawEllipse(QRect(gx + px(12), box.center().y() - ds / 2, ds, ds));
            p.setFont(itemF);
            p.setPen(hov ? bright : primary);
            const int tx = gx + px(12) + ds + px(8);
            p.drawText(QRect(tx, box.top(), box.right() - tx - px(4), box.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(itemF).elidedText(r.text, Qt::ElideRight, box.right() - tx - px(4)));
        } else if (r.kind == NewRow) {
            QFont plus = uiFont(19, QFont::Light);
            p.setFont(plus);
            p.setPen(muted);
            p.drawText(icoR.adjusted(-px(2), -px(2), px(2), px(2)), Qt::AlignCenter, QStringLiteral("+"));
            p.setFont(rowF);
            p.setPen(hov ? bright : muted);
            p.drawText(QRect(icoR.right() + px(10), box.top(), box.right() - icoR.right() - px(14), box.height()),
                       Qt::AlignLeft | Qt::AlignVCenter, r.text);
        }
    }
    p.restore();

    // indicador de rolagem
    const int ch = contentH();
    if (ch > body.height() && body.height() > 0) {
        const qreal frac = qreal(body.height()) / ch;
        const qreal th = qMax<qreal>(px(24), body.height() * frac);
        const qreal ty = body.top() + (body.height() - th) * (qreal(m_scroll) / qMax(1, ch - body.height()));
        p.setPen(Qt::NoPen);
        p.setBrush(withAlpha(muted, 0.35));
        p.drawRoundedRect(QRectF(W - px(4), ty, px(2.5), th), px(1.25), px(1.25));
    }

    // ---- Rodapé: recolher ----
    p.setPen(QPen(Theme::toColor(Theme::subtleBorder()), 1));
    p.drawLine(QPointF(0, height() - footerH() + 0.5), QPointF(W, height() - footerH() + 0.5));
    const QRect fr = footerRect();
    if (m_hover == -3) {
        p.setPen(Qt::NoPen);
        p.setBrush(hover);
        p.drawRoundedRect(fr, rr, rr);
    }
    const int is = px(20);
    const QRect icoR(fr.left() + px(6), fr.center().y() - is / 2, is, is);
    p.drawPixmap(icoR, toolIcon(m_mirrored ? QStringLiteral(":/icons/panel-right.svg")
                                           : QStringLiteral(":/icons/panel-left.svg"), false));
    p.setFont(rowF);
    p.setPen(m_hover == -3 ? bright : muted);
    p.drawText(QRect(icoR.right() + px(10), fr.top(), fr.right() - icoR.right() - px(14), fr.height()),
               Qt::AlignLeft | Qt::AlignVCenter, tr("Recolher"));
}

void LeftBarNamed::mouseMoveEvent(QMouseEvent* e) {
    const QPoint pt = e->position().toPoint();
    const int h = rowAt(pt);
    const bool ho = (h >= 0) && openHit(m_rows.at(h), pt);
    if (h != m_hover || ho != m_hoverOpen) { m_hover = h; m_hoverOpen = ho; update(); }
    setCursor(h == -1 ? Qt::ArrowCursor : Qt::PointingHandCursor);
    if (h >= 0) setToolTip(m_rows.at(h).kind == DrawerRow && ho ? tr("Abrir a gaveta") : QString());
    else setToolTip(QString());
}

void LeftBarNamed::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }
    m_press = rowAt(e->position().toPoint());
    e->accept();
}

void LeftBarNamed::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) { QWidget::mouseReleaseEvent(e); return; }
    const QPoint pt = e->position().toPoint();
    const int r = rowAt(pt);
    const int pressed = m_press;
    m_press = -1;
    if (r != pressed || r == -1) return;
    if (r == -2) { emit infoClicked(); return; }
    if (r == -3) { emit collapseClicked(); return; }
    const Row row = m_rows.at(r);
    switch (row.kind) {
    case ToolRow: emit toolClicked(row.id); break;
    case DrawerRow: {
        const LeftBarDrawerInfo& d = m_drawers.at(row.drawer);
        if (openHit(row, pt) || d.items.isEmpty()) { emit openDrawerRequested(row.key); break; }
        if (m_open.contains(row.key)) m_open.remove(row.key); else m_open.insert(row.key);
        saveOpen();
        rebuildRows();
        emit contentChanged();
        break;
    }
    case ItemRow: emit itemActivated(row.key, row.id); break;
    case NewRow: emit newDrawerClicked(); break;
    default: break;
    }
}

void LeftBarNamed::leaveEvent(QEvent* e) {
    QWidget::leaveEvent(e);
    if (m_hover != -1) { m_hover = -1; m_hoverOpen = false; update(); }
}

void LeftBarNamed::wheelEvent(QWheelEvent* e) {
    const int before = m_scroll;
    m_scroll -= qRound(e->angleDelta().y() / 120.0 * namedRowH() * 1.5);
    clampScroll();
    if (m_scroll != before) {
        update();
        e->accept();
        QMouseEvent fake(QEvent::MouseMove, e->position(), e->globalPosition(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        mouseMoveEvent(&fake);
    } else {
        e->ignore();
    }
}

void LeftBarNamed::contextMenuEvent(QContextMenuEvent* e) {
    const int r = rowAt(e->pos());
    if (r >= 0 && m_rows.at(r).kind == DrawerRow) { emit drawerContextRequested(m_rows.at(r).key, e->globalPos()); return; }
    emit emptyContextRequested(e->globalPos());
}

// ============================================================ Lingueta

LeftBarTag::LeftBarTag(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hide();
}

void LeftBarTag::setInfo(const QString& title, const QString& color, int count, bool mirrored, bool animate) {
    const bool changed = (title != m_title || color != m_color);
    m_title = title;
    m_color = color;
    m_count = count;
    m_mirrored = mirrored;
    const QFont tf = uiFont(13, QFont::Medium);
    const QFont cf = uiFont(11.5);
    int w = px(10) + px(8) + px(8) + QFontMetrics(tf).horizontalAdvance(m_title) + px(14);
    if (m_count > 0) w += QFontMetrics(cf).horizontalAdvance(QString::number(m_count)) + px(7);
    resize(w + 1, px(30));
    if (animate && changed) {
        if (m_anim) { QVariantAnimation* a = m_anim; m_anim.clear(); a->stop(); }
        auto* a = new QVariantAnimation(this);
        a->setDuration(300);
        a->setStartValue(0.0);
        a->setEndValue(1.0);
        a->setEasingCurve(QEasingCurve::OutCubic);
        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_t = v.toReal(); update(); });
        m_anim = a;
        m_t = 0;
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }
    update();
}

void LeftBarTag::placeAt(int barEdgeX, int centerY) {
    const int x = m_mirrored ? barEdgeX - width() + 1 : barEdgeX - 1;
    move(x, centerY - height() / 2);
}

void LeftBarTag::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const qreal h = height() - 1;
    // a forma sai da barra: o lado dela passa por baixo, sem canto
    p.setClipRect(rect());
    QPainterPath shape;
    const qreal r = h / 2.0;
    const QRectF body = m_mirrored ? QRectF(0.5, 0.5, width() - 1 + 20, h) : QRectF(-20, 0.5, width() - 0.5 + 20, h);
    shape.addRoundedRect(body, r, r);
    QColor bg = Theme::toColor(Theme::panelBackgroundFor(Theme::PanelKey::LeftBar));
    bg.setAlphaF(qMax(bg.alphaF(), 0.97f));
    p.setPen(QPen(Theme::toColor(Theme::panelBorderFor(Theme::PanelKey::LeftBar)), 1));
    p.setBrush(bg);
    p.drawPath(shape);

    const qreal t = qBound(0.0, m_t, 1.0);
    p.setOpacity(0.2 + 0.8 * t);
    p.translate((m_mirrored ? 1 : -1) * (1.0 - t) * px(6), 0);
    const int dot = px(8);
    const int x = m_mirrored ? px(12) : px(10);
    p.setPen(Qt::NoPen);
    p.setBrush(drawerColor(m_color));
    p.drawEllipse(QRectF(x, height() / 2.0 - dot / 2.0, dot, dot));
    const QFont tf = uiFont(13, QFont::Medium);
    p.setFont(tf);
    p.setPen(Theme::toColor(Theme::textBright()));
    const int tx = x + dot + px(8);
    const int tw = QFontMetrics(tf).horizontalAdvance(m_title);
    p.drawText(QRect(tx, 0, tw + 2, height()), Qt::AlignLeft | Qt::AlignVCenter, m_title);
    if (m_count > 0) {
        const QFont cf = uiFont(11.5);
        p.setFont(cf);
        p.setPen(Theme::toColor(Theme::textMuted()));
        p.drawText(QRect(tx + tw + px(7), 0, width(), height()), Qt::AlignLeft | Qt::AlignVCenter, QString::number(m_count));
    }
}

// ============================================================ Cartão da cena

LeftBarSceneCard::LeftBarSceneCard(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hide();
}

void LeftBarSceneCard::setContent(const QString& drawerTitle, const QString& color, const QString& heading,
                                  const QStringList& names) {
    m_title = drawerTitle;
    m_color = color;
    m_heading = heading;
    m_names = names;
    resize(contentSize());
    update();
}

QSize LeftBarSceneCard::contentSize() const {
    const QFont hf = capsFont();
    const QFont sf = uiFont(11.5);
    const QFont nf = uiFont(12.5, QFont::Medium);
    int w = QFontMetricsF(hf).horizontalAdvance(m_title.toUpper()) + 2;
    int h = px(10) + QFontMetrics(hf).height() + px(6);
    const QString sub = m_names.isEmpty() ? QObject::tr("Nada dessa gaveta aqui.") : m_heading + QLatin1Char(':');
    w = qMax(w, QFontMetrics(sf).horizontalAdvance(sub));
    h += QFontMetrics(sf).height();
    for (const QString& n : m_names) {
        w = qMax(w, px(20) + px(7) + QFontMetrics(nf).horizontalAdvance(n));
        h += px(4) + px(20);
    }
    h += px(10);
    return QSize(qMax(px(150), w + px(22)), h);
}

void LeftBarSceneCard::placeAt(int x, int centerY, bool mirrored) {
    int y = centerY - height() / 2;
    if (QWidget* p = parentWidget()) y = qBound(4, y, qMax(4, p->height() - height() - 4));
    move(mirrored ? x - width() : x, y);
}

void LeftBarSceneCard::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    QColor bg = Theme::toColor(Theme::panelBackground());
    bg.setAlphaF(qMax(bg.alphaF(), 0.97f));
    p.setPen(QPen(Theme::toColor(Theme::panelBorder()), 1));
    p.setBrush(bg);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), px(10), px(10));

    const int x = px(11);
    int y = px(10);
    const QFont hf = capsFont();
    p.setFont(hf);
    p.setPen(Theme::toColor(Theme::textMuted()));
    const int hh = QFontMetrics(hf).height();
    p.drawText(QRect(x, y, width() - 2 * x, hh), Qt::AlignLeft | Qt::AlignVCenter, m_title.toUpper());
    y += hh + px(6);
    const QFont sf = uiFont(11.5);
    p.setFont(sf);
    const int sh = QFontMetrics(sf).height();
    p.drawText(QRect(x, y, width() - 2 * x, sh), Qt::AlignLeft | Qt::AlignVCenter,
               m_names.isEmpty() ? QObject::tr("Nada dessa gaveta aqui.") : m_heading + QLatin1Char(':'));
    y += sh;
    const QColor col = drawerColor(m_color);
    const QFont nf = uiFont(12.5, QFont::Medium);
    const QFont af = uiFont(9.5, QFont::Bold);
    for (const QString& n : m_names) {
        y += px(4);
        const QRect av(x, y, px(20), px(20));
        p.setPen(Qt::NoPen);
        p.setBrush(withAlpha(col, 0.18));
        p.drawEllipse(av);
        p.setFont(af);
        p.setPen(col);
        p.drawText(av, Qt::AlignCenter, initials(n));
        p.setFont(nf);
        p.setPen(Theme::toColor(Theme::textBright()));
        p.drawText(QRect(av.right() + px(7), y, width() - av.right() - px(7) - x, px(20)), Qt::AlignLeft | Qt::AlignVCenter, n);
        y += px(20);
    }
}

// ============================================================ Fio

LeftBarThread::LeftBarThread(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hide();
}

void LeftBarThread::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(m_color);
    p.drawRoundedRect(QRectF(rect()), height() / 2.0, height() / 2.0);
}
