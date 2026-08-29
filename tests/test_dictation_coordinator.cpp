#include <QCoreApplication>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>

#include "AbstractSttClient.h"
#include "AudioFeedbackPlayer.h"
#include "AudioRecorder.h"
#include "DictationCoordinator.h"
#include "DictationPadModel.h"
#include "GlobalShortcutManager.h"
#include "GroqApiClient.h"
#include "GroqLlmClient.h"
#include "SystemHealthMonitor.h"
#include "TextInjectorClient.h"
#include "TranscriptionModel.h"

using namespace Qt::StringLiterals;

class FakeAudioRecorder : public AudioRecorder {
    Q_OBJECT

public:
    explicit FakeAudioRecorder(QObject* parent = nullptr)
        : AudioRecorder(parent) {
        setHasAudioInputDevice(true);
    }

    void setMockHasAudioInputDevice(bool hasDevice) {
        setHasAudioInputDevice(hasDevice);
    }

    void startRecording() override {
        setRecording(true);
    }

    void stopRecording() override {
        setRecording(false);
        emit recordingFinished(m_pendingWavData.isEmpty() ? QByteArray("RIFFfakeWavData") : m_pendingWavData);
    }

    void cancelRecording() override {
        setRecording(false);
    }

    void triggerMaxDuration() {
        emit maxDurationReached();
    }

    QByteArray m_pendingWavData;
};

class FakeSttClient : public AbstractSttClient {
    Q_OBJECT

public:
    explicit FakeSttClient(QObject* parent = nullptr)
        : AbstractSttClient(parent) {
        m_isReady = true;
        m_isBusy = false;
    }

    bool isReady() const override {
        return m_isReady;
    }

    bool isBusy() const override {
        return m_isBusy;
    }

    void transcribe(const QByteArray& audioData) override {
        m_lastTranscribeAudio = audioData;
        m_transcribeCallCount++;
        m_isBusy = true;
        emit busyChanged();

        if (m_autoRespond) {
            m_isBusy = false;
            emit busyChanged();
            if (m_failNext) {
                emit errorOccurred(m_simulatedError.isEmpty() ? u"Inference failed"_s : m_simulatedError);
            } else {
                emit transcriptionReady(m_simulatedText.isEmpty() ? u"Transcribed text"_s : m_simulatedText);
            }
        }
    }

    void cancel() override {
        m_cancelCalled = true;
        m_isBusy = false;
        emit busyChanged();
    }

    void activate() override {
        m_activateCalled = true;
    }

    void deactivate() override {
        m_deactivateCalled = true;
    }

    bool m_isReady = true;
    bool m_isBusy = false;
    QByteArray m_lastTranscribeAudio;
    int m_transcribeCallCount = 0;
    bool m_cancelCalled = false;
    bool m_activateCalled = false;
    bool m_deactivateCalled = false;
    bool m_autoRespond = true;
    bool m_failNext = false;
    QString m_simulatedText;
    QString m_simulatedError;
};

class FakeShortcutManager : public GlobalShortcutManager {
    Q_OBJECT

public:
    explicit FakeShortcutManager(QObject* parent = nullptr)
        : GlobalShortcutManager(parent) {
        setAvailable(true);
        setSupported(true);
    }

    void setMockSupported(bool supported) {
        setSupported(supported);
    }

    void setMockAvailable(bool available) {
        setAvailable(available);
    }

    void triggerShortcutActivated(const QString& id) {
        emit shortcutActivated(id);
    }

    void triggerShortcutDeactivated(const QString& id) {
        emit shortcutDeactivated(id);
    }
};

class TestDictationCoordinator : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testInitialState();
    void testBackendRegistrationAndSwitching();
    void testRecordingLifecycleSuccess();
    void testRecordingNoMicrophone();
    void testLlmEnhancementFlow();
    void testLlmEnhancementErrorFallback();
    void testCancelDictation();
    void testRetryTranscription();
    void testMaxDurationSafety();
    void testShortcutToggleMode();
    void testShortcutPushToTalkMode();
    void testPushToTalkUnsupportedFallback();
    void testDictationPadModelDirect();
    void testAudioFeedbackPlayerDirect();
    void testSystemHealthMonitorDirect();

private:
    DictationCoordinator* m_coordinator = nullptr;
    DictationPadModel* m_padModel = nullptr;
    SystemHealthMonitor* m_healthMonitor = nullptr;
    AudioFeedbackPlayer* m_feedbackPlayer = nullptr;
    FakeAudioRecorder* m_recorder = nullptr;
    FakeSttClient* m_whisperClient = nullptr;
    FakeSttClient* m_groqClient = nullptr;
    GroqLlmClient* m_llmClient = nullptr;
    GroqApiClient* m_apiClient = nullptr;
    FakeShortcutManager* m_shortcutMgr = nullptr;
    TextInjectorClient* m_injector = nullptr;
    TranscriptionModel* m_historyModel = nullptr;
};

void TestDictationCoordinator::initTestCase() {
    QCoreApplication::setOrganizationName(u"QTranscribeTestOrg"_s);
    QCoreApplication::setApplicationName(u"QTranscribeTestApp"_s);
}

void TestDictationCoordinator::cleanupTestCase() { }

void TestDictationCoordinator::init() {
    m_coordinator = new DictationCoordinator(this);
    m_padModel = new DictationPadModel(this);
    m_healthMonitor = new SystemHealthMonitor(this);
    m_feedbackPlayer = new AudioFeedbackPlayer(this);
    m_recorder = new FakeAudioRecorder(this);
    m_whisperClient = new FakeSttClient(this);
    m_groqClient = new FakeSttClient(this);
    m_apiClient = new GroqApiClient(this);
    m_llmClient = new GroqLlmClient(this);
    m_shortcutMgr = new FakeShortcutManager(this);
    m_injector = new TextInjectorClient(this);
    m_historyModel = new TranscriptionModel(this);

    m_llmClient->setApiClient(m_apiClient);
    m_apiClient->setApiKey(u"gsk_mock_api_key_for_testing"_s);

    m_coordinator->setDictationPadModel(m_padModel);
    m_coordinator->setSystemHealthMonitor(m_healthMonitor);
    m_coordinator->setAudioFeedbackPlayer(m_feedbackPlayer);
    m_coordinator->setAudioRecorder(m_recorder);
    m_coordinator->registerBackend(DictationCoordinator::TranscriptionBackend::WhisperCpp, m_whisperClient);
    m_coordinator->registerBackend(DictationCoordinator::TranscriptionBackend::Cloud, m_groqClient);
    m_coordinator->setLlmClient(m_llmClient);
    m_coordinator->setShortcutManager(m_shortcutMgr);
    m_coordinator->setTextInjector(m_injector);
    m_coordinator->setHistoryModel(m_historyModel);

    m_coordinator->initialize();
}

void TestDictationCoordinator::cleanup() {
    delete m_coordinator;
    delete m_padModel;
    delete m_healthMonitor;
    delete m_feedbackPlayer;
    delete m_recorder;
    delete m_whisperClient;
    delete m_groqClient;
    delete m_llmClient;
    delete m_apiClient;
    delete m_shortcutMgr;
    delete m_injector;
    delete m_historyModel;
}

void TestDictationCoordinator::testInitialState() {
    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Idle);
    QVERIFY(!m_coordinator->isBusy());
    QVERIFY(!m_coordinator->recording());
    QVERIFY(!m_coordinator->transcribing());
    QVERIFY(!m_coordinator->enhancing());
    QVERIFY(m_coordinator->canRecord());
    QCOMPARE(m_coordinator->statusMessage(), u"Ready"_s);
    QVERIFY(m_coordinator->lastError().isEmpty());
    QVERIFY(m_coordinator->lastTranscription().isEmpty());
    QVERIFY(m_coordinator->dictationPadModel() != nullptr);
    QVERIFY(m_coordinator->dictationPadModel()->text().isEmpty());
    QVERIFY(m_coordinator->audioFeedbackPlayer() != nullptr);
    QVERIFY(m_coordinator->systemHealthMonitor() != nullptr);
}

void TestDictationCoordinator::testBackendRegistrationAndSwitching() {
    m_coordinator->setActiveBackend(DictationCoordinator::TranscriptionBackend::WhisperCpp);
    QCOMPARE(m_coordinator->activeBackend(), DictationCoordinator::TranscriptionBackend::WhisperCpp);
    QCOMPARE(m_coordinator->activeSttClient(), m_whisperClient);

    QSignalSpy backendSpy(m_coordinator, &DictationCoordinator::activeBackendChanged);
    m_coordinator->setActiveBackend(DictationCoordinator::TranscriptionBackend::Cloud);
    QCOMPARE(backendSpy.count(), 1);
    QCOMPARE(m_coordinator->activeBackend(), DictationCoordinator::TranscriptionBackend::Cloud);
    QCOMPARE(m_coordinator->activeSttClient(), m_groqClient);
}

void TestDictationCoordinator::testRecordingLifecycleSuccess() {
    m_coordinator->setActiveBackend(DictationCoordinator::TranscriptionBackend::WhisperCpp);
    m_whisperClient->m_simulatedText = u"Hello from whisper.cpp"_s;

    QSignalSpy stateSpy(m_coordinator, &DictationCoordinator::dictationStateChanged);
    QSignalSpy finishSpy(m_coordinator, &DictationCoordinator::transcriptionFinished);

    m_coordinator->startRecording();
    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Recording);
    QVERIFY(m_coordinator->recording());
    QVERIFY(m_coordinator->isBusy());

    m_coordinator->stopRecording();

    QCOMPARE(finishSpy.count(), 1);
    QCOMPARE(finishSpy.first().at(0).toString(), u"Hello from whisper.cpp"_s);
    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Idle);
    QCOMPARE(m_coordinator->lastTranscription(), u"Hello from whisper.cpp"_s);
    QCOMPARE(m_padModel->text(), u"Hello from whisper.cpp"_s);
    QCOMPARE(m_historyModel->rowCount(), 1);
}

void TestDictationCoordinator::testRecordingNoMicrophone() {
    m_recorder->setMockHasAudioInputDevice(false);
    m_coordinator->updateCoordinatorHealth();

    QVERIFY(!m_coordinator->canRecord());

    m_coordinator->startRecording();
    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Error);
    QVERIFY(!m_coordinator->lastError().isEmpty());
}

void TestDictationCoordinator::testLlmEnhancementFlow() {
    m_coordinator->setActiveBackend(DictationCoordinator::TranscriptionBackend::Cloud);
    m_llmClient->setEnabled(true);
    m_groqClient->m_autoRespond = true;
    m_groqClient->m_simulatedText = u"um so basicly hello world"_s;

    QSignalSpy finishSpy(m_coordinator, &DictationCoordinator::transcriptionFinished);

    m_coordinator->startRecording();
    m_coordinator->stopRecording();

    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Enhancing);
    QVERIFY(m_coordinator->enhancing());

    // Simulate LLM response
    emit m_llmClient->enhancementReady(u"Hello world."_s);

    QCOMPARE(finishSpy.count(), 1);
    QCOMPARE(finishSpy.first().at(0).toString(), u"Hello world."_s);
    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Idle);
    QCOMPARE(m_coordinator->lastTranscription(), u"Hello world."_s);
}

void TestDictationCoordinator::testLlmEnhancementErrorFallback() {
    m_coordinator->setActiveBackend(DictationCoordinator::TranscriptionBackend::Cloud);
    m_llmClient->setEnabled(true);
    m_groqClient->m_autoRespond = true;
    m_groqClient->m_simulatedText = u"Raw whisper transcript"_s;

    QSignalSpy warningSpy(m_coordinator, &DictationCoordinator::llmFallbackWarningTriggered);
    QSignalSpy finishSpy(m_coordinator, &DictationCoordinator::transcriptionFinished);

    m_coordinator->startRecording();
    m_coordinator->stopRecording();

    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Enhancing);

    // Simulate LLM error with fallback text
    emit m_llmClient->errorOccurred(u"API Rate limit"_s, u"Raw whisper transcript"_s);

    QCOMPARE(warningSpy.count(), 1);
    QCOMPARE(finishSpy.count(), 1);
    QCOMPARE(finishSpy.first().at(0).toString(), u"Raw whisper transcript"_s);
    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Idle);
}

void TestDictationCoordinator::testCancelDictation() {
    m_coordinator->startRecording();
    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Recording);

    m_coordinator->cancelDictation();
    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Idle);
    QVERIFY(!m_recorder->recording());
}

void TestDictationCoordinator::testRetryTranscription() {
    m_coordinator->setActiveBackend(DictationCoordinator::TranscriptionBackend::WhisperCpp);
    m_whisperClient->m_failNext = true;
    m_whisperClient->m_simulatedError = u"Engine error"_s;

    QSignalSpy errorSpy(m_coordinator, &DictationCoordinator::errorOccurred);

    m_coordinator->startRecording();
    m_coordinator->stopRecording();

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Error);
    QCOMPARE(m_coordinator->lastError(), u"Engine error"_s);

    // Now retry
    m_whisperClient->m_failNext = false;
    m_whisperClient->m_simulatedText = u"Recovered text on retry"_s;

    m_coordinator->retryTranscription();
    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Idle);
    QCOMPARE(m_coordinator->lastTranscription(), u"Recovered text on retry"_s);
}

void TestDictationCoordinator::testMaxDurationSafety() {
    QSignalSpy warningSpy(m_coordinator, &DictationCoordinator::maxDurationWarningTriggered);

    m_coordinator->startRecording();
    QVERIFY(m_coordinator->recording());

    m_recorder->triggerMaxDuration();
    QCOMPARE(warningSpy.count(), 1);
    QVERIFY(!m_coordinator->recording());
}

void TestDictationCoordinator::testShortcutToggleMode() {
    m_coordinator->setRecordingMode(DictationCoordinator::RecordingMode::Toggle);

    m_shortcutMgr->triggerShortcutActivated(u"toggle-recording"_s);
    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Recording);

    m_shortcutMgr->triggerShortcutActivated(u"toggle-recording"_s);
    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Idle);
}

void TestDictationCoordinator::testShortcutPushToTalkMode() {
    m_shortcutMgr->setMockSupported(true);
    m_coordinator->setRecordingMode(DictationCoordinator::RecordingMode::PushToTalk);
    QCOMPARE(m_coordinator->recordingMode(), DictationCoordinator::RecordingMode::PushToTalk);

    m_shortcutMgr->triggerShortcutActivated(u"record"_s);
    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Recording);

    m_shortcutMgr->triggerShortcutDeactivated(u"record"_s);
    QCOMPARE(m_coordinator->dictationState(), DictationCoordinator::DictationState::Idle);
}

void TestDictationCoordinator::testPushToTalkUnsupportedFallback() {
    m_shortcutMgr->setMockSupported(false);
    m_coordinator->setRecordingMode(DictationCoordinator::RecordingMode::PushToTalk);
    QCOMPARE(m_coordinator->recordingMode(), DictationCoordinator::RecordingMode::Toggle);
}

void TestDictationCoordinator::testDictationPadModelDirect() {
    DictationPadModel model;
    QSignalSpy spy(&model, &DictationPadModel::textChanged);

    model.setText(u"First line"_s);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.wordCount(), 2);
    QCOMPARE(model.charCount(), 10);

    model.append(u"Second line"_s);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.text(), u"First line\nSecond line"_s);
    QCOMPARE(model.wordCount(), 4);

    model.clear();
    QCOMPARE(spy.count(), 3);
    QCOMPARE(model.text(), u""_s);
    QCOMPARE(model.wordCount(), 0);
    QCOMPARE(model.charCount(), 0);
}

void TestDictationCoordinator::testAudioFeedbackPlayerDirect() {
    AudioFeedbackPlayer player;
    QSignalSpy soundSpy(&player, &AudioFeedbackPlayer::soundEnabledChanged);

    player.setSoundEnabled(false);
    QCOMPARE(soundSpy.count(), 1);
    QVERIFY(!player.soundEnabled());

    player.setSoundEnabled(true);
    QCOMPARE(soundSpy.count(), 2);
    QVERIFY(player.soundEnabled());
}

void TestDictationCoordinator::testSystemHealthMonitorDirect() {
    SystemHealthMonitor monitor;
    QSignalSpy healthSpy(&monitor, &SystemHealthMonitor::systemHealthChanged);
    QSignalSpy canRecordSpy(&monitor, &SystemHealthMonitor::canRecordChanged);

    monitor.setShortcutManager(m_shortcutMgr);
    QVERIFY(healthSpy.count() >= 1);
    QVERIFY(monitor.systemShortcutSupported());
    QVERIFY(!monitor.systemShortcutHasIssue());

    m_shortcutMgr->setMockAvailable(false);
    QVERIFY(monitor.systemShortcutHasIssue());

    monitor.setAudioRecorder(m_recorder);
    monitor.setActiveSttClient(m_whisperClient);
    QVERIFY(monitor.canRecord(true));
    QVERIFY(!monitor.canRecord(false));

    m_recorder->setMockHasAudioInputDevice(false);
    QVERIFY(!monitor.canRecord(true));
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    TestDictationCoordinator test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_dictation_coordinator.moc"
