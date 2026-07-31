#pragma once

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

// Uma chamada de tool feita pelo modelo, no formato exato que a API espera
// de volta quando o histórico é reenviado (echo do id/nome/argumentos).
struct AIToolCall {
    QString id;
    QString name;
    QString argumentsJson; // JSON já serializado, como a API originalmente mandou
};

// Uma mensagem de conversa no formato OpenAI-compatible.
struct AIChatMessage {
    QString role;     // "system" | "user" | "assistant" | "tool"
    QString content;
    QVector<AIToolCall> toolCalls; // só quando role=="assistant" e ele chamou tool(s)
    QString toolCallId;            // só quando role=="tool" — qual chamada está sendo respondida
    // Imagem anexada a este turno (data URL base64) — só funciona com
    // modelos com visão (ex. gpt-4o/gpt-4o-mini). Quando preenchida, o
    // "content" é serializado como array multimodal (texto + image_url) em
    // vez de string simples. Vazio = mensagem só-texto normal.
    QString imageDataUrl;
};

// Uma tool (function calling) que o modelo pode chamar em vez de responder
// em texto livre. `parameters` é o JSON Schema do objeto de argumentos
// (formato OpenAI: {"type":"object","properties":{...},"required":[...]}).
struct AITool {
    QString name;
    QString description;
    QJsonObject parameters;
};

// Cliente de chat com API OpenAI-compatible (endpoint, chave e modelo vêm de
// fora — configuráveis pelo usuário). Streaming via SSE: cada chamada de
// sendMessage() emite tokenReceived() incrementalmente conforme os chunks
// chegam, e finished() com o texto completo ao fim.
// Mesma ferramenta de rede que o UpdateChecker (QNetworkAccessManager), mas
// lendo a resposta via readyRead() em vez de esperar finished() porque o
// corpo é um stream, não um JSON único.
//
// Suporta tool calling real (não parsing de regex em cima de texto livre):
// se setTools() foi chamado, o modelo pode responder com uma chamada de
// função em vez de texto — os fragmentos de argumento chegam incrementais
// no stream (delta.tool_calls[].function.arguments) e são acumulados por
// índice; ao fim do stream, toolCallReceived() é emitido uma vez por tool
// call completa, já com os argumentos parseados como QJsonObject.
class AIClient : public QObject {
    Q_OBJECT
public:
    explicit AIClient(QObject* parent = nullptr);

    void setApiKey(const QString& key);
    void setBaseUrl(const QString& url);   // ex: https://api.openai.com/v1 (sem barra final)
    void setModel(const QString& model);   // ex: gpt-4o
    void setTools(const QVector<AITool>& tools);

    // Cancela uma requisição em andamento, se houver.
    void abort();
    bool isBusy() const { return m_reply != nullptr; }

public slots:
    void sendMessage(const QVector<AIChatMessage>& messages);

signals:
    void tokenReceived(const QString& token);
    void finished(const QString& fullText);
    void toolCallReceived(const QString& id, const QString& name, const QJsonObject& arguments);
    void errorOccurred(const QString& message);

private:
    struct ToolCallAccum {
        QString id;
        QString name;
        QString argsJson;
    };

    void handleReadyRead();
    void handleFinished();
    void processSseLine(const QByteArray& line);

    QNetworkAccessManager* m_nam;
    QNetworkReply* m_reply = nullptr;

    QString m_apiKey;
    QString m_baseUrl = QStringLiteral("https://api.openai.com/v1");
    QString m_model = QStringLiteral("gpt-4o-mini");
    QVector<AITool> m_tools;

    QByteArray m_sseBuffer;   // bytes recebidos ainda sem quebra de linha completa
    QString m_accumulated;    // texto completo acumulado nesta requisição
    QMap<int, ToolCallAccum> m_toolCallAccum; // por índice, conforme chega no stream
};
