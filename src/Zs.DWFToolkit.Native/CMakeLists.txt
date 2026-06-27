cmake_minimum_required(VERSION 3.20)
project(zs_dwf_toolkit_native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ODA DWFToolkit assumes wchar_t is 16-bit and passes const wchar_t* to
# WT_String::set(const WT_Unsigned_Integer16*). MSVC's default native wchar_t is a
# distinct type, so that call fails overload resolution (C2665 in XAML/XamlText.cpp).
# /Zc:wchar_t- makes wchar_t an alias for unsigned short, matching the toolkit's
# assumption. Set before any target is defined so the ODA subdirectory inherits it.
# zlib/libtiff/freetype are C, so this only affects C++ translation units.
if(MSVC)
  add_compile_options(/Zc:wchar_t-)
  # ODA's win32 sources (e.g. publisher/win32/FontSubset.cpp) use the
  # GetSystemDirectory/LoadLibrary macros with wide (wchar_t) buffers and L"..."
  # literals. Without UNICODE defined the macros resolve to the ANSI *A variants,
  # which reject wchar_t* (C2664). Define UNICODE/_UNICODE so they bind to the wide
  # variants, matching the toolkit's standard Windows configuration.
  add_compile_definitions(UNICODE _UNICODE)
  # NOTE: NOMINMAX is intentionally NOT defined globally here. ODA's own win32
  # sources (e.g. XAML/win32/XamlFontUtilImpl.cpp) call the bare min()/max() macros
  # from <windows.h>, so suppressing them breaks the toolkit (C3861). We instead
  # define NOMINMAX per-target on our own translation units (which use std::max /
  # std::numeric_limits<T>::max()) via ZS_DWF_OUR_MSVC_DEFS below.
endif()

# WebAssembly (Emscripten) target. Set before any target is defined so the ODA
# subdirectory inherits these. The ODA toolkit ships an -DEMCC code path (portable
# <algorithm> instead of <bits/stl_algobase.h>, and major_version()/minor_version()
# ellipse accessors to dodge the <sys/sysmacros.h> major()/minor() macros), so we
# just enable it. Pair with scripts/setup-oda-dwftoolkit.sh (adds the wasm branch to
# dwfcore/Core.h) and build via scripts/build-wasm.sh.
if(EMSCRIPTEN)
  add_compile_definitions(EMCC)
  add_compile_options(-Wno-register -Wno-deprecated-declarations)
  # FreeType's fcntl.h/unistd.h cross-compile checks misfire under Emscripten; seed
  # them so its file backend (unused at runtime — we use in-memory faces) compiles.
  set(HAVE_FCNTL_H 1 CACHE INTERNAL "")
  set(HAVE_UNISTD_H 1 CACHE INTERNAL "")
  # A native render worker process makes no sense in the browser.
  set(ZS_DWF_BUILD_WORKER OFF CACHE BOOL "" FORCE)
endif()

# MSVC-only compile definitions for OUR translation units (not the ODA subdir):
# NOMINMAX so <windows.h> (pulled in by whiptk headers) does not shadow std::max /
# std::min / std::numeric_limits<T>::max() (C2589).
function(zs_dwf_apply_our_msvc_defs _tgt)
    if(MSVC)
        target_compile_definitions(${_tgt} PRIVATE NOMINMAX)
    endif()
endfunction()

option(ZS_DWF_ENABLE_ODA_DWFTK "Build and link the bundled ODA-modified DWFToolkit-7.7 source" OFF)
option(ZS_DWF_BUILD_TESTS "Build native RasterCanvas unit tests (CTest)" OFF)
option(ZS_DWF_WITH_FREETYPE "Render W2D text with FreeType glyphs instead of placeholder boxes" OFF)
set(ZS_DWF_ODA_DWFTK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/DWFToolkit-7.7" CACHE PATH "Path to ODA-modified DWFToolkit-7.7 root")

add_library(zs_dwf_toolkit_native SHARED
    src/zs_dwf_toolkit_native.cpp
    src/render/MinimalPng.cpp
    src/render/RasterCanvas.cpp
    src/pdf/MinimalPdf.cpp
    src/pdf/PdfVectorCanvas.cpp
    src/text/TextRenderer.cpp
    src/image/Group4Decoder.cpp
    src/edit/W2dStamp.cpp
    src/w2d/W2dWhipRenderer.cpp
    src/dwf/OdaDwfAdapter.cpp
)

target_include_directories(zs_dwf_toolkit_native PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(zs_dwf_toolkit_native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(zs_dwf_toolkit_native PRIVATE ZS_DWF_TOOLKIT_NATIVE_EXPORTS)
zs_dwf_apply_our_msvc_defs(zs_dwf_toolkit_native)

# ODA DWFCore uses pthreads. On glibc < 2.34 (e.g. the manylinux2014/glibc-2.17
# build container) pthread is a separate library and must be linked explicitly;
# newer glibc folds it into libc. Threads::Threads does the right thing everywhere.
# Not REQUIRED: the Emscripten/wasm target has no pthread by default (single-thread),
# so configuring proceeds without it.
find_package(Threads)
if(TARGET Threads::Threads)
    target_link_libraries(zs_dwf_toolkit_native PRIVATE Threads::Threads)
endif()

# Out-of-process render worker: the managed library invokes this with a timeout so
# a native crash/hang is isolated from the host. Built alongside the shared library.
option(ZS_DWF_BUILD_WORKER "Build the zs_dwf_worker out-of-process render executable" ON)
if(ZS_DWF_BUILD_WORKER)
    add_executable(zs_dwf_worker tools/zs_dwf_worker.cpp)
    target_link_libraries(zs_dwf_worker PRIVATE zs_dwf_toolkit_native)
    zs_dwf_apply_our_msvc_defs(zs_dwf_worker)
endif()

if(ZS_DWF_WITH_FREETYPE)
    # Self-contained text rendering: FreeType source is vendored in-tree
    # (third_party/freetype, v2.13.3) and built with no external deps, so the build
    # needs no network and no system FreeType, and a font is embedded below.
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/freetype" "${CMAKE_BINARY_DIR}/freetype" EXCLUDE_FROM_ALL)

    # Compile the bundled fonts into the library as byte arrays (build-time
    # generated): DejaVu Sans for Latin, Droid Sans Fallback for CJK and other
    # scripts DejaVu does not cover.
    set(ZS_DWF_FONT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_font.h")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/fonts/DejaVuSans.ttf" ZS_DWF_FONT_HEX HEX)
    string(LENGTH "${ZS_DWF_FONT_HEX}" ZS_DWF_FONT_HEXLEN)
    math(EXPR ZS_DWF_FONT_LEN "${ZS_DWF_FONT_HEXLEN} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," ZS_DWF_FONT_BYTES "${ZS_DWF_FONT_HEX}")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/fonts/DroidSansFallback.ttf" ZS_DWF_FB_HEX HEX)
    string(LENGTH "${ZS_DWF_FB_HEX}" ZS_DWF_FB_HEXLEN)
    math(EXPR ZS_DWF_FB_LEN "${ZS_DWF_FB_HEXLEN} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," ZS_DWF_FB_BYTES "${ZS_DWF_FB_HEX}")
    file(WRITE "${ZS_DWF_FONT_HEADER}"
        "// Generated at configure time from third_party/fonts/*.ttf. Do not edit.\n"
        "#pragma once\n"
        "namespace zs { namespace dwf { namespace text {\n"
        "static const unsigned char kEmbeddedFont[] = {${ZS_DWF_FONT_BYTES}};\n"
        "static const unsigned long kEmbeddedFontSize = ${ZS_DWF_FONT_LEN}UL;\n"
        "static const unsigned char kEmbeddedFallbackFont[] = {${ZS_DWF_FB_BYTES}};\n"
        "static const unsigned long kEmbeddedFallbackFontSize = ${ZS_DWF_FB_LEN}UL;\n"
        "}}}\n")

    target_compile_definitions(zs_dwf_toolkit_native PRIVATE ZS_DWF_WITH_FREETYPE=1)
    target_include_directories(zs_dwf_toolkit_native PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_link_libraries(zs_dwf_toolkit_native PRIVATE freetype)
endif()

option(ZS_DWF_WITH_TIFF "Decode Group4X fax W2D images via a vendored libtiff" OFF)
if(ZS_DWF_WITH_TIFF)
    # libtiff source is vendored in-tree (third_party/libtiff, v4.6.0), built static
    # with codecs/tools/tests off. The CCITT fax codec is internal, so Group4 decodes
    # with no network and no system libtiff.
    set(tiff-tools OFF CACHE BOOL "" FORCE)
    set(tiff-tests OFF CACHE BOOL "" FORCE)
    set(tiff-docs OFF CACHE BOOL "" FORCE)
    set(tiff-contrib OFF CACHE BOOL "" FORCE)
    set(jpeg OFF CACHE BOOL "" FORCE)
    set(old-jpeg OFF CACHE BOOL "" FORCE)
    set(jbig OFF CACHE BOOL "" FORCE)
    set(lzma OFF CACHE BOOL "" FORCE)
    set(zstd OFF CACHE BOOL "" FORCE)
    set(webp OFF CACHE BOOL "" FORCE)
    set(zlib OFF CACHE BOOL "" FORCE)
    set(libdeflate OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/libtiff" "${CMAKE_BINARY_DIR}/libtiff" EXCLUDE_FROM_ALL)
    target_compile_definitions(zs_dwf_toolkit_native PRIVATE ZS_DWF_WITH_TIFF=1)
    target_link_libraries(zs_dwf_toolkit_native PRIVATE tiff)
endif()

if(ZS_DWF_ENABLE_ODA_DWFTK)
    if(NOT EXISTS "${ZS_DWF_ODA_DWFTK_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR "DWFToolkit-7.7 source not found: ${ZS_DWF_ODA_DWFTK_ROOT}")
    endif()

    # The ODA-modified DWF Toolkit CMake files were originally designed to be
    # built inside an ODA SDK tree. The variables below provide the minimum
    # paths needed for a standalone attempt from this repository.
    set(TH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party" CACHE PATH "Third-party root used by ODA CMake files" FORCE)
    set(TKERNELBASE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/compat/oda" CACHE PATH "Minimal KernelBase compatibility headers" FORCE)
    set(ODA_LIB_DIR "${CMAKE_BINARY_DIR}/oda/lib" CACHE PATH "ODA library output directory" FORCE)
    set(ODA_BIN_DIR "${CMAKE_BINARY_DIR}/oda/bin" CACHE PATH "ODA binary output directory" FORCE)
    set(TD_ALLOC_LIB "" CACHE STRING "Optional ODA allocator lib" FORCE)
    set(ODA_WCHAR_LIB "" CACHE STRING "Optional ODA wchar lib" FORCE)
    set(TH_ZLIB_LIB "" CACHE STRING "Optional external zlib lib" FORCE)
    set(TH_THIRDPARTYRASTER_LIB "" CACHE STRING "Optional ODA raster lib" FORCE)

    file(MAKE_DIRECTORY "${ODA_LIB_DIR}" "${ODA_BIN_DIR}")

    target_compile_definitions(zs_dwf_toolkit_native PRIVATE ZS_DWF_WITH_ODA_DWFTK=1)
    # zlib is vendored and linked in this branch, so MinimalPdf can FlateDecode-
    # compress PDF content streams (vector linework compresses ~10x).
    target_compile_definitions(zs_dwf_toolkit_native PRIVATE ZS_DWF_WITH_ZLIB=1)
    target_include_directories(zs_dwf_toolkit_native PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/compat/oda
        ${CMAKE_CURRENT_SOURCE_DIR}/compat/oda/Include
        ${ZS_DWF_ODA_DWFTK_ROOT}/develop/global/src
        ${ZS_DWF_ODA_DWFTK_ROOT}/develop/global/src/dwf
        ${ZS_DWF_ODA_DWFTK_ROOT}
    )

    # dwf/Toolkit.cpp defines _DWFToolkitMemory::operator new/delete but the ODA
    # CMake does not compile it into DwfToolkit, leaving the symbol undefined in our
    # shared library. macOS tolerates undefined symbols in a dylib; GNU ld does not,
    # so any executable linking the .so fails. Compile it directly into our library.
    target_sources(zs_dwf_toolkit_native PRIVATE
        "${ZS_DWF_ODA_DWFTK_ROOT}/develop/global/src/dwf/Toolkit.cpp")

    # W3dTk pulls in zlib and libjpeg symbols and DwfToolkit references W3dTk, so
    # both must be built. zlib is always vendored in-tree (third_party/zlib, static).
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/zlib" "${CMAKE_BINARY_DIR}/zlib" EXCLUDE_FROM_ALL)

    # libjpeg: prefer a system libjpeg (typically the faster SIMD jpeg-turbo on
    # macOS/Linux) when present; otherwise fall back to building the libjpeg-6b
    # sources the ODA toolkit bundles under w3dtk/jpeg. W3dTk and our decoder both
    # compile against whichever jpeglib.h is selected, so the ABI stays consistent.
    option(ZS_DWF_PREFER_SYSTEM_JPEG "Use system libjpeg/jpeg-turbo if available" ON)
    set(ZS_JPEG_LIB "")
    if(ZS_DWF_PREFER_SYSTEM_JPEG)
        if(APPLE)
            list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/jpeg-turbo /usr/local/opt/jpeg-turbo)
        endif()
        find_package(JPEG QUIET)
    endif()
    if(JPEG_FOUND)
        message(STATUS "zs_dwf: using system libjpeg: ${JPEG_LIBRARIES}")
        include_directories(${JPEG_INCLUDE_DIRS})
        set(ZS_JPEG_LIB JPEG::JPEG)
    else()
        message(STATUS "zs_dwf: building bundled libjpeg-6b (no system libjpeg found)")
        set(ODA_JPEG_DIR "${ZS_DWF_ODA_DWFTK_ROOT}/develop/global/src/dwf/w3dtk/jpeg")
        file(GLOB ODA_JPEG_SRC "${ODA_JPEG_DIR}/*.c")
        # Keep one memory backend (jmemnobs); drop the platform-specific alternatives.
        list(FILTER ODA_JPEG_SRC EXCLUDE REGEX "jmem(ansi|dos|mac|name)\\.c$")
        add_library(oda_jpeg STATIC ${ODA_JPEG_SRC})
        target_include_directories(oda_jpeg PUBLIC "${ODA_JPEG_DIR}")
        set_target_properties(oda_jpeg PROPERTIES POSITION_INDEPENDENT_CODE ON)
        include_directories("${ODA_JPEG_DIR}")
        set(ZS_JPEG_LIB oda_jpeg)
    endif()

    # zlib.h on the global path for W3dTk's own TUs.
    include_directories(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/zlib"
        "${CMAKE_BINARY_DIR}/zlib"
    )

    # Add the Toolkit after the main shared library is declared so failures are
    # isolated behind the ZS_DWF_ENABLE_ODA_DWFTK option. If your platform still
    # needs additional ODA KernelBase headers, keep this option OFF and link
    # against prebuilt DwfToolkit/DwfCore binaries instead.
    add_subdirectory("${ZS_DWF_ODA_DWFTK_ROOT}" "${CMAKE_BINARY_DIR}/DWFToolkit-7.7")

    # The ODA static libs (DwfToolkit/DwfCore/WhipTk/W3dTk) reference each other
    # circularly. Apple's linker resolves that automatically, but GNU ld (Linux)
    # processes static libs in order and drops not-yet-referenced objects, leaving
    # undefined symbols (e.g. DWFCore::AtomicIncrement, _DWFToolkitMemory::operator
    # delete) in the shared library. Wrap them in a --start-group so ld rescans.
    set(_oda_libs "")
    foreach(_t DwfToolkit DwfCore WhipTk W3dTk)
        if(TARGET ${_t})
            list(APPEND _oda_libs ${_t})
        endif()
    endforeach()

    if(APPLE OR MSVC)
        target_link_libraries(zs_dwf_toolkit_native PRIVATE ${_oda_libs} ${ZS_JPEG_LIB} zlibstatic)
    else()
        target_link_libraries(zs_dwf_toolkit_native PRIVATE
            -Wl,--start-group ${_oda_libs} ${ZS_JPEG_LIB} zlibstatic -Wl,--end-group)
    endif()

    if(MSVC)
        # DwfCore's DWFUUIDImpl_Win32::uuid calls UuidCreate, which lives in
        # Rpcrt4.lib (RPC runtime); not linked by default (LNK2019 __imp_UuidCreate).
        target_link_libraries(zs_dwf_toolkit_native PRIVATE rpcrt4)
    endif()
endif()

if(ZS_DWF_BUILD_TESTS)
    enable_testing()
    add_executable(raster_canvas_tests
        tests/raster_canvas_tests.cpp
        src/render/RasterCanvas.cpp
        src/render/MinimalPng.cpp
        src/text/TextRenderer.cpp
    )
    target_include_directories(raster_canvas_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    # RasterCanvas::draw_text delegates to TextRenderer, so the standalone test must
    # link it (and FreeType when enabled); otherwise TextRenderer compiles to no-ops.
    if(ZS_DWF_WITH_FREETYPE)
        target_compile_definitions(raster_canvas_tests PRIVATE ZS_DWF_WITH_FREETYPE=1)
        target_include_directories(raster_canvas_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_link_libraries(raster_canvas_tests PRIVATE freetype)
    endif()
    add_test(NAME raster_canvas_tests COMMAND raster_canvas_tests)

    # End-to-end W2D round-trip needs the WHIP Toolkit (to write a W2D) and our
    # shared library (to render it), so it is only built when ODA is enabled.
    if(ZS_DWF_ENABLE_ODA_DWFTK)
        add_executable(w2d_roundtrip_tests tests/w2d_roundtrip_tests.cpp)
        target_include_directories(w2d_roundtrip_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/compat/oda
            ${CMAKE_CURRENT_SOURCE_DIR}/compat/oda/Include
            ${ZS_DWF_ODA_DWFTK_ROOT}/develop/global/src
            ${ZS_DWF_ODA_DWFTK_ROOT}/develop/global/src/dwf
        )
        # This test writes a W2D itself, so it links the ODA static libs directly.
        # Include DwfToolkit (_DWFToolkitMemory) and wrap the circular set in a
        # --start-group on GNU ld (same reason as the shared library above).
        target_link_libraries(w2d_roundtrip_tests PRIVATE zs_dwf_toolkit_native)
        zs_dwf_apply_our_msvc_defs(w2d_roundtrip_tests)
        set(_oda_test_libs "")
        foreach(_t DwfToolkit DwfCore WhipTk W3dTk)
            if(TARGET ${_t})
                list(APPEND _oda_test_libs ${_t})
            endif()
        endforeach()
        if(APPLE OR MSVC)
            target_link_libraries(w2d_roundtrip_tests PRIVATE ${_oda_test_libs} ${ZS_JPEG_LIB} zlibstatic)
        else()
            target_link_libraries(w2d_roundtrip_tests PRIVATE
                -Wl,--start-group ${_oda_test_libs} ${ZS_JPEG_LIB} zlibstatic -Wl,--end-group)
        endif()
        if(MSVC)
            target_link_libraries(w2d_roundtrip_tests PRIVATE rpcrt4)
        endif()
        if(TARGET Threads::Threads)
            target_link_libraries(w2d_roundtrip_tests PRIVATE Threads::Threads)
        endif()
        add_test(NAME w2d_roundtrip_tests COMMAND w2d_roundtrip_tests)

        # Layer reading: writes a layered W2D with the toolkit, reads it back.
        add_executable(layer_tests tests/layer_tests.cpp)
        target_include_directories(layer_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/compat/oda
            ${CMAKE_CURRENT_SOURCE_DIR}/compat/oda/Include
            ${ZS_DWF_ODA_DWFTK_ROOT}/develop/global/src
            ${ZS_DWF_ODA_DWFTK_ROOT}/develop/global/src/dwf
        )
        target_link_libraries(layer_tests PRIVATE zs_dwf_toolkit_native)
        zs_dwf_apply_our_msvc_defs(layer_tests)
        if(APPLE OR MSVC)
            target_link_libraries(layer_tests PRIVATE ${_oda_test_libs} ${ZS_JPEG_LIB} zlibstatic)
        else()
            target_link_libraries(layer_tests PRIVATE
                -Wl,--start-group ${_oda_test_libs} ${ZS_JPEG_LIB} zlibstatic -Wl,--end-group)
        endif()
        if(MSVC)
            target_link_libraries(layer_tests PRIVATE rpcrt4)
        endif()
        if(TARGET Threads::Threads)
            target_link_libraries(layer_tests PRIVATE Threads::Threads)
        endif()
        add_test(NAME layer_tests COMMAND layer_tests)

        # Regression against the public ocean_90.w2d sample (path via ZS_DWF_OCEAN_W2D;
        # no-op pass when unset). Baselines are defined here once captured.
        add_executable(ocean_regression_tests tests/ocean_regression_tests.cpp)
        target_include_directories(ocean_regression_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_link_libraries(ocean_regression_tests PRIVATE zs_dwf_toolkit_native)
        target_compile_definitions(ocean_regression_tests PRIVATE ZS_DWF_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../tests/data")
        # Baselines captured from ocean_90.w2d at 1000x800. Counts are exact
        # (deterministic parse); non-white pixels assert within a +/-15% band.
        set(ZS_DWF_OCEAN_POLYLINE 6977 CACHE STRING "ocean baseline polyline count")
        set(ZS_DWF_OCEAN_POLYGON 0 CACHE STRING "ocean baseline polygon count")
        set(ZS_DWF_OCEAN_NONWHITE 145084 CACHE STRING "ocean baseline non-white pixels")
        if(DEFINED ZS_DWF_OCEAN_NONWHITE)
            target_compile_definitions(ocean_regression_tests PRIVATE
                ZS_DWF_OCEAN_ASSERT=1
                ZS_DWF_OCEAN_POLYLINE=${ZS_DWF_OCEAN_POLYLINE}
                ZS_DWF_OCEAN_POLYGON=${ZS_DWF_OCEAN_POLYGON}
                ZS_DWF_OCEAN_NONWHITE=${ZS_DWF_OCEAN_NONWHITE})
        endif()
        add_test(NAME ocean_regression_tests COMMAND ocean_regression_tests)

        add_executable(dwf_package_tests tests/dwf_package_tests.cpp)
        target_include_directories(dwf_package_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_link_libraries(dwf_package_tests PRIVATE zs_dwf_toolkit_native)
        target_compile_definitions(dwf_package_tests PRIVATE ZS_DWF_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../tests/data")
        add_test(NAME dwf_package_tests COMMAND dwf_package_tests)

        # Vector-PDF regression: DWF -> true-vector PDF via the C ABI.
        add_executable(pdf_vector_tests tests/pdf_vector_tests.cpp)
        target_include_directories(pdf_vector_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_link_libraries(pdf_vector_tests PRIVATE zs_dwf_toolkit_native)
        target_compile_definitions(pdf_vector_tests PRIVATE ZS_DWF_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../tests/data")
        add_test(NAME pdf_vector_tests COMMAND pdf_vector_tests)

        if(ZS_DWF_WITH_TIFF)
            add_executable(group4_tests tests/group4_tests.cpp)
            target_include_directories(group4_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
            target_link_libraries(group4_tests PRIVATE zs_dwf_toolkit_native tiff)
            add_test(NAME group4_tests COMMAND group4_tests)
        endif()
    endif()
endif()
