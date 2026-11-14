#include "Relevo.h"

#include "Theme.h"

#include <QAbstractButton>
#include <QApplication>
#include <QChildEvent>
#include <QCursor>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QSettings>
#include <QStyle>
#include <QStyleOption>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

namespace Relevo {

bool enabled()
{
    return QSettings().value(QStringLiteral("ui/relevo"), false).toBool();
}

void setEnabled(bool on)
{
    if (enabled() == on) return;
    QSettings().setValue(QStringLiteral("ui/relevo"), on);
    emit Notifier::instance()->changed(on);
}

Notifier* Notifier::instance()
{
    static Notifier* n = new Notifier;
    return n;
}

} // namespace Relevo

namespace {

constexpr int kRestDelayMs = 450;   // o mouse sai e a barra ainda respira um pouco
constexpr int kWakeMs = 170;        // acordar é rápido: o mouse chegou pra usar
constexpr int kSleepMs = 520;       // dormir é devagar, como quem respira

QColor mixColor(const QColor& a, const QColor& b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

// A mesma silhueta numa cor só.
QImage tinted(const QImage& mask, const QColor& c)
{
    QImage t(mask.size(), QImage::Format_ARGB32_Premultiplied);
    t.fill(c);
    QPainter p(&t);
    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    p.drawImage(0, 0, mask);
    return t;
}

} // namespace

// ============================================================================

void RelevoEffect::setBlend(const Layers& layers, qreal t)
{
    m_blending = true;
    m_layers = layers;
    m_t = t;
    update();
}

namespace {
qreal smooth(qreal a, qreal b, qreal x)
{
    const qreal t = qBound(0.0, (x - a) / (b - a), 1.0);
    return t * t * (3 - 2 * t);
}
}

void RelevoEffect::draw(QPainter* painter)
{
    if (m_blending) {
        // As fotos já estão na escala da tela; o pintor está nas coordenadas do widget.
        painter->save();
        const qreal t = qBound(0.0, m_t, 1.0);
        // Painel some na primeira parte; a gravação entra por baixo dos ícones
        // e os ícones de verdade só apagam depois que ela já está lá.
        const qreal panelA = 1.0 - smooth(0.0, 0.6, t);
        const qreal engA = smooth(0.1, 0.55, t);
        const qreal inkA = 1.0 - smooth(0.4, 1.0, t);
        auto layer = [&](const QPixmap& pm, qreal a) {
            if (a <= 0.001 || pm.isNull()) return;
            painter->setOpacity(a);
            painter->drawPixmap(0, 0, pm);
        };
        layer(m_layers.panel, panelA);
        layer(m_layers.engraved, engA);
        layer(m_layers.ink, inkA);
        painter->restore();
        return;
    }
    if (beforeDraw) beforeDraw();
    QPoint offset;
    const QPixmap src = sourcePixmap(Qt::DeviceCoordinates, &offset, QGraphicsEffect::NoPad);
    if (src.isNull()) return;
    QImage in = src.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    in.setDevicePixelRatio(1.0);
    const QImage out = engrave(in);
    painter->save();
    painter->setWorldTransform(QTransform());
    painter->drawImage(offset, out);
    painter->restore();
}

QImage RelevoEffect::engrave(const QImage& srcImg)
{
    const QImage in = srcImg.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = in.width(), h = in.height();

    // Silhueta: só o que é tinta de verdade (ícone, letra, setinha). Véus
    // translúcidos (hover, botão marcado) ficam de fora pelo alfa, e fundos
    // opacos da cor do painel (abas, faixas, cartões) ficam de fora pelo
    // contraste: sem isso viravam blocos lisos e engoliam o texto (13/11).
    const QColor panel = Theme::toColor(Theme::panelBackground());
    const qreal panelL = (0.299 * panel.redF() + 0.587 * panel.greenF() + 0.114 * panel.blueF());
    auto ramp = [](qreal a, qreal b, qreal x) {
        const qreal t = qBound(0.0, (x - a) / (b - a), 1.0);
        return t * t * (3 - 2 * t);
    };
    QImage mask(w, h, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < h; ++y) {
        const QRgb* s = reinterpret_cast<const QRgb*>(in.constScanLine(y));
        QRgb* m = reinterpret_cast<QRgb*>(mask.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const int a = qAlpha(s[x]);
            if (a <= 80) { m[x] = 0; continue; }
            // Cor sem a pré-multiplicação, pra comparar com o painel.
            const qreal r = qRed(s[x]) / qreal(a), g = qGreen(s[x]) / qreal(a), bl = qBlue(s[x]) / qreal(a);
            const qreal L = 0.299 * r + 0.587 * g + 0.114 * bl;
            const qreal contrast = ramp(0.07, 0.20, qAbs(L - panelL));
            const qreal o = ramp(80.0, 200.0, a) * contrast;
            m[x] = qRgba(0, 0, 0, qRound(255 * o));
        }
    }

    const QColor table = Theme::toColor(Theme::appBackground());
    const QColor ink = Theme::toColor(Theme::iconColor());
    const bool lightTable = table.lightnessF() > 0.5;
    const QColor glyph = mixColor(table, ink, 0.38);
    QColor hi(255, 255, 255), lo(0, 0, 0);
    hi.setAlphaF(lightTable ? 0.60 : 0.10);
    lo.setAlphaF(lightTable ? 0.22 : 0.60);

    QImage out(w, h, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    {
        QPainter op(&out);
        op.drawImage(0, -1, tinted(mask, lo));   // sombra na borda de cima
        op.drawImage(0, 1, tinted(mask, hi));    // luz na borda de baixo
        op.drawImage(0, 0, tinted(mask, glyph));
    }
    out.setDevicePixelRatio(srcImg.devicePixelRatio());
    return out;
}

// ============================================================================

RelevoController::RelevoController(QWidget* bar)
    : QObject(bar)
    , m_bar(bar)
{
    m_restTimer = new QTimer(this);
    m_restTimer->setSingleShot(true);
    m_restTimer->setInterval(kRestDelayMs);
    connect(m_restTimer, &QTimer::timeout, this, &RelevoController::rest);

    // Conteúdo que se mexe com a barra dormindo (layout, carrossel, botão que
    // aparece): repinta a gravação depois que assentar. Ver a memória da sombra
    // que congelava a folha: o efeito pinta de um cache.
    m_repaintTimer = new QTimer(this);
    m_repaintTimer->setSingleShot(true);
    m_repaintTimer->setInterval(40);
    connect(m_repaintTimer, &QTimer::timeout, this, [this]() {
        if (m_resting && m_effect) m_effect->update();
    });

    watch(m_bar);
    connect(Relevo::Notifier::instance(), &Relevo::Notifier::changed, this, &RelevoController::refresh);
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, [this]() {
        if (m_resting && m_effect) m_effect->update();
    });
    QTimer::singleShot(0, this, &RelevoController::refresh);
}

void RelevoController::watch(QObject* o)
{
    o->installEventFilter(this);
    for (QObject* c : o->children())
        if (c->isWidgetType()) watch(c);
}

void RelevoController::refresh()
{
    if (!Relevo::enabled()) {
        m_restTimer->stop();
        if (m_anim) { QVariantAnimation* old = m_anim; m_anim.clear(); old->stop(); old->deleteLater(); }
        if (m_effect) { m_effect->clearBlend(); m_effect->setEnabled(false); }
        m_t = 0.0;
        if (m_resting) { m_resting = false; restoreBackgrounds(); m_bar->update(); }
        if (m_effect && m_bar->graphicsEffect() == m_effect) m_bar->setGraphicsEffect(nullptr);
        return;
    }
    if (mouseOver()) wake();
    else rest();
}

bool RelevoController::mouseOver() const
{
    if (!m_bar->isVisible()) return false;
    if (m_bar->rect().contains(m_bar->mapFromGlobal(QCursor::pos()))) return true;
    return extraHover && extraHover();
}

RelevoEffect::Layers RelevoController::captureLayers()
{
    const bool on = m_effect && m_effect->isEnabled();
    if (on) m_effect->setEnabled(false);
    RelevoEffect::Layers L;
    restoreBackgrounds();
    const qreal dpr = m_bar->devicePixelRatioF();
    // Painel: só o fundo da própria barra, pintado pelo estilo (o mesmo QSS).
    {
        QPixmap pm(qRound(m_bar->width() * dpr), qRound(m_bar->height() * dpr));
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        QStyleOption opt;
        opt.initFrom(m_bar);
        m_bar->style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, m_bar);
        p.end();
        L.panel = pm;
    }
    // Ícones: tudo que está dentro, com a cor de verdade (as faixas de dentro
    // continuam com o fundo delas).
    // Sem o grab(): ele pinta a cor de fundo da janela atrás de tudo, e a
    // gravação virava uma tarja. Aqui só o conteúdo, em fundo transparente.
    auto contentOnly = [&]() {
        QPixmap pm(qRound(m_bar->width() * dpr), qRound(m_bar->height() * dpr));
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        m_bar->render(&pm, QPoint(), QRegion(), QWidget::DrawChildren);
        return pm;
    };
    m_bar->setAttribute(Qt::WA_StyledBackground, false);
    L.ink = contentOnly();
    m_bar->setAttribute(Qt::WA_StyledBackground, true);
    // Gravação: sem fundo nenhum.
    stripBackgrounds();
    L.engraved = QPixmap::fromImage(RelevoEffect::engrave(contentOnly().toImage()));
    if (on) m_effect->setEnabled(true);
    return L;
}

bool RelevoController::ensureEffect()
{
    QGraphicsEffect* current = m_bar->graphicsEffect();
    if (m_effect && current == m_effect) return true;
    // O modo focado deixa um efeito de transparência na barra depois do
    // esmaecer. Parado e opaco, ele pode sair; no meio do esmaecer, não.
    if (current) {
        auto* fade = qobject_cast<QGraphicsOpacityEffect*>(current);
        if (!fade || fade->opacity() < 0.999) return false;
    }
    auto* e = new RelevoEffect(m_bar);
    e->beforeDraw = [this]() { stripBackgrounds(); };
    e->setEnabled(false);
    m_bar->setGraphicsEffect(e);   // apaga o efeito que estava lá
    m_effect = e;
    return true;
}

void RelevoController::animateTo(qreal target)
{
    if (m_anim) { QVariantAnimation* old = m_anim; m_anim.clear(); old->stop(); old->deleteLater(); }
    const bool sleeping = target > 0.5;
    auto* a = new QVariantAnimation(this);
    a->setStartValue(m_t);
    a->setEndValue(target);
    a->setDuration(qMax(1, qRound((sleeping ? kSleepMs : kWakeMs) * qAbs(target - m_t))));
    a->setEasingCurve(sleeping ? QEasingCurve::InOutCubic : QEasingCurve::OutCubic);
    connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_t = v.toReal();
        if (m_effect) m_effect->setBlendT(m_t);
    });
    connect(a, &QVariantAnimation::finished, this, [this, sleeping, a]() {
        if (m_anim != a) return;
        m_anim.clear();
        a->deleteLater();
        if (!m_effect) return;
        m_effect->clearBlend();
        if (sleeping) {
            m_effect->setEnabled(true);    // gravação ao vivo
        } else {
            restoreBackgrounds();
            m_effect->setEnabled(false);   // a barra de verdade
        }
        m_bar->update();
    });
    m_anim = a;
    a->start();
}

void RelevoController::wake()
{
    m_restTimer->stop();
    if (!m_resting) return;
    m_resting = false;
    if (!m_effect || !m_bar->isVisible()) {
        if (m_anim) { QVariantAnimation* old = m_anim; m_anim.clear(); old->stop(); old->deleteLater(); }
        restoreBackgrounds();
        if (m_effect) { m_effect->clearBlend(); m_effect->setEnabled(false); }
        m_t = 0.0;
        m_bar->update();
        return;
    }
    // Até a transição acabar, quem pinta é a mistura das camadas.
    m_effect->setBlend(captureLayers(), m_t);
    m_effect->setEnabled(true);
    animateTo(0.0);
}

void RelevoController::rest()
{
    if (!Relevo::enabled() || m_resting) return;
    // Menu ou janelinha aberta (fonte, tamanho, Arquivo): a barra espera acordada.
    if (mouseOver() || QApplication::activePopupWidget()) { m_restTimer->start(); return; }
    if (canRest && !canRest()) { m_restTimer->start(); return; }

    if (!ensureEffect()) { m_restTimer->start(); return; }
    m_resting = true;
    if (!m_bar->isVisible()) {
        // Escondida (arranque, RightBar desligada): dorme direto, sem transição.
        if (m_anim) { QVariantAnimation* old = m_anim; m_anim.clear(); old->stop(); old->deleteLater(); }
        m_t = 1.0;
        m_effect->clearBlend();
        m_effect->setEnabled(true);
        return;
    }
    m_effect->setBlend(captureLayers(), m_t);
    m_effect->setEnabled(true);
    animateTo(1.0);
}

// Sem caixa: a barra e as faixas de dentro param de pintar o fundo do QSS.
// Chamado a cada pintura dormindo, porque um repolish pode religar o fundo.
void RelevoController::stripBackgrounds()
{
    QList<QWidget*> all = m_bar->findChildren<QWidget*>();
    all.prepend(m_bar);
    for (QWidget* w : std::as_const(all)) {
        if (qobject_cast<QAbstractButton*>(w)) continue;
        if (!w->testAttribute(Qt::WA_StyledBackground)) continue;
        w->setAttribute(Qt::WA_StyledBackground, false);
        m_stripped << w;
    }
}

void RelevoController::restoreBackgrounds()
{
    for (const QPointer<QWidget>& w : std::as_const(m_stripped))
        if (w) w->setAttribute(Qt::WA_StyledBackground, true);
    m_stripped.clear();
}

void RelevoController::scheduleRepaint()
{
    if (m_resting) m_repaintTimer->start();
}

bool RelevoController::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::Enter:
        if (watched == m_bar && Relevo::enabled()) wake();
        break;
    case QEvent::Leave:
        if (watched == m_bar && Relevo::enabled()) m_restTimer->start();
        break;
    case QEvent::ChildAdded:
        if (auto* ce = static_cast<QChildEvent*>(event); ce->child() && ce->child()->isWidgetType())
            watch(ce->child());
        scheduleRepaint();
        break;
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::Move:
    case QEvent::Resize:
        scheduleRepaint();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}
