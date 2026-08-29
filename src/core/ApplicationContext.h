#pragma once

#include <QObject>

class AudioRecorder;
class ApiKeyStore;
class AudioFeedbackPlayer;
class CloudProviderModel;
class DBusService;
class DictationCoordinator;
class DictationPadModel;
class GlobalShortcutManager;
class GroqApiClient;
class GroqLlmClient;
class GroqSttClient;
class AbstractLlmClient;
class StatusNotifierService;
class SystemHealthMonitor;
class TextInjectorClient;
class TranscriptionModel;
class WhisperModelManager;
class WhisperSttClient;
class GeminiApiClient;
class GeminiSttClient;
class CloudSttRouter;
class QQmlApplicationEngine;

class ApplicationContext : public QObject {
    Q_OBJECT

public:
    explicit ApplicationContext(QObject* parent = nullptr);
    ~ApplicationContext() override = default;

    void initialize(QQmlApplicationEngine& engine);
    void initializeHeadless();
    [[nodiscard]] bool isInitialized() const noexcept;

    [[nodiscard]] DictationCoordinator* dictationCoordinator() const noexcept;
    [[nodiscard]] DictationPadModel* dictationPadModel() const noexcept;
    [[nodiscard]] SystemHealthMonitor* systemHealthMonitor() const noexcept;
    [[nodiscard]] AudioFeedbackPlayer* audioFeedbackPlayer() const noexcept;
    [[nodiscard]] ApiKeyStore* apiKeyStore() const noexcept;
    [[nodiscard]] WhisperSttClient* whisperSttClient() const noexcept;
    [[nodiscard]] WhisperModelManager* whisperModelManager() const noexcept;
    [[nodiscard]] AbstractLlmClient* llmClient() const noexcept;
    [[nodiscard]] CloudProviderModel* cloudProviderModel() const noexcept;
    [[nodiscard]] GroqApiClient* groqApiClient() const noexcept;
    [[nodiscard]] GroqSttClient* groqSttClient() const noexcept;
    [[nodiscard]] GeminiApiClient* geminiApiClient() const noexcept;
    [[nodiscard]] GeminiSttClient* geminiSttClient() const noexcept;
    [[nodiscard]] CloudSttRouter* cloudSttRouter() const noexcept;
    [[nodiscard]] AudioRecorder* audioRecorder() const noexcept;
    [[nodiscard]] GlobalShortcutManager* shortcutManager() const noexcept;
    [[nodiscard]] TextInjectorClient* textInjector() const noexcept;
    [[nodiscard]] TranscriptionModel* historyModel() const noexcept;
    [[nodiscard]] DBusService* dbusService() const noexcept;
    [[nodiscard]] StatusNotifierService* statusNotifierService() const noexcept;

private:
    void wireSubsystems();

    GroqApiClient* m_apiClient = nullptr;
    GroqSttClient* m_groqSttClient = nullptr;
    GeminiApiClient* m_geminiApiClient = nullptr;
    GeminiSttClient* m_geminiSttClient = nullptr;
    CloudSttRouter* m_cloudSttRouter = nullptr;
    WhisperSttClient* m_whisperSttClient = nullptr;
    WhisperModelManager* m_whisperModelManager = nullptr;
    GroqLlmClient* m_groqLlmClient = nullptr;
    CloudProviderModel* m_cloudProviderModel = nullptr;
    AudioRecorder* m_audioRecorder = nullptr;
    GlobalShortcutManager* m_shortcutManager = nullptr;
    TextInjectorClient* m_textInjector = nullptr;
    TranscriptionModel* m_historyModel = nullptr;
    DictationCoordinator* m_dictationCoordinator = nullptr;
    DictationPadModel* m_padModel = nullptr;
    SystemHealthMonitor* m_healthMonitor = nullptr;
    AudioFeedbackPlayer* m_feedbackPlayer = nullptr;
    DBusService* m_dbusService = nullptr;
    StatusNotifierService* m_statusNotifierService = nullptr;

    bool m_initialized = false;
};
