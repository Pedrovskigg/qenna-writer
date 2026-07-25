#include "AvatarUtils.h"

#include <QColor>
#include <QFont>
#include <QHash>
#include <QPainter>
#include <QPainterPath>

namespace AvatarUtils {

QPixmap decodeDataUrl(const QString& dataUrl)
{
    if (dataUrl.isEmpty()) return QPixmap();
    const int comma = dataUrl.indexOf(QLatin1Char(','));
    if (comma < 0) return QPixmap();
    const QByteArray raw = QByteArray::fromBase64(dataUrl.mid(comma + 1).toLatin1());
    QPixmap pm;
    pm.loadFromData(raw);
    return pm;
}

QPixmap circularAvatar(const QString& dataUrl, const QString& name,
                       const QString& idForColor, int size)
{
    const QPixmap photo = decodeDataUrl(dataUrl);

    QPixmap circular(size, size);
    circular.fill(Qt::transparent);
    QPainter p(&circular);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clip;
    clip.addEllipse(0, 0, size, size);
    p.setClipPath(clip);

    if (!photo.isNull()) {
        const QPixmap scaled = photo.scaled(size, size, Qt::KeepAspectRatioByExpanding,
                                             Qt::SmoothTransformation);
        p.drawPixmap((size - scaled.width()) / 2, (size - scaled.height()) / 2, scaled);
    } else {
        const QColor bg = QColor::fromHsv(int(qHash(idForColor) % 360), 120, 165);
        p.fillRect(0, 0, size, size, bg);
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(qMax(9, int(size * 0.42)));
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 235));
        const QString trimmed = name.trimmed();
        const QString initial = trimmed.isEmpty() ? QStringLiteral("?") : trimmed.left(1).toUpper();
        p.drawText(circular.rect(), Qt::AlignCenter, initial);
    }
    return circular;
}

} // namespace AvatarUtils
