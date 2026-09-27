#include "ThemesPanel.h"

#include "Theme.h"
#include "ThemeEditorDialog.h"
#include "ThemeExportDialog.h"
#include "ThemePackage.h"
#include "ThemeScene.h"
#include "IconUtils.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTimeEdit>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QtMath>
#include <climits>
#include <algorithm>
#include <functional>

using ThemeScene::ImageCache;

namespace {

constexpr int kRailW = 168;
constexpr int kSideW = 560;
constexpr int kSidePadX = 18;
constexpr int kPreviewW = kSideW - kSidePadX * 2;   // 524
const QColor kFav(0xe0, 0x70, 0x7a);

QColor c(const QString& css) { return Theme::toColor(css); }

QFont px(int size, int weight = QFont::Normal)
{
    QFont f = QApplication::font();
    f.setPixelSize(size);
    f.setWeight(QFont::Weight(weight));
    return f;
}

// Assinatura do conteúdo do tema: um tema personalizado editado mantém o id,
// então o cache da prévia não pode ser só por id.
QString fingerprint(const Theme::MiraTheme& t)
{
    return QString::number(qHash(QJsonDocument(Theme::themeToJson(t)).toJson(QJsonDocument::Compact)), 16);
}

QString levelWord(double v)
{
    // Contraste descrito, nunca julgado: tema com apagado discreto não é tema ruim.
    if (v >= 7.0) return QCoreApplication::translate("ThemesPanel", "alto");
    if (v >= 4.5) return QCoreApplication::translate("ThemesPanel", "médio");
    if (v >= 3.0) return QCoreApplication::translate("ThemesPanel", "suave");
    return QCoreApplication::translate("ThemesPanel", "bem suave");
}

QString num1(double v) { return QLocale().toString(v, 'f', 1); }

// Recomendações do desenvolvedor (escolhidas a dedo, 2026-09-27): sobem pro
// topo da categoria delas, nessa ordem, e ganham o selo na prévia.
const QHash<QString, QStringList>& recommended()
{
    static const QHash<QString, QStringList> r = {
        { QStringLiteral("dark"),       { QStringLiteral("tokyo-night"), QStringLiteral("grace-dark"), QStringLiteral("everforest"),
                                          QStringLiteral("one-dark"), QStringLiteral("basalt") } },
        { QStringLiteral("light"),      { QStringLiteral("solarized-light"), QStringLiteral("manuscrito"), QStringLiteral("tokyo-day"),
                                          QStringLiteral("e-ink-gray"), QStringLiteral("brutalismo") } },
        { QStringLiteral("estampados"), { QStringLiteral("paper-city-yellowed"), QStringLiteral("tokyo-noir"), QStringLiteral("one-dark-skyline"),
                                          QStringLiteral("solarized-highlands"), QStringLiteral("arquivo"),
                                          QStringLiteral("desk-mahogany") } },
        { QStringLiteral("warm"),       { QStringLiteral("gruvbox-dark"), QStringLiteral("cafe"), QStringLiteral("lamparina"),
                                          QStringLiteral("fig-preserve"), QStringLiteral("acafrao") } },
        { QStringLiteral("colorful"),   { QStringLiteral("blacklight"), QStringLiteral("rose-pine"), QStringLiteral("outrun"),
                                          QStringLiteral("ocean"), QStringLiteral("solarized-dark") } },
    };
    return r;
}
// Das recomendações acima, as que vieram do Tony (o tester mais antigo) e não
// do desenvolvedor: mesmo lugar na fila, selo com o nome dele.
bool isTonyPick(const QString& id)
{
    return id == QLatin1String("desk-mahogany");
}
bool isRecommended(const QString& id)
{
    for (const auto& ids : recommended()) if (ids.contains(id)) return true;
    return false;
}

} // namespace

namespace ThemesPanelDetail {

// ------------------------------------------------------------------ trilho

class RailItem : public QAbstractButton {
public:
    enum Glyph { None, Heart, User, Clock };
    RailItem(const QString& text, Glyph g, QWidget* parent)
        : QAbstractButton(parent), m_glyph(g)
    {
        setText(text);
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
        setFixedHeight(30);
    }
    void setCount(int n) { m_count = n; update(); }
    QSize sizeHint() const override { return QSize(kRailW, 30); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = QRectF(rect()).adjusted(8, 0, -8, 0);
        if (isChecked()) {
            p.setPen(Qt::NoPen);
            p.setBrush(c(Theme::accentInfoSoft()));
            p.drawRoundedRect(r, 6, 6);
            p.setBrush(c(Theme::accentDefault()));
            p.drawRoundedRect(QRectF(0, 7, 3, height() - 14), 1.5, 1.5);
        } else if (underMouse()) {
            p.setPen(Qt::NoPen);
            p.setBrush(c(Theme::hoverOverlay()));
            p.drawRoundedRect(r, 6, 6);
        }
        const bool strong = isChecked() || underMouse();
        const QColor ink = strong ? c(Theme::textBright())
                         : (m_glyph == Clock ? c(Theme::textMuted()) : c(Theme::textPrimary()));
        qreal x = r.left() + 10;
        const qreal cy = height() / 2.0;
        if (m_glyph == Heart) {
            p.setPen(kFav);
            p.setFont(px(12));
            p.drawText(QRectF(x, 0, 14, height()), Qt::AlignCenter, QStringLiteral("♥"));
            x += 14 + 7;
        } else if (m_glyph == User || m_glyph == Clock) {
            p.setPen(QPen(ink, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            if (m_glyph == User) {
                p.drawEllipse(QPointF(x + 7, cy - 2.5), 2.8, 2.8);
                QPainterPath arc;
                arc.moveTo(x + 2, cy + 6);
                arc.cubicTo(x + 2.5, cy + 1.5, x + 11.5, cy + 1.5, x + 12, cy + 6);
                p.drawPath(arc);
            } else {
                p.drawEllipse(QPointF(x + 7, cy), 6, 6);
                p.drawLine(QPointF(x + 7, cy - 3.2), QPointF(x + 7, cy));
                p.drawLine(QPointF(x + 7, cy), QPointF(x + 9.2, cy + 1.6));
            }
            x += 14 + 9;
        }
        p.setPen(ink);
        p.setFont(px(13));
        p.drawText(QRectF(x, 0, r.right() - x - 30, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
        if (m_count >= 0) {
            p.setPen(c(Theme::textMuted()));
            p.setFont(px(11));
            p.drawText(QRectF(r.right() - 40, 0, 30, height()), Qt::AlignVCenter | Qt::AlignRight, QString::number(m_count));
        }
    }

private:
    Glyph m_glyph;
    int m_count = -1;
};

// ------------------------------------------------------------------ grade

// Uma widget só pinta a grade inteira (só as linhas visíveis). Com 300+ temas,
// um widget por miniatura custava segundos pra abrir o painel.
class ThemeGridView : public QWidget {
public:
    std::function<void(const QString&)> onHover, onClick, onDoubleClick, onFavorite;
    std::function<void()> onNew;

    explicit ThemeGridView(QWidget* parent) : QWidget(parent)
    {
        setMouseTracking(true);
        connect(ImageCache::instance(), &ImageCache::ready, this, [this](const QString&, int cap) {
            if (cap == ImageCache::kThumbCap) update();
        });
    }

    void setThemes(const QList<Theme::MiraTheme>& themes, bool newTile, const QString& emptyText)
    {
        m_themes = themes;
        m_newTile = newTile;
        m_empty = emptyText;
        m_hover = -1;
        relayout();
        update();
    }
    void setSelected(const QString& id) { m_selected = id; update(); }
    bool contains(const QString& id) const
    {
        for (const auto& t : m_themes) if (t.id == id) return true;
        return false;
    }
    QRect itemRect(const QString& id) const
    {
        for (int i = 0; i < m_themes.size(); ++i)
            if (m_themes.at(i).id == id) return cell(i + (m_newTile ? 1 : 0)).toAlignedRect();
        return {};
    }

protected:
    void resizeEvent(QResizeEvent* e) override
    {
        QWidget::resizeEvent(e);
        relayout();
    }

    void paintEvent(QPaintEvent* e) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.fillRect(e->rect(), c(Theme::panelBackground()));
        const int n = count();
        if (n == 0) {
            p.setPen(c(Theme::textMuted()));
            p.setFont(px(12));
            p.drawText(QRectF(20, 30, width() - 40, 80), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, m_empty);
            return;
        }
        const auto* mgr = Theme::Manager::instance();
        const QString currentId = mgr->current().id;
        const auto& cfg = mgr->autoSwitchConfig();
        const QColor accent = c(Theme::accentDefault());
        for (int i = 0; i < n; ++i) {
            const QRectF cellR = cell(i);
            if (!cellR.toAlignedRect().intersects(e->rect())) continue;
            const bool hover = (i == m_hover);
            QRectF th = thumbRect(i);
            if (m_newTile && i == 0) {
                p.setPen(QPen(c(Theme::panelBorder()), 1.2, Qt::DashLine));
                p.setBrush(hover ? c(Theme::hoverOverlay()) : Qt::transparent);
                p.drawRoundedRect(th.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
                p.setPen(QPen(hover ? c(Theme::textBright()) : c(Theme::textMuted()), 1.6, Qt::SolidLine, Qt::RoundCap));
                const QPointF m = th.center();
                p.drawLine(m + QPointF(-8, 0), m + QPointF(8, 0));
                p.drawLine(m + QPointF(0, -8), m + QPointF(0, 8));
                p.setPen(hover ? c(Theme::textBright()) : c(Theme::textMuted()));
                p.setFont(px(11));
                p.drawText(nameRect(i), Qt::AlignLeft | Qt::AlignVCenter,
                           QCoreApplication::translate("ThemesPanel", "Novo tema"));
                continue;
            }
            const Theme::MiraTheme& t = m_themes.at(i - (m_newTile ? 1 : 0));
            const bool sel = (t.id == m_selected);
            if (hover) th.translate(0, -2);
            // Anel/sombra por estado
            p.setBrush(Qt::NoBrush);
            if (hover) {
                for (int k = 1; k <= 5; ++k) {
                    p.setPen(QPen(QColor(0, 0, 0, 16), 1));
                    p.drawRoundedRect(th.adjusted(-k * 0.4, k * 0.6, k * 0.4, k * 1.2), 6 + k, 6 + k);
                }
            }
            const QPixmap bg = t.backgroundImage.isEmpty() ? QPixmap()
                             : ImageCache::instance()->get(t.backgroundImage, ImageCache::kThumbCap);
            ThemeScene::paintThumb(p, th, t, bg, 6);
            if (sel) {
                p.setPen(QPen(accent, 2));
                p.drawRoundedRect(th.adjusted(-1, -1, 1, 1), 7, 7);
            } else {
                p.setPen(QPen(QColor(255, 255, 255, hover ? 41 : 15), 1));
                p.drawRoundedRect(th.adjusted(-0.5, -0.5, 0.5, 0.5), 6.5, 6.5);
            }
            // Coração: só no hover e nos favoritos
            const bool fav = mgr->isFavorite(t.id);
            if (hover || fav) {
                const QRectF hr = heartRect(th);
                if (!fav) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(0, 0, 0, 115));
                    p.drawEllipse(hr);
                    p.setPen(Qt::white);
                    p.setFont(px(10));
                    p.drawText(hr, Qt::AlignCenter, QStringLiteral("♥"));
                } else {
                    p.setFont(px(12));
                    p.setPen(QColor(0, 0, 0, 170));
                    p.drawText(hr.translated(0, 1), Qt::AlignCenter, QStringLiteral("♥"));
                    p.setPen(kFav);
                    p.drawText(hr, Qt::AlignCenter, QStringLiteral("♥"));
                }
            }
            // Nome (com EM USO e os papéis de dia/noite)
            QRectF nr = nameRect(i);
            if (t.id == currentId) {
                QFont uf = px(9, QFont::Bold);
                uf.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
                const QString u = QCoreApplication::translate("ThemesPanel", "EM USO");
                p.setFont(uf);
                p.setPen(accent);
                p.drawText(nr, Qt::AlignLeft | Qt::AlignVCenter, u);
                nr.setLeft(nr.left() + QFontMetricsF(uf).horizontalAdvance(u) + 5);
            }
            QString name = t.name;
            if (!cfg.dayThemeId.isEmpty() && cfg.dayThemeId == t.id) name += QStringLiteral(" ☀");
            if (!cfg.nightThemeId.isEmpty() && cfg.nightThemeId == t.id) name += QStringLiteral(" ☽");
            const QFont nf = px(11, sel ? QFont::DemiBold : QFont::Normal);
            p.setFont(nf);
            p.setPen((hover || sel) ? c(Theme::textBright()) : c(Theme::textMuted()));
            p.drawText(nr, Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetricsF(nf).elidedText(name, Qt::ElideRight, nr.width()));
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        const int h = hit(e->position());
        setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        if (h == m_hover) return;
        const int old = m_hover;
        m_hover = h;
        if (old >= 0) update(cell(old).adjusted(-4, -6, 4, 6).toAlignedRect());
        if (h >= 0) update(cell(h).adjusted(-4, -6, 4, 6).toAlignedRect());
        if (onHover) onHover(idAt(h));
    }

    void leaveEvent(QEvent* e) override
    {
        QWidget::leaveEvent(e);
        if (m_hover >= 0) {
            const int old = m_hover;
            m_hover = -1;
            update(cell(old).adjusted(-4, -6, 4, 6).toAlignedRect());
        }
        if (onHover) onHover(QString());
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        const int h = hit(e->position());
        if (h < 0) return;
        if (m_newTile && h == 0) { if (onNew) onNew(); return; }
        const QString id = idAt(h);
        QRectF th = thumbRect(h);
        if (h == m_hover) th.translate(0, -2);
        if (heartRect(th).adjusted(-3, -3, 3, 3).contains(e->position())) {
            if (onFavorite) onFavorite(id);
            update();
            return;
        }
        if (onClick) onClick(id);
    }

    void mouseDoubleClickEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        const int h = hit(e->position());
        if (h < 0 || (m_newTile && h == 0)) return;
        QRectF th = thumbRect(h);
        if (heartRect(th.translated(0, -2)).adjusted(-3, -3, 3, 3).contains(e->position())) return;
        if (onDoubleClick) onDoubleClick(idAt(h));
    }

private:
    static constexpr qreal kPad = 14, kGapX = 12, kGapY = 16, kNameGap = 6, kNameH = 16;

    int count() const { return m_themes.size() + (m_newTile ? 1 : 0); }
    int cols() const { return qMax(3, int((width() - kPad * 2 + kGapX) / (150 + kGapX))); }
    qreal cardW() const { const int n = cols(); return (width() - kPad * 2 - (n - 1) * kGapX) / n; }
    qreal thumbH() const { return cardW() * 86.0 / 140.0; }
    qreal rowH() const { return thumbH() + kNameGap + kNameH; }

    QRectF cell(int i) const
    {
        const int n = cols();
        const qreal w = cardW();
        return QRectF(kPad + (i % n) * (w + kGapX), kPad + (i / n) * (rowH() + kGapY), w, rowH());
    }
    QRectF thumbRect(int i) const { const QRectF r = cell(i); return QRectF(r.left(), r.top(), r.width(), thumbH()); }
    QRectF nameRect(int i) const { const QRectF r = cell(i); return QRectF(r.left(), r.top() + thumbH() + kNameGap, r.width(), kNameH); }
    static QRectF heartRect(const QRectF& th) { return QRectF(th.right() - 5 - 18, th.top() + 5, 18, 18); }

    int hit(const QPointF& pos) const
    {
        const int n = count();
        for (int i = 0; i < n; ++i)
            if (cell(i).adjusted(0, -2, 0, 0).contains(pos)) return i;
        return -1;
    }
    QString idAt(int i) const
    {
        if (i < 0) return {};
        if (m_newTile) { if (i == 0) return {}; --i; }
        return i < m_themes.size() ? m_themes.at(i).id : QString();
    }

    void relayout()
    {
        const int n = count();
        const int rows = n == 0 ? 1 : (n + cols() - 1) / cols();
        const int h = int(kPad * 2 + rows * rowH() + (rows - 1) * kGapY) + 4;
        if (minimumHeight() != h) setMinimumHeight(h);
    }

    QList<Theme::MiraTheme> m_themes;
    bool m_newTile = false;
    QString m_empty;
    QString m_selected;
    int m_hover = -1;
};

// ------------------------------------------------------------------ prévia

// A janela do Qenna dividida: à esquerda o tema em uso, à direita o escolhido,
// separados por uma linha fina que se arrasta de qualquer ponto da prévia.
class ComparePreview : public QWidget {
public:
    std::function<void(qreal)> onCut;
    std::function<void()> onZoom;

    ComparePreview(bool zoomButton, QWidget* parent) : QWidget(parent), m_zoomBtn(zoomButton)
    {
        setMouseTracking(true);
        setCursor(Qt::SizeHorCursor);
        connect(ImageCache::instance(), &ImageCache::ready, this, [this](const QString& path, int) {
            if (path == m_left.backgroundImage || path == m_right.backgroundImage) update();
        });
    }

    void setThemes(const Theme::MiraTheme& left, const Theme::MiraTheme& right)
    {
        m_left = left;
        m_right = right;
        m_leftFp = fingerprint(left);
        m_rightFp = fingerprint(right);
        update();
    }
    void setScene(int s) { m_scene = s; update(); }
    void setCut(qreal cut) { m_cut = qBound(0.0, cut, 1.0); update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = rect();
        QPainterPath clip;
        clip.addRoundedRect(r, 6, 6);
        p.setClipPath(clip);
        p.drawPixmap(0, 0, render(m_left, m_leftFp));
        const qreal x = qRound(r.width() * m_cut);
        p.save();
        p.setClipRect(QRectF(x, 0, r.width() - x, r.height()), Qt::IntersectClip);
        p.drawPixmap(0, 0, render(m_right, m_rightFp));
        p.restore();

        // Rótulos
        QFont lf = px(9, QFont::Bold);
        lf.setLetterSpacing(QFont::AbsoluteSpacing, 1);
        p.setFont(lf);
        const QFontMetricsF fm(lf);
        auto label = [&](const QString& text, bool right, bool dim) {
            const qreal w = fm.horizontalAdvance(text) + 12;
            const QRectF lr(right ? r.right() - 7 - w : 7, 7, w, 16);
            p.setOpacity(dim ? 0.35 : 1.0);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 128));
            p.drawRoundedRect(lr, 4, 4);
            p.setPen(Qt::white);
            p.drawText(lr, Qt::AlignCenter, text);
            p.setOpacity(1);
        };
        label(QCoreApplication::translate("ThemesPanel", "EM USO"), false, m_cut > 0.85);
        label(m_right.name.toUpper(), true, m_cut < 0.15);

        // Linha de 1px com a alcinha
        p.fillRect(QRectF(x - 1.5, 0, 4, r.height()), QColor(0, 0, 0, 40));
        p.fillRect(QRectF(x - 0.5, 0, 1, r.height()), QColor(255, 255, 255, 217));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 90));
        p.drawRoundedRect(QRectF(x - 3, r.center().y() - 12, 6, 26), 3, 3);
        p.setBrush(Qt::white);
        p.drawRoundedRect(QRectF(x - 3, r.center().y() - 13, 6, 26), 3, 3);

        if (m_zoomBtn) {
            const QRectF zb = zoomRect();
            p.setOpacity(m_overZoom ? 1.0 : 0.75);
            p.setBrush(QColor(0, 0, 0, 140));
            p.drawRoundedRect(zb, 5, 5);
            p.setPen(QPen(Qt::white, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            const QPointF a = zb.center() + QPointF(-4.5, 4.5), b = zb.center() + QPointF(4.5, -4.5);
            p.drawLine(a, b);
            p.drawLine(b, b + QPointF(-4, 0));
            p.drawLine(b, b + QPointF(0, 4));
            p.drawLine(a, a + QPointF(4, 0));
            p.drawLine(a, a + QPointF(0, -4));
            p.setOpacity(1);
        }
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        if (m_zoomBtn && zoomRect().contains(e->position())) {
            if (onZoom) onZoom();
            return;
        }
        m_drag = true;
        moveCut(e->position().x());
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        const bool over = m_zoomBtn && zoomRect().contains(e->position());
        if (over != m_overZoom) { m_overZoom = over; update(zoomRect().toAlignedRect()); }
        setCursor(over ? Qt::PointingHandCursor : Qt::SizeHorCursor);
        if (m_drag) moveCut(e->position().x());
    }
    void mouseReleaseEvent(QMouseEvent*) override { m_drag = false; }
    void leaveEvent(QEvent*) override { if (m_overZoom) { m_overZoom = false; update(); } }

private:
    QRectF zoomRect() const { return QRectF(width() - 7 - 24, height() - 7 - 24, 24, 24); }

    void moveCut(qreal x)
    {
        setCut(x / qMax(1, width()));
        if (onCut) onCut(m_cut);
    }

    QPixmap render(const Theme::MiraTheme& t, const QString& fp)
    {
        const qreal dpr = devicePixelRatioF();
        QPixmap bg;
        if (m_scene != ThemeScene::Editor && !t.backgroundImage.isEmpty()) {
            bg = ImageCache::instance()->get(t.backgroundImage, ImageCache::kSceneCap);
            // Enquanto a foto grande carrega, a pequena da grade segura a prévia.
            if (bg.isNull()) bg = ImageCache::instance()->get(t.backgroundImage, ImageCache::kThumbCap);
        }
        const QString key = QStringLiteral("%1|%2|%3x%4@%5|%6").arg(fp).arg(m_scene).arg(width()).arg(height())
                                .arg(dpr).arg(bg.cacheKey());
        const auto it = m_cache.constFind(key);
        if (it != m_cache.constEnd()) return it.value();
        QPixmap pm(QSize(qCeil(width() * dpr), qCeil(height() * dpr)));
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        {
            QPainter p(&pm);
            ThemeScene::paintScene(p, QRectF(0, 0, width(), height()), t, ThemeScene::Scene(m_scene), bg);
        }
        if (m_cache.size() >= 16) { m_cache.remove(m_order.takeFirst()); }
        m_cache.insert(key, pm);
        m_order.append(key);
        return pm;
    }

    Theme::MiraTheme m_left, m_right;
    QString m_leftFp, m_rightFp;
    int m_scene = ThemeScene::Writing;
    qreal m_cut = 0.5;
    bool m_zoomBtn;
    bool m_drag = false;
    bool m_overZoom = false;
    QHash<QString, QPixmap> m_cache;
    QStringList m_order;
};

// Bolinha com a cor do texto do tema, com anel pra não sumir no fundo do painel.
class Dot : public QWidget {
public:
    explicit Dot(QWidget* parent) : QWidget(parent) { setFixedSize(9, 9); }
    void setColor(const QColor& col) { m_color = col; update(); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(c(Theme::panelBorder()), 1));
        p.setBrush(m_color);
        p.drawEllipse(QRectF(1, 1, 7, 7));
    }
private:
    QColor m_color;
};

// ------------------------------------------------------------------ parecidos

class SimilarStrip : public QWidget {
public:
    std::function<void(const QString&)> onPick;

    explicit SimilarStrip(QWidget* parent) : QWidget(parent)
    {
        setMouseTracking(true);
        setFixedHeight(62 + 5 + 16 + 2);
        connect(ImageCache::instance(), &ImageCache::ready, this, [this](const QString&, int cap) {
            if (cap == ImageCache::kThumbCap) update();
        });
    }
    void setThemes(const QList<Theme::MiraTheme>& t) { m_themes = t; m_hover = -1; update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        for (int i = 0; i < m_themes.size(); ++i) {
            const auto& t = m_themes.at(i);
            QRectF th = thumb(i);
            const bool hover = (i == m_hover);
            if (hover) th.translate(0, -2);
            const QPixmap bg = t.backgroundImage.isEmpty() ? QPixmap()
                             : ImageCache::instance()->get(t.backgroundImage, ImageCache::kThumbCap);
            ThemeScene::paintThumb(p, th, t, bg, 5);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(255, 255, 255, hover ? 41 : 15), 1));
            p.drawRoundedRect(th.adjusted(-0.5, -0.5, 0.5, 0.5), 5.5, 5.5);
            const QFont f = px(11);
            p.setFont(f);
            p.setPen(hover ? c(Theme::textBright()) : c(Theme::textMuted()));
            const QRectF nr(thumb(i).left(), 62 + 5 + 2, thumb(i).width(), 16);
            p.drawText(nr, Qt::AlignLeft | Qt::AlignVCenter, QFontMetricsF(f).elidedText(t.name, Qt::ElideRight, nr.width()));
        }
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        const int h = hit(e->position());
        setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        if (h != m_hover) { m_hover = h; update(); }
    }
    void leaveEvent(QEvent*) override { if (m_hover >= 0) { m_hover = -1; update(); } }
    void mousePressEvent(QMouseEvent* e) override
    {
        const int h = hit(e->position());
        if (e->button() == Qt::LeftButton && h >= 0 && onPick) onPick(m_themes.at(h).id);
    }

private:
    QRectF thumb(int i) const
    {
        const qreal w = (width() - 3 * 10) / 4.0;
        return QRectF(i * (w + 10), 2, w, 62);
    }
    int hit(const QPointF& pos) const
    {
        for (int i = 0; i < m_themes.size(); ++i)
            if (thumb(i).adjusted(0, -2, 0, 5 + 16).contains(pos)) return i;
        return -1;
    }
    QList<Theme::MiraTheme> m_themes;
    int m_hover = -1;
};

// Recado dos temas com lore (Tifu e Tommy) na lateral: clicável, abre a foto.
class LoreCard : public QLabel {
public:
    explicit LoreCard(QWidget* parent) : QLabel(parent)
    {
        setObjectName(QStringLiteral("loreCard"));
        setWordWrap(true);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
    }
    std::function<void()> onClick;

protected:
    void mousePressEvent(QMouseEvent* e) override { e->accept(); }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && onClick) onClick();
    }
};

// A foto do gato por cima do painel: fundo escurecido, cartão com a foto e o
// nome, entra com fade subindo um pouco. Clique em qualquer lugar ou Esc fecha.
// Filho do próprio painel (não janela nova) — cobre o painel inteiro e
// acompanha o redimensionamento.
class CatPhotoOverlay : public QWidget {
public:
    CatPhotoOverlay(QWidget* host, const QString& photo, const QString& caption, const QColor& accent)
        : QWidget(host), m_photo(photo), m_caption(caption), m_accent(accent)
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        setGeometry(host->rect());
        host->installEventFilter(this);

        m_fade = new QGraphicsOpacityEffect(this);
        m_fade->setOpacity(0.0);
        setGraphicsEffect(m_fade);
        show();
        raise();
        setFocus();

        auto* in = new QVariantAnimation(this);
        in->setDuration(280);
        in->setStartValue(0.0);
        in->setEndValue(1.0);
        in->setEasingCurve(QEasingCurve::OutCubic);
        connect(in, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            m_t = v.toReal();
            m_fade->setOpacity(m_t);
            update();
        });
        in->start(QAbstractAnimation::DeleteWhenStopped);
    }

    void dismiss()
    {
        if (m_closing) return;
        m_closing = true;
        auto* out = new QVariantAnimation(this);
        out->setDuration(200);
        out->setStartValue(m_t);
        out->setEndValue(0.0);
        out->setEasingCurve(QEasingCurve::InCubic);
        connect(out, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            m_t = v.toReal();
            m_fade->setOpacity(m_t);
            update();
        });
        connect(out, &QVariantAnimation::finished, this, [this]() { hide(); close(); });
        out->start(QAbstractAnimation::DeleteWhenStopped);
    }

protected:
    bool eventFilter(QObject* obj, QEvent* e) override
    {
        if (obj == parentWidget() && e->type() == QEvent::Resize) setGeometry(parentWidget()->rect());
        return QWidget::eventFilter(obj, e);
    }
    void mousePressEvent(QMouseEvent* e) override
    {
        e->accept();
        dismiss();
    }
    void keyPressEvent(QKeyEvent* e) override
    {
        e->accept();
        if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter
            || e->key() == Qt::Key_Space)
            dismiss();
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.fillRect(rect(), QColor(0, 0, 0, 165));
        if (m_photo.isNull()) return;

        const qreal pad = 10, capH = 46;
        const qreal ph = qMin(height() * 0.74, 640.0);
        const qreal pw = ph * m_photo.width() / m_photo.height();
        const qreal rise = (1.0 - m_t) * 14;
        const QRectF card((width() - pw - 2 * pad) / 2, (height() - ph - 2 * pad - capH) / 2 + rise,
                          pw + 2 * pad, ph + 2 * pad + capH);

        p.setPen(QPen(m_accent, 1.5));
        p.setBrush(Theme::toColor(Theme::panelBackground()));
        p.drawRoundedRect(card, 14, 14);

        const QSize target = (QSizeF(pw, ph) * devicePixelRatioF()).toSize();
        if (m_scaled.size() != target) {
            m_scaled = m_photo.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_scaled.setDevicePixelRatio(devicePixelRatioF());
        }
        const QRectF photoR(card.left() + pad, card.top() + pad, pw, ph);
        QPainterPath clip;
        clip.addRoundedRect(photoR, 9, 9);
        p.save();
        p.setClipPath(clip);
        p.drawPixmap(photoR.topLeft(), m_scaled);
        p.restore();

        QFont f(QStringLiteral("Lora"));
        f.setPixelSize(18);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(Theme::toColor(Theme::textBright()));
        p.drawText(QRectF(card.left(), photoR.bottom(), card.width(), capH + pad),
                   Qt::AlignCenter, m_caption);
    }

private:
    QPixmap m_photo;
    QPixmap m_scaled;
    QString m_caption;
    QColor m_accent;
    QGraphicsOpacityEffect* m_fade = nullptr;
    qreal m_t = 0.0;
    bool m_closing = false;
};

} // namespace ThemesPanelDetail

using namespace ThemesPanelDetail;

// ======================================================================

ThemesPanel::ThemesPanel(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("themesPanel"));
    setWindowTitle(tr("Temas"));
    setModal(false);
    setMinimumSize(1040, 640);
    QSize want(1200, 720);
    if (const QScreen* s = (parent ? parent->screen() : QGuiApplication::primaryScreen())) {
        const QSize avail = s->availableGeometry().size() - QSize(40, 60);
        want = want.boundedTo(avail).expandedTo(minimumSize());
    }
    resize(want);

    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    // Respiro ao sair de um cartão: passar de um pro outro não pisca a prévia.
    connect(m_hoverTimer, &QTimer::timeout, this, [this]() {
        m_hoverId.clear();
        refreshSide();
    });

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(buildRail());
    m_middle = new QStackedWidget(this);
    m_middle->addWidget(buildGridPage());
    m_middle->addWidget(buildDayNightPage());
    row->addWidget(m_middle, 1);
    row->addWidget(buildSide());
    main->addLayout(row, 1);
    main->addWidget(buildFooter());
    buildZoom();

    applyStyle();

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, &ThemesPanel::onThemeChanged);
    connect(Theme::Manager::instance(), &Theme::Manager::customThemesChanged, this, &ThemesPanel::onCustomThemesChanged);
    connect(Theme::Manager::instance(), &Theme::Manager::autoSwitchConfigChanged, this, &ThemesPanel::onAutoSwitchConfigChanged);
    connect(Theme::Manager::instance(), &Theme::Manager::favoritesChanged, this, &ThemesPanel::onFavoritesChanged);

    // Abre no tema em uso; tema personalizado em uso abre em Meus temas.
    const QString currentId = Theme::Manager::instance()->current().id;
    m_selectedId = currentId;
    refreshCounts();
    m_autoSwitchCheck->setChecked(Theme::Manager::instance()->autoSwitchConfig().enabled);
    m_autoSwitchBody->setVisible(m_autoSwitchCheck->isChecked());
    setView(Theme::Manager::instance()->isCustom(currentId) ? QStringLiteral("mine") : QStringLiteral("all"));
    setScene(ThemeScene::Writing);
    refreshFooter();
    refreshAutoSwitchUi();
}

void ThemesPanel::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (m_scrolledToCurrent) return;
    m_scrolledToCurrent = true;
    // Rola a grade até o tema em uso — depois do layout, quando a largura
    // (e portanto as colunas) já é a de verdade.
    QTimer::singleShot(0, this, [this]() {
        const QRect r = m_grid->itemRect(m_selectedId);
        if (r.isValid()) m_gridScroll->ensureVisible(r.center().x(), r.center().y(), 0, m_gridScroll->viewport()->height() / 2);
    });
}

// ------------------------------------------------------------------ montagem

QWidget* ThemesPanel::buildRail()
{
    auto* rail = new QWidget(this);
    rail->setObjectName(QStringLiteral("themesRail"));
    rail->setAttribute(Qt::WA_StyledBackground, true);
    rail->setFixedWidth(kRailW);
    auto* lay = new QVBoxLayout(rail);
    lay->setContentsMargins(0, 14, 0, 12);
    lay->setSpacing(2);

    auto* title = new QLabel(tr("Temas"), rail);
    title->setObjectName(QStringLiteral("railTitle"));
    title->setContentsMargins(16, 0, 8, 10);
    lay->addWidget(title);

    auto* searchWrap = new QWidget(rail);
    auto* sl = new QHBoxLayout(searchWrap);
    sl->setContentsMargins(10, 0, 10, 10);
    m_searchEdit = new QLineEdit(searchWrap);
    m_searchEdit->setObjectName(QStringLiteral("themesSearch"));
    m_searchEdit->setPlaceholderText(tr("Buscar tema"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchIcon = m_searchEdit->addAction(QIcon(), QLineEdit::LeadingPosition);
    sl->addWidget(m_searchEdit);
    lay->addWidget(searchWrap);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ThemesPanel::onSearchTextChanged);

    struct Item { QString key; QString label; RailItem::Glyph glyph; };
    const QList<Item> cats = {
        { QStringLiteral("all"),        tr("Todos"),      RailItem::None },
        { QStringLiteral("favorites"),  tr("Favoritos"),  RailItem::Heart },
        { QStringLiteral("light"),      tr("Claros"),     RailItem::None },
        { QStringLiteral("warm"),       tr("Amarelados"), RailItem::None },
        { QStringLiteral("dark"),       tr("Escuros"),    RailItem::None },
        { QStringLiteral("colorful"),   tr("Coloridos"),  RailItem::None },
        { QStringLiteral("estampados"), tr("Estampados"), RailItem::None },
    };
    auto add = [&](const Item& it) {
        auto* b = new RailItem(it.label, it.glyph, rail);
        b->setObjectName(QStringLiteral("rail_") + it.key);
        const QString key = it.key;
        connect(b, &QAbstractButton::clicked, this, [this, key]() { setView(key); });
        m_railItems.insert(key, b);
        lay->addWidget(b);
    };
    for (const Item& it : cats) add(it);

    auto* sepWrap = new QWidget(rail);
    auto* sepLay = new QHBoxLayout(sepWrap);
    sepLay->setContentsMargins(10, 8, 10, 8);
    auto* sep = new QFrame(sepWrap);
    sep->setObjectName(QStringLiteral("railSep"));
    sep->setFixedHeight(1);
    sepLay->addWidget(sep);
    lay->addWidget(sepWrap);

    add({ QStringLiteral("mine"), tr("Meus temas"), RailItem::User });
    add({ QStringLiteral("daynight"), tr("Dia e noite"), RailItem::Clock });
    lay->addStretch(1);
    return rail;
}

QWidget* ThemesPanel::buildGridPage()
{
    m_gridScroll = new QScrollArea(this);
    m_gridScroll->setObjectName(QStringLiteral("themesGridScroll"));
    m_gridScroll->setFrameShape(QFrame::NoFrame);
    m_gridScroll->setWidgetResizable(true);
    m_gridScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_grid = new ThemeGridView(m_gridScroll);
    m_grid->setObjectName(QStringLiteral("themeGrid"));
    m_gridScroll->setWidget(m_grid);

    m_grid->onHover = [this](const QString& id) { onGridHover(id); };
    m_grid->onClick = [this](const QString& id) { selectId(id); };
    m_grid->onDoubleClick = [this](const QString& id) { selectId(id); onApplyClicked(); };
    m_grid->onFavorite = [](const QString& id) {
        auto* mgr = Theme::Manager::instance();
        mgr->setFavorite(id, !mgr->isFavorite(id));
    };
    m_grid->onNew = [this]() { onNewClicked(); };
    return m_gridScroll;
}

QWidget* ThemesPanel::buildDayNightPage()
{
    auto* page = new QWidget(this);
    page->setObjectName(QStringLiteral("autoSwitchWrap"));
    page->setAttribute(Qt::WA_StyledBackground, true);
    auto* wrapLay = new QVBoxLayout(page);
    wrapLay->setContentsMargins(24, 22, 24, 22);
    wrapLay->setSpacing(10);

    auto* title = new QLabel(tr("Dia e noite"), page);
    title->setObjectName(QStringLiteral("dayNightTitle"));
    wrapLay->addWidget(title);
    auto* about = new QLabel(tr("Alterna sozinho entre um tema diurno e um noturno, nos horários que você definir."), page);
    about->setObjectName(QStringLiteral("autoSwitchRoleLabel"));
    about->setWordWrap(true);
    wrapLay->addWidget(about);
    wrapLay->addSpacing(6);

    m_autoSwitchCheck = new QCheckBox(tr("Troca automática por horário"), page);
    m_autoSwitchCheck->setObjectName(QStringLiteral("autoSwitchMasterCheck"));
    m_autoSwitchCheck->setToolTip(
        tr("Alterna sozinho entre um tema diurno e um noturno, nos horários que você definir."));
    wrapLay->addWidget(m_autoSwitchCheck);

    m_autoSwitchBody = new QWidget(page);
    auto* bodyLay = new QVBoxLayout(m_autoSwitchBody);
    bodyLay->setContentsMargins(20, 4, 0, 0);
    bodyLay->setSpacing(8);

    auto* dayRow = new QHBoxLayout;
    m_dayRoleCheck = new QCheckBox(tr("Diurno"), m_autoSwitchBody);
    m_dayThemeLabel = new QLabel(m_autoSwitchBody);
    m_dayThemeLabel->setObjectName(QStringLiteral("autoSwitchRoleLabel"));
    dayRow->addWidget(m_dayRoleCheck);
    dayRow->addWidget(m_dayThemeLabel, 1);
    bodyLay->addLayout(dayRow);
    auto* nightRow = new QHBoxLayout;
    m_nightRoleCheck = new QCheckBox(tr("Noturno"), m_autoSwitchBody);
    m_nightThemeLabel = new QLabel(m_autoSwitchBody);
    m_nightThemeLabel->setObjectName(QStringLiteral("autoSwitchRoleLabel"));
    nightRow->addWidget(m_nightRoleCheck);
    nightRow->addWidget(m_nightThemeLabel, 1);
    bodyLay->addLayout(nightRow);
    m_roleHint = new QLabel(m_autoSwitchBody);
    m_roleHint->setObjectName(QStringLiteral("autoSwitchRoleLabel"));
    m_roleHint->setWordWrap(true);
    bodyLay->addWidget(m_roleHint);

    bodyLay->addSpacing(8);
    auto* dayTimeRow = new QHBoxLayout;
    dayTimeRow->addWidget(new QLabel(tr("Trocar pro diurno às"), m_autoSwitchBody));
    m_dayStartEdit = new QTimeEdit(m_autoSwitchBody);
    m_dayStartEdit->setDisplayFormat(QStringLiteral("HH:mm"));
    dayTimeRow->addWidget(m_dayStartEdit);
    dayTimeRow->addStretch(1);
    bodyLay->addLayout(dayTimeRow);
    auto* nightTimeRow = new QHBoxLayout;
    nightTimeRow->addWidget(new QLabel(tr("Trocar pro noturno às"), m_autoSwitchBody));
    m_nightStartEdit = new QTimeEdit(m_autoSwitchBody);
    m_nightStartEdit->setDisplayFormat(QStringLiteral("HH:mm"));
    nightTimeRow->addWidget(m_nightStartEdit);
    nightTimeRow->addStretch(1);
    bodyLay->addLayout(nightTimeRow);

    wrapLay->addWidget(m_autoSwitchBody);
    wrapLay->addStretch(1);

    connect(m_autoSwitchCheck, &QCheckBox::toggled, this, &ThemesPanel::onAutoSwitchToggled);
    connect(m_dayRoleCheck, &QCheckBox::toggled, this, &ThemesPanel::onDayRoleToggled);
    connect(m_nightRoleCheck, &QCheckBox::toggled, this, &ThemesPanel::onNightRoleToggled);
    connect(m_dayStartEdit, &QTimeEdit::timeChanged, this, &ThemesPanel::onAutoSwitchTimeChanged);
    connect(m_nightStartEdit, &QTimeEdit::timeChanged, this, &ThemesPanel::onAutoSwitchTimeChanged);
    return page;
}

static QLabel* sectionHead(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("sectionHead"));
    QFont f = px(10, QFont::DemiBold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    f.setCapitalization(QFont::AllUppercase);
    l->setFont(f);
    return l;
}

QWidget* ThemesPanel::buildSide()
{
    auto* side = new QWidget(this);
    side->setObjectName(QStringLiteral("themesSide"));
    side->setAttribute(Qt::WA_StyledBackground, true);
    side->setFixedWidth(kSideW);
    auto* lay = new QVBoxLayout(side);
    lay->setContentsMargins(kSidePadX, 16, kSidePadX, 16);
    lay->setSpacing(12);

    auto* tabs = new QHBoxLayout;
    tabs->setSpacing(18);
    const QStringList names = { tr("Escrita"), tr("Gavetas"), tr("Editor") };
    for (int i = 0; i < names.size(); ++i) {
        auto* b = new QPushButton(names.at(i), side);
        b->setObjectName(QStringLiteral("sceneTab"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, i]() { setScene(i); });
        m_sceneTabs.append(b);
        tabs->addWidget(b);
    }
    tabs->addStretch(1);
    lay->addLayout(tabs);

    m_preview = new ComparePreview(true, side);
    m_preview->setObjectName(QStringLiteral("themePreview"));
    m_preview->setFixedSize(kPreviewW, qRound(kPreviewW * ThemeScene::kSceneH / ThemeScene::kSceneW));
    m_preview->onCut = [this](qreal v) { setCut(v); };
    m_preview->onZoom = [this]() { setZoomVisible(true); };
    lay->addWidget(m_preview);

    auto* nameRow = new QHBoxLayout;
    nameRow->setSpacing(8);
    m_nameLabel = new QLabel(side);
    m_nameLabel->setObjectName(QStringLiteral("themeName"));
    QFont nf(QStringLiteral("Lora"));
    nf.setPixelSize(19);
    nf.setWeight(QFont::DemiBold);
    m_nameLabel->setFont(nf);
    nameRow->addWidget(m_nameLabel, 1);
    m_favButton = new QToolButton(side);
    m_favButton->setObjectName(QStringLiteral("favBtn"));
    m_favButton->setText(QStringLiteral("♥"));
    m_favButton->setCursor(Qt::PointingHandCursor);
    m_favButton->setToolTip(tr("Favoritar"));
    connect(m_favButton, &QToolButton::clicked, this, [this]() {
        auto* mgr = Theme::Manager::instance();
        const QString id = shownId();
        mgr->setFavorite(id, !mgr->isFavorite(id));
    });
    nameRow->addWidget(m_favButton);
    lay->addLayout(nameRow);
    m_recBadge = new QLabel(side);
    m_recBadge->setObjectName(QStringLiteral("recBadge"));
    m_recBadge->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_recBadge->hide();
    lay->addWidget(m_recBadge);
    m_loreCard = new LoreCard(side);
    m_loreCard->onClick = [this]() { showCatPhoto(shownId()); };
    m_loreCard->hide();
    lay->addWidget(m_loreCard);

    // Contraste
    auto* cf = new QVBoxLayout;
    cf->setSpacing(6);
    cf->addWidget(sectionHead(tr("Contraste"), side));
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setVerticalSpacing(6);
    auto mk = [side](const char* obj) {
        auto* l = new QLabel(side);
        l->setObjectName(QString::fromLatin1(obj));
        l->setTextFormat(Qt::RichText);
        return l;
    };
    auto* k1 = new QLabel(tr("Texto"), side);
    k1->setObjectName(QStringLiteral("cfKey"));
    auto* k2 = new QLabel(tr("Texto apagado"), side);
    k2->setObjectName(QStringLiteral("cfKey"));
    m_contrastText = mk("cfVal");
    m_contrastMuted = mk("cfVal");
    m_contrastTextUsing = mk("cfUsing");
    m_contrastMutedUsing = mk("cfUsing");
    m_contrastTextUsing->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_contrastMutedUsing->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_dotText = new Dot(side);
    m_dotMuted = new Dot(side);
    grid->addWidget(k1, 0, 0);
    grid->addWidget(m_dotText, 0, 1, Qt::AlignVCenter);
    grid->addWidget(m_contrastText, 0, 2);
    grid->addWidget(m_contrastTextUsing, 0, 3);
    grid->addWidget(k2, 1, 0);
    grid->addWidget(m_dotMuted, 1, 1, Qt::AlignVCenter);
    grid->addWidget(m_contrastMuted, 1, 2);
    grid->addWidget(m_contrastMutedUsing, 1, 3);
    grid->setColumnMinimumWidth(1, 9);
    grid->setColumnStretch(2, 1);
    cf->addLayout(grid);
    lay->addLayout(cf);

    // Parecidos com este
    auto* sim = new QVBoxLayout;
    sim->setSpacing(8);
    sim->setContentsMargins(0, 6, 0, 0);
    sim->addWidget(sectionHead(tr("Parecidos com este"), side));
    m_similar = new SimilarStrip(side);
    m_similar->onPick = [this](const QString& id) { selectId(id); };
    sim->addWidget(m_similar);
    lay->addLayout(sim);

    lay->addStretch(1);

    auto* acts = new QHBoxLayout;
    acts->setSpacing(4);
    m_applyButton = new QPushButton(tr("Aplicar"), side);
    m_applyButton->setObjectName(QStringLiteral("applyBtn"));
    m_applyButton->setCursor(Qt::PointingHandCursor);
    connect(m_applyButton, &QPushButton::clicked, this, &ThemesPanel::onApplyClicked);
    // Personalizar: nos temas do app abre o Criador numa cópia (o original
    // fica intacto, a cópia vai pra Meus temas); nos do usuário vira "Editar".
    m_customizeButton = new QPushButton(tr("Personalizar"), side);
    m_customizeButton->setObjectName(QStringLiteral("customizeBtn"));
    m_customizeButton->setCursor(Qt::PointingHandCursor);
    connect(m_customizeButton, &QPushButton::clicked, this, [this]() {
        const QString id = shownId();
        if (id.isEmpty()) return;
        m_selectedId = id;
        m_hoverId.clear();
        if (Theme::Manager::instance()->isCustom(id)) onEditClicked();
        else onDuplicateClicked();
    });
    m_moreButton = new QToolButton(side);
    m_moreButton->setObjectName(QStringLiteral("moreBtn"));
    m_moreButton->setText(QStringLiteral("⋯"));
    m_moreButton->setCursor(Qt::PointingHandCursor);
    m_moreButton->setToolTip(tr("Mais ações"));
    connect(m_moreButton, &QToolButton::clicked, this, &ThemesPanel::showMoreMenu);
    acts->addWidget(m_applyButton);
    acts->addSpacing(4);
    acts->addWidget(m_customizeButton);
    acts->addWidget(m_moreButton);
    acts->addStretch(1);
    lay->addLayout(acts);
    return side;
}

QWidget* ThemesPanel::buildFooter()
{
    auto* foot = new QWidget(this);
    foot->setObjectName(QStringLiteral("themesFooter"));
    foot->setAttribute(Qt::WA_StyledBackground, true);
    auto* lay = new QHBoxLayout(foot);
    lay->setContentsMargins(14, 10, 14, 10);
    lay->setSpacing(10);
    m_usingLabel = new QLabel(foot);
    m_usingLabel->setObjectName(QStringLiteral("usingLabel"));
    m_usingLabel->setTextFormat(Qt::RichText);
    lay->addWidget(m_usingLabel, 1);
    m_importButton = new QPushButton(tr("Importar…"), foot);
    m_importButton->setObjectName(QStringLiteral("footBtn"));
    m_importButton->setToolTip(tr("Abrir um tema que você recebeu de outra pessoa."));
    m_closeButton = new QPushButton(tr("Fechar"), foot);
    m_closeButton->setObjectName(QStringLiteral("footBtn"));
    lay->addWidget(m_importButton);
    lay->addWidget(m_closeButton);
    connect(m_importButton, &QPushButton::clicked, this, &ThemesPanel::onImportClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    return foot;
}

void ThemesPanel::buildZoom()
{
    m_zoom = new QWidget(this);
    m_zoom->setObjectName(QStringLiteral("themesZoom"));
    m_zoom->setAttribute(Qt::WA_StyledBackground, true);
    auto* lay = new QVBoxLayout(m_zoom);
    lay->setContentsMargins(25, 14, 25, 14);
    lay->setSpacing(12);
    auto* top = new QHBoxLayout;
    top->setSpacing(18);
    const QStringList names = { tr("Escrita"), tr("Gavetas"), tr("Editor") };
    for (int i = 0; i < names.size(); ++i) {
        auto* b = new QPushButton(names.at(i), m_zoom);
        b->setObjectName(QStringLiteral("sceneTab"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, i]() { setScene(i); });
        m_zoomTabs.append(b);
        top->addWidget(b);
    }
    top->addStretch(1);
    m_zoomNames = new QLabel(m_zoom);
    m_zoomNames->setObjectName(QStringLiteral("zoomNames"));
    m_zoomNames->setTextFormat(Qt::RichText);
    top->addWidget(m_zoomNames);
    auto* close = new QPushButton(tr("Fechar ✕"), m_zoom);
    close->setObjectName(QStringLiteral("linkBtn"));
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QPushButton::clicked, this, [this]() { setZoomVisible(false); });
    top->addWidget(close);
    lay->addLayout(top);
    m_zoomPreview = new ComparePreview(false, m_zoom);
    m_zoomPreview->setObjectName(QStringLiteral("themeZoomPreview"));
    m_zoomPreview->onCut = [this](qreal v) { setCut(v); };
    lay->addWidget(m_zoomPreview, 0, Qt::AlignHCenter);
    lay->addStretch(1);
    m_zoom->hide();
}

// ------------------------------------------------------------------ estado

const Theme::MiraTheme* ThemesPanel::themeById(const QString& id) const
{
    if (id.isEmpty()) return nullptr;
    for (const auto& t : Theme::Manager::instance()->available())
        if (t.id == id) return &t;
    return nullptr;
}

QString ThemesPanel::shownId() const
{
    if (themeById(m_hoverId)) return m_hoverId;
    if (themeById(m_selectedId)) return m_selectedId;
    return Theme::Manager::instance()->current().id;
}

void ThemesPanel::setView(const QString& key)
{
    m_view = key;
    for (auto it = m_railItems.constBegin(); it != m_railItems.constEnd(); ++it)
        it.value()->setChecked(it.key() == key);
    m_middle->setCurrentIndex(key == QLatin1String("daynight") ? 1 : 0);
    m_hoverId.clear();
    rebuildGrid();
    refreshSide();
    refreshAutoSwitchUi();
    m_gridScroll->verticalScrollBar()->setValue(0);
}

void ThemesPanel::rebuildGrid()
{
    auto* mgr = Theme::Manager::instance();
    QList<Theme::MiraTheme> list;
    QString empty = tr("Nenhum tema nessa categoria.");
    if (m_view == QLatin1String("mine")) {
        list = mgr->customThemes();
    } else if (m_view == QLatin1String("favorites")) {
        for (const auto& t : mgr->available()) if (mgr->isFavorite(t.id)) list.append(t);
        empty = tr("Nenhum favorito ainda.\nClique no coração de um tema pra marcá-lo.");
    } else if (m_view == QLatin1String("all") || m_view == QLatin1String("daynight")) {
        list = mgr->bundledThemes();
    } else {
        for (const auto& t : mgr->bundledThemes()) if (t.category == m_view) list.append(t);
        // As recomendações da categoria vêm primeiro, na ordem da lista.
        const QStringList rec = recommended().value(m_view);
        std::stable_sort(list.begin(), list.end(), [&rec](const Theme::MiraTheme& a, const Theme::MiraTheme& b) {
            const int ia = rec.indexOf(a.id), ib = rec.indexOf(b.id);
            return (ia < 0 ? INT_MAX : ia) < (ib < 0 ? INT_MAX : ib);
        });
    }
    if (!m_searchText.isEmpty()) {
        QList<Theme::MiraTheme> filtered;
        for (const auto& t : list) if (t.name.toLower().contains(m_searchText)) filtered.append(t);
        list = filtered;
        empty = tr("Nenhum tema encontrado.");
    }
    const bool newTile = (m_view == QLatin1String("mine")) && m_searchText.isEmpty();
    m_grid->setThemes(list, newTile, empty);
    m_grid->setSelected(m_selectedId);
}

void ThemesPanel::refreshCounts()
{
    auto* mgr = Theme::Manager::instance();
    const auto bundled = mgr->bundledThemes();
    QHash<QString, int> n;
    for (const auto& t : bundled) n[t.category] += 1;
    int fav = 0;
    for (const auto& t : mgr->available()) if (mgr->isFavorite(t.id)) ++fav;
    for (auto it = m_railItems.constBegin(); it != m_railItems.constEnd(); ++it) {
        const QString& k = it.key();
        int v = -1;
        if (k == QLatin1String("all")) v = bundled.size();
        else if (k == QLatin1String("favorites")) v = fav;
        else if (k == QLatin1String("mine")) v = mgr->customThemes().size();
        else if (k != QLatin1String("daynight")) v = n.value(k);
        it.value()->setCount(v);
    }
}

void ThemesPanel::refreshSide()
{
    auto* mgr = Theme::Manager::instance();
    const Theme::MiraTheme& cur = mgr->current();
    const Theme::MiraTheme* shown = themeById(shownId());
    if (!shown) shown = &cur;
    const bool isCur = shown->id == cur.id;

    m_preview->setThemes(cur, *shown);
    m_zoomPreview->setThemes(cur, *shown);
    m_zoomNames->setText(QStringLiteral("%1 <span style='color:%3'>|</span> %2")
                             .arg(cur.name.toHtmlEscaped(), shown->name.toHtmlEscaped(), Theme::subtleBorder()));

    m_nameLabel->setText(shown->name);
    m_recBadge->setText(QStringLiteral("★  ") + (isTonyPick(shown->id) ? tr("Escolha do Tony")
                                                                        : tr("Recomendação do desenvolvedor")));
    m_recBadge->setVisible(isRecommended(shown->id));
    // Temas com lore própria — recado de bastidores, clicável (abre a foto).
    if (shown->id == QLatin1String("tifu")) {
        m_loreCard->setText(tr("🐈‍⬛ Esse theme foi feito inspirado no gato preto e "
                               "calmo como a noite — Tifu, O Sábio."));
    } else if (shown->id == QLatin1String("tommy")) {
        m_loreCard->setText(tr("🐈 Esse theme foi feito inspirado na hiperatividade e "
                               "inquietação do melhor gato laranja — Tommy, O Temível."));
    }
    m_loreCard->setVisible(shown->id == QLatin1String("tifu") || shown->id == QLatin1String("tommy"));
    const bool fav = mgr->isFavorite(shown->id);
    m_favButton->setStyleSheet(QStringLiteral("QToolButton#favBtn { color: %1; }")
                                   .arg(fav ? kFav.name() : Theme::textMuted()));

    // Contraste: texto e texto apagado contra a folha.
    auto pair = [](const Theme::MiraTheme& t) {
        const QColor ed = Theme::toColor(t.editorBackground);
        return qMakePair(ThemeScene::contrast(Theme::toColor(t.editorTextColor), ed),
                         ThemeScene::contrast(Theme::toColor(t.textMuted), ed));
    };
    const auto s = pair(*shown), u = pair(cur);
    auto val = [](double v) { return QStringLiteral("%1 · %2").arg(levelWord(v).toHtmlEscaped(), num1(v)); };
    m_dotText->setColor(Theme::toColor(shown->editorTextColor));
    m_dotMuted->setColor(Theme::toColor(shown->textMuted));
    m_contrastText->setText(val(s.first));
    m_contrastMuted->setText(val(s.second));
    m_contrastTextUsing->setText(isCur ? QString() : tr("em uso %1").arg(num1(u.first)));
    m_contrastMutedUsing->setText(isCur ? QString() : tr("em uso %1").arg(num1(u.second)));

    // Parecidos com este
    QList<QPair<double, const Theme::MiraTheme*>> ranked;
    for (const auto& t : mgr->available())
        if (t.id != shown->id) ranked.append({ ThemeScene::distance(*shown, t), &t });
    std::partial_sort(ranked.begin(), ranked.begin() + qMin<qsizetype>(4, ranked.size()), ranked.end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });
    QList<Theme::MiraTheme> sims;
    for (int i = 0; i < qMin<qsizetype>(4, ranked.size()); ++i) sims.append(*ranked.at(i).second);
    m_similar->setThemes(sims);

    m_applyButton->setEnabled(!isCur);
    m_applyButton->setText(isCur ? tr("Em uso") : tr("Aplicar"));
    m_customizeButton->setText(mgr->isCustom(shown->id) ? tr("Editar") : tr("Personalizar"));
}

void ThemesPanel::refreshFooter()
{
    m_usingLabel->setText(QStringLiteral("%1&nbsp;&nbsp;<b style='color:%2'>%3</b>")
                              .arg(tr("Em uso:").toHtmlEscaped(), Theme::textBright(),
                                   Theme::Manager::instance()->current().name.toHtmlEscaped()));
}

void ThemesPanel::setScene(int scene)
{
    m_scene = scene;
    for (int i = 0; i < m_sceneTabs.size(); ++i) m_sceneTabs.at(i)->setChecked(i == scene);
    for (int i = 0; i < m_zoomTabs.size(); ++i) m_zoomTabs.at(i)->setChecked(i == scene);
    m_preview->setScene(scene);
    m_zoomPreview->setScene(scene);
}

void ThemesPanel::setCut(qreal cut)
{
    // A linha fica no mesmo lugar ao trocar de tema ou de cena, e vale nas duas prévias.
    m_cut = cut;
    m_preview->setCut(cut);
    m_zoomPreview->setCut(cut);
}

void ThemesPanel::setZoomVisible(bool on)
{
    if (on) {
        m_zoom->setGeometry(rect());
        const int w = m_zoom->width() - 50;
        const int maxH = m_zoom->height() - 14 * 2 - 40 - 12;
        int h = qRound(w * ThemeScene::kSceneH / ThemeScene::kSceneW);
        int ww = w;
        if (h > maxH) { h = maxH; ww = qRound(h * ThemeScene::kSceneW / ThemeScene::kSceneH); }
        m_zoomPreview->setFixedSize(ww, h);
        m_zoom->show();
        m_zoom->raise();
        m_zoom->setFocus();
    } else {
        m_zoom->hide();
    }
}

void ThemesPanel::selectId(const QString& id)
{
    m_selectedId = id;
    m_hoverId.clear();
    m_hoverTimer->stop();
    m_grid->setSelected(id);
    refreshSide();
    refreshAutoSwitchUi();
}

void ThemesPanel::onGridHover(const QString& id)
{
    if (!id.isEmpty()) {
        m_hoverTimer->stop();
        if (m_hoverId != id) {
            m_hoverId = id;
            refreshSide();
        }
    } else if (!m_hoverId.isEmpty()) {
        m_hoverTimer->start(140);
    }
}

void ThemesPanel::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && m_zoom && m_zoom->isVisible()) {
        setZoomVisible(false);
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void ThemesPanel::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    if (m_zoom && m_zoom->isVisible()) setZoomVisible(true);
}

// ------------------------------------------------------------------ ações

void ThemesPanel::showMoreMenu()
{
    const QString id = shownId();
    auto* mgr = Theme::Manager::instance();
    const bool custom = mgr->isCustom(id);
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setToolTipsVisible(true);
    auto select = [this, id]() { m_selectedId = id; m_hoverId.clear(); };
    connect(menu->addAction(tr("Editar uma cópia")), &QAction::triggered, this, [this, select]() { select(); onDuplicateClicked(); });
    QAction* exp = menu->addAction(tr("Exportar"));
    // Exportar só tema personalizado: quem recebe já tem todos os padrões.
    // Pra compartilhar um padrão, edite uma cópia e exporte a cópia.
    exp->setEnabled(custom);
    if (!custom) exp->setToolTip(tr("Só temas seus. Pra compartilhar este, edite uma cópia e exporte a cópia."));
    connect(exp, &QAction::triggered, this, [this, select]() { select(); onExportClicked(); });
    if (custom) {
        menu->addSeparator();
        connect(menu->addAction(tr("Excluir")), &QAction::triggered, this, [this, select]() { select(); onDeleteClicked(); });
    }
    menu->ensurePolished();
    const QSize sz = menu->sizeHint();
    menu->popup(m_moreButton->mapToGlobal(QPoint(0, -sz.height() - 6)));
}

void ThemesPanel::onApplyClicked()
{
    const QString id = shownId();
    if (id.isEmpty() || id == Theme::Manager::instance()->current().id) return;
    m_selectedId = id;
    m_hoverId.clear();
    Theme::Manager::instance()->setCurrent(id);
}

void ThemesPanel::showCatPhoto(const QString& id)
{
    const bool tifu = id == QLatin1String("tifu");
    if (!tifu && id != QLatin1String("tommy")) return;
    const Theme::MiraTheme* t = themeById(id);
    // Duas fotos de cada: o primeiro clique mostra sempre a principal (Tifu
    // contemplativo, Tommy de olhar "Temível"), depois alterna com a segunda.
    // Contagem só da sessão.
    static int clicks[2] = { 0, 0 };
    const bool first = clicks[tifu ? 0 : 1]++ % 2 == 0;
    const QString base = tifu ? QStringLiteral(":/app/cat-tifu") : QStringLiteral(":/app/cat-tommy");
    new CatPhotoOverlay(this,
                        base + (first ? QStringLiteral(".jpg") : QStringLiteral("-2.jpg")),
                        tifu ? tr("Tifu, O Sábio") : tr("Tommy, O Temível"),
                        Theme::toColor(t ? t->accentDefault : Theme::accentDefault()));
}

void ThemesPanel::onNewClicked()
{
    // Tema em branco a partir do primeiro padrão.
    auto* mgr = Theme::Manager::instance();
    Theme::MiraTheme base = mgr->bundledThemes().value(0);
    base.bundled = false;
    base.id = mgr->uniqueCustomId();
    base.name = tr("Novo tema");

    ThemeEditorDialog dlg(base, this);
    if (dlg.exec() == QDialog::Accepted) {
        const QString id = mgr->upsertCustom(dlg.theme());
        m_selectedId = id;
        setView(QStringLiteral("mine"));
    }
}

void ThemesPanel::onDuplicateClicked()
{
    const Theme::MiraTheme* found = themeById(m_selectedId);
    if (!found) return;
    auto* mgr = Theme::Manager::instance();
    Theme::MiraTheme copy = *found;
    copy.bundled = false;
    copy.id = mgr->uniqueCustomId();
    copy.name = tr("%1 (cópia)").arg(found->name);

    ThemeEditorDialog dlg(copy, this);
    if (dlg.exec() == QDialog::Accepted) {
        const QString id = mgr->upsertCustom(dlg.theme());
        m_selectedId = id;
        setView(QStringLiteral("mine"));
    }
}

void ThemesPanel::onEditClicked()
{
    auto* mgr = Theme::Manager::instance();
    if (!mgr->isCustom(m_selectedId)) return;
    const Theme::MiraTheme* found = themeById(m_selectedId);
    if (!found) return;
    ThemeEditorDialog dlg(*found, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_selectedId = mgr->upsertCustom(dlg.theme());
        rebuildGrid();
        refreshSide();
    }
}

void ThemesPanel::onDeleteClicked()
{
    auto* mgr = Theme::Manager::instance();
    if (!mgr->isCustom(m_selectedId)) return;
    const auto answer = QMessageBox::question(
        this, tr("Excluir tema"),
        tr("Excluir este tema personalizado? Esta ação não pode ser desfeita."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    mgr->removeCustom(m_selectedId);
    m_selectedId = mgr->current().id;
    rebuildGrid();
    refreshSide();
}

void ThemesPanel::onExportClicked()
{
    auto* mgr = Theme::Manager::instance();
    if (!mgr->isCustom(m_selectedId)) return;
    const Theme::MiraTheme* found = themeById(m_selectedId);
    if (!found) return;

    ThemeExportDialog meta(*found, this);
    if (meta.exec() != QDialog::Accepted) return;

    Theme::MiraTheme out = meta.theme();

    // Identidade estável do tema, criada na primeira exportação e mantida pra
    // sempre — é por ela que uma atualização do mesmo tema é reconhecida como
    // atualização, e não como um tema novo.
    if (out.uuid.isEmpty())
        out.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Cada exportação conta como uma publicação: quem receber duas versões do
    // mesmo tema precisa saber qual é a mais recente.
    out.themeVersion = qMax(1, found->themeVersion + 1);
    out.minAppVersion = QStringLiteral(APP_VERSION);

    const QString startDir = QDir::cleanPath(
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QLatin1Char('/') + ThemePackage::suggestedFileName(out));

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Exportar tema"), startDir, ThemePackage::fileFilter());
    if (path.isEmpty()) return;

    QString error;
    if (!ThemePackage::exportToFile(out, path, &error)) {
        QMessageBox::warning(this, tr("Exportar tema"), error);
        return;
    }

    // Só grava os metadados no tema local depois que o arquivo saiu: se a
    // escrita falhar, o tema fica como estava e a versão não avança à toa.
    mgr->upsertCustom(out);
    rebuildGrid();

    QMessageBox::information(
        this, tr("Tema exportado"),
        tr("“%1” foi salvo em:\n%2\n\nEsse arquivo pode ser enviado pra qualquer pessoa que use o Qenna.")
            .arg(out.name, QDir::toNativeSeparators(path)));
}

void ThemesPanel::onImportClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Importar tema"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        ThemePackage::fileFilter());
    if (path.isEmpty()) return;

    QString error;
    ThemePackage::ImportResult result;
    if (!ThemePackage::importFromFile(path, &result, &error)) {
        QMessageBox::warning(this, tr("Importar tema"), error);
        return;
    }

    auto* mgr = Theme::Manager::instance();

    // Já temos esse mesmo tema? A comparação é por uuid, não por nome: o nome
    // pode ter sido mudado dos dois lados e continua sendo o mesmo tema.
    const QList<Theme::MiraTheme> customs = mgr->customThemes();
    const Theme::MiraTheme* existing = nullptr;
    for (const auto& t : customs) {
        if (!t.uuid.isEmpty() && t.uuid == result.theme.uuid) { existing = &t; break; }
    }

    // Resumo do que está entrando — é aqui que o crédito de quem fez aparece,
    // antes de o tema virar mais um card no grid sem dono.
    QString summary = tr("<b>%1</b>").arg(result.theme.name.toHtmlEscaped());
    if (!result.theme.author.isEmpty())
        summary += tr("<br>por %1").arg(result.theme.author.toHtmlEscaped());
    if (!result.theme.description.isEmpty())
        summary += QStringLiteral("<br><i>%1</i>").arg(result.theme.description.toHtmlEscaped());
    if (!result.theme.license.isEmpty()) {
        summary += QStringLiteral("<br><br>%1").arg(
            tr("Licença: %1").arg(ThemePackage::licenseDisplayName(result.theme.license).toHtmlEscaped()));
    }
    if (!result.theme.authorContact.isEmpty())
        summary += QStringLiteral("<br>%1").arg(result.theme.authorContact.toHtmlEscaped());

    QMessageBox box(this);
    box.setWindowTitle(tr("Importar tema"));
    box.setTextFormat(Qt::RichText);
    box.setIcon(QMessageBox::NoIcon);

    QPushButton* replaceBtn = nullptr;
    QPushButton* keepBothBtn = nullptr;
    if (existing) {
        box.setText(summary + QStringLiteral("<br><br>") +
                    tr("Você já tem este tema (versão %1). O arquivo traz a versão %2.")
                        .arg(existing->themeVersion).arg(result.theme.themeVersion));
        replaceBtn = box.addButton(tr("Substituir"), QMessageBox::AcceptRole);
        keepBothBtn = box.addButton(tr("Manter os dois"), QMessageBox::ActionRole);
        box.addButton(tr("Cancelar"), QMessageBox::RejectRole);
        box.setDefaultButton(replaceBtn);
    } else {
        box.setText(summary);
        replaceBtn = box.addButton(tr("Importar"), QMessageBox::AcceptRole);
        box.addButton(tr("Cancelar"), QMessageBox::RejectRole);
        box.setDefaultButton(replaceBtn);
    }
    box.exec();

    const QAbstractButton* clicked = box.clickedButton();
    if (clicked != replaceBtn && clicked != keepBothBtn) return;

    Theme::MiraTheme incoming = result.theme;
    if (existing && clicked == replaceBtn) {
        // Substituição: fica no lugar do que já existia, mantendo o id local
        // (assim continua sendo o mesmo card, e segue aplicado se estiver em uso).
        incoming.id = existing->id;
    } else {
        incoming.id = mgr->uniqueCustomId();
        if (existing) {
            // "Manter os dois" precisa de uuid novo, senão as duas cópias
            // continuam sendo "o mesmo tema" e uma próxima importação não
            // saberia qual atualizar.
            incoming.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
            incoming.name = tr("%1 (cópia)").arg(incoming.name);
        }
    }

    m_selectedId = mgr->upsertCustom(incoming);
    setView(QStringLiteral("mine"));

    if (!result.warning.isEmpty())
        QMessageBox::information(this, tr("Importar tema"), result.warning);
}

// ------------------------------------------------------------------ sinais

void ThemesPanel::onThemeChanged()
{
    applyStyle();
    m_grid->update();
    for (auto* it : std::as_const(m_railItems)) it->update();
    refreshSide();
    refreshFooter();
}

void ThemesPanel::onCustomThemesChanged()
{
    refreshCounts();
    rebuildGrid();
    refreshSide();
}

void ThemesPanel::onFavoritesChanged()
{
    refreshCounts();
    if (m_view == QLatin1String("favorites")) rebuildGrid();
    else m_grid->update();
    refreshSide();
}

void ThemesPanel::onSearchTextChanged(const QString& text)
{
    m_searchText = text.trimmed().toLower();
    // Buscar a partir do Dia e noite leva pra grade de todos.
    if (m_view == QLatin1String("daynight") && !m_searchText.isEmpty()) {
        setView(QStringLiteral("all"));
        return;
    }
    rebuildGrid();
}

void ThemesPanel::onAutoSwitchToggled(bool checked)
{
    if (m_syncingAutoSwitchUi) return;
    m_autoSwitchBody->setVisible(checked);

    auto* mgr = Theme::Manager::instance();
    Theme::AutoSwitchConfig cfg = mgr->autoSwitchConfig();
    if (!checked) {
        cfg.enabled = false;
        mgr->setAutoSwitchConfig(cfg);
        return;
    }
    // Só liga de verdade quando os dois papéis já estão atribuídos — senão o
    // checkbox fica marcado (corpo expandido) esperando a atribuição.
    if (!cfg.dayThemeId.isEmpty() && !cfg.nightThemeId.isEmpty()) {
        cfg.enabled = true;
        mgr->setAutoSwitchConfig(cfg);
    }
}

void ThemesPanel::onDayRoleToggled(bool checked)
{
    if (m_syncingAutoSwitchUi || m_selectedId.isEmpty()) return;
    auto* mgr = Theme::Manager::instance();
    Theme::AutoSwitchConfig cfg = mgr->autoSwitchConfig();
    if (checked) {
        cfg.dayThemeId = m_selectedId;
        if (cfg.nightThemeId == m_selectedId) cfg.nightThemeId.clear();
    } else if (cfg.dayThemeId == m_selectedId) {
        cfg.dayThemeId.clear();
    }
    cfg.enabled = m_autoSwitchCheck->isChecked()
        && !cfg.dayThemeId.isEmpty() && !cfg.nightThemeId.isEmpty();
    mgr->setAutoSwitchConfig(cfg);
}

void ThemesPanel::onNightRoleToggled(bool checked)
{
    if (m_syncingAutoSwitchUi || m_selectedId.isEmpty()) return;
    auto* mgr = Theme::Manager::instance();
    Theme::AutoSwitchConfig cfg = mgr->autoSwitchConfig();
    if (checked) {
        cfg.nightThemeId = m_selectedId;
        if (cfg.dayThemeId == m_selectedId) cfg.dayThemeId.clear();
    } else if (cfg.nightThemeId == m_selectedId) {
        cfg.nightThemeId.clear();
    }
    cfg.enabled = m_autoSwitchCheck->isChecked()
        && !cfg.dayThemeId.isEmpty() && !cfg.nightThemeId.isEmpty();
    mgr->setAutoSwitchConfig(cfg);
}

void ThemesPanel::onAutoSwitchTimeChanged()
{
    if (m_syncingAutoSwitchUi) return;
    auto* mgr = Theme::Manager::instance();
    Theme::AutoSwitchConfig cfg = mgr->autoSwitchConfig();
    cfg.dayStart = m_dayStartEdit->time();
    cfg.nightStart = m_nightStartEdit->time();
    mgr->setAutoSwitchConfig(cfg);
}

void ThemesPanel::onAutoSwitchConfigChanged()
{
    const auto& cfg = Theme::Manager::instance()->autoSwitchConfig();
    // Desligamento externo de verdade (override manual de tema ou exclusão do
    // tema de um papel) — os dois ids continuam preenchidos mas enabled caiu
    // sozinho. Atribuição incompleta de papéis (algum id vazio, no meio do
    // fluxo de configuração) não deve recolher o painel.
    if (!cfg.enabled && m_autoSwitchCheck->isChecked()
        && !cfg.dayThemeId.isEmpty() && !cfg.nightThemeId.isEmpty()) {
        m_syncingAutoSwitchUi = true;
        m_autoSwitchCheck->setChecked(false);
        m_syncingAutoSwitchUi = false;
    }
    m_autoSwitchBody->setVisible(m_autoSwitchCheck->isChecked());
    refreshAutoSwitchUi();
    m_grid->update();
}

void ThemesPanel::refreshAutoSwitchUi()
{
    if (!m_autoSwitchCheck) return;
    const auto* mgr = Theme::Manager::instance();
    const auto& cfg = mgr->autoSwitchConfig();

    m_syncingAutoSwitchUi = true;

    const Theme::MiraTheme* dayTheme = themeById(cfg.dayThemeId);
    const Theme::MiraTheme* nightTheme = themeById(cfg.nightThemeId);
    m_dayRoleCheck->setChecked(!m_selectedId.isEmpty() && m_selectedId == cfg.dayThemeId);
    m_nightRoleCheck->setChecked(!m_selectedId.isEmpty() && m_selectedId == cfg.nightThemeId);
    m_dayThemeLabel->setText(dayTheme
        ? tr("— %1").arg(dayTheme->name)
        : tr("— nenhum tema definido"));
    m_nightThemeLabel->setText(nightTheme
        ? tr("— %1").arg(nightTheme->name)
        : tr("— nenhum tema definido"));
    const Theme::MiraTheme* sel = themeById(m_selectedId);
    m_roleHint->setText(tr("Diurno e Noturno valem pro tema selecionado, o da prévia à direita (agora: %1). "
                           "Pra escolher outro, clique nele em Todos ou numa categoria e volte aqui.")
                            .arg(sel ? sel->name : QStringLiteral("—")));
    m_dayStartEdit->setTime(cfg.dayStart);
    m_nightStartEdit->setTime(cfg.nightStart);

    m_syncingAutoSwitchUi = false;
}

void ThemesPanel::applyStyle()
{
    const QColor acc = Theme::toColor(Theme::accentDefault());
    auto accentAlpha = [&acc](qreal a) {
        return QStringLiteral("rgba(%1,%2,%3,%4)").arg(acc.red()).arg(acc.green()).arg(acc.blue()).arg(a);
    };
    setStyleSheet(Theme::qss(QStringLiteral(R"(
        #themesPanel { background: %1; }
        #themesRail, #themesSide, #themesFooter, #themesZoom { background: %1; }
        #autoSwitchWrap { background: %5; }
        #themesPanel QLabel { color: %2; }
        QLabel#railTitle { color: %3; font-size: 17px; font-weight: 600; }
        QLineEdit#themesSearch {
            background: %10; color: %3; border: 1px solid %6;
            border-radius: @radius-control; padding: 6px 8px; font-size: 12px;
        }
        QLineEdit#themesSearch:focus { border-color: %11; }
        QFrame#railSep { background: %8; border: none; }
        QScrollArea#themesGridScroll { background: %5; border: none; }
        QPushButton#sceneTab {
            background: transparent; border: none; border-bottom: 2px solid transparent;
            color: %4; font-size: 12px; padding: 2px 0px 6px 0px;
        }
        QPushButton#sceneTab:hover { color: %3; }
        QPushButton#sceneTab:checked { color: %3; border-bottom: 2px solid %9; }
        QLabel#themeName { color: %3; }
        QToolButton#favBtn { border: none; background: transparent; font-size: 17px; padding: 0px 2px; }
        QLabel#sectionHead { color: %4; }
        QLabel#recBadge {
            color: %9; background: %12; border: 1px solid %13;
            border-radius: @radius-control; padding: 3px 9px; font-size: 11px; font-weight: 600;
        }
        QLabel#loreCard {
            color: %2; background: transparent; border: 1px solid %8;
            border-radius: @radius-control; padding: 8px 10px; font-size: 12px;
        }
        QLabel#loreCard:hover { color: %3; background: %12; border-color: %13; }
        QLabel#cfKey { color: %4; font-size: 12px; }
        QLabel#cfVal { color: %3; font-size: 12px; }
        QLabel#cfUsing { color: %4; font-size: 11px; }
        QPushButton#applyBtn {
            background: %9; color: %5; border: 1px solid %9; border-radius: @radius-control;
            padding: 6px 14px; font-size: 13px; font-weight: 600;
        }
        QPushButton#applyBtn:disabled { background: transparent; color: %4; border-color: %6; }
        QPushButton#customizeBtn {
            background: transparent; color: %3; border: 1px solid %6; border-radius: @radius-control;
            padding: 6px 14px; font-size: 13px; font-weight: 600;
        }
        QPushButton#customizeBtn:hover { background: %7; border-color: %9; }
        QToolButton#moreBtn {
            border: none; background: transparent; color: %4; font-size: 17px;
            padding: 2px 10px; border-radius: @radius-control;
        }
        QToolButton#moreBtn:hover { color: %3; background: %7; }
        QPushButton#footBtn {
            background: transparent; color: %2; border: 1px solid %6;
            border-radius: @radius-control; padding: 6px 12px; font-size: 12px;
        }
        QPushButton#footBtn:hover { border-color: %11; color: %3; }
        QLabel#usingLabel { color: %4; font-size: 12px; }
        QLabel#zoomNames { color: %4; font-size: 12px; }
        QPushButton#linkBtn {
            background: transparent; border: none; color: %4; font-size: 12px;
            padding: 6px 8px; border-radius: @radius-control;
        }
        QPushButton#linkBtn:hover { color: %3; background: %7; }
        #autoSwitchWrap QLabel { font-size: 12px; }
        #autoSwitchWrap QCheckBox { color: %2; font-size: 12px; spacing: 6px; }
        #autoSwitchWrap QLabel#dayNightTitle { color: %3; font-size: 17px; font-weight: 600; }
        #autoSwitchWrap QCheckBox#autoSwitchMasterCheck { font-weight: 600; }
        #autoSwitchWrap QLabel#autoSwitchRoleLabel { color: %4; font-size: 12px; }
        #autoSwitchWrap QTimeEdit {
            background: %1; color: %2; border: 1px solid %6;
            border-radius: @radius-control; padding: 2px 6px; font-size: 12px;
        }
    )")).arg(
        Theme::appBackground(),     // 1
        Theme::textPrimary(),       // 2
        Theme::textBright(),        // 3
        Theme::textMuted(),         // 4
        Theme::panelBackground(),   // 5
        Theme::panelBorder(),       // 6
        Theme::hoverOverlay(),      // 7
        Theme::subtleBorder(),      // 8
        Theme::accentDefault(),     // 9
        Theme::inputBackground(),   // 10
        Theme::accentInfo(),        // 11
        accentAlpha(0.12),          // 12 — fundo do selo de recomendação
        accentAlpha(0.35)           // 13 — borda do selo
    ));
    if (m_searchIcon) {
        const QColor mu = Theme::toColor(Theme::textMuted());
        m_searchIcon->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/search.svg"), mu, mu, mu, QSize(14, 14)));
    }
    // Viewport não herda o fundo da QScrollArea (ver memória do projeto).
    if (m_gridScroll) {
        m_gridScroll->viewport()->setStyleSheet(
            QStringLiteral("QWidget#qt_scrollarea_viewport { background: %1; }").arg(Theme::panelBackground()));
    }
}
