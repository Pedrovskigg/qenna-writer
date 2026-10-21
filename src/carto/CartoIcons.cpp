#include "CartoIcons.h"

#include <QHash>
#include <QPainter>
#include <QSvgRenderer>

namespace carto {

namespace {
// miolo do SVG (viewBox 24×24, traço em currentColor)
const QHash<QString, QString>& shapes()
{
    static const QHash<QString, QString> s = {
        { "hand", R"(<path d="M8 12V6.5a1.5 1.5 0 0 1 3 0V11"/><path d="M11 10.5V5a1.5 1.5 0 0 1 3 0v5.5"/><path d="M14 10.5V6.5a1.5 1.5 0 0 1 3 0V14a6 6 0 0 1-6 6h-.6a5 5 0 0 1-4.1-2.2L4 14.3a1.4 1.4 0 0 1 2.2-1.7L8 14.5"/>)" },
        { "area", R"(<rect x="4" y="5" width="16" height="14" rx="1.5" stroke-dasharray="3 2.6"/><path d="M8.5 15.5c1.4-3 2.6-4.4 3.6-4.4s1.3 1.6 2.4 1.6 1.4-1.1 2-2.2"/>)" },
        { "brush", R"(<path d="M14.5 4.5 19.5 9.5 11 18l-5-5z"/><path d="M6 13c-2 .6-2.6 2.3-2.8 4.4-.1 1 .7 1.7 1.6 1.4C7 18.2 9.3 17.4 11 18"/>)" },
        { "clima", R"(<path d="M3 8.5c3-2 6-2 9 0s6 2 9 0"/><path d="M3 14c3-2 6-2 9 0s6 2 9 0" opacity=".65"/><path d="M3 19.5c3-2 6-2 9 0s6 2 9 0" opacity=".4"/>)" },
        { "flag", R"(<path d="M5 21V4"/><path d="M5 4h11l-2 3.5 2 3.5H5"/>)" },
        { "castle", R"(<path d="M4 20V9h3V6h2v3h6V6h2v3h3v11z"/><path d="M10 20v-4a2 2 0 0 1 4 0v4"/>)" },
        { "names", R"(<path d="M4 7V5h11v2"/><path d="M9.5 5v13"/><path d="M7.5 18h4"/><path d="M14 19c2-1.2 4-1.6 6-1.2"/>)" },
        { "ruler", R"(<path d="m3 17 14-14 4 4L7 21z"/><path d="m7 13 2 2M10 10l2 2M13 7l2 2"/>)" },
        { "undo", R"(<path d="M9 7 5 11l4 4"/><path d="M5 11h9a5 5 0 0 1 0 10h-2"/>)" },
        { "redo", R"(<path d="m15 7 4 4-4 4"/><path d="M19 11h-9a5 5 0 0 0 0 10h2"/>)" },
        { "fit", R"(<path d="M4 9V5h4M20 9V5h-4M4 15v4h4M20 15v4h-4"/>)" },
        { "gear", R"(<circle cx="12" cy="12" r="3"/><path d="M12 2.5v3M12 18.5v3M2.5 12h3M18.5 12h3M5.3 5.3l2.1 2.1M16.6 16.6l2.1 2.1M5.3 18.7l2.1-2.1M16.6 7.4l2.1-2.1"/>)" },
        { "gen", R"(<path d="m3 19 6-10 4 6 2.5-3.5L21 19z"/><path d="M8 13.5 9 12l1.2 1.8"/>)" },
        { "land", R"(<path d="M3 18c3-5 5-7 8-7s4 3 6 3 3-1 4-2"/><path d="M3 18h18"/>)" },
        { "sea", R"(<path d="M3 9c2-1.5 3.5-1.5 5 0s3.5 1.5 5 0 3.5-1.5 5 0 2.5 1 3 .8"/><path d="M3 14c2-1.5 3.5-1.5 5 0s3.5 1.5 5 0 3.5-1.5 5 0 2.5 1 3 .8"/>)" },
        { "mountain", R"(<path d="m2 19 6.5-11 4 6.5L15.5 10 22 19z"/><path d="m7 10.6 1.5 1.4 1.4-1.2"/>)" },
        { "smooth", R"(<path d="M3 15c4-6 7 2 10-2s5-3 8-1"/><path d="M3 19h18" opacity=".5"/>)" },
        { "river", R"(<path d="M6 3c3 3-3 5 0 8s5 2 4 6-1 4 2 5"/><path d="M14 3c1.5 2 0 3 1 4.5"/>)" },
        { "wet", R"(<path d="M12 3.5c3.5 4.5 5.5 7.4 5.5 10a5.5 5.5 0 0 1-11 0c0-2.6 2-5.5 5.5-10z"/>)" },
        { "dry", R"(<circle cx="12" cy="12" r="4"/><path d="M12 2.5v2.2M12 19.3v2.2M2.5 12h2.2M19.3 12h2.2M5.3 5.3l1.5 1.5M17.2 17.2l1.5 1.5M5.3 18.7l1.5-1.5M17.2 6.8l1.5-1.5"/>)" },
        { "cold", R"(<path d="M12 2.5v19M3.8 7.2l16.4 9.6M3.8 16.8l16.4-9.6"/><path d="m9.5 4 2.5 2 2.5-2M9.5 20l2.5-2 2.5 2"/>)" },
        { "play", R"(<path d="M8 5v14l11-7z" fill="currentColor" stroke="none"/>)" },
        { "pause", R"(<path d="M7 5h4v14H7zM13 5h4v14h-4z" fill="currentColor" stroke="none"/>)" },
        { "ring", R"(<circle cx="12" cy="12" r="8.5"/><circle cx="12" cy="12" r="3.5"/><path d="M12 3.5v5M12 15.5v5M3.5 12h5M15.5 12h5"/>)" },
        { "up", R"(<path d="m7 14 5-5 5 5"/>)" },
        { "down", R"(<path d="m7 10 5 5 5-5"/>)" },
        { "dice", R"(<rect x="4" y="4" width="16" height="16" rx="3.5"/><circle cx="9" cy="9" r="1.2" fill="currentColor" stroke="none"/><circle cx="15" cy="15" r="1.2" fill="currentColor" stroke="none"/><circle cx="15" cy="9" r="1.2" fill="currentColor" stroke="none"/><circle cx="9" cy="15" r="1.2" fill="currentColor" stroke="none"/><circle cx="12" cy="12" r="1.2" fill="currentColor" stroke="none"/>)" },
        { "map", R"(<path d="M9 4 3 6v14l6-2 6 2 6-2V4l-6 2z"/><path d="M9 4v14M15 6v14"/>)" },
        { "close", R"(<path d="M6 6l12 12M18 6 6 18"/>)" },
        { "plus", R"(<path d="M12 5v14M5 12h14"/>)" },
        { "minus", R"(<path d="M5 12h14"/>)" },
        { "cursor", R"(<path d="M5 3l12 8-5 1.5 3 6-2.4 1.2-3-6L6 17z"/>)" },
        { "tower", R"(<path d="M8 20V7h8v13"/><path d="M7 7V4h2v1.5h2V4h2v1.5h2V4h2v3"/><path d="M12 10v2"/>)" },
        { "lighthouse", R"(<path d="M9 20l1-11h4l1 11z"/><path d="M9.5 9h5V6h-5z"/><path d="M12 3v3M5 6l2 1M19 6l-2 1"/>)" },
        { "windmill", R"(<path d="M10 20l.8-8h2.4l.8 8z"/><path d="M12 9 7 4M12 9l5-5M12 9l-5 5M12 9l5 5"/>)" },
        { "temple", R"(<path d="M4 9 12 4l8 5z"/><path d="M6 9v9M10 9v9M14 9v9M18 9v9M4 20h16"/>)" },
        { "ruins", R"(<path d="M4 20h16"/><path d="M5 20v-7h3v3M11 20v-10h3v5"/><path d="M17 20v-4h2"/>)" },
        { "bridge", R"(<path d="M3 12h18"/><path d="M5 12v6M19 12v6"/><path d="M7 18a5 5 0 0 1 10 0"/>)" },
        { "house", R"(<path d="M3 20v-6l4-3 4 3v6z"/><path d="M13 20v-8l4-3 4 3v8z"/>)" },
        { "field", R"(<path d="M3 16 12 20l9-4-9-4z"/><path d="M6 17.3 15 13.3M9 18.6 18 14.6"/>)" },
        { "wall", R"(<path d="M3 18V11h18v7"/><path d="M3 11V8h3v3M9 11V8h3v3M15 11V8h3v3M21 11V8"/>)" },
        { "road", R"(<path d="M7 21c1-6 3-8 5-11s3-5 3-7"/><path d="M12 21c1-4 3-6 5-9s2-5 2-6"/>)" },
        { "erase", R"(<path d="M5 15 13 7l5 5-8 8H6z"/><path d="M9 11l5 5"/>)" },
        { "rotl", R"(<path d="M4 5v5h5"/><path d="M5 10a8 8 0 1 1 1.6 6.4"/>)" },
        { "rotr", R"(<path d="M20 5v5h-5"/><path d="M19 10a8 8 0 1 0-1.6 6.4"/>)" },
        { "trash", R"(<path d="M4 7h16M9 7V4h6v3M6 7l1 13h10l1-13"/>)" },
        { "image", R"(<rect x="3.5" y="5" width="17" height="14" rx="2"/><path d="m3.5 16 5-5 4 4 2.5-2.5 5.5 5.5"/><circle cx="15.5" cy="9.5" r="1.4"/>)" },
        { "check", R"(<rect x="4" y="4" width="16" height="16" rx="4"/><path d="m8 12.5 3 3 5.5-6"/>)" },
        { "box", R"(<rect x="4" y="4" width="16" height="16" rx="4"/>)" },
        { "mount", R"(<path d="m2 19 6.5-11 4 6.5L15.5 10 22 19z"/>)" },
        { "forest", R"(<circle cx="8.5" cy="10" r="4.2"/><circle cx="15.5" cy="12" r="4.2"/><path d="M8.5 14.2V19M15.5 16.2V20"/>)" },
        { "roadline", R"(<path d="M2 12h20" stroke-dasharray="4 3"/>)" },
        { "legend", R"(<rect x="4" y="3.5" width="16" height="17" rx="2"/><path d="M8 8h8M8 12h8M8 16h5"/>)" },
    };
    return s;
}
}

QPixmap iconPixmap(const QString& name, const QColor& color, int px, qreal dpr)
{
    QString body = shapes().value(name);
    const QString col = color.name(QColor::HexRgb);
    body.replace(QStringLiteral("currentColor"), col);
    const QString svg = QStringLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"%1\" stroke-width=\"1.7\" "
        "stroke-linecap=\"round\" stroke-linejoin=\"round\" stroke-opacity=\"%2\">%3</svg>")
        .arg(col).arg(color.alphaF()).arg(body);
    QSvgRenderer r(svg.toUtf8());
    QPixmap pm(qRound(px * dpr), qRound(px * dpr));
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    r.render(&p);
    p.end();
    pm.setDevicePixelRatio(dpr);
    return pm;
}

QIcon icon(const QString& name, const QColor& normal, const QColor& active, int px)
{
    QIcon ic;
    ic.addPixmap(iconPixmap(name, normal, px), QIcon::Normal, QIcon::Off);
    ic.addPixmap(iconPixmap(name, active, px), QIcon::Normal, QIcon::On);
    ic.addPixmap(iconPixmap(name, active, px), QIcon::Active, QIcon::Off);
    QColor dis = normal; dis.setAlphaF(0.35);
    ic.addPixmap(iconPixmap(name, dis, px), QIcon::Disabled, QIcon::Off);
    return ic;
}

} // namespace carto
