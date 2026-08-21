#include "GroqResponseParser.h"

#include <QHttpHeaders>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

#include <cmath>

using namespace Qt::StringLiterals;

namespace GroqResponseParser {

int parseRetryAfterSeconds(const QByteArray& raw) {
    if (raw.isEmpty()) {
        return 0;
    }
    QString str = QString::fromUtf8(raw).trimmed();
    if (str.endsWith(u's', Qt::CaseInsensitive)) {
        str.chop(1);
    }
    bool ok = false;
    const double val = str.toDouble(&ok);
    if (ok && val > 0.0) {
        return static_cast<int>(std::ceil(val));
    }
    return 0;
}

QString extractApiErrorMessage(const QByteArray& responseBody, const QString& defaultError) {
    if (responseBody.isEmpty()) {
        return defaultError;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(responseBody);
    if (doc.isObject()) {
        const QJsonObject rootObj = doc.object();
        if (rootObj.contains(u"error"_s)) {
            const QJsonValue errVal = rootObj.value(u"error"_s);
            if (errVal.isObject()) {
                const QString msg = errVal.toObject().value(u"message"_s).toString();
                if (!msg.isEmpty()) {
                    return u"Groq API error: %1"_s.arg(msg);
                }
            } else if (errVal.isString()) {
                return u"Groq API error: %1"_s.arg(errVal.toString());
            }
        }
    }

    return defaultError.isEmpty() ? u"Invalid response from Groq API"_s : defaultError;
}

GroqApiResponse parseReply(QNetworkReply* reply, qint64 latencyMs) {
    GroqApiResponse response;
    if (!reply) {
        response.errorMessage = u"Null network reply"_s;
        return response;
    }

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netErr = reply->error();
    const QString errorString = reply->errorString();
    const QByteArray responseBody = reply->readAll();

    response.httpStatus = httpStatus;
    response.latencyMs = latencyMs;
    response.rawBody = responseBody;
    response.networkError = netErr;
    response.isRateLimited = (httpStatus == 429);
    QByteArray retryAfterRaw = reply->headers().value(QHttpHeaders::WellKnownHeader::RetryAfter).toByteArray();
    if (retryAfterRaw.isEmpty()) {
        retryAfterRaw = reply->rawHeader("retry-after");
    }
    response.retryAfterSeconds = parseRetryAfterSeconds(retryAfterRaw);

    if (netErr == QNetworkReply::NoError && httpStatus >= 200 && httpStatus < 300) {
        response.isSuccess = true;
        const QJsonDocument doc = QJsonDocument::fromJson(responseBody);
        if (doc.isObject()) {
            response.json = doc.object();
        }
    } else {
        response.isSuccess = false;
        response.errorMessage = extractApiErrorMessage(responseBody, u"Network error: %1"_s.arg(errorString));
    }

    return response;
}

} // namespace GroqResponseParser
