#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QString>

struct GroqApiResponse {
    int httpStatus = 0;
    qint64 latencyMs = 0;
    QByteArray rawBody;
    QJsonObject json;
    QString errorMessage;
    bool isSuccess = false;
    bool isRateLimited = false;
    int retryAfterSeconds = 0;
    QNetworkReply::NetworkError networkError = QNetworkReply::NoError;
};

namespace GroqResponseParser {
/**
 * @brief Parses an HTTP response from Groq API without taking ownership of the QNetworkReply.
 * The caller remains responsible for the lifetime and deletion (e.g. via deleteLater()) of reply.
 */
GroqApiResponse parseReply(QNetworkReply* reply, qint64 latencyMs);
int parseRetryAfterSeconds(const QByteArray& raw);
QString extractApiErrorMessage(const QByteArray& responseBody, const QString& defaultError = QString());

} // namespace GroqResponseParser
