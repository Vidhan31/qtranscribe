#include <QByteArray>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

#include "AbstractSttClient.h"
#include "AudioRecorder.h"
#include "GroqLlmClient.h"
#include "TranscriptionPipeline.h"

class FakeSttClient : public AbstractSttClient {
    Q_OBJECT

public:
    explicit FakeSttClient(QObject* parent = nullptr)
        : AbstractSttClient(parent) { }

    void transcribe(const QByteArray& wavData) override {
        m_lastReceivedWav = wavData;
        m_transcribeCallCount++;
        m_busy = true;
        emit busyChanged();
    }

    void cancel() override {
        m_busy = false;
        m_cancelled = true;
        emit busyChanged();
    }

    void retryLast() override {
        m_retryCallCount++;
        if (!m_lastReceivedWav.isEmpty()) {
            transcribe(m_lastReceivedWav);
        }
    }

    bool isReady() const override {
        return m_ready;
    }

    bool isBusy() const override {
        return m_busy;
    }

    void setReady(bool ready) {
        if (m_ready != ready) {
            m_ready = ready;
            emit readyChanged();
        }
    }

    void simulateSuccess(const QString& text) {
        m_busy = false;
        emit busyChanged();
        emit transcriptionReady(text);
    }

    void simulateError(const QString& error) {
        m_busy = false;
        m_lastError = error;
        emit busyChanged();
        emit errorOccurred(error);
    }

    QString lastError() const override {
        return m_lastError;
    }

    void setNotice(const QVariantMap& noticeMap) {
        m_notice = noticeMap;
        emit noticeChanged();
    }

    bool hasNotice() const override {
        return !m_notice.isEmpty();
    }

    QVariantMap notice() const override {
        return m_notice;
    }

    QByteArray m_lastReceivedWav;
    QString m_lastError;
    QVariantMap m_notice;
    int m_transcribeCallCount = 0;
    int m_retryCallCount = 0;
    bool m_ready = true;
    bool m_busy = false;
    bool m_cancelled = false;
};

class TestTranscriptionPipeline : public QObject {
    Q_OBJECT

private slots:
    void testInitialState() {
        TranscriptionPipeline pipeline;
        QCOMPARE(pipeline.state(), TranscriptionPipeline::State::Idle);
        QCOMPARE(pipeline.isBusy(), false);
        QCOMPARE(pipeline.statusMessage(), QStringLiteral("Ready"));
        QVERIFY(pipeline.lastError().isEmpty());
        QVERIFY(pipeline.lastTranscription().isEmpty());
    }

    void testBackendSwitching() {
        TranscriptionPipeline pipeline;
        FakeSttClient groqClient;
        FakeSttClient whisperClient;

        pipeline.registerBackend(TranscriptionPipeline::Backend::Groq, &groqClient);
        pipeline.registerBackend(TranscriptionPipeline::Backend::WhisperCpp, &whisperClient);

        pipeline.setActiveBackend(TranscriptionPipeline::Backend::Groq);
        QCOMPARE(pipeline.activeBackend(), TranscriptionPipeline::Backend::Groq);
        QCOMPARE(pipeline.activeSttClient(), &groqClient);

        pipeline.setActiveBackend(TranscriptionPipeline::Backend::WhisperCpp);
        QCOMPARE(pipeline.activeBackend(), TranscriptionPipeline::Backend::WhisperCpp);
        QCOMPARE(pipeline.activeSttClient(), &whisperClient);
    }

    void testTranscriptionDispatchAndCompletion() {
        TranscriptionPipeline pipeline;
        FakeSttClient groqClient;
        pipeline.registerBackend(TranscriptionPipeline::Backend::Groq, &groqClient);
        pipeline.setActiveBackend(TranscriptionPipeline::Backend::Groq);
        pipeline.initialize();

        QSignalSpy finishedSpy(&pipeline, &TranscriptionPipeline::transcriptionFinished);
        QSignalSpy stateSpy(&pipeline, &TranscriptionPipeline::stateChanged);

        // Simulate audio payload arriving from recorder
        const QByteArray dummyWav("RIFFdummyWAVdata");
        QMetaObject::invokeMethod(&pipeline, "onRecordingFinished", Q_ARG(QByteArray, dummyWav));

        QCOMPARE(pipeline.state(), TranscriptionPipeline::State::Transcribing);
        QCOMPARE(pipeline.isBusy(), true);
        QCOMPARE(groqClient.m_transcribeCallCount, 1);
        QCOMPARE(groqClient.m_lastReceivedWav, dummyWav);

        // Simulate STT client producing transcription
        groqClient.simulateSuccess(QStringLiteral("Hello world dictation"));

        QCOMPARE(pipeline.state(), TranscriptionPipeline::State::Idle);
        QCOMPARE(pipeline.isBusy(), false);
        QCOMPARE(pipeline.lastTranscription(), QStringLiteral("Hello world dictation"));
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.at(0).at(0).toString(), QStringLiteral("Hello world dictation"));
    }

    void testErrorAndRetryFlow() {
        TranscriptionPipeline pipeline;
        FakeSttClient groqClient;
        pipeline.registerBackend(TranscriptionPipeline::Backend::Groq, &groqClient);
        pipeline.setActiveBackend(TranscriptionPipeline::Backend::Groq);
        pipeline.initialize();

        QSignalSpy errorSpy(&pipeline, &TranscriptionPipeline::errorOccurred);

        const QByteArray dummyWav("RIFFdummyWAVdata");
        QMetaObject::invokeMethod(&pipeline, "onRecordingFinished", Q_ARG(QByteArray, dummyWav));
        QCOMPARE(pipeline.state(), TranscriptionPipeline::State::Transcribing);

        // Simulate STT transient failure
        groqClient.simulateError(QStringLiteral("Rate Limit 429"));

        QCOMPARE(pipeline.state(), TranscriptionPipeline::State::Error);
        QCOMPARE(pipeline.lastError(), QStringLiteral("Rate Limit 429"));
        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(pipeline.hasActiveNotice());

        // Invoke unified retry method
        pipeline.retry();

        QCOMPARE(pipeline.state(), TranscriptionPipeline::State::Transcribing);
        QCOMPARE(groqClient.m_transcribeCallCount, 2);
        QCOMPARE(groqClient.m_lastReceivedWav, dummyWav);

        // Now simulate success on retry
        groqClient.simulateSuccess(QStringLiteral("Retried text"));
        QCOMPARE(pipeline.state(), TranscriptionPipeline::State::Idle);
        QCOMPARE(pipeline.lastTranscription(), QStringLiteral("Retried text"));
    }

    void testCancelFlow() {
        TranscriptionPipeline pipeline;
        FakeSttClient groqClient;
        pipeline.registerBackend(TranscriptionPipeline::Backend::Groq, &groqClient);
        pipeline.setActiveBackend(TranscriptionPipeline::Backend::Groq);
        pipeline.initialize();

        const QByteArray dummyWav("RIFFdummyWAVdata");
        QMetaObject::invokeMethod(&pipeline, "onRecordingFinished", Q_ARG(QByteArray, dummyWav));
        QCOMPARE(pipeline.state(), TranscriptionPipeline::State::Transcribing);

        pipeline.cancel();
        QCOMPARE(pipeline.state(), TranscriptionPipeline::State::Idle);
        QCOMPARE(groqClient.m_cancelled, true);
    }

    void testNoticePolymorphism() {
        TranscriptionPipeline pipeline;
        FakeSttClient groqClient;
        pipeline.registerBackend(TranscriptionPipeline::Backend::Groq, &groqClient);
        pipeline.setActiveBackend(TranscriptionPipeline::Backend::Groq);

        QVariantMap mockNotice;
        mockNotice[QStringLiteral("hasNotice")] = true;
        mockNotice[QStringLiteral("type")] = QStringLiteral("warning");
        mockNotice[QStringLiteral("title")] = QStringLiteral("API Key Missing");
        groqClient.setNotice(mockNotice);

        QVERIFY(pipeline.hasActiveNotice());
        QCOMPARE(pipeline.activeNotice().value(QStringLiteral("title")).toString(), QStringLiteral("API Key Missing"));
    }
};

QTEST_MAIN(TestTranscriptionPipeline)
#include "test_transcription_pipeline.moc"
