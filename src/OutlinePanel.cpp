#include "OutlinePanel.h"
#include "DialogueStore.h"
#include "DocPreview.h"
#include "ElementsStore.h"
#include "ProjectModel.h"
#include "Theme.h"
#include "WordCounter.h"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QScrollBar>
#include <QTextBrowser>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr const char* kSplitterKey = "ui/outlinePanel/splitter";
constexpr const char* kSceneMime   = "application/x-qenna-outline-scene";
constexpr const char* kChapterMime = "application/x-qenna-outline-chapter";

constexpr int kBigCardW   = 380;
constexpr int kBigCardH   = 196;
constexpr int kSmallCardW = 210;
constexpr int kSmallCardH = 138;
constexpr int kStatusBarH = 3;
// Corpo do painel de leitura. O manuscrito costuma estar em 16pt (tamanho de
// página); numa coluna lateral isso quebra a linha a cada cinco palavras.
constexpr int kPreviewFontPt = 13;  // mesmo padrão do preview do RefMenu
constexpr int kCastPanelW = 250;   // faixa colorida no topo do card grande

// Quanto de prévia cabe em cada card sem o QLabel comer o fim do texto. São
// medidas do que a caixa aguenta, não gosto: o card pequeno tem três linhas.
constexpr int kBigPreviewChars   = 220;
constexpr int kSmallPreviewChars = 105;

QString packSceneRef(const QString& chapterId, int sceneIndex) {
    return chapterId + QLatin1Char('|') + QString::number(sceneIndex);
}

bool unpackSceneRef(const QByteArray& raw, QString* chapterId, int* sceneIndex) {
    const QString s = QString::fromUtf8(raw);
    const int bar = s.lastIndexOf(QLatin1Char('|'));
    if (bar <= 0) return false;
    bool ok = false;
    const int idx = s.mid(bar + 1).toInt(&ok);
    if (!ok) return false;
    *chapterId = s.left(bar);
    *sceneIndex = idx;
    return true;
}

QString groupNumber(int n) {
    return QLocale().toString(n); // milhar com separador, como o resto do app
}

// "2.500 palavras" com o número em destaque e a unidade apagada.
QLabel* makeMetaLabel(int value, const QString& unit, QWidget* parent) {
    auto* lbl = new QLabel(parent);
    lbl->setTextFormat(Qt::RichText);
    lbl->setText(QStringLiteral("<span style='color:%1'>%2</span> %3")
                     .arg(Theme::textPrimary(), groupNumber(value), unit.toHtmlEscaped()));
    lbl->setStyleSheet(QStringLiteral("font-size: 12px; color: %1;").arg(Theme::textMuted()));
    return lbl;
}

void clearLayout(QLayout* layout) {
    if (!layout) return;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* w = item->widget()) { w->hide(); w->deleteLater(); }
        delete item;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// OutlineCard
// ---------------------------------------------------------------------------

OutlineCard::OutlineCard(Size size, const OutlineCardData& data, QWidget* parent)
    : QFrame(parent)
    , m_data(data)
    , m_size(size)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setCursor(Qt::PointingHandCursor);
    if (m_size == Big) {
        setFixedWidth(kBigCardW);
        setMinimumHeight(kBigCardH);
    } else {
        setFixedSize(kSmallCardW, kSmallCardH);
    }
    build();
    applyFrameStyle();
}

void OutlineCard::build()
{
    const bool big = (m_size == Big);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Faixa de status no topo, só no card grande. Na cena o status é um
    // pontinho ao lado do título — uma fila de faixas viraria arco-íris.
    if (big) {
        auto* bar = new QWidget(this);
        bar->setFixedHeight(kStatusBarH);
        bar->setStyleSheet(QStringLiteral("background: %1;")
            .arg(m_data.statusColor.isEmpty() ? QStringLiteral("transparent") : m_data.statusColor));
        root->addWidget(bar);
    }

    auto* body = new QVBoxLayout();
    body->setContentsMargins(big ? 20 : 14, big ? 16 : 13, big ? 20 : 14, big ? 16 : 13);
    body->setSpacing(big ? 10 : 7);

    // --- título -----------------------------------------------------------
    auto* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);

    const bool showingScene = big && !m_data.parentHeading.isEmpty();
    if (showingScene) {
        // O capítulo vira o caminho de volta quando a cena está promovida.
        auto* back = new QLabel(this);
        back->setText(QStringLiteral("<a href='#' style='color:%1; text-decoration:none;'>%2</a>")
                          .arg(Theme::textMuted(), m_data.parentHeading.toHtmlEscaped()));
        back->setStyleSheet(QStringLiteral("font-size: 18px;"));
        back->setCursor(Qt::PointingHandCursor);
        back->setToolTip(tr("Voltar para o capítulo"));
        connect(back, &QLabel::linkActivated, this, [this](const QString&) {
            emit parentHeadingClicked(m_data.chapterId);
        });
        titleRow->addWidget(back);

        auto* sep = new QLabel(QStringLiteral("·"), this);
        sep->setStyleSheet(QStringLiteral("color: %1; font-size: 16px;").arg(Theme::textMuted()));
        titleRow->addWidget(sep);
    }

    auto* heading = new QLabel(m_data.heading, this);
    heading->setStyleSheet(QStringLiteral("font-size: %1px; font-weight: 600; color: %2;")
        .arg(big ? 19 : 14)
        .arg(showingScene ? Theme::accentDefault() : Theme::textBright()));
    titleRow->addWidget(heading);

    if (big) {
        if (!m_data.statusLabel.isEmpty()) {
            auto* pill = new QLabel(m_data.statusLabel, this);
            pill->setStyleSheet(QStringLiteral(
                "color: %1; font-size: 11px; border: 1px solid %1; border-radius: 9px; padding: 1px 8px;")
                .arg(m_data.statusColor));
            titleRow->addWidget(pill);
        }
        titleRow->addStretch();
    } else {
        titleRow->addStretch();
        auto* dot = new QWidget(this);
        dot->setFixedSize(7, 7);
        dot->setStyleSheet(QStringLiteral("background: %1; border-radius: 3px;")
            .arg(m_data.statusColor.isEmpty() ? Theme::borderStrong() : m_data.statusColor));
        titleRow->addWidget(dot, 0, Qt::AlignVCenter);
    }
    body->addLayout(titleRow);

    // --- resumo -----------------------------------------------------------
    auto* summary = new QLabel(this);
    summary->setWordWrap(true);
    summary->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    if (m_data.summary.isEmpty()) {
        summary->setText(tr("Sem descrição"));
        summary->setStyleSheet(QStringLiteral("font-size: %1px; font-style: italic; color: %2;")
            .arg(big ? 15 : 12).arg(Theme::disabledText()));
    } else if (m_data.summaryIsPreview) {
        // Prévia do texto, não descrição: itálico e apagada, pra a diferença
        // ser visível sem precisar de rótulo ocupando linha no card.
        summary->setText(m_data.summary);
        summary->setStyleSheet(QStringLiteral("font-size: %1px; font-style: italic; color: %2;")
            .arg(big ? 14 : 12).arg(Theme::textMuted()));
        summary->setToolTip(tr("Começo do texto — este capítulo ainda não tem descrição."));
    } else {
        summary->setText(m_data.summary);
        summary->setStyleSheet(QStringLiteral("font-size: %1px; color: %2;")
            .arg(big ? 15 : 12)
            .arg(big ? Theme::textPrimary() : Theme::textMuted()));
        summary->setToolTip(m_data.summary);
    }
    body->addWidget(summary, /*stretch=*/1);

    if (big) {
        auto* rule = new QWidget(this);
        rule->setFixedHeight(1);
        rule->setStyleSheet(QStringLiteral("background: %1;").arg(Theme::panelBorder()));
        body->addWidget(rule);
    }

    // --- metadados --------------------------------------------------------
    if (big) {
        auto* metaRow = new QHBoxLayout();
        metaRow->setContentsMargins(0, 0, 0, 0);
        metaRow->setSpacing(16);
        metaRow->addWidget(makeMetaLabel(m_data.words, tr("palavras"), this));
        metaRow->addWidget(makeMetaLabel(m_data.dialogues, tr("diálogos"), this));
        if (!m_data.timeMarker.isEmpty()) {
            auto* when = new QLabel(m_data.timeMarker, this);
            when->setStyleSheet(QStringLiteral("font-size: 12px; color: %1;").arg(Theme::textMuted()));
            metaRow->addWidget(when);
        }
        metaRow->addStretch();
        if (m_data.povOther) {
            auto* pov = new QLabel(tr("POV"), this);
            pov->setToolTip(tr("Ponto de vista diferente do narrador principal."));
            pov->setStyleSheet(QStringLiteral(
                "color: %1; font-size: 11px; border: 1px solid %2; border-radius: 9px; padding: 1px 8px;")
                .arg(Theme::textMuted(), Theme::panelBorder()));
            metaRow->addWidget(pov);
        }
        body->addLayout(metaRow);

        // Barra de ritmo: quanto do capítulo é fala e quanto é narração. É a
        // pergunta que a contagem de palavras sozinha não responde — 7 mil
        // palavras de conversa e 7 mil de descrição são capítulos diferentes.
        if (m_data.dialogueWords >= 0 && m_data.words > 0) {
            const int dlg = qBound(0, m_data.dialogueWords, m_data.words);
            const int pct = qRound(100.0 * dlg / m_data.words);

            auto* track = new QWidget(this);
            track->setFixedHeight(4);
            track->setStyleSheet(QStringLiteral("background: %1; border-radius: 2px;")
                                     .arg(Theme::hoverOverlay()));
            auto* trackLayout = new QHBoxLayout(track);
            trackLayout->setContentsMargins(0, 0, 0, 0);
            trackLayout->setSpacing(0);

            auto* dialoguePart = new QWidget(track);
            dialoguePart->setStyleSheet(QStringLiteral("background: %1; border-radius: 2px;")
                                            .arg(Theme::accentInfo()));
            trackLayout->addWidget(dialoguePart, qMax(1, pct));
            if (pct < 100) trackLayout->addStretch(100 - pct);
            body->addWidget(track);

            auto* legend = new QLabel(tr("%1% diálogo · %2% narração")
                                          .arg(pct).arg(100 - pct), this);
            legend->setStyleSheet(QStringLiteral("font-size: 11px; color: %1;").arg(Theme::textMuted()));
            body->addWidget(legend);
        }

        if (!m_data.characters.isEmpty()) {
            auto* charRow = new QHBoxLayout();
            charRow->setContentsMargins(0, 0, 0, 0);
            charRow->setSpacing(6);

            auto* prefix = new QLabel(tr("Em cena:"), this);
            prefix->setStyleSheet(QStringLiteral("font-size: 12px; color: %1;").arg(Theme::textMuted()));
            charRow->addWidget(prefix);

            // Três cabem na largura do card; o resto vira "+N", com a lista
            // inteira no tooltip.
            constexpr int kMaxChips = 3;
            for (int i = 0; i < m_data.characters.size() && i < kMaxChips; ++i) {
                auto* chip = new QLabel(m_data.characters.at(i), this);
                chip->setStyleSheet(QStringLiteral(
                    "font-size: 12px; color: %1; background: %2; border-radius: 6px; padding: 1px 7px;")
                    .arg(Theme::textPrimary(), Theme::hoverOverlay()));
                charRow->addWidget(chip);
            }
            if (m_data.characters.size() > kMaxChips) {
                auto* more = new QLabel(QStringLiteral("+%1").arg(m_data.characters.size() - kMaxChips), this);
                more->setToolTip(m_data.characters.join(QStringLiteral(", ")));
                more->setStyleSheet(QStringLiteral("font-size: 12px; color: %1;").arg(Theme::textMuted()));
                charRow->addWidget(more);
            }
            charRow->addStretch();
            body->addLayout(charRow);
        }
    } else {
        auto* meta = new QLabel(tr("%1 pal. · %2 diál.")
                                    .arg(groupNumber(m_data.words), groupNumber(m_data.dialogues)), this);
        meta->setStyleSheet(QStringLiteral("font-size: 11px; color: %1;").arg(Theme::textMuted()));
        body->addWidget(meta);

        // Peso da cena dentro do capítulo: pra achar de longe a que inchou,
        // sem comparar número com número.
        auto* track = new QWidget(this);
        track->setFixedHeight(2);
        track->setStyleSheet(QStringLiteral("background: %1; border-radius: 1px;").arg(Theme::hoverOverlay()));
        auto* trackLayout = new QHBoxLayout(track);
        trackLayout->setContentsMargins(0, 0, 0, 0);
        trackLayout->setSpacing(0);
        auto* fill = new QWidget(track);
        fill->setStyleSheet(QStringLiteral("background: %1; border-radius: 1px;").arg(Theme::accentDefault()));
        const int weight = m_data.chapterWords > 0
            ? qBound(1, int(1000.0 * m_data.words / m_data.chapterWords), 1000) : 1;
        trackLayout->addWidget(fill, weight);
        if (weight < 1000) trackLayout->addStretch(1000 - weight);
        body->addWidget(track);
    }

    root->addLayout(body);
}

void OutlineCard::applyFrameStyle()
{
    const QString border = m_selected ? Theme::accentDefault() : Theme::panelBorder();
    const QString bg = m_selected ? Theme::hoverOverlay() : Theme::panelBackground();
    setStyleSheet(QStringLiteral(
        "OutlineCard { background: %1; border: 1px solid %2; border-radius: %3px; }")
        .arg(bg, border).arg(m_size == Big ? 14 : 11));
}

void OutlineCard::setSelected(bool on)
{
    if (m_selected == on) return;
    m_selected = on;
    applyFrameStyle();
}

void OutlineCard::setDimmed(bool on)
{
    if (m_dimmed == on) return;
    m_dimmed = on;
    // Opacidade de widget não existe fora de janela — apagar aqui é atenuar o
    // conteúdo, e o conteúdo já nasce apagado o bastante nos cards de cena.
    // Basta a borda perder o destaque, o que applyFrameStyle já faz.
    applyFrameStyle();
}

void OutlineCard::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        m_pressPos = event->pos();
    }
    // accept() no press é o que faz o Qt continuar entregando os mouseMove a
    // este widget — sem isso, o drag nunca começa.
    event->accept();
}

void OutlineCard::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_pressed) return;
    if ((event->pos() - m_pressPos).manhattanLength() < QApplication::startDragDistance()) return;
    // O card grande mostrando uma cena promovida não arrasta: seria ambíguo
    // (move o capítulo ou a cena?). Despromova antes.
    if (m_size == Big && !m_data.parentHeading.isEmpty()) return;

    m_pressed = false;
    emit dragStarted(m_data.chapterId, m_data.sceneIndex);

    auto* mime = new QMimeData();
    if (m_size == Small) {
        mime->setData(QLatin1String(kSceneMime),
                      packSceneRef(m_data.chapterId, m_data.sceneIndex).toUtf8());
    } else {
        mime->setData(QLatin1String(kChapterMime), m_data.chapterId.toUtf8());
    }

    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->setPixmap(grab());
    drag->setHotSpot(m_pressPos);
    drag->exec(Qt::MoveAction);
}

void OutlineCard::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_pressed && event->button() == Qt::LeftButton && rect().contains(event->pos()))
        emit clicked(m_data.chapterId, m_data.sceneIndex);
    m_pressed = false;
    event->accept();
}

void OutlineCard::contextMenuEvent(QContextMenuEvent* event)
{
    emit contextMenuRequested(m_data.chapterId, m_data.sceneIndex, event->globalPos());
    event->accept();
}

// ---------------------------------------------------------------------------
// OutlineStrip
// ---------------------------------------------------------------------------

OutlineStrip::OutlineStrip(QWidget* parent)
    : QWidget(parent)
{
    setAcceptDrops(true);

    m_root = new QHBoxLayout(this);
    m_root->setContentsMargins(0, 0, 0, 0);
    m_root->setSpacing(18);

    m_bigHost = new QWidget(this);
    auto* bigLayout = new QVBoxLayout(m_bigHost);
    bigLayout->setContentsMargins(0, 0, 0, 0);
    bigLayout->setSpacing(0);
    m_bigHost->setFixedWidth(kBigCardW);
    m_root->addWidget(m_bigHost, 0, Qt::AlignTop);

    m_queueHost = new QWidget(this);
    auto* queueOuter = new QVBoxLayout(m_queueHost);
    queueOuter->setContentsMargins(0, 2, 0, 0);
    queueOuter->setSpacing(8);

    m_queueLabel = new QLabel(tr("CENAS"), m_queueHost);
    queueOuter->addWidget(m_queueLabel);

    auto* queueRow = new QWidget(m_queueHost);
    m_queueLayout = new QHBoxLayout(queueRow);
    m_queueLayout->setContentsMargins(0, 0, 0, 0);
    m_queueLayout->setSpacing(12);
    queueOuter->addWidget(queueRow);
    queueOuter->addStretch();

    m_root->addWidget(m_queueHost, 1, Qt::AlignTop);

}

void OutlineStrip::setContent(const OutlineCardData& chapter,
                              const QList<OutlineCardData>& scenes,
                              int focusedScene)
{
    m_chapterId = chapter.chapterId;
    m_sceneCount = scenes.size();
    m_sceneCards.clear();
    m_dropMarker = nullptr;

    if (m_queueLabel) {
        m_queueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; letter-spacing: 1px;")
                                        .arg(Theme::textMuted()));
    }

    clearLayout(m_bigHost ? m_bigHost->layout() : nullptr);
    clearLayout(m_queueLayout);

    // --- card grande ------------------------------------------------------
    OutlineCardData bigData = chapter;
    if (focusedScene >= 0 && focusedScene < scenes.size()) {
        bigData = scenes.at(focusedScene);
        bigData.parentHeading = chapter.heading;
    }
    auto* bigCard = new OutlineCard(OutlineCard::Big, bigData, m_bigHost);
    connect(bigCard, &OutlineCard::clicked, this, &OutlineStrip::cardClicked);
    connect(bigCard, &OutlineCard::parentHeadingClicked, this, &OutlineStrip::backToChapterRequested);
    connect(bigCard, &OutlineCard::contextMenuRequested, this, &OutlineStrip::cardContextMenuRequested);
    m_bigHost->layout()->addWidget(bigCard);

    // --- fila de cenas ----------------------------------------------------
    for (int i = 0; i < scenes.size(); ++i) {
        auto* card = new OutlineCard(OutlineCard::Small, scenes.at(i), m_queueHost);
        card->setSelected(i == focusedScene);
        card->setDimmed(focusedScene >= 0 && i != focusedScene);
        connect(card, &OutlineCard::clicked, this, &OutlineStrip::cardClicked);
        connect(card, &OutlineCard::contextMenuRequested, this, &OutlineStrip::cardContextMenuRequested);
        m_queueLayout->addWidget(card);
        m_sceneCards.append(card);
    }

    // Um alvo de soltar só, sempre no fim. Num capítulo sem cenas ele é largo e
    // diz o que está acontecendo; com cenas, é um "+" estreito depois da
    // última. Antes eram dois elementos tracejados lado a lado no capítulo
    // vazio, e a fila parecia ter dois buracos em vez de um alvo.
    const bool empty = scenes.isEmpty();
    auto* tail = new QLabel(empty ? tr("Capítulo sem divisão de cenas")
                                  : QStringLiteral("+"), m_queueHost);
    tail->setAlignment(Qt::AlignCenter);
    tail->setFixedHeight(kSmallCardH);
    if (empty) tail->setMinimumWidth(300);
    else tail->setFixedWidth(74);
    tail->setStyleSheet(QStringLiteral(
        "color: %1; font-size: %2px; border: 1px dashed %3; border-radius: 11px;")
        .arg(Theme::disabledText())
        .arg(empty ? 12 : 20)
        .arg(empty ? Theme::subtleBorder() : Theme::borderStrong()));
    tail->setToolTip(tr("Solte uma cena aqui para movê-la para este capítulo."));
    m_queueLayout->addWidget(tail);
    m_queueLayout->addStretch();

}

int OutlineStrip::insertIndexAt(const QPoint& posInStrip) const
{
    if (m_sceneCards.isEmpty()) return 0;
    for (int i = 0; i < m_sceneCards.size(); ++i) {
        OutlineCard* card = m_sceneCards.at(i);
        if (!card) continue;
        const QPoint topLeft = card->mapTo(const_cast<OutlineStrip*>(this), QPoint(0, 0));
        if (posInStrip.x() < topLeft.x() + card->width() / 2) return i;
    }
    return m_sceneCards.size();
}

void OutlineStrip::showDropMarkerAt(int index)
{
    if (!m_queueLayout) return;
    if (!m_dropMarker) {
        m_dropMarker = new QWidget(m_queueHost);
        m_dropMarker->setFixedWidth(2);
        m_dropMarker->setFixedHeight(kSmallCardH);
        m_dropMarker->setStyleSheet(QStringLiteral("background: %1; border-radius: 1px;")
                                        .arg(Theme::accentDefault()));
    }
    m_queueLayout->removeWidget(m_dropMarker);
    m_queueLayout->insertWidget(qBound(0, index, m_queueLayout->count()), m_dropMarker);
    m_dropMarker->show();
}

void OutlineStrip::clearDropMarker()
{
    if (!m_dropMarker) return;
    if (m_queueLayout) m_queueLayout->removeWidget(m_dropMarker);
    m_dropMarker->hide();
    m_dropMarker->deleteLater();
    m_dropMarker = nullptr;
}

void OutlineStrip::dragEnterEvent(QDragEnterEvent* event)
{
    // Só cena. Capítulo arrastado atravessa a faixa e é tratado pelo painel,
    // que é quem conhece a ordem das faixas.
    if (event->mimeData()->hasFormat(QLatin1String(kSceneMime)))
        event->acceptProposedAction();
    else
        event->ignore();
}

void OutlineStrip::dragMoveEvent(QDragMoveEvent* event)
{
    if (!event->mimeData()->hasFormat(QLatin1String(kSceneMime))) return;
    showDropMarkerAt(insertIndexAt(event->position().toPoint()));
    event->acceptProposedAction();
}

void OutlineStrip::dragLeaveEvent(QDragLeaveEvent*)
{
    clearDropMarker();
}

void OutlineStrip::dropEvent(QDropEvent* event)
{
    const int targetIndex = insertIndexAt(event->position().toPoint());
    clearDropMarker();

    QString srcChapterId;
    int srcIndex = -1;
    if (!unpackSceneRef(event->mimeData()->data(QLatin1String(kSceneMime)),
                        &srcChapterId, &srcIndex)) return;
    if (srcIndex < 0) return;

    if (srcChapterId == m_chapterId) {
        // Soltar na própria posição, ou logo depois dela, não é movimento.
        if (targetIndex == srcIndex || targetIndex == srcIndex + 1) {
            event->acceptProposedAction();
            return;
        }
        emit reorderSceneRequested(m_chapterId, srcIndex, targetIndex);
    } else {
        emit moveSceneToChapterRequested(srcChapterId, srcIndex, m_chapterId, targetIndex);
    }
    event->acceptProposedAction();
}

// ---------------------------------------------------------------------------
// OutlinePanel
// ---------------------------------------------------------------------------

OutlinePanel::OutlinePanel(ProjectModel* model, QWidget* parent)
    : QWidget(parent, Qt::Window)
    , m_model(model)
{
    setObjectName(QStringLiteral("outlinePanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setWindowTitle(tr("Outline"));
    setAcceptDrops(true);
    resize(1400, 820);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // --- header -----------------------------------------------------------
    m_header = new QWidget(this);
    m_header->setObjectName(QStringLiteral("outlineHeader"));
    auto* headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(20, 12, 14, 12);
    headerLayout->setSpacing(12);

    auto* title = new QLabel(tr("Outline"), m_header);
    title->setObjectName(QStringLiteral("outlineTitleLabel"));
    headerLayout->addWidget(title);

    m_combo = new QComboBox(m_header);
    m_combo->setMinimumWidth(200);
    headerLayout->addWidget(m_combo);
    connect(m_combo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_rebuilding) return;
        scheduleRebuild();
    });

    headerLayout->addStretch();

    m_summaryLine = new QLabel(m_header);
    m_summaryLine->setObjectName(QStringLiteral("outlineSummaryLabel"));
    headerLayout->addWidget(m_summaryLine);

    m_closeBtn = new QPushButton(tr("Fechar"), m_header);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(m_closeBtn);
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() { close(); });

    root->addWidget(m_header);

    // --- faixas -----------------------------------------------------------
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_stripHost = new QWidget(m_scroll);
    m_stripLayout = new QVBoxLayout(m_stripHost);
    m_stripLayout->setContentsMargins(20, 20, 20, 24);
    m_stripLayout->setSpacing(20);
    m_stripLayout->setAlignment(Qt::AlignTop);
    m_scroll->setWidget(m_stripHost);

    // --- painel de leitura ------------------------------------------------
    m_previewPane = new QWidget(this);
    m_previewPane->setObjectName(QStringLiteral("outlinePreviewPane"));
    m_previewPane->setAttribute(Qt::WA_StyledBackground, true);
    auto* pvLayout = new QVBoxLayout(m_previewPane);
    pvLayout->setContentsMargins(20, 18, 16, 18);
    pvLayout->setSpacing(6);

    m_previewTitle = new QLabel(m_previewPane);
    m_previewTitle->setObjectName(QStringLiteral("outlinePreviewTitle"));
    m_previewTitle->setWordWrap(true);
    pvLayout->addWidget(m_previewTitle);

    m_previewMeta = new QLabel(m_previewPane);
    m_previewMeta->setObjectName(QStringLiteral("outlinePreviewMeta"));
    pvLayout->addWidget(m_previewMeta);

    m_preview = new QTextBrowser(m_previewPane);
    m_preview->setFrameShape(QFrame::NoFrame);
    m_preview->setOpenExternalLinks(false);
    m_preview->setOpenLinks(false);
    pvLayout->addWidget(m_preview, /*stretch=*/1);

    m_previewEmpty = new QLabel(tr("Clique num capítulo ou numa cena para ler aqui."), m_previewPane);
    m_previewEmpty->setAlignment(Qt::AlignCenter);
    m_previewEmpty->setWordWrap(true);
    pvLayout->addWidget(m_previewEmpty, /*stretch=*/1);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->addWidget(m_scroll);
    m_splitter->addWidget(m_previewPane);
    // As faixas absorvem o espaço extra quando a janela cresce; a leitura fica
    // numa largura de coluna confortável em vez de virar um paredão de texto.
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    m_previewPane->setMinimumWidth(360);

    // A largura escolhida sobrevive ao fechar o painel: arrastar o divisor e
    // ver tudo voltar ao padrão na próxima abertura é o mesmo que ignorar a
    // escolha de quem arrastou.
    const QByteArray savedSplit = QSettings().value(QLatin1String(kSplitterKey)).toByteArray();
    if (!savedSplit.isEmpty()) m_splitter->restoreState(savedSplit);
    else m_splitter->setSizes({ 880, 500 });
    connect(m_splitter, &QSplitter::splitterMoved, this, [this](int, int) {
        QSettings().setValue(QLatin1String(kSplitterKey), m_splitter->saveState());
    });
    root->addWidget(m_splitter, /*stretch=*/1);

    clearPreview();

    if (m_model) {
        connect(m_model, &ProjectModel::chaptersChanged, this, &OutlinePanel::scheduleRebuild);
        connect(m_model, &ProjectModel::manuscriptsChanged, this, [this]() {
            syncCombo();
            scheduleRebuild();
        });
        connect(m_model, &ProjectModel::loaded, this, [this]() {
            syncCombo();
            scheduleRebuild();
        });
    }

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &OutlinePanel::applyTheme);

    applyTheme();
    syncCombo();
    rebuild();
}

void OutlinePanel::setWordCounter(WordCounter* counter)
{
    if (m_wordCounter == counter) return;
    m_wordCounter = counter;
    if (m_wordCounter) {
        connect(m_wordCounter, &WordCounter::countsChanged, this, [this]() {
            // Só com a janela aberta: as faixas inteiras são remontadas a cada
            // emissão, e essas chegam a cada digitada no editor.
            if (isVisible()) scheduleRebuild();
        });
    }
    scheduleRebuild();
}

void OutlinePanel::setDialogueStore(DialogueStore* store)
{
    if (m_dialogueStore == store) return;
    m_dialogueStore = store;
    scheduleRebuild();
}

void OutlinePanel::setElementsStore(ElementsStore* store)
{
    if (m_elementsStore == store) return;
    m_elementsStore = store;
    scheduleRebuild();
}

QString OutlinePanel::activeManuscriptId() const
{
    if (m_combo && m_combo->currentIndex() >= 0)
        return m_combo->currentData().toString();
    return m_model ? m_model->activeManuscriptId() : QString();
}

void OutlinePanel::syncCombo()
{
    if (!m_combo || !m_model) return;
    const QString keep = activeManuscriptId();

    const bool was = m_rebuilding;
    m_rebuilding = true;
    m_combo->clear();
    for (const auto& ms : m_model->manuscripts())
        m_combo->addItem(m_model->manuscriptEffectiveTitle(ms.id), ms.id);
    const int at = m_combo->findData(keep.isEmpty() ? m_model->activeManuscriptId() : keep);
    if (at >= 0) m_combo->setCurrentIndex(at);
    m_rebuilding = was;
}

void OutlinePanel::scheduleRebuild()
{
    if (m_rebuildScheduled) return;
    m_rebuildScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        m_rebuildScheduled = false;
        rebuild();
    });
}

void OutlinePanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    syncCombo();
    rebuild();
}

void OutlinePanel::closeEvent(QCloseEvent* event)
{
    QWidget::closeEvent(event);
    emit closeRequested();
}

QStringList OutlinePanel::charactersForDoc(const QString& docKey) const
{
    if (!m_elementsStore || docKey.isEmpty()) return {};
    QStringList names;
    for (const QString& id : m_elementsStore->docElementIds(docKey)) {
        const Element* el = m_elementsStore->findElement(id);
        if (!el || el->name.isEmpty()) continue;
        // Só gente: cenário e objeto também moram no ElementsStore, mas o card
        // fala "Em cena", não "elementos detectados".
        if (el->type != QStringLiteral("character")) continue;
        names.append(el->name);
    }
    names.sort(Qt::CaseInsensitive);
    return names;
}

QString OutlinePanel::previewText(const QString& linkKey, int words, int maxChars) const
{
    if (!m_docTextResolver || linkKey.isEmpty()) return QString();

    const QString cacheKey = linkKey + QLatin1Char('#') + QString::number(words)
                           + QLatin1Char('#') + QString::number(maxChars);
    const auto hit = m_previewCache.constFind(cacheKey);
    if (hit != m_previewCache.constEnd()) return hit.value();

    QString text = m_docTextResolver(linkKey).simplified();

    // Corta numa fronteira de palavra e fecha com reticências. O limite vem de
    // quanto REALMENTE cabe no card: cortar só no fim faria o QLabel comer as
    // últimas linhas em silêncio, sem reticência nenhuma — que foi o que
    // aconteceu na primeira versão, com um único limite pros dois tamanhos.
    if (text.size() > maxChars) {
        int cut = text.lastIndexOf(QLatin1Char(' '), maxChars);
        if (cut < maxChars / 2) cut = maxChars;   // palavra gigante: corta seco
        text = text.left(cut).trimmed() + QStringLiteral("…");
    }

    // O cache cresce com o manuscrito; um projeto grande de 500 capítulos
    // guardaria meio MB de prévia à toa. Esvaziar tudo é mais simples do que
    // ter política de descarte, e a próxima remontagem repovoa só o visível.
    if (m_previewCache.size() > 400) m_previewCache.clear();
    m_previewCache.insert(cacheKey, text);
    return text;
}

int OutlinePanel::dialogueWordsFor(const QString& chapterId, int sceneIndex) const
{
    if (!m_dialogueStore) return -1;
    // O store já soma por capítulo com a mesma regra de contagem do
    // WordCounter; por cena não existe pronto, então soma na mão com a regra
    // pública — que é a MESMA função, senão a proporção não fecharia.
    if (sceneIndex < 0) return m_dialogueStore->dialogueWordsForChapter(chapterId);

    int words = 0;
    for (const auto& dlg : m_dialogueStore->dialogues()) {
        if (dlg.chapterId != chapterId || dlg.sceneIndex != sceneIndex) continue;
        words += WordCounter::countWordsInPlain(dlg.text);
    }
    return words;
}

OutlineCardData OutlinePanel::buildChapterCard(const Chapter& chapter) const
{
    OutlineCardData d;
    d.chapterId = chapter.id;
    d.sceneIndex = -1;
    d.heading = m_model ? m_model->chapterDisplayLabel(chapter) : chapter.title;
    d.summary = chapter.summary;
    d.statusId = chapter.status;
    d.statusLabel = ProjectModel::findWorkStatusLabel(chapter.status);
    d.statusColor = ProjectModel::findWorkStatusColor(chapter.status);
    d.timeMarker = chapter.timeMarker;
    d.povOther = chapter.povOther;
    d.words = m_wordCounter ? m_wordCounter->countChapter(chapter.id) : 0;
    d.chapterWords = d.words;

    // Sem descrição escrita, o card mostra o começo do próprio capítulo. O
    // Outline precisa servir pra quem nunca preenche formulário — que é o uso
    // normal, não a exceção.
    if (d.summary.isEmpty()) {
        d.summary = previewText(QStringLiteral("ch:") + chapter.id, d.words, kBigPreviewChars);
        d.summaryIsPreview = !d.summary.isEmpty();
    }

    if (m_dialogueStore) {
        int n = 0;
        for (const auto& dlg : m_dialogueStore->dialogues())
            if (dlg.chapterId == chapter.id) ++n;
        d.dialogues = n;
    }
    d.dialogueWords = dialogueWordsFor(chapter.id, -1);
    d.characters = charactersForDoc(
        ElementsStore::elementDocKeyForChapter(chapter.manuscriptId, chapter.id));
    return d;
}

OutlineCardData OutlinePanel::buildSceneCard(const Chapter& chapter, int sceneIndex) const
{
    OutlineCardData d;
    if (sceneIndex < 0 || sceneIndex >= chapter.scenes.size()) return d;
    const Scene& sc = chapter.scenes.at(sceneIndex);

    d.chapterId = chapter.id;
    d.sceneIndex = sceneIndex;
    d.heading = sc.title.isEmpty() ? tr("Cena %1").arg(sceneIndex + 1) : sc.title;
    d.summary = sc.summary;
    d.statusId = sc.status;
    d.statusLabel = ProjectModel::findWorkStatusLabel(sc.status);
    d.statusColor = ProjectModel::findWorkStatusColor(sc.status);
    d.timeMarker = sc.timeMarker;
    d.povOther = sc.povOther;
    d.words = m_wordCounter ? m_wordCounter->countScene(chapter.id, sceneIndex) : 0;
    d.chapterWords = m_wordCounter ? m_wordCounter->countChapter(chapter.id) : 0;

    // Mesma escada do capítulo, um degrau a mais: texto da própria cena
    // primeiro; sem texto, herda a descrição do capítulo (como a Timeline faz);
    // sem nada disso, o começo do capítulo.
    if (d.summary.isEmpty()) {
        d.summary = previewText(QStringLiteral("sc:") + sc.id, d.words, kSmallPreviewChars);
        d.summaryIsPreview = !d.summary.isEmpty();
    }
    if (d.summary.isEmpty()) {
        d.summary = chapter.summary;
        d.summaryIsPreview = false;
    }

    if (m_dialogueStore) {
        int n = 0;
        for (const auto& dlg : m_dialogueStore->dialogues())
            if (dlg.chapterId == chapter.id && dlg.sceneIndex == sceneIndex) ++n;
        d.dialogues = n;
    }
    d.dialogueWords = dialogueWordsFor(chapter.id, sceneIndex);
    d.characters = charactersForDoc(
        ElementsStore::elementDocKeyForScene(chapter.manuscriptId, chapter.id, sc.id));
    return d;
}

void OutlinePanel::rebuild()
{
    if (!m_stripLayout || !m_model) return;

    m_rebuilding = true;
    clearChapterDropMarker();
    clearLayout(m_stripLayout);
    m_strips.clear();

    const QString msId = activeManuscriptId();
    for (const Chapter* ch : m_model->orderedChaptersForManuscript(msId)) {
        if (!ch) continue;

        QList<OutlineCardData> scenes;
        scenes.reserve(ch->scenes.size());
        for (int i = 0; i < ch->scenes.size(); ++i)
            scenes.append(buildSceneCard(*ch, i));

        int focused = m_focusedScene.value(ch->id, -1);
        if (focused >= scenes.size()) {
            // A cena em foco sumiu (foi movida ou apagada) — volta pro capítulo.
            focused = -1;
            m_focusedScene.remove(ch->id);
        }

        auto* strip = new OutlineStrip(m_stripHost);
        strip->setContent(buildChapterCard(*ch), scenes, focused);

        connect(strip, &OutlineStrip::cardClicked, this,
                [this](const QString& chapterId, int sceneIndex) {
            // Qualquer card carrega a leitura à direita. Card de cena, além
            // disso, promove/despromove a cena no card grande da faixa.
            loadPreview(chapterId, sceneIndex);
            if (sceneIndex < 0) return;
            if (m_focusedScene.value(chapterId, -1) == sceneIndex)
                m_focusedScene.remove(chapterId);
            else
                m_focusedScene.insert(chapterId, sceneIndex);
            scheduleRebuild();
        });
        connect(strip, &OutlineStrip::backToChapterRequested, this,
                [this](const QString& chapterId) {
            m_focusedScene.remove(chapterId);
            scheduleRebuild();
        });
        connect(strip, &OutlineStrip::cardContextMenuRequested,
                this, &OutlinePanel::showCardMenu);
        connect(strip, &OutlineStrip::reorderSceneRequested,
                this, &OutlinePanel::reorderSceneRequested);
        connect(strip, &OutlineStrip::moveSceneToChapterRequested,
                this, &OutlinePanel::moveSceneToChapterRequested);

        m_stripLayout->addWidget(strip);
        m_strips.append(strip);
    }

    m_stripLayout->addStretch();
    m_rebuilding = false;
    updateSummaryLine();

    // Mantém o que estava sendo lido: uma remontagem não pode esvaziar o painel
    // de leitura na cara de quem está lendo.
    if (!m_previewChapterId.isEmpty()) {
        if (m_model->findChapter(m_previewChapterId)) loadPreview(m_previewChapterId, m_previewSceneIndex);
        else clearPreview();
    }
}

void OutlinePanel::updateSummaryLine()
{
    if (!m_summaryLine || !m_model) return;
    int chapters = 0, scenes = 0, words = 0;
    for (const Chapter* ch : m_model->orderedChaptersForManuscript(activeManuscriptId())) {
        if (!ch) continue;
        ++chapters;
        scenes += ch->scenes.size();
        if (m_wordCounter) words += m_wordCounter->countChapter(ch->id);
    }
    m_summaryLine->setText(tr("%1 capítulos · %2 cenas · %3 palavras")
                               .arg(groupNumber(chapters), groupNumber(scenes), groupNumber(words)));
}

int OutlinePanel::chapterInsertIndexAt(const QPoint& posInHost) const
{
    for (int i = 0; i < m_strips.size(); ++i) {
        OutlineStrip* strip = m_strips.at(i);
        if (!strip) continue;
        if (posInHost.y() < strip->y() + strip->height() / 2) return i;
    }
    return m_strips.size();
}

void OutlinePanel::showChapterDropMarkerAt(int index)
{
    if (!m_stripHost) return;
    if (!m_chapterDropMarker) {
        m_chapterDropMarker = new QWidget(m_stripHost);
        m_chapterDropMarker->setFixedHeight(2);
        m_chapterDropMarker->setStyleSheet(QStringLiteral("background: %1; border-radius: 1px;")
                                               .arg(Theme::accentDefault()));
    }
    const int y = (index < m_strips.size() && m_strips.at(index))
        ? m_strips.at(index)->y() - 10
        : (m_strips.isEmpty() ? 10 : m_strips.last()->y() + m_strips.last()->height() + 8);
    m_chapterDropMarker->setGeometry(20, y, qMax(0, m_stripHost->width() - 40), 2);
    m_chapterDropMarker->show();
    m_chapterDropMarker->raise();
}

void OutlinePanel::clearChapterDropMarker()
{
    if (!m_chapterDropMarker) return;
    m_chapterDropMarker->hide();
    m_chapterDropMarker->deleteLater();
    m_chapterDropMarker = nullptr;
}

void OutlinePanel::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(QLatin1String(kChapterMime)))
        event->acceptProposedAction();
}

void OutlinePanel::dragMoveEvent(QDragMoveEvent* event)
{
    if (!event->mimeData()->hasFormat(QLatin1String(kChapterMime))) return;
    if (!m_stripHost) return;
    const QPoint posInHost = m_stripHost->mapFrom(this, event->position().toPoint());
    showChapterDropMarkerAt(chapterInsertIndexAt(posInHost));
    event->acceptProposedAction();
}

void OutlinePanel::dragLeaveEvent(QDragLeaveEvent*)
{
    clearChapterDropMarker();
}

void OutlinePanel::dropEvent(QDropEvent* event)
{
    if (!event->mimeData()->hasFormat(QLatin1String(kChapterMime))) return;
    if (!m_stripHost) { clearChapterDropMarker(); return; }

    const QPoint posInHost = m_stripHost->mapFrom(this, event->position().toPoint());
    int targetIndex = chapterInsertIndexAt(posInHost);
    clearChapterDropMarker();

    const QString chapterId = QString::fromUtf8(event->mimeData()->data(QLatin1String(kChapterMime)));
    if (chapterId.isEmpty()) return;

    // Índice atual da faixa arrastada, pra não pedir um movimento que é parada.
    int srcIndex = -1;
    for (int i = 0; i < m_strips.size(); ++i)
        if (m_strips.at(i) && m_strips.at(i)->chapterId() == chapterId) { srcIndex = i; break; }
    if (srcIndex >= 0 && (targetIndex == srcIndex || targetIndex == srcIndex + 1)) {
        event->acceptProposedAction();
        return;
    }

    emit reorderChapterRequested(chapterId, targetIndex);
    event->acceptProposedAction();
}

void OutlinePanel::setProjectRoot(const QString& root)
{
    m_projectRoot = root;
    if (m_preview && !root.isEmpty()) {
        // Sem isto, imagem referenciada por caminho relativo no HTML salvo não
        // resolve e o documento aparece com buraco no lugar dela.
        m_preview->setSearchPaths({ root, root + QStringLiteral("/content/images") });
    }
}

void OutlinePanel::clearPreview()
{
    m_previewChapterId.clear();
    m_previewSceneIndex = -1;
    if (m_preview) m_preview->clear();
    if (m_preview) m_preview->setVisible(false);
    if (m_previewTitle) m_previewTitle->setVisible(false);
    if (m_previewMeta) m_previewMeta->setVisible(false);
    if (m_previewEmpty) m_previewEmpty->setVisible(true);
}

void OutlinePanel::loadPreview(const QString& chapterId, int sceneIndex)
{
    if (!m_model || !m_preview) return;
    const Chapter* ch = m_model->findChapter(chapterId);
    if (!ch) { clearPreview(); return; }

    const bool isScene = sceneIndex >= 0 && sceneIndex < ch->scenes.size();
    m_previewChapterId = chapterId;
    m_previewSceneIndex = isScene ? sceneIndex : -1;

    // --- cabeçalho --------------------------------------------------------
    const QString chapterLabel = m_model->chapterDisplayLabel(*ch);
    QString title = chapterLabel;
    if (isScene) {
        const Scene& sc = ch->scenes.at(sceneIndex);
        title += QStringLiteral(" · ")
               + (sc.title.isEmpty() ? tr("Cena %1").arg(sceneIndex + 1) : sc.title);
    } else if (!ch->title.isEmpty() && !chapterLabel.contains(ch->title)) {
        title += QStringLiteral(" — ") + ch->title;
    }
    m_previewTitle->setText(title);
    m_previewTitle->setVisible(true);

    const int words = m_wordCounter
        ? (isScene ? m_wordCounter->countScene(chapterId, sceneIndex)
                   : m_wordCounter->countChapter(chapterId))
        : 0;
    QStringList bits;
    bits << tr("%1 palavras").arg(groupNumber(words));
    const int dlg = dialogueWordsFor(chapterId, isScene ? sceneIndex : -1);
    if (dlg >= 0 && words > 0)
        bits << tr("%1% diálogo").arg(qRound(100.0 * qBound(0, dlg, words) / words));
    const QString when = isScene ? ch->scenes.at(sceneIndex).timeMarker : ch->timeMarker;
    if (!when.isEmpty()) bits << when;
    m_previewMeta->setText(bits.join(QStringLiteral(" · ")));
    m_previewMeta->setVisible(true);

    // --- corpo ------------------------------------------------------------
    QString html;
    if (m_docHtmlResolver) {
        html = m_docHtmlResolver(isScene
                                     ? QStringLiteral("sc:") + ch->scenes.at(sceneIndex).id
                                     : QStringLiteral("ch:") + chapterId);
    }

    if (html.trimmed().isEmpty()) {
        m_preview->setHtml(QStringLiteral("<p style='color:%2;'><i>%1</i></p>")
                               .arg(tr("(documento vazio)"), Theme::textMuted()));
    } else {
        m_preview->setHtml(html);
        // setHtml troca o documento, então a margem zerada na construção do
        // widget não vale mais — tem que ser reaplicada a cada carga.
        m_preview->document()->setDocumentMargin(0);
        // Ordem importa: primeiro o tamanho (mexe no charFormat de tudo),
        // depois a cor — senão o merge de fonte reescreveria o foreground.
        DocPreview::applyPreviewFontSize(m_preview->document(), kPreviewFontPt);
        // Obrigatório: o HTML salvo carrega a cor do tema da época em que foi
        // escrito. Sem isto, texto escrito no escuro some no claro.
        DocPreview::applyThemeTextColors(m_preview->document());
    }

    m_preview->setVisible(true);
    m_previewEmpty->setVisible(false);
    m_preview->verticalScrollBar()->setValue(0);
}

void OutlinePanel::showCardMenu(const QString& chapterId, int sceneIndex, const QPoint& globalPos)
{
    if (!m_model) return;
    const Chapter* ch = m_model->findChapter(chapterId);
    if (!ch) return;
    const bool isScene = sceneIndex >= 0;
    if (isScene && (sceneIndex >= ch->scenes.size())) return;

    QMenu menu(this);
    QAction* open = menu.addAction(isScene ? tr("Abrir cena no editor")
                                           : tr("Abrir capítulo no editor"));
    QAction* ref = menu.addAction(isScene ? tr("Abrir cena no menu de referência")
                                          : tr("Abrir capítulo no menu de referência"));
    menu.addSeparator();
    QAction* dialogues = menu.addAction(tr("Ver diálogos deste capítulo"));

    menu.addSeparator();
    QMenu* statusMenu = menu.addMenu(tr("Status"));
    const QString current = isScene ? ch->scenes.at(sceneIndex).status : ch->status;
    QAction* none = statusMenu->addAction(tr("Sem estágio"));
    none->setCheckable(true);
    none->setChecked(current.isEmpty());
    none->setData(QString());
    for (const auto& st : ProjectModel::workStatuses()) {
        QAction* act = statusMenu->addAction(st.label);
        act->setCheckable(true);
        act->setChecked(current == st.id);
        act->setData(st.id);
    }

    QAction* pov = menu.addAction(tr("Ponto de vista de outro personagem"));
    pov->setCheckable(true);
    pov->setChecked(isScene ? ch->scenes.at(sceneIndex).povOther : ch->povOther);

    menu.addSeparator();
    QAction* rename = menu.addAction(isScene ? tr("Renomear cena") : tr("Renomear capítulo"));
    QAction* describe = menu.addAction(tr("Alterar descrição"));

    QAction* chosen = menu.exec(globalPos);
    if (!chosen) return;

    if (chosen == open) {
        emit openRequested(ch->manuscriptId, chapterId, sceneIndex);
    } else if (chosen == ref) {
        emit openInRefMenuRequested(ch->manuscriptId, chapterId, sceneIndex);
    } else if (chosen == dialogues) {
        emit chapterDialoguesRequested(ch->manuscriptId, chapterId);
    } else if (chosen == pov) {
        if (isScene) m_model->updateScenePovOther(chapterId, sceneIndex, chosen->isChecked());
        else m_model->updateChapterPovOther(chapterId, chosen->isChecked());
    } else if (chosen == rename) {
        promptRename(chapterId, sceneIndex);
    } else if (chosen == describe) {
        promptSummary(chapterId, sceneIndex);
    } else if (chosen->parentWidget() == statusMenu) {
        const QString id = chosen->data().toString();
        if (isScene) m_model->updateSceneStatus(chapterId, sceneIndex, id);
        else m_model->updateChapterStatus(chapterId, id);
    }
}

void OutlinePanel::promptRename(const QString& chapterId, int sceneIndex)
{
    if (!m_model) return;
    const Chapter* ch = m_model->findChapter(chapterId);
    if (!ch) return;
    const bool isScene = sceneIndex >= 0 && sceneIndex < ch->scenes.size();

    const QString current = isScene ? ch->scenes.at(sceneIndex).title : ch->title;
    bool ok = false;
    const QString value = QInputDialog::getText(
        this,
        isScene ? tr("Renomear cena") : tr("Renomear capítulo"),
        tr("Título:"), QLineEdit::Normal, current, &ok);
    if (!ok) return;

    if (isScene) m_model->updateSceneTitle(chapterId, sceneIndex, value.trimmed());
    else m_model->updateChapterTitle(chapterId, value.trimmed());
}

void OutlinePanel::promptSummary(const QString& chapterId, int sceneIndex)
{
    if (!m_model) return;
    const Chapter* ch = m_model->findChapter(chapterId);
    if (!ch) return;
    const bool isScene = sceneIndex >= 0 && sceneIndex < ch->scenes.size();

    const QString current = isScene ? ch->scenes.at(sceneIndex).summary : ch->summary;
    bool ok = false;
    const QString value = QInputDialog::getMultiLineText(
        this, tr("Alterar descrição"),
        tr("Esta descrição é a mesma que aparece na Timeline."),
        current, &ok);
    if (!ok) return;

    if (isScene) m_model->updateSceneSummary(chapterId, sceneIndex, value.trimmed());
    else m_model->updateChapterSummary(chapterId, value.trimmed());
}

void OutlinePanel::applyTheme()
{
    setStyleSheet(Theme::panelQss(QStringLiteral("outlinePanel")));

    if (m_header) {
        m_header->setStyleSheet(QStringLiteral(
            "#outlineHeader { background: %1; border-bottom: 1px solid %2; }")
            .arg(Theme::panelBackground(), Theme::panelBorder()));
    }
    if (auto* title = m_header ? m_header->findChild<QLabel*>(QStringLiteral("outlineTitleLabel")) : nullptr) {
        title->setStyleSheet(QStringLiteral("color: %1; font-size: 20px; font-weight: 600;")
                                 .arg(Theme::textBright()));
    }
    if (m_summaryLine) {
        m_summaryLine->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                         .arg(Theme::textMuted()));
    }
    if (m_combo) {
        m_combo->setStyleSheet(Theme::qss(QStringLiteral(R"(
            QComboBox {
                background: transparent;
                color: %1;
                border: 1px solid %2;
                border-radius: @radius-control;
                padding: 5px 10px;
                font-size: 13px;
            }
            QComboBox:hover { border-color: %3; }
            QComboBox::drop-down { border: none; width: 18px; }
            QComboBox QAbstractItemView {
                background: %4;
                color: %1;
                border: 1px solid %2;
                selection-background-color: %5;
            }
        )")).arg(Theme::textBright(), Theme::panelBorder(), Theme::subtleBorder(),
               Theme::panelBackground(), Theme::hoverOverlay()));
    }
    if (m_closeBtn) {
        m_closeBtn->setStyleSheet(Theme::qss(QStringLiteral(R"(
            QPushButton {
                background: transparent;
                color: %1;
                border: 1px solid %2;
                border-radius: @radius-control;
                padding: 5px 16px;
            }
            QPushButton:hover { background: %3; }
        )")).arg(Theme::textMuted(), Theme::panelBorder(), Theme::hoverOverlay()));
    }
    if (m_previewPane) {
        m_previewPane->setStyleSheet(QStringLiteral(
            "#outlinePreviewPane { background: %1; border-left: 1px solid %2; }")
            .arg(Theme::panelBackground(), Theme::panelBorder()));
    }
    if (m_previewTitle) {
        m_previewTitle->setStyleSheet(QStringLiteral("color: %1; font-size: 17px; font-weight: 600;")
                                          .arg(Theme::textBright()));
    }
    if (m_previewMeta) {
        m_previewMeta->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                                         .arg(Theme::textMuted()));
    }
    if (m_previewEmpty) {
        m_previewEmpty->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-style: italic;")
                                          .arg(Theme::disabledText()));
    }
    if (m_preview) {
        // `padding: 0` NÃO é decoração: o stylesheet global do app tem
        // `QTextEdit { padding: 80px 100px; }` pra dar margem de página ao
        // editor, e QTextBrowser herda de QTextEdit — sem anular isso aqui, o
        // texto perde 100px de cada lado dentro de um painel estreito. O
        // preview do RefMenu faz o mesmo, pelo mesmo motivo.
        m_preview->setStyleSheet(QStringLiteral(
            "QTextBrowser { background: transparent; color: %1; border: none; padding: 0; }")
            .arg(Theme::textPrimary()));
        if (m_preview->viewport())
            m_preview->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
        // Recarrega: a correção de cor foi feita com o tema antigo.
        if (!m_previewChapterId.isEmpty() && !m_rebuilding)
            loadPreview(m_previewChapterId, m_previewSceneIndex);
    }
    if (m_scroll && m_scroll->viewport()) {
        // Viewport não herda fundo transparente do pai — tem que ser explícito,
        // senão fica com a cor nativa do estilo por cima do tema.
        m_scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    }

    // As cores dos cards vêm do tema e são pintadas item a item, não por QSS
    // herdado: trocar de tema exige remontar as faixas.
    if (!m_rebuilding) scheduleRebuild();
}
