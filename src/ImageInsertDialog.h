#ifndef IMAGEINSERTDIALOG_H
#define IMAGEINSERTDIALOG_H

#include <QDialog>
#include <QString>

class QButtonGroup;
class QLabel;
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

private:
    void updatePreview();

    QString path;
    QLabel *previewLabel;
    QButtonGroup *alignGroup;
    QSlider *widthSlider;
    QSpinBox *widthSpinBox;
};

#endif
