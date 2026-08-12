#include "StatsPanel.h"

#include "AvatarUtils.h"
#include "BondTypes.h"
#include "ChapterStatsDialog.h"
#include "DialogueChemistry.h"
#include "DialogueStore.h"
#include "DocCache.h"
#include "DocPreview.h"
#include "ElementsStore.h"
#include "ProjectModel.h"
#include "Theme.h"
#include "WordCounter.h"

#include <QColor>
#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QPainter>
#include <QProgressBar>
#include <QScreen>
#include <QScrollArea>
#include <QSet>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int kPanelWidth = 560;
constexpr int kMargin = 12;
constexpr int kAvatarSize = 56;
constexpr int kRowThumbSize = 28;
constexpr int kCharPhotoW = 150;
constexpr int kCharPhotoH = 200;
constexpr int kChapterBarMaxHeight = 120;
constexpr int kChapterBarWidth = 22;
constexpr int kDocHeight = 260;  // Ficha/documento: altura fixa, rola por dentro

// QSS dos pickers de Status/Local — migrado de DrawerListPanel (Modo
// Consistência), que deixou de existir nesta mesma fase.
QString pickerPopupQss() {
    return QStringLiteral(R"(
        QFrame#stPicker {
            background: %1;
            border: 1px solid %2;
            border-radius: 8px;
        }
        QPushButton#pickerOption {
            background: transparent;
            color: %3;
            border: none;
            border-radius: 5px;
            padding: 5px 10px;
            text-align: left;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 12px;
        }
        QPushButton#pickerOption:hover { background: %4; color: %5; }
        QPushButton#pickerOptionClear {
            background: transparent;
            color: %6;
            border: none;
            border-radius: 5px;
            padding: 5px 10px;
            text-align: left;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 11px;
            font-style: italic;
        }
        QPushButton#pickerOptionClear:hover { background: %4; color: %7; }
        QLineEdit#pickerInput {
            background: %8;
            color: %3;
            border: 1px solid %2;
            border-radius: 5px;
            padding: 4px 8px;
            font-size: 12px;
        }
    )").arg(Theme::panelBackground(), Theme::panelBorder(),
            Theme::textPrimary(), Theme::hoverOverlay(), Theme::textBright(),
            Theme::textMuted(), Theme::accentDanger(), Theme::inputBackground());
}
}

StatsPanel::StatsPanel(ProjectModel* model, ElementsStore* elements, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_elements(elements)
{
    setObjectName(QStringLiteral("statsPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    buildUi();
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &StatsPanel::applyTheme);
    if (m_elements)
        connect(m_elements, &ElementsStore::changed, this, &StatsPanel::refresh);
    hide();
}

void StatsPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---------------- Header ----------------
    m_header = new QWidget(this);
    m_header->setObjectName(QStringLiteral("stHeader"));
    m_header->setFixedHeight(40);
    auto* headLay = new QHBoxLayout(m_header);
    headLay->setContentsMargins(10, 0, 8, 0);
    headLay->setSpacing(6);

    m_backBtn = new QToolButton(m_header);
    m_backBtn->setObjectName(QStringLiteral("stBack"));
    m_backBtn->setText(QStringLiteral("←"));
    m_backBtn->setCursor(Qt::PointingHandCursor);
    m_backBtn->setToolTip(tr("Voltar"));
    m_backBtn->setFixedSize(28, 28);
    m_backBtn->setVisible(false);
    connect(m_backBtn, &QToolButton::clicked, this, &StatsPanel::backToOverview);
    headLay->addWidget(m_backBtn);

    m_title = new QLabel(tr("Estatísticas"), m_header);
    m_title->setObjectName(QStringLiteral("stTitle"));
    headLay->addWidget(m_title);
    headLay->addStretch();

    m_closeBtn = new QToolButton(m_header);
    m_closeBtn->setObjectName(QStringLiteral("stClose"));
    m_closeBtn->setText(QStringLiteral("×"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip(tr("Fechar"));
    m_closeBtn->setFixedSize(28, 28);
    connect(m_closeBtn, &QToolButton::clicked, this, &StatsPanel::closePanel);
    headLay->addWidget(m_closeBtn);

    root->addWidget(m_header);

    // ---------------- Corpo (stack) ----------------
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildOverviewPage());
    m_stack->addWidget(buildCharacterPage());
    root->addWidget(m_stack, 1);
}

QWidget* StatsPanel::buildOverviewPage()
{
    auto* scroll = new QScrollArea(m_stack);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));

    auto* inner = new QWidget(scroll);
    auto* lay = new QVBoxLayout(inner);
    lay->setContentsMargins(14, 12, 14, 14);
    lay->setSpacing(6);

    auto* charLabel = new QLabel(tr("Personagens"), inner);
    charLabel->setObjectName(QStringLiteral("stSectionLabel"));
    lay->addWidget(charLabel);

    auto* avatarScroll = new QScrollArea(inner);
    avatarScroll->setWidgetResizable(true);
    avatarScroll->setFrameShape(QFrame::NoFrame);
    avatarScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    avatarScroll->setFixedHeight(kAvatarSize + 46);
    m_avatarStrip = new QWidget(avatarScroll);
    m_avatarStripLay = new QHBoxLayout(m_avatarStrip);
    m_avatarStripLay->setContentsMargins(2, 2, 2, 2);
    m_avatarStripLay->setSpacing(10);
    m_avatarStripLay->addStretch();
    avatarScroll->setWidget(m_avatarStrip);
    avatarScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    lay->addWidget(avatarScroll);

    auto* participLabel = new QLabel(tr("Participação (cenas)"), inner);
    participLabel->setObjectName(QStringLiteral("stSectionLabel"));
    lay->addWidget(participLabel);

    m_participInner = new QWidget(inner);
    m_participLay = new QVBoxLayout(m_participInner);
    m_participLay->setContentsMargins(0, 0, 0, 0);
    m_participLay->setSpacing(6);
    lay->addWidget(m_participInner);

    auto* chapterRow = new QHBoxLayout();
    auto* chapterLabel = new QLabel(tr("Manuscrito, por capítulo"), inner);
    chapterLabel->setObjectName(QStringLiteral("stSectionLabel"));
    chapterRow->addWidget(chapterLabel);
    chapterRow->addStretch();

    m_chapterMetricBtn = new QToolButton(inner);
    m_chapterMetricBtn->setObjectName(QStringLiteral("stChemSortBtn"));
    m_chapterMetricBtn->setCursor(Qt::PointingHandCursor);
    m_chapterMetricBtn->setPopupMode(QToolButton::InstantPopup);
    auto* metricMenu = new QMenu(m_chapterMetricBtn);
    auto* actWords = metricMenu->addAction(tr("Palavras"));
    auto* actRatio = metricMenu->addAction(tr("Diálogo × narração"));
    actWords->setCheckable(true);
    actRatio->setCheckable(true);
    actWords->setChecked(true);
    auto* metricGrp = new QActionGroup(metricMenu);
    metricGrp->addAction(actWords);
    metricGrp->addAction(actRatio);
    m_chapterMetricBtn->setMenu(metricMenu);
    auto updateMetricText = [this]() {
        m_chapterMetricBtn->setText((m_chapterMetric == ChapterMetric::Words
            ? tr("Palavras") : tr("Diálogo × narração")) + QStringLiteral(" ▾"));
    };
    connect(actWords, &QAction::triggered, this, [this, updateMetricText]() {
        m_chapterMetric = ChapterMetric::Words; updateMetricText(); rebuildChapterBars();
    });
    connect(actRatio, &QAction::triggered, this, [this, updateMetricText]() {
        m_chapterMetric = ChapterMetric::DialogueRatio; updateMetricText(); rebuildChapterBars();
    });
    updateMetricText();
    chapterRow->addWidget(m_chapterMetricBtn);
    lay->addLayout(chapterRow);

    auto* chapterScroll = new QScrollArea(inner);
    chapterScroll->setWidgetResizable(true);
    chapterScroll->setFrameShape(QFrame::NoFrame);
    chapterScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chapterScroll->setFixedHeight(kChapterBarMaxHeight + 40);
    m_chapterBarsHost = new QWidget(chapterScroll);
    m_chapterBarsLay = new QHBoxLayout(m_chapterBarsHost);
    m_chapterBarsLay->setContentsMargins(2, 2, 2, 2);
    m_chapterBarsLay->setSpacing(6);
    m_chapterBarsLay->addStretch();
    chapterScroll->setWidget(m_chapterBarsHost);
    chapterScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    lay->addWidget(chapterScroll);

    auto* summaryLabel = new QLabel(tr("Resumo do projeto"), inner);
    summaryLabel->setObjectName(QStringLiteral("stSectionLabel"));
    lay->addWidget(summaryLabel);

    m_overviewStatsLabel = new QLabel(inner);
    m_overviewStatsLabel->setObjectName(QStringLiteral("stPresenceSummary"));
    m_overviewStatsLabel->setWordWrap(true);
    m_overviewStatsLabel->setTextFormat(Qt::RichText);
    lay->addWidget(m_overviewStatsLabel);

    lay->addStretch();
    scroll->setWidget(inner);
    return scroll;
}

QWidget* StatsPanel::buildCharacterPage()
{
    auto* scroll = new QScrollArea(m_stack);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));

    auto* page = new QWidget(scroll);
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(14, 12, 14, 14);
    lay->setSpacing(8);

    auto* topRow = new QHBoxLayout();
    m_charPhoto = new QLabel(page);
    m_charPhoto->setObjectName(QStringLiteral("stCharPhoto"));
    m_charPhoto->setFixedSize(kCharPhotoW, kCharPhotoH);
    m_charPhoto->setAlignment(Qt::AlignCenter);
    topRow->addWidget(m_charPhoto);

    auto* infoCol = new QVBoxLayout();
    m_charName = new QLabel(page);
    m_charName->setObjectName(QStringLiteral("stCharName"));
    m_charName->setWordWrap(true);
    infoCol->addWidget(m_charName);

    m_charPresenceSummary = new QLabel(page);
    m_charPresenceSummary->setObjectName(QStringLiteral("stPresenceSummary"));
    m_charPresenceSummary->setWordWrap(true);
    infoCol->addWidget(m_charPresenceSummary);

    m_charDialogueSummary = new QLabel(page);
    m_charDialogueSummary->setObjectName(QStringLiteral("stPresenceSummary"));
    m_charDialogueSummary->setWordWrap(true);
    infoCol->addWidget(m_charDialogueSummary);

    auto* statusRow = new QHBoxLayout();
    m_charStatusBtn = new QPushButton(page);
    m_charStatusBtn->setObjectName(QStringLiteral("stStatusBtn"));
    m_charStatusBtn->setCursor(Qt::PointingHandCursor);
    connect(m_charStatusBtn, &QPushButton::clicked, this, [this]() {
        const DrawerItem* item = drawerItemForElement(m_currentElementId);
        if (!item) return;
        showStatusPicker(item->id, m_charStatusBtn->mapToGlobal(
            QPoint(0, m_charStatusBtn->height() + 2)));
    });
    statusRow->addWidget(m_charStatusBtn);

    m_charLocationBtn = new QPushButton(page);
    m_charLocationBtn->setObjectName(QStringLiteral("stStatusBtn"));
    m_charLocationBtn->setCursor(Qt::PointingHandCursor);
    connect(m_charLocationBtn, &QPushButton::clicked, this, [this]() {
        const DrawerItem* item = drawerItemForElement(m_currentElementId);
        if (!item) return;
        showLocationPicker(item->id, m_charLocationBtn->mapToGlobal(
            QPoint(0, m_charLocationBtn->height() + 2)));
    });
    statusRow->addWidget(m_charLocationBtn);
    infoCol->addLayout(statusRow);

    m_charInconsistencyWarning = new QLabel(page);
    m_charInconsistencyWarning->setObjectName(QStringLiteral("stInconsistencyWarning"));
    m_charInconsistencyWarning->setWordWrap(true);
    m_charInconsistencyWarning->setVisible(false);
    infoCol->addWidget(m_charInconsistencyWarning);

    infoCol->addStretch();

    topRow->addLayout(infoCol, 1);
    lay->addLayout(topRow);

    auto* bondsLabel = new QLabel(tr("Vínculos"), page);
    bondsLabel->setObjectName(QStringLiteral("stSectionLabel"));
    lay->addWidget(bondsLabel);

    m_charBondsList = new QWidget(page);
    m_charBondsLay = new QVBoxLayout(m_charBondsList);
    m_charBondsLay->setContentsMargins(0, 0, 0, 0);
    m_charBondsLay->setSpacing(4);
    lay->addWidget(m_charBondsList);

    auto* chemRow = new QHBoxLayout();
    auto* chemLabel = new QLabel(tr("Química — quem mais contracenou"), page);
    chemLabel->setObjectName(QStringLiteral("stSectionLabel"));
    chemRow->addWidget(chemLabel);
    chemRow->addStretch();

    m_chemSortBtn = new QToolButton(page);
    m_chemSortBtn->setObjectName(QStringLiteral("stChemSortBtn"));
    m_chemSortBtn->setCursor(Qt::PointingHandCursor);
    m_chemSortBtn->setPopupMode(QToolButton::InstantPopup);
    auto* chemMenu = new QMenu(m_chemSortBtn);
    auto* actScenes = chemMenu->addAction(tr("Cenas conjuntas"));
    auto* actChapters = chemMenu->addAction(tr("Capítulos conjuntos"));
    auto* actCross = chemMenu->addAction(tr("Diálogos cruzados"));
    actScenes->setCheckable(true);
    actChapters->setCheckable(true);
    actCross->setCheckable(true);
    actScenes->setChecked(true);
    auto* chemGrp = new QActionGroup(chemMenu);
    chemGrp->addAction(actScenes);
    chemGrp->addAction(actChapters);
    chemGrp->addAction(actCross);
    m_chemSortBtn->setMenu(chemMenu);
    auto updateChemSortText = [this]() {
        const QString label = (m_chemSort == ChemistrySort::Scenes) ? tr("Cenas")
            : (m_chemSort == ChemistrySort::Chapters) ? tr("Capítulos") : tr("Diálogos");
        m_chemSortBtn->setText(label + QStringLiteral(" ▾"));
    };
    connect(actScenes, &QAction::triggered, this, [this, updateChemSortText]() {
        m_chemSort = ChemistrySort::Scenes; updateChemSortText(); rebuildChemistry();
    });
    connect(actChapters, &QAction::triggered, this, [this, updateChemSortText]() {
        m_chemSort = ChemistrySort::Chapters; updateChemSortText(); rebuildChemistry();
    });
    connect(actCross, &QAction::triggered, this, [this, updateChemSortText]() {
        m_chemSort = ChemistrySort::CrossDialogues; updateChemSortText(); rebuildChemistry();
    });
    updateChemSortText();
    chemRow->addWidget(m_chemSortBtn);
    lay->addLayout(chemRow);

    m_chemList = new QWidget(page);
    m_chemLay = new QVBoxLayout(m_chemList);
    m_chemLay->setContentsMargins(0, 0, 0, 0);
    m_chemLay->setSpacing(4);
    lay->addWidget(m_chemList);

    auto* docLabel = new QLabel(tr("Ficha / documento"), page);
    docLabel->setObjectName(QStringLiteral("stSectionLabel"));
    lay->addWidget(docLabel);

    // Não é um QTextBrowser: é só um QLabel em modo rich text (sem imagens —
    // ver DocPreview::stripImages) dentro de um QScrollArea com altura
    // limitada. QLabel participa do layout normalmente (sem viewport próprio
    // brigando com o resto da página); quem rola por dentro é só esse cartão.
    auto* docScroll = new QScrollArea(page);
    docScroll->setObjectName(QStringLiteral("stCharDocScroll"));
    docScroll->setWidgetResizable(true);
    docScroll->setFrameShape(QFrame::NoFrame);
    docScroll->setFixedHeight(kDocHeight);
    docScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));

    m_charDoc = new QLabel(docScroll);
    m_charDoc->setObjectName(QStringLiteral("stCharDoc"));
    m_charDoc->setTextFormat(Qt::RichText);
    m_charDoc->setWordWrap(true);
    m_charDoc->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    docScroll->setWidget(m_charDoc);
    lay->addWidget(docScroll);

    lay->addStretch();
    scroll->setWidget(page);
    return scroll;
}

void StatsPanel::refresh()
{
    refreshPresenceCache();
    rebuildOverview();
    rebuildChapterBars();
    rebuildOverviewStats();
    if (m_stack && m_stack->currentIndex() == 1) rebuildCharacterPage();
}

QList<Element> StatsPanel::projectCharacters() const
{
    QList<Element> out;
    if (!m_model || !m_elements) return out;

    QSet<QString> seen;
    for (const Drawer& d : m_model->drawers()) {
        if (d.drawerElementType != QStringLiteral("character")) continue;
        for (const DrawerItem& it : d.items) {
            if (it.elementId.isEmpty() || seen.contains(it.elementId)) continue;
            if (const Element* e = m_elements->findElement(it.elementId)) {
                seen.insert(it.elementId);
                out.append(*e);
            }
        }
    }
    return out;
}

void StatsPanel::refreshPresenceCache()
{
    m_presenceResults.clear();
    m_totalScenes = 0;
    m_totalChapters = 0;
    if (!m_presenceProvider) return;

    QStringList names;
    for (const Element& e : projectCharacters()) {
        if (!e.name.isEmpty()) names.append(e.name);
    }
    if (names.isEmpty()) return;

    m_presenceProvider(names, &m_presenceResults, &m_totalScenes, &m_totalChapters);
}

void StatsPanel::rebuildOverview()
{
    if (!m_avatarStripLay || !m_participLay) return;

    while (m_avatarStripLay->count() > 0) {
        QLayoutItem* item = m_avatarStripLay->takeAt(0);
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }
    while (m_participLay->count() > 0) {
        QLayoutItem* item = m_participLay->takeAt(0);
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }

    QList<Element> characters = projectCharacters();
    std::sort(characters.begin(), characters.end(), [](const Element& a, const Element& b) {
        return a.name.toLower() < b.name.toLower();
    });

    if (characters.isEmpty()) {
        auto* empty = new QLabel(tr("Nenhum personagem no projeto ainda."), m_avatarStrip);
        empty->setObjectName(QStringLiteral("stEmpty"));
        m_avatarStripLay->addWidget(empty);
        m_participLay->addStretch();
        m_avatarStripLay->addStretch();
        return;
    }

    for (const Element& e : characters) {
        auto* btn = new QToolButton(m_avatarStrip);
        btn->setObjectName(QStringLiteral("stAvatarBtn"));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setIcon(QIcon(AvatarUtils::circularAvatar(e.image, e.name, e.id, kAvatarSize)));
        btn->setIconSize(QSize(kAvatarSize, kAvatarSize));
        btn->setText(e.name.isEmpty() ? tr("(sem nome)") : e.name);
        btn->setFixedWidth(84);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setAutoRaise(true);
        const QString id = e.id;
        connect(btn, &QToolButton::clicked, this, [this, id]() { openCharacter(id); });
        m_avatarStripLay->addWidget(btn);

        const CharPresenceResult res = m_presenceResults.value(e.name.toLower());
        const int pct = (m_totalScenes > 0) ? qMin(100, (res.sceneCount * 100) / m_totalScenes) : 0;

        auto* row = new QWidget(m_participInner);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->setSpacing(8);

        auto* thumb = new QLabel(row);
        thumb->setFixedSize(kRowThumbSize, kRowThumbSize);
        thumb->setPixmap(AvatarUtils::circularAvatar(e.image, e.name, e.id, kRowThumbSize));
        rowLay->addWidget(thumb);

        auto* nameLbl = new QLabel(e.name, row);
        nameLbl->setObjectName(QStringLiteral("stParticName"));
        nameLbl->setFixedWidth(120);
        rowLay->addWidget(nameLbl);

        auto* bar = new QProgressBar(row);
        bar->setRange(0, 100);
        bar->setValue(pct);
        bar->setFixedHeight(6);
        bar->setTextVisible(false);
        bar->setToolTip(tr("Aparece em %1 de %2 cena(s) (%3%)")
            .arg(res.sceneCount).arg(m_totalScenes).arg(pct));
        bar->setStyleSheet(QStringLiteral(R"(
            QProgressBar { background: %1; border: none; border-radius: 3px; }
            QProgressBar::chunk { background: %2; border-radius: 3px; }
        )").arg(Theme::hoverOverlay(), Theme::accentDefault()));
        rowLay->addWidget(bar, 1);

        auto* pctLbl = new QLabel(QStringLiteral("%1%").arg(pct), row);
        pctLbl->setObjectName(QStringLiteral("stParticPct"));
        pctLbl->setFixedWidth(32);
        pctLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rowLay->addWidget(pctLbl);

        m_participLay->addWidget(row);
    }
    m_avatarStripLay->addStretch();
}

void StatsPanel::rebuildChapterBars()
{
    if (!m_chapterBarsLay) return;

    while (m_chapterBarsLay->count() > 0) {
        QLayoutItem* it = m_chapterBarsLay->takeAt(0);
        if (QWidget* w = it->widget()) w->deleteLater();
        delete it;
    }

    if (!m_model || !m_wordCounter) {
        auto* empty = new QLabel(tr("Sem dados ainda."), m_chapterBarsHost);
        empty->setObjectName(QStringLiteral("stEmpty"));
        m_chapterBarsLay->addWidget(empty);
        m_chapterBarsLay->addStretch();
        return;
    }

    const QString activeMs = m_model->activeManuscriptId();
    QList<Chapter> chapters;
    for (const Chapter& ch : m_model->chapters())
        if (ch.manuscriptId == activeMs) chapters.append(ch);
    std::sort(chapters.begin(), chapters.end(), [](const Chapter& a, const Chapter& b) {
        return a.order < b.order;
    });

    if (chapters.isEmpty()) {
        auto* empty = new QLabel(tr("Nenhum capítulo neste manuscrito."), m_chapterBarsHost);
        empty->setObjectName(QStringLiteral("stEmpty"));
        m_chapterBarsLay->addWidget(empty);
        m_chapterBarsLay->addStretch();
        return;
    }

    QVector<int> values;
    values.reserve(chapters.size());
    for (const Chapter& ch : chapters) {
        const int totalWords = m_wordCounter->countChapter(ch.id);
        if (m_chapterMetric == ChapterMetric::Words) {
            values.append(totalWords);
        } else {
            const int dlgWords = m_dialogues ? m_dialogues->dialogueWordsForChapter(ch.id) : 0;
            values.append(totalWords > 0 ? (dlgWords * 100) / totalWords : 0);
        }
    }
    const int maxVal = qMax(1, *std::max_element(values.begin(), values.end()));

    for (int i = 0; i < chapters.size(); ++i) {
        const Chapter& ch = chapters.at(i);
        const int val = values.at(i);
        const int barH = qBound(4, (val * kChapterBarMaxHeight) / maxVal, kChapterBarMaxHeight);

        auto* col = new QWidget(m_chapterBarsHost);
        col->setMinimumHeight(kChapterBarMaxHeight + 24);
        auto* colLay = new QVBoxLayout(col);
        colLay->setContentsMargins(0, 0, 0, 0);
        colLay->setSpacing(2);
        colLay->addStretch(1);

        // QFrame, não QToolButton: um botão vazio (sem ícone/texto) não pinta
        // o background QSS de forma confiável (a área "de conteúdo" do
        // QAbstractButton some quando não há ícone/texto pra ancorá-la).
        // QFrame pinta o retângulo inteiro sempre — mesmo padrão de
        // elemCard/stCharPhoto. Clique tratado via eventFilter (ver abaixo).
        // Estilo inline (não a folha de estilo global) — descarta qualquer
        // dúvida de cascata/seletor não bater.
        auto* bar = new QFrame(col);
        bar->setAttribute(Qt::WA_StyledBackground, true);
        bar->setFixedSize(kChapterBarWidth, barH);
        bar->setCursor(Qt::PointingHandCursor);
        bar->setStyleSheet(QStringLiteral(
            "background-color: %1; border: none; border-radius: 4px;")
            .arg(Theme::accentDefault()));
        const QString chTitle = ch.title.isEmpty() ? m_model->chapterDisplayLabel(ch) : ch.title;
        const QString tip = (m_chapterMetric == ChapterMetric::Words)
            ? tr("%1: %2 palavra(s)").arg(chTitle).arg(val)
            : tr("%1: %2% diálogo").arg(chTitle).arg(val);
        bar->setToolTip(tip);
        bar->setProperty("chapterId", ch.id);
        bar->setProperty("manuscriptId", ch.manuscriptId);
        bar->installEventFilter(this);
        colLay->addWidget(bar, 0, Qt::AlignHCenter);

        auto* lbl = new QLabel(QString::number(i + 1), col);
        lbl->setObjectName(QStringLiteral("stParticPct"));
        lbl->setAlignment(Qt::AlignHCenter);
        lbl->setFixedWidth(kChapterBarWidth + 10);
        colLay->addWidget(lbl);

        m_chapterBarsLay->addWidget(col);
    }
    m_chapterBarsLay->addStretch();
}

void StatsPanel::openChapterStats(const QString& manuscriptId, const QString& chapterId)
{
    if (!m_wordCounter || !m_model) return;
    auto* dlg = new ChapterStatsDialog(m_wordCounter, m_dialogues, m_elements, m_model,
                                        manuscriptId, chapterId, this);
    dlg->show();
}

void StatsPanel::rebuildOverviewStats()
{
    if (!m_overviewStatsLabel) return;
    if (!m_model || !m_wordCounter) {
        m_overviewStatsLabel->setText(tr("Sem dados ainda."));
        return;
    }

    const int totalWords = m_wordCounter->countProject();

    const QString activeMs = m_model->activeManuscriptId();
    QList<Chapter> chapters;
    for (const Chapter& ch : m_model->chapters())
        if (ch.manuscriptId == activeMs) chapters.append(ch);

    QString biggest, smallest;
    int biggestWords = -1, smallestWords = -1;
    for (const Chapter& ch : chapters) {
        const int w = m_wordCounter->countChapter(ch.id);
        const QString title = ch.title.isEmpty() ? tr("(sem título)") : ch.title;
        if (biggestWords < 0 || w > biggestWords) { biggestWords = w; biggest = title; }
        if (smallestWords < 0 || w < smallestWords) { smallestWords = w; smallest = title; }
    }

    QHash<QString, int> bondsByType;
    if (m_model) {
        for (const CharacterBond& b : m_model->characterBonds())
            bondsByType[BondTypes::displayName(b.type)]++;
    }
    QStringList bondLines;
    for (auto it = bondsByType.constBegin(); it != bondsByType.constEnd(); ++it)
        bondLines << tr("%1: %2").arg(it.key()).arg(it.value());

    const int streak = m_wordCounter->currentStreak();
    const int longest = m_wordCounter->longestStreak();
    const int pages = m_wordCounter->estimatedPages();

    QStringList html;
    html << tr("Total de palavras (projeto): <b>%1</b>").arg(totalWords);
    html << tr("Capítulos no manuscrito: <b>%1</b> · Cenas: <b>%2</b>")
        .arg(m_totalChapters).arg(m_totalScenes);
    if (biggestWords >= 0) {
        html << tr("Maior capítulo: <b>%1</b> (%2 palavras)").arg(biggest).arg(biggestWords);
        html << tr("Menor capítulo: <b>%1</b> (%2 palavras)").arg(smallest).arg(smallestWords);
    }
    html << tr("Vínculos: %1").arg(bondLines.isEmpty() ? tr("nenhum") : bondLines.join(QStringLiteral(" · ")));
    html << tr("Sequência atual: %1 dia(s) · Recorde: %2 dia(s) · ~%3 página(s) estimadas")
        .arg(streak).arg(longest).arg(pages);

    m_overviewStatsLabel->setText(html.join(QStringLiteral("<br>")));
}

const DrawerItem* StatsPanel::drawerItemForElement(const QString& elementId) const
{
    if (!m_model || elementId.isEmpty()) return nullptr;
    for (const Drawer& d : m_model->drawers()) {
        for (const DrawerItem& it : d.items) {
            if (it.elementId == elementId) return &it;
        }
    }
    return nullptr;
}

void StatsPanel::openCharacter(const QString& elementId)
{
    m_currentElementId = elementId;
    rebuildCharacterPage();
    if (m_stack) m_stack->setCurrentIndex(1);
    if (m_backBtn) m_backBtn->setVisible(true);
}

void StatsPanel::backToOverview()
{
    m_currentElementId.clear();
    if (m_stack) m_stack->setCurrentIndex(0);
    if (m_backBtn) m_backBtn->setVisible(false);
    if (m_title) m_title->setText(tr("Estatísticas"));
}

void StatsPanel::rebuildCharacterPage()
{
    const Element* e = (m_elements && !m_currentElementId.isEmpty())
        ? m_elements->findElement(m_currentElementId) : nullptr;
    if (!e) return;

    if (m_title) m_title->setText(e->name.isEmpty() ? tr("(sem nome)") : e->name);

    if (m_charPhoto) {
        const QPixmap pm = AvatarUtils::decodeDataUrl(e->image);
        if (!pm.isNull()) {
            const QPixmap scaled = pm.scaled(m_charPhoto->size(),
                Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            const int dx = (scaled.width() - m_charPhoto->width()) / 2;
            const int dy = (scaled.height() - m_charPhoto->height()) / 2;
            m_charPhoto->setPixmap(scaled.copy(dx, dy, m_charPhoto->width(), m_charPhoto->height()));
        } else {
            m_charPhoto->setPixmap(QPixmap());
            m_charPhoto->setText(tr("Sem foto"));
        }
    }

    if (m_charName) m_charName->setText(e->name);

    if (m_charPresenceSummary) {
        const CharPresenceResult res = m_presenceResults.value(e->name.toLower());
        const int pct = (m_totalScenes > 0) ? qMin(100, (res.sceneCount * 100) / m_totalScenes) : 0;
        m_charPresenceSummary->setText(tr("Aparece em %1 de %2 cena(s) (%3%) — %4 de %5 capítulo(s)")
            .arg(res.sceneCount).arg(m_totalScenes).arg(pct)
            .arg(res.chapterCount).arg(m_totalChapters));
    }

    const DrawerItem* item = drawerItemForElement(m_currentElementId);

    if (m_charDialogueSummary) {
        int lines = 0, words = 0;
        if (m_dialogues) {
            lines = m_dialogues->dialoguesForCharacter(m_currentElementId).size();
            words = m_dialogues->dialogueWordsForCharacter(m_currentElementId);
        }
        m_charDialogueSummary->setText(tr("%1 fala(s) detectada(s), %2 palavra(s) faladas")
            .arg(lines).arg(words));
    }

    if (m_charStatusBtn) {
        const QString status = item ? item->charStatus : QString();
        m_charStatusBtn->setText(status.isEmpty() ? tr("Status: —") : tr("Status: %1").arg(status));
        m_charStatusBtn->setToolTip(item ? item->charStatusDetail : QString());
    }
    if (m_charLocationBtn) {
        const QString location = item ? item->charLocation : QString();
        m_charLocationBtn->setText(location.isEmpty() ? tr("Local: —") : tr("Local: %1").arg(location));
    }
    if (m_charInconsistencyWarning) {
        static const QStringList kAbsentStatuses = { tr("Morto"), tr("Desaparecido") };
        const QString status = item ? item->charStatus : QString();
        const CharPresenceResult res = m_presenceResults.value(e->name.toLower());
        const bool warn = !status.isEmpty() && kAbsentStatuses.contains(status) && res.sceneCount > 0;
        m_charInconsistencyWarning->setVisible(warn);
        if (warn) {
            m_charInconsistencyWarning->setText(
                tr("⚠ Aparece em %1 cena(s) após %2").arg(res.sceneCount).arg(status.toLower()));
        }
    }

    if (m_charBondsLay) {
        while (m_charBondsLay->count() > 0) {
            QLayoutItem* it = m_charBondsLay->takeAt(0);
            if (QWidget* w = it->widget()) w->deleteLater();
            delete it;
        }
        const QList<CharacterBond> bonds = (m_model && item)
            ? m_model->characterBondsForItem(item->id) : QList<CharacterBond>();
        if (bonds.isEmpty()) {
            auto* empty = new QLabel(tr("Nenhum vínculo registrado."), m_charBondsList);
            empty->setObjectName(QStringLiteral("stEmpty"));
            m_charBondsLay->addWidget(empty);
        } else {
            for (const CharacterBond& b : bonds) {
                const QString otherId = (b.fromItemId == item->id) ? b.toItemId : b.fromItemId;
                const DrawerItem* other = m_model ? m_model->findDrawerItem(otherId) : nullptr;
                const QString otherName = other ? other->title : tr("(desconhecido)");

                auto* row = new QWidget(m_charBondsList);
                auto* rowLay = new QHBoxLayout(row);
                rowLay->setContentsMargins(0, 0, 0, 0);
                rowLay->setSpacing(6);

                auto* dot = new QLabel(row);
                dot->setFixedSize(10, 10);
                QColor c(b.color);
                if (!c.isValid()) c = QColor(QStringLiteral("#4a9eff"));
                dot->setStyleSheet(QStringLiteral("background:%1; border-radius:5px;").arg(c.name()));
                rowLay->addWidget(dot);

                auto* lbl = new QLabel(tr("%1 — %2").arg(otherName, BondTypes::displayName(b.type)), row);
                lbl->setObjectName(QStringLiteral("stParticName"));
                rowLay->addWidget(lbl, 1);

                m_charBondsLay->addWidget(row);
            }
        }
        m_charBondsLay->addStretch();
    }

    if (m_charDoc) {
        QString html = DocPreview::resolveDrawerItemHtml(
            item, m_elements, m_cache, m_projectRoot, /*includePhoto=*/false);
        html = DocPreview::stripImages(html);
        html = DocPreview::stripForegroundColors(html);
        m_charDoc->setText(html.trimmed().isEmpty()
            ? tr("<i>Sem ficha ou documento vinculado a este personagem.</i>")
            : html);
    }

    rebuildChemistry();
}

void StatsPanel::rebuildChemistry()
{
    if (!m_chemLay) return;

    while (m_chemLay->count() > 0) {
        QLayoutItem* it = m_chemLay->takeAt(0);
        if (QWidget* w = it->widget()) w->deleteLater();
        delete it;
    }

    if (!m_dialogues || m_currentElementId.isEmpty()) {
        auto* empty = new QLabel(tr("Sem diálogos detectados ainda."), m_chemList);
        empty->setObjectName(QStringLiteral("stEmpty"));
        m_chemLay->addWidget(empty);
        m_chemLay->addStretch();
        return;
    }

    QVector<DialogueChemistry::PairStats> pairs =
        DialogueChemistry::chemistryForCharacter(m_dialogues->dialogues(), m_currentElementId);

    std::sort(pairs.begin(), pairs.end(), [this](const DialogueChemistry::PairStats& a,
                                                  const DialogueChemistry::PairStats& b) {
        switch (m_chemSort) {
        case ChemistrySort::Chapters: return a.chaptersTogether > b.chaptersTogether;
        case ChemistrySort::CrossDialogues: return a.crossDialogues > b.crossDialogues;
        case ChemistrySort::Scenes:
        default: return a.scenesTogether > b.scenesTogether;
        }
    });

    if (pairs.isEmpty()) {
        auto* empty = new QLabel(tr("Ainda não contracenou com ninguém."), m_chemList);
        empty->setObjectName(QStringLiteral("stEmpty"));
        m_chemLay->addWidget(empty);
        m_chemLay->addStretch();
        return;
    }

    for (const DialogueChemistry::PairStats& ps : pairs) {
        const Element* other = m_elements ? m_elements->findElement(ps.otherElementId) : nullptr;
        const QString otherName = other ? other->name : tr("(desconhecido)");
        const QString otherImage = other ? other->image : QString();

        auto* row = new QToolButton(m_chemList);
        row->setObjectName(QStringLiteral("stChemRow"));
        row->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        row->setIcon(QIcon(AvatarUtils::circularAvatar(otherImage, otherName, ps.otherElementId, kRowThumbSize)));
        row->setIconSize(QSize(kRowThumbSize, kRowThumbSize));
        row->setCursor(Qt::PointingHandCursor);
        row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        const QString statText = tr("%1 cena(s) · %2 capítulo(s) · %3 fala(s)")
            .arg(ps.scenesTogether).arg(ps.chaptersTogether).arg(ps.crossDialogues);
        row->setText(otherName + QStringLiteral("  —  ") + statText);

        const QString otherId = ps.otherElementId;
        connect(row, &QToolButton::clicked, this, [this, otherId]() { openChemistryPopup(otherId); });
        m_chemLay->addWidget(row);
    }
    m_chemLay->addStretch();
}

void StatsPanel::openChemistryPopup(const QString& otherElementId)
{
    if (!m_dialogues || !m_elements || m_currentElementId.isEmpty() || otherElementId.isEmpty()) return;
    const Element* a = m_elements->findElement(m_currentElementId);
    const Element* b = m_elements->findElement(otherElementId);
    if (!a || !b) return;

    const QVector<DialogueStore::Dialogue> shared =
        DialogueChemistry::sharedSceneDialogues(m_dialogues->dialogues(), m_currentElementId, otherElementId);

    auto* popup = new QFrame(nullptr);
    popup->setObjectName(QStringLiteral("stChemPopup"));
    // Qt::Tool (não Qt::Popup): precisa dar pra arrastar pelo cabeçalho, e
    // Qt::Popup fecha sozinho ao "sair" da área durante o drag (o cursor sai
    // do retângulo antigo no meio do arrasto) — Qt::Tool não tem esse
    // conflito, só perde o fechamento automático ao clicar fora, por isso o
    // botão de fechar explícito abaixo.
    popup->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setStyleSheet(QStringLiteral(
        "QFrame#stChemPopup { background: %1; border: 1px solid %2; border-radius: 8px; }")
        .arg(Theme::panelBackground(), Theme::panelBorder()));
    popup->setFixedSize(420, 480);

    auto* lay = new QVBoxLayout(popup);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    auto* headerRow = new QHBoxLayout();
    auto* header = new QLabel(tr("%1 & %2").arg(a->name, b->name), popup);
    header->setStyleSheet(QStringLiteral("color: %1; font-size: 14px; font-weight: 600;")
        .arg(Theme::textPrimary()));
    header->setCursor(Qt::SizeAllCursor);
    header->setToolTip(tr("Arraste pra mover"));
    header->setProperty("dragHandle", true);
    header->installEventFilter(this);
    headerRow->addWidget(header, 1);

    auto* closeBtn = new QToolButton(popup);
    closeBtn->setText(QStringLiteral("×"));
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFixedSize(22, 22);
    closeBtn->setStyleSheet(QStringLiteral(
        "QToolButton { background: transparent; border: none; font-size: 16px; color: %1; border-radius: 6px; }"
        "QToolButton:hover { color: %2; background: %3; }")
        .arg(Theme::textMuted(), Theme::textBright(), Theme::hoverOverlay()));
    connect(closeBtn, &QToolButton::clicked, popup, &QWidget::close);
    headerRow->addWidget(closeBtn);

    lay->addLayout(headerRow);

    auto* chapterPicker = new QComboBox(popup);
    chapterPicker->setStyleSheet(QStringLiteral(
        "QComboBox { color: %1; background: %2; border: 1px solid %3; border-radius: 6px; padding: 4px 8px; }")
        .arg(Theme::textPrimary(), Theme::inputBackground(), Theme::subtleBorder()));
    lay->addWidget(chapterPicker);

    auto* scroll = new QScrollArea(popup);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* inner = new QWidget(scroll);
    auto* innerLay = new QVBoxLayout(inner);
    innerLay->setContentsMargins(0, 0, 0, 0);
    innerLay->setSpacing(6);

    QHash<QString, QLabel*> chapterHeaders; // chapterId -> header widget (pra rolar até)
    QString lastChapterId;
    if (shared.isEmpty()) {
        auto* empty = new QLabel(tr("Nenhuma cena em comum encontrada."), inner);
        empty->setObjectName(QStringLiteral("stEmpty"));
        innerLay->addWidget(empty);
    }
    for (const DialogueStore::Dialogue& d : shared) {
        if (d.chapterId != lastChapterId) {
            lastChapterId = d.chapterId;
            const Chapter* ch = m_model ? m_model->findChapter(d.chapterId) : nullptr;
            const QString chTitle = ch ? ch->title : d.sourceLabel;
            auto* h = new QLabel(chTitle, inner);
            h->setStyleSheet(QStringLiteral(
                "color: %1; font-size: 11px; font-weight: 700; text-transform: uppercase;"
                " letter-spacing: 0.5px; padding-top: 6px;").arg(Theme::accentDefault()));
            innerLay->addWidget(h);
            chapterHeaders.insert(d.chapterId, h);
            chapterPicker->addItem(chTitle, d.chapterId);
        }
        const Element* speaker = m_elements->findElement(d.characterId);
        const QString speakerName = speaker ? speaker->name : tr("(desconhecido)");
        auto* line = new QLabel(QStringLiteral("<b>%1:</b> %2")
            .arg(speakerName.toHtmlEscaped(), d.text.toHtmlEscaped()), inner);
        line->setWordWrap(true);
        line->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(Theme::textPrimary()));
        innerLay->addWidget(line);
    }
    innerLay->addStretch();
    scroll->setWidget(inner);
    lay->addWidget(scroll, 1);

    connect(chapterPicker, &QComboBox::currentIndexChanged, this, [chapterPicker, chapterHeaders, scroll](int idx) {
        const QString chId = chapterPicker->itemData(idx).toString();
        if (QLabel* h = chapterHeaders.value(chId)) scroll->ensureWidgetVisible(h, 0, 0);
    });

    // Posiciona perto do centro do painel de Estatísticas (sem âncora específica
    // de card — quem chamou foi um botão de linha, não vale a pena carregar
    // globalPos só pra isso).
    const QPoint center = mapToGlobal(rect().center());
    popup->move(center.x() - popup->width() / 2, center.y() - popup->height() / 2);
    if (auto* screen = QGuiApplication::screenAt(center)) {
        const QRect avail = screen->availableGeometry();
        QPoint p = popup->pos();
        if (p.x() < avail.left()) p.setX(avail.left() + 4);
        if (p.y() < avail.top()) p.setY(avail.top() + 4);
        if (p.x() + popup->width() > avail.right()) p.setX(avail.right() - popup->width() - 4);
        if (p.y() + popup->height() > avail.bottom()) p.setY(avail.bottom() - popup->height() - 4);
        popup->move(p);
    }
    popup->show();
    popup->raise();
}

void StatsPanel::showStatusPicker(const QString& itemId, const QPoint& globalPos)
{
    if (!m_model) return;
    const DrawerItem* item = m_model->findDrawerItem(itemId);
    if (!item) return;

    const QString currentStatus = item->charStatus;
    const QString currentLocation = item->charLocation;

    auto* popup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setObjectName(QStringLiteral("stPicker"));
    popup->setStyleSheet(pickerPopupQss());
    popup->setFixedWidth(180);

    auto* lay = new QVBoxLayout(popup);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(2);

    auto* customInput = new QLineEdit(popup);
    customInput->setObjectName(QStringLiteral("pickerInput"));
    customInput->setPlaceholderText(tr("Status personalizado..."));
    customInput->setFixedHeight(28);
    lay->addWidget(customInput);

    auto* sep = new QFrame(popup);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("background: %1; border: none; margin: 2px 0;").arg(Theme::subtleBorder()));
    sep->setFixedHeight(1);
    lay->addWidget(sep);

    if (!currentStatus.isEmpty()) {
        auto* clearBtn = new QPushButton(tr("Limpar status"), popup);
        clearBtn->setObjectName(QStringLiteral("pickerOptionClear"));
        clearBtn->setFixedHeight(26);
        clearBtn->setCursor(Qt::PointingHandCursor);
        connect(clearBtn, &QPushButton::clicked, popup, [this, popup, itemId, currentLocation]() {
            m_model->updateDrawerItemConsistency(itemId, QString(), QString(), currentLocation);
            rebuildCharacterPage();
            popup->close();
        });
        lay->addWidget(clearBtn);
    }

    const QStringList presetStatuses = {
        tr("Morto"),       tr("Desaparecido"), tr("Ferido"),
        tr("Curado"),      tr("Apaixonado"),   tr("Raivoso"),
        tr("Feliz"),       tr("Triste"),       tr("Confuso"),
        tr("Traído"),      tr("Com medo"),     tr("Em fuga"),
        tr("Preso"),       tr("Transformado"), tr("Aliviado"),
        tr("Perdido"),
    };
    for (const auto& s : presetStatuses) {
        auto* btn = new QPushButton(s, popup);
        btn->setObjectName(QStringLiteral("pickerOption"));
        btn->setFixedHeight(26);
        btn->setCursor(Qt::PointingHandCursor);
        if (s == currentStatus) {
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; color: %2; border: none; border-radius: 5px; "
                "padding: 0 10px; text-align: left; font-size: 12px; "
                "font-family: 'Lora','Crimson Text',serif; }")
                .arg(Theme::hoverOverlay(), Theme::accentDefault()));
        }
        connect(btn, &QPushButton::clicked, popup, [this, popup, itemId, s, currentLocation]() {
            m_model->updateDrawerItemConsistency(itemId, s, QString(), currentLocation);
            rebuildCharacterPage();
            popup->close();
        });
        lay->addWidget(btn);
    }

    connect(customInput, &QLineEdit::returnPressed, popup, [this, popup, customInput, itemId, currentLocation]() {
        const QString val = customInput->text().trimmed();
        if (!val.isEmpty()) {
            m_model->updateDrawerItemConsistency(itemId, val, QString(), currentLocation);
            rebuildCharacterPage();
            popup->close();
        }
    });

    popup->adjustSize();
    QPoint pos = globalPos;
    const QRect screen = popup->screen() ? popup->screen()->availableGeometry() : QRect(0, 0, 1920, 1080);
    if (pos.x() + popup->width() > screen.right())  pos.setX(screen.right() - popup->width());
    if (pos.y() + popup->height() > screen.bottom()) pos.setY(globalPos.y() - popup->height() - 30);
    popup->move(pos);
    popup->show();
    customInput->setFocus();
}

void StatsPanel::showLocationPicker(const QString& itemId, const QPoint& globalPos)
{
    if (!m_model) return;
    const DrawerItem* item = m_model->findDrawerItem(itemId);
    if (!item) return;

    const QString currentStatus = item->charStatus;
    const QString currentDetail = item->charStatusDetail;
    const QString currentLocation = item->charLocation;

    QStringList settingNames;
    for (const auto& d : m_model->drawers()) {
        if (d.drawerElementType == QStringLiteral("setting")) {
            for (const auto& it : d.items) {
                if (!it.title.isEmpty()) settingNames.append(it.title);
            }
        }
    }

    auto* popup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setObjectName(QStringLiteral("stPicker"));
    popup->setStyleSheet(pickerPopupQss());
    popup->setFixedWidth(180);

    auto* lay = new QVBoxLayout(popup);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(2);

    auto* customInput = new QLineEdit(popup);
    customInput->setObjectName(QStringLiteral("pickerInput"));
    customInput->setPlaceholderText(tr("Local personalizado..."));
    customInput->setFixedHeight(28);
    lay->addWidget(customInput);

    auto* sep = new QFrame(popup);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("background: %1; border: none; margin: 2px 0;").arg(Theme::subtleBorder()));
    sep->setFixedHeight(1);
    lay->addWidget(sep);

    if (!currentLocation.isEmpty()) {
        auto* clearBtn = new QPushButton(tr("Limpar local"), popup);
        clearBtn->setObjectName(QStringLiteral("pickerOptionClear"));
        clearBtn->setFixedHeight(26);
        clearBtn->setCursor(Qt::PointingHandCursor);
        connect(clearBtn, &QPushButton::clicked, popup, [this, popup, itemId, currentStatus, currentDetail]() {
            m_model->updateDrawerItemConsistency(itemId, currentStatus, currentDetail, QString());
            rebuildCharacterPage();
            popup->close();
        });
        lay->addWidget(clearBtn);
    }

    if (settingNames.isEmpty()) {
        auto* emptyLbl = new QLabel(tr("Nenhum cenário criado."), popup);
        emptyLbl->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; padding: 4px 10px; font-style: italic;").arg(Theme::textMuted()));
        lay->addWidget(emptyLbl);
    } else {
        for (const auto& s : settingNames) {
            auto* btn = new QPushButton(s, popup);
            btn->setObjectName(QStringLiteral("pickerOption"));
            btn->setFixedHeight(26);
            btn->setCursor(Qt::PointingHandCursor);
            if (s == currentLocation) {
                btn->setStyleSheet(QStringLiteral(
                    "QPushButton { background: %1; color: %2; border: none; border-radius: 5px; "
                    "padding: 0 10px; text-align: left; font-size: 12px; "
                    "font-family: 'Lora','Crimson Text',serif; }")
                    .arg(Theme::hoverOverlay(), Theme::accentDefault()));
            }
            connect(btn, &QPushButton::clicked, popup, [this, popup, itemId, s, currentStatus, currentDetail]() {
                m_model->updateDrawerItemConsistency(itemId, currentStatus, currentDetail, s);
                rebuildCharacterPage();
                popup->close();
            });
            lay->addWidget(btn);
        }
    }

    connect(customInput, &QLineEdit::returnPressed, popup,
            [this, popup, customInput, itemId, currentStatus, currentDetail]() {
        const QString val = customInput->text().trimmed();
        if (!val.isEmpty()) {
            m_model->updateDrawerItemConsistency(itemId, currentStatus, currentDetail, val);
            rebuildCharacterPage();
            popup->close();
        }
    });

    popup->adjustSize();
    QPoint pos = globalPos;
    const QRect screen = popup->screen() ? popup->screen()->availableGeometry() : QRect(0, 0, 1920, 1080);
    if (pos.x() + popup->width() > screen.right())  pos.setX(screen.right() - popup->width());
    if (pos.y() + popup->height() > screen.bottom()) pos.setY(globalPos.y() - popup->height() - 30);
    popup->move(pos);
    popup->show();
    customInput->setFocus();
}

void StatsPanel::togglePanel()
{
    if (isPanelOpen()) closePanel();
    else openPanel();
}

void StatsPanel::openPanel()
{
    applyTheme();
    refresh();
    ancorRight();
    show();
    raise();
}

void StatsPanel::closePanel()
{
    hide();
}

bool StatsPanel::isPanelOpen() const
{
    return isVisible();
}

void StatsPanel::ancorRight()
{
    QWidget* p = parentWidget();
    if (!p) return;
    const int top = m_topInset + kMargin;
    const int h = qMax(200, p->height() - top - kMargin);
    resize(kPanelWidth, h);
    move(p->width() - width() - kMargin, top);
    m_positioned = true;
}

void StatsPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (!m_positioned) ancorRight();
}

bool StatsPanel::eventFilter(QObject* watched, QEvent* event)
{
    // Arrastar o popup de diálogos do par (Química) pelo cabeçalho — ele abre
    // no centro do painel de Estatísticas e pode ficar bloqueando a visão do
    // resto, então dá pra puxar ele pra outro canto.
    if (auto* handle = qobject_cast<QWidget*>(watched); handle && handle->property("dragHandle").toBool()) {
        QWidget* win = handle->window();
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_chemPopupDragging = true;
                m_chemPopupDragOffset = me->globalPosition().toPoint() - win->pos();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (m_chemPopupDragging) {
                auto* me = static_cast<QMouseEvent*>(event);
                win->move(me->globalPosition().toPoint() - m_chemPopupDragOffset);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_chemPopupDragging) {
                m_chemPopupDragging = false;
                return true;
            }
        }
    }

    // Clique numa barra do gráfico por capítulo (ver rebuildChapterBars).
    if (event->type() == QEvent::MouseButtonRelease) {
        if (auto* w = qobject_cast<QWidget*>(watched)) {
            const QString chapterId = w->property("chapterId").toString();
            if (!chapterId.isEmpty()) {
                auto* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton && w->rect().contains(me->pos())) {
                    openChapterStats(w->property("manuscriptId").toString(), chapterId);
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void StatsPanel::applyTheme()
{
    const QString bg      = Theme::panelBackground();
    const QString border  = Theme::borderStrong();
    const QString textPri = Theme::textPrimary();
    const QString textMut = Theme::textMuted();
    const QString textBrt = Theme::textBright();
    const QString cardBg  = Theme::inputBackground();
    const QString cardBd  = Theme::subtleBorder();
    const QString hover   = Theme::hoverStrong();
    const QString accent  = Theme::accentDefault();

    setStyleSheet(QStringLiteral(R"(
        #statsPanel {
            background: %1;
            border: 1px solid %2;
            border-radius: 10px;
        }
        #statsPanel QScrollArea { background: transparent; border: none; }
        #statsPanel QScrollArea > QWidget { background: transparent; }
        #stHeader { border-bottom: 1px solid %2; }
        #stTitle { color: %3; font-size: 15px; font-weight: 600; }
        #stClose, #stBack {
            color: %4;
            border: none;
            background: transparent;
            font-size: 16px;
            border-radius: 6px;
        }
        #stClose:hover, #stBack:hover { background: %7; color: %5; }
        #stSectionLabel {
            color: %4;
            font-size: 11px;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            padding: 4px 2px 2px 2px;
        }
        #stAvatarBtn {
            color: %3;
            background: transparent;
            border: none;
            border-radius: 8px;
            font-size: 11px;
            padding: 4px;
        }
        #stAvatarBtn:hover { background: %7; }
        #stParticName { color: %3; font-size: 12px; }
        #stParticPct { color: %4; font-size: 11px; }
        #stEmpty { color: %4; font-size: 13px; }
        #stCharPhoto {
            background: %6;
            border: 1px solid %9;
            border-radius: 8px;
            color: %4;
            font-size: 12px;
        }
        #stCharName { color: %3; font-size: 18px; font-weight: 600; }
        #stPresenceSummary { color: %4; font-size: 12px; }
        #stStatusBtn {
            background: %6;
            color: %4;
            border: 1px solid %9;
            border-radius: 5px;
            padding: 3px 8px;
            font-size: 11px;
            text-align: left;
        }
        #stStatusBtn:hover { background: %7; color: %5; border-color: %8; }
        #stInconsistencyWarning { color: %10; font-size: 11px; font-weight: 700; }
        #stChemSortBtn {
            color: %4;
            background: transparent;
            border: 1px solid %9;
            border-radius: 6px;
            padding: 3px 8px;
            font-size: 11px;
        }
        #stChemSortBtn:hover { background: %7; color: %3; }
        #stChemSortBtn::menu-indicator { image: none; width: 0; }
        #stChemRow {
            color: %3;
            background: %6;
            border: 1px solid %9;
            border-radius: 6px;
            padding: 4px 8px;
            font-size: 12px;
            text-align: left;
        }
        #stChemRow:hover { border-color: %8; }
        #stChapterBar {
            background: %8;
            border: none;
            border-radius: 4px;
        }
        #stChapterBar:hover { background: %5; }
        #stCharDocScroll {
            background: %6;
            border: 1px solid %9;
            border-radius: 8px;
        }
        #stCharDocScroll QWidget { background: transparent; }
        #stCharDoc {
            color: %3;
            padding: 10px;
            font-size: 13px;
        }
    )")
        .arg(bg, border, textPri, textMut, textBrt, cardBg, hover, accent, cardBd)
        .arg(Theme::accentWarning()));
}
