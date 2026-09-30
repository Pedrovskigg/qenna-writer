#include "NewProjectSheet.h"

#include "CoverUtils.h"
#include "QuickCoverWidgets.h"
#include "Theme.h"
#include "TimelineTracksTypes.h"

#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Tracks;

namespace {

constexpr int kSheetW = 660;
const char* kLastAuthorKey = "newProject/lastAuthor";
const char* kLastParentKey = "newProject/lastParentDir";

QString sanitizeProjectName(const QString& raw)
{
    QString s = raw;
    s.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")));
    s = s.trimmed();
    if (s.isEmpty()) s = QObject::tr("Novo Projeto");
    return s;
}

} // namespace

// Gêneros em etiquetas: digita e Enter (ou vírgula) vira etiqueta; o ✕ tira;
// Backspace no campo vazio tira a última.
class TagEdit : public QFrame {
public:
    explicit TagEdit(QWidget* parent) : QFrame(parent)
    {
        setObjectName(QStringLiteral("sheetTags"));
        setFixedHeight(32);
        m_row = new QHBoxLayout(this);
        m_row->setContentsMargins(6, 0, 8, 0);
        m_row->setSpacing(5);
        m_edit = new QLineEdit(this);
        m_edit->setObjectName(QStringLiteral("sheetTagInput"));
        m_edit->setFont(uiFont(12.5));
        m_edit->setFrame(false);
        m_edit->setPlaceholderText(QCoreApplication::translate("NewProjectSheet", "+ gênero"));
        m_row->addWidget(m_edit, 1);
        m_edit->installEventFilter(this);
        QObject::connect(m_edit, &QLineEdit::textEdited, this, [this](const QString& s) {
            if (s.contains(QLatin1Char(','))) { commit(); }
        });
    }
    QStringList tags() const { return m_tags; }
    void setTags(const QStringList& tags)
    {
        while (!m_chips.isEmpty()) removeAt(int(m_chips.size()) - 1);
        m_edit->setText(tags.join(QLatin1Char(',')));
        commit();
    }
    QString joined() const
    {
        QStringList all = m_tags;
        const QString pending = m_edit->text().remove(QLatin1Char(',')).trimmed();
        if (!pending.isEmpty() && !all.contains(pending, Qt::CaseInsensitive)) all << pending;
        return all.join(QStringLiteral(", "));
    }

protected:
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (o == m_edit && e->type() == QEvent::KeyPress) {
            auto* k = static_cast<QKeyEvent*>(e);
            if ((k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter) && !m_edit->text().trimmed().isEmpty()) {
                commit();
                return true;
            }
            if (k->key() == Qt::Key_Backspace && m_edit->text().isEmpty() && !m_chips.isEmpty()) {
                removeAt(int(m_chips.size()) - 1);
                return true;
            }
        }
        return QFrame::eventFilter(o, e);
    }

private:
    void commit()
    {
        const QStringList parts = m_edit->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (QString p : parts) {
            p = p.trimmed();
            if (p.isEmpty() || m_tags.contains(p, Qt::CaseInsensitive) || m_tags.size() >= 4) continue;
            m_tags << p;
            auto* chip = new QToolButton(this);
            chip->setObjectName(QStringLiteral("sheetTag"));
            chip->setText(p + QStringLiteral("  ✕"));
            chip->setToolTip(QCoreApplication::translate("NewProjectSheet", "Tirar"));
            chip->setCursor(Qt::PointingHandCursor);
            chip->setFocusPolicy(Qt::NoFocus);
            chip->setFixedHeight(20);
            chip->setFont(uiFont(11.5));
            QObject::connect(chip, &QToolButton::clicked, this, [this, chip]() { removeAt(int(m_chips.indexOf(chip))); });
            m_row->insertWidget(int(m_chips.size()), chip);
            m_chips << chip;
        }
        m_edit->clear();
    }
    void removeAt(int i)
    {
        if (i < 0 || i >= m_chips.size()) return;
        m_tags.removeAt(i);
        m_chips.takeAt(i)->deleteLater();
    }

    QHBoxLayout* m_row;
    QLineEdit* m_edit;
    QStringList m_tags;
    QList<QToolButton*> m_chips;
};

NewProjectSheet::NewProjectSheet(const QStringList& fontFamilies, QWidget* parent)
    : SheetDialog(parent, kSheetW)
{
    setEyebrow(tr("Novo projeto"));
    const Palette pal = Palette::onPage();
    QSettings st;
    m_rememberedAuthor = st.value(QLatin1String(kLastAuthorKey)).toString();
    m_parentDir = st.value(QLatin1String(kLastParentKey)).toString();
    if (m_parentDir.isEmpty() || !QDir(m_parentDir).exists())
        m_parentDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    QStringList families = fontFamilies;
    const QString serif = serifFont(12).family();
    if (families.isEmpty()) families << serif;
    m_spec = QuickCover::defaults(families.contains(serif) ? serif : families.first());

    auto* cols = new QHBoxLayout;
    cols->setSpacing(22);
    body()->addLayout(cols);

    // ── a capa ──
    auto* left = new QVBoxLayout;
    left->setSpacing(8);
    m_canvas = new QuickCoverCanvas(&m_spec, card());
    left->addWidget(m_canvas);
    m_canvasHint = new QLabel(tr("clique num texto pra editar · arraste pra mover"), card());
    m_canvasHint->setObjectName(QStringLiteral("sheetDim"));
    m_canvasHint->setFont(uiFont(11));
    m_canvasHint->setAlignment(Qt::AlignHCenter);
    m_canvasHint->hide();
    left->addWidget(m_canvasHint);
    left->addStretch(1);
    cols->addLayout(left);

    // ── lado direito: campos, ou os controles da capa ──
    m_right = new QStackedWidget(card());
    m_right->setMinimumHeight(360);
    cols->addWidget(m_right, 1);

    auto* fields = new QWidget(m_right);
    auto* fv = new QVBoxLayout(fields);
    fv->setContentsMargins(0, 0, 0, 0);
    fv->setSpacing(10);
    m_title = titleEdit(fields, false, 25);
    m_title->setPlaceholderText(tr("Título do projeto"));
    confirmOnEnter(m_title);
    fv->addWidget(m_title);

    auto group = [&](const QString& label, QWidget* w) {
        auto* g = new QVBoxLayout;
        g->setSpacing(5);
        g->addWidget(sectionLabel(label, fields));
        g->addWidget(w);
        fv->addLayout(g);
    };
    m_author = field(fields);
    m_author->setPlaceholderText(tr("Ex: Maria Silva"));
    m_author->setText(m_rememberedAuthor);
    {
        // "do último projeto" dentro do campo, à direita, enquanto não mudar
        auto* hl = new QHBoxLayout(m_author);
        hl->setContentsMargins(0, 0, 10, 0);
        hl->addStretch(1);
        m_authorHint = new QLabel(tr("do último projeto"), m_author);
        m_authorHint->setObjectName(QStringLiteral("sheetDim"));
        m_authorHint->setFont(uiFont(11));
        m_authorHint->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_authorHint->setVisible(!m_rememberedAuthor.isEmpty());
        hl->addWidget(m_authorHint);
    }
    confirmOnEnter(m_author);
    group(tr("Autor"), m_author);
    m_genres = new TagEdit(fields);
    group(tr("Gêneros"), m_genres);
    m_synopsis = textArea(fields, 70);
    m_synopsis->setPlaceholderText(tr("Escreva uma breve sinopse…"));
    group(tr("Sinopse (opcional)"), m_synopsis);

    m_typeBox = new QWidget(fields);
    {
        auto* g = new QVBoxLayout(m_typeBox);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(5);
        g->addWidget(sectionLabel(tr("É um"), m_typeBox));
        m_type = new SheetChoice({ tr("Livro"), tr("Roteiro") }, m_typeBox);
        m_type->setCurrentIndex(0);
        g->addWidget(m_type);
    }
    fv->addWidget(m_typeBox);
    m_templateBox = new QWidget(fields);
    {
        auto* tg = new QVBoxLayout(m_templateBox);
        tg->setContentsMargins(0, 0, 0, 0);
        tg->setSpacing(5);
        tg->addWidget(sectionLabel(tr("Começa"), m_templateBox));
        m_template = new SheetChoice({ tr("Em branco"), tr("Com gavetas básicas"), tr("Completo") }, m_templateBox);
        m_template->setCurrentIndex(1);
        tg->addWidget(m_template);
        m_templateHint = new QLabel(m_templateBox);
        m_templateHint->setObjectName(QStringLiteral("sheetDim"));
        m_templateHint->setWordWrap(true);
        m_templateHint->setFont(uiFont(11));
        tg->addWidget(m_templateHint);
    }
    fv->addWidget(m_templateBox);
    fv->addStretch(1);
    m_right->addWidget(fields);

    auto refreshTemplateHint = [this]() {
        const int i = m_template->currentIndex();
        m_templateHint->setText(i == 0 ? tr("Projeto limpo, sem gavetas prontas. Dá pra criar depois.")
                              : i == 1 ? tr("Planejamento, Personagens, Cenários, Objetos e Notas. Dá pra mudar depois.")
                                       : tr("Lore, base de dados, pesquisa e planejamento detalhado. Dá pra mudar depois."));
    };
    connect(m_template, &SheetChoice::activated, this, refreshTemplateHint);
    refreshTemplateHint();

    m_panel = new QuickCoverPanel(&m_spec, m_canvas,
        [this]() { return m_title->text(); },
        [this](const QString& s) { m_title->setText(s); },
        [this]() { return m_author->text(); },
        [this](const QString& s) { m_author->setText(s); },
        families, m_right);
    m_right->addWidget(m_panel);
    connect(m_panel, &QuickCoverPanel::doneRequested, this, [this]() { setEditing(false); });
    connect(m_panel, &QuickCoverPanel::exportRequested, this, &NewProjectSheet::exportImage);
    connect(m_panel, &QuickCoverPanel::pickImageRequested, this, &NewProjectSheet::pickImage);
    connect(m_canvas, &QuickCoverCanvas::editRequested, this, [this]() { setEditing(true); });

    // ── rodapé: a pasta numa linha + Cancelar / Criar projeto ──
    QPushButton* ok = addFooter(tr("Criar projeto"), QString());
    auto* pathRow = new QWidget(card());
    auto* ph = new QHBoxLayout(pathRow);
    ph->setContentsMargins(0, 0, 0, 0);
    ph->setSpacing(6);
    auto* folderIco = new QLabel(pathRow);
    {
        QPixmap pm(QSize(14, 14) * 2);
        pm.setDevicePixelRatio(2.0);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#e8b04a")));
        p.drawRoundedRect(QRectF(1, 3, 12, 9), 1.5, 1.5);
        p.drawRoundedRect(QRectF(1, 2, 5, 3), 1, 1);
        folderIco->setPixmap(pm);
    }
    ph->addWidget(folderIco);
    m_path = new QLabel(pathRow);
    m_path->setObjectName(QStringLiteral("sheetDim"));
    m_path->setTextFormat(Qt::RichText);
    m_path->setFont(uiFont(11.5));
    ph->addWidget(m_path, 1);
    auto* change = new QToolButton(pathRow);
    change->setObjectName(QStringLiteral("sheetPathLink"));
    // na cor de destaque, como link (o estilo vai no próprio botão: o do
    // QToolButton da folha ganhava dele)
    change->setStyleSheet(QStringLiteral(
        "QToolButton { background: transparent; border: none; color: %1; font-size: 11.5px; padding: 0; }"
        "QToolButton:hover { text-decoration: underline; }").arg(pal.accent.name()));
    change->setText(tr("trocar"));
    change->setCursor(Qt::PointingHandCursor);
    change->setFocusPolicy(Qt::NoFocus);
    connect(change, &QToolButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Onde salvar o projeto?"), m_parentDir);
        if (dir.isEmpty()) return;
        m_parentDir = dir;
        refreshPath();
    });
    ph->addWidget(change);
    pathRow->setMaximumWidth(kSheetW - 230);
    footer()->insertWidget(0, pathRow, 1);
    m_pathRow = pathRow;

    auto sync = [this, ok]() { ok->setEnabled(!m_title->text().trimmed().isEmpty()); };
    connect(m_title, &QLineEdit::textChanged, this, [this, sync]() { sync(); refreshPath(); refreshCanvasTexts(); });
    connect(m_author, &QLineEdit::textChanged, this, [this]() {
        m_authorHint->setVisible(!m_rememberedAuthor.isEmpty() && m_author->text() == m_rememberedAuthor);
        refreshCanvasTexts();
    });
    sync();

    applySheetTheme(Theme::qss(QStringLiteral(
        "QFrame#sheetTags { background: %1; border: 1px solid %2; border-radius: @radius-control; }"
        "QLineEdit#sheetTagInput { background: transparent; border: none; color: %3; padding: 0 2px; }"
        "QToolButton#sheetTag { background: %4; border: none; border-radius: 10px; color: %3; padding: 0 8px; }"
        "QToolButton#sheetTag:hover { background: %5; }")
        .arg(mix(pal.app, pal.page, 0.55).name(), pal.border.name(), pal.bright.name(),
             alpha(pal.accent, 0.16).name(QColor::HexArgb), alpha(pal.accent, 0.28).name(QColor::HexArgb))));
    refreshPath();
    refreshCanvasTexts();
}

void NewProjectSheet::refreshCanvasTexts()
{
    m_canvas->setTexts(m_title->text().trimmed().isEmpty() ? tr("Título") : m_title->text(), m_author->text());
}

void NewProjectSheet::refreshPath()
{
    // "Projetos › Nome": a pasta onde fica basta pra reconhecer (o caminho
    // inteiro está no tooltip)
    const QStringList parts = QDir::toNativeSeparators(m_parentDir).split(QDir::separator(), Qt::SkipEmptyParts);
    const QString tail = parts.isEmpty() ? m_parentDir : parts.last();
    const QString name = sanitizeProjectName(m_title->text());
    const QFontMetrics fm(m_path->font());
    const int room = qMax(80, m_path->width() > 10 ? m_path->width() : kSheetW - 330);
    const QString shownName = fm.elidedText(name, Qt::ElideRight, room / 2);
    const QString shownTail = fm.elidedText(tail, Qt::ElideLeft, qMax(40, room - fm.horizontalAdvance(shownName) - 20));
    m_path->setText(QStringLiteral("%1 › <b style='color:%2;font-weight:500'>%3</b>")
                        .arg(shownTail.toHtmlEscaped(), Palette::onPage().bright.name(), shownName.toHtmlEscaped()));
    m_path->setToolTip(QDir::toNativeSeparators(fullPath()));
}

void NewProjectSheet::setEditing(bool on)
{
    if (on && m_asIs) {
        // a capa de antes vira ponto de partida: o fundo sem texto (o Cover
        // Creator grava um) com título e autor por cima; sem ele, a capa
        // inteira como foto e nenhum texto (senão sairia título sobre título)
        m_asIs = false;
        m_openedOnce = true;
        m_canvas->setStaticCover(QPixmap());
        const QString family = m_spec.texts.isEmpty() ? QString() : m_spec.texts.first().family;
        m_spec = QuickCover::defaults(family);
        m_spec.image = QStringLiteral("custom");
        if (!m_existing.coverBg.isEmpty()) {
            m_spec.customImage = m_existing.coverBg;
        } else {
            m_spec.customImage = m_existing.cover;
            m_spec.texts.clear();
        }
        m_panel->imagePicked();
    }
    if (on && !m_openedOnce) {
        m_openedOnce = true;
        // primeira vez: já abre com uma foto, o degradê embaixo e um pouco de
        // grão — o ponto de partida que mostra o que a ferramenta faz
        if (m_spec.image.isEmpty() && m_spec.fadeType == 0 && m_spec.grain == 0) {
            const QStringList names = QuickCover::imageNames();
            const QString preferred = QStringLiteral("derived-kanagawa-wave.jpg");
            if (!names.isEmpty()) m_spec.image = names.contains(preferred) ? preferred : names.first();
            m_spec.fadeType = 1;
            m_spec.grain = 25;
        }
    }
    m_canvas->setEditing(on);
    m_canvasHint->setVisible(on);
    m_right->setCurrentIndex(on ? 1 : 0);
    if (m_editMode) setEyebrow(on ? tr("Editar projeto · capa rápida") : tr("Editar projeto"));
    else setEyebrow(on ? tr("Novo projeto · capa rápida") : tr("Novo projeto"));
    if (on) m_panel->syncFromSpec();
    else m_title->setFocus();
}

void NewProjectSheet::keyPressEvent(QKeyEvent* e)
{
    // Esc na capa rápida volta pros campos em vez de fechar a folha
    if (e->key() == Qt::Key_Escape && m_canvas->editing()) {
        setEditing(false);
        return;
    }
    SheetDialog::keyPressEvent(e);
}

void NewProjectSheet::pickImage()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString path = QFileDialog::getOpenFileName(this, tr("Escolher imagem da capa"), dir,
                                                      tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
    if (path.isEmpty()) return;
    const QString url = CoverUtils::loadCoverAsDataUrl(path);
    if (url.isEmpty()) return;
    m_spec.customImage = url;
    m_spec.image = QStringLiteral("custom");
    m_panel->imagePicked();
    m_canvas->refresh();
}

void NewProjectSheet::exportImage()
{
    const QString base = sanitizeProjectName(m_title->text());
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString path = QFileDialog::getSaveFileName(this, tr("Salvar capa como imagem"),
                                                      dir + QLatin1Char('/') + base + QStringLiteral(".png"),
                                                      tr("PNG (*.png)"));
    if (path.isEmpty()) return;
    // as fotos e as fontes do Qenna são livres de direitos: a capa é do autor
    QuickCover::render(m_spec, QSize(1600, 2400), true, m_title->text(), m_author->text())
        .save(path, "PNG");
}

void NewProjectSheet::setExisting(const Existing& e)
{
    m_editMode = true;
    m_existing = e;
    setEyebrow(tr("Editar projeto"));
    if (m_okBtn) m_okBtn->setText(tr("Salvar"));
    m_typeBox->hide();
    m_templateBox->hide();
    m_pathRow->hide();
    m_title->setText(e.name);
    m_author->setText(e.author);
    m_authorHint->hide();
    m_rememberedAuthor.clear();
    m_genres->setTags(e.genres.split(QLatin1Char(','), Qt::SkipEmptyParts));
    m_synopsis->setPlainText(e.synopsis);
    if (!e.quickCover.isEmpty()) {
        // feita na capa rápida: reabre de onde parou
        const QString family = m_spec.texts.isEmpty() ? QString() : m_spec.texts.first().family;
        m_spec = QuickCover::fromJson(e.quickCover, family);
        m_openedOnce = true;
    } else if (!e.cover.isEmpty()) {
        // veio de fora (Cover Creator, imagem escolhida): fica como está
        m_asIs = true;
        m_canvas->setStaticCover(CoverUtils::pixmapFromDataUrl(e.cover));
    }
    refreshCanvasTexts();
}

void NewProjectSheet::accept()
{
    if (m_title->text().trimmed().isEmpty()) { m_title->setFocus(); return; }
    if (m_asIs) {
        // capa intocada: a de antes, igualzinha
        m_coverFull = m_existing.cover;
        m_coverBg = m_existing.coverBg;
        SheetDialog::accept();
        return;
    }
    // a capa vira a capa do projeto: com texto (coverFull) e sem (coverBg)
    const QSize size(800, 1200);
    m_coverFull = QuickCover::toDataUrl(QuickCover::render(m_spec, size, true, m_title->text(), m_author->text()));
    m_coverBg = QuickCover::toDataUrl(QuickCover::render(m_spec, size, false, m_title->text(), m_author->text()));
    if (!m_editMode) {
        QSettings st;
        if (!m_author->text().trimmed().isEmpty()) st.setValue(QLatin1String(kLastAuthorKey), m_author->text().trimmed());
        st.setValue(QLatin1String(kLastParentKey), m_parentDir);
    }
    SheetDialog::accept();
}

QString NewProjectSheet::projectName() const { return m_title->text().trimmed(); }
QString NewProjectSheet::author() const { return m_author->text().trimmed(); }
QString NewProjectSheet::genres() const { return m_genres->joined(); }
QString NewProjectSheet::synopsis() const { return m_synopsis->toPlainText().trimmed(); }
QString NewProjectSheet::projectType() const
{
    return m_type->currentIndex() == 1 ? QStringLiteral("screenplay") : QStringLiteral("book");
}
QString NewProjectSheet::templateId() const
{
    const int i = m_template->currentIndex();
    return i == 0 ? QStringLiteral("blank") : i == 2 ? QStringLiteral("advanced") : QStringLiteral("basic");
}
QString NewProjectSheet::fullPath() const
{
    return QDir(m_parentDir).filePath(sanitizeProjectName(m_title->text()));
}
