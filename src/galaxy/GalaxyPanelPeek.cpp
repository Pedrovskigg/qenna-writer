// Prévia rápida: mouse em cima de um sistema e um card no canto mostra o que tem lá, sem zoom e sem clique.

#include "GalaxyPanel.h"
#include "GalaxyBlackHole.h"
#include "GalaxyCaderno.h"
#include "GalaxyHab.h"

#include "../ProjectModel.h"
#include "../TerritorioStore.h"
#include "../Theme.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QRadialGradient>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

namespace galaxy {

namespace {
constexpr int kPeekWidth = 320, kPeekMaxPlanets = 8;
QLabel* textOf(const QString& text, const QString& obj, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(obj);
    l->setAttribute(Qt::WA_TransparentForMouseEvents);
    return l;
}
// O retrato do sistema: a estrela borrada ou o buraco negro.
QPixmap starPicture(const System& s, int px, double dpr)
{
    QPixmap pm(QSize(px, px) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const double R = px / 2.0;
    if (s.isCore || s.stars[0].kind == StarKind::BuracoNegro) {
        double a, b;
        blackHoleAxis(s, a, b);
        paintBlackHole(p, QPointF(R, R), R / BLACK_HOLE_EXTENT, a, b, 0, 0, 40);
        return pm;
    }
    const auto c = sysColor(s);
    QRadialGradient g(R, R, R);
    g.setColorAt(0, Qt::white);
    g.setColorAt(0.18, Qt::white);
    g.setColorAt(0.42, QColor(c[0], c[1], c[2]));
    g.setColorAt(0.72, QColor(c[0], c[1], c[2], 0));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(QPointF(R, R), R, R);
    return pm;
}
}

void Panel::buildPeek()
{
    m_peek = new QFrame(this);
    m_peek->setObjectName(QStringLiteral("gmPeek"));
    m_peek->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_peek->setFixedWidth(kPeekWidth);
    auto* l = new QVBoxLayout(m_peek);
    l->setContentsMargins(14, 12, 14, 12);
    l->setSpacing(8);
    m_peek->hide();
    m_peekTimer = new QTimer(this);
    m_peekTimer->setSingleShot(true);
    connect(m_peekTimer, &QTimer::timeout, this, [this]() {
        if (m_peekWant.isEmpty()) hidePeek();
        else showPeek(m_peekWant);
    });
    m_peekOn = QSettings().value(QStringLiteral("galaxy/peek"), true).toBool();
}
void Panel::hidePeek()
{
    m_peekWant.clear();
    m_peekKey.clear();
    if (m_peekTimer) m_peekTimer->stop();
    if (m_peek) m_peek->hide();
}
void Panel::setPeekOn(bool on)
{
    m_peekOn = on;
    QSettings().setValue(QStringLiteral("galaxy/peek"), on);
    if (!on) hidePeek();
    syncUi();
}
void Panel::peekChanged(const QString& key)
{
    if (!m_peek) return;
    QString target;
    if (m_peekOn && isOpen() && m_duoLink.isEmpty() && !(m_caderno && m_caderno->isVisible())) {
        const Ref o = m_doc.resolve(key);
        if (o.sys && !o.sys->gone) {
            target = keyS(*o.sys);
            // De perto o sistema já está na tela; aberto no painel, já está na leitura.
            if (o.sys->layR * m_canvas->cam.z > 170 || (panelShown() && !m_panelMini && m_canvas->sel == target)) target.clear();
        }
    }
    m_peekWant = target;
    if (target == m_peekKey && !target.isEmpty()) { m_peekTimer->stop(); return; }
    // Passar por cima no caminho não pisca: aparece depois de um respiro e some sem pressa.
    m_peekTimer->start(target.isEmpty() ? 180 : m_peek->isVisible() ? 60 : 150);
}
void Panel::showPeek(const QString& k)
{
    const Ref o = m_doc.resolve(k);
    if (o.kind != RefKind::System || m_settings->isVisible() || m_export->isVisible() || m_gear->isVisible()) { hidePeek(); return; }
    const System& s = *o.sys;
    // o miolo nasce de novo a cada sistema: nada do card anterior fica pra trás
    if (m_peekBody) { m_peekBody->hide(); m_peekBody->deleteLater(); }
    auto* card = new QWidget(m_peek);
    m_peekBody = card;
    static_cast<QVBoxLayout*>(m_peek->layout())->addWidget(card);
    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);
    const double dpr = devicePixelRatioF();
    const int room = kPeekWidth - 28;

    auto* head = new QHBoxLayout();
    head->setSpacing(10);
    auto* pic = new QLabel(card);
    pic->setFixedSize(44, 44);
    pic->setPixmap(starPicture(s, 44, dpr));
    head->addWidget(pic);
    auto* names = new QVBoxLayout();
    names->setSpacing(0);
    auto* nm = textOf(QString(), QStringLiteral("gmPeekName"), card);
    nm->setText(nm->fontMetrics().elidedText(m_doc.labelOf(k), Qt::ElideRight, room - 54));
    names->addWidget(nm);
    auto* sub = textOf(QString(), QStringLiteral("cartoNote"), card);
    sub->setText(sub->fontMetrics().elidedText(subShort(k, o), Qt::ElideRight, room - 54));
    names->addWidget(sub);
    head->addLayout(names, 1);
    lay->addLayout(head);

    QWidget* facts = factsGrid(quickFacts(k, o));
    facts->setParent(card);
    lay->addWidget(facts);
    {
        auto* bar = new HabBar(card);
        bar->setCounts(m_canvas->sysHab(s).counts);
        lay->addWidget(bar);
    }

    const std::vector<Body*> planets = planetsOf(s);
    if (m_peekBallsRev != m_doc.rev) { m_peekBalls.clear(); m_peekBallsRev = m_doc.rev; }
    for (size_t i = 0; i < planets.size() && i < kPeekMaxPlanets; i++) {
        const Body& p = *planets[i];
        const QString pk = keyP(s, p);
        auto* row = new QWidget(m_peek);
        row->setFixedHeight(26);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(8);
        auto* ball = new QLabel(row);
        ball->setFixedSize(24, 24);
        auto cached = m_peekBalls.constFind(pk);
        if (cached == m_peekBalls.constEnd()) cached = m_peekBalls.insert(pk, QPixmap::fromImage(m_canvas->smallBall(p, 24, V3 { -0.62, 0.3, 0.72 }, dpr)));
        ball->setPixmap(*cached);
        h->addWidget(ball);
        auto* name = textOf(QString(), QStringLiteral("cartoValue"), row);
        name->setText(name->fontMetrics().elidedText(m_doc.labelOf(pk), Qt::ElideRight, 112));
        h->addWidget(name);
        auto* kind = textOf(QString(), QStringLiteral("cartoNote"), row);
        kind->setText(kind->fontMetrics().elidedText(QCoreApplication::translate("galaxy::Core", typeLabel(p.type)), Qt::ElideRight, 66));
        h->addWidget(kind, 1);
        // Quem tem gente mostra a gente; o resto, a temperatura, em verde na zona habitável.
        auto* val = textOf(p.people != 0 ? tr("%1 hab.").arg(fmtCount(p.people)) : celsius(p.temp), QStringLiteral("cartoNote"), row);
        if (p.people != 0 || p.hz == Hz::Dentro) val->setStyleSheet(QStringLiteral("color: #6fd69a;"));
        h->addWidget(val);
        { const int cn = habInfo(p, &s, nullptr).cls; if (cn) h->addWidget(habPill(cn, row)); }
        lay->addWidget(row);
    }
    if (planets.size() > kPeekMaxPlanets) lay->addWidget(textOf(tr("E mais %1").arg(planets.size() - kPeekMaxPlanets), QStringLiteral("cartoNote"), card));

    // o que o autor já pôs aqui
    int docs = int(m_doc.docsOf(k).size());
    for (const auto& x : docsBelow(k)) docs += int(x.second.size());
    const int people = peopleCount(k), links = int(linksOf(k).size());
    QStringList mine;
    if (docs) mine << tr("%n documento(s)", "", docs);
    if (people) mine << tr("%n personagem(ns)", "", people);
    if (links) mine << tr("%n conexão(ões)", "", links);
    if (!mine.isEmpty()) lay->addWidget(textOf(mine.join(QStringLiteral(" · ")), QStringLiteral("gmPeekMine"), card));

    m_peekKey = k;
    m_peek->show();
    m_peek->raise();
    lay->activate();
    m_peek->layout()->invalidate();
    m_peek->layout()->activate();
    m_peek->adjustSize();
    placePeek();
}
void Panel::placePeek()
{
    if (!m_peek || !m_peek->isVisible()) return;
    const int right = m_habCard && m_habCard->isVisible() ? m_habCard->x() : m_cbar->x();
    m_peek->move(right - 8 - m_peek->width(), 14);
}
// Quantas fichas nasceram ou estão aqui, contando os lugares de dentro.
int Panel::peopleCount(const QString& k)
{
    if (!m_link.model || !m_link.places) return 0;
    QSet<QString> places;
    const QString own = m_doc.placeIds.value(k);
    if (!own.isEmpty()) places.insert(own);
    for (auto it = m_doc.placeIds.constBegin(); it != m_doc.placeIds.constEnd(); ++it)
        if (it.key() != k && m_doc.pathOf(it.key()).contains(k)) places.insert(it.value());
    if (places.isEmpty()) return 0;
    int n = 0;
    for (const Drawer& dr : m_link.model->drawers())
        for (const DrawerItem& it : dr.items)
            if (places.contains(it.origemTerritorioId) || places.contains(it.localAtualTerritorioId)) n++;
    return n;
}

} // namespace galaxy
