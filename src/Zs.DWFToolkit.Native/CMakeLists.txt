cmake_minimum_required(VERSION 3.20)
project(zs_dwf_toolkit_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(zs_dwf_toolkit_native SHARED
    src/zs_dwf_toolkit_native.cpp
)

target_include_directories(zs_dwf_toolkit_native PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(zs_dwf_toolkit_native PRIVATE ZS_DWF_TOOLKIT_NATIVE_EXPORTS)

# Optional future integration:
# 1. Put Autodesk/ODA DWF Toolkit source or binary references under vendor/DWFToolkit-7.7.
# 2. Add include directories and link DwfToolkit/DwfCore here.
# target_include_directories(zs_dwf_toolkit_native PRIVATE vendor/DWFToolkit-7.7/develop/global/src)
# target_link_libraries(zs_dwf_toolkit_native PRIVATE DwfToolkit DwfCore)
