#include "MainWindow.h"

#include "MainMenuDialog.h"
#include "NewProjectFlow.h"
#include "UpdateChecker.h"

#ifdef Q_OS_WIN
#include <dwmapi.h>
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#endif

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
#include <QCursor>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QCryptographicHash>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStyle>
#include <QSizePolicy>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextFrameFormat>
#include <QTextImageFormat>
#include <QTextLength>
#include <QHash>
#include <QUrl>
#include <QWidget>
#include <QFile>
#include <QIODevice>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressBar>
#include <QStandardPaths>

#include <QDir>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QLocale>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include "BondPopup.h"
#include "BondViewPanel.h"
#include "CharacterDetector.h"
#include "DocCache.h"
#include "DrawerCreateDialog.h"
#include "DrawerListPanel.h"
#include "EditorHost.h"
#include "EditorLayout.h"
#include "ElementCreateDialog.h"
#include "ElementsStore.h"
#include "FindBar.h"
#include "GlobalSearchPanel.h"
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
#include "PensarioPanel.h"
#include "NotesStore.h"
#include "MapPinsStore.h"
#include "SceneUtils.h"
#include "AmbienceManager.h"
#include "AmbiencePanel.h"
#include "GlossaryAddPopup.h"
#include "GlossaryPanel.h"
#include "GlossaryStore.h"
#include "MemoriesStore.h"
#include "MemoryAddPopup.h"
#include "LousaPanel.h"
#include "TimelinePanel.h"
#include "RemindersPanel.h"
#include "RemindersStore.h"
#include "GroupsPanel.h"
#include "MarkerHoverPopup.h"
#include "MarkerPickPopup.h"
#include "MarkerStore.h"
#include "PresencePopup.h"
#include "SelectionPopup.h"
#include "SettingsPanel.h"
#include "ExportPanel.h"
#include "Exporter.h"
#include "SpellChecker.h"
#include "SpellEditor.h"
#include "SpellHighlighter.h"
#include "ThemesPanel.h"
#include "BackgroundWidget.h"
#include "WordCountPanel.h"
#include "WordCounter.h"
#include "Theme.h"
#include "TopToolbar.h"
#include "VariationBar.h"

namespace {
// Documentos com mais de este número de caracteres têm rehighlight do spell
// diferido por 400ms para não bloquear a UI ao abrir o doc.
constexpr int kLargeDocCharThreshold = 30'000;

// Diálogo de capítulo: título + "quando se passa" (marcador temporal opcional).
// Usado tanto na criação quanto na edição. Retorna false se cancelado.
bool promptChapterDialog(QWidget* parent, bool editMode,
                         QString* title, QString* marker)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(editMode ? QObject::tr("Editar capítulo")
                                : QObject::tr("Novo capítulo"));
    dlg.setMinimumWidth(340);
    if (parent) dlg.setStyleSheet(parent->styleSheet());

    auto* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto* form = new QFormLayout;
    auto* titleEdit = new QLineEdit(*title, &dlg);
    titleEdit->setPlaceholderText(QObject::tr("Título do capítulo"));
    auto* markerEdit = new QLineEdit(*marker, &dlg);
    markerEdit->setPlaceholderText(QObject::tr("ex.: Dia 5, Verão de 1999, há 10 anos…"));
    form->addRow(QObject::tr("Título:"), titleEdit);
    form->addRow(QObject::tr("Quando se passa:"), markerEdit);
    root->addLayout(form);

    auto* hint = new QLabel(QObject::tr("O marcador temporal é opcional e alimenta o "
                                        "eixo História da linha do tempo."), &dlg);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:%1; font-size:11px;").arg(Theme::textMuted()));
    root->addWidget(hint);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btns->button(QDialogButtonBox::Ok)->setText(editMode ? QObject::tr("Salvar")
                                                         : QObject::tr("Criar"));
    btns->button(QDialogButtonBox::Cancel)->setText(QObject::tr("Cancelar"));
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    root->addWidget(btns);

    titleEdit->setFocus();
    if (dlg.exec() != QDialog::Accepted) return false;
    const QString t = titleEdit->text().trimmed();
    if (t.isEmpty()) return false;
    *title  = t;
    *marker = markerEdit->text().trimmed();
    return true;
}

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

// Widget flutuante que mostra "Voltando/Avançando para Cap. X" com barra de
// progresso preenchendo da esquerda enquanto o timer de auto-navegação corre.
class AutoNavHint : public QWidget {
public:
    AutoNavHint(QWidget* parent) : QWidget(parent) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoSystemBackground);
        setFixedSize(190, 28);
    }
    void setProgress(float p) { if (m_progress != p) { m_progress = p; update(); } }
    void setText(const QString& t) { if (m_text != t) { m_text = t; update(); } }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRect r = rect().adjusted(0, 2, -2, -2);
        p.setPen(Qt::NoPen);
        // Fundo escuro semi-transparente
        p.setBrush(QColor(18, 18, 18, 215));
        p.drawRoundedRect(r, 5, 5);
        // Preenchimento de progresso
        if (m_progress > 0.0f) {
            QColor ac = QColor(Theme::accentDefault());
            ac.setAlpha(170);
            QRect fill = r;
            fill.setWidth(qRound(r.width() * m_progress));
            p.setBrush(ac);
            p.drawRoundedRect(fill, 5, 5);
        }
        // Texto
        p.setPen(QColor(220, 220, 220));
        QFont f = font();
        f.setPointSizeF(8.0);
        p.setFont(f);
        p.drawText(r.adjusted(7, 0, -7, 0), Qt::AlignVCenter | Qt::AlignLeft, m_text);
    }

private:
    float m_progress = 0.0f;
    QString m_text;
};

MainWindow::~MainWindow() {
    // mainMenuDialog é criado sem parent (pra ter taskbar entry própria),
    // então não morre junto com o MainWindow — temos que deletar à mão.
    delete mainMenuDialog;
    mainMenuDialog = nullptr;
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

    // Toolbar flutuante: filho direto de this, posicionado sobre o centralWidget.
    // Sem setMenuWidget — o centralWidget ocupa a janela inteira (incluindo atrás
    // da toolbar), permitindo que a página vaze 100% até o topo.
    auto *holder = new QWidget(this);
    holder->setObjectName(QStringLiteral("topToolbarHolder"));
    holder->setAttribute(Qt::WA_StyledBackground, true);
    holder->setStyleSheet(QStringLiteral("#topToolbarHolder { background: %1; }").arg(Theme::appBackground()));
    toolbarHolder = holder;
    auto *holderLayout = new QHBoxLayout(holder);
    holderLayout->setContentsMargins(12, 0, 12, 4);
    holderLayout->setSpacing(0);
    holderLayout->addWidget(toolbar);
    toolbar->installEventFilter(this); // auto-hide da toolbar no modo focado

    resize(1100, 800);
    setWindowState(windowState() | Qt::WindowMaximized);

    // Aplica limite de docs na RAM salvo nas configurações (default 6).
    docCache->setMaxDocs(QSettings().value(QStringLiteral("docCache/maxDocs"), 6).toInt());

    setupEditor();
    setupToolbar();
    applyBackgroundFromTheme();
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

    // Checagem de atualização: silenciosa, em background, alguns segundos
    // após o startup pra não competir com o carregamento do projeto.
    m_updateChecker = new UpdateChecker(this);
    connect(m_updateChecker, &UpdateChecker::updateAvailable, this,
            [this](const QString& version, const QString& downloadUrl, const QString&, const QString& releaseNotes) {
                showUpdateToast(version, downloadUrl, releaseNotes);
            });
    QTimer::singleShot(3000, this, [this]() {
        if (m_updateChecker) m_updateChecker->check();
    });
}

void MainWindow::setupEditor()
{
    editor->setFrameStyle(0);
    editor->setAcceptRichText(false);
    editor->setFixedWidth(EditorLayout::pageWidth());

    leftBar = new LeftBar(projectModel, this);
    leftBar->installEventFilter(this); // auto-hide no modo focado
    drawerListPanel = new DrawerListPanel(projectModel, this);
    connect(drawerListPanel, &DrawerListPanel::panelWidthChanged, this, &MainWindow::positionSidePanels);
    connect(drawerListPanel, &DrawerListPanel::panelHeightChanged, this, &MainWindow::positionSidePanels);
    connect(drawerListPanel, &DrawerListPanel::consistencyModeChanged, this, [this](bool on) {
        if (on) {
            // Atualiza a gaveta selecionada visualmente (já foi aberta)
            if (!drawerListPanel->currentDrawerKey().isEmpty())
                leftBar->setActiveDrawer(drawerListPanel->currentDrawerKey());
        } else if (!drawerListPanel->isPanelOpen()) {
            leftBar->clearSelection();
        }
    });
    connect(drawerListPanel, &DrawerListPanel::consistencyUpdateRequested, this,
            [this](const QString& itemId, const QString& status,
                   const QString& statusDetail, const QString& location) {
        projectModel->updateDrawerItemConsistency(itemId, status, statusDetail, location);
    });
    m_presenceProvider = [this](
        const QStringList& names,
        QHash<QString, CharPresenceResult>* out,
        int* outTotalScenes,
        int* outTotalChapters)
    {
        if (!projectModel) return;

        const auto& chapters = projectModel->chapters();
        *outTotalChapters = chapters.size();
        *outTotalScenes   = 0;

        // Inicializa resultado vazio para cada personagem
        for (const QString& n : names)
            (*out)[n.toLower()] = CharPresenceResult{};

        // Tokeniza nomes: partes >= 3 chars usadas na busca por palavra-limite
        QHash<QString, QStringList> nameTokens;
        for (const QString& n : names) {
            const QString nl = n.toLower();
            QStringList toks;
            const QRegularExpression wsRe(QStringLiteral("\\s+"));
            for (const QString& t : nl.split(wsRe, Qt::SkipEmptyParts))
                if (t.size() >= 3) toks.append(t);
            if (!toks.isEmpty()) nameTokens[nl] = toks;
        }

        // Strip HTML simples para obter texto plano
        const QRegularExpression tagRe(QStringLiteral("<[^>]+>"));
        auto stripHtml = [&](const QString& html) -> QString {
            QString t = html;
            t.remove(tagRe);
            t.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
            t.replace(QStringLiteral("&amp;"),  QStringLiteral("&"));
            t.replace(QStringLiteral("&lt;"),   QStringLiteral("<"));
            t.replace(QStringLiteral("&gt;"),   QStringLiteral(">"));
            return t.toLower();
        };

        // Verifica se token aparece como palavra inteira no texto
        auto tokenInText = [](const QString& tok, const QString& text) -> bool {
            int pos = 0;
            while ((pos = text.indexOf(tok, pos)) != -1) {
                const bool prevOk = (pos == 0 || !text[pos - 1].isLetterOrNumber());
                const bool nextOk = (pos + tok.size() >= text.size()
                                     || !text[pos + tok.size()].isLetterOrNumber());
                if (prevOk && nextOk) return true;
                ++pos;
            }
            return false;
        };

        for (const Chapter& ch : chapters) {
            const QString chKey = DocCache::chapterKey(ch.manuscriptId, ch.id);
            QString html;
            if (docCache && docCache->has(chKey)) {
                html = docCache->get(chKey);
            } else {
                bool ok = false;
                html = ProjectStorage::readChapter(projectRoot, ch.file, &ok);
            }
            if (html.isEmpty()) continue;

            const QStringList sceneHtmls = SceneUtils::splitHtmlIntoScenes(html);
            const bool multiScene = sceneHtmls.size() > 1;
            *outTotalScenes += multiScene ? sceneHtmls.size() : 1;

            const QString chTitle = ch.title.isEmpty()
                ? tr("Capítulo %1").arg(ch.order + 1) : ch.title;

            for (const QString& n : names) {
                const QString nl = n.toLower();
                const QStringList& toks = nameTokens.value(nl);
                if (toks.isEmpty()) continue;

                auto charPresent = [&](const QString& plainText) -> bool {
                    for (const QString& tok : toks)
                        if (tokenInText(tok, plainText)) return true;
                    return false;
                };

                PresenceChapterEntry chEntry;
                chEntry.id    = ch.id;
                chEntry.title = chTitle;

                if (!multiScene) {
                    const QString plain = stripHtml(html);
                    if (charPresent(plain)) {
                        (*out)[nl].chapters.append(chEntry);
                        (*out)[nl].chapterCount++;
                        (*out)[nl].sceneCount++;
                    }
                } else {
                    bool anyInChapter = false;
                    for (int si = 0; si < sceneHtmls.size(); ++si) {
                        const QString plain = stripHtml(sceneHtmls[si]);
                        if (!charPresent(plain)) continue;

                        PresenceSceneEntry scEntry;
                        scEntry.index = si;
                        scEntry.title = (si < ch.scenes.size())
                            ? ch.scenes[si].title : QString();
                        chEntry.scenes.append(scEntry);
                        (*out)[nl].sceneCount++;
                        anyInChapter = true;
                    }
                    if (anyInChapter) {
                        (*out)[nl].chapters.append(chEntry);
                        (*out)[nl].chapterCount++;
                    }
                }
            }
        }
    };
    drawerListPanel->setPresenceProvider(m_presenceProvider);
    manuscriptPanel = new ManuscriptPanel(projectModel, this);
    editorHost = new EditorHost(editor, docCache, projectModel, this);
    editor->setEnabled(false);

    // Persiste o último doc aberto por projeto — restaurado no próximo load.
    // Ignora Disabled (estado transitório durante troca de projeto).
    connect(editorHost, &EditorHost::viewModeChanged, this, &MainWindow::updateDocCachePinnedKeys);
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
    // engasgar — para docs grandes, difere o rehighlight pra depois da UI renderizar.
    connect(editorHost, &EditorHost::contentLoaded, this, [this]() {
        if (!spellHighlighter || !editor) return;
        const int charCount = editor->document()->characterCount();
        if (charCount > kLargeDocCharThreshold) {
            QTimer::singleShot(400, this, [this]() {
                if (spellHighlighter) spellHighlighter->rehighlight();
            });
        } else {
            spellHighlighter->rehighlight();
        }
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
    selectionPopup->addAction(QStringLiteral("leftbar/timeline.svg"), tr("Criar evento da linha do tempo..."),
        [this]() { createTimelineEventFromSelection(); });
    selectionPopup->addAction(QStringLiteral("elements/heart.svg"), tr("Adicionar à memória..."),
        [this]() { addSelectionToMemory(); });

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

    // hrInserted: atalho ---- disparado — sincroniza cenas imediatamente (sem o delay
    // de 1,5s do sceneDetectTimer) e exibe toast visual para que o usuário saiba que
    // a nova cena foi criada.
    connect(editorHost, &EditorHost::hrInserted, this, [this](const QString& /*chId*/) {
        sceneDetectTimer->stop();
        detectScenesForPending();

        // Toast "Nova cena" flutuante sobre o editor
        if (!editorContainer) return;
        auto* toast = new QLabel(tr("Nova cena criada"), editorContainer);
        toast->setObjectName(QStringLiteral("sceneToast"));
        toast->setAlignment(Qt::AlignCenter);
        toast->setStyleSheet(QStringLiteral(
            "QLabel#sceneToast {"
            "  background: %1;"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 6px;"
            "  padding: 6px 16px;"
            "  font-family: 'Lora','Crimson Text',serif;"
            "  font-size: 12px;"
            "}").arg(Theme::panelBackground(), Theme::textBright(), Theme::panelBorder()));
        toast->adjustSize();
        const QPoint center = editorContainer->rect().center();
        toast->move(center.x() - toast->width() / 2,
                    editorContainer->height() - toast->height() - 32);
        toast->show();
        toast->raise();

        // Apaga após 1,5 s
        QTimer::singleShot(1500, toast, [toast]() {
            if (toast) toast->deleteLater();
        });
    });

    projectSaver = new ProjectSaver(projectModel, docCache, editorHost, this);
    elementsStore = new ElementsStore(this);
    projectSaver->setElementsStore(elementsStore);

    // PresencePopup + detecção de presença idioma-agnóstica (scan incremental no event loop).
    presencePopup = new PresencePopup(elementsStore, this);

    // Timer de debounce: 3s após parar de digitar inicia o scan
    detectionTimer = new QTimer(this);
    detectionTimer->setSingleShot(true);
    detectionTimer->setInterval(3000);

    // Timer de lote: processa 2 personagens por ciclo do event loop, sem travar a UI
    detectionBatchTimer = new QTimer(this);
    detectionBatchTimer->setSingleShot(false);
    detectionBatchTimer->setInterval(0);

    connect(detectionTimer, &QTimer::timeout, this, [this]() {
        if (!detectionEnabled || !elementsStore || !editorHost || !editor) return;
        const QString key = editorHost->activeKey();
        if (key.isEmpty()) return;

        qDebug() << "[detect] iniciando scan para" << key;
        // Cancela scan anterior e prepara novo estado
        detectionBatchTimer->stop();
        m_scanState = std::make_unique<DetectionScanState>();
        auto& s = *m_scanState;
        s.plainText = editor->toPlainText();
        s.lowerText = s.plainText.toLower();
        s.docKey    = key;
        s.elements  = elementsStore->elements();
        { const QStringList _p = elementsStore->docElementIds(key);
          s.alreadyPresent = QSet<QString>(_p.cbegin(), _p.cend()); }
        s.autoIds    = detectionAutoIds;
        s.neverIds   = detectionNeverIds;
        s.rejectedKeys = detectionRejectedKeys;
        s.markAll    = detectionMarkAll;
        s.wordCount  = s.lowerText.split(QRegularExpression(QStringLiteral("\\s+")),
                                          Qt::SkipEmptyParts).size();
        s.idx = 0;
        detectionBatchTimer->start();
    });

    connect(detectionBatchTimer, &QTimer::timeout, this, [this]() {
        if (!m_scanState) { detectionBatchTimer->stop(); return; }
        auto& s = *m_scanState;

        // Processa até 2 personagens por ciclo
        int processed = 0;
        while (s.idx < s.elements.size() && processed < 2) {
            const Element& elem = s.elements.at(s.idx++);
            ++processed;

            if (elem.type != QStringLiteral("character")) continue;
            if (s.alreadyPresent.contains(elem.id)) continue;
            if (s.rejectedKeys.contains(s.docKey + QLatin1Char(':') + elem.id)) continue;

            bool detected = false;
            if (elem.narrator) {
                detected = (s.wordCount >= CharacterDetector::kNarratorMinWords);
            } else {
                const QStringList tokens = CharacterDetector::buildTokensPublic(elem);
                for (const QString& token : tokens) {
                    const QString pattern = QStringLiteral("\\b")
                        + QRegularExpression::escape(token) + QStringLiteral("\\b");
                    QRegularExpression re(pattern, QRegularExpression::CaseInsensitiveOption);
                    int count = 0;
                    auto it = re.globalMatch(s.lowerText);
                    while (it.hasNext()) { it.next(); if (++count >= CharacterDetector::kThreshold) { detected = true; break; } }
                    if (detected) break;
                }
            }

            if (!detected) continue;

            CharacterDetection det{ elem.id, s.docKey };
            if (elem.narrator || s.markAll || s.autoIds.contains(elem.id))
                s.result.autoMark.append(det);
            else if (!s.neverIds.contains(elem.id))
                s.result.confirm.append(det);
        }

        // Scan completo
        if (s.idx >= s.elements.size()) {
            detectionBatchTimer->stop();
            const ScanResult res = std::move(s.result);
            m_scanState.reset();

            qDebug() << "[detect] scan completo — autoMark:" << res.autoMark.size() << "confirm:" << res.confirm.size();

            if (!res.autoMark.isEmpty() && elementsStore) {
                qDebug() << "[detect] adicionando autoMark ao elementsStore";
                // Batch por docKey para emitir changed() uma só vez (evita N×refresh)
                QHash<QString, QStringList> byDoc;
                for (const auto& d : res.autoMark)
                    byDoc[d.docKey].append(d.elementId);
                for (auto it = byDoc.cbegin(); it != byDoc.cend(); ++it)
                    elementsStore->addManyDocElements(it.key(), it.value());
            }
            if (!res.confirm.isEmpty() && presencePopup) {
                qDebug() << "[detect] enqueue no presencePopup";
                presencePopup->enqueue(res.confirm);
                presencePopup->adjustSize();
                const QRect win = this->geometry();
                presencePopup->move(
                    win.x() + win.width()  - presencePopup->width()  - 20,
                    win.y() + win.height() - presencePopup->height() - 48);
            }
        }
    });

    // Se um elemento é criado durante o scan, o snapshot fica desatualizado —
    // cancelar para que o próximo ciclo recomece com o estado correto.
    connect(elementsStore, &ElementsStore::changed, this, [this]() {
        if (!m_scanState || !elementsStore) return;
        if (m_scanState->elements.size() != elementsStore->elements().size()) {
            qDebug() << "[detect] lista de elementos mudou durante scan — cancelando";
            detectionBatchTimer->stop();
            m_scanState.reset();
        }
    });

    // Reinicia o timer a cada mudança de texto
    connect(editor, &QTextEdit::textChanged, this, [this]() {
        if (detectionEnabled) detectionTimer->start();
    });

    // Ao carregar novo doc, reinicia o timer e atualiza docKey no painel de consistência
    connect(editorHost, &EditorHost::contentLoaded, this, [this]() {
        if (detectionEnabled) detectionTimer->start();
        if (drawerListPanel && editorHost)
            drawerListPanel->setCurrentDocKey(editorHost->activeKey());
    });

    connect(presencePopup, &PresencePopup::markRequested,
            this, [this](const QString& elementId, const QString& docKey) {
        if (elementsStore) elementsStore->addDocElement(docKey, elementId);
    });
    connect(presencePopup, &PresencePopup::markAllActivated, this, [this]() {
        detectionMarkAll = true;
        if (settingsPanel) settingsPanel->setDetectionMarkAll(true);
        if (!projectRoot.isEmpty()) {
            QSettings qs;
            qs.beginGroup(QStringLiteral("detectionState"));
            qs.beginGroup(QString::fromLatin1(
                QCryptographicHash::hash(projectRoot.toUtf8(), QCryptographicHash::Md5).toHex()));
            qs.setValue(QStringLiteral("markAll"), true);
            qs.endGroup(); qs.endGroup();
        }
    });
    connect(presencePopup, &PresencePopup::ignoredNow,
            this, [this](const QString& elementId, const QString& docKey) {
        detectionRejectedKeys.insert(docKey + QLatin1Char(':') + elementId);
    });
    connect(presencePopup, &PresencePopup::neverRequested,
            this, [this](const QString& elementId, const QString& /*docKey*/) {
        detectionNeverIds.insert(elementId);
    });

    // MarkerStore: sidecar de markers comentados. Carrega ao abrir projeto,
    // re-aplica GUIDs no contentLoaded, captura ao flush e salva ao saveProject.
    markerStore = new MarkerStore(this);
    notesStore = new NotesStore(this);
    mapPinsStore = new MapPinsStore(this);
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

    // Memórias: store (sidecar JSON) + popup de "Adicionar à memória..." (mini
    // toolbar de seleção). O texto selecionado é salvo no projeto ou na
    // memória de um personagem, com o rótulo da fonte (capítulo/cena).
    memoriesStore = new MemoriesStore(this);
    memoryAddPopup = new MemoryAddPopup(this);
    connect(memoryAddPopup, &MemoryAddPopup::confirmed, this,
            [this](const QString& name, const QString& targetType, const QString& elementId) {
        if (!memoriesStore || !m_pendingMemory.has_value()) return;
        MemoriesStore::Memory mem = *m_pendingMemory;
        mem.name = name;
        mem.targetType = targetType;
        mem.elementId = (targetType == QStringLiteral("character")) ? elementId : QString();
        memoriesStore->add(mem);
        const QString where = (mem.targetType == QStringLiteral("character"))
            ? tr("memória do personagem") : tr("memória do projeto");
        m_pendingMemory.reset();
        showReminderToast(tr("Memória salva"),
            tr("Trecho guardado na %1.").arg(where));
    });
    connect(memoryAddPopup, &MemoryAddPopup::cancelled, this,
            [this]() { m_pendingMemory.reset(); });
    // Persiste o sidecar sempre que o store muda e atualiza o RefMenu.
    connect(memoriesStore, &MemoriesStore::changed, this, [this]() {
        if (memoriesStore) memoriesStore->save();
        if (refMenuPanel) refMenuPanel->refresh();
    });

    // Lembretes: store (sidecar JSON) + painel flutuante + polling de notificações.
    remindersStore = new RemindersStore(this);
    remindersPanel = new RemindersPanel(remindersStore, this);

    connect(toolbar, &TopToolbar::reminderRequested, this, [this]() {
        if (!remindersPanel || !toolbar) return;
        if (remindersPanel->isVisible()) {
            remindersPanel->hide();
            return;
        }
        remindersPanel->showNear(toolbar->reminderButtonGlobalRect());
    });
    connect(remindersStore, &RemindersStore::changed, this, [this]() {
        if (!remindersStore) return;
        remindersStore->save();
        if (toolbar) toolbar->setReminderBadge(remindersStore->hasActive());
    });

    m_reminderPollTimer = new QTimer(this);
    m_reminderPollTimer->setInterval(60'000);
    connect(m_reminderPollTimer, &QTimer::timeout, this, [this]() {
        if (!remindersStore) return;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        bool anyFired = false;
        for (const Reminder& r : remindersStore->all()) {
            if (r.completedAt != 0 || !r.notify || r.dueAt == 0 || r.notifiedAt != 0) continue;
            if (r.dueAt <= now) {
                remindersStore->markNotified(r.id);
                showReminderToast(tr("Lembrete"), r.text);
                anyFired = true;
            }
        }
        if (anyFired) remindersStore->save();
    });
    m_reminderPollTimer->start();

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

    // Autoload do projeto marcado como "abrir automaticamente" (opt-in via
    // checkbox no Main Menu). Sem opt-in, o app abre direto no Main Menu.
    {
        const QString autoPath = QSettings()
            .value(QStringLiteral("autoOpenProject")).toString();
        if (!autoPath.isEmpty() && QDir(autoPath).exists()) {
            QString loadErr;
            if (!loadProjectFrom(autoPath, &loadErr)) {
                qWarning("Falha no autoload: %s", qUtf8Printable(loadErr));
            }
        }
    }

    // Sem projeto carregado? Abre o Main Menu no próximo tick — espera a
    // window aparecer pra o dialog ter parent visível.
    if (projectRoot.isEmpty()) {
        QTimer::singleShot(0, this, &MainWindow::openMainMenu);
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
        QString title, marker;
        if (!promptChapterDialog(this, false, &title, &marker)) return;
        Chapter c;
        c.id = ProjectModel::uid();
        c.manuscriptId = msId;
        c.title = title;
        c.timeMarker = marker;
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

    // F11 → toggle Fullscreen.
    auto* fullscreenShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(fullscreenShortcut, &QShortcut::activated, this, [this]() {
        const bool entering = !isFullScreen();
        if (entering) showFullScreen(); else showMaximized();
        if (toolbar) toolbar->setFullscreenChecked(entering);
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

    // Background pintado atrás de tudo. Filho da janela (não do container) pra
    // cobrir TAMBÉM a área da toolbar no topo — assim, no modo focado, a imagem
    // do tema aparece no lugar das barras escondidas (sem faixa sólida).
    backgroundWidget = new BackgroundWidget(this);
    backgroundWidget->setGeometry(rect());
    backgroundWidget->lower();
    backgroundWidget->hide(); // só ativa quando o tema atual tem imagem

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

    // Scrollbar flutuante: filho do container (não do editorRow) para que sua
    // altura seja travada na viewport, independente do comprimento da página.
    externalScrollBar = new QScrollBar(Qt::Vertical, container);
    externalScrollBar->setObjectName(QStringLiteral("externalEditorScroll"));
    // Espelha o estado da scrollbar interna do editor.
    auto* innerSb = editor->verticalScrollBar();
    externalScrollBar->setRange(innerSb->minimum(), innerSb->maximum());
    externalScrollBar->setPageStep(innerSb->pageStep());
    externalScrollBar->setSingleStep(innerSb->singleStep());
    externalScrollBar->setValue(innerSb->value());
    // rangeChanged dispara depois que o editor recalcula o layout do documento
    // (mudança de conteúdo, fonte, ou tamanho do widget). É o momento certo pra
    // re-sincronizar range E pageStep/singleStep — daí o thumb fica sempre
    // proporcional ao conteúdo real.
    connect(innerSb, &QAbstractSlider::rangeChanged, this, [this, innerSb]() {
        externalScrollBar->setRange(innerSb->minimum(), innerSb->maximum());
        externalScrollBar->setPageStep(innerSb->pageStep());
        externalScrollBar->setSingleStep(innerSb->singleStep());
    });
    connect(innerSb, &QAbstractSlider::valueChanged, externalScrollBar, &QScrollBar::setValue);
    connect(externalScrollBar, &QAbstractSlider::valueChanged, innerSb, &QScrollBar::setValue);

    // Drag da scrollbar: ativa a zona de nav só enquanto o handle está pressionado
    // na borda. Soltar o handle cancela o carregamento se ele ainda não disparou.
    connect(externalScrollBar, &QAbstractSlider::sliderPressed, this, [this]() {
        m_sliderHeld = true;
        if (m_autoNavCooldown) return;
        auto* sb = editor->verticalScrollBar();
        const int v = sb->value(), max = sb->maximum();
        if (max <= 0) return;
        const auto vm = editorHost->viewMode();
        if (vm.type != EditorHost::ChapterDoc && vm.type != EditorHost::SceneDoc) return;
        if (v == 0)   activateNavZone(-1, vm);
        if (v == max) activateNavZone( 1, vm);
    });
    connect(externalScrollBar, &QAbstractSlider::sliderReleased, this, [this]() {
        m_sliderHeld = false;
        if (m_autoNavDir != 0) deactivateNavZone();
    });

    // Navegação automática entre capítulos: ao chegar na extremidade do scroll
    // e permanecer por 1,5 s (wheel ou arrastar) vai pro capítulo anterior/seguinte.
    m_autoNavTimer = new QTimer(this);
    m_autoNavTimer->setSingleShot(true);
    m_autoNavTimer->setInterval(2000);
    connect(m_autoNavTimer, &QTimer::timeout, this, [this]() {
        const int dir = m_autoNavDir;
        deactivateNavZone();
        navigateAdjacentChapter(dir);
    });

    // Hint widget: barra de progresso + label flutuante ao lado do scroll.
    m_autoNavHint = new AutoNavHint(container);
    m_autoNavHint->hide();

    m_autoNavProgressTimer = new QTimer(this);
    m_autoNavProgressTimer->setInterval(16);
    connect(m_autoNavProgressTimer, &QTimer::timeout, this, [this]() {
        m_autoNavProgressMs += 16;
        if (m_autoNavHint)
            m_autoNavHint->setProgress(qMin(1.0f, m_autoNavProgressMs / 2000.0f));
    });

    // valueChanged: drag ativa a zona; wheel usa o caminho do eventFilter.
    // Aqui apenas cancelamos se o valor saiu da borda.
    connect(innerSb, &QAbstractSlider::valueChanged, this, [this, innerSb]() {
        if (m_autoNavCooldown) return;
        const int v   = innerSb->value();
        const int max = innerSb->maximum();
        if (m_sliderHeld) {
            // Drag: ativa/cancela conforme posição atual do handle
            if (max <= 0) { deactivateNavZone(); return; }
            const auto vm = editorHost->viewMode();
            if (vm.type != EditorHost::ChapterDoc && vm.type != EditorHost::SceneDoc) {
                deactivateNavZone(); return;
            }
            int dir = 0;
            if (v == 0)   dir = -1;
            if (v == max) dir = 1;
            if (dir != 0) activateNavZone(dir, vm);
            else          deactivateNavZone();
        } else if (m_autoNavDir != 0) {
            // Sem drag: cancela zona ativa se o valor saiu da borda esperada
            const bool atEdge = (m_autoNavDir < 0 && v == 0) || (m_autoNavDir > 0 && v == max);
            if (!atEdge) deactivateNavZone();
        }
    });

    editorRowLayout->addWidget(editor, /*stretch=*/1);
    // externalScrollBar não entra no layout — é posicionado como overlay flutuante
    // por positionExternalScrollBar(), chamado em resizeEvent e positionSidePanels.

    editorColumnLayout->addWidget(editorRow, /*stretch=*/1);

    // Envelopa o editorColumn numa QScrollArea: garante centralização robusta
    // mesmo quando a página é mais larga que a janela (e nesse caso aparece
    // scroll horizontal em vez de empurrar a UI).
    editorScroll = new QScrollArea(container);
    editorScroll->setObjectName(QStringLiteral("editorScroll"));
    editorScroll->setWidget(editorColumnWidget);
    editorScroll->setWidgetResizable(false);
    // VCenter pra que, com comprimento fixo, a folha fique centrada na vertical
    // e a imagem de fundo apareça acima/abaixo. No modo "Automático" a coluna
    // tem a altura da viewport, então centralizar não muda nada.
    editorScroll->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    editorScroll->setFrameShape(QFrame::NoFrame);
    editorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    editorScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editorScroll->viewport()->setAutoFillBackground(false);
    editorScroll->setStyleSheet(QStringLiteral("QScrollArea, QScrollArea > QWidget, QScrollArea > QWidget > QWidget { background: transparent; }"));
    layout->addWidget(editorScroll, /*stretch=*/1);

    // Largura e margens internas — vêm do EditorLayout (global, QSettings).
    applyEditorLayout();

    setCentralWidget(container);

    // Toolbar flutua sobre o centralWidget: posicionar e levantar depois do
    // setCentralWidget, senão o resize do central a empurra pra baixo.
    if (toolbarHolder) {
        toolbarHolder->setGeometry(0, 0, width(), toolbarHolder->sizeHint().height());
        toolbarHolder->raise();
    }
    updateEditorContainerMargins();

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
    connect(refMenuPanel, &RefMenuPanel::selectedKeyChanged, this, &MainWindow::updateDocCachePinnedKeys);
    connect(toolbar, &TopToolbar::refMenuToggleRequested, this, [this]() {
        if (refMenuPanel) refMenuPanel->togglePanel();
    });

    // Pensário — painel auxiliar criativo. Fatia 1: agregador de comentários.
    pensarioPanel = new PensarioPanel(markerStore, projectModel, notesStore, container);
    pensarioPanel->setMapPinsStore(mapPinsStore);
    pensarioPanel->setElementsStore(elementsStore);
    pensarioPanel->setTopInset(toolbarHolder ? toolbarHolder->sizeHint().height() : 0);
    pensarioPanel->raise();
    connect(pensarioPanel, &PensarioPanel::openMarkerRequested,
            this, &MainWindow::openMarkerInEditor);
    connect(pensarioPanel, &PensarioPanel::openMemoryInEditorRequested,
            this, &MainWindow::openMemoryInEditor);
    connect(pensarioPanel, &PensarioPanel::openMemoryInRefRequested, this,
            [this](const MemoriesStore::Memory& mem) {
        if (refMenuPanel) refMenuPanel->openMemoryInRef(mem);
    });
    connect(toolbar, &TopToolbar::pensarioToggleRequested, this, [this]() {
        if (pensarioPanel) pensarioPanel->togglePanel();
    });
    if (markerStore) {
        connect(markerStore, &MarkerStore::markersChanged, this, [this](const QString&) {
            if (pensarioPanel && pensarioPanel->isPanelOpen()) pensarioPanel->refresh();
        });
    }

    // FindBar (Ctrl+F) — busca no doc em edição.
    findBar = new FindBar(container);
    findBar->attachTo(editor);
    connect(findBar, &FindBar::selectionsChanged, this, [this]() {
        setEditorSelectionsLayer(QStringLiteral("find"), findBar->findSelections());
    });
    connect(findBar, &FindBar::closed, this, [this]() {
        if (editor) editor->setFocus();
    });

    // GlobalSearchPanel (Ctrl+Shift+F) — busca no projeto inteiro.
    globalSearchPanel = new GlobalSearchPanel(projectModel, docCache, projectRoot, container);
    connect(globalSearchPanel, &GlobalSearchPanel::openRequested, this,
            [this](EditorHost::ViewMode vm, QString query) {
        if (editorHost) editorHost->setViewMode(vm);
        // Após carregar, dispara FindBar com a query pra mostrar onde está.
        if (!query.isEmpty() && findBar) {
            QTimer::singleShot(60, this, [this, query]() {
                if (!findBar) return;
                findBar->openBar();
                // Reabrir já chamou refreshMatches sem query; refletir a query
                // injetando via QLineEdit é o caminho — mas como o input do
                // FindBar é privado, faz-se via openBar+findChild:
                if (auto* in = findBar->findChild<QLineEdit*>(QStringLiteral("findBarInput"))) {
                    in->setText(query);
                }
            });
        }
    });

    // Atalhos das 3 buscas.
    auto* findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(findShortcut, &QShortcut::activated, this, [this]() {
        if (!findBar) return;
        positionFindBar();
        findBar->openBar();
    });
    auto* globalSearchShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")), this);
    connect(globalSearchShortcut, &QShortcut::activated, this, [this]() {
        if (!globalSearchPanel) return;
        positionGlobalSearchPanel();
        globalSearchPanel->openPanel();
    });
    auto* refSearchShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+F")), this);
    connect(refSearchShortcut, &QShortcut::activated, this, [this]() {
        if (refMenuPanel) refMenuPanel->openSearch();
    });
    auto* refPreviewFindShortcut = new QShortcut(QKeySequence(QStringLiteral("Alt+F")), this);
    connect(refPreviewFindShortcut, &QShortcut::activated, this, [this]() {
        if (refMenuPanel) refMenuPanel->openPreviewFind();
    });

    // Atalhos dos painéis principais (F2-F7, F12).
    auto* whiteboardShortcut = new QShortcut(QKeySequence(Qt::Key_F2), this);
    connect(whiteboardShortcut, &QShortcut::activated, this, [this]() {
        if (leftBar) leftBar->fixedActionTriggered(LeftBar::Whiteboard);
    });
    auto* timelineShortcut = new QShortcut(QKeySequence(Qt::Key_F3), this);
    connect(timelineShortcut, &QShortcut::activated, this, [this]() {
        if (leftBar) leftBar->fixedActionTriggered(LeftBar::Timeline);
    });
    auto* pensarioShortcut = new QShortcut(QKeySequence(Qt::Key_F4), this);
    connect(pensarioShortcut, &QShortcut::activated, this, [this]() {
        if (pensarioPanel) pensarioPanel->togglePanel();
    });
    auto* mapShortcut = new QShortcut(QKeySequence(QStringLiteral("Shift+F4")), this);
    connect(mapShortcut, &QShortcut::activated, this, [this]() {
        if (pensarioPanel) pensarioPanel->openMap();
    });
    auto* groupsShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(groupsShortcut, &QShortcut::activated, this, [this]() {
        if (leftBar) leftBar->fixedActionTriggered(LeftBar::Groups);
    });
    auto* refMenuShortcut = new QShortcut(QKeySequence(Qt::Key_F6), this);
    connect(refMenuShortcut, &QShortcut::activated, this, [this]() {
        if (refMenuPanel) refMenuPanel->togglePanel();
    });
    auto* remindersShortcut = new QShortcut(QKeySequence(Qt::Key_F7), this);
    connect(remindersShortcut, &QShortcut::activated, this, [this]() {
        if (!remindersPanel || !toolbar) return;
        if (remindersPanel->isVisible()) {
            remindersPanel->hide();
            return;
        }
        remindersPanel->showNear(toolbar->reminderButtonGlobalRect());
    });
    auto* homeShortcut = new QShortcut(QKeySequence(Qt::Key_F12), this);
    connect(homeShortcut, &QShortcut::activated, this, [this]() {
        openMainMenu();
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
        } else if (action == LeftBar::Whiteboard) {
            if (!lousaPanel) {
                lousaPanel = new LousaPanel(this);
                connect(lousaPanel, &LousaPanel::closeRequested, this, [this]() {
                    leftBar->clearSelection();
                });
                connect(lousaPanel, &LousaPanel::createTimelineEventRequested, this,
                        [this](const QString& title, const QString& description) {
                    auto* panel = ensureTimelinePanel();
                    panel->show();
                    panel->raise();
                    panel->activateWindow();
                    if (leftBar) leftBar->setActiveFixedAction(LeftBar::Timeline);
                    panel->promptNewEvent(description, QString(), title);
                });
                lousaPanel->setProjectModel(projectModel);
                lousaPanel->setElementsStore(elementsStore);
                if (!projectRoot.isEmpty())
                    lousaPanel->setProjectRoot(projectRoot);
            }
            if (lousaPanel->isVisible()) {
                lousaPanel->hide();
                leftBar->clearSelection();
            } else {
                lousaPanel->show();
                lousaPanel->raise();
                lousaPanel->activateWindow();
                leftBar->setActiveFixedAction(LeftBar::Whiteboard);
            }
        } else if (action == LeftBar::Timeline) {
            ensureTimelinePanel();
            if (timelinePanel->isVisible()) {
                timelinePanel->hide();
                leftBar->clearSelection();
            } else {
                timelinePanel->show();
                timelinePanel->raise();
                timelinePanel->activateWindow();
                leftBar->setActiveFixedAction(LeftBar::Timeline);
            }
        } else if (action == LeftBar::Groups) {
            if (!groupsPanel) {
                groupsPanel = new GroupsPanel(projectModel, this);
                connect(groupsPanel, &GroupsPanel::closeRequested, this, [this]() {
                    if (groupsPanel) groupsPanel->hide();
                    leftBar->clearSelection();
                });
                connect(groupsPanel, &GroupsPanel::itemActivated, this,
                        [this](const QString& /*drawerKey*/, const QString& itemId) {
                    EditorHost::ViewMode vm;
                    vm.type   = EditorHost::DrawerDoc;
                    vm.itemId = itemId;
                    editorHost->setViewMode(vm);
                });
            }
            if (groupsPanel->isVisible()) {
                groupsPanel->hide();
                leftBar->clearSelection();
            } else {
                const QToolButton* btn = leftBar->fixedButton(LeftBar::Groups);
                const QRect anchor = btn
                    ? QRect(btn->mapToGlobal(QPoint(0, 0)), btn->size())
                    : QRect(mapToGlobal(QPoint(60, 200)), QSize(1, 1));
                groupsPanel->showNear(anchor);
                leftBar->setActiveFixedAction(LeftBar::Groups);
            }
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
        QString title, marker;
        if (!promptChapterDialog(this, false, &title, &marker)) return;
        Chapter c;
        c.id = ProjectModel::uid();
        c.manuscriptId = manuscriptId;
        c.title = title;
        c.timeMarker = marker;
        if (!projectRoot.isEmpty()) {
            ProjectStorage::ensureManuscriptDirs(projectRoot, manuscriptId);
        }
        projectModel->addChapter(c);
    });

    // ---- Context menu / drag handlers do ManuscriptPanel ----
    connect(manuscriptPanel, &ManuscriptPanel::renameChapterRequested, this, [this](const QString& chapterId) {
        const Chapter* c = projectModel->findChapter(chapterId);
        if (!c) return;
        QString newTitle = c->title;
        QString newMarker = c->timeMarker;
        if (!promptChapterDialog(this, true, &newTitle, &newMarker)) return;
        projectModel->updateChapterTitle(chapterId, newTitle);
        projectModel->updateChapterTimeMarker(chapterId, newMarker);
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

    connect(manuscriptPanel, &ManuscriptPanel::openChapterInRefMenuRequested, this,
            [this](const QString& manuscriptId, const QString& chapterId) {
        if (refMenuPanel) refMenuPanel->openForChapter(manuscriptId, chapterId);
    });

    connect(manuscriptPanel, &ManuscriptPanel::openSceneInRefMenuRequested, this,
            [this](const QString& manuscriptId, const QString& chapterId, int sceneIndex) {
        if (refMenuPanel) refMenuPanel->openForScene(manuscriptId, chapterId, sceneIndex);
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
            elem.narrator = dlg.narrator();
            elem.trackMode = dlg.trackMode();
            elem.aliases = dlg.aliases();
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
        bool narratorVal = false;
        QString trackModeVal;
        QStringList aliasesVal;
        if (!item->elementId.isEmpty() && elementsStore) {
            if (const Element* e = elementsStore->findElement(item->elementId)) {
                imageDataUrl = e->image;
                if (role.isEmpty()) role = e->role;
                narratorVal = e->narrator;
                trackModeVal = e->trackMode;
                aliasesVal = e->aliases;
            }
        }
        dlg.setInitial(item->title, role, imageDataUrl, narratorVal, trackModeVal, aliasesVal);
        if (dlg.exec() != QDialog::Accepted) return;
        const QString newTitle = dlg.title();
        const QString newRole = dlg.role();
        const QString newImage = dlg.imageDataUrl();
        const bool newNarrator = dlg.narrator();
        const QString newTrack = dlg.trackMode();
        const QStringList newAliases = dlg.aliases();

        projectModel->updateDrawerItemMeta(itemId, newTitle, newRole);

        // Sincroniza Element vinculado, se houver. Foto só é editável quando há elementId.
        if (!item->elementId.isEmpty() && elementsStore) {
            if (const Element* existing = elementsStore->findElement(item->elementId)) {
                Element copy = *existing;
                copy.name = newTitle;
                copy.role = newRole;
                copy.image = newImage;
                copy.narrator = newNarrator;
                copy.trackMode = newTrack;
                copy.aliases = newAliases;
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
            elem.narrator = newNarrator;
            elem.trackMode = newTrack;
            elem.aliases = newAliases;
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
    connect(toolbar, &TopToolbar::fullscreenToggled, this, [this](bool enabled) {
        if (enabled) showFullScreen(); else showMaximized();
    });
    connect(toolbar, &TopToolbar::readModeToggled, this, &MainWindow::setReadMode);
    connect(toolbar, &TopToolbar::addImageRequested, this, &MainWindow::onAddImageRequested);
    connect(toolbar, &TopToolbar::mainMenuRequested, this, &MainWindow::openMainMenu);
    connect(toolbar, &TopToolbar::newProjectRequested, this, &MainWindow::onNewProjectRequested);
    connect(toolbar, &TopToolbar::openProjectRequested, this, &MainWindow::onOpenProjectRequested);
    connect(toolbar, &TopToolbar::saveProjectRequested, this, [this]() {
        if (projectSaver) projectSaver->saveProject();
    });
    connect(toolbar, &TopToolbar::settingsRequested, this, &MainWindow::onSettingsRequested);
    connect(toolbar, &TopToolbar::exportRequested, this, &MainWindow::onExportRequested);
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
    auto* emDashShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+-")), this);
    connect(emDashShortcut, &QShortcut::activated, this, [this]() {
        if (editor) editor->textCursor().insertText(QStringLiteral("—"));
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

void MainWindow::updateDocCachePinnedKeys()
{
    if (!docCache) return;
    QSet<QString> pinned;
    if (editorHost) {
        const QString k = editorHost->activeKey();
        if (!k.isEmpty()) pinned.insert(k);
    }
    if (refMenuPanel) {
        const QString k = refMenuPanel->selectedCacheKey();
        if (!k.isEmpty()) pinned.insert(k);
    }
    docCache->setPinnedKeys(pinned);
}

void MainWindow::applyEditorStyle()
{
    // Suspende o spell checker durante as operações em massa — sem isso, cada
    // mergeCharFormat dispara highlightBlock em todos os blocos afetados.
    if (spellHighlighter) spellHighlighter->suspend();

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
    const int opacity = qBound(0, Theme::editorOpacity(), 100);
    const QColor baseBg = QColor(Theme::editorBackground());
    QColor pageBg = baseBg;
    pageBg.setAlpha((opacity * 255) / 100);
    const QString pageBgCss = QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(pageBg.red()).arg(pageBg.green()).arg(pageBg.blue())
        .arg(QString::number(pageBg.alphaF(), 'f', 3));
    editor->setStyleSheet(QStringLiteral(
        "QTextEdit { background-color: %1; selection-background-color: %2; }")
        .arg(pageBgCss, Theme::accentDefault()));
    if (editor->viewport()) {
        // Viewport precisa ser transparente pro QSS (com alpha) aparecer;
        // se autoFillBackground=true, ele pinta a cor opaca por baixo.
        const bool wantTranslucent = opacity < 100;
        editor->viewport()->setAutoFillBackground(!wantTranslucent);
        QPalette vp = editor->viewport()->palette();
        vp.setColor(QPalette::Base, wantTranslucent ? Qt::transparent : baseBg);
        vp.setColor(QPalette::Window, wantTranslucent ? Qt::transparent : baseBg);
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

    // Força fonte/tamanho em todos os fragmentos — charFormats inline do HTML
    // (carregados do Mira 1) sobrepõem setDefaultFont e ignoram a escolha do usuário.
    applyEditorFont();

    if (spellHighlighter) spellHighlighter->resume();
}

void MainWindow::applyTextColor()
{
    if (!editor || !editor->document()) return;

    QColor textColor = baseTextColor;
    textColor.setAlpha(focusModeEnabled ? 100 : 255);

    QPalette p = editor->palette();
    p.setColor(QPalette::Text, textColor);
    editor->setPalette(p);

    const bool wasModified = editor->document()->isModified();
    QTextDocument* doc = editor->document();

    // Fast path: sem markers salvos para este doc, aplica foreground globalmente
    // num único mergeCharFormat em vez de iterar fragmento por fragmento.
    const QString activeKey = editorHost ? editorHost->activeKey() : QString();
    const bool hasMarkers = markerStore && markerStore->hasMarkersForKey(activeKey);
    if (!hasMarkers) {
        QTextCursor c(doc);
        c.select(QTextCursor::Document);
        QTextCharFormat fmt;
        fmt.setForeground(textColor);
        c.mergeCharFormat(fmt);
        doc->setModified(wasModified);
        if (editor->viewport()) editor->viewport()->update();
        return;
    }

    // Slow path: doc tem markers — itera por fragmento para preservar o foreground
    // contrastante (preto/branco) sobre os highlights coloridos.
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

void MainWindow::applyEditorFont()
{
    if (!editor || !editor->document()) return;
    const bool wasModified = editor->document()->isModified();
    QTextDocument* doc = editor->document();
    QTextCursor c(doc);
    c.select(QTextCursor::Document);
    QTextCharFormat fmt;
    fmt.setFontFamilies({currentFontFamily});
    fmt.setFontPointSize(currentFontSize);
    c.mergeCharFormat(fmt);
    doc->setModified(wasModified);
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
        setEditorSelectionsLayer(QStringLiteral("focus"), {});
    }
}

void MainWindow::updateFocusedBlock()
{
    QList<QTextEdit::ExtraSelection> selections;
    QTextBlock block = editor->textCursor().block();
    if (!block.isValid()) {
        setEditorSelectionsLayer(QStringLiteral("focus"), selections);
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
    setEditorSelectionsLayer(QStringLiteral("focus"), selections);
}

void MainWindow::navigateAdjacentChapter(int dir)
{
    if (!projectModel) return;
    const EditorHost::ViewMode vm = editorHost->viewMode();
    if (vm.type != EditorHost::ChapterDoc && vm.type != EditorHost::SceneDoc) return;

    // Capítulos do manuscrito ativo em ordem de lista
    QList<Chapter> chapters;
    for (const auto& c : projectModel->chapters())
        if (c.manuscriptId == vm.manuscriptId) chapters.append(c);
    if (chapters.isEmpty()) return;

    int idx = -1;
    for (int i = 0; i < chapters.size(); ++i)
        if (chapters[i].id == vm.chapterId) { idx = i; break; }
    if (idx == -1) return;

    const int target = idx + dir;
    if (target < 0 || target >= chapters.size()) return;

    EditorHost::ViewMode next;
    next.type        = EditorHost::ChapterDoc;
    next.manuscriptId = vm.manuscriptId;
    next.chapterId   = chapters[target].id;

    m_autoNavCooldown = true;
    editorHost->setViewMode(next);

    // Ao ir para frente, o doc já carrega no topo (padrão do QTextEdit).
    // Ao ir para trás, aguarda o layout finalizar via documentSizeChanged para
    // rolar até o final — evita o delay visível de singleShot com valor fixo.
    if (dir < 0) {
        connect(editor->document()->documentLayout(),
                &QAbstractTextDocumentLayout::documentSizeChanged,
                this, [this](const QSizeF&) {
            QTimer::singleShot(0, this, [this]() {
                editor->verticalScrollBar()->setValue(
                    editor->verticalScrollBar()->maximum());
                m_autoNavCooldown = false;
            });
        }, Qt::SingleShotConnection);
    } else {
        QTimer::singleShot(0, this, [this]() { m_autoNavCooldown = false; });
    }
}

void MainWindow::setReadMode(bool enabled)
{
    readModeEnabled = enabled;

    // Painéis flutuantes que somem no modo focado.
    if (drawerListPanel) drawerListPanel->hide();
    if (manuscriptPanel) manuscriptPanel->hide();
    if (refMenuPanel && refMenuPanel->isVisible()) refMenuPanel->hide();
    if (leftBar) leftBar->clearSelection();

    if (enabled) {
        // Toolbar flutuante: esconder o holder inteiro libera o topo da janela.
        if (toolbarHolder) toolbarHolder->hide();
        updateEditorContainerMargins();
        if (leftBar) leftBar->setChromeHidden(true);
        if (wordCountPanel) wordCountPanel->hide();

        // Hotzones finas e transparentes pra revelar cada barra no hover.
        if (!readModeHotTop) {
            readModeHotTop = new QWidget(this);
            readModeHotTop->setObjectName(QStringLiteral("readModeHotTop"));
            readModeHotTop->setAttribute(Qt::WA_NoSystemBackground, true);
            readModeHotTop->installEventFilter(this);
        }
        if (!readModeHotLeft && editorContainer) {
            readModeHotLeft = new QWidget(editorContainer);
            readModeHotLeft->setObjectName(QStringLiteral("readModeHotLeft"));
            readModeHotLeft->setAttribute(Qt::WA_NoSystemBackground, true);
            readModeHotLeft->installEventFilter(this);
        }
        positionReadModeHotzones();
        if (readModeHotTop) { readModeHotTop->show(); readModeHotTop->raise(); }
        if (readModeHotLeft) { readModeHotLeft->show(); readModeHotLeft->raise(); }
    } else {
        if (toolbarHolder) {
            toolbarHolder->show();
            toolbarHolder->raise();
        }
        updateEditorContainerMargins();
        if (leftBar) leftBar->setChromeHidden(false);
        if (wordCountPanel && hasProjectLoaded()) {
            wordCountPanel->show();
            positionWordCountPanel();
        }
        if (readModeHotTop) readModeHotTop->hide();
        if (readModeHotLeft) readModeHotLeft->hide();
    }

    if (editor) editor->setFocus();
}

void MainWindow::positionReadModeHotzones()
{
    if (readModeHotTop) {
        readModeHotTop->setGeometry(0, 0, width(), 6);
        readModeHotTop->raise();
    }
    if (readModeHotLeft && editorContainer) {
        readModeHotLeft->setGeometry(0, 0, 6, editorContainer->height());
        readModeHotLeft->raise();
    }
}

void MainWindow::setEditorSelectionsLayer(const QString& layer,
                                          const QList<QTextEdit::ExtraSelection>& sels)
{
    if (!editor) return;
    if (sels.isEmpty()) editorSelectionLayers.remove(layer);
    else editorSelectionLayers[layer] = sels;
    QList<QTextEdit::ExtraSelection> total;
    for (auto it = editorSelectionLayers.constBegin();
         it != editorSelectionLayers.constEnd(); ++it) {
        total += it.value();
    }
    editor->setExtraSelections(total);
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
    // Modo focado: a hotzone revela a barra (o conteúdo reaparece no espaço
    // já reservado, sem reflow); sair da barra a esconde de novo.
    if (readModeEnabled) {
        if (watched == readModeHotTop && event->type() == QEvent::Enter) {
            if (toolbarHolder) { toolbarHolder->show(); toolbarHolder->raise(); }
            return false;
        }
        if (watched == readModeHotLeft && event->type() == QEvent::Enter) {
            if (leftBar) leftBar->setChromeHidden(false);
            return false;
        }
        if (toolbar && watched == toolbar && event->type() == QEvent::Leave) {
            const QPoint g = QCursor::pos();
            if (!toolbar->rect().contains(toolbar->mapFromGlobal(g)))
                if (toolbarHolder) toolbarHolder->hide();
        }
        if (leftBar && watched == leftBar && event->type() == QEvent::Leave) {
            const QPoint g = QCursor::pos();
            if (!leftBar->rect().contains(leftBar->mapFromGlobal(g)))
                leftBar->setChromeHidden(true);
        }
    }

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
        } else if (event->type() == QEvent::Wheel) {
            // Wheel: exige 2 ticks extras além do limite para ativar a zona de nav.
            // Rolar na direção oposta cancela e zera o acúmulo.
            // (Um tick padrão de mouse = 120 unidades de angleDelta; threshold = 240.)
            if (!m_autoNavCooldown && m_autoNavTimer && !m_sliderHeld) {
                const auto vm = editorHost->viewMode();
                if (vm.type == EditorHost::ChapterDoc || vm.type == EditorHost::SceneDoc) {
                    auto* sb  = editor->verticalScrollBar();
                    const int max = sb->maximum();
                    if (max > 0) {
                        const int dy = static_cast<QWheelEvent*>(event)->angleDelta().y();
                        const int v  = sb->value();
                        if (dy > 0 && v == 0) {
                            m_wheelOverscroll += qAbs(dy);
                            if (m_wheelOverscroll >= 240) activateNavZone(-1, vm);
                        } else if (dy < 0 && v == max) {
                            m_wheelOverscroll += qAbs(dy);
                            if (m_wheelOverscroll >= 240) activateNavZone( 1, vm);
                        } else if (dy != 0) {
                            if (m_autoNavDir != 0) deactivateNavZone();
                            else m_wheelOverscroll = 0;
                        }
                    }
                }
            }
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
    if (globalSearchPanel) globalSearchPanel->setProjectRoot(root);
    if (elementsStore) elementsStore->setProjectRoot(root);
    if (spellChecker) spellChecker->setProjectRoot(root);
    if (markerStore) {
        markerStore->setProjectRoot(root);
        markerStore->load();
    }
    if (notesStore) {
        notesStore->setProjectRoot(root);
        notesStore->load();
    }
    if (mapPinsStore) {
        mapPinsStore->setProjectRoot(root);
        mapPinsStore->load();
    }
    if (pensarioPanel) pensarioPanel->refresh();
    if (glossaryStore) {
        glossaryStore->setProjectRoot(root);
        glossaryStore->load();
        if (spellChecker) spellChecker->setGlossaryWords(glossaryStore->terms());
    }
    if (memoriesStore) {
        memoriesStore->setProjectRoot(root);
        if (pensarioPanel) pensarioPanel->setMemoriesStore(memoriesStore);
        memoriesStore->load();
    }
    if (lousaPanel) {
        lousaPanel->setProjectModel(projectModel);
        lousaPanel->setElementsStore(elementsStore);
        lousaPanel->setProjectRoot(root);
    }
    if (timelinePanel) {
        timelinePanel->setProjectModel(projectModel);
        timelinePanel->setProjectRoot(root);
    }

    if (remindersStore) {
        remindersStore->setProjectRoot(root);
        remindersStore->load();
        if (toolbar) toolbar->setReminderBadge(remindersStore->hasActive());
        // Toast de boas-vindas: lista os lembretes ativos ao abrir o projeto.
        if (remindersStore->hasActive()) {
            QTimer::singleShot(800, this, [this]() {
                if (!remindersStore) return;
                const QVector<Reminder> actives = remindersStore->active();
                if (actives.isEmpty()) return;
                const int n = actives.size();
                const QString title = n == 1
                    ? tr("Lembrete")
                    : tr("Lembretes (%1)").arg(n);
                QStringList lines;
                for (int i = 0; i < qMin(n, 3); ++i)
                    lines << actives[i].text;
                if (n > 3) lines << tr("+ %1 mais").arg(n - 3);
                showReminderToast(title, lines.join(QStringLiteral("\n")));
            });
        }
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

    // Restaura estado de detecção (markAll) salvo por projeto
    {
        QSettings qs;
        qs.beginGroup(QStringLiteral("detectionState"));
        const QString phash = QString::fromLatin1(
            QCryptographicHash::hash(root.toUtf8(), QCryptographicHash::Md5).toHex());
        qs.beginGroup(phash);
        detectionMarkAll = qs.value(QStringLiteral("markAll"), false).toBool();
        qs.endGroup();
        qs.endGroup();
        if (settingsPanel) settingsPanel->setDetectionMarkAll(detectionMarkAll);
    }

    rememberLastProject(root);
    restoreLastDocFor(root);

    // Mostra a janela principal agora que tem projeto pra editar. Útil quando
    // o app começou escondido (sem autoOpen) e o user escolheu pelo menu.
    if (!isVisible()) show();
    return true;
}

void MainWindow::rememberLastProject(const QString& root)
{
    QSettings settings;
    settings.setValue(QStringLiteral("lastProjectPath"), root);

    // Atualiza lista de recentes: move/insere no topo, limita a 12.
    QStringList list = settings.value(QStringLiteral("recentProjects/list"))
                           .toStringList();
    const QString clean = QDir::cleanPath(root);
    list.removeAll(clean);
    for (int i = list.size() - 1; i >= 0; --i) {
        if (QDir::cleanPath(list.at(i)) == clean) list.removeAt(i);
    }
    list.prepend(clean);
    while (list.size() > 12) list.removeLast();
    settings.setValue(QStringLiteral("recentProjects/list"), list);
}

QString MainWindow::loadLastProjectPath() const
{
    QSettings settings;
    return settings.value(QStringLiteral("lastProjectPath")).toString();
}

QStringList MainWindow::loadRecentProjects() const
{
    QSettings settings;
    QStringList list = settings.value(QStringLiteral("recentProjects/list"))
                           .toStringList();
    // Compat: se a lista não existir, semeia com o último projeto, se houver.
    if (list.isEmpty()) {
        const QString last = settings.value(QStringLiteral("lastProjectPath")).toString();
        if (!last.isEmpty()) list << QDir::cleanPath(last);
    }
    // Filtra paths que não existem mais — não regrava (deixa pro próximo save).
    QStringList alive;
    for (const QString& p : list) {
        if (!p.isEmpty() && QDir(p).exists()) alive << p;
    }
    return alive;
}

void MainWindow::saveRecentProjects(const QStringList& list)
{
    QSettings settings;
    settings.setValue(QStringLiteral("recentProjects/list"), list);
}

void MainWindow::showReminderToast(const QString& title, const QString& body)
{
    // Constrói o widget na primeira chamada.
    if (!m_reminderToast) {
        m_reminderToast = new QFrame(this);
        m_reminderToast->setObjectName(QStringLiteral("reminderToast"));
        m_reminderToast->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        m_reminderToast->setFixedWidth(300);

        auto* shadow = new QGraphicsDropShadowEffect(m_reminderToast);
        shadow->setBlurRadius(18);
        shadow->setColor(QColor(0, 0, 0, 160));
        shadow->setOffset(0, 3);
        m_reminderToast->setGraphicsEffect(shadow);

        auto* root = new QVBoxLayout(m_reminderToast);
        root->setContentsMargins(14, 10, 14, 10);
        root->setSpacing(4);

        auto* topRow = new QHBoxLayout();
        topRow->setSpacing(6);
        m_reminderToastTitle = new QLabel(m_reminderToast);
        m_reminderToastTitle->setObjectName(QStringLiteral("rtTitle"));
        topRow->addWidget(m_reminderToastTitle, 1);
        auto* closeBtn = new QToolButton(m_reminderToast);
        closeBtn->setText(QStringLiteral("×"));
        closeBtn->setObjectName(QStringLiteral("rtClose"));
        closeBtn->setCursor(Qt::PointingHandCursor);
        connect(closeBtn, &QToolButton::clicked, m_reminderToast, &QFrame::hide);
        topRow->addWidget(closeBtn);
        root->addLayout(topRow);

        m_reminderToastBody = new QLabel(m_reminderToast);
        m_reminderToastBody->setObjectName(QStringLiteral("rtBody"));
        m_reminderToastBody->setWordWrap(true);
        root->addWidget(m_reminderToastBody);

        m_reminderToastTimer = new QTimer(this);
        m_reminderToastTimer->setSingleShot(true);
        connect(m_reminderToastTimer, &QTimer::timeout,
                m_reminderToast, &QFrame::hide);

        m_reminderToast->hide();
    }

    // Aplica tema.
    m_reminderToast->setStyleSheet(QStringLiteral(
        "QFrame#reminderToast {"
        "  background: %1; border: 1px solid %2; border-radius: 10px;"
        "}"
        "QLabel#rtTitle { color: %3; font-size: 13px; font-weight: 600; }"
        "QLabel#rtBody  { color: %4; font-size: 12px; }"
        "QToolButton#rtClose {"
        "  background: transparent; border: none;"
        "  color: %4; font-size: 16px; font-weight: 300;"
        "}"
        "QToolButton#rtClose:hover { color: %3; }"
    ).arg(Theme::panelBackground(),
         Theme::panelBorder(),
         Theme::textPrimary(),
         Theme::textMuted()));

    m_reminderToastTitle->setText(title);
    m_reminderToastBody->setText(body);
    m_reminderToastBody->setVisible(!body.isEmpty());

    m_reminderToast->adjustSize();
    positionReminderToast();
    m_reminderToast->show();
    m_reminderToast->raise();
    m_reminderToastTimer->start(5000);
}

void MainWindow::positionReminderToast()
{
    if (!m_reminderToast) return;
    const int margin = 16;
    const QRect r = rect();
    m_reminderToast->move(
        r.right()  - m_reminderToast->width()  - margin,
        r.bottom() - m_reminderToast->height() - margin);
}

static QString formatUpdateNotes(const QString& markdown, int maxChars = 300)
{
    QStringList out;
    for (const QString& raw : markdown.split(QLatin1Char('\n'))) {
        QString line = raw.trimmed();
        if (line.startsWith(QLatin1Char('#'))) continue; // pula headings
        // Bullets: "- " ou "* " → "•"
        if ((line.startsWith(QStringLiteral("- ")) || line.startsWith(QStringLiteral("* "))))
            line = QStringLiteral("• ") + line.mid(2);
        // Remove marcadores **bold** e *italic*
        static const QRegularExpression boldRe(QStringLiteral("\\*{1,3}([^*\\n]+)\\*{1,3}"));
        line.replace(boldRe, QStringLiteral("\\1"));
        out.append(line);
    }
    while (!out.isEmpty() && out.first().isEmpty()) out.removeFirst();
    while (!out.isEmpty() && out.last().isEmpty())  out.removeLast();

    QString text = out.join(QLatin1Char('\n'));
    // Colapsa 3+ quebras de linha em 2
    static const QRegularExpression blankRe(QStringLiteral("\\n{3,}"));
    text.replace(blankRe, QStringLiteral("\n\n"));
    text = text.trimmed();

    if (text.length() <= maxChars) return text;
    int pos = text.lastIndexOf(QLatin1Char('\n'), maxChars);
    if (pos < maxChars / 2) pos = maxChars;
    return text.left(pos).trimmed() + QStringLiteral("\n…");
}

void MainWindow::showUpdateToast(const QString& version, const QString& downloadUrl, const QString& releaseNotes)
{
    m_updateVersion = version;
    m_updateDownloadUrl = downloadUrl;

    if (!m_updateToast) {
        m_updateToast = new QFrame(this);
        m_updateToast->setObjectName(QStringLiteral("updateToast"));
        m_updateToast->setFixedWidth(300);

        auto* shadow = new QGraphicsDropShadowEffect(m_updateToast);
        shadow->setBlurRadius(18);
        shadow->setColor(QColor(0, 0, 0, 160));
        shadow->setOffset(0, 3);
        m_updateToast->setGraphicsEffect(shadow);

        auto* root = new QVBoxLayout(m_updateToast);
        root->setContentsMargins(14, 10, 14, 10);
        root->setSpacing(8);

        auto* topRow = new QHBoxLayout();
        topRow->setSpacing(6);
        m_updateToastLabel = new QLabel(m_updateToast);
        m_updateToastLabel->setObjectName(QStringLiteral("utTitle"));
        m_updateToastLabel->setWordWrap(true);
        topRow->addWidget(m_updateToastLabel, 1);
        auto* closeBtn = new QToolButton(m_updateToast);
        closeBtn->setText(QStringLiteral("×"));
        closeBtn->setObjectName(QStringLiteral("utClose"));
        closeBtn->setCursor(Qt::PointingHandCursor);
        connect(closeBtn, &QToolButton::clicked, m_updateToast, &QFrame::hide);
        topRow->addWidget(closeBtn);
        root->addLayout(topRow);

        m_updateToastNotes = new QLabel(m_updateToast);
        m_updateToastNotes->setObjectName(QStringLiteral("utNotes"));
        m_updateToastNotes->setWordWrap(true);
        m_updateToastNotes->hide();
        root->addWidget(m_updateToastNotes);

        m_updateToastProgress = new QProgressBar(m_updateToast);
        m_updateToastProgress->setObjectName(QStringLiteral("utProgress"));
        m_updateToastProgress->setRange(0, 100);
        m_updateToastProgress->setTextVisible(false);
        m_updateToastProgress->setFixedHeight(6);
        m_updateToastProgress->hide();
        root->addWidget(m_updateToastProgress);

        m_updateToastBtn = new QToolButton(m_updateToast);
        m_updateToastBtn->setObjectName(QStringLiteral("utAction"));
        m_updateToastBtn->setCursor(Qt::PointingHandCursor);
        connect(m_updateToastBtn, &QToolButton::clicked, this, &MainWindow::startUpdateDownload);
        root->addWidget(m_updateToastBtn);

        m_updateToast->hide();
    }

    m_updateToast->setStyleSheet(QStringLiteral(
        "QFrame#updateToast {"
        "  background: %1; border: 1px solid %2; border-radius: 10px;"
        "}"
        "QLabel#utTitle { color: %3; font-size: 13px; font-weight: 600; }"
        "QLabel#utNotes { color: %4; font-size: 11px; }"
        "QToolButton#utClose {"
        "  background: transparent; border: none;"
        "  color: %4; font-size: 16px; font-weight: 300;"
        "}"
        "QToolButton#utClose:hover { color: %3; }"
        "QToolButton#utAction {"
        "  background: %5; color: %6; border: none; border-radius: 6px;"
        "  padding: 6px 10px; font-size: 12px; font-weight: 600;"
        "}"
        "QToolButton#utAction:hover { background: %7; }"
        "QProgressBar#utProgress {"
        "  background: %2; border: none; border-radius: 3px;"
        "}"
        "QProgressBar#utProgress::chunk { background: %5; border-radius: 3px; }"
    ).arg(Theme::panelBackground(),
         Theme::panelBorder(),
         Theme::textPrimary(),
         Theme::textMuted(),
         Theme::accentInfo(),
         Theme::panelBackground(),
         Theme::accentInfo()));

    m_updateToastLabel->setText(tr("Nova versão disponível: %1").arg(version));

    const QString notes = formatUpdateNotes(releaseNotes);
    if (!notes.isEmpty()) {
        m_updateToastNotes->setText(notes);
        m_updateToastNotes->show();
    } else {
        m_updateToastNotes->hide();
    }

    m_updateToastBtn->setText(tr("Baixar e instalar"));
    m_updateToastBtn->setEnabled(true);
    m_updateToastProgress->setValue(0);
    m_updateToastProgress->hide();

    m_updateToast->adjustSize();
    positionUpdateToast();
    m_updateToast->show();
    m_updateToast->raise();
}

void MainWindow::positionUpdateToast()
{
    if (!m_updateToast) return;
    const int margin = 16;
    const QRect r = rect();
    int bottom = r.bottom() - margin;
    if (m_reminderToast && m_reminderToast->isVisible()) {
        bottom = m_reminderToast->y() - margin;
    }
    m_updateToast->move(
        r.right() - m_updateToast->width() - margin,
        bottom - m_updateToast->height());
}

void MainWindow::startUpdateDownload()
{
    if (m_updateDownloadUrl.isEmpty() || !m_updateToastBtn || !m_updateToastProgress) return;

    m_updateToastBtn->setEnabled(false);
    m_updateToastBtn->setText(tr("Baixando…"));
    m_updateToastProgress->setValue(0);
    m_updateToastProgress->show();
    m_updateToast->adjustSize();
    positionUpdateToast();

    if (!m_updateNam) m_updateNam = new QNetworkAccessManager(this);

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString fileName = QStringLiteral("mira-writing-setup-%1.exe").arg(m_updateVersion);
    const QString destPath = QDir(tempDir).filePath(fileName);

    auto* file = new QFile(destPath, this);
    if (!file->open(QIODevice::WriteOnly)) {
        delete file;
        m_updateToastBtn->setEnabled(true);
        m_updateToastBtn->setText(tr("Baixar e instalar"));
        m_updateToastProgress->hide();
        return;
    }

    QNetworkRequest req{QUrl(m_updateDownloadUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("mira-writing-update-checker"));
    QNetworkReply* reply = m_updateNam->get(req);

    connect(reply, &QNetworkReply::readyRead, this, [reply, file]() {
        file->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                if (total <= 0 || !m_updateToastProgress) return;
                m_updateToastProgress->setValue(static_cast<int>(received * 100 / total));
            });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, destPath]() {
        file->write(reply->readAll());
        file->close();
        const bool ok = reply->error() == QNetworkReply::NoError;
        reply->deleteLater();
        file->deleteLater();

        if (!ok) {
            QFile::remove(destPath);
            if (m_updateToastBtn) {
                m_updateToastBtn->setEnabled(true);
                m_updateToastBtn->setText(tr("Baixar e instalar"));
            }
            if (m_updateToastProgress) m_updateToastProgress->hide();
            return;
        }

        QProcess::startDetached(destPath, {});
        qApp->quit();
    });
}

void MainWindow::openMainMenu()
{
    if (!mainMenuDialog) {
        // Sem parent: garante que vire uma janela top-level real do Windows
        // (item de taskbar próprio, minimizar/restaurar nativos). Quando o
        // dialog tem parent, o Windows o trata como "owned window" e ele só
        // aparece na taskbar do dono — o que falha se o MainWindow estiver
        // hidden, deixando o menu impossível de recuperar após minimizar.
        mainMenuDialog = new MainMenuDialog(nullptr);

        connect(mainMenuDialog, &MainMenuDialog::autoOpenChanged,
                this, [this](const QString& path, bool enabled) {
                    QSettings s;
                    if (enabled) {
                        s.setValue(QStringLiteral("autoOpenProject"),
                                   QDir::cleanPath(path));
                        QMessageBox::information(mainMenuDialog,
                            tr("Abertura automática ativada"),
                            tr("O app agora sempre abrirá esse projeto de forma "
                               "automática. Caso queira desabilitar isso depois, "
                               "basta desmarcar essa opção."));
                    } else {
                        s.remove(QStringLiteral("autoOpenProject"));
                    }
                });

        connect(mainMenuDialog, &MainMenuDialog::newProjectRequested,
                this, [this]() {
                    onNewProjectRequested();
                    if (mainMenuDialog && !projectRoot.isEmpty()) {
                        mainMenuDialog->accept();
                    }
                });
        connect(mainMenuDialog, &MainMenuDialog::loadProjectRequested,
                this, [this]() {
                    if (!confirmDiscardOrSave()) return;
                    const QString startDir = QStandardPaths::writableLocation(
                        QStandardPaths::DocumentsLocation);
                    const QString chosen = QFileDialog::getExistingDirectory(
                        mainMenuDialog,
                        tr("Abrir projeto"),
                        startDir,
                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
                    if (chosen.isEmpty()) return;
                    QString err;
                    if (!loadProjectFrom(chosen, &err)) {
                        QMessageBox::warning(mainMenuDialog, tr("Erro ao abrir"),
                            tr("Não foi possível abrir o projeto:\n%1").arg(err));
                        return;
                    }
                    if (mainMenuDialog) mainMenuDialog->accept();
                });
        connect(mainMenuDialog, &MainMenuDialog::openRecentRequested,
                this, [this](const QString& path) {
                    if (!confirmDiscardOrSave()) return;
                    QString err;
                    if (!loadProjectFrom(path, &err)) {
                        // Path caiu — remove dos recentes e atualiza a lista.
                        QStringList list = loadRecentProjects();
                        list.removeAll(path);
                        list.removeAll(QDir::cleanPath(path));
                        saveRecentProjects(list);
                        if (mainMenuDialog) mainMenuDialog->setRecentProjects(list);
                        QMessageBox::warning(mainMenuDialog, tr("Erro ao abrir"),
                            tr("Não foi possível abrir o projeto:\n%1").arg(err));
                        return;
                    }
                    if (mainMenuDialog) mainMenuDialog->accept();
                });

        // Remover dos recentes — só tira da lista, não toca no disco.
        connect(mainMenuDialog, &MainMenuDialog::removeRecentRequested,
                this, [this](const QString& path) {
                    QStringList list = loadRecentProjects();
                    list.removeAll(path);
                    list.removeAll(QDir::cleanPath(path));
                    saveRecentProjects(list);
                    if (mainMenuDialog) mainMenuDialog->setRecentProjects(list);
                });

        // Excluir projeto — apaga a pasta do disco (a confirmação com countdown
        // já passou no MainMenuDialog) e atualiza recentes + autoOpen.
        connect(mainMenuDialog, &MainMenuDialog::deleteProjectRequested,
                this, [this](const QString& path) {
                    const QString clean = QDir::cleanPath(path);
                    // Bloqueia excluir o projeto que está aberto agora.
                    if (!projectRoot.isEmpty()
                        && QDir::cleanPath(projectRoot) == clean) {
                        QMessageBox::warning(mainMenuDialog, tr("Não é possível excluir"),
                            tr("Este é o projeto aberto no momento. Feche-o antes de excluí-lo."));
                        return;
                    }
                    QDir dir(path);
                    if (dir.exists() && !dir.removeRecursively()) {
                        QMessageBox::warning(mainMenuDialog, tr("Erro ao excluir"),
                            tr("Não foi possível apagar a pasta do projeto."));
                        return;
                    }
                    QStringList list = loadRecentProjects();
                    list.removeAll(path);
                    list.removeAll(clean);
                    saveRecentProjects(list);
                    QSettings s;
                    if (QDir::cleanPath(s.value(QStringLiteral("autoOpenProject")).toString()) == clean) {
                        s.remove(QStringLiteral("autoOpenProject"));
                    }
                    if (mainMenuDialog) mainMenuDialog->setRecentProjects(list);
                });
    }
    mainMenuDialog->setRecentProjects(loadRecentProjects());
    mainMenuDialog->setAutoOpenPath(
        QSettings().value(QStringLiteral("autoOpenProject")).toString());
    mainMenuDialog->show();
    mainMenuDialog->raise();
    mainMenuDialog->activateWindow();
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

    // Etapa 1 — template.
    NewProjectTemplateDialog tplDlg(this);
    if (tplDlg.exec() != QDialog::Accepted) return;
    const QString templateId = tplDlg.templateId();

    // Etapa 2 — detalhes da obra.
    NewProjectDetailsDialog detailsDlg(this);
    if (detailsDlg.exec() != QDialog::Accepted) return;

    // Etapa 3 — pasta-pai.
    NewProjectFolderDialog folderDlg(detailsDlg.projectName(), this);
    if (folderDlg.exec() != QDialog::Accepted) return;
    const QString fullPath = folderDlg.fullPath();

    if (QDir(fullPath).exists() && !QDir(fullPath).isEmpty()) {
        const auto answer = QMessageBox::question(this, tr("Pasta já existe"),
            tr("A pasta '%1' já existe e não está vazia. Usar mesmo assim?\n"
               "Conteúdo existente pode ser sobrescrito.").arg(fullPath),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
    }

    QString err;
    if (!ProjectStorage::ensureProjectDirs(fullPath, &err)) {
        QMessageBox::warning(this, tr("Erro"),
            tr("Falha ao criar o projeto:\n%1").arg(err));
        return;
    }

    // Monta o projeto em memória.
    if (editorHost) editorHost->disable();
    if (docCache) docCache->clear();
    if (projectModel) projectModel->clear();
    if (elementsStore) elementsStore->clear();
    projectModel->setProjectName(detailsDlg.projectName());
    projectModel->seedFromTemplate(templateId);
    projectModel->setProjectDetails(
        detailsDlg.projectName(),
        detailsDlg.author(),
        detailsDlg.genres(),
        detailsDlg.synopsis(),
        detailsDlg.coverDataUrl());
    applyProjectRoot(fullPath);

    if (projectSaver && !projectSaver->saveProject()) {
        QMessageBox::warning(this, tr("Erro"),
            tr("Projeto criado, mas falha ao salvar índice inicial:\n%1")
                .arg(projectSaver->lastError()));
    }
    rememberLastProject(fullPath);

    // Fecha o Main Menu se estiver aberto e mostra a janela principal.
    if (mainMenuDialog && mainMenuDialog->isVisible()) mainMenuDialog->accept();
    if (!isVisible()) show();
}

void MainWindow::positionWordCountPanel()
{
    if (!wordCountPanel || !wordCountPanel->parentWidget()) return;
    QWidget* parent = wordCountPanel->parentWidget();

    // Altura disponível entre a base da TopToolbar e a base do container. O painel
    // (scroll interno) é limitado a isso, então o conteúdo nunca passa atrás da
    // toolbar — quando é maior, rola (e abre já mostrando o calendário no fim).
    int topSafe = 0;
    if (toolbarHolder && toolbarHolder->isVisible()) {
        topSafe = parent->mapFromGlobal(
            toolbarHolder->mapToGlobal(QPoint(0, toolbarHolder->height()))).y();
    }
    const int bottomMargin = 10;
    const int toggleH = 18;   // altura do botão ▼ que fica fora do scroll
    const int avail = parent->height() - topSafe - bottomMargin - toggleH;
    wordCountPanel->setAvailableHeight(avail);

    wordCountPanel->adjustSize();
    const int leftBarOffset = LeftBar::barWidth() + 10 + 10; // largura LeftBar + spacing do layout + margem
    const int x = leftBarOffset;
    const int y = parent->height() - wordCountPanel->height() - bottomMargin;
    wordCountPanel->move(x, y);
}

void MainWindow::positionFindBar()
{
    if (!findBar || !editorContainer) return;
    findBar->adjustSize();
    const int margin = 12;
    const int tbH = (toolbarHolder && toolbarHolder->isVisible()) ? toolbarHolder->height() : 0;
    const int w = qMax(360, findBar->sizeHint().width());
    findBar->resize(w, findBar->height());
    const int x = editorContainer->width() - w - margin;
    const int y = tbH + margin;
    findBar->move(qMax(margin, x), y);
    if (findBar->isVisible()) findBar->raise();
}

void MainWindow::positionGlobalSearchPanel()
{
    if (!globalSearchPanel || !editorContainer) return;
    globalSearchPanel->adjustSize();
    const int margin = 12;
    const int tbH = (toolbarHolder && toolbarHolder->isVisible()) ? toolbarHolder->height() : 0;
    const int w = globalSearchPanel->width();
    const int h = qMin(520, editorContainer->height() - tbH - margin * 2);
    globalSearchPanel->resize(w, h);
    const int x = qMax(margin, (editorContainer->width() - w) / 2);
    const int y = tbH + margin + 8;
    globalSearchPanel->move(x, y);
    if (globalSearchPanel->isVisible()) globalSearchPanel->raise();
}

void MainWindow::activateNavZone(int dir, const EditorHost::ViewMode& vm)
{
    if (!m_autoNavEnabled) return;
    if (dir == m_autoNavDir) return; // já ativo nesta direção — timer continua
    m_autoNavDir = dir;
    m_autoNavTargetTitle.clear();
    QList<Chapter> filtered;
    for (const auto& c : projectModel->chapters())
        if (c.manuscriptId == vm.manuscriptId) filtered.append(c);
    for (int i = 0; i < filtered.size(); ++i) {
        if (filtered[i].id == vm.chapterId) {
            const int ti = i + dir;
            if (ti >= 0 && ti < filtered.size())
                m_autoNavTargetTitle = filtered[ti].title;
            break;
        }
    }
    if (m_autoNavHint) {
        const QString verb = (dir < 0) ? tr("Voltando para") : tr("Avançando para");
        const QString lbl  = m_autoNavTargetTitle.isEmpty()
            ? verb
            : QStringLiteral("%1: %2").arg(verb, m_autoNavTargetTitle);
        m_autoNavHint->setText(lbl);
        m_autoNavHint->setProgress(0.0f);
        m_autoNavProgressMs = 0;
        positionAutoNavHint();
        m_autoNavHint->show();
        m_autoNavHint->raise();
    }
    if (m_autoNavTimer)        m_autoNavTimer->start();
    if (m_autoNavProgressTimer) m_autoNavProgressTimer->start();
}

void MainWindow::deactivateNavZone()
{
    if (m_autoNavTimer)        m_autoNavTimer->stop();
    if (m_autoNavProgressTimer) m_autoNavProgressTimer->stop();
    if (m_autoNavHint)         m_autoNavHint->hide();
    m_autoNavDir      = 0;
    m_wheelOverscroll = 0;
}

void MainWindow::positionExternalScrollBar()
{
    if (!externalScrollBar || !editorScroll || !editorContainer || !editorColumn) return;
    QWidget* vp = editorScroll->viewport();
    // Coloca a scrollbar imediatamente à direita da página. Como a largura da
    // página é configurável, usamos mapTo para seguir a posição real do editorColumn.
    const QPoint colRight      = editorColumn->mapTo(vp, QPoint(editorColumn->width(), 0));
    const QPoint vpInContainer = vp->mapTo(editorContainer, QPoint(0, 0));
    const int sbW = style()->pixelMetric(QStyle::PM_ScrollBarExtent);

    // O track tem altura FIXA (máx 800px), centralizado verticalmente na viewport
    // — desacoplado do comprimento da página. Se ficasse do tamanho da página, com
    // página comprida o fundo do track sairia da tela e arrastar até o fundo
    // visível não chegava ao fim do conteúdo. O thumb deriva de range/pageStep do
    // scroll interno do editor (que cobre o documento inteiro), então arrastar o
    // thumb por todo o track de 800px percorre o documento todo, qualquer que seja
    // o comprimento da página.
    constexpr int kMaxScrollTrack = 800;
    const int sbH = qMin(kMaxScrollTrack, vp->height());
    const int sbY = vpInContainer.y() + (vp->height() - sbH) / 2;
    externalScrollBar->setGeometry(
        vpInContainer.x() + colRight.x(),
        sbY,
        sbW,
        sbH
    );
    externalScrollBar->raise();
    positionAutoNavHint();
}

void MainWindow::positionAutoNavHint()
{
    if (!m_autoNavHint || !externalScrollBar) return;
    // Sem guard isVisible(): precisa posicionar antes do show() em activateNavZone.
    // externalScrollBar e m_autoNavHint são filhos diretos de editorContainer,
    // então geometry() já está no sistema de coordenadas correto.
    const QRect sb   = externalScrollBar->geometry();
    const int hintW  = m_autoNavHint->width();
    const int hintH  = m_autoNavHint->height();
    const int x      = sb.right() + 4;
    const int y      = (m_autoNavDir < 0)
        ? sb.top()    + 6
        : sb.bottom() - hintH - 6;
    m_autoNavHint->setGeometry(x, y, hintW, hintH);
    m_autoNavHint->raise();
}

void MainWindow::positionSidePanels()
{
    // Posiciona o DrawerListPanel e o ManuscriptPanel como overlays flutuantes
    // logo à direita da LeftBar — não entram no layout pra não empurrar o editor.
    if (!leftBar) return;
    QWidget* parent = drawerListPanel ? drawerListPanel->parentWidget() : nullptr;
    if (!parent) return;
    const int margin = 10;
    const int tbH = (toolbarHolder && toolbarHolder->isVisible()) ? toolbarHolder->height() : 0;
    const int x = margin + leftBar->width() + margin;
    const int y = tbH + margin;
    const int maxH = qMax(0, parent->height() - tbH - margin * 2);

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

    positionExternalScrollBar();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && toolbar)
        toolbar->setFullscreenChecked(isFullScreen());
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (backgroundWidget) {
        backgroundWidget->setGeometry(rect());
        backgroundWidget->lower();
    }
    if (toolbarHolder && toolbarHolder->isVisible()) {
        toolbarHolder->setGeometry(0, 0, width(), toolbarHolder->sizeHint().height());
        toolbarHolder->raise();
    }
    updateEditorContainerMargins();
    resizeEditorColumnToViewport();
    positionWordCountPanel();
    positionSidePanels();
    positionExternalScrollBar();
    positionFindBar();
    positionGlobalSearchPanel();
    if (m_reminderToast && m_reminderToast->isVisible()) positionReminderToast();
    if (m_updateToast && m_updateToast->isVisible()) positionUpdateToast();
    if (readModeEnabled) positionReadModeHotzones();
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
    // Botão "Carregar" da TopToolbar agora abre o Main Menu, que lista os
    // recentes e tem os botões Novo/Carregar lá dentro — entrega mais
    // informação do que o QFileDialog puro.
    openMainMenu();
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
        connect(settingsPanel, &SettingsPanel::detectionEnabledChanged, this, [this](bool enabled) {
            detectionEnabled = enabled;
            if (!enabled && detectionTimer) detectionTimer->stop();
        });
        connect(settingsPanel, &SettingsPanel::detectionMarkAllChanged, this, [this](bool markAll) {
            detectionMarkAll = markAll;
            if (!projectRoot.isEmpty()) {
                QSettings qs;
                qs.beginGroup(QStringLiteral("detectionState"));
                qs.beginGroup(QString::fromLatin1(
                    QCryptographicHash::hash(projectRoot.toUtf8(), QCryptographicHash::Md5).toHex()));
                qs.setValue(QStringLiteral("markAll"), markAll);
                qs.endGroup(); qs.endGroup();
            }
        });
        connect(settingsPanel, &SettingsPanel::autoNavEnabledChanged, this, [this](bool enabled) {
            m_autoNavEnabled = enabled;
            QSettings().setValue(QStringLiteral("editor/autoNavEnabled"), enabled);
            if (!enabled) deactivateNavZone();
        });
        connect(settingsPanel, &SettingsPanel::maxDocsChanged, this, [this](int n) {
            QSettings().setValue(QStringLiteral("docCache/maxDocs"), n);
            if (docCache) docCache->setMaxDocs(n);
        });
        // Lê preferências globais (não por projeto)
        m_autoNavEnabled = QSettings().value(QStringLiteral("editor/autoNavEnabled"), true).toBool();
    }

    // Atualiza a UI do painel com o estado atual antes de mostrar.
    const QString lang = projectModel ? projectModel->spellLanguage() : QString();
    settingsPanel->setSpellEnabled(!lang.isEmpty());
    if (!lang.isEmpty()) settingsPanel->setSpellLanguage(lang);
    settingsPanel->setDetectionEnabled(detectionEnabled);
    settingsPanel->setDetectionMarkAll(detectionMarkAll);
    settingsPanel->setAutoNavEnabled(m_autoNavEnabled);
    settingsPanel->setMaxDocs(QSettings().value(QStringLiteral("docCache/maxDocs"), 6).toInt());
    // Teto do comprimento de página = altura útil da folha visível. Acima disso a
    // folha seria cortada fora da janela; no máximo, ela bate exatamente na tela.
    settingsPanel->setPageHeightMaximum(availableSheetHeight());

    settingsPanel->show();
    settingsPanel->raise();
    settingsPanel->activateWindow();
}

void MainWindow::onExportRequested()
{
    if (!projectModel || !hasProjectLoaded()) return;

    // Garante que o disco reflete as edições atuais antes de ler pra exportar.
    if (projectSaver) projectSaver->saveProject();

    auto* panel = new ExportPanel(projectModel, this);
    panel->setAttribute(Qt::WA_DeleteOnClose);
    connect(panel, &ExportPanel::exportRequested, this,
            [this](const Exporter::Selection& sel) {
        Exporter::DocStyle style;
        style.fontFamily = currentFontFamily;
        style.fontSize = currentFontSize;
        style.lineHeightPercent = currentLineHeight;
        style.firstLineIndent = firstLineIndentEnabled;
        style.spacingBefore = paragraphSpacingBefore;
        style.spacingAfter = paragraphSpacingAfter;
        Exporter exporter(projectModel, projectRoot, style);
        QString err;
        bool nothing = false;
        const bool ok = exporter.run(sel, this, &err, &nothing);
        if (ok) {
            // Toast flutuante de sucesso (mesmo padrão da "Nova cena criada").
            if (editorContainer) {
                auto* toast = new QLabel(tr("Exportado com sucesso"), editorContainer);
                toast->setObjectName(QStringLiteral("sceneToast"));
                toast->setAlignment(Qt::AlignCenter);
                toast->setStyleSheet(QStringLiteral(
                    "QLabel#sceneToast { background: %1; color: %2; border: 1px solid %3;"
                    " border-radius: 6px; padding: 6px 16px;"
                    " font-family: 'Lora','Crimson Text',serif; font-size: 12px; }")
                    .arg(Theme::panelBackground(), Theme::textBright(), Theme::panelBorder()));
                toast->adjustSize();
                const QPoint center = editorContainer->rect().center();
                toast->move(center.x() - toast->width() / 2,
                            editorContainer->height() - toast->height() - 32);
                toast->show();
                toast->raise();
                QTimer::singleShot(1800, toast, [toast]() { toast->deleteLater(); });
            }
        } else if (!nothing && !err.isEmpty()) {
            QMessageBox::warning(this, tr("Exportar"), err);
        }
        // Cancelado no diálogo de destino: silêncio.
    });
    panel->exec();
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

    applyBackgroundFromTheme();

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

void MainWindow::updateEditorContainerMargins()
{
    if (!editorContainer) return;
    auto* lay = qobject_cast<QHBoxLayout*>(editorContainer->layout());
    if (!lay) return;
    const int tbH = (toolbarHolder && toolbarHolder->isVisible())
                    ? toolbarHolder->sizeHint().height() : 0;
    // Sem respiro vertical: a folha começa colada na base da toolbar e vai até o
    // fundo da janela. Assim, no comprimento máximo (ou "Tela cheia"), a página
    // bate exatamente nos limites da janela — sem folgas em cima nem embaixo.
    lay->setContentsMargins(10, tbH, 10, 0);
}

void MainWindow::applyEditorLayout()
{
    const int pw = EditorLayout::pageWidth();
    const int ph = EditorLayout::pageHeight(); // 0 = automático (preenche viewport)
    const int hm = EditorLayout::horizontalMargin();
    const int vm = EditorLayout::verticalMargin();

    if (editor) {
        editor->setFixedWidth(pw);
        // Margens internas entre a borda da "página" e o texto.
        editor->setPageMargins(hm, vm, hm, vm);
        // Comprimento fixo: a folha vira uma página de altura definida; em
        // "Automático" (0) volta a preencher a viewport.
        if (ph > 0) {
            editor->setFixedHeight(ph);
        } else {
            editor->setMinimumHeight(0);
            editor->setMaximumHeight(QWIDGETSIZE_MAX);
        }
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
    const int colW = editorColumn->width();
    const int ph = EditorLayout::pageHeight();
    // Comprimento "Automático" (0): a coluna preenche a viewport (folha rola o
    // conteúdo internamente). Comprimento fixo: a coluna usa a altura da folha
    // (+ barra de variação); o QScrollArea a centraliza verticalmente, e a
    // imagem de fundo aparece em volta.
    // Espaço da coluna ocupado fora da "folha" (barra de variação, quando visível).
    int extra = 0;
    if (variationBar && variationBar->isVisible())
        extra = variationBar->height() + 8 /* spacing do QVBoxLayout */;

    int colH;
    if (ph > 0) {
        // Folha de altura fixa — mas NUNCA mais alta que a área visível. Um editor
        // mais alto que a viewport rola o documento dentro de um viewport interno
        // maior do que o que aparece na tela; o fim do documento fica renderizado
        // na faixa do editor cortada fora da janela, e o thumb chega ao limite com
        // conteúdo ainda embaixo. Limitar a folha à viewport mantém o scroll
        // interno coerente com o visível.
        const int avail = availableSheetHeight();
        const int effPh = (avail > 0) ? qMin(ph, avail) : ph;
        if (editor) editor->setFixedHeight(effPh);
        // Altura da coluna = folha + extras. Calculada direto (NÃO via sizeHint):
        // acabamos de mudar a altura fixa do editor e o sizeHint do layout ainda
        // não recalculou — leria um valor velho e a coluna ficaria do tamanho
        // errado (folha encolhida e centralizada, com folgas em cima e embaixo).
        colH = effPh + extra;
    } else {
        colH = vpH;
    }
    editorColumn->resize(colW, colH);
}

int MainWindow::availableSheetHeight() const
{
    if (!editorScroll) return 0;
    int extra = 0;
    if (variationBar && variationBar->isVisible())
        extra = variationBar->height() + 8 /* spacing do QVBoxLayout */;
    return qMax(0, editorScroll->viewport()->height() - extra);
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

void MainWindow::applyBackgroundFromTheme()
{
    if (!editorContainer) return;
    const QString img = Theme::backgroundImage();
    const int mode = Theme::backgroundMode();
    const bool hasImage = !img.isEmpty();

    if (backgroundWidget) {
        backgroundWidget->setBackground(hasImage ? img : QString(), mode);
        backgroundWidget->setFillColor(QColor(Theme::appBackground()));
        backgroundWidget->setGeometry(rect());
        backgroundWidget->setVisible(hasImage);
        backgroundWidget->lower();
    }

    // Quando há imagem: deixa o container transparente pra a imagem aparecer.
    // Sem imagem: cor sólida normal (comportamento antigo).
    if (hasImage) {
        editorContainer->setStyleSheet(
            QStringLiteral("#editorContainer { background: transparent; }"));
    } else {
        editorContainer->setStyleSheet(
            QStringLiteral("#editorContainer { background: %1; }").arg(Theme::appBackground()));
    }
    if (toolbarHolder) {
        toolbarHolder->setStyleSheet(
            QStringLiteral("#topToolbarHolder { background: %1; }")
                .arg(hasImage ? QStringLiteral("transparent") : Theme::appBackground()));
    }

#ifdef Q_OS_WIN
    {
        const QColor bg(Theme::appBackground());
        const COLORREF cr = RGB(bg.red(), bg.green(), bg.blue());
        HWND hwnd = reinterpret_cast<HWND>(winId());
        DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &cr, sizeof(cr));
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &cr, sizeof(cr));
    }
#endif
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
        // Em modo cena o documento é a cena isolada (sem <hr>); passa a cena
        // conhecida pelo viewMode pro store não inferir errado.
        const int sceneHint = (editorHost->viewMode().type == EditorHost::SceneDoc)
            ? editorHost->viewMode().sceneIndex : -1;
        appliedId = markerStore->applyMarkerToSelection(key, cur, color, comment, sceneHint);
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

TimelinePanel* MainWindow::ensureTimelinePanel()
{
    if (!timelinePanel) {
        timelinePanel = new TimelinePanel(this);
        connect(timelinePanel, &TimelinePanel::closeRequested, this, [this]() {
            leftBar->clearSelection();
        });
        timelinePanel->setProjectModel(projectModel);
        timelinePanel->setElementsStore(elementsStore);
        timelinePanel->setPresenceProvider(m_presenceProvider);
        timelinePanel->setDocTextResolver([this](const QString& key) {
            return docTextForLink(key);
        });
        if (!projectRoot.isEmpty())
            timelinePanel->setProjectRoot(projectRoot);
    }
    return timelinePanel;
}

QString MainWindow::docTextForLink(const QString& linkKey)
{
    if (!projectModel || linkKey.isEmpty()) return QString();

    // 1. obtém o HTML do alvo (preferindo o DocCache, que tem edições ao vivo)
    QString html;
    if (linkKey.startsWith(QStringLiteral("ch:"))) {
        const QString chId = linkKey.mid(3);
        if (const Chapter* ch = projectModel->findChapter(chId)) {
            const QString key = DocCache::chapterKey(ch->manuscriptId, ch->id);
            if (docCache && docCache->has(key)) html = docCache->get(key);
            else { bool ok = false; html = ProjectStorage::readChapter(projectRoot, ch->file, &ok); if (!ok) html.clear(); }
        }
    } else if (linkKey.startsWith(QStringLiteral("sc:"))) {
        const QString scId = linkKey.mid(3);
        const Chapter* foundCh = nullptr; int idx = -1;
        for (const Chapter& ch : projectModel->chapters()) {
            for (int i = 0; i < ch.scenes.size(); ++i)
                if (ch.scenes[i].id == scId) { foundCh = &ch; idx = i; break; }
            if (foundCh) break;
        }
        if (foundCh && idx >= 0) {
            const QString key = DocCache::chapterKey(foundCh->manuscriptId, foundCh->id);
            QString chHtml = (docCache && docCache->has(key)) ? docCache->get(key) : QString();
            if (chHtml.isEmpty()) { bool ok = false; chHtml = ProjectStorage::readChapter(projectRoot, foundCh->file, &ok); if (!ok) chHtml.clear(); }
            const QStringList segs = SceneUtils::splitHtmlIntoScenes(chHtml);
            if (idx < segs.size()) html = segs[idx];
        }
    } else if (linkKey.startsWith(QStringLiteral("doc:"))) {
        const QString itemId = linkKey.mid(4);
        const QString key = DocCache::itemKey(itemId);
        if (docCache && docCache->has(key)) html = docCache->get(key);
        else if (const DrawerItem* it = projectModel->findDrawerItem(itemId)) {
            if (it->hasInlineHtml) html = it->html;
        }
    }
    if (html.isEmpty()) return QString();

    // 2. HTML → texto puro
    QTextDocument doc;
    doc.setHtml(html);
    const QString text = doc.toPlainText().trimmed();
    if (text.isEmpty()) return QString();

    // 3. limite de ~600 palavras; acima disso, começo + aviso
    constexpr int kMaxWords = 600;
    const QStringList words = text.split(QRegularExpression(QStringLiteral("\\s+")),
                                         Qt::SkipEmptyParts);
    if (words.size() <= kMaxWords) return text;
    return words.mid(0, kMaxWords).join(QChar(' '))
         + QStringLiteral("\n\n[…] (trecho dos primeiros %1 de %2 mil palavras — "
                          "abra o documento original para ler tudo)")
               .arg(kMaxWords).arg(QString::number(words.size() / 1000.0, 'f', 1));
}

void MainWindow::createTimelineEventFromSelection()
{
    if (!editor || !projectModel || !editorHost) return;
    QTextCursor cur = editor->textCursor();
    if (!cur.hasSelection()) return;

    QString raw = cur.selectedText();
    raw.replace(QChar(0x2029), QChar('\n'));
    const QString text = raw.trimmed();
    if (text.isEmpty()) return;

    // Marcador do contexto: a cena onde está a seleção (se tiver marcador),
    // senão o capítulo. Para gavetas, fica sem marcador.
    QString marker;
    const auto vm = editorHost->viewMode();
    if (vm.type == EditorHost::SceneDoc) {
        if (const Scene* sc = projectModel->findScene(vm.chapterId, vm.sceneIndex))
            marker = sc->timeMarker;
        if (marker.isEmpty())
            if (const Chapter* ch = projectModel->findChapter(vm.chapterId))
                marker = ch->timeMarker;
    } else if (vm.type == EditorHost::ChapterDoc) {
        if (const Chapter* ch = projectModel->findChapter(vm.chapterId))
            marker = ch->timeMarker;
    }

    // Revela a linha do tempo (mostra o evento aterrissando) e abre o popup.
    auto* panel = ensureTimelinePanel();
    panel->show();
    panel->raise();
    panel->activateWindow();
    if (leftBar) leftBar->setActiveFixedAction(LeftBar::Timeline);
    panel->promptNewEvent(text, marker);
}

void MainWindow::addSelectionToMemory()
{
    if (!editor || !projectModel || !editorHost || !memoryAddPopup) return;
    QTextCursor cur = editor->textCursor();
    if (!cur.hasSelection()) return;

    QString raw = cur.selectedText();
    raw.replace(QChar(0x2029), QChar('\n'));
    const QString text = raw.trimmed();
    if (text.isEmpty()) return;

    // Monta a fonte (de onde o trecho veio) + um rótulo legível.
    MemoriesStore::Memory mem;
    mem.text = text;
    QString sourceLabel;
    const auto vm = editorHost->viewMode();
    if (vm.type == EditorHost::SceneDoc) {
        mem.sourceType = QStringLiteral("scene");
        mem.chapterId = vm.chapterId;
        mem.sceneIndex = vm.sceneIndex;
        mem.manuscriptId = vm.manuscriptId;
        const Chapter* ch = projectModel->findChapter(vm.chapterId);
        const Scene* sc = projectModel->findScene(vm.chapterId, vm.sceneIndex);
        const QString chTitle = ch && !ch->title.isEmpty() ? ch->title : tr("Capítulo");
        const QString scTitle = (sc && !sc->title.isEmpty())
            ? sc->title : tr("Cena %1").arg(vm.sceneIndex + 1);
        sourceLabel = tr("%1 — %2").arg(chTitle, scTitle);
    } else if (vm.type == EditorHost::ChapterDoc) {
        mem.sourceType = QStringLiteral("chapter");
        mem.chapterId = vm.chapterId;
        mem.manuscriptId = vm.manuscriptId;
        const Chapter* ch = projectModel->findChapter(vm.chapterId);
        sourceLabel = ch && !ch->title.isEmpty() ? ch->title : tr("Capítulo");
    } else if (vm.type == EditorHost::DrawerDoc) {
        mem.sourceType = QStringLiteral("drawer");
        mem.itemId = vm.itemId;
        const DrawerItem* it = projectModel->findDrawerItem(vm.itemId);
        sourceLabel = (it && !it->title.isEmpty()) ? it->title : tr("Documento");
    }
    mem.sourceLabel = sourceLabel;
    m_pendingMemory = mem;

    // Lista de personagens (elementos do tipo "character") pro seletor.
    QVector<QPair<QString, QString>> characters;
    if (elementsStore) {
        for (const Element& el : elementsStore->elements()) {
            if (el.type == QStringLiteral("character"))
                characters.append({ el.id, el.name.isEmpty() ? tr("(sem nome)") : el.name });
        }
    }

    // Posiciona o popup logo abaixo do fim da seleção.
    QTextCursor end(editor->document());
    end.setPosition(cur.selectionEnd());
    const QRect r = editor->cursorRect(end);
    const QPoint gp = editor->viewport()->mapToGlobal(r.bottomLeft()) + QPoint(0, 6);
    memoryAddPopup->presentAt(gp, text, sourceLabel, characters);
}

void MainWindow::openMemoryInEditor(const MemoriesStore::Memory& mem)
{
    if (!editorHost || !editor) return;

    EditorHost::ViewMode vm;
    if (mem.sourceType == QStringLiteral("scene") && !mem.chapterId.isEmpty()) {
        vm.type = EditorHost::SceneDoc;
        vm.manuscriptId = mem.manuscriptId;
        vm.chapterId = mem.chapterId;
        vm.sceneIndex = mem.sceneIndex;
    } else if (mem.sourceType == QStringLiteral("chapter") && !mem.chapterId.isEmpty()) {
        vm.type = EditorHost::ChapterDoc;
        vm.manuscriptId = mem.manuscriptId;
        vm.chapterId = mem.chapterId;
    } else if (mem.sourceType == QStringLiteral("drawer") && !mem.itemId.isEmpty()) {
        vm.type = EditorHost::DrawerDoc;
        vm.itemId = mem.itemId;
    } else {
        return;
    }
    editorHost->setViewMode(vm);

    // "Ctrl+F" automático: depois do doc carregar, leva o cursor ao início e
    // seleciona o 1º casamento do trecho (1ª linha, até ~60 chars).
    QString query = mem.text;
    query.replace(QChar(0x2029), QChar('\n'));
    const QStringList lines = query.split(QChar('\n'), Qt::SkipEmptyParts);
    query = lines.isEmpty() ? query.trimmed() : lines.first().trimmed();
    if (query.size() > 60) query = query.left(60);

    QTimer::singleShot(140, this, [this, query]() {
        if (!editor || query.isEmpty()) return;
        editor->moveCursor(QTextCursor::Start);
        if (editor->find(query))
            editor->ensureCursorVisible();
        editor->setFocus();
    });
}

EditorHost::ViewMode MainWindow::viewModeForDocKey(const QString& docKey) const
{
    EditorHost::ViewMode vm;
    if (!projectModel || docKey.isEmpty()) return vm;

    for (const Chapter& ch : projectModel->chapters()) {
        if (DocCache::chapterKey(ch.manuscriptId, ch.id) == docKey) {
            vm.type = EditorHost::ChapterDoc;
            vm.manuscriptId = ch.manuscriptId;
            vm.chapterId = ch.id;
            return vm;
        }
    }
    for (const Drawer& d : projectModel->drawers()) {
        for (const DrawerItem& it : d.items) {
            if (DocCache::itemKey(it.id) == docKey) {
                vm.type = EditorHost::DrawerDoc;
                vm.itemId = it.id;
                return vm;
            }
        }
    }
    for (const Manuscript& ms : projectModel->manuscripts()) {
        if (QStringLiteral("ms:%1").arg(ms.id) == docKey) {
            vm.type = EditorHost::ManuscriptDoc;
            vm.manuscriptId = ms.id;
            return vm;
        }
    }
    return vm; // Disabled — docKey órfão (doc removido)
}

void MainWindow::openMarkerInEditor(const QString& docKey, int start, int end, const QString& text)
{
    if (!editorHost || !editor) return;
    EditorHost::ViewMode vm = viewModeForDocKey(docKey);
    if (vm.type == EditorHost::Disabled) return;
    editorHost->setViewMode(vm);

    QString query = text;
    query.replace(QChar(0x2029), QChar(' '));
    query = query.trimmed();
    if (query.size() > 60) query = query.left(60);

    // Depois do doc carregar: prioriza busca pelo trecho (robusta a mudança de
    // posição); cai pro intervalo absoluto guardado se o trecho não casar.
    QTimer::singleShot(140, this, [this, query, start, end]() {
        if (!editor) return;
        if (!query.isEmpty()) {
            editor->moveCursor(QTextCursor::Start);
            if (editor->find(query)) {
                editor->ensureCursorVisible();
                editor->setFocus();
                return;
            }
        }
        QTextCursor c = editor->textCursor();
        const int docLen = editor->document()->characterCount();
        const int from = qBound(0, start, qMax(0, docLen - 1));
        const int to = qBound(from, end, qMax(0, docLen - 1));
        c.setPosition(from);
        c.setPosition(to, QTextCursor::KeepAnchor);
        editor->setTextCursor(c);
        editor->ensureCursorVisible();
        editor->setFocus();
    });
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
        elem.aliases = edlg.aliases();
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
