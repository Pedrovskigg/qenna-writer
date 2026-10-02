// Aba Diálogos do Pensário no desenho "Elenco na frente" (concept aprovado em
// 2026-09-29, concepts/pensario/dialogos/elenco-blocos.html). Vale nos estilos
// que usam o cartão padrão (Clássico, Trilho, Doca); os estilos com desenho
// próprio (Revista, WhatsQenna, Tabela…) seguem pelo rebuildDialogues antigo.
//
//   Diálogos  13 falas                 [↻ Escanear] [?]
//   [livro] [Cap. 5 · O mercado            ▾]
//   [ Todas 13 | Conferir 5 | Figurantes 2 ]
//   (rostos de quem fala — clicar filtra)
//   ──────────────────────────────────────────
//   CENA 1 · O MERCADO
//   ODA
//   ▎Você está atrasado.

#include "PensarioPanel.h"

#include "DialogueVoices.h"
#include "DocCache.h"
#include "ElementsStore.h"
#include "IconUtils.h"
#include "ProjectModel.h"
#include "Theme.h"

#include <QApplication>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScreen>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

const QString kExtrasKey = QStringLiteral("__extras__");
constexpr int kDialoguePageSize = 60; // mesma paginação do PensarioPanel.cpp

// Quem mais fala no manuscrito pega as primeiras cores, e a ordem não muda ao
// trocar de capítulo (paleta em DialogueVoices, a mesma do aviso do Granna).
QColor voiceColor(int index) { return DialogueVoices::color(index); }

QFont serif(qreal px)
{
    QFont f(QStringLiteral("Lora"));
    f.setPixelSize(qRound(px));
    return f;
}

QString menuQss()
{
    return Theme::qss(QStringLiteral(
        "QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: @radius-panel; padding: 4px; }"
        "QMenu::item { padding: 5px 18px 5px 10px; border-radius: @radius-item; }"
        "QMenu::item:selected { background: %4; color: %5; }"
        "QMenu::separator { height: 1px; background: %3; margin: 4px 6px; }"))
        .arg(Theme::panelBackground(), Theme::textPrimary(), Theme::panelBorder(),
             Theme::accentInfoSoft(), Theme::textBright());
}

// Roda uma função a cada resize do widget observado (texto elidido do seletor).
class PnResizeHook : public QObject
{
public:
    PnResizeHook(QObject* parent, std::function<void()> fn) : QObject(parent), m_fn(std::move(fn)) {}
protected:
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (e->type() == QEvent::Resize) m_fn();
        return QObject::eventFilter(o, e);
    }
private:
    std::function<void()> m_fn;
};

QPixmap initialsFace(const QString& name, const QColor& color, int size)
{
    const qreal dpr = qApp->devicePixelRatio();
    QPixmap pm(qRound(size * dpr), qRound(size * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(QRectF(0, 0, size, size));
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(qMax(9, int(size * 0.4)));
    p.setFont(f);
    p.setPen(color.lightnessF() > 0.6 ? QColor(0, 0, 0, 170) : QColor(255, 255, 255, 235));
    const QString initial = name.trimmed().isEmpty() ? QStringLiteral("?") : name.trimmed().left(1).toUpper();
    p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, initial);
    return pm;
}

QString capitalized(QString s)
{
    if (!s.isEmpty()) s[0] = s.at(0).toUpper();
    return s;
}

} // namespace

bool PensarioPanel::dialogueScriptLayout() const
{
    return m_style == Style::Classic || m_style == Style::Rail || m_style == Style::Dock;
}

QString PensarioPanel::dialogueManuscriptId() const
{
    if (!m_model) return QString();
    const auto& mss = m_model->manuscripts();
    auto exists = [&mss](const QString& id) {
        for (const Manuscript& m : mss) if (m.id == id) return true;
        return false;
    };
    if (!m_dialogueManuscriptId.isEmpty() && exists(m_dialogueManuscriptId)) return m_dialogueManuscriptId;
    const QString chId = m_dialogueOriginFilter.section(QStringLiteral("::"), 0, 0);
    if (!chId.isEmpty())
        if (const Chapter* ch = m_model->findChapter(chId)) return ch->manuscriptId;
    if (!m_currentChapterId.isEmpty())
        if (const Chapter* ch = m_model->findChapter(m_currentChapterId)) return ch->manuscriptId;
    return mss.isEmpty() ? QString() : mss.first().id;
}

void PensarioPanel::rebuildDialoguesScript(const QVector<DialogueStore::Dialogue>& all)
{
    const QString msId = dialogueManuscriptId();

    // Capítulos do manuscrito, em ordem de leitura.
    QHash<QString, int> chapterRank;
    if (m_model) {
        for (const Chapter& ch : m_model->chapters())
            if (ch.manuscriptId == msId)
                chapterRank.insert(ch.id, rankForKey(DocCache::chapterKey(ch.manuscriptId, ch.id)));
    }

    // Escopo: manuscrito + capítulo/cena escolhidos, em ordem de leitura.
    struct Row { const DialogueStore::Dialogue* d; int rank; int order; };
    QVector<Row> scoped;
    for (int i = 0; i < all.size(); ++i) {
        const auto& d = all.at(i);
        if (!chapterRank.contains(d.chapterId)) continue;
        if (!dialogueMatchesOriginFilter(d)) continue;
        scoped.append({ &d, chapterRank.value(d.chapterId), i });
    }
    std::stable_sort(scoped.begin(), scoped.end(), [](const Row& a, const Row& b) {
        if (a.rank != b.rank) return a.rank < b.rank;
        if (a.d->sceneIndex != b.d->sceneIndex) return a.d->sceneIndex < b.d->sceneIndex;
        return a.order < b.order;
    });

    int probableCount = 0, extrasCount = 0;
    QStringList speakers; // ordem de primeira fala
    bool anyExtra = false;
    for (const Row& r : scoped) {
        if (r.d->isProbable()) ++probableCount;
        if (r.d->isExtra()) { ++extrasCount; anyExtra = true; }
        if (!r.d->characterId.isEmpty() && !speakers.contains(r.d->characterId)) speakers.append(r.d->characterId);
    }
    // Índice de cor de cada voz: quem mais fala no manuscrito primeiro.
    QHash<QString, int> voiceIndex;
    {
        QHash<QString, int> lines;
        for (const auto& d : all)
            if (!d.characterId.isEmpty() && !d.isExtra() && chapterRank.contains(d.chapterId)) lines[d.characterId] += 1;
        QStringList ids = lines.keys();
        std::sort(ids.begin(), ids.end(), [&lines](const QString& a, const QString& b) {
            const int la = lines.value(a), lb = lines.value(b);
            return la != lb ? la > lb : a < b;
        });
        for (int i = 0; i < ids.size(); ++i) voiceIndex.insert(ids.at(i), i);
    }
    auto colorOf = [&voiceIndex](const QString& id) { return voiceColor(voiceIndex.value(id, -1)); };

    // Rosto marcado que não fala mais no escopo sai do filtro.
    for (auto it = m_dialogueSpeakerFilter.begin(); it != m_dialogueSpeakerFilter.end();) {
        if ((*it == kExtrasKey && !anyExtra) || (*it != kExtrasKey && !speakers.contains(*it))) it = m_dialogueSpeakerFilter.erase(it);
        else ++it;
    }

    // ---- Topo: título, contagem, Escanear, ajuda ----
    {
        auto* row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(8);
        auto* title = new QLabel(tr("Diálogos"), m_dialoguesInner);
        title->setStyleSheet(QStringLiteral("color: %1; font-size: 15px; font-weight: 600;").arg(Theme::textBright()));
        auto* count = new QLabel(tr("%1 falas").arg(scoped.size()), m_dialoguesInner);
        count->setStyleSheet(QStringLiteral("color: %1; font-size: 11.5px;").arg(Theme::textMuted()));
        row->addWidget(title);
        row->addWidget(count, 0, Qt::AlignBottom);
        row->addStretch(1);

        m_dlgScanBtn = new QToolButton(m_dialoguesInner);
        m_dlgScanBtn->setFocusPolicy(Qt::NoFocus);
        m_dlgScanBtn->setObjectName(QStringLiteral("pnDlgScanText"));
        m_dlgScanBtn->setCursor(Qt::PointingHandCursor);
        m_dlgScanBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        m_dlgScanBtn->setText(m_dialogueScanRunning
            ? tr("Escaneando… %1/%2").arg(m_dialogueScanDone).arg(m_dialogueScanTotal)
            : tr("↻ Escanear"));
        m_dlgScanBtn->setEnabled(!m_dialogueScanRunning);
        m_dlgScanBtn->setToolTip(tr("Escanear diálogos em todos os capítulos"));
        m_dlgScanBtn->setStyleSheet(Theme::qss(QStringLiteral(
            "QToolButton#pnDlgScanText { background: %1; border: 1px solid %2; border-radius: @radius-item; "
            "color: %3; font-size: 11.5px; padding: 3px 9px; }"
            "QToolButton#pnDlgScanText:hover { color: %4; border-color: %4; }"
            "QToolButton#pnDlgScanText:disabled { color: %5; }"))
            .arg(Theme::inputBackground(), Theme::panelBorder(), Theme::textPrimary(),
                 Theme::accentDefault(), Theme::textMuted()));
        connect(m_dlgScanBtn, &QToolButton::clicked, this, [this]() { emit rescanAllDialoguesRequested(); });
        row->addWidget(m_dlgScanBtn);

        auto* help = new QToolButton(m_dialoguesInner);
        help->setFocusPolicy(Qt::NoFocus);
        help->setObjectName(QStringLiteral("pnDlgHelpBtn"));
        help->setCursor(Qt::PointingHandCursor);
        help->setText(QStringLiteral("?"));
        help->setToolTip(tr("Como o detector de diálogos funciona"));
        connect(help, &QToolButton::clicked, this, [this, help]() { showDialogueHelp(help); });
        row->addWidget(help);
        m_dialoguesLay->addLayout(row);
    }

    // ---- Livro (manuscrito) + capítulo ----
    const QString fieldQss = Theme::qss(QStringLiteral(
        "QToolButton { background: %1; border: 1px solid %2; border-radius: @radius-item; color: %3; "
        "font-size: 12.5px; padding: 6px 10px; text-align: left; }"
        "QToolButton:hover { border-color: %4; }"))
        .arg(Theme::inputBackground(), Theme::panelBorder(), Theme::textPrimary(), Theme::accentDefault());
    {
        auto* row = new QHBoxLayout();
        row->setContentsMargins(0, 2, 0, 0);
        row->setSpacing(6);

        auto* book = new QToolButton(m_dialoguesInner);
        book->setFocusPolicy(Qt::NoFocus);
        book->setCursor(Qt::PointingHandCursor);
        book->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/manuscriptbook.svg"),
                                                 QColor(Theme::textMuted()), QColor(Theme::accentDefault()),
                                                 QColor(Theme::accentDefault()), QSize(17, 17)));
        book->setIconSize(QSize(17, 17));
        book->setFixedWidth(34);
        QString msTitle;
        if (m_model)
            for (const Manuscript& m : m_model->manuscripts()) if (m.id == msId) msTitle = m.title;
        book->setToolTip(tr("Manuscrito: %1").arg(msTitle));
        book->setStyleSheet(fieldQss + QStringLiteral("QToolButton { padding: 6px 0; text-align: center; }"));
        connect(book, &QToolButton::clicked, this, [this, book]() { showDialogueManuscriptMenu(book); });
        row->addWidget(book);

        QString chLabel = tr("Todos os capítulos");
        if (!m_dialogueOriginFilter.isEmpty() && m_model) {
            const QString chId = m_dialogueOriginFilter.section(QStringLiteral("::"), 0, 0);
            if (const Chapter* ch = m_model->findChapter(chId)) {
                chLabel = chapterLabel(ch->manuscriptId, ch->title);
                if (m_dialogueOriginFilter.contains(QStringLiteral("::"))) {
                    const int sc = m_dialogueOriginFilter.section(QStringLiteral("::"), 1, 1).toInt();
                    if (sc >= 0 && sc < ch->scenes.size()) {
                        const QString st = ch->scenes.at(sc).title;
                        chLabel += QStringLiteral(" · ") + (st.isEmpty() ? tr("Cena %1").arg(sc + 1) : st);
                    }
                }
            }
        }
        // QPushButton porque o QToolButton ignora text-align e centraliza.
        auto* chapter = new QPushButton(m_dialoguesInner);
        chapter->setObjectName(QStringLiteral("pnDlgChapterPick"));
        chapter->setCursor(Qt::PointingHandCursor);
        chapter->setFocusPolicy(Qt::NoFocus);
        chapter->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        chapter->setMinimumWidth(60);
        chapter->setToolTip(chLabel);
        chapter->setStyleSheet(Theme::qss(QStringLiteral(
            "QPushButton#pnDlgChapterPick { background: %1; border: 1px solid %2; border-radius: @radius-item; color: %3; "
            "font-size: 12.5px; padding: 6px 10px; text-align: left; }"
            "QPushButton#pnDlgChapterPick:hover { border-color: %4; }"))
            .arg(Theme::inputBackground(), Theme::panelBorder(), Theme::textPrimary(), Theme::accentDefault()));
        auto* chLay = new QHBoxLayout(chapter);
        chLay->setContentsMargins(0, 0, 10, 0);
        chLay->addStretch(1);
        auto* caret = new QLabel(QStringLiteral("▾"), chapter);
        caret->setAttribute(Qt::WA_TransparentForMouseEvents);
        caret->setStyleSheet(QStringLiteral("color: %1; background: transparent; font-size: 11px;").arg(Theme::textMuted()));
        chLay->addWidget(caret);
        // Texto cortado com "…" pra não empurrar a seta nem alargar o painel.
        const QString full = chLabel;
        auto elide = [chapter, full]() {
            chapter->setText(chapter->fontMetrics().elidedText(full, Qt::ElideRight, qMax(20, chapter->width() - 44)));
        };
        chapter->installEventFilter(new PnResizeHook(chapter, elide));
        elide();
        connect(chapter, &QPushButton::clicked, this, [this, chapter]() { showDialogueChapterMenu(chapter); });
        row->addWidget(chapter, 1);
        m_dialoguesLay->addLayout(row);
    }

    // ---- Todas / Conferir / Figurantes ----
    {
        auto* seg = new QFrame(m_dialoguesInner);
        seg->setObjectName(QStringLiteral("pnDlgSeg"));
        seg->setStyleSheet(Theme::qss(QStringLiteral(
            "QFrame#pnDlgSeg { background: %1; border: 1px solid %2; border-radius: @radius-item; }"
            "QFrame#pnDlgSeg QToolButton { background: transparent; border: none; border-radius: 5px; "
            "color: %3; font-size: 11.5px; padding: 4px 0; }"
            "QFrame#pnDlgSeg QToolButton:checked { background: %4; color: %5; }"
            "QFrame#pnDlgSeg QToolButton:hover:!checked { color: %5; }"))
            .arg(Theme::inputBackground(), Theme::panelBorder(), Theme::textMuted(),
                 Theme::accentInfoSoft(), Theme::textBright()));
        auto* sl = new QHBoxLayout(seg);
        sl->setContentsMargins(2, 2, 2, 2);
        sl->setSpacing(2);
        const struct { QString id; QString label; } parts[] = {
            { QString(), tr("Todas %1").arg(scoped.size()) },
            { QStringLiteral("probable"), tr("Conferir %1").arg(probableCount) },
            { QStringLiteral("extras"), tr("Figurantes %1").arg(extrasCount) },
        };
        for (const auto& p : parts) {
            auto* b = new QToolButton(seg);
            b->setFocusPolicy(Qt::NoFocus);
            b->setCheckable(true);
            b->setChecked(m_dialogueView == p.id);
            b->setCursor(Qt::PointingHandCursor);
            b->setText(p.label);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            const QString id = p.id;
            connect(b, &QToolButton::clicked, this, [this, id]() {
                m_dialogueView = id;
                m_dialogueVisibleCount = kDialoguePageSize;
                rebuildDialogues();
            });
            sl->addWidget(b, 1);
        }
        m_dialoguesLay->addWidget(seg);
    }

    // ---- Elenco na frente: rostos de quem fala, clicar filtra ----
    if (!speakers.isEmpty() || anyExtra) {
        auto* stripScroll = new QScrollArea(m_dialoguesInner);
        stripScroll->setFrameShape(QFrame::NoFrame);
        stripScroll->setWidgetResizable(true);
        stripScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        stripScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        stripScroll->setFixedHeight(94);
        stripScroll->setStyleSheet(QStringLiteral(
            "QScrollArea { background: transparent; }"
            "QScrollBar:horizontal { background: transparent; height: 6px; margin: 0; }"
            "QScrollBar::handle:horizontal { background: %1; border-radius: 3px; min-width: 24px; }"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }")
            .arg(Theme::panelBorder()));
        stripScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
        auto* strip = new QWidget(stripScroll);
        auto* hl = new QHBoxLayout(strip);
        hl->setContentsMargins(0, 4, 0, 4);
        hl->setSpacing(4);

        constexpr int kFace = 52;
        auto addFace = [&](const QString& key, const QPixmap& face, const QString& label, const QColor& color, bool italic) {
            auto* b = new QToolButton(strip);
            b->setFocusPolicy(Qt::NoFocus);
            b->setCursor(Qt::PointingHandCursor);
            b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            b->setIcon(QIcon(face));
            b->setIconSize(QSize(kFace, kFace));
            b->setText(italic ? label : label.toUpper());
            b->setToolTip(m_dialogueSpeakerFilter.contains(key) ? tr("Mostrar todos") : tr("Só as falas de %1").arg(label));
            // Largura pelo texto: o primeiro nome aparece inteiro, sem "…".
            QFont f = b->font();
            f.setPixelSize(10);
            f.setBold(true);
            b->setFixedWidth(qMax(kFace + 6, QFontMetrics(f).horizontalAdvance(b->text()) + 8));
            b->setStyleSheet(QStringLiteral(
                "QToolButton { background: transparent; border: none; color: %1; font-size: 9.5px; "
                "font-weight: %2; font-style: %3; padding: 0; }")
                .arg(color.name(), italic ? QStringLiteral("600") : QStringLiteral("700"),
                     italic ? QStringLiteral("italic") : QStringLiteral("normal")));
            if (!m_dialogueSpeakerFilter.isEmpty() && !m_dialogueSpeakerFilter.contains(key)) {
                auto* fx = new QGraphicsOpacityEffect(b);
                fx->setOpacity(0.35);
                b->setGraphicsEffect(fx);
            }
            connect(b, &QToolButton::clicked, this, [this, key]() {
                if (m_dialogueSpeakerFilter.contains(key)) m_dialogueSpeakerFilter.remove(key);
                else m_dialogueSpeakerFilter.insert(key);
                m_dialogueVisibleCount = kDialoguePageSize;
                rebuildDialogues();
            });
            hl->addWidget(b);
        };
        for (const QString& id : speakers) {
            const QString full = dialogueSpeakerLabel(id);
            const QString first = full.section(QLatin1Char(' '), 0, 0, QString::SectionSkipEmpty);
            const Element* el = m_elements ? m_elements->findElement(id) : nullptr;
            const QColor col = colorOf(id);
            // Sem foto, a bolinha de iniciais vai na cor da voz (a do avatar
            // padrão é outro hash e não batia com o nome embaixo).
            const QPixmap face = (el && !el->image.isEmpty()) ? characterAvatar(id, kFace)
                                                              : initialsFace(full, col, kFace);
            addFace(id, face, first.isEmpty() ? full : first, col, false);
        }
        if (anyExtra)
            addFace(kExtrasKey, unattributedAvatar(kFace), tr("figurantes"), QColor(Theme::textMuted()), true);
        hl->addStretch(1);
        stripScroll->setWidget(strip);
        m_dialoguesLay->addWidget(stripScroll);
    }

    auto* sep = new QFrame(m_dialoguesInner);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QStringLiteral("background: %1; border: none;").arg(Theme::panelBorder()));
    m_dialoguesLay->addWidget(sep);

    // ---- Falas ----
    QVector<const DialogueStore::Dialogue*> list;
    for (const Row& r : scoped) {
        const auto* d = r.d;
        if (m_dialogueView == QLatin1String("probable") && !d->isProbable()) continue;
        if (m_dialogueView == QLatin1String("extras") && !d->isExtra()) continue;
        if (!m_dialogueSpeakerFilter.isEmpty()) {
            const QString key = d->isExtra() ? kExtrasKey : d->characterId;
            if (!m_dialogueSpeakerFilter.contains(key)) continue;
        }
        list.append(d);
    }

    if (list.isEmpty()) {
        auto* empty = new QLabel(scoped.isEmpty()
            ? tr("Nenhum diálogo detectado aqui ainda. Escreva falas com travessão "
                 "(“— Não vou — disse Maria.”) e espere alguns segundos, ou use Escanear.")
            : tr("Nenhuma fala neste filtro."), m_dialoguesInner);
        empty->setObjectName(QStringLiteral("pnEmpty"));
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        m_dialoguesLay->addWidget(empty);
        m_dialoguesLay->addStretch();
        return;
    }

    const bool paginate = list.size() > kDialoguePageSize;
    if (paginate) {
        if (m_dialogueVisibleCount <= 0) m_dialogueVisibleCount = kDialoguePageSize;
        m_dialogueVisibleCount = qMin(m_dialogueVisibleCount, int(list.size()));
    }
    const int visible = paginate ? m_dialogueVisibleCount : int(list.size());
    const bool wholeManuscript = m_dialogueOriginFilter.isEmpty();

    // Cabeçalhos: capítulo (só quando a lista cobre o manuscrito todo) e cena
    // (só quando não se escolheu uma cena — aí o seletor já diz qual é).
    const bool oneScene = m_dialogueOriginFilter.contains(QStringLiteral("::"));
    QString lastChapter, lastScene;
    bool sceneShown = false;
    auto addHead = [this](const QString& text, bool chapterLevel) {
        auto* head = new QLabel(chapterLevel ? text : text.toUpper(), m_dialoguesInner);
        head->setWordWrap(true);
        head->setStyleSheet(chapterLevel
            ? QStringLiteral("color: %1; font-size: 12.5px; font-weight: 600; margin-top: 12px;").arg(Theme::textPrimary())
            : QStringLiteral("color: %1; font-size: 10px; font-weight: 700; letter-spacing: 1.2px; margin-top: 7px;")
                  .arg(Theme::textMuted()));
        m_dialoguesLay->addWidget(head);
    };
    for (int i = 0; i < visible; ++i) {
        const DialogueStore::Dialogue& d = *list.at(i);
        const Chapter* ch = m_model ? m_model->findChapter(d.chapterId) : nullptr;
        if (wholeManuscript && d.chapterId != lastChapter) {
            lastChapter = d.chapterId;
            sceneShown = false;
            addHead(ch ? chapterLabel(ch->manuscriptId, ch->title) : d.sourceLabel, true);
        }
        QString scene;
        if (ch && d.sceneIndex >= 0 && d.sceneIndex < ch->scenes.size()) {
            const QString st = ch->scenes.at(d.sceneIndex).title;
            scene = tr("Cena %1").arg(d.sceneIndex + 1);
            if (!st.isEmpty() && st != scene) scene += QStringLiteral(" · ") + st;
        }
        if (!oneScene && (!sceneShown || scene != lastScene)) {
            lastScene = scene;
            sceneShown = true;
            if (!scene.isEmpty()) addHead(scene, false);
        }

        const bool pv = d.isProbable();
        const bool fig = d.isExtra();
        const bool none = d.characterId.isEmpty() && !fig;
        const QColor c = (fig || none) ? QColor(Theme::panelBorder()) : colorOf(d.characterId);
        QString nameText;
        if (fig) nameText = d.extraLabel.isEmpty() ? tr("figurante") : capitalized(d.extraLabel);
        else if (none) nameText = tr("sem locutor");
        else nameText = dialogueSpeakerLabel(d.characterId).toUpper();

        auto* block = new QWidget(m_dialoguesInner);
        block->setCursor(Qt::PointingHandCursor);
        block->setProperty("dlgId", d.id);
        block->installEventFilter(this);
        block->setContextMenuPolicy(Qt::CustomContextMenu);
        block->setToolTip(tr("Clique pra abrir no texto · botão direito pra conferir ou trocar"));
        const QString dlgId = d.id;
        connect(block, &QWidget::customContextMenuRequested, this, [this, block, dlgId](const QPoint& pos) {
            showDialogueLineMenu(dlgId, block->mapToGlobal(pos));
        });
        auto* bl = new QVBoxLayout(block);
        bl->setContentsMargins(0, 3, 0, 3);
        bl->setSpacing(1);

        auto* name = new QLabel(nameText, block);
        QColor nameColor = (fig || none) ? QColor(Theme::textMuted()) : c;
        if (pv) nameColor.setAlphaF(0.6);
        name->setStyleSheet(QStringLiteral("color: %1; font-size: %2px; font-weight: %3; letter-spacing: %4px; font-style: %5;")
            .arg(nameColor.name(QColor::HexArgb),
                 (fig || none) ? QStringLiteral("10.5") : QStringLiteral("9.5"),
                 (fig || none) ? QStringLiteral("600") : QStringLiteral("700"),
                 (fig || none) ? QStringLiteral("0") : QStringLiteral("0.8"),
                 (fig || none) ? QStringLiteral("italic") : QStringLiteral("normal")));
        bl->addWidget(name);

        auto* speech = new QLabel(d.spokenText().trimmed(), block);
        speech->setWordWrap(true);
        speech->setFont(serif(14));
        speech->setStyleSheet(QStringLiteral("color: %1; border: none; border-left: 2px %2 %3; padding-left: 8px;")
            .arg((fig || none) ? Theme::textMuted() : Theme::textBright(),
                 (pv || none) ? QStringLiteral("dashed") : QStringLiteral("solid"),
                 c.name()));
        bl->addWidget(speech);
        m_dialoguesLay->addWidget(block);
    }

    if (paginate && visible < list.size()) {
        auto* more = new QToolButton(m_dialoguesInner);
        more->setFocusPolicy(Qt::NoFocus);
        more->setObjectName(QStringLiteral("pnDlgLoadMoreBtn"));
        more->setCursor(Qt::PointingHandCursor);
        more->setText(tr("Carregar mais (%1 restantes)").arg(list.size() - visible));
        connect(more, &QToolButton::clicked, this, [this]() {
            m_dialogueVisibleCount += kDialoguePageSize;
            rebuildDialogues();
        });
        m_dialoguesLay->addWidget(more);
    }
    m_dialoguesLay->addStretch();
}

void PensarioPanel::showDialogueManuscriptMenu(QWidget* anchor)
{
    if (!m_model) return;
    const QString current = dialogueManuscriptId();
    const QJsonObject colors = m_model->settings().value(QStringLiteral("manuscriptColors")).toObject();

    QHash<QString, int> countByChapter;
    if (m_dialogues)
        for (const auto& d : m_dialogues->dialogues()) countByChapter[d.chapterId] += 1;

    QMenu menu(anchor);
    menu.setStyleSheet(menuQss());
    auto* head = menu.addAction(tr("MANUSCRITO"));
    head->setEnabled(false);
    for (const Manuscript& m : m_model->manuscripts()) {
        int count = 0;
        for (const Chapter& ch : m_model->chapters())
            if (ch.manuscriptId == m.id) count += countByChapter.value(ch.id);
        QPixmap sw(14, 18);
        sw.fill(Qt::transparent);
        {
            QPainter p(&sw);
            p.setRenderHint(QPainter::Antialiasing, true);
            const QColor col = colors.contains(m.id) ? QColor(colors.value(m.id).toString())
                                                     : QColor(Theme::accentDefault());
            p.setBrush(col);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(QRectF(1, 1, 12, 16), 2, 2);
        }
        QString label = m.title.isEmpty() ? tr("(sem título)") : m.title;
        label += QStringLiteral("   ·   ") + tr("%1 falas").arg(count);
        QAction* a = menu.addAction(QIcon(sw), label);
        a->setCheckable(true);
        a->setChecked(m.id == current);
        const QString id = m.id;
        connect(a, &QAction::triggered, this, [this, id]() {
            m_dialogueManuscriptId = id;
            m_dialogueOriginFilter.clear();
            m_dialogueOriginFilterUserSet = true;
            m_dialogueSpeakerFilter.clear();
            m_dialogueVisibleCount = kDialoguePageSize;
            rebuildDialogues();
        });
    }
    menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height() + 2)));
}

void PensarioPanel::showDialogueChapterMenu(QWidget* anchor)
{
    if (!m_model) return;
    const QString msId = dialogueManuscriptId();

    QVector<const Chapter*> chapters;
    for (const Chapter& ch : m_model->chapters())
        if (ch.manuscriptId == msId) chapters.append(&ch);
    std::sort(chapters.begin(), chapters.end(), [this](const Chapter* a, const Chapter* b) {
        return rankForKey(DocCache::chapterKey(a->manuscriptId, a->id))
             < rankForKey(DocCache::chapterKey(b->manuscriptId, b->id));
    });

    auto* popup = new QFrame(nullptr);
    popup->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setStyleSheet(Theme::qss(QStringLiteral(
        "QFrame { background: %1; border: 1px solid %2; border-radius: @radius-panel; }")
        .arg(Theme::panelBackground(), Theme::panelBorder())));
    auto* plist = new QListWidget(popup);
    plist->setObjectName(QStringLiteral("pnDlgOriginList"));
    plist->setFrameShape(QFrame::NoFrame);
    plist->setFocusPolicy(Qt::NoFocus);
    plist->setUniformItemSizes(true);
    plist->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    plist->setStyleSheet(Theme::qss(QStringLiteral(
        "QListWidget#pnDlgOriginList { background: transparent; color: %1; outline: none; border: none; }"
        "QListWidget#pnDlgOriginList::item { padding: 5px 10px; border-radius: @radius-item; }"
        "QListWidget#pnDlgOriginList::item:selected { background: %2; color: %3; }"
        "QListWidget#pnDlgOriginList QScrollBar:vertical { background: transparent; width: 9px; margin: 0; }"
        "QListWidget#pnDlgOriginList QScrollBar::handle:vertical { background: %4; border-radius: 4px; min-height: 20px; }"
        "QListWidget#pnDlgOriginList QScrollBar::add-line:vertical, "
        "QListWidget#pnDlgOriginList QScrollBar::sub-line:vertical { height: 0px; }")
        .arg(Theme::textPrimary(), Theme::accentInfoSoft(), Theme::textBright(), Theme::panelBorder())));

    auto add = [this, plist](const QString& label, const QString& value) {
        auto* it = new QListWidgetItem(label, plist);
        it->setData(Qt::UserRole, value);
        if (value == m_dialogueOriginFilter) plist->setCurrentItem(it);
    };
    add(tr("Todos os capítulos"), QString());
    for (const Chapter* ch : chapters) {
        add(chapterLabel(ch->manuscriptId, ch->title), ch->id);
        for (int si = 0; si < ch->scenes.size(); ++si) {
            const QString st = ch->scenes.at(si).title;
            add(QStringLiteral("      · %1").arg(st.isEmpty() ? tr("Cena %1").arg(si + 1) : st),
                QStringLiteral("%1::%2").arg(ch->id).arg(si));
        }
    }

    auto* lay = new QVBoxLayout(popup);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->addWidget(plist);
    constexpr int kRowH = 27;
    const int w = qMax(240, anchor->width());
    plist->setFixedHeight(kRowH * qMin(9, qMax(1, plist->count())));
    plist->setFixedWidth(w - 8);
    popup->setFixedWidth(w);

    connect(plist, &QListWidget::itemClicked, this, [this, popup](QListWidgetItem* it) {
        m_dialogueOriginFilter = it->data(Qt::UserRole).toString();
        m_dialogueOriginFilterUserSet = true;
        m_dialogueVisibleCount = kDialoguePageSize;
        popup->close();
        rebuildDialogues();
    });

    QPoint pos = anchor->mapToGlobal(QPoint(0, anchor->height() + 2));
    const int h = plist->height() + 8;
    if (auto* screen = anchor->screen()) {
        const QRect avail = screen->availableGeometry();
        if (pos.y() + h > avail.bottom()) pos.setY(anchor->mapToGlobal(QPoint(0, 0)).y() - h - 2);
        if (pos.x() + w > avail.right()) pos.setX(avail.right() - w);
    }
    popup->move(pos);
    popup->show();
}

void PensarioPanel::showDialogueLineMenu(const QString& dlgId, const QPoint& globalPos)
{
    if (!m_dialogues) return;
    const DialogueStore::Dialogue* dlg = nullptr;
    for (const auto& d : m_dialogues->dialogues())
        if (d.id == dlgId) { dlg = &d; break; }
    if (!dlg) return;
    const DialogueStore::Dialogue copy = *dlg;

    QMenu menu;
    menu.setStyleSheet(menuQss());
    QAction* confirm = copy.isProbable()
        ? menu.addAction(tr("Confirmar: é %1").arg(dialogueSpeakerLabel(copy.characterId))) : nullptr;
    QAction* change = menu.addAction(copy.characterId.isEmpty() ? tr("Atribuir ao personagem…")
                                                                : tr("Trocar quem disse…"));
    QAction* open = menu.addAction(tr("Abrir no texto"));
    menu.addSeparator();
    QAction* notSpeech = menu.addAction(tr("Não é fala (some e não volta)"));

    QAction* chosen = menu.exec(globalPos);
    if (!chosen) return;
    if (chosen == confirm) m_dialogues->setCharacter(copy.id, copy.characterId);
    else if (chosen == change) showChangeSpeakerPopup(copy.id, globalPos);
    else if (chosen == open) emit openDialogueInEditorRequested(copy);
    else if (chosen == notSpeech) m_dialogues->remove(copy.id);
}
