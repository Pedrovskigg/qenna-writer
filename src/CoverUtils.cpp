#include "CoverUtils.h"

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QImageReader>

namespace CoverUtils {

QString loadCoverAsDataUrl(const QString& path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) return QString();
    if (img.width() > kMaxCoverSide || img.height() > kMaxCoverSide) {
        img = img.scaled(kMaxCoverSide, kMaxCoverSide,
                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG", kJpegQuality);
    return QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(bytes.toBase64());
}

QPixmap pixmapFromDataUrl(const QString& dataUrl) {
    if (dataUrl.isEmpty()) return QPixmap();
    const int comma = dataUrl.indexOf(QLatin1Char(','));
    if (comma < 0) return QPixmap();
    const QByteArray raw = QByteArray::fromBase64(dataUrl.mid(comma + 1).toLatin1());
    QPixmap pm;
    pm.loadFromData(raw);
    return pm;
}

} // namespace CoverUtils
