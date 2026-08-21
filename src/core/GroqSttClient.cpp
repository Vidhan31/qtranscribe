#include "GroqSttClient.h"

#include "GroqApiClient.h"
#include "LoggingCategories.h"

#include <QHttpMultiPart>
#include <QHttpPart>
#include <QSettings>
#include <QTimer>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

GroqSttClient::GroqSttClient(QObject* parent)
    : AbstractSttClient(parent)
    , m_retryCountdownTimer(new QTimer(this))
    , m_retryTimer(new QTimer(this)) {
    QSettings settings;
    m_selectedModel = settings.value(u"Groq/Model"_s, kDefaultModel.toString()).toString();
    if (m_selectedModel.isEmpty() ||
        (m_selectedModel != u"whisper-large-v3-turbo"_s && m_selectedModel != u"whisper-large-v3"_s)) {
        m_selectedModel = kDefaultModel.toString();
    }
    m_language = settings.value(u"Groq/Language"_s, QString()).toString();
    m_customPrompt = settings.value(u"Groq/CustomPrompt"_s, QString()).toString();

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

void GroqSttClient::setApiClient(GroqApiClient* apiClient) {
    if (m_apiClient == apiClient) {
        return;
    }
    if (m_apiClient) {
        disconnect(m_apiClient, &GroqApiClient::apiKeySetChanged, this, &GroqSttClient::onApiKeySetChanged);
    }
    m_apiClient = apiClient;
    if (m_apiClient) {
        connect(m_apiClient, &GroqApiClient::apiKeySetChanged, this, &GroqSttClient::onApiKeySetChanged);
    }
    emit readyChanged();
}

GroqApiClient* GroqSttClient::apiClient() const {
    return m_apiClient;
}

void GroqSttClient::onApiKeySetChanged() {
    emit readyChanged();
}

void GroqSttClient::activate() {
    emit readyChanged();
}

void GroqSttClient::deactivate() {
    cancel();
}

bool GroqSttClient::isReady() const {
    return m_apiClient && m_apiClient->apiKeySet();
}

bool GroqSttClient::isBusy() const {
    return m_requestRunner.isBusy();
}

bool GroqSttClient::isCancelled() const {
    return m_requestRunner.isCancelled();
}

void GroqSttClient::setBusy(bool busy) {
    if (m_requestRunner.isBusy() != busy) {
        m_requestRunner.setBusy(busy);
        emit busyChanged();
    }
}

QString GroqSttClient::lastError() const {
    return m_lastError;
}

GroqSttClient::ErrorCategory GroqSttClient::errorCategory() const {
    return m_errorCategory;
}

int GroqSttClient::retrySecondsRemaining() const {
    return m_retrySecondsRemaining;
}

void GroqSttClient::setErrorCategory(ErrorCategory category) {
    if (m_errorCategory != category) {
        m_errorCategory = category;
        emit errorCategoryChanged();
    }
}

void GroqSttClient::setRetrySecondsRemaining(int seconds) {
    if (m_retrySecondsRemaining != seconds) {
        m_retrySecondsRemaining = seconds;
        emit retrySecondsRemainingChanged();
    }
}

QString GroqSttClient::selectedModel() const {
    return m_selectedModel;
}

void GroqSttClient::setSelectedModel(const QString& model) {
    QString trimmed = model.trimmed();
    if (trimmed.isEmpty()) {
        trimmed = kDefaultModel.toString();
    }
    if (m_selectedModel != trimmed) {
        m_selectedModel = trimmed;
        QSettings settings;
        settings.setValue(u"Groq/Model"_s, m_selectedModel);
        emit selectedModelChanged();
    }
}

QString GroqSttClient::language() const {
    return m_language;
}

void GroqSttClient::setLanguage(const QString& lang) {
    QString trimmed = lang.trimmed();
    if (m_language != trimmed) {
        m_language = trimmed;
        QSettings settings;
        settings.setValue(u"Groq/Language"_s, m_language);
        emit languageChanged();
    }
}

QString GroqSttClient::customPrompt() const {
    return m_customPrompt;
}

void GroqSttClient::setCustomPrompt(const QString& prompt) {
    if (m_customPrompt != prompt) {
        m_customPrompt = prompt;
        QSettings settings;
        settings.setValue(u"Groq/CustomPrompt"_s, m_customPrompt);
        emit customPromptChanged();
    }
}

void GroqSttClient::cancel() {
    const bool wasBusy = m_requestRunner.isBusy();
    if (m_retryTimer) {
        m_retryTimer->stop();
    }
    m_requestRunner.cancel();
    m_retryCountdownTimer->stop();
    setRetrySecondsRemaining(0);
    m_lastWavData.clear();
    m_lastFilename.clear();
    if (wasBusy) {
        emit busyChanged();
    }
}

void GroqSttClient::retryLast() {
    if (!m_lastWavData.isEmpty() && !isBusy()) {
        if (m_retryTimer) {
            m_retryTimer->stop();
        }
        m_requestRunner.prepareNewRequest();
        sendTranscribeRequest();
    }
}

void GroqSttClient::transcribe(const QByteArray& wavData) {
    transcribe(wavData, u"audio.wav"_s);
}

void GroqSttClient::transcribe(const QByteArray& wavData, const QString& filename) {
    if (isBusy()) {
        qCDebug(lcNetwork) << "GroqSttClient: transcribe ignored — request already in progress";
        setLastError(u"A transcription request is already in progress"_s, ErrorCategory::GeneralError);
        return;
    }

    if (!m_apiClient || !m_apiClient->apiKeySet()) {
        qWarning() << "GroqSttClient: Attempted transcribe without an API key";
        setLastError(u"Groq API key is not set"_s, ErrorCategory::InvalidApiKey);
        emit errorOccurred(m_lastError);
        return;
    }

    if (wavData.isEmpty()) {
        qWarning() << "GroqSttClient: Attempted transcribe with empty audio data";
        setLastError(u"No audio data to transcribe"_s, ErrorCategory::GeneralError);
        emit errorOccurred(m_lastError);
        return;
    }

    if (m_retryTimer) {
        m_retryTimer->stop();
    }
    m_requestRunner.prepareNewRequest();
    m_lastWavData = wavData;
    m_lastFilename = filename;

    sendTranscribeRequest();
}

void GroqSttClient::sendTranscribeRequest() {
    if (!m_apiClient || !m_apiClient->apiKeySet() || m_lastWavData.isEmpty() || m_requestRunner.isCancelled()) {
        setBusy(false);
        return;
    }

    setBusy(true);
    setLastError({});

    QString modelToUse = m_selectedModel.trimmed().isEmpty() ? kDefaultModel.toString() : m_selectedModel.trimmed();
    QString langToUse = m_language.trimmed();
    QString promptToUse = m_customPrompt.trimmed();
    QString filenameToUse = m_lastFilename.isEmpty() ? u"audio.wav"_s : m_lastFilename;

    qCDebug(lcNetwork) << "Preparing Groq STT multipart request -> Model:" << modelToUse
                       << "Language:" << (langToUse.isEmpty() ? u"Auto-detect"_s : langToUse)
                       << "Prompt set:" << (!promptToUse.isEmpty()) << "Filename:" << filenameToUse
                       << "Audio payload size:" << m_lastWavData.size() << "bytes"
                       << "(Retry attempt:" << m_requestRunner.retryCount() << ")";

    auto* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart modelPart;
    modelPart.setHeader(QNetworkRequest::ContentDispositionHeader, u"form-data; name=\"model\""_s);
    modelPart.setBody(modelToUse.toUtf8());
    multiPart->append(modelPart);

    QHttpPart formatPart;
    formatPart.setHeader(QNetworkRequest::ContentDispositionHeader, u"form-data; name=\"response_format\""_s);
    formatPart.setBody("json");
    multiPart->append(formatPart);

    if (!langToUse.isEmpty()) {
        QHttpPart langPart;
        langPart.setHeader(QNetworkRequest::ContentDispositionHeader, u"form-data; name=\"language\""_s);
        langPart.setBody(langToUse.toUtf8());
        multiPart->append(langPart);
    }

    if (!promptToUse.isEmpty()) {
        QHttpPart promptPart;
        promptPart.setHeader(QNetworkRequest::ContentDispositionHeader, u"form-data; name=\"prompt\""_s);
        promptPart.setBody(promptToUse.toUtf8());
        multiPart->append(promptPart);
    }

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       u"form-data; name=\"file\"; filename=\"%1\""_s.arg(filenameToUse));
    filePart.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, u"audio/wav"_s);
    filePart.setBody(m_lastWavData);
    multiPart->append(filePart);

    qCDebug(lcNetwork) << "Posting STT request via GroqApiClient";

    auto* reply =
        m_apiClient->postMultipart(u"audio/transcriptions"_s, multiPart, u"Speech-to-Text (Whisper)"_s, modelToUse,
                                   [this](const GroqApiResponse& res) { handleTranscribeResponse(res); });
    m_requestRunner.setCurrentReply(reply);
}

GroqSttClient::ErrorCategory GroqSttClient::classifyError(const GroqApiResponse& res, QString& outMessage) const {
    ErrorCategory cat = ErrorCategory::GeneralError;
    outMessage = res.errorMessage;

    if (res.httpStatus == 401 || outMessage.contains(u"Invalid API Key"_s, Qt::CaseInsensitive)) {
        cat = ErrorCategory::InvalidApiKey;
        outMessage = u"Invalid API Key. Please check your Groq API key in Settings."_s;
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

void GroqSttClient::handleTranscribeResponse(const GroqApiResponse& res) {
    m_requestRunner.setCurrentReply(nullptr);

    qCDebug(lcNetwork) << "Groq STT HTTP response received -> Status:" << res.httpStatus
                       << "Elapsed time:" << res.latencyMs << "ms";

    if (m_requestRunner.isCancelled() || res.networkError == QNetworkReply::OperationCanceledError) {
        qCDebug(lcNetwork) << "GroqSttClient: Request cancelled/aborted, ignoring response";
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
            qCDebug(lcNetwork) << "GroqSttClient: Transient error encountered (Status:" << res.httpStatus
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

        qWarning() << "GroqSttClient error:" << errorText << "Category:" << static_cast<int>(cat)
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
    m_lastFilename.clear();
    m_retryCountdownTimer->stop();
    setRetrySecondsRemaining(0);
    setBusy(false);

    const QString text = res.json.value(u"text"_s).toString();
    if (text.isEmpty()) {
        qWarning() << "GroqSttClient: Empty 'text' field in Groq STT response";
        const QString err = u"Groq API returned empty transcription"_s;
        setLastError(err, ErrorCategory::GeneralError);
        emit errorOccurred(err);
        return;
    }

    qCDebug(lcNetwork) << "Transcription successfully received -> Length:" << text.size() << "chars";

    setLastError({}, ErrorCategory::None);
    emit transcriptionReady(text);
}

void GroqSttClient::setLastError(const QString& error, ErrorCategory category) {
    if (m_lastError != error) {
        m_lastError = error;
        emit lastErrorChanged();
    }
    if (m_errorCategory != category) {
        m_errorCategory = category;
        emit errorCategoryChanged();
    }
}
