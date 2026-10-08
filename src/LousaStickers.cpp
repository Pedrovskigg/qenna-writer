#include "LousaStickers.h"

#include <cmath>
#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QPainter>
#include <QSettings>
#include <QStandardPaths>
#include <QSvgRenderer>

namespace LousaStickers {

namespace {

// Desenhos da cartela (os mesmos da prévia aprovada, leva 2). Os carimbos e o
// "?" são desenhados com texto, mais abaixo, porque o texto muda com o idioma.
QByteArray svgOf(const QString& id)
{
    if (id == QStringLiteral("circle"))
        return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><path d="M24 58C20 34 42 20 64 22c22 3 28 24 20 42-9 19-40 24-58 10-9-8-6-24 8-32" fill="none" stroke="#d7263d" stroke-width="3.6" stroke-linecap="round" stroke-opacity="0.92"/></svg>)";
    if (id == QStringLiteral("arrow"))
        return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 60"><path d="M6 48C30 50 58 40 86 14" fill="none" stroke="#d7263d" stroke-width="4" stroke-linecap="round"/><path d="M70 12 88 12 86 30" fill="none" stroke="#d7263d" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/></svg>)";
    if (id == QStringLiteral("cross"))
        return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><path d="M18 20C40 42 62 64 84 82M82 16C60 40 40 62 20 86" fill="none" stroke="#d7263d" stroke-width="5" stroke-linecap="round"/></svg>)";
    if (id == QStringLiteral("underline"))
        return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 120 30"><path d="M4 14C30 10 70 12 116 8M10 22C40 18 80 20 112 17" fill="none" stroke="#d7263d" stroke-width="3.4" stroke-linecap="round"/></svg>)";
    if (id == QStringLiteral("tape"))
        return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 34"><polygon points="0,4 5,0 9,5 13,1 17,4 83,2 87,0 91,5 95,1 100,4 100,30 95,34 91,29 87,33 83,30 17,32 13,34 9,29 5,33 0,30" fill="#ece2c4" fill-opacity="0.88"/></svg>)";
    if (id == QStringLiteral("clip"))
        return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 40 100"><path d="M26 30V78a8 8 0 0 1-16 0V18a12 12 0 0 1 24 0v58" fill="none" stroke="#8d9399" stroke-width="4" stroke-linecap="round"/></svg>)";
    if (id == QStringLiteral("coffee"))
        return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><g fill="none" stroke="#8b5a2b"><circle cx="50" cy="50" r="38" stroke-width="5" stroke-opacity="0.30"/><circle cx="51" cy="51" r="36" stroke-width="2" stroke-opacity="0.38"/><path d="M14 46c2-18 18-32 36-32" stroke-width="7" stroke-opacity="0.18"/></g><circle cx="84" cy="80" r="4" fill="#8b5a2b" fill-opacity="0.26"/><circle cx="90" cy="70" r="2" fill="#8b5a2b" fill-opacity="0.26"/></svg>)";
    if (id == QStringLiteral("fingerprint"))
        return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 80 100"><g fill="none" stroke="#3a3530" stroke-width="2.2" stroke-opacity="0.7" stroke-linecap="round"><path d="M40 12c-16 0-28 14-28 34 0 14 4 26 10 36"/><path d="M40 20c-12 0-21 11-21 26 0 14 4 25 9 34"/><path d="M40 28c-8 0-14 8-14 18 0 14 4 24 8 32"/><path d="M40 36c-4 0-7 4-7 10 0 12 3 22 7 30"/><path d="M40 12c16 0 28 14 28 34 0 10-2 22-6 32"/><path d="M40 20c12 0 21 11 21 26 0 10-2 20-5 30"/><path d="M40 28c8 0 14 8 14 18 0 10-1 18-4 26"/><path d="M40 36c4 0 7 4 7 10 0 8-1 16-3 22"/><path d="M40 44v28"/></g></svg>)";
    if (id == QStringLiteral("magnifier"))
        return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><circle cx="40" cy="40" r="26" fill="#cfe3ef" fill-opacity="0.45" stroke="#4a3b2e" stroke-width="7"/><path d="M60 60 86 86" stroke="#6b4a2e" stroke-width="12" stroke-linecap="round"/><path d="M28 30a14 14 0 0 1 10-8" stroke="#ffffff" stroke-width="3" fill="none" stroke-linecap="round"/></svg>)";
    return {};
}

// Tinta de carimbo: falhas aqui e ali, sempre as mesmas pro mesmo texto.
void inkTexture(QImage& img, quint32 seed)
{
    img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int sw = qMax(4, img.width() / 3), sh = qMax(4, img.height() / 3);
    QImage noise(sw, sh, QImage::Format_RGB32);
    quint32 s = seed ? seed : 7u;
    for (int y = 0; y < sh; ++y) {
        auto* line = reinterpret_cast<QRgb*>(noise.scanLine(y));
        for (int x = 0; x < sw; ++x) {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            const int v = int(s % 256u);
            line[x] = qRgb(v, v, v);
        }
    }
    const QImage big = noise.scaled(img.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                            .convertToFormat(QImage::Format_RGB32);
    for (int y = 0; y < img.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        const auto* nl = reinterpret_cast<const QRgb*>(big.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const int n = qRed(nl[x]);
            const qreal f = n < 62 ? 0.12 : (n < 96 ? 0.55 : 0.92);
            const QRgb c = line[x];
            line[x] = qRgba(int(qRed(c) * f), int(qGreen(c) * f), int(qBlue(c) * f), int(qAlpha(c) * f));
        }
    }
}

QImage stamp(const QString& text, const QColor& ink, bool doubleBorder, int longestPx)
{
    QFont f(QStringLiteral("Arial Black"));
    f.setPixelSize(100);
    f.setLetterSpacing(QFont::AbsoluteSpacing, text.size() > 8 ? 6 : 14);
    const qreal tw = QFontMetricsF(f).horizontalAdvance(text);
    const qreal W = tw + 120.0, H = 190.0;
    const qreal sc = longestPx / W;
    QImage img(qMax(1, qRound(W * sc)), qMax(1, qRound(H * sc)), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.scale(sc, sc);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(ink, 14));
    p.drawRoundedRect(QRectF(10, 10, W - 20, H - 20), 16, 16);
    if (doubleBorder) {
        p.setPen(QPen(ink, 5));
        p.drawRect(QRectF(32, 32, W - 64, H - 64));
    }
    p.setFont(f);
    p.setPen(ink);
    p.drawText(QRectF(0, 0, W, H), Qt::AlignCenter, text);
    p.end();
    inkTexture(img, qHash(text));
    return img;
}

QImage question(int longestPx)
{
    const qreal W = 60.0, H = 100.0;
    const qreal sc = longestPx / H;
    QImage img(qRound(W * sc), qRound(H * sc), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.scale(sc, sc);
    const QStringList fams = QFontDatabase::families();
    QFont f(fams.contains(QStringLiteral("Ink Free")) ? QStringLiteral("Ink Free") : QStringLiteral("Segoe Print"));
    f.setPixelSize(96);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(0xd7, 0x26, 0x3d));
    p.drawText(QRectF(0, 0, W, H), Qt::AlignCenter, QStringLiteral("?"));
    p.end();
    return img;
}

QString recentDir()
{
    const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                    + QStringLiteral("/lousa-adesivos");
    QDir().mkpath(d);
    return d;
}

} // namespace

QStringList markIds()
{
    return { QStringLiteral("circle"), QStringLiteral("arrow"), QStringLiteral("cross"),
             QStringLiteral("question"), QStringLiteral("underline"), QStringLiteral("tape") };
}

QStringList objectIds()
{
    return { QStringLiteral("clue"), QStringLiteral("confidential"), QStringLiteral("fingerprint"),
             QStringLiteral("magnifier"), QStringLiteral("clip"), QStringLiteral("coffee") };
}

QImage render(const QString& id, int longestPx)
{
    if (id == QStringLiteral("clue"))
        return stamp(QCoreApplication::translate("LousaStickers", "PISTA"), QColor(0xc0, 0x39, 0x2b), true, longestPx);
    if (id == QStringLiteral("confidential"))
        return stamp(QCoreApplication::translate("LousaStickers", "CONFIDENCIAL"), QColor(0x1f, 0x4e, 0x8c), false, longestPx);
    if (id == QStringLiteral("question"))
        return question(longestPx);
    const QByteArray svg = svgOf(id);
    if (svg.isEmpty()) return {};
    QSvgRenderer r(svg);
    const QRectF vb = r.viewBoxF();
    if (!r.isValid() || vb.isEmpty()) return {};
    const qreal sc = longestPx / qMax(vb.width(), vb.height());
    QImage img(qMax(1, qRound(vb.width() * sc)), qMax(1, qRound(vb.height() * sc)),
               QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    r.render(&p);
    p.end();
    return img;
}

QString label(const QString& id)
{
    if (id == QStringLiteral("circle"))       return QCoreApplication::translate("LousaStickers", "Círculo de caneta");
    if (id == QStringLiteral("arrow"))        return QCoreApplication::translate("LousaStickers", "Seta");
    if (id == QStringLiteral("cross"))        return QCoreApplication::translate("LousaStickers", "X de caneta");
    if (id == QStringLiteral("question"))     return QCoreApplication::translate("LousaStickers", "Ponto de interrogação");
    if (id == QStringLiteral("underline"))    return QCoreApplication::translate("LousaStickers", "Sublinhado");
    if (id == QStringLiteral("tape"))         return QCoreApplication::translate("LousaStickers", "Fita adesiva");
    if (id == QStringLiteral("clue"))         return QCoreApplication::translate("LousaStickers", "Carimbo: pista");
    if (id == QStringLiteral("confidential")) return QCoreApplication::translate("LousaStickers", "Carimbo: confidencial");
    if (id == QStringLiteral("fingerprint"))  return QCoreApplication::translate("LousaStickers", "Impressão digital");
    if (id == QStringLiteral("magnifier"))    return QCoreApplication::translate("LousaStickers", "Lupa");
    if (id == QStringLiteral("clip"))         return QCoreApplication::translate("LousaStickers", "Clipe");
    if (id == QStringLiteral("coffee"))       return QCoreApplication::translate("LousaStickers", "Mancha de café");
    return {};
}

QString defaultOutline(const QString& id)
{
    if (id == QStringLiteral("magnifier") || id == QStringLiteral("clip")) return QStringLiteral("shadow");
    return QStringLiteral("none");   // caneta, tinta e mancha ficam no papel
}

qreal defaultSize(const QString& id)
{
    if (id == QStringLiteral("circle"))       return 170;
    if (id == QStringLiteral("underline"))    return 170;
    if (id == QStringLiteral("tape"))         return 110;
    if (id == QStringLiteral("clue"))         return 150;
    if (id == QStringLiteral("confidential")) return 190;
    if (id == QStringLiteral("question"))     return 90;
    if (id == QStringLiteral("clip"))         return 80;
    return 130;
}

void remember(const QImage& image)
{
    if (image.isNull()) return;
    QImage img = image;
    if (qMax(img.width(), img.height()) > 1000)
        img = img.scaled(1000, 1000, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    const QString name = QString::fromLatin1(QCryptographicHash::hash(png, QCryptographicHash::Sha1).toHex().left(20))
                       + QStringLiteral(".png");
    const QString dir = recentDir();
    const QString path = dir + QLatin1Char('/') + name;
    if (!QFile::exists(path)) {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) f.write(png);
    }
    QSettings st;
    QStringList list = st.value(QStringLiteral("lousa/recentStickers")).toStringList();
    list.removeAll(name);
    list.prepend(name);
    while (list.size() > 24) QFile::remove(dir + QLatin1Char('/') + list.takeLast());
    st.setValue(QStringLiteral("lousa/recentStickers"), list);
}

QList<Recent> recent()
{
    QList<Recent> out;
    const QString dir = recentDir();
    QSettings st;
    for (const QString& name : st.value(QStringLiteral("lousa/recentStickers")).toStringList()) {
        const QString path = dir + QLatin1Char('/') + name;
        QImage img(path);
        if (!img.isNull()) out.append({ path, img });
    }
    return out;
}

bool hasTransparency(const QImage& image)
{
    if (image.isNull() || !image.hasAlphaChannel()) return false;
    const QImage a = image.convertToFormat(QImage::Format_ARGB32);
    const int w = a.width(), h = a.height();
    auto clear = [&a](int x, int y) {
        return qAlpha(reinterpret_cast<const QRgb*>(a.constScanLine(y))[x]) < 250;
    };
    // as bordas inteiras (recorte com fundo transparente quase sempre começa nelas)
    for (int x = 0; x < w; ++x) if (clear(x, 0) || clear(x, h - 1)) return true;
    for (int y = 0; y < h; ++y) if (clear(0, y) || clear(w - 1, y)) return true;
    const int step = qMax(1, int(std::sqrt(double(w) * h / 40000.0)));
    for (int y = 0; y < h; y += step)
        for (int x = 0; x < w; x += step)
            if (clear(x, y)) return true;
    return false;
}

} // namespace LousaStickers
