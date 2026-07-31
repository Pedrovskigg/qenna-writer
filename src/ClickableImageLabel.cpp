#include "ClickableImageLabel.h"

#include "ImageViewerDialog.h"

#include <QMouseEvent>

ClickableImageLabel::ClickableImageLabel(QWidget* parent)
    : QLabel(parent)
{
}

void ClickableImageLabel::setFullImage(const QImage& img, const QString& suggestedFileName)
{
    m_fullImage = img;
    m_suggestedFileName = suggestedFileName;
    setCursor(m_fullImage.isNull() ? Qt::ArrowCursor : Qt::PointingHandCursor);
}

void ClickableImageLabel::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_fullImage.isNull()) {
        ImageViewerDialog::show(m_fullImage, this, m_suggestedFileName);
        event->accept();
        return;
    }
    QLabel::mouseReleaseEvent(event);
}
