#include "StackView.h"

#include <QCheckBox>
#include <QContextMenuEvent>
#include <QEasingCurve>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QImage>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QResizeEvent>
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
constexpr int kBackdropW = 900;        // "canvas" fixo do hero banner — o QLabel
constexpr int kBackdropH = 560;        // estica isso pra geometria real via setScaledContents

// Colagem N×N da MESMA capa, cada célula preenchida sem distorcer (recorta
// o excesso, igual um "object-fit: cover" por célula). Serve de base pro
// backdrop — ver buildBackdrop() logo abaixo pro porquê de colagem em vez de
// uma capa só esticada.
QPixmap buildCoverCollage(const QPixmap& src, const QSize& targetSize, int cols, int rows)
{
    QPixmap canvas(targetSize);
    canvas.fill(Qt::black);
    if (src.isNull() || cols <= 0 || rows <= 0) return canvas;

    const int cellW = targetSize.width() / cols;
    const int cellH = targetSize.height() / rows;
    QPainter p(&canvas);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const QPixmap filled = src.scaled(QSize(cellW, cellH), Qt::KeepAspectRatioByExpanding,
                                               Qt::SmoothTransformation);
            const int x = c * cellW - (filled.width() - cellW) / 2;
            const int y = r * cellH - (filled.height() - cellH) / 2;
            p.save();
            p.setClipRect(c * cellW, r * cellH, cellW, cellH);
            p.drawPixmap(x, y, filled);
            p.restore();
        }
    }
    p.end();
    return canvas;
}

// Box blur separável (horizontal + vertical, janela deslizante O(w·h),
// sem depender de radius² por pixel). Uma passada só já borra; três
// passadas em sequência (ver tripleBoxBlur) aproxima um blur gaussiano de
// verdade — é a técnica clássica "3x box blur ≈ gaussian blur", bem mais
// barata que convolução gaussiana de verdade e sem os artefatos do truque
// de "encolhe bem pequeno e escala de volta" (que ficava com cara de
// thumbnail de baixa resolução esticada, não de foto borrada).
QImage boxBlurPass(const QImage& srcIn, int radius)
{
    if (radius <= 0) return srcIn;
    const QImage src = srcIn.format() == QImage::Format_ARGB32
                      ? srcIn : srcIn.convertToFormat(QImage::Format_ARGB32);
    const int w = src.width(), h = src.height();
    const int win = 2 * radius + 1;

    // Passe horizontal: src -> tmp.
    QImage tmp(w, h, QImage::Format_ARGB32);
    for (int y = 0; y < h; ++y) {
        const QRgb* s = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        QRgb* d = reinterpret_cast<QRgb*>(tmp.scanLine(y));
        long sa = 0, sr = 0, sg = 0, sb = 0;
        for (int dx = -radius; dx <= radius; ++dx) {
            const QRgb p = s[qBound(0, dx, w - 1)];
            sa += qAlpha(p); sr += qRed(p); sg += qGreen(p); sb += qBlue(p);
        }
        for (int x = 0; x < w; ++x) {
            d[x] = qRgba(int(sr / win), int(sg / win), int(sb / win), int(sa / win));
            const QRgb pOut = s[qBound(0, x - radius, w - 1)];
            const QRgb pIn  = s[qBound(0, x + radius + 1, w - 1)];
            sa += qAlpha(pIn) - qAlpha(pOut);
            sr += qRed(pIn)   - qRed(pOut);
            sg += qGreen(pIn) - qGreen(pOut);
            sb += qBlue(pIn)  - qBlue(pOut);
        }
    }

    // Passe vertical: tmp -> out.
    QImage out(w, h, QImage::Format_ARGB32);
    for (int x = 0; x < w; ++x) {
        long sa = 0, sr = 0, sg = 0, sb = 0;
        for (int dy = -radius; dy <= radius; ++dy) {
            const QRgb p = reinterpret_cast<const QRgb*>(tmp.constScanLine(qBound(0, dy, h - 1)))[x];
            sa += qAlpha(p); sr += qRed(p); sg += qGreen(p); sb += qBlue(p);
        }
        for (int y = 0; y < h; ++y) {
            reinterpret_cast<QRgb*>(out.scanLine(y))[x] =
                qRgba(int(sr / win), int(sg / win), int(sb / win), int(sa / win));
            const QRgb pOut = reinterpret_cast<const QRgb*>(tmp.constScanLine(qBound(0, y - radius, h - 1)))[x];
            const QRgb pIn  = reinterpret_cast<const QRgb*>(tmp.constScanLine(qBound(0, y + radius + 1, h - 1)))[x];
            sa += qAlpha(pIn) - qAlpha(pOut);
            sr += qRed(pIn)   - qRed(pOut);
            sg += qGreen(pIn) - qGreen(pOut);
            sb += qBlue(pIn)  - qBlue(pOut);
        }
    }
    return out;
}

QImage tripleBoxBlur(const QImage& src, int radius)
{
    QImage img = boxBlurPass(src, radius);
    img = boxBlurPass(img, radius);
    img = boxBlurPass(img, radius);
    return img;
}

// Hero banner atrás da composição: colagem 4×4 da capa em foco (mesma capa
// repetida 16x), borrada, escurecida e com as bordas esmaecidas — mesma
// ideia do banner de destaque do Netflix/Spotify, preenchendo o vão vazio
// de cima sem competir com o texto por cima. Colagem em vez de uma capa só
// ampliada: capa de usuário costuma ter resolução baixa, e esticar UMA
// cópia pro tamanho do banner inteiro (900×560) pedia um upscale grande
// demais — ficava visivelmente pixelado/serrilhado mesmo borrado. Células
// menores = upscale bem menor (ou nenhum) por célula, e o blur por cima
// ainda esconde as costuras entre elas — 4×4 (em vez de 2×2) porque 2×2
// ainda deixava pixelização visível. Blur
// "pobre" de propósito (encolhe bem pequeno e escala de volta suavizando)
// em vez de QGraphicsBlurEffect — este widget mora dentro do mesmo
// QScrollArea externo (m_recentsScroll) cujo comentário em BookCard já
// registra que QGraphicsEffect corta o repaint ali dentro (ver Gotcha 1 na
// memória do modo Pilha).
QPixmap buildBackdrop(const QPixmap& src, const QSize& targetSize)
{
    if (src.isNull() || targetSize.isEmpty()) return {};

    const QPixmap collage = buildCoverCollage(src, targetSize, 4, 4);

    // Blur de verdade (box blur triplo, ver tripleBoxBlur) em vez do
    // truque de "encolhe bem pequeno e escala de volta" — aquele truque
    // precisava encolher demais (~1/24 do tamanho) pro blur ficar forte o
    // bastante, e reescalar de volta um recorte tão pequeno pro tamanho
    // final tem cara de thumbnail de baixa resolução esticada, não de foto
    // borrada (era exatamente a reclamação: "parece a foto de baixa
    // qualidade", não um blur suave). Trabalha em meia resolução (mais
    // barato, e o upscale final de só 2x no fim não introduz esse
    // artefato).
    const QSize halfSize(qMax(1, targetSize.width() / 2), qMax(1, targetSize.height() / 2));
    const QImage half = collage.scaled(halfSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                .toImage();
    const QImage blurredImg = tripleBoxBlur(half, 14);
    const QPixmap blurred = QPixmap::fromImage(blurredImg)
                                 .scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QPixmap out(targetSize);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawPixmap(0, 0, blurred);

    // Escurece (respeita o alfa já existente — imagem ainda opaca aqui).
    p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    p.fillRect(out.rect(), QColor(0, 0, 0, 120));

    // Esmaece as quatro bordas: duas passadas de gradiente linear
    // (horizontal + vertical) multiplicando a máscara de alfa, sem precisar
    // de transformação pra virar elipse.
    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    QLinearGradient gh(0, 0, targetSize.width(), 0);
    gh.setColorAt(0.00, QColor(255, 255, 255, 0));
    gh.setColorAt(0.20, QColor(255, 255, 255, 190));
    gh.setColorAt(0.80, QColor(255, 255, 255, 190));
    gh.setColorAt(1.00, QColor(255, 255, 255, 0));
    p.fillRect(out.rect(), gh);

    QLinearGradient gv(0, 0, 0, targetSize.height());
    gv.setColorAt(0.00, QColor(255, 255, 255, 0));
    gv.setColorAt(0.22, QColor(255, 255, 255, 255));
    gv.setColorAt(0.78, QColor(255, 255, 255, 255));
    gv.setColorAt(1.00, QColor(255, 255, 255, 0));
    p.fillRect(out.rect(), gv);

    p.end();
    return out;
}
}

StackView::StackView(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("menuStackView"));

    // Hero banner — criado ANTES de tudo, fora do QLayout, pra ficar atrás
    // de todo o resto na ordem de empilhamento (Qt pinta filhos por ordem
    // de criação; o mais novo cobre o mais velho quando se sobrepõem).
    // Geometria real calculada em resizeEvent(); o pixmap fixo (kBackdropW×
    // kBackdropH) é esticado pra ela via setScaledContents.
    m_backdropLbl = new QLabel(this);
    m_backdropLbl->setObjectName(QStringLiteral("stackBackdrop"));
    m_backdropLbl->setScaledContents(true);
    m_backdropLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_backdropLbl->hide();

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

    // --- Herói: legenda discreta de estatísticas + palco da capa (sem
    // QLayout, mesmo idiom do m_stage/m_coverLbl do BookCard, com folga
    // lateral pra caber o slide da transição) empilhados verticalmente. ---
    auto* heroCol = new QWidget(this);
    auto* heroColLay = new QVBoxLayout(heroCol);
    heroColLay->setContentsMargins(0, 0, 0, 0);
    heroColLay->setSpacing(8);

    m_heroStatsLbl = new QLabel(heroCol);
    m_heroStatsLbl->setObjectName(QStringLiteral("stackHeroStats"));
    m_heroStatsLbl->setAlignment(Qt::AlignHCenter);
    heroColLay->addWidget(m_heroStatsLbl, 0, Qt::AlignHCenter);

    m_heroStage = new QWidget(heroCol);
    m_heroStage->setCursor(Qt::PointingHandCursor);
    m_heroStage->installEventFilter(this);
    heroColLay->addWidget(m_heroStage, 0, Qt::AlignHCenter);

    row->addWidget(heroCol, 0, Qt::AlignVCenter);

    m_heroCoverLbl = new QLabel(m_heroStage);
    m_heroCoverLbl->setScaledContents(true);
    m_heroCoverLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    m_heroCoverCycleTimer = new QTimer(this);
    connect(m_heroCoverCycleTimer, &QTimer::timeout, this, &StackView::advanceHeroCoverCycle);

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

void StackView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!m_backdropLbl) return;
    const int bw = qMin(kBackdropW, width() - 40);
    const int bh = qMin(kBackdropH, height() - 20);
    if (bw <= 0 || bh <= 0) return;
    // Levemente puxado pra cima: compensa o addStretch(3)/addStretch(1) do
    // layout externo, que já concentra o trio herói/texto/pilha mais perto
    // do fundo — o banner cobre melhor o vão vazio de cima assim.
    m_backdropLbl->setGeometry((width() - bw) / 2, qMax(0, (height() - bh) / 2 - 30), bw, bh);
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
        m_heroCoverCycleTimer->stop();
        m_heroStage->hide();
        m_textCol->hide();
        m_heroStatsLbl->hide();
        if (m_backdropLbl) m_backdropLbl->hide();
        return;
    }
    m_heroStage->show();
    m_textCol->show();

    const StackEntry& hero = m_entries[0];

    if (m_backdropLbl) {
        // fullCover (resolução original) em vez de heroCover (já reduzido
        // pro tamanho de exibição do herói) — é a fonte de qualidade real
        // pro banner borrado.
        const QPixmap& backdropSrc = hero.fullCover.isNull() ? hero.heroCover : hero.fullCover;
        const QPixmap backdrop = buildBackdrop(backdropSrc, QSize(kBackdropW, kBackdropH));
        if (animate && !m_backdropLastPixmap.isNull()) {
            crossfadeLabel(m_backdropLbl, m_backdropLastPixmap, backdrop, kCrossfadeMs);
        } else {
            m_backdropLbl->setPixmap(backdrop);
        }
        m_backdropLastPixmap = backdrop;
        m_backdropLbl->show();
    }
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

    // Ciclo automático de capa entre manuscritos — reinicia do zero sempre
    // que o herói muda (relê m_entries[0] fresco a cada tick, então nunca
    // fica apontando pro herói antigo).
    m_heroCoverCycleIdx = 0;
    if (hero.manuscriptHeroCovers.size() > 1) {
        m_heroCoverCycleTimer->start(kHeroCoverCycleMs);
    } else {
        m_heroCoverCycleTimer->stop();
    }

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

    // Legenda discreta acima da capa: uma linha só, sem caixa — a
    // contagem de palavras ganha separador de milhar (só ela costuma ficar
    // grande o bastante pra precisar).
    QStringList stats;
    stats << tr("%1 manuscritos").arg(hero.manuscriptCount);
    stats << tr("%1 capítulos").arg(hero.chapterCount);
    stats << tr("%1 documentos").arg(hero.documentCount);
    if (hero.totalWords >= 0)
        stats << tr("%1 palavras").arg(QLocale().toString(hero.totalWords));
    m_heroStatsLbl->setText(stats.join(QStringLiteral("   ·   ")));
    m_heroStatsLbl->show();

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

void StackView::advanceHeroCoverCycle()
{
    if (m_entries.isEmpty()) { m_heroCoverCycleTimer->stop(); return; }
    const StackEntry& hero = m_entries[0];
    const int n = hero.manuscriptHeroCovers.size();
    if (n < 2) { m_heroCoverCycleTimer->stop(); return; }
    m_heroCoverCycleIdx = (m_heroCoverCycleIdx + 1) % n;
    const QPixmap to = hero.manuscriptHeroCovers[m_heroCoverCycleIdx];
    crossfadeLabel(m_heroCoverLbl, m_heroLastPixmap, to, kCrossfadeMs);
    m_heroLastPixmap = to;
}

void StackView::hideEvent(QHideEvent* event)
{
    m_heroCoverCycleTimer->stop();
    QWidget::hideEvent(event);
}

void StackView::wheelEvent(QWheelEvent* event)
{
    event->accept();

    // Mouse dentro da caixa da sinopse: nunca gira a pilha, mesmo quando a
    // sinopse já está no topo/fim e não tem mais o que rolar (senão o
    // scroll "vaza" pro StackView bem no meio da leitura e troca o livro
    // sem querer).
    if (m_synopsisScroll && m_synopsisScroll->isVisible()) {
        const QPoint posInScroll = m_synopsisScroll->mapFromGlobal(event->globalPosition().toPoint());
        if (m_synopsisScroll->rect().contains(posInScroll)) return;
    }

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
