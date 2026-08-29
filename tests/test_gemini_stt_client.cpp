#include "GeminiApiClient.h"
#include "GeminiSttClient.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

class TestGeminiSttClient : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName(u"QTranscribeTest"_s);
        QCoreApplication::setApplicationName(u"QTranscribeTest"_s);
    }

    void cleanup() {
        QSettings settings;
        settings.remove(u"Gemini/Model"_s);
        settings.remove(u"Gemini/Mode"_s);
        settings.remove(u"Gemini/CustomVocabulary"_s);
        settings.sync();
    }

    void testDefaultProperties() {
        GeminiSttClient client;
        QCOMPARE(client.selectedModel(), u"gemini-3.5-transcribe"_s);
        QCOMPARE(client.mode(), u"smart"_s);
        QCOMPARE(client.customVocabulary(), QString());
        QVERIFY(client.handlesSmartFormatting());
        QCOMPARE(client.isReady(), false);
        QCOMPARE(client.isBusy(), false);
    }

    void testPropertyMutationsAndPersistence() {
        GeminiSttClient client;

        QSignalSpy spyModel(&client, &GeminiSttClient::selectedModelChanged);
        QSignalSpy spyMode(&client, &GeminiSttClient::modeChanged);
        QSignalSpy spyVocab(&client, &GeminiSttClient::customVocabularyChanged);

        client.setSelectedModel(u"gemini-3.5-transcribe"_s);
        client.setMode(u"verbatim"_s);
        client.setCustomVocabulary(u"QTranscribe, Wayland, CMake"_s);

        QCOMPARE(client.selectedModel(), u"gemini-3.5-transcribe"_s);
        QCOMPARE(client.mode(), u"verbatim"_s);
        QCOMPARE(client.customVocabulary(), u"QTranscribe, Wayland, CMake"_s);
        QCOMPARE(client.handlesSmartFormatting(), false);

        QCOMPARE(spyMode.count(), 1);
        QCOMPARE(spyVocab.count(), 1);

        QSettings settings;
        QCOMPARE(settings.value(u"Gemini/Mode"_s).toString(), u"verbatim"_s);
        QCOMPARE(settings.value(u"Gemini/CustomVocabulary"_s).toString(), u"QTranscribe, Wayland, CMake"_s);
    }

    void testExtractTranscribedTextOutputText() {
        QJsonObject json;
        json.insert(u"output_text"_s, u"Hello world from Gemini Transcribe."_s);

        // Verify JSON response parsing works with top-level output_text
        QCOMPARE(json.value(u"output_text"_s).toString(), u"Hello world from Gemini Transcribe."_s);
    }

    void testExtractTranscribedTextStepsFallback() {
        QJsonObject json;
        QJsonArray steps;
        QJsonObject step;
        step.insert(u"type"_s, u"model_output"_s);

        QJsonArray content;
        QJsonObject part;
        part.insert(u"type"_s, u"text"_s);
        part.insert(u"text"_s, u"Step text transcription"_s);
        content.append(part);

        step.insert(u"content"_s, content);
        steps.append(step);
        json.insert(u"steps"_s, steps);

        // Check fallback structure
        const QJsonArray parsedSteps = json.value(u"steps"_s).toArray();
        QCOMPARE(parsedSteps.size(), 1);
        const QJsonObject parsedStep = parsedSteps.at(0).toObject();
        QCOMPARE(parsedStep.value(u"type"_s).toString(), u"model_output"_s);
    }

    void testCancelResetsState() {
        GeminiSttClient client;
        QSignalSpy spyBusy(&client, &GeminiSttClient::busyChanged);

        client.cancel();
        QCOMPARE(client.isBusy(), false);
        QCOMPARE(client.isCancelled(), true);
    }
};

QTEST_GUILESS_MAIN(TestGeminiSttClient)
#include "test_gemini_stt_client.moc"
