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
        { "gear", R"(<path d="M19.2 12.9 21.3 14.1 20.0 17.1 17.8 16.5 16.5 17.8 17.1 20.0 14.1 21.3 12.9 19.2 11.1 19.2 9.9 21.3 6.9 20.0 7.5 17.8 6.2 16.5 4.0 17.1 2.7 14.1 4.8 12.9 4.8 11.1 2.7 9.9 4.0 6.9 6.2 7.5 7.5 6.2 6.9 4.0 9.9 2.7 11.1 4.8 12.9 4.8 14.1 2.7 17.1 4.0 16.5 6.2 17.8 7.5 20.0 6.9 21.3 9.9 19.2 11.1z" stroke-linejoin="round"/><circle cx="12" cy="12" r="3"/>)" },
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
        // Explorar
        { "explore", R"(<path d="M4 6.5 9 4.5l6 2 5-2v13l-5 2-6-2-5 2z"/><path d="M9 4.5v13M15 6.5v13" opacity=".5"/><circle cx="15.5" cy="12.5" r="2.6"/><path d="m17.4 14.4 2.1 2.1"/>)" },
        { "pen", R"(<path d="M15 5l4 4L9 19H5v-4z"/><path d="M13 7l4 4"/>)" },
        { "expand", R"(<path d="M14 4h6v6M10 20H4v-6M20 4l-7 7M4 20l7-7"/>)" },
        { "shrink", R"(<path d="M20 10h-6V4M4 14h6v6M14 10l7-7M10 14l-7 7"/>)" },
        { "link", R"(<circle cx="6.5" cy="16" r="2.5"/><circle cx="17.5" cy="16" r="2.5"/><path d="M8.5 14.5C10 8 14 8 15.5 14.5" stroke-dasharray="2.2 2.2"/>)" },
        { "open", R"(<path d="M14 4h6v6M20 4l-8 8"/><path d="M18 14v5H5V6h5"/>)" },
        { "folder", R"(<path d="M3.5 7.5V18a1 1 0 0 0 1 1h15a1 1 0 0 0 1-1V9.5a1 1 0 0 0-1-1H11l-2-2.5H4.5a1 1 0 0 0-1 1.5z"/>)" },
        { "tray", R"(<path d="M4 13l2.2-7h11.6L20 13v6H4z"/><path d="M4 13h4.5l1 2h5l1-2H20"/>)" },
        { "globe", R"(<circle cx="12" cy="12" r="8.5"/><path d="M3.5 12h17M12 3.5c2.6 2.6 3.6 5.4 3.6 8.5s-1 5.9-3.6 8.5c-2.6-2.6-3.6-5.4-3.6-8.5s1-5.9 3.6-8.5z"/>)" },
        { "id", R"(<rect x="3.5" y="5" width="17" height="14" rx="2"/><circle cx="9" cy="11" r="2.3"/><path d="M5.8 16.5c.6-1.8 1.8-2.6 3.2-2.6s2.6.8 3.2 2.6M14.5 10h3.5M14.5 13.5h3.5"/>)" },
        { "back", R"(<path d="M15 6l-6 6 6 6"/>)" },
        { "swap", R"(<path d="M4 8h14l-3-3M20 16H6l3 3"/>)" },
        { "list", R"(<path d="M9 7h11M9 12h11M9 17h11"/><circle cx="4.5" cy="7" r=".8"/><circle cx="4.5" cy="12" r=".8"/><circle cx="4.5" cy="17" r=".8"/>)" },
        { "in", R"(<path d="M12 4v10M8 10l4 4 4-4"/><path d="M4 14v4.5h16V14"/>)" },
        { "plus", R"(<path d="M12 5v14M5 12h14"/>)" },
        { "hash", R"(<path d="M10 4 8 20M16 4l-2 16M5 9h15M4 15h15"/>)" },
        { "minus", R"(<path d="M5 12h14"/>)" },
        { "cursor", R"(<path d="M5 3l12 8-5 1.5 3 6-2.4 1.2-3-6L6 17z"/>)" },
        { "tower", R"(<path d="M8 20V7h8v13"/><path d="M7 7V4h2v1.5h2V4h2v1.5h2V4h2v3"/><path d="M12 10v2"/>)" },
        { "lighthouse", R"(<path d="M9 20l1-11h4l1 11z"/><path d="M9.5 9h5V6h-5z"/><path d="M12 3v3M5 6l2 1M19 6l-2 1"/>)" },
        { "windmill", R"(<path d="M10 20l.8-8h2.4l.8 8z"/><path d="M12 9 7 4M12 9l5-5M12 9l-5 5M12 9l5 5"/>)" },
        { "temple", R"(<path d="M4 9 12 4l8 5z"/><path d="M6 9v9M10 9v9M14 9v9M18 9v9M4 20h16"/>)" },
        { "ruins", R"(<path d="M4 20h16"/><path d="M5 20v-7h3v3M11 20v-10h3v5"/><path d="M17 20v-4h2"/>)" },
        // Construir: as levas novas
        { "a-statue", R"(<path d="M8 21h8M9 21v-3h6v3"/><circle cx="12" cy="5" r="1.6"/><path d="M12 7v6M10 18l2-5 2 5M12 9l3-4M12 9l-3 2"/>)" },
        { "a-ship", R"(<path d="M3 16h18l-2.5 4h-13z"/><path d="M8 16V5M14 16V3"/><path d="M8 6h-3l3 8M14 4h4l-4 10"/>)" },
        { "a-colosseum", R"(<ellipse cx="12" cy="12" rx="9" ry="6"/><ellipse cx="12" cy="12" rx="4.5" ry="2.6"/><path d="M5 9.5v5M8 7.4v8M16 7.4v8M19 9.5v5"/>)" },
        { "a-cave", R"(<path d="M3 19c1-7 4-12 9-12s8 5 9 12z"/><path d="M9 19v-3a3 3 0 0 1 6 0v3"/>)" },
        { "a-mine", R"(<path d="M4 19V9h8v10"/><path d="M3 9h10"/><path d="M12 19h9M14 17h6"/><rect x="15" y="13" width="4" height="3"/>)" },
        { "a-mage", R"(<path d="M10 21V10h4v11"/><path d="M9 10l3-5 3 5"/><path d="M12 2l1.2 1.6L12 5l-1.2-1.4z"/>)" },
        { "a-greattree", R"(<circle cx="12" cy="8" r="5.5"/><path d="M12 13v8M9 21h6M12 16l-3-2M12 15l3-2"/>)" },
        { "a-stones", R"(<path d="M5 18V11M9 18V9M15 18V9M19 18V11M8 9h8"/><path d="M3 18h18"/>)" },
        { "a-camp", R"(<path d="M3 18l5-9 5 9z"/><path d="M8 18v-3"/><path d="M16 18c0-2 2-2.5 2-5 0 2.5 2 3 2 5z"/>)" },
        { "a-dragon", R"(<path d="M4 18c3-1 5-3 6-6l2-4 3 1-2 4"/><path d="M12 8l-4-4-1 5M14 9l5-5 0 6"/><path d="M6 18c3 1 8 1 12-1"/>)" },
        { "a-bones", R"(<path d="M3 13h15"/><path d="M7 13c0-3 1-5 1-5M7 13c0 3 1 5 1 5M11 13c0-3 1-5 1-5M11 13c0 3 1 5 1 5M15 13c0-2 1-4 1-4M15 13c0 2 1 4 1 4"/><circle cx="20" cy="13" r="1.6"/>)" },
        { "a-portal", R"(<circle cx="12" cy="11" r="7"/><circle cx="12" cy="11" r="3.5"/><path d="M4 21h16"/>)" },
        { "a-floatisland", R"(<path d="M4 9h16l-5 7-3 5-3-5z"/><path d="M8 9V5h3v4"/><path d="M17 9v8"/>)" },
        { "a-sword", R"(<path d="M12 2v13M9 15h6M12 15v3"/><path d="M5 21c1-3 4-4 7-4s6 1 7 4z"/>)" },
        { "a-heads", R"(<path d="M6 20V8l2-4h3v16"/><path d="M14 20V8l2-4h3v16"/><path d="M3 20h18"/>)" },
        { "a-longship", R"(<path d="M3 15c3 3 15 3 18 0"/><path d="M3 15l-1-4M21 15l1-4"/><path d="M12 15V4M7 6h10v6H7z"/>)" },
        { "a-wreck", R"(<path d="M3 17l14-4 4 3"/><path d="M9 15l2-10M11 5l4 3"/><path d="M2 20h20"/>)" },
        { "a-seaserpent", R"(<path d="M2 17c2-5 4-5 5 0M8 17c2-5 4-5 5 0"/><path d="M14 17c1-6 3-9 6-9l1 2-3 1"/><path d="M2 20h20"/>)" },
        { "a-kraken", R"(<path d="M5 19c-1-5 1-9 3-9 1 0 1 2 0 2"/><path d="M19 19c1-5-1-9-3-9-1 0-1 2 0 2"/><path d="M12 19V8c0-2 2-2 2 0"/><path d="M2 20h20"/>)" },
        { "a-pyramid", R"(<path d="M12 4l9 15H3z"/><path d="M12 4l2 15"/>)" },
        { "a-ziggurat", R"(<path d="M3 20h18M5 20v-4h14v4M7 16v-4h10v4M9 12V8h6v4"/><path d="M11 20V8"/>)" },
        { "a-pagoda", R"(<path d="M12 2v3M5 9c3-1 11-1 14 0M6 14c3-1 9-1 12 0M4 19c4-1 12-1 16 0"/><path d="M8 9v5M16 9v5M7 14v5M17 14v5"/>)" },
        { "a-obelisk", R"(<path d="M11 4h2l1 15h-4z"/><path d="M12 2l1 2h-2z"/><path d="M7 21h10"/>)" },
        { "a-graveyard", R"(<path d="M6 20v-6a2 2 0 0 1 4 0v6"/><path d="M15 20V11M12 14h6"/><path d="M3 20h18"/>)" },
        { "a-farm", R"(<path d="M3 20v-8l5-4 5 4v8z"/><path d="M6 20v-4h4v4"/><path d="M16 20V8a2 2 0 0 1 4 0v12"/>)" },
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
        { "chevRight", R"(<path d="m9.5 6 6 6-6 6"/>)" },
        { "chevDown", R"(<path d="m6 9.5 6 6 6-6"/>)" },
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
