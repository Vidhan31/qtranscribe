include(FetchContent)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(WAYLAND_PROTOCOLS wayland-protocols)

if(WAYLAND_PROTOCOLS_FOUND)
    pkg_get_variable(WAYLAND_PROTOCOLS_PKGDATADIR wayland-protocols pkgdatadir)
endif()
if(WAYLAND_CLIENT_FOUND)
    pkg_get_variable(WAYLAND_CLIENT_PKGDATADIR wayland-client pkgdatadir)
endif()

find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

FetchContent_Declare(
    wl_clipboard_src
    GIT_REPOSITORY https://github.com/bugaevc/wl-clipboard.git
    GIT_TAG        v2.3.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(wl_clipboard_src)

set(WL_SRC_DIR "${wl_clipboard_src_SOURCE_DIR}/src")

set(GEN_PROTO_HEADERS "")
set(GEN_PROTO_SOURCES "")

macro(wl_generate_protocol xml_path name header_name)
    set(header "${CMAKE_CURRENT_BINARY_DIR}/wl-clipboard-proto/${header_name}")
    set(code "${CMAKE_CURRENT_BINARY_DIR}/wl-clipboard-proto/${name}-protocol.c")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/wl-clipboard-proto")

    add_custom_command(
        OUTPUT "${header}" "${code}"
        COMMAND ${WAYLAND_SCANNER} client-header "${xml_path}" "${header}"
        COMMAND ${WAYLAND_SCANNER} private-code "${xml_path}" "${code}"
        DEPENDS "${xml_path}"
        VERBATIM
    )
    list(APPEND GEN_PROTO_HEADERS "${header}")
    list(APPEND GEN_PROTO_SOURCES "${code}")
endmacro()

find_file(XDG_SHELL_XML
    NAMES
        xdg-shell.xml
    PATHS
        "${WAYLAND_PROTOCOLS_PKGDATADIR}/stable/xdg-shell"
        /usr/share/wayland-protocols/stable/xdg-shell
        /usr/share/qt6/wayland/protocols/xdg-shell
)
if(XDG_SHELL_XML)
    wl_generate_protocol("${XDG_SHELL_XML}" "xdg-shell" "xdg-shell.h")
endif()

wl_generate_protocol("${WL_SRC_DIR}/protocol/wlr-data-control-unstable-v1.xml" "wlr-data-control-unstable-v1" "wlr-data-control.h")
wl_generate_protocol("${WL_SRC_DIR}/protocol/gtk-primary-selection.xml" "gtk-primary-selection" "gtk-primary-selection.h")
wl_generate_protocol("${WL_SRC_DIR}/protocol/gtk-shell.xml" "gtk-shell" "gtk-shell.h")

find_file(WP_PRIMARY_SELECTION_XML
    NAMES
        primary-selection-unstable-v1.xml
        wp-primary-selection-unstable-v1.xml
        wp-primary-selection.xml
    PATHS
        "${WAYLAND_PROTOCOLS_PKGDATADIR}/unstable/primary-selection"
        /usr/share/wayland-protocols/unstable/primary-selection
        /usr/share/qt6/wayland/protocols/wp-primary-selection
)
if(WP_PRIMARY_SELECTION_XML)
    wl_generate_protocol("${WP_PRIMARY_SELECTION_XML}" "wp-primary-selection" "wp-primary-selection.h")
endif()

find_file(XDG_ACTIVATION_XML
    NAMES
        xdg-activation-v1.xml
    PATHS
        "${WAYLAND_PROTOCOLS_PKGDATADIR}/staging/xdg-activation"
        /usr/share/wayland-protocols/staging/xdg-activation
        /usr/share/qt6/wayland/protocols/xdg-activation
)
if(XDG_ACTIVATION_XML)
    wl_generate_protocol("${XDG_ACTIVATION_XML}" "xdg-activation" "xdg-activation.h")
endif()

find_file(WAYLAND_XML
    NAMES
        wayland.xml
    PATHS
        "${WAYLAND_CLIENT_PKGDATADIR}"
        /usr/share/wayland
        /usr/share/qt6/wayland/protocols/wayland
)
if(WAYLAND_XML)
    wl_generate_protocol("${WAYLAND_XML}" "wayland" "wayland-client-protocol.h")
endif()

set(HAVE_XDG_SHELL_DEF "/* #undef HAVE_XDG_SHELL */")
if(XDG_SHELL_XML)
    set(HAVE_XDG_SHELL_DEF "#define HAVE_XDG_SHELL 1")
endif()

set(HAVE_WP_PRIMARY_SELECTION_DEF "/* #undef HAVE_WP_PRIMARY_SELECTION */")
if(WP_PRIMARY_SELECTION_XML)
    set(HAVE_WP_PRIMARY_SELECTION_DEF "#define HAVE_WP_PRIMARY_SELECTION 1")
endif()

set(HAVE_XDG_ACTIVATION_DEF "/* #undef HAVE_XDG_ACTIVATION */")
if(XDG_ACTIVATION_XML)
    set(HAVE_XDG_ACTIVATION_DEF "#define HAVE_XDG_ACTIVATION 1")
endif()

set(CONFIG_H "${CMAKE_CURRENT_BINARY_DIR}/wl-clipboard-config/config.h")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/wl-clipboard-config")
file(WRITE "${CONFIG_H}" "
#pragma once
#define HAVE_MEMFD 1
/* #undef HAVE_SHM_ANON */
${HAVE_XDG_SHELL_DEF}
#define HAVE_GTK_SHELL 1
#define HAVE_WLR_DATA_CONTROL 1
#define HAVE_GTK_PRIMARY_SELECTION 1
${HAVE_WP_PRIMARY_SELECTION_DEF}
${HAVE_XDG_ACTIVATION_DEF}
")

set(WL_COMMON_SOURCES
    "${WL_SRC_DIR}/types/copy-action.c"
    "${WL_SRC_DIR}/types/device-manager.c"
    "${WL_SRC_DIR}/types/device.c"
    "${WL_SRC_DIR}/types/keyboard.c"
    "${WL_SRC_DIR}/types/offer.c"
    "${WL_SRC_DIR}/types/popup-surface.c"
    "${WL_SRC_DIR}/types/registry.c"
    "${WL_SRC_DIR}/types/seat.c"
    "${WL_SRC_DIR}/types/shell-surface.c"
    "${WL_SRC_DIR}/types/shell.c"
    "${WL_SRC_DIR}/types/source.c"
    "${WL_SRC_DIR}/util/files.c"
    "${WL_SRC_DIR}/util/misc.c"
    "${WL_SRC_DIR}/util/string.c"
)

add_custom_target(wl_clipboard_proto_gen
    DEPENDS
        ${GEN_PROTO_HEADERS}
        ${GEN_PROTO_SOURCES}
)

set_source_files_properties(${GEN_PROTO_HEADERS} ${GEN_PROTO_SOURCES} PROPERTIES GENERATED TRUE)

set(WL_ALL_C_SOURCES
    ${WL_COMMON_SOURCES}
    "${WL_SRC_DIR}/wl-copy.c"
    "${WL_SRC_DIR}/wl-paste.c"
)
set_source_files_properties(${WL_ALL_C_SOURCES} PROPERTIES OBJECT_DEPENDS "${GEN_PROTO_HEADERS};${CONFIG_H}")

add_library(wl_clipboard_common STATIC
    ${WL_COMMON_SOURCES}
    ${GEN_PROTO_SOURCES}
    ${GEN_PROTO_HEADERS}
)
add_dependencies(wl_clipboard_common wl_clipboard_proto_gen)

target_include_directories(wl_clipboard_common PUBLIC
    "${WL_SRC_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/wl-clipboard-config"
    "${CMAKE_CURRENT_BINARY_DIR}/wl-clipboard-proto"
)

target_link_libraries(wl_clipboard_common PUBLIC
    PkgConfig::WAYLAND_CLIENT
)

set_target_properties(wl_clipboard_common PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF LINKER_LANGUAGE C)
target_compile_definitions(wl_clipboard_common PRIVATE _GNU_SOURCE _DEFAULT_SOURCE PROJECT_VERSION="2.3.0")
target_compile_options(wl_clipboard_common PRIVATE -std=gnu99 -Wno-unused-parameter)

add_executable(wl-copy
    "${WL_SRC_DIR}/wl-copy.c"
)
set_target_properties(wl-copy PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF LINKER_LANGUAGE C)
target_compile_definitions(wl-copy PRIVATE _GNU_SOURCE _DEFAULT_SOURCE PROJECT_VERSION="2.3.0")
target_compile_options(wl-copy PRIVATE -std=gnu99 -Wno-unused-parameter)
target_link_libraries(wl-copy PRIVATE wl_clipboard_common)
add_dependencies(wl-copy wl_clipboard_proto_gen)

add_executable(wl-paste
    "${WL_SRC_DIR}/wl-paste.c"
)
set_target_properties(wl-paste PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF LINKER_LANGUAGE C)
target_compile_definitions(wl-paste PRIVATE _GNU_SOURCE _DEFAULT_SOURCE PROJECT_VERSION="2.3.0")
target_compile_options(wl-paste PRIVATE -std=gnu99 -Wno-unused-parameter)
target_link_libraries(wl-paste PRIVATE wl_clipboard_common)
add_dependencies(wl-paste wl_clipboard_proto_gen)

add_custom_target(wl_clipboard_build DEPENDS wl-copy wl-paste)
