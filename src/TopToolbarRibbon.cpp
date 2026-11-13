// Faixa estilo Word da TopToolbar (12/11/2026, pedido do Pe, desenhada no
// Topbar Lab). Três modos: completa, simplificada (uma linha) e só as abas.
// A barra clássica continua sendo a de sempre; aqui só se muda a CASA dos
// mesmos botões, que seguem com os mesmos menus, sinais e estados.

#include "TopToolbar.h"

#include "CarouselViewport.h"
#include "EditorLayout.h"
#include "IconUtils.h"
#include "MiraPersonality.h"
#include "SceneBreaks.h"
#include "Theme.h"
#include "ToolbarGroupWidget.h"
#include "UiScale.h"

#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QSlider>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVariantAnimation>
#include <QDateTime>
#include <cmath>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

enum Role { Small = 0, Big = 1, Inline = 2, Combo = 3 };

constexpr int kTabsH = 31;
constexpr int kFullBodyH = 94;
constexpr int kSimpleBodyH = 42;
constexpr int kBinderBodyH = 34;
constexpr int kReelBodyH = 42;     // Rolo: a altura da clássica
constexpr int kAccentTop = 4;      // sotaque: respiro em cima das abas
constexpr int kAccentBottom = 8;   // e embaixo do cartão
constexpr int kMinSheetHeight = 240;   // igual ao das Configurações

int sc(int v) { return qMax(1, qRound(v * UiScale::scale())); }

int tabsHFor(TopToolbar::BarStyle s)
{
    if (s == TopToolbar::BarStyle::RibbonAccent) return 34;
    if (s == TopToolbar::BarStyle::RibbonBinder) return 27;
    return kTabsH;
}

// O Q alto da marca (a letra da vinheta), só a silhueta, pintado numa cor só.
// Pedido do Pe (13/11): o ícone colorido inteiro não ficava bem na faixa.
QIcon solidQ(const QColor& color, int h)
{
    const QImage src(QStringLiteral(":/app/splash-letters/q.png"));
    if (src.isNull()) return QIcon();
    const qreal dpr = qApp->devicePixelRatio();
    QImage img = src.scaledToHeight(qRound(h * dpr), Qt::SmoothTransformation)
                    .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(img.rect(), color);
    p.end();
    QPixmap pm = QPixmap::fromImage(img);
    pm.setDevicePixelRatio(dpr);
    return QIcon(pm);
}

QColor mixColor(const QColor& a, const QColor& b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

QColor inkOn(const QColor& c)
{
    const qreal lum = (0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue()) / 255.0;
    return lum > 0.62 ? QColor(QStringLiteral("#1a1a19")) : QColor(QStringLiteral("#fffaf5"));
}

QIcon tinted(const QString& name, const QColor& color, int px)
{
    return IconUtils::loadToolbarIcon(QStringLiteral(":/icons/%1").arg(name), color, color, color, QSize(px, px));
}

QFrame* groupSep(QWidget* parent, int h)
{
    auto* f = new QFrame(parent);
    f->setObjectName(QStringLiteral("ribbonGroupSep"));
    f->setFixedSize(1, h);
    return f;
}

// Chave liga/desliga, o mesmo desenho da ToggleSwitch das Configurações.
class PageSwitch : public QCheckBox
{
public:
    explicit PageSwitch(QWidget* parent) : QCheckBox(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFixedSize(36, 20);
    }
protected:
    bool hitButton(const QPoint&) const override { return true; }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const bool on = isChecked();
        QColor track = Theme::toColor(on ? Theme::accentDefault() : Theme::borderStrong());
        QColor knob = on ? QColor(Qt::white) : Theme::toColor(Theme::textPrimary());
        p.setPen(Qt::NoPen);
        p.setBrush(track);
        const QRectF r(0.5, 0.5, width() - 1.0, height() - 1.0);
        p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
        const qreal d = height() - 7.0;
        p.setBrush(knob);
        p.drawEllipse(QRectF(on ? width() - 3.5 - d : 3.5, 3.5, d, d));
    }
};

// Setinha de "abre um menu" pintada por cima do botão, rente à direita.
void addCaret(QToolButton* b)
{
    if (!b || b->findChild<QLabel*>(QStringLiteral("ribbonCaret"))) return;
    auto* l = new QLabel(b);
    l->setObjectName(QStringLiteral("ribbonCaret"));
    l->setAttribute(Qt::WA_TransparentForMouseEvents);
    const int px = sc(10);
    l->setPixmap(tinted(QStringLiteral("caret-down.svg"), QColor(Theme::iconColor()), px).pixmap(px, px));
    l->setFixedSize(px, px);
    l->show();
    b->setProperty("ribbonCaret", true);
}

void placeCaret(QToolButton* b)
{
    if (auto* l = b->findChild<QLabel*>(QStringLiteral("ribbonCaret")))
        l->move(b->width() - l->width() - sc(5), (b->height() - l->height()) / 2);
}

// Arquivo: lista na cor do destaque à esquerda, explicação à direita.
class Backstage : public QFrame
{
public:
    struct Item { QToolButton* target; QString icon; QString glyph; QString label; QString desc; QString keys; };

    Backstage(QWidget* parent, const QList<Item>& items) : QFrame(parent, Qt::Popup)
    {
        setObjectName(QStringLiteral("ribbonBackstage"));
        setAttribute(Qt::WA_DeleteOnClose);
        const QColor acc = Theme::toColor(Theme::accentDefault());
        const QColor ink = inkOn(acc);
        setStyleSheet(Theme::qss(QStringLiteral(R"(
            QFrame#ribbonBackstage { background: %1; border: 1px solid %2; border-radius: @radius-panel; }
            QFrame#backCol { background: %3; border: none; border-top-left-radius: @radius-panel; border-bottom-left-radius: @radius-panel; }
            QToolButton#backItem { background: transparent; border: none; border-radius: @radius-control; color: %4;
                                   text-align: left; padding: 7px 10px; font-size: 13px; }
            QToolButton#backItem:hover { background: rgba(255,255,255,0.16); }
            QFrame#backRule { background: rgba(255,255,255,0.25); border: none; }
            QLabel#backTitle { color: %5; font-family: Lora; font-size: 17px; font-weight: 600; }
            QLabel#backDesc { color: %6; font-size: 13px; }
            QLabel#backKeys { color: %7; font-size: 11px; font-weight: 600; }
        )")).arg(Theme::panelBackground(), Theme::panelBorder(), acc.name(), ink.name(),
                Theme::textBright(), Theme::textPrimary(), Theme::textMuted()));

        auto* h = new QHBoxLayout(this);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(0);
        auto* col = new QFrame(this);
        col->setObjectName(QStringLiteral("backCol"));
        col->setFixedWidth(sc(200));
        auto* v = new QVBoxLayout(col);
        v->setContentsMargins(6, 8, 6, 8);
        v->setSpacing(1);
        auto* det = new QWidget(this);
        auto* dv = new QVBoxLayout(det);
        dv->setContentsMargins(20, 18, 20, 16);
        dv->setSpacing(6);
        m_title = new QLabel(TopToolbar::tr("Arquivo"), det);
        m_title->setObjectName(QStringLiteral("backTitle"));
        m_desc = new QLabel(det);
        m_desc->setObjectName(QStringLiteral("backDesc"));
        m_desc->setWordWrap(true);
        m_keys = new QLabel(det);
        m_keys->setObjectName(QStringLiteral("backKeys"));
        dv->addWidget(m_title);
        dv->addWidget(m_desc);
        dv->addStretch(1);
        dv->addWidget(m_keys);
        det->setFixedWidth(sc(300));

        m_defaultDesc = TopToolbar::tr("Tudo que é do projeto inteiro, e não do texto: abrir, salvar, exportar, voltar pro menu.");
        showInfo(TopToolbar::tr("Arquivo"), m_defaultDesc, QString());

        const int ipx = sc(18);
        for (const Item& it : items) {
            if (!it.target) { auto* r = new QFrame(col); r->setObjectName(QStringLiteral("backRule")); r->setFixedHeight(1); v->addSpacing(4); v->addWidget(r); v->addSpacing(4); continue; }
            auto* b = new QToolButton(col);
            b->setObjectName(QStringLiteral("backItem"));
            b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            b->setText(it.label);
            b->setCursor(Qt::PointingHandCursor);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            if (!it.icon.isEmpty()) b->setIcon(tinted(it.icon, ink, ipx));
            else b->setIcon(glyphIcon(it.glyph, ink, ipx));
            b->setIconSize(QSize(ipx, ipx));
            const Item copy = it;
            b->installEventFilter(this);
            b->setProperty("backIndex", m_items.size());
            m_items.append(copy);
            QObject::connect(b, &QToolButton::clicked, this, [this, copy]() {
                close();
                QToolButton* t = copy.target;
                QTimer::singleShot(0, t, [t]() { t->click(); });
            });
            v->addWidget(b);
        }
        v->addStretch(1);
        h->addWidget(col);
        h->addWidget(det, 1);
    }

protected:
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (e->type() == QEvent::Enter) {
            const int i = o->property("backIndex").toInt();
            if (i >= 0 && i < m_items.size()) showInfo(m_items[i].label, m_items[i].desc, m_items[i].keys);
        }
        return QFrame::eventFilter(o, e);
    }

private:
    static QIcon glyphIcon(const QString& g, const QColor& c, int px)
    {
        const qreal dpr = qApp->devicePixelRatio();
        QPixmap pm(QSize(px, px) * dpr);
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        QFont f = p.font();
        f.setPixelSize(px - 2);
        f.setBold(true);
        p.setFont(f);
        p.setPen(c);
        p.drawText(QRect(0, 0, px, px), Qt::AlignCenter, g);
        return QIcon(pm);
    }
    void showInfo(const QString& t, const QString& d, const QString& k)
    {
        m_title->setText(t);
        m_desc->setText(d);
        m_keys->setText(k);
    }
    QList<Item> m_items;
    QLabel* m_title = nullptr;
    QLabel* m_desc = nullptr;
    QLabel* m_keys = nullptr;
    QString m_defaultDesc;
};

// O Rolo: os nomes das seções num cilindro, como um cadeado de segredo. O do
// meio é a seção aberta; os vizinhos aparecem inclinados em cima e embaixo.
// Roda, clique (metade de cima volta, de baixo avança) ou arrasto giram.
class RibbonReel : public QWidget
{
public:
    std::function<void(int)> onStep;
    std::function<bool(int)> onWheel;
    QStringList names;

    explicit RibbonReel(QWidget* parent) : QWidget(parent)
    {
        setCursor(Qt::PointingHandCursor);
        m_anim.setDuration(420);
        m_anim.setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_pos = v.toReal(); update(); });
    }
    int index() const { return m_k; }
    void setIndex(int k, bool animated)
    {
        m_k = k;
        m_anim.stop();
        if (animated) { m_anim.setStartValue(m_pos); m_anim.setEndValue(qreal(k)); m_anim.start(); }
        else { m_pos = k; update(); }
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        if (names.isEmpty()) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        const qreal h = height(), w = width(), cy = h / 2.0;
        const qreal faceH = h * 0.36;   // discreto (pedido do Pe, 13/11)
        const qreal R = (faceH / 2.0) / std::tan(M_PI / 8.0);   // 8 faces de 45°
        // Os dois fios do segredo, acima e abaixo do nome aberto.
        QColor line = Theme::toColor(Theme::accentDefault());
        line.setAlphaF(0.3);
        p.setPen(QPen(line, 1));
        const qreal inset = sc(14);
        p.drawLine(QPointF(inset, cy - faceH / 2.0 - 1.5), QPointF(w - inset, cy - faceH / 2.0 - 1.5));
        p.drawLine(QPointF(inset, cy + faceH / 2.0 + 1.5), QPointF(w - inset, cy + faceH / 2.0 + 1.5));
        QFont f(QStringLiteral("Alegreya"));
        f.setItalic(true);
        f.setWeight(QFont::Normal);
        f.setPixelSize(sc(13));
        p.setFont(f);
        const QColor ink = Theme::toColor(Theme::textBright());
        const int base = int(std::floor(m_pos));
        for (int j = base - 2; j <= base + 3; ++j) {
            const qreal a = (j - m_pos) * (M_PI / 4.0);
            if (std::abs(a) >= M_PI / 2.0 - 0.05) continue;
            const qreal y = cy + R * std::sin(a);
            const qreal s = std::cos(a);
            QColor c = ink;
            c.setAlphaF(std::pow(s, 5.5));   // vizinhos quase sumindo
            p.save();
            p.translate(w / 2.0, y);
            p.scale(1.0, s);
            p.setPen(c);
            p.drawText(QRectF(-w / 2.0, -faceH / 2.0 - 2, w, faceH + 4), Qt::AlignCenter, names.at(((j % names.size()) + names.size()) % names.size()));
            p.restore();
        }
    }
    void wheelEvent(QWheelEvent* e) override
    {
        if (onWheel) onWheel(e->angleDelta().y());
        e->accept();
    }
    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        m_pressY = e->position().y();
        m_drag = false;
        e->accept();
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (!(e->buttons() & Qt::LeftButton)) return;
        const qreal dy = e->position().y() - m_pressY;
        if (!m_drag && std::abs(dy) < 4) return;
        m_drag = true;
        m_anim.stop();
        m_pos = m_k - dy / step();
        update();
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        const qreal dy = e->position().y() - m_pressY;
        if (m_drag) {
            const int d = qRound(-dy / step());
            if (d == 0) setIndex(m_k, true);
            else if (onStep) onStep(d);
        } else if (onStep) {
            onStep(e->position().y() < height() / 2.0 ? -1 : 1);
        }
        m_drag = false;
    }
private:
    static qreal step() { return sc(18); }
    QVariantAnimation m_anim;
    qreal m_pos = 0;
    int m_k = 0;
    qreal m_pressY = 0;
    bool m_drag = false;
};

// A faixa virando junto com o rolo: a seção velha sobe (ou desce) dobrando
// e a nova chega pela outra beira, como duas faces de um tambor.
class RibbonFlip : public QWidget
{
public:
    explicit RibbonFlip(QWidget* parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        hide();
        m_anim.setDuration(380);
        m_anim.setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_t = v.toReal(); update(); });
        QObject::connect(&m_anim, &QVariantAnimation::finished, this, [this]() { hide(); m_from = QPixmap(); m_to = QPixmap(); });
    }
    void run(const QPixmap& from, const QPixmap& to, int dir, const QColor& bg)
    {
        m_from = from; m_to = to; m_dir = dir; m_bg = bg; m_t = 0;
        raise();
        show();
        m_anim.stop();
        m_anim.setStartValue(0.0);
        m_anim.setEndValue(1.0);
        m_anim.start();
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        if (m_bg.alpha() > 0) p.fillRect(rect(), m_bg);
        const qreal th = m_t * M_PI / 2.0, h = height(), w = width();
        const qreal hA = h * std::cos(th), hB = h * std::sin(th);
        const qreal yA = h / 2.0 - m_dir * (h / 2.0) * std::sin(th);
        const qreal yB = h / 2.0 + m_dir * (h / 2.0) * std::cos(th);
        auto face = [&](const QPixmap& pm, qreal cy, qreal fh, qreal shade) {
            if (fh < 0.5 || pm.isNull()) return;
            const QRectF r(0, cy - fh / 2.0, w, fh);
            p.drawPixmap(r, pm, QRectF(QPointF(0, 0), QSizeF(pm.size())));
            QColor s(0, 0, 0);
            s.setAlphaF(qBound(0.0, shade, 1.0));
            p.fillRect(r, s);
        };
        face(m_from, yA, hA, 0.18 * std::sin(th));
        face(m_to, yB, hB, 0.18 * std::cos(th));
    }
private:
    QVariantAnimation m_anim;
    QPixmap m_from, m_to;
    QColor m_bg;
    int m_dir = 1;
    qreal m_t = 0;
};

} // namespace

// ============================================================================

void TopToolbar::setRibbonTargetWidth(int px)
{
    if (px == m_ribbonTargetWidth) return;
    m_ribbonTargetWidth = px;
    if (m_ribbon && ribbonActive()) {
        m_ribbon->setMinimumWidth(qMax(0, px));
        applyBinderIndent();
        updateGeometry();
    }
}

void TopToolbar::setRibbonPageCenter(int x)
{
    if (x == m_ribbonPageCenter) return;
    m_ribbonPageCenter = x;
    if (m_ribbon && ribbonActive()) applyBinderIndent();
}

// Fichário de ponta a ponta (pedidos do Pe, 13/11): o Q fica sozinho na beira,
// e as abas e os botões começam juntos numa linha só. A fileira de botões do
// Editor (a mais larga) fica centralizada com a folha, e as outras abas usam a
// mesma linha, pra nada pular ao trocar de aba. Nunca encosta no Q nem empurra
// os botões pro carrossel.
void TopToolbar::applyBinderIndent()
{
    if (!m_ribbon || !m_ribbonQGap || !m_ribbonBody || !m_ribbonTabsRow) return;
    auto* trl = qobject_cast<QHBoxLayout*>(m_ribbonTabsRow->layout());
    auto* bl = qobject_cast<QHBoxLayout*>(m_ribbonBody->layout());
    if (!trl || !bl) return;
    const int qEnd = trl->contentsMargins().left() + m_ribbonFile->sizeHint().width() + trl->spacing();
    int x = 0;
    if (m_barStyle == BarStyle::RibbonBinder && ribbonEdgeToEdge()) {
        // Largura dos botões do Editor, sem as margens de 4 px da página.
        const int rowW = (m_ribbonStack && m_ribbonStack->count() > 0)
            ? qMax(0, m_ribbonStack->widget(0)->sizeHint().width() - 8) : 0;
        x = m_ribbonPageCenter > 0 ? m_ribbonPageCenter - rowW / 2 : qMin(sc(382), m_ribbonTargetWidth / 4);
        const int lo = qEnd + sc(6);
        const int hi = qMax(lo, m_ribbonTargetWidth - rowW - sc(16));
        x = qBound(lo, x, hi);
    }
    // O espaçador não ganha espaçamento do layout (item vazio); a primeira aba ganha.
    const int gap = x > 0 ? qMax(sc(6), x - qEnd) : 0;
    m_ribbonQGap->changeSize(gap, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    trl->invalidate();
    // A fileira de botões começa 4 px pra dentro da página do carrossel.
    bl->setContentsMargins(x > 0 ? qMax(4, x - 4) : 4, 0, 4, 0);
}

void TopToolbar::setRibbonSideInsets(int left, int right)
{
    if (left == m_ribbonInsetL && right == m_ribbonInsetR) return;
    m_ribbonInsetL = qMax(0, left);
    m_ribbonInsetR = qMax(0, right);
    applyRibbonMargins();
}

void TopToolbar::applyRibbonMargins()
{
    if (!m_ribbon || !m_ribbon->layout()) return;
    // Sotaque: abas e cartão com respiro em volta, o cartão solto dentro da faixa.
    const bool accent = (m_barStyle == BarStyle::RibbonAccent);
    const int pad = accent ? sc(8) : 0;
    // Fichário (pedido do Pe, 13/11): rasga até a beira da janela, por cima do
    // canto da LeftBar, em vez de começar alinhado com a borda dela.
    const bool binder = (m_barStyle == BarStyle::RibbonBinder);
    const int l = binder ? 0 : m_ribbonInsetL, r = binder ? 0 : m_ribbonInsetR;
    m_ribbon->layout()->setContentsMargins(l + pad, accent ? sc(kAccentTop) : 0,
                                           r + pad, accent ? sc(kAccentBottom) : 0);
}

void TopToolbar::setRibbonFloating(bool on)
{
    if (on == m_ribbonFloating) return;
    m_ribbonFloating = on;
    QSettings().setValue(QStringLiteral("ui/topToolbarRibbonFloating"), on);
    if (on) setRibbonSideInsets(0, 0);
    applyRootStyle();
    if (ribbonActive()) rebuildRibbonPages();
    emit ribbonPlacementChanged(on);
}

int TopToolbar::ribbonHeight(bool withPeek) const
{
    if (m_barStyle == BarStyle::RibbonReel) return 2 + sc(kReelBodyH);   // Rolo: sem a linha das abas
    int h = sc(tabsHFor(m_barStyle)) + 2;
    if (m_barStyle == BarStyle::RibbonFull) h += sc(kFullBodyH);
    else if (m_barStyle == BarStyle::RibbonBinder) h += sc(kBinderBodyH);
    else if (ribbonOneLine()) h += sc(kSimpleBodyH);
    else if (withPeek && m_ribbonPeek) h += sc(kFullBodyH);
    if (m_barStyle == BarStyle::RibbonAccent) h += sc(kAccentTop + kAccentBottom);
    return h;
}

QString TopToolbar::ribbonQss() const
{
    const QColor acc = Theme::toColor(Theme::accentDefault());
    return Theme::qss(QStringLiteral(R"(
        QWidget#ttbRibbon, QWidget#ribbonTabsRow, QStackedWidget#ribbonStack, QWidget#ribbonPage { background: transparent; }
        QWidget#ribbonBody { background: transparent; border: none; border-top: 1px solid %1; }
        QToolButton#ribbonFile { background: %2; color: %3; border: none; border-radius: @radius-control;
                                 padding: 0px 14px; font-size: 12px; font-weight: 600; }
        QToolButton#ribbonFile:hover { background: %4; border: none; }
        QToolButton#ribbonTab { background: transparent; border: none; border-bottom: 3px solid transparent;
                                border-radius: 0px; color: %5; padding: 0px 12px; font-size: 12px; }
        QToolButton#ribbonTab:hover { color: %6; background: transparent; border-color: transparent; border-bottom-color: transparent; }
        QToolButton#ribbonTab:checked { color: %7; background: transparent; border-color: transparent; border-bottom: 3px solid %2; font-weight: 600; }
        QLabel#ribbonGroupLabel { color: %5; font-size: 10px; }
        QFrame#ribbonGroupSep { background: %1; border: none; }
        QToolButton[ribbonRole="big"] { font-size: 11px; padding: 3px 5px 2px 5px; min-width: 0px; }
        QToolButton[ribbonRole="inline"] { font-size: 12px; padding: 0px 6px; min-width: 0px; }
        QToolButton[ribbonRole="small"] { min-width: 0px; padding: 0px; }
        QToolButton[ribbonCaret="true"] { padding-right: 16px; }
        QToolButton[ribbonRole="combo"] { border: 1px solid %8; background: %9; text-align: left;
                                          padding: 0px 20px 0px 8px; min-width: 0px; color: %6; }
        QToolButton[ribbonRole="combo"]:hover { border-color: %6; }
    )")).arg(Theme::subtleBorder(), acc.name(), inkOn(acc).name(), acc.lighter(112).name(),
            Theme::textMuted(), Theme::textPrimary(), Theme::textBright(),
            Theme::panelBorder(), Theme::inputBackground())
        + ribbonLookQss();
}

// As duas faixas com cara de Qenna (13/11/2026, lab Escala Word–Qenna).
QString TopToolbar::ribbonLookQss() const
{
    const QString line = Theme::panelBorderFor(Theme::PanelKey::TopToolbar);
    const QString panel = Theme::panelBackgroundCssFor(Theme::PanelKey::TopToolbar);
    const QColor panelC = Theme::toColor(Theme::panelBackground());
    // Aba de trás: um tico deslocada do fundo da linha das abas, na direção do
    // texto (sem contorno, ela precisa se destacar só pela cor, nos dois temas).
    Q_UNUSED(panelC);
    const QColor chip = mixColor(Theme::toColor(Theme::appBackground()), Theme::toColor(Theme::textPrimary()), 0.08);
    return Theme::qss(QStringLiteral(R"(
        QToolButton#ribbonFile[qmark="true"] { background: transparent; border: 1px solid transparent; border-radius: @radius-control;
                                               padding: 0px 16px 0px 4px; }
        QToolButton#ribbonFile[qmark="true"]:hover { background: %1; border-color: %2; }
        QToolButton#ribbonTab[look="accent"] { font-size: 13px; padding: 0px 11px; }
        QWidget#ribbonBody[look="accent"] { background: %3; border: 1px solid %2; border-radius: 8px; }
        QWidget#ribbonTabsRow[look="binder"] { background: %3; border: none; border-bottom: 1px solid %4; }
        QWidget#ribbonTabsRow[look="binder-float"] { background: transparent; border: none; border-bottom: 1px solid %4; }
        QWidget#ribbonBody[look="binder"] { border: none; }
        /* 13/11, pedido do Pe: contorno demais poluía. As abas e as caixas de
           fonte ficam sem contorno; só as linhas da barra continuam. A de trás
           guarda a linha de baixo (é a linha da barra passando), a aberta a
           cobre com a cor da faixa e se emenda nela. */
        QToolButton#ribbonTab[look="binder"] { background: %5; color: %6; border: none; border-bottom: 1px solid %4;
            border-top-left-radius: 8px; border-top-right-radius: 8px; border-bottom-left-radius: 0px; border-bottom-right-radius: 0px;
            padding: 0px 14px; font-family: Lora; font-size: 12px; }
        QToolButton#ribbonTab[look="binder"]:hover { color: %7; background: %5; border: none; border-bottom: 1px solid %4; }
        QToolButton#ribbonTab[look="binder"]:checked { background: %8; color: %9; font-weight: 600;
            border: 1px solid %4; border-bottom: 1px solid %8; }   /* só a aberta tem contorno (teste do Pe, 13/11) */
        QToolButton[comboLook="binder"] { border: 1px solid transparent; }
        QToolButton[comboLook="binder"]:hover { border-color: transparent; color: %9; }
    )")).arg(Theme::hoverOverlay(), Theme::subtleBorder(), Theme::appBackground(), line,
            chip.name(), Theme::textMuted(), Theme::textPrimary(), panel, Theme::textBright());
}

QString TopToolbar::ribbonRootQss() const
{
    // Faixa = teto da janela: sem moldura dos lados nem cantos, só a linha de baixo.
    // Flutuante, ela fica com a moldura de painel de sempre.
    if (!ribbonEdgeToEdge()) return QString();
    return QStringLiteral("QWidget#topToolbar { border: none; border-bottom: 1px solid %1; border-radius: 0px; }")
        .arg(Theme::panelBorderFor(Theme::PanelKey::TopToolbar));
}

void TopToolbar::buildRibbonChrome()
{
    m_ribbon = new QWidget(this);
    m_ribbon->setObjectName(QStringLiteral("ttbRibbon"));
    auto* v = new QVBoxLayout(m_ribbon);
    v->setContentsMargins(m_ribbonInsetL, 0, m_ribbonInsetR, 0);
    v->setSpacing(0);

    // ---- Linha das abas ----
    m_ribbonTabsRow = new QWidget(m_ribbon);
    m_ribbonTabsRow->setObjectName(QStringLiteral("ribbonTabsRow"));
    m_ribbonTabsRow->setAttribute(Qt::WA_StyledBackground, true);
    auto* tr_ = new QHBoxLayout(m_ribbonTabsRow);
    tr_->setContentsMargins(8, 0, 6, 0);
    tr_->setSpacing(2);

    m_ribbonFile = new QToolButton(m_ribbonTabsRow);
    m_ribbonFile->setObjectName(QStringLiteral("ribbonFile"));
    m_ribbonFile->setText(tr("Arquivo"));
    m_ribbonFile->setCursor(Qt::PointingHandCursor);
    connect(m_ribbonFile, &QToolButton::clicked, this, &TopToolbar::showBackstage);
    m_ribbonFile->installEventFilter(this);   // a setinha do Q acompanha o tamanho
    tr_->addWidget(m_ribbonFile, 0, Qt::AlignBottom);
    m_ribbonQGap = new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    tr_->addItem(m_ribbonQGap);

    const QStringList names = { tr("Editor"), tr("Revisão"), tr("Mundo"), tr("Ambiente") };
    // Sem QButtonGroup exclusivo: no só abas, recolhida, nenhuma aba fica
    // marcada, e o grupo exclusivo não deixa desmarcar todas.
    for (int i = 0; i < names.size(); ++i) {
        auto* t = new QToolButton(m_ribbonTabsRow);
        t->setObjectName(QStringLiteral("ribbonTab"));
        t->setText(names.at(i));
        t->setCheckable(true);
        t->setCursor(Qt::PointingHandCursor);
        t->installEventFilter(this);
        m_ribbonTabs.append(t);
        tr_->addWidget(t, 0, Qt::AlignBottom);
        connect(t, &QToolButton::clicked, this, [this, i]() {
            if (m_barStyle == BarStyle::RibbonTabs) {
                const bool same = m_ribbonPeek && m_ribbonTab == i;
                setRibbonTab(i);
                setRibbonPeek(!same);
            } else {
                setRibbonTab(i);
            }
        });
    }
    tr_->addStretch(1);

    m_ribbonSave = new QToolButton(m_ribbonTabsRow);
    m_ribbonSave->setToolTip(tr("Salvar (Ctrl+S)"));
    m_ribbonSave->setCursor(Qt::PointingHandCursor);
    connect(m_ribbonSave, &QToolButton::clicked, this, [this]() { if (saveProjectButton) saveProjectButton->click(); });
    tr_->addWidget(m_ribbonSave, 0, Qt::AlignVCenter);

    m_ribbonModeBtn = new QToolButton(m_ribbonTabsRow);
    m_ribbonModeBtn->setToolTip(tr("Opções de exibição da faixa"));
    m_ribbonModeBtn->setCursor(Qt::PointingHandCursor);
    m_ribbonModeBtn->setPopupMode(QToolButton::InstantPopup);
    auto* modeMenu = new QMenu(m_ribbonModeBtn);
    auto* modeGroup = new QActionGroup(modeMenu);
    modeGroup->setExclusive(true);
    const QList<QPair<BarStyle, QString>> modes = {
        { BarStyle::RibbonFull,   tr("Abas completas") },
        { BarStyle::RibbonSimple, tr("Abas simples") },
        { BarStyle::RibbonAccent, tr("Abas em cartão") },
        { BarStyle::RibbonBinder, tr("Abas de pasta") },
        { BarStyle::RibbonReel,   tr("Giratória") },
        { BarStyle::RibbonTabs,   tr("Só as abas") },
        { BarStyle::Classic,      tr("Barra clássica") },
    };
    for (const auto& m : modes) {
        if (m.first == BarStyle::Classic) modeMenu->addSeparator();
        QAction* a = modeMenu->addAction(m.second);
        a->setCheckable(true);
        a->setData(int(m.first));
        modeGroup->addAction(a);
        const BarStyle st = m.first;
        connect(a, &QAction::triggered, this, [this, st]() {
            // Fora do clique do menu: a troca recria a faixa por baixo dele.
            QTimer::singleShot(0, this, [this, st]() { setBarStyle(st); });
        });
    }
    modeMenu->addSection(tr("Posição"));
    auto* placeGroup = new QActionGroup(modeMenu);
    placeGroup->setExclusive(true);
    for (const auto& pl : QList<QPair<bool, QString>>{ { false, tr("De ponta a ponta") }, { true, tr("Flutuante") } }) {
        QAction* a = modeMenu->addAction(pl.second);
        a->setCheckable(true);
        a->setData(pl.first ? -2 : -1);
        placeGroup->addAction(a);
        const bool floating = pl.first;
        connect(a, &QAction::triggered, this, [this, floating]() {
            QTimer::singleShot(0, this, [this, floating]() { setRibbonFloating(floating); });
        });
    }
    connect(modeMenu, &QMenu::aboutToShow, this, &TopToolbar::syncRibbonModeMenu);
    m_ribbonModeBtn->setMenu(modeMenu);
    tr_->addWidget(m_ribbonModeBtn, 0, Qt::AlignVCenter);
    v->addWidget(m_ribbonTabsRow);

    // ---- Corpo: as páginas num carrossel (janela estreita gira) ----
    m_ribbonBody = new QWidget(m_ribbon);
    m_ribbonBody->setObjectName(QStringLiteral("ribbonBody"));
    m_ribbonBody->setAttribute(Qt::WA_StyledBackground, true);
    auto* bl = new QHBoxLayout(m_ribbonBody);
    bl->setContentsMargins(4, 0, 4, 0);
    bl->setSpacing(0);
    m_ribbonCarousel = new CarouselViewport(Qt::Horizontal, m_ribbonBody);
    m_ribbonCarousel->setMinimumLength(160);
    m_ribbonStack = new QStackedWidget;
    m_ribbonStack->setObjectName(QStringLiteral("ribbonStack"));
    m_ribbonCarousel->setContent(m_ribbonStack);
    auto* reel = new RibbonReel(m_ribbonBody);
    reel->names = names;
    reel->onStep = [this](int d) { stepRibbonSection(d); };
    reel->onWheel = [this](int dy) { return wheelRibbonSection(dy); };
    reel->hide();
    m_ribbonReel = reel;
    bl->addWidget(reel, 0, Qt::AlignVCenter);
    bl->addWidget(m_ribbonCarousel, 1);
    m_ribbonFlip = new RibbonFlip(m_ribbonBody);
    m_ribbonBody->installEventFilter(this);   // roda do mouse em cima da faixa gira o rolo
    m_ribbonCollapse = new QToolButton(m_ribbonBody);
    m_ribbonCollapse->setToolTip(tr("Recolher a faixa"));
    m_ribbonCollapse->setCursor(Qt::PointingHandCursor);
    connect(m_ribbonCollapse, &QToolButton::clicked, this, [this]() {
        QTimer::singleShot(0, this, [this]() { setBarStyle(BarStyle::RibbonTabs); });
    });
    bl->addWidget(m_ribbonCollapse, 0, Qt::AlignBottom | Qt::AlignRight);
    v->addWidget(m_ribbonBody);

    m_ribbonParking = new QWidget(this);
    m_ribbonParking->hide();

    // ---- Botões que só a faixa tem ----
    struct AlignOpt { Qt::Alignment a; QString icon; QString tip; };
    const QList<AlignOpt> opts = {
        { Qt::AlignLeft,    QStringLiteral("align-left.svg"),    tr("Esquerda") },
        { Qt::AlignHCenter, QStringLiteral("align-center.svg"),  tr("Centro") },
        { Qt::AlignRight,   QStringLiteral("align-right.svg"),   tr("Direita") },
        { Qt::AlignJustify, QStringLiteral("align-justify.svg"), tr("Justificado") },
    };
    auto* alignGroup = new QButtonGroup(this);
    alignGroup->setExclusive(true);
    for (int i = 0; i < 4; ++i) {
        auto* b = new QToolButton(m_ribbonParking);
        b->setCheckable(true);
        b->setAutoRaise(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(opts.at(i).tip);
        b->setProperty("ribbonIcon", opts.at(i).icon);
        alignGroup->addButton(b);
        m_ribbonAlign[i] = b;
        const Qt::Alignment a = opts.at(i).a;
        connect(b, &QToolButton::clicked, this, [this, a]() {
            m_currentAlignment = a;
            updateAlignButtonIcon();
            emit alignmentRequested(a, m_alignScope);
            setCurrentAlignment(a);
        });
    }
    m_ribbonScope = new QToolButton(m_ribbonParking);
    m_ribbonScope->setToolTip(tr("Onde aplicar o alinhamento"));
    m_ribbonScope->setCursor(Qt::PointingHandCursor);
    m_ribbonScope->setPopupMode(QToolButton::InstantPopup);
    auto* scopeMenu = new QMenu(m_ribbonScope);
    scopeMenu->addSection(tr("Aplicar o alinhamento em"));
    auto* scopeGroup = new QActionGroup(scopeMenu);
    const QList<QPair<AlignScope, QString>> scopes = {
        { AlignScope::ThisDoc,    tr("Esse documento") },
        { AlignScope::AllDocs,    tr("Todos os documentos") },
        { AlignScope::Manuscript, tr("Somente manuscrito") },
        { AlignScope::Drawers,    tr("Somente gavetas") },
    };
    for (const auto& so : scopes) {
        QAction* a = scopeMenu->addAction(so.second);
        a->setCheckable(true);
        scopeGroup->addAction(a);
        const AlignScope scope = so.first;
        connect(a, &QAction::triggered, this, [this, scope]() { m_alignScope = scope; });
        connect(scopeMenu, &QMenu::aboutToShow, a, [this, a, scope]() { a->setChecked(m_alignScope == scope); });
    }
    m_ribbonScope->setMenu(scopeMenu);

    m_pageButton = new QToolButton(m_ribbonParking);
    m_pageButton->setToolTip(tr("Página e margens"));
    m_pageButton->setCursor(Qt::PointingHandCursor);
    m_pageButton->setProperty("ribbonIcon", QStringLiteral("page-setup.svg"));
    connect(m_pageButton, &QToolButton::clicked, this, &TopToolbar::showPagePopup);

    m_outerLayout->addWidget(m_ribbon);
    m_ribbon->hide();
}

void TopToolbar::syncRibbonModeMenu()
{
    if (!m_ribbonModeBtn || !m_ribbonModeBtn->menu()) return;
    for (QAction* a : m_ribbonModeBtn->menu()->actions()) {
        if (!a->isCheckable()) continue;
        const int d = a->data().toInt();
        if (d == -1) a->setChecked(!m_ribbonFloating);
        else if (d == -2) a->setChecked(m_ribbonFloating);
        else a->setChecked(d == int(m_barStyle));
    }
}

void TopToolbar::syncRibbonAlign()
{
    int idx = 0;
    if (m_currentAlignment & Qt::AlignHCenter) idx = 1;
    else if (m_currentAlignment & Qt::AlignRight) idx = 2;
    else if (m_currentAlignment & Qt::AlignJustify) idx = 3;
    if (m_ribbonAlign[idx]) m_ribbonAlign[idx]->setChecked(true);
}

void TopToolbar::setBarStyle(BarStyle style)
{
    if (style == m_barStyle) return;
    if (style != BarStyle::Classic && style != BarStyle::RibbonTabs) m_lastRibbonStyle = style;
    m_barStyle = style;
    QSettings().setValue(QStringLiteral("ui/topToolbarStyle"), int(style));
    applyBarStyle();
    emit barStyleChanged(style);
}

void TopToolbar::styleRibbonButton(QToolButton* b, int role, const QString& label)
{
    if (!b) return;
    if (!m_classicText.contains(b)) {
        m_classicText.insert(b, b->text());
        m_classicStyle.insert(b, b->toolButtonStyle());
    }
    b->setMinimumSize(0, 0);
    b->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    static const char* names[] = { "small", "big", "inline", "combo" };
    b->setProperty("ribbonRole", QString::fromLatin1(names[role]));
    switch (role) {
    case Big:
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setText(label);
        b->setIconSize(QSize(sc(28), sc(28)));
        b->setMinimumSize(sc(54), sc(66));
        b->setMaximumHeight(sc(66));
        break;
    case Inline:
        b->setToolButtonStyle(label.isEmpty() && b->text().isEmpty() ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
        if (!label.isEmpty()) b->setText(label);
        b->setIconSize(QSize(sc(18), sc(18)));
        b->setFixedHeight(sc(28));
        b->setMinimumWidth(sc(28));
        break;
    case Combo:
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        b->setFixedHeight(sc(26));
        break;
    default:
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        b->setIconSize(QSize(sc(18), sc(18)));
        b->setFixedSize(sc(28), sc(28));
        break;
    }
    // Ícone próprio da faixa (alinhamento, página): desenhado no tamanho certo.
    const QString own = b->property("ribbonIcon").toString();
    if (!own.isEmpty()) b->setIcon(toolbarIcon(own, b->iconSize().width()));
    style()->unpolish(b);
    style()->polish(b);
    b->installEventFilter(this);
}

void TopToolbar::restoreClassicButtons()
{
    for (auto it = m_classicText.constBegin(); it != m_classicText.constEnd(); ++it) {
        QToolButton* b = it.key();
        if (!b) continue;
        b->setText(it.value());
        b->setToolButtonStyle(m_classicStyle.value(b, Qt::ToolButtonIconOnly));
        b->setProperty("ribbonRole", QVariant());
        b->setProperty("ribbonCaret", QVariant());
        b->setProperty("comboLook", QVariant());
        if (auto* c = b->findChild<QLabel*>(QStringLiteral("ribbonCaret"))) delete c;
        b->setMinimumSize(0, 0);
        b->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        style()->unpolish(b);
        style()->polish(b);
    }
    m_classicText.clear();
    m_classicStyle.clear();
}

void TopToolbar::applyBarStyle()
{
    if (m_editMode) setEditMode(false);
    if (m_ribbonPeek) setRibbonPeek(false);
    // Moldura de painel (clássica) ou de teto (faixa). Antes de medir: o
    // setStyleSheet re-pole os botões e reaplicaria geometria do QSS por cima.
    applyRootStyle();

    if (ribbonActive()) {
        if (!m_ribbon) buildRibbonChrome();
        for (ToolbarGroupWidget* g : std::as_const(m_groupWidgets))
            if (g) g->clearButtons();
        if (m_carousel) m_carousel->hide();
        m_ribbon->show();
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        rebuildRibbonPages();
    } else {
        if (m_ribbon) {
            m_ribbon->hide();
            for (QToolButton* b : { m_ribbonAlign[0], m_ribbonAlign[1], m_ribbonAlign[2], m_ribbonAlign[3], m_ribbonScope, m_pageButton })
                if (b) b->setParent(m_ribbonParking);
        }
        if (m_carousel) m_carousel->show();
        restoreClassicButtons();
        rebuildGroupLayout();
        applyTypographyButtonMode();
        applyUiScale();
        setFontFamilies(fontFamilies, currentFontFamily);
    }
    refreshLayout();
    updateGeometry();
}

void TopToolbar::rebuildRibbonPages()
{
    if (!m_ribbon || !ribbonActive()) return;
    const bool full = !ribbonOneLine();   // só abas desce a completa

    // Todo mundo pro estacionamento antes de jogar as páginas fora.
    QList<QToolButton*> managed = m_buttonsById.values();
    for (QToolButton* b : { m_ribbonAlign[0], m_ribbonAlign[1], m_ribbonAlign[2], m_ribbonAlign[3], m_ribbonScope, m_pageButton })
        managed << b;
    for (QToolButton* b : std::as_const(managed)) {
        if (!b) continue;
        b->setParent(m_ribbonParking);
        // Setinha refeita a cada montagem (cor do tema, escala).
        if (auto* c = b->findChild<QLabel*>(QStringLiteral("ribbonCaret"))) delete c;
        b->setProperty("ribbonCaret", QVariant());
    }
    while (m_ribbonStack->count() > 0) {
        QWidget* w = m_ribbonStack->widget(0);
        m_ribbonStack->removeWidget(w);
        delete w;
    }
    m_ribbonTabOf.clear();

    // Rótulos curtos (o tooltip de cada botão continua com a dica completa).
    const QString assistant = miraAssistantName();
    const int sepH = full ? sc(58) : sc(22);

    auto put = [this](QBoxLayout* lay, QToolButton* b, int role, const QString& label, int tab) {
        if (!b) return;
        styleRibbonButton(b, role, label);
        lay->addWidget(b, 0, role == Big ? Qt::AlignTop : Qt::AlignVCenter);
        m_ribbonTabOf.insert(b, tab);
    };

    for (int tab = 0; tab < 4; ++tab) {
        auto* page = new QWidget;
        page->setObjectName(QStringLiteral("ribbonPage"));
        auto* row = new QHBoxLayout(page);
        row->setContentsMargins(full ? 0 : 4, 0, 4, 0);
        row->setSpacing(full ? 0 : 2);

        // Grupo da faixa completa: conteúdo em cima, nome embaixo.
        auto group = [&](const QString& title) -> QBoxLayout* {
            if (!full) {
                if (row->count() > 0) { row->addSpacing(sc(4)); row->addWidget(groupSep(page, sepH), 0, Qt::AlignVCenter); row->addSpacing(sc(4)); }
                return row;
            }
            if (row->count() > 0) row->addWidget(groupSep(page, sepH), 0, Qt::AlignVCenter);
            auto* g = new QWidget(page);
            auto* gv = new QVBoxLayout(g);
            gv->setContentsMargins(sc(8), sc(6), sc(8), 0);
            gv->setSpacing(0);
            auto* content = new QHBoxLayout;
            content->setSpacing(sc(3));
            gv->addLayout(content, 1);
            auto* lbl = new QLabel(title, g);
            lbl->setObjectName(QStringLiteral("ribbonGroupLabel"));
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setFixedHeight(sc(19));
            gv->addWidget(lbl);
            row->addWidget(g);
            return content;
        };
        // Duas fileiras empilhadas dentro de um grupo (Fonte, Parágrafo).
        auto stack2 = [&](QBoxLayout* parent, QHBoxLayout*& r1, QHBoxLayout*& r2) {
            auto* col = new QVBoxLayout;
            col->setSpacing(sc(6));
            r1 = new QHBoxLayout; r1->setSpacing(sc(2));
            r2 = new QHBoxLayout; r2->setSpacing(sc(2));
            col->addStretch(1);
            col->addLayout(r1);
            col->addLayout(r2);
            col->addStretch(1);
            parent->addLayout(col);
        };

        if (tab == 0) {   // Editor
            if (full) {
                QHBoxLayout *a, *b;
                stack2(group(tr("Fonte")), a, b);
                put(a, fontButton, Combo, QString(), tab);
                put(a, sizeButton, Combo, QString(), tab);
                for (QToolButton* x : { boldButton, italicButton, underlineButton, strikethroughButton }) put(b, x, Small, QString(), tab);
                b->addStretch(1);
                stack2(group(tr("Parágrafo")), a, b);
                for (QToolButton* x : m_ribbonAlign) put(a, x, Small, QString(), tab);
                put(a, m_ribbonScope, Small, QString(), tab);
                put(b, lineHeightButton, Inline, QString(), tab);
                put(b, indentButton, Small, QString(), tab);
                b->addStretch(1);
                put(group(tr("Página")), m_pageButton, Big, tr("Página"), tab);
                put(group(tr("Inserir")), imageButton, Big, tr("Imagem"), tab);
                put(group(tr("Edição")), searchButton, Big, tr("Buscar"), tab);
            } else {
                QBoxLayout* g = group(QString());
                put(g, fontButton, Combo, QString(), tab);
                put(g, sizeButton, Combo, QString(), tab);
                g = group(QString());
                for (QToolButton* x : { boldButton, italicButton, underlineButton, strikethroughButton }) put(g, x, Small, QString(), tab);
                g = group(QString());
                put(g, alignButton, Inline, QString(), tab);
                put(g, lineHeightButton, Inline, QString(), tab);
                put(g, indentButton, Small, QString(), tab);
                g = group(QString());
                put(g, m_pageButton, Inline, tr("Página"), tab);
                put(g, imageButton, Inline, tr("Imagem"), tab);
                put(g, searchButton, Inline, tr("Buscar"), tab);
            }
        } else if (tab == 1) {   // Revisão
            put(group(tr("Revisar")), repetitionsButton, full ? Big : Inline, tr("Repetições"), tab);
            if (full) put(group(tr("Ouvir")), readAloudButton, Big, tr("Ler em voz alta"), tab);
            else put(row, readAloudButton, Inline, tr("Ler em voz alta"), tab);
            put(group(tr("Contagem")), statisticsButton, full ? Big : Inline, tr("Estatísticas"), tab);
        } else if (tab == 2) {   // Mundo
            const int r = full ? Big : Inline;
            put(full ? group(tr("Criar")) : row, construtorButton, r, tr("Construtor"), tab);
            put(full ? group(tr("Anotar")) : row, pensarioButton, r, tr("Pensário"), tab);
            put(full ? group(tr("Consultar")) : row, refMenuButton, r, tr("Referência"), tab);
            put(full ? group(tr("Conversar")) : row, miraButton, r, assistant, tab);
        } else {   // Ambiente
            const int r = full ? Big : Inline;
            QBoxLayout* g = group(tr("Modos"));
            put(g, readModeButton, r, tr("Editor focado"), tab);
            put(g, focusButton, r, tr("Modo foco"), tab);
            g = group(tr("Sons e avisos"));
            put(g, immersiveSoundButton, r, tr("Som imersivo"), tab);
            put(g, reminderButton, r, tr("Lembretes"), tab);
            g = group(tr("Janela"));
            put(g, themePanelButton, r, tr("Temas"), tab);
            put(g, fullscreenButton, r, tr("Tela cheia"), tab);
        }
        row->addStretch(1);
        m_ribbonStack->addWidget(page);
    }

    // Rolo: some a linha das abas; o Q, o Salvar e o modo descem pra fileira,
    // fixos nas pontas, e o rolo entra logo depois do Q.
    const bool reelMode = (m_barStyle == BarStyle::RibbonReel);
    auto* trl0 = qobject_cast<QHBoxLayout*>(m_ribbonTabsRow->layout());
    auto* bl0 = qobject_cast<QHBoxLayout*>(m_ribbonBody->layout());
    if (reelMode != m_ribbonReelMode && trl0 && bl0) {
        m_ribbonReelMode = reelMode;
        for (QWidget* w : { static_cast<QWidget*>(m_ribbonFile), static_cast<QWidget*>(m_ribbonSave), static_cast<QWidget*>(m_ribbonModeBtn) }) {
            trl0->removeWidget(w);
            bl0->removeWidget(w);
        }
        if (reelMode) {
            bl0->insertWidget(0, m_ribbonFile, 0, Qt::AlignVCenter);
            const int ci = bl0->indexOf(m_ribbonCarousel);
            bl0->insertWidget(ci + 1, m_ribbonSave, 0, Qt::AlignVCenter);
            bl0->insertWidget(ci + 2, m_ribbonModeBtn, 0, Qt::AlignVCenter);
        } else {
            trl0->insertWidget(0, m_ribbonFile, 0, Qt::AlignBottom);
            trl0->addWidget(m_ribbonSave, 0, Qt::AlignVCenter);
            trl0->addWidget(m_ribbonModeBtn, 0, Qt::AlignVCenter);
        }
        for (QWidget* w : { static_cast<QWidget*>(m_ribbonFile), static_cast<QWidget*>(m_ribbonSave), static_cast<QWidget*>(m_ribbonModeBtn) })
            w->show();
    }
    m_ribbonTabsRow->setVisible(!reelMode);
    if (auto* reel = static_cast<RibbonReel*>(m_ribbonReel)) {
        reel->setVisible(reelMode);
        reel->setFixedSize(sc(74), sc(kReelBodyH));
        if (reelMode) { reel->setIndex(qBound(0, m_ribbonTab, 3), false); reel->update(); }
    }

    // Setinhas de "abre menu".
    for (QToolButton* b : { fontButton, sizeButton, lineHeightButton, alignButton, m_ribbonScope, m_pageButton }) {
        // Botão grande não leva setinha: ela cairia em cima do desenho.
        if (!b || b->parentWidget() == m_ribbonParking || b->property("ribbonRole").toString() == QLatin1String("big")) continue;
        addCaret(b);
        style()->unpolish(b);
        style()->polish(b);
    }

    // Tamanhos das caixas (depois do polish, que aplicaria min-width do QSS global).
    if (fontButton) fontButton->setFixedWidth(sc(156));
    if (sizeButton) sizeButton->setFixedWidth(sc(58));

    // Roupa da faixa: Word (padrão), sotaque ou fichário. Nas duas do Qenna,
    // o Q ocupa o lugar do Arquivo e abre o mesmo painel.
    const bool accent = (m_barStyle == BarStyle::RibbonAccent);
    // O Rolo veste a roupa do Fichário (Q sólido, caixas sem borda), sem as abas.
    const bool reelStyle = (m_barStyle == BarStyle::RibbonReel);
    const bool binder = (m_barStyle == BarStyle::RibbonBinder) || reelStyle;
    const QString look = accent ? QStringLiteral("accent") : binder ? QStringLiteral("binder") : QString();
    const bool qMark = accent || binder;
    auto repolish = [this](QWidget* w) { style()->unpolish(w); style()->polish(w); };
    m_ribbonTabsRow->setProperty("look", binder && !ribbonEdgeToEdge() ? QStringLiteral("binder-float") : look);
    m_ribbonBody->setProperty("look", look);
    repolish(m_ribbonTabsRow);
    repolish(m_ribbonBody);
    for (QToolButton* t : std::as_const(m_ribbonTabs)) { t->setProperty("look", look); repolish(t); }
    for (QToolButton* c : { fontButton, sizeButton }) {
        if (!c) continue;
        c->setProperty("comboLook", binder ? QVariant(QStringLiteral("binder")) : QVariant());
        repolish(c);
        c->setFixedWidth(c == fontButton ? sc(156) : sc(58));   // o polish reaplicaria o min-width do QSS
    }
    if (auto* trl = qobject_cast<QHBoxLayout*>(m_ribbonTabsRow->layout())) {
        trl->setContentsMargins(accent ? 0 : 8, 0, accent ? 0 : 6, 0);
        trl->setSpacing(binder ? sc(3) : 2);
        trl->setAlignment(m_ribbonFile, qMark ? Qt::AlignVCenter : Qt::AlignBottom);
    }
    if (auto* c = m_ribbonFile->findChild<QLabel*>(QStringLiteral("ribbonCaret"))) delete c;
    m_ribbonFile->setProperty("ribbonCaret", QVariant());
    m_ribbonFile->setProperty("qmark", qMark);
    if (qMark) {
        m_ribbonFile->setText(QString());
        const int qh = binder ? sc(18) : sc(21);
        m_ribbonFile->setIcon(solidQ(Theme::toColor(Theme::accentDefault()), qh));
        m_ribbonFile->setIconSize(QSize(qRound(qh * 483.0 / 727.0), qh));   // proporção da letra
        m_ribbonFile->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_ribbonFile->setToolTip(tr("Arquivo"));
        addCaret(m_ribbonFile);
    } else {
        m_ribbonFile->setText(tr("Arquivo"));
        m_ribbonFile->setIcon(QIcon());
        m_ribbonFile->setToolButtonStyle(Qt::ToolButtonTextOnly);
        m_ribbonFile->setToolTip(QString());
    }
    repolish(m_ribbonFile);
    applyRibbonMargins();
    applyBinderIndent();

    // Chrome da faixa na escala atual.
    const int ipx = sc(17);
    m_ribbonTabsRow->setFixedHeight(sc(tabsHFor(m_barStyle)));
    m_ribbonFile->setFixedHeight(binder ? sc(25) : qMark ? sc(30) : sc(26));
    if (!binder) for (QToolButton* t : std::as_const(m_ribbonTabs)) t->setFixedHeight(accent ? sc(30) : sc(29));
    for (QToolButton* q : { m_ribbonSave, m_ribbonModeBtn }) { q->setFixedSize(sc(28), sc(26)); q->setIconSize(QSize(ipx, ipx)); }
    m_ribbonSave->setIcon(toolbarIcon(QStringLiteral("save-project.svg"), ipx));
    m_ribbonModeBtn->setIcon(toolbarIcon(QStringLiteral("ribbon-mode.svg"), ipx));
    m_ribbonCollapse->setFixedSize(sc(20), sc(18));
    m_ribbonCollapse->setIconSize(QSize(sc(12), sc(12)));
    m_ribbonCollapse->setIcon(toolbarIcon(QStringLiteral("ribbon-collapse.svg"), sc(12)));
    m_ribbonCollapse->setVisible(m_barStyle == BarStyle::RibbonFull);

    const bool showBody = (m_barStyle != BarStyle::RibbonTabs) || m_ribbonPeek;
    m_ribbonBody->setFixedHeight(full ? sc(kFullBodyH) : reelStyle ? sc(kReelBodyH) : binder ? sc(kBinderBodyH) : sc(kSimpleBodyH));
    m_ribbonBody->setVisible(showBody);
    m_ribbon->setMinimumWidth(qMax(0, m_ribbonTargetWidth));

    reloadIcons();   // ícones no tamanho de cada botão (grande, pequeno, inline)
    for (QToolButton* b : std::as_const(managed)) if (b) placeCaret(b);
    placeCaret(m_ribbonFile);

    setRibbonTab(qBound(0, m_ribbonTab, 3));
    setFixedHeight(ribbonHeight(true));
    m_ribbonCarousel->updateGeometry();
    positionReminderBadge();
    positionPensarioBadge();
    positionModeBadges();
    updateGeometry();
    QCoreApplication::postEvent(this, new QEvent(QEvent::LayoutRequest));
}

bool TopToolbar::wheelRibbonSection(int angleDeltaY)
{
    if (!m_ribbonReelMode || angleDeltaY == 0) return false;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastReelWheel < 220) return true;   // um entalhe por vez
    m_lastReelWheel = now;
    stepRibbonSection(angleDeltaY < 0 ? 1 : -1);
    return true;
}

void TopToolbar::stepRibbonSection(int delta)
{
    auto* reel = static_cast<RibbonReel*>(m_ribbonReel);
    auto* flip = static_cast<RibbonFlip*>(m_ribbonFlip);
    if (!m_ribbonReelMode || delta == 0 || !reel || !flip || !m_ribbonStack) return;
    // As faces saem da própria barra (com o fundo dela), não do carrossel
    // transparente: assim o giro tem a cor certa em qualquer tema.
    flip->hide();
    const QRect area(m_ribbonCarousel->mapTo(this, QPoint(0, 0)), m_ribbonCarousel->size());
    const QPixmap from = grab(area);
    const int k = reel->index() + delta;
    reel->setIndex(k, true);
    setRibbonTab(((k % 4) + 4) % 4);
    if (QWidget* pg = m_ribbonStack->currentWidget()) if (pg->layout()) pg->layout()->activate();
    const QPixmap to = grab(area);
    flip->setGeometry(m_ribbonCarousel->geometry());
    flip->run(from, to, delta > 0 ? 1 : -1, QColor(Qt::transparent));
}

void TopToolbar::setRibbonTab(int idx)
{
    if (!m_ribbonStack || idx < 0 || idx >= m_ribbonStack->count()) return;
    m_ribbonTab = idx;
    m_ribbonStack->setCurrentIndex(idx);
    if (m_ribbonReelMode && m_ribbonReel) {
        auto* reel = static_cast<RibbonReel*>(m_ribbonReel);
        int d = ((idx - reel->index()) % 4 + 4) % 4;
        if (d == 3) d = -1;
        if (d != 0) reel->setIndex(reel->index() + d, false);
    }
    const bool showMark = (m_barStyle != BarStyle::RibbonTabs) || m_ribbonPeek;
    // Só abas, recolhida: nenhuma aba fica marcada.
    for (int i = 0; i < m_ribbonTabs.size(); ++i)
        m_ribbonTabs.at(i)->setChecked(showMark && i == idx);
    // Fichário: a aba aberta é mais alta e encosta na faixa; as de trás ficam baixas.
    if (m_barStyle == BarStyle::RibbonBinder)
        for (int i = 0; i < m_ribbonTabs.size(); ++i)
            m_ribbonTabs.at(i)->setFixedHeight(m_ribbonTabs.at(i)->isChecked() ? sc(25) : sc(22));
    if (QWidget* page = m_ribbonStack->currentWidget()) {
        for (QToolButton* b : page->findChildren<QToolButton*>()) placeCaret(b);
    }
}

void TopToolbar::setRibbonPeek(bool on)
{
    if (m_ribbonPeek == on) return;
    m_ribbonPeek = on;
    if (on && !m_peekFilter) { qApp->installEventFilter(this); m_peekFilter = true; }
    if (!on && m_peekFilter) { qApp->removeEventFilter(this); m_peekFilter = false; }
    if (m_ribbonBody) m_ribbonBody->setVisible(m_barStyle != BarStyle::RibbonTabs || on);
    setRibbonTab(m_ribbonTab);
    // A barra cresce POR CIMA do texto: thickness() continua a das abas.
    setFixedHeight(ribbonHeight(true));
    updateGeometry();
    // A MainWindow redimensiona o holder ao ver o LayoutRequest da barra.
    QCoreApplication::postEvent(this, new QEvent(QEvent::LayoutRequest));
    if (QWidget* holder = parentWidget()) holder->raise();
}

QRect TopToolbar::anchorRect(QToolButton* b) const
{
    if (!ribbonActive()) {
        revealButton(b);
        return QRect(b->mapToGlobal(QPoint(0, 0)), b->size());
    }
    auto* self = const_cast<TopToolbar*>(this);
    const int tab = m_ribbonTabOf.value(b, -1);
    if (tab < 0) return QRect(m_ribbonFile->mapToGlobal(QPoint(0, 0)), m_ribbonFile->size());
    if (m_barStyle == BarStyle::RibbonTabs && !m_ribbonPeek) {
        QToolButton* t = m_ribbonTabs.value(tab);
        return t ? QRect(t->mapToGlobal(QPoint(0, 0)), t->size()) : QRect();
    }
    if (tab != m_ribbonTab) self->setRibbonTab(tab);
    if (QWidget* page = m_ribbonStack->currentWidget()) if (page->layout()) page->layout()->activate();
    if (m_ribbonCarousel) m_ribbonCarousel->ensureVisible(b, false);
    return QRect(b->mapToGlobal(QPoint(0, 0)), b->size());
}

void TopToolbar::showBackstage()
{
    const QList<Backstage::Item> items = {
        { homeButton,        QStringLiteral("home.svg"),         QString(), tr("Início"),        tr("Volta pro Menu Principal, com a estante dos seus projetos."), QString() },
        { newProjectButton,  QStringLiteral("newproject.svg"),   QString(), tr("Novo projeto"),  tr("Começa um projeto do zero."), QString() },
        { openProjectButton, QStringLiteral("loadproject.svg"),  QString(), tr("Abrir projeto"), tr("Abre uma pasta de projeto do Qenna."), QString() },
        { saveProjectButton, QStringLiteral("save-project.svg"), QString(), tr("Salvar"),        tr("Grava o projeto agora."), QStringLiteral("Ctrl+S") },
        { exportButton,      QStringLiteral("download.svg"),     QString(), tr("Exportar"),      tr("ODT, PDF, EPUB e DOCX. Roteiro também sai em Fountain."), QString() },
        { nullptr, QString(), QString(), QString(), QString(), QString() },
        { helpButton,        QString(), QStringLiteral("?"),     tr("Ajuda"),         tr("O guia das funções do app."), QString() },
        { settingsButton,    QStringLiteral("settings.svg"),     QString(), tr("Configurações"), tr("Editor, interface, backup e o resto das preferências."), QString() },
    };
    auto* pop = new Backstage(this, items);
    pop->adjustSize();
    pop->move(m_ribbonFile->mapToGlobal(QPoint(0, m_ribbonFile->height() + 4)));
    pop->show();
}

void TopToolbar::showPagePopup()
{
    auto* pop = new QFrame(this, Qt::Popup);
    pop->setObjectName(QStringLiteral("ribbonPagePopup"));
    pop->setAttribute(Qt::WA_DeleteOnClose);
    pop->setStyleSheet(Theme::qss(QStringLiteral(R"(
        QFrame#ribbonPagePopup { background: %1; border: 1px solid %2; border-radius: @radius-panel; }
        QLabel { color: %3; font-size: 13px; }
        QLabel#pgHead { color: %4; font-size: 9px; font-weight: 700; letter-spacing: 1.5px; }
        QLabel#pgVal { color: %5; font-size: 12px; font-weight: 600; }
        QLabel#pgHint { color: %4; font-size: 12px; }
        QToolButton#pgSeg { background: transparent; border: 1px solid %2; color: %3; padding: 4px 8px; font-size: 12px; border-radius: @radius-control; }
        QToolButton#pgSeg:checked { background: %7; border-color: %7; color: %8; }
        QSlider::groove:horizontal { height: 4px; background: %6; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: %7; border-radius: 2px; }
        QSlider::handle:horizontal { background: %5; width: 12px; height: 12px; margin: -5px 0; border-radius: 6px; }
        QSlider::groove:horizontal:disabled, QSlider::sub-page:horizontal:disabled { background: %6; }
        QFrame#pgRule { background: %6; border: none; }
    )")).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(), Theme::textMuted(),
            Theme::textBright(), Theme::subtleBorder(), Theme::accentDefault(),
            inkOn(Theme::toColor(Theme::accentDefault())).name()));

    auto* v = new QVBoxLayout(pop);
    v->setContentsMargins(14, 10, 14, 12);
    v->setSpacing(6);
    auto* head = new QLabel(tr("PÁGINA"), pop);
    head->setObjectName(QStringLiteral("pgHead"));
    v->addWidget(head);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);
    v->addLayout(grid);

    auto* lm = EditorLayout::Manager::instance();
    const bool pageMode = QSettings().value(QStringLiteral("editor/pageMode"), false).toBool();

    int rowN = 0;
    grid->addWidget(new QLabel(tr("Modo de página"), pop), rowN, 0);
    auto* pm = new PageSwitch(pop);
    pm->setChecked(pageMode);
    pm->setCursor(Qt::PointingHandCursor);
    grid->addWidget(pm, rowN, 2, Qt::AlignRight);
    ++rowN;

    struct SliderRow { QSlider* s; QLabel* val; QLabel* name; };
    auto addSlider = [&](const QString& name, int min, int max, int step, int value) -> SliderRow {
        auto* l = new QLabel(name, pop);
        auto* s = new QSlider(Qt::Horizontal, pop);
        s->setRange(min, max);
        s->setSingleStep(step);
        s->setPageStep(step * 4);
        s->setValue(value);
        s->setFixedWidth(sc(150));
        auto* val = new QLabel(pop);
        val->setObjectName(QStringLiteral("pgVal"));
        val->setMinimumWidth(sc(66));
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(l, rowN, 0);
        grid->addWidget(s, rowN, 1);
        grid->addWidget(val, rowN, 2);
        ++rowN;
        return { s, val, l };
    };
    int hMax = pageHeightMaximum ? pageHeightMaximum() : EditorLayout::Manager::maxPageHeight();
    if (hMax <= kMinSheetHeight) hMax = EditorLayout::Manager::maxPageHeight();
    SliderRow width  = addSlider(tr("Largura"), EditorLayout::Manager::minPageWidth(), EditorLayout::Manager::maxPageWidth(), 20, lm->pageWidth());
    SliderRow height = addSlider(tr("Comprimento"), kMinSheetHeight, hMax, 20, lm->pageHeight() == 0 ? hMax : lm->pageHeight());
    SliderRow hMarg  = addSlider(tr("Margem lateral"), EditorLayout::Manager::minHorizontalMargin(), EditorLayout::Manager::maxHorizontalMargin(), 2, lm->horizontalMargin());
    SliderRow vMarg  = addSlider(tr("Margem topo/base"), EditorLayout::Manager::minVerticalMargin(), EditorLayout::Manager::maxVerticalMargin(), 2, lm->verticalMargin());

    auto px = [](int v) { return TopToolbar::tr("%1 px").arg(v); };
    auto refreshVals = [=]() {
        width.val->setText(px(width.s->value()));
        height.val->setText(height.s->value() >= hMax ? TopToolbar::tr("Tela cheia") : px(height.s->value()));
        hMarg.val->setText(px(hMarg.s->value()));
        vMarg.val->setText(px(vMarg.s->value()));
    };
    refreshVals();
    connect(width.s, &QSlider::valueChanged, pop, [=](int v) { lm->setPageWidth(qRound(v / 20.0) * 20); refreshVals(); });
    connect(height.s, &QSlider::valueChanged, pop, [=](int v) { lm->setPageHeight(v >= hMax ? 0 : v); refreshVals(); });
    connect(hMarg.s, &QSlider::valueChanged, pop, [=](int v) { lm->setHorizontalMargin(v); refreshVals(); });
    connect(vMarg.s, &QSlider::valueChanged, pop, [=](int v) { lm->setVerticalMargin(v); refreshVals(); });

    auto* hint = new QLabel(pop);
    hint->setObjectName(QStringLiteral("pgHint"));
    hint->setWordWrap(true);
    hint->setFixedWidth(sc(300));
    auto syncMode = [=](bool on) {
        // No modo de página a folha tem altura e margem próprias.
        for (QWidget* w : { static_cast<QWidget*>(height.s), static_cast<QWidget*>(height.name), static_cast<QWidget*>(height.val),
                            static_cast<QWidget*>(vMarg.s), static_cast<QWidget*>(vMarg.name), static_cast<QWidget*>(vMarg.val) })
            w->setEnabled(!on);
        hint->setText(on ? TopToolbar::tr("Folhas separadas, como no Word: a folha tem a margem dela, então comprimento e margem topo/base não se aplicam.")
                         : TopToolbar::tr("No comprimento máximo, a folha preenche a janela e acompanha o tamanho dela."));
    };
    syncMode(pageMode);
    connect(pm, &QCheckBox::toggled, pop, [this, syncMode](bool on) { syncMode(on); emit pageModeToggled(on); });

    auto* rule = new QFrame(pop);
    rule->setObjectName(QStringLiteral("pgRule"));
    rule->setFixedHeight(1);
    v->addSpacing(2);
    v->addWidget(rule);

    auto* brkRow = new QHBoxLayout;
    brkRow->setSpacing(4);
    brkRow->addWidget(new QLabel(tr("Quebra de cena"), pop), 1);
    auto* brkGroup = new QButtonGroup(pop);
    brkGroup->setExclusive(true);
    const QList<QPair<SceneBreaks::Style, QString>> brks = {
        { SceneBreaks::Knot, tr("Nó da Timeline") }, { SceneBreaks::Name, tr("Nome da cena") } };
    for (const auto& bk : brks) {
        auto* b = new QToolButton(pop);
        b->setObjectName(QStringLiteral("pgSeg"));
        b->setText(bk.second);
        b->setCheckable(true);
        b->setChecked(SceneBreaks::style() == bk.first);
        b->setCursor(Qt::PointingHandCursor);
        brkGroup->addButton(b);
        brkRow->addWidget(b);
        const SceneBreaks::Style st = bk.first;
        connect(b, &QToolButton::clicked, pop, [st]() { SceneBreaks::setStyle(st); });
    }
    v->addLayout(brkRow);
    v->addWidget(hint);

    pop->adjustSize();
    QPoint at = m_pageButton->mapToGlobal(QPoint(0, m_pageButton->height() + 4));
    if (QWidget* win = window()) {
        const int right = win->mapToGlobal(QPoint(win->width(), 0)).x() - 8;
        if (at.x() + pop->width() > right) at.setX(right - pop->width());
    }
    pop->move(at);
    pop->show();
}
