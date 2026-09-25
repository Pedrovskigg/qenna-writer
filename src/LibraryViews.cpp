#include "LibraryViews.h"

#include "IconUtils.h"
#include "Theme.h"
#include "WordCounter.h"

#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTextLayout>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

constexpr qint64 kGoalDayMs = 24LL * 60 * 60 * 1000;

QColor tc(const QString& css) { return Theme::toColor(css); }
QColor alpha(QColor c, qreal a) { c.setAlphaF(a); return c; }

QFont sans(qreal px, int weight = QFont::Normal)
{
    QFont f = QApplication::font();
    f.setPixelSize(qRound(px));
    f.setWeight(QFont::Weight(weight));
    return f;
}

QFont serif(qreal px, int weight = QFont::Normal, bool italic = false)
{
    QFont f(QStringLiteral("Lora"));
    f.setPixelSize(qRound(px));
    f.setWeight(QFont::Weight(weight));
    f.setItalic(italic);
    return f;
}

QFont kickFont()
{
    QFont f = sans(10, QFont::Bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
    return f;
}

QLabel* label(const QString& text, const char* objectName, const QFont& font, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(QLatin1String(objectName));
    l->setFont(font);
    return l;
}

QLabel* kick(const QString& text, QWidget* parent, const char* objectName = "libKick")
{
    return label(text.toUpper(), objectName, kickFont(), parent);
}

// Arte da capa num tamanho fixo: recorte arredondado, vinco de lombada à
// esquerda e um contorno sutil. A sombra fica por conta de quem pinta, que é
// o que se mexe no hover.
QPixmap renderCoverArt(const QPixmap& src, int w, int h, qreal dpr)
{
    QPixmap pm(QSize(w, h) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QRectF r(0, 0, w, h);
    QPainterPath clip;
    clip.addRoundedRect(r, 3, 3);
    p.setClipPath(clip);
    if (!src.isNull()) {
        QPixmap s = src.scaled(QSize(w, h) * dpr, Qt::KeepAspectRatioByExpanding,
                               Qt::SmoothTransformation);
        s.setDevicePixelRatio(dpr);
        const QSizeF ss = s.deviceIndependentSize();
        p.drawPixmap(QPointF((w - ss.width()) / 2.0, (h - ss.height()) / 2.0), s);
    }
    const qreal crease = qMax(6.0, w * 0.07);
    QLinearGradient spine(0, 0, crease, 0);
    spine.setColorAt(0.0, QColor(0, 0, 0, 90));
    spine.setColorAt(0.4, QColor(0, 0, 0, 18));
    spine.setColorAt(0.6, QColor(255, 255, 255, 24));
    spine.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(QRectF(0, 0, crease, h), spine);
    p.setClipping(false);
    p.setPen(QPen(QColor(255, 255, 255, 26), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
    return pm;
}

void paintCoverShadow(QPainter& p, const QRectF& r, qreal lift)
{
    p.save();
    p.setPen(Qt::NoPen);
    const int layers = 7;
    for (int i = layers; i >= 1; --i) {
        const qreal grow = i * (0.55 + lift * 0.25);
        const qreal drop = i * (1.2 + lift * 0.8);
        QPainterPath sp;
        sp.addRoundedRect(r.adjusted(-grow, drop * 0.4, grow, drop), 4 + i, 4 + i);
        p.fillPath(sp, QColor(0, 0, 0, 13));
    }
    p.restore();
}

void paintPlank(QPainter& p, const QRectF& r)
{
    const QColor base = tc(Theme::panelBorder());
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, base.lighter(118));
    g.setColorAt(1.0, base.darker(150));
    p.fillRect(r, g);
    p.fillRect(QRectF(r.left(), r.top(), r.width(), 1), QColor(255, 255, 255, 22));
    QLinearGradient sh(0, r.bottom(), 0, r.bottom() + 12);
    sh.setColorAt(0.0, QColor(0, 0, 0, 95));
    sh.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(QRectF(r.left(), r.bottom(), r.width(), 12), sh);
}

// Borrão barato e bonito: reduz, passa três caixas horizontais e verticais e
// deixa o Smooth do scaled() terminar o serviço na hora de ampliar.
QImage blurredThumb(const QPixmap& src)
{
    if (src.isNull()) return {};
    QImage img = src.toImage().scaled(72, 72, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
                     .convertToFormat(QImage::Format_ARGB32);
    const int w = img.width(), h = img.height(), r = 3;
    QImage tmp(img.size(), img.format());
    for (int pass = 0; pass < 3; ++pass) {
        for (int dir = 0; dir < 2; ++dir) {
            const QImage& in = dir == 0 ? img : tmp;
            QImage& out = dir == 0 ? tmp : img;
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    int sr = 0, sg = 0, sb = 0, n = 0;
                    for (int k = -r; k <= r; ++k) {
                        const int xx = dir == 0 ? qBound(0, x + k, w - 1) : x;
                        const int yy = dir == 0 ? y : qBound(0, y + k, h - 1);
                        const QRgb c = reinterpret_cast<const QRgb*>(in.constScanLine(yy))[xx];
                        sr += qRed(c); sg += qGreen(c); sb += qBlue(c); ++n;
                    }
                    reinterpret_cast<QRgb*>(out.scanLine(y))[x] = qRgb(sr / n, sg / n, sb / n);
                }
            }
        }
    }
    return img;
}

// Texto numa linha só que corta com "…" em vez de empurrar o layout.
class ElideLabel : public QLabel {
public:
    ElideLabel(const QString& text, const char* objectName, const QFont& font, QWidget* parent)
        : QLabel(text, parent)
    {
        setObjectName(QLatin1String(objectName));
        setFont(font);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        setFixedHeight(QFontMetrics(font).height() + 2);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.setFont(font());
        p.setPen(palette().color(foregroundRole()));
        const QRect r = contentsRect();
        p.drawText(r, int(alignment()) | Qt::TextSingleLine,
                   fontMetrics().elidedText(text(), Qt::ElideRight, r.width()));
    }
};

// A barrinha do quanto está pronto.
class Bar : public QWidget {
public:
    Bar(double value, int width, QWidget* parent) : QWidget(parent), m_value(value)
    {
        setFixedHeight(4);
        if (width > 0) setFixedWidth(width);
        else setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        const QRectF r = rect();
        p.setBrush(alpha(tc(Theme::textPrimary()), 0.13));
        p.drawRoundedRect(r, 2, 2);
        const qreal v = qBound(0.0, m_value, 1.0);
        if (v <= 0) return;
        p.setBrush(tc(Theme::accentDefault()));
        p.drawRoundedRect(QRectF(r.left(), r.top(), qMax(4.0, r.width() * v), r.height()), 2, 2);
    }
private:
    double m_value = 0;
};

// Uma capa de livro clicável. Sobe um pouco no hover (a sombra acompanha).
class Book : public QWidget {
public:
    static constexpr int kPadX = 5, kPadTop = 8, kPadBottom = 13;

    Book(const QPixmap& cover, QSize coverSize, QWidget* parent)
        : QWidget(parent), m_src(cover), m_size(coverSize)
    {
        setFixedSize(coverSize.width() + 2 * kPadX, coverSize.height() + kPadTop + kPadBottom);
        setCursor(Qt::PointingHandCursor);
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(170);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            m_lift = v.toReal();
            update();
        });
    }

    std::function<void()> onClick;
    std::function<void()> onDoubleClick;
    std::function<void(const QPoint&)> onMenu;

    // Dentro de um card, quem recebe o mouse é o card; a capa só acompanha.
    void setPassive() { setAttribute(Qt::WA_TransparentForMouseEvents, true); }
    void setHovered(bool on)
    {
        m_anim->stop();
        m_anim->setStartValue(m_lift);
        m_anim->setEndValue(on ? 1.0 : 0.0);
        m_anim->start();
    }
    void setSelected(bool on) { m_selected = on; update(); }
    void setCover(const QPixmap& pm) { m_src = pm; m_art = QPixmap(); update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        const qreal dpr = devicePixelRatioF();
        if (m_art.isNull() || !qFuzzyCompare(m_art.devicePixelRatio(), dpr))
            m_art = renderCoverArt(m_src, m_size.width(), m_size.height(), dpr);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r(kPadX, kPadTop - m_lift * 6, m_size.width(), m_size.height());
        paintCoverShadow(p, r, m_lift);
        p.drawPixmap(r.topLeft(), m_art);
        if (m_selected) {
            p.setPen(QPen(QColor(255, 255, 255, 235), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(-3.5, -3.5, 3.5, 3.5), 5, 5);
        }
    }
    void enterEvent(QEnterEvent* e) override { setHovered(true); QWidget::enterEvent(e); }
    void leaveEvent(QEvent* e) override { setHovered(false); QWidget::leaveEvent(e); }
    void mousePressEvent(QMouseEvent* e) override { e->accept(); }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && onClick)
            onClick();
    }
    void mouseDoubleClickEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && onDoubleClick) onDoubleClick();
    }
    void contextMenuEvent(QContextMenuEvent* e) override
    {
        if (onMenu) onMenu(e->globalPos());
    }

private:
    QPixmap m_src;
    QPixmap m_art;
    QSize m_size;
    qreal m_lift = 0;
    bool m_selected = false;
    QVariantAnimation* m_anim = nullptr;
};

// O fio de destaque à esquerda da citação do Onde parei. Pintado (e não
// border-left no QLabel) porque alguns temas zeram borda de QLabel.
class QuoteRule : public QWidget {
public:
    explicit QuoteRule(QWidget* parent) : QWidget(parent) { setFixedWidth(14); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(QRectF(0, 2, 2, height() - 4), tc(Theme::accentDefault()));
    }
};

// Card clicável; os filhos (textos, capa passiva) deixam o mouse passar.
class ClickFrame : public QFrame {
public:
    ClickFrame(const char* objectName, QWidget* parent) : QFrame(parent)
    {
        setObjectName(QLatin1String(objectName));
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
    }
    std::function<void()> onClick;
    std::function<void(const QPoint&)> onMenu;
    std::function<void(bool)> onHover;
protected:
    void enterEvent(QEnterEvent* e) override { if (onHover) onHover(true); QFrame::enterEvent(e); }
    void leaveEvent(QEvent* e) override { if (onHover) onHover(false); QFrame::leaveEvent(e); }
    void mousePressEvent(QMouseEvent* e) override { e->accept(); }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && onClick)
            onClick();
    }
    void contextMenuEvent(QContextMenuEvent* e) override { if (onMenu) onMenu(e->globalPos()); }
};

// Área tracejada "Novo projeto", do tamanho de uma capa.
class NewSlot : public QWidget {
public:
    NewSlot(QSize coverSize, const QString& text, QWidget* parent)
        : QWidget(parent), m_size(coverSize), m_text(text)
    {
        setFixedSize(coverSize.width() + 2 * Book::kPadX,
                     coverSize.height() + Book::kPadTop + Book::kPadBottom);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
    }
    std::function<void()> onClick;
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r(Book::kPadX + 0.75, Book::kPadTop + 0.75, m_size.width() - 1.5, m_size.height() - 1.5);
        const bool hot = underMouse();
        QPen pen(hot ? tc(Theme::accentDefault()) : tc(Theme::panelBorder()), 1.5, Qt::DashLine);
        p.setPen(pen);
        p.setBrush(hot ? alpha(tc(Theme::accentDefault()), 0.06) : Qt::transparent);
        p.drawRoundedRect(r, 4, 4);
        const QColor ink = hot ? tc(Theme::textBright()) : tc(Theme::textMuted());
        p.setPen(QPen(ink, 1.8, Qt::SolidLine, Qt::RoundCap));
        const QPointF c(r.center().x(), r.center().y() - (m_text.isEmpty() ? 0 : 10));
        p.drawLine(c + QPointF(-7, 0), c + QPointF(7, 0));
        p.drawLine(c + QPointF(0, -7), c + QPointF(0, 7));
        if (!m_text.isEmpty()) {
            p.setFont(sans(12.5));
            p.drawText(QRectF(r.left() + 6, c.y() + 14, r.width() - 12, 40),
                       Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, m_text);
        }
    }
    void enterEvent(QEnterEvent* e) override { update(); QWidget::enterEvent(e); }
    void leaveEvent(QEvent* e) override { update(); QWidget::leaveEvent(e); }
    void mousePressEvent(QMouseEvent* e) override { e->accept(); }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && onClick)
            onClick();
    }
private:
    QSize m_size;
    QString m_text;
};

QScrollArea* makeScroll(QWidget* parent)
{
    auto* s = new QScrollArea(parent);
    s->setObjectName(QStringLiteral("libScroll"));
    s->setFrameShape(QFrame::NoFrame);
    s->setWidgetResizable(true);
    s->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    s->viewport()->setAutoFillBackground(false);
    // Com seletor: um "background: transparent" solto no viewport vale pra
    // todos os filhos e ganha da folha do menu (o botão Continuar perdia a cor).
    s->viewport()->setStyleSheet(QStringLiteral("#qt_scrollarea_viewport { background: transparent; }"));
    return s;
}

QWidget* scrollBody(QScrollArea* s)
{
    auto* body = new QWidget;
    body->setObjectName(QStringLiteral("libBody"));
    body->setAttribute(Qt::WA_StyledBackground, false);
    s->setWidget(body);
    return body;
}

QString firstGenre(const QString& genres)
{
    const QStringList parts = genres.split(QLatin1Char(','), Qt::SkipEmptyParts);
    return parts.isEmpty() ? QString() : parts.first().trimmed();
}

QString metaLine(const LibraryEntry& e, bool withWhen)
{
    QStringList bits;
    const QString g = firstGenre(e.genres);
    if (!g.isEmpty()) bits << g;
    if (e.totalWords >= 0) bits << LibraryViews::wordsText(e.totalWords);
    if (withWhen && e.lastTouched.isValid()) bits << LibraryViews::relativeWhen(e.lastTouched);
    return bits.join(QStringLiteral(" · "));
}

QString bright(const QString& s)
{
    return QStringLiteral("<span style=\"color:%1; font-weight:600;\">%2</span>")
        .arg(Theme::textBright(), s.toHtmlEscaped());
}

QPushButton* goButton(const QString& text, QWidget* parent, const char* objectName = "libGo")
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(QLatin1String(objectName));
    b->setCursor(Qt::PointingHandCursor);
    b->setFont(sans(13, QFont::DemiBold));
    return b;
}

void wireBook(Book* b, const LibraryEntry& e, const LibraryHooks& hooks)
{
    const QString path = e.path;
    b->onClick = [hooks, path]() { if (hooks.open) hooks.open(path); };
    b->onMenu = [hooks, path](const QPoint& g) { if (hooks.menu) hooks.menu(path, g); };
}

void wireCard(ClickFrame* f, Book* cover, const LibraryEntry& e, const LibraryHooks& hooks)
{
    const QString path = e.path;
    f->onClick = [hooks, path]() { if (hooks.open) hooks.open(path); };
    f->onMenu = [hooks, path](const QPoint& g) { if (hooks.menu) hooks.menu(path, g); };
    if (cover) {
        cover->setPassive();
        f->onHover = [cover](bool on) { cover->setHovered(on); };
    }
}

// Texto que quebra em até N linhas e corta a última com "…" (sinopses).
class WrapLabel : public QLabel {
public:
    WrapLabel(const QString& text, int lines, const char* objectName, const QFont& font, QWidget* parent)
        : QLabel(text, parent), m_lines(lines)
    {
        setObjectName(QLatin1String(objectName));
        setFont(font);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        setFixedHeight(QFontMetrics(font).lineSpacing() * lines + 2);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.setFont(font());
        p.setPen(palette().color(foregroundRole()));
        const QString t = text();
        const QFontMetricsF fm(font());
        QTextLayout layout(t, font());
        layout.beginLayout();
        qreal y = 0;
        for (int i = 0; i < m_lines; ++i) {
            QTextLine line = layout.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(width());
            const int start = line.textStart();
            QString piece = t.mid(start, line.textLength());
            if (i == m_lines - 1 && start + line.textLength() < t.size())
                piece = fm.elidedText(t.mid(start), Qt::ElideRight, width());
            p.drawText(QPointF(0, y + fm.ascent()), piece);
            y += fm.lineSpacing();
        }
        layout.endLayout();
    }
private:
    int m_lines = 2;
};

// Fio vertical na cor da borda do tema (divisória dentro do destaque).
class VRule : public QWidget {
public:
    explicit VRule(QWidget* parent) : QWidget(parent) { setFixedWidth(1); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), tc(Theme::panelBorder()));
    }
};

// Cards de largura igual que se repartem pela largura disponível (1 a 3
// colunas), cada um com a altura fixa que já tem.
class CardFlow : public QWidget {
public:
    CardFlow(int minCellW, int gap, QWidget* parent) : QWidget(parent), m_minW(minCellW), m_gap(gap) {}
    void setCards(const QVector<QWidget*>& cards) { m_cards = cards; relayout(); }
protected:
    void resizeEvent(QResizeEvent* e) override { QWidget::resizeEvent(e); relayout(); }
private:
    void relayout()
    {
        const int w = qMax(1, width());
        const int cols = qBound(1, (w + m_gap) / (m_minW + m_gap), 3);
        const int cw = (w - m_gap * (cols - 1)) / cols;
        int y = 0, rowH = 0;
        for (int i = 0; i < m_cards.size(); ++i) {
            if (i > 0 && i % cols == 0) { y += rowH + m_gap; rowH = 0; }
            const int h = m_cards[i]->minimumHeight();
            m_cards[i]->setGeometry((i % cols) * (cw + m_gap), y, cw, h);
            rowH = qMax(rowH, h);
        }
        setMinimumHeight(m_cards.isEmpty() ? 0 : y + rowH);
    }
    int m_minW, m_gap;
    QVector<QWidget*> m_cards;
};

// Card de projeto: capa, título, gênero, palavras/quando, sinopse e a barra
// do quanto está pronto.
ClickFrame* projectCard(const LibraryEntry& e, const LibraryHooks& hooks, QWidget* parent)
{
    auto* card = new ClickFrame("libCard", parent);
    auto* row = new QHBoxLayout(card);
    row->setContentsMargins(12, 10, 16, 10);
    row->setSpacing(14);
    auto* book = new Book(e.cover, QSize(84, 122), card);
    wireCard(card, book, e, hooks);
    row->addWidget(book, 0, Qt::AlignTop);
    auto* col = new QVBoxLayout;
    col->setSpacing(2);
    col->addSpacing(Book::kPadTop - 2);
    col->addWidget(new ElideLabel(e.name, "libTitle", serif(16, QFont::DemiBold), card));
    const QString genre = firstGenre(e.genres);
    if (!genre.isEmpty()) col->addWidget(new ElideLabel(genre, "libMuted", sans(12), card));
    col->addSpacing(4);
    QStringList bits;
    if (e.totalWords >= 0) bits << LibraryViews::wordsText(e.totalWords);
    if (e.lastTouched.isValid()) bits << LibraryViews::relativeWhen(e.lastTouched);
    col->addWidget(new ElideLabel(bits.join(QStringLiteral(" · ")), "libText", sans(12), card));
    const QString syn = e.synopsis.simplified();
    if (!syn.isEmpty()) {
        col->addSpacing(6);
        col->addWidget(new WrapLabel(syn, 2, "libMuted", sans(12), card));
    }
    col->addStretch(1);
    if (e.progress() >= 0) col->addWidget(new Bar(e.progress(), 140, card), 0, Qt::AlignLeft);
    row->addLayout(col, 1);
    card->setFixedHeight(122 + Book::kPadTop + Book::kPadBottom + 20);
    return card;
}

// Grade de tiles do mesmo tamanho que reflui pela largura.
class TileGrid : public QWidget {
public:
    TileGrid(QSize cell, QWidget* parent) : QWidget(parent), m_cell(cell) {}
    void setTiles(const QVector<QWidget*>& visible, const QVector<QWidget*>& all)
    {
        for (QWidget* w : all) w->setVisible(visible.contains(w));
        m_tiles = visible;
        relayout();
    }
protected:
    void resizeEvent(QResizeEvent* e) override { QWidget::resizeEvent(e); relayout(); }
private:
    void relayout()
    {
        const int gapMin = 18, gapY = 22;
        const int w = qMax(width(), m_cell.width());
        const int cols = qMax(1, (w + gapMin) / (m_cell.width() + gapMin));
        const int gapX = cols > 1 ? qMin(44, (w - cols * m_cell.width()) / (cols - 1)) : 0;
        for (int i = 0; i < m_tiles.size(); ++i) {
            const int r = i / cols, c = i % cols;
            m_tiles[i]->setGeometry(c * (m_cell.width() + gapX), r * (m_cell.height() + gapY),
                                    m_cell.width(), m_cell.height());
        }
        const int rows = (m_tiles.size() + cols - 1) / cols;
        setMinimumHeight(qMax(0, rows * (m_cell.height() + gapY) - gapY));
    }
    QSize m_cell;
    QVector<QWidget*> m_tiles;
};

// ---- Seu dia -------------------------------------------------------------

class GoalRing : public QWidget {
public:
    GoalRing(const WritingDay& d, QWidget* parent) : QWidget(parent), m_day(d) { setFixedSize(136, 136); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = QRectF(rect()).adjusted(8, 8, -8, -8);
        p.setPen(QPen(alpha(tc(Theme::textPrimary()), 0.12), 11));
        p.drawEllipse(r);
        const qreal v = m_day.target > 0 ? qBound(0.0, double(m_day.done) / m_day.target, 1.0) : 0.0;
        if (v > 0) {
            p.setPen(QPen(tc(Theme::accentDefault()), 11, Qt::SolidLine, Qt::RoundCap));
            p.drawArc(r, 90 * 16, -int(v * 360 * 16));
        }
        QFont num(QStringLiteral("IBM Plex Mono"));
        num.setPixelSize(22);
        num.setWeight(QFont::Medium);
        p.setFont(num);
        p.setPen(tc(Theme::textBright()));
        const QLocale loc;
        p.drawText(QRectF(0, height() / 2.0 - 22, width(), 28), Qt::AlignCenter,
                   loc.toString(m_day.done));
        p.setFont(sans(11));
        p.setPen(tc(Theme::textMuted()));
        const QString of = m_day.timeGoal
            ? LibraryViews::tr("de %1 min").arg(loc.toString(m_day.target))
            : LibraryViews::tr("de %1").arg(loc.toString(m_day.target));
        p.drawText(QRectF(0, height() / 2.0 + 6, width(), 18), Qt::AlignCenter, of);
    }
private:
    WritingDay m_day;
};

class WeekBars : public QWidget {
public:
    WeekBars(const WritingDay& d, QWidget* parent) : QWidget(parent), m_day(d)
    {
        setFixedHeight(82);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const int n = m_day.week.size();
        if (n == 0) return;
        int top = qMax(1, m_day.target);
        for (int v : m_day.week) top = qMax(top, v);
        const qreal gap = 7, barArea = 60;
        const qreal bw = (width() - gap * (n - 1)) / n;
        const QLocale loc;
        for (int i = 0; i < n; ++i) {
            const qreal x = i * (bw + gap);
            const int v = m_day.week.at(i);
            const qreal h = v > 0 ? qMax(4.0, barArea * v / top) : 3.0;
            const bool today = (i == n - 1);
            p.setPen(Qt::NoPen);
            p.setBrush(today ? tc(Theme::accentDefault())
                             : alpha(tc(Theme::textPrimary()), v > 0 ? 0.24 : 0.10));
            p.drawRoundedRect(QRectF(x, barArea - h, bw, h), 3, 3);
            if (m_day.target > 0 && v >= m_day.target && !today) {
                p.setBrush(tc(Theme::accentDefault()));
                p.drawEllipse(QPointF(x + bw / 2, barArea - h - 5), 1.8, 1.8);
            }
            p.setFont(sans(10, today ? QFont::DemiBold : QFont::Normal));
            p.setPen(today ? tc(Theme::textBright()) : tc(Theme::textMuted()));
            const QDate d = m_day.weekDays.value(i);
            p.drawText(QRectF(x - 4, barArea + 6, bw + 8, 16), Qt::AlignCenter,
                       d.isValid() ? loc.dayName(d.dayOfWeek(), QLocale::NarrowFormat).toUpper() : QString());
        }
    }
private:
    WritingDay m_day;
};

QHBoxLayout* streakRow(const WritingDay& day, QWidget* parent)
{
    auto* sr = new QHBoxLayout;
    sr->setSpacing(6);
    sr->addStretch(1);
    auto* icon = new QLabel(parent);
    const QColor a = tc(Theme::accentDefault());
    icon->setPixmap(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/stats-streak.svg"), a, a, a, QSize(16, 16))
                        .pixmap(QSize(16, 16)));
    sr->addWidget(icon);
    const QString streak = day.streak == 1
        ? LibraryViews::tr("%1 dia seguido").arg(bright(QStringLiteral("1")))
        : LibraryViews::tr("%1 dias seguidos").arg(bright(QLocale().toString(day.streak)));
    sr->addWidget(label(streak, "libText", sans(13), parent));
    sr->addStretch(1);
    return sr;
}

// O destaque do Continuar: a coluna "Seu dia" à direita só cabe em janela
// larga; estreita, ela some e entra uma linha curta com os mesmos números.
class HeroFrame : public QFrame {
public:
    explicit HeroFrame(QWidget* parent) : QFrame(parent) { setObjectName(QStringLiteral("libHero")); }
    QWidget* side = nullptr;
    QWidget* compact = nullptr;
protected:
    void resizeEvent(QResizeEvent* e) override
    {
        QFrame::resizeEvent(e);
        const bool wide = width() >= 900;
        if (side) side->setVisible(wide);
        if (compact) compact->setVisible(!wide);
    }
};

// ---- Cinema --------------------------------------------------------------

class HScroll : public QScrollArea {
public:
    using QScrollArea::QScrollArea;
protected:
    void wheelEvent(QWheelEvent* e) override
    {
        const int d = e->angleDelta().y() != 0 ? e->angleDelta().y() : e->angleDelta().x();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d);
        e->accept();
    }
};

class CinemaStage : public QWidget {
public:
    CinemaStage(const QVector<LibraryEntry>& entries, const LibraryHooks& hooks, QWidget* parent)
        : QWidget(parent), m_entries(entries), m_hooks(hooks)
    {
        setObjectName(QStringLiteral("libCinema"));
        auto* col = new QVBoxLayout(this);
        col->setContentsMargins(44, 52, 30, 24);
        col->setSpacing(0);

        m_kick = kick(QString(), this, "libCineKick");
        col->addWidget(m_kick);
        col->addSpacing(12);
        m_title = label(QString(), "libCineTitle", serif(44, QFont::DemiBold), this);
        m_title->setWordWrap(true);
        m_title->setMaximumWidth(600);
        col->addWidget(m_title);
        col->addSpacing(14);
        m_synopsis = label(QString(), "libCineText", sans(14), this);
        m_synopsis->setWordWrap(true);
        m_synopsis->setMaximumWidth(540);
        m_synopsis->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        col->addWidget(m_synopsis);
        col->addSpacing(14);
        m_where = label(QString(), "libCineKickSoft", sans(12.5), this);
        m_where->setWordWrap(true);
        m_where->setMaximumWidth(540);
        col->addWidget(m_where);
        col->addSpacing(18);
        auto* btns = new QHBoxLayout;
        btns->setSpacing(10);
        auto* go = goButton(LibraryViews::tr("Continuar  →"), this);
        auto* more = goButton(LibraryViews::tr("Detalhes"), this, "libCineGhost");
        more->setFont(sans(13));
        connect(go, &QPushButton::clicked, this, [this]() {
            if (m_hooks.resume) m_hooks.resume(m_entries.at(m_sel).path);
        });
        connect(more, &QPushButton::clicked, this, [this]() {
            if (m_hooks.details) m_hooks.details(m_entries.at(m_sel).path);
        });
        btns->addWidget(go);
        btns->addWidget(more);
        btns->addStretch(1);
        col->addLayout(btns);
        col->addStretch(1);

        col->addWidget(kick(LibraryViews::tr("Seus projetos"), this, "libCineKick"));
        col->addSpacing(8);
        auto* scroll = new HScroll(this);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setWidgetResizable(false);
        scroll->viewport()->setAutoFillBackground(false);
        scroll->viewport()->setStyleSheet(QStringLiteral("#qt_scrollarea_viewport { background: transparent; }"));
        scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
        auto* strip = new QWidget;
        strip->setAttribute(Qt::WA_TranslucentBackground, true);
        auto* row = new QHBoxLayout(strip);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(10);
        for (int i = 0; i < m_entries.size(); ++i) {
            auto* b = new Book(m_entries.at(i).cover, QSize(96, 140), strip);
            const QString path = m_entries.at(i).path;
            b->onClick = [this, i]() { select(i); };
            b->onDoubleClick = [this, path]() { if (m_hooks.open) m_hooks.open(path); };
            b->onMenu = [this, path](const QPoint& g) { if (m_hooks.menu) m_hooks.menu(path, g); };
            b->setToolTip(m_entries.at(i).name);
            row->addWidget(b);
            m_books.append(b);
        }
        strip->adjustSize();
        scroll->setWidget(strip);
        // setWidget liga o autoFill: sem isto, fica um retângulo escuro atrás das capas.
        strip->setAutoFillBackground(false);
        scroll->setFixedHeight(strip->sizeHint().height());
        col->addWidget(scroll);

        m_fade = new QVariantAnimation(this);
        m_fade->setDuration(380);
        m_fade->setEasingCurve(QEasingCurve::InOutCubic);
        connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            m_mix = v.toReal();
            update();
        });
        select(0, false);
    }

protected:
    void resizeEvent(QResizeEvent* e) override
    {
        QWidget::resizeEvent(e);
        m_bgScaled = QPixmap();
        m_prevScaled = QPixmap();
        fitText();
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QRectF r = rect();
        QPainterPath clip;
        clip.addRoundedRect(r, Theme::panelRadius() + 4, Theme::panelRadius() + 4);
        p.setClipPath(clip);
        p.fillRect(r, QColor(16, 14, 20));
        if (m_bgScaled.isNull()) m_bgScaled = scaledBg(m_bg);
        if (m_prevScaled.isNull() && !m_prevBg.isNull()) m_prevScaled = scaledBg(m_prevBg);
        if (m_mix < 1.0 && !m_prevScaled.isNull()) {
            p.drawPixmap(0, 0, m_prevScaled);
            p.setOpacity(m_mix);
        }
        p.drawPixmap(0, 0, m_bgScaled);
        p.setOpacity(1.0);

        QRadialGradient glow(QPointF(r.width() * 0.72, r.height() * 0.28), r.width() * 0.6);
        glow.setColorAt(0.0, alpha(tc(Theme::accentDefault()), 0.20));
        glow.setColorAt(1.0, alpha(tc(Theme::accentDefault()), 0.0));
        p.fillRect(r, glow);
        QLinearGradient side(0, 0, r.width(), 0);
        side.setColorAt(0.0, QColor(16, 14, 20, 240));
        side.setColorAt(0.38, QColor(16, 14, 20, 225));
        side.setColorAt(1.0, QColor(16, 14, 20, 120));
        p.fillRect(r, side);
        QLinearGradient bottom(0, r.height(), 0, r.height() * 0.5);
        bottom.setColorAt(0.0, QColor(16, 14, 20, 245));
        bottom.setColorAt(1.0, QColor(16, 14, 20, 0));
        p.fillRect(r, bottom);

        // Com capa própria, ela aparece grande à direita, nítida, por cima do
        // próprio borrão. A capa gerada (nome sobre gradiente) fica só no fundo.
        const QRect cr = coverRect();
        const qreal dpr = devicePixelRatioF();
        const auto art = [&](const QPixmap& src, QPixmap& cache) -> const QPixmap& {
            if (cache.isNull() || cache.deviceIndependentSize().toSize() != cr.size()
                || !qFuzzyCompare(cache.devicePixelRatio(), dpr))
                cache = renderCoverArt(src, cr.width(), cr.height(), dpr);
            return cache;
        };
        if (m_mix < 1.0 && m_prevHasCover) {
            p.setOpacity(1.0 - m_mix);
            paintCoverShadow(p, cr, 0.8);
            p.drawPixmap(cr.topLeft(), art(m_prevCover, m_prevArt));
        }
        if (m_hasCover) {
            p.setOpacity(m_mix);
            paintCoverShadow(p, cr, 0.8);
            p.drawPixmap(cr.topLeft(), art(m_cover, m_art));
        }
        p.setOpacity(1.0);
    }

private:
    QRect coverRect() const
    {
        qreal h = qMin(height() * 0.56, 480.0);
        qreal w = h * 2.0 / 3.0;
        const qreal maxW = width() * 0.34;
        if (w > maxW) { w = maxW; h = w * 1.5; }
        return QRect(qRound(width() - w - 64), 56, qRound(w), qRound(h));
    }

    // Os textos não passam por baixo da capa; a sinopse corta em 4 linhas.
    void fitText()
    {
        const int avail = m_hasCover ? coverRect().left() - 44 - 56 : width() - 44 - 60;
        m_title->setMaximumWidth(qMax(220, qMin(avail, 640)));
        const int tw = qMax(220, qMin(avail, 560));
        m_synopsis->setMaximumWidth(tw);
        m_where->setMaximumWidth(tw);
        QString syn = m_synFull;
        const QFontMetrics fm(m_synopsis->font());
        const int maxLines = 4;
        const auto tooTall = [&](const QString& t) {
            return fm.boundingRect(QRect(0, 0, tw, 10000), Qt::TextWordWrap, t).height() > fm.lineSpacing() * maxLines;
        };
        if (tooTall(syn)) {
            while (syn.size() > 20 && tooTall(syn + QStringLiteral("…"))) syn.chop(qMax(1, syn.size() / 20));
            syn = syn.trimmed() + QStringLiteral("…");
        }
        m_synopsis->setText(syn);
        m_synopsis->setVisible(!syn.isEmpty());
    }

    QPixmap scaledBg(const QImage& img) const
    {
        if (img.isNull() || width() <= 0 || height() <= 0) return {};
        const qreal dpr = devicePixelRatioF();
        QPixmap pm = QPixmap::fromImage(img.scaled(size() * dpr, Qt::KeepAspectRatioByExpanding,
                                                   Qt::SmoothTransformation));
        const QSize target = size() * dpr;
        pm = pm.copy((pm.width() - target.width()) / 2, (pm.height() - target.height()) / 4,
                     target.width(), target.height());
        pm.setDevicePixelRatio(dpr);
        return pm;
    }

    void select(int i, bool animate = true)
    {
        if (i < 0 || i >= m_entries.size()) return;
        const LibraryEntry& e = m_entries.at(i);
        m_sel = i;
        for (int k = 0; k < m_books.size(); ++k) m_books[k]->setSelected(k == i);

        QStringList bits;
        if (e.manuscriptCount > 1) bits << LibraryViews::tr("Saga · %1 livros").arg(e.manuscriptCount);
        if (!e.genres.trimmed().isEmpty()) bits << e.genres.trimmed();
        if (e.totalWords >= 0) bits << LibraryViews::wordsText(e.totalWords);
        m_kick->setText(bits.join(QStringLiteral(" · ")).toUpper());
        m_title->setText(e.name);
        m_prevCover = m_cover;
        m_prevHasCover = m_hasCover;
        m_prevArt = m_art;
        m_cover = e.cover;
        m_hasCover = e.hasOwnCover;
        m_art = QPixmap();
        m_synFull = e.synopsis.simplified();
        fitText();
        QString where;
        if (!e.resumeWhere.isEmpty())
            where = LibraryViews::tr("Você parou em %1").arg(e.resumeWhere);
        if (e.lastTouched.isValid())
            where += (where.isEmpty() ? QString() : QStringLiteral(" · ")) + LibraryViews::relativeWhen(e.lastTouched);
        m_where->setText(where);
        m_where->setVisible(!where.isEmpty());

        m_prevBg = m_bg;
        m_prevScaled = m_bgScaled;
        m_bg = blurredThumb(e.cover);
        m_bgScaled = QPixmap();
        if (animate && !m_prevBg.isNull()) {
            m_fade->stop();
            m_fade->setStartValue(0.0);
            m_fade->setEndValue(1.0);
            m_fade->start();
        } else {
            m_mix = 1.0;
        }
        update();
    }

    QVector<LibraryEntry> m_entries;
    LibraryHooks m_hooks;
    QVector<Book*> m_books;
    QLabel* m_kick = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_synopsis = nullptr;
    QLabel* m_where = nullptr;
    QImage m_bg, m_prevBg;
    QPixmap m_bgScaled, m_prevScaled;
    QPixmap m_cover, m_prevCover, m_art, m_prevArt;
    bool m_hasCover = false, m_prevHasCover = false;
    QString m_synFull;
    QVariantAnimation* m_fade = nullptr;
    qreal m_mix = 1.0;
    int m_sel = 0;
};

// ---- Estante -------------------------------------------------------------

class ShelfWall : public QWidget {
public:
    ShelfWall(const QVector<LibraryEntry>& books, const QString& newText,
              const LibraryHooks& hooks, QWidget* parent)
        : QWidget(parent), m_hooks(hooks), m_newText(newText)
    {
        for (const auto& e : books) {
            Item it;
            it.path = e.path;
            it.name = e.name;
            it.cover = e.cover;
            QStringList bits;
            if (e.totalWords >= 0) bits << LibraryViews::wordsText(e.totalWords);
            if (e.lastTouched.isValid()) bits << LibraryViews::relativeWhen(e.lastTouched);
            it.caption = bits.join(QStringLiteral(" · "));
            m_items.append(it);
        }
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        m_tick = new QTimer(this);
        m_tick->setInterval(16);
        connect(m_tick, &QTimer::timeout, this, [this]() {
            bool moving = false;
            for (int i = 0; i < m_items.size(); ++i) {
                const qreal target = (i == m_hover) ? 1.0 : 0.0;
                qreal& l = m_items[i].lift;
                l += (target - l) * 0.28;
                if (std::abs(target - l) < 0.01) l = target;
                else moving = true;
            }
            if (!moving) m_tick->stop();
            update();
        });
    }

protected:
    void resizeEvent(QResizeEvent* e) override
    {
        QWidget::resizeEvent(e);
        const int rows = (slotCount() + cols() - 1) / cols();
        setMinimumHeight(rows * kRowH);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const qreal dpr = devicePixelRatioF();
        const int n = slotCount(), c = cols();
        const int rows = (n + c - 1) / c;
        for (int r = 0; r < rows; ++r)
            paintPlank(p, QRectF(0, r * kRowH + kHead + kBookH, width(), kPlank));
        for (int i = 0; i < n; ++i) {
            const QRectF br = bookRect(i);
            if (i == m_items.size()) {
                const bool hot = (m_hover == i);
                p.setPen(QPen(hot ? tc(Theme::accentDefault()) : tc(Theme::panelBorder()), 1.5, Qt::DashLine));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(br.adjusted(0.75, 0.75, -0.75, -0.75), 4, 4);
                p.setPen(QPen(hot ? tc(Theme::textBright()) : tc(Theme::textMuted()), 1.8, Qt::SolidLine, Qt::RoundCap));
                const QPointF ctr = br.center();
                p.drawLine(ctr + QPointF(-7, 0), ctr + QPointF(7, 0));
                p.drawLine(ctr + QPointF(0, -7), ctr + QPointF(0, 7));
                continue;
            }
            Item& it = m_items[i];
            if (it.art.isNull() || !qFuzzyCompare(it.art.devicePixelRatio(), dpr))
                it.art = renderCoverArt(it.cover, kBookW, kBookH, dpr);
            const QRectF lifted = br.translated(0, -it.lift * 14);
            paintCoverShadow(p, lifted, it.lift);
            p.drawPixmap(lifted.topLeft(), it.art);
        }
        if (m_hover >= 0 && m_hover < m_items.size()) {
            const Item& it = m_items.at(m_hover);
            const QRectF br = bookRect(m_hover);
            const qreal y = br.bottom() + kPlank + 8;
            QFont nf = serif(14.5, QFont::DemiBold);
            QFont cf = sans(12);
            const QString name = QFontMetrics(nf).elidedText(it.name, Qt::ElideRight, 260);
            const qreal w = qMax(QFontMetrics(nf).horizontalAdvance(name),
                                 QFontMetrics(cf).horizontalAdvance(it.caption)) + 4;
            qreal x = br.center().x() - w / 2;
            x = qBound(0.0, x, width() - w);
            p.setOpacity(it.lift);
            p.setFont(nf);
            p.setPen(tc(Theme::textBright()));
            p.drawText(QRectF(x, y, w, 20), Qt::AlignHCenter | Qt::AlignVCenter, name);
            p.setFont(cf);
            p.setPen(tc(Theme::textMuted()));
            p.drawText(QRectF(x - 40, y + 20, w + 80, 18), Qt::AlignHCenter | Qt::AlignVCenter, it.caption);
            p.setOpacity(1.0);
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        const int h = hitTest(e->position());
        if (h != m_hover) {
            m_hover = h;
            setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
            if (!m_tick->isActive()) m_tick->start();
            update();
        }
    }
    void leaveEvent(QEvent* e) override
    {
        m_hover = -1;
        if (!m_tick->isActive()) m_tick->start();
        update();
        QWidget::leaveEvent(e);
    }
    void mousePressEvent(QMouseEvent* e) override { e->accept(); }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        const int h = hitTest(e->position());
        if (h < 0) return;
        if (h == m_items.size()) { if (m_hooks.newProject) m_hooks.newProject(); return; }
        if (m_hooks.open) m_hooks.open(m_items.at(h).path);
    }
    void contextMenuEvent(QContextMenuEvent* e) override
    {
        const int h = hitTest(e->pos());
        if (h >= 0 && h < m_items.size() && m_hooks.menu) m_hooks.menu(m_items.at(h).path, e->globalPos());
    }

private:
    static constexpr int kBookW = 150, kBookH = 218, kGap = 30, kHead = 30, kPlank = 16, kBelow = 52;
    static constexpr int kRowH = kHead + kBookH + kPlank + kBelow;
    static constexpr int kSide = 14;

    struct Item {
        QString path, name, caption;
        QPixmap cover, art;
        qreal lift = 0;
    };

    int slotCount() const { return m_items.size() + 1; }
    int cols() const { return qMax(1, (width() - 2 * kSide + kGap) / (kBookW + kGap)); }
    QRectF bookRect(int i) const
    {
        const int c = cols();
        const int r = i / c, k = i % c;
        return QRectF(kSide + k * (kBookW + kGap), r * kRowH + kHead, kBookW, kBookH);
    }
    int hitTest(const QPointF& pos) const
    {
        for (int i = 0; i < slotCount(); ++i)
            if (bookRect(i).adjusted(0, -12, 0, 0).contains(pos)) return i;
        return -1;
    }

    LibraryHooks m_hooks;
    QString m_newText;
    QVector<Item> m_items;
    int m_hover = -1;
    QTimer* m_tick = nullptr;
};

// Faixa com uma prateleira no pé: o livro do destaque fica em pé nela (a
// sombra da capa cai por cima da tábua, como cairia de verdade).
class PlankHost : public QWidget {
public:
    static constexpr int kPlank = 14, kShadow = 12;
    explicit PlankHost(QWidget* parent) : QWidget(parent) {}
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        paintPlank(p, QRectF(0, height() - kPlank - kShadow, width(), kPlank));
    }
};

// ---- Primeira vez --------------------------------------------------------

class ThemeSwatch : public QWidget {
public:
    ThemeSwatch(const Theme::MiraTheme& t, QWidget* parent) : QWidget(parent), m_id(t.id)
    {
        m_bg = tc(t.appBackground);
        m_panel = tc(t.panelBackground);
        m_accent = tc(t.accentDefault);
        setFixedSize(26, 26);
        setCursor(Qt::PointingHandCursor);
        setToolTip(t.name);
        connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this,
                QOverload<>::of(&QWidget::update));
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const bool on = Theme::Manager::instance()->current().id == m_id;
        const QRectF r = QRectF(rect()).adjusted(2, 2, -2, -2);
        QPainterPath path;
        path.addRoundedRect(r, 6, 6);
        p.fillPath(path, m_bg);
        p.save();
        p.setClipPath(path);
        p.fillRect(QRectF(r.left(), r.top(), r.width() * 0.38, r.height()), m_panel);
        p.restore();
        p.setPen(Qt::NoPen);
        p.setBrush(m_accent);
        p.drawEllipse(QPointF(r.right() - 6, r.bottom() - 6), 3, 3);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(on ? tc(Theme::textBright()) : alpha(tc(Theme::textPrimary()), 0.22), on ? 2 : 1));
        p.drawRoundedRect(r, 6, 6);
    }
    void mousePressEvent(QMouseEvent* e) override { e->accept(); }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()))
            Theme::Manager::instance()->setCurrent(m_id);
    }
private:
    QString m_id;
    QColor m_bg, m_panel, m_accent;
};

} // namespace

// ---- Dados ---------------------------------------------------------------

WritingDay WritingDay::fromSettings(const WordCounterSettings& s, const QString& scopeName)
{
    // Réplica, em leitura, da janela rolante de 24h e do streak do
    // WordCounter (ver currentWindowStart/currentStreak lá): o menu não
    // carrega projeto, só lê o que ficou gravado.
    WritingDay d;
    d.valid = true;
    d.scopeName = scopeName;
    d.timeGoal = (s.goalType == QStringLiteral("time"));
    d.target = d.timeGoal ? s.goalTargetMinutes : s.goalTargetWords;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 start = s.goalDayStartAt;
    if (start > 0 && now - start >= kGoalDayMs) start += ((now - start) / kGoalDayMs) * kGoalDayMs;
    const QDate today = start > 0 ? QDateTime::fromMSecsSinceEpoch(start).date() : QDate::currentDate();
    const auto key = [](const QDate& dt) { return dt.toString(QStringLiteral("yyyy-MM-dd")); };
    const auto valueOf = [&](const QString& k) {
        const QJsonObject o = s.progress.value(k).toObject();
        return d.timeGoal ? int(o.value(QStringLiteral("timeMs")).toDouble(0) / 60000.0)
                          : o.value(QStringLiteral("words")).toInt(0);
    };
    const auto met = [&](const QString& k) {
        const QJsonObject o = s.progress.value(k).toObject();
        if (o.isEmpty()) return false;
        const QString type = o.value(QStringLiteral("goalType")).toString(s.goalType);
        if (type == QStringLiteral("time")) {
            const int t = o.value(QStringLiteral("goalTargetMinutes")).toInt(s.goalTargetMinutes);
            return t > 0 && o.value(QStringLiteral("timeMs")).toDouble(0) >= t * 60000.0;
        }
        const int t = o.value(QStringLiteral("goalTargetWords")).toInt(s.goalTargetWords);
        return t > 0 && o.value(QStringLiteral("words")).toInt(0) >= t;
    };

    d.done = valueOf(key(today));
    const bool todayCounts = met(key(today)) || s.offDays.contains(key(today));
    for (int i = todayCounts ? 0 : 1; i < 3650; ++i) {
        const QString k = key(today.addDays(-i));
        if (s.offDays.contains(k)) continue;
        if (!met(k)) break;
        ++d.streak;
    }
    for (int i = 6; i >= 0; --i) {
        const QDate dt = today.addDays(-i);
        d.week.append(valueOf(key(dt)));
        d.weekDays.append(dt);
    }
    return d;
}

QString LibraryViews::relativeWhen(const QDateTime& when)
{
    if (!when.isValid()) return QString();
    const QDateTime now = QDateTime::currentDateTime();
    const qint64 secs = when.secsTo(now);
    if (secs < 90) return tr("agora há pouco");
    const qint64 mins = secs / 60;
    if (mins < 60) return mins == 1 ? tr("há 1 minuto") : tr("há %1 minutos").arg(mins);
    const qint64 hours = mins / 60;
    if (hours < 24) return hours == 1 ? tr("há 1 hora") : tr("há %1 horas").arg(hours);
    const qint64 days = when.date().daysTo(now.date());
    if (days <= 1) return tr("ontem");
    if (days < 7) return tr("há %1 dias").arg(days);
    if (days < 30) return days < 14 ? tr("há 1 semana") : tr("há %1 semanas").arg(days / 7);
    if (days < 365) return days < 60 ? tr("há 1 mês") : tr("há %1 meses").arg(days / 30);
    return days < 730 ? tr("há 1 ano") : tr("há %1 anos").arg(days / 365);
}

QString LibraryViews::wordsText(int words)
{
    if (words < 0) return QString();
    return words == 1 ? tr("1 palavra") : tr("%1 palavras").arg(QLocale().toString(words));
}

// ---- M1 Continuar --------------------------------------------------------

QWidget* LibraryViews::continueView(const QVector<LibraryEntry>& entries, const WritingDay& day,
                                    const LibraryHooks& hooks, QWidget* parent)
{
    auto* scroll = makeScroll(parent);
    QWidget* body = scrollBody(scroll);
    auto* col = new QVBoxLayout(body);
    col->setContentsMargins(0, 2, 10, 16);
    col->setSpacing(0);
    const LibraryEntry& e = entries.first();
    const QLocale loc;

    auto* hero = new HeroFrame(body);
    auto* hl = new QHBoxLayout(hero);
    hl->setContentsMargins(22, 20, 26, 22);
    hl->setSpacing(26);
    auto* book = new Book(e.cover, QSize(200, 296), hero);
    wireBook(book, e, hooks);
    hl->addWidget(book, 0, Qt::AlignTop);

    auto* info = new QVBoxLayout;
    info->setSpacing(0);
    info->addSpacing(6);
    const QString head = e.resumeWhere.isEmpty() ? tr("Último projeto") : tr("Continuar de onde parou");
    info->addWidget(kick(e.lastTouched.isValid() ? head + QStringLiteral(" · ") + relativeWhen(e.lastTouched) : head, hero));
    info->addSpacing(8);
    auto* title = label(e.name, "libTitle", serif(30, QFont::DemiBold), hero);
    title->setWordWrap(true);
    info->addWidget(title);
    if (!e.resumeWhere.isEmpty()) {
        info->addSpacing(4);
        info->addWidget(label(e.resumeWhere, "libText", sans(14), hero));
    }
    if (!e.resumeSentence.trimmed().isEmpty()) {
        info->addSpacing(14);
        auto* qrow = new QHBoxLayout;
        qrow->setSpacing(0);
        qrow->addWidget(new QuoteRule(hero));
        auto* quote = label(QStringLiteral("\u201C%1\u201D").arg(e.resumeSentence.trimmed()), "libQuote",
                            serif(16, QFont::Normal, true), hero);
        quote->setWordWrap(true);
        qrow->addWidget(quote, 1);
        info->addLayout(qrow);
    }
    QStringList facts;
    if (!e.genres.trimmed().isEmpty()) facts << e.genres.trimmed();
    if (e.totalWords >= 0) facts << wordsText(e.totalWords);
    if (e.manuscriptCount > 1) facts << tr("Saga · %1 livros").arg(e.manuscriptCount);
    if (!facts.isEmpty()) {
        info->addSpacing(16);
        info->addWidget(new ElideLabel(facts.join(QStringLiteral(" · ")), "libMuted", sans(12.5), hero));
    }
    const QString syn = e.synopsis.simplified();
    if (!syn.isEmpty()) {
        info->addSpacing(8);
        info->addWidget(new WrapLabel(syn, 3, "libText", sans(13.5), hero));
    }
    if (day.valid) {
        // Janela estreita: a coluna "Seu dia" some e os números vêm aqui.
        QStringList bits;
        bits << (day.timeGoal ? tr("%1 min hoje").arg(bright(loc.toString(day.done)))
                              : tr("%1 palavras hoje").arg(bright(loc.toString(day.done))));
        bits << (day.streak == 1 ? tr("%1 dia seguido").arg(bright(QStringLiteral("1")))
                                 : tr("%1 dias seguidos").arg(bright(loc.toString(day.streak))));
        if (day.target > 0)
            bits << (day.done >= day.target ? tr("Meta de hoje batida")
                                             : tr("%1% da meta de hoje").arg(qMin(100, qRound(100.0 * day.done / day.target))));
        auto* compact = label(bits.join(QStringLiteral(" · ")), "libMuted", sans(12.5), hero);
        info->addSpacing(12);
        info->addWidget(compact);
        hero->compact = compact;
    }
    info->addStretch(1);
    info->addSpacing(16);
    auto* btns = new QHBoxLayout;
    btns->setSpacing(10);
    auto* go = goButton(tr("Continuar  →"), hero);
    auto* openBtn = goButton(tr("Abrir projeto"), hero, "libGhost");
    openBtn->setFont(sans(13));
    const QString path = e.path;
    QObject::connect(go, &QPushButton::clicked, hero, [hooks, path]() { if (hooks.resume) hooks.resume(path); });
    QObject::connect(openBtn, &QPushButton::clicked, hero, [hooks, path]() { if (hooks.open) hooks.open(path); });
    btns->addWidget(go);
    btns->addWidget(openBtn);
    btns->addStretch(1);
    info->addLayout(btns);
    hl->addLayout(info, 1);

    if (day.valid) {
        auto* side = new QWidget(hero);
        auto* sl = new QHBoxLayout(side);
        sl->setContentsMargins(0, 0, 0, 0);
        sl->setSpacing(26);
        sl->addWidget(new VRule(side));
        auto* dcol = new QVBoxLayout;
        dcol->setSpacing(0);
        dcol->addSpacing(6);
        dcol->addWidget(kick(tr("Seu dia"), side));
        if (!day.scopeName.isEmpty() && day.scopeName != e.name) {
            dcol->addSpacing(4);
            dcol->addWidget(new ElideLabel(tr("Meta de %1").arg(day.scopeName), "libMuted", sans(11), side));
        }
        dcol->addSpacing(10);
        dcol->addWidget(new GoalRing(day, side), 0, Qt::AlignHCenter);
        dcol->addSpacing(10);
        dcol->addLayout(streakRow(day, side));
        dcol->addSpacing(16);
        dcol->addWidget(kick(tr("Últimos 7 dias"), side));
        dcol->addSpacing(8);
        dcol->addWidget(new WeekBars(day, side));
        dcol->addStretch(1);
        sl->addLayout(dcol, 1);
        side->setFixedWidth(250);
        hl->addWidget(side);
        hero->side = side;
    }
    col->addWidget(hero);

    if (entries.size() > 1) {
        col->addSpacing(26);
        col->addWidget(kick(tr("Outros projetos"), body));
        col->addSpacing(12);
        auto* flow = new CardFlow(380, 14, body);
        QVector<QWidget*> cards;
        for (int i = 1; i < entries.size(); ++i) cards.append(projectCard(entries.at(i), hooks, flow));
        flow->setCards(cards);
        col->addWidget(flow);
    }
    col->addStretch(1);
    return scroll;
}

// ---- M2 Vitrine ----------------------------------------------------------

QWidget* LibraryViews::vitrineView(const QVector<LibraryEntry>& entries,
                                   const LibraryHooks& hooks, QWidget* parent)
{
    auto* root = new QWidget(parent);
    auto* col = new QVBoxLayout(root);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(14);

    auto* bar = new QHBoxLayout;
    bar->setSpacing(8);
    auto* search = new QLineEdit(root);
    search->setObjectName(QStringLiteral("libSearch"));
    search->setPlaceholderText(tr("Buscar por nome, autor ou gênero"));
    search->setClearButtonEnabled(true);
    search->setFixedWidth(260);
    {
        const QColor m = tc(Theme::textMuted());
        search->addAction(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/search.svg"), m, m, m, QSize(14, 14)),
                          QLineEdit::LeadingPosition);
    }
    auto* sort = new QComboBox(root);
    sort->setObjectName(QStringLiteral("libSort"));
    sort->setCursor(Qt::PointingHandCursor);
    sort->addItem(tr("Recentes"));
    sort->addItem(tr("A–Z"));
    sort->addItem(tr("Mais palavras"));
    {
        QSettings qs;
        sort->setCurrentIndex(qBound(0, qs.value(QStringLiteral("library/vitrineSort"), 0).toInt(), 2));
    }
    bar->addStretch(1);
    bar->addWidget(search);
    bar->addWidget(sort);
    col->addLayout(bar);

    auto* scroll = makeScroll(root);
    QWidget* body = scrollBody(scroll);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(0, 0, 10, 16);
    const QSize cell(160 + 2 * Book::kPadX, 232 + Book::kPadTop + Book::kPadBottom + 62);
    auto* grid = new TileGrid(cell, body);
    bl->addWidget(grid);
    bl->addStretch(1);
    col->addWidget(scroll, 1);

    QVector<QWidget*> tiles;
    for (const auto& e : entries) {
        auto* tile = new ClickFrame("libTile", grid);
        auto* tc_ = new QVBoxLayout(tile);
        tc_->setContentsMargins(0, 0, 0, 0);
        tc_->setSpacing(2);
        auto* book = new Book(e.cover, QSize(160, 232), tile);
        wireCard(tile, book, e, hooks);
        tc_->addWidget(book);
        tc_->addSpacing(2);
        auto* t = new ElideLabel(e.name, "libTitle", serif(14, QFont::DemiBold), tile);
        t->setContentsMargins(Book::kPadX, 0, Book::kPadX, 0);
        tc_->addWidget(t);
        auto* m = new ElideLabel(metaLine(e, false), "libMuted", sans(11.5), tile);
        m->setContentsMargins(Book::kPadX, 0, Book::kPadX, 0);
        tc_->addWidget(m);
        auto* row = new QHBoxLayout;
        row->setContentsMargins(Book::kPadX, 5, Book::kPadX, 0);
        row->setSpacing(8);
        if (e.progress() >= 0) row->addWidget(new Bar(e.progress(), 0, tile), 1);
        else row->addStretch(1);
        row->addWidget(label(relativeWhen(e.lastTouched), "libMuted", sans(11), tile));
        tc_->addLayout(row);
        tc_->addStretch(1);
        tiles.append(tile);
    }
    auto* slot = new NewSlot(QSize(160, 232), tr("Novo projeto"), grid);
    slot->onClick = [hooks]() { if (hooks.newProject) hooks.newProject(); };
    QVector<QWidget*> all = tiles;
    all.append(slot);

    const auto apply = [=]() {
        const QString q = search->text().trimmed();
        QVector<int> idx;
        for (int i = 0; i < entries.size(); ++i) {
            const LibraryEntry& e = entries.at(i);
            if (q.isEmpty() || e.name.contains(q, Qt::CaseInsensitive)
                || e.author.contains(q, Qt::CaseInsensitive) || e.genres.contains(q, Qt::CaseInsensitive))
                idx.append(i);
        }
        const int mode = sort->currentIndex();
        if (mode == 1) {
            std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
                return QString::localeAwareCompare(entries.at(a).name, entries.at(b).name) < 0;
            });
        } else if (mode == 2) {
            std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
                return entries.at(a).totalWords > entries.at(b).totalWords;
            });
        }
        QVector<QWidget*> visible;
        for (int i : idx) visible.append(tiles.at(i));
        if (q.isEmpty()) visible.append(slot);
        grid->setTiles(visible, all);
    };
    QObject::connect(search, &QLineEdit::textChanged, root, apply);
    QObject::connect(sort, QOverload<int>::of(&QComboBox::currentIndexChanged), root, [apply](int i) {
        QSettings().setValue(QStringLiteral("library/vitrineSort"), i);
        apply();
    });
    apply();
    return root;
}

// ---- M3 Cinema -----------------------------------------------------------

QWidget* LibraryViews::cinemaView(const QVector<LibraryEntry>& entries,
                                  const LibraryHooks& hooks, QWidget* parent)
{
    return new CinemaStage(entries, hooks, parent);
}

// ---- M4 Seu dia ----------------------------------------------------------

QWidget* LibraryViews::dayView(const QVector<LibraryEntry>& entries, const WritingDay& day,
                               const QVector<LibraryReminder>& reminders,
                               const LibraryHooks& hooks, QWidget* parent)
{
    auto* root = new QWidget(parent);
    auto* row = new QHBoxLayout(root);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(22);

    auto* scroll = makeScroll(root);
    QWidget* body = scrollBody(scroll);
    auto* list = new QVBoxLayout(body);
    list->setContentsMargins(0, 2, 8, 16);
    list->setSpacing(10);
    for (const auto& e : entries) {
        auto* card = new ClickFrame("libCard", body);
        auto* cl = new QHBoxLayout(card);
        cl->setContentsMargins(14, 10, 22, 10);
        cl->setSpacing(18);
        auto* book = new Book(e.cover, QSize(100, 145), card);
        wireCard(card, book, e, hooks);
        cl->addWidget(book, 0, Qt::AlignTop);
        auto* tcol = new QVBoxLayout;
        tcol->setSpacing(2);
        tcol->addSpacing(Book::kPadTop);
        tcol->addWidget(new ElideLabel(e.name, "libTitle", serif(19, QFont::DemiBold), card));
        tcol->addWidget(new ElideLabel(metaLine(e, true), "libMuted", sans(12.5), card));
        if (!e.resumeWhere.isEmpty()) {
            tcol->addSpacing(6);
            tcol->addWidget(new ElideLabel(tr("Você parou em %1").arg(e.resumeWhere), "libText", sans(12.5), card));
        }
        const QString syn = e.synopsis.simplified();
        if (!syn.isEmpty()) {
            tcol->addSpacing(8);
            tcol->addWidget(new WrapLabel(syn, 2, "libMuted", sans(12.5), card));
        }
        tcol->addStretch(1);
        cl->addLayout(tcol, 1);
        if (e.progress() >= 0) {
            auto* pcol = new QVBoxLayout;
            pcol->setSpacing(5);
            pcol->addStretch(1);
            pcol->addWidget(label(QStringLiteral("%1%").arg(qRound(e.progress() * 100)), "libMuted", sans(12), card),
                            0, Qt::AlignRight);
            pcol->addWidget(new Bar(e.progress(), 160, card));
            pcol->addStretch(1);
            cl->addLayout(pcol);
        }
        card->setFixedHeight(145 + Book::kPadTop + Book::kPadBottom + 20);
        list->addWidget(card);
    }
    list->addStretch(1);
    row->addWidget(scroll, 1);

    auto* panel = new QFrame(root);
    panel->setObjectName(QStringLiteral("libDayPanel"));
    panel->setFixedWidth(260);
    auto* pc = new QVBoxLayout(panel);
    pc->setContentsMargins(20, 18, 20, 18);
    pc->setSpacing(0);
    pc->addWidget(kick(tr("Seu dia"), panel));
    if (!day.scopeName.isEmpty()) {
        pc->addSpacing(4);
        pc->addWidget(new ElideLabel(tr("Meta de %1").arg(day.scopeName), "libMuted", sans(11), panel));
    }
    pc->addSpacing(12);
    pc->addWidget(new GoalRing(day, panel), 0, Qt::AlignHCenter);
    pc->addSpacing(12);
    pc->addLayout(streakRow(day, panel));
    pc->addSpacing(18);
    pc->addWidget(kick(tr("Últimos 7 dias"), panel));
    pc->addSpacing(10);
    pc->addWidget(new WeekBars(day, panel));
    pc->addSpacing(16);
    pc->addWidget(kick(tr("Lembretes de hoje"), panel));
    pc->addSpacing(8);
    if (reminders.isEmpty()) {
        pc->addWidget(label(tr("Nada marcado pra hoje."), "libMuted", sans(12.5), panel));
    } else {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (const auto& r : reminders) {
            QString html = QStringLiteral("• ") + r.text.toHtmlEscaped();
            QStringList tail;
            if (r.dueAt > 0 && r.dueAt < now) tail << tr("atrasado");
            if (!r.project.isEmpty()) tail << r.project.toHtmlEscaped();
            if (!tail.isEmpty())
                html += QStringLiteral(" <span style=\"color:%1;\">· %2</span>")
                            .arg(Theme::textMuted(), tail.join(QStringLiteral(" · ")));
            auto* l = label(html, "libText", sans(12.5), panel);
            l->setWordWrap(true);
            pc->addWidget(l);
            pc->addSpacing(4);
        }
    }
    pc->addStretch(1);
    row->addWidget(panel, 0, Qt::AlignTop);
    return root;
}

// ---- M5 Estante ----------------------------------------------------------

QWidget* LibraryViews::shelfView(const QVector<LibraryEntry>& entries,
                                 const LibraryHooks& hooks, QWidget* parent)
{
    auto* scroll = makeScroll(parent);
    QWidget* body = scrollBody(scroll);
    auto* col = new QVBoxLayout(body);
    col->setContentsMargins(0, 0, 10, 16);
    col->setSpacing(0);
    const LibraryEntry& e = entries.first();

    auto* host = new PlankHost(body);
    auto* hero = new QHBoxLayout(host);
    // O pé da capa encosta no topo da tábua: a margem de baixo desconta o
    // respiro de sombra que o Book já tem embaixo.
    hero->setContentsMargins(14, 0, 0, PlankHost::kPlank + PlankHost::kShadow - Book::kPadBottom);
    hero->setSpacing(26);
    auto* book = new Book(e.cover, QSize(220, 325), host);
    wireBook(book, e, hooks);
    hero->addWidget(book, 0, Qt::AlignBottom);
    auto* info = new QVBoxLayout;
    info->setSpacing(0);
    info->addStretch(1);
    info->addWidget(kick(tr("Na mesa agora"), host));
    info->addSpacing(6);
    auto* title = label(e.name, "libTitle", serif(30, QFont::DemiBold), host);
    title->setWordWrap(true);
    info->addWidget(title);
    info->addSpacing(4);
    QStringList bits;
    if (!e.resumeWhere.isEmpty()) bits << e.resumeWhere;
    if (e.lastTouched.isValid()) bits << relativeWhen(e.lastTouched);
    if (!bits.isEmpty()) info->addWidget(label(bits.join(QStringLiteral(" · ")), "libMuted", sans(13.5), host));
    info->addSpacing(18);
    auto* go = goButton(tr("Continuar  →"), host);
    const QString path = e.path;
    QObject::connect(go, &QPushButton::clicked, body, [hooks, path]() { if (hooks.resume) hooks.resume(path); });
    info->addWidget(go, 0, Qt::AlignLeft);
    info->addSpacing(Book::kPadBottom + 6);
    hero->addLayout(info, 1);
    col->addWidget(host);
    col->addSpacing(4);

    col->addWidget(new ShelfWall(entries.mid(1), tr("Novo projeto"), hooks, body));
    col->addStretch(1);
    return scroll;
}

// ---- Primeira vez --------------------------------------------------------

QWidget* LibraryViews::welcomeView(const LibraryHooks& hooks, QWidget* parent)
{
    auto* root = new QWidget(parent);
    auto* col = new QVBoxLayout(root);
    col->setContentsMargins(4, 30, 10, 10);
    col->setSpacing(0);
    col->addWidget(kick(tr("Bem-vindo ao Qenna"), root));
    col->addSpacing(12);
    auto* title = label(tr("Toda história começa em algum lugar."), "libTitle", serif(32, QFont::DemiBold), root);
    title->setWordWrap(true);
    col->addWidget(title);
    col->addSpacing(10);
    auto* lede = label(tr("Para qualquer autor, qualquer história.\n"
                          "Feito para que toda criação e ideia tome forma e alcance seu potencial máximo.\n"
                          "Seja bem-vindo à família Qenna! Inicie o seu novo projeto abaixo."),
                       "libText", sans(14), root);
    lede->setWordWrap(true);
    lede->setMaximumWidth(560);
    col->addWidget(lede);
    col->addSpacing(26);

    struct Act { QString icon, title, text; std::function<void()> fn; };
    const QVector<Act> acts = {
        { QStringLiteral(":/icons/newproject.svg"), tr("Começar um livro"),
          tr("Um projeto novo, com manuscrito e gavetas prontas."), hooks.newProject },
        { QStringLiteral(":/icons/doc-plus.svg"), tr("Anotar uma ideia"),
          tr("Só um documento, sem montar nada. Vira livro depois, se quiser."), hooks.newIdea },
        { QStringLiteral(":/icons/loadproject.svg"), tr("Abrir um projeto"),
          tr("Já tem um projeto do Qenna numa pasta? Abra por aqui."), hooks.loadProject },
    };
    auto* row = new QHBoxLayout;
    row->setSpacing(12);
    const QColor a = tc(Theme::accentDefault());
    for (const Act& act : acts) {
        auto* card = new ClickFrame("libAct", root);
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(18, 18, 18, 18);
        cl->setSpacing(0);
        auto* icon = new QLabel(card);
        icon->setPixmap(IconUtils::loadToolbarIcon(act.icon, a, a, a, QSize(24, 24)).pixmap(QSize(24, 24)));
        cl->addWidget(icon);
        cl->addSpacing(12);
        cl->addWidget(label(act.title, "libTitle", sans(14.5, QFont::DemiBold), card));
        cl->addSpacing(5);
        auto* t = label(act.text, "libMuted", sans(12.5), card);
        t->setWordWrap(true);
        cl->addWidget(t);
        cl->addStretch(1);
        const auto fn = act.fn;
        card->onClick = [fn]() { if (fn) fn(); };
        row->addWidget(card, 1);
    }
    col->addLayout(row);
    col->addSpacing(28);

    auto* quick = new QHBoxLayout;
    quick->setSpacing(8);
    quick->addWidget(label(tr("Idioma"), "libMuted", sans(12.5), root));
    auto* lang = new QComboBox(root);
    lang->setObjectName(QStringLiteral("libSort"));
    lang->setCursor(Qt::PointingHandCursor);
    lang->addItem(QStringLiteral("Português (BR)"), QStringLiteral("pt_BR"));
    lang->addItem(QStringLiteral("English"), QStringLiteral("en"));
    lang->addItem(QStringLiteral("Español"), QStringLiteral("es"));
    lang->addItem(QStringLiteral("Italiano"), QStringLiteral("it"));
    lang->addItem(QStringLiteral("Français"), QStringLiteral("fr"));
    {
        const QString cur = QSettings().value(QStringLiteral("app/language"), QStringLiteral("pt_BR")).toString();
        const int i = lang->findData(cur);
        lang->setCurrentIndex(i >= 0 ? i : 0);
    }
    QObject::connect(lang, QOverload<int>::of(&QComboBox::currentIndexChanged), root, [hooks, lang](int i) {
        if (hooks.language) hooks.language(lang->itemData(i).toString());
    });
    quick->addWidget(lang);
    quick->addSpacing(18);
    quick->addWidget(label(tr("Tema"), "libMuted", sans(12.5), root));
    const QStringList quickThemes = { QStringLiteral("grace-dark"), QStringLiteral("parchment"),
                                      QStringLiteral("nord"), QStringLiteral("cafe") };
    int shown = 0;
    for (const QString& id : quickThemes) {
        for (const Theme::MiraTheme& t : Theme::Manager::instance()->available()) {
            if (t.id != id) continue;
            quick->addWidget(new ThemeSwatch(t, root));
            ++shown;
            break;
        }
    }
    const int more = Theme::Manager::instance()->available().size() - shown;
    if (more > 0) {
        quick->addSpacing(4);
        quick->addWidget(label(tr("+ %1 no painel de Temas").arg(more), "libMuted", sans(12), root));
    }
    quick->addStretch(1);
    col->addLayout(quick);
    col->addStretch(1);
    return root;
}
