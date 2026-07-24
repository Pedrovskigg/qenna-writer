#include "ImageCropDialog.h"

#include "Theme.h"

#include <QBuffer>
#include <QByteArray>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

namespace {
constexpr int kMaxDisplaySide = 520;
constexpr int kHandleHitRadius = 12; // px, raio de detecção do canto
constexpr int kMinSide = 24;         // px mínimo do quadrado, em coords de exibição
constexpr int kHandleSize = 10;      // px, tamanho visual da alça desenhada
}

// Widget que desenha a imagem + máscara + retângulo de crop, e trata
// mover/redimensionar via mouse. Não é exposto no header — só o
// ImageCropDialog precisa conhecer essa classe.
class CropCanvas : public QWidget {
public:
    explicit CropCanvas(const QPixmap& displayPixmap, QWidget* parent)
        : QWidget(parent), m_pixmap(displayPixmap)
    {
        setFixedSize(m_pixmap.size());
        setMouseTracking(true);
        setCursor(Qt::ArrowCursor);

        // Retângulo inicial: centralizado, lado = menor dimensão da imagem
        // exibida — matematicamente idêntico ao crop automático de antes
        // (mesmo cálculo, só que em coords de exibição em vez de originais).
        const int side = qMin(m_pixmap.width(), m_pixmap.height());
        const int x = (m_pixmap.width() - side) / 2;
        const int y = (m_pixmap.height() - side) / 2;
        m_rect = QRect(x, y, side, side);
    }

    QRect cropRect() const { return m_rect; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.drawPixmap(0, 0, m_pixmap);

        // Máscara escura fora do retângulo de crop.
        QPainterPath outer;
        outer.addRect(rect());
        QPainterPath inner;
        inner.addRect(m_rect);
        p.fillPath(outer.subtracted(inner), QColor(0, 0, 0, 140));

        // Borda do retângulo.
        QPen pen(QColor(Theme::accentDefault()));
        pen.setWidth(2);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(m_rect);

        // Alças nos 4 cantos.
        p.setBrush(QColor(Theme::accentDefault()));
        p.setPen(Qt::NoPen);
        for (const QPoint& corner : cornersOf(m_rect)) {
            p.drawRect(QRect(corner.x() - kHandleSize / 2, corner.y() - kHandleSize / 2,
                              kHandleSize, kHandleSize));
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton) return;
        m_dragMode = hitTest(event->pos());
        m_dragStartMouse = event->pos();
        m_dragStartRect = m_rect;
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (m_dragMode == DragMode::None) {
            // Sem botão pressionado: só atualiza o cursor conforme o hover.
            setCursor(cursorFor(hitTest(event->pos())));
            return;
        }

        if (m_dragMode == DragMode::Move) {
            // Clamp simples: como o lado do retângulo nunca passa do tamanho
            // do pixmap (garantido pelo resize), um único passe de bordas
            // já é suficiente, sem precisar reconferir depois de ajustar.
            const QPoint delta = event->pos() - m_dragStartMouse;
            QRect r = m_dragStartRect.translated(delta);
            if (r.left() < 0) r.moveLeft(0);
            if (r.top() < 0) r.moveTop(0);
            if (r.right() > m_pixmap.width() - 1) r.moveRight(m_pixmap.width() - 1);
            if (r.bottom() > m_pixmap.height() - 1) r.moveBottom(m_pixmap.height() - 1);
            m_rect = r;
            update();
            return;
        }

        // Redimensionar: âncora é o canto OPOSTO ao que foi agarrado, fixo.
        // Recalcula sempre por min/max entre âncora e ponto atual (em vez de
        // se prender ao nome do modo) — evita bug de "cruzar" o mouse pro
        // outro lado da âncora.
        const QPoint anchor = anchorFor(m_dragMode, m_dragStartRect);
        const QPoint cur = event->pos();
        const int dx = cur.x() - anchor.x();
        const int dy = cur.y() - anchor.y();
        const int maxSideX = (dx >= 0) ? (m_pixmap.width() - anchor.x()) : anchor.x();
        const int maxSideY = (dy >= 0) ? (m_pixmap.height() - anchor.y()) : anchor.y();
        int side = qMin(qMin(qAbs(dx), qAbs(dy)), qMin(maxSideX, maxSideY));
        side = qMax(side, kMinSide);

        const int x0 = (dx >= 0) ? anchor.x() : anchor.x() - side;
        const int y0 = (dy >= 0) ? anchor.y() : anchor.y() - side;
        m_rect = QRect(x0, y0, side, side);
        update();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) m_dragMode = DragMode::None;
    }

private:
    enum class DragMode { None, Move, ResizeTL, ResizeTR, ResizeBL, ResizeBR };

    static QVector<QPoint> cornersOf(const QRect& r)
    {
        return { r.topLeft(), r.topRight(), r.bottomLeft(), r.bottomRight() };
    }

    static QPoint anchorFor(DragMode mode, const QRect& startRect)
    {
        switch (mode) {
            case DragMode::ResizeTL: return startRect.bottomRight();
            case DragMode::ResizeTR: return startRect.bottomLeft();
            case DragMode::ResizeBL: return startRect.topRight();
            case DragMode::ResizeBR: return startRect.topLeft();
            default: return startRect.topLeft();
        }
    }

    DragMode hitTest(const QPoint& p) const
    {
        auto near = [&](const QPoint& corner) {
            return qAbs(p.x() - corner.x()) <= kHandleHitRadius
                && qAbs(p.y() - corner.y()) <= kHandleHitRadius;
        };
        if (near(m_rect.topLeft()))     return DragMode::ResizeTL;
        if (near(m_rect.topRight()))    return DragMode::ResizeTR;
        if (near(m_rect.bottomLeft()))  return DragMode::ResizeBL;
        if (near(m_rect.bottomRight())) return DragMode::ResizeBR;
        if (m_rect.contains(p))         return DragMode::Move;
        return DragMode::None;
    }

    static Qt::CursorShape cursorFor(DragMode mode)
    {
        switch (mode) {
            case DragMode::ResizeTL:
            case DragMode::ResizeBR: return Qt::SizeFDiagCursor;
            case DragMode::ResizeTR:
            case DragMode::ResizeBL: return Qt::SizeBDiagCursor;
            case DragMode::Move:     return Qt::SizeAllCursor;
            default:                 return Qt::ArrowCursor;
        }
    }

    QPixmap  m_pixmap;
    QRect    m_rect;
    DragMode m_dragMode = DragMode::None;
    QPoint   m_dragStartMouse;
    QRect    m_dragStartRect;
};

ImageCropDialog::ImageCropDialog(const QImage& original, QWidget* parent)
    : QDialog(parent), m_original(original)
{
    setModal(true);
    setWindowTitle(tr("Ajustar recorte da foto"));
    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; border: 1px solid %2; }"
        "QLabel#cropHint { color: %3; font-size: 11px; }"
        "QPushButton {"
        "  background: transparent; color: %4;"
        "  border: 1px solid %2; border-radius: 6px;"
        "  padding: 5px 14px; font-size: 12px;"
        "}"
        "QPushButton:hover { background: %5; }"
        "QPushButton#cropOk { border-color: %6; }"
    ).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textMuted(),
          Theme::textBright(), Theme::hoverOverlay(), Theme::accentDefault()));

    const qreal scale = qMin(1.0, qreal(kMaxDisplaySide) / qMax(m_original.width(), m_original.height()));
    const QSize displaySize = m_original.size() * scale;
    const QPixmap displayPixmap = QPixmap::fromImage(m_original)
        .scaled(displaySize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(10);

    auto* hint = new QLabel(tr("Arraste para mover, puxe os cantos para redimensionar"), this);
    hint->setObjectName(QStringLiteral("cropHint"));
    root->addWidget(hint);

    m_canvas = new CropCanvas(displayPixmap, this);
    root->addWidget(m_canvas, 0, Qt::AlignHCenter);

    auto* row = new QHBoxLayout();
    row->addStretch(1);
    m_cancelBtn = new QPushButton(tr("Cancelar"), this);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    row->addWidget(m_cancelBtn);

    m_okBtn = new QPushButton(tr("Usar recorte"), this);
    m_okBtn->setObjectName(QStringLiteral("cropOk"));
    m_okBtn->setDefault(true);
    m_okBtn->setCursor(Qt::PointingHandCursor);
    connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);
    row->addWidget(m_okBtn);

    root->addLayout(row);
}

QString ImageCropDialog::resultDataUrl() const
{
    const qreal scale = qMin(1.0, qreal(kMaxDisplaySide) / qMax(m_original.width(), m_original.height()));
    const QRect dispRect = m_canvas->cropRect();

    QRect cropRect(
        qRound(dispRect.x() / scale), qRound(dispRect.y() / scale),
        qRound(dispRect.width() / scale), qRound(dispRect.height() / scale));
    cropRect = cropRect.intersected(QRect(0, 0, m_original.width(), m_original.height()));
    // Corrige possível desvio de 1px entre lado calculado e o intersected —
    // sempre reforça quadrado a partir do menor lado resultante.
    const int side = qMin(cropRect.width(), cropRect.height());
    cropRect.setWidth(side);
    cropRect.setHeight(side);

    const QImage cropped = m_original.copy(cropRect);
    const QImage scaled = cropped.scaled(400, 400, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    scaled.save(&buf, "JPEG", 92);
    return QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(bytes.toBase64());
}

QString ImageCropDialog::pickAndCropImage(QWidget* parent, const QString& fileDialogTitle)
{
    const QString path = QFileDialog::getOpenFileName(parent, fileDialogTitle, QString(),
        tr("Imagens (*.png *.jpg *.jpeg *.gif *.bmp *.webp)"));
    if (path.isEmpty()) return QString();

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage img = reader.read();
    if (img.isNull()) return QString();

    ImageCropDialog dlg(img, parent);
    if (dlg.exec() != QDialog::Accepted) return QString();
    return dlg.resultDataUrl();
}
