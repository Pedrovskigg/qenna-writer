#include "DocHeaderBar.h"

#include <QEnterEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QResizeEvent>
#include <QToolButton>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include <climits>
#include <cmath>
#include <functional>

#include "AvatarUtils.h"
#include "DialogueVoices.h"
#include "IconUtils.h"
#include "Theme.h"
#include "UiScale.h"

namespace {
// Comporta o bloco de duas linhas (título + subtítulo) com folga.
constexpr int kHeightBase = 54;
constexpr int kHeightFloor = 40;
constexpr int kSideMargin = 16;
constexpr int kVarBtnGap = 4;
constexpr int kVarBtnBase = 20;
// Cabe dentro da altura fixa da faixa (54 - 8 de margem) com folga.
constexpr int kAvatarBase = 38;
constexpr int kAvatarGap = 10;
// Abaixo disso o guia do roteiro mostra só o elemento, sem as teclas.
constexpr int kGuideKeysMinWidth = 700;
constexpr int kGuideGap = 12;

qreal luminance(const QColor& c)
{
    auto ch = [](qreal v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * ch(c.redF()) + 0.7152 * ch(c.greenF()) + 0.0722 * ch(c.blueF());
}

qreal contrast(const QColor& a, const QColor& b)
{
    const qreal la = luminance(a), lb = luminance(b);
    return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
}
}

// Aviso do Granna (ver DocHeaderBar::showGrannaMark). Fora do layout, por cima
// da faixa: um único progresso 0..1 desenha o gesto inteiro, e o tempo de cada
// estado vem de quanto o Granna tem certeza.
class GrannaBadge : public QWidget
{
public:
    using Mark = DocHeaderBar::GrannaMark;

    explicit GrannaBadge(QWidget* parent) : QWidget(parent)
    {
        setCursor(Qt::PointingHandCursor);
        m_anim = new QVariantAnimation(this);
        m_anim->setStartValue(0.0);
        m_anim->setEndValue(1.0);
        QObject::connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            m_t = v.toReal();
            update();
        });
        QObject::connect(m_anim, &QVariantAnimation::finished, this, [this]() {
            // Fixo: estacionou no meio do gesto (inteiro) e espera release().
            if (m_sticky || m_hold) return;
            finishHide();
        });
        hide();
    }

    std::function<void()> onClick;
    std::function<void()> onAnnounceEnded; // aviso que some sozinho acabou

    void configure(Mark mark, const QPixmap& face, const QColor& voice, const QColor& accent,
                   int side, int pad, int slide)
    {
        m_mark = mark;
        m_face = face;
        m_voice = voice;
        m_accent = accent;
        m_side = side;
        m_pad = pad;
        m_slide = slide;
    }
    void setClipRight(int x) { m_clipRight = x; update(); }

    // sticky = fica: faz só a entrada e estaciona no meio do gesto (t = 0,5),
    // até release(). Senão faz o gesto inteiro e some (aviso).
    void start(bool sticky)
    {
        m_anim->stop();
        m_sticky = sticky;
        m_announce = !sticky;
        m_hold = false;
        m_t = 0.0;
        m_anim->setStartValue(0.0);
        m_anim->setEndValue(sticky ? 0.5 : 1.0);
        m_anim->setDuration(sticky ? fullDuration() / 2 : fullDuration());
        show();
        raise();
        m_anim->start();
    }
    // Fixo: faz a saída a partir de onde está.
    void release()
    {
        if (isHidden() || !m_sticky || m_hold) return;
        continueOut();
    }
    void stopNow()
    {
        m_anim->stop();
        m_hold = false;
        m_sticky = false;
        m_announce = false;
        hide();
    }
    // Popup aberto: o aviso fica inteiro e parado até soltar.
    void setHold(bool hold)
    {
        if (hold == m_hold) return;
        m_hold = hold;
        if (hold) {
            m_anim->stop();
            m_t = 0.5;
            show();
            update();
            return;
        }
        if (!m_sticky && isVisible()) continueOut();
    }

protected:
    void enterEvent(QEnterEvent* event) override
    {
        QWidget::enterEvent(event);
        // Mouse em cima segura o aviso que ia sumir, pra dar tempo de clicar.
        if (!m_sticky && m_anim->state() == QAbstractAnimation::Running) m_anim->pause();
    }
    void leaveEvent(QEvent* event) override
    {
        QWidget::leaveEvent(event);
        if (!m_hold && m_anim->state() == QAbstractAnimation::Paused) m_anim->resume();
    }
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && onClick) onClick();
        event->accept();
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        // Nasce "de trás" do título: nada passa da borda esquerda do texto.
        p.setClipRect(QRect(0, 0, qMax(0, m_clipRight), height()));
        const qreal t = m_t;
        const qreal cy = height() / 2.0;
        const qreal s = m_side / 38.0;

        if (m_mark == Mark::Unknown) {
            // Anel: o (?) acende, pulsa duas vezes enquanto o anel se fecha
            // em volta, e apaga.
            const qreal op = t < 0.18 ? t / 0.18 : (t > 0.8 ? (1.0 - t) / 0.2 : 1.0);
            const qreal cx = m_pad + m_side / 2.0;
            const QRectF disc(cx - m_side / 2.0, cy - m_side / 2.0, m_side, m_side);
            QColor soft = m_accent;
            soft.setAlphaF(0.16);
            p.setOpacity(op);
            p.setPen(QPen(m_accent, 1.5 * s));
            p.setBrush(soft);
            p.drawEllipse(disc.adjusted(0.75 * s, 0.75 * s, -0.75 * s, -0.75 * s));

            qreal pulse = 1.0;
            if (t > 0.18 && t < 0.78) {
                const qreal w = std::sin((t - 0.18) / 0.6 * 2.0 * M_PI);
                pulse = 1.0 - 0.6 * w * w;
            }
            p.setOpacity(op * pulse);
            QFont f = font();
            f.setBold(true);
            f.setPixelSize(qMax(9, qRound(m_side * 0.5)));
            p.setFont(f);
            p.setPen(m_accent);
            p.drawText(disc, Qt::AlignCenter, QStringLiteral("?"));

            p.setOpacity(op);
            const qreal ring = qMin(1.0, t / 0.48);
            QPen pen(m_accent, 2.0 * s);
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            const qreal gap = 4.0 * s;
            p.drawArc(disc.adjusted(-gap, -gap, gap, gap), 90 * 16, -qRound(ring * 360 * 16));
            return;
        }

        // Deslizar: sai de trás do título pro lado, segura, e volta.
        qreal off = 0.0, op = 1.0;
        if (t < 0.28) {
            const qreal u = 1.0 - t / 0.28;
            const qreal e = 1.0 - u * u * u;
            off = m_slide * (1.0 - e);
            op = e;
        } else if (t > 0.72) {
            const qreal u = (t - 0.72) / 0.28;
            const qreal e = u * u * u;
            off = m_slide * e;
            op = 1.0 - e;
        }
        p.setOpacity(op);
        const qreal cx = m_pad + m_side / 2.0 + off;
        const QSizeF fs = m_face.deviceIndependentSize();
        p.drawPixmap(QPointF(cx - fs.width() / 2.0, cy - fs.height() / 2.0), m_face);
        if (m_mark == Mark::Probable) {
            QPen pen(m_voice, 1.6 * s);
            pen.setDashPattern({ 2.4, 1.7 });
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            const qreal r = m_side / 2.0 + 0.5 * s;
            p.drawEllipse(QPointF(cx, cy), r, r);
        }
    }

private:
    QVariantAnimation* m_anim = nullptr;
    Mark m_mark = Mark::Certain;
    QPixmap m_face;
    QColor m_voice;
    QColor m_accent;
    int m_side = 38;
    int m_pad = 6;
    int m_slide = 18;
    int m_clipRight = INT_MAX;
    qreal m_t = 0.0;
    bool m_hold = false;
    bool m_sticky = false;
    bool m_announce = false;

    int fullDuration() const
    {
        return m_mark == Mark::Certain ? 650 : m_mark == Mark::Probable ? 1000 : 1500;
    }
    void continueOut()
    {
        m_anim->stop();
        m_sticky = false;
        m_anim->setStartValue(m_t);
        // Ainda entrando (cursor passou rápido pela fala): volta pelo mesmo
        // caminho em vez de fazer o gesto inteiro.
        const bool back = m_t < 0.5;
        m_anim->setEndValue(back ? 0.0 : 1.0);
        m_anim->setDuration(qMax(60, qRound(fullDuration() * (back ? m_t : 1.0 - m_t))));
        m_anim->start();
    }
    void finishHide()
    {
        hide();
        const bool wasAnnounce = m_announce;
        m_announce = false;
        if (wasAnnounce && onAnnounceEnded) onAnnounceEnded();
    }
};

DocHeaderBar::DocHeaderBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("docHeaderBar"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(kSideMargin, 4, kSideMargin, 4);
    outer->setSpacing(0);
    outer->addStretch(1);

    // Avatar e bloco de texto no MESMO container, centralizado como um grupo
    // só: assim o título de capítulo (que nunca tem foto) continua exatamente
    // onde sempre esteve, e o de personagem apenas ganha a foto à esquerda —
    // sem sincronizar posição de nada por fora.
    auto* centerRow = new QHBoxLayout;
    centerRow->setContentsMargins(0, 0, 0, 0);
    centerRow->setSpacing(kAvatarGap);

    m_avatar = new QLabel(this);
    m_avatar->setObjectName(QStringLiteral("docHeaderAvatar"));
    m_avatar->setVisible(false);
    m_avatar->installEventFilter(this);
    centerRow->addWidget(m_avatar, 0, Qt::AlignVCenter);

    m_textCol = new QVBoxLayout;
    m_textCol->setContentsMargins(0, 0, 0, 0);
    m_textCol->setSpacing(0);

    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("docHeaderTitle"));
    m_title->setAlignment(Qt::AlignCenter);
    m_textCol->addWidget(m_title, 0, Qt::AlignHCenter);

    // Subtítulo e botão de variação viajam juntos: no bloco da toolbar o botão
    // fica colado à direita do subtítulo, e a linha inteira centraliza como uma
    // coisa só, sem deslocar o título de cima.
    m_subRow = new QHBoxLayout;
    auto* subRow = m_subRow;
    subRow->setContentsMargins(0, 0, 0, 0);
    subRow->setSpacing(kVarBtnGap);

    m_subtitle = new QLabel(this);
    m_subtitle->setObjectName(QStringLiteral("docHeaderSubtitle"));
    m_subtitle->setAlignment(Qt::AlignCenter);
    subRow->addWidget(m_subtitle, 0, Qt::AlignVCenter);

    m_varButton = new QToolButton(this);
    m_varButton->setObjectName(QStringLiteral("docHeaderVar"));
    m_varButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_varButton->setAutoRaise(true);
    m_varButton->setCursor(Qt::PointingHandCursor);
    m_varButton->setToolTip(tr("Variações desta cena"));
    m_varButton->setVisible(false);
    connect(m_varButton, &QToolButton::clicked, this, &DocHeaderBar::sceneVarRequested);
    subRow->addWidget(m_varButton, 0, Qt::AlignVCenter);

    m_textCol->addLayout(subRow);
    m_textCol->setAlignment(subRow, Qt::AlignHCenter);

    centerRow->addLayout(m_textCol);
    outer->addLayout(centerRow);
    outer->setAlignment(centerRow, Qt::AlignHCenter);
    outer->addStretch(1);

    m_guide = new QWidget(this);
    m_guide->setObjectName(QStringLiteral("docHeaderGuide"));
    m_guide->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_guide->setToolTip(tr("Elemento do roteiro na linha do cursor. Tab e Shift+Tab trocam o elemento."));
    auto* guideCol = new QVBoxLayout(m_guide);
    guideCol->setContentsMargins(0, 0, 0, 0);
    guideCol->setSpacing(1);
    m_guideElement = new QLabel(m_guide);
    m_guideElement->setObjectName(QStringLiteral("docHeaderGuideElement"));
    m_guideElement->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_guideKeys = new QLabel(m_guide);
    m_guideKeys->setObjectName(QStringLiteral("docHeaderGuideKeys"));
    m_guideKeys->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    guideCol->addWidget(m_guideElement, 0, Qt::AlignRight);
    guideCol->addWidget(m_guideKeys, 0, Qt::AlignRight);
    m_guide->setVisible(false);

    // Self-contained: o MainWindow não precisa lembrar de repassar tema/escala.
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &DocHeaderBar::applyTheme);
    connect(UiScale::Manager::instance(), &UiScale::Manager::scaleChanged,
            this, &DocHeaderBar::applyUiScale);

    applyUiScale();
    applyTheme();
}

void DocHeaderBar::applyUiScale()
{
    const qreal s = UiScale::scale();
    setFixedHeight(qMax(kHeightFloor, qRound(kHeightBase * s)));
    const int varSize = qMax(14, qRound(kVarBtnBase * s));
    m_varButton->setFixedSize(varSize, varSize);
    const int icoPx = qMax(10, qRound(14 * s));
    m_varButton->setIconSize(QSize(icoPx, icoPx));
    applyTheme(); // ícone é re-renderizado no tamanho novo
    refreshAvatar();
    relayoutText();
}

void DocHeaderBar::applyTheme()
{
    // Mesma cor E mesma opacidade da folha (ver MainWindow::applyEditorStyle):
    // a faixa é irmã do editor no layout, então sem isto apareceria uma emenda
    // visível entre ela e o começo da página.
    const int opacity = qBound(0, Theme::editorOpacity(), 100);
    QColor bg(Theme::editorBackground());
    bg.setAlpha((opacity * 255) / 100);
    const QString bgCss = QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(bg.red()).arg(bg.green()).arg(bg.blue())
        .arg(QString::number(bg.alphaF(), 'f', 3));

    // A faixa fica em cima da FOLHA, não do chrome: o texto tem que sair das
    // cores do editor. Com textBright/textMuted, tema de mesa escura e papel
    // claro (Mahogany: #f4e0cc sobre #efe2cc) deixava o título invisível.
    const QColor ink(Theme::editorTextColor());
    const QColor paper(Theme::editorBackground());
    const auto mix = [&](qreal t) {
        return QColor::fromRgbF(ink.redF()   * (1 - t) + paper.redF()   * t,
                                ink.greenF() * (1 - t) + paper.greenF() * t,
                                ink.blueF()  * (1 - t) + paper.blueF()  * t).name();
    };
    // O tema pode dar ao título cor e fonte próprias (Criador de Temas). O
    // subtítulo acompanha a cor do título, só mais apagado.
    const QColor head = Theme::toColor(Theme::docHeaderColor());
    const QString titleColor = head.isValid() ? head.name() : ink.name();
    const QString subtitleColor = Theme::hasDocHeaderColor() && head.isValid()
        ? QColor::fromRgbF(head.redF() * 0.55 + paper.redF() * 0.45,
                           head.greenF() * 0.55 + paper.greenF() * 0.45,
                           head.blueF() * 0.55 + paper.blueF() * 0.45).name()
        : mix(0.45);
    const QString family = Theme::docHeaderFont();
    const QString fontStack = family == QLatin1String("Lora")
        ? QStringLiteral("'Lora','Crimson Text',serif")
        : QStringLiteral("'%1','Lora',serif").arg(family);
    m_placeholderColor = QColor(mix(0.55));
    const QString hover = QStringLiteral("rgba(%1,%2,%3,0.08)")
        .arg(ink.red()).arg(ink.green()).arg(ink.blue());
    // O elemento do roteiro acende na cor de destaque do tema, se ela tiver
    // contraste com o papel; senão fica na cor do título.
    const QColor accent(Theme::accentDefault());
    const QString guideColor = (accent.isValid() && contrast(accent, paper) >= 3.0)
        ? accent.name() : titleColor;
    m_grannaAccent = QColor(guideColor);

    setStyleSheet(Theme::qss(QStringLiteral(R"(
        QWidget#docHeaderBar { background: %1; }
        QLabel#docHeaderTitle {
            color: %2;
            background: transparent;
            font-family: %5;
            font-size: 15px;
            font-weight: 700;
        }
        QLabel#docHeaderSubtitle {
            color: %3;
            background: transparent;
            font-family: %5;
            font-size: 11px;
            font-weight: 500;
        }
        QToolButton#docHeaderVar {
            background: transparent;
            border: none;
            border-radius: @radius-control;
        }
        QToolButton#docHeaderVar:hover { background: %4; }
        QWidget#docHeaderGuide { background: transparent; }
        QLabel#docHeaderGuideElement {
            color: %6;
            background: transparent;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 1px;
        }
        QLabel#docHeaderGuideKeys {
            color: %3;
            background: transparent;
            font-size: 10px;
        }
    )")).arg(bgCss, titleColor, subtitleColor, hover, fontStack, guideColor));

    m_varButton->setIcon(IconUtils::loadToolbarIcon(
        QStringLiteral(":/icons/scene-var.svg"),
        QColor(subtitleColor), QColor(mix(0.2)), ink,
        m_varButton->iconSize()));
    refreshAvatar(); // o círculo vazio usa a cor do tema
    refreshGuide();
    relayoutText();  // a fonte do título pode ter mudado de largura
}

void DocHeaderBar::setScreenplayGuide(bool visible, ScreenplayElement element, bool emptyLine)
{
    if (visible == m_guideWanted && element == m_guideEl && emptyLine == m_guideEmpty) return;
    const bool reserveChanged = visible != m_guideWanted;
    m_guideWanted = visible;
    m_guideEl = element;
    m_guideEmpty = emptyLine;
    refreshGuide();
    if (reserveChanged) relayoutText();
}

void DocHeaderBar::refreshGuide()
{
    if (!m_guide) return;
    if (!m_guideWanted) { m_guide->setVisible(false); return; }

    m_guideElement->setText(ScreenplayFormat::label(m_guideEl).toUpper());
    // Pra onde cada tecla leva (mesmas regras do SpellEditor). Enter numa
    // linha vazia que não é Ação só troca a linha pra Ação.
    const ScreenplayElement onEnter = (m_guideEmpty && m_guideEl != ScreenplayElement::Action)
        ? ScreenplayElement::Action : ScreenplayFormat::nextElement(m_guideEl);
    const QString enterTo = ScreenplayFormat::label(onEnter);
    const QString tabTo = ScreenplayFormat::label(ScreenplayFormat::cycleElement(m_guideEl));
    const QString backTo = ScreenplayFormat::label(ScreenplayFormat::cycleElement(m_guideEl, true));
    const QString arrow = QStringLiteral(" \u2192 ");
    const QString dot = QStringLiteral("   \u00b7   ");
    QString keys = enterTo == tabTo
        ? QStringLiteral("Enter / Tab") + arrow + enterTo
        : QStringLiteral("Enter") + arrow + enterTo + dot + QStringLiteral("Tab") + arrow + tabTo;
    keys += dot + QStringLiteral("Shift+Tab") + arrow + backTo;
    m_guideKeys->setText(keys);
    m_guideKeys->setVisible(width() >= kGuideKeysMinWidth);
    m_guide->adjustSize();
    m_guide->move(width() - kSideMargin - m_guide->width(), (height() - m_guide->height()) / 2);
    m_guide->setVisible(true);
    m_guide->raise();
}

int DocHeaderBar::guideReserve() const
{
    if (!m_guideWanted || !m_guide) return 0;
    return m_guide->sizeHint().width() + kGuideGap;
}

void DocHeaderBar::setDocumentTitle(const QString& title, const QString& subtitle)
{
    m_rawTitle = title;
    m_rawSubtitle = subtitle;
    m_subtitleWanted = !subtitle.isEmpty();
    relayoutText();
}

void DocHeaderBar::setDocumentAvatar(const QString& imageDataUrl)
{
    if (m_avatarDataUrl == imageDataUrl) return;
    m_avatarDataUrl = imageDataUrl;
    refreshAvatar();
    relayoutText(); // a foto come largura útil da elipse do título
}

void DocHeaderBar::setAvatarEditable(bool editable)
{
    if (m_avatarEditable == editable) return;
    m_avatarEditable = editable;
    m_avatar->setCursor(editable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    refreshAvatar();
    relayoutText();
}

void DocHeaderBar::refreshAvatar()
{
    if (!m_avatar) return;
    const qreal s = UiScale::scale();
    const int side = qMax(16, qRound(kAvatarBase * s));

    if (m_avatarDataUrl.isEmpty()) {
        m_avatar->clear();
        if (!m_avatarEditable) {
            m_avatar->setVisible(false);
            return;
        }
        // Lugar reservado mesmo sem foto: se o círculo entrasse no layout só
        // no hover, o nome pularia do centro pro lado a cada passada do mouse.
        m_avatar->setFixedSize(side, side);
        m_avatar->setToolTip(tr("Clique duas vezes para adicionar uma foto"));
        if (m_hover) {
            const qreal dpr = devicePixelRatioF();
            QPixmap pm(qRound(side * dpr), qRound(side * dpr));
            pm.setDevicePixelRatio(dpr);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing, true);
            QPen dash(m_placeholderColor, 1.2, Qt::DashLine);
            dash.setDashPattern({ 3, 3 });
            p.setPen(dash);
            p.drawEllipse(QRectF(1, 1, side - 2, side - 2));
            const qreal c = side / 2.0, arm = side * 0.16;
            p.setPen(QPen(m_placeholderColor, 1.4, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(c - arm, c), QPointF(c + arm, c));
            p.drawLine(QPointF(c, c - arm), QPointF(c, c + arm));
            p.end();
            m_avatar->setPixmap(pm);
        }
        m_avatar->setVisible(true);
        return;
    }
    m_avatar->setToolTip(m_avatarEditable ? tr("Clique duas vezes para trocar a foto") : QString());

    // Renderiza na resolução física e marca o DPR: em tela HiDPI, gerar no
    // tamanho lógico deixaria a foto borrada — que é justamente o efeito
    // "desfalcado" que se quer evitar aqui.
    const qreal dpr = devicePixelRatioF();
    QPixmap pm = AvatarUtils::circularAvatar(m_avatarDataUrl, QString(), QString(),
                                              qRound(side * dpr));
    pm.setDevicePixelRatio(dpr);

    m_avatar->setFixedSize(side, side);
    m_avatar->setPixmap(pm);
    m_avatar->setVisible(true);
}

void DocHeaderBar::setSceneVarButtonVisible(bool visible)
{
    m_varWanted = visible;
    relayoutText();
}

void DocHeaderBar::relayoutText()
{
    if (!m_title || !m_subtitle || !m_varButton) return;

    const bool hasAvatar = m_avatar && m_avatar->isVisible();

    // Com foto, o texto se alinha à esquerda dela (nome e papel empilhados,
    // como num crachá); sem foto, segue centralizado como sempre foi.
    if (m_textCol) {
        const Qt::Alignment ha = hasAvatar ? Qt::AlignLeft : Qt::AlignHCenter;
        m_textCol->setAlignment(m_title, ha | Qt::AlignVCenter);
        if (m_subRow) m_textCol->setAlignment(m_subRow, ha);
        m_title->setAlignment(hasAvatar ? (Qt::AlignLeft | Qt::AlignVCenter) : Qt::AlignCenter);
        m_subtitle->setAlignment(hasAvatar ? (Qt::AlignLeft | Qt::AlignVCenter) : Qt::AlignCenter);
    }

    const int avatarW = hasAvatar ? m_avatar->width() + kAvatarGap : 0;
    // O guia do roteiro mora no canto direito; o título continua centralizado,
    // então a folga sai dos dois lados.
    const int avail = qMax(40, width() - kSideMargin * 2 - avatarW - guideReserve() * 2);

    m_title->setVisible(!m_rawTitle.isEmpty());
    m_title->setText(QFontMetrics(m_title->font()).elidedText(m_rawTitle, Qt::ElideRight, avail));

    // Botão de variação só faz sentido junto do subtítulo — é a variação DAQUELA
    // cena. Mesma regra da toolbar.
    const bool showVar = m_subtitleWanted && m_varWanted;
    m_varButton->setVisible(showVar);
    m_subtitle->setVisible(m_subtitleWanted);
    if (m_subtitleWanted) {
        const int varW = showVar ? kVarBtnGap + m_varButton->width() : 0;
        m_subtitle->setText(QFontMetrics(m_subtitle->font())
                                .elidedText(m_rawSubtitle, Qt::ElideRight, qMax(20, avail - varW)));
    } else {
        m_subtitle->clear();
    }
    placeGrannaMark(); // o aviso encosta no texto, que acabou de mudar
}

QRect DocHeaderBar::sceneVarButtonGlobalRect() const
{
    if (!m_varButton || !m_varButton->isVisible()) return QRect();
    return QRect(m_varButton->mapToGlobal(QPoint(0, 0)), m_varButton->size());
}

bool DocHeaderBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_avatar && m_avatarEditable && event->type() == QEvent::MouseButtonDblClick) {
        emit avatarChangeRequested();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void DocHeaderBar::enterEvent(QEnterEvent* event)
{
    QWidget::enterEvent(event);
    m_hover = true;
    if (m_avatarEditable && m_avatarDataUrl.isEmpty()) refreshAvatar();
}

void DocHeaderBar::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    m_hover = false;
    if (m_avatarEditable && m_avatarDataUrl.isEmpty()) refreshAvatar();
}

void DocHeaderBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    refreshGuide();  // canto direito e teclas dependem da largura
    relayoutText(); // a elipse depende da largura disponível
}

void DocHeaderBar::showGrannaMark(GrannaMark mark, const QString& imageDataUrl, const QString& name,
                                  const QColor& voice, const QString& toolTip, bool sticky)
{
    if (!m_granna) {
        m_granna = new GrannaBadge(this);
        m_granna->onClick = [this]() { emit grannaMarkClicked(); };
        m_granna->onAnnounceEnded = [this]() { emit grannaAnnounceEnded(); };
    }
    const qreal s = UiScale::scale();
    const int side = qMax(16, qRound(kAvatarBase * s));
    QPixmap face;
    if (mark == GrannaMark::Certain) face = DialogueVoices::face(imageDataUrl, name, voice, side);
    else if (mark == GrannaMark::Probable)
        face = DialogueVoices::face(imageDataUrl, name, voice, side - qRound(6 * s));
    m_granna->configure(mark, face, voice, m_grannaAccent.isValid() ? m_grannaAccent : voice,
                        side, grannaPad(), grannaSlide());
    m_granna->setToolTip(toolTip);
    placeGrannaMark();
    m_granna->start(sticky);
}

void DocHeaderBar::releaseGrannaMark()
{
    if (m_granna) m_granna->release();
}

void DocHeaderBar::hideGrannaMark()
{
    if (m_granna) m_granna->stopNow();
}

void DocHeaderBar::holdGrannaMark(bool hold)
{
    if (m_granna) m_granna->setHold(hold);
}

QRect DocHeaderBar::grannaMarkGlobalRect() const
{
    if (!m_granna || m_granna->isHidden())
        return QRect(mapToGlobal(QPoint(width() / 2, height())), QSize(1, 1));
    return QRect(m_granna->mapToGlobal(QPoint(0, 0)), m_granna->size());
}

int DocHeaderBar::grannaPad() const { return qMax(4, qRound(6 * UiScale::scale())); }
int DocHeaderBar::grannaSlide() const { return qMax(8, qRound(18 * UiScale::scale())); }

void DocHeaderBar::placeGrannaMark()
{
    if (!m_granna) return;
    if (layout()) layout()->activate();
    // Borda esquerda do TEXTO (o QLabel pode ser mais largo que ele).
    int left = INT_MAX;
    for (QLabel* l : { m_title, m_subtitle }) {
        if (!l || l->isHidden() || l->text().isEmpty()) continue;
        const int w = qMin(l->width(), QFontMetrics(l->font()).horizontalAdvance(l->text()));
        left = qMin(left, l->geometry().center().x() - w / 2);
    }
    if (left == INT_MAX) left = width() / 2;
    const int side = qMax(16, qRound(kAvatarBase * UiScale::scale()));
    const int pad = grannaPad();
    const int w = pad * 2 + side + grannaSlide();
    const int h = side + pad * 2;
    const int x = left - qRound(kAvatarGap * UiScale::scale()) - side - pad;
    m_granna->setGeometry(x, (height() - h) / 2, w, h);
    m_granna->setClipRight(left - 2 - x);
}
