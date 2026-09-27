#include "ThemeScene.h"

#include "PanelGlass.h"

#include <QCoreApplication>
#include <QFile>
#include <QFont>
#include <QFontMetricsF>
#include <QHash>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QRegularExpression>
#include <QRunnable>
#include <QSvgRenderer>
#include <QTextLayout>
#include <QThreadPool>
#include <cmath>
#include <functional>
#include <memory>

namespace ThemeScene {

namespace {

QString tr(const char* s) { return QCoreApplication::translate("ThemeScene", s); }

QColor col(const QString& css, const QColor& fallback)
{
    const QColor c = Theme::toColor(css);
    return c.isValid() ? c : fallback;
}

QColor withAlpha(QColor c, qreal a) { c.setAlphaF(qBound(0.0, a, 1.0)); return c; }
QColor opaque(const QColor& c) { return QColor(c.red(), c.green(), c.blue()); }

// color-mix(in srgb, a k%, b)
QColor mix(const QColor& a, qreal k, const QColor& b)
{
    return QColor::fromRgbF(a.redF() * k + b.redF() * (1 - k),
                            a.greenF() * k + b.greenF() * (1 - k),
                            a.blueF() * k + b.blueF() * (1 - k));
}

// Cores já resolvidas de um tema, no vocabulário da cena.
struct Pal {
    QColor app, pn, bd, ed, tx, mu, ac, ui, br, dg, in, sb, hv, ic, dh;
    QColor dc;            // cor dentro da gaveta aberta: a da gaveta (Personagens) ou o destaque do tema
    bool icSet = false;
    QString dhFont;
    qreal r = 0;          // raio dos painéis na escala da cena (75% da tela real)
    qreal pnOpacity = 1;  // 0..1
    QMap<QString, Theme::MiraTheme::PanelColors> per;
    QImage glass;         // fundo + folha desfocados (a 1/4), quando o tema tem vidro

    QColor fill(const QString& key) const
    {
        QColor c = pn;
        const auto it = per.constFind(key);
        if (it != per.constEnd() && !it->background.isEmpty()) c = col(it->background, pn);
        c.setAlphaF(c.alphaF() * pnOpacity);
        return c;
    }
    QColor border(const QString& key) const
    {
        const auto it = per.constFind(key);
        return (it != per.constEnd() && !it->border.isEmpty()) ? col(it->border, bd) : bd;
    }
};

Pal palette(const Theme::MiraTheme& t)
{
    Pal p;
    p.app = col(t.appBackground, QColor(0x1a, 0x1a, 0x19));
    p.pn = col(t.panelBackground, p.app);
    p.bd = col(t.panelBorder, p.pn.lighter(130));
    p.ed = col(t.editorBackground, p.pn);
    p.tx = col(t.editorTextColor, QColor(0xcc, 0xcc, 0xcc));
    p.mu = col(t.textMuted, QColor(0x88, 0x88, 0x88));
    p.ac = col(t.accentDefault, QColor(0xd9, 0x77, 0x57));
    p.ui = col(t.textPrimary, p.tx);
    p.br = col(t.textBright, p.ui);
    p.dg = col(t.accentDanger, QColor(0xe0, 0x55, 0x55));
    p.in = col(t.accentInfo, QColor(0x4a, 0x9e, 0xff));
    p.sb = withAlpha(p.ui, 0.12);
    p.hv = withAlpha(p.ui, 0.08);
    p.icSet = !t.iconColor.isEmpty();
    p.ic = p.icSet ? col(t.iconColor, p.mu) : p.mu;
    p.dh = t.docHeaderColor.isEmpty() ? p.tx : col(t.docHeaderColor, p.tx);
    p.dc = t.drawerAccentFromTheme ? p.ac : QColor(0xdc, 0x26, 0x26);
    p.dhFont = t.docHeaderFont.isEmpty() ? QStringLiteral("Lora") : t.docHeaderFont;
    p.r = t.panelRadius * 0.75;
    p.pnOpacity = qBound(0, t.panelOpacity, 100) / 100.0;
    p.per = t.panelColors;
    return p;
}

QFont uiFont(qreal px, int weight = QFont::Normal)
{
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(qMax(1, qRound(px)));
    f.setWeight(QFont::Weight(weight));
    f.setHintingPreference(QFont::PreferNoHinting);
    return f;
}

QFont serifFont(qreal px, int weight = QFont::Normal, bool italic = false, const QString& family = QStringLiteral("Times New Roman"))
{
    QFont f(family);
    f.setStyleHint(QFont::Serif);
    f.setPixelSize(qMax(1, qRound(px)));
    f.setWeight(QFont::Weight(weight));
    f.setItalic(italic);
    f.setHintingPreference(QFont::PreferNoHinting);
    return f;
}

// SVGs reais do app, tingidos (mesma técnica do IconUtils) e desenhados como
// vetor — não pixelizam na escala da prévia nem no zoom.
QSvgRenderer* icon(const QString& path, const QColor& c)
{
    static QHash<QString, QSvgRenderer*> cache;
    const QString key = path + QLatin1Char('|') + c.name(QColor::HexArgb);
    if (auto* r = cache.value(key)) return r;
    QFile f(path);
    QString svg;
    if (f.open(QIODevice::ReadOnly)) svg = QString::fromUtf8(f.readAll());
    static const QRegularExpression fillRe(QStringLiteral("fill=\"#[0-9a-fA-F]+\""));
    static const QRegularExpression strokeRe(QStringLiteral("stroke=\"#[0-9a-fA-F]+\""));
    svg.replace(fillRe, QStringLiteral("fill=\"%1\"").arg(c.name()));
    svg.replace(strokeRe, QStringLiteral("stroke=\"%1\"").arg(c.name()));
    auto* r = new QSvgRenderer(svg.toUtf8());
    cache.insert(key, r);
    return r;
}

void drawIcon(QPainter& p, const QString& path, const QRectF& r, const QColor& c)
{
    QSvgRenderer* rd = icon(path, c);
    if (!rd->isValid()) return;
    p.save();
    p.setOpacity(p.opacity() * c.alphaF());
    rd->render(&p, r);
    p.restore();
}

QPainterPath roundedPath(const QRectF& r, qreal tl, qreal tr_, qreal br, qreal bl)
{
    QPainterPath path;
    path.moveTo(r.left() + tl, r.top());
    path.lineTo(r.right() - tr_, r.top());
    if (tr_ > 0) path.quadTo(r.right(), r.top(), r.right(), r.top() + tr_);
    path.lineTo(r.right(), r.bottom() - br);
    if (br > 0) path.quadTo(r.right(), r.bottom(), r.right() - br, r.bottom());
    path.lineTo(r.left() + bl, r.bottom());
    if (bl > 0) path.quadTo(r.left(), r.bottom(), r.left(), r.bottom() - bl);
    path.lineTo(r.left(), r.top() + tl);
    if (tl > 0) path.quadTo(r.left(), r.top(), r.left() + tl, r.top());
    path.closeSubpath();
    return path;
}

// Um painel: vidro (se o tema tiver), a cor translúcida dele e a borda.
void panelShape(QPainter& p, const QPainterPath& path, const Pal& c, const QString& key)
{
    if (!c.glass.isNull()) {
        p.save();
        p.setClipPath(path, Qt::IntersectClip);
        const QRectF b = path.boundingRect();
        p.drawImage(b, c.glass, QRectF(b.x() / 4.0, b.y() / 4.0, b.width() / 4.0, b.height() / 4.0));
        p.restore();
    }
    p.setPen(QPen(c.border(key), 1.0));
    p.setBrush(c.fill(key));
    p.drawPath(path);
}

void panelRect(QPainter& p, const QRectF& r, const Pal& c, qreal tl, qreal tr_, qreal br, qreal bl, const QString& key)
{
    panelShape(p, roundedPath(r, tl, tr_, br, bl), c, key);
}

void drawBackground(QPainter& p, const QRectF& r, const Theme::MiraTheme& t, const QColor& app, const QPixmap& bg)
{
    p.fillRect(r, app);
    if (bg.isNull()) return;
    p.save();
    p.setClipRect(r);
    const QSizeF s = bg.deviceIndependentSize();
    switch (t.backgroundMode) {
    case Theme::BgTile:
        p.drawTiledPixmap(r, bg);
        break;
    case Theme::BgStretch:
        p.drawPixmap(r, bg, QRectF(QPointF(0, 0), bg.size()));
        break;
    case Theme::BgCenter:
        p.drawPixmap(QRectF(r.center() - QPointF(s.width() / 2, s.height() / 2), s), bg, QRectF(QPointF(0, 0), bg.size()));
        break;
    case Theme::BgFit: {
        const QSizeF fit = s.scaled(r.size(), Qt::KeepAspectRatio);
        p.drawPixmap(QRectF(r.center() - QPointF(fit.width() / 2, fit.height() / 2), fit), bg, QRectF(QPointF(0, 0), bg.size()));
        break;
    }
    default: {
        const QSizeF cover = s.scaled(r.size(), Qt::KeepAspectRatioByExpanding);
        p.drawPixmap(QRectF(r.center() - QPointF(cover.width() / 2, cover.height() / 2), cover), bg, QRectF(QPointF(0, 0), bg.size()));
        break;
    }
    }
    p.restore();
}

// Parágrafo com recuo na primeira linha. Devolve o y logo abaixo dele.
qreal drawPara(QPainter& p, const QString& text, const QFont& font, qreal x, qreal y, qreal w,
               qreal lineH, qreal indent, qreal maxY)
{
    QTextLayout layout(text, font);
    layout.beginLayout();
    qreal ly = 0;
    bool first = true;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) break;
        const qreal off = first ? indent : 0;
        line.setLineWidth(w - off);
        line.setPosition(QPointF(off, ly + (lineH - line.height()) / 2));
        ly += lineH;
        first = false;
    }
    layout.endLayout();
    if (y < maxY) {
        p.save();
        p.setClipRect(QRectF(x - 2, y, w + 4, qMax(0.0, maxY - y)), Qt::IntersectClip);
        layout.draw(&p, QPointF(x, y));
        p.restore();
    }
    return y + ly;
}

// Texto da folha: o lorem da prévia de temas antiga (tributo, pedido do
// usuário) e, depois dele, o lorem clássico. Latim não se traduz.
const QStringList& sheetText()
{
    static const QStringList t = {
        QStringLiteral("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco."),
        QStringLiteral("Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."),
        QStringLiteral("Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium doloremque laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore veritatis et quasi architecto beatae vitae dicta sunt explicabo."),
        QStringLiteral("Nemo enim ipsam voluptatem quia voluptas sit aspernatur aut odit aut fugit, sed quia consequuntur magni dolores eos qui ratione voluptatem sequi nesciunt."),
        QStringLiteral("Neque porro quisquam est, qui dolorem ipsum quia dolor sit amet, consectetur, adipisci velit, sed quia non numquam eius modi tempora incidunt ut labore et dolore magnam aliquam quaerat voluptatem."),
        QStringLiteral("Ut enim ad minima veniam, quis nostrum exercitationem ullam corporis suscipit laboriosam, nisi ut aliquid ex ea commodi consequatur?"),
        QStringLiteral("Quis autem vel eum iure reprehenderit qui in ea voluptate velit esse quam nihil molestiae consequatur, vel illum qui dolorem eum fugiat quo voluptas nulla pariatur?"),
        QStringLiteral("At vero eos et accusamus et iusto odio dignissimos ducimus qui blanditiis praesentium voluptatum deleniti atque corrupti quos dolores et quas molestias excepturi sint occaecati cupiditate non provident."),
        QStringLiteral("Similique sunt in culpa qui officia deserunt mollitia animi, id est laborum et dolorum fuga."),
        QStringLiteral("Et harum quidem rerum facilis est et expedita distinctio. Nam libero tempore, cum soluta nobis est eligendi optio cumque nihil impedit quo minus id quod maxime placeat facere possimus, omnis voluptas assumenda est."),
        QStringLiteral("Temporibus autem quibusdam et aut officiis debitis aut rerum necessitatibus saepe eveniet ut et voluptates repudiandae sint et molestiae non recusandae."),
    };
    return t;
}

const QStringList& refText()
{
    static const QStringList t = {
        QStringLiteral("Curabitur pretium tincidunt lacus. Nulla gravida orci a odio."),
        QStringLiteral("Nullam varius, turpis et commodo pharetra, est eros bibendum elit, nec luctus magna felis sollicitudin mauris. Integer in mauris eu nibh euismod gravida. Duis ac tellus et risus vulputate vehicula."),
        QStringLiteral("Donec lobortis risus a elit. Etiam tempor. Ut ullamcorper, ligula eu tempor congue, eros est euismod turpis, id tincidunt sapien risus a quam. Maecenas fermentum consequat mi. Donec fermentum."),
        QStringLiteral("Pellentesque malesuada nulla a mi. Duis sapien sem, aliquet nec, commodo eget, consequat quis, neque. Aliquam faucibus, elit ut dictum aliquet, felis nisl adipiscing sapien, sed malesuada diam lacus eget erat."),
    };
    return t;
}

// Cores das gavetas são do PROJETO (o escritor escolhe), não do tema — ficam
// fixas como no print do app.
struct DrawerIcon { QString svg; QChar letter; QColor color; };
const QList<DrawerIcon>& drawerIcons()
{
    static const QList<DrawerIcon> d = {
        { QString(), QLatin1Char('T'), QColor(0xc0, 0x26, 0xd3) },
        { QStringLiteral(":/icons/elements/user.svg"), QChar(), QColor(0xdc, 0x26, 0x26) },
        { QStringLiteral(":/icons/elements/map.svg"), QChar(), QColor(0x15, 0x80, 0x3d) },
        { QStringLiteral(":/icons/elements/file.svg"), QChar(), QColor(0xea, 0xb3, 0x08) },
        { QStringLiteral(":/icons/elements/manuscript.svg"), QChar(), QColor(0x25, 0x63, 0xeb) },
    };
    return d;
}

void drawDrawerIcon(QPainter& p, const DrawerIcon& d, const QRectF& r)
{
    if (!d.letter.isNull()) {
        p.setPen(d.color);
        p.setFont(serifFont(r.height() * 1.0, QFont::Bold));
        p.drawText(r.adjusted(-4, -4, 4, 4), Qt::AlignCenter, QString(d.letter));
    } else {
        drawIcon(p, d.svg, r, d.color);
    }
}

// ------------------------------------------------------------ geometria
// Uma só fonte de verdade pra onde cada coisa fica na cena: a pintura e o
// clique do Criador de Temas (regions) leem daqui.

const QRectF kToolbar(254, -1, 978, 41);
const QRectF kLeftBar(7, 43, 38, kSceneH - 43 + 1);
const QRectF kSheet(410, 42, 666, kSceneH - 42);
const QRectF kTitle(410, 54, 666, 20);
const QRectF kColumn(488, 148, 510, kSceneH - 148);
const QRectF kRefMenu(1100, 86, kSceneW - 1100 + 1, 600);
const QRectF kCounter(54, 643, 192, 107);
const QRectF kCounterTab(57, 630, 14, 13);
const QRectF kRing(64, 668, 48, 48);
const QRectF kDrawer(56, -1, 252, kSceneH + 2);
const QRectF kNewChar(64, 43, 236, 28);

enum ItemKind { Icon, Text, Glyph };
struct TbItem { ItemKind k; QString a; QString b; qreal w; };
const QList<TbItem>& toolbarItems()
{
    const QString I = QStringLiteral(":/icons/");
    static const QList<TbItem> items = {
        {Icon, I + "fullscreen.svg", {}, 20}, {Icon, I + "home.svg", {}, 20}, {Icon, I + "save-project.svg", {}, 20},
        {Icon, I + "loadproject.svg", {}, 20}, {Icon, I + "newproject.svg", {}, 20}, {Icon, I + "download.svg", {}, 20},
        {Icon, I + "theme-panel.svg", {}, 20}, {Icon, I + "settings.svg", {}, 20}, {Glyph, QStringLiteral("?"), {}, 20},
        {Icon, I + "font-size.svg", QStringLiteral("14.5"), 40}, {Text, QStringLiteral("Times New Roman"), {}, 96},
        {Icon, I + "text-spacing.svg", QStringLiteral("1.1"), 34}, {Glyph, QStringLiteral("¶"), QStringLiteral("dot"), 20},
        {Icon, I + "align-left.svg", {}, 20}, {Icon, I + "focusmode-off.svg", {}, 20}, {Icon, I + "layout.svg", {}, 20},
        {Glyph, QStringLiteral("B"), {}, 20}, {Glyph, QStringLiteral("I"), {}, 20}, {Glyph, QStringLiteral("U"), {}, 20}, {Glyph, QStringLiteral("S"), {}, 20},
        {Icon, I + "add-image.svg", {}, 20}, {Icon, I + "search.svg", {}, 20}, {Icon, I + "repetitions.svg", {}, 20},
        {Icon, I + "read-aloud.svg", {}, 20}, {Icon, I + "compare.svg", {}, 20}, {Icon, I + "constructor.svg", {}, 20},
        {Icon, I + "pensario.svg", {}, 20}, {Icon, I + "stats-chart.svg", {}, 20}, {Icon, I + "refmenu.svg", {}, 20},
        {Icon, I + "reminder.svg", {}, 20}, {Icon, I + "elements/star.svg", {}, 20},
    };
    return items;
}

// x de cada item da barra de cima, distribuídos como space-between.
QList<QRectF> toolbarItemRects()
{
    const auto& items = toolbarItems();
    qreal total = 0;
    for (const auto& it : items) total += it.w;
    const qreal gap = (kToolbar.width() - 32 - total) / (items.size() - 1);
    QList<QRectF> out;
    qreal x = kToolbar.left() + 16;
    for (const auto& it : items) { out.append(QRectF(x, 10, it.w, 20)); x += it.w + gap; }
    return out;
}

enum LbKind { LbIcon, LbRule, LbAdd, LbDrawer };
struct LbItem { LbKind k; QString svg; int drawer; QRectF r; };
QList<LbItem> leftBarItems()
{
    QList<LbItem> out;
    const qreal cx = kLeftBar.center().x();
    qreal y = 43 + 12;
    const QString L = QStringLiteral(":/icons/leftbar/");
    auto item = [&](const QString& svg) { out.append({LbIcon, svg, -1, QRectF(cx - 12, y, 24, 24)}); y += 24 + 12; };
    auto rule = [&]() { out.append({LbRule, {}, -1, QRectF(cx - 11, y - 3, 22, 1)}); y += 1 + 12 - 6; };
    item(L + "projectinfo.svg");
    rule();
    item(L + "whiteboard.svg");
    item(L + "timeline.svg");
    rule();
    item(L + "manuscriptpanel.svg");
    item(L + "outline.svg");
    item(L + "groups.svg");
    rule();
    out.append({LbAdd, {}, -1, QRectF(cx - 11, y + 1, 22, 22)});
    y += 24 + 12;
    for (int i = 0; i < drawerIcons().size(); ++i) { out.append({LbDrawer, {}, i, QRectF(cx - 12, y, 24, 24)}); y += 24 + 12; }
    return out;
}

// Ícones do cabeçalho e do trilho da Referência (centro de cada um).
QList<QRectF> refMenuIconRects()
{
    QList<QRectF> out;
    qreal hx = kSceneW - 10 - 12;
    for (int i = 0; i < 7; ++i) { out.append(QRectF(hx, 86 + 8, 12, 12)); hx -= 12 + 13; }
    const qreal rcx = 1100 + 17;
    qreal ry = 86 + 29 + 12;
    out.append(QRectF(rcx - 6, ry, 12, 12));  // painel
    ry += 12 + 11 + (19 + 11) * 3;
    out.append(QRectF(rcx - 6, ry, 12, 12));  // estrela
    ry += 12 + 11;
    out.append(QRectF(rcx - 6, ry, 12, 12));  // relógio
    return out;
}

// ------------------------------------------------------------ pintura

void paintToolbar(QPainter& p, const Pal& c)
{
    panelRect(p, kToolbar, c, 0, 0, c.r, c.r, Theme::PanelKey::TopToolbar);
    const auto& items = toolbarItems();
    const auto rects = toolbarItemRects();
    const qreal cy = 20;
    // Botões de texto (fonte, tamanho, entrelinha) seguem a cor dos ícones
    // quando o tema define uma; sem ela ficam como no app de sempre.
    const QColor textCol = c.icSet ? c.ic : c.ui;
    const QColor fontCol = c.icSet ? c.ic : c.br;
    for (int i = 0; i < items.size(); ++i) {
        const auto& it = items.at(i);
        const qreal x = rects.at(i).left();
        if (it.k == Icon) {
            drawIcon(p, it.a, QRectF(x + 3.5, cy - 6.5, 13, 13), c.ic);
            if (!it.b.isEmpty()) {
                p.setPen(textCol);
                p.setFont(uiFont(9));
                p.drawText(QRectF(x + 19, cy - 8, it.w - 19, 16), Qt::AlignVCenter | Qt::AlignLeft, it.b);
            }
        } else if (it.k == Text) {
            p.setPen(fontCol);
            p.setFont(serifFont(10.5, QFont::DemiBold));
            p.drawText(QRectF(x, cy - 8, it.w, 16), Qt::AlignCenter, it.a);
        } else {
            QFont f = serifFont(11.5, QFont::Bold);
            if (it.a == QLatin1String("I")) f.setItalic(true);
            if (it.a == QLatin1String("U")) f.setUnderline(true);
            if (it.a == QLatin1String("S")) f.setStrikeOut(true);
            if (it.a == QLatin1String("?")) f = uiFont(11, QFont::DemiBold);
            p.setFont(f);
            p.setPen(c.ic);
            p.drawText(QRectF(x, cy - 9, it.w, 18), Qt::AlignCenter, it.a);
            if (it.b == QLatin1String("dot")) {
                p.setPen(Qt::NoPen);
                p.setBrush(c.dg);
                p.drawRoundedRect(QRectF(x + it.w - 7, cy - 9, 5, 5), 1, 1);
            }
        }
    }
}

void paintLeftBar(QPainter& p, const Pal& c, bool drawerOpen)
{
    panelRect(p, kLeftBar, c, c.r, c.r, 0, 0, Theme::PanelKey::LeftBar);
    const QColor bd = c.border(Theme::PanelKey::LeftBar);
    for (const LbItem& it : leftBarItems()) {
        const QPointF m = it.r.center();
        switch (it.k) {
        case LbIcon:
            drawIcon(p, it.svg, QRectF(m.x() - 7.5, m.y() - 7.5, 15, 15), c.ic);
            break;
        case LbRule:
            p.fillRect(it.r, bd);
            break;
        case LbAdd:
            p.setPen(QPen(bd, 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(it.r, 3, 3);
            p.setPen(QPen(c.ic, 1.2));
            p.drawLine(QPointF(m.x() - 4, m.y()), QPointF(m.x() + 4, m.y()));
            p.drawLine(QPointF(m.x(), m.y() - 4), QPointF(m.x(), m.y() + 4));
            break;
        case LbDrawer:
            if (drawerOpen && it.drawer == 1) {
                p.setPen(QPen(c.ac, 1));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(it.r, 3, 3);
            }
            drawDrawerIcon(p, drawerIcons().at(it.drawer), QRectF(m.x() - 7.5, m.y() - 7.5, 15, 15));
            break;
        }
    }
}

void paintSheet(QPainter& p, const Pal& c, const Theme::MiraTheme& t)
{
    // Sombra projetada da folha (quando o tema pede): camadas de alpha baixo.
    if (t.pageShadowEnabled) {
        const QColor sc = col(t.pageShadowColor, QColor(0, 0, 0, 140));
        const int layers = qBound(3, int(t.pageShadowRadius * 0.75 / 2), 18);
        const qreal off = t.pageShadowOffset * 0.75;
        for (int i = layers; i >= 1; --i) {
            QColor s = sc;
            s.setAlphaF(sc.alphaF() * 0.9 / layers);
            p.setPen(Qt::NoPen);
            p.setBrush(s);
            p.drawRoundedRect(kSheet.adjusted(-i * 1.4, -i * 1.4 + off, i * 1.4, i * 1.4 + off), i, i);
        }
    }
    QColor fill = c.ed;
    fill.setAlpha(qMin(fill.alpha(), qBound(0, t.editorOpacity * 255 / 100, 255)));
    p.fillRect(kSheet, fill);
    // Título do DocHeader: fonte e cor próprias do tema (padrão: Lora na cor do texto).
    p.setPen(c.dh);
    p.setFont(serifFont(11.5, QFont::Bold, false, c.dhFont));
    p.drawText(kTitle, Qt::AlignCenter, tr("Capítulo 3"));
    const QFont f = serifFont(13);
    qreal y = kColumn.top();
    for (const QString& para : sheetText()) {
        p.setPen(c.tx);
        y = drawPara(p, para, f, kColumn.left(), y, kColumn.width(), 18, y == kColumn.top() ? 0 : 23, kSceneH) + 4.5;
        if (y > kSceneH) break;
    }
    // Barra de rolagem da página, no vão entre a folha e a Referência.
    p.setPen(QPen(c.bd, 1));
    p.setBrush(c.fill(QString()));
    p.drawRoundedRect(QRectF(1083, 100, 5, 560), 2.5, 2.5);
}

void avatar(QPainter& p, const QRectF& r, const Pal& c, qreal k, const QString& letter)
{
    p.setPen(Qt::NoPen);
    p.setBrush(mix(c.ac, k, c.mu));
    p.drawEllipse(r);
    p.setPen(opaque(c.ed));
    p.setFont(serifFont(r.height() * 0.47, QFont::DemiBold));
    p.drawText(r, Qt::AlignCenter, letter);
}

void paintRefMenu(QPainter& p, const Pal& c)
{
    const QRectF box = kRefMenu;
    panelRect(p, box, c, c.r, 0, 0, c.r, Theme::PanelKey::RefMenu);
    p.save();
    p.setClipRect(box.adjusted(1, 1, -1, -1));
    const QString I = QStringLiteral(":/icons/");
    // Cabeçalho
    p.setPen(c.ic);
    p.setFont(uiFont(9));
    p.drawText(QRectF(1110, 86, 12, 28), Qt::AlignVCenter, QStringLiteral("⁞⁞"));
    p.setPen(c.br);
    p.setFont(uiFont(9.5, QFont::DemiBold));
    p.drawText(QRectF(1124, 86, 120, 28), Qt::AlignVCenter, tr("Referência"));
    const QStringList head = { I + "close.svg", I + "elements/pin.svg", I + "edit.svg", QStringLiteral("Aa"),
                               I + "search.svg", I + "panel-left.svg", I + "layout.svg" };
    const auto iconRects = refMenuIconRects();
    for (int i = 0; i < head.size(); ++i) {
        const QRectF ir = iconRects.at(i);
        if (head.at(i) == QLatin1String("Aa")) {
            p.setPen(c.ic);
            p.setFont(uiFont(9));
            p.drawText(QRectF(ir.left() - 2, 86, 16, 28), Qt::AlignCenter, head.at(i));
        } else {
            drawIcon(p, head.at(i), ir, c.ic);
        }
    }
    p.fillRect(QRectF(box.left(), 86 + 28, box.width(), 1), c.sb);
    // Trilho
    const qreal railX = 1100, railW = 34, bodyTop = 86 + 29;
    p.fillRect(QRectF(railX + railW, bodyTop, 1, 600 - 29), c.sb);
    qreal ry = bodyTop + 12;
    const qreal rcx = railX + railW / 2;
    drawIcon(p, I + "panel-left.svg", QRectF(rcx - 6, ry, 12, 12), c.ic); ry += 12 + 11;
    avatar(p, QRectF(rcx - 9.5, ry, 19, 19), c, 0.80, QStringLiteral("I")); ry += 19 + 11;
    avatar(p, QRectF(rcx - 9.5, ry, 19, 19), c, 0.45, QStringLiteral("B")); ry += 19 + 11;
    avatar(p, QRectF(rcx - 9.5, ry, 19, 19), c, 0.15, QStringLiteral("O")); ry += 19 + 11;
    drawIcon(p, I + "elements/star.svg", QRectF(rcx - 6, ry, 12, 12), c.ic); ry += 12 + 11;
    drawIcon(p, I + "stats-clock.svg", QRectF(rcx - 6, ry, 12, 12), c.ic); ry += 12 + 11;
    p.fillRect(QRectF(rcx - 5, ry, 10, 13), c.mu); ry += 13 + 11;
    for (const auto& d : drawerIcons()) { drawDrawerIcon(p, d, QRectF(rcx - 6, ry, 12, 12)); ry += 12 + 11; }
    drawIcon(p, I + "elements/planet.svg", QRectF(rcx - 6, ry, 12, 12), c.ic);
    // Migalha, aba e conteúdo
    const qreal mx = railX + railW + 1, mw = box.right() - mx;
    const QString chap = tr("Capítulo 2 · A meada");
    p.setFont(uiFont(8.5));
    p.setPen(c.mu);
    const QString all = tr("Tudo");
    const qreal allW = QFontMetricsF(p.font()).horizontalAdvance(all);
    p.drawText(QRectF(mx + 12, bodyTop, 60, 24), Qt::AlignVCenter, all);
    p.drawText(QRectF(mx + 12 + allW + 7, bodyTop, 10, 24), Qt::AlignVCenter, QStringLiteral("›"));
    p.setPen(c.ui);
    p.drawText(QRectF(mx + 12 + allW + 20, bodyTop, mw, 24), Qt::AlignVCenter, chap);
    p.fillRect(QRectF(mx, bodyTop + 24, mw, 1), c.sb);
    const qreal tabY = bodyTop + 25;
    const qreal tabW = QFontMetricsF(uiFont(8.5)).horizontalAdvance(chap) + 34;
    p.setPen(Qt::NoPen);
    p.setBrush(c.hv);
    p.drawRoundedRect(QRectF(mx + 8, tabY + 4, tabW, 22), 3, 3);
    p.setPen(c.br);
    p.drawText(QRectF(mx + 16, tabY + 4, tabW, 22), Qt::AlignVCenter, chap);
    p.setPen(c.mu);
    p.drawText(QRectF(mx + tabW - 8, tabY + 4, 12, 22), Qt::AlignVCenter, QStringLiteral("×"));
    drawIcon(p, I + "doc-plus.svg", QRectF(box.right() - 34, tabY + 8, 10, 10), c.ic);
    drawIcon(p, I + "layout.svg", QRectF(box.right() - 18, tabY + 8, 10, 10), c.ic);
    p.fillRect(QRectF(mx, tabY + 26, mw, 1), c.sb);
    qreal cy = tabY + 27 + 9;
    p.setPen(c.br);
    p.setFont(uiFont(10.5, QFont::Bold));
    p.drawText(QRectF(mx + 10, cy, mw - 20, 14), Qt::AlignVCenter, chap);
    cy += 14 + 5;
    const QFont rf = serifFont(11);
    for (const QString& para : refText()) {
        p.setPen(c.ui);
        cy = drawPara(p, para, rf, mx + 10, cy, mw - 22, 16.5, 22, box.bottom() - 4) + 4;
    }
    p.restore();
}

void paintCounter(QPainter& p, const Pal& c)
{
    const QString key = Theme::PanelKey::Counter;
    panelShape(p, roundedPath(kCounterTab, 0, 0, 0, 0), c, key);
    p.setPen(c.ic);
    p.setFont(uiFont(6));
    p.drawText(kCounterTab, Qt::AlignCenter, QStringLiteral("▼"));
    QPainterPath body;
    body.addRoundedRect(kCounter, c.r, c.r);
    panelShape(p, body, c, key);
    const QRectF box = kCounter;
    p.setPen(c.br);
    p.setFont(uiFont(8));
    p.drawText(QRectF(64, 650, 100, 12), Qt::AlignVCenter, tr("Contador"));
    p.setPen(c.mu);
    p.setFont(uiFont(7));
    p.drawText(QRectF(box.right() - 110, 650, 100, 12), Qt::AlignVCenter | Qt::AlignRight, tr("Estatísticas ›"));
    // Anel tracejado da meta
    const QRectF arc = kRing.adjusted(4, 4, -4, -4);
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 24; ++i) {
        p.setPen(QPen(c.ac, 5, Qt::SolidLine, Qt::FlatCap));
        p.drawArc(arc, int((90 - i * 15) * 16), int(-11.5 * 16));
    }
    p.setPen(c.br);
    p.setFont(uiFont(9.5, QFont::Bold));
    p.drawText(QRectF(kRing.left(), kRing.top() + 13, kRing.width(), 13), Qt::AlignCenter, QStringLiteral("281%"));
    p.setPen(c.mu);
    p.setFont(uiFont(5.5));
    p.drawText(QRectF(kRing.left(), kRing.top() + 26, kRing.width(), 8), Qt::AlignCenter, tr("da meta"));
    const qreal kx = 124, kw = box.right() - 10 - kx;
    p.setFont(uiFont(7.5));
    p.drawText(QRectF(kx, 676, kw, 14), Qt::AlignVCenter, tr("Palavras"));
    p.drawText(QRectF(kx, 697, kw, 14), Qt::AlignVCenter, tr("Hoje"));
    p.setPen(c.br);
    p.setFont(uiFont(10, QFont::DemiBold));
    p.drawText(QRectF(kx, 676, kw, 14), Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("1.936"));
    p.drawText(QRectF(kx, 697, kw, 14), Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("50min"));
    p.setPen(c.mu);
    p.setFont(uiFont(7));
    p.drawText(QRectF(134, 728, 100, 12), Qt::AlignVCenter, tr("Meta batida!"));
}

struct Person { QString name; QString role; bool bond; const char* seg; qreal k; };
QList<Person> people()
{
    return {
        { QStringLiteral("Ilse Varga"), tr("protagonista"), true, "1111100111111", 0.22 },
        { QStringLiteral("Bram Tecelão"), tr("protagonista"), true, "1111100111110", 0.12 },
        { QStringLiteral("Oda"), tr("coadjuvante"), false, "0011100111011", 0.30 },
        { QStringLiteral("Nils"), tr("coadjuvante"), false, "0000000000001", 0.08 },
        { tr("O Coletor"), tr("antagonista"), false, "1110010111111", 0.40 },
        { QStringLiteral("Mestra Hild"), tr("coadjuvante"), false, "0110000011000", 0.18 },
    };
}
QRectF cardRect(int i) { return QRectF(71, 109 + i * (79 + 8), 219, 79); }
QRectF tagRect(int i, const QString& role)
{
    QFont tf = uiFont(6.5, QFont::Bold);
    tf.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    tf.setCapitalization(QFont::AllUppercase);
    const QRectF card = cardRect(i);
    return QRectF(card.left() + 58 + 8, card.top() + 7, QFontMetricsF(tf).horizontalAdvance(role) + 8, 10);
}

void paintDrawer(QPainter& p, const Pal& c)
{
    const QString key = Theme::PanelKey::Drawers;
    const QRectF box = kDrawer;
    // Sombra suave da gaveta sobre a janela
    for (int i = 1; i <= 10; ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 5));
        p.drawRect(box.adjusted(-i * 0.4, 0, i * 1.6, 0).translated(0, 3));
    }
    panelRect(p, box, c, 0, 0, 0, 0, key);
    const QString I = QStringLiteral(":/icons/");
    p.setPen(c.br);
    p.setFont(serifFont(11.5, QFont::DemiBold));
    p.drawText(QRectF(67, 0, 140, 38), Qt::AlignVCenter, tr("Personagens"));
    qreal hx = box.right() - 10 - 11;
    drawIcon(p, I + "close.svg", QRectF(hx, 13.5, 11, 11), c.ic); hx -= 11 + 11;
    p.setPen(QPen(c.border(key), 1));
    p.setBrush(c.hv);
    p.drawRoundedRect(QRectF(hx - 6, 12, 18, 14), 2, 2);
    p.setPen(c.ic);
    p.setFont(uiFont(9));
    p.drawText(QRectF(hx - 6, 10, 18, 14), Qt::AlignCenter, QStringLiteral("…"));
    hx -= 11 + 13;
    p.setPen(QPen(c.ic, 1.2));
    p.drawLine(QPointF(hx + 1, 19), QPointF(hx + 10, 19));
    hx -= 11 + 11;
    drawIcon(p, I + "search.svg", QRectF(hx, 13.5, 11, 11), c.ic);
    // Criar novo personagem
    const QRectF nb = kNewChar;
    p.setPen(QPen(withAlpha(c.dc, 0.55), 1));
    p.setBrush(mix(c.dc, 0.13, opaque(c.pn)));
    p.drawRoundedRect(nb, 2, 2);
    p.setPen(QPen(c.dc, 1.3));
    p.setBrush(Qt::NoBrush);
    const QRectF pc(nb.left() + 10, nb.center().y() - 6.5, 13, 13);
    p.drawEllipse(pc);
    p.drawLine(QPointF(pc.center().x() - 3.5, pc.center().y()), QPointF(pc.center().x() + 3.5, pc.center().y()));
    p.drawLine(QPointF(pc.center().x(), pc.center().y() - 3.5), QPointF(pc.center().x(), pc.center().y() + 3.5));
    p.setPen(c.dc);
    p.setFont(serifFont(11, QFont::DemiBold));
    p.drawText(QRectF(nb.left() + 32, nb.top(), nb.width() - 36, nb.height()), Qt::AlignVCenter, tr("Criar novo personagem"));
    // + Pasta
    const QRectF fold(64, 79, 48, 16);
    p.setPen(QPen(c.border(key), 1, Qt::DashLine));
    p.drawRoundedRect(fold, 2, 2);
    p.setPen(c.mu);
    p.setFont(uiFont(7));
    p.drawText(fold, Qt::AlignCenter, tr("+ Pasta"));
    // Cartões
    const auto ppl = people();
    const QColor cardBg = mix(c.ui, 0.05, opaque(c.fill(key)));
    for (int i = 0; i < ppl.size(); ++i) {
        const Person& pr = ppl.at(i);
        const QRectF card = cardRect(i);
        QPainterPath clip;
        clip.addRoundedRect(card, 4, 4);
        p.save();
        p.setClipPath(clip);
        p.fillRect(card, cardBg);
        const QRectF pic(card.left(), card.top(), 58, card.height());
        p.fillRect(pic, mix(c.dc, pr.k, c.mu));
        // Silhueta
        p.setOpacity(0.55);
        p.setPen(Qt::NoPen);
        p.setBrush(opaque(c.ed));
        p.drawRoundedRect(QRectF(pic.center().x() - 17, pic.bottom() - 40, 34, 46), 17, 17);
        p.drawEllipse(QRectF(pic.center().x() - 10, pic.bottom() - 56, 20, 20));
        p.setOpacity(1);
        p.restore();
        p.setPen(QPen(c.border(key), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(card, 4, 4);
        const qreal ix = card.left() + 58 + 8, iw = card.right() - 8 - ix;
        QFont tf = uiFont(6.5, QFont::Bold);
        tf.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
        tf.setCapitalization(QFont::AllUppercase);
        const QRectF tg = tagRect(i, pr.role);
        p.setPen(Qt::NoPen);
        p.setBrush(c.dc);
        p.drawRoundedRect(tg, 1.5, 1.5);
        p.setPen(opaque(c.ed));
        p.setFont(tf);
        p.drawText(tg, Qt::AlignCenter, pr.role);
        p.setPen(c.br);
        p.setFont(serifFont(11, QFont::DemiBold));
        p.drawText(QRectF(ix, card.top() + 19, iw, 15), Qt::AlignVCenter, pr.name);
        qreal bx = ix;
        if (pr.bond) {
            p.setPen(Qt::NoPen);
            p.setBrush(c.in);
            p.drawEllipse(QRectF(ix, card.top() + 38.5, 5, 5));
            bx += 9;
        }
        p.setPen(c.mu);
        p.setFont(uiFont(7));
        p.drawText(QRectF(bx, card.top() + 35, iw, 12), Qt::AlignVCenter,
                   pr.bond ? tr("1 vínculo") : tr("sem vínculos"));
        const qreal sw = (iw - 12 * 2) / 13.0;
        for (int k = 0; k < 13; ++k) {
            p.setPen(Qt::NoPen);
            p.setBrush(pr.seg[k] == '1' ? c.dc : c.border(key));
            p.drawRoundedRect(QRectF(ix + k * (sw + 2), card.bottom() - 9, sw, 3), 1, 1);
        }
    }
}

// Geometria da cena Editor (a folha em tamanho de leitura).
struct EditorGeo { qreal f, padX, titleTop, subTop, textTop; };
EditorGeo editorGeo(const QRectF& r)
{
    EditorGeo g;
    g.f = qBound(13.5, r.width() / 36.0, 19.0);
    g.padX = qRound(r.width() * 0.09);
    g.titleTop = r.top() + qRound(g.f * 1.6);
    g.subTop = g.titleTop + g.f * 1.4 + g.f * 0.2 * 0.78;
    g.textTop = g.subTop + g.f + g.f * 1.4 * 0.78;
    return g;
}

void paintEditor(QPainter& p, const QRectF& r, const Pal& c)
{
    p.fillRect(r, opaque(c.ed));
    const EditorGeo g = editorGeo(r);
    const qreal f = g.f;
    p.setPen(c.dh);
    p.setFont(serifFont(f * 1.05, QFont::Bold, false, c.dhFont));
    p.drawText(QRectF(r.left(), g.titleTop, r.width(), f * 1.4), Qt::AlignCenter, tr("Capítulo 3"));
    p.setPen(c.mu);
    p.setFont(serifFont(f * 0.78, QFont::Normal, true));
    p.drawText(QRectF(r.left(), g.subTop, r.width(), f), Qt::AlignCenter, tr("Cena 1"));
    qreal y = g.textTop;
    const QFont pf = serifFont(f);
    const qreal lh = f * 1.55;
    const QStringList& paras = sheetText();
    for (int i = 0; i < paras.size() && y < r.bottom(); ++i) {
        p.setPen(c.tx);
        const qreal top = y;
        y = drawPara(p, paras.at(i), pf, r.left() + g.padX, y, r.width() - g.padX * 2, lh, i == 0 ? 0 : f * 1.7, r.bottom());
        if (i == 2) {
            // Cursor no fim do terceiro parágrafo, como quem está escrevendo.
            QTextLayout l(paras.at(i), pf);
            l.beginLayout();
            qreal lw = 0; bool first = true; int n = 0;
            while (true) {
                QTextLine line = l.createLine();
                if (!line.isValid()) break;
                line.setLineWidth(r.width() - g.padX * 2 - (first ? f * 1.7 : 0));
                lw = line.naturalTextWidth() + (first ? f * 1.7 : 0);
                first = false; ++n;
            }
            l.endLayout();
            const qreal cx = r.left() + g.padX + lw + 2;
            const qreal cyy = top + (n - 1) * lh + (lh - f * 1.1) / 2;
            p.fillRect(QRectF(cx, cyy, 1.5, f * 1.1), c.tx);
        }
        y += f * 0.35;
    }
}

// Fundo + folha desfocados, a 1/4 da cena — o que o vidro dos painéis mostra.
QImage glassBackdrop(const Theme::MiraTheme& t, const Pal& c, const QPixmap& bg)
{
    QImage img(QSize(int(kSceneW / 4), int(kSceneH / 4) + 1), QImage::Format_ARGB32_Premultiplied);
    img.fill(opaque(c.app));
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.scale(0.25, 0.25);
        drawBackground(p, QRectF(0, 0, kSceneW, kSceneH), t, c.app, bg);
        QColor fill = c.ed;
        fill.setAlpha(qMin(fill.alpha(), qBound(0, t.editorOpacity * 255 / 100, 255)));
        p.fillRect(kSheet, fill);
    }
    PanelGlass::boxBlur(img, qMax(1, qRound(t.panelBlur * 0.75 / 4.0)));
    return img;
}

} // namespace

void paintThumb(QPainter& p, const QRectF& r, const Theme::MiraTheme& t, const QPixmap& bg, qreal radius)
{
    const Pal c = palette(t);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QPainterPath clip;
    clip.addRoundedRect(r, radius, radius);
    p.setClipPath(clip);
    drawBackground(p, r, t, c.app, bg);
    const qreal w = r.width(), h = r.height();
    const qreal lineH = qMax(2.0, h * 4.0 / 86.0);
    // Topbar: um tico mais larga que a folha, centrada nela.
    const QRectF tb(r.left() + w * 0.15, r.top() + h * 0.07, w * 0.78, h * 0.08);
    p.setPen(QPen(c.border(Theme::PanelKey::TopToolbar), 1));
    p.setBrush(opaque(c.fill(Theme::PanelKey::TopToolbar)));
    p.drawRoundedRect(tb, tb.height() / 2, tb.height() / 2);
    // Lateral
    const QRectF lb(r.left() + w * 0.04, r.top() + h * 0.20, w * 0.07, h * 0.72);
    const qreal lr = qMin(5.0 * h / 86.0, lb.width() / 2);
    p.setPen(QPen(c.border(Theme::PanelKey::LeftBar), 1));
    p.setBrush(opaque(c.fill(Theme::PanelKey::LeftBar)));
    p.drawRoundedRect(lb, lr, lr);
    // Folha, quase encostando na topbar
    const QRectF pg(r.left() + w * 0.22, r.top() + h * 0.17, w * 0.64, h * 0.83 + 1);
    const qreal pr = 3.0 * h / 86.0;
    p.setPen(Qt::NoPen);
    p.setBrush(opaque(c.ed));
    p.drawPath(roundedPath(pg, pr, pr, 0, 0));
    // Linhas de texto
    const qreal padX = pg.width() * 0.08, padT = pg.width() * 0.09;
    const qreal gap = pg.height() * 0.07;
    const qreal cw = pg.width() - padX * 2;
    const qreal widths[] = { 0.40, 0.92, 0.84, 0.70 };
    qreal ly = pg.top() + padT;
    p.setOpacity(0.75);
    for (int i = 0; i < 4; ++i) {
        p.setBrush(i == 0 ? c.mu : c.tx);
        p.drawRoundedRect(QRectF(pg.left() + padX, ly, cw * widths[i], lineH), lineH / 2, lineH / 2);
        ly += lineH + gap;
    }
    p.setOpacity(1);
    // Destaque
    p.setBrush(c.ac);
    p.drawRoundedRect(QRectF(r.left() + w * 0.30, r.bottom() - h * 0.10 - lineH, w * 0.18, lineH), lineH / 2, lineH / 2);
    p.restore();
}

void paintScene(QPainter& p, const QRectF& r, const Theme::MiraTheme& t, Scene scene, const QPixmap& bg)
{
    Pal c = palette(t);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setClipRect(r);
    if (scene == Editor) {
        paintEditor(p, r, c);
        p.restore();
        return;
    }
    if (t.panelBlur > 0 && t.panelOpacity < 100) c.glass = glassBackdrop(t, c, bg);
    p.translate(r.topLeft());
    p.scale(r.width() / kSceneW, r.height() / kSceneH);
    const QRectF all(0, 0, kSceneW, kSceneH);
    drawBackground(p, all, t, c.app, bg);
    paintSheet(p, c, t);
    paintToolbar(p, c);
    paintRefMenu(p, c);
    if (scene == Writing) paintCounter(p, c);
    paintLeftBar(p, c, scene == Drawers);
    if (scene == Drawers) paintDrawer(p, c);
    p.restore();
}

QList<Region> regions(Scene scene, const QRectF& r)
{
    QList<Region> out;
    if (scene == Editor) {
        const EditorGeo g = editorGeo(r);
        out.append({ Group::Title, QString(), QRectF(r.left() + g.padX, g.titleTop, r.width() - g.padX * 2, g.f * 1.4) });
        out.append({ Group::Text, QString(), QRectF(r.left() + g.padX, g.subTop, r.width() - g.padX * 2, r.bottom() - g.subTop) });
        out.append({ Group::Sheet, QString(), r });
        return out;
    }
    const qreal kx = r.width() / kSceneW, ky = r.height() / kSceneH;
    auto map = [&](const QRectF& s) {
        return QRectF(r.left() + s.left() * kx, r.top() + s.top() * ky, s.width() * kx, s.height() * ky);
    };
    // Prioridade de cima pra baixo: o primeiro que contém o ponto ganha.
    if (scene == Drawers) {
        const auto ppl = people();
        out.append({ Group::Accent, QString(), map(kNewChar) });
        for (int i = 0; i < ppl.size(); ++i) out.append({ Group::Accent, QString(), map(tagRect(i, ppl.at(i).role)) });
        for (const LbItem& it : leftBarItems())
            if (it.k == LbDrawer && it.drawer == 1) out.append({ Group::Accent, QString(), map(it.r) });
    } else {
        out.append({ Group::Accent, QString(), map(kRing) });
    }
    for (const QRectF& ir : toolbarItemRects()) out.append({ Group::Icons, QString(), map(ir) });
    for (const LbItem& it : leftBarItems())
        if (it.k == LbIcon || it.k == LbAdd) out.append({ Group::Icons, QString(), map(it.r) });
    for (const QRectF& ir : refMenuIconRects()) out.append({ Group::Icons, QString(), map(ir.adjusted(-3, -3, 3, 3)) });
    if (scene == Drawers) {
        out.append({ Group::Panels, Theme::PanelKey::Drawers, map(kDrawer) });
    }
    out.append({ Group::Title, QString(), map(QRectF(kTitle.center().x() - 60, kTitle.top(), 120, kTitle.height())) });
    out.append({ Group::Text, QString(), map(kColumn) });
    out.append({ Group::Sheet, QString(), map(kSheet) });
    out.append({ Group::Panels, Theme::PanelKey::TopToolbar, map(kToolbar) });
    out.append({ Group::Panels, Theme::PanelKey::LeftBar, map(kLeftBar) });
    out.append({ Group::Panels, Theme::PanelKey::RefMenu, map(kRefMenu) });
    if (scene == Writing) {
        out.append({ Group::Panels, Theme::PanelKey::Counter, map(kCounter) });
        out.append({ Group::Panels, Theme::PanelKey::Counter, map(kCounterTab) });
    }
    out.append({ Group::Background, QString(), r });
    return out;
}

double contrast(const QColor& a, const QColor& b)
{
    auto lum = [](const QColor& c) {
        auto ch = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
        return 0.2126 * ch(c.redF()) + 0.7152 * ch(c.greenF()) + 0.0722 * ch(c.blueF());
    };
    const double x = lum(a), y = lum(b);
    return (qMax(x, y) + 0.05) / (qMin(x, y) + 0.05);
}

double distance(const Theme::MiraTheme& a, const Theme::MiraTheme& b)
{
    auto d = [](const QString& x, const QString& y) {
        const QColor p = Theme::toColor(x), q = Theme::toColor(y);
        return std::hypot(double(p.red() - q.red()), double(p.green() - q.green()), double(p.blue() - q.blue()));
    };
    double v = d(a.appBackground, b.appBackground) + d(a.editorBackground, b.editorBackground)
             + d(a.editorTextColor, b.editorTextColor) + 0.6 * d(a.accentDefault, b.accentDefault);
    if (a.backgroundImage.isEmpty() != b.backgroundImage.isEmpty()) v += 60;
    return v;
}

// ---------------------------------------------------------------- imagens

namespace {
class LoadTask : public QRunnable {
public:
    LoadTask(QString path, int cap, std::function<void(QImage)> done)
        : m_path(std::move(path)), m_cap(cap), m_done(std::move(done)) {}
    void run() override {
        QImageReader reader(m_path);
        reader.setAutoTransform(true);
        const QSize orig = reader.size();
        if (orig.isValid() && qMax(orig.width(), orig.height()) > m_cap)
            reader.setScaledSize(orig.scaled(m_cap, m_cap, Qt::KeepAspectRatio));
        m_done(reader.read());
    }
private:
    QString m_path;
    int m_cap;
    std::function<void(QImage)> m_done;
};
} // namespace

ImageCache* ImageCache::instance()
{
    static ImageCache* s = new ImageCache;
    return s;
}

ImageCache::ImageCache()
    : m_pool(new QThreadPool(this))
{
    // ~88 fotos pequenas cabem folgadas; as grandes (prévia/zoom) rodam num
    // LRU curto — só importam as do tema em uso e as últimas visitadas.
    m_small.setMaxCost(48 * 1024);   // KB
    m_large.setMaxCost(64 * 1024);
    m_pool->setMaxThreadCount(2);
}

QPixmap ImageCache::get(const QString& path, int cap)
{
    if (path.isEmpty()) return {};
    const QString key = QString::number(cap) + QLatin1Char('|') + path;
    auto& cache = cap <= kThumbCap ? m_small : m_large;
    if (QPixmap* pm = cache.object(key)) return *pm;
    if (m_pending.contains(key)) return {};
    m_pending.insert(key);
    QPointer<ImageCache> self(this);
    m_pool->start(new LoadTask(path, cap, [self, key, path, cap](QImage img) {
        // QImage atravessa a thread; o QPixmap só nasce na thread da GUI.
        QMetaObject::invokeMethod(self.data(), [self, key, path, cap, img]() {
            if (self) self->store(key, path, cap, img);
        }, Qt::QueuedConnection);
    }));
    return {};
}

void ImageCache::store(const QString& key, const QString& path, int cap, const QImage& img)
{
    m_pending.remove(key);
    // Foto que não abre vira pixmap vazio de custo 1: não tenta de novo a cada pintura.
    auto* pm = new QPixmap(img.isNull() ? QPixmap(1, 1) : QPixmap::fromImage(img));
    if (img.isNull()) pm->fill(Qt::transparent);
    const int cost = qMax(1, int(qint64(pm->width()) * pm->height() * 4 / 1024));
    (cap <= kThumbCap ? m_small : m_large).insert(key, pm, cost);
    emit ready(path, cap);
}

} // namespace ThemeScene
