set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_library(qtranscribe_ui STATIC)
qt_add_qml_module(qtranscribe_ui
    URI QTranscribe
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/QTranscribe"
    IMPORTS
        QtQuick
        QtQuick.Controls
        QtQuick.Layouts
    SOURCES
        ${CMAKE_SOURCE_DIR}/src/core/AudioRecorder.h
        ${CMAKE_SOURCE_DIR}/src/core/GlobalShortcutManager.h
        ${CMAKE_SOURCE_DIR}/src/core/GroqApiClient.h
        ${CMAKE_SOURCE_DIR}/src/core/ApiRequestHandler.h
        ${CMAKE_SOURCE_DIR}/src/core/GroqSttClient.h
        ${CMAKE_SOURCE_DIR}/src/core/GroqLlmClient.h
        ${CMAKE_SOURCE_DIR}/src/core/GroqUsageTracker.h
        ${CMAKE_SOURCE_DIR}/src/core/SpeechController.h
        ${CMAKE_SOURCE_DIR}/src/core/TextInjectorClient.h
        ${CMAKE_SOURCE_DIR}/src/core/TranscriptionModel.h
        ${CMAKE_SOURCE_DIR}/src/core/TrayIconHelper.h
        ${CMAKE_SOURCE_DIR}/src/core/LicenseHelper.h
        ${CMAKE_SOURCE_DIR}/src/core/ColorUtils.h
    QML_FILES
        Main.qml
        SpeechPanel.qml
        HistoryView.qml
        UsageView.qml
        CreditsView.qml
        LicenseView.qml
        Theme.qml
        StateBadge.qml
        StatusBanner.qml
        MetricCard.qml
        QuotaMeter.qml
        ShortcutGuideDialog.qml
        controls/StyledButton.qml
        controls/StyledCard.qml
        controls/StyledComboBox.qml
        controls/StyledDivider.qml
        controls/StyledIcon.qml
        controls/StyledSlider.qml
        controls/StyledText.qml
        controls/StyledTextField.qml
        settings/PreferenceDivider.qml
        settings/PreferenceCard.qml
        settings/PreferenceRow.qml
        settings/PreferenceSwitch.qml
        settings/ApiSettingsPage.qml
        settings/SpeechSettingsPage.qml
        settings/LlmSettingsPage.qml
        settings/SystemSettingsPage.qml
)

qt_add_resources(qtranscribe_ui "qtranscribe_license"
    PREFIX "/qt/qml/QTranscribe/license"
    BASE "${CMAKE_SOURCE_DIR}"
    FILES
        ${CMAKE_SOURCE_DIR}/LICENSE
)

qt_add_resources(qtranscribe_ui "qtranscribe_assets"
    PREFIX "/qt/qml/QTranscribe/assets"
    BASE "${CMAKE_SOURCE_DIR}/assets"
    FILES
        ${CMAKE_SOURCE_DIR}/assets/chime_start.wav
        ${CMAKE_SOURCE_DIR}/assets/chime_stop.wav
        ${CMAKE_SOURCE_DIR}/assets/speech-to-text-16.png
        ${CMAKE_SOURCE_DIR}/assets/speech-to-text-24.png
        ${CMAKE_SOURCE_DIR}/assets/speech-to-text-32.png
        ${CMAKE_SOURCE_DIR}/assets/speech-to-text-64.png
        ${CMAKE_SOURCE_DIR}/assets/speech-to-text-128.png
        ${CMAKE_SOURCE_DIR}/assets/speech-to-text-256.png
        ${CMAKE_SOURCE_DIR}/assets/speech-to-text-512.png
        ${CMAKE_SOURCE_DIR}/assets/mute.svg
        ${CMAKE_SOURCE_DIR}/assets/mute-dark.svg
        ${CMAKE_SOURCE_DIR}/assets/microphone.svg
        ${CMAKE_SOURCE_DIR}/assets/microphone-dark.svg
        ${CMAKE_SOURCE_DIR}/assets/icons/mic.svg
        ${CMAKE_SOURCE_DIR}/assets/icons/stop.svg
        ${CMAKE_SOURCE_DIR}/assets/icons/spinner.svg
        ${CMAKE_SOURCE_DIR}/assets/icons/keyboard.svg
        ${CMAKE_SOURCE_DIR}/assets/icons/bolt.svg
        ${CMAKE_SOURCE_DIR}/assets/icons/copy.svg
        ${CMAKE_SOURCE_DIR}/assets/icons/trash.svg
        ${CMAKE_SOURCE_DIR}/assets/icons/check.svg
        ${CMAKE_SOURCE_DIR}/assets/icons/history.svg
        ${CMAKE_SOURCE_DIR}/assets/icons/activity.svg
        ${CMAKE_SOURCE_DIR}/assets/icons/key.svg
        ${CMAKE_SOURCE_DIR}/assets/icons/speech.svg
        ${CMAKE_SOURCE_DIR}/assets/icons/sparkles.svg
        ${CMAKE_SOURCE_DIR}/assets/icons/info.svg
        ${CMAKE_SOURCE_DIR}/assets/icons/license.svg
)

target_include_directories(qtranscribe_ui
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(qtranscribe_ui
    PRIVATE
        QTRANSCRIBE_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(qtranscribe_ui
    PUBLIC
        qtranscribe_core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickEffects
        Qt6::Widgets
)

target_precompile_headers(qtranscribe_ui
    PRIVATE
        <QGuiApplication>
        <QQmlApplicationEngine>
        <QQuickItem>
        <QObject>
        <QString>
)

add_custom_command(TARGET qtranscribe_ui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_BINARY_DIR}/QTranscribe" "${CMAKE_SOURCE_DIR}/QTranscribe"
    COMMENT "Creating QTranscribe module symlink for editor and qmllint tooling"
)
