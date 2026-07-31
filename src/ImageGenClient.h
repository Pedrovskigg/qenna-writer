#pragma once

#include <QImage>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// Cliente de request único (não-streaming) pra POST /images/generations da
// OpenAI (gpt-image-1 / gpt-image-1-mini). Separado do AIClient de propósito:
// endpoint, payload e formato de resposta são incompatíveis com o chat
// completion SSE que o AIClient já implementa. O host é SEMPRE a OpenAI real
// (nunca ai/baseUrl, que pode apontar pra um proxy OpenAI-compatible sem
// esse endpoint) — só a API key é reaproveitada.
class ImageGenClient : public QObject {
    Q_OBJECT
public:
    explicit ImageGenClient(QObject* parent = nullptr);

    void setApiKey(const QString& key);

    // model: "gpt-image-1" | "gpt-image-1-mini"
    // quality: "low" | "medium" | "high"
    // size: "1024x1024" | "1024x1536" | "1536x1024"
    void generate(const QString& prompt, const QString& model,
                  const QString& quality, const QString& size);

    bool isBusy() const { return m_reply != nullptr; }

signals:
    void finished(const QImage& image);
    void errorOccurred(const QString& message);

private:
    void handleFinished();

    QNetworkAccessManager* m_nam;
    QNetworkReply* m_reply = nullptr;
    QString m_apiKey;
};
