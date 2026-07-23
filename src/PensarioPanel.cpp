#include "PensarioPanel.h"

#include "DocCache.h"
#include "ElementsStore.h"
#include "IconUtils.h"
#include "MarkerStore.h"
#include "NameGenerator.h"
#include "NoteEditPopup.h"
#include "NotesStore.h"
#include "MapPanel.h"
#include "ProjectModel.h"
#include "Theme.h"
#include "WordCounter.h"

#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

namespace {
constexpr int kPanelWidth = 380;
constexpr int kMargin = 12;
// Filtro "Todos os capítulos" da aba Diálogos: em projeto grande a lista
// pode ter milhares de falas. Em vez de renderizar tudo de uma vez (o que
// travava a aba), mostra por página com um "carregar mais" no final. Filtro
// por capítulo específico não pagina — o volume já é naturalmente pequeno.
constexpr int kDialoguePageSize = 60;

// Esvazia um layout de verdade, incluindo widgets escondidos dentro de
// sub-layouts (ex.: topRow, adicionado via addLayout em rebuildDialogues).
// Um clear raso (só takeAt + widget() no nível de cima) NUNCA pega esses
// widgets aninhados — deletar o QLayout pai não deleta os widgets que ele
// gerencia (eles pertencem ao widget-pai, não ao layout), então sobra lixo
// órfão, ainda visível, empilhado por cima do rebuild seguinte. Foi o que
// causava o botão de scan "bugar" (duplicar visualmente) ao navegar.
void clearLayoutRecursive(QLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        } else if (QLayout* child = item->layout()) {
            clearLayoutRecursive(child);
        }
        delete item;
    }
}
}

PensarioPanel::PensarioPanel(MarkerStore* markers, ProjectModel* model,
                             NotesStore* notes, QWidget* parent)
    : QWidget(parent)
    , m_markers(markers)
    , m_model(model)
    , m_notesStore(notes)
{
    setObjectName(QStringLiteral("pensarioPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    m_nameGen = new NameGenerator();
    buildUi();
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &PensarioPanel::applyTheme);
    if (m_notesStore)
        connect(m_notesStore, &NotesStore::notesChanged, this, &PensarioPanel::rebuildNotes);
    hide();
}

PensarioPanel::~PensarioPanel()
{
    delete m_nameGen;
}

void PensarioPanel::setMemoriesStore(MemoriesStore* s)
{
    if (m_memories == s) return;
    m_memories = s;
    if (m_memories)
        connect(m_memories, &MemoriesStore::changed, this, [this]() {
            if (m_tab == Tab::Memories) rebuildMemories();
        });
}

void PensarioPanel::setElementsStore(ElementsStore* s)
{
    if (m_elements == s) return;
    m_elements = s;
    m_avatarCache.clear();
    if (m_elements)
        connect(m_elements, &ElementsStore::changed, this, [this]() { m_avatarCache.clear(); });
}

void PensarioPanel::setCurrentChapterId(const QString& chapterId)
{
    m_currentChapterId = chapterId;
    if (m_dialogueOriginFilterUserSet) return;
    if (m_dialogueOriginFilter == chapterId) return;
    m_dialogueOriginFilter = chapterId;
    m_dialogueVisibleCount = kDialoguePageSize;
    if (m_tab == Tab::Dialogues) rebuildDialogues();
}

void PensarioPanel::resetDialogueFilterState()
{
    m_currentChapterId.clear();
    m_dialogueOriginFilter.clear();
    m_dialogueOriginFilterUserSet = false;
    m_dialoguePresenceFilter.clear();
    m_dialogueVisibleCount = 0;
}

void PensarioPanel::setDialogueStore(DialogueStore* s)
{
    if (m_dialogues == s) return;
    m_dialogues = s;
    if (m_dialogues)
        connect(m_dialogues, &DialogueStore::changed, this, [this]() {
            // Durante o scan em lote (setDialogueScanState), o próprio scan
            // já dispara changed() a cada capítulo — reconstruir a lista
            // inteira a cada um deles travaria a UI em projeto grande. O
            // rebuild final acontece quando o scan termina.
            if (m_tab == Tab::Dialogues && !m_dialogueScanRunning) rebuildDialogues();
        });
}

void PensarioPanel::setDialogueScanState(bool running, int done, int total)
{
    const bool wasRunning = m_dialogueScanRunning;
    m_dialogueScanRunning = running;
    m_dialogueScanDone = done;
    m_dialogueScanTotal = total;

    if (m_dlgScanBtn) {
        m_dlgScanBtn->setEnabled(!running);
        m_dlgScanBtn->setToolTip(running
            ? tr("Escaneando… (%1/%2)").arg(done).arg(total)
            : tr("Escanear diálogos em todos os capítulos"));
    }

    if (wasRunning && !running && m_tab == Tab::Dialogues) rebuildDialogues();
}

void PensarioPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---------------- Header (arrastável) ----------------
    m_header = new QWidget(this);
    m_header->setObjectName(QStringLiteral("pnHeader"));
    m_header->setFixedHeight(40);
    m_header->installEventFilter(this);
    auto* headLay = new QHBoxLayout(m_header);
    headLay->setContentsMargins(14, 0, 8, 0);
    headLay->setSpacing(6);

    m_title = new QLabel(tr("Pensário"), m_header);
    m_title->setObjectName(QStringLiteral("pnTitle"));
    m_title->installEventFilter(this);
    headLay->addWidget(m_title);
    headLay->addStretch();

    // Seletor de ordenação (só relevante na aba Comentários).
    m_sortBtn = new QToolButton(m_header);
    m_sortBtn->setObjectName(QStringLiteral("pnSort"));
    m_sortBtn->setCursor(Qt::PointingHandCursor);
    m_sortBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_sortBtn->setPopupMode(QToolButton::InstantPopup);
    auto* sortMenu = new QMenu(m_sortBtn);
    auto* actChap = sortMenu->addAction(tr("Por capítulo"));
    auto* actCre = sortMenu->addAction(tr("Por criação"));
    actChap->setCheckable(true);
    actCre->setCheckable(true);
    actChap->setChecked(true);
    auto* sortGrp = new QActionGroup(sortMenu);
    sortGrp->addAction(actChap);
    sortGrp->addAction(actCre);
    m_sortBtn->setMenu(sortMenu);
    auto updateSortText = [this]() {
        m_sortBtn->setText((m_sortMode == SortMode::Chapters
                                ? tr("Capítulo") : tr("Criação"))
                           + QStringLiteral(" ▾"));
    };
    connect(actChap, &QAction::triggered, this, [this, updateSortText]() {
        m_sortMode = SortMode::Chapters; updateSortText(); rebuildComments();
    });
    connect(actCre, &QAction::triggered, this, [this, updateSortText]() {
        m_sortMode = SortMode::Creation; updateSortText(); rebuildComments();
    });
    updateSortText();
    headLay->addWidget(m_sortBtn);

    // Acesso discreto ao gerador de nomes (ferramenta ocasional, fora das abas).
    m_namesBtn = new QToolButton(m_header);
    m_namesBtn->setObjectName(QStringLiteral("pnNamesBtn"));
    m_namesBtn->setText(QStringLiteral("✦"));
    m_namesBtn->setCheckable(true);
    m_namesBtn->setCursor(Qt::PointingHandCursor);
    m_namesBtn->setToolTip(tr("Gerador de nomes"));
    m_namesBtn->setFixedSize(28, 28);
    connect(m_namesBtn, &QToolButton::clicked, this, [this]() { selectTab(Tab::Names); });
    headLay->addWidget(m_namesBtn);

    // Acesso ao painel do mapa-múndi (painel próprio, fora das abas).
    m_mapBtn = new QToolButton(m_header);
    m_mapBtn->setObjectName(QStringLiteral("pnMapBtn"));
    m_mapBtn->setCursor(Qt::PointingHandCursor);
    m_mapBtn->setToolTip(tr("Mapa-múndi"));
    m_mapBtn->setFixedSize(28, 28);
    m_mapBtn->setIconSize(QSize(18, 18));
    connect(m_mapBtn, &QToolButton::clicked, this, &PensarioPanel::openMapPanel);
    headLay->addWidget(m_mapBtn);

    m_closeBtn = new QToolButton(m_header);
    m_closeBtn->setObjectName(QStringLiteral("pnClose"));
    m_closeBtn->setText(QStringLiteral("×")); // ×
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip(tr("Fechar"));
    m_closeBtn->setFixedSize(28, 28);
    connect(m_closeBtn, &QToolButton::clicked, this, &PensarioPanel::closePanel);
    headLay->addWidget(m_closeBtn);

    root->addWidget(m_header);

    // ---------------- Abas ----------------
    auto* tabsRow = new QWidget(this);
    tabsRow->setObjectName(QStringLiteral("pnTabsRow"));
    auto* tabsLay = new QHBoxLayout(tabsRow);
    tabsLay->setContentsMargins(8, 6, 8, 6);
    tabsLay->setSpacing(4);

    auto makeTab = [&](const QString& label) {
        auto* b = new QToolButton(tabsRow);
        b->setObjectName(QStringLiteral("pnTab"));
        b->setText(label);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        tabsLay->addWidget(b);
        return b;
    };
    m_tabComments  = makeTab(tr("Comentários"));
    m_tabNotes     = makeTab(tr("Notas"));
    m_tabMemories  = makeTab(tr("Memórias"));
    m_tabDialogues = makeTab(tr("Diálogos"));

    connect(m_tabComments,  &QToolButton::clicked, this, [this]() { selectTab(Tab::Comments); });
    connect(m_tabNotes,     &QToolButton::clicked, this, [this]() { selectTab(Tab::Notes); });
    connect(m_tabMemories,  &QToolButton::clicked, this, [this]() { selectTab(Tab::Memories); });
    connect(m_tabDialogues, &QToolButton::clicked, this, [this]() { selectTab(Tab::Dialogues); });

    root->addWidget(tabsRow);

    // ---------------- Corpo (stack) ----------------
    m_stack = new QStackedWidget(this);

    // Página 0: Comentários
    m_commentsScroll = new QScrollArea(m_stack);
    m_commentsScroll->setWidgetResizable(true);
    m_commentsScroll->setFrameShape(QFrame::NoFrame);
    m_commentsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_commentsInner = new QWidget(m_commentsScroll);
    m_commentsLay = new QVBoxLayout(m_commentsInner);
    m_commentsLay->setContentsMargins(12, 10, 12, 12);
    m_commentsLay->setSpacing(8);
    m_commentsLay->addStretch();
    m_commentsScroll->setWidget(m_commentsInner);
    m_commentsScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    m_stack->addWidget(m_commentsScroll);

    // Página 1: Notas (funcional)
    m_stack->addWidget(buildNotesPage());

    // Página 2: Gerador de nomes (funcional)
    m_stack->addWidget(buildNamesPage());

    // Página 3: Memórias do projeto (funcional)
    m_stack->addWidget(buildMemoriesPage());

    // Página 4: Diálogos detectados automaticamente (funcional)
    m_stack->addWidget(buildDialoguesPage());

    root->addWidget(m_stack, 1);

    selectTab(Tab::Comments);
}

QWidget* PensarioPanel::buildPlaceholderPage(const QString& title, const QString& subtitle)
{
    auto* page = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->addStretch();

    auto* t = new QLabel(title, page);
    t->setObjectName(QStringLiteral("pnPlaceholderTitle"));
    t->setAlignment(Qt::AlignCenter);
    lay->addWidget(t);

    auto* s = new QLabel(subtitle, page);
    s->setObjectName(QStringLiteral("pnPlaceholderSub"));
    s->setAlignment(Qt::AlignCenter);
    s->setWordWrap(true);
    lay->addWidget(s);

    lay->addStretch();
    return page;
}

QWidget* PensarioPanel::buildNotesPage()
{
    auto* page = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(12, 10, 12, 12);
    lay->setSpacing(8);

    auto* addBtn = new QToolButton(page);
    addBtn->setObjectName(QStringLiteral("pnAddNote"));
    addBtn->setText(tr("+ Nova nota"));
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(addBtn, &QToolButton::clicked, this, &PensarioPanel::openNoteCreate);
    lay->addWidget(addBtn);

    m_notesScroll = new QScrollArea(page);
    m_notesScroll->setWidgetResizable(true);
    m_notesScroll->setFrameShape(QFrame::NoFrame);
    m_notesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_notesInner = new QWidget(m_notesScroll);
    m_notesLay = new QVBoxLayout(m_notesInner);
    m_notesLay->setContentsMargins(0, 0, 0, 0);
    m_notesLay->setSpacing(8);
    m_notesLay->addStretch();
    m_notesScroll->setWidget(m_notesInner);
    m_notesScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    lay->addWidget(m_notesScroll, 1);

    return page;
}

void PensarioPanel::rebuildNotes()
{
    if (!m_notesLay) return;

    while (m_notesLay->count() > 0) {
        QLayoutItem* item = m_notesLay->takeAt(0);
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }

    int total = 0;
    if (m_notesStore) {
        QVector<NotesStore::Note> list = m_notesStore->notes();
        std::sort(list.begin(), list.end(),
                  [](const NotesStore::Note& a, const NotesStore::Note& b) {
                      return a.createdAt > b.createdAt; // mais recentes primeiro
                  });
        for (const NotesStore::Note& n : list) {
            m_notesLay->addWidget(buildNoteCard(n.id, n.color, n.title, n.text));
            ++total;
        }
    }

    if (total == 0) {
        auto* empty = new QLabel(
            tr("Nenhuma nota ainda.\nClique em “+ Nova nota” para criar a primeira."),
            m_notesInner);
        empty->setObjectName(QStringLiteral("pnEmpty"));
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        m_notesLay->addWidget(empty);
    }

    m_notesLay->addStretch();
}

QWidget* PensarioPanel::buildNoteCard(const QString& id, const QString& color,
                                      const QString& title, const QString& text)
{
    auto* card = new QFrame(m_notesInner);
    card->setObjectName(QStringLiteral("pnNoteCard"));
    card->setCursor(Qt::PointingHandCursor);
    card->setProperty("pnNoteId", id);
    card->installEventFilter(this);

    // Mesma caixa neutra dos comentários; a cor da nota vira uma faixa no topo.
    auto* outer = new QVBoxLayout(card);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    QColor c(color);
    if (!c.isValid()) c = QColor(QStringLiteral("#FFD54F"));
    auto* stripe = new QWidget(card);
    stripe->setFixedHeight(4);
    stripe->setStyleSheet(QStringLiteral(
        "background:%1; border-top-left-radius:8px; border-top-right-radius:8px;")
        .arg(c.name()));
    outer->addWidget(stripe);

    auto* body = new QWidget(card);
    auto* lay = new QVBoxLayout(body);
    lay->setContentsMargins(12, 9, 10, 10);
    lay->setSpacing(4);

    // Cabeçalho: título (se houver) + botão excluir.
    auto* head = new QHBoxLayout();
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(4);
    if (!title.trimmed().isEmpty()) {
        auto* t = new QLabel(title.trimmed(), body);
        t->setObjectName(QStringLiteral("pnNoteCardTitle"));
        t->setWordWrap(true);
        head->addWidget(t, 1);
    } else {
        head->addStretch();
    }
    auto* del = new QToolButton(body);
    del->setObjectName(QStringLiteral("pnNoteDelete"));
    del->setText(QStringLiteral("×"));
    del->setCursor(Qt::PointingHandCursor);
    del->setToolTip(tr("Excluir nota"));
    del->setFixedSize(20, 20);
    connect(del, &QToolButton::clicked, this, [this, id]() {
        if (m_notesStore) m_notesStore->removeNote(id);
    });
    head->addWidget(del, 0, Qt::AlignTop);
    lay->addLayout(head);

    if (!text.trimmed().isEmpty()) {
        auto* txt = new QLabel(text, body);
        txt->setObjectName(QStringLiteral("pnNoteText"));
        txt->setWordWrap(true);
        txt->setTextInteractionFlags(Qt::NoTextInteraction);
        txt->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        lay->addWidget(txt);

        if (text.trimmed().size() > 80) {
            txt->setMaximumHeight(64);
            card->setProperty("pnResizable", true);
            card->setProperty("pnResizeTarget", QStringLiteral("pnNoteText"));
        }
    }

    outer->addWidget(body);
    return card;
}

void PensarioPanel::ensureNotePopup()
{
    if (m_notePopup) return;
    QWidget* host = parentWidget() ? parentWidget() : this;
    m_notePopup = new NoteEditPopup(host);
    connect(m_notePopup, &NoteEditPopup::confirmed, this,
            [this](QColor color, QString title, QString text) {
        if (!m_notesStore) return;
        if (m_editingNoteId.isEmpty())
            m_notesStore->addNote(color.name(), title, text);
        else
            m_notesStore->updateNote(m_editingNoteId, color.name(), title, text);
        m_editingNoteId.clear();
    });
    connect(m_notePopup, &NoteEditPopup::cancelled, this,
            [this]() { m_editingNoteId.clear(); });
}

void PensarioPanel::openNoteCreate()
{
    ensureNotePopup();
    m_editingNoteId.clear();
    m_notePopup->openForCreate();
}

void PensarioPanel::openMap()
{
    openMapPanel();
}

void PensarioPanel::openMapPanel()
{
    if (!m_mapPanel) {
        QWidget* host = parentWidget() ? parentWidget() : this;
        m_mapPanel = new MapPanel(m_mapPins, m_model, host);
        m_mapPanel->setTopInset(m_topInset);
    }
    m_mapPanel->togglePanel();
}

void PensarioPanel::openNoteEditById(const QString& id)
{
    if (!m_notesStore) return;
    for (const NotesStore::Note& n : m_notesStore->notes()) {
        if (n.id == id) {
            ensureNotePopup();
            m_editingNoteId = id;
            m_notePopup->openForEdit(QColor(n.color), n.title, n.text);
            return;
        }
    }
}

QWidget* PensarioPanel::buildNamesPage()
{
    auto* page = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(12, 10, 12, 12);
    lay->setSpacing(8);

    // Categoria (pills)
    auto* catRow = new QHBoxLayout();
    catRow->setContentsMargins(0, 0, 0, 0);
    catRow->setSpacing(4);
    auto makeCat = [&](const QString& label) {
        auto* b = new QToolButton(page);
        b->setObjectName(QStringLiteral("pnCatBtn"));
        b->setText(label);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        catRow->addWidget(b);
        return b;
    };
    m_catChar = makeCat(tr("Personagens"));
    m_catPlace = makeCat(tr("Lugares"));
    m_catWeapon = makeCat(tr("Armas"));
    connect(m_catChar, &QToolButton::clicked, this,
            [this]() { setNameCategory(NameCategory::Character); });
    connect(m_catPlace, &QToolButton::clicked, this,
            [this]() { setNameCategory(NameCategory::Place); });
    connect(m_catWeapon, &QToolButton::clicked, this,
            [this]() { setNameCategory(NameCategory::Weapon); });
    lay->addLayout(catRow);

    // Estilo + botão Gerar
    auto* ctrlRow = new QHBoxLayout();
    ctrlRow->setContentsMargins(0, 0, 0, 0);
    ctrlRow->setSpacing(6);
    m_styleCombo = new QComboBox(page);
    m_styleCombo->setObjectName(QStringLiteral("pnStyleCombo"));
    m_styleCombo->setCursor(Qt::PointingHandCursor);
    ctrlRow->addWidget(m_styleCombo, 1);
    m_genBtn = new QToolButton(page);
    m_genBtn->setObjectName(QStringLiteral("pnGenBtn"));
    m_genBtn->setText(tr("Gerar"));
    m_genBtn->setCursor(Qt::PointingHandCursor);
    connect(m_genBtn, &QToolButton::clicked, this, &PensarioPanel::generateNames);
    ctrlRow->addWidget(m_genBtn);
    lay->addLayout(ctrlRow);

    // Gênero — só relevante pro estilo "Pessoa" (nomes reais).
    m_genderRow = new QWidget(page);
    auto* genLay = new QHBoxLayout(m_genderRow);
    genLay->setContentsMargins(0, 0, 0, 0);
    genLay->setSpacing(4);
    m_genFem = new QToolButton(m_genderRow);
    m_genFem->setObjectName(QStringLiteral("pnCatBtn"));
    m_genFem->setText(tr("Feminino"));
    m_genFem->setCheckable(true);
    m_genFem->setChecked(true);
    m_genFem->setCursor(Qt::PointingHandCursor);
    m_genFem->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_genMasc = new QToolButton(m_genderRow);
    m_genMasc->setObjectName(QStringLiteral("pnCatBtn"));
    m_genMasc->setText(tr("Masculino"));
    m_genMasc->setCheckable(true);
    m_genMasc->setCursor(Qt::PointingHandCursor);
    m_genMasc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_genFem, &QToolButton::clicked, this, [this]() {
        m_gender = Gender::Female; m_genFem->setChecked(true); m_genMasc->setChecked(false);
    });
    connect(m_genMasc, &QToolButton::clicked, this, [this]() {
        m_gender = Gender::Male; m_genMasc->setChecked(true); m_genFem->setChecked(false);
    });
    genLay->addWidget(m_genFem);
    genLay->addWidget(m_genMasc);
    lay->addWidget(m_genderRow);

    connect(m_styleCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { updateGenderVisibility(); });

    // Afixos opcionais (só pros estilos Markov).
    m_filterRow = new QWidget(page);
    auto* filterLay = new QHBoxLayout(m_filterRow);
    filterLay->setContentsMargins(0, 0, 0, 0);
    filterLay->setSpacing(6);
    m_prefixEdit = new QLineEdit(m_filterRow);
    m_prefixEdit->setObjectName(QStringLiteral("pnAffix"));
    m_prefixEdit->setPlaceholderText(tr("Começa com…"));
    m_prefixEdit->setClearButtonEnabled(true);
    m_suffixEdit = new QLineEdit(m_filterRow);
    m_suffixEdit->setObjectName(QStringLiteral("pnAffix"));
    m_suffixEdit->setPlaceholderText(tr("Termina com…"));
    m_suffixEdit->setClearButtonEnabled(true);
    connect(m_prefixEdit, &QLineEdit::returnPressed, this, &PensarioPanel::generateNames);
    connect(m_suffixEdit, &QLineEdit::returnPressed, this, &PensarioPanel::generateNames);
    filterLay->addWidget(m_prefixEdit);
    filterLay->addWidget(m_suffixEdit);
    lay->addWidget(m_filterRow);

    m_nameStatus = new QLabel(QString(), page);
    m_nameStatus->setObjectName(QStringLiteral("pnNameStatus"));
    m_nameStatus->setAlignment(Qt::AlignCenter);
    lay->addWidget(m_nameStatus);

    m_namesScroll = new QScrollArea(page);
    m_namesScroll->setWidgetResizable(true);
    m_namesScroll->setFrameShape(QFrame::NoFrame);
    m_namesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_namesInner = new QWidget(m_namesScroll);
    m_namesLay = new QVBoxLayout(m_namesInner);
    m_namesLay->setContentsMargins(0, 0, 0, 0);
    m_namesLay->setSpacing(4);
    auto* hint = new QLabel(tr("Escolha uma categoria e clique em Gerar."), m_namesInner);
    hint->setObjectName(QStringLiteral("pnEmpty"));
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    m_namesLay->addWidget(hint);
    m_namesLay->addStretch();
    m_namesScroll->setWidget(m_namesInner);
    m_namesScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    lay->addWidget(m_namesScroll, 1);

    setNameCategory(NameCategory::Character);
    return page;
}

void PensarioPanel::setNameCategory(NameCategory c)
{
    m_nameCategory = c;
    if (m_catChar) m_catChar->setChecked(c == NameCategory::Character);
    if (m_catPlace) m_catPlace->setChecked(c == NameCategory::Place);
    if (m_catWeapon) m_catWeapon->setChecked(c == NameCategory::Weapon);

    const bool useStyle = (c != NameCategory::Weapon);
    if (m_filterRow) m_filterRow->setVisible(useStyle);
    if (m_styleCombo) {
        m_styleCombo->setVisible(useStyle);
        m_styleCombo->clear();
        if (c == NameCategory::Character) {
            for (const auto& s : NameGenerator::characterStyles())
                m_styleCombo->addItem(s.label, s.id);
        } else if (c == NameCategory::Place) {
            for (const auto& s : NameGenerator::placeStyles())
                m_styleCombo->addItem(s.label, s.id);
        }
    }
    updateGenderVisibility();
}

void PensarioPanel::updateGenderVisibility()
{
    const bool show = (m_nameCategory == NameCategory::Character)
        && m_styleCombo
        && m_styleCombo->currentData().toString() == QStringLiteral("real");
    if (m_genderRow) m_genderRow->setVisible(show);
}

void PensarioPanel::generateNames()
{
    if (!m_namesLay || !m_nameGen) return;

    while (m_namesLay->count() > 0) {
        QLayoutItem* item = m_namesLay->takeAt(0);
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }
    if (m_nameStatus) m_nameStatus->clear();

    QStringList names;
    bool hadAffix = false;
    if (m_nameCategory == NameCategory::Weapon) {
        names = m_nameGen->generateWeapons(14);
    } else {
        QString styleId = m_styleCombo ? m_styleCombo->currentData().toString() : QString();
        // "Pessoa" + gênero escolhem o dataset real correspondente.
        if (m_nameCategory == NameCategory::Character && styleId == QStringLiteral("real"))
            styleId = (m_gender == Gender::Female) ? QStringLiteral("female")
                                                   : QStringLiteral("male");
        const QString pfx = m_prefixEdit ? m_prefixEdit->text() : QString();
        const QString sfx = m_suffixEdit ? m_suffixEdit->text() : QString();
        hadAffix = !pfx.trimmed().isEmpty() || !sfx.trimmed().isEmpty();
        names = m_nameGen->generate(styleId, 14, pfx, sfx);
    }

    for (const QString& n : names) {
        auto* item = new QPushButton(n, m_namesInner);
        item->setObjectName(QStringLiteral("pnNameItem"));
        item->setCursor(Qt::PointingHandCursor);
        item->setFlat(true);
        item->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        item->setToolTip(tr("Clique para copiar"));
        connect(item, &QPushButton::clicked, this, [this, n]() { copyName(n); });
        m_namesLay->addWidget(item);
    }
    if (names.isEmpty()) {
        auto* empty = new QLabel(
            hadAffix ? tr("Nenhum nome com esse começo/fim neste estilo.\n"
                          "Tente outro estilo ou um afixo mais comum.")
                     : tr("Nada gerado. Tente de novo."),
            m_namesInner);
        empty->setObjectName(QStringLiteral("pnEmpty"));
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        m_namesLay->addWidget(empty);
    }
    m_namesLay->addStretch();
}

void PensarioPanel::copyName(const QString& name)
{
    QGuiApplication::clipboard()->setText(name);
    if (m_nameStatus) {
        m_nameStatus->setText(tr("« %1 » copiado").arg(name));
        QTimer::singleShot(1800, m_nameStatus, [label = m_nameStatus]() {
            label->clear();
        });
    }
}

QWidget* PensarioPanel::buildMemoriesPage()
{
    auto* page = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(12, 10, 12, 12);
    lay->setSpacing(8);

    m_memoriesScroll = new QScrollArea(page);
    m_memoriesScroll->setWidgetResizable(true);
    m_memoriesScroll->setFrameShape(QFrame::NoFrame);
    m_memoriesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_memoriesInner = new QWidget(m_memoriesScroll);
    m_memoriesLay = new QVBoxLayout(m_memoriesInner);
    m_memoriesLay->setContentsMargins(0, 0, 0, 0);
    m_memoriesLay->setSpacing(8);
    m_memoriesLay->addStretch();
    m_memoriesScroll->setWidget(m_memoriesInner);
    m_memoriesScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    lay->addWidget(m_memoriesScroll, 1);

    return page;
}

void PensarioPanel::rebuildMemories()
{
    if (!m_memoriesLay) return;

    while (m_memoriesLay->count() > 0) {
        QLayoutItem* item = m_memoriesLay->takeAt(0);
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }

    const QVector<MemoriesStore::Memory> all =
        m_memories ? m_memories->memories() : QVector<MemoriesStore::Memory>();

    // Nome de um personagem a partir do elementId (via ElementsStore).
    auto charName = [this](const QString& elId) -> QString {
        if (m_elements) {
            if (const Element* el = m_elements->findElement(elId))
                if (!el->name.isEmpty()) return el->name;
        }
        return tr("Personagem");
    };

    // Rótulo do filtro atual.
    QString filterLabel = tr("Todas");
    if (m_memFilter == QStringLiteral("project")) filterLabel = tr("Do projeto");
    else if (m_memFilter != QStringLiteral("all")) filterLabel = charName(m_memFilter);

    // ---- Botão de filtro (abre um menu) ----
    auto* filterBtn = new QToolButton(m_memoriesInner);
    filterBtn->setObjectName(QStringLiteral("pnMemFilterBtn"));
    filterBtn->setCursor(Qt::PointingHandCursor);
    filterBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    filterBtn->setText(tr("Filtro: %1  ▾").arg(filterLabel));
    filterBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    filterBtn->setPopupMode(QToolButton::InstantPopup);

    auto* menu = new QMenu(filterBtn);
    menu->setStyleSheet(QStringLiteral(R"(
        QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 4px; }
        QMenu::item { padding: 5px 18px; border-radius: 4px; }
        QMenu::item:selected { background: %4; color: %5; }
        QMenu::separator { height: 1px; background: %3; margin: 4px 6px; }
    )").arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
            Theme::accentInfoSoft(), Theme::textBright()));
    auto addFilter = [this, menu](const QString& label, const QString& value) {
        QAction* a = menu->addAction(label);
        a->setCheckable(true);
        a->setChecked(m_memFilter == value);
        connect(a, &QAction::triggered, this, [this, value]() {
            m_memFilter = value;
            rebuildMemories();
        });
    };
    addFilter(tr("Todas as memórias"), QStringLiteral("all"));
    addFilter(tr("Memórias do projeto"), QStringLiteral("project"));
    // Personagens que têm ao menos uma memória.
    QStringList seen;
    for (const MemoriesStore::Memory& m : all) {
        if (m.targetType == QStringLiteral("character") && !m.elementId.isEmpty()
            && !seen.contains(m.elementId)) {
            seen.append(m.elementId);
        }
    }
    if (!seen.isEmpty()) {
        menu->addSeparator();
        for (const QString& elId : seen)
            addFilter(tr("Memórias de %1").arg(charName(elId)), elId);
    }
    filterBtn->setMenu(menu);
    m_memoriesLay->addWidget(filterBtn);

    rebuildMemTagChips(all);

    // ---- Lista filtrada ----
    QVector<MemoriesStore::Memory> list;
    for (const MemoriesStore::Memory& m : all) {
        bool passesTarget = false;
        if (m_memFilter == QStringLiteral("all")) passesTarget = true;
        else if (m_memFilter == QStringLiteral("project"))
            passesTarget = (m.targetType != QStringLiteral("character"));
        else
            passesTarget = (m.targetType == QStringLiteral("character") && m.elementId == m_memFilter);
        if (!passesTarget) continue;

        if (!m_memTagFilter.isEmpty()) {
            bool hasTag = false;
            for (const QString& t : m.tags)
                if (m_memTagFilter.contains(t)) { hasTag = true; break; }
            if (!hasTag) continue;
        }
        list.append(m);
    }
    std::sort(list.begin(), list.end(),
              [](const MemoriesStore::Memory& a, const MemoriesStore::Memory& b) {
                  return a.createdAt > b.createdAt;
              });

    if (list.isEmpty()) {
        auto* empty = new QLabel(
            (m_memFilter == QStringLiteral("all") && m_memTagFilter.isEmpty())
                ? tr("Nenhuma memória ainda. Selecione um trecho no editor e escolha "
                     "“Adicionar à memória…” na barra de seleção.")
                : tr("Nenhuma memória neste filtro."),
            m_memoriesInner);
        empty->setObjectName(QStringLiteral("pnEmpty"));
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        m_memoriesLay->addWidget(empty);
        m_memoriesLay->addStretch();
        return;
    }

    for (const MemoriesStore::Memory& m : list) {
        // Cabeçalho: nome (se houver) senão "Memória do <fonte>". No modo
        // "Todas", anexa o personagem quando a memória é de um.
        QString header = m.name.isEmpty()
            ? (m.sourceLabel.isEmpty() ? tr("Memória") : tr("Memória do %1").arg(m.sourceLabel))
            : (m.sourceLabel.isEmpty() ? m.name : tr("%1  ·  %2").arg(m.name, m.sourceLabel));
        if (m_memFilter == QStringLiteral("all")
            && m.targetType == QStringLiteral("character") && !m.elementId.isEmpty()) {
            header = tr("[%1]  %2").arg(charName(m.elementId), header);
        }
        m_memoriesLay->addWidget(buildMemoryCard(m.id, header, m.text, m.tags, m_memoriesInner));
    }

    m_memoriesLay->addStretch();
}

void PensarioPanel::rebuildMemTagChips(const QVector<MemoriesStore::Memory>& all)
{
    if (!m_memoriesLay) return;

    QStringList tags;
    for (const MemoriesStore::Memory& m : all) {
        for (const QString& t : m.tags) {
            bool dup = false;
            for (const QString& seen : tags)
                if (seen.compare(t, Qt::CaseInsensitive) == 0) { dup = true; break; }
            if (!dup) tags.append(t);
        }
    }
    if (tags.isEmpty()) return;
    std::sort(tags.begin(), tags.end(),
              [](const QString& a, const QString& b) { return a.compare(b, Qt::CaseInsensitive) < 0; });

    auto* wrap = new QWidget(m_memoriesInner);
    const int maxWidth = kPanelWidth - 2 * kMargin - 4;
    const int hGap = 5, vGap = 5;
    int x = 0, y = 0, rowH = 0;
    for (const QString& tag : tags) {
        auto* chip = new QPushButton(tag, wrap);
        chip->setObjectName(QStringLiteral("pnMemTagChip"));
        chip->setCheckable(true);
        chip->setChecked(m_memTagFilter.contains(tag));
        chip->setCursor(Qt::PointingHandCursor);
        chip->setStyleSheet(QStringLiteral(R"(
            QPushButton {
                background: transparent; color: %1; border: 1px solid %2;
                border-radius: 9px; padding: 2px 8px; font-size: 10px;
            }
            QPushButton:hover   { background: %3; color: %4; }
            QPushButton:checked { background: %5; color: %4; border-color: %6; }
        )").arg(Theme::textMuted(), Theme::panelBorder(), Theme::hoverOverlay(),
                Theme::textBright(), Theme::pressedOverlay(), Theme::accentDefault()));
        chip->adjustSize();
        const QSize sz = chip->sizeHint();
        if (x > 0 && x + sz.width() > maxWidth) { x = 0; y += sz.height() + vGap; }
        chip->setGeometry(x, y, sz.width(), sz.height());
        chip->show();
        x += sz.width() + hGap;
        rowH = sz.height();
        connect(chip, &QPushButton::clicked, this, [this, tag]() {
            if (m_memTagFilter.contains(tag)) m_memTagFilter.remove(tag);
            else m_memTagFilter.insert(tag);
            rebuildMemories();
        });
    }
    wrap->setFixedHeight(y + rowH);
    m_memoriesLay->addWidget(wrap);
}

QWidget* PensarioPanel::buildMemoryCard(const QString& memId, const QString& title,
                                        const QString& text, const QStringList& tags, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("pnMemCard"));
    card->setCursor(Qt::PointingHandCursor);
    card->setProperty("memId", memId);   // usado no eventFilter pro clique
    card->installEventFilter(this);

    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(12, 9, 10, 10);
    lay->setSpacing(4);

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(4);
    auto* titleLbl = new QLabel(title, card);
    titleLbl->setObjectName(QStringLiteral("pnMemCardTitle"));
    titleLbl->setWordWrap(true);
    titleLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    topRow->addWidget(titleLbl, 1);

    auto* delBtn = new QToolButton(card);
    delBtn->setObjectName(QStringLiteral("pnMemDelete"));
    delBtn->setText(QStringLiteral("×"));
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setToolTip(tr("Excluir memória"));
    delBtn->setFixedSize(20, 20);
    connect(delBtn, &QToolButton::clicked, this, [this, memId]() {
        if (m_memories) m_memories->remove(memId);
    });
    topRow->addWidget(delBtn, 0, Qt::AlignTop);
    lay->addLayout(topRow);

    auto* textLbl = new QLabel(QStringLiteral("“%1”").arg(text.trimmed()), card);
    textLbl->setObjectName(QStringLiteral("pnMemCardQuote"));
    textLbl->setWordWrap(true);
    textLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    lay->addWidget(textLbl);

    if (text.trimmed().size() > 80) {
        textLbl->setMaximumHeight(64);
        card->setProperty("pnResizable", true);
        card->setProperty("pnResizeTarget", QStringLiteral("pnMemCardQuote"));
    }

    if (!tags.isEmpty()) {
        auto* tagsLbl = new QLabel(tags.join(QStringLiteral("  ·  ")), card);
        tagsLbl->setObjectName(QStringLiteral("pnMemCardTags"));
        tagsLbl->setWordWrap(true);
        tagsLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        tagsLbl->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; font-style: italic;")
                                   .arg(Theme::textMuted()));
        lay->addWidget(tagsLbl);
    }

    return card;
}

void PensarioPanel::showMemoryActions(const QString& memId, const QPoint& globalPos)
{
    if (!m_memories) return;
    MemoriesStore::Memory target;
    bool found = false;
    for (const MemoriesStore::Memory& m : m_memories->memories()) {
        if (m.id == memId) { target = m; found = true; break; }
    }
    if (!found) return;

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(R"(
        QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 4px; }
        QMenu::item { padding: 6px 18px; border-radius: 4px; }
        QMenu::item:selected { background: %4; color: %5; }
    )").arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
            Theme::accentInfoSoft(), Theme::textBright()));
    QAction* aEditor = menu.addAction(tr("Abrir no editor"));
    QAction* aRef    = menu.addAction(tr("Abrir no menu de referência"));
    QAction* chosen = menu.exec(globalPos);
    if (chosen == aEditor) {
        emit openMemoryInEditorRequested(target);
    } else if (chosen == aRef) {
        emit openMemoryInRefRequested(target);
    }
}

QWidget* PensarioPanel::buildDialoguesPage()
{
    auto* page = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(12, 10, 12, 12);
    lay->setSpacing(8);

    m_dialoguesScroll = new QScrollArea(page);
    m_dialoguesScroll->setWidgetResizable(true);
    m_dialoguesScroll->setFrameShape(QFrame::NoFrame);
    m_dialoguesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_dialoguesInner = new QWidget(m_dialoguesScroll);
    m_dialoguesLay = new QVBoxLayout(m_dialoguesInner);
    m_dialoguesLay->setContentsMargins(0, 0, 0, 0);
    m_dialoguesLay->setSpacing(8);
    m_dialoguesLay->addStretch();
    m_dialoguesScroll->setWidget(m_dialoguesInner);
    m_dialoguesScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    lay->addWidget(m_dialoguesScroll, 1);

    return page;
}

void PensarioPanel::rebuildDialogues()
{
    if (!m_dialoguesLay) return;

    clearLayoutRecursive(m_dialoguesLay);
    m_dlgScanBtn = nullptr; // acabou de ser deleteLater'd junto com o topRow

    const QVector<DialogueStore::Dialogue> all =
        m_dialogues ? m_dialogues->dialogues() : QVector<DialogueStore::Dialogue>();

    auto charName = [this](const QString& elId) -> QString {
        if (m_elements) {
            if (const Element* el = m_elements->findElement(elId))
                if (!el->name.isEmpty()) return el->name;
        }
        return tr("Personagem");
    };

    // ---- Linha do topo: toggle de Estatísticas (se houver diálogo) à
    // esquerda + "?" de ajuda à direita, sempre presente — explica como o
    // detector funciona e onde corrigir erros dele.
    {
        auto* topRow = new QHBoxLayout();
        topRow->setContentsMargins(0, 0, 0, 0);
        topRow->setSpacing(4);

        if (!all.isEmpty()) {
            // Link discreto, não uma barra do mesmo peso visual dos filtros
            // — é informação complementar, não um controle primário da aba.
            auto* statsToggle = new QToolButton(m_dialoguesInner);
            statsToggle->setObjectName(QStringLiteral("pnDlgStatsToggle"));
            statsToggle->setCursor(Qt::PointingHandCursor);
            statsToggle->setToolButtonStyle(Qt::ToolButtonTextOnly);
            statsToggle->setText(m_dialogueStatsExpanded ? tr("▴  Estatísticas") : tr("▾  Estatísticas"));
            connect(statsToggle, &QToolButton::clicked, this, [this]() {
                m_dialogueStatsExpanded = !m_dialogueStatsExpanded;
                rebuildDialogues();
            });
            topRow->addWidget(statsToggle, 0, Qt::AlignLeft);
        }

        topRow->addStretch(1);

        // Varredura em lote: roda o motor de diálogos em todos os capítulos
        // do projeto, um por vez, sem travar a UI — pra projetos grandes
        // onde abrir capítulo por capítulo pra popular o detector não é
        // viável. Estado (rodando/progresso) sobrevive a rebuilds via
        // m_dialogueScan* — cobre o caso do usuário mexer num filtro
        // enquanto o lote roda.
        m_dlgScanBtn = new QToolButton(m_dialoguesInner);
        m_dlgScanBtn->setObjectName(QStringLiteral("pnDlgScanBtn"));
        m_dlgScanBtn->setCursor(Qt::PointingHandCursor);
        m_dlgScanBtn->setIcon(IconUtils::loadToolbarIcon(
            QStringLiteral(":/icons/loadproject.svg"),
            QColor(Theme::textMuted()), QColor(Theme::textPrimary()),
            QColor(Theme::textBright()), QSize(16, 16)));
        m_dlgScanBtn->setIconSize(QSize(16, 16));
        m_dlgScanBtn->setEnabled(!m_dialogueScanRunning);
        m_dlgScanBtn->setToolTip(m_dialogueScanRunning
            ? tr("Escaneando… (%1/%2)").arg(m_dialogueScanDone).arg(m_dialogueScanTotal)
            : tr("Escanear diálogos em todos os capítulos"));
        connect(m_dlgScanBtn, &QToolButton::clicked, this, [this]() {
            emit rescanAllDialoguesRequested();
        });
        topRow->addWidget(m_dlgScanBtn, 0, Qt::AlignRight);

        auto* helpBtn = new QToolButton(m_dialoguesInner);
        helpBtn->setObjectName(QStringLiteral("pnDlgHelpBtn"));
        helpBtn->setCursor(Qt::PointingHandCursor);
        helpBtn->setText(QStringLiteral("?"));
        helpBtn->setToolTip(tr("Como o detector de diálogos funciona"));
        connect(helpBtn, &QToolButton::clicked, this, [this, helpBtn]() {
            auto* popup = new QFrame(nullptr);
            popup->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
            popup->setAttribute(Qt::WA_DeleteOnClose);
            popup->setStyleSheet(QStringLiteral(
                "QFrame { background: %1; border: 1px solid %2; border-radius: 8px; }")
                .arg(Theme::panelBackground(), Theme::panelBorder()));

            auto* label = new QLabel(
                tr("O detector de diálogos lê e interpreta padrões de texto para "
                   "definir o que é um diálogo e quem o disse.\n"
                   "É uma ferramenta sólida, mas pode cometer erros.\n"
                   "Caso um diálogo detectado esteja vinculado ao personagem "
                   "errado, você pode corrigir através do clique direito.\n"
                   "As estatísticas são estimativas e não garantem precisão "
                   "absoluta com o conteúdo dos capítulos."),
                popup);
            label->setWordWrap(true);
            label->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                .arg(Theme::textPrimary()));

            auto* lay = new QVBoxLayout(popup);
            lay->setContentsMargins(12, 10, 12, 10);
            lay->addWidget(label);

            const int popupWidth = qMin(280, kPanelWidth - 2 * kMargin);
            // A largura do label precisa estar fixada ANTES do adjustSize()
            // do popup — senão o QVBoxLayout calcula a altura em cima do
            // sizeHint "largo" (sem quebra) do QLabel, e o texto quebrado de
            // verdade transborda pra fora da caixa desenhada.
            label->setFixedWidth(popupWidth - 24); // 12+12 de margem horizontal
            popup->setFixedWidth(popupWidth);
            popup->adjustSize();

            QPoint pos = helpBtn->mapToGlobal(QPoint(helpBtn->width() - popupWidth, helpBtn->height() + 4));
            if (auto* screen = helpBtn->screen()) {
                const QRect avail = screen->availableGeometry();
                if (pos.x() < avail.left()) pos.setX(avail.left());
                if (pos.y() + popup->height() > avail.bottom())
                    pos.setY(helpBtn->mapToGlobal(QPoint(0, 0)).y() - popup->height() - 4);
            }
            popup->move(pos);
            popup->show();
        });
        topRow->addWidget(helpBtn, 0, Qt::AlignRight);

        m_dialoguesLay->addLayout(topRow);
    }

    if (!all.isEmpty()) {
        if (m_dialogueStatsExpanded) {
            struct Tally { int words = 0; int count = 0; };
            QHash<QString, Tally> byChar;
            QHash<QString, Tally> byChapterTitle;
            const DialogueStore::Dialogue* longest = nullptr;
            int longestWords = 0;
            for (const auto& d : all) {
                const int w = WordCounter::countWordsInPlain(d.text);
                Tally& ct = byChar[d.characterId];
                ct.words += w;
                ct.count += 1;
                const QString chapterTitle = d.sourceLabel.section(QStringLiteral(" — "), 0, 0);
                if (!chapterTitle.isEmpty()) {
                    Tally& cht = byChapterTitle[chapterTitle];
                    cht.words += w;
                    cht.count += 1;
                }
                if (!longest || w > longestWords) { longest = &d; longestWords = w; }
            }
            QString topCharName;
            Tally topChar;
            for (auto it = byChar.constBegin(); it != byChar.constEnd(); ++it) {
                if (it.value().words > topChar.words) { topChar = it.value(); topCharName = charName(it.key()); }
            }
            QString topChapterTitle;
            Tally topChapter;
            for (auto it = byChapterTitle.constBegin(); it != byChapterTitle.constEnd(); ++it) {
                if (it.value().words > topChapter.words) { topChapter = it.value(); topChapterTitle = it.key(); }
            }

            // Sem card com fundo/borda pesados — só um bloco compacto com
            // divisórias finas, pra combinar com o resto da aba (que agora é
            // tudo minimalista: link de estatísticas, filtros discretos).
            auto* statsCard = new QFrame(m_dialoguesInner);
            statsCard->setObjectName(QStringLiteral("pnDialogueStats"));
            statsCard->setStyleSheet(QStringLiteral(
                "QFrame#pnDialogueStatRow { border-bottom: 1px solid %1; }"
                "QFrame#pnDialogueStatRow[last=\"true\"] { border-bottom: none; }"
                "QLabel[role=\"statLabel\"] { color: %2; font-size: 10px; }"
                "QLabel[role=\"statValue\"] { color: %3; font-size: 11px; font-weight: 600; }")
                .arg(Theme::panelBorder(), Theme::textMuted(), Theme::textBright()));
            auto* statsLay = new QVBoxLayout(statsCard);
            statsLay->setContentsMargins(0, 0, 0, 2);
            statsLay->setSpacing(0);

            struct StatRow { QString label, value; };
            QVector<StatRow> rows;
            if (!topCharName.isEmpty())
                rows.append({ tr("Quem mais fala"),
                              tr("%1 · %2 diálogos · %3 palavras").arg(topCharName).arg(topChar.count).arg(topChar.words) });
            if (!topChapterTitle.isEmpty())
                rows.append({ tr("Capítulo com mais diálogo"),
                              tr("%1 · %2 diálogos · %3 palavras").arg(topChapterTitle).arg(topChapter.count).arg(topChapter.words) });
            if (longest)
                rows.append({ tr("Diálogo mais longo"), tr("%1 · %2 palavras").arg(charName(longest->characterId)).arg(longestWords) });

            for (int i = 0; i < rows.size(); ++i) {
                auto* rowFrame = new QFrame(statsCard);
                rowFrame->setObjectName(QStringLiteral("pnDialogueStatRow"));
                rowFrame->setProperty("last", i == rows.size() - 1);
                auto* rowLay = new QVBoxLayout(rowFrame);
                rowLay->setContentsMargins(2, 4, 2, 4);
                rowLay->setSpacing(0);

                auto* lab = new QLabel(rows[i].label, rowFrame);
                lab->setProperty("role", QStringLiteral("statLabel"));
                lab->setWordWrap(true);
                auto* val = new QLabel(rows[i].value.toHtmlEscaped(), rowFrame);
                val->setProperty("role", QStringLiteral("statValue"));
                val->setWordWrap(true);
                rowLay->addWidget(lab);
                rowLay->addWidget(val);
                statsLay->addWidget(rowFrame);
            }

            m_dialoguesLay->addWidget(statsCard);
        }
    }

    // O filtro de personagem/capítulo agora mora todo dentro de
    // rebuildDialoguePresenceChips — o botão "+ Personagem" (picker com
    // fotinha) e o filtro discreto de capítulo/cena ficam lado a lado ali.
    rebuildDialoguePresenceChips(all);

    // ---- Lista filtrada ----
    QVector<DialogueStore::Dialogue> list;
    for (const DialogueStore::Dialogue& d : all) {
        if (!m_dialogueOriginFilter.isEmpty()) {
            const int sep = m_dialogueOriginFilter.indexOf(QStringLiteral("::"));
            if (sep < 0) {
                if (d.chapterId != m_dialogueOriginFilter) continue;
            } else {
                const QString chId = m_dialogueOriginFilter.left(sep);
                const int sc = m_dialogueOriginFilter.mid(sep + 2).toInt();
                if (d.chapterId != chId || d.sceneIndex != sc) continue;
            }
        }

        if (!m_dialoguePresenceFilter.isEmpty()) {
            // A cena (capítulo+índice) precisa ter fala de TODOS os
            // personagens marcados nos chips — proxy de "presença
            // confirmada" usando os próprios diálogos já salvos (mais
            // barato que reescanear o texto da cena).
            bool ok = true;
            for (const QString& reqId : m_dialoguePresenceFilter) {
                bool present = false;
                for (const DialogueStore::Dialogue& other : all) {
                    if (other.chapterId == d.chapterId && other.sceneIndex == d.sceneIndex
                        && other.characterId == reqId) { present = true; break; }
                }
                if (!present) { ok = false; break; }
            }
            if (!ok) continue;

            // Mas só EXIBE as falas de quem está marcado — com 1 só
            // personagem escolhido isso vira "holofote" (todas as falas
            // dele, projeto inteiro se o filtro de capítulo for "Todos");
            // com 2+, mostra só a conversa entre eles, sem misturar falas de
            // quem só está de passagem na mesma cena (narrador incluso).
            if (!m_dialoguePresenceFilter.contains(d.characterId)) continue;
        }
        list.append(d);
    }
    std::sort(list.begin(), list.end(),
              [](const DialogueStore::Dialogue& a, const DialogueStore::Dialogue& b) {
                  return a.createdAt > b.createdAt;
              });

    const bool noFilters = m_dialogueOriginFilter.isEmpty() && m_dialoguePresenceFilter.isEmpty();
    if (list.isEmpty()) {
        auto* empty = new QLabel(
            noFilters
                ? tr("Nenhum diálogo detectado ainda. Escreva falas com travessão "
                     "(“— Não vou, disse Maria.”) e espere alguns segundos.")
                : tr("Nenhum diálogo neste filtro."),
            m_dialoguesInner);
        empty->setObjectName(QStringLiteral("pnEmpty"));
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        m_dialoguesLay->addWidget(empty);
        m_dialoguesLay->addStretch();
        return;
    }

    // Paginação só entra em "Todos os capítulos" — filtrado por capítulo
    // específico o volume já é naturalmente pequeno, não precisa.
    const bool paginate = m_dialogueOriginFilter.isEmpty() && list.size() > kDialoguePageSize;
    if (paginate) {
        if (m_dialogueVisibleCount <= 0) m_dialogueVisibleCount = kDialoguePageSize;
        m_dialogueVisibleCount = qMin(m_dialogueVisibleCount, list.size());
    }
    const int visibleCount = paginate ? m_dialogueVisibleCount : list.size();

    for (int i = 0; i < visibleCount; ++i) {
        const DialogueStore::Dialogue& d = list.at(i);
        m_dialoguesLay->addWidget(buildDialogueCard(d, charName(d.characterId), d.sourceLabel, m_dialoguesInner));
    }

    if (paginate && visibleCount < list.size()) {
        auto* loadMore = new QToolButton(m_dialoguesInner);
        loadMore->setObjectName(QStringLiteral("pnDlgLoadMoreBtn"));
        loadMore->setCursor(Qt::PointingHandCursor);
        loadMore->setText(tr("Carregar mais (%1 restantes)").arg(list.size() - visibleCount));
        connect(loadMore, &QToolButton::clicked, this, [this]() {
            m_dialogueVisibleCount += kDialoguePageSize;
            rebuildDialogues();
        });
        m_dialoguesLay->addWidget(loadMore);
    }

    m_dialoguesLay->addStretch();
}

void PensarioPanel::rebuildDialoguePresenceChips(const QVector<DialogueStore::Dialogue>& all)
{
    if (!m_dialoguesLay) return;

    auto charName = [this](const QString& elId) -> QString {
        if (m_elements) {
            if (const Element* el = m_elements->findElement(elId))
                if (!el->name.isEmpty()) return el->name;
        }
        return tr("Personagem");
    };

    QStringList speakerIds;
    for (const DialogueStore::Dialogue& d : all) {
        if (!d.characterId.isEmpty() && !speakerIds.contains(d.characterId))
            speakerIds.append(d.characterId);
    }
    std::sort(speakerIds.begin(), speakerIds.end(), [&](const QString& a, const QString& b) {
        return charName(a).compare(charName(b), Qt::CaseInsensitive) < 0;
    });
    const bool canPickPresence = speakerIds.size() >= 2; // não faz sentido cruzar com só 1 personagem falando

    if (canPickPresence) {
        // Personagem some do filtro se não fala mais neste conjunto (ex.:
        // trocou o filtro de capítulo e ele não aparece mais ali).
        for (auto it = m_dialoguePresenceFilter.begin(); it != m_dialoguePresenceFilter.end();) {
            if (!speakerIds.contains(*it)) it = m_dialoguePresenceFilter.erase(it);
            else ++it;
        }
    } else {
        m_dialoguePresenceFilter.clear();
    }

    auto* wrap = new QWidget(m_dialoguesInner);
    auto* wrapLay = new QVBoxLayout(wrap);
    wrapLay->setContentsMargins(0, 0, 0, 0);
    wrapLay->setSpacing(5);

    // Um botão "escolhido" por personagem já selecionado (clicar remove) +
    // uma linha final com o botão "+ Personagem" (abre uma lista com
    // fotinha pra escolher o próximo) ao lado do filtro discreto de
    // capítulo/cena — em vez de mostrar TODOS os chips possíveis de uma vez
    // (poluído) ou duas barras cheias separadas (pesado).
    QStringList picked;
    if (canPickPresence) {
        if (!m_dialoguePresenceFilter.isEmpty()) {
            auto* label = new QLabel(tr("Também falam na cena com:"), wrap);
            label->setObjectName(QStringLiteral("pnDlgPresenceLabel"));
            wrapLay->addWidget(label);
        }
        picked = QStringList(m_dialoguePresenceFilter.begin(), m_dialoguePresenceFilter.end());
        std::sort(picked.begin(), picked.end(), [&](const QString& a, const QString& b) {
            return charName(a).compare(charName(b), Qt::CaseInsensitive) < 0;
        });
        for (const QString& elId : picked) {
            auto* row = new QToolButton(wrap);
            row->setObjectName(QStringLiteral("pnDlgPersonPicked"));
            row->setCursor(Qt::PointingHandCursor);
            row->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            row->setIcon(QIcon(characterAvatar(elId, 30)));
            row->setIconSize(QSize(30, 30));
            row->setText(charName(elId));
            row->setToolTip(tr("Clique para remover"));
            row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            connect(row, &QToolButton::clicked, this, [this, elId]() {
                m_dialoguePresenceFilter.remove(elId);
                m_dialogueVisibleCount = kDialoguePageSize;
                rebuildDialogues();
            });
            wrapLay->addWidget(row);
        }
    }

    // ---- Filtro de capítulo/cena — combina em E com o de cima. Semeia com
    // TODOS os capítulos do manuscrito (não só os que já têm diálogo
    // detectado) — senão capítulo nunca aberto no editor, ou que o scan em
    // lote ainda não alcançou, simplesmente some do filtro.
    struct OriginGroup { QString chapterId, manuscriptId, title; QVector<int> scenes; };
    QVector<OriginGroup> origins;
    auto ensureOrigin = [&origins](const QString& chapterId, const QString& manuscriptId,
                                    const QString& title) -> int {
        for (int i = 0; i < origins.size(); ++i)
            if (origins[i].chapterId == chapterId) return i;
        OriginGroup g;
        g.chapterId = chapterId;
        g.manuscriptId = manuscriptId;
        g.title = title;
        origins.append(g);
        return origins.size() - 1;
    };
    if (m_model) {
        for (const Chapter& ch : m_model->chapters()) {
            ensureOrigin(ch.id, ch.manuscriptId,
                         ch.title.isEmpty() ? tr("Capítulo sem título") : ch.title);
        }
    }
    for (const DialogueStore::Dialogue& d : all) {
        if (d.chapterId.isEmpty()) continue;
        const int gi = ensureOrigin(d.chapterId, d.manuscriptId,
                                    d.sourceLabel.section(QStringLiteral(" — "), 0, 0));
        if (d.sceneIndex >= 0 && !origins[gi].scenes.contains(d.sceneIndex))
            origins[gi].scenes.append(d.sceneIndex);
    }
    std::sort(origins.begin(), origins.end(), [this](const OriginGroup& a, const OriginGroup& b) {
        return rankForKey(DocCache::chapterKey(a.manuscriptId, a.chapterId))
             < rankForKey(DocCache::chapterKey(b.manuscriptId, b.chapterId));
    });

    QString originLabelText = tr("Todos");
    if (!m_dialogueOriginFilter.isEmpty()) {
        const int sep = m_dialogueOriginFilter.indexOf(QStringLiteral("::"));
        const QString chId = sep < 0 ? m_dialogueOriginFilter : m_dialogueOriginFilter.left(sep);
        for (const OriginGroup& g : origins) {
            if (g.chapterId != chId) continue;
            originLabelText = g.title;
            if (sep >= 0) {
                const int sc = m_dialogueOriginFilter.mid(sep + 2).toInt();
                for (const DialogueStore::Dialogue& d : all) {
                    if (d.chapterId == chId && d.sceneIndex == sc) {
                        originLabelText = QStringLiteral("%1 · %2")
                            .arg(g.title, d.sourceLabel.section(QStringLiteral(" — "), 1, 1));
                        break;
                    }
                }
            }
            break;
        }
    }

    auto* originBtn = new QToolButton(wrap);
    originBtn->setObjectName(QStringLiteral("pnDlgFilterBtn"));
    originBtn->setCursor(Qt::PointingHandCursor);
    originBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    originBtn->setText(tr("Cap.: %1  ▾").arg(originLabelText));
    originBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Lista de verdade (QListWidget), não QMenu — um projeto com muitos
    // capítulos/cenas fazia o menu abrir enorme, cobrindo a tela inteira.
    // Um QListWidget com altura travada em ~7 linhas rola o resto de
    // graça, ao contrário do QMenu (que só encolhe forçado pelo tamanho da
    // TELA, nunca por um limite que a gente escolhe).
    connect(originBtn, &QToolButton::clicked, this, [this, originBtn, origins, all]() {
        auto* popup = new QFrame(nullptr);
        popup->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
        popup->setAttribute(Qt::WA_DeleteOnClose);
        popup->setStyleSheet(QStringLiteral(
            "QFrame { background: %1; border: 1px solid %2; border-radius: 6px; }")
            .arg(Theme::panelBackground(), Theme::panelBorder()));

        auto* plist = new QListWidget(popup);
        plist->setObjectName(QStringLiteral("pnDlgOriginList"));
        plist->setFrameShape(QFrame::NoFrame);
        plist->setFocusPolicy(Qt::NoFocus);
        plist->setUniformItemSizes(true);
        plist->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        plist->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        // Largura da barra fixada por QSS — sem isso, a barra só reserva
        // espaço depois que aparece, e o popup (largura já travada antes
        // dela existir) não sobra espaço: a barra acaba desenhada pra fora
        // do frame visível.
        plist->setStyleSheet(QStringLiteral(
            "QListWidget#pnDlgOriginList { background: transparent; color: %1; outline: none; border: none; }"
            "QListWidget#pnDlgOriginList::item { padding: 5px 10px; border-radius: 4px; }"
            "QListWidget#pnDlgOriginList::item:selected { background: %2; color: %3; }"
            "QListWidget#pnDlgOriginList QScrollBar:vertical { background: transparent; width: 9px; margin: 0; }"
            "QListWidget#pnDlgOriginList QScrollBar::handle:vertical { background: %4; border-radius: 4px; min-height: 20px; }"
            "QListWidget#pnDlgOriginList QScrollBar::add-line:vertical, "
            "QListWidget#pnDlgOriginList QScrollBar::sub-line:vertical { height: 0px; }")
            .arg(Theme::textPrimary(), Theme::accentInfoSoft(), Theme::textBright(), Theme::panelBorder()));

        auto addItem = [this, plist](const QString& label, const QString& value) {
            auto* it = new QListWidgetItem(label, plist);
            it->setData(Qt::UserRole, value);
            if (value == m_dialogueOriginFilter) plist->setCurrentItem(it);
        };
        addItem(tr("Todos os capítulos"), QString());
        for (const OriginGroup& gConst : origins) {
            OriginGroup g = gConst;
            std::sort(g.scenes.begin(), g.scenes.end());
            addItem(g.title, g.chapterId);
            for (int sc : g.scenes) {
                QString sceneLabel = tr("Cena %1").arg(sc + 1);
                for (const DialogueStore::Dialogue& d : all) {
                    if (d.chapterId == g.chapterId && d.sceneIndex == sc) {
                        const QString part = d.sourceLabel.section(QStringLiteral(" — "), 1, 1);
                        if (!part.isEmpty()) sceneLabel = part;
                        break;
                    }
                }
                addItem(QStringLiteral("      · %1").arg(sceneLabel),
                        QStringLiteral("%1::%2").arg(g.chapterId).arg(sc));
            }
        }

        auto* play = new QVBoxLayout(popup);
        play->setContentsMargins(4, 4, 4, 4);
        play->addWidget(plist);

        constexpr int kRowH = 27;
        constexpr int kMaxVisibleRows = 7;
        constexpr int kMargins = 8; // 4px de cada lado do play
        const int popupWidth = qMax(240, originBtn->width());
        plist->setFixedHeight(kRowH * qMin(kMaxVisibleRows, qMax(1, plist->count())));
        plist->setFixedWidth(popupWidth - kMargins);
        popup->setFixedWidth(popupWidth);

        connect(plist, &QListWidget::itemClicked, this, [this, popup](QListWidgetItem* it) {
            m_dialogueOriginFilter = it->data(Qt::UserRole).toString();
            m_dialogueOriginFilterUserSet = true;
            m_dialogueVisibleCount = kDialoguePageSize;
            popup->close();
            rebuildDialogues();
        });

        // Clampa contra a tela: se abrir pra baixo empurrasse o popup pra
        // fora da borda inferior, abre pra CIMA do botão em vez disso.
        QPoint pos = originBtn->mapToGlobal(QPoint(0, originBtn->height() + 2));
        const int popupHeight = plist->height() + 8;
        if (auto* screen = originBtn->screen()) {
            const QRect avail = screen->availableGeometry();
            if (pos.y() + popupHeight > avail.bottom())
                pos.setY(originBtn->mapToGlobal(QPoint(0, 0)).y() - popupHeight - 2);
            if (pos.x() + popupWidth > avail.right())
                pos.setX(avail.right() - popupWidth);
        }
        popup->move(pos);
        popup->show();
    });

    // Linha final: "+ Personagem" (se ainda sobrar alguém pra adicionar) ao
    // lado do filtro de capítulo/cena, discreto.
    auto* lastRow = new QHBoxLayout();
    lastRow->setContentsMargins(0, 0, 0, 0);
    lastRow->setSpacing(6);
    if (canPickPresence && picked.size() < speakerIds.size()) {
        auto* addBtn = new QToolButton(wrap);
        addBtn->setObjectName(QStringLiteral("pnDlgPersonAdd"));
        addBtn->setCursor(Qt::PointingHandCursor);
        addBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        addBtn->setText(tr("+  Personagem"));
        addBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        addBtn->setPopupMode(QToolButton::InstantPopup);

        auto* pickMenu = new QMenu(addBtn);
        pickMenu->setStyleSheet(QStringLiteral(R"(
            QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 4px; }
            QMenu::item { padding: 5px 18px; border-radius: 4px; }
            QMenu::item:selected { background: %4; color: %5; }
        )").arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
                Theme::accentInfoSoft(), Theme::textBright()));
        for (const QString& elId : speakerIds) {
            if (m_dialoguePresenceFilter.contains(elId)) continue;
            QAction* a = pickMenu->addAction(QIcon(characterAvatar(elId, 24)), charName(elId));
            connect(a, &QAction::triggered, this, [this, elId]() {
                m_dialoguePresenceFilter.insert(elId);
                m_dialogueVisibleCount = kDialoguePageSize;
                rebuildDialogues();
            });
        }
        addBtn->setMenu(pickMenu);
        lastRow->addWidget(addBtn, 1);
    }
    lastRow->addWidget(originBtn, 1);
    wrapLay->addLayout(lastRow);

    m_dialoguesLay->addWidget(wrap);
}

QPixmap PensarioPanel::characterAvatar(const QString& elementId, int size) const
{
    const QString cacheKey = elementId + QLatin1Char(':') + QString::number(size);
    const auto cached = m_avatarCache.constFind(cacheKey);
    if (cached != m_avatarCache.constEnd()) return cached.value();

    const Element* el = m_elements ? m_elements->findElement(elementId) : nullptr;

    QPixmap photo;
    if (el && !el->image.isEmpty()) {
        const int comma = el->image.indexOf(QLatin1Char(','));
        const QByteArray raw = QByteArray::fromBase64(el->image.mid(comma + 1).toLatin1());
        photo.loadFromData(raw);
    }

    QPixmap circular(size, size);
    circular.fill(Qt::transparent);
    QPainter p(&circular);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clip;
    clip.addEllipse(0, 0, size, size);
    p.setClipPath(clip);

    if (!photo.isNull()) {
        const QPixmap scaled = photo.scaled(size, size, Qt::KeepAspectRatioByExpanding,
                                             Qt::SmoothTransformation);
        p.drawPixmap((size - scaled.width()) / 2, (size - scaled.height()) / 2, scaled);
    } else {
        // Sem foto: círculo colorido (cor estável, derivada do id — mesmo
        // personagem sempre cai na mesma cor) com a inicial do nome.
        const QColor bg = QColor::fromHsv(int(qHash(elementId) % 360), 120, 165);
        p.fillRect(0, 0, size, size, bg);
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(qMax(9, int(size * 0.42)));
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 235));
        const QString name = el ? el->name.trimmed() : QString();
        const QString initial = name.isEmpty() ? QStringLiteral("?") : name.left(1).toUpper();
        p.drawText(circular.rect(), Qt::AlignCenter, initial);
    }
    m_avatarCache.insert(cacheKey, circular);
    return circular;
}

QWidget* PensarioPanel::buildDialogueCard(const DialogueStore::Dialogue& dlg, const QString& speakerName,
                                          const QString& originLabel, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("pnDlgCard"));
    card->setCursor(Qt::PointingHandCursor);
    card->setProperty("dlgId", dlg.id);
    card->installEventFilter(this);
    card->setContextMenuPolicy(Qt::CustomContextMenu);
    const QString dlgIdForMenu = dlg.id;
    connect(card, &QWidget::customContextMenuRequested, this, [this, card, dlgIdForMenu](const QPoint& pos) {
        QMenu menu(card);
        menu.setStyleSheet(QStringLiteral(R"(
            QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 4px; }
            QMenu::item { padding: 5px 18px; border-radius: 4px; }
            QMenu::item:selected { background: %4; color: %5; }
        )").arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
                Theme::accentInfoSoft(), Theme::textBright()));
        QAction* changeSpeaker = menu.addAction(tr("Alterar locutor…"));
        QAction* chosen = menu.exec(card->mapToGlobal(pos));
        if (chosen == changeSpeaker) showChangeSpeakerPopup(dlgIdForMenu, card->mapToGlobal(pos));
    });

    auto* outer = new QHBoxLayout(card);
    outer->setContentsMargins(11, 9, 10, 9);
    outer->setSpacing(9);

    constexpr int kAvatarSize = 32;
    auto* avatarLbl = new QLabel(card);
    avatarLbl->setFixedSize(kAvatarSize, kAvatarSize);
    avatarLbl->setPixmap(characterAvatar(dlg.characterId, kAvatarSize));
    avatarLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    outer->addWidget(avatarLbl, 0, Qt::AlignTop);

    auto* col = new QVBoxLayout();
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(2);

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(4);
    auto* titleLbl = new QLabel(speakerName, card);
    titleLbl->setObjectName(QStringLiteral("pnDlgCardName"));
    titleLbl->setWordWrap(true);
    titleLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    topRow->addWidget(titleLbl, 1);

    auto* delBtn = new QToolButton(card);
    delBtn->setObjectName(QStringLiteral("pnMemDelete"));
    delBtn->setText(QStringLiteral("×"));
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setToolTip(tr("Excluir diálogo"));
    delBtn->setFixedSize(20, 20);
    const QString dlgId = dlg.id;
    connect(delBtn, &QToolButton::clicked, this, [this, dlgId]() {
        if (m_dialogues) m_dialogues->remove(dlgId);
    });
    topRow->addWidget(delBtn, 0, Qt::AlignTop);
    col->addLayout(topRow);

    auto* originLbl = new QLabel(originLabel, card);
    originLbl->setObjectName(QStringLiteral("pnDlgCardOrigin"));
    originLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    col->addWidget(originLbl);

    auto* textLbl = new QLabel(QStringLiteral("“%1”").arg(dlg.text.trimmed()), card);
    textLbl->setObjectName(QStringLiteral("pnDlgCardQuote"));
    textLbl->setWordWrap(true);
    textLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    col->addWidget(textLbl);

    outer->addLayout(col, 1);
    return card;
}

void PensarioPanel::showChangeSpeakerPopup(const QString& dlgId, const QPoint& globalPos)
{
    if (!m_elements || !m_dialogues) return;

    QList<Element> chars;
    for (const Element& e : m_elements->elements())
        if (e.type == QStringLiteral("character")) chars.append(e);
    if (chars.isEmpty()) return;
    std::sort(chars.begin(), chars.end(), [](const Element& a, const Element& b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    auto* popup = new QFrame(nullptr);
    popup->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setStyleSheet(QStringLiteral(
        "QFrame { background: %1; border: 1px solid %2; border-radius: 6px; }")
        .arg(Theme::panelBackground(), Theme::panelBorder()));

    auto* plist = new QListWidget(popup);
    plist->setObjectName(QStringLiteral("pnDlgSpeakerList"));
    plist->setFrameShape(QFrame::NoFrame);
    plist->setFocusPolicy(Qt::NoFocus);
    plist->setUniformItemSizes(true);
    plist->setIconSize(QSize(22, 22));
    plist->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    plist->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    plist->setStyleSheet(QStringLiteral(
        "QListWidget#pnDlgSpeakerList { background: transparent; color: %1; outline: none; border: none; }"
        "QListWidget#pnDlgSpeakerList::item { padding: 5px 8px; border-radius: 4px; }"
        "QListWidget#pnDlgSpeakerList::item:selected { background: %2; color: %3; }"
        "QListWidget#pnDlgSpeakerList QScrollBar:vertical { background: transparent; width: 9px; margin: 0; }"
        "QListWidget#pnDlgSpeakerList QScrollBar::handle:vertical { background: %4; border-radius: 4px; min-height: 20px; }"
        "QListWidget#pnDlgSpeakerList QScrollBar::add-line:vertical, "
        "QListWidget#pnDlgSpeakerList QScrollBar::sub-line:vertical { height: 0px; }")
        .arg(Theme::textPrimary(), Theme::accentInfoSoft(), Theme::textBright(), Theme::panelBorder()));

    for (const Element& e : chars) {
        auto* it = new QListWidgetItem(QIcon(characterAvatar(e.id, 22)),
                                        e.name.isEmpty() ? tr("(sem nome)") : e.name, plist);
        it->setData(Qt::UserRole, e.id);
    }

    auto* lay = new QVBoxLayout(popup);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->addWidget(plist);

    constexpr int kRowH = 30;
    constexpr int kMaxVisibleRows = 7;
    constexpr int kMargins = 8;
    const int popupWidth = 220;
    plist->setFixedWidth(popupWidth - kMargins);
    plist->setFixedHeight(kRowH * qMin(kMaxVisibleRows, qMax(1, plist->count())));
    popup->setFixedWidth(popupWidth);

    connect(plist, &QListWidget::itemClicked, this, [this, popup, dlgId](QListWidgetItem* it) {
        const QString newId = it->data(Qt::UserRole).toString();
        popup->close();
        if (m_dialogues) m_dialogues->setCharacter(dlgId, newId);
    });

    QPoint pos = globalPos;
    const int popupHeight = plist->height() + 8;
    if (auto* screen = QGuiApplication::screenAt(globalPos)) {
        const QRect avail = screen->availableGeometry();
        if (pos.y() + popupHeight > avail.bottom()) pos.setY(avail.bottom() - popupHeight);
        if (pos.x() + popupWidth > avail.right()) pos.setX(avail.right() - popupWidth);
    }
    popup->move(pos);
    popup->show();
}

void PensarioPanel::selectTab(Tab tab)
{
    m_tab = tab;
    m_tabComments->setChecked(tab == Tab::Comments);
    m_tabNotes->setChecked(tab == Tab::Notes);
    if (m_tabMemories) m_tabMemories->setChecked(tab == Tab::Memories);
    if (m_tabDialogues) m_tabDialogues->setChecked(tab == Tab::Dialogues);
    if (m_namesBtn) m_namesBtn->setChecked(tab == Tab::Names);
    m_stack->setCurrentIndex(static_cast<int>(tab));
    if (m_sortBtn) m_sortBtn->setVisible(tab == Tab::Comments);
    if (tab == Tab::Comments) rebuildComments();
    else if (tab == Tab::Notes) rebuildNotes();
    else if (tab == Tab::Memories) rebuildMemories();
    else if (tab == Tab::Dialogues) rebuildDialogues();
}

void PensarioPanel::rebuildComments()
{
    if (!m_commentsLay) return;

    // Limpa cards anteriores (menos o stretch final).
    while (m_commentsLay->count() > 0) {
        QLayoutItem* item = m_commentsLay->takeAt(0);
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }

    int total = 0;
    if (m_markers) {
        // Achata todos os comentários em (docKey, entry).
        struct Item { QString docKey; MarkerStore::Entry e; };
        QVector<Item> items;
        const auto& all = m_markers->allEntries();
        for (auto it = all.constBegin(); it != all.constEnd(); ++it)
            for (const MarkerStore::Entry& e : it.value())
                items.append({it.key(), e});
        total = items.size();

        if (m_sortMode == SortMode::Creation) {
            // Lista cronológica plana (mais recentes primeiro); origem no card.
            std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
                if (a.e.createdAt != b.e.createdAt) return a.e.createdAt > b.e.createdAt;
                return a.e.start < b.e.start;
            });
            for (const Item& item : items) {
                m_commentsLay->addWidget(buildCommentCard(
                    item.docKey, item.e.color, item.e.comment, item.e.text,
                    item.e.start, item.e.end,
                    originLabel(item.docKey, item.e.sceneIndex)));
            }
        } else {
            // Por capítulo: grupos na ordem da obra, subdivididos por cena.
            std::sort(items.begin(), items.end(), [this](const Item& a, const Item& b) {
                const int ra = rankForKey(a.docKey), rb = rankForKey(b.docKey);
                if (ra != rb) return ra < rb;
                if (a.docKey != b.docKey) return a.docKey < b.docKey;
                if (a.e.sceneIndex != b.e.sceneIndex) return a.e.sceneIndex < b.e.sceneIndex;
                if (a.e.blockIndex != b.e.blockIndex) return a.e.blockIndex < b.e.blockIndex;
                return a.e.start < b.e.start;
            });
            QString lastHeader;
            bool first = true;
            for (const Item& item : items) {
                const QString header = originLabel(item.docKey, item.e.sceneIndex);
                if (first || header != lastHeader) {
                    first = false;
                    lastHeader = header;
                    auto* group = new QLabel(header, m_commentsInner);
                    group->setObjectName(QStringLiteral("pnGroup"));
                    m_commentsLay->addWidget(group);
                }
                m_commentsLay->addWidget(buildCommentCard(
                    item.docKey, item.e.color, item.e.comment, item.e.text,
                    item.e.start, item.e.end));
            }
        }
    }

    if (total == 0) {
        auto* empty = new QLabel(
            tr("Nenhum comentário ainda.\nSelecione um trecho e use o marcador "
               "com comentário para que ele apareça aqui."),
            m_commentsInner);
        empty->setObjectName(QStringLiteral("pnEmpty"));
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        m_commentsLay->addWidget(empty);
    }

    m_commentsLay->addStretch();
}

QWidget* PensarioPanel::buildCommentCard(const QString& docKey, const QString& color,
                                         const QString& comment, const QString& text,
                                         int start, int end, const QString& origin)
{
    auto* card = new QFrame(m_commentsInner);
    card->setObjectName(QStringLiteral("pnCard"));
    card->setCursor(Qt::PointingHandCursor);
    card->setProperty("pnDocKey", docKey);
    card->setProperty("pnStart", start);
    card->setProperty("pnEnd", end);
    card->setProperty("pnText", text);
    card->installEventFilter(this);

    auto* row = new QHBoxLayout(card);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    // Faixa de cor do marcador na borda esquerda.
    auto* stripe = new QWidget(card);
    stripe->setFixedWidth(4);
    QColor c(color);
    if (!c.isValid()) c = QColor(QStringLiteral("#FFD54F"));
    stripe->setStyleSheet(QStringLiteral("background:%1;border-top-left-radius:8px;"
                                         "border-bottom-left-radius:8px;").arg(c.name()));
    row->addWidget(stripe);

    auto* body = new QWidget(card);
    auto* lay = new QVBoxLayout(body);
    lay->setContentsMargins(12, 9, 12, 9);
    lay->setSpacing(4);

    if (!origin.isEmpty()) {
        auto* org = new QLabel(origin, body);
        org->setObjectName(QStringLiteral("pnCardOrigin"));
        org->setWordWrap(true);
        lay->addWidget(org);
    }

    auto* cmt = new QLabel(comment, body);
    cmt->setObjectName(QStringLiteral("pnCardComment"));
    cmt->setWordWrap(true);
    cmt->setTextInteractionFlags(Qt::NoTextInteraction);
    lay->addWidget(cmt);

    const QString trimmed = text.trimmed();
    if (!trimmed.isEmpty()) {
        auto* quote = new QLabel(QStringLiteral("“%1”").arg(trimmed), body); // “ ”
        quote->setObjectName(QStringLiteral("pnCardQuote"));
        quote->setWordWrap(true);
        quote->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        lay->addWidget(quote);

        // Trecho longo: começa recortado (~3 linhas) e fica redimensionável.
        // A "alça" é a borda inferior do card (invisível) — ver eventFilter.
        if (trimmed.size() > 80) {
            quote->setMaximumHeight(54);
            card->setProperty("pnResizable", true);
            card->setProperty("pnResizeTarget", QStringLiteral("pnCardQuote"));
        }
    }

    row->addWidget(body, 1);
    return card;
}

const Chapter* PensarioPanel::chapterForKey(const QString& docKey) const
{
    if (!m_model) return nullptr;
    for (const Chapter& ch : m_model->chapters()) {
        if (DocCache::chapterKey(ch.manuscriptId, ch.id) == docKey)
            return &ch;
    }
    return nullptr;
}

QString PensarioPanel::originLabel(const QString& docKey, int sceneIndex) const
{
    const Chapter* chap = chapterForKey(docKey);
    QString base = docTitleForKey(docKey);
    // Capítulo tem >1 cena, ou o próprio marker está numa cena adiante: mostra cena.
    bool multi = (chap && chap->scenes.size() > 1) || sceneIndex > 0;
    if (multi && sceneIndex >= 0) {
        const QString sceneName =
            (chap && sceneIndex < chap->scenes.size() && !chap->scenes[sceneIndex].title.isEmpty())
                ? chap->scenes[sceneIndex].title
                : tr("Cena %1").arg(sceneIndex + 1);
        base += QStringLiteral(" • %1").arg(sceneName);
    }
    return base;
}

int PensarioPanel::rankForKey(const QString& docKey) const
{
    if (m_model) {
        int i = 0;
        for (const Chapter& ch : m_model->chapters()) {
            if (DocCache::chapterKey(ch.manuscriptId, ch.id) == docKey) return i;
            ++i;
        }
    }
    return std::numeric_limits<int>::max(); // não-capítulos vão pro fim
}

QString PensarioPanel::docTitleForKey(const QString& docKey) const
{
    if (!m_model) return docKey;

    // Capítulo? (compara chaves computadas em vez de parsear strings)
    for (const Chapter& ch : m_model->chapters()) {
        if (DocCache::chapterKey(ch.manuscriptId, ch.id) == docKey)
            return ch.title.isEmpty() ? tr("Capítulo sem título") : ch.title;
    }
    // Item de gaveta?
    for (const Drawer& d : m_model->drawers()) {
        for (const DrawerItem& it : d.items) {
            if (DocCache::itemKey(it.id) == docKey) {
                const QString name = it.title.isEmpty() ? tr("Item sem título") : it.title;
                return d.title.isEmpty() ? name
                                         : QStringLiteral("%1 · %2").arg(d.title, name);
            }
        }
    }
    // Manuscrito?
    for (const Manuscript& ms : m_model->manuscripts()) {
        if (QStringLiteral("ms:%1").arg(ms.id) == docKey)
            return ms.title.isEmpty() ? tr("Manuscrito") : ms.title;
    }
    return docKey;
}

void PensarioPanel::refresh()
{
    if (m_tab == Tab::Comments) rebuildComments();
    else if (m_tab == Tab::Notes) rebuildNotes();
    else if (m_tab == Tab::Memories) rebuildMemories();
}

void PensarioPanel::togglePanel()
{
    if (isPanelOpen()) closePanel();
    else openPanel();
}

void PensarioPanel::openPanel()
{
    applyTheme(); // tema pode ter sido restaurado em silêncio após a criação
    refresh();
    ancorRight();
    show();
    raise();
}

void PensarioPanel::closePanel()
{
    hide();
}

bool PensarioPanel::isPanelOpen() const
{
    return isVisible();
}

void PensarioPanel::ancorRight()
{
    QWidget* p = parentWidget();
    if (!p) return;
    const int top = m_topInset + kMargin; // começa abaixo da TopToolbar flutuante
    const int h = qMax(200, p->height() - top - kMargin);
    resize(kPanelWidth, h);
    move(p->width() - width() - kMargin, top);
    m_positioned = true;
}

void PensarioPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (!m_positioned) ancorRight();
}

bool PensarioPanel::eventFilter(QObject* watched, QEvent* event)
{
    // Card de diálogo: clicar abre a cena de origem no editor (sem menu —
    // só há uma ação possível nesta v1).
    if (event->type() == QEvent::MouseButtonRelease) {
        if (auto* w = qobject_cast<QWidget*>(watched)) {
            const QString dlgId = w->property("dlgId").toString();
            if (!dlgId.isEmpty()) {
                auto* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton && w->rect().contains(me->position().toPoint())) {
                    if (m_dialogues) {
                        for (const DialogueStore::Dialogue& d : m_dialogues->dialogues()) {
                            if (d.id == dlgId) { emit openDialogueInEditorRequested(d); break; }
                        }
                    }
                    return true;
                }
            }
        }
    }

    // Card (comentário, nota ou memória): clicar age (navega / edita / abre menu);
    // arrastar a borda inferior (alça invisível, últimos ~12px) redimensiona o
    // conteúdo recortado.
    if (auto* card = qobject_cast<QWidget*>(watched);
        card && (card->property("pnDocKey").isValid()
                 || card->property("pnNoteId").isValid()
                 || card->property("memId").isValid())) {
        const QEvent::Type t = event->type();
        if (t == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton
                && card->property("pnResizable").toBool()
                && me->position().toPoint().y() >= card->height() - 12) {
                m_resizingGrip = card;
                m_gripQuote = card->findChild<QLabel*>(
                    card->property("pnResizeTarget").toString());
                m_gripStartY = me->globalPosition().toPoint().y();
                m_gripStartH = m_gripQuote ? m_gripQuote->maximumHeight() : 0;
                card->grabMouse();
                return true;
            }
            return false;
        }
        if (t == QEvent::MouseMove && m_resizingGrip == card && m_gripQuote) {
            auto* me = static_cast<QMouseEvent*>(event);
            const int delta = me->globalPosition().toPoint().y() - m_gripStartY;
            m_gripQuote->setMaximumHeight(qBound(20, m_gripStartH + delta, 5000));
            return true;
        }
        if (t == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (m_resizingGrip == card) {
                card->releaseMouse();
                m_resizingGrip = nullptr;
                m_gripQuote = nullptr;
                return true;
            }
            if (me->button() == Qt::LeftButton
                && card->rect().contains(me->position().toPoint())) {
                if (card->property("pnDocKey").isValid())
                    emit openMarkerRequested(card->property("pnDocKey").toString(),
                                             card->property("pnStart").toInt(),
                                             card->property("pnEnd").toInt(),
                                             card->property("pnText").toString());
                else if (card->property("pnNoteId").isValid())
                    openNoteEditById(card->property("pnNoteId").toString());
                else
                    showMemoryActions(card->property("memId").toString(),
                                      me->globalPosition().toPoint());
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    // Arrastar o painel pelo header.
    if (watched == m_header || watched == m_title) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragOffset = me->globalPosition().toPoint() - frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            auto* me = static_cast<QMouseEvent*>(event);
            QPoint target = me->globalPosition().toPoint() - m_dragOffset;
            if (QWidget* p = parentWidget()) {
                QPoint local = p->mapFromGlobal(target);
                local.setX(qBound(0, local.x(), qMax(0, p->width() - width())));
                local.setY(qBound(0, local.y(), qMax(0, p->height() - height())));
                move(local);
            }
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_dragging = false;
        }
        return QWidget::eventFilter(watched, event);
    }

    return QWidget::eventFilter(watched, event);
}

void PensarioPanel::applyTheme()
{
    const QString bg       = Theme::panelBackground();
    const QString border   = Theme::borderStrong();
    const QString textPri  = Theme::textPrimary();
    const QString textMut  = Theme::textMuted();
    const QString textBrt  = Theme::textBright();
    const QString cardBg   = Theme::inputBackground();
    const QString cardBd   = Theme::subtleBorder();
    const QString hover    = Theme::hoverStrong();
    const QString accent   = Theme::accentDefault();

    setStyleSheet(QStringLiteral(R"(
        #pensarioPanel {
            background: %1;
            border: 1px solid %2;
            border-radius: 10px;
        }
        /* Áreas de rolagem e seus viewports herdam o fundo do painel
           (senão mostram a cor base padrão do Qt e destoam do tema). */
        #pensarioPanel QScrollArea { background: transparent; border: none; }
        #pensarioPanel QScrollArea > QWidget { background: transparent; }
        #pnHeader {
            border-bottom: 1px solid %2;
        }
        #pnTitle {
            color: %3;
            font-size: 15px;
            font-weight: 600;
        }
        #pnClose {
            color: %4;
            border: none;
            background: transparent;
            font-size: 18px;
            border-radius: 6px;
        }
        #pnClose:hover { background: %7; color: %5; }
        #pnNamesBtn {
            color: %4;
            background: transparent;
            border: none;
            font-size: 15px;
            border-radius: 6px;
        }
        #pnNamesBtn:hover { background: %7; color: %3; }
        #pnNamesBtn:checked { background: %8; color: %5; }
        #pnMapBtn { background: transparent; border: none; border-radius: 6px; }
        #pnMapBtn:hover { background: %7; }
        #pnSort {
            color: %4;
            background: transparent;
            border: 1px solid %9;
            border-radius: 6px;
            padding: 3px 8px;
            font-size: 11px;
        }
        #pnSort:hover { background: %7; color: %3; }
        #pnSort::menu-indicator { image: none; width: 0; }
        #pnCardOrigin {
            color: %8;
            font-size: 11px;
            font-weight: 600;
        }
        #pnTab {
            color: %4;
            background: transparent;
            border: none;
            border-radius: 7px;
            padding: 6px 4px;
            font-size: 12px;
        }
        #pnTab:hover { background: %7; color: %3; }
        #pnTab:checked { background: %8; color: %5; font-weight: 600; }
        #pnGroup {
            color: %4;
            font-size: 11px;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            padding: 6px 2px 2px 2px;
        }
        #pnCard {
            background: %6;
            border: 1px solid %9;
            border-radius: 8px;
        }
        #pnCard:hover { border-color: %8; }
        #pnCardComment {
            color: %3;
            font-size: 13px;
        }
        #pnCardQuote {
            color: %4;
            font-size: 12px;
            font-style: italic;
        }
        #pnAddNote {
            color: %3;
            background: %6;
            border: 1px dashed %9;
            border-radius: 8px;
            padding: 9px;
            font-size: 13px;
        }
        #pnAddNote:hover { background: %7; border-color: %8; }
        #pnNoteCard {
            background: %6;
            border: 1px solid %9;
            border-radius: 8px;
        }
        #pnNoteCard:hover { border-color: %8; }
        #pnNoteCardTitle { color: %3; font-size: 13px; font-weight: 600; }
        #pnNoteText { color: %3; font-size: 13px; }
        #pnNoteDelete {
            color: %4;
            background: transparent;
            border: none;
            font-size: 16px;
            border-radius: 4px;
        }
        #pnNoteDelete:hover { color: %5; background: %7; }
        #pnCatBtn {
            color: %4;
            background: transparent;
            border: 1px solid %9;
            border-radius: 7px;
            padding: 5px 4px;
            font-size: 12px;
        }
        #pnCatBtn:hover { background: %7; color: %3; }
        #pnCatBtn:checked { background: %8; color: %5; font-weight: 600; }
        #pnGenBtn {
            color: %3;
            background: %8;
            border: none;
            border-radius: 7px;
            padding: 6px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        #pnGenBtn:hover { background: %7; }
        #pnStyleCombo {
            color: %3;
            background: %6;
            border: 1px solid %9;
            border-radius: 6px;
            padding: 5px 8px;
            font-size: 13px;
        }
        #pnStyleCombo QAbstractItemView {
            background: %1;
            color: %3;
            border: 1px solid %9;
            selection-background-color: %7;
        }
        #pnNameItem {
            color: %3;
            background: %6;
            border: 1px solid %9;
            border-radius: 6px;
            padding: 7px 10px;
            font-size: 13px;
            text-align: left;
        }
        #pnNameItem:hover { border-color: %8; background: %7; }
        #pnAffix {
            color: %3;
            background: %6;
            border: 1px solid %9;
            border-radius: 6px;
            padding: 5px 8px;
            font-size: 12px;
        }
        #pnAffix:focus { border-color: %8; }
        #pnNameStatus { color: %8; font-size: 12px; }
        #pnMemFilterBtn {
            color: %3;
            background: %6;
            border: 1px solid %9;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 12px;
            text-align: left;
        }
        #pnMemFilterBtn:hover { color: %5; border-color: %8; }
        #pnMemFilterBtn::menu-indicator { image: none; width: 0; }
        #pnDlgFilterBtn {
            color: %3;
            background: %6;
            border: 1px solid %9;
            border-radius: 6px;
            padding: 5px 8px;
            font-size: 11px;
        }
        #pnDlgFilterBtn:hover { color: %5; border-color: %8; }
        #pnDlgFilterBtn::menu-indicator { image: none; width: 0; }
        #pnDlgLoadMoreBtn {
            color: %8;
            background: transparent;
            border: 1px dashed %9;
            border-radius: 6px;
            padding: 6px 8px;
            font-size: 12px;
            font-weight: 600;
        }
        #pnDlgLoadMoreBtn:hover { color: %5; border-color: %8; }
        #pnDlgStatsToggle {
            color: %4;
            background: transparent;
            border: none;
            padding: 2px 0px;
            font-size: 11px;
        }
        #pnDlgStatsToggle:hover { color: %8; }
        #pnDlgHelpBtn {
            color: %4;
            background: transparent;
            border: 1px solid %9;
            border-radius: 8px;
            min-width: 16px;
            max-width: 16px;
            min-height: 16px;
            max-height: 16px;
            font-size: 10px;
            font-weight: 700;
        }
        #pnDlgHelpBtn:hover { color: %5; border-color: %8; }
        #pnDlgScanBtn {
            background: transparent;
            border: 1px solid %9;
            border-radius: 5px;
            min-width: 24px;
            max-width: 24px;
            min-height: 24px;
            max-height: 24px;
        }
        #pnDlgScanBtn:hover { border-color: %8; }
        #pnDlgScanBtn:disabled { border-color: %9; }
        #pnDlgPresenceLabel { color: %4; font-size: 10px; }
        #pnDlgPersonPicked {
            color: %5;
            background: %7;
            border: 1px solid %8;
            border-radius: 7px;
            padding: 6px 10px;
            font-size: 13px;
            font-weight: 600;
            text-align: left;
        }
        #pnDlgPersonPicked:hover { border-color: %4; }
        #pnDlgPersonAdd {
            color: %4;
            background: transparent;
            border: 1px dashed %9;
            border-radius: 6px;
            padding: 4px 8px;
            font-size: 11px;
        }
        #pnDlgPersonAdd:hover { color: %3; border-color: %8; }
        #pnDlgPersonAdd::menu-indicator { image: none; width: 0; }
        #pnMemCard {
            background: %6;
            border: 1px solid %9;
            border-radius: 8px;
        }
        #pnMemCard:hover { border-color: %8; }
        #pnMemCardTitle { color: %8; font-size: 11px; font-weight: 700; }
        #pnMemCardQuote { color: %3; font-size: 12px; }
        #pnDlgCard {
            background: %6;
            border: 1px solid %9;
            border-left: 3px solid %8;
            border-radius: 8px;
        }
        #pnDlgCard:hover { border-color: %8; }
        #pnDlgCardName { color: %5; font-size: 12px; font-weight: 700; }
        #pnDlgCardOrigin { color: %4; font-size: 10px; }
        #pnDlgCardQuote { color: %3; font-size: 12px; }
        #pnMemDelete {
            color: %4;
            background: transparent;
            border: none;
            font-size: 16px;
            border-radius: 4px;
        }
        #pnMemDelete:hover { color: %5; background: %7; }
        #pnEmpty, #pnPlaceholderSub {
            color: %4;
            font-size: 13px;
        }
        #pnPlaceholderTitle {
            color: %3;
            font-size: 16px;
            font-weight: 600;
        }
    )")
        .arg(bg, border, textPri, textMut, textBrt, cardBg, hover, accent, cardBd));

    // Ícone SVG do mapa precisa ser re-tintado a cada troca de tema.
    if (m_mapBtn) {
        m_mapBtn->setIcon(IconUtils::loadToolbarIcon(
            QStringLiteral(":/icons/worldmap.svg"),
            QColor(textMut), QColor(textPri), QColor(textBrt), QSize(18, 18)));
    }
}
