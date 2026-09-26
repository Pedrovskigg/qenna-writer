#include "PanelMotion.h"

#include <QApplication>
#include <QEasingCurve>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QAbstractItemView>
#include <QMainWindow>
#include <QMenu>
#include <QPainter>
#include <QScrollArea>
#include <QLayout>
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
constexpr int kWindowShift = 10;
constexpr int kRowStagger = 22;
constexpr int kRowMs = 220;
constexpr int kMaxRows = 14;
constexpr int kSwapMs = 120;
constexpr int kPopMs = 150;

int g_enabled = -1;   // -1 = ainda não leu das configurações
std::function<bool()> g_barOnRight;

qreal outCubic(qreal t) { t = std::clamp(t, 0.0, 1.0); return 1.0 - std::pow(1.0 - t, 3.0); }

// Parte visível de `r` quando `frac` dele já saiu da borda `edge`.
QRect revealRect(const QRect& r, Qt::Edge edge, qreal frac) {
    frac = std::clamp(frac, 0.0, 1.0);
    const int w = int(std::round(r.width() * frac)), h = int(std::round(r.height() * frac));
    switch (edge) {
    case Qt::RightEdge:  return QRect(r.right() - w + 1, r.top(), w, r.height());
    case Qt::TopEdge:    return QRect(r.left(), r.top(), r.width(), h);
    case Qt::BottomEdge: return QRect(r.left(), r.bottom() - h + 1, r.width(), h);
    default:             return QRect(r.left(), r.top(), w, r.height());
    }
}

// Deslocamento de quem ainda está "dentro" da borda `edge`, em `amount` px.
QPoint edgeOffset(Qt::Edge edge, qreal amount) {
    const int a = int(std::round(amount));
    switch (edge) {
    case Qt::RightEdge:  return QPoint(a, 0);
    case Qt::TopEdge:    return QPoint(0, -a);
    case Qt::BottomEdge: return QPoint(0, a);
    default:             return QPoint(-a, 0);
    }
}

// Retrato do painel que some (fechar) ou do conteúdo velho (troca): pinta o
// pixmap recortado pela parte ainda "fora da barra", com opacidade.
class Ghost : public QWidget {
public:
    Ghost(QWidget* parent, const QPixmap& pm) : QWidget(parent), m_pm(pm) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
    }
    qreal reveal = 1.0;       // fração visível, a partir da borda de origem
    Qt::Edge edge = Qt::LeftEdge;
    qreal opacity = 1.0;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setOpacity(opacity);
        const QRect clip = revealRect(rect(), edge, reveal);
        if (clip.isEmpty()) return;
        p.setClipRect(clip);
        p.drawPixmap(0, 0, m_pm);
    }

private:
    QPixmap m_pm;
};

class DrawerMotion : public QObject {
public:
    DrawerMotion(QWidget* panel, std::function<Qt::Edge()> edge, std::function<void()> onOpening, bool popover = false)
        : QObject(panel), m_panel(panel), m_edgeFn(std::move(edge)), m_onOpening(std::move(onOpening)), m_popover(popover) {
        panel->setProperty("qennaMotionHandled", true);
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
            if (PanelMotion::enabled() && !m_popover) startClose();
        }
        return false;
    }

private:
    void applyOpen(qreal t) {
        if (!m_panel) return;
        // Quem posiciona o painel (positionSidePanels) pode movê-lo no meio da
        // animação: a nova posição vira a "casa".
        if (m_panel->pos() != m_lastSet) m_home = m_panel->pos();
        const bool window = m_panel->isWindow();
        const int shift = m_popover ? 7 : (window ? kWindowShift : kOpenShift);
        const QPoint p = m_home + edgeOffset(m_edge, shift * (1.0 - t));
        m_panel->move(p);
        m_lastSet = p;
        if (window) {
            // Janela solta (Som imersivo, Lembretes): máscara numa janela de
            // verdade vira região do Windows e corta os cantos e a sombra até
            // o fim — o painel "nasce torto". Aqui é fade + deslize.
            m_panel->setWindowOpacity(std::clamp(t, 0.0, 1.0));
            return;
        }
        QRect shown = revealRect(m_panel->rect(), m_edge, std::max(t, 0.01));
        if (shown.isEmpty()) shown = QRect(0, 0, 1, 1);
        m_panel->setMask(QRegion(shown));
    }

    void startOpen() {
        finishOpen();
        // Escondido até começar: o painel ainda vai rodar o próprio showEvent
        // (RefMenu confere se cabe na tela, a Mira se posiciona). Empurrar ele
        // ANTES disso fazia o RefMenu "corrigir" a posição e salvar a
        // corrigida — a cada abertura ele andava pro meio da tela.
        if (m_panel->isWindow()) m_panel->setWindowOpacity(0.0);
        else m_panel->setMask(QRegion(0, 0, 1, 1));
        const int gen = ++m_gen;
        QTimer::singleShot(0, this, [this, gen]() {
            if (gen != m_gen || !m_panel || !m_panel->isVisible()) return;
            beginOpen();
        });
    }

    void beginOpen() {
        m_edge = m_edgeFn ? m_edgeFn() : Qt::LeftEdge;
        m_home = m_panel->pos();
        m_lastSet = m_home;
        applyOpen(0.0);
        auto* anim = new QVariantAnimation(this);
        anim->setDuration(m_popover ? 150 : kOpenMs);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        QEasingCurve curve(m_popover ? QEasingCurve::OutCubic : QEasingCurve::OutBack);
        if (!m_popover) curve.setOvershoot(0.7);
        anim->setEasingCurve(curve);
        connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { applyOpen(v.toReal()); });
        connect(anim, &QVariantAnimation::finished, this, [this]() { finishOpen(); });
        m_open = anim;
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        if (m_onOpening) m_onOpening();
    }

    void finishOpen() {
        ++m_gen;   // abertura adiada que ainda não começou: cancela
        if (m_open) {
            QVariantAnimation* a = m_open;
            m_open.clear();
            a->stop();   // DeleteWhenStopped: some sozinha
        }
        if (!m_panel) return;
        if (m_panel->isWindow()) m_panel->setWindowOpacity(1.0);
        else if (!m_panel->mask().isEmpty()) m_panel->clearMask();
        if (m_panel->pos() == m_lastSet && m_lastSet != m_home) m_panel->move(m_home);
        m_lastSet = m_panel->pos();
    }

    void startClose() {
        if (!m_panel || m_panel->width() <= 0) return;
        // Janela solta (Som imersivo): o fantasma vai na janela principal, no
        // mesmo lugar da tela.
        QWidget* parent = m_panel->isWindow() ? (m_panel->parentWidget() ? m_panel->parentWidget()->window() : nullptr)
                                              : m_panel->parentWidget();
        if (!parent || !parent->isVisible()) return;
        const QPixmap pm = m_panel->grab();
        if (pm.isNull()) return;
        const Qt::Edge edge = m_edgeFn ? m_edgeFn() : Qt::LeftEdge;
        auto* g = new Ghost(parent, pm);
        g->edge = edge;
        g->setGeometry(m_panel->isWindow() ? QRect(parent->mapFromGlobal(m_panel->mapToGlobal(QPoint(0, 0))), m_panel->size())
                                           : m_panel->geometry());
        g->show();
        g->raise();
        const QPoint home = g->pos();
        auto* anim = new QVariantAnimation(g);
        anim->setDuration(kCloseMs);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::InCubic);
        connect(anim, &QVariantAnimation::valueChanged, g, [g, home, edge](const QVariant& v) {
            const qreal t = v.toReal();
            g->move(home + edgeOffset(edge, kCloseShift * t));
            g->reveal = 1.0 - t;
            g->opacity = 1.0 - 0.4 * t;
            g->update();
        });
        connect(anim, &QVariantAnimation::finished, g, &QObject::deleteLater);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    QPointer<QWidget> m_panel;
    std::function<Qt::Edge()> m_edgeFn;
    std::function<void()> m_onOpening;
    QPointer<QVariantAnimation> m_open;
    QPoint m_home, m_lastSet;
    Qt::Edge m_edge = Qt::LeftEdge;
    int m_gen = 0;
    bool m_popover = false;
};

// Fade de janela solta (menu): começa transparente no Show.
class MenuFade : public QObject {
public:
    explicit MenuFade(QMenu* menu) : QObject(menu) {
        menu->setProperty("qennaMotionHandled", true);
        menu->installEventFilter(this);
    }

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

// Filtro da aplicação: anima o Show de qualquer janela de nível superior.
class WindowMotion : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* obj, QEvent* e) override {
        if (e->type() != QEvent::Show || e->spontaneous()) return false;
        auto* w = qobject_cast<QWidget*>(obj);
        if (!w || !w->isWindow() || !PanelMotion::enabled()) return false;
        const Qt::WindowType t = w->windowType();
        if (t == Qt::ToolTip || t == Qt::SplashScreen || t == Qt::Desktop || t == Qt::Drawer) return false;
        if (qobject_cast<QMainWindow*>(w) || w->testAttribute(Qt::WA_DontShowOnScreen)) return false;
        if (w->property("qennaMotionHandled").toBool() || w->property("qennaNoMotion").toBool()) return false;
        // Já têm o próprio fade/animação.
        if (w->inherits("ColorPopover") || w->inherits("MainMenuDialog")) return false;
        // Listas de combo/autocompletar e menus: só fade (mexer a posição
        // desalinha com o campo/submenu).
        const bool fadeOnly = qobject_cast<QMenu*>(w) || qobject_cast<QAbstractItemView*>(w)
                              || w->inherits("QComboBoxPrivateContainer");
        const bool dialog = (t == Qt::Dialog || t == Qt::Window || t == Qt::Sheet);
        start(w, fadeOnly ? 0 : (dialog ? 28 : 16), fadeOnly ? 200 : (dialog ? 300 : 240));
        return false;
    }

private:
    void start(QWidget* w, int shift, int ms) {
        // Escondida até o próprio showEvent posicionar (diálogo centraliza,
        // popup se ancora); o movimento começa no giro seguinte.
        w->setWindowOpacity(0.0);
        QPointer<QWidget> pw(w);
        QTimer::singleShot(0, w, [pw, shift, ms]() {
            if (!pw || !pw->isVisible()) { if (pw) pw->setWindowOpacity(1.0); return; }
            auto home = std::make_shared<QPoint>(pw->pos());
            auto last = std::make_shared<QPoint>(*home + QPoint(0, shift));
            if (shift) pw->move(*last);
            auto* a = new QVariantAnimation(pw);
            a->setDuration(ms);
            a->setStartValue(0.0);
            a->setEndValue(1.0);
            a->setEasingCurve(QEasingCurve::OutCubic);
            QObject::connect(a, &QVariantAnimation::valueChanged, pw, [pw, shift, home, last](const QVariant& v) {
                if (!pw) return;
                const qreal k = v.toReal();
                if (shift) {
                    if (pw->pos() != *last) *home = pw->pos() - QPoint(0, int(std::round(shift * (1.0 - k))));
                    const QPoint p = *home + QPoint(0, int(std::round(shift * (1.0 - k))));
                    pw->move(p);
                    *last = p;
                }
                pw->setWindowOpacity(k);
            });
            QObject::connect(a, &QVariantAnimation::finished, pw, [pw, home, last, shift]() {
                if (!pw) return;
                pw->setWindowOpacity(1.0);
                if (shift && pw->pos() == *last) pw->move(*home);
            });
            a->start(QAbstractAnimation::DeleteWhenStopped);
        });
    }
};

}  // namespace

namespace PanelMotion {

void installGlobalWindowMotion() {
    static WindowMotion* filter = nullptr;
    if (filter || !qApp) return;
    filter = new WindowMotion(qApp);
    qApp->installEventFilter(filter);
}

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
    installFromEdge(panel, []() { return barOnRight() ? Qt::RightEdge : Qt::LeftEdge; }, std::move(onOpening));
}

void installFromEdge(QWidget* panel, std::function<Qt::Edge()> edge, std::function<void()> onOpening) {
    if (panel) new DrawerMotion(panel, std::move(edge), std::move(onOpening));
}

void installPopover(QWidget* w, Qt::Edge from) {
    if (w) new DrawerMotion(w, [from]() { return from; }, nullptr, true);
}

void autoCascade(QWidget* panel, Qt::Edge from, int delayMs) {
    if (!enabled() || !panel) return;
    // A lista principal é o maior QScrollArea à vista; as linhas são os
    // widgets do layout dele. Sem lista, entram os blocos do próprio painel.
    QScrollArea* best = nullptr;
    int bestArea = 0;
    for (QScrollArea* sa : panel->findChildren<QScrollArea*>()) {
        if (!sa->isVisible() || !sa->widget()) continue;
        const int area = sa->viewport()->width() * sa->viewport()->height();
        if (area > bestArea) { best = sa; bestArea = area; }
    }
    QList<QWidget*> rows;
    auto take = [&rows](QLayout* lay) {
        if (!lay) return;
        for (int i = 0; i < lay->count(); ++i)
            if (QWidget* w = lay->itemAt(i)->widget()) if (w->isVisible()) rows << w;
    };
    if (best) take(best->widget()->layout());
    if (rows.size() < 2) { rows.clear(); take(panel->layout()); }
    cascade(rows, delayMs, from);
}

qreal cascadeProgress(int order, qint64 elapsedMs, int delayMs) {
    const int start = delayMs + std::min(order, kMaxRows - 1) * kRowStagger;
    return outCubic((elapsedMs - start) / qreal(kRowMs));
}

int cascadeTotalMs(int delayMs) { return delayMs + (kMaxRows - 1) * kRowStagger + kRowMs; }

void cascade(const QList<QWidget*>& rows, int delayMs) {
    cascade(rows, delayMs, barOnRight() ? Qt::RightEdge : Qt::LeftEdge);
}

void cascade(const QList<QWidget*>& rows, int delayMs, Qt::Edge from) {
    if (!enabled() || rows.isEmpty()) return;
    struct Row { QPointer<QWidget> w; QPoint home, last; QPointer<QGraphicsOpacityEffect> eff; };
    auto items = std::make_shared<QList<Row>>();
    auto underEffect = [](QWidget* w) {
        // Efeito dentro de efeito (a sombra do Som imersivo, dos Lembretes)
        // o Qt desenha torto até acabar: essas linhas entram sem cascata.
        for (QWidget* p = w->parentWidget(); p; p = p->isWindow() ? nullptr : p->parentWidget())
            if (p->graphicsEffect()) return true;
        return false;
    };
    for (QWidget* w : rows) {
        if (!w || !w->isVisible() || w->graphicsEffect() || underEffect(w)) continue;
        if (items->size() >= kMaxRows) break;
        auto* eff = new QGraphicsOpacityEffect(w);
        eff->setOpacity(0.0);
        w->setGraphicsEffect(eff);
        items->append(Row{ w, w->pos(), w->pos(), eff });
    }
    if (items->isEmpty()) return;
    auto* anim = new QVariantAnimation(qApp);
    anim->setDuration(cascadeTotalMs(delayMs));
    anim->setStartValue(0);
    anim->setEndValue(cascadeTotalMs(delayMs));
    QObject::connect(anim, &QVariantAnimation::valueChanged, anim, [items, delayMs, from](const QVariant& v) {
        const int ms = v.toInt();
        for (int i = 0; i < items->size(); ++i) {
            Row& r = (*items)[i];
            if (!r.w || !r.eff) continue;
            const qreal e = cascadeProgress(i, ms, delayMs);
            if (r.w->pos() != r.last) r.home = r.w->pos();
            const QPoint p = r.home + edgeOffset(from, kCascadeShift * (1.0 - e));
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

void morph(QWidget* panel, const std::function<void()>& change) {
    const MorphStart start = beginMorph(panel);
    change();
    endMorph(start);
}

MorphStart beginMorph(QWidget* panel) {
    MorphStart s;
    if (!panel) return s;
    // Uma mudança no meio da outra (duplo clique): a anterior termina na hora.
    if (auto* running = panel->property("qennaMorphAnim").value<QObject*>())
        static_cast<QVariantAnimation*>(running)->stop();   // limpeza no stateChanged, na hora
    if (!enabled() || !panel->parentWidget() || !panel->isVisible() || panel->height() <= 0) return s;
    s.panel = panel;
    s.before = panel->geometry();
    s.pm = panel->grab();
    return s;
}

void endMorph(const MorphStart& start) {
    QWidget* panel = start.panel;
    QWidget* parent = panel ? panel->parentWidget() : nullptr;
    if (!panel || !parent || start.pm.isNull() || !panel->isVisible()) return;
    const QRect before = start.before;
    const QPixmap& pm = start.pm;
    const QRect after = panel->geometry();
    if (after.height() == before.height()) return;

    // A "janela" visível tem a altura h, presa no chão (a base do painel). O
    // conteúdo mostrado é o TOPO de quem é mais alto — o painel novo quando
    // cresce, o fantasma do velho quando encolhe —, empurrado pra baixo pelo
    // que ainda falta. Em h = altura do menor, o topo do maior é o menor.
    const bool grow = after.height() > before.height();
    QPointer<Ghost> ghost;
    if (!grow) {
        ghost = new Ghost(parent, pm);
        ghost->edge = Qt::TopEdge;
        ghost->setGeometry(before);
        ghost->show();
        ghost->raise();
    }
    QPointer<QWidget> pw(panel);
    struct State { QPoint home, last; int floor = 0; };
    auto st = std::make_shared<State>();
    st->home = after.topLeft();
    st->last = st->home;
    st->floor = before.bottom();
    const int from = before.height(), to = after.height();

    auto apply = [pw, ghost, st, grow, from, to](qreal t) {
        if (!pw) return;
        if (grow) {
            // Alguém reposicionou (o contador ganhou um dígito): segue a casa nova.
            if (pw->pos() != st->last) st->home = pw->pos();
            const int full = pw->height();
            const int h = std::clamp(int(std::round(from + (full - from) * t)), 1, full + 12);
            const QPoint p = st->home + QPoint(0, full - h);
            pw->move(p);
            st->last = p;
            QRect shown = revealRect(pw->rect(), Qt::TopEdge, std::min(1.0, h / qreal(full)));
            pw->setMask(QRegion(shown.isEmpty() ? QRect(0, 0, 1, 1) : shown));
        } else if (ghost) {
            const int h = std::max(1, int(std::round(from + (to - from) * t)));
            ghost->move(ghost->x(), st->floor - h + 1);
            ghost->reveal = h / qreal(from);
            ghost->update();
        }
    };

    if (!grow) panel->setMask(QRegion(0, 0, 1, 1));   // o novo espera o fantasma afundar até ele
    auto* anim = new QVariantAnimation(panel);
    anim->setDuration(grow ? kOpenMs : 220);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    QEasingCurve curve(grow ? QEasingCurve::OutBack : QEasingCurve::OutCubic);
    if (grow) curve.setOvershoot(0.7);
    anim->setEasingCurve(curve);
    QObject::connect(anim, &QVariantAnimation::valueChanged, panel, [apply](const QVariant& v) { apply(v.toReal()); });
    // Terminou ou foi interrompida: o painel volta pra casa, inteiro. Em
    // stateChanged (síncrono no stop()), não em destroyed — o DeleteWhenStopped
    // apaga depois, e a limpeza atrasada desfaria a próxima mudança.
    QObject::connect(anim, &QAbstractAnimation::stateChanged, panel,
                     [pw, ghost, st, anim](QAbstractAnimation::State now, QAbstractAnimation::State) {
        if (now != QAbstractAnimation::Stopped) return;
        if (ghost) ghost->deleteLater();
        if (!pw) return;
        if (pw->property("qennaMorphAnim").value<QObject*>() == anim) pw->setProperty("qennaMorphAnim", QVariant());
        if (!pw->mask().isEmpty()) pw->clearMask();
        if (pw->pos() == st->last && st->last != st->home) pw->move(st->home);
    });
    panel->setProperty("qennaMorphAnim", QVariant::fromValue<QObject*>(anim));
    apply(0.0);
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
