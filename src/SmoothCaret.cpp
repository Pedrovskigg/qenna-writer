#include "SmoothCaret.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QStandardPaths>
#include <QEasingCurve>
#include <QEvent>
#include <QKeyEvent>
#include "Theme.h"
#include <QPainter>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextLayout>
#include <QTimer>
#include <QVariantAnimation>
#include <cmath>

namespace {
constexpr qreal kMaxGlide = 700;      // saltos maiores (Ctrl+End, clique longe) pulam direto
SmoothCaret* g_instance = nullptr;
QList<SmoothCaret*> g_all;            // editor principal + caixa de teste
int g_glide = -1, g_fade = -1;        // -1 = ainda não leu das configurações

QRectF lerp(const QRectF& a, const QRectF& b, qreal t) {
    return QRectF(a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t,
                  a.width() + (b.width() - a.width()) * t, a.height() + (b.height() - a.height()) * t);
}
}

bool SmoothCaret::enabledSetting() {
    return QSettings().value(QStringLiteral("ui/smoothCaret"), true).toBool();
}

void SmoothCaret::setEnabledSetting(bool on) {
    QSettings().setValue(QStringLiteral("ui/smoothCaret"), on);
    for (SmoothCaret* c : std::as_const(g_all)) c->setActive(on);
}

int SmoothCaret::glideMs() {
    if (g_glide < 0) g_glide = qBound(0, QSettings().value(QStringLiteral("ui/smoothCaretGlideMs"), kDefaultGlideMs).toInt(), 400);
    return g_glide;
}

int SmoothCaret::fadeMs() {
    if (g_fade < 0) g_fade = qBound(0, QSettings().value(QStringLiteral("ui/smoothCaretFadeMs"), kDefaultFadeMs).toInt(), 400);
    return g_fade;
}

void SmoothCaret::setGlideMs(int ms) {
    g_glide = qBound(0, ms, 400);
    QSettings().setValue(QStringLiteral("ui/smoothCaretGlideMs"), g_glide);
    for (SmoothCaret* c : std::as_const(g_all)) c->m_anim->setDuration(qMax(1, g_glide));
}

void SmoothCaret::setFadeMs(int ms) {
    g_fade = qBound(0, ms, 400);
    QSettings().setValue(QStringLiteral("ui/smoothCaretFadeMs"), g_fade);
}

SmoothCaret* SmoothCaret::attach(QTextEdit* editor) {
    if (!editor) return nullptr;
    auto* c = new SmoothCaret(editor);
    c->setActive(enabledSetting());
    return c;
}

SmoothCaret::~SmoothCaret() {
    g_all.removeAll(this);
    if (g_instance == this) g_instance = nullptr;
}

SmoothCaret* SmoothCaret::instance() { return g_instance; }

SmoothCaret* SmoothCaret::install(QTextEdit* editor) {
    if (!editor) return nullptr;
    auto* c = new SmoothCaret(editor);
    g_instance = c;
    c->setActive(enabledSetting());
    return c;
}

SmoothCaret::SmoothCaret(QTextEdit* editor)
    : QWidget(editor), m_ed(editor) {
    // Irmão do viewport, não filho: ao rolar, o Qt empurra os FILHOS do
    // viewport junto com o texto (QWidget::scroll) — o cursor ia parar
    // parágrafos acima de onde o texto estava.
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    m_nativeWidth = qMax(1, editor->cursorWidth());
    setGeometry(editor->viewport()->geometry());

    g_all.append(this);
    m_anim = new QVariantAnimation(this);
    m_anim->setDuration(qMax(1, glideMs()));
    m_anim->setStartValue(0.0);
    m_anim->setEndValue(1.0);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        // De onde saiu (no documento) até onde o cursor do Qt está AGORA.
        const QRectF old = m_cur;
        const QPointF off = scrollOffset();
        m_to = caretRect();
        m_cur = lerp(m_fromDoc.translated(-off), m_to, v.toReal());
        updateArea(old, m_cur);
    });

    m_blink = new QTimer(this);
    connect(m_blink, &QTimer::timeout, this, [this]() {
        m_on = !m_on;
        updateArea(m_cur, m_cur);
    });

    connect(editor, &QTextEdit::cursorPositionChanged, this, [this]() {
        startLetterFade();
        retarget(m_keyMove);
    });
    m_fadeTick = new QTimer(this);
    m_fadeTick->setInterval(16);
    connect(m_fadeTick, &QTimer::timeout, this, [this]() {
        QRectF dirty;
        for (int i = m_patches.size() - 1; i >= 0; --i) {
            const QRectF r = m_patches.at(i).rect.translated(-scrollOffset());
            dirty = dirty.isNull() ? r : dirty.united(r);
            if (m_patches.at(i).clock.elapsed() > fadeMs()) m_patches.removeAt(i);
        }
        if (m_patches.isEmpty()) m_fadeTick->stop();
        if (!dirty.isNull()) update(dirty.adjusted(-2, -2, 2, 2).toAlignedRect());
    });
    connect(editor, &QTextEdit::textChanged, this, [this]() { watchDocument(); });
    // Rolagem: a posição sai de novo do cursor do Qt (sem somar deslocamento).
    auto onScroll = [this](int) {
        if (m_anim->state() != QAbstractAnimation::Running) m_cur = m_to = caretRect();
        update();
    };
    connect(editor->verticalScrollBar(), &QScrollBar::valueChanged, this, onScroll);
    connect(editor->horizontalScrollBar(), &QScrollBar::valueChanged, this, onScroll);
    editor->installEventFilter(this);
    editor->viewport()->installEventFilter(this);
    watchDocument();
}

void SmoothCaret::setActive(bool on) {
    m_active = on;
    if (!m_ed) return;
    m_ed->setCursorWidth(on ? 0 : m_nativeWidth);
    setVisible(on);
    if (on) { raise(); retarget(false); restartBlink(); }
    else m_blink->stop();
}

void SmoothCaret::watchDocument() {
    if (!m_ed || m_doc == m_ed->document()) return;
    if (m_doc && m_doc->documentLayout()) disconnect(m_doc->documentLayout(), nullptr, this, nullptr);
    m_doc = m_ed->document();
    if (!m_doc || !m_doc->documentLayout()) return;
    // Mudança de layout sem o cursor andar (fonte, largura da página, cena
    // carregada): o destino se ajusta; se não está deslizando, vai direto.
    // Durante uma tecla, NÃO: o Qt avisa do layout ANTES do cursorPositionChanged,
    // e o retarget(false) daqui teletransportava o cursor pro destino — o
    // deslize que vinha logo depois achava distância zero e não saía. Era o
    // "o cursor suave às vezes para": só deslizava quando o aviso chegava tarde.
    connect(m_doc->documentLayout(), &QAbstractTextDocumentLayout::update, this, [this](const QRectF&) {
        if (!m_active || m_keyMove) return;
        if (m_anim->state() == QAbstractAnimation::Running) m_to = caretRect();
        else retarget(false);
    });
}

QRectF SmoothCaret::caretRect() const {
    if (!m_ed) return QRectF();
    // Altura e topo: os do cursor do próprio Qt (entrelinha incluída); o x vem
    // do layout da linha, que não depende da largura do cursor nativo.
    const QRect nat = m_ed->cursorRect();
    const QTextCursor c = m_ed->textCursor();
    const QTextBlock b = c.block();
    const QTextLayout* lay = b.isValid() ? b.layout() : nullptr;
    qreal x = nat.center().x();
    if (lay && lay->lineCount() > 0) {
        const int rel = c.position() - b.position();
        QTextLine line = lay->lineForTextPosition(rel);
        if (!line.isValid()) line = lay->lineAt(lay->lineCount() - 1);
        x = line.cursorToX(rel) + lay->position().x() - m_ed->horizontalScrollBar()->value();
    }
    return QRectF(x, nat.top(), 0, nat.height());
}

void SmoothCaret::retarget(bool animate) {
    if (!m_ed || !m_active) return;
    watchDocument();
    const QRectF t = caretRect();
    restartBlink();
    const qreal dist = std::hypot(t.x() - m_cur.x(), t.y() - m_cur.y());
    if (!animate || glideMs() <= 0 || !isVisible() || m_cur.isNull() || m_cur.height() <= 0 || dist > kMaxGlide) {
        m_anim->stop();
        const QRectF old = m_cur;
        m_cur = m_from = m_to = t;
        updateArea(old, m_cur);
        return;
    }
    if (dist < 0.5) { m_to = t; return; }
    // Digitando rápido, o destino muda no meio do caminho: parte de onde está.
    m_from = m_cur;
    m_fromDoc = m_cur.translated(scrollOffset());
    m_to = t;
    m_anim->stop();
    m_anim->start();
}

void SmoothCaret::restartBlink() {
    m_on = true;
    const int flash = QApplication::cursorFlashTime();
    if (flash > 0) m_blink->start(flash / 2);
    else m_blink->stop();
    updateArea(m_cur, m_cur);
}

bool SmoothCaret::shouldDraw() const {
    if (!m_active || !m_ed || m_grabbing || m_cur.height() <= 0) return false;
    if (!m_ed->hasFocus() && !m_ed->viewport()->hasFocus()) return false;
    if (!(m_ed->textInteractionFlags() & Qt::TextEditable)) return false;
    return m_on || m_anim->state() == QAbstractAnimation::Running;
}

void SmoothCaret::updateArea(const QRectF& a, const QRectF& b) {
    const QRectF r = (a.isNull() ? b : a.united(b)).adjusted(-3, -2, 3 + m_nativeWidth, 2);
    update(r.toAlignedRect());
}

bool SmoothCaret::eventFilter(QObject* watched, QEvent* event) {
    if (m_ed && (watched == m_ed->viewport() || watched == m_ed)
        && (event->type() == QEvent::Resize || event->type() == QEvent::Move)) {
        setGeometry(m_ed->viewport()->geometry());
        raise();
        retarget(false);
    } else if (m_ed && watched == m_ed
               && (event->type() == QEvent::KeyPress
                   || (event->type() == QEvent::InputMethod
                       && !static_cast<QInputMethodEvent*>(event)->commitString().isEmpty()))) {
        // InputMethod: texto que o Windows entrega pelos serviços de texto
        // (ditado, sugestões de digitação, teclados de composição) em vez de
        // tecla. Sem isso, nesse caminho a letra nunca tinha fade.
        const QString txt = event->type() == QEvent::KeyPress ? static_cast<QKeyEvent*>(event)->text()
                                                              : static_cast<QInputMethodEvent*>(event)->commitString();
        const bool letter = txt.size() >= 1 && !txt.at(0).isSpace() && txt.at(0).isPrint()
                            && !(static_cast<QInputEvent*>(event)->modifiers() & (Qt::ControlModifier | Qt::AltModifier));
        // Letra anterior que nunca chegou ao cursorPositionChanged.
        if (m_pending && m_typedLetter) diag(QStringLiteral("letra sem cursorPositionChanged"));
        m_typedLetter = letter;
        if (letter) ensureLayer();
        prepareLetterFade();
        // O cursor anda dentro do processamento desta tecla; depois dela,
        // qualquer movimento volta a ser teleporte.
        m_keyMove = true;
        QTimer::singleShot(0, this, [this]() { m_keyMove = false; });
    } else if (m_ed && watched == m_ed->viewport()
               && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)) {
        m_keyMove = false;
    } else if (m_ed && watched == m_ed && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)) {
        if (event->type() == QEvent::FocusIn) retarget(false);
        restartBlink();
        update();
    }
    return false;
}

QColor SmoothCaret::solidPageColor(qreal y) const {
    if (!m_ed) return QColor();
    // Pergunta ao próprio editor que cor ele pinta na margem desta linha (um
    // pixel, dentro da margem do documento). A folha de estilo do Qt mexe no
    // autoFillBackground do viewport, então paleta e atributos não dizem a
    // verdade; o pixel diz. Opaco = página lisa; com alfa = translúcida.
    QWidget* vp = m_ed->viewport();
    const int py = qBound(0, int(y), vp->height() - 1);
    m_grabbing = true;
    const QColor c = vp->grab(QRect(1, py, 1, 1)).toImage().pixelColor(0, 0);
    m_grabbing = false;
    return c.alpha() == 255 ? c : QColor();
}

void SmoothCaret::ensureLayer() {
    // O que o liga/desliga da opção conserta, conferido a cada letra: a camada
    // do cursor com o tamanho do viewport e por cima dele. Se algo desfez,
    // anota (caixa-preta) e arruma.
    if (!m_ed || !m_active) return;
    QWidget* vp = m_ed->viewport();
    if (geometry() != vp->geometry()) {
        diag(QStringLiteral("camada fora do lugar: %1 x viewport %2")
                 .arg(rectText(geometry()), rectText(vp->geometry())));
        setGeometry(vp->geometry());
    }
    const QObjectList kids = m_ed->children();
    if (kids.indexOf(this) < kids.indexOf(vp)) {
        diag(QStringLiteral("camada abaixo do viewport"));
        raise();
    }
    if (!isVisible()) {
        diag(QStringLiteral("camada invisível com a opção ligada"));
        setVisible(true);
    }
}

QString SmoothCaret::rectText(const QRect& r) {
    return QStringLiteral("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}

void SmoothCaret::diag(const QString& reason) const {
    // Caixa-preta do fade (2026-10-10): o "fade morre do nada" não reproduz em
    // harness. Quando uma letra comum não ganha fade por motivo inesperado,
    // anota o motivo e o estado em smoothcaret.log, ao lado do crash.log.
    // No máximo uma linha por motivo a cada 20 s.
    static QHash<QString, qint64> last;
    const QString key = reason.section(QLatin1Char(':'), 0, 0);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - last.value(key, 0) < 20000) return;
    last.insert(key, now);
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    QFile f(dir + QStringLiteral("/smoothcaret.log"));
    if (f.size() > 256 * 1024) f.remove();
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    QString state;
    if (m_ed) {
        const QTextCursor c = m_ed->textCursor();
        QWidget* fw = QApplication::focusWidget();
        state = QStringLiteral("camada %1 | viewport %2 | visível %3 | ativo %4 | fade %5ms | foco %6 | rolagem %7,%8 | bloco %9/%10 | página %11")
                    .arg(rectText(geometry()), rectText(m_ed->viewport()->geometry()))
                    .arg(isVisible()).arg(m_active).arg(fadeMs())
                    .arg(fw ? QString::fromLatin1(fw->metaObject()->className()) + QLatin1Char('#') + fw->objectName() : QStringLiteral("-"))
                    .arg(m_ed->horizontalScrollBar()->value()).arg(m_ed->verticalScrollBar()->value())
                    .arg(c.positionInBlock()).arg(c.block().length() - 1)
                    .arg(solidPageColor(caretRect().center().y()).isValid() ? QStringLiteral("lisa") : QStringLiteral("translúcida"));
    }
    f.write((QDateTime::currentDateTime().toString(Qt::ISODate) + QStringLiteral("  ") + reason
             + QStringLiteral("  ||  ") + state + QLatin1Char('\n')).toUtf8());
}

void SmoothCaret::prepareLetterFade() {
    m_pending = false;
    if (!m_active || !m_ed || fadeMs() <= 0) return;
    if (!isVisible()) { if (m_typedLetter) diag(QStringLiteral("camada invisível")); return; }
    const QTextCursor c = m_ed->textCursor();
    if (c.hasSelection()) return;
    const QRectF cr = caretRect();
    if (cr.height() <= 0) { if (m_typedLetter) diag(QStringLiteral("cursor sem altura")); return; }
    m_pendingFrom = cr.translated(scrollOffset());
    m_pendingPos = c.position();
    m_pendingPm = QPixmap();

    // Página lisa: a cobertura é cor, decidida depois da tecla. Sem foto, e
    // por isso vale também no meio da linha. (Antes era sempre foto, e no meio
    // da linha não havia fade: voltar pra mexer numa frase "desligava" o
    // efeito sem aviso, e parecia que ele morria do nada.)
    m_pendingFill = solidPageColor(cr.center().y());
    if (m_pendingFill.isValid()) { m_pending = true; return; }

    // Página translúcida: foto do vazio, só no fim da linha (no meio, o texto
    // da direita anda e a foto não seria vazio). Tirada da JANELA, com o fundo
    // do tema que aparece através da página: o viewport sozinho dava só a cor
    // da página meio transparente, e a "cobertura" virava um quadrado claro.
    if (!c.atBlockEnd()) return;
    const int w = qMax(24, int(cr.height() * 1.6));
    const QRect g = QRect(int(std::floor(cr.x())), int(std::floor(cr.y())), w, int(std::ceil(cr.height())) + 1)
                        .intersected(m_ed->viewport()->rect());
    if (g.isEmpty()) return;
    QWidget* win = m_ed->window();
    const QRect inWin(m_ed->viewport()->mapTo(win, g.topLeft()), g.size());
    m_grabbing = true;   // o próprio cursor não entra na foto
    m_pendingPm = win->grab(inWin);
    m_grabbing = false;
    m_pendingOrigin = QPointF(g.topLeft()) + scrollOffset();
    m_pending = true;
}

void SmoothCaret::startLetterFade() {
    if (!m_pending) return;
    m_pending = false;
    if (!m_ed) return;
    const QRectF now = caretRect().translated(scrollOffset());
    const int pos = m_ed->textCursor().position();
    // A letra caiu na mesma linha, logo depois de onde estava o cursor.
    if (std::abs(now.y() - m_pendingFrom.y()) > 1.0 || now.x() <= m_pendingFrom.x() + 0.5 || pos <= m_pendingPos) {
        // Quebra de linha (a palavra desceu) é esperado; o resto, anota.
        const bool wrapped = now.y() > m_pendingFrom.y() + 1.0 && now.x() < m_pendingFrom.x();
        if (m_typedLetter && !wrapped)
            diag(QStringLiteral("letra sem fade: y %1 -> %2, x %3 -> %4, pos %5 -> %6")
                     .arg(m_pendingFrom.y(), 0, 'f', 1).arg(now.y(), 0, 'f', 1)
                     .arg(m_pendingFrom.x(), 0, 'f', 1).arg(now.x(), 0, 'f', 1)
                     .arg(m_pendingPos).arg(pos));
        return;
    }
    Patch p;
    p.rect = QRectF(m_pendingFrom.x() - 0.5, m_pendingFrom.y(), now.x() - m_pendingFrom.x() + 1.0, m_pendingFrom.height());
    if (m_pendingFill.isValid()) {
        // Letra caindo dentro de um marcador: cobre com a cor do grifo.
        QTextCursor at(m_ed->document());
        at.setPosition(pos);
        const QBrush bg = at.charFormat().background();
        QColor fill = m_pendingFill;
        if (bg.style() != Qt::NoBrush && bg.color().alpha() > 0) {
            const QColor g = bg.color();
            const qreal a = g.alphaF();
            fill = QColor::fromRgbF(g.redF() * a + fill.redF() * (1 - a), g.greenF() * a + fill.greenF() * (1 - a),
                                    g.blueF() * a + fill.blueF() * (1 - a));
        }
        p.fill = fill;
    } else {
        p.rect = p.rect.intersected(QRectF(m_pendingOrigin, QSizeF(m_pendingPm.size()) / m_pendingPm.devicePixelRatio()));
        p.pm = m_pendingPm;
        p.origin = m_pendingOrigin;
    }
    if (p.rect.isEmpty()) return;
    p.clock.start();
    m_patches.append(p);
    if (!m_fadeTick->isActive()) m_fadeTick->start();
    update(p.rect.translated(-scrollOffset()).adjusted(-1, -1, 1, 1).toAlignedRect());
}

QPointF SmoothCaret::scrollOffset() const {
    if (!m_ed) return QPointF();
    return QPointF(m_ed->horizontalScrollBar()->value(), m_ed->verticalScrollBar()->value());
}

void SmoothCaret::paintEvent(QPaintEvent*) {
    QPainter p(this);
    // Parado, o cursor fica exatamente onde o do Qt está.
    if (m_anim->state() != QAbstractAnimation::Running) m_cur = m_to = caretRect();
    const QPointF off = scrollOffset();
    // Letras novas: a foto do vazio por cima, cada vez mais transparente.
    p.translate(-off);
    for (const Patch& patch : std::as_const(m_patches)) {
        const qreal t = std::clamp(patch.clock.elapsed() / qreal(qMax(1, fadeMs())), 0.0, 1.0);
        const qreal cover = 1.0 - (1.0 - std::pow(1.0 - t, 2.0));   // sai rápido no começo
        if (cover <= 0.01) continue;
        p.save();
        p.setOpacity(cover);
        if (patch.fill.isValid()) {
            p.fillRect(patch.rect, patch.fill);
        } else {
            p.setClipRect(patch.rect);
            p.drawPixmap(patch.origin, patch.pm);
        }
        p.restore();
    }
    p.translate(off);
    if (!shouldDraw()) return;
    const QColor color = Theme::toColor(Theme::editorTextColor());
    p.fillRect(QRectF(m_cur.x(), m_cur.y(), m_nativeWidth, m_cur.height()), color.isValid() ? color : m_ed->palette().color(QPalette::Text));
}
