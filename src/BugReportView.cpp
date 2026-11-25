#include "BugReportView.h"

#include "BugReports.h"
#include "Recados.h"
#include "Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMap>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QSysInfo>
#include <QVBoxLayout>
#include <functional>

namespace {
const char* kMail = "qennawriter@gmail.com";

QColor stageColor(const QString& stage)
{
    if (stage == QLatin1String("done")) return QColor(QStringLiteral("#4cae68"));
    if (stage == QLatin1String("work")) return QColor(QStringLiteral("#d9a520"));
    return QColor(QStringLiteral("#e5534b"));
}

QString dateText(const QDateTime& dt)
{
    if (!dt.isValid()) return {};
    const QString code = QSettings().value(QStringLiteral("app/language")).toString();
    const QLocale loc = code.isEmpty() ? QLocale() : QLocale(code);
    QString s = loc.toString(dt.toLocalTime().date(), QStringLiteral("d MMM"));
    s.remove(QLatin1Char('.'));
    return s;
}

void clearLayout(QLayout* l)
{
    while (QLayoutItem* it = l->takeAt(0)) {
        if (it->widget()) { it->widget()->hide(); it->widget()->deleteLater(); }
        if (it->layout()) clearLayout(it->layout());
        delete it;
    }
}

void repolish(QWidget* w) { w->style()->unpolish(w); w->style()->polish(w); }

// A versão e o sistema vão junto com o relato; o tema, só se for bug visual.
QString envText(bool visual)
{
    QString s = QStringLiteral("Qenna %1 · %2").arg(QCoreApplication::applicationVersion(), QSysInfo::prettyProductName());
    if (visual) s += QStringLiteral(" · ") + QCoreApplication::translate("BugReportView", "tema %1").arg(Theme::Manager::instance()->current().name);
    return s;
}

QPixmap checkGlyph(const QColor& c, int size, qreal dpr)
{
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c, size / 7.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPolyline(QPolygonF({ QPointF(size * 0.2, size * 0.52), QPointF(size * 0.42, size * 0.74), QPointF(size * 0.82, size * 0.28) }));
    return pm;
}
}

BugReportView::BugReportView(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("bugView"));
    build();
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, [this]() { applyTheme(); refreshLists(); });
    BugFeed* feed = BugFeed::instance();
    connect(feed, &BugFeed::changed, this, [this, feed]() {
        refreshLists();
        emit newsChanged(feed->hasNews() && !isVisible());
    });
    feed->fetchMine();   // já na abertura das Configurações: o pontinho no trilho
}

void BugReportView::activate()
{
    BugFeed* feed = BugFeed::instance();
    const bool author = feed->isAuthor();
    if (author != m_author) {
        m_author = author;
        m_userBox->setVisible(!m_author);
        m_authorBox->setVisible(m_author);
    }
    feed->fetchMine();
    feed->fetchPublic();
    if (m_author) feed->fetchAll();
    if (m_tab == 2 || m_author) feed->markSeen();
    emit newsChanged(false);
}

void BugReportView::build()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 12, 0, 0);
    root->setSpacing(0);

    // ── usuário ──
    m_userBox = new QWidget(this);
    auto* uv = new QVBoxLayout(m_userBox);
    uv->setContentsMargins(0, 0, 0, 0);
    uv->setSpacing(16);
    auto* tabs = new QHBoxLayout();
    tabs->setSpacing(0);
    const QString names[3] = { tr("Novo relato"), tr("Todos os relatos"), tr("Meus bugs") };
    for (int i = 0; i < 3; ++i) {
        auto* b = new QPushButton(names[i], m_userBox);
        b->setObjectName(QStringLiteral("settingsSegBtn"));
        b->setProperty("pos", i == 0 ? "first" : i == 2 ? "last" : "mid");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, i]() { showTab(i); });
        tabs->addWidget(b);
        m_tabs << b;
    }
    tabs->addStretch(1);
    uv->addLayout(tabs);
    m_stack = new QStackedWidget(m_userBox);
    m_form = buildForm();
    m_stack->addWidget(m_form);
    auto makeList = [this](QVBoxLayout*& out) {
        auto* w = new QWidget(m_stack);
        auto* v = new QVBoxLayout(w);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);
        out = new QVBoxLayout();
        out->setContentsMargins(0, 0, 0, 0);
        out->setSpacing(12);
        v->addLayout(out);
        v->addStretch(1);
        m_stack->addWidget(w);
    };
    {   // todos os relatos: filtro por estágio em cima da lista
        auto* w = new QWidget(m_stack);
        auto* v = new QVBoxLayout(w);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(14);
        auto* fr = new QHBoxLayout();
        fr->setSpacing(0);
        m_pubFilters = makeFilters(w, fr, &m_pubFilter);
        fr->addStretch(1);
        v->addLayout(fr);
        m_publicList = new QVBoxLayout();
        m_publicList->setContentsMargins(0, 0, 0, 0);
        m_publicList->setSpacing(12);
        v->addLayout(m_publicList);
        v->addStretch(1);
        m_stack->addWidget(w);
    }
    makeList(m_mineList);
    uv->addWidget(m_stack);
    root->addWidget(m_userBox);

    // ── autor ──
    m_authorBox = new QWidget(this);
    auto* av = new QVBoxLayout(m_authorBox);
    av->setContentsMargins(0, 0, 0, 0);
    av->setSpacing(14);
    auto* fl = new QHBoxLayout();
    fl->setSpacing(0);
    m_filters = makeFilters(m_authorBox, fl, &m_filter);
    fl->addStretch(1);
    av->addLayout(fl);
    m_allList = new QVBoxLayout();
    m_allList->setContentsMargins(0, 0, 0, 0);
    m_allList->setSpacing(12);
    av->addLayout(m_allList);
    root->addWidget(m_authorBox);
    root->addStretch(1);

    m_author = BugFeed::instance()->isAuthor();
    m_userBox->setVisible(!m_author);
    m_authorBox->setVisible(m_author);
    showTab(1);   // abre em "Todos os relatos": mostra o serviço e quem chega vê se o bug já foi contado
}

QList<QPushButton*> BugReportView::makeFilters(QWidget* parent, QHBoxLayout* row, QString* target)
{
    QList<QPushButton*> out;
    const QString keys[4] = { QStringLiteral("all"), QStringLiteral("sent"), QStringLiteral("work"), QStringLiteral("done") };
    for (int i = 0; i < 4; ++i) {
        auto* b = new QPushButton(parent);
        b->setObjectName(QStringLiteral("settingsSegBtn"));
        b->setProperty("pos", i == 0 ? "first" : i == 3 ? "last" : "mid");
        b->setProperty("key", keys[i]);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, target, k = keys[i]]() { *target = k; refreshLists(); });
        row->addWidget(b);
        out << b;
    }
    return out;
}

void BugReportView::paintFilters(const QList<QPushButton*>& btns, const QList<BugReport>& list, const QString& current)
{
    int counts[4] = { 0, 0, 0, 0 };
    for (const BugReport& b : list) {
        counts[0]++;
        if (b.stage == QLatin1String("sent")) counts[1]++;
        else if (b.stage == QLatin1String("work")) counts[2]++;
        else counts[3]++;
    }
    const QString fn[4] = { tr("Todos"), tr("Enviados"), tr("Em andamento"), tr("Resolvidos") };
    for (int i = 0; i < btns.size() && i < 4; ++i) {
        btns.at(i)->setText(QStringLiteral("%1 (%2)").arg(fn[i]).arg(counts[i]));
        btns.at(i)->setChecked(btns.at(i)->property("key").toString() == current);
    }
}

void BugReportView::showTab(int tab)
{
    m_tab = tab;
    for (int i = 0; i < m_tabs.size(); ++i) m_tabs.at(i)->setChecked(i == tab);
    m_stack->setCurrentIndex(tab);
    if (tab == 2) { BugFeed::instance()->markSeen(); emit newsChanged(false); }
    refreshLists();
}

QWidget* BugReportView::buildForm()
{
    auto* f = new QWidget(this);
    f->setMaximumWidth(680);
    auto* v = new QVBoxLayout(f);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(14);
    auto field = [f, v](const QString& label, const QString& hint, QWidget* ctl, const QString& below = QString()) {
        auto* box = new QWidget(f);
        auto* bv = new QVBoxLayout(box);
        bv->setContentsMargins(0, 0, 0, 0);
        bv->setSpacing(5);
        auto* l = new QLabel(box);
        l->setObjectName(QStringLiteral("bugLabel"));
        l->setTextFormat(Qt::RichText);
        l->setText(QStringLiteral("<b>%1</b>").arg(label.toHtmlEscaped())
                   + (hint.isEmpty() ? QString() : QStringLiteral("&nbsp;&nbsp;<span style=\"font-weight:400;\">%1</span>").arg(hint.toHtmlEscaped())));
        bv->addWidget(l);
        ctl->setParent(box);
        bv->addWidget(ctl);
        if (!below.isEmpty()) {
            auto* h = new QLabel(below, box);
            h->setObjectName(QStringLiteral("bugHint"));
            h->setWordWrap(true);
            bv->addWidget(h);
        }
        v->addWidget(box);
    };
    auto* title = new QLineEdit();
    title->setObjectName(QStringLiteral("bugEdit"));
    title->setMaxLength(80);
    title->setPlaceholderText(tr("Ex.: o texto some quando troco de tema"));
    auto* body = new QPlainTextEdit();
    body->setObjectName(QStringLiteral("bugEdit"));
    body->setPlaceholderText(tr("O que você esperava que acontecesse, e o que aconteceu."));
    body->setFixedHeight(110);
    body->setTabChangesFocus(true);
    auto* steps = new QPlainTextEdit();
    steps->setObjectName(QStringLiteral("bugEdit"));
    steps->setPlaceholderText(tr("1. Abri um projeto  2. Cliquei em…"));
    steps->setFixedHeight(70);
    steps->setTabChangesFocus(true);
    auto* name = new QLineEdit();
    name->setObjectName(QStringLiteral("bugEdit"));
    name->setMaxLength(40);
    name->setText(QSettings().value(QStringLiteral("recados/nome")).toString());
    auto* pubNote = new QLabel(tr("Os relatos ficam visíveis para todo mundo: assim quem tiver o mesmo problema acha o seu. Não escreva dados pessoais."), f);
    pubNote->setObjectName(QStringLiteral("bugHint"));
    pubNote->setWordWrap(true);
    v->addWidget(pubNote);
    field(tr("Em poucas palavras"), QString(), title);
    auto* similar = new QLabel(f);
    similar->setObjectName(QStringLiteral("bugSimilar"));
    similar->setWordWrap(true);
    similar->setTextFormat(Qt::RichText);
    similar->hide();
    v->addWidget(similar);
    // enquanto digita o título: os relatos que já existem com palavras parecidas
    auto findSimilar = [title, similar]() {
        auto words = [](const QString& s) {
            QSet<QString> out;
            const QString d = s.normalized(QString::NormalizationForm_D).toLower();
            QString plain;
            for (const QChar c : d) if (c.category() != QChar::Mark_NonSpacing) plain += c;
            for (const QString& w : plain.split(QRegularExpression(QStringLiteral("[^a-z0-9]+")), Qt::SkipEmptyParts))
                if (w.size() >= 4) out.insert(w);
            return out;
        };
        const QSet<QString> mineW = words(title->text());
        if (mineW.isEmpty()) { similar->hide(); return; }
        QList<QPair<int, BugReport>> hits;
        for (const BugReport& b : BugFeed::instance()->publicBugs()) {
            const int n = int((words(b.title) + words(b.body)).intersect(mineW).size());
            if (n > 0) hits.append({ n, b });
        }
        std::stable_sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        if (hits.isEmpty()) { similar->hide(); return; }
        QString html = QStringLiteral("<b>%1</b>").arg(QCoreApplication::translate("BugReportView", "Já relatados, parecidos com o seu:").toHtmlEscaped());
        for (int i = 0; i < hits.size() && i < 3; ++i) {
            const BugReport& b = hits.at(i).second;
            const QColor c = stageColor(b.stage);
            const QString st = b.stage == QLatin1String("done") ? QCoreApplication::translate("BugReportView", "resolvido")
                             : b.stage == QLatin1String("work") ? QCoreApplication::translate("BugReportView", "em andamento")
                                                                : QCoreApplication::translate("BugReportView", "enviado");
            html += QStringLiteral("<br><span style=\"color:%1;\">\u25CF</span>&nbsp;%2 <span style=\"color:%1;\">(%3)</span>")
                        .arg(c.name(), b.title.toHtmlEscaped(), st);
        }
        similar->setText(html);
        similar->show();
    };
    connect(title, &QLineEdit::textChanged, f, findSimilar);
    field(tr("O que aconteceu"), QString(), body,
          tr("Caso o bug apresentado ou falha do app tenha gerado algum código de erro, cole aqui também."));
    field(tr("Como repetir"), tr("opcional"), steps);
    field(tr("Seu nome"), tr("o mesmo dos comentários"), name);

    auto* visual = new QCheckBox(tr("É um bug de interface ou visual"), f);
    visual->setObjectName(QStringLiteral("bugCheck"));
    visual->setCursor(Qt::PointingHandCursor);
    v->addWidget(visual);
    auto* autoLbl = new QLabel(f);
    autoLbl->setObjectName(QStringLiteral("bugHint"));
    autoLbl->setWordWrap(true);
    auto paintAuto = [autoLbl, visual]() {
        autoLbl->setText(QCoreApplication::translate("BugReportView", "Vai junto, automático: %1").arg(envText(visual->isChecked())));
    };
    connect(visual, &QCheckBox::toggled, f, paintAuto);
    paintAuto();
    v->addWidget(autoLbl);

    // o protocolo nasce com o formulário: dá pra mandar o print antes de enviar
    auto* protoBox = new QWidget(f);
    auto* pv = new QVBoxLayout(protoBox);
    pv->setContentsMargins(0, 2, 0, 0);
    pv->setSpacing(6);
    auto* pl = new QLabel(QStringLiteral("<b>%1</b>").arg(tr("Protocolo deste relato").toHtmlEscaped()), protoBox);
    pl->setObjectName(QStringLiteral("bugLabel"));
    pv->addWidget(pl);
    auto* prow = new QHBoxLayout();
    prow->setSpacing(6);
    auto* code = new QLabel(protoBox);
    code->setObjectName(QStringLiteral("bugCode"));
    code->setTextInteractionFlags(Qt::TextSelectableByMouse);
    prow->addWidget(code);
    auto* copy = new QPushButton(tr("copiar"), protoBox);
    copy->setObjectName(QStringLiteral("bugLink"));
    copy->setCursor(Qt::PointingHandCursor);
    copy->setFlat(true);
    prow->addWidget(copy);
    prow->addStretch(1);
    pv->addLayout(prow);
    auto* evid = new QLabel(protoBox);
    evid->setObjectName(QStringLiteral("bugHint"));
    evid->setWordWrap(true);
    evid->setTextFormat(Qt::RichText);
    evid->setOpenExternalLinks(true);
    evid->setTextInteractionFlags(Qt::TextBrowserInteraction);
    pv->addWidget(evid);
    v->addWidget(protoBox);
    auto paintCode = [code, evid]() {
        const QString c = BugFeed::instance()->draftCode();
        code->setText(c);
        const QString mail = QString::fromLatin1(kMail);
        evid->setText(QCoreApplication::translate("BugReportView", "Tem print ou vídeo do bug? Mande para %1 com <b>%2</b> no assunto.")
                          .arg(QStringLiteral("<a href=\"mailto:%1?subject=%2\" style=\"color:%3;\">%1</a>")
                                   .arg(mail, c, Theme::toColor(Theme::accentDefault()).name()), c));
    };
    paintCode();
    connect(copy, &QPushButton::clicked, f, [code, copy]() {
        QApplication::clipboard()->setText(code->text());
        copy->setText(QCoreApplication::translate("BugReportView", "copiado"));
    });

    auto* row = new QHBoxLayout();
    row->setSpacing(10);
    auto* status = new QLabel(f);
    status->setObjectName(QStringLiteral("bugHint"));
    status->setWordWrap(true);
    row->addWidget(status, 1);
    auto* send = new QPushButton(tr("Enviar relato"), f);
    send->setObjectName(QStringLiteral("bugSend"));
    send->setCursor(Qt::PointingHandCursor);
    row->addWidget(send);
    v->addLayout(row);
    v->addStretch(1);

    auto setStatus = [status](const QString& t, bool err) {
        status->setText(t);
        status->setProperty("err", err);
        repolish(status);
    };
    connect(send, &QPushButton::clicked, f, [=]() {
        if (title->text().trimmed().isEmpty() || body->toPlainText().trimmed().isEmpty()) {
            setStatus(tr("Escreva o título e o que aconteceu."), true);
            return;
        }
        if (!name->text().trimmed().isEmpty()) QSettings().setValue(QStringLiteral("recados/nome"), name->text().simplified());
        send->setEnabled(false);
        send->setText(tr("Enviando…"));
        setStatus(QString(), false);
        BugFeed::instance()->submit(name->text().simplified(), title->text().simplified(), body->toPlainText().trimmed(),
                                    steps->toPlainText().trimmed(), envText(visual->isChecked()));
    });
    connect(BugFeed::instance(), &BugFeed::submitted, f, [=](const QString& c) {
        send->setEnabled(true);
        send->setText(tr("Enviar relato"));
        title->clear(); body->clear(); steps->clear(); visual->setChecked(false);
        copy->setText(tr("copiar"));
        paintCode();
        setStatus(QString(), false);
        showTab(2);
        Q_UNUSED(c);
    });
    connect(BugFeed::instance(), &BugFeed::failed, f, [=](const QString& why) {
        send->setEnabled(true);
        send->setText(tr("Enviar relato"));
        QString msg;
        if (why == QLatin1String("empty")) msg = tr("Escreva o título e o que aconteceu.");
        else if (why == QLatin1String("too-long")) msg = tr("O relato passou do tamanho máximo.");
        else if (why == QLatin1String("no-links")) msg = tr("Links não são aceitos nos relatos (eles ficam públicos).");
        else if (why == QLatin1String("slow-down")) msg = tr("Muitos relatos seguidos. Tente de novo daqui a pouco.");
        else if (why == QLatin1String("banned")) msg = tr("Você não pode mais enviar relatos.");
        else msg = tr("Não foi possível enviar. Confira a internet e tente de novo.");
        setStatus(msg, true);
    });
    return f;
}

QWidget* BugReportView::pill(const QString& stage, const QString& version, QWidget* parent)
{
    QString text;
    if (stage == QLatin1String("done")) text = version.isEmpty() ? tr("Resolvido") : tr("Resolvido na %1").arg(version);
    else if (stage == QLatin1String("work")) text = tr("Em andamento");
    else text = tr("Enviado");
    auto* l = new QLabel(QStringLiteral("●  ") + text, parent);
    l->setObjectName(QStringLiteral("bugPill"));
    const QColor c = stageColor(stage);
    QColor soft = c;
    soft.setAlphaF(0.15);
    l->setStyleSheet(QStringLiteral("QLabel#bugPill { color: %1; background: %2; border-radius: 9px; padding: 2px 10px 2px 8px; font-size: 11px; font-weight: 600; }")
                         .arg(c.name(), soft.name(QColor::HexArgb)));
    return l;
}

QWidget* BugReportView::bugCard(const BugReport& b, bool author, QWidget* parent, bool showName, bool mine)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("bugCard"));
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(16, 14, 16, 14);
    v->setSpacing(6);
    auto* top = new QHBoxLayout();
    top->setSpacing(10);
    top->addWidget(pill(b.stage, b.version, card));
    auto* t = new QLabel(card);
    t->setObjectName(QStringLiteral("bugTitle"));
    t->setWordWrap(true);
    t->setTextFormat(Qt::RichText);
    t->setText(b.title.toHtmlEscaped() + (mine
        ? QStringLiteral("<span style=\"color:%1; font-size:11px; font-weight:400;\">&nbsp;·&nbsp;%2</span>")
              .arg(Theme::toColor(Theme::accentDefault()).name(), tr("o seu").toHtmlEscaped())
        : QString()));
    top->addWidget(t, 1);
    auto* when = new QLabel(dateText(b.at), card);
    when->setObjectName(QStringLiteral("bugMeta"));
    top->addWidget(when, 0, Qt::AlignTop);
    v->addLayout(top);
    auto* prow = new QHBoxLayout();
    prow->setSpacing(6);
    auto* code = new QLabel(b.code, card);
    code->setObjectName(QStringLiteral("bugCode"));
    code->setTextInteractionFlags(Qt::TextSelectableByMouse);
    prow->addWidget(code);
    auto* copy = new QPushButton(tr("copiar"), card);
    copy->setObjectName(QStringLiteral("bugLink"));
    copy->setCursor(Qt::PointingHandCursor);
    copy->setFlat(true);
    const QString c = b.code;
    connect(copy, &QPushButton::clicked, card, [c, copy]() {
        QApplication::clipboard()->setText(c);
        copy->setText(QCoreApplication::translate("BugReportView", "copiado"));
    });
    prow->addWidget(copy);
    prow->addStretch(1);
    v->addLayout(prow);
    if (!b.body.isEmpty()) {
        auto* body = new QLabel(b.body, card);
        body->setObjectName(QStringLiteral("bugBody"));
        body->setTextFormat(Qt::PlainText);
        body->setWordWrap(true);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->addWidget(body);
    }
    if (!b.steps.isEmpty()) {
        auto* st = new QLabel(tr("Como repetir: %1").arg(b.steps), card);
        st->setObjectName(QStringLiteral("bugMeta"));
        st->setTextFormat(Qt::PlainText);
        st->setWordWrap(true);
        st->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->addWidget(st);
    }
    auto* env = new QLabel((author || showName) ? b.name + QStringLiteral(" · ") + b.env : b.env, card);
    env->setObjectName(QStringLiteral("bugMeta"));
    env->setWordWrap(true);
    v->addWidget(env);

    if (!author && !b.reply.isEmpty()) {
        // igual à resposta de comentário nos Recados: recuada, fio fino, nome + selo + data
        auto* wrap = new QHBoxLayout();
        wrap->setContentsMargins(0, 4, 0, 0);
        wrap->addSpacing(18);
        auto* rep = new QWidget(card);
        rep->setObjectName(QStringLiteral("bugReply"));
        rep->setAttribute(Qt::WA_StyledBackground, true);
        auto* rv = new QVBoxLayout(rep);
        rv->setContentsMargins(12, 0, 0, 0);
        rv->setSpacing(3);
        auto* who = new QHBoxLayout();
        who->setSpacing(8);
        auto* n = new QLabel(QStringLiteral("P.H. Lobato"), rep);
        n->setObjectName(QStringLiteral("bugReplyName"));
        who->addWidget(n);
        auto* badge = new QLabel(tr("AUTOR"), rep);
        badge->setObjectName(QStringLiteral("bugBadge"));
        who->addWidget(badge);
        auto* rw = new QLabel(dateText(b.replyAt), rep);
        rw->setObjectName(QStringLiteral("bugReplyWhen"));
        who->addWidget(rw);
        who->addStretch(1);
        rv->addLayout(who);
        auto* rt = new QLabel(b.reply, rep);
        rt->setObjectName(QStringLiteral("bugReplyText"));
        rt->setTextFormat(Qt::PlainText);
        rt->setWordWrap(true);
        rt->setTextInteractionFlags(Qt::TextSelectableByMouse);
        rv->addWidget(rt);
        wrap->addWidget(rep, 1);
        v->addLayout(wrap);
    }

    if (author) {
        // estágio, resposta e versão
        auto* st = new QHBoxLayout();
        st->setSpacing(0);
        const QString keys[3] = { QStringLiteral("sent"), QStringLiteral("work"), QStringLiteral("done") };
        const QString labels[3] = { tr("Enviado"), tr("Em andamento"), tr("Resolvido") };
        auto chosen = std::make_shared<QString>(b.stage);
        QList<QPushButton*> stBtns;
        for (int i = 0; i < 3; ++i) {
            auto* sb = new QPushButton(labels[i], card);
            sb->setObjectName(QStringLiteral("settingsSegBtn"));
            sb->setProperty("pos", i == 0 ? "first" : i == 2 ? "last" : "mid");
            sb->setCheckable(true);
            sb->setChecked(b.stage == keys[i]);
            sb->setCursor(Qt::PointingHandCursor);
            st->addWidget(sb);
            stBtns << sb;
        }
        st->addStretch(1);
        v->addSpacing(4);
        v->addLayout(st);
        auto* reply = new QPlainTextEdit(card);
        reply->setObjectName(QStringLiteral("bugEdit"));
        reply->setPlaceholderText(b.reply.isEmpty() ? tr("Responder (sai com o selo AUTOR)") : tr("Atualizar a resposta"));
        reply->setPlainText(b.reply);
        reply->setFixedHeight(64);
        reply->setTabChangesFocus(true);
        v->addWidget(reply);
        auto* row = new QHBoxLayout();
        row->setSpacing(8);
        auto* verLbl = new QLabel(tr("Resolvido na versão"), card);
        verLbl->setObjectName(QStringLiteral("bugMeta"));
        auto* ver = new QLineEdit(card);
        ver->setObjectName(QStringLiteral("bugEdit"));
        ver->setFixedWidth(80);
        ver->setText(b.version.isEmpty() ? QCoreApplication::applicationVersion().section(QLatin1Char('.'), 0, 1) : b.version);
        row->addWidget(verLbl);
        row->addWidget(ver);
        row->addStretch(1);
        auto* del = new QPushButton(tr("apagar"), card);
        del->setObjectName(QStringLiteral("bugLink"));
        del->setCursor(Qt::PointingHandCursor);
        del->setFlat(true);
        row->addWidget(del);
        auto* save = new QPushButton(tr("Salvar"), card);
        save->setObjectName(QStringLiteral("bugSend"));
        save->setCursor(Qt::PointingHandCursor);
        row->addWidget(save);
        v->addLayout(row);
        auto syncVer = [verLbl, ver, chosen, stBtns, keys]() {
            const bool done = *chosen == QLatin1String("done");
            verLbl->setVisible(done);
            ver->setVisible(done);
            for (int j = 0; j < 3; ++j) {
                if (keys[j] != *chosen) { stBtns.at(j)->setStyleSheet(QString()); continue; }
                const QColor sc = stageColor(keys[j]);
                QColor soft = sc;
                soft.setAlphaF(0.16);
                stBtns.at(j)->setStyleSheet(QStringLiteral("QPushButton#settingsSegBtn:checked { color: %1; background: %2; border-color: %1; }")
                                                .arg(sc.name(), soft.name(QColor::HexArgb)));
            }
        };
        syncVer();
        for (int i = 0; i < 3; ++i) {
            connect(stBtns.at(i), &QPushButton::clicked, card, [stBtns, i, keys, chosen, syncVer]() {
                *chosen = keys[i];
                for (int j = 0; j < 3; ++j) stBtns.at(j)->setChecked(j == i);
                syncVer();
            });
        }
        const qint64 id = b.id;
        connect(save, &QPushButton::clicked, card, [id, chosen, reply, ver, save]() {
            QString stage = *chosen;
            // respondeu um relato ainda vermelho: já vira "em andamento"
            if (stage == QLatin1String("sent") && !reply->toPlainText().trimmed().isEmpty()) stage = QStringLiteral("work");
            save->setEnabled(false);
            BugFeed::instance()->update(id, stage, reply->toPlainText().trimmed(), stage == QLatin1String("done") ? ver->text().trimmed() : QString());
        });
        connect(del, &QPushButton::clicked, card, [id]() { BugFeed::instance()->remove(id); });
    }
    return card;
}

void BugReportView::refreshLists()
{
    BugFeed* feed = BugFeed::instance();
    const qreal dpr = devicePixelRatioF();
    // contagem nas abas
    if (m_tabs.size() == 3) {
        m_tabs.at(1)->setText(feed->publicBugs().isEmpty() ? tr("Todos os relatos") : tr("Todos os relatos (%1)").arg(feed->publicBugs().size()));
        m_tabs.at(2)->setText(feed->mine().isEmpty() ? tr("Meus bugs") : tr("Meus bugs (%1)").arg(feed->mine().size()));
    }
    // meus bugs
    clearLayout(m_mineList);
    if (feed->mine().isEmpty()) {
        auto* e = new QLabel(feed->mineLoaded() ? tr("Nenhum relato ainda. Quando você enviar um, ele aparece aqui com o estágio.")
                                                : tr("Carregando…"), this);
        e->setObjectName(QStringLiteral("bugHint"));
        e->setWordWrap(true);
        m_mineList->addWidget(e);
    }
    for (const BugReport& b : feed->mine()) m_mineList->addWidget(bugCard(b, false, this));

    // todos os relatos (público): abertos primeiro, depois os resolvidos, mais novos em cima
    clearLayout(m_publicList);
    QSet<qint64> mineIds;
    for (const BugReport& b : feed->mine()) mineIds.insert(b.id);
    QList<BugReport> pub = feed->publicBugs();
    paintFilters(m_pubFilters, pub, m_pubFilter);
    auto rankP = [](const QString& s) { return s == QLatin1String("done") ? 1 : 0; };
    std::stable_sort(pub.begin(), pub.end(), [&](const BugReport& a, const BugReport& b) { return rankP(a.stage) < rankP(b.stage); });
    int shownP = 0;
    for (const BugReport& b : pub) {
        if (m_pubFilter != QLatin1String("all") && b.stage != m_pubFilter) continue;
        m_publicList->addWidget(bugCard(b, false, this, true, mineIds.contains(b.id)));
        ++shownP;
    }
    if (!shownP) {
        auto* e = new QLabel(feed->publicLoaded() ? (pub.isEmpty() ? tr("Nenhum relato ainda. Se achar um bug, conte em \"Novo relato\".") : tr("Nenhum relato aqui."))
                                                  : tr("Carregando…"), this);
        e->setObjectName(QStringLiteral("bugHint"));
        e->setWordWrap(true);
        m_publicList->addWidget(e);
    }
    Q_UNUSED(dpr);

    // central do autor
    clearLayout(m_allList);
    if (m_author) {
        paintFilters(m_filters, feed->all(), m_filter);
        QList<BugReport> list = feed->all();
        auto rank = [](const QString& s) { return s == QLatin1String("sent") ? 0 : s == QLatin1String("work") ? 1 : 2; };
        std::stable_sort(list.begin(), list.end(), [&](const BugReport& a, const BugReport& b) { return rank(a.stage) < rank(b.stage); });
        int shown = 0;
        for (const BugReport& b : list) {
            if (m_filter != QLatin1String("all") && b.stage != m_filter) continue;
            m_allList->addWidget(bugCard(b, true, this));
            ++shown;
        }
        if (!shown) {
            auto* e = new QLabel(feed->allLoaded() ? tr("Nenhum relato aqui.") : tr("Carregando…"), this);
            e->setObjectName(QStringLiteral("bugHint"));
            m_allList->addWidget(e);
        }
    }
}

void BugReportView::applyTheme()
{
    const QColor accent = Theme::toColor(Theme::accentDefault());
    const QColor ink = accent.lightnessF() > 0.62 ? QColor(QStringLiteral("#15171c")) : QColor(QStringLiteral("#f7f8fa"));
    const QColor panel = Theme::toColor(Theme::panelBackground());
    const QColor text = Theme::toColor(Theme::textPrimary());
    const QColor card = QColor::fromRgbF(panel.redF() + (text.redF() - panel.redF()) * 0.035,
                                         panel.greenF() + (text.greenF() - panel.greenF()) * 0.035,
                                         panel.blueF() + (text.blueF() - panel.blueF()) * 0.035);
    setStyleSheet(Theme::qss(QStringLiteral(R"(
        QLabel#bugLabel { color: %3; font-size: 12.5px; background: transparent; }
        QLabel#bugHint { color: %4; font-size: 12px; background: transparent; }
        QLabel#bugHint[err="true"] { color: #e5534b; }
        QLabel#bugHint a { color: %5; }
        QLineEdit#bugEdit, QPlainTextEdit#bugEdit { background: %1; color: %2; border: 1px solid %6;
                                   border-radius: @radius-control; padding: 6px 8px; font-size: 13px; }
        QLineEdit#bugEdit:focus, QPlainTextEdit#bugEdit:focus { border-color: %5; }
        QCheckBox#bugCheck { color: %2; font-size: 13px; background: transparent; }
        QLabel#bugCode { color: %3; background: %1; border: 1px solid %6; border-radius: 5px; padding: 2px 8px;
                         font-family: 'IBM Plex Mono', Consolas, monospace; font-size: 12px; font-weight: 600; }
        QPushButton#bugLink { background: transparent; border: none; color: %4; font-size: 11.5px; padding: 0px 4px;
                              text-decoration: underline; }
        QPushButton#bugLink:hover { color: %5; }
        QPushButton#bugSend { background: %5; color: %7; border: 1px solid %5; border-radius: @radius-control;
                              padding: 7px 18px; font-size: 12.5px; font-weight: 600; }
        QPushButton#bugSend:hover { border-color: %3; }
        QPushButton#bugSend:disabled { background: transparent; color: %4; border-color: %6; }
        QFrame#bugCard { background: %8; border: 1px solid %6; border-radius: 7px; }
        QLabel#bugTitle { color: %3; font-size: 13.5px; font-weight: 600; background: transparent; }
        QLabel#bugBody { color: %2; font-size: 13px; background: transparent; }
        QLabel#bugMeta { color: %4; font-size: 11.5px; background: transparent; }
        QWidget#bugReply { border-left: 2px solid %6; background: transparent; }
        QLabel#bugReplyName { color: %5; font-family: 'Crimson Pro', Georgia, serif; font-size: 15px; font-weight: 700; background: transparent; }
        QLabel#bugBadge { background: %5; color: %7; border-radius: 8px; padding: 1px 8px; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
        QLabel#bugReplyWhen { color: %4; font-family: 'Crimson Pro', Georgia, serif; font-size: 12px; background: transparent; }
        QLabel#bugReplyText { color: %2; font-family: 'Crimson Pro', Georgia, serif; font-size: 16px; background: transparent; }
        QLabel#bugSimilar { color: %2; font-size: 12px; background: %8; border: 1px solid %6; border-radius: 5px; padding: 8px 10px; }
        QLabel#bugVer { color: %4; font-size: 11px; font-weight: 600; letter-spacing: 1px; background: transparent; padding-top: 2px; }
    )")).arg(Theme::panelBackground(), Theme::textPrimary(), Theme::textBright(), Theme::textMuted(),
             accent.name(), Theme::panelBorder(), ink.name(), card.name()));
}
