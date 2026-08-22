cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED QTRANSCRIBE_VERSION)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" QTRANSCRIBE_VERSION_FILE)
        string(STRIP "${QTRANSCRIBE_VERSION_FILE}" QTRANSCRIBE_VERSION)
    else()
        set(QTRANSCRIBE_VERSION "1.0.0")
    endif()
endif()

project(QTranscribe VERSION ${QTRANSCRIBE_VERSION} LANGUAGES C CXX)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib:$ORIGIN/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

find_package(Qt6 6.11 REQUIRED COMPONENTS Gui Qml Quick QuickControls2 Network Multimedia DBus Widgets WaylandClient QuickEffects)
find_package(Qt6Keychain REQUIRED)

qt_standard_project_setup(REQUIRES 6.11)

option(ENABLE_QT_DEPLOYMENT "Enable Qt deployment script generation for standalone bundle packaging" ON)
option(BUILD_WL_CLIPBOARD "Build bundled wl-clipboard utilities" ON)
option(BUILD_KEYINJECTORD "Build keyinjectord helper daemon" ON)
option(ENABLE_CPACK_DEB "Configure CPack for Debian (.deb) package generation" OFF)

include(cmake/CompilerFlags.cmake)
if(BUILD_WL_CLIPBOARD)
    include(cmake/WlClipboard.cmake)
endif()
include(FetchContent)

find_package(Vulkan QUIET COMPONENTS glslc)
find_package(SPIRV-Headers QUIET CONFIG)
if(Vulkan_FOUND AND Vulkan_glslc_FOUND AND SPIRV-Headers_FOUND)
    message(STATUS "Vulkan SDK, glslc, and SPIRV-Headers found (${Vulkan_VERSION}): enabling Vulkan acceleration for whisper.cpp")
    set(GGML_VULKAN ON CACHE BOOL "whisper: enable Vulkan backend" FORCE)
    add_compile_definitions(GGML_USE_VULKAN)
else()
    message(STATUS "Vulkan SDK, glslc, or SPIRV-Headers not found: building whisper.cpp with CPU inference only")
    set(GGML_VULKAN OFF CACHE BOOL "whisper: enable Vulkan backend" FORCE)
endif()

set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "whisper: disable examples" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "whisper: disable tests" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "whisper: disable server" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "whisper: build static libraries" FORCE)

FetchContent_Declare(
    whisper
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
    GIT_TAG        v1.9.3
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(whisper)
if(NOT whisper_POPULATED)
    FetchContent_Populate(whisper)
    add_subdirectory(${whisper_SOURCE_DIR} ${whisper_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_subdirectory(src/core)
add_subdirectory(src/ui)
if(BUILD_KEYINJECTORD)
    add_subdirectory(src/keyinjectord)
endif()

qt_add_executable(qtranscribe
    src/main.cpp
)

target_compile_definitions(qtranscribe
    PRIVATE
        QTRANSCRIBE_VERSION="${PROJECT_VERSION}"
)

target_include_directories(qtranscribe
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
)

target_link_libraries(qtranscribe
    PRIVATE
        qtranscribe_core
        qtranscribe_ui
        qtranscribe_uiplugin
)

target_precompile_headers(qtranscribe
    PRIVATE
        <QApplication>
        <QQmlApplicationEngine>
        <QIcon>
)

# Custom targets for C++ and QML code formatting
find_program(CLANG_FORMAT_BIN
    NAMES clang-format clang-format-22 clang-format-21 clang-format-20 clang-format-19 clang-format-18 clang-format-17
)

find_program(QMLFORMAT_BIN
    NAMES qmlformat qmlformat-qt6
    HINTS
        ${QT6_INSTALL_BINS}
        ${_qt6_install_prefix}/bin
        /usr/lib64/qt6/bin
        /usr/lib/qt6/bin
)

if(CLANG_FORMAT_BIN)
    file(GLOB_RECURSE ALL_CXX_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    )
    add_custom_target(format-cpp
        COMMAND ${CLANG_FORMAT_BIN} -i -style=file ${ALL_CXX_SOURCE_FILES}
        COMMENT "Formatting C++ source files with clang-format"
    )
    add_custom_target(format-check-cpp
        COMMAND ${CLANG_FORMAT_BIN} --dry-run --Werror -style=file ${ALL_CXX_SOURCE_FILES}
        COMMENT "Checking C++ source file formatting with clang-format"
    )
endif()

if(QMLFORMAT_BIN)
    file(GLOB_RECURSE ALL_QML_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.qml
    )
    add_custom_target(format-qml
        COMMAND ${QMLFORMAT_BIN} --inplace ${ALL_QML_SOURCE_FILES}
        COMMENT "Formatting QML source files with qmlformat"
    )
endif()

if(TARGET format-cpp AND TARGET format-qml)
    add_custom_target(format
        DEPENDS format-cpp format-qml
        COMMENT "Formatting all C++ and QML files"
    )
elseif(TARGET format-cpp)
    add_custom_target(format DEPENDS format-cpp)
elseif(TARGET format-qml)
    add_custom_target(format DEPENDS format-qml)
endif()
include(GNUInstallDirs)

install(TARGETS qtranscribe
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(TARGET keyinjectord)
    install(TARGETS keyinjectord
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    add_dependencies(qtranscribe keyinjectord)
endif()

if(BUILD_WL_CLIPBOARD)
    install(TARGETS wl-copy wl-paste
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    add_dependencies(qtranscribe wl_clipboard_build)
endif()


if(ENABLE_CPACK_DEB)
    set(DESKTOP_AND_ICON_PREFIX "/usr/share")
else()
    set(DESKTOP_AND_ICON_PREFIX "${CMAKE_INSTALL_DATADIR}")
endif()

install(
    FILES assets/io.github.qtranscribe.desktop
    DESTINATION ${DESKTOP_AND_ICON_PREFIX}/applications
)

set(APP_ICON_SIZES 16 24 32 64 128 256 512)
foreach(SIZE ${APP_ICON_SIZES})
    install(FILES "assets/speech-to-text-${SIZE}.png"
        DESTINATION "${DESKTOP_AND_ICON_PREFIX}/icons/hicolor/${SIZE}x${SIZE}/apps"
        RENAME "io.github.qtranscribe.png"
    )
    install(FILES "assets/speech-to-text-${SIZE}.png"
        DESTINATION "${DESKTOP_AND_ICON_PREFIX}/icons/hicolor/${SIZE}x${SIZE}/apps"
        RENAME "qtranscribe.png"
    )
endforeach()

install(FILES "assets/mute.svg"
    DESTINATION "${DESKTOP_AND_ICON_PREFIX}/icons/hicolor/scalable/status"
    RENAME "qtranscribe-tray.svg"
)
install(FILES "assets/mute.svg"
    DESTINATION "${DESKTOP_AND_ICON_PREFIX}/icons/hicolor/symbolic/status"
    RENAME "qtranscribe-tray-symbolic.svg"
)
install(FILES "assets/microphone.svg"
    DESTINATION "${DESKTOP_AND_ICON_PREFIX}/icons/hicolor/scalable/status"
    RENAME "qtranscribe-tray-recording.svg"
)
install(FILES "assets/microphone.svg"
    DESTINATION "${DESKTOP_AND_ICON_PREFIX}/icons/hicolor/symbolic/status"
    RENAME "qtranscribe-tray-recording-symbolic.svg"
)

if(ENABLE_QT_DEPLOYMENT)
    qt_generate_deploy_qml_app_script(
        TARGET qtranscribe
        OUTPUT_SCRIPT deploy_script
        NO_TRANSLATIONS

        INCLUDE_PLUGIN_TYPES
            platforms
            wayland-graphics-integration-client
            wayland-shell-integration
            wayland-decoration-client
            iconengines
            imageformats
            multimedia
            platformthemes
            platforminputcontexts

        EXCLUDE_PLUGIN_TYPES
            xcbglintegrations
            egldeviceintegrations

        EXCLUDE_PLUGINS
            qxcb
            qeglfs
            qlinuxfb
            qvnc
            qvkkhrdisplay
            qminimalegl

        POST_INCLUDE_REGEXES
            ".*libQt6.*\\.so.*"
            ".*libqt6keychain.*\\.so.*"
            ".*libicu.*\\.so.*"
            ".*libq.*\\.so.*"
        POST_EXCLUDE_REGEXES
            ".*libX11.*"
            ".*libxcb.*"
            ".*libXau.*"
            ".*libXdmcp.*"
            ".*"
    )
    install(SCRIPT ${deploy_script})

    find_file(QT_WAYLAND_PLATFORM_PLUGIN
        NAMES libqwayland.so libqwayland-generic.so
        PATHS
            "${QT6_INSTALL_PLUGINS}/platforms"
            "${CMAKE_PREFIX_PATH}/plugins/platforms"
            "${_qt6_install_prefix}/plugins/platforms"
        PATH_SUFFIXES platforms
    )
    if(QT_WAYLAND_PLATFORM_PLUGIN)
        install(FILES "${QT_WAYLAND_PLATFORM_PLUGIN}"
            DESTINATION "plugins/platforms"
        )
    endif()

    if(ENABLE_CPACK_DEB)
        include(packaging/deb/CPackDebConfig.cmake)
    else()
        include(packaging/tarball/CPackTarballConfig.cmake)
    endif()
    include(CPack)
endif()

include(CTest)
enable_testing()

if(BUILD_TESTING)
    add_test(
        NAME QmlLint
        COMMAND ${CMAKE_COMMAND} --build . --target all_qmllint --config $<CONFIGURATION>
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    add_subdirectory(tests)
endif()
