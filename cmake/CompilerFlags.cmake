if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
        $<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
        -Wformat=2
        -fstack-protector-strong
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-ffunction-sections>
        $<$<CONFIG:Release>:-fdata-sections>
        $<$<CONFIG:Release>:-fno-semantic-interposition>
        $<$<CONFIG:Release>:-U_FORTIFY_SOURCE>
        $<$<CONFIG:Release>:-D_FORTIFY_SOURCE=3>
    )

    add_link_options(
        $<$<CONFIG:Release>:-Wl,--gc-sections>
        $<$<CONFIG:Release>:-Wl,-O1>
        $<$<CONFIG:Release>:-Wl,--as-needed>
        $<$<CONFIG:Release>:-Wl,-z,relro,-z,now>
        $<$<CONFIG:Release>:-Wl,-z,noexecstack>
    )

    add_compile_definitions(
        $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>
        $<$<CONFIG:Release>:QT_NO_INFO_OUTPUT>
        $<$<CONFIG:Release>:KEYINJECTORD_NO_DEBUG_OUTPUT>
        $<$<CONFIG:Release>:QT_USE_QSTRINGBUILDER>
    )
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "C compiler launcher" FORCE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "CXX compiler launcher" FORCE)
    message(STATUS "Using ccache compiler launcher: ${CCACHE_PROGRAM}")
endif()
