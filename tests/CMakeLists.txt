find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(test_whisper_pipeline
    test_whisper_pipeline.cpp
)

target_link_libraries(test_whisper_pipeline
    PRIVATE
        qtranscribe_core
        Qt6::Core
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
