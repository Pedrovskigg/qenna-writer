#include "RecadosView.h"

#include "Recados.h"
#include "Theme.h"
#include "PanelMotion.h"

#include <QDesktopServices>
#include <QElapsedTimer>
#include <QEnterEvent>
#include <QTimer>
#include <cmath>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QLineEdit>
#include <QInputDialog>
#include <QMessageBox>
#include <functional>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTextCursor>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QStyle>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

// Uma linha só. Quando o texto não cabe, ele corre para o lado devagar, como o
// letreiro do telejornal, até aparecer inteiro e recomeçar (pedido do Pe,
// 26/11/2026). Para com o mouse em cima, para ler com calma. Com "Animações"
// desligado, volta ao texto cortado com "…". Só esta faixinha é redesenhada,
// a 30 quadros, e só enquanto o texto corre.
class ElidedLabel : public QLabel {
public:
    explicit ElidedLabel(QWidget* parent = nullptr) : QLabel(parent)
    {
        m_tick.setInterval(33);
        QObject::connect(&m_tick, &QTimer::timeout, this, [this]() { update(); });
        m_clock.start();
    }
    QSize minimumSizeHint() const override { return { 40, QLabel::minimumSizeHint().height() }; }
    QSize sizeHint() const override { return { 200, QLabel::sizeHint().height() }; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setPen(palette().color(QPalette::WindowText));
        const QString txt = text();
        if (txt != m_last) { m_last = txt; restart(); }
        const int tw = fontMetrics().horizontalAdvance(txt);
        if (tw <= width() || !PanelMotion::enabled()) {
            m_tick.stop();
            p.drawText(rect(), Qt::AlignVCenter | Qt::AlignLeft, fontMetrics().elidedText(txt, Qt::ElideRight, width()));
            return;
        }
        // o letreiro: duas cópias seguidas, com um respiro entre elas; a conta do
        // deslocamento é pelo relógio (não pelo quadro), então não acelera nem atrasa
        const int gap = fontMetrics().horizontalAdvance(QStringLiteral("     ·     "));
        const qint64 now = m_clock.elapsed();
        if (!m_hover) m_run += qMax<qint64>(0, now - qMax(m_lastPaint, m_holdUntil));
        m_lastPaint = now;
        const qreal speed = 38.0 / 1000.0;   // px por ms
        const qreal cycle = tw + gap;
        const qreal off = std::fmod(m_run * speed, cycle);
        QImage img(size() * devicePixelRatioF(), QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(devicePixelRatioF());
        img.fill(Qt::transparent);
        {
            QPainter ip(&img);
            ip.setRenderHint(QPainter::TextAntialiasing, true);
            ip.setFont(font());
            ip.setPen(palette().color(QPalette::WindowText));
            const QRectF r(-off, 0, tw + 2, height());
            ip.drawText(r, Qt::AlignVCenter | Qt::AlignLeft, txt);
            ip.drawText(r.translated(cycle, 0), Qt::AlignVCenter | Qt::AlignLeft, txt);
            // as pontas esfumam (o texto entra e sai, não aparece cortado)
            ip.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            const qreal fade = qMin(18.0, width() / 6.0);
            QLinearGradient g(0, 0, width(), 0);
            g.setColorAt(0.0, QColor(0, 0, 0, off < 1 ? 255 : 0));
            g.setColorAt(fade / width(), QColor(0, 0, 0, 255));
            g.setColorAt(1.0 - fade / width(), QColor(0, 0, 0, 255));
            g.setColorAt(1.0, QColor(0, 0, 0, 0));
            ip.fillRect(rect(), g);
        }
        p.drawImage(0, 0, img);
        if (isVisible() && !m_tick.isActive()) m_tick.start();
    }
    void enterEvent(QEnterEvent* e) override { m_hover = true; QLabel::enterEvent(e); }
    void leaveEvent(QEvent* e) override { m_hover = false; m_lastPaint = m_clock.elapsed(); QLabel::leaveEvent(e); }
    void showEvent(QShowEvent* e) override { QLabel::showEvent(e); m_lastPaint = m_clock.elapsed(); update(); }
    void hideEvent(QHideEvent* e) override { QLabel::hideEvent(e); m_tick.stop(); }

private:
    // texto novo: começa do início, parado um instante para dar tempo de ler o começo
    void restart()
    {
        m_run = 0;
        m_holdUntil = m_clock.elapsed() + 2200;
        m_lastPaint = m_clock.elapsed();
    }
    QTimer m_tick;
    QElapsedTimer m_clock;
    QString m_last;
    qint64 m_run = 0, m_lastPaint = 0, m_holdUntil = 0;
    bool m_hover = false;
};

namespace {

QColor mix(const QColor& a, const QColor& b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

enum class Glyph { Pin, Left, Right, Close, Bell, Heart, Fire, Clap, Laugh };

// Ícones desenhados (sem caractere de fonte, que no Linux pode faltar).
QIcon glyphIcon(Glyph g, const QColor& c, int size, qreal dpr, bool filled = false)
{
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    const qreal s = size / 24.0;
    p.scale(s, s);
    switch (g) {
    case Glyph::Pin: {
        QPainterPath path;
        path.moveTo(9, 4); path.lineTo(15, 4); path.lineTo(14, 9); path.lineTo(17, 12);
        path.lineTo(17, 14); path.lineTo(7, 14); path.lineTo(7, 12); path.lineTo(10, 9); path.closeSubpath();
        p.drawPath(path);
        p.drawLine(QPointF(12, 14), QPointF(12, 20));
        break;
    }
    case Glyph::Left:
        p.drawPolyline(QPolygonF({ QPointF(15, 6), QPointF(9, 12), QPointF(15, 18) }));
        break;
    case Glyph::Right:
        p.drawPolyline(QPolygonF({ QPointF(9, 6), QPointF(15, 12), QPointF(9, 18) }));
        break;
    case Glyph::Bell: {
        QPainterPath path;
        path.moveTo(6, 16); path.cubicTo(7, 14.5, 6.5, 12, 6.5, 10);
        path.cubicTo(6.5, 6.5, 9, 4.5, 12, 4.5);
        path.cubicTo(15, 4.5, 17.5, 6.5, 17.5, 10);
        path.cubicTo(17.5, 12, 17, 14.5, 18, 16); path.closeSubpath();
        p.drawPath(path);
        p.drawArc(QRectF(10, 17, 4, 3.5), 180 * 16, 180 * 16);
        break;
    }
    case Glyph::Close:
        p.drawLine(QPointF(7, 7), QPointF(17, 17));
        p.drawLine(QPointF(17, 7), QPointF(7, 17));
        break;
    // ── as reações (traço; "filled" = a pessoa marcou) ──
    case Glyph::Heart: {
        QPainterPath path;
        path.moveTo(12, 20);
        path.cubicTo(5, 15, 3.5, 11.5, 3.5, 8.8);
        path.cubicTo(3.5, 6.2, 5.5, 4.5, 7.8, 4.5);
        path.cubicTo(9.6, 4.5, 11.2, 5.6, 12, 7.2);
        path.cubicTo(12.8, 5.6, 14.4, 4.5, 16.2, 4.5);
        path.cubicTo(18.5, 4.5, 20.5, 6.2, 20.5, 8.8);
        path.cubicTo(20.5, 11.5, 19, 15, 12, 20);
        path.closeSubpath();
        if (filled) p.setBrush(c);
        p.drawPath(path);
        break;
    }
    case Glyph::Fire: {
        QPainterPath out;
        out.moveTo(12, 21);
        out.cubicTo(8, 21, 5.5, 18.3, 5.5, 14.8);
        out.cubicTo(5.5, 11.2, 8.5, 9.5, 9, 6);
        out.cubicTo(11, 7.5, 11.8, 9.2, 11.6, 11);
        out.cubicTo(13.2, 9.6, 13.8, 6.4, 13, 3);
        out.cubicTo(16.8, 5.5, 18.5, 10, 18.5, 14.8);
        out.cubicTo(18.5, 18.3, 16, 21, 12, 21);
        out.closeSubpath();
        QPainterPath in;
        in.moveTo(12, 21);
        in.cubicTo(10.2, 21, 9.2, 19.7, 9.2, 18.2);
        in.cubicTo(9.2, 16.4, 10.8, 15.6, 11.3, 13.8);
        in.cubicTo(13.2, 15, 14.8, 16.5, 14.8, 18.2);
        in.cubicTo(14.8, 19.8, 13.6, 21, 12, 21);
        // a chama de dentro sempre cheia (dá forma no tamanho pequeno); marcado = a de fora também
        if (filled) { QColor soft = c; soft.setAlphaF(0.45); p.setBrush(soft); }
        p.drawPath(out);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPath(in);
        break;
    }
    case Glyph::Clap: {
        // duas palmas batendo (silhueta unida, uma mais alta que a outra) + o estalo em cima
        auto palm = [](QPointF at, qreal deg) {
            QPainterPath h;
            h.addRoundedRect(QRectF(-3.3, -6.6, 6.6, 13.2), 3.3, 3.3);
            return QTransform().translate(at.x(), at.y()).rotate(deg).map(h);
        };
        // a mão da frente: palma + polegar, com os vãos dos dedos (é o que faz ler "mão")
        const QTransform tf = QTransform().translate(10.4, 15.2).rotate(-18);
        QPainterPath frontLocal;
        frontLocal.addRoundedRect(QRectF(-3.4, -7.2, 6.8, 13.6), 3.2, 3.2);
        QPainterPath thumb;
        thumb.addRoundedRect(QRectF(-6.4, -1.2, 4.6, 2.9), 1.45, 1.45);
        const QPainterPath front = tf.map(frontLocal.united(QTransform().rotate(-38).map(thumb)));
        const QPainterPath back = palm(QPointF(15.2, 13.6), 26);
        QColor soft = c; soft.setAlphaF(filled ? 0.45 : 0.0);
        p.setBrush(soft);
        p.drawPath(back.subtracted(front));
        p.drawPath(front);
        p.setBrush(Qt::NoBrush);
        p.drawLine(tf.map(QLineF(-1.1, -7.0, -1.1, -3.4)));
        p.drawLine(tf.map(QLineF(1.1, -7.0, 1.1, -3.4)));
        p.drawLine(QPointF(4.6, 6.6), QPointF(6.4, 8.4));
        p.drawLine(QPointF(8.6, 3.4), QPointF(9.2, 5.8));
        p.drawLine(QPointF(13.2, 3.2), QPointF(12.4, 5.6));
        break;
    }
    case Glyph::Laugh: {
        QColor soft = c; soft.setAlphaF(filled ? 0.3 : 0.0);
        p.setBrush(soft);
        p.drawEllipse(QPointF(12, 12), 8.6, 8.6);
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(QPolygonF({ QPointF(7.6, 10.4), QPointF(9, 8.9), QPointF(10.4, 10.4) }));
        p.drawPolyline(QPolygonF({ QPointF(13.6, 10.4), QPointF(15, 8.9), QPointF(16.4, 10.4) }));
        QPainterPath mouth;
        mouth.moveTo(7.6, 13.2);
        mouth.lineTo(16.4, 13.2);
        mouth.cubicTo(16.1, 16.4, 14.2, 17.9, 12, 17.9);
        mouth.cubicTo(9.8, 17.9, 7.9, 16.4, 7.6, 13.2);
        mouth.closeSubpath();
        p.setBrush(c);
        p.drawPath(mouth);
        break;
    }
    }
    return QIcon(pm);
}

QLocale appLocale()
{
    const QString code = QSettings().value(QStringLiteral("app/language")).toString();
    return code.isEmpty() ? QLocale() : QLocale(code);
}

// "24 NOV" na faixa; "24 de novembro de 2026" no leitor.
QString shortDate(const QDate& d)
{
    QString s = appLocale().toString(d, QStringLiteral("d MMM"));
    s.remove(QLatin1Char('.'));
    return s.toUpper();
}

QString longDate(const QDate& d)
{
    // Sem o dia da semana ("terça-feira, ..."): o formato longo do idioma, sem ele.
    const QLocale loc = appLocale();
    QString f = loc.dateFormat(QLocale::LongFormat);
    f.remove(QRegularExpression(QStringLiteral("d{4}[,.]?\\s*")));
    return loc.toString(d, f.trimmed());
}

QToolButton* iconButton(QWidget* parent, const QString& tip)
{
    auto* b = new QToolButton(parent);
    b->setObjectName(QStringLiteral("recIconBtn"));
    b->setCursor(Qt::PointingHandCursor);
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->setFixedSize(24, 24);
    b->setIconSize(QSize(14, 14));
    return b;
}

} // namespace

// ============================== Faixa ==============================

RecadosStrip::RecadosStrip(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("recadosStrip"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(46);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(16, 0, 10, 0);
    row->setSpacing(12);

    m_pin = new QLabel(this);
    m_pin->setFixedSize(18, 18);
    row->addWidget(m_pin);

    m_eyebrow = new QLabel(this);
    m_eyebrow->setObjectName(QStringLiteral("recEyebrow"));
    row->addWidget(m_eyebrow);

    m_title = new QPushButton(this);
    m_title->setObjectName(QStringLiteral("recTitle"));
    m_title->setCursor(Qt::PointingHandCursor);
    m_title->setFlat(true);
    connect(m_title, &QPushButton::clicked, this, &RecadosStrip::readRequested);
    row->addWidget(m_title);

    m_body = new ElidedLabel(this);
    m_body->setObjectName(QStringLiteral("recBody"));
    row->addWidget(m_body, 1);

    m_prev = iconButton(this, tr("Recado anterior"));
    connect(m_prev, &QToolButton::clicked, this, [this]() { step(-1); });
    row->addWidget(m_prev);
    m_count = new QLabel(this);
    m_count->setObjectName(QStringLiteral("recCount"));
    row->addWidget(m_count);
    m_next = iconButton(this, tr("Próximo recado"));
    connect(m_next, &QToolButton::clicked, this, [this]() { step(1); });
    row->addWidget(m_next);

    m_read = new QPushButton(tr("Ler"), this);
    m_read->setObjectName(QStringLiteral("recRead"));
    m_read->setCursor(Qt::PointingHandCursor);
    m_read->setFlat(true);
    connect(m_read, &QPushButton::clicked, this, &RecadosStrip::readRequested);
    row->addWidget(m_read);

    m_close = iconButton(this, tr("Dispensar (volta quando chegar recado novo)"));
    connect(m_close, &QToolButton::clicked, this, []() { RecadosFeed::instance()->dismissStrip(); });
    row->addWidget(m_close);

    RecadosFeed* feed = RecadosFeed::instance();
    connect(feed, &RecadosFeed::changed, this, [this]() { m_page = 0; refresh(); });
    connect(feed, &RecadosFeed::stateChanged, this, &RecadosStrip::refresh);
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, &RecadosStrip::applyTheme);
    applyTheme();
    refresh();
}

void RecadosStrip::step(int delta)
{
    const int n = RecadosFeed::instance()->recados().size();
    if (n == 0) return;
    m_page = (m_page + delta + n) % n;
    refresh();
}

void RecadosStrip::refresh()
{
    RecadosFeed* feed = RecadosFeed::instance();
    const QList<Recado> list = feed->recados();
    if (list.isEmpty() || feed->stripDismissed()) { hide(); return; }
    m_page = qBound(0, m_page, int(list.size()) - 1);
    const Recado& r = list.at(m_page);
    const QString when = r.date.isValid() ? shortDate(r.date) : QString();
    const bool fresh = !feed->isRead(r.id);
    const QString label = fresh ? tr("RECADO NOVO") : tr("RECADO");
    m_eyebrow->setText(when.isEmpty() ? label : label + QStringLiteral("  ·  ") + when);
    m_eyebrow->setProperty("fresh", fresh);
    m_eyebrow->style()->unpolish(m_eyebrow);
    m_eyebrow->style()->polish(m_eyebrow);
    m_title->setText(r.title);
    m_body->setText(QString(r.body).replace(QLatin1Char('\n'), QLatin1Char(' ')));
    const bool many = list.size() > 1;
    m_prev->setVisible(many);
    m_next->setVisible(many);
    m_count->setVisible(many);
    m_count->setText(tr("%1 de %2").arg(m_page + 1).arg(list.size()));
    show();
}

void RecadosStrip::applyTheme()
{
    const QColor accent = Theme::toColor(Theme::accentDefault());
    const QColor bg = Theme::toColor(Theme::chromeBackground());
    const QColor tint = mix(bg, accent, 0.07);
    const QColor text = Theme::toColor(Theme::textPrimary());
    const QColor muted = Theme::toColor(Theme::textMuted());
    const qreal dpr = devicePixelRatioF();
    m_pin->setPixmap(glyphIcon(Glyph::Pin, accent, 18, dpr).pixmap(18, 18));
    m_prev->setIcon(glyphIcon(Glyph::Left, muted, 14, dpr));
    m_next->setIcon(glyphIcon(Glyph::Right, muted, 14, dpr));
    m_close->setIcon(glyphIcon(Glyph::Close, muted, 14, dpr));
    QPalette pal = m_body->palette();
    pal.setColor(QPalette::WindowText, muted);
    m_body->setPalette(pal);
    setStyleSheet(Theme::qss(QStringLiteral(R"(
        QFrame#recadosStrip { background: %1; border: 1px solid %2; border-radius: @radius-control; }
        QLabel#recEyebrow { color: %4; font-size: 10px; font-weight: 600; letter-spacing: 1px; background: transparent; }
        QLabel#recEyebrow[fresh="true"] { color: %5; }
        QPushButton#recTitle { color: %3; background: transparent; border: none; padding: 0px;
                               font-family: 'Lora', Georgia, serif; font-size: 14px; font-weight: 600; text-align: left; }
        QPushButton#recTitle:hover { color: %5; }
        QLabel#recBody { font-size: 13px; background: transparent; }
        QLabel#recCount { color: %4; font-size: 11px; background: transparent; }
        QToolButton#recIconBtn { background: transparent; border: none; border-radius: 5px; }
        QToolButton#recIconBtn:hover { background: %6; }
        QPushButton#recRead { color: %5; background: transparent; border: none; padding: 0px 4px;
                              font-size: 13px; font-weight: 600; }
        QPushButton#recRead:hover { text-decoration: underline; }
    )")).arg(tint.name(), Theme::panelBorder(), text.name(), muted.name(),
             accent.name(), Theme::hoverOverlay()));
}

// ============================== Sininho ==============================

RecadosBell::RecadosBell(QWidget* parent)
    : QToolButton(parent)
{
    setObjectName(QStringLiteral("recadosBell"));
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setAutoRaise(true);
    setText(tr("Recados"));
    setToolTip(tr("Ler os recados do autor"));
    setIconSize(QSize(16, 16));
    setFixedHeight(24);
    connect(this, &QToolButton::clicked, this, &RecadosBell::readRequested);
    RecadosFeed* feed = RecadosFeed::instance();
    connect(feed, &RecadosFeed::changed, this, &RecadosBell::refresh);
    connect(feed, &RecadosFeed::stateChanged, this, &RecadosBell::refresh);
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, &RecadosBell::refresh);
    refresh();
}

void RecadosBell::refresh()
{
    RecadosFeed* feed = RecadosFeed::instance();
    const QList<Recado> list = feed->recados();
    if (list.isEmpty()) { hide(); return; }
    bool unread = false;
    for (const Recado& r : list) unread = unread || !feed->isRead(r.id);
    const QColor muted = Theme::toColor(Theme::textMuted());
    const QColor accent = Theme::toColor(Theme::accentDefault());
    const qreal dpr = devicePixelRatioF();
    QPixmap pm = glyphIcon(Glyph::Bell, muted, 16, dpr).pixmap(16, 16);
    if (unread) {
        // o pontinho de "tem recado novo", com um anel na cor do fundo
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(Theme::toColor(Theme::chromeBackground()), 1.5));
        p.setBrush(accent);
        p.drawEllipse(QRectF(9.5, 1, 6, 6));
    }
    setIcon(QIcon(pm));
    setStyleSheet(QStringLiteral(
        "QToolButton#recadosBell { background: transparent; border: none; color: %1; font-size: 11.5px; padding: 0 2px; }"
        "QToolButton#recadosBell:hover { color: %2; text-decoration: underline; }")
        .arg(muted.name(), Theme::textPrimary()));
    show();
}

// ============================== Leitor ==============================

RecadosDialog::RecadosDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("recadosDialog"));
    setWindowTitle(tr("Recados"));
    setModal(true);
    setFixedWidth(640);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("recScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* inner = new QWidget(scroll);
    inner->setObjectName(QStringLiteral("recInner"));
    auto* col = new QVBoxLayout(inner);
    col->setContentsMargins(30, 18, 30, 24);
    col->setSpacing(0);

    RecadosFeed* feed = RecadosFeed::instance();
    const QList<Recado> list = feed->recados();
    const int textW = 640 - 60 - 14;
    for (int i = 0; i < list.size(); ++i) {
        const Recado& r = list.at(i);
        if (i > 0) {
            col->addSpacing(18);
            auto* sep = new QFrame(inner);
            sep->setObjectName(QStringLiteral("recSep"));
            sep->setFixedHeight(1);
            col->addWidget(sep);
            col->addSpacing(18);
        }
        auto* eyebrow = new QLabel(inner);
        eyebrow->setObjectName(QStringLiteral("recEyebrowBig"));
        const QString when = r.date.isValid() ? longDate(r.date) : QString();
        const bool fresh = !feed->isRead(r.id);
        eyebrow->setText(fresh ? tr("Novo") + (when.isEmpty() ? QString() : QStringLiteral("  ·  ") + when) : when);
        eyebrow->setProperty("fresh", fresh);
        col->addWidget(eyebrow);
        col->addSpacing(4);

        auto* title = new QLabel(r.title, inner);
        title->setObjectName(QStringLiteral("recTitleBig"));
        title->setWordWrap(true);
        col->addWidget(title);
        col->addSpacing(10);

        if (!r.imageUrl.isEmpty()) {
            auto* pic = new QLabel(inner);
            pic->setObjectName(QStringLiteral("recPic"));
            pic->hide();
            col->addWidget(pic);
            col->addSpacing(12);
            QNetworkRequest req{QUrl(r.imageUrl)};
            req.setTransferTimeout(15000);
            QNetworkReply* reply = feed->network()->get(req);
            QPointer<QLabel> guard(pic);
            connect(reply, &QNetworkReply::finished, this, [reply, guard, textW]() {
                reply->deleteLater();
                if (!guard || reply->error() != QNetworkReply::NoError) return;
                QImage img;
                if (!img.loadFromData(reply->readAll())) return;
                const qreal dpr = guard->devicePixelRatioF();
                QImage scaled = img.scaledToWidth(int(textW * dpr), Qt::SmoothTransformation);
                if (scaled.height() > 300 * dpr)
                    scaled = scaled.copy(0, (scaled.height() - int(300 * dpr)) / 2, scaled.width(), int(300 * dpr));
                QPixmap pm = QPixmap::fromImage(scaled);
                pm.setDevicePixelRatio(dpr);
                guard->setPixmap(pm);
                guard->show();
            });
        }

        auto* body = new QLabel(r.body, inner);
        body->setObjectName(QStringLiteral("recBodyBig"));
        body->setWordWrap(true);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        col->addWidget(body);
        col->addSpacing(10);

        auto* foot = new QHBoxLayout();
        foot->setSpacing(10);
        auto* sig = new QLabel(QStringLiteral("— P.H. Lobato"), inner);
        sig->setObjectName(QStringLiteral("recSig"));
        foot->addWidget(sig);
        foot->addStretch(1);
        if (!r.linkUrl.isEmpty()) {
            auto* go = new QPushButton(r.linkLabel.isEmpty() ? tr("Abrir") : r.linkLabel, inner);
            go->setObjectName(QStringLiteral("recLink"));
            go->setCursor(Qt::PointingHandCursor);
            const QString url = r.linkUrl;
            connect(go, &QPushButton::clicked, this, [url]() { QDesktopServices::openUrl(QUrl(url)); });
            foot->addWidget(go);
        }
        col->addLayout(foot);
        if (feed->commentsEnabled()) {
            col->addSpacing(10);
            col->addWidget(buildComments(r, inner));
        }
    }
    col->addStretch(1);
    scroll->setWidget(inner);
    root->addWidget(scroll, 1);

    auto* bottom = new QHBoxLayout();
    bottom->setContentsMargins(30, 12, 30, 16);
    bottom->addStretch(1);
    auto* close = new QPushButton(tr("Fechar"), this);
    close->setObjectName(QStringLiteral("recClose"));
    close->setCursor(Qt::PointingHandCursor);
    close->setDefault(true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    bottom->addWidget(close);
    root->addLayout(bottom);

    // Altura do conteúdo, até um teto; passou disso, rola.
    inner->adjustSize();
    const int maxH = parent ? int(parent->window()->height() * 0.85) : 760;
    resize(640, qMin(inner->sizeHint().height() + 70, qMax(360, maxH)));

    // Abriu o leitor = leu tudo (a faixa troca "Recado novo" por "Recado").
    feed->markAllRead();
    feed->fetchComments();
    feed->fetchReactions();

    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, &RecadosDialog::applyTheme);
}

QWidget* RecadosDialog::buildComments(const Recado& r, QWidget* parent)
{
    RecadosFeed* feed = RecadosFeed::instance();
    const QString id = r.id;
    auto* box = new QWidget(parent);
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(10);

    auto* toggle = new QPushButton(box);
    toggle->setObjectName(QStringLiteral("recCmtToggle"));
    toggle->setCursor(Qt::PointingHandCursor);
    toggle->setFlat(true);
    auto* toggleRow = new QHBoxLayout();
    toggleRow->setSpacing(6);
    const QString tips[kReacoes] = { tr("Amei"), tr("Que demais"), tr("Mandou bem"), tr("Kkkkk") };
    // chave do servidor (recado ou "c-<id>") -> os 4 botões dela; os dos comentários renascem a cada rebuild
    auto reactSets = std::make_shared<QHash<QString, QList<QToolButton*>>>();
    auto paintSet = [feed, box](const QString& key, const QList<QToolButton*>& bs) {
        static const Glyph glyphs[kReacoes] = { Glyph::Heart, Glyph::Fire, Glyph::Clap, Glyph::Laugh };
        const Reacoes r = feed->reactions(key);
        const QColor accent = Theme::toColor(Theme::accentDefault());
        const QColor muted = Theme::toColor(Theme::textMuted());
        const qreal dpr = box->devicePixelRatioF();
        for (int k = 0; k < kReacoes && k < bs.size(); ++k) {
            QToolButton* b = bs.at(k);
            const int sz = b->iconSize().width();
            b->setChecked(r.mine[k]);
            b->setIcon(glyphIcon(glyphs[k], r.mine[k] ? accent : muted, sz, dpr, r.mine[k]));
            b->setText(r.count[k] > 0 ? QString::number(r.count[k]) : QString());
            b->setToolButtonStyle(r.count[k] > 0 ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);
        }
    };
    auto makeReacts = [feed, box, tips, reactSets, paintSet](const QString& key, QWidget* host, QHBoxLayout* row, bool small) {
        QList<QToolButton*> bs;
        for (int k = 0; k < kReacoes; ++k) {
            auto* b = new QToolButton(host);
            b->setObjectName(small ? QStringLiteral("recReactSm") : QStringLiteral("recReact"));
            b->setCursor(Qt::PointingHandCursor);
            b->setCheckable(true);
            b->setToolTip(tips[k]);
            b->setIconSize(small ? QSize(15, 15) : QSize(18, 18));
            b->setFixedHeight(small ? 22 : 26);
            connect(b, &QToolButton::clicked, box, [feed, key, k]() { feed->toggleReaction(key, Reacao(k)); });
            row->addWidget(b);
            bs << b;
        }
        reactSets->insert(key, bs);
        paintSet(key, bs);
    };
    makeReacts(id, box, toggleRow, false);
    auto repaintAll = [reactSets, paintSet]() { for (auto it = reactSets->begin(); it != reactSets->end(); ++it) paintSet(it.key(), it.value()); };
    connect(feed, &RecadosFeed::reactionsChanged, box, [reactSets, paintSet, repaintAll](const QString& rid) {
        if (rid.isEmpty()) { repaintAll(); return; }
        if (reactSets->contains(rid)) paintSet(rid, reactSets->value(rid));
    });
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, box, repaintAll);
    toggleRow->addSpacing(10);
    toggleRow->addWidget(toggle);
    toggleRow->addStretch(1);
    v->addLayout(toggleRow);

    auto* panel = new QFrame(box);
    panel->setObjectName(QStringLiteral("recCmtPanel"));
    panel->hide();
    auto* pv = new QVBoxLayout(panel);
    pv->setContentsMargins(16, 14, 16, 14);
    pv->setSpacing(12);
    auto* listHost = new QWidget(panel);
    auto* lv = new QVBoxLayout(listHost);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(4);
    pv->addWidget(listHost);

    // depois de publicar, o formulário fecha e fica só "Comentar de novo"
    auto* again = new QPushButton(tr("Comentar de novo"), panel);
    again->setObjectName(QStringLiteral("recCmtToggle"));
    again->setCursor(Qt::PointingHandCursor);
    again->setFlat(true);
    again->hide();
    auto* againRow = new QHBoxLayout();
    againRow->addWidget(again);
    againRow->addStretch(1);
    pv->addLayout(againRow);

    auto* form = new QWidget(panel);
    auto* fv = new QVBoxLayout(form);
    fv->setContentsMargins(0, 0, 0, 0);
    fv->setSpacing(12);
    pv->addWidget(form);

    auto* sep = new QFrame(form);
    sep->setObjectName(QStringLiteral("recSep"));
    sep->setFixedHeight(1);
    fv->addWidget(sep);

    auto replyTo = std::make_shared<qint64>(0);
    auto* replyBar = new QWidget(form);
    auto* rb = new QHBoxLayout(replyBar);
    rb->setContentsMargins(0, 0, 0, 0);
    rb->setSpacing(6);
    auto* replyLabel = new QLabel(replyBar);
    replyLabel->setObjectName(QStringLiteral("recCmtReplying"));
    rb->addWidget(replyLabel);
    auto* replyCancel = iconButton(replyBar, tr("Cancelar a resposta"));
    replyCancel->setIcon(glyphIcon(Glyph::Close, Theme::toColor(Theme::textMuted()), 14, devicePixelRatioF()));
    rb->addWidget(replyCancel);
    rb->addStretch(1);
    replyBar->hide();
    fv->addWidget(replyBar);
    auto clearReply = [replyTo, replyBar]() { *replyTo = 0; replyBar->hide(); };
    connect(replyCancel, &QToolButton::clicked, box, clearReply);

    auto* nameEdit = new QLineEdit(form);
    nameEdit->setObjectName(QStringLiteral("recCmtNameEdit"));
    nameEdit->setPlaceholderText(tr("Seu nome"));
    nameEdit->setMaxLength(40);
    nameEdit->setText(QSettings().value(QStringLiteral("recados/nome")).toString());
    fv->addWidget(nameEdit);
    auto* bodyEdit = new QPlainTextEdit(form);
    bodyEdit->setObjectName(QStringLiteral("recCmtEdit"));
    bodyEdit->setPlaceholderText(tr("Escreva um comentário"));
    bodyEdit->setFixedHeight(84);
    bodyEdit->setTabChangesFocus(true);
    fv->addWidget(bodyEdit);

    auto* row = new QHBoxLayout();
    row->setSpacing(10);
    auto* status = new QLabel(form);
    status->setObjectName(QStringLiteral("recCmtStatus"));
    status->setWordWrap(true);
    row->addWidget(status, 1);
    auto* count = new QLabel(form);
    count->setObjectName(QStringLiteral("recCmtStatus"));
    count->hide();
    row->addWidget(count);
    auto* send = new QPushButton(tr("Enviar"), form);
    send->setObjectName(QStringLiteral("recCmtSend"));
    send->setCursor(Qt::PointingHandCursor);
    row->addWidget(send);
    fv->addLayout(row);
    v->addWidget(panel);

    const int kMax = 1000;
    connect(bodyEdit, &QPlainTextEdit::textChanged, box, [bodyEdit, count, kMax]() {
        const QString t = bodyEdit->toPlainText();
        if (t.size() > kMax) {
            const QSignalBlocker block(bodyEdit);
            bodyEdit->setPlainText(t.left(kMax));
            bodyEdit->moveCursor(QTextCursor::End);
        }
        const int n = qMin(int(t.size()), kMax);
        count->setVisible(n > kMax - 200);
        count->setText(QStringLiteral("%1/%2").arg(n).arg(kMax));
    });

    auto setStatus = [status](const QString& text, bool err) {
        status->setText(text);
        status->setProperty("err", err);
        status->style()->unpolish(status);
        status->style()->polish(status);
    };

    auto rebuild = [this, feed, id, lv, listHost, toggle, panel, box, reactSets, makeReacts, replyTo, replyBar, replyLabel, form, again, bodyEdit]() {
        const std::function<void(QLayout*)> clear = [&clear](QLayout* l) {
            while (QLayoutItem* it = l->takeAt(0)) {
                if (it->widget()) it->widget()->deleteLater();
                if (it->layout()) clear(it->layout());
                delete it;
            }
        };
        clear(lv);
        for (auto it = reactSets->begin(); it != reactSets->end();) it = (it.key() == id) ? std::next(it) : reactSets->erase(it);
        const QList<Comentario> list = feed->comments(id);
        const QString arrow = panel->isVisible() ? QStringLiteral("  \u25B4") : QStringLiteral("  \u25BE");
        toggle->setText((list.isEmpty() ? tr("Comentar") : tr("Comentários (%1)").arg(list.size())) + arrow);
        if (list.isEmpty()) {
            auto* empty = new QLabel(feed->commentsLoaded() ? tr("Ninguém comentou ainda. Seja a primeira pessoa.")
                                                            : tr("Carregando comentários…"), listHost);
            empty->setObjectName(QStringLiteral("recCmtEmpty"));
            empty->setWordWrap(true);
            lv->addWidget(empty);
            return;
        }
        // ordem: cada comentário do recado seguido das respostas dele (as respostas sem "pai" vivo vão no fim)
        QList<Comentario> ordered;
        QSet<qint64> tops;
        for (const Comentario& c : list) if (c.parent == 0) tops.insert(c.id);
        for (const Comentario& c : list) {
            if (c.parent != 0) continue;
            ordered << c;
            for (const Comentario& rr : list) if (rr.parent == c.id) ordered << rr;
        }
        for (const Comentario& c : list) if (c.parent != 0 && !tops.contains(c.parent)) ordered << c;
        for (const Comentario& c : ordered) {
            const bool isReply = c.parent != 0;
            auto* item = new QWidget(listHost);
            item->setObjectName(isReply ? QStringLiteral("recCmtReply") : QStringLiteral("recCmtItem"));
            item->setAttribute(Qt::WA_StyledBackground, true);
            auto* iv = new QVBoxLayout(item);
            iv->setContentsMargins(isReply ? 12 : 0, 0, 0, 0);
            iv->setSpacing(4);
            auto* indent = new QHBoxLayout();
            indent->setContentsMargins(0, 0, 0, 0);
            if (isReply) indent->addSpacing(18);
            indent->addWidget(item, 1);
            lv->addLayout(indent);
            auto* headRow = new QHBoxLayout();
            headRow->setSpacing(8);
            auto* head = new QLabel(item);
            head->setObjectName(QStringLiteral("recCmtHead"));
            head->setTextFormat(Qt::RichText);
            // o autor: nome na cor de destaque + pílula "AUTOR" preenchida (de longe dá pra ver que é ele)
            head->setProperty("author", c.author);
            head->setText(QStringLiteral("<b>%1</b>").arg(c.name.toHtmlEscaped()));
            headRow->addWidget(head);
            if (c.author) {
                auto* badge = new QLabel(tr("AUTOR"), item);
                badge->setObjectName(QStringLiteral("recCmtBadge"));
                headRow->addWidget(badge);
            }
            auto* when = new QLabel(longDate(c.at.toLocalTime().date()), item);
            when->setObjectName(QStringLiteral("recCmtWhen"));
            headRow->addWidget(when);
            if (feed->isAuthor()) {   // só no PC do autor (tem a chave)
                auto* del = new QPushButton(tr("apagar"), item);
                del->setObjectName(QStringLiteral("recCmtDel"));
                del->setCursor(Qt::PointingHandCursor);
                del->setFlat(true);
                const qint64 cid = c.id;
                connect(del, &QPushButton::clicked, box, [feed, id, cid]() { feed->deleteComment(id, cid); });
                headRow->addWidget(del);
                if (!c.author) {
                    auto* banBtn = new QPushButton(tr("banir"), item);
                    banBtn->setObjectName(QStringLiteral("recCmtDel"));
                    banBtn->setCursor(Qt::PointingHandCursor);
                    banBtn->setFlat(true);
                    const QString who = c.name;
                    connect(banBtn, &QPushButton::clicked, box, [this, feed, id, cid, who]() {
                        const auto ans = QMessageBox::question(this, tr("Banir"),
                            tr("Banir %1? O comentário (e as respostas dele) some, e essa pessoa não poderá mais comentar nem reagir.").arg(who),
                            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
                        if (ans == QMessageBox::Yes) feed->banComment(id, cid);
                    });
                    headRow->addWidget(banBtn);
                }
            }
            headRow->addStretch(1);
            iv->addLayout(headRow);
            auto* body = new QLabel(c.body, item);
            body->setObjectName(QStringLiteral("recCmtBody"));
            body->setTextFormat(Qt::PlainText);
            body->setWordWrap(true);
            body->setTextInteractionFlags(Qt::TextSelectableByMouse);
            iv->addWidget(body);
            auto* acts = new QHBoxLayout();
            acts->setContentsMargins(0, 0, 0, 0);
            acts->setSpacing(2);
            makeReacts(RecadosFeed::commentKey(c.id), item, acts, true);
            acts->addSpacing(10);
            auto* reply = new QPushButton(tr("Responder"), item);
            reply->setObjectName(QStringLiteral("recCmtDel"));
            reply->setCursor(Qt::PointingHandCursor);
            reply->setFlat(true);
            const qint64 target = isReply ? c.parent : c.id;   // um nível só: responde na mesma conversa
            const QString who = c.name;
            connect(reply, &QPushButton::clicked, box, [this, replyTo, replyBar, replyLabel, form, again, bodyEdit, target, who]() {
                *replyTo = target;
                replyLabel->setText(tr("Respondendo a %1").arg(who));
                replyBar->show();
                again->hide();
                form->show();
                bodyEdit->setFocus();
            });
            acts->addWidget(reply);
            acts->addStretch(1);
            iv->addLayout(acts);
            lv->addSpacing(8);
        }
    };
    rebuild();
    connect(feed, &RecadosFeed::commentsChanged, box, rebuild);
    connect(again, &QPushButton::clicked, box, [form, again, bodyEdit]() {
        again->hide();
        form->show();
        bodyEdit->setFocus();
    });
    connect(toggle, &QPushButton::clicked, box, [panel, rebuild, bodyEdit, nameEdit, form, again, clearReply]() {
        panel->setVisible(!panel->isVisible());
        form->show();
        again->hide();
        clearReply();
        rebuild();
        if (panel->isVisible()) (nameEdit->text().isEmpty() ? static_cast<QWidget*>(nameEdit) : bodyEdit)->setFocus();
    });

    connect(send, &QPushButton::clicked, box, [this, feed, id, nameEdit, bodyEdit, send, setStatus, replyTo]() {
        const QString name = nameEdit->text().simplified();
        const QString body = bodyEdit->toPlainText().trimmed();
        if (name.isEmpty() || body.isEmpty()) { setStatus(tr("Escreva o seu nome e o comentário."), true); return; }
        if (!QSettings().value(QStringLiteral("recados/rulesOk")).toBool() && !confirmRules()) return;   // o texto fica lá
        QSettings().setValue(QStringLiteral("recados/nome"), name);
        send->setEnabled(false);
        send->setText(tr("Enviando…"));
        setStatus(QString(), false);
        feed->postComment(id, name, body, *replyTo);
    });
    connect(feed, &RecadosFeed::commentPosted, box, [this, id, bodyEdit, send, setStatus, form, again, clearReply](const QString& rid) {
        if (rid != id) return;
        clearReply();
        bodyEdit->clear();
        send->setEnabled(true);
        send->setText(tr("Enviar"));
        setStatus(QString(), false);
        form->hide();       // o comentário já aparece na lista
        again->show();
    });
    connect(feed, &RecadosFeed::adminKeyRejected, box, [this, setStatus]() { setStatus(tr("Chave errada."), true); });
    connect(feed, &RecadosFeed::commentFailed, box, [this, feed, id, send, setStatus](const QString& rid, const QString& why) {
        if (rid != id) return;
        send->setEnabled(true);
        send->setText(tr("Enviar"));
        QString msg;
        if (why == QLatin1String("no-links")) msg = tr("Links não são aceitos nos comentários.");
        else if (why == QLatin1String("reserved")) {
            // é o autor? A chave uma vez, guardada no PC dele, e o comentário vai de novo
            const bool hadKey = feed->isAuthor();
            if (hadKey) feed->setAdminKey(QString());
            bool ok = false;
            const QString key = QInputDialog::getText(this, tr("Assinatura do autor"),
                (hadKey ? tr("Chave errada.") + QStringLiteral("\n\n") : QString())
                    + tr("Esse nome é reservado ao autor do Qenna. Se você é o autor, digite a chave:"),
                QLineEdit::Password, QString(), &ok);
            if (ok && !key.trimmed().isEmpty()) {
                feed->setAdminKey(key.trimmed());
                send->click();
                return;
            }
            msg = tr("Esse nome é reservado ao autor do Qenna.");
        }
        else if (why == QLatin1String("slow-down")) msg = tr("Muitos comentários seguidos. Tente de novo daqui a alguns minutos.");
        else if (why == QLatin1String("empty")) msg = tr("Escreva o seu nome e o comentário.");
        else if (why == QLatin1String("too-long")) msg = tr("O comentário passou do tamanho máximo.");
        else if (why == QLatin1String("gone")) msg = tr("Esse comentário foi apagado; a resposta não foi enviada.");
        else if (why == QLatin1String("banned")) msg = tr("Você não pode mais comentar aqui.");
        else msg = tr("Não foi possível enviar. Confira a internet e tente de novo.");
        setStatus(msg, true);
    });
    return box;
}

bool RecadosDialog::confirmRules()
{
    QDialog dlg(this);
    dlg.setObjectName(QStringLiteral("recRules"));
    dlg.setWindowTitle(tr("Antes de comentar"));
    dlg.setModal(true);
    dlg.setFixedWidth(520);
    auto* col = new QVBoxLayout(&dlg);
    col->setContentsMargins(30, 26, 30, 22);
    col->setSpacing(14);
    auto* title = new QLabel(tr("Antes de comentar e interagir, lembre-se:"), &dlg);
    title->setObjectName(QStringLiteral("recRulesTitle"));
    title->setWordWrap(true);
    col->addWidget(title);
    struct Rule { QString head, body; };
    const Rule rules[] = {
        { tr("Respeito não é opcional, é obrigatório."),
          tr("Comentários ofensivos serão apagados e o usuário pode ser banido permanentemente de comentar, a depender do contexto.") },
        { tr("Discurso de ódio é CRIME aqui."),
          tr("Racismo, xenofobia, misoginia ou homofobia é ban direto e SECO. Sem segunda chance.") },
        { tr("Não comente links externos ou qualquer coisa relacionada."), QString() },
        { tr("Não tem problema falar palavrão, mas tenhamos classe."), QString() },
        { tr("Seja ativo!"), tr("Interaja e comente sempre que possível! Eu e a comunidade agradecemos!") },
    };
    int n = 0;
    for (const Rule& r : rules) {
        auto* row = new QHBoxLayout();
        row->setSpacing(12);
        auto* num = new QLabel(QString::number(++n), &dlg);
        num->setObjectName(QStringLiteral("recRulesNum"));
        num->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        num->setFixedWidth(22);
        row->addWidget(num, 0, Qt::AlignTop);
        auto* text = new QLabel(&dlg);
        text->setObjectName(QStringLiteral("recRulesText"));
        text->setWordWrap(true);
        text->setTextFormat(Qt::RichText);
        text->setText(QStringLiteral("<b>%1</b>").arg(r.head.toHtmlEscaped())
                      + (r.body.isEmpty() ? QString() : QStringLiteral("<br>") + r.body.toHtmlEscaped()));
        row->addWidget(text, 1);
        col->addLayout(row);
    }
    auto* sig = new QLabel(QStringLiteral("— P.H. Lobato"), &dlg);
    sig->setObjectName(QStringLiteral("recSig"));
    col->addWidget(sig);
    col->addSpacing(4);
    auto* btns = new QHBoxLayout();
    btns->setSpacing(10);
    auto* cancel = new QPushButton(tr("Cancelar e não comentar"), &dlg);
    cancel->setObjectName(QStringLiteral("recRulesCancel"));
    cancel->setCursor(Qt::PointingHandCursor);
    auto* agree = new QPushButton(tr("Entendi e concordo com tudo!"), &dlg);
    agree->setObjectName(QStringLiteral("recClose"));
    agree->setCursor(Qt::PointingHandCursor);
    agree->setDefault(true);
    btns->addStretch(1);
    btns->addWidget(cancel);
    btns->addWidget(agree);
    col->addLayout(btns);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(agree, &QPushButton::clicked, &dlg, &QDialog::accept);
    const QColor accent = Theme::toColor(Theme::accentDefault());
    const QColor ink = accent.lightnessF() > 0.62 ? QColor(QStringLiteral("#15171c")) : QColor(QStringLiteral("#f7f8fa"));
    dlg.setStyleSheet(Theme::qss(QStringLiteral(R"(
        QDialog#recRules { background: %1; }
        QLabel#recRulesTitle { color: %3; font-family: 'Crimson Pro', Georgia, serif; font-size: 22px; font-weight: 600; }
        QLabel#recRulesNum { color: %5; font-family: 'Crimson Pro', Georgia, serif; font-size: 20px; font-weight: 700; }
        QLabel#recRulesText { color: %2; font-family: 'Crimson Pro', Georgia, serif; font-size: 16px; }
        QLabel#recSig { color: %5; font-family: 'Crimson Pro', Georgia, serif; font-size: 16px; font-style: italic; }
        QPushButton#recRulesCancel { background: transparent; color: %4; border: 1px solid %6; border-radius: @radius-control;
                                     padding: 7px 16px; font-size: 13px; }
        QPushButton#recRulesCancel:hover { color: %3; border-color: %4; }
        QPushButton#recClose { background: %5; color: %7; border: 1px solid %5; border-radius: @radius-control;
                               padding: 7px 18px; font-size: 13px; font-weight: 600; }
        QPushButton#recClose:hover { border-color: %3; }
    )")).arg(Theme::panelBackground(), Theme::textPrimary(), Theme::textBright(), Theme::textMuted(),
             accent.name(), Theme::panelBorder(), ink.name()));
    if (dlg.exec() != QDialog::Accepted) return false;
    QSettings().setValue(QStringLiteral("recados/rulesOk"), true);
    return true;
}

void RecadosDialog::applyTheme()
{
    const QColor accent = Theme::toColor(Theme::accentDefault());
    const QColor ink = accent.lightnessF() > 0.62 ? QColor(QStringLiteral("#15171c")) : QColor(QStringLiteral("#f7f8fa"));
    setStyleSheet(Theme::qss(QStringLiteral(R"(
        #recadosDialog, QWidget#recInner, QScrollArea#recScroll { background: %1; }
        QLabel#recEyebrowBig { color: %4; font-size: 11px; font-weight: 600; letter-spacing: 1px; }
        QLabel#recEyebrowBig[fresh="true"] { color: %5; }
        QLabel#recTitleBig { color: %3; font-family: 'Crimson Pro', Georgia, serif; font-size: 24px; font-weight: 600; }
        QLabel#recBodyBig { color: %2; font-family: 'Crimson Pro', Georgia, serif; font-size: 17px; }
        QLabel#recSig { color: %5; font-family: 'Crimson Pro', Georgia, serif; font-size: 16px; font-style: italic; }
        QFrame#recSep { background: %6; border: none; }
        QPushButton#recLink { background: transparent; color: %3; border: 1px solid %6; border-radius: @radius-control;
                              padding: 6px 14px; font-size: 13px; }
        QPushButton#recLink:hover { border-color: %5; }
        QPushButton#recClose { background: %5; color: %7; border: 1px solid %5; border-radius: @radius-control;
                               padding: 7px 20px; font-size: 13px; font-weight: 600; }
        QPushButton#recClose:hover { border-color: %3; }
        QPushButton#recCmtToggle { background: transparent; border: none; padding: 0px; color: %5;
                                   font-size: 13px; font-weight: 600; text-align: left; }
        QPushButton#recCmtToggle:hover { text-decoration: underline; }
        QToolButton#recReact { background: transparent; color: %4; border: 1px solid %6; border-radius: 13px;
                               padding: 1px 8px; font-size: 12px; font-weight: 600; }
        QToolButton#recReact:hover { border-color: %5; }
        QToolButton#recReact:checked { color: %5; border-color: %5; background: %10; }
        QToolButton#recReactSm { background: transparent; color: %4; border: none; border-radius: 11px;
                                 padding: 0px 5px; font-size: 11px; font-weight: 600; }
        QToolButton#recReactSm:hover { background: %9; }
        QToolButton#recReactSm:checked { color: %5; }
        QWidget#recCmtReply { border-left: 2px solid %6; background: transparent; }
        QLabel#recCmtReplying { color: %5; font-size: 12px; font-weight: 600; background: transparent; }
        QFrame#recCmtPanel { background: %8; border: 1px solid %6; border-radius: @radius-control; }
        QLabel#recCmtHead { color: %3; font-family: 'Crimson Pro', Georgia, serif; font-size: 15px; background: transparent; }
        QLabel#recCmtHead[author="true"] { color: %5; }
        QLabel#recCmtBadge { background: %5; color: %7; border-radius: 8px; padding: 1px 8px;
                             font-size: 10px; font-weight: 700; letter-spacing: 1px; }
        QLabel#recCmtWhen { color: %4; font-family: 'Crimson Pro', Georgia, serif; font-size: 12px; background: transparent; }
        QLabel#recCmtBody { color: %2; font-family: 'Crimson Pro', Georgia, serif; font-size: 16px; background: transparent;
                            margin-bottom: 8px; }
        QLabel#recCmtEmpty { color: %4; font-size: 13px; background: transparent; }
        QLabel#recCmtStatus { color: %4; font-size: 12px; background: transparent; }
        QLabel#recCmtStatus[err="true"] { color: %5; }
        QLineEdit#recCmtNameEdit, QPlainTextEdit#recCmtEdit { background: %1; color: %2; border: 1px solid %6;
                                   border-radius: @radius-control; padding: 6px 8px; font-size: 13px; }
        QLineEdit#recCmtNameEdit:focus, QPlainTextEdit#recCmtEdit:focus { border-color: %5; }
        QPushButton#recCmtSend { background: transparent; color: %5; border: 1px solid %5; border-radius: @radius-control;
                                 padding: 6px 16px; font-size: 13px; font-weight: 600; }
        QPushButton#recCmtSend:hover { background: %9; }
        QPushButton#recCmtSend:disabled { color: %4; border-color: %6; }
        QPushButton#recCmtDel { background: transparent; border: none; padding: 0px; color: %4; font-size: 11px;
                                text-decoration: underline; }
        QPushButton#recCmtDel:hover { color: %5; }
    )")).arg(Theme::panelBackground(), Theme::textPrimary(), Theme::textBright(), Theme::textMuted(),
             accent.name(), Theme::panelBorder(), ink.name(),
             mix(Theme::toColor(Theme::panelBackground()), Theme::toColor(Theme::textPrimary()), 0.04).name(),
             Theme::hoverOverlay())
         .arg(mix(Theme::toColor(Theme::panelBackground()), accent, 0.12).name()));
}
