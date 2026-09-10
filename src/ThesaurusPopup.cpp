#include "ThesaurusPopup.h"

#include "Theme.h"
#include "Thesaurus.h"
#include "ThesaurusRanker.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

ThesaurusPopup::ThesaurusPopup(Thesaurus* thesaurus, QWidget* parent)
    : QWidget(parent, Qt::Popup), m_th(thesaurus) {
    setObjectName(QStringLiteral("thesaurusPopup"));
    setFocusPolicy(Qt::StrongFocus);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(18, 16, 14, 14);
    lay->setSpacing(12);

    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("thTitle"));
    m_title->setWordWrap(true);
    lay->addWidget(m_title);

    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName(QStringLiteral("thScroll"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // QAbstractScrollArea não propaga o background do QSS pro viewport — sem
    // isto a lista sai com o cinza padrão do sistema e vira caixa dentro da
    // caixa (já mordeu StatsPanel e AIChatPanel antes).
    m_scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));

    m_body = new QWidget(m_scroll);
    m_bodyLay = new QVBoxLayout(m_body);
    m_bodyLay->setContentsMargins(0, 0, 6, 0);
    m_bodyLay->setSpacing(2); // o respiro entre acepções vem da margem do bloco
    m_scroll->setWidget(m_body);
    lay->addWidget(m_scroll, 1);

    m_downloadBtn = new QPushButton(this);
    m_downloadBtn->setObjectName(QStringLiteral("thBtn"));
    m_downloadBtn->setCursor(Qt::PointingHandCursor);
    m_downloadBtn->setVisible(false);
    connect(m_downloadBtn, &QPushButton::clicked, this, [this]() {
        if (!m_th) return;
        m_downloadBtn->setEnabled(false);
        m_downloadBtn->setText(tr("Baixando…"));
        if (m_progress) m_progress->setVisible(true);
        m_th->download();
    });
    lay->addWidget(m_downloadBtn);

    m_progress = new QProgressBar(this);
    m_progress->setObjectName(QStringLiteral("thProgress"));
    m_progress->setTextVisible(false);
    m_progress->setVisible(false);
    m_progress->setFixedHeight(6);
    lay->addWidget(m_progress);

    if (m_th) {
        connect(m_th, &Thesaurus::downloadProgress, this, [this](qint64 got, qint64 total) {
            if (!m_progress) return;
            m_progress->setMaximum(total > 0 ? int(total / 1024) : 0);
            m_progress->setValue(int(got / 1024));
        });
        connect(m_th, &Thesaurus::downloadFinished, this, [this](bool ok, const QString& err) {
            if (m_progress) m_progress->setVisible(false);
            if (m_downloadBtn) {
                m_downloadBtn->setEnabled(true);
                m_downloadBtn->setVisible(false);
            }
            if (!ok) {
                clearBody();
                auto* l = new QLabel(tr("Não foi possível baixar: %1").arg(err), m_body);
                l->setObjectName(QStringLiteral("thHint"));
                l->setWordWrap(true);
                m_bodyLay->addWidget(l);
                m_bodyLay->addStretch();
                if (m_downloadBtn) m_downloadBtn->setVisible(true);
                return;
            }
            rebuild();
        });
    }

    applyTheme();
}

void ThesaurusPopup::clearBody()
{
    while (m_bodyLay->count() > 0) {
        QLayoutItem* it = m_bodyLay->takeAt(0);
        if (QWidget* w = it->widget()) w->deleteLater();
        delete it;
    }
}

void ThesaurusPopup::showDownloadOffer()
{
    clearBody();
    auto* l = new QLabel(
        tr("O dicionário de sinônimos deste idioma ainda não foi baixado. "
           "Ele fica guardado no seu computador e funciona sem internet depois disso."),
        m_body);
    l->setObjectName(QStringLiteral("thHint"));
    l->setWordWrap(true);
    m_bodyLay->addWidget(l);
    m_bodyLay->addStretch();

    if (m_downloadBtn) {
        m_downloadBtn->setText(tr("Baixar dicionário de sinônimos"));
        m_downloadBtn->setVisible(true);
        m_downloadBtn->setEnabled(true);
    }
}

void ThesaurusPopup::rebuild()
{
    clearBody();
    if (m_downloadBtn) m_downloadBtn->setVisible(false);

    if (!m_th) return;

    if (!m_th->hasData()) { showDownloadOffer(); return; }
    if (!m_th->isReady()) m_th->buildIndex();

    // Consulta pelo lema quando a palavra do texto está flexionada — o
    // dicionário é indexado por forma de dicionário.
    const QString lookupWord = m_lemma.isEmpty() ? m_word : m_lemma;
    const QList<Thesaurus::Sense> raw = m_th->lookup(lookupWord);
    // Ordena pelo que combina com o texto em volta, usando o manuscrito como
    // referência — sem isso "casa" abre em "armazém" e "botoeira".
    const QList<ThesaurusRanker::RankedSense> ranked =
        ThesaurusRanker::rank(raw, lookupWord, m_context, m_corpus);

    QList<Thesaurus::Sense> senses;
    bool anyGrounded = false;
    for (const ThesaurusRanker::RankedSense& r : ranked) {
        senses.append(r.sense);
        if (r.grounded) anyGrounded = true;
    }

    if (senses.isEmpty()) {
        auto* l = new QLabel(
            tr("Nenhum sinônimo encontrado para \"%1\". O dicionário não cobre "
               "todas as palavras.").arg(m_word), m_body);
        l->setObjectName(QStringLiteral("thHint"));
        l->setWordWrap(true);
        m_bodyLay->addWidget(l);
        m_bodyLay->addStretch();
        return;
    }

    // QSS não estiliza <a> dentro de QLabel — cor e sublinhado do link têm que
    // ir no HTML, senão sai o azul padrão do sistema.
    const QString linkStyle =
        QStringLiteral("color:%1;text-decoration:none;").arg(Theme::textPrimary());
    const QString headStyle =
        QStringLiteral("color:%1;text-decoration:none;font-weight:bold;font-size:13px;")
            .arg(Theme::accentInfo());
    const QString sep =
        QStringLiteral("<span style=\"color:%1\"> &middot; </span>").arg(Theme::textMuted());

    auto makeClickable = [this](const QString& html) {
        auto* l = new QLabel(html, m_body);
        l->setObjectName(QStringLiteral("thSyns"));
        l->setWordWrap(true);
        l->setTextFormat(Qt::RichText);
        l->setTextInteractionFlags(Qt::TextBrowserInteraction);
        connect(l, &QLabel::linkActivated, this, [this](const QString& word) {
            emit replaceRequested(word);
            close();
        });
        return l;
    };

    // Com evidência no texto, as primeiras acepções são as prováveis e o resto
    // é ruído — mostrar 21 sentidos de uma vez é o que tornava isso inútil. Sem
    // evidência nenhuma, não há por que esconder: a ordem seria arbitrária.
    const int kInitialLimit = 3;
    const bool limitList = anyGrounded && !m_showAll;

    int shown = 0, hidden = 0;
    for (const Thesaurus::Sense& s : senses) {
        // Quando NÃO há definição, o primeiro sinônimo é a palavra que nomeia a
        // acepção: vira cabeçalho e sai da lista, senão apareceria duas vezes
        // seguidas. Com definição, ela é o cabeçalho e nenhum sinônimo se perde.
        const bool headIsDef = !s.label.isEmpty()
            && s.label.contains(QLatin1Char(' ')) && s.label.size() > 24;
        QStringList rest = s.synonyms;
        if (!headIsDef && !rest.isEmpty()) rest.removeFirst();

        // Sentido de UMA palavra só ("punhal → adaga") é pouco, mas é o único
        // que existe para boa parte do vocabulário — descartá-lo fazia o verbete
        // inteiro sumir do popup.
        if (rest.isEmpty() && !s.synonyms.isEmpty() && !headIsDef)
            rest = s.synonyms;
        if (rest.isEmpty()) continue;

        if (limitList && shown >= kInitialLimit) { ++hidden; continue; }

        // Cada acepção é um bloco com régua embaixo: sem separação, 21 sentidos
        // viram um paredão em que não se acha nada.
        auto* card = new QWidget(m_body);
        card->setObjectName(QStringLiteral("thSense"));
        auto* cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(0, 0, 0, 12);
        cardLay->setSpacing(5);

        const QString head = s.label.isEmpty() ? s.category : s.label;
        const bool headIsDefinition = headIsDef;
        // Sentido de uma palavra só: o cabeçalho seria idêntico ao único
        // sinônimo listado, então some.
        const bool skipHead = !headIsDefinition && rest.size() == s.synonyms.size();
        QLabel* headLabel = headIsDefinition
            ? new QLabel(QStringLiteral("<span style=\"%2\">%1</span>")
                             .arg(head.toHtmlEscaped(), headStyle), m_body)
            : makeClickable(QStringLiteral("<a href=\"%1\" style=\"%2\">%1</a>")
                             .arg(head.toHtmlEscaped(), headStyle));
        headLabel->setObjectName(QStringLiteral("thSenseHead"));
        headLabel->setWordWrap(true);
        if (skipHead) headLabel->hide();
        cardLay->addWidget(headLabel);

        QStringList links;
        for (const QString& syn : rest) {
            // O texto exibido (e inserido) acompanha a flexão da palavra
            // original: em "apertou para aumentar", a sugestão é "pressionou",
            // não "pressionar".
            const QString shown = m_inflect ? m_inflect(syn) : syn;
            links << QStringLiteral("<a href=\"%1\" style=\"%2\">%1</a>")
                         .arg(shown.toHtmlEscaped(), linkStyle);
        }

        // line-height solta as linhas: com o wrap padrão elas encostam umas nas
        // outras e o bloco vira um borrão.
        auto* body = makeClickable(
            QStringLiteral("<div style=\"line-height:180%;\">%1</div>")
                .arg(links.join(sep)));
        cardLay->addWidget(body);

        m_bodyLay->addWidget(card);
        ++shown;
    }
    m_senseCount = shown;
    m_hiddenCount = hidden;

    if (hidden > 0) {
        auto* more = new QPushButton(tr("Mostrar outros %n sentido(s)", "", hidden), m_body);
        more->setObjectName(QStringLiteral("thMoreBtn"));
        more->setCursor(Qt::PointingHandCursor);
        more->setFlat(true);
        connect(more, &QPushButton::clicked, this, [this]() {
            m_showAll = true;
            rebuild();
        });
        m_bodyLay->addWidget(more);
    }

    if (shown == 0) {
        auto* l = new QLabel(
            tr("Nenhum sinônimo útil encontrado para \"%1\".").arg(m_word), m_body);
        l->setObjectName(QStringLiteral("thHint"));
        l->setWordWrap(true);
        m_bodyLay->addWidget(l);
    }
    m_bodyLay->addStretch();
}

void ThesaurusPopup::presentAt(const QPoint& globalPos, const QString& word,
                               const QString& context, const QString& corpus)
{
    m_word = word.trimmed();
    m_context = context;
    m_corpus = corpus;
    m_showAll = false; // cada consulta começa recolhida
    rebuild();

    // Título depois do rebuild: a contagem de sentidos só existe depois da
    // consulta, e ela é a informação que orienta quem vai rolar a lista.
    if (m_title) {
        const int n = m_senseCount;
        m_title->setText(n > 0
            ? tr("<b>%1</b> &nbsp;·&nbsp; %n sentido(s)", "", n).arg(m_word.toHtmlEscaped())
            : tr("<b>%1</b>").arg(m_word.toHtmlEscaped()));
    }

    resize(440, 420);
    // Não deixa vazar da tela: aberto perto da borda direita/inferior, o popup
    // sairia de vista.
    QRect area = QGuiApplication::primaryScreen()
                     ? QGuiApplication::primaryScreen()->availableGeometry()
                     : QRect(0, 0, 1920, 1080);
    if (QScreen* sc = QGuiApplication::screenAt(globalPos)) area = sc->availableGeometry();

    QPoint p = globalPos;
    if (p.x() + width() > area.right()) p.setX(area.right() - width() - 8);
    if (p.y() + height() > area.bottom()) p.setY(globalPos.y() - height() - 4);
    if (p.x() < area.left()) p.setX(area.left() + 8);
    if (p.y() < area.top()) p.setY(area.top() + 8);

    move(p);
    show();
    raise();
    setFocus();
}

void ThesaurusPopup::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) { close(); return; }
    QWidget::keyPressEvent(event);
}

void ThesaurusPopup::applyTheme()
{
    setStyleSheet(QStringLiteral(R"(
        QWidget#thesaurusPopup {
            background: %1;
            border: 1px solid %5;
            border-radius: 8px;
        }
        /* A área de rolagem tem que desaparecer: com fundo e borda próprios ela
           vira uma caixa dentro da caixa. */
        QScrollArea#thScroll { background: transparent; border: none; }
        QLabel#thTitle {
            color: %4; font-size: 14px;
            padding-bottom: 8px;
            border-bottom: 1px solid %5;
        }
        /* Régua fina embaixo de cada acepção — separa sem pesar. */
        QWidget#thSense { border-bottom: 1px solid %3; }
        QLabel#thSyns { color: %2; font-size: 12px; }
        QLabel#thHint { color: %7; font-size: 12px; }
        QPushButton#thBtn {
            background: %8; color: %2;
            border: 1px solid %5; border-radius: 5px;
            padding: 8px 14px; min-height: 28px;
        }
        QPushButton#thBtn:hover { background: %9; color: %4; }
        QPushButton#thMoreBtn {
            background: transparent; color: %7;
            border: none; text-align: left;
            padding: 6px 0 0 0; font-size: 11px;
        }
        QPushButton#thMoreBtn:hover { color: %6; }
        QProgressBar#thProgress {
            background: %3; border: none; border-radius: 3px;
        }
        QProgressBar#thProgress::chunk { background: %6; border-radius: 3px; }
        /* Barra de rolagem discreta: a padrão do sistema destoa do resto. */
        QScrollBar:vertical {
            background: transparent; width: 8px; margin: 0;
        }
        QScrollBar::handle:vertical {
            background: %5; border-radius: 4px; min-height: 30px;
        }
        QScrollBar::handle:vertical:hover { background: %6; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
    )").arg(Theme::panelBackground(),   // 1
           Theme::textPrimary(),        // 2
           Theme::inputBackground(),    // 3
           Theme::textBright(),         // 4
           Theme::panelBorder(),        // 5
           Theme::accentInfo(),         // 6
           Theme::textMuted(),          // 7
           Theme::hoverOverlay(),       // 8
           Theme::hoverStrong()         // 9
        ));
}
