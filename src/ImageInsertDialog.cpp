#include "ImageInsertDialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

ImageInsertDialog::ImageInsertDialog(const QString &imagePath, QWidget *parent)
    : QDialog(parent)
    , path(imagePath)
{
    setWindowTitle(tr("Inserir imagem"));
    setModal(true);
    setMinimumWidth(380);
    setObjectName(QStringLiteral("imageInsertDialog"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(14);

    previewLabel = new QLabel(this);
    previewLabel->setObjectName(QStringLiteral("imagePreview"));
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setMinimumHeight(180);
    layout->addWidget(previewLabel);

    auto *alignLabel = new QLabel(tr("Alinhamento"), this);
    layout->addWidget(alignLabel);

    auto *alignLayout = new QHBoxLayout();
    alignLayout->setSpacing(12);
    auto *leftBtn = new QRadioButton(tr("Esquerda"), this);
    auto *centerBtn = new QRadioButton(tr("Centro"), this);
    auto *rightBtn = new QRadioButton(tr("Direita"), this);
    leftBtn->setChecked(true);
    alignLayout->addWidget(leftBtn);
    alignLayout->addWidget(centerBtn);
    alignLayout->addWidget(rightBtn);
    layout->addLayout(alignLayout);

    alignGroup = new QButtonGroup(this);
    alignGroup->addButton(leftBtn, Left);
    alignGroup->addButton(centerBtn, Center);
    alignGroup->addButton(rightBtn, Right);

    auto *widthLabel = new QLabel(tr("Largura"), this);
    layout->addWidget(widthLabel);

    auto *widthLayout = new QHBoxLayout();
    widthLayout->setSpacing(10);
    widthSlider = new QSlider(Qt::Horizontal, this);
    widthSlider->setRange(100, 800);
    widthSlider->setValue(320);
    widthSpinBox = new QSpinBox(this);
    widthSpinBox->setRange(100, 800);
    widthSpinBox->setValue(320);
    widthSpinBox->setSuffix(tr(" px"));
    widthSpinBox->setFixedWidth(90);
    widthLayout->addWidget(widthSlider);
    widthLayout->addWidget(widthSpinBox);
    layout->addLayout(widthLayout);

    connect(widthSlider, &QSlider::valueChanged, widthSpinBox, &QSpinBox::setValue);
    connect(widthSpinBox, &QSpinBox::valueChanged, widthSlider, &QSlider::setValue);

    auto *buttons = new QDialogButtonBox(this);
    auto *cancelBtn = buttons->addButton(QDialogButtonBox::Cancel);
    cancelBtn->setText(tr("Cancelar"));
    auto *insertBtn = buttons->addButton(tr("Inserir"), QDialogButtonBox::AcceptRole);
    insertBtn->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updatePreview();
}

ImageInsertDialog::Alignment ImageInsertDialog::alignment() const
{
    const int id = alignGroup->checkedId();
    return static_cast<Alignment>(id < 0 ? Left : id);
}

int ImageInsertDialog::width() const
{
    return widthSpinBox->value();
}

void ImageInsertDialog::updatePreview()
{
    QImage img(path);
    if (img.isNull()) {
        previewLabel->setText(tr("(prévia indisponível)"));
        return;
    }
    const QPixmap pix = QPixmap::fromImage(img).scaled(
        340, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    previewLabel->setPixmap(pix);
}
