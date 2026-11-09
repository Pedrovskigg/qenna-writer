// Criação Livre: a aba de planeta novo. Abre colada na prateleira, do lado dela, quando se aperta o "+" de um
// grupo ou o "Novo planeta" de um sistema: nome, tipo do planeta e construções, tudo vendo. Criar faz o planeta e
// as construções de uma vez, e um Desfazer só desfaz tudo. Nome em branco usa o nome sugerido.

#include "GalaxyLivre.h"
#include "GalaxyDoc.h"
#include "GalaxyCanvas.h"
#include "GalaxyStructures.h"

#include "../Theme.h"
#include "../carto/CartoIcons.h"

#include <QCoreApplication>
#include <QEasingCurve>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <memory>

namespace galaxy {

namespace {
inline QString coreT(const char* s) { return QCoreApplication::translate("galaxy::Core", s); }
inline QString catT(const char* s) { return QCoreApplication::translate("galaxy::Catalog", s); }
inline QColor col(const QString& c) { return Theme::toColor(c); }
const std::vector<Type> PLANET_TYPES = { Type::Temperado, Type::Oceano, Type::Deserto, Type::Arido, Type::Gelado, Type::Lava, Type::Toxico, Type::Anao, Type::Gigante, Type::Quente, Type::GeloG, Type::Subnetuno, Type::Fortaleza, Type::Ecumeno, Type::Casco, Type::Forja };
constexpr int TAB_W = 368;
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
void repolish(QWidget* w) { w->style()->unpolish(w); w->style()->polish(w); }
// os modelos que podem nascer junto com um planeta: as estações e as megaestruturas que abraçam o planeta
std::vector<std::string> planetBuilds()
{
    std::vector<std::string> out;
    for (const CatalogCat& c : catalog()) {
        if (c.id != "estacoes" && c.id != "mega") continue;
        for (const CatalogItem& it : c.items) {
            const StructDef* d = structOf(it.id);
            if (!d || (c.id == "mega" && d->host != Host::Planet)) continue;
            out.push_back(it.id);
        }
    }
    return out;
}
QString catalogName(const std::string& id)
{
    for (const CatalogCat& c : catalog()) for (const CatalogItem& it : c.items) if (it.id == id) return catT(it.name);
    return QString::fromStdString(id);
}
} // namespace

QString Livre::newWhereText() const
{
    const QString p = m_nw.parent;
    if (p.isEmpty() || !valid(p)) return tr("Solto, fora de qualquer território");
    if (kindOf(p) == LK::Sys) return tr("No sistema %1").arg(label(p));
    return tr("Em %1").arg(label(p));
}

void Livre::openNewWorld(const QString& parent)
{
    closePop();
    hidePeek();
    // já aberta: só troca onde o planeta vai nascer, sem perder o que foi escolhido
    if (m_newTab) {
        m_nw.parent = parent;
        syncNewWorld();
        renderLeft();
        if (m_nwName) m_nwName->setFocus();
        return;
    }
    m_nw = {};
    m_nw.parent = parent;
    m_nw.hint = ghostName(QStringLiteral("planet"));
    m_nav->hide();

    auto* tab = new QFrame(this);
    tab->setObjectName(QStringLiteral("lvNewTab"));
    tab->setAttribute(Qt::WA_StyledBackground, true);
    m_newTab = tab;
    auto* root = new QVBoxLayout(tab);
    root->setContentsMargins(1, 1, 1, 1);
    root->setSpacing(0);

    // listra de cima, na cor de destaque: a aba
    auto* stripe = new QWidget(tab);
    stripe->setObjectName(QStringLiteral("lvNewStripe"));
    stripe->setAttribute(Qt::WA_StyledBackground, true);
    stripe->setFixedHeight(4);
    root->addWidget(stripe);

    // cabeçalho: o planeta como vai nascer, onde e o que é
    auto* head = new QWidget(tab);
    head->setObjectName(QStringLiteral("lvNewHead"));
    head->setAttribute(Qt::WA_StyledBackground, true);
    auto* hh = new QHBoxLayout(head);
    hh->setContentsMargins(18, 14, 10, 14);
    hh->setSpacing(14);
    m_nwBall = new QLabel(head);
    m_nwBall->setFixedSize(72, 72);
    m_nwBall->setAlignment(Qt::AlignCenter);
    hh->addWidget(m_nwBall, 0, Qt::AlignTop);
    auto* hv = new QVBoxLayout();
    hv->setSpacing(3);
    hv->setContentsMargins(0, 2, 0, 0);
    hv->addWidget(lab(tr("NOVO PLANETA"), QStringLiteral("lvSideTitle"), head));
    m_nwTitle = lab(QString(), QStringLiteral("lvNewTitle"), head);
    hv->addWidget(m_nwTitle);
    m_nwWhere = lab(QString(), QStringLiteral("lvNewWhere"), head, true);
    hv->addWidget(m_nwWhere);
    m_nwSummary = lab(QString(), QStringLiteral("lvNewType"), head, true);
    hv->addWidget(m_nwSummary);
    hv->addStretch();
    hh->addLayout(hv, 1);
    auto* cl = new QToolButton(head);
    cl->setObjectName(QStringLiteral("lvNavMini"));
    cl->setIcon(carto::icon(QStringLiteral("close"), col(Theme::textMuted()), col(Theme::accentDefault()), 14));
    cl->setIconSize(QSize(14, 14));
    cl->setToolTip(tr("Fechar (Esc)"));
    cl->setCursor(Qt::PointingHandCursor);
    cl->setFocusPolicy(Qt::NoFocus);
    connect(cl, &QToolButton::clicked, this, [this]() { QTimer::singleShot(0, this, &Livre::closeNewWorld); });
    hh->addWidget(cl, 0, Qt::AlignTop);
    root->addWidget(head);

    auto* scroll = new QScrollArea(tab);
    scroll->setObjectName(QStringLiteral("lvPaneIn"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget();
    body->setObjectName(QStringLiteral("lvPaneBody"));
    auto* v = new QVBoxLayout(body);
    v->setContentsMargins(18, 16, 14, 18);
    v->setSpacing(8);

    // ── Nome ──
    v->addWidget(lab(tr("NOME"), QStringLiteral("lvSideTitle"), body));
    auto* nr = new QHBoxLayout();
    nr->setSpacing(6);
    m_nwName = new QLineEdit(body);
    m_nwName->setObjectName(QStringLiteral("lvNewName"));
    m_nwName->setPlaceholderText(m_nw.hint);
    m_nwName->setMaxLength(80);
    m_nwName->setProperty("lvEscape", QStringLiteral("new"));
    m_nwName->installEventFilter(this);
    nr->addWidget(m_nwName, 1);
    auto* dice = new QToolButton(body);
    dice->setObjectName(QStringLiteral("lvNewDice"));
    dice->setFixedSize(38, 38);
    dice->setIcon(carto::icon(QStringLiteral("dice"), col(Theme::textMuted()), col(Theme::accentDefault()), 16));
    dice->setIconSize(QSize(16, 16));
    dice->setToolTip(tr("Sortear um nome"));
    dice->setCursor(Qt::PointingHandCursor);
    dice->setFocusPolicy(Qt::NoFocus);
    nr->addWidget(dice, 0, Qt::AlignVCenter);
    v->addLayout(nr);
    auto* nameNote = lab(tr("Em branco, ele nasce com o nome sugerido."), QStringLiteral("lvSmall"), body, true);
    v->addWidget(nameNote);
    connect(m_nwName, &QLineEdit::textChanged, this, [this](const QString& t) { m_nw.name = t; syncNewWorld(); });
    // fora do Enter: criar refaz a prateleira e fecha a aba, que ainda está tratando a tecla
    connect(m_nwName, &QLineEdit::returnPressed, this, [this]() { QTimer::singleShot(0, this, &Livre::createNewWorld); });
    connect(dice, &QToolButton::clicked, this, [this]() { if (m_nwName) { m_nwName->setText(ghostName(QStringLiteral("planet"))); m_nwName->setFocus(); m_nwName->selectAll(); } });
    v->addSpacing(10);

    // ── Tipo do planeta ──
    v->addWidget(lab(tr("TIPO DO PLANETA"), QStringLiteral("lvSideTitle"), body));
    auto* tg = new QGridLayout();
    tg->setSpacing(2);
    int n = 0;
    for (Type t : PLANET_TYPES) {
        auto* o = btn(QString(), QStringLiteral("lvTy"), body);
        o->setProperty("on", t == m_nw.type);
        o->setProperty("nwType", int(t));
        o->setFixedSize(76, 76);
        auto* ov = new QVBoxLayout(o);
        ov->setContentsMargins(2, 5, 2, 2);
        ov->setSpacing(2);
        auto* ic = new QLabel(o);
        ic->setPixmap(typeThumb(t, 40));
        ic->setAlignment(Qt::AlignHCenter);
        ic->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* tl = lab(coreT(typeLabel(t)), QStringLiteral("lvSmall"), o, true);
        tl->setAlignment(Qt::AlignHCenter);
        tl->setAttribute(Qt::WA_TransparentForMouseEvents);
        ov->addWidget(ic);
        ov->addWidget(tl);
        connect(o, &QPushButton::clicked, this, [this, t]() { m_nw.type = t; syncNewWorld(); });
        tg->addWidget(o, n / 4, n % 4);
        n++;
    }
    v->addLayout(tg);
    v->addSpacing(10);

    // ── Construções: quantas quiser, ou nenhuma ──
    auto* bh = new QHBoxLayout();
    bh->addWidget(lab(tr("CONSTRUÇÕES"), QStringLiteral("lvSideTitle"), body));
    bh->addStretch();
    m_nwBuildCount = lab(QString(), QStringLiteral("lvSmall"), body);
    bh->addWidget(m_nwBuildCount);
    v->addLayout(bh);
    auto* bg = new QGridLayout();
    bg->setSpacing(2);
    n = 0;
    for (const std::string& id : planetBuilds()) {
        auto* o = btn(QString(), QStringLiteral("lvTy"), body);
        o->setProperty("on", false);
        o->setProperty("nwBuild", QString::fromStdString(id));
        o->setFixedSize(76, 82);
        if (const StructDef* d = structOf(id)) o->setToolTip(catT(d->desc));
        auto* ov = new QVBoxLayout(o);
        ov->setContentsMargins(2, 5, 2, 2);
        ov->setSpacing(2);
        auto* ic = new QLabel(o);
        ic->setPixmap(modelPix(id, 91, 44));
        ic->setAlignment(Qt::AlignHCenter);
        ic->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* tl = lab(catalogName(id), QStringLiteral("lvSmall"), o, true);
        tl->setAlignment(Qt::AlignHCenter);
        tl->setAttribute(Qt::WA_TransparentForMouseEvents);
        ov->addWidget(ic);
        ov->addWidget(tl);
        connect(o, &QPushButton::clicked, this, [this, id]() {
            auto it = std::find(m_nw.builds.begin(), m_nw.builds.end(), id);
            if (it == m_nw.builds.end()) m_nw.builds.push_back(id);
            else m_nw.builds.erase(it);
            syncNewWorld();
        });
        bg->addWidget(o, n / 4, n % 4);
        n++;
    }
    v->addLayout(bg);
    v->addWidget(lab(tr("Opcional. Dá pra pôr ou tirar depois, na janela do planeta."), QStringLiteral("lvSmall"), body, true));
    v->addStretch();
    scroll->setWidget(body);
    root->addWidget(scroll, 1);

    // pé: Cancelar e Criar
    auto* foot = new QWidget(tab);
    foot->setObjectName(QStringLiteral("lvNewFoot"));
    foot->setAttribute(Qt::WA_StyledBackground, true);
    auto* fh = new QHBoxLayout(foot);
    fh->setContentsMargins(14, 12, 14, 12);
    fh->setSpacing(8);
    fh->addStretch();
    auto* cancel = btn(tr("Cancelar"), QStringLiteral("lvQuiet"), foot);
    connect(cancel, &QPushButton::clicked, this, [this]() { QTimer::singleShot(0, this, &Livre::closeNewWorld); });
    fh->addWidget(cancel);
    auto* go = btn(tr("Criar planeta"), QStringLiteral("lvNewGo"), foot);
    go->setToolTip(tr("Criar e abrir a folha dele (Enter)"));
    connect(go, &QPushButton::clicked, this, [this]() { QTimer::singleShot(0, this, &Livre::createNewWorld); });
    fh->addWidget(go);
    root->addWidget(foot);

    syncNewWorld();
    placeNewWorld();
    tab->show();
    tab->raise();
    // desliza pra fora da prateleira
    const QPoint end = tab->pos();
    auto* anim = new QPropertyAnimation(tab, "pos", tab);
    anim->setDuration(170);
    anim->setStartValue(end - QPoint(22, 0));
    anim->setEndValue(end);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
    renderLeft();
    QTimer::singleShot(0, m_nwName, [this]() { if (m_nwName) m_nwName->setFocus(); });
}

void Livre::syncNewWorld()
{
    if (!m_newTab) return;
    if (m_nwBall) m_nwBall->setPixmap(typeThumb(m_nw.type, 72));
    if (m_nwTitle) m_nwTitle->setText(m_nw.name.trimmed().isEmpty() ? m_nw.hint : m_nw.name.trimmed());
    if (m_nwWhere) m_nwWhere->setText(newWhereText());
    const int nb = int(m_nw.builds.size());
    QString sum = coreT(typeLabel(m_nw.type));
    if (nb == 1) sum += QStringLiteral(" · ") + tr("1 construção");
    else if (nb > 1) sum += QStringLiteral(" · ") + tr("%1 construções").arg(nb);
    if (m_nwSummary) m_nwSummary->setText(sum);
    if (m_nwBuildCount) m_nwBuildCount->setText(nb == 0 ? tr("nenhuma") : nb == 1 ? tr("1 escolhida") : tr("%1 escolhidas").arg(nb));
    for (QPushButton* b : m_newTab->findChildren<QPushButton*>(QStringLiteral("lvTy"))) {
        bool on = false;
        if (b->property("nwType").isValid()) on = b->property("nwType").toInt() == int(m_nw.type);
        else if (b->property("nwBuild").isValid()) on = std::find(m_nw.builds.begin(), m_nw.builds.end(), b->property("nwBuild").toString().toStdString()) != m_nw.builds.end();
        if (b->property("on").toBool() != on) { b->setProperty("on", on); repolish(b); }
    }
}

void Livre::placeNewWorld()
{
    if (!m_newTab) return;
    const QRect lb(m_leftCol->mapTo(this, QPoint(0, 0)), m_leftCol->size());
    m_newTab->setFixedSize(TAB_W, lb.height());
    m_newTab->move(lb.right() + 8, lb.top());
}

void Livre::closeNewWorld()
{
    if (!m_newTab) return;
    m_newTab->hide();
    m_newTab->deleteLater();
    m_newTab = nullptr;
    renderLeft();
    renderNav();
}

void Livre::createNewWorld()
{
    if (!m_newTab) return;
    const QString name = m_nw.name.trimmed().isEmpty() ? m_nw.hint : m_nw.name.trimmed();
    const QString parent = m_nw.parent.isEmpty() || valid(m_nw.parent) ? m_nw.parent : QString();
    const Type type = m_nw.type;
    const std::vector<std::string> builds = m_nw.builds;
    m_newTab->hide();
    m_newTab->deleteLater();
    m_newTab = nullptr;
    flushSheet();
    const QString key = createWorld(name, parent, type, false);
    int made = 1;
    for (const std::string& id : builds) if (!createBuild(id, key, false).isEmpty()) made++;
    // um Desfazer só pro planeta e tudo o que nasceu com ele
    auto& st = m_canvas->undoStack;
    if (made > 1 && int(st.size()) >= made) {
        auto parts = std::make_shared<std::vector<Canvas::UndoEntry>>(std::make_move_iterator(st.end() - made), std::make_move_iterator(st.end()));
        st.erase(st.end() - made, st.end());
        const int nb = made - 1;
        const QString msg = nb == 1 ? tr("%1 nasceu com 1 construção.").arg(name) : tr("%1 nasceu com %2 construções.").arg(name).arg(nb);
        m_canvas->pushUndo(msg,
            [parts] { for (auto it = parts->rbegin(); it != parts->rend(); ++it) if (it->undo) it->undo(); },
            [parts] { for (auto& p : *parts) if (p.redo) p.redo(); });
    }
    m_sel = key;
    m_navOpen = true;
    render();
    goBody();
}

} // namespace galaxy
