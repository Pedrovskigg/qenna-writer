#include "MainMenuDialog.h"

#include "IconUtils.h"
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
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSettings>
#include <QShowEvent>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidgetItem>

#include <functional>

// Layout que dispõe os filhos em linha e quebra pra próxima quando estoura a
// largura — a "parede de capas". Implementação canônica do exemplo de Flow
// Layout do Qt (sem Q_OBJECT: não tem signals/slots). Vive em escopo global
// porque o header o referencia como membro de MainMenuDialog.
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent, int margin = 0, int hSpacing = -1, int vSpacing = -1)
        : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing)
    {
        setContentsMargins(margin, margin, margin, margin);
    }
    ~FlowLayout() override {
        QLayoutItem* item;
        while ((item = takeAt(0))) delete item;
    }

    void addItem(QLayoutItem* item) override { m_items.append(item); }
    int count() const override { return m_items.size(); }
    QLayoutItem* itemAt(int index) const override { return m_items.value(index); }
    QLayoutItem* takeAt(int index) override {
        return (index >= 0 && index < m_items.size()) ? m_items.takeAt(index) : nullptr;
    }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return doLayout(QRect(0, 0, width, 0), true); }
    void setGeometry(const QRect& rect) override {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override {
        QSize size;
        for (QLayoutItem* item : m_items) size = size.expandedTo(item->minimumSize());
        const QMargins m = contentsMargins();
        return size + QSize(m.left() + m.right(), m.top() + m.bottom());
    }

private:
    int doLayout(const QRect& rect, bool testOnly) const {
        int left, top, right, bottom;
        getContentsMargins(&left, &top, &right, &bottom);
        const QRect eff = rect.adjusted(left, top, -right, -bottom);
        int x = eff.x();
        int y = eff.y();
        int lineHeight = 0;
        const int spaceX = m_hSpace >= 0 ? m_hSpace : 16;
        const int spaceY = m_vSpace >= 0 ? m_vSpace : 16;
        for (QLayoutItem* item : m_items) {
            const QSize hint = item->sizeHint();
            int nextX = x + hint.width() + spaceX;
            if (nextX - spaceX > eff.right() + 1 && lineHeight > 0) {
                x = eff.x();
                y = y + lineHeight + spaceY;
                nextX = x + hint.width() + spaceX;
                lineHeight = 0;
            }
            if (!testOnly) item->setGeometry(QRect(QPoint(x, y), hint));
            x = nextX;
            lineHeight = qMax(lineHeight, hint.height());
        }
        return y + lineHeight - rect.y() + bottom;
    }
    QList<QLayoutItem*> m_items;
    int m_hSpace;
    int m_vSpace;
};

// QScrollArea que casa com layouts dependentes de largura (FlowLayout, ou um
// QVBoxLayout de linhas). Em vez de widgetResizable (que ignora
// heightForWidth e corta o conteúdo), redimensionamos o widget manualmente:
// largura = viewport, altura = heightForWidth na largura do viewport.
class FlowScrollArea : public QScrollArea {
public:
    explicit FlowScrollArea(QWidget* parent = nullptr) : QScrollArea(parent) {
        setWidgetResizable(false);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }
    void reflow() {
        QWidget* w = widget();
        if (!w) return;
        const int vw = viewport()->width();
        QLayout* l = w->layout();
        const int contentH = (l && l->hasHeightForWidth()) ? l->heightForWidth(vw)
                                                            : w->sizeHint().height();
        w->resize(vw, qMax(contentH, viewport()->height()));
    }
protected:
    void resizeEvent(QResizeEvent* e) override {
        QScrollArea::resizeEvent(e);
        reflow();
    }
};

namespace {

constexpr int kCardCoverW = 240;
constexpr int kCardCoverH = 360;
constexpr int kDialogW = 1320;
constexpr int kDialogH = 1000;
constexpr int kSidebarW = 410;   // largura da barra lateral
constexpr int kLogoSize = 330;   // caixa do logo — cabe na largura interna (410 - margens) com folga
constexpr int kEditCoverW = 260; // capa grande do diálogo Editar projeto
constexpr int kEditCoverH = 390;

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

// Versão escurecida da capa-vitrine usada no hover. SourceAtop pinta o preto
// apenas onde o pixmap já tem alpha — ou seja, sobre a capa, o bloco de
// páginas e a lombada, respeitando o formato exato do livro (o miolo escurece
// junto, sem vazar um retângulo pra fora da silhueta).
QPixmap renderVitrineCoverDimmed(const QPixmap& vit, qreal alpha)
{
    QPixmap pm = vit;
    QPainter p(&pm);
    p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    p.fillRect(pm.rect(), QColor(0, 0, 0, qBound(0, int(alpha * 255), 255)));
    p.end();
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
// conteúdo é capa-vitrine + título + toggle. Dois efeitos no hover:
//   - Escurecer os DEMAIS cards: troca do pixmap por uma versão escurecida
//     (SourceAtop, inclui miolo/lombada). Não usa QGraphicsOpacityEffect, que
//     cortaria o repaint dentro do QScrollArea.
//   - Aumentar ESTE card: a capa vive num "palco" de tamanho fixo (já com a
//     folga do zoom reservada) e cresce ~10% via animação de geometria. O
//     crescimento fica contido no palco, então o FlowLayout não reflui.
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
        m_vitNormal = renderVitrineCover(cover, kCardCoverW, kCardCoverH);
        m_vitDimmed = renderVitrineCoverDimmed(m_vitNormal, 0.62);

        const QSize baseSz = m_vitNormal.size();
        const QSize stageSz(int(baseSz.width()  * kHoverScale),
                            int(baseSz.height() * kHoverScale));
        m_normalRect = QRect(QPoint((stageSz.width()  - baseSz.width())  / 2,
                                    (stageSz.height() - baseSz.height()) / 2), baseSz);
        m_hoverRect  = QRect(QPoint(0, 0), stageSz);

        // Palco fixo: reserva o espaço do zoom pra o footprint do card não
        // mudar ao crescer (FlowLayout fica quieto).
        m_stage = new QWidget(this);
        m_stage->setFixedSize(stageSz);
        m_stage->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        m_coverLbl = new QLabel(m_stage);
        m_coverLbl->setScaledContents(true);
        m_coverLbl->setPixmap(m_vitNormal);
        m_coverLbl->setGeometry(m_normalRect);
        m_coverLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        col->addWidget(m_stage, 0, Qt::AlignHCenter);

        m_coverAnim = new QPropertyAnimation(m_coverLbl, "geometry", this);
        m_coverAnim->setDuration(170);
        m_coverAnim->setEasingCurve(QEasingCurve::OutCubic);

        auto* nameLbl = new QLabel(info.name.isEmpty()
                                       ? QFileInfo(path).fileName()
                                       : info.name,
                                   this);
        nameLbl->setObjectName(QStringLiteral("bookCardName"));
        nameLbl->setAlignment(Qt::AlignHCenter);
        nameLbl->setWordWrap(true);
        nameLbl->setFixedWidth(stageSz.width());
        nameLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        col->addWidget(nameLbl, 0, Qt::AlignHCenter);

        auto* autoOpenChk = new QCheckBox(tr("Abrir automaticamente"), this);
        autoOpenChk->setObjectName(QStringLiteral("bookCardAutoOpen"));
        autoOpenChk->setCursor(Qt::PointingHandCursor);
        autoOpenChk->setChecked(autoOpen);
        QObject::connect(autoOpenChk, &QCheckBox::toggled, this,
            [cb = m_cbs.autoOpen](bool checked) { if (cb) cb(checked); });
        col->addWidget(autoOpenChk, 0, Qt::AlignHCenter);

        setFixedWidth(stageSz.width());
    }

    void setDimmed(bool dim) {
        if (!m_coverLbl) return;
        m_coverLbl->setPixmap(dim ? m_vitDimmed : m_vitNormal);
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
        animateCover(m_hoverRect);
        if (m_cbs.hover) m_cbs.hover(this, true);
        QFrame::enterEvent(event);
    }
    void leaveEvent(QEvent* event) override {
        animateCover(m_normalRect);
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
    void animateCover(const QRect& target) {
        if (!m_coverAnim) return;
        m_coverAnim->stop();
        m_coverAnim->setStartValue(m_coverLbl->geometry());
        m_coverAnim->setEndValue(target);
        m_coverAnim->start();
    }

    static constexpr double kHoverScale = 1.10;

    CardCallbacks m_cbs;
    QWidget* m_stage = nullptr;
    QLabel* m_coverLbl = nullptr;
    QPropertyAnimation* m_coverAnim = nullptr;
    QPixmap m_vitNormal;
    QPixmap m_vitDimmed;
    QRect m_normalRect;
    QRect m_hoverRect;
};

// Miniatura simples (cantos arredondados, sem o 3D da vitrine) pra usar na
// visualização em Lista, onde a capa aparece pequena ao lado do texto.
QPixmap renderThumb(const QPixmap& coverIn, int w, int h)
{
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QPainterPath clip;
    clip.addRoundedRect(0, 0, w, h, 3, 3);
    p.setClipPath(clip);
    const QPixmap scaled = coverIn.scaled(w, h, Qt::KeepAspectRatioByExpanding,
                                          Qt::SmoothTransformation);
    p.drawPixmap(QPointF(-(scaled.width() - w) / 2.0, -(scaled.height() - h) / 2.0),
                 scaled);
    p.setClipping(false);
    p.setPen(QPen(QColor(255, 255, 255, 28), 1));
    p.drawPath(clip);
    return pm;
}

// Linha da visualização em Lista: capa miniatura + nome + metadados (autor /
// gêneros) + toggle "abrir automaticamente". Compartilha os callbacks e o
// menu de contexto do BookCard.
class BookRow : public QFrame {
public:
    BookRow(const QString& path, const RecentInfo& info, bool autoOpen,
            CardCallbacks cbs, QWidget* parent = nullptr)
        : QFrame(parent), m_cbs(std::move(cbs))
    {
        setObjectName(QStringLiteral("bookRow"));
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_StyledBackground, true);

        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(12, 9, 16, 9);
        row->setSpacing(14);

        QPixmap cover = decodeCoverDataUrl(info.coverDataUrl);
        if (cover.isNull())
            cover = renderDefaultCover(info.name, info.author, 120, 180);
        const QPixmap thumb = renderThumb(cover, 62, 92);
        auto* thumbLbl = new QLabel(this);
        thumbLbl->setFixedSize(thumb.size());
        thumbLbl->setPixmap(thumb);
        thumbLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        row->addWidget(thumbLbl, 0, Qt::AlignVCenter);

        auto* textCol = new QVBoxLayout();
        textCol->setSpacing(3);
        textCol->setContentsMargins(0, 0, 0, 0);
        auto* nameLbl = new QLabel(info.name.isEmpty() ? QFileInfo(path).fileName()
                                                       : info.name, this);
        nameLbl->setObjectName(QStringLiteral("bookRowName"));
        nameLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        QStringList meta;
        if (!info.author.isEmpty()) meta << info.author;
        if (!info.genres.isEmpty()) meta << info.genres;
        auto* metaLbl = new QLabel(meta.join(QStringLiteral("   ·   ")), this);
        metaLbl->setObjectName(QStringLiteral("bookRowMeta"));
        metaLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        metaLbl->setVisible(!meta.isEmpty());
        textCol->addStretch();
        textCol->addWidget(nameLbl);
        textCol->addWidget(metaLbl);
        textCol->addStretch();
        row->addLayout(textCol, 1);

        auto* chk = new QCheckBox(tr("Abrir automaticamente"), this);
        chk->setObjectName(QStringLiteral("bookCardAutoOpen"));
        chk->setCursor(Qt::PointingHandCursor);
        chk->setChecked(autoOpen);
        QObject::connect(chk, &QCheckBox::toggled, this,
            [cb = m_cbs.autoOpen](bool checked) { if (cb) cb(checked); });
        row->addWidget(chk, 0, Qt::AlignVCenter);

        setFixedHeight(92 + 18);
    }

protected:
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())
            && m_cbs.open) {
            m_cbs.open();
        }
        QFrame::mouseReleaseEvent(event);
    }
    void contextMenuEvent(QContextMenuEvent* event) override {
        QMenu menu(this);
        menu.setObjectName(QStringLiteral("bookCardMenu"));
        QAction* aEdit   = menu.addAction(tr("Editar projeto"));
        QAction* aRemove = menu.addAction(tr("Remover dos recentes"));
        menu.addSeparator();
        QAction* aDelete = menu.addAction(tr("Excluir projeto"));
        QAction* chosen = menu.exec(event->globalPos());
        if (chosen == aEdit && m_cbs.edit)                m_cbs.edit();
        else if (chosen == aRemove && m_cbs.removeRecent) m_cbs.removeRecent();
        else if (chosen == aDelete && m_cbs.del)          m_cbs.del();
    }

private:
    CardCallbacks m_cbs;
};

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
        setWindowTitle(tr("Editar projeto"));
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
        m_coverPreview->setText(tr("Sem capa"));
        m_coverPreview->setScaledContents(false);
        leftCol->addWidget(m_coverPreview);

        auto* pickBtn = new QPushButton(tr("Escolher capa…"), this);
        pickBtn->setObjectName(QStringLiteral("projectInfoBtn"));
        pickBtn->setCursor(Qt::PointingHandCursor);
        QObject::connect(pickBtn, &QPushButton::clicked, this, [this]() {
            const QString startDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
            const QString file = QFileDialog::getOpenFileName(
                this, tr("Escolher capa"), startDir,
                tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
            if (file.isEmpty()) return;
            const QString dataUrl = loadCoverAsDataUrl(file);
            if (dataUrl.isEmpty()) return;
            m_coverDataUrl = dataUrl;
            updateCoverPreview();
        });
        leftCol->addWidget(pickBtn);

        auto* clearBtn = new QPushButton(tr("Remover capa"), this);
        clearBtn->setObjectName(QStringLiteral("projectInfoBtn"));
        clearBtn->setCursor(Qt::PointingHandCursor);
        QObject::connect(clearBtn, &QPushButton::clicked, this, [this]() {
            if (m_coverDataUrl.isEmpty()) return;
            m_coverDataUrl.clear();
            updateCoverPreview();
        });
        leftCol->addWidget(clearBtn);
        leftCol->addStretch();
        root->addLayout(leftCol);

        // ---- Coluna direita: header + formulário + footer ----
        auto* rightCol = new QVBoxLayout();
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

        auto* actions = new QHBoxLayout();
        actions->setSpacing(8);
        actions->addStretch();
        auto* cancelBtn = new QPushButton(tr("Cancelar"), this);
        cancelBtn->setObjectName(QStringLiteral("projectInfoBtn"));
        cancelBtn->setCursor(Qt::PointingHandCursor);
        QObject::connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        auto* saveBtn = new QPushButton(tr("Salvar"), this);
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

private:
    void updateCoverPreview() {
        QPixmap pm = decodeCoverDataUrl(m_coverDataUrl);
        if (pm.isNull()) {
            m_coverPreview->clear();
            m_coverPreview->setText(tr("Sem capa"));
            return;
        }
        m_coverPreview->setPixmap(pm.scaled(kEditCoverW, kEditCoverH,
                                            Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
    }

    void applyDialogStyle() {
        setStyleSheet(QStringLiteral(R"(
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
                border-radius: 6px;
                font-size: 11px;
            }
            #projectEditDialog QLineEdit,
            #projectEditDialog QPlainTextEdit {
                background: %5;
                color: %3;
                border: 1px solid %6;
                border-radius: 6px;
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
    QPixmap logoPm(QStringLiteral(":/app/logo.png"));
    if (!logoPm.isNull()) {
        m_logoLabel->setPixmap(logoPm.scaled(kLogoSize, kLogoSize,
                                             Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));
    } else {
        m_logoLabel->setText(QStringLiteral("Mira Writing"));
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
    langRow->addWidget(m_langCombo, 1);
    col->addLayout(langRow);
}

void MainMenuDialog::buildMainArea(QVBoxLayout* col)
{
    // --- Cabeçalho: título + contagem à esquerda, alternador de visão à dir. ---
    auto* header = new QHBoxLayout();
    header->setSpacing(10);
    auto* titleCol = new QVBoxLayout();
    titleCol->setSpacing(2);
    m_headingLabel = new QLabel(tr("Biblioteca"), this);
    m_headingLabel->setObjectName(QStringLiteral("menuHeading"));
    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("menuCount"));
    titleCol->addWidget(m_headingLabel);
    titleCol->addWidget(m_countLabel);
    header->addLayout(titleCol);
    header->addStretch(1);

    m_estanteBtn = new QPushButton(tr("Estante"), this);
    m_estanteBtn->setObjectName(QStringLiteral("menuViewToggle"));
    m_estanteBtn->setCheckable(true);
    m_estanteBtn->setCursor(Qt::PointingHandCursor);
    m_listaBtn = new QPushButton(tr("Lista"), this);
    m_listaBtn->setObjectName(QStringLiteral("menuViewToggle"));
    m_listaBtn->setCheckable(true);
    m_listaBtn->setCursor(Qt::PointingHandCursor);
    connect(m_estanteBtn, &QPushButton::clicked, this, [this]() { setViewMode(ViewMode::Estante); });
    connect(m_listaBtn, &QPushButton::clicked, this, [this]() { setViewMode(ViewMode::Lista); });
    auto* toggleWrap = new QHBoxLayout();
    toggleWrap->setSpacing(6);
    toggleWrap->addWidget(m_estanteBtn);
    toggleWrap->addWidget(m_listaBtn);
    header->addLayout(toggleWrap);
    col->addLayout(header);

    // --- Scroll + holder com os dois containers (estante / lista) ---
    m_recentsScroll = new FlowScrollArea(this);
    m_recentsScroll->setObjectName(QStringLiteral("menuRecentsScroll"));

    m_holder = new QWidget(m_recentsScroll);
    m_holder->setObjectName(QStringLiteral("menuRecentsHolder"));
    auto* holderCol = new QVBoxLayout(m_holder);
    holderCol->setContentsMargins(0, 2, 6, 8);
    holderCol->setSpacing(0);

    m_gridContainer = new QWidget(m_holder);
    m_gridFlow = new FlowLayout(m_gridContainer, 0, 18, 18);
    holderCol->addWidget(m_gridContainer);

    m_listContainer = new QWidget(m_holder);
    m_listCol = new QVBoxLayout(m_listContainer);
    m_listCol->setContentsMargins(0, 0, 0, 0);
    m_listCol->setSpacing(8);
    holderCol->addWidget(m_listContainer);

    m_emptyLabel = new QLabel(
        tr("Você ainda não tem projetos.\n"
           "Clique em \"Novo projeto\" pra começar, ou em \"Carregar pasta\" pra abrir uma existente."),
        m_holder);
    m_emptyLabel->setObjectName(QStringLiteral("menuEmpty"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->hide();
    holderCol->addWidget(m_emptyLabel);

    holderCol->addStretch(1);

    m_recentsScroll->setWidget(m_holder);
    col->addWidget(m_recentsScroll, 1);

    // Estado inicial do alternador.
    m_estanteBtn->setChecked(m_viewMode == ViewMode::Estante);
    m_listaBtn->setChecked(m_viewMode == ViewMode::Lista);
    m_gridContainer->setVisible(m_viewMode == ViewMode::Estante);
    m_listContainer->setVisible(m_viewMode == ViewMode::Lista);
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
    // m_cards guarda apenas BookCard* da Estante (na Lista fica vazio); o
    // static_cast é seguro e evita Q_OBJECT/qobject_cast no tipo local.
    for (QWidget* c : m_cards) {
        if (!c) continue;
        static_cast<BookCard*>(c)->setDimmed(hovered != nullptr && c != hovered);
    }
}

void MainMenuDialog::setViewMode(ViewMode mode)
{
    m_viewMode = mode;
    if (m_estanteBtn) m_estanteBtn->setChecked(mode == ViewMode::Estante);
    if (m_listaBtn)   m_listaBtn->setChecked(mode == ViewMode::Lista);
    populateActiveView();
}

void MainMenuDialog::refreshRecents()
{
    populateActiveView();
}

void MainMenuDialog::populateActiveView()
{
    if (!m_gridFlow || !m_listCol) return;

    // Esvazia os dois containers e o estado de hover, destruindo os widgets
    // antigos. (Ambos são limpos; só o ativo é repovoado.)
    m_cards.clear();
    auto clearLayout = [](QLayout* l) {
        while (QLayoutItem* it = l->takeAt(0)) {
            if (QWidget* w = it->widget()) w->deleteLater();
            delete it;
        }
    };
    clearLayout(m_gridFlow);
    clearLayout(m_listCol);

    // Coleta os projetos válidos (lê o índice uma única vez por projeto).
    QStringList validPaths;
    QList<RecentInfo> validInfos;
    for (const QString& path : m_recentPaths) {
        if (path.isEmpty() || !QDir(path).exists()) continue;
        const RecentInfo info = readRecentInfo(path);
        if (!info.valid) continue;
        validPaths.append(path);
        validInfos.append(info);
    }

    const int n = validPaths.size();
    if (m_countLabel) {
        m_countLabel->setText(n == 0 ? tr("Nenhum projeto ainda")
                              : n == 1 ? tr("1 projeto")
                                       : tr("%1 projetos").arg(n));
    }

    const bool empty = (n == 0);
    if (m_emptyLabel)     m_emptyLabel->setVisible(empty);
    if (m_gridContainer)  m_gridContainer->setVisible(!empty && m_viewMode == ViewMode::Estante);
    if (m_listContainer)  m_listContainer->setVisible(!empty && m_viewMode == ViewMode::Lista);

    QWidget* parentForCards = (m_viewMode == ViewMode::Estante) ? m_gridContainer
                                                                : m_listContainer;
    for (int i = 0; i < n; ++i) {
        const QString capturedPath = validPaths.at(i);
        const RecentInfo& info = validInfos.at(i);
        const bool isAutoOpen = !m_autoOpenPath.isEmpty()
            && QDir::cleanPath(capturedPath) == m_autoOpenPath;
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

        if (m_viewMode == ViewMode::Estante) {
            auto* card = new BookCard(capturedPath, info, isAutoOpen, std::move(cbs), parentForCards);
            m_gridFlow->addWidget(card);
            m_cards.append(card);
        } else {
            auto* roww = new BookRow(capturedPath, info, isAutoOpen, std::move(cbs), parentForCards);
            m_listCol->addWidget(roww);
        }
    }

    if (m_recentsScroll) m_recentsScroll->reflow();
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
            border-radius: 8px;
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
            border-radius: 8px;
            padding: 10px 14px;
            font-size: 13px;
            min-height: 22px;
        }
        QPushButton#menuSecondaryBtn:hover { background: %7; color: %3; border-color: %9; }

        #menuLangLabel { color: %4; font-size: 11px; }
        QComboBox#menuLangCombo {
            background: %1;
            color: %2;
            border: 1px solid %6;
            border-radius: 5px;
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
            border-radius: 7px;
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

        #menuEmpty {
            color: %4;
            font-size: 13px;
            padding: 60px 30px;
        }
        #menuRecentsScroll { background: transparent; border: none; }
        #menuRecentsHolder { background: transparent; }
        #menuRecentsScroll QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 0;
        }
        #menuRecentsScroll QScrollBar::handle:vertical {
            background: %6;
            border-radius: 5px;
            min-height: 32px;
        }
        #menuRecentsScroll QScrollBar::handle:vertical:hover { background: %9; }
        #menuRecentsScroll QScrollBar::add-line:vertical,
        #menuRecentsScroll QScrollBar::sub-line:vertical { height: 0; }
        #menuRecentsScroll QScrollBar::add-page:vertical,
        #menuRecentsScroll QScrollBar::sub-page:vertical { background: transparent; }

        QFrame#bookCard { background: transparent; border: none; }
        #bookCardName {
            color: %3;
            font-size: 14px;
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

        QFrame#bookRow {
            background: %5;
            border: 1px solid %6;
            border-radius: 10px;
        }
        QFrame#bookRow:hover { background: %7; border-color: %9; }
        #bookRowName { color: %3; font-size: 15px; font-weight: 600; }
        #bookRowMeta { color: %4; font-size: 12px; }
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
    refreshActionIcons();
}
