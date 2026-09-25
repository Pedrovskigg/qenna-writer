#include "DrawerViews.h"
#include "PanelMotion.h"
#include "Theme.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QSet>
#include <QTimer>
#include <QtMath>

namespace {
QColor tc(const QString& css) { return Theme::toColor(css); }
QFont ui(qreal px, int weight = QFont::Normal) {
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(qRound(px));
    f.setWeight(QFont::Weight(weight));
    return f;
}
QFont serif(qreal px, int weight = QFont::Normal, bool italic = false) {
    QFont f(QStringLiteral("Lora"));
    f.setPixelSize(qRound(px));
    f.setWeight(QFont::Weight(weight));
    f.setItalic(italic);
    return f;
}
QFont hand(qreal px) {
    QFont f;
    f.setFamilies({ QStringLiteral("Caveat"), QStringLiteral("Segoe Print"), QStringLiteral("Comic Sans MS") });
    f.setPixelSize(qRound(px));
    return f;
}
QColor alpha(QColor c, qreal a) { c.setAlphaF(a); return c; }

// Tirinha de presença por capítulo (ferramenta Aparições).
void paintAppears(QPainter& p, const QRectF& r, const QList<bool>& ap, const QColor& on) {
    if (ap.isEmpty()) return;
    const qreal gap = 2, w = (r.width() - gap * (ap.size() - 1)) / ap.size();
    for (int i = 0; i < ap.size(); ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(ap.at(i) ? on : alpha(tc(Theme::textPrimary()), 0.14));
        p.drawRoundedRect(QRectF(r.left() + i * (w + gap), r.top(), w, r.height()), 1.5, 1.5);
    }
}

qreal segDist(const QPointF& p, const QPointF& a, const QPointF& b) {
    const QPointF ab = b - a;
    const qreal len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 <= 0.0001) return QLineF(p, a).length();
    const qreal t = qBound(0.0, QPointF::dotProduct(p - a, ab) / len2, 1.0);
    return QLineF(p, a + ab * t).length();
}
}

namespace DwPaint {
QPixmap face(const QPixmap& photo, const QString& name, const QColor& ring, int size, qreal dpr, bool round) {
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath clip;
    if (round) clip.addEllipse(QRectF(0, 0, size, size));
    else clip.addRoundedRect(QRectF(0, 0, size, size), 4, 4);
    p.setClipPath(clip);
    if (!photo.isNull()) {
        QPixmap sc = photo.scaled(QSize(size, size) * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        sc.setDevicePixelRatio(dpr);
        p.drawPixmap(QPointF((size - sc.width() / dpr) / 2.0, (size - sc.height() / dpr) / 2.0), sc);
    } else {
        QRadialGradient g(QPointF(size * 0.35, size * 0.25), size);
        const QColor base = ring.isValid() ? ring : tc(Theme::accentDefault());
        g.setColorAt(0, base.lighter(115));
        g.setColorAt(0.8, base.darker(250));
        p.fillRect(QRectF(0, 0, size, size), g);
        p.setPen(QColor(255, 255, 255, 200));
        p.setFont(serif(size * 0.34, QFont::DemiBold));
        QString ini;
        for (const QString& w : name.split(QLatin1Char(' '), Qt::SkipEmptyParts)) { ini += w.left(1).toUpper(); if (ini.size() == 2) break; }
        p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, ini.isEmpty() ? QStringLiteral("?") : ini);
    }
    return pm;
}
}

// ============================================================ base de arraste

DwBondDragBase::DwBondDragBase(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
}

void DwBondDragBase::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    m_pressPos = e->position();
    m_pressItem = hitItem(m_pressPos);
    m_pressOnHandle = bondsEnabled && m_pressItem >= 0 && isDragHandle(m_pressItem, m_pressPos);
    m_dragging = false;
    emit itemHovered(QString(), QRect());
}

void DwBondDragBase::mouseMoveEvent(QMouseEvent* e) {
    const QPointF pos = e->position();
    if ((e->buttons() & Qt::LeftButton) && m_pressOnHandle) {
        if (!m_dragging && QLineF(pos, m_pressPos).length() >= QApplication::startDragDistance()) m_dragging = true;
        if (m_dragging) {
            m_dragPos = pos;
            const int t = hitItem(pos);
            m_dropTarget = (t != m_pressItem) ? t : -1;
            setCursor(Qt::ClosedHandCursor);
            update();
            return;
        }
    }
    const int h = hitItem(pos);
    if (h != m_hover) {
        m_hover = h;
        hoverChanged();
        if (h >= 0) {
            const QRectF r = itemRect(h);
            emit itemHovered(itemId(h), QRect(mapToGlobal(r.topLeft().toPoint()), r.size().toSize()));
        } else {
            emit itemHovered(QString(), QRect());
        }
    }
    if (h >= 0 && bondsEnabled && isDragHandle(h, pos)) setCursor(Qt::OpenHandCursor);
    else if (h >= 0 || !hitBond(pos).isEmpty()) setCursor(Qt::PointingHandCursor);
    else setCursor(Qt::ArrowCursor);
}

void DwBondDragBase::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const QPointF pos = e->position();
    if (m_dragging) {
        const int target = m_dropTarget;
        const int from = m_pressItem;
        m_dragging = false;
        m_dropTarget = -1;
        m_pressItem = -1;
        setCursor(Qt::ArrowCursor);
        update();
        if (target >= 0 && from >= 0 && target != from)
            emit bondCreateRequested(itemId(from), itemId(target), e->globalPosition().toPoint());
        return;
    }
    const int rel = hitItem(pos);
    const int pressed = m_pressItem;
    m_pressItem = -1;
    if (pressed >= 0 && rel == pressed) {
        emit itemActivated(itemId(rel));
        return;
    }
    if (pressed < 0) {
        const QString b = hitBond(pos);
        if (!b.isEmpty()) { emit bondClicked(b, e->globalPosition().toPoint()); return; }
    }
}

void DwBondDragBase::leaveEvent(QEvent*) {
    if (m_hover != -1) { m_hover = -1; hoverChanged(); }
    emit itemHovered(QString(), QRect());
}

void DwBondDragBase::contextMenuEvent(QContextMenuEvent* e) {
    const int i = hitItem(e->pos());
    if (i >= 0) emit itemContextRequested(itemId(i), e->globalPos());
}

void DwBondDragBase::startIntro(int delayMs) {
    if (!PanelMotion::enabled()) return;
    m_intro = true;
    m_introDelay = delayMs;
    m_introClock.start();
    if (!m_introTimer) {
        m_introTimer = new QTimer(this);
        m_introTimer->setInterval(16);
        connect(m_introTimer, &QTimer::timeout, this, [this]() {
            if (m_introClock.elapsed() > PanelMotion::cascadeTotalMs(m_introDelay)) {
                m_intro = false;
                m_introTimer->stop();
            }
            update();
        });
    }
    m_introTimer->start();
    update();
}

void DwBondDragBase::applyIntro(QPainter& p, int order) const {
    if (!m_intro) return;
    const qreal e = PanelMotion::cascadeProgress(order, m_introClock.elapsed(), m_introDelay);
    p.setOpacity(p.opacity() * e);
    p.translate((PanelMotion::barOnRight() ? 1 : -1) * PanelMotion::kCascadeShift * (1.0 - e), 0);
}

void DwBondDragBase::paintDragLine(QPainter& p) {
    if (!m_dragging || m_pressItem < 0) return;
    const QColor acc = tc(Theme::accentDefault());
    QPen pen(acc, 2, Qt::DashLine);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawLine(dragAnchor(m_pressItem), m_dragPos);
    p.setPen(Qt::NoPen);
    p.setBrush(acc);
    p.drawEllipse(m_dragPos, 4, 4);
    if (m_dropTarget >= 0) {
        p.setPen(QPen(acc, 2, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(itemRect(m_dropTarget).adjusted(-3, -3, 3, 3), 6, 6);
    }
}

// ============================================================ Retratos

namespace { constexpr qreal kFace = 66; }

DwPortraits::DwPortraits(QWidget* parent) : DwBondDragBase(parent) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void DwPortraits::setData(const QList<DwEntry>& entries, const QList<DwSection>& sections,
                          const QList<DwBond>& bonds, const QColor& accent) {
    m_entries = entries;
    m_sections = sections;
    m_bonds = bonds;
    m_accent = accent;
    relayout();
    update();
}

void DwPortraits::resizeEvent(QResizeEvent*) { relayout(); }

void DwPortraits::relayout() {
    m_rows.clear();
    m_heads.clear();
    // Margem dos arcos proporcional a quem tem mais vínculos: sem vínculo
    // nenhum, nada de faixa vazia do lado.
    int maxDeg = 0;
    if (bondsEnabled) {
        QHash<QString, int> deg;
        for (const DwBond& b : m_bonds) { deg[b.from] += 1; deg[b.to] += 1; }
        for (int d : deg) maxDeg = qMax(maxDeg, d);
    }
    m_arcMargin = maxDeg ? qMin(44, 12 + 6 * maxDeg) : 4;
    const qreal left = m_arcMargin;
    const qreal w = qMax(120, width()) - left - 4;
    qreal y = 4;
    for (const DwSection& s : m_sections) {
        if (!s.title.isEmpty()) { m_heads.append({ s.title, y }); y += 28; }
        for (int idx : s.rows) {
            if (idx < 0 || idx >= m_entries.size()) continue;
            const bool ap = !m_entries.at(idx).appears.isEmpty();
            const qreal h = kFace + 18 + (ap ? 22 : 0);
            m_rows.insert(idx, QRectF(left, y, w, h));
            y += h + 2;
        }
    }
    m_height = int(y + 8);
    setFixedHeight(m_height);
}

int DwPortraits::hitItem(const QPointF& p) const {
    for (auto it = m_rows.cbegin(); it != m_rows.cend(); ++it)
        if (it.value().contains(p)) return it.key();
    return -1;
}

bool DwPortraits::isDragHandle(int item, const QPointF& p) const {
    const QRectF r = itemRect(item);
    return QLineF(p, QPointF(r.left() + 8 + kFace / 2, r.top() + 9 + kFace / 2)).length() <= kFace / 2 + 2;
}

QPointF DwPortraits::dragAnchor(int item) const {
    const QRectF r = itemRect(item);
    return QPointF(r.left() + 8 + kFace / 2, r.top() + 9 + kFace / 2);
}

QRectF DwPortraits::itemRect(int item) const { return m_rows.value(item); }
QString DwPortraits::itemId(int item) const { return m_entries.value(item).id; }

void DwPortraits::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal dpr = devicePixelRatioF();
    const QColor muted = tc(Theme::textMuted()), bright = tc(Theme::textBright());

    for (const auto& h : m_heads) {
        QFont f = ui(10, QFont::Bold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
        p.setFont(f);
        p.setPen(muted);
        const QRectF r(m_arcMargin + 6, h.second + 8, width() - m_arcMargin - 12, 16);
        const qreal tw = QFontMetricsF(f).horizontalAdvance(h.first.toUpper());
        p.drawText(r, Qt::AlignVCenter | Qt::AlignLeft, h.first.toUpper());
        p.setPen(QPen(tc(Theme::subtleBorder()), 1));
        p.drawLine(QPointF(r.left() + tw + 8, r.center().y()), QPointF(r.right(), r.center().y()));
    }

    // Quem está em foco (mouse ou arraste) acende os próprios vínculos.
    const int focusIdx = m_dragging ? m_pressItem : m_hover;
    const QString focus = focusIdx >= 0 ? itemId(focusIdx) : QString();
    QHash<QString, const DwBond*> linkTo;
    if (!focus.isEmpty())
        for (const DwBond& b : m_bonds) {
            if (b.from == focus) linkTo.insert(b.to, &b);
            else if (b.to == focus) linkTo.insert(b.from, &b);
        }

    for (auto it = m_rows.cbegin(); it != m_rows.cend(); ++it) {
        const DwEntry& e = m_entries.at(it.key());
        const QRectF r = it.value();
        p.save();
        applyIntro(p, it.key());
        if (it.key() == m_hover && !m_dragging) {
            p.setPen(Qt::NoPen);
            p.setBrush(tc(Theme::hoverOverlay()));
            p.drawRoundedRect(r, 8, 8);
        }
        const QRectF faceR(r.left() + 8, r.top() + 9, kFace, kFace);
        p.setPen(QPen(alpha(m_accent, 0.6), 1));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(faceR.adjusted(-3, -3, 3, 3));
        p.drawPixmap(faceR.topLeft(), DwPaint::face(e.photo, e.title, m_accent, int(kFace), dpr));
        const qreal x = faceR.right() + 12;
        qreal right = r.right() - 8;
        if (const DwBond* b = linkTo.value(e.id, nullptr)) {
            p.setFont(ui(10, QFont::DemiBold));
            const qreal tw = QFontMetricsF(p.font()).horizontalAdvance(b->label) + 14;
            const QRectF tag(right - tw, r.top() + 22, tw, 18);
            p.setPen(QPen(b->color, 1));
            p.setBrush(alpha(b->color, 0.12));
            p.drawRoundedRect(tag, 9, 9);
            p.setPen(b->color);
            p.drawText(tag, Qt::AlignCenter, b->label);
            right = tag.left() - 8;
        }
        p.setFont(serif(16, QFont::DemiBold));
        p.setPen(bright);
        p.drawText(QRectF(x, r.top() + 10, right - x, 22), Qt::AlignVCenter | Qt::AlignLeft,
                   QFontMetricsF(p.font()).elidedText(e.title, Qt::ElideRight, right - x));
        if (!e.oneLine.isEmpty()) {
            p.setFont(serif(12.5, QFont::Normal, true));
            p.setPen(muted);
            const QRectF lr(x, r.top() + 34, right - x, 40);
            p.drawText(lr, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, e.oneLine);
        } else if (!e.role.isEmpty()) {
            QFont f = ui(9.5, QFont::Bold);
            f.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
            p.setFont(f);
            p.setPen(tc(Theme::accentInfo()));
            p.drawText(QRectF(x, r.top() + 36, right - x, 14), Qt::AlignLeft | Qt::AlignVCenter, e.role.toUpper());
        }
        if (!e.appears.isEmpty()) {
            paintAppears(p, QRectF(x, r.top() + kFace + 16, r.right() - 8 - x, 6), e.appears, m_accent);
            p.setFont(ui(10.5));
            p.setPen(muted);
            p.drawText(QRectF(x, r.top() + kFace + 23, r.right() - 8 - x, 14), Qt::AlignLeft | Qt::AlignVCenter, e.appearsMeta);
        }
        if (m_dropTarget == it.key()) {
            p.setPen(QPen(tc(Theme::accentDefault()), 2, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 8, 8);
        }
        p.restore();
    }

    // Arcos na margem esquerda, só de quem está em foco.
    if (!focus.isEmpty() && bondsEnabled) {
        const qreal x0 = m_arcMargin - 3;
        const QRectF fr = m_rows.value(focusIdx);
        const qreal y0 = fr.top() + 9 + kFace / 2;
        int arcN = 0;
        for (auto it = linkTo.cbegin(); it != linkTo.cend(); ++it) {
            int other = -1;
            for (auto r = m_rows.cbegin(); r != m_rows.cend(); ++r)
                if (m_entries.at(r.key()).id == it.key()) other = r.key();
            if (other < 0) continue;
            const qreal y1 = m_rows.value(other).top() + 9 + kFace / 2;
            // Cada vínculo num arco próprio, um dentro do outro.
            const qreal k = qMin(x0 - 2, 6.0 + 6.0 * arcN++);
            QPainterPath path(QPointF(x0, y0));
            path.cubicTo(QPointF(x0 - k, y0), QPointF(x0 - k, y1), QPointF(x0, y1));
            p.setPen(QPen(it.value()->color, 2));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
            p.setPen(Qt::NoPen);
            p.setBrush(it.value()->color);
            p.drawEllipse(QPointF(x0, y1), 3, 3);
        }
        p.setBrush(tc(Theme::textBright()));
        p.drawEllipse(QPointF(x0, y0), 3.5, 3.5);
    }
    paintDragLine(p);
}

// ============================================================ Polaroid

namespace {
const qreal kRots[] = { -3, 2.5, -1.5, 3, -2.5, 1.5, -2, 2 };
constexpr qreal kPolTop = 20, kPolGapX = 14, kPolGapY = 24, kPolSide = 12;
}

DwPolaroid::DwPolaroid(QWidget* parent) : DwBondDragBase(parent) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void DwPolaroid::setData(const QList<DwEntry>& entries, const QList<DwBond>& bonds, const QColor& pin,
                         const QString& legendTitle) {
    m_entries = entries;
    m_bonds = bonds;
    m_pin = pin;
    m_legendTitle = legendTitle;
    relayout();
    update();
}

void DwPolaroid::resizeEvent(QResizeEvent*) { relayout(); }

void DwPolaroid::relayout() {
    m_rects.clear();
    const qreal w = qMax(160, width());
    const int cols = qMax(2, int((w - 2 * kPolSide + kPolGapX) / 150));
    const qreal cellW = (w - 2 * kPolSide - (cols - 1) * kPolGapX) / cols;
    bool anyAp = false;
    for (const auto& e : m_entries) if (!e.appears.isEmpty()) anyAp = true;
    const qreal photoH = (cellW - 16) * 0.78;
    const qreal polH = 8 + photoH + 44 + (anyAp ? 10 : 0);
    for (int i = 0; i < m_entries.size(); ++i) {
        const int c = i % cols, r = i / cols;
        m_rects.append(QRectF(kPolSide + c * (cellW + kPolGapX), kPolTop + r * (polH + kPolGapY), cellW, polH));
    }
    const int rows = (m_entries.size() + cols - 1) / cols;
    m_legendY = kPolTop + rows * (polH + kPolGapY) - 6;
    bool hasLegend = bondsEnabled && !m_bonds.isEmpty();
    m_height = int(m_legendY + (hasLegend ? 34 : 8));
    setFixedHeight(m_height);
}

QPointF DwPolaroid::pinPos(int i) const {
    const QRectF r = m_rects.value(i);
    if (i == m_hover && !m_dragging) return QPointF(r.center().x(), r.top());
    const qreal a = qDegreesToRadians(kRots[i % 8]);
    const qreal hh = r.height() / 2;
    return QPointF(r.center().x() + hh * qSin(a), r.center().y() - hh * qCos(a));
}

int DwPolaroid::hitItem(const QPointF& p) const {
    for (int i = m_rects.size() - 1; i >= 0; --i)
        if (m_rects.at(i).adjusted(-2, -10, 2, 2).contains(p)) return i;
    return -1;
}

bool DwPolaroid::isDragHandle(int item, const QPointF& p) const {
    return QLineF(p, pinPos(item)).length() <= 10;
}

QPointF DwPolaroid::dragAnchor(int item) const { return pinPos(item); }
QRectF DwPolaroid::itemRect(int item) const { return m_rects.value(item); }
QString DwPolaroid::itemId(int item) const { return m_entries.value(item).id; }

void DwPolaroid::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal dpr = devicePixelRatioF();
    // Fundo = o da própria gaveta (sem cortiça pintada por cima).
    auto drawPol = [&](int i) {
        const DwEntry& e = m_entries.at(i);
        const QRectF r = m_rects.at(i);
        const bool hov = (i == m_hover && !m_dragging);
        p.save();
        p.translate(r.center());
        p.rotate(hov ? 0 : kRots[i % 8]);
        if (hov) p.scale(1.04, 1.04);
        const QRectF lr(-r.width() / 2, -r.height() / 2, r.width(), r.height());
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 110));
        p.drawRect(lr.translated(2, 5));
        p.setBrush(QColor(0xef, 0xea, 0xe0));
        p.drawRect(lr);
        const qreal photoH = (r.width() - 16) * 0.78;
        const QRectF ph(lr.left() + 8, lr.top() + 8, lr.width() - 16, photoH);
        if (!e.photo.isNull()) {
            QPixmap sc = e.photo.scaled((ph.size() * dpr).toSize(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            sc.setDevicePixelRatio(dpr);
            p.save();
            p.setClipRect(ph);
            p.drawPixmap(QPointF(ph.center().x() - sc.width() / dpr / 2, ph.center().y() - sc.height() / dpr / 2), sc);
            p.restore();
        } else {
            const QPixmap f = DwPaint::face(QPixmap(), e.title, m_pin, int(qMin(ph.width(), ph.height())), dpr, false);
            p.fillRect(ph, m_pin.darker(260));
            p.drawPixmap(QPointF(ph.center().x() - f.width() / dpr / 2, ph.center().y() - f.height() / dpr / 2), f);
        }
        p.setPen(QColor(0x2b, 0x28, 0x22));
        p.setFont(hand(19));
        p.drawText(QRectF(lr.left() + 4, ph.bottom() + 4, lr.width() - 8, 24), Qt::AlignCenter,
                   QFontMetricsF(p.font()).elidedText(e.title, Qt::ElideRight, lr.width() - 10));
        if (!e.role.isEmpty()) {
            QFont f = ui(8.5, QFont::DemiBold);
            f.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
            p.setFont(f);
            p.setPen(QColor(0x8a, 0x6a, 0x55));
            p.drawText(QRectF(lr.left(), ph.bottom() + 27, lr.width(), 12), Qt::AlignCenter, e.role.toUpper());
        }
        if (!e.appears.isEmpty())
            paintAppears(p, QRectF(lr.left() + 12, lr.bottom() - 10, lr.width() - 24, 4), e.appears, m_pin.darker(120));
        if (m_dropTarget == i) {
            p.setPen(QPen(tc(Theme::accentDefault()), 2, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(lr.adjusted(-4, -4, 4, 4));
        }
        p.restore();
    };
    for (int i = 0; i < m_entries.size(); ++i) if (i != m_hover) { p.save(); applyIntro(p, i); drawPol(i); p.restore(); }
    if (m_hover >= 0 && m_hover < m_entries.size()) { p.save(); applyIntro(p, m_hover); drawPol(m_hover); p.restore(); }

    // Barbante entre os alfinetes (quadro de detetive).
    QHash<QString, int> idx;
    for (int i = 0; i < m_entries.size(); ++i) idx.insert(m_entries.at(i).id, i);
    const QString hovId = (m_hover >= 0 && !m_dragging) ? itemId(m_hover) : QString();
    if (bondsEnabled) {
        for (const DwBond& b : m_bonds) {
            if (!idx.contains(b.from) || !idx.contains(b.to)) continue;
            const QPointF a = pinPos(idx.value(b.from)), c = pinPos(idx.value(b.to));
            const bool on = !hovId.isEmpty() && (b.from == hovId || b.to == hovId);
            const qreal op = hovId.isEmpty() ? 0.6 : (on ? 0.95 : 0.1);
            const qreal sw = on ? 2.3 : 1.7;
            const qreal sag = 18 + QLineF(a, c).length() * 0.08;
            QPainterPath path(a);
            path.quadTo(QPointF((a.x() + c.x()) / 2, (a.y() + c.y()) / 2 + sag), c);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(0, 0, 0, int(115 * op)), sw + 0.4));
            p.drawPath(path.translated(1.5, 3));
            p.setPen(QPen(alpha(b.color, op), sw));
            p.drawPath(path);
        }
    }
    for (int i = 0; i < m_entries.size(); ++i) {
        const QPointF c = pinPos(i);
        QRadialGradient g(c + QPointF(-2, -2), 7);
        g.setColorAt(0, m_pin.lighter(150));
        g.setColorAt(0.55, m_pin);
        g.setColorAt(1, m_pin.darker(220));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 90));
        p.drawEllipse(c + QPointF(1, 2), 6.5, 6.5);
        p.setBrush(g);
        p.drawEllipse(c, 6.5, 6.5);
    }
    // legenda dos tipos
    if (bondsEnabled && !m_bonds.isEmpty()) {
        QList<QPair<QString, QColor>> types;
        QSet<QString> seen;
        for (const DwBond& b : m_bonds) if (!seen.contains(b.label)) { seen.insert(b.label); types.append({ b.label, b.color }); }
        p.setFont(ui(10.5));
        qreal x = kPolSide + 6;
        const QRectF strip(kPolSide, m_legendY + 4, width() - 2 * kPolSide, 22);
        p.setPen(Qt::NoPen);
        QColor stripBg = tc(Theme::appBackground());
        stripBg.setAlphaF(0.7);
        p.setBrush(stripBg);
        p.drawRoundedRect(strip, 4, 4);
        const QFontMetricsF fm(p.font());
        for (const auto& t : types) {
            const qreal tw = fm.horizontalAdvance(t.first);
            if (x + 22 + tw > strip.right()) break;
            p.fillRect(QRectF(x, strip.center().y() - 1, 14, 2), t.second);
            p.setPen(tc(Theme::textMuted()));
            p.drawText(QRectF(x + 18, strip.top(), tw + 4, strip.height()), Qt::AlignVCenter | Qt::AlignLeft, t.first);
            x += 18 + tw + 14;
        }
    }
    paintDragLine(p);
}

// ============================================================ Crachás

namespace { constexpr qreal kBadgePhoto = 78, kBadgeGap = 8, kBadgeSide = 10; }

DwBadges::DwBadges(QWidget* parent) : DwBondDragBase(parent) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void DwBadges::setData(const QList<DwEntry>& entries, const QList<DwBond>& bonds, const QColor& accent) {
    m_entries = entries;
    m_bonds = bonds;
    m_accent = accent;
    relayout();
    update();
}

void DwBadges::resizeEvent(QResizeEvent*) { relayout(); }

void DwBadges::relayout() {
    m_rects.clear();
    const qreal w = qMax(180, width()) - 2 * kBadgeSide;
    qreal y = 8;
    for (const auto& e : m_entries) {
        qreal h = 9 + 20 + 4 + 16 + 9;                 // nome + faixa de dados
        if (!e.role.isEmpty()) h += 15 + 4;
        if (!e.facts.isEmpty()) h += 17;
        if (bondsEnabled) h += 18;
        if (!e.appears.isEmpty()) h += 12;
        h = qMax<qreal>(h, 96);
        m_rects.append(QRectF(kBadgeSide, y, w, h));
        y += h + kBadgeGap;
    }
    m_height = int(y + 6);
    setFixedHeight(m_height);
}

int DwBadges::hitItem(const QPointF& p) const {
    for (int i = 0; i < m_rects.size(); ++i) if (m_rects.at(i).contains(p)) return i;
    return -1;
}

bool DwBadges::isDragHandle(int item, const QPointF& p) const {
    const QRectF r = m_rects.value(item);
    return QRectF(r.left(), r.top(), kBadgePhoto, r.height()).contains(p);
}

QPointF DwBadges::dragAnchor(int item) const {
    const QRectF r = m_rects.value(item);
    return QPointF(r.left() + kBadgePhoto / 2, r.center().y());
}

QRectF DwBadges::itemRect(int item) const { return m_rects.value(item); }
QString DwBadges::itemId(int item) const { return m_entries.value(item).id; }

void DwBadges::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal dpr = devicePixelRatioF();
    const QColor accent = m_accent.isValid() ? m_accent : tc(Theme::accentDefault());
    for (int i = 0; i < m_entries.size(); ++i) {
        const DwEntry& e = m_entries.at(i);
        const QRectF r = m_rects.at(i);
        const bool hov = (i == m_hover && !m_dragging);
        p.save();
        applyIntro(p, i);
        QPainterPath card;
        card.addRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        p.setPen(QPen(hov ? alpha(accent, 0.85) : tc(Theme::subtleBorder()), 1));
        p.setBrush(tc(Theme::hoverOverlay()));
        p.drawPath(card);

        // Foto na altura toda do crachá.
        const QRectF ph(r.left(), r.top(), kBadgePhoto, r.height());
        p.save();
        p.setClipPath(card);
        if (!e.photo.isNull()) {
            QPixmap sc = e.photo.scaled((ph.size() * dpr).toSize(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            sc.setDevicePixelRatio(dpr);
            p.setClipRect(ph, Qt::IntersectClip);
            p.drawPixmap(QPointF(ph.center().x() - sc.width() / dpr / 2, ph.center().y() - sc.height() / dpr / 2), sc);
        } else {
            QRadialGradient g(QPointF(ph.left() + ph.width() * 0.35, ph.top() + ph.height() * 0.3), ph.height());
            g.setColorAt(0, accent.lighter(115));
            g.setColorAt(0.85, accent.darker(260));
            p.fillRect(ph, g);
            QString ini;
            for (const QString& w : e.title.split(QLatin1Char(' '), Qt::SkipEmptyParts)) { ini += w.left(1).toUpper(); if (ini.size() == 2) break; }
            p.setPen(QColor(255, 255, 255, 210));
            p.setFont(serif(21, QFont::DemiBold));
            p.drawText(ph, Qt::AlignCenter, ini.isEmpty() ? QStringLiteral("?") : ini);
        }
        p.restore();

        const qreal x = r.left() + kBadgePhoto + 11;
        const qreal tw = r.right() - 11 - x;
        qreal y = r.top() + 9;
        if (!e.role.isEmpty()) {
            QFont bf = ui(9, QFont::Bold);
            bf.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
            p.setFont(bf);
            const QString band = e.role.toUpper();
            const qreal bw = qMin(tw, QFontMetricsF(bf).horizontalAdvance(band) + 12);
            p.setPen(Qt::NoPen);
            p.setBrush(accent);
            p.drawRoundedRect(QRectF(x, y, bw, 15), 3, 3);
            p.setPen(Qt::white);
            p.drawText(QRectF(x + 6, y, bw - 12, 15), Qt::AlignVCenter | Qt::AlignLeft,
                       QFontMetricsF(bf).elidedText(band, Qt::ElideRight, bw - 12));
            y += 19;
        }
        p.setFont(serif(15, QFont::DemiBold));
        p.setPen(tc(Theme::textBright()));
        p.drawText(QRectF(x, y, tw, 20), Qt::AlignVCenter | Qt::AlignLeft,
                   QFontMetricsF(p.font()).elidedText(e.title, Qt::ElideRight, tw));
        y += 22;
        if (!e.facts.isEmpty()) {
            // Os dois primeiros campos curtos da ficha: RÓTULO valor.
            qreal fx = x;
            for (int k = 0; k < e.facts.size() && k < 2; ++k) {
                QFont lf = ui(9, QFont::DemiBold);
                lf.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
                const QString label = e.facts.at(k).first.toUpper();
                const qreal lw = QFontMetricsF(lf).horizontalAdvance(label);
                if (fx + lw + 20 > x + tw) break;
                p.setFont(lf);
                p.setPen(tc(Theme::textMuted()));
                p.drawText(QRectF(fx, y, lw + 2, 16), Qt::AlignVCenter | Qt::AlignLeft, label);
                fx += lw + 5;
                QFont vf = ui(11.5);
                p.setFont(vf);
                p.setPen(tc(Theme::textPrimary()));
                const qreal room = x + tw - fx;
                const QString val = QFontMetricsF(vf).elidedText(e.facts.at(k).second, Qt::ElideRight, k == 0 ? qMin(room, room * 0.45 + 30) : room);
                p.drawText(QRectF(fx, y, room, 16), Qt::AlignVCenter | Qt::AlignLeft, val);
                fx += QFontMetricsF(vf).horizontalAdvance(val) + 12;
            }
            y += 17;
        }
        if (bondsEnabled) {
            QList<DwBond> mine;
            for (const auto& b : m_bonds) if (b.from == e.id || b.to == e.id) mine << b;
            p.setFont(ui(10.5));
            qreal bx = x;
            if (mine.isEmpty()) {
                p.setPen(tc(Theme::textMuted()));
                p.drawText(QRectF(bx, y + 2, tw, 14), Qt::AlignVCenter | Qt::AlignLeft, tr("sem vínculos"));
            } else {
                for (int k = 0; k < mine.size() && k < 8; ++k) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(mine.at(k).color);
                    p.drawEllipse(QPointF(bx + 3.5, y + 9), 3.5, 3.5);
                    bx += 10;
                }
                p.setPen(tc(Theme::textMuted()));
                p.drawText(QRectF(bx + 4, y + 2, x + tw - bx - 4, 14), Qt::AlignVCenter | Qt::AlignLeft,
                           tr("%n vínculo(s)", "", mine.size()));
            }
            y += 18;
        }
        if (!e.appears.isEmpty())
            paintAppears(p, QRectF(x, qMax(y + 2, r.bottom() - 13), tw, 5), e.appears, accent);
        p.restore();
    }
    paintDragLine(p);
}

// ============================================================ Galeria de rostos

namespace { constexpr qreal kGalGap = 8, kGalSide = 10; }

DwGallery::DwGallery(QWidget* parent) : DwBondDragBase(parent) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void DwGallery::setData(const QList<DwEntry>& entries, const QList<DwBond>& bonds, const QColor& accent) {
    m_entries = entries;
    m_bonds = bonds;
    m_accent = accent;
    relayout();
    update();
}

void DwGallery::resizeEvent(QResizeEvent*) { relayout(); }

void DwGallery::relayout() {
    m_rects.clear();
    const qreal w = qMax(180, width());
    const int cols = qMax(2, int((w - 2 * kGalSide + kGalGap) / 108));
    const qreal cw = (w - 2 * kGalSide - (cols - 1) * kGalGap) / cols;
    const qreal ch = cw * 4.0 / 3.0;
    for (int i = 0; i < m_entries.size(); ++i)
        m_rects.append(QRectF(kGalSide + (i % cols) * (cw + kGalGap), 10 + (i / cols) * (ch + kGalGap), cw, ch));
    const int rows = (m_entries.size() + cols - 1) / cols;
    m_height = int(10 + rows * (ch + kGalGap) + 4);
    setFixedHeight(m_height);
}

int DwGallery::hitItem(const QPointF& p) const {
    for (int i = 0; i < m_rects.size(); ++i) if (m_rects.at(i).contains(p)) return i;
    return -1;
}

bool DwGallery::isDragHandle(int, const QPointF&) const { return true; }
QPointF DwGallery::dragAnchor(int item) const { return m_rects.value(item).center(); }
QRectF DwGallery::itemRect(int item) const { return m_rects.value(item); }
QString DwGallery::itemId(int item) const { return m_entries.value(item).id; }

void DwGallery::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal dpr = devicePixelRatioF();
    const QColor accent = m_accent.isValid() ? m_accent : tc(Theme::accentDefault());
    // Rosto sob o mouse: quem tem vínculo com ele fica aceso, o resto apaga.
    const int focus = (m_hover >= 0 && !m_dragging) ? m_hover : -1;
    const QString fid = focus >= 0 ? m_entries.at(focus).id : QString();
    for (int i = 0; i < m_entries.size(); ++i) {
        const DwEntry& e = m_entries.at(i);
        const QRectF r = m_rects.at(i);
        const DwBond* bond = nullptr;
        if (focus >= 0 && i != focus) {
            for (const auto& b : m_bonds)
                if ((b.from == fid && b.to == e.id) || (b.to == fid && b.from == e.id)) { bond = &b; break; }
        }
        const bool dim = focus >= 0 && i != focus && !bond && bondsEnabled;
        p.save();
        p.setOpacity(dim ? 0.28 : 1.0);
        applyIntro(p, i);
        QPainterPath tile;
        tile.addRoundedRect(r, 7, 7);
        p.setClipPath(tile);
        if (!e.photo.isNull()) {
            QPixmap sc = e.photo.scaled((r.size() * dpr).toSize(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            sc.setDevicePixelRatio(dpr);
            p.drawPixmap(QPointF(r.center().x() - sc.width() / dpr / 2, r.center().y() - sc.height() / dpr / 2), sc);
        } else {
            QRadialGradient g(QPointF(r.left() + r.width() * 0.35, r.top() + r.height() * 0.3), r.height());
            g.setColorAt(0, accent.lighter(115));
            g.setColorAt(0.85, accent.darker(260));
            p.fillRect(r, g);
            QString ini;
            for (const QString& w : e.title.split(QLatin1Char(' '), Qt::SkipEmptyParts)) { ini += w.left(1).toUpper(); if (ini.size() == 2) break; }
            p.setPen(QColor(255, 255, 255, 210));
            p.setFont(serif(22, QFont::DemiBold));
            p.drawText(r.adjusted(0, 0, 0, -22), Qt::AlignCenter, ini.isEmpty() ? QStringLiteral("?") : ini);
        }
        // Legenda no pé da foto.
        const QRectF capR(r.left(), r.bottom() - 46, r.width(), 46);
        QLinearGradient cg(capR.topLeft(), capR.bottomLeft());
        cg.setColorAt(0, QColor(0, 0, 0, 0));
        cg.setColorAt(1, QColor(0, 0, 0, 210));
        p.fillRect(capR, cg);
        const qreal tx = r.left() + 7, tw = r.width() - 14;
        qreal ty = r.bottom() - 7;
        if (!e.role.isEmpty()) {
            QFont rf = ui(8, QFont::Bold);
            rf.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
            p.setFont(rf);
            p.setPen(QColor(255, 255, 255, 180));
            p.drawText(QRectF(tx, ty - 11, tw, 11), Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetricsF(rf).elidedText(e.role.toUpper(), Qt::ElideRight, tw));
            ty -= 12;
        }
        p.setFont(serif(12.5, QFont::DemiBold));
        p.setPen(Qt::white);
        p.drawText(QRectF(tx, ty - 17, tw, 17), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetricsF(p.font()).elidedText(e.title, Qt::ElideRight, tw));
        if (!e.appears.isEmpty()) paintAppears(p, QRectF(tx, r.top() + 6, tw, 4), e.appears, accent);
        if (bond) {
            // Etiqueta do tipo do vínculo.
            QFont bf = ui(8.5, QFont::Bold);
            p.setFont(bf);
            const qreal bw = qMin(r.width() - 12, QFontMetricsF(bf).horizontalAdvance(bond->label) + 12);
            const QRectF pill(r.left() + 6, r.top() + (e.appears.isEmpty() ? 6 : 14), bw, 15);
            p.setPen(Qt::NoPen);
            p.setBrush(bond->color);
            p.drawRoundedRect(pill, 7.5, 7.5);
            p.setPen(QColor(20, 20, 20));
            p.drawText(pill.adjusted(6, 0, -6, 0), Qt::AlignCenter,
                       QFontMetricsF(bf).elidedText(bond->label, Qt::ElideRight, bw - 12));
        }
        p.restore();
        if (i == focus) {
            p.setPen(QPen(accent, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(-2.5, -2.5, 2.5, 2.5), 9, 9);
        }
    }
    paintDragLine(p);
}
