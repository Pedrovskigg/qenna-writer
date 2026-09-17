#include "DocPreview.h"

#include "DocCache.h"
#include "ElementsStore.h"
#include "ProjectModel.h"
#include "ProjectStorage.h"

#include "MarkerStore.h"
#include "Theme.h"

#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>

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

QString stripForegroundColors(const QString& html)
{
    static const QRegularExpression re(
        QStringLiteral("(?<!background-)color\\s*:\\s*[^;\"']+;?"),
        QRegularExpression::CaseInsensitiveOption);
    QString out = html;
    out.remove(re);
    return out;
}

void applyThemeTextColors(QTextDocument* doc)
{
    if (!doc) return;

    const QColor textColor(Theme::textPrimary());
    for (QTextBlock block = doc->firstBlock(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid() || frag.length() == 0) continue;

            const QTextCharFormat existing = frag.charFormat();
            QColor fg = textColor;
            const QBrush bgBrush = existing.background();
            if (bgBrush.style() != Qt::NoBrush && bgBrush.color().alpha() > 0)
                fg = MarkerStore::pickContrastingFg(bgBrush.color());

            QTextCursor c(doc);
            c.setPosition(frag.position());
            c.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
            QTextCharFormat fmt;
            fmt.setForeground(fg);
            c.mergeCharFormat(fmt);
        }
    }
}

void applyPreviewFontSize(QTextDocument* doc, int pointSize)
{
    if (!doc || pointSize <= 0) return;
    QTextCursor cur(doc);
    cur.select(QTextCursor::Document);
    QTextCharFormat fmt;
    fmt.setFontPointSize(pointSize);
    cur.mergeCharFormat(fmt);
}

} // namespace DocPreview
