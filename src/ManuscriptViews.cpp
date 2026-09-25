#include "ManuscriptViews.h"
#include "Theme.h"

#include <QContextMenuEvent>
#include <QCryptographicHash>
#include <QEnterEvent>
#include <QHelpEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QtMath>

namespace {
QColor col(const QString& css) { return Theme::toColor(css); }
QColor withAlpha(QColor c, qreal a) { c.setAlphaF(a); return c; }
QFont uiFont(qreal px, int weight = QFont::Normal) {
    QFont f = QFont(QStringLiteral("Segoe UI"));
    f.setPixelSize(qRound(px));
    f.setWeight(QFont::Weight(weight));
    return f;
}
QFont serifFont(qreal px, int weight = QFont::Normal, bool italic = false) {
    QFont f(QStringLiteral("Lora"));
    f.setPixelSize(qRound(px));
    f.setWeight(QFont::Weight(weight));
    f.setItalic(italic);
    return f;
}
}

// ============================================================ MsPaint

namespace MsPaint {

static QHash<QString, QColor>& overrides() { static QHash<QString, QColor> h; return h; }
void setBookColorOverrides(const QHash<QString, QColor>& colors) { overrides() = colors; }

QColor bookColor(const QString& id) {
    if (overrides().contains(id)) return overrides().value(id);
    const QByteArray h = QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Md5);
    const int hue = (quint8(h.at(0)) * 256 + quint8(h.at(1))) % 360;
    return QColor::fromHsl(hue, 95, 105);
}

QString fmtInt(int n) { return QLocale().toString(n); }

QPixmap avatar(const QPixmap& photo, const QString& name, const QColor& color, int size, qreal dpr) {
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath clip;
    clip.addEllipse(QRectF(0, 0, size, size));
    p.setClipPath(clip);
    if (!photo.isNull()) {
        const QPixmap sc = photo.scaled(QSize(size, size) * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const QPointF off((sc.width() / dpr - size) / 2.0, (sc.height() / dpr - size) / 2.0);
        QPixmap s2 = sc; s2.setDevicePixelRatio(dpr);
        p.drawPixmap(QPointF(-off.x(), -off.y()), s2);
    } else {
        p.fillRect(QRectF(0, 0, size, size), color.darker(135));
        p.setPen(QColor(255, 255, 255, 220));
        p.setFont(uiFont(size * 0.46, QFont::DemiBold));
        p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, name.left(1).toUpper());
    }
    p.setClipping(false);
    p.setPen(QPen(color, qMax(1.0, size / 12.0)));
    p.setBrush(Qt::NoBrush);
    const qreal w = qMax(1.0, size / 12.0) / 2;
    p.drawEllipse(QRectF(w, w, size - 2 * w, size - 2 * w));
    return pm;
}

QPixmap generatedCover(const QString& title, int number, const QColor& color, QSize size, qreal dpr,
                       const QString& bookWord) {
    QPixmap pm(size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(0, 0, size.width(), size.height());
    QLinearGradient g(r.topLeft(), r.bottomRight());
    g.setColorAt(0, color.lighter(118));
    g.setColorAt(1, color.darker(160));
    QPainterPath path;
    path.addRoundedRect(r, 2, 2);
    p.fillPath(path, g);
    p.fillRect(QRectF(0, 0, qMax(2.0, size.width() * 0.06), size.height()), QColor(0, 0, 0, 60));
    const qreal pad = size.width() * 0.12;
    if (size.height() >= 70) {
        p.setPen(QColor(255, 255, 255, 235));
        p.setFont(serifFont(qMax(8.0, size.width() * 0.15), QFont::DemiBold));
        p.drawText(r.adjusted(pad + 2, pad, -pad, -size.height() * 0.3),
                   Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, title);
        p.setPen(QColor(255, 255, 255, 170));
        p.setFont(uiFont(qMax(7.0, size.width() * 0.11), QFont::DemiBold));
        p.drawText(r.adjusted(pad + 2, 0, -pad, -pad), Qt::AlignBottom | Qt::AlignLeft,
                   QStringLiteral("%1 %2").arg(bookWord).arg(number));
    } else {
        p.setPen(QColor(255, 255, 255, 230));
        p.setFont(uiFont(size.width() * 0.42, QFont::Bold));
        p.drawText(r, Qt::AlignCenter, QString::number(number));
    }
    return pm;
}

} // namespace MsPaint

// ============================================================ MsRow

MsRow::MsRow(QWidget* parent) : QToolButton(parent) {
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAutoRaise(true);
    setAttribute(Qt::WA_Hover, true);
}

void MsRow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_painter) m_painter(p, rect(), this);
}

void MsRow::enterEvent(QEnterEvent* e) { m_hovered = true; update(); QToolButton::enterEvent(e); }
void MsRow::leaveEvent(QEvent* e) {
    m_hovered = false;
    if (m_hoverPart != -1) { m_hoverPart = -1; emit partHovered(-1); }
    update();
    QToolButton::leaveEvent(e);
}

void MsRow::mouseMoveEvent(QMouseEvent* e) {
    if (m_hit) {
        const int part = m_hit(e->position().toPoint());
        if (part != m_hoverPart) { m_hoverPart = part; emit partHovered(part); update(); }
    }
    QToolButton::mouseMoveEvent(e);
}

void MsRow::mouseReleaseEvent(QMouseEvent* e) {
    if (m_hit && e->button() == Qt::LeftButton && rect().contains(e->position().toPoint())) {
        const int part = m_hit(e->position().toPoint());
        if (part >= 0) {
            setDown(false);
            emit partClicked(part);
            e->accept();
            return;
        }
    }
    QToolButton::mouseReleaseEvent(e);
}

// ============================================================ MsShelf

MsShelf::MsShelf(Mode mode, QWidget* parent) : QWidget(parent), m_mode(mode) {
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    setFixedHeight(mode == Spines ? 140 : 122);
}

void MsShelf::setBooks(const QList<MsBook>& books, const QString& currentId) {
    m_books = books;
    m_current = currentId;
    m_maxWords = 1;
    for (const auto& b : books) m_maxWords = qMax(m_maxWords, b.words);
    updateGeometry();
    update();
}

int MsShelf::itemWidth(int i) const {
    if (m_mode == Covers) return 60;
    if (i >= m_books.size()) return 22;
    return 22 + qRound(double(m_books.at(i).words) / m_maxWords * 26.0);
}

QRect MsShelf::itemRect(int i) const {
    const int gap = m_mode == Spines ? 5 : 10;
    int x = 12;
    for (int k = 0; k < i; ++k) x += itemWidth(k) + gap;
    const int w = itemWidth(i);
    const int baseY = height() - (m_mode == Spines ? 18 : 16);
    int h;
    if (m_mode == Spines) {
        h = (i >= m_books.size()) ? 84 : 98 + (i % 3) * 7;
    } else {
        h = (i >= m_books.size()) ? 84 : 86;
    }
    int y = baseY - h;
    if (i < m_books.size() && m_books.at(i).id == m_current) y -= 8;   // levantado
    return QRect(x, y, w, h);
}

QSize MsShelf::sizeHint() const {
    const QRect last = itemRect(m_books.size());
    return QSize(last.right() + 14, height());
}

int MsShelf::hitIndex(const QPoint& p) const {
    for (int i = 0; i <= m_books.size(); ++i)
        if (itemRect(i).adjusted(-2, -2, 2, 2).contains(p)) return i;
    return -1;
}

void MsShelf::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal dpr = devicePixelRatioF();
    const int baseY = height() - (m_mode == Spines ? 18 : 16);
    // tábua
    const QColor board = col(Theme::panelBorder());
    p.fillRect(QRect(4, baseY, width() - 8, 5), board);
    p.fillRect(QRect(4, baseY + 5, width() - 8, 3), withAlpha(QColor(0, 0, 0), 0.25));

    const QColor accent = col(Theme::accentDefault());
    for (int i = 0; i < m_books.size(); ++i) {
        const MsBook& b = m_books.at(i);
        const QRect r = itemRect(i);
        const bool cur = (b.id == m_current);
        if (m_mode == Spines) {
            QLinearGradient g(r.topLeft(), r.topRight());
            g.setColorAt(0, b.color.darker(140));
            g.setColorAt(0.35, b.color);
            g.setColorAt(1, b.color.darker(170));
            QPainterPath path;
            path.addRoundedRect(QRectF(r), 2.5, 2.5);
            p.fillPath(path, g);
            p.fillRect(QRect(r.left() + 3, r.top() + 10, r.width() - 6, 2), QColor(255, 255, 255, 60));
            p.fillRect(QRect(r.left() + 3, r.bottom() - 12, r.width() - 6, 2), QColor(255, 255, 255, 60));
            p.setPen(QColor(255, 255, 255, 170));
            p.setFont(uiFont(9, QFont::Bold));
            p.drawText(QRect(r.left(), r.top() + 13, r.width(), 14), Qt::AlignCenter, QString::number(b.number));
            p.save();
            p.translate(r.center().x(), r.center().y() + 6);
            p.rotate(-90);
            p.setPen(QColor(255, 255, 255, 225));
            p.setFont(serifFont(qMin(12, r.width() - 9), QFont::DemiBold));
            const int len = r.height() - 44;
            const QString t = p.fontMetrics().elidedText(b.title, Qt::ElideRight, len);
            p.drawText(QRect(-len / 2, -r.width() / 2, len, r.width()), Qt::AlignCenter, t);
            p.restore();
            if (cur) {
                p.setPen(QPen(accent, 2));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(QRectF(r).adjusted(-2.5, -2.5, 2.5, 2.5), 4, 4);
            } else if (i == m_hover) {
                p.fillPath(path, QColor(255, 255, 255, 25));
            }
        } else {
            const QPixmap pm = b.cover.isNull()
                ? MsPaint::generatedCover(b.title, b.number, b.color, r.size(), dpr, tr("Livro"))
                : b.cover.scaled(r.size() * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            QPixmap draw = pm; draw.setDevicePixelRatio(dpr);
            // sombra
            p.fillRect(r.adjusted(2, 4, 3, 4), QColor(0, 0, 0, 70));
            QPainterPath clip;
            clip.addRoundedRect(QRectF(r), 2, 2);
            p.save();
            p.setClipPath(clip);
            p.drawPixmap(r.topLeft(), draw, QRectF(0, 0, r.width(), r.height()));
            p.restore();
            if (!b.cover.isNull()) {
                // Número discreto no canto, pra capa de verdade também dizer a ordem.
                const QRect badge(r.left() + 4, r.top() + 4, 16, 14);
                p.setBrush(QColor(0, 0, 0, 150));
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(badge, 3, 3);
                p.setPen(QColor(255, 255, 255, 220));
                p.setFont(uiFont(9, QFont::Bold));
                p.drawText(badge, Qt::AlignCenter, QString::number(b.number));
            }
            if (cur) {
                p.setPen(QPen(accent, 2));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(QRectF(r).adjusted(-3, -3, 3, 3), 4, 4);
            } else if (i == m_hover) {
                p.fillPath(clip, QColor(255, 255, 255, 25));
            }
        }
    }
    // "+"
    const QRect a = itemRect(m_books.size());
    QPen dash(m_hover == m_books.size() ? col(Theme::textBright()) : col(Theme::textMuted()), 1.2, Qt::DashLine);
    p.setPen(dash);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(a).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
    p.setFont(uiFont(16));
    p.drawText(a, Qt::AlignCenter, QStringLiteral("+"));
}

void MsShelf::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const int i = hitIndex(e->position().toPoint());
    if (i < 0) return;
    if (i == m_books.size()) emit addClicked();
    else emit bookClicked(m_books.at(i).id);
}

void MsShelf::mouseMoveEvent(QMouseEvent* e) {
    const int i = hitIndex(e->position().toPoint());
    if (i != m_hover) { m_hover = i; update(); }
}

void MsShelf::leaveEvent(QEvent*) { if (m_hover != -1) { m_hover = -1; update(); } }

void MsShelf::contextMenuEvent(QContextMenuEvent* e) {
    const int i = hitIndex(e->pos());
    if (i >= 0 && i < m_books.size()) emit bookContextRequested(m_books.at(i).id, e->globalPos());
}

bool MsShelf::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        const int i = hitIndex(he->pos());
        if (i >= 0 && i < m_books.size()) {
            const MsBook& b = m_books.at(i);
            QToolTip::showText(he->globalPos(), QStringLiteral("%1 · %2").arg(b.title, MsPaint::fmtInt(b.words)), this);
        } else if (i == m_books.size()) {
            QToolTip::showText(he->globalPos(), addLabel, this);
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(e);
}

// ============================================================ MsMosaic

namespace { constexpr int kTile = 34, kTileGap = 5; }

MsMosaic::MsMosaic(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
}

void MsMosaic::setDots(const QList<MsChapterDot>& dots) { m_dots = dots; updateGeometry(); update(); }

int MsMosaic::cols(int w) const { return qMax(1, (w - 8 + kTileGap) / (kTile + kTileGap)); }

int MsMosaic::heightForWidth(int w) const {
    const int c = cols(w);
    const int rows = (m_dots.size() + c - 1) / c;
    return 8 + rows * (kTile + kTileGap);
}

QRect MsMosaic::tileRect(int i) const {
    const int c = cols(width());
    const int used = c * (kTile + kTileGap) - kTileGap;
    const int x0 = qMax(4, (width() - used) / 2);
    return QRect(x0 + (i % c) * (kTile + kTileGap), 4 + (i / c) * (kTile + kTileGap), kTile, kTile);
}

int MsMosaic::hitIndex(const QPoint& p) const {
    for (int i = 0; i < m_dots.size(); ++i) if (tileRect(i).contains(p)) return i;
    return -1;
}

void MsMosaic::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor bright = col(Theme::textBright());
    for (int i = 0; i < m_dots.size(); ++i) {
        const MsChapterDot& d = m_dots.at(i);
        const QRectF r = QRectF(tileRect(i)).adjusted(0.5, 0.5, -0.5, -0.5);
        QColor fill = d.color;
        fill.setAlphaF(d.current ? 0.95 : (i == m_hover ? 0.5 : 0.28));
        p.setBrush(fill);
        QPen pen(d.color, d.current ? 2 : 1);
        if (d.special) pen.setStyle(Qt::DashLine);
        p.setPen(pen);
        p.drawRoundedRect(r, 5, 5);
        p.setPen(d.current ? QColor(20, 20, 20) : bright);
        p.setFont(uiFont(12, QFont::DemiBold));
        p.drawText(r, Qt::AlignCenter, d.shortLabel);
        if (d.dim) {
            p.setPen(Qt::NoPen);
            p.setBrush(withAlpha(col(Theme::panelBackground()), 0.6));
            p.drawRoundedRect(r, 5, 5);
        }
    }
}

void MsMosaic::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const int i = hitIndex(e->position().toPoint());
    if (i >= 0) emit chapterClicked(m_dots.at(i).chapterId);
}

void MsMosaic::mouseMoveEvent(QMouseEvent* e) {
    const int i = hitIndex(e->position().toPoint());
    if (i == m_hover) return;
    m_hover = i;
    update();
    if (i >= 0) emit chapterHovered(m_dots.at(i).chapterId, QRect(mapToGlobal(tileRect(i).topLeft()), tileRect(i).size()));
    else emit chapterHovered(QString(), QRect());
}

void MsMosaic::leaveEvent(QEvent*) {
    if (m_hover != -1) { m_hover = -1; update(); }
    emit chapterHovered(QString(), QRect());
}

void MsMosaic::contextMenuEvent(QContextMenuEvent* e) {
    const int i = hitIndex(e->pos());
    if (i >= 0) emit chapterContextRequested(m_dots.at(i).chapterId, e->globalPos());
}

// ============================================================ MsJourney

namespace { constexpr int kJTop = 46, kJDy = 70; }

MsJourney::MsJourney(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void MsJourney::setData(const QList<MsChapterDot>& dots, const QList<MsBand>& bands) {
    m_dots = dots;
    m_bands = bands;
    setFixedHeight(sizeHint().height());
    updateGeometry();
    update();
}

QSize MsJourney::sizeHint() const {
    return QSize(260, kJTop + qMax(0, int(m_dots.size()) - 1) * kJDy + 40);
}

QPointF MsJourney::nodePos(int i) const {
    const qreal w = width();
    const qreal x = (i % 2 == 0) ? w * 0.26 : w * 0.74;
    return QPointF(x, kJTop + i * kJDy);
}

int MsJourney::hitIndex(const QPoint& p) const {
    for (int i = 0; i < m_dots.size(); ++i) {
        const QPointF c = nodePos(i);
        if (QLineF(c, p).length() <= 18) return i;
        // rótulo também é clicável
        const bool right = (i % 2 == 0);
        const QRectF lbl = right ? QRectF(c.x() + 18, c.y() - 14, width() - c.x() - 22, 30)
                                 : QRectF(4, c.y() - 14, c.x() - 22, 30);
        if (lbl.contains(p)) return i;
    }
    return -1;
}

void MsJourney::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal w = width();
    // Partes = regiões do caminho
    for (const MsBand& b : m_bands) {
        if (b.first < 0 || b.last >= m_dots.size() || b.first > b.last) continue;
        const qreal y1 = nodePos(b.first).y() - 40, y2 = nodePos(b.last).y() + 26;
        QColor bg = b.color; bg.setAlphaF(0.08);
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(QRectF(6, y1, w - 12, y2 - y1), 10, 10);
        QColor tc = b.color; tc.setAlphaF(0.9);
        p.setPen(tc);
        QFont f = uiFont(9, QFont::Bold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
        p.setFont(f);
        p.drawText(QRectF(16, y1 + 4, w - 32, 14), Qt::AlignLeft | Qt::AlignVCenter, b.title.toUpper());
    }
    // caminho pontilhado
    if (m_dots.size() > 1) {
        QPainterPath path(nodePos(0));
        for (int i = 1; i < m_dots.size(); ++i) {
            const QPointF a = nodePos(i - 1), b = nodePos(i);
            const qreal my = (a.y() + b.y()) / 2;
            path.cubicTo(QPointF(a.x(), my), QPointF(b.x(), my), b);
        }
        QPen pen(col(Theme::panelBorder()), 3, Qt::CustomDashLine, Qt::RoundCap);
        pen.setDashPattern({0.3, 2.4});
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
    const QColor panel = col(Theme::panelBackground());
    const QColor bright = col(Theme::textBright()), ink = col(Theme::textPrimary()), muted = col(Theme::textMuted());
    for (int i = 0; i < m_dots.size(); ++i) {
        const MsChapterDot& d = m_dots.at(i);
        const QPointF c = nodePos(i);
        const bool cur = d.current, hov = (i == m_hover);
        const qreal r = cur ? 15 : (hov ? 13.5 : 12);
        if (cur) {
            QColor halo = d.color; halo.setAlphaF(0.3);
            p.setPen(QPen(halo, 2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, 21, 21);
        }
        QPen pen(d.color, 2.5);
        if (d.special) pen.setStyle(Qt::DashLine);
        p.setPen(pen);
        p.setBrush(cur ? d.color : panel);
        p.drawEllipse(c, r, r);
        p.setPen(cur ? QColor(20, 20, 20) : bright);
        p.setFont(uiFont(11, QFont::Bold));
        p.drawText(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), Qt::AlignCenter, d.shortLabel);
        const bool right = (i % 2 == 0);
        const qreal tx = right ? c.x() + 22 : 6;
        const qreal tw = right ? w - c.x() - 28 : c.x() - 28;
        const Qt::Alignment al = (right ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter;
        p.setPen(cur || hov ? bright : ink);
        p.setFont(serifFont(13));
        p.drawText(QRectF(tx, c.y() - 15, tw, 16), al, p.fontMetrics().elidedText(d.title, Qt::ElideRight, int(tw)));
        p.setPen(muted);
        p.setFont(uiFont(9.5));
        p.drawText(QRectF(tx, c.y() + 2, tw, 14), al, d.meta);
    }
}

void MsJourney::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const int i = hitIndex(e->position().toPoint());
    if (i >= 0) emit chapterClicked(m_dots.at(i).chapterId);
}

void MsJourney::mouseMoveEvent(QMouseEvent* e) {
    const int i = hitIndex(e->position().toPoint());
    if (i == m_hover) return;
    m_hover = i;
    setCursor(i >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update();
    if (i >= 0) {
        const QPointF c = nodePos(i);
        emit chapterHovered(m_dots.at(i).chapterId, QRect(mapToGlobal(QPoint(int(c.x()) - 15, int(c.y()) - 15)), QSize(30, 30)));
    } else {
        emit chapterHovered(QString(), QRect());
    }
}

void MsJourney::leaveEvent(QEvent*) {
    if (m_hover != -1) { m_hover = -1; update(); }
    emit chapterHovered(QString(), QRect());
}

void MsJourney::contextMenuEvent(QContextMenuEvent* e) {
    const int i = hitIndex(e->pos());
    if (i >= 0) emit chapterContextRequested(m_dots.at(i).chapterId, e->globalPos());
}

// ============================================================ MsRhythm

MsRhythm::MsRhythm(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(96);
}

void MsRhythm::setDots(const QList<MsChapterDot>& dots) { m_dots = dots; update(); }

QRectF MsRhythm::barRect(int i) const {
    const int n = qMax(1, int(m_dots.size()));
    const qreal area = height() - 16;
    int maxW = 1;
    for (const auto& d : m_dots) maxW = qMax(maxW, d.words);
    const qreal slot = qreal(width() - 2) / n;
    const qreal gap = qMin(3.0, slot * 0.25);
    const qreal h = qMax(3.0, area * m_dots.at(i).words / maxW);
    return QRectF(1 + i * slot + gap / 2, area - h, slot - gap, h);
}

int MsRhythm::hitIndex(const QPoint& p) const {
    if (m_dots.isEmpty()) return -1;
    const qreal slot = qreal(width() - 2) / m_dots.size();
    const int i = int((p.x() - 1) / slot);
    return (i >= 0 && i < m_dots.size()) ? i : -1;
}

void MsRhythm::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_dots.isEmpty()) return;
    const qreal area = height() - 16;
    int maxW = 1, sum = 0, n = 0;
    for (const auto& d : m_dots) { maxW = qMax(maxW, d.words); if (d.words) { sum += d.words; ++n; } }
    const QColor ink = col(Theme::textPrimary()), accent = col(Theme::accentDefault()), muted = col(Theme::textMuted());
    for (int i = 0; i < m_dots.size(); ++i) {
        const MsChapterDot& d = m_dots.at(i);
        const QRectF r = barRect(i);
        QColor body = ink; body.setAlphaF(d.current || i == m_hover ? 0.75 : (d.special ? 0.25 : 0.42));
        p.setPen(Qt::NoPen);
        p.setBrush(body);
        p.drawRoundedRect(r, 1.5, 1.5);
        if (d.dialogue > 0) {
            QRectF dr = r;
            dr.setHeight(r.height() * d.dialogue);
            QColor dc = accent; dc.setAlphaF(d.current || i == m_hover ? 1.0 : 0.8);
            p.setBrush(dc);
            p.drawRoundedRect(dr, 1.5, 1.5);
        }
        if (d.current) {
            p.setPen(QPen(col(Theme::textBright()), 1.2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(-1, -1, 1, 1), 2, 2);
        }
    }
    if (n) {
        const qreal y = area - area * (double(sum) / n) / maxW;
        QPen pen(muted, 1, Qt::DashLine);
        p.setPen(pen);
        p.drawLine(QPointF(0, y), QPointF(width(), y));
    }
    // eixo: rótulos quando cabem
    p.setPen(muted);
    p.setFont(uiFont(9));
    const qreal slot = qreal(width() - 2) / m_dots.size();
    const int step = slot >= 14 ? 1 : (slot >= 7 ? 2 : 5);
    for (int i = 0; i < m_dots.size(); i += step) {
        const QRectF r = barRect(i);
        p.drawText(QRectF(r.center().x() - 12, area + 2, 24, 13), Qt::AlignCenter, m_dots.at(i).shortLabel);
    }
}

void MsRhythm::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const int i = hitIndex(e->position().toPoint());
    if (i >= 0) emit chapterClicked(m_dots.at(i).chapterId);
}

void MsRhythm::mouseMoveEvent(QMouseEvent* e) {
    const int i = hitIndex(e->position().toPoint());
    if (i != m_hover) { m_hover = i; update(); emit hoverChanged(i); }
}

void MsRhythm::leaveEvent(QEvent*) { if (m_hover != -1) { m_hover = -1; update(); emit hoverChanged(-1); } }

// ============================================================ MsCrossing

MsCrossing::MsCrossing(QWidget* parent) : QWidget(parent) { setFixedHeight(78); }

void MsCrossing::setData(const QList<int>& storyPos, const QList<bool>& jump,
                         const QString& readLabel, const QString& storyLabel) {
    m_story = storyPos; m_jump = jump; m_readLabel = readLabel; m_storyLabel = storyLabel;
    update();
}

void MsCrossing::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int n = m_story.size();
    if (n < 2) return;
    const qreal y0 = 18, y1 = height() - 16;
    const qreal x0 = 10, x1 = width() - 10;
    auto X = [&](int i) { return x0 + i * (x1 - x0) / (n - 1); };
    const QColor warn = col(Theme::accentWarning());
    QColor faint = col(Theme::textPrimary()); faint.setAlphaF(0.3);
    for (int pass = 0; pass < 2; ++pass) {           // saltos por cima
        for (int i = 0; i < n; ++i) {
            const bool j = m_jump.value(i);
            if ((pass == 1) != j) continue;
            const qreal xa = X(i), xb = X(m_story.at(i));
            QPainterPath path(QPointF(xa, y0));
            const qreal my = (y0 + y1) / 2;
            path.cubicTo(QPointF(xa, my), QPointF(xb, my), QPointF(xb, y1));
            p.setPen(QPen(j ? warn : faint, j ? 2 : 1.2));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        }
    }
    p.setPen(Qt::NoPen);
    p.setBrush(col(Theme::textPrimary()));
    for (int i = 0; i < n; ++i) p.drawEllipse(QPointF(X(i), y0), 2.6, 2.6);
    p.setBrush(col(Theme::accentInfo()));
    for (int i = 0; i < n; ++i) p.drawEllipse(QPointF(X(i), y1), 2.6, 2.6);
    QFont f = uiFont(8.5, QFont::DemiBold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1);
    p.setFont(f);
    p.setPen(col(Theme::textMuted()));
    p.drawText(QRectF(0, 0, width(), 11), Qt::AlignLeft | Qt::AlignTop, m_readLabel.toUpper());
    p.setPen(col(Theme::accentInfo()));
    p.drawText(QRectF(0, height() - 11, width(), 11), Qt::AlignLeft | Qt::AlignBottom, m_storyLabel.toUpper());
}

// ============================================================ MsPovStrip

MsPovStrip::MsPovStrip(QWidget* parent) : QWidget(parent) {
    setFixedHeight(14);
    setCursor(Qt::PointingHandCursor);
}

void MsPovStrip::setSegments(const QList<Seg>& segs) { m_segs = segs; update(); }

int MsPovStrip::hitIndex(const QPoint& p) const {
    if (m_segs.isEmpty()) return -1;
    const int i = int(p.x() * m_segs.size() / qMax(1, width()));
    return (i >= 0 && i < m_segs.size()) ? i : -1;
}

void MsPovStrip::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int n = m_segs.size();
    if (!n) return;
    const qreal slot = qreal(width()) / n;
    for (int i = 0; i < n; ++i) {
        const Seg& s = m_segs.at(i);
        QColor c = s.color;
        if (s.dim) c.setAlphaF(0.22);
        const QRectF r(i * slot + 1, s.current ? 0 : 3, slot - 2, s.current ? height() : height() - 6);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(r, 2, 2);
    }
}

void MsPovStrip::mousePressEvent(QMouseEvent* e) {
    const int i = hitIndex(e->position().toPoint());
    if (i >= 0 && e->button() == Qt::LeftButton) emit chapterClicked(m_segs.at(i).chapterId);
}

bool MsPovStrip::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        const int i = hitIndex(he->pos());
        if (i >= 0) QToolTip::showText(he->globalPos(), m_segs.at(i).tip, this);
        else QToolTip::hideText();
        return true;
    }
    return QWidget::event(e);
}
