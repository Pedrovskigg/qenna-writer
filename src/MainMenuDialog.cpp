#include "MainMenuDialog.h"

#include "ProjectStorage.h"
#include "Quotes.h"
#include "Theme.h"

#include <QAction>
#include <QBuffer>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
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
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

namespace {

constexpr int kCardCoverW = 240;
constexpr int kCardCoverH = 360;
constexpr int kDialogW = 1320;
constexpr int kDialogH = 1000;
constexpr int kLogoSize = 360;

// Padding ao redor da capa pra acomodar sombra projetada + bloco de páginas.
constexpr int kVitPadL = 2;
constexpr int kVitPadT = 3;
constexpr int kVitPadR = 19;
constexpr int kVitPadB = 17;

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

// Conjunto de callbacks do card — agrupados pra não inflar a assinatura.
struct CardCallbacks {
    std::function<void()> open;            // clique esquerdo → abrir projeto
    std::function<void(bool)> autoOpen;    // toggle "abrir automaticamente"
    std::function<void(QWidget*, bool)> hover; // enter/leave → escurecer outros
    std::function<void()> edit;            // context menu → editar
    std::function<void()> removeRecent;    // context menu → remover dos recentes
    std::function<void()> del;             // context menu → excluir projeto
};

// Card de livro do grid de recentes. Sem Q_OBJECT — dispara callbacks. O
// conteúdo é capa-vitrine + título + toggle. O escurecimento de hover NÃO usa
// QGraphicsOpacityEffect (que corta o repaint dentro do QScrollArea): em vez
// disso, um "véu" translúcido (filho da capa) é mostrado/escondido.
class BookCard : public QFrame {
public:
    BookCard(const QString& path, const RecentInfo& info, bool autoOpen,
             CardCallbacks cbs, QWidget* parent = nullptr)
        : QFrame(parent), m_cbs(std::move(cbs))
    {
        setObjectName(QStringLiteral("bookCard"));
        setCursor(Qt::PointingHandCursor);

        auto* col = new QVBoxLayout(this);
        col->setContentsMargins(0, 0, 0, 0);
        col->setSpacing(7);
        col->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

        QPixmap cover = decodeCoverDataUrl(info.coverDataUrl);
        if (cover.isNull()) {
            cover = renderDefaultCover(info.name, info.author, kCardCoverW, kCardCoverH);
        }
        const QPixmap vit = renderVitrineCover(cover, kCardCoverW, kCardCoverH);

        auto* coverLbl = new QLabel(this);
        coverLbl->setFixedSize(vit.size());
        coverLbl->setPixmap(vit);
        coverLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        col->addWidget(coverLbl, 0, Qt::AlignHCenter);

        // Véu de escurecimento — cobre só a área da capa (sem o padding da
        // sombra). Começa escondido; setDimmed() controla a visibilidade.
        m_veil = new QWidget(coverLbl);
        m_veil->setObjectName(QStringLiteral("bookCardVeil"));
        m_veil->setAttribute(Qt::WA_StyledBackground, true);
        m_veil->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        m_veil->setGeometry(kVitPadL, kVitPadT, kCardCoverW, kCardCoverH);
        m_veil->hide();

        auto* nameLbl = new QLabel(info.name.isEmpty()
                                       ? QFileInfo(path).fileName()
                                       : info.name,
                                   this);
        nameLbl->setObjectName(QStringLiteral("bookCardName"));
        nameLbl->setAlignment(Qt::AlignHCenter);
        nameLbl->setWordWrap(true);
        nameLbl->setFixedWidth(vit.width());
        nameLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        col->addWidget(nameLbl, 0, Qt::AlignHCenter);

        auto* autoOpenChk = new QCheckBox(tr("Abrir automaticamente"), this);
        autoOpenChk->setObjectName(QStringLiteral("bookCardAutoOpen"));
        autoOpenChk->setCursor(Qt::PointingHandCursor);
        autoOpenChk->setChecked(autoOpen);
        QObject::connect(autoOpenChk, &QCheckBox::toggled, this,
            [cb = m_cbs.autoOpen](bool checked) { if (cb) cb(checked); });
        col->addWidget(autoOpenChk, 0, Qt::AlignHCenter);

        setFixedWidth(vit.width());
    }

    void setDimmed(bool dim) {
        if (m_veil) m_veil->setVisible(dim);
    }

protected:
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())
            && m_cbs.open) {
            m_cbs.open();
        }
        QFrame::mouseReleaseEvent(event);
    }
    void enterEvent(QEnterEvent* event) override {
        if (m_cbs.hover) m_cbs.hover(this, true);
        QFrame::enterEvent(event);
    }
    void leaveEvent(QEvent* event) override {
        if (m_cbs.hover) m_cbs.hover(this, false);
        QFrame::leaveEvent(event);
    }
    void contextMenuEvent(QContextMenuEvent* event) override {
        QMenu menu(this);
        menu.setObjectName(QStringLiteral("bookCardMenu"));
        QAction* aEdit   = menu.addAction(tr("Editar projeto"));
        QAction* aRemove = menu.addAction(tr("Remover dos recentes"));
        menu.addSeparator();
        QAction* aDelete = menu.addAction(tr("Excluir projeto"));
        QAction* chosen = menu.exec(event->globalPos());
        if (chosen == aEdit && m_cbs.edit)            m_cbs.edit();
        else if (chosen == aRemove && m_cbs.removeRecent) m_cbs.removeRecent();
        else if (chosen == aDelete && m_cbs.del)      m_cbs.del();
    }

private:
    CardCallbacks m_cbs;
    QWidget* m_veil = nullptr;
};

// Diálogo de edição dos metadados do projeto: nome, autor, gêneros, sinopse e
// capa. Não toca no ProjectModel — quem grava é o MainMenuDialog, direto no
// índice. Sem Q_OBJECT: usa apenas accept()/reject() do QDialog + lambdas.
class ProjectEditDialog : public QDialog {
public:
    explicit ProjectEditDialog(QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setObjectName(QStringLiteral("projectEditDialog"));
        setWindowTitle(tr("Editar projeto"));
        setModal(true);
        resize(520, 560);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(22, 22, 22, 18);
        root->setSpacing(14);

        auto* topRow = new QHBoxLayout();
        topRow->setSpacing(16);

        // Coluna da capa (preview + botões).
        auto* coverCol = new QVBoxLayout();
        coverCol->setSpacing(8);
        m_coverPreview = new QLabel(this);
        m_coverPreview->setObjectName(QStringLiteral("projectEditCover"));
        m_coverPreview->setFixedSize(150, 225);
        m_coverPreview->setAlignment(Qt::AlignCenter);
        m_coverPreview->setScaledContents(false);
        coverCol->addWidget(m_coverPreview, 0, Qt::AlignHCenter);

        auto* changeCover = new QPushButton(tr("Trocar capa"), this);
        changeCover->setCursor(Qt::PointingHandCursor);
        QObject::connect(changeCover, &QPushButton::clicked, this, [this]() {
            const QString file = QFileDialog::getOpenFileName(
                this, tr("Escolher capa"), QString(),
                tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
            if (file.isEmpty()) return;
            QImage img(file);
            if (img.isNull()) return;
            if (img.width() > 900)
                img = img.scaledToWidth(900, Qt::SmoothTransformation);
            QByteArray ba;
            QBuffer buf(&ba);
            buf.open(QIODevice::WriteOnly);
            img.save(&buf, "PNG");
            m_coverDataUrl = QStringLiteral("data:image/png;base64,")
                             + QString::fromLatin1(ba.toBase64());
            updateCoverPreview();
        });
        coverCol->addWidget(changeCover);

        auto* clearCover = new QPushButton(tr("Remover capa"), this);
        clearCover->setCursor(Qt::PointingHandCursor);
        QObject::connect(clearCover, &QPushButton::clicked, this, [this]() {
            m_coverDataUrl.clear();
            updateCoverPreview();
        });
        coverCol->addWidget(clearCover);
        coverCol->addStretch();
        topRow->addLayout(coverCol);

        // Coluna dos campos de texto.
        auto* form = new QFormLayout();
        form->setLabelAlignment(Qt::AlignLeft);
        form->setSpacing(8);
        m_nameEdit = new QLineEdit(this);
        m_authorEdit = new QLineEdit(this);
        m_genresEdit = new QLineEdit(this);
        form->addRow(tr("Nome"), m_nameEdit);
        form->addRow(tr("Autor"), m_authorEdit);
        form->addRow(tr("Gêneros"), m_genresEdit);
        topRow->addLayout(form, /*stretch=*/1);

        root->addLayout(topRow);

        auto* synLabel = new QLabel(tr("Sinopse"), this);
        root->addWidget(synLabel);
        m_synopsisEdit = new QPlainTextEdit(this);
        m_synopsisEdit->setMinimumHeight(120);
        root->addWidget(m_synopsisEdit, /*stretch=*/1);

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
        buttons->button(QDialogButtonBox::Save)->setText(tr("Salvar"));
        buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
        QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(buttons);
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

private:
    void updateCoverPreview() {
        QPixmap pm = decodeCoverDataUrl(m_coverDataUrl);
        if (pm.isNull()) {
            m_coverPreview->setText(tr("Sem capa"));
            return;
        }
        m_coverPreview->setPixmap(pm.scaled(m_coverPreview->size(),
                                            Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
    }

    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_authorEdit = nullptr;
    QLineEdit* m_genresEdit = nullptr;
    QPlainTextEdit* m_synopsisEdit = nullptr;
    QLabel* m_coverPreview = nullptr;
    QString m_coverDataUrl;
};

// Confirmação de exclusão com trava de tempo: o botão "Excluir" só habilita
// após um countdown de 5s, dando margem pra desistir de uma ação destrutiva.
class DeleteConfirmDialog : public QDialog {
public:
    DeleteConfirmDialog(const QString& projectName, QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setObjectName(QStringLiteral("deleteConfirmDialog"));
        setWindowTitle(tr("Excluir projeto"));
        setModal(true);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(24, 22, 24, 18);
        root->setSpacing(16);

        auto* msg = new QLabel(
            tr("Tem certeza que deseja excluir \"%1\"?\n\n"
               "A pasta do projeto será apagada do disco. "
               "Esta ação NÃO pode ser desfeita.").arg(projectName),
            this);
        msg->setWordWrap(true);
        root->addWidget(msg);

        auto* buttons = new QDialogButtonBox(this);
        auto* cancelBtn = buttons->addButton(tr("Cancelar"), QDialogButtonBox::RejectRole);
        m_deleteBtn = buttons->addButton(tr("Excluir"), QDialogButtonBox::AcceptRole);
        m_deleteBtn->setObjectName(QStringLiteral("deleteConfirmBtn"));
        m_deleteBtn->setEnabled(false);
        cancelBtn->setCursor(Qt::PointingHandCursor);
        m_deleteBtn->setCursor(Qt::PointingHandCursor);
        QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(buttons);

        // Countdown 5 → 0.
        m_remaining = 5;
        m_deleteBtn->setText(tr("Excluir (%1)").arg(m_remaining));
        auto* timer = new QTimer(this);
        timer->setInterval(1000);
        QObject::connect(timer, &QTimer::timeout, this, [this, timer]() {
            if (--m_remaining <= 0) {
                timer->stop();
                m_deleteBtn->setEnabled(true);
                m_deleteBtn->setText(tr("Excluir"));
            } else {
                m_deleteBtn->setText(tr("Excluir (%1)").arg(m_remaining));
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
    setMinimumSize(1000, 700);
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
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 16, 40, 26);
    root->setSpacing(10);

    // --- Topo: seletor de idioma à direita (discreto) ---
    auto* langRow = new QHBoxLayout();
    langRow->setSpacing(6);
    langRow->setContentsMargins(0, 0, 0, 0);
    langRow->addStretch();
    auto* langLbl = new QLabel(tr("Idioma:"), this);
    langLbl->setObjectName(QStringLiteral("menuLangLabel"));
    m_langCombo = new QComboBox(this);
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
    langRow->addWidget(langLbl);
    langRow->addWidget(m_langCombo);
    root->addLayout(langRow);

    // --- Logo + quote centralizados no topo ---
    m_logoLabel = new QLabel(this);
    m_logoLabel->setObjectName(QStringLiteral("menuLogo"));
    // Logo em alta resolução (1024px) pra não borrar no tamanho grande.
    QPixmap logoPm(QStringLiteral(":/app/logo.png"));
    if (!logoPm.isNull()) {
        m_logoLabel->setPixmap(logoPm.scaled(kLogoSize, kLogoSize,
                                             Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));
    } else {
        m_logoLabel->setText(QStringLiteral("Mira Writing"));
    }
    root->addWidget(m_logoLabel, 0, Qt::AlignHCenter);

    m_quoteLabel = new QLabel(this);
    m_quoteLabel->setObjectName(QStringLiteral("menuQuote"));
    m_quoteLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_quoteLabel->setWordWrap(true);
    // Largura fixa torna o word-wrap determinístico (a altura cresce em vez
    // de cortar o texto). Centralizado em relação ao logo.
    m_quoteLabel->setFixedWidth(880);
    m_quoteLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    root->addWidget(m_quoteLabel, 0, Qt::AlignHCenter);

    root->addSpacing(16);

    // --- Botões Novo/Carregar no canto esquerdo, acima do grid ---
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    btnRow->setContentsMargins(0, 0, 0, 0);
    m_newBtn = new QPushButton(tr("Novo"), this);
    m_newBtn->setObjectName(QStringLiteral("menuActionBtn"));
    m_newBtn->setCursor(Qt::PointingHandCursor);
    connect(m_newBtn, &QPushButton::clicked, this, [this]() { emit newProjectRequested(); });
    btnRow->addWidget(m_newBtn);
    m_loadBtn = new QPushButton(tr("Carregar"), this);
    m_loadBtn->setObjectName(QStringLiteral("menuActionBtn"));
    m_loadBtn->setCursor(Qt::PointingHandCursor);
    connect(m_loadBtn, &QPushButton::clicked, this, [this]() { emit loadProjectRequested(); });
    btnRow->addWidget(m_loadBtn);
    btnRow->addStretch();
    root->addLayout(btnRow);

    // --- Grid HORIZONTAL de recentes (scroll lateral) ---
    m_recentsScroll = new QScrollArea(this);
    m_recentsScroll->setObjectName(QStringLiteral("menuRecentsScroll"));
    m_recentsScroll->setWidgetResizable(true);
    m_recentsScroll->setFrameShape(QFrame::NoFrame);
    m_recentsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_recentsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // Garante altura pra exibir o card inteiro (capa + nome + toggle).
    m_recentsScroll->setMinimumHeight(kCardCoverH + kVitPadT + kVitPadB + 80);

    m_recentsHolder = new QWidget(m_recentsScroll);
    m_recentsHolder->setObjectName(QStringLiteral("menuRecentsHolder"));
    m_recentsRow = new QHBoxLayout(m_recentsHolder);
    m_recentsRow->setContentsMargins(0, 0, 0, 8);
    m_recentsRow->setSpacing(20);
    m_recentsRow->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_emptyLabel = new QLabel(
        tr("Você ainda não tem projetos.\n"
           "Clique em \"Novo\" pra começar um, ou em \"Carregar\" pra abrir uma pasta existente."),
        m_recentsHolder);
    m_emptyLabel->setObjectName(QStringLiteral("menuEmpty"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->hide();

    m_recentsScroll->setWidget(m_recentsHolder);
    root->addWidget(m_recentsScroll, /*stretch=*/1);
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

void MainMenuDialog::setHoveredCard(QWidget* hovered)
{
    // m_cards guarda apenas BookCard* (criados em refreshRecents); o
    // static_cast é seguro e evita Q_OBJECT/qobject_cast no tipo local.
    for (QWidget* c : m_cards) {
        if (!c) continue;
        static_cast<BookCard*>(c)->setDimmed(hovered != nullptr && c != hovered);
    }
}

void MainMenuDialog::refreshRecents()
{
    if (!m_recentsRow) return;

    // Limpa os cards anteriores (e o estado de hover). O m_emptyLabel é
    // preservado — só sai do layout, sem ser destruído.
    m_cards.clear();
    while (QLayoutItem* it = m_recentsRow->takeAt(0)) {
        if (QWidget* w = it->widget()) {
            if (w == m_emptyLabel) {
                w->hide();
                w->setParent(m_recentsHolder);
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
        CardCallbacks cbs;
        cbs.open = [this, capturedPath]() { emit openRecentRequested(capturedPath); };
        cbs.autoOpen = [this, capturedPath](bool enabled) {
            emit autoOpenChanged(capturedPath, enabled);
        };
        cbs.hover = [this](QWidget* c, bool entered) {
            setHoveredCard(entered ? c : nullptr);
        };
        cbs.edit = [this, capturedPath]() { editProject(capturedPath); };
        cbs.removeRecent = [this, capturedPath]() { emit removeRecentRequested(capturedPath); };
        cbs.del = [this, capturedPath]() { confirmDeleteProject(capturedPath); };
        auto* card = new BookCard(path, info, isAutoOpen, std::move(cbs), m_recentsHolder);
        m_recentsRow->addWidget(card, 0, Qt::AlignTop);
        m_cards.append(card);
        ++added;
    }

    if (added == 0) {
        m_emptyLabel->show();
        m_recentsRow->addWidget(m_emptyLabel, 1, Qt::AlignCenter);
    } else {
        // Empurra os cards pra a esquerda (sobra fica à direita).
        m_recentsRow->addStretch(1);
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
    QString cover = pd.value(QStringLiteral("coverFull")).toString();
    if (cover.isEmpty()) cover = pd.value(QStringLiteral("cover")).toString();

    ProjectEditDialog dlg(this);
    dlg.setValues(name.isEmpty() ? QFileInfo(path).fileName() : name,
                  pd.value(QStringLiteral("author")).toString(),
                  pd.value(QStringLiteral("genres")).toString(),
                  pd.value(QStringLiteral("synopsis")).toString(),
                  cover);
    if (dlg.exec() != QDialog::Accepted) return;

    const QString newName = dlg.name().isEmpty() ? name : dlg.name();
    // Grava nas duas chaves de nome (compat Mira 1 + leitura do card).
    idx.insert(QStringLiteral("projectName"), newName);
    idx.insert(QStringLiteral("name"), newName);

    auto setOrRemove = [](QJsonObject& o, const QString& key, const QString& value) {
        if (value.isEmpty()) o.remove(key);
        else o.insert(key, value);
    };
    setOrRemove(pd, QStringLiteral("author"), dlg.author());
    setOrRemove(pd, QStringLiteral("genres"), dlg.genres());
    setOrRemove(pd, QStringLiteral("synopsis"), dlg.synopsis());
    const QString newCover = dlg.coverDataUrl();
    setOrRemove(pd, QStringLiteral("cover"), newCover);
    setOrRemove(pd, QStringLiteral("coverFull"), newCover);

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

void MainMenuDialog::confirmDeleteProject(const QString& path)
{
    const RecentInfo info = readRecentInfo(path);
    const QString nm = info.name.isEmpty() ? QFileInfo(path).fileName() : info.name;
    DeleteConfirmDialog dlg(nm, this);
    if (dlg.exec() != QDialog::Accepted) return;
    // A exclusão de fato (apagar pasta + atualizar recentes) fica com o
    // MainWindow, que conhece o estado do projeto aberto e a lista.
    emit deleteProjectRequested(path);
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

        QFrame#bookCard { background: transparent; border: none; }
        #bookCardVeil {
            background-color: rgba(0, 0, 0, 0.62);
            border-radius: 4px;
        }
        #bookCardName {
            color: %3;
            font-size: 15px;
            font-weight: 600;
        }
        #bookCardAutoOpen {
            color: %4;
            font-size: 10px;
            spacing: 5px;
        }
        #bookCardAutoOpen::indicator {
            width: 13px;
            height: 13px;
            border: 1px solid %6;
            border-radius: 3px;
            background: %1;
        }
        #bookCardAutoOpen::indicator:hover { border-color: %9; }
        #bookCardAutoOpen::indicator:checked {
            background: %9;
            border-color: %9;
            image: url(:/icons/check.svg);
        }

        QFrame#menuReservedPanel {
            background: transparent;
            border: 1px dashed %6;
            border-radius: 8px;
        }
        #menuReservedLabel {
            color: %4;
            font-size: 12px;
            font-style: italic;
            letter-spacing: 0.5px;
        }

        #menuLogo { padding: 4px; }
        #menuQuote {
            color: %2;
            font-size: 18px;
            font-style: italic;
            line-height: 150%;
            padding: 2px 2px;
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
