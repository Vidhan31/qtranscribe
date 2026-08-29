#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

#include "AbstractLlmClient.h"
#include "ApiKeyStore.h"
#include "ApplicationContext.h"
#include "AudioFeedbackPlayer.h"
#include "AudioRecorder.h"
#include "CloudProviderModel.h"
#include "CloudSttRouter.h"
#include "DictationCoordinator.h"
#include "DictationPadModel.h"
#include "GeminiApiClient.h"
#include "GeminiSttClient.h"
#include "GlobalShortcutManager.h"
#include "GroqApiClient.h"
#include "GroqSttClient.h"
#include "SystemHealthMonitor.h"
#include "TextInjectorClient.h"
#include "TranscriptionModel.h"
#include "WhisperModelManager.h"
#include "WhisperSttClient.h"

using namespace Qt::StringLiterals;

class TestApplicationContext : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void testHeadlessInitialization();
    void testSubsystemWiringIntegrity();
    void testRepeatedInitializeIsNoOp();
};

void TestApplicationContext::initTestCase() {
    QCoreApplication::setOrganizationName(u"QTranscribeTestOrg"_s);
    QCoreApplication::setApplicationName(u"QTranscribeTestApp"_s);
}

void TestApplicationContext::testHeadlessInitialization() {
    ApplicationContext context;
    QVERIFY(!context.isInitialized());
    QVERIFY(context.dictationCoordinator() == nullptr);

    context.initializeHeadless();
    QVERIFY(context.isInitialized());

    QVERIFY(context.dictationCoordinator() != nullptr);
    QVERIFY(context.dictationPadModel() != nullptr);
    QVERIFY(context.systemHealthMonitor() != nullptr);
    QVERIFY(context.audioFeedbackPlayer() != nullptr);
    QVERIFY(context.apiKeyStore() != nullptr);
    QVERIFY(context.whisperSttClient() != nullptr);
    QVERIFY(context.whisperModelManager() != nullptr);
    QVERIFY(context.groqApiClient() != nullptr);
    QVERIFY(context.groqSttClient() != nullptr);
    QVERIFY(context.geminiApiClient() != nullptr);
    QVERIFY(context.geminiSttClient() != nullptr);
    QVERIFY(context.cloudSttRouter() != nullptr);
    QVERIFY(context.llmClient() != nullptr);
    QVERIFY(context.cloudProviderModel() != nullptr);
    QVERIFY(context.audioRecorder() != nullptr);
    QVERIFY(context.shortcutManager() != nullptr);
    QVERIFY(context.textInjector() != nullptr);
    QVERIFY(context.historyModel() != nullptr);
}

void TestApplicationContext::testSubsystemWiringIntegrity() {
    ApplicationContext context;
    context.initializeHeadless();

    QCOMPARE(context.whisperSttClient()->modelManager(), context.whisperModelManager());
    QVERIFY(context.dictationCoordinator() != nullptr);
    QVERIFY(context.llmClient() != nullptr);
    QCOMPARE(context.cloudSttRouter()->cloudProviderModel(), context.cloudProviderModel());
    QVERIFY(context.cloudSttRouter()->activeClient() != nullptr);
}

void TestApplicationContext::testRepeatedInitializeIsNoOp() {
    ApplicationContext context;
    context.initializeHeadless();
    auto* initialCoordinator = context.dictationCoordinator();

    context.initializeHeadless();
    QCOMPARE(context.dictationCoordinator(), initialCoordinator);
}

QTEST_MAIN(TestApplicationContext)
#include "test_application_context.moc"
