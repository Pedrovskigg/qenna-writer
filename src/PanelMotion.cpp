#include "PanelMotion.h"

#include <QApplication>
#include <QEasingCurve>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QMenu>
#include <QPainter>
#include <QSettings>
#include <QTimer>
#include <QVariantAnimation>
#include <algorithm>
#include <cmath>

namespace {

constexpr int kOpenMs = 260;
constexpr int kCloseMs = 170;
constexpr int kOpenShift = 18;
constexpr int kCloseShift = 14;
constexpr int kRowStagger = 22;
constexpr int kRowMs = 220;
constexpr int kMaxRows = 14;
constexpr int kSwapMs = 120;
constexpr int kPopMs = 150;

int g_enabled = -1;   // -1 = ainda não leu das configurações
std::function<bool()> g_barOnRight;

qreal outCubic(qreal t) { t = std::clamp(t, 0.0, 1.0); return 1.0 - std::pow(1.0 - t, 3.0); }

// Retrato do painel que some (fechar) ou do conteúdo velho (troca): pinta o
// pixmap recortado pela parte ainda "fora da barra", com opacidade.
class Ghost : public QWidget {
public:
    Ghost(QWidget* parent, const QPixmap& pm) : QWidget(parent), m_pm(pm) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
    }
    qreal reveal = 1.0;       // fração visível, a partir da borda da barra
    bool fromRight = false;
    qreal opacity = 1.0;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setOpacity(opacity);
        const int w = int(std::round(width() * std::clamp(reveal, 0.0, 1.0)));
        if (w <= 0) return;
        p.setClipRect(fromRight ? QRect(width() - w, 0, w, height()) : QRect(0, 0, w, height()));
        p.drawPixmap(0, 0, m_pm);
    }

private:
    QPixmap m_pm;
};

class DrawerMotion : public QObject {
public:
    DrawerMotion(QWidget* panel, std::function<void()> onOpening)
        : QObject(panel), m_panel(panel), m_onOpening(std::move(onOpening)) {
        panel->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* obj, QEvent* e) override {
        if (obj != m_panel) return false;
        if (e->type() == QEvent::Show && !e->spontaneous()) {
            if (PanelMotion::enabled()) startOpen();
            else if (m_onOpening) m_onOpening();
        } else if (e->type() == QEvent::Hide && !e->spontaneous()) {
            finishOpen();
            if (PanelMotion::enabled()) startClose();
        }
        return false;
    }

private:
    void applyOpen(qreal t) {
        if (!m_panel) return;
        // Quem posiciona o painel (positionSidePanels) pode movê-lo no meio da
        // animação: a nova posição vira a "casa".
        if (m_panel->pos() != m_lastSet) m_home = m_panel->pos();
        const int dir = m_right ? 1 : -1;
        const QPoint p = m_home + QPoint(int(std::round(dir * kOpenShift * (1.0 - t))), 0);
        m_panel->move(p);
        m_lastSet = p;
        const int w = m_panel->width(), h = m_panel->height();
        const int shown = std::clamp(int(std::round(w * std::clamp(t, 0.0, 1.0))), 1, w);
        m_panel->setMask(QRegion(m_right ? QRect(w - shown, 0, shown, h) : QRect(0, 0, shown, h)));
    }

    void startOpen() {
        finishOpen();
        m_right = PanelMotion::barOnRight();
        m_home = m_panel->pos();
        m_lastSet = m_home;
        applyOpen(0.0);
        auto* anim = new QVariantAnimation(this);
        anim->setDuration(kOpenMs);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        QEasingCurve curve(QEasingCurve::OutBack);
        curve.setOvershoot(0.7);
        anim->setEasingCurve(curve);
        connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { applyOpen(v.toReal()); });
        connect(anim, &QVariantAnimation::finished, this, [this]() { finishOpen(); });
        m_open = anim;
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        // As linhas já existem (a lista foi montada antes do show); a posição
        // final chega no mesmo ciclo — daí o tiro zero.
        if (m_onOpening) QTimer::singleShot(0, this, [this]() { if (m_panel && m_panel->isVisible()) m_onOpening(); });
    }

    void finishOpen() {
        if (m_open) {
            QVariantAnimation* a = m_open;
            m_open.clear();
            a->stop();   // DeleteWhenStopped: some sozinha
        }
        if (!m_panel) return;
        if (!m_panel->mask().isEmpty()) m_panel->clearMask();
        if (m_panel->pos() == m_lastSet && m_lastSet != m_home) m_panel->move(m_home);
        m_lastSet = m_panel->pos();
    }

    void startClose() {
        QWidget* parent = m_panel ? m_panel->parentWidget() : nullptr;
        if (!parent || !parent->isVisible() || m_panel->width() <= 0) return;
        const QPixmap pm = m_panel->grab();
        if (pm.isNull()) return;
        const bool right = PanelMotion::barOnRight();
        auto* g = new Ghost(parent, pm);
        g->fromRight = right;
        g->setGeometry(m_panel->geometry());
        g->show();
        g->raise();
        const QPoint home = g->pos();
        auto* anim = new QVariantAnimation(g);
        anim->setDuration(kCloseMs);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::InCubic);
        connect(anim, &QVariantAnimation::valueChanged, g, [g, home, right](const QVariant& v) {
            const qreal t = v.toReal();
            g->move(home + QPoint(int(std::round((right ? 1 : -1) * kCloseShift * t)), 0));
            g->reveal = 1.0 - t;
            g->opacity = 1.0 - 0.4 * t;
            g->update();
        });
        connect(anim, &QVariantAnimation::finished, g, &QObject::deleteLater);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    QPointer<QWidget> m_panel;
    std::function<void()> m_onOpening;
    QPointer<QVariantAnimation> m_open;
    QPoint m_home, m_lastSet;
    bool m_right = false;
};

// Fade de janela solta (menu): começa transparente no Show.
class MenuFade : public QObject {
public:
    explicit MenuFade(QMenu* menu) : QObject(menu) { menu->installEventFilter(this); }

protected:
    bool eventFilter(QObject* obj, QEvent* e) override {
        auto* w = qobject_cast<QWidget*>(obj);
        if (w && e->type() == QEvent::Show) {
            w->setWindowOpacity(0.0);
            auto* anim = new QVariantAnimation(w);
            anim->setDuration(130);
            anim->setStartValue(0.0);
            anim->setEndValue(1.0);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            QPointer<QWidget> pw(w);
            connect(anim, &QVariantAnimation::valueChanged, w, [pw](const QVariant& v) {
                if (pw) pw->setWindowOpacity(v.toReal());
            });
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        }
        return false;
    }
};

}  // namespace

namespace PanelMotion {

bool enabled() {
    if (g_enabled < 0) g_enabled = QSettings().value(QStringLiteral("ui/animations"), true).toBool() ? 1 : 0;
    return g_enabled == 1;
}

void setEnabled(bool on) {
    g_enabled = on ? 1 : 0;
    QSettings().setValue(QStringLiteral("ui/animations"), on);
}

void setBarOnRightProvider(std::function<bool()> provider) { g_barOnRight = std::move(provider); }
bool barOnRight() { return g_barOnRight ? g_barOnRight() : false; }

void installDrawer(QWidget* panel, std::function<void()> onOpening) {
    if (panel) new DrawerMotion(panel, std::move(onOpening));
}

qreal cascadeProgress(int order, qint64 elapsedMs, int delayMs) {
    const int start = delayMs + std::min(order, kMaxRows - 1) * kRowStagger;
    return outCubic((elapsedMs - start) / qreal(kRowMs));
}

int cascadeTotalMs(int delayMs) { return delayMs + (kMaxRows - 1) * kRowStagger + kRowMs; }

void cascade(const QList<QWidget*>& rows, int delayMs) {
    if (!enabled() || rows.isEmpty()) return;
    struct Row { QPointer<QWidget> w; QPoint home, last; QPointer<QGraphicsOpacityEffect> eff; };
    auto items = std::make_shared<QList<Row>>();
    for (QWidget* w : rows) {
        if (!w || !w->isVisible() || w->graphicsEffect()) continue;
        if (items->size() >= kMaxRows) break;
        auto* eff = new QGraphicsOpacityEffect(w);
        eff->setOpacity(0.0);
        w->setGraphicsEffect(eff);
        items->append(Row{ w, w->pos(), w->pos(), eff });
    }
    if (items->isEmpty()) return;
    const int dir = barOnRight() ? 1 : -1;
    auto* anim = new QVariantAnimation(qApp);
    anim->setDuration(cascadeTotalMs(delayMs));
    anim->setStartValue(0);
    anim->setEndValue(cascadeTotalMs(delayMs));
    QObject::connect(anim, &QVariantAnimation::valueChanged, anim, [items, delayMs, dir](const QVariant& v) {
        const int ms = v.toInt();
        for (int i = 0; i < items->size(); ++i) {
            Row& r = (*items)[i];
            if (!r.w || !r.eff) continue;
            const qreal e = cascadeProgress(i, ms, delayMs);
            if (r.w->pos() != r.last) r.home = r.w->pos();
            const QPoint p = r.home + QPoint(int(std::round(dir * kCascadeShift * (1.0 - e))), 0);
            r.w->move(p);
            r.last = p;
            r.eff->setOpacity(e);
        }
    });
    QObject::connect(anim, &QVariantAnimation::finished, anim, [items]() {
        for (Row& r : *items) {
            if (!r.w) continue;
            if (r.w->graphicsEffect() == r.eff) r.w->setGraphicsEffect(nullptr);
            if (r.w->pos() == r.last) r.w->move(r.home);
        }
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void swapOut(QWidget* area) {
    if (!enabled() || !area || !area->isVisible() || area->width() <= 0) return;
    const QPixmap pm = area->grab();
    if (pm.isNull()) return;
    auto* g = new Ghost(area, pm);
    g->setGeometry(area->rect());
    g->show();
    g->raise();
    const int dir = barOnRight() ? 1 : -1;
    auto* anim = new QVariantAnimation(g);
    anim->setDuration(kSwapMs);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::InCubic);
    QObject::connect(anim, &QVariantAnimation::valueChanged, g, [g, dir](const QVariant& v) {
        const qreal t = v.toReal();
        g->move(int(std::round(dir * 12 * t)), 0);
        g->opacity = 1.0 - t;
        g->update();
    });
    QObject::connect(anim, &QVariantAnimation::finished, g, &QObject::deleteLater);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void popIn(QWidget* window) {
    if (!enabled() || !window) return;
    const QPoint home = window->pos();
    const int dir = barOnRight() ? 1 : -1;
    window->setWindowOpacity(0.0);
    window->move(home + QPoint(dir * 8, 0));
    QPointer<QWidget> pw(window);
    auto* anim = new QVariantAnimation(window);
    anim->setDuration(kPopMs);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    auto last = std::make_shared<QPoint>(home + QPoint(dir * 8, 0));
    auto base = std::make_shared<QPoint>(home);
    QObject::connect(anim, &QVariantAnimation::valueChanged, window, [pw, dir, last, base](const QVariant& v) {
        if (!pw) return;
        const qreal t = v.toReal();
        if (pw->pos() != *last) *base = pw->pos();   // alguém reposicionou: segue a nova casa
        const QPoint p = *base + QPoint(int(std::round(dir * 8 * (1.0 - t))), 0);
        pw->move(p);
        *last = p;
        pw->setWindowOpacity(t);
    });
    QObject::connect(anim, &QVariantAnimation::finished, window, [pw]() { if (pw) pw->setWindowOpacity(1.0); });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void animateMenu(QMenu* menu) {
    if (enabled() && menu) new MenuFade(menu);
}

}
