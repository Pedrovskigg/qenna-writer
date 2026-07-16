#ifndef ICONUTILS_H
#define ICONUTILS_H

#include <QColor>
#include <QIcon>
#include <QSize>
#include <QString>

namespace IconUtils {

QIcon loadToolbarIcon(const QString &resourcePath,
                      const QColor &normal,
                      const QColor &active,
                      const QColor &selected,
                      const QSize &size = QSize(20, 20));

}

#endif
