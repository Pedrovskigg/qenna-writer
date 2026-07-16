#include "IconUtils.h"

#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QSvgRenderer>

namespace {

QPixmap renderTintedSvg(const QString &source, const QColor &color, const QSize &size)
{
    QString svg = source;
    static const QRegularExpression fillRegex(QStringLiteral("fill=\"#[0-9a-fA-F]+\""));
    static const QRegularExpression strokeRegex(QStringLiteral("stroke=\"#[0-9a-fA-F]+\""));
    svg.replace(fillRegex, QStringLiteral("fill=\"%1\"").arg(color.name()));
    svg.replace(strokeRegex, QStringLiteral("stroke=\"%1\"").arg(color.name()));

    QSvgRenderer renderer(svg.toUtf8());
    QPixmap pix(size);
    pix.setDevicePixelRatio(1.0);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter);
    return pix;
}

}

namespace IconUtils {

QIcon loadToolbarIcon(const QString &resourcePath,
                      const QColor &normal,
                      const QColor &active,
                      const QColor &selected,
                      const QSize &size)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QIcon();
    }
    const QString source = QString::fromUtf8(file.readAll());

    QIcon icon;
    icon.addPixmap(renderTintedSvg(source, normal, size), QIcon::Normal, QIcon::Off);
    icon.addPixmap(renderTintedSvg(source, active, size), QIcon::Active, QIcon::Off);
    icon.addPixmap(renderTintedSvg(source, selected, size), QIcon::Selected, QIcon::Off);
    icon.addPixmap(renderTintedSvg(source, selected, size), QIcon::Normal, QIcon::On);
    icon.addPixmap(renderTintedSvg(source, selected, size), QIcon::Active, QIcon::On);
    return icon;
}

}
