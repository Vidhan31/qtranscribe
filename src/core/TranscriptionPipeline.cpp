#include "TranscriptionPipeline.h"

#include "AudioRecorder.h"
#include "GroqLlmClient.h"
#include "LoggingCategories.h"

#include <QDebug>
#include <QSettings>
#include <QTimer>

using namespace Qt::StringLiterals;

TranscriptionPipeline::TranscriptionPipeline(QObject* parent)
    : QObject(parent)
    , m_maxDurationNoticeTimer(new QTimer(this)) {
    m_maxDurationNoticeTimer->setInterval(6000);
    m_maxDurationNoticeTimer->setSingleShot(true);
    connect(m_maxDurationNoticeTimer, &QTimer::timeout, this, [this]() {
        m_showMaxDurationNotice = false;
        emit activeNoticeChanged();
    });

    QSettings settings;
    const QString backendStr = settings.value(u"Speech/Backend"_s, u"Groq"_s).toString();
    if (backendStr == u"WhisperCpp"_s) {
        m_activeBackend = Backend::WhisperCpp;
    } else {
        m_activeBackend = Backend::Groq;
    }

    setStatusMessage(tr("Ready"));
    qCDebug(lcSpeech) << "TranscriptionPipeline deep module constructed. Initial backend:"
                      << (m_activeBackend == Backend::WhisperCpp ? "WhisperCpp" : "Groq");
}

void TranscriptionPipeline::setAudioRecorder(AudioRecorder* recorder) {
    if (m_recorder == recorder) {
        return;
    }

    if (m_recorder) {
        disconnect(m_recorder, &AudioRecorder::recordingFinished, this, &TranscriptionPipeline::onRecordingFinished);
        disconnect(m_recorder, &AudioRecorder::maxDurationReached, this, &TranscriptionPipeline::onMaxDurationReached);
        disconnect(m_recorder, &AudioRecorder::hasAudioInputDeviceChanged, this,
                   &TranscriptionPipeline::updatePipelineHealth);
        disconnect(m_recorder, &AudioRecorder::audioLevelChanged, this, nullptr);
    }

    m_recorder = recorder;

    if (m_recorder) {
        connect(m_recorder, &AudioRecorder::recordingFinished, this, &TranscriptionPipeline::onRecordingFinished);
        connect(m_recorder, &AudioRecorder::maxDurationReached, this, &TranscriptionPipeline::onMaxDurationReached);
        connect(m_recorder, &AudioRecorder::hasAudioInputDeviceChanged, this,
                &TranscriptionPipeline::updatePipelineHealth);
        connect(m_recorder, &AudioRecorder::audioLevelChanged, this, [this]() {
            if (m_recorder) {
                emit audioLevelChanged(m_recorder->audioLevel());
            }
        });
    }

    updatePipelineHealth();
}

void TranscriptionPipeline::registerBackend(Backend backend, AbstractSttClient* client) {
    if (m_sttClients.value(backend) == client) {
        return;
    }

    if (auto* old = m_sttClients.value(backend)) {
        disconnect(old, &AbstractSttClient::transcriptionReady, this, &TranscriptionPipeline::onSttTranscriptionReady);
        disconnect(old, &AbstractSttClient::errorOccurred, this, &TranscriptionPipeline::onSttError);
        disconnect(old, &AbstractSttClient::readyChanged, this, &TranscriptionPipeline::updatePipelineHealth);
        disconnect(old, &AbstractSttClient::busyChanged, this, &TranscriptionPipeline::updatePipelineHealth);
        disconnect(old, &AbstractSttClient::noticeChanged, this, &TranscriptionPipeline::activeNoticeChanged);
    }

    if (client) {
        m_sttClients.insert(backend, client);
        connect(client, &AbstractSttClient::transcriptionReady, this, &TranscriptionPipeline::onSttTranscriptionReady);
        connect(client, &AbstractSttClient::errorOccurred, this, &TranscriptionPipeline::onSttError);
        connect(client, &AbstractSttClient::readyChanged, this, &TranscriptionPipeline::updatePipelineHealth);
        connect(client, &AbstractSttClient::busyChanged, this, &TranscriptionPipeline::updatePipelineHealth);
        connect(client, &AbstractSttClient::noticeChanged, this, &TranscriptionPipeline::activeNoticeChanged);

        if (backend == m_activeBackend && m_initialized) {
            client->activate();
        }
    } else {
        m_sttClients.remove(backend);
    }

    updatePipelineHealth();
}

void TranscriptionPipeline::setLlmClient(GroqLlmClient* llmClient) {
    if (m_llmClient == llmClient) {
        return;
    }

    if (m_llmClient) {
        disconnect(m_llmClient, &GroqLlmClient::enhancementReady, this, &TranscriptionPipeline::onLlmEnhancementReady);
        disconnect(m_llmClient, &GroqLlmClient::errorOccurred, this, &TranscriptionPipeline::onLlmError);
    }

    m_llmClient = llmClient;

    if (m_llmClient) {
        connect(m_llmClient, &GroqLlmClient::enhancementReady, this, &TranscriptionPipeline::onLlmEnhancementReady);
        connect(m_llmClient, &GroqLlmClient::errorOccurred, this, &TranscriptionPipeline::onLlmError);
    }
}

TranscriptionPipeline::State TranscriptionPipeline::state() const {
    return m_state;
}

TranscriptionPipeline::Backend TranscriptionPipeline::activeBackend() const {
    return m_activeBackend;
}

void TranscriptionPipeline::setActiveBackend(Backend backend) {
    if (m_activeBackend != backend) {
        if (isBusy()) {
            cancel();
        }
        if (auto* oldClient = activeSttClient()) {
            oldClient->deactivate();
        }

        m_activeBackend = backend;
        QSettings settings;
        settings.setValue(u"Speech/Backend"_s, m_activeBackend == Backend::WhisperCpp ? u"WhisperCpp"_s : u"Groq"_s);

        if (auto* newClient = activeSttClient()) {
            newClient->activate();
        }

        emit activeBackendChanged();
        updatePipelineHealth();
    }
}

AbstractSttClient* TranscriptionPipeline::activeSttClient() const {
    return m_sttClients.value(m_activeBackend, nullptr);
}

bool TranscriptionPipeline::isBusy() const {
    return m_state == State::Recording || m_state == State::Transcribing || m_state == State::Enhancing;
}

bool TranscriptionPipeline::canRecord() const {
    const bool micReady = m_recorder && m_recorder->hasAudioInputDevice();
    const bool notProcessing = (m_state != State::Transcribing && m_state != State::Enhancing);
    const bool sttReady = activeSttClient() && activeSttClient()->isReady();
    return micReady && notProcessing && sttReady;
}

qreal TranscriptionPipeline::audioLevel() const {
    return m_recorder ? m_recorder->audioLevel() : 0.0;
}

QString TranscriptionPipeline::statusMessage() const {
    return m_statusMessage;
}

QString TranscriptionPipeline::lastError() const {
    return m_lastError;
}

QString TranscriptionPipeline::lastTranscription() const {
    return m_lastTranscription;
}

void TranscriptionPipeline::initialize() {
    if (m_initialized) {
        return;
    }
    m_initialized = true;
    if (auto* client = activeSttClient()) {
        client->activate();
    }
    updatePipelineHealth();
    qCDebug(lcSpeech) << "TranscriptionPipeline initialized with registered STT engines";
}

void TranscriptionPipeline::startRecording() {
    if (isBusy()) {
        qCDebug(lcSpeech) << "TranscriptionPipeline: startRecording ignored — already busy";
        return;
    }

    if (!m_recorder || !m_recorder->hasAudioInputDevice()) {
        qWarning() << "TranscriptionPipeline: No microphone input device available";
        setLastError(tr("No microphone found. Please connect a microphone."));
        setStatusMessage(tr("No microphone found"));
        setState(State::Error);
        updatePipelineHealth();
        return;
    }

    setLastError({});
    setState(State::Recording);
    setStatusMessage(tr("Listening…"));
    m_recorder->startRecording();
    updatePipelineHealth();
}

void TranscriptionPipeline::stopRecording() {
    if (m_state != State::Recording) {
        qCDebug(lcSpeech) << "TranscriptionPipeline: stopRecording ignored — not currently recording";
        return;
    }

    if (m_recorder) {
        m_recorder->stopRecording();
    }
}

void TranscriptionPipeline::toggleRecording() {
    if (m_state == State::Recording) {
        stopRecording();
    } else if (m_state == State::Idle || m_state == State::Error) {
        startRecording();
    } else {
        qCDebug(lcSpeech) << "TranscriptionPipeline: Toggle ignored while busy (state:" << static_cast<int>(m_state)
                          << ")";
    }
}

void TranscriptionPipeline::cancel() {
    if (m_state == State::Recording && m_recorder) {
        m_recorder->stopRecording();
    }
    if (auto* stt = activeSttClient()) {
        stt->cancel();
    }
    if (m_llmClient) {
        m_llmClient->cancel();
    }

    setState(State::Idle);
    setStatusMessage(tr("Ready"));
    updatePipelineHealth();
}

void TranscriptionPipeline::retry() {
    if (isBusy()) {
        return;
    }

    auto* stt = activeSttClient();
    if (!stt) {
        return;
    }

    setLastError({});
    setState(State::Transcribing);
    setStatusMessage(tr("Retrying transcription…"));
    updatePipelineHealth();

    if (!m_lastWavData.isEmpty()) {
        stt->transcribe(m_lastWavData);
    } else {
        stt->retryLast();
    }
}

void TranscriptionPipeline::clearError() {
    if (m_state == State::Error) {
        setLastError({});
        setState(State::Idle);
        setStatusMessage(tr("Ready"));
        updatePipelineHealth();
    }
}

void TranscriptionPipeline::clearLastTranscription() {
    if (!m_lastTranscription.isEmpty()) {
        m_lastTranscription.clear();
        emit lastTranscriptionChanged();
    }
}

void TranscriptionPipeline::onRecordingFinished(const QByteArray& wavData) {
    if (wavData.isEmpty()) {
        qCDebug(lcSpeech) << "TranscriptionPipeline: Empty audio payload, resetting to Idle";
        setState(State::Idle);
        setStatusMessage(tr("Ready"));
        updatePipelineHealth();
        return;
    }

    m_lastWavData = wavData;
    setState(State::Transcribing);
    setStatusMessage(tr("Transcribing audio…"));
    updatePipelineHealth();

    auto* stt = activeSttClient();
    if (stt) {
        stt->transcribe(wavData);
    } else {
        setLastError(tr("Speech-to-text service is unavailable"));
        setState(State::Error);
        updatePipelineHealth();
    }
}

void TranscriptionPipeline::onMaxDurationReached() {
    qWarning() << "TranscriptionPipeline: Maximum recording duration safety limit reached";
    m_showMaxDurationNotice = true;
    m_maxDurationNoticeTimer->start();
    emit maxDurationWarningTriggered();
    emit activeNoticeChanged();
    stopRecording();
}

void TranscriptionPipeline::onSttTranscriptionReady(const QString& text) {
    if (m_state != State::Transcribing || (sender() && sender() != activeSttClient())) {
        qCDebug(lcSpeech) << "TranscriptionPipeline: Ignoring transcriptionReady: not in Transcribing state or sender "
                             "is not active client";
        return;
    }

    qCDebug(lcSpeech) << "TranscriptionPipeline: STT transcription received:" << text;

    if (text.trimmed().isEmpty()) {
        setState(State::Idle);
        setStatusMessage(tr("Ready"));
        updatePipelineHealth();
        return;
    }

    if (m_llmClient && m_llmClient->enabled()) {
        qCDebug(lcSpeech) << "TranscriptionPipeline: Passing transcription to LLM post-processing...";
        setState(State::Enhancing);
        setStatusMessage(tr("Enhancing text with AI…"));
        m_llmClient->processText(text);
        updatePipelineHealth();
        return;
    }

    completeTranscription(text);
}

void TranscriptionPipeline::onLlmEnhancementReady(const QString& enhancedText) {
    qCDebug(lcSpeech) << "TranscriptionPipeline: LLM enhanced text received:" << enhancedText;
    completeTranscription(enhancedText);
}

void TranscriptionPipeline::onLlmError(const QString& error, const QString& fallbackRawText) {
    qWarning() << "TranscriptionPipeline: LLM enhancement failed:" << error << "-> Falling back to raw transcription.";
    emit llmFallbackWarningTriggered(error);

    if (!fallbackRawText.isEmpty()) {
        completeTranscription(fallbackRawText);
        setStatusMessage(tr("Ready (Used raw Whisper text)"));
    } else {
        setState(State::Idle);
        setStatusMessage(tr("Ready"));
        updatePipelineHealth();
    }
}

void TranscriptionPipeline::completeTranscription(const QString& text) {
    m_lastTranscription = text;
    emit lastTranscriptionChanged();
    setState(State::Idle);
    setStatusMessage(tr("Ready"));
    updatePipelineHealth();
    emit transcriptionFinished(text);
}

void TranscriptionPipeline::onSttError(const QString& error) {
    if (m_state != State::Transcribing || (sender() && sender() != activeSttClient())) {
        qCDebug(lcSpeech)
            << "TranscriptionPipeline: Ignoring STT error: not in Transcribing state or sender is not active client";
        return;
    }

    qWarning() << "TranscriptionPipeline: STT Error:" << error;
    setLastError(error);
    setStatusMessage(error);
    setState(State::Error);
    updatePipelineHealth();
    emit errorOccurred(error);
}

void TranscriptionPipeline::updatePipelineHealth() {
    emit canRecordChanged();
    emit activeNoticeChanged();
}

QVariantMap TranscriptionPipeline::activeNotice() const {
    if (auto* stt = activeSttClient()) {
        if (stt->hasNotice()) {
            return stt->notice();
        }
    }

    if (m_recorder && !m_recorder->hasAudioInputDevice()) {
        QVariantMap notice;
        notice[u"hasNotice"_s] = true;
        notice[u"type"_s] = u"warning"_s;
        notice[u"title"_s] = tr("No Microphone Detected");
        notice[u"message"_s] = tr("Please connect a microphone or check your audio permissions in system settings.");
        notice[u"actionText"_s] = tr("Audio Settings");
        notice[u"actionId"_s] = u"openSpeechSettings"_s;
        return notice;
    }

    if (m_showMaxDurationNotice) {
        QVariantMap notice;
        notice[u"hasNotice"_s] = true;
        notice[u"type"_s] = u"info"_s;
        notice[u"title"_s] = tr("Maximum Duration (5 min) Reached");
        notice[u"message"_s] =
            tr("Recording automatically stopped at the 5-minute safety ceiling. Transcribing audio now…");
        return notice;
    }

    if (m_state == State::Error) {
        QVariantMap notice;
        notice[u"hasNotice"_s] = true;
        notice[u"type"_s] = u"danger"_s;
        notice[u"title"_s] =
            (m_activeBackend == Backend::WhisperCpp) ? tr("Offline Transcription Failed") : tr("Transcription Failed");
        notice[u"message"_s] = m_lastError.isEmpty() ? (activeSttClient() && !activeSttClient()->lastError().isEmpty()
                                                            ? activeSttClient()->lastError()
                                                            : tr("An error occurred during transcription."))
                                                     : m_lastError;
        notice[u"actionText"_s] = tr("Retry Transcription");
        notice[u"actionId"_s] = u"retryStt"_s;
        notice[u"secondaryActionText"_s] = tr("Dismiss");
        notice[u"secondaryActionId"_s] = u"dismissError"_s;
        return notice;
    }

    return {};
}

bool TranscriptionPipeline::hasActiveNotice() const {
    return activeNotice().value(u"hasNotice"_s, false).toBool();
}

void TranscriptionPipeline::triggerNoticeAction(const QString& actionId) {
    qCDebug(lcSpeech) << "TranscriptionPipeline: triggerNoticeAction:" << actionId;
    if (actionId == QLatin1String("openApiKeySettings")) {
        emit openSettingsRequested(0);
    } else if (actionId == QLatin1String("openSpeechSettings")) {
        emit openSettingsRequested(1);
    } else if (actionId == QLatin1String("openOfflineSettings")) {
        emit openSettingsRequested(2);
    } else if (actionId == QLatin1String("openLlmSettings")) {
        emit openSettingsRequested(3);
    } else if (actionId == QLatin1String("retryStt")) {
        retry();
    } else if (actionId == QLatin1String("dismissError")) {
        clearError();
    }
}

void TranscriptionPipeline::setState(State state) {
    if (m_state != state) {
        m_state = state;
        emit stateChanged(m_state);
        emit canRecordChanged();
    }
}

void TranscriptionPipeline::setStatusMessage(const QString& msg) {
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
    }
}

void TranscriptionPipeline::setLastError(const QString& error) {
    if (m_lastError != error) {
        m_lastError = error;
        emit lastErrorChanged();
    }
}
