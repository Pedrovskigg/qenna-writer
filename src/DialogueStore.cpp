#include "DialogueStore.h"

#include "WordCounter.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <algorithm>

namespace {

QString confidenceName(DialogueConfidence c)
{
    switch (c) {
    case DialogueConfidence::Certain:  return QStringLiteral("certain");
    case DialogueConfidence::Probable: return QStringLiteral("probable");
    case DialogueConfidence::Extra:    return QStringLiteral("extra");
    case DialogueConfidence::None:     break;
    }
    return QStringLiteral("none");
}

double similarity(const QString& a, const QString& b)
{
    const int n = qMax(a.size(), b.size());
    if (n == 0) return 1.0;
    int pre = 0;
    while (pre < a.size() && pre < b.size() && a.at(pre) == b.at(pre)) ++pre;
    int suf = 0;
    while (suf < a.size() - pre && suf < b.size() - pre
           && a.at(a.size() - 1 - suf) == b.at(b.size() - 1 - suf)) ++suf;
    return double(pre + suf) / double(n);
}

bool sameDialogue(const DialogueStore::Dialogue& a, const DialogueStore::Dialogue& b)
{
    return a.id == b.id && a.text == b.text && a.speech == b.speech && a.characterId == b.characterId
        && a.origin == b.origin && a.confidence == b.confidence && a.extraLabel == b.extraLabel
        && a.manuscriptId == b.manuscriptId && a.chapterId == b.chapterId
        && a.sceneIndex == b.sceneIndex && a.sourceLabel == b.sourceLabel;
}

} // namespace

DialogueStore::DialogueStore(QObject* parent)
    : QObject(parent)
{
}

void DialogueStore::setProjectRoot(const QString& root)
{
    if (m_root == root) return;
    m_root = root;
    m_dialogues.clear();
    m_ignored.clear();
    m_genderVotes.clear();
}

QString DialogueStore::sidecarPath() const
{
    if (m_root.isEmpty()) return QString();
    return QDir::cleanPath(m_root + QStringLiteral("/dialogs.json"));
}

QString DialogueStore::ignoreKey(const QString& chapterId, const QString& text)
{
    return chapterId + QLatin1Char('\n') + text.trimmed();
}

bool DialogueStore::load()
{
    m_dialogues.clear();
    m_ignored.clear();
    m_genderVotes.clear();
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
    if (err.error != QJsonParseError::NoError || (!doc.isArray() && !doc.isObject())) {
        // Arquivo com defeito: guarda uma cópia antes que o próximo save grave
        // por cima. Antes, o app começava vazio e apagava as correções
        // manuais em silêncio.
        const QString backup = path + QStringLiteral(".defeito-")
            + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        QFile::copy(path, backup);
        qWarning() << "[dialogs] dialogs.json ilegível, cópia em" << backup;
        emit changed();
        return false;
    }

    const bool legacy = doc.isArray();
    const QJsonArray arr = legacy ? doc.array() : doc.object().value(QStringLiteral("dialogues")).toArray();
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        Dialogue d;
        d.id           = o.value(QStringLiteral("id")).toString();
        d.text         = o.value(QStringLiteral("text")).toString();
        d.speech       = o.value(QStringLiteral("speech")).toString();
        d.characterId  = o.value(QStringLiteral("characterId")).toString();
        d.origin       = o.value(QStringLiteral("origin")).toString();
        d.confidence   = o.value(QStringLiteral("confidence")).toString();
        d.extraLabel   = o.value(QStringLiteral("extra")).toString();
        d.manuscriptId = o.value(QStringLiteral("manuscriptId")).toString();
        d.chapterId    = o.value(QStringLiteral("chapterId")).toString();
        d.sceneIndex   = o.value(QStringLiteral("sceneIndex")).toInt(-1);
        d.sourceLabel  = o.value(QStringLiteral("sourceLabel")).toString();
        d.createdAt    = qint64(o.value(QStringLiteral("createdAt")).toDouble());
        if (d.id.isEmpty() || d.text.isEmpty()) continue;
        if (d.origin.isEmpty()) {
            // Formato 1: não dá pra saber o que foi corrigido à mão. O primeiro
            // scan decide (ver applyChapterScan).
            d.origin = QStringLiteral("legacy");
            d.confidence = d.characterId.isEmpty() ? QStringLiteral("none") : QStringLiteral("certain");
        }
        m_dialogues.append(d);
    }

    // Lista de ignoradas e votos de gênero moram num arquivo à parte, pra o
    // dialogs.json continuar uma lista que a versão anterior do app ainda lê
    // (ela ignora os campos novos; se regravar, as falas voltam como
    // "legacy" e a migração reclassifica — nada se perde).
    QJsonObject root = legacy ? QJsonObject() : doc.object();
    {
        QFile meta(metaPath());
        if (meta.open(QIODevice::ReadOnly)) {
            const QJsonDocument md = QJsonDocument::fromJson(meta.readAll());
            if (md.isObject()) root = md.object();
        }
    }
    {
        for (const auto& v : root.value(QStringLiteral("ignored")).toArray()) {
            const QJsonObject o = v.toObject();
            m_ignored.insert(ignoreKey(o.value(QStringLiteral("chapterId")).toString(),
                                       o.value(QStringLiteral("text")).toString()));
        }
        const QJsonObject gv = root.value(QStringLiteral("genderVotes")).toObject();
        for (auto ch = gv.constBegin(); ch != gv.constEnd(); ++ch) {
            DialogueDetector::GenderVotes votes;
            const QJsonObject per = ch.value().toObject();
            for (auto e = per.constBegin(); e != per.constEnd(); ++e) {
                const QJsonArray fm = e.value().toArray();
                votes.insert(e.key(), { fm.at(0).toDouble(), fm.at(1).toDouble() });
            }
            m_genderVotes.insert(ch.key(), votes);
        }
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
        if (!d.speech.isEmpty() && d.speech != d.text) o.insert(QStringLiteral("speech"), d.speech);
        o.insert(QStringLiteral("characterId"), d.characterId);
        o.insert(QStringLiteral("origin"), d.origin);
        o.insert(QStringLiteral("confidence"), d.confidence);
        if (!d.extraLabel.isEmpty()) o.insert(QStringLiteral("extra"), d.extraLabel);
        o.insert(QStringLiteral("manuscriptId"), d.manuscriptId);
        o.insert(QStringLiteral("chapterId"), d.chapterId);
        o.insert(QStringLiteral("sceneIndex"), d.sceneIndex);
        o.insert(QStringLiteral("sourceLabel"), d.sourceLabel);
        o.insert(QStringLiteral("createdAt"), double(d.createdAt));
        arr.append(o);
    }

    QJsonArray ignored;
    QStringList keys(m_ignored.begin(), m_ignored.end());
    keys.sort();
    for (const QString& k : keys) {
        const int nl = k.indexOf(QLatin1Char('\n'));
        QJsonObject o;
        o.insert(QStringLiteral("chapterId"), k.left(nl));
        o.insert(QStringLiteral("text"), k.mid(nl + 1));
        ignored.append(o);
    }

    QJsonObject gv;
    for (auto ch = m_genderVotes.constBegin(); ch != m_genderVotes.constEnd(); ++ch) {
        QJsonObject per;
        for (auto e = ch.value().constBegin(); e != ch.value().constEnd(); ++e)
            per.insert(e.key(), QJsonArray{ e.value().first, e.value().second });
        gv.insert(ch.key(), per);
    }

    QJsonObject meta;
    meta.insert(QStringLiteral("version"), 2);
    meta.insert(QStringLiteral("ignored"), ignored);
    meta.insert(QStringLiteral("genderVotes"), gv);

    QSaveFile mf(metaPath());
    if (mf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        mf.write(QJsonDocument(meta).toJson(QJsonDocument::Indented));
        mf.commit();
    }

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return f.commit();
}

QString DialogueStore::metaPath() const
{
    if (m_root.isEmpty()) return QString();
    return QDir::cleanPath(m_root + QStringLiteral("/dialogs-meta.json"));
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
        if (d.chapterId == chapterId) words += WordCounter::countWordsInPlain(d.spokenText());
    return words;
}

int DialogueStore::dialogueWordsForCharacter(const QString& elementId) const
{
    if (elementId.isEmpty()) return 0;
    int words = 0;
    for (const Dialogue& d : m_dialogues)
        if (d.characterId == elementId) words += WordCounter::countWordsInPlain(d.spokenText());
    return words;
}

void DialogueStore::applyChapterScan(const QString& manuscriptId, const QString& chapterId,
                                     const QVector<ScannedLine>& found, int sceneScope,
                                     const DialogueDetector::Cast& cast,
                                     const QString& legacyNarratorId)
{
    const QSet<QString>& validCharacterIds = cast.ids;
    if (chapterId.isEmpty()) return;
    const bool whole = sceneScope == kWholeChapter;

    QVector<ScannedLine> lines;
    lines.reserve(found.size());
    for (const ScannedLine& f : found)
        if (!m_ignored.contains(ignoreKey(chapterId, f.text))) lines.append(f);

    QVector<int> chIdx;
    for (int i = 0; i < m_dialogues.size(); ++i)
        if (m_dialogues.at(i).chapterId == chapterId) chIdx.append(i);

    auto inScope = [&](int storeIdx) {
        return whole || m_dialogues.at(storeIdx).sceneIndex == sceneScope;
    };
    // Identidade: cena + texto + ordem entre textos iguais da mesma cena.
    auto key = [](int scene, const QString& text, int occ) {
        return QString::number(scene) + QLatin1Char('\n') + QString::number(occ) + QLatin1Char('\n') + text;
    };

    QHash<QString, int> existingByKey;
    {
        QHash<QString, int> occ;
        for (int i : chIdx) {
            const Dialogue& d = m_dialogues.at(i);
            const QString base = QString::number(d.sceneIndex) + QLatin1Char('\n') + d.text;
            existingByKey.insert(key(d.sceneIndex, d.text, occ[base]++), i);
        }
    }

    QVector<int> match(lines.size(), -1);
    QSet<int> used;
    {
        QHash<QString, int> occ;
        for (int j = 0; j < lines.size(); ++j) {
            const ScannedLine& f = lines.at(j);
            const QString base = QString::number(f.sceneIndex) + QLatin1Char('\n') + f.text;
            const int i = existingByKey.value(key(f.sceneIndex, f.text, occ[base]++), -1);
            if (i >= 0 && !used.contains(i)) { match[j] = i; used.insert(i); }
        }
    }
    // Mesmo texto em outra cena (cena reordenada, capítulo que ganhou a
    // primeira cena: sceneIndex -1 → 0).
    for (int j = 0; j < lines.size(); ++j) {
        if (match[j] >= 0) continue;
        for (int i : chIdx) {
            if (used.contains(i) || m_dialogues.at(i).text != lines.at(j).text) continue;
            match[j] = i;
            used.insert(i);
            break;
        }
    }
    // Linha editada: não perder a correção manual — casa pela parecença.
    for (int i : chIdx) {
        if (used.contains(i) || !inScope(i)) continue;
        const Dialogue& d = m_dialogues.at(i);
        const bool keepSpeaker = d.isManual()
            || (d.origin == QLatin1String("legacy") && !d.characterId.isEmpty());
        if (!keepSpeaker) continue;
        int best = -1;
        double bestSim = 0.6;
        for (int j = 0; j < lines.size(); ++j) {
            if (match[j] >= 0) continue;
            const double s = similarity(d.text, lines.at(j).text);
            if (s >= bestSim) { bestSim = s; best = j; }
        }
        if (best >= 0) { match[best] = i; used.insert(i); }
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool trustCast = !validCharacterIds.isEmpty();
    struct Placed { Dialogue d; int scene; qint64 seq; };
    QVector<Placed> placed;
    placed.reserve(lines.size() + chIdx.size());

    for (int j = 0; j < lines.size(); ++j) {
        const ScannedLine& f = lines.at(j);
        Dialogue d;
        if (match[j] >= 0) {
            d = m_dialogues.at(match[j]);
            if (d.origin == QLatin1String("legacy")) {
                // Formato 1 não diz o que foi corrigido à mão. O motor antigo
                // era determinístico: se ele mesmo daria essa resposta pro
                // texto salvo, ela foi automática e vai ser recalculada; se
                // não daria, foi você que corrigiu, e fica.
                // Também não é correção uma atribuição que contradiz o nome
                // escrito na tag ("— Você não ligou! — Wallison interrompeu"
                // salva como João): veio do bug antigo em que uma fala nova
                // herdava o locutor de outra.
                const ScannedLine& f0 = lines.at(j);
                const bool contradictsTag = f0.confidence == DialogueConfidence::Certain
                    && !f0.characterId.isEmpty() && f0.characterId != d.characterId;
                // E se o motor novo dá a mesma resposta, tanto faz: fica
                // automática (e passa a acompanhar o texto).
                const bool wasAutomatic = d.characterId.isEmpty()
                    || d.characterId == legacyNarratorId
                    || contradictsTag
                    || f0.characterId == d.characterId
                    || DialogueDetector::legacyAttribution(d.text, cast) == d.characterId;
                d.origin = wasAutomatic ? QStringLiteral("auto") : QStringLiteral("manual");
            }
        } else {
            d.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            d.origin = QStringLiteral("auto");
            d.createdAt = now;
        }
        d.text = f.text;
        d.speech = f.speech;
        d.manuscriptId = manuscriptId;
        d.chapterId = chapterId;
        d.sceneIndex = f.sceneIndex;
        d.sourceLabel = f.sourceLabel;

        const bool speakerGone = trustCast && !d.characterId.isEmpty()
                                 && !validCharacterIds.contains(d.characterId);
        if (d.isManual() && speakerGone) d.origin = QStringLiteral("auto");

        if (d.isManual()) {
            d.confidence = QStringLiteral("certain");
            d.extraLabel = f.extraLabel;
        } else {
            d.characterId = f.characterId;
            d.confidence = confidenceName(f.confidence);
            d.extraLabel = f.extraLabel;
        }
        placed.append({ d, f.sceneIndex, qint64(j) });
    }

    // O que ficou sem par.
    for (int k = 0; k < chIdx.size(); ++k) {
        const int i = chIdx.at(k);
        if (used.contains(i)) continue;
        const Dialogue& d = m_dialogues.at(i);
        if (whole) continue; // capítulo inteiro lido: saiu do texto, sai do arquivo
        const bool speakerKept = d.isManual()
            || (d.origin == QLatin1String("legacy") && !d.characterId.isEmpty());
        if (inScope(i) && !speakerKept) continue; // automática da cena lida que sumiu
        placed.append({ d, d.sceneIndex, 1000000 + k });
    }

    std::stable_sort(placed.begin(), placed.end(), [](const Placed& a, const Placed& b) {
        if (a.scene != b.scene) return a.scene < b.scene;
        return a.seq < b.seq;
    });

    QVector<Dialogue> before;
    for (int i : chIdx) before.append(m_dialogues.at(i));
    bool dirty = before.size() != placed.size();
    for (int k = 0; !dirty && k < placed.size(); ++k)
        dirty = !sameDialogue(before.at(k), placed.at(k).d);
    if (!dirty) return;

    // O bloco do capítulo volta pra onde estava na lista.
    const int insertAt = chIdx.isEmpty() ? m_dialogues.size() : chIdx.first();
    QVector<Dialogue> next;
    next.reserve(m_dialogues.size() - chIdx.size() + placed.size());
    for (int i = 0; i < m_dialogues.size(); ++i) {
        if (i == insertAt) for (const Placed& p : placed) next.append(p.d);
        if (m_dialogues.at(i).chapterId != chapterId) next.append(m_dialogues.at(i));
    }
    if (insertAt >= m_dialogues.size()) for (const Placed& p : placed) next.append(p.d);
    m_dialogues = next;
    emit changed();
}

void DialogueStore::pruneChapters(const QSet<QString>& validChapterIds)
{
    const int before = m_dialogues.size();
    m_dialogues.erase(std::remove_if(m_dialogues.begin(), m_dialogues.end(),
                                     [&](const Dialogue& d) { return !validChapterIds.contains(d.chapterId); }),
                      m_dialogues.end());
    for (auto it = m_genderVotes.begin(); it != m_genderVotes.end();) {
        if (!validChapterIds.contains(it.key())) it = m_genderVotes.erase(it);
        else ++it;
    }
    for (auto it = m_ignored.begin(); it != m_ignored.end();) {
        if (!validChapterIds.contains(it->section(QLatin1Char('\n'), 0, 0))) it = m_ignored.erase(it);
        else ++it;
    }
    if (m_dialogues.size() != before) emit changed();
}

void DialogueStore::removeChapter(const QString& chapterId)
{
    if (chapterId.isEmpty()) return;
    QSet<QString> keep;
    for (const Dialogue& d : m_dialogues) keep.insert(d.chapterId);
    for (auto it = m_genderVotes.constBegin(); it != m_genderVotes.constEnd(); ++it) keep.insert(it.key());
    keep.remove(chapterId);
    pruneChapters(keep);
}

bool DialogueStore::setChapterGenderVotes(const QString& chapterId,
                                          const DialogueDetector::GenderVotes& votes)
{
    if (chapterId.isEmpty() || m_genderVotes.value(chapterId) == votes) return false;
    if (votes.isEmpty()) m_genderVotes.remove(chapterId);
    else m_genderVotes.insert(chapterId, votes);
    return true;
}

DialogueDetector::GenderVotes DialogueStore::genderVotes() const
{
    DialogueDetector::GenderVotes sum;
    for (const auto& per : m_genderVotes) {
        for (auto it = per.constBegin(); it != per.constEnd(); ++it) {
            sum[it.key()].first += it.value().first;
            sum[it.key()].second += it.value().second;
        }
    }
    return sum;
}

int DialogueStore::replaceInTexts(const QVector<QPair<QString, QString>>& terms)
{
    if (terms.isEmpty() || m_dialogues.isEmpty()) return 0;

    QVector<QPair<QRegularExpression, QString>> compiled;
    compiled.reserve(terms.size());
    for (const auto& t : terms) {
        if (t.first.isEmpty() || t.first == t.second) continue;
        compiled.append({ QRegularExpression(
                              QStringLiteral("(?<![\\p{L}\\p{N}_])%1(?![\\p{L}\\p{N}_])")
                                  .arg(QRegularExpression::escape(t.first)),
                              QRegularExpression::UseUnicodePropertiesOption),
                          t.second });
    }
    if (compiled.isEmpty()) return 0;

    int touched = 0;
    for (Dialogue& d : m_dialogues) {
        const QString before = d.text;
        for (const auto& c : compiled) {
            d.text.replace(c.first, c.second);
            d.speech.replace(c.first, c.second);
            d.extraLabel.replace(c.first, c.second);
        }
        if (d.text != before) ++touched;
    }
    if (touched == 0) return 0;

    QSet<QString> renamedIgnored;
    for (QString k : m_ignored) {
        for (const auto& c : compiled) k.replace(c.first, c.second);
        renamedIgnored.insert(k);
    }
    m_ignored = renamedIgnored;

    // Duas falas que ficaram iguais depois da troca (mesma cena, mesmo
    // texto) viram uma só: fica a manual; empatando, a com locutor; depois
    // a mais antiga.
    QHash<QString, int> keepByKey;
    QVector<Dialogue> deduped;
    deduped.reserve(m_dialogues.size());
    for (const Dialogue& d : m_dialogues) {
        const QString k = d.chapterId + QLatin1Char('\n') + QString::number(d.sceneIndex)
                        + QLatin1Char('\n') + d.text;
        auto it = keepByKey.constFind(k);
        if (it == keepByKey.constEnd()) {
            keepByKey.insert(k, deduped.size());
            deduped.append(d);
            continue;
        }
        Dialogue& kept = deduped[it.value()];
        if (!kept.isManual() && (d.isManual() || (kept.characterId.isEmpty() && !d.characterId.isEmpty()))) {
            const qint64 created = kept.createdAt;
            kept = d;
            kept.createdAt = created;
        }
        if (d.createdAt > 0 && (kept.createdAt == 0 || d.createdAt < kept.createdAt))
            kept.createdAt = d.createdAt;
    }
    m_dialogues = deduped;

    save();
    emit changed();
    return touched;
}

double DialogueStore::textSimilarity(const QString& a, const QString& b)
{
    return similarity(a, b);
}

bool DialogueStore::remove(const QString& id)
{
    for (int i = 0; i < m_dialogues.size(); ++i) {
        if (m_dialogues.at(i).id != id) continue;
        m_ignored.insert(ignoreKey(m_dialogues.at(i).chapterId, m_dialogues.at(i).text));
        m_dialogues.removeAt(i);
        emit changed();
        return true;
    }
    return false;
}

bool DialogueStore::setCharacter(const QString& id, const QString& newCharacterId)
{
    if (newCharacterId.isEmpty()) return false;
    for (Dialogue& d : m_dialogues) {
        if (d.id != id) continue;
        if (d.isManual() && d.characterId == newCharacterId) return false;
        d.characterId = newCharacterId;
        d.origin = QStringLiteral("manual");
        d.confidence = QStringLiteral("certain");
        emit changed();
        return true;
    }
    return false;
}
