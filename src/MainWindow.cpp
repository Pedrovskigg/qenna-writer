#include "MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QSizePolicy>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextFrameFormat>
#include <QTextImageFormat>
#include <QTextLength>
#include <QUrl>
#include <QWidget>

#include <QDir>
#include <QInputDialog>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include "DocCache.h"
#include "DrawerCreateDialog.h"
#include "DrawerListPanel.h"
#include "EditorHost.h"
#include "ElementCreateDialog.h"
#include "ElementsStore.h"
#include "ImageInsertDialog.h"
#include "ImageOverlay.h"
#include "LeftBar.h"
#include "ManuscriptPanel.h"
#include "ProjectModel.h"
#include "ProjectSaver.h"
#include "ProjectStorage.h"
#include "RefMenuPanel.h"
#include "WordCountPanel.h"
#include "WordCounter.h"
#include "Theme.h"
#include "TopToolbar.h"
#include "VariationBar.h"

namespace {
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
    , editor(new QTextEdit)
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
    , sceneDetectTimer(nullptr)
    , currentFontFamily(QStringLiteral("Alegreya"))
    , currentFontSize(16)
    , currentLineHeight(170)
    , firstLineIndentEnabled(true)
    , focusModeEnabled(false)
    , baseTextColor(QColor("#e8e3d6"))
{
    baseWindowTitle = tr("Mira Writing");
    setWindowTitle(baseWindowTitle);

    // Toolbar flutuante: holder com margem em volta da TopToolbar
    auto *toolbarHolder = new QWidget(this);
    toolbarHolder->setObjectName(QStringLiteral("topToolbarHolder"));
    auto *holderLayout = new QHBoxLayout(toolbarHolder);
    holderLayout->setContentsMargins(12, 10, 12, 4);
    holderLayout->setSpacing(0);
    holderLayout->addWidget(toolbar);
    setMenuWidget(toolbarHolder);

    resize(1100, 800);

    setupEditor();
    setupToolbar();
    applyEditorStyle();

    QPalette p = editor->palette();
    p.setColor(QPalette::Text, baseTextColor);
    editor->setPalette(p);

    imageOverlay = new ImageOverlay(editor->viewport());
    imageOverlay->hide();
    connect(imageOverlay, &ImageOverlay::widthChangeRequested, this, &MainWindow::changeSelectedImageWidth);
    connect(imageOverlay, &ImageOverlay::alignmentRequested, this, &MainWindow::changeSelectedImageAlignment);

    editor->viewport()->installEventFilter(this);
    connect(editor->verticalScrollBar(), &QAbstractSlider::valueChanged, this, [this]() { hideOverlay(); });
}

void MainWindow::setupEditor()
{
    editor->setFrameStyle(0);
    editor->setAcceptRichText(false);
    editor->setFixedWidth(820);

    leftBar = new LeftBar(projectModel, this);
    drawerListPanel = new DrawerListPanel(projectModel, this);
    manuscriptPanel = new ManuscriptPanel(projectModel, this);
    editorHost = new EditorHost(editor, docCache, projectModel, this);
    editor->setEnabled(false);
    variationBar = new VariationBar(projectModel, editorHost, this);
    connect(editorHost, &EditorHost::viewModeChanged, variationBar, &VariationBar::refresh);

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

    auto *container = new QWidget(this);
    container->setObjectName(QStringLiteral("editorContainer"));
    container->setStyleSheet(QStringLiteral("#editorContainer { background: %1; }").arg(Theme::appBackground()));
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    layout->addWidget(leftBar);
    layout->addStretch();

    // Drawer/Manuscript panels não entram no layout — flutuam ao lado da
    // LeftBar pra não empurrar o editor.
    drawerListPanel->setParent(container);
    manuscriptPanel->setParent(container);
    drawerListPanel->hide();
    manuscriptPanel->hide();

    // Coluna do editor: VariationBar (auto-hide) acima + editor
    auto* editorColumn = new QWidget(this);
    editorColumn->setFixedWidth(820);
    auto* editorColumnLayout = new QVBoxLayout(editorColumn);
    editorColumnLayout->setContentsMargins(0, 0, 0, 0);
    editorColumnLayout->setSpacing(8);
    editorColumnLayout->addWidget(variationBar);
    editor->setFixedWidth(820); // já tava; reafirma
    editorColumnLayout->addWidget(editor, /*stretch=*/1);
    layout->addWidget(editorColumn);

    layout->addStretch();

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

    // Painel flutuante de Referência — canto superior direito.
    refMenuPanel = new RefMenuPanel(projectModel, editorHost, docCache, container);
    refMenuPanel->setProjectRoot(projectRoot);
    refMenuPanel->raise();
    connect(refMenuPanel, &RefMenuPanel::geometryChanged, this, &MainWindow::positionWordCountPanel);
    connect(toolbar, &TopToolbar::refMenuToggleRequested, this, [this]() {
        if (refMenuPanel) {
            refMenuPanel->togglePanel();
            // Posicionar manualmente ao abrir
            if (refMenuPanel->isVisible()) {
                QWidget* parent = refMenuPanel->parentWidget();
                if (parent) {
                    const int margin = 10;
                    refMenuPanel->resize(refMenuPanel->width(), parent->height() - margin * 2);
                    refMenuPanel->move(parent->width() - refMenuPanel->width() - margin, margin);
                }
            }
        }
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
        }
    });
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

    connect(toolbar, &TopToolbar::fontFamilyChanged, this, &MainWindow::setFontFamily);
    connect(toolbar, &TopToolbar::fontSizeChanged, this, &MainWindow::setFontSize);
    connect(toolbar, &TopToolbar::lineHeightChanged, this, &MainWindow::setLineHeight);
    connect(toolbar, &TopToolbar::firstLineIndentToggled, this, &MainWindow::setFirstLineIndent);
    connect(toolbar, &TopToolbar::focusModeToggled, this, &MainWindow::setFocusMode);
    connect(toolbar, &TopToolbar::addImageRequested, this, &MainWindow::onAddImageRequested);
    connect(toolbar, &TopToolbar::newProjectRequested, this, &MainWindow::onNewProjectRequested);
    connect(toolbar, &TopToolbar::openProjectRequested, this, &MainWindow::onOpenProjectRequested);
    connect(toolbar, &TopToolbar::saveProjectRequested, this, [this]() {
        if (projectSaver) projectSaver->saveProject();
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

    QTextCursor cursor(editor->document());
    cursor.select(QTextCursor::Document);
    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(currentLineHeight, QTextBlockFormat::ProportionalHeight);
    blockFormat.setTextIndent(firstLineIndentEnabled ? 30 : 0);
    cursor.mergeBlockFormat(blockFormat);
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
    applyEditorStyle();
}

void MainWindow::setFirstLineIndent(bool enabled)
{
    firstLineIndentEnabled = enabled;
    applyEditorStyle();
}

void MainWindow::setFocusMode(bool enabled)
{
    focusModeEnabled = enabled;

    QColor dimmed = baseTextColor;
    dimmed.setAlpha(enabled ? 100 : 255);
    QPalette p = editor->palette();
    p.setColor(QPalette::Text, dimmed);
    editor->setPalette(p);

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
    QTextEdit::ExtraSelection selection;
    QColor focused = baseTextColor;
    focused.setAlpha(255);
    selection.format.setForeground(focused);
    selection.cursor = editor->textCursor();
    selection.cursor.movePosition(QTextCursor::EndOfBlock);
    selection.cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
    editor->setExtraSelections({selection});
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
    if (watched == editor->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(event);
            QTextCursor imageCursor;
            if (findImageAt(me->pos(), imageCursor)) {
                showOverlayForImage(imageCursor);
                return true;
            }
            hideOverlay();
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
    const int x = margin + leftBar->width() + margin; // após margem esquerda + LeftBar + spacing
    const int y = margin;
    const int h = qMax(0, parent->height() - margin * 2);
    auto place = [&](QWidget* w) {
        if (!w) return;
        w->move(x, y);
        w->resize(w->width(), h);
        if (w->isVisible()) w->raise();
    };
    place(drawerListPanel);
    place(manuscriptPanel);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    positionWordCountPanel();
    positionSidePanels();
    if (refMenuPanel && refMenuPanel->isVisible()) {
        QWidget* parent = refMenuPanel->parentWidget();
        if (parent) {
            const int margin = 10;
            refMenuPanel->resize(refMenuPanel->width(), parent->height() - margin * 2);
            refMenuPanel->move(parent->width() - refMenuPanel->width() - margin, margin);
        }
    }
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
