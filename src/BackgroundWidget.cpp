#include "BackgroundWidget.h"

#include "AnimBackground.h"
#include "BackgroundOverlay.h"
#include "PanelGlass.h"
#include "Theme.h"

#include <QGuiApplication>
#include <QImageReader>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QtMath>

BackgroundWidget::BackgroundWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    // Aceita transparência: deixamos o background do pai aparecer onde a
    // imagem não cobre (modos centralizado / ajustar).
    setAutoFillBackground(false);

    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(33); // 30 quadros por segundo; Configurações pode baixar pra 20 ou 15
    connect(m_timer, &QTimer::timeout, this, &BackgroundWidget::tick);
    // Personalizar, intensidade ou o liga/desliga mudaram: remonta a mesa.
    connect(AnimBg::Notifier::instance(), &AnimBg::Notifier::changed, this, &BackgroundWidget::refreshAnimation);
    // Voltou pro Qenna: o relógio recomeça daqui, sem pular o tempo parado.
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState) { m_clock.restart(); });
}

BackgroundWidget::~BackgroundWidget() = default;

void BackgroundWidget::setBackground(const QString& imagePath, int mode)
{
    const bool pathChanged = (imagePath != m_path);
    const bool modeChanged = (mode != m_mode);
    m_path = imagePath;
    m_mode = mode;

    if (pathChanged) {
        m_source = QPixmap();
        if (!imagePath.isEmpty()) {
            QImageReader reader(imagePath);
            reader.setAutoTransform(true);
            const QImage img = reader.read();
            if (!img.isNull()) {
                m_source = QPixmap::fromImage(img);
            }
        }
        m_scaled = QPixmap();
        m_scaledFor = QSize();
    } else if (modeChanged) {
        m_scaled = QPixmap();
        m_scaledFor = QSize();
    }
    update();
}

void BackgroundWidget::setFillColor(const QColor& color)
{
    if (m_fill == color) return;
    m_fill = color;
    update();
}

void BackgroundWidget::rebuildScaled()
{
    if (m_source.isNull()) {
        m_scaled = QPixmap();
        m_scaledFor = QSize();
        return;
    }
    const QSize target = size();
    if (target.isEmpty()) {
        m_scaled = QPixmap();
        m_scaledFor = QSize();
        return;
    }
    if (m_scaledFor == target && !m_scaled.isNull()) return;

    switch (m_mode) {
    case Theme::BgCenter:
    case Theme::BgTile:
        m_scaled = m_source; // sem escalonar
        break;
    case Theme::BgStretch:
        m_scaled = m_source.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        break;
    case Theme::BgFit:
        m_scaled = m_source.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        break;
    case Theme::BgZoom:
    default:
        m_scaled = m_source.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        break;
    }
    m_scaledFor = target;
}

void BackgroundWidget::paintEvent(QPaintEvent* event)
{
    QPainter p(this);
    // 1) Cor sólida cobrindo (caso a imagem deixe áreas vazias).
    if (m_fill.isValid()) {
        p.fillRect(rect(), m_fill);
    }
    // 2) Imagem em cima.
    paintImage(p);
    // 3) O clima dos temas Animados, nas coordenadas da cena (758 de altura).
    if (m_anim) {
        const double s = sceneScale();
        if (AnimBg::quality() < 100) {
            paintAnimLow(p, event->region(), s);
        } else {
            if (!m_low.isNull()) m_low = QImage();
            p.save();
            p.scale(s, s);
            m_anim->paint(p, s * devicePixelRatioF());
            p.restore();
        }
    }
    // 4) Degradê e grão do tema por cima da mesa (cor ou foto).
    BackgroundOverlay::paint(p, QRectF(rect()), Theme::Manager::instance()->current());
    if (m_anim) paintSheetShadow(p);
}

void BackgroundWidget::paintImage(QPainter& p)
{
    if (m_source.isNull()) return;
    rebuildScaled();
    if (m_scaled.isNull()) return;

    const QSize w = size();
    switch (m_mode) {
    case Theme::BgCenter: {
        const int x = (w.width()  - m_scaled.width())  / 2;
        const int y = (w.height() - m_scaled.height()) / 2;
        p.drawPixmap(x, y, m_scaled);
        break;
    }
    case Theme::BgTile: {
        p.drawTiledPixmap(rect(), m_scaled);
        break;
    }
    case Theme::BgStretch:
        p.drawPixmap(rect(), m_scaled);
        break;
    case Theme::BgFit: {
        const int x = (w.width()  - m_scaled.width())  / 2;
        const int y = (w.height() - m_scaled.height()) / 2;
        p.drawPixmap(x, y, m_scaled);
        break;
    }
    case Theme::BgZoom:
    default: {
        // imagem maior que área — recorta centralizado
        const int x = (w.width()  - m_scaled.width())  / 2;
        const int y = (w.height() - m_scaled.height()) / 2;
        p.drawPixmap(x, y, m_scaled);
        break;
    }
    }
}

// ---------------------------------------------------------------- temas Animados

namespace {

QString configKey(const Theme::MiraTheme& t, const AnimBg::Config& c)
{
    return t.id + QLatin1Char('|') + c.tod + QLatin1Char('|') + c.rain + QLatin1Char('|') + c.extras.join(QLatin1Char(','))
         + QLatin1Char('|') + QString::number(AnimBg::intensity());
}

} // namespace

double BackgroundWidget::sceneScale() const
{
    return qMax(1, height()) / AnimBg::kSceneH;
}

static AnimBg::Zones zonesFor(const QRect& sheet, const QSize& size, double s)
{
    const double W = size.width() / s;
    if (sheet.isEmpty()) return { QRectF(0, 0, W * 0.28, AnimBg::kSceneH), QRectF(W * 0.72, 0, W * 0.28, AnimBg::kSceneH) };
    return { QRectF(0, 0, sheet.left() / s, AnimBg::kSceneH),
             QRectF((sheet.right() + 1) / s, 0, qMax(0.0, (size.width() - sheet.right() - 1) / s), AnimBg::kSceneH) };
}

void BackgroundWidget::refreshAnimation()
{
    const Theme::MiraTheme& t = Theme::Manager::instance()->current();
    if (t.animScene.isEmpty()) {
        const bool had = bool(m_anim);
        m_anim.reset();
        m_animKey.clear();
        updateTimer();
        if (had) update();
        return;
    }
    const AnimBg::Config cfg = AnimBg::config(t);
    const QString key = configKey(t, cfg);
    if (!m_anim || key != m_animKey) {
        const double s = sceneScale();
        if (!m_anim) m_anim = std::make_unique<AnimBg::Stack>();
        m_anim->setup(t, cfg, width() / s, zonesFor(m_sheet, size(), s));
        m_animKey = key;
        m_clock.restart();
    }
    updateTimer();
    update();
}

void BackgroundWidget::setSheetRect(const QRect& r)
{
    if (r == m_sheet) return;
    m_sheet = r;
    if (m_anim) {
        const double s = sceneScale();
        m_anim->resize(width() / s, zonesFor(m_sheet, size(), s));
        update();
    }
}

void BackgroundWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_anim) {
        const double s = sceneScale();
        m_anim->resize(width() / s, zonesFor(m_sheet, size(), s));
    }
}

void BackgroundWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    updateTimer();
}

void BackgroundWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    updateTimer();
}

// Qualidade abaixo de 100%: a cena é desenhada numa imagem menor e esticada
// na tela. Só a parte da mesa que vai ser repintada é redesenhada nela.
void BackgroundWidget::paintAnimLow(QPainter& p, const QRegion& region, double s)
{
    const double q = AnimBg::quality() / 100.0, dpr = devicePixelRatioF();
    const QSize sz(qMax(1, qCeil(width() * dpr * q)), qMax(1, qCeil(height() * dpr * q)));
    if (m_low.size() != sz) {
        m_low = QImage(sz, QImage::Format_ARGB32_Premultiplied);
        m_low.fill(Qt::transparent);
    }
    const double kx = sz.width() / double(qMax(1, width())), ky = sz.height() / double(qMax(1, height()));
    // a região a repintar, na escala da imagem, com folga pro esticar suave não puxar pixel velho da borda
    QRegion clip;
    for (const QRect& r : region)
        clip += QRect(QPoint(qFloor(r.left() * kx) - 2, qFloor(r.top() * ky) - 2),
                      QPoint(qCeil((r.right() + 1) * kx) + 1, qCeil((r.bottom() + 1) * ky) + 1));
    {
        QPainter x(&m_low);
        x.setClipRegion(clip);
        x.setCompositionMode(QPainter::CompositionMode_Source);
        x.fillRect(m_low.rect(), Qt::transparent);
        x.setCompositionMode(QPainter::CompositionMode_SourceOver);
        x.scale(s * kx, s * ky);
        m_anim->paint(x, s * qMin(kx, ky));
    }
    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(QRectF(rect()), m_low);
    p.restore();
}

void BackgroundWidget::updateTimer()
{
    m_timer->setInterval(qRound(1000.0 / AnimBg::fps()));
    const bool run = m_anim && AnimBg::enabled() && isVisible();
    if (run && !m_timer->isActive()) { m_clock.restart(); m_timer->start(); }
    else if (!run && m_timer->isActive()) m_timer->stop();
}

void BackgroundWidget::tick()
{
    if (!m_anim) return;
    QWidget* win = window();
    // Fora de foco ou minimizado: a mesa fica parada no último quadro.
    if (!win || !win->isActiveWindow() || win->isMinimized() || qApp->applicationState() != Qt::ApplicationActive) {
        m_clock.restart();
        return;
    }
    const double dt = m_clock.isValid() ? m_clock.restart() / 1000.0 : 1.0 / 30;
    m_anim->step(dt);
    const QRegion r = animRegion();
    if (!r.isEmpty()) update(r);
}

// A mesa à vista: tudo menos a folha e os painéis opacos por cima. Repintar
// só isso deixa o editor e os painéis quietos — eles não são redesenhados a
// cada quadro.
QRegion BackgroundWidget::animRegion() const
{
    QRegion r(rect());
    if (!m_sheet.isEmpty()) r -= m_sheet;
    QWidget* win = parentWidget();
    if (!win) return r;
    const qint64 maxArea = qint64(width()) * height() * 4 / 10;
    auto sub = [&](QWidget* w, const QPoint& off) {
        if (!w || w == this || w->isWindow() || !w->isVisible()) return;
        const QRect g = w->geometry().translated(off);
        if (qint64(g.width()) * g.height() > maxArea) return; // contêiner transparente grande: não tapa nada
        if (w->testAttribute(Qt::WA_TranslucentBackground)) return;
        r -= g;
    };
    for (QObject* o : win->children()) {
        auto* w = qobject_cast<QWidget*>(o);
        if (!w || w == this || w->isWindow() || !w->isVisible()) continue;
        if (w->objectName() == QLatin1String("editorContainer")) {
            for (QObject* oc : w->children()) {
                auto* cw = qobject_cast<QWidget*>(oc);
                if (cw && cw->objectName() != QLatin1String("editorScroll")) sub(cw, w->geometry().topLeft());
            }
            continue;
        }
        sub(w, QPoint());
    }
    return r;
}

// Sombra da folha pintada pela mesa: uma imagem desfocada feita uma vez por
// tamanho de folha e só carimbada a cada quadro.
void BackgroundWidget::paintSheetShadow(QPainter& p)
{
    if (m_sheet.isEmpty() || !Theme::pageShadowEnabled()) return;
    const QColor col = Theme::toColor(Theme::pageShadowColor());
    if (col.alpha() == 0) return;
    const int r = qMax(2, Theme::pageShadowRadius());
    const int off = Theme::pageShadowOffset();
    const QString key = QStringLiteral("%1x%2|%3|%4").arg(m_sheet.width()).arg(m_sheet.height()).arg(col.name(QColor::HexArgb)).arg(r);
    if (key != m_shadowKey) {
        QImage img(m_sheet.width() + 4 * r, m_sheet.height() + 4 * r, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        {
            QPainter x(&img);
            x.fillRect(QRect(2 * r, 2 * r, m_sheet.width(), m_sheet.height()), col);
        }
        PanelGlass::boxBlur(img, qMax(1, r / 3));
        m_shadow = img;
        m_shadowKey = key;
    }
    p.drawImage(m_sheet.topLeft() + QPoint(-2 * r, -2 * r + off), m_shadow);
}
