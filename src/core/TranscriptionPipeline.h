#pragma once

#include "AbstractSttClient.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QString>

class AudioRecorder;
class GroqLlmClient;
class QTimer;

class TranscriptionPipeline : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    enum class State { Idle, Recording, Transcribing, Enhancing, Error };
    Q_ENUM(State)

    enum class Backend { Groq, WhisperCpp };
    Q_ENUM(Backend)

    Q_PROPERTY(State state READ state NOTIFY stateChanged FINAL)
    Q_PROPERTY(Backend activeBackend READ activeBackend WRITE setActiveBackend NOTIFY activeBackendChanged FINAL)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool canRecord READ canRecord NOTIFY canRecordChanged FINAL)
    Q_PROPERTY(qreal audioLevel READ audioLevel NOTIFY audioLevelChanged FINAL)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(QString lastTranscription READ lastTranscription NOTIFY lastTranscriptionChanged FINAL)

public:
    explicit TranscriptionPipeline(QObject* parent = nullptr);
    ~TranscriptionPipeline() override = default;

    void setAudioRecorder(AudioRecorder* recorder);
    void registerBackend(Backend backend, AbstractSttClient* client);
    void setLlmClient(GroqLlmClient* llmClient);

    State state() const;
    Backend activeBackend() const;
    void setActiveBackend(Backend backend);

    bool isBusy() const;
    bool canRecord() const;
    qreal audioLevel() const;
    QString statusMessage() const;
    QString lastError() const;
    QString lastTranscription() const;

    AbstractSttClient* activeSttClient() const;

public slots:
    void initialize();
    void startRecording();
    void stopRecording();
    void toggleRecording();
    void cancel();
    void retry();
    void clearError();
    void clearLastTranscription();

signals:
    void stateChanged(TranscriptionPipeline::State newState);
    void activeBackendChanged();
    void canRecordChanged();
    void audioLevelChanged(qreal level);
    void statusMessageChanged();
    void lastErrorChanged();
    void lastTranscriptionChanged();

    void transcriptionFinished(const QString& text);
    void errorOccurred(const QString& error);
    void maxDurationWarningTriggered();
    void llmFallbackWarningTriggered(const QString& warning);

private slots:
    void onRecordingFinished(const QByteArray& wavData);
    void onMaxDurationReached();
    void onSttTranscriptionReady(const QString& text);
    void onSttError(const QString& error);
    void onLlmEnhancementReady(const QString& enhancedText);
    void onLlmError(const QString& error, const QString& fallbackRawText);
    void updatePipelineHealth();

private:
    void setState(State state);
    void setStatusMessage(const QString& msg);
    void setLastError(const QString& error);
    void completeTranscription(const QString& text);

    AudioRecorder* m_recorder = nullptr;
    QHash<Backend, AbstractSttClient*> m_sttClients;
    GroqLlmClient* m_llmClient = nullptr;

    Backend m_activeBackend = Backend::Groq;
    State m_state = State::Idle;
    QString m_statusMessage;
    QString m_lastError;
    QString m_lastTranscription;
    QByteArray m_lastWavData;

    bool m_initialized = false;

    friend class TestTranscriptionPipeline;
};
