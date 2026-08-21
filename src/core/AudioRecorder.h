#pragma once

#include <QAudioFormat>
#include <QBuffer>
#include <QByteArray>
#include <QObject>
#include <QQmlEngine>
#include <QString>

class QAudioSource;
class QTimer;

class AudioRecorder : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged FINAL)
    Q_PROPERTY(bool hasAudioInputDevice READ hasAudioInputDevice NOTIFY hasAudioInputDeviceChanged FINAL)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged FINAL)
    Q_PROPERTY(qreal audioLevel READ audioLevel NOTIFY audioLevelChanged FINAL)

public:
    explicit AudioRecorder(QObject* parent = nullptr);
    ~AudioRecorder() override;

    bool recording() const;
    bool hasAudioInputDevice() const;
    QString statusMessage() const;
    qreal audioLevel() const;

    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void cancelRecording();

signals:
    void recordingFinished(const QByteArray& wavData);
    void recordingChanged();
    void hasAudioInputDeviceChanged();
    void statusMessageChanged();
    void audioLevelChanged();
    void maxDurationReached();

private slots:
    void onReadyRead();
    void onAudioInputsChanged();

private:
    void setRecording(bool recording);
    void setStatusMessage(const QString& message);
    void setAudioLevel(qreal level);
    QByteArray buildWavFile(const QByteArray& pcmData) const;

    QAudioFormat m_format;
    QAudioSource* m_source = nullptr;
    QIODevice* m_ioDevice = nullptr;
    QByteArray m_pcmData;

    bool m_recording = false;
    bool m_hasAudioInputDevice = false;
    QString m_statusMessage;
    qreal m_audioLevel = 0.0;
    QTimer* m_maxDurationTimer = nullptr;

    static constexpr int kSampleRate = 16000;
    static constexpr int kChannelCount = 1;
    static constexpr int kSampleSize = 16;
    static constexpr auto kMaxRecordingDuration = std::chrono::minutes(5);
};
