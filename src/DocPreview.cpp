#include "DocPreview.h"

#include "DocCache.h"
#include "ElementsStore.h"
#include "ProjectModel.h"
#include "ProjectStorage.h"

#include <QRegularExpression>

namespace DocPreview {

QString resolveDrawerItemHtml(const DrawerItem* item, ElementsStore* elements,
                               DocCache* cache, const QString& projectRoot,
                               bool includePhoto)
{
    if (!item) return QString();

    if (item->isSheet) {
        // Ficha: gera o html dos campos. Nome/apelido ficam de fora (quem
        // chama já os mostra no cabeçalho); a foto entra como <img>, só se
        // pedida (ver includePhoto no header).
        QString img;
        if (includePhoto && elements && !item->elementId.isEmpty()) {
            if (const Element* e = elements->findElement(item->elementId))
                img = e->image;
        }
        return ProjectModel::characterSheetToHtml(item->sheet, QString(), QString(), img);
    }

    const QString cacheKey = DocCache::itemKey(item->id);
    if (cache && cache->has(cacheKey)) return cache->get(cacheKey);
    if (item->hasInlineHtml) return item->html;
    if (!item->file.isEmpty() && !projectRoot.isEmpty()) {
        bool ok = false;
        return ProjectStorage::readText(ProjectStorage::joinPath(projectRoot, item->file), &ok);
    }
    return QString();
}

QString stripImages(const QString& html)
{
    static const QRegularExpression re(
        QStringLiteral("<img\\b[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    QString out = html;
    out.remove(re);
    return out;
}

} // namespace DocPreview
