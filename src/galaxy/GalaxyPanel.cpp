#include "GalaxyPanel.h"
#include "GalaxyCaderno.h"

#include "../TerritorioStore.h"
#include "../Theme.h"
#include "../carto/CartoAstrolabe.h"
#include "../carto/CartoCartela.h"
#include "../carto/CartoIcons.h"
#include "../carto/CartoRing.h"

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTextEdit>
#include <QWheelEvent>
#include <QSaveFile>
#include <QSettings>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

namespace galaxy {

namespace {
constexpr double DEG = PI_D / 180;
constexpr double TIME_LOOP = 240;   // uma volta inteira no aro é um ano do planeta de dentro (4 minutos de relógio)
QString viewName(const QString& m)
{
    if (m == QLatin1String("full")) return Panel::tr("Completo");
    if (m == QLatin1String("atlas")) return Panel::tr("Atlas");
    if (m == QLatin1String("terrain")) return Panel::tr("Céu");
    return carto::artModeName(m);
}
QString sizeNameOf(Size s)
{
    switch (s) {
    case Size::Pequena: return Panel::tr("Pequena");
    case Size::Media: return Panel::tr("Média");
    case Size::Grande: return Panel::tr("Grande");
    default: return Panel::tr("Imensa");
    }
}
// a barrinha da escala: risco em U, como no Lab
class ScaleBar : public QWidget {
public:
    using QWidget::QWidget;
    QColor color;
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setPen(QPen(color, 1));
        const double w = width() - 1, h = height() - 1;
        p.drawLine(QPointF(0.5, 0), QPointF(0.5, h));
        p.drawLine(QPointF(0.5, h), QPointF(w, h));
        p.drawLine(QPointF(w, 0), QPointF(w, h));
    }
};
}

QString nf(double n, int d) { return QLocale().toString(n, 'f', d); }
QString sig(double n)
{
    if (n >= 100) return nf(n, 0);
    if (n >= 10) return nf(n, 1);
    if (n >= 1) return nf(n, 2);
    if (n >= 0.01) return nf(n, 3);
    return QLocale().toString(n, 'e', 1);
}
QString celsius(double k)
{
    const double c = k - 273.15;
    return QStringLiteral("%1%2 °C").arg(c < 0 ? QStringLiteral("−") : QString(), nf(std::fabs(std::round(c)), 0));
}
QString periodText(double days)
{
    if (days < 1) return Panel::tr("%1 horas").arg(nf(days * 24, 1));
    if (days < 400) return Panel::tr("%1 dias").arg(nf(days, days < 10 ? 1 : 0));
    return Panel::tr("%1 anos").arg(nf(days / 365.25, days < 3650 ? 1 : 0));
}
QString fmtCount(double n)
{
    if (n < 1e6) return Panel::tr("%1 pessoas").arg(nf(std::round(n / 100) * 100));
    if (n < 1e9) return Panel::tr("%1 milhões de pessoas").arg(nf(n / 1e6, n < 1e7 ? 1 : 0));
    if (n < 1e12) return Panel::tr("%1 bilhões de pessoas").arg(nf(n / 1e9, n < 1e10 ? 1 : 0));
    return Panel::tr("%1 trilhões de pessoas").arg(nf(n / 1e12, 1));
}

Panel::Panel(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("galaxyPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    // O mapa cobre o painel inteiro: sem isso, o Mapa de fantasia aqui embaixo se repinta junto a cada quadro.
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::StrongFocus);
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(1500);
    connect(m_saveTimer, &QTimer::timeout, this, &Panel::saveNow);
    m_uiTimer = new QTimer(this);
    m_uiTimer->setInterval(120);
    connect(m_uiTimer, &QTimer::timeout, this, [this]() { if (isVisible() && isOpen()) { syncCview(); syncScale(); syncFocusBar(); if (m_hourPill->isVisible() && m_hourPill->sizeHint().width() != m_hourPill->width()) positionOverlays(); if (m_tool == QLatin1String("build")) placeSelPill(); } });
    QSettings st;
    m_ui = st.value(QStringLiteral("carto/ui"), QStringLiteral("astro")).toString();
    buildUi();
    wireCanvas();
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, &Panel::applyTheme);
    m_uiTimer->start();
}
Panel::~Panel()
{
    m_uiTimer->stop();
    if (m_saveTimer->isActive()) saveNow();
    if (m_canvas) m_canvas->setDoc(nullptr);
}

QFrame* Panel::makeFloat(const QString& name)
{
    auto* f = new QFrame(this);
    f->setObjectName(name);
    f->setProperty("cartoFloat", true);
    f->setAttribute(Qt::WA_StyledBackground, true);
    return f;
}
QToolButton* Panel::makeBtn(QWidget* parent, const QString& iconName, const QString& tip, const QString& text)
{
    auto* b = new QToolButton(parent);
    b->setObjectName(QStringLiteral("cartoBtn"));
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setIconSize(QSize(18, 18));
    b->setFixedHeight(30);
    b->setFocusPolicy(Qt::NoFocus);
    if (!text.isEmpty()) { b->setText(text); b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); }
    else b->setFixedWidth(30);
    m_icons.append({ b, iconName });
    return b;
}

// ── montagem ──
void Panel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    m_canvas = new Canvas(this);
    root->addWidget(m_canvas);

    // o título da galáxia: clicar troca o nome; a setinha abre o menu
    m_titlePill = makeFloat(QStringLiteral("cartoTitle"));
    auto* tl = new QHBoxLayout(m_titlePill);
    tl->setContentsMargins(14, 6, 6, 6);
    tl->setSpacing(6);
    auto* tcol = new QVBoxLayout();
    tcol->setSpacing(1);
    m_title = new QLabel(m_titlePill);
    m_title->setObjectName(QStringLiteral("cartoTitleText"));
    m_title->setCursor(Qt::IBeamCursor);
    m_title->setToolTip(tr("Clique pra trocar o nome da galáxia"));
    m_title->installEventFilter(this);
    m_titleEdit = new QLineEdit(m_titlePill);
    m_titleEdit->setObjectName(QStringLiteral("cartoTitleEdit"));
    m_titleEdit->hide();
    connect(m_titleEdit, &QLineEdit::editingFinished, this, [this]() {
        if (m_titleEdit->isHidden()) return;
        const QString v = m_titleEdit->text().trimmed();
        m_titleEdit->hide(); m_title->show();
        if (!v.isEmpty()) { m_doc.userNames.insert(QStringLiteral("GAL"), v); setMapName(v); scheduleSave(); }
        m_canvas->setFocus();
        syncUi();
    });
    m_sub = new QLabel(m_titlePill);
    m_sub->setObjectName(QStringLiteral("cartoSubText"));
    tcol->addWidget(m_title);
    tcol->addWidget(m_titleEdit);
    tcol->addWidget(m_sub);
    tl->addLayout(tcol);
    m_menuBtn = makeBtn(m_titlePill, QStringLiteral("down"), tr("Trocar, criar, renomear ou excluir mapas"));
    connect(m_menuBtn, &QToolButton::clicked, this, &Panel::showMapMenu);
    tl->addWidget(m_menuBtn, 0, Qt::AlignTop);

    // a barra da direita: as ferramentas (só na Cartela) e as ações do mapa nas duas interfaces
    m_cbar = makeFloat(QStringLiteral("cartoCbar"));
    auto* cb = new QVBoxLayout(m_cbar);
    cb->setContentsMargins(4, 4, 4, 4);
    cb->setSpacing(2);
    m_panBtn = makeBtn(m_cbar, QStringLiteral("hand"), tr("Mover (H)"));
    m_panBtn->setCheckable(true);
    connect(m_panBtn, &QToolButton::clicked, this, [this]() { setTool(QStringLiteral("pan")); });
    cb->addWidget(m_panBtn);
    m_cbarSep = new QFrame(m_cbar);
    m_cbarSep->setObjectName(QStringLiteral("cartoSep"));
    m_cbarSep->setFixedSize(18, 1);
    cb->addWidget(m_cbarSep, 0, Qt::AlignHCenter);
    m_fitBtn = makeBtn(m_cbar, QStringLiteral("fit"), tr("Enquadrar (F)"));
    connect(m_fitBtn, &QToolButton::clicked, m_canvas, &Canvas::fitMap);
    cb->addWidget(m_fitBtn);
    m_rulerBtn = makeBtn(m_cbar, QStringLiteral("ruler"), tr("Régua: distância em anos-luz (R)"));
    m_rulerBtn->setCheckable(true);
    connect(m_rulerBtn, &QToolButton::clicked, this, [this]() { setTool(m_tool == QLatin1String("ruler") ? QStringLiteral("pan") : QStringLiteral("ruler")); });
    cb->addWidget(m_rulerBtn);
    buildHab();
    cb->addWidget(m_habBtn);
    m_cadernoBtn = makeBtn(m_cbar, QStringLiteral("galBook"), tr("Caderno (C)"));
    connect(m_cadernoBtn, &QToolButton::clicked, this, [this]() { openCaderno(m_canvas->sel); });
    cb->addWidget(m_cadernoBtn);
    m_exportBtn = makeBtn(m_cbar, QStringLiteral("image"), tr("Exportar imagem (PNG pro livro)"));
    m_exportBtn->setCheckable(true);
    connect(m_exportBtn, &QToolButton::clicked, this, [this]() { m_settings->hide(); m_export->setVisible(!m_export->isVisible()); positionOverlays(); syncUi(); });
    cb->addWidget(m_exportBtn);
    m_setBtn = makeBtn(m_cbar, QStringLiteral("gear"), tr("Ajustes"));
    m_setBtn->setCheckable(true);
    connect(m_setBtn, &QToolButton::clicked, this, [this]() { m_export->hide(); m_settings->setVisible(!m_settings->isVisible()); positionOverlays(); syncUi(); });
    cb->addWidget(m_setBtn);
    m_helpBtn = makeBtn(m_cbar, QStringLiteral("galHelp"), tr("Como usar o Galaxy Maps"));
    connect(m_helpBtn, &QToolButton::clicked, this, [this]() { if (m_link.openHelp) m_link.openHelp(QStringLiteral("galaxy-maps")); });
    cb->addWidget(m_helpBtn);

    buildSettings();
    buildExport();
    buildCview();

    // o Astrolábio e as pílulas dele
    m_astro = new carto::Astrolabe(this);
    m_astro->setGalaxy(true);
    connect(m_astro, &carto::Astrolabe::modeRequested, this, &Panel::setViewMode);
    m_artPick = new carto::ArtPicker(this);
    m_artPick->setGalaxy(true);
    connect(m_astro, &carto::Astrolabe::artPickRequested, this, [this](QPoint at) { m_artPick->openAt(at, m_canvas->view); });
    connect(m_artPick, &carto::ArtPicker::picked, this, &Panel::setViewMode);
    connect(m_astro, &carto::Astrolabe::angleDragged, this, [this](double a, bool) { m_canvas->cam.yaw = -a; syncCview(); m_canvas->update(); });
    connect(m_astro, &carto::Astrolabe::pitchDragged, this, [this](double p, bool) { m_canvas->cam.tilt = (90 - p) * DEG; syncCview(); m_canvas->update(); });
    connect(m_astro, &carto::Astrolabe::hourDragged, this, [this](double h, bool) {
        // o aro inteiro é um ciclo: 0 a 24 "horas" do instrumento viram 0 a 240 segundos do relógio
        const double n = std::floor(m_doc.simT / TIME_LOOP);
        m_doc.simT = n * TIME_LOOP + std::fmod(std::fmod(h, 24.0) + 24, 24.0) / 24 * TIME_LOOP;
        syncCview();
        m_canvas->update();
    });
    connect(m_astro, &carto::Astrolabe::northRequested, this, [this]() { m_canvas->setTilt(m_canvas->cam.tilt, std::round(m_canvas->cam.yaw / TAU) * TAU); });

    m_hourPill = makeFloat(QStringLiteral("cartoHour"));
    auto* hp = new QHBoxLayout(m_hourPill);
    hp->setContentsMargins(4, 4, 10, 4);
    hp->setSpacing(4);
    m_playBtn = makeBtn(m_hourPill, QStringLiteral("pause"), tr("Passar o tempo (P)"));
    connect(m_playBtn, &QToolButton::clicked, this, [this]() { setPlay(!m_canvas->play); });
    m_timeLabel = new QLabel(m_hourPill);
    m_timeLabel->setObjectName(QStringLiteral("cartoHourText"));
    hp->addWidget(m_playBtn); hp->addWidget(m_timeLabel);

    m_toolPill = makeFloat(QStringLiteral("cartoTool"));
    auto* tpl = new QHBoxLayout(m_toolPill);
    tpl->setContentsMargins(4, 4, 4, 4);
    m_toolBtn = makeBtn(m_toolPill, QStringLiteral("ring"), tr("Ferramentas (ou botão direito no mapa)"), QStringLiteral(" "));
    m_toolBtn->setObjectName(QStringLiteral("cartoToolBtn"));
    connect(m_toolBtn, &QToolButton::clicked, this, [this]() {
        const QPoint c = m_toolPill->mapTo(this, QPoint(m_toolPill->width() / 2, 0));
        openRing(QPointF(c.x(), c.y() - 140), QString());
    });
    tpl->addWidget(m_toolBtn);

    m_rose = new carto::Rose(this);
    connect(m_rose, &carto::Rose::angleDragged, this, [this](double a, bool) { m_canvas->cam.yaw = -a; m_canvas->update(); });
    connect(m_rose, &carto::Rose::northRequested, this, [this]() { m_canvas->setTilt(m_canvas->cam.tilt, std::round(m_canvas->cam.yaw / TAU) * TAU); });

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("cartoHint"));
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setAttribute(Qt::WA_TransparentForMouseEvents);

    // a escala, em anos-luz
    m_scale = new QWidget(this);
    m_scale->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* sl = new QVBoxLayout(m_scale);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(3);
    m_scaleText = new QLabel(m_scale);
    m_scaleText->setObjectName(QStringLiteral("cartoSubText"));
    m_scaleText->setAlignment(Qt::AlignRight);
    m_scaleBar = new ScaleBar(m_scale);
    m_scaleBar->setFixedHeight(5);
    sl->addWidget(m_scaleText);
    sl->addWidget(m_scaleBar, 0, Qt::AlignRight);

    // o aviso com Desfazer
    m_toast = makeFloat(QStringLiteral("cartoToast"));
    auto* tsl = new QHBoxLayout(m_toast);
    tsl->setContentsMargins(14, 6, 6, 6);
    tsl->setSpacing(10);
    m_toastText = new QLabel(m_toast);
    m_toastText->setObjectName(QStringLiteral("cartoValue"));
    m_toastUndo = new QPushButton(tr("Desfazer"), m_toast);
    m_toastUndo->setObjectName(QStringLiteral("gmLink"));
    m_toastUndo->setCursor(Qt::PointingHandCursor);
    m_toastUndo->setFocusPolicy(Qt::NoFocus);
    connect(m_toastUndo, &QPushButton::clicked, m_canvas, &Canvas::undo);
    tsl->addWidget(m_toastText); tsl->addWidget(m_toastUndo);
    m_toast->hide();
    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, m_toast, &QWidget::hide);

    // o nome do que está embaixo do ponteiro
    m_tip = new QLabel(this);
    m_tip->setObjectName(QStringLiteral("gmTip"));
    m_tip->setTextFormat(Qt::RichText);
    m_tip->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_tip->hide();

    // a barra do Modo Foco
    m_focusBar = makeFloat(QStringLiteral("cartoFocus"));
    auto* fl = new QHBoxLayout(m_focusBar);
    fl->setContentsMargins(12, 5, 6, 5);
    fl->setSpacing(8);
    m_focusSwatch = new QLabel(m_focusBar);
    m_focusSwatch->setFixedSize(10, 10);
    m_focusName = new QLabel(m_focusBar);
    m_focusName->setObjectName(QStringLiteral("cartoValue"));
    auto* fo = new QPushButton(tr("Sair do foco"), m_focusBar);
    fo->setObjectName(QStringLiteral("gmLink"));
    fo->setCursor(Qt::PointingHandCursor);
    fo->setFocusPolicy(Qt::NoFocus);
    connect(fo, &QPushButton::clicked, this, [this]() { m_canvas->setFocus2(QString()); renderPanel(); });
    fl->addWidget(m_focusSwatch); fl->addWidget(m_focusName); fl->addWidget(fo);
    m_focusBar->hide();

    buildSide();
    buildPeek();
    buildBuildBar();
    buildRealmDock();

    m_ring = new carto::Ring(this);
    m_ring->setItems({
        { QStringLiteral("pan"), tr("Mover"), QStringLiteral("hand"), true },
        { QStringLiteral("realm"), tr("Territórios"), QStringLiteral("flag"), true },
        { QStringLiteral("build"), tr("Construir"), QStringLiteral("galBuild"), true },
        { QStringLiteral("names"), tr("Nomes"), QStringLiteral("names"), true },
        { QStringLiteral("explore"), tr("Explorar"), QStringLiteral("explore"), true },
        { QStringLiteral("edit"), tr("Editar"), QStringLiteral("galSliders"), true },
        { QStringLiteral("traffic"), tr("Tráfego"), QStringLiteral("galRocket"), true },
    });
    connect(m_ring, &carto::Ring::picked, this, [this](const QString& id, QPoint) { pickTool(id, m_ringTarget); });

    m_caderno = new Caderno(this, m_canvas, &m_doc);
    m_caderno->hide();
}
void Panel::buildSettings()
{
    m_settings = new QFrame(this);
    m_settings->setObjectName(QStringLiteral("cartoCard"));
    m_settings->setFixedWidth(320);
    auto* l = new QVBoxLayout(m_settings);
    l->setContentsMargins(14, 12, 14, 14);
    l->setSpacing(8);
    auto* t = new QLabel(tr("Ajustes do mapa"), m_settings);
    t->setObjectName(QStringLiteral("cartoCardTitle"));
    l->addWidget(t);
    const auto caption = [&](const QString& text) { auto* c = new QLabel(text, m_settings); c->setObjectName(QStringLiteral("cartoCaption")); l->addWidget(c); };
    const auto seg = [&](const QStringList& values, const QStringList& labels, const QStringList& tips, QHash<QString, QToolButton*>& out, const std::function<void(const QString&)>& fn) {
        auto* row = new QHBoxLayout();
        row->setSpacing(4);
        for (int i = 0; i < values.size(); i++) {
            auto* b = new QToolButton(m_settings);
            b->setObjectName(QStringLiteral("cartoSeg"));
            b->setText(labels[i]); b->setToolTip(tips[i]);
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            const QString v = values[i];
            connect(b, &QToolButton::clicked, this, [fn, v]() { fn(v); });
            out.insert(v, b);
            row->addWidget(b);
        }
        l->addLayout(row);
    };
    caption(tr("VISUAL DA GALÁXIA"));
    seg({ QStringLiteral("cheio"), QStringLiteral("limpo") }, { tr("Cheio"), tr("Limpo") }, { tr("O brilho inteiro da galáxia (V)"), tr("O brilho baixo: as estrelas e os nomes aparecem mais (V)") }, m_skyBtns, [this](const QString& v) { setSkyMode(v); });
    caption(tr("INTERFACE"));
    seg({ QStringLiteral("astro"), QStringLiteral("cartela") }, { tr("Astrolábio"), tr("Cartela") },
        { tr("O instrumento no canto junta vista, rotação, inclinação e tempo; as ferramentas abrem num anel"), tr("Vistas e inclinação no cartão, ferramentas na barra do lado; a bússola no canto gira o mapa") },
        m_uiBtns, [this](const QString& v) { setUiMode(v); });
    const auto check = [&](const QString& text, const std::function<void(bool)>& fn) {
        auto* b = new QToolButton(m_settings);
        b->setObjectName(QStringLiteral("cartoCheck"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setText(text);
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(b, &QToolButton::clicked, this, fn);
        l->addWidget(b);
        return b;
    };
    m_lyTerr = check(tr("Territórios"), [this](bool on) { m_canvas->ly.terr = on; m_canvas->docChanged(); syncUi(); });
    m_lyNames = check(tr("Nomes"), [this](bool on) { m_canvas->ly.names = on; m_canvas->docChanged(); syncUi(); });
    m_lyOrb = check(tr("Órbitas"), [this](bool on) { m_canvas->ly.orb = on; m_canvas->docChanged(); syncUi(); });
    m_lySprites = check(tr("Estações e colossos (S)"), [this](bool on) { setSpritesOn(on); });
    m_lyFrame = check(tr("Moldura da galáxia"), [this](bool on) { m_canvas->frameOn = on; m_canvas->docChanged(); syncUi(); });
    m_lyPeek = check(tr("Prévia ao passar o mouse"), [this](bool on) { setPeekOn(on); });
    m_lyTraffic = check(tr("Tráfego de naves"), [this](bool on) { setTrafficOn(on); });
    m_trafficLevel = new QComboBox(m_settings);
    m_trafficLevel->setObjectName(QStringLiteral("cartoCombo"));
    m_trafficLevel->addItem(tr("Pouco movimento"), 1);
    m_trafficLevel->addItem(tr("Movimento normal"), 2);
    m_trafficLevel->addItem(tr("Muito movimento"), 3);
    m_trafficLevel->setCurrentIndex(1);
    connect(m_trafficLevel, &QComboBox::activated, this, [this]() { m_canvas->trafficLevel = m_trafficLevel->currentData().toInt(); m_canvas->docChanged(); });
    l->addWidget(m_trafficLevel);
    m_trafficLevel->hide();
    m_settings->hide();
}
void Panel::buildExport()
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
    auto* row = new QHBoxLayout();
    row->setSpacing(4);
    const QStringList tips = { tr("Do tamanho da tela"), tr("Dobro: bom pra imprimir"), tr("Triplo: pra página inteira do livro") };
    for (int i = 1; i <= 3; i++) {
        auto* b = new QToolButton(m_export);
        b->setObjectName(QStringLiteral("cartoSeg"));
        b->setText(tr("%1×").arg(i));
        b->setToolTip(tips[i - 1]);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(b, &QToolButton::clicked, this, [this, i]() { m_expScale = i; syncUi(); });
        m_expBtns.insert(QString::number(i), b);
        row->addWidget(b);
    }
    l->addLayout(row);
    m_expSize = new QLabel(m_export);
    m_expSize->setObjectName(QStringLiteral("cartoNote"));
    l->addWidget(m_expSize);
    m_expTitle = new QToolButton(m_export);
    m_expTitle->setObjectName(QStringLiteral("cartoCheck"));
    m_expTitle->setCheckable(true);
    m_expTitle->setChecked(true);
    m_expTitle->setCursor(Qt::PointingHandCursor);
    m_expTitle->setFocusPolicy(Qt::NoFocus);
    m_expTitle->setText(tr("Título e escala"));
    m_expTitle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_expTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_expTitle, &QToolButton::clicked, this, [this](bool on) { m_expWithTitle = on; syncUi(); });
    l->addWidget(m_expTitle);
    auto* go = new QPushButton(tr("Salvar PNG…"), m_export);
    go->setObjectName(QStringLiteral("cartoGo"));
    go->setCursor(Qt::PointingHandCursor);
    go->setFocusPolicy(Qt::NoFocus);
    connect(go, &QPushButton::clicked, this, &Panel::doExport);
    l->addWidget(go);
    m_export->hide();
}
void Panel::buildCview()
{
    m_cview = new QFrame(this);
    m_cview->setObjectName(QStringLiteral("cartoCard"));
    m_cview->setFixedWidth(320);
    auto* cv = new QVBoxLayout(m_cview);
    cv->setContentsMargins(12, 10, 12, 10);
    cv->setSpacing(8);
    auto* vs = new QHBoxLayout();
    vs->setSpacing(2);
    for (const QString& v : { QStringLiteral("full"), QStringLiteral("atlas"), QStringLiteral("art"), QStringLiteral("terrain") }) {
        auto* b = new QToolButton(m_cview);
        b->setObjectName(QStringLiteral("cartoBtn"));
        b->setCheckable(true);
        b->setFixedHeight(30);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(b, &QToolButton::clicked, this, [this, v, b]() {
            if (v != QLatin1String("art")) { setViewMode(v); return; }
            if (m_canvas->skin()) { m_artPick->openAt(b->mapToGlobal(QPoint(b->width() / 2, -6)), m_canvas->view); syncUi(); }
            else setViewMode(m_canvas->artMode);
        });
        m_viewBtns.insert(v, b);
        vs->addWidget(b, 1);
    }
    cv->addLayout(vs);
    const auto row = [&](const QString& label, QSlider** s, QLabel** v, QToolButton** play) {
        auto* h = new QHBoxLayout();
        h->setSpacing(6);
        if (play) {
            *play = makeBtn(m_cview, QStringLiteral("pause"), tr("Passar o tempo (P)"));
            (*play)->setFixedSize(22, 22);
            (*play)->setIconSize(QSize(12, 12));
            h->addWidget(*play);
        }
        auto* cap = new QLabel(label, m_cview);
        cap->setObjectName(QStringLiteral("cartoCaption"));
        cap->setFixedWidth(play ? 44 : 72);
        h->addWidget(cap);
        *s = new QSlider(Qt::Horizontal, m_cview);
        (*s)->setFocusPolicy(Qt::NoFocus);
        h->addWidget(*s, 1);
        *v = new QLabel(m_cview);
        (*v)->setObjectName(QStringLiteral("cartoValue"));
        (*v)->setFixedWidth(40);
        (*v)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        h->addWidget(*v);
        cv->addLayout(h);
    };
    row(tr("INCLINAÇÃO"), &m_cPitch, &m_cPitchV, nullptr);
    row(tr("TEMPO"), &m_cTime, &m_cTimeV, &m_cPlay);
    m_cPitch->setRange(20, 90);
    m_cTime->setRange(0, int(TIME_LOOP));
    connect(m_cPitch, &QSlider::sliderMoved, this, [this](int v) { m_canvas->cam.tilt = (90 - v) * DEG; m_canvas->update(); syncCview(); });
    connect(m_cTime, &QSlider::sliderMoved, this, [this](int v) { m_doc.simT = std::floor(m_doc.simT / TIME_LOOP) * TIME_LOOP + v; m_canvas->update(); syncCview(); });
    connect(m_cPlay, &QToolButton::clicked, this, [this]() { setPlay(!m_canvas->play); });
}
void Panel::wireCanvas()
{
    connect(m_canvas, &Canvas::selectionChanged, this, &Panel::selected);
    connect(m_canvas, &Canvas::linkPicked, this, &Panel::linkPicked);
    connect(m_canvas, &Canvas::linkClicked, this, [this](const QString& id) { QTimer::singleShot(0, this, [this, id]() { openDuo(id, m_canvas->sel); }); });
    connect(m_canvas, &Canvas::clickedPlace, this, [this](const QString&) { openPanel(); });
    connect(m_canvas, &Canvas::requestRing, this, [this](const QPointF& at, const QString&) { openRing(at, m_tool == QLatin1String("ruler") ? QString() : m_canvas->pickAt(at.x(), at.y())); });
    connect(m_canvas, &Canvas::hintChanged, this, [this]() { syncHint(); m_toolBtn->setText(toolLabel() + QStringLiteral("  ▴")); syncBuildChecks(); });
    connect(m_canvas, &Canvas::toast, this, [this](const QString& text, bool undo) { if (text.isEmpty()) m_toast->hide(); else showToast(text, undo); });
    connect(m_canvas, &Canvas::toolDockChanged, this, [this]() { if (m_tool == QLatin1String("realm")) renderRealmDock(); });
    connect(m_canvas, &Canvas::nameClickRequested, this, [this](double mx, double my, const QString& key, bool) { nameClick(mx, my, key); });
    connect(m_canvas, &Canvas::focusChanged, this, &Panel::syncFocusBar);
    connect(m_canvas, &Canvas::pressed, this, [this]() { if (m_ring->isVisible()) m_ring->hide(); m_canvas->setFocus(); m_tip->hide(); });
    connect(m_canvas, &Canvas::tipChanged, this, &Panel::syncTip);
    connect(m_canvas, &Canvas::peekChanged, this, &Panel::peekChanged);
    connect(m_canvas, &Canvas::docEdited, this, &Panel::scheduleSave);
    connect(m_canvas, &Canvas::cameraMoved, this, [this]() { if (m_ui == QLatin1String("cartela")) m_rose->setAngle(-m_canvas->cam.yaw); });
}

void Panel::applyTheme()
{
    const QColor a = Theme::toColor(Theme::accentDefault());
    const QString soft = QStringLiteral("rgba(%1,%2,%3,0.18)").arg(a.red()).arg(a.green()).arg(a.blue());
    setStyleSheet(Theme::qss(QStringLiteral(
        "QWidget#galaxyPanel { background: %8; }"
        "QFrame#gmPeek { background: %1; border: 1px solid %2; border-radius: 12px; }"
        "QPushButton#gmHabRow { background: transparent; border: none; border-radius: 8px; text-align: left; }"
        "QPushButton#gmHabRow:hover { background: rgba(255,255,255,0.05); }"
        "QPushButton#gmHabRow:checked { background: %9; }"
        "QPushButton#gmHabBest { background: transparent; border: none; text-align: left; }"
        "QLabel#gmPeekName { color: %3; font-size: 15px; font-weight: 600; background: transparent; }"
        "QLabel#gmPeekMine { color: %5; font-size: 12px; font-weight: 600; background: transparent; }"
        "QLabel#gmTip { background: %1; border: 1px solid %2; border-radius: 8px; color: %3; font-size: 12px; padding: 5px 9px; }"
        "QPushButton#gmLink { background: transparent; border: none; color: %5; font-size: 12px; font-weight: 600; padding: 3px 6px; text-align: left; }"
        "QPushButton#gmLink:hover { text-decoration: underline; }"
        "QFrame#gmSearch { background: %1; border: 1px solid %2; border-radius: 22px; }"
        "QLineEdit#gmQuery { background: transparent; border: none; color: %3; font-size: 14px; padding: 0; }"
        "QFrame#gmResults, QFrame#gmPanel, QFrame#gmMini { background: %1; border: 1px solid %2; border-radius: 12px; }"
        "QScrollArea#gmScroll, QWidget#gmScrollBody { background: transparent; border: none; }"
        "QPushButton#gmRes { background: transparent; border: none; border-radius: 8px; padding: 5px 8px; text-align: left; color: %3; }"
        "QPushButton#gmRes:hover, QPushButton#gmRes[on=\"true\"] { background: %9; }"
        "QLabel#gmH1 { color: %7; font-size: 28px; font-weight: 600; background: transparent; }"
        "QLineEdit#gmRename { color: %7; font-size: 26px; font-weight: 600; background: transparent; border: none; border-bottom: 1px solid %5; padding: 0; }"
        "QLabel#gmSub { color: %6; font-size: 12px; background: transparent; }"
        "QLabel#gmGen { color: %10; font-size: 11px; background: transparent; }"
        "QLabel#gmEyebrow { color: %6; font-size: 11px; font-weight: 700; letter-spacing: 1px; background: transparent; }"
        "QLabel#gmFactK { color: %6; font-size: 11px; background: transparent; }"
        "QLabel#gmFactV { color: %3; font-size: 13px; font-weight: 500; background: transparent; }"
        "QLabel#gmStatK { color: %6; font-size: 12px; background: transparent; }"
        "QLabel#gmStatV { color: %3; font-size: 12px; background: transparent; }"
        "QPushButton#gmCrumb { background: transparent; border: none; color: %6; font-size: 11px; padding: 0; text-align: left; }"
        "QPushButton#gmCrumb:hover { color: %3; }"
        "QToolButton#gmAct { background: transparent; border: none; color: %5; font-size: 11px; font-weight: 600; padding: 2px; }"
        "QToolButton#gmAct:hover { color: %3; }"
        "QPushButton#gmWrite { background: %4; border: 1px solid %2; border-radius: 10px; color: %3; font-size: 13px; padding: 9px 12px; text-align: left; }"
        "QPushButton#gmWrite:hover { border-color: %5; }"
        "QToolButton#gmTab { background: transparent; border: none; border-bottom: 2px solid transparent; color: %6; font-size: 12px; font-weight: 600; padding: 8px 6px; }"
        "QToolButton#gmTab:hover { color: %3; }"
        "QToolButton#gmTab:checked { color: %5; border-bottom-color: %5; }"
        "QFrame#gmTabs { background: transparent; border: none; border-bottom: 1px solid %2; }"
        "QPushButton#gmDoc { background: %4; border: 1px solid %2; border-radius: 10px; padding: 9px; text-align: left; color: %3; font-size: 12px; }"
        "QPushButton#gmDoc:hover { border-color: %5; }"
        "QPushButton#gmDocNew { background: transparent; border: 1px dashed %2; border-radius: 10px; color: %5; font-size: 12px; font-weight: 600; min-height: 70px; }"
        "QPushButton#gmDocNew:hover { border-color: %5; }"
        "QPushButton#gmItem { background: transparent; border: 1px solid transparent; border-radius: 9px; }"
        "QPushButton#gmItem:hover { background: %4; border-color: %2; }"
        "QPushButton#gmChip { background: %4; border: 1px solid %2; border-radius: 13px; color: %3; font-size: 12px; padding: 2px 10px 2px 4px; text-align: left; }"
        "QPushButton#gmChip:hover { border-color: %5; }"
        "QLabel#gmTag { background: %4; border: 1px solid %2; border-radius: 9px; color: %3; font-size: 11px; padding: 2px 8px; }"
        "QLabel#gmTagHz { background: %9; border: 1px solid %5; border-radius: 9px; color: %5; font-size: 11px; padding: 2px 8px; }"
        "QToolButton#gmRound { background: %1; border: 1px solid %2; border-radius: 16px; color: %3; }"
        "QToolButton#gmRound:hover { background: %9; }"
        "QPushButton#gmMBtn { background: %4; border: 1px solid %2; border-radius: 9px; color: %3; font-size: 12px; padding: 6px 10px; }"
        "QPushButton#gmMBtn:hover { border-color: %5; }"
        "QPushButton#gmMPri { background: %5; border: none; border-radius: 9px; color: %12; font-size: 12px; font-weight: 700; padding: 6px 10px; }"
        "QLabel#gmMName { color: %7; font-size: 21px; font-weight: 600; background: transparent; }"
        "QLineEdit#gmField { background: %11; border: 1px solid %2; border-radius: 8px; color: %3; padding: 5px 8px; font-size: 13px; }"
        "QLabel#gmEditLabel { color: %6; font-size: 11.5px; background: transparent; }"
        "QPushButton#gmEditBtn { background: %4; border: 1px solid %2; border-radius: 8px; color: %3; font-size: 12px; padding: 6px 10px; }"
        "QPushButton#gmEditBtn:hover { border-color: %5; }"
        "QPushButton#gmEditBtn[danger=\"true\"] { color: #e0705c; }"
        "QLineEdit#gmEdTitle { background: transparent; border: none; color: %7; font-size: 22px; font-weight: 600; padding: 2px 0; }"
        "QTextEdit#gmEdBody { background: transparent; border: none; color: %3; font-size: 14px; padding: 8px 10px; }"
        "QFrame#gmEdBox { background: %4; border: 1px solid %2; border-radius: 10px; }"
        "QToolButton#gmFmt { background: transparent; border: none; border-radius: 6px; color: %6; font-size: 13px; padding: 3px 8px; }"
        "QToolButton#gmFmt:hover { background: %9; color: %3; }"
        "QPushButton#gmNameEdit { }"
        "QLineEdit#gmNameEdit { background: %1; border: 1px solid %5; border-radius: 8px; color: %3; font-size: 13px; font-weight: 600; padding: 5px 8px; }"
        "QToolButton#gmCard { background: transparent; border: 1px solid transparent; border-radius: 10px; color: %6; font-size: 11px; padding: 4px; }"
        "QToolButton#gmCard:hover { background: %4; color: %3; }"
        "QToolButton#gmCard:checked { background: %9; border-color: %5; color: %5; }"
    ).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textPrimary(), Theme::hoverOverlay(), Theme::accentDefault(),
          Theme::textMuted(), Theme::textBright(), QStringLiteral("#03050a"), soft)
     .arg(Theme::disabledText(), Theme::inputBackground(), Theme::readableTextOn(a).name())));
    const QColor muted = Theme::toColor(Theme::textMuted()), ink = Theme::toColor(Theme::textPrimary());
    for (const auto& [btn, name] : m_icons) btn->setIcon(carto::icon(name, muted, a));
    if (m_toolBtn) m_toolBtn->setIcon(carto::icon(QStringLiteral("ring"), ink, a));
    if (auto* sb = static_cast<ScaleBar*>(m_scaleBar)) sb->color = muted;
    if (m_caderno) m_caderno->applyTheme();
    syncUi();
    if (isOpen()) { m_artCache.clear(); renderBuildBar(); renderPanel(true); }
}

// ── abrir e salvar ──
bool Panel::openFile(const QString& path, const QString& name)
{
    closeMap();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool opened = m_doc.fromJson(root);
    QApplication::restoreOverrideCursor();
    if (!opened) return false;
    m_path = path;
    m_name = name;
    afterOpen();
    const QJsonObject v = root[QStringLiteral("ui")].toObject();
    if (v.contains(QStringLiteral("view"))) { const QString vw = v[QStringLiteral("view")].toString(); setViewMode(vw == QLatin1String("full") || vw == QLatin1String("atlas") || vw == QLatin1String("terrain") || skinOf(vw) ? vw : QStringLiteral("full")); }
    return true;
}
void Panel::startNew(const GalOpts& opts, const QString& path, const QString& name)
{
    closeMap();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_doc.generate(opts);
    QApplication::restoreOverrideCursor();
    if (!name.isEmpty()) m_doc.userNames.insert(QStringLiteral("GAL"), name);
    m_path = path;
    m_name = name;
    afterOpen();
    saveNow();
}
// Regenerar troca tudo: o estado de antes fica guardado inteiro no desfazer.
void Panel::regenerate(const GalOpts& opts, const QString& name)
{
    saveNow();
    QJsonObject was = m_doc.toJson();
    const QString path = m_path, oldName = m_name;
    startNew(opts, path, name);
    m_canvas->pushUndo(tr("Galáxia nova no lugar."), [this, was, oldName]() {
        m_canvas->setDoc(nullptr);
        m_doc = Doc();
        m_doc.fromJson(was);
        m_canvas->setDoc(&m_doc);
        m_canvas->sel = QStringLiteral("GAL");
        setMapName(oldName);
        syncTitle();
    });
}
void Panel::afterOpen()
{
    m_canvas->setDoc(&m_doc);
    m_canvas->setTiltNow(0);
    setTool(QStringLiteral("pan"));
    m_panelOpen = false; m_panelMini = false; m_openDoc = nullptr;
    hidePeek();
    m_duoLink.clear(); m_linkFrom.clear(); m_peopleFor.clear(); m_canvas->pickingLink = false; if (m_duo) m_duo->hide();
    m_canvas->sel = QStringLiteral("GAL");
    setPlay(true);
    syncUi();
    renderPanel();
    m_canvas->setFocus();
}
void Panel::closeMap()
{
    if (m_saveTimer->isActive() || !m_path.isEmpty()) saveNow();
    m_canvas->setDoc(nullptr);
    m_doc = Doc();
    m_path.clear();
    m_name.clear();
    m_openDoc = nullptr;
    m_editorFlush = nullptr;
}
// O que o editor do mapa deixou pendente vai pro disco de uma vez só, quando a pessoa para de escrever.
void Panel::flushDocSave()
{
    if (m_editorFlush) m_editorFlush();
    if (m_docPlacesDirty && m_link.places) m_link.places->save();
    m_docPlacesDirty = false;
    if (m_docProjectDirty && m_link.save) m_link.save();
    m_docProjectDirty = false;
}
void Panel::saveNow()
{
    m_saveTimer->stop();
    flushDocSave();
    if (!m_doc.gal || m_path.isEmpty()) return;
    QJsonObject o = m_doc.toJson();
    QJsonObject ui;
    ui[QStringLiteral("view")] = m_canvas->view;
    o[QStringLiteral("ui")] = ui;
    QSaveFile f(m_path);
    if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(o).toJson(QJsonDocument::Compact)); f.commit(); }
}
// Toda edição do documento passa aqui: o que mudou também precisa aparecer, mesmo com o mapa parado.
void Panel::scheduleSave() { m_saveTimer->start(); if (m_canvas) m_canvas->markDirty(); }
void Panel::setProjectLink(const carto::ProjectLink& link) { m_link = link; }
int Panel::systemCount() const
{
    int n = 0;
    if (m_doc.gal) for (const auto& s : m_doc.gal->systems) if (!s->gone) n++;
    return n;
}
void Panel::setMapName(const QString& n)
{
    m_name = n;
    if (renamed) renamed(n);
}

// ── posições ──
void Panel::positionOverlays()
{
    const int W = width(), H = height();
    for (QWidget* w : { static_cast<QWidget*>(m_titlePill), static_cast<QWidget*>(m_toolPill), static_cast<QWidget*>(m_hourPill), static_cast<QWidget*>(m_cbar) })
        if (w->layout()) { QCoreApplication::sendPostedEvents(w, QEvent::LayoutRequest); w->layout()->invalidate(); w->layout()->activate(); }
    m_titlePill->adjustSize();
    m_titlePill->move(16, 14);
    m_cbar->adjustSize();
    m_cbar->move(W - m_cbar->width() - 16, 14);
    const int cardRight = m_cbar->x() - 8;
    m_settings->adjustSize();
    m_settings->move(cardRight - m_settings->width(), 14);
    m_export->adjustSize();
    m_export->move(cardRight - m_export->width(), 14);
    const bool cart = m_ui == QLatin1String("cartela");
    m_astro->move(W - m_astro->width() - 4, H - m_astro->height() - 2);
    m_toolPill->adjustSize();
    m_hourPill->adjustSize();
    const int px = m_astro->x() - 8;
    m_toolPill->move(px - m_toolPill->width(), H - 18 - m_toolPill->height());
    m_hourPill->move(px - m_hourPill->width(), m_toolPill->y() - 8 - m_hourPill->height());
    if (m_cview->layout()) m_cview->layout()->invalidate();
    m_cview->adjustSize();
    m_cview->move(W - m_cview->width() - 16, H - m_cview->height() - 14);

    // o painel à esquerda empurra o que mora daquele lado; o catálogo sobe a dica
    const int panelRight = W > 760 && panelShown() && !(m_panelMini && !m_openDoc) ? m_side->geometry().right() + 9 : panelShown() && m_panelMini && !m_openDoc && W > 760 ? 424 : 16;
    m_rose->move(panelRight, H - m_rose->height() - 14);
    m_side->setGeometry(16, 76, m_openDoc ? 620 : 400, std::max(200, H - 92));
    placeDuo();
    placePeek();
    m_mini->adjustSize();
    m_mini->move(16, H - 16 - m_mini->height());

    const int left = cart ? panelRight + m_rose->width() + 12 : panelRight;
    const int right = (cart ? m_cview->x() : m_toolPill->x()) - 12;
    placeBuildBar();
    const int bottom = m_buildBar->isVisible() ? H - m_buildBar->y() + 10 : 16;
    const int room = std::max(200, right - left);
    const bool wrap = m_hint->fontMetrics().horizontalAdvance(m_hint->text()) + 28 > room;
    m_hint->setWordWrap(wrap);
    if (wrap) m_hint->setFixedWidth(room); else { m_hint->setMinimumWidth(0); m_hint->setMaximumWidth(QWIDGETSIZE_MAX); }
    m_hint->adjustSize();
    m_hint->move(left + (room - m_hint->width()) / 2, H - bottom - m_hint->height());

    // a escala sobe acima do Astrolábio
    m_scale->adjustSize();
    m_scale->move(W - 20 - m_scale->width(), cart ? m_cview->y() - 10 - m_scale->height() : H - 276 - m_scale->height());

    m_toast->adjustSize();
    m_toast->move(left + (room - m_toast->width()) / 2, H - bottom - m_hint->height() - 10 - m_toast->height());

    const QPointF vc = m_canvas->viewCenter();
    m_focusBar->adjustSize();
    m_focusBar->move(int(vc.x()) - m_focusBar->width() / 2, 14);
    placeDock();
    if (m_ring->isVisible()) m_ring->setGeometry(rect());
    if (m_caderno && m_caderno->isVisible()) { m_caderno->setGeometry(rect()); m_caderno->raise(); }
    for (QWidget* w : { static_cast<QWidget*>(m_cbar), static_cast<QWidget*>(m_settings), static_cast<QWidget*>(m_export) }) w->raise();
    syncHabCard();
    m_tip->raise();
    m_toast->raise();
}
void Panel::placeDock()
{
    if (!m_dock->isVisible()) return;
    m_dock->adjustSize();
    const int w = m_dock->width(), h = m_dock->height();
    const auto clampTo = [&](int& cx, int& cy) {
        cx = std::max(8, std::min(width() - w - 8, cx));
        cy = std::max(64, std::min(std::max(64, height() - h - 8), cy));
    };
    int x, y;
    if (m_dockUser) { x = m_dockAt.x(); y = m_dockAt.y(); clampTo(x, y); }
    else {
        const bool cart = m_ui == QLatin1String("cartela");
        const QRect corner = (cart ? m_cview->geometry() : m_astro->geometry().united(m_hourPill->geometry()).united(m_toolPill->geometry())).adjusted(-8, -8, 8, 8);
        QList<QPoint> spots;
        if (m_hasRingAt) spots = { { m_ringAt.x() + 158, m_ringAt.y() - h / 2 }, { m_ringAt.x() - 158 - w, m_ringAt.y() - h / 2 } };
        spots.append(cart ? QPoint(width() - w - 76, 64) : QPoint(16, 86));
        x = spots.first().x(); y = spots.first().y(); clampTo(x, y);
        for (const QPoint sp : spots) {
            int cx = sp.x(), cy = sp.y();
            clampTo(cx, cy);
            if (!QRect(cx, cy, w, h).intersects(corner)) { x = cx; y = cy; break; }
        }
    }
    m_dock->move(x, y);
    m_dock->raise();
}
void Panel::resizeEvent(QResizeEvent*) { positionOverlays(); syncUi(); }

// ── estado → interface ──
QString Panel::toolLabel() const
{
    if (m_tool == QLatin1String("realm")) return tr("Territórios") + QStringLiteral(" · ") + (m_canvas->realmMode == RealmMode::Paint ? tr("Pintar") : m_canvas->realmMode == RealmMode::Neutral ? tr("Neutro") : tr("Semear"));
    if (m_tool == QLatin1String("build")) return m_canvas->placing ? tr("Construir") + QStringLiteral(" · ") + m_canvas->placing->name : tr("Construir");
    if (m_tool == QLatin1String("explore")) return tr("Explorar");
    if (m_tool == QLatin1String("edit")) return tr("Editar");
    if (m_tool == QLatin1String("names")) return tr("Nomes");
    if (m_tool == QLatin1String("ruler")) return tr("Régua");
    if (m_tool == QLatin1String("caderno")) return tr("Caderno");
    return tr("Mover");
}
QString Panel::toolHint() const
{
    if (m_tool == QLatin1String("explore")) return tr("Explorar: clique num lugar pra ver e escrever sobre ele · a busca leva o mapa até lá · arraste pra mover · Esc fecha");
    if (m_tool == QLatin1String("edit")) return tr("Editar: clique num sistema, planeta ou lua · arraste pra mover · botão direito: ferramentas");
    if (m_tool == QLatin1String("realm")) return tr("Territórios: semear, pintar ou neutro (na doca) · Ctrl+Z desfaz · botão direito: ferramentas");
    if (m_tool == QLatin1String("names")) return tr("Nomes: clique num nome pra editar, arraste um nome solto pra mover · clique no mapa pra pôr um nome novo · Delete esconde");
    if (m_tool == QLatin1String("ruler")) return tr("Régua: clique pra marcar o caminho · duplo clique termina · Esc limpa");
    if (m_tool == QLatin1String("build")) {
        const PlaceItem* it = m_canvas->placing.get();
        if (!it) return tr("Construir: clique numa peça pra selecionar e arrastar · Q e E giram · [ e ] tamanho · Delete apaga · Ctrl+Z desfaz");
        if (it->kind == ItemKind::Star) return tr("Clique em qualquer lugar da galáxia pra pôr uma estrela nova · Esc solta a peça");
        const StructDef* d = it->kind == ItemKind::Struct ? structOf(it->id) : nullptr;
        if (it->kind == ItemKind::Moon || (d && d->host == Host::Planet)) return tr("Clique em cima de um planeta. Esc solta a peça.");
        if (d && d->deep) return tr("Clique dentro de um sistema pra pôr em órbita, ou no espaço aberto. Esc solta a peça.");
        return tr("Clique dentro de um sistema, na órbita que quiser. Esc solta a peça.");
    }
    return tr("Arraste pra mover · roda: zoom · botão direito: ferramentas (arrastado gira e inclina)");
}
void Panel::syncHint()
{
    m_hint->setText(m_canvas->placing && m_canvas->ghostMessage().size() ? m_canvas->ghostMessage() : toolHint());
    positionOverlays();
}
void Panel::syncTitle()
{
    if (!isOpen()) return;
    if (!m_titleEdit->isVisible()) m_title->setText(m_doc.labelOf(QStringLiteral("GAL")));
    m_sub->setText(QStringLiteral("%1 · %2 · %3").arg(viewName(m_canvas->view), sizeNameOf(m_doc.gal->size), tr("semente %1").arg(m_doc.gal->seed)));
}
void Panel::syncCview()
{
    if (!isOpen()) return;
    const double pitch = 90 - m_canvas->cam.tilt / DEG, t = std::fmod(m_doc.simT, TIME_LOOP);
    m_astro->setState(-m_canvas->cam.yaw, pitch, t / TIME_LOOP * 24, m_canvas->view);
    m_rose->setAngle(-m_canvas->cam.yaw);
    if (!m_cPitch->isSliderDown()) { m_cPitch->blockSignals(true); m_cPitch->setValue(qRound(pitch)); m_cPitch->blockSignals(false); }
    if (!m_cTime->isSliderDown()) { m_cTime->blockSignals(true); m_cTime->setValue(qRound(t)); m_cTime->blockSignals(false); }
    m_cPitchV->setText(QStringLiteral("%1°").arg(qRound(pitch)));
    m_cTimeV->setText(QStringLiteral("%1%").arg(qRound(t / TIME_LOOP * 100)));
    m_timeLabel->setText(tr("Ciclo %1 · %2").arg(nf(m_doc.simT / TIME_LOOP, 2), m_canvas->play ? tr("andando") : tr("parado")));
}
void Panel::syncScale()
{
    if (!isOpen()) return;
    const double z = m_canvas->cam.z, ly = m_doc.gal->ly / 2;
    QColor col = m_canvas->skin() ? m_canvas->skin()->inkC() : Theme::toColor(Theme::textMuted());
    static_cast<ScaleBar*>(m_scaleBar)->color = col;
    m_scaleText->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(col.name()));
    if (z / m_canvas->fitZ() > 25) {
        m_scaleText->setText(tr("Órbitas em esquema"));
        m_scaleBar->hide();
        m_scale->adjustSize();
        return;
    }
    const double target = 110 / z * ly, p = std::pow(10, std::floor(std::log10(target)));
    double v = p * 10;
    for (double m : { 1.0, 2.0, 5.0, 10.0 }) if (m * p >= target * 0.6) { v = m * p; break; }
    m_scaleBar->show();
    m_scaleBar->setFixedWidth(std::max(4, int(v / ly * z)));
    m_scaleText->setText(v >= 1000 ? tr("%1 mil anos-luz").arg(nf(v / 1000, 0)) : tr("%1 anos-luz").arg(nf(v, 0)));
    m_scale->adjustSize();
    m_scale->move(width() - 20 - m_scale->width(), m_scale->y());
}
void Panel::syncFocusBar()
{
    Region* r = nullptr;
    if (!m_canvas->focusKey.isEmpty()) for (const auto& q : m_doc.regions) if (q->key == m_canvas->focusKey) r = q.get();
    m_focusBar->setVisible(r != nullptr);
    if (!r) return;
    m_focusSwatch->setStyleSheet(QStringLiteral("background: %1; border-radius: 5px;").arg(r->color));
    m_focusName->setText(tr("%1 · %2 sistemas").arg(r->name).arg(r->systems.size()));
    m_focusBar->adjustSize();
    m_focusBar->move(int(m_canvas->viewCenter().x()) - m_focusBar->width() / 2, 14);
    m_focusBar->raise();
}
void Panel::syncTip(const QString& key, const QPointF& at)
{
    const Ref o = isOpen() ? m_doc.resolve(key) : Ref();
    if (!o) { m_tip->hide(); return; }
    m_tip->setText(QStringLiteral("%1&nbsp;&nbsp;<span style=\"color:%2\">%3</span>").arg(m_doc.labelOf(key).toHtmlEscaped(), Theme::textMuted(), subShort(key, o).toHtmlEscaped()));
    m_tip->adjustSize();
    m_tip->move(std::min(width() - m_tip->width() - 8, int(at.x()) + 14), std::min(height() - m_tip->height() - 8, int(at.y()) + 14));
    m_tip->show();
    m_tip->raise();
}
void Panel::syncUi()
{
    if (!m_canvas) return;
    syncTitle();
    syncCview();
    const QColor muted = Theme::toColor(Theme::textMuted()), a = Theme::toColor(Theme::accentDefault());
    const auto setCheck = [&](QToolButton* b, bool on) { b->setChecked(on); b->setIcon(carto::icon(on ? QStringLiteral("check") : QStringLiteral("box"), muted, a)); };
    setCheck(m_lyTerr, m_canvas->ly.terr);
    setCheck(m_lyNames, m_canvas->ly.names);
    setCheck(m_lyOrb, m_canvas->ly.orb);
    setCheck(m_lySprites, m_canvas->sprites);
    setCheck(m_lyFrame, m_canvas->frameOn);
    setCheck(m_lyPeek, m_peekOn);
    setCheck(m_lyTraffic, m_canvas->ly.traffic);
    setCheck(m_expTitle, m_expWithTitle);
    m_trafficLevel->setVisible(m_canvas->ly.traffic);
    for (auto it = m_skyBtns.begin(); it != m_skyBtns.end(); ++it) it.value()->setChecked(it.key() == m_sky);
    for (auto it = m_uiBtns.begin(); it != m_uiBtns.end(); ++it) it.value()->setChecked(it.key() == m_ui);
    for (auto it = m_expBtns.begin(); it != m_expBtns.end(); ++it) it.value()->setChecked(it.key().toInt() == int(m_expScale));
    m_expSize->setText(tr("%1 × %2 px").arg(nf(width() * m_expScale), nf(height() * m_expScale)));
    m_setBtn->setChecked(m_settings->isVisible());
    syncHabCard();
    m_exportBtn->setChecked(m_export->isVisible());
    m_rulerBtn->setChecked(m_tool == QLatin1String("ruler"));
    m_panBtn->setChecked(m_tool == QLatin1String("pan"));
    const bool cart = m_ui == QLatin1String("cartela");
    m_panBtn->setVisible(cart); m_cbarSep->setVisible(cart);
    m_cview->setVisible(cart); m_rose->setVisible(cart);
    m_astro->setVisible(!cart); m_hourPill->setVisible(!cart); m_toolPill->setVisible(!cart);
    for (auto it = m_viewBtns.begin(); it != m_viewBtns.end(); ++it) {
        const bool artBtn = it.key() == QLatin1String("art");
        it.value()->setChecked(artBtn ? m_canvas->skin() != nullptr : it.key() == m_canvas->view);
        it.value()->setText(artBtn ? carto::artModeShort(m_canvas->artMode) + QStringLiteral(" ▾") : viewName(it.key()));
    }
    const QString playIcon = m_canvas->play ? QStringLiteral("pause") : QStringLiteral("play");
    m_playBtn->setIcon(carto::icon(playIcon, muted, a));
    m_cPlay->setIcon(carto::icon(playIcon, muted, a));
    m_toolBtn->setText(toolLabel() + QStringLiteral("  ▴"));
    m_hint->setText(m_canvas->placing && m_canvas->ghostMessage().size() ? m_canvas->ghostMessage() : toolHint());
    positionOverlays();
}
void Panel::flash(const QString& text) { showToast(text, false); }
void Panel::showToast(const QString& text, bool withUndo)
{
    m_toastText->setText(text);
    m_toastUndo->setVisible(withUndo);
    m_toast->show();
    positionOverlays();
    m_toastTimer->start(withUndo ? 9000 : 2500);
}

// ── ferramentas e vista ──
void Panel::openRing(QPointF at, const QString& target)
{
    m_ringTarget = !target.isEmpty() && m_doc.resolve(target) ? target : QString();
    const QPoint c(std::max(156, std::min(width() - 156, int(at.x()))), std::max(156, std::min(height() - 156, int(at.y()))));
    m_ringAt = c;
    const QStringList parts = toolLabel().split(QStringLiteral(" · "));
    QString sub = !m_ringTarget.isEmpty() ? m_doc.labelOf(m_ringTarget) : parts.value(1);
    if (sub.size() > 14) sub = sub.left(13) + QStringLiteral("…");
    m_ring->openAt(c, m_tool, parts.value(0), sub);
    m_ring->setActiveExtra(m_canvas->ly.traffic ? QStringLiteral("traffic") : QString());
    m_tip->hide();
}
// escolher no anel: a ferramenta, e o lugar onde o clique direito caiu
void Panel::pickTool(const QString& id, const QString& target)
{
    if (id == QLatin1String("traffic")) { setTrafficOn(!m_canvas->ly.traffic); return; }
    m_hasRingAt = true; m_dockUser = false;
    setTool(id);
    m_hasRingAt = false;
    if (!target.isEmpty() && (id == QLatin1String("explore") || id == QLatin1String("edit"))) { m_canvas->select(target, true); openPanel(); }
}
void Panel::ensureView(const QString& id)
{
    if (id == QLatin1String("build") && m_canvas->view != QLatin1String("full")) setViewMode(QStringLiteral("full"));
    else if ((id == QLatin1String("realm") || id == QLatin1String("names") || id == QLatin1String("explore") || id == QLatin1String("edit")) && m_canvas->view == QLatin1String("terrain")) setViewMode(QStringLiteral("full"));
}
void Panel::setTool(const QString& id)
{
    closeDuo();
    m_linkFrom.clear(); m_peopleFor.clear(); m_canvas->pickingLink = false;
    const QString was = m_tool;
    m_tool = id;
    ensureView(id);
    m_tip->hide();
    m_canvas->tool = id == QLatin1String("realm") ? Tool::Realm : id == QLatin1String("build") ? Tool::Build : id == QLatin1String("names") ? Tool::Names : id == QLatin1String("explore") ? Tool::Explore
                    : id == QLatin1String("edit") ? Tool::Edit : id == QLatin1String("ruler") ? Tool::Ruler : id == QLatin1String("caderno") ? Tool::Caderno : Tool::Pan;
    if (id != QLatin1String("build")) m_canvas->armItem(nullptr);
    m_buildBar->setVisible(id == QLatin1String("build"));
    if (id == QLatin1String("build")) renderBuildBar();
    m_gear->hide();
    m_dock->setVisible(id == QLatin1String("realm"));
    if (id == QLatin1String("realm")) { if (!m_canvas->focusKey.isEmpty()) m_canvas->setFocus2(QString()); renderRealmDock(); placeDock(); }
    if (was == QLatin1String("ruler") && id != QLatin1String("ruler")) { m_canvas->rulerPts.clear(); m_canvas->rulerDone = false; }
    if (id != QLatin1String("names")) { m_canvas->nameSel.clear(); m_nameEdit->hide(); }
    if (id == QLatin1String("explore") || id == QLatin1String("edit")) { m_panelOpen = true; if (id == QLatin1String("edit")) m_tab = QStringLiteral("dados"); m_panelMini = false; }
    else { m_panelOpen = false; m_openDoc = nullptr; }
    m_canvas->unsetCursor();
    syncSelPill(true);
    renderPanel();
    syncUi();
    m_canvas->update();
}
void Panel::setViewMode(const QString& m)
{
    m_canvas->setView(m);
    m_artPick->hide();
    syncUi();
    scheduleSave();
}
void Panel::setPlay(bool on) { m_canvas->play = on; syncUi(); }
void Panel::setTrafficOn(bool on) { m_canvas->ly.traffic = on; m_canvas->docChanged(); syncUi(); }
void Panel::setSpritesOn(bool on) { m_canvas->sprites = on; m_canvas->docChanged(); syncUi(); }
void Panel::setSkyMode(const QString& v) { m_sky = v; m_canvas->skyClean = v == QLatin1String("limpo"); m_canvas->docChanged(); syncUi(); }
void Panel::setUiMode(const QString& v)
{
    m_ui = v;
    QSettings().setValue(QStringLiteral("carto/ui"), v);
    m_dockUser = false;
    syncUi();
}
void Panel::showMapMenu()
{
    QMenu menu(this);
    if (fillMapMenu) fillMapMenu(menu);
    menu.exec(m_menuBtn->mapToGlobal(QPoint(0, m_menuBtn->height() + 4)));
}
void Panel::renameGalaxy()
{
    m_title->hide();
    m_titleEdit->setText(m_title->text());
    m_titleEdit->show();
    m_titleEdit->setFocus();
    m_titleEdit->selectAll();
    positionOverlays();
}
void Panel::doExport()
{
    QString name = m_doc.labelOf(QStringLiteral("GAL"));
    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral(" "));
    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar mapa"), name.trimmed() + QStringLiteral(".png"), tr("Imagem PNG (*.png)"));
    if (path.isEmpty()) return;
    const QImage img = m_canvas->exportImage(m_expScale, m_expWithTitle);
    if (img.isNull() || !img.save(path, "PNG")) { flash(tr("Não deu pra salvar a imagem nesse lugar.")); return; }
    m_export->hide();
    flash(tr("Imagem salva: %1").arg(QFileInfo(path).fileName()));
    syncUi();
}
void Panel::openCaderno(const QString& key)
{
    m_caderno->open(key, m_tool);
    m_caderno->setGeometry(rect());
    m_caderno->show();
    m_caderno->raise();
    m_canvas->tool = Tool::Caderno;
    syncHabCard();
}
void Panel::closeCaderno()
{
    const QString back = m_caderno->returnTool();
    m_caderno->closeIt();
    m_caderno->hide();
    setTool(back);
    m_canvas->setFocus();
}

// ── teclado ──
bool Panel::handleKey(QKeyEvent* e)
{
    const int k = e->key();
    const bool ctrl = e->modifiers() & Qt::ControlModifier;
    QWidget* f = QApplication::focusWidget();
    // campo de texto: só o Esc interessa aqui
    if (qobject_cast<QLineEdit*>(f) || qobject_cast<QTextEdit*>(f) || qobject_cast<QComboBox*>(f)) {
        if (k == Qt::Key_Escape && f == m_query) { m_results->hide(); m_canvas->setFocus(); return true; }
        return false;
    }
    if (ctrl && k == Qt::Key_Z && !(e->modifiers() & Qt::ShiftModifier)) { m_canvas->undo(); return true; }
    if (ctrl && (k == Qt::Key_Y || (k == Qt::Key_Z && (e->modifiers() & Qt::ShiftModifier)))) { m_canvas->redo(); return true; }
    if (ctrl || (e->modifiers() & Qt::AltModifier)) return false;
    if (m_caderno && m_caderno->isVisible()) {
        if (k == Qt::Key_Escape) { if (!m_caderno->escape()) closeCaderno(); return true; }
        if (k == Qt::Key_C) { closeCaderno(); return true; }
        return false;
    }
    const auto toggle = [this](const QString& id) { m_dockUser = false; setTool(m_tool == id ? QStringLiteral("pan") : id); };
    switch (k) {
    case Qt::Key_H: setTool(QStringLiteral("pan")); break;
    case Qt::Key_T: toggle(QStringLiteral("realm")); break;
    case Qt::Key_K: toggle(QStringLiteral("build")); break;
    case Qt::Key_N: toggle(QStringLiteral("names")); break;
    case Qt::Key_X: toggle(QStringLiteral("explore")); break;
    case Qt::Key_C: openCaderno(m_canvas->sel); break;
    case Qt::Key_R: toggle(QStringLiteral("ruler")); break;
    case Qt::Key_L: setHabOn(!m_canvas->hab); break;
    case Qt::Key_S: setSpritesOn(!m_canvas->sprites); break;
    case Qt::Key_G: emit regenerateRequested(); break;
    case Qt::Key_F: m_canvas->fitMap(); break;
    case Qt::Key_P: setPlay(!m_canvas->play); break;
    case Qt::Key_V: setSkyMode(m_sky == QLatin1String("limpo") ? QStringLiteral("cheio") : QStringLiteral("limpo")); break;
    case Qt::Key_Q: if (!rotatePiece(-PI_D / 12)) m_canvas->setTilt(m_canvas->cam.tilt, m_canvas->cam.yaw + 15 * DEG); break;
    case Qt::Key_E: if (!rotatePiece(PI_D / 12)) m_canvas->setTilt(m_canvas->cam.tilt, m_canvas->cam.yaw - 15 * DEG); break;
    case Qt::Key_BracketLeft: scalePiece(1 / 1.1); break;
    case Qt::Key_BracketRight: scalePiece(1.1); break;
    case Qt::Key_Delete: case Qt::Key_Backspace: deleteSelected(); break;
    case Qt::Key_Escape: escapeOne(); break;
    default: return false;
    }
    return true;
}
void Panel::keyPressEvent(QKeyEvent* e) { if (!handleKey(e)) QWidget::keyPressEvent(e); }
// Esc desfaz uma coisa de cada vez: o que está aberto, a peça na mão, o selecionado e, no fim, a ferramenta.
bool Panel::escapeOne()
{
    if (m_ring->isVisible() || m_artPick->isVisible()) { m_ring->hide(); m_artPick->hide(); return true; }
    if (!m_duoLink.isEmpty()) { closeDuo(); return true; }
    if (!m_linkFrom.isEmpty() || !m_peopleFor.isEmpty()) { m_linkFrom.clear(); m_peopleFor.clear(); m_canvas->pickingLink = false; renderPanel(true); return true; }
    if (m_settings->isVisible() || m_export->isVisible() || m_gear->isVisible()) { m_settings->hide(); m_export->hide(); m_gear->hide(); syncUi(); return true; }
    if (m_openDoc) { docDropIfEmpty(m_openDoc); m_openDoc = nullptr; renderPanel(); return true; }
    if (m_canvas->placing) { m_canvas->armItem(nullptr); renderBuildBar(); syncUi(); return true; }
    if (m_tool == QLatin1String("ruler") && !m_canvas->rulerPts.empty()) { m_canvas->rulerPts.clear(); m_canvas->rulerDone = false; m_canvas->update(); return true; }
    if (m_tool == QLatin1String("build") && m_canvas->removable(m_canvas->sel)) { m_canvas->select(m_doc.parentOf(m_canvas->sel), false); return true; }
    if ((m_tool == QLatin1String("explore") || m_tool == QLatin1String("edit")) && m_canvas->sel != QLatin1String("GAL")) { const QString p = m_doc.parentOf(m_canvas->sel); if (!p.isEmpty()) { m_canvas->select(p, true); return true; } }
    if (m_tool != QLatin1String("pan")) { setTool(QStringLiteral("pan")); return true; }
    if (m_canvas->habOnly) { m_canvas->setHabOnly(0); renderHabCard(); return true; }
    if (m_canvas->hab) { setHabOn(false); return true; }
    return false;
}
void Panel::deleteSelected()
{
    if (m_tool == QLatin1String("names")) { if (!m_canvas->nameSel.isEmpty()) deleteName(m_canvas->nameSel); return; }
    if ((m_tool == QLatin1String("build") || m_tool == QLatin1String("edit")) && m_canvas->removable(m_canvas->sel)) m_canvas->removeThing(m_canvas->sel);
}
bool Panel::rotatePiece(double d)
{
    if (m_tool != QLatin1String("build")) return false;
    const Ref o = m_doc.resolve(m_canvas->sel);
    if (o.kind != RefKind::Build) return false;
    o.build->spinU += d;
    m_canvas->update();
    scheduleSave();
    return true;
}
void Panel::scalePiece(double k)
{
    if (m_tool != QLatin1String("build")) return;
    const Ref o = m_doc.resolve(m_canvas->sel);
    if (o.kind != RefKind::Build) return;
    o.build->scale = std::max(0.4, std::min(4.0, o.build->scale * k));
    syncSelPill(true);
    m_canvas->update();
    scheduleSave();
}

bool Panel::eventFilter(QObject* o, QEvent* e)
{
    if (o == m_title && e->type() == QEvent::MouseButtonPress) { renameGalaxy(); return true; }
    // a roda do mouse anda a fileira do catálogo pro lado
    if (m_cardScroll && o == m_cardScroll->viewport() && e->type() == QEvent::Wheel) {
        auto* we = static_cast<QWheelEvent*>(e);
        const int d = we->angleDelta().y() ? we->angleDelta().y() : we->angleDelta().x();
        m_cardScroll->horizontalScrollBar()->setValue(m_cardScroll->horizontalScrollBar()->value() - d);
        return true;
    }
    if (o == m_selSize && e->type() == QEvent::MouseButtonDblClick) { m_selSize->setValue(100); return true; }
    if (o == m_nameEdit && e->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape) { m_nameFn = nullptr; m_nameEdit->hide(); m_canvas->setFocus(); return true; }
    if (o == m_query && e->type() == QEvent::KeyPress) {
        const int k = static_cast<QKeyEvent*>(e)->key();
        if ((k == Qt::Key_Down || k == Qt::Key_Up) && !m_resList.isEmpty()) {
            m_resIdx = (m_resIdx + (k == Qt::Key_Down ? 1 : -1) + m_resList.size()) % m_resList.size();
            const auto rows = m_results->findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly);
            int i = 0;
            for (QPushButton* b : rows) { if (b->isHidden()) continue; b->setProperty("on", i == m_resIdx); b->style()->unpolish(b); b->style()->polish(b); i++; }
            return true;
        }
    }
    // a linha da lista de territórios: clicar escolhe
    if (e->type() == QEvent::MouseButtonPress && o->property("gmRegion").isValid()) {
        m_canvas->realmActive = o->property("gmRegion").toString();
        QTimer::singleShot(0, this, [this]() { renderRealmDock(); });
        m_canvas->update();
    }
    if (o == m_dockTitle) {
        if (e->type() == QEvent::MouseButtonPress) { m_dockDrag = true; m_dockGrab = static_cast<QMouseEvent*>(e)->globalPosition().toPoint() - m_dock->pos(); return true; }
        if (e->type() == QEvent::MouseMove && m_dockDrag) { m_dockUser = true; m_dockAt = static_cast<QMouseEvent*>(e)->globalPosition().toPoint() - m_dockGrab; placeDock(); return true; }
        if (e->type() == QEvent::MouseButtonRelease) { m_dockDrag = false; return true; }
    }
    return QWidget::eventFilter(o, e);
}

} // namespace galaxy
