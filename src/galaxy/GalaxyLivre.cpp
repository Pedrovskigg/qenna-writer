// Criação Livre: a casca. Prateleira à esquerda, folha no meio, direita e gaveta em GalaxyLivreSide.cpp.
// A esquerda e a direita se refazem a cada mudança; a folha só quando troca o que está aberto, pro cursor não pular.

#include "GalaxyLivre.h"
#include "GalaxyLivreMap.h"
#include "GalaxyDoc.h"
#include "GalaxyCanvas.h"
#include "GalaxyHab.h"
#include "GalaxyPanel.h"
#include "GalaxyStructures.h"
#include "GalaxyWidgets.h"

#include "../ProjectModel.h"
#include "../TerritorioStore.h"
#include "../Theme.h"
#include "../carto/CartoIcons.h"

#include <QApplication>
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QDrag>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStyle>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace galaxy {

namespace {
const char* const MIME = "application/x-qenna-livre";
struct FontDef { const char* name; const char* family; };
const FontDef FONTS[] = { { "Source Serif", "Source Serif 4" }, { "Georgia", "Georgia" }, { "Times New Roman", "Times New Roman" }, { "Figtree", "Figtree" }, { "Segoe UI", "Segoe UI" }, { "Consolas", "Consolas" } };
constexpr int NFONTS = 6;
inline QColor col(const QString& c) { return Theme::toColor(c); }
inline QString rgba(const QColor& c, double a) { return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a); }
QLabel* lab(const QString& text, const QString& obj, QWidget* parent, bool wrap = false)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(obj);
    l->setWordWrap(wrap);
    return l;
}
QPushButton* btn(const QString& text, const QString& obj, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(obj);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}
void clearLayout(QLayout* l)
{
    while (QLayoutItem* it = l->takeAt(0)) {
        if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
        else if (QLayout* c = it->layout()) clearLayout(c);
        delete it;
    }
}
QPixmap starPix(const QColor& c, int px)
{
    QPixmap pm(px * 2, px * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    const double cx = px / 2.0, cy = px / 2.0 + 0.4, R = px * 0.48, r = R * 0.45;
    for (int i = 0; i < 10; i++) {
        const double a = -PI_D / 2 + i * PI_D / 5, rr = i % 2 ? r : R;
        const QPointF q(cx + std::cos(a) * rr, cy + std::sin(a) * rr);
        if (i) path.lineTo(q); else path.moveTo(q);
    }
    path.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawPath(path);
    return pm;
}

// A bolinha da prateleira: o mundo em 3D, o nome embaixo, o selo de documentos e a estrela da capital.
class Ball : public QWidget {
public:
    Ball(const QPixmap& pix, const QString& name, int docs, bool on, bool ghost, bool capital, bool fresh, QWidget* parent)
        : QWidget(parent), m_pix(pix), m_name(name), m_docs(docs), m_on(on), m_ghost(ghost), m_capital(capital), m_fresh(fresh)
    {
        setFixedSize(78, 96);
        setCursor(ghost ? Qt::ArrowCursor : Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
    }
    std::function<void()> clicked;
    std::function<void()> dragged;
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor a = col(Theme::accentDefault()), ink = col(Theme::textPrimary()), warn = col(Theme::accentWarning());
        if (m_on || underMouse()) {
            QColor bg = m_on ? a : ink;
            bg.setAlphaF(m_on ? 0.16 : 0.06);
            p.setPen(m_on ? QPen(a, 1.2) : Qt::NoPen);
            p.setBrush(bg);
            p.drawRoundedRect(QRectF(1, 1, width() - 2, height() - 2), 10, 10);
        }
        const QRectF ballR((width() - 54) / 2.0, 6, 54, 54);
        if (m_ghost) p.setOpacity(0.5);
        p.drawPixmap(ballR.toRect(), m_pix);
        p.setOpacity(1);
        if (m_ghost) { QPen pen(warn, 1.3, Qt::DashLine); p.setPen(pen); p.setBrush(Qt::NoBrush); p.drawEllipse(ballR.adjusted(-3, -3, 3, 3)); }
        if (m_fresh) { QPen pen(a, 2); p.setPen(pen); p.setBrush(Qt::NoBrush); p.drawEllipse(ballR.adjusted(-2, -2, 2, 2)); }
        if (m_docs > 0) {
            const QRectF pip(ballR.right() - 14, ballR.top() - 2, 18, 18);
            p.setPen(QPen(col(Theme::panelBackground()), 2));
            p.setBrush(a);
            p.drawEllipse(pip);
            QFont f = font(); f.setPixelSize(10); f.setBold(true);
            p.setFont(f);
            p.setPen(Qt::white);
            p.drawText(pip, Qt::AlignCenter, QString::number(m_docs));
        }
        if (m_capital) p.drawPixmap(QPointF(ballR.left() - 3, ballR.top() - 3), starPix(warn, 15));
        QFont f = font(); f.setPixelSize(12); f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(m_ghost ? warn : ink);
        const QRectF tr(2, 64, width() - 4, 30);
        const QString t = QFontMetrics(f).elidedText(m_name, Qt::ElideRight, int(tr.width()) * 2 - 8);
        p.drawText(tr, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, t);
    }
    void mousePressEvent(QMouseEvent* e) override { if (e->button() == Qt::LeftButton) { m_press = e->position(); m_down = true; } }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (m_down && !m_ghost && dragged && (e->position() - m_press).manhattanLength() > 6) { m_down = false; dragged(); }
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (m_down && e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && clicked) { m_down = false; clicked(); }
        m_down = false;
    }
private:
    QPixmap m_pix;
    QString m_name;
    int m_docs;
    bool m_on, m_ghost, m_capital, m_fresh;
    QPointF m_press;
    bool m_down = false;
};

// A borda entre duas colunas: arrastar muda a largura, dois cliques voltam ao padrão.
class Grip : public QWidget {
public:
    explicit Grip(QWidget* parent) : QWidget(parent) { setObjectName(QStringLiteral("lvGrip")); setAttribute(Qt::WA_StyledBackground, true); setFixedWidth(8); setCursor(Qt::SplitHCursor); setToolTip(QCoreApplication::translate("galaxy::Livre", "Arraste pra mudar a largura")); }
    std::function<void()> pressed;
    std::function<void(int dx)> moved;
    std::function<void()> released, reset;
protected:
    void mousePressEvent(QMouseEvent* e) override { m_x = e->globalPosition().x(); setProperty("on", true); style()->unpolish(this); style()->polish(this); if (pressed) pressed(); }
    void mouseMoveEvent(QMouseEvent* e) override { if (e->buttons() & Qt::LeftButton && moved) moved(int(e->globalPosition().x() - m_x)); }
    void mouseReleaseEvent(QMouseEvent*) override { setProperty("on", false); style()->unpolish(this); style()->polish(this); if (released) released(); }
    void mouseDoubleClickEvent(QMouseEvent*) override { if (reset) reset(); }
private:
    double m_x = 0;
};

// O ladrilho do sistema (prateleira C): o sol no meio, os mundos em órbita, o nome e quantos mundos tem.
class SysTile : public QWidget {
public:
    SysTile(const QColor& sun, bool hole, std::vector<QPixmap> planets, const QString& name, const QString& sub, int pending, bool on, bool ghost, bool fresh, QWidget* parent)
        : QWidget(parent), m_sun(sun), m_hole(hole), m_planets(std::move(planets)), m_name(name), m_sub(sub), m_pending(pending), m_on(on), m_ghost(ghost), m_fresh(fresh)
    {
        setFixedSize(78, 104);
        setCursor(ghost ? Qt::ArrowCursor : Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
    }
    std::function<void()> clicked;
    std::function<void()> dragged;
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QColor a = col(Theme::accentDefault()), ink = col(Theme::textPrimary()), warn = col(Theme::accentWarning());
        QColor bg = m_on ? a : ink;
        bg.setAlphaF(m_on ? 0.16 : underMouse() ? 0.09 : 0.05);
        p.setPen(m_on || m_fresh ? QPen(a, 1.2) : m_ghost ? QPen(warn, 1.2, Qt::DashLine) : Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(QRectF(1, 1, width() - 2, height() - 2), 10, 10);
        if (m_ghost) p.setOpacity(0.55);
        const QPointF c(width() / 2.0, 33);
        // órbitas tracejadas
        QColor orb = ink; orb.setAlphaF(0.22);
        QPen op(orb, 1, Qt::DashLine);
        op.setDashPattern({ 2, 3 });
        p.setPen(op);
        p.setBrush(Qt::NoBrush);
        const int n = int(m_planets.size());
        for (int i = 0; i < std::min(3, n); i++) { const double r = 12 + i * 2.7; p.drawEllipse(c, r, r); }
        // o sol
        QRadialGradient g(c, 12);
        if (m_hole) { g.setColorAt(0, Qt::black); g.setColorAt(0.5, Qt::black); g.setColorAt(0.6, QColor(255, 176, 112)); g.setColorAt(1, QColor(255, 120, 50, 0)); }
        else { QColor z = m_sun; z.setAlpha(0); QColor h = m_sun; h.setAlpha(110); g.setColorAt(0, m_sun); g.setColorAt(0.45, m_sun); g.setColorAt(0.5, h); g.setColorAt(1, z); }
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(c, 12, 12);
        // os mundos
        for (int i = 0; i < n; i++) {
            const double r = 12 + i * 2.7, an = -0.6 + i * 1.05, sz = i == 0 ? 11 : 9;
            p.drawPixmap(QRectF(c.x() + std::cos(an) * r - sz / 2, c.y() + std::sin(an) * r * 0.92 - sz / 2, sz, sz), m_planets[size_t(i)], QRectF(0, 0, m_planets[size_t(i)].width(), m_planets[size_t(i)].height()));
        }
        p.setOpacity(1);
        QFont f = font(); f.setPixelSize(12); f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(m_ghost ? warn : ink);
        p.drawText(QRectF(2, 64, width() - 4, 16), Qt::AlignHCenter | Qt::AlignVCenter, QFontMetrics(f).elidedText(m_name, Qt::ElideRight, width() - 6));
        QFont sf = font(); sf.setPixelSize(10);
        p.setFont(sf);
        QColor mu = col(Theme::textMuted());
        const QString pend = m_pending ? QStringLiteral("+%1").arg(m_pending) : QString();
        const QFontMetrics fm(sf);
        const int wSub = fm.horizontalAdvance(m_sub), wPend = pend.isEmpty() ? 0 : fm.horizontalAdvance(pend) + 10;
        double x = (width() - wSub - wPend) / 2.0;
        p.setPen(mu);
        p.drawText(QRectF(x, 81, wSub, 14), Qt::AlignVCenter, m_sub);
        if (!pend.isEmpty()) {
            const QRectF pr(x + wSub + 4, 81, wPend - 4, 14);
            p.setPen(QPen(warn, 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(pr.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
            p.setPen(warn);
            p.drawText(pr, Qt::AlignCenter, pend);
        }
    }
    void mousePressEvent(QMouseEvent* e) override { if (e->button() == Qt::LeftButton) { m_press = e->position(); m_down = true; } }
    void mouseMoveEvent(QMouseEvent* e) override { if (m_down && !m_ghost && dragged && (e->position() - m_press).manhattanLength() > 6) { m_down = false; dragged(); } }
    void mouseReleaseEvent(QMouseEvent* e) override { if (m_down && rect().contains(e->position().toPoint()) && clicked) { m_down = false; clicked(); } m_down = false; }
private:
    QColor m_sun;
    bool m_hole;
    std::vector<QPixmap> m_planets;
    QString m_name, m_sub;
    int m_pending;
    bool m_on, m_ghost, m_fresh;
    QPointF m_press;
    bool m_down = false;
};

// Um pedaço da prateleira que aceita mundo solto em cima: soltar dentro já pertence.
class DropBox : public QFrame {
public:
    DropBox(Livre* owner, const QString& parent, const std::function<bool(const QString&)>& can, const std::function<void(const QString&)>& drop, QWidget* w)
        : QFrame(w), m_can(can), m_drop(drop) { Q_UNUSED(owner); Q_UNUSED(parent); setAcceptDrops(true); }
protected:
    void dragEnterEvent(QDragEnterEvent* e) override { if (want(e->mimeData())) { e->acceptProposedAction(); setHot(true); } else e->ignore(); }
    void dragMoveEvent(QDragMoveEvent* e) override { if (want(e->mimeData())) e->acceptProposedAction(); else e->ignore(); }
    void dragLeaveEvent(QDragLeaveEvent*) override { setHot(false); }
    void dropEvent(QDropEvent* e) override
    {
        setHot(false);
        const QString k = QString::fromUtf8(e->mimeData()->data(QString::fromLatin1(MIME)));
        if (!m_can(k)) return;
        e->acceptProposedAction();
        m_drop(k);
    }
private:
    bool want(const QMimeData* m) const { return m->hasFormat(QString::fromLatin1(MIME)) && m_can(QString::fromUtf8(m->data(QString::fromLatin1(MIME)))); }
    void setHot(bool on) { setProperty("drop", on); style()->unpolish(this); style()->polish(this); }
    std::function<bool(const QString&)> m_can;
    std::function<void(const QString&)> m_drop;
};

// O potinho: uma bolinha por mundo criado, e nada que cobre o que falta.
class Pot : public QWidget {
public:
    Pot(std::vector<QPixmap> balls, QWidget* parent) : QWidget(parent), m_balls(std::move(balls))
    {
        const int per = 14;
        setFixedHeight(int((m_balls.size() + per - 1) / per) * 17 + 2);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const int per = std::max(1, width() / 17);
        for (size_t i = 0; i < m_balls.size(); i++) p.drawPixmap(int(i % size_t(per)) * 17, int(i / size_t(per)) * 17, 14, 14, m_balls[i]);
    }
private:
    std::vector<QPixmap> m_balls;
};
}

Livre::Livre(Panel* panel, Canvas* canvas, Doc* doc) : QWidget(panel), m_panel(panel), m_canvas(canvas), m_doc(doc)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("lvRoot"));
    QSettings st;
    m_fontFamily = st.value(QStringLiteral("galaxy/livreFontFamily"), m_fontFamily).toString();
    m_align = st.value(QStringLiteral("galaxy/livreAlign"), m_align).toString();
    m_lineH = std::clamp(st.value(QStringLiteral("galaxy/livreLine"), 175).toInt(), 100, 250);
    m_focusMode = st.value(QStringLiteral("galaxy/livreFocus"), false).toBool();
    m_leftW = std::clamp(st.value(QStringLiteral("galaxy/livreLeftW"), 292).toInt(), 240, 480);
    m_rightW = std::clamp(st.value(QStringLiteral("galaxy/livreRightW"), 384).toInt(), 320, 720);
    m_size = std::clamp(st.value(QStringLiteral("galaxy/livreSize"), 17).toInt(), 11, 32);
    m_lastGaveta = st.value(QStringLiteral("galaxy/livreGaveta"), false).toBool();
    m_lastDrawer = st.value(QStringLiteral("galaxy/livreDrawer")).toString();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 10, 14, 14);
    root->setSpacing(10);
    auto* top = new QHBoxLayout();
    top->setSpacing(10);
    top->addWidget(lab(tr("CRIAÇÃO LIVRE"), QStringLiteral("lvBrand"), this));
    // o nome da galáxia abre a mesma lista de mapas do mapa: trocar de mapa, criar um novo (Terra ou Galáxia), renomear
    m_brandName = btn(QString(), QStringLiteral("lvBrandName"), this);
    m_brandName->setToolTip(tr("Trocar de mapa ou criar um novo"));
    connect(m_brandName, &QPushButton::clicked, this, [this]() {
        if (!m_panel->fillMapMenu) return;
        flushSheet();
        QMenu menu(this);
        m_panel->fillMapMenu(menu);
        menu.exec(m_brandName->mapToGlobal(QPoint(0, m_brandName->height() + 4)));
        // renomear acontece no título do mapa: abre o mapa pra caixa de texto aparecer
        if (m_panel->m_titleEdit && m_panel->m_titleEdit->isVisible() && isVisible()) QTimer::singleShot(0, m_panel, [this]() { m_panel->closeLivre(); m_panel->m_titleEdit->setFocus(); });
    });
    top->addWidget(m_brandName);
    top->addStretch();
    auto* back = btn(tr("Ver no mapa"), QStringLiteral("lvTopBtn"), this);
    back->setToolTip(tr("O que você cria aqui aparece no mapa da galáxia."));
    connect(back, &QPushButton::clicked, this, [this]() { QTimer::singleShot(0, m_panel, [this]() { m_panel->closeLivre(); }); });
    top->addWidget(back);
    root->addLayout(top);

    m_cols = new QWidget(this);
    auto* cols = new QHBoxLayout(m_cols);
    cols->setContentsMargins(0, 0, 0, 0);
    cols->setSpacing(2);
    // a esquerda: a prateleira rola; o minimapa fica no pé
    m_leftCol = new QWidget(m_cols);
    m_leftCol->setObjectName(QStringLiteral("lvLeftCol"));
    m_leftCol->setAttribute(Qt::WA_StyledBackground, true);
    m_leftCol->setFixedWidth(m_leftW);
    auto* lc = new QVBoxLayout(m_leftCol);
    lc->setContentsMargins(1, 1, 1, 1);
    lc->setSpacing(0);
    m_left = new QScrollArea(m_leftCol);
    m_left->setObjectName(QStringLiteral("lvPaneIn"));
    m_left->setWidgetResizable(true);
    m_left->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lc->addWidget(m_left, 1);
    buildMapSection(lc);
    cols->addWidget(m_leftCol);
    // as duas bordas: a da prateleira e a da direita
    auto* gl = new Grip(m_cols);
    auto leftStart = std::make_shared<int>(0);
    gl->pressed = [this, leftStart]() { *leftStart = m_leftW; };
    gl->moved = [this, leftStart](int dx) { m_leftW = std::clamp(*leftStart + dx, 240, 480); m_leftCol->setFixedWidth(m_leftW); if (m_nav->isVisible()) placeNav(); };
    gl->released = [this]() { QSettings().setValue(QStringLiteral("galaxy/livreLeftW"), m_leftW); };
    gl->reset = [this]() { m_leftW = 292; m_leftCol->setFixedWidth(m_leftW); QSettings().setValue(QStringLiteral("galaxy/livreLeftW"), m_leftW); if (m_nav->isVisible()) placeNav(); };
    cols->addWidget(gl);
    m_center = new QWidget(m_cols);
    m_center->setObjectName(QStringLiteral("lvCenter"));
    auto* cl = new QVBoxLayout(m_center);
    cl->setContentsMargins(0, 0, 0, 0);
    cols->addWidget(m_center, 1);
    auto* gr = new Grip(m_cols);
    auto rightStart = std::make_shared<int>(0);
    gr->pressed = [this, rightStart]() { *rightStart = m_rightCol->width(); };
    gr->moved = [this, rightStart](int dx) { m_rightW = std::clamp(*rightStart - dx, 320, 720); m_rightCol->setFixedWidth(m_rightHead->isVisible() ? std::max(480, m_rightW) : m_rightW); };
    gr->released = [this]() { QSettings().setValue(QStringLiteral("galaxy/livreRightW"), m_rightW); renderRight(); };
    gr->reset = [this]() { m_rightW = 384; QSettings().setValue(QStringLiteral("galaxy/livreRightW"), m_rightW); renderRight(); };
    cols->addWidget(gr);
    m_rightCol = new QWidget(m_cols);
    m_rightCol->setObjectName(QStringLiteral("lvRightCol"));
    m_rightCol->setAttribute(Qt::WA_StyledBackground, true);
    m_rightCol->setFixedWidth(m_rightW);
    auto* rc = new QVBoxLayout(m_rightCol);
    rc->setContentsMargins(1, 1, 1, 1);
    rc->setSpacing(0);
    // o fechar da ficha completa mora aqui, colado no topo, e não some quando a ficha rola
    m_rightHead = new QWidget(m_rightCol);
    m_rightHead->setObjectName(QStringLiteral("lvRightHead"));
    m_rightHead->setAttribute(Qt::WA_StyledBackground, true);
    auto* rh = new QHBoxLayout(m_rightHead);
    rh->setContentsMargins(12, 9, 14, 9);
    auto* fxClose = btn(QStringLiteral("‹  ") + tr("Fechar ficha"), QStringLiteral("lvFxClose"), m_rightHead);
    fxClose->setToolTip(tr("Voltar pra ficha resumida (Esc)"));
    connect(fxClose, &QPushButton::clicked, this, [this]() { m_fichaOpen = false; QTimer::singleShot(0, this, &Livre::renderRight); });
    rh->addWidget(fxClose);
    rh->addStretch();
    rh->addWidget(lab(tr("FICHA TÉCNICA COMPLETA"), QStringLiteral("lvFxEye"), m_rightHead));
    m_rightHead->hide();
    rc->addWidget(m_rightHead);
    m_right = new QScrollArea(m_rightCol);
    m_right->setObjectName(QStringLiteral("lvPaneIn"));
    m_right->setWidgetResizable(true);
    m_right->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rc->addWidget(m_right, 1);
    cols->addWidget(m_rightCol);
    root->addWidget(m_cols, 1);

    m_nav = new QFrame(this);
    m_nav->setObjectName(QStringLiteral("lvNav"));
    m_nav->hide();

    m_toast = new QFrame(this);
    m_toast->setObjectName(QStringLiteral("lvToast"));
    auto* th = new QHBoxLayout(m_toast);
    th->setContentsMargins(14, 8, 8, 8);
    th->setSpacing(12);
    m_toastText = lab(QString(), QStringLiteral("lvToastText"), m_toast);
    th->addWidget(m_toastText);
    m_toastUndo = btn(tr("Desfazer"), QStringLiteral("lvToastBtn"), m_toast);
    connect(m_toastUndo, &QPushButton::clicked, this, [this]() { m_toast->hide(); flushSheet(); m_canvas->undo(); m_edFor.clear(); if (!valid(m_sel)) m_sel.clear(); render(); });
    th->addWidget(m_toastUndo);
    m_toast->hide();
    m_toastTimer.setSingleShot(true);
    connect(&m_toastTimer, &QTimer::timeout, m_toast, &QWidget::hide);

    m_sheetTimer.setSingleShot(true);
    m_sheetTimer.setInterval(600);
    connect(&m_sheetTimer, &QTimer::timeout, this, &Livre::flushSheet);
    m_peekTimer.setSingleShot(true);
    connect(&m_peekTimer, &QTimer::timeout, this, [this]() { if (m_peekAt && m_peekAt->underMouse()) showPeek(m_peekAt, m_peekKey); });
    qApp->installEventFilter(this);
    applyTheme();
}

void Livre::applyTheme()
{
    const QColor a = col(Theme::accentDefault()), warn = col(Theme::accentWarning()), ink = col(Theme::textPrimary());
    QString s = QStringLiteral(
        "QWidget#lvRoot { background: $void; }"
        "QLabel#lvBrand { color: $accent; font-family: 'IM FELL English SC', Georgia, serif; font-size: 17px; letter-spacing: 2px; }"
        "QLabel#lvSub { color: $muted; font-size: 13px; }"
        "QPushButton#lvTopBtn { background: $hover; border: 1px solid $line; border-radius: 8px; color: $ink; font-size: 12px; padding: 6px 12px; }"
        "QPushButton#lvTopBtn:hover { border-color: $accent; }"
        "QScrollArea#lvPane { background: $pane; border: 1px solid $line; border-radius: 12px; }"
        "QPushButton#lvBrandName { background: transparent; border: none; border-radius: 6px; color: $muted; font-size: 13px; padding: 3px 7px; }"
        "QPushButton#lvBrandName:hover { background: $hover; color: $ink; }"
        "QWidget#lvLeftCol { background: $pane; border: 1px solid $line; border-radius: 12px; }"
        "QWidget#lvMapSec { background: transparent; border: none; border-top: 1px solid $line; }"
        "QPushButton#lvMapTitle { background: transparent; border: none; color: $ink; font-size: 12px; font-weight: 600; padding: 2px 4px 2px 2px; text-align: left; }"
        "QPushButton#lvMapTitle:hover { color: $accent; }"
        "QLabel#lvMapSub { color: $muted; font-size: 11.5px; }"
        "QToolButton#lvMapExp { background: transparent; border: none; border-radius: 6px; padding: 4px; }"
        "QToolButton#lvMapExp:hover { background: $hover; }"
        "QToolButton#lvMapBtn { background: rgba(10,14,22,0.78); color: #dfe6f0; border: 1px solid rgba(236,241,250,0.14); border-radius: 6px; font-size: 11px; font-weight: 600; min-width: 24px; min-height: 22px; max-height: 22px; padding: 0 5px; }"
        "QToolButton#lvMapBtn:hover { background: rgba(30,38,52,0.92); color: #ffffff; }"
        "QToolButton#lvMapBtn[on=\"true\"] { color: $accent; border-color: $accent; }"
        "QToolButton#lvMapBtn[big=\"true\"] { min-width: 32px; min-height: 30px; max-height: 30px; font-size: 13px; border-radius: 8px; }"
        "QFrame#lvMapOv { background: #03050a; }"
        "QWidget#lvMapBar { background: $pane; border-bottom: 1px solid $line; }"
        "QLabel#lvMapBigTitle { color: $ink; font-size: 17px; font-weight: 600; }"
        "QWidget#lvMapFoot { background: #03050a; }"
        "QLabel#lvMapHint { color: rgba(236,241,250,0.62); font-size: 12px; }"
        "QPushButton#lvMapGo { background: $accent; color: #0b1220; border: none; border-radius: 15px; padding: 6px 16px; font-weight: 700; font-size: 13px; }"
        "QPushButton#lvMapGo:hover { background: $accent; }"
        "QScrollArea#lvPane > QWidget > QWidget { background: transparent; }"
        "QWidget#lvRightCol { background: $pane; border: 1px solid $line; border-radius: 12px; }"
        "QScrollArea#lvPaneIn { background: transparent; border: none; }"
        "QScrollArea#lvPaneIn > QWidget > QWidget { background: transparent; }"
        "QWidget#lvRightHead { background: $pane; border: none; border-bottom: 1px solid $line; border-top-left-radius: 12px; border-top-right-radius: 12px; }"
        "QPushButton#lvFxClose { background: $accentSoft; border: 1px solid $accent; border-radius: 13px; color: $accent; font-size: 12px; font-weight: 600; padding: 4px 12px 4px 9px; }"
        "QPushButton#lvFxClose:hover { background: $accent; color: $pane; }"
        "QFrame#lvCmpRow { background: transparent; border: none; border-radius: 9px; }"
        "QFrame#lvCmpRow:hover { background: $hover; }"
        "QFrame#lvCmpFixed { background: $accentSoft; border: none; border-radius: 9px; }"
        "QLineEdit#lvCmpName { background: transparent; border: none; border-bottom: 1px solid $line2; color: $ink; font-family: 'Source Serif 4', Georgia, serif; font-size: 15px; font-weight: 600; padding: 3px 2px; }"
        "QLineEdit#lvCmpName:focus { border-bottom-color: $accent; }"
        "QLineEdit#lvCmpPlanet { background: transparent; border: none; border-bottom: 1px solid transparent; color: $ink; font-size: 13px; font-weight: 600; padding: 1px 0; }"
        "QLineEdit#lvCmpPlanet:hover, QLineEdit#lvCmpPlanet:focus { border-bottom-color: $line2; }"
        "QPushButton#lvCmpType { background: transparent; border: none; color: $muted; font-size: 11px; padding: 0; text-align: left; }"
        "QPushButton#lvCmpType:hover { color: $accent; }"
        "QPushButton#lvCmpThumb { background: transparent; border: none; border-radius: 19px; }"
        "QPushButton#lvCmpThumb:hover { background: $hover; }"
        "QToolButton#lvCmpStar { background: transparent; border: 1.5px solid transparent; border-radius: 13px; }"
        "QToolButton#lvCmpStar:hover { background: $hover; }"
        "QToolButton#lvCmpStar:checked { border-color: $accent; }"
        "QPushButton#lvCmpStep { background: transparent; border: 1px solid $line2; border-radius: 13px; color: $ink; font-size: 15px; padding: 0; }"
        "QPushButton#lvCmpStep:hover { border-color: $accent; color: $accent; }"
        "QPushButton#lvCmpStep:disabled { color: $line2; border-color: $line; }"
        "QLabel#lvCmpN { color: $ink; font-size: 15px; font-weight: 700; }"
        "QPushButton#lvPill:disabled { color: $line2; }"
        "QLabel#lvPeekFact { color: $ink; font-size: 13px; font-weight: 600; }"
        "QLabel#lvPeekRow { color: $ink; font-size: 12px; font-weight: 600; }"
        "QLabel#lvPeekPend { color: $warn; font-size: 11px; border: 1px dashed $warn; border-radius: 8px; padding: 4px 8px; }"
        "QWidget#lvPaneBody { background: transparent; }"
        "QLabel#lvCount { color: $ink; font-size: 26px; font-weight: 700; }"
        "QLabel#lvCountSub { color: $muted; font-size: 12px; }"
        "QPushButton#lvQuiet { background: transparent; border: 1px solid transparent; border-radius: 8px; color: $muted; font-size: 12px; padding: 4px 8px; }"
        "QPushButton#lvQuiet:hover { color: $ink; border-color: $line; }"
        "QFrame#lvGroup { border: 1px solid $line; border-radius: 12px; }"
        "QFrame#lvGroup[drop=\"true\"], QFrame#lvTie[drop=\"true\"] { border: 1.5px dashed $accent; }"
        "QFrame#lvTie { background: $hover; border: 1px solid transparent; border-radius: 10px; }"
        "QPushButton#lvGroupName { background: transparent; border: none; text-align: left; padding: 0; }"
        "QLabel#lvGroupLabel { color: $ink; font-size: 13px; font-weight: 700; }"
        "QLabel#lvGroupLabel[on=\"true\"] { color: $accent; }"
        "QLabel#lvSmall { color: $muted; font-size: 11px; }"
        "QToolButton#lvArrow { background: transparent; border: none; color: $muted; font-size: 11px; padding: 0 2px; }"
        "QPushButton#lvTieName { background: transparent; border: none; color: $ink; font-size: 12px; font-weight: 600; text-align: left; padding: 0; }"
        "QPushButton#lvTieName[on=\"true\"] { color: $accent; }"
        "QPushButton#lvPlus { background: transparent; border: 1.5px dashed $line2; border-radius: 27px; color: $muted; font-size: 22px; }"
        "QPushButton#lvPlus:hover { border-color: $accent; color: $accent; }"
        "QFrame#lvTileDrop { border: 1.5px solid transparent; border-radius: 11px; }"
        "QFrame#lvTileDrop[drop=\"true\"] { border: 1.5px dashed $accent; }"
        "QLineEdit#lvInline { background: $pane; border: 1px solid $accent; border-radius: 8px; color: $ink; font-size: 12px; padding: 4px 8px; }"
        "QToolButton#lvGhostOk, QToolButton#lvGhostNo { background: $pane; border: 1px solid $line; border-radius: 10px; color: $ink; font-size: 11px; padding: 0; }"
        "QToolButton#lvGhostOk:hover { border-color: $accent; color: $accent; }"
        "QToolButton#lvGhostNo:hover { border-color: $warn; color: $warn; }"
        // a folha
        "QFrame#lvFolha { background: $sheet; border: 1px solid $pborder; border-radius: 8px; }"
        "QWidget#lvEdBar { background: $sheet; border-bottom: 1px solid $pline; }"
        "QToolButton#lvEd { background: transparent; border: 1px solid transparent; border-radius: 6px; color: $pmuted; font-size: 13px; min-width: 26px; min-height: 26px; }"
        "QToolButton#lvEd:hover { background: $phover; color: $pink; }"
        "QToolButton#lvEd:checked { background: $phover; color: $pink; border-color: $pline2; }"
        "QToolButton#lvEdFont { background: transparent; border: 1px solid $pline; border-radius: 6px; color: $pink; font-size: 12px; padding: 3px 8px; }"
        "QLabel#lvEdSize { color: $pink; font-size: 12px; min-width: 22px; }"
        "QLineEdit#lvNameInput { background: transparent; border: none; color: $pink; font-family: 'Source Serif 4', Georgia, serif; font-size: 30px; font-weight: 600; padding: 0; }"
        "QTextEdit#lvBody { background: transparent; border: none; padding: 0px; margin: 0px; color: $pink; selection-background-color: $accentSoft; }"
        "QLabel#lvHintBig { color: $ink; font-size: 16px; font-weight: 600; }"
        "QLabel#lvWlKind { color: $accent; font-size: 12px; font-weight: 600; }"
        "QLabel#lvWlSep { color: $pmuted; font-size: 12px; }"
        "QPushButton#lvWlLink { background: transparent; border: none; color: $pmuted; font-size: 12px; padding: 0; }"
        "QPushButton#lvWlLink:hover { color: $accent; text-decoration: underline; }"
        "QWidget#lvSheetFoot { background: $sheet; border-top: 1px solid $pline; border-bottom-left-radius: 8px; border-bottom-right-radius: 8px; }"
        "QLabel#lvSheetCount { color: $pmuted; font-size: 11px; }"
        "QWidget#lvGrip { background: transparent; }"
        "QWidget#lvGrip:hover, QWidget#lvGrip[on=\"true\"] { background: $accentSoft; }"
        "QLabel#lvHint { color: $muted; font-size: 13px; }"
        "QPushButton#lvWhere { background: transparent; border: 1px solid $pline; border-radius: 10px; color: $pmuted; font-size: 11px; padding: 2px 8px; }"
        "QPushButton#lvWhere:hover { border-color: $accent; color: $pink; }"
        "QPushButton#lvEnd { background: $phover; border: 1px solid $pline; border-radius: 16px; color: $pink; font-size: 13px; font-weight: 600; padding: 3px 12px 3px 4px; text-align: left; }"
        "QPushButton#lvEnd:hover { border-color: $accent; }"
        "QPushButton#lvLinkType { background: transparent; border: 1px dashed $pline2; border-radius: 12px; color: $pink; font-size: 12px; padding: 3px 10px; }"
        "QPushButton#lvLinkType[unset=\"true\"] { color: $pmuted; }"
        // balões, aviso, prévia
        "QFrame#lvPop, QFrame#lvPeek { background: $pane; border: 1px solid $line2; border-radius: 10px; }"
        "QLabel#lvPopTitle { color: $muted; font-size: 11px; font-weight: 700; letter-spacing: 1px; padding: 6px 8px 2px; }"
        "QLabel#lvPopHead { color: $ink; font-family: 'Source Serif 4', Georgia, serif; font-size: 15px; font-weight: 600; padding: 6px 8px 2px; }"
        "QPushButton#lvOpt { background: transparent; border: 1px solid transparent; border-radius: 7px; color: $ink; font-size: 13px; padding: 5px 8px; text-align: left; }"
        "QPushButton#lvOpt:hover { background: $hover; }"
        "QPushButton#lvOpt[on=\"true\"] { background: $accentSoft; border-color: $accent; }"
        "QPushButton#lvTy { background: transparent; border: 1px solid transparent; border-radius: 9px; color: $ink; font-size: 11px; padding: 4px 2px; }"
        "QPushButton#lvTy:hover { background: $hover; border-color: $line; }"
        "QPushButton#lvTy[on=\"true\"] { background: $accentSoft; border-color: $accent; }"
        "QFrame#lvPopSep { background: $line; max-height: 1px; min-height: 1px; margin: 4px 6px; }"
        "QLineEdit#lvPopSearch { background: $hover; border: 1px solid $line; border-radius: 7px; color: $ink; font-size: 12px; padding: 5px 8px; margin: 2px 4px 4px; }"
        "QFrame#lvToast { background: $ink; border-radius: 10px; }"
        "QLabel#lvToastText { color: $pane; font-size: 13px; }"
        "QPushButton#lvToastBtn { background: transparent; border: 1px solid $pane; border-radius: 7px; color: $pane; font-size: 12px; font-weight: 600; padding: 4px 10px; }"
        "QLabel#lvPeekName { color: $ink; font-size: 14px; font-weight: 700; }"
        "QLabel#lvPeekText { color: $ink; font-size: 12px; }"
        "QLabel#lvPeekText[none=\"true\"] { color: $muted; font-style: italic; }"
        "QLabel#lvPeekFoot { color: $muted; font-size: 11px; border-top: 1px solid $line; padding-top: 6px; }"
        // a direita
        "QWidget#lvCrumbs QPushButton { background: transparent; border: none; color: $muted; font-size: 12px; padding: 0; }"
        "QWidget#lvCrumbs QPushButton:hover { color: $ink; }"
        "QLabel#lvCrumbHere { color: $ink; font-size: 12px; font-weight: 600; }"
        "QLabel#lvCrumbSep { color: $muted; font-size: 12px; }"
        "QFrame#lvSysv { background: $hover; border: 1px solid $line; border-radius: 12px; }"
        "QPushButton#lvSysName { background: transparent; border: none; color: $ink; font-family: 'Source Serif 4', Georgia, serif; font-size: 18px; font-weight: 600; text-align: left; padding: 0; }"
        "QLabel#lvSysPlain { color: $ink; font-family: 'Source Serif 4', Georgia, serif; font-size: 18px; font-weight: 600; }"
        "QPushButton#lvSysStar { background: transparent; border: none; color: $muted; font-size: 12px; text-align: left; padding: 0; }"
        "QPushButton#lvSysStar:hover { color: $accent; }"
        "QPushButton#lvOrbit { background: transparent; border: 1px solid transparent; border-radius: 10px; text-align: left; }"
        "QPushButton#lvOrbit:hover { background: $hover; }"
        "QPushButton#lvOrbit[on=\"true\"] { background: $accentSoft; border-color: $accent; }"
        "QPushButton#lvOrbit[near=\"true\"] { border-color: $line2; }"
        "QLabel#lvOrbitName { color: $ink; font-size: 13px; font-weight: 600; }"
        "QLabel#lvOrbitName[ghost=\"true\"] { color: $warn; }"
        "QPushButton#lvRow { background: transparent; border: 1px solid transparent; border-radius: 9px; text-align: left; }"
        "QPushButton#lvRow:hover { background: $hover; }"
        "QPushButton#lvRow[on=\"true\"] { background: $accentSoft; border-color: $accent; }"
        "QLabel#lvRowName { color: $ink; font-size: 13px; }"
        "QFrame#lvFicha { background: transparent; }"
        "QFrame#lvWin { background: #05070c; border: 1px solid $line; border-radius: 12px; }"
        "QLabel#lvWinSmall { color: rgba(236,241,250,0.6); font-size: 10px; font-weight: 700; letter-spacing: 1px; background: transparent; }"
        "QLabel#lvWinName { color: #eef2f8; font-family: 'Source Serif 4', Georgia, serif; font-size: 17px; font-weight: 600; background: transparent; }"
        "QPushButton#lvWinBtn { background: rgba(10,14,22,0.72); border: 1px solid rgba(236,241,250,0.18); border-radius: 12px; color: #e6ecf5; font-size: 11px; padding: 3px 9px; }"
        "QPushButton#lvWinBtn:hover { border-color: $accent; }"
        "QLabel#lvWinZoom { color: #e6ecf5; font-size: 11px; background: transparent; min-width: 34px; }"
        "QPushButton#lvProp { background: transparent; border: none; border-bottom: 1px solid $line; border-radius: 0; text-align: left; padding: 7px 2px; }"
        "QPushButton#lvProp:hover { background: $hover; }"
        "QLabel#lvPropK { color: $muted; font-size: 12px; }"
        "QLabel#lvPropV { color: $ink; font-size: 13px; font-weight: 600; }"
        "QLabel#lvPropV[unset=\"true\"] { color: $muted; font-weight: 400; font-style: italic; }"
        "QPushButton#lvFxOpen { background: transparent; border: 1px solid $line; border-radius: 9px; color: $accent; font-size: 12px; font-weight: 600; padding: 7px 10px; }"
        "QPushButton#lvFxOpen:hover { border-color: $accent; }"
        "QLabel#lvSideTitle { color: $muted; font-size: 11px; font-weight: 700; letter-spacing: 1px; }"
        "QPushButton#lvOrder { background: $warnSoft; border: 1px dashed $warn; border-radius: 9px; color: $ink; font-size: 12px; padding: 7px 10px; text-align: left; }"
        "QPushButton#lvOrder:hover { background: $warnStrong; }"
        "QLabel#lvFxEye { color: $muted; font-size: 11px; font-weight: 700; letter-spacing: 1px; }"
        "QLabel#lvFxName { color: $ink; font-family: 'Source Serif 4', Georgia, serif; font-size: 22px; font-weight: 600; }"
        "QLabel#lvFxSec { color: $accent; font-size: 11px; font-weight: 700; letter-spacing: 1px; padding-top: 6px; }"
        "QLabel#lvFxNote { color: $muted; font-size: 12px; }"
        "QLabel#lvFxKey { color: $muted; font-size: 12px; }"
        "QLineEdit#lvFxIn { background: transparent; border: none; border-bottom: 1px solid $line; color: $ink; font-size: 13px; padding: 3px 2px; }"
        "QLineEdit#lvFxIn:focus { border-bottom-color: $accent; }"
        "QLineEdit#lvFxIn[gen=\"true\"] { color: $warn; border-bottom: 1px dashed $warn; }"
        "QFrame#lvGenBan { background: $warnSoft; border: 1px dashed $warn; border-radius: 9px; }"
        "QLabel#lvGenText { color: $ink; font-size: 12px; }"
        "QPushButton#lvGenBtn { background: transparent; border: 1px solid $warn; border-radius: 7px; color: $ink; font-size: 11px; padding: 3px 8px; }"
        // a gaveta flutuante, com a receita da gaveta de verdade
        "QFrame#lvNav { background: $pane; border: 1px solid $line2; border-radius: 12px; }"
        "QWidget#lvNavHead { border-bottom: 1px solid $line; }"
        "QPushButton#lvNavTitle { background: transparent; border: none; color: $ink; font-family: 'Lora', 'Source Serif 4', Georgia, serif; font-size: 16px; font-weight: 600; text-align: left; padding: 0; }"
        "QToolButton#lvNavMini { background: transparent; border: 1px solid transparent; border-radius: 7px; padding: 3px; }"
        "QToolButton#lvNavMini:hover { background: $hover; }"
        "QToolButton#lvNavMini:checked { background: $accentSoft; border-color: $accent; }"
        "QPushButton#lvNavCreate { background: $accentFaint; color: $ink; border: 1px solid $accent; border-radius: @radius-control; padding: 9px 12px; font-family: 'Lora', 'Crimson Text', serif; font-size: 14px; font-weight: 600; min-height: 22px; text-align: left; }"
        "QPushButton#lvNavCreate:hover { background: $accentSoft; }"
        "QPushButton#lvPill { background: transparent; color: $muted; border: 1px dashed $line2; border-radius: @radius-control; padding: 2px 10px; font-family: 'Lora', 'Crimson Text', serif; font-size: 11px; min-height: 20px; }"
        "QPushButton#lvPill:hover, QPushButton#lvPill[on=\"true\"] { background: $hover; color: $ink; border-color: $accent; }"
        "QLabel#lvNavSec { color: $muted; font-size: 11px; font-weight: 700; letter-spacing: 1px; }"
        "QPushButton#lvTn { background: transparent; border: 1px solid transparent; border-radius: 7px; text-align: left; }"
        "QPushButton#lvTn:hover { background: $hover; }"
        "QPushButton#lvTn[on=\"true\"] { background: $accentSoft; border-color: $accent; }"
        "QLabel#lvTnName { color: $ink; font-size: 13px; }"
        "QLabel#lvTnName[ghost=\"true\"] { color: $warn; }"
        "QToolButton#lvRowAdd { background: transparent; border: none; border-radius: 6px; padding: 2px; }"
        "QToolButton#lvRowAdd:hover { background: $hover; }"
        "QLabel#lvTag { color: $muted; font-size: 10px; border: 1px solid $line; border-radius: 8px; padding: 0 6px; }"
        "QLabel#lvEmpty { color: $muted; font-size: 12px; font-style: italic; padding: 2px 8px; }"
        "QFrame#lvAsk { background: $hover; border: 1px solid $line2; border-radius: 10px; }"
        "QLabel#lvAskTitle { color: $ink; font-size: 13px; font-weight: 600; }"
        "QToolButton#lvSeg { background: transparent; border: 1px solid $line; color: $muted; font-size: 12px; padding: 4px 8px; }"
        "QToolButton#lvSeg:checked { background: $accentSoft; border-color: $accent; color: $ink; }"
        "QPushButton#lvGo { background: $accent; border: none; border-radius: 8px; color: white; font-size: 12px; font-weight: 600; padding: 6px 12px; }"
        "QComboBox#lvCombo { background: $pane; border: 1px solid $line; border-radius: 7px; color: $ink; font-size: 12px; padding: 3px 8px; }"
        "QLabel#lvAskNote { color: $muted; font-size: 11px; }"
        "QLineEdit#lvNavSearch { background: $hover; border: 1px solid $line; border-radius: 8px; color: $ink; font-size: 12px; padding: 5px 8px; }");
    const QColor sheet = col(Theme::editorBackground()), pink = col(Theme::editorTextColor()), pane = col(Theme::panelBackground());
    const QColor pm = QColor::fromRgbF(float(pink.redF() * 0.68 + sheet.redF() * 0.32), float(pink.greenF() * 0.68 + sheet.greenF() * 0.32), float(pink.blueF() * 0.68 + sheet.blueF() * 0.32));
    const QList<QPair<QString, QString>> tokens = {
        { "$accentSoft", rgba(a, 0.17) }, { "$accentFaint", rgba(a, 0.10) }, { "$accent", a.name() },
        { "$warnStrong", rgba(warn, 0.3) }, { "$warnSoft", rgba(warn, 0.14) }, { "$warn", warn.name() },
        { "$void", Theme::chromeBackground() }, { "$pane", pane.name() }, { "$line2", rgba(ink, 0.28) }, { "$line", rgba(ink, 0.14) },
        { "$ink", ink.name() }, { "$muted", Theme::textMuted() }, { "$hover", rgba(ink, 0.07) },
        { "$sheet", sheet.name() }, { "$pborder", rgba(pink, 0.18) }, { "$pline2", rgba(pink, 0.3) }, { "$pline", rgba(pink, 0.14) },
        { "$pink", pink.name() }, { "$pmuted", pm.name() }, { "$phover", rgba(pink, 0.07) },
    };
    for (const auto& t : tokens) s.replace(t.first, t.second);
    setStyleSheet(Theme::qss(s));
    m_balls.clear();
    if (isVisible()) { m_edFor.clear(); render(); }
}

// ── abrir e fechar ──
void Livre::open(const QString& key)
{
    m_sel = valid(key) && kindOf(key) != LK::Gal ? key : QString();
    if (!m_sel.isEmpty() && kindOf(m_sel) != LK::Doc && kindOf(m_sel) != LK::Link) m_navOpen = true;
    m_edFor = QStringLiteral("\x01");
    m_balls.clear();
    render();
}
void Livre::closeIt()
{
    flushSheet();
    closePop();
    hidePeek();
    m_nav->hide();
}
bool Livre::escape()
{
    if (m_pop) { closePop(); return true; }
    if (m_draw) return true;
    if (m_bigMap) { closeBigMap(); return true; }
    if (m_bigView) { closeBigView(); return true; }
    if (m_fichaOpen && m_rightHead->isVisible()) { m_fichaOpen = false; renderRight(); return true; }
    if (m_nav->isVisible() && !m_navPinned) { m_navOpen = false; m_nav->hide(); return true; }
    if (!m_sel.isEmpty()) { select(parentKey(m_sel)); return true; }
    return false;
}
void Livre::select(const QString& k)
{
    QTimer::singleShot(0, this, [this, k]() {
        m_sel = valid(k) && kindOf(k) != LK::Gal ? k : QString();
        render();
    });
}
void Livre::toast(const QString& text, bool withUndo)
{
    if (text.isEmpty()) { m_toast->hide(); return; }
    m_toastText->setText(text);
    m_toastUndo->setVisible(withUndo);
    m_toast->adjustSize();
    m_toast->move((width() - m_toast->width()) / 2, height() - m_toast->height() - 26);
    m_toast->show();
    m_toast->raise();
    m_toastTimer.start(withUndo ? 7000 : 3000);
}
void Livre::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (m_toast->isVisible()) m_toast->move((width() - m_toast->width()) / 2, height() - m_toast->height() - 26);
    if (m_nav->isVisible()) placeNav();
    if (m_draw) m_draw->setGeometry(rect());
    if (m_bigView) m_bigView->setGeometry(rect());
    if (m_bigMap) m_bigMap->setGeometry(rect());
    closePop();
}

// ── desenhar tudo ──
void Livre::render()
{
    if (!m_doc->gal) return;
    if (!m_sel.isEmpty() && !valid(m_sel)) m_sel.clear();
    closePop();
    hidePeek();
    m_brandName->setText(m_doc->labelOf(QStringLiteral("GAL")) + QStringLiteral("  ▾"));
    if (m_edFor != m_sel) {
        flushSheet();
        renderCenter();
        m_edFor = m_sel;
        m_navAdd.clear(); m_rowAdd.clear();
        // abrir algo desdobra o grupo dele na prateleira
        if (!m_sel.isEmpty()) {
            QString top = m_sel;
            while (!parentKey(top).isEmpty()) top = parentKey(top);
            m_closed.remove(kindOf(top) == LK::Terr ? top : QStringLiteral("loose"));
        }
    }
    renderLeft();
    renderRight();
    renderNav();
    syncMap();
    if (!m_justMade.isEmpty()) QTimer::singleShot(900, this, [this]() { if (!m_justMade.isEmpty()) { m_justMade.clear(); renderLeft(); } });
}

QPixmap Livre::ball(const QString& k, int px, bool rings)
{
    const LK t = kindOf(k);
    if (t == LK::Build) return modelPix(buildType(k), buildSeed(k), px);
    Body* b = bodyOf(k);
    const int D = px * 2;
    if (!b) {
        QPixmap pm(D, D);
        pm.setDevicePixelRatio(2);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        QColor c = QColor(0x8e, 0x9d, 0xb4);
        if (t == LK::Terr) if (const Region* r = m_doc->resolve(k).region) c = QColor(r->color);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        if (t == LK::Terr) p.drawRoundedRect(QRectF(px * 0.1, px * 0.1, px * 0.8, px * 0.8), px * 0.2, px * 0.2);
        else p.drawEllipse(QRectF(px * 0.1, px * 0.1, px * 0.8, px * 0.8));
        return pm;
    }
    const bool withRings = rings && b->rings;
    const QString ck = QStringLiteral("%1|%2|%3|%4|%5").arg(quintptr(b)).arg(b->texRev).arg(int(b->type)).arg(px).arg(withRings ? (b->rings ? quintptr(b->rings.get()) : 0) : 0);
    auto it = m_balls.find(ck);
    if (it != m_balls.end()) return it.value();
    QImage out(D, D, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter g(&out);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::SmoothPixmapTransform);
    const double k0 = withRings ? 1 / (b->rings->outer * 1.08) : 1;
    const int S2 = std::max(4, int(D * 0.94 * k0));
    const Frame fr = b->frame.ax[0] != 0 || b->frame.ax[1] != 0 || b->frame.ax[2] != 0 ? b->frame : frameOf(0, 0);
    const SphereTable tab = sphereTable(S2, 0, 0.3, fr);
    QImage img(S2, S2, QImage::Format_RGBA8888);
    img.fill(Qt::transparent);
    const double n = std::sqrt(0.52 * 0.52 + 0.36 * 0.36 + 0.78 * 0.78);
    const V3 L { -0.52 / n, 0.36 / n, 0.78 / n };
    shadeSphere(img.bits(), tab, m_canvas->textureOf(*b, D > 120 ? 256 : 128), 0.6, &L, atmOf(*b));
    const double cx = D / 2.0, cy = D / 2.0, R = S2 / 2.0;
    if (withRings) m_canvas->ringOn(g, *b, cx, cy, R, false, 0.34);
    g.drawImage(QRectF(cx - R, cy - R, S2, S2), img);
    if (withRings) m_canvas->ringOn(g, *b, cx, cy, R, true, 0.34);
    g.end();
    QPixmap pm = QPixmap::fromImage(out);
    pm.setDevicePixelRatio(2);
    if (m_balls.size() > 600) m_balls.clear();
    m_balls.insert(ck, pm);
    return pm;
}
QColor Livre::starShown(StarKind k)
{
    switch (k) {
    case StarKind::G: return QColor(255, 207, 90); case StarKind::K: return QColor(255, 154, 69); case StarKind::M: return QColor(240, 96, 63);
    case StarKind::F: return QColor(255, 222, 120); case StarKind::A: return QColor(196, 214, 255); case StarKind::B: return QColor(150, 186, 255);
    case StarKind::O: return QColor(118, 160, 255); case StarKind::Gigante: return QColor(232, 69, 47); case StarKind::AnaBranca: return QColor(184, 204, 240);
    case StarKind::Neutrons: return QColor(120, 205, 240); case StarKind::BuracoNegro: return QColor(138, 109, 214); default: return QColor(255, 196, 140);
    }
}
QPixmap Livre::modelPix(const std::string& type, uint32_t seed, int px)
{
    const QString ck = QStringLiteral("s|%1|%2|%3").arg(QString::fromStdString(type)).arg(seed).arg(px);
    auto it = m_balls.find(ck);
    if (it != m_balls.end()) return it.value();
    QPixmap pm = QPixmap::fromImage(artImage(m_canvas, type, ItemKind::Struct, px, seed));
    pm.setDevicePixelRatio(2);
    m_balls.insert(ck, pm);
    return pm;
}
// O tipo escolhido vendo o planeta: um mundo de amostra de cada tipo, feito uma vez.
QPixmap Livre::typeThumb(Type t, int px)
{
    const QString ck = QStringLiteral("t|%1|%2").arg(int(t)).arg(px);
    auto it = m_balls.find(ck);
    if (it != m_balls.end()) return it.value();
    System s;
    s.seed = 4242;
    s.stars = { starOf(StarKind::G, 1, 4242) };
    s.kind = StarKind::G;
    refreshSystem(s);
    Body* b = nullptr;
    const bool moon = t == Type::Rochosa || t == Type::Gelada || t == Type::Vulcanica || t == Type::Temperada;
    if (moon) { Body* host = addPlanet(s, Type::Temperado, 1, 4242 ^ 77); host->moons.clear(); b = addMoon(s, *host, t, 4242); }
    else b = addPlanet(s, t, 1, 4242);
    const Texture tex = bodyTexture(*b, 128, 64);
    const int D = px * 2;
    const SphereTable tab = sphereTable(D, 0, 0.3, frameOf(0, 0));
    QImage img(D, D, QImage::Format_RGBA8888);
    img.fill(Qt::transparent);
    const double n = std::sqrt(0.52 * 0.52 + 0.36 * 0.36 + 0.78 * 0.78);
    const V3 L { -0.52 / n, 0.36 / n, 0.78 / n };
    shadeSphere(img.bits(), tab, tex, 0.6, &L, atmOf(*b));
    QPixmap pm = QPixmap::fromImage(img);
    pm.setDevicePixelRatio(2);
    m_balls.insert(ck, pm);
    return pm;
}

QWidget* Livre::starMark(QWidget* parent)
{
    auto* l = new QLabel(parent);
    l->setPixmap(starPix(col(Theme::accentWarning()), 12));
    l->setToolTip(tr("Capital"));
    l->setFixedSize(14, 14);
    return l;
}
QWidget* Livre::markOf(const QString& k, int px, QWidget* parent)
{
    auto* l = new QLabel(parent);
    l->setFixedSize(px + 2, px + 2);
    l->setAlignment(Qt::AlignCenter);
    l->setAttribute(Qt::WA_TransparentForMouseEvents);
    const LK t = kindOf(k);
    const QColor muted = col(Theme::textMuted()), a = col(Theme::accentDefault());
    if (t == LK::Planet || t == LK::Moon || t == LK::Build || t == LK::Terr) l->setPixmap(ball(k, px));
    else if (t == LK::Doc) l->setPixmap(carto::iconPixmap(QStringLiteral("galDoc"), muted, px));
    else if (t == LK::Place) l->setPixmap(carto::iconPixmap(QStringLiteral("galPlace"), muted, px));
    else if (t == LK::Link) l->setPixmap(carto::iconPixmap(QStringLiteral("link"), a, px));
    else if (t == LK::Sys) {
        QPixmap pm(px * 2, px * 2);
        pm.setDevicePixelRatio(2);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const System* s = sysOf(k);
        const QColor c = starShown(s ? s->stars[0].kind : StarKind::G);
        QColor z = c; z.setAlpha(0);
        QRadialGradient g(px / 2.0, px / 2.0, px / 2.0);
        g.setColorAt(0, c); g.setColorAt(0.45, c); g.setColorAt(0.5, QColor(c.red(), c.green(), c.blue(), 110)); g.setColorAt(1, z);
        p.setPen(Qt::NoPen); p.setBrush(g);
        p.drawEllipse(QRectF(0, 0, px, px));
        l->setPixmap(pm);
    }
    return l;
}
QWidget* Livre::ghostActs(const QString& k, QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(3);
    auto* y = new QToolButton(w);
    y->setObjectName(QStringLiteral("lvGhostOk"));
    y->setText(QStringLiteral("✓"));
    y->setToolTip(tr("Ficar com este"));
    y->setFixedSize(22, 20);
    y->setCursor(Qt::PointingHandCursor);
    auto* n = new QToolButton(w);
    n->setObjectName(QStringLiteral("lvGhostNo"));
    n->setText(QStringLiteral("✕"));
    n->setToolTip(tr("Dispensar"));
    n->setFixedSize(22, 20);
    n->setCursor(Qt::PointingHandCursor);
    connect(y, &QToolButton::clicked, this, [this, k]() { QTimer::singleShot(0, this, [this, k]() { accept(k); }); });
    connect(n, &QToolButton::clicked, this, [this, k]() { QTimer::singleShot(0, this, [this, k]() { dismiss(k); }); });
    h->addWidget(y);
    h->addWidget(n);
    return w;
}

// ── esquerda: a prateleira ──
QWidget* Livre::ballEl(const QString& k, QWidget* parent)
{
    int docs = 0;
    for (const QString& c : children(k)) if (kindOf(c) == LK::Doc) docs++;
    auto* holder = new QWidget(parent);
    auto* v = new QVBoxLayout(holder);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    const bool ghost = isGhost(k);
    auto* b = new Ball(ball(k, 54), label(k), docs, m_sel == k, ghost, isCapital(k), m_justMade == k, holder);
    b->setProperty("lvKey", k);
    v->addWidget(b, 0, Qt::AlignHCenter);
    if (ghost) v->addWidget(ghostActs(k, holder), 0, Qt::AlignHCenter);
    else {
        b->clicked = [this, k]() { m_navOpen = true; select(k); };
        b->dragged = [this, k, b]() {
            hidePeek();
            auto* drag = new QDrag(b);
            auto* md = new QMimeData();
            md->setData(QString::fromLatin1(MIME), k.toUtf8());
            drag->setMimeData(md);
            drag->setPixmap(ball(k, 40));
            drag->setHotSpot(QPoint(20, 20));
            m_dragKey = k;
            drag->exec(Qt::MoveAction);
            m_dragKey.clear();
        };
        peekable(b, k);
    }
    return holder;
}
// O "+" da prateleira: vira campo de nome; Enter cria e o cursor já cai na folha.
QWidget* Livre::addBallEl(const QString& slot, const QString& parent, QWidget* where)
{
    auto* w = new QWidget(where);
    w->setFixedSize(78, 96);
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 6, 0, 0);
    v->setSpacing(4);
    if (m_shelfAdd == slot) {
        auto* in = new QLineEdit(w);
        in->setObjectName(QStringLiteral("lvInline"));
        in->setPlaceholderText(tr("Nome"));
        in->setFixedWidth(76);
        v->addWidget(in, 0, Qt::AlignHCenter);
        auto* hint = lab(tr("Enter cria"), QStringLiteral("lvSmall"), w);
        hint->setAlignment(Qt::AlignHCenter);
        v->addWidget(hint);
        v->addStretch();
        connect(in, &QLineEdit::returnPressed, this, [this, in, parent]() {
            const QString n = in->text().trimmed();
            if (n.isEmpty() || m_shelfAdd.isEmpty()) return;
            m_shelfAdd.clear();
            // fora do Enter: o render refaz a prateleira e apaga este campo, que ainda está tratando a tecla
            QTimer::singleShot(0, this, [this, n, parent]() {
                m_sel = createWorld(n, parent, Type::Temperado, false);
                m_navOpen = true;
                render();
                goBody();
            });
        });
        in->installEventFilter(this);
        in->setProperty("lvEscape", QStringLiteral("shelf"));
        QTimer::singleShot(0, in, [in]() { in->setFocus(); });
    } else {
        auto* plus = btn(QStringLiteral("+"), QStringLiteral("lvPlus"), w);
        plus->setFixedSize(54, 54);
        plus->setToolTip(tr("Novo planeta aqui"));
        connect(plus, &QPushButton::clicked, this, [this, slot]() { m_shelfAdd = slot; QTimer::singleShot(0, this, &Livre::renderLeft); });
        v->addWidget(plus, 0, Qt::AlignHCenter);
        auto* l = lab(tr("Novo"), QStringLiteral("lvSmall"), w);
        l->setAlignment(Qt::AlignHCenter);
        v->addWidget(l);
        v->addStretch();
    }
    return w;
}
QWidget* Livre::groupEl(const QString& title, const QString& id, QWidget* parent)
{
    const QString key = id.isEmpty() ? QStringLiteral("loose") : id;
    const bool closed = m_closed.contains(key);
    auto* g = new DropBox(this, id, [this, id](const QString& k) { return canDrop(k, id); }, [this, id](const QString& k) { QTimer::singleShot(0, this, [this, k, id]() { moveTo(k, id); render(); }); }, parent);
    g->setObjectName(QStringLiteral("lvGroup"));
    g->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    const Region* r = id.isEmpty() ? nullptr : m_doc->resolve(id).region;
    if (r) g->setStyleSheet(QStringLiteral("QFrame#lvGroup { background: %1; }").arg(rgba(QColor(r->color), 0.10)));
    auto* v = new QVBoxLayout(g);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(6);
    // cabeçalho
    auto* hd = new QPushButton(g);
    hd->setObjectName(QStringLiteral("lvGroupName"));
    hd->setCursor(Qt::PointingHandCursor);
    hd->setFocusPolicy(Qt::NoFocus);
    auto* hh = new QHBoxLayout(hd);
    hh->setContentsMargins(2, 0, 2, 0);
    hh->setSpacing(6);
    auto* ar = new QToolButton(hd);
    ar->setObjectName(QStringLiteral("lvArrow"));
    ar->setText(closed ? QStringLiteral("▸") : QStringLiteral("▾"));
    ar->setToolTip(closed ? tr("Abrir") : tr("Dobrar"));
    ar->setCursor(Qt::PointingHandCursor);
    connect(ar, &QToolButton::clicked, this, [this, key, closed]() { if (closed) m_closed.remove(key); else m_closed.insert(key); QTimer::singleShot(0, this, &Livre::renderLeft); });
    hh->addWidget(ar);
    if (r) {
        auto* sq = new QLabel(hd);
        if (!r->stamps.empty()) { sq->setFixedSize(18, 18); sq->setPixmap(ball(id, 16)); }
        else { sq->setFixedSize(12, 12); sq->setStyleSheet(QStringLiteral("background: %1; border-radius: 3px;").arg(r->color)); }
        sq->setAttribute(Qt::WA_TransparentForMouseEvents);
        hh->addWidget(sq);
    }
    auto* nm = lab(title, QStringLiteral("lvGroupLabel"), hd);
    nm->setProperty("on", !id.isEmpty() && m_sel == id);
    nm->setAttribute(Qt::WA_TransparentForMouseEvents);
    hh->addWidget(nm, 1);
    const QStringList worlds = id.isEmpty() ? [this]() {
        QStringList out;
        for (const QString& c : children(QString())) { const LK t = kindOf(c); if (t == LK::Planet && !isGhost(c)) { out << c; out << worldsIn(c); } else if (t == LK::Sys) out << worldsIn(c); }
        return out;
    }() : worldsIn(id);
    if (closed && !worlds.isEmpty()) {
        for (int i = 0; i < std::min<int>(7, worlds.size()); i++) { auto* d = new QLabel(hd); d->setFixedSize(13, 13); d->setPixmap(ball(worlds[i], 13)); d->setAttribute(Qt::WA_TransparentForMouseEvents); hh->addWidget(d); }
    } else if (!worlds.isEmpty()) { auto* c = lab(QString::number(worlds.size()), QStringLiteral("lvSmall"), hd); c->setAttribute(Qt::WA_TransparentForMouseEvents); hh->addWidget(c); }
    if (!id.isEmpty()) {
        hd->setProperty("lvKey", id);
        connect(hd, &QPushButton::clicked, this, [this, id]() { m_navOpen = true; select(id); });
        peekable(hd, id);
    } else connect(hd, &QPushButton::clicked, this, [this, key, closed]() { if (closed) m_closed.remove(key); else m_closed.insert(key); QTimer::singleShot(0, this, &Livre::renderLeft); });
    v->addWidget(hd);
    if (closed) return g;
    const QStringList kids = children(id);
    const auto here = [this](const QString& x) { return !m_sel.isEmpty() && isInside(m_sel, x); };
    // Prateleira C (escolha do Pe): o sistema vira um ladrilho do tamanho de um planeta, com o sol e os mundos dele
    // em órbita desenhados, lado a lado com os mundos isolados. Sistema grande não empilha o império; o sistema
    // inteiro, em pé, mora na direita. Sugestão do Gerar fica só lá, até o sim (aqui, só o selo "+N").
    auto* grid = new QGridLayout();
    grid->setSpacing(2);
    int n = 0;
    for (const QString& sk : kids) {
        if (kindOf(sk) != LK::Sys) continue;
        QStringList ws;
        int pending = 0;
        for (const QString& w : children(sk)) { if (!isWorld(w)) continue; if (isGhost(w)) pending++; else ws << w; }
        const System* sy = sysOf(sk);
        std::vector<QPixmap> pl;
        for (int i2 = 0; i2 < std::min<int>(7, ws.size()); i2++) pl.push_back(ball(ws[i2], i2 == 0 ? 11 : 9));
        QString sub = ws.isEmpty() ? tr("vazio") : ws.size() == 1 ? tr("1 mundo") : tr("%1 mundos").arg(ws.size());
        auto* holder = new DropBox(this, sk, [this, sk](const QString& k) { return canDrop(k, sk); }, [this, sk](const QString& k) { QTimer::singleShot(0, this, [this, k, sk]() { moveTo(k, sk); render(); }); }, g);
        holder->setObjectName(QStringLiteral("lvTileDrop"));
        auto* hv = new QVBoxLayout(holder);
        hv->setContentsMargins(0, 0, 0, 0);
        hv->setSpacing(0);
        auto* tile = new SysTile(starShown(sy ? sy->stars[0].kind : StarKind::G), sy && sy->stars[0].kind == StarKind::BuracoNegro, std::move(pl), label(sk), sub, pending, m_sel == sk, isGhost(sk), m_justMade == sk, holder);
        tile->setProperty("lvKey", sk);
        if (pending) tile->setToolTip(tr("%1 vizinhos sugeridos, esperando o sim na direita.").arg(pending));
        hv->addWidget(tile, 0, Qt::AlignHCenter);
        if (isGhost(sk)) hv->addWidget(ghostActs(sk, holder), 0, Qt::AlignHCenter);
        else {
            tile->clicked = [this, sk]() { m_navOpen = true; select(sk); };
            tile->dragged = [this, sk, tile]() {
                hidePeek();
                auto* drag = new QDrag(tile);
                auto* md = new QMimeData();
                md->setData(QString::fromLatin1(MIME), sk.toUtf8());
                drag->setMimeData(md);
                drag->setPixmap(tile->grab());
                drag->setHotSpot(QPoint(39, 30));
                drag->exec(Qt::MoveAction);
            };
            peekable(tile, sk);
        }
        grid->addWidget(holder, n / 3, n % 3);
        n++;
    }
    // os mundos isolados
    for (const QString& w : kids) if (kindOf(w) == LK::Planet) { grid->addWidget(ballEl(w, g), n / 3, n % 3); n++; }
    const QString cur = m_sel;
    if (id.isEmpty() || kids.isEmpty() || (here(id) && (parentKey(cur) == id || cur == id || kindOf(parentKey(cur)) != LK::Sys))) { grid->addWidget(addBallEl(key, id, g), n / 3, n % 3); n++; }
    if (n) v->addLayout(grid);
    return g;
}
void Livre::renderLeft()
{
    const int keep = m_left->verticalScrollBar()->value();
    auto* body = new QWidget();
    body->setObjectName(QStringLiteral("lvPaneBody"));
    auto* v = new QVBoxLayout(body);
    v->setContentsMargins(12, 12, 12, 12);
    v->setSpacing(10);
    const QStringList worlds = allWorlds();
    auto* top = new QHBoxLayout();
    top->setSpacing(6);
    top->addWidget(lab(QString::number(worlds.size()), QStringLiteral("lvCount"), body), 0, Qt::AlignBottom);
    top->addWidget(lab(worlds.size() == 1 ? tr("mundo") : tr("mundos"), QStringLiteral("lvCountSub"), body), 0, Qt::AlignBottom);
    top->addStretch();
    auto* nt = btn(tr("+ Território"), QStringLiteral("lvQuiet"), body);
    connect(nt, &QPushButton::clicked, this, [this]() { QTimer::singleShot(0, this, [this]() { m_sel = createTerritory(QString()); m_navOpen = false; render(); focusName(m_sel); }); });
    top->addWidget(nt, 0, Qt::AlignBottom);
    v->addLayout(top);
    if (!worlds.isEmpty()) {
        std::vector<QPixmap> pix;
        for (const QString& w : worlds) pix.push_back(ball(w, 14));
        v->addWidget(new Pot(std::move(pix), body));
    }
    for (const auto& r : m_doc->regions) v->addWidget(groupEl(r->name, r->key, body));
    v->addWidget(groupEl(tr("Soltos"), QString(), body));
    v->addStretch(1);
    m_left->setWidget(body);
    m_left->verticalScrollBar()->setValue(keep);
}

// ── meio: a folha ──
QWidget* Livre::whereTag(const DocLink* d, QWidget* parent)
{
    auto* l = new QLabel(parent);
    l->setObjectName(QStringLiteral("lvTag"));
    QString text = tr("Criador");
    if (d->gaveta && m_panel->m_link.model) for (const Drawer& dr : m_panel->m_link.model->drawers()) if (dr.key == d->drawer) {
        text = QStringLiteral("<span style=\"color:%1\">●</span> %2").arg(dr.color.isEmpty() ? Theme::accentDefault() : dr.color, dr.title.toHtmlEscaped());
        l->setTextFormat(Qt::RichText);
    }
    if (!d->gaveta) text = QStringLiteral("◍ ") + text;
    l->setText(text);
    l->setToolTip(tr("Salvo em: %1").arg(m_panel->docDestLabel(const_cast<DocLink*>(d))));
    return l;
}
void Livre::flushSheet()
{
    if (m_sheetTimer.isActive()) m_sheetTimer.stop();
    if (!m_body || m_sheetFor.isEmpty() || !m_body->property("dirty").toBool()) return;
    m_body->setProperty("dirty", false);
    const QString html = m_body->toHtml();
    const bool empty = m_body->toPlainText().trimmed().isEmpty() && !html.contains(QStringLiteral("<img"));
    if (valid(m_sheetFor)) writeSheet(m_sheetFor, empty ? QString() : html);
}
void Livre::goBody()
{
    QTimer::singleShot(0, this, [this]() { if (m_body) { m_body->setFocus(); m_body->moveCursor(QTextCursor::End); } });
}
void Livre::focusName(const QString& k)
{
    QTimer::singleShot(0, this, [this, k]() { if (m_name && m_sel == k) { m_name->setFocus(); m_name->selectAll(); } });
}
// ── prévia ──
void Livre::peekable(QWidget* w, const QString& k)
{
    w->setProperty("lvPeek", k);
    w->setAttribute(Qt::WA_Hover);
    w->installEventFilter(this);
}
void Livre::hidePeek()
{
    m_peekTimer.stop();
    if (m_peek) { m_peek->hide(); m_peek->deleteLater(); m_peek = nullptr; }
}
void Livre::showPeek(QWidget* at, const QString& k)
{
    hidePeek();
    if (!at || !valid(k) || m_pop) return;
    if (kindOf(k) == LK::Sys) { showSysPeek(at, k); return; }
    m_peek = new QFrame(this);
    m_peek->setObjectName(QStringLiteral("lvPeek"));
    m_peek->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_peek->setFixedWidth(280);
    auto* v = new QVBoxLayout(m_peek);
    v->setContentsMargins(12, 10, 12, 10);
    v->setSpacing(8);
    auto* head = new QHBoxLayout();
    head->setSpacing(10);
    const LK t = kindOf(k);
    head->addWidget(markOf(k, t == LK::Planet || t == LK::Moon || t == LK::Build ? 44 : 16, m_peek));
    auto* col2 = new QVBoxLayout();
    col2->setSpacing(1);
    auto* nm = lab(label(k), QStringLiteral("lvPeekName"), m_peek);
    nm->setWordWrap(true);
    col2->addWidget(nm);
    const QString par = parentKey(k);
    col2->addWidget(lab(kindName(t) + (par.isEmpty() ? QString() : QStringLiteral(" · ") + label(par)), QStringLiteral("lvSmall"), m_peek));
    head->addLayout(col2, 1);
    v->addLayout(head);
    QTextDocument td;
    td.setHtml(sheetHtml(k));
    QString txt = td.toPlainText().simplified();
    if (txt.size() > 300) txt = txt.left(300).trimmed() + QStringLiteral("…");
    auto* body = lab(txt.isEmpty() ? tr("Ainda sem texto.") : txt, QStringLiteral("lvPeekText"), m_peek, true);
    body->setProperty("none", txt.isEmpty());
    v->addWidget(body);
    int docs = 0, rest = 0;
    for (const QString& c : children(k)) { if (isGhost(c)) continue; if (kindOf(c) == LK::Doc) docs++; else rest++; }
    QStringList bits;
    if (t == LK::Doc) if (const DocLink* d = docOf(k)) bits << tr("Salvo em %1").arg(m_panel->docDestLabel(const_cast<DocLink*>(d)));
    if (docs) bits << (docs == 1 ? tr("1 documento") : tr("%1 documentos").arg(docs));
    if (rest) bits << (rest == 1 ? tr("1 coisa dentro") : tr("%1 coisas dentro").arg(rest));
    { const int mn = mentionsOf(k); if (mn) bits << (mn == 1 ? tr("mencionado em 1 folha") : tr("mencionado em %1 folhas").arg(mn)); }
    if (!bits.isEmpty()) v->addWidget(lab(bits.join(QStringLiteral(" · ")), QStringLiteral("lvPeekFoot"), m_peek, true));
    m_peek->adjustSize();
    const QRect r(at->mapTo(this, QPoint(0, 0)), at->size());
    const bool rightSide = m_right->isAncestorOf(at);
    int x = rightSide ? r.left() - m_peek->width() - 10 : r.right() + 10;
    x = std::clamp(x, 8, std::max(8, width() - m_peek->width() - 8));
    const int y = std::clamp(r.top() - 6, 8, std::max(8, height() - m_peek->height() - 8));
    m_peek->move(x, y);
    m_peek->show();
    m_peek->raise();
}

// ── balão de opções ──
void Livre::closePop()
{
    if (!m_pop) return;
    m_pop->hide();
    m_pop->deleteLater();
    m_pop = nullptr;
}
QFrame* Livre::openPop(QWidget* anchor, const std::function<void(QVBoxLayout*, QFrame*)>& build, int width)
{
    closePop();
    hidePeek();
    auto* pop = new QFrame(this);
    pop->setObjectName(QStringLiteral("lvPop"));
    auto* outer = new QVBoxLayout(pop);
    outer->setContentsMargins(4, 4, 4, 4);
    auto* scroll = new QScrollArea(pop);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }"));
    auto* body = new QWidget();
    auto* l = new QVBoxLayout(body);
    l->setContentsMargins(2, 2, 2, 2);
    l->setSpacing(1);
    build(l, pop);
    scroll->setWidget(body);
    outer->addWidget(scroll);
    const int w = width > 0 ? width : std::max(220, body->sizeHint().width() + 16);
    const int h = std::min(480, body->sizeHint().height() + 14);
    pop->setFixedSize(w, h);
    const QRect r(anchor->mapTo(this, QPoint(0, 0)), anchor->size());
    const int x = std::clamp(r.left(), 8, std::max(8, this->width() - w - 8));
    const int y = r.bottom() + h + 8 < height() ? r.bottom() + 5 : std::max(8, r.top() - h - 5);
    pop->move(x, y);
    pop->show();
    pop->raise();
    m_pop = pop;
    return pop;
}
void Livre::popTitle(QVBoxLayout* l, QWidget* parent, const QString& text) { l->addWidget(lab(text.toUpper(), QStringLiteral("lvPopTitle"), parent)); }
QPushButton* Livre::popOpt(QVBoxLayout* l, QWidget* parent, const QString& text, bool on, const QString& small, const std::function<void()>& fn, const QPixmap& mark)
{
    auto* b = btn(QString(), QStringLiteral("lvOpt"), parent);
    b->setProperty("on", on);
    auto* h = new QHBoxLayout(b);
    h->setContentsMargins(6, 3, 6, 3);
    h->setSpacing(8);
    if (!mark.isNull()) { auto* m = new QLabel(b); m->setPixmap(mark); m->setAttribute(Qt::WA_TransparentForMouseEvents); h->addWidget(m); }
    auto* t = lab(text, QStringLiteral("lvRowName"), b);
    t->setAttribute(Qt::WA_TransparentForMouseEvents);
    h->addWidget(t, 1);
    if (!small.isEmpty()) { auto* s = lab(small, QStringLiteral("lvSmall"), b); s->setAttribute(Qt::WA_TransparentForMouseEvents); h->addWidget(s); }
    b->setMinimumHeight(30);
    connect(b, &QPushButton::clicked, this, [this, fn]() { closePop(); QTimer::singleShot(0, this, fn); });
    l->addWidget(b);
    return b;
}
void Livre::pickList(QWidget* anchor, const QString& title, const QStringList& opts, const QString& cur, const QString& none, const std::function<void(const QString&)>& fn)
{
    openPop(anchor, [&](QVBoxLayout* l, QFrame* p) {
        popTitle(l, p, title);
        for (const QString& o : opts) popOpt(l, p, o, o == cur, QString(), [fn, o]() { fn(o); });
        if (!cur.isEmpty() && !none.isEmpty()) {
            auto* sep = new QFrame(p); sep->setObjectName(QStringLiteral("lvPopSep")); l->addWidget(sep);
            popOpt(l, p, none, false, QString(), [fn]() { fn(QString()); });
        }
    });
}

// ── teclado e cliques fora ──
bool Livre::eventFilter(QObject* o, QEvent* e)
{
    if (!isVisible()) return false;
    const QEvent::Type t = e->type();
    if (t == QEvent::MouseButtonPress) {
        QWidget* w = qobject_cast<QWidget*>(o);
        if (w && w->window() == window()) {
            if (m_pop && !m_pop->isAncestorOf(w) && w != m_pop) closePop();
            if (m_bigView && w == m_bigView) { QTimer::singleShot(0, this, &Livre::closeBigView); return false; }
            if (m_mentionPop && w != m_mentionPop && !m_mentionPop->isAncestorOf(w) && w != m_body && !(m_body && m_body->isAncestorOf(w))) closeMention();
            // a gaveta solta fecha ao clicar fora (fixada, não)
            if (m_nav->isVisible() && !m_navPinned && w != m_nav && !m_nav->isAncestorOf(w) && !m_left->isAncestorOf(w) && w != m_toast && !m_toast->isAncestorOf(w)) {
                if (!m_pop) { m_navOpen = false; m_nav->hide(); }
            }
        }
        return false;
    }
    if (t == QEvent::KeyPress) {
        auto* k = static_cast<QKeyEvent*>(e);
        if (k->key() == Qt::Key_Escape) {
            const QString what = o->property("lvEscape").toString();
            if (what == QLatin1String("shelf")) { m_shelfAdd.clear(); QTimer::singleShot(0, this, &Livre::renderLeft); return true; }
            if (what == QLatin1String("sys")) { m_sysAdd = false; QTimer::singleShot(0, this, &Livre::renderRight); return true; }
            if (what == QLatin1String("nav")) { m_navAdd.clear(); m_rowAdd.clear(); m_ask = {}; QTimer::singleShot(0, this, &Livre::renderNav); return true; }
            if (what == QLatin1String("orb")) { o->deleteLater(); return true; }
            if (what == QLatin1String("navq")) { m_navSearch = false; m_navQuery.clear(); QTimer::singleShot(0, this, &Livre::renderNav); return true; }
            if (o == m_body || o == m_name || isAncestorOf(qobject_cast<QWidget*>(o))) {
                if (m_pop) { closePop(); return true; }
            }
        }
        return false;
    }
    if (t == QEvent::MouseButtonRelease) {
        const QString dk = o->property("lvDraw").toString();
        if (!dk.isEmpty()) { QTimer::singleShot(0, this, [this, dk]() { openDraw(dk); }); return true; }
    }
    const QString pk = o->property("lvPeek").toString();
    if (!pk.isEmpty()) {
        if (t == QEvent::Enter || t == QEvent::HoverEnter) { m_peekAt = qobject_cast<QWidget*>(o); m_peekKey = pk; m_peekTimer.start(m_peek ? 60 : 160); }
        else if (t == QEvent::Leave || t == QEvent::HoverLeave || t == QEvent::MouseButtonPress) { if (m_peekAt == o) { m_peekTimer.stop(); QTimer::singleShot(100, this, [this]() { if (!m_peekAt || !m_peekAt->underMouse()) hidePeek(); }); } }
    }
    return false;
}

} // namespace galaxy
