#include "UiScale.h"

#include <QSettings>
#include <QtGlobal>

namespace UiScale {

namespace {
constexpr const char* kKeyScale = "ui/scale";
constexpr const char* kKeyAppZoom = "ui/appZoom";

qreal boundedZoom(qreal v)
{
    // Em degraus de 5%, como o controle das Configurações.
    return qBound(minAppZoom(), qRound(v * 20.0) / 20.0, maxAppZoom());
}
}

Manager* Manager::instance()
{
    static Manager* mgr = new Manager();
    return mgr;
}

Manager::Manager()
{
    load();
}

void Manager::load()
{
    QSettings s;
    m_scale = qBound(minScale(), s.value(kKeyScale, m_scale).toReal(), maxScale());
}

void Manager::save() const
{
    QSettings s;
    s.setValue(kKeyScale, m_scale);
}

void Manager::setScale(qreal factor)
{
    factor = qBound(minScale(), factor, maxScale());
    if (qFuzzyCompare(factor, m_scale)) return;
    m_scale = factor;
    save();
    emit scaleChanged();
}

qreal scale() { return Manager::instance()->scale(); }

qreal defaultAppZoom()
{
#ifdef Q_OS_MACOS
    return 1.2;
#else
    return 1.0;
#endif
}

qreal appZoom()
{
    return boundedZoom(QSettings().value(kKeyAppZoom, defaultAppZoom()).toReal());
}

void setAppZoom(qreal factor)
{
    QSettings().setValue(kKeyAppZoom, boundedZoom(factor));
}

void applyAppZoomBeforeApp(const QString& orgAndAppName)
{
    if (qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) return;   // quem chamou de fora manda
    const QSettings s(QSettings::NativeFormat, QSettings::UserScope, orgAndAppName, orgAndAppName);
    const qreal zoom = boundedZoom(s.value(kKeyAppZoom, defaultAppZoom()).toReal());
    if (qFuzzyCompare(zoom, 1.0)) return;
    qputenv("QT_SCALE_FACTOR", QByteArray::number(zoom, 'f', 2));
}

} // namespace UiScale
