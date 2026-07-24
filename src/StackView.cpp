#include "StackView.h"

#include <QCheckBox>
#include <QContextMenuEvent>
#include <QEasingCurve>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWheelEvent>

namespace {
constexpr int kWheelDetent = 120;      // angleDelta() por "clique" de roda
constexpr int kCrossfadeMs = 240;
constexpr int kSideCrossfadeMs = 200;
constexpr int kTextColWidth = 320;     // coluna de texto mais estreita que o espaço todo
}

StackView::StackView(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("menuStackView"));

    // Layout externo: posiciona o trio (herói/texto/pilha) mais pra baixo
    // dentro do espaço disponível — stretch de CIMA bem maior que o de
    // baixo, então a folga concentra em cima e o conteúdo fica mais perto
    // do fundo do que do topo.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 20, 24, 20);
    outer->addStretch(3);

    // Linha interna: centraliza o trio horizontalmente também.
    auto* row = new QHBoxLayout();
    row->setSpacing(30);
    row->addStretch(1);

    // --- Herói: palco sem QLayout (mesmo idiom do m_stage/m_coverLbl do
    // BookCard), com folga lateral pra caber o slide da transição. ---
    m_heroStage = new QWidget(this);
    m_heroStage->setCursor(Qt::PointingHandCursor);
    m_heroStage->installEventFilter(this);
    row->addWidget(m_heroStage, 0, Qt::AlignVCenter);

    m_heroCoverLbl = new QLabel(m_heroStage);
    m_heroCoverLbl->setScaledContents(true);
    m_heroCoverLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // --- Texto: título/autor/breadcrumb/sinopse (com scroll)/auto-abrir.
    // Largura fixa, mais estreita que o espaço todo — não é pra esticar. ---
    m_textCol = new QWidget(this);
    m_textCol->setFixedWidth(kTextColWidth);
    auto* textLay = new QVBoxLayout(m_textCol);
    textLay->setContentsMargins(0, 0, 0, 0);
    textLay->setSpacing(8);

    m_heroTitleLbl = new QLabel(m_textCol);
    m_heroTitleLbl->setObjectName(QStringLiteral("stackHeroTitle"));
    m_heroTitleLbl->setWordWrap(true);
    textLay->addWidget(m_heroTitleLbl);

    m_heroAuthorLbl = new QLabel(m_textCol);
    m_heroAuthorLbl->setObjectName(QStringLiteral("stackHeroAuthor"));
    m_heroAuthorLbl->setWordWrap(true);
    textLay->addWidget(m_heroAuthorLbl);

    m_heroBreadcrumbLbl = new QLabel(m_textCol);
    m_heroBreadcrumbLbl->setObjectName(QStringLiteral("stackHeroBreadcrumb"));
    m_heroBreadcrumbLbl->setWordWrap(true);
    textLay->addWidget(m_heroBreadcrumbLbl);

    m_synopsisLbl = new QLabel;
    m_synopsisLbl->setObjectName(QStringLiteral("stackSynopsisText"));
    m_synopsisLbl->setWordWrap(true);
    m_synopsisLbl->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_synopsisScroll = new QScrollArea(m_textCol);
    m_synopsisScroll->setObjectName(QStringLiteral("stackSynopsisScroll"));
    m_synopsisScroll->setWidget(m_synopsisLbl);
    m_synopsisScroll->setWidgetResizable(true);
    m_synopsisScroll->setFrameShape(QFrame::NoFrame);
    m_synopsisScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // O QSS global (#stackSynopsisScroll) não vence o fundo padrão da
    // paleta no viewport interno do QAbstractScrollArea — força
    // transparência direto no widget, redundante de propósito (autoFill +
    // stylesheet local), pra garantir que nenhuma caixa apareça atrás do
    // texto da sinopse.
    m_synopsisScroll->viewport()->setAutoFillBackground(false);
    m_synopsisScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    m_synopsisLbl->setAutoFillBackground(false);
    textLay->addWidget(m_synopsisScroll, 1);

    m_heroAutoOpenChk = new QCheckBox(tr("Abrir automaticamente"), m_textCol);
    m_heroAutoOpenChk->setObjectName(QStringLiteral("bookCardAutoOpen"));
    m_heroAutoOpenChk->setCursor(Qt::PointingHandCursor);
    connect(m_heroAutoOpenChk, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_entries.isEmpty()) emit autoOpenChanged(m_entries[0].path, checked);
    });
    textLay->addWidget(m_heroAutoOpenChk);

    // Sem AlignVCenter aqui: a coluna precisa esticar pra altura toda da
    // linha (mesma altura da capa herói) pra sobrar espaço de verdade pro
    // stretch da sinopse — só a LARGURA é fixa (setFixedWidth acima).
    row->addWidget(m_textCol, 0);

    // --- Pilha lateral: pool fixo de QLabel, posicionados à mão (peek
    // horizontal — cada capa revela uma fatia da seguinte, da esquerda pra
    // direita, como um leque). ---
    m_sidePile = new QWidget(this);
    row->addWidget(m_sidePile, 0, Qt::AlignVCenter);
    for (int i = 0; i < kVisibleSideSlots; ++i) {
        auto* slot = new QLabel(m_sidePile);
        slot->setScaledContents(true);
        slot->setCursor(Qt::PointingHandCursor);
        slot->installEventFilter(this);
        slot->hide();
        m_sideSlots.append(slot);
        m_sideLastPixmaps.append(QPixmap());
    }

    row->addStretch(1);
    outer->addLayout(row);
    outer->addStretch(1);
}

void StackView::setEntries(const QVector<StackEntry>& entries)
{
    m_entries = entries;
    m_wheelAccum = 0;
    m_animating = false;
    rebuildHeroPanel(/*animate=*/false, 0);
    rebuildSidePile(/*animate=*/false);
}

void StackView::rotateBy(int k)
{
    const int n = m_entries.size();
    if (n < 2 || k == 0 || m_animating) return;
    k = ((k % n) + n) % n;
    if (k == 0) return;

    const QVector<StackEntry> rotated = m_entries.mid(k) + m_entries.mid(0, k);
    const int direction = (k <= n - k) ? 1 : -1;
    m_animating = true;
    m_entries = rotated;
    rebuildHeroPanel(/*animate=*/true, direction);
    rebuildSidePile(/*animate=*/true);
}

void StackView::rebuildHeroPanel(bool animate, int direction)
{
    if (m_entries.isEmpty()) {
        m_heroStage->hide();
        m_textCol->hide();
        return;
    }
    m_heroStage->show();
    m_textCol->show();

    const StackEntry& hero = m_entries[0];
    const QSize coverSz = hero.heroCover.size();
    const QSize stageSz(coverSz.width() + kHeroSlideMargin * 2, coverSz.height());
    m_heroStage->setFixedSize(stageSz);
    m_heroCoverLbl->setFixedSize(coverSz);
    const QPoint restPos((stageSz.width() - coverSz.width()) / 2, 0);
    m_heroCoverLbl->move(restPos);

    if (animate && !m_heroLastPixmap.isNull()) {
        crossfadeLabel(m_heroCoverLbl, m_heroLastPixmap, hero.heroCover, kCrossfadeMs, direction);
        QTimer::singleShot(kCrossfadeMs + 20, this, [this]() { m_animating = false; });
    } else {
        m_heroCoverLbl->setPixmap(hero.heroCover);
    }
    m_heroLastPixmap = hero.heroCover;

    m_heroTitleLbl->setText(hero.name);
    m_heroAuthorLbl->setText(hero.author);
    m_heroAuthorLbl->setVisible(!hero.author.isEmpty());

    QStringList crumbs;
    if (!hero.genres.isEmpty()) crumbs << hero.genres;
    if (hero.totalWords >= 0) crumbs << tr("%1 palavras").arg(hero.totalWords);
    m_heroBreadcrumbLbl->setText(crumbs.join(QStringLiteral("   ·   ")));
    m_heroBreadcrumbLbl->setVisible(!crumbs.isEmpty());

    m_synopsisLbl->setText(hero.synopsis);
    m_synopsisScroll->setVisible(!hero.synopsis.isEmpty());

    {
        const QSignalBlocker blocker(m_heroAutoOpenChk);
        m_heroAutoOpenChk->setChecked(hero.autoOpen);
    }
}

void StackView::rebuildSidePile(bool animate)
{
    const int n = m_entries.size();
    const int visible = qMin(kVisibleSideSlots, qMax(0, n - 1));

    QSize slotSz;
    for (int e = 1; e < n; ++e) {
        if (!m_entries[e].sideCover.isNull()) { slotSz = m_entries[e].sideCover.size(); break; }
    }

    const int pileW = slotSz.isEmpty() ? 0 : slotSz.width() + kSidePeekOffset * qMax(0, visible - 1);
    m_sidePile->setFixedSize(pileW, slotSz.isEmpty() ? 0 : slotSz.height());

    for (int i = 0; i < kVisibleSideSlots; ++i) {
        QLabel* slot = m_sideSlots[i];
        if (i >= visible) {
            slot->hide();
            m_sideLastPixmaps[i] = QPixmap();
            continue;
        }

        const StackEntry& entry = m_entries[1 + i];
        slot->setGeometry(i * kSidePeekOffset, 0, slotSz.width(), slotSz.height());
        slot->show();

        if (animate) {
            crossfadeLabel(slot, m_sideLastPixmaps[i], entry.sideCover, kSideCrossfadeMs);
        } else {
            slot->setPixmap(entry.sideCover);
        }
        m_sideLastPixmaps[i] = entry.sideCover;
    }
    // Slot 0 (próximo a virar herói) fica por cima, cobrindo a fatia
    // esquerda dos vizinhos — como um leque de cartas.
    for (int i = kVisibleSideSlots - 1; i >= 0; --i) m_sideSlots[i]->raise();
}

void StackView::crossfadeLabel(QLabel* label, const QPixmap& from, const QPixmap& to,
                                int durationMs, int slideDirection)
{
    if (!label) return;
    if (from.isNull() || to.isNull() || from.size() != to.size()) {
        label->setPixmap(to);
        return;
    }

    const QSize sz = to.size();
    auto* fade = new QVariantAnimation(this);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setDuration(durationMs);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    connect(fade, &QVariantAnimation::valueChanged, label, [label, from, to, sz](const QVariant& v) {
        const qreal t = v.toReal();
        QPixmap blended(sz);
        blended.fill(Qt::transparent);
        QPainter p(&blended);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setOpacity(1.0 - t);
        p.drawPixmap(0, 0, from);
        p.setOpacity(t);
        p.drawPixmap(0, 0, to);
        p.end();
        label->setPixmap(blended);
    });
    connect(fade, &QVariantAnimation::finished, label, [label, to]() { label->setPixmap(to); });
    fade->start(QAbstractAnimation::DeleteWhenStopped);

    if (slideDirection != 0) {
        const QPoint restPos = label->pos();
        const QPoint fromPos = restPos + QPoint(slideDirection * kHeroSlideMargin, 0);
        label->move(fromPos);
        auto* slide = new QPropertyAnimation(label, "pos", this);
        slide->setDuration(durationMs);
        slide->setEasingCurve(QEasingCurve::OutCubic);
        slide->setStartValue(fromPos);
        slide->setEndValue(restPos);
        slide->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void StackView::wheelEvent(QWheelEvent* event)
{
    event->accept();
    if (m_entries.size() < 2 || m_animating) return;

    m_wheelAccum += event->angleDelta().y();
    if (m_wheelAccum >= kWheelDetent) {
        m_wheelAccum = 0;
        rotateBy(-1);
    } else if (m_wheelAccum <= -kWheelDetent) {
        m_wheelAccum = 0;
        rotateBy(1);
    }
}

bool StackView::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            if (watched == m_heroStage) {
                if (!m_entries.isEmpty()) emit openRequested(m_entries[0].path);
                return true;
            }
            const int slotIdx = m_sideSlots.indexOf(qobject_cast<QLabel*>(watched));
            if (slotIdx >= 0 && m_sideSlots[slotIdx]->isVisible()) {
                rotateBy(1 + slotIdx);
                return true;
            }
        }
    } else if (event->type() == QEvent::ContextMenu) {
        auto* ce = static_cast<QContextMenuEvent*>(event);
        if (watched == m_heroStage) {
            if (!m_entries.isEmpty()) showContextMenuFor(m_entries[0].path, ce->globalPos());
            return true;
        }
        const int slotIdx = m_sideSlots.indexOf(qobject_cast<QLabel*>(watched));
        if (slotIdx >= 0 && m_sideSlots[slotIdx]->isVisible() && 1 + slotIdx < m_entries.size()) {
            showContextMenuFor(m_entries[1 + slotIdx].path, ce->globalPos());
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void StackView::showContextMenuFor(const QString& path, const QPoint& globalPos)
{
    QMenu menu(this);
    menu.setObjectName(QStringLiteral("bookCardMenu"));
    QAction* aEdit   = menu.addAction(tr("Editar projeto"));
    QAction* aCover  = menu.addAction(tr("Criar capa"));
    menu.addSeparator();
    QAction* aRemove = menu.addAction(tr("Remover dos recentes"));
    menu.addSeparator();
    QAction* aDelete = menu.addAction(tr("Excluir projeto"));
    QAction* chosen = menu.exec(globalPos);
    if      (chosen == aEdit)   emit editRequested(path);
    else if (chosen == aCover)  emit coverCreateRequested(path);
    else if (chosen == aRemove) emit removeRequested(path);
    else if (chosen == aDelete) emit deleteRequested(path);
}
