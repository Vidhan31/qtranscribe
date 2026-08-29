#include "ApplicationContext.h"

#include "AudioRecorder.h"
#include "DBusService.h"
#include "DictationCoordinator.h"
#include "GroqApiClient.h"
#include "GroqLlmClient.h"
#include "GroqSttClient.h"
#include "LoggingCategories.h"
#include "TextInjectorClient.h"
#include "TranscriptionModel.h"

#include "ApiKeyStore.h"
#include "AudioFeedbackPlayer.h"
#include "CloudProviderModel.h"
#include "CloudSttRouter.h"
#include "DictationPadModel.h"
#include "GeminiApiClient.h"
#include "GeminiSttClient.h"
#include "GlobalShortcutManager.h"
#include "StatusNotifierService.h"
#include "SystemHealthMonitor.h"
#include "WhisperModelManager.h"
#include "WhisperSttClient.h"

#include <QQmlApplicationEngine>

using namespace Qt::StringLiterals;

ApplicationContext::ApplicationContext(QObject* parent)
    : QObject(parent) { }

void ApplicationContext::initialize(QQmlApplicationEngine& engine) {
    if (m_initialized) {
        return;
    }

    m_apiClient = engine.singletonInstance<GroqApiClient*>("QTranscribe", "GroqApiClient");
    m_groqSttClient = engine.singletonInstance<GroqSttClient*>("QTranscribe", "GroqSttClient");
    m_geminiApiClient = engine.singletonInstance<GeminiApiClient*>("QTranscribe", "GeminiApiClient");
    m_geminiSttClient = engine.singletonInstance<GeminiSttClient*>("QTranscribe", "GeminiSttClient");
    m_cloudSttRouter = engine.singletonInstance<CloudSttRouter*>("QTranscribe", "CloudSttRouter");
    m_whisperSttClient = engine.singletonInstance<WhisperSttClient*>("QTranscribe", "WhisperSttClient");
    m_whisperModelManager = engine.singletonInstance<WhisperModelManager*>("QTranscribe", "WhisperModelManager");
    m_groqLlmClient = engine.singletonInstance<GroqLlmClient*>("QTranscribe", "GroqLlmClient");
    m_cloudProviderModel = engine.singletonInstance<CloudProviderModel*>("QTranscribe", "CloudProviderModel");
    m_audioRecorder = engine.singletonInstance<AudioRecorder*>("QTranscribe", "AudioRecorder");
    m_shortcutManager = engine.singletonInstance<GlobalShortcutManager*>("QTranscribe", "GlobalShortcutManager");
    m_textInjector = engine.singletonInstance<TextInjectorClient*>("QTranscribe", "TextInjectorClient");
    m_historyModel = engine.singletonInstance<TranscriptionModel*>("QTranscribe", "TranscriptionModel");
    m_padModel = engine.singletonInstance<DictationPadModel*>("QTranscribe", "DictationPadModel");
    m_healthMonitor = engine.singletonInstance<SystemHealthMonitor*>("QTranscribe", "SystemHealthMonitor");
    m_feedbackPlayer = engine.singletonInstance<AudioFeedbackPlayer*>("QTranscribe", "AudioFeedbackPlayer");
    m_dictationCoordinator = engine.singletonInstance<DictationCoordinator*>("QTranscribe", "DictationCoordinator");

    wireSubsystems();

    if (m_dictationCoordinator) {
        m_dbusService = new DBusService(this);
        m_dbusService->registerController(m_dictationCoordinator);

        m_statusNotifierService = new StatusNotifierService(this);
        m_statusNotifierService->registerController(m_dictationCoordinator);
    }

    m_initialized = true;
    qCDebug(lcSpeech) << "ApplicationContext: GUI composition root successfully initialized";
}

void ApplicationContext::initializeHeadless() {
    if (m_initialized) {
        return;
    }

    m_apiClient = new GroqApiClient(this);
    m_groqSttClient = new GroqSttClient(this);
    m_geminiApiClient = new GeminiApiClient(this);
    m_geminiSttClient = new GeminiSttClient(this);
    m_cloudSttRouter = new CloudSttRouter(this);
    m_whisperModelManager = new WhisperModelManager(this);
    m_whisperSttClient = new WhisperSttClient(this);
    m_groqLlmClient = new GroqLlmClient(this);
    m_cloudProviderModel = new CloudProviderModel(this);
    m_audioRecorder = new AudioRecorder(this);
    m_shortcutManager = new GlobalShortcutManager(this);
    m_textInjector = new TextInjectorClient(this);
    m_historyModel = new TranscriptionModel(this);
    m_padModel = new DictationPadModel(this);
    m_healthMonitor = new SystemHealthMonitor(this);
    m_feedbackPlayer = new AudioFeedbackPlayer(this);
    m_dictationCoordinator = new DictationCoordinator(this);

    wireSubsystems();

    m_initialized = true;
    qCDebug(lcSpeech) << "ApplicationContext: Headless composition root successfully initialized";
}

void ApplicationContext::wireSubsystems() {
    if (m_groqSttClient && m_apiClient) {
        m_groqSttClient->setApiClient(m_apiClient);
    }
    if (m_geminiSttClient && m_geminiApiClient) {
        m_geminiSttClient->setApiClient(m_geminiApiClient);
    }
    if (m_whisperSttClient && m_whisperModelManager) {
        m_whisperSttClient->setModelManager(m_whisperModelManager);
    }
    if (m_groqLlmClient && m_apiClient) {
        m_groqLlmClient->setApiClient(m_apiClient);
    }

    if (m_cloudSttRouter) {
        m_cloudSttRouter->setCloudProviderModel(m_cloudProviderModel);
        m_cloudSttRouter->registerProvider(u"groq"_s, m_groqSttClient);
        m_cloudSttRouter->registerProvider(u"gemini"_s, m_geminiSttClient);
    }

    if (m_healthMonitor) {
        m_healthMonitor->setShortcutManager(m_shortcutManager);
        m_healthMonitor->setTextInjector(m_textInjector);
        m_healthMonitor->setAudioRecorder(m_audioRecorder);
    }

    if (m_dictationCoordinator) {
        m_dictationCoordinator->setDictationPadModel(m_padModel);
        m_dictationCoordinator->setSystemHealthMonitor(m_healthMonitor);
        m_dictationCoordinator->setAudioFeedbackPlayer(m_feedbackPlayer);
        m_dictationCoordinator->setAudioRecorder(m_audioRecorder);
        AbstractSttClient* cloudClient = m_cloudSttRouter ? static_cast<AbstractSttClient*>(m_cloudSttRouter)
                                                          : static_cast<AbstractSttClient*>(m_groqSttClient);
        m_dictationCoordinator->registerBackend(DictationCoordinator::TranscriptionBackend::Cloud, cloudClient);
        m_dictationCoordinator->registerBackend(DictationCoordinator::TranscriptionBackend::WhisperCpp,
                                                m_whisperSttClient);
        m_dictationCoordinator->setLlmClient(m_groqLlmClient);
        m_dictationCoordinator->setShortcutManager(m_shortcutManager);
        m_dictationCoordinator->setTextInjector(m_textInjector);
        m_dictationCoordinator->setHistoryModel(m_historyModel);
        m_dictationCoordinator->initialize();
    }
}

bool ApplicationContext::isInitialized() const noexcept {
    return m_initialized;
}

DictationCoordinator* ApplicationContext::dictationCoordinator() const noexcept {
    return m_dictationCoordinator;
}

DictationPadModel* ApplicationContext::dictationPadModel() const noexcept {
    return m_padModel;
}

SystemHealthMonitor* ApplicationContext::systemHealthMonitor() const noexcept {
    return m_healthMonitor;
}

AudioFeedbackPlayer* ApplicationContext::audioFeedbackPlayer() const noexcept {
    return m_feedbackPlayer;
}

ApiKeyStore* ApplicationContext::apiKeyStore() const noexcept {
    return m_apiClient ? m_apiClient->keyStore() : nullptr;
}

WhisperSttClient* ApplicationContext::whisperSttClient() const noexcept {
    return m_whisperSttClient;
}

WhisperModelManager* ApplicationContext::whisperModelManager() const noexcept {
    return m_whisperModelManager;
}

AbstractLlmClient* ApplicationContext::llmClient() const noexcept {
    return m_groqLlmClient;
}

CloudProviderModel* ApplicationContext::cloudProviderModel() const noexcept {
    return m_cloudProviderModel;
}

GroqApiClient* ApplicationContext::groqApiClient() const noexcept {
    return m_apiClient;
}

GroqSttClient* ApplicationContext::groqSttClient() const noexcept {
    return m_groqSttClient;
}

GeminiApiClient* ApplicationContext::geminiApiClient() const noexcept {
    return m_geminiApiClient;
}

GeminiSttClient* ApplicationContext::geminiSttClient() const noexcept {
    return m_geminiSttClient;
}

CloudSttRouter* ApplicationContext::cloudSttRouter() const noexcept {
    return m_cloudSttRouter;
}

AudioRecorder* ApplicationContext::audioRecorder() const noexcept {
    return m_audioRecorder;
}

GlobalShortcutManager* ApplicationContext::shortcutManager() const noexcept {
    return m_shortcutManager;
}

TextInjectorClient* ApplicationContext::textInjector() const noexcept {
    return m_textInjector;
}

TranscriptionModel* ApplicationContext::historyModel() const noexcept {
    return m_historyModel;
}

DBusService* ApplicationContext::dbusService() const noexcept {
    return m_dbusService;
}

StatusNotifierService* ApplicationContext::statusNotifierService() const noexcept {
    return m_statusNotifierService;
}
