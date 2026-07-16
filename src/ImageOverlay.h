#ifndef IMAGEOVERLAY_H
#define IMAGEOVERLAY_H

#include <QWidget>

class QLabel;
class QToolButton;

class ImageOverlay : public QWidget
{
    Q_OBJECT

public:
    enum Alignment { Left = 0, Center = 1, Right = 2 };

    explicit ImageOverlay(QWidget *parent = nullptr, bool showAlignment = true);

    void setCurrentAlignment(Alignment alignment);
    void setCurrentWidth(int width);

signals:
    void alignmentRequested(int alignment);
    void widthChangeRequested(int delta);

private:
    QToolButton *leftBtn;
    QToolButton *centerBtn;
    QToolButton *rightBtn;
    QToolButton *minusBtn;
    QLabel *widthLabel;
    QToolButton *plusBtn;

    void updateAlignmentButtons(Alignment current);
};

#endif
