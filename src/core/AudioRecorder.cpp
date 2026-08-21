#include "AudioRecorder.h"

#include "LoggingCategories.h"

#include <QAudioDevice>
#include <QAudioSource>
#include <QDataStream>
#include <QMediaDevices>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace Qt::StringLiterals;

AudioRecorder::AudioRecorder(QObject* parent)
    : QObject(parent)
    , m_maxDurationTimer(new QTimer(this)) {
    m_format.setSampleRate(kSampleRate);
    m_format.setChannelCount(kChannelCount);
    m_format.setSampleFormat(QAudioFormat::Int16);

    m_hasAudioInputDevice = !QMediaDevices::defaultAudioInput().isNull();

    connect(m_maxDurationTimer, &QTimer::timeout, this, [this]() {
        if (m_recording) {
            qCDebug(lcAudio) << "AudioRecorder: Maximum recording duration of 5 minutes reached. Auto-stopping...";
            emit maxDurationReached();
            stopRecording();
        }
    });
    m_maxDurationTimer->setSingleShot(true);

    auto* devices = new QMediaDevices(this);
    connect(devices, &QMediaDevices::audioInputsChanged, this, &AudioRecorder::onAudioInputsChanged);
}

AudioRecorder::~AudioRecorder() {
    if (m_recording) {
        cancelRecording();
    }
}

bool AudioRecorder::recording() const {
    return m_recording;
}

bool AudioRecorder::hasAudioInputDevice() const {
    return m_hasAudioInputDevice;
}

QString AudioRecorder::statusMessage() const {
    return m_statusMessage;
}

qreal AudioRecorder::audioLevel() const {
    return m_audioLevel;
}

void AudioRecorder::onAudioInputsChanged() {
    const bool hasDevice = !QMediaDevices::defaultAudioInput().isNull();
    if (m_hasAudioInputDevice != hasDevice) {
        m_hasAudioInputDevice = hasDevice;
        qCDebug(lcAudio) << "AudioRecorder: Audio input device availability changed ->" << m_hasAudioInputDevice;
        emit hasAudioInputDeviceChanged();
    }
}

void AudioRecorder::startRecording() {
    if (m_recording) {
        qCDebug(lcAudio) << "startRecording ignored: already recording";
        return;
    }

    QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    if (inputDevice.isNull()) {
        qWarning() << "AudioRecorder: No default audio input device found";
        m_hasAudioInputDevice = false;
        emit hasAudioInputDeviceChanged();
        setStatusMessage(u"No microphone found"_s);
        return;
    }

    m_hasAudioInputDevice = true;
    emit hasAudioInputDeviceChanged();

    qCDebug(lcAudio) << "Using audio input device:" << inputDevice.description()
                     << "Preferred format sample rate:" << inputDevice.preferredFormat().sampleRate();

    m_format.setSampleRate(kSampleRate);
    m_format.setChannelCount(kChannelCount);
    m_format.setSampleFormat(QAudioFormat::Int16);

    if (!inputDevice.isFormatSupported(m_format)) {
        qWarning("AudioRecorder: requested format not supported, using device preferred format");
        m_format = inputDevice.preferredFormat();
        QAudioFormat monoFormat = m_format;
        monoFormat.setChannelCount(kChannelCount);
        if (inputDevice.isFormatSupported(monoFormat)) {
            m_format = monoFormat;
        }
    }

    qCDebug(lcAudio) << "Audio format setup -> SampleRate:" << m_format.sampleRate()
                     << "Channels:" << m_format.channelCount() << "SampleFormat:" << m_format.sampleFormat();

    m_pcmData.clear();
    m_pcmData.reserve(m_format.sampleRate() * m_format.channelCount() * m_format.bytesPerSample() * 30);

    delete m_source;
    m_source = new QAudioSource(inputDevice, m_format, this);

    connect(m_source, &QAudioSource::stateChanged, this, [this](QAudio::State state) {
        if (state == QAudio::StoppedState && m_recording) {
            if (m_source && m_source->error() != QAudio::NoError) {
                qWarning() << "AudioRecorder: QAudioSource encountered error state:" << m_source->error();
                // Device disconnected mid-recording -> gracefully stop and finalize captured partial data
                stopRecording();
            }
        }
    });

    m_ioDevice = m_source->start();
    if (!m_ioDevice) {
        qWarning() << "AudioRecorder: Failed to start QAudioSource IO device";
        setStatusMessage(u"Failed to start audio capture"_s);
        delete m_source;
        m_source = nullptr;
        return;
    }

    connect(m_ioDevice, &QIODevice::readyRead, this, &AudioRecorder::onReadyRead);

    setRecording(true);
    setAudioLevel(0.0);
    setStatusMessage(u"Recording…"_s);
    m_maxDurationTimer->start(kMaxRecordingDuration);
    qCDebug(lcAudio) << "Recording started successfully (5m safety ceiling active)";
}

void AudioRecorder::stopRecording() {
    if (!m_recording) {
        qCDebug(lcAudio) << "stopRecording ignored: not currently recording";
        return;
    }

    m_maxDurationTimer->stop();
    qCDebug(lcAudio) << "Stopping audio recording...";

    if (m_ioDevice) {
        QByteArray remaining = m_ioDevice->readAll();
        if (!remaining.isEmpty()) {
            m_pcmData.append(remaining);
        }
    }

    if (m_source) {
        m_source->stop();
        delete m_source;
        m_source = nullptr;
    }
    m_ioDevice = nullptr;

    setRecording(false);
    setAudioLevel(0.0);

    if (m_pcmData.isEmpty()) {
        qCDebug(lcAudio) << "Audio recording stopped: zero bytes captured";
        setStatusMessage(u"No audio captured"_s);
        return;
    }

    QByteArray wavData = buildWavFile(m_pcmData);

    const qreal durationSecs = static_cast<qreal>(m_pcmData.size()) /
                               (m_format.sampleRate() * m_format.channelCount() * m_format.bytesPerSample());
    qCDebug(lcAudio) << "Audio recording stopped -> PCM bytes:" << m_pcmData.size()
                     << "Duration:" << QString::number(durationSecs, 'f', 2) << "s"
                     << "WAV total size:" << wavData.size() << "bytes";

    setStatusMessage(u"Captured %1s of audio (%2 KB)"_s.arg(durationSecs, 0, 'f', 1).arg(wavData.size() / 1024));

    m_pcmData.clear();
    m_pcmData.squeeze();
    emit recordingFinished(wavData);
}

void AudioRecorder::cancelRecording() {
    if (!m_recording) {
        return;
    }

    m_maxDurationTimer->stop();
    qCDebug(lcAudio) << "Cancelling audio recording...";

    if (m_source) {
        m_source->stop();
        delete m_source;
        m_source = nullptr;
    }
    m_ioDevice = nullptr;

    m_pcmData.clear();
    m_pcmData.squeeze();

    setRecording(false);
    setAudioLevel(0.0);
    setStatusMessage(u"Recording cancelled"_s);
}

void AudioRecorder::onReadyRead() {
    if (!m_ioDevice) {
        return;
    }

    QByteArray chunk = m_ioDevice->readAll();
    if (chunk.isEmpty()) {
        return;
    }

    m_pcmData.append(chunk);

    double sumSquares = 0.0;
    qsizetype sampleCount = 0;

    const auto sampleFormat = m_format.sampleFormat();
    if (sampleFormat == QAudioFormat::Float) {
        sampleCount = chunk.size() / sizeof(float);
        if (sampleCount > 0) {
            const auto* samples = reinterpret_cast<const float*>(chunk.constData());
            for (qsizetype i = 0; i < sampleCount; ++i) {
                const double val = static_cast<double>(samples[i]);
                sumSquares += val * val;
            }
        }
    } else if (sampleFormat == QAudioFormat::Int32) {
        sampleCount = chunk.size() / sizeof(qint32);
        if (sampleCount > 0) {
            const auto* samples = reinterpret_cast<const qint32*>(chunk.constData());
            for (qsizetype i = 0; i < sampleCount; ++i) {
                const double val = static_cast<double>(samples[i]) / 2147483647.0;
                sumSquares += val * val;
            }
        }
    } else if (sampleFormat == QAudioFormat::UInt8) {
        sampleCount = chunk.size() / sizeof(quint8);
        if (sampleCount > 0) {
            const auto* samples = reinterpret_cast<const quint8*>(chunk.constData());
            for (qsizetype i = 0; i < sampleCount; ++i) {
                const double val = (static_cast<double>(samples[i]) - 128.0) / 128.0;
                sumSquares += val * val;
            }
        }
    } else {
        sampleCount = chunk.size() / sizeof(qint16);
        if (sampleCount > 0) {
            const auto* samples = reinterpret_cast<const qint16*>(chunk.constData());
            for (qsizetype i = 0; i < sampleCount; ++i) {
                const double val = static_cast<double>(samples[i]) / 32767.0;
                sumSquares += val * val;
            }
        }
    }

    if (sampleCount == 0) {
        return;
    }

    const double rms = std::sqrt(sumSquares / sampleCount);
    setAudioLevel(std::clamp(rms, 0.0, 1.0));
}

QByteArray AudioRecorder::buildWavFile(const QByteArray& pcmData) const {
    const quint32 dataSize = static_cast<quint32>(pcmData.size());
    const quint32 fileSize = 36 + dataSize;
    const quint16 audioFormat = (m_format.sampleFormat() == QAudioFormat::Float) ? 3 : 1;
    const quint16 numChannels = static_cast<quint16>(m_format.channelCount());
    const quint32 sampleRate = static_cast<quint32>(m_format.sampleRate());
    const quint16 bitsPerSample = static_cast<quint16>(m_format.bytesPerSample() * 8);
    const quint16 blockAlign = numChannels * (bitsPerSample / 8);
    const quint32 byteRate = sampleRate * blockAlign;

    QByteArray header;
    header.reserve(44);
    QDataStream stream(&header, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream.writeRawData("RIFF", 4);
    stream << fileSize;
    stream.writeRawData("WAVE", 4);

    stream.writeRawData("fmt ", 4);
    stream << quint32(16);
    stream << audioFormat;
    stream << numChannels;
    stream << sampleRate;
    stream << byteRate;
    stream << blockAlign;
    stream << bitsPerSample;

    stream.writeRawData("data", 4);
    stream << dataSize;

    QByteArray wavFile;
    wavFile.reserve(44 + pcmData.size());
    wavFile.append(header);
    wavFile.append(pcmData);
    return wavFile;
}

void AudioRecorder::setRecording(bool recording) {
    if (m_recording != recording) {
        m_recording = recording;
        emit recordingChanged();
    }
}

void AudioRecorder::setStatusMessage(const QString& message) {
    if (m_statusMessage != message) {
        m_statusMessage = message;
        emit statusMessageChanged();
    }
}

void AudioRecorder::setAudioLevel(qreal level) {
    if (!qFuzzyCompare(m_audioLevel, level)) {
        m_audioLevel = level;
        emit audioLevelChanged();
    }
}
