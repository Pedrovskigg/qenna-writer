#include "TimelineGeneratorDialog.h"

#include "ProjectModel.h"
#include "Theme.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr int kSummaryBoxHeight = 64; // ~3 linhas — resumo curto, mas legível
}

TimelineGeneratorDialog::TimelineGeneratorDialog(ProjectModel* model, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
{
    setWindowTitle(tr("Gerador de Timeline"));
    setModal(true);
    if (parent) setStyleSheet(parent->styleSheet());
    resize(720, 640);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto* hint = new QLabel(tr(
        "Preencha \"quando se passa\" e o resumo de vários capítulos/cenas de uma vez, "
        "em vez de abrir a edição um por um — útil pra colocar um manuscrito inteiro em "
        "dia com a Timeline. Os dois campos são opcionais e alimentam a Timeline "
        "automaticamente."), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:%1; font-size:11px;").arg(Theme::textMuted()));
    root->addWidget(hint);

    auto* msRow = new QHBoxLayout();
    msRow->addWidget(new QLabel(tr("Manuscrito:"), this));
    m_manuscriptCombo = new QComboBox(this);
    if (m_model) {
        for (const Manuscript& ms : m_model->manuscripts())
            m_manuscriptCombo->addItem(ms.title.isEmpty() ? tr("Sem título") : ms.title, ms.id);
    }
    connect(m_manuscriptCombo, &QComboBox::currentIndexChanged, this, &TimelineGeneratorDialog::rebuildRows);
    msRow->addWidget(m_manuscriptCombo, 1);
    root->addLayout(msRow);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_rowsContainer = new QWidget(scroll);
    m_rowsLayout = new QGridLayout(m_rowsContainer);
    m_rowsLayout->setContentsMargins(0, 0, 8, 0);
    m_rowsLayout->setHorizontalSpacing(8);
    m_rowsLayout->setVerticalSpacing(4);
    m_rowsLayout->setColumnStretch(0, 2);
    m_rowsLayout->setColumnStretch(1, 2);
    m_rowsLayout->setColumnStretch(2, 3);
    scroll->setWidget(m_rowsContainer);
    root->addWidget(scroll, 1);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto* saveBtn = btns->addButton(tr("Salvar tudo"), QDialogButtonBox::AcceptRole);
    saveBtn->setDefault(true);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, this, &TimelineGeneratorDialog::saveAll);
    root->addWidget(btns);

    rebuildRows();
}

void TimelineGeneratorDialog::rebuildRows()
{
    // Remonta a grade inteira do zero — mais simples que reconciliar linhas
    // existentes, e essa tela não precisa manter estado ao trocar de manuscrito.
    QLayoutItem* item;
    while ((item = m_rowsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_rows.clear();

    if (!m_model || m_manuscriptCombo->count() == 0) return;
    const QString msId = m_manuscriptCombo->currentData().toString();

    QList<Chapter> chapters;
    for (const Chapter& c : m_model->chapters())
        if (c.manuscriptId == msId) chapters.append(c);
    std::sort(chapters.begin(), chapters.end(), [](const Chapter& a, const Chapter& b) {
        return a.order < b.order;
    });

    const QString muted = Theme::textMuted();
    int row = 0;

    m_rowsLayout->addWidget(new QLabel(QStringLiteral("<b style='font-size:11px;'>%1</b>")
        .arg(tr("Capítulo / Cena")), m_rowsContainer), row, 0);
    m_rowsLayout->addWidget(new QLabel(QStringLiteral("<b style='font-size:11px;'>%1</b>")
        .arg(tr("Quando se passa")), m_rowsContainer), row, 1);
    m_rowsLayout->addWidget(new QLabel(QStringLiteral("<b style='font-size:11px;'>%1</b>")
        .arg(tr("Resumo")), m_rowsContainer), row, 2);
    ++row;

    int chapterNum = 0;
    for (const Chapter& c : chapters) {
        ++chapterNum;
        const QString chapTitle = c.title.isEmpty() ? tr("Capítulo %1").arg(chapterNum) : c.title;

        auto* titleLabel = new QLabel(chapTitle, m_rowsContainer);
        titleLabel->setStyleSheet(QStringLiteral("font-weight:600;"));
        m_rowsLayout->addWidget(titleLabel, row, 0, Qt::AlignTop);
        auto* markerEdit = new QLineEdit(c.timeMarker, m_rowsContainer);
        markerEdit->setPlaceholderText(tr("ex.: Dia 5, Verão de 1999…"));
        m_rowsLayout->addWidget(markerEdit, row, 1, Qt::AlignTop);
        auto* summaryEdit = new QPlainTextEdit(m_rowsContainer);
        summaryEdit->setPlainText(c.summary);
        summaryEdit->setPlaceholderText(tr("Resumo curto do capítulo…"));
        summaryEdit->setFixedHeight(kSummaryBoxHeight);
        m_rowsLayout->addWidget(summaryEdit, row, 2);
        m_rows.append({ c.id, -1, markerEdit, summaryEdit });
        ++row;

        int sceneNum = 0;
        for (const Scene& sc : c.scenes) {
            const int sceneIndex = sceneNum;
            ++sceneNum;
            const QString scTitle = sc.title.isEmpty() ? tr("Cena %1").arg(sceneNum) : sc.title;

            auto* scLabel = new QLabel(QStringLiteral("    ↳ %1").arg(scTitle), m_rowsContainer);
            scLabel->setStyleSheet(QStringLiteral("color:%1;").arg(muted));
            m_rowsLayout->addWidget(scLabel, row, 0, Qt::AlignTop);
            auto* scMarkerEdit = new QLineEdit(sc.timeMarker, m_rowsContainer);
            scMarkerEdit->setPlaceholderText(tr("herda do capítulo se vazio"));
            m_rowsLayout->addWidget(scMarkerEdit, row, 1, Qt::AlignTop);
            auto* scSummaryEdit = new QPlainTextEdit(m_rowsContainer);
            scSummaryEdit->setPlainText(sc.summary);
            scSummaryEdit->setPlaceholderText(tr("herda do capítulo se vazio"));
            scSummaryEdit->setFixedHeight(kSummaryBoxHeight);
            m_rowsLayout->addWidget(scSummaryEdit, row, 2);
            m_rows.append({ c.id, sceneIndex, scMarkerEdit, scSummaryEdit });
            ++row;
        }
    }
}

void TimelineGeneratorDialog::saveAll()
{
    if (!m_model) { reject(); return; }

    m_model->beginBatchUpdate();
    for (const RowRef& r : m_rows) {
        if (r.sceneIndex < 0) {
            m_model->updateChapterTimeMarker(r.chapterId, r.markerEdit->text().trimmed());
            m_model->updateChapterSummary(r.chapterId, r.summaryEdit->toPlainText().trimmed());
        } else {
            m_model->updateSceneTimeMarker(r.chapterId, r.sceneIndex, r.markerEdit->text().trimmed());
            m_model->updateSceneSummary(r.chapterId, r.sceneIndex, r.summaryEdit->toPlainText().trimmed());
        }
    }
    m_model->endBatchUpdate();

    accept();
}
