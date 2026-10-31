#include "PageBackgroundUi.h"

#include "ColorPopover.h"
#include "SheetDialog.h"
#include "Theme.h"
#include "TimelineTracksTypes.h"

#include <QBuffer>
#include <QButtonGroup>
#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHash>
#include <QImageReader>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Tracks;

namespace {

constexpr int kFade = 110;       // a troca de fundo na quebra de cena esfuma nessa altura (px), pra cada lado
constexpr int kTileW = 340;      // "repetir": largura de cada ladrilho
constexpr int kTopH = 420;       // "só no topo": altura da imagem, que some até o fim dela
constexpr int kMaxSide = 1800;   // a imagem guardada no projeto

// A imagem decodificada de uma data URL (decodificar a cada pintura sairia caro).
QImage sourceImage(const QString& dataUrl)
{
    static QHash<uint, QImage> cache;
    const uint key = qHash(dataUrl);
    auto it = cache.constFind(key);
    if (it != cache.constEnd()) return *it;
    QImage img;
    const int comma = dataUrl.indexOf(QLatin1Char(','));
    if (comma > 0) img.loadFromData(QByteArray::fromBase64(dataUrl.mid(comma + 1).toLatin1()));
    if (cache.size() > 12) cache.clear();
    cache.insert(key, img);
    return img;
}

// Desfoque barato: reduz e amplia de novo, duas vezes.
QImage blurred(const QImage& src, int radius)
{
    if (radius <= 0 || src.isNull()) return src;
    const qreal k = 1.0 + radius * 0.55;
    QImage out = src;
    for (int i = 0; i < 2; ++i) {
        const QSize small(qMax(1, qRound(out.width() / k)), qMax(1, qRound(out.height() / k)));
        out = out.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                  .scaled(src.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return out;
}

// As versões prontas pra pintar (escala, desfoque, esfumado), guardadas por chave.
QImage prepared(const PageBackground& bg, const QString& kind, const QSize& size)
{
    static QHash<QString, QImage> cache;
    const QString key = QStringLiteral("%1|%2|%3x%4|%5").arg(qHash(bg.image)).arg(kind).arg(size.width()).arg(size.height()).arg(bg.blur);
    auto it = cache.constFind(key);
    if (it != cache.constEnd()) return *it;
    const QImage src = sourceImage(bg.image);
    QImage out;
    if (!src.isNull() && size.width() > 0) {
        if (kind == QLatin1String("width")) {
            // largura da folha; se ficar mais baixa que a tela, cobre a altura (corta dos lados)
            out = src.scaledToWidth(size.width(), Qt::SmoothTransformation);
            if (out.height() < size.height()) {
                out = src.scaledToHeight(size.height(), Qt::SmoothTransformation);
                out = out.copy((out.width() - size.width()) / 2, 0, size.width(), out.height());
            }
        } else if (kind == QLatin1String("cover")) {
            out = src.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            out = out.copy((out.width() - size.width()) / 2, (out.height() - size.height()) / 2, size.width(), size.height());
        } else if (kind == QLatin1String("tile")) {
            out = src.scaledToWidth(kTileW, Qt::SmoothTransformation);
        } else if (kind == QLatin1String("top")) {
            out = src.scaledToWidth(size.width(), Qt::SmoothTransformation);
            out = out.copy(0, 0, out.width(), qMin(out.height(), kTopH)).convertToFormat(QImage::Format_ARGB32_Premultiplied);
            QPainter p(&out);
            p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            QLinearGradient g(0, 0, 0, out.height());
            g.setColorAt(0, Qt::black);
            g.setColorAt(0.33, Qt::black);
            g.setColorAt(0.92, Qt::transparent);
            g.setColorAt(1, Qt::transparent);
            p.fillRect(out.rect(), g);
        }
        out = blurred(out, bg.blur);
    }
    if (cache.size() > 16) cache.clear();
    cache.insert(key, out);
    return out;
}

// O fundo de uma faixa, sem opacidade nem esfumado, em coordenadas do viewport.
// top = onde a faixa começa (pra imagem começar junto com a cena).
void drawContent(QPainter& p, const PageBackground& bg, const QRect& area, int top, const QSize& vp)
{
    if (bg.mode == QLatin1String("color")) {
        p.fillRect(area, QColor(bg.color));
        return;
    }
    if (bg.image.isEmpty()) return;
    p.save();
    p.setClipRect(area);
    if (bg.fit == QLatin1String("tile")) {
        const QImage t = prepared(bg, QStringLiteral("tile"), vp);
        if (!t.isNull()) p.drawTiledPixmap(area, QPixmap::fromImage(t), QPoint(0, bg.fixed ? area.top() : area.top() - top));
    } else if (bg.fit == QLatin1String("top")) {
        const QImage t = prepared(bg, QStringLiteral("top"), vp);
        p.drawImage(0, bg.fixed ? 0 : top, t);
    } else if (bg.fixed) {
        p.drawImage(0, 0, prepared(bg, QStringLiteral("cover"), vp));
    } else {
        // rola com o texto: a imagem na largura da folha, repetida pra baixo
        // espelhada (a emenda some), a partir do começo da faixa
        const QImage t = prepared(bg, QStringLiteral("width"), vp);
        if (!t.isNull()) {
            const QImage flip = t.mirrored(false, true);
            const int h = t.height();
            int k = (area.top() - top) / h - ((area.top() - top) % h < 0 ? 1 : 0);
            for (int y = top + k * h; y < area.bottom() + 1; y += h, ++k)
                p.drawImage(0, y, (k % 2 == 0) ? t : flip);
        }
    }
    p.restore();
}

QString toDataUrl(const QImage& img)
{
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG", 85);
    return QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(bytes.toBase64());
}

} // namespace

PageBackground PageBg::themeBackground()
{
    return themeBackground(Theme::Manager::instance()->current());
}

PageBackground PageBg::themeBackground(const Theme::MiraTheme& t)
{
    PageBackground b;
    if (t.pageImage.isEmpty()) return b;
    b.mode = QStringLiteral("image");
    b.image = t.pageImage;
    b.opacity = t.pageImageOpacity;
    b.blur = t.pageImageBlur;
    b.fit = t.pageImageFit;
    b.fixed = t.pageImageFixed;
    return b;
}

PageBackground PageBg::resolve(const PageBackground& bg)
{
    if (bg.isSet()) return bg;
    if (bg.mode == QLatin1String("none")) return PageBackground();
    return themeBackground();
}

bool PageBg::anyFixed(const QList<Region>& regions)
{
    for (const Region& r : regions) if (r.bg.isSet() && r.bg.fixed) return true;
    return false;
}

void PageBg::paint(QPainter& p, const QSize& vp, int yOffset, const QList<Region>& regions, const QRect& clip)
{
    for (int i = 0; i < regions.size(); ++i) {
        const Region& r = regions[i];
        if (!r.bg.isSet()) continue;
        // a troca com a vizinha esfuma: a faixa avança kFade pra dentro dela
        const bool fadeTop = i > 0 && !(regions[i - 1].bg == r.bg);
        const bool fadeBottom = i + 1 < regions.size() && !(regions[i + 1].bg == r.bg);
        const int top = (i == 0 ? 0 : r.y0) + yOffset;
        const int y0 = i == 0 ? -(1 << 28) : top - (fadeTop ? kFade : 0);
        const int y1 = i + 1 == regions.size() ? (1 << 28) : r.y1 + yOffset + (fadeBottom ? kFade : 0);
        // sem cortar em cima: a faixa do título (y negativo) também recebe o fundo
        const QRect area = QRect(0, qMax(y0, -(1 << 20)), vp.width(), qMin(y1, 1 << 20) - qMax(y0, -(1 << 20))).intersected(clip);
        if (area.isEmpty()) continue;
        QImage layer(area.size(), QImage::Format_ARGB32_Premultiplied);
        layer.fill(Qt::transparent);
        {
            QPainter lp(&layer);
            lp.setRenderHint(QPainter::SmoothPixmapTransform);
            lp.translate(-area.topLeft());
            drawContent(lp, r.bg, area, top, vp);
            lp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            if (fadeTop) {
                QLinearGradient g(0, y0, 0, y0 + 2 * kFade);
                g.setColorAt(0, Qt::transparent);
                g.setColorAt(1, Qt::black);
                lp.fillRect(QRect(0, y0, vp.width(), 2 * kFade), g);
            }
            if (fadeBottom) {
                QLinearGradient g(0, y1 - 2 * kFade, 0, y1);
                g.setColorAt(0, Qt::black);
                g.setColorAt(1, Qt::transparent);
                lp.fillRect(QRect(0, y1 - 2 * kFade, vp.width(), 2 * kFade), g);
            }
        }
        p.save();
        p.setOpacity(qBound(0, r.bg.opacity, 100) / 100.0);
        p.drawImage(area.topLeft(), layer);
        p.restore();
    }
}

QString PageBg::pickImage(QWidget* parent)
{
    const QString path = QFileDialog::getOpenFileName(parent, PageBgField::tr("Selecionar imagem de fundo"), QString(),
                                                      PageBgField::tr("Imagens (*.png *.jpg *.jpeg *.gif *.bmp *.webp)"));
    if (path.isEmpty()) return QString();
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) return QString();
    if (qMax(img.width(), img.height()) > kMaxSide)
        img = img.scaled(kMaxSide, kMaxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (img.hasAlphaChannel()) {
        // JPEG não tem transparência: o que era transparente vira papel branco
        QImage flat(img.size(), QImage::Format_RGB32);
        flat.fill(Qt::white);
        QPainter p(&flat);
        p.drawImage(0, 0, img);
        img = flat;
    }
    return toDataUrl(img);
}

// ─────────────────────────── a faixa de prévia ───────────────────────────

// A faixa da seção: a folha com o fundo na opacidade escolhida e o título por
// cima. Vazia, vira um tracejado com um texto. Com fundo próprio, as pílulas
// trocar · cor/imagem · remover ficam no canto.
class PageBgStrip : public QWidget {
public:
    explicit PageBgStrip(QWidget* parent) : QWidget(parent) { setFixedHeight(78); }
    void set(const PageBackground& bg, const QString& title, const QString& tag, const QString& emptyText, bool dim)
    {
        m_bg = bg; m_title = title; m_tag = tag; m_empty = emptyText; m_dim = dim;
        update();
    }
    QToolButton* addPill(const QString& text)
    {
        auto* b = new QToolButton(this);
        b->setText(text);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setFont(uiFont(10.5));
        b->setFixedHeight(19);
        b->setStyleSheet(QStringLiteral(
            "QToolButton { background: rgba(0,0,0,0.5); color: #ffffff; border: none; border-radius: 9px; padding: 0 8px; }"
            "QToolButton:hover { background: rgba(0,0,0,0.68); }"));
        m_pills << b;
        return b;
    }
    void layoutPills()
    {
        int x = width() - 8;
        for (int i = m_pills.size() - 1; i >= 0; --i) {
            QToolButton* b = m_pills[i];
            if (b->isHidden()) continue;
            b->adjustSize();
            x -= b->width();
            b->move(x, 8);
            x -= 5;
        }
    }

protected:
    void resizeEvent(QResizeEvent*) override { layoutPills(); }
    void paintEvent(QPaintEvent*) override
    {
        const Palette pal = Palette::onPage();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath box;
        box.addRoundedRect(r, 8, 8);
        if (!m_empty.isEmpty()) {
            p.fillPath(box, alpha(pal.ink, 0.04));
            QPen pen(alpha(pal.ink, 0.28), 1.2, Qt::DashLine);
            p.setPen(pen);
            p.drawPath(box);
            p.setPen(pal.dim);
            p.setFont(uiFont(12.5));
            p.drawText(rect(), Qt::AlignCenter, m_empty);
            return;
        }
        p.setClipPath(box);
        p.fillRect(rect(), pal.page);
        p.setOpacity(qBound(0, m_bg.opacity, 100) / 100.0 * (m_dim ? 0.8 : 1.0));
        if (m_bg.mode == QLatin1String("color")) {
            p.fillRect(rect(), QColor(m_bg.color));
        } else {
            const QImage src = sourceImage(m_bg.image);
            if (!src.isNull()) {
                const qreal dpr = devicePixelRatioF();
                QImage sc = src.scaled(size() * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                sc = blurred(sc, m_bg.blur);
                p.drawImage(QRectF(rect()), sc, QRectF((sc.width() - width() * dpr) / 2, (sc.height() - height() * dpr) / 2,
                                                       width() * dpr, height() * dpr));
            }
        }
        p.setOpacity(1.0);
        p.setPen(pal.ink);
        p.setFont(serifFont(15, QFont::Bold));
        p.drawText(QRectF(10, 0, width() - 20, height() - 7), Qt::AlignLeft | Qt::AlignBottom, m_title);
        if (!m_tag.isEmpty()) {
            const QFont f = monoFont(10.5);
            p.setFont(f);
            const QRectF tr(9, 8, QFontMetricsF(f).horizontalAdvance(m_tag) + 10, 16);
            p.setPen(Qt::NoPen);
            p.setBrush(alpha(pal.page, 0.8));
            p.drawRoundedRect(tr, 3, 3);
            p.setPen(pal.dim);
            p.drawText(tr, Qt::AlignCenter, m_tag);
        }
        p.setClipping(false);
        p.setPen(QPen(alpha(pal.ink, 0.18), 1));
        p.setBrush(Qt::NoBrush);
        p.drawPath(box);
    }

private:
    PageBackground m_bg;
    QString m_title, m_tag, m_empty;
    bool m_dim = false;
    QList<QToolButton*> m_pills;
};

// ─────────────────────────── a seção ───────────────────────────

namespace {

QToolButton* link(const QString& text, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setObjectName(QStringLiteral("sheetLink"));
    b->setText(text);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

QToolButton* dashed(const QString& text, QWidget* parent)
{
    const Palette pal = Palette::onPage();
    auto* b = new QToolButton(parent);
    b->setText(text);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    b->setFixedHeight(46);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    b->setFont(uiFont(12.5));
    b->setStyleSheet(QStringLiteral(
        "QToolButton { background: %1; color: %2; border: 1px dashed %3; border-radius: 8px; }"
        "QToolButton:hover { color: %4; border-color: %4; }")
        .arg(alpha(pal.ink, 0.04).name(QColor::HexArgb), pal.dim.name(), alpha(pal.ink, 0.28).name(QColor::HexArgb), pal.accent.name()));
    return b;
}

QSlider* slider(int lo, int hi, int v, QWidget* parent)
{
    const Palette pal = Palette::onPage();
    auto* s = new QSlider(Qt::Horizontal, parent);
    s->setRange(lo, hi);
    s->setValue(v);
    s->setFocusPolicy(Qt::NoFocus);
    s->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: %1; }"
        "QSlider::sub-page:horizontal { height: 4px; border-radius: 2px; background: %2; }"
        "QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; background: %2; }")
        .arg(alpha(pal.ink, 0.15).name(QColor::HexArgb), pal.accent.name()));
    return s;
}

QLabel* rowLabel(const QString& t, QWidget* parent)
{
    auto* l = new QLabel(t, parent);
    l->setObjectName(QStringLiteral("sheetDim"));
    l->setFont(uiFont(12));
    l->setFixedWidth(68);
    return l;
}

} // namespace

PageBgField::PageBgField(const PageBackground& current, const PageBackground& inherited, const QString& inheritFrom,
                         bool chapterLevel, const QString& previewTitle, QWidget* parent)
    : QWidget(parent), m_bg(current), m_inherited(inherited), m_chapterLevel(chapterLevel),
      m_inheritFrom(inheritFrom), m_title(previewTitle)
{
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    m_strip = new PageBgStrip(this);
    QToolButton* pSwap = m_strip->addPill(tr("trocar"));
    QToolButton* pKind = m_strip->addPill(QString());
    QToolButton* pRemove = m_strip->addPill(tr("remover"));
    pSwap->setObjectName(QStringLiteral("pbSwap"));
    pKind->setObjectName(QStringLiteral("pbKind"));
    pRemove->setObjectName(QStringLiteral("pbRemove"));
    connect(pSwap, &QToolButton::clicked, this, [this]() {
        if (m_bg.mode == QLatin1String("color")) pickColor(); else pickImage();
    });
    connect(pKind, &QToolButton::clicked, this, [this]() {
        if (m_bg.mode == QLatin1String("color")) pickImage(); else pickColor();
    });
    connect(pRemove, &QToolButton::clicked, this, [this]() { m_bg.mode.clear(); refresh(); });
    v->addWidget(m_strip);

    m_emptyRow = new QWidget(this);
    auto* er = new QHBoxLayout(m_emptyRow);
    er->setContentsMargins(0, 0, 0, 0);
    er->setSpacing(8);
    m_addImage = dashed(tr("＋ Imagem"), m_emptyRow);
    m_addColor = dashed(tr("＋ Cor"), m_emptyRow);
    connect(m_addImage, &QToolButton::clicked, this, &PageBgField::pickImage);
    connect(m_addColor, &QToolButton::clicked, this, &PageBgField::pickColor);
    er->addWidget(m_addImage);
    er->addWidget(m_addColor);
    v->addWidget(m_emptyRow);

    m_links = new QWidget(this);
    auto* lr = new QHBoxLayout(m_links);
    lr->setContentsMargins(0, 0, 0, 0);
    lr->setSpacing(14);
    m_lnOther = link(tr("outra imagem"), m_links);
    m_lnColor = link(tr("uma cor"), m_links);
    m_lnNone = link(tr("sem fundo aqui"), m_links);
    m_lnInherit = link(tr("voltar a herdar"), m_links);
    m_lnClearBook = link(tr("tirar do livro"), m_links);
    for (QToolButton* b : { m_lnOther, m_lnColor, m_lnNone, m_lnInherit, m_lnClearBook }) lr->addWidget(b);
    lr->addStretch(1);
    connect(m_lnOther, &QToolButton::clicked, this, &PageBgField::pickImage);
    connect(m_lnColor, &QToolButton::clicked, this, &PageBgField::pickColor);
    connect(m_lnNone, &QToolButton::clicked, this, [this]() { m_bg.mode = QStringLiteral("none"); refresh(); });
    connect(m_lnInherit, &QToolButton::clicked, this, [this]() { m_bg.mode.clear(); refresh(); });
    connect(m_lnClearBook, &QToolButton::clicked, this, [this]() { m_clearInherited = true; m_inherited = PageBackground(); refresh(); });
    v->addWidget(m_links);

    m_opRow = new QWidget(this);
    auto* orow = new QHBoxLayout(m_opRow);
    orow->setContentsMargins(0, 0, 0, 0);
    orow->setSpacing(10);
    orow->addWidget(rowLabel(tr("Opacidade"), m_opRow));
    m_op = slider(0, 100, m_bg.opacity, m_opRow);
    orow->addWidget(m_op, 1);
    m_opVal = new QLabel(m_opRow);
    m_opVal->setObjectName(QStringLiteral("sheetDim"));
    m_opVal->setFont(monoFont(11.5));
    m_opVal->setFixedWidth(36);
    m_opVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    orow->addWidget(m_opVal);
    connect(m_op, &QSlider::valueChanged, this, [this](int val) {
        m_bg.opacity = val;
        m_opVal->setText(QStringLiteral("%1%").arg(val));
        m_strip->set(shown(), m_title, QString(), QString(), false);
    });
    v->addWidget(m_opRow);

    m_moreBtn = link(QString(), this);
    connect(m_moreBtn, &QToolButton::clicked, this, [this]() { m_more = !m_more; refresh(); });
    v->addWidget(m_moreBtn, 0, Qt::AlignLeft);

    m_moreBox = new QWidget(this);
    auto* mv = new QVBoxLayout(m_moreBox);
    mv->setContentsMargins(0, 0, 0, 0);
    mv->setSpacing(7);
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(10);
        row->addWidget(rowLabel(tr("Desfoque"), m_moreBox));
        m_blur = slider(0, 12, m_bg.blur, m_moreBox);
        row->addWidget(m_blur, 1);
        row->addSpacing(46);
        connect(m_blur, &QSlider::valueChanged, this, [this](int val) {
            m_bg.blur = val;
            m_strip->set(shown(), m_title, QString(), QString(), false);
        });
        mv->addLayout(row);
    }
    auto pillRow = [this, mv](const QString& label, const QStringList& texts, QButtonGroup*& group) {
        auto* row = new QHBoxLayout;
        row->setSpacing(5);
        row->addWidget(rowLabel(label, m_moreBox));
        group = new QButtonGroup(this);
        group->setExclusive(true);
        for (int i = 0; i < texts.size(); ++i) {
            QToolButton* b = SheetDialog::pill(texts[i], m_moreBox);
            group->addButton(b, i);
            row->addWidget(b);
        }
        row->addStretch(1);
        mv->addLayout(row);
    };
    pillRow(tr("Ocupa"), { tr("folha toda"), tr("repetir"), tr("só no topo") }, m_fit);
    pillRow(tr("Ao rolar"), { tr("rola junto"), tr("fica parada") }, m_scroll);
    connect(m_fit, &QButtonGroup::idClicked, this, [this](int id) {
        static const char* fits[] = { "cover", "tile", "top" };
        m_bg.fit = QString::fromLatin1(fits[qBound(0, id, 2)]);
    });
    connect(m_scroll, &QButtonGroup::idClicked, this, [this](int id) { m_bg.fixed = id == 1; });
    v->addWidget(m_moreBox);

    m_all = new QCheckBox(tr("Usar em todos os capítulos"), this);
    m_all->setObjectName(QStringLiteral("sheetChk"));
    m_all->setToolTip(tr("Vira o fundo do livro: os capítulos sem fundo próprio passam a usar este."));
    v->addWidget(m_all);

    refresh();
}

bool PageBgField::useForAll() const
{
    return m_chapterLevel && m_all->isVisible() && m_all->isChecked() && m_bg.isSet();
}

PageBackground PageBgField::shown() const
{
    if (m_bg.isSet()) return m_bg;
    if (m_bg.mode.isEmpty() && m_inherited.isSet()) return m_inherited;
    return PageBackground();
}

void PageBgField::refresh()
{
    const bool own = m_bg.isSet();
    const bool none = m_bg.mode == QLatin1String("none");
    const bool inheriting = !own && !none && m_inherited.isSet();
    const bool color = m_bg.mode == QLatin1String("color");
    const QString inheritTag = m_inheritFrom == QLatin1String("book") ? tr("herda do livro")
                             : m_inheritFrom == QLatin1String("theme") ? tr("herda do tema") : tr("herda do capítulo");

    if (own) m_strip->set(m_bg, m_title, QString(), QString(), false);
    else if (inheriting) m_strip->set(m_inherited, m_title, inheritTag, QString(), true);
    else if (none && m_inherited.isSet())
        m_strip->set(PageBackground(), m_title, QString(),
                     m_chapterLevel ? tr("sem fundo neste capítulo") : tr("sem fundo nesta cena"), false);
    // (isVisible() mente antes de a folha abrir: decide pela conta)
    const bool stripOn = own || inheriting || (none && m_inherited.isSet());
    m_strip->setVisible(stripOn);
    m_emptyRow->setVisible(!stripOn);

    if (auto* b = m_strip->findChild<QToolButton*>(QStringLiteral("pbSwap"))) b->setVisible(own);
    if (auto* b = m_strip->findChild<QToolButton*>(QStringLiteral("pbKind"))) { b->setVisible(own); b->setText(color ? tr("imagem") : tr("cor")); }
    if (auto* b = m_strip->findChild<QToolButton*>(QStringLiteral("pbRemove"))) b->setVisible(own);
    m_strip->layoutPills();

    m_lnOther->setVisible(inheriting);
    m_lnColor->setVisible(inheriting);
    m_lnNone->setVisible(inheriting);
    m_lnInherit->setVisible(none && m_inherited.isSet());
    m_lnClearBook->setVisible(inheriting && m_inheritFrom == QLatin1String("book"));
    m_links->setVisible(inheriting || (none && m_inherited.isSet()));

    m_opRow->setVisible(own);
    m_op->blockSignals(true);
    m_op->setValue(m_bg.opacity);
    m_op->blockSignals(false);
    m_opVal->setText(QStringLiteral("%1%").arg(m_bg.opacity));
    // desfoque, ocupa e rolar só fazem sentido pra imagem
    m_moreBtn->setVisible(own && !color);
    m_moreBtn->setText(m_more ? tr("menos ajustes ▴") : tr("mais ajustes ▾"));
    m_moreBox->setVisible(own && !color && m_more);
    m_blur->blockSignals(true);
    m_blur->setValue(m_bg.blur);
    m_blur->blockSignals(false);
    const int fitId = m_bg.fit == QLatin1String("tile") ? 1 : m_bg.fit == QLatin1String("top") ? 2 : 0;
    if (QAbstractButton* b = m_fit->button(fitId)) b->setChecked(true);
    if (QAbstractButton* b = m_scroll->button(m_bg.fixed ? 1 : 0)) b->setChecked(true);
    m_all->setVisible(own && m_chapterLevel);
    emit sizeChanged();
}

void PageBgField::pickImage()
{
    const QString url = PageBg::pickImage(window());
    if (url.isEmpty()) return;
    // imagem: começa discreta (22%), pra o texto continuar mandando
    if (m_bg.mode != QLatin1String("image")) m_bg.opacity = 22;
    m_bg.mode = QStringLiteral("image");
    m_bg.image = url;
    refresh();
}

void PageBgField::pickColor()
{
    const QColor start = m_bg.color.isEmpty() ? Palette::onPage().accent : QColor(m_bg.color);
    const QColor c = ColorPopover::getColor(start, window(), tr("Cor do fundo"));
    if (!c.isValid()) return;
    // cor sólida: começa cobrindo a folha inteira; o slider clareia
    if (m_bg.mode != QLatin1String("color")) m_bg.opacity = 100;
    m_bg.mode = QStringLiteral("color");
    m_bg.color = c.name();
    refresh();
}
