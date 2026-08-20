#include "WhisperWorker.h"

#include "LoggingCategories.h"

#include <QFile>
#include <QFileInfo>
#include <QThread>

#include <algorithm>
#include <vector>

#include <whisper.h>

using namespace Qt::StringLiterals;

WhisperWorker::WhisperWorker(QObject* parent)
    : QObject(parent) { }

WhisperWorker::~WhisperWorker() {
    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
    }
}

void WhisperWorker::loadModel(const QString& modelPath, bool useGpu) {
    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
    }

    if (!QFile::exists(modelPath)) {
        qWarning() << "WhisperWorker: Model file does not exist at:" << modelPath;
        emit modelLoaded(false, tr("Whisper model file not found at %1").arg(modelPath), QString());
        return;
    }

    qCDebug(lcSpeech) << "WhisperWorker: Initializing whisper.cpp context from" << modelPath
                      << "(requested GPU:" << useGpu << ")";

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = useGpu;
    cparams.flash_attn = false;

    m_ctx = whisper_init_from_file_with_params(modelPath.toUtf8().constData(), cparams);
    if (!m_ctx) {
        qWarning() << "WhisperWorker: whisper_init_from_file_with_params failed for" << modelPath;
        emit modelLoaded(false, tr("Failed to initialize whisper model context"), QString());
        return;
    }

#if defined(GGML_USE_VULKAN)
    m_activeDevice = useGpu ? u"GPU (Vulkan)"_s : u"CPU"_s;
#else
    m_activeDevice = u"CPU"_s;
#endif

    qCDebug(lcSpeech) << "WhisperWorker: Model loaded successfully. Active device:" << m_activeDevice;
    emit modelLoaded(true, QString(), m_activeDevice);
}

void WhisperWorker::unloadModel() {
    if (m_ctx) {
        qCDebug(lcSpeech) << "WhisperWorker: Unloading whisper context";
        whisper_free(m_ctx);
        m_ctx = nullptr;
        m_activeDevice.clear();
        emit modelUnloaded();
    }
}

void WhisperWorker::cancel() {
    m_cancelled = true;
}

void WhisperWorker::transcribe(const QByteArray& wavData) {
    m_cancelled = false;

    if (!m_ctx) {
        qWarning() << "WhisperWorker: Transcribe called but model is not loaded";
        emit transcriptionFailed(tr("Offline Whisper model is not loaded"));
        return;
    }

    if (wavData.size() <= 44) {
        qWarning() << "WhisperWorker: Audio payload is too small to contain valid audio data";
        emit transcriptionFailed(tr("Invalid audio data: buffer too small"));
        return;
    }

    // Skip the 44-byte standard RIFF WAV header to read 16-bit PCM samples
    const int headerOffset = 44;
    const int rawAudioBytes = wavData.size() - headerOffset;
    const int sampleCount = rawAudioBytes / sizeof(int16_t);

    if (sampleCount <= 0) {
        qWarning() << "WhisperWorker: Zero audio samples extracted";
        emit transcriptionFailed(tr("No audio samples to transcribe"));
        return;
    }

    const auto* pcm16 = reinterpret_cast<const int16_t*>(wavData.constData() + headerOffset);
    std::vector<float> pcmf32(sampleCount);
    for (int i = 0; i < sampleCount; ++i) {
        pcmf32[i] = static_cast<float>(pcm16[i]) / 32768.0f;
    }

    if (m_cancelled) {
        qCDebug(lcSpeech) << "WhisperWorker: Transcription cancelled prior to inference";
        return;
    }

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.n_threads = std::clamp(QThread::idealThreadCount(), 1, 4);
    wparams.language = "en";
    wparams.single_segment = true;
    wparams.no_timestamps = true;
    wparams.print_special = false;
    wparams.print_progress = false;
    wparams.print_realtime = false;
    wparams.print_timestamps = false;

    qCDebug(lcSpeech) << "WhisperWorker: Running inference on" << sampleCount << "samples (~"
                      << (sampleCount / 16000.0f) << "s of audio) with" << wparams.n_threads << "threads";

    int ret = whisper_full(m_ctx, wparams, pcmf32.data(), static_cast<int>(pcmf32.size()));

    if (m_cancelled) {
        qCDebug(lcSpeech) << "WhisperWorker: Transcription cancelled during/after inference";
        return;
    }

    if (ret != 0) {
        qWarning() << "WhisperWorker: whisper_full failed with return code:" << ret;
        emit transcriptionFailed(tr("Whisper inference failed (code: %1)").arg(ret));
        return;
    }

    QString transcription;
    const int n_segments = whisper_full_n_segments(m_ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(m_ctx, i);
        if (text) {
            transcription += QString::fromUtf8(text);
        }
    }

    transcription = transcription.trimmed();
    qCDebug(lcSpeech) << "WhisperWorker: Transcription completed successfully (" << transcription.size() << "chars)";
    emit transcriptionFinished(transcription);
}
