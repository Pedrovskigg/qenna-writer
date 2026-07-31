#pragma once

#include <QDialog>
#include <QString>

// Galeria de todas as imagens geradas por IA num projeto (ver
// GeneratedImageGallery) — grid de miniaturas, clicar numa abre o
// ImageViewerDialog (ver grande + salvar como). Somente leitura: não edita
// nem apaga nada do índice, só mostra.
class ImageGalleryDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImageGalleryDialog(const QString& projectRoot, QWidget* parent = nullptr);
};
