#include "ProjectSaver.h"

#include "DocCache.h"
#include "EditorHost.h"
#include "ElementsStore.h"
#include "ProjectModel.h"
#include "ProjectStorage.h"

#include <QTimer>

namespace {
constexpr int kDefaultPerDocMs = 800;
constexpr int kDefaultAutosaveMs = 10 * 60 * 1000; // 10 minutos
}

ProjectSaver::ProjectSaver(ProjectModel* model, DocCache* cache, EditorHost* host, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_cache(cache)
    , m_host(host)
    , m_perDocTimer(new QTimer(this))
    , m_autosaveTimer(new QTimer(this))
{
    m_perDocTimer->setSingleShot(true);
    m_perDocTimer->setInterval(kDefaultPerDocMs);
    connect(m_perDocTimer, &QTimer::timeout, this, [this]() {
        flushDirtyDocsNow();
    });

    m_autosaveTimer->setInterval(kDefaultAutosaveMs);
    connect(m_autosaveTimer, &QTimer::timeout, this, &ProjectSaver::onAutosaveTimeout);
    m_autosaveTimer->start();

    if (m_host) {
        connect(m_host, &EditorHost::contentFlushed, this, &ProjectSaver::onContentFlushed);
    }
    if (m_model) {
        connect(m_model, &ProjectModel::settingsChanged, this, &ProjectSaver::onSettingsChanged);
    }
}

void ProjectSaver::setProjectRoot(const QString& root) {
    m_root = root;
}

void ProjectSaver::setAutosaveIntervalMs(int ms) {
    if (ms <= 0) {
        m_autosaveTimer->stop();
        return;
    }
    m_autosaveTimer->setInterval(ms);
    if (!m_autosaveTimer->isActive()) m_autosaveTimer->start();
}

void ProjectSaver::setPerDocDebounceMs(int ms) {
    m_perDocTimer->setInterval(qMax(0, ms));
}

bool ProjectSaver::hasDirtyContent() const {
    if (m_settingsDirty) return true;
    return m_cache && !m_cache->dirtyKeys().isEmpty();
}

void ProjectSaver::onContentFlushed(const QString& /*key*/) {
    if (!m_perDocTimer->isActive()) m_perDocTimer->start();
    else m_perDocTimer->start(); // restart
}

void ProjectSaver::onAutosaveTimeout() {
    if (!hasDirtyContent()) return;
    saveProject();
}

void ProjectSaver::onSettingsChanged() {
    m_settingsDirty = true;
    if (!m_perDocTimer->isActive()) m_perDocTimer->start();
    else m_perDocTimer->start();
}

void ProjectSaver::setSaving(bool saving) {
    if (m_saving == saving) return;
    m_saving = saving;
    emit savingChanged(saving);
}

bool ProjectSaver::writeDirtyDocs(QString* errorOut) {
    if (!m_cache || !m_model) return true;
    const QStringList keys = m_cache->dirtyKeys();
    if (keys.isEmpty()) return true;
    if (m_root.isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("Raiz do projeto indefinida.");
        return false;
    }

    bool allOk = true;
    for (const QString& key : keys) {
        const QString html = m_cache->get(key);

        if (key.startsWith(QStringLiteral("ch:"))) {
            // ch:<msId>:<chId>  — msId pode ser vazio (capítulo solto)
            const int firstColon = key.indexOf(QLatin1Char(':'));
            const int secondColon = key.indexOf(QLatin1Char(':'), firstColon + 1);
            if (firstColon < 0 || secondColon < 0) continue;
            const QString msId = key.mid(firstColon + 1, secondColon - firstColon - 1);
            const QString chId = key.mid(secondColon + 1);
            const Chapter* ch = m_model->findChapter(chId);
            if (!ch) continue;
            QString relFile = ch->file;
            if (relFile.isEmpty()) {
                relFile = ProjectModel::chapterDefaultFile(msId, chId);
            }
            QString err;
            const auto outcome = ProjectStorage::writeChapter(m_root, relFile, html, &err);
            if (outcome == ProjectStorage::WriteOutcome::Written) {
                m_cache->markClean(key);
            } else if (outcome == ProjectStorage::WriteOutcome::BlankOverwriteBlocked) {
                // Bloqueio de blank é proteção, não erro fatal — deixa dirty pra próxima tentativa.
                qWarning("Write bloqueado (blank) para %s", qUtf8Printable(relFile));
            } else {
                allOk = false;
                if (errorOut && errorOut->isEmpty()) *errorOut = err;
            }
        } else if (key.startsWith(QStringLiteral("it:"))) {
            const QString itemId = key.mid(3);
            const DrawerItem* item = m_model->findDrawerItem(itemId);
            if (!item) continue;
            if (!item->file.isEmpty()) {
                // file-backed
                const QString absPath = ProjectStorage::joinPath(m_root, item->file);
                QString err;
                const auto outcome = ProjectStorage::writeTextGuarded(absPath, html, m_root, item->file, &err);
                if (outcome == ProjectStorage::WriteOutcome::Written) {
                    m_cache->markClean(key);
                } else if (outcome == ProjectStorage::WriteOutcome::BlankOverwriteBlocked) {
                    qWarning("Write bloqueado (blank) para %s", qUtf8Printable(item->file));
                } else {
                    allOk = false;
                    if (errorOut && errorOut->isEmpty()) *errorOut = err;
                }
            } else {
                // inline: atualiza o model. Será gravado quando o index for escrito.
                m_model->updateDrawerItemHtml(itemId, html);
                m_cache->markClean(key);
            }
        }
        // ms:<id> não persiste fora do index — ignora aqui.
    }
    return allOk;
}

bool ProjectSaver::writeIndexNow(QString* errorOut) {
    if (!m_model) return true;
    if (m_root.isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("Raiz do projeto indefinida.");
        return false;
    }
    return ProjectStorage::writeIndex(m_root, m_model->toJson(), errorOut);
}

bool ProjectSaver::flushDirtyDocsNow() {
    if (m_root.isEmpty()) return true;
    if (m_saving) {
        // Já tem save rolando — vai pegar tudo. Não enfileira segundo per-doc.
        return true;
    }
    if (!hasDirtyContent()) return true;

    setSaving(true);
    if (m_host) m_host->syncEditorToCache();

    QString err;
    bool ok = writeDirtyDocs(&err);
    if (ok && m_settingsDirty) {
        ok = writeIndexNow(&err);
        if (ok) m_settingsDirty = false;
    }
    if (!ok) {
        m_lastError = err;
        setSaving(false);
        emit saveFailed(err);
        return false;
    }
    setSaving(false);
    return true;
}

bool ProjectSaver::saveProject() {
    if (m_root.isEmpty()) {
        // Sem projeto aberto — noop silencioso (não é erro).
        return true;
    }
    if (m_saving) {
        // Marca pra rodar uma nova paulada quando a atual acabar.
        m_pendingFullSave = true;
        return true;
    }
    setSaving(true);
    if (m_perDocTimer->isActive()) m_perDocTimer->stop();
    if (m_host) m_host->syncEditorToCache();

    QString err;
    bool ok = writeDirtyDocs(&err);
    if (ok) {
        ok = writeIndexNow(&err);
        if (ok) m_settingsDirty = false;
    }
    if (ok && m_elementsStore && m_elementsStore->isDirty()) {
        if (!m_elementsStore->save()) {
            err = QStringLiteral("Falha ao salvar elements.json");
            ok = false;
        }
    }

    setSaving(false);

    if (!ok) {
        m_lastError = err;
        emit saveFailed(err);
        if (m_pendingFullSave) {
            m_pendingFullSave = false;
        }
        return false;
    }

    m_lastError.clear();
    emit projectSaved();

    if (m_pendingFullSave) {
        m_pendingFullSave = false;
        // Roda de novo de forma assíncrona — pode ter chegado mais dirty enquanto salvávamos.
        QMetaObject::invokeMethod(this, "saveProject", Qt::QueuedConnection);
    }
    return true;
}
