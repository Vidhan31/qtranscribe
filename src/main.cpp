#include "AudioRecorder.h"
#include "DBusService.h"
#include "GroqApiClient.h"
#include "GroqLlmClient.h"
#include "GroqSttClient.h"
#include "GroqUsageTracker.h"
#include "SpeechController.h"
#include "TextInjectorClient.h"
#include "TranscriptionModel.h"

#include "GlobalShortcutManager.h"
#include "TranscriptionPipeline.h"
#include "WhisperModelManager.h"
#include "WhisperSttClient.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTimer>
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(QTranscribePlugin)

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

#ifndef QTRANSCRIBE_VERSION
#define QTRANSCRIBE_VERSION "0.0.0-dev"
#endif

int main(int argc, char* argv[]) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "wayland");
    }

    QApplication app(argc, argv);
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(u"Fusion"_s);
        QQuickStyle::setFallbackStyle(u"Fusion"_s);
    }
    app.setApplicationName(u"QTranscribe"_s);
    app.setApplicationVersion(u"" QTRANSCRIBE_VERSION ""_s);
    app.setOrganizationName(u"QTranscribe"_s);
    app.setOrganizationDomain(u"io.github.qtranscribe"_s);
    app.setDesktopFileName(u"io.github.qtranscribe"_s);
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(u"QTranscribe — Wayland native Speech-to-Text application"_s);
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption toggleOption({u"t"_s, u"toggle"_s}, u"Toggle speech-to-text recording"_s);
    QCommandLineOption startOption(u"start"_s, u"Start speech-to-text recording"_s);
    QCommandLineOption stopOption(u"stop"_s, u"Stop speech-to-text recording"_s);
    QCommandLineOption showOption({u"s"_s, u"show"_s}, u"Show and activate the main window"_s);
    QCommandLineOption quitOption({u"q"_s, u"quit"_s}, u"Quit the running application"_s);

    parser.addOption(toggleOption);
    parser.addOption(startOption);
    parser.addOption(stopOption);
    parser.addOption(showOption);
    parser.addOption(quitOption);

    parser.process(app);

    if (parser.isSet(toggleOption)) {
        if (DBusService::sendRemoteCommand(u"toggleRecording"_s))
            return 0;
    } else if (parser.isSet(startOption)) {
        if (DBusService::sendRemoteCommand(u"startRecording"_s))
            return 0;
    } else if (parser.isSet(stopOption)) {
        if (DBusService::sendRemoteCommand(u"stopRecording"_s))
            return 0;
    } else if (parser.isSet(quitOption)) {
        if (DBusService::sendRemoteCommand(u"quitApp"_s))
            return 0;
    } else if (parser.isSet(showOption)) {
        if (DBusService::sendRemoteCommand(u"showWindow"_s))
            return 0;
    } else {
        if (DBusService::sendRemoteCommand(u"showWindow"_s))
            return 0;
    }

    QIcon appIcon;
    static constexpr int kIconSizes[] = {16, 24, 32, 64, 128, 256, 512};
    for (int sz : kIconSizes) {
        appIcon.addFile(QString(u":/qt/qml/QTranscribe/assets/speech-to-text-%1.png"_s).arg(sz), QSize(sz, sz));
    }
    app.setWindowIcon(appIcon);

    QQmlApplicationEngine engine;

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("QTranscribe", "Main");

    auto* api = engine.singletonInstance<GroqApiClient*>("QTranscribe", "GroqApiClient");
    auto* stt = engine.singletonInstance<GroqSttClient*>("QTranscribe", "GroqSttClient");
    auto* whisperStt = engine.singletonInstance<WhisperSttClient*>("QTranscribe", "WhisperSttClient");
    auto* whisperModels = engine.singletonInstance<WhisperModelManager*>("QTranscribe", "WhisperModelManager");
    auto* llm = engine.singletonInstance<GroqLlmClient*>("QTranscribe", "GroqLlmClient");
    auto* tracker = engine.singletonInstance<GroqUsageTracker*>("QTranscribe", "GroqUsageTracker");
    auto* recorder = engine.singletonInstance<AudioRecorder*>("QTranscribe", "AudioRecorder");
    auto* shortcut = engine.singletonInstance<GlobalShortcutManager*>("QTranscribe", "GlobalShortcutManager");
    auto* injector = engine.singletonInstance<TextInjectorClient*>("QTranscribe", "TextInjectorClient");
    auto* history = engine.singletonInstance<TranscriptionModel*>("QTranscribe", "TranscriptionModel");
    auto* pipeline = engine.singletonInstance<TranscriptionPipeline*>("QTranscribe", "TranscriptionPipeline");
    auto* controller = engine.singletonInstance<SpeechController*>("QTranscribe", "SpeechController");

    if (stt && api)
        stt->setApiClient(api);
    if (whisperStt && whisperModels)
        whisperStt->setModelManager(whisperModels);
    if (llm && api)
        llm->setApiClient(api);
    if (tracker && api)
        tracker->setApiClient(api);

    if (pipeline) {
        pipeline->setAudioRecorder(recorder);
        pipeline->registerBackend(TranscriptionPipeline::Backend::Groq, stt);
        pipeline->registerBackend(TranscriptionPipeline::Backend::WhisperCpp, whisperStt);
        pipeline->setLlmClient(llm);
    }

    if (controller) {
        if (pipeline) {
            controller->setPipeline(pipeline);
        }
        controller->setApiClient(api);
        controller->setShortcutManager(shortcut);
        controller->setTextInjector(injector);
        controller->setHistoryModel(history);
        controller->initialize();

        auto* dbus = new DBusService(&app);
        dbus->registerController(controller);
    }

    if (parser.isSet(toggleOption) || parser.isSet(startOption)) {
        QTimer::singleShot(200ms, [controller]() {
            if (controller) {
                controller->startRecording();
            }
        });
    }

    if (parser.isSet(showOption)) {
        QTimer::singleShot(100ms, [controller]() {
            if (controller) {
                controller->showWindow();
            }
        });
    }

    return app.exec();
}
