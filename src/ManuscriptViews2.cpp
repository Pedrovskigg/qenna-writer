#include "ManuscriptViews2.h"

#include "PanelMotion.h"
#include "Theme.h"

#include <QContextMenuEvent>
#include <QCursor>
#include <QEasingCurve>
#include <QFontDatabase>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QToolTip>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <functional>

namespace {
QColor tcol(const QString& css) { return Theme::toColor(css); }

QFont mkFont(const QString& family, qreal px, int weight = QFont::Normal, bool italic = false) {
    QFont f(family);
    f.setPixelSize(qMax(1, qRound(px)));
    f.setWeight(QFont::Weight(weight));
    f.setItalic(italic);
    return f;
}
QFont ui(qreal px, int weight = QFont::Normal) { return mkFont(QStringLiteral("Segoe UI"), px, weight); }
QFont spaced(QFont f, qreal sp) { f.setLetterSpacing(QFont::AbsoluteSpacing, sp); return f; }

QColor withAlpha(QColor c, qreal a) { c.setAlphaF(float(std::clamp(a, 0.0, 1.0))); return c; }
QColor hsl(const QColor& base, qreal s, qreal l) {
    const float h = base.hslHueF() < 0 ? 0.08f : base.hslHueF();
    return QColor::fromHslF(h, float(s), float(l));
}

// Anima de 0 a 1 chamando step; sem animações ligadas, pula direto pro fim.
QVariantAnimation* animate(QObject* owner, int ms, QEasingCurve::Type curve, const std::function<void(qreal)>& step) {
    if (!PanelMotion::enabled()) { step(1.0); return nullptr; }
    auto* a = new QVariantAnimation(owner);
    a->setDuration(ms);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setEasingCurve(curve);
    QObject::connect(a, &QVariantAnimation::valueChanged, owner, [step](const QVariant& v) { step(v.toReal()); });
    a->start(QAbstractAnimation::DeleteWhenStopped);
    return a;
}

void dropShadow(QPainter& p, const QRectF& r, qreal radius, int alpha) {
    for (int i = 4; i >= 1; --i) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, alpha / (i + 1)));
        p.drawRoundedRect(r.adjusted(-i * 0.8, i * 1.2, i * 0.8, i * 2.2), radius + i, radius + i);
    }
}
}

// ============================================================ fontes

namespace MsFonts {
QString pick(const QStringList& families) {
    for (const QString& f : families)
        if (QFontDatabase::hasFamily(f)) return f;
    return families.isEmpty() ? QString() : families.last();
}
QString display() {
    static const QString f = pick({ QStringLiteral("Big Shoulders"), QStringLiteral("Big Shoulders 24pt"),
                                    QStringLiteral("Big Shoulders Display"), QStringLiteral("Oswald"), QStringLiteral("Segoe UI") });
    return f;
}
QString garamond() {
    static const QString f = pick({ QStringLiteral("EB Garamond"), QStringLiteral("Cormorant Garamond"), QStringLiteral("Lora") });
    return f;
}
QString bodoni() {
    static const QString f = pick({ QStringLiteral("Bodoni Moda"), QStringLiteral("Bodoni Moda 28pt"),
                                    QStringLiteral("Bodoni Moda 18pt"), QStringLiteral("Playfair Display"), QStringLiteral("Lora") });
    return f;
}
QString cormorant() {
    static const QString f = pick({ QStringLiteral("Cormorant Garamond"), QStringLiteral("EB Garamond"), QStringLiteral("Lora") });
    return f;
}
QString sticker() {
    static const QString f = pick({ QStringLiteral("Archivo Black"), QStringLiteral("Anton"), QStringLiteral("Segoe UI") });
    return f;
}
}

// ============================================================ MsHero

MsHero::MsHero(QWidget* parent) : QWidget(parent) {
    setFixedHeight(244);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void MsHero::setData(const Data& d) {
    const bool newArt = d.art.cacheKey() != m_d.art.cacheKey();
    if (newArt && !m_d.art.isNull()) {
        m_prevArt = m_d.art;
        m_fade = 0.0;
        if (m_anim) m_anim->stop();
        m_anim = animate(this, 240, QEasingCurve::OutCubic, [this](qreal t) { m_fade = t; update(); });
    }
    m_d = d;
    update();
}

QRect MsHero::chipRect() const {
    if (m_d.bookChip.isEmpty()) return QRect();
    const QFontMetrics fm(spaced(ui(10, QFont::Bold), 1.4));
    const int w = fm.horizontalAdvance(m_d.bookChip) + 18;
    return QRect(width() - 10 - w, 10, w, 22);
}

void MsHero::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF r = rect();
    p.fillRect(r, QColor(15, 15, 14));
    auto drawArt = [&](const QPixmap& pm, qreal op) {
        if (pm.isNull() || op <= 0) return;
        const QSizeF s = pm.deviceIndependentSize();
        const qreal k = std::max(r.width() / s.width(), r.height() / s.height());
        const QSizeF d(s.width() * k, s.height() * k);
        p.setOpacity(op);
        p.drawPixmap(QRectF((r.width() - d.width()) / 2, (r.height() - d.height()) / 2, d.width(), d.height()),
                     pm, QRectF(QPointF(0, 0), pm.size()));
        p.setOpacity(1.0);
    };
    if (m_fade < 1.0) drawArt(m_prevArt, 1.0);
    drawArt(m_d.art, m_fade < 1.0 ? m_fade : 1.0);

    QLinearGradient g(0, 0, 0, r.height());
    g.setColorAt(0.0, QColor(0, 0, 0, 40));
    g.setColorAt(0.35, QColor(0, 0, 0, 0));
    g.setColorAt(0.78, QColor(0, 0, 0, 205));
    g.setColorAt(1.0, QColor(0, 0, 0, 235));
    p.fillRect(r, g);

    const QColor white(245, 244, 239);
    const int x = 16, w = width() - 32;
    int y = height() - 12;
    // de baixo pra cima: meta, título, CAPÍTULO N, rótulo da Parte
    p.setFont(ui(11));
    p.setPen(withAlpha(white, 0.68));
    const QFontMetrics fmM(p.font());
    p.drawText(QRect(x, y - fmM.height(), w, fmM.height()), Qt::AlignLeft | Qt::AlignVCenter,
               fmM.elidedText(m_d.meta, Qt::ElideRight, w));
    y -= fmM.height() + 2;
    p.setFont(mkFont(MsFonts::cormorant(), 22, QFont::Medium, true));
    const QFontMetrics fmT(p.font());
    p.setPen(white);
    p.drawText(QRect(x, y - fmT.height(), w, fmT.height()), Qt::AlignLeft | Qt::AlignVCenter,
               fmT.elidedText(m_d.title, Qt::ElideRight, w));
    y -= fmT.height() - 2;
    QFont big = mkFont(MsFonts::display(), 46, QFont::Black);
    big.setCapitalization(QFont::AllUppercase);
    p.setFont(big);
    const QFontMetrics fmB(big);
    p.drawText(QRect(x, y - fmB.ascent() - 2, w, fmB.ascent() + 4), Qt::AlignLeft | Qt::AlignBottom,
               fmB.elidedText(m_d.big, Qt::ElideRight, w));
    y -= fmB.ascent() + 6;
    p.setFont(spaced(ui(10, QFont::Bold), 2.6));
    p.setPen(hsl(m_d.tint.isValid() ? m_d.tint : QColor(217, 119, 87), 0.70, 0.74));
    const QFontMetrics fmE(p.font());
    p.drawText(QRect(x, y - fmE.height(), w, fmE.height()), Qt::AlignLeft | Qt::AlignVCenter,
               fmE.elidedText(m_d.eyebrow, Qt::ElideRight, w));

    if (m_d.preview && !m_d.previewText.isEmpty()) {
        p.setFont(spaced(ui(9, QFont::Bold), 1.8));
        const QFontMetrics fm(p.font());
        const QRectF pr(10, 10, fm.horizontalAdvance(m_d.previewText) + 14, 18);
        p.setPen(Qt::NoPen);
        p.setBrush(withAlpha(hsl(m_d.tint.isValid() ? m_d.tint : QColor(217, 119, 87), 0.6, 0.40), 0.85));
        p.drawRoundedRect(pr, 2, 2);
        p.setPen(white);
        p.drawText(pr, Qt::AlignCenter, m_d.previewText);
    }
    const QRect cr = chipRect();
    if (!cr.isNull()) {
        const bool hov = cr.contains(mapFromGlobal(QCursor::pos()));
        p.setPen(QPen(QColor(255, 255, 255, 60), 1));
        p.setBrush(QColor(0, 0, 0, hov ? 150 : 110));
        p.drawRoundedRect(QRectF(cr).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
        p.setFont(spaced(ui(10, QFont::Bold), 1.4));
        p.setPen(white);
        p.drawText(cr, Qt::AlignCenter, m_d.bookChip);
    }
}

void MsHero::mousePressEvent(QMouseEvent* e) {
    if (chipRect().contains(e->position().toPoint()) && e->button() == Qt::LeftButton) {
        emit bookChipClicked(mapToGlobal(chipRect().bottomLeft() + QPoint(0, 4)));
        return;
    }
    QWidget::mousePressEvent(e);
}

void MsHero::mouseMoveEvent(QMouseEvent* e) {
    setCursor(chipRect().contains(e->position().toPoint()) ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update(chipRect().adjusted(-2, -2, 2, 2));
    QWidget::mouseMoveEvent(e);
}

// ============================================================ MsCheckpoints

MsCheckpoints::MsCheckpoints(QWidget* parent) : QWidget(parent) {
    setFixedHeight(42);
    setMouseTracking(true);
}

void MsCheckpoints::setData(int count, int here, const QColor& color, const QStringList& labels, const QStringList& tips) {
    m_count = count;
    m_here = here;
    m_color = color;
    m_labels = labels;
    m_tips = tips;
    update();
}

int MsCheckpoints::hitIndex(const QPoint& p) const {
    if (m_count <= 0) return -1;
    const qreal cw = (width() - 32.0) / m_count;
    const int i = int((p.x() - 16) / cw);
    return (i >= 0 && i < m_count) ? i : -1;
}

void MsCheckpoints::paintEvent(QPaintEvent*) {
    if (m_count <= 0) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal cw = (width() - 32.0) / m_count;
    const QColor on = hsl(m_color.isValid() ? m_color : QColor(217, 119, 87), 0.60, 0.62);
    const QColor off = withAlpha(tcol(Theme::textPrimary()), 0.28);
    const qreal cy = 10;
    for (int i = 1; i < m_count; ++i) {
        const qreal x0 = 16 + (i - 0.5) * cw, x1 = 16 + (i + 0.5) * cw;
        p.setPen(QPen(i <= m_here ? on : off, 2));
        p.drawLine(QPointF(x0 + 8, cy), QPointF(x1 - 8, cy));
    }
    for (int i = 0; i < m_count; ++i) {
        const QPointF c(16 + (i + 0.5) * cw, cy);
        const bool done = i < m_here, here = i == m_here, hov = i == m_hover;
        if (here) {
            p.setPen(Qt::NoPen);
            p.setBrush(withAlpha(on, 0.30));
            p.drawEllipse(c, 11, 11);
        }
        p.setPen(QPen(here ? tcol(Theme::textBright()) : (done ? on : (hov ? tcol(Theme::textBright()) : off)), 2));
        p.setBrush(done ? on : tcol(Theme::panelBackground()));
        p.drawEllipse(c, 6, 6);
        p.setFont(spaced(ui(9.5, QFont::DemiBold), 0.8));
        p.setPen(here || hov ? tcol(Theme::textBright()) : tcol(Theme::textMuted()));
        p.drawText(QRectF(c.x() - cw / 2, cy + 10, cw, 16), Qt::AlignCenter,
                   QFontMetrics(p.font()).elidedText(m_labels.value(i), Qt::ElideRight, int(cw) - 4));
    }
}

void MsCheckpoints::mousePressEvent(QMouseEvent* e) {
    const int i = hitIndex(e->position().toPoint());
    if (i >= 0 && e->button() == Qt::LeftButton) emit sceneClicked(i);
}

void MsCheckpoints::mouseMoveEvent(QMouseEvent* e) {
    const int i = hitIndex(e->position().toPoint());
    if (i != m_hover) { m_hover = i; setCursor(i >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); }
}

bool MsCheckpoints::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        const int i = hitIndex(he->pos());
        if (i >= 0 && i < m_tips.size()) QToolTip::showText(he->globalPos(), m_tips.at(i), this);
        else QToolTip::hideText();
        return true;
    }
    if (e->type() == QEvent::Leave && m_hover >= 0) { m_hover = -1; update(); }
    return QWidget::event(e);
}

// ============================================================ MsDrum

MsDrum::MsDrum(QWidget* parent) : QWidget(parent) {
    setFixedHeight(300);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::SizeVerCursor);
    m_typeTimer = new QTimer(this);
    m_typeTimer->setSingleShot(true);
    m_typeTimer->setInterval(800);
    connect(m_typeTimer, &QTimer::timeout, this, [this]() { m_typed.clear(); });
}

void MsDrum::setItems(const QList<Item>& items, int current, const QColor& accent) {
    m_items = items;
    m_cur = std::clamp(current, 0, std::max(0, int(items.size()) - 1));
    m_pos = m_cur;
    m_accent = accent;
    update();
}

void MsDrum::spinTo(int index) {
    index = std::clamp(index, 0, std::max(0, int(m_items.size()) - 1));
    if (index == m_cur) return;
    const qreal from = m_pos;
    m_cur = index;
    if (m_anim) m_anim->stop();
    const qreal to = index;
    m_anim = animate(this, 260, QEasingCurve::OutCubic, [this, from, to](qreal t) { m_pos = from + (to - from) * t; update(); });
    emit currentChanged(index);
}

int MsDrum::itemAt(const QPoint& p) const {
    const qreal cy = height() / 2.0;
    int best = -1;
    qreal bestD = 1e9;
    for (int i = 0; i < m_items.size(); ++i) {
        const qreal d = i - m_pos, a = std::abs(d);
        if (a > 4.5) continue;
        const qreal y = cy + d * 54 - (d > 0 ? 1 : -1) * std::max(0.0, a - 1) * a * 3;
        const qreal dist = std::abs(p.y() - y);
        if (dist < bestD && dist < 26) { bestD = dist; best = i; }
    }
    return best;
}

void MsDrum::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const qreal cx = width() / 2.0, cy = height() / 2.0;
    // faixa do escolhido
    p.setPen(QPen(tcol(Theme::subtleBorder()), 1));
    p.drawLine(QPointF(12, cy - 29), QPointF(width() - 12, cy - 29));
    p.drawLine(QPointF(12, cy + 29), QPointF(width() - 12, cy + 29));

    QList<int> order;
    for (int i = 0; i < m_items.size(); ++i) if (std::abs(i - m_pos) <= 4.5) order << i;
    std::sort(order.begin(), order.end(), [this](int a, int b) { return std::abs(a - m_pos) > std::abs(b - m_pos); });
    for (int i : order) {
        const Item& it = m_items.at(i);
        const qreal d = i - m_pos, a = std::abs(d);
        const qreal y = cy + d * 54 - (d > 0 ? 1 : -1) * std::max(0.0, a - 1) * a * 3;
        const qreal s = 1.0 - std::min(a, 4.0) * 0.13;
        const qreal sy = s * std::cos(std::min(a, 4.0) * 16.0 * M_PI / 180.0);
        const qreal op = a <= 1 ? 1.0 - a * 0.38 : std::max(0.0, 0.62 - (a - 1) * 0.2);
        if (op <= 0.01) continue;
        p.save();
        p.setOpacity(op);
        p.translate(cx, y);
        p.scale(s, sy);
        const int w = int((width() - 40) / s);
        p.setFont(spaced(ui(9.5, QFont::DemiBold), 2.4));
        p.setPen(a < 0.5 ? m_accent : tcol(Theme::textMuted()));
        p.drawText(QRectF(-w / 2.0, -26, w, 14), Qt::AlignCenter, it.label);
        p.setFont(mkFont(QStringLiteral("Lora"), 21, QFont::Normal, it.empty));
        p.setPen(it.empty ? tcol(Theme::textMuted()) : tcol(Theme::textBright()));
        const QFontMetrics fm(p.font());
        p.drawText(QRectF(-w / 2.0, -12, w, 30), Qt::AlignCenter, fm.elidedText(it.title, Qt::ElideRight, w));
        p.restore();
    }
    // (as bordas somem pela opacidade de cada item: pintar a cor do painel por
    // cima marcava uma faixa, porque o painel tem transparência própria)
    if (hasFocus()) {
        p.setPen(QPen(withAlpha(m_accent, 0.35), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(rect()).adjusted(3.5, 3.5, -3.5, -3.5), 6, 6);
    }
}

void MsDrum::wheelEvent(QWheelEvent* e) {
    m_wheelAcc += e->angleDelta().y();
    while (std::abs(m_wheelAcc) >= 60) {
        spinTo(m_cur + (m_wheelAcc > 0 ? -1 : 1));
        m_wheelAcc += m_wheelAcc > 0 ? -60 : 60;
    }
    e->accept();
}

void MsDrum::mousePressEvent(QMouseEvent* e) {
    setFocus(Qt::MouseFocusReason);
    if (e->button() != Qt::LeftButton) return;
    m_dragY = int(e->position().y());
    m_dragged = false;
}

void MsDrum::mouseMoveEvent(QMouseEvent* e) {
    if (m_dragY < 0) return;
    const int dy = int(e->position().y()) - m_dragY;
    if (std::abs(dy) > 34) {
        spinTo(m_cur + (dy > 0 ? -1 : 1));
        m_dragY = int(e->position().y());
        m_dragged = true;
    }
}

void MsDrum::mouseReleaseEvent(QMouseEvent* e) {
    const bool wasDrag = m_dragged;
    m_dragY = -1;
    m_dragged = false;
    if (wasDrag || e->button() != Qt::LeftButton) return;
    const int i = itemAt(e->position().toPoint());
    if (i < 0) return;
    if (i == m_cur) emit activated(i);
    else spinTo(i);
}

void MsDrum::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_Up:   spinTo(m_cur - 1); return;
    case Qt::Key_Down: spinTo(m_cur + 1); return;
    case Qt::Key_PageUp:   spinTo(m_cur - 5); return;
    case Qt::Key_PageDown: spinTo(m_cur + 5); return;
    case Qt::Key_Home: spinTo(0); return;
    case Qt::Key_End:  spinTo(int(m_items.size()) - 1); return;
    case Qt::Key_Return:
    case Qt::Key_Enter: emit activated(m_cur); return;
    default: break;
    }
    // Número digitado: vai direto pro capítulo com esse número.
    const QString t = e->text();
    if (t.size() == 1 && t.at(0).isDigit()) {
        m_typed += t;
        m_typeTimer->start();
        for (int i = 0; i < m_items.size(); ++i)
            if (m_items.at(i).number == m_typed) { spinTo(i); break; }
        return;
    }
    QWidget::keyPressEvent(e);
}

// ============================================================ MsAlbumPage

namespace {
constexpr int kAlbumPad = 12, kAlbumHead = 36, kAlbumGap = 14, kAlbumInset = 6;
int stickerWidth(int w) { return std::max(60, (w - kAlbumPad * 2 - kAlbumInset * 2 - kAlbumGap) / 2); }
int stickerHeight(int w) { return 4 + (stickerWidth(w) - 8) + 4 + 16 + 14 + 4; }
qreal stickerRotation(const QString& id) { return ((int(qHash(id) % 5)) - 2) * 0.8; }
}

MsAlbumPage::MsAlbumPage(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
    m_foilTimer = new QTimer(this);
    m_foilTimer->setInterval(45);
    connect(m_foilTimer, &QTimer::timeout, this, [this]() { m_foil = std::fmod(m_foil + 0.014, 1.0); update(); });
}

int MsAlbumPage::artSide(int width) { return stickerWidth(width) - 8; }

void MsAlbumPage::setPage(const QString& eyebrow, const QString& title, const QString& watermark,
                          const QColor& color, const QList<Sticker>& stickers, const QString& countText) {
    m_eyebrow = eyebrow;
    m_title = title;
    m_watermark = watermark;
    m_color = color;
    m_stickers = stickers;
    m_count = countText;
    bool foil = false;
    for (const auto& s : stickers) foil = foil || (s.foil && !s.empty);
    if (foil && PanelMotion::enabled() && isVisible()) m_foilTimer->start(); else m_foilTimer->stop();
    updateGeometry();
    update();
}

int MsAlbumPage::heightForWidth(int w) const {
    const int rows = std::max(1, int((m_stickers.size() + 1) / 2));
    return kAlbumPad + kAlbumHead + rows * stickerHeight(w) + (rows - 1) * kAlbumGap + kAlbumPad + 4;
}

QRect MsAlbumPage::stickerRect(int i) const {
    const int sw = stickerWidth(width()), sh = stickerHeight(width());
    const int col = i % 2, row = i / 2;
    return QRect(kAlbumPad + kAlbumInset + col * (sw + kAlbumGap), kAlbumPad + kAlbumHead + row * (sh + kAlbumGap), sw, sh);
}

int MsAlbumPage::hitIndex(const QPoint& p) const {
    for (int i = 0; i < m_stickers.size(); ++i) if (stickerRect(i).contains(p)) return i;
    return -1;
}

void MsAlbumPage::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF page = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QColor bg = tcol(Theme::panelBackground());
    QColor top = bg;
    top = QColor::fromRgbF(bg.redF() * 0.78f + m_color.redF() * 0.22f, bg.greenF() * 0.78f + m_color.greenF() * 0.22f,
                           bg.blueF() * 0.78f + m_color.blueF() * 0.22f);
    QLinearGradient g(page.topLeft(), page.bottomRight());
    g.setColorAt(0, top);
    g.setColorAt(0.7, bg.darker(108));
    QPainterPath clip;
    clip.addRoundedRect(page, 6, 6);
    p.fillPath(clip, g);
    p.save();
    p.setClipPath(clip);
    // pontinhos do papel do álbum
    p.setPen(Qt::NoPen);
    p.setBrush(withAlpha(tcol(Theme::textPrimary()), 0.035));
    for (int y = 12; y < height(); y += 16)
        for (int x = 12; x < width(); x += 16) p.drawEllipse(QPointF(x, y), 1.4, 1.4);
    // número grande da Parte, lá atrás
    p.setFont(mkFont(MsFonts::sticker(), 150));
    p.setPen(withAlpha(m_color, 0.08));
    p.drawText(QRect(width() - 200, height() - 150, 206, 190), Qt::AlignRight | Qt::AlignBottom, m_watermark);
    p.restore();
    p.setPen(QPen(withAlpha(m_color, 0.35), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(clip);

    // cabeçalho da página
    int x = kAlbumPad + kAlbumInset;
    const int hy = kAlbumPad + 4;
    p.setFont(spaced(ui(10, QFont::Bold), 2.0));
    p.setPen(m_color);
    const int ew = QFontMetrics(p.font()).horizontalAdvance(m_eyebrow);
    p.drawText(QRect(x, hy, ew + 2, 22), Qt::AlignLeft | Qt::AlignVCenter, m_eyebrow);
    x += ew + 8;
    p.setFont(spaced(ui(10), 0));
    QFont cf(QStringLiteral("IBM Plex Mono"));
    cf.setPixelSize(10);
    p.setFont(cf);
    const int cw = QFontMetrics(cf).horizontalAdvance(m_count);
    p.setPen(tcol(Theme::textMuted()));
    p.drawText(QRect(width() - kAlbumPad - kAlbumInset - cw, hy, cw + 2, 22), Qt::AlignRight | Qt::AlignVCenter, m_count);
    QFont tf = mkFont(MsFonts::sticker(), 17);
    tf.setItalic(true);
    tf.setCapitalization(QFont::AllUppercase);
    p.setFont(tf);
    p.setPen(tcol(Theme::textBright()));
    const int tw = width() - kAlbumPad - kAlbumInset - cw - 8 - x;
    p.drawText(QRect(x, hy - 2, tw, 26), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetrics(tf).elidedText(m_title.toUpper(), Qt::ElideRight, tw));

    const QColor paper(244, 242, 236), ink(27, 26, 24), inkMuted(109, 106, 99);
    for (int i = 0; i < m_stickers.size(); ++i) {
        const Sticker& s = m_stickers.at(i);
        const QRectF r = stickerRect(i);
        const bool hov = (i == m_hover);
        if (s.empty) {
            QPen dash(withAlpha(m_color, hov ? 0.9 : 0.55), 1.5, Qt::DashLine);
            p.setPen(dash);
            p.setBrush(hov ? withAlpha(m_color, 0.06) : Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 4, 4);
            p.setFont(mkFont(MsFonts::sticker(), 34));
            p.setPen(withAlpha(m_color, 0.45));
            p.drawText(QRectF(r.left(), r.top() + r.height() * 0.18, r.width(), r.height() * 0.4), Qt::AlignCenter, s.number);
            p.setFont(ui(11.5, QFont::DemiBold));
            p.setPen(tcol(Theme::textPrimary()));
            p.drawText(QRectF(r.left() + 6, r.top() + r.height() * 0.58, r.width() - 12, 30), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                       QFontMetrics(p.font()).elidedText(s.title, Qt::ElideRight, int(r.width() - 12) * 2));
            p.setFont(mkFont(QStringLiteral("Segoe UI"), 10.5, QFont::Normal, true));
            p.setPen(tcol(Theme::textMuted()));
            p.drawText(QRectF(r.left(), r.bottom() - 22, r.width(), 16), Qt::AlignCenter, s.meta);
            continue;
        }
        p.save();
        p.translate(r.center());
        p.rotate(hov || s.current ? 0 : stickerRotation(s.chapterId));
        if (hov) p.translate(0, -2);
        const QRectF lr(-r.width() / 2, -r.height() / 2, r.width(), r.height());
        dropShadow(p, lr, 4, 90);
        p.setPen(Qt::NoPen);
        p.setBrush(paper);
        p.drawRoundedRect(lr, 4, 4);
        const QRectF art(lr.left() + 4, lr.top() + 4, lr.width() - 8, lr.width() - 8);
        if (!s.art.isNull()) p.drawPixmap(art, s.art, QRectF(QPointF(0, 0), s.art.size()));
        if (s.foil) {
            // brilhante: faixa de arco-íris passando por cima da arte
            p.save();
            QPainterPath ap;
            ap.addRoundedRect(art, 2, 2);
            p.setClipPath(ap);
            p.setCompositionMode(QPainter::CompositionMode_Screen);
            const qreal off = (m_foil * 2.4 - 1.2) * art.width();
            QLinearGradient fg(art.left() + off, art.top(), art.left() + off + art.width() * 0.9, art.bottom());
            fg.setColorAt(0.0, QColor(0, 0, 0, 0));
            fg.setColorAt(0.35, QColor(255, 120, 200, 110));
            fg.setColorAt(0.5, QColor(120, 220, 255, 120));
            fg.setColorAt(0.65, QColor(255, 240, 140, 110));
            fg.setColorAt(1.0, QColor(0, 0, 0, 0));
            p.fillRect(art, fg);
            p.restore();
        }
        // número no canto
        const QRectF nb(lr.left(), lr.top(), 28, 28);
        QPainterPath np;
        np.moveTo(nb.topLeft() + QPointF(4, 0));
        np.lineTo(nb.topRight());
        np.lineTo(nb.right(), nb.bottom() - 12);
        np.quadTo(nb.right(), nb.bottom(), nb.right() - 12, nb.bottom());
        np.lineTo(nb.left(), nb.bottom());
        np.lineTo(nb.left(), nb.top() + 4);
        np.quadTo(nb.topLeft(), nb.topLeft() + QPointF(4, 0));
        p.setBrush(m_color);
        p.drawPath(np);
        p.setFont(mkFont(MsFonts::sticker(), 15));
        p.setPen(Qt::white);
        p.drawText(nb, Qt::AlignCenter, s.number);
        const qreal ty = art.bottom() + 4;
        p.setFont(ui(12, QFont::Bold));
        p.setPen(ink);
        const QString title = s.foil ? s.title + QStringLiteral(" ✦") : s.title;
        p.drawText(QRectF(lr.left() + 4, ty, lr.width() - 8, 16), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(p.font()).elidedText(title, Qt::ElideRight, int(lr.width()) - 8));
        p.setFont(ui(10));
        p.setPen(inkMuted);
        p.drawText(QRectF(lr.left() + 4, ty + 16, lr.width() - 8, 14), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(p.font()).elidedText(s.meta, Qt::ElideRight, int(lr.width()) - 8));
        if (s.current) {
            p.setPen(QPen(tcol(Theme::textBright()), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(lr.adjusted(-3, -3, 3, 3), 6, 6);
        }
        p.restore();
    }
}

void MsAlbumPage::mousePressEvent(QMouseEvent* e) {
    const int i = hitIndex(e->position().toPoint());
    if (i >= 0 && e->button() == Qt::LeftButton) emit stickerClicked(m_stickers.at(i).chapterId);
}

void MsAlbumPage::mouseMoveEvent(QMouseEvent* e) {
    const int i = hitIndex(e->position().toPoint());
    if (i != m_hover) {
        m_hover = i;
        setCursor(i >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        setToolTip(i >= 0 ? m_stickers.at(i).title : QString());
        update();
    }
}

void MsAlbumPage::leaveEvent(QEvent*) {
    if (m_hover >= 0) { m_hover = -1; update(); }
}

void MsAlbumPage::contextMenuEvent(QContextMenuEvent* e) {
    const int i = hitIndex(e->pos());
    if (i >= 0) emit stickerContextRequested(m_stickers.at(i).chapterId, e->globalPos());
}

void MsAlbumPage::showEvent(QShowEvent*) {
    bool foil = false;
    for (const auto& s : m_stickers) foil = foil || (s.foil && !s.empty);
    if (foil && PanelMotion::enabled()) m_foilTimer->start();
}

void MsAlbumPage::hideEvent(QHideEvent*) { m_foilTimer->stop(); }

// ============================================================ MsChecklist

MsChecklist::MsChecklist(QWidget* parent) : QWidget(parent) {}

void MsChecklist::setData(const QList<bool>& glued, const QStringList& tips) {
    m_glued = glued;
    m_tips = tips;
    updateGeometry();
    update();
}

QSize MsChecklist::sizeHint() const {
    const int perRow = 11;
    const int rows = std::max(1, int((m_glued.size() + perRow - 1) / perRow));
    return QSize(std::min(int(m_glued.size()), perRow) * 11, rows * 11);
}

void MsChecklist::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    for (int i = 0; i < m_glued.size(); ++i) {
        const QRectF r((i % 11) * 11 + 0.5, (i / 11) * 11 + 0.5, 9, 9);
        if (m_glued.at(i)) {
            p.setPen(Qt::NoPen);
            p.setBrush(withAlpha(tcol(Theme::textBright()), 0.85));
        } else {
            p.setPen(QPen(tcol(Theme::textMuted()), 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
        }
        p.drawRoundedRect(r, 1.5, 1.5);
    }
}

bool MsChecklist::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        const int i = (he->pos().y() / 11) * 11 + he->pos().x() / 11;
        if (i >= 0 && i < m_tips.size()) QToolTip::showText(he->globalPos(), m_tips.at(i), this);
        else QToolTip::hideText();
        return true;
    }
    return QWidget::event(e);
}

// ============================================================ MsCarousel

namespace { constexpr int kCarW = 116, kCarH = 170, kCarStep = 104; }

MsCarousel::MsCarousel(QWidget* parent) : QWidget(parent) {
    setFixedHeight(262);
    setMouseTracking(true);
}

void MsCarousel::setItems(const QList<MsCoverItem>& items, int current, int fromIndex,
                          const QString& title, const QString& meta, const QColor& accent) {
    m_items = items;
    m_cur = current;
    m_title = title;
    m_meta = meta;
    m_accent = accent;
    if (fromIndex >= 0 && fromIndex != current && fromIndex < items.size()) {
        const qreal from = fromIndex, to = current;
        m_pos = from;
        m_anim = animate(this, 380, QEasingCurve::OutCubic, [this, from, to](qreal t) { m_pos = from + (to - from) * t; update(); });
    } else {
        m_pos = current;
    }
    update();
}

QRectF MsCarousel::coverRect(int i, qreal& scale, qreal& opacity) const {
    const qreal d = i - m_pos, a = std::abs(d);
    scale = std::max(0.4, 1.0 - a * 0.24);
    opacity = a < 0.5 ? 1.0 : std::max(0.0, 1.0 - (a - 0.5) * 0.45);
    const qreal w = kCarW * scale, h = kCarH * scale;
    const qreal cx = width() / 2.0 + d * kCarStep;
    return QRectF(cx - w / 2, 14 + (kCarH - h), w, h);
}

int MsCarousel::hitIndex(const QPoint& p) const {
    int best = -1;
    qreal bestA = 1e9;
    for (int i = 0; i < m_items.size(); ++i) {
        qreal s, o;
        const QRectF r = coverRect(i, s, o);
        const qreal a = std::abs(i - m_pos);
        if (r.contains(p) && a < bestA && o > 0.05) { bestA = a; best = i; }
    }
    return best;
}

void MsCarousel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QList<int> order;
    for (int i = 0; i < m_items.size(); ++i) order << i;
    std::sort(order.begin(), order.end(), [this](int a, int b) { return std::abs(a - m_pos) > std::abs(b - m_pos); });
    for (int i : order) {
        qreal s, op;
        const QRectF r = coverRect(i, s, op);
        if (op <= 0.02 || r.right() < -20 || r.left() > width() + 20) continue;
        p.setOpacity(op);
        dropShadow(p, r, 3, 120);
        const QPixmap& pm = m_items.at(i).cover;
        QPainterPath clip;
        clip.addRoundedRect(r, 2, 4);
        p.save();
        p.setClipPath(clip);
        if (!pm.isNull()) p.drawPixmap(r, pm, QRectF(QPointF(0, 0), pm.size()));
        else p.fillRect(r, QColor(60, 60, 60));
        p.fillRect(QRectF(r.left(), r.top(), 3, r.height()), QColor(0, 0, 0, 70));
        const qreal a = std::abs(i - m_pos);
        if (a > 0.05) p.fillRect(r, QColor(0, 0, 0, int(std::min(1.0, a) * 105)));
        p.restore();
        p.setOpacity(1.0);
    }
    // pontinhos
    const qreal dy = 190;
    const qreal total = m_items.size() * 10.0 - 5;
    for (int i = 0; i < m_items.size(); ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(i == m_cur ? m_accent : tcol(Theme::panelBorder()));
        p.drawEllipse(QPointF(width() / 2.0 - total / 2 + i * 10 + 2.5, dy), 2.6, 2.6);
    }
    p.setFont(mkFont(MsFonts::cormorant(), 20, QFont::DemiBold, true));
    p.setPen(tcol(Theme::textBright()));
    const QFontMetrics fm(p.font());
    p.drawText(QRect(12, 200, width() - 24, 26), Qt::AlignCenter, fm.elidedText(m_title, Qt::ElideRight, width() - 24));
    p.setFont(ui(11));
    p.setPen(tcol(Theme::textMuted()));
    p.drawText(QRect(12, 228, width() - 24, 18), Qt::AlignCenter, m_meta);
}

void MsCarousel::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const int i = hitIndex(e->position().toPoint());
    if (i >= 0 && i != m_cur) emit bookClicked(m_items.at(i).id);
}

void MsCarousel::contextMenuEvent(QContextMenuEvent* e) {
    const int i = hitIndex(e->pos());
    if (i >= 0) emit bookContextRequested(m_items.at(i).id, e->globalPos());
}

// ============================================================ MsSpineStack

MsSpineStack::MsSpineStack(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
}

void MsSpineStack::setSpines(const QList<Spine>& spines, const QString& currentId, const QString& addTip) {
    m_spines = spines;
    m_current = currentId;
    m_addTip = addTip;
    setFixedWidth(preferredWidth());
    update();
}

int MsSpineStack::spineWidth() const {
    const int n = std::max(1, int(m_spines.size()));
    return std::clamp(150 / n, 12, 26);
}

int MsSpineStack::preferredWidth() const {
    const int w = spineWidth();
    return 6 + int(m_spines.size()) * (w + 2) + 10 + 20 + 2 + 6;
}

QRect MsSpineStack::spineRect(int i) const {
    const int w = spineWidth();
    int x = 6;
    for (int k = 0; k < i && k < m_spines.size(); ++k) x += (m_spines.at(k).id == m_current ? w + 10 : w) + 2;
    if (i >= m_spines.size()) return QRect(x, 14, 20, 70);
    const bool cur = m_spines.at(i).id == m_current;
    const int top = cur ? 0 : (i == m_hover ? 8 : 14);
    return QRect(x, top, cur ? w + 10 : w, height() - top);
}

int MsSpineStack::hitIndex(const QPoint& p) const {
    for (int i = 0; i <= m_spines.size(); ++i) if (spineRect(i).contains(p)) return i;
    return -1;
}

void MsSpineStack::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), tcol(Theme::chromeBackground()));
    QColor curColor;
    for (int i = 0; i < m_spines.size(); ++i) {
        const Spine& s = m_spines.at(i);
        const QRectF r = spineRect(i);
        const bool cur = s.id == m_current;
        if (cur) curColor = s.color;
        QLinearGradient g(r.topLeft(), r.topRight());
        g.setColorAt(0.0, hsl(s.color, 0.38, 0.24));
        g.setColorAt(0.45, hsl(s.color, 0.38, 0.34));
        g.setColorAt(1.0, hsl(s.color, 0.38, 0.22));
        QPainterPath path;
        path.addRoundedRect(r, cur ? 0 : 3, cur ? 0 : 3);
        p.fillPath(path, g);
        p.fillRect(QRectF(r.right() - 2, r.top(), 2, r.height()), QColor(0, 0, 0, 70));
        if (cur) p.fillRect(QRectF(r.left(), r.top(), r.width(), r.height()), QColor(255, 255, 255, 12));
        // título em pé, de baixo pra cima, e o número no pé da lombada
        p.save();
        p.translate(r.center().x(), r.bottom() - 10);
        p.setPen(QColor(255, 255, 255, cur ? 235 : 185));
        p.setFont(ui(9, QFont::DemiBold));
        p.drawText(QRectF(-r.width() / 2, -12, r.width(), 12), Qt::AlignCenter, QString::number(s.number));
        if (r.width() >= 16) {
            p.rotate(-90);
            p.setFont(mkFont(QStringLiteral("Lora"), r.width() >= 22 ? 11 : 9.5, QFont::DemiBold));
            const int avail = int(r.height()) - 40;
            p.drawText(QRectF(16, -r.width() / 2, avail, r.width()), Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(p.font()).elidedText(s.title, Qt::ElideRight, avail));
        }
        p.restore();
    }
    // o "+" no fim
    const QRectF ar = spineRect(int(m_spines.size()));
    p.setPen(QPen(m_hover == m_spines.size() ? tcol(Theme::textBright()) : tcol(Theme::panelBorder()), 1, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(ar.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
    p.setPen(m_hover == m_spines.size() ? tcol(Theme::textBright()) : tcol(Theme::textMuted()));
    p.setFont(ui(14));
    p.drawText(ar, Qt::AlignCenter, QStringLiteral("+"));
    // a cor do livro aberto corre pela margem da página
    if (curColor.isValid()) {
        p.fillRect(QRectF(width() - 3, 0, 3, height()), hsl(curColor, 0.45, 0.45));
        QLinearGradient sh(width() - 9, 0, width() - 3, 0);
        sh.setColorAt(0, QColor(0, 0, 0, 0));
        sh.setColorAt(1, QColor(0, 0, 0, 60));
        p.fillRect(QRectF(width() - 9, 0, 6, height()), sh);
    }
}

void MsSpineStack::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const int i = hitIndex(e->position().toPoint());
    if (i < 0) return;
    if (i == m_spines.size()) emit addClicked();
    else if (m_spines.at(i).id != m_current) emit bookClicked(m_spines.at(i).id);
}

void MsSpineStack::mouseMoveEvent(QMouseEvent* e) {
    const int i = hitIndex(e->position().toPoint());
    if (i != m_hover) { m_hover = i; setCursor(i >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); }
}

void MsSpineStack::leaveEvent(QEvent*) {
    if (m_hover >= 0) { m_hover = -1; update(); }
}

void MsSpineStack::contextMenuEvent(QContextMenuEvent* e) {
    const int i = hitIndex(e->pos());
    if (i >= 0 && i < m_spines.size()) emit bookContextRequested(m_spines.at(i).id, e->globalPos());
}

bool MsSpineStack::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        const int i = hitIndex(he->pos());
        if (i >= 0 && i < m_spines.size()) QToolTip::showText(he->globalPos(), m_spines.at(i).title, this);
        else if (i == m_spines.size()) QToolTip::showText(he->globalPos(), m_addTip, this);
        else QToolTip::hideText();
        return true;
    }
    return QWidget::event(e);
}

// ============================================================ MsFan

namespace { constexpr int kFanW = 90, kFanH = 132; }

MsFan::MsFan(QWidget* parent) : QWidget(parent) {
    setFixedHeight(176);
    setMouseTracking(true);
}

void MsFan::setItems(const QList<MsCoverItem>& items, const QString& currentId, const QColor& tint,
                     const QString& label, const QString& addTip) {
    m_items = items;
    m_current = currentId;
    m_tint = tint;
    m_label = label;
    m_addTip = addTip;
    m_lift.clear();
    for (int i = 0; i <= items.size(); ++i) m_lift << (i < items.size() && items.at(i).id == currentId ? 22.0 : 0.0);
    update();
}

QTransform MsFan::cardTransform(int slot, qreal lift) const {
    const int n = int(m_items.size()) + 1;
    const qreal mid = (n - 1) / 2.0;
    const qreal spread = n > 6 ? 78.0 / (n - 1) : 13.0;
    QTransform t;
    t.translate(width() / 2.0, height() + 162);
    t.rotate((slot - mid) * spread);
    t.translate(0, -264 - lift);
    return t;
}

int MsFan::hitIndex(const QPoint& p) const {
    // de cima pra baixo na pilha: aberto, com mouse, depois pela ordem
    QList<int> z;
    for (int i = 0; i <= m_items.size(); ++i) z << i;
    std::sort(z.begin(), z.end(), [this](int a, int b) {
        auto rank = [this](int s) { return (s < m_items.size() && m_items.at(s).id == m_current) ? 1000 : (s == m_hover ? 900 : s); };
        return rank(a) > rank(b);
    });
    for (int s : z) {
        bool ok = false;
        const QTransform inv = cardTransform(s, m_lift.value(s)).inverted(&ok);
        if (ok && QRectF(-kFanW / 2.0, 0, kFanW, kFanH).contains(inv.map(QPointF(p)))) return s;
    }
    return -1;
}

void MsFan::setHover(int slot) {
    if (slot == m_hover) return;
    m_hover = slot;
    setCursor(slot >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    QList<qreal> from = m_lift, to;
    for (int i = 0; i <= m_items.size(); ++i) {
        const bool cur = i < m_items.size() && m_items.at(i).id == m_current;
        to << (cur ? 22.0 : (i == slot ? 14.0 : 0.0));
    }
    if (m_anim) m_anim->stop();
    m_anim = animate(this, 180, QEasingCurve::OutCubic, [this, from, to](qreal t) {
        for (int i = 0; i < m_lift.size() && i < to.size(); ++i) m_lift[i] = from.value(i) + (to.at(i) - from.value(i)) * t;
        update();
    });
}

void MsFan::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), tcol(Theme::chromeBackground()));
    QRadialGradient glow(QPointF(width() / 2.0, height() * 1.2), width() * 0.75);
    glow.setColorAt(0, withAlpha(hsl(m_tint, 0.40, 0.30), 0.45));
    glow.setColorAt(1, QColor(0, 0, 0, 0));
    p.fillRect(rect(), glow);
    p.setFont(spaced(ui(9, QFont::Bold), 1.8));
    p.setPen(tcol(Theme::textMuted()));
    p.drawText(QRect(10, 6, width() - 20, 16), Qt::AlignLeft | Qt::AlignVCenter, m_label);

    QList<int> z;
    for (int i = 0; i <= m_items.size(); ++i) z << i;
    std::sort(z.begin(), z.end(), [this](int a, int b) {
        auto rank = [this](int s) { return (s < m_items.size() && m_items.at(s).id == m_current) ? 1000 : (s == m_hover ? 900 : s); };
        return rank(a) < rank(b);
    });
    const QRectF card(-kFanW / 2.0, 0, kFanW, kFanH);
    for (int s : z) {
        p.save();
        p.setTransform(cardTransform(s, m_lift.value(s)), true);
        if (s == m_items.size()) {
            p.setPen(QPen(s == m_hover ? tcol(Theme::textBright()) : tcol(Theme::panelBorder()), 1.5, Qt::DashLine));
            p.setBrush(tcol(Theme::panelBackground()));
            p.drawRoundedRect(card, 4, 4);
            p.setPen(s == m_hover ? tcol(Theme::textBright()) : tcol(Theme::textMuted()));
            p.setFont(ui(18));
            p.drawText(QRectF(card.left(), card.top() + 10, card.width(), 30), Qt::AlignCenter, QStringLiteral("+"));
        } else {
            dropShadow(p, card, 3, 130);
            QPainterPath clip;
            clip.addRoundedRect(card, 2, 4);
            p.setClipPath(clip);
            const QPixmap& pm = m_items.at(s).cover;
            if (!pm.isNull()) p.drawPixmap(card, pm, QRectF(QPointF(0, 0), pm.size()));
            else p.fillRect(card, QColor(60, 60, 60));
            p.fillRect(QRectF(card.left(), card.top(), 3, card.height()), QColor(0, 0, 0, 70));
        }
        p.restore();
    }
}

void MsFan::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const int s = hitIndex(e->position().toPoint());
    if (s < 0) return;
    if (s == m_items.size()) emit addClicked();
    else if (m_items.at(s).id != m_current) emit bookClicked(m_items.at(s).id);
}

void MsFan::mouseMoveEvent(QMouseEvent* e) { setHover(hitIndex(e->position().toPoint())); }

void MsFan::leaveEvent(QEvent*) { setHover(-1); }

void MsFan::contextMenuEvent(QContextMenuEvent* e) {
    const int s = hitIndex(e->pos());
    if (s >= 0 && s < m_items.size()) emit bookContextRequested(m_items.at(s).id, e->globalPos());
}

bool MsFan::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        const int s = hitIndex(he->pos());
        if (s >= 0 && s < m_items.size()) QToolTip::showText(he->globalPos(), m_items.at(s).title, this);
        else if (s == m_items.size()) QToolTip::showText(he->globalPos(), m_addTip, this);
        else QToolTip::hideText();
        return true;
    }
    return QWidget::event(e);
}
