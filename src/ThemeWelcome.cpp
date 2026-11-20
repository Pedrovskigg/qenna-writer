#include "ThemeWelcome.h"

#include "Theme.h"
#include "ThemeScene.h"

#include <QApplication>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QToolTip>
#include <cmath>

namespace {

constexpr qreal kSceneW = ThemeScene::kSceneW;
constexpr qreal kSceneH = ThemeScene::kSceneH;
constexpr int kMaxBigScenes = 4;   // janelas inteiras guardadas (cada uma pesa uns MB)

const Theme::MiraTheme* findTheme(const QString& id)
{
    for (const auto& t : Theme::Manager::instance()->available())
        if (t.id == id) return &t;
    return nullptr;
}

QColor over(const QColor& top, const QColor& bottom)
{
    if (top.alpha() >= 255) return top;
    const qreal a = top.alphaF();
    return QColor::fromRgbF(top.redF() * a + bottom.redF() * (1 - a),
                            top.greenF() * a + bottom.greenF() * (1 - a),
                            top.blueF() * a + bottom.blueF() * (1 - a));
}

QColor withAlpha(QColor c, qreal a)
{
    c.setAlphaF(a);
    return c;
}

qreal luminance(const QColor& c)
{
    auto ch = [](qreal v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * ch(c.redF()) + 0.7152 * ch(c.greenF()) + 0.0722 * ch(c.blueF());
}

QColor lerp(const QColor& a, const QColor& b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

qreal approach(qreal v, qreal target, qreal k)
{
    const qreal n = v + (target - v) * k;
    return std::abs(target - n) < 0.002 ? target : n;
}

} // namespace

QStringList ThemeWelcome::themeIds()
{
    return { QStringLiteral("basalt"), QStringLiteral("tokyo-night"),          // Escuros
             QStringLiteral("tokyo-day"), QStringLiteral("pearl"),             // Claros
             QStringLiteral("gruvbox-dark"), QStringLiteral("sepia"),          // Amarelados
             QStringLiteral("one-dark-skyline"), QStringLiteral("instant-film") };   // Estampados
}

ThemeWelcome::ThemeWelcome(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    for (const QString& id : themeIds()) {
        if (const auto* t = findTheme(id)) { m_ids << id; m_names << t->name; }
    }
    m_pal = m_palTarget = palFor(QString());
    m_anim.setInterval(16);
    connect(&m_anim, &QTimer::timeout, this, &ThemeWelcome::tick);
    m_spinTimer.setSingleShot(true);
    connect(&m_spinTimer, &QTimer::timeout, this, &ThemeWelcome::spinStep);
    connect(ThemeScene::ImageCache::instance(), &ThemeScene::ImageCache::ready, this, [this]() { update(); });
    if (parent) parent->installEventFilter(this);
    hide();
}

void ThemeWelcome::open()
{
    if (parentWidget()) setGeometry(parentWidget()->rect());
    relayout();
    m_show = 0.0;
    m_showTarget = 1.0;
    show();
    raise();
    setFocus();
    kick();
}

ThemeWelcome::Pal ThemeWelcome::palFor(const QString& themeId)
{
    const Theme::MiraTheme& t = themeId.isEmpty() || !findTheme(themeId) ? Theme::Manager::instance()->current()
                                                                         : *findTheme(themeId);
    Pal p;
    p.app = over(Theme::toColor(t.appBackground), QColor(Qt::black));
    p.panel = over(Theme::toColor(t.panelBackground), p.app);
    p.line = over(Theme::toColor(t.panelBorder), p.panel);
    p.ink = Theme::toColor(t.textPrimary);
    p.muted = Theme::toColor(t.textMuted);
    p.accent = Theme::toColor(t.accentDefault);
    p.hover = withAlpha(p.ink, 0.06);
    p.subtle = withAlpha(p.ink, 0.12);
    p.onAccent = luminance(p.accent) > 0.3 ? QColor(0x14, 0x14, 0x14) : QColor(Qt::white);
    return p;
}

ThemeWelcome::Pal ThemeWelcome::mix(const Pal& a, const Pal& b, qreal t)
{
    Pal p;
    p.panel = lerp(a.panel, b.panel, t);
    p.line = lerp(a.line, b.line, t);
    p.ink = lerp(a.ink, b.ink, t);
    p.muted = lerp(a.muted, b.muted, t);
    p.accent = lerp(a.accent, b.accent, t);
    p.app = lerp(a.app, b.app, t);
    p.hover = lerp(a.hover, b.hover, t);
    p.subtle = lerp(a.subtle, b.subtle, t);
    p.onAccent = lerp(a.onAccent, b.onAccent, t);
    return p;
}

// ---------------------------------------------------------------- layout

void ThemeWelcome::relayout()
{
    const qreal W = width(), H = height();
    const qreal dlgW = std::min<qreal>(1060, W - 48);
    const int cols = dlgW >= 760 ? 4 : 2;
    const qreal padX = 28, padTop = 26, padBottom = 20, gap = 18, colGap = 16, rowGap = 18;
    const qreal inner = dlgW - padX * 2;
    const qreal colW = (inner - colGap * (cols - 1)) / cols;

    QFont titleF(QStringLiteral("Lora"));
    titleF.setPixelSize(24);
    titleF.setWeight(QFont::DemiBold);
    QFont subF = QApplication::font();
    subF.setPixelSize(13);
    const qreal titleH = QFontMetricsF(titleF).height() + 4;
    const qreal subW = std::min<qreal>(inner, 560);
    const QString sub = tr("É com ele que você vai escrever. Passe o mouse num tema pra ver a janela inteira com ele atrás.");
    const qreal subH = QFontMetricsF(subF).boundingRect(QRectF(0, 0, subW, 1000), Qt::TextWordWrap, sub).height();

    const qreal headH = 14, nameH = 18, footH = 15 + 34;
    const int groupRows = cols == 4 ? 1 : 2;
    qreal shotW = colW - 12;
    qreal shotH = shotW * 505.0 / 960.0;
    auto cardHFor = [&](qreal sh) { return 6 + sh + 7 + nameH + 6; };
    auto groupHFor = [&](qreal sh) { return headH + 10 + cardHFor(sh) * 2 + 10; };
    const qreal fixedH = padTop + titleH + 6 + subH + gap + gap + footH + padBottom + (groupRows - 1) * rowGap;
    const qreal availGroups = (H - 48) - fixedH;
    if (groupHFor(shotH) * groupRows > availGroups) {
        // Janela baixa: as miniaturas encolhem pra caber, centradas na coluna.
        const qreal perGroup = availGroups / groupRows;
        shotH = std::max<qreal>(40, ((perGroup - headH - 10 - 10) / 2) - (6 + 7 + nameH + 6));
        shotW = std::min(colW - 12, shotH * 960.0 / 505.0);
        shotH = shotW * 505.0 / 960.0;
    }
    const qreal cardH = cardHFor(shotH);
    const qreal groupH = groupHFor(shotH);
    const qreal total = fixedH + groupH * groupRows;

    m_dlg = QRectF((W - dlgW) / 2, std::max<qreal>(24, (H - total) / 2), dlgW, total);
    const qreal x0 = m_dlg.x() + padX;
    qreal y = m_dlg.y() + padTop;
    m_title = QRectF(x0, y, inner, titleH);
    y += titleH + 6;
    m_sub = QRectF(x0, y, subW, subH);
    y += subH + gap;

    m_cards.clear(); m_shots.clear(); m_heads.clear();
    for (int g = 0; g < 4; ++g) {
        const int gr = g / cols, gc = g % cols;
        const qreal gx = x0 + gc * (colW + colGap);
        const qreal gy = y + gr * (groupH + rowGap);
        m_heads << QRectF(gx, gy, colW, headH);
        for (int k = 0; k < 2; ++k) {
            const QRectF card(gx, gy + headH + 10 + k * (cardH + 10), colW, cardH);
            m_cards << card;
            m_shots << QRectF(card.x() + (colW - shotW) / 2, card.y() + 6, shotW, shotH);
        }
    }
    y += groupH * groupRows + (groupRows - 1) * rowGap + gap;
    m_footLine = QRectF(x0, y, inner, 1);
    y += 15;

    QFont btnF = QApplication::font();
    btnF.setPixelSize(13);
    btnF.setWeight(QFont::DemiBold);
    const QFontMetricsF bfm(btnF);
    // O botão tem a largura do texto dele (muda com o tema escolhido).
    const QString goText = m_picked >= 0 ? tr("Começar com %1").arg(m_names.at(m_picked)) : tr("Escolha um tema");
    const qreal goW = bfm.horizontalAdvance(goText) + 32;
    m_go = QRectF(m_dlg.right() - padX - goW, y, goW, 34);
    const qreal skipW = bfm.horizontalAdvance(tr("Ignorar")) + 32;
    m_skip = QRectF(m_go.left() - 8 - skipW, y, skipW, 34);
    m_more = QRectF(x0, y, std::max<qreal>(0, m_skip.left() - 14 - x0), 34);
    update();
}

int ThemeWelcome::cardAt(const QPointF& p) const
{
    for (int i = 0; i < m_cards.size() && i < m_ids.size(); ++i)
        if (m_cards.at(i).contains(p)) return i;
    return -1;
}

// ---------------------------------------------------------------- estado

void ThemeWelcome::setFocusTheme(const QString& id)
{
    if (id == m_focus) return;
    m_prevFocus = m_focus;
    m_focus = id;
    m_bgT = 0.0;
    m_palTarget = palFor(id);
    kick();
}

void ThemeWelcome::pick(int card)
{
    if (card < 0 || card >= m_ids.size()) return;
    m_picked = card;
    setFocusTheme(m_ids.at(card));
    relayout();   // o botão "Começar com …" muda de largura
}

void ThemeWelcome::finish(const QString& id, bool random)
{
    if (m_done) return;
    m_done = true;
    m_awayTarget = 0.0;
    setFocusTheme(id);
    // Aplica por baixo da camada (o app repinta escondido) e só então some com ela.
    emit chosen(id, random);
    m_showTarget = 0.0;
    kick();
}

void ThemeWelcome::startSpin()
{
    if (m_spinning || m_done || m_ids.isEmpty()) return;
    m_spinning = true;
    m_awayTarget = 0.0;
    m_spinTarget = QRandomGenerator::global()->bounded(int(m_ids.size()));
    m_spinStep = 0;
    spinStep();
}

// Roletinha do Ignorar: passa pelos cartões, freando, e para no sorteado.
void ThemeWelcome::spinStep()
{
    const int n = int(m_ids.size());
    const int steps = n * 2 + m_spinTarget;
    if (m_spinStep > steps) {
        m_spinCard = -1;
        pick(m_spinTarget);
        m_spinning = false;
        finish(m_ids.at(m_spinTarget), true);
        return;
    }
    m_spinCard = m_spinStep % n;
    setFocusTheme(m_ids.at(m_spinCard));
    update();
    if (m_spinStep == steps) {
        ++m_spinStep;
        m_spinTimer.start(450);   // respira no sorteado antes de fechar
        return;
    }
    ++m_spinStep;
    m_spinTimer.start(int(45 + m_spinStep * m_spinStep * 0.9));
}

void ThemeWelcome::kick()
{
    if (!m_anim.isActive()) m_anim.start();
    update();
}

void ThemeWelcome::tick()
{
    const qreal k = 0.2;
    const bool peek = m_hover >= 0 || m_spinning || m_done;
    m_dimTarget = peek ? 0.28 : 0.55;
    m_dim = approach(m_dim, m_dimTarget, k);
    m_away = approach(m_away, m_awayTarget, k);
    m_show = approach(m_show, m_showTarget, m_done ? 0.16 : 0.22);
    m_bgT = approach(m_bgT, 1.0, 0.18);
    m_pal = mix(m_pal, m_palTarget, 0.18);
    update();
    if (m_done && m_show <= 0.0) {
        m_anim.stop();
        hide();
        deleteLater();
        return;
    }
    if (m_dim == m_dimTarget && m_away == m_awayTarget && m_show == m_showTarget && m_bgT >= 1.0
        && m_pal.panel == m_palTarget.panel && m_pal.ink == m_palTarget.ink && m_pal.accent == m_palTarget.accent) {
        m_pal = m_palTarget;
        m_anim.stop();
    }
}

QString ThemeWelcome::shownName() const
{
    if (!m_focus.isEmpty())
        if (const auto* t = findTheme(m_focus)) return t->name;
    return Theme::Manager::instance()->current().name;
}

// A janela do Qenna desenhada com o tema (a mesma do painel de Temas). Com o
// fundo da janela inteira, cobre a camada como object-fit: cover.
QPixmap ThemeWelcome::sceneFor(const QString& id, const QSize& size, bool thumb)
{
    const Theme::MiraTheme* t = findTheme(id);
    if (!t || size.isEmpty()) return QPixmap();
    const qreal dpr = devicePixelRatioF();
    const QString key = QStringLiteral("%1|%2x%3|%4").arg(id).arg(size.width()).arg(size.height()).arg(thumb);
    if (const auto it = m_sceneCache.constFind(key); it != m_sceneCache.constEnd()) return *it;

    const QPixmap bg = t->backgroundImage.isEmpty() ? QPixmap()
        : ThemeScene::ImageCache::instance()->get(t->backgroundImage, thumb ? ThemeScene::ImageCache::kThumbCap
                                                                             : ThemeScene::ImageCache::kSceneCap);
    const bool waiting = !t->backgroundImage.isEmpty() && bg.isNull();

    QPixmap pm(QSize(qCeil(size.width() * dpr), qCeil(size.height() * dpr)));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Theme::toColor(t->appBackground));
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal s = std::max(size.width() / kSceneW, size.height() / kSceneH);
    const QRectF r((size.width() - kSceneW * s) / 2, (size.height() - kSceneH * s) / 2, kSceneW * s, kSceneH * s);
    ThemeScene::paintScene(p, r, *t, ThemeScene::Writing, bg);
    p.end();

    if (!waiting) {   // sem a foto ainda: desenha de novo quando ela chegar
        if (!thumb) {
            int big = 0;
            for (auto it = m_sceneCache.cbegin(); it != m_sceneCache.cend(); ++it)
                if (it.key().endsWith(QLatin1String("|0"))) ++big;
            if (big >= kMaxBigScenes)
                for (auto it = m_sceneCache.begin(); it != m_sceneCache.end();)
                    it = it.key().endsWith(QLatin1String("|0")) ? m_sceneCache.erase(it) : std::next(it);
        }
        m_sceneCache.insert(key, pm);
    }
    return pm;
}

// ---------------------------------------------------------------- pintura

void ThemeWelcome::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setOpacity(m_show);

    // Fundo: o tema em foco (cruzando com o anterior). Sem foco, o app de verdade.
    const QSize full = size();
    if (!m_prevFocus.isEmpty() && m_bgT < 1.0) {
        p.save();
        p.setOpacity(m_show * (m_focus.isEmpty() ? 1.0 - m_bgT : 1.0));
        p.drawPixmap(0, 0, sceneFor(m_prevFocus, full, false));
        p.restore();
    }
    if (!m_focus.isEmpty()) {
        p.save();
        p.setOpacity(m_show * m_bgT);
        p.drawPixmap(0, 0, sceneFor(m_focus, full, false));
        p.restore();
    }
    p.fillRect(rect(), withAlpha(Qt::black, m_dim * (1.0 - m_away)));

    const Pal& c = m_pal;
    const qreal dlgOpacity = m_show * (1.0 - m_away);
    if (dlgOpacity > 0.001) {
        p.save();
        p.setOpacity(dlgOpacity);
        const qreal sc = 1.0 - 0.015 * m_away;
        p.translate(m_dlg.center());
        p.scale(sc, sc);
        p.translate(-m_dlg.center());

        // Sombra macia (camadas) e o painel.
        p.setPen(Qt::NoPen);
        for (int i = 16; i >= 1; --i) {
            p.setBrush(withAlpha(Qt::black, 0.018));
            p.drawRoundedRect(m_dlg.adjusted(-i * 2.2, -i * 1.2, i * 2.2, i * 3.2), 14 + i * 2, 14 + i * 2);
        }
        p.setBrush(c.panel);
        p.setPen(QPen(c.line, 1));
        p.drawRoundedRect(m_dlg.adjusted(0.5, 0.5, -0.5, -0.5), 14, 14);

        QFont titleF(QStringLiteral("Lora"));
        titleF.setPixelSize(24);
        titleF.setWeight(QFont::DemiBold);
        p.setFont(titleF);
        p.setPen(c.ink);
        p.drawText(m_title, Qt::AlignLeft | Qt::AlignVCenter, tr("Escolha o visual do seu Qenna"));

        QFont ui = QApplication::font();
        ui.setPixelSize(13);
        p.setFont(ui);
        p.setPen(c.muted);
        p.drawText(m_sub, Qt::TextWordWrap,
                   tr("É com ele que você vai escrever. Passe o mouse num tema pra ver a janela inteira com ele atrás."));

        QFont head = QApplication::font();
        head.setPixelSize(11);
        head.setWeight(QFont::DemiBold);
        head.setLetterSpacing(QFont::PercentageSpacing, 108);
        const QStringList groups = { tr("Escuros"), tr("Claros"), tr("Amarelados"), tr("Estampados") };
        p.setFont(head);
        for (int g = 0; g < m_heads.size(); ++g)
            p.drawText(m_heads.at(g), Qt::AlignLeft | Qt::AlignVCenter, groups.value(g).toUpper());

        for (int i = 0; i < m_cards.size() && i < m_ids.size(); ++i) {
            const QRectF card = m_cards.at(i);
            const bool picked = i == m_picked, spin = i == m_spinCard, hov = i == m_hover;
            if (picked || spin || hov) {
                p.setBrush(c.hover);
                p.setPen(picked || spin ? QPen(spin ? c.ink : c.accent, 1) : Qt::NoPen);
                p.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
            }
            const QRectF shot = m_shots.at(i);
            QPainterPath clip;
            clip.addRoundedRect(shot, 7, 7);
            p.save();
            p.setClipPath(clip);
            p.drawPixmap(shot.topLeft(), sceneFor(m_ids.at(i), shot.size().toSize(), true));
            p.restore();
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(withAlpha(c.ink, 0.08), 1));
            p.drawRoundedRect(shot.adjusted(0.5, 0.5, -0.5, -0.5), 7, 7);

            const QRectF nameR(card.x() + 8, shot.bottom() + 7, card.width() - 16, 18);
            p.setFont(ui);
            p.setPen(c.ink);
            p.drawText(nameR.adjusted(0, 0, -22, 0), Qt::AlignLeft | Qt::AlignVCenter, m_names.at(i));
            const QRectF tick(nameR.right() - 16, nameR.center().y() - 8, 16, 16);
            if (picked) {
                p.setPen(Qt::NoPen);
                p.setBrush(c.accent);
                p.drawEllipse(tick);
                QPen check(c.onAccent, 1.8);
                check.setCapStyle(Qt::RoundCap);
                check.setJoinStyle(Qt::RoundJoin);
                p.setPen(check);
                p.setBrush(Qt::NoBrush);
                const QPointF o = tick.center();
                p.drawPolyline(QPolygonF({ o + QPointF(-3.5, 0), o + QPointF(-1, 2.5), o + QPointF(3.5, -2.5) }));
            } else {
                p.setPen(QPen(c.line, 1));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(tick.adjusted(0.5, 0.5, -0.5, -0.5));
            }
        }

        p.fillRect(m_footLine, c.line);
        QFont small = QApplication::font();
        small.setPixelSize(12);
        p.setFont(small);
        p.setPen(c.muted);
        const QString more = tr("São mais de 500 no painel de Temas. Troque ou crie novos quando quiser.");
        p.drawText(m_more, Qt::AlignLeft | Qt::AlignVCenter, QFontMetricsF(small).elidedText(more, Qt::ElideRight, m_more.width()));

        QFont btn = QApplication::font();
        btn.setPixelSize(13);
        if (m_overSkip && !m_spinning && !m_done) {
            p.setPen(Qt::NoPen);
            p.setBrush(c.hover);
            p.drawRoundedRect(m_skip, 7, 7);
        }
        p.setFont(btn);
        p.setPen(m_overSkip ? c.ink : c.muted);
        p.drawText(m_skip, Qt::AlignCenter, tr("Ignorar"));

        const bool can = m_picked >= 0;
        p.setPen(Qt::NoPen);
        p.setBrush(can ? (m_overGo ? c.accent.lighter(108) : c.accent) : c.subtle);
        p.drawRoundedRect(m_go, 7, 7);
        btn.setWeight(QFont::DemiBold);
        p.setFont(btn);
        p.setPen(can ? c.onAccent : c.muted);
        p.drawText(m_go, Qt::AlignCenter, can ? tr("Começar com %1").arg(m_names.at(m_picked)) : tr("Escolha um tema"));
        p.restore();
    }

    // Mouse fora: a pílula avisa que a janela só saiu da frente.
    if (m_away > 0.001) {
        QFont small = QApplication::font();
        small.setPixelSize(12);
        const QString text = tr("Assim fica o %1. Volte o mouse pra escolher.").arg(shownName());
        const QFontMetricsF fm(small);
        const qreal w = fm.horizontalAdvance(text) + 28, h = 32;
        const QRectF pill((width() - w) / 2, height() - 22 - h + (1.0 - m_away) * 6, w, h);
        p.save();
        p.setOpacity(m_show * m_away);
        p.setPen(QPen(c.line, 1));
        p.setBrush(c.panel);
        p.drawRoundedRect(pill.adjusted(0.5, 0.5, -0.5, -0.5), h / 2, h / 2);
        p.setFont(small);
        p.setPen(c.ink);
        p.drawText(pill, Qt::AlignCenter, text);
        p.restore();
    }
}

// ---------------------------------------------------------------- mouse e teclado

void ThemeWelcome::mouseMoveEvent(QMouseEvent* e)
{
    if (m_done) return;
    const QPointF pos = e->position();
    const bool inside = m_dlg.contains(pos);
    if (m_spinning) { setCursor(Qt::ArrowCursor); return; }

    if (inside) {
        setProperty("twEntered", true);
        m_awayTarget = 0.0;
        const int card = cardAt(pos);
        if (card != m_hover) {
            m_hover = card;
            if (card >= 0) setFocusTheme(m_ids.at(card));
            else setFocusTheme(m_picked >= 0 ? m_ids.at(m_picked) : QString());
        }
        m_overSkip = m_skip.contains(pos);
        m_overGo = m_go.contains(pos);
        setCursor((card >= 0 || m_overSkip || (m_overGo && m_picked >= 0)) ? Qt::PointingHandCursor : Qt::ArrowCursor);
    } else {
        m_hover = -1;
        m_overSkip = m_overGo = false;
        setCursor(Qt::ArrowCursor);
        // Fora da janela de escolha (depois de ter passado por ela): ela sai da frente.
        if (property("twEntered").toBool()) m_awayTarget = 1.0;
    }
    kick();
}

void ThemeWelcome::leaveEvent(QEvent*)
{
    if (m_done || m_spinning) return;
    m_hover = -1;
    m_overSkip = m_overGo = false;
    if (property("twEntered").toBool()) m_awayTarget = 1.0;
    kick();
}

void ThemeWelcome::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton || m_done || m_spinning) return;
    const QPointF pos = e->position();
    if (!m_dlg.contains(pos)) return;
    const int card = cardAt(pos);
    if (card >= 0) { pick(card); return; }
    if (m_skip.contains(pos)) { QToolTip::hideText(); startSpin(); return; }
    if (m_go.contains(pos) && m_picked >= 0) finish(m_ids.at(m_picked), false);
}

void ThemeWelcome::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton || m_done || m_spinning) return;
    const int card = cardAt(e->position());
    if (card < 0) return;
    pick(card);
    finish(m_ids.at(card), false);
}

void ThemeWelcome::keyPressEvent(QKeyEvent* e)
{
    // Sem Esc: não tem "pular". Enter começa com o escolhido.
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && m_picked >= 0 && !m_done && !m_spinning) {
        finish(m_ids.at(m_picked), false);
        return;
    }
    e->accept();
}

bool ThemeWelcome::event(QEvent* e)
{
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        if (!m_done && !m_spinning && m_away < 0.5 && m_skip.contains(he->pos()))
            QToolTip::showText(he->globalPos(), tr("Pula a escolha, um tema será escolhido aleatoriamente"), this, m_skip.toRect());
        else
            QToolTip::hideText();
        return true;
    }
    return QWidget::event(e);
}

bool ThemeWelcome::eventFilter(QObject* watched, QEvent* e)
{
    if (watched == parentWidget() && e->type() == QEvent::Resize) {
        setGeometry(parentWidget()->rect());
        m_sceneCache.clear();
        relayout();
    }
    return QWidget::eventFilter(watched, e);
}
