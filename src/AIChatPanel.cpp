#include "AIChatPanel.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "AvatarUtils.h"
#include "CharacterImageGenService.h"
#include "ClickableImageLabel.h"
#include "ConstrutorStore.h"
#include "DialogueChemistry.h"
#include "DialogueStore.h"
#include "DocCache.h"
#include "DocPreview.h"
#include "ElementsStore.h"
#include "GeneratedImageGallery.h"
#include "GeoData.h"
#include "GlossaryStore.h"
#include "IconUtils.h"
#include "ImageCropDialog.h"
#include "ImageGalleryDialog.h"
#include "MapPinsStore.h"
#include "MarkerStore.h"
#include "MiraPersonality.h"
#include "MiraPersonalityDialog.h"
#include "MiraStyleStore.h"
#include "MiraUserMemoryStore.h"
#include "NotesStore.h"
#include "ProjectModel.h"
#include "ProjectStorage.h"
#include "Theme.h"
#include "WordCounter.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QShowEvent>
#include <QStringConverter>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFormat>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

namespace {
constexpr int kPanelWidth = 420;
constexpr int kMargin = 12;
constexpr int kHeaderH = 50; // acomoda o bloco nome+subtítulo do modo janela
// Teto de segurança contra caso patológico (ex. um "documento" que na
// verdade tem um romance inteiro colado dentro) — não é um limite prático
// real: gpt-4o-mini tem janela de 128k tokens (~500k caracteres), então até
// o capítulo mais longo de um projeto normal cabe folgado. Cortar cedo
// demais aqui faz a IA nunca ver o final de capítulos longos, mesmo pedindo
// resumo mais longo depois — o gargalo é na entrada, não na saída.
constexpr int kMaxCharsPerDoc = 100000;

QString chipQss() {
    return QStringLiteral(R"(
        QPushButton {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 6px 12px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 12px;
        }
        QPushButton:hover { background: %4; border-color: %5; }
        QPushButton:disabled { color: %6; border-color: %1; }
    )").arg(Theme::inputBackground(), Theme::textPrimary(), Theme::subtleBorder(),
           Theme::hoverOverlay(), Theme::borderStrong(), Theme::disabledText());
}

QString sendBtnQss() {
    return QStringLiteral(R"(
        QPushButton {
            background: %1;
            color: %2;
            border: none;
            border-radius: 8px;
            padding: 8px 18px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 12px;
            font-weight: 700;
        }
        QPushButton:hover { background: %3; }
        QPushButton:disabled { background: %4; color: %5; }
    )").arg(Theme::accentDefault(), Theme::textBright(), Theme::accentDefault(),
           Theme::subtleBorder(), Theme::disabledText());
}

// ---- Stylesheets derivadas do tema ----
// Todas leem Theme::xxx() na hora da chamada, então servem pra montar a UI no
// construtor E pra remontá-la em applyTheme() quando o tema troca ao vivo.
// Nada de QSS inline solto em buildUi(): o que não estiver aqui não é
// reaplicado na troca de tema e fica congelado nas cores antigas.

QString panelFrameQss() {
    return QStringLiteral(
        "QFrame#aiChatPanel { background: %1; border: 1px solid %2; border-radius: 10px; }")
        .arg(Theme::panelBackground(), Theme::panelBorder());
}

QString titleQss() {
    return QStringLiteral(
        "color: %1; font-family: 'Segoe UI', sans-serif; font-size: 14px; font-weight: 700;")
        .arg(Theme::textBright());
}

QString headerBtnQss() {
    return QStringLiteral(R"(
        QToolButton { background: transparent; border: 1px solid transparent; border-radius: 4px; padding: 2px; font-size: 12px; }
        QToolButton:hover { background: %1; border-color: %2; }
        QToolButton:checked { background: %3; border-color: %3; }
    )").arg(Theme::hoverOverlay(), Theme::borderStrong(), Theme::accentDefault());
}

QString closeBtnQss() {
    return QStringLiteral(R"(
        QToolButton { background: transparent; border: 1px solid transparent; border-radius: 4px; padding: 2px; }
        QToolButton:hover { background: %1; border-color: %2; }
    )").arg(Theme::hoverOverlay(), Theme::borderStrong());
}

QString smallComboQss() {
    return QStringLiteral(R"(
        QComboBox {
            background: %1; color: %2; border: 1px solid %3;
            border-radius: 4px; padding: 2px 6px; font-size: 10px;
        }
        QComboBox:hover { border-color: %4; }
        QComboBox:disabled { color: %5; }
    )").arg(Theme::inputBackground(), Theme::textMuted(), Theme::subtleBorder(),
           Theme::borderStrong(), Theme::disabledText());
}

QString transcriptScrollQss() {
    return QStringLiteral(
        "QScrollArea { background: %1; border: 1px solid %2; border-radius: 10px; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
    ).arg(Theme::inputBackground(), Theme::subtleBorder());
}

QString statusLabelQss() {
    return QStringLiteral("color: %1; font-size: 11px; font-style: italic;")
        .arg(Theme::textMuted());
}

QString docFocusCheckQss() {
    return QStringLiteral("color: %1; font-size: 11px; font-family: 'Segoe UI', sans-serif;")
        .arg(Theme::textMuted());
}

QString inputEditQss() {
    // Seletor por objectName (não só "QTextEdit { }"): garante especificidade
    // contra o QTextEdit { padding: 80px 100px; } global (Theme::globalStyleSheet,
    // escrito pro editor de manuscrito) — mesmo remédio já usado em
    // bubbleTextQss() e no CharacterSheetPanel.
    return QStringLiteral(R"(
        QTextEdit#chatInputEdit {
            background: %1; color: %2; border: 1px solid %3;
            border-radius: 8px; padding: 8px 10px; font-size: 12px;
            font-family: 'Segoe UI', sans-serif;
        }
        QTextEdit#chatInputEdit:focus { border-color: %4; }
    )").arg(Theme::inputBackground(), Theme::textBright(), Theme::subtleBorder(), Theme::focusBorder());
}

QString previewBannerQss() {
    return QStringLiteral(
        "background: %1; border: 1px solid %2; border-radius: 8px; color: %3; font-size: 11.5px;"
    ).arg(Theme::hoverOverlay(), Theme::accentDefault(), Theme::textBright());
}

QString railFolderBtnQss(bool active) {
    return QStringLiteral(
        "QToolButton { text-align: left; padding: 6px 8px; border-radius: 6px; "
        "border: none; background: %1; color: %2; font-size: 12px; font-weight: 600; } "
        "QToolButton:hover { background: %3; }"
    ).arg(active ? Theme::hoverOverlay() : QStringLiteral("transparent"),
          Theme::textBright(), Theme::hoverOverlay());
}

QString railSessionListQss() {
    return QStringLiteral(
        "QListWidget { background: transparent; border: none; font-size: 11.5px; color: %1; } "
        "QListWidget::item { padding: 4px 8px 4px 20px; border-radius: 6px; } "
        "QListWidget::item:hover { background: %2; } "
        "QListWidget::item:selected { background: %3; color: %4; }"
    ).arg(Theme::textMuted(), Theme::hoverOverlay(), Theme::hoverOverlay(), Theme::textBright());
}

QString avatarQss() {
    return QStringLiteral(
        "background: %1; color: %2; border-radius: 8px; font-size: 13px; font-weight: 700;"
    ).arg(Theme::hoverOverlay(), Theme::accentDefault());
}

QString studioNameQss() {
    return QStringLiteral("color: %1; font-size: 13.5px; font-weight: 700;").arg(Theme::textBright());
}

QString studioSubtitleQss() {
    return QStringLiteral("color: %1; font-size: 10.5px;").arg(Theme::textMuted());
}

QString memoryChipQss() {
    return QStringLiteral(
        "color: %1; font-size: 10px; background: %2; border: 1px solid %3; "
        "border-radius: 999px; padding: 3px 9px;"
    ).arg(Theme::textMuted(), Theme::panelBackground(), Theme::subtleBorder());
}

QString chatContextQss() {
    return QStringLiteral("color: %1; font-size: 10.5px; padding: 2px 2px;").arg(Theme::textMuted());
}

// Mesmo remédio de transcriptScrollQss(): sem isso, o viewport/conteúdo
// interno do QScrollArea usa o background padrão da plataforma (cinza claro
// do Windows) em vez do tema escuro do resto do painel — bug já visto antes
// nesse mesmo arquivo.
QString railScrollQss() {
    return QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
    );
}

QString railSearchQss() {
    return QStringLiteral(R"(
        QLineEdit {
            background: %1; color: %2; border: 1px solid %3;
            border-radius: 7px; padding: 5px 8px; font-size: 11px;
        }
        QLineEdit:focus { border-color: %4; }
    )").arg(Theme::inputBackground(), Theme::textPrimary(), Theme::subtleBorder(), Theme::focusBorder());
}

QString gripQss() {
    return QStringLiteral(
        "background: transparent; border-bottom: 3px solid %1; border-right: 3px solid %1; border-bottom-right-radius: 8px;"
    ).arg(Theme::subtleBorder());
}

QString bubbleQss(bool isUser) {
    return isUser
        ? QStringLiteral("QFrame#chatBubbleUser { background: %1; border-radius: 14px; }")
            .arg(Theme::accentDefault())
        : QStringLiteral("QFrame#chatBubbleMira { background: %1; border: 1px solid %2; border-radius: 14px; }")
            .arg(Theme::panelBackground(), Theme::subtleBorder());
}

// Cor do texto da bolha: sobre o acento (usuário) usa textBright; sobre o
// fundo de painel (Mira) usa textPrimary.
QString bubbleTextColor(bool isUser) {
    return isUser ? Theme::textBright() : Theme::textPrimary();
}

// Espaçamento vertical entre blocos da bolha — pedido do usuário
// (2026-08-15) pra dar mais respiro visual e separar tópicos, já que a
// resposta da Mira tende a ter várias seções numa mensagem só. Primeira
// tentativa foi via QTextDocument::setDefaultStyleSheet() (CSS de p/h1-h6/
// ul/li), mas não teve efeito nenhum: QTextEdit::setMarkdown() constrói o
// documento direto por QTextBlockFormat/QTextCharFormat (ver
// qtextmarkdownimporter no Qt), sem passar pelo pipeline HTML+CSS — a folha
// de estilo padrão só vale pra setHtml(). Único jeito real de controlar
// margem de bloco no resultado do setMarkdown() é aplicar QTextBlockFormat
// depois, bloco a bloco. Chamar sempre logo após setMarkdown() (bolha nova
// E cada token de streaming, já que streaming rechama setMarkdown() no
// texto acumulado inteiro a cada token).
void applyBubbleBlockSpacing(QTextEdit* te) {
    QTextBlock block = te->document()->begin();
    while (block.isValid()) {
        QTextBlockFormat fmt = block.blockFormat();
        const bool isHeading = fmt.headingLevel() > 0;
        const bool isListItem = block.textList() != nullptr;
        fmt.setTopMargin(isHeading ? 10 : 0);
        fmt.setBottomMargin(isListItem ? 3 : 8);
        QTextCursor cur(block);
        cur.mergeBlockFormat(fmt);
        block = block.next();
    }
}

QString bubbleTextQss(const QString& textColorHex) {
    // "padding: 0" NÃO é decorativo — é o que faz o texto da bolha existir na
    // tela. O stylesheet GLOBAL do app (Theme::globalStyleSheet, aplicado com
    // qApp->setStyleSheet em main.cpp) tem uma regra `QTextEdit { padding:
    // 80px 100px; }` escrita pro editor de manuscrito, e ela vale pra TODO
    // QTextEdit do app. Em QAbstractScrollArea o padding do QSS encolhe o
    // viewport: numa bolha de ~360x24 o viewport virava 162x0 (altura
    // negativa, clampada em zero), então nenhum glifo era pintado e nada era
    // selecionável — mesmo com o documento cheio, geometria correta e a
    // cadeia toda visível. O fundo continuava certo porque ele é pintado no
    // rect inteiro do widget, não no viewport; daí o sintoma "bolha colorida
    // e vazia". A regra local só sobrescreve as propriedades que declara (a
    // cascata do QSS é por propriedade), então o padding do sheet global
    // vazava justamente por não estar declarado aqui. Mesmo remédio já usado
    // no CharacterSheetPanel (QTextEdit#sheetText { ... padding: 0; }); o
    // seletor por objectName garante a especificidade contra o sheet global.
    return QStringLiteral(
        "QTextEdit#chatBubbleText { padding: 0; background: transparent; border: none; color: %1;"
        " font-family: 'Segoe UI', sans-serif; font-size: 13px; }")
        .arg(textColorHex);
}

// QSS do header "Pensando…" da bolha de pensamento — cor passada pronta
// (com alpha já calculado pelo timer de pulso) em vez de fixa, já que ela
// muda a cada tick. Ver AIChatPanel::showThinkingBubble().
QString thinkingToggleQss(const QColor& color) {
    return QStringLiteral(
        "QToolButton { background: transparent; border: none; text-align: left; padding: 0;"
        " font-family: 'Segoe UI', sans-serif; font-size: 13px; color: rgba(%1,%2,%3,%4); }")
        .arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
}

QString traceToggleQss() {
    return QStringLiteral(
        "QToolButton { background: transparent; border: none; color: %1; font-size: 10px; padding: 2px 0; text-align: left; font-family: 'Segoe UI', sans-serif; }"
        "QToolButton:hover { color: %2; }"
    ).arg(Theme::textMuted(), Theme::textBright());
}

QString traceTextQss() {
    return QStringLiteral(
        "color: %1; background: transparent; font-size: 10px; font-style: italic; font-family: 'Segoe UI', sans-serif;")
        .arg(Theme::textMuted());
}

QString traceChipQss() {
    return QStringLiteral(R"(
        QToolButton {
            background: %1; color: %2; border: 1px solid %3;
            border-radius: 9px; padding: 1px 7px; font-size: 9px;
            font-style: normal; font-family: 'Segoe UI', sans-serif;
        }
        QToolButton:hover { background: %4; color: %5; border-color: %5; }
    )").arg(Theme::inputBackground(), Theme::textMuted(), Theme::subtleBorder(),
           Theme::hoverOverlay(), Theme::textBright());
}

QString feedbackBtnQss() {
    return QStringLiteral(
        "QToolButton { background: transparent; border: none; font-size: 11px; padding: 1px 3px; }"
        "QToolButton:hover { background: %1; border-radius: 4px; }"
        "QToolButton:disabled { background: transparent; }"
    ).arg(Theme::hoverOverlay());
}

// Cartão de sugestão de edição (propose_document_edit) — mesma paleta
// "info" já usada no cartão de sugestão do AISelectionChat, só que embutido
// dentro da bolha da Mira em vez de num painel fixo, porque aqui pode haver
// mais de uma sugestão pendente na mesma conversa longa.
QString editCardQss() {
    return QStringLiteral(
        "QFrame#chatEditCard { background: %1; border: 1px solid %2; border-radius: 6px; }")
        .arg(Theme::accentInfoSoft(), Theme::accentInfoBorderSoft());
}

QString editCardTitleQss() {
    return QStringLiteral(
        "color: %1; font-size: 11px; font-weight: 700; letter-spacing: 0.4px; background: transparent;")
        .arg(Theme::textMuted());
}

QString editCardTextQss() {
    return QStringLiteral(
        "color: %1; font-size: 12px; background: transparent;").arg(Theme::textPrimary());
}

QString editCardStatusQss() {
    return QStringLiteral(
        "color: %1; font-size: 11px; font-style: italic; background: transparent;").arg(Theme::textMuted());
}

// Ícone do X do header é gerado com cores do tema (não é QSS), então precisa
// ser recriado na troca de tema — mesmo cuidado do SelectionPopup.
QIcon closeBtnIcon() {
    return IconUtils::loadToolbarIcon(QStringLiteral(":/icons/close.svg"),
        QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()),
        QSize(14, 14));
}

// Mesmo padrão de closeBtnIcon() (cores do tema, precisa recriar na troca de
// tema) pros demais ícones de ação do painel — antes eram emoji (🕐 ＋ 📚
// 🖼️ ⛶ 📎), trocados por SVG de verdade reaproveitando ícones já existentes
// no app (mesma família Material usada em MainMenuDialog/TopToolbar).
QIcon toolIcon(const QString& resourcePath, const QSize& size = QSize(16, 16)) {
    return IconUtils::loadToolbarIcon(resourcePath,
        QColor(Theme::textMuted()), QColor(Theme::textBright()), QColor(Theme::textBright()), size);
}

QString stripHtmlToPlainText(const QString& html) {
    QTextDocument doc;
    doc.setHtml(html);
    return doc.toPlainText().trimmed();
}

// Fixa a cor no QTextCharFormat do documento inteiro. O `color` do QSS já
// colore o conteúdo do QTextEdit (vira QPalette::Text), mas isso vale só
// enquanto o texto não trouxer cor própria: conteúdo com cor inline (HTML
// herdado de um doc do projeto, colado numa bolha) ganharia do QSS e podia
// sair ilegível sobre a cor da bolha — mesma classe de problema que o
// stripForegroundColors() do DocPreview resolve no preview de ficha.
// OBS.: isto NÃO tem nada a ver com o bug antigo de "bolha colorida e sem
// texto" — esse era o padding do stylesheet global zerando o viewport, ver
// o comentário longo em createBubbleRow().
void forceBubbleTextColor(QTextEdit* te, const QString& hexColor) {
    QTextCursor cur(te->document());
    cur.select(QTextCursor::Document);
    QTextCharFormat fmt;
    fmt.setForeground(QColor(hexColor));
    cur.mergeCharFormat(fmt);
}

// Serialização de AIChatMessage pra persistir sessões inteiras em disco
// (não só o log de texto) — precisa dos tool_calls/tool_call_id brutos pra
// uma conversa recarregada continuar sem quebrar o protocolo da API.
QJsonObject chatMessageToJson(const AIChatMessage& m) {
    QJsonObject o;
    o[QStringLiteral("role")] = m.role;
    o[QStringLiteral("content")] = m.content;
    if (!m.toolCalls.isEmpty()) {
        QJsonArray tc;
        for (const AIToolCall& t : m.toolCalls) {
            QJsonObject to;
            to[QStringLiteral("id")] = t.id;
            to[QStringLiteral("name")] = t.name;
            to[QStringLiteral("argumentsJson")] = t.argumentsJson;
            tc.append(to);
        }
        o[QStringLiteral("toolCalls")] = tc;
    }
    if (!m.toolCallId.isEmpty()) o[QStringLiteral("toolCallId")] = m.toolCallId;
    if (!m.imageDataUrl.isEmpty()) o[QStringLiteral("imageDataUrl")] = m.imageDataUrl;
    return o;
}

AIChatMessage chatMessageFromJson(const QJsonObject& o) {
    AIChatMessage m;
    m.role = o.value(QStringLiteral("role")).toString();
    m.content = o.value(QStringLiteral("content")).toString();
    m.toolCallId = o.value(QStringLiteral("toolCallId")).toString();
    m.imageDataUrl = o.value(QStringLiteral("imageDataUrl")).toString();
    for (const QJsonValue& v : o.value(QStringLiteral("toolCalls")).toArray()) {
        const QJsonObject to = v.toObject();
        AIToolCall t;
        t.id = to.value(QStringLiteral("id")).toString();
        t.name = to.value(QStringLiteral("name")).toString();
        t.argumentsJson = to.value(QStringLiteral("argumentsJson")).toString();
        m.toolCalls.append(t);
    }
    return m;
}

void flattenConstrutorNodes(const QList<ConstrutorStore::Node>& nodes, QStringList& out, int depth = 0) {
    for (const ConstrutorStore::Node& n : nodes) {
        const QString typeLabel = n.type == ConstrutorStore::NodeType::Rule
            ? QStringLiteral("Regra") : QStringLiteral("Seção");
        out.append(QStringLiteral("%1[%2] %3: %4")
            .arg(QString(depth * 2, QLatin1Char(' ')), typeLabel, n.name, n.content));
        flattenConstrutorNodes(n.children, out, depth + 1);
    }
}

AITool searchProjectTool() {
    QJsonObject queryProp;
    queryProp[QStringLiteral("type")] = QStringLiteral("string");
    queryProp[QStringLiteral("description")] = QStringLiteral(
        "Termo ou trecho a buscar no texto bruto do projeto (nome próprio, "
        "palavra-chave, frase curta).");

    QJsonObject properties;
    properties[QStringLiteral("query")] = queryProp;

    QJsonArray required;
    required.append(QStringLiteral("query"));

    QJsonObject params;
    params[QStringLiteral("type")] = QStringLiteral("object");
    params[QStringLiteral("properties")] = properties;
    params[QStringLiteral("required")] = required;

    AITool tool;
    tool.name = QStringLiteral("search_project");
    tool.description = QStringLiteral(
        "Busca no texto bruto (não resumido) de todos os capítulos e fichas "
        "do projeto. A busca é por PALAVRA (OR, não frase literal exata) — "
        "\"carro Klara\" retorna trechos com \"carro\" OU \"Klara\" "
        "separadamente, não exige as duas juntas. Use quando o resumo do "
        "projeto (contexto de base) não tiver o detalhe exato que você "
        "precisa. Estratégia recomendada: comece com um termo genérico "
        "ligado ao assunto (ex.: nome do personagem + palavra do tema — "
        "\"carro\", \"arma\", \"gravado\"); se os trechos retornados "
        "mencionarem um termo mais específico (uma marca, um nome exato), "
        "chame a função de novo só com esse termo pra confirmar o detalhe "
        "exato. Retorna trechos do texto original onde cada termo aparece, "
        "com a origem de cada trecho.");
    tool.parameters = params;
    return tool;
}

AITool readDocumentTool() {
    QJsonObject titleProp;
    titleProp[QStringLiteral("type")] = QStringLiteral("string");
    titleProp[QStringLiteral("description")] = QStringLiteral(
        "Título (ou parte dele) do capítulo/ficha a ler por inteiro — use o "
        "título exatamente como aparece entre colchetes nos resultados de "
        "search_project, ou como está no resumo do projeto.");

    QJsonObject questionProp;
    questionProp[QStringLiteral("type")] = QStringLiteral("string");
    questionProp[QStringLiteral("description")] = QStringLiteral(
        "O que você precisa descobrir lendo esse documento — repita ou "
        "resuma a pergunta original do usuário.");

    QJsonObject properties;
    properties[QStringLiteral("title")] = titleProp;
    properties[QStringLiteral("question")] = questionProp;

    QJsonArray required;
    required.append(QStringLiteral("title"));
    required.append(QStringLiteral("question"));

    QJsonObject params;
    params[QStringLiteral("type")] = QStringLiteral("object");
    params[QStringLiteral("properties")] = properties;
    params[QStringLiteral("required")] = required;

    AITool tool;
    tool.name = QStringLiteral("read_document");
    tool.description = QStringLiteral(
        "Lê o texto bruto INTEIRO de um capítulo ou ficha específico (não um "
        "trecho isolado — o documento completo) e responde à sua pergunta "
        "com base nele. Use depois de search_project ter indicado QUAL "
        "documento é relevante, quando os trechos isolados não bastarem "
        "pra entender a cena inteira — por exemplo quando um personagem é "
        "descrito ou referido indiretamente (\"a mulher\", \"a tal "
        "investigadora\") bem antes de ser nomeado no mesmo capítulo, algo "
        "que busca por palavra nunca vai conseguir conectar sozinha.");
    tool.parameters = params;
    return tool;
}

AITool saveProjectNoteTool() {
    QJsonObject categoryProp;
    categoryProp[QStringLiteral("type")] = QStringLiteral("string");
    categoryProp[QStringLiteral("enum")] = QJsonArray{
        QStringLiteral("canon"), QStringLiteral("personagem"), QStringLiteral("lore"),
        QStringLiteral("planejamento"), QStringLiteral("ideia"),
        QStringLiteral("pendencia"), QStringLiteral("preferencia")
    };
    categoryProp[QStringLiteral("description")] = QStringLiteral(
        "canon = fato oficial da história. personagem = personalidade/"
        "relação/objetivo/segredo/evolução de um personagem. lore = regra "
        "do mundo, lugar, cultura, tecnologia, magia. planejamento = "
        "estrutura, arco, capítulo/cena futura já decidida. ideia = "
        "possibilidade ainda não decidida. pendencia = pergunta em aberto "
        "que o autor precisa resolver. preferencia = estilo, tom ou limite "
        "criativo do autor (não é sobre a história em si).");

    QJsonObject statusProp;
    statusProp[QStringLiteral("type")] = QStringLiteral("string");
    statusProp[QStringLiteral("enum")] = QJsonArray{
        QStringLiteral("confirmada"), QStringLiteral("em_discussao"),
        QStringLiteral("ideia_futura"), QStringLiteral("descartada")
    };
    statusProp[QStringLiteral("description")] = QStringLiteral(
        "confirmada = fato estabelecido, pode tratar como verdade da obra "
        "daqui pra frente. em_discussao = cogitado nesta conversa, ainda "
        "NÃO é decisão fechada — não citar como fato consumado depois. "
        "ideia_futura = ideia solta pro futuro, sem compromisso nenhum. "
        "descartada = já foi considerado e rejeitado pelo autor — não "
        "sugerir de novo sem avisar que já foi descartado antes.");

    QJsonObject titleProp;
    titleProp[QStringLiteral("type")] = QStringLiteral("string");
    titleProp[QStringLiteral("description")] = QStringLiteral(
        "Título curto da nota, poucas palavras.");

    QJsonObject contentProp;
    contentProp[QStringLiteral("type")] = QStringLiteral("string");
    contentProp[QStringLiteral("description")] = QStringLiteral(
        "O conteúdo da nota, 1 a 4 frases, direto ao ponto.");

    QJsonObject properties;
    properties[QStringLiteral("category")] = categoryProp;
    properties[QStringLiteral("status")] = statusProp;
    properties[QStringLiteral("title")] = titleProp;
    properties[QStringLiteral("content")] = contentProp;

    QJsonArray required;
    required.append(QStringLiteral("category"));
    required.append(QStringLiteral("status"));
    required.append(QStringLiteral("title"));
    required.append(QStringLiteral("content"));

    QJsonObject params;
    params[QStringLiteral("type")] = QStringLiteral("object");
    params[QStringLiteral("properties")] = properties;
    params[QStringLiteral("required")] = required;

    AITool tool;
    tool.name = QStringLiteral("save_project_note");
    tool.description = QStringLiteral(
        "Salva uma nota permanente sobre o projeto — um fato de cânone, "
        "detalhe de personagem, regra de worldbuilding, plano futuro, ideia "
        "solta, pendência em aberto, ou preferência do autor. Chame quando "
        "o autor decidir/afirmar algo que vale lembrar depois, ou quando "
        "ele pedir explicitamente (\"lembra disso\", \"anota isso\", "
        "\"guarda essa ideia\"). Não chame pra conversa casual que não "
        "precisa virar registro permanente. Escolha o status com cuidado — "
        "uma ideia só cogitada em conversa é em_discussao ou ideia_futura, "
        "NUNCA confirmada, mesmo que pareça boa e o autor tenha gostado.");
    tool.parameters = params;
    return tool;
}

AITool saveUserNoteTool() {
    QJsonObject categoryProp;
    categoryProp[QStringLiteral("type")] = QStringLiteral("string");
    categoryProp[QStringLiteral("enum")] = QJsonArray{
        QStringLiteral("preferencia_criativa"), QStringLiteral("processo"),
        QStringLiteral("ideia_solta"), QStringLiteral("vinculo"),
        QStringLiteral("pendencia_geral")
    };
    categoryProp[QStringLiteral("description")] = QStringLiteral(
        "preferencia_criativa = gosto/estilo de escrita, temas favoritos, "
        "limite criativo — independente de qualquer projeto específico. "
        "processo = como o autor gosta de trabalhar (tipo de feedback que "
        "prefere, ritmo, o que anima/frustra numa revisão). ideia_solta = "
        "ideia de história/projeto que ainda não tem um projeto formal no "
        "Qenna. vinculo = piada interna, forma de conversar, referência "
        "recorrente — o que mantém a companhia genuína, não descreve a "
        "obra. pendencia_geral = algo que o autor quer decidir/fazer "
        "depois, não ligado a um projeto específico.");

    QJsonObject statusProp;
    statusProp[QStringLiteral("type")] = QStringLiteral("string");
    statusProp[QStringLiteral("enum")] = QJsonArray{
        QStringLiteral("confirmada"), QStringLiteral("em_discussao"),
        QStringLiteral("ideia_futura"), QStringLiteral("descartada")
    };
    statusProp[QStringLiteral("description")] = QStringLiteral(
        "confirmada = fato estável sobre o autor, pode tratar como certo "
        "daqui pra frente. em_discussao = cogitado nesta conversa, ainda "
        "NÃO é certeza. ideia_futura = ideia solta pro futuro, sem "
        "compromisso. descartada = já foi considerado e descartado pelo "
        "autor — não sugerir de novo sem avisar que já foi descartado.");

    QJsonObject titleProp;
    titleProp[QStringLiteral("type")] = QStringLiteral("string");
    titleProp[QStringLiteral("description")] = QStringLiteral(
        "Título curto da nota, poucas palavras.");

    QJsonObject contentProp;
    contentProp[QStringLiteral("type")] = QStringLiteral("string");
    contentProp[QStringLiteral("description")] = QStringLiteral(
        "O conteúdo da nota, 1 a 4 frases, direto ao ponto.");

    QJsonObject properties;
    properties[QStringLiteral("category")] = categoryProp;
    properties[QStringLiteral("status")] = statusProp;
    properties[QStringLiteral("title")] = titleProp;
    properties[QStringLiteral("content")] = contentProp;

    QJsonArray required;
    required.append(QStringLiteral("category"));
    required.append(QStringLiteral("status"));
    required.append(QStringLiteral("title"));
    required.append(QStringLiteral("content"));

    QJsonObject params;
    params[QStringLiteral("type")] = QStringLiteral("object");
    params[QStringLiteral("properties")] = properties;
    params[QStringLiteral("required")] = required;

    AITool tool;
    tool.name = QStringLiteral("save_user_note");
    tool.description = QStringLiteral(
        "Salva uma nota permanente sobre O AUTOR (não sobre a história) — "
        "vale em QUALQUER projeto, presente ou futuro, diferente de "
        "save_project_note que é só sobre a obra deste projeto específico. "
        "Use save_project_note para fatos da história/obra atual; use "
        "save_user_note para algo sobre o autor: preferência criativa, "
        "jeito de trabalhar, ideia solta sem projeto ainda, piada ou "
        "vínculo, pendência pessoal. Chame PROATIVAMENTE quando perceber "
        "algo assim na conversa — não espere o autor pedir \"lembra "
        "disso\". Não chame pra conversa casual que não revela nada que "
        "valha lembrar depois.");
    tool.parameters = params;
    return tool;
}

AITool resummarizeDocumentTool() {
    QJsonObject titleProp;
    titleProp[QStringLiteral("type")] = QStringLiteral("string");
    titleProp[QStringLiteral("description")] = QStringLiteral(
        "Título (ou parte dele) do documento cujo resumo salvo deve ser atualizado.");

    QJsonObject properties;
    properties[QStringLiteral("title")] = titleProp;

    QJsonArray required;
    required.append(QStringLiteral("title"));

    QJsonObject params;
    params[QStringLiteral("type")] = QStringLiteral("object");
    params[QStringLiteral("properties")] = properties;
    params[QStringLiteral("required")] = required;

    AITool tool;
    tool.name = QStringLiteral("resummarize_document");
    tool.description = QStringLiteral(
        "Relê um documento específico por inteiro e ATUALIZA o resumo salvo "
        "dele no contexto do projeto. Use SÓ quando o usuário pedir "
        "explicitamente pra reler algo que mudou (ex.: \"reescrevi o "
        "capítulo 5, lê de novo\", \"atualiza o resumo dessa ficha\"). "
        "Nunca chame por iniciativa própria — um resumo já salvo não fica "
        "desatualizado sozinho aos seus olhos, só quando o usuário avisa "
        "que mudou algo no documento.");
    tool.parameters = params;
    return tool;
}

// Compartilhada entre o scan em lote (processNextScanItem) e a releitura
// pontual sob pedido (handleResummarizeDocumentTool) — mesmo padrão de
// resumo nos dois casos, um só lugar pra manter.
QString docSummaryInstruction() {
    return QStringLiteral(
        "Resuma o documento a seguir em até 8 frases, em português, cobrindo "
        "o arco INTEIRO do texto — início, meio E fim. Se for um capítulo "
        "narrativo, a última frase do resumo deve dizer como a cena termina; "
        "não pare de resumir no meio só porque o começo já rendeu bastante "
        "frase. Foque em fatos concretos e específicos: nomes, idades, "
        "lugares, datas, eventos, características físicas, relações entre "
        "personagens, objetos importantes (armas, itens, veículos etc.) e "
        "detalhes textuais exatos quando existirem (ex.: o que está "
        "gravado/escrito em algo). Não invente nada que não esteja no "
        "texto. Não use frases de preâmbulo como \"este documento fala "
        "sobre\" — vá direto aos fatos.");
}

AITool lookupWorldDataTool() {
    QJsonObject queryProp;
    queryProp[QStringLiteral("type")] = QStringLiteral("string");
    queryProp[QStringLiteral("description")] = QStringLiteral(
        "Nome (ou parte dele) de um país ou cidade do mundo REAL a consultar.");

    QJsonObject properties;
    properties[QStringLiteral("query")] = queryProp;

    QJsonArray required;
    required.append(QStringLiteral("query"));

    QJsonObject params;
    params[QStringLiteral("type")] = QStringLiteral("object");
    params[QStringLiteral("properties")] = properties;
    params[QStringLiteral("required")] = required;

    AITool tool;
    tool.name = QStringLiteral("lookup_world_data");
    tool.description = QStringLiteral(
        "Consulta dados geográficos REAIS do mundo (não do projeto/história "
        "do autor) — países e cidades de verdade, com capital, população, "
        "área, continente, moeda e idiomas. Dataset embutido no app "
        "(Natural Earth/GeoNames), não depende de internet. Use quando o "
        "autor perguntar sobre geografia real (ex.: população do Japão, "
        "capital da Nigéria, onde fica Marselha, moeda da Tailândia) — NÃO "
        "use pra lugares fictícios do projeto (esses vêm de search_project "
        "ou dos pins do Mapa-múndi do próprio projeto, que são coisas "
        "diferentes deste dataset real).");
    tool.parameters = params;
    return tool;
}

AITool generateCharacterImageTool() {
    QJsonObject nameProp;
    nameProp[QStringLiteral("type")] = QStringLiteral("string");
    nameProp[QStringLiteral("description")] = QStringLiteral(
        "Nome do personagem JÁ EXISTENTE no projeto, como aparece na "
        "Biblioteca/gaveta de personagens.");

    QJsonObject descProp;
    descProp[QStringLiteral("type")] = QStringLiteral("string");
    descProp[QStringLiteral("description")] = QStringLiteral(
        "Descrição livre da CENA pedida pelo autor (pose, ação, roupa, "
        "cenário, expressão) — nunca um prompt de imagem técnico pronto, "
        "isso é gerado depois por você mesma noutra etapa. Preencha isso "
        "OU final_prompt, nunca os dois.");

    QJsonObject styleProp;
    styleProp[QStringLiteral("type")] = QStringLiteral("string");
    styleProp[QStringLiteral("enum")] = QJsonArray{
        QStringLiteral("padrao"), QStringLiteral("fotorrealista"),
        QStringLiteral("realismo_digital"), QStringLiteral("ilustracao_digital"),
        QStringLiteral("anime"), QStringLiteral("cartoon"), QStringLiteral("capa_gta")
    };
    styleProp[QStringLiteral("description")] = QStringLiteral(
        "Estilo visual pedido. Use \"padrao\" se o autor não especificar. "
        "Ignorado se final_prompt for usado.");

    QJsonObject finalPromptProp;
    finalPromptProp[QStringLiteral("type")] = QStringLiteral("string");
    finalPromptProp[QStringLiteral("description")] = QStringLiteral(
        "Prompt de imagem JÁ PRONTO fornecido pelo próprio autor (ele "
        "escreveu/colou o texto exato e pediu explicitamente pra usar "
        "aquilo, sem reescrever nada). Preencha isso OU description, nunca "
        "os dois. Só use este campo quando o autor disser algo como \"usa "
        "esse prompt\", \"gera com esse texto exato\" ou colar um prompt "
        "pronto — NUNCA decida sozinha pular a etapa de engenharia de "
        "prompt achando que sabe escrever melhor.");

    QJsonObject properties;
    properties[QStringLiteral("character_name")] = nameProp;
    properties[QStringLiteral("description")] = descProp;
    properties[QStringLiteral("style")] = styleProp;
    properties[QStringLiteral("final_prompt")] = finalPromptProp;

    QJsonArray required;
    required.append(QStringLiteral("character_name"));

    QJsonObject params;
    params[QStringLiteral("type")] = QStringLiteral("object");
    params[QStringLiteral("properties")] = properties;
    params[QStringLiteral("required")] = required;

    AITool tool;
    tool.name = QStringLiteral("generate_character_image");
    tool.description = QStringLiteral(
        "Gera uma nova foto pra um personagem JÁ EXISTENTE no projeto e "
        "salva na ficha dele, substituindo a foto atual. Dois modos, "
        "mutuamente exclusivos: (1) NORMAL — preencha description (+ "
        "style opcional) com a cena pedida em linguagem livre; você NUNCA "
        "escreve o prompt técnico final nesse modo, ele é engenhado "
        "automaticamente depois, usando também a ficha do personagem "
        "quando existir. (2) MANUAL — se o autor forneceu um prompt "
        "PRONTO e pediu explicitamente pra usar exatamente aquele texto "
        "(sem reescrever), preencha final_prompt em vez de description; "
        "esse texto vai direto pra API de imagem, sem nenhum ajuste seu. "
        "Só chame sob pedido EXPLÍCITO do autor pra gerar/trocar a imagem "
        "de um personagem — nunca por iniciativa própria. Se o personagem "
        "não existir ainda no projeto, NÃO chame esta função — avise que é "
        "preciso criar o personagem primeiro.");
    tool.parameters = params;
    return tool;
}

AITool generateSceneImageTool() {
    QJsonObject descProp;
    descProp[QStringLiteral("type")] = QStringLiteral("string");
    descProp[QStringLiteral("description")] = QStringLiteral(
        "Descrição livre da CENA/AMBIENTE/OBJETO pedido pelo autor (lugar, "
        "momento, atmosfera, elementos visuais) — nunca um prompt de imagem "
        "técnico pronto, isso é gerado depois por você mesma noutra etapa. "
        "Pode citar personagens de passagem (ex. \"a silhueta de alguém ao "
        "longe\"), mas isso NÃO é a mesma coisa que gerar o retrato de um "
        "personagem específico — se o pedido for claramente um retrato "
        "focado numa pessoa já cadastrada no projeto, use "
        "generate_character_image em vez desta. Preencha isso OU "
        "final_prompt, nunca os dois.");

    QJsonObject styleProp;
    styleProp[QStringLiteral("type")] = QStringLiteral("string");
    styleProp[QStringLiteral("enum")] = QJsonArray{
        QStringLiteral("padrao"), QStringLiteral("fotorrealista"),
        QStringLiteral("realismo_digital"), QStringLiteral("ilustracao_digital"),
        QStringLiteral("anime"), QStringLiteral("cartoon"), QStringLiteral("capa_gta")
    };
    styleProp[QStringLiteral("description")] = QStringLiteral(
        "Estilo visual pedido. Use \"padrao\" se o autor não especificar. "
        "Ignorado se final_prompt for usado.");

    QJsonObject finalPromptProp;
    finalPromptProp[QStringLiteral("type")] = QStringLiteral("string");
    finalPromptProp[QStringLiteral("description")] = QStringLiteral(
        "Prompt de imagem JÁ PRONTO fornecido pelo próprio autor (ele "
        "escreveu/colou o texto exato e pediu explicitamente pra usar "
        "aquilo, sem reescrever nada). Preencha isso OU description, nunca "
        "os dois. NUNCA decida sozinha pular a etapa de engenharia de "
        "prompt achando que sabe escrever melhor.");

    QJsonObject properties;
    properties[QStringLiteral("description")] = descProp;
    properties[QStringLiteral("style")] = styleProp;
    properties[QStringLiteral("final_prompt")] = finalPromptProp;

    QJsonArray required;
    required.append(QStringLiteral("description"));

    QJsonObject params;
    params[QStringLiteral("type")] = QStringLiteral("object");
    params[QStringLiteral("properties")] = properties;
    params[QStringLiteral("required")] = required;

    AITool tool;
    tool.name = QStringLiteral("generate_scene_image");
    tool.description = QStringLiteral(
        "Gera uma imagem livre — cena, ambiente, objeto, momento do "
        "enredo — SEM associar a foto de nenhum personagem específico "
        "(não mexe em nenhuma ficha). A imagem gerada fica disponível na "
        "galeria do projeto e aparece na sua resposta. Dois modos, "
        "mutuamente exclusivos: (1) NORMAL — preencha description (+ style "
        "opcional) em linguagem livre; você NUNCA escreve o prompt técnico "
        "final nesse modo. (2) MANUAL — se o autor forneceu um prompt "
        "PRONTO e pediu explicitamente pra usar exatamente aquele texto, "
        "preencha final_prompt em vez de description. Só chame sob pedido "
        "EXPLÍCITO do autor pra gerar uma imagem — nunca por iniciativa "
        "própria. Se o pedido for claramente sobre o retrato de um "
        "personagem específico já existente no projeto, use "
        "generate_character_image em vez desta.");
    tool.parameters = params;
    return tool;
}

AITool proposeDocumentEditTool() {
    QJsonObject originalProp;
    originalProp[QStringLiteral("type")] = QStringLiteral("string");
    originalProp[QStringLiteral("description")] = QStringLiteral(
        "O trecho ORIGINAL exatamente como aparece no documento aberto no "
        "editor agora — copiado literalmente (mesma pontuação, acentuação e "
        "espaçamento do texto real, nunca parafraseado ou de memória). É "
        "usado pra localizar onde aplicar a correção; se não bater "
        "exatamente com o texto do documento, a edição falha. Inclua "
        "contexto suficiente ao redor pra garantir que o trecho apareça "
        "só UMA vez no documento — uma frase inteira costuma bastar, uma "
        "única palavra comum geralmente não.");

    QJsonObject newProp;
    newProp[QStringLiteral("type")] = QStringLiteral("string");
    newProp[QStringLiteral("description")] = QStringLiteral(
        "O texto de substituição, pronto para entrar no lugar exato do "
        "original_text.");

    QJsonObject explanationProp;
    explanationProp[QStringLiteral("type")] = QStringLiteral("string");
    explanationProp[QStringLiteral("description")] = QStringLiteral(
        "Explicação BREVE (uma frase) do que a mudança resolve — mostrada "
        "junto do cartão de confirmação. Opcional, mas recomendado.");

    QJsonObject properties;
    properties[QStringLiteral("original_text")] = originalProp;
    properties[QStringLiteral("new_text")] = newProp;
    properties[QStringLiteral("explanation")] = explanationProp;

    QJsonArray required;
    required.append(QStringLiteral("original_text"));
    required.append(QStringLiteral("new_text"));

    QJsonObject params;
    params[QStringLiteral("type")] = QStringLiteral("object");
    params[QStringLiteral("properties")] = properties;
    params[QStringLiteral("required")] = required;

    AITool tool;
    tool.name = QStringLiteral("propose_document_edit");
    tool.description = QStringLiteral(
        "Propõe uma correção pontual DIRETO no documento que o autor tem "
        "aberto no editor agora — aparece como um cartão com Aplicar/"
        "Descartar na conversa; você NUNCA aplica sozinha, o autor decide. "
        "Só chame isso quando já tiver identificado um problema concreto e "
        "uma correção específica (seguindo as mesmas regras de \"Método de "
        "análise\"/\"Reescritas\" da sua personalidade — corrija erro real "
        "ou deixe claro que é alternativa de tom, nunca proponha edição só "
        "por hábito). Não serve pra reescrever o documento inteiro nem pra "
        "múltiplos trechos não relacionados de uma vez — uma chamada por "
        "correção pontual; pode chamar várias vezes na mesma resposta se "
        "houver mais de um ponto. Se nenhum documento estiver aberto no "
        "editor, a chamada falha — avise o autor que precisa abrir o "
        "capítulo/cena primeiro.");
    tool.parameters = params;
    return tool;
}

} // namespace

AIChatPanel::AIChatPanel(ProjectModel* projectModel,
                         ElementsStore* elementsStore,
                         DocCache* docCache,
                         QWidget* parent)
    : QFrame(parent)
    , m_client(new AIClient(this))
    , m_projectModel(projectModel)
    , m_elementsStore(elementsStore)
    , m_docCache(docCache)
{
    setObjectName(QStringLiteral("aiChatPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFrameShape(QFrame::NoFrame);

    // NUNCA usar QGraphicsEffect (drop shadow/opacity) num widget que
    // contenha um QScrollArea entre seus descendentes — corta o repaint do
    // conteúdo (mesmo gotcha já documentado no StackView da Biblioteca,
    // ver memória stack-view-pilha-feature: os balões calculam a altura
    // certa a partir do texto real, mas o repaint nunca chega na tela,
    // ficando vazios visualmente). O painel de chat tem m_transcriptScroll
    // (QScrollArea) entre seus filhos, então a sombra fica de fora.

    buildUi();
    applyTheme();
    // O painel é construído UMA vez, no construtor da MainWindow (boot do app),
    // e depois só é mostrado/escondido — sem esta conexão ele fica preso pra
    // sempre nas cores do tema que estava ativo no boot, e destoa da janela
    // inteira assim que o usuário troca de tema (ou a troca automática
    // dia/noite dispara). Padrão usado por todos os outros painéis flutuantes.
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &AIChatPanel::applyTheme);
    hide();

    connect(m_client, &AIClient::tokenReceived, this, [this](const QString& token) {
        if (m_scanning) return; // silencioso durante o scan — só o status label mostra progresso
        appendStreamToken(token);
    });
    connect(m_client, &AIClient::finished, this, [this](const QString& fullText) {
        if (m_scanning) {
            onScanDocFinished(fullText);
            return;
        }
        if (m_pendingToolCall) {
            // Não é resposta final — o toolCallReceived (que já disparou
            // antes deste finished, ver ordem em AIClient::handleFinished)
            // montou o round-trip e agendou o reenvio.
            return;
        }
        AIChatMessage assistantMsg;
        assistantMsg.role = QStringLiteral("assistant");
        assistantMsg.content = fullText;
        m_messages.append(assistantMsg);
        logConversation(miraAssistantName(), fullText);
        finalizeMiraStreamBubble(m_pendingToolTraces, m_pendingBubbleImages, m_pendingEditSuggestions);
        m_pendingToolTraces.clear();
        m_pendingBubbleImages.clear();
        m_pendingEditSuggestions.clear();
        saveCurrentSession();
        setBusy(false);
    });
    connect(m_client, &AIClient::toolCallReceived, this,
            [this](const QString& id, const QString& name, const QJsonObject& args) {
        if (m_scanning) return; // scan não usa tools; defensivo
        if (++m_toolHopCount > 4) {
            finalizeMiraStreamBubble(m_pendingToolTraces, m_pendingBubbleImages, m_pendingEditSuggestions);
            m_pendingToolTraces.clear();
            m_pendingBubbleImages.clear();
            m_pendingEditSuggestions.clear();
            addMiraBubble(tr("(precisei buscar demais e vou parar por aqui pra não ficar em loop — pode reformular a pergunta?)"));
            setBusy(false);
            return;
        }
        m_pendingToolCall = true;
        handleToolCall(id, name, args);
    });
    connect(m_client, &AIClient::errorOccurred, this, [this](const QString& msg) {
        if (m_scanning) {
            const ScanDoc& doc = m_scanQueue[m_scanIndex];
            m_scanSummaries.append(QStringLiteral("## %1\n\n(Erro ao resumir: %2)\n").arg(doc.title, msg));
            ++m_scanIndex;
            processNextScanItem();
            return;
        }
        m_pendingToolCall = false;
        finalizeMiraStreamBubble(m_pendingToolTraces, m_pendingBubbleImages, m_pendingEditSuggestions);
        m_pendingToolTraces.clear();
        m_pendingBubbleImages.clear();
        m_pendingEditSuggestions.clear();
        addMiraBubble(tr("⚠️ Erro: %1").arg(msg));
        setBusy(false);
    });
}

void AIChatPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 0, 12, 12);
    root->setSpacing(8);

    // Header — drag handle + título (ou identidade completa, em modo
    // janela) + ícones de ação + X. UMA barra só, largura cheia — antes
    // isso era duas barras empilhadas (identidade + ícones), o que num
    // painel largo (modo janela, rail+chat lado a lado) deixava o título
    // parecendo flutuar sobre o rail e os ícones sobre o chat, como se
    // fossem coisas soltas em vez de um cabeçalho único.
    m_header = new QWidget(this);
    m_header->setFixedHeight(kHeaderH);
    m_header->setCursor(Qt::OpenHandCursor);
    auto* hlay = new QHBoxLayout(m_header);
    hlay->setContentsMargins(2, 0, 2, 0);
    hlay->setSpacing(8);

    m_titleLabel = new QLabel(miraAssistantName(), m_header);
    hlay->addWidget(m_titleLabel);

    // Bloco de identidade (avatar + nome/subtítulo) — substitui m_titleLabel
    // no modo janela (ver applyLayoutMode); os dois nunca ficam visíveis ao
    // mesmo tempo, é uma troca, não uma segunda linha.
    m_studioHeaderWidget = new QWidget(m_header);
    auto* studioLay = new QHBoxLayout(m_studioHeaderWidget);
    studioLay->setContentsMargins(0, 0, 0, 0);
    studioLay->setSpacing(8);

    m_studioAvatarLabel = new QLabel(m_studioHeaderWidget);
    m_studioAvatarLabel->setFixedSize(26, 26);
    m_studioAvatarLabel->setAlignment(Qt::AlignCenter);
    m_studioAvatarLabel->setText(miraAssistantName().left(1).toUpper());
    m_studioAvatarLabel->setStyleSheet(avatarQss());
    studioLay->addWidget(m_studioAvatarLabel, 0, Qt::AlignVCenter);

    // Nome+subtítulo num QWidget de verdade (não um QLayout solto direto no
    // QHBoxLayout) — isso garante um sizeHint próprio e evita o esticamento/
    // sobreposição que addLayout() num QHBoxLayout pode causar quando a
    // altura calculada do conteúdo não bate exatamente com a altura fixa do
    // cabeçalho (kHeaderH).
    auto* whoWidget = new QWidget(m_studioHeaderWidget);
    auto* whoCol = new QVBoxLayout(whoWidget);
    whoCol->setContentsMargins(0, 0, 0, 0);
    whoCol->setSpacing(0);
    m_studioNameLabel = new QLabel(miraAssistantName(), whoWidget);
    m_studioNameLabel->setWordWrap(false);
    m_studioNameLabel->setStyleSheet(studioNameQss());
    whoCol->addWidget(m_studioNameLabel);
    m_studioSubtitleLabel = new QLabel(tr("parceira criativa · sempre por perto"), whoWidget);
    m_studioSubtitleLabel->setWordWrap(false);
    m_studioSubtitleLabel->setStyleSheet(studioSubtitleQss());
    whoCol->addWidget(m_studioSubtitleLabel);
    studioLay->addWidget(whoWidget, 0, Qt::AlignVCenter);

    m_studioHeaderWidget->setVisible(false); // começa em modo painel — applyLayoutMode() decide
    m_studioHeaderWidget->setCursor(Qt::PointingHandCursor);
    m_studioHeaderWidget->setToolTip(tr("Clique pra personalizar a %1").arg(miraAssistantName()));
    m_studioHeaderWidget->installEventFilter(this);
    hlay->addWidget(m_studioHeaderWidget, 0, Qt::AlignVCenter);

    hlay->addStretch();

    m_memoryChipLabel = new QLabel(m_header);
    m_memoryChipLabel->setStyleSheet(memoryChipQss());
    m_memoryChipLabel->setVisible(false);
    hlay->addWidget(m_memoryChipLabel);

    auto makeHeaderBtn = [this](const QString& iconPath, const QString& tooltip) {
        auto* b = new QToolButton(m_header);
        b->setIcon(toolIcon(iconPath));
        b->setToolTip(tooltip);
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumSize(24, 24);
        return b;
    };

    m_historyBtn = makeHeaderBtn(QStringLiteral(":/icons/stats-clock.svg"), tr("Conversas anteriores"));
    connect(m_historyBtn, &QToolButton::clicked, this, &AIChatPanel::showSessionMenu);
    hlay->addWidget(m_historyBtn);

    m_newChatBtn = makeHeaderBtn(QStringLiteral(":/icons/doc-plus.svg"), tr("Nova conversa"));
    connect(m_newChatBtn, &QToolButton::clicked, this, &AIChatPanel::startNewSession);
    hlay->addWidget(m_newChatBtn);

    m_scanBtn = makeHeaderBtn(QStringLiteral(":/icons/glossary.svg"), tr("Ler documentos do projeto"));
    connect(m_scanBtn, &QToolButton::clicked, this, &AIChatPanel::startProjectScan);
    hlay->addWidget(m_scanBtn);

    m_galleryBtn = makeHeaderBtn(QStringLiteral(":/icons/add-image.svg"), tr("Galeria de imagens geradas"));
    connect(m_galleryBtn, &QToolButton::clicked, this, &AIChatPanel::openImageGallery);
    hlay->addWidget(m_galleryBtn);

    m_windowMode = QSettings().value(QStringLiteral("ai/chatWindowMode"), false).toBool();
    m_layoutBtn = makeHeaderBtn(QStringLiteral(":/icons/fullscreen.svg"), m_windowMode
        ? tr("Voltar pro modo painel (ancorado à direita)")
        : tr("Modo janela (centralizada, maior)"));
    connect(m_layoutBtn, &QToolButton::clicked, this, &AIChatPanel::toggleLayoutMode);
    hlay->addWidget(m_layoutBtn);

    m_modelCombo = new QComboBox(m_header);
    m_modelCombo->setEditable(true);
    // Só sugestões — o campo é livre porque o Endpoint (Configurações →
    // Assistente de IA) decide de fato pra qual provedor a chamada vai.
    // Trocar o modelo aqui sem trocar o Endpoint pro provedor certo não
    // funciona (todos falam formatos diferentes de chat/completions).
    m_modelCombo->addItems({ QStringLiteral("gpt-4o-mini"), QStringLiteral("gpt-4o"),
                            QStringLiteral("gpt-4.1-mini"), QStringLiteral("gpt-4.1"),
                            QStringLiteral("gpt-5-mini"),
                            QStringLiteral("claude-sonnet-5"), QStringLiteral("claude-opus-5"),
                            QStringLiteral("gemini-2.5-flash"), QStringLiteral("gemini-2.5-pro"),
                            QStringLiteral("grok-4") });
    m_modelCombo->setCurrentText(QSettings().value(QStringLiteral("ai/model"),
        QStringLiteral("gpt-4o-mini")).toString());
    m_modelCombo->setToolTip(tr("Modelo usado pela %1 (troca vale pra próxima mensagem)").arg(miraAssistantName()));
    m_modelCombo->setFixedWidth(102);
    auto saveModelChoice = [this]() {
        QSettings().setValue(QStringLiteral("ai/model"), m_modelCombo->currentText().trimmed());
    };
    connect(m_modelCombo, qOverload<int>(&QComboBox::activated), this, saveModelChoice);
    connect(m_modelCombo->lineEdit(), &QLineEdit::editingFinished, this, saveModelChoice);
    hlay->addWidget(m_modelCombo);

    m_closeBtn = new QToolButton(m_header);
    m_closeBtn->setIconSize(QSize(14, 14));
    m_closeBtn->setToolTip(tr("Fechar"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setMinimumSize(24, 24);
    connect(m_closeBtn, &QToolButton::clicked, this, [this]() { closePanel(); });
    hlay->addWidget(m_closeBtn);
    root->addWidget(m_header);

    // Corpo: rail de projetos/conversas (só visível em modo janela, ver
    // applyLayoutMode) + coluna de chat (sempre visível) lado a lado. A
    // coluna de chat concentra tudo que antes ia direto pro `root` — troca
    // mecânica de destino, nenhum widget interno muda de comportamento.
    auto* bodyRow = new QHBoxLayout();
    bodyRow->setContentsMargins(0, 0, 0, 0);
    bodyRow->setSpacing(8);

    m_railWidget = new QWidget(this);
    m_railWidget->setFixedWidth(230);
    m_railWidget->setVisible(false); // só aparece em modo janela
    auto* railLay = new QVBoxLayout(m_railWidget);
    railLay->setContentsMargins(0, 0, 0, 0);
    railLay->setSpacing(4);

    m_railSearchEdit = new QLineEdit(m_railWidget);
    m_railSearchEdit->setPlaceholderText(tr("Buscar projeto ou conversa…"));
    m_railSearchEdit->setStyleSheet(railSearchQss());
    connect(m_railSearchEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_railFilterText = text.trimmed();
        rebuildProjectRail();
    });
    railLay->addWidget(m_railSearchEdit);

    auto* railScroll = new QScrollArea(m_railWidget);
    railScroll->setWidgetResizable(true);
    railScroll->setFrameShape(QFrame::NoFrame);
    railScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    railScroll->setStyleSheet(railScrollQss());
    m_railScrollContent = new QWidget(railScroll);
    m_railScrollLayout = new QVBoxLayout(m_railScrollContent);
    m_railScrollLayout->setContentsMargins(4, 4, 4, 4);
    m_railScrollLayout->setSpacing(2);
    m_railScrollLayout->addStretch(1);
    railScroll->setWidget(m_railScrollContent);
    railLay->addWidget(railScroll);

    bodyRow->addWidget(m_railWidget);

    m_chatColumnWidget = new QWidget(this);
    auto* chatColumnLay = new QVBoxLayout(m_chatColumnWidget);
    chatColumnLay->setContentsMargins(0, 0, 0, 0);
    chatColumnLay->setSpacing(8);
    bodyRow->addWidget(m_chatColumnWidget, /*stretch=*/1);

    root->addLayout(bodyRow, /*stretch=*/1);

    // Barra de contexto "Projeto · Conversa" (modo janela) — troca de lugar
    // com m_previewBanner conforme o estado, ver updateChatContext().
    m_chatContextLabel = new QLabel(m_chatColumnWidget);
    m_chatContextLabel->setTextFormat(Qt::RichText);
    m_chatContextLabel->setStyleSheet(chatContextQss());
    m_chatContextLabel->setVisible(false);
    chatColumnLay->addWidget(m_chatContextLabel);

    // Barra de "modo somente-leitura" (ver previewForeignSession) — some
    // por padrão, só aparece ao visualizar a conversa de outro projeto.
    m_previewBanner = new QWidget(this);
    auto* previewLay = new QHBoxLayout(m_previewBanner);
    previewLay->setContentsMargins(8, 6, 8, 6);
    previewLay->setSpacing(8);
    m_previewBannerLabel = new QLabel(m_previewBanner);
    m_previewBannerLabel->setWordWrap(true);
    m_previewBannerLabel->setTextFormat(Qt::RichText);
    previewLay->addWidget(m_previewBannerLabel, 1);
    m_previewOpenBtn = new QPushButton(tr("Abrir esse projeto"), m_previewBanner);
    m_previewOpenBtn->setCursor(Qt::PointingHandCursor);
    connect(m_previewOpenBtn, &QPushButton::clicked, this, [this]() {
        emit openProjectRequested(m_previewRoot);
    });
    previewLay->addWidget(m_previewOpenBtn);
    m_previewBanner->setStyleSheet(previewBannerQss());
    m_previewOpenBtn->setStyleSheet(sendBtnQss());
    m_previewBanner->setVisible(false);
    chatColumnLay->addWidget(m_previewBanner);

    // Transcrição — bolhas de chat de verdade (esquerda/direita), não um
    // QLabel de texto corrido (tinha bug de quebra de linha patológica em
    // layouts aninhados). Cada bolha é inserida logo antes do addStretch(1)
    // final, que fica sempre no fim do layout empurrando o conteúdo pra
    // cima quando há pouca conversa.
    m_transcriptScroll = new QScrollArea(this);
    m_transcriptScroll->setWidgetResizable(true);
    m_transcriptScroll->setFrameShape(QFrame::NoFrame);
    m_transcriptScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_transcriptScroll->setMinimumHeight(160);

    m_transcriptContent = new QWidget(m_transcriptScroll);
    m_transcriptLayout = new QVBoxLayout(m_transcriptContent);
    m_transcriptLayout->setContentsMargins(10, 10, 10, 10);
    m_transcriptLayout->setSpacing(10);
    m_transcriptLayout->addStretch(1);
    m_transcriptScroll->setWidget(m_transcriptContent);

    // Auto-scroll pro fim sempre que o conteúdo crescer (streaming token a
    // token, nova bolha etc.) — dispensa lógica manual de "rolar depois".
    connect(m_transcriptScroll->verticalScrollBar(), &QScrollBar::rangeChanged, this,
            [this](int, int max) { m_transcriptScroll->verticalScrollBar()->setValue(max); });

    chatColumnLay->addWidget(m_transcriptScroll, /*stretch=*/1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setVisible(false);
    chatColumnLay->addWidget(m_statusLabel);

    // Foco em documento — controle manual do que entra como contexto extra
    // nesta conversa (texto INTEIRO de um doc, não só o resumo), sem
    // depender da IA decidir buscar sozinha. Desligado por padrão.
    //
    // m_docFocusCheck/m_docFocusCombo continuam sendo o ESTADO de verdade
    // (lido por buildSystemPrompt/onDocFocusChanged) — só não viram mais uma
    // linha própria sempre visível acima do composer (isso ficava chamativo
    // demais pra um controle usado raramente). Ficam órfãos de layout
    // (parented a `this`, nunca adicionados/mostrados) e só são mexidos
    // através do menu de m_docFocusBtn, no composer — ver mais abaixo.
    m_docFocusCheck = new QCheckBox(this);
    m_docFocusCombo = new QComboBox(this);
    m_docFocusCombo->setEnabled(false);
    // Sem layout dono nenhum — um QWidget filho sem setVisible(false)
    // explícito herda a visibilidade do pai e flutua na posição padrão
    // (0,0), foi o que sobrepôs o cabeçalho. Precisam ficar de verdade
    // escondidos, não só "fora do layout".
    m_docFocusCheck->setVisible(false);
    m_docFocusCombo->setVisible(false);

    connect(m_docFocusCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_docFocusCombo->setEnabled(checked);
        if (checked) {
            refreshDocFocusCombo();
            if (m_docFocusCombo->currentIndex() < 0 && m_currentDocTitleProvider) {
                const QString cur = m_currentDocTitleProvider();
                const int idx = m_docFocusCombo->findText(cur);
                if (idx >= 0) m_docFocusCombo->setCurrentIndex(idx);
            }
        }
        onDocFocusChanged();
    });
    connect(m_docFocusCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { onDocFocusChanged(); });

    // Preview do anexo de imagem — some quando não há nada anexado ainda.
    m_attachPreviewRow = new QWidget(this);
    auto* attachPreviewLay = new QHBoxLayout(m_attachPreviewRow);
    attachPreviewLay->setContentsMargins(2, 0, 2, 0);
    attachPreviewLay->setSpacing(6);

    m_attachThumb = new QLabel(this);
    m_attachThumb->setFixedSize(40, 40);
    m_attachThumb->setScaledContents(false);
    m_attachThumb->setAlignment(Qt::AlignCenter);
    attachPreviewLay->addWidget(m_attachThumb);

    auto* attachLabel = new QLabel(tr("Imagem anexada"), this);
    attachPreviewLay->addWidget(attachLabel, 1);

    m_attachClearBtn = new QToolButton(this);
    m_attachClearBtn->setIcon(toolIcon(QStringLiteral(":/icons/close.svg"), QSize(12, 12)));
    m_attachClearBtn->setCursor(Qt::PointingHandCursor);
    connect(m_attachClearBtn, &QToolButton::clicked, this, &AIChatPanel::clearAttachImage);
    attachPreviewLay->addWidget(m_attachClearBtn);

    m_attachPreviewRow->setVisible(false);
    chatColumnLay->addWidget(m_attachPreviewRow);

    // Campo de pedido livre — multi-linha de verdade (Enter envia, Shift+
    // Enter quebra linha; cresce sozinho até um teto, depois rola por
    // dentro). QLineEdit antigo não permitia quebra de linha nenhuma.
    auto* inputRow = new QWidget(this);
    auto* irlay = new QHBoxLayout(inputRow);
    irlay->setContentsMargins(0, 0, 0, 0);
    irlay->setSpacing(6);

    m_attachBtn = new QToolButton(this);
    m_attachBtn->setIcon(toolIcon(QStringLiteral(":/icons/add-image.svg")));
    m_attachBtn->setToolTip(tr("Anexar imagem"));
    m_attachBtn->setCursor(Qt::PointingHandCursor);
    connect(m_attachBtn, &QToolButton::clicked, this, &AIChatPanel::pickAttachImage);
    irlay->addWidget(m_attachBtn);

    // Foco em documento (discreto — ver comentário acima de m_docFocusCheck):
    // menu popup com os documentos do projeto; escolher um liga o foco,
    // clicar de novo desliga. Ícone marcado (cor de destaque) enquanto ativo.
    m_docFocusBtn = new QToolButton(this);
    m_docFocusBtn->setIcon(toolIcon(QStringLiteral(":/icons/readmode.svg")));
    m_docFocusBtn->setCheckable(true);
    m_docFocusBtn->setCursor(Qt::PointingHandCursor);
    m_docFocusBtn->setToolTip(tr("Focar em um documento específico desta conversa"));
    connect(m_docFocusBtn, &QToolButton::clicked, this, [this]() {
        if (m_docFocusCheck->isChecked()) {
            m_docFocusCheck->setChecked(false);
            m_docFocusBtn->setToolTip(tr("Focar em um documento específico desta conversa"));
            m_docFocusBtn->setChecked(false);
            return;
        }
        refreshDocFocusCombo();
        QMenu menu(this);
        if (m_docFocusCombo->count() == 0) {
            QAction* empty = menu.addAction(tr("(nenhum documento encontrado)"));
            empty->setEnabled(false);
        }
        for (int i = 0; i < m_docFocusCombo->count(); ++i) {
            const QString title = m_docFocusCombo->itemText(i);
            QAction* a = menu.addAction(title);
            connect(a, &QAction::triggered, this, [this, title]() {
                const int idx = m_docFocusCombo->findText(title);
                if (idx >= 0) m_docFocusCombo->setCurrentIndex(idx);
                m_docFocusCheck->setChecked(true);
                m_docFocusBtn->setToolTip(tr("Focando em \"%1\" (clique pra desligar)").arg(title));
            });
        }
        menu.exec(m_docFocusBtn->mapToGlobal(QPoint(0, m_docFocusBtn->height())));
        m_docFocusBtn->setChecked(m_docFocusCheck->isChecked());
    });
    irlay->addWidget(m_docFocusBtn);

    // QTextEdit, não QPlainTextEdit: o motor de layout do QPlainTextEdit
    // (QPlainTextDocumentLayout) não responde de forma confiável a
    // document()->setTextWidth() manual, deixando fitInputHeight() sempre
    // travado numa altura mínima mesmo com texto de várias linhas —
    // QTextEdit usa o motor "normal" (QTextDocumentLayout), já comprovado
    // funcionando em createBubbleRow/fitBubbleHeight.
    m_inputEdit = new QTextEdit(this);
    m_inputEdit->setObjectName(QStringLiteral("chatInputEdit"));
    m_inputEdit->setAcceptRichText(false);
    m_inputEdit->setPlaceholderText(tr("Converse com a %1 sobre o projeto… (Enter envia, Shift+Enter quebra linha)").arg(miraAssistantName()));
    m_inputEdit->setTabChangesFocus(true);
    m_inputEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_inputEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_inputEdit->setFixedHeight(38);
    m_inputEdit->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    m_inputEdit->installEventFilter(this);
    connect(m_inputEdit, &QTextEdit::textChanged, this, [this]() { fitInputHeight(); });
    irlay->addWidget(m_inputEdit, 1);

    m_sendBtn = new QPushButton(tr("Enviar"), this);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    connect(m_sendBtn, &QPushButton::clicked, this, [this]() {
        const QString text = m_inputEdit->toPlainText().trimmed();
        if (text.isEmpty()) return;
        sendUserMessage(text);
    });
    irlay->addWidget(m_sendBtn);

    chatColumnLay->addWidget(inputRow);

    // Grip de resize no canto inferior direito — funciona nos dois modos de
    // layout (painel ancorado ou janela centralizada). Overlay solto (não
    // faz parte do layout), reposicionado em resizeEvent().
    m_resizeGrip = new QWidget(this);
    m_resizeGrip->setFixedSize(16, 16);
    m_resizeGrip->setCursor(Qt::SizeFDiagCursor);
    m_resizeGrip->setToolTip(tr("Arraste pra redimensionar"));
    m_resizeGrip->installEventFilter(this);
    m_resizeGrip->raise();
}

void AIChatPanel::applyTheme()
{
    setStyleSheet(panelFrameQss());

    if (m_titleLabel) m_titleLabel->setStyleSheet(titleQss());

    const QString headerQss = headerBtnQss();
    for (QToolButton* b : { m_historyBtn, m_newChatBtn, m_scanBtn, m_galleryBtn, m_layoutBtn }) {
        if (b) b->setStyleSheet(headerQss);
    }
    // Ícones dependem de cor do tema (mesmo cuidado de closeBtnIcon) —
    // precisam ser recriados na troca, não só o QSS do botão em volta.
    if (m_historyBtn) m_historyBtn->setIcon(toolIcon(QStringLiteral(":/icons/stats-clock.svg")));
    if (m_newChatBtn) m_newChatBtn->setIcon(toolIcon(QStringLiteral(":/icons/doc-plus.svg")));
    if (m_scanBtn) m_scanBtn->setIcon(toolIcon(QStringLiteral(":/icons/glossary.svg")));
    if (m_galleryBtn) m_galleryBtn->setIcon(toolIcon(QStringLiteral(":/icons/add-image.svg")));
    if (m_layoutBtn) m_layoutBtn->setIcon(toolIcon(QStringLiteral(":/icons/fullscreen.svg")));
    if (m_attachBtn) m_attachBtn->setIcon(toolIcon(QStringLiteral(":/icons/add-image.svg")));
    if (m_attachClearBtn) m_attachClearBtn->setIcon(toolIcon(QStringLiteral(":/icons/close.svg"), QSize(12, 12)));
    if (m_docFocusBtn) {
        m_docFocusBtn->setIcon(toolIcon(QStringLiteral(":/icons/readmode.svg")));
        m_docFocusBtn->setStyleSheet(headerQss);
    }
    if (m_closeBtn) {
        m_closeBtn->setStyleSheet(closeBtnQss());
        m_closeBtn->setIcon(closeBtnIcon());
    }

    if (m_modelCombo) m_modelCombo->setStyleSheet(smallComboQss());
    if (m_docFocusCombo) m_docFocusCombo->setStyleSheet(smallComboQss());
    if (m_docFocusCheck) m_docFocusCheck->setStyleSheet(docFocusCheckQss());
    if (m_transcriptScroll) m_transcriptScroll->setStyleSheet(transcriptScrollQss());
    if (m_statusLabel) m_statusLabel->setStyleSheet(statusLabelQss());
    if (m_inputEdit) m_inputEdit->setStyleSheet(inputEditQss());
    if (m_sendBtn) m_sendBtn->setStyleSheet(sendBtnQss());
    if (m_resizeGrip) m_resizeGrip->setStyleSheet(gripQss());
    if (m_previewBanner) m_previewBanner->setStyleSheet(previewBannerQss());
    if (m_previewOpenBtn) m_previewOpenBtn->setStyleSheet(sendBtnQss());
    if (m_chatContextLabel) m_chatContextLabel->setStyleSheet(chatContextQss());
    if (m_memoryChipLabel) m_memoryChipLabel->setStyleSheet(memoryChipQss());
    if (m_studioAvatarLabel) m_studioAvatarLabel->setStyleSheet(avatarQss());
    if (m_studioNameLabel) m_studioNameLabel->setStyleSheet(studioNameQss());
    if (m_studioSubtitleLabel) m_studioSubtitleLabel->setStyleSheet(studioSubtitleQss());
    if (m_railSearchEdit) m_railSearchEdit->setStyleSheet(railSearchQss());
    // Folders/sessões do rail usam cores do tema — reconstrói pra não ficar
    // com o QSS congelado da troca de tema anterior.
    rebuildProjectRail();

    // Bolhas já na tela (inclusive a que está em streaming neste instante).
    // Varre por objectName em vez de manter uma lista paralela de handles: as
    // bolhas e seus sub-widgets (rastro de pesquisas) são criados
    // dinamicamente e removidos com deleteLater, e a árvore de filhos já é a
    // fonte de verdade de "o que está na tela agora".
    if (m_transcriptContent) {
        const QList<QFrame*> bubbles = m_transcriptContent->findChildren<QFrame*>();
        for (QFrame* bubble : bubbles) {
            const QString name = bubble->objectName();
            if (name == QStringLiteral("chatEditCard")) {
                bubble->setStyleSheet(editCardQss());
                continue;
            }
            const bool isUser = (name == QStringLiteral("chatBubbleUser"));
            if (!isUser && name != QStringLiteral("chatBubbleMira")) continue;
            bubble->setStyleSheet(bubbleQss(isUser));

            const QString colorHex = bubbleTextColor(isUser);
            for (QTextEdit* te : bubble->findChildren<QTextEdit*>()) {
                if (te->objectName() != QStringLiteral("chatBubbleText")) continue;
                te->setStyleSheet(bubbleTextQss(colorHex));
                te->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
                // A cor do texto da bolha não vive só no QSS: ela é gravada no
                // QTextCharFormat do documento (ver forceBubbleTextColor), pra
                // não perder pra cor inline de conteúdo colado. Trocar só o QSS
                // deixaria o texto na cor do tema antigo.
                forceBubbleTextColor(te, colorHex);
            }
        }

        // Rastro de pesquisas: botão "▸ Ver pesquisas", linhas em itálico e
        // chips de documento clicáveis.
        for (QToolButton* b : m_transcriptContent->findChildren<QToolButton*>()) {
            const QString name = b->objectName();
            if (name == QStringLiteral("chatTraceToggle")) b->setStyleSheet(traceToggleQss());
            else if (name == QStringLiteral("chatTraceChip")) b->setStyleSheet(traceChipQss());
            else if (name == QStringLiteral("chatFeedbackBtn")) b->setStyleSheet(feedbackBtnQss());
        }
        for (QLabel* l : m_transcriptContent->findChildren<QLabel*>()) {
            const QString name = l->objectName();
            if (name == QStringLiteral("chatTraceText")) l->setStyleSheet(traceTextQss());
            else if (name == QStringLiteral("chatEditTitle")) l->setStyleSheet(editCardTitleQss());
            else if (name == QStringLiteral("chatEditText")) l->setStyleSheet(editCardTextQss());
            else if (name == QStringLiteral("chatEditStatus")) l->setStyleSheet(editCardStatusQss());
        }
        // Cartão de sugestão de edição: botões Aplicar/Descartar são
        // QPushButton (mesma classe do input de envio), não QToolButton.
        for (QPushButton* b : m_transcriptContent->findChildren<QPushButton*>()) {
            const QString name = b->objectName();
            if (name == QStringLiteral("chatEditDismissBtn")) b->setStyleSheet(chipQss());
            else if (name == QStringLiteral("chatEditApplyBtn")) b->setStyleSheet(sendBtnQss());
        }
    }
}

void AIChatPanel::setProjectRoot(const QString& root)
{
    // Trocar de projeto tem que resetar a conversa — sem isso, o chat do
    // projeto anterior continuava aparecendo (e sendo escrito) dentro do
    // projeto novo, e saveCurrentSession() acabava gravando a sessão do
    // projeto antigo na pasta ai_context/ do projeto errado (usa
    // m_projectRoot na hora de salvar, então precisa salvar ANTES de trocar
    // a raiz). Mesmo reset de startNewSession(), só que disparado pela
    // troca de projeto em vez de um clique explícito do usuário.
    if (root != m_projectRoot) {
        if (m_previewMode) exitPreviewMode(); // projeto mudou de verdade — preview não faz mais sentido
        saveCurrentSession(); // salva o que tinha, na pasta do projeto ANTIGO
        m_messages.clear();
        m_currentSessionId.clear();
        m_pendingToolTraces.clear();
        m_pendingBubbleImages.clear();
        m_pendingEditSuggestions.clear();
        m_toolHopCount = 0;
        clearTranscriptUi();
        if (m_docFocusCheck) m_docFocusCheck->setChecked(false); // combo tinha docs do projeto antigo
    }

    m_projectRoot = root;
    updateChatContext();
    if (m_projectRoot.isEmpty()) return;

    QSettings settings;
    if (!settings.value(QStringLiteral("ai/autoScanNewProjects"), false).toBool()) return;
    if (settings.value(QStringLiteral("ai/apiKey")).toString().isEmpty()) return;

    // "1ª vez" = ainda não existe resumo salvo pra este projeto específico.
    // Projetos já escaneados antes não disparam de novo sozinhos — só o
    // botão manual rescaneia um projeto que já tem resumo.
    if (QFile::exists(m_projectRoot + QStringLiteral("/ai_context/resumo_projeto.md"))) return;

    // Adiado — na hora de trocar de projeto, o ProjectModel pode ainda não
    // ter terminado de popular capítulos/gavetas; um tick de folga garante
    // que collectAllDocs() já vê tudo quando o scan realmente começar.
    QTimer::singleShot(500, this, [this]() {
        if (!m_scanning && m_client && !m_client->isBusy()) startProjectScan();
    });
}

void AIChatPanel::setTopInset(int px)
{
    m_topInset = px;
}

void AIChatPanel::setMarkerStore(MarkerStore* store)
{
    m_markerStore = store;
}

void AIChatPanel::setNotesStore(NotesStore* store)
{
    m_notesStore = store;
}

void AIChatPanel::setDialogueStore(DialogueStore* store)
{
    m_dialogueStore = store;
}

void AIChatPanel::setConstrutorStore(ConstrutorStore* store)
{
    m_construtorStore = store;
}

void AIChatPanel::setGlossaryStore(GlossaryStore* store)
{
    m_glossaryStore = store;
}

void AIChatPanel::setMapPinsStore(MapPinsStore* store)
{
    m_mapPinsStore = store;
}

void AIChatPanel::setKnownProjects(const QStringList& roots)
{
    m_knownProjectRoots = roots;
    rebuildProjectRail();
}

void AIChatPanel::setDocOpener(std::function<void(const QString&)> opener)
{
    m_docOpener = std::move(opener);
}

void AIChatPanel::setCurrentDocTitleProvider(std::function<QString()> provider)
{
    m_currentDocTitleProvider = std::move(provider);
}

void AIChatPanel::setActiveEditorProvider(std::function<QTextEdit*()> provider)
{
    m_activeEditorProvider = std::move(provider);
}

void AIChatPanel::setWordCounter(WordCounter* counter)
{
    m_wordCounter = counter;
    if (m_wordCounter) {
        connect(m_wordCounter, &WordCounter::progressChanged,
                this, &AIChatPanel::onWordCounterProgressChanged);
    }
}

void AIChatPanel::onWordCounterProgressChanged()
{
    if (!m_wordCounter) return;
    const QString dayKey = m_wordCounter->currentGoalDayKey();
    if (m_dailyGoalNotifiedDateKey == dayKey) return; // já avisou hoje, progressChanged() dispara toda hora
    if (!m_wordCounter->isGoalMet()) return;
    m_dailyGoalNotifiedDateKey = dayKey;

    // Mensagem instantânea, sem chamada de API — comemorar não precisa
    // esperar um round-trip, e um evento simples desses não vale o custo.
    static const QStringList kCelebrations = {
        tr("Meta do dia batida! 🎉 Parabéns, foi um baita progresso hoje."),
        tr("Aí sim! Meta de hoje concluída — bom trabalho. 🙌"),
        tr("Você bateu a meta de hoje! Vai comemorar, mereceu. ✨"),
    };
    const QString msg = kCelebrations.at(QRandomGenerator::global()->bounded(kCelebrations.size()));
    addMiraBubble(msg);
}

void AIChatPanel::togglePanel()
{
    if (isVisible()) closePanel();
    else openPanel();
}

void AIChatPanel::openPanel()
{
    if (!m_positioned) applyLayoutMode();
    show();
    raise();
    if (m_resizeGrip) m_resizeGrip->raise();
}

void AIChatPanel::closePanel()
{
    hide();
}

bool AIChatPanel::isPanelOpen() const
{
    return isVisible();
}

void AIChatPanel::showEvent(QShowEvent* e)
{
    QFrame::showEvent(e);
    if (!m_positioned) applyLayoutMode();
    if (m_resizeGrip) m_resizeGrip->raise();
    // O painel é criado uma única vez e só escondido/mostrado depois — se o
    // nome mudou em Settings enquanto ele já existia, o título só reflete
    // ao reabrir. Cobre o fluxo real (ninguém deixa o chat aberto olhando
    // pra Settings ao mesmo tempo).
    if (m_titleLabel) m_titleLabel->setText(miraAssistantName());
}

void AIChatPanel::resizeEvent(QResizeEvent* e)
{
    QFrame::resizeEvent(e);
    if (m_resizeGrip) {
        const int gs = m_resizeGrip->width();
        m_resizeGrip->move(width() - gs - 3, height() - gs - 3);
    }
    refitAllBubbles();
}

void AIChatPanel::ancorRight()
{
    QWidget* p = parentWidget();
    if (!p) return;
    const int top = m_topInset + kMargin;

    const QSize saved = QSettings().value(QStringLiteral("ai/chatPanelSize")).toSize();
    int w = (saved.isValid() && saved.width() >= 360)
        ? qMin(saved.width(), p->width() - kMargin * 2) : kPanelWidth;
    int h = (saved.isValid() && saved.height() >= 320)
        ? qMin(saved.height(), p->height() - top - kMargin)
        : qMax(320, p->height() - top - kMargin);

    resize(w, h);
    move(p->width() - w - kMargin, top);
    m_positioned = true;
}

void AIChatPanel::applyLayoutMode()
{
    QWidget* p = parentWidget();
    if (!p) return;

    // Coluna de projetos/conversas e a barra de identidade só fazem sentido
    // no espaço maior do modo janela — no modo painel (compacto), o
    // histórico continua no QMenu de sempre (showSessionMenu), sem mudança
    // de comportamento.
    if (m_railWidget) m_railWidget->setVisible(m_windowMode);
    if (m_historyBtn) m_historyBtn->setVisible(!m_windowMode);
    if (m_studioHeaderWidget) m_studioHeaderWidget->setVisible(m_windowMode);
    if (m_titleLabel) m_titleLabel->setVisible(!m_windowMode);
    if (m_memoryChipLabel) m_memoryChipLabel->setVisible(m_windowMode);
    if (m_windowMode) updateChatContext();

    if (m_windowMode) {
        // "Janela" — centralizada, maior, mais parecida com ChatGPT/Claude
        // (referência de tamanho pedida pelo usuário: diálogo de Temas).
        // Mínimo subiu de 480 pra 620: com a coluna de projetos/conversas
        // (230px fixos) ocupando espaço, 480 deixaria o chat espremido.
        const QSize saved = QSettings().value(QStringLiteral("ai/chatWindowSize")).toSize();
        int w = (saved.isValid() && saved.width() >= 360)
            ? qMin(saved.width(), p->width() - kMargin * 2)
            : qBound(620, int(p->width() * 0.62), 900);
        int h = (saved.isValid() && saved.height() >= 320)
            ? qMin(saved.height(), p->height() - kMargin * 2)
            : qBound(480, int(p->height() * 0.78), 680);
        resize(w, h);
        const int top = qMax(m_topInset + kMargin, (p->height() - h) / 2);
        move((p->width() - w) / 2, top);
    } else {
        ancorRight();
        return; // ancorRight() já marca m_positioned
    }
    m_positioned = true;
}

void AIChatPanel::toggleLayoutMode()
{
    m_windowMode = !m_windowMode;
    QSettings().setValue(QStringLiteral("ai/chatWindowMode"), m_windowMode);
    if (m_layoutBtn) {
        m_layoutBtn->setToolTip(m_windowMode
            ? tr("Voltar pro modo painel (ancorado à direita)")
            : tr("Modo janela (centralizada, maior)"));
    }
    applyLayoutMode();
    if (m_windowMode) rebuildProjectRail();
}

void AIChatPanel::saveCurrentPanelSize()
{
    QSettings().setValue(
        m_windowMode ? QStringLiteral("ai/chatWindowSize") : QStringLiteral("ai/chatPanelSize"),
        size());
}

QString AIChatPanel::loadProjectSummaryFile() const
{
    if (m_projectRoot.isEmpty()) return QString();
    QFile f(m_projectRoot + QStringLiteral("/ai_context/resumo_projeto.md"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    return in.readAll();
}

QString AIChatPanel::loadProjectMemoryFile() const
{
    if (m_projectRoot.isEmpty()) return QString();
    QFile f(m_projectRoot + QStringLiteral("/ai_context/memoria_mira.md"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    return in.readAll();
}

QString AIChatPanel::buildDailyProgressSummary() const
{
    if (!m_wordCounter) return QString();

    const WordCounterSettings settings = m_wordCounter->settings();
    const bool met = m_wordCounter->isGoalMet();

    QString goalText;
    if (settings.goalType == QStringLiteral("time")) {
        const qint64 minutes = m_wordCounter->progressTimeMs() / 60000;
        goalText = tr("%1/%2 minutos escritos hoje").arg(minutes).arg(settings.goalTargetMinutes);
    } else {
        goalText = tr("%1/%2 palavras escritas hoje")
            .arg(m_wordCounter->progressWords()).arg(settings.goalTargetWords);
    }

    return tr("Progresso de escrita: %1 (meta %2). Sequência atual de dias com meta batida: %3. "
               "Recorde de sequência: %4.")
        .arg(goalText, met ? tr("BATIDA ✅") : tr("ainda não batida"))
        .arg(m_wordCounter->currentStreak())
        .arg(m_wordCounter->longestStreak());
}

QString AIChatPanel::buildSystemPrompt() const
{
    QString base = miraPersonalityPrompt(miraAssistantName());
    base += miraPersonalityAdjustmentFragment(
        QSettings().value(QStringLiteral("ai/personalityWarmth"), 50).toInt(),
        QSettings().value(QStringLiteral("ai/personalityHarshness"), 50).toInt(),
        QSettings().value(QStringLiteral("ai/personalityFreeform")).toString());
    base += miraTraitsFragment(QSettings().value(QStringLiteral("ai/personalityTraits")).toStringList());
    base += miraCompanionshipFragment();
    base += miraResponseDensityFragment();
    base += miraMarkdownFormattingFragment();
    base += MiraStyleStore::buildPromptFragment();
    base += QStringLiteral(
        "\n\nVocê está conversando livremente com o autor sobre o projeto "
        "dele — não é uma revisão pontual de um trecho específico do "
        "editor. Responda sempre no idioma em que o usuário escreve.");

    const QString summary = loadProjectSummaryFile();
    if (!summary.isEmpty()) {
        base += QStringLiteral(
            "\n\nContexto do projeto (resumo gerado a partir da leitura de "
            "todos os documentos — pode estar incompleto ou desatualizado "
            "se o projeto mudou depois da última leitura):\n\n%1"
            "\n\nEsse resumo é compacto e pode não ter um detalhe muito "
            "específico (um nome próprio pontual, um número, uma frase "
            "exata, uma marca, algo gravado num objeto).").arg(summary);
    } else {
        base += QStringLiteral(
            "\n\nVocê ainda não tem um resumo geral do projeto (o usuário "
            "nunca clicou em \"Ler documentos do projeto\"). Isso NÃO "
            "significa que você não tem acesso a nada — search_project e "
            "read_document funcionam normalmente mesmo sem esse resumo, "
            "porque leem o texto bruto direto do projeto, não o resumo. "
            "Nunca diga \"não sei\" só porque falta o resumo geral: use as "
            "ferramentas de busca do mesmo jeito antes de desistir.");
    }

    // Regra de uso das tools — vale INDEPENDENTE de já existir resumo geral
    // ou não (bug corrigido 2026-07-29: essa regra vivia só dentro do "if"
    // acima, então num projeto sem scan ainda ela desistia na hora, sem
    // nunca tentar buscar — ver memória ai-assistant-feature).
    base += QStringLiteral(
        "\n\nREGRA OBRIGATÓRIA: antes de responder \"não sei\" ou \"não "
        "encontrei\" para qualquer pergunta sobre um fato específico do "
        "projeto (um sistema do Construtor, um termo do Glossário, um "
        "personagem, um objeto, uma cena) que não esteja LITERALMENTE "
        "escrito no contexto acima, você DEVE chamar a função "
        "search_project pelo menos uma vez com o termo mais provável (ex.: "
        "o nome citado pelo usuário, a palavra-chave central da pergunta). "
        "Se a primeira busca não achar nada, tente pelo menos mais um termo "
        "relacionado antes de desistir (sinônimo, variação, palavra mais "
        "genérica). A busca é por palavra solta (não frase literal) — se os "
        "trechos retornados mencionarem um termo mais específico e "
        "promissor que você ainda não buscou, chame a função DE NOVO só com "
        "esse termo antes de responder — siga a pista até o fim em vez de "
        "parar na primeira busca genérica. Só diga que não encontrou DEPOIS "
        "de ter chamado a função de verdade — nunca alegue ter buscado ou "
        "\"analisado o texto\" sem ter chamado a função search_project de "
        "fato nesta mesma resposta.\n\n"
        "search_project só acha trechos ISOLADOS por palavra — ela NÃO "
        "resolve referência indireta (ex.: um personagem descrito como \"a "
        "mulher\" ou \"a tal investigadora\" bem antes de ser nomeado no "
        "mesmo capítulo). Se os trechos de search_project indicarem QUAL "
        "documento é relevante mas não derem certeza suficiente pra "
        "responder, chame read_document com o título daquele documento pra "
        "ler ele por inteiro antes de responder ou de desistir.");

    const QString memory = loadProjectMemoryFile();
    if (!memory.isEmpty()) {
        base += QStringLiteral(
            "\n\nMemória registrada do projeto (notas que você mesma foi "
            "salvando ao longo das conversas, cada uma com uma categoria e "
            "um status):\n\n%1"
            "\n\nStatus tem peso real, não é decoração: 🟢 confirmada = fato "
            "estabelecido da obra, trate como verdade. 🟡 em_discussao = só "
            "foi cogitado em conversa, NÃO é decisão fechada — nunca "
            "apresente como fato consumado (\"você estabeleceu que...\"), "
            "sempre enquadre como possibilidade ainda em aberto. 🔵 "
            "ideia_futura = ideia solta sem compromisso nenhum. 🔴 "
            "descartada = já foi rejeitado — se o assunto voltar à tona, "
            "avise que essa ideia específica já foi descartada antes em vez "
            "de sugerir de novo como se fosse nova.")
            .arg(memory);
    }

    const QString userMemory = MiraUserMemoryStore::load();
    if (!userMemory.isEmpty()) {
        base += QStringLiteral(
            "\n\nMemória sobre o AUTOR (não sobre este projeto — vale em "
            "QUALQUER projeto, presente ou futuro; notas que você mesma "
            "foi salvando ao longo do tempo com save_user_note):\n\n%1"
            "\n\nMesmas regras de peso do status que valem pra memória do "
            "projeto acima se aplicam aqui.").arg(userMemory);
    }

    if (m_docFocusCheck && m_docFocusCheck->isChecked() &&
        m_docFocusCombo && m_docFocusCombo->currentIndex() >= 0) {
        const QString focusTitle = m_docFocusCombo->currentText();
        const QVector<ScanDoc> docs = collectAllDocs();
        const ScanDoc* match = findDocByTitle(docs, focusTitle);
        if (match) {
            base += QStringLiteral(
                "\n\nFOCO DE DOCUMENTO ATIVO: o autor marcou pra você focar "
                "especificamente no documento \"%1\" nesta conversa. "
                "Priorize esse texto completo abaixo pra responder — ele "
                "complementa (não substitui) o resumo geral e a memória. "
                "Ainda assim, use search_project/read_document se a "
                "conversa precisar de outro documento.\n\nTexto completo de "
                "\"%1\":\n\n%2").arg(match->title, match->plainText.left(kMaxCharsPerDoc));
        }
    }

    base += QStringLiteral(
        "\n\nCITAÇÃO DE ORIGEM: sempre que afirmar algo sobre o projeto que "
        "veio de search_project, read_document ou da memória registrada "
        "acima, diga de onde veio (ex.: \"no Capítulo 3...\", \"segundo a "
        "nota registrada sobre X...\"). Nunca apresente informação do "
        "projeto como se fosse conhecimento seu do nada — a origem faz "
        "parte da resposta, não é opcional.");

    const QString progress = buildDailyProgressSummary();
    if (!progress.isEmpty()) {
        base += QStringLiteral("\n\n%1").arg(progress);
    }

    return base;
}

void AIChatPanel::pickAttachImage()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Selecionar imagem"),
        QString(), tr("Imagens (*.png *.jpg *.jpeg *.gif *.bmp *.webp)"));
    if (path.isEmpty()) return;

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage img = reader.read();
    if (img.isNull()) {
        QMessageBox::warning(this, windowTitle(), tr("Não foi possível carregar essa imagem."));
        return;
    }

    m_pendingAttachImage = img;
    m_attachThumb->setPixmap(QPixmap::fromImage(img).scaled(
        40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_attachPreviewRow->setVisible(true);
}

void AIChatPanel::clearAttachImage()
{
    m_pendingAttachImage = QImage();
    m_attachThumb->setPixmap(QPixmap());
    m_attachPreviewRow->setVisible(false);
}

void AIChatPanel::sendUserMessage(const QString& text)
{
    if (text.trimmed().isEmpty()) return;
    if (m_client->isBusy()) return;

    QSettings settings;
    const QString apiKey = settings.value(QStringLiteral("ai/apiKey")).toString();
    if (apiKey.isEmpty()) {
        addMiraBubble(tr("Nenhuma chave de API configurada. Abra Configurações → Assistente de IA e cole sua chave."));
        return;
    }
    m_client->setApiKey(apiKey);
    m_client->setBaseUrl(settings.value(QStringLiteral("ai/baseUrl"),
        QStringLiteral("https://api.openai.com/v1")).toString());
    m_client->setModel(settings.value(QStringLiteral("ai/model"),
        QStringLiteral("gpt-4o-mini")).toString());
    m_client->setTools({ searchProjectTool(), readDocumentTool(), saveProjectNoteTool(),
                         saveUserNoteTool(), resummarizeDocumentTool(), lookupWorldDataTool(),
                         generateCharacterImageTool(), generateSceneImageTool(),
                         proposeDocumentEditTool() });

    if (m_messages.isEmpty()) {
        AIChatMessage sys;
        sys.role = QStringLiteral("system");
        sys.content = buildSystemPrompt();
        m_messages.append(sys);
    }

    // Imagem anexada (opcional): encoda pro mesmo campo multimodal que o
    // AIClient já sabe serializar — é o único caso em que isso é correto
    // (o resultado É conteúdo real da mensagem do usuário, deve persistir).
    QString imageDataUrl;
    if (!m_pendingAttachImage.isNull()) {
        imageDataUrl = AvatarUtils::encodeDataUrl(m_pendingAttachImage, 1536, 85);
    }

    AIChatMessage userMsg;
    userMsg.role = QStringLiteral("user");
    userMsg.content = text;
    userMsg.imageDataUrl = imageDataUrl;
    m_messages.append(userMsg);

    addUserBubble(text, imageDataUrl);
    updateChatContext(); // 1ª mensagem de uma conversa nova já vira título na barra de contexto
    clearAttachImage();
    m_inputEdit->clear();
    m_inputEdit->setFixedHeight(38);

    setBusy(true);
    m_assistantTurnOpen = false;
    m_toolHopCount = 0;
    m_pendingToolTraces.clear();
    m_client->sendMessage(m_messages);
}

void AIChatPanel::handleToolCall(const QString& id, const QString& name, const QJsonObject& arguments)
{
    if (name == QStringLiteral("search_project")) {
        handleSearchProjectTool(id, arguments);
    } else if (name == QStringLiteral("read_document")) {
        handleReadDocumentTool(id, arguments);
    } else if (name == QStringLiteral("save_project_note")) {
        handleSaveProjectNoteTool(id, arguments);
    } else if (name == QStringLiteral("save_user_note")) {
        handleSaveUserNoteTool(id, arguments);
    } else if (name == QStringLiteral("resummarize_document")) {
        handleResummarizeDocumentTool(id, arguments);
    } else if (name == QStringLiteral("lookup_world_data")) {
        handleLookupWorldDataTool(id, arguments);
    } else if (name == QStringLiteral("generate_character_image")) {
        handleGenerateCharacterImageTool(id, arguments);
    } else if (name == QStringLiteral("generate_scene_image")) {
        handleGenerateSceneImageTool(id, arguments);
    } else if (name == QStringLiteral("propose_document_edit")) {
        handleProposeDocumentEditTool(id, arguments);
    } else {
        m_pendingToolCall = false;
    }
}

void AIChatPanel::handleSearchProjectTool(const QString& id, const QJsonObject& arguments)
{
    const QString query = arguments.value(QStringLiteral("query")).toString();
    const SearchResult result = runProjectSearch(query);

    // Não vai mais direto na conversa (poluía muito) — fica recolhido
    // dentro da bolha final, atrás da setinha "Ver pesquisas". O log em
    // disco continua com o detalhe completo, pra debug.
    const QString traceText = tr("🔍 Buscando \"%1\"…\n%2").arg(query, result.text);
    logConversation(miraAssistantName(), traceText);
    ToolTraceEntry entry;
    entry.text = traceText;
    entry.docTitles = result.docTitles;
    m_pendingToolTraces.append(entry);

    QJsonObject argsEcho;
    argsEcho[QStringLiteral("query")] = query;
    finishToolRoundTrip(id, QStringLiteral("search_project"), argsEcho, result.text);
}

void AIChatPanel::handleReadDocumentTool(const QString& id, const QJsonObject& arguments)
{
    const QString title = arguments.value(QStringLiteral("title")).toString();
    const QString question = arguments.value(QStringLiteral("question")).toString();

    QJsonObject argsEcho;
    argsEcho[QStringLiteral("title")] = title;
    argsEcho[QStringLiteral("question")] = question;

    const QVector<ScanDoc> docs = collectAllDocs();
    const ScanDoc* match = findDocByTitle(docs, title);
    if (!match) {
        QStringList titles;
        for (const ScanDoc& d : docs) titles.append(d.title);
        const QString resultText = tr("Nenhum documento com título parecido com \"%1\" foi encontrado. "
            "Títulos disponíveis: %2").arg(title, titles.join(QStringLiteral(", ")));
        const QString traceText = tr("📖 Tentando ler o documento \"%1\"…\n%2").arg(title, resultText);
        logConversation(miraAssistantName(), traceText);
        ToolTraceEntry entry;
        entry.text = traceText;
        m_pendingToolTraces.append(entry);
        finishToolRoundTrip(id, QStringLiteral("read_document"), argsEcho, resultText);
        return;
    }

    const QString traceStart = tr("📖 Lendo o documento inteiro \"%1\" pra responder: %2").arg(match->title, question);
    logConversation(miraAssistantName(), traceStart);
    ToolTraceEntry entry;
    entry.text = traceStart;
    if (!match->key.isEmpty()) entry.docTitles.append(match->title);
    m_pendingToolTraces.append(entry);
    refreshThinkingBubbleDetails();

    // Sub-chamada efêmera, num AIClient próprio e descartável: o texto
    // bruto do capítulo (até 100k chars) NÃO entra no histórico principal
    // (m_messages) — só a resposta já destilada entra. Sem isso, cada
    // pergunta futura na mesma conversa reenviaria o capítulo inteiro de
    // novo pra sempre, inflando custo sem necessidade. É aqui que a
    // compreensão de leitura de verdade acontece — conectar "a mulher que
    // desceu do Civic" a "Klara" exige ler a cena inteira, não dá pra
    // resolver com busca por palavra (ver ai-assistant-feature na memória).
    auto* reader = new AIClient(this);
    QSettings settings;
    reader->setApiKey(settings.value(QStringLiteral("ai/apiKey")).toString());
    reader->setBaseUrl(settings.value(QStringLiteral("ai/baseUrl"),
        QStringLiteral("https://api.openai.com/v1")).toString());
    reader->setModel(settings.value(QStringLiteral("ai/model"),
        QStringLiteral("gpt-4o-mini")).toString());

    AIChatMessage sys;
    sys.role = QStringLiteral("system");
    sys.content = QStringLiteral(
        "Leia o documento a seguir por inteiro e responda à pergunta com base "
        "SOMENTE no que está escrito nele. Preste atenção especial a "
        "referências indiretas — um personagem pode ser descrito antes de "
        "ser nomeado (\"a mulher\", \"a tal investigadora\", um pronome) — "
        "conecte essas referências ao nome certo usando o contexto ao redor "
        "antes de responder. Se a resposta não estiver no documento, diga "
        "isso claramente em vez de arriscar um palpite. Responda em "
        "português, direto, sem preâmbulo.");

    AIChatMessage user;
    user.role = QStringLiteral("user");
    user.content = QStringLiteral("Pergunta: %1\n\nDocumento (\"%2\"):\n\n%3")
        .arg(question.isEmpty() ? tr("(sem pergunta específica — resuma o que for relevante)") : question,
             match->title, match->plainText.left(kMaxCharsPerDoc));

    connect(reader, &AIClient::finished, this, [this, reader, id, argsEcho](const QString& answer) {
        finishToolRoundTrip(id, QStringLiteral("read_document"), argsEcho, answer);
        reader->deleteLater();
    });
    connect(reader, &AIClient::errorOccurred, this, [this, reader, id, argsEcho](const QString& err) {
        finishToolRoundTrip(id, QStringLiteral("read_document"), argsEcho,
            tr("Erro ao ler o documento: %1").arg(err));
        reader->deleteLater();
    });

    reader->sendMessage({ sys, user });
}

void AIChatPanel::handleSaveProjectNoteTool(const QString& id, const QJsonObject& arguments)
{
    const QString category = arguments.value(QStringLiteral("category")).toString();
    const QString status = arguments.value(QStringLiteral("status")).toString();
    const QString title = arguments.value(QStringLiteral("title")).toString();
    const QString content = arguments.value(QStringLiteral("content")).toString();

    static const QHash<QString, QString> kCategoryLabels = {
        { QStringLiteral("canon"), tr("Cânone confirmado") },
        { QStringLiteral("personagem"), tr("Personagens") },
        { QStringLiteral("lore"), tr("Lore e mundo") },
        { QStringLiteral("planejamento"), tr("Planejamento") },
        { QStringLiteral("ideia"), tr("Ideias em desenvolvimento") },
        { QStringLiteral("pendencia"), tr("Pendências") },
        { QStringLiteral("preferencia"), tr("Preferências do autor") },
    };
    static const QHash<QString, QString> kStatusEmoji = {
        { QStringLiteral("confirmada"), QStringLiteral("🟢") },
        { QStringLiteral("em_discussao"), QStringLiteral("🟡") },
        { QStringLiteral("ideia_futura"), QStringLiteral("🔵") },
        { QStringLiteral("descartada"), QStringLiteral("🔴") },
    };
    const QString categoryLabel = kCategoryLabels.value(category, category);
    const QString statusEmoji = kStatusEmoji.value(status, QStringLiteral("⚪"));

    const QString dirPath = m_projectRoot + QStringLiteral("/ai_context");
    QDir().mkpath(dirPath);
    QFile f(dirPath + QStringLiteral("/memoria_mira.md"));
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&f);
        out.setEncoding(QStringConverter::Utf8);
        out << QStringLiteral("- **[%1]** %2 **%3** — %4 _(%5)_\n\n")
            .arg(categoryLabel, statusEmoji, title, content,
                 QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
    }

    const QString traceText = tr("💾 Nota salva — %1 %2: \"%3\" — %4")
        .arg(statusEmoji, categoryLabel, title, content);
    logConversation(miraAssistantName(), traceText);
    ToolTraceEntry entry;
    entry.text = traceText;
    m_pendingToolTraces.append(entry);

    // Atualiza o system prompt já nesta conversa — sem isso, a nota só
    // entraria no contexto na PRÓXIMA vez que o chat fosse aberto do zero.
    if (!m_messages.isEmpty() && m_messages.first().role == QStringLiteral("system")) {
        m_messages[0].content = buildSystemPrompt();
    }

    QJsonObject argsEcho;
    argsEcho[QStringLiteral("category")] = category;
    argsEcho[QStringLiteral("status")] = status;
    argsEcho[QStringLiteral("title")] = title;
    argsEcho[QStringLiteral("content")] = content;

    finishToolRoundTrip(id, QStringLiteral("save_project_note"), argsEcho,
        tr("Nota salva com sucesso: [%1] %2 \"%3\".").arg(categoryLabel, statusEmoji, title));
}

void AIChatPanel::handleSaveUserNoteTool(const QString& id, const QJsonObject& arguments)
{
    const QString category = arguments.value(QStringLiteral("category")).toString();
    const QString status = arguments.value(QStringLiteral("status")).toString();
    const QString title = arguments.value(QStringLiteral("title")).toString();
    const QString content = arguments.value(QStringLiteral("content")).toString();

    static const QHash<QString, QString> kCategoryLabels = {
        { QStringLiteral("preferencia_criativa"), tr("Preferência criativa") },
        { QStringLiteral("processo"), tr("Jeito de trabalhar") },
        { QStringLiteral("ideia_solta"), tr("Ideia solta") },
        { QStringLiteral("vinculo"), tr("Vínculo") },
        { QStringLiteral("pendencia_geral"), tr("Pendência geral") },
    };
    static const QHash<QString, QString> kStatusEmoji = {
        { QStringLiteral("confirmada"), QStringLiteral("🟢") },
        { QStringLiteral("em_discussao"), QStringLiteral("🟡") },
        { QStringLiteral("ideia_futura"), QStringLiteral("🔵") },
        { QStringLiteral("descartada"), QStringLiteral("🔴") },
    };
    const QString categoryLabel = kCategoryLabels.value(category, category);
    const QString statusEmoji = kStatusEmoji.value(status, QStringLiteral("⚪"));

    MiraUserMemoryStore::appendEntry(category, status, title, content);

    const QString traceText = tr("💾 Nota sobre você salva — %1 %2: \"%3\" — %4")
        .arg(statusEmoji, categoryLabel, title, content);
    logConversation(miraAssistantName(), traceText);
    ToolTraceEntry entry;
    entry.text = traceText;
    m_pendingToolTraces.append(entry);

    // Mesmo motivo do save_project_note: atualiza o system prompt já nesta
    // conversa, senão a nota só entraria no contexto na próxima sessão.
    if (!m_messages.isEmpty() && m_messages.first().role == QStringLiteral("system")) {
        m_messages[0].content = buildSystemPrompt();
    }

    QJsonObject argsEcho;
    argsEcho[QStringLiteral("category")] = category;
    argsEcho[QStringLiteral("status")] = status;
    argsEcho[QStringLiteral("title")] = title;
    argsEcho[QStringLiteral("content")] = content;

    finishToolRoundTrip(id, QStringLiteral("save_user_note"), argsEcho,
        tr("Nota sobre o autor salva com sucesso: [%1] %2 \"%3\".").arg(categoryLabel, statusEmoji, title));
}

void AIChatPanel::handleResummarizeDocumentTool(const QString& id, const QJsonObject& arguments)
{
    const QString title = arguments.value(QStringLiteral("title")).toString();
    QJsonObject argsEcho;
    argsEcho[QStringLiteral("title")] = title;

    const QVector<ScanDoc> docs = collectAllDocs();
    const ScanDoc* match = findDocByTitle(docs, title);
    if (!match) {
        const QString resultText = tr("Nenhum documento com título parecido com \"%1\" foi encontrado.").arg(title);
        logConversation(miraAssistantName(), resultText);
        ToolTraceEntry entry;
        entry.text = resultText;
        m_pendingToolTraces.append(entry);
        finishToolRoundTrip(id, QStringLiteral("resummarize_document"), argsEcho, resultText);
        return;
    }

    const QString traceText = tr("🔄 Relendo \"%1\" por inteiro pra atualizar o resumo…").arg(match->title);
    logConversation(miraAssistantName(), traceText);
    ToolTraceEntry entry;
    entry.text = traceText;
    if (!match->key.isEmpty()) entry.docTitles.append(match->title);
    m_pendingToolTraces.append(entry);
    refreshThinkingBubbleDetails();

    // Mesma lógica de sub-chamada efêmera do read_document — o texto bruto
    // do documento não entra no histórico persistente, só o resumo final.
    auto* reader = new AIClient(this);
    QSettings settings;
    reader->setApiKey(settings.value(QStringLiteral("ai/apiKey")).toString());
    reader->setBaseUrl(settings.value(QStringLiteral("ai/baseUrl"),
        QStringLiteral("https://api.openai.com/v1")).toString());
    reader->setModel(settings.value(QStringLiteral("ai/model"),
        QStringLiteral("gpt-4o-mini")).toString());

    AIChatMessage sys;
    sys.role = QStringLiteral("system");
    sys.content = docSummaryInstruction();

    AIChatMessage user;
    user.role = QStringLiteral("user");
    user.content = QStringLiteral("Título: %1\n\n%2").arg(match->title, match->plainText.left(kMaxCharsPerDoc));

    const QString resolvedTitle = match->title;
    connect(reader, &AIClient::finished, this, [this, reader, id, argsEcho, resolvedTitle](const QString& summary) {
        QVector<QPair<QString, QString>> sections = parseSummaryFile(loadProjectSummaryFile());
        bool replaced = false;
        for (auto& pair : sections) {
            if (pair.first == resolvedTitle) { pair.second = summary.trimmed(); replaced = true; break; }
        }
        if (!replaced) sections.append({ resolvedTitle, summary.trimmed() });
        writeSummaryFile(sections);

        if (!m_messages.isEmpty() && m_messages.first().role == QStringLiteral("system")) {
            m_messages[0].content = buildSystemPrompt();
        }

        finishToolRoundTrip(id, QStringLiteral("resummarize_document"), argsEcho,
            tr("Resumo de \"%1\" atualizado com sucesso.").arg(resolvedTitle));
        reader->deleteLater();
    });
    connect(reader, &AIClient::errorOccurred, this, [this, reader, id, argsEcho](const QString& err) {
        finishToolRoundTrip(id, QStringLiteral("resummarize_document"), argsEcho,
            tr("Erro ao reler o documento: %1").arg(err));
        reader->deleteLater();
    });

    reader->sendMessage({ sys, user });
}

void AIChatPanel::handleLookupWorldDataTool(const QString& id, const QJsonObject& arguments)
{
    const QString query = arguments.value(QStringLiteral("query")).toString();
    const QString resultText = runWorldDataLookup(query);

    const QString traceText = tr("🌍 Consultando dados geográficos reais de \"%1\"…\n%2").arg(query, resultText);
    logConversation(miraAssistantName(), traceText);
    ToolTraceEntry entry;
    entry.text = traceText;
    m_pendingToolTraces.append(entry);

    QJsonObject argsEcho;
    argsEcho[QStringLiteral("query")] = query;
    finishToolRoundTrip(id, QStringLiteral("lookup_world_data"), argsEcho, resultText);
}

void AIChatPanel::handleGenerateCharacterImageTool(const QString& id, const QJsonObject& arguments)
{
    const QString characterName = arguments.value(QStringLiteral("character_name")).toString();
    const QString description = arguments.value(QStringLiteral("description")).toString();
    const QString styleKey = arguments.value(QStringLiteral("style")).toString();
    const QString finalPrompt = arguments.value(QStringLiteral("final_prompt")).toString().trimmed();

    QJsonObject argsEcho;
    argsEcho[QStringLiteral("character_name")] = characterName;
    argsEcho[QStringLiteral("description")] = description;
    argsEcho[QStringLiteral("style")] = styleKey;
    if (!finalPrompt.isEmpty()) argsEcho[QStringLiteral("final_prompt")] = finalPrompt;

    // Resolve o personagem: item de gaveta com elementType=="character" cujo
    // Element vinculado bate com character_name — exato primeiro, depois
    // parcial (nome ou apelido), mesma tolerância case-insensitive de
    // findDocByTitle, mas restrita a personagens.
    QString resolvedItemId;
    const Element* resolvedElement = nullptr;
    if (m_projectModel && m_elementsStore) {
        for (const Drawer& drawer : m_projectModel->drawers()) {
            for (const DrawerItem& item : drawer.items) {
                if (item.elementType != QStringLiteral("character")) continue;
                const Element* e = m_elementsStore->findElement(item.elementId);
                if (!e) continue;
                if (e->name.compare(characterName, Qt::CaseInsensitive) == 0) {
                    resolvedItemId = item.id;
                    resolvedElement = e;
                }
            }
        }
        if (!resolvedElement) {
            for (const Drawer& drawer : m_projectModel->drawers()) {
                for (const DrawerItem& item : drawer.items) {
                    if (item.elementType != QStringLiteral("character")) continue;
                    const Element* e = m_elementsStore->findElement(item.elementId);
                    if (!e) continue;
                    bool matches = e->name.contains(characterName, Qt::CaseInsensitive)
                        || characterName.contains(e->name, Qt::CaseInsensitive);
                    if (!matches) {
                        for (const QString& alias : e->aliases) {
                            if (alias.contains(characterName, Qt::CaseInsensitive)
                                || characterName.contains(alias, Qt::CaseInsensitive)) {
                                matches = true;
                                break;
                            }
                        }
                    }
                    if (matches) { resolvedItemId = item.id; resolvedElement = e; break; }
                }
                if (resolvedElement) break;
            }
        }
    }

    if (!resolvedElement) {
        const QString resultText = tr(
            "Nenhum personagem chamado \"%1\" foi encontrado no projeto. "
            "Crie o personagem primeiro antes de gerar a imagem.").arg(characterName);
        logConversation(miraAssistantName(), resultText);
        finishToolRoundTrip(id, QStringLiteral("generate_character_image"), argsEcho, resultText);
        return;
    }

    // Funciona tanto pra ficha estruturada quanto pra documento livre —
    // resolveDrawerItemHtml já sintetiza a ficha em html quando isSheet, ou
    // devolve o html do documento livre quando não é (mesma função já usada
    // em collectAllDocs() pra indexar fichas como "documentos").
    const DrawerItem* item = m_projectModel->findDrawerItem(resolvedItemId);
    QString sheetContext;
    if (item) {
        const QString html = DocPreview::resolveDrawerItemHtml(item, m_elementsStore, m_docCache,
            m_projectRoot, /*includePhoto=*/false);
        sheetContext = stripHtmlToPlainText(html);
    }

    const QString traceText = !finalPrompt.isEmpty()
        ? tr("🎨 Gerando imagem pra \"%1\" com prompt fornecido pelo autor (sem reescrita).")
              .arg(resolvedElement->name)
        : tr("🎨 Gerando imagem pra \"%1\": %2").arg(resolvedElement->name, description);
    logConversation(miraAssistantName(), traceText);
    ToolTraceEntry entry;
    entry.text = traceText;
    m_pendingToolTraces.append(entry);
    refreshThinkingBubbleDetails();

    QSettings settings;
    const QString imageModel = settings.value(QStringLiteral("ai/imageModel"),
        QStringLiteral("gpt-image-1-mini")).toString();
    const QString imageQuality = settings.value(QStringLiteral("ai/imageQuality"),
        QStringLiteral("medium")).toString();
    const QString imageSize = settings.value(QStringLiteral("ai/imageSize"),
        QStringLiteral("1024x1024")).toString();

    auto* service = new CharacterImageGenService(this);
    const QString itemId = resolvedItemId;
    const QString charName = resolvedElement->name;
    // Compartilhado entre os 2 lambdas abaixo: promptEngineered chega ANTES
    // de imageReady no mesmo fluxo, e imageReady precisa desse texto pra
    // registrar na galeria (GeneratedImageGallery). Em modo manual
    // (finalPrompt preenchido) promptEngineered nunca dispara — usa
    // finalPrompt direto nesse caso.
    auto promptHolder = std::make_shared<QString>();

    connect(service, &CharacterImageGenService::imageReady, this,
            [this, service, id, argsEcho, itemId, charName, finalPrompt, promptHolder](const QImage& img) {
        // Headless: não há usuário no loop pra ajustar o crop, então usa o
        // recorte automático (quadrado central), igual ao caminho manual
        // faria se o usuário só clicasse "usar" sem mexer no quadrado.
        const QString dataUrl = ImageCropDialog::autoSquareDataUrl(img);
        // Efêmero, só pra exibir na bolha final — NUNCA gravado em
        // AIChatMessage/m_messages (ver comentário em m_pendingBubbleImages).
        // Resolução plena (não o recorte quadrado) — melhor pro "ver grande".
        m_pendingBubbleImages.append(img);
        GeneratedImageGallery::save(m_projectRoot, img, charName,
            finalPrompt.isEmpty() ? *promptHolder : finalPrompt);
        if (!dataUrl.isEmpty() && m_elementsStore) {
            const DrawerItem* freshItem = m_projectModel->findDrawerItem(itemId);
            if (freshItem) {
                if (const Element* cur = m_elementsStore->findElement(freshItem->elementId)) {
                    Element copy = *cur;
                    copy.image = dataUrl;
                    m_elementsStore->updateElement(freshItem->elementId, copy);
                }
            }
        }
        emit characterImageUpdated(itemId);
        finishToolRoundTrip(id, QStringLiteral("generate_character_image"), argsEcho,
            tr("Imagem gerada e salva como foto de \"%1\".").arg(charName));
        service->deleteLater();
    });
    connect(service, &CharacterImageGenService::errorOccurred, this,
            [this, service, id, argsEcho](const QString& err) {
        finishToolRoundTrip(id, QStringLiteral("generate_character_image"), argsEcho,
            tr("Erro ao gerar imagem: %1").arg(err));
        service->deleteLater();
    });
    // Trace próprio pro prompt engenhado — some junto do resto atrás do
    // "▸ Ver pesquisas" da bolha, mesmo mecanismo já usado por
    // search_project/read_document. Sem isso o prompt real que vai pro GPT
    // Image fica invisível pro autor.
    connect(service, &CharacterImageGenService::promptEngineered, this,
            [this, promptHolder](const QString& prompt) {
        *promptHolder = prompt;
        ToolTraceEntry entry;
        entry.text = tr("📝 Prompt gerado: %1").arg(prompt);
        m_pendingToolTraces.append(entry);
    });

    if (!finalPrompt.isEmpty()) {
        service->generateFromRawPrompt(finalPrompt, imageModel, imageQuality, imageSize);
    } else {
        service->generate(description, imageStylePresetFromKey(styleKey), sheetContext,
            imageModel, imageQuality, imageSize);
    }
}

void AIChatPanel::handleGenerateSceneImageTool(const QString& id, const QJsonObject& arguments)
{
    const QString description = arguments.value(QStringLiteral("description")).toString();
    const QString styleKey = arguments.value(QStringLiteral("style")).toString();
    const QString finalPrompt = arguments.value(QStringLiteral("final_prompt")).toString().trimmed();

    QJsonObject argsEcho;
    argsEcho[QStringLiteral("description")] = description;
    argsEcho[QStringLiteral("style")] = styleKey;
    if (!finalPrompt.isEmpty()) argsEcho[QStringLiteral("final_prompt")] = finalPrompt;

    const QString traceText = !finalPrompt.isEmpty()
        ? tr("🎨 Gerando imagem de cena com prompt fornecido pelo autor (sem reescrita).")
        : tr("🎨 Gerando imagem de cena: %1").arg(description);
    logConversation(miraAssistantName(), traceText);
    ToolTraceEntry entry;
    entry.text = traceText;
    m_pendingToolTraces.append(entry);
    refreshThinkingBubbleDetails();

    QSettings settings;
    const QString imageModel = settings.value(QStringLiteral("ai/imageModel"),
        QStringLiteral("gpt-image-1-mini")).toString();
    const QString imageQuality = settings.value(QStringLiteral("ai/imageQuality"),
        QStringLiteral("medium")).toString();
    const QString imageSize = settings.value(QStringLiteral("ai/imageSize"),
        QStringLiteral("1024x1024")).toString();

    auto* service = new CharacterImageGenService(this);
    // Mesmo compartilhamento entre lambdas de handleGenerateCharacterImageTool:
    // promptEngineered chega antes de imageReady, que precisa do texto pra
    // registrar na galeria.
    auto promptHolder = std::make_shared<QString>();

    connect(service, &CharacterImageGenService::imageReady, this,
            [this, service, id, argsEcho, finalPrompt, promptHolder](const QImage& img) {
        // Efêmero, só pra exibir na bolha final — nunca gravado em
        // AIChatMessage/m_messages (ver comentário em m_pendingBubbleImages).
        m_pendingBubbleImages.append(img);
        // Sem personagem associado (characterName vazio) — não mexe em
        // nenhuma ficha, só entra na galeria geral do projeto.
        GeneratedImageGallery::save(m_projectRoot, img, QString(),
            finalPrompt.isEmpty() ? *promptHolder : finalPrompt);
        finishToolRoundTrip(id, QStringLiteral("generate_scene_image"), argsEcho,
            tr("Imagem gerada."));
        service->deleteLater();
    });
    connect(service, &CharacterImageGenService::errorOccurred, this,
            [this, service, id, argsEcho](const QString& err) {
        finishToolRoundTrip(id, QStringLiteral("generate_scene_image"), argsEcho,
            tr("Erro ao gerar imagem: %1").arg(err));
        service->deleteLater();
    });
    connect(service, &CharacterImageGenService::promptEngineered, this,
            [this, promptHolder](const QString& prompt) {
        *promptHolder = prompt;
        ToolTraceEntry entry;
        entry.text = tr("📝 Prompt gerado: %1").arg(prompt);
        m_pendingToolTraces.append(entry);
    });

    if (!finalPrompt.isEmpty()) {
        service->generateFromRawPrompt(finalPrompt, imageModel, imageQuality, imageSize);
    } else {
        // characterContext vazio — geração livre, mesmo caminho que
        // MainWindow::generateImageFromSelection já usa manualmente.
        service->generate(description, imageStylePresetFromKey(styleKey), QString(),
            imageModel, imageQuality, imageSize);
    }
}

void AIChatPanel::handleProposeDocumentEditTool(const QString& id, const QJsonObject& arguments)
{
    const QString originalText = arguments.value(QStringLiteral("original_text")).toString();
    const QString newText = arguments.value(QStringLiteral("new_text")).toString();
    const QString explanation = arguments.value(QStringLiteral("explanation")).toString();

    QJsonObject argsEcho;
    argsEcho[QStringLiteral("original_text")] = originalText;
    argsEcho[QStringLiteral("new_text")] = newText;
    if (!explanation.isEmpty()) argsEcho[QStringLiteral("explanation")] = explanation;

    QTextEdit* editor = m_activeEditorProvider ? m_activeEditorProvider() : nullptr;
    if (!editor) {
        finishToolRoundTrip(id, QStringLiteral("propose_document_edit"), argsEcho,
            tr("Não há nenhum documento aberto no editor agora — não é possível propor uma "
               "edição. Avise o autor que precisa abrir o capítulo/cena primeiro."));
        return;
    }
    if (originalText.trimmed().isEmpty() || newText.trimmed().isEmpty()) {
        finishToolRoundTrip(id, QStringLiteral("propose_document_edit"), argsEcho,
            tr("original_text e new_text não podem ficar vazios."));
        return;
    }

    QTextDocument* doc = editor->document();
    const QTextCursor firstMatch = doc->find(originalText);
    if (firstMatch.isNull()) {
        finishToolRoundTrip(id, QStringLiteral("propose_document_edit"), argsEcho,
            tr("Não encontrei esse trecho, exatamente como foi citado, no documento aberto "
               "agora. Cite o texto literal (mesma pontuação e espaçamento do original) ou "
               "avise o autor que não conseguiu localizar o trecho."));
        return;
    }
    const QTextCursor secondMatch = doc->find(originalText, firstMatch.selectionEnd());
    if (!secondMatch.isNull()) {
        finishToolRoundTrip(id, QStringLiteral("propose_document_edit"), argsEcho,
            tr("Esse trecho aparece mais de uma vez no documento — inclua mais contexto ao "
               "redor pra torná-lo único antes de propor a edição."));
        return;
    }

    PendingEditSuggestion suggestion;
    suggestion.originalText = originalText;
    suggestion.newText = newText;
    suggestion.explanation = explanation;
    suggestion.docTitleAtProposal = m_currentDocTitleProvider ? m_currentDocTitleProvider() : QString();
    m_pendingEditSuggestions.append(suggestion);

    logConversation(miraAssistantName(), tr("✏️ Sugestão de edição criada, aguardando confirmação do autor."));

    finishToolRoundTrip(id, QStringLiteral("propose_document_edit"), argsEcho,
        tr("Sugestão de edição criada e exibida ao autor como um cartão de confirmação — ele "
           "vai decidir se aplica ou descarta, não assuma que já foi aplicada."));
}

QString AIChatPanel::runWorldDataLookup(const QString& query) const
{
    const QString needle = query.trimmed();
    if (needle.isEmpty()) return tr("(consulta vazia — nada pra buscar)");

    const GeoData& geo = GeoData::instance();
    QStringList results;

    // Países — nome em PT ou EN, comparação parcial.
    for (const GeoData::Country& c : geo.countries()) {
        if (!c.name.contains(needle, Qt::CaseInsensitive) &&
            !c.nameEn.contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        QString line = tr("País: %1 (%2)").arg(c.name, c.iso);
        if (const GeoData::CountryInfo* info = geo.countryInfo(c.iso)) {
            line += tr(" — capital: %1, população: %2, área: %3 km², continente: %4, moeda: %5, idiomas: %6")
                .arg(info->capital.isEmpty() ? tr("desconhecida") : info->capital)
                .arg(info->population)
                .arg(info->area, 0, 'f', 0)
                .arg(info->continent.isEmpty() ? tr("desconhecido") : info->continent,
                     info->currency.isEmpty() ? tr("desconhecida") : info->currency,
                     info->languages.isEmpty() ? tr("desconhecidos") : info->languages);
        }
        results.append(line);
    }

    // Cidades — nomes comuns (ex. "Santos", "Springfield") podem casar
    // dezenas de vezes; ordena por população e limita a 8 pra não afogar a
    // resposta em ruído.
    QVector<const GeoData::Place*> cityMatches;
    for (const GeoData::Place& p : geo.places()) {
        if (p.name.contains(needle, Qt::CaseInsensitive) ||
            p.asciiName.contains(needle, Qt::CaseInsensitive) ||
            p.altNames.contains(needle, Qt::CaseInsensitive)) {
            cityMatches.append(&p);
        }
    }
    std::sort(cityMatches.begin(), cityMatches.end(),
        [](const GeoData::Place* a, const GeoData::Place* b) { return a->population > b->population; });

    int shown = 0;
    for (const GeoData::Place* p : cityMatches) {
        if (shown >= 8) break;
        QString tag;
        if (p->isCapital()) tag = tr(" (capital nacional)");
        else if (p->isStateCapital()) tag = tr(" (capital estadual)");
        results.append(tr("Cidade: %1, %2 — população: %3%4")
            .arg(p->name, p->countryCode).arg(p->population).arg(tag));
        ++shown;
    }
    if (cityMatches.size() > 8) {
        results.append(tr("(+ %1 outra(s) cidade(s) com nome parecido, não mostradas)")
            .arg(cityMatches.size() - 8));
    }

    if (results.isEmpty()) {
        return tr("Nenhum país ou cidade do mundo real encontrado com \"%1\" no dataset "
                   "geográfico (Natural Earth/GeoNames). Se for um lugar FICTÍCIO do "
                   "projeto, use search_project em vez desta ferramenta.").arg(needle);
    }
    return results.join(QStringLiteral("\n"));
}

void AIChatPanel::finishToolRoundTrip(const QString& id, const QString& toolName,
                                      const QJsonObject& argumentsEcho, const QString& resultText)
{
    // Ponto único por onde TODA tool call passa ao terminar — cobre a
    // bolha de pensamento com o rastro completo até aqui, mesmo pras
    // chamadas "silenciosas" (busca no projeto) que não tocavam m_statusLabel
    // antes.
    refreshThinkingBubbleDetails();

    AIChatMessage assistantMsg;
    assistantMsg.role = QStringLiteral("assistant");
    assistantMsg.content = QString();
    AIToolCall tc;
    tc.id = id;
    tc.name = toolName;
    tc.argumentsJson = QString::fromUtf8(QJsonDocument(argumentsEcho).toJson(QJsonDocument::Compact));
    assistantMsg.toolCalls.append(tc);
    m_messages.append(assistantMsg);

    AIChatMessage toolMsg;
    toolMsg.role = QStringLiteral("tool");
    toolMsg.toolCallId = id;
    toolMsg.content = resultText;
    m_messages.append(toolMsg);

    // Reenvio adiado pro próximo ciclo do event loop — chamar sendMessage()
    // direto daqui dentro reentraria em AIClient::handleFinished() (ainda
    // em execução, é de lá que o toolCallReceived original foi emitido) e
    // limparia m_accumulated/m_sseBuffer do turno atual antes dele terminar.
    QTimer::singleShot(0, this, [this]() {
        m_pendingToolCall = false;
        m_client->sendMessage(m_messages);
    });
}

const AIChatPanel::ScanDoc* AIChatPanel::findDocByTitle(const QVector<ScanDoc>& docs, const QString& title) const
{
    if (title.trimmed().isEmpty()) return nullptr;
    for (const ScanDoc& d : docs) {
        if (d.title.compare(title, Qt::CaseInsensitive) == 0) return &d;
    }
    for (const ScanDoc& d : docs) {
        if (d.title.contains(title, Qt::CaseInsensitive) || title.contains(d.title, Qt::CaseInsensitive)) return &d;
    }
    return nullptr;
}

AIChatPanel::SearchResult AIChatPanel::runProjectSearch(const QString& query) const
{
    const QString trimmedQuery = query.trimmed();
    SearchResult result;
    if (trimmedQuery.isEmpty()) {
        result.text = tr("(consulta vazia — nada pra buscar)");
        return result;
    }

    // OR, não AND: "carro Klara" deve achar trechos com "carro" OU "Klara"
    // separadamente — exigir a frase inteira literal falha sempre que o
    // texto não usa exatamente essas palavras juntas nessa ordem, mesmo
    // quando a informação está lá (ex.: "Honda" em vez de "carro").
    // Palavras curtas (<3 letras, tipo "de"/"da"/"o") são ignoradas.
    //
    // Termos maiores primeiro: uma palavra comum no romance inteiro (ex.
    // "carro", já que a protagonista é motorista) tem MUITO mais ocorrência
    // bruta que um nome próprio específico. Sem essa ordem, o termo genérico
    // esgota o orçamento de resultados sozinho antes do termo específico
    // (o que realmente importa) ter qualquer chance.
    QStringList terms;
    for (const QString& w : trimmedQuery.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts)) {
        if (w.size() >= 3) terms.append(w);
    }
    if (terms.isEmpty()) terms.append(trimmedQuery);
    std::sort(terms.begin(), terms.end(), [](const QString& a, const QString& b) {
        return a.size() > b.size();
    });

    constexpr int kMaxResultsTotal = 16;
    constexpr int kMaxPerTermPerDoc = 2;

    const QVector<ScanDoc> docs = collectAllDocs();
    QStringList results;
    QStringList hitTitles; // em ordem de primeira ocorrência, sem repetir
    for (const QString& term : terms) {
        for (const ScanDoc& doc : docs) {
            int from = 0;
            int foundForTermInDoc = 0;
            while (foundForTermInDoc < kMaxPerTermPerDoc && results.size() < kMaxResultsTotal) {
                const int idx = doc.plainText.indexOf(term, from, Qt::CaseInsensitive);
                if (idx < 0) break;
                const int ctxStart = qMax(0, idx - 150);
                const int ctxEnd = qMin(doc.plainText.size(), idx + term.size() + 150);
                const QString snippet = doc.plainText.mid(ctxStart, ctxEnd - ctxStart).trimmed();
                results.append(QStringLiteral("[%1 · termo \"%2\"] …%3…").arg(doc.title, term, snippet));
                if (!hitTitles.contains(doc.title)) hitTitles.append(doc.title);
                from = idx + term.size();
                ++foundForTermInDoc;
            }
            if (results.size() >= kMaxResultsTotal) break;
        }
        if (results.size() >= kMaxResultsTotal) break;
    }

    if (results.isEmpty()) {
        result.text = tr("Nenhuma ocorrência de \"%1\" (nem de suas palavras individuais) encontrada no texto bruto do projeto.").arg(trimmedQuery);
        return result;
    }

    // Trechos isolados por palavra não bastam quando a resposta depende de
    // conectar uma referência indireta ("a mulher", "a tal investigadora")
    // ao nome certo — algo que só ler o capítulo inteiro resolve. Sem essa
    // sugestão explícita listando ONDE bateu, o modelo tende a nunca cogitar
    // usar read_document sozinho, mesmo com a instrução geral no system
    // prompt — precisa do lembrete concreto, não só da regra abstrata.
    QStringList suggestTitles = hitTitles.mid(0, 6);
    const QString footer = tr(
        "\n\n[Se nenhum trecho acima confirmar a resposta com certeza — por "
        "exemplo se descrevem alguém sem nomear quem é —, considere chamar "
        "read_document num destes documentos pra ler por inteiro antes de "
        "responder ou desistir: %1]").arg(suggestTitles.join(QStringLiteral(", ")));

    result.text = results.join(QStringLiteral("\n\n---\n\n")) + footer;
    result.docTitles = hitTitles.mid(0, 8); // teto de chips clicáveis na UI
    return result;
}

void AIChatPanel::setBusy(bool busy)
{
    m_sendBtn->setEnabled(!busy);
    m_inputEdit->setEnabled(!busy);
    m_attachBtn->setEnabled(!busy);
    m_scanBtn->setEnabled(!busy);
    // m_statusLabel aqui era só o "Pensando…" do turno de chat — a varredura
    // de documentos (m_scanning) usa o mesmo QLabel pra outra coisa e nunca
    // passa por setBusy(), então esse guard nunca disparava de verdade; mesmo
    // assim mantido por segurança caso isso mude no futuro.
    if (m_scanning) return;
    if (busy) showThinkingBubble();
    else hideThinkingBubble();
}

void AIChatPanel::logConversation(const QString& speakerLabel, const QString& text) const
{
    if (m_projectRoot.isEmpty()) return;
    const QString dirPath = m_projectRoot + QStringLiteral("/ai_context");
    QDir().mkpath(dirPath);
    QFile f(dirPath + QStringLiteral("/conversa_mira.md"));
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << QStringLiteral("### %1 — %2\n\n%3\n\n")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")), speakerLabel, text);
}

void AIChatPanel::fitBubbleHeight(QTextEdit* te, int textWidth) const
{
    // setFixedHeight sozinho não bastava: o documento interno quebrava
    // linha na largura de TETO (textWidth), mas o WIDGET em si, sem largura
    // própria travada, ficava com o sizeHint natural do Qt (geralmente bem
    // menor) — o texto continuava sendo desenhado pra a largura de teto,
    // cortado pela borda direita do widget mais estreito. Trava a largura
    // também, encolhendo pro conteúdo real (idealWidth, a largura da maior
    // linha já quebrada) até o teto — efeito "bolha de chat" de verdade.
    te->document()->setTextWidth(textWidth);
    const int idealW = int(te->document()->idealWidth()) + 1; // arredonda pra cima
    te->setFixedWidth(qBound(20, idealW + 4, textWidth));
    const int h = int(te->document()->size().height()) + 6;
    te->setFixedHeight(qMax(20, h));
}

void AIChatPanel::fitInputHeight()
{
    if (!m_inputEdit) return;
    // Mesmo motivo de fitBubbleHeight: sem travar explicitamente a largura
    // usada pro cálculo de quebra na largura ATUAL do viewport, a altura
    // calculada não acompanha nem a digitação nem o redimensionamento do
    // painel.
    m_inputEdit->document()->setTextWidth(m_inputEdit->viewport()->width());
    const int h = int(m_inputEdit->document()->size().height()) + 16;
    m_inputEdit->setFixedHeight(qBound(38, h, 120));
}

int AIChatPanel::transcriptAvailableWidth() const
{
    // Largura ATUAL da área de transcrição (não do painel inteiro) — no modo
    // janela, m_railWidget (230px fixos) fica ao lado da coluna de chat, então
    // width() do painel inclui espaço que as bolhas não têm de verdade. Usar
    // o viewport do scroll da transcrição é o que sobra pra bolha depois de
    // descontar o rail; cai pra kPanelWidth só antes do 1º layout (viewport
    // ainda com largura 0).
    const int viewportW = m_transcriptScroll && m_transcriptScroll->viewport()
        ? m_transcriptScroll->viewport()->width() : 0;
    return qMax(viewportW > 0 ? viewportW : width(), kPanelWidth);
}

AIChatPanel::BubbleHandle AIChatPanel::createBubbleRow(bool isUser, const QString& initialText,
                                                       const QString& imageDataUrl)
{
    auto* row = new QWidget(m_transcriptContent);
    auto* rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(0);

    // Largura ATUAL da área de transcrição, não uma constante — assim as
    // bolhas se adaptam sozinhas ao trocar entre modo painel/janela ou
    // redimensionar.
    const int bubbleMaxW = int(transcriptAvailableWidth() * 0.88);
    const int textWidth = bubbleMaxW - 24; // menos as margens horizontais da bolha (12+12)

    auto* bubble = new QFrame(row);
    bubble->setObjectName(isUser ? QStringLiteral("chatBubbleUser") : QStringLiteral("chatBubbleMira"));
    bubble->setMaximumWidth(bubbleMaxW);
    bubble->setStyleSheet(bubbleQss(isUser));

    auto* bubbleLay = new QVBoxLayout(bubble);
    bubbleLay->setContentsMargins(12, 9, 12, 9);
    bubbleLay->setSpacing(4);

    // Imagem anexada pelo usuário (se houver) — vem ANTES do texto, mesmo
    // padrão visual de apps de chat comuns. ClickableImageLabel (QLabel, não
    // QAbstractScrollArea — não tem o histórico de bugs de viewport/padding
    // global que os QTextEdit desta bolha tiveram) abre um visualizador
    // grande/salvar ao clicar; objectName próprio só como seguro barato
    // contra uma futura regra QSS global por tipo.
    if (!imageDataUrl.isEmpty()) {
        const QPixmap pm = AvatarUtils::decodeDataUrl(imageDataUrl);
        if (!pm.isNull()) {
            auto* imgLabel = new ClickableImageLabel(bubble);
            imgLabel->setObjectName(QStringLiteral("chatBubbleImage"));
            imgLabel->setPixmap(pm.scaled(qMin(pm.width(), textWidth), pm.height(),
                Qt::KeepAspectRatio, Qt::SmoothTransformation));
            imgLabel->setFullImage(pm.toImage(), QStringLiteral("imagem_anexada"));
            bubbleLay->addWidget(imgLabel);
        }
    }

    // QTextEdit em vez de QLabel: renderiza markdown real (setMarkdown) e
    // calcula largura/altura de forma muito mais previsível — o QLabel
    // antigo tinha bug de quebra de linha patológica (uma palavra por
    // linha) em certas combinações de layout aninhado + markdown.
    auto* te = new QTextEdit(bubble);
    te->setObjectName(QStringLiteral("chatBubbleText"));
    te->setReadOnly(true);
    te->setFrameShape(QFrame::NoFrame);
    te->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    te->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    te->setTextInteractionFlags(Qt::TextSelectableByMouse);
    te->document()->setDocumentMargin(0);
    const QString textColorHex = bubbleTextColor(isUser);
    // Ver bubbleTextQss(): o "padding: 0" de lá não é decorativo, é o que faz
    // o texto da bolha existir na tela.
    te->setStyleSheet(bubbleTextQss(textColorHex));
    // O QSS setado no QTextEdit não propaga pro viewport() interno
    // (QAbstractScrollArea tem um widget de viewport separado, que pinta a
    // própria QPalette::Base opaca por cima da cor da bolha). Mesmo fix já
    // aplicado no preview de ficha do StatsPanel (commit b31a631).
    te->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    te->setMarkdown(initialText);
    applyBubbleBlockSpacing(te);
    forceBubbleTextColor(te, textColorHex);
    fitBubbleHeight(te, textWidth);
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

    BubbleHandle h;
    h.bubble = bubble;
    h.textEdit = te;
    h.bubbleLayout = bubbleLay;
    h.textWidth = textWidth;
    return h;
}

void AIChatPanel::refitAllBubbles()
{
    // createBubbleRow trava textWidth/setFixedWidth com a largura do painel
    // NO MOMENTO da criação — sem isso, bolhas já na tela ficavam com o
    // texto cortado (largura antiga, maior) ao encolher o painel (troca de
    // modo painel/janela ou arrasto do canto), mesmo a bolha-frame em si
    // encolhendo via setMaximumWidth. Chamado a cada resizeEvent.
    if (!m_transcriptContent) return;
    const int bubbleMaxW = int(transcriptAvailableWidth() * 0.88);
    const int textWidth = bubbleMaxW - 24;
    for (QFrame* bubble : m_transcriptContent->findChildren<QFrame*>()) {
        const QString name = bubble->objectName();
        if (name != QStringLiteral("chatBubbleUser") && name != QStringLiteral("chatBubbleMira")) continue;
        bubble->setMaximumWidth(bubbleMaxW);
        for (QTextEdit* te : bubble->findChildren<QTextEdit*>()) {
            if (te->objectName() == QStringLiteral("chatBubbleText")) fitBubbleHeight(te, textWidth);
        }
    }
}

void AIChatPanel::showThinkingBubble()
{
    if (m_thinkingRow) return; // já existe — setBusy(true) só roda 1x por turno, isso é só defensivo

    auto* bubble = new QFrame(m_transcriptContent);
    bubble->setObjectName(QStringLiteral("chatBubbleMira"));
    bubble->setMaximumWidth(int(transcriptAvailableWidth() * 0.88));
    bubble->setStyleSheet(bubbleQss(/*isUser=*/false));

    auto* bubbleLay = new QVBoxLayout(bubble);
    bubbleLay->setContentsMargins(12, 9, 12, 9);
    bubbleLay->setSpacing(4);

    // O próprio header é o botão que expande/recolhe — sem seta, o cursor
    // de mão + o clique no texto já bastam (mesmo espírito minimalista do
    // pedido: só "Pensando…" pulsando, nada de UI extra até clicar).
    m_thinkingToggle = new QToolButton(bubble);
    m_thinkingToggle->setObjectName(QStringLiteral("chatThinkingToggle"));
    m_thinkingToggle->setText(tr("Pensando…"));
    m_thinkingToggle->setCheckable(true);
    m_thinkingToggle->setCursor(Qt::PointingHandCursor);
    m_thinkingToggle->setToolTip(tr("Clique pra ver o que estou fazendo"));
    m_thinkingToggle->setStyleSheet(thinkingToggleQss(QColor(Theme::textPrimary())));
    bubbleLay->addWidget(m_thinkingToggle);

    auto* details = new QWidget(bubble);
    m_thinkingDetailsLay = new QVBoxLayout(details);
    m_thinkingDetailsLay->setContentsMargins(0, 2, 0, 0);
    m_thinkingDetailsLay->setSpacing(6);
    details->setVisible(false);
    bubbleLay->addWidget(details);

    connect(m_thinkingToggle, &QToolButton::toggled, this,
            [details](bool checked) { details->setVisible(checked); });

    auto* row = new QWidget(m_transcriptContent);
    auto* rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->addWidget(bubble);
    rowLay->addStretch();
    m_transcriptLayout->insertWidget(m_transcriptLayout->count() - 1, row);
    m_thinkingRow = row;

    refreshThinkingBubbleDetails(); // pega o que já tiver em m_pendingToolTraces (raro, mas seguro)

    // Pulso: sem QGraphicsOpacityEffect de propósito — ele quebra o repaint
    // dentro de QScrollArea (mesmo gotcha já documentado no StackView da
    // Biblioteca). Anima a cor do texto direto via QSS a cada tick.
    m_thinkingPulsePhase = 0.0;
    m_thinkingPulseTimer = new QTimer(this);
    connect(m_thinkingPulseTimer, &QTimer::timeout, this, [this]() {
        if (!m_thinkingToggle) return;
        m_thinkingPulsePhase += 0.12;
        const qreal t = (qSin(m_thinkingPulsePhase) + 1.0) / 2.0; // 0..1
        QColor c(Theme::textPrimary());
        c.setAlpha(130 + int(t * 125)); // 130..255
        m_thinkingToggle->setStyleSheet(thinkingToggleQss(c));
    });
    m_thinkingPulseTimer->start(60);
}

void AIChatPanel::hideThinkingBubble()
{
    if (m_thinkingPulseTimer) {
        m_thinkingPulseTimer->stop();
        m_thinkingPulseTimer->deleteLater();
        m_thinkingPulseTimer = nullptr;
    }
    if (m_thinkingRow) {
        m_thinkingRow->deleteLater();
        m_thinkingRow = nullptr;
    }
    m_thinkingToggle = nullptr;
    m_thinkingDetailsLay = nullptr;
}

// Repopula o painel de detalhe da bolha de pensamento a partir de
// m_pendingToolTraces inteiro — chamado a cada tool call concluída
// (finishToolRoundTrip) e nos pontos que já avisavam uma ação demorada
// (leitura de documento inteiro, geração de imagem), pra quem clicar
// enquanto a Mira ainda está trabalhando ver o progresso ao vivo, não só
// o resultado final. Mesmo visual de attachToolTraces (chips de documento
// clicáveis incluídos), só que reconstruído do zero a cada chamada — a
// lista é curta (poucos tool calls por turno), não compensa comparar diffs.
void AIChatPanel::refreshThinkingBubbleDetails()
{
    if (!m_thinkingDetailsLay) return;
    QWidget* detailsContainer = m_thinkingDetailsLay->parentWidget();

    QLayoutItem* item;
    while ((item = m_thinkingDetailsLay->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    for (const ToolTraceEntry& traceEntry : m_pendingToolTraces) {
        auto* text = new QLabel(traceEntry.text, detailsContainer);
        text->setObjectName(QStringLiteral("chatTraceText"));
        text->setWordWrap(true);
        text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        text->setStyleSheet(traceTextQss());
        m_thinkingDetailsLay->addWidget(text);

        if (!traceEntry.docTitles.isEmpty()) {
            auto* chipsRow = new QWidget(detailsContainer);
            auto* chipsLay = new QHBoxLayout(chipsRow);
            chipsLay->setContentsMargins(0, 0, 0, 0);
            chipsLay->setSpacing(4);
            for (const QString& docTitle : traceEntry.docTitles) {
                auto* chip = new QToolButton(chipsRow);
                chip->setObjectName(QStringLiteral("chatTraceChip"));
                chip->setText(QStringLiteral("📄 %1").arg(docTitle));
                chip->setCursor(Qt::PointingHandCursor);
                chip->setToolTip(tr("Abrir no editor"));
                chip->setStyleSheet(traceChipQss());
                connect(chip, &QToolButton::clicked, this, [this, docTitle]() {
                    if (!m_docOpener) return;
                    const QVector<ScanDoc> docs = collectAllDocs();
                    const ScanDoc* match = findDocByTitle(docs, docTitle);
                    if (match && !match->key.isEmpty()) m_docOpener(match->key);
                });
                chipsLay->addWidget(chip);
            }
            chipsLay->addStretch();
            m_thinkingDetailsLay->addWidget(chipsRow);
        }
    }
}

void AIChatPanel::attachToolTraces(BubbleHandle& handle, const QVector<ToolTraceEntry>& traces)
{
    if (traces.isEmpty() || !handle.bubble) return;

    auto* toggleBtn = new QToolButton(handle.bubble);
    toggleBtn->setObjectName(QStringLiteral("chatTraceToggle"));
    toggleBtn->setText(tr("▸ Ver pesquisas (%1)").arg(traces.size()));
    toggleBtn->setCheckable(true);
    toggleBtn->setCursor(Qt::PointingHandCursor);
    toggleBtn->setStyleSheet(traceToggleQss());
    handle.bubbleLayout->addWidget(toggleBtn);

    auto* detailsContainer = new QWidget(handle.bubble);
    auto* detailsLay = new QVBoxLayout(detailsContainer);
    detailsLay->setContentsMargins(0, 2, 0, 0);
    detailsLay->setSpacing(6);

    for (const ToolTraceEntry& traceEntry : traces) {
        auto* text = new QLabel(traceEntry.text, detailsContainer);
        text->setObjectName(QStringLiteral("chatTraceText"));
        text->setWordWrap(true);
        text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        text->setStyleSheet(traceTextQss());
        detailsLay->addWidget(text);

        // Citação clicável: cada documento referenciado nessa chamada vira
        // um chip que abre o documento no editor (via setDocOpener), em vez
        // de tentar detectar "no Capítulo 3..." dentro da prosa livre da
        // resposta — frágil demais pra confiar em NLP sobre texto natural.
        if (!traceEntry.docTitles.isEmpty()) {
            auto* chipsRow = new QWidget(detailsContainer);
            auto* chipsLay = new QHBoxLayout(chipsRow);
            chipsLay->setContentsMargins(0, 0, 0, 0);
            chipsLay->setSpacing(4);
            for (const QString& docTitle : traceEntry.docTitles) {
                auto* chip = new QToolButton(chipsRow);
                chip->setObjectName(QStringLiteral("chatTraceChip"));
                chip->setText(QStringLiteral("📄 %1").arg(docTitle));
                chip->setCursor(Qt::PointingHandCursor);
                chip->setToolTip(tr("Abrir no editor"));
                chip->setStyleSheet(traceChipQss());
                connect(chip, &QToolButton::clicked, this, [this, docTitle]() {
                    if (!m_docOpener) return;
                    const QVector<ScanDoc> docs = collectAllDocs();
                    const ScanDoc* match = findDocByTitle(docs, docTitle);
                    if (match && !match->key.isEmpty()) m_docOpener(match->key);
                });
                chipsLay->addWidget(chip);
            }
            chipsLay->addStretch();
            detailsLay->addWidget(chipsRow);
        }
    }

    detailsContainer->setVisible(false);
    handle.bubbleLayout->addWidget(detailsContainer);

    const int count = traces.size();
    connect(toggleBtn, &QToolButton::toggled, this, [toggleBtn, detailsContainer, count](bool checked) {
        detailsContainer->setVisible(checked);
        toggleBtn->setText(checked
            ? QStringLiteral("▾ Ver pesquisas (%1)").arg(count)
            : QStringLiteral("▸ Ver pesquisas (%1)").arg(count));
    });
}

void AIChatPanel::attachBubbleImages(BubbleHandle& handle, const QVector<QImage>& images)
{
    if (images.isEmpty() || !handle.bubble) return;
    for (const QImage& img : images) {
        if (img.isNull()) continue;
        const QPixmap pm = QPixmap::fromImage(img);
        auto* imgLabel = new ClickableImageLabel(handle.bubble);
        imgLabel->setObjectName(QStringLiteral("chatBubbleImage"));
        imgLabel->setPixmap(pm.scaled(qMin(pm.width(), handle.textWidth), pm.height(),
            Qt::KeepAspectRatio, Qt::SmoothTransformation));
        imgLabel->setFullImage(img, QStringLiteral("imagem_gerada"));
        handle.bubbleLayout->addWidget(imgLabel);
    }
}

void AIChatPanel::attachEditSuggestions(BubbleHandle& handle, const QVector<PendingEditSuggestion>& suggestions)
{
    if (suggestions.isEmpty() || !handle.bubble) return;

    for (const PendingEditSuggestion& sug : suggestions) {
        auto* card = new QFrame(handle.bubble);
        card->setObjectName(QStringLiteral("chatEditCard"));
        card->setStyleSheet(editCardQss());
        auto* cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(10, 8, 10, 8);
        cardLay->setSpacing(4);

        auto* title = new QLabel(tr("Sugestão de edição:"), card);
        title->setObjectName(QStringLiteral("chatEditTitle"));
        title->setStyleSheet(editCardTitleQss());
        cardLay->addWidget(title);

        if (!sug.explanation.trimmed().isEmpty()) {
            auto* explanationLabel = new QLabel(sug.explanation.trimmed(), card);
            explanationLabel->setObjectName(QStringLiteral("chatEditText"));
            explanationLabel->setWordWrap(true);
            explanationLabel->setStyleSheet(editCardTextQss());
            cardLay->addWidget(explanationLabel);
        }

        auto* beforeLabel = new QLabel(tr("Antes: %1").arg(sug.originalText), card);
        beforeLabel->setObjectName(QStringLiteral("chatEditText"));
        beforeLabel->setWordWrap(true);
        beforeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        beforeLabel->setStyleSheet(editCardTextQss());
        cardLay->addWidget(beforeLabel);

        auto* afterLabel = new QLabel(tr("Depois: %1").arg(sug.newText), card);
        afterLabel->setObjectName(QStringLiteral("chatEditText"));
        afterLabel->setWordWrap(true);
        afterLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        afterLabel->setStyleSheet(editCardTextQss());
        cardLay->addWidget(afterLabel);

        auto* statusLabel = new QLabel(card);
        statusLabel->setObjectName(QStringLiteral("chatEditStatus"));
        statusLabel->setWordWrap(true);
        statusLabel->setStyleSheet(editCardStatusQss());
        statusLabel->setVisible(false);
        cardLay->addWidget(statusLabel);

        auto* btnRow = new QWidget(card);
        auto* btnLay = new QHBoxLayout(btnRow);
        btnLay->setContentsMargins(0, 2, 0, 0);
        btnLay->setSpacing(6);
        btnLay->addStretch();
        auto* dismissBtn = new QPushButton(tr("Descartar"), btnRow);
        dismissBtn->setObjectName(QStringLiteral("chatEditDismissBtn"));
        dismissBtn->setCursor(Qt::PointingHandCursor);
        dismissBtn->setStyleSheet(chipQss());
        btnLay->addWidget(dismissBtn);
        auto* applyBtn = new QPushButton(tr("Aplicar ✓"), btnRow);
        applyBtn->setObjectName(QStringLiteral("chatEditApplyBtn"));
        applyBtn->setCursor(Qt::PointingHandCursor);
        applyBtn->setStyleSheet(sendBtnQss());
        btnLay->addWidget(applyBtn);
        cardLay->addWidget(btnRow);

        connect(dismissBtn, &QPushButton::clicked, this, [statusLabel, btnRow]() {
            statusLabel->setText(QObject::tr("Descartada."));
            statusLabel->setVisible(true);
            btnRow->setVisible(false);
        });

        const QString originalText = sug.originalText;
        const QString newText = sug.newText;
        const QString docTitleAtProposal = sug.docTitleAtProposal;
        connect(applyBtn, &QPushButton::clicked, this,
                [this, originalText, newText, docTitleAtProposal, statusLabel, btnRow]() {
            // Mesmo QTextEdit é reaproveitado pra qualquer documento (ver
            // EditorHost) — se o autor trocou de capítulo desde que a
            // sugestão apareceu, o ponteiro continua válido mas aponta pro
            // documento ERRADO, daí a checagem de título antes de tocar em
            // qualquer QTextCursor.
            if (m_currentDocTitleProvider && !docTitleAtProposal.isEmpty()
                    && m_currentDocTitleProvider() != docTitleAtProposal) {
                statusLabel->setText(tr("⚠️ O documento aberto mudou desde a sugestão — reabra "
                                        "\"%1\" antes de aplicar.").arg(docTitleAtProposal));
                statusLabel->setVisible(true);
                return;
            }
            QTextEdit* editor = m_activeEditorProvider ? m_activeEditorProvider() : nullptr;
            if (!editor) {
                statusLabel->setText(tr("⚠️ Nenhum documento está aberto agora."));
                statusLabel->setVisible(true);
                return;
            }
            QTextDocument* doc = editor->document();
            QTextCursor match = doc->find(originalText);
            if (match.isNull()) {
                statusLabel->setText(tr("⚠️ Não encontrei mais esse trecho — o texto pode ter mudado."));
                statusLabel->setVisible(true);
                return;
            }
            const QTextCursor secondMatch = doc->find(originalText, match.selectionEnd());
            if (!secondMatch.isNull()) {
                statusLabel->setText(tr("⚠️ Esse trecho aparece mais de uma vez agora — peça uma "
                                        "sugestão mais específica."));
                statusLabel->setVisible(true);
                return;
            }
            match.beginEditBlock();
            match.insertText(newText);
            match.endEditBlock();
            statusLabel->setText(tr("✓ Edição aplicada."));
            statusLabel->setVisible(true);
            btnRow->setVisible(false);
        });

        handle.bubbleLayout->addWidget(card);
    }
}

void AIChatPanel::attachFeedbackButtons(BubbleHandle& handle, const QString& fullText)
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

    // Voto único por mensagem, sem alternar/desfazer — mesmo comportamento
    // de like/dislike de apps de chat conhecidos.
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

void AIChatPanel::addUserBubble(const QString& text, const QString& imageDataUrl)
{
    logConversation(tr("Você"), imageDataUrl.isEmpty() ? text
        : text + QStringLiteral("\n[imagem anexada]"));
    createBubbleRow(/*isUser=*/true, text, imageDataUrl);
}

void AIChatPanel::addMiraBubble(const QString& text, const QVector<ToolTraceEntry>& traces)
{
    logConversation(miraAssistantName(), text);
    BubbleHandle h = createBubbleRow(/*isUser=*/false, text);
    attachToolTraces(h, traces);
    attachFeedbackButtons(h, text);
}

void AIChatPanel::beginMiraStreamBubble()
{
    hideThinkingBubble(); // a resposta de verdade toma o lugar dela no mesmo ponto do transcript
    m_currentMiraBubble = createBubbleRow(/*isUser=*/false, QString());
    m_streamingText.clear();
    m_assistantTurnOpen = true;
}

void AIChatPanel::appendStreamToken(const QString& token)
{
    if (!m_assistantTurnOpen) beginMiraStreamBubble();
    m_streamingText += token;
    m_currentMiraBubble.textEdit->setMarkdown(m_streamingText);
    applyBubbleBlockSpacing(m_currentMiraBubble.textEdit);
    forceBubbleTextColor(m_currentMiraBubble.textEdit, Theme::textPrimary());
    fitBubbleHeight(m_currentMiraBubble.textEdit, m_currentMiraBubble.textWidth);
}

void AIChatPanel::finalizeMiraStreamBubble(const QVector<ToolTraceEntry>& traces,
                                           const QVector<QImage>& images,
                                           const QVector<PendingEditSuggestion>& editSuggestions)
{
    // Nada foi streamado e não há pistas/imagens/sugestões pra mostrar — não cria bolha vazia.
    if (!m_currentMiraBubble.bubble && traces.isEmpty() && images.isEmpty() && editSuggestions.isEmpty()) return;

    if (!m_currentMiraBubble.bubble) {
        m_currentMiraBubble = createBubbleRow(/*isUser=*/false, QString());
    }
    attachBubbleImages(m_currentMiraBubble, images);
    attachEditSuggestions(m_currentMiraBubble, editSuggestions);
    attachToolTraces(m_currentMiraBubble, traces);
    attachFeedbackButtons(m_currentMiraBubble, m_streamingText);
    m_currentMiraBubble = BubbleHandle();
    m_assistantTurnOpen = false;
}

void AIChatPanel::clearTranscriptUi()
{
    // Remove todas as bolhas, mas preserva o addStretch(1) final (sempre o
    // último item do layout) que mantém a conversa colada embaixo.
    while (m_transcriptLayout->count() > 1) {
        QLayoutItem* item = m_transcriptLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    m_currentMiraBubble = BubbleHandle();
    m_assistantTurnOpen = false;
}

void AIChatPanel::saveCurrentSession()
{
    if (m_projectRoot.isEmpty() || m_messages.isEmpty()) return;

    bool hasUser = false;
    QString title;
    for (const AIChatMessage& m : m_messages) {
        if (m.role == QStringLiteral("user")) {
            hasUser = true;
            if (title.isEmpty() && !m.content.trimmed().isEmpty()) title = m.content.trimmed();
        }
    }
    if (!hasUser) return; // só system prompt ainda — nada de conversa de verdade pra salvar

    if (m_currentSessionId.isEmpty()) {
        m_currentSessionId = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmsszzz"));
    }
    if (title.size() > 60) title = title.left(60) + QStringLiteral("…");
    if (title.isEmpty()) title = tr("Conversa sem título");

    QJsonObject root;
    root[QStringLiteral("title")] = title;
    root[QStringLiteral("updatedAt")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonArray arr;
    for (const AIChatMessage& m : m_messages) arr.append(chatMessageToJson(m));
    root[QStringLiteral("messages")] = arr;

    const QString dirPath = m_projectRoot + QStringLiteral("/ai_context/sessoes");
    QDir().mkpath(dirPath);
    QFile f(dirPath + QStringLiteral("/%1.json").arg(m_currentSessionId));
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

QVector<AIChatSessionInfo> AIChatPanel::listSessionsForRoot(const QString& root) const
{
    QVector<AIChatSessionInfo> out;
    if (root.isEmpty()) return out;

    QDir dir(root + QStringLiteral("/ai_context/sessoes"));
    if (!dir.exists()) return out;

    for (const QFileInfo& fi : dir.entryInfoList({ QStringLiteral("*.json") }, QDir::Files)) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        AIChatSessionInfo info;
        info.id = fi.completeBaseName();
        info.title = obj.value(QStringLiteral("title")).toString();
        info.updatedAt = obj.value(QStringLiteral("updatedAt")).toString();
        out.append(info);
    }
    std::sort(out.begin(), out.end(), [](const AIChatSessionInfo& a, const AIChatSessionInfo& b) {
        return a.updatedAt > b.updatedAt; // mais recente primeiro
    });
    return out;
}

QVector<AIChatMessage> AIChatPanel::loadSessionMessagesForRoot(const QString& root, const QString& id) const
{
    QVector<AIChatMessage> out;
    if (root.isEmpty() || id.isEmpty()) return out;

    QFile f(root + QStringLiteral("/ai_context/sessoes/%1.json").arg(id));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return out;
    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    for (const QJsonValue& v : obj.value(QStringLiteral("messages")).toArray()) {
        out.append(chatMessageFromJson(v.toObject()));
    }
    return out;
}

void AIChatPanel::deleteSessionsForRoot(const QString& root, const QStringList& ids)
{
    if (root.isEmpty() || ids.isEmpty()) return;

    if (m_previewMode && m_previewRoot == root) {
        // Não dá pra checar qual sessão o preview está mostrando (não
        // guardamos o id da sessão em preview) — mais seguro sair do
        // preview do que arriscar deixar o banner referenciando algo
        // apagado.
        exitPreviewMode();
    }
    if (root == m_projectRoot && ids.contains(m_currentSessionId)) {
        startNewSession(); // já chama rebuildProjectRail() — ok, é só mais 1
    }

    const QString dir = root + QStringLiteral("/ai_context/sessoes/");
    for (const QString& id : ids) {
        QFile::remove(dir + id + QStringLiteral(".json"));
    }
    rebuildProjectRail(); // uma vez só pro lote inteiro, não uma por item
}

void AIChatPanel::renderMessageHistory(const QVector<AIChatMessage>& messages)
{
    clearTranscriptUi();
    for (const AIChatMessage& m : messages) {
        if (m.role == QStringLiteral("user")
            && (!m.content.trimmed().isEmpty() || !m.imageDataUrl.isEmpty())) {
            createBubbleRow(/*isUser=*/true, m.content, m.imageDataUrl);
        } else if (m.role == QStringLiteral("assistant") && !m.content.trimmed().isEmpty()) {
            createBubbleRow(/*isUser=*/false, m.content);
        }
        // system e turnos de tool_calls/tool ficam de fora da UI — só
        // alimentam o histórico da API, não viram bolha visível de novo.
    }
}

void AIChatPanel::loadSession(const QString& id)
{
    if (m_projectRoot.isEmpty() || id.isEmpty()) return;
    if (!m_previewMode && id == m_currentSessionId) return; // já é essa

    const QVector<AIChatMessage> loaded = loadSessionMessagesForRoot(m_projectRoot, id);
    if (loaded.isEmpty()) return;

    if (m_previewMode) exitPreviewMode();
    else saveCurrentSession(); // não perde a conversa atual ao trocar

    m_messages = loaded;
    m_currentSessionId = id;
    m_pendingToolTraces.clear();
    m_toolHopCount = 0;

    renderMessageHistory(m_messages);
    rebuildProjectRail();
    updateChatContext();
}

void AIChatPanel::previewForeignSession(const QString& root, const QString& projectName, const QString& id)
{
    if (root.isEmpty() || id.isEmpty()) return;
    if (root == m_projectRoot) { loadSession(id); return; } // é o projeto ativo, não é "estrangeiro"

    const QVector<AIChatMessage> loaded = loadSessionMessagesForRoot(root, id);
    if (loaded.isEmpty()) return;

    // Não mexe em m_messages/m_currentSessionId — a conversa ativa do
    // projeto de verdade continua intacta em memória, só a TELA muda
    // temporariamente pra mostrar a conversa alheia.
    m_previewMode = true;
    m_previewRoot = root;
    renderMessageHistory(loaded);

    if (m_inputEdit) m_inputEdit->setEnabled(false);
    if (m_sendBtn) m_sendBtn->setEnabled(false);
    if (m_attachBtn) m_attachBtn->setEnabled(false);
    if (m_previewBanner) m_previewBanner->setVisible(true);
    if (m_previewBannerLabel) {
        m_previewBannerLabel->setText(
            tr("Vendo uma conversa de <b>%1</b> (somente leitura).").arg(projectName.toHtmlEscaped()));
    }
    updateChatContext();
}

void AIChatPanel::exitPreviewMode()
{
    if (!m_previewMode) return;
    m_previewMode = false;
    m_previewRoot.clear();

    if (m_inputEdit) m_inputEdit->setEnabled(true);
    if (m_sendBtn) m_sendBtn->setEnabled(true);
    if (m_attachBtn) m_attachBtn->setEnabled(true);
    if (m_previewBanner) m_previewBanner->setVisible(false);

    renderMessageHistory(m_messages);
    updateChatContext();
}

QString AIChatPanel::projectDisplayName(const QString& root) const
{
    if (root.isEmpty()) return QString();
    bool ok = false;
    const QJsonObject idx = ProjectStorage::readIndex(root, &ok);
    QString name = ok ? idx.value(QStringLiteral("name")).toString() : QString();
    if (name.trimmed().isEmpty()) name = QFileInfo(root).fileName();
    return name;
}

void AIChatPanel::updateChatContext()
{
    if (m_memoryChipLabel) {
        m_memoryChipLabel->setText(
            tr("%1 notas sobre você").arg(MiraUserMemoryStore::entryCount()));
    }

    if (!m_chatContextLabel || !m_previewBanner) return;

    if (m_previewMode) {
        m_chatContextLabel->setVisible(false);
        return; // banner já foi preenchido por previewForeignSession()
    }
    m_previewBanner->setVisible(false);

    if (!m_windowMode) { m_chatContextLabel->setVisible(false); return; }

    QString title;
    for (const AIChatMessage& m : m_messages) {
        if (m.role == QStringLiteral("user") && !m.content.trimmed().isEmpty()) {
            title = m.content.trimmed();
            break;
        }
    }
    if (title.size() > 46) title = title.left(46) + QStringLiteral("…");
    if (title.isEmpty()) title = tr("Nova conversa");

    m_chatContextLabel->setText(
        QStringLiteral("<b>%1</b> · %2").arg(projectDisplayName(m_projectRoot).toHtmlEscaped(),
                                             title.toHtmlEscaped()));
    m_chatContextLabel->setVisible(true);
}

void AIChatPanel::openPersonalityDialog()
{
    MiraPersonalityDialog dlg(this);
    connect(&dlg, &MiraPersonalityDialog::nameChanged, this, [this](const QString& newName) {
        if (m_titleLabel) m_titleLabel->setText(newName);
        if (m_studioNameLabel) m_studioNameLabel->setText(newName);
        if (m_studioAvatarLabel) m_studioAvatarLabel->setText(newName.left(1).toUpper());
        if (m_studioHeaderWidget) m_studioHeaderWidget->setToolTip(tr("Clique pra personalizar a %1").arg(newName));
        // O system prompt já em conversa (se houver) só pegaria o novo nome
        // na próxima sessão — atualiza também aqui, mesmo motivo de
        // handleSaveUserNoteTool/handleSaveProjectNoteTool.
        if (!m_messages.isEmpty() && m_messages.first().role == QStringLiteral("system")) {
            m_messages[0].content = buildSystemPrompt();
        }
    });
    dlg.exec();
}

void AIChatPanel::rebuildProjectRail()
{
    if (!m_railScrollLayout) return; // buildUi() ainda não rodou

    // Reconstrução completa — mais simples que reconciliar item a item, e o
    // custo é desprezível (poucos projetos conhecidos, poucas sessões cada).
    QLayoutItem* item;
    while ((item = m_railScrollLayout->takeAt(0)) != nullptr) {
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }
    m_railScrollLayout->addStretch(1);

    static const QVector<QString> kPalette = {
        QStringLiteral("#c0483a"), QStringLiteral("#7a4fc9"), QStringLiteral("#c98f2f"),
        QStringLiteral("#3a8c7a"), QStringLiteral("#4a7fc9"), QStringLiteral("#c94f8f"),
    };

    // Projeto ativo sempre primeiro; os demais na ordem de "recentes".
    QStringList ordered = m_knownProjectRoots;
    if (!m_projectRoot.isEmpty()) {
        ordered.removeAll(m_projectRoot);
        ordered.prepend(m_projectRoot);
    }

    for (const QString& root : ordered) {
        if (root.isEmpty()) continue;
        const QString name = projectDisplayName(root);
        QVector<AIChatSessionInfo> sessions = listSessionsForRoot(root);
        const bool isActive = (root == m_projectRoot);
        const QString color = kPalette.at(int(qHash(root) % uint(kPalette.size())));

        // Filtro de busca (m_railFilterText): pula o projeto inteiro se nem
        // o nome nem nenhuma sessão baterem; se só as sessões baterem, a
        // lista mostrada fica reduzida a elas (o projeto continua listado
        // pra dar contexto de onde a conversa vive).
        bool nameMatches = true;
        bool forceExpand = false;
        if (!m_railFilterText.isEmpty()) {
            nameMatches = name.contains(m_railFilterText, Qt::CaseInsensitive);
            if (!nameMatches) {
                QVector<AIChatSessionInfo> filtered;
                for (const AIChatSessionInfo& s : sessions) {
                    if (s.title.contains(m_railFilterText, Qt::CaseInsensitive)) filtered.append(s);
                }
                if (filtered.isEmpty()) continue; // nem projeto nem conversa batem — pula
                sessions = filtered;
                forceExpand = true;
            } else {
                forceExpand = true;
            }
        }

        auto* header = new QToolButton(m_railScrollContent);
        header->setCheckable(true);
        header->setChecked(isActive || forceExpand);
        header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        header->setAutoRaise(true);
        header->setCursor(Qt::PointingHandCursor);
        header->setStyleSheet(railFolderBtnQss(isActive));
        header->setIconSize(QSize(10, 10));
        {
            QPixmap dotPm(10, 10);
            dotPm.fill(Qt::transparent);
            QPainter painter(&dotPm);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setBrush(QColor(color));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(0, 0, 10, 10);
            painter.end();
            header->setIcon(QIcon(dotPm));
        }
        header->setText(QStringLiteral("%1  (%2)").arg(name).arg(sessions.size()));
        header->setToolTip(root);
        m_railScrollLayout->insertWidget(m_railScrollLayout->count() - 1, header);

        auto* sessionList = new QListWidget(m_railScrollContent);
        sessionList->setFrameShape(QFrame::NoFrame);
        sessionList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        sessionList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        sessionList->setStyleSheet(railSessionListQss());
        sessionList->setFocusPolicy(Qt::NoFocus);
        sessionList->setSelectionMode(QAbstractItemView::ExtendedSelection); // Ctrl/Shift+clique
        for (const AIChatSessionInfo& s : sessions) {
            QString dateLabel = s.updatedAt;
            dateLabel.replace(QChar('T'), QChar(' '));
            if (dateLabel.size() > 16) dateLabel = dateLabel.left(16);
            auto* it = new QListWidgetItem(QStringLiteral("%1 — %2").arg(
                s.title.isEmpty() ? tr("Conversa sem título") : s.title, dateLabel));
            it->setData(Qt::UserRole, s.id);
            if (isActive && s.id == m_currentSessionId) {
                QFont f = it->font(); f.setBold(true); it->setFont(f);
                sessionList->setCurrentItem(it);
            }
            sessionList->addItem(it);
        }
        if (isActive) {
            auto* newItem = new QListWidgetItem(QStringLiteral("+ ") + tr("Nova ideia / conversa"));
            newItem->setData(Qt::UserRole, QString());
            sessionList->addItem(newItem);
        }
        const int rowH = sessionList->count() > 0
            ? qMax(20, sessionList->sizeHintForRow(0)) : 0;
        sessionList->setFixedHeight(sessionList->count() * rowH + (sessionList->count() > 0 ? 6 : 0));
        sessionList->setVisible(isActive || forceExpand);

        connect(header, &QToolButton::toggled, sessionList, &QWidget::setVisible);

        connect(sessionList, &QListWidget::itemClicked, this,
                [this, root, name](QListWidgetItem* clicked) {
            // Ctrl/Shift+clique é seleção múltipla pra exclusão em lote, não
            // navegação — não abre/troca de conversa nesse caso.
            if (QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) return;
            const QString sid = clicked->data(Qt::UserRole).toString();
            if (sid.isEmpty()) { startNewSession(); return; } // linha "Nova conversa"
            if (root == m_projectRoot) loadSession(sid);
            else previewForeignSession(root, name, sid);
        });

        // Clique direito numa conversa (não na linha "Nova conversa", que
        // tem id vazio) — oferece excluir, com confirmação de verdade já
        // que apaga o(s) arquivo(s) em disco pra sempre. Se o item clicado
        // já faz parte de uma seleção múltipla (Ctrl/Shift+clique), a ação
        // vale pra seleção inteira — mesmo padrão de gerenciador de
        // arquivos; clicar num item fora da seleção age só sobre ele.
        sessionList->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(sessionList, &QListWidget::customContextMenuRequested, this,
                [this, sessionList, root](const QPoint& pos) {
            QListWidgetItem* item = sessionList->itemAt(pos);
            if (!item) return;
            if (item->data(Qt::UserRole).toString().isEmpty()) return; // "Nova conversa"

            QList<QListWidgetItem*> targets;
            if (item->isSelected() && sessionList->selectedItems().size() > 1) {
                targets = sessionList->selectedItems();
            } else {
                targets = { item };
            }
            QStringList ids;
            for (QListWidgetItem* it : std::as_const(targets)) {
                const QString sid = it->data(Qt::UserRole).toString();
                if (!sid.isEmpty()) ids.append(sid);
            }
            if (ids.isEmpty()) return;

            QMenu menu(this);
            QAction* delAction = menu.addAction(ids.size() == 1
                ? tr("Excluir conversa")
                : tr("Excluir %1 conversas selecionadas").arg(ids.size()));
            const QString singleTitle = item->text();
            connect(delAction, &QAction::triggered, this, [this, root, ids, singleTitle]() {
                const QString question = ids.size() == 1
                    ? tr("Excluir \"%1\" para sempre? Essa ação não pode ser desfeita.").arg(singleTitle)
                    : tr("Excluir %1 conversas para sempre? Essa ação não pode ser desfeita.").arg(ids.size());
                const auto choice = QMessageBox::question(this, tr("Excluir conversa"),
                    question, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (choice != QMessageBox::Yes) return;
                deleteSessionsForRoot(root, ids);
            });
            menu.exec(sessionList->mapToGlobal(pos));
        });

        m_railScrollLayout->insertWidget(m_railScrollLayout->count() - 1, sessionList);
    }
}

void AIChatPanel::startNewSession()
{
    if (m_previewMode) exitPreviewMode();
    saveCurrentSession();
    m_messages.clear();
    m_currentSessionId.clear();
    m_pendingToolTraces.clear();
    m_toolHopCount = 0;
    clearTranscriptUi();
    addMiraBubble(tr("Nova conversa — pode perguntar!"));
    rebuildProjectRail();
    updateChatContext();
}

void AIChatPanel::showSessionMenu()
{
    QMenu menu(this);
    QAction* newAction = menu.addAction(tr("＋ Nova conversa"));
    connect(newAction, &QAction::triggered, this, &AIChatPanel::startNewSession);
    menu.addSeparator();

    const QVector<AIChatSessionInfo> sessions = listSessions();
    if (sessions.isEmpty()) {
        QAction* empty = menu.addAction(tr("(nenhuma conversa salva ainda)"));
        empty->setEnabled(false);
    }
    for (const AIChatSessionInfo& s : sessions) {
        QString dateLabel = s.updatedAt;
        dateLabel.replace(QChar('T'), QChar(' '));
        if (dateLabel.size() > 16) dateLabel = dateLabel.left(16);
        QAction* a = menu.addAction(QStringLiteral("%1 — %2").arg(s.title, dateLabel));
        if (s.id == m_currentSessionId) {
            QFont f = a->font(); f.setBold(true); a->setFont(f);
        }
        const QString sid = s.id;
        connect(a, &QAction::triggered, this, [this, sid]() { loadSession(sid); });
    }
    menu.exec(m_historyBtn->mapToGlobal(QPoint(0, m_historyBtn->height())));
}

void AIChatPanel::openImageGallery()
{
    if (m_projectRoot.isEmpty()) return;
    ImageGalleryDialog dlg(m_projectRoot, this);
    dlg.exec();
}

void AIChatPanel::refreshDocFocusCombo()
{
    if (!m_docFocusCombo) return;
    const QString prevSelection = m_docFocusCombo->currentText();
    m_docFocusCombo->blockSignals(true);
    m_docFocusCombo->clear();
    for (const ScanDoc& d : collectAllDocs()) {
        if (d.key.isEmpty()) continue; // só docs navegáveis (capítulo/ficha), não sintéticos
        m_docFocusCombo->addItem(d.title);
    }
    const int idx = m_docFocusCombo->findText(prevSelection);
    if (idx >= 0) m_docFocusCombo->setCurrentIndex(idx);
    m_docFocusCombo->blockSignals(false);
}

void AIChatPanel::onDocFocusChanged()
{
    if (!m_messages.isEmpty() && m_messages.first().role == QStringLiteral("system")) {
        m_messages[0].content = buildSystemPrompt();
    }
}

QVector<AIChatPanel::ScanDoc> AIChatPanel::collectAllDocs() const
{
    QVector<ScanDoc> docs;
    if (!m_projectModel || m_projectRoot.isEmpty()) return docs;

    for (const Chapter& ch : m_projectModel->chapters()) {
        if (ch.file.isEmpty()) continue;
        const QString cacheKey = DocCache::chapterKey(ch.manuscriptId, ch.id);
        QString html;
        if (m_docCache && m_docCache->has(cacheKey)) {
            html = m_docCache->get(cacheKey);
        } else {
            bool ok = false;
            html = ProjectStorage::readChapter(m_projectRoot, ch.file, &ok);
        }
        const QString text = stripHtmlToPlainText(html);
        if (text.isEmpty()) continue;
        ScanDoc d;
        d.title = ch.title.isEmpty() ? m_projectModel->chapterDisplayLabel(ch) : ch.title;
        d.plainText = text;
        d.key = cacheKey;
        docs.append(d);
    }

    for (const Drawer& drawer : m_projectModel->drawers()) {
        for (const DrawerItem& item : drawer.items) {
            const QString html = DocPreview::resolveDrawerItemHtml(
                &item, m_elementsStore, m_docCache, m_projectRoot, /*includePhoto=*/false);
            const QString text = stripHtmlToPlainText(html);
            if (text.isEmpty()) continue;
            ScanDoc d;
            d.title = item.title.isEmpty() ? tr("Documento sem título") : item.title;
            d.plainText = text;
            d.key = DocCache::itemKey(item.id);
            docs.append(d);
        }
    }

    // Marcadores com comentário — agrupados por documento de origem. Título
    // resolvido via WordCounter::docDisplayName, o mesmo parser de chave
    // "ch:ms:chId"/"it:itemId" já usado no resto do app (não reinventa).
    // Chave preservada (mesma do doc original) — clicar no chip de citação
    // abre o capítulo/ficha onde o marcador está, não um lugar sintético.
    if (m_markerStore) {
        const auto& allMarkers = m_markerStore->allEntries();
        for (auto it = allMarkers.constBegin(); it != allMarkers.constEnd(); ++it) {
            if (it.value().isEmpty()) continue;
            const QString docTitle = m_wordCounter ? m_wordCounter->docDisplayName(it.key()) : it.key();
            QStringList lines;
            for (const MarkerStore::Entry& e : it.value()) {
                lines.append(tr("Marcador (cor %1) sobre \"%2\" — comentário: %3")
                    .arg(e.color, e.text.left(200), e.comment));
            }
            ScanDoc d;
            d.title = tr("Marcadores — %1").arg(docTitle);
            d.plainText = lines.join(QStringLiteral("\n"));
            d.key = it.key();
            docs.append(d);
        }
    }

    // Notas soltas do Pensário — um doc só, já que não são ancoradas a
    // nenhum capítulo/cena específico.
    if (m_notesStore && !m_notesStore->notes().isEmpty()) {
        QStringList lines;
        for (const NotesStore::Note& n : m_notesStore->notes()) {
            lines.append(QStringLiteral("%1: %2").arg(n.title, n.text));
        }
        ScanDoc d;
        d.title = tr("Notas do Pensário");
        d.plainText = lines.join(QStringLiteral("\n\n"));
        docs.append(d);
    }

    // Estatísticas de diálogo por personagem (palavras faladas + "química"/
    // co-ocorrência com outros personagens). Não replica a heurística de
    // presença por menção do StatsPanel (é privada e cara de reimplementar)
    // — só a parte já pública via DialogueStore/DialogueChemistry.
    if (m_dialogueStore) {
        QStringList lines;
        for (const Drawer& drawer : m_projectModel->drawers()) {
            if (drawer.drawerElementType != QStringLiteral("character")) continue;
            for (const DrawerItem& item : drawer.items) {
                if (item.elementId.isEmpty()) continue;
                const int words = m_dialogueStore->dialogueWordsForCharacter(item.elementId);
                if (words <= 0) continue;

                QStringList pairLines;
                const QVector<DialogueChemistry::PairStats> pairs =
                    DialogueChemistry::chemistryForCharacter(m_dialogueStore->dialogues(), item.elementId);
                for (const DialogueChemistry::PairStats& p : pairs) {
                    if (p.scenesTogether <= 0) continue;
                    QString otherName = p.otherElementId;
                    if (m_elementsStore) {
                        if (const Element* e = m_elementsStore->findElement(p.otherElementId)) otherName = e->name;
                    }
                    pairLines.append(tr("%1 (%2 cenas juntos)").arg(otherName).arg(p.scenesTogether));
                }
                lines.append(tr("%1 — %2 palavras de diálogo. Contracena mais com: %3")
                    .arg(item.title).arg(words)
                    .arg(pairLines.isEmpty() ? tr("(sem dados de química)") : pairLines.join(QStringLiteral(", "))));
            }
        }
        if (!lines.isEmpty()) {
            ScanDoc d;
            d.title = tr("Estatísticas de diálogo por personagem");
            d.plainText = lines.join(QStringLiteral("\n"));
            docs.append(d);
        }
    }

    // Construtor — um doc por sistema (não um só pra todos), já que cada
    // sistema é uma unidade temática própria (magia, política, etc.).
    // Menções ao manuscrito dentro do sistema ficam de fora de propósito —
    // são só snapshots do texto que já é pesquisável nos capítulos direto.
    if (m_construtorStore) {
        for (const ConstrutorStore::System& sys : m_construtorStore->systems()) {
            QStringList lines;
            if (!sys.content.isEmpty()) lines.append(sys.content);
            flattenConstrutorNodes(sys.nodes, lines);
            if (lines.isEmpty()) continue;
            ScanDoc d;
            d.title = tr("Construtor — %1").arg(sys.name);
            d.plainText = lines.join(QStringLiteral("\n"));
            docs.append(d);
        }
    }

    // Glossário — um doc só, termo:definição.
    if (m_glossaryStore && !m_glossaryStore->entries().isEmpty()) {
        QStringList lines;
        for (const GlossaryStore::Entry& e : m_glossaryStore->entries()) {
            lines.append(QStringLiteral("%1: %2").arg(e.term, e.definition));
        }
        ScanDoc d;
        d.title = tr("Glossário");
        d.plainText = lines.join(QStringLiteral("\n"));
        docs.append(d);
    }

    // Pins do Mapa-múndi (lugares que o autor marcou, reais ou fictícios,
    // com rótulo + nota livre) — diferente de lookup_world_data, que é o
    // dataset geográfico do mundo real embutido no app.
    if (m_mapPinsStore && !m_mapPinsStore->pins().isEmpty()) {
        QStringList lines;
        for (const MapPinsStore::Pin& p : m_mapPinsStore->pins()) {
            QString line = p.label;
            if (!p.note.trimmed().isEmpty()) line += QStringLiteral(": ") + p.note.trimmed();
            if (!p.linkLabel.isEmpty()) line += tr(" (vinculado a: %1)").arg(p.linkLabel);
            lines.append(line);
        }
        ScanDoc d;
        d.title = tr("Pins do Mapa-múndi");
        d.plainText = lines.join(QStringLiteral("\n"));
        docs.append(d);
    }

    return docs;
}

QVector<QPair<QString, QString>> AIChatPanel::parseSummaryFile(const QString& raw) const
{
    QVector<QPair<QString, QString>> sections;
    if (raw.isEmpty()) return sections;

    const QStringList blocks = raw.split(QStringLiteral("\n## "));
    for (int i = 0; i < blocks.size(); ++i) {
        QString block = blocks[i];
        if (i == 0) {
            if (!block.startsWith(QStringLiteral("## "))) continue; // lixo antes do primeiro título
            block = block.mid(3);
        }
        const int nl = block.indexOf(QChar('\n'));
        if (nl < 0) continue;
        const QString title = block.left(nl).trimmed();
        const QString content = block.mid(nl + 1).trimmed();
        if (title.isEmpty()) continue;
        sections.append({ title, content });
    }
    return sections;
}

void AIChatPanel::writeSummaryFile(const QVector<QPair<QString, QString>>& sections) const
{
    const QString dirPath = m_projectRoot + QStringLiteral("/ai_context");
    QDir().mkpath(dirPath);
    QFile f(dirPath + QStringLiteral("/resumo_projeto.md"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    for (const auto& section : sections) {
        out << QStringLiteral("## %1\n\n%2\n\n").arg(section.first, section.second);
    }
}

void AIChatPanel::startProjectScan()
{
    if (m_scanning || m_client->isBusy()) return;
    if (!m_projectModel || m_projectRoot.isEmpty()) return;

    // Incremental: só entra na fila quem ainda não tem seção salva no
    // resumo atual. Reler algo já resumido é sempre sob pedido explícito
    // (tool resummarize_document), nunca automático — ver decisão do
    // usuário registrada na memória ai-assistant-feature.
    m_scanExistingSections = parseSummaryFile(loadProjectSummaryFile());
    QSet<QString> alreadyDone;
    for (const auto& section : m_scanExistingSections) alreadyDone.insert(section.first);

    m_scanQueue.clear();
    m_scanSummaries.clear();
    m_scanIndex = 0;

    for (ScanDoc d : collectAllDocs()) {
        if (alreadyDone.contains(d.title)) continue;
        d.plainText = d.plainText.left(kMaxCharsPerDoc);
        m_scanQueue.append(d);
    }

    if (m_scanQueue.isEmpty()) {
        addMiraBubble(m_scanExistingSections.isEmpty()
            ? tr("Não encontrei documentos com conteúdo pra ler neste projeto.")
            : tr("Já li todos os %1 documentos que existem hoje — nada novo pra ler. Se você "
                 "reescreveu algo, me peça pra reler aquele documento específico.")
                  .arg(m_scanExistingSections.size()));
        return;
    }

    QSettings settings;
    if (settings.value(QStringLiteral("ai/apiKey")).toString().isEmpty()) {
        addMiraBubble(tr("Nenhuma chave de API configurada. Abra Configurações → Assistente de IA e cole sua chave."));
        return;
    }

    m_scanning = true;
    m_sendBtn->setEnabled(false);
    m_inputEdit->setEnabled(false);
    m_attachBtn->setEnabled(false);
    m_scanBtn->setEnabled(false);
    addMiraBubble(tr("Lendo %1 documento(s) novo(s) do projeto…").arg(m_scanQueue.size()));
    processNextScanItem();
}

void AIChatPanel::processNextScanItem()
{
    if (m_scanIndex >= m_scanQueue.size()) {
        finishScan();
        return;
    }

    const ScanDoc& doc = m_scanQueue[m_scanIndex];
    m_statusLabel->setText(tr("Lendo: %1 (%2/%3)…")
        .arg(doc.title).arg(m_scanIndex + 1).arg(m_scanQueue.size()));
    m_statusLabel->setVisible(true);

    QSettings settings;
    m_client->setApiKey(settings.value(QStringLiteral("ai/apiKey")).toString());
    m_client->setBaseUrl(settings.value(QStringLiteral("ai/baseUrl"),
        QStringLiteral("https://api.openai.com/v1")).toString());
    m_client->setModel(settings.value(QStringLiteral("ai/model"),
        QStringLiteral("gpt-4o-mini")).toString());
    m_client->setTools({}); // resumo não usa tools — só o chat livre usa search_project

    AIChatMessage sys;
    sys.role = QStringLiteral("system");
    sys.content = docSummaryInstruction();

    AIChatMessage user;
    user.role = QStringLiteral("user");
    user.content = QStringLiteral("Título: %1\n\n%2").arg(doc.title, doc.plainText);

    m_client->sendMessage({ sys, user });
}

void AIChatPanel::onScanDocFinished(const QString& summary)
{
    m_scanSummaries.append(summary.trimmed());
    ++m_scanIndex;
    processNextScanItem();
}

void AIChatPanel::finishScan()
{
    m_scanning = false;
    m_sendBtn->setEnabled(true);
    m_inputEdit->setEnabled(true);
    m_attachBtn->setEnabled(true);
    m_scanBtn->setEnabled(true);
    m_statusLabel->setVisible(false);

    QVector<QPair<QString, QString>> merged = m_scanExistingSections;
    for (int i = 0; i < m_scanQueue.size() && i < m_scanSummaries.size(); ++i) {
        merged.append({ m_scanQueue[i].title, m_scanSummaries[i] });
    }
    writeSummaryFile(merged);

    addMiraBubble(tr("Pronto — li %1 documento(s) novo(s) e atualizei os resumos. Já posso responder com esse contexto.")
        .arg(m_scanQueue.size()));

    // Se já havia conversa em andamento, atualiza o system prompt com o
    // resumo novo em vez de esperar uma nova conversa começar do zero.
    if (!m_messages.isEmpty() && m_messages.first().role == QStringLiteral("system")) {
        m_messages[0].content = buildSystemPrompt();
    }

    if (m_docFocusCheck && m_docFocusCheck->isChecked()) refreshDocFocusCombo();
}

void AIChatPanel::mousePressEvent(QMouseEvent* e)
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

void AIChatPanel::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging) {
        const QPoint delta = e->pos() - m_dragOffset;
        move(pos() + delta);
        e->accept();
        return;
    }
    QFrame::mouseMoveEvent(e);
}

void AIChatPanel::mouseReleaseEvent(QMouseEvent* e)
{
    if (m_dragging) {
        m_dragging = false;
        if (m_header) m_header->setCursor(Qt::OpenHandCursor);
        e->accept();
        return;
    }
    QFrame::mouseReleaseEvent(e);
}

bool AIChatPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_resizeGrip) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_resizingPanel = true;
                m_resizeStartMouse = me->globalPosition().toPoint();
                m_resizeStartSize = size();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (m_resizingPanel) {
                auto* me = static_cast<QMouseEvent*>(event);
                const QPoint delta = me->globalPosition().toPoint() - m_resizeStartMouse;
                const int newW = qBound(360, m_resizeStartSize.width() + delta.x(), 1600);
                const int newH = qBound(320, m_resizeStartSize.height() + delta.y(), 1600);
                resize(newW, newH);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_resizingPanel) {
                m_resizingPanel = false;
                saveCurrentPanelSize();
                return true;
            }
        }
    } else if (watched == m_inputEdit && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) &&
            !(ke->modifiers() & Qt::ShiftModifier)) {
            const QString text = m_inputEdit->toPlainText().trimmed();
            if (!text.isEmpty()) sendUserMessage(text);
            return true; // consome — Shift+Enter continua inserindo quebra de linha normalmente
        }
    } else if (watched == m_inputEdit && event->type() == QEvent::Resize) {
        // Painel redimensionado com texto já digitado — sem isso a largura
        // usada pro cálculo de quebra ficava desatualizada até a próxima
        // tecla, deixando o texto "invisível" (fora da área calculada).
        fitInputHeight();
    } else if (watched == m_studioHeaderWidget && event->type() == QEvent::MouseButtonPress) {
        openPersonalityDialog();
        return true;
    }
    return QFrame::eventFilter(watched, event);
}
