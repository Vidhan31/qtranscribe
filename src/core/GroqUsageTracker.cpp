#include "GroqUsageTracker.h"

#include "GroqApiClient.h"
#include "LoggingCategories.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>

#include <algorithm>

using namespace Qt::StringLiterals;

GroqUsageTracker::GroqUsageTracker(QObject* parent)
    : QObject(parent) {
    loadFromSettings();
}

void GroqUsageTracker::setApiClient(GroqApiClient* client) {
    if (m_apiClient == client) {
        return;
    }

    if (m_apiClient) {
        disconnect(m_apiClient, &GroqApiClient::apiKeySetChanged, this, nullptr);
        disconnect(m_apiClient, &GroqApiClient::responseProcessed, this, &GroqUsageTracker::recordResponse);
    }

    m_apiClient = client;

    if (m_apiClient) {
        connect(m_apiClient, &GroqApiClient::apiKeySetChanged, this, [this]() {
            if (m_apiClient && m_apiClient->apiKeySet() && !m_hasData) {
                qCDebug(lcNetwork) << "Groq API key became available, auto-refreshing quota...";
                refreshQuota();
            }
        });

        connect(m_apiClient, &GroqApiClient::responseProcessed, this, &GroqUsageTracker::recordResponse);

        if (m_apiClient->apiKeySet() && !m_hasData) {
            qCDebug(lcNetwork) << "Groq API key already available on init, refreshing quota...";
            refreshQuota();
        }
    }
}

GroqApiClient* GroqUsageTracker::apiClient() const {
    return m_apiClient;
}

bool GroqUsageTracker::hasData() const {
    return m_hasData;
}

qint64 GroqUsageTracker::limitRequests() const {
    return m_limitRequests;
}

qint64 GroqUsageTracker::remainingRequests() const {
    return m_remainingRequests;
}

QString GroqUsageTracker::resetRequests() const {
    return m_resetRequests;
}

qreal GroqUsageTracker::requestsUsageFraction() const {
    if (m_limitRequests <= 0) {
        return 0.0;
    }
    const qint64 used = m_limitRequests - m_remainingRequests;
    return std::clamp(static_cast<qreal>(used) / static_cast<qreal>(m_limitRequests), 0.0, 1.0);
}

qint64 GroqUsageTracker::limitTokens() const {
    return m_limitTokens;
}

qint64 GroqUsageTracker::remainingTokens() const {
    return m_remainingTokens;
}

QString GroqUsageTracker::resetTokens() const {
    return m_resetTokens;
}

qreal GroqUsageTracker::tokensUsageFraction() const {
    if (m_limitTokens <= 0) {
        return 0.0;
    }
    const qint64 used = m_limitTokens - m_remainingTokens;
    return std::clamp(static_cast<qreal>(used) / static_cast<qreal>(m_limitTokens), 0.0, 1.0);
}

bool GroqUsageTracker::hasAudioSecondsLimit() const {
    return m_hasAudioSecondsLimit;
}

qint64 GroqUsageTracker::limitAudioSeconds() const {
    return m_limitAudioSeconds;
}

qint64 GroqUsageTracker::remainingAudioSeconds() const {
    return m_remainingAudioSeconds;
}

QString GroqUsageTracker::resetAudioSeconds() const {
    return m_resetAudioSeconds;
}

bool GroqUsageTracker::isRateLimited() const {
    return m_isRateLimited;
}

int GroqUsageTracker::retryAfterSeconds() const {
    return m_retryAfterSeconds;
}

int GroqUsageTracker::sessionTotalRequests() const {
    return m_sessionTotalRequests;
}

int GroqUsageTracker::sessionSttRequests() const {
    return m_sessionSttRequests;
}

int GroqUsageTracker::sessionLlmRequests() const {
    return m_sessionLlmRequests;
}

qint64 GroqUsageTracker::sessionPromptTokens() const {
    return m_sessionPromptTokens;
}

qint64 GroqUsageTracker::sessionCompletionTokens() const {
    return m_sessionCompletionTokens;
}

qint64 GroqUsageTracker::sessionTotalTokens() const {
    return m_sessionTotalTokens;
}

QString GroqUsageTracker::lastEndpoint() const {
    return m_lastEndpoint;
}

QString GroqUsageTracker::lastModel() const {
    return m_lastModel;
}

int GroqUsageTracker::lastHttpStatus() const {
    return m_lastHttpStatus;
}

qint64 GroqUsageTracker::lastLatencyMs() const {
    return m_lastLatencyMs;
}

QString GroqUsageTracker::lastUpdatedTimestamp() const {
    return m_lastUpdatedTimestamp;
}

bool GroqUsageTracker::checkingQuota() const {
    return m_checkingQuota;
}

QString GroqUsageTracker::quotaCheckError() const {
    return m_quotaCheckError;
}

void GroqUsageTracker::setCheckingQuota(bool checking) {
    if (m_checkingQuota != checking) {
        m_checkingQuota = checking;
        emit checkingQuotaChanged();
    }
}

void GroqUsageTracker::setQuotaCheckError(const QString& error) {
    if (m_quotaCheckError != error) {
        m_quotaCheckError = error;
        emit quotaCheckErrorChanged();
    }
}

void GroqUsageTracker::recordResponse(QNetworkReply* reply, const QString& endpointType, const QString& model,
                                      qint64 latencyMs, const QByteArray& responseBody) {
    if (!reply) {
        return;
    }

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    m_lastHttpStatus = httpStatus;
    m_lastEndpoint = endpointType;
    m_lastModel = model;
    m_lastLatencyMs = latencyMs;
    m_lastUpdatedTimestamp = QDateTime::currentDateTimeUtc().toString(u"MMM d, hh:mm:ss AP 'UTC'"_s);

    m_isRateLimited = (httpStatus == 429);

    const QByteArray hdrRetryAfter = reply->rawHeader("retry-after");
    m_retryAfterSeconds = GroqResponseParser::parseRetryAfterSeconds(hdrRetryAfter);

    qCDebug(lcNetwork) << "GroqUsageTracker: Processing response for" << endpointType << "Status:" << httpStatus
                       << "Latency:" << latencyMs << "ms";

    bool updatedHeaders = false;

    const QByteArray hdrLimitReq = reply->rawHeader("x-ratelimit-limit-requests");
    if (!hdrLimitReq.isEmpty()) {
        m_limitRequests = hdrLimitReq.toLongLong();
        updatedHeaders = true;
    }

    const QByteArray hdrRemReq = reply->rawHeader("x-ratelimit-remaining-requests");
    if (!hdrRemReq.isEmpty()) {
        m_remainingRequests = hdrRemReq.toLongLong();
        updatedHeaders = true;
    }

    const QByteArray hdrResetReq = reply->rawHeader("x-ratelimit-reset-requests");
    if (!hdrResetReq.isEmpty()) {
        m_resetRequests = QString::fromUtf8(hdrResetReq).trimmed();
        updatedHeaders = true;
    }

    const QByteArray hdrLimitTok = reply->rawHeader("x-ratelimit-limit-tokens");
    if (!hdrLimitTok.isEmpty()) {
        m_limitTokens = hdrLimitTok.toLongLong();
        updatedHeaders = true;
    }

    const QByteArray hdrRemTok = reply->rawHeader("x-ratelimit-remaining-tokens");
    if (!hdrRemTok.isEmpty()) {
        m_remainingTokens = hdrRemTok.toLongLong();
        updatedHeaders = true;
    }

    const QByteArray hdrResetTok = reply->rawHeader("x-ratelimit-reset-tokens");
    if (!hdrResetTok.isEmpty()) {
        m_resetTokens = QString::fromUtf8(hdrResetTok).trimmed();
        updatedHeaders = true;
    }

    const QByteArray hdrLimitAud = reply->rawHeader("x-ratelimit-limit-audio-seconds");
    const QByteArray hdrRemAud = reply->rawHeader("x-ratelimit-remaining-audio-seconds");
    const QByteArray hdrResetAud = reply->rawHeader("x-ratelimit-reset-audio-seconds");
    if (!hdrLimitAud.isEmpty() || !hdrRemAud.isEmpty()) {
        m_hasAudioSecondsLimit = true;
        if (!hdrLimitAud.isEmpty())
            m_limitAudioSeconds = hdrLimitAud.toLongLong();
        if (!hdrRemAud.isEmpty())
            m_remainingAudioSeconds = hdrRemAud.toLongLong();
        if (!hdrResetAud.isEmpty())
            m_resetAudioSeconds = QString::fromUtf8(hdrResetAud).trimmed();
        updatedHeaders = true;
    }

    if (updatedHeaders) {
        m_hasData = true;
        saveToSettings();
    }

    if (!responseBody.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(responseBody);
        if (doc.isObject()) {
            const QJsonObject rootObj = doc.object();
            if (rootObj.contains(u"usage"_s) && rootObj.value(u"usage"_s).isObject()) {
                const QJsonObject usageObj = rootObj.value(u"usage"_s).toObject();
                const qint64 promptTok = usageObj.value(u"prompt_tokens"_s).toInteger();
                const qint64 compTok = usageObj.value(u"completion_tokens"_s).toInteger();
                const qint64 totalTok = usageObj.value(u"total_tokens"_s).toInteger();

                m_sessionPromptTokens += promptTok;
                m_sessionCompletionTokens += compTok;
                m_sessionTotalTokens += (totalTok > 0 ? totalTok : (promptTok + compTok));
            }
        }
    }

    m_sessionTotalRequests++;
    if (endpointType.contains(u"Transcription"_s, Qt::CaseInsensitive) ||
        endpointType.contains(u"STT"_s, Qt::CaseInsensitive)) {
        m_sessionSttRequests++;
    } else if (endpointType.contains(u"LLM"_s, Qt::CaseInsensitive) ||
               endpointType.contains(u"Post-Processing"_s, Qt::CaseInsensitive)) {
        m_sessionLlmRequests++;
    }

    qCDebug(lcNetwork) << "GroqUsageTracker state updated ->" << "RPD:" << m_remainingRequests << "/" << m_limitRequests
                       << "TPM:" << m_remainingTokens << "/" << m_limitTokens << "HasData:" << m_hasData
                       << "SessionRequests:" << m_sessionTotalRequests;

    emit dataChanged();
    emit sessionStatsChanged();
    emit lastRequestChanged();
}

void GroqUsageTracker::refreshQuota() {
    if (!m_apiClient || !m_apiClient->apiKeySet()) {
        qCDebug(lcNetwork) << "GroqUsageTracker: Cannot refresh quota — API key is not set";
        setQuotaCheckError(u"Groq API key is not configured"_s);
        return;
    }

    if (m_checkingQuota) {
        return;
    }

    setCheckingQuota(true);
    setQuotaCheckError({});

    // Use a lightweight 1-token chat completion ping to retrieve full RPD and TPM rate limit headers
    QJsonObject rootObj;
    rootObj[u"model"_s] = u"openai/gpt-oss-20b"_s;
    QJsonArray messages;
    QJsonObject userMsg;
    userMsg[u"role"_s] = u"user"_s;
    userMsg[u"content"_s] = u"ping"_s;
    messages.append(userMsg);
    rootObj[u"messages"_s] = messages;
    rootObj[u"max_completion_tokens"_s] = 1;

    qCDebug(lcNetwork) << "Sending Groq quota refresh request via GroqApiClient::postJson";

    m_apiClient->postJson(u"chat/completions"_s, rootObj, u"Quota Check"_s, u"openai/gpt-oss-20b"_s,
                          [this](const GroqApiResponse& res) {
                              setCheckingQuota(false);

                              if (!res.isSuccess) {
                                  qWarning() << "GroqUsageTracker quota refresh error:" << res.errorMessage;
                                  setQuotaCheckError(res.errorMessage);
                              } else {
                                  setQuotaCheckError({});
                                  qCDebug(lcNetwork) << "Groq quota refresh succeeded in" << res.latencyMs << "ms";
                              }
                          });
}

void GroqUsageTracker::resetSessionStats() {
    m_sessionTotalRequests = 0;
    m_sessionSttRequests = 0;
    m_sessionLlmRequests = 0;
    m_sessionPromptTokens = 0;
    m_sessionCompletionTokens = 0;
    m_sessionTotalTokens = 0;

    emit sessionStatsChanged();
    qCDebug(lcNetwork) << "GroqUsageTracker session stats reset";
}

void GroqUsageTracker::loadFromSettings() {
    QSettings settings;
    m_hasData = settings.value(u"GroqUsage/HasData"_s, false).toBool();
    if (m_hasData) {
        m_limitRequests = settings.value(u"GroqUsage/LimitRequests"_s, 0).toLongLong();
        m_remainingRequests = settings.value(u"GroqUsage/RemainingRequests"_s, 0).toLongLong();
        m_resetRequests = settings.value(u"GroqUsage/ResetRequests"_s, QString()).toString();

        m_limitTokens = settings.value(u"GroqUsage/LimitTokens"_s, 0).toLongLong();
        m_remainingTokens = settings.value(u"GroqUsage/RemainingTokens"_s, 0).toLongLong();
        m_resetTokens = settings.value(u"GroqUsage/ResetTokens"_s, QString()).toString();

        m_hasAudioSecondsLimit = settings.value(u"GroqUsage/HasAudioSecondsLimit"_s, false).toBool();
        m_limitAudioSeconds = settings.value(u"GroqUsage/LimitAudioSeconds"_s, 0).toLongLong();
        m_remainingAudioSeconds = settings.value(u"GroqUsage/RemainingAudioSeconds"_s, 0).toLongLong();
        m_resetAudioSeconds = settings.value(u"GroqUsage/ResetAudioSeconds"_s, QString()).toString();

        m_lastUpdatedTimestamp = settings.value(u"GroqUsage/LastUpdated"_s, QString()).toString();
    }
}

void GroqUsageTracker::saveToSettings() {
    QSettings settings;
    settings.setValue(u"GroqUsage/HasData"_s, m_hasData);
    settings.setValue(u"GroqUsage/LimitRequests"_s, m_limitRequests);
    settings.setValue(u"GroqUsage/RemainingRequests"_s, m_remainingRequests);
    settings.setValue(u"GroqUsage/ResetRequests"_s, m_resetRequests);

    settings.setValue(u"GroqUsage/LimitTokens"_s, m_limitTokens);
    settings.setValue(u"GroqUsage/RemainingTokens"_s, m_remainingTokens);
    settings.setValue(u"GroqUsage/ResetTokens"_s, m_resetTokens);

    settings.setValue(u"GroqUsage/HasAudioSecondsLimit"_s, m_hasAudioSecondsLimit);
    settings.setValue(u"GroqUsage/LimitAudioSeconds"_s, m_limitAudioSeconds);
    settings.setValue(u"GroqUsage/RemainingAudioSeconds"_s, m_remainingAudioSeconds);
    settings.setValue(u"GroqUsage/ResetAudioSeconds"_s, m_resetAudioSeconds);

    settings.setValue(u"GroqUsage/LastUpdated"_s, m_lastUpdatedTimestamp);
}
