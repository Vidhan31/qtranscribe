add_library(qtranscribe_core STATIC
    AbstractSttClient.h
    AbstractSttClient.cpp
    AudioRecorder.h
    AudioRecorder.cpp
    GlobalShortcutManager.h
    GlobalShortcutManager.cpp
    GroqApiClient.h
    GroqApiClient.cpp
    GroqResponseParser.h
    GroqResponseParser.cpp
    HttpRequestRunner.h
    HttpRequestRunner.cpp
    GroqSttClient.h
    GroqSttClient.cpp
    WhisperWorker.h
    WhisperWorker.cpp
    WhisperSttClient.h
    WhisperSttClient.cpp
    WhisperModelStorage.h
    WhisperModelStorage.cpp
    WhisperModelManager.h
    WhisperModelManager.cpp
    GroqLlmClient.h
    GroqLlmClient.cpp
    GroqUsageTracker.h
    GroqUsageTracker.cpp
    ClipboardManager.h
    ClipboardManager.cpp
    DaemonConnector.h
    DaemonConnector.cpp
    TextInjectorClient.h
    TextInjectorClient.cpp
    SpeechController.h
    SpeechController.cpp
    TranscriptionPipeline.h
    TranscriptionPipeline.cpp
    TranscriptionModel.h
    TranscriptionModel.cpp
    DBusService.h
    DBusService.cpp
    LoggingCategories.h
    LoggingCategories.cpp
    TrayIconHelper.h
    LicenseHelper.h
    ColorUtils.h
    ColorUtils.cpp
)

target_include_directories(qtranscribe_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${whisper_SOURCE_DIR}/include
        ${whisper_SOURCE_DIR}/ggml/include
        ${whisper_SOURCE_DIR}/examples
)

target_link_libraries(qtranscribe_core
    PUBLIC
        whisper
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Network
        Qt6::Multimedia
        Qt6::DBus
        Qt6Keychain::Qt6Keychain
)

if(GGML_VULKAN AND TARGET Vulkan::Vulkan)
    target_link_libraries(qtranscribe_core PRIVATE Vulkan::Vulkan)
endif()

target_compile_definitions(qtranscribe_core
    PRIVATE
        QTRANSCRIBE_VERSION="${PROJECT_VERSION}"
)
