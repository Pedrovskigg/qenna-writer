#include "SceneUtils.h"

#include <QRegularExpression>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextDocument>

namespace SceneUtils {

namespace {
const QRegularExpression& delimiterRegex() {
    static const QRegularExpression re(QStringLiteral("<hr\\s*/?>"),
                                       QRegularExpression::CaseInsensitiveOption);
    return re;
}
}

QStringList splitHtmlIntoScenes(const QString& html) {
    if (html.isEmpty()) return { QStringLiteral("<p></p>") };
    QStringList parts;
    for (const QString& raw : html.split(delimiterRegex())) {
        const QString trimmed = raw.trimmed();
        if (!trimmed.isEmpty()) parts.append(trimmed);
    }
    if (parts.isEmpty()) parts.append(QStringLiteral("<p></p>"));
    return parts;
}

QString joinScenesHtml(const QStringList& segments) {
    if (segments.isEmpty()) return QStringLiteral("<p></p>");
    QStringList kept;
    kept.reserve(segments.size());
    for (const QString& s : segments) {
        if (!s.isEmpty()) kept.append(s);
    }
    if (kept.isEmpty()) return QStringLiteral("<p></p>");
    return kept.join(QStringLiteral("<hr>"));
}

QString getSceneHtml(const QString& fullHtml, int sceneIndex) {
    const QStringList segments = splitHtmlIntoScenes(fullHtml);
    if (sceneIndex < 0 || sceneIndex >= segments.size()) {
        return QStringLiteral("<p></p>");
    }
    const QString seg = segments.at(sceneIndex);
    return seg.isEmpty() ? QStringLiteral("<p></p>") : seg;
}

QString replaceSceneHtml(const QString& fullHtml, int sceneIndex, const QString& newSegment) {
    QStringList segments = splitHtmlIntoScenes(fullHtml);
    if (sceneIndex < 0 || sceneIndex >= segments.size()) return fullHtml;
    segments[sceneIndex] = newSegment;
    return joinScenesHtml(segments);
}

int countSceneDelimiters(const QString& html) {
    if (html.isEmpty()) return 0;
    int count = 0;
    auto it = delimiterRegex().globalMatch(html);
    while (it.hasNext()) { it.next(); ++count; }
    return count;
}

int sceneIndexForBlock(QTextDocument* doc, int targetBlock) {
    if (!doc) return -1;
    int scene = 0;
    for (QTextBlock b = doc->firstBlock(); b.isValid(); b = b.next()) {
        if (b.blockNumber() >= targetBlock) break;
        if (b.blockFormat().hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth))
            ++scene;
    }
    return scene;
}

} // namespace SceneUtils
