#include "ImageGalleryDialog.h"

#include "ClickableImageLabel.h"
#include "GeneratedImageGallery.h"
#include "Theme.h"

#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPixmap>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
constexpr int kThumbSide = 140;
constexpr int kColumns = 4;
}

ImageGalleryDialog::ImageGalleryDialog(const QString& projectRoot, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Galeria de imagens geradas"));
    setModal(true);
    resize(680, 560);

    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; border: 1px solid %2; }"
        "QLabel#igdCaption { color: %3; font-size: 10px; }"
        "QLabel#igdEmpty { color: %3; font-size: 13px; }"
        "QScrollArea { background: transparent; border: none; }"
    ).arg(Theme::panelBackground(), Theme::panelBorder(), Theme::textMuted()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);

    const QVector<GeneratedImageGallery::Entry> entries = GeneratedImageGallery::load(projectRoot);

    if (entries.isEmpty()) {
        auto* empty = new QLabel(tr("Nenhuma imagem gerada ainda neste projeto."), this);
        empty->setObjectName(QStringLiteral("igdEmpty"));
        empty->setAlignment(Qt::AlignCenter);
        root->addWidget(empty, 1);
        return;
    }

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    auto* grid = new QGridLayout(content);
    grid->setSpacing(12);

    int row = 0;
    int col = 0;
    for (const GeneratedImageGallery::Entry& entry : entries) {
        const QImage img(entry.filePath);
        if (img.isNull()) continue;

        auto* cell = new QWidget(content);
        auto* cellLay = new QVBoxLayout(cell);
        cellLay->setContentsMargins(0, 0, 0, 0);
        cellLay->setSpacing(4);

        auto* thumb = new ClickableImageLabel(cell);
        thumb->setFixedSize(kThumbSide, kThumbSide);
        thumb->setAlignment(Qt::AlignCenter);
        thumb->setPixmap(QPixmap::fromImage(img).scaled(
            kThumbSide, kThumbSide, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        const QString suggested = entry.characterName.isEmpty()
            ? QStringLiteral("imagem") : entry.characterName;
        thumb->setFullImage(img, suggested);
        cellLay->addWidget(thumb, 0, Qt::AlignHCenter);

        QString caption = entry.characterName.isEmpty() ? tr("(geração livre)") : entry.characterName;
        const QDateTime dt = QDateTime::fromString(entry.createdAt, Qt::ISODate);
        if (dt.isValid()) caption += QStringLiteral("\n") + dt.toString(QStringLiteral("dd/MM/yyyy HH:mm"));
        auto* captionLbl = new QLabel(caption, cell);
        captionLbl->setObjectName(QStringLiteral("igdCaption"));
        captionLbl->setAlignment(Qt::AlignCenter);
        captionLbl->setWordWrap(true);
        cellLay->addWidget(captionLbl);

        grid->addWidget(cell, row, col);
        if (++col >= kColumns) { col = 0; ++row; }
    }
    grid->setRowStretch(row + 1, 1);

    scroll->setWidget(content);
    root->addWidget(scroll, 1);
}
