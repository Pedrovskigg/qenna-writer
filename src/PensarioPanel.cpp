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

#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
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
}

PensarioPanel::PensarioPanel(MarkerStore* markers, ProjectModel* model,
                             NotesStore* notes, QWidget* parent)
    : QWidget(parent)
    , m_markers(markers)
    , m_model(model)
    , m_notesStore(notes)
{
    setObjectName(QStringLiteral("pensarioPanel"));
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
    m_tabComments = makeTab(tr("Comentários"));
    m_tabNotes    = makeTab(tr("Notas"));
    m_tabMemories = makeTab(tr("Memórias"));

    connect(m_tabComments, &QToolButton::clicked, this, [this]() { selectTab(Tab::Comments); });
    connect(m_tabNotes,    &QToolButton::clicked, this, [this]() { selectTab(Tab::Notes); });
    connect(m_tabMemories, &QToolButton::clicked, this, [this]() { selectTab(Tab::Memories); });

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

    // ---- Lista filtrada ----
    QVector<MemoriesStore::Memory> list;
    for (const MemoriesStore::Memory& m : all) {
        if (m_memFilter == QStringLiteral("all")) list.append(m);
        else if (m_memFilter == QStringLiteral("project")) {
            if (m.targetType != QStringLiteral("character")) list.append(m);
        } else if (m.targetType == QStringLiteral("character") && m.elementId == m_memFilter) {
            list.append(m);
        }
    }
    std::sort(list.begin(), list.end(),
              [](const MemoriesStore::Memory& a, const MemoriesStore::Memory& b) {
                  return a.createdAt > b.createdAt;
              });

    if (list.isEmpty()) {
        auto* empty = new QLabel(
            m_memFilter == QStringLiteral("all")
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
        m_memoriesLay->addWidget(buildMemoryCard(m.id, header, m.text, m_memoriesInner));
    }

    m_memoriesLay->addStretch();
}

QWidget* PensarioPanel::buildMemoryCard(const QString& memId, const QString& title,
                                        const QString& text, QWidget* parent)
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

void PensarioPanel::selectTab(Tab tab)
{
    m_tab = tab;
    m_tabComments->setChecked(tab == Tab::Comments);
    m_tabNotes->setChecked(tab == Tab::Notes);
    if (m_tabMemories) m_tabMemories->setChecked(tab == Tab::Memories);
    if (m_namesBtn) m_namesBtn->setChecked(tab == Tab::Names);
    m_stack->setCurrentIndex(static_cast<int>(tab));
    if (m_sortBtn) m_sortBtn->setVisible(tab == Tab::Comments);
    if (tab == Tab::Comments) rebuildComments();
    else if (tab == Tab::Notes) rebuildNotes();
    else if (tab == Tab::Memories) rebuildMemories();
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
    // Card de memória: clicar abre o menu de ações (editor / refmenu).
    if (event->type() == QEvent::MouseButtonRelease) {
        if (auto* w = qobject_cast<QWidget*>(watched)) {
            const QString memId = w->property("memId").toString();
            if (!memId.isEmpty()) {
                auto* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton && w->rect().contains(me->position().toPoint())) {
                    showMemoryActions(memId, me->globalPosition().toPoint());
                    return true;
                }
            }
        }
    }

    // Card (comentário ou nota): clicar age (navega / edita); arrastar a borda
    // inferior (alça invisível, últimos ~12px) redimensiona o conteúdo recortado.
    if (auto* card = qobject_cast<QWidget*>(watched);
        card && (card->property("pnDocKey").isValid()
                 || card->property("pnNoteId").isValid())) {
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
                else
                    openNoteEditById(card->property("pnNoteId").toString());
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
        #pnMemCard {
            background: %6;
            border: 1px solid %9;
            border-radius: 8px;
        }
        #pnMemCard:hover { border-color: %8; }
        #pnMemCardTitle { color: %8; font-size: 11px; font-weight: 700; }
        #pnMemCardQuote { color: %3; font-size: 12px; }
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
