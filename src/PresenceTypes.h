#pragma once

#include <QList>
#include <QString>

// Resultado da análise de presença de personagens por capítulo/cena.
// Compartilhado entre o provider (MainWindow), o DrawerListPanel (consistência)
// e a TimelinePanel (trilhas automáticas de personagem).
struct PresenceSceneEntry { int index = 0; QString title; };
struct PresenceChapterEntry { QString id; QString title; QList<PresenceSceneEntry> scenes; };
struct CharPresenceResult {
    int sceneCount    = 0;
    int chapterCount  = 0;
    QList<PresenceChapterEntry> chapters;
};
