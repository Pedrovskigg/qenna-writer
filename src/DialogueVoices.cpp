#include "DialogueVoices.h"

#include "AvatarUtils.h"
#include "Theme.h"

#include <QApplication>
#include <QPainter>
#include <algorithm>

namespace DialogueVoices {

QColor color(int index)
{
    static const char* dark[]  = { "#F0A35A", "#6FB3F2", "#F07FB0", "#7CCB7A",
                                   "#B48CF2", "#E8D36A", "#F07A6E", "#5FCFC4" };
    static const char* light[] = { "#B8621A", "#1F6FB8", "#B8337A", "#2E8A3A",
                                   "#6A3FB8", "#8A7400", "#B8342A", "#13807A" };
    constexpr int n = 8;
    if (index < 0) return QColor(Theme::textMuted());
    const bool isDark = QColor(Theme::panelBackground()).lightnessF() < 0.5;
    QColor c(isDark ? dark[index % n] : light[index % n]);
    if ((index / n) % 2 == 1) c = isDark ? c.lighter(125) : c.darker(130);
    return c;
}

QHash<QString, int> rankForManuscript(const QVector<DialogueStore::Dialogue>& all,
                                      const QString& manuscriptId)
{
    QHash<QString, int> lines;
    for (const DialogueStore::Dialogue& d : all)
        if (!d.characterId.isEmpty() && d.manuscriptId == manuscriptId) lines[d.characterId] += 1;
    QStringList ids = lines.keys();
    std::sort(ids.begin(), ids.end(), [&lines](const QString& a, const QString& b) {
        const int la = lines.value(a), lb = lines.value(b);
        return la != lb ? la > lb : a < b;
    });
    QHash<QString, int> rank;
    for (int i = 0; i < ids.size(); ++i) rank.insert(ids.at(i), i);
    return rank;
}

QPixmap face(const QString& imageDataUrl, const QString& name, const QColor& voice, int size)
{
    const qreal dpr = qApp->devicePixelRatio();
    const int px = qRound(size * dpr);
    if (!AvatarUtils::decodeDataUrl(imageDataUrl).isNull()) {
        QPixmap pm = AvatarUtils::circularAvatar(imageDataUrl, name, QString(), px);
        pm.setDevicePixelRatio(dpr);
        return pm;
    }
    QPixmap pm(px, px);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(voice);
    p.drawEllipse(QRectF(0, 0, size, size));
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(qMax(9, int(size * 0.4)));
    p.setFont(f);
    p.setPen(voice.lightnessF() > 0.6 ? QColor(0, 0, 0, 170) : QColor(255, 255, 255, 235));
    const QString trimmed = name.trimmed();
    p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter,
               trimmed.isEmpty() ? QStringLiteral("?") : trimmed.left(1).toUpper());
    return pm;
}

} // namespace DialogueVoices
