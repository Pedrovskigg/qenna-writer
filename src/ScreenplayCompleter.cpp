#include "ScreenplayCompleter.h"

#include "ScreenplayFormat.h"
#include "SpellEditor.h"
#include "Theme.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QListWidget>
#include <QScrollBar>
#include <QSet>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace {
constexpr int kMaxItems = 8;
constexpr int kRowH = 28;
constexpr int kWidth = 320;
}

ScreenplayCompleter::ScreenplayCompleter(QWidget* ownerWindow, QObject* parent)
    : QObject(parent)
    , m_owner(ownerWindow)
{
    m_list = new QListWidget(m_owner);
    m_list->setObjectName(QStringLiteral("screenplayCompleter"));
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setUniformItemSizes(true);
    m_list->setMouseTracking(true);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->hide();
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem*) { accept(); });
}

void ScreenplayCompleter::attach(SpellEditor* editor)
{
    if (!editor) return;
    m_editor = editor;
    editor->installEventFilter(this);
    connect(editor, &QTextEdit::textChanged, this, [this]() {
        if (m_editor && m_editor->hasFocus() && !m_inserting) update();
    });
    connect(editor, &QTextEdit::cursorPositionChanged, this, [this]() {
        if (m_list->isVisible() && !m_inserting) update();
    });
}

void ScreenplayCompleter::update()
{
    if (!m_editor || !m_editor->isScreenplayMode()) { hide(); return; }
    const QTextCursor cur = m_editor->textCursor();
    const QTextBlock block = cur.block();
    const QString text = block.text();
    // Só no fim da linha: no meio de um nome já escrito, a lista atrapalharia.
    if (cur.hasSelection() || cur.positionInBlock() != text.size() || text.trimmed().isEmpty()) { hide(); return; }

    const ScreenplayElement el = ScreenplayFormat::detect(block.blockFormat(), text);
    QList<Candidate> items;

    if (el == ScreenplayElement::Character) {
        const QString q = text.trimmed().toUpper();
        if (q.contains(QLatin1Char('('))) { hide(); return; }
        // Elenco primeiro (com o nome completo do lado), depois quem já falou
        // neste documento e não está no elenco (figurante).
        QSet<QString> seen;
        QStringList cast = m_cast.keys();
        std::sort(cast.begin(), cast.end());
        for (const QString& cue : cast) {
            if (!cue.startsWith(q) || cue == q) continue;
            seen.insert(cue);
            items.append({ cue, cue, m_cast.value(cue) });
        }
        QStringList used;
        for (QTextBlock b = m_editor->document()->begin(); b.isValid(); b = b.next()) {
            if (b == block) continue;
            if (ScreenplayFormat::detect(b.blockFormat(), b.text()) != ScreenplayElement::Character) continue;
            const QString cue = ScreenplayFormat::cueName(b.text()).toUpper();
            if (cue.isEmpty() || seen.contains(cue) || !cue.startsWith(q) || cue == q) continue;
            seen.insert(cue);
            used.append(cue);
        }
        for (const QString& cue : used) items.append({ cue, cue, QString() });
        m_replaceFrom = block.position() + text.indexOf(text.trimmed());
    } else if (el == ScreenplayElement::Scene) {
        const int pre = ScreenplayFormat::scenePrefixLength(text);
        if (pre < 0) {
            // Ainda sem prefixo: "I" → INT. / INT./EXT. ; "E" → EXT.
            const QString q = text.trimmed().toUpper();
            if (q.size() > 8) { hide(); return; }
            for (const QString& p : { QStringLiteral("INT. "), QStringLiteral("EXT. "), QStringLiteral("INT./EXT. ") })
                if (p.startsWith(q) && p.trimmed() != q) items.append({ p, p.trimmed(), QString() });
            m_replaceFrom = block.position() + text.indexOf(text.trimmed());
        } else {
            const QString rest = text.mid(pre);
            int sep = -1, sepLen = 0;
            for (const QString& s : { QStringLiteral(" - "), QStringLiteral(" – "), QStringLiteral(" — ") }) {
                const int i = rest.lastIndexOf(s);
                if (i > sep) { sep = i; sepLen = s.size(); }
            }
            if (sep < 0) {
                // Local: os do roteiro inteiro + os desta página, ao vivo.
                QMap<QString, int> locs = m_locations;
                for (QTextBlock b = m_editor->document()->begin(); b.isValid(); b = b.next()) {
                    if (b == block) continue;
                    if (ScreenplayFormat::detect(b.blockFormat(), b.text()) != ScreenplayElement::Scene) continue;
                    const QString loc = ScreenplayFormat::sceneLocation(b.text());
                    if (!loc.isEmpty() && !m_locations.contains(loc)) locs[loc] += 1;
                }
                const QString q = rest.trimmed().toUpper();
                for (auto it = locs.cbegin(); it != locs.cend(); ++it) {
                    if (!it.key().startsWith(q) || it.key() == q) continue;
                    items.append({ it.key() + QStringLiteral(" - "), it.key(),
                                   it.value() == 1
                                       ? QCoreApplication::translate("ScreenplayCompleter", "1 cena")
                                       : QCoreApplication::translate("ScreenplayCompleter", "%1 cenas").arg(it.value()) });
                }
                m_replaceFrom = block.position() + pre;
            } else {
                // Hora do dia, depois do " - ".
                const QString q = rest.mid(sep + sepLen).trimmed().toUpper();
                const QStringList times = QCoreApplication::translate("ScreenplayCompleter",
                    "DIA|NOITE|MADRUGADA|AMANHECER|ENTARDECER|CONTÍNUO|MAIS TARDE|MOMENTOS DEPOIS")
                    .split(QLatin1Char('|'), Qt::SkipEmptyParts);
                for (const QString& t : times)
                    if (t.startsWith(q) && t != q) items.append({ t, t, QString() });
                m_replaceFrom = block.position() + pre + sep + sepLen;
            }
        }
    } else {
        hide();
        return;
    }

    if (items.isEmpty()) { hide(); return; }
    while (items.size() > kMaxItems) items.removeLast();
    showList(items);
}

void ScreenplayCompleter::showList(const QList<Candidate>& items)
{
    m_list->setStyleSheet(Theme::qss(QStringLiteral(
        "QListWidget#screenplayCompleter {"
        "  background: %1; color: %2;"
        "  border: 1px solid %3; border-radius: @radius-panel; padding: 4px; outline: none;"
        "}"
        "QListWidget#screenplayCompleter::item { padding: 4px 8px; border-radius: @radius-item; }"
        "QListWidget#screenplayCompleter::item:selected { background: %4; color: %5; }"
    ).arg(Theme::panelBackground(), Theme::textPrimary(), Theme::subtleBorder(),
          Theme::accentDefault(), Theme::textBright())));

    m_list->clear();
    QFont mono(QStringLiteral("Courier New"));
    mono.setBold(true);
    for (const Candidate& c : items) {
        auto* it = new QListWidgetItem(c.hint.isEmpty() ? c.display
                                                        : QStringLiteral("%1   ·  %2").arg(c.display, c.hint));
        it->setData(Qt::UserRole, c.insert);
        it->setFont(mono);
        m_list->addItem(it);
    }
    m_list->setCurrentRow(0);

    const QRect r = m_editor->cursorRect(m_editor->textCursor());
    const QPoint below = m_owner->mapFromGlobal(m_editor->viewport()->mapToGlobal(r.bottomLeft()));
    const QPoint above = m_owner->mapFromGlobal(m_editor->viewport()->mapToGlobal(r.topLeft()));
    const int h = int(items.size()) * kRowH + 10;
    int x = qBound(8, below.x(), qMax(8, m_owner->width() - kWidth - 8));
    int y = below.y() + 2;
    if (y + h > m_owner->height() - 4) y = above.y() - h - 2;
    m_list->setGeometry(x, qMax(4, y), kWidth, h);
    m_list->show();
    m_list->raise();
}

void ScreenplayCompleter::hide()
{
    if (m_list) m_list->hide();
    m_replaceFrom = -1;
}

void ScreenplayCompleter::moveSel(int delta)
{
    const int n = m_list->count();
    if (n == 0) return;
    m_list->setCurrentRow((m_list->currentRow() + delta + n) % n);
}

void ScreenplayCompleter::accept()
{
    if (!m_editor || m_replaceFrom < 0 || !m_list->currentItem()) { hide(); return; }
    const QString insert = m_list->currentItem()->data(Qt::UserRole).toString();
    QTextCursor c = m_editor->textCursor();
    const int end = c.position();
    c.setPosition(m_replaceFrom);
    c.setPosition(end, QTextCursor::KeepAnchor);
    m_inserting = true;
    c.insertText(insert);
    m_editor->setTextCursor(c);
    m_inserting = false;
    hide();
    // O local termina em " - ": já abre a lista das horas.
    if (insert.endsWith(QLatin1Char(' '))) update();
}

bool ScreenplayCompleter::eventFilter(QObject* watched, QEvent* event)
{
    if (m_list && m_list->isVisible() && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        switch (ke->key()) {
        case Qt::Key_Up:     moveSel(-1); return true;
        case Qt::Key_Down:   moveSel(1);  return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:    accept();    return true;
        case Qt::Key_Escape: hide();      return true;
        default: break;
        }
    }
    if (event->type() == QEvent::FocusOut) hide();
    return QObject::eventFilter(watched, event);
}
