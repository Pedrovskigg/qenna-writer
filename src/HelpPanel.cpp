#include "HelpPanel.h"

#include "Theme.h"

#include <QCoreApplication>
#include <QDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QScreen>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace {
constexpr int kPanelWidth = 980;
constexpr int kPanelHeight = 640;
constexpr int kMinWidth = 760;
constexpr int kMinHeight = 480;
constexpr int kGapBelowAnchor = 6;
constexpr int kSidebarWidth = 205;
constexpr int kThumbWidth = 480;
// Larguras de exibição das screenshots — aumentadas propositalmente acima da
// resolução nativa de várias delas (leve upscale) porque a qualidade das
// capturas é boa o bastante pra aguentar e, em compensação, ficam legíveis
// sem precisar clicar pra expandir toda hora (pedido do usuário 2026-07-18:
// "aumenta um pouco o tamanho dos prints").
constexpr int kManuscriptsThumbWidth = 460; // nativo 372x246
constexpr int kDrawersCreateThumbWidth  = 650; // nativo 556
constexpr int kDrawersOpenThumbWidth    = 460; // nativo 372
constexpr int kDrawersMenuThumbWidth    = 400; // nativo 300
constexpr int kExportTopbarThumbWidth   = 340; // nativo 245
constexpr int kExportPanelThumbWidth    = 500; // nativo 475
constexpr int kEditorToolbarThumbWidth      = 720; // nativo 893
constexpr int kEditorSelectionThumbWidth    = 500; // nativo 448
constexpr int kEditorFocusModesThumbWidth   = 160; // nativo 76 — recorte minúsculo, upscale maior
constexpr int kEditorFocusOnThumbWidth      = 320; // nativo 230
constexpr int kEditorPageConfigThumbWidth   = 480; // nativo 410
constexpr int kCounterThumbWidth       = 360; // nativo 269
constexpr int kCounterPanelThumbWidth  = 340; // nativo 260 (recorte alto, 260x865)
constexpr int kCalendarThumbWidth      = 520; // nativo 474
constexpr int kStatisticsThumbWidth    = 420; // nativo 337
constexpr int kOffDaysThumbWidth       = 340; // nativo 255
constexpr int kRefMenuOpenThumbWidth   = 500; // nativo 498, recorte alto
constexpr int kRefMenuSearchThumbWidth = 500; // nativo 460
constexpr int kRefMenuEditThumbWidth   = 480; // nativo 454, recorte alto
constexpr int kTimelinePanelThumbWidth       = 620; // nativo 748
constexpr int kTimelineManuscriptThumbWidth  = 420; // nativo 341
constexpr int kTimelineChapterSceneThumbWidth = 420; // nativo 344
constexpr int kTimelineCharactersThumbWidth  = 560; // nativo 621
constexpr int kTimelineEventCreatorThumbWidth = 380; // nativo 480x621, recorte alto
constexpr int kTimelineCreatorThumbWidth     = 420; // nativo 408
constexpr int kTimelineModeTrackThumbWidth   = 520; // nativo 561
constexpr int kTimelineModeBranchesThumbWidth = 620; // nativo 864
constexpr int kTimelineModeSpiralThumbWidth  = 520; // nativo 667
constexpr int kTimelineLineFocusThumbWidth   = 640; // nativo 893
constexpr int kCoverContextMenuThumbWidth = 320; // nativo 262
constexpr int kCoverAppEditingThumbWidth  = 640; // nativo 936
constexpr int kMarkerSelectMenuThumbWidth   = 500; // nativo 455
constexpr int kMarkerColorPickerThumbWidth  = 300; // nativo 284
constexpr int kMarkerCommentPickerThumbWidth = 340; // nativo 328
constexpr int kMarkerCommentHoverThumbWidth = 380; // nativo 340
constexpr int kMarkerPlainThumbWidth        = 400; // nativo 387
}

HelpPanel::HelpPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("helpPanel"));
    // Janela normal (não-modal, com decoração própria do SO): fica aberta ao
    // lado do app, sem fechar sozinha quando o usuário clica de volta no
    // editor pra seguir as instruções.
    setWindowFlags(Qt::Window);
    setWindowTitle(tr("Ajuda"));
    setMinimumSize(kMinWidth, kMinHeight);
    resize(kPanelWidth, kPanelHeight);

    buildTopics();
    buildUi();
    applyTheme();

    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &HelpPanel::applyTheme);

    rebuildList();
    updateContent();

    hide();
}

// Estrutura de tópicos herdada do Help Panel do Mira 1 — os rótulos vêm de
// lá, o conteúdo (contentFor) ainda não: será reescrito do zero, mais
// detalhado e com screenshots, nesta versão.
void HelpPanel::buildTopics()
{
    m_topics = {
        { QStringLiteral("comece-aqui"), tr("Comece aqui") },
        { QStringLiteral("manuscritos"), tr("Manuscritos") },
        { QStringLiteral("gavetas"), tr("Gavetas") },
        { QStringLiteral("exportacao"), tr("Exportação") },
        { QStringLiteral("editor"), tr("Editor") },
        { QStringLiteral("meta-diaria-contador"), tr("Meta Diária e Contador") },
        { QStringLiteral("menu-referencia"), tr("Menu de Referência") },
        { QStringLiteral("funcao-timeline"), tr("Função Timeline") },
        { QStringLiteral("criar-capas"), tr("Criar capas") },
        { QStringLiteral("atalhos-teclado"), tr("Atalhos de teclado") },
        { QStringLiteral("marcadores"), tr("Marcadores e Comentários") },
        { QStringLiteral("memorias"), tr("Memórias") },
        { QStringLiteral("criar-documentos"), tr("Criar Documentos a partir do texto ou comentários") },
        { QStringLiteral("funcao-temas"), tr("Função de Temas") },
        { QStringLiteral("construtor"), tr("Construtor") },
        { QStringLiteral("pensario"), tr("Pensário") },
        { QStringLiteral("consistencia"), tr("Consistência") },
        { QStringLiteral("recursos-adicionais"), tr("Recursos adicionais") },
    };
}

void HelpPanel::buildUi()
{
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* sidebar = new QWidget(this);
    sidebar->setObjectName(QStringLiteral("helpSidebar"));
    sidebar->setFixedWidth(kSidebarWidth);
    auto* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(10, 12, 6, 12);
    sideLayout->setSpacing(6);

    m_sidebarTitle = new QLabel(tr("Ajuda"), sidebar);
    m_sidebarTitle->setObjectName(QStringLiteral("helpSidebarTitle"));
    sideLayout->addWidget(m_sidebarTitle);

    m_list = new QListWidget(sidebar);
    m_list->setObjectName(QStringLiteral("helpList"));
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setWordWrap(true);
    connect(m_list, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem*, QListWidgetItem*) { onTopicSelected(); });
    sideLayout->addWidget(m_list, 1);

    root->addWidget(sidebar);

    auto* right = new QVBoxLayout();
    right->setContentsMargins(12, 10, 10, 10);
    right->setSpacing(6);

    m_contentTitle = new QLabel(this);
    m_contentTitle->setObjectName(QStringLiteral("helpContentTitle"));
    right->addWidget(m_contentTitle);

    m_content = new QTextBrowser(this);
    m_content->setObjectName(QStringLiteral("helpContent"));
    m_content->setOpenExternalLinks(false);
    m_content->setOpenLinks(false);
    m_content->setFrameShape(QFrame::NoFrame);
    m_content->setLineWrapMode(QTextEdit::WidgetWidth);
    m_content->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_content, &QTextBrowser::anchorClicked, this, &HelpPanel::onAnchorClicked);
    right->addWidget(m_content, 1);

    root->addLayout(right, 1);
}

void HelpPanel::applyTheme()
{
    setStyleSheet(QStringLiteral(
        "QFrame#helpPanel {"
        "  background: %1;"
        "}"
        "QWidget#helpSidebar {"
        "  background: %6; border-right: 1px solid %2;"
        "}"
        "QLabel#helpSidebarTitle { color: %3; font-size: 13px; font-weight: 600; }"
        "QLabel#helpContentTitle { color: %3; font-size: 16px; font-weight: 600; }"
        "QListWidget#helpList {"
        "  background: transparent; color: %4;"
        "  border: none; outline: 0;"
        "}"
        "QListWidget#helpList::item { padding: 7px 8px; border-radius: 4px; }"
        "QListWidget#helpList::item:hover { background: %5; color: %3; }"
        "QListWidget#helpList::item:selected { background: %7; color: %3; }"
        "QTextBrowser#helpContent {"
        "  background: transparent; color: %3;"
        "  border: none; font-size: 14.5px;"
        "}"
    ).arg(Theme::panelBackground(),
          Theme::panelBorder(),
          Theme::textPrimary(),
          Theme::textMuted(),
          Theme::hoverOverlay(),
          Theme::editorBackground(),
          Theme::pressedOverlay()));
}

void HelpPanel::rebuildList()
{
    if (!m_list) return;
    m_list->clear();
    for (const Topic& t : m_topics) {
        auto* item = new QListWidgetItem(t.label, m_list);
        item->setData(Qt::UserRole, t.id);
    }
    if (m_list->count() > 0) m_list->setCurrentRow(0);
}

void HelpPanel::onTopicSelected()
{
    auto* item = m_list ? m_list->currentItem() : nullptr;
    m_selectedId = item ? item->data(Qt::UserRole).toString() : QString();
    updateContent();
}

QString HelpPanel::contentFor(const QString& id) const
{
    if (id == QStringLiteral("comece-aqui")) return startHereContent();
    if (id == QStringLiteral("manuscritos")) return manuscriptsContent();
    if (id == QStringLiteral("gavetas")) return drawersContent();
    if (id == QStringLiteral("exportacao")) return exportContent();
    if (id == QStringLiteral("editor")) return editorContent();
    if (id == QStringLiteral("meta-diaria-contador")) return dailyGoalCounterContent();
    if (id == QStringLiteral("menu-referencia")) return referenceMenuContent();
    if (id == QStringLiteral("atalhos-teclado")) return keyboardShortcutsContent();
    if (id == QStringLiteral("marcadores")) return markersContent();
    if (id == QStringLiteral("memorias")) return memoriesContent();
    if (id == QStringLiteral("funcao-timeline")) return timelineContent();
    if (id == QStringLiteral("criar-capas")) return coverCreatorContent();
    // Placeholder até reescrevermos o conteúdo das demais seções.
    return QStringLiteral("<p>%1</p>")
        .arg(tr("Conteúdo desta seção será adicionado em breve."));
}

// Conteúdo escrito pelo usuário em help-panel/start-here/, montado aqui em HTML.
// A imagem fica em miniatura (números do callout ainda legíveis) e é
// clicável — abre ampliada numa janela própria via onAnchorClicked/openImageZoom.
QString HelpPanel::startHereContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr("Olá, seja bem-vindo ao Qenna Writer!"));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Qenna Writer é um programa voltado para a escrita criativa e conta com "
        "inúmeras opções para torná-la mais fácil, organizada e aprofundada."));
    html += QStringLiteral("<p>%1</p>").arg(tr("Vamos começar!"));
    html += QStringLiteral("<p>%1</p>").arg(tr("Aqui, está a sua interface básica de usuário."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/start-here/numbered-ui.png' style='text-decoration:none;'>"
        "<img src=':/help/start-here/numbered-ui.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O editor é dividido em seções, para que tudo seja acessível no mesmo lugar sem "
        "que se espalhe. Conforme a screenshot acima, aqui estão o que cada número é:"));

    struct Step { const char* title; const char* desc; };
    const Step steps[] = {
        { QT_TR_NOOP("Opções diversas do editor de texto."),
          QT_TR_NOOP("Negrito, itálico, tamanho e fonte, alinhamento, adicionar imagens.") },
        { QT_TR_NOOP("Opções básicas do programa."),
          QT_TR_NOOP("Retornar ao menu, criar novo projeto, carregar, salvar e exportar documentos.") },
        { QT_TR_NOOP("Acesso geral de projeto e planejamento."),
          QT_TR_NOOP("Informações da obra, Lousa, Timeline, Manuscrito e organização de Grupos.") },
        { QT_TR_NOOP("Gavetas."),
          QT_TR_NOOP("Para criar novas gavetas e acessar as que já existem. Não há um limite de quantas gavetas você pode criar.") },
        { QT_TR_NOOP("Contador."),
          QT_TR_NOOP("Aqui você visualiza o seu contador de palavras e acessa o gerenciamento das suas metas de escrita.") },
        { QT_TR_NOOP("Documento em edição."),
          QT_TR_NOOP("Exibe o documento que está sendo editado no momento.") },
        { QT_TR_NOOP("Opções gerais."),
          QT_TR_NOOP("Aqui, você consegue acessar o Glossário, ativar o Editor Focado e o Modo Foco (que "
                     "apesar dos nomes similares, são funções diferentes). O Editor Focado recua toda a "
                     "UI e deixa somente a página em exibição. O Modo Foco esmaece o texto e foca somente "
                     "no parágrafo que está sendo escrito.") },
        { QT_TR_NOOP("Opções de Ambiente de Trabalho."),
          QT_TR_NOOP("Acesso a função de Lembretes e Som Imersivo.") },
        { QT_TR_NOOP("Opções de Referência e Criação."),
          QT_TR_NOOP("Aqui, você pode acessar as ferramentas: Construtor, Pensário e Menu de Referência.") },
        { QT_TR_NOOP("Configurações do programa."),
          QT_TR_NOOP("Alterar o Tema, configurar e ativar o modo tela cheia.") },
    };

    // Numeração manual em vez de <ol>: o motor de rich text do Qt tem
    // problemas de layout com listas (indent/margin bugados, empurram o
    // texto todo pra direita). Um <p> com o número em negrito é confiável.
    for (int i = 0; i < static_cast<int>(sizeof(steps) / sizeof(steps[0])); ++i) {
        html += QStringLiteral("<p style='margin-bottom:12px;'><b>%1- %2</b><br>%3</p>")
            .arg(QString::number(i + 1), tr(steps[i].title), tr(steps[i].desc));
    }

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "A UI básica do app é pensada para manter as opções fora do seu caminho e não lhe "
        "atrapalharem durante o processo de escrita. Pode parecer que são muitos botões, mas "
        "acredite, é fácil de se acostumar."));
    return html;
}

// Conteúdo escrito pelo usuário em help-panel/manuscripts/, montado aqui em HTML.
QString HelpPanel::manuscriptsContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr("Bem-vindo à gaveta de Manuscrito — ou só Manuscrito."));
    html += QStringLiteral("<p>%1</p>").arg(tr("É aqui onde você gerencia os manuscritos do seu projeto."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Não há um limite de quantos manuscritos você pode ter. Você pode escrever uma saga de 60 "
        "livros no mesmo projeto — em teoria pelo menos."));
    html += QStringLiteral("<p>%1</p>").arg(tr("A gaveta de Manuscrito é bem direta, então vamos aos detalhes."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/manuscripts/manuscript-tutorial.png' style='text-decoration:none;'>"
        "<img src=':/help/manuscripts/manuscript-tutorial.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kManuscriptsThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("Barra do Manuscrito"),
             tr("É nessa barra que você escolhe o manuscrito aberto. Você também pode renomear e "
                "excluir manuscritos clicando com o botão direito nela quando o manuscrito que quiser "
                "renomear ou excluir esteja selecionado."));

    html += QStringLiteral(
        "<p style='margin-bottom:4px;'><b>2- %1</b></p>"
        "<p style='margin-bottom:4px;'>%2</p>"
        "<p style='margin-bottom:4px;'>%3</p>"
        "<p style='margin-bottom:12px;'>%4</p>")
        .arg(tr("Criar novo capítulo"),
             tr("Nesse botão, você cria novos capítulos."),
             tr("É necessário que já haja um manuscrito para que capítulos sejam criados. Caso não "
                "tenha um, o app chamará a opção para criá-lo."),
             tr("Ao criar um novo capítulo, o app chamará uma janela pedindo duas informações: o nome "
                "do capítulo e quando ele se passa. Sobre quando ele se passa, essa informação é usada "
                "para organização da linha do tempo na Timeline, mas falaremos disso quando chegar a "
                "hora."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>3- %1</b><br>%2</p>")
        .arg(tr("É através desse botão que você abre a gaveta de Manuscritos."),
             tr("Ele fica na barra à esquerda, acessível a qualquer momento."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>4- %1</b><br>%2</p>")
        .arg(tr("Capítulos"),
             tr("Os capítulos criados aparecem nessa lista. Para acessá-los no editor, é somente "
                "clicar. Você também pode usar o clique direito para acessar as opções dele (renomear, "
                "excluir, abrir no Menu de Referência, definir elementos presentes e etc)."));

    html += QStringLiteral(
        "<p style='margin-bottom:4px;'><b>5- %1</b></p>"
        "<p style='margin-bottom:4px;'>%2</p>"
        "<p style='margin-bottom:2px;'>%3</p>"
        "<p style='margin:2px 0 2px 16px; font-family:monospace;'>%4</p>"
        "<p style='margin-bottom:12px;'>%5</p>")
        .arg(tr("Cenas"),
             tr("As cenas dos capítulos são exibidas abaixo deles conforme a imagem. Ao usar o clique "
                "direito em uma, você também pode acessar as opções dela."),
             tr("Para criar uma cena, digite quatro riscas em uma linha vazia e aperte Enter:"),
             QStringLiteral("----"),
             tr("Ao fazer isso, o app cria uma nova cena automaticamente. Com isso, você pode gerar "
                "novas cenas a qualquer momento no seu capítulo, sem atrapalhar o seu fluxo de "
                "escrita."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>6- %1</b><br>%2</p>")
        .arg(tr("Criar Manuscrito"),
             tr("Aperte nesse botão para criar novos Manuscritos."));

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/drawers/, montado aqui em HTML.
QString HelpPanel::drawersContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "As gavetas são uma parte fundamental da organização do seu projeto no app. Nelas, "
        "você pode dividir os seus documentos com base em contexto e função, mantendo cada "
        "coisa em seu devido lugar."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Não há um limite de gavetas — você pode ter quantas o seu projeto precisar."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/drawers/drawer-create.png' style='text-decoration:none;'>"
        "<img src=':/help/drawers/drawer-create.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kDrawersCreateThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Para criar uma gaveta, clique no botão de \"+\" que aparece na imagem acima (1). Ao "
        "clicar nele, surge um popup de criação (2)."));
    html += QStringLiteral("<p>%1</p>").arg(tr("Nesse popup, você tem as seguintes opções:"));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("Ícone e cor da gaveta."),
             tr("Você pode escolher qualquer ícone da lista e escolher uma cor na paleta de cores."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("Nome da gaveta."),
             tr("Defina o nome da sua gaveta. Com base no nome, a gaveta pode receber elementos "
                "de forma automática."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>3- %1</b><br>%2</p>")
        .arg(tr("Elemento das gavetas."),
             tr("Os elementos definem o que aquela gaveta armazena. Atualmente, há três opções: "
                "Personagens, Cenários e Objetos. Gavetas com esses três elementos têm um padrão "
                "de exibição diferente e recebem opções adicionais com base no elemento dela — "
                "falaremos dessas opções abaixo."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Após criar sua gaveta, você a encontrará na barra da esquerda e pode clicar para abri-la."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/drawers/drawer-open.png' style='text-decoration:none;'>"
        "<img src=':/help/drawers/drawer-open.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kDrawersOpenThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Dentro da gaveta aberta, você tem as opções a seguir (seguindo as áreas numeradas da "
        "imagem acima)."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("Fixar, Modo de Consistência, alternar exibição entre listas e blocos, "
                "configurar o tamanho dos blocos e ordenar os itens da gaveta."),
             tr("A ordenação inclui opções como A-Z e ordem de criação, entre outras."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("O botão grande e colorido."),
             tr("Nele, você cria novos documentos dentro dessa gaveta. Nas opções abaixo dele, "
                "você consegue criar pastas dentro da gaveta e acessar pastas que já existam no "
                "projeto."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Algumas opções só existem em gavetas de elementos (como o Modo de Consistência e o "
        "tamanho dos blocos)."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Para os documentos e itens dentro das gavetas, você encontra as seguintes opções ao "
        "dar um clique direito neles:"));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/drawers/drawer-item-menu.png' style='text-decoration:none;'>"
        "<img src=':/help/drawers/drawer-item-menu.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kDrawersMenuThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("Editar metadados..."),
             tr("Altera nome, foto e demais opções. Para personagens, há opções adicionais, "
                "como definir apelido ou narrador."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("Remover elemento."),
             tr("Remove o elemento de um documento (torna um cenário um documento comum, por "
                "exemplo)."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>3- %1</b><br>%2</p>")
        .arg(tr("Adicionar ao grupo."),
             tr("Adiciona o documento a um grupo específico da função de Grupos (falaremos dela "
                "mais tarde)."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>4- %1</b><br>%2</p>")
        .arg(tr("Abrir no Menu de Referência."),
             tr("Abre o documento em questão no Menu de Referência."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>5- %1</b><br>%2</p>")
        .arg(tr("Mover para."),
             tr("Move o documento para outra gaveta."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>6- %1</b><br>%2</p>")
        .arg(tr("Excluir."),
             tr("Exclui o documento (quem diria, hein?)."));

    return html;
}

// Conteúdo escrito pelo assistente em help-panel/export/, revisado pelo usuário,
// montado aqui em HTML.
QString HelpPanel::exportContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "A exportação é onde você tira o que escreveu de dentro do app e transforma num "
        "arquivo de verdade, pronto pra ler em outro programa, mandar pra alguém ou até "
        "publicar."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Pra abrir, clique no botão \"Exportar\" na barra de ferramentas do topo do editor."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/export/export-topbar.png' style='text-decoration:none;'>"
        "<img src=':/help/export/export-topbar.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kExportTopbarThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Isso abre uma janela com três partes: uma árvore de seleção, as opções de "
        "formato/modo, e o botão de exportar."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/export/export-panel.png' style='text-decoration:none;'>"
        "<img src=':/help/export/export-panel.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kExportPanelThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral(
        "<p style='margin-bottom:4px;'><b>1- %1</b></p>"
        "<p style='margin-bottom:4px;'>%2</p>"
        "<p style='margin-bottom:4px;'>%3</p>"
        "<p style='margin-bottom:12px;'>%4</p>")
        .arg(tr("Árvore de seleção."),
             tr("Você escolhe exatamente o que quer exportar. Ela é dividida em duas seções:"),
             tr("Manuscritos: mostra cada manuscrito do projeto com seus capítulos dentro. "
                "Você pode marcar o manuscrito inteiro (marca todos os capítulos de uma vez) "
                "ou só capítulos específicos. Gavetas: mostra suas gavetas, com pastas e "
                "documentos dentro delas, do mesmo jeito."),
             tr("Os botões \"Selecionar tudo\" e \"Desmarcar tudo\" no topo ajudam quando você "
                "quer exportar (quase) tudo de uma vez. O contador no canto (\"X / Y\") mostra "
                "quantos itens estão marcados no momento."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("Formato."),
             tr("Escolha entre ODT, PDF, EPUB ou DOCX, clicando no botão do formato desejado."));

    html += QStringLiteral(
        "<p style='margin-bottom:4px;'><b>3- %1</b></p>"
        "<p style='margin-bottom:4px;'>%2</p>"
        "<p style='margin-bottom:4px;'>%3</p>"
        "<p style='margin-bottom:12px;'>%4</p>")
        .arg(tr("Modo do manuscrito."),
             tr("Só importa se você marcou mais de um capítulo:"),
             tr("Documento único: junta todos os capítulos marcados num arquivo só, na ordem "
                "deles."),
             tr("Capítulos separados: gera um arquivo pra cada capítulo."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>4- %1</b><br>%2</p>")
        .arg(tr("Marcadores."),
             tr("Se você usa a função de Marcadores (marcações de texto tipo grifo) no seu "
                "texto, aqui você escolhe se eles aparecem no arquivo exportado ou são "
                "removidos."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Um aviso que já vem na própria janela: se uma cena tiver variações criadas, a "
        "exportação sempre usa a variação marcada como primária — as outras não entram no "
        "arquivo."));

    html += QStringLiteral(
        "<p style='margin-bottom:4px;'>%1</p>"
        "<p style='margin-bottom:4px;'>%2</p>"
        "<p style='margin-bottom:12px;'>%3</p>")
        .arg(tr("Depois de escolher tudo, clique em \"Exportar (N)\" — o número mostra quantos "
                "itens serão exportados. O app pergunta onde salvar:"),
             tr("Se for só 1 arquivo, ele pede direto o nome/local do arquivo."),
             tr("Se for mais de um (ex.: \"capítulos separados\" com vários marcados, ou "
                "capítulos + itens de gaveta juntos), o app empacota tudo num .zip e pergunta "
                "onde salvar esse zip."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Durante a exportação (principalmente em PDF, que pode demorar um pouco mais), "
        "aparece um aviso na tela — é normal, só esperar terminar. No final, uma confirmação "
        "de sucesso aparece."));

    return html;
}

// Conteúdo escrito pelo assistente em help-panel/editor/, revisado pelo usuário,
// montado aqui em HTML.
QString HelpPanel::editorContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Editor é onde você realmente escreve — é a página em branco (ou não tão em branco "
        "assim) que ocupa o centro da tela. É a parte do app onde você passará mais tempo, "
        "então foi construída para ser confortável."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>1- %1</b></p>")
        .arg(tr("Formatação (barra de ferramentas do topo)."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/editor/toolbar-options.png' style='text-decoration:none;'>"
        "<img src=':/help/editor/toolbar-options.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kEditorToolbarThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Você encontrará as opções de Fonte, Tamanho, Espaçamento, Indentação (\"¶\"), "
        "Alinhamento, Negrito/Itálico/Sublinhado/Tachado. Diferente de negrito/itálico."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Fonte/Tamanho/Espaçamento/Indentação não exigem seleção — valem pro documento "
        "inteiro que você está editando. O botão Espaçamento abre um popup com entre-linhas, "
        "espaço antes e depois do parágrafo."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "O botão Alinhamento tem um extra: \"Aplicar em\" (só esse doc / todos / manuscrito / "
        "gavetas). Isso é diferente de Configurações → Página de escrita, que mexe no tamanho "
        "da folha e margens (visual do app, não do texto)."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("Inserir imagem."),
             tr("Seletor de arquivo → diálogo com prévia, alinhamento e largura ajustável. "
                "Clicar na imagem depois de inserida abre um menu pra realinhar/redimensionar "
                "sem refazer tudo."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>3- %1</b></p>")
        .arg(tr("Selecionar um trecho."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Um menu flutuante aparece acima da seleção: formatação, marcador, marcador com "
        "comentário, Glossário, criar documento, criar evento na timeline, adicionar à "
        "memória, alinhamento. Com essa opção, você mudar o alinhamento ou adicionar negrito "
        "em um trecho específico, por exemplo."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/editor/selection-menu.png' style='text-decoration:none;'>"
        "<img src=':/help/editor/selection-menu.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kEditorSelectionThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Falaremos das Memórias, Glossário e Timeline mais tarde."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>4- %1</b></p>")
        .arg(tr("Editor Focado × Modo Foco."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/editor/focus-modes.png' style='text-decoration:none;'>"
        "<img src=':/help/editor/focus-modes.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kEditorFocusModesThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Editor Focado recua toda a UI, deixa só a página (mouse no topo/esquerda traz de "
        "volta)."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Modo Foco (Ctrl+F10) esmaece o texto todo menos o parágrafo atual (segurar Ctrl "
        "mostra tudo por um instante)."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/editor/focus-mode-on.png' style='text-decoration:none;'>"
        "<img src=':/help/editor/focus-mode-on.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kEditorFocusOnThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Dá pra usar os dois juntos para um modo foco extremo."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>5- %1</b></p>")
        .arg(tr("Opções adicionais."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Além disso, você pode acessar as Configurações e definir a largura e altura da "
        "página."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/editor/page-configuration.png' style='text-decoration:none;'>"
        "<img src=':/help/editor/page-configuration.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kEditorPageConfigThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    return html;
}

// Conteúdo escrito pelo assistente em help-panel/daily-goal-counter/, revisado
// pelo usuário, montado aqui em HTML.
QString HelpPanel::dailyGoalCounterContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Contador é o painel flutuante no canto inferior esquerdo da tela — aquele que "
        "mostra quantas palavras você já escreveu. Além de contar, ele também cuida da sua "
        "meta diária, de sprints de escrita e guarda um histórico completo do seu progresso."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/daily-goal-counter/counter.png' style='text-decoration:none;'>"
        "<img src=':/help/daily-goal-counter/counter.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kCounterThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr("Ele tem dois modos de exibição:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Compacto: mostra dois cartões (por padrão, Palavras e Caracteres) e, se você quiser, "
        "uma barrinha fina de progresso da meta do dia. Você pode alternar o que é exibido "
        "nele usando o clique direito (palavras, tempo de sessão, páginas e etc)."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Completo: abre o painel do contador completo — de onde contar, meta diária, sprint "
        "de escrita e o calendário. Tudo é acessível dentro dele."));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Clique no triângulo pra recolher/expandir o contador menor. Para exibir o painel "
        "completo, clique no corpo do contador pequeno. Pra reduzi-lo novamente, é o mesmo "
        "processo."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/daily-goal-counter/counter-panel.png' style='text-decoration:none;'>"
        "<img src=':/help/daily-goal-counter/counter-panel.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kCounterPanelThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "No painel completo, você encontrará as seguintes opções:"));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>0- %1</b><br>%2</p>")
        .arg(tr("O contador minúsculo."),
             tr("Ainda é exibido no modo completo. Para recuar do modo completo, basta clicar "
                "nele novamente."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("De onde contar."),
             tr("No modo completo, você escolhe o que os cartões contam: só os capítulos do "
                "manuscrito, só o documento que está aberto no editor agora, só as gavetas, ou "
                "tudo junto. Clique com o botão direito nos cartões pra escolher quais "
                "métricas aparecem ali (Palavras, Caracteres, Palavras hoje, Páginas, entre "
                "outras)."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("Meta diária."),
             tr("Você define uma meta — em palavras ou em tempo (minutos escrevendo) — e "
                "escolhe se ela conta só os capítulos, só as gavetas, ou os dois juntos. Uma "
                "barra de progresso mostra \"Hoje: X / Y palavras\" (ou minutos), e embaixo "
                "dela aparece quanto tempo falta até a meta reiniciar (à meia-noite). Um botão "
                "de reiniciar deixa você zerar o progresso do dia manualmente, se precisar."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Tem também um pequeno grid de estatísticas ali do lado — streak atual, recorde, se "
        "você já bateu a meta hoje, páginas, e sua média de palavras por dia."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "E existe um sistema de folgas: dependendo de quantos dias seguidos você bate a meta, "
        "o app libera dias de descanso que não quebram seu streak."));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>3- %1</b><br>%2</p>")
        .arg(tr("Sprint de escrita."),
             tr("Um sprint é uma corrida contra o tempo: você define uma duração (padrão 25 "
                "minutos) e uma meta de palavras (padrão 300), aperta \"Iniciar sprint\", e um "
                "cronômetro regressivo aparece junto com a contagem de palavras escritas desde "
                "que você começou. Ao final (ou quando você encerrar manualmente), o app avisa "
                "se você bateu a meta do sprint ou não."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>4- %1</b></p>")
        .arg(tr("Calendário."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "No fim do modo completo, um botão \"Exibir calendário\" abre um calendário mensal. "
        "Cada dia mostra até 5 estrelinhas — quanto mais vezes você bateu sua meta diária "
        "naquele dia, mais estrelas. Dias de folga aparecem com um ícone de lua. Clicando num "
        "dia, você vê os detalhes: quantas palavras escreveu e quanto tempo passou escrevendo. "
        "Clicando com o botão direito, você pode marcar ou desmarcar aquele dia como folga, "
        "usando o sistema de folgas (explicado mais abaixo)."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/daily-goal-counter/calendar.png' style='text-decoration:none;'>"
        "<img src=':/help/daily-goal-counter/calendar.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kCalendarThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;margin-top:12px;'><b>5- %1</b></p>")
        .arg(tr("Estatísticas."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Tudo que você faz no editor é contabilizado e salvo — e pode ser consultado."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/daily-goal-counter/statistics.png' style='text-decoration:none;'>"
        "<img src=':/help/daily-goal-counter/statistics.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kStatisticsThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "No modo compacto ou completo, o botão \"Estatísticas\" abre um resumo de tudo que "
        "você já escreveu no projeto: dias com meta batida, streak atual e recorde, total de "
        "palavras e tempo de escrita, seu recorde diário, sua média por dia ativo, qual dia da "
        "semana você mais escreve, e qual documento você mais trabalhou."));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Uma coisa importante: os cartões do topo mostram um total acumulado (de onde você "
        "escolheu contar), a barra da meta mostra seu progresso de hoje, e as Estatísticas "
        "mostram o histórico completo do projeto — são três números diferentes, cada um com "
        "seu propósito."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>6- %1</b></p>")
        .arg(tr("O sistema de folgas."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/daily-goal-counter/off-days.png' style='text-decoration:none;'>"
        "<img src=':/help/daily-goal-counter/off-days.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kOffDaysThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "O sistema de folgas é simples de entender e dinâmico."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Ele é desbloqueado após sua primeira semana completa (7 dias) batendo a meta diária "
        "— a partir daí você pode configurar o seu sistema de folgas. Ele funciona assim: "
        "cada vez que você bater sua meta diária ou manter sua streak pelo tempo configurado "
        "nele, você ganha direito a uma folga, que pode ser marcada no calendário. No dia de "
        "folga marcado, se nada for escrito, sua streak não é perdida — afinal foi uma folga."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Você pode marcar folgas no calendário clicando com o botão direito no dia que deseja "
        "folgar. Também é possível guardar folgas e depois tirá-las todas de uma vez quando "
        "quiser."));

    html += QStringLiteral("<p style='font-style:italic;color:%1;'>%2</p>")
        .arg(Theme::textMuted(), tr(
            "Nota do dev: por favor, ignore minhas metas e números nos prints acima. Como "
            "desenvolvedor, não tenho escrito muito, então meus números estão péssimos, mas "
            "vou voltar aos trilhos em breve, prometo."));

    return html;
}

// Conteúdo escrito pelo assistente em help-panel/reference-menu/, revisado
// pelo usuário, montado aqui em HTML.
QString HelpPanel::referenceMenuContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Menu de Referência é um painel flutuante que deixa você consultar (ou até editar) "
        "outro documento do projeto sem sair do que está escrevendo agora. Quer conferir a "
        "ficha de um personagem, reler uma cena anterior ou dar uma olhada num nó do "
        "Construtor enquanto escreve? É pra isso que ele existe — o editor principal continua "
        "exatamente do jeito que você deixou."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Pra abrir, clique no botão de Menu de Referência na barra do topo, ou use o atalho "
        "F6."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/reference-menu/refmenu-open.png' style='text-decoration:none;'>"
        "<img src=':/help/reference-menu/refmenu-open.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kRefMenuOpenThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Existem várias formas de abrir um documento dentro dele:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Ctrl+clique em qualquer @menção no seu texto abre automaticamente aquilo que foi "
        "mencionado (uma ficha, um capítulo, uma cena)."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Clique direito num capítulo, cena ou item de gaveta e escolha \"Abrir no Menu de "
        "Referência\"."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Dentro do próprio painel, use os seletores \"Manuscritos ▾\" e \"Gaveta ▾\" no topo "
        "pra navegar até o que você quer."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Use a busca (ícone de lupa, ou Ctrl+Alt+F) pra filtrar manuscritos, capítulos, cenas "
        "e gavetas de uma vez e pular direto pro resultado."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/reference-menu/refmenu-search.png' style='text-decoration:none;'>"
        "<img src=':/help/reference-menu/refmenu-search.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kRefMenuSearchThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Dá pra editar o documento aberto ali? Sim, com algumas restrições. O botão de lápis "
        "no topo do painel liga o modo de edição. Capítulos inteiros e itens de gaveta "
        "(fichas de personagem, cenários, etc.) podem ser editados direto dali. Cenas "
        "individuais, por enquanto, são só leitura. E se o mesmo documento já estiver aberto "
        "no editor principal, a edição fica bloqueada no Menu de Referência — pra evitar você "
        "editar a mesma coisa em dois lugares ao mesmo tempo e perder alguma coisa."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Uma vez ativado o modo edição, ela é feita diretamente na área de visualização do "
        "texto no Menu de Referência, conforme o print abaixo."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/reference-menu/edit-mode.png' style='text-decoration:none;'>"
        "<img src=':/help/reference-menu/edit-mode.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kRefMenuEditThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Outras coisas que você pode fazer no painel:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Arrastar pela alcinha no topo pra mover, ou puxar as bordas pra redimensionar. Duplo "
        "clique na alcinha reseta a posição."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Esconder a área de navegação e deixar só o preview visível, pra ganhar espaço de "
        "leitura."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Ajustar o tamanho da fonte do texto exibido (botão \"Aa\")."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Em gavetas de personagens/cenários/objetos, alternar entre visualização em grade "
        "(com foto) ou lista simples."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Fechar com o \"✕\" ou apertando F6 de novo."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Caso você use múltiplos monitores, o Menu de Referência pode ser arrastado para "
        "outro monitor."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O painel lembra onde você deixou (posição, tamanho, tamanho da fonte) na próxima vez "
        "que abrir o app."));

    return html;
}

// Conteúdo escrito pelo assistente (sem rascunho prévio do usuário — feature
// sem tela própria pra fotografar, então não há screenshots aqui).
QString HelpPanel::keyboardShortcutsContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Uma lista com todos os atalhos de teclado do Qenna Writer, separados por onde eles "
        "atuam. Os atalhos padrão de edição de texto (Ctrl+Z para desfazer, Ctrl+Y/Ctrl+Shift+Z "
        "para refazer, Ctrl+C/Ctrl+X/Ctrl+V para copiar/recortar/colar, Ctrl+A para selecionar "
        "tudo) também funcionam normalmente e não estão listados abaixo."));

    auto key = [](const QString& k) {
        return QStringLiteral(
            "<span style='background:%1;color:%2;border-radius:4px;padding:1px 7px;"
            "font-family:Consolas,monospace;font-size:12px;'>%3</span>")
            .arg(Theme::hoverOverlay(), Theme::textPrimary(), k);
    };
    auto row = [&key](const QString& k, const QString& desc) {
        return QStringLiteral(
            "<tr>"
            "<td style='padding:3px 14px 3px 0; white-space:nowrap; vertical-align:top;'>%1</td>"
            "<td style='padding:3px 0;'>%2</td>"
            "</tr>").arg(key(k), desc);
    };
    auto group = [](const QString& title) {
        return QStringLiteral("<p style='margin:14px 0 4px 0;'><b>%1</b></p>").arg(title);
    };

    html += group(tr("Painéis e navegação"));
    html += QStringLiteral("<table cellspacing='0' cellpadding='0'>");
    html += row(QStringLiteral("F2"), tr("Abrir a Lousa."));
    html += row(QStringLiteral("F3"), tr("Abrir a Timeline."));
    html += row(QStringLiteral("F4"), tr("Abrir/fechar o Pensário."));
    html += row(QStringLiteral("Shift+F4"), tr("Abrir o Mapa do Pensário."));
    html += row(QStringLiteral("F5"), tr("Abrir os Grupos."));
    html += row(QStringLiteral("F6"), tr("Abrir/fechar o Menu de Referência."));
    html += row(QStringLiteral("F7"), tr("Abrir/fechar os Lembretes."));
    html += row(QStringLiteral("F11"), tr("Ativar/sair da tela cheia."));
    html += row(QStringLiteral("F12"), tr("Voltar ao Menu Principal."));
    html += row(QStringLiteral("Ctrl+F10"), tr("Ativar/desativar o Modo Foco."));
    html += QStringLiteral("</table>");

    html += group(tr("Manuscrito e gavetas"));
    html += QStringLiteral("<table cellspacing='0' cellpadding='0'>");
    html += row(QStringLiteral("Ctrl+S"), tr("Salvar o projeto."));
    html += row(QStringLiteral("Ctrl+Shift+N"), tr("Criar um novo capítulo no manuscrito ativo."));
    html += row(QStringLiteral("Ctrl+N"), tr("Criar um novo item na gaveta aberta no momento."));
    html += row(QStringLiteral("Shift+Page&nbsp;Down"), tr(
        "Ir para o próximo capítulo (ou próximo item da gaveta aberta)."));
    html += row(QStringLiteral("Shift+Page&nbsp;Up"), tr(
        "Ir para o capítulo anterior (ou item anterior da gaveta aberta)."));
    html += QStringLiteral("</table>");

    html += group(tr("Busca"));
    html += QStringLiteral("<table cellspacing='0' cellpadding='0'>");
    html += row(QStringLiteral("Ctrl+F"), tr(
        "Buscar dentro do documento aberto no editor (ou dentro da ficha, se ela estiver "
        "aberta na tela)."));
    html += row(QStringLiteral("Ctrl+Shift+F"), tr("Busca global — em todo o projeto."));
    html += row(QStringLiteral("Ctrl+Alt+F"), tr("Buscar dentro do Menu de Referência."));
    html += row(QStringLiteral("Alt+F"), tr(
        "Buscar dentro do preview do documento já aberto no Menu de Referência."));
    html += QStringLiteral("</table>");

    html += group(tr("Formatação de texto"));
    html += QStringLiteral("<table cellspacing='0' cellpadding='0'>");
    html += row(QStringLiteral("Ctrl+B"), tr("Negrito."));
    html += row(QStringLiteral("Ctrl+I"), tr("Itálico."));
    html += row(QStringLiteral("Ctrl+U"), tr("Sublinhado."));
    html += row(QStringLiteral("Ctrl+Shift+S"), tr("Tachado."));
    html += row(QStringLiteral("Ctrl+-"), tr("Inserir um travessão (—) na posição do cursor."));
    html += QStringLiteral("</table>");

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/markers/, montado aqui em HTML.
QString HelpPanel::markersContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Marcadores são grifos coloridos que você aplica sobre um trecho do texto — com ou "
        "sem um comentário junto. Servem pra sinalizar o que precisa de atenção (\"revisar "
        "esse diálogo\", \"checar esse fato depois\") sem interromper a escrita: o grifo fica "
        "ali, visível, esperando você voltar."));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Pra criar um, selecione o trecho — o menu flutuante de seleção tem duas opções:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'><b>%1</b><br>%2</p>")
        .arg(tr("Marcador"),
             tr("Só pinta o trecho com a cor escolhida, sem comentário."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>%1</b><br>%2</p>")
        .arg(tr("Marcador com comentário"),
             tr("Pinta o trecho e abre um campo de texto pra você escrever uma nota sobre "
                "ele."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/markers/select-menu.png' style='text-decoration:none;'>"
        "<img src=':/help/markers/select-menu.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kMarkerSelectMenuThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Nos dois casos, um popup de cores aparece acima da seleção: oito cores prontas, mais "
        "um botão pra escolher qualquer cor customizada. No modo \"com comentário\" o popup "
        "também mostra a caixinha de texto pro comentário."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/markers/color-picker.png' style='text-decoration:none;'>"
        "<img src=':/help/markers/color-picker.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "&nbsp;&nbsp;"
        "<a href='zoom:/help/markers/comment-picker.png' style='text-decoration:none;'>"
        "<img src=':/help/markers/comment-picker.png' width='%4'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kMarkerColorPickerThumbWidth), Theme::textMuted(),
          tr("Clique para expandir"), QString::number(kMarkerCommentPickerThumbWidth));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Passando o mouse sobre um trecho marcado que tem comentário, um balão aparece "
        "mostrando o texto da nota, com botões pra editar (cor e/ou comentário) ou excluir o "
        "marcador."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/markers/comment-hover.png' style='text-decoration:none;'>"
        "<img src=':/help/markers/comment-hover.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kMarkerCommentHoverThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Marcadores sem comentário não têm esse balão — pra mudar a cor de um deles (ou pra "
        "removê-lo de vez), é só selecionar o trecho de novo: o popup de cores reabre, com um "
        "botão de lixeira ao lado de confirmar/cancelar."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/markers/plain-marker.png' style='text-decoration:none;'>"
        "<img src=':/help/markers/plain-marker.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kMarkerPlainThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Todos os seus marcadores comentados ficam reunidos numa lista só, dentro do "
        "Pensário, na aba \"Comentários\" — dá pra ordenar por data de criação ou agrupados "
        "na ordem do próprio manuscrito (por capítulo/cena), e clicar num deles leva direto "
        "pro trecho de origem. Mas o Pensário é um assunto diferente, falaremos dele quando "
        "chegar a hora."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Na hora de exportar, você escolhe se os marcadores aparecem no arquivo final ou se "
        "são removidos — essa opção fica na própria janela de Exportação (já falamos dela lá "
        "atrás)."));

    return html;
}

// Conteúdo escrito pelo assistente (sem rascunho prévio do usuário — feature
// sem tela própria pra fotografar, então não há screenshots aqui).
QString HelpPanel::memoriesContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Memórias são trechos de texto que você guarda de lado pra consultar depois — uma "
        "fala marcante, uma descrição que você quer reaproveitar, um detalhe de lore que "
        "surgiu no meio de uma cena e você não quer perder. Diferente do Marcador (que fica "
        "grudado no texto original), a Memória vira um cartão avulso, guardado à parte."));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Pra criar uma, selecione o trecho no editor e escolha \"Adicionar à memória...\" no "
        "menu flutuante de seleção. Um popup abre com:"));

    html += QStringLiteral("<p style='margin-bottom:12px;'><b>1- %1</b><br>%2</p>")
        .arg(tr("Destino."),
             tr("Guardar como memória do Projeto (geral) ou de um Personagem específico — "
                "nesse caso, você escolhe qual, numa lista dos personagens já cadastrados no "
                "projeto."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>2- %1</b><br>%2</p>")
        .arg(tr("Nome (opcional)."),
             tr("Dê um título pra memória, se quiser. Se deixar em branco, ela aparece "
                "identificada pela fonte de onde veio (ex: \"Memória do Capítulo 3\")."));
    html += QStringLiteral("<p style='margin-bottom:12px;'><b>3- %1</b><br>%2</p>")
        .arg(tr("Tags."),
             tr("Marque uma ou mais etiquetas livres pra organizar suas memórias — o popup "
                "sugere as tags que você já usou antes no projeto, pra manter consistência."));

    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "A memória guarda também, automaticamente, de onde ela veio (o capítulo, a cena ou o "
        "documento de gaveta), então você sempre sabe o contexto original."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Pra consultar, abra o Pensário e vá na aba \"Memórias\": lá você filtra entre "
        "memórias do Projeto ou de cada Personagem, e também por tag. Clicar num cartão leva "
        "você até o trecho de origem no editor; o \"×\" no canto do cartão exclui a memória."));

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/timeline/, montado aqui em HTML.
QString HelpPanel::timelineContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "A Timeline é a função responsável por organizar a linha do tempo da história. Ela gera "
        "ramificações, conexões, backstories e muito mais conforme você escreve o seu projeto."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Porém, apesar do conceito brilhante, sou sincero: a Timeline pode ser confusa de se "
        "entender caso você não saiba o que está fazendo. Então, para que tudo flua de maneira "
        "boa e você não fique perdido, irei te ensinar a navegar nas águas traiçoeiras do tempo."));
    html += QStringLiteral("<p>%1</p>").arg(tr("Vem comigo."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Pra abrir, clique no botão de Timeline na barra lateral (ou use o atalho F3)."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/timeline/timeline-panel.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/timeline-panel.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kTimelinePanelThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p>%1</p>").arg(tr("Vamos começar."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>1- %1</b></p>")
        .arg(tr("A data-base da sua história."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Quando você cria um manuscrito (ou edita um já existente), tem um campo opcional "
        "\"Quando a história se passa\". Esse é o marco zero da sua Timeline — a partir dele, o "
        "app sabe distinguir o que é \"a história principal\" do que é \"flashback\"."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/timeline/manuscript-data.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/manuscript-data.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kTimelineManuscriptThumbWidth), Theme::textMuted(), tr("Clique para expandir"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "O marco zero da história é importante. Tenha uma data boa em mente. Usando um marcador "
        "não convencional — como uma medição temporal fictícia, textos com números (ex: verão, "
        "1988) e etc. — ela ainda deverá funcionar caso o marcador usado tenha lógica e o "
        "sistema consiga compreender o que se passa antes ou depois do marco zero, mas fica mais "
        "propenso a erros."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Como dev, eu realmente recomendo que seja utilizado um formato de data convencional. "
        "Como Dia/Mês/Ano, Mês/Dia/Ano, Dia/Mês, Mês/Ano, e etc. O sistema funciona de forma "
        "MUITO mais precisa desse modo."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>2- %1</b></p>")
        .arg(tr("Marcador e resumo do capítulo (e da cena)."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Ao criar ou editar um capítulo, você já viu os campos \"Quando se passa\" e \"Resumo\" "
        "— pois é, eles não são só enfeite. Preenchendo os dois, o app cria automaticamente um "
        "evento na Timeline pra aquele capítulo, usando o resumo como descrição do evento. Se um "
        "capítulo tiver cenas separadas (lembra do \"----\"?), cada cena pode ter seu próprio "
        "marcador e resumo — se você não preencher o da cena, ela simplesmente herda o do "
        "capítulo."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/timeline/chapter-scene-data.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/chapter-scene-data.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kTimelineChapterSceneThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>3- %1</b></p>")
        .arg(tr("História × Flashback: a organização automática."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Aqui está o pulo do gato. Comparando o marcador de cada capítulo/cena com a data-base "
        "do manuscrito, o app decide sozinho onde aquele evento entra:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Se a data é igual ou depois da data-base → vai pra trilha \"Narrativa\" (a história "
        "andando pra frente)."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Se a data é antes da data-base → vai automaticamente pra trilha \"Flashback\" (mostrada "
        "com um trilho tracejado)."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Você não precisa arrastar nada nem escolher trilha manualmente — só escrever a data "
        "certa no capítulo/cena."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "E agora, vamos explorar o painel da Timeline de fato."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>4- %1</b></p>")
        .arg(tr("Alternando entre os eixos."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Um botão na barra superior do painel alterna entre o eixo \"Narrativa\" e o eixo "
        "\"História\" (aparece como \"Backstory\" na tradução em inglês) — cada evento só "
        "aparece num dos dois, nunca nos dois ao mesmo tempo, então essa alternância é como você "
        "navega entre \"o que acontece na história\" e \"o que aconteceu antes\"."));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>5- %1</b></p>")
        .arg(tr("Quem está na cena."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Se você já usa a detecção de personagens, os eventos da Timeline mostram "
        "automaticamente \"Presentes: Fulano, Beltrana\" — quem aparece naquele capítulo/cena. E "
        "tem um filtro \"Personagem: Todos ▾\" na barra do topo que deixa só os eventos daquele "
        "personagem em destaque, esmaecendo o resto."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/timeline/characters-narrative.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/characters-narrative.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kTimelineCharactersThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p style='margin-bottom:4px;'><b>6- %1</b></p>")
        .arg(tr("Criando e editando eventos e linhas do tempo manualmente."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Nem tudo precisa vir do capítulo. Clicando no \"+\" flutuante no canto do painel (ou "
        "selecionando um trecho de texto no editor e usando o menu de seleção), você cria um "
        "evento manual."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Você também pode criar linhas do tempo próprias, definir a importância delas e "
        "nomeá-las."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Linhas do tempo próprias não são alimentadas de forma automática como as linhas de "
        "narrativa e backstory, mas são boas para criação de lore e worldbuilding."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/timeline/event-creator.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/event-creator.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "&nbsp;&nbsp;"
        "<a href='zoom:/help/timeline/timeline-creator.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/timeline-creator.png' width='%4'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kTimelineEventCreatorThumbWidth), Theme::textMuted(),
          tr("Clique para expandir"), QString::number(kTimelineCreatorThumbWidth));

    html += QStringLiteral("<p style='margin-bottom:4px;margin-top:12px;'><b>7- %1</b></p>")
        .arg(tr("Formas de visualizar."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Um botão alterna entre três modos: Trilho (linhas horizontais organizadas por faixa), "
        "Ramificações (constelação livre) e Espiral. É só estética/organização — os mesmos "
        "eventos aparecem nos três, só a disposição muda."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/timeline/mode-track.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/mode-track.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
        "<p align='center'>"
        "<a href='zoom:/help/timeline/mode-branches.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/mode-branches.png' width='%4'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
        "<p align='center'>"
        "<a href='zoom:/help/timeline/mode-spiral.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/mode-spiral.png' width='%5'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kTimelineModeTrackThumbWidth), Theme::textMuted(), tr("Clique para expandir"),
          QString::number(kTimelineModeBranchesThumbWidth), QString::number(kTimelineModeSpiralThumbWidth));

    html += QStringLiteral("<p style='margin-bottom:4px;margin-top:12px;'><b>8- %1</b></p>")
        .arg(tr("Foco."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Clicando em um evento (ou usando o menu de Foco), o resto do painel esmaece, "
        "destacando só aquela linha e o que está conectado a ela — útil quando o projeto cresce "
        "e a tela fica cheia de eventos."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "E sim, quanto mais a sua timeline cresce, especialmente se você usar linhas e eventos "
        "manuais que se conectam, isso fará toda a diferença, observe:"));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/timeline/line-focus.png' style='text-decoration:none;'>"
        "<img src=':/help/timeline/line-focus.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kTimelineLineFocusThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Por último: se você preferir o jeito antigo de acompanhar personagem por personagem "
        "(uma trilha dedicada por personagem, em vez do filtro), tem um botão \"Personagens "
        "(legado)\" que liga esse sistema de volta. Ele vem desligado por padrão porque o filtro "
        "+ \"Presentes:\" cobre o mesmo uso de um jeito mais simples, mas a opção continua lá se "
        "você preferir."));

    html += QStringLiteral("<p style='font-style:italic;color:%1;'><b>%2</b> %3</p>")
        .arg(Theme::textMuted(), tr("Nota do dev:"), tr(
            "A Timeline talvez seja a função que pareça mais complexa dentro do app. É UMA MINA "
            "DE OURO, mas tem uma leve curva de aprendizado para alcançar seu potencial máximo. "
            "Eu pretendo tentar simplificá-la um pouco em versões futuras, mas acredite, vale a "
            "pena tentar entendê-la. Especialmente se sua obra exige um bom planejamento "
            "cronológico e consistente."));

    return html;
}

// Conteúdo escrito pelo usuário em help-panel/cover-creator/, montado aqui em HTML.
QString HelpPanel::coverCreatorContent() const
{
    QString html;
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Qenna Writer vem com uma ferramenta separada pra criar a capa do seu livro — o Mira "
        "Cover. Ela abre numa janela própria, fora do editor."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Pra acessar, vá até a tela inicial (o Menu Principal, com a lista dos seus projetos) e "
        "clique com o botão direito no card do livro que você quer dar uma capa. A opção \"Criar "
        "capa\" aparece no menu de contexto."));
    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/cover-creator/context-menu.png' style='text-decoration:none;'>"
        "<img src=':/help/cover-creator/context-menu.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kCoverContextMenuThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Se for a primeira vez que você usa essa função, o app vai perguntar se quer instalar o "
        "Mira Cover — é rapidinho, e só precisa fazer isso uma vez."));

    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Dentro do Mira Cover, você monta a capa do zero:"));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Título e autor como textos editáveis, com controle de fonte (mais de 100 opções, "
        "agrupadas por estilo — literário, fantasia, terror, ficção científica, etc.), tamanho, "
        "cor, sombra, contorno, brilho e rotação."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Imagem de fundo: você pode subir uma imagem do seu computador ou escolher de uma "
        "galeria com dezenas de fotos já disponíveis no próprio app, com ajuste de zoom, foco e "
        "filtro."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Cor de fundo sólida, se preferir não usar imagem."));
    html += QStringLiteral("<p style='margin-bottom:4px;'>%1</p>").arg(tr(
        "Camadas extras: mais textos, símbolos, e formas geométricas (círculo, linha, triângulo, "
        "retângulo)."));
    html += QStringLiteral("<p style='margin-bottom:12px;'>%1</p>").arg(tr(
        "Borda ao redor da capa, com cor e espessura ajustáveis."));

    html += QStringLiteral(
        "<p align='center'>"
        "<a href='zoom:/help/cover-creator/app-editing.png' style='text-decoration:none;'>"
        "<img src=':/help/cover-creator/app-editing.png' width='%1'>"
        "<br><span style='font-size:11px;color:%2;'>%3</span>"
        "</a>"
        "</p>"
    ).arg(QString::number(kCoverAppEditingThumbWidth), Theme::textMuted(), tr("Clique para expandir"));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Se você já tinha editado uma capa desse projeto antes, o Mira Cover lembra o que você "
        "fez da última vez e abre de onde parou."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Pra salvar, clique em \"OK\" (ou o botão equivalente de confirmar) — isso grava a capa "
        "na pasta do projeto e fecha o Mira Cover; ao voltar pro Qenna Writer, a capa já aparece "
        "atualizada no card do projeto. Também tem um botão de exportar (ícone de download) que "
        "baixa a imagem da capa avulsa, sem mexer no projeto — útil se você quiser usar essa "
        "capa em outro lugar."));

    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Cover Creator conta com uma galeria de imagens sem copyright que podem ser usadas "
        "para capas do seu projeto em uso profissional."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "Ele tem várias opções: ajustes de imagem, adicionar textos, símbolos, efeitos e muito "
        "mais."));
    html += QStringLiteral("<p>%1</p>").arg(tr(
        "O Cover Creator não é muito difícil de se usar, caso já tenha costume com editores "
        "básicos de imagem, você vai se sentir em casa. Se não tiver, explore. Não vai se "
        "arrepender."));

    return html;
}

void HelpPanel::updateContent()
{
    QString label;
    for (const Topic& t : m_topics) {
        if (t.id == m_selectedId) { label = t.label; break; }
    }
    if (m_contentTitle) m_contentTitle->setText(label);
    if (m_content) m_content->setHtml(contentFor(m_selectedId));
}

void HelpPanel::onAnchorClicked(const QUrl& url)
{
    if (url.scheme() != QStringLiteral("zoom")) return;
    openImageZoom(QStringLiteral(":") + url.path());
}

void HelpPanel::openImageZoom(const QString& resourcePath)
{
    const QPixmap pix(resourcePath);
    if (pix.isNull()) return;

    auto* dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Ajuda"));

    auto* layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* label = new QLabel(dlg);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    const QScreen* screen = QGuiApplication::screenAt(geometry().center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    QSize maxSize = screen ? screen->availableGeometry().size() * 0.85 : QSize(1200, 800);
    QPixmap shown = pix;
    if (shown.width() > maxSize.width() || shown.height() > maxSize.height()) {
        shown = shown.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    label->setPixmap(shown);
    dlg->setFixedSize(shown.size());
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void HelpPanel::openNear(const QRect& anchorGlobal)
{
    if (!m_positioned) {
        m_positioned = true;
        QPoint pos(anchorGlobal.right() - width(), anchorGlobal.bottom() + kGapBelowAnchor);
        const QScreen* screen = QGuiApplication::screenAt(anchorGlobal.center());
        if (screen) {
            const QRect avail = screen->availableGeometry();
            if (pos.x() + width() > avail.right()) pos.setX(avail.right() - width() - 4);
            if (pos.x() < avail.left()) pos.setX(avail.left() + 4);
            if (pos.y() + height() > avail.bottom()) {
                pos.setY(qMax(avail.top(), anchorGlobal.top() - height() - kGapBelowAnchor));
            }
        }
        move(pos);
    }
    show();
    raise();
    activateWindow();
}
