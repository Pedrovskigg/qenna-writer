#include "MainMenuDialog.h"
#include "ColorPopover.h"

#include "AboutDialog.h"
#include "TrashDialog.h"
#include "IconUtils.h"
#include "LibraryViews.h"
#include "PanelMotion.h"
#include "RemindersStore.h"
#include "WordCounter.h"
#include "ProjectStorage.h"
#include "Quotes.h"
#include "StackView.h"
#include "Theme.h"

#include <QAction>
#include <QBuffer>
#include <QByteArray>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QAbstractAnimation>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QDialogButtonBox>
#include <QDir>
#include <QEasingCurve>
#include <QEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPropertyAnimation>
#include <QSet>
#include <QPushButton>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSettings>
#include <QShowEvent>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QVector>
#include <QWidgetAction>
#include <QWidgetItem>

#include <functional>

namespace {

constexpr int kCardCoverW = 240;
constexpr int kCardCoverH = 360;
constexpr int kStackHeroCoverW = 340; // capa herói da Pilha — mesma proporção 2:3
constexpr int kStackHeroCoverH = 510;
constexpr int kDialogW = 1320;
constexpr int kDialogH = 1000;
constexpr int kSidebarW = 410;   // largura da barra lateral
constexpr int kLogoSize = 330;   // caixa do logo — cabe na largura interna (410 - margens) com folga
constexpr int kLogoHoldMs = 5000;   // tempo de cada arte do Q parada na tela
constexpr int kLogoFadeMs = 900;    // duração do crossfade entre duas artes

// Menor retângulo que contém tudo que não é transparente. As artes do Q vêm
// do gerador de imagem com enquadramentos diferentes (1024², 1254², 1536x1024…)
// e margem vazia variável em volta; sem recortar por aqui, cada arte entraria
// com um tamanho e a letra pularia a cada troca.
QRect opaqueBounds(const QImage& img)
{
    int left = img.width(), right = -1, top = img.height(), bottom = -1;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(line[x]) <= 8) continue;   // tolera lixo de anti-alias
            if (x < left) left = x;
            if (x > right) right = x;
            if (y < top) top = y;
            if (y > bottom) bottom = y;
        }
    }
    if (right < left || bottom < top) return {};
    return QRect(QPoint(left, top), QPoint(right, bottom));
}

// Recorta pelo conteúdo, escala pra caber na caixa e centraliza. Todas as
// artes saem daqui com o mesmo tamanho de quadro e a letra na mesma posição,
// que é o que o crossfade precisa pra não tremer. Imagem sem canal alpha cai
// no caminho de baixo e é usada inteira.
QImage normalizedLogo(const QString& path, int box)
{
    QImage img(path);
    if (img.isNull()) return {};
    img = img.convertToFormat(QImage::Format_ARGB32);
    const QRect bounds = opaqueBounds(img);
    if (bounds.isValid()) img = img.copy(bounds);

    const QImage scaled = img.scaled(box, box, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QImage frame(box, box, QImage::Format_ARGB32_Premultiplied);
    frame.fill(Qt::transparent);
    QPainter painter(&frame);
    painter.drawImage((box - scaled.width()) / 2, (box - scaled.height()) / 2, scaled);
    return frame;
}

// Crossfade interpolando os quatro canais. Compor uma por cima da outra com
// setOpacity não serve aqui: as artes têm silhuetas ligeiramente diferentes
// (foram geradas em rodadas separadas), então ou sobra resíduo da anterior nas
// bordas, ou a letra pisca translúcida no meio do caminho. Com as imagens em
// premultiplicado, interpolar RGB e alpha linearmente é o resultado correto.
QPixmap crossfadedLogo(const QImage& from, const QImage& to, qreal progress)
{
    if (from.size() != to.size()) return QPixmap::fromImage(to);

    QImage out(from.size(), QImage::Format_ARGB32_Premultiplied);
    const int count = from.width() * from.height();
    const auto* a = reinterpret_cast<const QRgb*>(from.constBits());
    const auto* b = reinterpret_cast<const QRgb*>(to.constBits());
    auto* dst = reinterpret_cast<QRgb*>(out.bits());

    const int w = qBound(0, int(progress * 256), 256);
    for (int i = 0; i < count; ++i) {
        dst[i] = qRgba((qRed(a[i])   * (256 - w) + qRed(b[i])   * w) >> 8,
                       (qGreen(a[i]) * (256 - w) + qGreen(b[i]) * w) >> 8,
                       (qBlue(a[i])  * (256 - w) + qBlue(b[i])  * w) >> 8,
                       (qAlpha(a[i]) * (256 - w) + qAlpha(b[i]) * w) >> 8);
    }
    return QPixmap::fromImage(out);
}
constexpr int kEditCoverW = 260; // capa grande do diálogo Editar projeto
constexpr int kEditCoverH = 390;

// Padding ao redor da capa pra acomodar sombra projetada + bloco de páginas.
constexpr int kVitPadL = 2;
constexpr int kVitPadT = 3;
constexpr int kVitPadR = 19;
constexpr int kVitPadB = 17;

// Metadados por manuscrito, já com fallback resolvido pro projeto — réplica
// em JSON cru da lógica de ProjectModel::manuscriptEffectiveTitle/
// CoverDataUrl (este arquivo não carrega ProjectModel, só lê o índice do
// disco). Sempre populado (mesmo com 1 manuscrito só) — barato, é só
// QString, decode de imagem fica por conta de quem consome.
struct ManuscriptCoverInfo {
    QString id;
    QString title;
    QString coverDataUrl;
};

struct RecentInfo {
    bool valid = false;
    QString name;
    QString author;
    QString genres;
    QString synopsis;
    QString coverDataUrl;
    int     totalWords = -1; // -1 = ainda não cacheado (projeto não resalvo desde essa feature)
    int     manuscriptCount = 0; // direto de "data.manuscripts" no índice — sempre disponível
    int     chapterCount    = 0; // direto de "chapters" no índice — sempre disponível
    int     documentCount   = 0; // soma de "drawers[].items" no índice — sempre disponível
    QList<ManuscriptCoverInfo> manuscripts; // um item por manuscrito, título/capa já efetivos
    QJsonArray  chapters;     // capítulos crus do índice (título, ordem, status) — Onde parei e progresso
    QJsonObject wordCounter;  // settings.wordCounter — meta, progresso por dia, folgas

    // --- Lombada da Prateleira 3D (persistidos em projectDetails, mesmo
    // esquema de campos do Mira 1) ---
    QString coverBgDataUrl;     // render sem texto (Cover Creator) — textura "capa do livro"
    QString spineColor;         // hex; vazio = cor-padrão por índice
    QString spineImageTexture;  // "" / "none" | "cover"
    int     spineBgPosX = 0;    // 0-100, posição horizontal quando spineImageTexture=="cover"
    QString spineTexture;       // "" / "none" | couro | linho | veludo | fosco | metalico
    QString spineFontFamily;    // vazio = Alegreya (padrão)
    QString spineFontColor;     // vazio = creme (padrão)
    int     spineFontSize = 0;  // 0 = automático (proporcional à largura)
    QString spineTextOrientation; // "vertical" (padrão) | "horizontal"
    QString spineTextPosition;   // "top" | "center" (padrão) | "bottom"
    QString spineWidthMode;      // "auto" (padrão) | "manual"
    int     spineWidthManual = 0;
};

// Lê o JSON do projeto e extrai só os campos exibidos no card. Sem carregar
// no ProjectModel — é só leitura passiva.
RecentInfo readRecentInfo(const QString& rootPath)
{
    RecentInfo info;
    const QString idx = ProjectStorage::indexPath(rootPath);
    if (!QFile::exists(idx)) return info;
    QFile f(idx);
    if (!f.open(QIODevice::ReadOnly)) return info;
    const QByteArray raw = f.readAll();
    f.close();
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return info;
    const QJsonObject root = doc.object();
    info.valid = true;
    // "projectName" é a chave gravada pelo save normal (ProjectModel::toJson);
    // "name" só existe em projetos editados pelo diálogo do menu (compat) —
    // sem esse fallback, renomear pelo Painel de Informações dentro do editor
    // nunca refletia no menu, e o card ficava preso no nome da pasta.
    info.name = root.value(QStringLiteral("projectName")).toString();
    if (info.name.isEmpty()) info.name = root.value(QStringLiteral("name")).toString();
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const QJsonObject details = data.value(QStringLiteral("projectDetails")).toObject();
    info.author = details.value(QStringLiteral("author")).toString();
    info.genres = details.value(QStringLiteral("genres")).toString();
    info.synopsis = details.value(QStringLiteral("synopsis")).toString();
    if (details.contains(QStringLiteral("totalWords")))
        info.totalWords = details.value(QStringLiteral("totalWords")).toInt(-1);
    // Manuscritos/capítulos/documentos não precisam de cache à parte — já
    // vêm de graça no próprio índice ("data.manuscripts"/"chapters"/
    // "drawers" fazem parte da estrutura básica do projeto, gravados
    // sempre, ao contrário de totalWords que só existe depois de um save
    // com o WordCounter já ter rodado).
    const QJsonArray msArr = data.value(QStringLiteral("manuscripts")).toArray();
    info.manuscriptCount = msArr.size();
    info.chapters = root.value(QStringLiteral("chapters")).toArray();
    info.chapterCount = info.chapters.size();
    info.wordCounter = root.value(QStringLiteral("settings")).toObject()
                           .value(QStringLiteral("wordCounter")).toObject();
    int docCount = 0;
    for (const auto& dv : root.value(QStringLiteral("drawers")).toArray())
        docCount += dv.toObject().value(QStringLiteral("items")).toArray().size();
    info.documentCount = docCount;
    // Compat Mira 1: ProjectModel grava em "coverFull"/"cover" (não em
    // "coverDataUrl"). Prefere coverFull (full res), cai pra cover.
    info.coverDataUrl = details.value(QStringLiteral("coverFull")).toString();
    if (info.coverDataUrl.isEmpty()) {
        info.coverDataUrl = details.value(QStringLiteral("cover")).toString();
    }
    info.coverBgDataUrl = details.value(QStringLiteral("coverBg")).toString();
    info.spineColor          = details.value(QStringLiteral("spineColor")).toString();
    info.spineImageTexture   = details.value(QStringLiteral("spineImageTexture")).toString();
    info.spineBgPosX         = details.value(QStringLiteral("spineBgPosX")).toInt(0);
    info.spineTexture        = details.value(QStringLiteral("spineTexture")).toString();
    info.spineFontFamily     = details.value(QStringLiteral("spineFontFamily")).toString();
    info.spineFontColor      = details.value(QStringLiteral("spineFontColor")).toString();
    info.spineFontSize       = details.value(QStringLiteral("spineFontSize")).toInt(0);
    info.spineTextOrientation = details.value(QStringLiteral("spineTextOrientation")).toString();
    info.spineTextPosition    = details.value(QStringLiteral("spineTextPosition")).toString();
    info.spineWidthMode       = details.value(QStringLiteral("spineWidthMode")).toString();
    info.spineWidthManual     = details.value(QStringLiteral("spineWidthManual")).toInt(0);
    if (info.name.isEmpty()) {
        info.name = QFileInfo(rootPath).fileName();
    }
    // Título/capa efetivos por manuscrito (réplica de
    // ProjectModel::manuscriptEffectiveTitle/CoverDataUrl em JSON cru) —
    // barato: só strings, sem decodificar imagem alguma aqui. Deduplicado
    // por capa: quando nenhum manuscrito tem capa própria, todos caem no
    // MESMO fallback (a capa do projeto) — sem isso, os 3 pontos de
    // exibição (Lista/Prateleira/Pilha) tratariam isso como "N capas
    // diferentes" e repetiriam a mesma imagem várias vezes.
    QSet<QString> seenCovers;
    for (const auto& mv : msArr) {
        const QJsonObject mo = mv.toObject();
        ManuscriptCoverInfo mi;
        mi.id = mo.value(QStringLiteral("id")).toString();
        const QString title = mo.value(QStringLiteral("title")).toString();
        mi.title = title.trimmed().isEmpty() ? info.name : title;
        const QString cover = mo.value(QStringLiteral("coverDataUrl")).toString();
        mi.coverDataUrl = cover.isEmpty() ? info.coverDataUrl : cover;
        if (seenCovers.contains(mi.coverDataUrl)) continue;
        seenCovers.insert(mi.coverDataUrl);
        info.manuscripts.append(mi);
    }
    return info;
}

QPixmap decodeCoverDataUrl(const QString& dataUrl)
{
    if (dataUrl.isEmpty()) return {};
    const int comma = dataUrl.indexOf(QLatin1Char(','));
    if (comma < 0) return {};
    const QByteArray raw = QByteArray::fromBase64(dataUrl.mid(comma + 1).toLatin1());
    QPixmap pm;
    pm.loadFromData(raw);
    return pm;
}

// Carrega imagem do disco, reduz pra caber em kMaxCoverSide mantendo proporção
// e devolve data URL JPEG. Mesmo tratamento do ProjectInfoPanel (capa retrato,
// sem crop forçado).
constexpr int kMaxCoverSide = 1200;
QString loadCoverAsDataUrl(const QString& path)
{
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

// Capa gerada quando o projeto não tem uma própria. Gradiente sóbrio + nome
// (serif bold) + autor (serif menor) — placeholder visual, não toca o projeto.
QPixmap renderDefaultCover(const QString& name, const QString& author, int w, int h)
{
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    QLinearGradient grad(0, 0, 0, h);
    grad.setColorAt(0.0, QColor("#2a2f3a"));
    grad.setColorAt(1.0, QColor("#161922"));
    QPainterPath rounded;
    rounded.addRoundedRect(0, 0, w, h, 4, 4);
    p.fillPath(rounded, grad);

    p.setPen(QPen(QColor(255, 255, 255, 30), 1));
    p.drawPath(rounded);

    QFont serif(QStringLiteral("Alegreya"));
    serif.setStyleHint(QFont::Serif);

    const QRect nameRect(6, int(h * 0.20), w - 12, int(h * 0.55));
    QFont nameFont = serif;
    nameFont.setBold(true);
    nameFont.setPixelSize(qMax(8, int(w * 0.13)));
    p.setFont(nameFont);
    p.setPen(QColor(238, 232, 213));
    p.drawText(nameRect, Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap,
               name.isEmpty() ? QStringLiteral("Sem nome") : name);

    if (!author.isEmpty()) {
        QFont authorFont = serif;
        authorFont.setItalic(true);
        authorFont.setPixelSize(qMax(7, int(w * 0.085)));
        p.setFont(authorFont);
        p.setPen(QColor(200, 195, 180));
        const QRect authorRect(6, h - int(h * 0.22), w - 12, int(h * 0.18));
        p.drawText(authorRect, Qt::AlignHCenter | Qt::AlignVCenter,
                   QStringLiteral("— %1").arg(author));
    }
    return pm;
}

// Embrulha a capa num "livro de vitrine": sombra projetada, bloco de páginas
// na borda direita e vinco de lombada à esquerda — um 3D sutil, de frente.
// A área de capa fica exatamente w×h; a sombra/páginas extrapolam via padding.
QPixmap renderVitrineCover(const QPixmap& coverIn, int w, int h)
{
    const int cw = kVitPadL + w + kVitPadR;
    const int ch = kVitPadT + h + kVitPadB;
    QPixmap pm(cw, ch);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF coverRect(kVitPadL, kVitPadT, w, h);
    const qreal radius = 4.0;

    // 1) Sombra projetada — empilha rects translúcidos pra aproximar um blur.
    for (int i = 8; i >= 1; --i) {
        const qreal off = i * 1.1;
        QPainterPath sp;
        sp.addRoundedRect(coverRect.translated(off * 0.45, off), radius + i, radius + i);
        p.fillPath(sp, QColor(0, 0, 0, 14));
    }

    // 2) Bloco de páginas na borda direita (creme), recuado em cima/baixo.
    for (int i = 5; i >= 1; --i) {
        const qreal x = coverRect.right() - 1 + i * 1.5;
        const qreal inset = i * 0.7;
        const QColor edge = (i % 2 == 0) ? QColor(232, 226, 210) : QColor(208, 200, 182);
        p.setPen(QPen(edge, 1.6));
        p.drawLine(QPointF(x, coverRect.top() + 2 + inset),
                   QPointF(x, coverRect.bottom() - 2 - inset));
    }

    // 3) Capa em si, recortada em cantos arredondados.
    QPainterPath clip;
    clip.addRoundedRect(coverRect, radius, radius);
    p.save();
    p.setClipPath(clip);
    const QPixmap scaled = coverIn.scaled(int(coverRect.width()), int(coverRect.height()),
                                          Qt::KeepAspectRatioByExpanding,
                                          Qt::SmoothTransformation);
    const qreal dx = coverRect.left() - (scaled.width() - coverRect.width()) / 2.0;
    const qreal dy = coverRect.top() - (scaled.height() - coverRect.height()) / 2.0;
    p.drawPixmap(QPointF(dx, dy), scaled);

    // 4) Vinco de lombada: gradiente escuro→claro na borda esquerda.
    QLinearGradient spine(coverRect.left(), 0, coverRect.left() + 13, 0);
    spine.setColorAt(0.0, QColor(0, 0, 0, 90));
    spine.setColorAt(0.35, QColor(0, 0, 0, 20));
    spine.setColorAt(0.55, QColor(255, 255, 255, 28));
    spine.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(QRectF(coverRect.left(), coverRect.top(), 13, coverRect.height()), spine);
    p.restore();

    // 5) Contorno sutil.
    p.setPen(QPen(QColor(255, 255, 255, 28), 1));
    p.drawPath(clip);
    return pm;
}

// Diálogo de edição dos metadados do projeto (nome, autor, gêneros, sinopse,
// capa). Não toca no ProjectModel — quem grava é o MainMenuDialog, direto no
// índice. Visual espelhado no ProjectInfoPanel ("Informações da obra"): capa
// grande à esquerda, formulário com labels empilhados à direita, QSS de tema.
// Sem Q_OBJECT: connect por PMF (funciona com receiver QObject sem moc).
class ProjectEditDialog : public QDialog {
public:
    explicit ProjectEditDialog(QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setObjectName(QStringLiteral("projectEditDialog"));
        setWindowTitle(QCoreApplication::translate("ProjectEditDialog", "Editar projeto"));
        setModal(true);
        resize(820, 560);

        auto* root = new QHBoxLayout(this);
        root->setContentsMargins(22, 22, 22, 18);
        root->setSpacing(24);

        // ---- Coluna esquerda: capa grande + ações ----
        auto* leftCol = new QVBoxLayout();
        leftCol->setSpacing(10);

        m_coverPreview = new QLabel(this);
        m_coverPreview->setObjectName(QStringLiteral("projectInfoCover"));
        m_coverPreview->setFixedSize(kEditCoverW, kEditCoverH);
        m_coverPreview->setAlignment(Qt::AlignCenter);
        m_coverPreview->setText(QCoreApplication::translate("ProjectEditDialog", "Sem capa"));
        m_coverPreview->setScaledContents(false);
        leftCol->addWidget(m_coverPreview);

        auto* pickBtn = new QPushButton(QCoreApplication::translate("ProjectEditDialog", "Escolher capa…"), this);
        pickBtn->setObjectName(QStringLiteral("projectInfoBtn"));
        pickBtn->setCursor(Qt::PointingHandCursor);
        QObject::connect(pickBtn, &QPushButton::clicked, this, [this]() {
            const QString startDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
            const QString file = QFileDialog::getOpenFileName(
                this, QCoreApplication::translate("ProjectEditDialog", "Escolher capa"), startDir,
                QCoreApplication::translate("ProjectEditDialog", "Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
            if (file.isEmpty()) return;
            const QString dataUrl = loadCoverAsDataUrl(file);
            if (dataUrl.isEmpty()) return;
            m_coverDataUrl = dataUrl;
            updateCoverPreview();
        });
        leftCol->addWidget(pickBtn);

        auto* clearBtn = new QPushButton(QCoreApplication::translate("ProjectEditDialog", "Remover capa"), this);
        clearBtn->setObjectName(QStringLiteral("projectInfoBtn"));
        clearBtn->setCursor(Qt::PointingHandCursor);
        QObject::connect(clearBtn, &QPushButton::clicked, this, [this]() {
            if (m_coverDataUrl.isEmpty()) return;
            m_coverDataUrl.clear();
            updateCoverPreview();
        });
        leftCol->addWidget(clearBtn);

        auto* coverCreateBtn = new QPushButton(QCoreApplication::translate("ProjectEditDialog", "Criar capa…"), this);
        coverCreateBtn->setObjectName(QStringLiteral("projectInfoBtn"));
        coverCreateBtn->setCursor(Qt::PointingHandCursor);
        QObject::connect(coverCreateBtn, &QPushButton::clicked, this, [this]() {
            // Fecha o diálogo (sem salvar as outras edições desta sessão,
            // igual à ação equivalente no menu de contexto) e sinaliza pro
            // MainMenuDialog abrir o Cover Creator de verdade.
            m_coverCreateRequested = true;
            reject();
        });
        leftCol->addWidget(coverCreateBtn);
        leftCol->addStretch();
        root->addLayout(leftCol);

        // ---- Coluna direita: header + abas (Detalhes / Lombada) + footer ----
        auto* rightCol = new QVBoxLayout();
        rightCol->setSpacing(10);

        auto* heading = new QLabel(QCoreApplication::translate("ProjectEditDialog", "Detalhes da obra"), this);
        heading->setObjectName(QStringLiteral("projectInfoHeading"));
        rightCol->addWidget(heading);

        auto* tabs = new QTabWidget(this);
        tabs->setObjectName(QStringLiteral("projectEditTabs"));

        // ---- Aba "Detalhes" ----
        auto* detailsPage = new QWidget(tabs);
        auto* detailsCol = new QVBoxLayout(detailsPage);
        detailsCol->setContentsMargins(0, 8, 0, 0);
        detailsCol->setSpacing(8);

        auto addLabel = [this, detailsCol](const QString& text) {
            auto* lab = new QLabel(text, this);
            lab->setObjectName(QStringLiteral("projectInfoLabel"));
            detailsCol->addWidget(lab);
        };

        addLabel(QCoreApplication::translate("ProjectEditDialog", "Nome do projeto"));
        m_nameEdit = new QLineEdit(this);
        m_nameEdit->setPlaceholderText(QCoreApplication::translate("ProjectEditDialog", "Ex: Minha história"));
        detailsCol->addWidget(m_nameEdit);

        addLabel(QCoreApplication::translate("ProjectEditDialog", "Autor"));
        m_authorEdit = new QLineEdit(this);
        m_authorEdit->setPlaceholderText(QCoreApplication::translate("ProjectEditDialog", "Ex: Maria Silva"));
        detailsCol->addWidget(m_authorEdit);

        addLabel(QCoreApplication::translate("ProjectEditDialog", "Gêneros"));
        m_genresEdit = new QLineEdit(this);
        m_genresEdit->setPlaceholderText(QCoreApplication::translate("ProjectEditDialog", "Ex: Fantasia, Romance"));
        detailsCol->addWidget(m_genresEdit);

        addLabel(QCoreApplication::translate("ProjectEditDialog", "Sinopse"));
        m_synopsisEdit = new QPlainTextEdit(this);
        m_synopsisEdit->setPlaceholderText(QCoreApplication::translate("ProjectEditDialog", "Escreva uma breve sinopse…"));
        detailsCol->addWidget(m_synopsisEdit, /*stretch=*/1);

        tabs->addTab(detailsPage, QCoreApplication::translate("ProjectEditDialog", "Detalhes"));

        // ---- Aba "Lombada" (Prateleira 3D) ----
        auto* spinePage = new QWidget(tabs);
        auto* spineCol = new QVBoxLayout(spinePage);
        spineCol->setContentsMargins(0, 8, 0, 0);
        spineCol->setSpacing(8);

        auto mkSpineLabel = [this](const QString& text) {
            auto* lab = new QLabel(text, this);
            lab->setObjectName(QStringLiteral("projectInfoLabel"));
            return lab;
        };
        auto addSpineLabel = [this, spineCol, mkSpineLabel](const QString& text) {
            spineCol->addWidget(mkSpineLabel(text));
        };

        // Cor da lombada (vazio = cor-padrão por posição na prateleira)
        addSpineLabel(QCoreApplication::translate("ProjectEditDialog", "Cor da lombada"));
        {
            auto* row = new QHBoxLayout();
            row->setSpacing(6);
            m_spineColorBtn = new QPushButton(this);
            m_spineColorBtn->setObjectName(QStringLiteral("spineColorSwatch"));
            m_spineColorBtn->setFixedSize(30, 26);
            m_spineColorBtn->setCursor(Qt::PointingHandCursor);
            QObject::connect(m_spineColorBtn, &QPushButton::clicked, this, [this]() {
                const QColor seed = m_spineColor.isValid() ? m_spineColor : QColor(QStringLiteral("#7a1e28"));
                const QColor c = ColorPopover::getColor(seed, this,
                    QCoreApplication::translate("ProjectEditDialog", "Cor da lombada"));
                if (c.isValid()) { m_spineColor = c; updateSwatches(); }
            });
            auto* resetBtn = new QPushButton(QCoreApplication::translate("ProjectEditDialog", "Padrão"), this);
            resetBtn->setObjectName(QStringLiteral("projectInfoBtn"));
            resetBtn->setCursor(Qt::PointingHandCursor);
            QObject::connect(resetBtn, &QPushButton::clicked, this, [this]() {
                m_spineColor = QColor();
                updateSwatches();
            });
            row->addWidget(m_spineColorBtn);
            row->addWidget(resetBtn);
            row->addStretch();
            spineCol->addLayout(row);
        }

        // Base: cor sólida ou a própria capa (fatia horizontal, ajustável)
        addSpineLabel(QCoreApplication::translate("ProjectEditDialog", "Base da lombada"));
        m_baseCombo = new QComboBox(this);
        m_baseCombo->addItem(QCoreApplication::translate("ProjectEditDialog", "Cor sólida"),
                             QStringLiteral("none"));
        m_baseCombo->addItem(QCoreApplication::translate("ProjectEditDialog", "Usar a capa como textura"),
                             QStringLiteral("cover"));
        spineCol->addWidget(m_baseCombo);

        auto* posRow = new QHBoxLayout();
        posRow->setSpacing(8);
        m_bgPosLabel = new QLabel(QCoreApplication::translate("ProjectEditDialog", "Posição:"), this);
        m_bgPosLabel->setObjectName(QStringLiteral("projectInfoLabel"));
        m_bgPosSlider = new QSlider(Qt::Horizontal, this);
        m_bgPosSlider->setRange(0, 100);
        m_bgPosValueLabel = new QLabel(QStringLiteral("0%"), this);
        m_bgPosValueLabel->setObjectName(QStringLiteral("projectInfoLabel"));
        QObject::connect(m_bgPosSlider, &QSlider::valueChanged, this, [this](int v) {
            m_bgPosValueLabel->setText(QStringLiteral("%1%").arg(v));
        });
        posRow->addWidget(m_bgPosLabel);
        posRow->addWidget(m_bgPosSlider, 1);
        posRow->addWidget(m_bgPosValueLabel);
        spineCol->addLayout(posRow);

        QObject::connect(m_baseCombo, &QComboBox::currentIndexChanged, this, [this](int) {
            const bool useCover = m_baseCombo->currentData().toString() == QStringLiteral("cover");
            m_bgPosLabel->setEnabled(useCover);
            m_bgPosSlider->setEnabled(useCover);
            m_bgPosValueLabel->setEnabled(useCover);
        });

        // Acabamento (textura procedural por cima da base)
        addSpineLabel(QCoreApplication::translate("ProjectEditDialog", "Acabamento"));
        m_finishCombo = new QComboBox(this);
        m_finishCombo->addItem(QCoreApplication::translate("ProjectEditDialog", "Liso"), QStringLiteral("none"));
        m_finishCombo->addItem(QCoreApplication::translate("ProjectEditDialog", "Couro"), QStringLiteral("couro"));
        m_finishCombo->addItem(QCoreApplication::translate("ProjectEditDialog", "Linho"), QStringLiteral("linho"));
        m_finishCombo->addItem(QCoreApplication::translate("ProjectEditDialog", "Veludo"), QStringLiteral("veludo"));
        m_finishCombo->addItem(QCoreApplication::translate("ProjectEditDialog", "Fosco"), QStringLiteral("fosco"));
        m_finishCombo->addItem(QCoreApplication::translate("ProjectEditDialog", "Metálico"), QStringLiteral("metalico"));
        spineCol->addWidget(m_finishCombo);

        // Texto: fonte, cor, tamanho, orientação, posição
        auto* fontRow = new QHBoxLayout();
        fontRow->setSpacing(10);
        {
            auto* col1 = new QVBoxLayout();
            col1->addWidget(mkSpineLabel(QCoreApplication::translate("ProjectEditDialog", "Fonte")));
            m_fontCombo = new QComboBox(this);
            m_fontCombo->addItem(QStringLiteral("Alegreya"));
            m_fontCombo->addItem(QStringLiteral("EB Garamond"));
            m_fontCombo->addItem(QStringLiteral("Inter"));
            m_fontCombo->addItem(QStringLiteral("Crimson Text"));
            col1->addWidget(m_fontCombo);
            fontRow->addLayout(col1, 1);

            auto* col2 = new QVBoxLayout();
            col2->addWidget(mkSpineLabel(QCoreApplication::translate("ProjectEditDialog", "Cor do texto")));
            auto* textColorRow = new QHBoxLayout();
            m_fontColorBtn = new QPushButton(this);
            m_fontColorBtn->setObjectName(QStringLiteral("spineColorSwatch"));
            m_fontColorBtn->setFixedSize(30, 26);
            m_fontColorBtn->setCursor(Qt::PointingHandCursor);
            QObject::connect(m_fontColorBtn, &QPushButton::clicked, this, [this]() {
                const QColor seed = m_fontColor.isValid() ? m_fontColor : QColor(245, 240, 226);
                const QColor c = ColorPopover::getColor(seed, this,
                    QCoreApplication::translate("ProjectEditDialog", "Cor do texto"));
                if (c.isValid()) { m_fontColor = c; updateSwatches(); }
            });
            auto* fontResetBtn = new QPushButton(QCoreApplication::translate("ProjectEditDialog", "Padrão"), this);
            fontResetBtn->setObjectName(QStringLiteral("projectInfoBtn"));
            fontResetBtn->setCursor(Qt::PointingHandCursor);
            QObject::connect(fontResetBtn, &QPushButton::clicked, this, [this]() {
                m_fontColor = QColor();
                updateSwatches();
            });
            textColorRow->addWidget(m_fontColorBtn);
            textColorRow->addWidget(fontResetBtn);
            col2->addLayout(textColorRow);
            fontRow->addLayout(col2, 1);
        }
        spineCol->addLayout(fontRow);

        auto* orientRow = new QHBoxLayout();
        orientRow->setSpacing(10);
        {
            auto* col1 = new QVBoxLayout();
            col1->addWidget(mkSpineLabel(QCoreApplication::translate("ProjectEditDialog", "Orientação")));
            m_orientCombo = new QComboBox(this);
            m_orientCombo->addItem(QCoreApplication::translate("ProjectEditDialog", "Vertical"), QStringLiteral("vertical"));
            m_orientCombo->addItem(QCoreApplication::translate("ProjectEditDialog", "Horizontal"), QStringLiteral("horizontal"));
            col1->addWidget(m_orientCombo);
            orientRow->addLayout(col1, 1);

            auto* col2 = new QVBoxLayout();
            col2->addWidget(mkSpineLabel(QCoreApplication::translate("ProjectEditDialog", "Posição do texto")));
            m_textPosCombo = new QComboBox(this);
            m_textPosCombo->addItem(QCoreApplication::translate("ProjectEditDialog", "Topo"), QStringLiteral("top"));
            m_textPosCombo->addItem(QCoreApplication::translate("ProjectEditDialog", "Centro"), QStringLiteral("center"));
            m_textPosCombo->addItem(QCoreApplication::translate("ProjectEditDialog", "Baixo"), QStringLiteral("bottom"));
            col2->addWidget(m_textPosCombo);
            orientRow->addLayout(col2, 1);

            auto* col3 = new QVBoxLayout();
            col3->addWidget(mkSpineLabel(QCoreApplication::translate("ProjectEditDialog", "Tam. da fonte (0=auto)")));
            m_fontSizeSpin = new QSpinBox(this);
            m_fontSizeSpin->setRange(0, 48);
            col3->addWidget(m_fontSizeSpin);
            orientRow->addLayout(col3, 1);
        }
        spineCol->addLayout(orientRow);

        // Largura da lombada
        addSpineLabel(QCoreApplication::translate("ProjectEditDialog", "Largura da lombada"));
        auto* widthRow = new QHBoxLayout();
        widthRow->setSpacing(8);
        m_widthCombo = new QComboBox(this);
        m_widthCombo->addItem(QCoreApplication::translate("ProjectEditDialog", "Conforme o projeto"), QStringLiteral("auto"));
        m_widthCombo->addItem(QCoreApplication::translate("ProjectEditDialog", "Manual"), QStringLiteral("manual"));
        m_widthSpin = new QSpinBox(this);
        m_widthSpin->setRange(10, 240);
        m_widthSpin->setSuffix(QStringLiteral(" px"));
        m_widthSpin->setEnabled(false);
        QObject::connect(m_widthCombo, &QComboBox::currentIndexChanged, this, [this](int) {
            m_widthSpin->setEnabled(m_widthCombo->currentData().toString() == QStringLiteral("manual"));
        });
        widthRow->addWidget(m_widthCombo, 1);
        widthRow->addWidget(m_widthSpin);
        spineCol->addLayout(widthRow);

        spineCol->addStretch(1);
        // A Prateleira 3D saiu do menu: a aba Lombada não tem mais o que
        // mostrar. Os campos seguem vivos (escondidos) pra que salvar
        // preserve o que o projeto já tinha gravado.
        spinePage->hide();
        tabs->tabBar()->hide();

        rightCol->addWidget(tabs, /*stretch=*/1);

        auto* actions = new QHBoxLayout();
        actions->setSpacing(8);
        actions->addStretch();
        auto* cancelBtn = new QPushButton(QCoreApplication::translate("ProjectEditDialog", "Cancelar"), this);
        cancelBtn->setObjectName(QStringLiteral("projectInfoBtn"));
        cancelBtn->setCursor(Qt::PointingHandCursor);
        QObject::connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        auto* saveBtn = new QPushButton(QCoreApplication::translate("ProjectEditDialog", "Salvar"), this);
        saveBtn->setObjectName(QStringLiteral("projectInfoBtn"));
        saveBtn->setCursor(Qt::PointingHandCursor);
        saveBtn->setDefault(true);
        QObject::connect(saveBtn, &QPushButton::clicked, this, &QDialog::accept);
        actions->addWidget(cancelBtn);
        actions->addWidget(saveBtn);
        rightCol->addLayout(actions);

        root->addLayout(rightCol, /*stretch=*/1);

        applyDialogStyle();
        connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
                this, &ProjectEditDialog::applyDialogStyle);
    }

    void setValues(const QString& name, const QString& author,
                   const QString& genres, const QString& synopsis,
                   const QString& coverDataUrl)
    {
        m_nameEdit->setText(name);
        m_authorEdit->setText(author);
        m_genresEdit->setText(genres);
        m_synopsisEdit->setPlainText(synopsis);
        m_coverDataUrl = coverDataUrl;
        updateCoverPreview();
    }

    QString name() const     { return m_nameEdit->text().trimmed(); }
    QString author() const   { return m_authorEdit->text().trimmed(); }
    QString genres() const   { return m_genresEdit->text().trimmed(); }
    QString synopsis() const { return m_synopsisEdit->toPlainText().trimmed(); }
    QString coverDataUrl() const { return m_coverDataUrl; }
    // true quando o usuário clicou "Criar capa…" (o diálogo fecha via
    // reject() nesse caso — quem chamou exec() precisa checar isso antes de
    // tratar o retorno como "cancelou").
    bool coverCreateRequested() const { return m_coverCreateRequested; }

    // Preenche a aba "Lombada" a partir de um RecentInfo já lido do JSON.
    void setSpineValues(const RecentInfo& info) {
        m_spineColor = info.spineColor.isEmpty() ? QColor() : QColor(info.spineColor);
        m_fontColor  = info.spineFontColor.isEmpty() ? QColor() : QColor(info.spineFontColor);
        updateSwatches();

        // Sem escolha salva ainda: default é usar a capa (se existir alguma
        // imagem pra usar) — só cai pra cor sólida se o campo já foi
        // explicitamente deixado em "none", ou se não há capa nenhuma.
        const bool hasCoverImage = !info.coverDataUrl.isEmpty() || !info.coverBgDataUrl.isEmpty();
        QString baseChoice = info.spineImageTexture;
        if (baseChoice.isEmpty()) baseChoice = hasCoverImage ? QStringLiteral("cover") : QStringLiteral("none");
        const int baseIdx = m_baseCombo->findData(baseChoice);
        m_baseCombo->setCurrentIndex(qMax(0, baseIdx));
        m_bgPosSlider->setValue(qBound(0, info.spineBgPosX, 100));

        const int finishIdx = m_finishCombo->findData(
            info.spineTexture.isEmpty() ? QStringLiteral("none") : info.spineTexture);
        m_finishCombo->setCurrentIndex(qMax(0, finishIdx));

        const int fontIdx = m_fontCombo->findText(
            info.spineFontFamily.isEmpty() ? QStringLiteral("Alegreya") : info.spineFontFamily);
        m_fontCombo->setCurrentIndex(qMax(0, fontIdx));

        m_fontSizeSpin->setValue(qMax(0, info.spineFontSize));

        const int orientIdx = m_orientCombo->findData(
            info.spineTextOrientation.isEmpty() ? QStringLiteral("vertical") : info.spineTextOrientation);
        m_orientCombo->setCurrentIndex(qMax(0, orientIdx));

        const int posIdx = m_textPosCombo->findData(
            info.spineTextPosition.isEmpty() ? QStringLiteral("center") : info.spineTextPosition);
        m_textPosCombo->setCurrentIndex(qMax(0, posIdx));

        const int widthIdx = m_widthCombo->findData(
            info.spineWidthMode == QStringLiteral("manual") ? QStringLiteral("manual") : QStringLiteral("auto"));
        m_widthCombo->setCurrentIndex(qMax(0, widthIdx));
        m_widthSpin->setValue(info.spineWidthManual > 0 ? info.spineWidthManual : 104);
    }

    QString spineColor() const { return m_spineColor.isValid() ? m_spineColor.name() : QString(); }
    // Ao contrário dos outros getters, "none" aqui é salvo explicitamente
    // (não vira string vazia) — senão não daria pra distinguir "nunca
    // configurado" de "escolheu cor sólida de propósito", e o padrão de usar
    // a capa (ver populateActiveView) reverteria a escolha do usuário toda
    // vez que ele salvasse o diálogo por outro motivo qualquer.
    QString spineImageTexture() const { return m_baseCombo->currentData().toString(); }
    int     spineBgPosX() const { return m_bgPosSlider->value(); }
    QString spineTexture() const {
        const QString v = m_finishCombo->currentData().toString();
        return v == QStringLiteral("none") ? QString() : v;
    }
    QString spineFontFamily() const {
        const QString v = m_fontCombo->currentText();
        return v == QStringLiteral("Alegreya") ? QString() : v;
    }
    QString spineFontColor() const { return m_fontColor.isValid() ? m_fontColor.name() : QString(); }
    int     spineFontSize() const { return m_fontSizeSpin->value(); }
    QString spineTextOrientation() const {
        const QString v = m_orientCombo->currentData().toString();
        return v == QStringLiteral("vertical") ? QString() : v;
    }
    QString spineTextPosition() const {
        const QString v = m_textPosCombo->currentData().toString();
        return v == QStringLiteral("center") ? QString() : v;
    }
    QString spineWidthMode() const {
        const QString v = m_widthCombo->currentData().toString();
        return v == QStringLiteral("auto") ? QString() : v;
    }
    int     spineWidthManual() const {
        return spineWidthMode().isEmpty() ? 0 : m_widthSpin->value();
    }

private:
    void updateSwatches() {
        auto paintSwatch = [](QPushButton* btn, const QColor& c, const QColor& fallback) {
            const QColor use = c.isValid() ? c : fallback;
            btn->setStyleSheet(Theme::qss(QStringLiteral("background-color: %1; border-radius: @radius-item;")).arg(use.name()));
        };
        if (m_spineColorBtn) paintSwatch(m_spineColorBtn, m_spineColor, QColor(QStringLiteral("#7a1e28")));
        if (m_fontColorBtn)  paintSwatch(m_fontColorBtn, m_fontColor, QColor(245, 240, 226));
    }

    void updateCoverPreview() {
        QPixmap pm = decodeCoverDataUrl(m_coverDataUrl);
        if (pm.isNull()) {
            m_coverPreview->clear();
            m_coverPreview->setText(QCoreApplication::translate("ProjectEditDialog", "Sem capa"));
            return;
        }
        m_coverPreview->setPixmap(pm.scaled(kEditCoverW, kEditCoverH,
                                            Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
    }

    void applyDialogStyle() {
        setStyleSheet(Theme::qss(QStringLiteral(R"(
            #projectEditDialog { background: %1; }
            #projectEditDialog QLabel { color: %2; font-size: 12px; }
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
                border-radius: @radius-control;
                font-size: 11px;
            }
            #projectEditDialog QLineEdit,
            #projectEditDialog QPlainTextEdit {
                background: %5;
                color: %3;
                border: 1px solid %6;
                border-radius: @radius-control;
                padding: 6px 8px;
                selection-background-color: %7;
            }
            #projectEditDialog QLineEdit:focus,
            #projectEditDialog QPlainTextEdit:focus {
                border-color: %9;
            }
            QPushButton#projectInfoBtn {
                background: %5;
                color: %2;
                border: 1px solid %6;
                padding: 6px 14px;
                border-radius: @radius-control;
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
            #projectEditTabs::pane { border: 1px solid %6; border-radius: @radius-control; top: -1px; }
            #projectEditTabs QTabBar::tab {
                background: %5;
                color: %4;
                border: 1px solid %6;
                border-bottom: none;
                padding: 5px 12px;
                border-top-left-radius: @radius-control;
                border-top-right-radius: @radius-control;
            }
            #projectEditTabs QTabBar::tab:selected { color: %3; background: %1; }
            #projectEditDialog QComboBox,
            #projectEditDialog QSpinBox {
                background: %5;
                color: %3;
                border: 1px solid %6;
                border-radius: @radius-control;
                padding: 4px 8px;
                min-height: 22px;
            }
            #projectEditDialog QComboBox:focus,
            #projectEditDialog QSpinBox:focus { border-color: %9; }
            #projectEditDialog QComboBox QAbstractItemView {
                background: %5;
                color: %3;
                border: 1px solid %6;
                selection-background-color: %7;
                selection-color: %3;
            }
            #spineColorSwatch { border: 1px solid %6; }
            #projectEditDialog QSlider::groove:horizontal {
                height: 4px;
                background: %6;
                border-radius: 2px;
            }
            #projectEditDialog QSlider::handle:horizontal {
                width: 14px;
                margin: -6px 0;
                background: %9;
                border-radius: 7px;
            }
        )")).arg(
            Theme::appBackground(),     // 1
            Theme::textPrimary(),       // 2
            Theme::textBright(),        // 3
            Theme::textMuted(),         // 4
            Theme::panelBackground(),   // 5
            Theme::panelBorder(),       // 6
            Theme::hoverOverlay(),      // 7
            Theme::subtleBorder(),      // 8 (não usado, mantém indexação)
            Theme::accentDefault()      // 9
        ));
    }

    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_authorEdit = nullptr;
    QLineEdit* m_genresEdit = nullptr;
    QPlainTextEdit* m_synopsisEdit = nullptr;
    QLabel* m_coverPreview = nullptr;
    QString m_coverDataUrl;
    bool m_coverCreateRequested = false;

    // --- Aba "Lombada" ---
    QPushButton* m_spineColorBtn = nullptr;
    QColor       m_spineColor;               // inválida = usar cor-padrão por índice
    QComboBox*   m_baseCombo = nullptr;       // "none" | "cover"
    QLabel*      m_bgPosLabel = nullptr;
    QSlider*     m_bgPosSlider = nullptr;
    QLabel*      m_bgPosValueLabel = nullptr;
    QComboBox*   m_finishCombo = nullptr;
    QComboBox*   m_fontCombo = nullptr;
    QPushButton* m_fontColorBtn = nullptr;
    QColor       m_fontColor;                // inválida = usar creme padrão
    QComboBox*   m_orientCombo = nullptr;
    QComboBox*   m_textPosCombo = nullptr;
    QSpinBox*    m_fontSizeSpin = nullptr;
    QComboBox*   m_widthCombo = nullptr;
    QSpinBox*    m_widthSpin = nullptr;
};

// Confirmação de exclusão com trava de tempo: o botão "Excluir" só habilita
// após um countdown de 5s, dando margem pra desistir de uma ação destrutiva.
class DeleteConfirmDialog : public QDialog {
public:
    DeleteConfirmDialog(const QString& projectName, const QString& projectPath, QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setObjectName(QStringLiteral("deleteConfirmDialog"));
        setWindowTitle(QCoreApplication::translate("DeleteConfirmDialog", "Excluir projeto"));
        setModal(true);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(24, 22, 24, 18);
        root->setSpacing(16);

        // Mostra o CAMINHO real, não só o nome — um nome como "Teste" não diz
        // nada sobre qual pasta vai ser movida pra lixeira; o caminho sim.
        auto* msg = new QLabel(
            QCoreApplication::translate("DeleteConfirmDialog",
                "Tem certeza que deseja excluir \"%1\"?\n\n"
                "Pasta: %2\n\n"
                "O projeto vai para a Lixeira (acessível pelo ícone de lixeira "
                "aqui embaixo), de onde pode ser restaurado depois.")
                .arg(projectName, QDir::toNativeSeparators(projectPath)),
            this);
        msg->setWordWrap(true);
        root->addWidget(msg);

        auto* buttons = new QDialogButtonBox(this);
        auto* cancelBtn = buttons->addButton(QCoreApplication::translate("DeleteConfirmDialog", "Cancelar"), QDialogButtonBox::RejectRole);
        m_deleteBtn = buttons->addButton(QCoreApplication::translate("DeleteConfirmDialog", "Excluir"), QDialogButtonBox::AcceptRole);
        m_deleteBtn->setObjectName(QStringLiteral("deleteConfirmBtn"));
        m_deleteBtn->setEnabled(false);
        cancelBtn->setCursor(Qt::PointingHandCursor);
        m_deleteBtn->setCursor(Qt::PointingHandCursor);
        QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(buttons);

        // Countdown 5 → 0.
        m_remaining = 5;
        m_deleteBtn->setText(QCoreApplication::translate("DeleteConfirmDialog", "Excluir (%1)").arg(m_remaining));
        auto* timer = new QTimer(this);
        timer->setInterval(1000);
        QObject::connect(timer, &QTimer::timeout, this, [this, timer]() {
            if (--m_remaining <= 0) {
                timer->stop();
                m_deleteBtn->setEnabled(true);
                m_deleteBtn->setText(QCoreApplication::translate("DeleteConfirmDialog", "Excluir"));
            } else {
                m_deleteBtn->setText(QCoreApplication::translate("DeleteConfirmDialog", "Excluir (%1)").arg(m_remaining));
            }
        });
        timer->start();
    }

private:
    QPushButton* m_deleteBtn = nullptr;
    int m_remaining = 5;
};

} // namespace

MainMenuDialog::MainMenuDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("mainMenuDialog"));
    setWindowTitle(tr("Qenna Writer"));
    setModal(false);
    // Promove a janela a top-level real: entra na taskbar, ganha botões de
    // minimizar/maximizar/fechar nativos, e — crucial — pode ser restaurada
    // pelo ícone da taskbar quando minimizada. QDialog por default não faz
    // nada disso, o que tornava o menu invisível ao minimizar e impossível
    // de detectar (inclusive pra encerrar antes de um rebuild).
    setWindowFlags(Qt::Window);
    setWindowIcon(QIcon(QStringLiteral(":/app/mira.png")));
    resize(kDialogW, kDialogH);
    setMinimumSize(1000, 700);
    {
        // Restaura o modo de exibição salvo ANTES de montar a UI — o bloco
        // de estado inicial do alternador dentro de buildUi() já lê
        // m_viewMode pra marcar o botão certo e mostrar a view certa.
        QSettings qs;
        // "estante", "lista" e "prateleira" eram as vistas antigas; quem
        // estava nelas cai no Continuar, que é o novo padrão.
        const QString saved = qs.value(QStringLiteral("library/viewMode")).toString();
        if (saved == QStringLiteral("vitrine"))          m_viewMode = ViewMode::Vitrine;
        else if (saved == QStringLiteral("cinema"))      m_viewMode = ViewMode::Cinema;
        else if (saved == QStringLiteral("seudia"))      m_viewMode = ViewMode::SeuDia;
        else if (saved == QStringLiteral("prateleiras")) m_viewMode = ViewMode::Estante;
        else if (saved == QStringLiteral("pilha"))       m_viewMode = ViewMode::Pilha;
        else                                              m_viewMode = ViewMode::Continuar;
    }
    buildUi();
    applyDialogStyle();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &MainMenuDialog::applyDialogStyle);
    // As vistas pintam com as cores do tema e embutem algumas em rich text:
    // refazer é mais simples (e barato) do que caçar cada uma.
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, [this]() { if (isVisible()) refreshRecents(); });

    // Rotação de quotes (compat Mira 1): 6s pra quotes curtos, 9s pra longos.
    // Timer single-shot reprogramado por rotateQuote() conforme o tamanho do
    // próximo quote.
    m_quoteTimer = new QTimer(this);
    m_quoteTimer->setSingleShot(true);
    connect(m_quoteTimer, &QTimer::timeout, this, &MainMenuDialog::rotateQuote);
}

void MainMenuDialog::buildUi()
{
    // Layout editorial em duas colunas: barra lateral (logo + quote + ações) e
    // área principal (cabeçalho + grid). Sem margens externas — cada coluna
    // tem o seu padding interno e a divisória nasce do fundo da sidebar.
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("menuSidebar"));
    sidebar->setFixedWidth(kSidebarW);
    auto* sideCol = new QVBoxLayout(sidebar);
    sideCol->setContentsMargins(28, 26, 28, 22);
    sideCol->setSpacing(14);
    buildSidebar(sideCol);
    root->addWidget(sidebar);

    auto* mainArea = new QWidget(this);
    mainArea->setObjectName(QStringLiteral("menuMainArea"));
    auto* mainCol = new QVBoxLayout(mainArea);
    mainCol->setContentsMargins(34, 26, 30, 22);
    mainCol->setSpacing(16);
    buildMainArea(mainCol);
    root->addWidget(mainArea, 1);
}

void MainMenuDialog::buildSidebar(QVBoxLayout* col)
{
    // --- Logo no topo (bom tamanho), centralizado ---
    m_logoLabel = new QLabel(this);
    m_logoLabel->setObjectName(QStringLiteral("menuLogo"));
    m_logoLabel->setFixedSize(kLogoSize, kLogoSize);
    m_logoLabel->setAlignment(Qt::AlignCenter);

    loadLogoVariants();
    if (!m_logoPaths.isEmpty()) {
        // Começa num ponto aleatório da lista: abrir o menu várias vezes no
        // mesmo dia não deve mostrar sempre o mesmo mundo primeiro.
        m_logoIndex = QRandomGenerator::global()->bounded(m_logoPaths.size());
        m_logoLabel->setPixmap(QPixmap::fromImage(logoVariant(m_logoIndex)));

        m_logoTimer = new QTimer(this);
        m_logoTimer->setSingleShot(true);
        connect(m_logoTimer, &QTimer::timeout, this, &MainMenuDialog::rotateLogo);
    } else {
        // Pasta ausente ou vazia: cai no logo fixo de sempre.
        QPixmap logoPm(QStringLiteral(":/app/logo.png"));
        if (!logoPm.isNull()) {
            m_logoLabel->setPixmap(logoPm.scaled(kLogoSize, kLogoSize,
                                                 Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation));
        } else {
            m_logoLabel->setText(QStringLiteral("Qenna Writer"));
        }
    }
    col->addSpacing(6);
    col->addWidget(m_logoLabel, 0, Qt::AlignHCenter);

    // --- Quote literário rotativo ---
    // Ancorado abaixo do logo, acima de um stretch. Pode crescer pra mostrar o
    // texto inteiro (sem corte): a folga vem do stretch, então nem o logo nem
    // os botões se mexem ao rotacionar.
    m_quoteLabel = new QLabel(this);
    m_quoteLabel->setObjectName(QStringLiteral("menuQuote"));
    m_quoteLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_quoteLabel->setWordWrap(true);
    m_quoteLabel->setFixedWidth(kLogoSize);
    m_quoteLabel->setMinimumHeight(80);
    m_quoteLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    // Efeito de opacidade pro crossfade entre quotes (sidebar não é scroll
    // area, então o efeito não sofre o problema de repaint cortado).
    m_quoteOpacity = new QGraphicsOpacityEffect(m_quoteLabel);
    m_quoteOpacity->setOpacity(1.0);
    m_quoteLabel->setGraphicsEffect(m_quoteOpacity);
    col->addWidget(m_quoteLabel, 0, Qt::AlignHCenter);

    col->addStretch(1);

    // --- Botões de ação com ícone, largura cheia ---
    m_newBtn = new QPushButton(tr("Novo projeto"), this);
    m_newBtn->setObjectName(QStringLiteral("menuPrimaryBtn"));
    m_newBtn->setCursor(Qt::PointingHandCursor);
    m_newBtn->setIconSize(QSize(18, 18));
    connect(m_newBtn, &QPushButton::clicked, this, [this]() { emit newProjectRequested(); });
    col->addWidget(m_newBtn);

    m_newIdeaBtn = new QPushButton(tr("Nova ideia"), this);
    m_newIdeaBtn->setObjectName(QStringLiteral("menuSecondaryBtn"));
    m_newIdeaBtn->setCursor(Qt::PointingHandCursor);
    m_newIdeaBtn->setIconSize(QSize(18, 18));
    m_newIdeaBtn->setToolTip(tr("Comece a escrever agora, sem criar um projeto ainda"));
    connect(m_newIdeaBtn, &QPushButton::clicked, this, [this]() { emit newIdeaRequested(); });
    col->addWidget(m_newIdeaBtn);

    m_loadBtn = new QPushButton(tr("Carregar pasta"), this);
    m_loadBtn->setObjectName(QStringLiteral("menuSecondaryBtn"));
    m_loadBtn->setCursor(Qt::PointingHandCursor);
    m_loadBtn->setIconSize(QSize(18, 18));
    connect(m_loadBtn, &QPushButton::clicked, this, [this]() { emit loadProjectRequested(); });
    col->addWidget(m_loadBtn);

    refreshActionIcons();

    col->addSpacing(8);

    // --- Seletor de idioma discreto no rodapé ---
    auto* langRow = new QHBoxLayout();
    langRow->setSpacing(6);
    langRow->setContentsMargins(0, 0, 0, 0);
    auto* langLbl = new QLabel(tr("Idioma:"), this);
    langLbl->setObjectName(QStringLiteral("menuLangLabel"));
    m_langCombo = new QComboBox(this);
    m_langCombo->setObjectName(QStringLiteral("menuLangCombo"));
    m_langCombo->setCursor(Qt::PointingHandCursor);
    m_langCombo->addItem(tr("Português (BR)"), QStringLiteral("pt_BR"));
    m_langCombo->addItem(tr("English"),        QStringLiteral("en"));
    m_langCombo->addItem(tr("Español"),        QStringLiteral("es"));
    m_langCombo->addItem(QStringLiteral("Italiano"), QStringLiteral("it"));
    m_langCombo->addItem(QStringLiteral("Français"), QStringLiteral("fr"));
    {
        QSettings qs;
        const QString cur = qs.value(QStringLiteral("app/language"), QStringLiteral("pt_BR")).toString();
        const int idx = m_langCombo->findData(cur);
        m_langCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        applyLanguage(m_langCombo->itemData(idx).toString());
    });
    langRow->addWidget(langLbl);
    langRow->addWidget(m_langCombo, 1);

    auto* checkUpdatesBtn = new QPushButton(QStringLiteral("↑"), this);
    checkUpdatesBtn->setObjectName(QStringLiteral("menuInfoBtn"));
    checkUpdatesBtn->setCursor(Qt::PointingHandCursor);
    checkUpdatesBtn->setFixedSize(24, 24);
    checkUpdatesBtn->setToolTip(tr("Verificar atualizações"));
    connect(checkUpdatesBtn, &QPushButton::clicked, this, [this]() {
        emit checkUpdatesRequested();
    });
    langRow->addWidget(checkUpdatesBtn);

    auto* infoBtn = new QPushButton(QStringLiteral("ⓘ"), this);
    infoBtn->setObjectName(QStringLiteral("menuInfoBtn"));
    infoBtn->setCursor(Qt::PointingHandCursor);
    infoBtn->setFixedSize(24, 24);
    infoBtn->setToolTip(tr("Sobre o Qenna Writer"));
    connect(infoBtn, &QPushButton::clicked, this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });
    langRow->addWidget(infoBtn);

    m_trashBtn = new QPushButton(this);
    m_trashBtn->setObjectName(QStringLiteral("menuInfoBtn"));
    m_trashBtn->setCursor(Qt::PointingHandCursor);
    m_trashBtn->setFixedSize(24, 24);
    m_trashBtn->setIconSize(QSize(14, 14));
    m_trashBtn->setToolTip(tr("Lixeira — projetos excluídos"));
    connect(m_trashBtn, &QPushButton::clicked, this, [this]() {
        TrashDialog dlg(this);
        dlg.exec();
        refreshRecents();
    });
    langRow->addWidget(m_trashBtn);

    col->addLayout(langRow);
}

void MainMenuDialog::buildMainArea(QVBoxLayout* col)
{
    // --- Cabeçalho: título + contagem à esquerda, vistas à direita ---
    m_header = new QWidget(this);
    auto* header = new QHBoxLayout(m_header);
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(10);
    auto* titleCol = new QVBoxLayout();
    titleCol->setSpacing(2);
    m_headingLabel = new QLabel(tr("Biblioteca"), m_header);
    m_headingLabel->setObjectName(QStringLiteral("menuHeading"));
    m_countLabel = new QLabel(m_header);
    m_countLabel->setObjectName(QStringLiteral("menuCount"));
    titleCol->addWidget(m_headingLabel);
    titleCol->addWidget(m_countLabel);
    header->addLayout(titleCol);
    header->addStretch(1);

    // Mesma ordem do enum ViewMode: o índice do botão é o modo.
    const QStringList names = { tr("Continuar"), tr("Vitrine"), tr("Cinema"),
                                tr("Seu dia"), tr("Estante"), tr("Pilha") };
    auto* toggleWrap = new QHBoxLayout();
    toggleWrap->setSpacing(6);
    for (int i = 0; i < names.size(); ++i) {
        auto* b = new QPushButton(names.at(i), m_header);
        b->setObjectName(QStringLiteral("menuViewToggle"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setChecked(i == int(m_viewMode));
        const ViewMode mode = ViewMode(i);
        connect(b, &QPushButton::clicked, this, [this, mode]() { setViewMode(mode); });
        toggleWrap->addWidget(b);
        m_viewBtns.append(b);
    }
    header->addLayout(toggleWrap);
    col->addWidget(m_header);

    // --- Área da vista ativa: cada vista é montada do zero em
    // populateActiveView; só a Pilha é um widget fixo (ela guarda estado de
    // animação e só esconde).
    m_viewHost = new QWidget(this);
    m_viewHost->setObjectName(QStringLiteral("menuViewHost"));
    auto* hostCol = new QVBoxLayout(m_viewHost);
    hostCol->setContentsMargins(0, 0, 0, 0);
    hostCol->setSpacing(0);

    m_stackView = new StackView(m_viewHost);
    connect(m_stackView, &StackView::openRequested, this, &MainMenuDialog::openRecentRequested);
    connect(m_stackView, &StackView::autoOpenChanged, this, &MainMenuDialog::autoOpenChanged);
    connect(m_stackView, &StackView::editRequested, this, &MainMenuDialog::editProject);
    connect(m_stackView, &StackView::coverCreateRequested, this, &MainMenuDialog::launchMiraCover);
    connect(m_stackView, &StackView::removeRequested, this, &MainMenuDialog::removeRecentRequested);
    connect(m_stackView, &StackView::deleteRequested, this, &MainMenuDialog::confirmDeleteProject);
    m_stackView->hide();
    hostCol->addWidget(m_stackView, 1);

    col->addWidget(m_viewHost, 1);
}

void MainMenuDialog::setRecentProjects(const QStringList& paths)
{
    m_recentPaths = paths;
    refreshRecents();
}

void MainMenuDialog::setAutoOpenPath(const QString& path)
{
    m_autoOpenPath = QDir::cleanPath(path);
    refreshRecents();
}

void MainMenuDialog::setViewMode(ViewMode mode)
{
    const bool changed = (mode != m_viewMode);
    m_viewMode = mode;
    for (int i = 0; i < m_viewBtns.size(); ++i) m_viewBtns[i]->setChecked(i == int(mode));
    if (changed && m_viewHost && m_viewHost->isVisible()) PanelMotion::swapOut(m_viewHost);
    populateActiveView();

    static const char* kKeys[] = { "continuar", "vitrine", "cinema", "seudia", "prateleiras", "pilha" };
    QSettings().setValue(QStringLiteral("library/viewMode"), QLatin1String(kKeys[int(mode)]));
}

void MainMenuDialog::refreshRecents()
{
    populateActiveView();
}

namespace {

// Mesma chave do MainWindow (resumeGroupFor): QSettings "resume/<md5 do root>".
QString resumeGroupForRoot(const QString& root)
{
    const QByteArray hash = QCryptographicHash::hash(
        QDir::cleanPath(root).toUtf8(), QCryptographicHash::Md5).toHex();
    return QStringLiteral("resume/") + QString::fromLatin1(hash);
}

// "cap. 7 · O que a aranha pediu" a partir do id do capítulo: o número conta
// só os capítulos (não prólogo/interlúdio) do mesmo manuscrito, em ordem.
QString chapterLabel(const QJsonArray& chapters, const QString& chapterId)
{
    QJsonObject target;
    for (const auto& v : chapters) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("id")).toString() == chapterId) { target = o; break; }
    }
    if (target.isEmpty()) return QString();
    const QString title = target.value(QStringLiteral("title")).toString().trimmed();
    const QString type = target.value(QStringLiteral("type")).toString(QStringLiteral("chapter"));
    int number = 0;
    if (type == QStringLiteral("chapter")) {
        const QString ms = target.value(QStringLiteral("manuscriptId")).toString();
        QList<QPair<int, QString>> order;
        for (const auto& v : chapters) {
            const QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("manuscriptId")).toString() != ms) continue;
            if (o.value(QStringLiteral("type")).toString(QStringLiteral("chapter")) != QStringLiteral("chapter")) continue;
            order.append({ o.value(QStringLiteral("order")).toInt(0), o.value(QStringLiteral("id")).toString() });
        }
        std::stable_sort(order.begin(), order.end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });
        for (int i = 0; i < order.size(); ++i)
            if (order.at(i).second == chapterId) { number = i + 1; break; }
    }
    if (title.isEmpty())
        return number > 0 ? MainMenuDialog::tr("Capítulo %1").arg(number) : QString();
    static const QRegularExpression numbered(
        QStringLiteral("^(\\d|cap|chap|capítulo|capitolo|chapitre|chapter)"),
        QRegularExpression::CaseInsensitiveOption);
    if (number <= 0 || numbered.match(title).hasMatch()) return title;
    return MainMenuDialog::tr("cap. %1 · %2").arg(number).arg(title);
}

LibraryEntry makeEntry(const QString& path, const RecentInfo& info, bool autoOpen)
{
    LibraryEntry e;
    e.path = path;
    e.name = info.name.isEmpty() ? QFileInfo(path).fileName() : info.name;
    e.author = info.author;
    e.genres = info.genres;
    e.synopsis = info.synopsis;
    e.totalWords = info.totalWords;
    e.manuscriptCount = info.manuscriptCount;
    e.chapterCount = info.chapterCount;
    e.autoOpen = autoOpen;
    QPixmap cover = decodeCoverDataUrl(info.coverDataUrl);
    e.hasOwnCover = !cover.isNull();
    if (cover.isNull()) cover = renderDefaultCover(e.name, e.author, kCardCoverW, kCardCoverH);
    else if (cover.height() > 720) cover = cover.scaledToHeight(720, Qt::SmoothTransformation);
    e.cover = cover;

    for (const auto& v : info.chapters) {
        const QString st = v.toObject().value(QStringLiteral("status")).toString();
        if (st.isEmpty()) continue;
        ++e.statusChapters;
        if (st == QStringLiteral("final")) ++e.finalChapters;
    }

    QDateTime when;
    const QVariantList trail = QSettings().value(resumeGroupForRoot(path) + QStringLiteral("/trail")).toList();
    if (!trail.isEmpty()) {
        const QVariantMap m = trail.first().toMap();
        e.resumeWhere = chapterLabel(info.chapters, m.value(QStringLiteral("ch")).toString());
        if (!e.resumeWhere.isEmpty()) e.resumeSentence = m.value(QStringLiteral("sentence")).toString();
        when = m.value(QStringLiteral("when")).toDateTime();
    }
    const QDateTime saved = QFileInfo(ProjectStorage::indexPath(path)).lastModified();
    e.lastTouched = (when.isValid() && (!saved.isValid() || when > saved)) ? when : saved;
    return e;
}

} // namespace

LibraryHooks MainMenuDialog::makeHooks()
{
    // Tudo sai num QTimer 0: quem clicou (uma capa, um card) termina o próprio
    // evento antes de o menu se refazer por baixo dele — remover dos recentes
    // ou abrir um projeto pode trocar a vista inteira.
    auto later = [this](std::function<void()> fn) { QTimer::singleShot(0, this, std::move(fn)); };
    LibraryHooks h;
    h.open = [this, later](const QString& p) { later([this, p]() { emit openRecentRequested(p); }); };
    h.resume = [this, later](const QString& p) { later([this, p]() { emit resumeRequested(p); }); };
    h.details = [this, later](const QString& p) { later([this, p]() { editProject(p); }); };
    h.menu = [this](const QString& p, const QPoint& g) { showProjectMenu(p, g); };
    h.newProject = [this, later]() { later([this]() { emit newProjectRequested(); }); };
    h.newIdea = [this, later]() { later([this]() { emit newIdeaRequested(); }); };
    h.loadProject = [this, later]() { later([this]() { emit loadProjectRequested(); }); };
    h.language = [this](const QString& lang) { applyLanguage(lang); };
    return h;
}

void MainMenuDialog::showProjectMenu(const QString& path, const QPoint& globalPos)
{
    QMenu menu(this);
    QAction* aOpen   = menu.addAction(tr("Abrir"));
    QAction* aResume = menu.addAction(tr("Continuar de onde parei"));
    menu.addSeparator();
    QAction* aAuto   = menu.addAction(QCoreApplication::translate("BookCard", "Abrir automaticamente"));
    aAuto->setCheckable(true);
    aAuto->setChecked(!m_autoOpenPath.isEmpty() && QDir::cleanPath(path) == m_autoOpenPath);
    QAction* aEdit   = menu.addAction(QCoreApplication::translate("BookCard", "Editar projeto"));
    QAction* aCover  = menu.addAction(QCoreApplication::translate("BookCard", "Criar capa"));
    menu.addSeparator();
    QAction* aRemove = menu.addAction(QCoreApplication::translate("BookCard", "Remover dos recentes"));
    QAction* aDelete = menu.addAction(QCoreApplication::translate("BookCard", "Excluir projeto"));
    PanelMotion::animateMenu(&menu);
    QAction* chosen = menu.exec(globalPos);
    if (!chosen) return;
    // Executa depois de o evento de quem abriu o menu terminar (ver makeHooks).
    QTimer::singleShot(0, this, [=, this]() {
        if (chosen == aOpen) emit openRecentRequested(path);
        else if (chosen == aResume) emit resumeRequested(path);
        else if (chosen == aAuto) {
            const bool on = !(!m_autoOpenPath.isEmpty() && QDir::cleanPath(path) == m_autoOpenPath);
            m_autoOpenPath = on ? QDir::cleanPath(path) : QString();
            emit autoOpenChanged(path, on);
        }
        else if (chosen == aEdit) editProject(path);
        else if (chosen == aCover) launchMiraCover(path);
        else if (chosen == aRemove) emit removeRecentRequested(path);
        else if (chosen == aDelete) confirmDeleteProject(path);
    });
}

void MainMenuDialog::applyLanguage(const QString& lang)
{
    QSettings qs;
    if (qs.value(QStringLiteral("app/language"), QStringLiteral("pt_BR")).toString() == lang) return;
    qs.setValue(QStringLiteral("app/language"), lang);
    qs.sync();
    QProcess::startDetached(QCoreApplication::applicationFilePath(),
                            QCoreApplication::arguments());
    QCoreApplication::quit();
}

void MainMenuDialog::populateActiveView()
{
    if (!m_viewHost) return;
    if (m_currentView) {
        m_currentView->hide();
        m_currentView->deleteLater();
        m_currentView = nullptr;
    }

    // Lê o índice uma vez por projeto; recentes que sumiram do disco ficam de fora.
    QStringList paths;
    QList<RecentInfo> infos;
    QVector<LibraryEntry> entries;
    for (const QString& path : m_recentPaths) {
        if (path.isEmpty() || !QDir(path).exists()) continue;
        const RecentInfo info = readRecentInfo(path);
        if (!info.valid) continue;
        const bool autoOpen = !m_autoOpenPath.isEmpty() && QDir::cleanPath(path) == m_autoOpenPath;
        paths.append(path);
        infos.append(info);
        entries.append(makeEntry(path, info, autoOpen));
    }
    const int n = entries.size();
    if (m_countLabel) {
        m_countLabel->setText(n == 1 ? tr("1 projeto") : tr("%1 projetos").arg(n));
    }
    if (m_header) m_header->setVisible(n > 0);

    auto* hostCol = static_cast<QVBoxLayout*>(m_viewHost->layout());
    const LibraryHooks hooks = makeHooks();

    if (n == 0) {
        m_stackView->hide();
        m_currentView = LibraryViews::welcomeView(hooks, m_viewHost);
        hostCol->addWidget(m_currentView, 1);
        return;
    }

    // Meta do dia: a unificada (entre projetos), se estiver ligada; senão a
    // do projeto em foco. Sem histórico nenhum, o usuário não usa meta — as
    // vistas então não falam dela.
    WritingDay day;
    {
        QSettings qs;
        WordCounterSettings wc;
        QString scope;
        if (qs.value(QStringLiteral("wordCounter/unifiedGoal"), false).toBool()) {
            wc = WordCounterSettings::fromJson(QJsonDocument::fromJson(
                qs.value(QStringLiteral("wordCounter/unifiedData")).toString().toUtf8()).object());
        } else {
            wc = WordCounterSettings::fromJson(infos.first().wordCounter);
            scope = entries.first().name;
        }
        day = WritingDay::fromSettings(wc, scope);
        day.valid = !wc.progress.isEmpty();
    }

    if (m_viewMode == ViewMode::Pilha) {
        QVector<StackEntry> stackEntries;
        for (int i = 0; i < n; ++i) {
            const RecentInfo& info = infos.at(i);
            const LibraryEntry& le = entries.at(i);
            QPixmap cover = decodeCoverDataUrl(info.coverDataUrl);
            if (cover.isNull())
                cover = renderDefaultCover(le.name, info.author, kStackHeroCoverW, kStackHeroCoverH);
            StackEntry e;
            e.path = le.path;
            e.name = le.name;
            e.author = info.author;
            e.genres = info.genres;
            e.synopsis = info.synopsis;
            e.totalWords = info.totalWords;
            e.manuscriptCount = info.manuscriptCount;
            e.chapterCount = info.chapterCount;
            e.documentCount = info.documentCount;
            e.heroCover = renderVitrineCover(cover, kStackHeroCoverW, kStackHeroCoverH);
            e.sideCover = renderVitrineCover(cover, kCardCoverW, kCardCoverH);
            e.fullCover = cover; // resolução original, antes do encolhimento pro herói
            if (info.manuscripts.size() > 1) {
                e.manuscriptHeroCovers.reserve(info.manuscripts.size());
                for (const auto& mi : info.manuscripts) {
                    QPixmap c = decodeCoverDataUrl(mi.coverDataUrl);
                    if (c.isNull()) c = cover; // fallback pro cover efetivo do projeto
                    e.manuscriptHeroCovers.append(renderVitrineCover(c, kStackHeroCoverW, kStackHeroCoverH));
                }
            }
            e.autoOpen = le.autoOpen;
            stackEntries.append(std::move(e));
        }
        m_stackView->setEntries(stackEntries);
        m_stackView->show();
        return;
    }

    m_stackView->hide();
    switch (m_viewMode) {
    case ViewMode::Vitrine:
        m_currentView = LibraryViews::vitrineView(entries, hooks, m_viewHost);
        break;
    case ViewMode::Cinema:
        m_currentView = LibraryViews::cinemaView(entries, hooks, m_viewHost);
        break;
    case ViewMode::SeuDia: {
        // Lembretes de hoje (e os atrasados) de todos os projetos da lista.
        QVector<LibraryReminder> reminders;
        const QDate today = QDate::currentDate();
        for (int i = 0; i < n; ++i) {
            RemindersStore store;
            store.setProjectRoot(paths.at(i));
            if (!store.load()) continue;
            for (const Reminder& r : store.active()) {
                if (r.dueAt <= 0 || QDateTime::fromMSecsSinceEpoch(r.dueAt).date() > today) continue;
                reminders.append({ r.text, n > 1 ? entries.at(i).name : QString(), r.dueAt });
            }
        }
        std::sort(reminders.begin(), reminders.end(),
                  [](const LibraryReminder& a, const LibraryReminder& b) { return a.dueAt < b.dueAt; });
        if (reminders.size() > 6) reminders.resize(6);
        m_currentView = LibraryViews::dayView(entries, day, reminders, hooks, m_viewHost);
        break;
    }
    case ViewMode::Estante:
        m_currentView = LibraryViews::shelfView(entries, hooks, m_viewHost);
        break;
    default:
        m_currentView = LibraryViews::continueView(entries, day, hooks, m_viewHost);
        break;
    }
    hostCol->addWidget(m_currentView, 1);
}

void MainMenuDialog::refreshActionIcons()
{
    if (m_newBtn) {
        // Ícone claro fixo sobre o preenchimento de acento (independe do tema).
        const QColor onAccent(245, 247, 246);
        m_newBtn->setIcon(IconUtils::loadToolbarIcon(
            QStringLiteral(":/icons/newproject.svg"), onAccent, onAccent, onAccent,
            QSize(18, 18)));
    }
    if (m_loadBtn) {
        const QColor c(Theme::textPrimary());
        m_loadBtn->setIcon(IconUtils::loadToolbarIcon(
            QStringLiteral(":/icons/loadproject.svg"), c, c, c, QSize(18, 18)));
    }
    if (m_newIdeaBtn) {
        const QColor c(Theme::textPrimary());
        m_newIdeaBtn->setIcon(IconUtils::loadToolbarIcon(
            QStringLiteral(":/icons/doc-plus.svg"), c, c, c, QSize(18, 18)));
    }
    if (m_trashBtn) {
        const QColor c(Theme::textPrimary());
        m_trashBtn->setIcon(IconUtils::loadToolbarIcon(
            QStringLiteral(":/icons/trash.svg"), c, c, c, QSize(14, 14)));
    }
}

void MainMenuDialog::editProject(const QString& path)
{
    bool ok = false;
    QJsonObject idx = ProjectStorage::readIndex(path, &ok);
    if (!ok) {
        QMessageBox::warning(this, tr("Erro"),
            tr("Não foi possível ler o projeto para edição."));
        return;
    }

    QString name = idx.value(QStringLiteral("projectName")).toString();
    if (name.isEmpty()) name = idx.value(QStringLiteral("name")).toString();
    QJsonObject data = idx.value(QStringLiteral("data")).toObject();
    QJsonObject pd = data.value(QStringLiteral("projectDetails")).toObject();

    const RecentInfo info = readRecentInfo(path);

    ProjectEditDialog dlg(this);
    dlg.setValues(name.isEmpty() ? QFileInfo(path).fileName() : name,
                  info.author, info.genres, info.synopsis, info.coverDataUrl);
    dlg.setSpineValues(info);
    const int dlgResult = dlg.exec();
    if (dlg.coverCreateRequested()) {
        launchMiraCover(path);
        return;
    }
    if (dlgResult != QDialog::Accepted) return;

    const QString newName = dlg.name().isEmpty() ? name : dlg.name();
    // Grava nas duas chaves de nome (compat Mira 1 + leitura do card).
    idx.insert(QStringLiteral("projectName"), newName);
    idx.insert(QStringLiteral("name"), newName);

    auto setOrRemove = [](QJsonObject& o, const QString& key, const QString& value) {
        if (value.isEmpty()) o.remove(key);
        else o.insert(key, value);
    };
    auto setOrRemoveInt = [](QJsonObject& o, const QString& key, int value, int sentinel) {
        if (value == sentinel) o.remove(key);
        else o.insert(key, value);
    };
    setOrRemove(pd, QStringLiteral("author"), dlg.author());
    setOrRemove(pd, QStringLiteral("genres"), dlg.genres());
    setOrRemove(pd, QStringLiteral("synopsis"), dlg.synopsis());
    const QString newCover = dlg.coverDataUrl();
    setOrRemove(pd, QStringLiteral("cover"), newCover);
    setOrRemove(pd, QStringLiteral("coverFull"), newCover);

    // Lombada (Prateleira 3D)
    setOrRemove(pd, QStringLiteral("spineColor"), dlg.spineColor());
    setOrRemove(pd, QStringLiteral("spineImageTexture"), dlg.spineImageTexture());
    setOrRemoveInt(pd, QStringLiteral("spineBgPosX"),
                   dlg.spineImageTexture() == QStringLiteral("cover") ? dlg.spineBgPosX() : 0, 0);
    setOrRemove(pd, QStringLiteral("spineTexture"), dlg.spineTexture());
    setOrRemove(pd, QStringLiteral("spineFontFamily"), dlg.spineFontFamily());
    setOrRemove(pd, QStringLiteral("spineFontColor"), dlg.spineFontColor());
    setOrRemoveInt(pd, QStringLiteral("spineFontSize"), dlg.spineFontSize(), 0);
    setOrRemove(pd, QStringLiteral("spineTextOrientation"), dlg.spineTextOrientation());
    setOrRemove(pd, QStringLiteral("spineTextPosition"), dlg.spineTextPosition());
    setOrRemove(pd, QStringLiteral("spineWidthMode"), dlg.spineWidthMode());
    setOrRemoveInt(pd, QStringLiteral("spineWidthManual"), dlg.spineWidthManual(), 0);

    if (pd.isEmpty()) data.remove(QStringLiteral("projectDetails"));
    else data.insert(QStringLiteral("projectDetails"), pd);
    idx.insert(QStringLiteral("data"), data);

    QString err;
    if (!ProjectStorage::writeIndex(path, idx, &err)) {
        QMessageBox::warning(this, tr("Erro"),
            tr("Não foi possível salvar as alterações:\n%1").arg(err));
        return;
    }
    refreshRecents();
}

void MainMenuDialog::launchMiraCover(const QString& projectPath)
{
    const QString exeDir = QCoreApplication::applicationDirPath();

    // ── 1) Produção: Cover Creator já instalado ────────────────────────────
    const QString installedExe = exeDir + QStringLiteral("/Cover Creator/Mira Cover.exe");
    if (QFile::exists(installedExe)) {
        startCoverProcess(installedExe, { projectPath }, QString(), projectPath);
        return;
    }

    // ── 2) Produção: setup disponível — perguntar se quer instalar ─────────
    const QString setupExe = exeDir + QStringLiteral("/Cover Creator/Mira Cover Setup.exe");
    if (QFile::exists(setupExe)) {
        const auto resp = QMessageBox::question(
            this, tr("Cover Creator"),
            tr("O Cover Creator ainda não está instalado.\n\n"
               "Deseja instalá-lo agora? (rápido, sem precisar de internet)"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (resp != QMessageBox::Yes) return;

        const QString exeToUse = QFile::copy(setupExe, installedExe)
                                 ? installedExe : setupExe;
        startCoverProcess(exeToUse, { projectPath }, QString(), projectPath);
        return;
    }

    // ── 3) Dev: diretório mira-cover irmão ────────────────────────────────
    const QString devCoverDir = QDir::cleanPath(
        exeDir + QStringLiteral("/../../mira-cover"));
    const QString devElectron = devCoverDir +
        QStringLiteral("/node_modules/electron/dist/electron.exe");

    if (QFile::exists(devElectron)) {
        if (!QFile::exists(devCoverDir + QStringLiteral("/dist/index.html"))) {
            QMessageBox::information(this, tr("Cover Creator"),
                tr("Execute 'npm run build' no diretório mira-cover para "
                   "habilitar o criador de capas no modo desenvolvimento."));
            return;
        }
        startCoverProcess(devElectron, { devCoverDir, projectPath },
                          devCoverDir, projectPath);
        return;
    }

    // ── 4) Nada local — pede pro MainWindow baixar a release mais recente
    // do GitHub (Cover Creator não vem mais bundlado no instalador).
    emit coverCreatorInstallRequested();
}

void MainMenuDialog::startCoverProcess(const QString& program,
                                       const QStringList& args,
                                       const QString& workingDir,
                                       const QString& projectPath)
{
    QProcess* proc = new QProcess(this);
    if (!workingDir.isEmpty()) proc->setWorkingDirectory(workingDir);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, projectPath, proc](int, QProcess::ExitStatus) {
        proc->deleteLater();
        updateCoverFromFile(projectPath);
    });

    proc->start(program, args);
}

void MainMenuDialog::updateCoverFromFile(const QString& projectPath)
{
    const QString coverFilePath = projectPath + QStringLiteral("/cover.jpg");
    if (!QFile::exists(coverFilePath)) return;

    QFile f(coverFilePath);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray bytes = f.readAll();
    f.close();
    if (bytes.isEmpty()) return;

    const QString dataUrl = QStringLiteral("data:image/jpeg;base64,") +
                            QString::fromLatin1(bytes.toBase64());

    // cover-bg.jpg: render sem texto que o Cover Creator grava ao lado da
    // capa final (mesmo fundo/foco/zoom, só sem o título) — usado como
    // textura opcional da lombada na Prateleira 3D. Nem toda capa antiga
    // tem esse arquivo (recurso novo); nesse caso a lombada cai pra "cover".
    QString coverBgDataUrl;
    const QString coverBgFilePath = projectPath + QStringLiteral("/cover-bg.jpg");
    if (QFile::exists(coverBgFilePath)) {
        QFile bg(coverBgFilePath);
        if (bg.open(QIODevice::ReadOnly)) {
            const QByteArray bgBytes = bg.readAll();
            bg.close();
            if (!bgBytes.isEmpty()) {
                coverBgDataUrl = QStringLiteral("data:image/jpeg;base64,") +
                                 QString::fromLatin1(bgBytes.toBase64());
            }
        }
    }

    bool ok = false;
    QJsonObject idx = ProjectStorage::readIndex(projectPath, &ok);
    if (!ok) return;

    QJsonObject projectData = idx.value(QStringLiteral("data")).toObject();
    QJsonObject details     = projectData.value(QStringLiteral("projectDetails")).toObject();
    details.insert(QStringLiteral("cover"),     dataUrl);
    details.insert(QStringLiteral("coverFull"), dataUrl);
    if (!coverBgDataUrl.isEmpty()) details.insert(QStringLiteral("coverBg"), coverBgDataUrl);
    projectData.insert(QStringLiteral("projectDetails"), details);
    idx.insert(QStringLiteral("data"), projectData);

    QString err;
    if (ProjectStorage::writeIndex(projectPath, idx, &err)) {
        refreshRecents();
        emit coverUpdated(projectPath);
    }
}

void MainMenuDialog::confirmDeleteProject(const QString& path)
{
    const RecentInfo info = readRecentInfo(path);
    const QString nm = info.name.isEmpty() ? QFileInfo(path).fileName() : info.name;
    DeleteConfirmDialog dlg(nm, path, this);
    if (dlg.exec() != QDialog::Accepted) return;
    // A exclusão de fato (apagar pasta + atualizar recentes) fica com o
    // MainWindow, que conhece o estado do projeto aberto e a lista.
    emit deleteProjectRequested(path);
}

void MainMenuDialog::rotateQuote()
{
    if (!m_quoteLabel) return;
    // Primeira exibição (sem texto ainda): entra direto com fade-in.
    if (m_quoteLabel->text().isEmpty() || !m_quoteOpacity) {
        showNextQuote();
        return;
    }
    // Caso normal: fade-out do quote atual; troca o texto e faz fade-in ao
    // terminar (em showNextQuote).
    auto* fadeOut = new QPropertyAnimation(m_quoteOpacity, "opacity", this);
    fadeOut->setDuration(360);
    fadeOut->setStartValue(m_quoteOpacity->opacity());
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::InOutQuad);
    connect(fadeOut, &QPropertyAnimation::finished, this, [this]() { showNextQuote(); });
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainMenuDialog::showNextQuote()
{
    if (!m_quoteLabel) return;
    const QString quote = Quotes::next();
    m_quoteLabel->setText(QStringLiteral("“%1”").arg(quote));
    if (m_quoteOpacity) {
        m_quoteOpacity->setOpacity(0.0);
        auto* fadeIn = new QPropertyAnimation(m_quoteOpacity, "opacity", this);
        fadeIn->setDuration(420);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);
        fadeIn->setEasingCurve(QEasingCurve::InOutQuad);
        fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
    }
    if (m_quoteTimer) {
        const int ms = quote.size() > 120 ? 9000 : 6000;
        m_quoteTimer->start(ms);
    }
}

void MainMenuDialog::loadLogoVariants()
{
    // Mesmo padrão de registerCustomFonts(): pasta ao lado do executável em
    // build/instalação, código-fonte como fallback no ambiente de dev.
    QString dir = QCoreApplication::applicationDirPath()
                  + QStringLiteral("/logo/main-menu-Q");
    if (!QDir(dir).exists()) {
        dir = QString::fromUtf8(DEV_ASSETS_DIR) + QStringLiteral("/logo/main-menu-Q");
    }
    if (!QDir(dir).exists()) return;

    const QStringList filters{QStringLiteral("*.png"), QStringLiteral("*.webp"),
                              QStringLiteral("*.jpg"), QStringLiteral("*.jpeg")};
    const QFileInfoList files = QDir(dir).entryInfoList(filters, QDir::Files, QDir::Name);
    for (const QFileInfo& fi : files) {
        m_logoPaths << fi.absoluteFilePath();
    }
}

QImage MainMenuDialog::logoVariant(int index)
{
    if (index < 0 || index >= m_logoPaths.size()) return {};
    auto it = m_logoFrames.constFind(index);
    if (it != m_logoFrames.constEnd()) return it.value();

    const QImage frame = normalizedLogo(m_logoPaths.at(index), kLogoSize);
    m_logoFrames.insert(index, frame);
    return frame;
}

void MainMenuDialog::rotateLogo()
{
    if (!m_logoLabel || m_logoPaths.size() < 2) return;
    if (m_logoAnim) return;   // transição já em curso

    const int nextIndex = (m_logoIndex + 1) % m_logoPaths.size();
    const QImage from = logoVariant(m_logoIndex);
    const QImage to = logoVariant(nextIndex);
    if (to.isNull()) {           // arquivo ilegível: pula pro seguinte
        m_logoIndex = nextIndex;
        if (m_logoTimer) m_logoTimer->start(kLogoHoldMs);
        return;
    }

    m_logoAnim = new QVariantAnimation(this);
    m_logoAnim->setDuration(kLogoFadeMs);
    m_logoAnim->setStartValue(0.0);
    m_logoAnim->setEndValue(1.0);
    m_logoAnim->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_logoAnim, &QVariantAnimation::valueChanged, this,
            [this, from, to](const QVariant& v) {
                m_logoLabel->setPixmap(crossfadedLogo(from, to, v.toReal()));
            });
    connect(m_logoAnim, &QVariantAnimation::finished, this, [this, nextIndex, to]() {
        // Assenta na arte pura, sem passar pela interpolação.
        m_logoIndex = nextIndex;
        m_logoLabel->setPixmap(QPixmap::fromImage(to));
        m_logoAnim = nullptr;   // DeleteWhenStopped se destrói sozinha
        // Adianta a decodificação da próxima enquanto a tela está parada —
        // assim o primeiro quadro do crossfade seguinte não engasga.
        logoVariant((m_logoIndex + 1) % m_logoPaths.size());
        if (m_logoTimer) m_logoTimer->start(kLogoHoldMs);
    });
    m_logoAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainMenuDialog::showEvent(QShowEvent* event)
{
    rotateQuote();
    refreshRecents();
    if (m_logoTimer && !m_logoTimer->isActive() && !m_logoAnim) {
        m_logoTimer->start(kLogoHoldMs);
    }
    QDialog::showEvent(event);
}

void MainMenuDialog::hideEvent(QHideEvent* event)
{
    // Menu escondido não precisa animar: para o relógio e corta um crossfade
    // em andamento, senão o app segue compondo pixmap a 60fps sem ninguém
    // olhando.
    if (m_logoTimer) m_logoTimer->stop();
    if (m_logoAnim) {
        m_logoAnim->stop();   // DeleteWhenStopped destrói; finished não dispara
        m_logoAnim = nullptr;
        m_logoLabel->setPixmap(QPixmap::fromImage(logoVariant(m_logoIndex)));
    }
    QDialog::hideEvent(event);
}

void MainMenuDialog::applyDialogStyle()
{
    const QString css = Theme::qss(QStringLiteral(R"(
        #mainMenuDialog { background: %1; }
        #menuSidebar { background: %5; border-right: 1px solid %6; }
        #menuMainArea { background: %1; }

        #menuLogo { padding: 2px; }
        #menuQuote {
            color: %2;
            font-size: 15px;
            font-style: italic;
        }

        QPushButton#menuPrimaryBtn {
            background: %9;
            color: #f5f7f6;
            border: 1px solid %9;
            border-radius: @radius-control;
            padding: 10px 14px;
            font-size: 13px;
            font-weight: 600;
            min-height: 22px;
        }
        QPushButton#menuPrimaryBtn:hover { border: 1px solid rgba(255, 255, 255, 0.55); }
        QPushButton#menuPrimaryBtn:pressed { background: %7; }

        QPushButton#menuSecondaryBtn {
            background: transparent;
            color: %2;
            border: 1px solid %6;
            border-radius: @radius-control;
            padding: 10px 14px;
            font-size: 13px;
            min-height: 22px;
        }
        QPushButton#menuSecondaryBtn:hover { background: %7; color: %3; border-color: %9; }

        #menuLangLabel { color: %4; font-size: 11px; }
        QPushButton#menuInfoBtn {
            background: transparent;
            color: %4;
            border: 1px solid %6;
            border-radius: @radius-control;
            font-size: 13px;
            padding: 0;
        }
        QPushButton#menuInfoBtn:hover {
            color: %3;
            border-color: %9;
        }
        QComboBox#menuLangCombo {
            background: %1;
            color: %2;
            border: 1px solid %6;
            border-radius: @radius-control;
            padding: 3px 8px;
            font-size: 11px;
        }
        QComboBox#menuLangCombo:hover { border-color: %9; }
        QComboBox#menuLangCombo::drop-down { border: none; width: 18px; }
        QComboBox#menuLangCombo QAbstractItemView {
            background: %5;
            color: %2;
            border: 1px solid %6;
            selection-background-color: %7;
            selection-color: %3;
        }

        #menuHeading {
            color: %3;
            font-size: 26px;
            font-weight: 700;
            letter-spacing: 0.3px;
        }
        #menuCount { color: %4; font-size: 12px; }

        QPushButton#menuViewToggle {
            background: %5;
            color: %4;
            border: 1px solid %6;
            border-radius: @radius-control;
            padding: 5px 16px;
            font-size: 12px;
            min-height: 22px;
        }
        QPushButton#menuViewToggle:hover { color: %3; border-color: %9; }
        QPushButton#menuViewToggle:checked {
            background: %7;
            color: %3;
            border-color: %9;
            font-weight: 600;
        }

        #libKick { color: %4; }
        #libTitle { color: %3; }
        #libText { color: %2; }
        #libMuted { color: %4; }
        #libQuote { color: %2; }
        QFrame#libHero, QFrame#libDayPanel {
            background: %5;
            border: 1px solid %8;
            border-radius: @radius-panel;
        }
        QFrame#libCard, QFrame#libAct {
            background: %5;
            border: 1px solid %6;
            border-radius: @radius-panel;
        }
        QFrame#libCard:hover, QFrame#libAct:hover { border-color: %9; background: %7; }
        QFrame#libTile { background: transparent; border: none; }
        QPushButton#libGo {
            background: %9;
            color: #ffffff;
            border: 1px solid %9;
            border-radius: @radius-control;
            padding: 8px 20px;
            min-height: 18px;
        }
        QPushButton#libGo:hover { border-color: rgba(255, 255, 255, 0.6); }
        QPushButton#libGo:pressed { background: %7; }
        QPushButton#libGhost {
            background: transparent;
            color: %2;
            border: 1px solid %6;
            border-radius: @radius-control;
            padding: 8px 16px;
            min-height: 18px;
        }
        QPushButton#libGhost:hover { color: %3; border-color: %9; background: %7; }
        #libCineKick { color: rgba(236, 230, 245, 0.78); }
        #libCineKickSoft { color: rgba(226, 220, 236, 0.72); }
        #libCineTitle { color: #ffffff; }
        #libCineText { color: rgba(240, 235, 226, 0.86); }
        QPushButton#libCineGhost {
            background: rgba(255, 255, 255, 0.06);
            color: #f2f0ea;
            border: 1px solid rgba(255, 255, 255, 0.32);
            border-radius: @radius-control;
            padding: 8px 16px;
            min-height: 18px;
        }
        QPushButton#libCineGhost:hover { background: rgba(255, 255, 255, 0.14); }
        QLineEdit#libSearch {
            background: %5;
            color: %2;
            border: 1px solid %6;
            border-radius: @radius-control;
            padding: 5px 8px;
            font-size: 12px;
        }
        QLineEdit#libSearch:focus { border-color: %9; }
        QComboBox#libSort {
            background: %5;
            color: %2;
            border: 1px solid %6;
            border-radius: @radius-control;
            padding: 5px 10px;
            font-size: 12px;
        }
        QComboBox#libSort:hover { border-color: %9; }
        QComboBox#libSort::drop-down { border: none; width: 18px; }
        QComboBox#libSort QAbstractItemView {
            background: %5;
            color: %2;
            border: 1px solid %6;
            selection-background-color: %7;
            selection-color: %3;
        }
        #libScroll { background: transparent; border: none; }
        #libBody { background: transparent; }
        #libScroll QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 0;
        }
        #libScroll QScrollBar::handle:vertical {
            background: %6;
            border-radius: 5px;
            min-height: 32px;
        }
        #libScroll QScrollBar::handle:vertical:hover { background: %9; }
        #libScroll QScrollBar::add-line:vertical,
        #libScroll QScrollBar::sub-line:vertical { height: 0; }
        #libScroll QScrollBar::add-page:vertical,
        #libScroll QScrollBar::sub-page:vertical { background: transparent; }

        #menuStackView { background: transparent; border: none; }
        #stackHeroStats {
            color: %4;
            font-size: 11px;
            letter-spacing: 0.05em;
        }
        #stackHeroTitle { color: %3; font-size: 22px; font-weight: 700; }
        #stackHeroAuthor { color: %4; font-size: 14px; font-style: italic; }
        #stackHeroBreadcrumb { color: %4; font-size: 12px; }
        #stackSynopsisScroll { background: transparent; border: none; }
        #stackSynopsisText { color: %2; font-size: 13px; }
        #stackSynopsisScroll QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 0;
        }
        #stackSynopsisScroll QScrollBar::handle:vertical {
            background: %6;
            border-radius: 4px;
            min-height: 28px;
        }
        #stackSynopsisScroll QScrollBar::handle:vertical:hover { background: %9; }
        #stackSynopsisScroll QScrollBar::add-line:vertical,
        #stackSynopsisScroll QScrollBar::sub-line:vertical { height: 0; }
        #stackSynopsisScroll QScrollBar::add-page:vertical,
        #stackSynopsisScroll QScrollBar::sub-page:vertical { background: transparent; }
    )")).arg(
        Theme::appBackground(),    // 1
        Theme::textPrimary(),      // 2
        Theme::textBright(),       // 3
        Theme::textMuted(),        // 4
        Theme::panelBackground(),  // 5
        Theme::panelBorder(),      // 6
        Theme::hoverOverlay(),     // 7
        // O %8 tem que aparecer na folha: o arg() com vários argumentos
        // numera pelos marcadores presentes, e um buraco empurrava o
        // destaque (%9) pro 8º argumento — o "Novo projeto" saía cinza.
        Theme::subtleBorder(),     // 8
        Theme::accentDefault()     // 9
    );
    setStyleSheet(css);
    refreshActionIcons();
}
