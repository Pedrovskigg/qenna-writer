#include "WordCounter.h"

#include "DocCache.h"
#include "EditorHost.h"
#include "ProjectModel.h"
#include "ProjectStorage.h"
#include "SceneUtils.h"

#include <QDateTime>
#include <QJsonValue>
#include <QRegularExpression>
#include <QTimer>

namespace {

constexpr qint64 kGoalDayMs = 24LL * 60 * 60 * 1000;
constexpr int kTimeTickMs = 1000;
constexpr int kCursorIdleMs = 4000;
constexpr int kDeltaMaxMs = 5000;

QString stripTagsAndEntities(const QString& html) {
    static const QRegularExpression styleBlockRe(
        QStringLiteral("<style\\b[^>]*>.*?</style\\s*>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression scriptBlockRe(
        QStringLiteral("<script\\b[^>]*>.*?</script\\s*>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression commentRe(
        QStringLiteral("<!--.*?-->"),
        QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression tagRe(QStringLiteral("<[^>]*>"));
    static const QRegularExpression entityRe(QStringLiteral("&[#A-Za-z0-9]+;"));
    QString text = html;
    text.replace(styleBlockRe, QStringLiteral(" "));
    text.replace(scriptBlockRe, QStringLiteral(" "));
    text.replace(commentRe, QStringLiteral(" "));
    text.replace(tagRe, QStringLiteral(" "));
    text.replace(entityRe, QStringLiteral(" "));
    return text;
}

QString dateKey(const QDateTime& dt) {
    return dt.toString(QStringLiteral("yyyy-MM-dd"));
}

QString dateKey(qint64 epochMs) {
    return dateKey(QDateTime::fromMSecsSinceEpoch(epochMs));
}

}

QJsonObject WordCounterSettings::toJson() const {
    QJsonObject o;
    o.insert(QStringLiteral("scope"), scope);
    o.insert(QStringLiteral("goalScope"), goalScope);
    o.insert(QStringLiteral("goalType"), goalType);
    o.insert(QStringLiteral("goalTargetWords"), goalTargetWords);
    o.insert(QStringLiteral("goalTargetMinutes"), goalTargetMinutes);
    o.insert(QStringLiteral("goalDayStartAt"), goalDayStartAt);
    o.insert(QStringLiteral("goalDayKey"), goalDayKey);
    o.insert(QStringLiteral("progress"), progress);
    o.insert(QStringLiteral("offDays"), offDays);
    o.insert(QStringLiteral("offDayEvery"), offDayEvery);
    o.insert(QStringLiteral("offDayEveryChangedAt"), offDayEveryChangedAt);
    o.insert(QStringLiteral("folgasEarnedAtChange"), folgasEarnedAtChange);
    o.insert(QStringLiteral("compactSlot1"), compactSlot1);
    o.insert(QStringLiteral("compactSlot1Scope"), compactSlot1Scope);
    o.insert(QStringLiteral("compactSlot2"), compactSlot2);
    o.insert(QStringLiteral("compactSlot2Scope"), compactSlot2Scope);
    o.insert(QStringLiteral("compactShowGoalBar"), compactShowGoalBar);
    return o;
}

WordCounterSettings WordCounterSettings::fromJson(const QJsonObject& o) {
    WordCounterSettings s;
    if (o.contains(QStringLiteral("scope"))) s.scope = o.value(QStringLiteral("scope")).toString(s.scope);
    if (o.contains(QStringLiteral("goalScope"))) s.goalScope = o.value(QStringLiteral("goalScope")).toString(s.goalScope);
    if (o.contains(QStringLiteral("goalType"))) s.goalType = o.value(QStringLiteral("goalType")).toString(s.goalType);
    s.goalTargetWords = o.value(QStringLiteral("goalTargetWords")).toInt(s.goalTargetWords);
    s.goalTargetMinutes = o.value(QStringLiteral("goalTargetMinutes")).toInt(s.goalTargetMinutes);
    if (o.contains(QStringLiteral("goalDayStartAt"))) s.goalDayStartAt = static_cast<qint64>(o.value(QStringLiteral("goalDayStartAt")).toDouble(0));
    s.goalDayKey = o.value(QStringLiteral("goalDayKey")).toString();
    s.progress = o.value(QStringLiteral("progress")).toObject();
    s.offDays = o.value(QStringLiteral("offDays")).toObject();
    s.offDayEvery = o.value(QStringLiteral("offDayEvery")).toInt(7);
    s.offDayEveryChangedAt = o.value(QStringLiteral("offDayEveryChangedAt")).toString();
    s.folgasEarnedAtChange = o.value(QStringLiteral("folgasEarnedAtChange")).toInt(0);
    s.compactSlot1 = o.value(QStringLiteral("compactSlot1")).toString(s.compactSlot1);
    s.compactSlot1Scope = o.value(QStringLiteral("compactSlot1Scope")).toString(s.compactSlot1Scope);
    s.compactSlot2 = o.value(QStringLiteral("compactSlot2")).toString(s.compactSlot2);
    s.compactSlot2Scope = o.value(QStringLiteral("compactSlot2Scope")).toString(s.compactSlot2Scope);
    s.compactShowGoalBar = o.value(QStringLiteral("compactShowGoalBar")).toBool(true);
    return s;
}

int WordCounter::countWordsInPlain(const QString& text) {
    if (text.isEmpty()) return 0;
    int count = 0;
    bool inWord = false;
    for (const QChar& ch : text) {
        const bool isWordChar = ch.isLetterOrNumber() || ch == QLatin1Char('\'') || ch == QChar(0x2019);
        if (isWordChar) {
            if (!inWord) { inWord = true; ++count; }
        } else {
            inWord = false;
        }
    }
    return count;
}

int WordCounter::countWordsInHtml(const QString& html) {
    if (html.isEmpty()) return 0;
    return countWordsInPlain(stripTagsAndEntities(html));
}

int WordCounter::countCharsInHtml(const QString& html) {
    if (html.isEmpty()) return 0;
    QString plain = stripTagsAndEntities(html);
    static const QRegularExpression wsRe(QStringLiteral("\\s+"));
    plain.replace(wsRe, QStringLiteral(" "));
    return plain.trimmed().size();
}

WordCounter::WordCounter(ProjectModel* model, DocCache* cache, EditorHost* host, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_cache(cache)
    , m_host(host)
    , m_emitDebounce(new QTimer(this))
    , m_timeTickTimer(new QTimer(this))
{
    m_emitDebounce->setSingleShot(true);
    m_emitDebounce->setInterval(250);
    connect(m_emitDebounce, &QTimer::timeout, this, &WordCounter::emitChange);

    m_timeTickTimer->setInterval(kTimeTickMs);
    connect(m_timeTickTimer, &QTimer::timeout, this, &WordCounter::onTimeTick);
    m_timeTickTimer->start();

    if (m_cache) {
        connect(m_cache, &DocCache::contentChanged, this, &WordCounter::onCacheContentChanged);
    }
    if (m_model) {
        connect(m_model, &ProjectModel::chaptersChanged, this, &WordCounter::onChaptersChanged);
        connect(m_model, &ProjectModel::drawersChanged, this, &WordCounter::onDrawersChanged);
        connect(m_model, &ProjectModel::loaded, this, &WordCounter::onProjectLoaded);
        connect(m_model, &ProjectModel::settingsChanged, this, [this]() { loadSettingsFromModel(); });
    }
    if (m_host) {
        connect(m_host, &EditorHost::contentFlushed, this, &WordCounter::onEditorContentFlushed);
    }

    loadSettingsFromModel();
}

void WordCounter::setProjectRoot(const QString& root) {
    m_root = root;
    m_chapterCounts.clear();
    m_itemCounts.clear();
    m_chapterCharCounts.clear();
    m_itemCharCounts.clear();
    m_goalWordSnapshot.clear();
    scheduleEmit();
}

void WordCounter::onCacheContentChanged(const QString& key) {
    if (key.startsWith(QStringLiteral("ch:"))) {
        const int secondColon = key.indexOf(QLatin1Char(':'), 3);
        if (secondColon > 0) {
            const QString chId = key.mid(secondColon + 1);
            m_chapterCounts.remove(chId);
            m_chapterCharCounts.remove(chId);
        }
    } else if (key.startsWith(QStringLiteral("it:"))) {
        const QString itemId = key.mid(3);
        m_itemCounts.remove(itemId);
        m_itemCharCounts.remove(itemId);
    }
    scheduleEmit();
}

void WordCounter::onChaptersChanged() {
    scheduleEmit();
}

void WordCounter::onDrawersChanged() {
    if (m_model) {
        auto sweep = [this](QHash<QString,int>& hash) {
            for (auto it = hash.begin(); it != hash.end(); ) {
                const QString id = it.key();
                const DrawerItem* item = m_model->findDrawerItem(id);
                if (!item || (item->file.isEmpty() && item->hasInlineHtml)) {
                    it = hash.erase(it);
                } else {
                    ++it;
                }
            }
        };
        sweep(m_itemCounts);
        sweep(m_itemCharCounts);
    }
    scheduleEmit();
}

void WordCounter::onProjectLoaded() {
    m_chapterCounts.clear();
    m_itemCounts.clear();
    m_chapterCharCounts.clear();
    m_itemCharCounts.clear();
    m_goalWordSnapshot.clear();
    loadSettingsFromModel();
    scheduleEmit();
}

void WordCounter::scheduleEmit() {
    if (!m_emitDebounce->isActive()) m_emitDebounce->start();
}

void WordCounter::emitChange() {
    emit countsChanged();
}

QString WordCounter::chapterHtml(const QString& chapterId) const {
    if (!m_model) return QString();
    const Chapter* ch = m_model->findChapter(chapterId);
    if (!ch) return QString();
    const QString key = DocCache::chapterKey(ch->manuscriptId, ch->id);
    if (m_cache && m_cache->has(key)) return m_cache->get(key);
    if (!m_root.isEmpty() && !ch->file.isEmpty()) {
        bool ok = false;
        const QString html = ProjectStorage::readChapter(m_root, ch->file, &ok);
        return ok ? html : QString();
    }
    return QString();
}

QString WordCounter::itemHtml(const QString& itemId) const {
    if (!m_model) return QString();
    const DrawerItem* item = m_model->findDrawerItem(itemId);
    if (!item) return QString();
    const QString key = DocCache::itemKey(itemId);
    if (m_cache && m_cache->has(key)) return m_cache->get(key);
    if (!item->file.isEmpty() && !m_root.isEmpty()) {
        const QString absPath = ProjectStorage::joinPath(m_root, item->file);
        bool ok = false;
        const QString html = ProjectStorage::readText(absPath, &ok);
        return ok ? html : QString();
    }
    return item->hasInlineHtml ? item->html : QString();
}

int WordCounter::countScene(const QString& chapterId, int sceneIndex) const {
    const QString chHtml = chapterHtml(chapterId);
    const QString segment = SceneUtils::getSceneHtml(chHtml, sceneIndex);
    return countWordsInHtml(segment);
}

int WordCounter::countChapter(const QString& chapterId) const {
    auto it = m_chapterCounts.constFind(chapterId);
    if (it != m_chapterCounts.constEnd()) return it.value();
    const QString html = chapterHtml(chapterId);
    const int n = countWordsInHtml(html);
    m_chapterCounts.insert(chapterId, n);
    return n;
}

int WordCounter::countManuscript(const QString& manuscriptId) const {
    if (!m_model) return 0;
    int total = 0;
    for (const auto& ch : m_model->chapters()) {
        if (ch.manuscriptId == manuscriptId) total += countChapter(ch.id);
    }
    return total;
}

int WordCounter::countItem(const QString& itemId) const {
    auto it = m_itemCounts.constFind(itemId);
    if (it != m_itemCounts.constEnd()) return it.value();
    const QString html = itemHtml(itemId);
    const int n = countWordsInHtml(html);
    m_itemCounts.insert(itemId, n);
    return n;
}

int WordCounter::countDrawer(const QString& drawerKey) const {
    if (!m_model) return 0;
    const Drawer* d = m_model->findDrawer(drawerKey);
    if (!d) return 0;
    int total = 0;
    for (const auto& item : d->items) total += countItem(item.id);
    return total;
}

int WordCounter::countProject() const {
    if (!m_model) return 0;
    int total = 0;
    for (const auto& ch : m_model->chapters()) total += countChapter(ch.id);
    for (const auto& d : m_model->drawers())
        for (const auto& item : d.items) total += countItem(item.id);
    return total;
}

int WordCounter::countProjectChars() const {
    if (!m_model) return 0;
    int total = 0;
    for (const auto& ch : m_model->chapters()) {
        auto it = m_chapterCharCounts.constFind(ch.id);
        if (it != m_chapterCharCounts.constEnd()) total += it.value();
        else {
            const int n = countCharsInHtml(chapterHtml(ch.id));
            m_chapterCharCounts.insert(ch.id, n);
            total += n;
        }
    }
    for (const auto& d : m_model->drawers())
        for (const auto& item : d.items) {
            auto it = m_itemCharCounts.constFind(item.id);
            if (it != m_itemCharCounts.constEnd()) total += it.value();
            else {
                const int n = countCharsInHtml(itemHtml(item.id));
                m_itemCharCounts.insert(item.id, n);
                total += n;
            }
        }
    return total;
}

int WordCounter::countActiveScopeWords() const {
    if (!m_model) return 0;
    const QString sc = m_settings.scope;
    if (sc == QStringLiteral("active") && m_host) {
        const auto vm = m_host->viewMode();
        switch (vm.type) {
        case EditorHost::SceneDoc: return countScene(vm.chapterId, vm.sceneIndex);
        case EditorHost::ChapterDoc: return countChapter(vm.chapterId);
        case EditorHost::DrawerDoc: return countItem(vm.itemId);
        default: return 0;
        }
    }
    if (sc == QStringLiteral("drawers")) {
        int total = 0;
        for (const auto& d : m_model->drawers())
            for (const auto& item : d.items) total += countItem(item.id);
        return total;
    }
    if (sc == QStringLiteral("all")) return countProject();
    // default: "manuscript" — todos os chapters
    int total = 0;
    for (const auto& ch : m_model->chapters()) total += countChapter(ch.id);
    return total;
}

int WordCounter::countActiveScopeChars() const {
    if (!m_model) return 0;
    const QString sc = m_settings.scope;
    auto charsForChapter = [this](const QString& chId) -> int {
        auto it = m_chapterCharCounts.constFind(chId);
        if (it != m_chapterCharCounts.constEnd()) return it.value();
        const int n = countCharsInHtml(chapterHtml(chId));
        m_chapterCharCounts.insert(chId, n);
        return n;
    };
    auto charsForItem = [this](const QString& id) -> int {
        auto it = m_itemCharCounts.constFind(id);
        if (it != m_itemCharCounts.constEnd()) return it.value();
        const int n = countCharsInHtml(itemHtml(id));
        m_itemCharCounts.insert(id, n);
        return n;
    };
    if (sc == QStringLiteral("active") && m_host) {
        const auto vm = m_host->viewMode();
        switch (vm.type) {
        case EditorHost::SceneDoc: {
            const QString seg = SceneUtils::getSceneHtml(chapterHtml(vm.chapterId), vm.sceneIndex);
            return countCharsInHtml(seg);
        }
        case EditorHost::ChapterDoc: return charsForChapter(vm.chapterId);
        case EditorHost::DrawerDoc: return charsForItem(vm.itemId);
        default: return 0;
        }
    }
    if (sc == QStringLiteral("drawers")) {
        int total = 0;
        for (const auto& d : m_model->drawers())
            for (const auto& item : d.items) total += charsForItem(item.id);
        return total;
    }
    if (sc == QStringLiteral("all")) return countProjectChars();
    int total = 0;
    for (const auto& ch : m_model->chapters()) total += charsForChapter(ch.id);
    return total;
}

void WordCounter::loadSettingsFromModel() {
    if (!m_model) return;
    const QJsonObject all = m_model->settings();
    const QJsonObject wc = all.value(QStringLiteral("wordCounter")).toObject();
    m_settings = WordCounterSettings::fromJson(wc);
    // Semeia offDayEveryChangedAt com hoje (lock começa a partir de agora pra projetos antigos)
    if (m_settings.offDayEveryChangedAt.isEmpty()) {
        m_settings.offDayEveryChangedAt = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd"));
    }
    emit settingsChanged();
    scheduleEmit();
}

void WordCounter::writeSettingsToModel() {
    if (!m_model) return;
    QJsonObject all = m_model->settings();
    all.insert(QStringLiteral("wordCounter"), m_settings.toJson());
    m_model->setSettings(all);
}

void WordCounter::setScope(const QString& scope) {
    if (m_settings.scope == scope) return;
    m_settings.scope = scope;
    writeSettingsToModel();
    emit settingsChanged();
    scheduleEmit();
}

void WordCounter::setGoalScope(const QString& goalScope) {
    if (m_settings.goalScope == goalScope) return;
    m_settings.goalScope = goalScope;
    writeSettingsToModel();
    emit settingsChanged();
}

void WordCounter::setGoalType(const QString& goalType) {
    if (m_settings.goalType == goalType) return;
    m_settings.goalType = goalType;
    writeSettingsToModel();
    emit settingsChanged();
}

void WordCounter::setGoalTargetWords(int words) {
    if (m_settings.goalTargetWords == words) return;
    m_settings.goalTargetWords = words;
    writeSettingsToModel();
    emit settingsChanged();
}

void WordCounter::setGoalTargetMinutes(int minutes) {
    if (m_settings.goalTargetMinutes == minutes) return;
    m_settings.goalTargetMinutes = minutes;
    writeSettingsToModel();
    emit settingsChanged();
}

void WordCounter::ensureCurrentDayKey() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_settings.goalDayStartAt == 0 || (now - m_settings.goalDayStartAt) >= kGoalDayMs) {
        m_settings.goalDayStartAt = now;
        m_settings.goalDayKey = dateKey(now);
    } else if (m_settings.goalDayKey.isEmpty()) {
        m_settings.goalDayKey = dateKey(m_settings.goalDayStartAt);
    }
}

QString WordCounter::currentGoalDayKey() const {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_settings.goalDayStartAt == 0 || (now - m_settings.goalDayStartAt) >= kGoalDayMs) {
        return dateKey(now);
    }
    return m_settings.goalDayKey.isEmpty() ? dateKey(m_settings.goalDayStartAt) : m_settings.goalDayKey;
}

qint64 WordCounter::currentGoalDayStartAt() const {
    return m_settings.goalDayStartAt;
}

qint64 WordCounter::goalDayRemainingMs() const {
    if (m_settings.goalDayStartAt == 0) return kGoalDayMs;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 elapsed = now - m_settings.goalDayStartAt;
    return qMax<qint64>(0, kGoalDayMs - elapsed);
}

int WordCounter::progressWords() const {
    const QString key = currentGoalDayKey();
    const QJsonObject today = m_settings.progress.value(key).toObject();
    return today.value(QStringLiteral("words")).toInt(0);
}

qint64 WordCounter::progressTimeMs() const {
    const QString key = currentGoalDayKey();
    const QJsonObject today = m_settings.progress.value(key).toObject();
    return static_cast<qint64>(today.value(QStringLiteral("timeMs")).toDouble(0));
}

bool WordCounter::isGoalMet() const {
    if (m_settings.goalType == QStringLiteral("time")) {
        return progressTimeMs() >= static_cast<qint64>(m_settings.goalTargetMinutes) * 60000;
    }
    return progressWords() >= m_settings.goalTargetWords;
}

void WordCounter::resetGoalDay() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QString key = dateKey(now);
    // Zera o progresso do dia atual (mantém histórico anterior intacto).
    m_settings.progress.remove(key);
    m_settings.goalDayStartAt = now;
    m_settings.goalDayKey = key;
    // Reseta também o snapshot de palavras pra não contar como delta o que já estava no editor.
    m_goalWordSnapshot.clear();
    writeSettingsToModel();
    emit progressChanged();
    emit settingsChanged();
}

bool WordCounter::dayMetGoal(const QString& dateKey) const {
    const QJsonObject day = m_settings.progress.value(dateKey).toObject();
    if (day.isEmpty()) return false;
    const QString type = day.value(QStringLiteral("goalType")).toString(m_settings.goalType);
    const int tWords = day.value(QStringLiteral("goalTargetWords")).toInt(m_settings.goalTargetWords);
    const int tMinutes = day.value(QStringLiteral("goalTargetMinutes")).toInt(m_settings.goalTargetMinutes);
    if (type == QStringLiteral("time")) {
        const qint64 ms = static_cast<qint64>(day.value(QStringLiteral("timeMs")).toDouble(0));
        return tMinutes > 0 && ms >= static_cast<qint64>(tMinutes) * 60000;
    }
    return tWords > 0 && day.value(QStringLiteral("words")).toInt(0) >= tWords;
}

WordCounter::OffDayType WordCounter::offDayType(const QString& dateKey) const {
    const QJsonValue v = m_settings.offDays.value(dateKey);
    if (v.isUndefined()) return OffDayType::None;
    if (v.isString() && v.toString() == QStringLiteral("stolen")) return OffDayType::Stolen;
    if (v.toBool(false)) return OffDayType::Legit;
    return OffDayType::None;
}

int WordCounter::remainingFolgas() const {
    const int every = m_settings.offDayEvery;
    if (every == 999) return 0;          // nunca
    if (every == 0) return 999;          // ilimitado
    const QString since = m_settings.offDayEveryChangedAt;
    int metSince = 0;
    int usedLegit = 0;
    const QJsonObject& prog = m_settings.progress;
    for (auto it = prog.begin(); it != prog.end(); ++it) {
        const QString k = it.key();
        if (since.isEmpty() || k >= since) {
            if (dayMetGoal(k)) ++metSince;
        }
    }
    for (auto it = m_settings.offDays.begin(); it != m_settings.offDays.end(); ++it) {
        if (offDayType(it.key()) == OffDayType::Legit) ++usedLegit;
    }
    const int earnedSince = metSince / every;
    return qMax(0, m_settings.folgasEarnedAtChange + earnedSince - usedLegit);
}

bool WordCounter::setOffDay(const QString& dateKey, OffDayType type) {
    if (type == OffDayType::None) {
        if (!m_settings.offDays.contains(dateKey)) return false;
        m_settings.offDays.remove(dateKey);
    } else if (type == OffDayType::Legit) {
        m_settings.offDays.insert(dateKey, true);
    } else { // Stolen
        m_settings.offDays.insert(dateKey, QStringLiteral("stolen"));
    }
    writeSettingsToModel();
    emit settingsChanged();
    emit progressChanged();
    return true;
}

bool WordCounter::setOffDayEvery(int every) {
    if (m_settings.offDayEvery == every) return true;
    // Banking: salva ganhas até agora antes de mudar.
    const int prevRemaining = remainingFolgas();
    // Soma folgas usadas (legit) com remaining → total ganho até agora.
    int usedLegit = 0;
    for (auto it = m_settings.offDays.begin(); it != m_settings.offDays.end(); ++it) {
        if (offDayType(it.key()) == OffDayType::Legit) ++usedLegit;
    }
    m_settings.folgasEarnedAtChange = prevRemaining + usedLegit;
    m_settings.offDayEvery = every;
    m_settings.offDayEveryChangedAt = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd"));
    writeSettingsToModel();
    emit settingsChanged();
    return true;
}

int WordCounter::daysUntilOffDayChange() const {
    const int every = m_settings.offDayEvery;
    if (every == 0 || every == 999) return 0; // sem lock
    const QString since = m_settings.offDayEveryChangedAt;
    if (since.isEmpty()) return 0;
    int metSince = 0;
    const QJsonObject& prog = m_settings.progress;
    for (auto it = prog.begin(); it != prog.end(); ++it) {
        if (it.key() >= since && dayMetGoal(it.key())) ++metSince;
    }
    return qMax(0, every - metSince);
}

bool WordCounter::isOffDayChangeLocked() const {
    return daysUntilOffDayChange() > 0;
}

int WordCounter::currentStreak() const {
    const QDate today = QDate::currentDate();
    const QString todayKey = today.toString(QStringLiteral("yyyy-MM-dd"));
    const bool todayMet = dayMetGoal(todayKey);
    const bool todayIsOff = m_settings.offDays.contains(todayKey);
    const int startOffset = (todayMet || todayIsOff) ? 0 : 1;
    int streak = 0;
    for (int i = startOffset; i < 3650; ++i) {
        const QDate d = today.addDays(-i);
        const QString key = d.toString(QStringLiteral("yyyy-MM-dd"));
        if (m_settings.offDays.contains(key)) continue;
        if (!dayMetGoal(key)) break;
        ++streak;
    }
    return streak;
}

int WordCounter::longestStreak() const {
    QStringList keys = m_settings.progress.keys();
    keys.sort();
    int best = 0, current = 0;
    QDate previousDate;
    for (const QString& key : keys) {
        const QDate date = QDate::fromString(key, QStringLiteral("yyyy-MM-dd"));
        if (!date.isValid()) continue;
        const bool met = dayMetGoal(key);
        if (!met) {
            current = 0;
            previousDate = date;
            continue;
        }
        if (!previousDate.isValid()) {
            current = 1;
        } else {
            const int diffDays = previousDate.daysTo(date);
            if (diffDays == 1) {
                current += 1;
            } else {
                bool allOff = true;
                for (int i = 1; i < diffDays; ++i) {
                    const QString iKey = previousDate.addDays(i).toString(QStringLiteral("yyyy-MM-dd"));
                    if (!m_settings.offDays.contains(iKey)) { allOff = false; break; }
                }
                current = allOff ? current + 1 : 1;
            }
        }
        if (current > best) best = current;
        previousDate = date;
    }
    return best;
}

int WordCounter::estimatedPages() const {
    const int w = countActiveScopeWords();
    if (w <= 0) return 0;
    return qMax(1, (w + 249) / 250);
}

void WordCounter::writingAverages(int& activeDays, int& wordsPerDay, int& minutesPerDay) const {
    activeDays = 0;
    int totalWords = 0;
    qint64 totalTimeMs = 0;
    const QJsonObject& prog = m_settings.progress;
    for (auto it = prog.begin(); it != prog.end(); ++it) {
        const QJsonObject day = it.value().toObject();
        const int w = day.value(QStringLiteral("words")).toInt(0);
        const qint64 t = static_cast<qint64>(day.value(QStringLiteral("timeMs")).toDouble(0));
        if (w <= 0 && t <= 0) continue;
        ++activeDays;
        totalWords += w;
        totalTimeMs += t;
    }
    if (activeDays == 0) { wordsPerDay = 0; minutesPerDay = 0; return; }
    wordsPerDay = totalWords / activeDays;
    minutesPerDay = static_cast<int>((totalTimeMs / activeDays) / 60000);
}

int WordCounter::sessionWordsManuscript() const {
    const QString key = currentGoalDayKey();
    return m_settings.progress.value(key).toObject()
        .value(QStringLiteral("wordsManuscript")).toInt(0);
}

int WordCounter::sessionWordsAll() const {
    const QString key = currentGoalDayKey();
    return m_settings.progress.value(key).toObject()
        .value(QStringLiteral("wordsAll")).toInt(0);
}

void WordCounter::setCompactSlot1(const QString& metric, const QString& scope) {
    if (m_settings.compactSlot1 == metric && m_settings.compactSlot1Scope == scope) return;
    m_settings.compactSlot1 = metric;
    m_settings.compactSlot1Scope = scope;
    writeSettingsToModel();
    emit settingsChanged();
}

void WordCounter::setCompactSlot2(const QString& metric, const QString& scope) {
    if (m_settings.compactSlot2 == metric && m_settings.compactSlot2Scope == scope) return;
    m_settings.compactSlot2 = metric;
    m_settings.compactSlot2Scope = scope;
    writeSettingsToModel();
    emit settingsChanged();
}

void WordCounter::setCompactShowGoalBar(bool show) {
    if (m_settings.compactShowGoalBar == show) return;
    m_settings.compactShowGoalBar = show;
    writeSettingsToModel();
    emit settingsChanged();
}

void WordCounter::updateGoalProgress(int deltaGoalWords, qint64 deltaTimeMs, int deltaSessionManuscript, int deltaSessionAll) {
    const bool any = deltaGoalWords > 0 || deltaTimeMs > 0 || deltaSessionManuscript > 0 || deltaSessionAll > 0;
    if (!any) return;
    ensureCurrentDayKey();
    const QString key = m_settings.goalDayKey;
    QJsonObject today = m_settings.progress.value(key).toObject();
    if (deltaGoalWords > 0)
        today.insert(QStringLiteral("words"),
            today.value(QStringLiteral("words")).toInt(0) + deltaGoalWords);
    if (deltaTimeMs > 0)
        today.insert(QStringLiteral("timeMs"), static_cast<double>(
            static_cast<qint64>(today.value(QStringLiteral("timeMs")).toDouble(0)) + deltaTimeMs));
    if (deltaSessionManuscript > 0)
        today.insert(QStringLiteral("wordsManuscript"),
            today.value(QStringLiteral("wordsManuscript")).toInt(0) + deltaSessionManuscript);
    if (deltaSessionAll > 0)
        today.insert(QStringLiteral("wordsAll"),
            today.value(QStringLiteral("wordsAll")).toInt(0) + deltaSessionAll);
    if (!today.contains(QStringLiteral("goalType"))) {
        today.insert(QStringLiteral("goalType"), m_settings.goalType);
        today.insert(QStringLiteral("goalTargetWords"), m_settings.goalTargetWords);
        today.insert(QStringLiteral("goalTargetMinutes"), m_settings.goalTargetMinutes);
    }
    m_settings.progress.insert(key, today);
    writeSettingsToModel();
    emit progressChanged();
}

bool WordCounter::shouldCountTimeNow() const {
    if (!m_host) return false;
    const auto vm = m_host->viewMode();
    const QString gs = m_settings.goalScope;
    auto isManuscriptView = [](EditorHost::ViewModeType t) {
        return t == EditorHost::ChapterDoc || t == EditorHost::SceneDoc || t == EditorHost::ManuscriptDoc;
    };
    if (gs == QStringLiteral("manuscript")) return isManuscriptView(vm.type);
    if (gs == QStringLiteral("all-items-manuscript")) return isManuscriptView(vm.type) || vm.type == EditorHost::DrawerDoc;
    if (gs == QStringLiteral("all-items")) return vm.type == EditorHost::DrawerDoc;
    return false;
}

QString WordCounter::viewModeScope() const {
    if (!m_host) return QString();
    const auto vm = m_host->viewMode();
    if (vm.type == EditorHost::ChapterDoc || vm.type == EditorHost::SceneDoc) return QStringLiteral("manuscript");
    if (vm.type == EditorHost::DrawerDoc) return QStringLiteral("drawer");
    return QString();
}

QString WordCounter::keyForCurrentEdit() const {
    if (!m_host) return QString();
    return m_host->activeKey();
}

void WordCounter::registerCursorActivity() {
    m_lastCursorActivityAt = QDateTime::currentMSecsSinceEpoch();
}

void WordCounter::onTimeTick() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 lastTick = m_lastTimeTickAt > 0 ? m_lastTimeTickAt : now;
    m_lastTimeTickAt = now;
    if (m_lastCursorActivityAt == 0) return;
    const bool recentlyActive = (now - m_lastCursorActivityAt) <= kCursorIdleMs;
    if (!recentlyActive) return;
    if (!shouldCountTimeNow()) return;
    const qint64 delta = now - lastTick;
    if (delta > 0 && delta < kDeltaMaxMs) {
        updateGoalProgress(0, delta, 0, 0);
    }
}

void WordCounter::onEditorContentFlushed(const QString& key) {
    if (!m_model) return;
    const bool isChapter = key.startsWith(QStringLiteral("ch:"));
    const bool isItem = key.startsWith(QStringLiteral("it:"));
    if (!isChapter && !isItem) return;

    const QString html = m_cache ? m_cache->get(key) : QString();
    const int now = countWordsInHtml(html);
    const auto it = m_goalWordSnapshot.constFind(key);
    const bool hasPrev = (it != m_goalWordSnapshot.constEnd());
    const int prev = hasPrev ? it.value() : 0;
    m_goalWordSnapshot.insert(key, now);
    if (!hasPrev) return;
    const int delta = now - prev;
    if (delta <= 0) return;

    // Filtra meta diária pelo goalScope; métricas de sessão sempre rastreiam.
    const QString gs = m_settings.goalScope;
    const bool countsForGoal =
        (gs == QStringLiteral("manuscript") && isChapter) ||
        (gs == QStringLiteral("all-items") && isItem) ||
        gs == QStringLiteral("all-items-manuscript");

    updateGoalProgress(
        countsForGoal ? delta : 0,
        0,
        isChapter ? delta : 0,   // wordsManuscript
        delta                    // wordsAll (capítulos + gavetas)
    );
}
