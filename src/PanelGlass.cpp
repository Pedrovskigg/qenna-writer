#include "PanelGlass.h"

#include "Theme.h"

#include <QEvent>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QTimer>
#include <QWidget>

namespace PanelGlass {

namespace {

// O desfoque trabalha numa cópia a 1/4 da janela: fica ~16x mais barato e a
// ampliação suave na hora de pintar ainda soma maciez ao vidro.
constexpr qreal kScale = 0.25;

} // namespace

// Três passadas de média móvel (horizontal + vertical) aproximam um gaussiano.
void boxBlur(QImage& img, int radius)
{
    if (radius < 1 || img.isNull()) return;
    img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = img.width(), h = img.height();
    QList<quint32> line(qMax(w, h));
    auto pass = [&](bool horizontal) {
        const int n = horizontal ? w : h, count = horizontal ? h : w;
        for (int k = 0; k < count; ++k) {
            auto px = [&](int i) -> quint32& {
                return horizontal ? reinterpret_cast<quint32*>(img.scanLine(k))[i]
                                  : reinterpret_cast<quint32*>(img.scanLine(i))[k];
            };
            for (int i = 0; i < n; ++i) line[i] = px(i);
            int sa = 0, sr = 0, sg = 0, sb = 0;
            auto add = [&](quint32 c, int s) {
                sa += s * int(qAlpha(c)); sr += s * int(qRed(c)); sg += s * int(qGreen(c)); sb += s * int(qBlue(c));
            };
            const int win = radius * 2 + 1;
            for (int i = -radius; i <= radius; ++i) add(line[qBound(0, i, n - 1)], 1);
            for (int i = 0; i < n; ++i) {
                px(i) = qRgba(sr / win, sg / win, sb / win, sa / win);
                add(line[qBound(0, i - radius, n - 1)], -1);
                add(line[qBound(0, i + radius + 1, n - 1)], 1);
            }
        }
    };
    for (int i = 0; i < 3; ++i) { pass(true); pass(false); }
}

namespace {

class Backdrop : public QObject {
public:
    static Backdrop* instance() { static Backdrop* b = new Backdrop; return b; }

    QPointer<QWidget> window, background;
    QRect pageRect;
    QColor pageFill;
    QImage small;            // fundo + folha, desfocados, a kScale
    QList<QPointer<QWidget>> panes;

    bool active() const { return Theme::panelBlur() > 0 && Theme::panelOpacity() < 100; }

    void schedule() { m_timer.start(); }

    void rebuild()
    {
        small = QImage();
        if (window && active()) {
            const QSize full = window->size();
            QImage img(QSize(qMax(1, qRound(full.width() * kScale)), qMax(1, qRound(full.height() * kScale))),
                       QImage::Format_ARGB32_Premultiplied);
            img.fill(Theme::toColor(Theme::appBackground()));
            {
                QPainter p(&img);
                p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                p.scale(kScale, kScale);
                // Só o próprio fundo, sem filhos: os painéis não podem entrar
                // no desfoque que vai aparecer atrás deles mesmos.
                if (background && background->isVisible())
                    background->render(&p, background->mapTo(window, QPoint(0, 0)), QRegion(),
                                       QWidget::DrawWindowBackground);
                if (pageRect.isValid() && pageFill.isValid()) p.fillRect(pageRect, pageFill);
            }
            boxBlur(img, qMax(1, qRound(Theme::panelBlur() * kScale)));
            small = img;
        }
        for (const auto& pane : std::as_const(panes)) {
            if (!pane) continue;
            pane->setVisible(!small.isNull() && pane->property("panelVisible").toBool());
            pane->update();
        }
    }

private:
    Backdrop()
    {
        m_timer.setSingleShot(true);
        m_timer.setInterval(40);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this]() { rebuild(); });
        QObject::connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, [this]() { schedule(); });
    }
    QTimer m_timer;
};

// A placa: irmã do painel, logo abaixo dele, pintando o pedaço desfocado.
class Pane : public QWidget {
public:
    explicit Pane(QWidget* panel)
        : QWidget(panel->parentWidget()), m_panel(panel)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setFocusPolicy(Qt::NoFocus);
        panel->installEventFilter(this);
        sync();
    }

    void sync()
    {
        if (!m_panel) return;
        setGeometry(m_panel->geometry());
        setProperty("panelVisible", m_panel->isVisible());
        setVisible(m_panel->isVisible() && !Backdrop::instance()->small.isNull());
        stackUnder(m_panel);
    }

protected:
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (o == m_panel) {
            switch (e->type()) {
            case QEvent::Move: case QEvent::Resize: case QEvent::Show: case QEvent::Hide:
            case QEvent::ZOrderChange:
                sync();
                break;
            case QEvent::ParentChange:
                setParent(m_panel->parentWidget());
                sync();
                break;
            default: break;
            }
        }
        return false;
    }

    void paintEvent(QPaintEvent*) override
    {
        const auto* b = Backdrop::instance();
        if (b->small.isNull() || !b->window) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const qreal r = Theme::panelRadius();
        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
        p.setClipPath(clip);
        const QPoint at = mapTo(b->window, QPoint(0, 0));
        const QRectF src(at.x() * kScale, at.y() * kScale, width() * kScale, height() * kScale);
        p.drawImage(QRectF(rect()), b->small, src);
    }

private:
    QPointer<QWidget> m_panel;
};

} // namespace

void setWindow(QWidget* window, QWidget* background)
{
    auto* b = Backdrop::instance();
    b->window = window;
    b->background = background;
    b->schedule();
}

void setPage(const QRect& rectInWindow, const QColor& fill)
{
    auto* b = Backdrop::instance();
    if (b->pageRect == rectInWindow && b->pageFill == fill) return;
    b->pageRect = rectInWindow;
    b->pageFill = fill;
    b->schedule();
}

void attach(QWidget* panel)
{
    if (!panel || panel->isWindow() || !panel->parentWidget()) return;
    auto* pane = new Pane(panel);
    Backdrop::instance()->panes.append(pane);
    QObject::connect(panel, &QObject::destroyed, pane, &QObject::deleteLater);
}

void invalidate()
{
    Backdrop::instance()->schedule();
}

} // namespace PanelGlass
