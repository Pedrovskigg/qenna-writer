#include "LeftBar.h"
#include "IconUtils.h"
#include "PanelMotion.h"
#include "ProjectModel.h"
#include "Theme.h"
#include "ToolbarGroupWidget.h"
#include "UiScale.h"

#include <QApplication>
#include <QColor>
#include <QCursor>
#include <QEnterEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QSettings>
#include <QTimer>
#include <QVariantAnimation>
#include <functional>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFrame>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QString separatorQss() {
    return QStringLiteral(
        "background: %1; border: none; max-height: 1px; margin: 2px 6px;"
    ).arg(Theme::subtleBorder());
}
}

namespace {
constexpr const char* kDrawerMime = "application/x-mira-drawer-key";
}

namespace {

// Valores-base (escala 1.0). O tamanho de verdade em pixels vem das funções
// abaixo, que aplicam o fator de UiScale — permite o slider de "Tamanho da
// interface" nas Configurações reescalar a barra em tempo real.
constexpr int kBarWidthBase = 60;
constexpr int kBtnSizeBase  = 40;
constexpr int kIconSizeBase = 26;

int barWidthPx() { return qMax(40, qRound(kBarWidthBase * UiScale::scale())); }
int btnSizePx()  { return qMax(24, qRound(kBtnSizeBase  * UiScale::scale())); }
int iconSizePx() { return qMax(14, qRound(kIconSizeBase * UiScale::scale())); }

QIcon loadLeftbarIcon(const QString& fileName) {
    const int px = iconSizePx();
    return IconUtils::loadToolbarIcon(
        QStringLiteral(":/icons/leftbar/%1").arg(fileName),
        QColor(Theme::textMuted()),
        QColor(Theme::textPrimary()),
        QColor(Theme::textBright()),
        QSize(px, px));
}

QString fixedButtonQss() {
    // Sem fundo no estado normal; borda translúcida no hover; outline em
    // accent + leve tint no estado checked (= painel aberto). Sem mudança
    // de tamanho — todas as bordas têm 1px reservado em transparent.
    return Theme::qss(QStringLiteral(R"(
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: @radius-control;
            color: %1;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 15px;
            font-weight: 600;
        }
        QToolButton:hover {
            background: %2;
            border-color: %3;
            color: %4;
        }
        QToolButton:checked {
            background: %6;
            border-color: %5;
            color: %4;
        }
    )")).arg(Theme::textMuted(),
            Theme::hoverOverlay(),
            Theme::subtleBorder(),
            Theme::textBright(),
            Theme::accentDefault(),
            Theme::pressedOverlay());
}

QString drawerButtonQss(const QString& accent) {
    // Letra colorida com o accent da gaveta; fundo sempre transparente.
    // Hover e checked usam a própria cor da gaveta como borda.
    return Theme::qss(QStringLiteral(R"(
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: @radius-control;
            color: %1;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 17px;
            font-weight: 700;
        }
        QToolButton:hover {
            background: %3;
            border-color: %4;
            color: %2;
        }
        QToolButton:checked {
            background: %5;
            border-color: %1;
            color: %2;
        }
    )")).arg(accent, Theme::textBright(),
           Theme::hoverOverlay(), Theme::subtleBorder(),
           Theme::pressedOverlay());
}

QString newDrawerQss() {
    return Theme::qss(QStringLiteral(R"(
        QToolButton {
            background: transparent;
            color: %1;
            border: 1px dashed %2;
            border-radius: @radius-control;
            font-size: 22px;
            font-weight: 300;
        }
        QToolButton:hover {
            color: %3;
            border-color: %4;
            background: %5;
        }
    )")).arg(Theme::textMuted(),
            Theme::panelBorder(),
            Theme::textBright(),
            Theme::textMuted(),
            Theme::pressedOverlay());
}

QFrame* makeGroupSeparator(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    line->setStyleSheet(separatorQss());
    return line;
}

} // namespace

// ============================================================ Etiquetas
// Passando o mouse na barra, uma faixa sai da borda dela com o nome de cada
// botão (alinhado a ele), o título dos grupos em cima dos separadores e
// quantos itens cada gaveta tem. A barra de verdade continua embaixo, inteira:
// arrasto de gaveta, reorganizar e menus seguem funcionando nela.

namespace {
constexpr int kLabelsShadow = 14;
constexpr int kLabelsOpenDelayMs = 280;
// A faixa tem a largura do nome mais comprido, entre estes limites.
int labelsMinWidthPx() { return qRound(104 * UiScale::scale()); }
int labelsMaxWidthPx() { return qRound(176 * UiScale::scale()); }
}

class LeftBarLabels : public QWidget {
public:
    struct Row {
        int y = 0, h = 0;
        QString text;
        QString count;
        bool active = false;
        std::function<void()> click;
        std::function<void(const QPoint&)> context;
    };
    struct Head { int y = 0; QString text; };

    explicit LeftBarLabels(QWidget* parent) : QWidget(parent) {
        setMouseTracking(true);
        setAttribute(Qt::WA_NoSystemBackground);
        hide();
    }

    std::function<void()> onLeave;

    void setContent(const QList<Row>& rows, const QList<Head>& heads, bool mirrored) {
        m_rows = rows;
        m_heads = heads;
        m_mirrored = mirrored;
        m_hover = -1;
        update();
    }

    void open(bool animate) {
        stopAnim();
        if (!animate) { m_reveal = 1.0; update(); return; }
        m_reveal = 0.0;
        auto* a = new QVariantAnimation(this);
        a->setDuration(170);
        a->setStartValue(0.0);
        a->setEndValue(1.0);
        a->setEasingCurve(QEasingCurve::OutCubic);
        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_reveal = v.toReal(); update(); });
        m_anim = a;
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }

    void close(bool animate) {
        stopAnim();
        if (!animate || !isVisible()) { hide(); return; }
        auto* a = new QVariantAnimation(this);
        a->setDuration(110);
        a->setStartValue(m_reveal);
        a->setEndValue(0.0);
        a->setEasingCurve(QEasingCurve::InCubic);
        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_reveal = v.toReal(); update(); });
        connect(a, &QVariantAnimation::finished, this, [this]() { hide(); m_reveal = 1.0; });
        m_anim = a;
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int W = width() - kLabelsShadow;
        const int shown = qRound(W * qBound(0.0, m_reveal, 1.0));
        if (shown <= 0) return;
        // Recorte a partir da borda da barra: a faixa "sai" dela.
        // Recorte a partir da borda da barra: a faixa "sai" dela. Layout do
        // widget: [corpo W][sombra] (ou espelhado: [sombra][corpo W]).
        const int left = m_mirrored ? kLabelsShadow : 0;
        const QRectF body(left, 0.5, W, height() - 1.0);
        const qreal visL = m_mirrored ? left + (W - shown) : left;
        p.setClipRect(QRectF(m_mirrored ? visL - kLabelsShadow : 0, 0, shown + kLabelsShadow, height()));
        // Sombra suave no lado de fora.
        const qreal sx = m_mirrored ? visL - kLabelsShadow : visL + shown;
        QLinearGradient sh(QPointF(sx, 0), QPointF(sx + kLabelsShadow, 0));
        sh.setColorAt(m_mirrored ? 1 : 0, QColor(0, 0, 0, 70));
        sh.setColorAt(m_mirrored ? 0 : 1, QColor(0, 0, 0, 0));
        p.fillRect(QRectF(sx, 8, kLabelsShadow, height() - 16), sh);

        // Fundo com os cantos arredondados só do lado de fora; o lado da barra
        // passa por baixo dela, sem borda, pra as duas parecerem uma peça só.
        const qreal r = 10;
        QPainterPath path;
        const QRectF shape = m_mirrored ? QRectF(visL, body.top(), shown + 20, body.height())
                                        : QRectF(visL - 20, body.top(), shown + 20, body.height());
        path.addRoundedRect(shape, r, r);
        QColor bg = Theme::toColor(Theme::panelBackground());
        bg.setAlphaF(qMax(bg.alphaF(), 0.97f));
        p.setPen(QPen(Theme::toColor(Theme::panelBorder()), 1));
        p.setBrush(bg);
        p.drawPath(path);

        const qreal fade = qBound(0.0, (m_reveal - 0.35) / 0.65, 1.0);
        p.setOpacity(fade);
        const qreal s = UiScale::scale();
        QFont hf(QStringLiteral("Segoe UI"));
        hf.setPixelSize(qMax(7, qRound(8.5 * s)));
        hf.setBold(true);
        hf.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
        for (const Head& h : m_heads) {
            p.setFont(hf);
            QColor c = Theme::toColor(Theme::textMuted());
            c.setAlphaF(0.85f);
            p.setPen(c);
            const QRectF tr(body.left() + 12, h.y - 7, W - 24, 14);
            p.drawText(tr, Qt::AlignVCenter | (m_mirrored ? Qt::AlignRight : Qt::AlignLeft), h.text.toUpper());
        }
        QFont nf(QStringLiteral("Segoe UI"));
        nf.setPixelSize(qMax(10, qRound(13 * s)));
        QFont cf(QStringLiteral("Segoe UI"));
        cf.setPixelSize(qMax(9, qRound(11 * s)));
        for (int i = 0; i < m_rows.size(); ++i) {
            const Row& row = m_rows.at(i);
            const QRectF rr(body.left() + 4, row.y, W - 8, row.h);
            if (i == m_hover || row.active) {
                p.setPen(Qt::NoPen);
                p.setBrush(Theme::toColor(i == m_hover ? Theme::hoverOverlay() : Theme::pressedOverlay()));
                p.drawRoundedRect(rr, 7, 7);
            }
            p.setFont(cf);
            p.setPen(Theme::toColor(Theme::textMuted()));
            const qreal cw = row.count.isEmpty() ? 0 : QFontMetricsF(cf).horizontalAdvance(row.count) + 8;
            if (cw > 0) {
                const QRectF cr = m_mirrored ? QRectF(rr.left() + 8, rr.top(), cw, rr.height())
                                             : QRectF(rr.right() - 8 - cw, rr.top(), cw, rr.height());
                p.drawText(cr, Qt::AlignVCenter | (m_mirrored ? Qt::AlignLeft : Qt::AlignRight), row.count);
            }
            QFont f = nf;
            f.setWeight(row.active ? QFont::DemiBold : QFont::Normal);
            p.setFont(f);
            p.setPen(Theme::toColor((row.active || i == m_hover) ? Theme::textBright() : Theme::textPrimary()));
            const QRectF tr = m_mirrored ? QRectF(rr.left() + 8 + cw, rr.top(), rr.width() - 16 - cw, rr.height())
                                         : QRectF(rr.left() + 8, rr.top(), rr.width() - 16 - cw, rr.height());
            p.drawText(tr, Qt::AlignVCenter | (m_mirrored ? Qt::AlignRight : Qt::AlignLeft),
                       QFontMetricsF(f).elidedText(row.text, Qt::ElideRight, tr.width()));
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        const int h = rowAt(e->position().toPoint());
        if (h != m_hover) { m_hover = h; update(); }
        setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
    void mousePressEvent(QMouseEvent* e) override { m_press = rowAt(e->position().toPoint()); e->accept(); }
    void mouseReleaseEvent(QMouseEvent* e) override {
        const int r = rowAt(e->position().toPoint());
        if (e->button() == Qt::LeftButton && r >= 0 && r == m_press && m_rows.at(r).click) {
            const auto click = m_rows.at(r).click;
            click();
        }
        m_press = -1;
    }
    void contextMenuEvent(QContextMenuEvent* e) override {
        const int r = rowAt(e->pos());
        if (r >= 0 && m_rows.at(r).context) {
            const auto ctx = m_rows.at(r).context;
            ctx(e->globalPos());
        }
    }
    void leaveEvent(QEvent*) override {
        if (m_hover != -1) { m_hover = -1; update(); }
        if (onLeave) onLeave();
    }

private:
    int rowAt(const QPoint& p) const {
        const int W = width() - kLabelsShadow;
        const int left = m_mirrored ? kLabelsShadow : 0;
        if (p.x() < left || p.x() > left + W) return -1;
        for (int i = 0; i < m_rows.size(); ++i)
            if (p.y() >= m_rows.at(i).y && p.y() < m_rows.at(i).y + m_rows.at(i).h) return i;
        return -1;
    }
    void stopAnim() {
        if (m_anim) { QVariantAnimation* a = m_anim; m_anim.clear(); a->stop(); }
    }

    QList<Row> m_rows;
    QList<Head> m_heads;
    bool m_mirrored = false;
    qreal m_reveal = 1.0;
    int m_hover = -1;
    int m_press = -1;
    QPointer<QVariantAnimation> m_anim;
};

int LeftBar::barWidth() {
    return barWidthPx();
}

LeftBar::LeftBar(ProjectModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_drawerLayout(nullptr)
    , m_rootLayout(nullptr)
{
    setObjectName(QStringLiteral("leftBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(barWidthPx());
    setStyleSheet(Theme::panelQss(QStringLiteral("leftBar")));
    setAcceptDrops(true);

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(8, 10, 8, 10);
    m_rootLayout->setSpacing(4);

    // ---- Botões fixos: Projeto / Planejamento / Escrita ----
    // Cada bloco é um ToolbarGroupWidget; a composição (ordem dos grupos e
    // quais botões cada um tem) vem do QSettings e é montada em
    // rebuildGroupLayout(), que também cuida dos separadores.
    makeFixedButton(QStringLiteral("projectinfo.svg"), QStringLiteral("i"),
                    tr("Informações do projeto"), Info);
    makeFixedButton(QStringLiteral("whiteboard.svg"), QStringLiteral("L"),
                    tr("Lousa de planejamento"), Whiteboard);
    makeFixedButton(QStringLiteral("timeline.svg"), QStringLiteral("T"),
                    tr("Linha do tempo"), Timeline);
    makeFixedButton(QStringLiteral("manuscriptpanel.svg"), QStringLiteral("T"),
                    tr("Manuscritos"), Manuscripts);
    makeFixedButton(QStringLiteral("outline.svg"), QStringLiteral("O"),
                    tr("Outline"), Outline);
    makeFixedButton(QStringLiteral("groups.svg"), QStringLiteral("G"),
                    tr("Grupos"), Groups);

    m_groupsLayout = new QVBoxLayout();
    m_groupsLayout->setContentsMargins(0, 0, 0, 0);
    m_groupsLayout->setSpacing(4);
    m_rootLayout->addLayout(m_groupsLayout);

    buildGroups();
    const QStringList groupIds = m_groupWidgets.keys();
    const QStringList buttonIds = m_buttonsById.keys();
    m_groupOrder = ToolbarLayout::loadGroupOrder(QStringLiteral("ui/leftBarGroupOrder"), defaultGroupOrder());
    m_groupButtons = ToolbarLayout::loadButtonLayout(QStringLiteral("ui/leftBarButtonLayout"),
                                                     defaultButtonLayout(),
                                                     QSet<QString>(groupIds.begin(), groupIds.end()),
                                                     QSet<QString>(buttonIds.begin(), buttonIds.end()));
    rebuildGroupLayout();

    // ---- Botão "+" (nova gaveta) — fica logo acima da lista, igual Mira 1
    m_newDrawerBtn = makeNewDrawerButton();
    m_rootLayout->addWidget(m_newDrawerBtn);

    // ---- Gavetas ----
    m_drawerLayout = new QVBoxLayout();
    m_drawerLayout->setContentsMargins(0, 0, 0, 0);
    m_drawerLayout->setSpacing(4);
    m_rootLayout->addLayout(m_drawerLayout);

    m_rootLayout->addStretch();

    if (m_model) {
        connect(m_model, &ProjectModel::drawersChanged, this, &LeftBar::rebuildDrawerButtons);
        connect(m_model, &ProjectModel::loaded, this, &LeftBar::rebuildDrawerButtons);
        rebuildDrawerButtons();
    }

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &LeftBar::applyTheme);
    connect(UiScale::Manager::instance(), &UiScale::Manager::scaleChanged,
            this, &LeftBar::applyUiScale);
    applyUiScale(); // aplica a escala persistida (botões/ícones no tamanho certo)
}

void LeftBar::applyUiScale() {
    setFixedWidth(barWidthPx());
    const int btn = btnSizePx();
    const int ico = iconSizePx();

    for (auto it = m_fixedButtons.constBegin(); it != m_fixedButtons.constEnd(); ++it) {
        auto* b = it.value();
        if (!b) continue;
        b->setFixedSize(btn, btn);
        const QString res = b->property("iconRes").toString();
        if (!res.isEmpty()) {
            const QIcon ic = loadLeftbarIcon(res);
            if (!ic.isNull()) {
                b->setIcon(ic);
                b->setIconSize(QSize(ico, ico));
            }
        }
    }
    if (m_newDrawerBtn) m_newDrawerBtn->setFixedSize(btn, btn);
    rebuildDrawerButtons(); // recria os botões de gaveta já no tamanho novo
}

void LeftBar::applyTheme() {
    setStyleSheet(Theme::panelQss(QStringLiteral("leftBar")));
    for (auto it = m_fixedButtons.constBegin(); it != m_fixedButtons.constEnd(); ++it) {
        if (auto* btn = it.value()) {
            btn->setStyleSheet(fixedButtonQss());
            // Re-tinta o ícone com as cores do novo tema.
            const QString res = btn->property("iconRes").toString();
            if (!res.isEmpty()) {
                const QIcon ic = loadLeftbarIcon(res);
                if (!ic.isNull()) btn->setIcon(ic);
            }
        }
    }
    if (m_newDrawerBtn) m_newDrawerBtn->setStyleSheet(newDrawerQss());
    for (auto* sep : m_groupSeparators) {
        if (sep) sep->setStyleSheet(separatorQss());
    }
    for (ToolbarGroupWidget* g : std::as_const(m_groupWidgets)) {
        if (g) g->applyTheme(QColor(Theme::textMuted()), QColor(Theme::accentDefault()));
    }
    // Recria botões de gaveta pra refletirem accent/textBright atualizados.
    rebuildDrawerButtons();
}

// A barra em si nao muda nada ao trocar de lado (ela e vertical dos dois
// lados); quem reposiciona e o MainWindow, ouvindo barSideChanged. Guardar o
// lado aqui existe pra que chromeInset() e os paineis flutuantes possam
// perguntar "de que lado voce esta?" em vez de assumir a esquerda.
void LeftBar::setBarSide(Qt::Edge side)
{
    if (m_barSide == side) return;
    m_barSide = side;
    emit barSideChanged(m_barSide);
}

void LeftBar::setChromeHidden(bool hidden) {
    if (hidden) hideLabels(false);
    // Modo focado recolhendo a barra no meio de uma reorganização: sai da
    // edição antes, senão os grupos voltariam tracejados quando ela reaparecer.
    if (hidden) setEditMode(false);
    m_chromeHidden = hidden;
    for (auto it = m_fixedButtons.constBegin(); it != m_fixedButtons.constEnd(); ++it)
        if (it.value()) it.value()->setVisible(!hidden);
    for (ToolbarGroupWidget* g : std::as_const(m_groupWidgets))
        if (g && !g->isEmpty()) g->setVisible(!hidden);
    for (auto it = m_drawerButtons.constBegin(); it != m_drawerButtons.constEnd(); ++it)
        if (it.value()) it.value()->setVisible(!hidden);
    if (m_newDrawerBtn) m_newDrawerBtn->setVisible(!hidden);
    for (auto* sep : m_groupSeparators)
        if (sep) sep->setVisible(!hidden);
    if (hidden)
        setStyleSheet(QStringLiteral("#leftBar { background: transparent; }"));
    else
        setStyleSheet(Theme::panelQss(QStringLiteral("leftBar")));
}

void LeftBar::setMirrored(bool mirrored) {
    if (m_mirrored == mirrored) return;
    m_mirrored = mirrored;
    applyMirrorStyle();
}

void LeftBar::applyMirrorStyle() {
    // O lado físico (esquerda/direita) na janela é decidido pelo MainWindow.
    // Aqui o painel tem borda completa (radius); por ora nada muda visualmente.
    setStyleSheet(Theme::panelQss(QStringLiteral("leftBar")));
}

void LeftBar::setActiveFixedAction(FixedAction action) {
    m_activeFixed = static_cast<int>(action);
    m_activeDrawer.clear();
    refreshActiveStates();
}

void LeftBar::setActiveDrawer(const QString& drawerKey) {
    m_activeDrawer = drawerKey;
    m_activeFixed = -1;
    refreshActiveStates();
}

void LeftBar::clearSelection() {
    m_activeFixed = -1;
    m_activeDrawer.clear();
    refreshActiveStates();
}

void LeftBar::refreshActiveStates() {
    for (auto it = m_fixedButtons.constBegin(); it != m_fixedButtons.constEnd(); ++it) {
        if (auto* btn = it.value()) {
            btn->setChecked(it.key() == m_activeFixed);
        }
    }
    for (auto it = m_drawerButtons.constBegin(); it != m_drawerButtons.constEnd(); ++it) {
        if (auto* btn = it.value()) {
            btn->setChecked(it.key() == m_activeDrawer);
        }
    }
}

QToolButton* LeftBar::fixedButton(FixedAction action) const {
    return m_fixedButtons.value(static_cast<int>(action), nullptr);
}

QToolButton* LeftBar::makeFixedButton(const QString& iconResource,
                                     const QString& placeholderLetter,
                                     const QString& tooltip,
                                     FixedAction action) {
    auto* btn = new QToolButton(this);
    btn->setToolTip(tooltip);
    btn->setFixedSize(btnSizePx(), btnSizePx());
    btn->setCursor(Qt::PointingHandCursor);
    btn->setCheckable(true);
    btn->setAutoRaise(true);
    btn->setStyleSheet(fixedButtonQss());
    // Guarda o recurso do ícone pra recarregá-lo (recolorido) ao trocar de tema —
    // o IconUtils tinta o SVG no load, então o ícone precisa ser re-tintado.
    btn->setProperty("iconRes", iconResource);

    if (!iconResource.isEmpty()) {
        QIcon ic = loadLeftbarIcon(iconResource);
        if (!ic.isNull()) {
            btn->setIcon(ic);
            btn->setIconSize(QSize(iconSizePx(), iconSizePx()));
            btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        } else {
            btn->setText(placeholderLetter);
            btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        }
    } else {
        btn->setText(placeholderLetter);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

    // Os botões são "radio-like" entre si — clicar emite o sinal e o MainWindow
    // decide se abre/fecha. O setChecked é controlado por setActiveFixedAction.
    connect(btn, &QToolButton::clicked, this, [this, action, btn]() {
        // Bloqueia o toggle automático do QToolButton — quem manda no estado é
        // o MainWindow via setActiveFixedAction. Reverte o checked imediato.
        btn->setChecked(m_activeFixed == static_cast<int>(action));
        emit fixedActionTriggered(action);
    });

    m_fixedButtons.insert(static_cast<int>(action), btn);
    return btn;
}

QToolButton* LeftBar::makeDrawerButton(const QString& drawerKey,
                                      const QString& title,
                                      const QString& color,
                                      const QString& iconId) {
    auto* btn = new QToolButton(this);
    btn->setToolTip(title);
    btn->setFixedSize(btnSizePx(), btnSizePx());
    btn->setCursor(Qt::PointingHandCursor);
    btn->setCheckable(true);
    btn->setAutoRaise(true);

    const QString accent = color.isEmpty() ? Theme::accentDefault() : color;

    // Se a gaveta tem ícone escolhido, renderiza-o tintado com a cor da gaveta;
    // senão, cai pra letra inicial (fallback).
    bool iconLoaded = false;
    if (!iconId.isEmpty()) {
        QIcon ic = IconUtils::loadToolbarIcon(
            QStringLiteral(":/icons/elements/%1.svg").arg(iconId),
            QColor(accent),
            QColor(accent),
            QColor(Theme::textBright()),
            QSize(iconSizePx(), iconSizePx()));
        if (!ic.isNull()) {
            btn->setIcon(ic);
            btn->setIconSize(QSize(iconSizePx(), iconSizePx()));
            btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
            iconLoaded = true;
        }
    }
    if (!iconLoaded) {
        const QString letter = title.isEmpty() ? QStringLiteral("?") : title.left(1).toUpper();
        btn->setText(letter);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

    btn->setStyleSheet(drawerButtonQss(accent));

    connect(btn, &QToolButton::clicked, this, [this, drawerKey, btn]() {
        btn->setChecked(m_activeDrawer == drawerKey);
        emit drawerSelected(drawerKey);
    });

    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(btn, &QToolButton::customContextMenuRequested, this, [this, btn, drawerKey](const QPoint& pos) {
        emit drawerContextRequested(drawerKey, btn->mapToGlobal(pos));
    });

    btn->installEventFilter(this);
    btn->setProperty("drawerKey", drawerKey);

    m_drawerButtons.insert(drawerKey, btn);
    return btn;
}

QToolButton* LeftBar::makeNewDrawerButton() {
    auto* btn = new QToolButton(this);
    btn->setText(QStringLiteral("+"));
    btn->setToolTip(tr("Nova gaveta"));
    btn->setFixedSize(btnSizePx(), btnSizePx());
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(newDrawerQss());
    connect(btn, &QToolButton::clicked, this, &LeftBar::newDrawerRequested);
    return btn;
}

void LeftBar::buildGroups() {
    for (const QString& id : defaultGroupOrder()) {
        auto* g = new ToolbarGroupWidget(id, Qt::Vertical, this);
        g->setSpacing(4); // o mesmo passo que os botões sempre tiveram na barra
        connect(g, &ToolbarGroupWidget::groupDroppedOn, this, &LeftBar::onGroupDropped);
        connect(g, &ToolbarGroupWidget::buttonDroppedOn, this, &LeftBar::onButtonDropped);
        connect(g, &ToolbarGroupWidget::toggleEditModeRequested,
                this, [this]() { setEditMode(!m_editMode); });
        g->applyTheme(QColor(Theme::textMuted()), QColor(Theme::accentDefault()));
        m_groupWidgets.insert(id, g);
    }

    // Ids ESTÁVEIS: é isso que vai pro QSettings. Renomear um id aqui invalida
    // a organização salva do usuário — trate como formato de arquivo.
    m_buttonsById = {
        { QStringLiteral("info"),        m_fixedButtons.value(Info) },
        { QStringLiteral("whiteboard"),  m_fixedButtons.value(Whiteboard) },
        { QStringLiteral("timeline"),    m_fixedButtons.value(Timeline) },
        { QStringLiteral("manuscripts"), m_fixedButtons.value(Manuscripts) },
        { QStringLiteral("outline"),     m_fixedButtons.value(Outline) },
        { QStringLiteral("groups"),      m_fixedButtons.value(Groups) },
    };
}

QHash<QString, QStringList> LeftBar::defaultButtonLayout() const {
    return {
        { QStringLiteral("project"),  { QStringLiteral("info") } },
        { QStringLiteral("planning"), { QStringLiteral("whiteboard"), QStringLiteral("timeline") } },
        { QStringLiteral("writing"),  { QStringLiteral("manuscripts"), QStringLiteral("outline"),
                                        QStringLiteral("groups") } },
    };
}

QStringList LeftBar::defaultGroupOrder() const {
    return { QStringLiteral("project"), QStringLiteral("planning"), QStringLiteral("writing") };
}

void LeftBar::rebuildGroupLayout() {
    if (!m_groupsLayout) return;

    QLayoutItem* item;
    while ((item = m_groupsLayout->takeAt(0)) != nullptr) delete item; // não deleta os widgets
    // hide() antes do deleteLater: fora do layout, o separador velho ficaria
    // desenhado na posição antiga até o próximo giro do event loop.
    for (QFrame* sep : std::as_const(m_groupSeparators)) if (sep) { sep->hide(); sep->deleteLater(); }
    m_groupSeparators.clear();

    for (const QString& gid : std::as_const(m_groupOrder)) {
        ToolbarGroupWidget* g = m_groupWidgets.value(gid);
        if (!g) continue;
        g->clearButtons();
        for (const QString& bid : m_groupButtons.value(gid)) {
            if (QToolButton* b = m_buttonsById.value(bid)) g->addButton(b, bid);
        }
        g->refreshEmptyPlaceholder();
    }

    // Grupo vazio só ocupa lugar durante a edição (é onde se devolve um botão
    // pra ele); fora dela some, junto com o separador que ficaria sobrando.
    for (const QString& gid : std::as_const(m_groupOrder)) {
        ToolbarGroupWidget* g = m_groupWidgets.value(gid);
        if (!g) continue;
        if (g->isEmpty() && !m_editMode) { g->hide(); continue; }
        g->setVisible(!m_chromeHidden);
        m_groupsLayout->addWidget(g, 0, Qt::AlignHCenter);
        // Separador DEPOIS de cada grupo, inclusive o último: é ele que divide
        // os botões fixos do "+" e das gavetas, como sempre foi.
        auto* sep = makeGroupSeparator(this);
        sep->setVisible(!m_chromeHidden);
        m_groupSeparators.append(sep);
        m_groupsLayout->addWidget(sep);
    }
}

void LeftBar::setEditMode(bool on) {
    if (m_editMode == on) return;
    m_editMode = on;
    if (on) hideLabels(false);
    // Filtro de aplicação só enquanto edita — existe só pra perceber o clique
    // fora da barra que encerra a edição (ver eventFilter).
    if (on) qApp->installEventFilter(this);
    else    qApp->removeEventFilter(this);
    for (ToolbarGroupWidget* g : std::as_const(m_groupWidgets)) {
        if (g) g->setDraggable(on);
    }
    rebuildGroupLayout();
}

void LeftBar::onButtonDropped(const QString& buttonId, const QString& targetGroupId, int index) {
    if (!m_buttonsById.contains(buttonId)) return;
    if (!ToolbarLayout::moveButton(m_groupButtons, buttonId, targetGroupId, index)) return;
    ToolbarLayout::saveButtonLayout(QStringLiteral("ui/leftBarButtonLayout"), m_groupOrder, m_groupButtons);
    rebuildGroupLayout();
}

void LeftBar::onGroupDropped(const QString& draggedId, const QString& targetId) {
    if (!ToolbarLayout::moveGroup(m_groupOrder, draggedId, targetId)) return;
    ToolbarLayout::saveGroupOrder(QStringLiteral("ui/leftBarGroupOrder"), m_groupOrder);
    // O layout é serializado na ordem dos grupos — precisa acompanhar.
    ToolbarLayout::saveButtonLayout(QStringLiteral("ui/leftBarButtonLayout"), m_groupOrder, m_groupButtons);
    rebuildGroupLayout();
}

// Sair da reorganização é clicar em qualquer coisa que não seja um ícone
// arrastável. Este handler cobre o vazio da própria barra — inclusive o de
// dentro de um grupo, cujo press ignorado sobe pra cá. Clique fora da barra é
// o eventFilter; gaveta e "+" são tratados lá também.
void LeftBar::mousePressEvent(QMouseEvent* event) {
    if (m_editMode && event->button() == Qt::LeftButton) {
        setEditMode(false);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

bool LeftBar::eventFilter(QObject* watched, QEvent* event) {
    // Etiquetas abertas: tooltip de botão sobra, e clicar na barra fecha a
    // faixa (a gaveta vai abrir bem ali).
    if (m_labels && m_labels->isVisible() && watched != this && qobject_cast<QToolButton*>(watched)) {
        if (event->type() == QEvent::ToolTip) return true;
        if (event->type() == QEvent::MouseButtonPress) hideLabels(false);
    }
    // Filtro de aplicação (só instalado durante a edição). O clique não é
    // consumido: quem clicou no texto quer o cursor lá, e quem clicou numa
    // gaveta quer a gaveta aberta — sair da edição é efeito colateral.
    if (m_editMode && event->type() == QEvent::MouseButtonPress) {
        auto* w = qobject_cast<QWidget*>(watched);
        if (w && w != this) {
            bool insideGroup = false;
            for (ToolbarGroupWidget* g : std::as_const(m_groupWidgets))
                if (g && (w == g || g->isAncestorOf(w))) { insideGroup = true; break; }
            // Dentro da barra mas fora dos grupos (gaveta, "+"): também encerra.
            // O vazio da barra em si cai no mousePressEvent acima.
            if (!insideGroup) setEditMode(false);
        }
    }

    auto* btn = qobject_cast<QToolButton*>(watched);
    if (btn && m_drawerButtons.values().contains(btn)) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragStartPos = me->pos();
                m_pressedDrawerKey = btn->property("drawerKey").toString();
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            if ((me->buttons() & Qt::LeftButton) && !m_pressedDrawerKey.isEmpty()) {
                if ((me->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
                    const QString key = m_pressedDrawerKey;
                    m_pressedDrawerKey.clear();
                    startDrawerDrag(btn, key);
                    return true; // consome o move para não disparar click
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_pressedDrawerKey.clear();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void LeftBar::startDrawerDrag(QToolButton* btn, const QString& drawerKey) {
    QDrag* drag = new QDrag(btn);
    auto* mime = new QMimeData();
    mime->setData(QLatin1String(kDrawerMime), drawerKey.toUtf8());
    drag->setMimeData(mime);
    const QPixmap pm = btn->grab();
    drag->setPixmap(pm);
    drag->setHotSpot(QPoint(pm.width() / 2, pm.height() / 2));
    drag->exec(Qt::MoveAction);
    clearDropIndicator();
}

int LeftBar::drawerInsertIndexAt(const QPoint& posInBar) const {
    // Calcula índice de inserção (0..N) com base na coordenada Y dentro da barra.
    // Pega o midpoint vertical de cada drawer button; se posY < midpoint, insere antes.
    if (!m_drawerLayout) return 0;
    const int count = m_model ? m_model->drawers().size() : 0;
    int idx = 0;
    for (const auto& d : (m_model ? m_model->drawers() : QList<Drawer>{})) {
        QToolButton* b = m_drawerButtons.value(d.key, nullptr);
        if (!b) { ++idx; continue; }
        const QPoint topLeft = b->mapTo(const_cast<LeftBar*>(this), QPoint(0, 0));
        const int mid = topLeft.y() + b->height() / 2;
        if (posInBar.y() < mid) return idx;
        ++idx;
    }
    return count;
}

void LeftBar::updateDropIndicator(int targetIndex) {
    if (targetIndex == m_dropTargetIndex && m_dropIndicator && m_dropIndicator->isVisible()) return;
    m_dropTargetIndex = targetIndex;

    if (!m_dropIndicator) {
        m_dropIndicator = new QWidget(this);
        m_dropIndicator->setFixedHeight(2);
        m_dropIndicator->setStyleSheet(Theme::qss(QStringLiteral(
            "background: %1; border-radius: @radius-control;").arg(Theme::accentDefault())));
        m_dropIndicator->hide();
    }

    // Posição Y do indicador: topo do botão no targetIndex, ou base do último se idx == count.
    int y = 0;
    const QList<Drawer>& list = m_model ? m_model->drawers() : QList<Drawer>{};
    if (targetIndex >= list.size()) {
        if (!list.isEmpty()) {
            QToolButton* b = m_drawerButtons.value(list.last().key);
            if (b) y = b->mapTo(this, QPoint(0, b->height())).y();
        }
    } else {
        QToolButton* b = m_drawerButtons.value(list.at(targetIndex).key);
        if (b) y = b->mapTo(this, QPoint(0, 0)).y();
    }

    const int margin = 6;
    m_dropIndicator->setGeometry(margin, y - 1, width() - margin * 2, 2);
    m_dropIndicator->raise();
    m_dropIndicator->show();
}

void LeftBar::clearDropIndicator() {
    if (m_dropIndicator) m_dropIndicator->hide();
    m_dropTargetIndex = -1;
}

void LeftBar::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat(QLatin1String(kDrawerMime))) {
        event->acceptProposedAction();
    }
}

void LeftBar::dragMoveEvent(QDragMoveEvent* event) {
    if (!event->mimeData()->hasFormat(QLatin1String(kDrawerMime))) return;
    updateDropIndicator(drawerInsertIndexAt(event->position().toPoint()));
    event->acceptProposedAction();
}

void LeftBar::dragLeaveEvent(QDragLeaveEvent* /*event*/) {
    clearDropIndicator();
}

void LeftBar::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasFormat(QLatin1String(kDrawerMime))) return;
    const QString key = QString::fromUtf8(event->mimeData()->data(QLatin1String(kDrawerMime)));
    const int target = drawerInsertIndexAt(event->position().toPoint());
    clearDropIndicator();
    if (m_model && !key.isEmpty()) {
        m_model->reorderDrawer(key, target);
    }
    event->acceptProposedAction();
}

void LeftBar::rebuildDrawerButtons() {
    if (!m_drawerLayout) return;
    m_drawerButtons.clear();
    QLayoutItem* child;
    while ((child = m_drawerLayout->takeAt(0)) != nullptr) {
        if (auto* w = child->widget()) w->deleteLater();
        delete child;
    }
    if (!m_model) return;
    for (const auto& d : m_model->drawers()) {
        // Preferência: drawerIcon (escolha do usuário). Fallback:
        // drawerElementIcon (herdado do tipo). Vazio => letra.
        const QString iconId = !d.drawerIcon.isEmpty() ? d.drawerIcon : d.drawerElementIcon;
        m_drawerLayout->addWidget(makeDrawerButton(d.key, d.title, d.color, iconId));
    }
    refreshActiveStates();
    // Faixa aberta: os botões novos ainda vão ganhar posição no layout.
    if (m_labels && m_labels->isVisible())
        QTimer::singleShot(0, this, [this]() { if (m_labels && m_labels->isVisible()) refreshLabels(); });
}

bool LeftBar::labelsEnabled() {
    return QSettings().value(QStringLiteral("ui/leftBarLabels"), true).toBool();
}

void LeftBar::setLabelsEnabled(bool on) {
    QSettings().setValue(QStringLiteral("ui/leftBarLabels"), on);
}

void LeftBar::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
    if (!labelsEnabled() || (m_labels && m_labels->isVisible())) return;
    if (!m_labelsTimer) {
        m_labelsTimer = new QTimer(this);
        m_labelsTimer->setSingleShot(true);
        connect(m_labelsTimer, &QTimer::timeout, this, &LeftBar::showLabels);
    }
    m_labelsTimer->start(kLabelsOpenDelayMs);
}

void LeftBar::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    if (m_labelsTimer) m_labelsTimer->stop();
    maybeHideLabels();
}

void LeftBar::moveEvent(QMoveEvent* event) {
    QWidget::moveEvent(event);
    if (m_labels && m_labels->isVisible()) updateLabelsGeometry();
}

void LeftBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_labels && m_labels->isVisible()) updateLabelsGeometry();
}

void LeftBar::hideEvent(QHideEvent* event) {
    hideLabels(false);
    QWidget::hideEvent(event);
}

void LeftBar::updateLabelsGeometry() {
    if (!m_labels) return;
    const int w = m_labelsWidth + kLabelsShadow;
    const QRect g = geometry();
    const bool mirrored = (m_barSide == Qt::RightEdge);
    // 1px por cima da borda da barra, pra não ficar uma linha dupla no meio.
    const int x = mirrored ? g.x() - w + 1 : g.x() + g.width() - 1;
    // Só até a última linha: a faixa é uma aba do tamanho do conteúdo, não um
    // painel vazio até o chão.
    const int h = m_labelsHeight > 0 ? qMin(g.height(), m_labelsHeight) : g.height();
    m_labels->setGeometry(x, g.y(), w, h);
}

void LeftBar::showLabels() {
    if (!labelsEnabled() || m_editMode || m_chromeHidden || !isVisible() || QApplication::mouseButtons() != Qt::NoButton) return;
    QWidget* host = parentWidget();
    if (!host) return;
    if (!m_labels || m_labels->parentWidget() != host) {
        delete m_labels;
        m_labels = new LeftBarLabels(host);
        m_labels->onLeave = [this]() { maybeHideLabels(); };
    }
    refreshLabels();
    updateLabelsGeometry();
    m_labels->raise();
    m_labels->show();
    m_labels->open(PanelMotion::enabled());
    // Enquanto a faixa está aberta, os botões não precisam de tooltip, e um
    // clique na barra fecha a faixa (é a gaveta que vai abrir ali).
    for (QToolButton* b : findChildren<QToolButton*>()) b->installEventFilter(this);
}

void LeftBar::refreshLabels() {
    if (!m_labels) return;
    QList<LeftBarLabels::Row> rows;
    QList<LeftBarLabels::Head> heads;
    auto rowFor = [&](QToolButton* b, const QString& text, const QString& count) {
        LeftBarLabels::Row r;
        const QPoint at = b->mapTo(this, QPoint(0, 0));
        r.y = at.y();
        r.h = b->height();
        r.text = text;
        r.count = count;
        r.active = b->isChecked();
        QPointer<QToolButton> pb(b);
        r.click = [this, pb]() { hideLabels(false); if (pb) pb->click(); };
        return r;
    };
    auto groupName = [this](const QString& gid) {
        if (gid == QStringLiteral("project")) return tr("Projeto");
        if (gid == QStringLiteral("planning")) return tr("Planejamento");
        if (gid == QStringLiteral("writing")) return tr("Escrita");
        return QString();
    };
    // Na faixa, o nome curto; a dica completa continua no tooltip do botão.
    const QHash<QString, QString> shortName = {
        { QStringLiteral("info"),        tr("Informações") },
        { QStringLiteral("whiteboard"),  tr("Lousa") },
        { QStringLiteral("timeline"),    tr("Linha do tempo") },
        { QStringLiteral("manuscripts"), tr("Manuscritos") },
        { QStringLiteral("outline"),     tr("Outline") },
        { QStringLiteral("groups"),      tr("Grupos") },
    };
    QStringList shownGroups;
    for (const QString& gid : std::as_const(m_groupOrder)) {
        ToolbarGroupWidget* g = m_groupWidgets.value(gid);
        if (!g || !g->isVisible()) continue;
        shownGroups << gid;
        for (const QString& bid : m_groupButtons.value(gid)) {
            QToolButton* b = m_buttonsById.value(bid);
            if (b && b->isVisible()) rows << rowFor(b, shortName.value(bid, b->toolTip()), QString());
        }
    }
    // Título de cada grupo em cima do separador que o antecede; o último
    // separador abre as gavetas.
    for (int i = 0; i < m_groupSeparators.size(); ++i) {
        QFrame* sep = m_groupSeparators.at(i);
        if (!sep || !sep->isVisible()) continue;
        LeftBarLabels::Head h;
        h.y = sep->mapTo(this, QPoint(0, 0)).y() + sep->height() / 2;
        h.text = (i + 1 < shownGroups.size()) ? groupName(shownGroups.at(i + 1)) : tr("Gavetas");
        if (!h.text.isEmpty()) heads << h;
    }
    if (m_newDrawerBtn && m_newDrawerBtn->isVisible()) rows << rowFor(m_newDrawerBtn, tr("Nova gaveta"), QString());
    if (m_model) {
        for (const auto& d : m_model->drawers()) {
            QToolButton* b = m_drawerButtons.value(d.key);
            if (!b || !b->isVisible()) continue;
            LeftBarLabels::Row r = rowFor(b, d.title, d.items.isEmpty() ? QString() : QString::number(d.items.size()));
            const QString key = d.key;
            r.context = [this, key](const QPoint& pos) { hideLabels(false); emit drawerContextRequested(key, pos); };
            rows << r;
        }
    }
    // Largura pelo conteúdo: nome mais comprido (em negrito, que é o mais
    // largo) + contagem + respiros das margens.
    const qreal s = UiScale::scale();
    QFont nf(QStringLiteral("Segoe UI"));
    nf.setPixelSize(qMax(10, qRound(13 * s)));
    nf.setWeight(QFont::DemiBold);
    QFont cf(QStringLiteral("Segoe UI"));
    cf.setPixelSize(qMax(9, qRound(11 * s)));
    qreal need = 0;
    for (const auto& r : std::as_const(rows)) {
        qreal w = QFontMetricsF(nf).horizontalAdvance(r.text);
        if (!r.count.isEmpty()) w += QFontMetricsF(cf).horizontalAdvance(r.count) + 8;
        need = qMax(need, w);
    }
    QFont hf(QStringLiteral("Segoe UI"));
    hf.setPixelSize(qMax(7, qRound(8.5 * s)));
    hf.setBold(true);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
    for (const auto& h : std::as_const(heads)) need = qMax(need, QFontMetricsF(hf).horizontalAdvance(h.text.toUpper()) - 8);
    m_labelsWidth = qBound(labelsMinWidthPx(), qCeil(need) + 30, labelsMaxWidthPx());
    int bottom = 0;
    for (const auto& r : std::as_const(rows)) bottom = qMax(bottom, r.y + r.h);
    m_labelsHeight = bottom > 0 ? bottom + qRound(10 * s) : 0;
    m_labels->setContent(rows, heads, m_barSide == Qt::RightEdge);
    updateLabelsGeometry();
}

void LeftBar::hideLabels(bool animated) {
    if (m_labelsTimer) m_labelsTimer->stop();
    if (m_labels && m_labels->isVisible()) m_labels->close(animated && PanelMotion::enabled());
}

void LeftBar::maybeHideLabels() {
    // Um respiro pro mouse atravessar da barra pra faixa (e de volta).
    QTimer::singleShot(70, this, [this]() {
        const QPoint g = QCursor::pos();
        if (rect().contains(mapFromGlobal(g))) return;
        if (m_labels && m_labels->isVisible() && m_labels->rect().contains(m_labels->mapFromGlobal(g))) return;
        hideLabels(true);
    });
}
