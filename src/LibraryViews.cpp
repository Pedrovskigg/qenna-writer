#include "LibraryViews.h"

#include "IconUtils.h"
#include "PanelMotion.h"
#include "Theme.h"
#include "WordCounter.h"

#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QEasingCurve>
#include <QKeyEvent>
#include <QtMath>
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

// ---- Destaque (o menu flutuante) ------------------------------------------
// Ideia do Pe (26/11/2026, desenho no Paint; Lab D6TuYGLtomfY4KkszkxFVg): o
// projeto em destaque com a capa grande à esquerda, o texto do lado e o "Seu
// dia" na direita; embaixo, o próximo da fila inclinado e a fileira dos outros,
// cortada pela borda de baixo. Cada livro é uma caixa 3D (capa, lombada e
// páginas) projetada em perspectiva: o destaque e o próximo flutuam de leve, e
// todos se inclinam para onde o mouse empurra (o lado embaixo do cursor recua).
// Rolar a roda troca o destaque; clicar num livro traz ele para o destaque.
//
// As medidas vêm do Lab, numa moldura de 1578 × 980 que escala com a vista.

class FloatStage : public QWidget {
    static constexpr qreal kDW = 1578, kDH = 980;   // moldura de desenho
    static constexpr qreal kPersp = 1500;            // distância da câmera (perspective do CSS)
    static constexpr qreal kRestRy = -19;            // de lado: aparece o bloco das páginas à direita (desenho do Pe, 27/11; era -11)
    static constexpr int kMoveMs = 850;
    static constexpr int kTextMs = 460;

    struct Pose { QRectF r; qreal rot = 0, op = 1, t = 80, z = 0; bool floating = false; };   // t = espessura (dobrada em 27/11: "os livros estão mt finos", o Pe)
    struct Item {
        Pose from, to;
        qreal ry = kRestRy, rx = 0, tz = 0;          // inclinação atual
        qreal try_ = kRestRy, trx = 0, ttz = 0;      // inclinação pedida pelo mouse
        qreal bobAmp = 0;                            // 0 parado .. 1 flutuando
        QPixmap art;                                 // capa no tamanho de tela (dpr 1)
        QPolygonF hit;                               // capa projetada (para o mouse)
        QRectF bounds;                               // caixa pintada (para o update parcial)
    };

public:
    FloatStage(const QVector<LibraryEntry>& entries, const WritingDay& day,
               const QVector<LibraryReminder>& reminders, const LibraryHooks& hooks, QWidget* parent)
        : QWidget(parent), m_entries(entries), m_hooks(hooks)
    {
        setObjectName(QStringLiteral("libFloat"));
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        m_items.resize(m_entries.size());
        m_clock.start();

        m_go = goButton(LibraryViews::tr("Continuar  →"), this);
        connect(m_go, &QPushButton::clicked, this, [this]() {
            if (m_hooks.resume && !m_entries.isEmpty()) m_hooks.resume(m_entries.at(m_cur).path);
        });

        buildSide(day, reminders);

        m_ease.addCubicBezierSegment(QPointF(0.2, 0.8), QPointF(0.22, 1.0), QPointF(1.0, 1.0));
        m_tick = new QTimer(this);
        m_tick->setTimerType(Qt::PreciseTimer);
        connect(m_tick, &QTimer::timeout, this, [this]() { onTick(); });

        m_shown = 0;
        setBackdrop(false);
        for (int k = 0; k < m_items.size(); ++k) {
            m_items[k].to = poseFor(k);
            m_items[k].from = m_items[k].to;
        }
        m_moveStart = -1;
    }

protected:
    void resizeEvent(QResizeEvent* e) override
    {
        QWidget::resizeEvent(e);
        m_bgScaled = QPixmap();
        m_prevScaled = QPixmap();
        for (Item& it : m_items) it.art = QPixmap();
        // Os lugares dependem do tamanho (quantas capas cabem na fileira).
        for (int k = 0; k < m_items.size(); ++k) {
            m_items[k].to = poseFor(k);
            if (m_moveStart < 0) m_items[k].from = m_items[k].to;
        }
        layoutChildren();
    }

    void showEvent(QShowEvent* e) override { QWidget::showEvent(e); wake(); }
    void hideEvent(QHideEvent* e) override { QWidget::hideEvent(e); m_tick->stop(); }

    void paintEvent(QPaintEvent* ev) override
    {
        QElapsedTimer cost;
        cost.start();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setClipRegion(ev->region());
        const qreal s = scale();
        const QColor bg = tc(Theme::chromeBackground());

        // Fundo: a capa do destaque, desfocada e fraquinha, apagando nas bordas.
        if (m_bgScaled.isNull()) m_bgScaled = scaledBg(m_bg);
        if (m_prevScaled.isNull() && !m_prevBg.isNull()) m_prevScaled = scaledBg(m_prevBg);
        const qreal glow = isDark() ? 0.34 : 0.12;   // no claro, o borrão escuro vira cinza sujo
        const qreal mix = bgMix();
        if (mix < 1.0 && !m_prevScaled.isNull()) {
            p.setOpacity(glow * (1.0 - mix));
            p.drawPixmap(0, 0, m_prevScaled);
        }
        if (!m_bgScaled.isNull()) {
            p.setOpacity(glow * mix);
            p.drawPixmap(0, 0, m_bgScaled);
        }
        p.setOpacity(1.0);
        {
            QRadialGradient vg(QPointF(width() * 0.30, height() * 0.30), qMax(width(), height()) * 0.85);
            vg.setColorAt(0.30, alpha(bg, 0.0));
            vg.setColorAt(0.85, bg);
            p.fillRect(rect(), vg);
            // e as quatro bordas apagam no fundo do menu, sem linha dura
            const qreal e = 90 * s;
            const auto edge = [&](const QRectF& r, QPointF a0, QPointF a1) {
                QLinearGradient g(a0, a1);
                g.setColorAt(0.0, bg);
                g.setColorAt(1.0, alpha(bg, 0.0));
                p.fillRect(r, g);
            };
            edge(QRectF(0, 0, width(), e), QPointF(0, 0), QPointF(0, e));
            edge(QRectF(0, height() - e, width(), e), QPointF(0, height()), QPointF(0, height() - e));
            edge(QRectF(0, 0, e, height()), QPointF(0, 0), QPointF(e, 0));
            edge(QRectF(width() - e, 0, e, height()), QPointF(width(), 0), QPointF(width() - e, 0));
        }

        // Os livros, do fundo para a frente.
        const qreal now = m_clock.elapsed() / 1000.0;
        QVector<int> order(m_items.size());
        for (int k = 0; k < order.size(); ++k) order[k] = k;
        std::sort(order.begin(), order.end(), [this](int a, int b) { return m_items[a].to.z < m_items[b].to.z; });
        const QRect dirty = ev->rect();
        for (int k : order) {
            // livro parado fora da área a redesenhar: não precisa pintar de novo
            const Item& it = m_items.at(k);
            const bool still = m_moveStart < 0 && it.bobAmp < 0.01 && std::abs(it.ry - it.try_) < 0.05
                && std::abs(it.rx - it.trx) < 0.05 && std::abs(it.tz - it.ttz) < 0.1;
            if (still && !it.bounds.isEmpty() && !dirty.intersects(it.bounds.toAlignedRect().adjusted(-6, -6, 6, 6))) continue;
            paintBook(p, k, s, now);
        }

        // Vinheta no canto de baixo à direita: a fileira some em vez de acabar seca.
        {
            const QRectF vr(width() - 440 * s, 420 * s, 440 * s, height() - 420 * s);
            if (m_vignette.isNull() || m_vignetteColor != bg) {
                // alfa = some da esquerda para a direita × some de cima para baixo
                m_vignetteColor = bg;
                m_vignette = QImage(128, 128, QImage::Format_ARGB32_Premultiplied);
                const auto smooth = [](qreal v) { v = qBound(0.0, v, 1.0); return v * v * (3 - 2 * v); };
                for (int y = 0; y < 128; ++y) {
                    QRgb* line = reinterpret_cast<QRgb*>(m_vignette.scanLine(y));
                    const qreal vy = smooth(y / 127.0 / 0.25);
                    for (int x = 0; x < 128; ++x) {
                        const qreal a = smooth(x / 127.0 / 0.72) * vy;
                        line[x] = qPremultiply(qRgba(bg.red(), bg.green(), bg.blue(), qRound(a * 255)));
                    }
                }
            }
            p.drawImage(vr, m_vignette);
        }

        paintText(p, s);
        // Quanto o quadro custa (média móvel): é o que decide o ritmo da flutuação.
        const qreal ms = cost.nsecsElapsed() / 1e6;
        m_paintMs = m_paintMs <= 0 ? ms : m_paintMs * 0.9 + ms * 0.1;
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        const int k = hitTest(e->position());
        setCursor(k >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        for (int i = 0; i < m_items.size(); ++i) {
            Item& it = m_items[i];
            if (i == k) {
                const Pose cp = currentPose(i);
                const QPointF c = cp.r.center() * scale();
                const QPointF d = e->position() - c;
                const qreal a = -qDegreesToRadians(cp.rot);
                const qreal lx = d.x() * std::cos(a) - d.y() * std::sin(a);
                const qreal ly = d.x() * std::sin(a) + d.y() * std::cos(a);
                const qreal nx = qBound(-1.0, lx / (cp.r.width() * scale() / 2), 1.0);
                const qreal ny = qBound(-1.0, ly / (cp.r.height() * scale() / 2), 1.0);
                it.try_ = kRestRy + nx * 15;
                it.trx = -ny * 11;
                it.ttz = -26;
            } else {
                it.try_ = kRestRy; it.trx = 0; it.ttz = 0;
            }
        }
        wake();
    }
    void leaveEvent(QEvent* e) override
    {
        for (Item& it : m_items) { it.try_ = kRestRy; it.trx = 0; it.ttz = 0; }
        wake();
        QWidget::leaveEvent(e);
    }
    void mousePressEvent(QMouseEvent* e) override { m_pressed = hitTest(e->position()); e->accept(); }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        const int k = hitTest(e->position());
        if (e->button() == Qt::LeftButton && k >= 0 && k == m_pressed) {
            if (k == m_cur) { if (m_hooks.open) m_hooks.open(m_entries.at(k).path); }
            else go(k);
        }
        m_pressed = -1;
    }
    void contextMenuEvent(QContextMenuEvent* e) override
    {
        const int k = hitTest(e->pos());
        if (k >= 0 && m_hooks.menu) m_hooks.menu(m_entries.at(k).path, e->globalPos());
    }
    void wheelEvent(QWheelEvent* e) override
    {
        m_wheel += e->angleDelta().y() != 0 ? e->angleDelta().y() : e->angleDelta().x();
        if (std::abs(m_wheel) >= 120 && m_clock.elapsed() - m_lastStep > 380) {
            step(m_wheel < 0 ? 1 : -1);
            m_wheel = 0;
            m_lastStep = m_clock.elapsed();
        }
        if (std::abs(m_wheel) > 240) m_wheel = m_wheel > 0 ? 240 : -240;
        e->accept();
    }
    void keyPressEvent(QKeyEvent* e) override
    {
        switch (e->key()) {
        case Qt::Key_Down: case Qt::Key_Right: step(1); return;
        case Qt::Key_Up: case Qt::Key_Left: step(-1); return;
        case Qt::Key_Return: case Qt::Key_Enter: m_go->click(); return;
        default: QWidget::keyPressEvent(e);
        }
    }

private:
    qreal scale() const { return qMax(0.45, qMin(width() / kDW, height() / kDH)); }
    bool isDark() const { return tc(Theme::chromeBackground()).lightnessF() < 0.5; }
    bool motion() const { return PanelMotion::enabled(); }

    int rowCount() const
    {
        const qreal s = scale();
        const int fit = int(std::ceil((width() / s - 600.0) / 360.0));
        return qBound(1, fit, 12);
    }

    // Lugar do livro k na fila atual: 0 destaque, 1 o próximo, depois a fileira.
    Pose poseFor(int k) const
    {
        const int n = m_entries.size();
        const int d = ((k - m_cur) % n + n) % n;
        const int rows = rowCount();
        Pose p;
        if (d == 0) { p.r = QRectF(70, 50, 340, 510); p.t = 92; p.z = 8; p.floating = true; return p; }
        if (d == 1) { p.r = QRectF(260, 560, 290, 435); p.rot = 10; p.t = 80; p.z = 7; p.floating = true; return p; }
        const int ri = d - 2;
        if (ri < rows) { p.r = QRectF(600 + ri * 360, 610, 330, 495); p.t = 86; p.z = 5; return p; }
        if (d == n - 1) { p.r = QRectF(70, -660, 340, 510); p.rot = -4; p.op = 0; p.t = 92; p.z = 9; return p; }
        p.r = QRectF(600 + rows * 360, 610, 330, 495); p.op = 0; p.t = 86; p.z = 4;
        return p;
    }

    qreal moveProgress() const
    {
        if (m_moveStart < 0) return 1.0;
        const qreal t = qBound(0.0, (m_clock.elapsed() - m_moveStart) / qreal(kMoveMs), 1.0);
        return m_ease.valueForProgress(t);
    }
    Pose currentPose(int k) const
    {
        const Item& it = m_items.at(k);
        const qreal e = moveProgress();
        if (e >= 1.0) return it.to;
        const auto L = [e](qreal a, qreal b) { return a + (b - a) * e; };
        Pose p;
        p.r = QRectF(L(it.from.r.x(), it.to.r.x()), L(it.from.r.y(), it.to.r.y()),
                     L(it.from.r.width(), it.to.r.width()), L(it.from.r.height(), it.to.r.height()));
        p.rot = L(it.from.rot, it.to.rot);
        p.op = L(it.from.op, it.to.op);
        p.t = L(it.from.t, it.to.t);
        p.z = it.to.z;
        p.floating = it.to.floating;
        return p;
    }

    void step(int dir)
    {
        if (m_entries.size() < 2) return;
        go(m_cur + dir);
    }
    void go(int k)
    {
        const int n = m_entries.size();
        if (n == 0) return;
        k = ((k % n) + n) % n;
        if (k == m_cur) return;
        for (int i = 0; i < m_items.size(); ++i) m_items[i].from = currentPose(i);
        m_prevShown = m_shown;
        m_cur = k;
        for (int i = 0; i < m_items.size(); ++i) m_items[i].to = poseFor(i);
        if (motion()) {
            m_moveStart = m_clock.elapsed();
            m_textStart = m_clock.elapsed();
        } else {
            m_moveStart = -1;
            m_textStart = -1;
            m_shown = m_cur;
            layoutChildren();
        }
        setBackdrop(motion());
        wake();
    }

    void setBackdrop(bool animate)
    {
        m_prevBg = m_bg;
        m_prevScaled = m_bgScaled;
        m_bg = blurredThumb(m_entries.value(m_cur).cover);
        m_bgScaled = QPixmap();
        m_bgStart = (animate && !m_prevBg.isNull()) ? m_clock.elapsed() : -1;
    }
    qreal bgMix() const
    {
        if (m_bgStart < 0) return 1.0;
        return qBound(0.0, (m_clock.elapsed() - m_bgStart) / 900.0, 1.0);
    }
    QPixmap scaledBg(const QImage& img) const
    {
        if (img.isNull() || width() <= 0 || height() <= 0) return {};
        const qreal dpr = devicePixelRatioF();
        QPixmap pm = QPixmap::fromImage(img.scaled(size() * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        const QSize target = size() * dpr;
        pm = pm.copy((pm.width() - target.width()) / 2, (pm.height() - target.height()) / 3, target.width(), target.height());
        pm.setDevicePixelRatio(dpr);
        return pm;
    }

    // Liga o relógio no ritmo que a cena pede: 60 quadros enquanto algo anda,
    // 30 só para a flutuação (e pintando só em volta dos dois livros que flutuam),
    // e parado quando nada se mexe ou a vista está escondida.
    void wake()
    {
        if (!isVisible()) return;
        if (!m_tick->isActive()) m_tick->start(16);
    }
    bool busy() const
    {
        if (m_moveStart >= 0 || m_textStart >= 0 || m_bgStart >= 0) return true;
        for (const Item& it : m_items) {
            if (std::abs(it.ry - it.try_) > 0.05 || std::abs(it.rx - it.trx) > 0.05 || std::abs(it.tz - it.ttz) > 0.1) return true;
            const qreal amp = (it.to.floating && motion() && !m_floatOff) ? 1.0 : 0.0;
            if (std::abs(it.bobAmp - amp) > 0.01) return true;
        }
        return false;
    }
    void onTick()
    {
        const qint64 now = m_clock.elapsed();
        const qreal dt = m_lastTick > 0 ? qBound(0.0, (now - m_lastTick) / 16.0, 4.0) : 1.0;
        m_lastTick = now;
        const qreal k = motion() ? 1.0 - std::pow(1.0 - 0.16, dt) : 1.0;
        for (Item& it : m_items) {
            it.ry += (it.try_ - it.ry) * k;
            it.rx += (it.trx - it.rx) * k;
            it.tz += (it.ttz - it.tz) * k;
            const qreal amp = (it.to.floating && motion() && !m_floatOff) ? 1.0 : 0.0;
            it.bobAmp += (amp - it.bobAmp) * (motion() ? 1.0 - std::pow(1.0 - 0.06, dt) : 1.0);
        }
        if (m_moveStart >= 0 && now - m_moveStart >= kMoveMs) {
            m_moveStart = -1;
            for (Item& it : m_items) it.from = it.to;
        }
        if (m_textStart >= 0) {
            const qint64 t = now - m_textStart;
            if (t >= kTextMs / 2 && m_shown != m_cur) { m_shown = m_cur; layoutChildren(); }
            if (t >= kTextMs) m_textStart = -1;
        }
        if (m_bgStart >= 0 && now - m_bgStart >= 900) { m_bgStart = -1; m_prevScaled = QPixmap(); m_prevBg = QImage(); }

        if (busy()) {
            if (m_tick->interval() != 16) m_tick->setInterval(16);
            update();
            return;
        }
        if (motion() && !m_floatOff && m_entries.size() > 0) {
            // Só flutuando: repinta em volta dos livros que se mexem, a 60
            // quadros. Em computador fraco o próprio custo do quadro freia:
            // 30 quadros, e se nem assim der, os livros param de flutuar
            // (um Pentium não fica a 100% só por estar no menu).
            if (m_paintMs > 18) { m_floatOff = true; update(); return; }
            const int iv = m_paintMs < 7 ? 16 : 33;
            if (m_tick->interval() != iv) m_tick->setInterval(iv);
            QRegion reg;
            for (const Item& it : m_items)
                if (it.to.floating && !it.bounds.isEmpty()) reg += it.bounds.toAlignedRect().adjusted(-24, -24, 24, 70);
            update(reg);
            return;
        }
        m_tick->stop();
        update();
    }

    // ---- o livro em 3D --------------------------------------------------------
    void paintBook(QPainter& p, int k, qreal s, qreal now)
    {
        Item& it = m_items[k];
        const Pose cp = currentPose(k);
        if (cp.op <= 0.01) { it.hit.clear(); it.bounds = QRectF(); return; }
        const qreal dpr = devicePixelRatioF();
        const QSize artPx = (QSizeF(340, 510) * s * dpr).toSize();
        if (it.art.isNull() || it.art.size() != artPx) {
            it.art = renderCoverArt(m_entries.at(k).cover, artPx.width(), artPx.height(), 1.0);
            it.art.setDevicePixelRatio(1.0);
        }

        // flutuação: cada livro num tempo, o próximo meio ciclo atrás do destaque
        const qreal phase = (k % 2) ? 3.1 : 0.0;
        const qreal w2 = (now + phase) * 2.0 * M_PI / 6.2;
        const qreal bob = (1.0 - std::cos(w2)) / 2.0 * it.bobAmp;   // 0..1
        const qreal bobY = -14.0 * bob;
        const qreal bobRot = 0.7 * bob;

        const qreal w = cp.r.width(), h = cp.r.height(), t = cp.t;
        const QPointF c(cp.r.center().x(), cp.r.center().y() + bobY);
        const qreal ry = qDegreesToRadians(it.ry), rx = qDegreesToRadians(it.rx);
        const qreal rr = qDegreesToRadians(cp.rot + bobRot);
        const qreal cy = std::cos(ry), sy = std::sin(ry), cx = std::cos(rx), sx = std::sin(rx);
        const qreal cr = std::cos(rr), sr = std::sin(rr);
        const auto proj = [&](qreal x, qreal y, qreal z) -> QPointF {
            const qreal x1 = x * cy + z * sy, z1 = -x * sy + z * cy;
            const qreal y2 = y * cx - z1 * sx, z2 = y * sx + z1 * cx + it.tz;
            const qreal f = kPersp / (kPersp - z2);
            const qreal px = x1 * f, py = y2 * f;
            return QPointF((c.x() + px * cr - py * sr) * s, (c.y() + px * sr + py * cr) * s);
        };
        const qreal X = w / 2, Y = h / 2, Z = t / 2;
        const QPolygonF front { proj(-X, -Y, Z), proj(X, -Y, Z), proj(X, Y, Z), proj(-X, Y, Z) };
        const QPolygonF spine { proj(-X, -Y, -Z), proj(-X, -Y, Z), proj(-X, Y, Z), proj(-X, Y, -Z) };
        const qreal in = 3;
        const QPolygonF pages { proj(X - 2, -Y + in, Z - 1), proj(X - 2, -Y + in, -Z + 1), proj(X - 2, Y - in, -Z + 1), proj(X - 2, Y - in, Z - 1) };
        const QPolygonF top { proj(-X + 2, -Y + 2, -Z + 1), proj(X - 3, -Y + 2, -Z + 1), proj(X - 3, -Y + 2, Z - 1), proj(-X + 2, -Y + 2, Z - 1) };
        const QPolygonF bottom { proj(-X + 2, Y - 2, Z - 1), proj(X - 3, Y - 2, Z - 1), proj(X - 3, Y - 2, -Z + 1), proj(-X + 2, Y - 2, -Z + 1) };
        const auto facing = [](const QPolygonF& q) {
            qreal a = 0;
            for (int i = 0; i < 4; ++i) a += q[i].x() * q[(i + 1) % 4].y() - q[(i + 1) % 4].x() * q[i].y();
            return a > 0;
        };

        p.save();
        p.setOpacity(cp.op);

        // sombra no chão, que encolhe quando o livro sobe
        if (it.bobAmp > 0.01) {
            const qreal sw = w * 0.84 * (1.0 - 0.16 * bob), sh = 34 * (1.0 - 0.16 * bob);
            const QPointF sc(cp.r.center().x() * s, (cp.r.bottom() + 46 - 17) * s);
            QRadialGradient g(sc, sw * s / 2);
            const QColor sc0 = isDark() ? QColor(0, 0, 0) : QColor(60, 45, 30);
            g.setColorAt(0.0, alpha(sc0, (isDark() ? 0.55 : 0.30) * it.bobAmp * (1.0 - 0.4 * bob) * cp.op));
            g.setColorAt(1.0, alpha(sc0, 0.0));
            p.save();
            p.translate(sc);
            p.scale(1.0, sh / sw);
            p.translate(-sc);
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawEllipse(sc, sw * s / 2, sw * s / 2);
            p.restore();
        }

        const auto faceFill = [&](const QPolygonF& q, const std::function<void(QPainter&)>& fill) {
            QTransform T;
            if (!QTransform::quadToQuad(QPolygonF(QRectF(0, 0, 1, 1)), q, T)) return;
            p.save();
            p.setTransform(T, false);
            p.setPen(Qt::NoPen);
            fill(p);
            p.restore();
        };

        if (facing(spine)) {
            // a lombada é a beirada esquerda da própria capa, esticada e sombreada
            QTransform T;
            const qreal stripW = qMax(2.0, it.art.width() * 0.06);
            if (QTransform::quadToQuad(QPolygonF(QRectF(0, 0, stripW, it.art.height())), spine, T)) {
                p.save();
                p.setTransform(T, false);
                p.drawPixmap(QRectF(0, 0, stripW, it.art.height()), it.art, QRectF(0, 0, stripW, it.art.height()));
                p.restore();
            }
            faceFill(spine, [](QPainter& q) {
                QLinearGradient g(0, 0, 1, 0);
                g.setColorAt(0.0, QColor(0, 0, 0, 140));
                g.setColorAt(0.4, QColor(0, 0, 0, 38));
                g.setColorAt(0.6, QColor(255, 255, 255, 16));
                g.setColorAt(1.0, QColor(0, 0, 0, 115));
                q.fillRect(QRectF(0, 0, 1, 1), g);
            });
        }
        // As folhas: listras ao longo da espessura. O número de listras sai do
        // tamanho da face na tela (no mínimo ~4 px cada) e a passagem entre
        // elas é suave: listra mais fina que o pixel vira onda de moiré, a
        // "TV de tubo" que o Pe viu quando os livros engrossaram (27/11).
        const auto paper = [](QPainter& q, bool vertical, qreal depthPx, const QColor& a, const QColor& b) {
            const qreal n = qBound(3.0, depthPx / 4.0, 34.0);
            const qreal per = 1.0 / n;
            QLinearGradient g(0, 0, vertical ? 0 : per, vertical ? per : 0);
            g.setSpread(QGradient::RepeatSpread);
            g.setColorAt(0.0, a);
            g.setColorAt(0.35, a);
            g.setColorAt(0.65, b);
            g.setColorAt(1.0, a);
            q.fillRect(QRectF(0, 0, 1, 1), g);
            QLinearGradient sh(0, 0, 1, 0);
            sh.setColorAt(0.0, QColor(0, 0, 0, 60));
            sh.setColorAt(0.5, QColor(0, 0, 0, 0));
            sh.setColorAt(1.0, QColor(0, 0, 0, 50));
            q.fillRect(QRectF(0, 0, 1, 1), sh);
        };
        // quantos pixels a espessura ocupa na tela, em cada face (média das duas bordas)
        const auto depth = [](const QPolygonF& q, int a0, int a1, int b0, int b1) {
            return (QLineF(q[a0], q[a1]).length() + QLineF(q[b0], q[b1]).length()) / 2.0;
        };
        if (facing(pages)) faceFill(pages, [&](QPainter& q) { paper(q, false, depth(pages, 0, 1, 3, 2), QColor(0xef, 0xe9, 0xdc), QColor(0xdc, 0xd4, 0xc4)); });
        if (facing(top)) faceFill(top, [&](QPainter& q) { paper(q, true, depth(top, 1, 2, 0, 3), QColor(0xf1, 0xeb, 0xde), QColor(0xdd, 0xd5, 0xc6)); });
        if (facing(bottom)) faceFill(bottom, [&](QPainter& q) { paper(q, true, depth(bottom, 1, 2, 0, 3), QColor(0xec, 0xe6, 0xd8), QColor(0xd6, 0xce, 0xbe)); });

        QTransform T;
        if (QTransform::quadToQuad(QPolygonF(QRectF(0, 0, it.art.width(), it.art.height())), front, T)) {
            p.save();
            p.setTransform(T, false);
            p.drawPixmap(0, 0, it.art);
            p.restore();
        }
        p.restore();

        it.hit = front;
        QRectF b = front.boundingRect().united(spine.boundingRect()).united(pages.boundingRect());
        it.bounds = b;
    }

    int hitTest(const QPointF& pos) const
    {
        int best = -1;
        qreal bestZ = -1e9;
        for (int k = 0; k < m_items.size(); ++k) {
            const Item& it = m_items.at(k);
            if (it.hit.size() == 4 && it.to.op > 0.5 && it.hit.containsPoint(pos, Qt::OddEvenFill) && it.to.z > bestZ) {
                best = k;
                bestZ = it.to.z;
            }
        }
        return best;
    }

    // ---- o texto do destaque --------------------------------------------------
    struct TextBlock {
        QString kick, title, meta, synopsis, where;
        QFont kickF, titleF, metaF, synF, whereF;
        qreal width = 0;
        QRectF kickR, titleR, metaR, synR, whereR;
        qreal bottom = 0;
    };

    TextBlock textFor(int k, qreal s) const
    {
        TextBlock b;
        const LibraryEntry& e = m_entries.at(k);
        const QString head = e.resumeWhere.isEmpty() ? LibraryViews::tr("Último projeto") : LibraryViews::tr("Continuar de onde parou");
        b.kick = (e.lastTouched.isValid() ? head + QStringLiteral(" · ") + LibraryViews::relativeWhen(e.lastTouched) : head).toUpper();
        b.title = e.name;
        QStringList facts;
        if (!e.genres.trimmed().isEmpty()) facts << e.genres.trimmed();
        if (e.totalWords >= 0) facts << LibraryViews::wordsText(e.totalWords);
        if (e.manuscriptCount > 1) facts << LibraryViews::tr("Saga · %1 livros").arg(e.manuscriptCount);
        b.meta = facts.join(QStringLiteral(" · "));
        b.synopsis = e.synopsis.simplified();
        b.where = e.resumeWhere;

        b.kickF = sans(qMax(9.5, 11.5 * s), QFont::DemiBold);
        b.kickF.setLetterSpacing(QFont::AbsoluteSpacing, qMax(1.2, 1.8 * s));
        b.titleF = serif(qMax(26.0, 54 * s), QFont::DemiBold);
        b.metaF = sans(qMax(12.0, 16.5 * s));
        b.synF = serif(qMax(13.0, 20 * s), QFont::Normal, true);
        b.whereF = sans(qMax(12.0, 15.5 * s));

        const qreal x = textLeft(s);
        b.width = textWidth(s);
        qreal y = 96 * s;
        const auto block = [&](const QString& txt, const QFont& f, int maxLines, qreal gapAfter, QString* out) -> QRectF {
            if (txt.isEmpty()) return {};
            const QFontMetricsF fm(f);
            QString t = txt;
            const auto tall = [&](const QString& v) {
                return fm.boundingRect(QRectF(0, 0, b.width, 1e5), Qt::TextWordWrap, v).height() > fm.lineSpacing() * maxLines + 1;
            };
            if (tall(t)) {
                while (t.size() > 8 && tall(t + QStringLiteral("…"))) t.chop(qMax(1, int(t.size() / 24)));
                t = t.trimmed() + QStringLiteral("…");
            }
            if (out) *out = t;
            const QRectF r = fm.boundingRect(QRectF(x, y, b.width, 1e5), Qt::TextWordWrap, t);
            const QRectF rr(x, y, b.width, r.height());
            y += r.height() + gapAfter;
            return rr;
        };
        b.kickR = block(b.kick, b.kickF, 1, 14 * s, &b.kick);
        b.titleR = block(b.title, b.titleF, 3, 12 * s, &b.title);
        b.metaR = block(b.meta, b.metaF, 1, 18 * s, &b.meta);
        b.synR = block(b.synopsis, b.synF, 5, 18 * s, &b.synopsis);
        b.whereR = block(b.where, b.whereF, 1, 26 * s, &b.where);
        b.bottom = y;
        return b;
    }
    qreal textLeft(qreal s) const { return 460 * s; }
    qreal textWidth(qreal s) const
    {
        const qreal right = m_side && m_side->isVisible() ? m_side->x() - 40 * s : width() - 40 * s;
        return qMax(220.0, qMin(480 * s, right - textLeft(s)));
    }

    void paintText(QPainter& p, qreal s)
    {
        if (m_entries.isEmpty()) return;
        qreal op = 1.0, dy = 0;
        if (m_textStart >= 0) {
            const qreal t = qBound(0.0, (m_clock.elapsed() - m_textStart) / qreal(kTextMs), 1.0);
            op = std::abs(2.0 * t - 1.0);
            dy = (1.0 - op) * 8 * s;
        }
        const TextBlock b = textFor(m_shown, s);
        p.save();
        p.setOpacity(op);
        p.translate(0, dy);
        p.setPen(tc(Theme::textMuted()));
        p.setFont(b.kickF);
        if (!b.kickR.isNull()) p.drawText(b.kickR, Qt::TextWordWrap, b.kick);
        p.setPen(tc(Theme::textBright()));
        p.setFont(b.titleF);
        if (!b.titleR.isNull()) p.drawText(b.titleR, Qt::TextWordWrap, b.title);
        p.setPen(tc(Theme::textMuted()));
        p.setFont(b.metaF);
        if (!b.metaR.isNull()) p.drawText(b.metaR, Qt::TextWordWrap, b.meta);
        p.setPen(alpha(tc(Theme::textPrimary()), 0.9));
        p.setFont(b.synF);
        if (!b.synR.isNull()) p.drawText(b.synR, Qt::TextWordWrap, b.synopsis);
        if (!b.whereR.isNull()) {
            const qreal bh = QFontMetricsF(b.whereF).height();
            p.fillRect(QRectF(b.whereR.left(), b.whereR.top() + bh * 0.12, 3, bh * 0.76), tc(Theme::accentDefault()));
            p.setPen(tc(Theme::textMuted()));
            p.setFont(b.whereF);
            p.drawText(b.whereR.adjusted(12, 0, 0, 0), Qt::TextWordWrap, b.where);
        }
        // "1 de 8 projetos", embaixo do botão
        if (m_entries.size() > 1) {
            QFont f = sans(qMax(10.0, 12 * s), QFont::DemiBold);
            f.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
            p.setFont(f);
            p.setPen(tc(Theme::textMuted()));
            p.setOpacity(op * 0.8);
            const qreal y = m_go->geometry().bottom() + 22 * s - dy;
            p.drawText(QRectF(textLeft(s), y, b.width, 20 * s + 6), Qt::AlignLeft | Qt::AlignTop,
                       LibraryViews::tr("%1 de %2 projetos").arg(m_shown + 1).arg(m_entries.size()).toUpper());
        }
        p.restore();
    }

    // ---- o "Seu dia" à direita ------------------------------------------------
    void buildSide(const WritingDay& day, const QVector<LibraryReminder>& reminders)
    {
        m_side = new QWidget(this);
        m_side->setAttribute(Qt::WA_TranslucentBackground, true);
        auto* col = new QVBoxLayout(m_side);
        col->setContentsMargins(0, 0, 0, 0);
        col->setSpacing(0);
        const QLocale loc;
        if (day.valid) {
            col->addWidget(kick(LibraryViews::tr("Seu dia"), m_side));
            if (!day.scopeName.isEmpty()) {
                col->addSpacing(4);
                col->addWidget(new ElideLabel(LibraryViews::tr("Meta de %1").arg(day.scopeName), "libMuted", sans(12), m_side));
            }
            col->addSpacing(16);
            auto* row = new QHBoxLayout;
            row->setSpacing(24);
            row->addWidget(new GoalRing(day, m_side), 0, Qt::AlignTop);
            auto* facts = new QVBoxLayout;
            facts->setSpacing(6);
            facts->addSpacing(8);
            const QString streak = day.streak == 1
                ? LibraryViews::tr("%1 dia seguido").arg(bright(QStringLiteral("1")))
                : LibraryViews::tr("%1 dias seguidos").arg(bright(loc.toString(day.streak)));
            facts->addWidget(label(streak, "libText", sans(15), m_side));
            int week = 0;
            for (int v : day.week) week += v;
            const QString wk = day.timeGoal ? LibraryViews::tr("%1 min nesta semana").arg(bright(loc.toString(week)))
                                            : LibraryViews::tr("%1 palavras nesta semana").arg(bright(loc.toString(week)));
            facts->addWidget(label(wk, "libText", sans(15), m_side));
            facts->addSpacing(8);
            facts->addWidget(new WeekBars(day, m_side));
            row->addLayout(facts, 1);
            col->addLayout(row);
        } else {
            // Sem meta nenhuma: a biblioteca em números no lugar do dia.
            int words = 0;
            for (const LibraryEntry& e : m_entries) words += qMax(0, e.totalWords);
            col->addWidget(kick(LibraryViews::tr("Sua biblioteca"), m_side));
            col->addSpacing(14);
            col->addWidget(label(loc.toString(words), "libTitle", serif(40, QFont::DemiBold), m_side));
            col->addSpacing(2);
            const QString in = m_entries.size() == 1 ? LibraryViews::tr("palavras em 1 projeto")
                                                     : LibraryViews::tr("palavras em %1 projetos").arg(m_entries.size());
            col->addWidget(label(in, "libText", sans(15), m_side));
        }
        if (!reminders.isEmpty()) {
            col->addSpacing(20);
            const LibraryReminder& r = reminders.first();
            const QString txt = r.project.isEmpty() ? r.text : QStringLiteral("%1 (%2)").arg(r.text, r.project);
            auto* rem = label(LibraryViews::tr("Lembrete de hoje: %1").arg(txt.toHtmlEscaped()), "libMuted", sans(14), m_side);
            rem->setWordWrap(true);
            col->addWidget(rem);
        }
    }

    void layoutChildren()
    {
        const qreal s = scale();
        if (m_side) {
            const int sx = qRound(990 * s), sw = qRound(qMax(380.0, 520 * s));
            const bool fits = sx + 340 <= width();
            m_side->setVisible(fits);
            if (fits) {
                m_side->setFixedWidth(qMin(sw, width() - sx - 24));
                m_side->adjustSize();
                m_side->move(sx, qRound(128 * s));
            }
        }
        if (!m_entries.isEmpty()) {
            m_go->setFont(sans(qMax(13.0, 16 * s), QFont::Medium));
            m_go->adjustSize();
            const TextBlock b = textFor(m_shown, s);
            m_go->move(qRound(textLeft(s)), qRound(b.bottom));
        }
        update();
    }

    QVector<LibraryEntry> m_entries;
    LibraryHooks m_hooks;
    QVector<Item> m_items;
    QPushButton* m_go = nullptr;
    QWidget* m_side = nullptr;
    QTimer* m_tick = nullptr;
    QElapsedTimer m_clock;
    QEasingCurve m_ease { QEasingCurve::BezierSpline };
    qint64 m_moveStart = -1, m_textStart = -1, m_bgStart = -1, m_lastStep = -1000, m_lastTick = 0;
    int m_cur = 0, m_shown = 0, m_prevShown = 0, m_pressed = -1, m_wheel = 0;
    QImage m_bg, m_prevBg;
    QPixmap m_bgScaled, m_prevScaled;
    QImage m_vignette;
    QColor m_vignetteColor;
    qreal m_paintMs = 0;
    bool m_floatOff = false;
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

// ---- Destaque ----------------------------------------------------------------

QWidget* LibraryViews::floatView(const QVector<LibraryEntry>& entries, const WritingDay& day,
                                 const QVector<LibraryReminder>& reminders,
                                 const LibraryHooks& hooks, QWidget* parent)
{
    return new FloatStage(entries, day, reminders, hooks, parent);
}
