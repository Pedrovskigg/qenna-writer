#include "RenameDialog.h"

#include "Theme.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

RenameDialog::RenameDialog(const RenameService::Plan& plan, QWidget* parent)
    : QDialog(parent), m_plan(plan) {
    setObjectName(QStringLiteral("renameDialog"));
    setWindowTitle(tr("Renomear no projeto"));
    setModal(true);
    collectGroups();
    buildUi();
}

void RenameDialog::collectGroups() {
    QHash<QString, int> indexByKey;
    for (int i = 0; i < m_plan.occurrences.size(); ++i) {
        const RenameService::Occurrence& o = m_plan.occurrences.at(i);
        if (o.certainty != RenameService::Certainty::Loose) continue;

        const QString key = o.where + QLatin1Char('\n') + o.matched;
        auto it = indexByKey.constFind(key);
        if (it != indexByKey.constEnd()) {
            m_groups[it.value()].indexes.append(i);
            continue;
        }
        Group g;
        g.where = o.where;
        g.matched = o.matched;
        g.replacement = o.replacement;
        g.sample = o.snippet;
        g.fromAlias = o.fromAlias;
        g.selected = o.selected;
        g.indexes.append(i);
        indexByKey.insert(key, m_groups.size());
        m_groups.append(g);
    }
    m_checks = QList<QCheckBox*>();
    for (int i = 0; i < m_groups.size(); ++i) m_checks.append(nullptr);
}

void RenameDialog::addGroupRows(QVBoxLayout* into, bool aliasSection) {
    for (int gi = 0; gi < m_groups.size(); ++gi) {
        Group& g = m_groups[gi];
        if (g.fromAlias != aliasSection) continue;

        auto* row = new QWidget(into->parentWidget());
        auto* rowLay = new QVBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->setSpacing(1);

        const int n = g.indexes.size();
        const QString label = (n > 1)
            ? tr("%1  ·  %2 → %3  (%4 vezes)").arg(g.where, g.matched, g.replacement).arg(n)
            : tr("%1  ·  %2 → %3").arg(g.where, g.matched, g.replacement);

        auto* check = new QCheckBox(label, row);
        check->setObjectName(QStringLiteral("rnCheck"));
        check->setChecked(g.selected);
        rowLay->addWidget(check);

        auto* snip = new QLabel(g.sample.toHtmlEscaped(), row);
        snip->setObjectName(QStringLiteral("rnSnippet"));
        snip->setWordWrap(true);
        snip->setContentsMargins(20, 0, 0, 0);
        rowLay->addWidget(snip);

        into->addWidget(row);
        m_checks[gi] = check;
    }
}

void RenameDialog::buildUi() {
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(18, 16, 18, 14);
    lay->setSpacing(9);

    auto* header = new QLabel(
        tr("<b>%1</b> passa a se chamar <b>%2</b>")
            .arg(m_plan.oldName.toHtmlEscaped(), m_plan.newName.toHtmlEscaped()), this);
    header->setObjectName(QStringLiteral("rnTitle"));
    header->setWordWrap(true);
    lay->addWidget(header);

    const int linked = m_plan.countOf(RenameService::Certainty::Linked);
    if (linked > 0) {
        auto* box = new QFrame(this);
        box->setObjectName(QStringLiteral("rnLinkedBox"));
        auto* boxLay = new QVBoxLayout(box);
        boxLay->setContentsMargins(12, 9, 12, 9);
        boxLay->setSpacing(2);

        auto* t = new QLabel(tr("%n menção(ões) com @ — trocadas automaticamente", "", linked), box);
        t->setObjectName(QStringLiteral("rnLinkedTitle"));
        boxLay->addWidget(t);

        auto* sub = new QLabel(
            tr("A menção guarda o vínculo com a ficha, então não há dúvida de que é "
               "a pessoa certa."), box);
        sub->setObjectName(QStringLiteral("rnHint"));
        sub->setWordWrap(true);
        boxLay->addWidget(sub);
        lay->addWidget(box);
    }

    int nameGroups = 0, aliasGroups = 0;
    for (const Group& g : m_groups) (g.fromAlias ? aliasGroups : nameGroups)++;

    if (nameGroups + aliasGroups > 0) {
        auto* scroll = new QScrollArea(this);
        scroll->setObjectName(QStringLiteral("rnScroll"));
        scroll->setWidgetResizable(true);
        scroll->setMinimumHeight(240);
        // QAbstractScrollArea não propaga background pro viewport pelo QSS do
        // pai — sem isto o fundo da lista sai com a cor padrão do sistema.
        scroll->viewport()->setStyleSheet(
            QStringLiteral("background: %1;").arg(Theme::inputBackground()));

        auto* inner = new QWidget(scroll);
        auto* innerLay = new QVBoxLayout(inner);
        innerLay->setContentsMargins(10, 8, 10, 8);
        innerLay->setSpacing(8);

        if (nameGroups > 0) {
            auto* t = new QLabel(tr("Escrito no texto"), inner);
            t->setObjectName(QStringLiteral("rnSectionTitle"));
            innerLay->addWidget(t);
            addGroupRows(innerLay, /*aliasSection=*/false);
        }

        if (aliasGroups > 0) {
            auto* t = new QLabel(tr("Apelidos e nomes antigos"), inner);
            t->setObjectName(QStringLiteral("rnSectionTitle"));
            innerLay->addWidget(t);

            auto* hint = new QLabel(
                tr("Desmarcados de propósito. Um apelido de verdade (\"Capitã\") deve "
                   "continuar como está — mas um nome antigo que virou apelido numa "
                   "renomeação anterior provavelmente precisa mudar junto. Só você sabe "
                   "qual é qual."), inner);
            hint->setObjectName(QStringLiteral("rnHint"));
            hint->setWordWrap(true);
            innerLay->addWidget(hint);

            addGroupRows(innerLay, /*aliasSection=*/true);
        }

        innerLay->addStretch();
        scroll->setWidget(inner);
        lay->addWidget(scroll, 1);

        auto* bulkRow = new QHBoxLayout();
        auto* allBtn = new QPushButton(tr("Marcar todas"), this);
        auto* noneBtn = new QPushButton(tr("Desmarcar todas"), this);
        allBtn->setObjectName(QStringLiteral("rnBtn"));
        noneBtn->setObjectName(QStringLiteral("rnBtn"));
        connect(allBtn, &QPushButton::clicked, this, [this]() {
            for (QCheckBox* c : m_checks) if (c) c->setChecked(true);
        });
        connect(noneBtn, &QPushButton::clicked, this, [this]() {
            for (QCheckBox* c : m_checks) if (c) c->setChecked(false);
        });
        bulkRow->addWidget(allBtn);
        bulkRow->addWidget(noneBtn);
        bulkRow->addStretch();
        lay->addLayout(bulkRow);
    }

    m_aliasKeepCheck = new QCheckBox(
        tr("Manter \"%1\" como apelido").arg(m_plan.oldName), this);
    m_aliasKeepCheck->setObjectName(QStringLiteral("rnCheck"));
    m_aliasKeepCheck->setChecked(m_plan.keepOldAsAlias);
    lay->addWidget(m_aliasKeepCheck);

    auto* aliasHint = new QLabel(
        tr("A detecção de personagens e de diálogos reconhece pelo nome: sem o apelido, "
           "tudo que já foi encontrado com o nome antigo deixa de ser reconhecido."), this);
    aliasHint->setObjectName(QStringLiteral("rnHint"));
    aliasHint->setWordWrap(true);
    aliasHint->setContentsMargins(20, 0, 0, 0);
    lay->addWidget(aliasHint);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* cancelBtn = new QPushButton(tr("Cancelar"), this);
    cancelBtn->setObjectName(QStringLiteral("rnBtn"));
    auto* applyBtn = new QPushButton(tr("Renomear"), this);
    applyBtn->setObjectName(QStringLiteral("rnBtn"));
    applyBtn->setDefault(true);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(applyBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(applyBtn);
    lay->addLayout(btnRow);

    resize(620, m_groups.isEmpty() ? 320 : 600);

    setStyleSheet(QStringLiteral(R"(
        QDialog#renameDialog { background: %1; }
        QDialog#renameDialog QLabel { color: %2; font-size: 12px; }
        QLabel#rnTitle { font-size: 14px; color: %4; }
        QLabel#rnSectionTitle { font-size: 12px; color: %4; font-weight: bold; margin-top: 6px; }
        QLabel#rnHint { color: %7; font-size: 11px; }
        QLabel#rnSnippet { color: %7; font-size: 11px; font-style: italic; }
        QFrame#rnLinkedBox { background: %3; border: 1px solid %10; border-radius: 4px; }
        QLabel#rnLinkedTitle { color: %4; font-weight: bold; }
        QScrollArea#rnScroll { background: %3; border: 1px solid %5; border-radius: 4px; }
        QCheckBox#rnCheck { color: %2; font-size: 12px; spacing: 6px; }
        QCheckBox#rnCheck::indicator {
            width: 14px; height: 14px; border: 1px solid %5;
            border-radius: 3px; background: %3;
        }
        /* Marcado precisa ser inconfundível: com a variante suave o estado
           ligado ficava idêntico ao desligado no tema escuro. */
        QCheckBox#rnCheck::indicator:checked { background: %11; border: 2px solid %11; }
        QCheckBox#rnCheck::indicator:hover { border-color: %10; }
        QPushButton#rnBtn {
            background: %8; color: %2;
            border: 1px solid %5; border-radius: 4px;
            padding: 6px 14px; min-height: 26px;
        }
        QPushButton#rnBtn:hover { background: %9; color: %4; }
        QPushButton#rnBtn:default { background: %6; color: %4; border-color: %10; }
    )").arg(Theme::panelBackground(),      // 1
           Theme::textPrimary(),           // 2
           Theme::inputBackground(),       // 3
           Theme::textBright(),            // 4
           Theme::panelBorder(),           // 5
           Theme::accentInfoSoft(),        // 6
           Theme::textMuted(),             // 7
           Theme::hoverOverlay(),          // 8
           Theme::hoverStrong(),           // 9
           Theme::accentInfoBorderSoft(),  // 10
           Theme::accentInfo()             // 11 (checkbox marcado)
        ));
}

RenameService::Plan RenameDialog::resultPlan() const {
    RenameService::Plan out = m_plan;
    for (int gi = 0; gi < m_groups.size(); ++gi) {
        const bool on = (gi < m_checks.size() && m_checks.at(gi))
            ? m_checks.at(gi)->isChecked()
            : m_groups.at(gi).selected;
        for (int idx : m_groups.at(gi).indexes)
            out.occurrences[idx].selected = on;
    }
    out.keepOldAsAlias = m_aliasKeepCheck && m_aliasKeepCheck->isChecked();
    return out;
}
