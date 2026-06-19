#pragma once

#ifdef _WIN32
  #ifdef ZS_DWF_TOOLKIT_NATIVE_EXPORTS
    #define ZS_DWF_API __declspec(dllexport)
  #else
    #define ZS_DWF_API __declspec(dllimport)
  #endif
#else
  #define ZS_DWF_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Return codes mirrored by the managed wrapper.
enum ZsDwfResultCode
{
    ZS_DWF_OK = 0,
    ZS_DWF_INVALID_ARGUMENT = 1001,
    ZS_DWF_FILE_NOT_FOUND = 1002,
    ZS_DWF_OPEN_FAILED = 1003,
    ZS_DWF_UNSUPPORTED_FORMAT = 1004,
    ZS_DWF_UNSUPPORTED_3D = 1005,
    ZS_DWF_PAGE_NOT_FOUND = 1006,
    ZS_DWF_RENDER_FAILED = 1007,
    ZS_DWF_OUTPUT_FAILED = 1008,
    ZS_DWF_TIMEOUT = 1009,
    ZS_DWF_OUT_OF_MEMORY = 1010,
    ZS_DWF_INTERNAL_ERROR = 1999
};

ZS_DWF_API int zs_dwf_get_info_json(
    const char* input_path,
    char* output_json,
    int output_json_size);

ZS_DWF_API int zs_dwf_render_page(
    const char* input_path,
    int page_index,
    const char* output_path,
    int width_px,
    int height_px,
    int dpi,
    char* output_json,
    int output_json_size);

ZS_DWF_API const char* zs_dwf_get_last_error();

#ifdef __cplusplus
}
#endif
