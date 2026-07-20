#pragma once

#include <QString>
#include <QStringList>

class QTextDocument;

// Helpers de cena. Delimitador entre cenas no HTML do capítulo é a tag <hr>
// — espelha o Mira 1 (SCENE_DELIMITER_RE = /<hr\s*\/?>/gi).
namespace SceneUtils {

QStringList splitHtmlIntoScenes(const QString& html);
QString joinScenesHtml(const QStringList& segments);
QString getSceneHtml(const QString& fullHtml, int sceneIndex);
QString replaceSceneHtml(const QString& fullHtml, int sceneIndex, const QString& newSegment);
int countSceneDelimiters(const QString& html);

// Índice (0-based) da cena que contém targetBlock num QTextDocument já
// carregado (ChapterDoc inteiro), contando separadores <hr> — que viram
// blocos com a propriedade de régua horizontal (BlockTrailingHorizontalRulerWidth).
int sceneIndexForBlock(QTextDocument* doc, int targetBlock);

} // namespace SceneUtils
