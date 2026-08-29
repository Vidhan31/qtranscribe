#include "AbstractCloudApiClient.h"
#include "AbstractCloudSttClient.h"
#include "CloudProviderModel.h"
#include "CloudSttRouter.h"

#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

class FakeCloudApiClient : public AbstractCloudApiClient {
    Q_OBJECT

public:
    explicit FakeCloudApiClient(QObject* parent = nullptr)
        : AbstractCloudApiClient(QUrl(u"https://fake.example.com"_s), std::chrono::seconds(5), parent) { }
};

class FakeMockCloudSttClient : public AbstractCloudSttClient {
    Q_OBJECT

public:
    explicit FakeMockCloudSttClient(bool smart = false, QObject* parent = nullptr)
        : AbstractCloudSttClient(parent)
        , m_smart(smart) { }

    void transcribe(const QByteArray& wavData) override {
        m_lastWavData = wavData;
        m_transcribeCallCount++;
        emit transcriptionReady(m_mockText);
    }

    void cancel() override {
        m_cancelCallCount++;
    }

    void retryLast() override {
        m_retryCallCount++;
    }

    bool handlesSmartFormatting() const override {
        return m_smart;
    }

    void setMockApiKeySet(bool set) {
        if (!m_fakeApi) {
            m_fakeApi = new FakeCloudApiClient(this);
            setApiClient(m_fakeApi);
        }
        m_fakeApi->setApiKey(set ? u"valid_key"_s : QString());
    }

    void triggerError(const QString& error, ErrorCategory category) {
        setLastError(error, category);
        emit errorOccurred(error);
    }

    void setMockRetrySeconds(int seconds) {
        setRetrySecondsRemaining(seconds);
    }

    void setMockBusy(bool busy) {
        setBusy(busy);
    }

    bool m_smart = false;
    int m_transcribeCallCount = 0;
    int m_cancelCallCount = 0;
    int m_retryCallCount = 0;
    QString m_mockText = u"Mock transcription result"_s;
    FakeCloudApiClient* m_fakeApi = nullptr;

protected:
    QString providerDisplayName() const override {
        return u"MockProvider"_s;
    }
    QString defaultModel() const override {
        return u"mock-model"_s;
    }
    QString settingsModelKey() const override {
        return {};
    }
    QNetworkReply* buildAndSendRequest(const QByteArray&) override {
        return nullptr;
    }
    QString extractTranscribedText(const CloudApiResponse&) override {
        return m_mockText;
    }
};

class TestCloudSttRouter : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName(u"QTranscribeTestRouter"_s);
        QCoreApplication::setApplicationName(u"QTranscribeTestRouter"_s);
    }

    void cleanup() {
        QSettings settings;
        settings.remove(u"Cloud/ActiveProvider"_s);
        settings.sync();
    }

    void testRoutingAndProviderSwitching() {
        cleanup();
        CloudProviderModel model;
        model.setActiveProviderId(u"groq"_s);
        CloudSttRouter router;
        router.setCloudProviderModel(&model);

        FakeMockCloudSttClient groqClient(false);
        groqClient.m_mockText = u"Groq result"_s;
        groqClient.setMockApiKeySet(true);

        FakeMockCloudSttClient geminiClient(true);
        geminiClient.m_mockText = u"Gemini result"_s;
        geminiClient.setMockApiKeySet(true);

        router.registerProvider(u"groq"_s, &groqClient);
        router.registerProvider(u"gemini"_s, &geminiClient);

        // Default active provider is groq
        QCOMPARE(model.activeProviderId(), u"groq"_s);
        QCOMPARE(router.activeClient(), &groqClient);
        QCOMPARE(router.activeCloudClient(), &groqClient);
        QCOMPARE(router.handlesSmartFormatting(), false);
        QCOMPARE(router.isReady(), true);
        QCOMPARE(router.isApiKeySet(), true);

        QSignalSpy spyTransReady(&router, &AbstractSttClient::transcriptionReady);

        router.transcribe(QByteArray("fakeAudio"));
        QCOMPARE(groqClient.m_transcribeCallCount, 1);
        QCOMPARE(geminiClient.m_transcribeCallCount, 0);
        QCOMPARE(spyTransReady.count(), 1);
        QCOMPARE(spyTransReady.at(0).at(0).toString(), u"Groq result"_s);

        // Switch to Gemini
        model.setActiveProviderId(u"gemini"_s);
        QCOMPARE(router.activeClient(), &geminiClient);
        QCOMPARE(router.activeCloudClient(), &geminiClient);
        QCOMPARE(router.handlesSmartFormatting(), true);

        spyTransReady.clear();
        router.transcribe(QByteArray("fakeAudio2"));
        QCOMPARE(geminiClient.m_transcribeCallCount, 1);
        QCOMPARE(groqClient.m_transcribeCallCount, 1);
        QCOMPARE(spyTransReady.count(), 1);
        QCOMPARE(spyTransReady.at(0).at(0).toString(), u"Gemini result"_s);
    }

    void testCancelAndSignalDelegation() {
        CloudProviderModel model;
        model.setActiveProviderId(u"gemini"_s);

        CloudSttRouter router;
        router.setCloudProviderModel(&model);

        FakeMockCloudSttClient geminiClient(true);
        geminiClient.setMockApiKeySet(true);
        router.registerProvider(u"gemini"_s, &geminiClient);

        QSignalSpy spyBusy(&router, &AbstractSttClient::busyChanged);
        QSignalSpy spyReady(&router, &AbstractSttClient::readyChanged);

        geminiClient.setMockBusy(true);
        QCOMPARE(spyBusy.count(), 1);
        QCOMPARE(router.isBusy(), true);

        geminiClient.setMockApiKeySet(false);
        QCOMPARE(spyReady.count(), 1);
        QCOMPARE(router.isReady(), false);

        router.cancel();
        QCOMPARE(geminiClient.m_cancelCallCount, 1);
    }

    void testPolymorphicStatusPropertiesAndSignals() {
        CloudProviderModel model;
        model.setActiveProviderId(u"groq"_s);

        CloudSttRouter router;
        router.setCloudProviderModel(&model);

        FakeMockCloudSttClient client(false);
        router.registerProvider(u"groq"_s, &client);

        QSignalSpy spyApiKey(&router, &CloudSttRouter::apiKeySetChanged);
        QSignalSpy spyKeyInvalid(&router, &CloudSttRouter::isApiKeyInvalidChanged);
        QSignalSpy spyRateLimited(&router, &CloudSttRouter::isRateLimitedChanged);
        QSignalSpy spyRetrySeconds(&router, &CloudSttRouter::retrySecondsRemainingChanged);
        QSignalSpy spyLastError(&router, &CloudSttRouter::lastErrorChanged);
        QSignalSpy spyErrorCategory(&router, &CloudSttRouter::errorCategoryChanged);

        // Initially no API key set
        QCOMPARE(router.isApiKeySet(), false);
        QCOMPARE(router.isApiKeyInvalid(), false);
        QCOMPARE(router.isRateLimited(), false);
        QCOMPARE(router.retrySecondsRemaining(), 0);
        QCOMPARE(router.lastError(), QString());

        // Set API key
        client.setMockApiKeySet(true);
        QCOMPARE(spyApiKey.count(), 1);
        QCOMPARE(router.isApiKeySet(), true);
        QCOMPARE(router.isApiKeyInvalid(), false);

        // Trigger Invalid API key error
        client.triggerError(u"Unauthorized API key"_s, AbstractCloudSttClient::ErrorCategory::InvalidApiKey);
        QCOMPARE(spyKeyInvalid.count(), 1);
        QCOMPARE(spyLastError.count(), 1);
        QCOMPARE(spyErrorCategory.count(), 1);
        QCOMPARE(router.isApiKeyInvalid(), true);
        QCOMPARE(router.lastError(), u"Unauthorized API key"_s);
        QCOMPARE(router.errorCategory(), AbstractCloudSttClient::ErrorCategory::InvalidApiKey);

        // Trigger Rate limit error with countdown
        client.triggerError(u"Rate limit reached"_s, AbstractCloudSttClient::ErrorCategory::RateLimited);
        client.setMockRetrySeconds(30);
        QCOMPARE(spyRateLimited.count(), 1);
        QCOMPARE(spyRetrySeconds.count(), 1);
        QCOMPARE(router.isRateLimited(), true);
        QCOMPARE(router.retrySecondsRemaining(), 30);
    }

    void testProviderSwitchUpdatesStatusPropertiesAndRewiresSignals() {
        CloudProviderModel model;
        model.setActiveProviderId(u"groq"_s);

        CloudSttRouter router;
        router.setCloudProviderModel(&model);

        FakeMockCloudSttClient groqClient(false);
        groqClient.setMockApiKeySet(true);

        FakeMockCloudSttClient geminiClient(true);
        geminiClient.setMockApiKeySet(false);
        geminiClient.triggerError(u"Gemini Rate Limited"_s, AbstractCloudSttClient::ErrorCategory::RateLimited);
        geminiClient.setMockRetrySeconds(15);

        router.registerProvider(u"groq"_s, &groqClient);
        router.registerProvider(u"gemini"_s, &geminiClient);

        // Initially Groq is active: API key is set, not rate limited
        QCOMPARE(router.isApiKeySet(), true);
        QCOMPARE(router.isRateLimited(), false);
        QCOMPARE(router.retrySecondsRemaining(), 0);

        QSignalSpy spyApiKey(&router, &CloudSttRouter::apiKeySetChanged);
        QSignalSpy spyRateLimited(&router, &CloudSttRouter::isRateLimitedChanged);
        QSignalSpy spyRetrySeconds(&router, &CloudSttRouter::retrySecondsRemainingChanged);

        // Switch to Gemini
        model.setActiveProviderId(u"gemini"_s);
        QCOMPARE(spyApiKey.count(), 1);
        QCOMPARE(spyRateLimited.count(), 1);
        QCOMPARE(spyRetrySeconds.count(), 1);

        QCOMPARE(router.isApiKeySet(), false);
        QCOMPARE(router.isRateLimited(), true);
        QCOMPARE(router.retrySecondsRemaining(), 15);
        QCOMPARE(router.lastError(), u"Gemini Rate Limited"_s);

        // Modifying deactivated Groq client must NOT affect router
        spyApiKey.clear();
        groqClient.setMockApiKeySet(false);
        QCOMPARE(spyApiKey.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TestCloudSttRouter)
#include "test_cloud_stt_router.moc"
