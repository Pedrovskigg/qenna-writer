#include "BackgroundWidget.h"

#include "Theme.h"

#include <QImageReader>
#include <QPainter>
#include <QPaintEvent>

BackgroundWidget::BackgroundWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    // Aceita transparência: deixamos o background do pai aparecer onde a
    // imagem não cobre (modos centralizado / ajustar).
    setAutoFillBackground(false);
}

void BackgroundWidget::setBackground(const QString& imagePath, int mode)
{
    const bool pathChanged = (imagePath != m_path);
    const bool modeChanged = (mode != m_mode);
    m_path = imagePath;
    m_mode = mode;

    if (pathChanged) {
        m_source = QPixmap();
        if (!imagePath.isEmpty()) {
            QImageReader reader(imagePath);
            reader.setAutoTransform(true);
            const QImage img = reader.read();
            if (!img.isNull()) {
                m_source = QPixmap::fromImage(img);
            }
        }
        m_scaled = QPixmap();
        m_scaledFor = QSize();
    } else if (modeChanged) {
        m_scaled = QPixmap();
        m_scaledFor = QSize();
    }
    update();
}

void BackgroundWidget::setFillColor(const QColor& color)
{
    if (m_fill == color) return;
    m_fill = color;
    update();
}

void BackgroundWidget::rebuildScaled()
{
    if (m_source.isNull()) {
        m_scaled = QPixmap();
        m_scaledFor = QSize();
        return;
    }
    const QSize target = size();
    if (target.isEmpty()) {
        m_scaled = QPixmap();
        m_scaledFor = QSize();
        return;
    }
    if (m_scaledFor == target && !m_scaled.isNull()) return;

    switch (m_mode) {
    case Theme::BgCenter:
    case Theme::BgTile:
        m_scaled = m_source; // sem escalonar
        break;
    case Theme::BgStretch:
        m_scaled = m_source.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        break;
    case Theme::BgFit:
        m_scaled = m_source.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        break;
    case Theme::BgZoom:
    default:
        m_scaled = m_source.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        break;
    }
    m_scaledFor = target;
}

void BackgroundWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    // 1) Cor sólida cobrindo (caso a imagem deixe áreas vazias).
    if (m_fill.isValid()) {
        p.fillRect(rect(), m_fill);
    }
    // 2) Imagem em cima.
    if (m_source.isNull()) return;
    rebuildScaled();
    if (m_scaled.isNull()) return;

    const QSize w = size();
    switch (m_mode) {
    case Theme::BgCenter: {
        const int x = (w.width()  - m_scaled.width())  / 2;
        const int y = (w.height() - m_scaled.height()) / 2;
        p.drawPixmap(x, y, m_scaled);
        break;
    }
    case Theme::BgTile: {
        p.drawTiledPixmap(rect(), m_scaled);
        break;
    }
    case Theme::BgStretch:
        p.drawPixmap(rect(), m_scaled);
        break;
    case Theme::BgFit: {
        const int x = (w.width()  - m_scaled.width())  / 2;
        const int y = (w.height() - m_scaled.height()) / 2;
        p.drawPixmap(x, y, m_scaled);
        break;
    }
    case Theme::BgZoom:
    default: {
        // imagem maior que área — recorta centralizado
        const int x = (w.width()  - m_scaled.width())  / 2;
        const int y = (w.height() - m_scaled.height()) / 2;
        p.drawPixmap(x, y, m_scaled);
        break;
    }
    }
}
