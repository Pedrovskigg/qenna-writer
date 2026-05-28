#include "MainMenuDialog.h"

#include "ProjectStorage.h"
#include "Quotes.h"
#include "Theme.h"

#include <QBuffer>
#include <QByteArray>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QCheckBox>
#include <QIcon>
#include <QSettings>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

namespace {

constexpr int kCardCoverW = 140;
constexpr int kCardCoverH = 210;
constexpr int kDialogW = 820;
constexpr int kDialogH = 640;
constexpr int kLogoSize = 315;

struct RecentInfo {
    bool valid = false;
    QString name;
    QString author;
    QString genres;
    QString coverDataUrl;
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
    info.name = root.value(QStringLiteral("name")).toString();
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const QJsonObject details = data.value(QStringLiteral("projectDetails")).toObject();
    info.author = details.value(QStringLiteral("author")).toString();
    info.genres = details.value(QStringLiteral("genres")).toString();
    // Compat Mira 1: ProjectModel grava em "coverFull"/"cover" (não em
    // "coverDataUrl"). Prefere coverFull (full res), cai pra cover.
    info.coverDataUrl = details.value(QStringLiteral("coverFull")).toString();
    if (info.coverDataUrl.isEmpty()) {
        info.coverDataUrl = details.value(QStringLiteral("cover")).toString();
    }
    if (info.name.isEmpty()) {
        info.name = QFileInfo(rootPath).fileName();
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

// Card clicável da lista de recentes. Sem Q_OBJECT — dispara callbacks pra
// click (abrir) e pra toggle do checkbox "abrir automaticamente". Hover é
// QSS puro via :hover no objeto.
class RecentCard : public QFrame {
public:
    using OpenCallback = std::function<void()>;
    using AutoOpenCallback = std::function<void(bool)>;

    RecentCard(const QString& path, const RecentInfo& info, bool autoOpen,
               OpenCallback openCb, AutoOpenCallback autoOpenCb,
               QWidget* parent = nullptr)
        : QFrame(parent), m_openCallback(std::move(openCb))
    {
        setObjectName(QStringLiteral("recentCard"));
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(kCardCoverH + 18);

        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(9, 9, 14, 9);
        row->setSpacing(14);

        auto* coverLbl = new QLabel(this);
        coverLbl->setFixedSize(kCardCoverW, kCardCoverH);
        coverLbl->setAlignment(Qt::AlignCenter);
        QPixmap pm = decodeCoverDataUrl(info.coverDataUrl);
        if (pm.isNull()) {
            pm = renderDefaultCover(info.name, info.author, kCardCoverW, kCardCoverH);
        } else {
            pm = pm.scaled(kCardCoverW, kCardCoverH,
                           Qt::KeepAspectRatioByExpanding,
                           Qt::SmoothTransformation);
        }
        coverLbl->setPixmap(pm);
        row->addWidget(coverLbl);

        auto* col = new QVBoxLayout;
        col->setSpacing(4);
        col->setContentsMargins(0, 4, 0, 4);
        col->setAlignment(Qt::AlignTop);

        auto* nameLbl = new QLabel(info.name.isEmpty()
                                       ? QFileInfo(path).fileName()
                                       : info.name,
                                   this);
        nameLbl->setObjectName(QStringLiteral("recentName"));
        nameLbl->setWordWrap(true);
        col->addWidget(nameLbl);

        if (!info.author.isEmpty()) {
            auto* authorLbl = new QLabel(info.author, this);
            authorLbl->setObjectName(QStringLiteral("recentAuthor"));
            col->addWidget(authorLbl);
        }

        if (!info.genres.isEmpty()) {
            auto* genresLbl = new QLabel(info.genres, this);
            genresLbl->setObjectName(QStringLiteral("recentGenres"));
            genresLbl->setWordWrap(true);
            col->addWidget(genresLbl);
        }

        col->addStretch();

        auto* autoOpenChk = new QCheckBox(tr("Abrir automaticamente"), this);
        autoOpenChk->setObjectName(QStringLiteral("recentAutoOpen"));
        autoOpenChk->setCursor(Qt::PointingHandCursor);
        autoOpenChk->setChecked(autoOpen);
        QObject::connect(autoOpenChk, &QCheckBox::toggled, this,
            [cb = std::move(autoOpenCb)](bool checked) { if (cb) cb(checked); });
        col->addWidget(autoOpenChk);

        row->addLayout(col, /*stretch=*/1);
    }

protected:
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos()) && m_openCallback) {
            m_openCallback();
        }
        QFrame::mouseReleaseEvent(event);
    }

private:
    OpenCallback m_openCallback;
};

} // namespace

MainMenuDialog::MainMenuDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("mainMenuDialog"));
    setWindowTitle(tr("Mira Writing"));
    setModal(false);
    // Promove a janela a top-level real: entra na taskbar, ganha botões de
    // minimizar/maximizar/fechar nativos, e — crucial — pode ser restaurada
    // pelo ícone da taskbar quando minimizada. QDialog por default não faz
    // nada disso, o que tornava o menu invisível ao minimizar e impossível
    // de detectar (inclusive pra encerrar antes de um rebuild).
    setWindowFlags(Qt::Window);
    setWindowIcon(QIcon(QStringLiteral(":/app/mira.png")));
    resize(kDialogW, kDialogH);
    setMinimumSize(680, 520);
    buildUi();
    applyDialogStyle();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &MainMenuDialog::applyDialogStyle);

    // Rotação de quotes (compat Mira 1): 6s pra quotes curtos, 9s pra longos.
    // Timer single-shot reprogramado por rotateQuote() conforme o tamanho do
    // próximo quote.
    m_quoteTimer = new QTimer(this);
    m_quoteTimer->setSingleShot(true);
    connect(m_quoteTimer, &QTimer::timeout, this, &MainMenuDialog::rotateQuote);
}

void MainMenuDialog::buildUi()
{
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- Coluna esquerda: projetos ----
    auto* leftFrame = new QFrame(this);
    leftFrame->setObjectName(QStringLiteral("menuLeftCol"));
    auto* leftCol = new QVBoxLayout(leftFrame);
    leftCol->setContentsMargins(24, 24, 24, 24);
    leftCol->setSpacing(14);

    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(8);
    auto* heading = new QLabel(tr("Projetos"), leftFrame);
    heading->setObjectName(QStringLiteral("menuHeading"));
    headerRow->addWidget(heading);
    headerRow->addStretch();

    m_newBtn = new QPushButton(tr("Novo"), leftFrame);
    m_newBtn->setObjectName(QStringLiteral("menuActionBtn"));
    m_newBtn->setCursor(Qt::PointingHandCursor);
    connect(m_newBtn, &QPushButton::clicked, this, [this]() {
        emit newProjectRequested();
    });
    headerRow->addWidget(m_newBtn);

    m_loadBtn = new QPushButton(tr("Carregar"), leftFrame);
    m_loadBtn->setObjectName(QStringLiteral("menuActionBtn"));
    m_loadBtn->setCursor(Qt::PointingHandCursor);
    connect(m_loadBtn, &QPushButton::clicked, this, [this]() {
        emit loadProjectRequested();
    });
    headerRow->addWidget(m_loadBtn);

    leftCol->addLayout(headerRow);

    auto* sub = new QLabel(tr("Recentes"), leftFrame);
    sub->setObjectName(QStringLiteral("menuSubheading"));
    leftCol->addWidget(sub);

    m_recentsScroll = new QScrollArea(leftFrame);
    m_recentsScroll->setObjectName(QStringLiteral("menuRecentsScroll"));
    m_recentsScroll->setWidgetResizable(true);
    m_recentsScroll->setFrameShape(QFrame::NoFrame);
    m_recentsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_recentsHolder = new QWidget(m_recentsScroll);
    m_recentsHolder->setObjectName(QStringLiteral("menuRecentsHolder"));
    m_recentsLayout = new QVBoxLayout(m_recentsHolder);
    m_recentsLayout->setContentsMargins(0, 0, 0, 0);
    m_recentsLayout->setSpacing(8);

    m_emptyLabel = new QLabel(
        tr("Você ainda não tem projetos.\n"
           "Clique em \"Novo\" pra começar um, ou em \"Carregar\" pra abrir uma pasta existente."),
        m_recentsHolder);
    m_emptyLabel->setObjectName(QStringLiteral("menuEmpty"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_recentsLayout->addWidget(m_emptyLabel);
    m_recentsLayout->addStretch();

    m_recentsScroll->setWidget(m_recentsHolder);
    leftCol->addWidget(m_recentsScroll, /*stretch=*/1);

    root->addWidget(leftFrame, /*stretch=*/1);

    // ---- Coluna direita: logo + quote ----
    auto* rightFrame = new QFrame(this);
    rightFrame->setObjectName(QStringLiteral("menuRightCol"));
    auto* rightCol = new QVBoxLayout(rightFrame);
    rightCol->setContentsMargins(28, 28, 28, 28);
    rightCol->setSpacing(0);

    m_quoteLabel = new QLabel(rightFrame);
    m_quoteLabel->setObjectName(QStringLiteral("menuQuote"));
    m_quoteLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_quoteLabel->setWordWrap(true);
    rightCol->addWidget(m_quoteLabel, 0, Qt::AlignTop);

    rightCol->addSpacing(12);

    m_logoLabel = new QLabel(rightFrame);
    m_logoLabel->setObjectName(QStringLiteral("menuLogo"));
    m_logoLabel->setAlignment(Qt::AlignCenter);
    QPixmap logoPm(QStringLiteral(":/app/mira.png"));
    if (!logoPm.isNull()) {
        m_logoLabel->setPixmap(logoPm.scaled(kLogoSize, kLogoSize,
                                             Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));
    } else {
        m_logoLabel->setText(QStringLiteral("Mira Writing"));
    }
    rightCol->addWidget(m_logoLabel, 0, Qt::AlignCenter);

    rightCol->addStretch();

    // Seletor de idioma — rodapé direito.
    auto* langRow = new QHBoxLayout();
    langRow->setSpacing(6);
    langRow->setContentsMargins(0, 0, 0, 0);
    auto* langLbl = new QLabel(tr("Idioma:"), rightFrame);
    langLbl->setObjectName(QStringLiteral("menuLangLabel"));
    m_langCombo = new QComboBox(rightFrame);
    m_langCombo->setObjectName(QStringLiteral("menuLangCombo"));
    m_langCombo->setCursor(Qt::PointingHandCursor);
    m_langCombo->addItem(tr("Português (BR)"), QStringLiteral("pt_BR"));
    m_langCombo->addItem(tr("English"),        QStringLiteral("en"));
    {
        QSettings qs;
        const QString cur = qs.value(QStringLiteral("app/language"), QStringLiteral("pt_BR")).toString();
        m_langCombo->setCurrentIndex(cur == QStringLiteral("en") ? 1 : 0);
    }
    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        const QString lang = m_langCombo->itemData(idx).toString();
        QSettings qs;
        if (qs.value(QStringLiteral("app/language"), QStringLiteral("pt_BR")).toString() == lang) return;
        qs.setValue(QStringLiteral("app/language"), lang);
        qs.sync();
        QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                QCoreApplication::arguments());
        QCoreApplication::quit();
    });
    langRow->addStretch();
    langRow->addWidget(langLbl);
    langRow->addWidget(m_langCombo);
    rightCol->addLayout(langRow);

    root->addWidget(rightFrame, /*stretch=*/1);
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

void MainMenuDialog::refreshRecents()
{
    if (!m_recentsLayout) return;

    // Limpa o layout sem destruir o m_emptyLabel — só remove do layout.
    while (m_recentsLayout->count() > 0) {
        QLayoutItem* it = m_recentsLayout->takeAt(0);
        if (!it) break;
        if (QWidget* w = it->widget()) {
            if (w == m_emptyLabel) {
                delete it;
                continue;
            }
            w->deleteLater();
        }
        delete it;
    }

    int added = 0;
    for (const QString& path : m_recentPaths) {
        if (path.isEmpty() || !QDir(path).exists()) continue;
        const RecentInfo info = readRecentInfo(path);
        if (!info.valid) continue;
        const QString capturedPath = path;
        const bool isAutoOpen = !m_autoOpenPath.isEmpty()
            && QDir::cleanPath(path) == m_autoOpenPath;
        auto* card = new RecentCard(path, info, isAutoOpen,
            [this, capturedPath]() { emit openRecentRequested(capturedPath); },
            [this, capturedPath](bool enabled) {
                emit autoOpenChanged(capturedPath, enabled);
            },
            m_recentsHolder);
        m_recentsLayout->addWidget(card);
        ++added;
    }

    if (added == 0) {
        m_emptyLabel->show();
        m_recentsLayout->addWidget(m_emptyLabel);
    } else {
        m_emptyLabel->hide();
    }
    m_recentsLayout->addStretch();
}

void MainMenuDialog::rotateQuote()
{
    if (!m_quoteLabel) return;
    const QString quote = Quotes::next();
    m_quoteLabel->setText(QStringLiteral("“%1”").arg(quote));
    if (m_quoteTimer) {
        const int ms = quote.size() > 120 ? 9000 : 6000;
        m_quoteTimer->start(ms);
    }
}

void MainMenuDialog::showEvent(QShowEvent* event)
{
    rotateQuote();
    refreshRecents();
    QDialog::showEvent(event);
}

void MainMenuDialog::applyDialogStyle()
{
    const QString css = QStringLiteral(R"(
        #mainMenuDialog { background: %1; }
        #menuLeftCol { background: %1; border-right: 1px solid %6; }
        #menuRightCol { background: %5; }

        #menuHeading {
            color: %3;
            font-size: 20px;
            font-weight: 600;
            letter-spacing: 0.5px;
        }
        #menuSubheading {
            color: %4;
            font-size: 11px;
            font-weight: 600;
            letter-spacing: 1.5px;
            text-transform: uppercase;
            margin-top: 4px;
        }
        #menuEmpty {
            color: %4;
            font-size: 12px;
            padding: 40px 20px;
        }
        #menuRecentsScroll { background: transparent; border: none; }
        #menuRecentsHolder { background: transparent; }

        QPushButton#menuActionBtn {
            background: %5;
            color: %2;
            border: 1px solid %6;
            padding: 6px 16px;
            border-radius: 6px;
            font-size: 12px;
            min-height: 28px;
        }
        QPushButton#menuActionBtn:hover {
            background: %7;
            color: %3;
            border-color: %9;
        }

        QFrame#recentCard {
            background: %5;
            border: 1px solid %6;
            border-radius: 8px;
        }
        QFrame#recentCard:hover {
            background: %7;
            border-color: %9;
        }
        #recentName {
            color: %3;
            font-size: 15px;
            font-weight: 600;
        }
        #recentAuthor {
            color: %2;
            font-size: 12px;
            font-style: italic;
        }
        #recentGenres {
            color: %4;
            font-size: 11px;
        }
        #recentAutoOpen {
            color: %4;
            font-size: 11px;
            spacing: 6px;
        }
        #recentAutoOpen::indicator {
            width: 14px;
            height: 14px;
            border: 1px solid %6;
            border-radius: 3px;
            background: %1;
        }
        #recentAutoOpen::indicator:hover {
            border-color: %9;
        }
        #recentAutoOpen::indicator:checked {
            background: %9;
            border-color: %9;
            image: url(:/icons/check.svg);
        }

        #menuLogo { padding: 8px; }
        #menuQuote {
            color: %2;
            font-size: 16px;
            font-style: italic;
            line-height: 150%;
            padding: 4px 4px;
        }
        #menuLangLabel {
            color: %4;
            font-size: 11px;
        }
        QComboBox#menuLangCombo {
            background: %1;
            color: %2;
            border: 1px solid %6;
            border-radius: 5px;
            padding: 3px 8px;
            font-size: 11px;
            min-width: 120px;
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
    )").arg(
        Theme::appBackground(),    // 1
        Theme::textPrimary(),      // 2
        Theme::textBright(),       // 3
        Theme::textMuted(),        // 4
        Theme::panelBackground(),  // 5
        Theme::panelBorder(),      // 6
        Theme::hoverOverlay(),     // 7
        Theme::subtleBorder(),     // 8 (mantém indexação)
        Theme::accentDefault()     // 9
    );
    setStyleSheet(css);
}
