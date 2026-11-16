#include "EditorHost.h"

#include "CrashLogger.h"
#include "DocCache.h"
#include "ProjectModel.h"
#include "ProjectStorage.h"
#include "SceneUtils.h"

#include <QCoreApplication>
#include <algorithm>
#include <QCryptographicHash>
#include <QPointer>
#include <QScrollBar>
#include <QSettings>
#include <QClipboard>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextFragment>
#include <QTimer>

namespace {
constexpr int kDefaultFlushMs = 200;
constexpr int kHistoryMax = 60;     // passos de Voltar guardados
constexpr int kSpotsMax = 400;      // documentos com posição lembrada, por projeto
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
        // rolar com a mão (roda, barra, teclas) cancela a rolagem guardada que ainda ia ser aplicada
        connect(m_editor->verticalScrollBar(), &QAbstractSlider::actionTriggered, this, [this]() { m_spotPending = false; });
        // Manuscrito completo: a costura e o capítulo do cursor (EditorHostBook.cpp)
        connect(m_editor->document(), &QTextDocument::contentsChange, this, &EditorHost::onBookContentsChange);
        connect(m_editor, &QTextEdit::cursorPositionChanged, this, &EditorHost::updateFocusChapter);
    }
    if (m_model) {
        connect(m_model, &ProjectModel::chaptersChanged, this, &EditorHost::onModelChaptersChanged);
        connect(m_model, &ProjectModel::manuscriptsChanged, this, &EditorHost::onModelChaptersChanged);
    }
    // fechar o app: o documento aberto guarda onde parou (abre ali na próxima vez)
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() { rememberSpot(); });
}

bool EditorHost::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_editor && event->type() == QEvent::MouseButtonPress) {
        CrashLogger::log("EditorHost: clique no editor");
    }
    if (watched == m_editor && isBook() && event->type() == QEvent::InputMethod) {
        if (bookGuardInputMethod(static_cast<QInputMethodEvent*>(event))) return true;
    }
    if (watched == m_editor && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (isBook() && bookGuardKey(key)) return true;
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (tryConvertHrShortcut()) return true;
        }
        const bool isPaste = (key->key() == Qt::Key_V && (key->modifiers() & Qt::ControlModifier))
                          || (key->key() == Qt::Key_Insert && (key->modifiers() & Qt::ShiftModifier));
        if (isPaste && tryPasteClean()) return true;
    }
    return QObject::eventFilter(watched, event);
}

bool EditorHost::tryPasteClean() {
    if (!m_editor) return false;
    // Sanitização de <hr> só necessária nos modos de capítulo/cena: nesses modos,
    // <hr> é delimitador de cena e um <hr> colado do Word criaria cenas fantasmas.
    if (m_viewMode.type != ChapterDoc && m_viewMode.type != SceneDoc && m_viewMode.type != BookDoc) return false;

    const QMimeData* mimeData = QGuiApplication::clipboard()->mimeData();
    if (!mimeData || !mimeData->hasHtml()) return false;

    const QString html = mimeData->html();
    // Regex amplo: captura <hr>, <hr/>, <hr />, <HR style="...">, etc.
    static const QRegularExpression hrRe(QStringLiteral("<hr\\b[^>]*/?>"),
                                          QRegularExpression::CaseInsensitiveOption);
    if (!hrRe.match(html).hasMatch()) return false; // sem <hr> — deixa o Qt colar normalmente

    QString cleaned = html;
    cleaned.remove(hrRe);
    QTextCursor cursor = m_editor->textCursor();
    const int from = cursor.selectionStart();
    cursor.insertHtml(cleaned);
    m_editor->setTextCursor(cursor);
    // mesmo aviso do colar normal (SpellEditor::pasted): a formatação de parágrafo do projeto entra
    QMetaObject::invokeMethod(m_editor, "pasted", Q_ARG(int, from), Q_ARG(int, cursor.position()));
    return true;
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
    // Precisa ir num insertHtml() só, com o <p></p> já embutido no fragmento —
    // separar num insertHtml("<hr/>") + insertBlock() faz o importador de HTML
    // do Qt duplicar o <hr> (confirmado isolado: gera 2 <hr> em vez de 1 quando
    // o fragmento é só a tag sozinha, inserida no meio de um documento existente).
    edit.insertHtml(QStringLiteral("<hr/><p></p>"));
    edit.endEditBlock();
    m_editor->setTextCursor(edit);

    // Sincroniza imediatamente (sem esperar o debounce de 200ms) e avisa MainWindow
    // para detectar cenas sem o delay adicional de 1,5s. Isso evita que o usuário
    // tente criar a mesma cena múltiplas vezes por falta de feedback.
    syncEditorToCache();
    emit hrInserted(isBook() ? m_focusChapterId : m_viewMode.chapterId);
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
    case BookDoc:
        return QStringLiteral("book:%1").arg(vm.manuscriptId);
    }
    return QString();
}

void EditorHost::setFlushDebounceMs(int ms) {
    m_flushTimer->setInterval(ms);
}

void EditorHost::setProjectRoot(const QString& root) {
    if (root == m_projectRoot) return;
    rememberSpot();
    persistSpots();
    m_projectRoot = root;
    m_back.clear();
    m_forward.clear();
    loadSpots();
    emit historyChanged();
}

// ── Voltar/Avançar e a posição de cada documento ──
QString EditorHost::spotKey(const ViewMode& vm) {
    const QString k = keyFor(vm);
    if (k.isEmpty()) return k;
    // a cena mostra só o pedaço dela: a posição é da cena, não do capítulo
    return vm.type == SceneDoc ? QStringLiteral("%1#s%2").arg(k).arg(vm.sceneIndex) : k;
}

void EditorHost::saveSpot() {
    if (!m_editor || m_viewMode.type == Disabled || m_loadingContent) return;
    const QString key = spotKey(m_viewMode);
    if (key.isEmpty()) return;
    const QTextCursor c = m_editor->textCursor();
    m_spots.insert(key, Spot{ c.position(), c.anchor(), m_editor->verticalScrollBar()->value() });
}

void EditorHost::rememberSpot() {
    saveSpot();
    persistSpots();
}

void EditorHost::restoreSpot() {
    m_spotPending = false;
    if (!m_editor) return;
    const QString key = spotKey(m_viewMode);
    const auto it = m_spots.constFind(key);
    if (it == m_spots.constEnd()) return;
    const Spot sp = *it;
    QTextDocument* doc = m_editor->document();
    const int maxPos = qMax(0, doc->characterCount() - 1);
    QTextCursor c(doc);
    c.setPosition(qBound(0, sp.anchor, maxPos));
    c.setPosition(qBound(0, sp.cursor, maxPos), QTextCursor::KeepAnchor);
    m_editor->setTextCursor(c);
    m_editor->verticalScrollBar()->setValue(sp.scroll);
    // O layout do documento termina depois (no modo de página, as folhas): a rolagem é refeita quando ele
    // chegar. Quem abriu o documento pra mostrar outro lugar (busca, fala do Granna) move o cursor, e aí
    // a rolagem guardada não vale mais; a mão do usuário na barra também cancela.
    m_spotPending = true;
    const int at = c.position();
    QPointer<QTextEdit> ed(m_editor);
    auto apply = [this, ed, key, at, sp]() {
        if (!ed || !m_spotPending || spotKey(m_viewMode) != key || ed->textCursor().position() != at) return;
        ed->verticalScrollBar()->setValue(sp.scroll);
    };
    QTimer::singleShot(0, this, apply);
    QTimer::singleShot(90, this, [this, apply]() { apply(); m_spotPending = false; });
}

void EditorHost::loadSpots() {
    m_spots.clear();
    m_open.clear();
    if (m_projectRoot.isEmpty()) return;
    const QString id = QString::fromLatin1(QCryptographicHash::hash(m_projectRoot.toUtf8(), QCryptographicHash::Md5).toHex().left(16));
    for (const QString& s : QSettings().value(QStringLiteral("editorTabs/") + id).toStringList()) {
        const QStringList p = s.split(QLatin1Char('|'));
        if (p.size() != 5) continue;
        ViewMode vm;
        vm.type = static_cast<ViewModeType>(p[0].toInt());
        vm.manuscriptId = p[1]; vm.chapterId = p[2]; vm.sceneIndex = p[3].toInt(); vm.itemId = p[4];
        if (vm.type != Disabled) m_open.append(vm);
    }
    const QVariantMap map = QSettings().value(QStringLiteral("editorSpots/") + id).toMap();
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        const QStringList p = it.value().toString().split(QLatin1Char(','));
        if (p.size() == 3) m_spots.insert(it.key(), Spot{ p[0].toInt(), p[1].toInt(), p[2].toInt() });
    }
}

void EditorHost::persistSpots() const {
    if (m_projectRoot.isEmpty()) return;
    const QString id = QString::fromLatin1(QCryptographicHash::hash(m_projectRoot.toUtf8(), QCryptographicHash::Md5).toHex().left(16));
    QVariantMap map;
    for (auto it = m_spots.constBegin(); it != m_spots.constEnd() && map.size() < kSpotsMax; ++it)
        map.insert(it.key(), QStringLiteral("%1,%2,%3").arg(it->cursor).arg(it->anchor).arg(it->scroll));
    QSettings().setValue(QStringLiteral("editorSpots/") + id, map);
    QStringList tabs;
    for (const ViewMode& vm : m_open)
        tabs << QStringLiteral("%1|%2|%3|%4|%5").arg(int(vm.type)).arg(vm.manuscriptId, vm.chapterId).arg(vm.sceneIndex).arg(vm.itemId);
    QSettings().setValue(QStringLiteral("editorTabs/") + id, tabs);
}

QVector<EditorHost::ViewMode> EditorHost::openDocs() const {
    QVector<ViewMode> out;
    for (const ViewMode& vm : m_open) if (isOpenable(vm)) out.append(vm);
    return out;
}

void EditorHost::openTarget(const ViewMode& vm) {
    if (m_model && !vm.manuscriptId.isEmpty() && m_model->activeManuscriptId() != vm.manuscriptId && m_model->findManuscript(vm.manuscriptId))
        m_model->setActiveManuscriptId(vm.manuscriptId);
    setViewMode(vm);
}

void EditorHost::openDocAt(int index) {
    const QVector<ViewMode> docs = openDocs();
    if (index >= 0 && index < docs.size()) openTarget(docs[index]);
}

void EditorHost::closeDocAt(int index) {
    const QVector<ViewMode> docs = openDocs();
    if (index < 0 || index >= docs.size()) return;
    const ViewMode gone = docs[index];
    m_open.erase(std::remove_if(m_open.begin(), m_open.end(), [&](const ViewMode& v) { return keyFor(v) == keyFor(gone); }), m_open.end());
    // fechou a que está na tela: vai pra vizinha (a da esquerda; sem ela, a da direita)
    if (keyFor(gone) == keyFor(m_viewMode)) {
        const QVector<ViewMode> rest = openDocs();
        if (!rest.isEmpty()) { openTarget(rest[qBound(0, index - 1, rest.size() - 1)]); return; }
    }
    emit historyChanged();
}

bool EditorHost::isOpenable(const ViewMode& vm) const {
    if (!m_model) return false;
    switch (vm.type) {
    case ChapterDoc: return m_model->findChapter(vm.chapterId) != nullptr;
    case SceneDoc: {
        const Chapter* ch = m_model->findChapter(vm.chapterId);
        return ch && vm.sceneIndex >= 0 && vm.sceneIndex < ch->scenes.size();
    }
    case DrawerDoc: return m_model->findDrawerItem(vm.itemId) != nullptr;
    case ManuscriptDoc: return m_model->findManuscript(vm.manuscriptId) != nullptr;
    case BookDoc: return m_model->findManuscript(vm.manuscriptId) != nullptr
                      && !m_model->orderedChaptersForManuscript(vm.manuscriptId).isEmpty();
    case Disabled: break;
    }
    return false;
}

bool EditorHost::canGoBack() const {
    for (int i = m_back.size() - 1; i >= 0; --i) if (isOpenable(m_back[i]) && m_back[i] != m_viewMode) return true;
    return false;
}

bool EditorHost::canGoForward() const {
    for (int i = m_forward.size() - 1; i >= 0; --i) if (isOpenable(m_forward[i]) && m_forward[i] != m_viewMode) return true;
    return false;
}

bool EditorHost::travel(QVector<ViewMode>& from, QVector<ViewMode>& to) {
    // pula o que foi apagado (ou é o próprio documento aberto) no caminho
    while (!from.isEmpty() && (!isOpenable(from.last()) || from.last() == m_viewMode)) from.removeLast();
    if (from.isEmpty()) { emit historyChanged(); return false; }
    const ViewMode target = from.takeLast();
    if (m_viewMode.type != Disabled) to.append(m_viewMode);
    // o documento de outro manuscrito: o manuscrito ativo acompanha, como no abrir pela gaveta
    if (m_model && !target.manuscriptId.isEmpty() && m_model->activeManuscriptId() != target.manuscriptId
        && m_model->findManuscript(target.manuscriptId))
        m_model->setActiveManuscriptId(target.manuscriptId);
    m_travelling = true;
    setViewMode(target);
    m_travelling = false;
    emit historyChanged();
    return true;
}

bool EditorHost::goBack() { return travel(m_back, m_forward); }
bool EditorHost::goForward() { return travel(m_forward, m_back); }

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

    // Mesmo sync que create/switchVariationForCurrentScene sempre fazem antes de
    // ler o cache — sem isso, o chapterHtml abaixo pode estar desatualizado em
    // relação ao que o usuário acabou de digitar.
    syncEditorToCache();

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
            const QString path = ProjectStorage::variationPath(
                m_projectRoot, msId, sceneId, updated->activeVariationId);
            bool readOk = false;
            const QString fileText = ProjectStorage::readText(path, &readOk);
            if (!readOk) {
                // Falha de leitura de verdade (não é "variação vazia de propósito")
                // — não sobrescreve a cena com conteúdo em branco, só avisa.
                CrashLogger::log(QStringLiteral(
                    "deleteVariation: falha ao ler %1, cena mantida como estava").arg(path));
            } else {
                const QString newSegment = fileText.isEmpty() ? QStringLiteral("<p></p>") : fileText;
                const QString key = activeKey();
                const QString chapterHtml = m_cache->get(key);
                const QString newChapterHtml = SceneUtils::replaceSceneHtml(chapterHtml, sceneIdx, newSegment);
                m_cache->set(key, newChapterHtml, /*markDirty=*/true);
                loadIntoEditor(newSegment);
            }
        }
    }
    return true;
}

void EditorHost::setViewMode(const ViewMode& vm) {
    if (m_viewMode == vm) return;
    if (vm.type == BookDoc && !isOpenable(vm)) return;   // livro que não existe mais (ou sem capítulos)

    {
        char buf[160];
        snprintf(buf, sizeof(buf), "setViewMode type=%d key=%s",
                 static_cast<int>(vm.type), keyFor(vm).toUtf8().constData());
        CrashLogger::log(buf);
    }

    // Antes de trocar, garante que o conteúdo atual do editor está no cache.
    syncEditorToCache();

    // onde o cursor estava no documento que sai, e o documento que sai vira o passo de Voltar
    saveSpot();
    if (!m_travelling && m_viewMode.type != Disabled && vm.type != Disabled) {
        if (m_back.isEmpty() || m_back.last() != m_viewMode) m_back.append(m_viewMode);
        while (m_back.size() > kHistoryMax) m_back.removeFirst();
        m_forward.clear();
    }

    m_viewMode = vm;
    // o livro que sai (ou entra) zera a costura antes de alguém perguntar por ela
    m_seams.clear();
    m_bookDirty.clear();
    m_bookChapterIds.clear();
    m_focusChapterId.clear();
    m_breakTableRev = -1;
    if (m_viewMode.type == BookDoc && m_model) {
        for (const Chapter* ch : m_model->orderedChaptersForManuscript(m_viewMode.manuscriptId))
            m_bookChapterIds << ch->id;
        if (!m_bookChapterIds.isEmpty()) m_focusChapterId = m_bookChapterIds.first();
    }
    emit viewModeChanged();

    if (m_viewMode.type == Disabled) {
        replaceContent(QString());
        if (m_editor) m_editor->setEnabled(false);
        return;
    }

    const QString key = activeKey();
    QString chapterOrItemHtml;
    if (m_viewMode.type == BookDoc) {
        // o livro não mora no cache: é montado dos capítulos (que moram)
        chapterOrItemHtml = buildBookHtml();
    } else if (m_cache && !key.isEmpty()) {
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
    {
        const QString k = keyFor(m_viewMode);
        auto it = std::find_if(m_open.begin(), m_open.end(), [&](const ViewMode& v) { return keyFor(v) == k; });
        if (it != m_open.end()) *it = m_viewMode;   // a cena ou o capítulo inteiro: a mesma aba
        else m_open.append(m_viewMode);
    }
    restoreSpot();          // volta onde o cursor estava da última vez nesse documento
    emit historyChanged();
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

void EditorHost::reloadFromCache() {
    if (!m_editor || !m_cache || m_viewMode.type == Disabled) return;
    if (isBook()) { m_flushTimer->stop(); rebuildBook(true); return; }
    const QString key = activeKey();
    if (key.isEmpty() || !m_cache->has(key)) return;
    m_flushTimer->stop();
    QString html = m_cache->get(key);
    if (m_viewMode.type == SceneDoc && m_viewMode.sceneIndex >= 0)
        html = SceneUtils::getSceneHtml(html, m_viewMode.sceneIndex);
    loadIntoEditor(html);
}

void EditorHost::syncEditorToCache() {
    if (!m_editor || !m_cache) return;
    if (m_viewMode.type == Disabled) return;
    // Ficha de personagem: o conteúdo vive em DrawerItem.sheet (painel próprio),
    // não no editor de texto. Não sincroniza pra não sujar o cache/html do item.
    if (m_viewMode.type == DrawerDoc && m_model) {
        const DrawerItem* it = m_model->findDrawerItem(m_viewMode.itemId);
        if (it && it->isSheet) return;
    }
    if (m_flushTimer->isActive()) m_flushTimer->stop();
    if (isBook()) { flushBook(); return; }

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
    auto* doc = m_editor->document();
    const bool undoWas = doc->isUndoRedoEnabled();
    doc->setUndoRedoEnabled(false);
    m_editor->setUpdatesEnabled(false);
    if (html.isEmpty()) {
        m_editor->clear();
    } else {
        m_editor->setHtml(html);
        normalizeRefAnchors();
        if (m_viewMode.type == BookDoc) stampBookSeams();
    }
    // O setHtml chama ensureCursorVisible no meio da carga; no modo de página
    // (margem do documento de 76 px) isso deixava o capítulo aberto já rolado
    // e a primeira folha sem a margem de cima. Documento novo abre no topo.
    m_editor->verticalScrollBar()->setValue(0);
    doc->setUndoRedoEnabled(undoWas);
    m_editor->setUpdatesEnabled(true);
    emit contentLoaded();
    m_loadingContent = false;
}

void EditorHost::normalizeRefAnchors() {
    // O toHtml() serializa as menções como <a href="ref:...">. Ao recarregar, o
    // motor de rich text do Qt aplica o estilo padrão de link (sublinhado) — daí o
    // underline "fantasma" que volta sempre que o doc é reaberto. As menções devem
    // ser invisíveis em repouso (só o Ctrl as realça), então tiramos o sublinhado
    // herdado de TODO anchor "ref:", sem mexer no href.
    if (!m_editor) return;
    QTextDocument* doc = m_editor->document();
    const bool wasModified = doc->isModified();
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;
            const QTextCharFormat fmt = frag.charFormat();
            if (!fmt.isAnchor() || !fmt.anchorHref().startsWith(QStringLiteral("ref:")))
                continue;
            if (!fmt.fontUnderline() && fmt.underlineStyle() == QTextCharFormat::NoUnderline)
                continue;
            QTextCursor c(doc);
            c.setPosition(frag.position());
            c.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
            QTextCharFormat clr;
            clr.setFontUnderline(false);
            clr.setUnderlineStyle(QTextCharFormat::NoUnderline);
            c.mergeCharFormat(clr);
        }
    }
    doc->setModified(wasModified);
}
