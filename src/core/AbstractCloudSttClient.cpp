#include "AbstractCloudSttClient.h"

#include "LoggingCategories.h"

#include "AbstractCloudApiClient.h"

#include <QSettings>
#include <QTimer>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

AbstractCloudSttClient::AbstractCloudSttClient(QObject* parent)
    : AbstractSttClient(parent)
    , m_retryCountdownTimer(new QTimer(this))
    , m_retryTimer(new QTimer(this)) {
    m_retryCountdownTimer->setInterval(1s);
    connect(m_retryCountdownTimer, &QTimer::timeout, this, [this]() {
        if (m_retrySecondsRemaining > 0) {
            setRetrySecondsRemaining(m_retrySecondsRemaining - 1);
        } else {
            m_retryCountdownTimer->stop();
        }
    });

    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, [this]() {
        if (!m_requestRunner.isCancelled() && !m_lastWavData.isEmpty()) {
            sendTranscribeRequest();
        }
    });
}

void AbstractCloudSttClient::setApiClient(AbstractCloudApiClient* apiClient) {
    if (m_apiClient == apiClient) {
        return;
    }
    const bool wasApiKeySet = isApiKeySet();
    const bool wasKeyInvalid = isApiKeyInvalid();
    if (m_apiClient) {
        disconnect(m_apiClient, &AbstractCloudApiClient::apiKeySetChanged, this,
                   &AbstractCloudSttClient::onApiKeySetChanged);
    }
    m_apiClient = apiClient;
    if (m_apiClient) {
        connect(m_apiClient, &AbstractCloudApiClient::apiKeySetChanged, this,
                &AbstractCloudSttClient::onApiKeySetChanged);
    }
    if (isApiKeySet() != wasApiKeySet) {
        emit apiKeySetChanged();
    }
    if (isApiKeyInvalid() != wasKeyInvalid) {
        emit isApiKeyInvalidChanged();
    }
    emit readyChanged();
}

AbstractCloudApiClient* AbstractCloudSttClient::apiClient() const {
    return m_apiClient;
}

void AbstractCloudSttClient::onApiKeySetChanged() {
    const bool wasKeyInvalid = isApiKeyInvalid();
    emit apiKeySetChanged();
    if (isApiKeyInvalid() != wasKeyInvalid) {
        emit isApiKeyInvalidChanged();
    }
    emit readyChanged();
}

bool AbstractCloudSttClient::isApiKeySet() const {
    return m_apiClient && m_apiClient->apiKeySet();
}

bool AbstractCloudSttClient::isApiKeyInvalid() const {
    return isApiKeySet() && m_errorCategory == ErrorCategory::InvalidApiKey;
}

bool AbstractCloudSttClient::isRateLimited() const {
    return m_errorCategory == ErrorCategory::RateLimited && m_retrySecondsRemaining > 0;
}

void AbstractCloudSttClient::activate() {
    if (m_apiClient) {
        m_apiClient->ensureApiKeyLoaded();
    }
    emit readyChanged();
}

void AbstractCloudSttClient::deactivate() {
    cancel();
}

bool AbstractCloudSttClient::isReady() const {
    return m_apiClient && m_apiClient->apiKeySet();
}

bool AbstractCloudSttClient::isBusy() const {
    return m_requestRunner.isBusy();
}

bool AbstractCloudSttClient::isCancelled() const {
    return m_requestRunner.isCancelled();
}

void AbstractCloudSttClient::setBusy(bool busy) {
    if (m_requestRunner.isBusy() != busy) {
        m_requestRunner.setBusy(busy);
        emit busyChanged();
    }
}

QString AbstractCloudSttClient::lastError() const {
    return m_lastError;
}

AbstractCloudSttClient::ErrorCategory AbstractCloudSttClient::errorCategory() const {
    return m_errorCategory;
}

int AbstractCloudSttClient::retrySecondsRemaining() const {
    return m_retrySecondsRemaining;
}

void AbstractCloudSttClient::setErrorCategory(ErrorCategory category) {
    if (m_errorCategory != category) {
        const bool wasKeyInvalid = isApiKeyInvalid();
        const bool wasRateLimited = isRateLimited();
        m_errorCategory = category;
        emit errorCategoryChanged();
        if (isApiKeyInvalid() != wasKeyInvalid) {
            emit isApiKeyInvalidChanged();
        }
        if (isRateLimited() != wasRateLimited) {
            emit isRateLimitedChanged();
        }
    }
}

void AbstractCloudSttClient::setRetrySecondsRemaining(int seconds) {
    if (m_retrySecondsRemaining != seconds) {
        const bool wasRateLimited = isRateLimited();
        m_retrySecondsRemaining = seconds;
        emit retrySecondsRemainingChanged();
        if (isRateLimited() != wasRateLimited) {
            emit isRateLimitedChanged();
        }
    }
}

QString AbstractCloudSttClient::selectedModel() const {
    return m_selectedModel;
}

void AbstractCloudSttClient::setSelectedModel(const QString& model) {
    QString trimmed = model.trimmed();
    if (trimmed.isEmpty()) {
        trimmed = defaultModel();
    }
    if (m_selectedModel != trimmed) {
        m_selectedModel = trimmed;
        const QString key = settingsModelKey();
        if (!key.isEmpty()) {
            QSettings settings;
            settings.setValue(key, m_selectedModel);
        }
        emit selectedModelChanged();
    }
}

void AbstractCloudSttClient::cancel() {
    const bool wasBusy = m_requestRunner.isBusy();
    if (m_retryTimer) {
        m_retryTimer->stop();
    }
    m_requestRunner.cancel();
    m_retryCountdownTimer->stop();
    setRetrySecondsRemaining(0);
    m_lastWavData.clear();
    if (wasBusy) {
        emit busyChanged();
    }
}

void AbstractCloudSttClient::retryLast() {
    if (!m_lastWavData.isEmpty() && !isBusy()) {
        if (m_retryTimer) {
            m_retryTimer->stop();
        }
        m_requestRunner.prepareNewRequest();
        sendTranscribeRequest();
    }
}

void AbstractCloudSttClient::transcribe(const QByteArray& wavData) {
    if (isBusy()) {
        qCDebug(lcNetwork) << providerDisplayName() << "STT: transcribe ignored — request already in progress";
        setLastError(u"A transcription request is already in progress"_s, ErrorCategory::GeneralError);
        return;
    }

    if (!m_apiClient || !m_apiClient->apiKeySet()) {
        qWarning() << providerDisplayName() << "STT: Attempted transcribe without an API key";
        setLastError(u"%1 API key is not set"_s.arg(providerDisplayName()), ErrorCategory::InvalidApiKey);
        emit errorOccurred(m_lastError);
        return;
    }

    if (wavData.isEmpty()) {
        qWarning() << providerDisplayName() << "STT: Attempted transcribe with empty audio data";
        setLastError(u"No audio data to transcribe"_s, ErrorCategory::GeneralError);
        emit errorOccurred(m_lastError);
        return;
    }

    if (m_retryTimer) {
        m_retryTimer->stop();
    }
    m_requestRunner.prepareNewRequest();
    m_lastWavData = wavData;

    sendTranscribeRequest();
}

void AbstractCloudSttClient::sendTranscribeRequest() {
    if (!m_apiClient || !m_apiClient->apiKeySet() || m_lastWavData.isEmpty() || m_requestRunner.isCancelled()) {
        setBusy(false);
        return;
    }

    setBusy(true);
    setLastError({});

    auto* reply = buildAndSendRequest(m_lastWavData);
    m_requestRunner.setCurrentReply(reply);
}

AbstractCloudSttClient::ErrorCategory AbstractCloudSttClient::classifyError(const CloudApiResponse& res,
                                                                            QString& outMessage) const {
    ErrorCategory cat = ErrorCategory::GeneralError;
    outMessage = res.errorMessage;

    if (res.httpStatus == 401 || res.httpStatus == 403 ||
        outMessage.contains(u"API_KEY_INVALID"_s, Qt::CaseInsensitive) ||
        outMessage.contains(u"Invalid API Key"_s, Qt::CaseInsensitive) ||
        outMessage.contains(u"API key not valid"_s, Qt::CaseInsensitive)) {
        cat = ErrorCategory::InvalidApiKey;
        outMessage = u"Invalid API Key. Please check your %1 API key in Settings."_s.arg(providerDisplayName());
    } else if (res.isRateLimited) {
        cat = ErrorCategory::RateLimited;
    } else if (res.networkError == QNetworkReply::HostNotFoundError ||
               res.networkError == QNetworkReply::ConnectionRefusedError ||
               res.networkError == QNetworkReply::TimeoutError ||
               res.networkError == QNetworkReply::NetworkSessionFailedError) {
        cat = ErrorCategory::NetworkOffline;
        outMessage = u"No internet connection. Please check your network and try again."_s;
    }

    return cat;
}

void AbstractCloudSttClient::handleApiResponse(const CloudApiResponse& res) {
    m_requestRunner.setCurrentReply(nullptr);

    qCDebug(lcNetwork) << providerDisplayName() << "STT HTTP response received -> Status:" << res.httpStatus
                       << "Elapsed time:" << res.latencyMs << "ms";

    if (m_requestRunner.isCancelled() || res.networkError == QNetworkReply::OperationCanceledError) {
        qCDebug(lcNetwork) << providerDisplayName() << "STT: Request cancelled/aborted, ignoring response";
        if (m_retryTimer) {
            m_retryTimer->stop();
        }
        setBusy(false);
        return;
    }

    if (!res.isSuccess) {
        if (m_requestRunner.shouldRetry(res) && !m_lastWavData.isEmpty()) {
            m_requestRunner.incrementRetryCount();
            const int delayMs = m_requestRunner.calculateRetryDelayMs(res);
            qCDebug(lcNetwork) << providerDisplayName() << "STT: Transient error encountered (Status:" << res.httpStatus
                               << "Error:" << res.networkError << "). Scheduling retry in" << delayMs << "ms (Attempt"
                               << m_requestRunner.retryCount() << "/" << m_requestRunner.policy().maxRetries << ")";
            if (m_retryTimer) {
                m_retryTimer->start(delayMs);
            }
            return;
        }

        if (m_retryTimer) {
            m_retryTimer->stop();
        }
        m_requestRunner.reset();
        setBusy(false);

        QString errorText;
        ErrorCategory cat = classifyError(res, errorText);

        if (cat == ErrorCategory::RateLimited && res.retryAfterSeconds > 0) {
            setRetrySecondsRemaining(res.retryAfterSeconds);
            m_retryCountdownTimer->start();
        }

        qWarning() << providerDisplayName() << "STT error:" << errorText << "Category:" << static_cast<int>(cat)
                   << "Raw body:" << res.rawBody;
        setLastError(errorText, cat);
        emit errorOccurred(errorText);
        return;
    }

    if (m_retryTimer) {
        m_retryTimer->stop();
    }
    m_requestRunner.reset();
    m_lastWavData.clear();
    m_retryCountdownTimer->stop();
    setRetrySecondsRemaining(0);
    setBusy(false);

    const QString text = extractTranscribedText(res);
    if (text.isEmpty()) {
        qWarning() << providerDisplayName() << "STT: Empty transcription text in response";
        const QString err = u"%1 API returned empty transcription"_s.arg(providerDisplayName());
        setLastError(err, ErrorCategory::GeneralError);
        emit errorOccurred(err);
        return;
    }

    qCDebug(lcNetwork) << providerDisplayName() << "STT: Transcription successfully received -> Length:" << text.size()
                       << "chars";

    setLastError({}, ErrorCategory::None);
    emit transcriptionReady(text);
}

void AbstractCloudSttClient::setLastError(const QString& error, ErrorCategory category) {
    if (m_lastError != error) {
        m_lastError = error;
        emit lastErrorChanged();
    }
    setErrorCategory(category);
}
