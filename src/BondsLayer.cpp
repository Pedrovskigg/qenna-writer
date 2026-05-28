#include "BondsLayer.h"

#include <QHashIterator>
#include <QLineF>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QtMath>

#include <algorithm>

namespace {
// Constantes do circuito — espelham o layout do Mira 1 ([[compaction-2026-04-26]]).
// kPhotoHalf removido: o valor real vem de BondCardPos::photoHalf (varia com o tamanho dos cards).
constexpr qreal kEdgeGap   = 2.0;
constexpr qreal kStrokeW   = 4.0;
constexpr qreal kLineGap   = 4.0;
constexpr qreal kBondSep   = kStrokeW + kLineGap; // 8 px entre lanes paralelas
constexpr qreal kDotRadius = 3.5;
constexpr qreal kMinMargin = 6.0;
constexpr qreal kLayoutPad = 4.0;

// Atalho pra retângulo da bounding-box de uma linha entre dois pontos.
QRectF segmentBounds(const QPointF& a, const QPointF& b, qreal halfWidth) {
    const qreal x1 = std::min(a.x(), b.x()) - halfWidth;
    const qreal y1 = std::min(a.y(), b.y()) - halfWidth;
    const qreal x2 = std::max(a.x(), b.x()) + halfWidth;
    const qreal y2 = std::max(a.y(), b.y()) + halfWidth;
    return QRectF(QPointF(x1, y1), QPointF(x2, y2));
}

qreal distancePointToSegment(const QPointF& p, const QLineF& seg) {
    const QPointF a = seg.p1();
    const QPointF b = seg.p2();
    const qreal lx = b.x() - a.x();
    const qreal ly = b.y() - a.y();
    const qreal len2 = lx * lx + ly * ly;
    if (len2 < 1e-6) return QLineF(p, a).length();
    qreal t = ((p.x() - a.x()) * lx + (p.y() - a.y()) * ly) / len2;
    t = std::clamp(t, 0.0, 1.0);
    const QPointF proj(a.x() + t * lx, a.y() + t * ly);
    return QLineF(p, proj).length();
}

} // namespace

BondsLayer::BondsLayer(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("bondsLayer"));
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
}

void BondsLayer::setBonds(const QList<CharacterBond>& bonds) {
    m_bonds = bonds;
    recompute();
    update();
}

void BondsLayer::setCardPositions(const QHash<QString, BondCardPos>& positions) {
    m_positions = positions;
    recompute();
    update();
}

void BondsLayer::setDrawerWidth(int w) {
    if (m_drawerWidth == w) return;
    m_drawerWidth = w;
    recompute();
    update();
}

void BondsLayer::setDragPreview(bool active, const QPointF& from, const QPointF& to) {
    m_dragActive = active;
    m_dragFrom = from;
    m_dragTo = to;
    update();
}

void BondsLayer::clearDragPreview() {
    m_dragActive = false;
    update();
}

void BondsLayer::setHoveredBond(const QString& bondId) {
    if (m_hoveredBondId == bondId) return;
    m_hoveredBondId = bondId;
    update();
}

QString BondsLayer::hitTestBond(const QPointF& pos, qreal tolerance) const {
    QString hitId;
    qreal best = tolerance;
    for (const auto& g : m_geometry) {
        for (const auto& seg : g.segments) {
            const qreal d = distancePointToSegment(pos, seg);
            if (d < best) {
                best = d;
                hitId = g.id;
            }
        }
    }
    return hitId;
}

void BondsLayer::recompute() {
    m_geometry.clear();
    m_bondById.clear();
    m_requiredSidePadding = 0;

    if (m_bonds.isEmpty() || m_positions.isEmpty()) return;

    // 1) Classifica bonds por roteamento:
    //    - cross: cards em colunas diferentes (cruza o corredor central)
    //    - sameLeft: ambos cards na coluna esquerda
    //    - sameRight: ambos cards na coluna direita
    const qreal midX = m_drawerWidth / 2.0;

    QList<CharacterBond> crossBonds;
    QList<CharacterBond> sameLeftBonds;
    QList<CharacterBond> sameRightBonds;

    for (const auto& b : m_bonds) {
        const auto p1It = m_positions.find(b.fromItemId);
        const auto p2It = m_positions.find(b.toItemId);
        if (p1It == m_positions.end() || p2It == m_positions.end()) continue;
        const BondCardPos& p1 = p1It.value();
        const BondCardPos& p2 = p2It.value();
        if (qAbs(p2.x - p1.x) < 15.0) {
            // Mesma coluna → routing pelo lado da parede mais próxima
            if (p1.x < midX) sameLeftBonds.append(b);
            else             sameRightBonds.append(b);
        } else {
            crossBonds.append(b);
        }
    }

    auto byAvgY = [this](const CharacterBond& a, const CharacterBond& b) {
        auto avg = [this](const CharacterBond& x) {
            const auto p1 = m_positions.value(x.fromItemId);
            const auto p2 = m_positions.value(x.toItemId);
            return (p1.ay + p2.ay) / 2.0;
        };
        return avg(a) < avg(b);
    };
    std::sort(crossBonds.begin(), crossBonds.end(), byAvgY);
    std::sort(sameLeftBonds.begin(), sameLeftBonds.end(), byAvgY);
    std::sort(sameRightBonds.begin(), sameRightBonds.end(), byAvgY);

    // 2) Calcula laneX por bond (corredores paralelos)
    qreal leftMaxR = 0, rightMinL = m_drawerWidth;
    qreal leftMinL = m_drawerWidth, rightMaxR = 0;
    bool sawLeft = false, sawRight = false;
    for (auto it = m_positions.cbegin(); it != m_positions.cend(); ++it) {
        const BondCardPos& p = it.value();
        if (p.x < midX) {
            sawLeft = true;
            leftMaxR = std::max(leftMaxR, p.right);
            leftMinL = std::min(leftMinL, p.left);
        } else {
            sawRight = true;
            rightMinL = std::min(rightMinL, p.left);
            rightMaxR = std::max(rightMaxR, p.right);
        }
    }
    if (!sawLeft)  { leftMaxR = 40; leftMinL = 8; }
    if (!sawRight) { rightMinL = m_drawerWidth - 40; rightMaxR = m_drawerWidth - 8; }

    QHash<QString, qreal> lanes;
    auto laneCenter = [&lanes](const QList<CharacterBond>& arr, qreal center) {
        const int n = arr.size();
        for (int i = 0; i < n; ++i) {
            const qreal offset = n > 1 ? (i - (n - 1) / 2.0) * kBondSep : 0.0;
            lanes.insert(arr.at(i).id, center + offset);
        }
    };
    laneCenter(crossBonds, (leftMaxR + rightMinL) / 2.0);
    laneCenter(sameLeftBonds, leftMinL / 2.0);
    laneCenter(sameRightBonds, (rightMaxR + m_drawerWidth) / 2.0);

    m_requiredSidePadding = int(std::max(
        sameLeftBonds.size() * kBondSep + kLayoutPad,
        sameRightBonds.size() * kBondSep + kLayoutPad
    ));
    if (m_requiredSidePadding < kMinMargin) m_requiredSidePadding = 0;

    // 3) Per-card endpoint fanout: múltiplos bonds num mesmo card são
    //    distribuídos verticalmente pra não sobrepor o dot.
    struct CardEntry { QString bondId; qreal otherY; };
    QHash<QString, QList<CardEntry>> cardBonds;
    for (const auto& b : m_bonds) {
        const auto p1 = m_positions.value(b.fromItemId);
        const auto p2 = m_positions.value(b.toItemId);
        cardBonds[b.fromItemId].append({ b.id, p2.ay });
        cardBonds[b.toItemId].append({ b.id, p1.ay });
    }
    QHash<QString, qreal> endpointOffset; // "bondId|cardId" → dy
    for (auto it = cardBonds.begin(); it != cardBonds.end(); ++it) {
        QList<CardEntry>& entries = it.value();
        if (entries.size() <= 1) {
            if (!entries.isEmpty()) {
                endpointOffset.insert(entries.first().bondId + QLatin1Char('|') + it.key(), 0);
            }
            continue;
        }
        std::sort(entries.begin(), entries.end(),
                  [](const CardEntry& a, const CardEntry& b) { return a.otherY < b.otherY; });
        const int n = entries.size();
        for (int i = 0; i < n; ++i) {
            const qreal off = (i - (n - 1) / 2.0) * kBondSep;
            endpointOffset.insert(entries.at(i).bondId + QLatin1Char('|') + it.key(), off);
        }
    }

    // 4) Constrói geometria por bond.
    m_geometry.reserve(m_bonds.size());
    for (const auto& b : m_bonds) {
        const auto p1It = m_positions.find(b.fromItemId);
        const auto p2It = m_positions.find(b.toItemId);
        if (p1It == m_positions.end() || p2It == m_positions.end()) continue;
        const BondCardPos& p1 = p1It.value();
        const BondCardPos& p2 = p2It.value();
        const qreal ey1 = p1.ay + endpointOffset.value(b.id + QLatin1Char('|') + b.fromItemId, 0);
        const qreal ey2 = p2.ay + endpointOffset.value(b.id + QLatin1Char('|') + b.toItemId, 0);
        const qreal laneX = lanes.value(b.id, (p1.x + p2.x) / 2.0);
        const qreal dx = p2.x - p1.x;
        const qreal dy = p2.y - p1.y;

        BondGeometry g;
        g.id = b.id;
        g.color = b.color.isEmpty() ? QStringLiteral("#4a9eff") : b.color;
        g.type = b.type;

        qreal ep1x = 0, ep2x = 0;
        if (qAbs(dx) < 15.0) {
            const bool toLeft = laneX < p1.x;
            ep1x = toLeft ? p1.x - p1.photoHalf - kEdgeGap : p1.x + p1.photoHalf + kEdgeGap;
            ep2x = toLeft ? p2.x - p2.photoHalf - kEdgeGap : p2.x + p2.photoHalf + kEdgeGap;
            g.segments << QLineF(ep1x, ey1, laneX, ey1)
                       << QLineF(laneX, ey1, laneX, ey2)
                       << QLineF(laneX, ey2, ep2x, ey2);
            g.labelPos = QPointF(laneX, (ey1 + ey2) / 2.0 - 8);
        } else {
            const bool goRight = dx > 0;
            ep1x = goRight ? p1.x + p1.photoHalf + kEdgeGap : p1.x - p1.photoHalf - kEdgeGap;
            ep2x = goRight ? p2.x - p2.photoHalf - kEdgeGap : p2.x + p2.photoHalf + kEdgeGap;
            if (qAbs(dy) < 15.0) {
                g.segments << QLineF(ep1x, ey1, ep2x, ey2);
                g.labelPos = QPointF((ep1x + ep2x) / 2.0, ey1 - 8);
            } else {
                g.segments << QLineF(ep1x, ey1, laneX, ey1)
                           << QLineF(laneX, ey1, laneX, ey2)
                           << QLineF(laneX, ey2, ep2x, ey2);
                g.labelPos = QPointF(laneX, (ey1 + ey2) / 2.0 - 8);
            }
        }

        g.path.moveTo(g.segments.first().p1());
        for (const auto& seg : g.segments) g.path.lineTo(seg.p2());

        g.endpoint1 = QPointF(ep1x, ey1);
        g.endpoint2 = QPointF(ep2x, ey2);

        m_geometry.append(g);
    }
    for (auto& g : m_geometry) {
        m_bondById.insert(g.id, &g);
    }
}

void BondsLayer::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false); // linhas ortogonais ficam crisp sem AA
    p.setRenderHint(QPainter::TextAntialiasing, true);

    for (const auto& g : m_geometry) {
        const bool isHover = (g.id == m_hoveredBondId);
        QColor color(g.color);
        QPen pen(color);
        pen.setWidthF(isHover ? kStrokeW + 1 : kStrokeW);
        pen.setCapStyle(Qt::SquareCap);
        pen.setJoinStyle(Qt::MiterJoin);
        color.setAlphaF(isHover ? 1.0 : 0.85);
        pen.setColor(color);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(g.path);

        // Dots nos endpoints
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(g.endpoint1, kDotRadius, kDotRadius);
        p.drawEllipse(g.endpoint2, kDotRadius, kDotRadius);
    }

    // Label do bond hovered, com sombra (fallback ao filtro SVG do Mira 1).
    if (!m_hoveredBondId.isEmpty()) {
        for (const auto& g : m_geometry) {
            if (g.id != m_hoveredBondId) continue;
            if (g.type.isEmpty()) break;
            QFont f = font();
            f.setPointSizeF(8.5);
            f.setBold(true);
            p.setFont(f);

            const QFontMetrics fm(f);
            const QRect textRect = fm.boundingRect(g.type);
            const QPointF center = g.labelPos;
            const QRectF bg(center.x() - textRect.width() / 2.0 - 5,
                            center.y() - textRect.height() + 1,
                            textRect.width() + 10,
                            textRect.height() + 2);
            QPainterPath bgPath;
            bgPath.addRoundedRect(bg, 4, 4);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 220));
            p.drawPath(bgPath);

            p.setPen(QColor(255, 255, 255));
            p.drawText(QRectF(bg.x() + 5, bg.y(), textRect.width(), bg.height()),
                       Qt::AlignVCenter | Qt::AlignLeft, g.type);
            break;
        }
    }

    // Preview da linha de drag (criar bond).
    if (m_dragActive) {
        QPen previewPen(QColor(255, 255, 255, 120));
        previewPen.setWidthF(1.5);
        previewPen.setStyle(Qt::DashLine);
        p.setPen(previewPen);
        p.drawLine(m_dragFrom, m_dragTo);
    }
}
