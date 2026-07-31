#include "ImageGenClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {
const char* kImageGenerationsUrl = "https://api.openai.com/v1/images/generations";
}

ImageGenClient::ImageGenClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void ImageGenClient::setApiKey(const QString& key)
{
    m_apiKey = key;
}

void ImageGenClient::generate(const QString& prompt, const QString& model,
                              const QString& quality, const QString& size)
{
    if (m_reply) return; // já tem uma chamada em andamento

    QJsonObject payload;
    payload[QStringLiteral("model")] = model;
    payload[QStringLiteral("prompt")] = prompt;
    payload[QStringLiteral("size")] = size;
    payload[QStringLiteral("quality")] = quality;
    payload[QStringLiteral("n")] = 1;

    QNetworkRequest req(QUrl(QString::fromLatin1(kImageGenerationsUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/json"));
    req.setRawHeader("Authorization", QByteArray("Bearer ") + m_apiKey.toUtf8());

    m_reply = m_nam->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::finished, this, &ImageGenClient::handleFinished);
}

void ImageGenClient::handleFinished()
{
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();

    const QByteArray raw = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    const QJsonObject obj = doc.object();

    if (reply->error() != QNetworkReply::NoError) {
        const QString apiMsg = obj.value(QStringLiteral("error")).toObject()
            .value(QStringLiteral("message")).toString();
        emit errorOccurred(apiMsg.isEmpty() ? reply->errorString() : apiMsg);
        return;
    }

    const QJsonArray data = obj.value(QStringLiteral("data")).toArray();
    if (data.isEmpty()) {
        const QString apiMsg = obj.value(QStringLiteral("error")).toObject()
            .value(QStringLiteral("message")).toString();
        emit errorOccurred(apiMsg.isEmpty()
            ? QStringLiteral("Resposta da API de imagem sem dados.") : apiMsg);
        return;
    }

    const QString b64 = data.first().toObject().value(QStringLiteral("b64_json")).toString();
    const QByteArray bytes = QByteArray::fromBase64(b64.toUtf8());
    QImage img;
    if (bytes.isEmpty() || !img.loadFromData(bytes)) {
        emit errorOccurred(QStringLiteral("Não foi possível decodificar a imagem retornada pela API."));
        return;
    }

    emit finished(img);
}
