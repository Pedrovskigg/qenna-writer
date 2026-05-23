#include "EditorLayout.h"

#include <QSettings>
#include <QtGlobal>

namespace EditorLayout {

namespace {
constexpr const char* kKeyPageWidth  = "editor/pageWidth";
constexpr const char* kKeyHMargin    = "editor/horizontalMargin";
constexpr const char* kKeyVMargin    = "editor/verticalMargin";
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
    m_pageWidth        = qBound(minPageWidth(),
                                s.value(kKeyPageWidth, m_pageWidth).toInt(),
                                maxPageWidth());
    m_horizontalMargin = qBound(minHorizontalMargin(),
                                s.value(kKeyHMargin, m_horizontalMargin).toInt(),
                                maxHorizontalMargin());
    m_verticalMargin   = qBound(minVerticalMargin(),
                                s.value(kKeyVMargin, m_verticalMargin).toInt(),
                                maxVerticalMargin());
}

void Manager::save() const
{
    QSettings s;
    s.setValue(kKeyPageWidth, m_pageWidth);
    s.setValue(kKeyHMargin,   m_horizontalMargin);
    s.setValue(kKeyVMargin,   m_verticalMargin);
}

void Manager::setPageWidth(int px)
{
    px = qBound(minPageWidth(), px, maxPageWidth());
    if (px == m_pageWidth) return;
    m_pageWidth = px;
    save();
    emit layoutChanged();
}

void Manager::setHorizontalMargin(int px)
{
    px = qBound(minHorizontalMargin(), px, maxHorizontalMargin());
    if (px == m_horizontalMargin) return;
    m_horizontalMargin = px;
    save();
    emit layoutChanged();
}

void Manager::setVerticalMargin(int px)
{
    px = qBound(minVerticalMargin(), px, maxVerticalMargin());
    if (px == m_verticalMargin) return;
    m_verticalMargin = px;
    save();
    emit layoutChanged();
}

int pageWidth()        { return Manager::instance()->pageWidth(); }
int horizontalMargin() { return Manager::instance()->horizontalMargin(); }
int verticalMargin()   { return Manager::instance()->verticalMargin(); }

} // namespace EditorLayout
