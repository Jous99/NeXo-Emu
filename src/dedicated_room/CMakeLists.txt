# SPDX-FileCopyrightText: 2017 Citra Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

add_executable(citron-room
    precompiled_headers.h
    citron_room.cpp
)

# [NeXo] Same fix as src/citron/CMakeLists.txt's citron_rc: keep citron_room.rc out of the
# Boost/Qt include list so its RC-compile command line stays under Windows' length limit.
if (WIN32)
    add_library(citron_room_rc OBJECT citron_room.rc)
    set_target_properties(citron_room_rc PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    target_link_libraries(citron-room PRIVATE citron_room_rc)
else()
    target_sources(citron-room PRIVATE citron_room.rc)
endif()

# Robust static linking order using GNU ld group to resolve cycles
# Linker selection: For Clang, the global configuration defaults to lld.
# This guard ensures we only use BFD for legacy GCC builds.
if (NOT MSVC AND NOT APPLE)
    target_link_options(citron-room PRIVATE "$<$<CXX_COMPILER_ID:GNU>:-fuse-ld=bfd>")

    # Robust static linking under LTO: force-include these archives and allow cyclic resolution
    if (ENABLE_WEB_SERVICE)
        target_compile_definitions(citron-room PRIVATE -DENABLE_WEB_SERVICE)
        target_link_libraries(citron-room PRIVATE
            "-Wl,--start-group"
            "-Wl,--whole-archive" common network core web_service "-Wl,--no-whole-archive"
            "-Wl,--end-group"
        )
    else()
        target_link_libraries(citron-room PRIVATE
            "-Wl,--start-group"
            "-Wl,--whole-archive" common network core "-Wl,--no-whole-archive"
            "-Wl,--end-group"
        )
    endif()
else()
    # Fallback for MSVC (no --start-group/--end-group)
    target_link_libraries(citron-room PRIVATE common network core)
    if (ENABLE_WEB_SERVICE)
        target_compile_definitions(citron-room PRIVATE -DENABLE_WEB_SERVICE)
        target_link_libraries(citron-room PRIVATE web_service)
    endif()
endif()

target_link_libraries(citron-room PRIVATE OpenSSL::Crypto)
if (MSVC)
    target_link_libraries(citron-room PRIVATE getopt)
endif()
target_link_libraries(citron-room PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

if (MINGW)
    include(CopyMinGWDeps)
    copy_mingw_deps(citron-room SEARCH_PATHS "${FFmpeg_PATH}/bin" "${QT_TARGET_PATH}/bin")
endif()

if(UNIX AND NOT APPLE)
    install(TARGETS citron-room)
endif()

if (CITRON_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(citron-room PRIVATE precompiled_headers.h)
endif()

create_target_directory_groups(citron-room)
