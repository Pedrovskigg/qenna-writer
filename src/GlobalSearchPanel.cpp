#include "GlobalSearchPanel.h"

#include "DocCache.h"
#include "IconUtils.h"
#include "ProjectModel.h"
#include "ProjectStorage.h"
#include "SceneUtils.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QStyle>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <algorithm>

namespace {
constexpr int kPanelWidth = 440;
constexpr int kPanelMaxHeight = 520;
constexpr int kDebounceMs = 180;

struct FilterDef {
    const char* id;
    const char* label;
};

const FilterDef kFilters[] = {
    { "all",      QT_TRANSLATE_NOOP("GlobalSearchPanel", "Todos") },
    { "chapters", QT_TRANSLATE_NOOP("GlobalSearchPanel", "Capítulos") },
    { "scenes",   QT_TRANSLATE_NOOP("GlobalSearchPanel", "Cenas") },
    { "drawers",  QT_TRANSLATE_NOOP("GlobalSearchPanel", "Gavetas") },
};
}

GlobalSearchPanel::GlobalSearchPanel(ProjectModel* model, DocCache* cache,
                                     const QString& projectRoot, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_cache(cache)
    , m_projectRoot(projectRoot)
{
    setObjectName(QStringLiteral("globalSearchPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(kPanelWidth);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 10, 12, 12);
    outer->setSpacing(8);

    // Linha do input
    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(6);

    m_input = new QLineEdit(this);
    m_input->setObjectName(QStringLiteral("gsInput"));
    m_input->setPlaceholderText(tr("Buscar no projeto..."));
    topRow->addWidget(m_input, 1);

    m_close = new QToolButton(this);
    m_close->setObjectName(QStringLiteral("gsCloseBtn"));
    m_close->setAutoRaise(true);
    m_close->setCursor(Qt::PointingHandCursor);
    m_close->setToolTip(tr("Fechar (Esc)"));
    const QIcon ic = IconUtils::loadToolbarIcon(QStringLiteral(":/icons/close.svg"),
        QColor(Theme::textMuted()),
        QColor(Theme::textPrimary()),
        QColor(Theme::textPrimary()),
        QSize(14, 14));
    if (!ic.isNull()) m_close->setIcon(ic);
    else m_close->setText(QStringLiteral("✕"));
    m_close->setFixedSize(28, 28);
    topRow->addWidget(m_close);

    outer->addLayout(topRow);

    // Filtros
    m_filterRow = new QWidget(this);
    m_filterRow->setObjectName(QStringLiteral("gsFilters"));
    m_filterLay = new QHBoxLayout(m_filterRow);
    m_filterLay->setContentsMargins(0, 0, 0, 0);
    m_filterLay->setSpacing(6);
    rebuildFilterRow();
    outer->addWidget(m_filterRow);
    m_filterRow->setVisible(false);

    // Lista de resultados
    m_resultList = new QListWidget(this);
    m_resultList->setObjectName(QStringLiteral("gsResults"));
    m_resultList->setFrameShape(QFrame::NoFrame);
    m_resultList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_resultList->setMaximumHeight(kPanelMaxHeight - 130);
    outer->addWidget(m_resultList, 1);

    m_emptyLabel = new QLabel(tr("Digite para buscar capítulos, cenas e itens."), this);
    m_emptyLabel->setObjectName(QStringLiteral("gsEmpty"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    outer->addWidget(m_emptyLabel);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(kDebounceMs);

    connect(m_input, &QLineEdit::textChanged, this, &GlobalSearchPanel::onQueryChanged);
    connect(m_close, &QToolButton::clicked, this, &GlobalSearchPanel::closePanel);
    connect(m_resultList, &QListWidget::itemActivated, this, &GlobalSearchPanel::onResultActivated);
    connect(m_resultList, &QListWidget::itemClicked, this, &GlobalSearchPanel::onResultActivated);
    connect(m_debounce, &QTimer::timeout, this, &GlobalSearchPanel::runSearch);

    applyTheme();
    hide();
}

void GlobalSearchPanel::openPanel()
{
    show();
    raise();
    m_input->setFocus();
    m_input->selectAll();
}

void GlobalSearchPanel::closePanel()
{
    hide();
}

void GlobalSearchPanel::applyTheme()
{
    setStyleSheet(QStringLiteral(R"(
        #globalSearchPanel {
            background: %1;
            border: 1px solid %2;
            border-radius: 10px;
        }
        #gsInput {
            background: %3;
            color: %4;
            border: 1px solid %2;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 13px;
            selection-background-color: %5;
        }
        #gsInput:focus { border-color: %5; }
        QToolButton#gsCloseBtn {
            border: none;
            background: transparent;
            color: %4;
            border-radius: 5px;
        }
        QToolButton#gsCloseBtn:hover { background: %6; }

        QPushButton[gsFilterChip="true"] {
            background: transparent;
            color: %7;
            border: 1px solid %2;
            border-radius: 12px;
            padding: 3px 10px;
            font-size: 11px;
        }
        QPushButton[gsFilterChip="true"]:hover { background: %6; }
        QPushButton[gsFilterChipActive="true"] {
            background: %5;
            color: %1;
            border-color: %5;
        }

        QListWidget#gsResults {
            background: transparent;
            border: none;
        }
        QListWidget#gsResults::item {
            color: %4;
            padding: 6px 8px;
            border-radius: 6px;
        }
        QListWidget#gsResults::item:hover { background: %6; }
        QListWidget#gsResults::item:selected { background: %5; color: %1; }

        #gsEmpty { color: %7; font-size: 11px; padding: 14px; }
    )")
        .arg(Theme::panelBackground())
        .arg(Theme::panelBorder())
        .arg(Theme::inputBackground())
        .arg(Theme::textPrimary())
        .arg(Theme::accentDefault())
        .arg(Theme::hoverOverlay())
        .arg(Theme::textMuted()));
}

void GlobalSearchPanel::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        event->accept();
        closePanel();
        return;
    }
    QWidget::keyPressEvent(event);
}

void GlobalSearchPanel::onQueryChanged(const QString& q)
{
    m_query = q.trimmed();
    m_debounce->start();
    if (m_query.isEmpty()) {
        m_results.clear();
        rebuildResultList();
    }
}

void GlobalSearchPanel::onFilterClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    m_filter = btn->property("filterId").toString();
    for (int i = 0; i < m_filterLay->count(); ++i) {
        auto* b = qobject_cast<QPushButton*>(m_filterLay->itemAt(i)->widget());
        if (!b) continue;
        const bool active = b->property("filterId").toString() == m_filter;
        b->setProperty("gsFilterChip", true);
        b->setProperty("gsFilterChipActive", active);
        b->style()->unpolish(b);
        b->style()->polish(b);
    }
    rebuildResultList();
}

void GlobalSearchPanel::onResultActivated(QListWidgetItem* item)
{
    if (!item) return;
    bool ok = false;
    const int idx = item->data(Qt::UserRole).toInt(&ok);
    if (!ok || idx < 0 || idx >= m_results.size()) return;
    const Result& r = m_results.at(idx);

    EditorHost::ViewMode vm;
    switch (r.type) {
    case RManuscript:
        vm.type = EditorHost::ManuscriptDoc;
        vm.manuscriptId = r.manuscriptId;
        break;
    case RChapter:
        vm.type = EditorHost::ChapterDoc;
        vm.manuscriptId = r.manuscriptId;
        vm.chapterId = r.chapterId;
        break;
    case RScene:
        vm.type = EditorHost::SceneDoc;
        vm.manuscriptId = r.manuscriptId;
        vm.chapterId = r.chapterId;
        vm.sceneIndex = r.sceneIndex;
        break;
    case RDrawerItem:
        vm.type = EditorHost::DrawerDoc;
        vm.itemId = r.itemId;
        break;
    }
    emit openRequested(vm, m_query);
    closePanel();
}

void GlobalSearchPanel::runSearch()
{
    m_results.clear();
    const QString needle = m_query.toLower();
    if (needle.isEmpty() || !m_model) {
        rebuildResultList();
        return;
    }

    // Drawer items
    for (const auto& d : m_model->drawers()) {
        for (const auto& it : d.items) {
            const QString title = it.title;
            const bool titleHit = title.toLower().contains(needle);
            bool contentHit = false;
            if (!titleHit) {
                const QString html = readItemHtml(d.key, it.id, it.file);
                if (!html.isEmpty()) {
                    contentHit = htmlToPlainText(html).toLower().contains(needle);
                }
            }
            if (titleHit || contentHit) {
                Result r;
                r.type = RDrawerItem;
                r.title = title.isEmpty() ? tr("Documento") : title;
                r.matchType = titleHit ? QStringLiteral("title") : QStringLiteral("content");
                r.drawerKey = d.key;
                r.drawerTitle = d.title.isEmpty() ? tr("Gaveta") : d.title;
                r.itemId = it.id;
                m_results.append(r);
            }
        }
    }

    // Manuscripts (só título)
    for (const auto& m : m_model->manuscripts()) {
        if (m.title.toLower().contains(needle)) {
            Result r;
            r.type = RManuscript;
            r.title = m.title.isEmpty() ? tr("Manuscrito") : m.title;
            r.matchType = QStringLiteral("title");
            r.manuscriptId = m.id;
            m_results.append(r);
        }
    }

    // Chapters + scenes
    for (const auto& c : m_model->chapters()) {
        const QString chTitle = c.title.isEmpty() ? tr("Capítulo") : c.title;
        const bool titleHit = chTitle.toLower().contains(needle);
        const QString html = readChapterHtml(c.manuscriptId, c.id, c.file);
        const QString plain = html.isEmpty() ? QString() : htmlToPlainText(html);
        const bool contentHit = !plain.isEmpty() && plain.toLower().contains(needle);

        if (titleHit || contentHit) {
            Result r;
            r.type = RChapter;
            r.title = chTitle;
            r.matchType = titleHit ? QStringLiteral("title") : QStringLiteral("content");
            r.manuscriptId = c.manuscriptId;
            r.chapterId = c.id;
            m_results.append(r);
        }

        if (!html.isEmpty() && c.scenes.size() > 1) {
            for (int si = 0; si < c.scenes.size(); ++si) {
                const Scene& sc = c.scenes.at(si);
                const QString scHtml = SceneUtils::getSceneHtml(html, si);
                const QString scTitle = sc.title.isEmpty() ? tr("Cena %1").arg(si + 1) : sc.title;
                const bool scTitleHit = scTitle.toLower().contains(needle);
                const bool scContentHit = !scHtml.isEmpty()
                    && htmlToPlainText(scHtml).toLower().contains(needle);
                if (scTitleHit || scContentHit) {
                    Result r;
                    r.type = RScene;
                    r.title = scTitle;
                    r.matchType = scTitleHit ? QStringLiteral("title") : QStringLiteral("content");
                    r.manuscriptId = c.manuscriptId;
                    r.chapterId = c.id;
                    r.chapterTitle = chTitle;
                    r.sceneIndex = si;
                    m_results.append(r);
                }
            }
        }
    }

    rebuildResultList();
}

void GlobalSearchPanel::rebuildFilterRow()
{
    while (m_filterLay->count() > 0) {
        QLayoutItem* it = m_filterLay->takeAt(0);
        if (auto* w = it->widget()) w->deleteLater();
        delete it;
    }
    for (const auto& f : kFilters) {
        auto* btn = new QPushButton(tr(f.label), m_filterRow);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("filterId", QString::fromLatin1(f.id));
        btn->setProperty("gsFilterChip", true);
        btn->setProperty("gsFilterChipActive", QString::fromLatin1(f.id) == m_filter);
        btn->setFlat(true);
        connect(btn, &QPushButton::clicked, this, &GlobalSearchPanel::onFilterClicked);
        m_filterLay->addWidget(btn);
    }
    m_filterLay->addStretch();
}

bool GlobalSearchPanel::resultPassesFilter(const Result& r) const
{
    if (m_filter == QStringLiteral("chapters")) {
        return r.type == RChapter || r.type == RManuscript;
    }
    if (m_filter == QStringLiteral("scenes")) {
        return r.type == RScene;
    }
    if (m_filter == QStringLiteral("drawers")) {
        return r.type == RDrawerItem;
    }
    return true;
}

QString GlobalSearchPanel::typeMeta(const Result& r) const
{
    switch (r.type) {
    case RChapter: return tr("Capítulo");
    case RManuscript: return tr("Manuscrito");
    case RScene: return tr("%1 • Cena").arg(r.chapterTitle.isEmpty() ? tr("Capítulo") : r.chapterTitle);
    case RDrawerItem: return r.drawerTitle.isEmpty() ? tr("Documento") : r.drawerTitle;
    }
    return QString();
}

void GlobalSearchPanel::rebuildResultList()
{
    m_resultList->clear();

    m_filterRow->setVisible(!m_query.isEmpty());

    if (m_query.isEmpty()) {
        m_emptyLabel->setText(tr("Digite para buscar capítulos, cenas e itens."));
        m_emptyLabel->setVisible(true);
        m_resultList->setVisible(false);
        return;
    }

    QVector<int> directIdx;
    QVector<int> citedIdx;
    for (int i = 0; i < m_results.size(); ++i) {
        if (!resultPassesFilter(m_results.at(i))) continue;
        if (m_results.at(i).matchType == QStringLiteral("title")) directIdx.append(i);
        else citedIdx.append(i);
    }

    if (directIdx.isEmpty() && citedIdx.isEmpty()) {
        m_emptyLabel->setText(tr("Nada encontrado."));
        m_emptyLabel->setVisible(true);
        m_resultList->setVisible(false);
        return;
    }

    m_emptyLabel->setVisible(false);
    m_resultList->setVisible(true);

    auto addEntry = [this](int idx) {
        const Result& r = m_results.at(idx);
        auto* it = new QListWidgetItem(m_resultList);
        it->setData(Qt::UserRole, idx);
        const QString text = QStringLiteral("%1\n%2").arg(r.title, typeMeta(r));
        it->setText(text);
    };

    auto addHeader = [this](const QString& label) {
        auto* it = new QListWidgetItem(label, m_resultList);
        it->setFlags(Qt::NoItemFlags);
        QFont f = it->font();
        f.setPointSize(qMax(8, f.pointSize() - 1));
        it->setFont(f);
        it->setForeground(QColor(Theme::textMuted()));
        it->setTextAlignment(Qt::AlignLeft);
    };

    for (int i : directIdx) addEntry(i);
    if (!citedIdx.isEmpty()) {
        addHeader(tr("É citado em..."));
        for (int i : citedIdx) addEntry(i);
    }
}

QString GlobalSearchPanel::htmlToPlainText(const QString& html) const
{
    if (html.isEmpty()) return QString();
    QTextDocument doc;
    doc.setHtml(html);
    return doc.toPlainText();
}

QString GlobalSearchPanel::readItemHtml(const QString& /*drawerKey*/, const QString& itemId,
                                        const QString& fileRel) const
{
    if (m_cache) {
        const QString key = DocCache::itemKey(itemId);
        if (m_cache->has(key)) return m_cache->get(key);
    }
    if (m_projectRoot.isEmpty() || fileRel.isEmpty()) return QString();
    const QString abs = ProjectStorage::joinPath(m_projectRoot, fileRel);
    bool ok = false;
    return ProjectStorage::readText(abs, &ok);
}

QString GlobalSearchPanel::readChapterHtml(const QString& manuscriptId, const QString& chapterId,
                                           const QString& fileRel) const
{
    if (m_cache) {
        const QString key = DocCache::chapterKey(manuscriptId, chapterId);
        if (m_cache->has(key)) return m_cache->get(key);
    }
    if (m_projectRoot.isEmpty() || fileRel.isEmpty()) return QString();
    bool ok = false;
    return ProjectStorage::readChapter(m_projectRoot, fileRel, &ok);
}
