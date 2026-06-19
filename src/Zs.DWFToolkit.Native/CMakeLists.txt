cmake_minimum_required(VERSION 3.20)
project(zs_dwf_toolkit_native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ZS_DWF_ENABLE_ODA_DWFTK "Build and link the bundled ODA-modified DWFToolkit-7.7 source" OFF)
set(ZS_DWF_ODA_DWFTK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/DWFToolkit-7.7" CACHE PATH "Path to ODA-modified DWFToolkit-7.7 root")

add_library(zs_dwf_toolkit_native SHARED
    src/zs_dwf_toolkit_native.cpp
    src/render/MinimalPng.cpp
    src/render/RasterCanvas.cpp
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
