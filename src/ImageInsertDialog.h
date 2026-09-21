#ifndef IMAGEINSERTDIALOG_H
#define IMAGEINSERTDIALOG_H

#include <QDialog>
#include <QImage>
#include <QString>

class QButtonGroup;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;

class ImageInsertDialog : public QDialog
{
    Q_OBJECT

public:
    enum Alignment { Left = 0, Center = 1, Right = 2 };

    explicit ImageInsertDialog(const QString &imagePath, QWidget *parent = nullptr);

    Alignment alignment() const;
    int width() const;
    QString imagePath() const { return path; }

    // Imagem recortada pelo usuário, na resolução original do recorte.
    // QImage() se ele não recortou (ou desfez o recorte) — nesse caso quem
    // chama insere o arquivo de origem como sempre, sem cópia nenhuma.
    QImage croppedImage() const { return cropped; }

private:
    void updatePreview();
    void openCropDialog();
    void resetCrop();

    QString path;
    QImage source;   // original em resolução plena, carregado uma vez
    QImage cropped;  // resultado do recorte manual, se houver
    QLabel *previewLabel;
    QButtonGroup *alignGroup;
    QSlider *widthSlider;
    QSpinBox *widthSpinBox;
    QPushButton *cropBtn;
    QPushButton *resetCropBtn;
};

#endif
