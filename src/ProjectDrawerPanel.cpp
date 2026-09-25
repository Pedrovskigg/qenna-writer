#include "ProjectDrawerPanel.h"

#include "CoverUtils.h"
#include "ManuscriptViews.h"
#include "ProjectModel.h"
#include "Theme.h"
#include "WordCounter.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

namespace {

constexpr int kPanelWidth = 340;

QColor tcol(const QString& css) { return Theme::toColor(css); }

QFont serif(qreal px, int weight = QFont::Normal, bool italic = false) {
    QFont f(QStringLiteral("Lora"));
    f.setPixelSize(qRound(px));
    f.setWeight(QFont::Weight(weight));
    f.setItalic(italic);
    return f;
}

QFont ui(qreal px, int weight = QFont::Normal) {
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(qRound(px));
    f.setWeight(QFont::Weight(weight));
    return f;
}

QLabel* caps(const QString& text, QWidget* parent) {
    auto* l = new QLabel(text.toUpper(), parent);
    QFont f = ui(9.5, QFont::Bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
    l->setFont(f);
    l->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textMuted()));
    return l;
}

// Capa clicável: a imagem do projeto (ou uma gerada com o nome) e, com o
// mouse em cima, "Trocar capa".
class CoverButton : public QToolButton {
public:
    explicit CoverButton(QWidget* parent) : QToolButton(parent) {
        setFixedSize(138, 198);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
    }
    QPixmap image;
    QString title, author;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF r = QRectF(rect()).adjusted(0, 0, -4, -4);
        // sombra
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 90));
        p.drawRoundedRect(r.translated(3, 4), 5, 5);
        QPainterPath clip;
        clip.addRoundedRect(r, 3, 6);
        p.save();
        p.setClipPath(clip);
        if (!image.isNull()) {
            const qreal dpr = devicePixelRatioF();
            QPixmap sc = image.scaled((r.size() * dpr).toSize(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            sc.setDevicePixelRatio(dpr);
            p.drawPixmap(QPointF(r.center().x() - sc.width() / dpr / 2, r.center().y() - sc.height() / dpr / 2), sc);
        } else {
            const QColor c = MsPaint::bookColor(title.isEmpty() ? QStringLiteral("projeto") : title);
            QLinearGradient g(r.topLeft(), r.bottomRight());
            g.setColorAt(0, c.lighter(125));
            g.setColorAt(1, c.darker(260));
            p.fillRect(r, g);
            p.setPen(QColor(255, 255, 255, 235));
            p.setFont(serif(19, QFont::DemiBold));
            p.drawText(r.adjusted(13, 13, -10, -36), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, title);
            QFont sf = ui(8, QFont::Bold);
            sf.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
            p.setFont(sf);
            p.setPen(QColor(255, 255, 255, 170));
            p.drawText(r.adjusted(10, 0, -8, -10), Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap, author.toUpper());
        }
        p.fillRect(QRectF(r.left(), r.top(), 4, r.height()), QColor(0, 0, 0, 70));
        if (underMouse()) {
            const QRectF band(r.left() + 6, r.bottom() - 28, r.width() - 12, 22);
            p.setBrush(QColor(0, 0, 0, 150));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(band, 6, 6);
            p.setPen(Qt::white);
            p.setFont(ui(11));
            p.drawText(band, Qt::AlignCenter, ProjectDrawerPanel::tr("Trocar capa"));
        }
        p.restore();
    }
};

// Pílulas que quebram linha (gêneros).
class ChipFlow : public QWidget {
public:
    explicit ChipFlow(QWidget* parent) : QWidget(parent) {
        QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sp.setHeightForWidth(true);
        setSizePolicy(sp);
    }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return place(w, false); }
    QSize sizeHint() const override { return QSize(kPanelWidth - 40, heightForWidth(width() > 0 ? width() : kPanelWidth - 40)); }
    void relayout() { place(width(), true); updateGeometry(); }

protected:
    void resizeEvent(QResizeEvent*) override { place(width(), true); }

private:
    int place(int w, bool apply) const {
        int x = 0, y = 0, h = 0;
        for (QObject* o : children()) {
            auto* c = qobject_cast<QWidget*>(o);
            if (!c || c->isHidden()) continue;
            const QSize s = c->sizeHint();
            if (x > 0 && x + s.width() > w) { x = 0; y += h + 6; h = 0; }
            if (apply) c->setGeometry(x, y, s.width(), s.height());
            x += s.width() + 6;
            h = qMax(h, s.height());
        }
        return y + h;
    }
};

}  // namespace

ProjectDrawerPanel::ProjectDrawerPanel(ProjectModel* model, QWidget* parent)
    : QFrame(parent), m_model(model) {
    setObjectName(QStringLiteral("projectDrawerPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(kPanelWidth);
    buildUi();
    applyTheme();

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(650);
    connect(m_saveTimer, &QTimer::timeout, this, &ProjectDrawerPanel::saveNow);

    if (m_model) {
        connect(m_model, &ProjectModel::projectDetailsChanged, this, [this]() { if (isVisible()) refresh(); });
        connect(m_model, &ProjectModel::projectNameChanged, this, [this]() { if (isVisible()) refresh(); });
        connect(m_model, &ProjectModel::manuscriptsChanged, this, [this]() { if (isVisible()) { rebuildStats(); rebuildBooks(); } });
        connect(m_model, &ProjectModel::drawersChanged, this, [this]() { if (isVisible()) rebuildStats(); });
        connect(m_model, &ProjectModel::activeManuscriptChanged, this, [this]() { if (isVisible()) rebuildBooks(); });
    }
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, &ProjectDrawerPanel::applyTheme);
    hide();
}

void ProjectDrawerPanel::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Cabeçalho
    auto* head = new QWidget(this);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(18, 12, 10, 4);
    hl->addWidget(caps(tr("Projeto"), head));
    hl->addStretch(1);
    auto* close = new QToolButton(head);
    close->setText(QStringLiteral("×"));
    close->setCursor(Qt::PointingHandCursor);
    close->setToolTip(tr("Fechar"));
    close->setObjectName(QStringLiteral("pdClose"));
    connect(close, &QToolButton::clicked, this, &ProjectDrawerPanel::closePanel);
    hl->addWidget(close);
    root->addWidget(head);

    auto* scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* body = new QWidget(scroll);
    body->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* v = new QVBoxLayout(body);
    v->setContentsMargins(18, 6, 18, 18);
    v->setSpacing(0);

    // Capa + nome + autor
    auto* top = new QHBoxLayout;
    top->setSpacing(14);
    auto* cover = new CoverButton(body);
    m_cover = cover;
    cover->setToolTip(tr("Clique pra trocar a capa · botão direito pra remover"));
    cover->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(cover, &QToolButton::clicked, this, &ProjectDrawerPanel::pickCover);
    connect(cover, &QToolButton::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        QAction* pick = menu.addAction(tr("Trocar capa…"));
        QAction* clear = menu.addAction(tr("Remover capa"));
        clear->setEnabled(!m_coverDataUrl.isEmpty());
        QAction* a = menu.exec(m_cover->mapToGlobal(pos));
        if (a == pick) pickCover();
        else if (a == clear) { m_coverDataUrl.clear(); updateCover(); saveNow(); }
    });
    top->addWidget(cover, 0, Qt::AlignBottom);
    auto* id = new QVBoxLayout;
    id->setSpacing(2);
    id->addStretch(1);
    m_name = new QLineEdit(body);
    m_name->setObjectName(QStringLiteral("pdName"));
    m_name->setFont(serif(19, QFont::DemiBold));
    m_name->setPlaceholderText(tr("Nome do projeto"));
    id->addWidget(m_name);
    m_author = new QLineEdit(body);
    m_author->setObjectName(QStringLiteral("pdAuthor"));
    m_author->setFont(ui(13));
    m_author->setPlaceholderText(tr("Autor"));
    id->addWidget(m_author);
    m_saved = new QLabel(tr("✓ salvo"), body);
    m_saved->setFont(ui(11));
    m_saved->setContentsMargins(5, 4, 0, 0);
    m_saved->hide();
    id->addWidget(m_saved);
    top->addLayout(id, 1);
    v->addLayout(top);
    connect(m_name, &QLineEdit::textEdited, this, [this]() { scheduleSave(); updateCover(); });
    connect(m_author, &QLineEdit::textEdited, this, [this]() { scheduleSave(); updateCover(); });
    connect(m_name, &QLineEdit::editingFinished, this, [this]() { if (m_saveTimer->isActive()) saveNow(); });
    connect(m_author, &QLineEdit::editingFinished, this, [this]() { if (m_saveTimer->isActive()) saveNow(); });

    // Gêneros
    v->addSpacing(14);
    m_genres = new ChipFlow(body);
    v->addWidget(m_genres);

    // Números
    v->addSpacing(14);
    m_stats = new QWidget(body);
    new QHBoxLayout(m_stats);
    m_stats->layout()->setContentsMargins(0, 0, 0, 0);
    m_stats->layout()->setSpacing(8);
    v->addWidget(m_stats);

    // Sinopse
    v->addSpacing(16);
    v->addWidget(caps(tr("Sinopse"), body));
    v->addSpacing(6);
    m_synopsis = new QTextEdit(body);
    m_synopsis->setObjectName(QStringLiteral("pdSynopsis"));
    m_synopsis->setAcceptRichText(false);
    m_synopsis->setFrameShape(QFrame::NoFrame);
    m_synopsis->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_synopsis->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_synopsis->setFont(serif(14));
    m_synopsis->setPlaceholderText(tr("Escreva a sinopse do projeto…"));
    m_synopsis->document()->setDocumentMargin(4);
    connect(m_synopsis, &QTextEdit::textChanged, this, [this]() {
        fitSynopsis();
        if (!m_loading) scheduleSave();
    });
    v->addWidget(m_synopsis);

    // A saga
    v->addSpacing(16);
    v->addWidget(caps(tr("A saga"), body));
    v->addSpacing(4);
    m_books = new QWidget(body);
    m_booksLayout = new QVBoxLayout(m_books);
    m_booksLayout->setContentsMargins(0, 0, 0, 0);
    m_booksLayout->setSpacing(2);
    v->addWidget(m_books);
    v->addStretch(1);

    scroll->setWidget(body);
    root->addWidget(scroll, 1);
}

void ProjectDrawerPanel::applyTheme() {
    setStyleSheet(Theme::panelQss(QStringLiteral("projectDrawerPanel")) + Theme::qss(QStringLiteral(R"(
        QToolButton#pdClose { background: transparent; border: none; color: %1; font-size: 17px; padding: 0 6px; }
        QToolButton#pdClose:hover { color: %2; }
        QLineEdit#pdName, QLineEdit#pdAuthor {
            background: transparent; border: 1px solid transparent; border-radius: @radius-item;
            padding: 1px 4px; selection-background-color: %5;
        }
        QLineEdit#pdName { color: %2; }
        QLineEdit#pdAuthor { color: %3; }
        QLineEdit#pdName:hover, QLineEdit#pdAuthor:hover { background: %4; }
        QLineEdit#pdName:focus, QLineEdit#pdAuthor:focus { background: %4; border-color: %6; }
        QTextEdit#pdSynopsis {
            background: transparent; border: 1px solid transparent; border-radius: @radius-item;
            padding: 0; color: %3;
        }
        QTextEdit#pdSynopsis:hover { background: %4; }
        QTextEdit#pdSynopsis:focus { background: %4; border-color: %6; }
    )")).arg(Theme::textMuted(), Theme::textBright(), Theme::textPrimary(), Theme::hoverOverlay(),
             Theme::accentInfoSoft(), Theme::accentDefault()));
    if (m_saved) m_saved->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::accentSuccess()));
    if (isVisible()) refresh();
}

void ProjectDrawerPanel::setWordCounter(WordCounter* wc) { m_wordCounter = wc; }

void ProjectDrawerPanel::open() {
    refresh();
    show();
    raise();
}

void ProjectDrawerPanel::closePanel() {
    if (m_saveTimer && m_saveTimer->isActive()) saveNow();
    hide();
    emit panelClosed();
}

QStringList ProjectDrawerPanel::genres() const { return m_genreList; }

void ProjectDrawerPanel::setGenres(const QStringList& list) {
    m_genreList = list;
    rebuildGenres();
    saveNow();
}

void ProjectDrawerPanel::refresh() {
    if (!m_model) return;
    m_loading = true;
    if (!m_name->hasFocus()) m_name->setText(m_model->projectName());
    if (!m_author->hasFocus()) m_author->setText(m_model->projectAuthor());
    if (!m_synopsis->hasFocus() && m_synopsis->toPlainText() != m_model->projectSynopsis())
        m_synopsis->setPlainText(m_model->projectSynopsis());
    m_coverDataUrl = m_model->projectCoverDataUrl();
    QStringList g;
    for (const QString& s : m_model->projectGenres().split(QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts))
        if (!s.trimmed().isEmpty()) g << s.trimmed();
    m_genreList = g;
    m_loading = false;
    updateCover();
    rebuildGenres();
    rebuildStats();
    rebuildBooks();
    fitSynopsis();
}

void ProjectDrawerPanel::updateCover() {
    auto* c = static_cast<CoverButton*>(m_cover);
    c->image = CoverUtils::pixmapFromDataUrl(m_coverDataUrl);
    c->title = m_name->text().trimmed().isEmpty() ? tr("(sem nome)") : m_name->text().trimmed();
    c->author = m_author->text().trimmed();
    c->update();
}

void ProjectDrawerPanel::rebuildGenres() {
    for (QObject* o : m_genres->children()) if (auto* w = qobject_cast<QWidget*>(o)) { w->hide(); w->deleteLater(); }
    const QString chipQss = Theme::qss(QStringLiteral(
        "QFrame#pdChip { border: 1px solid %1; border-radius: 11px; background: transparent; }"
        "QLabel { color: %2; background: transparent; }"
        "QToolButton { color: %3; background: transparent; border: none; padding: 0 2px; }"
        "QToolButton:hover { color: %4; }"))
        .arg(Theme::subtleBorder(), Theme::textPrimary(), Theme::textMuted(), Theme::textBright());
    for (int i = 0; i < m_genreList.size(); ++i) {
        auto* chip = new QFrame(m_genres);
        chip->setObjectName(QStringLiteral("pdChip"));
        chip->setStyleSheet(chipQss);
        auto* l = new QHBoxLayout(chip);
        l->setContentsMargins(10, 2, 4, 2);
        l->setSpacing(2);
        auto* t = new QLabel(m_genreList.at(i), chip);
        t->setFont(ui(12));
        l->addWidget(t);
        auto* x = new QToolButton(chip);
        x->setText(QStringLiteral("×"));
        x->setCursor(Qt::PointingHandCursor);
        x->setToolTip(tr("Tirar gênero"));
        connect(x, &QToolButton::clicked, this, [this, i]() {
            QStringList g = m_genreList;
            if (i < g.size()) g.removeAt(i);
            QTimer::singleShot(0, this, [this, g]() { setGenres(g); });
        });
        l->addWidget(x);
        chip->show();
    }
    // "+ gênero": vira um campo ali mesmo.
    auto* add = new QToolButton(m_genres);
    add->setText(tr("+ gênero"));
    add->setCursor(Qt::PointingHandCursor);
    add->setStyleSheet(Theme::qss(QStringLiteral(
        "QToolButton { color: %1; background: transparent; border: 1px dashed %2; border-radius: 11px; padding: 3px 10px; font-size: 12px; }"
        "QToolButton:hover { color: %3; border-color: %3; }"))
        .arg(Theme::textMuted(), Theme::subtleBorder(), Theme::textBright()));
    auto* edit = new QLineEdit(m_genres);
    edit->setPlaceholderText(tr("novo gênero"));
    edit->setFixedWidth(130);
    edit->setFont(ui(12));
    edit->setStyleSheet(Theme::qss(QStringLiteral(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 11px; padding: 2px 10px; }"))
        .arg(Theme::hoverOverlay(), Theme::textBright(), Theme::accentDefault()));
    edit->hide();
    connect(add, &QToolButton::clicked, this, [add, edit, this]() {
        add->hide();
        edit->show();
        static_cast<ChipFlow*>(m_genres)->relayout();
        edit->setFocus();
    });
    connect(edit, &QLineEdit::editingFinished, this, [this, edit]() {
        const QString g = edit->text().trimmed();
        QStringList list = m_genreList;
        if (!g.isEmpty() && !list.contains(g, Qt::CaseInsensitive)) list << g;
        QTimer::singleShot(0, this, [this, list]() { setGenres(list); });
    });
    add->show();
    static_cast<ChipFlow*>(m_genres)->relayout();
}

void ProjectDrawerPanel::rebuildStats() {
    QLayout* lay = m_stats->layout();
    while (QLayoutItem* it = lay->takeAt(0)) { if (it->widget()) it->widget()->deleteLater(); delete it; }
    if (!m_model) return;
    int characters = 0;
    for (const auto& d : m_model->drawers())
        if (d.drawerElementType == QStringLiteral("character")) characters += d.items.size();
    const int words = m_wordCounter ? m_wordCounter->countProject() : 0;
    auto box = [&](const QString& value, const QString& label) {
        auto* f = new QFrame(m_stats);
        f->setObjectName(QStringLiteral("pdStat"));
        f->setStyleSheet(QStringLiteral("#pdStat { background: %1; border: 1px solid %2; border-radius: 8px; }")
            .arg(Theme::hoverOverlay(), Theme::subtleBorder()));
        auto* l = new QVBoxLayout(f);
        l->setContentsMargins(4, 7, 4, 7);
        l->setSpacing(1);
        auto* vl = new QLabel(value, f);
        vl->setAlignment(Qt::AlignCenter);
        vl->setFont(ui(15, QFont::Bold));
        vl->setStyleSheet(QStringLiteral("color: %1; background: transparent; border: none;").arg(Theme::textBright()));
        l->addWidget(vl);
        auto* ll = new QLabel(label, f);
        ll->setAlignment(Qt::AlignCenter);
        QFont lf = ui(8.5);
        lf.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
        ll->setFont(lf);
        ll->setStyleSheet(QStringLiteral("color: %1; background: transparent; border: none;").arg(Theme::textMuted()));
        l->addWidget(ll);
        lay->addWidget(f);
    };
    box(QString::number(m_model->manuscripts().size()), tr("LIVROS"));
    box(MsPaint::fmtInt(words), tr("PALAVRAS"));
    box(QString::number(characters), tr("PERSONAGENS"));
}

void ProjectDrawerPanel::rebuildBooks() {
    while (QLayoutItem* it = m_booksLayout->takeAt(0)) { if (it->widget()) it->widget()->deleteLater(); delete it; }
    if (!m_model) return;
    const auto& mss = m_model->manuscripts();
    const QString active = m_model->activeManuscriptId();
    const qreal dpr = devicePixelRatioF();
    for (int i = 0; i < mss.size(); ++i) {
        const Manuscript& m = mss.at(i);
        QPixmap mini = CoverUtils::pixmapFromDataUrl(m_model->manuscriptEffectiveCoverDataUrl(m.id));
        if (mini.isNull()) mini = MsPaint::generatedCover(m.title, i + 1, MsPaint::bookColor(m.id), QSize(28, 40), dpr, tr("Livro"));
        else {
            mini = mini.scaled(QSize(28, 40) * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            mini.setDevicePixelRatio(dpr);
        }
        const int words = m_wordCounter ? m_wordCounter->countManuscript(m.id) : 0;
        const bool on = (m.id == active);
        auto* row = new QToolButton(m_books);
        row->setCursor(Qt::PointingHandCursor);
        row->setToolTip(tr("Abrir este livro"));
        row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->setFixedHeight(50);
        row->setStyleSheet(Theme::qss(QStringLiteral(
            "QToolButton { background: %1; border: none; border-radius: @radius-item; }"
            "QToolButton:hover { background: %2; }"))
            .arg(on ? Theme::accentInfoSoft() : QStringLiteral("transparent"), Theme::hoverOverlay()));
        auto* l = new QHBoxLayout(row);
        l->setContentsMargins(6, 4, 6, 4);
        l->setSpacing(10);
        auto* cv = new QLabel(row);
        cv->setPixmap(mini);
        cv->setFixedSize(28, 40);
        cv->setAttribute(Qt::WA_TransparentForMouseEvents);
        l->addWidget(cv);
        auto* tx = new QVBoxLayout;
        tx->setSpacing(0);
        auto* t = new QLabel(m.title.isEmpty() ? tr("(sem título)") : m.title, row);
        t->setFont(serif(13.5, QFont::DemiBold));
        t->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textBright()));
        t->setAttribute(Qt::WA_TransparentForMouseEvents);
        tx->addWidget(t);
        auto* s = new QLabel(tr("livro %1 · %2 palavras").arg(i + 1).arg(MsPaint::fmtInt(words)), row);
        s->setFont(ui(11));
        s->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textMuted()));
        s->setAttribute(Qt::WA_TransparentForMouseEvents);
        tx->addWidget(s);
        l->addLayout(tx, 1);
        const QString id = m.id;
        connect(row, &QToolButton::clicked, this, [this, id]() {
            if (m_model && m_model->activeManuscriptId() != id) m_model->setActiveManuscriptId(id);
            emit manuscriptActivated(id);
        });
        m_booksLayout->addWidget(row);
    }
    if (mss.isEmpty()) {
        auto* none = new QLabel(tr("Nenhum livro ainda."), m_books);
        none->setFont(serif(13, QFont::Normal, true));
        none->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Theme::textMuted()));
        m_booksLayout->addWidget(none);
    }
}

void ProjectDrawerPanel::fitSynopsis() {
    const int w = qMax(100, m_synopsis->viewport()->width());
    m_synopsis->document()->setTextWidth(w);
    const int h = int(std::ceil(m_synopsis->document()->size().height())) + 4;
    m_synopsis->setFixedHeight(qMax(60, h));
}

void ProjectDrawerPanel::scheduleSave() {
    if (!m_loading) m_saveTimer->start();
}

void ProjectDrawerPanel::saveNow() {
    if (!m_model || m_loading) return;
    m_saveTimer->stop();
    m_loading = true;   // o sinal do modelo não reescreve o que está sendo digitado
    m_model->setProjectDetails(m_name->text().trimmed(), m_author->text().trimmed(),
                               m_genreList.join(QStringLiteral(", ")),
                               m_synopsis->toPlainText().trimmed(), m_coverDataUrl);
    m_loading = false;
    m_saved->show();
    QTimer::singleShot(1600, m_saved, [this]() { if (!m_saveTimer->isActive()) m_saved->hide(); });
}

void ProjectDrawerPanel::pickCover() {
    const QString startDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString path = QFileDialog::getOpenFileName(this, tr("Escolher capa"), startDir,
                                                      tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
    if (path.isEmpty()) return;
    const QString dataUrl = CoverUtils::loadCoverAsDataUrl(path);
    if (dataUrl.isEmpty()) return;
    m_coverDataUrl = dataUrl;
    updateCover();
    saveNow();
}
