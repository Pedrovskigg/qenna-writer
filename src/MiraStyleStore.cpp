#include "MiraStyleStore.h"

#include "AIClient.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

namespace MiraStyleStore {

namespace {

// Teto de exemplos acumulados (por lado) que dispara a consolidação. Sem
// exposição na UI de propósito — ajustável só via QSettings
// "ai/_debugStyleThreshold" pra testar a consolidação sem precisar dar 12
// likes de verdade.
constexpr int kDefaultConsolidationThreshold = 12;

int consolidationThreshold()
{
    QSettings settings;
    return settings.value(QStringLiteral("ai/_debugStyleThreshold"),
                           kDefaultConsolidationThreshold).toInt();
}

QString consolidationSystemPrompt(bool positive)
{
    return positive
        ? QStringLiteral(
            "Você é um analista de estilo de comunicação. Abaixo estão "
            "respostas de uma assistente de IA que o autor marcou como "
            "POSITIVAS (gostou de como ela respondeu). Se houver, também há "
            "um resumo consolidado anterior desses mesmos padrões. Escreva "
            "um ÚNICO resumo consolidado em prosa direta (não em bullets), "
            "descrevendo os PADRÕES de tom, tamanho, estrutura e abordagem "
            "que o autor recompensou com like. Funda o aprendizado do "
            "resumo anterior com o dos exemplos novos — não descarte o que "
            "já valia. Não cite trechos literais longos. Máximo ~200 "
            "palavras. Responda só com o resumo, sem preâmbulo.")
        : QStringLiteral(
            "Você é um analista de estilo de comunicação. Abaixo estão "
            "respostas de uma assistente de IA que o autor marcou como "
            "NEGATIVAS (não gostou de como ela respondeu). Se houver, "
            "também há um resumo consolidado anterior desses mesmos "
            "padrões. Escreva um ÚNICO resumo consolidado em prosa direta "
            "(não em bullets), descrevendo os PADRÕES de tom, tamanho, "
            "estrutura ou abordagem que o autor quer EVITAR. Funda o "
            "aprendizado do resumo anterior com o dos exemplos novos — não "
            "descarte o que já valia. Não cite trechos literais longos. "
            "Máximo ~200 palavras. Responda só com o resumo, sem preâmbulo.");
}

void triggerConsolidation(bool positive)
{
    Data data = load();
    const QJsonArray& examples = positive ? data.positiveExamples : data.negativeExamples;
    const QString oldSummary = positive ? data.positiveSummary : data.negativeSummary;

    QStringList exampleTexts;
    for (const QJsonValue& v : examples)
        exampleTexts << v.toObject().value(QStringLiteral("text")).toString();

    QString body;
    if (!oldSummary.isEmpty())
        body += QStringLiteral("Resumo consolidado anterior:\n%1\n\n").arg(oldSummary);
    body += QStringLiteral("Exemplos novos:\n\n");
    for (int i = 0; i < exampleTexts.size(); ++i)
        body += QStringLiteral("--- Exemplo %1 ---\n%2\n\n").arg(i + 1).arg(exampleTexts[i]);

    AIChatMessage sys;
    sys.role = QStringLiteral("system");
    sys.content = consolidationSystemPrompt(positive);

    AIChatMessage user;
    user.role = QStringLiteral("user");
    user.content = body;

    // Sem parent de widget — processo de fundo desacoplado da UI que
    // disparou o like/dislike (a bolha pode já nem existir mais quando a
    // resposta chegar). Mesmo padrão de AIClient efêmero já usado em
    // AIChatPanel::handleReadDocumentTool, só que sem dono na árvore Qt.
    auto* consolidator = new AIClient(nullptr);
    QSettings settings;
    consolidator->setApiKey(settings.value(QStringLiteral("ai/apiKey")).toString());
    consolidator->setBaseUrl(settings.value(QStringLiteral("ai/baseUrl"),
        QStringLiteral("https://api.openai.com/v1")).toString());
    consolidator->setModel(settings.value(QStringLiteral("ai/model"),
        QStringLiteral("gpt-4o-mini")).toString());

    QObject::connect(consolidator, &AIClient::finished, consolidator,
                      [positive](const QString& newSummary) {
        // Relê do disco: outro like/dislike pode ter chegado durante a
        // chamada de rede — não queremos sobrescrever exemplos novos que
        // chegaram nesse meio-tempo.
        Data d = load();
        if (positive) {
            d.positiveSummary = newSummary.trimmed();
            d.positiveExamples = QJsonArray();
        } else {
            d.negativeSummary = newSummary.trimmed();
            d.negativeExamples = QJsonArray();
        }
        save(d);
    });
    QObject::connect(consolidator, &AIClient::finished, consolidator, &QObject::deleteLater);
    QObject::connect(consolidator, &AIClient::errorOccurred, consolidator,
                      [](const QString&) {
        // Falha silenciosa: os exemplos brutos continuam salvos (nada foi
        // limpo ainda), a próxima vez que o teto for atingido tenta de novo.
    });
    QObject::connect(consolidator, &AIClient::errorOccurred, consolidator, &QObject::deleteLater);

    consolidator->sendMessage({ sys, user });
}

} // namespace

QString filePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/mira_style.json");
}

Data load()
{
    Data data;
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return data;

    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    data.positiveExamples = root.value(QStringLiteral("positiveExamples")).toArray();
    data.negativeExamples = root.value(QStringLiteral("negativeExamples")).toArray();
    data.positiveSummary = root.value(QStringLiteral("positiveSummary")).toString();
    data.negativeSummary = root.value(QStringLiteral("negativeSummary")).toString();
    return data;
}

void save(const Data& data)
{
    QJsonObject root;
    root[QStringLiteral("positiveExamples")] = data.positiveExamples;
    root[QStringLiteral("negativeExamples")] = data.negativeExamples;
    root[QStringLiteral("positiveSummary")] = data.positiveSummary;
    root[QStringLiteral("negativeSummary")] = data.negativeSummary;

    QFile f(filePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

void recordFeedback(bool positive, const QString& fullResponseText)
{
    if (fullResponseText.trimmed().isEmpty()) return;

    Data data = load();
    QJsonObject entry;
    entry[QStringLiteral("text")] = fullResponseText;
    entry[QStringLiteral("timestamp")] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonArray& arr = positive ? data.positiveExamples : data.negativeExamples;
    arr.append(entry);
    save(data); // grava já com o exemplo novo, mesmo que a consolidação abaixo não dispare/falhe

    if (arr.size() >= consolidationThreshold()) {
        triggerConsolidation(positive);
    }
}

QString buildPromptFragment()
{
    const Data data = load();
    if (data.positiveSummary.isEmpty() && data.negativeSummary.isEmpty()) return QString();

    QString out = QStringLiteral(
        "\n\n## Estilo de resposta aprendido com feedback do autor\n\n"
        "O autor já reagiu com 👍/👎 a respostas anteriores. Use isso como "
        "orientação adicional de estilo, sem contradizer a personalidade e "
        "as regras definidas acima.");
    if (!data.positiveSummary.isEmpty()) {
        out += QStringLiteral("\n\nO que o autor GOSTOU no estilo de respostas anteriores:\n%1")
            .arg(data.positiveSummary);
    }
    if (!data.negativeSummary.isEmpty()) {
        out += QStringLiteral("\n\nO que o autor NÃO gostou / quer evitar:\n%1")
            .arg(data.negativeSummary);
    }
    return out;
}

} // namespace MiraStyleStore
