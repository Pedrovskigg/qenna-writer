// A Cartela (legenda no mapa e interface alternativa), a exportação do PNG pro
// livro e as preferências de interface (lembradas pro app inteiro, não por mapa).

#include "CartoPanel.h"

#include "CartoAstrolabe.h"
#include "CartoCartela.h"
#include "CartoIcons.h"
#include "../Theme.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace carto {

namespace {
// itens da legenda: o que cada um escolhe
struct CItem { const char* icon; const char* label; const char* build; int brush; const char* tool; };
const CItem kCItems[12] = {
    { "castle", QT_TRANSLATE_NOOP("carto::Panel", "Castelo"), "castle", -1, nullptr },
    { "tower", QT_TRANSLATE_NOOP("carto::Panel", "Torre"), "tower", -1, nullptr },
    { "house", QT_TRANSLATE_NOOP("carto::Panel", "Vila"), "village", -1, nullptr },
    { "field", QT_TRANSLATE_NOOP("carto::Panel", "Plantação"), "fields", -1, nullptr },
    { "wall", QT_TRANSLATE_NOOP("carto::Panel", "Muralha"), "wall", -1, nullptr },
    { "roadline", QT_TRANSLATE_NOOP("carto::Panel", "Estrada"), "road", -1, nullptr },
    { "bridge", QT_TRANSLATE_NOOP("carto::Panel", "Ponte"), "bridge", -1, nullptr },
    { "flag", QT_TRANSLATE_NOOP("carto::Panel", "Território"), nullptr, -1, "realm" },
    { "mount", QT_TRANSLATE_NOOP("carto::Panel", "Serra"), nullptr, int(Brush::Mountain), nullptr },
    { "river", QT_TRANSLATE_NOOP("carto::Panel", "Rio"), nullptr, int(Brush::River), nullptr },
    { "forest", QT_TRANSLATE_NOOP("carto::Panel", "Floresta"), nullptr, int(Brush::Wet), nullptr },
    { "names", QT_TRANSLATE_NOOP("carto::Panel", "Nome"), nullptr, -1, "names" },
};
}

void Panel::buildCartelaUi()
{
    QSettings st;
    m_ui = st.value(QStringLiteral("carto/ui"), QStringLiteral("astro")).toString();
    m_cartelaOn = false;   // 22/10: a Cartela saiu da tela (só vai no PNG); na interface Cartela fica a bússola no canto dela
    m_canvas->opts().sprites = st.value(QStringLiteral("carto/sprites"), true).toBool();
    m_canvas->opts().frame = st.value(QStringLiteral("carto/frame"), false).toBool();

    m_cartela = new Cartela(this);
    std::vector<CartelaItem> items;
    for (const CItem& c : kCItems) items.push_back({ QString::fromLatin1(c.icon), tr(c.label) });
    m_cartela->setItems(items);
    m_cartela->setMinimized(st.value(QStringLiteral("carto/cartelaMin"), false).toBool());
    connect(m_cartela, &Cartela::itemPicked, this, &Panel::cartelaPicked);
    connect(m_cartela, &Cartela::minimizeToggled, this, [this](bool on) {
        QSettings().setValue(QStringLiteral("carto/cartelaMin"), on);
        positionOverlays();
    });
    m_rose = new Rose(this);
    connect(m_rose, &Rose::angleDragged, this, [this](double a, bool quick) { m_canvas->setView(a, m_canvas->pitch(), quick); });
    connect(m_rose, &Rose::northRequested, this, [this]() { m_canvas->setView(0, m_canvas->pitch(), false); });

    // barra das ferramentas que não são item de legenda (só no modo Cartela)
    m_cbar = new QFrame(this);
    m_cbar->setProperty("cartoFloat", true);
    m_cbar->setAttribute(Qt::WA_StyledBackground, true);
    auto* cb = new QVBoxLayout(m_cbar);
    cb->setContentsMargins(4, 4, 4, 4);
    cb->setSpacing(2);
    const QList<QPair<QString, QString>> tools = { { "pan", "hand" }, { "area", "area" }, { "brush", "brush" }, { "climate", "clima" } };
    const QStringList tips = { tr("Mover (H)"), tr("Área: arraste um retângulo pra criar terra (A)"), tr("Pincéis de terreno (B)"), tr("Clima (C)") };
    for (int i = 0; i < tools.size(); i++) {
        auto* b = new QToolButton(m_cbar);
        b->setObjectName(QStringLiteral("cartoBtn"));
        b->setCheckable(true);
        b->setFixedSize(30, 30);
        b->setIconSize(QSize(18, 18));
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tips[i]);
        m_icons.append({ b, tools[i].second });
        const QString id = tools[i].first;
        connect(b, &QToolButton::clicked, this, [this, id]() { m_dockFromRing = false; m_dockFromCartela = false; pickTool(id, QPoint()); m_dockFromRing = false; placeDock(); });
        m_cbarBtns.insert(id, b);
        cb->addWidget(b);
    }
    // embaixo das ferramentas, as ações que moravam na pílula de cima (essas valem nos dois modos)
    m_cbarSep = new QFrame(m_cbar);
    m_cbarSep->setObjectName(QStringLiteral("cartoSep"));
    m_cbarSep->setFixedSize(18, 1);
    cb->addWidget(m_cbarSep, 0, Qt::AlignHCenter);
    for (QToolButton* b : { m_fitBtn, m_rulerBtn, m_exportBtn, m_setBtn }) cb->addWidget(b);
    // vistas, inclinação e hora (o Astrolábio faz isso no outro modo)
    m_cview = new QFrame(this);
    m_cview->setObjectName(QStringLiteral("cartoCard"));
    m_cview->setFixedWidth(320);   // cabe "Topográfico ▾", a vista de nome mais comprido
    auto* cv = new QVBoxLayout(m_cview);
    cv->setContentsMargins(12, 10, 12, 10);
    cv->setSpacing(8);
    cv->addWidget(m_viewSeg);
    auto row = [&](const QString& label, QSlider** s, QLabel** v, QToolButton** play) {
        auto* h = new QHBoxLayout();
        h->setSpacing(6);
        if (play) {
            *play = new QToolButton(m_cview);
            (*play)->setObjectName(QStringLiteral("cartoBtn"));
            (*play)->setFixedSize(22, 22);
            (*play)->setIconSize(QSize(12, 12));
            (*play)->setToolTip(tr("Passar o dia (P)"));
            (*play)->setCursor(Qt::PointingHandCursor);
            h->addWidget(*play);
        }
        auto* cap = new QLabel(label, m_cview);
        cap->setObjectName(QStringLiteral("cartoCaption"));
        cap->setFixedWidth(play ? 44 : 72);
        h->addWidget(cap);
        *s = new QSlider(Qt::Horizontal, m_cview);
        h->addWidget(*s, 1);
        *v = new QLabel(m_cview);
        (*v)->setObjectName(QStringLiteral("cartoValue"));
        (*v)->setFixedWidth(40);
        (*v)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        h->addWidget(*v);
        cv->addLayout(h);
    };
    row(tr("INCLINAÇÃO"), &m_cPitch, &m_cPitchV, nullptr);
    row(tr("HORA"), &m_cHour, &m_cHourV, &m_cPlay);
    m_cPitch->setRange(20, 90);
    m_cHour->setRange(0, 24 * 20);
    connect(m_cPitch, &QSlider::sliderMoved, this, [this](int v) { m_canvas->setView(m_canvas->angle(), v, true); });
    connect(m_cPitch, &QSlider::sliderReleased, this, [this]() { m_canvas->setView(m_canvas->angle(), m_cPitch->value(), false); });
    connect(m_cHour, &QSlider::sliderMoved, this, [this](int v) { m_canvas->setHour(v / 20.0, false); });
    connect(m_cHour, &QSlider::sliderReleased, this, [this]() { m_canvas->setHour(m_cHour->value() / 20.0, true); scheduleSave(); });
    connect(m_cPlay, &QToolButton::clicked, this, [this]() { setPlaying(!m_playTimer->isActive()); });

    m_flashTimer = new QTimer(this);
    m_flashTimer->setSingleShot(true);
    m_flashTimer->setInterval(3500);
    connect(m_flashTimer, &QTimer::timeout, this, [this]() { m_flash.clear(); syncUi(); });
}

void Panel::flash(const QString& msg)
{
    m_flash = msg;
    m_flashTimer->start();
    syncUi();
}

void Panel::cartelaPicked(int i)
{
    if (i < 0 || i >= 12) return;
    const CItem& c = kCItems[i];
    if (c.build) {
        setClimate(false);
        ensureView(QStringLiteral("full"));
        m_canvas->opts().buildKind = QString::fromLatin1(c.build);
        m_canvas->optsChanged();
        setTool(QStringLiteral("build"));
    } else if (c.brush >= 0) {
        ensureView(QStringLiteral("terrain"));
        setClimate(false);
        setBrush(static_cast<Brush>(c.brush));
    } else {
        pickTool(QString::fromLatin1(c.tool), QPoint());
    }
    m_dockFromRing = false; m_dockFromCartela = true; m_dockUser = false;
    syncUi();
    placeDock();
}

// o subtítulo da Cartela: o mar que dá nome à região
QString Panel::regionName()
{
    if (!m_hasWorld || !m_doc.labels.autoOn) return QString();   // sem nomes sugeridos, sem mar com nome
    const auto& F = m_doc.ensureNames(m_doc.labels.density);
    const NameFeature* s = nullptr;
    for (const NameFeature& f : F) if (f.kind == QLatin1String("ocean")) { s = &f; break; }
    if (!s) for (const NameFeature& f : F) if (f.kind == QLatin1String("sea")) { s = &f; break; }
    if (!s) return QString();
    const Label* lab = m_canvas->scene().nameByRef(s->id);
    return tr("Região do %1").arg(lab ? lab->text : s->name);
}

void Panel::syncCartela()
{
    if (!m_cartela) return;
    const bool cartelaUi = m_ui == QLatin1String("cartela");
    const int idx = m_store.indexOf(m_mapId);
    m_cartela->setTitle(idx >= 0 ? m_store.maps()[idx].name : autoTitle(), regionName());
    m_cartela->setLeaguePx(m_canvas->leaguePx());
    int active = -1;
    for (int i = 0; i < 12; i++) {
        const CItem& c = kCItems[i];
        const bool on = c.build ? (m_tool == QLatin1String("build") && m_canvas->opts().buildKind == QLatin1String(c.build))
                      : c.brush >= 0 ? (m_tool == QLatin1String("brush") && int(m_brush) == c.brush && !m_canvas->showClimate())
                                     : m_tool == QLatin1String(c.tool);
        if (on) { active = i; break; }
    }
    m_cartela->setActive(active);
    if (cartelaUi) {
        m_rose->setAngle(m_canvas->angle());
        const QString act = (m_tool == QLatin1String("brush") && m_canvas->showClimate()) ? QStringLiteral("climate") : m_tool;
        for (auto it = m_cbarBtns.begin(); it != m_cbarBtns.end(); ++it) it.value()->setChecked(it.key() == act);
        for (auto it = m_viewBtns.begin(); it != m_viewBtns.end(); ++it)
            it.value()->setChecked(it.key() == QLatin1String("ink") ? isArtMode(m_canvas->mode()) : it.key() == m_canvas->mode());
        if (QToolButton* ab = m_viewBtns.value(QStringLiteral("ink"))) ab->setText(artModeShort(isArtMode(m_canvas->mode()) ? m_canvas->mode() : m_astro->artMode()) + QStringLiteral(" ▾"));
        if (!m_cPitch->isSliderDown()) { m_cPitch->blockSignals(true); m_cPitch->setValue(qRound(m_canvas->pitch())); m_cPitch->blockSignals(false); }
        if (!m_cHour->isSliderDown()) { m_cHour->blockSignals(true); m_cHour->setValue(qRound(m_canvas->hour() * 20)); m_cHour->blockSignals(false); }
        m_cPitchV->setText(QStringLiteral("%1°").arg(qRound(m_canvas->pitch())));
        const int mm = qRound(std::fmod(m_canvas->hour() + 24, 24.0) * 60) % 1440;
        m_cHourV->setText(QStringLiteral("%1:%2").arg(mm / 60, 2, 10, QLatin1Char('0')).arg(mm % 60, 2, 10, QLatin1Char('0')));
        m_cPlay->setIcon(icon(m_playTimer->isActive() ? QStringLiteral("pause") : QStringLiteral("play"), Theme::toColor(Theme::textMuted()), Theme::toColor(Theme::accentDefault())));
    }
}

void Panel::applyUiMode()
{
    const bool cartelaUi = m_ui == QLatin1String("cartela");
    m_astro->setVisible(!cartelaUi);
    m_hourPill->setVisible(!cartelaUi);
    m_toolPill->setVisible(!cartelaUi);
    m_viewSeg->setVisible(cartelaUi);
    m_cbar->setVisible(true);
    for (QToolButton* b : std::as_const(m_cbarBtns)) b->setVisible(cartelaUi);
    m_cbarSep->setVisible(cartelaUi);
    m_cview->setVisible(cartelaUi);
    m_cartela->setVisible(false);
    m_rose->setVisible(cartelaUi);
    for (auto it = m_uiBtns.begin(); it != m_uiBtns.end(); ++it) it.value()->setChecked(it.key() == m_ui);
    m_cartelaChk->setVisible(false);
    m_cartelaChk->setChecked(m_cartelaOn);
    m_cartelaChk->setIcon(icon(m_cartelaOn ? QStringLiteral("check") : QStringLiteral("box"), Theme::toColor(Theme::textMuted()), Theme::toColor(Theme::accentDefault())));
    const bool spr = m_canvas->opts().sprites;
    m_spritesChk->setChecked(spr);
    m_spritesChk->setIcon(icon(spr ? QStringLiteral("check") : QStringLiteral("box"), Theme::toColor(Theme::textMuted()), Theme::toColor(Theme::accentDefault())));
    const bool frame = m_canvas->opts().frame;
    m_frameChk->setChecked(frame);
    m_frameChk->setIcon(icon(frame ? QStringLiteral("check") : QStringLiteral("box"), Theme::toColor(Theme::textMuted()), Theme::toColor(Theme::accentDefault())));
    positionOverlays();
    syncUi();
}

// ── exportar a imagem pro livro ──
void Panel::buildExportCard()
{
    m_export = new QFrame(this);
    m_export->setObjectName(QStringLiteral("cartoCard"));
    m_export->setFixedWidth(300);
    auto* l = new QVBoxLayout(m_export);
    l->setContentsMargins(16, 14, 16, 16);
    l->setSpacing(10);
    auto* t = new QLabel(tr("Exportar imagem"), m_export);
    t->setObjectName(QStringLiteral("cartoCardTitle"));
    l->addWidget(t);
    auto* cap = new QLabel(tr("TAMANHO"), m_export);
    cap->setObjectName(QStringLiteral("cartoCaption"));
    l->addWidget(cap);
    l->addLayout(newSeg(m_export, { QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3") }, { tr("1×"), tr("2×"), tr("3×") },
                        { tr("Do tamanho da tela"), tr("Dobro: bom pra imprimir"), tr("Triplo: pra página inteira do livro") },
                        m_expScaleBtns, [this](const QString& v) { m_expScale = v.toDouble(); syncUi(); }));
    m_expSize = new QLabel(m_export);
    m_expSize->setObjectName(QStringLiteral("cartoNote"));
    l->addWidget(m_expSize);
    m_expCartela = new QToolButton(m_export);
    m_expCartela->setObjectName(QStringLiteral("cartoCheck"));
    m_expCartela->setCheckable(true);
    m_expCartela->setChecked(true);
    m_expCartela->setCursor(Qt::PointingHandCursor);
    m_expCartela->setText(tr("Cartela (título, escala e legenda)"));
    m_expCartela->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_expCartela->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_expCartela, &QToolButton::clicked, this, [this](bool on) { m_expWithCartela = on; syncUi(); });
    l->addWidget(m_expCartela);
    auto* go = new QPushButton(tr("Salvar PNG…"), m_export);
    go->setObjectName(QStringLiteral("cartoGo"));
    go->setCursor(Qt::PointingHandCursor);
    connect(go, &QPushButton::clicked, this, &Panel::exportPng);
    l->addWidget(go);
    auto* n = new QLabel(tr("Sai a vista de agora, sem os botões. Na vista Tinta, vira o mapa das primeiras páginas do livro."), m_export);
    n->setObjectName(QStringLiteral("cartoNote"));
    n->setWordWrap(true);
    l->addWidget(n);
    m_export->hide();
}

void Panel::exportPng()
{
    if (!m_hasWorld) return;
    const int idx = m_store.indexOf(m_mapId);
    QString name = idx >= 0 ? m_store.maps()[idx].name : autoTitle();
    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral(" "));
    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar mapa"), name.trimmed() + QStringLiteral(".png"), tr("Imagem PNG (*.png)"));
    if (path.isEmpty()) return;
    const bool withCartela = m_expWithCartela;
    const bool wasMin = m_cartela->minimized();
    std::vector<QRectF> avoid;
    if (withCartela) {
        m_cartela->setMinimized(false);
        const QSize cs = m_cartela->sizeHint();
        avoid.push_back(QRectF(16, m_canvas->height() - cs.height() - 14, cs.width(), cs.height()).adjusted(-6, -6, 6, 6));
    }
    QImage img = m_canvas->exportImage(m_expScale, [&](QPainter& p) {
        if (!withCartela) return;
        // a Cartela inteira (aberta, sem o botão de recolher) no canto, como num mapa impresso
        m_cartela->setInteractive(false);
        m_cartela->setMinimized(false);
        syncCartela();
        const QSize cs = m_cartela->sizeHint();
        m_cartela->resize(cs);
        m_cartela->render(&p, QPoint(16, m_canvas->height() - cs.height() - 14), QRegion(), QWidget::DrawChildren);
        m_cartela->setInteractive(true);
        m_cartela->setMinimized(wasMin);
    }, avoid);
    m_cartela->setMinimized(wasMin);
    if (img.isNull() || !img.save(path, "PNG")) { flash(tr("Não deu pra salvar a imagem nesse lugar.")); return; }
    m_export->hide();
    flash(tr("Imagem salva: %1").arg(QFileInfo(path).fileName()));
    positionOverlays();
}

} // namespace carto
