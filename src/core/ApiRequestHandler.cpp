#include "ApiRequestHandler.h"

#include <QNetworkReply>

using namespace Qt::StringLiterals;

ApiRequestHandler::ApiRequestHandler(QObject* parent)
    : QObject(parent) { }

ApiRequestHandler::~ApiRequestHandler() {
    if (m_currentReply) {
        m_currentReply->abort();
    }
}

bool ApiRequestHandler::isBusy() const {
    return m_busy;
}

bool ApiRequestHandler::isCancelled() const {
    return m_cancelled;
}

void ApiRequestHandler::cancel() {
    m_cancelled = true;
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply = nullptr;
    }
    m_retryCount = 0;
    setBusy(false);
}

void ApiRequestHandler::setBusy(bool busy) {
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

void ApiRequestHandler::setCurrentReply(QNetworkReply* reply) {
    m_currentReply = reply;
}

void ApiRequestHandler::resetRequestState() {
    m_retryCount = 0;
    m_currentReply = nullptr;
    setBusy(false);
}

void ApiRequestHandler::prepareNewRequest() {
    m_cancelled = false;
    m_retryCount = 0;
}

bool ApiRequestHandler::shouldRetry(const GroqApiResponse& res) const {
    if (m_cancelled || m_retryCount >= m_retryPolicy.maxRetries) {
        return false;
    }

    const bool isTransientRateLimit =
        (res.isRateLimited && res.retryAfterSeconds <= m_retryPolicy.maxTransientRetryAfterSec);
    const bool isTransientServerErr =
        (res.httpStatus == 503 || res.networkError == QNetworkReply::RemoteHostClosedError ||
         res.networkError == QNetworkReply::TemporaryNetworkFailureError);

    return (isTransientRateLimit || isTransientServerErr);
}

int ApiRequestHandler::calculateRetryDelayMs(const GroqApiResponse& res) const {
    if (res.retryAfterSeconds > 0) {
        return res.retryAfterSeconds * 1000;
    }
    return m_retryPolicy.defaultDelayMs;
}
