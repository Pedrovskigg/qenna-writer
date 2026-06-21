#include "MentionPopup.h"

#include "ProjectModel.h"
#include "Theme.h"

#include <algorithm>
#include <QEvent>
#include <QKeyEvent>
#include <QListWidget>
#include <QTextBlock>
#include <QTextEdit>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QWidget>

MentionPopup::MentionPopup(ProjectModel* model, QWidget* ownerWindow, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_owner(ownerWindow)
{
    // Lista flutuante como filha da janela (não rouba o foco do editor).
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

void MentionPopup::updateForEditor(QTextEdit* ed)
{
    QTextCursor cur = ed->textCursor();
    if (cur.hasSelection()) { hidePopup(); return; }
    const int pos = cur.position();

    // Texto do início do bloco até o cursor.
    QTextCursor lineCur(ed->document());
    lineCur.setPosition(cur.block().position());
    lineCur.setPosition(pos, QTextCursor::KeepAnchor);
    const QString before = lineCur.selectedText();

    const int at = before.lastIndexOf(QLatin1Char('@'));
    if (at < 0) { hidePopup(); return; }
    // O '@' deve iniciar palavra (precedido de espaço/início).
    if (at > 0 && !before.at(at - 1).isSpace()) { hidePopup(); return; }
    const QString query = before.mid(at + 1);
    if (query.contains(QLatin1Char(' '))) { hidePopup(); return; }

    m_activeEditor = ed;
    m_atPos = cur.block().position() + at;
    rebuildList(query);
    if (m_list->count() == 0) { hidePopup(); return; }
    showBelowCursor(ed);
}

void MentionPopup::rebuildList(const QString& query)
{
    m_list->clear();
    const QString q = query.trimmed().toLower();
    QList<DocEntry> docs = allDocs();

    // Ordena: quem começa com a query primeiro, depois alfabético.
    std::stable_sort(docs.begin(), docs.end(), [&q](const DocEntry& a, const DocEntry& b) {
        const bool as = a.title.toLower().startsWith(q);
        const bool bs = b.title.toLower().startsWith(q);
        if (as != bs) return as;
        return a.title.toLower() < b.title.toLower();
    });

    int added = 0;
    for (const DocEntry& d : docs) {
        if (!q.isEmpty() && !d.title.toLower().contains(q)) continue;
        auto* item = new QListWidgetItem(
            d.subtitle.isEmpty() ? d.title : QStringLiteral("%1   ·  %2").arg(d.title, d.subtitle));
        item->setData(Qt::UserRole, d.drawerKey);
        item->setData(Qt::UserRole + 1, d.itemId);
        item->setData(Qt::UserRole + 2, d.title);
        m_list->addItem(item);
        if (++added >= 50) break;   // teto de segurança; o scroll cuida do resto
    }
    if (m_list->count() > 0) m_list->setCurrentRow(0);
}

void MentionPopup::showBelowCursor(QTextEdit* ed)
{
    constexpr int kRowH = 30;
    constexpr int kMaxVisible = 6;   // mostra até 6; o resto rola
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

    // Abre abaixo do cursor; se não couber até o rodapé, abre acima.
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
}

void MentionPopup::moveSel(int delta)
{
    const int n = m_list->count();
    if (n == 0) return;
    int row = m_list->currentRow() + delta;
    row = (row % n + n) % n;
    m_list->setCurrentRow(row);
}

void MentionPopup::confirm()
{
    if (!m_activeEditor || m_atPos < 0) { hidePopup(); return; }
    QListWidgetItem* item = m_list->currentItem();
    if (!item && m_list->count() > 0) item = m_list->item(0);
    if (!item) { hidePopup(); return; }

    const QString drawerKey = item->data(Qt::UserRole).toString();
    const QString itemId    = item->data(Qt::UserRole + 1).toString();
    const QString title     = item->data(Qt::UserRole + 2).toString();

    QTextEdit* ed = m_activeEditor;
    const int endPos = ed->textCursor().position();

    QTextCursor cur = ed->textCursor();
    cur.beginEditBlock();
    cur.setPosition(m_atPos);
    cur.setPosition(endPos, QTextCursor::KeepAnchor);
    cur.removeSelectedText();

    // Formato base SEM anchor (preserva fonte/cor atuais). clearProperty remove a
    // propriedade de vez — só setAnchor(false) deixava o anchor "grudar" no texto
    // digitado em seguida (e vazava pros parágrafos seguintes).
    QTextCharFormat base = cur.charFormat();
    base.setAnchor(false);
    base.clearProperty(QTextFormat::IsAnchor);
    base.clearProperty(QTextFormat::AnchorHref);
    base.clearProperty(QTextFormat::AnchorName);

    // Link = base + anchor, SEM cor/sublinhado: em repouso a menção é igual ao
    // texto; o realce só aparece ao segurar Ctrl (no editor).
    QTextCharFormat linkFmt = base;
    linkFmt.setAnchor(true);
    linkFmt.setAnchorHref(QStringLiteral("ref:%1:%2").arg(drawerKey, itemId));

    cur.insertText(title, linkFmt);
    cur.insertText(QStringLiteral(" "), base);
    cur.endEditBlock();

    ed->setTextCursor(cur);
    ed->setCurrentCharFormat(base);   // novo texto digitado não herda o link
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
        case Qt::Key_Escape: hidePopup(); return true;
        default: break;
        }
    }
    if (event->type() == QEvent::FocusOut) hidePopup();
    return QObject::eventFilter(watched, event);
}
