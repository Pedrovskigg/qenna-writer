#include "UiScale.h"

#include <QSettings>
#include <QtGlobal>

namespace UiScale {

namespace {
constexpr const char* kKeyScale = "ui/scale";
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

} // namespace UiScale
