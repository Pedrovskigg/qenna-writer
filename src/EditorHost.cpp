#include "EditorHost.h"

#include "CrashLogger.h"
#include "DocCache.h"
#include "ProjectModel.h"
#include "ProjectStorage.h"
#include "SceneUtils.h"

#include <QEvent>
#include <QFile>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>

namespace {
constexpr int kDefaultFlushMs = 200;
}

bool EditorHost::ViewMode::operator==(const ViewMode& other) const {
    return type == other.type
        && manuscriptId == other.manuscriptId
        && chapterId == other.chapterId
        && sceneIndex == other.sceneIndex
        && itemId == other.itemId;
}

EditorHost::EditorHost(QTextEdit* editor, DocCache* cache, ProjectModel* model, QObject* parent)
    : QObject(parent)
    , m_editor(editor)
    , m_cache(cache)
    , m_model(model)
    , m_flushTimer(new QTimer(this))
{
    m_flushTimer->setSingleShot(true);
    m_flushTimer->setInterval(kDefaultFlushMs);
    connect(m_flushTimer, &QTimer::timeout, this, &EditorHost::flushPendingEditorContent);

    if (m_editor) {
        connect(m_editor, &QTextEdit::textChanged, this, &EditorHost::onEditorTextChanged);
        m_editor->installEventFilter(this);
    }
}

bool EditorHost::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_editor && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (tryConvertHrShortcut()) return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

bool EditorHost::tryConvertHrShortcut() {
    if (!m_editor) return false;
    QTextCursor cursor = m_editor->textCursor();
    if (cursor.hasSelection()) return false;
    const QTextBlock block = cursor.block();
    const QString trimmed = block.text().trimmed();
    static const QRegularExpression hrShortcut(QStringLiteral("^-{4,}$"));
    if (!hrShortcut.match(trimmed).hasMatch()) return false;

    QTextCursor edit(block);
    edit.beginEditBlock();
    edit.movePosition(QTextCursor::StartOfBlock);
    edit.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    edit.removeSelectedText();
    edit.insertHtml(QStringLiteral("<hr/>"));
    edit.insertBlock();
    edit.endEditBlock();
    m_editor->setTextCursor(edit);

    // Sincroniza imediatamente (sem esperar o debounce de 200ms) e avisa MainWindow
    // para detectar cenas sem o delay adicional de 1,5s. Isso evita que o usuário
    // tente criar a mesma cena múltiplas vezes por falta de feedback.
    syncEditorToCache();
    emit hrInserted(m_viewMode.chapterId);
    return true;
}

QString EditorHost::activeKey() const {
    return keyFor(m_viewMode);
}

QString EditorHost::keyFor(const ViewMode& vm) {
    switch (vm.type) {
    case Disabled:
        return QString();
    case ChapterDoc:
    case SceneDoc:
        // Cenas vivem dentro do HTML do capítulo — mesma chave de cache.
        // Split/replace por sceneIndex entra na camada de cenas (10).
        return DocCache::chapterKey(vm.manuscriptId, vm.chapterId);
    case DrawerDoc:
        return DocCache::itemKey(vm.itemId);
    case ManuscriptDoc:
        return QStringLiteral("ms:%1").arg(vm.manuscriptId);
    }
    return QString();
}

void EditorHost::setFlushDebounceMs(int ms) {
    m_flushTimer->setInterval(ms);
}

void EditorHost::setProjectRoot(const QString& root) {
    m_projectRoot = root;
}

namespace {
QString readVariationFile(const QString& root, const QString& msId,
                          const QString& sceneId, const QString& varId) {
    if (root.isEmpty()) return QString();
    const QString path = ProjectStorage::variationPath(root, msId, sceneId, varId);
    bool ok = false;
    const QString text = ProjectStorage::readText(path, &ok);
    return ok ? text : QString();
}

void writeVariationFile(const QString& root, const QString& msId,
                        const QString& sceneId, const QString& varId,
                        const QString& html) {
    if (root.isEmpty() || varId.isEmpty()) return;
    const QString rel = QStringLiteral("content/manuscripts/ms_%1/chapters/vars/scene_%2_%3.html")
        .arg(msId, sceneId, varId);
    const QString absPath = ProjectStorage::variationPath(root, msId, sceneId, varId);
    QString err;
    ProjectStorage::writeTextGuarded(absPath, html, root, rel, &err);
}
}

QString EditorHost::createVariationForCurrentScene(const QString& label) {
    if (m_viewMode.type != SceneDoc || !m_model || !m_cache) return QString();
    const QString chapterId = m_viewMode.chapterId;
    const int sceneIdx = m_viewMode.sceneIndex;
    const Scene* scene = m_model->findScene(chapterId, sceneIdx);
    if (!scene) return QString();
    const QString sceneId = scene->id;
    const QString msId = m_viewMode.manuscriptId;

    syncEditorToCache();
    const QString key = activeKey();
    const QString chapterHtml = m_cache->get(key);
    const QString currentSegment = SceneUtils::getSceneHtml(chapterHtml, sceneIdx);

    // Persiste o segmento atual no arquivo da var ativa (se houver).
    if (!scene->activeVariationId.isEmpty()) {
        writeVariationFile(m_projectRoot, msId, sceneId, scene->activeVariationId, currentSegment);
    }

    if (scene->variations.isEmpty()) {
        // Primeira chamada: cria "Original" com o conteúdo atual e marca como primária,
        // depois cria a variação solicitada (clone) e ativa.
        const QString origId = m_model->createSceneVariation(chapterId, sceneIdx, tr("Original"));
        writeVariationFile(m_projectRoot, msId, sceneId, origId, currentSegment);
        m_model->setPrimaryVariation(chapterId, sceneIdx, origId);
    }

    const QString newId = m_model->createSceneVariation(chapterId, sceneIdx, label);
    if (newId.isEmpty()) return QString();
    m_model->switchActiveVariation(chapterId, sceneIdx, newId);
    writeVariationFile(m_projectRoot, msId, sceneId, newId, currentSegment);
    // O chapter html não muda — clone do segmento atual.
    return newId;
}

bool EditorHost::switchVariationForCurrentScene(const QString& variationId) {
    if (m_viewMode.type != SceneDoc || !m_model || !m_cache) return false;
    const QString chapterId = m_viewMode.chapterId;
    const int sceneIdx = m_viewMode.sceneIndex;
    const Scene* scene = m_model->findScene(chapterId, sceneIdx);
    if (!scene) return false;
    if (scene->activeVariationId == variationId) return true;
    const QString sceneId = scene->id;
    const QString msId = m_viewMode.manuscriptId;
    const QString oldVarId = scene->activeVariationId;

    syncEditorToCache();
    const QString key = activeKey();
    const QString chapterHtml = m_cache->get(key);
    const QString currentSegment = SceneUtils::getSceneHtml(chapterHtml, sceneIdx);

    if (!oldVarId.isEmpty()) {
        writeVariationFile(m_projectRoot, msId, sceneId, oldVarId, currentSegment);
    }

    QString newSegment = readVariationFile(m_projectRoot, msId, sceneId, variationId);
    if (newSegment.isEmpty()) newSegment = QStringLiteral("<p></p>");

    if (!m_model->switchActiveVariation(chapterId, sceneIdx, variationId)) return false;

    const QString newChapterHtml = SceneUtils::replaceSceneHtml(chapterHtml, sceneIdx, newSegment);
    m_cache->set(key, newChapterHtml, /*markDirty=*/true);
    loadIntoEditor(newSegment);
    return true;
}

bool EditorHost::deleteVariationForCurrentScene(const QString& variationId) {
    if (m_viewMode.type != SceneDoc || !m_model || !m_cache) return false;
    const QString chapterId = m_viewMode.chapterId;
    const int sceneIdx = m_viewMode.sceneIndex;
    const Scene* scene = m_model->findScene(chapterId, sceneIdx);
    if (!scene) return false;
    if (scene->variations.size() <= 1) return false;
    const QString sceneId = scene->id;
    const QString msId = m_viewMode.manuscriptId;
    const bool wasActive = (scene->activeVariationId == variationId);

    if (!m_model->removeVariation(chapterId, sceneIdx, variationId)) return false;

    // Apaga arquivo da var removida.
    if (!m_projectRoot.isEmpty()) {
        const QString path = ProjectStorage::variationPath(m_projectRoot, msId, sceneId, variationId);
        QFile::remove(path);
    }

    // Se a removida era a ativa, recarrega o segmento da nova ativa.
    if (wasActive) {
        const Scene* updated = m_model->findScene(chapterId, sceneIdx);
        if (updated) {
            QString newSegment = readVariationFile(m_projectRoot, msId, sceneId, updated->activeVariationId);
            if (newSegment.isEmpty()) newSegment = QStringLiteral("<p></p>");
            const QString key = activeKey();
            const QString chapterHtml = m_cache->get(key);
            const QString newChapterHtml = SceneUtils::replaceSceneHtml(chapterHtml, sceneIdx, newSegment);
            m_cache->set(key, newChapterHtml, /*markDirty=*/true);
            loadIntoEditor(newSegment);
        }
    }
    return true;
}

void EditorHost::setViewMode(const ViewMode& vm) {
    if (m_viewMode == vm) return;

    {
        char buf[160];
        snprintf(buf, sizeof(buf), "setViewMode type=%d key=%s",
                 static_cast<int>(vm.type), keyFor(vm).toUtf8().constData());
        CrashLogger::log(buf);
    }

    // Antes de trocar, garante que o conteúdo atual do editor está no cache.
    syncEditorToCache();

    m_viewMode = vm;
    emit viewModeChanged();

    if (m_viewMode.type == Disabled) {
        replaceContent(QString());
        if (m_editor) m_editor->setEnabled(false);
        return;
    }

    const QString key = activeKey();
    QString chapterOrItemHtml;
    if (m_cache && !key.isEmpty()) {
        if (m_cache->has(key)) {
            chapterOrItemHtml = m_cache->get(key);
        } else {
            chapterOrItemHtml = hydrateFromModel(m_viewMode);
            if (!chapterOrItemHtml.isEmpty()) {
                m_cache->set(key, chapterOrItemHtml, /*markDirty=*/false);
            }
        }
    }

    QString htmlToEdit = chapterOrItemHtml;
    if (m_viewMode.type == SceneDoc && m_viewMode.sceneIndex >= 0) {
        htmlToEdit = SceneUtils::getSceneHtml(chapterOrItemHtml, m_viewMode.sceneIndex);
    }
    loadIntoEditor(htmlToEdit);

    if (m_editor) m_editor->setEnabled(true);
}

QString EditorHost::hydrateFromModel(const ViewMode& vm) const {
    if (!m_model) return QString();
    switch (vm.type) {
    case DrawerDoc: {
        for (const auto& d : m_model->drawers()) {
            for (const auto& it : d.items) {
                if (it.id == vm.itemId) {
                    if (it.hasInlineHtml) return it.html;
                    // file-backed: leitura via ProjectStorage entra em camada futura
                    return QString();
                }
            }
        }
        break;
    }
    case ChapterDoc:
    case SceneDoc: {
        if (m_projectRoot.isEmpty()) return QString();
        const Chapter* ch = m_model->findChapter(vm.chapterId);
        if (!ch || ch->file.isEmpty()) return QString();
        bool ok = false;
        const QString html = ProjectStorage::readChapter(m_projectRoot, ch->file, &ok);
        return ok ? html : QString();
    }
    case ManuscriptDoc:
        // ManuscriptDoc legado/raro — pega html inline do model.
        for (const auto& m : m_model->manuscripts()) {
            if (m.id == vm.manuscriptId) return m.html;
        }
        break;
    case Disabled:
        break;
    }
    return QString();
}

void EditorHost::syncEditorToCache() {
    if (!m_editor || !m_cache) return;
    if (m_viewMode.type == Disabled) return;
    if (m_flushTimer->isActive()) m_flushTimer->stop();

    const QString key = activeKey();
    if (key.isEmpty()) return;

    const QString editorHtml = m_editor->toHtml();
    QString toStore = editorHtml;
    if (m_viewMode.type == SceneDoc && m_viewMode.sceneIndex >= 0) {
        const QString currentChapterHtml = m_cache->has(key)
            ? m_cache->get(key)
            : hydrateFromModel(m_viewMode);
        toStore = SceneUtils::replaceSceneHtml(currentChapterHtml, m_viewMode.sceneIndex, editorHtml);
    }
    m_cache->set(key, toStore, /*markDirty=*/true);
    emit contentFlushed(key);
}

void EditorHost::onEditorTextChanged() {
    if (m_loadingContent) return;
    if (m_viewMode.type == Disabled) return;
    m_flushTimer->start();
}

void EditorHost::flushPendingEditorContent() {
    syncEditorToCache();
}

void EditorHost::loadIntoEditor(const QString& html) {
    replaceContent(html);
}

void EditorHost::replaceContent(const QString& html) {
    if (!m_editor) return;
    m_loadingContent = true;
    if (html.isEmpty()) {
        m_editor->clear();
    } else {
        m_editor->setHtml(html);
    }
    emit contentLoaded();
    m_loadingContent = false;
}
