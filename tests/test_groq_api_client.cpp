#include "ApiKeyStore.h"
#include "GroqApiClient.h"
#include "GroqSttClient.h"
#include "PresetProvider.h"

#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

class TestGroqApiClient : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName(u"QTranscribeTest"_s);
        QCoreApplication::setApplicationName(u"QTranscribeTest"_s);
    }

    void cleanup() {
        QSettings settings;
        settings.remove(u"GroqTest/ApiKey"_s);
        settings.remove(u"GroqTest/CustomKey"_s);
        settings.sync();
    }

    void testApiKeyStoreDeferredLoading() {
        QSettings settings;
        settings.setValue(u"GroqTest/ApiKey"_s, u"gsk_test_deferred_key"_s);
        settings.sync();

        ApiKeyStore store;
        store.setStorageKeys(u"QTranscribeTestService"_s, u"test_groq_api_key"_s, u"GroqTest/ApiKey"_s);

        QCOMPARE(store.apiKey(), QString());
        QCOMPARE(store.apiKeySet(), false);

        QSignalSpy spyKeyChanged(&store, &ApiKeyStore::apiKeyChanged);
        QSignalSpy spyKeySetChanged(&store, &ApiKeyStore::apiKeySetChanged);

        store.ensureApiKeyLoaded();

        QTRY_COMPARE(store.apiKey(), u"gsk_test_deferred_key"_s);
        QCOMPARE(store.apiKeySet(), true);
        QVERIFY(spyKeyChanged.count() > 0);
        QVERIFY(spyKeySetChanged.count() > 0);

        // Ensure idempotency: calling ensureApiKeyLoaded again shouldn't reload or duplicate
        const int prevKeyChangedCount = spyKeyChanged.count();
        store.ensureApiKeyLoaded();
        QCOMPARE(spyKeyChanged.count(), prevKeyChangedCount);
    }

    void testApiKeyStoreSetAndClear() {
        ApiKeyStore store;
        store.setStorageKeys(u"QTranscribeTestService"_s, u"test_groq_api_key"_s, u"GroqTest/ApiKey"_s);
        QCOMPARE(store.apiKeySet(), false);

        QSignalSpy spyKeyChanged(&store, &ApiKeyStore::apiKeyChanged);
        QSignalSpy spyKeySetChanged(&store, &ApiKeyStore::apiKeySetChanged);

        store.setApiKey(u"  gsk_new_active_key_12345  "_s);

        QCOMPARE(store.apiKey(), u"gsk_new_active_key_12345"_s);
        QCOMPARE(store.apiKeySet(), true);
        QCOMPARE(spyKeyChanged.count(), 1);
        QCOMPARE(spyKeySetChanged.count(), 1);

        store.setApiKey(QString());

        QCOMPARE(store.apiKey(), QString());
        QCOMPARE(store.apiKeySet(), false);

        QSettings settings;
        QVERIFY(settings.value(u"GroqTest/ApiKey"_s).toString().isEmpty());
    }

    void testApiKeyStoreCustomStorageKeys() {
        QSettings settings;
        settings.setValue(u"GroqTest/CustomKey"_s, u"gsk_custom_storage_value"_s);
        settings.sync();

        ApiKeyStore store;
        store.setStorageKeys(u"CustomService"_s, u"custom_key"_s, u"GroqTest/CustomKey"_s);

        QCOMPARE(store.keychainService(), u"CustomService"_s);
        QCOMPARE(store.keychainKey(), u"custom_key"_s);
        QCOMPARE(store.settingsKey(), u"GroqTest/CustomKey"_s);

        store.ensureApiKeyLoaded();
        QTRY_COMPARE(store.apiKey(), u"gsk_custom_storage_value"_s);
    }

    void testPresetProviderPresets() {
        const QString grammar = PresetProvider::systemPromptForPreset(u"grammar"_s);
        QVERIFY(!grammar.isEmpty());
        QVERIFY(grammar.contains(u"speech-to-text post-processor"_s));

        const QString bullets = PresetProvider::systemPromptForPreset(u"bullets"_s);
        QVERIFY(!bullets.isEmpty());
        QVERIFY(bullets.contains(u"bullet points"_s));

        const QString professional = PresetProvider::systemPromptForPreset(u"professional"_s);
        QVERIFY(!professional.isEmpty());
        QVERIFY(professional.contains(u"executive communication"_s));

        // Custom preset with custom string
        const QString customPrompt = u"Format text as JSON object only."_s;
        const QString resolvedCustom = PresetProvider::systemPromptForPreset(u"custom"_s, customPrompt);
        QCOMPARE(resolvedCustom, customPrompt);

        // Custom preset with empty string falls back to default custom
        const QString defaultCustom = PresetProvider::systemPromptForPreset(u"custom"_s, QString());
        QCOMPARE(defaultCustom, PresetProvider::defaultCustomPrompt());

        // Unknown preset falls back to grammar
        const QString unknownFallback = PresetProvider::systemPromptForPreset(u"nonexistent_preset"_s);
        QCOMPARE(unknownFallback, PresetProvider::grammarPrompt());

        // Available presets and default
        QCOMPARE(PresetProvider::defaultPreset(), u"grammar"_s);
        const QStringList presets = PresetProvider::availablePresets();
        QVERIFY(presets.contains(u"grammar"_s));
        QVERIFY(presets.contains(u"bullets"_s));
        QVERIFY(presets.contains(u"professional"_s));
        QVERIFY(presets.contains(u"custom"_s));
    }

    void testGroqApiClientDelegatesToKeyStore() {
        QSettings settings;
        settings.setValue(u"GroqTest/ApiKey"_s, u"gsk_test_delegation_key"_s);
        settings.sync();

        GroqApiClient client;
        client.setStorageKeys(u"QTranscribeTestService"_s, u"test_groq_api_key"_s, u"GroqTest/ApiKey"_s);

        QCOMPARE(client.apiKey(), QString());
        QCOMPARE(client.apiKeySet(), false);

        QSignalSpy spyKeyChanged(&client, &GroqApiClient::apiKeyChanged);
        QSignalSpy spyKeySetChanged(&client, &GroqApiClient::apiKeySetChanged);

        client.ensureApiKeyLoaded();

        QTRY_COMPARE(client.apiKey(), u"gsk_test_delegation_key"_s);
        QCOMPARE(client.apiKeySet(), true);
        QVERIFY(spyKeyChanged.count() > 0);
        QVERIFY(spyKeySetChanged.count() > 0);

        QNetworkRequest req = client.createApiRequest(u"models"_s);
        QCOMPARE(req.header(QNetworkRequest::KnownHeaders::UserAgentHeader).toString(), GroqApiClient::userAgent());

        client.setApiKey(QString());
        QCOMPARE(client.apiKey(), QString());
        QCOMPARE(client.apiKeySet(), false);
    }

    void testGroqApiClientInjectCustomKeyStore() {
        GroqApiClient client;
        auto* customStore = new ApiKeyStore(&client);
        customStore->setStorageKeys(u"CustomSvc"_s, u"custom_key"_s, u"GroqTest/ApiKey"_s);

        client.setKeyStore(customStore);
        QCOMPARE(client.keyStore(), customStore);

        QSignalSpy spyKeyChanged(&client, &GroqApiClient::apiKeyChanged);
        customStore->setApiKey(u"gsk_injected_store_key"_s);

        QCOMPARE(client.apiKey(), u"gsk_injected_store_key"_s);
        QCOMPARE(client.apiKeySet(), true);
        QCOMPARE(spyKeyChanged.count(), 1);
    }

    void testGroqSttClientActivateTriggersLazyLoad() {
        QSettings settings;
        settings.setValue(u"GroqTest/ApiKey"_s, u"gsk_stt_integration_key"_s);
        settings.sync();

        GroqApiClient api;
        api.setStorageKeys(u"QTranscribeTestService"_s, u"test_groq_api_key"_s, u"GroqTest/ApiKey"_s);
        GroqSttClient stt;

        QCOMPARE(api.apiKeySet(), false);
        QCOMPARE(stt.isReady(), false);

        stt.setApiClient(&api);

        // Setting api client should not load key yet
        QCOMPARE(api.apiKeySet(), false);
        QCOMPARE(stt.isReady(), false);

        // Activating GroqSttClient triggers lazy load
        stt.activate();

        QTRY_VERIFY(stt.isReady());
        QCOMPARE(api.apiKey(), u"gsk_stt_integration_key"_s);
    }
};

QTEST_GUILESS_MAIN(TestGroqApiClient)
#include "test_groq_api_client.moc"
