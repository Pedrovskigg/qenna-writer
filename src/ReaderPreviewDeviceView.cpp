#include "ReaderPreviewDeviceView.h"

#include <QAbstractTextDocumentLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTextDocument>
#include <QWheelEvent>

ReaderPreviewDeviceView::ReaderPreviewDeviceView(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(240, 320);
}

void ReaderPreviewDeviceView::setDocument(QTextDocument* doc) {
    m_doc = doc;
    recomputePagination();
}

void ReaderPreviewDeviceView::setPositionFraction(double frac) {
    m_positionFraction = qBound(0.0, frac, 1.0);
    if (m_pageCount > 1)
        m_currentPage = qBound(0, qRound(m_positionFraction * (m_pageCount - 1)), m_pageCount - 1);
    else
        m_currentPage = 0;
    update();
}

double ReaderPreviewDeviceView::positionFraction() const {
    return m_pageCount > 1 ? double(m_currentPage) / (m_pageCount - 1) : 0.0;
}

void ReaderPreviewDeviceView::setTextColor(const QColor& color) {
    m_textColor = color;
    update();
}

void ReaderPreviewDeviceView::setBackgroundColor(const QColor& color) {
    m_backgroundColor = color;
    update();
}

void ReaderPreviewDeviceView::nextPage() { goToPage(m_currentPage + 1); }
void ReaderPreviewDeviceView::previousPage() { goToPage(m_currentPage - 1); }

void ReaderPreviewDeviceView::goToPage(int page) {
    page = qBound(0, page, qMax(0, m_pageCount - 1));
    if (page == m_currentPage) return;
    m_currentPage = page;
    m_positionFraction = positionFraction();
    update();
    emit positionChanged(m_positionFraction);
}

QRectF ReaderPreviewDeviceView::computeScreenRect() const {
    const QRectF full = rect();
    const qreal bezelMargin = 18;
    const QRectF bezel = full.adjusted(bezelMargin, bezelMargin, -bezelMargin, -bezelMargin);
    // Margem inferior maior ("queixo") pro rodapé de página, como um e-reader real.
    return bezel.adjusted(16, 22, -16, -46);
}

void ReaderPreviewDeviceView::recomputePagination() {
    if (!m_doc) { m_pageCount = 0; m_currentPage = 0; update(); return; }
    const QSizeF screenSize = computeScreenRect().size();
    if (screenSize.width() <= 0 || screenSize.height() <= 0) return;
    m_doc->setPageSize(screenSize);
    // Reancora sempre pela fração salva — nunca pelo índice de página antigo,
    // que deixa de fazer sentido quando o número de páginas muda com o resize.
    m_pageCount = qMax(1, m_doc->pageCount());
    m_currentPage = qBound(0, qRound(m_positionFraction * (m_pageCount - 1)), m_pageCount - 1);
    update();
}

void ReaderPreviewDeviceView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    recomputePagination();
}

void ReaderPreviewDeviceView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF full = rect();
    const qreal bezelMargin = 18;
    const QRectF bezel = full.adjusted(bezelMargin, bezelMargin, -bezelMargin, -bezelMargin);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x2b, 0x2b, 0x2b));
    p.drawRoundedRect(bezel, 22, 22);

    const QRectF screenRect = computeScreenRect();
    p.setBrush(m_backgroundColor);
    p.drawRect(screenRect);

    if (m_doc) {
        p.save();
        p.setClipRect(screenRect);
        p.translate(screenRect.topLeft());
        p.translate(0, -m_currentPage * screenRect.height());
        QAbstractTextDocumentLayout::PaintContext ctx;
        ctx.palette.setColor(QPalette::Text, m_textColor);
        m_doc->documentLayout()->draw(&p, ctx);
        p.restore();
    }

    // Rodapé "Página X de Y", na "queixo" da moldura.
    p.setPen(m_textColor);
    QFont f = p.font();
    f.setPointSize(9);
    p.setFont(f);
    const QString label = m_pageCount > 0
        ? tr("Página %1 de %2").arg(m_currentPage + 1).arg(m_pageCount)
        : QString();
    const QRectF footer(bezel.left(), screenRect.bottom(), bezel.width(), bezel.bottom() - screenRect.bottom());
    p.drawText(footer, Qt::AlignCenter, label);
}

void ReaderPreviewDeviceView::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Right:
        case Qt::Key_Down:
        case Qt::Key_PageDown:
            nextPage();
            return;
        case Qt::Key_Left:
        case Qt::Key_Up:
        case Qt::Key_PageUp:
            previousPage();
            return;
        default:
            QWidget::keyPressEvent(event);
    }
}

void ReaderPreviewDeviceView::wheelEvent(QWheelEvent* event) {
    if (event->angleDelta().y() > 0) previousPage();
    else if (event->angleDelta().y() < 0) nextPage();
    event->accept();
}

void ReaderPreviewDeviceView::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) { QWidget::mousePressEvent(event); return; }
    setFocus();
    const QRectF screen = computeScreenRect();
    if (!screen.contains(event->pos())) { QWidget::mousePressEvent(event); return; }
    if (event->pos().x() < screen.center().x()) previousPage();
    else nextPage();
}
