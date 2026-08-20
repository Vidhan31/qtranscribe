#include "WhisperWorker.h"

#include "LoggingCategories.h"

#include <QFile>
#include <QFileInfo>
#include <QThread>

#include <algorithm>
#include <cstring>
#include <vector>

#include <whisper.h>

using namespace Qt::StringLiterals;

namespace {
bool extractPcmSamples(const QByteArray& wavData, std::vector<float>& outPcmf32) {
    if (wavData.size() < 12) {
        return false;
    }

    const char* data = wavData.constData();
    if (std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) {
        return false;
    }

    int pos = 12;
    int dataOffset = -1;
    uint32_t dataBytes = 0;
    uint16_t audioFormat = 1;
    uint16_t bitsPerSample = 16;

    while (pos + 8 <= wavData.size()) {
        const char* chunkId = data + pos;
        uint32_t chunkSize = 0;
        std::memcpy(&chunkSize, data + pos + 4, 4);
        pos += 8;

        if (std::memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16 && pos + 16 <= wavData.size()) {
            std::memcpy(&audioFormat, data + pos, 2);
            std::memcpy(&bitsPerSample, data + pos + 14, 2);
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            dataOffset = pos;
            dataBytes = std::min<uint32_t>(chunkSize, static_cast<uint32_t>(wavData.size() - pos));
            break;
        }

        pos += chunkSize + (chunkSize % 2);
    }

    if (dataOffset < 0 || dataBytes < sizeof(int16_t) || audioFormat != 1 || bitsPerSample != 16) {
        // Fallback to standard 44-byte offset if chunk parsing could not resolve PCM data chunk
        if (wavData.size() > 44) {
            dataOffset = 44;
            dataBytes = static_cast<uint32_t>(wavData.size() - 44);
        } else {
            return false;
        }
    }

    const int sampleCount = static_cast<int>(dataBytes / sizeof(int16_t));
    if (sampleCount <= 0) {
        return false;
    }

    const auto* pcm16 = reinterpret_cast<const int16_t*>(data + dataOffset);
    outPcmf32.resize(sampleCount);
    for (int i = 0; i < sampleCount; ++i) {
        outPcmf32[i] = static_cast<float>(pcm16[i]) / 32768.0f;
    }

    return true;
}
} // namespace

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

void WhisperWorker::transcribe(const QByteArray& wavData, const QString& language, const QString& prompt) {
    m_cancelled = false;

    if (!m_ctx) {
        qWarning() << "WhisperWorker: Transcribe called but model is not loaded";
        emit transcriptionFailed(tr("Offline Whisper model is not loaded"));
        return;
    }

    std::vector<float> pcmf32;
    if (!extractPcmSamples(wavData, pcmf32)) {
        qWarning() << "WhisperWorker: Invalid or unsupported audio format in WAV payload";
        emit transcriptionFailed(tr("Invalid audio data: unable to extract 16-bit PCM samples"));
        return;
    }

    if (m_cancelled) {
        qCDebug(lcSpeech) << "WhisperWorker: Transcription cancelled prior to inference";
        return;
    }

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.n_threads = std::clamp(QThread::idealThreadCount(), 1, 4);

    QByteArray langUtf8;
    if (whisper_is_multilingual(m_ctx) == 0) {
        wparams.language = "en";
    } else {
        langUtf8 = language.isEmpty() ? QByteArray("auto") : language.toUtf8();
        wparams.language = langUtf8.constData();
    }

    const QByteArray promptUtf8 = prompt.toUtf8();
    if (!promptUtf8.isEmpty()) {
        wparams.initial_prompt = promptUtf8.constData();
    }

    wparams.single_segment = true;
    wparams.no_timestamps = true;
    wparams.print_special = false;
    wparams.print_progress = false;
    wparams.print_realtime = false;
    wparams.print_timestamps = false;

    const int sampleCount = static_cast<int>(pcmf32.size());
    qCDebug(lcSpeech) << "WhisperWorker: Running inference on" << sampleCount << "samples (~"
                      << (sampleCount / 16000.0f) << "s of audio) with" << wparams.n_threads
                      << "threads (lang:" << wparams.language << ")";

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
