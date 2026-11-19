#include "ExportPanel.h"

#include "EbookReader.h"
#include "ProjectModel.h"
#include "Theme.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyle>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <functional>

namespace {

QColor mixTo(const QColor& a, const QColor& b, qreal t) {
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, 1.0);
}

// O poço onde a folha fica: um tom abaixo do painel.
QColor wellColor() {
    const QColor panel = Theme::toColor(Theme::panelBackground());
    const bool dark = qGray(panel.rgb()) < 128;
    return mixTo(panel, QColor(0, 0, 0), dark ? 0.35 : 0.08);
}

// Chave liga/desliga (a mesma cara da das Configurações).
class Switch : public QCheckBox {
public:
    explicit Switch(QWidget* parent = nullptr) : QCheckBox(parent) {
        setCursor(Qt::PointingHandCursor);
        setFixedSize(30, 17);
    }
protected:
    bool hitButton(const QPoint&) const override { return true; }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const bool on = isChecked();
        QColor track = Theme::toColor(on ? Theme::accentDefault() : Theme::borderStrong());
        QColor knob = on ? QColor(Qt::white) : Theme::toColor(Theme::textPrimary());
        if (!isEnabled()) { track.setAlphaF(track.alphaF() * 0.4); knob.setAlphaF(0.5); }
        p.setPen(Qt::NoPen);
        p.setBrush(track);
        const QRectF r(0.5, 0.5, width() - 1.0, height() - 1.0);
        p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
        const qreal d = height() - 6.0;
        p.setBrush(knob);
        p.drawEllipse(QRectF(on ? width() - 3.0 - d : 3.0, 3.0, d, d));
    }
};

} // namespace

// ============================================================ A folha
// Desenha a página da prévia no centro do poço, com sombra; no EPUB, a tela
// dentro de um e-reader.
class ExportPreviewView : public QWidget {
public:
    explicit ExportPreviewView(QWidget* parent) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMinimumSize(300, 300);
    }
    void setPage(const Exporter::PagePreview* p, int index) {
        m_p = p;
        m_index = index;
        m_cache = QImage();
        update();
    }
    QString emptyText;
    // A roda do mouse folheia: cada entalhe é uma página (o touchpad soma os
    // pedacinhos até dar um entalhe).
    std::function<void(int)> onStep;

protected:
    void resizeEvent(QResizeEvent*) override { m_cache = QImage(); }
    void wheelEvent(QWheelEvent* e) override {
        e->accept();
        if (!onStep) return;
        m_wheelAcc += e->angleDelta().y();
        while (qAbs(m_wheelAcc) >= 120) {
            const int dir = m_wheelAcc > 0 ? -1 : 1;
            m_wheelAcc -= (m_wheelAcc > 0 ? 120 : -120);
            onStep(dir);
        }
    }
    void paintEvent(QPaintEvent*) override {
        QPainter g(this);
        g.setRenderHint(QPainter::Antialiasing);
        g.setRenderHint(QPainter::SmoothPixmapTransform);
        g.fillRect(rect(), wellColor());
        if (!m_p || m_p->isEmpty()) {
            g.setPen(Theme::toColor(Theme::textMuted()));
            QFont f = font();
            f.setPixelSize(13);
            g.setFont(f);
            g.drawText(rect(), Qt::AlignCenter, emptyText);
            return;
        }
        const bool reader = (m_p->kind == Exporter::PagePreview::Reader);
        const QSizeF page = m_p->pagePx;
        // e-reader: a moldura soma em volta da tela
        const qreal padX = reader ? 16 : 0, padTop = reader ? 22 : 0, padBot = reader ? 34 : 0;
        const QRectF avail = QRectF(rect()).adjusted(32, 26, -32, -10);
        const qreal outerW = page.width() + 2 * padX, outerH = page.height() + padTop + padBot;
        const qreal k = qMin(avail.width() / outerW, avail.height() / outerH);
        const QSizeF outer(outerW * k, outerH * k);
        const QRectF box(avail.center().x() - outer.width() / 2, avail.center().y() - outer.height() / 2,
                         outer.width(), outer.height());
        const QRectF pageRect(box.left() + padX * k, box.top() + padTop * k, page.width() * k, page.height() * k);

        const int w = qRound(pageRect.width());
        const qreal dpr = devicePixelRatioF();
        if (m_cache.isNull() || m_cacheW != w || !qFuzzyCompare(m_cacheDpr, dpr)) {
            m_cache = Exporter::renderPreviewPage(*m_p, m_index, w, dpr);
            m_cacheW = w;
            m_cacheDpr = dpr;
        }
        // sombra macia
        const QRectF shadowOf = reader ? box : pageRect;
        const qreal rr = reader ? 26 * k : 0;
        g.setPen(Qt::NoPen);
        for (int i = 8; i >= 1; --i) {
            g.setBrush(QColor(0, 0, 0, 7));
            g.drawRoundedRect(shadowOf.adjusted(-i, -i + 6, i, i + 6), rr + i, rr + i);
        }
        if (reader) {
            g.setBrush(QColor(0x15, 0x15, 0x13));
            g.drawRoundedRect(box, rr, rr);
            QPainterPath clip;
            clip.addRoundedRect(pageRect, 6 * k, 6 * k);
            g.save();
            g.setClipPath(clip);
            g.drawImage(pageRect.topLeft(), m_cache);
            g.restore();
        } else {
            g.drawImage(pageRect.topLeft(), m_cache);
        }
    }

private:
    const Exporter::PagePreview* m_p = nullptr;
    int m_index = 0;
    QImage m_cache;
    int m_cacheW = 0;
    qreal m_cacheDpr = 1.0;
    int m_wheelAcc = 0;
};

// ============================================================ Painel

ExportPanel::ExportPanel(ProjectModel* model, QWidget* parent)
    : QDialog(parent), m_model(model) {
    setObjectName(QStringLiteral("exportPanel"));
    setWindowTitle(tr("Exportar"));
    setModal(true);
    {
        int w = 1040, h = 720;
        if (parent) { w = qMin(w, parent->width() - 60); h = qMin(h, parent->height() - 60); }
        resize(qMax(820, w), qMax(560, h));
    }
    const bool screenplay = m_model && m_model->isScreenplay();
    m_format = QSettings().value(QStringLiteral("ui/exportFormat"), QStringLiteral("pdf")).toString();

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── A folha ──
    auto* well = new QWidget(this);
    well->setObjectName(QStringLiteral("exportWell"));
    auto* wl = new QVBoxLayout(well);
    wl->setContentsMargins(0, 0, 0, 12);
    wl->setSpacing(10);
    // EPUB: a barra do leitor (aparelho, papel, tamanho, letra)
    m_ebookTools = new QWidget(well);
    m_ebookTools->setObjectName(QStringLiteral("exportEbookTools"));
    {
        auto* th = new QHBoxLayout(m_ebookTools);
        th->setContentsMargins(16, 10, 16, 10);
        m_ebookBar = new EbookLookBar(EbookLookBar::DevicePart | EbookLookBar::PaperPart
                                      | EbookLookBar::SizePart | EbookLookBar::FontPart, m_ebookTools);
        th->addWidget(m_ebookBar);
        th->addStretch(1);
        connect(m_ebookBar, &EbookLookBar::lookChanged, this, [this]() { schedulePreview(); });
    }
    wl->addWidget(m_ebookTools);
    m_view = new ExportPreviewView(well);
    m_view->emptyText = tr("Escolha o que vai sair.");
    m_view->onStep = [this](int d) { stepPage(d); };
    wl->addWidget(m_view, 1);
    m_ebookView = new EbookView(well);
    m_ebookView->setMaxScale(1.0);
    m_ebookView->setEmptyText(tr("Escolha o que vai sair."));
    connect(m_ebookView, &EbookView::screenChanged, this, [this]() { refreshNav(); refreshStrip(); });
    wl->addWidget(m_ebookView, 1);
    auto* nav = new QHBoxLayout;
    nav->setSpacing(12);
    nav->addStretch(1);
    m_prevBtn = new QToolButton(well);
    m_prevBtn->setObjectName(QStringLiteral("exportNavBtn"));
    m_prevBtn->setText(QStringLiteral("‹"));
    m_prevBtn->setCursor(Qt::PointingHandCursor);
    m_prevBtn->setFixedSize(28, 28);
    m_nextBtn = new QToolButton(well);
    m_nextBtn->setObjectName(QStringLiteral("exportNavBtn"));
    m_nextBtn->setText(QStringLiteral("›"));
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    m_nextBtn->setFixedSize(28, 28);
    m_navLabel = new QLabel(well);
    m_navLabel->setObjectName(QStringLiteral("exportNavLabel"));
    connect(m_prevBtn, &QToolButton::clicked, this, [this]() { stepPage(-1); });
    connect(m_nextBtn, &QToolButton::clicked, this, [this]() { stepPage(1); });
    nav->addWidget(m_prevBtn);
    nav->addWidget(m_navLabel);
    nav->addWidget(m_nextBtn);
    nav->addStretch(1);
    wl->addLayout(nav);
    // EPUB: a trilha das partes (capa, sumário e a parte da tela, que abre a
    // lista de todas; no máximo 4 pílulas por linha)
    m_ebookStrip = new QWidget(well);
    {
        auto* sh = new QHBoxLayout(m_ebookStrip);
        sh->setContentsMargins(16, 0, 16, 0);
        sh->setSpacing(6);
        sh->addStretch(1);
        auto chip = [this, sh](const QString& text) {
            auto* b = new QPushButton(text, m_ebookStrip);
            b->setObjectName(QStringLiteral("exportChip"));
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            sh->addWidget(b);
            return b;
        };
        m_stripCover = chip(tr("Capa"));
        m_stripToc = chip(tr("Sumário"));
        m_stripPart = chip(QString());
        connect(m_stripCover, &QPushButton::clicked, this, [this]() { m_ebookView->setScreen(0); refreshStrip(); });
        connect(m_stripToc, &QPushButton::clicked, this, [this]() {
            if (m_ebook && !m_ebook->sections.isEmpty()) m_ebookView->setScreen(m_ebook->screenOf(0));
            refreshStrip();
        });
        connect(m_stripPart, &QPushButton::clicked, this, [this]() {
            refreshStrip();
            if (!m_ebook) return;
            QMenu menu(this);
            const int cur = m_ebook->sectionAt(m_ebookView->screen());
            for (int i = 1; i < m_ebook->sections.size(); ++i) {
                const auto& s = m_ebook->sections.at(i);
                QAction* a = menu.addAction(s.kind == Exporter::EbookSection::Chapter
                    ? QStringLiteral("%1. %2").arg(s.number).arg(s.name) : s.name);
                a->setCheckable(true);
                a->setChecked(i == cur);
                connect(a, &QAction::triggered, this, [this, i]() { m_ebookView->setScreen(m_ebook->screenOf(i)); });
            }
            menu.exec(m_stripPart->mapToGlobal(QPoint(0, m_stripPart->height() + 4)));
        });
        sh->addStretch(1);
    }
    wl->addWidget(m_ebookStrip);
    root->addWidget(well, 1);

    // ── A coluna ──
    auto* side = new QWidget(this);
    side->setObjectName(QStringLiteral("exportSide"));
    side->setFixedWidth(300);
    auto* sl = new QVBoxLayout(side);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(0);
    root->addWidget(side);

    auto* head = new QHBoxLayout;
    head->setContentsMargins(20, 14, 12, 4);
    auto* title = new QLabel(tr("Exportar"), side);
    title->setObjectName(QStringLiteral("exportTitle"));
    auto* closeBtn = new QPushButton(QStringLiteral("✕"), side);
    closeBtn->setObjectName(QStringLiteral("exportClose"));
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFixedSize(26, 26);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    head->addWidget(title);
    head->addStretch(1);
    head->addWidget(closeBtn);
    sl->addLayout(head);

    bool firstGroup = true;
    auto group = [&](const QString& caps) {
        auto* g = new QWidget(side);
        g->setObjectName(QStringLiteral("exportGroup"));
        g->setProperty("first", firstGroup);
        firstGroup = false;
        auto* v = new QVBoxLayout(g);
        v->setContentsMargins(20, 12, 20, 12);
        v->setSpacing(8);
        if (!caps.isEmpty()) {
            auto* l = new QLabel(caps.toUpper(), g);
            l->setObjectName(QStringLiteral("exportCaps"));
            v->addWidget(l);
        }
        sl->addWidget(g);
        return v;
    };
    auto tabsRow = [&](QWidget* parentW) {
        auto* w = new QWidget(parentW);
        w->setObjectName(QStringLiteral("exportTabs"));
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(2, 2, 2, 2);
        h->setSpacing(2);
        return std::make_pair(w, h);
    };
    auto tabBtn = [](const QString& text, QWidget* parentW) {
        auto* b = new QPushButton(text, parentW);
        b->setObjectName(QStringLiteral("exportTab"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        return b;
    };
    auto switchRow = [&](QWidget* parentW, const QString& text, QCheckBox*& sw, QLabel** labelOut = nullptr) {
        auto* row = new QWidget(parentW);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 2, 0, 2);
        auto* l = new QLabel(text, row);
        l->setObjectName(QStringLiteral("exportRowLabel"));
        sw = new Switch(row);
        h->addWidget(l, 1);
        h->addWidget(sw);
        if (labelOut) *labelOut = l;
        return row;
    };

    // Formato. Livro: os quatro. Roteiro: PDF, DOCX e Fountain na frente, ODT
    // e EPUB no "Mais" (no máximo 4 por linha).
    {
        QVBoxLayout* v = group(tr("Formato"));
        auto [w, h] = tabsRow(side);
        const QStringList fronts = screenplay
            ? QStringList{ QStringLiteral("pdf"), QStringLiteral("docx"), QStringLiteral("fountain") }
            : QStringList{ QStringLiteral("pdf"), QStringLiteral("docx"), QStringLiteral("epub"), QStringLiteral("odt") };
        for (const QString& f : fronts) {
            auto* b = tabBtn(f == QLatin1String("fountain") ? QStringLiteral("Fountain") : f.toUpper(), w);
            b->setProperty("fmt", f);
            connect(b, &QPushButton::clicked, this, [this, f]() { selectFormat(f); });
            m_formatBtns.append(b);
            h->addWidget(b);
        }
        if (screenplay) {
            auto* more = tabBtn(tr("Mais") + QStringLiteral(" ▾"), w);
            more->setProperty("fmt", QStringLiteral("more"));
            auto* menu = new QMenu(more);
            for (const QString& f : { QStringLiteral("odt"), QStringLiteral("epub") })
                menu->addAction(f.toUpper(), this, [this, f]() { selectFormat(f); });
            connect(more, &QPushButton::clicked, this, [this, more, menu]() {
                more->setChecked(m_format == QLatin1String("odt") || m_format == QLatin1String("epub"));
                menu->popup(more->mapToGlobal(QPoint(0, more->height())));
            });
            m_formatBtns.append(more);
            h->addWidget(more);
        }
        v->addWidget(w);
    }

    // O que vai: um seletor; a lista abre só quando pedida.
    {
        QVBoxLayout* v = group(tr("O que vai"));
        m_pickBtn = new QPushButton(side);
        m_pickBtn->setObjectName(QStringLiteral("exportPick"));
        m_pickBtn->setCursor(Qt::PointingHandCursor);
        m_pickBtn->setMinimumHeight(34);
        auto* ph = new QHBoxLayout(m_pickBtn);
        ph->setContentsMargins(10, 0, 10, 0);
        m_pickText = new QLabel(m_pickBtn);
        m_pickText->setObjectName(QStringLiteral("exportPickText"));
        m_pickText->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_pickCaret = new QLabel(QStringLiteral("▾"), m_pickBtn);
        m_pickCaret->setObjectName(QStringLiteral("exportPickCaret"));
        m_pickCaret->setAttribute(Qt::WA_TransparentForMouseEvents);
        ph->addWidget(m_pickText, 1);
        ph->addWidget(m_pickCaret);
        connect(m_pickBtn, &QPushButton::clicked, this, [this]() { setListOpen(!m_listOpen); });
        v->addWidget(m_pickBtn);

        m_listBox = new QWidget(side);
        auto* lv = new QVBoxLayout(m_listBox);
        lv->setContentsMargins(0, 0, 0, 0);
        lv->setSpacing(4);
        auto* links = new QHBoxLayout;
        links->setSpacing(10);
        auto* all = new QPushButton(tr("Tudo"), m_listBox);
        auto* none = new QPushButton(tr("Nada"), m_listBox);
        for (QPushButton* b : { all, none }) { b->setObjectName(QStringLiteral("exportLink")); b->setCursor(Qt::PointingHandCursor); }
        connect(all, &QPushButton::clicked, this, [this]() { setAllChecked(true); });
        connect(none, &QPushButton::clicked, this, [this]() { setAllChecked(false); });
        m_countLabel = new QLabel(m_listBox);
        m_countLabel->setObjectName(QStringLiteral("exportHint"));
        links->addWidget(all);
        links->addWidget(none);
        links->addStretch(1);
        links->addWidget(m_countLabel);
        lv->addLayout(links);
        m_tree = new QTreeWidget(m_listBox);
        m_tree->setObjectName(QStringLiteral("exportTree"));
        m_tree->setHeaderHidden(true);
        m_tree->setColumnCount(1);
        m_tree->setIndentation(14);
        m_tree->setRootIsDecorated(false);
        m_tree->setUniformRowHeights(true);
        // Aberta, a lista ocupa o que sobra da coluna (setListOpen).
        m_tree->setMinimumHeight(110);
        m_tree->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem*, int) {
            if (!m_building) recomputeCount();
        });
        lv->addWidget(m_tree);
        auto* note = new QLabel(tr("Cena com variações sai com a variação primária."), m_listBox);
        note->setObjectName(QStringLiteral("exportHint"));
        note->setWordWrap(true);
        lv->addWidget(note);
        m_listBox->hide();
        v->addWidget(m_listBox, 1);
        m_whatGroup = v->parentWidget();
    }

    // Manuscrito: um arquivo ou um por capítulo; marcadores.
    {
        QVBoxLayout* v = group(tr("Manuscrito"));
        auto [w, h] = tabsRow(side);
        m_singleBtn = tabBtn(tr("Um arquivo"), w);
        m_separateBtn = tabBtn(tr("Um por capítulo"), w);
        connect(m_singleBtn, &QPushButton::clicked, this, [this]() { setSingleDocument(true); });
        connect(m_separateBtn, &QPushButton::clicked, this, [this]() { setSingleDocument(false); });
        h->addWidget(m_singleBtn);
        h->addWidget(m_separateBtn);
        v->addWidget(w);
        m_markersRow = switchRow(side, tr("Levar os marcadores"), m_markersSwitch);
        m_markersSwitch->setChecked(true);
        m_markersSwitch->setToolTip(tr("Os trechos marcados no texto saem grifados no arquivo."));
        connect(m_markersSwitch, &QCheckBox::toggled, this, [this]() { refreshSummary(); schedulePreview(); });
        v->addWidget(m_markersRow);
        // EPUB: a letra capitular no começo de cada capítulo
        m_dropCapRow = switchRow(side, tr("Letra capitular"), m_dropCapSwitch);
        m_dropCapSwitch->setChecked(QSettings().value(QStringLiteral("ui/exportDropCap"), true).toBool());
        m_dropCapSwitch->setToolTip(tr("A primeira letra de cada capítulo grande, ocupando três linhas, como nos livros impressos."));
        connect(m_dropCapSwitch, &QCheckBox::toggled, this, [this](bool on) {
            QSettings().setValue(QStringLiteral("ui/exportDropCap"), on);
            schedulePreview();
        });
        v->addWidget(m_dropCapRow);
    }

    // EPUB: a sala de leitura, com o livro inteiro em tela cheia
    {
        QVBoxLayout* v = group(QString());
        m_bigGroup = v->parentWidget();
        auto* big = new QPushButton(side);
        big->setObjectName(QStringLiteral("exportBig"));
        big->setCursor(Qt::PointingHandCursor);
        big->setMinimumHeight(48);
        auto* bh = new QVBoxLayout(big);
        bh->setContentsMargins(12, 7, 12, 7);
        bh->setSpacing(1);
        auto* t1 = new QLabel(tr("Abrir a prévia grande"), big);
        t1->setObjectName(QStringLiteral("exportBigTitle"));
        auto* t2 = new QLabel(tr("ler o livro inteiro como no Kindle"), big);
        t2->setObjectName(QStringLiteral("exportHint"));
        for (QLabel* l : { t1, t2 }) l->setAttribute(Qt::WA_TransparentForMouseEvents);
        bh->addWidget(t1);
        bh->addWidget(t2);
        connect(big, &QPushButton::clicked, this, &ExportPanel::openReadingRoom);
        v->addWidget(big);
    }

    // Formato de editora (livro) ou numerar as cenas (roteiro).
    {
        QVBoxLayout* v = group(QString());
        m_editorGroup = v->parentWidget();
        v->addWidget(switchRow(side, screenplay ? tr("Numerar as cenas") : tr("Formato de editora (Shunn)"),
                               m_editorSwitch, &m_editorLabel));
        m_editorSwitch->setToolTip(screenplay
            ? tr("Número de cada cena nas margens, como no roteiro de filmagem. A numeração segue a ordem do roteiro inteiro.")
            : tr("Formato que editoras e revistas esperam receber (padrão Shunn): Courier 12, entrelinha dupla, "
                 "margens de 1 polegada, página de rosto com contato e contagem de palavras, cabeçalho corrido "
                 "e \"#\" como quebra de cena."));
        connect(m_editorSwitch, &QCheckBox::toggled, this, [this]() { m_page = 0; refreshOptions(); schedulePreview(); });
        m_editorHint = new QLabel(side);
        m_editorHint->setObjectName(QStringLiteral("exportHint"));
        m_editorHint->setWordWrap(true);
        v->addWidget(m_editorHint);
        m_authorBtn = new QPushButton(tr("Dados do autor…"), side);
        m_authorBtn->setObjectName(QStringLiteral("exportLink"));
        m_authorBtn->setCursor(Qt::PointingHandCursor);
        connect(m_authorBtn, &QPushButton::clicked, this, &ExportPanel::editSubmissionData);
        auto* ab = new QHBoxLayout;
        ab->addWidget(m_authorBtn);
        ab->addStretch(1);
        v->addLayout(ab);
    }

    sl->addStretch(1);
    m_sideLayout = sl;

    // ── O pé: o que vai sair e o Exportar ──
    auto* end = new QWidget(side);
    end->setObjectName(QStringLiteral("exportEnd"));
    auto* ev = new QVBoxLayout(end);
    ev->setContentsMargins(20, 14, 20, 16);
    ev->setSpacing(10);
    m_summary = new QLabel(end);
    m_summary->setObjectName(QStringLiteral("exportSummary"));
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::RichText);
    ev->addWidget(m_summary);
    auto* btns = new QHBoxLayout;
    btns->setSpacing(8);
    auto* cancel = new QPushButton(tr("Cancelar"), end);
    cancel->setObjectName(QStringLiteral("exportCancel"));
    cancel->setCursor(Qt::PointingHandCursor);
    m_exportBtn = new QPushButton(tr("Exportar"), end);
    m_exportBtn->setObjectName(QStringLiteral("exportConfirm"));
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    m_exportBtn->setDefault(true);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_exportBtn, &QPushButton::clicked, this, &ExportPanel::onExportClicked);
    btns->addWidget(cancel);
    btns->addWidget(m_exportBtn, 1);
    ev->addLayout(btns);
    m_bibleBtn = new QPushButton(tr("Exportar a bíblia do universo…"), end);
    m_bibleBtn->setObjectName(QStringLiteral("exportLink"));
    m_bibleBtn->setCursor(Qt::PointingHandCursor);
    connect(m_bibleBtn, &QPushButton::clicked, this, [this]() {
        // Fecha antes de emitir: quem ouve abre progresso e diálogo de salvar,
        // e empilhar isso por cima deste modal fica confuso.
        const Exporter::Format fmt = currentFormat();
        accept();
        emit bibleRequested(fmt);
    });
    auto* bb = new QHBoxLayout;
    bb->addWidget(m_bibleBtn);
    bb->addStretch(1);
    ev->addLayout(bb);
    sl->addWidget(end);

    m_submission = Exporter::SubmissionInfo::load();
    // Byline em branco cai no autor do projeto: quem já preencheu os dados do
    // projeto não precisa redigitar.
    if (m_submission.byline.trimmed().isEmpty() && m_model)
        m_submission.byline = m_model->projectAuthor();

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(120);
    connect(m_previewTimer, &QTimer::timeout, this, &ExportPanel::rebuildPreview);

    buildTree();
    applyTheme();
    selectFormat(m_format);
    setSingleDocument(true);
    recomputeCount();
}

ExportPanel::~ExportPanel() = default;

void ExportPanel::setPreviewSource(const QString& projectRoot, const Exporter::DocStyle& style) {
    m_root = projectRoot;
    m_style = style;
    m_exporter = std::make_unique<Exporter>(m_model, projectRoot, style);
    refreshSummary();   // as palavras (chapterWords) chegam junto com a fonte
    schedulePreview();
}

Exporter::Format ExportPanel::currentFormat() const {
    if (m_format == QLatin1String("pdf"))  return Exporter::Format::Pdf;
    if (m_format == QLatin1String("epub")) return Exporter::Format::Epub;
    if (m_format == QLatin1String("docx")) return Exporter::Format::Docx;
    if (m_format == QLatin1String("fountain")) return Exporter::Format::Fountain;
    return Exporter::Format::Odt;
}

void ExportPanel::selectFormat(const QString& f) {
    const bool screenplay = m_model && m_model->isScreenplay();
    const QStringList valid = screenplay
        ? QStringList{ QStringLiteral("pdf"), QStringLiteral("docx"), QStringLiteral("fountain"), QStringLiteral("odt"), QStringLiteral("epub") }
        : QStringList{ QStringLiteral("pdf"), QStringLiteral("docx"), QStringLiteral("epub"), QStringLiteral("odt") };
    m_format = valid.contains(f) ? f : QStringLiteral("pdf");
    QSettings().setValue(QStringLiteral("ui/exportFormat"), m_format);
    for (QPushButton* b : std::as_const(m_formatBtns)) {
        const QString bf = b->property("fmt").toString();
        if (bf == QLatin1String("more")) {
            const bool inMore = (m_format == QLatin1String("odt") || m_format == QLatin1String("epub"));
            b->setChecked(inMore);
            b->setText((inMore ? m_format.toUpper() : tr("Mais")) + QStringLiteral(" ▾"));
        } else {
            b->setChecked(bf == m_format);
        }
    }
    m_page = 0;
    refreshOptions();
    recomputeCount();
}

void ExportPanel::setSingleDocument(bool single) {
    m_single = single;
    if (m_singleBtn) m_singleBtn->setChecked(single);
    if (m_separateBtn) m_separateBtn->setChecked(!single);
    m_page = 0;
    refreshOptions();
    refreshSummary();
    schedulePreview();
}

void ExportPanel::refreshOptions() {
    const bool screenplay = m_model && m_model->isScreenplay();
    const bool paged = (m_format != QLatin1String("epub"));
    const bool fountain = (m_format == QLatin1String("fountain"));

    // EPUB: a prévia vira o leitor, com a barra e a trilha; a capitular e a
    // prévia grande só valem nele.
    const bool epub = !paged;
    if (m_ebookTools) m_ebookTools->setVisible(epub);
    if (m_ebookStrip) m_ebookStrip->setVisible(epub);
    if (m_view) m_view->setVisible(!epub);
    if (m_ebookView) m_ebookView->setVisible(epub);
    if (m_dropCapRow) m_dropCapRow->setVisible(epub);
    if (m_bigGroup) m_bigGroup->setVisible(epub);

    // EPUB sai sempre num arquivo só; Fountain não leva marcador.
    if (m_singleBtn) m_singleBtn->setEnabled(paged);
    if (m_separateBtn) m_separateBtn->setEnabled(paged);
    if (m_markersSwitch) m_markersSwitch->setEnabled(!fountain);
    if (m_bibleBtn) {
        m_bibleBtn->setEnabled(paged && !fountain);
        m_bibleBtn->setToolTip(paged && !fountain
            ? tr("Um documento só com gavetas, fichas, vínculos, glossário, territórios, sistemas do mundo "
                 "e locais do mapa, pra quem precisa consultar o universo sem abrir o app.")
            : tr("A bíblia do universo sai em PDF, DOCX ou ODT."));
    }

    if (screenplay) {
        const bool on = paged && !fountain;
        m_editorGroup->setVisible(true);
        m_editorSwitch->setEnabled(on);
        if (!on) { const QSignalBlocker b(m_editorSwitch); m_editorSwitch->setChecked(false); }
        m_authorBtn->setVisible(paged);
        m_editorHint->setText(fountain
            ? tr("Fountain é o roteiro em texto puro, que abre em outros programas de roteiro. Leva só o "
                 "manuscrito; as gavetas ficam de fora.")
            : paged
            ? tr("Roteiro sai no formato da indústria: Courier 12, página Carta, página de rosto e número de "
                 "página. O contato da página de rosto vem dos dados do autor.")
            : QString());
        m_editorHint->setVisible(!m_editorHint->text().isEmpty());
        return;
    }

    // No EPUB o formato de editora não existe (o texto reflui): some o grupo.
    m_editorGroup->setVisible(paged);
    const bool usable = paged && m_single;
    m_editorSwitch->setEnabled(usable);
    if (!usable) { const QSignalBlocker b(m_editorSwitch); m_editorSwitch->setChecked(false); }
    const bool on = m_editorSwitch->isChecked();
    m_authorBtn->setVisible(on);
    QString hint;
    bool warn = false;
    if (!paged)
        hint = tr("Não vale no EPUB: o texto reflui, não tem página fixa.");
    else if (!m_single)
        hint = tr("Pede um arquivo só: as páginas são numeradas do manuscrito inteiro.");
    else if (on && m_format == QLatin1String("odt"))
        { hint = tr("Em ODT o cabeçalho corrido não sai; pra mandar de verdade, use DOCX ou PDF."); warn = true; }
    else if (on && m_submission.legalName.trimmed().isEmpty())
        { hint = tr("Falta preencher os dados do autor."); warn = true; }
    else
        hint = tr("Courier, entrelinha dupla e página de rosto com seu contato.");
    m_editorHint->setText(hint);
    m_editorHint->setProperty("warn", warn);
    m_editorHint->style()->unpolish(m_editorHint);
    m_editorHint->style()->polish(m_editorHint);
}

void ExportPanel::editSubmissionData() {
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Dados do autor"));
    auto* form = new QFormLayout(&dlg);

    auto* legal = new QLineEdit(m_submission.legalName, &dlg);
    legal->setPlaceholderText(tr("Nome que assina contrato"));
    auto* address = new QTextEdit(m_submission.address, &dlg);
    address->setAcceptRichText(false);
    address->setFixedHeight(70);
    auto* contact = new QLineEdit(m_submission.contact, &dlg);
    contact->setPlaceholderText(tr("E-mail e/ou telefone"));
    auto* byline = new QLineEdit(m_submission.byline, &dlg);
    byline->setPlaceholderText(tr("Nome de publicação, se for diferente"));
    auto* shortTitle = new QLineEdit(m_submission.titleShort, &dlg);
    shortTitle->setPlaceholderText(tr("Uma ou duas palavras, para o cabeçalho"));

    form->addRow(tr("Nome legal:"), legal);
    form->addRow(tr("Endereço:"), address);
    form->addRow(tr("Contato:"), contact);
    form->addRow(tr("Assinar como:"), byline);
    form->addRow(tr("Título curto:"), shortTitle);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(box);

    if (dlg.exec() != QDialog::Accepted) return;
    m_submission.legalName  = legal->text();
    m_submission.address    = address->toPlainText();
    m_submission.contact    = contact->text();
    m_submission.byline     = byline->text();
    m_submission.titleShort = shortTitle->text();
    // Grava na hora: é dado do AUTOR, não do projeto — quem preenche uma vez
    // não deveria digitar de novo no livro seguinte.
    m_submission.save();
    refreshOptions();
    schedulePreview();
}

void ExportPanel::buildTree() {
    m_building = true;
    m_tree->clear();
    m_totalLeaves = 0;

    const auto groupFlags = Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsAutoTristate;
    const auto leafFlags = Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;

    auto styleHeader = [](QTreeWidgetItem* it) {
        it->setData(0, KindRole, KindHeader);
        it->setFlags(Qt::ItemIsEnabled);
        QFont f = it->font(0);
        f.setBold(true);
        f.setPointSizeF(f.pointSizeF() - 1.5);
        it->setFont(0, f);
        it->setForeground(0, QColor(Theme::textMuted()));
    };

    // Já vem marcado o primeiro manuscrito inteiro: a folha abre mostrando o
    // livro, e quem quer outra coisa desmarca.
    bool preselected = false;

    auto* msHeader = new QTreeWidgetItem(m_tree);
    msHeader->setText(0, tr("MANUSCRITOS"));
    styleHeader(msHeader);
    for (const Manuscript& ms : m_model->manuscripts()) {
        const QList<const Chapter*> chaps = m_model->orderedChaptersForManuscript(ms.id);
        if (chaps.isEmpty()) continue;
        const bool pick = !preselected;
        preselected = true;
        auto* msItem = new QTreeWidgetItem(msHeader);
        msItem->setText(0, ms.title.isEmpty() ? tr("Manuscrito") : ms.title);
        msItem->setData(0, KindRole, KindGroup);
        msItem->setFlags(groupFlags);
        msItem->setCheckState(0, pick ? Qt::Checked : Qt::Unchecked);
        for (int ci = 0; ci < chaps.size(); ++ci) {
            const Chapter* ch = chaps.at(ci);
            auto* c = new QTreeWidgetItem(msItem);
            const QString num = QString::number(ci + 1).rightJustified(2, QLatin1Char('0'));
            c->setText(0, QStringLiteral("%1  %2").arg(num,
                ch->title.isEmpty() ? m_model->chapterDisplayLabel(*ch) : ch->title));
            c->setData(0, IdRole, ch->id);
            c->setData(0, MsRole, ms.id);
            c->setData(0, KindRole, KindChapter);
            c->setFlags(leafFlags);
            c->setCheckState(0, pick ? Qt::Checked : Qt::Unchecked);
            ++m_totalLeaves;
        }
        msItem->setExpanded(true);
    }
    if (msHeader->childCount() == 0) delete msHeader;
    else msHeader->setExpanded(true);

    auto* gvHeader = new QTreeWidgetItem(m_tree);
    gvHeader->setText(0, tr("GAVETAS"));
    styleHeader(gvHeader);
    std::function<int(QTreeWidgetItem*, const Drawer&, const QString&)> addFolder =
        [&](QTreeWidgetItem* node, const Drawer& d, const QString& folderId) -> int {
            int leaves = 0;
            for (const DrawerItem& it : d.items) {
                if ((it.folderId.isEmpty() ? QString() : it.folderId) != folderId) continue;
                auto* leaf = new QTreeWidgetItem(node);
                leaf->setText(0, it.title.isEmpty() ? tr("Documento") : it.title);
                leaf->setData(0, IdRole, it.id);
                leaf->setData(0, KindRole, KindItem);
                leaf->setFlags(leafFlags);
                leaf->setCheckState(0, Qt::Unchecked);
                ++leaves;
                ++m_totalLeaves;
            }
            for (const Folder& f : d.folders) {
                if ((f.parentId.isEmpty() ? QString() : f.parentId) != folderId) continue;
                auto* sub = new QTreeWidgetItem(node);
                sub->setText(0, f.title);
                sub->setData(0, KindRole, KindGroup);
                sub->setFlags(groupFlags);
                sub->setCheckState(0, Qt::Unchecked);
                const int childLeaves = addFolder(sub, d, f.id);
                if (childLeaves == 0) { delete sub; }
                else { sub->setExpanded(false); leaves += childLeaves; }
            }
            return leaves;
        };
    for (const Drawer& d : m_model->drawers()) {
        auto* dItem = new QTreeWidgetItem(gvHeader);
        dItem->setText(0, d.title);
        dItem->setData(0, KindRole, KindGroup);
        dItem->setFlags(groupFlags);
        dItem->setCheckState(0, Qt::Unchecked);
        const int leaves = addFolder(dItem, d, QString());
        if (leaves == 0) delete dItem;
        else dItem->setExpanded(false);
    }
    if (gvHeader->childCount() == 0) delete gvHeader;
    else gvHeader->setExpanded(true);
    m_tree->setRootIsDecorated(true);

    m_building = false;
}

void ExportPanel::setAllChecked(bool checked) {
    m_building = true;
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        const int kind = (*it)->data(0, KindRole).toInt();
        if (kind == KindChapter || kind == KindItem)
            (*it)->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
        ++it;
    }
    m_building = false;
    recomputeCount();
}

int ExportPanel::selectedWords(QString* firstMs, int* firstMsWords) const {
    int total = 0, first = 0;
    QString firstId;
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        if ((*it)->data(0, KindRole).toInt() == KindChapter && (*it)->checkState(0) == Qt::Checked) {
            const int w = chapterWords ? chapterWords((*it)->data(0, IdRole).toString()) : 0;
            total += w;
            const QString ms = (*it)->data(0, MsRole).toString();
            if (firstId.isEmpty()) firstId = ms;
            if (ms == firstId) first += w;
        }
        ++it;
    }
    if (firstMs) *firstMs = firstId;
    if (firstMsWords) *firstMsWords = first;
    return total;
}

void ExportPanel::recomputeCount() {
    int selected = 0;
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        const int kind = (*it)->data(0, KindRole).toInt();
        if ((kind == KindChapter || kind == KindItem) && (*it)->checkState(0) == Qt::Checked) ++selected;
        ++it;
    }
    if (m_countLabel) m_countLabel->setText(QStringLiteral("%1 / %2").arg(selected).arg(m_totalLeaves));
    m_page = 0;
    refreshPickLabel();
    refreshSummary();
    schedulePreview();
}

void ExportPanel::setListOpen(bool open) {
    m_listOpen = open;
    if (m_listBox) m_listBox->setVisible(open);
    // Aberta, o grupo "O que vai" leva o espaço livre da coluna; fechada, o
    // vão volta pra baixo das opções.
    if (m_sideLayout && m_whatGroup) {
        m_sideLayout->setStretchFactor(m_whatGroup, open ? 1 : 0);
        const int spacer = m_sideLayout->count() - 2;   // o vão antes do pé
        if (spacer >= 0) m_sideLayout->setStretch(spacer, open ? 0 : 1);
    }
    if (m_pickCaret) m_pickCaret->setText(open ? QStringLiteral("▴") : QStringLiteral("▾"));
}

void ExportPanel::refreshPickLabel() {
    if (!m_pickText) return;
    const Exporter::Selection sel = currentSelection();
    const bool fountain = (m_format == QLatin1String("fountain"));
    const int items = fountain ? 0 : sel.itemIds.size();
    const int chs = sel.chapterIds.size();
    QString text;
    // O manuscrito inteiro e nada mais: diz o nome.
    if (chs > 0 && items == 0) {
        QSet<QString> ms;
        for (const QString& id : sel.chapterIds) if (const Chapter* c = m_model->findChapter(id)) ms.insert(c->manuscriptId);
        if (ms.size() == 1) {
            const QString msId = *ms.begin();
            if (m_model->orderedChaptersForManuscript(msId).size() == chs) {
                const QString t = m_model->manuscriptEffectiveTitle(msId);
                // Sem concordância com o título ("A Motorista inteiro" soava errado).
                text = tr("%1 (manuscrito)").arg(t.isEmpty() ? tr("Manuscrito") : t);
            }
        }
    }
    if (text.isEmpty()) {
        QStringList parts;
        if (chs) parts << (chs == 1 ? tr("1 capítulo") : tr("%1 capítulos").arg(chs));
        if (items) parts << (items == 1 ? tr("1 de gaveta") : tr("%1 de gaveta").arg(items));
        text = parts.isEmpty() ? tr("Nada escolhido") : parts.join(QStringLiteral(" + "));
    }
    m_pickText->setText(text);
}

void ExportPanel::refreshSummary() {
    if (!m_summary) return;
    const Exporter::Selection sel = currentSelection();
    const bool fountain = (m_format == QLatin1String("fountain"));
    const bool epub = (m_format == QLatin1String("epub"));
    const int items = fountain ? 0 : sel.itemIds.size();
    QSet<QString> ms;
    QString firstMs;
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        if ((*it)->data(0, KindRole).toInt() == KindChapter && (*it)->checkState(0) == Qt::Checked) {
            const QString id = (*it)->data(0, MsRole).toString();
            if (firstMs.isEmpty()) firstMs = id;
            ms.insert(id);
        }
        ++it;
    }
    const int chs = sel.chapterIds.size();
    const int files = epub ? ((chs || items) ? 1 : 0) : (m_single ? ms.size() : chs) + items;
    if (m_exportBtn) m_exportBtn->setEnabled(files > 0);
    if (files == 0) { m_summary->setText(tr("Nada escolhido ainda.")); return; }

    const QString ext = fountain ? QStringLiteral("fountain") : m_format;
    QString line1;
    if (files == 1) {
        QString name;
        if (chs) {
            name = m_model->manuscriptEffectiveTitle(firstMs);
            if (name.isEmpty()) name = tr("Manuscrito");
        } else {
            for (const Drawer& d : m_model->drawers())
                for (const DrawerItem& di : d.items)
                    if (name.isEmpty() && sel.itemIds.contains(di.id)) name = di.title;
            if (name.isEmpty()) name = tr("Documento");
        }
        line1 = tr("Sai %1").arg(QStringLiteral("<b>%1.%2</b>").arg(name.toHtmlEscaped(), ext));
    } else {
        line1 = tr("Saem %1 num .zip").arg(QStringLiteral("<b>%1</b>")
            .arg(tr("%1 arquivos %2").arg(files).arg(fountain ? QStringLiteral("Fountain") : m_format.toUpper())));
    }
    QStringList parts;
    const int words = selectedWords();
    if (chs) {
        QString c = chs == 1 ? tr("1 capítulo") : tr("%1 capítulos").arg(chs);
        if (words > 0) c += QStringLiteral(" · ") + tr("%1 palavras").arg(QLocale().toString(words));
        parts << c;
    }
    if (items) parts << (items == 1 ? tr("1 de gaveta") : tr("%1 de gaveta").arg(items));
    QString line2 = parts.join(QStringLiteral(" + "));
    if (!fountain && m_markersSwitch && !m_markersSwitch->isChecked()) line2 += QStringLiteral(" · ") + tr("sem marcadores");
    m_summary->setText(line1 + QStringLiteral("<br>") + line2.toHtmlEscaped());
}

Exporter::Selection ExportPanel::currentSelection() const {
    Exporter::Selection sel;
    sel.format = currentFormat();
    sel.manuscriptMode = m_single ? Exporter::ManuscriptMode::SingleDocument
                                  : Exporter::ManuscriptMode::SeparateChapters;
    sel.includeMarkers = !m_markersSwitch || m_markersSwitch->isChecked();
    const bool screenplay = m_model && m_model->isScreenplay();
    const bool switchOn = m_editorSwitch && m_editorSwitch->isChecked() && m_editorSwitch->isEnabled();
    sel.submissionFormat = !screenplay && switchOn;
    sel.sceneNumbers = screenplay && switchOn;
    sel.submission = m_submission;
    sel.dropCap = !m_dropCapSwitch || m_dropCapSwitch->isChecked();
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        if ((*it)->checkState(0) == Qt::Checked) {
            const int kind = (*it)->data(0, KindRole).toInt();
            const QString id = (*it)->data(0, IdRole).toString();
            if (kind == KindChapter) sel.chapterIds.insert(id);
            else if (kind == KindItem) sel.itemIds.insert(id);
        }
        ++it;
    }
    return sel;
}

void ExportPanel::onExportClicked() {
    const Exporter::Selection sel = currentSelection();
    if (sel.chapterIds.isEmpty() && sel.itemIds.isEmpty()) return;
    emit exportRequested(sel);
    accept();
}

// ============================================================ Prévia

void ExportPanel::schedulePreview() {
    if (m_previewTimer) m_previewTimer->start();
}

void ExportPanel::rebuildPreview() {
    if (!m_exporter) { m_preview = Exporter::PagePreview(); m_view->setPage(nullptr, 0); refreshNav(); return; }
    if (isEpub()) {
        // o livro inteiro no leitor; volta pro mesmo ponto (parte e fração)
        int sec = -1;
        qreal frac = 0;
        if (m_ebook && !m_ebook->isEmpty()) {
            const int s = m_ebookView->screen();
            sec = m_ebook->sectionAt(s);
            const int a = sec < 0 ? 0 : m_ebook->screenOf(sec);
            const int b = sec + 1 < m_ebook->sections.size() ? m_ebook->screenOf(sec + 1) - 1 : m_ebook->pages() - 1;
            frac = b > a ? qreal(s - a) / (b - a) : 0;
        }
        const Ebook::Look look = m_ebookBar->look();
        QApplication::setOverrideCursor(Qt::WaitCursor);
        m_ebook = std::make_shared<Exporter::EbookPreview>(m_exporter->buildEbookPreview(
            currentSelection(), Ebook::exporterLook(look), Ebook::contentSize(look.device)));
        QApplication::restoreOverrideCursor();
        m_ebookView->setBook(m_ebook, look);
        if (sec >= 0 && sec < m_ebook->sections.size()) {
            const int a = m_ebook->screenOf(sec);
            const int b = sec + 1 < m_ebook->sections.size() ? m_ebook->screenOf(sec + 1) - 1 : m_ebook->pages() - 1;
            m_ebookView->setScreen(a + qRound(frac * qMax(0, b - a)), false);
        }
        refreshNav();
        refreshStrip();
        return;
    }
    int firstWords = 0;
    selectedWords(nullptr, &firstWords);
    m_preview = m_exporter->buildPagePreview(currentSelection(), firstWords);
    m_page = qBound(0, m_page, qMax(0, m_preview.pages - 1));
    m_view->setPage(&m_preview, m_page);
    refreshNav();
}

void ExportPanel::stepPage(int delta) {
    if (isEpub()) { m_ebookView->step(delta); return; }
    if (m_preview.isEmpty()) return;
    m_page = qBound(0, m_page + delta, m_preview.pages - 1);
    m_view->setPage(&m_preview, m_page);
    refreshNav();
}

void ExportPanel::refreshNav() {
    if (isEpub()) {
        const bool has = m_ebook && !m_ebook->isEmpty();
        m_prevBtn->setVisible(has);
        m_nextBtn->setVisible(has);
        m_navLabel->setVisible(has);
        if (!has) return;
        const int s = m_ebookView->screen(), n = m_ebook->pages();
        m_prevBtn->setEnabled(s > 0);
        m_nextBtn->setEnabled(s < n - 1);
        m_navLabel->setText(tr("Tela %1 de %2 · o texto se ajusta à tela de cada leitor").arg(s + 1).arg(n));
        return;
    }
    const bool has = !m_preview.isEmpty();
    m_prevBtn->setVisible(has);
    m_nextBtn->setVisible(has);
    m_navLabel->setVisible(has);
    if (!has) return;
    m_prevBtn->setEnabled(m_page > 0);
    m_nextBtn->setEnabled(m_page < m_preview.pages - 1);
    using K = Exporter::PagePreview;
    QString t;
    if (m_preview.kind == K::Reader)
        t = tr("Tela %1 · o texto se ajusta ao Kindle ou ao celular").arg(m_page + 1);
    else if (m_preview.kind == K::PlainText)
        t = tr("Texto puro, abre em outros programas de roteiro");
    else if (m_preview.titlePage && m_page == 0)
        t = tr("Página de rosto");
    else if (m_preview.estimatedTotal > m_preview.pages)
        t = tr("Página %1 de ~%2").arg(m_page + 1).arg(QLocale().toString(m_preview.estimatedTotal));
    else
        t = tr("Página %1 de %2").arg(m_page + 1).arg(m_preview.estimatedTotal);
    m_navLabel->setText(t);
}

void ExportPanel::refreshStrip() {
    if (!m_ebookStrip) return;
    const bool has = m_ebook && !m_ebook->isEmpty();
    m_ebookStrip->setVisible(isEpub() && has);
    if (!has) return;
    const int sec = m_ebook->sectionAt(m_ebookView->screen());
    m_stripCover->setVisible(m_ebook->coverPages() > 0);
    m_stripCover->setChecked(sec < 0);
    m_stripToc->setText(m_ebook->tocLabel);
    m_stripToc->setChecked(sec == 0);
    QString part = tr("Capítulos");
    if (sec > 0) {
        const auto& s = m_ebook->sections.at(sec);
        part = s.kind == Exporter::EbookSection::Chapter ? QStringLiteral("%1. %2").arg(s.number).arg(s.name) : s.name;
        const QFontMetrics fm(m_stripPart->font());
        part = fm.elidedText(part, Qt::ElideRight, 200);
    }
    m_stripPart->setText(part + QStringLiteral("  ▾"));
    m_stripPart->setChecked(sec > 0);
}

void ExportPanel::openReadingRoom() {
    if (!m_exporter) return;
    const Exporter::Selection sel = currentSelection();
    if (sel.chapterIds.isEmpty() && sel.itemIds.isEmpty()) return;
    QString name;
    for (const QString& id : sel.chapterIds)
        if (const Chapter* c = m_model->findChapter(id)) { name = m_model->manuscriptEffectiveTitle(c->manuscriptId); break; }
    if (name.isEmpty()) name = tr("O que vai no EPUB");
    auto exporter = std::make_shared<Exporter>(m_model, m_root, m_style);
    auto* room = new EbookRoom(exporter, sel, name, QString(), this);
    room->setAttribute(Qt::WA_DeleteOnClose);
    if (QWidget* host = parentWidget()) room->setGeometry(host->window()->geometry());
    connect(room, &EbookRoom::exportRequested, this, [this](const Exporter::Selection& s) {
        emit exportRequested(s);
        accept();
    });
    // o que mudou lá (aparelho, papel, letra) volta pra prévia daqui
    connect(room, &QDialog::finished, this, [this]() {
        m_ebookBar->setLook(Ebook::Look::load());
        schedulePreview();
    });
    room->open();
}

void ExportPanel::applyTheme() {
    const QString panelBg   = Theme::panelBackground();
    const QString panelBd   = Theme::panelBorder();
    const QString txtPrim   = Theme::textPrimary();
    const QString txtMuted  = Theme::textMuted();
    const QString txtBright = Theme::textBright();
    const QString inputBg   = Theme::inputBackground();
    const QString subtleBd  = Theme::subtleBorder();
    const QString hover     = Theme::hoverOverlay();
    const QString hoverStr  = Theme::hoverStrong();
    const QString accent    = Theme::accentDefault();
    const QString well      = wellColor().name();

    setStyleSheet(Theme::qss(QStringLiteral(R"(
        #exportPanel { background-color: %1; }
        #exportPanel #exportWell { background: %11; }
        #exportPanel #exportSide { background: %1; border-left: 1px solid %7; }
        #exportPanel QLabel { color: %4; font-size: 13px; }
        #exportPanel QLabel#exportTitle { color: %6; font-family: 'Lora','Crimson Text',serif; font-size: 16px; font-weight: 600; }
        #exportPanel QLabel#exportCaps { color: %5; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
        #exportPanel QLabel#exportHint { color: %5; font-size: 11px; }
        #exportPanel QLabel#exportHint[warn="true"] { color: #d9a441; }
        #exportPanel QLabel#exportRowLabel { color: %4; font-size: 12px; }
        #exportPanel QLabel#exportNavLabel { color: %5; font-size: 12px; }
        #exportPanel QLabel#exportSummary { color: %5; font-size: 12px; }
        #exportPanel QLabel#exportPickText { color: %6; font-size: 12px; }
        #exportPanel QLabel#exportPickCaret { color: %5; font-size: 10px; }
        #exportPanel #exportGroup[first="false"] { border-top: 1px solid %7; }
        #exportPanel #exportEnd { border-top: 1px solid %7; }
        #exportPanel #exportTabs { background: %3; border-radius: @radius-control; }
        #exportPanel QPushButton#exportTab {
            background: transparent; color: %5; border: none; border-radius: @radius-item;
            padding: 6px 0; font-size: 11px; font-weight: bold;
        }
        #exportPanel QPushButton#exportTab:hover { color: %6; }
        #exportPanel QPushButton#exportTab:checked { background: %1; color: %6; }
        #exportPanel QPushButton#exportTab:disabled { color: %7; }
        #exportPanel QPushButton#exportPick {
            background: %3; border: 1px solid %2; border-radius: @radius-control; text-align: left;
        }
        #exportPanel QPushButton#exportPick:hover { border-color: %5; }
        #exportPanel QPushButton#exportClose {
            background: transparent; color: %5; border: none; font-size: 15px; border-radius: @radius-control;
        }
        #exportPanel QPushButton#exportClose:hover { background: %8; color: %6; }
        #exportPanel QPushButton#exportLink {
            background: transparent; color: %5; border: none; font-size: 12px; padding: 0; text-align: left;
        }
        #exportPanel QPushButton#exportLink:hover { color: %6; }
        #exportPanel QPushButton#exportLink:disabled { color: %7; }
        #exportPanel QPushButton#exportCancel {
            background: transparent; color: %4; border: none; padding: 8px 14px; border-radius: @radius-control; font-size: 12px;
        }
        #exportPanel QPushButton#exportCancel:hover { background: %8; color: %6; }
        #exportPanel QPushButton#exportConfirm {
            background: %10; color: #ffffff; border: none; padding: 8px 18px; border-radius: @radius-control;
            font-size: 12px; font-weight: bold;
        }
        #exportPanel QPushButton#exportConfirm:disabled { background: %8; color: %7; }
        #exportPanel QToolButton#exportNavBtn {
            background: transparent; color: %6; border: 1px solid %2; border-radius: 14px; font-size: 15px;
        }
        #exportPanel QToolButton#exportNavBtn:hover { background: %8; }
        #exportPanel QToolButton#exportNavBtn:disabled { color: %7; border-color: %7; }
        #exportPanel QTreeWidget#exportTree {
            background: transparent; border: none; outline: none; color: %4; font-size: 12px;
        }
        #exportPanel QTreeWidget#exportTree::item { padding: 2px 0; }
        #exportPanel QTreeWidget#exportTree::item:hover { background: %8; }
        #exportPanel QTreeWidget#exportTree::item:selected { background: transparent; color: %4; }
        #exportPanel QTreeWidget#exportTree::indicator {
            width: 13px; height: 13px; border-radius: 7px; border: 1px solid %5; background: transparent;
        }
        #exportPanel QTreeWidget#exportTree::indicator:checked { background: %10; border-color: %10; }
        #exportPanel QTreeWidget#exportTree::indicator:indeterminate { background: %9; border-color: %10; }
        #exportPanel #exportEbookTools { border-bottom: 1px solid %7; }
        #exportPanel QPushButton#exportChip {
            background: transparent; color: %5; border: 1px solid %2; border-radius: 12px; padding: 4px 11px; font-size: 11px;
        }
        #exportPanel QPushButton#exportChip:hover { color: %6; border-color: %5; }
        #exportPanel QPushButton#exportChip:checked { background: %9; border-color: %10; color: %6; }
        #exportPanel QPushButton#exportBig {
            background: transparent; border: 1px dashed %2; border-radius: @radius-control; text-align: left;
        }
        #exportPanel QPushButton#exportBig:hover { border-color: %10; }
        #exportPanel QLabel#exportBigTitle { color: %6; font-size: 12px; }
        #exportPanel QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
        #exportPanel QScrollBar::handle:vertical { background: %2; border-radius: 4px; min-height: 30px; }
        #exportPanel QScrollBar::add-line:vertical, #exportPanel QScrollBar::sub-line:vertical { height: 0; }
        #exportPanel QScrollBar::add-page:vertical, #exportPanel QScrollBar::sub-page:vertical { background: transparent; }
    )"))
        .arg(panelBg,   // 1
             panelBd,   // 2
             inputBg,   // 3
             txtPrim,   // 4
             txtMuted,  // 5
             txtBright, // 6
             subtleBd,  // 7
             hover,     // 8
             hoverStr)  // 9
        .arg(accent,    // 10
             well));    // 11
}
