cmake_minimum_required(VERSION 3.20)
project(zs_dwf_toolkit_native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ZS_DWF_ENABLE_ODA_DWFTK "Build and link the bundled ODA-modified DWFToolkit-7.7 source" OFF)
option(ZS_DWF_BUILD_TESTS "Build native RasterCanvas unit tests (CTest)" OFF)
option(ZS_DWF_WITH_FREETYPE "Render W2D text with FreeType glyphs instead of placeholder boxes" OFF)
set(ZS_DWF_ODA_DWFTK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/DWFToolkit-7.7" CACHE PATH "Path to ODA-modified DWFToolkit-7.7 root")

add_library(zs_dwf_toolkit_native SHARED
    src/zs_dwf_toolkit_native.cpp
    src/render/MinimalPng.cpp
    src/render/RasterCanvas.cpp
    src/text/TextRenderer.cpp
    src/image/Group4Decoder.cpp
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

    # Compile the bundled font into the library as a byte array (build-time generated).
    set(ZS_DWF_FONT_TTF "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/fonts/DejaVuSans.ttf")
    set(ZS_DWF_FONT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_font.h")
    file(READ "${ZS_DWF_FONT_TTF}" ZS_DWF_FONT_HEX HEX)
    string(LENGTH "${ZS_DWF_FONT_HEX}" ZS_DWF_FONT_HEXLEN)
    math(EXPR ZS_DWF_FONT_LEN "${ZS_DWF_FONT_HEXLEN} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," ZS_DWF_FONT_BYTES "${ZS_DWF_FONT_HEX}")
    file(WRITE "${ZS_DWF_FONT_HEADER}"
        "// Generated at configure time from third_party/fonts/DejaVuSans.ttf. Do not edit.\n"
        "#pragma once\n"
        "namespace zs { namespace dwf { namespace text {\n"
        "static const unsigned char kEmbeddedFont[] = {${ZS_DWF_FONT_BYTES}};\n"
        "static const unsigned long kEmbeddedFontSize = ${ZS_DWF_FONT_LEN}UL;\n"
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
    target_include_directories(zs_dwf_toolkit_native PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/compat/oda
        ${CMAKE_CURRENT_SOURCE_DIR}/compat/oda/Include
        ${ZS_DWF_ODA_DWFTK_ROOT}/develop/global/src
        ${ZS_DWF_ODA_DWFTK_ROOT}/develop/global/src/dwf
        ${ZS_DWF_ODA_DWFTK_ROOT}
    )

    # W3dTk's BOpcodeHandler.cpp includes <jpeglib.h>, but the bundled ODA CMake
    # never adds the directory that holds the vendored libjpeg headers. Inject it
    # here (directory-scoped, so it propagates into the add_subdirectory tree below)
    # so W3dTk compiles. DwfToolkit references W3dTk symbols, so it must be built.
    include_directories("${ZS_DWF_ODA_DWFTK_ROOT}/develop/global/src/dwf/w3dtk/jpeg")

    # Add the Toolkit after the main shared library is declared so failures are
    # isolated behind the ZS_DWF_ENABLE_ODA_DWFTK option. If your platform still
    # needs additional ODA KernelBase headers, keep this option OFF and link
    # against prebuilt DwfToolkit/DwfCore binaries instead.
    add_subdirectory("${ZS_DWF_ODA_DWFTK_ROOT}" "${CMAKE_BINARY_DIR}/DWFToolkit-7.7")

    if(TARGET DwfToolkit)
        target_link_libraries(zs_dwf_toolkit_native PRIVATE DwfToolkit)
    endif()
    if(TARGET DwfCore)
        target_link_libraries(zs_dwf_toolkit_native PRIVATE DwfCore)
    endif()
    if(TARGET WhipTk)
        target_link_libraries(zs_dwf_toolkit_native PRIVATE WhipTk)
    endif()
    # W3dTk (3D/eModel toolkit) is required because DwfToolkit references its
    # opcode-handler symbols, even though this library only renders 2D W2D content.
    if(TARGET W3dTk)
        target_link_libraries(zs_dwf_toolkit_native PRIVATE W3dTk)
    endif()

    # W3dTk pulls in libjpeg and zlib symbols. The toolkit ships jpeg headers but
    # its bundled CMake builds neither library, so link the system ones (Homebrew
    # jpeg-turbo on macOS; libjpeg-dev / zlib1g-dev on Linux CI).
    if(APPLE)
        list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/jpeg-turbo /usr/local/opt/jpeg-turbo)
    endif()
    find_package(ZLIB REQUIRED)
    find_package(JPEG REQUIRED)
    target_link_libraries(zs_dwf_toolkit_native PRIVATE ZLIB::ZLIB JPEG::JPEG)
endif()

if(ZS_DWF_BUILD_TESTS)
    enable_testing()
    add_executable(raster_canvas_tests
        tests/raster_canvas_tests.cpp
        src/render/RasterCanvas.cpp
        src/render/MinimalPng.cpp
    )
    target_include_directories(raster_canvas_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
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
        target_link_libraries(w2d_roundtrip_tests PRIVATE zs_dwf_toolkit_native)
        if(TARGET WhipTk)
            target_link_libraries(w2d_roundtrip_tests PRIVATE WhipTk)
        endif()
        if(TARGET DwfCore)
            target_link_libraries(w2d_roundtrip_tests PRIVATE DwfCore)
        endif()
        if(TARGET W3dTk)
            target_link_libraries(w2d_roundtrip_tests PRIVATE W3dTk)
        endif()
        target_link_libraries(w2d_roundtrip_tests PRIVATE ZLIB::ZLIB JPEG::JPEG)
        add_test(NAME w2d_roundtrip_tests COMMAND w2d_roundtrip_tests)

        # Regression against the public ocean_90.w2d sample (path via ZS_DWF_OCEAN_W2D;
        # no-op pass when unset). Baselines are defined here once captured.
        add_executable(ocean_regression_tests tests/ocean_regression_tests.cpp)
        target_include_directories(ocean_regression_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_link_libraries(ocean_regression_tests PRIVATE zs_dwf_toolkit_native)
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

        if(ZS_DWF_WITH_TIFF)
            add_executable(group4_tests tests/group4_tests.cpp)
            target_include_directories(group4_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
            target_link_libraries(group4_tests PRIVATE zs_dwf_toolkit_native tiff)
            add_test(NAME group4_tests COMMAND group4_tests)
        endif()
    endif()
endif()
