#include "DialogueChemistry.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace DialogueChemistry {

namespace {
QString groupKey(const DialogueStore::Dialogue& d)
{
    return d.chapterId + QStringLiteral("::") + QString::number(d.sceneIndex);
}
}

QVector<PairStats> chemistryForCharacter(const QVector<DialogueStore::Dialogue>& all,
                                          const QString& elementId)
{
    QVector<PairStats> out;
    if (elementId.isEmpty()) return out;

    // groupKey -> falantes distintos presentes, capítulo do grupo, índices das falas.
    QHash<QString, QSet<QString>> speakersByGroup;
    QHash<QString, QString> chapterOfGroup;
    QHash<QString, QVector<int>> lineIndexesByGroup;

    for (int i = 0; i < all.size(); ++i) {
        const DialogueStore::Dialogue& d = all.at(i);
        if (d.characterId.isEmpty()) continue;
        const QString key = groupKey(d);
        speakersByGroup[key].insert(d.characterId);
        chapterOfGroup[key] = d.chapterId;
        lineIndexesByGroup[key].append(i);
    }

    QHash<QString, int> scenesTogether;
    QHash<QString, QSet<QString>> chaptersTogether;
    QHash<QString, int> crossDialogues;

    for (auto it = speakersByGroup.constBegin(); it != speakersByGroup.constEnd(); ++it) {
        const QSet<QString>& group = it.value();
        if (!group.contains(elementId) || group.size() < 2) continue;
        const QString& key = it.key();
        const QString chapterId = chapterOfGroup.value(key);
        const QVector<int>& lines = lineIndexesByGroup.value(key);

        for (const QString& other : group) {
            if (other == elementId) continue;
            scenesTogether[other]++;
            chaptersTogether[other].insert(chapterId);

            int count = 0;
            for (int idx : lines) {
                const QString& cid = all.at(idx).characterId;
                if (cid == elementId || cid == other) ++count;
            }
            crossDialogues[other] += count;
        }
    }

    for (auto it = scenesTogether.constBegin(); it != scenesTogether.constEnd(); ++it) {
        PairStats ps;
        ps.otherElementId = it.key();
        ps.scenesTogether = it.value();
        ps.chaptersTogether = chaptersTogether.value(it.key()).size();
        ps.crossDialogues = crossDialogues.value(it.key());
        out.append(ps);
    }
    return out;
}

QVector<DialogueStore::Dialogue> sharedSceneDialogues(const QVector<DialogueStore::Dialogue>& all,
                                                       const QString& a, const QString& b)
{
    QVector<DialogueStore::Dialogue> out;
    if (a.isEmpty() || b.isEmpty()) return out;

    QHash<QString, QSet<QString>> speakersByGroup;
    for (const DialogueStore::Dialogue& d : all) {
        if (d.characterId.isEmpty()) continue;
        speakersByGroup[groupKey(d)].insert(d.characterId);
    }

    for (const DialogueStore::Dialogue& d : all) {
        if (d.characterId != a && d.characterId != b) continue;
        const QSet<QString>& group = speakersByGroup.value(groupKey(d));
        if (group.contains(a) && group.contains(b)) out.append(d);
    }
    std::sort(out.begin(), out.end(),
              [](const DialogueStore::Dialogue& x, const DialogueStore::Dialogue& y) {
        return x.createdAt < y.createdAt;
    });
    return out;
}

} // namespace DialogueChemistry
