#pragma once

#include <QImage>
#include <QLabel>
#include <QString>

// QLabel que abre um ImageViewerDialog (ver grande + salvar como) ao
// clicar — usado em toda miniatura de imagem gerada/anexada do app (bolhas
// do chat, preview do diálogo de geração, galeria). setFullImage() guarda a
// imagem em resolução PLENA pro visualizador, independente de quão reduzido
// o pixmap exibido (via setPixmap) estiver.
class ClickableImageLabel : public QLabel {
    Q_OBJECT
public:
    explicit ClickableImageLabel(QWidget* parent = nullptr);

    void setFullImage(const QImage& img, const QString& suggestedFileName = QString());

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QImage m_fullImage;
    QString m_suggestedFileName;
};
