#include "SelectionPopup.h"

#include "IconUtils.h"
#include "Theme.h"

#include <QCoreApplication>
#include <QCursor>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QScrollBar>
#include <QSettings>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kIconSize = 18;
constexpr int kButtonSize = 28;
constexpr int kGapAboveSelection = 6;
constexpr int kShadow = 10;      // margem transparente em volta do balão, pra sombra
constexpr int kTabH = 15;        // lingueta embaixo do balão (onde mora o ↔)
constexpr int kTabW = 30;
constexpr int kModeW = 22;
constexpr int kModeH = 14;
constexpr int kViewW = 300;      // largura da janela da fita
constexpr int kEdgeZone = 52;    // pontas que giram no modo "só pelas pontas"
constexpr qreal kThetaMax = 1.08; // ~62°: quanto as pontas curvam
constexpr qreal kRadius = 10.0;

QColor tc(const QString& css) { return Theme::toColor(css); }

QToolButton* makeBtn(QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setObjectName(QStringLiteral("selPopupBtn"));
    b->setIconSize(QSize(kIconSize, kIconSize));
    b->setFixedSize(kButtonSize, kButtonSize);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    b->setAutoRaise(true);
    return b;
}

}

// Os risquinhos embaixo da fita: um por grupo, o do meio da janela aceso.
class SelectionGroupDots : public QWidget
{
public:
    explicit SelectionGroupDots(QWidget *parent) : QWidget(parent)
    {
        setFixedHeight(4);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    void setState(int active, int count)
    {
        if (active == m_active && count == m_count) return;
        m_active = active;
        m_count = count;
        setFixedWidth(qMax(1, count * 14 + (count - 1) * 4));
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        for (int i = 0; i < m_count; ++i) {
            p.setBrush(i == m_active ? tc(Theme::accentDefault()) : tc(Theme::panelBorder()));
            p.drawRoundedRect(QRectF(i * 18, 1, 14, 2), 1, 1);
        }
    }
private:
    int m_active = 0;
    int m_count = 0;
};

// A janela da fita. Os botões de verdade moram numa faixa fora da área
// visível; aqui ela é desenhada fatiada sobre um tambor (pontas curvas e
// apagadas), e o mouse é traduzido de volta pra posição na faixa.
class SelectionCarousel : public QWidget
{
public:
    std::function<void(int, int)> onGroup;

    explicit SelectionCarousel(QWidget *parent) : QWidget(parent)
    {
        setMouseTracking(true);
        setAttribute(Qt::WA_Hover, true);
        m_strip = new QWidget(this);
        m_strip->setObjectName(QStringLiteral("selPopupStrip"));
        m_strip->setAttribute(Qt::WA_TranslucentBackground, true);
        m_tick.setInterval(16);
        QObject::connect(&m_tick, &QTimer::timeout, this, [this]() { step(); });
        m_tipTimer.setSingleShot(true);
        m_tipTimer.setInterval(550);
        QObject::connect(&m_tipTimer, &QTimer::timeout, this, [this]() {
            if (m_hover && m_inside)
                QToolTip::showText(mapToGlobal(QPoint(m_mouse.x(), height() + 8)), m_hover->toolTip(), this);
        });
    }

    QWidget *strip() const { return m_strip; }
    void setGroups(const QVector<QWidget*> &groups) { m_groups = groups; }

    // A faixa mudou (botão novo): recoleta os botões e redesenha.
    void stripChanged()
    {
        m_buttons = m_strip->findChildren<QToolButton*>();
        for (QToolButton *b : m_buttons) {
            if (b->property("carouselHooked").toBool()) continue;
            b->setProperty("carouselHooked", true);
            QObject::connect(b, &QToolButton::toggled, this, [this]() { markDirty(); });
        }
        // Fora da área visível: existe, tem layout, mas quem pinta é o carrossel.
        m_strip->move(0, 4000);
        markDirty();
    }

    void markDirty() { m_dirty = true; update(); }

    void setEdgeMode(bool on) { m_edge = on; m_vel = 0; }

    void reset()
    {
        m_x = m_target = 0;
        m_vel = 0;
        m_hover = nullptr;
        m_inside = false;
        m_tick.stop();
        markDirty();
        reportGroup();
    }

protected:
    qreal maxScroll() const { return qMax(0, m_strip->width() - width()); }

    // Coluna u da janela → posição na faixa (e o cosseno da curva ali).
    qreal src(qreal u, qreal *cosOut = nullptr) const
    {
        const qreal half = width() / 2.0;
        const qreal c = m_x + half;
        const qreal d = u - half;
        const qreal mx = maxScroll();
        // Só curva o lado que ainda tem botão escondido: parada no começo, o
        // primeiro botão encosta na borda inteiro.
        const qreal amt = d < 0 ? qMin(1.0, m_x / 40.0) : qMin(1.0, (mx - m_x) / 40.0);
        const qreal tm = kThetaMax * amt;
        if (tm < 0.02 || half <= 0) { if (cosOut) *cosOut = 1.0; return c + d; }
        const qreal R = half / std::sin(tm);
        const qreal th = std::asin(std::clamp(d / R, -1.0, 1.0));
        if (cosOut) *cosOut = std::cos(th);
        return c + R * th;
    }

    void ensurePix()
    {
        if (!m_dirty && !m_pix.isNull()) return;
        m_dirty = false;
        const qreal dpr = devicePixelRatioF();
        const QSize ss = m_strip->size();
        if (ss.isEmpty()) { m_pix = QPixmap(); return; }
        m_pix = QPixmap((QSizeF(ss) * dpr).toSize());
        m_pix.setDevicePixelRatio(dpr);
        m_pix.fill(Qt::transparent);
        QPainter pp(&m_pix);
        pp.setRenderHint(QPainter::Antialiasing, true);
        if (m_hover) {
            const QRect r(m_hover->mapTo(m_strip, QPoint(0, 0)), m_hover->size());
            pp.setPen(Qt::NoPen);
            pp.setBrush(tc(Theme::hoverOverlay()));
            pp.drawRoundedRect(QRectF(r), 6, 6);
        }
        pp.end();
        m_strip->render(&m_pix, QPoint(), QRegion(), QWidget::DrawChildren);
    }

    void paintEvent(QPaintEvent *) override
    {
        ensurePix();
        if (m_pix.isNull()) return;
        QPainter p(this);
        const qreal dpr = m_pix.devicePixelRatio();
        const qreal W = width(), H = height();
        const qreal stripW = m_strip->width();
        const qreal mx = maxScroll();
        const qreal lAmt = qMin(1.0, m_x / 40.0), rAmt = qMin(1.0, (mx - m_x) / 40.0);
        QColor bg = tc(Theme::panelBackground());

        // Miolo 1:1 (14/11/2026, "tá farinhado, parece DLSS"): onde a curva
        // desvia menos de meio pixel da reta, a fita entra inteira, sem filtro
        // e com a origem presa ao pixel da tela. Antes TUDO era fatiado em
        // colunas de 2 px esticadas pra 2,6 px com filtro suave, inclusive o
        // meio, que nem curva — e os ícones saíam borrados.
        const qreal half = W / 2.0;
        const qreal off = std::round(m_x * dpr) / dpr; // posição na fita da coluna 0
        auto bent = [&](qreal u) { return std::abs(src(u) - (m_x + u)) >= 0.5; };
        qreal uL = std::floor(half), uR = std::ceil(half);
        while (uL > 0 && !bent(uL - 1)) uL -= 1;
        while (uR < W && !bent(uR + 1)) uR += 1;
        uL = std::round(uL * dpr) / dpr;
        uR = std::round(uR * dpr) / dpr;
        {
            const qreal d0 = std::max(uL, -off), d1 = std::min(uR, stripW - off);
            if (d1 > d0) {
                p.drawPixmap(QRectF(d0, 0, d1 - d0, H), m_pix,
                             QRectF((off + d0) * dpr, 0, (d1 - d0) * dpr, H * dpr));
            }
        }

        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        constexpr qreal S = 2.0;
        for (qreal u = 0; u < W; u += S) {
            const qreal u1 = qMin(W, u + S);
            qreal c0, c1;
            qreal s0 = src(u, &c0), s1 = src(u1, &c1);
            if (s1 <= 0 || s0 >= stripW) continue;
            s0 = std::clamp(s0, 0.0, stripW);
            s1 = std::clamp(s1, 0.0, stripW);
            if (s1 - s0 <= 0.01) continue;
            const qreal cm = (c0 + c1) / 2.0;
            // Só as pontas curvas são fatiadas; o miolo já foi copiado acima.
            if (u1 <= uL || u >= uR) {
                const qreal k = 0.88 + 0.12 * cm;
                const qreal th = H * k;
                const qreal du0 = u, du1 = u1 <= uL ? qMin(uL, u1 + 0.6) : u1 + 0.6;
                p.drawPixmap(QRectF(du0, (H - th) / 2.0, du1 - du0, th), m_pix,
                             QRectF(s0 * dpr, 0, (s1 - s0) * dpr, H * dpr));
            } else if (u < uL || u1 > uR) {
                // Coluna que cruza a borda do miolo: desenha só a parte de fora.
                const qreal a0 = u < uL ? u : uR, a1 = u < uL ? uL : u1;
                const qreal t0 = src(a0), t1 = src(a1);
                if (t1 > t0)
                    p.drawPixmap(QRectF(a0, 0, a1 - a0, H), m_pix,
                                 QRectF(std::clamp(t0, 0.0, stripW) * dpr, 0,
                                        (std::clamp(t1, 0.0, stripW) - std::clamp(t0, 0.0, stripW)) * dpr, H * dpr));
            }
            // Apaga conforme curva, e some de vez rente à borda.
            qreal a = (1.0 - cm) * 0.95;
            const bool left = u < W / 2.0;
            const qreal fadeW = 26.0 * (left ? lAmt : rAmt);
            const qreal e = left ? u : W - u1;
            if (fadeW > 0.5 && e < fadeW) a = qMax(a, 1.0 - e / fadeW);
            if (a > 0.01) {
                bg.setAlphaF(std::clamp(a, 0.0, 1.0));
                p.fillRect(QRectF(u, 0, u1 - u + 0.6, H), bg);
            }
        }
    }

    QToolButton *hitButton(const QPoint &pos, qreal *cosOut = nullptr) const
    {
        const qreal s = src(pos.x(), cosOut);
        for (QToolButton *b : m_buttons) {
            const QRect r(b->mapTo(m_strip, QPoint(0, 0)), b->size());
            if (r.contains(QPoint(qRound(s), pos.y()))) return b;
        }
        return nullptr;
    }

    void updateHover()
    {
        QToolButton *h = m_inside ? hitButton(m_mouse) : nullptr;
        if (h == m_hover) return;
        m_hover = h;
        setCursor(h ? Qt::PointingHandCursor : Qt::ArrowCursor);
        QToolTip::hideText();
        if (h) m_tipTimer.start(); else m_tipTimer.stop();
        markDirty();
    }

    void reportGroup()
    {
        if (!onGroup) return;
        const qreal c = m_x + width() / 2.0;
        int best = 0;
        qreal bd = 1e9;
        for (int i = 0; i < m_groups.size(); ++i) {
            QWidget *g = m_groups.at(i);
            if (!g) continue;
            const qreal gc = g->mapTo(m_strip, QPoint(0, 0)).x() + g->width() / 2.0;
            if (std::abs(gc - c) < bd) { bd = std::abs(gc - c); best = i; }
        }
        onGroup(best, m_groups.size());
    }

    void kick() { if (!m_tick.isActive()) m_tick.start(); }

    void step()
    {
        const qreal mx = maxScroll();
        if (m_vel != 0) m_target = std::clamp(m_target + m_vel * 7.0, 0.0, mx);
        m_x += (m_target - m_x) * 0.2;
        if (std::abs(m_target - m_x) < 0.3) m_x = m_target;
        updateHover();
        reportGroup();
        update();
        const bool pushing = (m_vel < 0 && m_x > 0) || (m_vel > 0 && m_x < mx);
        if (m_x == m_target && !pushing) m_tick.stop();
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        m_mouse = e->position().toPoint();
        m_inside = true;
        const qreal W = width(), x = m_mouse.x();
        if (!m_edge) {
            // Normal: a fita acompanha o mouse de ponta a ponta.
            const qreal pad = 40;
            const qreal t = std::clamp((x - pad) / qMax(1.0, W - pad * 2), 0.0, 1.0);
            m_vel = 0;
            m_target = t * maxScroll();
        } else if (x < kEdgeZone) {
            m_vel = -std::pow((kEdgeZone - x) / kEdgeZone, 1.6);
        } else if (x > W - kEdgeZone) {
            m_vel = std::pow((x - (W - kEdgeZone)) / kEdgeZone, 1.6);
        } else {
            m_vel = 0; // meio parado pra mirar
        }
        updateHover();
        kick();
    }

    void leaveEvent(QEvent *e) override
    {
        m_inside = false;
        m_vel = 0;
        m_tipTimer.stop();
        QToolTip::hideText();
        updateHover();
        kick();
        QWidget::leaveEvent(e);
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        e->accept();
        m_press = m_hover;
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() != Qt::LeftButton) return;
        qreal c = 1.0;
        QToolButton *b = hitButton(e->position().toPoint(), &c);
        if (!b || b != m_press) return;
        m_press = nullptr;
        if (m_edge && c < 0.62) {
            // Botão meio apagado na ponta: só traz pro meio, sem disparar.
            const qreal bc = b->mapTo(m_strip, QPoint(0, 0)).x() + b->width() / 2.0;
            m_target = std::clamp(bc - width() / 2.0, 0.0, maxScroll());
            m_vel = 0;
            kick();
            return;
        }
        QToolTip::hideText();
        b->click();
        markDirty();
    }

    void wheelEvent(QWheelEvent *e) override
    {
        const QPoint d = e->angleDelta();
        const int delta = std::abs(d.x()) > std::abs(d.y()) ? -d.x() : -d.y();
        m_target = std::clamp(m_target + delta * 0.6, 0.0, maxScroll());
        kick();
        e->accept();
    }

private:
    QWidget *m_strip = nullptr;
    QList<QToolButton*> m_buttons;
    QVector<QWidget*> m_groups;
    QPixmap m_pix;
    bool m_dirty = true;
    qreal m_x = 0, m_target = 0, m_vel = 0;
    bool m_edge = false;
    bool m_inside = false;
    QPoint m_mouse;
    QToolButton *m_hover = nullptr;
    QToolButton *m_press = nullptr;
    QTimer m_tick;
    QTimer m_tipTimer;
};

SelectionPopup::SelectionPopup(QTextEdit *editor, QWidget *parent)
    : QFrame(parent)
    , m_editor(editor)
    , m_layout(nullptr)
{
    setObjectName(QStringLiteral("selectionPopup"));
    // WindowDoesNotAcceptFocus: no Mac, clicar numa janela Tool a tornava a
    // janela ativa, o editor perdia o foco e o popup sumia antes do clique
    // chegar no botão (nenhum botão da barra funcionava, relato da Marley).
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    // Transparente fora do balão: a lingueta de baixo e a sombra são desenhadas.
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::NoFocus);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(kShadow + 1, kShadow + 6, kShadow + 1, kShadow + kTabH + 5);
    outer->setSpacing(4);

    m_carousel = new SelectionCarousel(this);
    m_strip = m_carousel->strip();
    m_layout = new QHBoxLayout(m_strip);
    m_layout->setContentsMargins(6, 0, 6, 0);
    m_layout->setSpacing(2);
    m_layout->setSizeConstraint(QLayout::SetFixedSize);
    outer->addWidget(m_carousel, 0, Qt::AlignHCenter);

    m_dots = new SelectionGroupDots(this);
    outer->addWidget(m_dots, 0, Qt::AlignHCenter);
    m_carousel->onGroup = [this](int active, int count) { if (m_dots) m_dots->setState(active, count); };

    m_modeBtn = new QToolButton(this);
    m_modeBtn->setObjectName(QStringLiteral("selPopupMode"));
    m_modeBtn->setFixedSize(kModeW, kModeH);
    m_modeBtn->setIconSize(QSize(12, 12));
    m_modeBtn->setCheckable(true);
    m_modeBtn->setCursor(Qt::PointingHandCursor);
    m_modeBtn->setFocusPolicy(Qt::NoFocus);
    m_modeBtn->setAutoRaise(true);
    const bool edge = QSettings().value(QStringLiteral("selectionPopup/edgeMode"), false).toBool();
    m_modeBtn->setChecked(edge);
    connect(m_modeBtn, &QToolButton::toggled, this, [this](bool on) {
        QSettings().setValue(QStringLiteral("selectionPopup/edgeMode"), on);
        setEdgeMode(on);
    });
    setEdgeMode(edge);

    applyRootStyle();
    reloadAllIcons();

    hide();

    if (m_editor) {
        m_editor->viewport()->installEventFilter(this);
        m_editor->installEventFilter(this);
        connect(m_editor, &QTextEdit::selectionChanged,
                this, &SelectionPopup::onSelectionChanged);
        connect(m_editor, &QTextEdit::cursorPositionChanged,
                this, &SelectionPopup::onSelectionChanged);
        if (auto *bar = m_editor->verticalScrollBar()) {
            connect(bar, &QScrollBar::valueChanged,
                    this, &SelectionPopup::onScrollChanged);
        }
        if (auto *bar = m_editor->horizontalScrollBar()) {
            connect(bar, &QScrollBar::valueChanged,
                    this, &SelectionPopup::onScrollChanged);
        }
    }

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &SelectionPopup::applyTheme);
}

void SelectionPopup::setEdgeMode(bool on)
{
    if (m_carousel) m_carousel->setEdgeMode(on);
    if (m_modeBtn) {
        m_modeBtn->setToolTip(on ? tr("Voltar: a barra segue o mouse")
                                 : tr("Girar só pelas pontas"));
    }
}

void SelectionPopup::applyRootStyle()
{
    QColor a = tc(Theme::accentDefault());
    const QString accentSoft = QStringLiteral("rgba(%1, %2, %3, 0.16)").arg(a.red()).arg(a.green()).arg(a.blue());
    setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame#selectionPopup { background: transparent; border: none; }"
        "QWidget#selPopupStrip { background: transparent; }"
        "QToolButton#selPopupBtn {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: @radius-control;"
        "}"
        "QToolButton#selPopupBtn:hover { background: %1; }"
        "QToolButton#selPopupBtn:checked { background: %2; }"
        "QToolButton#selPopupMode { background: transparent; border: none; border-radius: 4px; }"
        "QToolButton#selPopupMode:hover { background: %1; }"
        "QToolButton#selPopupMode:checked { background: %3; }"
    ).arg(Theme::hoverOverlay(),
          Theme::pressedOverlay(),
          accentSoft)));
    m_bgCache = QPixmap();
    update();
}

void SelectionPopup::applyTheme()
{
    applyRootStyle();
    reloadAllIcons();
    for (QFrame* line : m_separators) {
        if (line) line->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::subtleBorder()));
    }
    for (QLabel* l : m_groupTitles) if (l) styleGroupTitle(l);
    if (m_carousel) m_carousel->markDirty();
    if (m_dots) m_dots->update();
}

void SelectionPopup::reloadAllIcons()
{
    for (auto it = m_iconAliasByBtn.cbegin(); it != m_iconAliasByBtn.cend(); ++it) {
        QToolButton* btn = it.key();
        if (!btn) continue;
        const QString resPath = QStringLiteral(":/icons/%1").arg(it.value());
        btn->setIcon(IconUtils::loadToolbarIcon(resPath,
                                                QColor(Theme::textPrimary()),
                                                QColor(Theme::textBright()),
                                                QColor(Theme::accentDefault()),
                                                QSize(kIconSize, kIconSize)));
    }
    if (m_modeBtn) {
        m_modeBtn->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/carousel-arrows.svg"),
                                                      QColor(Theme::textMuted()),
                                                      QColor(Theme::textBright()),
                                                      QColor(Theme::accentDefault()),
                                                      QSize(12, 12)));
    }
    if (m_carousel) m_carousel->markDirty();
}

void SelectionPopup::refitStrip()
{
    if (!m_strip || !m_carousel) return;
    // Os grupos guardam o tamanho antigo até o próximo giro do loop (o pedido
    // de relayout é adiado); sem invalidar cada um, a fita era medida com o
    // tamanho de antes e a janela ficava baixa demais (sem os títulos).
    ensurePolished();
    for (QWidget *box : std::as_const(m_groupBoxes)) {
        if (!box || !box->layout()) continue;
        QCoreApplication::sendPostedEvents(box, QEvent::LayoutRequest);
        box->layout()->invalidate();
        box->layout()->activate();
    }
    QCoreApplication::sendPostedEvents(m_strip, QEvent::LayoutRequest);
    m_layout->invalidate();
    m_layout->activate();
    const QSize ss = m_strip->sizeHint();
    m_strip->resize(ss);
    m_carousel->setFixedSize(qMin(kViewW, ss.width()), ss.height());
    m_carousel->setGroups(m_groupBoxes);
    m_carousel->stripChanged();
    const bool rolls = ss.width() > kViewW;
    m_dots->setVisible(rolls && m_groupBoxes.size() > 1);
    m_dots->setState(0, int(m_groupBoxes.size()));
    m_modeBtn->setVisible(rolls);
    adjustSize();
}

QToolButton* SelectionPopup::addAction(const QString &iconAlias,
                                       const QString &tooltip,
                                       Callback cb)
{
    auto *b = makeBtn(m_strip);
    const QString resPath = QStringLiteral(":/icons/%1").arg(iconAlias);
    b->setIcon(IconUtils::loadToolbarIcon(resPath,
                                          QColor(Theme::textPrimary()),
                                          QColor(Theme::textBright()),
                                          QColor(Theme::accentDefault()),
                                          QSize(kIconSize, kIconSize)));
    b->setToolTip(tooltip);
    if (cb) {
        connect(b, &QToolButton::clicked, this, [cb]() { cb(); });
    }
    if (m_groupRow) m_groupRow->addWidget(b);
    else m_layout->addWidget(b);
    m_iconAliasByBtn.insert(b, iconAlias);
    refitStrip();
    return b;
}

void SelectionPopup::beginGroup(const QString &title)
{
    if (m_groupRow) addSeparator();
    auto *box = new QWidget(m_strip);
    auto *v = new QVBoxLayout(box);
    v->setContentsMargins(1, 0, 1, 0);
    v->setSpacing(0);
    m_groupRow = new QHBoxLayout;
    m_groupRow->setSpacing(2);
    v->addLayout(m_groupRow);
    auto *l = new QLabel(title.toUpper(), box);
    l->setAlignment(Qt::AlignHCenter);
    styleGroupTitle(l);
    v->addWidget(l);
    m_groupTitles.append(l);
    m_groupBoxes.append(box);
    m_layout->addWidget(box);
    refitStrip();
}

void SelectionPopup::styleGroupTitle(QLabel *l)
{
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(8);
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    l->setFont(f);
    l->setStyleSheet(QStringLiteral("color: %1; background: transparent; padding-bottom: 1px;").arg(Theme::textMuted()));
}

void SelectionPopup::addSeparator()
{
    auto *line = new QFrame(m_strip);
    line->setFrameShape(QFrame::VLine);
    line->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::subtleBorder()));
    line->setFixedSize(1, kButtonSize - 8);
    if (m_groupRow) {
        // Entre grupos: o fio fica na altura dos botões, não do título.
        auto *holder = new QWidget(m_strip);
        holder->setFixedSize(7, kButtonSize);
        line->setParent(holder);
        line->move(3, 4);
        m_layout->addWidget(holder, 0, Qt::AlignTop);
    } else {
        m_layout->addWidget(line);
    }
    m_separators.append(line);
    refitStrip();
}

QPainterPath SelectionPopup::balloonPath() const
{
    const QRectF body(kShadow + 0.5, kShadow + 0.5,
                      width() - 2 * kShadow - 1, height() - 2 * kShadow - kTabH - 1);
    QPainterPath path;
    path.addRoundedRect(body, kRadius, kRadius);
    if (m_modeBtn && m_modeBtn->isVisible()) {
        // Lingueta embaixo, no meio: o balão aponta pro texto.
        const qreal cx = width() / 2.0;
        const qreal l = cx - kTabW / 2.0, r = cx + kTabW / 2.0;
        const qreal top = body.bottom() - 2, bottom = body.bottom() + kTabH;
        const qreal rr = 6.0;
        QPainterPath tab;
        tab.moveTo(l, top);
        tab.lineTo(l, bottom - rr);
        tab.quadTo(l, bottom, l + rr, bottom);
        tab.lineTo(r - rr, bottom);
        tab.quadTo(r, bottom, r, bottom - rr);
        tab.lineTo(r, top);
        tab.closeSubpath();
        path = path.united(tab).simplified();
    }
    return path;
}

void SelectionPopup::paintEvent(QPaintEvent *)
{
    const qreal dpr = devicePixelRatioF();
    if (m_bgCache.isNull() || m_bgCache.size() != (QSizeF(size()) * dpr).toSize()) {
        m_bgCache = QPixmap((QSizeF(size()) * dpr).toSize());
        m_bgCache.setDevicePixelRatio(dpr);
        m_bgCache.fill(Qt::transparent);
        QPainter p(&m_bgCache);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QPainterPath path = balloonPath();
        // Sombra macia: o contorno engordado em camadas bem transparentes.
        p.setPen(Qt::NoPen);
        for (int i = 7; i >= 1; --i) {
            QPainterPathStroker st;
            st.setWidth(i * 2.2);
            st.setJoinStyle(Qt::RoundJoin);
            const QPainterPath grown = st.createStroke(path).united(path);
            p.fillPath(grown.translated(0, 2), QColor(0, 0, 0, 10));
        }
        p.fillPath(path, tc(Theme::panelBackground()));
        p.setPen(QPen(tc(Theme::panelBorder()), 1));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
    QPainter p(this);
    p.drawPixmap(0, 0, m_bgCache);
}

void SelectionPopup::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    m_bgCache = QPixmap();
    if (m_modeBtn) {
        const int bodyBottom = height() - kShadow - kTabH - 1;
        m_modeBtn->move((width() - kModeW) / 2, bodyBottom);
        m_modeBtn->raise();
    }
}

void SelectionPopup::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    // Medida definitiva já com tudo visível (antes da 1ª exibição o Qt pode
    // ainda não contar os títulos dos grupos).
    refitStrip();
    if (m_carousel) m_carousel->reset();
}

bool SelectionPopup::hasSelection() const
{
    return m_editor && m_editor->textCursor().hasSelection();
}

bool SelectionPopup::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_editor) return QFrame::eventFilter(watched, event);

    const QEvent::Type t = event->type();
    if (watched == m_editor->viewport()) {
        if (t == QEvent::MouseButtonPress) {
            m_dragging = true;
            hide();
        } else if (t == QEvent::MouseButtonRelease) {
            m_dragging = false;
            onSelectionChanged();
        }
    } else if (watched == m_editor) {
        if (t == QEvent::FocusOut) {
            // O foco saiu porque o clique foi no próprio popup: deixa o clique chegar.
            if (isVisible() && frameGeometry().contains(QCursor::pos())) return QFrame::eventFilter(watched, event);
            hide();
        } else if (t == QEvent::Hide) {
            hide();
        }
    }
    return QFrame::eventFilter(watched, event);
}

void SelectionPopup::onSelectionChanged()
{
    if (m_dragging) return;
    if (!hasSelection()) {
        hide();
        return;
    }
    updatePosition();
    if (!isVisible()) show();
}

void SelectionPopup::onScrollChanged()
{
    if (!isVisible()) return;
    if (!hasSelection()) {
        hide();
        return;
    }
    updatePosition();
}

void SelectionPopup::updatePosition()
{
    if (!m_editor) return;

    QTextCursor cur = m_editor->textCursor();
    if (!cur.hasSelection()) return;

    QTextCursor a(m_editor->document());
    a.setPosition(cur.selectionStart());
    QTextCursor b(m_editor->document());
    b.setPosition(cur.selectionEnd());

    const QRect ra = m_editor->cursorRect(a);
    const QRect rb = m_editor->cursorRect(b);
    QRect bounds = ra.united(rb);

    const QRect viewportRect = m_editor->viewport()->rect();
    // Não posiciona se a seleção saiu da janela visível.
    if (!viewportRect.intersects(bounds)) {
        hide();
        return;
    }

    if (!isVisible()) refitStrip();
    adjustSize();
    const QSize ps = size();
    // A ponta da lingueta (base visível do balão) fica logo acima da seleção;
    // a margem da sombra é transparente e não conta.
    const QPoint topMid(bounds.left() + bounds.width() / 2,
                        bounds.top() - kGapAboveSelection);
    QPoint global = m_editor->viewport()->mapToGlobal(topMid);
    global.rx() -= ps.width() / 2;
    global.ry() -= ps.height() - kShadow;

    // Se for sair em cima, posiciona abaixo da seleção.
    const QPoint topOfViewportGlobal = m_editor->viewport()->mapToGlobal(QPoint(0, 0));
    if (global.y() + kShadow < topOfViewportGlobal.y()) {
        global.setY(m_editor->viewport()
                        ->mapToGlobal(QPoint(0, bounds.bottom() + kGapAboveSelection))
                        .y() - kShadow);
    }
    move(global);
}
