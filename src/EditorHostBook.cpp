// Manuscrito completo (16/11/2026): o livro inteiro no editor.
//
// O livro é montado juntando o HTML de cada capítulo, em ordem, com um
// parágrafo-sentinela antes de cada um. Depois da carga, cada sentinela vira
// uma abertura (bloco vazio com BookSeams::kSeamProperty = id do capítulo).
// Salvar é o caminho de volta: o trecho entre uma abertura e a próxima é
// copiado pra um documento à parte e exportado como o HTML do capítulo, e só
// os capítulos que receberam edição (m_bookDirty) são regravados.
//
// A abertura nunca pode sumir nem receber texto: o filtro de teclas e de
// ditado (bookGuardKey/bookGuardInputMethod) barra o que a atravessaria, e a
// rede de segurança (onBookContentsChange → repairBook) desfaz qualquer outra
// edição que a tenha quebrado.

#include "EditorHost.h"

#include "BookSeams.h"
#include "CrashLogger.h"
#include "DocCache.h"
#include "ProjectModel.h"

#include <QAbstractTextDocumentLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QPointer>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QTimer>
#include <algorithm>
#include <memory>

namespace {
// Texto do parágrafo-sentinela (só existe entre a montagem e a carga).
const QString& seamSentinel()
{
    static const QString s = QStringLiteral("⁣QENNA-BOOK-SEAM⁣");
    return s;
}

QString bodyInner(const QString& html)
{
    static const QRegularExpression bodyRe(QStringLiteral("<body[^>]*>(.*)</body>"),
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = bodyRe.match(html);
    if (m.hasMatch()) return m.captured(1);
    QString t = html;
    static const QRegularExpression headRe(QStringLiteral("<head.*</head>"),
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression wrapRe(QStringLiteral("</?(html|body)[^>]*>|<!DOCTYPE[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    t.remove(headRe);
    t.remove(wrapRe);
    return t;
}

QString headOf(const QString& html)
{
    static const QRegularExpression headRe(QStringLiteral("<head.*</head>"),
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = headRe.match(html);
    return m.hasMatch() ? m.captured(0) : QString();
}

bool isRuler(const QTextBlock& b)
{
    return b.blockFormat().hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth);
}
} // namespace

// ============================================================ montagem

QString EditorHost::buildBookHtml()
{
    QString head;
    QString body;
    if (!m_model) return QString();
    for (const QString& id : std::as_const(m_bookChapterIds)) {
        const Chapter* ch = m_model->findChapter(id);
        if (!ch) continue;
        const QString key = DocCache::chapterKey(ch->manuscriptId, id);
        QString html;
        if (m_cache && m_cache->has(key)) {
            html = m_cache->get(key);
        } else {
            ViewMode cv;
            cv.type = ChapterDoc;
            cv.manuscriptId = ch->manuscriptId;
            cv.chapterId = id;
            html = hydrateFromModel(cv);
            if (m_cache && !html.isEmpty()) m_cache->set(key, html, /*markDirty=*/false);
        }
        if (head.isEmpty()) head = headOf(html);
        QString inner = bodyInner(html);
        if (inner.trimmed().isEmpty()) inner = QStringLiteral("<p><br /></p>");
        body += QStringLiteral("<p>") + seamSentinel() + QStringLiteral("</p>\n") + inner + QLatin1Char('\n');
    }
    if (head.isEmpty()) {
        head = QStringLiteral("<head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">"
                              "p, li { white-space: pre-wrap; }</style></head>");
    }
    return QStringLiteral("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">"
                          "<html>") + head + QStringLiteral("<body>") + body + QStringLiteral("</body></html>");
}

void EditorHost::stampBookSeams()
{
    if (!m_editor) return;
    QTextDocument* doc = m_editor->document();
    QList<QTextBlock> found;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next())
        if (b.text() == seamSentinel()) found << b;
    if (found.size() != m_bookChapterIds.size()) {
        CrashLogger::log(QStringLiteral("book: %1 aberturas para %2 capítulos").arg(found.size()).arg(m_bookChapterIds.size()));
    }
    for (int i = 0; i < found.size() && i < m_bookChapterIds.size(); ++i) {
        QTextCursor c(found.at(i));
        c.movePosition(QTextCursor::StartOfBlock);
        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        c.removeSelectedText();
        QTextBlockFormat f = c.blockFormat();
        f.setProperty(BookSeams::kSeamProperty, m_bookChapterIds.at(i));
        f.setTextIndent(0);
        f.setAlignment(Qt::AlignHCenter);
        c.setBlockFormat(f);
    }
    BookSeams::applySpacing(doc, false);
    rescanSeams();
    if (!m_seams.isEmpty()) {
        const QString id = chapterAt(m_editor->textCursor().position());
        if (!id.isEmpty()) m_focusChapterId = id;
    }
}

bool EditorHost::rescanSeams()
{
    m_seams.clear();
    if (!isBook() || !m_editor) return true;
    QTextDocument* doc = m_editor->document();
    m_seamsBlockCount = doc->blockCount();
    bool ok = true;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        if (!BookSeams::isSeam(b)) continue;
        m_seams.append(Seam{ b, BookSeams::seamChapter(b) });
        if (b.length() > 1) ok = false;          // abertura com texto dentro
        if (!b.next().isValid()) ok = false;     // capítulo sem bloco nenhum
    }
    if (m_seams.size() != m_bookChapterIds.size()) {
        ok = false;
    } else {
        for (int i = 0; i < m_seams.size(); ++i)
            if (m_seams.at(i).chapterId != m_bookChapterIds.at(i)) { ok = false; break; }
    }
    if (!m_seams.isEmpty() && m_seams.first().block.blockNumber() != 0) ok = false;  // texto antes do 1º capítulo
    return ok;
}

// ============================================================ posições

int EditorHost::seamIndexAt(int position) const
{
    if (m_seams.isEmpty()) return -1;
    int lo = 0, hi = int(m_seams.size()) - 1, best = 0;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (m_seams.at(mid).block.position() <= position) { best = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    return best;
}

int EditorHost::seamIndexOf(const QString& chapterId) const
{
    for (int i = 0; i < m_seams.size(); ++i)
        if (m_seams.at(i).chapterId == chapterId) return i;
    return -1;
}

int EditorHost::partStart(int i) const
{
    if (i < 0 || i >= m_seams.size()) return 0;
    const QTextBlock next = m_seams.at(i).block.next();
    return next.isValid() ? next.position() : m_seams.at(i).block.position();
}

int EditorHost::partEnd(int i) const
{
    if (i < 0 || i >= m_seams.size() || !m_editor) return 0;
    if (i + 1 < m_seams.size()) return m_seams.at(i + 1).block.position() - 1;
    return m_editor->document()->characterCount() - 1;
}

bool EditorHost::isSeamPosition(int position) const
{
    const int i = seamIndexAt(position);
    return i >= 0 && m_seams.at(i).block.position() == position;
}

// Apagar [from, to) mexeria na abertura? (os separadores dos dois lados dela)
bool EditorHost::rangeTouchesSeam(int from, int to) const
{
    const int i = seamIndexAt(to);
    return i >= 0 && m_seams.at(i).block.position() >= from && m_seams.at(i).block.position() <= to;
}

EditorHost::ViewMode EditorHost::scopeViewMode() const
{
    if (!isBook()) return m_viewMode;
    ViewMode v;
    v.type = ChapterDoc;
    v.manuscriptId = m_viewMode.manuscriptId;
    v.chapterId = !m_focusChapterId.isEmpty() ? m_focusChapterId
                : (m_bookChapterIds.isEmpty() ? QString() : m_bookChapterIds.first());
    return v;
}

int EditorHost::scopeStart() const
{
    if (!isBook()) return 0;
    return partStart(seamIndexOf(m_focusChapterId));
}

int EditorHost::scopeEnd() const
{
    if (!m_editor) return 0;
    if (!isBook()) return qMax(0, m_editor->document()->characterCount() - 1);
    return partEnd(seamIndexOf(m_focusChapterId));
}

QString EditorHost::scopePlainText() const
{
    if (!m_editor) return QString();
    if (!isBook()) return m_editor->toPlainText();
    const int i = seamIndexOf(m_focusChapterId);
    if (i < 0) return QString();
    QTextCursor c(m_editor->document());
    c.setPosition(partStart(i));
    c.setPosition(partEnd(i), QTextCursor::KeepAnchor);
    QString t = c.selectedText();
    t.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    t.replace(QChar::LineSeparator, QLatin1Char('\n'));
    return t;
}

bool EditorHost::inScope(const QTextBlock& b) const
{
    if (!isBook()) return true;
    const int i = seamIndexOf(m_focusChapterId);
    if (i < 0 || !b.isValid()) return false;
    const int p = b.position();
    return p >= partStart(i) && p <= partEnd(i);
}

QString EditorHost::chapterAt(int position) const
{
    if (!isBook()) return (m_viewMode.type == ChapterDoc || m_viewMode.type == SceneDoc) ? m_viewMode.chapterId : QString();
    const int i = seamIndexAt(position);
    return i >= 0 ? m_seams.at(i).chapterId : QString();
}

QString EditorHost::keyAt(int position) const
{
    if (!isBook()) return activeKey();
    const QString id = chapterAt(position);
    return id.isEmpty() ? QString() : DocCache::chapterKey(m_viewMode.manuscriptId, id);
}

QStringList EditorHost::bookKeys() const
{
    QStringList out;
    if (!isBook()) return out;
    for (const QString& id : m_bookChapterIds) out << DocCache::chapterKey(m_viewMode.manuscriptId, id);
    return out;
}

bool EditorHost::showsKey(const QString& key) const
{
    if (key.isEmpty()) return false;
    if (isBook()) return bookKeys().contains(key);
    return key == activeKey();
}

QVector<EditorHost::BookPart> EditorHost::bookParts() const
{
    QVector<BookPart> out;
    if (!isBook()) return out;
    for (int i = 0; i < m_seams.size(); ++i) {
        BookPart bp;
        bp.chapterId = m_seams.at(i).chapterId;
        bp.key = DocCache::chapterKey(m_viewMode.manuscriptId, bp.chapterId);
        const QTextBlock first = m_seams.at(i).block.next();
        bp.firstBlock = first.isValid() ? first.blockNumber() : m_seams.at(i).block.blockNumber();
        bp.firstPos = partStart(i);
        bp.endPos = partEnd(i);
        out << bp;
    }
    return out;
}

int EditorHost::bookBreakLocal(int breakIndex, QString* chapterId) const
{
    if (!isBook() || !m_editor) return -1;
    QTextDocument* doc = m_editor->document();
    if (m_breakTableRev != doc->revision() || m_breakTableBlocks != doc->blockCount()) {
        m_breakTable.clear();
        QString cur;
        int local = 0;
        for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
            if (BookSeams::isSeam(b)) { cur = BookSeams::seamChapter(b); local = 0; continue; }
            if (isRuler(b)) m_breakTable.append({ cur, local++ });
        }
        m_breakTableRev = doc->revision();
        m_breakTableBlocks = doc->blockCount();
    }
    if (breakIndex < 0 || breakIndex >= m_breakTable.size()) return -1;
    if (chapterId) *chapterId = m_breakTable.at(breakIndex).first;
    return m_breakTable.at(breakIndex).second;
}

// ============================================================ escopo

void EditorHost::updateFocusChapter()
{
    if (!isBook() || m_seams.isEmpty() || m_loadingContent || !m_editor) return;
    const QString id = chapterAt(m_editor->textCursor().position());
    if (id.isEmpty() || id == m_focusChapterId) return;
    const QString prev = m_focusChapterId;
    m_focusChapterId = id;
    emit scopeChanged(prev);
    m_editor->viewport()->update();   // a abertura acesa troca de capítulo
}

// ============================================================ edição e costura

void EditorHost::markBookDirtyRange(int from, int to)
{
    if (m_seams.isEmpty()) return;
    const int i0 = seamIndexAt(from), i1 = seamIndexAt(qMax(from, to));
    for (int i = qMax(0, i0); i <= i1 && i < m_seams.size(); ++i) m_bookDirty.insert(m_seams.at(i).chapterId);
}

void EditorHost::onBookContentsChange(int pos, int removed, int added)
{
    if (!isBook() || m_loadingContent || m_bookRepairing || !m_editor) return;
    QTextDocument* doc = m_editor->document();
    bool ok = true;
    if (removed > 0 || m_seams.isEmpty() || doc->blockCount() != m_seamsBlockCount) {
        ok = rescanSeams();
    } else {
        // só entrou texto: a única abertura que pode ter quebrado é a do lugar
        for (QTextBlock b = doc->findBlock(pos); b.isValid() && b.position() <= pos + added; b = b.next())
            if (BookSeams::isSeam(b) && b.length() > 1) { ok = false; break; }
    }
    if (!ok) {
        if (!m_bookCheckPending) {
            m_bookCheckPending = true;
            QTimer::singleShot(0, this, &EditorHost::checkBookStructure);
        }
        return;
    }
    markBookDirtyRange(pos, pos + added);
}

void EditorHost::checkBookStructure()
{
    m_bookCheckPending = false;
    if (!isBook()) return;
    if (rescanSeams()) return;
    repairBook();
}

void EditorHost::repairBook()
{
    if (!isBook() || !m_editor) return;
    QTextDocument* doc = m_editor->document();
    if (doc->isUndoAvailable()) {
        m_bookRepairing = true;
        doc->undo();
        m_bookRepairing = false;
        if (rescanSeams()) {
            // o passo desfeito pode ter mexido em qualquer capítulo
            for (const QString& id : std::as_const(m_bookChapterIds)) m_bookDirty.insert(id);
            CrashLogger::log(QStringLiteral("book: edição que quebrava a costura desfeita"));
            emit bookEditBlocked(tr("A abertura de capítulo não pode ser apagada. A última edição foi desfeita."));
            return;
        }
    }
    // sem como desfazer: o livro volta ao que já estava gravado
    CrashLogger::log(QStringLiteral("book: costura quebrada sem desfazer; remontando o livro"));
    m_bookDirty.clear();
    rebuildBook(true);
    emit bookEditBlocked(tr("A abertura de capítulo foi refeita. A última edição não pôde ser mantida."));
}

void EditorHost::deleteAcrossSeams(int from, int to)
{
    if (!m_editor || m_seams.isEmpty() || to <= from) return;
    QTextDocument* doc = m_editor->document();
    QVector<QPair<int, int>> pieces;
    const int first = seamIndexAt(from), last = seamIndexAt(to);
    for (int i = qMax(0, first); i <= last && i < m_seams.size(); ++i) {
        const int s0 = qMax(from, partStart(i)), s1 = qMin(to, partEnd(i));
        if (s1 > s0) pieces.append({ s0, s1 });
    }
    QTextCursor c(doc);
    c.beginEditBlock();
    for (int k = int(pieces.size()) - 1; k >= 0; --k) {
        c.setPosition(pieces.at(k).first);
        c.setPosition(pieces.at(k).second, QTextCursor::KeepAnchor);
        c.removeSelectedText();
    }
    c.endEditBlock();
    int at = pieces.isEmpty() ? from : pieces.first().first;
    if (isSeamPosition(at)) at = partStart(seamIndexAt(at));
    QTextCursor nc(doc);
    nc.setPosition(qBound(0, at, doc->characterCount() - 1));
    m_editor->setTextCursor(nc);
}

bool EditorHost::bookGuardKey(QKeyEvent* key)
{
    if (!m_editor || m_seams.isEmpty()) return false;
    const bool back = key->key() == Qt::Key_Backspace;
    const bool del = key->key() == Qt::Key_Delete;
    const bool cut = key->matches(QKeySequence::Cut);
    const bool paste = key->matches(QKeySequence::Paste);
    const bool wordBack = key->matches(QKeySequence::DeleteStartOfWord);
    const bool wordDel = key->matches(QKeySequence::DeleteEndOfWord) || key->matches(QKeySequence::DeleteCompleteLine);
    const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
    const QString t = key->text();
    const bool typing = !t.isEmpty() && (t.at(0).isPrint() || t.at(0) == QLatin1Char('\t'));
    if (!(back || del || cut || paste || wordBack || wordDel || enter || typing)) return false;

    QTextCursor c = m_editor->textCursor();
    if (c.hasSelection()) {
        const int a = c.selectionStart(), b = c.selectionEnd();
        if (!rangeTouchesSeam(a, b)) return false;   // dentro de um capítulo: normal
        // a seleção atravessa capítulos: sai o texto de cada um, as aberturas ficam
        if (cut) m_editor->copy();
        deleteAcrossSeams(a, b);
        return back || del || cut || wordBack || wordDel;   // digitar/colar segue no lugar
    }
    const int pos = c.position();
    if (isSeamPosition(pos)) {
        // na abertura: escrever vai pro começo do capítulo; apagar não faz nada
        if (typing || enter || paste) {
            QTextCursor nc(m_editor->document());
            nc.setPosition(partStart(seamIndexAt(pos)));
            m_editor->setTextCursor(nc);
            return false;
        }
        return true;
    }
    const QTextBlock blk = c.block();
    if ((back || wordBack) && pos == blk.position() && BookSeams::isSeam(blk.previous())) return true;
    if ((del || wordDel) && pos == blk.position() + blk.length() - 1 && BookSeams::isSeam(blk.next())) return true;
    return false;
}

bool EditorHost::bookGuardInputMethod(QInputMethodEvent* ev)
{
    if (!m_editor || m_seams.isEmpty()) return false;
    if (ev->commitString().isEmpty() && ev->preeditString().isEmpty() && ev->replacementLength() == 0) return false;
    QTextCursor c = m_editor->textCursor();
    if (c.hasSelection()) {
        if (rangeTouchesSeam(c.selectionStart(), c.selectionEnd())) deleteAcrossSeams(c.selectionStart(), c.selectionEnd());
        return false;
    }
    const int pos = c.position();
    if (ev->replacementLength() > 0) {
        const int from = pos + ev->replacementStart();
        if (rangeTouchesSeam(from, from + ev->replacementLength())) return true;
    }
    if (isSeamPosition(pos)) {
        QTextCursor nc(m_editor->document());
        nc.setPosition(partStart(seamIndexAt(pos)));
        m_editor->setTextCursor(nc);
    }
    return false;
}

// ============================================================ gravar

QString EditorHost::extractChapterHtml(int i, QTextDocument* out) const
{
    QTextDocument* doc = m_editor->document();
    const int s0 = partStart(i), s1 = partEnd(i);
    out->setDefaultFont(doc->defaultFont());
    out->setDefaultTextOption(doc->defaultTextOption());
    out->setDefaultStyleSheet(doc->defaultStyleSheet());
    out->setIndentWidth(doc->indentWidth());
    if (s1 > s0) {
        QTextCursor c(doc);
        c.setPosition(s0);
        c.setPosition(s1, QTextCursor::KeepAnchor);
        QTextCursor oc(out);
        oc.insertFragment(c.selection());
    }
    // o primeiro bloco colado fica com o formato do bloco vazio de destino
    const QTextBlock src = doc->findBlock(s0);
    QTextCursor fc(out->firstBlock());
    QTextBlockFormat bf = src.blockFormat();
    bf.clearProperty(BookSeams::kSeamProperty);
    fc.setBlockFormat(bf);
    fc.setBlockCharFormat(src.charFormat());
    return out->toHtml();
}

void EditorHost::flushBook()
{
    if (!isBook() || !m_editor || !m_cache || m_bookRepairing || m_loadingContent) return;
    if (!rescanSeams()) {
        repairBook();
        if (!isBook() || !rescanSeams()) return;
    }
    if (m_bookDirty.isEmpty()) return;
    const QSet<QString> dirty = m_bookDirty;
    m_bookDirty.clear();
    // primeiro separa tudo; só depois avisa (quem escuta pode remontar o livro)
    struct Out { QString key; QString html; std::shared_ptr<QTextDocument> doc; };
    QVector<Out> outs;
    for (int i = 0; i < m_seams.size(); ++i) {
        const QString id = m_seams.at(i).chapterId;
        if (!dirty.contains(id)) continue;
        const Chapter* ch = m_model ? m_model->findChapter(id) : nullptr;
        if (!ch) continue;
        auto tmp = std::make_shared<QTextDocument>();
        const QString html = extractChapterHtml(i, tmp.get());
        outs.append(Out{ DocCache::chapterKey(ch->manuscriptId, id), html, tmp });
    }
    for (const Out& o : std::as_const(outs)) m_cache->set(o.key, o.html, /*markDirty=*/true);
    for (const Out& o : std::as_const(outs)) {
        emit chapterDocFlushed(o.key, o.doc.get());
        emit contentFlushed(o.key);
    }
}

// ============================================================ abrir, sair, remontar

void EditorHost::rebuildBook(bool keepCursor)
{
    if (!isBook() || !m_editor || !m_model) return;
    QString ch;
    int rel = 0, seamOffset = 0;
    QScrollBar* sb = m_editor->verticalScrollBar();
    if (keepCursor && !m_seams.isEmpty()) {
        const int pos = m_editor->textCursor().position();
        const int i = seamIndexAt(pos);
        ch = m_seams.at(i).chapterId;
        rel = pos - partStart(i);
        const QRectF r = m_editor->document()->documentLayout()->blockBoundingRect(m_seams.at(i).block);
        seamOffset = sb->value() - int(r.top());
    }
    m_bookChapterIds.clear();
    for (const Chapter* c : m_model->orderedChaptersForManuscript(m_viewMode.manuscriptId)) m_bookChapterIds << c->id;
    if (m_bookChapterIds.isEmpty()) { disable(); return; }
    m_seams.clear();
    m_bookDirty.clear();
    m_breakTableRev = -1;
    if (!m_bookChapterIds.contains(m_focusChapterId)) m_focusChapterId = m_bookChapterIds.first();
    replaceContent(buildBookHtml());
    if (!keepCursor) return;
    if (!m_bookChapterIds.contains(ch)) ch = m_bookChapterIds.first();
    placeCursorInChapter(ch, rel, -1);
    const int i = seamIndexOf(ch);
    if (i >= 0) {
        const QRectF r = m_editor->document()->documentLayout()->blockBoundingRect(m_seams.at(i).block);
        sb->setValue(int(r.top()) + seamOffset);
    }
}

void EditorHost::placeCursorInChapter(const QString& chapterId, int relPos, int sceneIndex)
{
    if (!m_editor) return;
    const int i = seamIndexOf(chapterId);
    if (i < 0) return;
    const int s0 = partStart(i), s1 = partEnd(i);
    int base = s0;
    if (sceneIndex > 0) {
        int k = 0;
        for (QTextBlock b = m_seams.at(i).block.next(); b.isValid() && b.position() <= s1; b = b.next()) {
            if (!isRuler(b)) continue;
            if (++k == sceneIndex) { base = b.next().isValid() ? b.next().position() : b.position(); break; }
        }
    }
    QTextCursor c(m_editor->document());
    c.setPosition(qBound(s0, base + qMax(0, relPos), s1));
    m_editor->setTextCursor(c);
}

void EditorHost::scrollCursorToThird(bool retry)
{
    if (!m_editor) return;
    QPointer<QTextEdit> ed(m_editor);
    auto apply = [ed]() {
        if (!ed) return;
        QScrollBar* sb = ed->verticalScrollBar();
        const QRect r = ed->cursorRect();
        sb->setValue(sb->value() + r.top() - ed->viewport()->height() / 3);
    };
    apply();
    if (!retry) return;
    // o layout de um livro grande termina aos poucos: refaz quando assentar
    const int at = m_editor->textCursor().position();
    for (int ms : { 0, 120, 400 }) {
        QTimer::singleShot(ms, this, [this, ed, at, apply]() {
            if (!ed || !isBook() || ed->textCursor().position() != at) return;
            apply();
        });
    }
}

bool EditorHost::scrollToChapter(const QString& chapterId, int sceneIndex)
{
    if (!isBook() || !m_editor || seamIndexOf(chapterId) < 0) return false;
    m_spotPending = false;
    placeCursorInChapter(chapterId, 0, sceneIndex);
    const int i = seamIndexOf(chapterId);
    QPointer<QTextEdit> ed(m_editor);
    const QTextBlock target = sceneIndex > 0 ? m_editor->textCursor().block() : m_seams.at(i).block;
    // a abertura tem o rótulo no respiro acima da linha: rola até mostrar ele também
    const int lift = sceneIndex > 0 ? 8 : int(target.blockFormat().topMargin() * 0.7);
    const int at = m_editor->textCursor().position();
    auto apply = [this, ed, target, at, lift]() {
        if (!ed || !isBook() || ed->textCursor().position() != at) return;
        const QRectF r = ed->document()->documentLayout()->blockBoundingRect(target);
        ed->verticalScrollBar()->setValue(int(r.top()) - lift);
    };
    apply();
    for (int ms : { 0, 120, 400 }) QTimer::singleShot(ms, this, apply);
    m_editor->setFocus();
    return true;
}

void EditorHost::replaceOpenTab(const QString& oldKey)
{
    // virar a chave troca a aba no lugar: o capítulo vira o livro e vice-versa
    const QString nowKey = activeKey();
    if (oldKey.isEmpty() || oldKey == nowKey) return;
    int oldIdx = -1, nowIdx = -1;
    for (int i = 0; i < m_open.size(); ++i) {
        const QString k = keyFor(m_open.at(i));
        if (k == oldKey) oldIdx = i;
        if (k == nowKey) nowIdx = i;
    }
    if (oldIdx < 0 || nowIdx < 0) return;
    const ViewMode now = m_open.at(nowIdx);
    m_open[oldIdx] = now;
    m_open.removeAt(nowIdx);
}

void EditorHost::openBook(const QString& manuscriptId)
{
    if (!m_model || !m_model->findManuscript(manuscriptId) || !m_editor) return;
    if (isBook() && m_viewMode.manuscriptId == manuscriptId) return;
    const ViewMode old = m_viewMode;
    const QString oldKey = activeKey();
    QString fromCh;
    int rel = 0, scene = -1;
    if ((old.type == ChapterDoc || old.type == SceneDoc) && old.manuscriptId == manuscriptId) {
        fromCh = old.chapterId;
        rel = m_editor->textCursor().position();
        if (old.type == SceneDoc) scene = old.sceneIndex;
    }
    if (m_model->activeManuscriptId() != manuscriptId) m_model->setActiveManuscriptId(manuscriptId);
    ViewMode vm;
    vm.type = BookDoc;
    vm.manuscriptId = manuscriptId;
    setViewMode(vm);
    if (!isBook()) return;
    if (!rescanSeams()) {
        // o livro não montou direito: nada foi editado ainda, volta pro que estava
        CrashLogger::log(QStringLiteral("book: montagem falhou; voltando ao documento anterior"));
        m_bookDirty.clear();
        m_seams.clear();
        setViewMode(old);
        emit bookEditBlocked(tr("Não deu pra abrir o livro inteiro. O capítulo continua aberto como antes."));
        return;
    }
    if (!fromCh.isEmpty()) {
        m_spotPending = false;
        placeCursorInChapter(fromCh, rel, scene);
        scrollCursorToThird(true);
    }
    replaceOpenTab(oldKey);
    emit historyChanged();
}

void EditorHost::leaveBook()
{
    if (!isBook() || !m_editor) return;
    const QString bookKey = activeKey();
    QString id = m_focusChapterId;
    int rel = 0;
    if (!m_seams.isEmpty()) {
        const int pos = m_editor->textCursor().position();
        const int i = seamIndexAt(pos);
        id = m_seams.at(i).chapterId;
        rel = qMax(0, pos - partStart(i));
    }
    if (id.isEmpty() && !m_bookChapterIds.isEmpty()) id = m_bookChapterIds.first();
    if (id.isEmpty()) { disable(); return; }
    ViewMode vm;
    vm.type = ChapterDoc;
    vm.manuscriptId = m_viewMode.manuscriptId;
    vm.chapterId = id;
    setViewMode(vm);
    if (m_viewMode != vm) return;
    m_spotPending = false;
    QTextCursor c(m_editor->document());
    c.setPosition(qBound(0, rel, m_editor->document()->characterCount() - 1));
    m_editor->setTextCursor(c);
    scrollCursorToThird(false);
    replaceOpenTab(bookKey);
    emit historyChanged();
}

void EditorHost::onModelChaptersChanged()
{
    if (!isBook() || !m_model) return;
    if (!m_model->findManuscript(m_viewMode.manuscriptId)) { disable(); return; }
    QStringList ids;
    for (const Chapter* c : m_model->orderedChaptersForManuscript(m_viewMode.manuscriptId)) ids << c->id;
    if (ids.isEmpty()) { disable(); return; }
    if (ids == m_bookChapterIds) {
        if (m_editor) m_editor->viewport()->update();   // título novo na abertura
        return;
    }
    // capítulo novo, apagado ou fora de ordem: grava o pendente e remonta
    QPointer<EditorHost> self(this);
    QTimer::singleShot(0, this, [self]() {
        if (!self || !self->isBook()) return;
        self->syncEditorToCache();
        self->rebuildBook(true);
    });
}
