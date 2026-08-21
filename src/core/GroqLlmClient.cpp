#include "GroqLlmClient.h"

#include "GroqApiClient.h"
#include "LoggingCategories.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTimer>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

GroqLlmClient::GroqLlmClient(QObject* parent)
    : QObject(parent)
    , m_retryTimer(new QTimer(this)) {
    QSettings settings;
    m_enabled = settings.value(u"Groq/LlmEnabled"_s, false).toBool();
    m_selectedModel = settings.value(u"Groq/LlmModel"_s, kDefaultModel.toString()).toString();
    if (m_selectedModel.isEmpty() || m_selectedModel == u"llama-3.1-8b-instant"_s) {
        m_selectedModel = kDefaultModel.toString();
    }
    m_activePreset = settings.value(u"Groq/LlmPreset"_s, u"grammar"_s).toString();
    if (m_activePreset.isEmpty()) {
        m_activePreset = u"grammar"_s;
    }
    m_customPrompt = settings.value(u"Groq/LlmCustomPrompt"_s, QString()).toString();
    m_temperature = settings.value(u"Groq/LlmTemperature"_s, 0.1).toDouble();

    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, [this]() {
        if (!m_requestRunner.isCancelled() && !m_pendingRawText.isEmpty()) {
            sendProcessRequest();
        }
    });
}

void GroqLlmClient::setApiClient(GroqApiClient* apiClient) {
    m_apiClient = apiClient;
}

GroqApiClient* GroqLlmClient::apiClient() const {
    return m_apiClient;
}

bool GroqLlmClient::isBusy() const {
    return m_requestRunner.isBusy();
}

bool GroqLlmClient::isCancelled() const {
    return m_requestRunner.isCancelled();
}

void GroqLlmClient::setBusy(bool busy) {
    if (m_requestRunner.isBusy() != busy) {
        m_requestRunner.setBusy(busy);
        emit busyChanged();
    }
}

bool GroqLlmClient::enabled() const {
    return m_enabled;
}

void GroqLlmClient::setEnabled(bool enabled) {
    if (m_enabled != enabled) {
        m_enabled = enabled;
        QSettings settings;
        settings.setValue(u"Groq/LlmEnabled"_s, m_enabled);
        qCDebug(lcLLM) << "LLM post-processing enabled state changed to:" << m_enabled;
        emit enabledChanged();
    }
}

QString GroqLlmClient::lastError() const {
    return m_lastError;
}

QString GroqLlmClient::selectedModel() const {
    return m_selectedModel;
}

void GroqLlmClient::setSelectedModel(const QString& model) {
    QString trimmed = model.trimmed();
    if (trimmed.isEmpty()) {
        trimmed = kDefaultModel.toString();
    }
    if (m_selectedModel != trimmed) {
        m_selectedModel = trimmed;
        QSettings settings;
        settings.setValue(u"Groq/LlmModel"_s, m_selectedModel);
        qCDebug(lcLLM) << "LLM model set to:" << m_selectedModel;
        emit selectedModelChanged();
    }
}

QString GroqLlmClient::activePreset() const {
    return m_activePreset;
}

void GroqLlmClient::setActivePreset(const QString& preset) {
    QString trimmed = preset.trimmed();
    if (trimmed.isEmpty()) {
        trimmed = u"grammar"_s;
    }
    if (m_activePreset != trimmed) {
        m_activePreset = trimmed;
        QSettings settings;
        settings.setValue(u"Groq/LlmPreset"_s, m_activePreset);
        qCDebug(lcLLM) << "Active LLM preset changed to:" << m_activePreset;
        emit activePresetChanged();
    }
}

QString GroqLlmClient::customPrompt() const {
    return m_customPrompt;
}

void GroqLlmClient::setCustomPrompt(const QString& prompt) {
    if (m_customPrompt != prompt) {
        m_customPrompt = prompt;
        QSettings settings;
        settings.setValue(u"Groq/LlmCustomPrompt"_s, m_customPrompt);
        qCDebug(lcLLM) << "Custom LLM prompt updated";
        emit customPromptChanged();
    }
}

double GroqLlmClient::temperature() const {
    return m_temperature;
}

void GroqLlmClient::setTemperature(double temp) {
    temp = std::clamp(temp, 0.0, 1.0);
    if (!qFuzzyCompare(m_temperature, temp)) {
        m_temperature = temp;
        QSettings settings;
        settings.setValue(u"Groq/LlmTemperature"_s, m_temperature);
        emit temperatureChanged();
    }
}

QString GroqLlmClient::systemPromptForPreset(const QString& preset) const {
    if (preset == u"bullets"_s) {
        return u"You are an expert speech-to-text post-processor. Your task is to organize and format the spoken transcription into clean markdown bullet points or numbered steps if sequential items are dictated. Group related points concisely. Output ONLY the formatted markdown list. Do NOT add any preamble, conversational remarks, or closing commentary."_s;
    }
    if (preset == u"professional"_s) {
        return u"You are an expert executive communication assistant. Your task is to polish the dictated speech into crisp, clear, and professional text suitable for emails, messages, or documents. Remove vocal filler words (e.g., um, uh, like), fix all grammar and punctuation, and enhance flow while preserving the core message. Output ONLY the polished text with NO preamble, quotes, or conversational commentary."_s;
    }
    if (preset == u"custom"_s) {
        if (!m_customPrompt.trimmed().isEmpty()) {
            return m_customPrompt;
        }
        return u"You are a helpful speech post-processor. Clean up the transcribed speech while preserving its original meaning. Output ONLY the processed text with no extra conversational commentary."_s;
    }
    return u"You are an expert speech-to-text post-processor. Your task is to fix spelling errors, speech recognition typos, homophone mistakes, missing punctuation, and capitalization in the transcribed text while strictly preserving the original meaning, intent, vocabulary, and tone. Output ONLY the corrected text. Do NOT wrap in quotes, do NOT add explanations, do NOT add introductory or concluding remarks."_s;
}

QString GroqLlmClient::currentSystemPrompt() const {
    return systemPromptForPreset(m_activePreset);
}

void GroqLlmClient::cancel() {
    const bool wasBusy = m_requestRunner.isBusy();
    if (m_retryTimer) {
        m_retryTimer->stop();
    }
    m_requestRunner.cancel();
    m_pendingRawText.clear();
    if (wasBusy) {
        emit busyChanged();
    }
}

void GroqLlmClient::processText(const QString& rawText) {
    if (isBusy()) {
        qCDebug(lcLLM) << "processText ignored: request already in progress";
        setLastError(u"An LLM post-processing request is already in progress"_s);
        emit errorOccurred(m_lastError, rawText);
        return;
    }

    if (!m_apiClient || !m_apiClient->apiKeySet()) {
        qWarning() << "GroqLlmClient: API key not set, falling back to raw transcription";
        setLastError(u"Groq API key is not set"_s);
        emit errorOccurred(m_lastError, rawText);
        return;
    }

    if (rawText.trimmed().isEmpty()) {
        qCDebug(lcLLM) << "GroqLlmClient: Empty input text, emitting ready directly";
        emit enhancementReady(rawText);
        return;
    }

    if (m_retryTimer) {
        m_retryTimer->stop();
    }
    m_requestRunner.prepareNewRequest();
    m_pendingRawText = rawText;

    sendProcessRequest();
}

void GroqLlmClient::sendProcessRequest() {
    if (!m_apiClient || !m_apiClient->apiKeySet() || m_pendingRawText.trimmed().isEmpty() ||
        m_requestRunner.isCancelled()) {
        setBusy(false);
        return;
    }

    setBusy(true);
    setLastError({});

    QString modelToUse = m_selectedModel.trimmed().isEmpty() ? kDefaultModel.toString() : m_selectedModel.trimmed();
    QString systemPrompt = currentSystemPrompt();

    qCDebug(lcLLM) << "Dispatching Groq LLM completion request -> Model:" << modelToUse << "Preset:" << m_activePreset
                   << "Input length:" << m_pendingRawText.size() << "chars"
                   << "(Retry attempt:" << m_requestRunner.retryCount() << ")";

    QJsonObject rootObj;
    rootObj[u"model"_s] = modelToUse;

    QJsonArray messages;
    QJsonObject sysMsg;
    sysMsg[u"role"_s] = u"system"_s;
    sysMsg[u"content"_s] = systemPrompt;
    messages.append(sysMsg);

    QJsonObject userMsg;
    userMsg[u"role"_s] = u"user"_s;
    userMsg[u"content"_s] = m_pendingRawText;
    messages.append(userMsg);

    rootObj[u"messages"_s] = messages;
    rootObj[u"temperature"_s] = m_temperature;
    rootObj[u"max_completion_tokens"_s] = 2048;
    rootObj[u"reasoning_format"_s] = u"hidden"_s;
    rootObj[u"stream"_s] = false;

    auto* reply = m_apiClient->postJson(u"chat/completions"_s, rootObj, u"LLM Post-Processing"_s, modelToUse,
                                        [this](const GroqApiResponse& res) { handleProcessResponse(res); });
    m_requestRunner.setCurrentReply(reply);
}

void GroqLlmClient::handleProcessResponse(const GroqApiResponse& res) {
    m_requestRunner.setCurrentReply(nullptr);

    qCDebug(lcLLM) << "Groq LLM HTTP response received -> Status:" << res.httpStatus << "Elapsed time:" << res.latencyMs
                   << "ms";

    if (m_requestRunner.isCancelled() || res.networkError == QNetworkReply::OperationCanceledError) {
        qCDebug(lcLLM) << "GroqLlmClient: Request cancelled/aborted, ignoring response";
        if (m_retryTimer) {
            m_retryTimer->stop();
        }
        setBusy(false);
        return;
    }

    if (!res.isSuccess) {
        if (m_requestRunner.shouldRetry(res) && !m_pendingRawText.isEmpty()) {
            m_requestRunner.incrementRetryCount();
            const int delayMs = m_requestRunner.calculateRetryDelayMs(res);
            qCDebug(lcLLM) << "GroqLlmClient: Transient error encountered (Status:" << res.httpStatus
                           << "Error:" << res.networkError << "). Scheduling retry in" << delayMs << "ms (Attempt"
                           << m_requestRunner.retryCount() << "/" << m_requestRunner.policy().maxRetries << ")";
            if (m_retryTimer) {
                m_retryTimer->start(delayMs);
            }
            return;
        }

        const QString rawFallback = m_pendingRawText;
        if (m_retryTimer) {
            m_retryTimer->stop();
        }
        m_requestRunner.reset();
        m_pendingRawText.clear();
        setBusy(false);

        const QString errorText = res.errorMessage;
        qWarning() << "GroqLlmClient error:" << errorText << "Raw body:" << res.rawBody;
        setLastError(errorText);
        emit errorOccurred(errorText, rawFallback);
        return;
    }

    const QString rawFallback = m_pendingRawText;
    if (m_retryTimer) {
        m_retryTimer->stop();
    }
    m_requestRunner.reset();
    m_pendingRawText.clear();
    setBusy(false);

    const QJsonArray choices = res.json.value(u"choices"_s).toArray();
    if (choices.isEmpty()) {
        qWarning() << "GroqLlmClient: No choices returned in LLM completion";
        const QString err = u"Empty completion choices from Groq LLM API"_s;
        setLastError(err);
        emit errorOccurred(err, rawFallback);
        return;
    }

    const QJsonObject choice0 = choices.first().toObject();
    const QJsonObject message = choice0.value(u"message"_s).toObject();
    QString enhancedText = message.value(u"content"_s).toString().trimmed();

    if (enhancedText.isEmpty()) {
        qWarning() << "GroqLlmClient: LLM returned empty text content, using fallback";
        enhancedText = rawFallback;
    }

    qCDebug(lcLLM) << "LLM enhancement succeeded -> Length:" << enhancedText.size() << "chars";

    setLastError({});
    emit enhancementReady(enhancedText);
}

void GroqLlmClient::setLastError(const QString& error) {
    if (m_lastError != error) {
        m_lastError = error;
        emit lastErrorChanged();
    }
}
