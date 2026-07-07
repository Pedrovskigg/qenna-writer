#include "MentionPopup.h"

#include "ConstrutorStore.h"
#include "ProjectModel.h"
#include "Theme.h"

#include <algorithm>
#include <functional>
#include <QEvent>
#include <QKeyEvent>
#include <QListWidget>
#include <QPointer>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTimer>
#include <QWidget>

// Chaves especiais de navegação (não são drawer keys reais).
static constexpr auto kPortalChapters   = "__portal_ch__";
static constexpr auto kPortalConstrutor = "__portal_ctr__";
static constexpr auto kNavBack          = "__nav_back__";
static constexpr auto kMsChapter        = "__ms_ch__";
static constexpr auto kMsScene          = "__ms_sc__";
static constexpr auto kCtrSystem        = "__ctr_system__";
static constexpr auto kCtrNode          = "__construtor_node__";

MentionPopup::MentionPopup(ProjectModel* model, QWidget* ownerWindow, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_owner(ownerWindow)
{
    m_list = new QListWidget(m_owner);
    m_list->setObjectName(QStringLiteral("mentionPopup"));
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setUniformItemSizes(true);
    m_list->setMouseTracking(true);
    m_list->hide();
    m_list->setStyleSheet(QStringLiteral(
        "QListWidget#mentionPopup {"
        "  background: %1; color: %2;"
        "  border: 1px solid %3; border-radius: 6px; padding: 4px; outline: none;"
        "}"
        "QListWidget#mentionPopup::item { padding: 5px 8px; border-radius: 4px; }"
        "QListWidget#mentionPopup::item:selected { background: %4; color: %5; }"
    ).arg(Theme::panelBackground(), Theme::textPrimary(), Theme::subtleBorder(),
          Theme::accentDefault(), Theme::textBright()));

    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem*) { confirm(); });
}

void MentionPopup::attach(QTextEdit* editor)
{
    if (!editor) return;
    editor->installEventFilter(this);
    connect(editor, &QTextEdit::textChanged, this, [this, editor]() {
        if (editor->hasFocus()) updateForEditor(editor);
    });
    connect(editor, &QTextEdit::cursorPositionChanged, this, [this, editor]() {
        if (m_list->isVisible() && editor->hasFocus()) updateForEditor(editor);
    });
    connect(editor->document(), &QTextDocument::contentsChange, this,
            [this, editor](int pos, int removed, int added) {
        onContentsChange(editor, pos, removed, added);
    });
}

void MentionPopup::onContentsChange(QTextEdit* ed, int pos, int removed, int added)
{
    if (m_insertingMention || m_cleaningAnchor || added <= 0) return;
    if (removed != 0) return;
    if (added != 1) return;
    QTextDocument* doc = ed->document();

    QTextCursor probe(doc);
    probe.setPosition(pos);
    probe.setPosition(qMin(pos + 1, doc->characterCount()), QTextCursor::KeepAnchor);
    const QTextCharFormat fmt = probe.charFormat();
    if (!fmt.isAnchor() || !fmt.anchorHref().startsWith(QStringLiteral("ref:"))) return;

    QPointer<QTextEdit> edp(ed);
    const int from = pos;
    const int to = pos + added;
    QTimer::singleShot(0, this, [this, edp, from, to]() {
        if (!edp) return;
        m_cleaningAnchor = true;
        QTextCursor c(edp->document());
        c.setPosition(from);
        c.setPosition(qMin(to, edp->document()->characterCount()), QTextCursor::KeepAnchor);
        QTextCharFormat clr;
        clr.setAnchor(false);
        clr.setProperty(QTextFormat::AnchorHref, QString());
        c.mergeCharFormat(clr);
        m_cleaningAnchor = false;
        emit documentTouched();
    });
}

// ---- Detecção do @ e despacho ----

void MentionPopup::updateForEditor(QTextEdit* ed)
{
    QTextCursor cur = ed->textCursor();
    if (cur.hasSelection()) { hidePopup(); return; }
    const int pos = cur.position();

    QTextCursor lineCur(ed->document());
    lineCur.setPosition(cur.block().position());
    lineCur.setPosition(pos, QTextCursor::KeepAnchor);
    const QString before = lineCur.selectedText();

    const int at = before.lastIndexOf(QLatin1Char('@'));
    if (at < 0) { hidePopup(); return; }
    if (at > 0 && !before.at(at - 1).isSpace()) { hidePopup(); return; }
    const QString query = before.mid(at + 1);
    if (query.contains(QLatin1Char(' '))) { hidePopup(); return; }

    m_activeEditor = ed;
    m_atPos = cur.block().position() + at;
    m_currentQuery = query;
    rebuildList(query);
    if (!hasSelectableItems()) { hidePopup(); return; }
    showBelowCursor(ed);
}

// ---- Construção da lista ----

void MentionPopup::rebuildList(const QString& query)
{
    m_list->clear();
    const QString q = query.trimmed().toLower();

    // Atalho de teclado: @ch1 ou @ch1-sc2 — resolve independente do nível.
    if (m_includeManuscripts && m_model) {
        static const QRegularExpression rxShort(
            QStringLiteral("^ch(\\d+)(?:-sc(\\d+))?$"));
        const auto m = rxShort.match(q);
        if (m.hasMatch()) {
            buildShorthand(m);
            selectFirstSelectable();
            return;
        }
    }

    switch (m_level) {
    case Level::Root:                  rebuildRoot(q);                  break;
    case Level::Chapters:               rebuildChapters();               break;
    case Level::Scenes:                 rebuildScenes();                 break;
    case Level::ConstrutorSystems:      rebuildConstrutorSystems();      break;
    case Level::ConstrutorSystemNodes:  rebuildConstrutorSystemNodes();  break;
    }
    selectFirstSelectable();
}

void MentionPopup::rebuildRoot(const QString& q)
{
    QList<DocEntry> docs = allDocs();
    std::stable_sort(docs.begin(), docs.end(), [&q](const DocEntry& a, const DocEntry& b) {
        const bool as = a.title.toLower().startsWith(q);
        const bool bs = b.title.toLower().startsWith(q);
        if (as != bs) return as;
        return a.title.toLower() < b.title.toLower();
    });
    int added = 0;
    for (const DocEntry& d : docs) {
        if (!q.isEmpty() && !d.title.toLower().contains(q)) continue;
        m_list->addItem(makeDocItem(d));
        if (++added >= 50) break;
    }
    // Portal de capítulos: sempre visível no fim (não filtrado pela query).
    if (m_includeManuscripts && m_model && !m_model->chapters().isEmpty())
        addPortalItem(tr("Capítulos"), QLatin1String(kPortalChapters));
    // Portal do Construtor: idem, sempre visível se houver algum sistema.
    if (m_construtorStore && !m_construtorStore->systems().isEmpty())
        addPortalItem(tr("Construtor"), QLatin1String(kPortalConstrutor));
}

void MentionPopup::rebuildChapters()
{
    addBackItem(tr("← Voltar"));
    if (!m_model) return;
    for (const Chapter& ch : m_model->chapters()) {
        const QString t = ch.title.trimmed();
        if (t.isEmpty()) continue;
        auto* item = new QListWidgetItem(t);
        item->setData(Qt::UserRole,     QLatin1String(kMsChapter));
        item->setData(Qt::UserRole + 1, QStringLiteral("%1:%2").arg(ch.manuscriptId, ch.id));
        item->setData(Qt::UserRole + 2, t);
        m_list->addItem(item);
    }
}

void MentionPopup::rebuildScenes()
{
    addBackItem(m_drillChapterTitle.isEmpty()
        ? tr("← Capítulo") : QStringLiteral("← %1").arg(m_drillChapterTitle));
    const Chapter* ch = findDrillChapter();
    if (!ch) return;
    for (int i = 0; i < ch->scenes.size(); ++i) {
        const Scene& sc = ch->scenes.at(i);
        const QString t = sc.title.trimmed().isEmpty()
            ? tr("Cena %1").arg(i + 1) : sc.title.trimmed();
        auto* item = new QListWidgetItem(t);
        item->setData(Qt::UserRole,     QLatin1String(kMsScene));
        item->setData(Qt::UserRole + 1,
            QStringLiteral("%1:%2:%3").arg(m_drillManuscriptId, m_drillChapterId).arg(i));
        item->setData(Qt::UserRole + 2, t);
        m_list->addItem(item);
    }
}

void MentionPopup::rebuildConstrutorSystems()
{
    addBackItem(tr("← Voltar"));
    if (!m_construtorStore) return;
    for (const auto& sys : m_construtorStore->systems()) {
        auto* item = new QListWidgetItem(sys.name + QStringLiteral("  ▸"));
        item->setData(Qt::UserRole,     QLatin1String(kCtrSystem));
        item->setData(Qt::UserRole + 1, sys.id);
        item->setData(Qt::UserRole + 2, sys.name);
        m_list->addItem(item);
    }
}

void MentionPopup::rebuildConstrutorSystemNodes()
{
    addBackItem(m_drillConstrutorSystemName.isEmpty()
        ? tr("← Construtor") : QStringLiteral("← %1").arg(m_drillConstrutorSystemName));
    if (!m_construtorStore) return;
    const ConstrutorStore::System* sys = m_construtorStore->system(m_drillConstrutorSystemId);
    if (!sys) return;

    // Nós só desse sistema, achatados por breadcrumb (sem repetir o nome do
    // sistema — já está no item "← Voltar" acima). Nós têm profundidade
    // arbitrária, então não dá pra espelhar o drill-down fixo de
    // Capítulos→Cenas dentro do próprio sistema.
    std::function<void(const QString&, const QList<ConstrutorStore::Node>&)> walk;
    walk = [&](const QString& breadcrumb, const QList<ConstrutorStore::Node>& nodes) {
        for (const auto& n : nodes) {
            const QString path = breadcrumb.isEmpty() ? n.name : breadcrumb + QStringLiteral(" ▸ ") + n.name;
            auto* item = new QListWidgetItem(path);
            item->setData(Qt::UserRole,     QLatin1String(kCtrNode));
            item->setData(Qt::UserRole + 1, QStringLiteral("%1:%2").arg(sys->id, n.id));
            item->setData(Qt::UserRole + 2, n.name);
            m_list->addItem(item);
            walk(path, n.children);
        }
    };
    walk(QString(), sys->nodes);
}

void MentionPopup::buildShorthand(const QRegularExpressionMatch& m)
{
    const int chIdx = m.captured(1).toInt() - 1;
    const auto& chapters = m_model->chapters();
    if (chIdx < 0 || chIdx >= chapters.size()) return;
    const Chapter& ch = chapters.at(chIdx);

    if (m.captured(2).isEmpty()) {
        // @ch1 → capítulo
        const QString t = ch.title.trimmed().isEmpty()
            ? tr("Capítulo %1").arg(chIdx + 1) : ch.title.trimmed();
        auto* item = new QListWidgetItem(t);
        item->setData(Qt::UserRole,     QLatin1String(kMsChapter));
        item->setData(Qt::UserRole + 1, QStringLiteral("%1:%2").arg(ch.manuscriptId, ch.id));
        item->setData(Qt::UserRole + 2, t);
        m_list->addItem(item);
    } else {
        // @ch1-sc2 → cena
        const int scIdx = m.captured(2).toInt() - 1;
        if (scIdx < 0 || scIdx >= ch.scenes.size()) return;
        const Scene& sc = ch.scenes.at(scIdx);
        const QString t = sc.title.trimmed().isEmpty()
            ? tr("Cena %1").arg(scIdx + 1) : sc.title.trimmed();
        auto* item = new QListWidgetItem(
            QStringLiteral("%1   ·  %2").arg(t, ch.title.trimmed().isEmpty()
                ? tr("Capítulo %1").arg(chIdx + 1) : ch.title.trimmed()));
        item->setData(Qt::UserRole,     QLatin1String(kMsScene));
        item->setData(Qt::UserRole + 1,
            QStringLiteral("%1:%2:%3").arg(ch.manuscriptId, ch.id).arg(scIdx));
        item->setData(Qt::UserRole + 2, t);
        m_list->addItem(item);
    }
}

// ---- Helpers de item ----

QListWidgetItem* MentionPopup::makeDocItem(const DocEntry& d) const
{
    auto* item = new QListWidgetItem(
        d.subtitle.isEmpty() ? d.title
                             : QStringLiteral("%1   ·  %2").arg(d.title, d.subtitle));
    item->setData(Qt::UserRole,     d.drawerKey);
    item->setData(Qt::UserRole + 1, d.itemId);
    item->setData(Qt::UserRole + 2, d.title);
    return item;
}

void MentionPopup::addPortalItem(const QString& text, const QString& key)
{
    auto* item = new QListWidgetItem(text + QStringLiteral("  ▶"));
    item->setData(Qt::UserRole, key);
    item->setForeground(QColor(Theme::accentDefault()));
    m_list->addItem(item);
}

void MentionPopup::addBackItem(const QString& text)
{
    auto* item = new QListWidgetItem(text);
    item->setData(Qt::UserRole, QLatin1String(kNavBack));
    item->setForeground(QColor(Theme::textMuted()));
    m_list->addItem(item);
}

void MentionPopup::selectFirstSelectable()
{
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->flags() & Qt::ItemIsSelectable) {
            m_list->setCurrentRow(i);
            return;
        }
    }
}

bool MentionPopup::hasSelectableItems() const
{
    for (int i = 0; i < m_list->count(); ++i)
        if (m_list->item(i)->flags() & Qt::ItemIsSelectable) return true;
    return false;
}

const Chapter* MentionPopup::findDrillChapter() const
{
    if (!m_model || m_drillChapterId.isEmpty()) return nullptr;
    return m_model->findChapter(m_drillChapterId);
}

QList<MentionPopup::DocEntry> MentionPopup::allDocs() const
{
    QList<DocEntry> out;
    if (!m_model) return out;
    for (const Drawer& d : m_model->drawers()) {
        for (const DrawerItem& it : d.items) {
            if (it.title.trimmed().isEmpty()) continue;
            out.append({ d.key, it.id, it.title, d.title });
        }
    }
    return out;
}

// ---- Posicionamento ----

void MentionPopup::showBelowCursor(QTextEdit* ed)
{
    constexpr int kRowH = 30;
    constexpr int kMaxVisible = 6;
    const QRect r = ed->cursorRect(ed->textCursor());
    const QPoint belowPt = m_owner
        ? m_owner->mapFromGlobal(ed->viewport()->mapToGlobal(r.bottomLeft())) : r.bottomLeft();
    const QPoint abovePt = m_owner
        ? m_owner->mapFromGlobal(ed->viewport()->mapToGlobal(r.topLeft())) : r.topLeft();

    const int rows = qMin(kMaxVisible, m_list->count());
    const int h = rows * kRowH + 10;
    const int w = 280;

    int x = belowPt.x();
    if (m_owner) { x = qMin(x, m_owner->width() - w - 8); x = qMax(8, x); }

    int y = belowPt.y() + 2;
    if (m_owner && y + h > m_owner->height() - 4)
        y = abovePt.y() - h - 2;
    if (y < 4) y = 4;

    m_list->setGeometry(x, y, w, h);
    m_list->show();
    m_list->raise();
}

void MentionPopup::hidePopup()
{
    if (m_list) m_list->hide();
    m_atPos = -1;
    m_activeEditor = nullptr;
    m_currentQuery.clear();
    m_level = Level::Root;
    m_drillManuscriptId.clear();
    m_drillChapterId.clear();
    m_drillChapterTitle.clear();
    m_drillConstrutorSystemId.clear();
    m_drillConstrutorSystemName.clear();
}

void MentionPopup::drillBack()
{
    if (m_level == Level::Scenes) {
        m_level = Level::Chapters;
        m_drillChapterId.clear();
        m_drillManuscriptId.clear();
        m_drillChapterTitle.clear();
    } else if (m_level == Level::ConstrutorSystemNodes) {
        m_level = Level::ConstrutorSystems;
        m_drillConstrutorSystemId.clear();
        m_drillConstrutorSystemName.clear();
    } else {
        m_level = Level::Root;
    }
    rebuildList(m_currentQuery);
    if (m_activeEditor) showBelowCursor(m_activeEditor);
}

// ---- Navegação e confirmação ----

void MentionPopup::moveSel(int delta)
{
    const int n = m_list->count();
    if (n == 0) return;
    int row = m_list->currentRow();
    for (int tries = 0; tries < n; ++tries) {
        row = ((row + delta) % n + n) % n;
        if (m_list->item(row)->flags() & Qt::ItemIsSelectable) {
            m_list->setCurrentRow(row);
            return;
        }
    }
}

void MentionPopup::confirm()
{
    if (!m_activeEditor || m_atPos < 0) { hidePopup(); return; }
    QListWidgetItem* item = m_list->currentItem();
    if (!item && m_list->count() > 0) item = m_list->item(0);
    if (!item) { hidePopup(); return; }

    const QString key   = item->data(Qt::UserRole).toString();
    const QString id    = item->data(Qt::UserRole + 1).toString();
    const QString title = item->data(Qt::UserRole + 2).toString();

    // Portal: drill in para capítulos.
    if (key == QLatin1String(kPortalChapters)) {
        m_level = Level::Chapters;
        rebuildList(m_currentQuery);
        showBelowCursor(m_activeEditor);
        return;
    }
    // Portal: drill in para a lista de sistemas do Construtor.
    if (key == QLatin1String(kPortalConstrutor)) {
        m_level = Level::ConstrutorSystems;
        rebuildList(m_currentQuery);
        showBelowCursor(m_activeEditor);
        return;
    }
    // Voltar um nível.
    if (key == QLatin1String(kNavBack)) {
        drillBack();
        return;
    }
    // Capítulo na lista de drill: drill into scenes.
    if (key == QLatin1String(kMsChapter) && m_level == Level::Chapters) {
        const int sep = id.indexOf(QLatin1Char(':'));
        m_drillManuscriptId = id.left(sep);
        m_drillChapterId    = id.mid(sep + 1);
        m_drillChapterTitle = title;
        m_level = Level::Scenes;
        rebuildList(m_currentQuery);
        showBelowCursor(m_activeEditor);
        return;
    }
    // Sistema na lista de sistemas do Construtor: drill in pros nós dele.
    if (key == QLatin1String(kCtrSystem) && m_level == Level::ConstrutorSystems) {
        m_drillConstrutorSystemId   = id;
        m_drillConstrutorSystemName = title;
        m_level = Level::ConstrutorSystemNodes;
        rebuildList(m_currentQuery);
        showBelowCursor(m_activeEditor);
        return;
    }

    // Inserção real (gaveta, cena via drill, ou capítulo via shorthand @ch1).
    QTextEdit* ed = m_activeEditor;
    const int endPos = ed->textCursor().position();

    m_insertingMention = true;
    QTextCursor cur = ed->textCursor();
    cur.beginEditBlock();
    cur.setPosition(m_atPos);
    cur.setPosition(endPos, QTextCursor::KeepAnchor);
    cur.removeSelectedText();

    QTextCharFormat base = cur.charFormat();
    base.setAnchor(false);
    base.clearProperty(QTextFormat::IsAnchor);
    base.clearProperty(QTextFormat::AnchorHref);
    base.clearProperty(QTextFormat::AnchorName);

    const int linkStart = cur.position();
    cur.insertText(title, base);
    const int linkEnd = cur.position();
    cur.insertText(QStringLiteral(" "), base);

    QTextCursor linkCur(ed->document());
    linkCur.setPosition(linkStart);
    linkCur.setPosition(linkEnd, QTextCursor::KeepAnchor);
    QTextCharFormat anchorFmt;
    anchorFmt.setAnchor(true);
    anchorFmt.setAnchorHref(QStringLiteral("ref:%1:%2").arg(key, id));
    linkCur.mergeCharFormat(anchorFmt);

    cur.endEditBlock();

    QTextCursor after(ed->document());
    after.setPosition(linkEnd + 1);
    ed->setTextCursor(after);
    ed->setCurrentCharFormat(base);
    m_insertingMention = false;
    hidePopup();
}

bool MentionPopup::eventFilter(QObject* watched, QEvent* event)
{
    if (m_list && m_list->isVisible() && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        switch (ke->key()) {
        case Qt::Key_Up:     moveSel(-1); return true;
        case Qt::Key_Down:   moveSel(1);  return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:
        case Qt::Key_Space:  confirm();   return true;
        case Qt::Key_Escape:
            if (m_level != Level::Root) { drillBack(); return true; }
            hidePopup();
            return true;
        default: break;
        }
    }
    if (event->type() == QEvent::FocusOut) hidePopup();
    return QObject::eventFilter(watched, event);
}
