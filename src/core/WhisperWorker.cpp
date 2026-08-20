#include "WhisperWorker.h"

#include "LoggingCategories.h"

#include <QFile>
#include <QFileInfo>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <whisper.h>

#define MA_NO_DEVICE_IO
#define MA_NO_THREADING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

using namespace Qt::StringLiterals;

namespace {

struct AbortableModelLoaderContext {
    std::ifstream file;
    const WhisperWorker* worker = nullptr;
    uint64_t loadRequestId = 0;
};

} // namespace

bool WhisperWorker::extractPcmSamples(const QByteArray& wavData, std::vector<float>& outPcmf32) {
    if (wavData.size() < 12) {
        return false;
    }

    // 1. Try decoding with miniaudio configured to 1-channel mono at WHISPER_SAMPLE_RATE (16 kHz).
    // miniaudio automatically handles format conversion (Float32, Int32, Int16, UInt8),
    // channel downmixing (stereo/multichannel -> mono), and anti-aliased resampling.
    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 1, WHISPER_SAMPLE_RATE);
    ma_decoder decoder;

    if (ma_decoder_init_memory(wavData.constData(), static_cast<size_t>(wavData.size()), &decoderConfig, &decoder) ==
        MA_SUCCESS) {
        ma_uint64 frameCount = 0;
        if (ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount) == MA_SUCCESS && frameCount > 0) {
            outPcmf32.resize(static_cast<size_t>(frameCount));
            ma_uint64 framesRead = 0;
            if (ma_decoder_read_pcm_frames(&decoder, outPcmf32.data(), frameCount, &framesRead) == MA_SUCCESS &&
                framesRead > 0) {
                outPcmf32.resize(static_cast<size_t>(framesRead));
                ma_decoder_uninit(&decoder);
                return true;
            }
        }
        ma_decoder_uninit(&decoder);
    }

    // 2. Fallback manual WAV parser for custom/raw PCM streams
    const char* data = wavData.constData();
    if (std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) {
        return false;
    }

    int pos = 12;
    int dataOffset = -1;
    uint32_t dataBytes = 0;
    uint16_t audioFormat = 1;
    uint16_t numChannels = 1;
    uint32_t sampleRate = 16000;
    uint16_t bitsPerSample = 16;

    while (pos + 8 <= wavData.size()) {
        const char* chunkId = data + pos;
        uint32_t chunkSize = 0;
        std::memcpy(&chunkSize, data + pos + 4, 4);
        pos += 8;

        if (std::memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16 && pos + 16 <= wavData.size()) {
            std::memcpy(&audioFormat, data + pos, 2);
            std::memcpy(&numChannels, data + pos + 2, 2);
            std::memcpy(&sampleRate, data + pos + 4, 4);
            std::memcpy(&bitsPerSample, data + pos + 14, 2);
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            dataOffset = pos;
            dataBytes = std::min<uint32_t>(chunkSize, static_cast<uint32_t>(wavData.size() - pos));
            break;
        }

        pos += chunkSize + (chunkSize % 2);
    }

    if (dataOffset < 0 || dataBytes < sizeof(int16_t)) {
        if (wavData.size() > 44) {
            dataOffset = 44;
            dataBytes = static_cast<uint32_t>(wavData.size() - 44);
        } else {
            return false;
        }
    }

    if (numChannels == 0) {
        numChannels = 1;
    }

    const int bytesPerSample = bitsPerSample / 8;
    if (bytesPerSample <= 0) {
        return false;
    }

    const int totalSamples = static_cast<int>(dataBytes / bytesPerSample);
    const int frameCount = totalSamples / numChannels;
    if (frameCount <= 0) {
        return false;
    }

    std::vector<float> monoPcm(frameCount);
    const char* samplePtr = data + dataOffset;

    for (int f = 0; f < frameCount; ++f) {
        float sum = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            const int sampleIdx = (f * numChannels + ch) * bytesPerSample;
            if (audioFormat == 3 && bitsPerSample == 32) { // IEEE Float32
                float val = 0.0f;
                std::memcpy(&val, samplePtr + sampleIdx, sizeof(float));
                sum += val;
            } else if (bitsPerSample == 32) { // 32-bit PCM
                int32_t val = 0;
                std::memcpy(&val, samplePtr + sampleIdx, sizeof(int32_t));
                sum += static_cast<float>(val) / 2147483648.0f;
            } else if (bitsPerSample == 16) { // 16-bit PCM
                int16_t val = 0;
                std::memcpy(&val, samplePtr + sampleIdx, sizeof(int16_t));
                sum += static_cast<float>(val) / 32768.0f;
            } else if (bitsPerSample == 8) { // 8-bit PCM
                const uint8_t val = static_cast<uint8_t>(samplePtr[sampleIdx]);
                sum += (static_cast<float>(val) - 128.0f) / 128.0f;
            }
        }
        monoPcm[f] = std::clamp(sum / static_cast<float>(numChannels), -1.0f, 1.0f);
    }

    // Resampling fallback if sample rate is not 16000 Hz
    if (sampleRate != WHISPER_SAMPLE_RATE && sampleRate > 0) {
        const double ratio = static_cast<double>(sampleRate) / static_cast<double>(WHISPER_SAMPLE_RATE);
        const int outSampleCount = static_cast<int>(static_cast<double>(frameCount) / ratio);
        outPcmf32.resize(outSampleCount);
        for (int i = 0; i < outSampleCount; ++i) {
            const double srcIdx = i * ratio;
            const int idx0 = static_cast<int>(srcIdx);
            const int idx1 = std::min(idx0 + 1, frameCount - 1);
            const float frac = static_cast<float>(srcIdx - idx0);
            outPcmf32[i] = monoPcm[idx0] * (1.0f - frac) + monoPcm[idx1] * frac;
        }
    } else {
        outPcmf32 = std::move(monoPcm);
    }

    return !outPcmf32.empty();
}

WhisperWorker::WhisperWorker(QObject* parent)
    : QObject(parent) { }

WhisperWorker::~WhisperWorker() {
    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
    }
}

void WhisperWorker::loadModel(uint64_t loadRequestId, const QString& modelPath, bool useGpu) {
    if (isLoadAborted(loadRequestId)) {
        qCDebug(lcSpeech) << "WhisperWorker: Model load request" << loadRequestId << "cancelled prior to processing";
        return;
    }

    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
        m_activeDevice.clear();
    }

    if (!QFile::exists(modelPath)) {
        qWarning() << "WhisperWorker: Model file does not exist at:" << modelPath;
        emit modelLoaded(loadRequestId, modelPath, false, tr("Whisper model file not found at %1").arg(modelPath),
                         QString());
        return;
    }

    qCDebug(lcSpeech) << "WhisperWorker: Initializing whisper.cpp context from" << modelPath
                      << "(requested GPU:" << useGpu << ", loadRequestId:" << loadRequestId << ")";

    std::ifstream fin(modelPath.toStdString(), std::ios::binary);
    if (!fin.is_open()) {
        qWarning() << "WhisperWorker: Failed to open model file at:" << modelPath;
        emit modelLoaded(loadRequestId, modelPath, false, tr("Failed to open model file at %1").arg(modelPath),
                         QString());
        return;
    }

    AbortableModelLoaderContext loaderCtx;
    loaderCtx.file = std::move(fin);
    loaderCtx.worker = this;
    loaderCtx.loadRequestId = loadRequestId;

    whisper_model_loader loader = {};
    loader.context = &loaderCtx;

    loader.read = [](void* ctx, void* output, size_t read_size) -> size_t {
        auto* lctx = static_cast<AbortableModelLoaderContext*>(ctx);
        if (!lctx || !lctx->file.is_open()) {
            return 0;
        }
        if (lctx->worker && lctx->worker->isLoadAborted(lctx->loadRequestId)) {
            lctx->file.close();
            return 0;
        }
        lctx->file.read(static_cast<char*>(output), static_cast<std::streamsize>(read_size));
        if (!lctx->file && lctx->file.bad()) {
            lctx->file.close();
            return 0;
        }
        return static_cast<size_t>(lctx->file.gcount());
    };

    loader.eof = [](void* ctx) -> bool {
        auto* lctx = static_cast<AbortableModelLoaderContext*>(ctx);
        if (!lctx || !lctx->file.is_open()) {
            return true;
        }
        if (lctx->worker && lctx->worker->isLoadAborted(lctx->loadRequestId)) {
            return true;
        }
        return lctx->file.eof();
    };

    loader.close = [](void* ctx) -> void {
        auto* lctx = static_cast<AbortableModelLoaderContext*>(ctx);
        if (lctx && lctx->file.is_open()) {
            lctx->file.close();
        }
    };

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = useGpu;
    cparams.gpu_device = 0;
    cparams.flash_attn = false;

    [[maybe_unused]] bool gpuFallback = false;
    try {
        m_ctx = whisper_init_with_params(&loader, cparams);
    } catch (const std::exception& e) {
        qWarning() << "WhisperWorker: Exception during whisper_init_with_params:" << e.what();
        m_ctx = nullptr;
    } catch (...) {
        qWarning() << "WhisperWorker: Unknown exception during whisper_init_with_params";
        m_ctx = nullptr;
    }

    if (!m_ctx && useGpu && !isLoadAborted(loadRequestId)) {
        qWarning() << "WhisperWorker: GPU initialization failed for" << modelPath
                   << ". Falling back to CPU inference...";
        gpuFallback = true;
        if (loaderCtx.file.is_open()) {
            loaderCtx.file.clear();
            loaderCtx.file.seekg(0, std::ios::beg);
        } else {
            loaderCtx.file.open(modelPath.toStdString(), std::ios::binary);
        }

        if (loaderCtx.file.is_open()) {
            cparams.use_gpu = false;
            try {
                m_ctx = whisper_init_with_params(&loader, cparams);
            } catch (const std::exception& e) {
                qWarning() << "WhisperWorker: Exception during CPU fallback whisper_init_with_params:" << e.what();
                m_ctx = nullptr;
            } catch (...) {
                qWarning() << "WhisperWorker: Unknown exception during CPU fallback whisper_init_with_params";
                m_ctx = nullptr;
            }
        }
    }

    if (isLoadAborted(loadRequestId)) {
        qCDebug(lcSpeech) << "WhisperWorker: Model loading cancelled for request" << loadRequestId;
        if (m_ctx) {
            whisper_free(m_ctx);
            m_ctx = nullptr;
            m_activeDevice.clear();
        }
        return;
    }

    if (!m_ctx) {
        qWarning() << "WhisperWorker: whisper_init_with_params failed for" << modelPath;
        emit modelLoaded(loadRequestId, modelPath, false, tr("Failed to initialize whisper model context"), QString());
        return;
    }

#if defined(GGML_USE_VULKAN)
    if (useGpu && !gpuFallback) {
        m_activeDevice = u"GPU (Vulkan)"_s;
    } else if (gpuFallback) {
        m_activeDevice = u"CPU (Fallback)"_s;
    } else {
        m_activeDevice = u"CPU"_s;
    }
#else
    m_activeDevice = u"CPU"_s;
#endif

    qCDebug(lcSpeech) << "WhisperWorker: Model loaded successfully. Active device:" << m_activeDevice;
    emit modelLoaded(loadRequestId, modelPath, true, QString(), m_activeDevice);
}

void WhisperWorker::unloadModel() {
    cancel();
    if (m_ctx) {
        qCDebug(lcSpeech) << "WhisperWorker: Unloading whisper context";
        whisper_free(m_ctx);
        m_ctx = nullptr;
        m_activeDevice.clear();
        emit modelUnloaded();
    }
}

void WhisperWorker::resetAbort() {
    m_abortRequested.store(false, std::memory_order_release);
}

void WhisperWorker::cancel(uint64_t requestId) {
    if (requestId == 0) {
        m_abortRequested.store(true, std::memory_order_release);
    } else {
        uint64_t current = m_cancelledRequestId.load(std::memory_order_relaxed);
        while (current < requestId && !m_cancelledRequestId.compare_exchange_weak(
                                          current, requestId, std::memory_order_release, std::memory_order_relaxed)) { }
    }
}

void WhisperWorker::cancelLoad(uint64_t loadRequestId) {
    if (loadRequestId == 0) {
        m_abortRequested.store(true, std::memory_order_release);
    } else {
        uint64_t current = m_cancelledLoadRequestId.load(std::memory_order_relaxed);
        while (current < loadRequestId &&
               !m_cancelledLoadRequestId.compare_exchange_weak(current, loadRequestId, std::memory_order_release,
                                                               std::memory_order_relaxed)) { }
    }
}

bool WhisperWorker::isAborted(uint64_t requestId) const {
    if (m_abortRequested.load(std::memory_order_acquire)) {
        return true;
    }
    if (QThread::currentThread() && QThread::currentThread()->isInterruptionRequested()) {
        return true;
    }
    if (requestId > 0 && requestId <= m_cancelledRequestId.load(std::memory_order_acquire)) {
        return true;
    }
    return false;
}

bool WhisperWorker::isLoadAborted(uint64_t loadRequestId) const {
    if (m_abortRequested.load(std::memory_order_acquire)) {
        return true;
    }
    if (QThread::currentThread() && QThread::currentThread()->isInterruptionRequested()) {
        return true;
    }
    if (loadRequestId > 0 && loadRequestId <= m_cancelledLoadRequestId.load(std::memory_order_acquire)) {
        return true;
    }
    return false;
}

void WhisperWorker::transcribe(uint64_t requestId, const QByteArray& wavData, const QString& language,
                               const QString& prompt) {
    if (requestId > 0 && requestId <= m_cancelledRequestId.load(std::memory_order_acquire)) {
        qCDebug(lcSpeech) << "WhisperWorker: Transcription request" << requestId << "cancelled prior to processing";
        return;
    }

    if (isAborted(requestId)) {
        qCDebug(lcSpeech) << "WhisperWorker: Transcription request" << requestId << "cancelled prior to processing";
        return;
    }

    if (!m_ctx) {
        qWarning() << "WhisperWorker: Transcribe called but model is not loaded";
        emit transcriptionFailed(requestId, tr("Offline Whisper model is not loaded"));
        return;
    }

    std::vector<float> pcmf32;
    if (!extractPcmSamples(wavData, pcmf32)) {
        qWarning() << "WhisperWorker: Invalid or unsupported audio format in WAV payload";
        emit transcriptionFailed(requestId, tr("Invalid audio data: unable to extract PCM samples"));
        return;
    }

    if (isAborted(requestId)) {
        qCDebug(lcSpeech) << "WhisperWorker: Transcription request" << requestId << "cancelled prior to inference";
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

    struct AbortContext {
        WhisperWorker* worker;
        uint64_t reqId;
    } abortCtx {this, requestId};

    wparams.abort_callback = [](void* userData) -> bool {
        auto* ctx = static_cast<AbortContext*>(userData);
        return ctx && ctx->worker && ctx->worker->isAborted(ctx->reqId);
    };
    wparams.abort_callback_user_data = &abortCtx;

    wparams.encoder_begin_callback = [](whisper_context* /*ctx*/, whisper_state* /*state*/, void* userData) -> bool {
        auto* ctx = static_cast<AbortContext*>(userData);
        return !ctx || !ctx->worker || !ctx->worker->isAborted(ctx->reqId);
    };
    wparams.encoder_begin_callback_user_data = &abortCtx;

    const int sampleCount = static_cast<int>(pcmf32.size());
    qCDebug(lcSpeech) << "WhisperWorker: Running inference for request" << requestId << "on" << sampleCount
                      << "samples (~" << (sampleCount / 16000.0f) << "s of audio) with" << wparams.n_threads
                      << "threads (lang:" << wparams.language << ")";

    int ret = whisper_full(m_ctx, wparams, pcmf32.data(), static_cast<int>(pcmf32.size()));

    if (isAborted(requestId)) {
        qCDebug(lcSpeech) << "WhisperWorker: Transcription request" << requestId << "cancelled during/after inference";
        return;
    }

    if (ret != 0) {
        qWarning() << "WhisperWorker: whisper_full failed with return code:" << ret;
        emit transcriptionFailed(requestId, tr("Whisper inference failed (code: %1)").arg(ret));
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
    qCDebug(lcSpeech) << "WhisperWorker: Transcription request" << requestId << "completed successfully ("
                      << transcription.size() << "chars)";
    emit transcriptionFinished(requestId, transcription);
}
