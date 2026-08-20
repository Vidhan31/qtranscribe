#include "SpeechController.h"

#include "AudioRecorder.h"
#include "GroqApiClient.h"
#include "GroqLlmClient.h"
#include "GroqSttClient.h"
#include "LoggingCategories.h"
#include "TextInjectorClient.h"
#include "TranscriptionModel.h"

#include "GlobalShortcutManager.h"
#include "WhisperSttClient.h"

#include <QDebug>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSettings>
#include <QSoundEffect>
#include <QTimer>
#include <QUrl>

using namespace Qt::StringLiterals;

SpeechController::SpeechController(QObject* parent)
    : QObject(parent)
    , m_startChime(new QSoundEffect(this))
    , m_stopChime(new QSoundEffect(this))
    , m_maxDurationWarningTimer(new QTimer(this)) {
    m_startChime->setSource(QUrl(u"qrc:/qt/qml/QTranscribe/assets/chime_start.wav"_s));
    m_startChime->setVolume(0.8f);

    m_stopChime->setSource(QUrl(u"qrc:/qt/qml/QTranscribe/assets/chime_stop.wav"_s));
    m_stopChime->setVolume(0.8f);

    m_maxDurationWarningTimer->setInterval(6000);
    m_maxDurationWarningTimer->setSingleShot(true);
    connect(m_maxDurationWarningTimer, &QTimer::timeout, this, [this]() {
        m_showMaxDurationNotice = false;
        emit activeNoticeChanged();
    });

    QSettings settings;
    m_soundEnabled = settings.value(u"Audio/SoundEnabled"_s, true).toBool();

    const QString backendStr = settings.value(u"Speech/Backend"_s, u"Groq"_s).toString();
    if (backendStr == u"WhisperCpp"_s) {
        m_activeBackend = TranscriptionBackend::WhisperCpp;
    } else {
        m_activeBackend = TranscriptionBackend::Groq;
    }

    setStatusMessage(u"Ready"_s);
    qCDebug(lcSpeech) << "SpeechController dictation pipeline engine constructed. Active backend:"
                      << (m_activeBackend == TranscriptionBackend::WhisperCpp ? "WhisperCpp" : "Groq");
}

void SpeechController::setApiClient(GroqApiClient* api) {
    if (m_apiClient == api) {
        return;
    }
    if (m_apiClient) {
        disconnect(m_apiClient, &GroqApiClient::apiKeySetChanged, this, &SpeechController::updatePresenterState);
    }
    m_apiClient = api;
    if (m_apiClient) {
        connect(m_apiClient, &GroqApiClient::apiKeySetChanged, this, &SpeechController::updatePresenterState);
    }
    updatePresenterState();
}

void SpeechController::setShortcutManager(GlobalShortcutManager* mgr) {
    if (m_shortcutMgr == mgr) {
        return;
    }
    if (m_shortcutMgr) {
        disconnect(m_shortcutMgr, &GlobalShortcutManager::shortcutActivated, this,
                   &SpeechController::onShortcutActivated);
        disconnect(m_shortcutMgr, &GlobalShortcutManager::availableChanged, this,
                   &SpeechController::updatePresenterState);
        disconnect(m_shortcutMgr, &GlobalShortcutManager::supportedChanged, this,
                   &SpeechController::updatePresenterState);
        disconnect(m_shortcutMgr, &GlobalShortcutManager::statusMessageChanged, this,
                   &SpeechController::updatePresenterState);
    }
    m_shortcutMgr = mgr;
    if (m_shortcutMgr) {
        connect(m_shortcutMgr, &GlobalShortcutManager::shortcutActivated, this, &SpeechController::onShortcutActivated);
        connect(m_shortcutMgr, &GlobalShortcutManager::availableChanged, this, &SpeechController::updatePresenterState);
        connect(m_shortcutMgr, &GlobalShortcutManager::supportedChanged, this, &SpeechController::updatePresenterState);
        connect(m_shortcutMgr, &GlobalShortcutManager::statusMessageChanged, this,
                &SpeechController::updatePresenterState);
    }
    updatePresenterState();
}

void SpeechController::setAudioRecorder(AudioRecorder* recorder) {
    if (m_recorder == recorder) {
        return;
    }
    if (m_recorder) {
        disconnect(m_recorder, &AudioRecorder::recordingFinished, this, &SpeechController::onRecordingFinished);
        disconnect(m_recorder, &AudioRecorder::maxDurationReached, this, &SpeechController::onMaxDurationReached);
        disconnect(m_recorder, &AudioRecorder::hasAudioInputDeviceChanged, this,
                   &SpeechController::updatePresenterState);
    }
    m_recorder = recorder;
    if (m_recorder) {
        connect(m_recorder, &AudioRecorder::recordingFinished, this, &SpeechController::onRecordingFinished);
        connect(m_recorder, &AudioRecorder::maxDurationReached, this, &SpeechController::onMaxDurationReached);
        connect(m_recorder, &AudioRecorder::hasAudioInputDeviceChanged, this, &SpeechController::updatePresenterState);
    }
    updatePresenterState();
}

void SpeechController::registerSttClient(TranscriptionBackend backend, AbstractSttClient* client) {
    if (m_sttClients.value(backend) == client) {
        return;
    }

    if (auto* old = m_sttClients.value(backend)) {
        disconnect(old, &AbstractSttClient::transcriptionReady, this, &SpeechController::onTranscriptionReady);
        disconnect(old, &AbstractSttClient::errorOccurred, this, &SpeechController::onSttError);
        disconnect(old, &AbstractSttClient::readyChanged, this, &SpeechController::updatePresenterState);
        disconnect(old, &AbstractSttClient::busyChanged, this, &SpeechController::updatePresenterState);
        disconnect(old, &AbstractSttClient::noticeChanged, this, &SpeechController::updatePresenterState);
    }

    if (client) {
        m_sttClients.insert(backend, client);
        connect(client, &AbstractSttClient::transcriptionReady, this, &SpeechController::onTranscriptionReady);
        connect(client, &AbstractSttClient::errorOccurred, this, &SpeechController::onSttError);
        connect(client, &AbstractSttClient::readyChanged, this, &SpeechController::updatePresenterState);
        connect(client, &AbstractSttClient::busyChanged, this, &SpeechController::updatePresenterState);
        connect(client, &AbstractSttClient::noticeChanged, this, &SpeechController::updatePresenterState);

        if (backend == m_activeBackend && m_initialized) {
            client->activate();
        }
    } else {
        m_sttClients.remove(backend);
    }

    updatePresenterState();
}

void SpeechController::setSttClient(GroqSttClient* sttClient) {
    registerSttClient(TranscriptionBackend::Groq, sttClient);
}

void SpeechController::setWhisperSttClient(WhisperSttClient* whisperClient) {
    registerSttClient(TranscriptionBackend::WhisperCpp, whisperClient);
}

void SpeechController::setLlmClient(GroqLlmClient* llmClient) {
    if (m_llmClient == llmClient) {
        return;
    }
    if (m_llmClient) {
        disconnect(m_llmClient, &GroqLlmClient::enhancementReady, this, &SpeechController::onLlmEnhancementReady);
        disconnect(m_llmClient, &GroqLlmClient::errorOccurred, this, &SpeechController::onLlmError);
    }
    m_llmClient = llmClient;
    if (m_llmClient) {
        connect(m_llmClient, &GroqLlmClient::enhancementReady, this, &SpeechController::onLlmEnhancementReady);
        connect(m_llmClient, &GroqLlmClient::errorOccurred, this, &SpeechController::onLlmError);
    }
}

void SpeechController::setTextInjector(TextInjectorClient* injector) {
    if (m_injector == injector) {
        return;
    }
    if (m_injector) {
        disconnect(m_injector, &TextInjectorClient::connectedChanged, this, &SpeechController::updatePresenterState);
        disconnect(m_injector, &TextInjectorClient::hasFatalErrorChanged, this,
                   &SpeechController::updatePresenterState);
        disconnect(m_injector, &TextInjectorClient::fatalErrorMessageChanged, this,
                   &SpeechController::updatePresenterState);
    }
    m_injector = injector;
    if (m_injector) {
        connect(m_injector, &TextInjectorClient::connectedChanged, this, &SpeechController::updatePresenterState);
        connect(m_injector, &TextInjectorClient::hasFatalErrorChanged, this, &SpeechController::updatePresenterState);
        connect(m_injector, &TextInjectorClient::fatalErrorMessageChanged, this,
                &SpeechController::updatePresenterState);
    }
    updatePresenterState();
}

void SpeechController::setHistoryModel(TranscriptionModel* model) {
    m_historyModel = model;
}

SpeechController::TranscriptionBackend SpeechController::activeBackend() const {
    return m_activeBackend;
}

void SpeechController::setActiveBackend(TranscriptionBackend backend) {
    if (m_activeBackend != backend) {
        if (auto* oldClient = activeSttClient()) {
            oldClient->deactivate();
        }

        m_activeBackend = backend;
        QSettings settings;
        settings.setValue(u"Speech/Backend"_s,
                          m_activeBackend == TranscriptionBackend::WhisperCpp ? u"WhisperCpp"_s : u"Groq"_s);

        if (auto* newClient = activeSttClient()) {
            newClient->activate();
        }

        emit activeBackendChanged();
        updatePresenterState();
    }
}

AbstractSttClient* SpeechController::activeSttClient() const {
    return m_sttClients.value(m_activeBackend, nullptr);
}

void SpeechController::initialize() {
    if (m_initialized) {
        return;
    }
    m_initialized = true;
    if (auto* client = activeSttClient()) {
        client->activate();
    }
    updatePresenterState();
    qCDebug(lcSpeech) << "SpeechController: pipeline initialized with registered STT engines";
}

void SpeechController::updatePresenterState() {
    emit canRecordChanged();
    emit activeNoticeChanged();
    emit systemHealthChanged();
}

SpeechController::DictationState SpeechController::dictationState() const {
    return m_state;
}

bool SpeechController::isBusy() const {
    return m_state == DictationState::Recording || m_state == DictationState::Transcribing ||
           m_state == DictationState::Enhancing;
}

bool SpeechController::canRecord() const {
    const bool micReady = m_recorder && m_recorder->hasAudioInputDevice();
    const bool notProcessing = !transcribing() && !enhancing();
    const bool sttReady = activeSttClient() && activeSttClient()->isReady();
    return micReady && notProcessing && sttReady;
}

QString SpeechController::statusMessage() const {
    return m_statusMessage;
}

QString SpeechController::lastError() const {
    return m_lastError;
}

bool SpeechController::recording() const {
    return m_state == DictationState::Recording;
}

bool SpeechController::transcribing() const {
    return m_state == DictationState::Transcribing;
}

bool SpeechController::enhancing() const {
    return m_state == DictationState::Enhancing;
}

QString SpeechController::lastTranscription() const {
    return m_lastTranscription;
}

bool SpeechController::soundEnabled() const {
    return m_soundEnabled;
}

void SpeechController::setSoundEnabled(bool enabled) {
    if (m_soundEnabled != enabled) {
        m_soundEnabled = enabled;
        QSettings settings;
        settings.setValue(u"Audio/SoundEnabled"_s, m_soundEnabled);
        emit soundEnabledChanged();
    }
}

QVariantMap SpeechController::activeNotice() const {
    if (auto* stt = activeSttClient()) {
        if (stt->hasNotice()) {
            return stt->notice();
        }
    }

    if (m_injector && m_injector->hasFatalError()) {
        QVariantMap notice;
        notice[u"hasNotice"_s] = true;
        notice[u"type"_s] = u"danger"_s;
        notice[u"title"_s] = tr("Direct Typing Service Stopped");
        notice[u"message"_s] = m_injector->fatalErrorMessage().isEmpty()
                                   ? tr("The background key injection daemon encountered an error. Text will fallback "
                                        "to clipboard paste until restarted.")
                                   : m_injector->fatalErrorMessage();
        notice[u"actionText"_s] = tr("Restart Service");
        notice[u"actionId"_s] = u"restartInjector"_s;
        notice[u"secondaryActionText"_s] = tr("Settings");
        notice[u"secondaryActionId"_s] = u"openSystemSettings"_s;
        return notice;
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

    if (m_state == DictationState::Error) {
        QVariantMap notice;
        notice[u"hasNotice"_s] = true;

        if (auto* groq = qobject_cast<GroqSttClient*>(activeSttClient())) {
            const auto errCat = groq->errorCategory();

            if (errCat == GroqSttClient::ErrorCategory::RateLimited && groq->retrySecondsRemaining() > 0) {
                notice[u"type"_s] = u"warning"_s;
                notice[u"title"_s] = tr("Rate Limit Exceeded");
                notice[u"message"_s] = tr("Auto-retrying in %1s…").arg(groq->retrySecondsRemaining());
                notice[u"actionText"_s] = tr("Dismiss");
                notice[u"actionId"_s] = u"dismissError"_s;
            } else if (errCat == GroqSttClient::ErrorCategory::InvalidApiKey) {
                notice[u"type"_s] = u"warning"_s;
                notice[u"title"_s] = tr("Invalid API Key");
                notice[u"message"_s] = groq->lastError().isEmpty()
                                           ? tr("Authentication failed. Please verify your Groq API key.")
                                           : groq->lastError();
                notice[u"actionText"_s] = tr("Configure API Key");
                notice[u"actionId"_s] = u"openApiKeySettings"_s;
                notice[u"secondaryActionText"_s] = tr("Dismiss");
                notice[u"secondaryActionId"_s] = u"dismissError"_s;
            } else {
                notice[u"type"_s] = u"danger"_s;
                notice[u"title"_s] = tr("Transcription Failed");
                notice[u"message"_s] =
                    m_lastError.isEmpty() ? (groq->lastError().isEmpty() ? tr("An error occurred during transcription.")
                                                                         : groq->lastError())
                                          : m_lastError;
                notice[u"actionText"_s] = tr("Retry Transcription");
                notice[u"actionId"_s] = u"retryStt"_s;
                notice[u"secondaryActionText"_s] = tr("Dismiss");
                notice[u"secondaryActionId"_s] = u"dismissError"_s;
            }
        } else {
            notice[u"type"_s] = u"danger"_s;
            notice[u"title"_s] = tr("Offline Transcription Failed");
            notice[u"message"_s] =
                m_lastError.isEmpty() ? tr("An error occurred during offline Whisper inference.") : m_lastError;
            notice[u"actionText"_s] = tr("Dismiss");
            notice[u"actionId"_s] = u"dismissError"_s;
        }
        return notice;
    }

    return {};
}

bool SpeechController::hasActiveNotice() const {
    return activeNotice().value(u"hasNotice"_s, false).toBool();
}

void SpeechController::triggerNoticeAction(const QString& actionId) {
    qCDebug(lcSpeech) << "SpeechController: triggerNoticeAction:" << actionId;
    if (actionId == QLatin1String("openApiKeySettings")) {
        emit openSettingsRequested(0);
    } else if (actionId == QLatin1String("openSpeechSettings")) {
        emit openSettingsRequested(1);
    } else if (actionId == QLatin1String("openOfflineSettings")) {
        emit openSettingsRequested(2);
    } else if (actionId == QLatin1String("openSystemSettings")) {
        emit openSettingsRequested(4);
    } else if (actionId == QLatin1String("restartInjector")) {
        if (m_injector) {
            m_injector->restartService();
        }
    } else if (actionId == QLatin1String("retryStt")) {
        retryTranscription();
    } else if (actionId == QLatin1String("dismissError")) {
        clearError();
    }
}

bool SpeechController::systemShortcutHasIssue() const {
    return m_shortcutMgr && !m_shortcutMgr->isAvailable();
}

bool SpeechController::systemShortcutSupported() const {
    return m_shortcutMgr && m_shortcutMgr->isSupported();
}

QString SpeechController::systemShortcutStatus() const {
    return m_shortcutMgr ? m_shortcutMgr->statusMessage() : QString();
}

bool SpeechController::directTypingHasIssue() const {
    return m_injector && (!m_injector->isConnected() || m_injector->hasFatalError());
}

bool SpeechController::directTypingConnected() const {
    return m_injector && m_injector->isConnected();
}

bool SpeechController::directTypingFatalError() const {
    return m_injector && m_injector->hasFatalError();
}

QString SpeechController::directTypingStatus() const {
    if (!m_injector) {
        return QString();
    }
    if (m_injector->hasFatalError()) {
        return m_injector->fatalErrorMessage().isEmpty() ? tr("Direct Typing Error") : m_injector->fatalErrorMessage();
    }
    if (!m_injector->isConnected()) {
        return tr("Clipboard Fallback");
    }
    return tr("Connected");
}

QString SpeechController::dictationPadText() const {
    return m_dictationPadText;
}

void SpeechController::setDictationPadText(const QString& text) {
    if (m_dictationPadText != text) {
        m_dictationPadText = text;
        emit dictationPadTextChanged();
    }
}

int SpeechController::dictationWordCount() const {
    return calculateWordCount(m_dictationPadText);
}

int SpeechController::dictationCharCount() const {
    return m_dictationPadText.length();
}

void SpeechController::appendDictationPadText(const QString& text) {
    if (text.isEmpty()) {
        return;
    }
    if (m_dictationPadText.isEmpty()) {
        m_dictationPadText = text;
    } else {
        m_dictationPadText += u"\n"_s + text;
    }
    emit dictationPadTextChanged();
}

void SpeechController::clearDictationPad() {
    if (!m_dictationPadText.isEmpty()) {
        m_dictationPadText.clear();
        emit dictationPadTextChanged();
    }
}

void SpeechController::copyDictationPad() {
    if (!m_dictationPadText.isEmpty() && m_historyModel) {
        m_historyModel->copyToClipboard(m_dictationPadText);
    }
}

int SpeechController::calculateWordCount(const QString& text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return 0;
    }
    return trimmed.split(QRegularExpression(u"\\s+"_s), Qt::SkipEmptyParts).size();
}

void SpeechController::showWindow() {
    qCDebug(lcSpeech) << "SpeechController: showWindow requested via IPC/D-Bus";
    emit requestShowWindow();
}

void SpeechController::quitApp() {
    qCDebug(lcSpeech) << "SpeechController: quitApp requested via IPC/D-Bus";
    emit requestQuitApp();
}

void SpeechController::startRecording() {
    if (isBusy()) {
        qCDebug(lcSpeech) << "SpeechController: startRecording ignored — already busy";
        return;
    }

    if (m_injector) {
        m_injector->cancelPendingInjection();
    }

    if (!m_recorder || !m_recorder->hasAudioInputDevice()) {
        qWarning() << "SpeechController: No microphone input device available";
        setLastError(tr("No microphone found. Please connect a microphone."));
        setStatusMessage(tr("No microphone found"));
        setDictationState(DictationState::Error);
        updatePresenterState();
        return;
    }

    setLastError({});
    setDictationState(DictationState::Recording);
    setStatusMessage(tr("Listening…"));
    if (m_soundEnabled && m_startChime) {
        m_startChime->play();
    }
    m_recorder->startRecording();
    updatePresenterState();
}

void SpeechController::stopRecording() {
    if (m_state != DictationState::Recording) {
        qCDebug(lcSpeech) << "SpeechController: stopRecording ignored — not currently recording";
        return;
    }

    if (m_soundEnabled && m_stopChime) {
        m_stopChime->play();
    }
    if (m_recorder) {
        m_recorder->stopRecording();
    }
}

void SpeechController::toggleRecording() {
    if (m_state == DictationState::Recording) {
        stopRecording();
    } else if (m_state == DictationState::Idle || m_state == DictationState::Error) {
        startRecording();
    } else {
        qCDebug(lcSpeech) << "SpeechController: Toggle ignored while busy (state:" << static_cast<int>(m_state) << ")";
    }
}

void SpeechController::cancelDictation() {
    if (m_state == DictationState::Recording && m_recorder) {
        m_recorder->stopRecording();
    }
    if (activeSttClient()) {
        activeSttClient()->cancel();
    }
    if (m_llmClient) {
        m_llmClient->cancel();
    }
    if (m_injector) {
        m_injector->cancelPendingInjection();
    }

    setDictationState(DictationState::Idle);
    setStatusMessage(tr("Ready"));
    updatePresenterState();
}

void SpeechController::retryTranscription() {
    if (auto* groq = qobject_cast<GroqSttClient*>(activeSttClient())) {
        if (!isBusy()) {
            setLastError({});
            setDictationState(DictationState::Transcribing);
            setStatusMessage(tr("Retrying transcription…"));
            groq->retryLast();
            updatePresenterState();
        }
    }
}

void SpeechController::clearLastTranscription() {
    if (!m_lastTranscription.isEmpty()) {
        m_lastTranscription.clear();
        emit lastTranscriptionChanged();
    }
}

void SpeechController::clearError() {
    if (m_state == DictationState::Error) {
        setLastError({});
        setDictationState(DictationState::Idle);
        setStatusMessage(tr("Ready"));
        updatePresenterState();
    }
}

void SpeechController::playStartSound() {
    if (m_startChime) {
        m_startChime->play();
    }
}

void SpeechController::playStopSound() {
    if (m_stopChime) {
        m_stopChime->play();
    }
}

void SpeechController::onShortcutActivated(const QString& shortcutId) {
    if (shortcutId == QLatin1String("toggle-recording")) {
        qCDebug(lcSpeech) << "SpeechController: Global shortcut triggered toggleRecording";
        toggleRecording();
    }
}

void SpeechController::onRecordingFinished(const QByteArray& wavData) {
    if (wavData.isEmpty()) {
        qCDebug(lcSpeech) << "SpeechController: Empty audio payload, resetting to Idle";
        setDictationState(DictationState::Idle);
        setStatusMessage(tr("Ready"));
        updatePresenterState();
        return;
    }

    setDictationState(DictationState::Transcribing);
    setStatusMessage(tr("Transcribing audio…"));
    updatePresenterState();

    auto* stt = activeSttClient();
    if (stt) {
        stt->transcribe(wavData);
    } else {
        setLastError(tr("Speech-to-text service is unavailable"));
        setDictationState(DictationState::Error);
        updatePresenterState();
    }
}

void SpeechController::onTranscriptionReady(const QString& text) {
    qCDebug(lcSpeech) << "SpeechController: Transcription received:" << text;

    if (text.trimmed().isEmpty()) {
        setDictationState(DictationState::Idle);
        setStatusMessage(tr("Ready"));
        updatePresenterState();
        return;
    }

    if (m_llmClient && m_llmClient->enabled()) {
        qCDebug(lcSpeech) << "SpeechController: Passing transcription to LLM post-processing...";
        setDictationState(DictationState::Enhancing);
        setStatusMessage(tr("Enhancing text with AI…"));
        m_llmClient->processText(text);
        updatePresenterState();
        return;
    }

    finishTranscriptionAndInject(text);
    setDictationState(DictationState::Idle);
    setStatusMessage(tr("Ready"));
    updatePresenterState();
}

void SpeechController::onLlmEnhancementReady(const QString& enhancedText) {
    qCDebug(lcSpeech) << "SpeechController: LLM enhanced text received:" << enhancedText;
    finishTranscriptionAndInject(enhancedText);
    setDictationState(DictationState::Idle);
    setStatusMessage(tr("Ready"));
    updatePresenterState();
}

void SpeechController::onLlmError(const QString& error, const QString& fallbackRawText) {
    qWarning() << "SpeechController: LLM enhancement failed:" << error << "-> Falling back to raw transcription.";
    emit llmFallbackWarningTriggered(error);

    if (!fallbackRawText.isEmpty()) {
        finishTranscriptionAndInject(fallbackRawText);
        setStatusMessage(tr("Ready (Used raw Whisper text)"));
    } else {
        setStatusMessage(tr("Ready"));
    }
    setDictationState(DictationState::Idle);
    updatePresenterState();
}

void SpeechController::onSttError(const QString& error) {
    qWarning() << "SpeechController: STT Error:" << error;
    setLastError(error);
    setStatusMessage(error);
    setDictationState(DictationState::Error);
    updatePresenterState();
}

void SpeechController::onMaxDurationReached() {
    qWarning() << "SpeechController: Maximum recording duration safety limit reached";
    m_showMaxDurationNotice = true;
    m_maxDurationWarningTimer->start();
    emit maxDurationWarningTriggered();
    emit activeNoticeChanged();
    stopRecording();
}

void SpeechController::finishTranscriptionAndInject(const QString& text) {
    m_lastTranscription = text;
    emit lastTranscriptionChanged();

    if (text.isEmpty()) {
        return;
    }

    if (m_historyModel) {
        m_historyModel->addRecord(text);
    }

    appendDictationPadText(text);

    // Only inject virtual keystrokes into external target applications.
    // If QTranscribe itself currently has window focus, the text is already
    // displayed in the dictation pad and injecting Ctrl+V would cause duplicate text.
    const bool isAppActive = (QGuiApplication::focusWindow() != nullptr);
    if (m_injector && !isAppActive) {
        m_injector->typeText(text);
    }
}

void SpeechController::setDictationState(DictationState state) {
    if (m_state != state) {
        const bool wasRecording = recording();
        const bool wasTranscribing = transcribing();
        const bool wasEnhancing = enhancing();

        m_state = state;
        emit dictationStateChanged();

        if (wasRecording != recording()) {
            emit recordingChanged();
        }
        if (wasTranscribing != transcribing()) {
            emit transcribingChanged();
        }
        if (wasEnhancing != enhancing()) {
            emit enhancingChanged();
        }
        emit canRecordChanged();
    }
}

void SpeechController::setStatusMessage(const QString& msg) {
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
    }
}

void SpeechController::setLastError(const QString& error) {
    if (m_lastError != error) {
        m_lastError = error;
        emit lastErrorChanged();
    }
}
