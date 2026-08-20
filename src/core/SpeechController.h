#pragma once

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantMap>

class GlobalShortcutManager;
class AudioRecorder;
class GroqApiClient;
class GroqSttClient;
class WhisperSttClient;
class AbstractSttClient;
class GroqLlmClient;
class TextInjectorClient;
class TranscriptionModel;
class QSoundEffect;
class QTimer;

class SpeechController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    enum class DictationState { Idle, Recording, Transcribing, Enhancing, Error };
    Q_ENUM(DictationState)

    enum class TranscriptionBackend { Groq, WhisperCpp };
    Q_ENUM(TranscriptionBackend)

    Q_PROPERTY(TranscriptionBackend activeBackend READ activeBackend WRITE setActiveBackend NOTIFY activeBackendChanged FINAL)
    Q_PROPERTY(DictationState dictationState READ dictationState NOTIFY dictationStateChanged FINAL)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY dictationStateChanged FINAL)
    Q_PROPERTY(bool canRecord READ canRecord NOTIFY canRecordChanged FINAL)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged FINAL)
    Q_PROPERTY(bool transcribing READ transcribing NOTIFY transcribingChanged FINAL)
    Q_PROPERTY(bool enhancing READ enhancing NOTIFY enhancingChanged FINAL)
    Q_PROPERTY(QString lastTranscription READ lastTranscription NOTIFY lastTranscriptionChanged FINAL)
    Q_PROPERTY(bool soundEnabled READ soundEnabled WRITE setSoundEnabled NOTIFY soundEnabledChanged FINAL)

    Q_PROPERTY(QVariantMap activeNotice READ activeNotice NOTIFY activeNoticeChanged FINAL)
    Q_PROPERTY(bool hasActiveNotice READ hasActiveNotice NOTIFY activeNoticeChanged FINAL)

    Q_PROPERTY(bool systemShortcutHasIssue READ systemShortcutHasIssue NOTIFY systemHealthChanged FINAL)
    Q_PROPERTY(bool systemShortcutSupported READ systemShortcutSupported NOTIFY systemHealthChanged FINAL)
    Q_PROPERTY(QString systemShortcutStatus READ systemShortcutStatus NOTIFY systemHealthChanged FINAL)
    Q_PROPERTY(bool directTypingHasIssue READ directTypingHasIssue NOTIFY systemHealthChanged FINAL)
    Q_PROPERTY(bool directTypingConnected READ directTypingConnected NOTIFY systemHealthChanged FINAL)
    Q_PROPERTY(bool directTypingFatalError READ directTypingFatalError NOTIFY systemHealthChanged FINAL)
    Q_PROPERTY(QString directTypingStatus READ directTypingStatus NOTIFY systemHealthChanged FINAL)

    Q_PROPERTY(QString dictationPadText READ dictationPadText WRITE setDictationPadText NOTIFY dictationPadTextChanged FINAL)
    Q_PROPERTY(int dictationWordCount READ dictationWordCount NOTIFY dictationPadTextChanged FINAL)
    Q_PROPERTY(int dictationCharCount READ dictationCharCount NOTIFY dictationPadTextChanged FINAL)

public:
    explicit SpeechController(QObject* parent = nullptr);
    ~SpeechController() override = default;

    void registerSttClient(TranscriptionBackend backend, AbstractSttClient* client);
    void setSttClient(GroqSttClient* sttClient);
    void setWhisperSttClient(WhisperSttClient* whisperClient);
    void setApiClient(GroqApiClient* api);
    void setShortcutManager(GlobalShortcutManager* mgr);
    void setAudioRecorder(AudioRecorder* recorder);
    void setLlmClient(GroqLlmClient* llmClient);
    void setTextInjector(TextInjectorClient* injector);
    void setHistoryModel(TranscriptionModel* model);

    TranscriptionBackend activeBackend() const;
    void setActiveBackend(TranscriptionBackend backend);

    DictationState dictationState() const;
    bool isBusy() const;
    bool canRecord() const;
    QString statusMessage() const;
    QString lastError() const;
    bool recording() const;
    bool transcribing() const;
    bool enhancing() const;
    QString lastTranscription() const;
    bool soundEnabled() const;
    void setSoundEnabled(bool enabled);

    QVariantMap activeNotice() const;
    bool hasActiveNotice() const;

    bool systemShortcutHasIssue() const;
    bool systemShortcutSupported() const;
    QString systemShortcutStatus() const;
    bool directTypingHasIssue() const;
    bool directTypingConnected() const;
    bool directTypingFatalError() const;
    QString directTypingStatus() const;

    QString dictationPadText() const;
    void setDictationPadText(const QString& text);
    int dictationWordCount() const;
    int dictationCharCount() const;

public slots:
    void initialize();
    void triggerNoticeAction(const QString& actionId);
    void appendDictationPadText(const QString& text);
    void clearDictationPad();
    void copyDictationPad();

    void toggleRecording();
    void startRecording();
    void stopRecording();
    void cancelDictation();
    void retryTranscription();

    void clearLastTranscription();
    void clearError();
    void playStartSound();
    void playStopSound();

    void showWindow();
    void quitApp();

signals:
    void activeBackendChanged();
    void dictationStateChanged();
    void canRecordChanged();
    void statusMessageChanged();
    void lastErrorChanged();
    void recordingChanged();
    void transcribingChanged();
    void enhancingChanged();
    void lastTranscriptionChanged();
    void soundEnabledChanged();
    void activeNoticeChanged();
    void systemHealthChanged();
    void dictationPadTextChanged();

    void requestShowWindow();
    void requestQuitApp();
    void openSettingsRequested(int categoryIndex);
    void maxDurationWarningTriggered();
    void llmFallbackWarningTriggered(const QString& warning);

private slots:
    void onShortcutActivated(const QString& shortcutId);
    void onRecordingFinished(const QByteArray& wavData);
    void onTranscriptionReady(const QString& text);
    void onSttError(const QString& error);
    void onLlmEnhancementReady(const QString& enhancedText);
    void onLlmError(const QString& error, const QString& fallbackRawText);
    void onMaxDurationReached();
    void updatePresenterState();

private:
    AbstractSttClient* activeSttClient() const;
    void setDictationState(DictationState state);
    void setStatusMessage(const QString& msg);
    void setLastError(const QString& error);
    void finishTranscriptionAndInject(const QString& text);
    static int calculateWordCount(const QString& text);

    GroqApiClient* m_apiClient = nullptr;
    GlobalShortcutManager* m_shortcutMgr = nullptr;
    AudioRecorder* m_recorder = nullptr;
    QHash<TranscriptionBackend, AbstractSttClient*> m_sttClients;
    GroqLlmClient* m_llmClient = nullptr;
    TextInjectorClient* m_injector = nullptr;
    TranscriptionModel* m_historyModel = nullptr;
    QSoundEffect* m_startChime = nullptr;
    QSoundEffect* m_stopChime = nullptr;
    QTimer* m_maxDurationWarningTimer = nullptr;

    TranscriptionBackend m_activeBackend = TranscriptionBackend::Groq;
    bool m_soundEnabled = true;
    bool m_initialized = false;
    bool m_showMaxDurationNotice = false;
    DictationState m_state = DictationState::Idle;
    QString m_statusMessage;
    QString m_lastError;
    QString m_lastTranscription;
    QString m_dictationPadText;
};
