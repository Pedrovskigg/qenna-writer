#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>

class ProjectModel;
class DocCache;
class EditorHost;
class QTimer;

// Configurações persistidas em settings.wordCounter do project.mira.json.
// Espelha o DEFAULT_WORD_COUNTER do Mira 1.
struct WordCounterSettings {
    // Escopo dos cards do contador (no que conta "palavras totais"):
    //   "manuscript" | "active" | "drawers" | "all"
    QString scope = QStringLiteral("manuscript");

    // Escopo da meta diária:
    //   "manuscript" — só conta progresso quando estiver em chapter/scene
    //   "all-items"  — só em drawer
    //   "all-items-manuscript" — em ambos
    QString goalScope = QStringLiteral("manuscript");

    // "words" | "time"
    QString goalType = QStringLiteral("words");
    int goalTargetWords = 500;
    int goalTargetMinutes = 30;

    // Início do "dia" da meta (epoch ms). Dia dura 24h.
    qint64 goalDayStartAt = 0;
    QString goalDayKey;

    // progress[YYYY-MM-DD] = { words, timeMs, goalType, goalTargetWords, goalTargetMinutes }
    QJsonObject progress;

    // Folgas (Fase 3) — campos reservados, ainda não usados na Fase 1.
    QJsonObject offDays;
    int offDayEvery = 7;
    QString offDayEveryChangedAt;
    int folgasEarnedAtChange = 0;

    // Personalização do contador compacto
    QString compactSlot1 = QStringLiteral("words");
    QString compactSlot1Scope;          // "" | "manuscript" | "all" (relevante só em métricas "de sessão")
    QString compactSlot2 = QStringLiteral("chars");
    QString compactSlot2Scope;
    bool compactShowGoalBar = true;

    QJsonObject toJson() const;
    static WordCounterSettings fromJson(const QJsonObject& o);
};

// Conta palavras por escopo. Mantém cache interno por key (chapter/item) pra
// evitar re-leitura/relayout. Cache é invalidado por sinais do model e do cache de docs.
class WordCounter : public QObject {
    Q_OBJECT
public:
    WordCounter(ProjectModel* model, DocCache* cache, EditorHost* host, QObject* parent = nullptr);

    void setProjectRoot(const QString& root);

    int countScene(const QString& chapterId, int sceneIndex) const;
    int countChapter(const QString& chapterId) const;
    int countManuscript(const QString& manuscriptId) const;
    int countItem(const QString& itemId) const;
    int countDrawer(const QString& drawerKey) const;
    int countProject() const;

    int countProjectChars() const;

    // Contagem segundo escopo ativo dos cards (settings.scope).
    int countActiveScopeWords() const;
    int countActiveScopeChars() const;

    // Settings — getters e setters granulares (escrevem em project.settings.wordCounter)
    WordCounterSettings settings() const { return m_settings; }
    void setScope(const QString& scope);
    void setGoalScope(const QString& goalScope);
    void setGoalType(const QString& goalType);
    void setGoalTargetWords(int words);
    void setGoalTargetMinutes(int minutes);

    // Meta diária
    QString currentGoalDayKey() const;
    qint64 currentGoalDayStartAt() const;
    int progressWords() const;
    qint64 progressTimeMs() const;
    bool isGoalMet() const;
    qint64 goalDayRemainingMs() const;

    // Reseta o "dia" da meta agora (botão ↻).
    void resetGoalDay();

    // Folgas — Mira 1 Style.
    enum class OffDayType { None, Legit, Stolen };
    int remainingFolgas() const;
    OffDayType offDayType(const QString& dateKey) const;
    bool setOffDay(const QString& dateKey, OffDayType type);
    // Aplica nova cadência. Faz banking: salva folgas ganhas até agora em folgasEarnedAtChange
    // e marca offDayEveryChangedAt = hoje. Lock só desbloqueia após N dias batidos desde então.
    bool setOffDayEvery(int every);
    int daysUntilOffDayChange() const; // 0 quando desbloqueado
    bool isOffDayChangeLocked() const;

    // Dia bateu meta (considerando snapshot do dia).
    bool dayMetGoal(const QString& dateKey) const;

    // Estatísticas
    int currentStreak() const;
    int longestStreak() const;
    int estimatedPages() const;   // palavras do escopo ativo / 250
    void writingAverages(int& activeDays, int& wordsPerDay, int& minutesPerDay) const;

    // Palavras escritas hoje por escopo fixo (independente de goalScope)
    int sessionWordsManuscript() const;
    int sessionWordsAll() const;

    // Personalização do contador compacto
    void setCompactSlot1(const QString& metric, const QString& scope);
    void setCompactSlot2(const QString& metric, const QString& scope);
    void setCompactShowGoalBar(bool show);

    static int countWordsInHtml(const QString& html);
    static int countWordsInPlain(const QString& text);
    static int countCharsInHtml(const QString& html);

signals:
    void countsChanged();
    void settingsChanged();
    void progressChanged();

private slots:
    void onCacheContentChanged(const QString& key);
    void onChaptersChanged();
    void onDrawersChanged();
    void scheduleEmit();
    void emitChange();
    void onTimeTick();
    void onEditorContentFlushed(const QString& key);

public slots:
    void registerCursorActivity();
    void onProjectLoaded();

private:
    QString chapterHtml(const QString& chapterId) const;
    QString itemHtml(const QString& itemId) const;
    void loadSettingsFromModel();
    void writeSettingsToModel();
    void ensureCurrentDayKey();
    void updateGoalProgress(int deltaGoalWords, qint64 deltaTimeMs, int deltaSessionManuscript = 0, int deltaSessionAll = 0);
    bool shouldCountTimeNow() const;
    QString viewModeScope() const; // scope name pra rastrear baseline
    QString keyForCurrentEdit() const; // chapter ou item

    ProjectModel* m_model;
    DocCache* m_cache;
    EditorHost* m_host;
    QString m_root;
    WordCounterSettings m_settings;
    mutable QHash<QString, int> m_chapterCounts;       // chapterId -> words
    mutable QHash<QString, int> m_itemCounts;          // itemId -> words
    mutable QHash<QString, int> m_chapterCharCounts;   // chapterId -> chars
    mutable QHash<QString, int> m_itemCharCounts;      // itemId -> chars
    QHash<QString, int> m_goalWordSnapshot;            // key -> last known word count, pra diff
    QTimer* m_emitDebounce;
    QTimer* m_timeTickTimer;
    qint64 m_lastTimeTickAt = 0;
    qint64 m_lastCursorActivityAt = 0;
};
