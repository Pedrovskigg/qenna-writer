#include "SpellEditor.h"

#include "ScreenplayFormat.h"
#include "SpellChecker.h"

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QContextMenuEvent>
#include <QFile>
#include <QFocusEvent>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextImageFormat>
#include <QTextObjectInterface>

namespace {
bool isRefHref(const QString& href) {
    return href.startsWith(QStringLiteral("ref:"));
}

// Substitui o QTextImageHandler padrão do Qt para ativar SmoothPixmapTransform
// no painter antes de desenhar cada imagem. Sem isso, o QTextEdit escala imagens
// com interpolação de baixa qualidade (FastTransformation), causando serrilhado.
class SmoothImageHandler : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

    // Cache das dimensões naturais das imagens para evitar leitura repetida de disco.
    QHash<QUrl, QSize> m_sizeCache;

public:
    explicit SmoothImageHandler(QObject* parent = nullptr) : QObject(parent) {}

    // Retorna o tamanho de exibição da imagem (em px lógicos) a partir do formato.
    // Qt chama isso para o layout; deve concordar com o que drawObject vai pintar.
    QSizeF intrinsicSize(QTextDocument* /*doc*/, int /*pos*/, const QTextFormat& format) override {
        const QTextImageFormat fmt = format.toImageFormat();
        const bool hasW = fmt.hasProperty(QTextFormat::ImageWidth);
        const bool hasH = fmt.hasProperty(QTextFormat::ImageHeight);

        if (hasW && hasH)
            return QSizeF(fmt.width(), fmt.height());

        // Precisa das dimensões naturais para calcular o lado faltante.
        const QUrl url(fmt.name());
        if (!m_sizeCache.contains(url)) {
            QSize sz;
            const QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                QImageReader reader(path);
                sz = reader.size();   // lê só o header — rápido
            }
            m_sizeCache[url] = sz.isEmpty() ? QSize(100, 100) : sz;
        }

        const QSize nat = m_sizeCache.value(url, QSize(100, 100));
        if (hasW && nat.height() > 0)
            return QSizeF(fmt.width(), fmt.width() * nat.height() / nat.width());
        if (hasH && nat.width() > 0)
            return QSizeF(fmt.height() * nat.width() / nat.height(), fmt.height());
        return QSizeF(nat);
    }

    // Desenha a imagem com SmoothPixmapTransform — o ponto central do fix.
    void drawObject(QPainter* painter, const QRectF& rect, QTextDocument* doc,
                    int /*pos*/, const QTextFormat& format) override {
        const QUrl url(format.toImageFormat().name());
        const QVariant data = doc->resource(QTextDocument::ImageResource, url);

        QImage image;
        if (data.userType() == QMetaType::QPixmap)
            image = qvariant_cast<QPixmap>(data).toImage();
        else if (data.userType() == QMetaType::QImage)
            image = qvariant_cast<QImage>(data);
        else if (data.userType() == QMetaType::QByteArray)
            image.loadFromData(data.toByteArray());

        if (image.isNull()) return;

        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->drawImage(rect, image, image.rect());
        painter->restore();
    }
};
// moc precisa ver Q_OBJECT numa translation unit — inclui o moc gerado inline.
#include "SpellEditor.moc"
}

namespace {
constexpr int kMaxSuggestions = 6;
}

SpellEditor::SpellEditor(QWidget* parent)
    : QTextEdit(parent)
{
    // Registra nosso handler que substitui o padrão do Qt para imagens.
    // O padrão não seta SmoothPixmapTransform, causando escala com baixa qualidade.
    document()->documentLayout()->registerHandler(
        QTextFormat::ImageObject, new SmoothImageHandler(this));
}

void SpellEditor::setSpellChecker(SpellChecker* checker)
{
    m_checker = checker;
}

void SpellEditor::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu(event->pos());

    // Marcador de inserção — tudo que adicionamos (spell + glossário) vai antes
    // das ações padrão (Undo, Cut, Copy...).
    QAction* stdAnchor = menu->actions().isEmpty() ? nullptr : menu->actions().first();

    if (m_checker && m_checker->isEnabled()) {
        QTextCursor cursor = cursorForPosition(event->pos());
        cursor.select(QTextCursor::WordUnderCursor);
        const QString word = cursor.selectedText();

        if (!word.isEmpty() && !m_checker->isCorrect(word)) {
            const QStringList suggestions = m_checker->suggest(word);

            QAction* firstStandardAction = menu->actions().isEmpty()
                ? nullptr : menu->actions().first();

            QFont boldFont;
            boldFont.setBold(true);

            // Insere as sugestões no topo do menu, em ordem.
            if (suggestions.isEmpty()) {
                QAction* noSugg = new QAction(tr("(sem sugestões)"), menu);
                noSugg->setEnabled(false);
                menu->insertAction(firstStandardAction, noSugg);
            } else {
                const int n = qMin(kMaxSuggestions, suggestions.size());
                for (int i = 0; i < n; ++i) {
                    const QString suggestion = suggestions.at(i);
                    QAction* act = new QAction(suggestion, menu);
                    act->setFont(boldFont);
                    connect(act, &QAction::triggered, this, [this, cursor, suggestion]() mutable {
                        cursor.beginEditBlock();
                        cursor.insertText(suggestion);
                        cursor.endEditBlock();
                    });
                    menu->insertAction(firstStandardAction, act);
                }
            }

            // Ações específicas da palavra: adicionar / ignorar (uma vez nesta sessão).
            menu->insertSeparator(firstStandardAction);

            QAction* addAct = new QAction(tr("Adicionar \"%1\" ao dicionário").arg(word), menu);
            connect(addAct, &QAction::triggered, this, [this, word]() {
                if (m_checker) m_checker->addToPersonalDictionary(word);
            });
            menu->insertAction(firstStandardAction, addAct);

            menu->insertSeparator(firstStandardAction);
        }
    }

    // "Adicionar ao Glossário..." — SEMPRE disponível, mesmo sem seleção.
    // Usa a seleção atual ou, se vazia, a palavra sob o cursor. Inserido logo
    // antes das ações padrão (Undo, Cut, Copy...). Trim de paragraph-separator
    // ( ) e whitespace pra label não vir vazia com "lixo".
    {
        QString glossarySeed;
        QTextCursor selCursor = textCursor();
        if (selCursor.hasSelection()) {
            glossarySeed = selCursor.selectedText();
        } else {
            QTextCursor probe = cursorForPosition(event->pos());
            probe.select(QTextCursor::WordUnderCursor);
            glossarySeed = probe.selectedText();
        }
        glossarySeed = glossarySeed.trimmed();
        glossarySeed.remove(QChar(0x2029));

        const QString label = glossarySeed.isEmpty()
            ? tr("Adicionar ao Glossário...")
            : tr("Adicionar \"%1\" ao Glossário...").arg(glossarySeed.left(40));
        QAction* glsAct = new QAction(label, menu);
        const QPoint globalPos = event->globalPos();
        const QString seedCopy = glossarySeed;
        connect(glsAct, &QAction::triggered, this, [this, seedCopy, globalPos]() {
            emit addToGlossaryRequested(seedCopy, globalPos);
        });
        menu->insertAction(stdAnchor, glsAct);
        menu->insertSeparator(stdAnchor);
    }

    menu->exec(event->globalPos());
    delete menu;
}

void SpellEditor::mousePressEvent(QMouseEvent* event)
{
    // Ctrl+clique num link de referência abre o doc no RefMenu (não posiciona cursor).
    if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ControlModifier)) {
        const QString href = anchorAt(event->pos());
        if (isRefHref(href)) {
            emit refHighlightRequested(false);   // o clique vai tirar o foco; limpa o realce
            emit refActivated(href);
            event->accept();
            return;
        }
    }
    QTextEdit::mousePressEvent(event);
}

void SpellEditor::mouseMoveEvent(QMouseEvent* event)
{
    // Com Ctrl segurado, mostra a mãozinha sobre links de referência.
    if (event->modifiers() & Qt::ControlModifier) {
        const QString href = anchorAt(event->pos());
        viewport()->setCursor(isRefHref(href) ? Qt::PointingHandCursor : Qt::IBeamCursor);
    }
    QTextEdit::mouseMoveEvent(event);
}

namespace {
// Maiusculiza o texto do bloco ao sair dele (Cena/Personagem/Transição são
// sempre em caixa alta na convenção de roteiro) — só no momento de avançar
// pro próximo elemento via Enter, não a cada tecla.
void uppercaseBlockIfNeeded(const QTextCursor& cursor, ScreenplayElement element)
{
    if (element != ScreenplayElement::Scene && element != ScreenplayElement::Character
        && element != ScreenplayElement::Transition) {
        return;
    }
    const QTextBlock block = cursor.block();
    const QString text = block.text();
    const QString upper = text.toUpper();
    if (text.isEmpty() || text == upper) return;
    QTextCursor c(block);
    c.movePosition(QTextCursor::StartOfBlock);
    c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    c.insertText(upper);
}
}

void SpellEditor::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control && !event->isAutoRepeat())
        emit refHighlightRequested(true);   // "modo ver os links"

    if (m_screenplayMode && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
        if (event->key() == Qt::Key_Tab) {
            QTextCursor cur = textCursor();
            const ScreenplayElement current = ScreenplayFormat::detect(cur.blockFormat(), cur.block().text());
            ScreenplayFormat::applyBlockFormat(cur, ScreenplayFormat::cycleElement(current));
            setTextCursor(cur);
            return;
        }
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
            && !(event->modifiers() & Qt::ShiftModifier)) {
            QTextCursor before = textCursor();
            const ScreenplayElement current = ScreenplayFormat::detect(before.blockFormat(), before.block().text());
            uppercaseBlockIfNeeded(before, current);
            QTextEdit::keyPressEvent(event); // insere a quebra de bloco
            QTextCursor after = textCursor();
            ScreenplayFormat::applyBlockFormat(after, ScreenplayFormat::nextElement(current));
            setTextCursor(after);
            return;
        }
    }

    QTextEdit::keyPressEvent(event);
}

void SpellEditor::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control && !event->isAutoRepeat())
        emit refHighlightRequested(false);
    QTextEdit::keyReleaseEvent(event);
}

void SpellEditor::focusOutEvent(QFocusEvent* event)
{
    // Sem foco, o keyRelease do Ctrl não chega — limpa o realce pra não ficar preso.
    emit refHighlightRequested(false);
    QTextEdit::focusOutEvent(event);
}
