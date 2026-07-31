#include "CharacterImageGenService.h"

#include "AIClient.h"
#include "AvatarUtils.h"
#include "ImageGenClient.h"
#include "ProjectModel.h"

#include <QSettings>
#include <QTextDocument>

namespace {

constexpr int kReferenceMaxSide = 768; // suficiente pro modelo "ver" detalhes; mantém custo baixo

QString stripHtmlToPlainText(const QString& html)
{
    QTextDocument doc;
    doc.setHtml(html);
    return doc.toPlainText().trimmed();
}

// Encode simples (sem crop) pra anexar como referência visual no passo 1 —
// diferente do pipeline de ImageCropDialog (que sempre recorta quadrado
// 400x400 pra salvar como foto final), aqui queremos a imagem INTEIRA, só
// reduzida de tamanho pra não gastar tokens de visão à toa.
QString encodeReferenceImage(const QImage& img)
{
    return AvatarUtils::encodeDataUrl(img, kReferenceMaxSide, 85);
}

// Fragmento de direção de arte injetado no prompt do passo 1 — só afeta
// iluminação/textura/técnica, nunca a cena/pose pedida pelo usuário.
QString stylePromptFragment(ImageStylePreset preset)
{
    switch (preset) {
    case ImageStylePreset::Photorealistic:
        return QStringLiteral(
            "Estilo: fotografia realista — iluminação de câmera real, textura "
            "de pele e tecido convincente, profundidade de campo natural, "
            "nada de aparência de renderização 3D ou pintura digital.");
    case ImageStylePreset::DigitalRealism:
        return QStringLiteral(
            "Estilo: pintura digital hiper-detalhada (digital painting), "
            "realista mas perceptivelmente arte digital, não uma foto.");
    case ImageStylePreset::DigitalIllustration:
        return QStringLiteral(
            "Estilo: ilustração/concept art digital estilizada, traços e "
            "sombreamento de arte de personagem, não fotorrealista.");
    case ImageStylePreset::Anime:
        return QStringLiteral(
            "Estilo: anime/mangá japonês — traços limpos, olhos "
            "estilizados, cores vibrantes e sombreamento chapado típico.");
    case ImageStylePreset::Cartoon:
        return QStringLiteral(
            "Estilo: cartoon ocidental — traço simplificado, cores chapadas, "
            "proporções levemente exageradas.");
    case ImageStylePreset::Default:
    default:
        return QString();
    }
}

QString promptEngineeringSystemPrompt()
{
    return QStringLiteral(
        "Você é o Diretor de Arte e Engenheiro de Prompts Visuais desta "
        "aplicação — responsável por transformar informações narrativas de "
        "um projeto literário em UM prompt de imagem em texto corrido, "
        "pronto pra ser enviado direto a um modelo de geração de imagem "
        "(GPT Image). Seu objetivo não é repetir a descrição recebida: é "
        "interpretar as informações, selecionar os detalhes visualmente "
        "relevantes, e montar uma direção de arte completa que represente "
        "fielmente o personagem/cena pedida, respeitando a intenção do "
        "autor. O prompt precisa ser DENSO e ESPECÍFICO — modelos de "
        "imagem preenchem com o próprio viés genérico (pose de ensaio "
        "fotográfico, iluminação de estúdio, expressão de sedução) "
        "qualquer detalhe que você deixar vago.\n\n"

        "## Fontes de informação\n"
        "Você pode receber: (1) ficha do personagem — aparência física, "
        "idade, etnia/tom de pele, cabelo, olhos, características "
        "marcantes, roupas, acessórios, tatuagens/cicatrizes, contexto "
        "narrativo; (2) uma imagem de referência anexada pelo autor; (3) o "
        "pedido atual do autor — ação, pose, cenário, emoção, "
        "enquadramento, estilo.\n\n"

        "## Hierarquia — o que pode e o que não pode mudar\n"
        "Ao combinar essas fontes, siga esta ordem de prioridade:\n"
        "1. Características físicas PERMANENTES já estabelecidas (ficha e/"
        "ou imagem de referência) — idade aparente, cor/textura de cabelo, "
        "tom de pele, olhos, tatuagens/cicatrizes/marcas, acessórios "
        "permanentes. NUNCA altere isso pra deixar a imagem \"mais bonita\" "
        "ou conveniente, e nunca por causa do pedido atual — identidade "
        "visual não é negociável.\n"
        "2. O que o pedido atual pede explicitamente — ação, pose, roupa "
        "temporária, cenário, expressão, enquadramento, estilo. Isso é o "
        "que MUDA de imagem pra imagem; siga o pedido à risca.\n"
        "3. Fatos confirmados do projeto (contexto narrativo relevante que "
        "apareça na ficha — profissão, época, papel na história).\n"
        "4. Inferências visuais razoáveis pra preencher lacunas (tipo de "
        "iluminação, textura de ambiente, pequenos elementos de cenário).\n"
        "5. Detalhes criativos secundários que você mesma adicionar — "
        "sempre a menor prioridade, nunca competindo com os itens acima.\n"
        "Se uma informação estiver ausente, não invente um detalhe "
        "específico desnecessário — não invente cor de olhos, tatuagem, "
        "altura, relação entre personagens, evento de enredo ou símbolo "
        "narrativo que não esteja registrado nem pedido; use termos "
        "neutros ou deixe o modelo de imagem completar o secundário "
        "sozinho. IMPORTANTE: \"não invente identidade\" não significa "
        "\"seja vago\" — quando a ficha do personagem for pouco "
        "aprofundada, compense sendo RICA e ESPECÍFICA em tudo que NÃO é "
        "identidade permanente (pose, expressão, ambiente, roupa, "
        "iluminação, enquadramento, textura, atmosfera). Um prompt vago "
        "produz uma imagem genérica mesmo quando a identidade está "
        "correta — a regra de não inventar vale só pros traços físicos "
        "permanentes, nunca como desculpa pra descrever a cena com menos "
        "detalhe.\n\n"

        "## Imagem de referência (se houver)\n"
        "Examine com cuidado e descreva os traços CONCRETOS e ESPECÍFICOS "
        "que você vê — não use termos vagos como \"cabelo claro\" ou "
        "\"pele bronzeada\": diga o tom exato (ex. \"loiro dourado\", "
        "\"castanho-avermelhado\", \"pele morena clara\"), textura e "
        "comprimento do cabelo, formato do rosto, cor dos olhos se "
        "visível, e qualquer marca ou traço distintivo. Esses detalhes "
        "concretos são o que garante que a imagem gerada pareça a MESMA "
        "pessoa da referência, em vez de uma reinterpretação genérica. "
        "Trate a referência como fonte de características permanentes "
        "(prioridade 1 acima) — se a ficha ou o pedido atual contradisser "
        "algo nela, eles têm prioridade.\n\n"

        "## Como montar o prompt\n"
        "Organize nesta sequência (pule o que não se aplicar): tipo de "
        "imagem/estilo visual → sujeito principal → características "
        "físicas essenciais → roupa/acessórios/identidade visual → pose, "
        "ação e linguagem corporal → expressão e emoção → cenário e "
        "elementos ambientais → iluminação → enquadramento e composição → "
        "atmosfera e direção estética → acabamento visual.\n"
        "Prefira sempre o que é visualmente representável. Converta "
        "personalidade e emoção em corpo/rosto/roupa/ambiente — nunca "
        "deixe um traço abstrato solto no prompt. Exemplos:\n"
        "\"Ela é independente e rebelde\" → \"Postura confiante, ombros "
        "relaxados, olhar direto, expressão desafiadora.\"\n"
        "\"Ele está triste\" → \"Olhar baixo, expressão cansada, ombros "
        "levemente curvados, postura silenciosa.\"\n"
        "Descreva cabelo/marcas com verbos de comportamento, não só "
        "adjetivo estático — não só \"cabelo cacheado\", mas \"cabelo "
        "preto extremamente volumoso, caindo ao redor do rosto e dos "
        "ombros\"; tatuagens/piercings ganham uma frase própria logo após "
        "a descrição física central, dizendo ONDE ficam (\"diversas "
        "tatuagens visíveis nos braços e nas pernas\", não só \"tem "
        "tatuagens\"). Termine o prompt com um parágrafo técnico curto de "
        "acabamento — atmosfera geral, gênero fotográfico, orientação e "
        "enquadramento explícitos (\"composição vertical de corpo "
        "inteiro\"), qualificadores concretos de realismo (\"textura "
        "natural da pele\", \"profundidade de campo suave\"). Isso é "
        "diferente de empilhar adjetivo vazio (ver seção seguinte) — são "
        "termos técnicos de fotografia, cada um fazendo trabalho real.\n\n"

        "## Detalhamento equilibrado\n"
        "Seja específico o bastante pra preservar identidade e intenção, "
        "mas não sobrecarregue o prompt com listas de adjetivos vazios "
        "(\"extremamente lindo, perfeito, incrível, épico, deslumbrante, "
        "cinematográfico\"). Prefira detalhes concretos (\"luz lateral "
        "suave destacando a textura da jaqueta de couro\") a superlativos "
        "genéricos. Nunca repita a mesma característica em partes "
        "diferentes do prompt só com palavras diferentes.\n\n"

        "## Consistência do personagem\n"
        "Ao representar um personagem já existente: priorize a ficha/"
        "referência, preserve traços visuais importantes como âncoras de "
        "identidade, nunca o transforme numa pessoa genérica. Só mencione "
        "tatuagem/cicatriz/acessório se ele estiver de fato visível no "
        "enquadramento escolhido — não descreva tatuagem na perna se a "
        "imagem é um retrato fechado do rosto.\n\n"

        "## Direção de cena\n"
        "O pedido do autor pode ser curto e informal (\"Quero ela nos "
        "bastidores de um show, cansada depois da apresentação\") — "
        "transforme isso numa cena visual completa, acrescentando só "
        "detalhes coerentes com o pedido (ambiente de bastidores, "
        "equipamento musical, iluminação de palco distante, postura "
        "cansada). Nunca invente acontecimento importante, personagem novo "
        "ou elemento que altere a história.\n"
        "IMPORTANTE — a menos que o autor peça explicitamente uma cena "
        "sensual, NÃO adicione pose de ensaio fotográfico, ângulo de "
        "câmera provocante, roupa mais reveladora que a descrita, ou "
        "expressão de sedução. Uma cena doméstica/casual (ex. \"lendo no "
        "sofá\") tem que ficar doméstica e casual — descreva câmera e "
        "postura de forma explícita o bastante pra não sobrar espaço de "
        "interpretação nessa direção (ex. \"plano médio, câmera na altura "
        "dos olhos, postura relaxada e natural\"). Isso vale mesmo quando "
        "o estilo pedido for realista/fotográfico — realismo não é "
        "sinônimo de sensualização.\n\n"

        "## Enquadramento\n"
        "Escolha (ou siga o que o autor pedir): retrato fechado (rosto/"
        "expressão), meio corpo (expressão/postura/roupa superior), corpo "
        "inteiro (roupa/acessórios/silhueta/pose), plano aberto "
        "(personagem no cenário), ou plano cinematográfico (ação+ambiente+"
        "narrativa). Nunca peça enquadramentos contraditórios na mesma "
        "imagem (ex. \"retrato fechado\" + \"corpo inteiro visível\", ou "
        "\"fundo desfocado\" + \"todos os detalhes do cenário nítidos\").\n\n"

        "## Iluminação\n"
        "Use a luz pra reforçar o clima, sem exagerar por padrão: luz "
        "suave e quente → intimidade/conforto; luz lateral dramática → "
        "tensão/mistério; luz fria e difusa → solidão/melancolia; luz "
        "natural → realismo/espontaneidade; neon → urbano/noturno; "
        "contraluz → silhueta/impacto. Escolha a que combina com a cena "
        "pedida — não adicione iluminação dramática por padrão. Sempre que "
        "possível, amarre a luz a uma superfície/textura concreta que ela "
        "revela (\"luz lateral suave destacando o volume do cabelo, a "
        "textura da jaqueta e os detalhes das tatuagens\") em vez de só "
        "descrever o clima no vácuo — isso dá ao modelo de imagem algo "
        "concreto pra renderizar, não só uma sensação abstrata.\n\n"

        "## Estilo visual\n"
        "Se um estilo foi indicado junto do pedido, ele é só direção de "
        "arte (iluminação/textura/técnica) — nunca sobrepõe a cena pedida. "
        "Se NENHUM estilo foi indicado, escolha o mais coerente com o "
        "conteúdo: personagem → fotografia editorial/cinematográfica "
        "realista; cenário → concept art detalhada; fantasia → arte "
        "conceitual cinematográfica; objeto → fotografia de produto. Nunca "
        "copie o estilo exato de um artista vivo ou celebridade específica "
        "— descreva a estética em termos gerais (\"pinceladas "
        "expressivas, contraste intenso, atmosfera surreal\") em vez de "
        "nomear alguém. Arquétipo/década/gênero (\"presença magnética e "
        "atitude irreverente de uma rockstar dos anos 1980\", \"estética "
        "de detetive noir\") é um atalho eficiente e permitido pra "
        "transmitir muita informação de estilo de uma vez — só não nomeie "
        "uma pessoa real específica.\n\n"

        "## Texto, logos e negativos\n"
        "Não peça letras, logos, marcas, assinaturas ou marcas-d'água a "
        "menos que explicitamente pedido — se o personagem usa camiseta "
        "de banda, descreva \"camiseta preta vintage inspirada em "
        "merchandising de bandas de rock\" em vez de exigir um logo "
        "específico. Não empilhe uma lista de restrições negativas por "
        "padrão (\"sem mãos ruins, sem dedos extras\") — dê instruções "
        "positivas e claras; só use restrição negativa quando for "
        "relevante ao pedido ou pedida explicitamente.\n\n"

        "## Regras finais (não negociáveis)\n"
        "Mantenha o idioma da descrição do usuário — não traduza pra "
        "inglês. Nunca devolva um prompt vazio ou uma recusa: se o pedido "
        "esbarrar em algo sensível, produza a versão mais fiel possível "
        "dentro de limites seguros (ex. trocar nudez por traje apropriado) "
        "em vez de simplesmente falhar.\n\n"

        "## Saída\n"
        "Responda APENAS com o prompt final em texto corrido — sem aspas, "
        "sem título, sem a palavra \"Prompt:\", sem explicação, sem "
        "múltiplas opções, sem markdown, sem comentários ou "
        "justificativas.");
}

} // namespace

QString imageStylePresetLabel(ImageStylePreset preset)
{
    switch (preset) {
    case ImageStylePreset::Photorealistic:      return QObject::tr("Fotorrealista");
    case ImageStylePreset::DigitalRealism:      return QObject::tr("Realismo digital");
    case ImageStylePreset::DigitalIllustration: return QObject::tr("Ilustração digital");
    case ImageStylePreset::Anime:               return QObject::tr("Anime");
    case ImageStylePreset::Cartoon:             return QObject::tr("Cartoon");
    case ImageStylePreset::Default:
    default:                                    return QObject::tr("Padrão");
    }
}

QString imageStylePresetKey(ImageStylePreset preset)
{
    switch (preset) {
    case ImageStylePreset::Photorealistic:      return QStringLiteral("fotorrealista");
    case ImageStylePreset::DigitalRealism:      return QStringLiteral("realismo_digital");
    case ImageStylePreset::DigitalIllustration: return QStringLiteral("ilustracao_digital");
    case ImageStylePreset::Anime:               return QStringLiteral("anime");
    case ImageStylePreset::Cartoon:             return QStringLiteral("cartoon");
    case ImageStylePreset::Default:
    default:                                    return QStringLiteral("padrao");
    }
}

ImageStylePreset imageStylePresetFromKey(const QString& key)
{
    if (key == QStringLiteral("fotorrealista")) return ImageStylePreset::Photorealistic;
    if (key == QStringLiteral("realismo_digital")) return ImageStylePreset::DigitalRealism;
    if (key == QStringLiteral("ilustracao_digital")) return ImageStylePreset::DigitalIllustration;
    if (key == QStringLiteral("anime")) return ImageStylePreset::Anime;
    if (key == QStringLiteral("cartoon")) return ImageStylePreset::Cartoon;
    return ImageStylePreset::Default;
}

CharacterImageGenService::CharacterImageGenService(QObject* parent)
    : QObject(parent)
{
}

QString CharacterImageGenService::sheetContextText(const CharacterSheet& sheet)
{
    if (sheet.isEmpty()) return QString();

    QStringList lines;
    for (const SheetField& f : sheet.fields) {
        if (f.value.trimmed().isEmpty()) continue;
        const QString value = f.kind == QStringLiteral("text")
            ? stripHtmlToPlainText(f.value) : f.value.trimmed();
        if (value.isEmpty()) continue;
        lines.append(QStringLiteral("%1: %2").arg(f.label, value));
    }
    return lines.join(QStringLiteral("\n"));
}

void CharacterImageGenService::generate(const QString& userDescription, ImageStylePreset style,
                                        const QString& characterContext,
                                        const QString& imageModel, const QString& imageQuality,
                                        const QString& imageSize, const QImage& referenceImage)
{
    m_imageModel = imageModel;
    m_imageQuality = imageQuality;
    m_imageSize = imageSize;

    AIChatMessage sys;
    sys.role = QStringLiteral("system");
    sys.content = promptEngineeringSystemPrompt();

    QString userContent = userDescription.trimmed();
    const QString styleFragment = stylePromptFragment(style);
    if (!styleFragment.isEmpty()) userContent += QStringLiteral("\n\n") + styleFragment;
    if (!characterContext.trimmed().isEmpty()) {
        userContent += QStringLiteral("\n\nFicha do personagem (referência de aparência):\n")
            + characterContext.trimmed();
    }
    if (!referenceImage.isNull()) {
        userContent += QStringLiteral(
            "\n\n(Uma imagem de referência foi anexada — descreva em "
            "palavras os traços concretos e específicos dela e incorpore "
            "no prompt, seguindo a seção \"Imagem de referência\" do "
            "sistema.)");
    }

    AIChatMessage user;
    user.role = QStringLiteral("user");
    user.content = userContent;
    user.imageDataUrl = encodeReferenceImage(referenceImage);

    // Zera a conversa — generate() é sempre um começo novo; refine() é que
    // continua a partir daqui.
    m_promptMessages = { sys, user };
    runPromptStep();
}

void CharacterImageGenService::refine(const QString& correctionText)
{
    if (m_promptMessages.isEmpty()) return; // precisa de um generate() bem-sucedido antes

    AIChatMessage user;
    user.role = QStringLiteral("user");
    user.content = QStringLiteral(
        "O autor pediu esta correção sobre a imagem gerada pelo prompt "
        "anterior — ajuste o prompt pra atender exatamente isso, mantendo "
        "tudo o mais que já estava certo: %1").arg(correctionText.trimmed());
    m_promptMessages.append(user);
    runPromptStep();
}

void CharacterImageGenService::generateFromRawPrompt(const QString& rawPrompt, const QString& imageModel,
                                                      const QString& imageQuality, const QString& imageSize)
{
    m_imageModel = imageModel;
    m_imageQuality = imageQuality;
    m_imageSize = imageSize;
    m_promptMessages.clear(); // sem conversa de engenharia — refine() não se aplica aqui
    runImageStep(rawPrompt.trimmed());
}

void CharacterImageGenService::runPromptStep()
{
    m_promptClient = new AIClient(this);
    QSettings settings;
    m_promptClient->setApiKey(settings.value(QStringLiteral("ai/apiKey")).toString());
    m_promptClient->setBaseUrl(settings.value(QStringLiteral("ai/baseUrl"),
        QStringLiteral("https://api.openai.com/v1")).toString());
    m_promptClient->setModel(settings.value(QStringLiteral("ai/model"),
        QStringLiteral("gpt-4o-mini")).toString());

    AIClient* client = m_promptClient;
    connect(client, &AIClient::finished, this, [this, client](const QString& imagePrompt) {
        const QString prompt = imagePrompt.trimmed();
        // Guarda a resposta como turno assistant na conversa — é o que
        // permite a PRÓXIMA correção enxergar "o prompt anterior era X".
        AIChatMessage assistantMsg;
        assistantMsg.role = QStringLiteral("assistant");
        assistantMsg.content = prompt;
        m_promptMessages.append(assistantMsg);

        emit promptEngineered(prompt);
        runImageStep(prompt);
        client->deleteLater();
    });
    connect(client, &AIClient::errorOccurred, this, [this, client](const QString& err) {
        emit errorOccurred(err);
        client->deleteLater();
    });

    m_promptClient->sendMessage(m_promptMessages);
}

void CharacterImageGenService::runImageStep(const QString& prompt)
{
    m_imageClient = new ImageGenClient(this);
    QSettings settings;
    m_imageClient->setApiKey(settings.value(QStringLiteral("ai/apiKey")).toString());

    ImageGenClient* client = m_imageClient;
    connect(client, &ImageGenClient::finished, this, [this, client](const QImage& img) {
        emit imageReady(img);
        client->deleteLater();
    });
    connect(client, &ImageGenClient::errorOccurred, this, [this, client](const QString& err) {
        emit errorOccurred(err);
        client->deleteLater();
    });
    m_imageClient->generate(prompt, m_imageModel, m_imageQuality, m_imageSize);
}
