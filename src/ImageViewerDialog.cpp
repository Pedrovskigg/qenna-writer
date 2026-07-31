#include "ImageViewerDialog.h"

#include "Theme.h"

#include <QApplication>
#include <QDialog>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QWidget>

namespace ImageViewerDialog {

void show(const QImage& image, QWidget* parent, const QString& suggestedFileName)
{
    if (image.isNull()) return;

    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("Visualizar imagem"));
    dlg.setModal(true);
    dlg.setStyleSheet(QStringLiteral(
        "QDialog { background: %1; border: 1px solid %2; }"
        "QPushButton {"
        "  background: transparent; color: %3; border: 1px solid %2;"
        "  border-radius: 6px; padding: 6px 16px; font-size: 12px;"
        "}"
        "QPushButton:hover { background: %4; }"
    ).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textBright(), Theme::hoverOverlay()));

    auto* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    // Cabe na tela: até 80% da área disponível, sem estourar a resolução real
    // da imagem (não amplia imagem pequena além do tamanho original).
    QScreen* screen = parent && parent->screen() ? parent->screen() : QApplication::primaryScreen();
    const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 800);
    const QSize maxSize(int(avail.width() * 0.8), int(avail.height() * 0.8));
    const QSize shown = image.size().boundedTo(maxSize);

    auto* imgLabel = new QLabel(&dlg);
    imgLabel->setPixmap(QPixmap::fromImage(image).scaled(
        shown, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    imgLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(imgLabel);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch(1);

    auto* saveBtn = new QPushButton(QObject::tr("Salvar como..."), &dlg);
    saveBtn->setCursor(Qt::PointingHandCursor);
    QObject::connect(saveBtn, &QPushButton::clicked, &dlg, [&dlg, &image, suggestedFileName]() {
        const QString defaultName = suggestedFileName.isEmpty()
            ? QStringLiteral("imagem") : suggestedFileName;
        const QString path = QFileDialog::getSaveFileName(&dlg, QObject::tr("Salvar imagem"),
            defaultName + QStringLiteral(".png"), QObject::tr("PNG (*.png);;JPEG (*.jpg)"));
        if (path.isEmpty()) return;
        if (!image.save(path)) {
            QMessageBox::warning(&dlg, QObject::tr("Salvar imagem"),
                QObject::tr("Não foi possível salvar a imagem."));
        }
    });
    btnRow->addWidget(saveBtn);

    auto* closeBtn = new QPushButton(QObject::tr("Fechar"), &dlg);
    closeBtn->setCursor(Qt::PointingHandCursor);
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addWidget(closeBtn);

    root->addLayout(btnRow);
    dlg.exec();
}

} // namespace ImageViewerDialog
