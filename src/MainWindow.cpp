#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QCryptographicHash>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSizePolicy>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QToolButton>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextFrameFormat>
#include <QTextImageFormat>
#include <QTextLength>
#include <QUrl>
#include <QWidget>

#include <QDir>
#include <QInputDialog>
#include <QLineEdit>
#include <QLocale>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include "BondPopup.h"
#include "BondViewPanel.h"
#include "DocCache.h"
#include "DrawerCreateDialog.h"
#include "DrawerListPanel.h"
#include "EditorHost.h"
#include "EditorLayout.h"
#include "ElementCreateDialog.h"
#include "ElementsStore.h"
#include "ImageInsertDialog.h"
#include "ImageOverlay.h"
#include "LeftBar.h"
#include "ManuscriptPanel.h"
#include "ProjectInfoHover.h"
#include "ProjectInfoPanel.h"
#include "ProjectModel.h"
#include "ProjectSaver.h"
#include "ProjectStorage.h"
#include "RefMenuPanel.h"
#include "SceneUtils.h"
#include "AmbienceManager.h"
#include "AmbiencePanel.h"
#include "GlossaryAddPopup.h"
#include "GlossaryPanel.h"
#include "GlossaryStore.h"
#include "MarkerHoverPopup.h"
#include "MarkerPickPopup.h"
#include "MarkerStore.h"
#include "SelectionPopup.h"
#include "SettingsPanel.h"
#include "SpellChecker.h"
#include "SpellEditor.h"
#include "SpellHighlighter.h"
#include "ThemesPanel.h"
#include "WordCountPanel.h"
#include "WordCounter.h"
#include "Theme.h"
#include "TopToolbar.h"
#include "VariationBar.h"

namespace {
// Parseia uma cor no formato "rgba(r,g,b,a)" (o resto fica como hex). A versão
// do QColor antes do Qt 6.6 não cobre essa sintaxe.
QColor parseColor(const QString& s)
{
    if (!s.startsWith(QLatin1String("rgba("))) return QColor(s);
    QString inner = s.mid(5);
    if (inner.endsWith(QChar(')'))) inner.chop(1);
    const QStringList parts = inner.split(QChar(','));
    if (parts.size() != 4) return QColor(s);
    bool ok = false;
    const int r = parts.at(0).trimmed().toInt(&ok); if (!ok) return QColor();
    const int g = parts.at(1).trimmed().toInt(&ok); if (!ok) return QColor();
    const int b = parts.at(2).trimmed().toInt(&ok); if (!ok) return QColor();
    // Alpha: 0..255 inteiro (formato simplificado que usamos no MiraTheme).
    const int a = parts.at(3).trimmed().toInt(&ok); if (!ok) return QColor();
    return QColor(r, g, b, a);
}

// Configura margens externas do frame float. O lado virado pra borda da página
// fica em 0 (não há texto pra afastar); o lado do texto recebe o gap real.
void applyFloatFrameStyle(QTextFrameFormat &ff, bool isLeft, int w)
{
    ff.setPosition(isLeft ? QTextFrameFormat::FloatLeft : QTextFrameFormat::FloatRight);
    ff.setBorder(0);
    ff.setPadding(0);
    ff.setLeftMargin(isLeft ? 0 : 16);
    ff.setRightMargin(isLeft ? 16 : 0);
    ff.setTopMargin(4);
    ff.setBottomMargin(4);
    ff.setWidth(QTextLength(QTextLength::FixedLength, w));
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , editor(new SpellEditor)
    , toolbar(new TopToolbar(this))
    , imageOverlay(nullptr)
    , leftBar(nullptr)
    , drawerListPanel(nullptr)
    , manuscriptPanel(nullptr)
    , projectModel(new ProjectModel(this))
    , docCache(new DocCache(this))
    , editorHost(nullptr)
    , variationBar(nullptr)
    , projectSaver(nullptr)
    , wordCounter(nullptr)
    , wordCountPanel(nullptr)
    , refMenuPanel(nullptr)
    , elementsStore(nullptr)
    , spellChecker(nullptr)
    , spellHighlighter(nullptr)
    , settingsPanel(nullptr)
    , themesPanel(nullptr)
    , selectionPopup(nullptr)
    , editorContainer(nullptr)
    , editorColumn(nullptr)
    , toolbarHolder(nullptr)
    , selBoldBtn(nullptr)
    , selItalicBtn(nullptr)
    , selUnderlineBtn(nullptr)
    , selStrikeBtn(nullptr)
    , sceneDetectTimer(nullptr)
    , currentFontFamily(QStringLiteral("Alegreya"))
    , currentFontSize(16)
    , currentLineHeight(170)
    , firstLineIndentEnabled(true)
    , paragraphSpacingBefore(0)
    , paragraphSpacingAfter(0)
    , focusModeEnabled(false)
    , baseTextColor(QColor(Theme::editorTextColor()))
{
    baseWindowTitle = tr("Mira Writing");
    setWindowTitle(baseWindowTitle);

    // Toolbar flutuante: holder com margem em volta da TopToolbar
    auto *holder = new QWidget(this);
    holder->setObjectName(QStringLiteral("topToolbarHolder"));
    holder->setAttribute(Qt::WA_StyledBackground, true);
    holder->setStyleSheet(QStringLiteral("#topToolbarHolder { background: %1; }").arg(Theme::appBackground()));
    toolbarHolder = holder;
    auto *holderLayout = new QHBoxLayout(holder);
    holderLayout->setContentsMargins(12, 10, 12, 4);
    holderLayout->setSpacing(0);
    holderLayout->addWidget(toolbar);
    setMenuWidget(holder);

    resize(1100, 800);
    setWindowState(windowState() | Qt::WindowMaximized);

    setupEditor();
    setupToolbar();
    applyEditorStyle();
    applyPageShadow();

    QPalette p = editor->palette();
    p.setColor(QPalette::Text, baseTextColor);
    editor->setPalette(p);

    imageOverlay = new ImageOverlay(editor->viewport());
    imageOverlay->hide();
    connect(imageOverlay, &ImageOverlay::widthChangeRequested, this, &MainWindow::changeSelectedImageWidth);
    connect(imageOverlay, &ImageOverlay::alignmentRequested, this, &MainWindow::changeSelectedImageAlignment);

    editor->viewport()->installEventFilter(this);
    editor->viewport()->setMouseTracking(true);
    connect(editor->verticalScrollBar(), &QAbstractSlider::valueChanged, this, [this]() {
        hideOverlay();
        if (markerHoverPopup && markerHoverPopup->isVisible()) {
            markerHoverPopup->hide();
            markerHoverId.clear();
        }
    });
}

void MainWindow::setupEditor()
{
    editor->setFrameStyle(0);
    editor->setAcceptRichText(false);
    editor->setFixedWidth(EditorLayout::pageWidth());

    leftBar = new LeftBar(projectModel, this);
    drawerListPanel = new DrawerListPanel(projectModel, this);
    connect(drawerListPanel, &DrawerListPanel::panelWidthChanged, this, &MainWindow::positionSidePanels);
    connect(drawerListPanel, &DrawerListPanel::panelHeightChanged, this, &MainWindow::positionSidePanels);
    manuscriptPanel = new ManuscriptPanel(projectModel, this);
    editorHost = new EditorHost(editor, docCache, projectModel, this);
    editor->setEnabled(false);

    // Persiste o último doc aberto por projeto — restaurado no próximo load.
    // Ignora Disabled (estado transitório durante troca de projeto).
    connect(editorHost, &EditorHost::viewModeChanged, this, [this]() {
        if (projectRoot.isEmpty()) return;
        if (editorHost->viewMode().type == EditorHost::Disabled) return;
        rememberLastDocFor(projectRoot);
    });

    // Spell checker + highlighter. Idioma e custom dict são aplicados a partir
    // do projectModel quando ele carrega (onLoaded → applySpellLanguageFromModel).
    spellChecker = new SpellChecker(this);
    editor->setSpellChecker(spellChecker);
    spellHighlighter = new SpellHighlighter(editor->document(), spellChecker, this);
    // Durante o load de um doc novo, o highlight roda numa thread única e pode
    // engasgar — suspende durante o load, reaplica em seguida.
    connect(editorHost, &EditorHost::contentLoaded, this, [this]() {
        if (spellHighlighter) spellHighlighter->rehighlight();
    });

    // Mini-toolbar de seleção (infra para CreateDocFrom, Marcadores, Glossário).
    // Versão inicial leva apenas as ações de formatação inline; outros features
    // pendurarão suas ações com addAction(...).
    selectionPopup = new SelectionPopup(editor, this);
    selBoldBtn = selectionPopup->addAction(QStringLiteral("bold.svg"), tr("Negrito"),
        [this]() {
            const bool now = editor && editor->currentCharFormat().fontWeight() > QFont::Normal;
            setBold(!now);
            syncInlineFormatButtons();
        });
    selBoldBtn->setCheckable(true);
    selItalicBtn = selectionPopup->addAction(QStringLiteral("italic.svg"), tr("Itálico"),
        [this]() {
            const bool now = editor && editor->currentCharFormat().fontItalic();
            setItalic(!now);
            syncInlineFormatButtons();
        });
    selItalicBtn->setCheckable(true);
    selUnderlineBtn = selectionPopup->addAction(QStringLiteral("underline.svg"), tr("Sublinhado"),
        [this]() {
            const bool now = editor && editor->currentCharFormat().fontUnderline();
            setUnderline(!now);
            syncInlineFormatButtons();
        });
    selUnderlineBtn->setCheckable(true);
    selStrikeBtn = selectionPopup->addAction(QStringLiteral("strikethrough.svg"), tr("Tachado"),
        [this]() {
            const bool now = editor && editor->currentCharFormat().fontStrikeOut();
            setStrikethrough(!now);
            syncInlineFormatButtons();
        });
    selStrikeBtn->setCheckable(true);

    selectionPopup->addSeparator();
    selectionPopup->addAction(QStringLiteral("marker.svg"), tr("Marcador"),
        [this]() { openMarkerPickerForSelection(/*withComment=*/false); });
    selectionPopup->addAction(QStringLiteral("marker-comment.svg"), tr("Marcador com comentário"),
        [this]() { openMarkerPickerForSelection(/*withComment=*/true); });
    selectionPopup->addAction(QStringLiteral("glossary.svg"), tr("Adicionar ao Glossário"),
        [this]() {
            if (!editor || !glossaryAddPopup) return;
            QTextCursor cur = editor->textCursor();
            if (!cur.hasSelection()) return;
            QString seed = cur.selectedText();
            seed = seed.trimmed();
            seed.remove(QChar(0x2029));
            // Posiciona o popup logo abaixo do fim da seleção.
            QTextCursor end(editor->document()); end.setPosition(cur.selectionEnd());
            const QRect r = editor->cursorRect(end);
            const QPoint gp = editor->viewport()->mapToGlobal(r.bottomLeft())
                              + QPoint(0, 6);
            glossaryAddPopup->presentAt(gp, seed);
        });
    selectionPopup->addAction(QStringLiteral("doc-plus.svg"), tr("Criar documento disso..."),
        [this]() { createDocFromSelection(); });

    variationBar = new VariationBar(projectModel, editorHost, this);
    connect(editorHost, &EditorHost::viewModeChanged, variationBar, &VariationBar::refresh);

    // Atualiza o título do doc na TopToolbar conforme troca viewMode ou metadata muda.
    auto refreshDocTitle = [this]() {
        if (!toolbar || !editorHost || !projectModel) return;
        const auto vm = editorHost->viewMode();
        QString title;
        switch (vm.type) {
        case EditorHost::ChapterDoc: {
            if (const Chapter* ch = projectModel->findChapter(vm.chapterId)) {
                title = ch->title.isEmpty() ? tr("(capítulo sem título)") : ch->title;
            }
            break;
        }
        case EditorHost::SceneDoc: {
            if (const Scene* sc = projectModel->findScene(vm.chapterId, vm.sceneIndex)) {
                title = sc->title.isEmpty()
                    ? tr("Cena %1").arg(vm.sceneIndex + 1)
                    : sc->title;
            }
            break;
        }
        case EditorHost::DrawerDoc: {
            if (const DrawerItem* it = projectModel->findDrawerItem(vm.itemId)) {
                title = it->title.isEmpty() ? tr("(item sem título)") : it->title;
            }
            break;
        }
        case EditorHost::ManuscriptDoc:
        case EditorHost::Disabled:
        default:
            title.clear();
            break;
        }
        toolbar->setDocumentTitle(title);
    };
    connect(editorHost, &EditorHost::viewModeChanged, this, refreshDocTitle);
    connect(projectModel, &ProjectModel::chaptersChanged, this, refreshDocTitle);
    connect(projectModel, &ProjectModel::drawersChanged, this, refreshDocTitle);
    connect(projectModel, &ProjectModel::loaded, this, refreshDocTitle);

    // Toda vez que um doc é carregado no editor, reaplica o style do projeto
    // (font, line-height, indent) — caso contrário, o HTML serializado do doc
    // sobrescreve os settings com formatação antiga.
    connect(editorHost, &EditorHost::contentLoaded, this, &MainWindow::applyEditorStyle);

    // Sincroniza settings do projeto ao carregar (line-height + indent + paragraph spacing).
    connect(projectModel, &ProjectModel::loaded, this, [this]() {
        if (!projectModel) return;
        firstLineIndentEnabled = projectModel->firstLineIndentEnabled();
        paragraphSpacingBefore = projectModel->paragraphSpacingBefore();
        paragraphSpacingAfter = projectModel->paragraphSpacingAfter();
        currentLineHeight = projectModel->lineHeightPercent();
        if (toolbar) {
            toolbar->setFirstLineIndentEnabled(firstLineIndentEnabled);
            toolbar->setParagraphSpacingBefore(paragraphSpacingBefore);
            toolbar->setParagraphSpacingAfter(paragraphSpacingAfter);
            toolbar->setLineHeightPercent(currentLineHeight);
        }
        applyEditorStyle();
        applySpellLanguageFromModel();
    });

    sceneDetectTimer = new QTimer(this);
    sceneDetectTimer->setSingleShot(true);
    sceneDetectTimer->setInterval(1500);
    connect(sceneDetectTimer, &QTimer::timeout, this, &MainWindow::detectScenesForPending);
    connect(editorHost, &EditorHost::contentFlushed, this, [this](const QString& key) {
        if (!key.startsWith(QStringLiteral("ch:"))) return;
        sceneDetectKey = key;
        sceneDetectTimer->start();
    });

    projectSaver = new ProjectSaver(projectModel, docCache, editorHost, this);
    elementsStore = new ElementsStore(this);
    projectSaver->setElementsStore(elementsStore);

    // MarkerStore: sidecar de markers comentados. Carrega ao abrir projeto,
    // re-aplica GUIDs no contentLoaded, captura ao flush e salva ao saveProject.
    markerStore = new MarkerStore(this);
    markerPickPopup = new MarkerPickPopup(this);
    markerHoverPopup = new MarkerHoverPopup(this);

    connect(markerPickPopup, &MarkerPickPopup::confirmed,
            this, &MainWindow::applyMarkerFromPicker);
    connect(markerPickPopup, &MarkerPickPopup::cancelled,
            this, [this]() { markerEditId.clear(); });
    connect(markerHoverPopup, &MarkerHoverPopup::deleteRequested,
            this, [this](const QString& id) {
        if (!markerStore || !editor || !editorHost) return;
        const QString key = editorHost->activeKey();
        markerStore->removeMarker(key, editor->document(), id);
        markerHoverId.clear();
    });
    connect(markerHoverPopup, &MarkerHoverPopup::editRequested,
            this, [this](const QString& id) {
        openMarkerPickerForEdit(id);
    });

    // contentLoaded: reaplica markers depois que o style padrão foi reaplicado
    // (applyEditorStyle conectado ANTES vai pintar tudo com baseTextColor; nosso
    // applyToDocument depois reinjeta cor+GUID nos ranges salvos).
    connect(editorHost, &EditorHost::contentLoaded, this, [this]() {
        if (!markerStore || !editor || !editorHost) return;
        const QString key = editorHost->activeKey();
        if (key.isEmpty()) return;
        markerStore->applyToDocument(key, editor->document());
    });
    // contentFlushed: captura posições atuais dos markers no doc. Roda DEPOIS
    // do cache.set, mas o doc do editor ainda contém os GUIDs vivos.
    connect(editorHost, &EditorHost::contentFlushed, this, [this](const QString& key) {
        if (!markerStore || !editor) return;
        markerStore->captureFromDocument(key, editor->document());
    });
    // Saveproject já flushou (no syncEditorToCache), então captura já rodou —
    // basta persistir o sidecar.
    connect(projectSaver, &ProjectSaver::projectSaved, this, [this]() {
        if (markerStore) markerStore->save();
    });
    // Troca de tema chama applyTextColor (pinta tudo de baseTextColor) — perde
    // cor dos markers. Reaplica em seguida.
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, [this]() {
        if (markerStore && editor && editorHost) {
            markerStore->applyToDocument(editorHost->activeKey(), editor->document());
        }
    });

    // Som imersivo: manager (varre assets/ambience-sounds, persiste em QSettings)
    // + painel flutuante invocado pelo botão da TopToolbar.
    ambienceManager = new AmbienceManager(this);
    ambiencePanel = new AmbiencePanel(ambienceManager, this);
    connect(toolbar, &TopToolbar::immersiveSoundRequested, this, [this]() {
        if (!ambiencePanel || !toolbar) return;
        if (ambiencePanel->isVisible()) {
            ambiencePanel->hide();
            return;
        }
        ambiencePanel->showNear(toolbar->immersiveSoundButtonGlobalRect());
    });

    // Glossário: store (sidecar JSON), painel flutuante (TopToolbar) e popup de
    // "Adicionar ao Glossário..." (context menu do editor). Termos alimentam o
    // spell-checker pra nunca virarem erro.
    glossaryStore = new GlossaryStore(this);
    glossaryPanel = new GlossaryPanel(glossaryStore, this);
    glossaryAddPopup = new GlossaryAddPopup(this);

    connect(toolbar, &TopToolbar::glossaryRequested, this, [this]() {
        if (!glossaryPanel || !toolbar) return;
        if (glossaryPanel->isVisible()) {
            glossaryPanel->hide();
            return;
        }
        glossaryPanel->showNear(toolbar->glossaryButtonGlobalRect());
    });
    connect(editor, &SpellEditor::addToGlossaryRequested, this,
            [this](const QString& word, const QPoint& gp) {
        if (!glossaryAddPopup) return;
        glossaryAddPopup->presentAt(gp, word);
    });
    connect(glossaryAddPopup, &GlossaryAddPopup::confirmed, this,
            [this](const QString& term, const QString& def) {
        if (glossaryStore) glossaryStore->add(term, def);
    });
    // Persiste sidecar + alimenta spell-checker sempre que o store muda.
    connect(glossaryStore, &GlossaryStore::changed, this, [this]() {
        if (!glossaryStore) return;
        glossaryStore->save();
        if (spellChecker) spellChecker->setGlossaryWords(glossaryStore->terms());
    });

    drawerListPanel->setElementsStore(elementsStore);
    wordCounter = new WordCounter(projectModel, docCache, editorHost, this);
    // Tracking de tempo: cursor/texto/seleção contam como atividade.
    connect(editor, &QTextEdit::cursorPositionChanged, wordCounter, &WordCounter::registerCursorActivity);
    connect(editor, &QTextEdit::textChanged, wordCounter, &WordCounter::registerCursorActivity);
    connect(editor, &QTextEdit::selectionChanged, wordCounter, &WordCounter::registerCursorActivity);

    // Título reflete estado modificado/salvando.
    auto updateTitle = [this]() {
        QString suffix;
        if (projectSaver && projectSaver->isSaving()) suffix = QStringLiteral(" — salvando…");
        else if (docCache && !docCache->dirtyKeys().isEmpty()) suffix = QStringLiteral(" •");
        setWindowTitle(baseWindowTitle + suffix);
    };
    connect(docCache, &DocCache::dirtyChanged, this, updateTitle);
    connect(projectSaver, &ProjectSaver::savingChanged, this, updateTitle);
    connect(projectSaver, &ProjectSaver::projectSaved, this, updateTitle);
    connect(projectSaver, &ProjectSaver::saveFailed, this, [this](const QString& err) {
        QMessageBox::warning(this, tr("Erro ao salvar"),
            tr("Não foi possível salvar o projeto:\n%1").arg(err));
    });

    // Restaura o modo foco como estava na sessão anterior.
    {
        const bool savedFocus = QSettings()
            .value(QStringLiteral("editor/focusMode"), false).toBool();
        if (savedFocus) {
            toolbar->setFocusModeChecked(true);
            setFocusMode(true);
        }
    }

    // Autoload do último projeto (se houver).
    const QString lastPath = loadLastProjectPath();
    if (!lastPath.isEmpty() && QDir(lastPath).exists()) {
        QString loadErr;
        if (!loadProjectFrom(lastPath, &loadErr)) {
            qWarning("Falha ao recarregar último projeto: %s", qUtf8Printable(loadErr));
        }
    }

    // Ctrl+S → paulada.
    auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, [this]() {
        if (projectSaver) projectSaver->saveProject();
    });

    // Ctrl+Shift+N → novo capítulo no manuscrito ativo.
    auto* newChapterShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+N")), this);
    connect(newChapterShortcut, &QShortcut::activated, this, [this]() {
        if (!projectModel) return;
        const QString msId = projectModel->activeManuscriptId();
        if (msId.isEmpty()) return;
        bool ok = false;
        const QString title = QInputDialog::getText(this, tr("Novo capítulo"),
            tr("Título do capítulo:"), QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || title.isEmpty()) return;
        Chapter c;
        c.id = ProjectModel::uid();
        c.manuscriptId = msId;
        c.title = title;
        if (!projectRoot.isEmpty()) {
            ProjectStorage::ensureManuscriptDirs(projectRoot, msId);
        }
        projectModel->addChapter(c);
    });

    // Ctrl+N → novo item na gaveta aberta (sem efeito quando nenhuma gaveta).
    auto* newItemShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+N")), this);
    connect(newItemShortcut, &QShortcut::activated, this, [this]() {
        if (!drawerListPanel || !drawerListPanel->isPanelOpen()) return;
        emit drawerListPanel->newItemRequested(drawerListPanel->currentDrawerKey(), QString());
    });

    // Ctrl+F10 → toggle Focus Mode.
    auto* focusShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+F10")), this);
    connect(focusShortcut, &QShortcut::activated, this, [this]() {
        setFocusMode(!focusModeEnabled);
    });

    // Shift+PageDown / Shift+PageUp → próximo/anterior capítulo ou drawer item.
    // (PageUp/Down "puros" foram preservados pro QTextEdit fazer scroll nativo.)
    auto navigateRelative = [this](int delta) {
        if (!projectModel || !editorHost) return;
        const auto vm = editorHost->viewMode();
        if (vm.type == EditorHost::ChapterDoc) {
            const QString msId = vm.manuscriptId;
            QList<Chapter> list;
            for (const auto& c : projectModel->chapters()) {
                if (c.manuscriptId == msId) list.append(c);
            }
            std::sort(list.begin(), list.end(), [](const Chapter& a, const Chapter& b) {
                return a.order < b.order;
            });
            int idx = -1;
            for (int i = 0; i < list.size(); ++i) if (list.at(i).id == vm.chapterId) { idx = i; break; }
            if (idx < 0) return;
            const int next = idx + delta;
            if (next < 0 || next >= list.size()) return;
            EditorHost::ViewMode nm;
            nm.type = EditorHost::ChapterDoc;
            nm.manuscriptId = msId;
            nm.chapterId = list.at(next).id;
            editorHost->setViewMode(nm);
        } else if (vm.type == EditorHost::DrawerDoc) {
            QString drawerKey;
            const DrawerItem* current = projectModel->findDrawerItem(vm.itemId, &drawerKey);
            if (!current) return;
            const Drawer* d = projectModel->findDrawer(drawerKey);
            if (!d) return;
            // Mantém só items na mesma pasta do atual (consistente com a UI).
            QList<QString> ids;
            for (const auto& it : d->items) {
                if (it.folderId == current->folderId) ids.append(it.id);
            }
            int idx = ids.indexOf(vm.itemId);
            if (idx < 0) return;
            const int next = idx + delta;
            if (next < 0 || next >= ids.size()) return;
            EditorHost::ViewMode nm;
            nm.type = EditorHost::DrawerDoc;
            nm.itemId = ids.at(next);
            editorHost->setViewMode(nm);
        }
    };
    auto* nextDocShortcut = new QShortcut(QKeySequence(QStringLiteral("Shift+PgDown")), this);
    connect(nextDocShortcut, &QShortcut::activated, this, [navigateRelative]() { navigateRelative(+1); });
    auto* prevDocShortcut = new QShortcut(QKeySequence(QStringLiteral("Shift+PgUp")), this);
    connect(prevDocShortcut, &QShortcut::activated, this, [navigateRelative]() { navigateRelative(-1); });

    auto *container = new QWidget(this);
    container->setObjectName(QStringLiteral("editorContainer"));
    editorContainer = container;
    container->setStyleSheet(QStringLiteral("#editorContainer { background: %1; }").arg(Theme::appBackground()));
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    layout->addWidget(leftBar);

    // Drawer/Manuscript panels não entram no layout — flutuam ao lado da
    // LeftBar pra não empurrar o editor.
    drawerListPanel->setParent(container);
    manuscriptPanel->setParent(container);
    drawerListPanel->hide();
    manuscriptPanel->hide();

    // Coluna do editor: VariationBar (auto-hide) acima + [editor | scrollbar externo]
    auto* editorColumnWidget = new QWidget(this);
    editorColumn = editorColumnWidget;
    auto* editorColumnLayout = new QVBoxLayout(editorColumnWidget);
    editorColumnLayout->setContentsMargins(0, 0, 0, 0);
    editorColumnLayout->setSpacing(8);
    editorColumnLayout->addWidget(variationBar);

    // Linha do editor + scrollbar externa (fora da "folha").
    auto* editorRow = new QWidget(editorColumnWidget);
    auto* editorRowLayout = new QHBoxLayout(editorRow);
    editorRowLayout->setContentsMargins(0, 0, 0, 0);
    editorRowLayout->setSpacing(6);

    // Esconde o scrollbar nativo — o externo cuida do scroll.
    editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    externalScrollBar = new QScrollBar(Qt::Vertical, editorRow);
    externalScrollBar->setObjectName(QStringLiteral("externalEditorScroll"));
    // Espelha o estado da scrollbar interna do editor.
    auto* innerSb = editor->verticalScrollBar();
    externalScrollBar->setRange(innerSb->minimum(), innerSb->maximum());
    externalScrollBar->setPageStep(innerSb->pageStep());
    externalScrollBar->setSingleStep(innerSb->singleStep());
    externalScrollBar->setValue(innerSb->value());
    connect(innerSb, &QAbstractSlider::rangeChanged, externalScrollBar, &QScrollBar::setRange);
    connect(innerSb, &QAbstractSlider::valueChanged, externalScrollBar, &QScrollBar::setValue);
    connect(externalScrollBar, &QAbstractSlider::valueChanged, innerSb, &QScrollBar::setValue);
    // pageStep/singleStep podem mudar quando a fonte ou tamanho do widget muda;
    // re-sincroniza sob demanda.
    connect(innerSb, &QAbstractSlider::actionTriggered, this, [this, innerSb]() {
        externalScrollBar->setPageStep(innerSb->pageStep());
        externalScrollBar->setSingleStep(innerSb->singleStep());
    });

    editorRowLayout->addWidget(editor, /*stretch=*/1);
    editorRowLayout->addWidget(externalScrollBar);

    editorColumnLayout->addWidget(editorRow, /*stretch=*/1);

    // Envelopa o editorColumn numa QScrollArea: garante centralização robusta
    // mesmo quando a página é mais larga que a janela (e nesse caso aparece
    // scroll horizontal em vez de empurrar a UI).
    editorScroll = new QScrollArea(container);
    editorScroll->setObjectName(QStringLiteral("editorScroll"));
    editorScroll->setWidget(editorColumnWidget);
    editorScroll->setWidgetResizable(false);
    editorScroll->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    editorScroll->setFrameShape(QFrame::NoFrame);
    editorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    editorScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editorScroll->viewport()->setAutoFillBackground(false);
    editorScroll->setStyleSheet(QStringLiteral("QScrollArea, QScrollArea > QWidget, QScrollArea > QWidget > QWidget { background: transparent; }"));
    layout->addWidget(editorScroll, /*stretch=*/1);

    // Largura e margens internas — vêm do EditorLayout (global, QSettings).
    applyEditorLayout();

    setCentralWidget(container);

    // Painéis laterais flutuantes: ficam acima do conteúdo, à direita da LeftBar.
    drawerListPanel->raise();
    manuscriptPanel->raise();
    positionSidePanels();

    // Painel flutuante de word count — bottom-left, ao lado da LeftBar.
    wordCountPanel = new WordCountPanel(wordCounter, editorHost, projectModel, container);
    wordCountPanel->raise();
    connect(wordCountPanel, &WordCountPanel::geometryChanged, this, &MainWindow::positionWordCountPanel);
    positionWordCountPanel();

    // Painel flutuante de Referência — drag/resize livres, geometria persistida em QSettings.
    refMenuPanel = new RefMenuPanel(projectModel, editorHost, docCache, elementsStore, container);
    refMenuPanel->setProjectRoot(projectRoot);
    refMenuPanel->raise();
    connect(refMenuPanel, &RefMenuPanel::geometryChanged, this, &MainWindow::positionWordCountPanel);
    connect(toolbar, &TopToolbar::refMenuToggleRequested, this, [this]() {
        if (refMenuPanel) refMenuPanel->togglePanel();
    });

    connect(leftBar, &LeftBar::drawerSelected, this, [this](const QString& key) {
        manuscriptPanel->closePanel();
        if (drawerListPanel->isPanelOpen() && drawerListPanel->currentDrawerKey() == key) {
            drawerListPanel->closePanel();
            leftBar->clearSelection();
        } else {
            drawerListPanel->openDrawer(key);
            positionSidePanels();
            drawerListPanel->raise();
            leftBar->setActiveDrawer(key);
        }
    });
    connect(leftBar, &LeftBar::fixedActionTriggered, this, [this](LeftBar::FixedAction action) {
        if (action == LeftBar::Manuscripts) {
            drawerListPanel->closePanel();
            if (manuscriptPanel->isPanelOpen()) {
                manuscriptPanel->closePanel();
                leftBar->clearSelection();
            } else {
                manuscriptPanel->open();
                positionSidePanels();
                manuscriptPanel->raise();
                leftBar->setActiveFixedAction(LeftBar::Manuscripts);
            }
        } else if (action == LeftBar::Info) {
            if (!projectInfoPanel) {
                projectInfoPanel = new ProjectInfoPanel(projectModel, this);
                connect(projectInfoPanel, &QDialog::finished, this, [this](int) {
                    leftBar->clearSelection();
                });
            }
            // Esconde o hover preview se estava aberto pra evitar sobreposição.
            if (projectInfoHover && projectInfoHover->isVisible()) projectInfoHover->hide();
            projectInfoPanel->show();
            projectInfoPanel->raise();
            projectInfoPanel->activateWindow();
            leftBar->setActiveFixedAction(LeftBar::Info);
        }
    });

    // Hover preview no botão Info da LeftBar (read-only). Igual Mira 1.
    if (auto* infoBtn = leftBar->fixedButton(LeftBar::Info)) {
        projectInfoHover = new ProjectInfoHover(projectModel, nullptr);
        projectInfoHoverOpenTimer = new QTimer(this);
        projectInfoHoverOpenTimer->setSingleShot(true);
        projectInfoHoverOpenTimer->setInterval(350);
        projectInfoHoverCloseTimer = new QTimer(this);
        projectInfoHoverCloseTimer->setSingleShot(true);
        projectInfoHoverCloseTimer->setInterval(220);

        connect(projectInfoHoverOpenTimer, &QTimer::timeout, this, [this, infoBtn]() {
            // Reaberto pela posição atual do botão (handles repaginação).
            const QPoint topRight = infoBtn->mapToGlobal(QPoint(infoBtn->width(), 0));
            projectInfoHover->presentNear(topRight);
        });
        connect(projectInfoHoverCloseTimer, &QTimer::timeout, this, [this]() {
            if (projectInfoHover && !projectInfoHover->underMouse()) projectInfoHover->hide();
        });
        connect(projectInfoHover, &ProjectInfoHover::hoverLeft, this, [this]() {
            if (projectInfoHover) projectInfoHover->hide();
        });

        infoBtn->installEventFilter(this);
    }
    connect(manuscriptPanel, &ManuscriptPanel::chapterActivated, this, [this](const QString& manuscriptId, const QString& chapterId) {
        EditorHost::ViewMode vm;
        vm.type = EditorHost::ChapterDoc;
        vm.manuscriptId = manuscriptId;
        vm.chapterId = chapterId;
        editorHost->setViewMode(vm);
    });
    connect(manuscriptPanel, &ManuscriptPanel::sceneActivated, this, [this](const QString& manuscriptId, const QString& chapterId, int sceneIndex) {
        EditorHost::ViewMode vm;
        vm.type = EditorHost::SceneDoc;
        vm.manuscriptId = manuscriptId;
        vm.chapterId = chapterId;
        vm.sceneIndex = sceneIndex;
        editorHost->setViewMode(vm);
    });
    connect(manuscriptPanel, &ManuscriptPanel::newManuscriptRequested, this, [this]() {
        bool ok = false;
        const QString title = QInputDialog::getText(this, tr("Novo manuscrito"),
            tr("Título do manuscrito:"), QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || title.isEmpty()) return;
        Manuscript m;
        m.id = ProjectModel::uid();
        m.title = title;
        projectModel->addManuscript(m);
        projectModel->setActiveManuscriptId(m.id);
    });
    connect(manuscriptPanel, &ManuscriptPanel::newChapterRequested, this, [this](const QString& manuscriptId) {
        if (manuscriptId.isEmpty()) return;
        bool ok = false;
        const QString title = QInputDialog::getText(this, tr("Novo capítulo"),
            tr("Título do capítulo:"), QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || title.isEmpty()) return;
        Chapter c;
        c.id = ProjectModel::uid();
        c.manuscriptId = manuscriptId;
        c.title = title;
        if (!projectRoot.isEmpty()) {
            ProjectStorage::ensureManuscriptDirs(projectRoot, manuscriptId);
        }
        projectModel->addChapter(c);
    });

    // ---- Context menu / drag handlers do ManuscriptPanel ----
    connect(manuscriptPanel, &ManuscriptPanel::renameChapterRequested, this, [this](const QString& chapterId) {
        const Chapter* c = projectModel->findChapter(chapterId);
        if (!c) return;
        bool ok = false;
        const QString newTitle = QInputDialog::getText(this, tr("Renomear capítulo"),
            tr("Novo título:"), QLineEdit::Normal, c->title, &ok).trimmed();
        if (!ok || newTitle.isEmpty()) return;
        projectModel->updateChapterTitle(chapterId, newTitle);
    });

    connect(manuscriptPanel, &ManuscriptPanel::deleteChapterRequested, this, [this](const QString& chapterId) {
        const Chapter* c = projectModel->findChapter(chapterId);
        if (!c) return;
        const QString title = c->title.isEmpty() ? tr("(sem título)") : c->title;
        const auto ret = QMessageBox::question(this, tr("Excluir capítulo"),
            tr("Excluir \"%1\"? O texto do capítulo será removido. Esta ação não pode ser desfeita.").arg(title),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) return;

        // Se o capítulo está aberto no editor, desabilita primeiro.
        if (editorHost) {
            const auto vm = editorHost->viewMode();
            if ((vm.type == EditorHost::ChapterDoc || vm.type == EditorHost::SceneDoc) && vm.chapterId == chapterId) {
                editorHost->disable();
            }
        }
        projectModel->removeChapter(chapterId);
    });

    connect(manuscriptPanel, &ManuscriptPanel::renameSceneRequested, this, [this](const QString& chapterId, int sceneIndex) {
        const Scene* s = projectModel->findScene(chapterId, sceneIndex);
        if (!s) return;
        const QString current = s->title.isEmpty() ? tr("Cena %1").arg(sceneIndex + 1) : s->title;
        bool ok = false;
        const QString newTitle = QInputDialog::getText(this, tr("Renomear cena"),
            tr("Novo título:"), QLineEdit::Normal, current, &ok).trimmed();
        if (!ok || newTitle.isEmpty()) return;
        projectModel->updateSceneTitle(chapterId, sceneIndex, newTitle);
    });

    connect(manuscriptPanel, &ManuscriptPanel::createVariationRequested, this, [this](const QString& chapterId, int sceneIndex) {
        projectModel->createSceneVariation(chapterId, sceneIndex, QString());
    });

    auto reloadActiveEditorIfMatches = [this](const QString& chapterId) {
        if (!editorHost) return;
        const auto vm = editorHost->viewMode();
        if ((vm.type == EditorHost::ChapterDoc || vm.type == EditorHost::SceneDoc) && vm.chapterId == chapterId) {
            EditorHost::ViewMode tmp; tmp.type = EditorHost::Disabled;
            editorHost->setViewMode(tmp);
            editorHost->setViewMode(vm);
        }
    };

    connect(manuscriptPanel, &ManuscriptPanel::deleteSceneRequested, this, [this, reloadActiveEditorIfMatches](const QString& chapterId, int sceneIndex) {
        const Chapter* ch = projectModel->findChapter(chapterId);
        if (!ch) return;
        if (ch->scenes.size() <= 1) {
            QMessageBox::information(this, tr("Excluir cena"),
                tr("Não dá pra excluir a única cena de um capítulo. Apague o texto manualmente se quiser limpar."));
            return;
        }
        const auto ret = QMessageBox::question(this, tr("Excluir cena"),
            tr("Excluir esta cena? O texto da cena será removido."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) return;

        // Garante que o editor sincronizou pro cache antes de mexer.
        if (editorHost) editorHost->syncEditorToCache();

        const QString key = DocCache::chapterKey(ch->manuscriptId, chapterId);
        QString html = docCache && docCache->has(key) ? docCache->get(key) : QString();
        if (html.isEmpty() && !projectRoot.isEmpty()) {
            bool ok = false;
            html = ProjectStorage::readChapter(projectRoot, ch->file, &ok);
        }
        QStringList segments = SceneUtils::splitHtmlIntoScenes(html);
        if (sceneIndex < 0 || sceneIndex >= segments.size()) return;
        segments.removeAt(sceneIndex);
        const QString newHtml = SceneUtils::joinScenesHtml(segments);
        if (docCache) docCache->set(key, newHtml, /*markDirty=*/true);

        // Atualiza metadados de cenas no model.
        QList<Scene> remaining = ch->scenes;
        if (sceneIndex < remaining.size()) remaining.removeAt(sceneIndex);
        for (int i = 0; i < remaining.size(); ++i) remaining[i].order = i;
        projectModel->updateChapterScenes(chapterId, remaining);

        reloadActiveEditorIfMatches(chapterId);
    });

    connect(manuscriptPanel, &ManuscriptPanel::reorderChapterRequested, this, [this](const QString& chapterId, int targetIndex) {
        projectModel->reorderChapter(chapterId, targetIndex);
    });

    connect(manuscriptPanel, &ManuscriptPanel::reorderSceneRequested, this, [this, reloadActiveEditorIfMatches](const QString& chapterId, int srcIdx, int targetIdx) {
        const Chapter* ch = projectModel->findChapter(chapterId);
        if (!ch) return;

        if (editorHost) editorHost->syncEditorToCache();

        const QString key = DocCache::chapterKey(ch->manuscriptId, chapterId);
        QString html = docCache && docCache->has(key) ? docCache->get(key) : QString();
        if (html.isEmpty() && !projectRoot.isEmpty()) {
            bool ok = false;
            html = ProjectStorage::readChapter(projectRoot, ch->file, &ok);
        }
        QStringList segments = SceneUtils::splitHtmlIntoScenes(html);
        if (srcIdx < 0 || srcIdx >= segments.size()) return;
        if (targetIdx < 0) targetIdx = 0;
        if (targetIdx > segments.size()) targetIdx = segments.size();
        if (srcIdx < targetIdx) --targetIdx;
        if (srcIdx == targetIdx) return;
        const QString moved = segments.takeAt(srcIdx);
        segments.insert(targetIdx, moved);
        const QString newHtml = SceneUtils::joinScenesHtml(segments);
        if (docCache) docCache->set(key, newHtml, /*markDirty=*/true);

        // Atualiza metadados de cenas (mantém títulos/variations).
        QList<Scene> scenes = ch->scenes;
        if (srcIdx < scenes.size()) {
            Scene movedScene = scenes.takeAt(srcIdx);
            const int insertAt = qBound(0, targetIdx, scenes.size());
            scenes.insert(insertAt, movedScene);
            for (int i = 0; i < scenes.size(); ++i) scenes[i].order = i;
            projectModel->updateChapterScenes(chapterId, scenes);
        }

        reloadActiveEditorIfMatches(chapterId);
    });
    connect(leftBar, &LeftBar::newDrawerRequested, this, [this]() {
        DrawerCreateDialog dlg(elementsStore, this);
        if (dlg.exec() != QDialog::Accepted) return;
        if (dlg.title().isEmpty()) return;
        Drawer d;
        d.key = ProjectModel::uid();
        d.title = dlg.title();
        d.color = dlg.color();
        d.drawerElementType = dlg.elementTypeId();
        d.drawerIcon = dlg.iconId();
        // Se há tipo de elemento, herda o ícone canônico do tipo (compat Mira 1).
        if (!d.drawerElementType.isEmpty() && elementsStore) {
            if (const ElementType* t = elementsStore->findType(d.drawerElementType)) {
                d.drawerElementIcon = t->icon;
            }
        }
        projectModel->addDrawer(d);
    });
    connect(drawerListPanel, &DrawerListPanel::newItemRequested, this, [this](const QString& drawerKey, const QString& folderId) {
        const Drawer* drawer = projectModel->findDrawer(drawerKey);
        const QString elemType = drawer ? drawer->drawerElementType : QString();
        const bool visual = elemType == QStringLiteral("character")
            || elemType == QStringLiteral("setting")
            || elemType == QStringLiteral("object");
        if (visual) {
            ElementCreateDialog dlg(elemType, this);
            if (dlg.exec() != QDialog::Accepted) return;
            if (dlg.title().isEmpty()) return;
            // Cria registro em ElementsStore primeiro
            Element elem;
            elem.name = dlg.title();
            elem.type = elemType;
            elem.icon = elemType == QStringLiteral("character") ? QStringLiteral("user")
                      : elemType == QStringLiteral("setting") ? QStringLiteral("map")
                      : QStringLiteral("cube");
            elem.role = dlg.role();
            elem.image = dlg.imageDataUrl();
            const QString elementId = elementsStore->addElement(elem);
            // Cria drawer item vinculado
            DrawerItem it;
            it.id = ProjectModel::uid();
            it.title = dlg.title();
            it.folderId = folderId;
            it.hasInlineHtml = true;
            it.html = QStringLiteral("<p></p>");
            it.elementType = elemType;
            it.elementId = elementId;
            it.role = dlg.role();
            projectModel->addDrawerItem(drawerKey, it);
            return;
        }
        bool ok = false;
        const QString title = QInputDialog::getText(this, tr("Novo item"),
            tr("Nome do item:"), QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || title.isEmpty()) return;
        DrawerItem it;
        it.id = ProjectModel::uid();
        it.title = title;
        it.folderId = folderId;
        it.hasInlineHtml = true;
        it.html = QStringLiteral("<p></p>");
        projectModel->addDrawerItem(drawerKey, it);
    });
    connect(drawerListPanel, &DrawerListPanel::itemActivated, this, [this](const QString& /*drawerKey*/, const QString& itemId) {
        EditorHost::ViewMode vm;
        vm.type = EditorHost::DrawerDoc;
        vm.itemId = itemId;
        editorHost->setViewMode(vm);
    });
    connect(drawerListPanel, &DrawerListPanel::newFolderRequested, this, [this](const QString& drawerKey, const QString& parentFolderId) {
        bool ok = false;
        const QString title = QInputDialog::getText(this, tr("Nova pasta"),
            tr("Nome da pasta:"), QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || title.isEmpty()) return;
        Folder f;
        f.id = ProjectModel::uid();
        f.title = title;
        f.parentId = parentFolderId;
        projectModel->addDrawerFolder(drawerKey, f);
    });

    // ---- Context menu dos cards: editar / add+remove elemento / refMenu / excluir ----
    connect(drawerListPanel, &DrawerListPanel::editItemRequested, this,
            [this](const QString& drawerKey, const QString& itemId) {
        const DrawerItem* item = projectModel->findDrawerItem(itemId);
        if (!item) return;
        const QString elemType = item->elementType;
        ElementCreateDialog dlg(elemType, this);
        // Pré-preenche valores atuais — pega foto/role do Element vinculado se houver.
        QString imageDataUrl;
        QString role = item->role;
        if (!item->elementId.isEmpty() && elementsStore) {
            if (const Element* e = elementsStore->findElement(item->elementId)) {
                imageDataUrl = e->image;
                if (role.isEmpty()) role = e->role;
            }
        }
        dlg.setInitial(item->title, role, imageDataUrl);
        if (dlg.exec() != QDialog::Accepted) return;
        const QString newTitle = dlg.title();
        const QString newRole = dlg.role();
        const QString newImage = dlg.imageDataUrl();

        projectModel->updateDrawerItemMeta(itemId, newTitle, newRole);

        // Sincroniza Element vinculado, se houver. Foto só é editável quando há elementId.
        if (!item->elementId.isEmpty() && elementsStore) {
            if (const Element* existing = elementsStore->findElement(item->elementId)) {
                Element copy = *existing;
                copy.name = newTitle;
                copy.role = newRole;
                copy.image = newImage;
                elementsStore->updateElement(copy.id, copy);
            }
        } else if (!elemType.isEmpty() && !newImage.isEmpty() && elementsStore) {
            // Item tem tipo mas ainda não tinha Element vinculado e o usuário colocou foto agora —
            // cria Element novo e amarra ao item.
            Element elem;
            elem.name = newTitle;
            elem.type = elemType;
            elem.icon = elemType == QStringLiteral("character") ? QStringLiteral("user")
                      : elemType == QStringLiteral("setting") ? QStringLiteral("map")
                      : QStringLiteral("cube");
            elem.role = newRole;
            elem.image = newImage;
            const QString newElementId = elementsStore->addElement(elem);
            projectModel->setDrawerItemElement(itemId, elemType, item->elementIcon, newElementId);
        }
        Q_UNUSED(drawerKey);
    });

    connect(drawerListPanel, &DrawerListPanel::addElementRequested, this,
            [this](const QString& /*drawerKey*/, const QString& itemId) {
        const DrawerItem* item = projectModel->findDrawerItem(itemId);
        if (!item) return;
        // Pergunta o tipo via menu pop-up
        QStringList typeIds;
        QStringList labels;
        if (elementsStore) {
            for (const auto& t : elementsStore->elementTypes()) {
                typeIds.append(t.id);
                labels.append(t.label);
            }
        }
        if (typeIds.isEmpty()) return;
        bool ok = false;
        const QString chosenLabel = QInputDialog::getItem(this, tr("Adicionar elemento"),
            tr("Tipo do elemento:"), labels, 0, false, &ok);
        if (!ok || chosenLabel.isEmpty()) return;
        const int idx = labels.indexOf(chosenLabel);
        if (idx < 0) return;
        const QString chosenType = typeIds.at(idx);
        QString icon;
        if (elementsStore) {
            if (const ElementType* t = elementsStore->findType(chosenType)) icon = t->icon;
        }
        projectModel->setDrawerItemElement(itemId, chosenType, icon, QString());
    });

    connect(drawerListPanel, &DrawerListPanel::removeElementRequested, this,
            [this](const QString& /*drawerKey*/, const QString& itemId) {
        const DrawerItem* item = projectModel->findDrawerItem(itemId);
        if (!item) return;
        // Limpa elementType/Icon/Id — o doc puro permanece (html/file inalterados).
        // O Element no ElementsStore continua existindo (pode estar vinculado em outro lugar).
        projectModel->setDrawerItemElement(itemId, QString(), QString(), QString());
    });

    connect(drawerListPanel, &DrawerListPanel::openInRefMenuRequested, this,
            [this](const QString& drawerKey, const QString& itemId) {
        if (refMenuPanel) refMenuPanel->openForDrawer(drawerKey, itemId);
    });

    connect(drawerListPanel, &DrawerListPanel::deleteItemRequested, this,
            [this](const QString& /*drawerKey*/, const QString& itemId) {
        const DrawerItem* item = projectModel->findDrawerItem(itemId);
        if (!item) return;
        const QString title = item->title.isEmpty() ? tr("(sem título)") : item->title;
        const auto ret = QMessageBox::question(this, tr("Excluir item"),
            tr("Excluir \"%1\" da gaveta? Esta ação não pode ser desfeita.").arg(title),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
        projectModel->removeDrawerItem(itemId);
        projectModel->removeBondsForItem(itemId);
    });

    // ---- Vínculos: drag-to-create e clique numa linha ----
    connect(drawerListPanel, &DrawerListPanel::bondCreateRequested, this,
            [this](const QString& drawerKey, const QString& fromItemId,
                   const QString& toItemId, const QPoint& spawn) {
        // Se já existe bond entre os dois, abre o view em vez do popup de criação.
        for (const auto& b : projectModel->characterBonds()) {
            if (b.drawerKey != drawerKey) continue;
            if ((b.fromItemId == fromItemId && b.toItemId == toItemId) ||
                (b.fromItemId == toItemId && b.toItemId == fromItemId)) {
                openBondViewPanel(drawerKey, b.id, spawn);
                return;
            }
        }
        openBondCreatePopup(drawerKey, fromItemId, toItemId, spawn);
    });
    connect(drawerListPanel, &DrawerListPanel::bondViewRequested, this,
            [this](const QString& drawerKey, const QString& bondId, const QPoint& spawn) {
        openBondViewPanel(drawerKey, bondId, spawn);
    });
    // Re-render dos bonds quando a lista global mudar (criar/editar/excluir).
    connect(projectModel, &ProjectModel::characterBondsChanged, this, [this]() {
        if (drawerListPanel) drawerListPanel->refreshBonds();
    });
    // Quando uma gaveta é removida ou drawer items mudam, limpa bonds órfãos.
    connect(projectModel, &ProjectModel::drawersChanged, this, [this]() {
        // Bonds cujo from/to não existe mais somem silenciosamente.
        const auto allBonds = projectModel->characterBonds();
        for (const auto& b : allBonds) {
            const bool fromMissing = projectModel->findDrawerItem(b.fromItemId) == nullptr;
            const bool toMissing = projectModel->findDrawerItem(b.toItemId) == nullptr;
            if (fromMissing || toMissing) projectModel->removeCharacterBond(b.id);
        }
    });

    // ---- Context menu da gaveta (LeftBar) ----
    connect(leftBar, &LeftBar::drawerContextRequested, this,
            [this](const QString& drawerKey, const QPoint& globalPos) {
        const Drawer* drawer = projectModel->findDrawer(drawerKey);
        if (!drawer) return;

        QMenu menu(this);
        menu.setStyleSheet(QStringLiteral(R"(
            QMenu {
                background: %1;
                color: %2;
                border: 1px solid %3;
                border-radius: 6px;
                padding: 4px;
            }
            QMenu::item { padding: 6px 16px; border-radius: 4px; }
            QMenu::item:selected { background: %4; color: %5; }
            QMenu::separator { height: 1px; background: %3; margin: 4px 6px; }
        )").arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
               Theme::hoverOverlay(), Theme::textBright()));

        auto* editAct = menu.addAction(tr("Editar gaveta…"));
        connect(editAct, &QAction::triggered, this, [this, drawerKey]() {
            const Drawer* d = projectModel->findDrawer(drawerKey);
            if (!d) return;
            DrawerCreateDialog dlg(elementsStore, this);
            dlg.configureForEdit(d->title, d->drawerIcon, d->color, d->drawerElementType);
            if (dlg.exec() != QDialog::Accepted) return;
            QString newElementIcon;
            const QString newElemType = dlg.elementTypeId();
            if (!newElemType.isEmpty() && elementsStore) {
                if (const ElementType* t = elementsStore->findType(newElemType)) {
                    newElementIcon = t->icon;
                }
            }
            projectModel->updateDrawer(drawerKey, dlg.title(), dlg.color(),
                                       dlg.iconId(), newElemType, newElementIcon);
        });

        menu.addSeparator();

        auto* deleteAct = menu.addAction(tr("Excluir gaveta"));
        connect(deleteAct, &QAction::triggered, this, [this, drawerKey]() {
            const Drawer* d = projectModel->findDrawer(drawerKey);
            if (!d) return;
            if (!projectModel->drawerIsEmpty(drawerKey)) {
                QMessageBox::information(this, tr("Excluir gaveta"),
                    tr("Esta gaveta não está vazia. Esvazie os itens e pastas antes de excluí-la."));
                return;
            }
            const QString title = d->title.isEmpty() ? tr("(sem nome)") : d->title;
            const auto ret = QMessageBox::question(this, tr("Excluir gaveta"),
                tr("Excluir a gaveta \"%1\"?").arg(title),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (ret != QMessageBox::Yes) return;
            projectModel->removeDrawer(drawerKey);
        });

        menu.exec(globalPos);
    });

    // Seed opcional pra screenshots e dev local: setar env MIRA_DEMO_SEED=1
    if (qEnvironmentVariableIntValue("MIRA_DEMO_SEED") == 1) {
        const QString seedRoot = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
            .absoluteFilePath(QStringLiteral("dev-project"));
        applyProjectRoot(seedRoot);
        Drawer d; d.key = QStringLiteral("demo-pers"); d.title = QStringLiteral("Personagens"); d.color = QStringLiteral("#8c5a3a");
        projectModel->addDrawer(d);
        Folder f; f.id = QStringLiteral("fld-1"); f.title = QStringLiteral("Protagonistas");
        projectModel->addDrawerFolder(d.key, f);
        Folder f2; f2.id = QStringLiteral("fld-2"); f2.title = QStringLiteral("Coadjuvantes"); f2.parentId = QString();
        projectModel->addDrawerFolder(d.key, f2);
        Folder f3; f3.id = QStringLiteral("fld-3"); f3.title = QStringLiteral("Família"); f3.parentId = QStringLiteral("fld-1");
        projectModel->addDrawerFolder(d.key, f3);

        DrawerItem it1; it1.id = QStringLiteral("itm-1"); it1.title = QStringLiteral("Marina"); it1.folderId = QStringLiteral("fld-1"); it1.hasInlineHtml = true;
        it1.html = QStringLiteral("<h2>Marina</h2><p>Vinte e oito anos. Cresceu em Florianópolis, sempre olhando o mar como quem aguarda alguém.</p>");
        DrawerItem it2; it2.id = QStringLiteral("itm-2"); it2.title = QStringLiteral("Lóris"); it2.folderId = QStringLiteral("fld-3"); it2.hasInlineHtml = true;
        it2.html = QStringLiteral("<h2>Lóris</h2><p>O irmão mais velho.</p>");
        DrawerItem it3; it3.id = QStringLiteral("itm-3"); it3.title = QStringLiteral("O Velho do Cais"); it3.folderId = QStringLiteral("fld-2"); it3.hasInlineHtml = true;
        it3.html = QStringLiteral("<h2>O Velho do Cais</h2><p>Ninguém sabe o nome dele.</p>");
        DrawerItem it4; it4.id = QStringLiteral("itm-4"); it4.title = QStringLiteral("Nota solta"); it4.folderId = QString(); it4.hasInlineHtml = true;
        it4.html = QStringLiteral("<p>Item na raiz da gaveta.</p>");
        projectModel->addDrawerItem(d.key, it1);
        projectModel->addDrawerItem(d.key, it2);
        projectModel->addDrawerItem(d.key, it3);
        projectModel->addDrawerItem(d.key, it4);

        Drawer d2; d2.key = QStringLiteral("demo-cen"); d2.title = QStringLiteral("Cenários"); d2.color = QStringLiteral("#3a8c7a");
        projectModel->addDrawer(d2);

        // Manuscrito de demo com 3 capítulos file-backed
        Manuscript ms; ms.id = QStringLiteral("demo-ms"); ms.title = QStringLiteral("A Motorista");
        projectModel->addManuscript(ms);
        projectModel->setActiveManuscriptId(ms.id);
        if (!projectRoot.isEmpty()) {
            ProjectStorage::ensureManuscriptDirs(projectRoot, ms.id);
        }
        struct Seed { QString id; QString title; QString body; };
        const QVector<Seed> seeds = {
            { QStringLiteral("ch-1"), QStringLiteral("Primeira corrida"),
              QStringLiteral("<h2>Primeira corrida</h2><p>Era madrugada quando o aplicativo apitou pela primeira vez. Marina respirou fundo e aceitou.</p>") },
            { QStringLiteral("ch-2"), QStringLiteral("Reflexos no asfalto"),
              QStringLiteral("<h2>Reflexos no asfalto</h2><p>A chuva fina cobriu as ruas como um véu.</p>"
                             "<hr>"
                             "<h3>O posto vazio</h3><p>Marina parou no posto e ficou olhando o céu por um tempo que não soube medir.</p>"
                             "<hr>"
                             "<h3>Encontro com Lóris</h3><p>O irmão apareceu na esquina, como se sempre estivesse esperando.</p>") },
            { QStringLiteral("ch-3"), QStringLiteral("O passageiro silencioso"),
              QStringLiteral("<h2>O passageiro silencioso</h2><p>Ele entrou sem dizer pra onde queria ir.</p>") },
        };
        for (const auto& s : seeds) {
            Chapter c; c.id = s.id; c.manuscriptId = ms.id; c.title = s.title;
            projectModel->addChapter(c);
            const Chapter* stored = projectModel->findChapter(c.id);
            if (stored && !projectRoot.isEmpty()) {
                ProjectStorage::writeChapter(projectRoot, stored->file, s.body);
            }
            // Popula cenas a partir do HTML
            const QList<Scene> scenes = ProjectModel::buildScenesFromHtml(s.body, {});
            if (!scenes.isEmpty()) {
                projectModel->updateChapterScenes(s.id, scenes);
            }
        }

        // Abre o painel de manuscritos
        manuscriptPanel->open();

        // Foca na Cena 1 do capítulo 2 e cria uma variação alternativa
        EditorHost::ViewMode vm;
        vm.type = EditorHost::SceneDoc;
        vm.manuscriptId = ms.id;
        vm.chapterId = QStringLiteral("ch-2");
        vm.sceneIndex = 0;
        editorHost->setViewMode(vm);
        editorHost->createVariationForCurrentScene(QStringLiteral("Versão noturna"));
    }
}

void MainWindow::setupToolbar()
{
    toolbar->setFontFamilies({QStringLiteral("Alegreya")}, currentFontFamily);
    toolbar->setFontSize(currentFontSize);
    toolbar->setLineHeightPercent(currentLineHeight);
    toolbar->setFirstLineIndentEnabled(firstLineIndentEnabled);
    toolbar->setParagraphSpacingBefore(paragraphSpacingBefore);
    toolbar->setParagraphSpacingAfter(paragraphSpacingAfter);

    connect(toolbar, &TopToolbar::fontFamilyChanged, this, &MainWindow::setFontFamily);
    connect(toolbar, &TopToolbar::fontSizeChanged, this, &MainWindow::setFontSize);
    connect(toolbar, &TopToolbar::lineHeightChanged, this, &MainWindow::setLineHeight);
    connect(toolbar, &TopToolbar::firstLineIndentToggled, this, &MainWindow::setFirstLineIndent);
    connect(toolbar, &TopToolbar::paragraphSpacingBeforeChanged, this, &MainWindow::setParagraphSpacingBefore);
    connect(toolbar, &TopToolbar::paragraphSpacingAfterChanged, this, &MainWindow::setParagraphSpacingAfter);
    connect(toolbar, &TopToolbar::focusModeToggled, this, &MainWindow::setFocusMode);
    connect(toolbar, &TopToolbar::addImageRequested, this, &MainWindow::onAddImageRequested);
    connect(toolbar, &TopToolbar::newProjectRequested, this, &MainWindow::onNewProjectRequested);
    connect(toolbar, &TopToolbar::openProjectRequested, this, &MainWindow::onOpenProjectRequested);
    connect(toolbar, &TopToolbar::saveProjectRequested, this, [this]() {
        if (projectSaver) projectSaver->saveProject();
    });
    connect(toolbar, &TopToolbar::settingsRequested, this, &MainWindow::onSettingsRequested);
    connect(toolbar, &TopToolbar::themePanelRequested, this, &MainWindow::onThemePanelRequested);
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &MainWindow::onThemeChanged);
    connect(EditorLayout::Manager::instance(), &EditorLayout::Manager::layoutChanged,
            this, &MainWindow::onEditorLayoutChanged);

    connect(toolbar, &TopToolbar::boldToggled, this, &MainWindow::setBold);
    connect(toolbar, &TopToolbar::italicToggled, this, &MainWindow::setItalic);
    connect(toolbar, &TopToolbar::underlineToggled, this, &MainWindow::setUnderline);
    connect(toolbar, &TopToolbar::strikethroughToggled, this, &MainWindow::setStrikethrough);
    connect(editor, &QTextEdit::currentCharFormatChanged, this, &MainWindow::syncInlineFormatButtons);

    auto* boldShortcut = new QShortcut(QKeySequence::Bold, this);
    connect(boldShortcut, &QShortcut::activated, this, [this]() {
        setBold(editor && editor->currentCharFormat().fontWeight() <= QFont::Normal);
        syncInlineFormatButtons();
    });
    auto* italicShortcut = new QShortcut(QKeySequence::Italic, this);
    connect(italicShortcut, &QShortcut::activated, this, [this]() {
        setItalic(editor && !editor->currentCharFormat().fontItalic());
        syncInlineFormatButtons();
    });
    auto* underlineShortcut = new QShortcut(QKeySequence::Underline, this);
    connect(underlineShortcut, &QShortcut::activated, this, [this]() {
        setUnderline(editor && !editor->currentCharFormat().fontUnderline());
        syncInlineFormatButtons();
    });
    auto* strikeShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")), this);
    connect(strikeShortcut, &QShortcut::activated, this, [this]() {
        setStrikethrough(editor && !editor->currentCharFormat().fontStrikeOut());
        syncInlineFormatButtons();
    });
}

void MainWindow::setAvailableFontFamilies(const QStringList &families)
{
    if (families.isEmpty()) {
        return;
    }
    QString preferredDefault = currentFontFamily;
    if (families.contains(QStringLiteral("Alegreya"), Qt::CaseInsensitive)) {
        preferredDefault = QStringLiteral("Alegreya");
    } else if (families.contains(QStringLiteral("Lora"), Qt::CaseInsensitive)) {
        preferredDefault = QStringLiteral("Lora");
    } else if (families.contains(QStringLiteral("Crimson Text"), Qt::CaseInsensitive)) {
        preferredDefault = QStringLiteral("Crimson Text");
    } else {
        preferredDefault = families.first();
    }
    currentFontFamily = preferredDefault;
    toolbar->setFontFamilies(families, currentFontFamily);
    applyEditorStyle();
}

void MainWindow::applyEditorStyle()
{
    QFont font(currentFontFamily, currentFontSize);
    editor->setFont(font);
    editor->document()->setDefaultFont(font);

    // Cor de texto do tema é a base; baseTextColor segue o tema atual.
    baseTextColor = QColor(Theme::editorTextColor());
    QPalette p = editor->palette();
    p.setColor(QPalette::Base, QColor(Theme::editorBackground()));
    editor->setPalette(p);

    // Fundo da "página" via stylesheet — palette sozinha não vence o styling
    // padrão do QTextEdit/viewport. NÃO setamos `color` aqui: o focus mode
    // depende de mexer em QPalette::Text pra fazer o dim dos blocos não focados,
    // e CSS-color sobrepõe palette, quebrando o efeito.
    editor->setStyleSheet(QStringLiteral(
        "QTextEdit { background-color: %1; selection-background-color: %2; }")
        .arg(Theme::editorBackground(),
             Theme::accentDefault()));
    if (editor->viewport()) {
        editor->viewport()->setAutoFillBackground(true);
        QPalette vp = editor->viewport()->palette();
        vp.setColor(QPalette::Base, QColor(Theme::editorBackground()));
        vp.setColor(QPalette::Window, QColor(Theme::editorBackground()));
        editor->viewport()->setPalette(vp);
    }

    QTextCursor cursor(editor->document());
    cursor.select(QTextCursor::Document);
    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(currentLineHeight, QTextBlockFormat::ProportionalHeight);
    blockFormat.setTextIndent(firstLineIndentEnabled ? 30 : 0);
    blockFormat.setTopMargin(paragraphSpacingBefore);
    blockFormat.setBottomMargin(paragraphSpacingAfter);
    cursor.mergeBlockFormat(blockFormat);

    // Aplica a cor de texto do tema em todo o documento (charFormat foreground
    // sobrepõe palette nos blocos já existentes; sem isso, texto fica preso na
    // cor antiga ao trocar de tema).
    applyTextColor();
}

void MainWindow::applyTextColor()
{
    if (!editor || !editor->document()) return;

    QColor textColor = baseTextColor;
    textColor.setAlpha(focusModeEnabled ? 100 : 255);

    QPalette p = editor->palette();
    p.setColor(QPalette::Text, textColor);
    editor->setPalette(p);

    // Itera por fragmentos pra preservar foreground custom dos markers:
    // onde tem background (highlight), o foreground vem de pickContrastingFg
    // baseado na cor do highlight; nos demais, usa textColor do tema.
    const bool wasModified = editor->document()->isModified();
    QTextDocument* doc = editor->document();
    for (QTextBlock block = doc->firstBlock(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid() || frag.length() == 0) continue;
            const QTextCharFormat existing = frag.charFormat();
            QColor fg = textColor;
            const QBrush bgBrush = existing.background();
            if (bgBrush.style() != Qt::NoBrush && bgBrush.color().alpha() > 0) {
                fg = MarkerStore::pickContrastingFg(bgBrush.color());
            }
            QTextCursor c(doc);
            c.setPosition(frag.position());
            c.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
            QTextCharFormat fmt;
            fmt.setForeground(fg);
            c.mergeCharFormat(fmt);
        }
    }
    doc->setModified(wasModified);

    if (editor->viewport()) editor->viewport()->update();
}

void MainWindow::setFontFamily(const QString &family)
{
    currentFontFamily = family;
    applyEditorStyle();
}

void MainWindow::setFontSize(int pt)
{
    currentFontSize = pt;
    applyEditorStyle();
}

void MainWindow::setLineHeight(int percent)
{
    currentLineHeight = percent;
    if (projectModel) projectModel->setLineHeightPercent(percent);
    applyEditorStyle();
}

void MainWindow::setFirstLineIndent(bool enabled)
{
    firstLineIndentEnabled = enabled;
    if (projectModel) projectModel->setFirstLineIndentEnabled(enabled);
    applyEditorStyle();
}

void MainWindow::setParagraphSpacingBefore(int px)
{
    paragraphSpacingBefore = px;
    if (projectModel) projectModel->setParagraphSpacingBefore(px);
    applyEditorStyle();
}

void MainWindow::setParagraphSpacingAfter(int px)
{
    paragraphSpacingAfter = px;
    if (projectModel) projectModel->setParagraphSpacingAfter(px);
    applyEditorStyle();
}

void MainWindow::setBold(bool enabled)
{
    if (!editor) return;
    QTextCharFormat fmt;
    fmt.setFontWeight(enabled ? QFont::Bold : QFont::Normal);
    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection()) cursor.select(QTextCursor::WordUnderCursor);
    cursor.mergeCharFormat(fmt);
    editor->mergeCurrentCharFormat(fmt);
}

void MainWindow::setItalic(bool enabled)
{
    if (!editor) return;
    QTextCharFormat fmt;
    fmt.setFontItalic(enabled);
    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection()) cursor.select(QTextCursor::WordUnderCursor);
    cursor.mergeCharFormat(fmt);
    editor->mergeCurrentCharFormat(fmt);
}

void MainWindow::setUnderline(bool enabled)
{
    if (!editor) return;
    QTextCharFormat fmt;
    fmt.setFontUnderline(enabled);
    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection()) cursor.select(QTextCursor::WordUnderCursor);
    cursor.mergeCharFormat(fmt);
    editor->mergeCurrentCharFormat(fmt);
}

void MainWindow::setStrikethrough(bool enabled)
{
    if (!editor) return;
    QTextCharFormat fmt;
    fmt.setFontStrikeOut(enabled);
    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection()) cursor.select(QTextCursor::WordUnderCursor);
    cursor.mergeCharFormat(fmt);
    editor->mergeCurrentCharFormat(fmt);
}

void MainWindow::syncInlineFormatButtons()
{
    if (!editor) return;
    const QTextCharFormat fmt = editor->currentCharFormat();
    const bool b = fmt.fontWeight() > QFont::Normal;
    const bool i = fmt.fontItalic();
    const bool u = fmt.fontUnderline();
    const bool s = fmt.fontStrikeOut();
    if (toolbar) {
        toolbar->setBoldChecked(b);
        toolbar->setItalicChecked(i);
        toolbar->setUnderlineChecked(u);
        toolbar->setStrikethroughChecked(s);
    }
    if (selBoldBtn)      selBoldBtn->setChecked(b);
    if (selItalicBtn)    selItalicBtn->setChecked(i);
    if (selUnderlineBtn) selUnderlineBtn->setChecked(u);
    if (selStrikeBtn)    selStrikeBtn->setChecked(s);
}

void MainWindow::setFocusMode(bool enabled)
{
    focusModeEnabled = enabled;

    // Persiste a preferência — ao reabrir o app, o modo foco volta como estava.
    QSettings().setValue(QStringLiteral("editor/focusMode"), enabled);

    // Atualiza a cor base do texto em todo o documento (dim ou full).
    applyTextColor();

    if (enabled) {
        connect(editor, &QTextEdit::cursorPositionChanged, this, &MainWindow::updateFocusedBlock);
        connect(editor, &QTextEdit::textChanged, this, &MainWindow::updateFocusedBlock);
        connect(editor, &QTextEdit::selectionChanged, this, &MainWindow::updateFocusedBlock);
        updateFocusedBlock();
    } else {
        disconnect(editor, &QTextEdit::cursorPositionChanged, this, &MainWindow::updateFocusedBlock);
        disconnect(editor, &QTextEdit::textChanged, this, &MainWindow::updateFocusedBlock);
        disconnect(editor, &QTextEdit::selectionChanged, this, &MainWindow::updateFocusedBlock);
        editor->setExtraSelections({});
    }
}

void MainWindow::updateFocusedBlock()
{
    QList<QTextEdit::ExtraSelection> selections;
    QTextBlock block = editor->textCursor().block();
    if (!block.isValid()) {
        editor->setExtraSelections(selections);
        return;
    }

    QColor focused = baseTextColor;
    focused.setAlpha(255);

    // Uma ExtraSelection por fragmento, pulando fragmentos com background
    // (markers). Sem isso, o foreground da ExtraSelection sobrepõe o
    // foreground de contraste do marker e o texto highlighted vira a cor
    // do tema — invisível contra o fundo claro do marker.
    for (auto it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment frag = it.fragment();
        if (!frag.isValid() || frag.length() == 0) continue;
        const QBrush bg = frag.charFormat().background();
        if (bg.style() != Qt::NoBrush && bg.color().alpha() > 0) continue;

        QTextEdit::ExtraSelection sel;
        sel.format.setForeground(focused);
        sel.cursor = QTextCursor(editor->document());
        sel.cursor.setPosition(frag.position());
        sel.cursor.setPosition(frag.position() + frag.length(),
                               QTextCursor::KeepAnchor);
        selections.append(sel);
    }
    editor->setExtraSelections(selections);
}

void MainWindow::onAddImageRequested()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Selecionar imagem"),
        QString(),
        tr("Imagens (*.png *.jpg *.jpeg *.gif *.bmp *.webp)"));
    if (path.isEmpty()) return;

    ImageInsertDialog dialog(path, this);
    if (dialog.exec() != QDialog::Accepted) return;

    const QString fileUrl = QUrl::fromLocalFile(path).toString();
    const int w = dialog.width();

    QTextImageFormat imgFmt;
    imgFmt.setName(fileUrl);
    imgFmt.setWidth(w);

    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();

    // Garantir que estamos numa linha vazia para hospedar a imagem.
    if (!cursor.block().text().isEmpty()) {
        cursor.movePosition(QTextCursor::EndOfBlock);
        cursor.insertBlock();
    }

    if (dialog.alignment() == ImageInsertDialog::Center) {
        QTextBlockFormat bf;
        bf.setAlignment(Qt::AlignHCenter);
        bf.setTextIndent(0);
        bf.setTopMargin(6);
        bf.setBottomMargin(6);
        cursor.setBlockFormat(bf);
        cursor.insertImage(imgFmt);

        cursor.insertBlock();
        QTextBlockFormat normalBf;
        normalBf.setAlignment(Qt::AlignLeft);
        normalBf.setTextIndent(firstLineIndentEnabled ? 30 : 0);
        cursor.setBlockFormat(normalBf);
    } else {
        QTextFrameFormat ff;
        applyFloatFrameStyle(ff, dialog.alignment() == ImageInsertDialog::Left, w);
        cursor.insertFrame(ff);
        cursor.insertImage(imgFmt);
    }

    cursor.endEditBlock();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!projectSaver) {
        QMainWindow::closeEvent(event);
        return;
    }
    // Garante que tudo que está no editor vai pro cache antes de avaliar dirty.
    if (editorHost) editorHost->syncEditorToCache();
    if (!projectSaver->hasDirtyContent() && !projectSaver->isSaving()) {
        QMainWindow::closeEvent(event);
        return;
    }
    // Salva síncrono antes de fechar. Se falhar, oferece sair mesmo assim.
    const bool ok = projectSaver->saveProject();
    if (!ok) {
        const auto choice = QMessageBox::question(this, tr("Salvar projeto"),
            tr("Falha ao salvar o projeto:\n%1\n\nFechar mesmo assim?").arg(projectSaver->lastError()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (choice != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Hover preview do botão Info da LeftBar.
    if (leftBar && watched == leftBar->fixedButton(LeftBar::Info)) {
        const QEvent::Type t = event->type();
        if (t == QEvent::Enter) {
            if (projectInfoHoverCloseTimer) projectInfoHoverCloseTimer->stop();
            // Não abre o hover enquanto o painel de edição está aberto.
            if (!projectInfoPanel || !projectInfoPanel->isVisible()) {
                if (projectInfoHoverOpenTimer) projectInfoHoverOpenTimer->start();
            }
        } else if (t == QEvent::Leave) {
            if (projectInfoHoverOpenTimer) projectInfoHoverOpenTimer->stop();
            if (projectInfoHoverCloseTimer) projectInfoHoverCloseTimer->start();
        }
    }

    if (watched == editor->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(event);
            QTextCursor imageCursor;
            if (findImageAt(me->pos(), imageCursor)) {
                showOverlayForImage(imageCursor);
                return true;
            }
            hideOverlay();
        } else if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent*>(event);
            QString id;
            QRect boundsGlobal;
            if (markerStore && markerAtViewportPos(me->pos(), id, boundsGlobal)) {
                const MarkerStore::Entry e = markerStore->findById(editorHost->activeKey(), id);
                if (!e.id.isEmpty() && !e.comment.trimmed().isEmpty()) {
                    if (markerHoverPopup && id != markerHoverId) {
                        markerHoverId = id;
                        markerHoverPopup->showFor(id, QColor(e.color), e.comment, boundsGlobal);
                    } else if (markerHoverPopup) {
                        markerHoverPopup->cancelClose();
                    }
                }
            } else {
                // Mouse fora de qualquer marker — limpa o id sentinela mesmo
                // que o popup não esteja visível (caso contrário, o id ficaria
                // travado depois de criar um marker e o hover nunca abriria).
                if (!markerHoverId.isEmpty()) markerHoverId.clear();
                if (markerHoverPopup && markerHoverPopup->isVisible()) {
                    markerHoverPopup->scheduleClose();
                }
            }
        } else if (event->type() == QEvent::Leave) {
            if (markerHoverPopup && markerHoverPopup->isVisible()) {
                markerHoverPopup->scheduleClose();
                markerHoverId.clear();
            }
        } else if (event->type() == QEvent::Resize) {
            hideOverlay();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::detectScenesForPending()
{
    if (sceneDetectKey.isEmpty()) return;
    const QStringList parts = sceneDetectKey.split(QLatin1Char(':'));
    if (parts.size() < 3) return;
    const QString chId = parts.at(2);
    const Chapter* ch = projectModel->findChapter(chId);
    if (!ch) return;
    const QString html = docCache->get(sceneDetectKey);
    const QList<Scene> scenes = ProjectModel::buildScenesFromHtml(html, ch->scenes);
    projectModel->updateChapterScenes(chId, scenes);
}

bool MainWindow::findImageAt(const QPoint &viewportPos, QTextCursor &imageCursor) const
{
    auto *layout = editor->document()->documentLayout();
    QTextFrame *root = editor->document()->rootFrame();
    const int scrollY = editor->verticalScrollBar()->value();

    for (QTextBlock block = editor->document()->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            QTextFragment frag = it.fragment();
            if (!frag.isValid() || !frag.charFormat().isImageFormat()) continue;

            QTextCursor probe(editor->document());
            probe.setPosition(frag.position());

            QTextFrame *parentFrame = probe.currentFrame();
            QRect visRect;
            if (parentFrame && parentFrame != root) {
                visRect = layout->frameBoundingRect(parentFrame).toRect();
            } else {
                visRect = layout->blockBoundingRect(block).toRect();
            }
            visRect.translate(0, -scrollY);

            if (visRect.adjusted(-4, -4, 4, 4).contains(viewportPos)) {
                imageCursor = QTextCursor(editor->document());
                imageCursor.setPosition(frag.position());
                imageCursor.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
                return true;
            }
        }
    }
    return false;
}

int MainWindow::detectAlignmentForImage(const QTextCursor &imageCursor) const
{
    QTextFrame *frame = imageCursor.currentFrame();
    QTextFrame *root = editor->document()->rootFrame();
    if (frame && frame != root) {
        const QTextFrameFormat::Position pos = frame->frameFormat().position();
        if (pos == QTextFrameFormat::FloatLeft) return ImageOverlay::Left;
        if (pos == QTextFrameFormat::FloatRight) return ImageOverlay::Right;
    }
    if (imageCursor.blockFormat().alignment() & Qt::AlignHCenter) {
        return ImageOverlay::Center;
    }
    return ImageOverlay::Left;
}

void MainWindow::showOverlayForImage(const QTextCursor &imageCursor)
{
    selectedImageCursor = imageCursor;

    QTextImageFormat fmt = imageCursor.charFormat().toImageFormat();
    const int imgW = static_cast<int>(fmt.width() > 0 ? fmt.width() : 320);
    const ImageOverlay::Alignment align =
        static_cast<ImageOverlay::Alignment>(detectAlignmentForImage(imageCursor));

    imageOverlay->setCurrentWidth(imgW);
    imageOverlay->setCurrentAlignment(align);

    auto *layout = editor->document()->documentLayout();
    QTextFrame *root = editor->document()->rootFrame();
    QTextFrame *parentFrame = imageCursor.currentFrame();

    QRect visRect;
    if (parentFrame && parentFrame != root) {
        visRect = layout->frameBoundingRect(parentFrame).toRect();
    } else {
        visRect = layout->blockBoundingRect(imageCursor.block()).toRect();
    }
    visRect.translate(0, -editor->verticalScrollBar()->value());

    imageOverlay->adjustSize();
    int x = visRect.center().x() - imageOverlay->width() / 2;
    int y = visRect.top() + 4;

    if (x + imageOverlay->width() > editor->viewport()->width() - 4) {
        x = editor->viewport()->width() - imageOverlay->width() - 4;
    }
    if (x < 4) x = 4;
    if (y < 4) y = 4;

    imageOverlay->move(x, y);
    imageOverlay->show();
    imageOverlay->raise();
}

void MainWindow::hideOverlay()
{
    if (imageOverlay) imageOverlay->hide();
}

void MainWindow::changeSelectedImageWidth(int delta)
{
    if (selectedImageCursor.isNull() || !selectedImageCursor.charFormat().isImageFormat()) return;
    QTextImageFormat fmt = selectedImageCursor.charFormat().toImageFormat();
    int newWidth = static_cast<int>(fmt.width() > 0 ? fmt.width() : 320) + delta;
    newWidth = qBound(60, newWidth, 1000);
    fmt.setWidth(newWidth);
    selectedImageCursor.setCharFormat(fmt);

    // Se a imagem está num frame float, manter a largura do frame em sincronia.
    QTextFrame *frame = selectedImageCursor.currentFrame();
    QTextFrame *root = editor->document()->rootFrame();
    if (frame && frame != root) {
        QTextFrameFormat ff = frame->frameFormat();
        const QTextFrameFormat::Position pos = ff.position();
        if (pos == QTextFrameFormat::FloatLeft || pos == QTextFrameFormat::FloatRight) {
            ff.setWidth(QTextLength(QTextLength::FixedLength, newWidth));
            frame->setFrameFormat(ff);
            const int frameStart = frame->firstPosition();
            const int docEnd = editor->document()->characterCount();
            editor->document()->markContentsDirty(frameStart, qMax(1, docEnd - frameStart));
        }
    }

    imageOverlay->setCurrentWidth(newWidth);
    showOverlayForImage(selectedImageCursor);
}

void MainWindow::changeSelectedImageAlignment(int alignment)
{
    if (selectedImageCursor.isNull() || !selectedImageCursor.charFormat().isImageFormat()) return;

    QTextImageFormat fmt = selectedImageCursor.charFormat().toImageFormat();
    const QString fileUrl = fmt.name();
    const int w = static_cast<int>(fmt.width() > 0 ? fmt.width() : 320);

    QTextFrame *rootFrame = editor->document()->rootFrame();
    QTextFrame *currentFrame = selectedImageCursor.currentFrame();
    const bool inFloatFrame = (currentFrame && currentFrame != rootFrame &&
        (currentFrame->frameFormat().position() == QTextFrameFormat::FloatLeft ||
         currentFrame->frameFormat().position() == QTextFrameFormat::FloatRight));

    // Caminho rápido: Left <-> Right só altera o atributo do frame existente.
    // Operação atômica, sem mexer no fluxo do texto — nada de creep vertical.
    if (inFloatFrame && alignment != ImageOverlay::Center) {
        QTextFrameFormat ff = currentFrame->frameFormat();
        applyFloatFrameStyle(ff, alignment == ImageOverlay::Left, w);
        currentFrame->setFrameFormat(ff);
        // Forçar relayout do texto ao redor — Qt não reflowa sozinho após mudar o frame.
        const int frameStart = currentFrame->firstPosition();
        const int docEnd = editor->document()->characterCount();
        editor->document()->markContentsDirty(frameStart, qMax(1, docEnd - frameStart));
        imageOverlay->setCurrentAlignment(static_cast<ImageOverlay::Alignment>(alignment));
        showOverlayForImage(selectedImageCursor);
        return;
    }

    // Transição Center <-> Float: precisa remover e reconstruir.
    QTextCursor c(editor->document());
    c.beginEditBlock();

    if (inFloatFrame) {
        // Remover o frame inteiro, incluindo as marcas de fronteira.
        c.setPosition(currentFrame->firstPosition() - 1);
        c.setPosition(currentFrame->lastPosition() + 1, QTextCursor::KeepAnchor);
        c.removeSelectedText();
    } else {
        // Imagem em bloco solitário (modo center). Limpa o bloco preservando-o.
        QTextBlock block = selectedImageCursor.block();
        c.setPosition(block.position());
        c.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
        c.removeSelectedText();
    }

    QTextImageFormat imgFmt;
    imgFmt.setName(fileUrl);
    imgFmt.setWidth(w);

    int newImagePos = -1;

    if (alignment == ImageOverlay::Center) {
        QTextBlockFormat bf;
        bf.setAlignment(Qt::AlignHCenter);
        bf.setTextIndent(0);
        bf.setTopMargin(6);
        bf.setBottomMargin(6);
        c.setBlockFormat(bf);
        c.insertImage(imgFmt);
        newImagePos = c.position() - 1;
    } else {
        QTextFrameFormat ff;
        applyFloatFrameStyle(ff, alignment == ImageOverlay::Left, w);
        c.insertFrame(ff);
        c.insertImage(imgFmt);
        newImagePos = c.position() - 1;
    }

    c.endEditBlock();

    if (newImagePos >= 0) {
        QTextCursor found(editor->document());
        found.setPosition(newImagePos);
        found.setPosition(newImagePos + 1, QTextCursor::KeepAnchor);
        if (found.charFormat().isImageFormat()) {
            selectedImageCursor = found;
        }
    }
    imageOverlay->setCurrentAlignment(static_cast<ImageOverlay::Alignment>(alignment));
    showOverlayForImage(selectedImageCursor);
}

void MainWindow::applyProjectRoot(const QString& root)
{
    projectRoot = root;
    QString err;
    ProjectStorage::ensureProjectDirs(root, &err);
    if (editorHost) editorHost->setProjectRoot(root);
    if (projectSaver) projectSaver->setProjectRoot(root);
    if (wordCounter) wordCounter->setProjectRoot(root);
    if (refMenuPanel) refMenuPanel->setProjectRoot(root);
    if (elementsStore) elementsStore->setProjectRoot(root);
    if (spellChecker) spellChecker->setProjectRoot(root);
    if (markerStore) {
        markerStore->setProjectRoot(root);
        markerStore->load();
    }
    if (glossaryStore) {
        glossaryStore->setProjectRoot(root);
        glossaryStore->load();
        if (spellChecker) spellChecker->setGlossaryWords(glossaryStore->terms());
    }
    // Atualiza o título com o nome da pasta do projeto.
    const QString name = QDir(root).dirName();
    baseWindowTitle = name.isEmpty() ? tr("Mira Writing") : tr("Mira Writing — %1").arg(name);
    setWindowTitle(baseWindowTitle);
}

bool MainWindow::confirmDiscardOrSave()
{
    if (!projectSaver) return true;
    if (editorHost) editorHost->syncEditorToCache();
    if (!projectSaver->hasDirtyContent()) return true;
    const auto choice = QMessageBox::question(this, tr("Alterações não salvas"),
        tr("Há alterações no projeto atual. Salvar antes de continuar?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Cancel) return false;
    if (choice == QMessageBox::Save) {
        if (!projectSaver->saveProject()) return false;
    }
    return true;
}

bool MainWindow::loadProjectFrom(const QString& root, QString* errorOut)
{
    if (root.isEmpty()) {
        if (errorOut) *errorOut = tr("Caminho do projeto vazio.");
        return false;
    }
    if (!QDir(root).exists()) {
        if (errorOut) *errorOut = tr("Pasta do projeto não existe: %1").arg(root);
        return false;
    }
    QString ensureErr;
    if (!ProjectStorage::ensureProjectDirs(root, &ensureErr)) {
        if (errorOut) *errorOut = ensureErr;
        return false;
    }

    // Limpa estado em memória.
    if (editorHost) editorHost->disable();
    if (docCache) docCache->clear();
    if (projectModel) projectModel->clear();
    if (elementsStore) elementsStore->clear();

    applyProjectRoot(root);

    // Carrega index, se existir.
    const QString indexPath = ProjectStorage::indexPath(root);
    if (QFile::exists(indexPath)) {
        bool ok = false;
        QString readErr;
        const QJsonObject obj = ProjectStorage::readIndex(root, &ok, &readErr);
        if (!ok) {
            if (errorOut) *errorOut = readErr;
            return false;
        }
        projectModel->loadFromJson(obj);
    }

    if (elementsStore) elementsStore->load();

    rememberLastProject(root);
    restoreLastDocFor(root);
    return true;
}

void MainWindow::rememberLastProject(const QString& root)
{
    QSettings settings;
    settings.setValue(QStringLiteral("lastProjectPath"), root);
}

QString MainWindow::loadLastProjectPath() const
{
    QSettings settings;
    return settings.value(QStringLiteral("lastProjectPath")).toString();
}

static QString lastDocGroupFor(const QString& root)
{
    const QByteArray hash = QCryptographicHash::hash(
        QDir::cleanPath(root).toUtf8(), QCryptographicHash::Md5).toHex();
    return QStringLiteral("lastDoc/") + QString::fromLatin1(hash);
}

void MainWindow::rememberLastDocFor(const QString& root)
{
    if (!editorHost || root.isEmpty()) return;
    const EditorHost::ViewMode vm = editorHost->viewMode();
    QSettings s;
    s.beginGroup(lastDocGroupFor(root));
    s.setValue(QStringLiteral("type"), int(vm.type));
    s.setValue(QStringLiteral("manuscriptId"), vm.manuscriptId);
    s.setValue(QStringLiteral("chapterId"), vm.chapterId);
    s.setValue(QStringLiteral("sceneIndex"), vm.sceneIndex);
    s.setValue(QStringLiteral("itemId"), vm.itemId);
    s.endGroup();
}

void MainWindow::restoreLastDocFor(const QString& root)
{
    if (!editorHost || root.isEmpty()) return;
    QSettings s;
    s.beginGroup(lastDocGroupFor(root));
    if (!s.contains(QStringLiteral("type"))) {
        s.endGroup();
        return;
    }
    EditorHost::ViewMode vm;
    vm.type = static_cast<EditorHost::ViewModeType>(
        s.value(QStringLiteral("type"), int(EditorHost::Disabled)).toInt());
    vm.manuscriptId = s.value(QStringLiteral("manuscriptId")).toString();
    vm.chapterId    = s.value(QStringLiteral("chapterId")).toString();
    vm.sceneIndex   = s.value(QStringLiteral("sceneIndex"), -1).toInt();
    vm.itemId       = s.value(QStringLiteral("itemId")).toString();
    s.endGroup();
    if (vm.type == EditorHost::Disabled) return;
    // EditorHost valida IDs e cai pra Disabled se não conseguir resolver — ok.
    editorHost->setViewMode(vm);
}

void MainWindow::onNewProjectRequested()
{
    if (!confirmDiscardOrSave()) return;

    const QString startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString chosen = QFileDialog::getExistingDirectory(this,
        tr("Escolher pasta para o novo projeto"),
        startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (chosen.isEmpty()) return;

    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Novo projeto"),
        tr("Nome do projeto:"), QLineEdit::Normal,
        tr("Meu Projeto"), &ok).trimmed();
    if (!ok || name.isEmpty()) return;

    const QString fullPath = QDir(chosen).absoluteFilePath(name);
    if (QDir(fullPath).exists() && !QDir(fullPath).isEmpty()) {
        const auto answer = QMessageBox::question(this, tr("Pasta já existe"),
            tr("A pasta '%1' já existe e não está vazia. Usar mesmo assim?\n"
               "Conteúdo existente pode ser sobrescrito.").arg(fullPath),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
    }

    QString err;
    if (!ProjectStorage::ensureProjectDirs(fullPath, &err)) {
        QMessageBox::warning(this, tr("Erro"), tr("Falha ao criar o projeto:\n%1").arg(err));
        return;
    }

    // Limpa o estado, monta projeto vazio com nome.
    if (editorHost) editorHost->disable();
    if (docCache) docCache->clear();
    if (projectModel) projectModel->clear();
    projectModel->setProjectName(name);
    applyProjectRoot(fullPath);

    // Grava o index inicial.
    if (projectSaver && !projectSaver->saveProject()) {
        QMessageBox::warning(this, tr("Erro"),
            tr("Projeto criado, mas falha ao salvar índice inicial:\n%1").arg(projectSaver->lastError()));
    }
    rememberLastProject(fullPath);
}

void MainWindow::positionWordCountPanel()
{
    if (!wordCountPanel || !wordCountPanel->parentWidget()) return;
    QWidget* parent = wordCountPanel->parentWidget();
    wordCountPanel->adjustSize();
    const int leftBarOffset = LeftBar::barWidth() + 10 + 10; // largura LeftBar + spacing do layout + margem
    const int x = leftBarOffset;
    const int y = parent->height() - wordCountPanel->height() - 10;
    wordCountPanel->move(x, y);
}

void MainWindow::positionSidePanels()
{
    // Posiciona o DrawerListPanel e o ManuscriptPanel como overlays flutuantes
    // logo à direita da LeftBar — não entram no layout pra não empurrar o editor.
    if (!leftBar) return;
    QWidget* parent = drawerListPanel ? drawerListPanel->parentWidget() : nullptr;
    if (!parent) return;
    const int margin = 10;
    const int x = margin + leftBar->width() + margin;
    const int y = margin;
    const int maxH = qMax(0, parent->height() - margin * 2);

    // DrawerListPanel: respeita altura escolhida pelo usuário (se houver),
    // só clampa pra caber. Senão, expande full-height (comportamento antigo).
    if (drawerListPanel) {
        drawerListPanel->move(x, y);
        int targetH;
        if (drawerListPanel->heightIsUserSet()) {
            // Usa desiredHeight, não height() — pode ter sido encolhido por show().
            const int desired = drawerListPanel->desiredHeight();
            targetH = qMin(desired > 0 ? desired : drawerListPanel->height(), maxH);
        } else {
            targetH = maxH;
        }
        drawerListPanel->resize(drawerListPanel->width(), targetH);
        if (drawerListPanel->isVisible()) drawerListPanel->raise();
    }

    // ManuscriptPanel: continua sempre full-height por enquanto.
    if (manuscriptPanel) {
        manuscriptPanel->move(x, y);
        manuscriptPanel->resize(manuscriptPanel->width(), maxH);
        if (manuscriptPanel->isVisible()) manuscriptPanel->raise();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    resizeEditorColumnToViewport();
    positionWordCountPanel();
    positionSidePanels();
    if (toolbar && editor) {
        const QPoint editorCenterGlobal =
            editor->mapToGlobal(QPoint(editor->width() / 2, 0));
        const int anchorX = toolbar->mapFromGlobal(editorCenterGlobal).x();
        toolbar->setTitleAnchorX(anchorX);
    }
    // RefMenuPanel: drag/resize livres. O showEvent dele já mantém dentro do pai
    // quando reaberto, e a geometria persiste em QSettings.
}

void MainWindow::onOpenProjectRequested()
{
    if (!confirmDiscardOrSave()) return;

    const QString startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString chosen = QFileDialog::getExistingDirectory(this,
        tr("Abrir projeto"),
        startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (chosen.isEmpty()) return;

    QString err;
    if (!loadProjectFrom(chosen, &err)) {
        QMessageBox::warning(this, tr("Erro ao abrir"),
            tr("Não foi possível abrir o projeto:\n%1").arg(err));
    }
}

void MainWindow::onSettingsRequested()
{
    if (!settingsPanel) {
        settingsPanel = new SettingsPanel(this);
        settingsPanel->setAvailableSpellLanguages(SpellChecker::availableLanguages());

        connect(settingsPanel, &SettingsPanel::spellEnabledChanged, this, [this](bool enabled) {
            if (!projectModel || !spellChecker) return;
            if (enabled) {
                // Liga com o idioma mostrado no combo (ou o último salvo, se vazio).
                QString code = settingsPanel->spellLanguage();
                if (code.isEmpty()) {
                    const auto langs = SpellChecker::availableLanguages();
                    if (!langs.isEmpty()) code = langs.first().first;
                }
                projectModel->setSpellLanguage(code);
                spellChecker->setLanguage(code);
                settingsPanel->setSpellLanguage(code);
            } else {
                projectModel->setSpellLanguage(QString());
                spellChecker->setLanguage(QString());
            }
        });
        connect(settingsPanel, &SettingsPanel::spellLanguageChanged, this, [this](const QString& code) {
            if (!projectModel || !spellChecker) return;
            projectModel->setSpellLanguage(code);
            spellChecker->setLanguage(code);
        });
    }

    // Atualiza a UI do painel com o estado atual antes de mostrar.
    const QString lang = projectModel ? projectModel->spellLanguage() : QString();
    settingsPanel->setSpellEnabled(!lang.isEmpty());
    if (!lang.isEmpty()) settingsPanel->setSpellLanguage(lang);

    settingsPanel->show();
    settingsPanel->raise();
    settingsPanel->activateWindow();
}

void MainWindow::onThemePanelRequested()
{
    if (!themesPanel) {
        themesPanel = new ThemesPanel(this);
    }
    themesPanel->show();
    themesPanel->raise();
    themesPanel->activateWindow();
}

void MainWindow::onThemeChanged()
{
    // Tema cuida só de cores/aparência. Largura/margens são do EditorLayout.
    // Stylesheet global (QMainWindow, QMenu, QScrollBar, TopToolbar, popups
    // de imagem/fonte) é derivada do tema e precisa ser reaplicada.
    if (auto* a = qobject_cast<QApplication*>(QApplication::instance())) {
        a->setStyleSheet(Theme::globalStyleSheet());
    }

    if (editorContainer) {
        editorContainer->setStyleSheet(
            QStringLiteral("#editorContainer { background: %1; }").arg(Theme::appBackground()));
    }
    if (toolbarHolder) {
        toolbarHolder->setStyleSheet(
            QStringLiteral("#topToolbarHolder { background: %1; }").arg(Theme::appBackground()));
    }

    applyEditorStyle();
    applyPageShadow();

    // Reposiciona painéis flutuantes.
    positionSidePanels();
    positionWordCountPanel();
}

void MainWindow::onEditorLayoutChanged()
{
    applyEditorLayout();
    applyPageShadow();
    positionSidePanels();
    positionWordCountPanel();
}

void MainWindow::applyEditorLayout()
{
    const int pw = EditorLayout::pageWidth();
    const int hm = EditorLayout::horizontalMargin();
    const int vm = EditorLayout::verticalMargin();

    if (editor) {
        editor->setFixedWidth(pw);
        // Margens internas entre a borda da "página" e o texto.
        editor->setPageMargins(hm, vm, hm, vm);
    }
    if (editorColumn) {
        // editorColumn = página + scrollbar externa (fora da folha).
        const int scrollExtra = externalScrollBar
            ? (externalScrollBar->sizeHint().width() + 6 /*spacing*/)
            : 0;
        editorColumn->setFixedWidth(pw + scrollExtra);
    }

    // editorColumn vive dentro de uma QScrollArea (widgetResizable=false), então
    // sua altura precisa ser ajustada manualmente ao tamanho da viewport.
    resizeEditorColumnToViewport();

    // Reposiciona o título da toolbar — ele segue o centro horizontal do editor.
    if (toolbar && editor) {
        const QPoint editorCenterGlobal =
            editor->mapToGlobal(QPoint(editor->width() / 2, 0));
        const int anchorX = toolbar->mapFromGlobal(editorCenterGlobal).x();
        toolbar->setTitleAnchorX(anchorX);
    }
}

void MainWindow::resizeEditorColumnToViewport()
{
    if (!editorScroll || !editorColumn) return;
    const int vpH = editorScroll->viewport()->height();
    const int vpW = editorScroll->viewport()->width();
    const int colW = editorColumn->width();
    // Se a página é menor que a viewport, mantém ela centralizada (a viewport
    // tem hScrollBar oculto). Se é maior, deixa o scroll horizontal aparecer.
    editorColumn->resize(qMax(colW, vpW > colW ? colW : colW), vpH);
}

void MainWindow::applyPageShadow()
{
    if (!editorColumn) return;
    if (Theme::pageShadowEnabled()) {
        auto* effect = qobject_cast<QGraphicsDropShadowEffect*>(editorColumn->graphicsEffect());
        if (!effect) {
            effect = new QGraphicsDropShadowEffect(editorColumn);
            editorColumn->setGraphicsEffect(effect);
        }
        effect->setBlurRadius(Theme::pageShadowRadius());
        effect->setOffset(0, Theme::pageShadowOffset());
        effect->setColor(parseColor(Theme::pageShadowColor()));
    } else {
        editorColumn->setGraphicsEffect(nullptr);
    }
}

void MainWindow::applySpellLanguageFromModel()
{
    if (!projectModel || !spellChecker) return;
    const QString code = projectModel->spellLanguage();
    spellChecker->setLanguage(code);
    if (settingsPanel) {
        settingsPanel->setSpellEnabled(!code.isEmpty());
        if (!code.isEmpty()) settingsPanel->setSpellLanguage(code);
    }
}

void MainWindow::openMarkerPickerForSelection(bool withComment)
{
    if (!editor || !markerPickPopup || !editor->textCursor().hasSelection()) return;
    if (markerHoverPopup) markerHoverPopup->hide();
    markerEditId.clear();

    // Captura o range AGORA — o popup vai ativar e a seleção visual do editor
    // perde efeito quando outro top-level rouba foco. apply usa essas posições.
    QTextCursor cur = editor->textCursor();
    markerPendingStart = cur.selectionStart();
    markerPendingEnd = cur.selectionEnd();

    // Cor inicial: do char format atual da seleção, se já tem marker;
    // senão amarelo padrão. Marker é background (igual marca-texto).
    QColor seed = cur.charFormat().background().color();
    if (!seed.isValid() || seed.alpha() == 0) {
        seed = QColor(QStringLiteral("#FFD54F"));
    }
    markerPickPopup->setMode(withComment ? MarkerPickPopup::WithComment
                                         : MarkerPickPopup::ColorOnly);
    markerPickPopup->setColor(seed);
    markerPickPopup->setComment(QString());

    // Anchor: retângulo da seleção em coords globais.
    QTextCursor a(editor->document()); a.setPosition(markerPendingStart);
    QTextCursor b(editor->document()); b.setPosition(markerPendingEnd);
    QRect ra = editor->cursorRect(a);
    QRect rb = editor->cursorRect(b);
    QRect bounds = ra.united(rb);
    QPoint topLeft = editor->viewport()->mapToGlobal(bounds.topLeft());
    QPoint bottomRight = editor->viewport()->mapToGlobal(bounds.bottomRight());
    markerPickPopup->showAbove(QRect(topLeft, bottomRight));
}

void MainWindow::openMarkerPickerForEdit(const QString& markerId)
{
    if (!markerStore || !editor || !editorHost || !markerPickPopup) return;
    const MarkerStore::Entry e = markerStore->findById(editorHost->activeKey(), markerId);
    if (e.id.isEmpty()) return;

    markerEditId = markerId;
    markerPickPopup->setMode(MarkerPickPopup::WithComment);
    markerPickPopup->setColor(QColor(e.color));
    markerPickPopup->setComment(e.comment);

    // Anchor no range do marker.
    QTextCursor a(editor->document()); a.setPosition(e.start);
    QTextCursor b(editor->document()); b.setPosition(e.end);
    QRect ra = editor->cursorRect(a);
    QRect rb = editor->cursorRect(b);
    QRect bounds = ra.united(rb);
    QPoint topLeft = editor->viewport()->mapToGlobal(bounds.topLeft());
    QPoint bottomRight = editor->viewport()->mapToGlobal(bounds.bottomRight());
    markerPickPopup->showAbove(QRect(topLeft, bottomRight));
}

void MainWindow::applyMarkerFromPicker(const QColor& color, const QString& comment)
{
    if (!markerStore || !editor || !editorHost) return;
    const QString key = editorHost->activeKey();
    if (key.isEmpty()) return;

    QString appliedId;

    if (!markerEditId.isEmpty()) {
        markerStore->updateMarker(key, editor->document(), markerEditId, color, comment);
        appliedId = markerEditId;
        markerEditId.clear();
    } else {
        // Usa range capturado em openMarkerPickerForSelection — não confia na
        // seleção atual do editor (popup ativo pode tê-la perdido).
        int s = markerPendingStart;
        int e = markerPendingEnd;
        markerPendingStart = -1;
        markerPendingEnd = -1;
        if (s < 0 || e <= s) return;
        const int docLen = editor->document()->characterCount();
        s = qBound(0, s, docLen);
        e = qBound(0, e, docLen);
        if (e <= s) return;
        // Usa um cursor "ativo" do editor e devolve com setTextCursor — força
        // o QTextEdit a invalidar a region e repintar com o novo charFormat.
        // QTextCursor(doc) "solto" modifica o documento mas o editor pode não
        // perceber pra repaint imediato.
        QTextCursor cur = editor->textCursor();
        cur.setPosition(s);
        cur.setPosition(e, QTextCursor::KeepAnchor);
        appliedId = markerStore->applyMarkerToSelection(key, cur, color, comment);
        editor->setTextCursor(cur);
        editor->viewport()->update();
    }

    // Esconde o hover popup e marca o id atual — assim ele não reaparece
    // grudado em cima do trecho recém-marcado. Só abre quando o user sai e
    // volta com o mouse.
    if (markerHoverPopup) {
        markerHoverPopup->hide();
    }
    if (!appliedId.isEmpty()) {
        markerHoverId = appliedId;
    } else {
        markerHoverId.clear();
    }
}

bool MainWindow::markerAtViewportPos(const QPoint& viewportPos, QString& outId,
                                     QRect& outBoundsGlobal) const
{
    if (!editor) return false;
    QTextCursor probe = editor->cursorForPosition(viewportPos);
    const int pos = probe.position();
    QTextBlock block = editor->document()->findBlock(pos);
    if (!block.isValid()) return false;

    for (auto it = block.begin(); !it.atEnd(); ++it) {
        QTextFragment frag = it.fragment();
        if (!frag.isValid()) continue;
        if (pos < frag.position() || pos >= frag.position() + frag.length()) continue;
        const QString id = frag.charFormat().property(MarkerStore::MarkerIdProperty).toString();
        if (id.isEmpty()) return false;

        // Estende pra todos os fragments contíguos com mesmo id no bloco.
        int rangeStart = frag.position();
        int rangeEnd = frag.position() + frag.length();
        for (auto jt = block.begin(); !jt.atEnd(); ++jt) {
            QTextFragment f = jt.fragment();
            if (!f.isValid()) continue;
            if (f.charFormat().property(MarkerStore::MarkerIdProperty).toString() != id) continue;
            rangeStart = qMin(rangeStart, f.position());
            rangeEnd = qMax(rangeEnd, f.position() + f.length());
        }

        QTextCursor a(editor->document()); a.setPosition(rangeStart);
        QTextCursor b(editor->document()); b.setPosition(rangeEnd);
        QRect ra = editor->cursorRect(a);
        QRect rb = editor->cursorRect(b);
        QRect bounds = ra.united(rb);
        QPoint topLeft = editor->viewport()->mapToGlobal(bounds.topLeft());
        QPoint bottomRight = editor->viewport()->mapToGlobal(bounds.bottomRight());
        outBoundsGlobal = QRect(topLeft, bottomRight);
        outId = id;
        return true;
    }
    return false;
}

void MainWindow::closeBondPopup()
{
    if (bondPopup) {
        bondPopup->hide();
        bondPopup->deleteLater();
        bondPopup = nullptr;
    }
}

void MainWindow::closeBondViewPanel()
{
    if (bondViewPanel) {
        bondViewPanel->hide();
        bondViewPanel->deleteLater();
        bondViewPanel = nullptr;
    }
}

namespace {
// Posiciona o popup à direita da gaveta (drawerListPanel), respeitando o
// container central. Espelha o getDrawerSpawn() do Mira 1.
QPoint computeBondSpawnPos(QWidget* container, QWidget* anchor, int popupW, int popupH) {
    if (!container) return QPoint(80, 80);
    int x, y;
    if (anchor) {
        const QPoint right = anchor->mapTo(container, QPoint(anchor->width() + 16, 20));
        x = right.x();
        y = right.y();
    } else {
        x = 80;
        y = 80;
    }
    const int maxX = container->width() - popupW - 10;
    const int maxY = container->height() - popupH - 10;
    if (x > maxX) x = maxX;
    if (y > maxY) y = maxY;
    if (x < 10) x = 10;
    if (y < 10) y = 10;
    return QPoint(x, y);
}
}

void MainWindow::openBondCreatePopup(const QString& drawerKey, const QString& fromItemId,
                                     const QString& toItemId, const QPoint& /*spawnGlobal*/)
{
    if (!projectModel) return;
    const DrawerItem* fromIt = projectModel->findDrawerItem(fromItemId);
    const DrawerItem* toIt = projectModel->findDrawerItem(toItemId);
    if (!fromIt || !toIt) return;

    closeBondPopup();
    closeBondViewPanel();

    CharacterBond seed;
    seed.color = QStringLiteral("#4a9eff");
    bondPopup = new BondPopup(editorContainer, BondPopup::Create,
                              fromIt->title, toIt->title, seed);
    bondPopup->setAttribute(Qt::WA_DeleteOnClose, false);
    const QPoint p = computeBondSpawnPos(editorContainer, drawerListPanel,
                                         bondPopup->width(), bondPopup->minimumHeight());
    bondPopup->move(p);
    bondPopup->show();
    bondPopup->raise();

    connect(bondPopup, &BondPopup::closeRequested, this, &MainWindow::closeBondPopup);
    connect(bondPopup, &BondPopup::confirmed, this,
            [this, drawerKey, fromItemId, toItemId](const QString& type, const QString& desc, const QString& color) {
        projectModel->addCharacterBond(drawerKey, fromItemId, toItemId, type, desc, color);
        closeBondPopup();
    });
}

void MainWindow::openBondViewPanel(const QString& drawerKey, const QString& bondId,
                                   const QPoint& /*spawnGlobal*/)
{
    if (!projectModel) return;
    const CharacterBond* bond = projectModel->findCharacterBond(bondId);
    if (!bond) return;
    const DrawerItem* fromIt = projectModel->findDrawerItem(bond->fromItemId);
    const DrawerItem* toIt = projectModel->findDrawerItem(bond->toItemId);
    if (!fromIt || !toIt) return;

    closeBondViewPanel();
    closeBondPopup();

    bondViewPanel = new BondViewPanel(editorContainer, *bond, fromIt->title, toIt->title);
    const QPoint p = computeBondSpawnPos(editorContainer, drawerListPanel,
                                         bondViewPanel->width(), bondViewPanel->minimumHeight());
    bondViewPanel->move(p);
    bondViewPanel->show();
    bondViewPanel->raise();

    connect(bondViewPanel, &BondViewPanel::closeRequested, this, &MainWindow::closeBondViewPanel);
    connect(bondViewPanel, &BondViewPanel::editRequested, this,
            [this, drawerKey, bondId]() {
        const QPoint dummy(0, 0);
        openBondEditPopup(drawerKey, bondId, dummy);
    });
    connect(bondViewPanel, &BondViewPanel::deleteRequested, this,
            [this, bondId]() {
        const auto ret = QMessageBox::question(this, tr("Excluir vínculo"),
            tr("Excluir este vínculo? Esta ação não pode ser desfeita."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
        projectModel->removeCharacterBond(bondId);
        closeBondViewPanel();
    });
    connect(bondViewPanel, &BondViewPanel::createDocRequested, this,
            [this, drawerKey, bondId]() {
        createDocFromBond(drawerKey, bondId);
        closeBondViewPanel();
    });
}

void MainWindow::openBondEditPopup(const QString& drawerKey, const QString& bondId,
                                   const QPoint& /*spawnGlobal*/)
{
    if (!projectModel) return;
    const CharacterBond* bond = projectModel->findCharacterBond(bondId);
    if (!bond) return;
    const DrawerItem* fromIt = projectModel->findDrawerItem(bond->fromItemId);
    const DrawerItem* toIt = projectModel->findDrawerItem(bond->toItemId);
    if (!fromIt || !toIt) return;

    closeBondPopup();
    closeBondViewPanel();

    bondPopup = new BondPopup(editorContainer, BondPopup::Edit,
                              fromIt->title, toIt->title, *bond);
    const QPoint p = computeBondSpawnPos(editorContainer, drawerListPanel,
                                         bondPopup->width(), bondPopup->minimumHeight());
    bondPopup->move(p);
    bondPopup->show();
    bondPopup->raise();

    connect(bondPopup, &BondPopup::closeRequested, this, &MainWindow::closeBondPopup);
    connect(bondPopup, &BondPopup::confirmed, this,
            [this, bondId](const QString& type, const QString& desc, const QString& color) {
        projectModel->updateCharacterBond(bondId, type, desc, color);
        closeBondPopup();
    });
    connect(bondPopup, &BondPopup::deleteRequested, this,
            [this, bondId]() {
        const auto ret = QMessageBox::question(this, tr("Excluir vínculo"),
            tr("Excluir este vínculo? Esta ação não pode ser desfeita."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
        projectModel->removeCharacterBond(bondId);
        closeBondPopup();
    });
    Q_UNUSED(drawerKey);
}

void MainWindow::createDocFromBond(const QString& drawerKey, const QString& bondId)
{
    if (!projectModel) return;
    const CharacterBond* bond = projectModel->findCharacterBond(bondId);
    if (!bond) return;
    const DrawerItem* fromIt = projectModel->findDrawerItem(bond->fromItemId);
    const DrawerItem* toIt = projectModel->findDrawerItem(bond->toItemId);
    if (!fromIt || !toIt) return;

    const QString suggested = QStringLiteral("%1 ↔ %2").arg(fromIt->title, toIt->title);

    // Dialog próprio: campo de nome + combo de gaveta destino.
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Documento do vínculo"));
    dlg.setModal(true);
    dlg.setMinimumWidth(380);

    auto* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(16, 16, 16, 12);
    root->setSpacing(10);

    auto* nameLab = new QLabel(tr("Nome do documento:"), &dlg);
    root->addWidget(nameLab);
    auto* nameEdit = new QLineEdit(suggested, &dlg);
    nameEdit->selectAll();
    root->addWidget(nameEdit);

    auto* drawerLab = new QLabel(tr("Gaveta de destino:"), &dlg);
    root->addWidget(drawerLab);
    auto* drawerCombo = new QComboBox(&dlg);
    int defaultIdx = 0;
    int i = 0;
    for (const auto& d : projectModel->drawers()) {
        const QString label = d.title.isEmpty() ? tr("(sem nome)") : d.title;
        drawerCombo->addItem(label, d.key);
        if (d.key == drawerKey) defaultIdx = i;
        ++i;
    }
    drawerCombo->setCurrentIndex(defaultIdx);
    root->addWidget(drawerCombo);

    root->addStretch();

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* cancel = new QPushButton(tr("Cancelar"), &dlg);
    auto* ok = new QPushButton(tr("Criar"), &dlg);
    ok->setDefault(true);
    btnRow->addWidget(cancel);
    btnRow->addWidget(ok);
    root->addLayout(btnRow);

    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted) return;
    const QString title = nameEdit->text().trimmed();
    if (title.isEmpty()) return;
    const QString destKey = drawerCombo->currentData().toString();
    if (destKey.isEmpty()) return;

    QString html = QStringLiteral("<h2>%1</h2>").arg(title.toHtmlEscaped());
    if (!bond->type.isEmpty()) {
        html += QStringLiteral("<p><em>%1</em></p>")
            .arg(QStringLiteral("%1 — %2 de %3").arg(bond->type, fromIt->title, toIt->title).toHtmlEscaped());
    }
    if (!bond->description.isEmpty()) {
        const QStringList paras = bond->description.split(QChar('\n'), Qt::SkipEmptyParts);
        for (const auto& p : paras) {
            html += QStringLiteral("<p>%1</p>").arg(p.toHtmlEscaped());
        }
    } else {
        html += QStringLiteral("<p></p>");
    }

    DrawerItem it;
    it.id = ProjectModel::uid();
    it.title = title;
    it.folderId = QString();
    it.hasInlineHtml = true;
    it.html = html;
    projectModel->addDrawerItem(destKey, it);
}

void MainWindow::createDocFromSelection()
{
    if (!editor || !projectModel) return;
    QTextCursor cur = editor->textCursor();
    if (!cur.hasSelection()) return;

    // Texto bruto: Qt usa U+2029 como separador de parágrafo no selectedText().
    QString raw = cur.selectedText();
    raw.replace(QChar(0x2029), QChar('\n'));
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty()) return;

    if (projectModel->drawers().isEmpty()) {
        QMessageBox::warning(this, tr("Criar documento"),
            tr("Crie uma gaveta antes de usar este recurso."));
        return;
    }

    // Sugere nome a partir das primeiras palavras (limite ~48 chars, máx 8 palavras).
    auto suggestNameFrom = [](const QString& text) -> QString {
        QString flat = text;
        flat.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
        flat = flat.trimmed();
        const QStringList words = flat.split(QChar(' '), Qt::SkipEmptyParts);
        QStringList picked;
        int total = 0;
        for (const QString& w : words) {
            if (picked.size() >= 8) break;
            if (total + w.size() + (picked.isEmpty() ? 0 : 1) > 48) break;
            picked.append(w);
            total += w.size() + (picked.isEmpty() ? 0 : 1);
        }
        QString s = picked.join(QChar(' '));
        if (s.isEmpty()) s = flat.left(48);
        return s;
    };
    const QString suggested = suggestNameFrom(trimmed);

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Criar documento"));
    dlg.setModal(true);
    dlg.setMinimumWidth(380);

    auto* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(16, 16, 16, 12);
    root->setSpacing(10);

    auto* nameLab = new QLabel(tr("Nome do documento:"), &dlg);
    root->addWidget(nameLab);
    auto* nameEdit = new QLineEdit(suggested, &dlg);
    nameEdit->selectAll();
    root->addWidget(nameEdit);

    auto* drawerLab = new QLabel(tr("Gaveta de destino:"), &dlg);
    root->addWidget(drawerLab);
    auto* drawerCombo = new QComboBox(&dlg);
    for (const auto& d : projectModel->drawers()) {
        const QString label = d.title.isEmpty() ? tr("(sem nome)") : d.title;
        drawerCombo->addItem(label, d.key);
    }
    drawerCombo->setCurrentIndex(0);
    root->addWidget(drawerCombo);

    // Hint dinâmico pra gavetas visuais (personagem/cenário/objeto).
    auto* hint = new QLabel(&dlg);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(Theme::textMuted()));
    root->addWidget(hint);
    auto refreshHint = [this, drawerCombo, hint]() {
        const QString key = drawerCombo->currentData().toString();
        const Drawer* d = projectModel->findDrawer(key);
        const QString et = d ? d->drawerElementType : QString();
        if (et == QStringLiteral("character"))      hint->setText(tr("Vai abrir o cadastro de personagem em seguida (foto e papel)."));
        else if (et == QStringLiteral("setting"))   hint->setText(tr("Vai abrir o cadastro de cenário em seguida (foto)."));
        else if (et == QStringLiteral("object"))    hint->setText(tr("Vai abrir o cadastro de objeto em seguida (foto)."));
        else hint->clear();
    };
    refreshHint();
    connect(drawerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
            [refreshHint](int) { refreshHint(); });

    root->addStretch();

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* cancel = new QPushButton(tr("Cancelar"), &dlg);
    auto* ok = new QPushButton(tr("Criar"), &dlg);
    ok->setDefault(true);
    btnRow->addWidget(cancel);
    btnRow->addWidget(ok);
    root->addLayout(btnRow);

    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted) return;
    const QString title = nameEdit->text().trimmed();
    if (title.isEmpty()) return;
    const QString destKey = drawerCombo->currentData().toString();
    if (destKey.isEmpty()) return;

    // Constrói o HTML a partir do texto selecionado: parágrafos separados por
    // linhas em branco, quebras simples viram <br>.
    auto buildHtmlFromText = [](const QString& text) -> QString {
        const QString normalized = QString(text).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        const QStringList paras = normalized.split(QRegularExpression(QStringLiteral("\n{2,}")),
                                                   Qt::SkipEmptyParts);
        QString out;
        for (const QString& p : paras) {
            QString chunk = p.trimmed();
            if (chunk.isEmpty()) continue;
            chunk = chunk.toHtmlEscaped();
            chunk.replace(QChar('\n'), QStringLiteral("<br/>"));
            out += QStringLiteral("<p>%1</p>").arg(chunk);
        }
        if (out.isEmpty()) out = QStringLiteral("<p></p>");
        return out;
    };

    const Drawer* destDrawer = projectModel->findDrawer(destKey);
    const QString elemType = destDrawer ? destDrawer->drawerElementType : QString();
    const bool isVisual = elemType == QStringLiteral("character")
        || elemType == QStringLiteral("setting")
        || elemType == QStringLiteral("object");

    QString role;
    QString imageDataUrl;
    if (isVisual) {
        ElementCreateDialog edlg(elemType, this);
        edlg.setInitial(title, QString(), QString());
        if (edlg.exec() != QDialog::Accepted) return;
        const QString finalTitle = edlg.title().trimmed();
        if (finalTitle.isEmpty()) return;
        role = edlg.role();
        imageDataUrl = edlg.imageDataUrl();

        Element elem;
        elem.name = finalTitle;
        elem.type = elemType;
        elem.icon = elemType == QStringLiteral("character") ? QStringLiteral("user")
                  : elemType == QStringLiteral("setting")   ? QStringLiteral("map")
                  : QStringLiteral("cube");
        elem.role = role;
        elem.image = imageDataUrl;
        const QString elementId = elementsStore->addElement(elem);

        DrawerItem it;
        it.id = ProjectModel::uid();
        it.title = finalTitle;
        it.folderId = QString();
        it.hasInlineHtml = true;
        it.html = buildHtmlFromText(raw);
        it.elementType = elemType;
        it.elementId = elementId;
        it.role = role;
        projectModel->addDrawerItem(destKey, it);
        return;
    }

    DrawerItem it;
    it.id = ProjectModel::uid();
    it.title = title;
    it.folderId = QString();
    it.hasInlineHtml = true;
    it.html = buildHtmlFromText(raw);
    projectModel->addDrawerItem(destKey, it);
}
