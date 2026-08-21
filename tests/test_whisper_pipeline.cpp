#include <QAudioFormat>
#include <QBuffer>
#include <QByteArray>
#include <QCoreApplication>
#include <QDataStream>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "WhisperModelManager.h"
#include "WhisperSttClient.h"
#include "WhisperWorker.h"

#include <cmath>
#include <vector>

using namespace Qt::StringLiterals;

namespace {
QByteArray createWav(uint32_t sampleRate, uint16_t numChannels, uint16_t bitsPerSample, uint16_t audioFormat,
                     const QByteArray& pcmData) {
    const uint32_t dataSize = static_cast<uint32_t>(pcmData.size());
    const uint32_t fileSize = 36 + dataSize;
    const uint16_t blockAlign = numChannels * (bitsPerSample / 8);
    const uint32_t byteRate = sampleRate * blockAlign;

    QByteArray wav;
    wav.reserve(44 + pcmData.size());
    QDataStream stream(&wav, QIODevice::WriteOnly);
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

    wav.append(pcmData);
    return wav;
}
} // namespace

class TestWhisperPipeline : public QObject {
    Q_OBJECT

private slots:
    void testAudioResampling_48kFloatTo16kMono() {
        // 1 second of 48 kHz float audio (48,000 samples)
        constexpr int sampleRate = 48000;
        constexpr int frameCount = 48000;
        QByteArray pcm;
        pcm.resize(frameCount * sizeof(float));
        auto* floatPtr = reinterpret_cast<float*>(pcm.data());

        for (int i = 0; i < frameCount; ++i) {
            floatPtr[i] = 0.5f * std::sin(2.0f * M_PI * 440.0f * i / sampleRate);
        }

        const QByteArray wav = createWav(sampleRate, 1, 32, 3, pcm); // audioFormat 3 = IEEE Float
        std::vector<float> pcmf32;
        const bool success = WhisperWorker::extractPcmSamples(wav, pcmf32);

        QVERIFY(success);
        // Resampled to 16 kHz -> expect ~16000 samples (+/- 5 samples tolerance for filter delay)
        QVERIFY(std::abs(static_cast<int>(pcmf32.size()) - 16000) <= 5);

        for (float s : pcmf32) {
            QVERIFY(s >= -1.05f && s <= 1.05f);
        }
    }

    void testAudioResampling_44kStereoTo16kMono() {
        // 1 second of 44.1 kHz 16-bit stereo audio (44,100 frames * 2 channels = 88,200 samples)
        constexpr int sampleRate = 44100;
        constexpr int frameCount = 44100;
        QByteArray pcm;
        pcm.resize(frameCount * 2 * sizeof(int16_t));
        auto* shortPtr = reinterpret_cast<int16_t*>(pcm.data());

        for (int i = 0; i < frameCount; ++i) {
            const int16_t val = static_cast<int16_t>(16000.0f * std::sin(2.0f * M_PI * 440.0f * i / sampleRate));
            shortPtr[2 * i] = val;     // Left
            shortPtr[2 * i + 1] = val; // Right
        }

        const QByteArray wav = createWav(sampleRate, 2, 16, 1, pcm); // 1 = PCM
        std::vector<float> pcmf32;
        const bool success = WhisperWorker::extractPcmSamples(wav, pcmf32);

        QVERIFY(success);
        // Resampled from 44.1 kHz to 16 kHz -> expect ~16000 samples
        QVERIFY(std::abs(static_cast<int>(pcmf32.size()) - 16000) <= 10);

        for (float s : pcmf32) {
            QVERIFY(s >= -1.05f && s <= 1.05f);
        }
    }

    void testAudioResampling_48kInt32To16kMono() {
        // 1 second of 48 kHz 32-bit int audio
        constexpr int sampleRate = 48000;
        constexpr int frameCount = 48000;
        QByteArray pcm;
        pcm.resize(frameCount * sizeof(int32_t));
        auto* intPtr = reinterpret_cast<int32_t*>(pcm.data());

        for (int i = 0; i < frameCount; ++i) {
            intPtr[i] = static_cast<int32_t>(1000000000.0f * std::sin(2.0f * M_PI * 440.0f * i / sampleRate));
        }

        const QByteArray wav = createWav(sampleRate, 1, 32, 1, pcm);
        std::vector<float> pcmf32;
        const bool success = WhisperWorker::extractPcmSamples(wav, pcmf32);

        QVERIFY(success);
        QVERIFY(std::abs(static_cast<int>(pcmf32.size()) - 16000) <= 5);
    }

    void testAudioPassthrough_16kInt16Mono() {
        // Native 16 kHz 16-bit mono audio
        constexpr int sampleRate = 16000;
        constexpr int frameCount = 16000;
        QByteArray pcm;
        pcm.resize(frameCount * sizeof(int16_t));
        auto* shortPtr = reinterpret_cast<int16_t*>(pcm.data());

        for (int i = 0; i < frameCount; ++i) {
            shortPtr[i] = static_cast<int16_t>(16000.0f * std::sin(2.0f * M_PI * 440.0f * i / sampleRate));
        }

        const QByteArray wav = createWav(sampleRate, 1, 16, 1, pcm);
        std::vector<float> pcmf32;
        const bool success = WhisperWorker::extractPcmSamples(wav, pcmf32);

        QVERIFY(success);
        QCOMPARE(static_cast<int>(pcmf32.size()), 16000);
    }

    void testModelSelectionSynchronization() {
        WhisperModelManager manager;
        WhisperSttClient client;

        client.setModelManager(&manager);

        const QString initialId = manager.selectedModelId();
        QVERIFY(!initialId.isEmpty());
        QCOMPARE(client.modelPath(), manager.selectedModelPath());

        // Since no model is actually loaded in this headless test, model is not loaded and client is not ready
        QVERIFY(!client.isModelLoaded());
        QVERIFY(!client.isReady());

        // Switch selection to another model if available
        if (manager.rowCount() > 1) {
            const QString secondId = manager.data(manager.index(1), WhisperModelManager::IdRole).toString();
            manager.setSelectedModelId(secondId);
            QCOMPARE(manager.selectedModelId(), secondId);
            QCOMPARE(client.modelPath(), manager.selectedModelPath());
        }
    }

    void testStaleModelLoadHandling() {
        WhisperSttClient client;

        QVERIFY(!client.isModelLoaded());
        QVERIFY(client.loadedModelPath().isEmpty());

        client.loadModel(u"/nonexistent/dummy/model.bin"_s);
        QVERIFY(!client.isModelLoaded());
        QVERIFY(client.loadedModelPath().isEmpty());

        client.unloadModel();
        QVERIFY(!client.isModelLoaded());
        QVERIFY(client.loadedModelPath().isEmpty());
    }

    void testWorkerAbortLogic() {
        WhisperWorker worker;

        QVERIFY(!worker.isAborted(1));
        QVERIFY(!worker.isAborted(0));
        QVERIFY(!worker.isLoadAborted(1));
        QVERIFY(!worker.isLoadAborted(0));

        // Cancel specific transcribe request ID (42)
        worker.cancel(42);
        QVERIFY(worker.isAborted(42));
        QVERIFY(worker.isAborted(10));
        QVERIFY(!worker.isAborted(50));
        QVERIFY(!worker.isAborted(0));
        QVERIFY(!worker.isLoadAborted(1));

        // Cancel specific load request ID (5)
        worker.cancelLoad(5);
        QVERIFY(worker.isLoadAborted(5));
        QVERIFY(worker.isLoadAborted(2));
        QVERIFY(!worker.isLoadAborted(10));
        QVERIFY(!worker.isLoadAborted(0));

        // Cancel all globally (requestId = 0)
        worker.cancel(0);
        QVERIFY(worker.isAborted(0));
        QVERIFY(worker.isAborted(100));
        QVERIFY(worker.isLoadAborted(0));
        QVERIFY(worker.isLoadAborted(100));

        // Reset global abort
        worker.resetAbort();
        QVERIFY(!worker.isAborted(0));
        QVERIFY(!worker.isLoadAborted(0));
        QVERIFY(worker.isAborted(42));
        QVERIFY(worker.isLoadAborted(5));
        QVERIFY(!worker.isAborted(50));
        QVERIFY(!worker.isLoadAborted(10));
    }

    void testVulkanSupportReporting() {
        WhisperSttClient client;
#if defined(GGML_USE_VULKAN)
        QVERIFY(client.isVulkanSupported());
#else
        QVERIFY(!client.isVulkanSupported());
#endif
    }

    void testClientLifecycleAndGracefulShutdown() {
        for (int i = 0; i < 3; ++i) {
            auto client = std::make_unique<WhisperSttClient>();
            client->loadModel(u"/tmp/nonexistent_test_model.bin"_s);
            client->cancel();
        }
    }
};

QTEST_GUILESS_MAIN(TestWhisperPipeline)
#include "test_whisper_pipeline.moc"
