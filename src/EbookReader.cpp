#include "EbookReader.h"

#include "Theme.h"

#include <QApplication>
#include <QButtonGroup>
#include <QEnterEvent>
#include <QFontMetricsF>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <cmath>

// ================================================================== o jeito de ler

namespace Ebook {

namespace {
const qreal kSizes[kSizeSteps] = { 0.8, 0.9, 1.0, 1.14, 1.3 };
const qreal kSpacings[kSpacingCount] = { 1.42, 1.6, 1.85 };
constexpr int kWordsPerMinute = 230;
}

Look Look::load() {
    QSettings s;
    Look l;
    l.device = qBound(0, s.value(QStringLiteral("ebook/device"), Eink).toInt(), 2);
    l.paper = qBound(0, s.value(QStringLiteral("ebook/paper"), White).toInt(), 2);
    l.size = qBound(0, s.value(QStringLiteral("ebook/size"), 2).toInt(), kSizeSteps - 1);
    l.font = qBound(0, s.value(QStringLiteral("ebook/font"), 0).toInt(), kFontCount - 1);
    l.spacing = qBound(0, s.value(QStringLiteral("ebook/spacing"), 1).toInt(), kSpacingCount - 1);
    return l;
}

void Look::save() const {
    QSettings s;
    s.setValue(QStringLiteral("ebook/device"), device);
    s.setValue(QStringLiteral("ebook/paper"), paper);
    s.setValue(QStringLiteral("ebook/size"), size);
    s.setValue(QStringLiteral("ebook/font"), font);
    s.setValue(QStringLiteral("ebook/spacing"), spacing);
}

Geo geometry(int device) {
    Geo g;
    switch (device) {
    case Phone:
        g.outer = QSizeF(288, 594);
        g.bezel = QMarginsF(13, 13, 13, 13);
        g.radius = 42; g.screenRadius = 30;
        g.text = QMarginsF(22, 46, 22, 30);
        g.fontPx = 14.5;
        g.body = QColor(0x11, 0x11, 0x13);
        break;
    case Tablet:
        g.outer = QSizeF(470, 600);
        g.bezel = QMarginsF(20, 20, 20, 20);
        g.radius = 28; g.screenRadius = 10;
        g.text = QMarginsF(42, 40, 42, 32);
        g.fontPx = 16.5;
        g.body = QColor(0x18, 0x18, 0x1b);
        break;
    case Eink:
    default:
        g.outer = QSizeF(428, 586);
        g.bezel = QMarginsF(24, 28, 24, 52);
        g.radius = 24; g.screenRadius = 3;
        g.text = QMarginsF(30, 34, 30, 30);
        g.fontPx = 15.5;
        g.body = QColor(0x1f, 0x1f, 0x21);
        break;
    }
    return g;
}

QSizeF screenSize(int device) {
    const Geo g = geometry(device);
    return QSizeF(g.outer.width() - g.bezel.left() - g.bezel.right(), g.outer.height() - g.bezel.top() - g.bezel.bottom());
}

QSizeF contentSize(int device) {
    const Geo g = geometry(device);
    const QSizeF s = screenSize(device);
    return QSizeF(s.width() - g.text.left() - g.text.right(), s.height() - g.text.top() - g.text.bottom() - kFooterH);
}

Colors colors(const Look& look) {
    if (look.device == Eink) return { QColor(0xe8, 0xe7, 0xe2), QColor(0x15, 0x15, 0x15), QColor(0x5e, 0x5e, 0x5e) };
    switch (look.paper) {
    case Sepia: return { QColor(0xf4, 0xea, 0xd5), QColor(0x4f, 0x3d, 0x2c), QColor(0x8c, 0x77, 0x60) };
    case Black: return { QColor(0x0b, 0x0b, 0x0b), QColor(0xd3, 0xd0, 0xca), QColor(0x86, 0x83, 0x7d) };
    case White:
    default:    return { QColor(0xff, 0xff, 0xff), QColor(0x1c, 0x1b, 0x1a), QColor(0x77, 0x73, 0x6c) };
    }
}

QString fontFamily(int font) {
    switch (font) {
    case 1: return QStringLiteral("Alegreya");
    case 2: return QStringLiteral("Alegreya Sans");
    default: return QStringLiteral("Source Serif 4");
    }
}

Exporter::EbookLook exporterLook(const Look& look) {
    const Colors c = colors(look);
    Exporter::EbookLook e;
    e.fontFamily = fontFamily(look.font);
    e.fontPx = geometry(look.device).fontPx * kSizes[qBound(0, look.size, kSizeSteps - 1)];
    e.lineHeight = kSpacings[qBound(0, look.spacing, kSpacingCount - 1)];
    e.ink = c.ink;
    e.muted = c.muted;
    return e;
}

QString deviceName(int device) {
    switch (device) {
    case Phone:  return QCoreApplication::translate("Ebook", "Celular");
    case Tablet: return QCoreApplication::translate("Ebook", "Tablet");
    default:     return QCoreApplication::translate("Ebook", "E-reader");
    }
}

QString fontName(int font) {
    switch (font) {
    case 1: return QStringLiteral("Alegreya");
    case 2: return QCoreApplication::translate("Ebook", "Sem serifa");
    default: return QStringLiteral("Source Serif");
    }
}

QString spacingName(int spacing) {
    switch (spacing) {
    case 0: return QCoreApplication::translate("Ebook", "Justo");
    case 2: return QCoreApplication::translate("Ebook", "Largo");
    default: return QCoreApplication::translate("Ebook", "Normal");
    }
}

} // namespace Ebook

// ================================================================== a tela

EbookView::EbookView(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(260, 320);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    m_slide = new QVariantAnimation(this);
    m_slide->setDuration(300);
    m_slide->setEasingCurve(QEasingCurve::OutCubic);
    m_slide->setStartValue(0.0);
    m_slide->setEndValue(1.0);
    connect(m_slide, &QVariantAnimation::valueChanged, this, [this]() { update(); });
    connect(m_slide, &QVariantAnimation::finished, this, [this]() { m_fromScreen = -1; update(); });
    // a piscada da tinta eletrônica ao virar a página
    m_flash = new QVariantAnimation(this);
    m_flash->setDuration(170);
    m_flash->setStartValue(1.0);
    m_flash->setEndValue(0.0);
    connect(m_flash, &QVariantAnimation::valueChanged, this, [this]() { update(); });
}

void EbookView::setBook(std::shared_ptr<const Exporter::EbookPreview> book, const Ebook::Look& look) {
    m_book = std::move(book);
    m_look = look;
    m_cache.clear();
    m_slide->stop();
    m_fromScreen = -1;
    m_screen = qBound(0, m_screen, qMax(0, screens() - 1));
    update();
}

void EbookView::setScreen(int screen, bool animate) {
    if (!m_book) return;
    screen = qBound(0, screen, screens() - 1);
    if (screen == m_screen) return;
    m_dir = screen > m_screen ? 1 : -1;
    const int from = m_screen;
    m_screen = screen;
    if (animate) {
        if (m_look.device == Ebook::Eink) {
            m_flash->stop();
            m_flash->start();
        } else if (qAbs(from - screen) == 1) {
            m_fromScreen = from;
            m_slide->stop();
            m_slide->start();
        }
    }
    update();
    emit screenChanged(m_screen);
}

EbookView::Frame EbookView::frame() const {
    const Ebook::Geo g = Ebook::geometry(m_look.device);
    const QRectF avail = QRectF(rect()).adjusted(16, 14, -16, -14);
    const qreal k = qMax(0.2, qMin(m_maxScale, qMin(avail.width() / g.outer.width(), avail.height() / g.outer.height())));
    const QSizeF outer(g.outer.width() * k, g.outer.height() * k);
    Frame f;
    f.k = k;
    f.device = QRectF(avail.center().x() - outer.width() / 2, avail.center().y() - outer.height() / 2, outer.width(), outer.height());
    f.screen = f.device.adjusted(g.bezel.left() * k, g.bezel.top() * k, -g.bezel.right() * k, -g.bezel.bottom() * k);
    return f;
}

QPair<QString, QString> EbookView::footer(int screen) const {
    if (!m_book) return {};
    const int s = m_book->sectionAt(screen);
    if (s < 0) return {};
    const auto& sec = m_book->sections.at(s);
    const int last = qMax(1, m_book->pages() - 1);
    const QString pct = QStringLiteral("%1%").arg(qRound(100.0 * screen / last));
    if (sec.kind != Exporter::EbookSection::Chapter) return { sec.name, pct };
    const int start = m_book->screenOf(s);
    const int end = s + 1 < m_book->sections.size() ? m_book->screenOf(s + 1) - 1 : m_book->pages() - 1;
    const qreal left = qMax(0, end - screen) / qreal(qMax(1, end - start + 1));
    const int mins = qMax(1, qRound(sec.words * left / Ebook::kWordsPerMinute));
    return { tr("Cap. %1 · %2 min restantes no capítulo").arg(sec.number).arg(mins), pct };
}

QImage EbookView::screenImage(int screen, const QSize& px, qreal k, qreal dpr) {
    for (const Cached& c : std::as_const(m_cache))
        if (c.screen == screen && c.img.size() == px) return c.img;
    const Ebook::Geo g = Ebook::geometry(m_look.device);
    const Ebook::Colors col = Ebook::colors(m_look);
    QImage img(px, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(col.bg);
    QPainter p(&img);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    const QSizeF scr(px.width() / dpr, px.height() / dpr);
    if (screen < m_book->coverPages()) {
        // a capa ocupa a tela inteira, como no Kindle
        const QImage& cv = m_book->cover;
        const qreal s = qMax(scr.width() / cv.width(), scr.height() / cv.height());
        const QSizeF d(cv.width() * s, cv.height() * s);
        p.drawImage(QRectF((scr.width() - d.width()) / 2, (scr.height() - d.height()) / 2, d.width(), d.height()), cv);
    } else {
        p.save();
        p.translate(g.text.left() * k, g.text.top() * k);
        p.scale(k, k);
        Exporter::renderEbookPage(p, *m_book, screen - m_book->coverPages());
        p.restore();
        const auto ft = footer(screen);
        QFont f = font();
        f.setPixelSize(qMax(7, qRound(10.5 * k)));
        p.setFont(f);
        p.setPen(col.muted);
        const qreal fy = scr.height() - qMax(10.0, g.text.bottom() - 18) * k;
        const QRectF fr(g.text.left() * k, fy - 14 * k, scr.width() - (g.text.left() + g.text.right()) * k, 14 * k);
        p.drawText(fr, Qt::AlignLeft | Qt::AlignBottom, ft.first);
        p.drawText(fr, Qt::AlignRight | Qt::AlignBottom, ft.second);
    }
    p.end();
    if (m_look.device == Ebook::Eink) {
        // tinta eletrônica: tudo em tons de cinza (capa e marcadores inclusive)
        QImage gray = img.convertToFormat(QImage::Format_Grayscale8).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        gray.setDevicePixelRatio(dpr);
        img = gray;
    }
    m_cache.append({ screen, img });
    while (m_cache.size() > 6) m_cache.removeFirst();
    return img;
}

void EbookView::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    if (!m_book || m_book->isEmpty()) {
        g.setPen(Theme::toColor(Theme::textMuted()));
        QFont f = font();
        f.setPixelSize(13);
        g.setFont(f);
        g.drawText(rect(), Qt::AlignCenter, m_emptyText);
        return;
    }
    const Ebook::Geo geo = Ebook::geometry(m_look.device);
    const Frame fr = frame();
    const qreal k = fr.k;
    // sombra macia
    g.setPen(Qt::NoPen);
    for (int i = 9; i >= 1; --i) {
        g.setBrush(QColor(0, 0, 0, 8));
        g.drawRoundedRect(fr.device.adjusted(-i, -i + 8, i, i + 8), geo.radius * k + i, geo.radius * k + i);
    }
    g.setBrush(geo.body);
    g.drawRoundedRect(fr.device, geo.radius * k, geo.radius * k);
    g.setPen(QPen(QColor(255, 255, 255, 18), 1));
    g.setBrush(Qt::NoBrush);
    g.drawRoundedRect(fr.device.adjusted(0.5, 0.5, -0.5, -0.5), geo.radius * k, geo.radius * k);
    g.setPen(Qt::NoPen);
    if (m_look.device == Ebook::Eink) {
        const QRectF bar(fr.device.center().x() - 23 * k, fr.device.bottom() - 25 * k, 46 * k, 5 * k);
        g.setBrush(QColor(0x2c, 0x2c, 0x31));
        g.drawRoundedRect(bar, 2.5 * k, 2.5 * k);
    }

    const qreal dpr = devicePixelRatioF();
    const QSize px(qCeil(fr.screen.width() * dpr), qCeil(fr.screen.height() * dpr));
    QPainterPath clip;
    clip.addRoundedRect(fr.screen, geo.screenRadius * k, geo.screenRadius * k);
    g.save();
    g.setClipPath(clip);
    const QImage cur = screenImage(m_screen, px, k, dpr);
    if (m_fromScreen >= 0 && m_slide->state() == QAbstractAnimation::Running) {
        const qreal t = m_slide->currentValue().toReal();
        const QImage old = screenImage(m_fromScreen, px, k, dpr);
        const qreal w = fr.screen.width();
        g.drawImage(fr.screen.topLeft() + QPointF(-m_dir * t * w, 0), old);
        g.drawImage(fr.screen.topLeft() + QPointF(m_dir * (1 - t) * w, 0), cur);
    } else {
        g.drawImage(fr.screen.topLeft(), cur);
    }
    if (m_flash->state() == QAbstractAnimation::Running) {
        const qreal a = m_flash->currentValue().toReal();
        g.fillRect(fr.screen, QColor(17, 17, 17, int(255 * qMin(1.0, a * 1.4))));
    }
    g.restore();
    if (m_look.device == Ebook::Phone) {
        const QRectF notch(fr.device.center().x() - 37 * k, fr.device.top() + 20 * k, 74 * k, 20 * k);
        g.setBrush(Qt::black);
        g.drawRoundedRect(notch, 10 * k, 10 * k);
    }
}

int EbookView::linkUnder(const QPointF& pos) const {
    if (!m_book || m_screen < m_book->coverPages()) return -1;
    const Frame fr = frame();
    const Ebook::Geo g = Ebook::geometry(m_look.device);
    const QPointF in = (pos - fr.screen.topLeft()) / fr.k - QPointF(g.text.left(), g.text.top());
    if (in.x() < 0 || in.y() < 0 || in.x() > m_book->contentPx.width() || in.y() > m_book->contentPx.height()) return -1;
    return m_book->linkAt(m_screen - m_book->coverPages(), in);
}

void EbookView::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton || !m_book) return;
    setFocus();
    const int link = linkUnder(e->position());
    if (link >= 0) { setScreen(m_book->screenOf(link)); return; }
    const Frame fr = frame();
    if (!fr.device.contains(e->position())) return;
    step(e->position().x() < fr.screen.left() + fr.screen.width() * 0.33 ? -1 : 1);
}

void EbookView::mouseMoveEvent(QMouseEvent* e) {
    if (!m_book) { unsetCursor(); return; }
    setCursor(frame().device.contains(e->position()) ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void EbookView::wheelEvent(QWheelEvent* e) {
    e->accept();
    m_wheelAcc += e->angleDelta().y();
    while (qAbs(m_wheelAcc) >= 120) {
        const int dir = m_wheelAcc > 0 ? -1 : 1;
        m_wheelAcc -= m_wheelAcc > 0 ? 120 : -120;
        step(dir);
    }
}

void EbookView::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_Right: case Qt::Key_PageDown: case Qt::Key_Space: case Qt::Key_Down: step(1); break;
    case Qt::Key_Left: case Qt::Key_PageUp: case Qt::Key_Up: step(-1); break;
    case Qt::Key_Home: setScreen(0); break;
    case Qt::Key_End: setScreen(screens() - 1); break;
    default: QWidget::keyPressEvent(e); return;
    }
    e->accept();
}

void EbookView::enterEvent(QEnterEvent* e) {
    // passou o mouse, as setas já folheiam
    setFocus(Qt::MouseFocusReason);
    QWidget::enterEvent(e);
}

// ================================================================== os controles

namespace {
// Bolinha da cor do papel.
class PaperDot : public QToolButton {
public:
    PaperDot(const QColor& c, QWidget* parent) : QToolButton(parent), m_color(c) {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(24, 24);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r(5, 5, 14, 14);
        if (!isEnabled()) p.setOpacity(0.25);
        if (isChecked()) {
            p.setPen(QPen(Theme::toColor(Theme::accentDefault()), 2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(r.adjusted(-3.5, -3.5, 3.5, 3.5));
        }
        QColor ring = Theme::toColor(Theme::textMuted());
        ring.setAlphaF(0.55);
        p.setPen(QPen(ring, 1));
        p.setBrush(m_color);
        p.drawEllipse(r);
    }
private:
    QColor m_color;
};
} // namespace

EbookLookBar::EbookLookBar(int parts, QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("ebookLookBar"));
    m_look = Ebook::Look::load();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(10);
    auto seg = [this, h](int count, const std::function<QString(int)>& name, QList<QPushButton*>& out,
                         const std::function<void(Ebook::Look&, int)>& set) {
        auto* box = new QWidget(this);
        box->setObjectName(QStringLiteral("ebookSeg"));
        box->setFixedHeight(30);
        auto* bl = new QHBoxLayout(box);
        bl->setContentsMargins(2, 2, 2, 2);
        bl->setSpacing(2);
        auto* grp = new QButtonGroup(box);
        grp->setExclusive(true);
        for (int i = 0; i < count; ++i) {
            auto* b = new QPushButton(name(i), box);
            b->setObjectName(QStringLiteral("ebookSegBtn"));
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            grp->addButton(b);
            connect(b, &QPushButton::clicked, this, [this, i, set]() { change([&](Ebook::Look& l) { set(l, i); }); });
            bl->addWidget(b);
            out.append(b);
        }
        h->addWidget(box);
    };
    if (parts & DevicePart)
        seg(3, [](int i) { return Ebook::deviceName(i); }, m_device, [](Ebook::Look& l, int i) { l.device = i; });
    if (parts & PaperPart) {
        auto* box = new QWidget(this);
        auto* bl = new QHBoxLayout(box);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(2);
        const QColor cs[3] = { QColor(0xff, 0xff, 0xff), QColor(0xf4, 0xea, 0xd5), QColor(0x0b, 0x0b, 0x0b) };
        const QString tips[3] = { tr("Papel branco"), tr("Sépia"), tr("Noturno") };
        for (int i = 0; i < 3; ++i) {
            auto* d = new PaperDot(cs[i], box);
            d->setToolTip(tips[i]);
            connect(d, &QToolButton::clicked, this, [this, i]() { change([i](Ebook::Look& l) { l.paper = i; }); });
            bl->addWidget(d);
            m_paper.append(d);
        }
        h->addWidget(box);
    }
    if (parts & SizePart) {
        m_smaller = new QToolButton(this);
        m_bigger = new QToolButton(this);
        m_smaller->setText(QStringLiteral("A−"));
        m_bigger->setText(QStringLiteral("A+"));
        m_smaller->setToolTip(tr("Letra menor"));
        m_bigger->setToolTip(tr("Letra maior"));
        for (QToolButton* b : { m_smaller, m_bigger }) {
            b->setObjectName(QStringLiteral("ebookIconBtn"));
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedHeight(28);
            b->setMinimumWidth(32);
            h->addWidget(b);
        }
        connect(m_smaller, &QToolButton::clicked, this, [this]() { change([](Ebook::Look& l) { l.size = qMax(0, l.size - 1); }); });
        connect(m_bigger, &QToolButton::clicked, this, [this]() { change([](Ebook::Look& l) { l.size = qMin(Ebook::kSizeSteps - 1, l.size + 1); }); });
    }
    if (parts & FontPart)
        seg(Ebook::kFontCount, [](int i) { return Ebook::fontName(i); }, m_font, [](Ebook::Look& l, int i) { l.font = i; });
    if (parts & SpacingPart)
        seg(Ebook::kSpacingCount, [](int i) { return Ebook::spacingName(i); }, m_spacing, [](Ebook::Look& l, int i) { l.spacing = i; });
    applyTheme();
    sync();
}

void EbookLookBar::setLook(const Ebook::Look& look) {
    m_look = look;
    sync();
}

void EbookLookBar::change(const std::function<void(Ebook::Look&)>& f) {
    Ebook::Look l = m_look;
    f(l);
    if (l == m_look) { sync(); return; }
    m_look = l;
    m_look.save();
    sync();
    emit lookChanged(m_look);
}

void EbookLookBar::sync() {
    for (int i = 0; i < m_device.size(); ++i) m_device.at(i)->setChecked(i == m_look.device);
    for (int i = 0; i < m_font.size(); ++i) m_font.at(i)->setChecked(i == m_look.font);
    for (int i = 0; i < m_spacing.size(); ++i) m_spacing.at(i)->setChecked(i == m_look.spacing);
    // a tinta eletrônica não tem cor de papel
    for (int i = 0; i < m_paper.size(); ++i) {
        m_paper.at(i)->setChecked(i == m_look.paper);
        m_paper.at(i)->setEnabled(m_look.device != Ebook::Eink);
        m_paper.at(i)->setToolTip(m_look.device == Ebook::Eink ? tr("A tinta eletrônica é sempre preto e branco") : QString());
    }
    if (m_smaller) m_smaller->setEnabled(m_look.size > 0);
    if (m_bigger) m_bigger->setEnabled(m_look.size < Ebook::kSizeSteps - 1);
}

void EbookLookBar::applyTheme() {
    setStyleSheet(Theme::qss(QStringLiteral(R"(
        QWidget#ebookSeg { background: %1; border: 1px solid %6; border-radius: @radius-control; }
        QPushButton#ebookSegBtn { background: transparent; color: %3; border: none; border-radius: @radius-item;
                                  padding: 5px 10px; font-size: 11px; font-weight: bold; }
        QPushButton#ebookSegBtn:hover { color: %4; }
        QPushButton#ebookSegBtn:checked { background: %5; color: %4; }
        QToolButton#ebookIconBtn { background: %1; color: %4; border: 1px solid %2; border-radius: @radius-control;
                                   padding: 0 8px; font-size: 12px; font-weight: bold; }
        QToolButton#ebookIconBtn:hover { border-color: %3; }
        QToolButton#ebookIconBtn:disabled { color: %2; }
    )")).arg(Theme::inputBackground(), Theme::subtleBorder(), Theme::textMuted(), Theme::textBright(), Theme::hoverStrong(),
             Theme::subtleBorder()));
}

// ================================================================== a régua

EbookScrubber::EbookScrubber(QWidget* parent) : QWidget(parent) {
    setFixedHeight(28);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);
}

void EbookScrubber::setState(int screen, int screens, const QList<int>& ticks, const QStringList& tickNames) {
    m_screen = screen;
    m_screens = qMax(1, screens);
    m_ticks = ticks;
    m_tickNames = tickNames;
    update();
}

int EbookScrubber::screenAt(qreal x) const {
    const qreal w = qMax(1.0, width() - 16.0);
    return qBound(0, qRound((x - 8) / w * (m_screens - 1)), m_screens - 1);
}

void EbookScrubber::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal x0 = 8, w = width() - 16.0, cy = height() / 2.0;
    auto xOf = [&](int s) { return x0 + w * (m_screens > 1 ? qreal(s) / (m_screens - 1) : 0); };
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::toColor(Theme::subtleBorder()));
    p.drawRoundedRect(QRectF(x0, cy - 1.5, w, 3), 1.5, 1.5);
    p.setBrush(Theme::toColor(Theme::accentDefault()));
    p.drawRoundedRect(QRectF(x0, cy - 1.5, xOf(m_screen) - x0, 3), 1.5, 1.5);
    p.setBrush(Theme::toColor(Theme::textMuted()));
    p.setOpacity(0.45);
    for (int t : std::as_const(m_ticks)) {
        if (t <= 0) continue;
        p.drawRoundedRect(QRectF(xOf(t) - 1, cy - 6.5, 2, 13), 1, 1);
    }
    p.setOpacity(1.0);
    p.setBrush(QColor(0, 0, 0, 70));
    p.drawEllipse(QPointF(xOf(m_screen), cy + 1), 8, 8);
    p.setBrush(Theme::toColor(Theme::textBright()));
    p.drawEllipse(QPointF(xOf(m_screen), cy), 7, 7);
}

void EbookScrubber::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) emit seek(screenAt(e->position().x()));
}

void EbookScrubber::mouseMoveEvent(QMouseEvent* e) {
    if (e->buttons() & Qt::LeftButton) { emit seek(screenAt(e->position().x())); return; }
    // dica: o nome da parte mais perto da marquinha
    const int s = screenAt(e->position().x());
    QString tip;
    for (int i = 0; i < m_ticks.size(); ++i) if (m_ticks.at(i) <= s && i < m_tickNames.size()) tip = m_tickNames.at(i);
    setToolTip(tip);
}

// ================================================================== a sala de leitura

EbookRoom::EbookRoom(std::shared_ptr<Exporter> exporter, const Exporter::Selection& selection,
                     const QString& subtitle, const QString& positionKey, QWidget* parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog), m_exporter(std::move(exporter)),
      m_selection(selection), m_positionKey(positionKey) {
    setObjectName(QStringLiteral("ebookRoom"));
    setWindowTitle(tr("Ler como e-book"));
    setModal(true);
    m_look = Ebook::Look::load();
    m_selection.format = Exporter::Format::Epub;

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── em cima ──
    m_top = new QWidget(this);
    m_top->setObjectName(QStringLiteral("ebookRoomTop"));
    m_top->setFixedHeight(56);
    auto* th = new QHBoxLayout(m_top);
    th->setContentsMargins(18, 0, 14, 0);
    th->setSpacing(14);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(0);
    m_title = new QLabel(tr("Ler como e-book"), m_top);
    m_title->setObjectName(QStringLiteral("ebookRoomTitle"));
    m_subtitle = new QLabel(subtitle, m_top);
    m_subtitle->setObjectName(QStringLiteral("ebookRoomSub"));
    titles->addStretch(1);
    titles->addWidget(m_title);
    titles->addWidget(m_subtitle);
    titles->addStretch(1);
    th->addLayout(titles);
    th->addStretch(1);
    m_bar = new EbookLookBar(EbookLookBar::DevicePart | EbookLookBar::PaperPart, m_top);
    th->addWidget(m_bar);
    m_aaBtn = new QToolButton(m_top);
    m_aaBtn->setObjectName(QStringLiteral("ebookAa"));
    m_aaBtn->setText(QStringLiteral("Aa"));
    m_aaBtn->setToolTip(tr("Tamanho, letra e entrelinha"));
    m_aaBtn->setCursor(Qt::PointingHandCursor);
    m_aaBtn->setFixedSize(38, 30);
    th->addWidget(m_aaBtn);
    m_exportBtn = new QPushButton(tr("Exportar EPUB"), m_top);
    m_exportBtn->setObjectName(QStringLiteral("ebookRoomExport"));
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    th->addWidget(m_exportBtn);
    m_closeBtn = new QToolButton(m_top);
    m_closeBtn->setObjectName(QStringLiteral("ebookRoomClose"));
    m_closeBtn->setText(QStringLiteral("✕"));
    m_closeBtn->setToolTip(tr("Fechar (Esc)"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setFixedSize(30, 30);
    th->addWidget(m_closeBtn);
    root->addWidget(m_top);

    // ── o meio: sumário e o aparelho ──
    auto* mid = new QHBoxLayout;
    mid->setContentsMargins(0, 0, 0, 0);
    mid->setSpacing(0);
    m_rail = new QWidget(this);
    m_rail->setObjectName(QStringLiteral("ebookRoomRail"));
    m_rail->setFixedWidth(260);
    auto* rv = new QVBoxLayout(m_rail);
    rv->setContentsMargins(10, 14, 10, 12);
    rv->setSpacing(4);
    auto* caps = new QLabel(tr("NO LIVRO"), m_rail);
    caps->setObjectName(QStringLiteral("ebookRoomCaps"));
    caps->setContentsMargins(10, 0, 0, 6);
    rv->addWidget(caps);
    auto* scroll = new QScrollArea(m_rail);
    scroll->setObjectName(QStringLiteral("ebookRoomScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* listW = new QWidget(scroll);
    listW->setObjectName(QStringLiteral("ebookRoomList"));
    m_railList = new QVBoxLayout(listW);
    m_railList->setContentsMargins(0, 0, 0, 0);
    m_railList->setSpacing(1);
    m_railList->addStretch(1);
    scroll->setWidget(listW);
    rv->addWidget(scroll, 1);
    m_railNote = new QLabel(tr("Os números são a tela onde cada parte começa neste aparelho, com esta letra. "
                               "Mudou a letra, muda tudo, como no Kindle."), m_rail);
    m_railNote->setObjectName(QStringLiteral("ebookRoomNote"));
    m_railNote->setWordWrap(true);
    m_railNote->setContentsMargins(10, 10, 6, 0);
    rv->addWidget(m_railNote);
    mid->addWidget(m_rail);
    m_view = new EbookView(this);
    m_view->setMaxScale(1.2);
    m_view->setEmptyText(tr("Montando o livro…"));
    mid->addWidget(m_view, 1);
    root->addLayout(mid, 1);

    // ── embaixo: a régua ──
    m_bottom = new QWidget(this);
    m_bottom->setObjectName(QStringLiteral("ebookRoomBottom"));
    m_bottom->setFixedHeight(62);
    auto* bh = new QHBoxLayout(m_bottom);
    bh->setContentsMargins(24, 0, 24, 0);
    bh->setSpacing(14);
    m_prevBtn = new QToolButton(m_bottom);
    m_nextBtn = new QToolButton(m_bottom);
    m_prevBtn->setText(QStringLiteral("‹"));
    m_nextBtn->setText(QStringLiteral("›"));
    for (QToolButton* b : { m_prevBtn, m_nextBtn }) {
        b->setObjectName(QStringLiteral("ebookRoomNav"));
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(30, 30);
    }
    m_scrub = new EbookScrubber(m_bottom);
    m_where = new QLabel(m_bottom);
    m_where->setObjectName(QStringLiteral("ebookRoomWhere"));
    m_where->setMinimumWidth(230);
    m_where->setTextFormat(Qt::RichText);
    bh->addWidget(m_prevBtn);
    bh->addWidget(m_scrub, 1);
    bh->addWidget(m_nextBtn);
    bh->addWidget(m_where);
    root->addWidget(m_bottom);

    connect(m_bar, &EbookLookBar::lookChanged, this, [this](const Ebook::Look& l) { m_look = l; rebuild(); });
    connect(m_aaBtn, &QToolButton::clicked, this, &EbookRoom::toggleAaPopup);
    connect(m_closeBtn, &QToolButton::clicked, this, &QDialog::reject);
    connect(m_exportBtn, &QPushButton::clicked, this, [this]() {
        savePosition();
        const Exporter::Selection sel = m_selection;
        accept();
        emit exportRequested(sel);
    });
    connect(m_prevBtn, &QToolButton::clicked, this, [this]() { m_view->step(-1); });
    connect(m_nextBtn, &QToolButton::clicked, this, [this]() { m_view->step(1); });
    connect(m_scrub, &EbookScrubber::seek, this, [this](int s) { m_view->setScreen(s, false); });
    connect(m_view, &EbookView::screenChanged, this, &EbookRoom::refresh);
    connect(this, &QDialog::finished, this, [this]() { savePosition(); });

    if (parent) setGeometry(parent->window()->geometry());
    applyTheme();
}

EbookRoom::~EbookRoom() = default;

void EbookRoom::showEvent(QShowEvent* e) {
    QDialog::showEvent(e);
    if (!m_book) QTimer::singleShot(0, this, &EbookRoom::rebuild);   // pinta o "Montando" antes
    m_view->setFocus();
}

void EbookRoom::rebuild() {
    if (!m_exporter) return;
    // guarda onde estava (parte e fração) pra voltar no mesmo ponto
    int sec = -1;
    qreal frac = 0;
    if (m_book && !m_book->isEmpty()) {
        const int s = m_view->screen();
        sec = m_book->sectionAt(s);
        const int a = sec < 0 ? 0 : m_book->screenOf(sec);
        const int b = sec + 1 < m_book->sections.size() ? m_book->screenOf(sec + 1) - 1 : m_book->pages() - 1;
        frac = b > a ? qreal(s - a) / (b - a) : 0;
    } else if (!m_restored && !m_positionKey.isEmpty()) {
        const QStringList v = QSettings().value(m_positionKey).toString().split(QChar(':'));
        if (v.size() == 2) { sec = v.at(0).toInt(); frac = v.at(1).toDouble(); }
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_book = std::make_shared<Exporter::EbookPreview>(
        m_exporter->buildEbookPreview(m_selection, Ebook::exporterLook(m_look), Ebook::contentSize(m_look.device)));
    QApplication::restoreOverrideCursor();
    m_view->setBook(m_book, m_look);
    m_view->setMaxScale(m_look.device == Ebook::Tablet ? 1.12 : 1.2);
    m_view->setEmptyText(tr("Nada pra mostrar: o livro está vazio."));
    int target = 0;
    if (sec >= 0 && sec < m_book->sections.size()) {
        const int a = m_book->screenOf(sec);
        const int b = sec + 1 < m_book->sections.size() ? m_book->screenOf(sec + 1) - 1 : m_book->pages() - 1;
        target = a + qRound(frac * qMax(0, b - a));
    } else if (sec == -1 && m_restored) {
        target = 0;
    }
    m_restored = true;
    m_view->setScreen(target, false);

    // o sumário do lado
    for (QPushButton* b : std::as_const(m_railBtns)) b->deleteLater();
    m_railBtns.clear();
    QList<QPair<QString, int>> rows;
    if (m_book->coverPages()) rows.append({ tr("Capa"), -1 });
    for (int i = 0; i < m_book->sections.size(); ++i) {
        const auto& s = m_book->sections.at(i);
        rows.append({ s.kind == Exporter::EbookSection::Chapter ? QStringLiteral("%1. %2").arg(s.number).arg(s.name) : s.name, i });
    }
    for (const auto& r : rows) {
        auto* b = new QPushButton(m_railList->parentWidget());
        b->setObjectName(QStringLiteral("ebookRoomRow"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("section", r.second);
        auto* h = new QHBoxLayout(b);
        h->setContentsMargins(10, 0, 10, 0);
        auto* name = new QLabel(r.first, b);
        name->setObjectName(QStringLiteral("ebookRoomRowName"));
        name->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* num = new QLabel(QString::number((r.second < 0 ? 0 : m_book->screenOf(r.second)) + 1), b);
        num->setObjectName(QStringLiteral("ebookRoomRowNum"));
        num->setAttribute(Qt::WA_TransparentForMouseEvents);
        h->addWidget(name, 1);
        h->addWidget(num);
        b->setMinimumHeight(32);
        const int sidx = r.second;
        connect(b, &QPushButton::clicked, this, [this, sidx]() { m_view->setScreen(sidx < 0 ? 0 : m_book->screenOf(sidx)); refresh(); });
        m_railList->insertWidget(m_railList->count() - 1, b);
        m_railBtns.append(b);
    }
    refresh();
}

void EbookRoom::refresh() {
    if (!m_book || m_book->isEmpty()) { m_where->clear(); return; }
    const int s = m_view->screen(), n = m_book->pages();
    const int sec = m_book->sectionAt(s);
    for (QPushButton* b : std::as_const(m_railBtns)) b->setChecked(b->property("section").toInt() == sec);
    const QString name = sec < 0 ? tr("Capa") : m_book->sections.at(sec).name;
    m_where->setText(QStringLiteral("<span style=\"font-weight:600;\">%1</span><br><span style=\"font-size:11px;\">%2</span>")
        .arg(name.toHtmlEscaped(), tr("Tela %1 de %2 · %3% do livro").arg(s + 1).arg(n).arg(qRound(100.0 * s / qMax(1, n - 1)))));
    QList<int> ticks;
    QStringList names;
    for (int i = 0; i < m_book->sections.size(); ++i) { ticks << m_book->screenOf(i); names << m_book->sections.at(i).name; }
    m_scrub->setState(s, n, ticks, names);
    m_prevBtn->setEnabled(s > 0);
    m_nextBtn->setEnabled(s < n - 1);
}

void EbookRoom::savePosition() {
    if (m_positionKey.isEmpty() || !m_book || m_book->isEmpty()) return;
    const int s = m_view->screen();
    const int sec = m_book->sectionAt(s);
    const int a = sec < 0 ? 0 : m_book->screenOf(sec);
    const int b = sec + 1 < m_book->sections.size() ? m_book->screenOf(sec + 1) - 1 : m_book->pages() - 1;
    const qreal frac = b > a ? qreal(s - a) / (b - a) : 0;
    QSettings().setValue(m_positionKey, QStringLiteral("%1:%2").arg(sec).arg(frac, 0, 'f', 3));
}

void EbookRoom::toggleAaPopup() {
    auto* pop = new QFrame(this, Qt::Popup);
    pop->setObjectName(QStringLiteral("ebookAaPop"));
    pop->setAttribute(Qt::WA_DeleteOnClose);
    auto* v = new QVBoxLayout(pop);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(8);
    auto row = [&](const QString& label, int part) {
        auto* l = new QLabel(label, pop);
        l->setObjectName(QStringLiteral("ebookAaLabel"));
        v->addWidget(l);
        auto* bar = new EbookLookBar(part, pop);
        bar->setLook(m_look);
        connect(bar, &EbookLookBar::lookChanged, this, [this](const Ebook::Look& l2) {
            m_look = l2;
            m_bar->setLook(l2);
            rebuild();
        });
        v->addWidget(bar);
    };
    row(tr("Tamanho"), EbookLookBar::SizePart);
    row(tr("Letra"), EbookLookBar::FontPart);
    row(tr("Espaço entre linhas"), EbookLookBar::SpacingPart);
    pop->setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#ebookAaPop { background: %1; border: 1px solid %2; border-radius: @radius-panel; }"
        "QLabel#ebookAaLabel { color: %3; font-size: 12px; }"))
        .arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary()));
    pop->adjustSize();
    const QPoint at = m_aaBtn->mapToGlobal(QPoint(m_aaBtn->width() - pop->width(), m_aaBtn->height() + 6));
    pop->move(at);
    pop->show();
}

void EbookRoom::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) { reject(); return; }
    if (e->key() == Qt::Key_Right || e->key() == Qt::Key_Left || e->key() == Qt::Key_PageDown
        || e->key() == Qt::Key_PageUp || e->key() == Qt::Key_Home || e->key() == Qt::Key_End) {
        QApplication::sendEvent(m_view, e);
        return;
    }
    QDialog::keyPressEvent(e);
}

void EbookRoom::applyTheme() {
    const QString panel = Theme::panelBackground();
    const QColor pc = Theme::toColor(panel);
    const bool dark = qGray(pc.rgb()) < 128;
    const QColor stage = dark ? pc.darker(135) : pc.darker(108);
    setStyleSheet(Theme::qss(QStringLiteral(R"(
        QDialog#ebookRoom { background: %9; }
        #ebookRoomTop, #ebookRoomBottom { background: %1; }
        #ebookRoomTop { border-bottom: 1px solid %2; }
        #ebookRoomBottom { border-top: 1px solid %2; }
        #ebookRoomRail { background: %1; border-right: 1px solid %2; }
        #ebookRoomList, QScrollArea#ebookRoomScroll { background: transparent; }
        QLabel#ebookRoomTitle { color: %5; font-family: 'Lora','Crimson Text',serif; font-size: 15px; font-weight: 600; }
        QLabel#ebookRoomSub { color: %4; font-size: 11px; }
        QLabel#ebookRoomCaps { color: %4; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
        QLabel#ebookRoomNote { color: %4; font-size: 11px; border-top: 1px solid %6; padding-top: 10px; }
        QLabel#ebookRoomWhere { color: %3; font-size: 12px; }
        QPushButton#ebookRoomRow { background: transparent; border: none; border-radius: @radius-control; text-align: left; }
        QPushButton#ebookRoomRow:hover { background: %7; }
        QPushButton#ebookRoomRow:checked { background: %8; }
        QPushButton#ebookRoomRow QLabel#ebookRoomRowName { color: %3; font-size: 12px; }
        QPushButton#ebookRoomRow:checked QLabel#ebookRoomRowName { color: %5; }
        QPushButton#ebookRoomRow QLabel#ebookRoomRowNum { color: %4; font-size: 11px; }
        QPushButton#ebookRoomExport { background: %10; color: #ffffff; border: none; border-radius: @radius-control;
                                      padding: 7px 16px; font-size: 12px; font-weight: bold; }
        QToolButton#ebookRoomClose, QToolButton#ebookAa, QToolButton#ebookRoomNav {
            background: transparent; color: %3; border: 1px solid %2; border-radius: @radius-control; font-size: 14px; }
        QToolButton#ebookRoomClose { border: none; font-size: 15px; }
        QToolButton#ebookAa { font-family: 'Source Serif 4','Lora',serif; font-size: 14px; }
        QToolButton#ebookRoomClose:hover, QToolButton#ebookAa:hover, QToolButton#ebookRoomNav:hover { background: %7; color: %5; }
        QToolButton#ebookRoomNav:disabled { color: %6; border-color: %6; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
        QScrollBar::handle:vertical { background: %2; border-radius: 4px; min-height: 30px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )"))
        .arg(panel, Theme::subtleBorder(), Theme::textPrimary(), Theme::textMuted(), Theme::textBright(),
             Theme::subtleBorder(), Theme::hoverOverlay(), Theme::hoverStrong(), stage.name())
        .arg(Theme::accentDefault()));
}
