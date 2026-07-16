#include "ThemePreviewWidget.h"

#include <QHash>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

namespace {

// Cache de fontes de imagem já decodificadas (em baixa resolução), compartilhado
// por todos os previews. Trocar de categoria no painel e voltar não redecodifica
// nenhum JPEG — fica instantâneo depois da primeira vez.
QPixmap loadPreviewSource(const QString& path)
{
    static QHash<QString, QPixmap> cache;
    const auto it = cache.constFind(path);
    if (it != cache.constEnd()) return it.value();

    QPixmap pm;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    // O preview é pequeno (card ~220x140). Decodifica já reduzido: o libjpeg
    // baixa direto na leitura (DCT scaling), muito mais rápido e leve que ler a
    // imagem cheia (1920px+) e só depois reescalar. cap=480 cobre até o preview
    // grande do editor sem perda visível.
    const QSize orig = reader.size();
    const int cap = 480;
    if (orig.isValid() && !orig.isEmpty() && qMax(orig.width(), orig.height()) > cap)
        reader.setScaledSize(orig.scaled(cap, cap, Qt::KeepAspectRatio));
    const QImage img = reader.read();
    if (!img.isNull()) pm = QPixmap::fromImage(img);
    cache.insert(path, pm);
    return pm;
}

QColor parseAnyColor(const QString& s)
{
    if (s.isEmpty()) return QColor();
    QColor c(s);
    if (c.isValid()) return c;
    QString raw = s.trimmed().toLower();
    if (raw.startsWith("rgba(") && raw.endsWith(")")) {
        raw = raw.mid(5, raw.size() - 6);
        const auto parts = raw.split(',');
        if (parts.size() == 4) {
            const int r = parts.at(0).trimmed().toInt();
            const int g = parts.at(1).trimmed().toInt();
            const int b = parts.at(2).trimmed().toInt();
            bool ok = false;
            const double a = parts.at(3).trimmed().toDouble(&ok);
            int alpha = ok ? (a <= 1.0 ? int(a * 255) : int(a)) : 255;
            return QColor(r, g, b, qBound(0, alpha, 255));
        }
    }
    return QColor();
}

} // namespace

ThemePreviewWidget::ThemePreviewWidget(QWidget* parent)
    : QWidget(parent)
    , m_lorem(QStringLiteral(
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
        "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
        "Ut enim ad minim veniam, quis nostrud exercitation ullamco."))
{
    setMinimumSize(220, 140);
    setAutoFillBackground(false);
}

void ThemePreviewWidget::setTheme(const Theme::MiraTheme& theme)
{
    m_theme = theme;
    if (theme.backgroundImage != m_bgPath) {
        m_bgPath = theme.backgroundImage;
        m_bgSource = QPixmap();
        m_bgScaled = QPixmap();
        m_bgScaledFor = QSize();
        if (!m_bgPath.isEmpty())
            m_bgSource = loadPreviewSource(m_bgPath);
    }
    if (theme.backgroundMode != m_bgMode) {
        m_bgMode = theme.backgroundMode;
        m_bgScaled = QPixmap();
        m_bgScaledFor = QSize();
    }
    update();
}

void ThemePreviewWidget::setShowChrome(bool show)
{
    m_showChrome = show;
    update();
}

void ThemePreviewWidget::setLoremText(const QString& text)
{
    m_lorem = text;
    update();
}

void ThemePreviewWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_bgScaled = QPixmap();
    m_bgScaledFor = QSize();
}

void ThemePreviewWidget::rebuildBackgroundPixmap()
{
    if (m_bgSource.isNull()) {
        m_bgScaled = QPixmap();
        m_bgScaledFor = QSize();
        return;
    }
    const QSize target = size();
    if (m_bgScaledFor == target && !m_bgScaled.isNull()) return;
    switch (m_bgMode) {
    case Theme::BgCenter:
    case Theme::BgTile:
        m_bgScaled = m_bgSource;
        break;
    case Theme::BgStretch:
        m_bgScaled = m_bgSource.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        break;
    case Theme::BgFit:
        m_bgScaled = m_bgSource.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        break;
    case Theme::BgZoom:
    default:
        m_bgScaled = m_bgSource.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        break;
    }
    m_bgScaledFor = target;
}

void ThemePreviewWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRect full = rect();
    const QColor appBg = parseAnyColor(m_theme.appBackground);
    const QColor panelBg = parseAnyColor(m_theme.panelBackground);
    const QColor panelBorder = parseAnyColor(m_theme.panelBorder);
    const QColor pageBg = parseAnyColor(m_theme.editorBackground);
    const QColor textColor = parseAnyColor(m_theme.editorTextColor);
    const QColor accent = parseAnyColor(m_theme.accentDefault);

    // 1) Fundo do app
    p.fillRect(full, appBg.isValid() ? appBg : QColor("#101010"));

    // 2) Imagem de fundo (se houver) cobrindo o app
    if (!m_bgSource.isNull()) {
        rebuildBackgroundPixmap();
        if (!m_bgScaled.isNull()) {
            const QSize w = size();
            switch (m_bgMode) {
            case Theme::BgCenter: {
                const int x = (w.width() - m_bgScaled.width()) / 2;
                const int y = (w.height() - m_bgScaled.height()) / 2;
                p.drawPixmap(x, y, m_bgScaled);
                break;
            }
            case Theme::BgTile:
                p.drawTiledPixmap(full, m_bgScaled);
                break;
            case Theme::BgStretch:
                p.drawPixmap(full, m_bgScaled);
                break;
            case Theme::BgFit:
            case Theme::BgZoom:
            default: {
                const int x = (w.width() - m_bgScaled.width()) / 2;
                const int y = (w.height() - m_bgScaled.height()) / 2;
                p.drawPixmap(x, y, m_bgScaled);
                break;
            }
            }
        }
    }

    // 3) Chrome: top toolbar + left bar simulados
    const int margin = qMax(6, full.height() / 24);
    int topY = margin;
    int leftX = margin;
    int contentX = margin;
    int contentY = margin;

    if (m_showChrome) {
        const int ttbH = qMax(14, full.height() / 14);
        const int leftW = qMax(14, full.width() / 22);
        // top toolbar
        QRectF ttbRect(margin, margin, full.width() - margin * 2, ttbH);
        p.setPen(Qt::NoPen);
        p.setBrush(panelBg.isValid() ? panelBg : QColor("#1a1a1a"));
        p.drawRoundedRect(ttbRect, 4, 4);
        if (panelBorder.isValid()) {
            p.setPen(QPen(panelBorder, 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(ttbRect, 4, 4);
        }
        // left bar
        const int lbY = margin + ttbH + margin / 2;
        QRectF lbRect(margin, lbY, leftW, full.height() - lbY - margin);
        p.setPen(Qt::NoPen);
        p.setBrush(panelBg.isValid() ? panelBg : QColor("#1a1a1a"));
        p.drawRoundedRect(lbRect, 4, 4);
        if (panelBorder.isValid()) {
            p.setPen(QPen(panelBorder, 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(lbRect, 4, 4);
        }

        contentX = margin + leftW + margin;
        contentY = lbY;
        topY = lbY;
        leftX = contentX;
    }

    // 4) "Página" do editor — retângulo com sombra
    const int pageInsetX = qMax(8, full.width() / 16);
    const int pageInsetY = qMax(8, full.height() / 16);
    QRectF pageRect(contentX + pageInsetX,
                    contentY + pageInsetY,
                    full.width() - contentX - pageInsetX * 2 - margin,
                    full.height() - contentY - pageInsetY - margin);
    if (pageRect.width() < 40 || pageRect.height() < 30) {
        // muito pequeno, pula
        return;
    }

    // Sombra
    if (m_theme.pageShadowEnabled) {
        const QColor shadowCol = parseAnyColor(m_theme.pageShadowColor);
        if (shadowCol.isValid() && shadowCol.alpha() > 0) {
            const int sr = qMax(2, m_theme.pageShadowRadius / 3);
            const int so = qMax(0, m_theme.pageShadowOffset / 2);
            for (int i = sr; i >= 1; --i) {
                QColor c = shadowCol;
                c.setAlpha(qBound(0, int(shadowCol.alphaF() * 255 / (i * 1.4)), 255));
                p.setPen(Qt::NoPen);
                p.setBrush(c);
                p.drawRoundedRect(pageRect.adjusted(-i, -i + so, i, i + so), 4, 4);
            }
        }
    }

    // Página com opacidade
    QColor pageFill = pageBg.isValid() ? pageBg : QColor("#ffffff");
    pageFill.setAlpha(qBound(0, (m_theme.editorOpacity * 255) / 100, 255));
    p.setPen(Qt::NoPen);
    p.setBrush(pageFill);
    p.drawRoundedRect(pageRect, 3, 3);

    // 5) Texto Lorem na página
    QFont font = p.font();
    // Mantém proporção pequena: o preview é só uma "vinheta" do tema. No card
    // (height ~110) dá 7pt; no editor (height ~460) clamp em 10pt.
    const int fs = qBound(7, int(pageRect.height() / 32), 10);
    font.setPointSize(fs);
    p.setFont(font);
    p.setPen(textColor.isValid() ? textColor : QColor("#111"));
    const QRectF textRect = pageRect.adjusted(10, 8, -10, -8);
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_lorem);

    // 6) Linha de accent fininha embaixo (sugere selection / accent)
    if (accent.isValid() && pageRect.height() > 50) {
        QRectF accentRect(pageRect.left() + 8,
                          pageRect.bottom() - 6,
                          pageRect.width() * 0.3,
                          2.0);
        p.setPen(Qt::NoPen);
        p.setBrush(accent);
        p.drawRoundedRect(accentRect, 1, 1);
    }
}
