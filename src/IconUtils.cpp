#include "IconUtils.h"

#include "Theme.h"

#include <QFile>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QSvgRenderer>

namespace {

qreal iconPixelRatio()
{
    qreal r = 1.0;
    for (QScreen* screen : QGuiApplication::screens()) r = qMax(r, screen->devicePixelRatio());
    return r;
}

QPixmap renderTintedSvg(const QString &source, const QColor &color, const QSize &size)
{
    QString svg = source;
    static const QRegularExpression fillRegex(QStringLiteral("fill=\"#[0-9a-fA-F]+\""));
    static const QRegularExpression strokeRegex(QStringLiteral("stroke=\"#[0-9a-fA-F]+\""));
    svg.replace(fillRegex, QStringLiteral("fill=\"%1\"").arg(color.name()));
    svg.replace(strokeRegex, QStringLiteral("stroke=\"%1\"").arg(color.name()));

    QSvgRenderer renderer(svg.toUtf8());
    // Na densidade da tela mais densa: no Retina (2x) o ícone desenhado a 1x
    // era esticado pelo sistema e borrava (relato da Marley no Mac). Numa tela
    // comum o QIcon reduz sozinho, e o custo é só memória.
    const qreal dpr = iconPixelRatio();
    QPixmap pix((QSizeF(size) * dpr).toSize());
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)));
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

    // Cor dos ícones do tema: quase todo ícone do app pede textMuted como cor
    // "normal", então é aqui que ela entra — sem tocar em cada chamada. Quem
    // pinta com outra cor (gavetas com a cor do projeto, o título da folha)
    // fica de fora sozinho.
    QColor base = normal;
    if (Theme::hasIconColor() && normal == Theme::toColor(Theme::textMuted()))
        base = Theme::toColor(Theme::iconColor());

    QIcon icon;
    icon.addPixmap(renderTintedSvg(source, base, size), QIcon::Normal, QIcon::Off);
    icon.addPixmap(renderTintedSvg(source, active, size), QIcon::Active, QIcon::Off);
    icon.addPixmap(renderTintedSvg(source, selected, size), QIcon::Selected, QIcon::Off);
    icon.addPixmap(renderTintedSvg(source, selected, size), QIcon::Normal, QIcon::On);
    icon.addPixmap(renderTintedSvg(source, selected, size), QIcon::Active, QIcon::On);
    return icon;
}

}
