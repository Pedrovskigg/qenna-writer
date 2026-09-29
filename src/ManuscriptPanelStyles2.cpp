// Estilos das levas 6 a 8 da gaveta de Manuscritos: Seleção de capítulo,
// Álbum de figurinhas, Programa da peça (temáticos); Colunas, Tambor,
// Camadas, Comando, Margem (listas limpas); Aura, Aura limpa, Carrossel,
// Encadernado, Leque, Janela (a lista limpa com a capa e a saga em volta).
// A montagem geral (rebuildHeader/Top/Body/Bottom) continua em
// ManuscriptPanel.cpp; aqui ficam só as peças de cada estilo.

#include "ManuscriptPanel.h"
#include "ManuscriptViews.h"
#include "ManuscriptViews2.h"
#include "MsVignette.h"
#include "CoverUtils.h"
#include "ElementsStore.h"
#include "PanelMotion.h"
#include "ProjectModel.h"
#include "Theme.h"
#include "WordCounter.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCache>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

namespace {
QColor tcol(const QString& css) { return Theme::toColor(css); }

QFont fontOf(const QString& family, qreal px, int weight = QFont::Normal, bool italic = false) {
    QFont f(family);
    f.setPixelSize(qMax(1, qRound(px)));
    f.setWeight(QFont::Weight(weight));
    f.setItalic(italic);
    return f;
}
QFont serif(qreal px, int weight = QFont::Normal, bool italic = false) { return fontOf(QStringLiteral("Lora"), px, weight, italic); }
QFont ui(qreal px, int weight = QFont::Normal) { return fontOf(QStringLiteral("Segoe UI"), px, weight); }
QFont caps(qreal px, qreal spacing = 1.4, int weight = QFont::Bold) {
    QFont f = ui(px, weight);
    f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
    return f;
}
QFont mono(qreal px) {
    QFont f(QStringLiteral("IBM Plex Mono"));
    f.setPixelSize(qRound(px));
    return f;
}
QColor alpha(QColor c, qreal a) { c.setAlphaF(float(std::clamp(a, 0.0, 1.0))); return c; }
QString css(const QColor& c) {
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alphaF(), 0, 'f', 3);
}
QLabel* label(const QString& text, QWidget* parent, const QFont& f, const QColor& color, bool wrap = false) {
    auto* l = new QLabel(text, parent);
    l->setFont(f);
    l->setWordWrap(wrap);
    l->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(css(color)));
    return l;
}
QToolButton* textButton(const QString& text, QWidget* parent, const QColor& color, const QColor& hover, qreal px = 12, bool bold = false) {
    auto* b = new QToolButton(parent);
    b->setText(text);
    b->setCursor(Qt::PointingHandCursor);
    b->setAutoRaise(true);
    b->setStyleSheet(QStringLiteral(
        "QToolButton { color: %1; background: transparent; border: none; padding: 2px 3px; font-size: %2px; font-weight: %3; }"
        "QToolButton:hover { color: %4; }").arg(css(color)).arg(px).arg(bold ? 600 : 400).arg(css(hover)));
    return b;
}
QString kSep() { return QStringLiteral(" · "); }
QString shortK(int words) {
    if (words >= 1000) return QLocale().toString(words / 1000.0, 'f', 1) + QStringLiteral("k");
    return QString::number(words);
}
// Linha de rótulo de Parte ("PARTE I · O ARQUIVO"): não clica, só separa.
void paintCapsRow(QPainter& p, const QRect& r, const QString& text, const QColor& color, int indent, bool line) {
    p.setFont(caps(9.5, 2.0));
    p.setPen(color);
    const int tw = p.fontMetrics().horizontalAdvance(text);
    p.drawText(QRect(r.left() + indent, r.top(), r.width() - indent, r.height() - 3), Qt::AlignLeft | Qt::AlignBottom,
               p.fontMetrics().elidedText(text, Qt::ElideRight, r.width() - indent - 8));
    if (line && tw + indent + 16 < r.width()) {
        p.setPen(QPen(alpha(color, 0.25), 1));
        const int y = r.bottom() - 3 - p.fontMetrics().height() / 2 + 1;
        p.drawLine(r.left() + indent + tw + 8, y, r.right() - 6, y);
    }
}
}

// ============================================================ utilidades

bool ManuscriptPanel::panelIsDark() const { return tcol(Theme::panelBackground()).lightnessF() < 0.5; }

QColor ManuscriptPanel::bookAccent(const QString& msId, bool onDark) const {
    const QColor c = MsPaint::bookColor(msId);
    const float h = c.hslHueF() < 0 ? 0.05f : c.hslHueF();
    return QColor::fromHslF(h, 0.55f, onDark ? 0.66f : 0.40f);
}

ManuscriptPanel::RowInk ManuscriptPanel::themeInk() const {
    RowInk k;
    k.text = tcol(Theme::textPrimary());
    k.muted = tcol(Theme::textMuted());
    k.bright = tcol(Theme::textBright());
    k.hover = tcol(Theme::hoverOverlay());
    k.accent = bookAccent(activeManuscriptId(), panelIsDark());
    k.openBg = alpha(k.accent, 0.18);
    return k;
}

ManuscriptPanel::RowInk ManuscriptPanel::auraInk() const {
    RowInk k;
    k.text = QColor(255, 255, 255, 205);
    k.muted = QColor(255, 255, 255, 120);
    k.bright = QColor(255, 255, 255);
    k.hover = QColor(255, 255, 255, 18);
    k.accent = bookAccent(activeManuscriptId(), true);
    k.openBg = alpha(k.accent, 0.24);
    return k;
}

ManuscriptPanel::RowInk ManuscriptPanel::paperInk() const {
    RowInk k;
    k.text = QColor(0x3b, 0x35, 0x2c);
    k.muted = QColor(0x8a, 0x81, 0x71);
    k.bright = QColor(0x15, 0x12, 0x0e);
    k.hover = QColor(0, 0, 0, 13);
    k.accent = bookAccent(activeManuscriptId(), false);
    k.openBg = alpha(k.accent, 0.16);
    return k;
}

bool ManuscriptPanel::isCaptionHeaderStyle() const {
    switch (m_style) {
    case Style::Rail: case Style::Layers: case Style::ChapterSelect: case Style::Aura: case Style::AuraClean:
    case Style::Carousel: case Style::Bound: case Style::Fan: case Style::Window:
        return true;
    default:
        return false;
    }
}

bool ManuscriptPanel::isCoverBackgroundStyle() const { return m_style == Style::Aura || m_style == Style::Window; }

bool ManuscriptPanel::toolsAllowed() const {
    switch (m_style) {
    case Style::Drum: case Style::Aura: case Style::Window: case Style::Layers: case Style::Album:
        return false;
    default:
        return true;
    }
}

int ManuscriptPanel::activeBookNumber() const {
    if (!m_model) return 1;
    const QString id = activeManuscriptId();
    const auto& mss = m_model->manuscripts();
    for (int i = 0; i < mss.size(); ++i) if (mss.at(i).id == id) return i + 1;
    return 1;
}

QList<ManuscriptPart> ManuscriptPanel::partsForDisplay(const QList<Chapter>& reading) const {
    return partsActive() ? validParts(reading) : QList<ManuscriptPart>();
}

QString ManuscriptPanel::partLabel(const ManuscriptPart& p) const {
    return p.title.isEmpty() ? tr("Parte") : p.title;
}

void ManuscriptPanel::showBookSwitchMenu(const QPoint& globalPos) {
    if (!m_model) return;
    QMenu menu(this);
    menu.setStyleSheet(contextMenuStyle());
    auto* group = new QActionGroup(&menu);
    const auto& mss = m_model->manuscripts();
    const QString active = activeManuscriptId();
    for (int i = 0; i < mss.size(); ++i) {
        const Manuscript& m = mss.at(i);
        QAction* a = menu.addAction(QIcon(bookCoverPixmap(m, i + 1, QSize(20, 29))),
                                    QStringLiteral("%1. %2").arg(i + 1).arg(m.title.isEmpty() ? tr("(sem título)") : m.title));
        a->setCheckable(true);
        a->setChecked(m.id == active);
        group->addAction(a);
        const QString id = m.id;
        connect(a, &QAction::triggered, this, [this, id]() { selectManuscript(id); });
    }
    menu.addSeparator();
    connect(menu.addAction(tr("Novo manuscrito")), &QAction::triggered, this, &ManuscriptPanel::newManuscriptRequested);
    PanelMotion::animateMenu(&menu);
    menu.exec(globalPos);
}

// ============================================================ lista limpa (levas 7 e 8)

QWidget* ManuscriptPanel::makeSimpleChapterRow(const Chapter& c, const RowInk& ink) {
    const bool open = !m_curChapterId.isEmpty() && m_curChapterId == c.id;
    const bool special = c.type != QStringLiteral("chapter");
    const QString num = special ? QStringLiteral("·") : m_model->chapterNumberLabel(c);
    const QString title = special ? m_model->chapterTypeName(c) + (c.title.isEmpty() ? QString() : QStringLiteral(": ") + c.title)
                                  : chapterTitleOnly(c);
    const int words = chapterWords(c.id);
    const bool empty = words <= 0;
    auto* row = new MsRow(this);
    row->setRowHeight(32);
    row->setToolTip(empty ? tr("Capítulo vazio") : tr("%1 palavras").arg(MsPaint::fmtInt(words)));
    row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF box = QRectF(r).adjusted(6, 1, -6, -1);
        if (open) {
            p.setPen(Qt::NoPen);
            p.setBrush(ink.openBg);
            p.drawRoundedRect(box, 5, 5);
            p.setBrush(ink.accent);
            p.drawRoundedRect(QRectF(box.left(), box.top() + 2, 3, box.height() - 4), 1.5, 1.5);
        } else if (self->isHovered()) {
            p.setPen(Qt::NoPen);
            p.setBrush(ink.hover);
            p.drawRoundedRect(box, 5, 5);
        }
        p.setFont(ui(12));
        p.setPen(open ? ink.accent : ink.muted);
        p.drawText(QRect(r.left() + 12, r.top(), 18, r.height()), Qt::AlignRight | Qt::AlignVCenter, num);
        p.setFont(serif(14.5, QFont::Normal, empty));
        p.setPen(open || self->isHovered() ? ink.bright : (empty ? ink.muted : ink.text));
        const QRect tr2(r.left() + 39, r.top(), r.width() - 51, r.height());
        p.drawText(tr2, Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(title, Qt::ElideRight, tr2.width()));
    });
    row->setContextMenuPolicy(Qt::CustomContextMenu);
    const QString chapterId = c.id, manuscriptId = c.manuscriptId;
    connect(row, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() { emit chapterActivated(manuscriptId, chapterId); });
    connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId](const QPoint& pos) {
        showChapterContextMenu(manuscriptId, chapterId, row->mapToGlobal(pos));
    });
    wireRow(row, QStringLiteral("chapter"), chapterId);
    return row;
}

QWidget* ManuscriptPanel::makeSimpleSceneRow(const Chapter& c, int idx, const RowInk& ink) {
    const bool open = isCurrent(c.id, idx);
    const QString text = sceneTitle(c, idx);
    auto* row = new MsRow(this);
    row->setRowHeight(26);
    row->setToolTip(tr("%1 palavras").arg(MsPaint::fmtInt(sceneWords(c.id, idx))));
    row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
        p.setRenderHint(QPainter::Antialiasing);
        if (self->isHovered()) {
            p.setPen(Qt::NoPen);
            p.setBrush(ink.hover);
            p.drawRoundedRect(QRectF(r).adjusted(6, 1, -6, -1), 5, 5);
        }
        int x = r.left() + 43;
        if (open) {
            p.setPen(Qt::NoPen);
            p.setBrush(ink.accent);
            p.drawEllipse(QPointF(x + 3, r.center().y() + 0.5), 3, 3);
            x += 13;
        }
        p.setFont(serif(13, QFont::Normal, true));
        p.setPen(open || self->isHovered() ? ink.bright : ink.muted);
        p.drawText(QRect(x, r.top(), r.right() - 12 - x, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(text, Qt::ElideRight, r.right() - 12 - x));
    });
    row->setContextMenuPolicy(Qt::CustomContextMenu);
    const QString chapterId = c.id, manuscriptId = c.manuscriptId;
    connect(row, &QToolButton::clicked, this, [this, manuscriptId, chapterId, idx]() { emit sceneActivated(manuscriptId, chapterId, idx); });
    connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId, idx](const QPoint& pos) {
        showSceneContextMenu(manuscriptId, chapterId, idx, row->mapToGlobal(pos));
    });
    wireRow(row, QStringLiteral("scene"), chapterId, idx);
    return row;
}

QWidget* ManuscriptPanel::makeSimplePartRow(const ManuscriptPart& part, const RowInk& ink) {
    auto* row = new MsRow(this);
    row->setRowHeight(30);
    row->setCursor(Qt::ArrowCursor);
    const QString text = partLabel(part).toUpper();
    row->setPainter([=](QPainter& p, const QRect& r, const MsRow*) { paintCapsRow(p, r, text, ink.muted, 14, false); });
    row->setContextMenuPolicy(Qt::CustomContextMenu);
    const QString pid = part.id;
    connect(row, &QToolButton::customContextMenuRequested, this, [this, row, pid](const QPoint& pos) {
        showPartContextMenu(pid, row->mapToGlobal(pos));
    });
    return row;
}

void ManuscriptPanel::buildSimpleList(const QList<Chapter>& chs, const RowInk& ink) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QList<Chapter> reading = readingChapters();
    const QList<ManuscriptPart> parts = partsForDisplay(reading);
    const QHash<QString, int> pidx = partIndexByChapter(reading, parts);
    int curPart = -2;
    for (const auto& c : chs) {
        const int pi = parts.isEmpty() ? -1 : pidx.value(c.id, -1);
        if (pi != curPart) {
            curPart = pi;
            if (pi >= 0) add(makeSimplePartRow(parts.at(pi), ink));
        }
        add(makeSimpleChapterRow(c, ink));
        // As cenas só aparecem no capítulo aberto: o resto da lista fica limpo.
        if (c.id == m_curChapterId && c.scenes.size() > 1)
            for (int i = 0; i < c.scenes.size(); ++i) add(makeSimpleSceneRow(c, i, ink));
    }
}

QWidget* ManuscriptPanel::makeCoverBlock(bool large, const RowInk& ink) {
    const QString msId = activeManuscriptId();
    const Manuscript* m = m_model ? m_model->findManuscript(msId) : nullptr;
    if (!m) return nullptr;
    const int number = activeBookNumber();
    const QSize cs = large ? QSize(74, 108) : QSize(46, 67);
    auto* w = new QWidget(m_top);
    w->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(w, &QWidget::customContextMenuRequested, this, [this, w, msId](const QPoint& pos) {
        showManuscriptContextMenu(msId, w->mapToGlobal(pos));
    });
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(2, large ? 6 : 4, 2, large ? 6 : 2);
    lay->setSpacing(large ? 14 : 12);
    // Capa com sombra desenhada na própria imagem (efeito gráfico brigaria com a cascata).
    const qreal dpr = devicePixelRatioF();
    const QPixmap cover = bookCoverPixmap(*m, number, cs);
    QPixmap framed(QSize(cs.width() + 10, cs.height() + 12) * dpr);
    framed.setDevicePixelRatio(dpr);
    framed.fill(Qt::transparent);
    {
        QPainter p(&framed);
        p.setRenderHint(QPainter::Antialiasing);
        for (int i = 5; i >= 1; --i) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 22));
            p.drawRoundedRect(QRectF(5 - i * 0.6, 3 + i * 1.1, cs.width() + i * 1.2, cs.height() + i * 0.6), 3 + i, 3 + i);
        }
        p.drawPixmap(QPointF(5, 3), cover);
        p.fillRect(QRectF(5, 3, 3, cs.height()), QColor(0, 0, 0, 60));
    }
    auto* cov = new QLabel(w);
    cov->setPixmap(framed);
    cov->setFixedSize(framed.size() / dpr);
    lay->addWidget(cov, 0, large ? Qt::AlignBottom : Qt::AlignVCenter);

    auto* txt = new QVBoxLayout;
    txt->setSpacing(3);
    txt->addStretch(1);
    txt->addWidget(label(tr("Livro %1").arg(number).toUpper(), w, caps(9, 2.4), ink.accent));
    QFont tf = fontOf(MsFonts::cormorant(), large ? 23 : 20, QFont::DemiBold, true);
    txt->addWidget(label(m->title.isEmpty() ? tr("(sem título)") : m->title, w, tf, ink.bright, true));
    const int words = m_wordCounter ? m_wordCounter->countManuscript(msId) : 0;
    const int chs = readingChapters().size();
    txt->addWidget(label(large ? tr("%n capítulo(s)", "", chs) + kSep() + tr("%1 palavras").arg(MsPaint::fmtInt(words))
                               : tr("%1 palavras").arg(MsPaint::fmtInt(words)),
                         w, ui(11), ink.muted));
    if (large && m_model->manuscripts().size() > 1) {
        auto* sw = new QToolButton(w);
        sw->setText(QStringLiteral("⇄  ") + tr("Trocar de livro"));
        sw->setCursor(Qt::PointingHandCursor);
        const bool overCover = (m_style == Style::Aura);
        sw->setStyleSheet(QStringLiteral(
            "QToolButton { color: %1; background: %2; border: 1px solid %3; border-radius: 11px; padding: 2px 10px; font-size: 11px; }"
            "QToolButton:hover { color: %4; background: %5; }")
            .arg(css(ink.text), overCover ? QStringLiteral("rgba(0,0,0,0.2)") : QStringLiteral("transparent"),
                 overCover ? QStringLiteral("rgba(255,255,255,0.22)") : Theme::panelBorder(), css(ink.bright), css(ink.hover)));
        connect(sw, &QToolButton::clicked, this, [this, sw]() { showBookSwitchMenu(sw->mapToGlobal(QPoint(0, sw->height() + 2))); });
        auto* swRow = new QHBoxLayout;
        swRow->setContentsMargins(0, 5, 0, 0);
        swRow->addWidget(sw);
        swRow->addStretch(1);
        txt->addLayout(swRow);
    }
    lay->addLayout(txt, 1);
    return w;
}

QWidget* ManuscriptPanel::makeDashedNewChapter(const QColor& color) {
    auto* b = new QPushButton(QStringLiteral("＋  ") + tr("Novo capítulo"), m_bottom);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(Theme::qss(QStringLiteral(
        "QPushButton { color: %1; background: transparent; border: 1px dashed %2; border-radius: @radius-control;"
        " padding: 7px 10px; font-size: 12.5px; }"
        "QPushButton:hover { background: %3; }"))
        .arg(css(color), css(alpha(color, 0.55)), css(alpha(color, 0.14))));
    connect(b, &QPushButton::clicked, this, [this]() { emit newChapterRequested(activeManuscriptId()); });
    return b;
}

// ============================================================ Seleção de capítulo

void ManuscriptPanel::buildChapterSelectTop() {
    if (activeManuscriptId().isEmpty()) return;
    m_topLayout->setContentsMargins(0, 0, 0, 0);
    m_topLayout->setSpacing(0);
    m_hero = new MsHero(m_top);
    connect(m_hero, &MsHero::bookChipClicked, this, &ManuscriptPanel::showBookSwitchMenu);
    m_topLayout->addWidget(m_hero);

    auto* under = new QWidget(m_top);
    auto* ul = new QVBoxLayout(under);
    ul->setContentsMargins(12, 8, 12, 6);
    ul->setSpacing(6);
    m_checkpoints = new MsCheckpoints(under);
    connect(m_checkpoints, &MsCheckpoints::sceneClicked, this, [this](int i) {
        const QString ch = m_previewChapterId.isEmpty() ? m_curChapterId : m_previewChapterId;
        if (!ch.isEmpty()) emit sceneActivated(activeManuscriptId(), ch, i);
    });
    ul->addWidget(m_checkpoints);
    m_heroGo = new QPushButton(under);
    m_heroGo->setCursor(Qt::PointingHandCursor);
    m_heroGo->setMinimumHeight(38);
    auto* gl = new QHBoxLayout(m_heroGo);
    gl->setContentsMargins(12, 0, 12, 0);
    gl->setSpacing(10);
    auto* key = new QLabel(QStringLiteral("▶"), m_heroGo);
    key->setAttribute(Qt::WA_TransparentForMouseEvents);
    key->setObjectName(QStringLiteral("csKey"));
    auto* goText = new QLabel(m_heroGo);
    goText->setObjectName(QStringLiteral("csGoText"));
    goText->setAttribute(Qt::WA_TransparentForMouseEvents);
    goText->setFont(fontOf(MsFonts::display(), 16, QFont::ExtraBold));
    auto* goSmall = new QLabel(m_heroGo);
    goSmall->setObjectName(QStringLiteral("csGoSmall"));
    goSmall->setAttribute(Qt::WA_TransparentForMouseEvents);
    goSmall->setFont(ui(11));
    gl->addWidget(key);
    gl->addWidget(goText);
    gl->addStretch(1);
    gl->addWidget(goSmall);
    const QColor light = tcol(Theme::textBright());
    const QColor dark = tcol(Theme::panelBackground());
    m_heroGo->setStyleSheet(Theme::qss(QStringLiteral(
        "QPushButton { background: %1; border: none; border-radius: @radius-control; }"
        "QPushButton:hover { background: %2; }"
        "QLabel { color: %3; background: transparent; }"
        "QLabel#csKey { font-size: 11px; }"))
        .arg(css(light), css(light.darker(108)), css(dark)));
    connect(m_heroGo, &QPushButton::clicked, this, [this]() {
        const QString msId = activeManuscriptId();
        const QString ch = m_previewChapterId.isEmpty() ? m_curChapterId : m_previewChapterId;
        if (ch.isEmpty()) {
            const QList<Chapter> reading = readingChapters();
            if (!reading.isEmpty()) emit chapterActivated(msId, reading.first().id);
            return;
        }
        ResumeEntry e;
        if (ch == m_curChapterId || m_previewChapterId.isEmpty()) {
            if (lastResumeHere(&e) && e.chapterId == ch) {
                emit resumeRequested(msId, e.chapterId, e.sceneIndex, e.sentence, e.position);
                return;
            }
        }
        emit chapterActivated(msId, ch);
    });
    ul->addWidget(m_heroGo);
    m_topLayout->addWidget(under);
    m_previewChapterId.clear();
    updateChapterSelectHero();
}

void ManuscriptPanel::updateChapterSelectHero() {
    if (!m_hero || !m_model) return;
    const QList<Chapter> reading = readingChapters();
    if (reading.isEmpty()) { m_hero->hide(); if (m_checkpoints) m_checkpoints->hide(); if (m_heroGo) m_heroGo->hide(); return; }
    QString id = m_previewChapterId.isEmpty() ? m_curChapterId : m_previewChapterId;
    const Chapter* c = nullptr;
    for (const auto& ch : reading) if (ch.id == id) c = &ch;
    if (!c) { c = &reading.first(); id = c->id; }
    const bool isPreview = !m_previewChapterId.isEmpty() && m_previewChapterId != m_curChapterId;

    const QList<ManuscriptPart> parts = partsForDisplay(reading);
    const int pi = parts.isEmpty() ? -1 : partIndexByChapter(reading, parts).value(c->id, -1);
    const QColor tint = pi >= 0 && QColor(parts.at(pi).color).isValid() ? QColor(parts.at(pi).color) : MsPaint::bookColor(activeManuscriptId());
    const Manuscript* m = m_model->findManuscript(activeManuscriptId());

    MsHero::Data d;
    static QCache<QString, QPixmap> artCache(48);
    const int w = std::max(240, width());
    const int words = chapterWords(c->id);
    const QString key = QStringLiteral("%1|%2|%3|%4|%5").arg(c->id).arg(words).arg(w).arg(tint.name(), c->vignette + c->vignetteImage.left(40));
    if (QPixmap* hit = artCache.object(key)) d.art = *hit;
    else {
        const QHash<QString, int> fams = vignetteFamilies();
        d.art = chapterVignette(*c, fams.value(c->id, MsVignette::autoFamily(c->id)), QSize(w, 244), false);
        artCache.insert(key, new QPixmap(d.art));
    }
    d.tint = tint;
    d.eyebrow = pi >= 0 ? partLabel(parts.at(pi)).toUpper() : (m && !m->title.isEmpty() ? m->title.toUpper() : QString());
    const bool special = c->type != QStringLiteral("chapter");
    d.big = special ? m_model->chapterTypeName(*c) : tr("Capítulo %1").arg(m_model->chapterNumberLabel(*c));
    d.title = special ? c->title : chapterTitleOnly(*c);
    QStringList meta;
    if (words > 0) {
        meta << tr("%n cena(s)", "", std::max<int>(1, c->scenes.size())) << tr("%1 palavras").arg(MsPaint::fmtInt(words));
        const QString pov = povOf(*c);
        if (!pov.isEmpty()) meta << elementName(pov);
        if (!c->timeMarker.trimmed().isEmpty()) meta << c->timeMarker.trimmed();
    } else {
        meta << tr("Capítulo em branco");
    }
    d.meta = meta.join(kSep());
    d.preview = isPreview;
    d.previewText = tr("PRÉVIA");
    if (m_model->manuscripts().size() > 1) d.bookChip = tr("LIVRO %1").arg(activeBookNumber()) + QStringLiteral("  ⇄");
    m_hero->setData(d);
    m_hero->show();

    // Checkpoints: as cenas do capítulo mostrado.
    int here = -1;
    if (c->id == m_curChapterId) here = std::max(0, m_curScene);
    else { ResumeEntry e; if (lastResumeHere(&e) && e.chapterId == c->id) here = std::max(0, e.sceneIndex); }
    if (m_checkpoints) {
        if (c->scenes.size() > 1) {
            QStringList labels, tips;
            for (int i = 0; i < c->scenes.size(); ++i) {
                labels << tr("CENA %1").arg(i + 1);
                tips << sceneTitle(*c, i) + kSep() + tr("%1 palavras").arg(MsPaint::fmtInt(sceneWords(c->id, i)));
            }
            m_checkpoints->setData(c->scenes.size(), here, tint, labels, tips);
            m_checkpoints->show();
        } else {
            m_checkpoints->hide();
        }
    }
    if (m_heroGo) {
        if (auto* t = m_heroGo->findChild<QLabel*>(QStringLiteral("csGoText")))
            t->setText((c->id == m_curChapterId ? tr("Continuar") : tr("Abrir capítulo")).toUpper());
        if (auto* s = m_heroGo->findChild<QLabel*>(QStringLiteral("csGoSmall")))
            s->setText(here >= 0 && c->scenes.size() > 1 ? tr("cena %1 de %2").arg(here + 1).arg(c->scenes.size())
                                                         : (words > 0 ? tr("do começo") : tr("começar a escrever")));
        m_heroGo->show();
    }
}

void ManuscriptPanel::setChapterPreview(const QString& chapterId) {
    if (m_style != Style::ChapterSelect || chapterId == m_previewChapterId) return;
    m_previewChapterId = chapterId;
    updateChapterSelectHero();
    for (auto* row : m_scroll->widget()->findChildren<MsRow*>()) row->update();
}

void ManuscriptPanel::buildChapterSelectView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QList<Chapter> reading = readingChapters();
    const QList<ManuscriptPart> parts = partsForDisplay(reading);
    const QHash<QString, int> pidx = partIndexByChapter(reading, parts);
    const QColor bookTint = MsPaint::bookColor(activeManuscriptId());
    int curPart = -2;
    for (const auto& c : chs) {
        const int pi = parts.isEmpty() ? -1 : pidx.value(c.id, -1);
        const QColor tint = pi >= 0 && QColor(parts.at(pi).color).isValid() ? QColor(parts.at(pi).color) : bookTint;
        if (pi != curPart) {
            curPart = pi;
            if (pi >= 0) {
                auto* head = new MsRow(this);
                head->setRowHeight(30);
                head->setCursor(Qt::ArrowCursor);
                const QString t = partLabel(parts.at(pi)).toUpper();
                head->setPainter([t](QPainter& p, const QRect& r, const MsRow*) {
                    paintCapsRow(p, r, t, tcol(Theme::textMuted()), 8, true);
                });
                add(head);
            }
        }
        const bool special = c.type != QStringLiteral("chapter");
        const QString num = special ? QStringLiteral("✦") : m_model->chapterNumberLabel(c).rightJustified(2, QLatin1Char('0'));
        const QString title = special ? m_model->chapterTypeName(c) + (c.title.isEmpty() ? QString() : QStringLiteral(": ") + c.title)
                                      : chapterTitleOnly(c);
        const int words = chapterWords(c.id);
        const bool cur = c.id == m_curChapterId;
        const QColor hue = QColor::fromHslF(tint.hslHueF() < 0 ? 0.05f : tint.hslHueF(), 0.70f, panelIsDark() ? 0.70f : 0.34f);
        const QString chapterId = c.id;
        auto* row = new MsRow(this);
        row->setRowHeight(32);
        row->setPainter([this, num, title, words, cur, hue, chapterId](QPainter& p, const QRect& r, const MsRow* self) {
            p.setRenderHint(QPainter::Antialiasing);
            const bool prev = (m_previewChapterId == chapterId) && !cur;
            if (cur) {
                QLinearGradient g(r.topLeft(), r.topRight());
                g.setColorAt(0, alpha(hue, 0.22));
                g.setColorAt(1, alpha(hue, 0.0));
                p.fillRect(r, g);
                p.fillRect(QRect(r.left(), r.top(), 2, r.height()), hue);
            } else if (self->isHovered() || prev) {
                p.fillRect(r, tcol(Theme::hoverOverlay()));
            }
            p.setFont(fontOf(MsFonts::display(), 17, QFont::ExtraBold));
            p.setPen(cur ? hue : alpha(tcol(Theme::textPrimary()), 0.45));
            p.drawText(QRect(r.left() + 10, r.top(), 26, r.height()), Qt::AlignLeft | Qt::AlignVCenter, num);
            p.setFont(mono(10));
            const QString wk = words > 0 ? shortK(words) : QString();
            const int ww = p.fontMetrics().horizontalAdvance(wk);
            p.setPen(alpha(tcol(Theme::textPrimary()), 0.35));
            p.drawText(QRect(r.right() - 10 - ww, r.top(), ww + 2, r.height()), Qt::AlignRight | Qt::AlignVCenter, wk);
            p.setFont(fontOf(MsFonts::cormorant(), 17, QFont::Medium, words <= 0));
            p.setPen(cur || self->isHovered() || prev ? tcol(Theme::textBright()) : tcol(Theme::textPrimary()));
            QString t = cur ? QStringLiteral("▸ ") + title : title;
            const int tw = r.right() - 16 - ww - (r.left() + 42);
            p.drawText(QRect(r.left() + 42, r.top(), tw, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(t, Qt::ElideRight, tw));
        });
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString manuscriptId = c.manuscriptId;
        connect(row, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() { emit chapterActivated(manuscriptId, chapterId); });
        connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId](const QPoint& pos) {
            showChapterContextMenu(manuscriptId, chapterId, row->mapToGlobal(pos));
        });
        wireRow(row, QStringLiteral("chapter"), chapterId);
        row->setProperty("previewChapterId", chapterId);
        add(row);
    }
}

// ============================================================ Álbum de figurinhas

namespace {
struct AlbumPage { QString eyebrow, title, watermark; QColor color; QList<int> idx; };
}

static QList<AlbumPage> albumPages(const QList<Chapter>& reading, const QList<ManuscriptPart>& parts,
                                   const QHash<QString, int>& pidx, const QColor& bookColor,
                                   const std::function<QString(int, int)>& rangeLabel, const QString& openingWord) {
    QList<AlbumPage> pages;
    if (!parts.isEmpty()) {
        AlbumPage lead;
        for (int i = 0; i < reading.size(); ++i) {
            const int pi = pidx.value(reading.at(i).id, -1);
            if (pi < 0) { lead.idx << i; continue; }
            while (pages.size() <= pi) {
                const ManuscriptPart& part = parts.at(pages.size());
                AlbumPage pg;
                pg.eyebrow = ProjectModel::toRomanNumeral(pages.size() + 1);
                pg.title = part.title;
                pg.watermark = ProjectModel::toRomanNumeral(pages.size() + 1);
                pg.color = QColor(part.color).isValid() ? QColor(part.color) : bookColor;
                pages << pg;
            }
            pages[pi].idx << i;
        }
        if (!lead.idx.isEmpty()) {
            lead.eyebrow = QStringLiteral("✦");
            lead.title = openingWord;
            lead.watermark = QStringLiteral("✦");
            lead.color = bookColor;
            pages.prepend(lead);
        }
    } else {
        for (int start = 0; start < reading.size(); start += 6) {
            AlbumPage pg;
            for (int i = start; i < std::min<int>(reading.size(), start + 6); ++i) pg.idx << i;
            pg.eyebrow = QString::number(pages.size() + 1);
            pg.title = rangeLabel(start + 1, start + pg.idx.size());
            pg.watermark = QString::number(pages.size() + 1);
            pg.color = bookColor;
            pages << pg;
        }
    }
    pages.erase(std::remove_if(pages.begin(), pages.end(), [](const AlbumPage& p) { return p.idx.isEmpty(); }), pages.end());
    return pages;
}

void ManuscriptPanel::buildAlbumTop() {
    const QList<Chapter> reading = readingChapters();
    if (reading.isEmpty()) return;
    const QList<ManuscriptPart> parts = partsForDisplay(reading);
    const QList<AlbumPage> pages = albumPages(reading, parts, partIndexByChapter(reading, parts), MsPaint::bookColor(activeManuscriptId()),
        [this](int a, int b) { return tr("Capítulos %1–%2").arg(a).arg(b); }, tr("Abertura"));
    if (m_albumPage < 0 || m_albumPage >= pages.size()) {
        m_albumPage = 0;
        for (int p = 0; p < pages.size(); ++p)
            for (int i : pages.at(p).idx) if (reading.at(i).id == m_curChapterId) m_albumPage = p;
    }
    auto* w = new QWidget(m_top);
    auto* l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(10);
    int glued = 0;
    QList<bool> flags;
    QStringList tips;
    for (const auto& c : reading) {
        const bool g = chapterWords(c.id) > 0;
        glued += g ? 1 : 0;
        flags << g;
        tips << chapterRowText(c);
    }
    auto* cnt = new QLabel(w);
    cnt->setTextFormat(Qt::RichText);
    cnt->setText(tr("<span style=\"font-family:'%1'; font-size:15px; color:%2;\">%3</span> de %4 coladas")
        .arg(MsFonts::sticker(), Theme::textBright()).arg(glued).arg(reading.size()));
    cnt->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; background: transparent;").arg(Theme::textMuted()));
    l->addWidget(cnt);
    auto* check = new MsChecklist(w);
    check->setData(flags, tips);
    l->addWidget(check, 0, Qt::AlignVCenter);
    l->addStretch(1);
    const QColor mutedC = tcol(Theme::textMuted()), brightC = tcol(Theme::textBright());
    auto* prev = textButton(QStringLiteral("‹"), w, tcol(Theme::textPrimary()), brightC, 16);
    auto* next = textButton(QStringLiteral("›"), w, tcol(Theme::textPrimary()), brightC, 16);
    prev->setToolTip(tr("Página anterior"));
    next->setToolTip(tr("Próxima página"));
    auto* pg = label(tr("pág. %1 de %2").arg(m_albumPage + 1).arg(pages.size()), w, ui(11), mutedC);
    const int n = pages.size();
    auto turn = [this, n](int step) {
        if (n <= 1) return;
        m_albumPage = (m_albumPage + step + n) % n;
        const bool animate = isVisible();
        if (animate) PanelMotion::swapOut(m_scroll->viewport());
        rebuildList();
        if (animate) playIntro(40);
    };
    connect(prev, &QToolButton::clicked, this, [turn]() { turn(-1); });
    connect(next, &QToolButton::clicked, this, [turn]() { turn(1); });
    l->addWidget(prev);
    l->addWidget(pg);
    l->addWidget(next);
    m_topLayout->addWidget(w);
}

void ManuscriptPanel::buildAlbumView(const QList<Chapter>&) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QList<Chapter> reading = readingChapters();
    const QList<ManuscriptPart> parts = partsForDisplay(reading);
    const QList<AlbumPage> pages = albumPages(reading, parts, partIndexByChapter(reading, parts), MsPaint::bookColor(activeManuscriptId()),
        [this](int a, int b) { return tr("Capítulos %1–%2").arg(a).arg(b); }, tr("Abertura"));
    if (pages.isEmpty()) return;
    const AlbumPage& page = pages.at(std::clamp(m_albumPage, 0, int(pages.size()) - 1));
    const int pageW = std::max(220, width() - 16 - 8);
    const int side = MsAlbumPage::artSide(pageW);
    const QHash<QString, int> fams = vignetteFamilies();
    QList<MsAlbumPage::Sticker> stickers;
    for (int i : page.idx) {
        const Chapter& c = reading.at(i);
        const int words = chapterWords(c.id);
        MsAlbumPage::Sticker s;
        s.chapterId = c.id;
        const bool special = c.type != QStringLiteral("chapter");
        s.number = special ? QStringLiteral("★") : m_model->chapterNumberLabel(c);
        s.title = special ? m_model->chapterTypeName(c) : chapterTitleOnly(c);
        s.foil = special;
        s.empty = words <= 0;
        s.current = c.id == m_curChapterId;
        if (s.empty) {
            s.number = special ? QStringLiteral("✦") : s.number;
            s.title = chapterTitleOnly(c);
            s.meta = tr("escreva aqui");
        } else {
            s.meta = tr("%n cena(s)", "", std::max<int>(1, c.scenes.size())) + kSep() + shortK(words);
            s.art = chapterVignette(c, fams.value(c.id, MsVignette::autoFamily(c.id)), QSize(side, side), false);
        }
        stickers << s;
    }
    auto* album = new MsAlbumPage(this);
    QString eyebrow = page.eyebrow == QStringLiteral("✦") ? page.eyebrow
        : (!parts.isEmpty() ? tr("PARTE %1").arg(page.eyebrow) : tr("PÁGINA %1").arg(page.eyebrow));
    // Parte que já se chama "Parte II · A torre": o rótulo repetiria.
    if (page.title.toUpper().startsWith(eyebrow.toUpper())) eyebrow.clear();
    album->setPage(eyebrow, page.title, page.watermark, page.color, stickers,
                   tr("%n figurinha(s)", "", stickers.size()));
    connect(album, &MsAlbumPage::stickerClicked, this, [this](const QString& id) { emit chapterActivated(activeManuscriptId(), id); });
    connect(album, &MsAlbumPage::stickerContextRequested, this, [this](const QString& id, const QPoint& pos) {
        showChapterContextMenu(activeManuscriptId(), id, pos);
    });
    add(album);

    // As cenas do capítulo aberto, como figurinhas pequenas no verso.
    for (int i : page.idx) {
        const Chapter& c = reading.at(i);
        if (c.id != m_curChapterId || c.scenes.size() < 2 || chapterWords(c.id) <= 0) continue;
        auto* verso = new QWidget(this);
        auto* vl = new QHBoxLayout(verso);
        vl->setContentsMargins(6, 10, 6, 4);
        vl->setSpacing(6);
        vl->addWidget(label(tr("Cenas de %1").arg(chapterTitleOnly(c)).toUpper(), verso, caps(9, 1.2), tcol(Theme::textMuted())));
        for (int j = 0; j < c.scenes.size(); ++j) {
            auto* b = new QToolButton(verso);
            b->setText(QString::number(j + 1));
            b->setToolTip(sceneTitle(c, j));
            b->setCursor(Qt::PointingHandCursor);
            const bool on = isCurrent(c.id, j);
            b->setStyleSheet(QStringLiteral(
                "QToolButton { color: %1; background: %2; border: none; border-radius: 3px; padding: 3px 8px;"
                " font-size: 11px; font-weight: 700; }"
                "QToolButton:hover { background: %3; }")
                .arg(on ? QStringLiteral("#ffffff") : QStringLiteral("#1b1a18"), on ? page.color.name() : QStringLiteral("#f4f2ec"),
                     on ? page.color.lighter(115).name() : QStringLiteral("#ffffff")));
            const QString chId = c.id;
            connect(b, &QToolButton::clicked, this, [this, chId, j]() { emit sceneActivated(activeManuscriptId(), chId, j); });
            vl->addWidget(b);
        }
        vl->addStretch(1);
        add(verso);
    }
}

// ============================================================ Programa da peça

void ManuscriptPanel::buildPlaybillView(const QList<Chapter>&) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QList<Chapter> reading = readingChapters();
    const Manuscript* m = m_model->findManuscript(activeManuscriptId());
    if (!m || reading.isEmpty()) return;
    const QColor gold(201, 164, 92), paper(241, 234, 216);
    const QColor ink = alpha(paper, 0.92), faint = alpha(paper, 0.5);

    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("msPlaybill"));
    card->setStyleSheet(Theme::qss(QStringLiteral(
        "#msPlaybill { border-radius: @radius-panel; border: 1px solid rgba(201,164,92,0.25);"
        " background: qradialgradient(cx:0.5, cy:0, radius:0.9, fx:0.5, fy:0, stop:0 #2a2418, stop:0.55 #171513, stop:1 #161513); }")));
    auto* cl = new QVBoxLayout(card);
    cl->setContentsMargins(16, 18, 16, 16);
    cl->setSpacing(4);
    auto center = [](QLabel* l) { l->setAlignment(Qt::AlignHCenter); return l; };
    cl->addWidget(center(label(tr("Programa").toUpper(), card, caps(9, 4.5), gold)));
    cl->addSpacing(4);
    cl->addWidget(center(label(m->title.isEmpty() ? tr("(sem título)") : m->title, card,
                               fontOf(MsFonts::bodoni(), 26, QFont::Medium, true), paper, true)));

    // Atos = Partes. O que vem antes da primeira Parte é abertura (prólogo…).
    const QList<ManuscriptPart> parts = partsForDisplay(reading);
    const QHash<QString, int> pidx = partIndexByChapter(reading, parts);
    const int acts = parts.isEmpty() ? 1 : parts.size();
    bool hasPro = false, hasEpi = false;
    for (const auto& c : reading) {
        hasPro = hasPro || c.type == QStringLiteral("prologue");
        hasEpi = hasEpi || c.type == QStringLiteral("epilogue");
    }
    const QStringList words = { tr("um"), tr("dois"), tr("três"), tr("quatro"), tr("cinco"),
                                tr("seis"), tr("sete"), tr("oito"), tr("nove"), tr("dez") };
    QString sub = tr("uma peça em %1 %2").arg(acts <= words.size() ? words.at(acts - 1) : QString::number(acts),
                                             acts == 1 ? tr("ato") : tr("atos"));
    if (hasPro && hasEpi) sub += tr(", com prólogo e epílogo");
    else if (hasPro) sub += tr(" e um prólogo");
    else if (hasEpi) sub += tr(" e um epílogo");
    cl->addWidget(center(label(sub, card, fontOf(QStringLiteral("Source Serif 4"), 13, QFont::Normal, true), faint)));

    auto ornament = [&]() {
        auto* o = new MsRow(card);
        o->setRowHeight(22);
        o->setCursor(Qt::ArrowCursor);
        o->setEnabled(false);
        o->setPainter([gold](QPainter& p, const QRect& r, const MsRow*) {
            p.setRenderHint(QPainter::Antialiasing);
            const int cx = r.center().x(), cy = r.center().y();
            QLinearGradient g1(r.left(), 0, cx - 12, 0), g2(cx + 12, 0, r.right(), 0);
            g1.setColorAt(0, alpha(gold, 0)); g1.setColorAt(1, alpha(gold, 0.6));
            g2.setColorAt(0, alpha(gold, 0.6)); g2.setColorAt(1, alpha(gold, 0));
            p.fillRect(QRectF(r.left(), cy, cx - 12 - r.left(), 1), g1);
            p.fillRect(QRectF(cx + 12, cy, r.right() - cx - 12, 1), g2);
            p.setPen(gold);
            p.setFont(ui(10));
            p.drawText(QRect(cx - 8, r.top(), 16, r.height()), Qt::AlignCenter, QStringLiteral("✦"));
        });
        cl->addWidget(o);
    };

    // Rótulo de onde cada capítulo fica no programa ("Ato I, cena 2").
    QHash<QString, QString> where;
    QHash<int, int> sceneNo;
    for (const auto& c : reading) {
        if (c.type != QStringLiteral("chapter")) { where.insert(c.id, m_model->chapterTypeName(c)); continue; }
        const int pi = parts.isEmpty() ? 0 : pidx.value(c.id, -1);
        if (pi < 0) { where.insert(c.id, tr("Abertura")); continue; }
        const int k = ++sceneNo[pi];
        where.insert(c.id, tr("Ato %1, cena %2").arg(ProjectModel::toRomanNumeral(pi + 1)).arg(k));
    }

    // Elenco por ordem de entrada: primeiro capítulo em que cada personagem aparece.
    if (m_elementsStore) {
        QStringList order;
        QHash<QString, QString> entrance;
        QHash<QString, int> presence;
        for (const auto& c : reading) {
            const QStringList ids = m_elementsStore->docElementIds(ElementsStore::elementDocKeyForChapter(c.manuscriptId, c.id));
            for (const QString& id : ids) {
                const Element* e = m_elementsStore->findElement(id);
                if (!e || e->type != QStringLiteral("character")) continue;
                presence[id] += 1;
                if (!entrance.contains(id)) { entrance.insert(id, where.value(c.id)); order << id; }
            }
        }
        if (!order.isEmpty()) {
            ornament();
            cl->addWidget(center(label(tr("Elenco").toUpper(), card, caps(10, 4), gold)));
            cl->addWidget(center(label(tr("por ordem de entrada em cena"), card, fontOf(QStringLiteral("Source Serif 4"), 11, QFont::Normal, true), alpha(paper, 0.45))));
            cl->addSpacing(4);
            const int shown = std::min<int>(order.size(), 14);
            for (int i = 0; i < shown; ++i) {
                const QString name = elementName(order.at(i));
                const QString ent = entrance.value(order.at(i));
                auto* row = new MsRow(card);
                row->setRowHeight(22);
                row->setCursor(Qt::ArrowCursor);
                row->setToolTip(tr("Em cena em %n capítulo(s)", "", presence.value(order.at(i))));
                row->setPainter([name, ent, ink, faint, gold](QPainter& p, const QRect& r, const MsRow* self) {
                    if (self->isHovered()) p.fillRect(r, alpha(gold, 0.08));
                    p.setFont(fontOf(MsFonts::bodoni(), 14, QFont::Medium));
                    p.setPen(ink);
                    const int nw = std::min(p.fontMetrics().horizontalAdvance(name), r.width() / 2);
                    p.drawText(QRect(r.left() + 4, r.top(), nw, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                               p.fontMetrics().elidedText(name, Qt::ElideRight, nw));
                    p.setFont(fontOf(QStringLiteral("Source Serif 4"), 12, QFont::Normal, true));
                    const int ew = p.fontMetrics().horizontalAdvance(ent);
                    p.setPen(faint);
                    p.drawText(QRect(r.right() - 4 - ew, r.top(), ew + 2, r.height()), Qt::AlignRight | Qt::AlignVCenter, ent);
                    QPen dots(alpha(gold, 0.4), 1, Qt::DotLine);
                    p.setPen(dots);
                    const int y = r.center().y() + 4;
                    if (r.right() - 10 - ew > r.left() + nw + 12) p.drawLine(r.left() + nw + 10, y, r.right() - 10 - ew, y);
                });
                cl->addWidget(row);
            }
            if (order.size() > shown)
                cl->addWidget(center(label(tr("e mais %n", "", order.size() - shown), card, fontOf(QStringLiteral("Source Serif 4"), 11, QFont::Normal, true), faint)));
        }
    }
    ornament();

    // Uma linha do programa: cena numerada com a rubrica, ou bloco centrado.
    auto sceneRow = [&](const Chapter& c, int k) {
        const bool cur = c.id == m_curChapterId;
        const bool empty = chapterWords(c.id) <= 0;
        const QString title = chapterTitleOnly(c);
        QString rub = c.timeMarker.trimmed();
        const QString pov = povOf(c);
        if (!pov.isEmpty()) rub += (rub.isEmpty() ? QString() : kSep()) + tr("%1 em cena").arg(elementName(pov));
        if (rub.isEmpty() && empty) rub = tr("a escrever");
        auto* row = new MsRow(card);
        row->setRowHeight(rub.isEmpty() ? 26 : 42);
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            if (cur) p.fillRect(r, alpha(gold, 0.13));
            else if (self->isHovered()) p.fillRect(r, alpha(gold, 0.07));
            p.setPen(gold);
            p.setFont(ui(10));
            if (cur) p.drawText(QRect(r.left() + 2, r.top() + 4, 12, 18), Qt::AlignCenter, QStringLiteral("✦"));
            p.setFont(fontOf(QStringLiteral("Source Serif 4"), 12));
            p.setPen(faint);
            const QString n = QString::number(k) + QStringLiteral(".");
            const int nw = p.fontMetrics().horizontalAdvance(n);
            p.drawText(QRect(r.left() + 16, r.top() + 4, nw + 2, 20), Qt::AlignLeft | Qt::AlignVCenter, n);
            p.setFont(fontOf(QStringLiteral("Source Serif 4"), 14.5, QFont::Normal, empty));
            p.setPen(empty ? faint : ink);
            const int tx = r.left() + 20 + nw, tw = r.right() - 6 - tx;
            p.drawText(QRect(tx, r.top() + 4, tw, 20), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(title, Qt::ElideRight, tw));
            if (!rub.isEmpty()) {
                p.setFont(fontOf(QStringLiteral("Source Serif 4"), 12, QFont::Normal, true));
                p.setPen(faint);
                p.drawText(QRect(tx, r.top() + 22, tw, 16), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(rub, Qt::ElideRight, tw));
            }
        });
        return row;
    };
    auto blockRow = [&](const Chapter& c, const QString& head) {
        const bool cur = c.id == m_curChapterId;
        QString line = c.title.trimmed();
        if (!c.timeMarker.trimmed().isEmpty()) line += (line.isEmpty() ? QString() : kSep()) + c.timeMarker.trimmed();
        auto* row = new MsRow(card);
        row->setRowHeight(line.isEmpty() ? 30 : 46);
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            if (cur) p.fillRect(r, alpha(gold, 0.13));
            else if (self->isHovered()) p.fillRect(r, alpha(gold, 0.07));
            p.setPen(QPen(alpha(gold, 0.25), 1));
            p.drawLine(r.left() + 4, r.top(), r.right() - 4, r.top());
            p.drawLine(r.left() + 4, r.bottom(), r.right() - 4, r.bottom());
            p.setFont(caps(10, 4));
            p.setPen(gold);
            p.drawText(QRect(r.left(), r.top() + 5, r.width(), 18), Qt::AlignCenter, head.toUpper());
            if (!line.isEmpty()) {
                p.setFont(fontOf(QStringLiteral("Source Serif 4"), 13, QFont::Normal, true));
                p.setPen(faint);
                p.drawText(QRect(r.left() + 6, r.top() + 23, r.width() - 12, 18), Qt::AlignCenter,
                           p.fontMetrics().elidedText(line, Qt::ElideRight, r.width() - 12));
            }
        });
        return row;
    };
    auto wire = [this](MsRow* row, const Chapter& c) {
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString chapterId = c.id, manuscriptId = c.manuscriptId;
        connect(row, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() { emit chapterActivated(manuscriptId, chapterId); });
        connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId](const QPoint& pos) {
            showChapterContextMenu(manuscriptId, chapterId, row->mapToGlobal(pos));
        });
        wireRow(row, QStringLiteral("chapter"), chapterId);
    };
    auto actHeader = [&](int ai, const QString& title, int wordsInAct) {
        auto* row = new MsRow(card);
        row->setRowHeight(30);
        row->setCursor(Qt::ArrowCursor);
        const QString act = tr("Ato %1").arg(ProjectModel::toRomanNumeral(ai + 1)).toUpper();
        const QString wtxt = MsPaint::fmtInt(wordsInAct);
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow*) {
            p.setFont(caps(11, 3.4));
            p.setPen(gold);
            const int aw = p.fontMetrics().horizontalAdvance(act);
            p.drawText(QRect(r.left() + 4, r.top(), aw + 2, r.height() - 2), Qt::AlignLeft | Qt::AlignBottom, act);
            p.setFont(mono(10));
            const int ww = p.fontMetrics().horizontalAdvance(wtxt);
            p.setPen(alpha(paper, 0.35));
            p.drawText(QRect(r.right() - 4 - ww, r.top(), ww + 2, r.height() - 3), Qt::AlignRight | Qt::AlignBottom, wtxt);
            p.setFont(fontOf(MsFonts::bodoni(), 17, QFont::Medium, true));
            p.setPen(paper);
            const int tx = r.left() + 12 + aw, tw = r.right() - 12 - ww - tx;
            p.drawText(QRect(tx, r.top(), tw, r.height() - 1), Qt::AlignLeft | Qt::AlignBottom, p.fontMetrics().elidedText(title, Qt::ElideRight, tw));
        });
        cl->addSpacing(8);
        cl->addWidget(row);
    };

    int curAct = -2;
    int k = 0;
    for (const auto& c : reading) {
        const int ai = parts.isEmpty() ? 0 : pidx.value(c.id, -1);
        if (ai != curAct) {
            curAct = ai;
            k = 0;
            if (ai >= 0) {
                int w = 0;
                for (const auto& x : reading) if ((parts.isEmpty() ? 0 : pidx.value(x.id, -1)) == ai) w += chapterWords(x.id);
                actHeader(ai, parts.isEmpty() ? QString() : partLabel(parts.at(ai)), w);
            }
        }
        MsRow* row = nullptr;
        if (c.type == QStringLiteral("interlude")) row = blockRow(c, tr("Intervalo"));
        else if (c.type != QStringLiteral("chapter")) row = blockRow(c, m_model->chapterTypeName(c));
        else row = sceneRow(c, ++k);
        wire(row, c);
        cl->addWidget(row);
    }
    cl->addSpacing(10);
    const int total = m_wordCounter ? m_wordCounter->countManuscript(m->id) : 0;
    auto* foot = center(label(tr("%1 palavras").arg(MsPaint::fmtInt(total)) + kSep() + tr("%n cena(s) no programa", "", reading.size()),
                              card, mono(10.5), alpha(paper, 0.35)));
    cl->addWidget(foot);
    add(card);
}

// ============================================================ Colunas

void ManuscriptPanel::buildColumnsView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    bool found = false;
    for (const auto& c : chs) found = found || c.id == m_colSel;
    if (!found) {
        m_colSel = chs.isEmpty() ? QString() : chs.first().id;
        for (const auto& c : chs) if (c.id == m_curChapterId) m_colSel = c.id;
    }
    const QList<Chapter> reading = readingChapters();
    const QList<ManuscriptPart> parts = partsForDisplay(reading);
    const QHash<QString, int> pidx = partIndexByChapter(reading, parts);
    int curPart = -2;
    for (const auto& c : chs) {
        const int pi = parts.isEmpty() ? -1 : pidx.value(c.id, -1);
        if (pi != curPart) {
            curPart = pi;
            if (pi >= 0) add(makeSimplePartRow(parts.at(pi), themeInk()));
        }
        const bool special = c.type != QStringLiteral("chapter");
        const QString num = special ? QStringLiteral("·") : m_model->chapterNumberLabel(c);
        const QString title = special ? m_model->chapterTypeName(c) + (c.title.isEmpty() ? QString() : QStringLiteral(": ") + c.title)
                                      : chapterTitleOnly(c);
        const QString chapterId = c.id;
        const bool open = c.id == m_curChapterId;
        auto* row = new MsRow(this);
        row->setRowHeight(32);
        row->setToolTip(tr("Clique pra ver as cenas · duplo clique abre"));
        row->setPainter([this, chapterId, num, title, open](QPainter& p, const QRect& r, const MsRow* self) {
            p.setRenderHint(QPainter::Antialiasing);
            const bool sel = (m_colSel == chapterId);
            if (sel || self->isHovered()) {
                p.setPen(Qt::NoPen);
                p.setBrush(tcol(Theme::hoverOverlay()));
                p.drawRoundedRect(QRectF(r).adjusted(4, 1, -4, -1), 5, 5);
            }
            p.setFont(ui(12));
            p.setPen(tcol(Theme::textMuted()));
            p.drawText(QRect(r.left() + 8, r.top(), 18, r.height()), Qt::AlignRight | Qt::AlignVCenter, num);
            p.setFont(serif(14));
            p.setPen(sel || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textPrimary()));
            const int tx = r.left() + 34, tw = r.right() - 22 - tx - (open ? 14 : 0);
            const QString t = p.fontMetrics().elidedText(title, Qt::ElideRight, tw);
            p.drawText(QRect(tx, r.top(), tw, r.height()), Qt::AlignLeft | Qt::AlignVCenter, t);
            if (open) {
                p.setPen(Qt::NoPen);
                p.setBrush(tcol(Theme::accentDefault()));
                p.drawEllipse(QPointF(tx + p.fontMetrics().horizontalAdvance(t) + 9, r.center().y() + 0.5), 3, 3);
            }
            if (sel) {
                p.setPen(tcol(Theme::textMuted()));
                p.setFont(ui(13));
                p.drawText(QRect(r.right() - 20, r.top(), 12, r.height()), Qt::AlignCenter, QStringLiteral("›"));
            }
        });
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString manuscriptId = c.manuscriptId;
        connect(row, &QToolButton::clicked, this, [this, chapterId]() {
            if (m_colSel == chapterId) return;
            m_colSel = chapterId;
            for (auto* r : m_scroll->widget()->findChildren<MsRow*>()) r->update();
            rebuildColumnsSide();
        });
        connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId](const QPoint& pos) {
            showChapterContextMenu(manuscriptId, chapterId, row->mapToGlobal(pos));
        });
        wireRow(row, QStringLiteral("chapter"), chapterId);
        row->setProperty("dblOpen", true);
        add(row);
    }
    rebuildColumnsSide();
}

void ManuscriptPanel::rebuildColumnsSide() {
    clearLayout(m_sideLayout);
    if (m_style != Style::Columns || !m_model) { m_side->hide(); return; }
    const Chapter* c = m_model->findChapter(m_colSel);
    if (!c) { m_side->hide(); return; }
    m_side->setFixedWidth(std::max(130, int(width() * 0.42)));
    auto* sc = new QScrollArea(m_side);
    sc->setFrameShape(QFrame::NoFrame);
    sc->setWidgetResizable(true);
    sc->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sc->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    sc->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* host = new QWidget(sc);
    host->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* hl = new QVBoxLayout(host);
    hl->setContentsMargins(0, 4, 0, 4);
    hl->setSpacing(0);
    const bool special = c->type != QStringLiteral("chapter");
    const QString head = special ? m_model->chapterTypeName(*c).toUpper() : tr("Capítulo %1").arg(m_model->chapterNumberLabel(*c)).toUpper();
    auto* h = label(head, host, caps(9.5, 2.0), tcol(Theme::textMuted()));
    h->setContentsMargins(12, 6, 8, 4);
    hl->addWidget(h);
    const QString msId = c->manuscriptId, chId = c->id;
    auto sceneRow = [&](const QString& text, int idx, bool italic, bool open) {
        auto* row = new MsRow(host);
        row->setRowHeight(30);
        row->setPainter([text, italic, open](QPainter& p, const QRect& r, const MsRow* self) {
            if (self->isHovered()) p.fillRect(r, tcol(Theme::hoverOverlay()));
            if (open) p.fillRect(QRect(r.left(), r.top() + 3, 2, r.height() - 6), tcol(Theme::accentDefault()));
            p.setFont(serif(13.5, QFont::Normal, italic));
            p.setPen(open || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textPrimary()));
            p.drawText(r.adjusted(12, 0, -8, 0), Qt::AlignLeft | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(text, Qt::ElideRight, r.width() - 20));
        });
        connect(row, &QToolButton::clicked, this, [this, msId, chId, idx]() {
            if (idx < 0) emit chapterActivated(msId, chId); else emit sceneActivated(msId, chId, idx);
        });
        if (idx >= 0) {
            row->setToolTip(tr("%1 palavras").arg(MsPaint::fmtInt(sceneWords(chId, idx))));
            row->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(row, &QToolButton::customContextMenuRequested, this, [this, row, msId, chId, idx](const QPoint& pos) {
                showSceneContextMenu(msId, chId, idx, row->mapToGlobal(pos));
            });
        }
        hl->addWidget(row);
    };
    if (c->scenes.size() > 1) {
        for (int i = 0; i < c->scenes.size(); ++i) sceneRow(sceneTitle(*c, i), i, true, isCurrent(chId, i));
    } else {
        const bool empty = chapterWords(chId) <= 0;
        sceneRow(empty ? tr("Começar a escrever") : tr("Abrir capítulo"), -1, false, chId == m_curChapterId);
        auto* note = label(empty ? tr("Capítulo vazio.") : tr("Capítulo de uma cena só."), host,
                           fontOf(QStringLiteral("Source Serif 4"), 12.5, QFont::Normal, true), tcol(Theme::textMuted()), true);
        note->setContentsMargins(12, 6, 8, 0);
        hl->addWidget(note);
    }
    hl->addStretch(1);
    sc->setWidget(host);
    m_sideLayout->addWidget(sc, 1);
    m_side->show();
}

// ============================================================ Tambor

void ManuscriptPanel::buildDrumTop() {
    const QList<Chapter> reading = readingChapters();
    if (reading.isEmpty()) return;
    m_topLayout->setContentsMargins(0, 4, 0, 0);
    QList<MsDrum::Item> items;
    int cur = 0;
    bool found = false;
    for (int i = 0; i < reading.size(); ++i) {
        const Chapter& c = reading.at(i);
        MsDrum::Item it;
        const bool special = c.type != QStringLiteral("chapter");
        it.number = special ? QString() : m_model->chapterNumberLabel(c);
        it.label = special ? m_model->chapterTypeName(c).toUpper() : tr("Capítulo %1").arg(it.number).toUpper();
        it.title = chapterTitleOnly(c);
        it.empty = chapterWords(c.id) <= 0;
        items << it;
        if (c.id == m_drumSel) { cur = i; found = true; }
    }
    if (!found) {
        for (int i = 0; i < reading.size(); ++i) if (reading.at(i).id == m_curChapterId) cur = i;
        m_drumSel = reading.at(cur).id;
    }
    m_drum = new MsDrum(m_top);
    m_drum->setItems(items, cur, tcol(Theme::accentDefault()));
    m_drum->setToolTip(tr("Rodinha do mouse, arrastar ou as setas · Enter abre · digite o número pra pular"));
    connect(m_drum, &MsDrum::currentChanged, this, [this](int i) {
        const QList<Chapter> r = readingChapters();
        if (i < 0 || i >= r.size()) return;
        m_drumSel = r.at(i).id;
        rebuildBody();
        if (m_drumCount) m_drumCount->setText(tr("<b>%1</b> de %2").arg(i + 1).arg(r.size()));
    });
    connect(m_drum, &MsDrum::activated, this, [this](int i) {
        const QList<Chapter> r = readingChapters();
        if (i >= 0 && i < r.size()) emit chapterActivated(activeManuscriptId(), r.at(i).id);
    });
    m_topLayout->addWidget(m_drum);
}

void ManuscriptPanel::buildDrumView() {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const Chapter* c = m_model->findChapter(m_drumSel);
    if (!c) return;
    const QString msId = c->manuscriptId, chId = c->id;
    auto row = [&](const QString& text, int idx, bool open) {
        auto* r = new MsRow(this);
        r->setRowHeight(28);
        r->setPainter([text, open](QPainter& p, const QRect& rr, const MsRow* self) {
            p.setFont(serif(14, QFont::Normal, true));
            p.setPen(open || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textMuted()));
            const QString t = open ? QStringLiteral("·  ") + text : text;
            p.drawText(rr, Qt::AlignCenter, p.fontMetrics().elidedText(t, Qt::ElideRight, rr.width() - 20));
        });
        connect(r, &QToolButton::clicked, this, [this, msId, chId, idx]() {
            if (idx < 0) emit chapterActivated(msId, chId); else emit sceneActivated(msId, chId, idx);
        });
        if (idx >= 0) {
            r->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(r, &QToolButton::customContextMenuRequested, this, [this, r, msId, chId, idx](const QPoint& pos) {
                showSceneContextMenu(msId, chId, idx, r->mapToGlobal(pos));
            });
            wireRow(r, QStringLiteral("scene"), chId, idx);
        }
        add(r);
    };
    if (c->scenes.size() > 1) {
        for (int i = 0; i < c->scenes.size(); ++i) row(sceneTitle(*c, i), i, isCurrent(chId, i));
    } else {
        const bool empty = chapterWords(chId) <= 0;
        auto* note = label(empty ? tr("capítulo vazio") : tr("uma cena só"), this,
                           fontOf(QStringLiteral("Source Serif 4"), 12.5, QFont::Normal, true), tcol(Theme::textMuted()));
        note->setAlignment(Qt::AlignCenter);
        note->setContentsMargins(0, 6, 0, 2);
        add(note);
        row(empty ? tr("Começar a escrever") : tr("Abrir capítulo"), -1, chId == m_curChapterId);
    }
}

// ============================================================ Camadas

void ManuscriptPanel::layersGo(const QString& ms, const QString& part, const QString& chapter) {
    const bool animate = isVisible();
    if (animate) PanelMotion::swapOut(m_scroll->viewport());
    m_layerSaga = ms.isEmpty();
    if (!ms.isEmpty() && ms != activeManuscriptId()) {
        m_layerMs = ms;              // antes de trocar: o onComboChanged não zera o caminho
        selectManuscript(ms);
    }
    m_layerMs = ms.isEmpty() ? activeManuscriptId() : ms;
    m_layerPart = part;
    m_layerChapter = chapter;
    rebuildList();
    if (animate) playIntro(40);
}

void ManuscriptPanel::buildLayersTop() {
    const QString active = activeManuscriptId();
    if (!m_layerInit || m_layerMs != active) {
        m_layerInit = true;
        m_layerSaga = false;
        m_layerMs = active;
        m_layerChapter.clear();
        m_layerPart.clear();
        const QList<Chapter> reading = readingChapters();
        const QList<ManuscriptPart> parts = partsForDisplay(reading);
        const int pi = parts.isEmpty() ? -1 : partIndexByChapter(reading, parts).value(m_curChapterId, -1);
        if (pi >= 0) m_layerPart = parts.at(pi).id;
    }
    auto* w = new QWidget(m_top);
    auto* l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    const QColor muted = tcol(Theme::textMuted()), bright = tcol(Theme::textBright());
    struct Crumb { QString text; QString ms, part, ch; bool saga; };
    QList<Crumb> crumbs;
    crumbs << Crumb{ tr("Saga"), QString(), QString(), QString(), true };
    const Manuscript* m = m_model->findManuscript(active);
    if (!m_layerSaga && m) {
        crumbs << Crumb{ m->title.isEmpty() ? tr("(sem título)") : m->title, active, QString(), QString(), false };
        const QList<Chapter> reading = readingChapters();
        for (const auto& p : partsForDisplay(reading))
            if (p.id == m_layerPart) crumbs << Crumb{ partLabel(p), active, p.id, QString(), false };
        if (const Chapter* c = m_model->findChapter(m_layerChapter))
            crumbs << Crumb{ c->type == QStringLiteral("chapter") ? tr("Cap. %1").arg(m_model->chapterNumberLabel(*c)) : m_model->chapterTypeName(*c),
                             active, m_layerPart, c->id, false };
    }
    for (int i = 0; i < crumbs.size(); ++i) {
        const Crumb cr = crumbs.at(i);
        const bool last = i == crumbs.size() - 1;
        if (i > 0) l->addWidget(label(QStringLiteral("›"), w, ui(12), alpha(muted, 0.7)));
        const QString text = QFontMetrics(ui(12)).elidedText(cr.text, Qt::ElideRight, 120);
        auto* b = textButton(text, w, last ? bright : muted, bright, 12, last);
        b->setToolTip(cr.text);
        if (last) b->setCursor(Qt::ArrowCursor);
        else connect(b, &QToolButton::clicked, this, [this, cr]() { layersGo(cr.saga ? QString() : cr.ms, cr.part, cr.ch); });
        l->addWidget(b);
    }
    l->addStretch(1);
    m_topLayout->setContentsMargins(8, 8, 8, 6);
    m_topLayout->addWidget(w);
}

void ManuscriptPanel::buildLayersView() {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QColor accent = tcol(Theme::accentDefault());
    auto title = [&](const QString& t) {
        auto* l = label(t, this, serif(17, QFont::Normal, true), tcol(Theme::textBright()), true);
        l->setContentsMargins(8, 6, 8, 6);
        add(l);
    };
    // Linha de um nível: ponto (caminho até o que está aberto), nome, conta e seta.
    auto row = [&](const QString& text, const QString& meta, bool here, bool drill, bool leaf, bool open) {
        auto* r = new MsRow(this);
        r->setRowHeight(38);
        r->setPainter([=](QPainter& p, const QRect& rr, const MsRow* self) {
            p.setRenderHint(QPainter::Antialiasing);
            if (self->isHovered()) {
                p.setPen(Qt::NoPen);
                p.setBrush(tcol(Theme::hoverOverlay()));
                p.drawRoundedRect(QRectF(rr).adjusted(2, 1, -2, -1), 5, 5);
            }
            int x = rr.left() + 12;
            if (here) {
                p.setPen(Qt::NoPen);
                p.setBrush(accent);
                p.drawEllipse(QPointF(x + 3, rr.center().y() + 0.5), 3, 3);
            }
            x += 14;
            int right = rr.right() - 10;
            if (drill) {
                p.setFont(ui(14));
                p.setPen(tcol(Theme::textMuted()));
                p.drawText(QRect(right - 10, rr.top(), 10, rr.height()), Qt::AlignCenter, QStringLiteral("›"));
                right -= 18;
            }
            if (!meta.isEmpty()) {
                p.setFont(ui(11));
                const int mw = p.fontMetrics().horizontalAdvance(meta);
                p.setPen(tcol(Theme::textMuted()));
                p.drawText(QRect(right - mw, rr.top(), mw + 2, rr.height()), Qt::AlignRight | Qt::AlignVCenter, meta);
                right -= mw + 10;
            }
            p.setFont(serif(14.5, QFont::Normal, leaf));
            p.setPen(open || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textPrimary()));
            p.drawText(QRect(x, rr.top(), right - x, rr.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(text, Qt::ElideRight, right - x));
        });
        add(r);
        return r;
    };

    const QString active = activeManuscriptId();
    const Chapter* openCh = m_model->findChapter(m_curChapterId);
    if (m_layerSaga) {
        title(tr("A saga"));
        const auto& mss = m_model->manuscripts();
        for (int i = 0; i < mss.size(); ++i) {
            const Manuscript& m = mss.at(i);
            int n = 0;
            for (const auto& c : m_model->chapters()) if (c.manuscriptId == m.id) ++n;
            auto* r = row(QStringLiteral("%1. %2").arg(i + 1).arg(m.title.isEmpty() ? tr("(sem título)") : m.title),
                          tr("%n cap.", "", n), openCh && openCh->manuscriptId == m.id, true, false, false);
            const QString id = m.id;
            r->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(r, &QToolButton::clicked, this, [this, id]() { layersGo(id, QString(), QString()); });
            connect(r, &QToolButton::customContextMenuRequested, this, [this, r, id](const QPoint& pos) { showManuscriptContextMenu(id, r->mapToGlobal(pos)); });
        }
        add(makeAddRow(tr("Novo manuscrito"), QStringLiteral("addBook")));
        return;
    }
    const QList<Chapter> reading = readingChapters();
    const QList<ManuscriptPart> parts = partsForDisplay(reading);
    const QHash<QString, int> pidx = partIndexByChapter(reading, parts);
    // Cenas de um capítulo
    if (const Chapter* c = m_model->findChapter(m_layerChapter)) {
        title(chapterRowText(*c));
        for (int i = 0; i < c->scenes.size(); ++i) {
            const bool open = isCurrent(c->id, i);
            auto* r = row(sceneTitle(*c, i), MsPaint::fmtInt(sceneWords(c->id, i)), open, false, true, open);
            const QString msId = c->manuscriptId, chId = c->id;
            r->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(r, &QToolButton::clicked, this, [this, msId, chId, i]() { emit sceneActivated(msId, chId, i); });
            connect(r, &QToolButton::customContextMenuRequested, this, [this, r, msId, chId, i](const QPoint& pos) {
                showSceneContextMenu(msId, chId, i, r->mapToGlobal(pos));
            });
            wireRow(r, QStringLiteral("scene"), chId, i);
        }
        return;
    }
    // Partes do livro
    if (!parts.isEmpty() && m_layerPart.isEmpty()) {
        const Manuscript* m = m_model->findManuscript(active);
        title(m && !m->title.isEmpty() ? m->title : tr("(sem título)"));
        QHash<int, int> counts;
        for (const auto& c : reading) counts[pidx.value(c.id, -1)] += 1;
        if (counts.value(-1) > 0) {
            // capítulos antes da primeira Parte aparecem soltos, em cima
            for (const auto& c : reading) {
                if (pidx.value(c.id, -1) >= 0) continue;
                const bool open = c.id == m_curChapterId;
                auto* r = row(chapterRowText(c), QString(), open, false, true, open);
                const QString msId = c.manuscriptId, chId = c.id;
                connect(r, &QToolButton::clicked, this, [this, msId, chId]() { emit chapterActivated(msId, chId); });
                wireRow(r, QStringLiteral("chapter"), chId);
            }
        }
        for (int i = 0; i < parts.size(); ++i) {
            const ManuscriptPart& p = parts.at(i);
            const bool here = openCh && pidx.value(openCh->id, -1) == i;
            auto* r = row(partLabel(p), tr("%n cap.", "", counts.value(i)), here, true, false, false);
            const QString pid = p.id;
            r->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(r, &QToolButton::clicked, this, [this, active, pid]() { layersGo(active, pid, QString()); });
            connect(r, &QToolButton::customContextMenuRequested, this, [this, r, pid](const QPoint& pos) { showPartContextMenu(pid, r->mapToGlobal(pos)); });
        }
        return;
    }
    // Capítulos (de uma Parte, ou do livro sem Partes)
    QString heading;
    int partIdx = -1;
    for (int i = 0; i < parts.size(); ++i) if (parts.at(i).id == m_layerPart) { heading = partLabel(parts.at(i)); partIdx = i; }
    if (heading.isEmpty()) {
        const Manuscript* m = m_model->findManuscript(active);
        heading = m && !m->title.isEmpty() ? m->title : tr("(sem título)");
    }
    title(heading);
    for (const auto& c : reading) {
        if (partIdx >= 0 && pidx.value(c.id, -1) != partIdx) continue;
        const bool multi = c.scenes.size() > 1;
        const bool open = c.id == m_curChapterId;
        auto* r = row(chapterRowText(c), multi ? tr("%n cena(s)", "", c.scenes.size()) : (chapterWords(c.id) > 0 ? QString() : tr("vazio")),
                      open, multi, false, open);
        const QString msId = c.manuscriptId, chId = c.id, part = m_layerPart;
        r->setContextMenuPolicy(Qt::CustomContextMenu);
        if (multi) connect(r, &QToolButton::clicked, this, [this, active, part, chId]() { layersGo(active, part, chId); });
        else connect(r, &QToolButton::clicked, this, [this, msId, chId]() { emit chapterActivated(msId, chId); });
        connect(r, &QToolButton::customContextMenuRequested, this, [this, r, msId, chId](const QPoint& pos) {
            showChapterContextMenu(msId, chId, r->mapToGlobal(pos));
        });
        wireRow(r, QStringLiteral("chapter"), chId);
    }
    add(makeAddRow(tr("Novo capítulo"), QStringLiteral("addChapter")));
}

// ============================================================ Comando

void ManuscriptPanel::buildCommandTop() {
    if (activeManuscriptId().isEmpty()) return;
    m_topLayout->setContentsMargins(10, 10, 10, 2);
    m_topLayout->setSpacing(8);
    const QColor accent = tcol(Theme::accentDefault());
    // "Você está em": fixo no topo, sempre visível; clicar rola até a linha.
    if (const Chapter* c = m_model->findChapter(m_curChapterId)) {
        if (c->manuscriptId == activeManuscriptId()) {
            const QString title = chapterRowText(*c);
            const QString sceneTxt = (m_curScene >= 0 && c->scenes.size() > 1)
                ? tr("cena %1 de %2").arg(m_curScene + 1).arg(c->scenes.size()) : QString();
            auto* here = new MsRow(m_top);
            here->setRowHeight(46);
            here->setToolTip(tr("Mostrar na lista"));
            const QString caption = tr("Você está em").toUpper();
            here->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
                p.setRenderHint(QPainter::Antialiasing);
                QPainterPath path;
                path.addRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
                QColor bg = tcol(Theme::accentInfoSoft());
                if (self->isHovered()) bg = bg.lighter(115);
                p.fillPath(path, bg);
                p.save();
                p.setClipPath(path);
                p.fillRect(QRect(r.left(), r.top(), 3, r.height()), accent);
                p.restore();
                p.setFont(caps(8.5, 1.8));
                p.setPen(tcol(Theme::textMuted()));
                p.drawText(QRect(r.left() + 12, r.top() + 6, r.width() - 20, 14), Qt::AlignLeft | Qt::AlignVCenter, caption);
                p.setFont(ui(11));
                const int sw = p.fontMetrics().horizontalAdvance(sceneTxt);
                p.setPen(tcol(Theme::textPrimary()));
                p.drawText(QRect(r.right() - 10 - sw, r.top() + 20, sw + 2, 20), Qt::AlignRight | Qt::AlignVCenter, sceneTxt);
                p.setFont(serif(14.5, QFont::DemiBold));
                p.setPen(tcol(Theme::textBright()));
                const int tw = r.width() - 24 - sw - 8;
                p.drawText(QRect(r.left() + 12, r.top() + 20, tw, 20), Qt::AlignLeft | Qt::AlignVCenter,
                           p.fontMetrics().elidedText(title, Qt::ElideRight, tw));
            });
            connect(here, &QToolButton::clicked, this, [this]() {
                m_cmdQuery.clear();
                if (m_cmdEdit) { QSignalBlocker b(m_cmdEdit); m_cmdEdit->clear(); }
                m_cmdKb = -1;
                m_cmdExpanded.insert(m_curChapterId);
                rebuildBody();
                QTimer::singleShot(0, this, [this]() {
                    for (auto* w : m_scroll->widget()->findChildren<QWidget*>())
                        if (w->property("cmdOpen").toBool()) { m_scroll->ensureWidgetVisible(w, 0, 60); break; }
                });
            });
            m_topLayout->addWidget(here);
        }
    }
    m_cmdEdit = new QLineEdit(m_top);
    m_cmdEdit->setPlaceholderText(tr("Filtrar por nome, número ou resumo"));
    m_cmdEdit->setClearButtonEnabled(true);
    m_cmdEdit->setText(m_cmdQuery);
    m_cmdEdit->setStyleSheet(Theme::qss(QStringLiteral(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: @radius-control; padding: 6px 10px; font-size: 13px; }"
        "QLineEdit:focus { border-color: %4; }"))
        .arg(Theme::inputBackground(), Theme::textBright(), Theme::panelBorder(), Theme::accentInfoBorderSoft()));
    m_cmdEdit->installEventFilter(this);
    connect(m_cmdEdit, &QLineEdit::textChanged, this, [this](const QString& t) {
        m_cmdQuery = t;
        m_cmdKb = t.trimmed().isEmpty() ? -1 : 0;
        rebuildBody();
    });
    m_topLayout->addWidget(m_cmdEdit);

    // Voltar pra: os lugares anteriores, sem repetir o aberto.
    QList<ResumeEntry> places;
    QSet<QString> seen;
    seen.insert(m_curChapterId);
    for (const auto& e : m_resume) {
        if (e.manuscriptId != activeManuscriptId() || seen.contains(e.chapterId) || !m_model->findChapter(e.chapterId)) continue;
        seen.insert(e.chapterId);
        places << e;
        if (places.size() == 3) break;
    }
    if (!places.isEmpty()) {
        auto* w = new QWidget(m_top);
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(5);
        l->addWidget(label(tr("Voltar pra").toUpper(), w, caps(8.5, 1.6), tcol(Theme::textMuted())));
        for (const auto& e : places) {
            const Chapter* c = m_model->findChapter(e.chapterId);
            QString t = chapterShortLabel(*c) + kSep() + chapterTitleOnly(*c);
            if (e.sceneIndex >= 0 && c->scenes.size() > 1) t += kSep() + tr("cena %1").arg(e.sceneIndex + 1);
            auto* b = new QToolButton(w);
            b->setText(QFontMetrics(serif(12)).elidedText(t, Qt::ElideRight, 150));
            b->setToolTip(t);
            b->setCursor(Qt::PointingHandCursor);
            b->setStyleSheet(QStringLiteral(
                "QToolButton { color: %1; background: transparent; border: 1px solid %2; border-radius: 10px;"
                " padding: 1px 8px; font-family: 'Lora'; font-size: 12px; }"
                "QToolButton:hover { color: %3; background: %4; border-color: %5; }")
                .arg(Theme::textPrimary(), Theme::subtleBorder(), Theme::textBright(), Theme::hoverOverlay(), Theme::borderStrong()));
            const ResumeEntry ee = e;
            connect(b, &QToolButton::clicked, this, [this, ee]() {
                if (ee.sceneIndex >= 0) emit sceneActivated(ee.manuscriptId, ee.chapterId, ee.sceneIndex);
                else emit chapterActivated(ee.manuscriptId, ee.chapterId);
            });
            l->addWidget(b);
        }
        l->addStretch(1);
        m_topLayout->addWidget(w);
    }
}

void ManuscriptPanel::buildCommandView() {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    static QString s_expandedFor;
    if (s_expandedFor != m_curChapterId && !m_curChapterId.isEmpty()) {
        m_cmdExpanded.insert(m_curChapterId);   // o aberto começa com as cenas à mostra
        s_expandedFor = m_curChapterId;
    }
    const QString q = m_cmdQuery.trimmed().toLower();
    bool numOnly = !q.isEmpty();
    for (QChar ch : q) numOnly = numOnly && ch.isDigit();
    const QColor accent = tcol(Theme::accentDefault());
    const QList<Chapter> reading = readingChapters();
    const QList<ManuscriptPart> parts = partsForDisplay(reading);
    const QHash<QString, int> pidx = partIndexByChapter(reading, parts);
    m_cmdVis.clear();
    int curPart = -2;
    bool any = false;
    QList<QWidget*> pendingHeader;   // cabeçalho da Parte só entra se ela tiver linha
    for (const auto& c : reading) {
        const QString name = chapterRowText(c);
        const QString num = m_model->chapterNumberLabel(c);
        const bool byName = q.isEmpty() || (numOnly ? (num == q) : name.toLower().contains(q));
        const bool bySum = !q.isEmpty() && !numOnly && !byName && c.summary.toLower().contains(q);
        if (!byName && !bySum) continue;
        const int pi = parts.isEmpty() ? -1 : pidx.value(c.id, -1);
        if (pi != curPart) {
            curPart = pi;
            if (pi >= 0) add(makeSimplePartRow(parts.at(pi), themeInk()));
        }
        any = true;
        const bool multi = c.scenes.size() > 1;
        const bool expanded = multi && m_cmdExpanded.contains(c.id);
        const bool isOpen = c.id == m_curChapterId;
        const bool fullOpen = isOpen && (!expanded || m_curScene < 0);
        const int vi = m_cmdVis.size();
        m_cmdVis << qMakePair(c.id, -1);
        const bool kb = (vi == m_cmdKb);
        const QString special = c.type != QStringLiteral("chapter") ? QStringLiteral("·") : num;
        const QString title = c.type != QStringLiteral("chapter")
            ? m_model->chapterTypeName(c) + (c.title.isEmpty() ? QString() : QStringLiteral(": ") + c.title) : chapterTitleOnly(c);
        const QString openTag = tr("aberto").toUpper();
        const QString sumTag = tr("no resumo");
        auto* row = new MsRow(this);
        row->setRowHeight(30);
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            p.setRenderHint(QPainter::Antialiasing);
            const QRectF box = QRectF(r).adjusted(4, 1, -4, -1);
            if (fullOpen) {
                p.setPen(Qt::NoPen);
                p.setBrush(tcol(Theme::accentInfoSoft()));
                p.drawRoundedRect(box, 5, 5);
                p.setBrush(accent);
                p.drawRoundedRect(QRectF(box.left(), box.top() + 2, 3, box.height() - 4), 1.5, 1.5);
            } else if (self->isHovered()) {
                p.setPen(Qt::NoPen);
                p.setBrush(tcol(Theme::hoverOverlay()));
                p.drawRoundedRect(box, 5, 5);
            }
            if (kb) {
                p.setPen(QPen(tcol(Theme::accentInfoBorderSoft()), 1.2));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(box, 5, 5);
            }
            if (multi) {
                p.setFont(ui(10));
                p.setPen(self->hoverPart() == 0 ? tcol(Theme::textBright()) : tcol(Theme::textMuted()));
                p.drawText(QRect(r.left() + 6, r.top(), 16, r.height()), Qt::AlignCenter, expanded ? QStringLiteral("▾") : QStringLiteral("▸"));
            }
            p.setFont(ui(12));
            p.setPen(isOpen ? accent : tcol(Theme::textMuted()));
            p.drawText(QRect(r.left() + 22, r.top(), 16, r.height()), Qt::AlignRight | Qt::AlignVCenter, special);
            int right = r.right() - 10;
            if (fullOpen) {
                p.setFont(caps(8.5, 1.2));
                const int tw = p.fontMetrics().horizontalAdvance(openTag);
                p.setPen(accent);
                p.drawText(QRect(right - tw, r.top(), tw + 2, r.height()), Qt::AlignRight | Qt::AlignVCenter, openTag);
                right -= tw + 8;
            }
            // título com o trecho buscado aceso
            p.setFont(serif(14));
            const QColor base = (isOpen || self->isHovered()) ? tcol(Theme::textBright()) : tcol(Theme::textPrimary());
            int x = r.left() + 46;
            const int avail = right - x - (bySum ? p.fontMetrics().horizontalAdvance(sumTag) + 10 : 0);
            const QString shown = p.fontMetrics().elidedText(title, Qt::ElideRight, avail);
            const int at = (q.isEmpty() || numOnly) ? -1 : shown.toLower().indexOf(q);
            auto piece = [&](const QString& s, const QColor& col, bool bold) {
                if (s.isEmpty()) return;
                QFont f = serif(14, bold ? QFont::DemiBold : QFont::Normal);
                p.setFont(f);
                p.setPen(col);
                const int w = p.fontMetrics().horizontalAdvance(s);
                p.drawText(QRect(x, r.top(), w + 2, r.height()), Qt::AlignLeft | Qt::AlignVCenter, s);
                x += w;
            };
            if (at < 0) piece(shown, base, false);
            else { piece(shown.left(at), base, false); piece(shown.mid(at, q.size()), accent, true); piece(shown.mid(at + q.size()), base, false); }
            if (bySum) {
                p.setFont(fontOf(QStringLiteral("Segoe UI"), 10.5, QFont::Normal, true));
                p.setPen(tcol(Theme::textMuted()));
                p.drawText(QRect(x + 8, r.top(), right - x - 8, r.height()), Qt::AlignLeft | Qt::AlignVCenter, sumTag);
            }
        });
        if (multi) row->setHitTest([](const QPoint& pt) { return pt.x() < 24 ? 0 : -1; });
        const QString chId = c.id, msId = c.manuscriptId;
        connect(row, &MsRow::partClicked, this, [this, chId](int) {
            if (m_cmdExpanded.contains(chId)) m_cmdExpanded.remove(chId); else m_cmdExpanded.insert(chId);
            rebuildBody();
        });
        connect(row, &QToolButton::clicked, this, [this, msId, chId]() {
            const Chapter* cc = m_model->findChapter(chId);
            if (cc && cc->scenes.size() > 1) { m_cmdExpanded.insert(chId); emit sceneActivated(msId, chId, 0); }
            else emit chapterActivated(msId, chId);
        });
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(row, &QToolButton::customContextMenuRequested, this, [this, row, msId, chId](const QPoint& pos) {
            showChapterContextMenu(msId, chId, row->mapToGlobal(pos));
        });
        wireRow(row, QStringLiteral("chapter"), chId);
        row->setProperty("cmdIndex", vi);
        if (fullOpen) row->setProperty("cmdOpen", true);
        add(row);
        if (expanded) {
            for (int j = 0; j < c.scenes.size(); ++j) {
                const int svi = m_cmdVis.size();
                m_cmdVis << qMakePair(c.id, j);
                const bool sOpen = isCurrent(c.id, j);
                const bool skb = (svi == m_cmdKb);
                const QString text = sceneTitle(c, j);
                const QString openS = tr("aberta").toUpper();
                auto* sr = new MsRow(this);
                sr->setRowHeight(26);
                sr->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
                    p.setRenderHint(QPainter::Antialiasing);
                    const QRectF box = QRectF(r).adjusted(4, 1, -4, -1);
                    if (sOpen) {
                        p.setPen(Qt::NoPen);
                        p.setBrush(tcol(Theme::accentInfoSoft()));
                        p.drawRoundedRect(box, 5, 5);
                        p.setBrush(accent);
                        p.drawRoundedRect(QRectF(box.left(), box.top() + 2, 3, box.height() - 4), 1.5, 1.5);
                    } else if (self->isHovered()) {
                        p.setPen(Qt::NoPen);
                        p.setBrush(tcol(Theme::hoverOverlay()));
                        p.drawRoundedRect(box, 5, 5);
                    }
                    if (skb) {
                        p.setPen(QPen(tcol(Theme::accentInfoBorderSoft()), 1.2));
                        p.setBrush(Qt::NoBrush);
                        p.drawRoundedRect(box, 5, 5);
                    }
                    int right = r.right() - 10;
                    if (sOpen) {
                        p.setFont(caps(8.5, 1.2));
                        const int tw = p.fontMetrics().horizontalAdvance(openS);
                        p.setPen(accent);
                        p.drawText(QRect(right - tw, r.top(), tw + 2, r.height()), Qt::AlignRight | Qt::AlignVCenter, openS);
                        right -= tw + 8;
                    }
                    p.setFont(serif(13, QFont::Normal, true));
                    p.setPen(sOpen || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textMuted()));
                    p.drawText(QRect(r.left() + 46, r.top(), right - r.left() - 46, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                               p.fontMetrics().elidedText(text, Qt::ElideRight, right - r.left() - 46));
                });
                connect(sr, &QToolButton::clicked, this, [this, msId, chId, j]() { emit sceneActivated(msId, chId, j); });
                sr->setContextMenuPolicy(Qt::CustomContextMenu);
                connect(sr, &QToolButton::customContextMenuRequested, this, [this, sr, msId, chId, j](const QPoint& pos) {
                    showSceneContextMenu(msId, chId, j, sr->mapToGlobal(pos));
                });
                wireRow(sr, QStringLiteral("scene"), chId, j);
                sr->setProperty("cmdIndex", svi);
                if (sOpen) sr->setProperty("cmdOpen", true);
                add(sr);
            }
        }
    }
    if (!any && !q.isEmpty()) {
        auto* w = new QWidget(this);
        auto* l = new QVBoxLayout(w);
        l->setContentsMargins(10, 14, 10, 8);
        l->setSpacing(6);
        l->addWidget(label(tr("Nenhum capítulo com \"%1\".").arg(m_cmdQuery.trimmed()), w,
                           fontOf(QStringLiteral("Source Serif 4"), 13, QFont::Normal, true), tcol(Theme::textMuted()), true));
        auto* go = textButton(tr("Buscar \"%1\" no texto do livro ›").arg(m_cmdQuery.trimmed()), w,
                              tcol(Theme::accentInfo()), tcol(Theme::textBright()), 12);
        const QString query = m_cmdQuery.trimmed();
        connect(go, &QToolButton::clicked, this, [this, query]() { emit searchTextRequested(query); });
        l->addWidget(go, 0, Qt::AlignLeft);
        add(w);
    } else if (q.isEmpty()) {
        add(makeAddRow(tr("Novo capítulo"), QStringLiteral("addChapter")));
    }
    if (m_cmdKb >= m_cmdVis.size()) m_cmdKb = m_cmdVis.isEmpty() ? -1 : 0;
}

void ManuscriptPanel::commandKey(int key) {
    const int n = m_cmdVis.size();
    if (key == Qt::Key_Escape) {
        if (m_cmdEdit && !m_cmdEdit->text().isEmpty()) { m_cmdEdit->clear(); return; }
        m_cmdKb = -1;
    } else if (key == Qt::Key_Down || key == Qt::Key_Up) {
        if (!n) return;
        m_cmdKb = m_cmdKb < 0 ? (key == Qt::Key_Down ? 0 : n - 1) : (m_cmdKb + (key == Qt::Key_Down ? 1 : -1) + n) % n;
    } else if (key == Qt::Key_Right || key == Qt::Key_Left) {
        if (m_cmdKb < 0 || m_cmdKb >= n) return;
        const auto cur = m_cmdVis.at(m_cmdKb);
        if (key == Qt::Key_Right) {
            const Chapter* c = m_model->findChapter(cur.first);
            if (cur.second >= 0 || !c || c->scenes.size() < 2) return;
            m_cmdExpanded.insert(cur.first);
        } else {
            if (!m_cmdExpanded.contains(cur.first)) return;
            m_cmdExpanded.remove(cur.first);
            // o cursor volta pra linha do capítulo
            rebuildBody();
            for (int i = 0; i < m_cmdVis.size(); ++i)
                if (m_cmdVis.at(i).first == cur.first && m_cmdVis.at(i).second < 0) m_cmdKb = i;
        }
    } else if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        const int i = m_cmdKb >= 0 ? m_cmdKb : 0;
        if (i >= n) return;
        const auto cur = m_cmdVis.at(i);
        const QString msId = activeManuscriptId();
        if (m_cmdEdit) { QSignalBlocker b(m_cmdEdit); m_cmdEdit->clear(); }
        m_cmdQuery.clear();
        m_cmdKb = -1;
        if (cur.second >= 0) emit sceneActivated(msId, cur.first, cur.second);
        else emit chapterActivated(msId, cur.first);
        return;
    }
    rebuildBody();
    if (m_cmdEdit) m_cmdEdit->setFocus();
    QTimer::singleShot(0, this, [this]() {
        for (auto* w : m_scroll->widget()->findChildren<QWidget*>())
            if (w->property("cmdIndex").isValid() && w->property("cmdIndex").toInt() == m_cmdKb) { m_scroll->ensureWidgetVisible(w, 0, 30); break; }
    });
}

// ============================================================ Margem

void ManuscriptPanel::buildMarginView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QList<Chapter> reading = readingChapters();
    const QList<ManuscriptPart> parts = partsForDisplay(reading);
    const QHash<QString, int> pidx = partIndexByChapter(reading, parts);
    const QColor accent = tcol(Theme::accentDefault());
    const QString gar = MsFonts::garamond();
    auto marginFont = [gar](qreal px, bool italic) {
        QFont f = fontOf(gar, px, QFont::Normal, italic);
        f.setFeature(QFont::Tag("onum"), 1);   // algarismos de texto
        return f;
    };
    int curPart = -2;
    bool first = true;
    for (const auto& c : chs) {
        const int pi = parts.isEmpty() ? -1 : pidx.value(c.id, -1);
        if (pi != curPart) {
            curPart = pi;
            if (pi >= 0) {
                auto* h = new MsRow(this);
                h->setRowHeight(first ? 24 : 38);
                h->setCursor(Qt::ArrowCursor);
                const QString t = partLabel(parts.at(pi)).toUpper();
                h->setPainter([t](QPainter& p, const QRect& r, const MsRow*) {
                    p.setFont(caps(9.5, 3.0, QFont::Medium));
                    p.setPen(tcol(Theme::textMuted()));
                    p.drawText(QRect(r.left() + 44, r.top(), r.width() - 50, r.height() - 4), Qt::AlignLeft | Qt::AlignBottom,
                               p.fontMetrics().elidedText(t, Qt::ElideRight, r.width() - 50));
                });
                const QString pid = parts.at(pi).id;
                h->setContextMenuPolicy(Qt::CustomContextMenu);
                connect(h, &QToolButton::customContextMenuRequested, this, [this, h, pid](const QPoint& pos) { showPartContextMenu(pid, h->mapToGlobal(pos)); });
                add(h);
            }
        }
        first = false;
        const bool special = c.type != QStringLiteral("chapter");
        const QString num = special ? QString() : m_model->chapterNumberLabel(c);
        const QString title = special ? m_model->chapterTypeName(c) + (c.title.isEmpty() ? QString() : QStringLiteral(": ") + c.title)
                                      : chapterTitleOnly(c);
        const int words = chapterWords(c.id);
        const bool open = c.id == m_curChapterId;
        const QString wtxt = words > 0 ? MsPaint::fmtInt(words) : tr("vazio");
        auto* row = new MsRow(this);
        row->setRowHeight(31);
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            p.setRenderHint(QPainter::Antialiasing);
            if (open) p.fillRect(QRectF(r.left() + 8, r.center().y() + 1, 16, 1.2), accent);
            p.setFont(marginFont(15, true));
            p.setPen(open ? accent : tcol(Theme::textMuted()));
            p.drawText(QRect(r.left(), r.top(), 40, r.height()), Qt::AlignRight | Qt::AlignVCenter, num);
            int right = r.right() - 6;
            if (self->isHovered()) {
                p.setFont(ui(11));
                const int ww = p.fontMetrics().horizontalAdvance(wtxt);
                p.setPen(tcol(Theme::textMuted()));
                p.drawText(QRect(right - ww, r.top(), ww + 2, r.height()), Qt::AlignRight | Qt::AlignVCenter, wtxt);
                right -= ww + 8;
            }
            p.setFont(marginFont(18, words <= 0));
            p.setPen(open || self->isHovered() ? tcol(Theme::textBright()) : (words <= 0 ? tcol(Theme::textMuted()) : tcol(Theme::textPrimary())));
            p.drawText(QRect(r.left() + 52, r.top(), right - r.left() - 52, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(title, Qt::ElideRight, right - r.left() - 52));
        });
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString chapterId = c.id, manuscriptId = c.manuscriptId;
        connect(row, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() { emit chapterActivated(manuscriptId, chapterId); });
        connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId](const QPoint& pos) {
            showChapterContextMenu(manuscriptId, chapterId, row->mapToGlobal(pos));
        });
        wireRow(row, QStringLiteral("chapter"), chapterId);
        add(row);
        if (open && c.scenes.size() > 1) {
            for (int j = 0; j < c.scenes.size(); ++j) {
                const bool sOpen = isCurrent(c.id, j);
                const QString st = c.scenes.at(j).title.isEmpty() ? tr("cena %1").arg(ProjectModel::toRomanNumeral(j + 1).toLower())
                                                                   : c.scenes.at(j).title;
                auto* sr = new MsRow(this);
                sr->setRowHeight(24);
                sr->setToolTip(tr("%1 palavras").arg(MsPaint::fmtInt(sceneWords(c.id, j))));
                sr->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
                    p.setFont(marginFont(15, true));
                    p.setPen(sOpen || self->isHovered() ? tcol(Theme::textBright()) : tcol(Theme::textMuted()));
                    p.drawText(QRect(r.left() + 70, r.top(), r.width() - 76, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                               p.fontMetrics().elidedText(st, Qt::ElideRight, r.width() - 76));
                });
                connect(sr, &QToolButton::clicked, this, [this, manuscriptId, chapterId, j]() { emit sceneActivated(manuscriptId, chapterId, j); });
                sr->setContextMenuPolicy(Qt::CustomContextMenu);
                connect(sr, &QToolButton::customContextMenuRequested, this, [this, sr, manuscriptId, chapterId, j](const QPoint& pos) {
                    showSceneContextMenu(manuscriptId, chapterId, j, sr->mapToGlobal(pos));
                });
                wireRow(sr, QStringLiteral("scene"), chapterId, j);
                add(sr);
            }
        }
    }
}

// ============================================================ Carrossel

void ManuscriptPanel::buildCarouselHead() {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    if (!m_model || m_model->manuscripts().isEmpty()) return;
    const auto& mss = m_model->manuscripts();
    const QString active = activeManuscriptId();
    QList<MsCoverItem> items;
    int cur = 0;
    for (int i = 0; i < mss.size(); ++i) {
        MsCoverItem it;
        it.id = mss.at(i).id;
        it.title = mss.at(i).title.isEmpty() ? tr("(sem título)") : mss.at(i).title;
        it.cover = bookCoverPixmap(mss.at(i), i + 1, QSize(116, 170));
        items << it;
        if (it.id == active) cur = i;
    }
    const Manuscript* m = m_model->findManuscript(active);
    const QString title = m && !m->title.isEmpty() ? m->title : tr("(sem título)");
    const QString meta = tr("livro %1 de %2 · %n capítulo(s)", "", readingChapters().size()).arg(cur + 1).arg(mss.size());
    auto* car = new MsCarousel(this);
    car->setItems(items, cur, m_carouselFrom, title, meta, bookAccent(active, panelIsDark()));
    m_carouselFrom = -1;
    connect(car, &MsCarousel::bookClicked, this, [this](const QString& id) { selectManuscript(id); });
    connect(car, &MsCarousel::bookContextRequested, this, [this](const QString& id, const QPoint& pos) { showManuscriptContextMenu(id, pos); });
    add(car);
    auto* sep = new QWidget(this);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QStringLiteral("background: %1;").arg(Theme::subtleBorder()));
    add(sep);

    // A faixa que aparece quando as capas saem de vista.
    if (QLayout* old = m_carouselMini->layout()) { clearLayout(old); delete old; }
    auto* ml = new QHBoxLayout(m_carouselMini);
    ml->setContentsMargins(14, 6, 14, 6);
    ml->setSpacing(10);
    auto* cov = new QLabel(m_carouselMini);
    cov->setPixmap(m ? bookCoverPixmap(*m, cur + 1, QSize(28, 40)) : QPixmap());
    cov->setFixedSize(28, 40);
    ml->addWidget(cov);
    auto* tl = new QVBoxLayout;
    tl->setSpacing(0);
    tl->addWidget(label(title, m_carouselMini, fontOf(MsFonts::cormorant(), 16, QFont::DemiBold, true), tcol(Theme::textBright())));
    tl->addWidget(label(meta, m_carouselMini, ui(10.5), tcol(Theme::textMuted())));
    ml->addLayout(tl, 1);
    updateCarouselMini();
}

void ManuscriptPanel::updateCarouselMini() {
    if (!m_carouselMini) return;
    const bool show = m_style == Style::Carousel && m_scroll->verticalScrollBar()->value() > 196;
    if (show == m_carouselMini->isVisible()) return;
    if (show) {
        placeOverlays();
        m_carouselMini->show();
        m_carouselMini->raise();
    } else {
        m_carouselMini->hide();
    }
}

// ============================================================ rodapés e sobreposições

void ManuscriptPanel::buildBottomNew() {
    if (!m_model || activeManuscriptId().isEmpty()) return;
    const QColor accent = tcol(Theme::accentDefault());
    const int total = readingChapters().size();
    auto hints = [&](const QStringList& pairs) {
        // pares "tecla|ação"
        auto* w = new QWidget(m_bottom);
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(10);
        for (const QString& pr : pairs) {
            const QString k = pr.section(QLatin1Char('|'), 0, 0), a = pr.section(QLatin1Char('|'), 1);
            auto* lb = new QLabel(w);
            lb->setTextFormat(Qt::RichText);
            lb->setText(QStringLiteral("<span style=\"font-family:'IBM Plex Mono'; border:1px solid %1; color:%2;\">&nbsp;%3&nbsp;</span> %4")
                .arg(Theme::panelBorder(), Theme::textPrimary(), k.toHtmlEscaped(), a.toHtmlEscaped()));
            lb->setStyleSheet(QStringLiteral("color: %1; font-size: 10.5px; background: transparent;").arg(Theme::textMuted()));
            l->addWidget(lb);
        }
        l->addStretch(1);
        return w;
    };
    switch (m_style) {
    case Style::Columns:
    case Style::Drum: {
        auto* w = new QWidget(m_bottom);
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        auto* count = new QLabel(w);
        count->setTextFormat(Qt::RichText);
        count->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; background: transparent;").arg(Theme::textMuted()));
        if (m_style == Style::Drum) {
            int idx = 0;
            const QList<Chapter> r = readingChapters();
            for (int i = 0; i < r.size(); ++i) if (r.at(i).id == m_drumSel) idx = i;
            count->setText(tr("<b>%1</b> de %2").arg(idx + 1).arg(total));
            m_drumCount = count;
        } else {
            count->setText(tr("%n capítulo(s)", "", total));
        }
        l->addWidget(count);
        l->addStretch(1);
        auto* add = textButton(QStringLiteral("＋ ") + tr("Novo capítulo"), w, accent, tcol(Theme::textBright()), 12);
        connect(add, &QToolButton::clicked, this, [this]() { emit newChapterRequested(activeManuscriptId()); });
        l->addWidget(add);
        m_bottomLayout->addWidget(w);
        break;
    }
    case Style::ChapterSelect:
        m_bottomLayout->addWidget(hints({ QStringLiteral("↑↓|") + tr("escolher"), QStringLiteral("↵|") + tr("abrir"),
                                          QStringLiteral("Tab|") + tr("livro") }));
        break;
    case Style::Command:
        m_bottomLayout->addWidget(hints({ QStringLiteral("↑↓|") + tr("andar"), QStringLiteral("→|") + tr("cenas"),
                                          QStringLiteral("↵|") + tr("abrir"), QStringLiteral("Esc|") + tr("limpar") }));
        break;
    case Style::Aura:
        m_bottomLayout->addWidget(makeDashedNewChapter(auraInk().accent));
        break;
    case Style::AuraClean:
    case Style::Bound:
        m_bottomLayout->addWidget(makeDashedNewChapter(themeInk().accent));
        break;
    case Style::Fan: {
        m_bottomLayout->setContentsMargins(0, 0, 0, 0);
        auto* fan = new MsFan(m_bottom);
        QList<MsCoverItem> items;
        const auto& mss = m_model->manuscripts();
        for (int i = 0; i < mss.size(); ++i) {
            MsCoverItem it;
            it.id = mss.at(i).id;
            it.title = mss.at(i).title.isEmpty() ? tr("(sem título)") : mss.at(i).title;
            it.cover = bookCoverPixmap(mss.at(i), i + 1, QSize(90, 132));
            items << it;
        }
        fan->setItems(items, activeManuscriptId(), MsPaint::bookColor(activeManuscriptId()), tr("A saga").toUpper(), tr("Novo manuscrito"));
        connect(fan, &MsFan::bookClicked, this, [this](const QString& id) { selectManuscript(id); });
        connect(fan, &MsFan::addClicked, this, &ManuscriptPanel::newManuscriptRequested);
        connect(fan, &MsFan::bookContextRequested, this, [this](const QString& id, const QPoint& pos) { showManuscriptContextMenu(id, pos); });
        m_bottomLayout->addWidget(fan);
        break;
    }
    case Style::Window: {
        m_bottomLayout->setContentsMargins(18, 6, 18, 10);
        auto* w = new QWidget(m_bottom);
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(8);
        const QString roundQss = QStringLiteral(
            "QToolButton { color: rgba(255,255,255,0.9); background: rgba(0,0,0,0.38); border: 1px solid rgba(255,255,255,0.22);"
            " border-radius: 13px; font-size: 14px; }"
            "QToolButton:hover { background: rgba(255,255,255,0.14); }");
        const auto& mss = m_model->manuscripts();
        const int cur = activeBookNumber() - 1;
        auto arrow = [&](const QString& t, int step) {
            auto* b = new QToolButton(w);
            b->setText(t);
            b->setFixedSize(26, 26);
            b->setCursor(Qt::PointingHandCursor);
            b->setStyleSheet(roundQss);
            b->setEnabled(mss.size() > 1);
            connect(b, &QToolButton::clicked, this, [this, step]() {
                const auto& all = m_model->manuscripts();
                if (all.size() < 2) return;
                const int i = (activeBookNumber() - 1 + step + all.size()) % all.size();
                selectManuscript(all.at(i).id);
            });
            return b;
        };
        l->addWidget(arrow(QStringLiteral("‹"), -1));
        auto* dots = new QLabel(w);
        QString d;
        for (int i = 0; i < mss.size() && i < 12; ++i)
            d += QStringLiteral("<span style=\"color:%1\">●</span>").arg(i == cur ? QStringLiteral("#ffffff") : QStringLiteral("rgba(255,255,255,0.35)"));
        dots->setTextFormat(Qt::RichText);
        dots->setText(d);
        dots->setStyleSheet(QStringLiteral("font-size: 7px; letter-spacing: 3px; background: transparent;"));
        l->addWidget(dots);
        l->addWidget(arrow(QStringLiteral("›"), 1));
        l->addStretch(1);
        auto* nw = new QToolButton(w);
        nw->setText(QStringLiteral("＋ ") + tr("capítulo"));
        nw->setCursor(Qt::PointingHandCursor);
        nw->setStyleSheet(QStringLiteral(
            "QToolButton { color: rgba(255,255,255,0.9); background: rgba(0,0,0,0.3); border: 1px solid rgba(255,255,255,0.3);"
            " border-radius: 11px; padding: 3px 10px; font-size: 11.5px; }"
            "QToolButton:hover { background: rgba(255,255,255,0.14); }"));
        connect(nw, &QToolButton::clicked, this, [this]() { emit newChapterRequested(activeManuscriptId()); });
        l->addWidget(nw);
        m_bottomLayout->addWidget(w);
        break;
    }
    default:
        break;
    }
}

void ManuscriptPanel::placeOverlays() {
    if (!m_scroll) return;
    // Janela: o papel dentro do recorte.
    if (m_style == Style::Window) {
        m_scroll->setStyleSheet(QStringLiteral("#manuscriptScroll { background: #f1ece1; border: none; border-radius: 16px; }"));
    } else {
        m_scroll->setStyleSheet(QStringLiteral("#manuscriptScroll { background: transparent; }"));
    }
    const QRect sr(m_scroll->mapTo(this, QPoint(0, 0)), m_scroll->size());
    if (m_windowFrame) {
        if (m_style == Style::Window && m_scroll->isVisible()) {
            m_windowFrame->setGeometry(sr);
            m_windowFrame->show();
            m_windowFrame->raise();
        } else {
            m_windowFrame->hide();
        }
    }
    if (m_carouselMini) {
        m_carouselMini->setGeometry(sr.left(), sr.top(), sr.width(), 52);
        if (m_carouselMini->isVisible()) m_carouselMini->raise();
    }
    for (QWidget* h : { m_resizeHandle, m_resizeHandleBottom, m_resizeHandleCorner }) if (h) h->raise();
}

// ============================================================ fundo, teclado

void ManuscriptPanel::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    if (!isCoverBackgroundStyle() || !m_model) return;
    const Manuscript* m = m_model->findManuscript(activeManuscriptId());
    if (!m) return;
    const QString key = QStringLiteral("%1|%2|%3x%4|%5").arg(styleId(m_style), m->id).arg(width()).arg(height())
                        .arg(QString::number(m_model->manuscriptEffectiveCoverDataUrl(m->id).size()) + MsPaint::bookColor(m->id).name());
    if (key != m_bgKey) {
        m_bgKey = key;
        const qreal dpr = devicePixelRatioF();
        QPixmap bg(size() * dpr);
        bg.setDevicePixelRatio(dpr);
        bg.fill(Qt::transparent);
        QPainter p(&bg);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        if (m_style == Style::Aura) {
            // Desfoque barato: capa minúscula esticada, duas vezes, e escurecida.
            const QImage tiny = bookCoverPixmap(*m, activeBookNumber(), QSize(60, 88)).toImage()
                                    .scaled(10, 15, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            const QImage mid = tiny.scaled(40, 60, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            const QSizeF s = QSizeF(size()) * 1.25;
            p.drawImage(QRectF((width() - s.width()) / 2, (height() - s.height()) / 2, s.width(), s.height()), mid);
            p.fillRect(rect(), QColor(0, 0, 0, 140));
        } else {
            const QPixmap cov = bookCoverPixmap(*m, activeBookNumber(), size());
            p.drawPixmap(0, 0, cov);
            QLinearGradient g(0, height() - 90, 0, height());
            g.setColorAt(0, QColor(0, 0, 0, 0));
            g.setColorAt(1, QColor(0, 0, 0, 120));
            p.fillRect(QRect(0, height() - 90, width(), 90), g);
        }
        p.end();
        m_bgCache = bg;
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect()), Theme::panelRadius(), Theme::panelRadius());
    p.setClipPath(clip);
    p.drawPixmap(0, 0, m_bgCache);
}

void ManuscriptPanel::keyPressEvent(QKeyEvent* event) {
    if (m_style == Style::ChapterSelect) {
        const QList<Chapter> reading = readingChapters();
        if (!reading.isEmpty()) {
            const QString from = m_previewChapterId.isEmpty() ? m_curChapterId : m_previewChapterId;
            int i = 0;
            for (int k = 0; k < reading.size(); ++k) if (reading.at(k).id == from) i = k;
            if (event->key() == Qt::Key_Down || event->key() == Qt::Key_Up) {
                i = std::clamp(i + (event->key() == Qt::Key_Down ? 1 : -1), 0, int(reading.size()) - 1);
                ++m_previewGen;
                setChapterPreview(reading.at(i).id == m_curChapterId ? QString() : reading.at(i).id);
                return;
            }
            if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
                emit chapterActivated(activeManuscriptId(), reading.at(i).id);
                return;
            }
        }
    }
    QWidget::keyPressEvent(event);
}

bool ManuscriptPanel::event(QEvent* event) {
    // Tab na Seleção de capítulo: próximo livro da saga (não passeia o foco).
    if (event->type() == QEvent::KeyPress && m_style == Style::ChapterSelect && m_model) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab) {
            const auto& mss = m_model->manuscripts();
            if (mss.size() > 1) {
                const int step = ke->key() == Qt::Key_Tab ? 1 : -1;
                const int i = (activeBookNumber() - 1 + step + mss.size()) % mss.size();
                selectManuscript(mss.at(i).id);
            }
            return true;
        }
    }
    return QWidget::event(event);
}

// ============================================================ Tarô, Rolo de filme, Quadros
// Concept "Gavetas ilustradas" (leva 2, 2026-09-28): a vinheta do capítulo,
// na cor da Parte, como arte principal.

namespace {
QString romanNumeral(int n) {
    if (n <= 0 || n >= 4000) return QString::number(n);
    static const int vals[] = { 1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1 };
    static const char* const syms[] = { "M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I" };
    QString out;
    for (int i = 0; i < 13; ++i) while (n >= vals[i]) { out += QLatin1String(syms[i]); n -= vals[i]; }
    return out;
}
// A cor da Parte puxada pro branco: rótulos em cima do fundo escuro.
QColor partInk(const QColor& c) {
    return QColor::fromRgbF(c.redF() * .7f + .3f, c.greenF() * .7f + .3f, c.blueF() * .7f + .3f);
}
QColor mixColor(const QColor& a, const QColor& b, qreal t) {
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}
}

void ManuscriptPanel::wireChapterCard(MsRow* row, const Chapter& c) {
    row->setContextMenuPolicy(Qt::CustomContextMenu);
    const QString chapterId = c.id, manuscriptId = c.manuscriptId;
    connect(row, &QToolButton::clicked, this, [this, manuscriptId, chapterId]() { emit chapterActivated(manuscriptId, chapterId); });
    connect(row, &QToolButton::customContextMenuRequested, this, [this, row, manuscriptId, chapterId](const QPoint& pos) {
        showChapterContextMenu(manuscriptId, chapterId, row->mapToGlobal(pos));
    });
    wireRow(row, QStringLiteral("chapter"), chapterId);
}

void ManuscriptPanel::buildTarotView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QHash<QString, int> fams = vignetteFamilies();
    const int avail = std::max(220, width() - 16 - 8);
    const int gap = 12;
    const int cardW = (avail - 12 - gap) / 2;
    const int artW = cardW - 12, artH = int(std::round(artW * 4.0 / 3.0));
    const int cardH = 3 + 6 + 18 + 4 + artH + 5 + 32 + 6;

    auto* grid = new QWidget(this);
    auto* gl = new QGridLayout(grid);
    gl->setContentsMargins(6, 10, 6, 10);
    gl->setHorizontalSpacing(gap);
    gl->setVerticalSpacing(gap);
    int pos = 0;
    for (const auto& c : chs) {
        const bool special = c.type != QStringLiteral("chapter");
        const QString num = m_model->chapterNumberLabel(c);
        bool isInt = false;
        const int n = num.toInt(&isInt);
        const QString numeral = special ? m_model->chapterTypeName(c).toUpper() : (isInt ? romanNumeral(n) : num);
        const QString title = chapterTitleOnly(c);
        const QColor pc = vignetteColor(c.id);
        const QPixmap art = chapterVignette(c, fams.value(c.id, MsVignette::autoFamily(c.id)), QSize(artW, artH), false);
        const bool cur = isCurrent(c.id, -1);
        const bool dim = !m_povFilter.isEmpty() && povOf(c) != m_povFilter;
        const int words = chapterWords(c.id);
        auto* card = new MsRow(grid);
        card->setRowHeight(cardH);
        card->setFixedWidth(cardW);
        card->setToolTip(words > 0 ? tr("%1 palavras").arg(MsPaint::fmtInt(words)) : QString());
        card->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            if (dim) p.setOpacity(0.4);
            p.setRenderHint(QPainter::Antialiasing);
            // O atual sobe um pouco, com sombra e o contorno do destaque.
            const QRectF box = cur ? QRectF(r).adjusted(1, 1, -1, -3) : QRectF(r).adjusted(1, 3, -1, -1);
            if (cur) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 0, 0, 60));
                p.drawRoundedRect(box.translated(0, 3), 8, 8);
            }
            const QColor bg = tcol(Theme::panelBackground());
            QColor page = mixColor(bg, QColor(255, 255, 255), bg.lightnessF() < .5 ? .05 : .5);
            if (self->isHovered()) page = mixColor(page, tcol(Theme::textPrimary()), .06);
            p.setBrush(page);
            p.setPen(cur ? QPen(tcol(Theme::accentDefault()), 2)
                         : QPen(mixColor(tcol(Theme::subtleBorder()), pc, .55), 1));
            p.drawRoundedRect(box, 8, 8);
            QFont nf = fontOf(MsFonts::cormorant(), numeral.size() > 5 ? 10.5 : 13, QFont::DemiBold);
            nf.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
            p.setFont(nf);
            p.setPen(partInk(pc));
            p.drawText(QRectF(box.left() + 6, box.top() + 6, box.width() - 12, 18), Qt::AlignCenter, numeral);
            p.drawPixmap(QPointF(box.left() + 6, box.top() + 6 + 18 + 4), art);
            QFont tf = fontOf(MsFonts::cormorant(), 13, QFont::DemiBold);
            tf.setLetterSpacing(QFont::AbsoluteSpacing, 0.4);
            p.setFont(tf);
            p.setPen(tcol(Theme::textBright()));
            const QRect tr = QRectF(box.left() + 6, box.top() + 6 + 18 + 4 + artH + 5, box.width() - 12, 32).toRect();
            const QFontMetrics fm(tf);
            // Até duas linhas; o que passar disso ganha reticências na segunda.
            QString t = title;
            auto fits = [&](const QString& s) {
                return fm.boundingRect(tr, Qt::AlignHCenter | Qt::TextWordWrap, s).height() <= tr.height() + 1;
            };
            if (!fits(t)) {
                while (t.size() > 1 && !fits(t + QStringLiteral("…"))) t.chop(1);
                t = t.trimmed() + QStringLiteral("…");
            }
            p.drawText(tr, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, t);
        });
        wireChapterCard(card, c);
        gl->addWidget(card, pos / 2, pos % 2, Qt::AlignTop);
        ++pos;
    }
    gl->setColumnStretch(2, 1);
    add(grid);
}

void ManuscriptPanel::buildFilmView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QHash<QString, int> fams = vignetteFamilies();
    const QList<Chapter> reading = readingChapters();
    const QList<ManuscriptPart> parts = validParts(reading);
    const QHash<QString, int> pidx = partIndexByChapter(reading, parts);
    // Uma tira só: as linhas coladas, sem espaço entre elas, e a altura
    // múltipla do passo dos furos pra película não "pular" de uma pra outra.
    constexpr int kRowH = 84, kHole = 14, kStripW = 118;
    auto* film = new QWidget(this);
    auto* fl = new QVBoxLayout(film);
    fl->setContentsMargins(6, 10, 6, 10);
    fl->setSpacing(0);
    for (int i = 0; i < chs.size(); ++i) {
        const Chapter& c = chs.at(i);
        const bool first = i == 0, last = i == chs.size() - 1;
        const QString num = m_model->chapterNumberLabel(c);
        const QString code = (c.type == QStringLiteral("chapter") && !num.isEmpty())
            ? num.rightJustified(2, QLatin1Char('0')) + QStringLiteral("A") : QStringLiteral("00");
        const QString title = chapterTitleOnly(c);
        const int words = chapterWords(c.id);
        const int pi = pidx.value(c.id, -1);
        QString part = pi >= 0 ? parts.at(pi).title.section(QStringLiteral(" · "), 0, 0).trimmed() : QString();
        if (c.type != QStringLiteral("chapter")) part = m_model->chapterTypeName(c);
        const QString meta = (part.isEmpty() ? QString() : part + kSep())
            + (words > 0 ? tr("%1 palavras").arg(shortK(words)) : tr("em branco"));
        const QPixmap art = chapterVignette(c, fams.value(c.id, MsVignette::autoFamily(c.id)), QSize(90, 62), false);
        const bool cur = isCurrent(c.id, -1);
        const bool dim = !m_povFilter.isEmpty() && povOf(c) != m_povFilter;
        auto* row = new MsRow(film);
        row->setRowHeight(kRowH);
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            if (dim) p.setOpacity(0.4);
            p.setRenderHint(QPainter::Antialiasing);
            const QRectF strip(r.left(), r.top(), kStripW, r.height());
            // Película: preta, com as pontas arredondadas só no começo e no fim.
            QPainterPath sp;
            sp.addRoundedRect(strip.adjusted(0, first ? 0 : -6, 0, last ? 0 : 6), 4, 4);
            p.save();
            p.setClipRect(strip);
            p.fillPath(sp, QColor(0x0b, 0x0b, 0x0c));
            p.restore();
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0x3a, 0x3a, 0x3c));
            for (int y = 0; y < r.height(); y += kHole) {
                p.drawRoundedRect(QRectF(strip.left() + 3, r.top() + y + 5, 8, 5), 1.2, 1.2);
                p.drawRoundedRect(QRectF(strip.right() - 11, r.top() + y + 5, 8, 5), 1.2, 1.2);
            }
            const QPointF at(strip.left() + 18, r.top() + (r.height() - 62) / 2.0);
            p.drawPixmap(at, art);
            if (cur) {
                p.setPen(QPen(tcol(Theme::accentDefault()), 2));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(QRectF(at.x() - 2, at.y() - 2, 94, 66), 3, 3);
            }
            // O número na borda da película, de pé, como nos rolos de verdade.
            p.save();
            p.translate(strip.left() + 17, at.y() + 1);
            p.rotate(90);
            p.setFont(mono(7));
            p.setPen(QColor(0xc9, 0xa3, 0x4a));
            p.drawText(QRectF(0, 0, 40, 5), Qt::AlignLeft | Qt::AlignVCenter, code);
            p.restore();
            // Legenda ao lado.
            const QRectF cap(strip.right() + 10, r.top(), r.right() - strip.right() - 10, r.height());
            if (self->isHovered()) p.fillRect(cap, tcol(Theme::hoverOverlay()));
            p.setPen(QPen(tcol(Theme::subtleBorder()), 1, Qt::DashLine));
            p.drawLine(QPointF(cap.left(), cap.bottom() - .5), QPointF(cap.right(), cap.bottom() - .5));
            const int tw = int(cap.width()) - 8;
            p.setFont(ui(12.5, QFont::DemiBold));
            p.setPen(cur ? tcol(Theme::accentDefault()) : tcol(Theme::textBright()));
            p.drawText(QRectF(cap.left() + 4, cap.center().y() - 20, tw, 20), Qt::AlignLeft | Qt::AlignBottom,
                       p.fontMetrics().elidedText(title, Qt::ElideRight, tw));
            p.setFont(ui(11));
            p.setPen(tcol(Theme::textMuted()));
            p.drawText(QRectF(cap.left() + 4, cap.center().y() + 1, tw, 16), Qt::AlignLeft | Qt::AlignTop,
                       p.fontMetrics().elidedText(meta, Qt::ElideRight, tw));
        });
        wireChapterCard(row, c);
        fl->addWidget(row);
    }
    add(film);
}

void ManuscriptPanel::buildFramesView(const QList<Chapter>& chs) {
    auto add = [this](QWidget* w) { m_listLayout->insertWidget(m_listLayout->count() - 1, w); };
    const QHash<QString, int> fams = vignetteFamilies();
    const QList<Chapter> reading = readingChapters();
    const QList<ManuscriptPart> parts = validParts(reading);
    const QHash<QString, int> pidx = partIndexByChapter(reading, parts);
    constexpr int kArt = 104;
    int lastPart = -2;
    for (const auto& c : chs) {
        const int pi = pidx.value(c.id, -1);
        const QColor pc = vignetteColor(c.id);
        // As Partes separam os blocos, com a cor delas.
        if (!parts.isEmpty() && pi != lastPart) {
            lastPart = pi;
            if (pi >= 0) {
                const QString name = parts.at(pi).title.toUpper();
                auto* head = new MsRow(this);
                head->setRowHeight(30);
                head->setAttribute(Qt::WA_TransparentForMouseEvents);
                head->setPainter([=](QPainter& p, const QRect& r, const MsRow*) {
                    paintCapsRow(p, r, name, partInk(pc), 16, true);
                });
                add(head);
            }
        }
        const bool special = c.type != QStringLiteral("chapter");
        const QString num = m_model->chapterNumberLabel(c);
        const QString kicker = (special || num.isEmpty()) ? m_model->chapterTypeName(c).toUpper()
                                                        : tr("Capítulo %1").arg(num).toUpper();
        const QString title = chapterTitleOnly(c);
        const int words = chapterWords(c.id);
        const QString meta = words > 0
            ? tr("%1 palavras").arg(shortK(words)) + kSep() + tr("%n cena(s)", "", std::max<int>(1, c.scenes.size()))
            : tr("ainda vazio");
        const QPixmap art = chapterVignette(c, fams.value(c.id, MsVignette::autoFamily(c.id)), QSize(kArt, kArt), false);
        const bool cur = isCurrent(c.id, -1);
        const bool dim = !m_povFilter.isEmpty() && povOf(c) != m_povFilter;
        auto* row = new MsRow(this);
        row->setRowHeight(kArt + 28);
        row->setToolTip(c.summary.trimmed());
        row->setPainter([=](QPainter& p, const QRect& r, const MsRow* self) {
            if (dim) p.setOpacity(0.4);
            p.setRenderHint(QPainter::Antialiasing);
            if (cur) {
                p.fillRect(r, alpha(tcol(Theme::accentDefault()), 0.09));
                p.fillRect(QRect(r.left(), r.top(), 3, r.height()), tcol(Theme::accentDefault()));
            } else if (self->isHovered()) {
                p.fillRect(r, tcol(Theme::hoverOverlay()));
            }
            p.fillRect(QRect(r.left(), r.bottom(), r.width(), 1), alpha(tcol(Theme::textPrimary()), 0.07));
            // Sombra macia embaixo do quadro.
            const QRectF artBox(r.left() + 16, r.top() + (r.height() - kArt) / 2.0, kArt, kArt);
            p.setPen(Qt::NoPen);
            for (int k = 3; k >= 1; --k) {
                p.setBrush(QColor(0, 0, 0, 22));
                p.drawRoundedRect(artBox.adjusted(-k * .5, k * 1.2, k * .5, k * 1.6), 7 + k, 7 + k);
            }
            p.drawPixmap(artBox.topLeft(), art);
            const int x = int(artBox.right()) + 16;
            const int tw = r.right() - 12 - x;
            const QFont tf = serif(17, QFont::DemiBold);
            const QFontMetrics tfm(tf);
            const QRect tb = tfm.boundingRect(QRect(0, 0, tw, 1000), Qt::TextWordWrap, title);
            const int titleH = std::min(tb.height(), tfm.lineSpacing() * 2);
            const int blockH = 14 + 4 + titleH + 4 + 16 + (cur ? 16 : 0);
            int y = r.top() + (r.height() - blockH) / 2;
            p.setFont(caps(9.5, 1.4));
            p.setPen(partInk(pc));
            p.drawText(QRect(x, y, tw, 14), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(kicker, Qt::ElideRight, tw));
            y += 18;
            p.setFont(tf);
            p.setPen(tcol(Theme::textBright()));
            p.save();
            p.setClipRect(QRect(x, y, tw, titleH));
            p.drawText(QRect(x, y, tw, titleH + tfm.lineSpacing()), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, title);
            p.restore();
            y += titleH + 4;
            p.setFont(ui(11.5));
            p.setPen(tcol(Theme::textMuted()));
            p.drawText(QRect(x, y, tw, 16), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(meta, Qt::ElideRight, tw));
            if (cur) {
                y += 16;
                p.setFont(ui(11));
                p.setPen(tcol(Theme::accentDefault()));
                p.drawText(QRect(x, y, tw, 16), Qt::AlignLeft | Qt::AlignVCenter, tr("você está aqui"));
            }
        });
        wireChapterCard(row, c);
        add(row);
    }
}
