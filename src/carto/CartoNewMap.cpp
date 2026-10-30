#include "CartoNewMap.h"

#include "CartoIcons.h"
#include "CartoPaint.h"
#include "CartoStore.h"
#include "../Theme.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QThreadPool>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace carto {

namespace {
const char* const kTypes[4] = { "continent", "archipelago", "oceanic", "empty" };
const char* const kClimates[4] = { "mixed", "cold", "arid", "green" };
const QColor kSea(0x1a, 0x3d, 0x58);
const QColor kParchment(0xf4, 0xec, 0xd8);
const QColor kHalo(10, 24, 36, 215);

int indexOf(const char* const* list, int n, const QString& v)
{
    for (int i = 0; i < n; i++) if (v == QLatin1String(list[i])) return i;
    return 0;
}
QString sizeLabel(int s)
{
    switch (s) {
    case 0: return NewMap::tr("Ilha");
    case 1: return NewMap::tr("Região");
    case 2: return NewMap::tr("Mundo");
    case 3: return NewMap::tr("Épico");
    case 4: return NewMap::tr("Continente");
    default: return NewMap::tr("Supercontinente");
    }
}
QSize worldPx(int s)
{
    const SizePreset& sp = sizePreset(s);
    return { (sp.gw - 1) * 5, (sp.gh - 1) * 5 };
}
QColor themeColor(const QString& c) { return Theme::toColor(c); }

// o mundo visto de cima, recortado na borda do mundo (sem a folga de mar em volta)
QImage topDown(World& w, Style style, int targetW, int threads, const std::atomic<bool>* cancel)
{
    View v;
    v.pitch = 90;
    v.style = style;
    v.step = std::max(1, static_cast<int>(std::lround(double(w.W) / std::max(32, targetW))));
    Geom g = viewGeom(w, v);
    const auto rc = worldRectToView(g, 0, 0, w.W - 1, w.H - 1);
    const int x0 = std::min(rc[0] + 2, rc[2] - 1), y0 = std::min(rc[1] + 2, rc[3] - 1);
    const int x1 = std::max(x0 + 1, rc[2] - 3), y1 = std::max(y0 + 1, rc[3] - 3);
    std::vector<uint8_t> px(static_cast<size_t>(g.W) * g.H * 4, 0);
    g.cancel = cancel;
    paintBands(w, g, px.data(), x0, y0, x1, y1, nullptr, nullptr, threads);
    if (cancel && cancel->load()) return QImage();
    const QImage full(px.data(), g.W, g.H, g.W * 4, QImage::Format_RGBA8888);
    return full.copy(x0, y0, x1 - x0, y1 - y0);
}
}

// ── a prévia: o mundo de cima, com o nome por cima e o aviso de mar vazio ──
class NewMapPreview : public QWidget {
public:
    explicit NewMapPreview(QWidget* p) : QWidget(p) {}
    void setImage(const QImage& img) { m_img = img; update(); }
    void setBusy(bool b) { if (b != m_busy) { m_busy = b; update(); } }
    void setName(const QString& n) { if (n != m_name) { m_name = n; update(); } }
    void setEmpty(bool e) { if (e != m_empty) { m_empty = e; update(); } }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath clip;
        clip.addRoundedRect(r, 10, 10);
        p.save();
        p.setClipPath(clip);
        p.fillRect(rect(), kSea);
        if (!m_img.isNull()) {
            // cobre a caixa inteira mantendo a proporção (todos os tamanhos são 4:3)
            const QSizeF s = QSizeF(m_img.size()).scaled(r.size(), Qt::KeepAspectRatioByExpanding);
            p.drawImage(QRectF(r.center().x() - s.width() / 2, r.center().y() - s.height() / 2, s.width(), s.height()), m_img);
        }
        if (m_empty) {
            QFont f(QStringLiteral("IM FELL English SC"));
            f.setPixelSize(22);
            p.setFont(f);
            p.setPen(kParchment);
            const QRectF mid(r.left() + 20, r.center().y() - 30, r.width() - 40, 30);
            p.drawText(mid, Qt::AlignHCenter | Qt::AlignBottom, NewMap::tr("Mar vazio"));
            QFont g = font();
            g.setPixelSize(13);
            p.setFont(g);
            QColor c = kParchment; c.setAlphaF(0.85);
            p.setPen(c);
            p.drawText(QRectF(mid.left(), mid.bottom() + 4, mid.width(), 22), Qt::AlignHCenter | Qt::AlignTop,
                       NewMap::tr("Você desenha a terra com os pincéis."));
        }
        if (!m_name.isEmpty()) {
            QFont f(QStringLiteral("IM FELL English SC"));
            f.setPixelSize(24);
            const QFontMetricsF fm(f);
            const QString text = fm.elidedText(m_name, Qt::ElideRight, r.width() - 28);
            QPainterPath path;
            path.addText(r.left() + 14, r.top() + 10 + fm.ascent(), f, text);
            p.strokePath(path, QPen(kHalo, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.fillPath(path, kParchment);
        }
        if (m_busy) {
            QFont g = font();
            g.setPixelSize(11);
            p.setFont(g);
            const QString t = NewMap::tr("Gerando…");
            const qreal w = QFontMetricsF(g).horizontalAdvance(t) + 20;
            const QRectF pill(r.right() - w - 10, r.bottom() - 30, w, 22);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(10, 24, 36, 170));
            p.drawRoundedRect(pill, 11, 11);
            p.setPen(kParchment);
            p.drawText(pill, Qt::AlignCenter, t);
        }
        p.restore();
        p.setPen(QPen(themeColor(Theme::panelBorder()), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, 10, 10);
    }

private:
    QImage m_img;
    QString m_name;
    bool m_busy = false, m_empty = false;
};

// ── os botões de escolha: tamanho (retângulo na proporção), estilo (miniatura) e bioma (faixa de cor) ──
class NewMapTile : public QAbstractButton {
public:
    enum Kind { Size, Thumb, Swatch };
    NewMapTile(Kind k, const QString& text, QWidget* p) : QAbstractButton(p), m_kind(k)
    {
        setText(text);
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
        setAttribute(Qt::WA_Hover);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumHeight(sizeHint().height());   // tela baixa aperta os respiros, nunca o botão
    }
    QString sub;
    double scale = 1;               // Size: o tamanho relativo do retângulo (0–1)
    QImage thumb;                   // Thumb
    QList<QColor> stops;            // Swatch

    QSize sizeHint() const override { return { 90, m_kind == Size ? 66 : m_kind == Swatch ? 56 : 92 }; }
    static int thumbHeight(int w) { return qRound((w - 12) * 0.75) + 32; }   // a miniatura é 4:3 + o nome

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QColor accent = themeColor(Theme::accentDefault()), border = themeColor(Theme::panelBorder());
        const QColor muted = themeColor(Theme::textMuted()), ink = themeColor(Theme::textPrimary());
        const bool on = isChecked(), hot = underMouse() || hasFocus();
        QColor bg = on ? accent : themeColor(Theme::hoverOverlay());
        if (on) bg.setAlphaF(0.16);
        QColor edge = on ? accent : hot ? QColor::fromRgbF((accent.redF() + border.redF()) / 2, (accent.greenF() + border.greenF()) / 2,
                                                          (accent.blueF() + border.blueF()) / 2) : border;
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setPen(QPen(edge, 1));
        p.setBrush(bg);
        p.drawRoundedRect(r, 9, 9);
        const QColor fg = on ? accent : hot ? ink : muted;

        QFont nameFont = font();
        nameFont.setPixelSize(12);
        nameFont.setBold(on);
        QFont subFont = font();
        subFont.setPixelSize(10);
        qreal y = r.top() + 6;
        if (m_kind == Size) {
            // o retângulo cresce com a área do mapa; fica no pé da faixa de cima pra todos alinharem
            const qreal bw = 9 + scale * 27, bh = bw * 0.75, band = 26;
            const QRectF box(r.center().x() - bw / 2, y + band - bh, bw, bh);
            QColor line = fg; line.setAlphaF(on ? 1.0 : 0.75);
            p.setPen(QPen(line, 1.5));
            QColor fill = accent; fill.setAlphaF(on ? 0.2 : 0.0);
            p.setBrush(fill);
            p.drawRoundedRect(box, 2, 2);
            y += band + 3;
        } else if (m_kind == Thumb) {
            const QRectF box(r.left() + 6, y, r.width() - 12, (r.width() - 12) * 0.75);
            QPainterPath clip;
            clip.addRoundedRect(box, 6, 6);
            p.save();
            p.setClipPath(clip);
            p.fillRect(box, kSea);
            if (!thumb.isNull()) {
                const QSizeF s = QSizeF(thumb.size()).scaled(box.size(), Qt::KeepAspectRatioByExpanding);
                p.drawImage(QRectF(box.center().x() - s.width() / 2, box.center().y() - s.height() / 2, s.width(), s.height()), thumb);
            }
            p.restore();
            y = box.bottom() + 4;
        } else {
            const QRectF bar(r.left() + 7, y + 1, r.width() - 14, 20);
            QLinearGradient grad(bar.topLeft(), bar.topRight());
            for (int i = 0; i < stops.size(); i++) grad.setColorAt(stops.size() > 1 ? double(i) / (stops.size() - 1) : 0, stops[i]);
            p.setPen(Qt::NoPen);
            p.setBrush(grad);
            p.drawRoundedRect(bar, 5, 5);
            y = bar.bottom() + 5;
        }
        p.setFont(nameFont);
        p.setPen(fg);
        const QFontMetricsF nfm(nameFont);
        p.drawText(QRectF(r.left() + 3, y, r.width() - 6, nfm.height()), Qt::AlignHCenter | Qt::AlignTop,
                   nfm.elidedText(text(), Qt::ElideRight, r.width() - 6));
        if (!sub.isEmpty()) {
            p.setFont(subFont);
            QColor sc = on ? accent : muted;
            if (on) sc.setAlphaF(0.85);
            p.setPen(sc);
            p.drawText(QRectF(r.left() + 3, y + nfm.height(), r.width() - 6, 14), Qt::AlignHCenter | Qt::AlignTop, sub);
        }
    }

private:
    Kind m_kind;
};

// ── a janela ──
NewMap::NewMap(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("cartoNewMap"));
    setAttribute(Qt::WA_NoSystemBackground);
    m_pool = new QThreadPool(this);
    m_pool->setMaxThreadCount(2);
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(90);
    connect(m_debounce, &QTimer::timeout, this, [this]() {
        startPreview();
        if (m_thumbsDirty) startThumbs();
    });
    build();
    hide();
}

NewMap::~NewMap()
{
    if (m_cancel) m_cancel->store(true);
    if (m_thumbCancel) m_thumbCancel->store(true);
    m_pool->waitForDone();
}

void NewMap::build()
{
    m_card = new QFrame(this);
    m_card->setObjectName(QStringLiteral("cartoCard"));
    auto* outer = new QVBoxLayout(m_card);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // cabeçalho
    auto* head = new QHBoxLayout();
    head->setContentsMargins(22, 18, 16, 0);
    auto* titles = new QVBoxLayout();
    titles->setSpacing(2);
    m_title = new QLabel(m_card);
    m_title->setObjectName(QStringLiteral("cartoCardTitle"));
    m_title->setStyleSheet(QStringLiteral("font-size: 22px;"));
    m_sub = new QLabel(m_card);
    m_sub->setObjectName(QStringLiteral("cartoNote"));
    m_sub->setStyleSheet(QStringLiteral("font-size: 12px;"));
    titles->addWidget(m_title);
    titles->addWidget(m_sub);
    head->addLayout(titles, 1);
    m_close = new QToolButton(m_card);
    m_close->setObjectName(QStringLiteral("cartoBtn"));
    m_close->setFixedSize(30, 30);
    m_close->setCursor(Qt::PointingHandCursor);
    m_close->setToolTip(tr("Fechar"));
    connect(m_close, &QToolButton::clicked, this, &NewMap::dismiss);
    head->addWidget(m_close, 0, Qt::AlignTop);
    outer->addLayout(head);

    auto caption = [](QWidget* parent, const QString& text) {
        auto* c = new QLabel(text, parent);
        c->setObjectName(QStringLiteral("cartoCaption"));
        return c;
    };
    auto hint = [](QWidget* parent) {
        auto* h = new QLabel(parent);
        h->setObjectName(QStringLiteral("cartoNote"));
        h->setWordWrap(true);
        return h;
    };

    // corpo: prévia e bioma à esquerda, o resto à direita
    auto* body = new QHBoxLayout();
    body->setContentsMargins(22, 16, 22, 18);
    body->setSpacing(22);
    m_left = new QWidget(m_card);
    auto* left = new QVBoxLayout(m_left);
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(7);
    m_preview = new NewMapPreview(m_left);
    left->addWidget(m_preview);
    auto* meta = new QHBoxLayout();
    m_metaSize = hint(m_left);
    m_metaSize->setWordWrap(false);
    m_metaSeed = hint(m_left);
    m_metaSeed->setWordWrap(false);
    meta->addWidget(m_metaSize);
    meta->addStretch();
    meta->addWidget(m_metaSeed);
    left->addLayout(meta);
    left->addSpacing(8);
    left->addWidget(caption(m_left, tr("BIOMA DOMINANTE")));
    auto* crow = new QHBoxLayout();
    crow->setSpacing(6);
    m_climates = new QButtonGroup(this);
    const QString cNames[4] = { tr("Misto"), tr("Frio"), tr("Árido"), tr("Verde") };
    const QList<QColor> cStops[4] = {
        { QColor("#466e40"), QColor("#7e9c5a"), QColor("#bfb26c"), QColor("#dcc48c"), QColor("#a3a184"), QColor("#eef1f0") },
        { QColor("#5c7458"), QColor("#a09e80"), QColor("#c9ccc4"), QColor("#eef1f0") },
        { QColor("#e2c68e"), QColor("#d4b47a"), QColor("#bfa06a"), QColor("#6f8a4c"), QColor("#e2c68e") },
        { QColor("#2f5d3a"), QColor("#3f6e44"), QColor("#58834a"), QColor("#53664c"), QColor("#7e9c5a") },
    };
    for (int i = 0; i < 4; i++) {
        auto* t = new NewMapTile(NewMapTile::Swatch, cNames[i], m_left);
        t->stops = cStops[i];
        m_climates->addButton(t, i);
        crow->addWidget(t);
    }
    left->addLayout(crow);
    m_climateHint = hint(m_left);
    left->addWidget(m_climateHint);
    left->addStretch();
    body->addWidget(m_left, 1);

    m_right = new QWidget(m_card);
    auto* right = new QVBoxLayout(m_right);
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(7);
    right->addWidget(caption(m_right, tr("NOME DO MAPA")));
    m_name = new QLineEdit(m_right);
    m_name->setObjectName(QStringLiteral("cartoField"));
    m_name->setMaxLength(80);
    connect(m_name, &QLineEdit::textChanged, this, [this]() { m_preview->setName(m_name->text().trimmed()); });
    right->addWidget(m_name);
    m_nameHint = hint(m_right);
    m_nameHint->setText(tr("Em branco, o mapa fica com o nome sugerido."));
    right->addWidget(m_nameHint);
    auto gap = [this](QVBoxLayout* l) {
        auto* sp = new QSpacerItem(0, 9, QSizePolicy::Minimum, QSizePolicy::Fixed);
        l->addSpacerItem(sp);
        m_gaps.append(sp);
    };
    gap(right);

    right->addWidget(caption(m_right, tr("TAMANHO")));
    auto* sgrid = new QGridLayout();
    sgrid->setSpacing(6);
    m_sizes = new QButtonGroup(this);
    for (int s = 0; s < 6; s++) {
        const QSize px = worldPx(s);
        auto* t = new NewMapTile(NewMapTile::Size, sizeLabel(s), m_right);
        t->sub = QStringLiteral("%1×%2").arg(px.width()).arg(px.height());
        t->scale = std::sqrt(double(sizePreset(s).gw) / sizePreset(5).gw);
        if (s == 4) t->setToolTip(tr("2× o Épico"));
        if (s == 5) t->setToolTip(tr("3× o Épico"));
        m_sizes->addButton(t, s);
        sgrid->addWidget(t, s / 3, s % 3);
    }
    right->addLayout(sgrid);
    m_sizeHint = hint(m_right);
    m_sizeHint->setText(tr("Quanto maior, mais espaço pra desenhar e mais pesado fica."));
    right->addWidget(m_sizeHint);
    gap(right);

    right->addWidget(caption(m_right, tr("ESTILO")));
    auto* trow = new QHBoxLayout();
    trow->setSpacing(6);
    m_types = new QButtonGroup(this);
    const QString tNames[4] = { tr("Continente"), tr("Arquipélago"), tr("Oceânico"), tr("Vazio") };
    for (int i = 0; i < 4; i++) {
        auto* t = new NewMapTile(NewMapTile::Thumb, tNames[i], m_right);
        m_types->addButton(t, i);
        m_typeTiles.append(t);
        trow->addWidget(t);
    }
    right->addLayout(trow);
    m_typeHint = hint(m_right);
    right->addWidget(m_typeHint);
    gap(right);

    auto* wrow = new QHBoxLayout();
    m_waterCap = caption(m_right, tr("OCEANO"));
    m_waterValue = new QLabel(m_right);
    m_waterValue->setObjectName(QStringLiteral("cartoValue"));
    wrow->addWidget(m_waterCap);
    wrow->addStretch();
    wrow->addWidget(m_waterValue);
    right->addLayout(wrow);
    m_water = new QSlider(Qt::Horizontal, m_right);
    m_water->setRange(20, 85);
    m_water->setToolTip(tr("Quanto do mapa é mar"));
    right->addWidget(m_water);
    right->addStretch();
    m_right->setFixedWidth(380);
    body->addWidget(m_right, 0);
    outer->addLayout(body);

    auto* sep = new QFrame(m_card);
    sep->setObjectName(QStringLiteral("cartoSep"));
    sep->setFixedHeight(1);
    outer->addWidget(sep);

    // rodapé: semente e Gerar, como no painel de antes
    auto* foot = new QHBoxLayout();
    foot->setContentsMargins(22, 14, 22, 16);
    foot->setSpacing(8);
    foot->addWidget(caption(m_card, tr("SEMENTE")));
    m_seed = new QSpinBox(m_card);
    m_seed->setObjectName(QStringLiteral("cartoSpin"));
    m_seed->setRange(0, 99999);
    m_seed->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_seed->setFixedWidth(88);
    foot->addWidget(m_seed);
    m_dice = new QToolButton(m_card);
    m_dice->setObjectName(QStringLiteral("cartoBtn"));
    m_dice->setFixedSize(34, 32);
    m_dice->setToolTip(tr("Semente aleatória"));
    m_dice->setCursor(Qt::PointingHandCursor);
    connect(m_dice, &QToolButton::clicked, this, [this]() { m_seed->setValue(QRandomGenerator::global()->bounded(99999)); });
    foot->addWidget(m_dice);
    foot->addSpacing(10);
    m_info = new QToolButton(m_card);
    m_info->setObjectName(QStringLiteral("cartoCheck"));
    m_info->setCheckable(true);
    m_info->setCursor(Qt::PointingHandCursor);
    m_info->setText(tr("Gerar territórios e locais"));
    m_info->setToolTip(tr("Reinos, cidades, estradas e nomes sugeridos pelo terreno. Desmarcado, o mapa vem só com o relevo e o resto é com você."));
    m_info->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_info, &QToolButton::clicked, this, [this](bool on) { QSettings().setValue(QStringLiteral("carto/genInfo"), on); sync(); });
    foot->addWidget(m_info);
    foot->addStretch();
    m_go = new QPushButton(m_card);
    m_go->setObjectName(QStringLiteral("cartoGo"));
    m_go->setCursor(Qt::PointingHandCursor);
    m_go->setStyleSheet(QStringLiteral("padding: 9px 24px;"));
    m_go->setMinimumWidth(150);
    connect(m_go, &QPushButton::clicked, this, &NewMap::accept);
    foot->addWidget(m_go);
    outer->addLayout(foot);

    // cada escolha grava o padrão da próxima vez e refaz a prévia
    connect(m_sizes, &QButtonGroup::idClicked, this, [this](int id) { if (!m_loading) QSettings().setValue(QStringLiteral("carto/newSize"), id); changed(true); });
    connect(m_types, &QButtonGroup::idClicked, this, [this](int id) { if (!m_loading) QSettings().setValue(QStringLiteral("carto/newType"), QString::fromLatin1(kTypes[id])); changed(false); });
    connect(m_climates, &QButtonGroup::idClicked, this, [this](int id) { if (!m_loading) QSettings().setValue(QStringLiteral("carto/newClimate"), QString::fromLatin1(kClimates[id])); changed(true); });
    connect(m_water, &QSlider::valueChanged, this, [this](int v) { if (!m_loading) QSettings().setValue(QStringLiteral("carto/newWater"), v); changed(true); });
    connect(m_seed, &QSpinBox::valueChanged, this, [this]() { changed(true); });
}

void NewMap::applyTheme()
{
    const QColor muted = themeColor(Theme::textMuted()), accent = themeColor(Theme::accentDefault());
    m_close->setIcon(icon(QStringLiteral("close"), muted, accent, 16));
    m_dice->setIcon(icon(QStringLiteral("dice"), muted, accent));
    // apagado (Vazio): o oceano não vale, a barra perde a cor
    m_water->setStyleSheet(Theme::qss(QStringLiteral(
        "QSlider::sub-page:horizontal:disabled { background: %1; border-radius: 2px; }"
        "QSlider::handle:horizontal:disabled { border-color: %1; }").arg(Theme::panelBorder())));
    m_waterValue->setStyleSheet(QStringLiteral("QLabel:disabled { color: %1; }").arg(Theme::disabledText()));
    sync();
    update();
}

void NewMap::open(Mode mode, const World* current, const QString& currentName, const QString& terrainStyle)
{
    m_mode = mode;
    m_style = terrainStyle;
    m_loading = true;
    QSettings st;
    int size = st.value(QStringLiteral("carto/newSize"), 1).toInt();
    QString type = st.value(QStringLiteral("carto/newType"), QStringLiteral("continent")).toString();
    QString climate = st.value(QStringLiteral("carto/newClimate"), QStringLiteral("mixed")).toString();
    int water = st.value(QStringLiteral("carto/newWater"), 60).toInt();
    int seed = QRandomGenerator::global()->bounded(99999);
    QString name;
    if (mode == Mode::Regen && current) {
        // parte do mapa aberto: com a mesma semente a prévia é o próprio mapa, e dá pra mexer só no que quiser
        size = current->size;
        type = current->type;
        climate = current->climate;
        if (type != QLatin1String("empty")) water = current->water;
        seed = current->seed;
        if (currentName != mapTitle(*current)) name = currentName;   // nome sugerido não vem: ele acompanha a semente
    }
    m_sizes->button(std::clamp(size, 0, 5))->setChecked(true);
    m_types->button(indexOf(kTypes, 4, type))->setChecked(true);
    m_climates->button(indexOf(kClimates, 4, climate))->setChecked(true);
    m_water->setValue(std::clamp(water, 20, 85));
    m_seed->setValue(seed);
    m_info->setChecked(st.value(QStringLiteral("carto/genInfo"), true).toBool());
    m_name->setText(name);
    m_title->setText(mode == Mode::Regen ? tr("Regenerar mapa") : tr("Novo mapa"));
    m_sub->setText(mode == Mode::Regen ? tr("O mapa atual é substituído. Ctrl+Z desfaz.")
                                       : tr("Escolha como o seu mundo começa. Depois, dá pra redesenhar tudo com os pincéis."));
    m_loading = false;
    m_ready.reset();
    m_readyKey.clear();
    m_preview->setImage(QImage());
    m_thumbsDirty = true;
    sync();
    setGeometry(parentWidget()->rect());
    show();
    raise();
    relayout();
    m_debounce->stop();
    startPreview();
    startThumbs();
    m_name->setFocus();
    if (!name.isEmpty()) m_name->selectAll();
}

void NewMap::dismiss()
{
    if (!isVisible()) return;
    if (m_cancel) m_cancel->store(true);
    if (m_thumbCancel) m_thumbCancel->store(true);
    m_debounce->stop();
    hide();
    m_ready.reset();
    if (dismissed) dismissed();
}

QString NewMap::key() const
{
    const int t = std::max(0, m_types->checkedId());
    return QStringLiteral("%1|%2|%3|%4|%5").arg(m_seed->value()).arg(m_sizes->checkedId())
        .arg(t == 3 ? 0 : m_water->value()).arg(QLatin1String(kTypes[t]), QLatin1String(kClimates[std::max(0, m_climates->checkedId())]));
}

NewMapChoice NewMap::choice() const
{
    NewMapChoice c;
    c.name = m_name->text().trimmed();
    c.seed = m_seed->value();
    c.size = std::max(0, m_sizes->checkedId());
    c.water = m_water->value();
    c.type = QString::fromLatin1(kTypes[std::max(0, m_types->checkedId())]);
    c.climate = QString::fromLatin1(kClimates[std::max(0, m_climates->checkedId())]);
    c.info = m_info->isChecked();
    return c;
}

void NewMap::sync()
{
    const int t = std::max(0, m_types->checkedId()), c = std::max(0, m_climates->checkedId()), s = std::max(0, m_sizes->checkedId());
    const bool empty = t == 3;
    static const char* typeTips[4] = {
        QT_TR_NOOP("Uma terra grande cercada de mar."), QT_TR_NOOP("Muitas ilhas, grandes e pequenas, espalhadas."),
        QT_TR_NOOP("Mais mar do que terra, com ilhas soltas."), QT_TR_NOOP("Só oceano: desenhe o mundo com os pincéis."),
    };
    static const char* climateTips[4] = {
        QT_TR_NOOP("Sorteia a latitude, se o mundo é mais frio ou mais quente, e manchas de clima maiores."),
        QT_TR_NOOP("Perto do polo, cheio de gelo e tundra. No Mundo e acima, era do gelo."),
        QT_TR_NOOP("Cinturão de desertos, com oásis onde sobra umidade."),
        QT_TR_NOOP("Úmido: floresta e pântano."),
    };
    m_typeHint->setText(tr(typeTips[t]));
    m_climateHint->setText(tr(climateTips[c]));
    m_water->setEnabled(!empty);
    m_waterCap->setEnabled(!empty);
    m_waterValue->setEnabled(!empty);
    m_waterValue->setText(QStringLiteral("%1%").arg(m_water->value()));
    m_info->setEnabled(!empty);
    m_info->setIcon(icon(m_info->isChecked() && !empty ? QStringLiteral("check") : QStringLiteral("box"),
                         themeColor(Theme::textMuted()), themeColor(Theme::accentDefault())));
    m_go->setText(empty ? tr("Criar mar vazio") : tr("Gerar mapa"));
    m_preview->setEmpty(empty);
    const QSize px = worldPx(s);
    m_metaSize->setText(QStringLiteral("%1 · %2×%3").arg(sizeLabel(s)).arg(px.width()).arg(px.height()));
    m_metaSeed->setText(tr("semente %1").arg(m_seed->value()));
    World probe;
    probe.seed = m_seed->value();
    m_name->setPlaceholderText(mapTitle(probe));   // o nome que o mapa leva se ficar em branco
}

void NewMap::changed(bool thumbs)
{
    if (m_loading) return;
    sync();
    if (thumbs) m_thumbsDirty = true;
    m_preview->setBusy(true);
    m_debounce->start();
}

void NewMap::startPreview()
{
    if (m_cancel) m_cancel->store(true);
    const NewMapChoice c = choice();
    const QString k = key();
    if (m_ready && k == m_readyKey) { m_preview->setBusy(false); return; }
    auto cancel = std::make_shared<std::atomic<bool>>(false);
    m_cancel = cancel;
    const int gen = ++m_gen;
    const Style style = m_style == QLatin1String("mira") ? Style::Mira : Style::Rich;
    const int target = std::min(720, qRound(m_preview->width() * m_preview->devicePixelRatioF()));
    m_preview->setBusy(true);
    QPointer<NewMap> self(this);
    m_pool->start([self, gen, c, k, style, target, cancel]() {
        auto w = std::make_shared<World>(rebuildBase(c.seed, c.water, c.type, c.size, c.climate));
        if (cancel->load()) return;
        // o pintor só guarda caches preguiçosos no mundo (o mapa ia calcular os mesmos)
        const QImage img = topDown(*w, style, target, 0, cancel.get());
        if (cancel->load() || !self) return;
        QMetaObject::invokeMethod(self, [self, gen, w, img, k]() {
            if (!self || gen != self->m_gen) return;
            self->m_ready = w;
            self->m_readyKey = k;
            self->m_preview->setImage(img);
            self->m_preview->setBusy(false);
        }, Qt::QueuedConnection);
    });
}

void NewMap::startThumbs()
{
    m_thumbsDirty = false;
    if (m_thumbCancel) m_thumbCancel->store(true);
    auto cancel = std::make_shared<std::atomic<bool>>(false);
    m_thumbCancel = cancel;
    const int gen = ++m_thumbGen;
    NewMapChoice c = choice();
    c.size = std::min(c.size, 1);   // miniatura mostra o estilo: Ilha e Região batem com a prévia, acima disso vale a Região
    const Style style = m_style == QLatin1String("mira") ? Style::Mira : Style::Rich;
    const int target = qRound(m_typeTiles.first()->width() * m_typeTiles.first()->devicePixelRatioF());
    QPointer<NewMap> self(this);
    m_pool->start([self, gen, c, style, target, cancel]() {
        QList<QImage> imgs;
        for (int i = 0; i < 4 && !cancel->load(); i++) {
            World w = rebuildBase(c.seed, c.water, QString::fromLatin1(kTypes[i]), c.size, c.climate);
            imgs.append(topDown(w, style, std::max(64, target), 1, cancel.get()));
        }
        if (cancel->load() || !self) return;
        QMetaObject::invokeMethod(self, [self, gen, imgs]() {
            if (!self || gen != self->m_thumbGen) return;
            for (int i = 0; i < imgs.size() && i < self->m_typeTiles.size(); i++) {
                self->m_typeTiles[i]->thumb = imgs[i];
                self->m_typeTiles[i]->update();
            }
        }, Qt::QueuedConnection);
    });
}

void NewMap::accept()
{
    NewMapChoice c = choice();
    if (m_ready && m_readyKey == key()) c.world = std::move(m_ready);
    m_ready.reset();
    if (m_cancel) m_cancel->store(true);
    if (m_thumbCancel) m_thumbCancel->store(true);
    m_debounce->stop();
    hide();
    if (accepted) accepted(c, m_mode);
}

void NewMap::relayout()
{
    if (!parentWidget()) return;
    const int W = width(), H = height();
    // cabe na janela: a coluna da direita tem largura fixa e a prévia fica com o resto (4:3)
    const int cw = std::min(900, W - 32);
    const int rightW = cw < 820 ? 340 : 380;
    m_right->setFixedWidth(rightW);
    for (NewMapTile* t : m_typeTiles) t->setFixedHeight(NewMapTile::thumbHeight((rightW - 18) / 4));
    const int pw = std::max(220, cw - 44 - 22 - rightW);
    // tela baixa: as dicas de baixo dos campos saem e os respiros apertam antes de a janela passar da borda
    auto compact = [this](bool on) {
        for (QLabel* h : { m_nameHint, m_sizeHint, m_typeHint, m_sub }) h->setVisible(!on);
        for (QSpacerItem* g : m_gaps) g->changeSize(0, on ? 2 : 9, QSizePolicy::Minimum, QSizePolicy::Fixed);
        m_right->layout()->setSpacing(on ? 4 : 7);
        m_card->layout()->invalidate();
        m_card->layout()->activate();
        return m_card->layout()->sizeHint().height();
    };
    m_preview->setFixedSize(pw, pw * 3 / 4);
    m_card->setFixedWidth(cw);
    int ch = compact(false);
    if (ch > H - 24) {
        ch = compact(true);
        if (ch > H - 24) {
            const int lose = ch - (H - 24);
            const int ph = std::max(120, pw * 3 / 4 - lose);
            m_preview->setFixedSize(ph * 4 / 3, ph);
            m_card->layout()->invalidate();
            m_card->layout()->activate();
            ch = m_card->layout()->sizeHint().height();
        }
    }
    m_card->setFixedHeight(std::min(ch, H - 16));
    m_card->move((W - cw) / 2, std::max(8, (H - m_card->height()) / 2));
}

void NewMap::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(6, 16, 24, 120));
}

void NewMap::resizeEvent(QResizeEvent*) { relayout(); }
void NewMap::mousePressEvent(QMouseEvent* e) { e->accept(); }
void NewMap::wheelEvent(QWheelEvent* e) { e->accept(); }

void NewMap::keyPressEvent(QKeyEvent* e)
{
    // tudo que chega aqui morre aqui: os atalhos do mapa atrás não disparam com a janela aberta
    if (e->key() == Qt::Key_Escape) dismiss();
    else if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) accept();
    e->accept();
}

} // namespace carto
