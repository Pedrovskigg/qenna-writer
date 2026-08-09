#include "AISelectionChat.h"

#include "IconUtils.h"
#include "MiraPersonality.h"
#include "MiraStyleStore.h"
#include "MiraUserMemoryStore.h"
#include "Theme.h"

#include <QCheckBox>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kPopupW = 400;
constexpr int kPopupMinH = 500;
constexpr int kHeaderH = 36;
// Largura de texto disponível numa bolha: kPopupW menos margens da bolha
// (12+12) e do conteúdo do scroll da transcrição (8+8).
constexpr int kBubbleTextWidth = kPopupW - 24 - 16;

struct Preset {
    const char* label;
    const char* instruction;
};

// Pedidos prontos — cada um dispara um envio imediato com essa instrução.
const Preset kPresets[] = {
    { QT_TR_NOOP("Revisão gramatical"),
      QT_TR_NOOP("Faça uma revisão gramatical, ortográfica e de pontuação deste trecho. Aponte os erros encontrados e sugira a correção.") },
    { QT_TR_NOOP("Tom mais formal"),
      QT_TR_NOOP("Reescreva este trecho com um tom mais formal, mantendo o sentido original.") },
    { QT_TR_NOOP("Mais conciso"),
      QT_TR_NOOP("Reescreva este trecho de forma mais concisa, cortando redundâncias sem perder o essencial.") },
    { QT_TR_NOOP("Aumentar tensão"),
      QT_TR_NOOP("Reescreva este trecho aumentando a tensão narrativa.") },
    { QT_TR_NOOP("Revisão aprofundada"),
      QT_TR_NOOP("Faça uma revisão aprofundada deste trecho: narrativa, ritmo, diálogo e voz autoral. Aponte o que funciona, o que pode melhorar, e sugestões concretas.") },
};

QString chipQss() {
    return QStringLiteral(R"(
        QPushButton {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 12px;
            padding: 4px 10px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 11px;
        }
        QPushButton:hover { background: %4; border-color: %5; }
        QPushButton:disabled { color: %6; border-color: %1; }
    )").arg(Theme::inputBackground(), Theme::textPrimary(), Theme::subtleBorder(),
           Theme::hoverOverlay(), Theme::borderStrong(), Theme::disabledText());
}

// Bolha por mensagem (versão simplificada do padrão de AIChatPanel — só
// texto, sem imagem/trace chips, e sem largura calculada a partir do
// painel: aqui a largura é sempre a constante kPopupW).
QString bubbleQss(bool isUser) {
    return isUser
        ? QStringLiteral("QFrame#selectionBubbleUser { background: %1; border-radius: 12px; }")
            .arg(Theme::accentDefault())
        : QStringLiteral("QFrame#selectionBubbleMira { background: %1; border: 1px solid %2; border-radius: 12px; }")
            .arg(Theme::inputBackground(), Theme::subtleBorder());
}

QString bubbleTextColor(bool isUser) {
    return isUser ? Theme::textBright() : Theme::textPrimary();
}

QString bubbleTextQss(const QString& textColorHex) {
    // Mesmo motivo do AIChatPanel: "padding: 0" não é decorativo — sem ele,
    // o QTextEdit { padding: 80px 100px; } global (Theme::globalStyleSheet,
    // escrito pro editor de manuscrito) esmaga o viewport interno da bolha.
    return QStringLiteral(
        "QTextEdit#selectionBubbleText { background: transparent; color: %1; border: none; "
        "padding: 0; font-family: 'Lora','Crimson Text',serif; font-size: 13px; }")
        .arg(textColorHex);
}

void forceBubbleTextColor(QTextEdit* te, const QString& hexColor) {
    QTextCursor cur(te->document());
    cur.select(QTextCursor::Document);
    QTextCharFormat fmt;
    fmt.setForeground(QColor(hexColor));
    cur.mergeCharFormat(fmt);
}

QString feedbackBtnQss() {
    return QStringLiteral(
        "QToolButton { background: transparent; border: none; font-size: 11px; padding: 1px 3px; }"
        "QToolButton:hover { background: %1; border-radius: 4px; }"
        "QToolButton:disabled { background: transparent; }"
    ).arg(Theme::hoverOverlay());
}

// Mesma lógica de AIChatPanel::fitBubbleHeight — trava a largura pro
// conteúdo real (idealWidth) até o teto, senão o texto é desenhado pra
// largura de teto mas o widget fica menor (sizeHint natural do Qt).
void fitSelectionBubbleHeight(QTextEdit* te) {
    te->document()->setTextWidth(kBubbleTextWidth);
    const int idealW = int(te->document()->idealWidth()) + 1;
    te->setFixedWidth(qBound(20, idealW + 4, kBubbleTextWidth));
    te->setFixedHeight(qMax(20, int(te->document()->size().height()) + 6));
}

QString sendBtnQss() {
    return QStringLiteral(R"(
        QPushButton {
            background: %1;
            color: %2;
            border: none;
            border-radius: 6px;
            padding: 6px 16px;
            font-family: 'Lora','Crimson Text',serif;
            font-size: 12px;
            font-weight: 700;
        }
        QPushButton:hover { background: %3; }
        QPushButton:disabled { background: %4; color: %5; }
    )").arg(Theme::accentDefault(), Theme::textBright(), Theme::accentDefault(),
           Theme::subtleBorder(), Theme::disabledText());
}

AITool proposeEditTool() {
    QJsonObject textProp;
    textProp[QStringLiteral("type")] = QStringLiteral("string");
    textProp[QStringLiteral("description")] = QStringLiteral(
        "O texto completo de substituição, pronto para aplicar no lugar exato do trecho selecionado.");

    QJsonObject properties;
    properties[QStringLiteral("text")] = textProp;

    QJsonArray required;
    required.append(QStringLiteral("text"));

    QJsonObject params;
    params[QStringLiteral("type")] = QStringLiteral("object");
    params[QStringLiteral("properties")] = properties;
    params[QStringLiteral("required")] = required;

    AITool tool;
    tool.name = QStringLiteral("propose_edit");
    tool.description = QStringLiteral(
        "Chame esta função quando tiver um texto de substituição concreto e pronto para o "
        "trecho selecionado pelo usuário. Não chame para respostas que são só explicação, "
        "opinião ou conversa — nesse caso, responda normalmente em texto.");
    tool.parameters = params;
    return tool;
}

} // namespace

AISelectionChat::AISelectionChat(QWidget* parent,
                                 QTextEdit* editor,
                                 int selectionStart,
                                 int selectionEnd,
                                 const QString& selectedText,
                                 const QString& charactersContext,
                                 std::function<QString()> sceneTextProvider)
    : QFrame(parent)
    , m_client(new AIClient(this))
    , m_editor(editor)
    , m_selectionStart(selectionStart)
    , m_selectionEnd(selectionEnd)
    , m_selectedText(selectedText)
    , m_charactersContext(charactersContext)
    , m_sceneTextProvider(std::move(sceneTextProvider))
{
    m_client->setTools({ proposeEditTool() });

    setObjectName(QStringLiteral("aiSelectionChat"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFrameShape(QFrame::NoFrame);
    setFixedWidth(kPopupW);
    setMinimumHeight(kPopupMinH);
    setStyleSheet(QStringLiteral(
        "QFrame#aiSelectionChat { background: %1; border: 1px solid %2; border-radius: 8px; }")
        .arg(Theme::panelBackground(), Theme::panelBorder()));

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setColor(QColor(0, 0, 0, 160));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);

    buildUi();

    connect(m_client, &AIClient::tokenReceived, this, [this](const QString& token) {
        appendStreamToken(token);
    });
    connect(m_client, &AIClient::finished, this, [this](const QString& fullText) {
        AIChatMessage assistantMsg;
        assistantMsg.role = QStringLiteral("assistant");
        assistantMsg.content = fullText;
        m_messages.append(assistantMsg);
        attachFeedbackButtons(m_currentAssistantBubble, fullText);
        m_currentAssistantBubble = SelectionBubbleHandle();
        m_assistantTurnOpen = false;
        setBusy(false);
    });
    connect(m_client, &AIClient::errorOccurred, this, [this](const QString& msg) {
        appendTranscriptEntry(tr("Erro"), msg, true);
        setBusy(false);
    });
    connect(m_client, &AIClient::toolCallReceived, this, &AISelectionChat::onToolCallReceived);
}

void AISelectionChat::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 0, 12, 12);
    root->setSpacing(8);

    // Header — drag handle + título + X
    m_header = new QWidget(this);
    m_header->setFixedHeight(kHeaderH);
    m_header->setCursor(Qt::OpenHandCursor);
    auto* hlay = new QHBoxLayout(m_header);
    hlay->setContentsMargins(2, 0, 2, 0);
    hlay->setSpacing(8);
    auto* title = new QLabel(miraAssistantName(), m_header);
    title->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Lora','Crimson Text',serif; font-size: 14px; font-weight: 700;")
        .arg(Theme::textBright()));
    hlay->addWidget(title);
    hlay->addStretch();
    m_closeBtn = new QToolButton(m_header);
    m_closeBtn->setIcon(IconUtils::loadToolbarIcon(QStringLiteral(":/icons/close.svg"),
        QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
        QSize(14, 14)));
    m_closeBtn->setIconSize(QSize(14, 14));
    m_closeBtn->setToolTip(tr("Fechar"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setMinimumSize(24, 24);
    m_closeBtn->setStyleSheet(QStringLiteral(R"(
        QToolButton { background: transparent; border: 1px solid transparent; border-radius: 4px; padding: 2px; }
        QToolButton:hover { background: %1; border-color: %2; }
    )").arg(Theme::hoverOverlay(), Theme::borderStrong()));
    connect(m_closeBtn, &QToolButton::clicked, this, [this]() { emit closeRequested(); });
    hlay->addWidget(m_closeBtn);
    root->addWidget(m_header);

    // Trecho selecionado (quotado, fixo)
    QString preview = m_selectedText;
    preview.replace(QChar('\n'), QChar(' '));
    if (preview.size() > 220) preview = preview.left(220) + QStringLiteral("…");
    auto* quoteLabel = new QLabel(QStringLiteral("“%1”").arg(preview.toHtmlEscaped()), this);
    quoteLabel->setWordWrap(true);
    quoteLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Lora','Crimson Text',serif; font-size: 12px; font-style: italic; "
        "background: %2; border-left: 3px solid %3; border-radius: 4px; padding: 8px 10px;")
        .arg(Theme::textMuted(), Theme::inputBackground(), Theme::accentDefault()));
    root->addWidget(quoteLabel);

    // Transcrição — QScrollArea com uma bolha (QFrame) por mensagem, mesmo
    // espírito do AIChatPanel, mas sem imagem/trace chips (não fazem
    // sentido nesse popup compacto).
    m_transcriptScroll = new QScrollArea(this);
    m_transcriptScroll->setWidgetResizable(true);
    m_transcriptScroll->setFrameShape(QFrame::NoFrame);
    m_transcriptScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_transcriptScroll->setMinimumHeight(180);
    m_transcriptScroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: %1; border: 1px solid %2; border-radius: 6px; }")
        .arg(Theme::inputBackground(), Theme::subtleBorder()));

    m_transcriptContent = new QWidget(m_transcriptScroll);
    m_transcriptContent->setStyleSheet(QStringLiteral("background: transparent;"));
    m_transcriptLayout = new QVBoxLayout(m_transcriptContent);
    m_transcriptLayout->setContentsMargins(8, 8, 8, 8);
    m_transcriptLayout->setSpacing(8);
    m_transcriptLayout->addStretch(1);
    m_transcriptScroll->setWidget(m_transcriptContent);

    connect(m_transcriptScroll->verticalScrollBar(), &QScrollBar::rangeChanged, this,
            [this](int, int max) { m_transcriptScroll->verticalScrollBar()->setValue(max); });

    root->addWidget(m_transcriptScroll, /*stretch=*/1);

    m_statusLabel = new QLabel(tr("Pensando…"), this);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 11px; font-style: italic;").arg(Theme::textMuted()));
    m_statusLabel->setVisible(false);
    root->addWidget(m_statusLabel);

    // Cartão de sugestão pronta (aparece quando a IA chama propose_edit)
    m_suggestionPanel = new QWidget(this);
    m_suggestionPanel->setStyleSheet(QStringLiteral(
        "background: %1; border: 1px solid %2; border-radius: 6px;")
        .arg(Theme::accentInfoSoft(), Theme::accentInfoBorderSoft()));
    auto* suggLay = new QVBoxLayout(m_suggestionPanel);
    suggLay->setContentsMargins(10, 8, 10, 8);
    suggLay->setSpacing(6);

    auto* suggTitle = new QLabel(tr("Sugestão pronta:"), m_suggestionPanel);
    suggTitle->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 11px; font-weight: 700; letter-spacing: 0.4px;")
        .arg(Theme::textMuted()));
    suggLay->addWidget(suggTitle);

    m_suggestionText = new QTextEdit(m_suggestionPanel);
    m_suggestionText->setReadOnly(true);
    m_suggestionText->setMinimumHeight(220);
    m_suggestionText->setMaximumHeight(280);
    m_suggestionText->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_suggestionText->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_suggestionText->setStyleSheet(QStringLiteral(R"(
        QTextEdit {
            background: %1; color: %2; border: 1px solid %3;
            border-radius: 4px; padding: 8px 10px; font-size: 13px;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 9px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: %4;
            border-radius: 4px;
            min-height: 24px;
        }
        QScrollBar::handle:vertical:hover { background: %5; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )").arg(Theme::inputBackground(), Theme::textBright(), Theme::subtleBorder(),
           Theme::textMuted(), Theme::textBright()));
    suggLay->addWidget(m_suggestionText);
    suggLay->addSpacing(4);

    auto* suggBtnRow = new QWidget(m_suggestionPanel);
    auto* suggBtnLay = new QHBoxLayout(suggBtnRow);
    suggBtnLay->setContentsMargins(0, 0, 0, 0);
    suggBtnLay->setSpacing(6);
    suggBtnLay->addStretch();
    m_dismissBtn = new QPushButton(tr("Descartar"), suggBtnRow);
    m_dismissBtn->setCursor(Qt::PointingHandCursor);
    m_dismissBtn->setStyleSheet(chipQss());
    connect(m_dismissBtn, &QPushButton::clicked, this, &AISelectionChat::dismissSuggestion);
    suggBtnLay->addWidget(m_dismissBtn);
    m_applyBtn = new QPushButton(tr("Aplicar ✓"), suggBtnRow);
    m_applyBtn->setCursor(Qt::PointingHandCursor);
    m_applyBtn->setStyleSheet(sendBtnQss());
    connect(m_applyBtn, &QPushButton::clicked, this, &AISelectionChat::applySuggestion);
    suggBtnLay->addWidget(m_applyBtn);
    suggLay->addWidget(suggBtnRow);

    m_suggestionPanel->setVisible(false);
    root->addWidget(m_suggestionPanel);

    // Chips de ação rápida
    auto* chipsRow1 = new QWidget(this);
    auto* c1 = new QHBoxLayout(chipsRow1);
    c1->setContentsMargins(0, 0, 0, 0);
    c1->setSpacing(6);
    auto* chipsRow2 = new QWidget(this);
    auto* c2 = new QHBoxLayout(chipsRow2);
    c2->setContentsMargins(0, 0, 0, 0);
    c2->setSpacing(6);

    const int total = int(sizeof(kPresets) / sizeof(kPresets[0]));
    for (int i = 0; i < total; ++i) {
        const QString label = tr(kPresets[i].label);
        const QString instruction = tr(kPresets[i].instruction);
        auto* btn = new QPushButton(label, this);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(chipQss());
        connect(btn, &QPushButton::clicked, this, [this, label, instruction]() {
            sendUserMessage(label, instruction);
        });
        (i < (total + 1) / 2 ? c1 : c2)->addWidget(btn);
    }
    c1->addStretch();
    c2->addStretch();
    root->addWidget(chipsRow1);
    root->addWidget(chipsRow2);

    // Contexto amplo (checkbox manual — nunca decidido pela IA sozinha)
    if (m_sceneTextProvider) {
        m_fullContextCheck = new QCheckBox(tr("Considerar todo o texto aberto (cena/capítulo)"), this);
        m_fullContextCheck->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px;").arg(Theme::textMuted()));
        root->addWidget(m_fullContextCheck);
    }

    // Campo de pedido livre
    auto* inputRow = new QWidget(this);
    auto* irlay = new QHBoxLayout(inputRow);
    irlay->setContentsMargins(0, 0, 0, 0);
    irlay->setSpacing(6);

    // QTextEdit, não QPlainTextEdit: o motor de layout do QPlainTextEdit
    // (QPlainTextDocumentLayout) não responde de forma confiável a
    // document()->setTextWidth() manual, deixando fitInputHeight() sempre
    // travado numa altura mínima mesmo com texto de várias linhas —
    // QTextEdit usa o motor "normal" (QTextDocumentLayout), já comprovado
    // funcionando em createSelectionBubble/fitBubbleHeight.
    m_inputEdit = new QTextEdit(this);
    m_inputEdit->setObjectName(QStringLiteral("selectionInputEdit"));
    m_inputEdit->setAcceptRichText(false);
    m_inputEdit->setPlaceholderText(tr("Peça algo específico… (Enter envia, Shift+Enter quebra linha)"));
    m_inputEdit->setTabChangesFocus(true);
    m_inputEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_inputEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_inputEdit->setFixedHeight(32);
    // Seletor por objectName (não só "QTextEdit { }"): garante especificidade
    // contra o QTextEdit { padding: 80px 100px; } global (Theme::globalStyleSheet,
    // escrito pro editor de manuscrito) — mesmo remédio já usado em
    // bubbleTextQss() e no CharacterSheetPanel.
    m_inputEdit->setStyleSheet(QStringLiteral(R"(
        QTextEdit#selectionInputEdit {
            background: %1; color: %2; border: 1px solid %3;
            border-radius: 6px; padding: 6px 10px; font-size: 12px;
        }
        QTextEdit#selectionInputEdit:focus { border-color: %4; }
    )").arg(Theme::inputBackground(), Theme::textBright(), Theme::subtleBorder(), Theme::focusBorder()));
    m_inputEdit->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    m_inputEdit->installEventFilter(this);
    connect(m_inputEdit, &QTextEdit::textChanged, this, [this]() { fitInputHeight(); });
    irlay->addWidget(m_inputEdit, 1);

    m_sendBtn = new QPushButton(tr("Enviar"), this);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setStyleSheet(sendBtnQss());
    connect(m_sendBtn, &QPushButton::clicked, this, [this]() {
        const QString text = m_inputEdit->toPlainText().trimmed();
        if (text.isEmpty()) return;
        sendUserMessage(text, text);
    });
    irlay->addWidget(m_sendBtn);

    root->addWidget(inputRow);
}

AISelectionChat::SelectionBubbleHandle AISelectionChat::createSelectionBubble(bool isUser, const QString& initialText)
{
    auto* row = new QWidget(m_transcriptContent);
    auto* rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(0);

    auto* bubble = new QFrame(row);
    bubble->setObjectName(isUser ? QStringLiteral("selectionBubbleUser") : QStringLiteral("selectionBubbleMira"));
    bubble->setMaximumWidth(kBubbleTextWidth + 24);
    bubble->setStyleSheet(bubbleQss(isUser));

    auto* bubbleLay = new QVBoxLayout(bubble);
    bubbleLay->setContentsMargins(10, 8, 10, 8);
    bubbleLay->setSpacing(4);

    auto* te = new QTextEdit(bubble);
    te->setObjectName(QStringLiteral("selectionBubbleText"));
    te->setReadOnly(true);
    te->setFrameShape(QFrame::NoFrame);
    te->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    te->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    te->setTextInteractionFlags(Qt::TextSelectableByMouse);
    te->document()->setDocumentMargin(0);
    const QString textColorHex = bubbleTextColor(isUser);
    te->setStyleSheet(bubbleTextQss(textColorHex));
    // Mesmo fix de viewport do AIChatPanel — o QSS setado no QTextEdit não
    // propaga pro viewport() interno (QAbstractScrollArea).
    te->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    te->setMarkdown(initialText);
    forceBubbleTextColor(te, textColorHex);
    fitSelectionBubbleHeight(te);
    bubbleLay->addWidget(te);

    if (isUser) {
        rowLay->addStretch();
        rowLay->addWidget(bubble);
    } else {
        rowLay->addWidget(bubble);
        rowLay->addStretch();
    }

    // Insere sempre antes do addStretch(1) final, que fica fixo no fim.
    m_transcriptLayout->insertWidget(m_transcriptLayout->count() - 1, row);

    SelectionBubbleHandle h;
    h.bubble = bubble;
    h.textEdit = te;
    h.bubbleLayout = bubbleLay;
    return h;
}

void AISelectionChat::attachFeedbackButtons(SelectionBubbleHandle& handle, const QString& fullText)
{
    if (!handle.bubble || fullText.trimmed().isEmpty()) return;

    auto* row = new QWidget(handle.bubble);
    auto* rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 2, 0, 0);
    rowLay->setSpacing(2);
    rowLay->addStretch();

    auto* likeBtn = new QToolButton(row);
    likeBtn->setObjectName(QStringLiteral("chatFeedbackBtn"));
    likeBtn->setText(QStringLiteral("👍"));
    likeBtn->setCursor(Qt::PointingHandCursor);
    likeBtn->setToolTip(tr("Bom estilo de resposta"));
    likeBtn->setStyleSheet(feedbackBtnQss());

    auto* dislikeBtn = new QToolButton(row);
    dislikeBtn->setObjectName(QStringLiteral("chatFeedbackBtn"));
    dislikeBtn->setText(QStringLiteral("👎"));
    dislikeBtn->setCursor(Qt::PointingHandCursor);
    dislikeBtn->setToolTip(tr("Não gostei desse estilo"));
    dislikeBtn->setStyleSheet(feedbackBtnQss());

    auto vote = [likeBtn, dislikeBtn, fullText](bool positive) {
        MiraStyleStore::recordFeedback(positive, fullText);
        likeBtn->setEnabled(false);
        dislikeBtn->setEnabled(false);
    };
    connect(likeBtn, &QToolButton::clicked, this, [vote]() { vote(true); });
    connect(dislikeBtn, &QToolButton::clicked, this, [vote]() { vote(false); });

    rowLay->addWidget(likeBtn);
    rowLay->addWidget(dislikeBtn);
    handle.bubbleLayout->addWidget(row);
}

QString AISelectionChat::buildSystemPrompt() const
{
    QString base = miraPersonalityPrompt(miraAssistantName());
    base += miraPersonalityAdjustmentFragment(
        QSettings().value(QStringLiteral("ai/personalityWarmth"), 50).toInt(),
        QSettings().value(QStringLiteral("ai/personalityHarshness"), 50).toInt(),
        QSettings().value(QStringLiteral("ai/personalityFreeform")).toString());
    base += miraTraitsFragment(QSettings().value(QStringLiteral("ai/personalityTraits")).toStringList());
    base += miraCompanionshipFragment();
    base += MiraStyleStore::buildPromptFragment();

    const QString userMemory = MiraUserMemoryStore::load();
    if (!userMemory.isEmpty()) {
        base += QStringLiteral(
            "\n\nMemória sobre o autor (vale em qualquer projeto):\n\n%1")
            .arg(userMemory);
    }

    base += QStringLiteral(
        "\n\nAqui você está revisando um trecho ESPECÍFICO que o autor "
        "selecionou no editor, não numa conversa livre sobre o projeto "
        "inteiro. Responda sempre no idioma em que o usuário escreve. Sem "
        "preâmbulo, sem se apresentar, sem repetir o pedido antes de "
        "responder.");

    QString ctx = QStringLiteral("\n\nTrecho selecionado pelo usuário:\n\"\"\"\n%1\n\"\"\"")
        .arg(m_selectedText);
    if (!m_charactersContext.isEmpty()) {
        ctx += QStringLiteral("\n\nPersonagens presentes nesta cena: %1").arg(m_charactersContext);
    }
    return base + ctx;
}

void AISelectionChat::sendUserMessage(const QString& displayText, const QString& payloadText)
{
    if (payloadText.trimmed().isEmpty()) return;
    if (m_client->isBusy()) return;

    QSettings settings;
    const QString apiKey = settings.value(QStringLiteral("ai/apiKey")).toString();
    if (apiKey.isEmpty()) {
        appendTranscriptEntry(miraAssistantName(),
            tr("Nenhuma chave de API configurada. Abra Configurações → Assistente de IA e cole sua chave."),
            true);
        return;
    }
    m_client->setApiKey(apiKey);
    m_client->setBaseUrl(settings.value(QStringLiteral("ai/baseUrl"),
        QStringLiteral("https://api.openai.com/v1")).toString());
    m_client->setModel(settings.value(QStringLiteral("ai/model"),
        QStringLiteral("gpt-4o-mini")).toString());

    if (m_messages.isEmpty()) {
        AIChatMessage sys;
        sys.role = QStringLiteral("system");
        sys.content = buildSystemPrompt();
        m_messages.append(sys);
    }

    QString payload = payloadText;
    if (m_fullContextCheck && m_fullContextCheck->isChecked() && m_sceneTextProvider) {
        const QString full = m_sceneTextProvider();
        if (!full.isEmpty()) {
            payload += QStringLiteral(
                "\n\n[Contexto adicional — texto completo do documento aberto; "
                "o trecho selecionado está incluído dentro dele]:\n\"\"\"\n%1\n\"\"\"").arg(full);
        }
    }

    AIChatMessage userMsg;
    userMsg.role = QStringLiteral("user");
    userMsg.content = payload;
    m_messages.append(userMsg);

    appendTranscriptEntry(tr("Você"), displayText, false);
    m_inputEdit->clear();

    setBusy(true);
    m_assistantTurnOpen = false;
    m_client->sendMessage(m_messages);
}

void AISelectionChat::setBusy(bool busy)
{
    m_sendBtn->setEnabled(!busy);
    m_inputEdit->setEnabled(!busy);
    m_statusLabel->setVisible(busy);
}

void AISelectionChat::appendTranscriptEntry(const QString& speakerLabel, const QString& text, bool isAssistant)
{
    createSelectionBubble(/*isUser=*/!isAssistant,
        QStringLiteral("**%1:**\n\n%2").arg(speakerLabel, text));
}

void AISelectionChat::beginAssistantStreamEntry()
{
    m_currentAssistantBubble = createSelectionBubble(/*isUser=*/false, QString());
    m_streamingText.clear();
    m_assistantTurnOpen = true;
}

void AISelectionChat::appendStreamToken(const QString& token)
{
    if (!m_assistantTurnOpen) beginAssistantStreamEntry();
    m_streamingText += token;

    QTextEdit* te = m_currentAssistantBubble.textEdit;
    te->setMarkdown(QStringLiteral("**%1:**\n\n%2").arg(miraAssistantName(), m_streamingText));
    forceBubbleTextColor(te, bubbleTextColor(false));
    fitSelectionBubbleHeight(te);
}

void AISelectionChat::onToolCallReceived(const QString& id, const QString& name, const QJsonObject& arguments)
{
    Q_UNUSED(id); // fire-and-forget aqui — não precisa do round-trip completo, ver comentário na classe
    if (name != QStringLiteral("propose_edit")) return;

    const QString text = arguments.value(QStringLiteral("text")).toString();
    if (text.isEmpty()) return;

    appendTranscriptEntry(miraAssistantName(), tr("(sugestão de texto pronta — veja o cartão abaixo)"), true);

    // Guarda como turno assistant simples pra manter o histórico coerente em
    // futuras mensagens, sem precisar replicar o protocolo formal de tool
    // calls/tool role da API (não é necessário aqui — o "resultado" da tool
    // é só a exibição pro usuário, não uma resposta que volta pro modelo).
    AIChatMessage assistantMsg;
    assistantMsg.role = QStringLiteral("assistant");
    assistantMsg.content = QStringLiteral("[Sugestão de texto proposta]: %1").arg(text);
    m_messages.append(assistantMsg);

    showSuggestion(text);
}

void AISelectionChat::showSuggestion(const QString& text)
{
    m_pendingSuggestion = text;
    m_suggestionText->setPlainText(text);
    m_suggestionPanel->setVisible(true);
    // A janela não é gerenciada por um layout externo (é posicionada via
    // move() direto), então mostrar um widget novo não a redimensiona
    // sozinha — sem isso, o cartão de sugestão fica espremido/sobreposto
    // no espaço antigo, menor do que o conteúdo precisa.
    adjustSize();
}

void AISelectionChat::applySuggestion()
{
    if (!m_editor || m_pendingSuggestion.isEmpty()) return;

    QTextCursor cur(m_editor->document());
    cur.setPosition(m_selectionStart);
    cur.setPosition(m_selectionEnd, QTextCursor::KeepAnchor);
    cur.insertText(m_pendingSuggestion);

    // Range seguinte passa a ser o texto recém-aplicado, pra permitir pedir
    // outro ajuste em cima do resultado sem reabrir o chat.
    m_selectionEnd = m_selectionStart + m_pendingSuggestion.size();

    appendTranscriptEntry(tr("Você"), tr("(sugestão aplicada no editor)"), false);
    dismissSuggestion();
}

void AISelectionChat::dismissSuggestion()
{
    m_pendingSuggestion.clear();
    m_suggestionPanel->setVisible(false);
    adjustSize();
}

void AISelectionChat::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && m_header && m_header->geometry().contains(e->pos())) {
        m_dragging = true;
        m_dragOffset = e->pos();
        m_header->setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    QFrame::mousePressEvent(e);
}

void AISelectionChat::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging) {
        const QPoint delta = e->pos() - m_dragOffset;
        move(pos() + delta);
        e->accept();
        return;
    }
    QFrame::mouseMoveEvent(e);
}

void AISelectionChat::mouseReleaseEvent(QMouseEvent* e)
{
    if (m_dragging) {
        m_dragging = false;
        if (m_header) m_header->setCursor(Qt::OpenHandCursor);
        e->accept();
        return;
    }
    QFrame::mouseReleaseEvent(e);
}

bool AISelectionChat::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_inputEdit && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) &&
            !(ke->modifiers() & Qt::ShiftModifier)) {
            const QString text = m_inputEdit->toPlainText().trimmed();
            if (!text.isEmpty()) sendUserMessage(text, text);
            return true; // consome — Shift+Enter continua inserindo quebra de linha normalmente
        }
    } else if (watched == m_inputEdit && event->type() == QEvent::Resize) {
        fitInputHeight();
    }
    return QFrame::eventFilter(watched, event);
}

void AISelectionChat::fitInputHeight()
{
    if (!m_inputEdit) return;
    m_inputEdit->document()->setTextWidth(m_inputEdit->viewport()->width());
    const int h = int(m_inputEdit->document()->size().height()) + 14;
    m_inputEdit->setFixedHeight(qBound(32, h, 120));
}
