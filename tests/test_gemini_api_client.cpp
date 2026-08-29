#include "ApiKeyStore.h"
#include "GeminiApiClient.h"
#include "GeminiSttClient.h"

#include <QCoreApplication>
#include <QHttpHeaders>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

class TestGeminiApiClient : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName(u"QTranscribeTest"_s);
        QCoreApplication::setApplicationName(u"QTranscribeTest"_s);
    }

    void cleanup() {
        QSettings settings;
        settings.remove(u"GeminiTest/ApiKey"_s);
        settings.remove(u"GeminiTest/CustomKey"_s);
        settings.sync();
    }

    void testApiKeyStoreDeferredLoading() {
        QSettings settings;
        settings.setValue(u"GeminiTest/ApiKey"_s, u"AIzaSyTest_deferred_key_123"_s);
        settings.sync();

        ApiKeyStore store;
        store.setStorageKeys(u"QTranscribeTestService"_s, u"test_gemini_api_key"_s, u"GeminiTest/ApiKey"_s);

        QCOMPARE(store.apiKey(), QString());
        QCOMPARE(store.apiKeySet(), false);

        QSignalSpy spyKeyChanged(&store, &ApiKeyStore::apiKeyChanged);
        QSignalSpy spyKeySetChanged(&store, &ApiKeyStore::apiKeySetChanged);

        store.ensureApiKeyLoaded();

        QTRY_COMPARE(store.apiKey(), u"AIzaSyTest_deferred_key_123"_s);
        QCOMPARE(store.apiKeySet(), true);
        QVERIFY(spyKeyChanged.count() > 0);
        QVERIFY(spyKeySetChanged.count() > 0);
    }

    void testGeminiApiClientDelegationAndHeaders() {
        QSettings settings;
        settings.setValue(u"GeminiTest/ApiKey"_s, u"AIzaSyTest_api_key_456"_s);
        settings.sync();

        GeminiApiClient client;
        client.setStorageKeys(u"QTranscribeTestService"_s, u"test_gemini_api_key"_s, u"GeminiTest/ApiKey"_s);

        QCOMPARE(client.apiKey(), QString());
        QCOMPARE(client.apiKeySet(), false);

        client.ensureApiKeyLoaded();

        QTRY_COMPARE(client.apiKey(), u"AIzaSyTest_api_key_456"_s);
        QCOMPARE(client.apiKeySet(), true);

        QNetworkRequest req = client.createApiRequest(u"interactions"_s);
        QCOMPARE(req.header(QNetworkRequest::KnownHeaders::UserAgentHeader).toString(), GeminiApiClient::userAgent());

        QHttpHeaders headers = req.headers();
        QCOMPARE(headers.value("x-goog-api-key"), QByteArray("AIzaSyTest_api_key_456"));
    }

    void testGeminiSttClientActivateLazyLoad() {
        QSettings settings;
        settings.setValue(u"GeminiTest/ApiKey"_s, u"AIzaSyTest_stt_key_789"_s);
        settings.sync();

        GeminiApiClient api;
        api.setStorageKeys(u"QTranscribeTestService"_s, u"test_gemini_api_key"_s, u"GeminiTest/ApiKey"_s);
        GeminiSttClient stt;

        QCOMPARE(api.apiKeySet(), false);
        QCOMPARE(stt.isReady(), false);

        stt.setApiClient(&api);
        QCOMPARE(api.apiKeySet(), false);
        QCOMPARE(stt.isReady(), false);

        stt.activate();

        QTRY_VERIFY(stt.isReady());
        QCOMPARE(api.apiKey(), u"AIzaSyTest_stt_key_789"_s);
    }
};

QTEST_GUILESS_MAIN(TestGeminiApiClient)
#include "test_gemini_api_client.moc"
