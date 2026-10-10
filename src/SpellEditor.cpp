#include "SpellEditor.h"
#include <QPaintEvent>
#include <QToolTip>

#include "PanelMotion.h"
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
#include <QResizeEvent>
#include <QScrollBar>
#include <QPixmap>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextImageFormat>
#include <QTextObjectInterface>
#include <QTimer>
#include <QVariantAnimation>
#include <qmath.h>

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
    connect(this, &QTextEdit::cursorPositionChanged, this, &SpellEditor::onScreenplayCursorMoved);
}

void SpellEditor::setPageMargins(int left, int top, int right, int bottom)
{
    m_pageMargins = QMargins(left, top, right, bottom);
    updateScreenplayColumn();
}

void SpellEditor::setScreenplayMode(bool on)
{
    // Documento trocado (applyEditorStyle roda a cada abertura): a linha
    // digitada era do documento anterior.
    m_typedBlock = QTextCursor();
    if (m_screenplayMode == on) return;
    m_screenplayMode = on;
    updateScreenplayColumn();
}

void SpellEditor::resizeEvent(QResizeEvent* event)
{
    QTextEdit::resizeEvent(event);
    updateScreenplayColumn();
}

void SpellEditor::updateScreenplayColumn()
{
    QMargins m = m_pageMargins;
    if (m_screenplayMode) {
        // Largura que o texto teria só com a margem da página.
        const QMargins cur = viewportMargins();
        const int baseWidth = viewport()->width() + cur.left() + cur.right() - m.left() - m.right();
        const int column = qCeil(ScreenplayFormat::columnWidthPx() + 2 * document()->documentMargin());
        const int extra = qMax(0, baseWidth - column);
        m.setLeft(m.left() + extra / 2);
        m.setRight(m.right() + extra - extra / 2);
    }
    if (m != viewportMargins()) setViewportMargins(m);
}

void SpellEditor::setSpellChecker(SpellChecker* checker)
{
    if (m_checker) disconnect(m_checker, nullptr, this, nullptr);
    m_checker = checker;
    // Idioma trocado ou palavra aprendida: "nome que também é palavra comum"
    // depende do dicionário.
    if (m_checker) connect(m_checker, &SpellChecker::changed, this, &SpellEditor::rebuildAutocorrectNames);
    rebuildAutocorrectNames();
}

void SpellEditor::setAutocorrectNames(const QStringList& names)
{
    if (names == m_autocorrectSource) return;
    m_autocorrectSource = names;
    rebuildAutocorrectNames();
}

void SpellEditor::rebuildAutocorrectNames()
{
    SpellChecker* checker = m_checker;
    m_autocorrect.setNames(m_autocorrectSource, [checker](const QString& w) {
        return checker && checker->isEnabled() && checker->isCorrect(w);
    });
}

namespace {
// Letra de palavra pra autocorreção: letras, acentos soltos e o apóstrofo do
// possessivo ("Wallidon's").
bool isNameChar(QChar c)
{
    return c.isLetter() || c.isMark() || c == QLatin1Char('\'') || c == QChar(0x2019);
}
}

SpellEditor::NameFix SpellEditor::nameFixBeforeKey(QKeyEvent* event) const
{
    if (!NameAutocorrect::enabledSetting() || m_autocorrect.isEmpty()) return {};
    if (!m_checker || !m_checker->isEnabled() || isReadOnly() || event->isAutoRepeat()) return {};
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) return {};

    // Só a tecla que TERMINA a palavra: espaço, Enter, pontuação, travessão,
    // aspas. Apóstrofo e hífen continuam a palavra.
    bool ends = event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
    if (!ends && event->text().size() == 1) {
        const QChar c = event->text().at(0);
        ends = !isNameChar(c) && !c.isDigit() && c != QLatin1Char('-') && c != QLatin1Char('_')
               && (c.isSpace() || c.isPunct() || c.isSymbol());
    }
    if (!ends) return {};

    const QTextCursor cur = textCursor();
    if (cur.hasSelection()) return {};
    const QString text = cur.block().text();
    const int end = cur.positionInBlock();
    if (end < text.size() && isNameChar(text.at(end))) return {};   // cursor no meio da palavra
    int start = end;
    while (start > 0 && isNameChar(text.at(start - 1))) --start;
    // Aspas simples abrindo a fala não fazem parte do nome.
    while (start < end && (text.at(start) == QLatin1Char('\'') || text.at(start) == QChar(0x2019))) ++start;
    if (end - start < 3) return {};
    if (start > 0 && (text.at(start - 1).isDigit() || text.at(start - 1) == QLatin1Char('-'))) return {};

    const QString word = text.mid(start, end - start);
    if (m_checker->isCorrect(word)) return {};
    const QString fixed = m_autocorrect.correctionFor(word);
    if (fixed.isEmpty()) return {};
    return { cur.block().position() + start, word, fixed, -1 };
}

void SpellEditor::applyNameFix(const NameFix& fix)
{
    QTextCursor c(document());
    c.setPosition(fix.start);
    c.setPosition(fix.start + fix.typed.size(), QTextCursor::KeepAnchor);
    const QString now = c.selectedText();
    // A tecla pode ter mexido na palavra (o roteiro põe o Personagem em caixa
    // alta no Enter): segue valendo se for a mesma palavra.
    if (now.compare(fix.typed, Qt::CaseInsensitive) != 0) return;
    QString fixed = fix.fixed;
    if (now != fix.typed && now.size() > 1 && now == now.toUpper()) fixed = fixed.toUpper();

    c.beginEditBlock();
    c.insertText(fixed);
    c.endEditBlock();
    m_lastNameFix = { fix.start, now, fixed, textCursor().position() };
    m_pulseText = fixed;
    pulseRange(fix.start, fixed.size());
}

bool SpellEditor::revertNameFix(QKeyEvent* event)
{
    if (m_lastNameFix.start < 0) return false;
    if (event->key() != Qt::Key_Backspace || event->modifiers() != Qt::NoModifier) return false;
    const NameFix fix = m_lastNameFix;
    m_lastNameFix = NameFix();
    const QTextCursor cur = textCursor();
    if (cur.hasSelection() || cur.position() != fix.cursorAfter) return false;

    QTextCursor c(document());
    c.setPosition(fix.start);
    c.setPosition(fix.start + fix.fixed.size(), QTextCursor::KeepAnchor);
    if (c.selectedText() != fix.fixed) return false;
    c.beginEditBlock();
    c.insertText(fix.typed);
    c.endEditBlock();
    m_pulseLength = 0;
    if (m_pulseAnim) m_pulseAnim->stop();
    viewport()->update();
    // A pessoa quis essa palavra: o projeto passa a aceitá-la, sem sublinhado
    // vermelho e sem nova correção.
    if (m_checker) m_checker->addToPersonalDictionary(NameAutocorrect::withoutPossessive(fix.typed));
    return true;
}

void SpellEditor::pulseRange(int start, int length)
{
    m_pulseStart = start;
    m_pulseLength = length;
    if (!m_pulseAnim) {
        m_pulseAnim = new QVariantAnimation(this);
        m_pulseAnim->setStartValue(0.0);
        m_pulseAnim->setEndValue(1.0);
        m_pulseAnim->setDuration(700);
        connect(m_pulseAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            // Sobe rápido e apaga devagar.
            const qreal t = v.toReal();
            m_pulse = t < 0.18 ? t / 0.18 : 1.0 - (t - 0.18) / 0.82;
            viewport()->update();
        });
        connect(m_pulseAnim, &QVariantAnimation::finished, this, [this]() {
            m_pulseLength = 0;
            viewport()->update();
        });
    }
    m_pulseAnim->stop();
    if (PanelMotion::enabled()) {
        m_pulseAnim->start();
    } else {
        // Sem animações: o realce aparece e some, sem esmaecer.
        m_pulse = 0.8;
        viewport()->update();
        QTimer::singleShot(450, this, [this]() { m_pulseLength = 0; viewport()->update(); });
    }
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

    // "Sinônimos de..." — só faz sentido para UMA palavra: o dicionário é
    // indexado por verbete, não por expressão.
    {
        QString word;
        QTextCursor target = textCursor();
        const bool hadSelection = target.hasSelection();
        if (!hadSelection) {
            target = cursorForPosition(event->pos());
            target.select(QTextCursor::WordUnderCursor);
        }
        word = target.selectedText().trimmed();
        word.remove(QChar(0x2029));

        if (!word.isEmpty() && !word.contains(QLatin1Char(' '))) {
            const QPoint globalPos = event->globalPos();
            // Selecionar antes de qualquer ação: o autor vê o que vai ser
            // trocado, e quem recebe o sinal só mexe no cursor atual em vez de
            // recalcular a posição do clique.
            auto ensureSelected = [this, target, hadSelection]() {
                if (!hadSelection) setTextCursor(target);
            };

            const QStringList quick = m_synProvider ? m_synProvider(word) : QStringList();

            auto* synMenu = new QMenu(tr("Sinônimos"), menu);
            for (const QString& s : quick) {
                QAction* a = synMenu->addAction(s);
                connect(a, &QAction::triggered, this, [this, s, ensureSelected]() {
                    ensureSelected();
                    emit synonymChosen(s);
                });
            }
            if (!quick.isEmpty()) synMenu->addSeparator();

            QAction* moreAct = synMenu->addAction(
                quick.isEmpty() ? tr("Procurar sinônimos...") : tr("Todos os sinônimos..."));
            connect(moreAct, &QAction::triggered, this, [this, word, globalPos, ensureSelected]() {
                ensureSelected();
                emit synonymsRequested(word, globalPos);
            });

            menu->insertMenu(stdAnchor, synMenu);
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

    // "Ler em voz alta" — ferramenta de revisão: ouvir o texto denuncia frase
    // truncada e diálogo que não soa natural. Com seleção lê só o trecho; sem
    // seleção, lê do clique até o fim (não do começo do documento — quem está
    // revisando quer continuar de onde parou).
    {
        QTextCursor sel = textCursor();
        const bool hasSel = sel.hasSelection();
        const int start = hasSel ? sel.selectionStart()
                                 : cursorForPosition(event->pos()).position();
        const int end = hasSel ? sel.selectionEnd() : -1;

        QAction* readAct = new QAction(
            hasSel ? tr("Ler seleção em voz alta") : tr("Ler em voz alta a partir daqui"),
            menu);
        connect(readAct, &QAction::triggered, this, [this, start, end]() {
            emit readAloudRequested(start, end);
        });
        menu->insertAction(stdAnchor, readAct);
        menu->insertSeparator(stdAnchor);
    }

    menu->exec(event->globalPos());
    delete menu;
}

void SpellEditor::mousePressEvent(QMouseEvent* event)
{
    m_lastNameFix = NameFix();
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

void SpellEditor::paintEvent(QPaintEvent* event)
{
    if (m_beforePaint) m_beforePaint();
    QTextEdit::paintEvent(event);
    if (m_pulseLength > 0 && m_pulse > 0
        && m_pulseStart + m_pulseLength < document()->characterCount()) {
        QTextCursor a(document());
        a.setPosition(m_pulseStart);
        a.setPosition(m_pulseStart + m_pulseLength, QTextCursor::KeepAnchor);
        // Texto trocado no meio do pulsar (outro capítulo, desfazer): não pinta.
        if (a.selectedText() != m_pulseText) m_pulseLength = 0;
        a.setPosition(m_pulseStart);
        const QRect r1 = cursorRect(a);
        a.setPosition(m_pulseStart + m_pulseLength);
        const QRect r2 = cursorRect(a);
        if (m_pulseLength > 0 && r1.top() == r2.top() && r2.left() > r1.left()) {
            QPainter p(viewport());
            p.setRenderHint(QPainter::Antialiasing);
            QColor c = palette().color(QPalette::Highlight);
            c.setAlphaF(0.38 * m_pulse);
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawRoundedRect(QRectF(r1.left() - 2, r1.top(), r2.left() - r1.left() + 4, r1.height()), 3, 3);
        }
    }
    if (!m_overlay) return;
    QPainter p(viewport());
    m_overlayTips = m_overlay(p, event->rect());
}

void SpellEditor::mouseMoveEvent(QMouseEvent* event)
{
    // Tooltip das quebras de cena ("Cena 2 · A carta · Dia 4, noite").
    for (const auto& t : std::as_const(m_overlayTips))
        if (t.first.contains(event->pos())) { QToolTip::showText(event->globalPosition().toPoint(), t.second, viewport(), t.first); break; }

    // Com Ctrl segurado, mostra a mãozinha sobre links de referência.
    if (event->modifiers() & Qt::ControlModifier) {
        const QString href = anchorAt(event->pos());
        viewport()->setCursor(isRefHref(href) ? Qt::PointingHandCursor : Qt::IBeamCursor);
    }
    QTextEdit::mouseMoveEvent(event);
}

namespace {
// Maiusculiza o texto do bloco ao sair dele (Cena/Personagem/Transição são
// sempre em caixa alta na convenção de roteiro) — no momento de sair da linha,
// não a cada tecla.
void uppercaseBlockIfNeeded(const QTextBlock& block, ScreenplayElement element)
{
    if (!block.isValid() || !ScreenplayFormat::isUppercaseElement(element)) return;
    const QString text = block.text();
    const QString upper = text.toUpper();
    if (text.isEmpty() || text == upper) return;
    QTextCursor c(block);
    c.movePosition(QTextCursor::StartOfBlock);
    c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    c.insertText(upper);
}
}

QSet<QString> SpellEditor::screenplayCuesWithDocument() const
{
    QSet<QString> cues = m_screenplayCues;
    for (QTextBlock b = document()->begin(); b.isValid(); b = b.next()) {
        if (ScreenplayFormat::detect(b.blockFormat(), b.text()) == ScreenplayElement::Character) {
            const QString cue = ScreenplayFormat::cueName(b.text()).toUpper();
            if (!cue.isEmpty()) cues.insert(cue);
        }
    }
    return cues;
}

void SpellEditor::onScreenplayCursorMoved()
{
    if (!m_screenplayMode || m_uppercasing || m_typedBlock.isNull()) return;
    if (m_typedBlock.document() != document()) { m_typedBlock = QTextCursor(); return; }
    const QTextBlock typed = m_typedBlock.block();
    if (typed == textCursor().block()) return;
    m_typedBlock = QTextCursor();
    m_uppercasing = true;
    const ScreenplayElement el = ScreenplayFormat::detect(typed.blockFormat(), typed.text());
    const ScreenplayElement finished = ScreenplayFormat::refineOnEnter(
        el, typed.text(), el == ScreenplayElement::Action ? screenplayCuesWithDocument() : QSet<QString>());
    if (finished != el) {
        QTextCursor tc(typed);
        ScreenplayFormat::applyBlockFormat(tc, finished);
    }
    uppercaseBlockIfNeeded(typed, finished);
    m_uppercasing = false;
    emit screenplayLineFinished();
}

bool SpellEditor::screenplayKeyPress(QKeyEvent* event)
{
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) return false;
    QTextCursor cur = textCursor();
    const ScreenplayElement current = ScreenplayFormat::detect(cur.blockFormat(), cur.block().text());

    if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab) {
        const bool back = event->key() == Qt::Key_Backtab || (event->modifiers() & Qt::ShiftModifier);
        ScreenplayFormat::applyBlockFormat(cur, ScreenplayFormat::cycleElement(current, back));
        setTextCursor(cur);
        emit screenplayElementChanged();
        return true;
    }

    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && !(event->modifiers() & Qt::ShiftModifier)) {
        // Enter numa linha vazia que não é Ação só troca a linha pra Ação — é
        // a saída quando o Enter anterior abriu um elemento que você não quer.
        if (!cur.hasSelection() && cur.block().text().trimmed().isEmpty()
            && current != ScreenplayElement::Action) {
            ScreenplayFormat::applyBlockFormat(cur, ScreenplayElement::Action);
            setTextCursor(cur);
            emit screenplayElementChanged();
            return true;
        }
        ScreenplayElement finished = ScreenplayFormat::refineOnEnter(
            current, cur.block().text(),
            current == ScreenplayElement::Action ? screenplayCuesWithDocument() : QSet<QString>());
        if (finished != current) ScreenplayFormat::applyBlockFormat(cur, finished);
        m_uppercasing = true;
        uppercaseBlockIfNeeded(cur.block(), finished);
        m_uppercasing = false;
        m_typedBlock = QTextCursor();
        QTextEdit::keyPressEvent(event); // insere a quebra de bloco
        QTextCursor after = textCursor();
        ScreenplayFormat::applyBlockFormat(after, ScreenplayFormat::nextElement(finished));
        setTextCursor(after);
        emit screenplayElementChanged();
        if (finished == ScreenplayElement::Scene) emit screenplayLineFinished();
        return true;
    }

    // "(" no começo de uma fala vazia abre os Parênteses, como nos programas de roteiro.
    if (event->text() == QLatin1String("(") && current == ScreenplayElement::Dialogue
        && cur.block().text().isEmpty()) {
        ScreenplayFormat::applyBlockFormat(cur, ScreenplayElement::Parenthetical);
        setTextCursor(cur);
        emit screenplayElementChanged();
    }
    return false;
}

void SpellEditor::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control && !event->isAutoRepeat())
        emit refHighlightRequested(true);   // "modo ver os links"

    if (revertNameFix(event)) return;
    // Qualquer outra tecla (fora Shift/Ctrl/Alt sozinhos) encerra a chance de desfazer.
    const int k = event->key();
    if (k != Qt::Key_Shift && k != Qt::Key_Control && k != Qt::Key_Alt && k != Qt::Key_Meta && k != Qt::Key_AltGr)
        m_lastNameFix = NameFix();

    const NameFix fix = nameFixBeforeKey(event);
    handleKey(event);
    if (fix.start >= 0) applyNameFix(fix);
}

void SpellEditor::handleKey(QKeyEvent* event)
{
    if (m_screenplayMode) {
        if (screenplayKeyPress(event)) return;
        QTextEdit::keyPressEvent(event);
        if (!event->text().isEmpty() && event->text().at(0).isPrint())
            m_typedBlock = QTextCursor(textCursor().block());
        return;
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
    m_lastNameFix = NameFix();
    QTextEdit::focusOutEvent(event);
}
