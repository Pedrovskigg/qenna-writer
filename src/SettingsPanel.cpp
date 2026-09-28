#include "SettingsPanel.h"
#include "SceneBreaks.h"
#include "GlossaryInText.h"
#include "IconUtils.h"
#include "LeftBar.h"
#include "PanelMotion.h"
#include "SmoothCaret.h"

#include "AboutDialog.h"
#include "EditorLayout.h"
#include "MiraPersonality.h"
#include "SpellChecker.h"
#include "Theme.h"
#include "UiScale.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QKeyEvent>
#include <QTextEdit>
#include <functional>
#include <QSpinBox>
#include <QStyle>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace {
// Menor folha de altura fixa oferecida pelo slider. Abaixo disso vira inútil;
// o extremo direito do slider é "Tela cheia" (preenche a janela).
constexpr int kMinSheetHeight = 240;
constexpr int kNavWidth = 200;
constexpr int kControlWidth = 240;

// Endpoint fixo de cada provedor + um modelo de exemplo (vira placeholder,
// não sobrescreve o campo de Modelo — só o Endpoint é preenchido de fato).
// Todos falam o formato OpenAI chat/completions, que é o único que o
// AIClient sabe montar; "custom" não tem baseUrl (não mexe no campo).
struct AiProviderInfo {
    const char* key;
    const char* baseUrl;
    const char* modelHint;
};
constexpr AiProviderInfo kAiProviders[] = {
    { "openai",    "https://api.openai.com/v1",                          "gpt-4o-mini" },
    { "anthropic", "https://api.anthropic.com/v1",                       "claude-sonnet-5" },
    { "gemini",    "https://generativelanguage.googleapis.com/v1beta/openai", "gemini-2.5-flash" },
    { "xai",       "https://api.x.ai/v1",                                "grok-4" },
    { "custom",    "",                                                   "llama3.2" },
};

// Caixa de teste do cursor suave: uma linha só (Enter não quebra).
class SingleLineFilter : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject*, QEvent* e) override {
        if (e->type() != QEvent::KeyPress) return false;
        const int k = static_cast<QKeyEvent*>(e)->key();
        return k == Qt::Key_Return || k == Qt::Key_Enter;
    }
};

// Liga/desliga em forma de chave. Continua sendo um QCheckBox (toggled,
// setChecked, isChecked), então quem já falava com os checkboxes não muda.
class ToggleSwitch : public QCheckBox {
public:
    explicit ToggleSwitch(QWidget* parent = nullptr) : QCheckBox(parent) {
        setCursor(Qt::PointingHandCursor);
        setFixedSize(36, 20);
    }
protected:
    bool hitButton(const QPoint&) const override { return true; }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const bool on = isChecked();
        QColor track = Theme::toColor(on ? Theme::accentDefault() : Theme::borderStrong());
        QColor knob = on ? QColor(Qt::white) : Theme::toColor(Theme::textPrimary());
        if (!isEnabled()) { track.setAlphaF(track.alphaF() * 0.4); knob.setAlphaF(0.5); }
        p.setPen(Qt::NoPen);
        p.setBrush(track);
        const QRectF r(0.5, 0.5, width() - 1.0, height() - 1.0);
        p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
        const qreal d = height() - 7.0;
        p.setBrush(knob);
        p.drawEllipse(QRectF(on ? width() - 3.5 - d : 3.5, 3.5, d, d));
        if (hasFocus()) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(Theme::toColor(Theme::focusBorder()), 1.2));
            p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
        }
    }
};

// Escolha de 2 ou 3 opções como botões lado a lado. Espelha um QComboBox
// escondido, que continua sendo a fonte da verdade (setters e sinais de hoje).
class Segmented : public QWidget {
public:
    Segmented(QComboBox* combo, QWidget* parent) : QWidget(parent) {
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(0);
        const int n = combo->count();
        for (int i = 0; i < n; ++i) {
            auto* b = new QPushButton(combo->itemText(i), this);
            b->setObjectName(QStringLiteral("settingsSegBtn"));
            b->setProperty("pos", n == 1 ? "only" : i == 0 ? "first" : i == n - 1 ? "last" : "mid");
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            lay->addWidget(b);
            m_buttons << b;
            connect(b, &QPushButton::clicked, this, [this, combo, i]() {
                combo->setCurrentIndex(i);
                sync(combo);
            });
        }
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, combo]() { sync(combo); });
        sync(combo);
        combo->hide();
    }
private:
    void sync(QComboBox* combo) {
        for (int i = 0; i < m_buttons.size(); ++i) m_buttons.at(i)->setChecked(i == combo->currentIndex());
    }
    QList<QPushButton*> m_buttons;
};

// Sem acento e em minúsculas, pra busca achar "memoria" em "Memória".
QString fold(const QString& s) {
    const QString d = s.normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(d.size());
    for (const QChar c : d)
        if (c.category() != QChar::Mark_NonSpacing) out.append(c.toLower());
    return out;
}

// Título com o trecho buscado marcado. A dobra de acento não muda o tamanho
// do texto nos casos do português, então o índice vale no original.
QString highlighted(const QString& plain, const QString& query) {
    const int i = fold(plain).indexOf(fold(query));
    if (i < 0 || fold(plain).size() != plain.size()) return plain.toHtmlEscaped();
    QColor mark = Theme::toColor(Theme::accentDefault());
    mark.setAlphaF(0.32);
    return plain.left(i).toHtmlEscaped()
        + QStringLiteral("<span style=\"background-color:%1;\">").arg(mark.name(QColor::HexArgb))
        + plain.mid(i, query.size()).toHtmlEscaped() + QStringLiteral("</span>")
        + plain.mid(i + query.size()).toHtmlEscaped();
}

QLabel* valueLabel(QWidget* parent) {
    auto* l = new QLabel(parent);
    l->setObjectName(QStringLiteral("pageValueLabel"));
    l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    l->setMinimumWidth(62);
    return l;
}

// Slider + valor, lado a lado, no espaço de controle da linha.
QWidget* sliderBox(QSlider* slider, QLabel* value, QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);
    slider->setParent(w);
    slider->setFixedWidth(150);
    value->setParent(w);
    lay->addWidget(slider);
    lay->addWidget(value);
    return w;
}
}

SettingsPanel::SettingsPanel(QWidget* parent)
    : QDialog(parent)
    , m_spellCheck(new ToggleSwitch(this))
    , m_langCombo(new QComboBox(this))
{
    setObjectName(QStringLiteral("settingsPanel"));
    setWindowTitle(tr("Configurações"));
    setModal(false);
    resize(830, 620);
    setMinimumSize(760, 480);

    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged, this, [this]() {
        applyTheme();
        refreshThemeName();
        buildNav(nullptr);   // repinta os ícones do trilho na cor do tema novo
    });

    // ---- Páginas: uma seção por categoria, todas no mesmo scroll ----
    auto* paneBody = new QWidget(this);
    paneBody->setObjectName(QStringLiteral("settingsPaneBody"));
    auto* paneLay = new QVBoxLayout(paneBody);
    paneLay->setContentsMargins(22, 6, 22, 20);
    paneLay->setSpacing(0);
    const QString catTitles[CategoryCount] = {
        tr("Aparência"), tr("Escrita"), tr("Corretor"), tr("Personagens"),
        tr("Timeline"), tr("Assistente de IA"), tr("Backup"), tr("Avançado")
    };
    const QString catSubtitles[CategoryCount] = {
        tr("Tamanho, barras, tema e idioma."),
        tr("A folha, o cursor e os capítulos."),
        tr("Sublinhados no texto e dicionário."),
        tr("Presença nas cenas e menções com @."),
        tr("Tempo e resumo das cenas."),
        tr("Provedor, chave e imagem de personagem. O app nunca exige IA."),
        tr("Cópia zipada do projeto inteiro."),
        tr("Memória.")
    };
    for (int c = 0; c < CategoryCount; ++c) {
        Section& s = m_sections[c];
        s.widget = new QWidget(paneBody);
        auto* v = new QVBoxLayout(s.widget);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);
        auto* head = new QHBoxLayout;
        head->setContentsMargins(0, 0, 0, 0);
        s.bigTitle = new QLabel(catTitles[c], s.widget);
        s.bigTitle->setObjectName(QStringLiteral("settingsCatTitle"));
        head->addWidget(s.bigTitle);
        head->addStretch(1);
        v->addLayout(head);
        s.subtitle = new QLabel(catSubtitles[c], s.widget);
        s.subtitle->setObjectName(QStringLiteral("settingsCatSub"));
        s.subtitle->setWordWrap(true);
        v->addWidget(s.subtitle);
        s.smallTitle = new QLabel(catTitles[c].toUpper(), s.widget);
        s.smallTitle->setObjectName(QStringLiteral("settingsCatSmall"));
        s.smallTitle->hide();
        v->addWidget(s.smallTitle);
        s.rows = new QVBoxLayout;
        s.rows->setContentsMargins(0, 4, 0, 0);
        s.rows->setSpacing(0);
        v->addLayout(s.rows);
        paneLay->addWidget(s.widget);
        if (c == Assistant) {
            // O "?" da IA continua: explica que a chave é sua e que nada exige IA.
            auto* aiInfoBtn = new QPushButton(QStringLiteral("?"), s.widget);
            aiInfoBtn->setObjectName(QStringLiteral("settingsInfoBtn"));
            aiInfoBtn->setCursor(Qt::PointingHandCursor);
            aiInfoBtn->setFixedSize(20, 20);
            aiInfoBtn->setToolTip(tr("O que é isso?"));
            connect(aiInfoBtn, &QPushButton::clicked, this, [this]() {
                QMessageBox::information(this, tr("Assistente de IA"),
                    tr("O Qenna Writer conta com uma configuração completa para uso "
                       "de um assistente de IA para escrita. Ele pode fazer "
                       "revisões, leituras críticas, discutir sobre o projeto, "
                       "pesquisar nele e muito mais.\n\n"
                       "Como o Qenna Writer é uma ferramenta gratuita, o uso dessa "
                       "ferramenta exige que o usuário possua sua própria chave de "
                       "API.\n"
                       "Caso você não tenha uma chave de API ou simplesmente não "
                       "queira usar o assistente, sem problemas. O app não exige e "
                       "jamais exigirá ele para nenhuma função específica."));
            });
            head->insertWidget(1, aiInfoBtn, 0, Qt::AlignVCenter);
            head->insertSpacing(1, 8);
        }
    }
    m_noResults = new QLabel(paneBody);
    m_noResults->setObjectName(QStringLiteral("settingsCatSub"));
    m_noResults->setContentsMargins(0, 18, 0, 0);
    m_noResults->hide();
    paneLay->addWidget(m_noResults);
    paneLay->addStretch(1);

    // ================= Aparência =================
    m_uiScaleSlider = new QSlider(Qt::Horizontal);
    m_uiScaleSlider->setRange(qRound(UiScale::Manager::minScale() * 100),
                              qRound(UiScale::Manager::maxScale() * 100));
    m_uiScaleSlider->setSingleStep(5);
    m_uiScaleSlider->setPageStep(10);
    m_uiScaleSlider->setValue(qRound(UiScale::scale() * 100));
    m_uiScaleValue = valueLabel(nullptr);
    m_uiScaleValue->setText(tr("%1%").arg(m_uiScaleSlider->value()));
    addRow(Appearance, tr("Tamanho da interface"), tr("Barra de ferramentas, barra lateral e ícones."),
           sliderBox(m_uiScaleSlider, m_uiScaleValue, this),
           tr("Ajusta o tamanho da barra de ferramentas, da barra lateral e dos "
              "ícones — útil em telas menores ou de resolução mais alta."));
    connect(m_uiScaleSlider, &QSlider::valueChanged, this, [this](int v) {
        m_uiScaleValue->setText(tr("%1%").arg(v));
        UiScale::Manager::instance()->setScale(v / 100.0);
    });
    connect(UiScale::Manager::instance(), &UiScale::Manager::scaleChanged, this, [this]() {
        const int pct = qRound(UiScale::scale() * 100);
        const QSignalBlocker block(m_uiScaleSlider);
        m_uiScaleSlider->setValue(pct);
        m_uiScaleValue->setText(tr("%1%").arg(pct));
    });

    m_toolbarSideCombo = new QComboBox(this);
    m_toolbarSideCombo->addItem(tr("Topo"), 0);
    m_toolbarSideCombo->addItem(tr("Lateral direita"), 1);
    addRow(Appearance, tr("Barra de ferramentas"), tr("Onde a barra de ferramentas fica."),
           new Segmented(m_toolbarSideCombo, this));
    connect(m_toolbarSideCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_blockSignals) return;
        emit topToolbarSideChanged(m_toolbarSideCombo->itemData(idx).toInt());
    });

    // Só esquerda/direita: deitada em cima ou embaixo, o conteúdo dela
    // (gavetas, capítulos) atrapalharia o centro da tela.
    m_leftBarSideCombo = new QComboBox(this);
    m_leftBarSideCombo->addItem(tr("Esquerda"), 0);
    m_leftBarSideCombo->addItem(tr("Direita"), 1);
    addRow(Appearance, tr("Barra lateral"), tr("De que lado ficam as gavetas."),
           new Segmented(m_leftBarSideCombo, this));
    connect(m_leftBarSideCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_blockSignals) return;
        emit leftBarSideChanged(m_leftBarSideCombo->itemData(idx).toInt());
    });

    auto* labelsCheck = new ToggleSwitch(this);
    labelsCheck->setChecked(LeftBar::labelsEnabled());
    addRow(Appearance, tr("Nomes na barra lateral"), tr("Mostra o nome de cada botão com o mouse em cima."), labelsCheck,
           tr("Parando o mouse na barra lateral, ela mostra o nome de cada botão e quantos "
              "itens cada gaveta tem. Desligado, fica só o ícone, com a dica de sempre."));
    connect(labelsCheck, &QCheckBox::toggled, this, [](bool on) { LeftBar::setLabelsEnabled(on); });

    auto* animCheck = new ToggleSwitch(this);
    animCheck->setChecked(PanelMotion::enabled());
    addRow(Appearance, tr("Animações da interface"), tr("Gavetas deslizando e listas em cascata."), animCheck,
           tr("As gavetas e a gaveta de Manuscritos saem de trás da barra lateral, com as "
              "linhas entrando em cascata. Desligado, tudo aparece na hora."));
    connect(animCheck, &QCheckBox::toggled, this, [](bool on) { PanelMotion::setEnabled(on); });

    m_themeButton = new QPushButton(this);
    m_themeButton->setObjectName(QStringLiteral("settingsLinkBtn"));
    m_themeButton->setCursor(Qt::PointingHandCursor);
    refreshThemeName();
    addRow(Appearance, tr("Tema"), tr("Abre os Temas."), m_themeButton);
    connect(m_themeButton, &QPushButton::clicked, this, [this]() { emit themesRequested(); });

    // Idioma do app: antes só dava pra trocar no menu principal. Aqui não
    // reinicia sozinho (pode ter texto sem salvar): vale na próxima abertura.
    auto* appLangCombo = new QComboBox(this);
    appLangCombo->setFixedWidth(kControlWidth);
    const QList<QPair<QString, QString>> appLangs = {
        { QStringLiteral("pt_BR"), QStringLiteral("Português (Brasil)") },
        { QStringLiteral("en"),    QStringLiteral("English") },
        { QStringLiteral("es"),    QStringLiteral("Español") },
        { QStringLiteral("it"),    QStringLiteral("Italiano") },
        { QStringLiteral("fr"),    QStringLiteral("Français") },
    };
    for (const auto& l : appLangs) appLangCombo->addItem(l.second, l.first);
    {
        const int idx = appLangCombo->findData(QSettings().value(QStringLiteral("app/language"), QStringLiteral("pt_BR")).toString());
        appLangCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    addRow(Appearance, tr("Idioma do app"), tr("Vale na próxima vez que você abrir o Qenna."), appLangCombo);
    connect(appLangCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [appLangCombo](int idx) {
        QSettings().setValue(QStringLiteral("app/language"), appLangCombo->itemData(idx).toString());
    });

    // ================= Escrita =================
    auto makeSlider = [](int lo, int hi, int step) {
        auto* s = new QSlider(Qt::Horizontal);
        s->setRange(lo, hi);
        s->setSingleStep(step);
        s->setPageStep(step * 4);
        return s;
    };
    m_pageWidthSlider = makeSlider(EditorLayout::Manager::minPageWidth(), EditorLayout::Manager::maxPageWidth(), 20);
    // O comprimento vai de uma folha curta (kMinSheetHeight) até o extremo direito,
    // que significa "Tela cheia" (preenche a janela dinamicamente, armazenado como 0).
    m_pageHeightSlider = makeSlider(kMinSheetHeight, EditorLayout::Manager::maxPageHeight(), 20);
    m_hMarginSlider = makeSlider(EditorLayout::Manager::minHorizontalMargin(), EditorLayout::Manager::maxHorizontalMargin(), 2);
    m_vMarginSlider = makeSlider(EditorLayout::Manager::minVerticalMargin(), EditorLayout::Manager::maxVerticalMargin(), 2);
    m_pageWidthValue = valueLabel(nullptr);
    m_pageHeightValue = valueLabel(nullptr);
    m_hMarginValue = valueLabel(nullptr);
    m_vMarginValue = valueLabel(nullptr);
    const QString pageHint = tr("Define o tamanho da \"folha\" e o respiro interno entre a borda e o "
                                "texto. No comprimento máximo (\"Tela cheia\") a folha preenche a "
                                "janela inteira e acompanha o seu tamanho; arrastando para a esquerda, "
                                "a folha ganha uma altura fixa e o fundo aparece em volta. Vale para "
                                "todos os projetos.");
    addRow(Writing, tr("Largura da página"), tr("Largura da folha de escrita."),
           sliderBox(m_pageWidthSlider, m_pageWidthValue, this), pageHint);
    addRow(Writing, tr("Comprimento da página"), tr("\"Tela cheia\" acompanha a janela."),
           sliderBox(m_pageHeightSlider, m_pageHeightValue, this), pageHint);
    addRow(Writing, tr("Margem lateral"), tr("Respiro entre a borda da folha e o texto."),
           sliderBox(m_hMarginSlider, m_hMarginValue, this), pageHint);
    addRow(Writing, tr("Margem topo/base"), tr("Respiro em cima e embaixo."),
           sliderBox(m_vMarginSlider, m_vMarginValue, this), pageHint);
    syncPageLayoutFromManager();

    // Desenho da quebra de cena no editor (ver SceneBreaks)
    auto* breakCombo = new QComboBox(this);
    breakCombo->addItem(tr("Nó da Timeline"), int(SceneBreaks::Knot));
    breakCombo->addItem(tr("Nome da cena"), int(SceneBreaks::Name));
    breakCombo->setCurrentIndex(SceneBreaks::style() == SceneBreaks::Name ? 1 : 0);
    addRow(Writing, tr("Quebra de cena"), tr("Como a divisão entre cenas aparece no texto."),
           new Segmented(breakCombo, this),
           tr("Nó da Timeline: a bolinha da Timeline entre dois fios, na cor da linha da cena "
              "(Flashback na cor dele, vazada quando a cena não tem data). Nome da cena: "
              "\"CENA 2\" com o título e o \"quando se passa\" embaixo. Só muda o desenho na tela; "
              "o arquivo e a exportação não mudam."));
    connect(breakCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [breakCombo](int idx) {
        SceneBreaks::setStyle(SceneBreaks::Style(breakCombo->itemData(idx).toInt()));
    });

    auto* caretCheck = new ToggleSwitch(this);
    caretCheck->setChecked(SmoothCaret::enabledSetting());
    const QString caretHint = tr("Enquanto você digita, o cursor desliza até a próxima letra em vez de pular, "
                                 "como no Word. Desligado, volta o cursor de sempre.");
    addRow(Writing, tr("Cursor suave"), tr("O cursor desliza até a próxima letra, como no Word."), caretCheck, caretHint);
    // Tempos do cursor suave + uma linha pra testar com o efeito de verdade.
    QList<QWidget*> caretRows;
    auto msRow = [&](const QString& label, int value, int maxMs, std::function<void(int)> apply) {
        auto* sl = new QSlider(Qt::Horizontal);
        sl->setRange(0, maxMs);
        sl->setSingleStep(5);
        sl->setPageStep(25);
        sl->setValue(value);
        auto* val = valueLabel(nullptr);
        auto show = [val](int v) { val->setText(v == 0 ? tr("desligado") : tr("%1 ms").arg(v)); };
        show(value);
        connect(sl, &QSlider::valueChanged, this, [show, apply](int v) { show(v); apply(v); });
        caretRows << addRow(Writing, label, QString(), sliderBox(sl, val, this), caretHint, true);
    };
    msRow(tr("Deslize do cursor"), SmoothCaret::glideMs(), 200, [](int v) { SmoothCaret::setGlideMs(v); });
    msRow(tr("Fade da letra"), SmoothCaret::fadeMs(), 250, [](int v) { SmoothCaret::setFadeMs(v); });
    auto* caretTest = new QTextEdit(this);
    caretTest->setObjectName(QStringLiteral("caretTestBox"));
    caretTest->setAcceptRichText(false);
    caretTest->setLineWrapMode(QTextEdit::NoWrap);
    caretTest->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    caretTest->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    caretTest->setPlaceholderText(tr("Digite aqui pra testar…"));
    caretTest->document()->setDocumentMargin(6);
    QFont tf(QStringLiteral("Lora"));
    tf.setPixelSize(16);
    caretTest->setFont(tf);
    caretTest->setFixedHeight(QFontMetrics(tf).height() + 18);
    caretTest->setFixedWidth(kControlWidth + 70);
    // A cara da página: fundo e cor do editor. padding 0 por causa do
    // "QTextEdit { padding }" global do tema.
    caretTest->setStyleSheet(Theme::qss(QStringLiteral(
        "#caretTestBox { background: %1; color: %2; border: 1px solid %3; border-radius: @radius-control; padding: 0; }"))
        .arg(Theme::editorBackground(), Theme::editorTextColor(), Theme::subtleBorder()));
    caretTest->installEventFilter(new SingleLineFilter(caretTest));
    SmoothCaret::attach(caretTest);
    caretRows << addRow(Writing, tr("Testar o cursor"), QString(), caretTest, caretHint, true);
    for (QWidget* r : caretRows) r->setEnabled(SmoothCaret::enabledSetting());
    connect(caretCheck, &QCheckBox::toggled, this, [caretRows](bool on) {
        SmoothCaret::setEnabledSetting(on);
        for (QWidget* r : caretRows) r->setEnabled(on);
    });

    m_autoNavCheck = new ToggleSwitch(this);
    addRow(Writing, tr("Navegar automaticamente entre capítulos"), tr("Segurar o scroll na borda passa pro próximo capítulo."), m_autoNavCheck,
           tr("Ao chegar no início ou fim de um capítulo, manter o scroll pressionado "
              "na borda por 2 segundos avança ou retrocede automaticamente para o próximo."));
    connect(m_autoNavCheck, &QCheckBox::toggled, this, [this](bool checked) { emit autoNavEnabledChanged(checked); });

    m_romanNumeralsCheck = new ToggleSwitch(this);
    addRow(Writing, tr("Numerais romanos nos capítulos"), tr("\"3 - A Batalha\" vira \"III - A Batalha\"."), m_romanNumeralsCheck,
           tr("Troca o número que aparece antes do título do capítulo (ex.: \"3 - "
              "A Batalha\" vira \"III - A Batalha\") na barra lateral do manuscrito."));
    connect(m_romanNumeralsCheck, &QCheckBox::toggled, this, [this](bool checked) { emit romanChapterNumbersChanged(checked); });

    m_unifiedGoalCheck = new ToggleSwitch(this);
    addRow(Writing, tr("Meta unificada entre projetos"), tr("A meta do dia soma todos os projetos."), m_unifiedGoalCheck,
           tr("Quando ativado, a meta e o progresso do dia passam a ser somados entre "
              "todos os projetos abertos no Qenna Writer, em vez de contar isolado "
              "por projeto — útil pra quem escreve em mais de um no mesmo dia."));
    connect(m_unifiedGoalCheck, &QCheckBox::toggled, this, [this](bool checked) { emit unifiedGoalEnabledChanged(checked); });

    // ================= Corretor =================
    addRow(Spelling, tr("Corretor ortográfico"), tr("Sublinhado vermelho nas palavras desconhecidas."), m_spellCheck,
           tr("Palavras desconhecidas ganham um sublinhado vermelho. "
              "Clique com o botão direito numa delas para ver sugestões "
              "ou adicionar ao dicionário do projeto."));
    m_langCombo->setFixedWidth(kControlWidth);
    addRow(Spelling, tr("Idioma do dicionário"), tr("Qual dicionário o corretor usa."), m_langCombo);
    auto* glsCheck = new ToggleSwitch(this);
    glsCheck->setChecked(GlossaryInText::enabledSetting());
    addRow(Spelling, tr("Termos do glossário no texto"), tr("Sublinhado pontilhado e ficha no hover."), glsCheck,
           tr("Os termos do glossário ganham um sublinhado pontilhado discreto. Parando o mouse "
              "em cima, aparece a ficha do termo: tipo, definição e a primeira vez que ele aparece no livro."));
    connect(glsCheck, &QCheckBox::toggled, this, [this](bool on) {
        QSettings().setValue(QStringLiteral("editor/glossaryInText"), on);
        emit glossaryInTextChanged(on);
    });

    // ================= Personagens =================
    m_detectionCheck = new ToggleSwitch(this);
    addRow(Characters, tr("Detectar personagens automaticamente"), tr("Sugere marcar quem aparece na cena."), m_detectionCheck,
           tr("Quando ativado, o app detecta nomes de personagens no texto e sugere marcar a presença deles na cena."));
    m_detectionAllCheck = new ToggleSwitch(this);
    addRow(Characters, tr("Marcar todos sem confirmar"), tr("Marca direto, sem perguntar."), m_detectionAllCheck, QString(), true);
    m_rescanScenesBtn = new QPushButton(tr("Rodar"), this);
    addRow(Characters, tr("Detectar presença por cena em todos os capítulos"), tr("Pra capítulos antigos. Um por vez, sem travar."), m_rescanScenesBtn,
           tr("Preenche a presença por CENA (não só por capítulo) usando quem já está "
              "confirmado — útil pra capítulos antigos que você ainda não reabriu "
              "nesta versão. Roda um capítulo por vez, não trava o app."));
    connect(m_detectionCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_detectionAllCheck->setEnabled(checked);
        emit detectionEnabledChanged(checked);
    });
    connect(m_detectionAllCheck, &QCheckBox::toggled, this, [this](bool checked) { emit detectionMarkAllChanged(checked); });
    connect(m_rescanScenesBtn, &QPushButton::clicked, this, [this]() { emit rescanAllScenesRequested(); });

    m_mentionManuscriptsCheck = new ToggleSwitch(this);
    addRow(Characters, tr("Menções (@) a capítulos e cenas"), tr("Por padrão o @ só sugere as gavetas."), m_mentionManuscriptsCheck,
           tr("Por padrão, @ só sugere documentos das gavetas (personagens, locais etc.). "
              "Ative para incluir também capítulos e cenas do manuscrito."));
    connect(m_mentionManuscriptsCheck, &QCheckBox::toggled, this, [this](bool checked) { emit mentionManuscriptsEnabledChanged(checked); });

    // ================= Timeline =================
    m_scenePopupCheck = new ToggleSwitch(this);
    addRow(Timeline, tr("Perguntar tempo e resumo ao criar cena com \"----\""), tr("Alimenta a Timeline na hora."), m_scenePopupCheck,
           tr("Ao dividir o capítulo numa cena nova, pergunta o marcador temporal e "
              "o resumo que alimentam a linha do tempo. Se desligado, defina isso "
              "manualmente pelo clique direito na cena."));
    connect(m_scenePopupCheck, &QCheckBox::toggled, this, [this](bool checked) { emit showScenePopupOnHrChanged(checked); });

    // ================= Assistente de IA =================
    // Personalidade (nome/tom/dureza/descrição livre) mora só no
    // MiraPersonalityDialog (clique no nome da assistente no cabeçalho do chat).
    // Cada provedor tem endpoint fixo compatível com o formato OpenAI
    // (chat/completions + Authorization: Bearer), o único que o AIClient fala.
    // "Local" ainda não tem suporte: fica visível e desabilitado no combo.
    m_aiProviderCombo = new QComboBox(this);
    m_aiProviderCombo->setFixedWidth(kControlWidth);
    m_aiProviderCombo->addItem(QStringLiteral("OpenAI"), QStringLiteral("openai"));
    m_aiProviderCombo->addItem(QStringLiteral("Anthropic (Claude)"), QStringLiteral("anthropic"));
    m_aiProviderCombo->addItem(QStringLiteral("Google (Gemini)"), QStringLiteral("gemini"));
    m_aiProviderCombo->addItem(QStringLiteral("xAI (Grok)"), QStringLiteral("xai"));
    m_aiProviderCombo->addItem(tr("Local — em breve"), QStringLiteral("custom"));
    if (auto* model = qobject_cast<QStandardItemModel*>(m_aiProviderCombo->model())) {
        if (auto* item = model->item(m_aiProviderCombo->count() - 1))
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }
    addRow(Assistant, tr("Provedor"), tr("Anthropic, Gemini e xAI via endpoint compatível."), m_aiProviderCombo,
           tr("Trocar o provedor preenche o Endpoint automaticamente (Anthropic, "
              "Gemini e xAI oferecem endpoints compatíveis com o formato OpenAI, "
              "que é o único que este app fala). Suporte a modelo local ainda "
              "está a caminho."));
    m_aiApiKeyEdit = new QLineEdit(this);
    m_aiApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_aiApiKeyEdit->setPlaceholderText(tr("sk-..."));
    m_aiApiKeyEdit->setFixedWidth(kControlWidth);
    addRow(Assistant, tr("Chave de API"), tr("Fica salva neste computador, sem criptografia."), m_aiApiKeyEdit,
           tr("Usada pela assistente de revisão ao selecionar um trecho no editor. "
              "A chave fica salva localmente em texto puro, sem criptografia — "
              "não compartilhe seu computador/config com quem não deva ver essa chave."));
    m_aiBaseUrlEdit = new QLineEdit(this);
    m_aiBaseUrlEdit->setFixedWidth(kControlWidth);
    addRow(Assistant, tr("Endpoint (URL base)"), QString(), m_aiBaseUrlEdit);
    m_aiModelEdit = new QLineEdit(this);
    m_aiModelEdit->setFixedWidth(kControlWidth);
    addRow(Assistant, tr("Modelo"), QString(), m_aiModelEdit);
    m_aiAutoScanCheck = new ToggleSwitch(this);
    addRow(Assistant, tr("Ler documentos na 1ª vez que abrir um projeto"), tr("Uma chamada de API por documento. Custa de verdade."), m_aiAutoScanCheck,
           tr("Roda o mesmo scan do botão \"Ler documentos do projeto\" (uma "
              "chamada de API por documento) sozinho, em segundo plano, só na "
              "primeira vez que um projeto sem resumo salvo ainda é aberto. "
              "Desligado por padrão — liga sob sua responsabilidade, é custo "
              "de API real."));

    {
        QSettings settings;
        m_aiApiKeyEdit->setText(settings.value(QStringLiteral("ai/apiKey")).toString());
        const QString savedBaseUrl = settings.value(QStringLiteral("ai/baseUrl"),
            QStringLiteral("https://api.openai.com/v1")).toString();
        m_aiBaseUrlEdit->setText(savedBaseUrl);
        m_aiModelEdit->setText(settings.value(QStringLiteral("ai/model"),
            QStringLiteral("gpt-4o-mini")).toString());
        m_aiAutoScanCheck->setChecked(settings.value(QStringLiteral("ai/autoScanNewProjects"), false).toBool());

        // Provedor inicial deduzido do Endpoint salvo (não de uma chave própria
        // em QSettings) — assim quem já tinha um Endpoint customizado abre o
        // painel com "Personalizado" já selecionado, em vez de "OpenAI".
        int providerIdx = m_aiProviderCombo->count() - 1; // último item = "custom"
        const char* providerModelHint = "llama3.2";
        for (int i = 0; i < m_aiProviderCombo->count(); ++i) {
            const QString key = m_aiProviderCombo->itemData(i).toString();
            for (const AiProviderInfo& info : kAiProviders) {
                if (key != QLatin1String(info.key)) continue;
                if (key != QLatin1String("custom") && savedBaseUrl == QLatin1String(info.baseUrl)) {
                    providerIdx = i;
                    providerModelHint = info.modelHint;
                }
            }
        }
        QSignalBlocker blocker(m_aiProviderCombo);
        m_aiProviderCombo->setCurrentIndex(providerIdx);
        m_aiModelEdit->setPlaceholderText(QString::fromUtf8(providerModelHint));
    }
    connect(m_aiApiKeyEdit, &QLineEdit::editingFinished, this, [this]() {
        QSettings().setValue(QStringLiteral("ai/apiKey"), m_aiApiKeyEdit->text().trimmed());
    });
    connect(m_aiBaseUrlEdit, &QLineEdit::editingFinished, this, [this]() {
        QSettings().setValue(QStringLiteral("ai/baseUrl"), m_aiBaseUrlEdit->text().trimmed());
    });
    connect(m_aiModelEdit, &QLineEdit::editingFinished, this, [this]() {
        QSettings().setValue(QStringLiteral("ai/model"), m_aiModelEdit->text().trimmed());
    });
    connect(m_aiAutoScanCheck, &QCheckBox::toggled, this, [](bool checked) {
        QSettings().setValue(QStringLiteral("ai/autoScanNewProjects"), checked);
    });
    connect(m_aiProviderCombo, qOverload<int>(&QComboBox::activated), this, [this](int idx) {
        const QString key = m_aiProviderCombo->itemData(idx).toString();
        for (const AiProviderInfo& info : kAiProviders) {
            if (key != QLatin1String(info.key)) continue;
            m_aiModelEdit->setPlaceholderText(QString::fromUtf8(info.modelHint));
            if (key == QLatin1String("custom")) return;
            m_aiBaseUrlEdit->setText(QString::fromUtf8(info.baseUrl));
            QSettings().setValue(QStringLiteral("ai/baseUrl"), QString::fromUtf8(info.baseUrl));
            return;
        }
    });

    // Geração de imagem de personagem: mesma chave, sempre na API da OpenAI.
    const QString imgHint = tr("Ponto de partida do diálogo de geração (a escolha feita lá "
                               "atualiza estes campos) e também o que a %1 usa quando gera uma "
                               "imagem sozinha durante o chat, sem abrir diálogo nenhum. A "
                               "geração de imagem sempre usa a API oficial da OpenAI "
                               "(api.openai.com) com a Chave de API acima, independente do "
                               "Endpoint configurado pro chat.").arg(miraAssistantName());
    m_imgModelCombo = new QComboBox(this);
    m_imgModelCombo->setFixedWidth(kControlWidth);
    m_imgModelCombo->addItem(QStringLiteral("GPT Image 1 Mini"), QStringLiteral("gpt-image-1-mini"));
    m_imgModelCombo->addItem(QStringLiteral("GPT Image 1"), QStringLiteral("gpt-image-1"));
    m_imgQualityCombo = new QComboBox(this);
    m_imgQualityCombo->addItem(tr("Baixa"), QStringLiteral("low"));
    m_imgQualityCombo->addItem(tr("Média"), QStringLiteral("medium"));
    m_imgQualityCombo->addItem(tr("Alta"), QStringLiteral("high"));
    m_imgSizeCombo = new QComboBox(this);
    m_imgSizeCombo->addItem(tr("Quadrado"), QStringLiteral("1024x1024"));
    m_imgSizeCombo->addItem(tr("Retrato"), QStringLiteral("1024x1536"));
    m_imgSizeCombo->addItem(tr("Paisagem"), QStringLiteral("1536x1024"));
    {
        QSettings settings;
        auto restoreCombo = [&settings](QComboBox* combo, const QString& key, const QString& fallback) {
            const QString saved = settings.value(key, fallback).toString();
            const int idx = combo->findData(saved);
            combo->setCurrentIndex(idx >= 0 ? idx : 0);
        };
        restoreCombo(m_imgModelCombo, QStringLiteral("ai/imageModel"), QStringLiteral("gpt-image-1-mini"));
        restoreCombo(m_imgQualityCombo, QStringLiteral("ai/imageQuality"), QStringLiteral("medium"));
        restoreCombo(m_imgSizeCombo, QStringLiteral("ai/imageSize"), QStringLiteral("1024x1024"));
    }
    addRow(Assistant, tr("Imagem de personagem · modelo"), tr("Usa a API da OpenAI com a mesma chave."), m_imgModelCombo, imgHint);
    addRow(Assistant, tr("Imagem de personagem · qualidade"), QString(), new Segmented(m_imgQualityCombo, this), imgHint);
    addRow(Assistant, tr("Imagem de personagem · tamanho"), QString(), new Segmented(m_imgSizeCombo, this), imgHint);
    connect(m_imgModelCombo, &QComboBox::currentIndexChanged, this, [this]() {
        QSettings().setValue(QStringLiteral("ai/imageModel"), m_imgModelCombo->currentData().toString());
    });
    connect(m_imgQualityCombo, &QComboBox::currentIndexChanged, this, [this]() {
        QSettings().setValue(QStringLiteral("ai/imageQuality"), m_imgQualityCombo->currentData().toString());
    });
    connect(m_imgSizeCombo, &QComboBox::currentIndexChanged, this, [this]() {
        QSettings().setValue(QStringLiteral("ai/imageSize"), m_imgSizeCombo->currentData().toString());
    });

    // ================= Backup =================
    const QString backupHint = tr("Zipa a pasta inteira do projeto (manuscritos, fichas, lousas, tudo) "
                                  "periodicamente. Escolha uma pasta de destino FORA da pasta do "
                                  "projeto e, se possível, fora de Documentos — outro disco, pendrive "
                                  "ou uma pasta sincronizada na nuvem protegem de verdade contra "
                                  "perder o projeto e o backup juntos.");
    m_backupModeCombo = new QComboBox(this);
    m_backupModeCombo->addItem(tr("Desligado"), 0);
    m_backupModeCombo->addItem(tr("Automático"), 1);
    m_backupModeCombo->addItem(tr("Só lembrete"), 2);
    addRow(Backup, tr("Backup do projeto"), tr("Zipa a pasta inteira do projeto."), new Segmented(m_backupModeCombo, this), backupHint);

    auto* folderBox = new QWidget(this);
    auto* folderLay = new QHBoxLayout(folderBox);
    folderLay->setContentsMargins(0, 0, 0, 0);
    folderLay->setSpacing(8);
    m_backupFolderEdit = new QLineEdit(folderBox);
    m_backupFolderEdit->setReadOnly(true);
    m_backupFolderEdit->setPlaceholderText(tr("Nenhuma pasta escolhida"));
    m_backupFolderEdit->setFixedWidth(kControlWidth - 60);
    m_backupFolderBtn = new QPushButton(tr("Escolher…"), folderBox);
    folderLay->addWidget(m_backupFolderEdit);
    folderLay->addWidget(m_backupFolderBtn);
    addRow(Backup, tr("Pasta de destino"), tr("Fora da pasta do projeto."), folderBox, backupHint);

    m_backupIntervalCombo = new QComboBox(this);
    m_backupIntervalCombo->addItem(tr("1 dia"), 1440);
    m_backupIntervalCombo->addItem(tr("2 dias"), 2880);
    m_backupIntervalCombo->addItem(tr("3 dias"), 4320);
    m_backupIntervalCombo->addItem(tr("4 dias"), 5760);
    m_backupIntervalCombo->addItem(tr("5 dias"), 7200);
    m_backupIntervalCombo->addItem(tr("6 dias"), 8640);
    m_backupIntervalCombo->addItem(tr("7 dias"), 10080);
    m_backupIntervalCombo->setFixedWidth(120);
    m_backupIntervalRow = addRow(Backup, tr("A cada"), QString(), m_backupIntervalCombo, backupHint);

    m_backupRunBtn = new QPushButton(tr("Fazer agora"), this);
    m_backupStatusLabel = new QLabel(this);
    addRow(Backup, tr("Fazer backup agora"), QString(), m_backupRunBtn, backupHint, false, m_backupStatusLabel);

    connect(m_backupModeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
        applyFilter();   // "A cada" só aparece com o backup ligado
        if (m_blockSignals) return;
        emit backupModeChanged(m_backupModeCombo->itemData(idx).toInt());
    });
    connect(m_backupIntervalCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_blockSignals) return;
        emit backupIntervalMinutesChanged(m_backupIntervalCombo->itemData(idx).toInt());
    });
    connect(m_backupFolderBtn, &QPushButton::clicked, this, [this]() {
        const QString chosen = QFileDialog::getExistingDirectory(this,
            tr("Escolher pasta de destino do backup"), m_backupFolderEdit->text(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (chosen.isEmpty()) return;
        m_backupFolderEdit->setText(chosen);
        emit backupFolderChanged(chosen);
    });
    connect(m_backupRunBtn, &QPushButton::clicked, this, [this]() { emit backupRunNowRequested(); });

    // ================= Avançado =================
    m_maxDocsSpinBox = new QSpinBox(this);
    m_maxDocsSpinBox->setRange(1, 20);
    m_maxDocsSpinBox->setValue(6);
    m_maxDocsSpinBox->setFixedWidth(70);
    addRow(Advanced, tr("Documentos simultâneos na RAM"), tr("Mais é troca mais rápida; menos é menos memória."), m_maxDocsSpinBox,
           tr("O app mantém os documentos abertos recentemente na memória para troca rápida. "
              "Ao atingir o limite, o mais antigo (sem edições pendentes) é descarregado automaticamente."));
    connect(m_maxDocsSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) { emit maxDocsChanged(v); });

    // ---- Montagem: trilho | páginas, e o rodapé embaixo ----
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    auto* nav = new QWidget(this);
    nav->setObjectName(QStringLiteral("settingsNav"));
    nav->setFixedWidth(kNavWidth);
    buildNav(nav);
    body->addWidget(nav);

    m_pane = new QScrollArea(this);
    m_pane->setObjectName(QStringLiteral("settingsScrollArea"));
    m_pane->setWidgetResizable(true);
    m_pane->setFrameShape(QFrame::NoFrame);
    m_pane->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pane->setWidget(paneBody);
    body->addWidget(m_pane, 1);
    root->addLayout(body, 1);

    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("settingsFooter"));
    auto* footLay = new QHBoxLayout(footer);
    footLay->setContentsMargins(16, 10, 16, 10);
    footLay->setSpacing(8);
    auto* about = new QPushButton(tr("ⓘ  Qenna Writer %1 · Sobre").arg(QCoreApplication::applicationVersion()), footer);
    about->setObjectName(QStringLiteral("settingsAboutBtn"));
    about->setCursor(Qt::PointingHandCursor);
    about->setToolTip(tr("Sobre o Qenna Writer"));
    connect(about, &QPushButton::clicked, this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });
    footLay->addWidget(about);
    footLay->addStretch(1);
    auto* close = new QPushButton(tr("Fechar"), footer);
    close->setObjectName(QStringLiteral("settingsCloseBtn"));
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    footLay->addWidget(close);
    root->addWidget(footer);

    connect(m_spellCheck, &QCheckBox::toggled, this, &SettingsPanel::onCheckToggled);
    connect(m_langCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SettingsPanel::onLanguageChanged);

    // Layout da página — escreve direto no manager; ele emite layoutChanged()
    // e a MainWindow reage. Sem signals locais.
    auto* layoutMgr = EditorLayout::Manager::instance();
    connect(m_pageWidthSlider, &QSlider::valueChanged, this,
            [this, layoutMgr](int v) {
                m_pageWidthValue->setText(QStringLiteral("%1 px").arg(v));
                if (m_blockLayoutSignals) return;
                layoutMgr->setPageWidth(v);
            });
    connect(m_pageHeightSlider, &QSlider::valueChanged, this,
            [this, layoutMgr](int v) {
                m_pageHeightValue->setText(pageHeightLabelText(v));
                if (m_blockLayoutSignals) return;
                // Extremo direito = "Tela cheia" → armazena 0 (preenche a janela
                // dinamicamente). Valores menores são folhas de altura fixa.
                const bool full = (v >= m_pageHeightSlider->maximum());
                layoutMgr->setPageHeight(full ? 0 : v);
            });
    connect(m_hMarginSlider, &QSlider::valueChanged, this,
            [this, layoutMgr](int v) {
                m_hMarginValue->setText(QStringLiteral("%1 px").arg(v));
                if (m_blockLayoutSignals) return;
                layoutMgr->setHorizontalMargin(v);
            });
    connect(m_vMarginSlider, &QSlider::valueChanged, this,
            [this, layoutMgr](int v) {
                m_vMarginValue->setText(QStringLiteral("%1 px").arg(v));
                if (m_blockLayoutSignals) return;
                layoutMgr->setVerticalMargin(v);
            });
    // Sincronia reversa: se outro lugar mudar o layout, o slider reflete.
    connect(layoutMgr, &EditorLayout::Manager::layoutChanged,
            this, &SettingsPanel::syncPageLayoutFromManager);

    // Abre na última categoria usada.
    selectCategory(qBound(0, QSettings().value(QStringLiteral("settings/lastCategory"), 0).toInt(), CategoryCount - 1));
}

QWidget* SettingsPanel::addRow(int category, const QString& title, const QString& desc, QWidget* control,
                               const QString& longHint, bool sub, QLabel* descLabel)
{
    Section& s = m_sections[category];
    auto* row = new QWidget(s.widget);
    row->setObjectName(QStringLiteral("settingsRow"));
    row->setAttribute(Qt::WA_StyledBackground, true);
    auto* g = new QGridLayout(row);
    g->setContentsMargins(sub ? 18 : 0, 10, 0, 10);
    g->setHorizontalSpacing(18);
    g->setVerticalSpacing(2);
    auto* t = new QLabel(title.toHtmlEscaped(), row);
    t->setObjectName(sub ? QStringLiteral("settingsRowTitleSub") : QStringLiteral("settingsRowTitle"));
    t->setTextFormat(Qt::RichText);
    t->setWordWrap(true);
    g->addWidget(t, 0, 0);
    QLabel* d = descLabel;
    if (!d && !desc.isEmpty()) d = new QLabel(desc, row);
    if (d) {
        d->setParent(row);
        d->setObjectName(QStringLiteral("settingsRowDesc"));
        d->setWordWrap(true);
        g->addWidget(d, 1, 0);
    }
    control->setParent(row);
    g->addWidget(control, 0, 1, d ? 2 : 1, 1, Qt::AlignRight | Qt::AlignVCenter);
    g->setColumnStretch(0, 1);
    if (!longHint.isEmpty()) {
        t->setToolTip(longHint);
        if (d) d->setToolTip(longHint);
    }
    s.rows->addWidget(row);

    Row r;
    r.widget = row;
    r.title = t;
    r.plainTitle = title;
    r.haystack = fold(title + QLatin1Char(' ') + desc + QLatin1Char(' ') + longHint);
    r.category = category;
    r.sub = sub;
    m_rows.append(r);
    return row;
}

// Trilho: título, busca e uma linha por categoria (ícone, nome, contagem).
// Chamado de novo com nav == nullptr só pra repintar os ícones no tema novo.
void SettingsPanel::buildNav(QWidget* nav)
{
    static const char* icons[CategoryCount] = {
        ":/icons/theme-panel.svg", ":/icons/edit.svg", ":/icons/check.svg", ":/icons/elements/user.svg",
        ":/icons/leftbar/timeline.svg", ":/icons/elements/star.svg", ":/icons/save-project.svg", ":/icons/settings.svg"
    };
    const QColor muted = Theme::toColor(Theme::textMuted());
    const QColor bright = Theme::toColor(Theme::textBright());
    if (!nav) {
        for (int c = 0; c < CategoryCount; ++c)
            if (m_navItems[c])
                m_navItems[c]->setIcon(IconUtils::loadToolbarIcon(QString::fromLatin1(icons[c]), muted, bright, bright, QSize(16, 16)));
        return;
    }
    auto* v = new QVBoxLayout(nav);
    v->setContentsMargins(8, 14, 8, 12);
    v->setSpacing(2);
    auto* title = new QLabel(tr("Configurações"), nav);
    title->setObjectName(QStringLiteral("settingsTitle"));
    title->setContentsMargins(8, 0, 0, 8);
    v->addWidget(title);
    m_search = new QLineEdit(nav);
    m_search->setObjectName(QStringLiteral("settingsSearch"));
    m_search->setPlaceholderText(tr("Buscar opção"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/search.svg"), muted, muted, muted, QSize(14, 14)),
                        QLineEdit::LeadingPosition);
    connect(m_search, &QLineEdit::textChanged, this, [this]() { applyFilter(); });
    v->addWidget(m_search);
    v->addSpacing(8);

    auto* group = new QButtonGroup(nav);
    group->setExclusive(false);   // na busca, nenhuma fica marcada
    for (int c = 0; c < CategoryCount; ++c) {
        auto* b = new QPushButton(m_sections[c].bigTitle->text(), nav);
        b->setObjectName(QStringLiteral("settingsNavItem"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setIcon(IconUtils::loadToolbarIcon(QString::fromLatin1(icons[c]), muted, bright, bright, QSize(16, 16)));
        b->setIconSize(QSize(16, 16));
        auto* lay = new QHBoxLayout(b);
        lay->setContentsMargins(0, 0, 10, 0);
        lay->addStretch(1);
        auto* count = new QLabel(b);
        count->setObjectName(QStringLiteral("settingsNavCount"));
        count->setAttribute(Qt::WA_TransparentForMouseEvents);
        lay->addWidget(count);
        m_navItems[c] = b;
        m_navCounts[c] = count;
        group->addButton(b, c);
        connect(b, &QPushButton::clicked, this, [this, c]() {
            if (!m_search->text().isEmpty()) {
                const QSignalBlocker block(m_search);
                m_search->clear();
            }
            selectCategory(c);
        });
        v->addWidget(b);
    }
    v->addStretch(1);
}

void SettingsPanel::selectCategory(int category)
{
    m_category = category;
    QSettings().setValue(QStringLiteral("settings/lastCategory"), category);
    applyFilter();
    if (m_pane) m_pane->verticalScrollBar()->setValue(0);
}

// Categoria aberta (sem busca) ou resultados da busca em todas as categorias.
void SettingsPanel::applyFilter()
{
    if (!m_search) return;
    const QString q = m_search->text().trimmed();
    const bool searching = q.size() >= 2;
    const QString fq = fold(q);
    const bool backupOn = m_backupModeCombo && m_backupModeCombo->currentData().toInt() != 0;
    int hits[CategoryCount] = {};
    int totals[CategoryCount] = {};
    int anyHit = 0;
    for (Row& r : m_rows) {
        bool vis = !searching || r.haystack.contains(fq);
        if (r.widget == m_backupIntervalRow && !backupOn) vis = false;
        r.widget->setVisible(vis);
        r.title->setText(searching && vis ? highlighted(r.plainTitle, q) : r.plainTitle.toHtmlEscaped());
        if (!r.sub) ++totals[r.category];
        if (vis && searching && !r.sub) ++hits[r.category];
        if (vis && searching) ++anyHit;
    }
    for (int c = 0; c < CategoryCount; ++c) {
        Section& s = m_sections[c];
        s.widget->setVisible(searching ? hits[c] > 0 : c == m_category);
        if (auto* info = s.widget->findChild<QPushButton*>(QStringLiteral("settingsInfoBtn")))
            info->setVisible(!searching);
        s.bigTitle->setVisible(!searching);
        s.subtitle->setVisible(!searching);
        s.smallTitle->setVisible(searching);
        if (m_navItems[c]) {
            const QSignalBlocker block(m_navItems[c]);
            m_navItems[c]->setChecked(!searching && c == m_category);
            m_navItems[c]->setProperty("dim", searching && hits[c] == 0);
            m_navItems[c]->style()->unpolish(m_navItems[c]);
            m_navItems[c]->style()->polish(m_navItems[c]);
            m_navCounts[c]->setText(QString::number(searching ? hits[c] : totals[c]));
        }
    }
    // O "?" da IA mora no cabeçalho grande; na busca ele some junto.
    m_noResults->setVisible(searching && anyHit == 0);
    if (searching && anyHit == 0) m_noResults->setText(tr("Nenhuma opção com \"%1\".").arg(q));
}

void SettingsPanel::refreshThemeName()
{
    if (m_themeButton) m_themeButton->setText(Theme::Manager::instance()->current().name + QStringLiteral("  ›"));
}

void SettingsPanel::syncPageLayoutFromManager()
{
    if (!m_pageWidthSlider) return;
    m_blockLayoutSignals = true;
    auto* mgr = EditorLayout::Manager::instance();
    m_pageWidthSlider->setValue(mgr->pageWidth());
    // ph == 0 (Tela cheia) → extremo direito do slider. ph fixo → posição direta.
    m_pageHeightSlider->setValue(mgr->pageHeight() <= 0
                                 ? m_pageHeightSlider->maximum()
                                 : mgr->pageHeight());
    m_hMarginSlider->setValue(mgr->horizontalMargin());
    m_vMarginSlider->setValue(mgr->verticalMargin());
    m_pageWidthValue->setText(QStringLiteral("%1 px").arg(mgr->pageWidth()));
    m_pageHeightValue->setText(pageHeightLabelText(m_pageHeightSlider->value()));
    m_hMarginValue->setText(QStringLiteral("%1 px").arg(mgr->horizontalMargin()));
    m_vMarginValue->setText(QStringLiteral("%1 px").arg(mgr->verticalMargin()));
    m_blockLayoutSignals = false;
}

QString SettingsPanel::pageHeightLabelText(int v) const
{
    // Extremo direito do slider = preenche a janela (sem altura fixa).
    if (v <= 0 || (m_pageHeightSlider && v >= m_pageHeightSlider->maximum()))
        return tr("Tela cheia");
    return QStringLiteral("%1 px").arg(v);
}

void SettingsPanel::setPageHeightMaximum(int px)
{
    if (!m_pageHeightSlider || px <= 0) return;
    m_blockLayoutSignals = true;
    m_pageHeightSlider->setMaximum(px);
    m_blockLayoutSignals = false;
    // Reexibe o valor atual já dentro do novo teto (setMaximum pode tê-lo grampeado).
    syncPageLayoutFromManager();
}

void SettingsPanel::setRescanScenesButtonText(const QString& text)
{
    if (m_rescanScenesBtn) m_rescanScenesBtn->setText(text);
}

void SettingsPanel::setRescanScenesButtonEnabled(bool enabled)
{
    if (m_rescanScenesBtn) m_rescanScenesBtn->setEnabled(enabled);
}

void SettingsPanel::setTopToolbarSide(int side)
{
    if (!m_toolbarSideCombo) return;
    m_blockSignals = true;
    const int idx = m_toolbarSideCombo->findData(side);
    if (idx >= 0) m_toolbarSideCombo->setCurrentIndex(idx);
    m_blockSignals = false;
}

void SettingsPanel::setLeftBarSide(int side)
{
    if (!m_leftBarSideCombo) return;
    m_blockSignals = true;
    const int idx = m_leftBarSideCombo->findData(side);
    if (idx >= 0) m_leftBarSideCombo->setCurrentIndex(idx);
    m_blockSignals = false;
}

void SettingsPanel::setBackupMode(int mode)
{
    if (!m_backupModeCombo) return;
    m_blockSignals = true;
    const int idx = m_backupModeCombo->findData(mode);
    if (idx >= 0) m_backupModeCombo->setCurrentIndex(idx);
    m_blockSignals = false;
    applyFilter();
}

void SettingsPanel::setBackupFolder(const QString& folder)
{
    if (m_backupFolderEdit) m_backupFolderEdit->setText(folder);
}

void SettingsPanel::setBackupIntervalMinutes(int minutes)
{
    if (!m_backupIntervalCombo) return;
    m_blockSignals = true;
    const int idx = m_backupIntervalCombo->findData(minutes);
    if (idx >= 0) m_backupIntervalCombo->setCurrentIndex(idx);
    m_blockSignals = false;
}

void SettingsPanel::setBackupStatusText(const QString& text)
{
    if (m_backupStatusLabel) m_backupStatusLabel->setText(text);
}

void SettingsPanel::setBackupRunButtonEnabled(bool enabled)
{
    if (m_backupRunBtn) m_backupRunBtn->setEnabled(enabled);
}

void SettingsPanel::setAvailableSpellLanguages(const QList<QPair<QString, QString>>& langs)
{
    m_blockSignals = true;
    const QString prev = spellLanguage();
    m_langCombo->clear();
    // Primeira opção: acompanhar o idioma do app. Aparece mesmo quando esse
    // dicionário ainda não foi baixado — escolher dispara o download.
    const QString appDict = SpellChecker::dictionaryForAppLanguage();
    m_langCombo->addItem(tr("Idioma do app (%1)").arg(SpellChecker::labelForLanguage(appDict)),
                         SpellChecker::followAppValue());
    for (const auto& pair : langs) {
        m_langCombo->addItem(pair.second, pair.first);
    }
    if (!prev.isEmpty()) {
        const int idx = m_langCombo->findData(prev);
        if (idx >= 0) m_langCombo->setCurrentIndex(idx);
    }
    m_blockSignals = false;
}

void SettingsPanel::setSpellEnabled(bool enabled)
{
    m_blockSignals = true;
    m_spellCheck->setChecked(enabled);
    m_langCombo->setEnabled(enabled);
    m_blockSignals = false;
}

void SettingsPanel::setSpellLanguage(const QString& code)
{
    m_blockSignals = true;
    const int idx = m_langCombo->findData(code);
    if (idx >= 0) m_langCombo->setCurrentIndex(idx);
    m_blockSignals = false;
}

bool SettingsPanel::spellEnabled() const
{
    return m_spellCheck->isChecked();
}

QString SettingsPanel::spellLanguage() const
{
    return m_langCombo->currentData().toString();
}

void SettingsPanel::onCheckToggled(bool checked)
{
    m_langCombo->setEnabled(checked);
    if (m_blockSignals) return;
    emit spellEnabledChanged(checked);
}

void SettingsPanel::onLanguageChanged(int /*index*/)
{
    if (m_blockSignals) return;
    emit spellLanguageChanged(spellLanguage());
}

void SettingsPanel::applyTheme()
{
    const QString panelBg    = Theme::panelBackground();
    const QString panelBd    = Theme::panelBorder();
    const QString txtPrim    = Theme::textPrimary();
    const QString txtMuted   = Theme::textMuted();
    const QString txtBright  = Theme::textBright();
    const QString inputBg    = Theme::inputBackground();
    const QString subtleBd   = Theme::subtleBorder();
    const QString hover      = Theme::hoverOverlay();
    const QString hoverStr   = Theme::hoverStrong();
    const QString accent     = Theme::accentDefault();

    QString qss = Theme::qss(QStringLiteral(R"(
        #settingsPanel {
            background-color: %1;
        }
        #settingsScrollArea, #settingsScrollArea > QWidget > QWidget {
            background: transparent;
            border: none;
        }
        #settingsPanel QLabel {
            color: %4;
            font-size: 12px;
        }
        #settingsPanel QLabel#settingsTitle {
            color: %6;
            font-size: 17px;
            font-weight: bold;
        }
        #settingsPanel QComboBox {
            background: %3;
            color: %6;
            border: 1px solid %2;
            border-radius: @radius-control;
            padding: 4px 8px;
            font-size: 12px;
            font-weight: normal;
            min-height: 22px;
        }
        #settingsPanel QComboBox:hover {
            border-color: %9;
        }
        #settingsPanel QLineEdit, #settingsPanel QSpinBox {
            background: %3;
            color: %6;
            border: 1px solid %2;
            border-radius: @radius-control;
            padding: 4px 8px;
            font-size: 12px;
            font-weight: normal;
            min-height: 22px;
        }
        #settingsPanel QLineEdit:focus, #settingsPanel QSpinBox:focus {
            border-color: %9;
        }
        #settingsPanel QComboBox QAbstractItemView {
            background: %1;
            color: %4;
            border: 1px solid %2;
            selection-background-color: %8;
            selection-color: %6;
        }
        #settingsPanel QPushButton {
            background: %8;
            color: %4;
            border: none;
            padding: 6px 16px;
            border-radius: @radius-control;
            font-size: 12px;
        }
        #settingsPanel QPushButton:hover {
            background: %9;
            color: %6;
        }
        #settingsPanel QPushButton:disabled {
            color: %5;
        }
        QPushButton#settingsInfoBtn {
            background: transparent;
            color: %5;
            border: 1px solid %2;
            border-radius: @radius-control;
            font-size: 12px;
            padding: 0;
        }
        QPushButton#settingsInfoBtn:hover {
            color: %6;
            border-color: %9;
        }
        #settingsPanel QSlider::groove:horizontal {
            background: %3;
            height: 4px;
            border-radius: 2px;
        }
        #settingsPanel QSlider::sub-page:horizontal {
            background: %10;
            border-radius: 2px;
        }
        #settingsPanel QSlider::handle:horizontal {
            background: %4;
            width: 12px;
            height: 12px;
            margin: -5px 0;
            border-radius: 6px;
            border: none;
        }
        #settingsPanel QSlider::handle:horizontal:hover {
            background: %6;
        }
        #settingsPanel QLabel#pageValueLabel {
            color: %6;
            font-size: 12px;
        }
        #settingsPanel QLabel:disabled {
            color: %5;
        }
        #settingsPanel QLineEdit:read-only {
            border-color: %7;
        }
    )"))
        .arg(panelBg,   // 1
             panelBd,   // 2
             inputBg,   // 3
             txtPrim,   // 4
             txtMuted,  // 5
             txtBright, // 6
             subtleBd,  // 7
             hover,     // 8
             hoverStr,  // 9
             accent);   // 10

    // Trilho, linhas, segmentados e rodapé.
    qss += Theme::qss(QStringLiteral(R"(
        #settingsPanel #settingsNav {
            background: %1;
            border-right: 1px solid %2;
        }
        #settingsPanel QLineEdit#settingsSearch {
            padding: 5px 6px;
            margin: 0 2px;
        }
        #settingsPanel QPushButton#settingsNavItem {
            text-align: left;
            background: transparent;
            color: %3;
            border: none;
            border-left: 3px solid transparent;
            border-radius: @radius-item;
            padding: 7px 10px;
            font-size: 13px;
        }
        #settingsPanel QPushButton#settingsNavItem:hover {
            background: %4;
            color: %5;
        }
        #settingsPanel QPushButton#settingsNavItem:checked {
            background: %6;
            color: %5;
            border-left: 3px solid %7;
        }
        #settingsPanel QPushButton#settingsNavItem[dim="true"] {
            color: %8;
        }
        #settingsPanel QLabel#settingsNavCount {
            color: %8;
            font-size: 11px;
            background: transparent;
        }
        #settingsPanel QLabel#settingsCatTitle {
            color: %5;
            font-size: 17px;
            font-weight: bold;
            padding-top: 14px;
        }
        #settingsPanel QLabel#settingsCatSub {
            color: %8;
            font-size: 12px;
            padding: 2px 0 6px 0;
        }
        #settingsPanel QLabel#settingsCatSmall {
            color: %8;
            font-size: 10px;
            font-weight: bold;
            letter-spacing: 1px;
            padding-top: 16px;
            padding-bottom: 2px;
            border-bottom: 1px solid %2;
        }
        #settingsPanel #settingsRow {
            background: transparent;
            border-bottom: 1px solid %2;
        }
        #settingsPanel QLabel#settingsRowTitle {
            color: %5;
            font-size: 13px;
        }
        #settingsPanel QLabel#settingsRowTitleSub {
            color: %3;
            font-size: 12px;
        }
        #settingsPanel QLabel#settingsRowDesc {
            color: %8;
            font-size: 11.5px;
        }
        #settingsPanel QPushButton#settingsSegBtn {
            background: transparent;
            color: %3;
            border: 1px solid %9;
            border-left: none;
            border-radius: 0;
            padding: 5px 11px;
            font-size: 12px;
        }
        #settingsPanel QPushButton#settingsSegBtn[pos="first"] {
            border-left: 1px solid %9;
            border-top-left-radius: @radius-control;
            border-bottom-left-radius: @radius-control;
        }
        #settingsPanel QPushButton#settingsSegBtn[pos="last"] {
            border-top-right-radius: @radius-control;
            border-bottom-right-radius: @radius-control;
        }
        #settingsPanel QPushButton#settingsSegBtn[pos="only"] {
            border-left: 1px solid %9;
            border-radius: @radius-control;
        }
        #settingsPanel QPushButton#settingsSegBtn:hover {
            background: %4;
            color: %5;
        }
        #settingsPanel QPushButton#settingsSegBtn:checked {
            background: %6;
            color: %5;
        }
        #settingsPanel QPushButton#settingsLinkBtn {
            background: transparent;
            color: %5;
            padding: 4px 2px;
        }
        #settingsPanel QPushButton#settingsLinkBtn:hover {
            color: %7;
        }
        #settingsPanel #settingsFooter {
            background: %1;
            border-top: 1px solid %2;
        }
        #settingsPanel QPushButton#settingsAboutBtn {
            background: transparent;
            color: %8;
            padding: 4px 6px;
            font-size: 11.5px;
        }
        #settingsPanel QPushButton#settingsAboutBtn:hover {
            color: %5;
        }
    )"))
        .arg(Theme::appBackground(),   // 1
             subtleBd,                 // 2
             txtPrim,                  // 3
             hover,                    // 4
             txtBright,                // 5
             Theme::accentInfoSoft(),  // 6
             accent,                   // 7
             txtMuted,                 // 8
             panelBd);                 // 9
    setStyleSheet(qss);
}

bool SettingsPanel::detectionEnabled() const
{
    return m_detectionCheck ? m_detectionCheck->isChecked() : true;
}

bool SettingsPanel::detectionMarkAll() const
{
    return m_detectionAllCheck ? m_detectionAllCheck->isChecked() : false;
}

void SettingsPanel::setDetectionEnabled(bool enabled)
{
    if (m_detectionCheck) {
        m_detectionCheck->setChecked(enabled);
        if (m_detectionAllCheck) m_detectionAllCheck->setEnabled(enabled);
    }
}

void SettingsPanel::setDetectionMarkAll(bool markAll)
{
    if (m_detectionAllCheck) m_detectionAllCheck->setChecked(markAll);
}

bool SettingsPanel::autoNavEnabled() const
{
    return m_autoNavCheck ? m_autoNavCheck->isChecked() : true;
}

void SettingsPanel::setAutoNavEnabled(bool enabled)
{
    if (m_autoNavCheck) m_autoNavCheck->setChecked(enabled);
}

bool SettingsPanel::unifiedGoalEnabled() const
{
    return m_unifiedGoalCheck ? m_unifiedGoalCheck->isChecked() : false;
}

void SettingsPanel::setUnifiedGoalEnabled(bool enabled)
{
    if (m_unifiedGoalCheck) m_unifiedGoalCheck->setChecked(enabled);
}

bool SettingsPanel::showScenePopupOnHr() const
{
    return m_scenePopupCheck ? m_scenePopupCheck->isChecked() : true;
}

void SettingsPanel::setShowScenePopupOnHr(bool enabled)
{
    if (m_scenePopupCheck) m_scenePopupCheck->setChecked(enabled);
}

bool SettingsPanel::romanChapterNumbers() const
{
    return m_romanNumeralsCheck ? m_romanNumeralsCheck->isChecked() : false;
}

void SettingsPanel::setRomanChapterNumbers(bool enabled)
{
    if (m_romanNumeralsCheck) m_romanNumeralsCheck->setChecked(enabled);
}

int SettingsPanel::maxDocs() const
{
    return m_maxDocsSpinBox ? m_maxDocsSpinBox->value() : 6;
}

void SettingsPanel::setMaxDocs(int n)
{
    if (m_maxDocsSpinBox) m_maxDocsSpinBox->setValue(qBound(1, n, 20));
}

bool SettingsPanel::mentionManuscriptsEnabled() const
{
    return m_mentionManuscriptsCheck ? m_mentionManuscriptsCheck->isChecked() : false;
}

void SettingsPanel::setMentionManuscriptsEnabled(bool enabled)
{
    if (m_mentionManuscriptsCheck) m_mentionManuscriptsCheck->setChecked(enabled);
}
