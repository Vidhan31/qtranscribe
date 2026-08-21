find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(test_whisper_pipeline
    test_whisper_pipeline.cpp
)

target_link_libraries(test_whisper_pipeline
    PRIVATE
        qtranscribe_core
        Qt6::Core
        Qt6::Gui
        Qt6::Multimedia
        Qt6::Test
        Qt6::Network
)

target_include_directories(test_whisper_pipeline
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME WhisperPipelineTest
    COMMAND test_whisper_pipeline
)

qt_add_executable(test_whisper_storage
    test_whisper_storage.cpp
)

target_link_libraries(test_whisper_storage
    PRIVATE
        qtranscribe_core
        Qt6::Core
        Qt6::Test
        Qt6::Network
)

target_include_directories(test_whisper_storage
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME WhisperStorageTest
    COMMAND test_whisper_storage
)

qt_add_executable(test_transcription_pipeline
    test_transcription_pipeline.cpp
)

target_link_libraries(test_transcription_pipeline
    PRIVATE
        qtranscribe_core
        Qt6::Core
        Qt6::Gui
        Qt6::Multimedia
        Qt6::Test
        Qt6::Network
)

target_include_directories(test_transcription_pipeline
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME TranscriptionPipelineTest
    COMMAND test_transcription_pipeline
)

qt_add_executable(test_http_request_runner
    test_http_request_runner.cpp
)

target_link_libraries(test_http_request_runner
    PRIVATE
        qtranscribe_core
        Qt6::Core
        Qt6::Test
        Qt6::Network
)

target_include_directories(test_http_request_runner
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core
)

add_test(
    NAME HttpRequestRunnerTest
    COMMAND test_http_request_runner
)

qt_add_executable(test_ipc_server
    test_ipc_server.cpp
    ${CMAKE_SOURCE_DIR}/src/keyinjectord/ipc_server.cpp
)

target_link_libraries(test_ipc_server
    PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::Network
)

target_include_directories(test_ipc_server
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/keyinjectord
)

add_test(
    NAME IpcServerTest
    COMMAND test_ipc_server
)

file(REAL_PATH "${CMAKE_PREFIX_PATH}/lib" QT_TEST_LIB_DIR EXPAND_TILDE)

set_tests_properties(
    WhisperPipelineTest
    WhisperStorageTest
    TranscriptionPipelineTest
    HttpRequestRunnerTest
    IpcServerTest
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_MEDIA_BACKEND=null;LD_LIBRARY_PATH=${QT_TEST_LIB_DIR}:$ENV{LD_LIBRARY_PATH}"
)
