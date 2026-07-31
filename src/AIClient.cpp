#include "AIClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

AIClient::AIClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void AIClient::setApiKey(const QString& key)
{
    m_apiKey = key;
}

void AIClient::setBaseUrl(const QString& url)
{
    m_baseUrl = url;
}

void AIClient::setModel(const QString& model)
{
    m_model = model;
}

void AIClient::setTools(const QVector<AITool>& tools)
{
    m_tools = tools;
}

void AIClient::abort()
{
    if (m_reply) {
        m_reply->abort();
    }
}

void AIClient::sendMessage(const QVector<AIChatMessage>& messages)
{
    if (m_reply) {
        emit errorOccurred(QStringLiteral("Já existe uma requisição em andamento."));
        return;
    }
    if (m_apiKey.isEmpty()) {
        emit errorOccurred(QStringLiteral("Nenhuma chave de API configurada."));
        return;
    }

    QJsonArray messagesArray;
    for (const AIChatMessage& msg : messages) {
        QJsonObject obj;
        obj[QStringLiteral("role")] = msg.role;
        if (msg.imageDataUrl.isEmpty()) {
            obj[QStringLiteral("content")] = msg.content;
        } else {
            // Conteúdo multimodal: array com a parte de texto + a imagem,
            // formato que a API exige quando uma mensagem inclui visão.
            QJsonObject textPart;
            textPart[QStringLiteral("type")] = QStringLiteral("text");
            textPart[QStringLiteral("text")] = msg.content;

            QJsonObject imageUrlObj;
            imageUrlObj[QStringLiteral("url")] = msg.imageDataUrl;
            QJsonObject imagePart;
            imagePart[QStringLiteral("type")] = QStringLiteral("image_url");
            imagePart[QStringLiteral("image_url")] = imageUrlObj;

            QJsonArray contentArray;
            contentArray.append(textPart);
            contentArray.append(imagePart);
            obj[QStringLiteral("content")] = contentArray;
        }
        if (!msg.toolCalls.isEmpty()) {
            QJsonArray tcArray;
            for (const AIToolCall& tc : msg.toolCalls) {
                QJsonObject func;
                func[QStringLiteral("name")] = tc.name;
                func[QStringLiteral("arguments")] = tc.argumentsJson;

                QJsonObject tcObj;
                tcObj[QStringLiteral("id")] = tc.id;
                tcObj[QStringLiteral("type")] = QStringLiteral("function");
                tcObj[QStringLiteral("function")] = func;
                tcArray.append(tcObj);
            }
            obj[QStringLiteral("tool_calls")] = tcArray;
        }
        if (msg.role == QStringLiteral("tool") && !msg.toolCallId.isEmpty()) {
            obj[QStringLiteral("tool_call_id")] = msg.toolCallId;
        }
        messagesArray.append(obj);
    }

    QJsonObject payload;
    payload[QStringLiteral("model")] = m_model;
    payload[QStringLiteral("messages")] = messagesArray;
    payload[QStringLiteral("stream")] = true;

    if (!m_tools.isEmpty()) {
        QJsonArray toolsArray;
        for (const AITool& tool : m_tools) {
            QJsonObject function;
            function[QStringLiteral("name")] = tool.name;
            function[QStringLiteral("description")] = tool.description;
            function[QStringLiteral("parameters")] = tool.parameters;

            QJsonObject toolObj;
            toolObj[QStringLiteral("type")] = QStringLiteral("function");
            toolObj[QStringLiteral("function")] = function;
            toolsArray.append(toolObj);
        }
        payload[QStringLiteral("tools")] = toolsArray;
        payload[QStringLiteral("tool_choice")] = QStringLiteral("auto");
    }

    QNetworkRequest req(QUrl(m_baseUrl + QStringLiteral("/chat/completions")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/json"));
    req.setRawHeader("Authorization", QByteArray("Bearer ") + m_apiKey.toUtf8());

    m_sseBuffer.clear();
    m_accumulated.clear();
    m_toolCallAccum.clear();

    m_reply = m_nam->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::readyRead, this, &AIClient::handleReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &AIClient::handleFinished);
}

void AIClient::handleReadyRead()
{
    if (!m_reply) return;

    m_sseBuffer += m_reply->readAll();

    int newlineIdx;
    while ((newlineIdx = m_sseBuffer.indexOf('\n')) != -1) {
        const QByteArray line = m_sseBuffer.left(newlineIdx);
        m_sseBuffer.remove(0, newlineIdx + 1);
        processSseLine(line);
    }
}

void AIClient::processSseLine(const QByteArray& lineRaw)
{
    QByteArray line = lineRaw.trimmed();
    if (line.isEmpty()) return;
    if (!line.startsWith("data:")) return;

    line.remove(0, 5); // "data:"
    line = line.trimmed();
    if (line == "[DONE]") return;

    const QJsonDocument doc = QJsonDocument::fromJson(line);
    if (!doc.isObject()) return;
    const QJsonObject obj = doc.object();

    if (obj.contains(QStringLiteral("error"))) {
        const QJsonObject err = obj.value(QStringLiteral("error")).toObject();
        emit errorOccurred(err.value(QStringLiteral("message")).toString(QStringLiteral("Erro desconhecido da API.")));
        return;
    }

    const QJsonArray choices = obj.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) return;

    const QJsonObject delta = choices.first().toObject().value(QStringLiteral("delta")).toObject();

    // Chamada de tool: fragmentos de argumento chegam incrementais, um
    // "index" por tool call (normalmente só 0). Acumula até o fim do stream.
    const QJsonArray toolCalls = delta.value(QStringLiteral("tool_calls")).toArray();
    for (const QJsonValue& tcVal : toolCalls) {
        const QJsonObject tc = tcVal.toObject();
        const int index = tc.value(QStringLiteral("index")).toInt();
        ToolCallAccum& acc = m_toolCallAccum[index];
        if (tc.contains(QStringLiteral("id"))) {
            acc.id = tc.value(QStringLiteral("id")).toString();
        }
        const QJsonObject func = tc.value(QStringLiteral("function")).toObject();
        if (func.contains(QStringLiteral("name"))) {
            acc.name = func.value(QStringLiteral("name")).toString();
        }
        if (func.contains(QStringLiteral("arguments"))) {
            acc.argsJson += func.value(QStringLiteral("arguments")).toString();
        }
    }

    const QString content = delta.value(QStringLiteral("content")).toString();
    if (content.isEmpty()) return;

    m_accumulated += content;
    emit tokenReceived(content);
}

void AIClient::handleFinished()
{
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    if (!reply) return;

    // Processa qualquer resto no buffer que não terminou com '\n'.
    if (!m_sseBuffer.isEmpty()) {
        processSseLine(m_sseBuffer);
        m_sseBuffer.clear();
    }

    const bool hadError = reply->error() != QNetworkReply::NoError;
    if (hadError && m_accumulated.isEmpty() && m_toolCallAccum.isEmpty()) {
        emit errorOccurred(reply->errorString());
    } else {
        // Tool calls primeiro: quem escuta (ex. AIChatPanel) pode precisar
        // marcar "não é resposta final ainda" antes do finished() abaixo
        // disparar, pra montar o round-trip completo (tool_calls + tool
        // role + reenvio) em vez de tratar isso como turno encerrado.
        for (auto it = m_toolCallAccum.constBegin(); it != m_toolCallAccum.constEnd(); ++it) {
            const QJsonDocument argsDoc = QJsonDocument::fromJson(it.value().argsJson.toUtf8());
            emit toolCallReceived(it.value().id, it.value().name, argsDoc.object());
        }
        m_toolCallAccum.clear();
        emit finished(m_accumulated);
    }

    reply->deleteLater();
}
