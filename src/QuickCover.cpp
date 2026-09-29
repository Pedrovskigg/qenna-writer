#include "QuickCover.h"

#include "BackgroundOverlay.h"
#include "CoverUtils.h"
#include "Theme.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFontMetricsF>
#include <QHash>
#include <QImageReader>
#include <QJsonArray>
#include <QPainter>

namespace QuickCover {

namespace {

// Mesma procura do Theme: ao lado do exe (instalado) e depois a pasta de dev.
QString imagesDir()
{
    const QString prod = QCoreApplication::applicationDirPath() + QStringLiteral("/theme-images");
    if (QDir(prod).exists()) return prod;
#ifdef DEV_ASSETS_DIR
    const QString dev = QString::fromUtf8(DEV_ASSETS_DIR) + QStringLiteral("/theme-images");
    if (QDir(dev).exists()) return dev;
#endif
    return QString();
}

// Corta no meio pra proporção da capa (2:3) e reduz pra size.
QImage coverCrop(const QImage& src, const QSize& size)
{
    if (src.isNull() || size.isEmpty()) return QImage();
    const qreal want = qreal(size.width()) / size.height();
    QRect r = src.rect();
    if (qreal(r.width()) / r.height() > want) {
        const int w = qRound(r.height() * want);
        r = QRect((r.width() - w) / 2, 0, w, r.height());
    } else {
        const int h = qRound(r.width() / want);
        r = QRect(0, (r.height() - h) / 2, r.width(), h);
    }
    return src.copy(r).scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

// A foto já cortada em 800×1200 (o tamanho máximo das capas). As fotos dos
// temas têm 1920px ou mais: decodificar e cortar toda vez que a prévia
// repinta (arrastando um texto) travaria; guarda as últimas usadas.
QImage sourceImage(const Spec& s)
{
    static QHash<QString, QImage> cache;
    static QStringList order;
    QString key = s.image;
    if (key == QLatin1String("custom")) key = QStringLiteral("custom:") + QString::number(qHash(s.customImage));
    if (key.isEmpty()) return QImage();
    auto it = cache.find(key);
    if (it != cache.end()) return it.value();

    QImage raw;
    if (s.image == QLatin1String("custom")) {
        raw = CoverUtils::pixmapFromDataUrl(s.customImage).toImage();
    } else {
        QImageReader reader(imagesDir() + QLatin1Char('/') + s.image);
        reader.setAutoTransform(true);
        raw = reader.read();
    }
    const QImage img = coverCrop(raw, QSize(800, 1200));
    cache.insert(key, img);
    order << key;
    while (order.size() > 6) cache.remove(order.takeFirst());
    return img;
}

QColor colorFrom(const QJsonValue& v, const QColor& fallback)
{
    const QColor c(v.toString());
    return c.isValid() ? c : fallback;
}

// Sombra macia atrás de um texto: a camada do texto reduzida e ampliada de
// volta (o borrão sai de graça na interpolação), tingida na cor da sombra.
QImage softShadow(const QImage& layer, const QColor& color)
{
    QImage tinted = layer;
    {
        QPainter p(&tinted);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(tinted.rect(), color);
    }
    const QSize small(qMax(1, layer.width() / 6), qMax(1, layer.height() / 6));
    return tinted.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                 .scaled(layer.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

bool isLight(const QColor& c)
{
    return (c.red() * 299 + c.green() * 587 + c.blue() * 114) / 1000 > 140;
}

} // namespace

Spec defaults(const QString& family)
{
    Spec s;
    Text title;
    title.role = QStringLiteral("title");
    title.family = family;
    title.size = 36;
    title.bold = true;
    title.pos = QPointF(0.5, 0.72);
    Text author;
    author.role = QStringLiteral("author");
    author.family = family;
    author.size = 12;
    author.pos = QPointF(0.5, 0.9);
    author.spacing = 0.16;
    author.upper = true;
    s.texts = { title, author };
    return s;
}

QJsonObject toJson(const Spec& s)
{
    QJsonObject o;
    o.insert(QStringLiteral("image"), s.image);
    if (!s.customImage.isEmpty()) o.insert(QStringLiteral("customImage"), s.customImage);
    o.insert(QStringLiteral("bg"), s.bg.name());
    o.insert(QStringLiteral("fadeType"), s.fadeType);
    o.insert(QStringLiteral("fadeColor"), s.fadeColor.name());
    o.insert(QStringLiteral("fadeOpacity"), s.fadeOpacity);
    o.insert(QStringLiteral("fadeSize"), s.fadeSize);
    o.insert(QStringLiteral("grain"), s.grain);
    o.insert(QStringLiteral("grainSize"), s.grainSize);
    QJsonArray texts;
    for (const Text& t : s.texts) {
        QJsonObject j;
        j.insert(QStringLiteral("role"), t.role);
        if (t.role == QLatin1String("extra")) j.insert(QStringLiteral("text"), t.text);
        j.insert(QStringLiteral("family"), t.family);
        j.insert(QStringLiteral("size"), t.size);
        j.insert(QStringLiteral("x"), t.pos.x());
        j.insert(QStringLiteral("y"), t.pos.y());
        j.insert(QStringLiteral("bold"), t.bold);
        j.insert(QStringLiteral("italic"), t.italic);
        j.insert(QStringLiteral("underline"), t.underline);
        j.insert(QStringLiteral("color"), t.color.name());
        j.insert(QStringLiteral("spacing"), t.spacing);
        j.insert(QStringLiteral("upper"), t.upper);
        texts.append(j);
    }
    o.insert(QStringLiteral("texts"), texts);
    return o;
}

Spec fromJson(const QJsonObject& o, const QString& fallbackFamily)
{
    Spec s = defaults(fallbackFamily);
    if (o.isEmpty()) return s;
    s.image = o.value(QStringLiteral("image")).toString();
    s.customImage = o.value(QStringLiteral("customImage")).toString();
    s.bg = colorFrom(o.value(QStringLiteral("bg")), s.bg);
    s.fadeType = qBound(0, o.value(QStringLiteral("fadeType")).toInt(s.fadeType), 4);
    s.fadeColor = colorFrom(o.value(QStringLiteral("fadeColor")), s.fadeColor);
    s.fadeOpacity = qBound(0, o.value(QStringLiteral("fadeOpacity")).toInt(s.fadeOpacity), 100);
    s.fadeSize = qBound(5, o.value(QStringLiteral("fadeSize")).toInt(s.fadeSize), 100);
    s.grain = qBound(0, o.value(QStringLiteral("grain")).toInt(s.grain), 100);
    s.grainSize = qBound(1, o.value(QStringLiteral("grainSize")).toInt(s.grainSize), 12);
    const QJsonArray texts = o.value(QStringLiteral("texts")).toArray();
    if (!texts.isEmpty()) {
        s.texts.clear();
        for (const QJsonValue& v : texts) {
            const QJsonObject j = v.toObject();
            Text t;
            t.role = j.value(QStringLiteral("role")).toString(QStringLiteral("extra"));
            t.text = j.value(QStringLiteral("text")).toString();
            t.family = j.value(QStringLiteral("family")).toString(fallbackFamily);
            t.size = qBound(6.0, j.value(QStringLiteral("size")).toDouble(24), 96.0);
            t.pos = QPointF(qBound(0.0, j.value(QStringLiteral("x")).toDouble(0.5), 1.0),
                            qBound(0.0, j.value(QStringLiteral("y")).toDouble(0.5), 1.0));
            t.bold = j.value(QStringLiteral("bold")).toBool();
            t.italic = j.value(QStringLiteral("italic")).toBool();
            t.underline = j.value(QStringLiteral("underline")).toBool();
            t.color = colorFrom(j.value(QStringLiteral("color")), t.color);
            t.spacing = j.value(QStringLiteral("spacing")).toDouble();
            t.upper = j.value(QStringLiteral("upper")).toBool();
            s.texts.append(t);
        }
    }
    return s;
}

QImage render(const Spec& s, const QSize& size, bool withText,
              const QString& title, const QString& author, QVector<QRectF>* hitRects)
{
    QImage out(size, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    const QRectF r(QPointF(0, 0), QSizeF(size));
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // 1) foto ou cor
    const QImage src = sourceImage(s);
    if (!src.isNull()) {
        p.drawImage(r, src);
    } else {
        QLinearGradient g(0, 0, r.width() * 0.4, r.height());
        g.setColorAt(0, s.bg);
        g.setColorAt(1, s.bg.darker(200));
        p.fillRect(r, g);
    }

    // 2) degradê + grão: o mesmo pintor do fundo dos temas
    Theme::MiraTheme t;
    t.bgOverlayType = s.fadeType;
    t.bgOverlayColor = s.fadeColor.name();
    t.bgOverlayOpacity = s.fadeOpacity;
    t.bgOverlaySize = s.fadeSize;
    t.bgGrain = s.grain;
    t.bgGrainSize = s.grainSize;
    BackgroundOverlay::paint(p, r, t);

    // 3) textos
    const qreal k = r.width() / kRefW;
    if (hitRects) hitRects->clear();
    for (const Text& tx : s.texts) {
        QString str = tx.role == QLatin1String("title") ? title
                    : tx.role == QLatin1String("author") ? author : tx.text;
        if (tx.upper) str = str.toUpper();
        QFont f(tx.family);
        f.setPixelSize(qMax(1, qRound(tx.size * k)));
        f.setBold(tx.bold);
        f.setItalic(tx.italic);
        f.setUnderline(tx.underline);
        if (tx.spacing > 0) f.setLetterSpacing(QFont::AbsoluteSpacing, tx.spacing * tx.size * k);
        const QFontMetricsF fm(f);
        const qreal maxW = 196.0 * k;
        const int flags = Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap;
        QRectF box = fm.boundingRect(QRectF(0, 0, maxW, 100000), flags, str.isEmpty() ? QStringLiteral(" ") : str);
        box.moveCenter(QPointF(tx.pos.x() * r.width(), tx.pos.y() * r.height()));
        if (hitRects) {
            const QRectF ref(box.x() / k, box.y() / k, box.width() / k, box.height() / k);
            hitRects->append(ref.adjusted(-4, -3, 4, 3));
        }
        if (!withText || str.isEmpty()) continue;

        // camada só do texto, pra sombra sair dela
        QImage layer(size, QImage::Format_ARGB32_Premultiplied);
        layer.fill(Qt::transparent);
        {
            QPainter lp(&layer);
            lp.setRenderHint(QPainter::TextAntialiasing);
            lp.setFont(f);
            lp.setPen(tx.color);
            lp.drawText(box, flags, str);
        }
        const QColor shadow = isLight(tx.color) ? QColor(0, 0, 0, 150) : QColor(255, 255, 255, 110);
        p.drawImage(QPointF(0, k), softShadow(layer, shadow));
        p.drawImage(QPointF(0, 0), layer);
    }
    return out;
}

QString toDataUrl(const QImage& img)
{
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.convertToFormat(QImage::Format_RGB32).save(&buf, "JPEG", CoverUtils::kJpegQuality);
    return QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(bytes.toBase64());
}

namespace { QStringList& familiesStore() { static QStringList f; return f; } }
void setBundledFamilies(const QStringList& families) { familiesStore() = families; }
QStringList bundledFamilies() { return familiesStore(); }

QStringList imageNames()
{
    const QString dir = imagesDir();
    if (dir.isEmpty()) return {};
    QStringList names = QDir(dir).entryList({ QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
                                              QStringLiteral("*.png"), QStringLiteral("*.webp") },
                                            QDir::Files, QDir::Name | QDir::IgnoreCase);
    return names;
}

QPixmap thumbnail(const QString& name, const QSize& size)
{
    static QHash<QString, QPixmap> cache;
    const QString key = name + QLatin1Char('@') + QString::number(size.width());
    auto it = cache.find(key);
    if (it != cache.end()) return it.value();
    QImageReader reader(imagesDir() + QLatin1Char('/') + name);
    reader.setAutoTransform(true);
    // decodifica já pequeno (o JPEG reduz na leitura): 92 fotos de 1920px
    // abrindo inteiras travariam a folha
    const QSize full = reader.size();
    if (full.isValid()) {
        const qreal scale = qMax(qreal(size.width() * 2) / full.width(), qreal(size.height() * 2) / full.height());
        if (scale < 1.0) reader.setScaledSize((QSizeF(full) * scale).toSize());
    }
    const QPixmap pm = QPixmap::fromImage(coverCrop(reader.read(), size));
    cache.insert(key, pm);
    return pm;
}

} // namespace QuickCover
