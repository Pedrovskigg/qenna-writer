#include "SilhouettePicker.h"

#include "TimelineTracksTypes.h"

#include <QBuffer>
#include <QButtonGroup>
#include <QColor>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QRandomGenerator>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Tracks;

namespace {

constexpr int kCount = 19;           // por gênero
constexpr int kCell = 68;            // quadradinho da grade
constexpr int kCols = 5;
constexpr int kOut = 400;            // o tamanho da foto guardada (o mesmo do recorte de imagem)
// os mesmos tons do Lab das silhuetas
constexpr int kHueDeg[Silhouettes::kHues] = { 212, 6, 146, 34, 268, 186, 330, 52, 100, 236 };

QColor hsl(int deg, qreal s, qreal l) { return QColor::fromHslF(deg / 360.0, s, l); }
QColor mixc(const QColor& a, const QColor& b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

} // namespace

QStringList Silhouettes::ids(bool male)
{
    QStringList out;
    for (int i = 1; i <= kCount; ++i)
        out << QStringLiteral("%1%2").arg(male ? QLatin1Char('m') : QLatin1Char('f')).arg(i, 2, 10, QLatin1Char('0'));
    return out;
}

int Silhouettes::hueFor(const QString& seed)
{
    if (seed.trimmed().isEmpty()) return int(QRandomGenerator::global()->bounded(kHues));
    return int(qHash(seed.trimmed().toLower()) % kHues);
}

QImage Silhouettes::render(const QString& id, int hue, int px)
{
    QImage src(QStringLiteral(":/silhouettes/%1.png").arg(id));
    if (src.isNull()) return QImage();
    src = src.convertToFormat(QImage::Format_ARGB32);
    const int deg = kHueDeg[qBound(0, hue, kHues - 1)];
    const QColor bg = hsl(deg, 0.42, 0.86), fg = hsl(deg, 0.36, 0.40);
    // cabelo mais fundo que a roupa, pele um pouco mais clara: a cabeça sai do corpo
    const QColor tones[4] = { mixc(fg, Qt::black, 0.32), mixc(fg, bg, 0.16), fg, mixc(fg, Qt::black, 0.40) };
    QImage out(src.size(), QImage::Format_RGB32);
    for (int y = 0; y < src.height(); ++y) {
        const QRgb* s = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        QRgb* o = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            const int part = qRed(s[x]);
            const qreal line = qGreen(s[x]) / 255.0 * 0.8, a = qAlpha(s[x]) / 255.0;
            const QColor& t = tones[part < 95 ? 0 : part < 165 ? 1 : part < 228 ? 2 : 3];
            qreal r = t.redF(), g = t.greenF(), b = t.blueF();
            r += (bg.redF() - r) * line; g += (bg.greenF() - g) * line; b += (bg.blueF() - b) * line;
            o[x] = qRgb(qRound((bg.redF() + (r - bg.redF()) * a) * 255), qRound((bg.greenF() + (g - bg.greenF()) * a) * 255),
                        qRound((bg.blueF() + (b - bg.blueF()) * a) * 255));
        }
    }
    return px == out.width() ? out : out.scaled(px, px, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

SilhouettePickerDialog::SilhouettePickerDialog(QWidget* parent, bool male, const QString& seed)
    : SheetDialog(parent, kCols * kCell + (kCols - 1) * 6 + 34), m_male(male)
{
    setEyebrow(tr("Definir silhueta"));
    QWidget* c = card();
    const Palette pal = Palette::onPage();
    m_hue = Silhouettes::hueFor(seed);
    const QStringList pool = Silhouettes::ids(male);
    m_id = pool.value(int(qHash(seed.trimmed().toLower()) % uint(pool.size())));

    m_sex = new SheetChoice({ tr("Masculinas"), tr("Femininas") }, c);
    m_sex->setCurrentIndex(male ? 0 : 1);
    connect(m_sex, &SheetChoice::activated, this, [this](int i) {
        m_male = i == 0;
        const QStringList ids = Silhouettes::ids(m_male);
        if (!ids.contains(m_id)) m_id = ids.first();
        rebuildGrid();
    });
    body()->addWidget(m_sex);

    m_grid = new QGridLayout;
    m_grid->setSpacing(6);
    m_cells = new QButtonGroup(this);
    m_cells->setExclusive(true);
    body()->addLayout(m_grid);

    auto* g = new QVBoxLayout;
    g->setSpacing(7);
    g->addWidget(sectionLabel(tr("Cor"), c));
    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    m_swatches = new QButtonGroup(this);
    m_swatches->setExclusive(true);
    for (int i = 0; i < Silhouettes::kHues; ++i) {
        auto* b = new QToolButton(c);
        b->setCheckable(true);
        b->setFixedSize(24, 24);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        m_swatches->addButton(b, i);
        row->addWidget(b);
    }
    row->addStretch(1);
    connect(m_swatches, &QButtonGroup::idClicked, this, [this](int i) { m_hue = i; rebuildGrid(); });
    g->addLayout(row);
    body()->addLayout(g);

    addFooter(tr("Usar silhueta"), tr("usa a escolhida"));
    applySheetTheme(QStringLiteral(
        "QToolButton#silCell { background: transparent; border: 2px solid transparent; border-radius: 9px; padding: 0; }"
        "QToolButton#silCell:hover { border-color: %1; }"
        "QToolButton#silCell:checked { border-color: %2; }")
        .arg(alpha(pal.ink, 0.25).name(QColor::HexArgb), pal.accent.name()));
    refreshSwatches();
    rebuildGrid();
}

void SilhouettePickerDialog::refreshSwatches()
{
    const Palette pal = Palette::onPage();
    for (QAbstractButton* b : m_swatches->buttons()) {
        const int i = m_swatches->id(b);
        const int deg = kHueDeg[i];
        b->setStyleSheet(QStringLiteral(
            "QToolButton { background: %1; border: 2px solid %2; border-radius: 12px; }"
            "QToolButton:hover { border-color: %3; }"
            "QToolButton:checked { border: 2px solid %4; }")
            .arg(hsl(deg, 0.36, 0.40).name(), hsl(deg, 0.42, 0.86).name(),
                 alpha(pal.ink, 0.35).name(QColor::HexArgb), pal.bright.name()));
        b->setChecked(i == m_hue);
    }
}

void SilhouettePickerDialog::rebuildGrid()
{
    for (QAbstractButton* b : m_cells->buttons()) { m_cells->removeButton(b); b->deleteLater(); }
    const QStringList ids = Silhouettes::ids(m_male);
    const qreal dpr = devicePixelRatioF();
    const int inner = kCell - 6;
    for (int i = 0; i < ids.size(); ++i) {
        auto* b = new QToolButton(card());
        b->setObjectName(QStringLiteral("silCell"));
        b->setCheckable(true);
        b->setFixedSize(kCell, kCell);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        QImage img = Silhouettes::render(ids[i], m_hue, qRound(inner * dpr));
        // cantos arredondados, como a foto nos cartões
        QImage rounded(img.size(), QImage::Format_ARGB32_Premultiplied);
        rounded.fill(Qt::transparent);
        {
            QPainter p(&rounded);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath clip;
            clip.addRoundedRect(QRectF(0, 0, img.width(), img.height()), 6 * dpr, 6 * dpr);
            p.setClipPath(clip);
            p.drawImage(0, 0, img);
        }
        QPixmap pm = QPixmap::fromImage(rounded);
        pm.setDevicePixelRatio(dpr);
        b->setIcon(QIcon(pm));
        b->setIconSize(QSize(inner, inner));
        b->setProperty("silId", ids[i]);
        b->setChecked(ids[i] == m_id);
        connect(b, &QToolButton::clicked, this, [this, b]() { m_id = b->property("silId").toString(); });
        m_cells->addButton(b, i);
        m_grid->addWidget(b, i / kCols, i % kCols);
    }
}

QString SilhouettePickerDialog::resultDataUrl() const
{
    const QImage img = Silhouettes::render(m_id, m_hue, kOut);
    if (img.isNull()) return QString();
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG", 92);
    return QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(bytes.toBase64());
}

QString SilhouettePickerDialog::pick(QWidget* parent, bool male, const QString& seed)
{
    SilhouettePickerDialog dlg(parent, male, seed);
    if (dlg.exec() != QDialog::Accepted) return QString();
    return dlg.resultDataUrl();
}
