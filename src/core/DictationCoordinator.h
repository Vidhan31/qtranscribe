#pragma once

#include "AbstractSttClient.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QString>

class AudioFeedbackPlayer;
class AudioRecorder;
class DictationPadModel;
class GlobalShortcutManager;
class AbstractLlmClient;
class SystemHealthMonitor;
class TextInjectorClient;
class TranscriptionModel;

class DictationCoordinator : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(TranscriptionBackend activeBackend READ activeBackend WRITE setActiveBackend NOTIFY activeBackendChanged FINAL)
    Q_PROPERTY(RecordingMode recordingMode READ recordingMode WRITE setRecordingMode NOTIFY recordingModeChanged FINAL)
    Q_PROPERTY(DictationState dictationState READ dictationState NOTIFY dictationStateChanged FINAL)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY dictationStateChanged FINAL)
    Q_PROPERTY(bool canRecord READ canRecord NOTIFY canRecordChanged FINAL)
    Q_PROPERTY(qreal audioLevel READ audioLevel NOTIFY audioLevelChanged FINAL)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged FINAL)
    Q_PROPERTY(bool transcribing READ transcribing NOTIFY transcribingChanged FINAL)
    Q_PROPERTY(bool enhancing READ enhancing NOTIFY enhancingChanged FINAL)
    Q_PROPERTY(QString lastTranscription READ lastTranscription NOTIFY lastTranscriptionChanged FINAL)

public:
    enum class DictationState { Idle, Recording, Transcribing, Enhancing, Error };
    Q_ENUM(DictationState)

    enum class TranscriptionBackend { Cloud, WhisperCpp };
    Q_ENUM(TranscriptionBackend)

    enum class RecordingMode { Toggle, PushToTalk };
    Q_ENUM(RecordingMode)

public:
    explicit DictationCoordinator(QObject* parent = nullptr);
    ~DictationCoordinator() override = default;

    void setAudioRecorder(AudioRecorder* recorder);
    void registerBackend(TranscriptionBackend backend, AbstractSttClient* client);
    void setLlmClient(AbstractLlmClient* llmClient);
    void setShortcutManager(GlobalShortcutManager* mgr);
    void setTextInjector(TextInjectorClient* injector);
    void setHistoryModel(TranscriptionModel* model);
    void setDictationPadModel(DictationPadModel* padModel);
    void setSystemHealthMonitor(SystemHealthMonitor* healthMonitor);
    void setAudioFeedbackPlayer(AudioFeedbackPlayer* feedbackPlayer);

    TranscriptionBackend activeBackend() const;
    void setActiveBackend(TranscriptionBackend backend);

    RecordingMode recordingMode() const;
    void setRecordingMode(RecordingMode mode);

    DictationState dictationState() const;
    bool isBusy() const;
    bool canRecord() const;
    qreal audioLevel() const;
    QString statusMessage() const;
    QString lastError() const;
    bool recording() const;
    bool transcribing() const;
    bool enhancing() const;
    QString lastTranscription() const;

    AbstractSttClient* activeSttClient() const;
    DictationPadModel* dictationPadModel() const;
    AudioFeedbackPlayer* audioFeedbackPlayer() const;
    SystemHealthMonitor* systemHealthMonitor() const;

public slots:
    void initialize();
    void toggleRecording();
    void startRecording();
    void stopRecording();
    void cancelDictation();
    void retryTranscription();

    void clearLastTranscription();
    void clearError();

    void showWindow();
    void quitApp();

signals:
    void activeBackendChanged();
    void recordingModeChanged();
    void dictationStateChanged();
    void canRecordChanged();
    void audioLevelChanged(qreal level);
    void statusMessageChanged();
    void lastErrorChanged();
    void recordingChanged();
    void transcribingChanged();
    void enhancingChanged();
    void lastTranscriptionChanged();

    void transcriptionFinished(const QString& text);
    void errorOccurred(const QString& error);
    void requestShowWindow();
    void requestQuitApp();
    void maxDurationWarningTriggered();
    void llmFallbackWarningTriggered(const QString& warning);

private slots:
    void onRecordingFinished(const QByteArray& wavData);
    void onMaxDurationReached();
    void onSttTranscriptionReady(const QString& text);
    void onSttError(const QString& error);
    void onLlmEnhancementReady(const QString& enhancedText);
    void onLlmError(const QString& error, const QString& fallbackRawText);
    void onShortcutActivated(const QString& shortcutId);
    void onShortcutDeactivated(const QString& shortcutId);
    void updateCoordinatorHealth();

private:
    void setState(DictationState state);
    void setStatusMessage(const QString& msg);
    void setLastError(const QString& error);
    void completeTranscription(const QString& text);
    void finishTranscriptionAndInject(const QString& text);

    AudioRecorder* m_recorder = nullptr;
    QHash<TranscriptionBackend, AbstractSttClient*> m_sttClients;
    AbstractLlmClient* m_llmClient = nullptr;
    GlobalShortcutManager* m_shortcutMgr = nullptr;
    TextInjectorClient* m_injector = nullptr;
    TranscriptionModel* m_historyModel = nullptr;

    DictationPadModel* m_padModel = nullptr;
    AudioFeedbackPlayer* m_feedbackPlayer = nullptr;
    SystemHealthMonitor* m_healthMonitor = nullptr;

    TranscriptionBackend m_activeBackend = TranscriptionBackend::WhisperCpp;
    DictationState m_state = DictationState::Idle;
    RecordingMode m_recordingMode = RecordingMode::Toggle;
    QString m_statusMessage;
    QString m_lastError;
    QString m_lastTranscription;
    QByteArray m_lastWavData;

    bool m_initialized = false;

    friend class TestDictationCoordinator;
};
