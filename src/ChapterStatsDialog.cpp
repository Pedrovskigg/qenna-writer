#include "ChapterStatsDialog.h"

#include "DialogueStore.h"
#include "ElementsStore.h"
#include "ProjectModel.h"
#include "Theme.h"
#include "WordCounter.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

// Mesma lógica do WritingStatsDialog — locale de formatação numérica segue o
// idioma do app, não fica fixa em pt-BR.
QLocale statsLocale()
{
    QSettings qs;
    const bool en = qs.value(QStringLiteral("app/language")).toString() == QStringLiteral("en");
    return en ? QLocale(QLocale::English, QLocale::UnitedStates)
              : QLocale(QLocale::Portuguese, QLocale::Brazil);
}

} // namespace

ChapterStatsDialog::ChapterStatsDialog(WordCounter* wordCounter, DialogueStore* dialogueStore,
                                       ElementsStore* elementsStore, ProjectModel* model,
                                       const QString& manuscriptId, const QString& chapterId,
                                       QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , m_wordCounter(wordCounter)
    , m_dialogueStore(dialogueStore)
    , m_elementsStore(elementsStore)
    , m_model(model)
    , m_manuscriptId(manuscriptId)
    , m_chapterId(chapterId)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setObjectName(QStringLiteral("chapterStatsDialog"));
    setModal(false);
    buildUi();
}

void ChapterStatsDialog::buildUi()
{
    const Chapter* chapter = m_model ? m_model->findChapter(m_chapterId) : nullptr;
    const QString chapterTitle = chapter && !chapter->title.isEmpty()
        ? chapter->title : tr("Capítulo sem título");
    const bool hasMultiScenes = chapter && chapter->scenes.size() > 1;

    QLocale loc = statsLocale();

    const int totalWords = m_wordCounter ? m_wordCounter->countChapter(m_chapterId) : 0;

    int totalDialogues = 0;
    if (m_dialogueStore) {
        for (const DialogueStore::Dialogue& d : m_dialogueStore->dialogues())
            if (d.chapterId == m_chapterId) ++totalDialogues;
    }

    // Personagens presentes no capítulo inteiro (união agregada, fonte de
    // verdade) — narrador incluído aqui, mas nunca em nenhuma cena específica
    // (ver ElementsStore::elementDocKeyForScene: a derivação por cena
    // deliberadamente pula narradores).
    struct CharRow { QString id; QString name; QStringList sceneLabels; bool isNarrator = false; };
    QVector<CharRow> chars;
    if (m_elementsStore && chapter) {
        const QString chapterKey = ElementsStore::elementDocKeyForChapter(m_manuscriptId, m_chapterId);
        for (const QString& elId : m_elementsStore->docElementIds(chapterKey)) {
            const Element* el = m_elementsStore->findElement(elId);
            if (!el || el->type != QStringLiteral("character")) continue;
            CharRow row;
            row.id = elId;
            row.name = el->name.isEmpty() ? tr("(sem nome)") : el->name;
            row.isNarrator = el->narrator;
            if (hasMultiScenes && !el->narrator) {
                for (int i = 0; i < chapter->scenes.size(); ++i) {
                    const Scene& sc = chapter->scenes.at(i);
                    const QString sceneKey = ElementsStore::elementDocKeyForScene(
                        m_manuscriptId, m_chapterId, sc.id);
                    if (m_elementsStore->docElementIds(sceneKey).contains(elId)) {
                        row.sceneLabels.append(sc.title.isEmpty()
                            ? tr("Cena %1").arg(i + 1)
                            : sc.title);
                    }
                }
            }
            chars.append(row);
        }
        std::sort(chars.begin(), chars.end(), [](const CharRow& a, const CharRow& b) {
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });
    }

    // ── UI ───────────────────────────────────────────────────────────────

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("chStatsHeader"));
    auto* headerLay = new QHBoxLayout(header);
    headerLay->setContentsMargins(14, 10, 8, 10);
    headerLay->setSpacing(6);

    auto* titleLbl = new QLabel(tr("Estatísticas — %1").arg(chapterTitle), header);
    titleLbl->setObjectName(QStringLiteral("chStatsTitle"));
    titleLbl->setWordWrap(true);
    headerLay->addWidget(titleLbl, 1);

    auto* closeBtn = new QPushButton(QStringLiteral("✕"), header);
    closeBtn->setObjectName(QStringLiteral("chStatsClose"));
    closeBtn->setFixedSize(22, 22);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    headerLay->addWidget(closeBtn, 0, Qt::AlignTop);

    outer->addWidget(header);

    auto* sep0 = new QFrame(this);
    sep0->setFrameShape(QFrame::HLine);
    sep0->setObjectName(QStringLiteral("chStatsSep"));
    outer->addWidget(sep0);

    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("chStatsBody"));
    auto* bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(14, 12, 14, 14);
    bodyLay->setSpacing(8);

    auto addStat = [&](const QString& label, const QString& value, bool accent = false) {
        auto* row = new QHBoxLayout();
        row->setSpacing(8);
        auto* labelLbl = new QLabel(label, body);
        labelLbl->setObjectName(QStringLiteral("chStatsLabel"));
        row->addWidget(labelLbl);
        row->addStretch();
        auto* valueLbl = new QLabel(value, body);
        valueLbl->setObjectName(accent ? QStringLiteral("chStatsValueAccent")
                                       : QStringLiteral("chStatsValue"));
        row->addWidget(valueLbl);
        bodyLay->addLayout(row);
    };

    auto addSection = [&](const QString& title) {
        if (bodyLay->count() > 0) {
            auto* s = new QFrame(body);
            s->setFrameShape(QFrame::HLine);
            s->setObjectName(QStringLiteral("chStatsSep"));
            bodyLay->addWidget(s);
        }
        auto* lbl = new QLabel(title.toUpper(), body);
        lbl->setObjectName(QStringLiteral("chStatsSectionTitle"));
        bodyLay->addWidget(lbl);
    };

    // — Capítulo —
    addSection(tr("Capítulo"));
    addStat(tr("Palavras totais"), loc.toString(totalWords), true);
    addStat(tr("Diálogos detectados"), loc.toString(totalDialogues));

    // — Cenas — só quando há mais de uma (capítulo de cena única não ganha
    // quebra, mesma regra visual já usada no ManuscriptPanel).
    if (hasMultiScenes) {
        addSection(tr("Cenas"));
        for (int i = 0; i < chapter->scenes.size(); ++i) {
            const Scene& sc = chapter->scenes.at(i);
            const QString label = sc.title.isEmpty() ? tr("Cena %1").arg(i + 1) : sc.title;
            const int sceneWords = m_wordCounter ? m_wordCounter->countScene(m_chapterId, i) : 0;
            addStat(label, loc.toString(sceneWords));
        }
    }

    // — Personagens presentes — cada linha expande (clicável) mostrando em
    // quais cenas aparece, quando o capítulo tem mais de uma cena.
    addSection(tr("Personagens presentes (%1)").arg(chars.size()));
    if (chars.isEmpty()) {
        auto* empty = new QLabel(tr("Nenhum personagem marcado neste capítulo ainda."), body);
        empty->setObjectName(QStringLiteral("chStatsLabel"));
        empty->setWordWrap(true);
        bodyLay->addWidget(empty);
    }
    for (const CharRow& row : chars) {
        const bool expandable = hasMultiScenes && !row.isNarrator;

        auto* charToggle = new QToolButton(body);
        charToggle->setObjectName(QStringLiteral("chStatsCharToggle"));
        charToggle->setToolButtonStyle(Qt::ToolButtonTextOnly);
        charToggle->setCursor(expandable ? Qt::PointingHandCursor : Qt::ArrowCursor);
        const QString suffix = row.isNarrator ? tr(" (narrador)") : QString();
        charToggle->setText(expandable ? tr("▸  %1%2").arg(row.name, suffix)
                                       : tr("•  %1%2").arg(row.name, suffix));
        bodyLay->addWidget(charToggle);

        if (!expandable) continue;

        auto* sceneList = new QWidget(body);
        auto* sceneLay = new QVBoxLayout(sceneList);
        sceneLay->setContentsMargins(20, 2, 0, 4);
        sceneLay->setSpacing(2);
        if (row.sceneLabels.isEmpty()) {
            auto* none = new QLabel(tr("Presença não confirmada em nenhuma cena específica ainda."), sceneList);
            none->setObjectName(QStringLiteral("chStatsCharScene"));
            none->setWordWrap(true);
            sceneLay->addWidget(none);
        } else {
            for (const QString& sceneLabel : row.sceneLabels) {
                auto* sceneLbl = new QLabel(QStringLiteral("· %1").arg(sceneLabel), sceneList);
                sceneLbl->setObjectName(QStringLiteral("chStatsCharScene"));
                sceneLay->addWidget(sceneLbl);
            }
        }
        sceneList->setVisible(false);
        bodyLay->addWidget(sceneList);

        connect(charToggle, &QToolButton::clicked, this, [charToggle, sceneList, row]() {
            const bool nowVisible = !sceneList->isVisible();
            sceneList->setVisible(nowVisible);
            const QString suffix = row.isNarrator ? QStringLiteral(" (narrador)") : QString();
            charToggle->setText(QStringLiteral("%1  %2%3")
                .arg(nowVisible ? QStringLiteral("▾") : QStringLiteral("▸"), row.name, suffix));
        });
    }

    // Corpo rolável — capítulos com muita gente/cenas geram uma lista maior
    // que a tela sobrando embaixo do cursor (onde a janela abre), então a
    // altura fica travada e o excesso vira scroll em vez de cortar.
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("chStatsScroll"));
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setWidget(body);

    int maxBodyHeight = 420;
    if (QScreen* scr = this->screen())
        maxBodyHeight = qMin(maxBodyHeight, int(scr->availableGeometry().height() * 0.6));
    scrollArea->setMaximumHeight(maxBodyHeight);

    outer->addWidget(scrollArea);

    setStyleSheet(QStringLiteral(R"(
        QDialog#chapterStatsDialog {
            background: %1;
            border: 1px solid %3;
            border-radius: 8px;
        }
        QWidget#chStatsHeader, QWidget#chStatsBody { background: transparent; }
        QScrollArea#chStatsScroll { background: transparent; border: none; }
        QScrollArea#chStatsScroll QScrollBar:vertical {
            width: 9px;
            background: transparent;
            margin: 0;
        }
        QScrollArea#chStatsScroll QScrollBar::handle:vertical {
            background: %3;
            border-radius: 4px;
            min-height: 24px;
        }
        QScrollArea#chStatsScroll QScrollBar::add-line:vertical,
        QScrollArea#chStatsScroll QScrollBar::sub-line:vertical { height: 0; }
        QScrollArea#chStatsScroll QScrollBar::add-page:vertical,
        QScrollArea#chStatsScroll QScrollBar::sub-page:vertical { background: transparent; }
        QLabel#chStatsTitle {
            color: %5;
            font-size: 12px;
            font-weight: 700;
        }
        QPushButton#chStatsClose {
            background: transparent;
            color: %4;
            border: none;
            border-radius: 4px;
            font-size: 10px;
        }
        QPushButton#chStatsClose:hover { background: %2; color: %5; }
        QFrame#chStatsSep {
            background: %3;
            border: none;
            max-height: 1px;
            margin: 2px 0;
        }
        QLabel#chStatsSectionTitle {
            color: %6;
            font-size: 9px;
            font-weight: 800;
            letter-spacing: 0.8px;
        }
        QLabel#chStatsLabel {
            color: %4;
            font-size: 11px;
        }
        QLabel#chStatsValue {
            color: %5;
            font-size: 11px;
            font-weight: 600;
        }
        QLabel#chStatsValueAccent {
            color: %6;
            font-size: 11px;
            font-weight: 700;
        }
        QToolButton#chStatsCharToggle {
            background: transparent;
            color: %5;
            border: none;
            font-size: 11px;
            font-weight: 600;
            text-align: left;
            padding: 2px 0;
        }
        QToolButton#chStatsCharToggle:hover { color: %6; }
        QLabel#chStatsCharScene {
            color: %4;
            font-size: 10px;
        }
    )")
        .arg(Theme::panelBackground(),  // 1
             Theme::hoverOverlay(),     // 2
             Theme::panelBorder(),      // 3
             Theme::textPrimary(),      // 4
             Theme::textBright(),       // 5
             Theme::accentDefault()));  // 6

    setFixedWidth(320);
    adjustSize();
}
