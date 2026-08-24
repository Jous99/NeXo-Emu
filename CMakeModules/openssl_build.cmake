# SPDX-FileCopyrightText: 2026 citron Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# CMakeModules/openssl_build.cmake — Build OpenSSL from source for cross-compilation
#
# OpenSSL uses Perl/Configure, not CMake.  This module downloads the source via
# CPM and builds it with execute_process during cmake configure.
#
# Native MSYS2 builds also prefer the downloaded source build so behavior
# stays consistent across Windows and Linux-to-Windows cross builds.

# CMakeModules/openssl_build.cmake — Build OpenSSL from source
#
# Builds a static OpenSSL for any target: Windows PE (mingw64) via native
# MSYS2 or Linux-to-Windows cross-compile, or Linux ELF for native builds.
# OpenSSL uses Perl/Configure, not CMake; this module drives it with
# execute_process during cmake configure.

set(_OPENSSL_VERSION "3.4.1")

# ── clang-cl global artifact cache ──────────────────────────────────────────
# When CLANGCL_OPENSSL_CACHE_DIR is set (by build-clangtron-windows.sh), the
# built OpenSSL install is stored there (under CPM_SOURCE_CACHE) rather than
# in the per-stage cmake binary dir.  This lets generate/csgenerate/use stages
# share a single OpenSSL build and survive binary-dir rebuilds.
if (DEFINED CLANGCL_OPENSSL_CACHE_DIR AND NOT "${CLANGCL_OPENSSL_CACHE_DIR}" STREQUAL "")
    set(_OPENSSL_INSTALL "${CLANGCL_OPENSSL_CACHE_DIR}")
    message(STATUS "[OpenSSL] Using global clang-cl cache dir: ${_OPENSSL_INSTALL}")
else()
    set(_OPENSSL_INSTALL "${CMAKE_BINARY_DIR}/externals/openssl-install")
endif()

# OpenSSL's Perl Configure script cannot handle spaces in the working directory
# or source path (same limitation as FFmpeg's configure).  When CMAKE_BINARY_DIR
# contains a space (e.g. a username like "Gaming PC" or a folder like
# "citron whitespace test"), redirect both the build staging area and the install
# prefix to a guaranteed-space-free location.
#
# Priority: %SystemRoot%\Temp (C:\Windows\Temp) → $TMPDIR → /tmp
string(FIND "${CMAKE_BINARY_DIR}" " " _openssl_bindir_has_space)
if (_openssl_bindir_has_space GREATER -1)
    if (DEFINED ENV{SystemRoot})
        string(REPLACE "\\" "/" _openssl_sysroot "$ENV{SystemRoot}")
        set(_OPENSSL_SAFE_TMP "${_openssl_sysroot}/Temp/citron-openssl-${CMAKE_SYSTEM_NAME}")
    elseif(DEFINED ENV{TMPDIR})
        set(_OPENSSL_SAFE_TMP "$ENV{TMPDIR}/citron-openssl-${CMAKE_SYSTEM_NAME}")
    else()
        set(_OPENSSL_SAFE_TMP "/tmp/citron-openssl-${CMAKE_SYSTEM_NAME}")
    endif()
    set(_OPENSSL_BUILD_DIR  "${_OPENSSL_SAFE_TMP}/build")
    set(_OPENSSL_INSTALL    "${_OPENSSL_SAFE_TMP}/install")
    message(STATUS "[OpenSSL] Binary dir has spaces — redirecting build/install to ${_OPENSSL_SAFE_TMP}")
else()
    set(_OPENSSL_BUILD_DIR "${CMAKE_BINARY_DIR}/externals/openssl-build")
endif()

# Determine the OpenSSL build target and toolchain.
#
# Three cases:
#   1. MSYS2 native (WIN32=TRUE)
#      Target: mingw64   CC: clang (CLANG64 sysroot resolves it)
#   2. Linux → Windows cross-compile (CMAKE_C_COMPILER contains x86_64-w64-mingw32)
#      Target: mingw64   CC: clang + cross-prefix (tools prepended to PATH)
#   3. Linux native (everything else)
#      Target: (empty → OpenSSL auto-detects linux-x86_64 etc.)
#              CC: CMAKE_C_COMPILER   AR: CMAKE_AR   RANLIB: CMAKE_RANLIB

set(_OPENSSL_CROSS  "")
set(_OPENSSL_TARGET "")
set(_OPENSSL_CC     "${CMAKE_C_COMPILER}")
set(_OPENSSL_AR     "${CMAKE_AR}")
set(_OPENSSL_RANLIB "${CMAKE_RANLIB}")
set(_OPENSSL_RC     "${CMAKE_RC_COMPILER}")

set(_OPENSSL_BUILD_TOOL make)
set(_OPENSSL_PARALLEL_ARGS "-j${_NPROC}")
set(_OPENSSL_SSL_NAME "libssl.a")
set(_OPENSSL_CRYPTO_NAME "libcrypto.a")

set(_OPENSSL_EXTRA_CFLAGS "")

if (WIN32 AND MSVC AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(_OPENSSL_TARGET "VC-WIN64A")
    set(_OPENSSL_CC "clang-cl")
    set(_OPENSSL_AR "llvm-lib")
    set(_OPENSSL_RANLIB "")
    set(_OPENSSL_RC "rc")
    find_program(_OPENSSL_JOM jom)
    if (_OPENSSL_JOM)
        set(_OPENSSL_BUILD_TOOL "${_OPENSSL_JOM}")
    else()
        set(_OPENSSL_BUILD_TOOL nmake)
    endif()
    set(_OPENSSL_SSL_NAME "libssl.lib")
    set(_OPENSSL_CRYPTO_NAME "libcrypto.lib")
    # LTO/PGO flags from build-clangtron-windows.sh's clang-cl stage (pgo_flags_dash).
    # Only read inside VC-WIN64A — llvm-mingw and Linux paths never reach this branch.
    if (DEFINED CLANGCL_OPENSSL_EXTRA_CFLAGS AND NOT "${CLANGCL_OPENSSL_EXTRA_CFLAGS}" STREQUAL "")
        set(_OPENSSL_EXTRA_CFLAGS "${CLANGCL_OPENSSL_EXTRA_CFLAGS}")
    endif()
elseif (CMAKE_CROSSCOMPILING AND CMAKE_C_COMPILER MATCHES "x86_64-w64-mingw32")
    # Case 2: Linux → Windows cross-compile with llvm-mingw.
    set(_OPENSSL_TARGET "mingw64")
    set(_OPENSSL_CROSS  "x86_64-w64-mingw32-")
    set(_OPENSSL_CC     "clang")
    set(_OPENSSL_AR     "llvm-ar")
    set(_OPENSSL_RANLIB "llvm-ranlib")
    set(_OPENSSL_RC     "windres")
elseif (WIN32)
    # Case 1: MSYS2 native Windows build.  CMake's WIN32 is target-based, so
    # this must come after the Linux-to-Windows cross-compile case above.
    set(_OPENSSL_TARGET "mingw64")
    set(_OPENSSL_CC     "clang")
    set(_OPENSSL_AR     "llvm-ar")
    set(_OPENSSL_RANLIB "llvm-ranlib")
    set(_OPENSSL_RC     "windres")
endif()
# Case 3: Linux native — _OPENSSL_TARGET stays empty (auto-detect), tools stay
# as CMAKE_C_COMPILER / CMAKE_AR / CMAKE_RANLIB set above.

set(_OPENSSL_IS_MINGW_CROSS FALSE)
if (_OPENSSL_CROSS)
    set(_OPENSSL_IS_MINGW_CROSS TRUE)
endif()

function(_citron_detect_openssl_libdir out_var)
    set(_detected "")
    foreach(_candidate_libdir lib64 lib)
        if (EXISTS "${_OPENSSL_INSTALL}/${_candidate_libdir}/${_OPENSSL_SSL_NAME}" AND
            EXISTS "${_OPENSSL_INSTALL}/${_candidate_libdir}/${_OPENSSL_CRYPTO_NAME}")
            set(_detected "${_candidate_libdir}")
            break()
        endif()
    endforeach()
    set(${out_var} "${_detected}" PARENT_SCOPE)
endfunction()

function(_citron_publish_openssl_imports)
    _citron_detect_openssl_libdir(_OPENSSL_PUBLISH_LIBDIR)
    if (NOT _OPENSSL_PUBLISH_LIBDIR)
        message(WARNING "[OpenSSL] Static libraries (${_OPENSSL_SSL_NAME} and/or ${_OPENSSL_CRYPTO_NAME}) not found under ${_OPENSSL_INSTALL}/{lib64,lib}")
        return()
    endif()

    set(OPENSSL_ROOT_DIR     "${_OPENSSL_INSTALL}" CACHE PATH     "" FORCE)
    set(OPENSSL_INCLUDE_DIR  "${_OPENSSL_INSTALL}/include" CACHE PATH "" FORCE)
    set(OPENSSL_SSL_LIBRARY  "${_OPENSSL_INSTALL}/${_OPENSSL_PUBLISH_LIBDIR}/${_OPENSSL_SSL_NAME}" CACHE FILEPATH "" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY "${_OPENSSL_INSTALL}/${_OPENSSL_PUBLISH_LIBDIR}/${_OPENSSL_CRYPTO_NAME}" CACHE FILEPATH "" FORCE)
    set(OPENSSL_FOUND TRUE CACHE BOOL "" FORCE)

    # Platform-specific link requirements:
    #   Windows PE (MSYS2 native or cross-compile): ws2_32 for Winsock, crypt32 for CryptoAPI
    #   Linux ELF native: dl for any remaining dynamic resolution paths
    if (WIN32 OR CMAKE_CROSSCOMPILING)
        set(_openssl_extra_libs "ws2_32;crypt32")
    else()
        set(_openssl_extra_libs "dl")
    endif()

    if (NOT TARGET OpenSSL::Crypto)
        add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)
    endif()
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION              "${OPENSSL_CRYPTO_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES  "${OPENSSL_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES       "${_openssl_extra_libs}")

    if (NOT TARGET OpenSSL::SSL)
        add_library(OpenSSL::SSL STATIC IMPORTED GLOBAL)
    endif()
    set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LOCATION              "${OPENSSL_SSL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES  "${OPENSSL_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES       "OpenSSL::Crypto")
endfunction()

_citron_detect_openssl_libdir(_OPENSSL_LIBDIR)

# If a previous configure left stale OpenSSL cache entries behind, clear them
# before we decide whether to use the cached install or rebuild.
if (CMAKE_CROSSCOMPILING)
    unset(OPENSSL_FOUND CACHE)
    unset(OPENSSL_ROOT_DIR CACHE)
    unset(OPENSSL_INCLUDE_DIR CACHE)
    unset(OPENSSL_SSL_LIBRARY CACHE)
    unset(OPENSSL_CRYPTO_LIBRARY CACHE)
endif()

# A Windows cross OpenSSL archive compiled with host clang contains ELF members,
# which lld later rejects as "unknown file type".  Treat any cached cross build
# whose generated Makefile lacks the expected prefix as stale and rebuild it.
if (_OPENSSL_LIBDIR AND _OPENSSL_IS_MINGW_CROSS)
    set(_openssl_cache_valid TRUE)
    set(_openssl_makefile "${_OPENSSL_BUILD_DIR}/Makefile")
    if (EXISTS "${_openssl_makefile}")
        file(STRINGS "${_openssl_makefile}" _openssl_cross_compile_line
            REGEX "^CROSS_COMPILE=" LIMIT_COUNT 1)
        if (NOT _openssl_cross_compile_line STREQUAL "CROSS_COMPILE=${_OPENSSL_CROSS}")
            set(_openssl_cache_valid FALSE)
        endif()
    else()
        set(_openssl_cache_valid FALSE)
    endif()

    if (NOT _openssl_cache_valid)
        message(STATUS "[OpenSSL] Cached Windows cross build is stale; rebuilding with ${_OPENSSL_CROSS} tools")
        file(REMOVE_RECURSE "${_OPENSSL_BUILD_DIR}" "${_OPENSSL_INSTALL}")
        set(_OPENSSL_LIBDIR "")
    endif()
endif()

# Flag sentinel for VC-WIN64A: if cached flags don't match current, force rebuild.
# The CLANGCL_OPENSSL_CACHE_DIR path key is the primary protection; this is a fallback.
if (_OPENSSL_LIBDIR AND _OPENSSL_TARGET STREQUAL "VC-WIN64A")
    set(_openssl_flags_sentinel "${_OPENSSL_INSTALL}/.citron-clangcl-extra-cflags")
    set(_openssl_flags_sentinel_content "")
    if (EXISTS "${_openssl_flags_sentinel}")
        file(READ "${_openssl_flags_sentinel}" _openssl_flags_sentinel_content)
        string(STRIP "${_openssl_flags_sentinel_content}" _openssl_flags_sentinel_content)
    endif()
    if (NOT _openssl_flags_sentinel_content STREQUAL "${_OPENSSL_EXTRA_CFLAGS}")
        message(STATUS "[OpenSSL] Cached build's recorded flags don't match the current build's; rebuilding")
        file(REMOVE_RECURSE "${_OPENSSL_BUILD_DIR}" "${_OPENSSL_INSTALL}")
        set(_OPENSSL_LIBDIR "")
    endif()
endif()

# Reuse a previously built cross OpenSSL only when the install tree is intact.
if (_OPENSSL_LIBDIR)
    _citron_publish_openssl_imports()
    message(STATUS "[OpenSSL] Using cached static build at ${_OPENSSL_INSTALL}")
    return()
endif()

# ── Download source via CPM ──────────────────────────────────────────────────
CPMAddPackage(
    NAME openssl_src
    URL "https://github.com/openssl/openssl/releases/download/openssl-${_OPENSSL_VERSION}/openssl-${_OPENSSL_VERSION}.tar.gz"
    DOWNLOAD_ONLY YES
)

if (NOT openssl_src_ADDED)
    message(WARNING "[OpenSSL] Source download failed — OpenSSL will not be available")
    return()
endif()

# ── Build from source ────────────────────────────────────────────────────────
if (PERL_EXECUTABLE)
    set(_PERL "${PERL_EXECUTABLE}")
else()
    find_program(_PERL perl REQUIRED)
endif()
if (NOT _PERL)
    message(FATAL_ERROR "[OpenSSL] Perl is required to build OpenSSL from source")
endif()

# OpenSSL's Configure script often generates broken relative paths in the Makefile
# when the source and build directories are on different drives (e.g. source on C:,
# build on D:).  To avoid this, we copy the source into the build directory.
set(_OPENSSL_LOCAL_SRC "${_OPENSSL_BUILD_DIR}/src")

if (NOT EXISTS "${_OPENSSL_LOCAL_SRC}/Configure")
    message(STATUS "[OpenSSL] Copying source to build directory to avoid cross-drive path issues...")
    file(REMOVE_RECURSE "${_OPENSSL_LOCAL_SRC}")
    file(COPY "${openssl_src_SOURCE_DIR}/" DESTINATION "${_OPENSSL_LOCAL_SRC}")
endif()

# For Linux cross-compile (case 2) the llvm-mingw tools must be in PATH so
# x86_64-w64-mingw32-clang is found. Prepend the toolchain dir from CMAKE_C_COMPILER.
set(_openssl_env_path "$ENV{PATH}")
if (_OPENSSL_CROSS)
    get_filename_component(_openssl_tool_dir "${CMAKE_C_COMPILER}" DIRECTORY)
    set(_openssl_env_path "${_openssl_tool_dir}:$ENV{PATH}")
endif()
if (WIN32 AND MSVC AND _OPENSSL_NASM)
    get_filename_component(_openssl_nasm_dir "${_OPENSSL_NASM}" DIRECTORY)
    set(_openssl_env_path "${_openssl_nasm_dir};${_openssl_env_path}")
endif()

# Determine what we are building for (for the status message).
if (_OPENSSL_TARGET)
    set(_openssl_target_label "${_OPENSSL_TARGET}")
else()
    set(_openssl_target_label "native (auto)")
endif()

message(STATUS "[OpenSSL] Building OpenSSL ${_OPENSSL_VERSION} from source (static, ${_openssl_target_label})...")

file(MAKE_DIRECTORY "${_OPENSSL_BUILD_DIR}")

# AES-NI / assembly: OpenSSL's x86_64 assembly (aesni-x86_64.pl, sha256-x86_64.pl, etc.)
# requires NASM as a host tool.  NASM is only needed for x86_64 targets; arm64
# targets (darwin64-arm64-cc on Apple Silicon, linux-aarch64 on native aarch64)
# use OpenSSL's own Perl-generated ARM assembly and have no NASM dependency.
#
# Note: for the Linux→Windows cross-compile (Case 2), OpenSSL's mingw64 target
# generates x86_64 NASM object files using the *host* nasm binary — not a
# prefixed cross-tool — so the same nasm package used for FFmpeg works here too.
if (_OPENSSL_TARGET STREQUAL "mingw64" OR
    (NOT _OPENSSL_TARGET AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64"))
    find_program(_OPENSSL_NASM nasm)
    if (NOT _OPENSSL_NASM)
        message(FATAL_ERROR
            "[OpenSSL] NASM not found in PATH.  OpenSSL's AES-NI / SHA-NI assembly "
            "optimisations require NASM.\n"
            "  Ubuntu/Debian : sudo apt-get install nasm\n"
            "  Fedora/RHEL   : sudo dnf install nasm\n"
            "  openSUSE      : sudo zypper install nasm\n"
            "  MSYS2         : pacman -S mingw-w64-clang-x86_64-nasm\n"
            "  macOS (Intel) : brew install nasm\n"
            "Both build scripts (build-citron-linux.sh, build-clangtron-windows.sh) "
            "already install nasm as part of their dependency setup.")
    endif()
    message(STATUS "[OpenSSL] NASM found: ${_OPENSSL_NASM}")
endif()

# Build Configure argument list
set(_OPENSSL_CONFIGURE_ARGS
    ${_OPENSSL_TARGET}
    --prefix=${_OPENSSL_INSTALL}
    no-shared
    no-dso
    no-tests
    no-docs
    no-apps
    no-capieng
    no-winstore
)
if (_OPENSSL_CROSS)
    list(APPEND _OPENSSL_CONFIGURE_ARGS "--cross-compile-prefix=${_OPENSSL_CROSS}")
endif()
if (_OPENSSL_TARGET STREQUAL "VC-WIN64A")
    # The rest of the project is forced onto the dynamic CRT (/MD, /MDd) via
    # CMAKE_MSVC_RUNTIME_LIBRARY in the top-level CMakeLists.txt. OpenSSL's
    # VC-WIN64A Configure target does not automatically match that; passing
    # -MD here pins OpenSSL's own build to the same CRT so its static libs
    # don't get linked against a mismatched runtime (which otherwise shows up
    # as CRT-mismatch link errors, e.g. LNK2038/LNK4098-style conflicts).
    list(APPEND _OPENSSL_CONFIGURE_ARGS "-MD")
    if (_OPENSSL_EXTRA_CFLAGS)
        # Configure uses [-Dxxx] [-fxxx] bare token syntax — pass dash-prefixed flags,
        # not /clang:-prefixed (which is clang-cl driver syntax, not plain token-splitting).
        separate_arguments(_openssl_extra_cflags_list UNIX_COMMAND "${_OPENSSL_EXTRA_CFLAGS}")
        list(APPEND _OPENSSL_CONFIGURE_ARGS ${_openssl_extra_cflags_list})
    endif()
endif()
list(APPEND _OPENSSL_CONFIGURE_ARGS "CC=${_OPENSSL_CC}" "AR=${_OPENSSL_AR}")
if (_OPENSSL_RANLIB)
    list(APPEND _OPENSSL_CONFIGURE_ARGS "RANLIB=${_OPENSSL_RANLIB}")
endif()
if (_OPENSSL_RC)
    list(APPEND _OPENSSL_CONFIGURE_ARGS "RC=${_OPENSSL_RC}")
endif()

# Configure
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${_openssl_env_path}"
        ${_PERL} "${_OPENSSL_LOCAL_SRC}/Configure"
        ${_OPENSSL_CONFIGURE_ARGS}
    WORKING_DIRECTORY "${_OPENSSL_BUILD_DIR}"
    RESULT_VARIABLE _ssl_config_result
)

if (NOT _ssl_config_result EQUAL 0)
    message(FATAL_ERROR "[OpenSSL] Configure failed (exit ${_ssl_config_result}). "
        "Check that Perl and a MinGW-compatible toolchain are available.")
endif()

# Build + install (just libraries, no apps)
include(ProcessorCount)
ProcessorCount(_NPROC)
if (_NPROC EQUAL 0)
    set(_NPROC 4)
endif()
if (_OPENSSL_BUILD_TOOL MATCHES "(^|[/\\\\])(make|jom)(\\.exe)?$")
    set(_OPENSSL_PARALLEL_ARGS "-j${_NPROC}")
else()
    set(_OPENSSL_PARALLEL_ARGS "")
endif()
set(_OPENSSL_INSTALL_TOOL "${_OPENSSL_BUILD_TOOL}")
if (_OPENSSL_BUILD_TOOL MATCHES "(^|[/\\\\])jom(\\.exe)?$")
    set(_OPENSSL_INSTALL_TOOL nmake)
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${_openssl_env_path}"
        ${_OPENSSL_BUILD_TOOL} ${_OPENSSL_PARALLEL_ARGS} build_libs
    WORKING_DIRECTORY "${_OPENSSL_BUILD_DIR}"
    RESULT_VARIABLE _ssl_build_result
    OUTPUT_QUIET
)

if (NOT _ssl_build_result EQUAL 0)
    message(FATAL_ERROR "[OpenSSL] Build failed (exit ${_ssl_build_result}).")
endif()

# VC install expects this file even when clang-cl does not emit it.
if (_OPENSSL_INSTALL_TOOL STREQUAL "nmake" AND
    NOT EXISTS "${_OPENSSL_BUILD_DIR}/ossl_static.pdb")
    file(TOUCH "${_OPENSSL_BUILD_DIR}/ossl_static.pdb")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${_openssl_env_path}"
        ${_OPENSSL_INSTALL_TOOL} install_sw
    WORKING_DIRECTORY "${_OPENSSL_BUILD_DIR}"
    RESULT_VARIABLE _ssl_install_result
    OUTPUT_QUIET
)

if (NOT _ssl_install_result EQUAL 0)
    message(FATAL_ERROR "[OpenSSL] Install failed (exit ${_ssl_install_result}).")
endif()

message(STATUS "[OpenSSL] Successfully built static OpenSSL ${_OPENSSL_VERSION}")

# Write flag sentinel for future cache reuse validation.
if (_OPENSSL_TARGET STREQUAL "VC-WIN64A")
    file(WRITE "${_OPENSSL_INSTALL}/.citron-clangcl-extra-cflags" "${_OPENSSL_EXTRA_CFLAGS}")
endif()

_citron_publish_openssl_imports()
