#include "ProjectInfoPanel.h"

#include "ProjectModel.h"
#include "Theme.h"

#include <QBuffer>
#include <QByteArray>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {

constexpr int kCoverFrameW = 260;
constexpr int kCoverFrameH = 390;
constexpr int kMaxCoverSide = 1200;

// Carrega imagem do disco, redimensiona pra caber em kMaxCoverSide mantendo
// proporção e devolve data URL JPEG. Sem crop forçado — capa é retrato.
QString loadCoverAsDataUrl(const QString& path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) return QString();
    if (img.width() > kMaxCoverSide || img.height() > kMaxCoverSide) {
        img = img.scaled(kMaxCoverSide, kMaxCoverSide,
                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG", 88);
    return QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(bytes.toBase64());
}

QPixmap pixmapFromDataUrl(const QString& dataUrl) {
    if (dataUrl.isEmpty()) return QPixmap();
    const int comma = dataUrl.indexOf(QLatin1Char(','));
    if (comma < 0) return QPixmap();
    const QByteArray raw = QByteArray::fromBase64(dataUrl.mid(comma + 1).toLatin1());
    QPixmap pm;
    pm.loadFromData(raw);
    return pm;
}

} // namespace

ProjectInfoPanel::ProjectInfoPanel(ProjectModel* model, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
{
    setObjectName(QStringLiteral("projectInfoPanel"));
    setWindowTitle(tr("Informações da obra"));
    setModal(false);
    resize(820, 560);
    buildUi();
    applyDialogStyle();
}

void ProjectInfoPanel::buildUi() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(22, 22, 22, 18);
    root->setSpacing(24);

    // ---- Coluna esquerda: capa + ações ----
    auto* leftCol = new QVBoxLayout;
    leftCol->setSpacing(10);

    m_coverPreview = new QLabel(this);
    m_coverPreview->setObjectName(QStringLiteral("projectInfoCover"));
    m_coverPreview->setFixedSize(kCoverFrameW, kCoverFrameH);
    m_coverPreview->setAlignment(Qt::AlignCenter);
    m_coverPreview->setText(tr("Sem capa"));
    m_coverPreview->setScaledContents(false);
    leftCol->addWidget(m_coverPreview);

    m_pickCoverBtn = new QPushButton(tr("Escolher capa…"), this);
    m_pickCoverBtn->setObjectName(QStringLiteral("projectInfoBtn"));
    m_pickCoverBtn->setCursor(Qt::PointingHandCursor);
    connect(m_pickCoverBtn, &QPushButton::clicked, this, &ProjectInfoPanel::onPickCover);
    leftCol->addWidget(m_pickCoverBtn);

    m_clearCoverBtn = new QPushButton(tr("Remover capa"), this);
    m_clearCoverBtn->setObjectName(QStringLiteral("projectInfoBtn"));
    m_clearCoverBtn->setCursor(Qt::PointingHandCursor);
    connect(m_clearCoverBtn, &QPushButton::clicked, this, &ProjectInfoPanel::onClearCover);
    leftCol->addWidget(m_clearCoverBtn);

    leftCol->addStretch();
    root->addLayout(leftCol);

    // ---- Coluna direita: header + formulário + footer ----
    auto* rightCol = new QVBoxLayout;
    rightCol->setSpacing(10);

    auto* heading = new QLabel(tr("Detalhes da obra"), this);
    heading->setObjectName(QStringLiteral("projectInfoHeading"));
    rightCol->addWidget(heading);

    auto addLabel = [this, rightCol](const QString& text) {
        auto* lab = new QLabel(text, this);
        lab->setObjectName(QStringLiteral("projectInfoLabel"));
        rightCol->addWidget(lab);
    };

    addLabel(tr("Nome do projeto"));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("Ex: Minha história"));
    rightCol->addWidget(m_nameEdit);

    addLabel(tr("Autor"));
    m_authorEdit = new QLineEdit(this);
    m_authorEdit->setPlaceholderText(tr("Ex: Maria Silva"));
    rightCol->addWidget(m_authorEdit);

    addLabel(tr("Gêneros"));
    m_genresEdit = new QLineEdit(this);
    m_genresEdit->setPlaceholderText(tr("Ex: Fantasia, Romance"));
    rightCol->addWidget(m_genresEdit);

    addLabel(tr("Sinopse"));
    m_synopsisEdit = new QPlainTextEdit(this);
    m_synopsisEdit->setPlaceholderText(tr("Escreva uma breve sinopse…"));
    rightCol->addWidget(m_synopsisEdit, /*stretch=*/1);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(8);
    actions->addStretch();
    m_cancelBtn = new QPushButton(tr("Cancelar"), this);
    m_cancelBtn->setObjectName(QStringLiteral("projectInfoBtn"));
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    m_okBtn = new QPushButton(tr("Salvar"), this);
    m_okBtn->setObjectName(QStringLiteral("projectInfoBtn"));
    m_okBtn->setDefault(true);
    connect(m_okBtn, &QPushButton::clicked, this, &ProjectInfoPanel::onSave);
    actions->addWidget(m_cancelBtn);
    actions->addWidget(m_okBtn);
    rightCol->addLayout(actions);

    root->addLayout(rightCol, /*stretch=*/1);

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &ProjectInfoPanel::applyDialogStyle);
}

void ProjectInfoPanel::loadFromModel() {
    if (!m_model) return;
    m_nameEdit->setText(m_model->projectName());
    m_authorEdit->setText(m_model->projectAuthor());
    m_genresEdit->setText(m_model->projectGenres());
    m_synopsisEdit->setPlainText(m_model->projectSynopsis());
    m_coverDataUrl = m_model->projectCoverDataUrl();
    updateCoverPreview();
}

void ProjectInfoPanel::showEvent(QShowEvent* event) {
    loadFromModel();
    QDialog::showEvent(event);
}

void ProjectInfoPanel::updateCoverPreview() {
    if (!m_coverPreview) return;
    const QPixmap pm = pixmapFromDataUrl(m_coverDataUrl);
    if (pm.isNull()) {
        m_coverPreview->clear();
        m_coverPreview->setText(tr("Sem capa"));
        return;
    }
    m_coverPreview->setPixmap(pm.scaled(kCoverFrameW, kCoverFrameH,
                                        Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void ProjectInfoPanel::onPickCover() {
    const QString startDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString path = QFileDialog::getOpenFileName(this,
        tr("Escolher capa"),
        startDir,
        tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
    if (path.isEmpty()) return;
    const QString dataUrl = loadCoverAsDataUrl(path);
    if (dataUrl.isEmpty()) return;
    m_coverDataUrl = dataUrl;
    updateCoverPreview();
}

void ProjectInfoPanel::onClearCover() {
    if (m_coverDataUrl.isEmpty()) return;
    m_coverDataUrl.clear();
    updateCoverPreview();
}

void ProjectInfoPanel::onSave() {
    if (!m_model) { accept(); return; }
    m_model->setProjectDetails(
        m_nameEdit->text().trimmed(),
        m_authorEdit->text().trimmed(),
        m_genresEdit->text().trimmed(),
        m_synopsisEdit->toPlainText().trimmed(),
        m_coverDataUrl);
    accept();
}

void ProjectInfoPanel::applyDialogStyle() {
    setStyleSheet(QStringLiteral(R"(
        #projectInfoPanel { background: %1; }
        #projectInfoPanel QLabel { color: %2; font-size: 12px; }
        #projectInfoHeading {
            color: %3;
            font-size: 16px;
            font-weight: 600;
            padding-bottom: 4px;
        }
        #projectInfoLabel { color: %4; font-size: 11px; }
        #projectInfoCover {
            background: %5;
            color: %4;
            border: 1px solid %6;
            border-radius: 6px;
            font-size: 11px;
        }
        #projectInfoPanel QLineEdit,
        #projectInfoPanel QPlainTextEdit {
            background: %5;
            color: %3;
            border: 1px solid %6;
            border-radius: 6px;
            padding: 6px 8px;
            selection-background-color: %7;
        }
        #projectInfoPanel QLineEdit:focus,
        #projectInfoPanel QPlainTextEdit:focus {
            border-color: %9;
        }
        QPushButton#projectInfoBtn {
            background: %5;
            color: %2;
            border: 1px solid %6;
            padding: 6px 14px;
            border-radius: 6px;
            font-size: 12px;
            min-height: 26px;
        }
        QPushButton#projectInfoBtn:hover {
            background: %7;
            color: %3;
        }
        QPushButton#projectInfoBtn:default {
            border-color: %9;
        }
    )").arg(
        Theme::appBackground(),     // 1
        Theme::textPrimary(),       // 2
        Theme::textBright(),        // 3
        Theme::textMuted(),         // 4
        Theme::panelBackground(),   // 5
        Theme::panelBorder(),       // 6
        Theme::hoverOverlay(),      // 7
        Theme::subtleBorder(),      // 8 (não usado mas mantém indexação)
        Theme::accentDefault()      // 9
    ));
}
