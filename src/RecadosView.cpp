#include "RecadosView.h"

#include "Recados.h"
#include "Theme.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QStyle>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

// Uma linha só, com "…" quando não cabe (o QLabel corta seco).
class ElidedLabel : public QLabel {
public:
    using QLabel::QLabel;
    QSize minimumSizeHint() const override { return { 40, QLabel::minimumSizeHint().height() }; }
    QSize sizeHint() const override { return { 200, QLabel::sizeHint().height() }; }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setPen(palette().color(QPalette::WindowText));
        const QString t = fontMetrics().elidedText(text(), Qt::ElideRight, width());
        p.drawText(rect(), Qt::AlignVCenter | Qt::AlignLeft, t);
    }
};

namespace {

QColor mix(const QColor& a, const QColor& b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

enum class Glyph { Pin, Left, Right, Close };

// Ícones desenhados (sem caractere de fonte, que no Linux pode faltar).
QIcon glyphIcon(Glyph g, const QColor& c, int size, qreal dpr)
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
    case Glyph::Close:
        p.drawLine(QPointF(7, 7), QPointF(17, 17));
        p.drawLine(QPointF(17, 7), QPointF(7, 17));
        break;
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

    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, &RecadosDialog::applyTheme);
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
    )")).arg(Theme::panelBackground(), Theme::textPrimary(), Theme::textBright(), Theme::textMuted(),
             accent.name(), Theme::panelBorder(), ink.name()));
}
