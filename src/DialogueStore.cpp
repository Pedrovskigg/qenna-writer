#include "DialogueStore.h"

#include "WordCounter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <algorithm>

DialogueStore::DialogueStore(QObject* parent)
    : QObject(parent)
{
}

void DialogueStore::setProjectRoot(const QString& root)
{
    if (m_root == root) return;
    m_root = root;
    m_dialogues.clear();
}

QString DialogueStore::sidecarPath() const
{
    if (m_root.isEmpty()) return QString();
    return QDir::cleanPath(m_root + QStringLiteral("/dialogs.json"));
}

bool DialogueStore::load()
{
    m_dialogues.clear();
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;
    QFile f(path);
    if (!f.exists()) {
        emit changed();
        return true;
    }
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) return false;

    for (const auto& v : doc.array()) {
        const QJsonObject o = v.toObject();
        Dialogue d;
        d.id           = o.value(QStringLiteral("id")).toString();
        d.text         = o.value(QStringLiteral("text")).toString();
        d.characterId  = o.value(QStringLiteral("characterId")).toString();
        d.manuscriptId = o.value(QStringLiteral("manuscriptId")).toString();
        d.chapterId    = o.value(QStringLiteral("chapterId")).toString();
        d.sceneIndex   = o.value(QStringLiteral("sceneIndex")).toInt(-1);
        d.sourceLabel  = o.value(QStringLiteral("sourceLabel")).toString();
        d.createdAt    = qint64(o.value(QStringLiteral("createdAt")).toDouble());
        if (d.id.isEmpty() || d.text.isEmpty()) continue;
        m_dialogues.append(d);
    }
    emit changed();
    return true;
}

bool DialogueStore::save() const
{
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;

    QJsonArray arr;
    for (const Dialogue& d : m_dialogues) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), d.id);
        o.insert(QStringLiteral("text"), d.text);
        o.insert(QStringLiteral("characterId"), d.characterId);
        o.insert(QStringLiteral("manuscriptId"), d.manuscriptId);
        o.insert(QStringLiteral("chapterId"), d.chapterId);
        o.insert(QStringLiteral("sceneIndex"), d.sceneIndex);
        o.insert(QStringLiteral("sourceLabel"), d.sourceLabel);
        o.insert(QStringLiteral("createdAt"), double(d.createdAt));
        arr.append(o);
    }

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return f.commit();
}

QVector<DialogueStore::Dialogue> DialogueStore::dialoguesForCharacter(const QString& elementId) const
{
    QVector<Dialogue> out;
    if (elementId.isEmpty()) return out;
    for (const Dialogue& d : m_dialogues)
        if (d.characterId == elementId) out.append(d);
    std::sort(out.begin(), out.end(),
              [](const Dialogue& a, const Dialogue& b) { return a.createdAt > b.createdAt; });
    return out;
}

int DialogueStore::dialogueWordsForChapter(const QString& chapterId) const
{
    if (chapterId.isEmpty()) return 0;
    int words = 0;
    for (const Dialogue& d : m_dialogues)
        if (d.chapterId == chapterId) words += WordCounter::countWordsInPlain(d.text);
    return words;
}

void DialogueStore::upsertScanResults(const QString& manuscriptId, const QString& chapterId,
                                      const QVector<ScannedLine>& found)
{
    if (chapterId.isEmpty()) return;

    auto hashOf = [](const QString& t) { return t.left(100); };

    QHash<QString, const ScannedLine*> foundByHash;
    for (const ScannedLine& f : found) foundByHash.insert(hashOf(f.text), &f);

    // Reconciliação é por CAPÍTULO inteiro, não por (chapterId, sceneIndex)
    // exato — um capítulo sem cenas que ganha a primeira cena muda o
    // sceneIndex de -1 pra 0 pra todo o conteúdo já existente. Se a
    // identidade fosse por cena exata, isso duplicaria tudo em vez de só
    // atualizar o "endereço".
    QVector<int> existingIdx;
    for (int i = 0; i < m_dialogues.size(); ++i) {
        if (m_dialogues.at(i).chapterId == chapterId) existingIdx.append(i);
    }

    QSet<QString> claimedHashes;
    for (int idx : existingIdx) {
        const QString h = hashOf(m_dialogues.at(idx).text);
        if (foundByHash.contains(h)) claimedHashes.insert(h);
    }

    bool dirty = false;

    // Falas já salvas cujo hash ainda bate: corrige cena/rótulo se mudaram
    // (ex.: capítulo ganhou/perdeu cenas), sem duplicar nem recriar.
    for (int idx : existingIdx) {
        Dialogue& d = m_dialogues[idx];
        const QString h = hashOf(d.text);
        auto it = foundByHash.constFind(h);
        if (it == foundByHash.constEnd()) continue;
        const ScannedLine* f = it.value();
        if (d.sceneIndex != f->sceneIndex || d.sourceLabel != f->sourceLabel) {
            d.sceneIndex = f->sceneIndex;
            d.sourceLabel = f->sourceLabel;
            dirty = true;
        }
    }

    // Falas cujo texto mudou (linha editada): tenta casar com um candidato
    // do scan atual ainda não reclamado, só quando a correspondência é
    // inequívoca (evita atribuir errado quando várias falas mudaram juntas).
    for (int idx : existingIdx) {
        Dialogue& d = m_dialogues[idx];
        const QString h = hashOf(d.text);
        if (foundByHash.contains(h)) continue;

        QVector<const ScannedLine*> candidates;
        for (const ScannedLine& f : found) {
            if (!claimedHashes.contains(hashOf(f.text))) candidates.append(&f);
        }
        if (candidates.size() == 1) {
            d.text = candidates.first()->text;
            d.sceneIndex = candidates.first()->sceneIndex;
            d.sourceLabel = candidates.first()->sourceLabel;
            claimedHashes.insert(hashOf(candidates.first()->text));
            dirty = true;
        }
    }

    // Falas novas: o que sobrou do scan sem dono.
    for (const ScannedLine& f : found) {
        const QString h = hashOf(f.text);
        if (claimedHashes.contains(h)) continue;

        Dialogue d;
        d.id           = QUuid::createUuid().toString(QUuid::WithoutBraces);
        d.text         = f.text;
        d.characterId  = f.characterId;
        d.manuscriptId = manuscriptId;
        d.chapterId    = chapterId;
        d.sceneIndex   = f.sceneIndex;
        d.sourceLabel  = f.sourceLabel;
        d.createdAt    = QDateTime::currentMSecsSinceEpoch();
        m_dialogues.append(d);
        claimedHashes.insert(h);
        dirty = true;
    }

    if (dirty) emit changed();
}

bool DialogueStore::remove(const QString& id)
{
    const int before = m_dialogues.size();
    m_dialogues.erase(std::remove_if(m_dialogues.begin(), m_dialogues.end(),
                                     [&](const Dialogue& d) { return d.id == id; }),
                      m_dialogues.end());
    if (m_dialogues.size() == before) return false;
    emit changed();
    return true;
}

bool DialogueStore::setCharacter(const QString& id, const QString& newCharacterId)
{
    if (newCharacterId.isEmpty()) return false;
    for (Dialogue& d : m_dialogues) {
        if (d.id != id) continue;
        if (d.characterId == newCharacterId) return false;
        d.characterId = newCharacterId;
        emit changed();
        return true;
    }
    return false;
}
