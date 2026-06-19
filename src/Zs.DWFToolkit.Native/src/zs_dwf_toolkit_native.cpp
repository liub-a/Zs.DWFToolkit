#include "zs_dwf_toolkit_native.h"
#include <cstring>
#include <string>
#include <fstream>
#include "dwf/OdaDwfAdapter.h"
#include "w2d/W2dWhipRenderer.h"

static thread_local std::string g_last_error;

static std::string json_escape(const char* value)
{
    if (!value) return {};
    std::string out;
    for (const char* p = value; *p; ++p)
    {
        switch (*p)
        {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += *p; break;
        }
    }
    return out;
}

static bool copy_json(const std::string& json, char* output_json, int output_json_size)
{
    if (!output_json || output_json_size <= 0)
    {
        g_last_error = "output_json buffer is null or too small";
        return false;
    }
    const auto n = json.size();
    if (static_cast<int>(n + 1) > output_json_size)
    {
        g_last_error = "output_json buffer is too small";
        return false;
    }
    std::memset(output_json, 0, output_json_size);
    std::memcpy(output_json, json.c_str(), n);
    return true;
}

extern "C" ZS_DWF_API int zs_dwf_get_info_json(
    const char* input_path,
    char* output_json,
    int output_json_size)
{
    if (!input_path || std::strlen(input_path) == 0)
    {
        g_last_error = "input_path is required";
        return ZS_DWF_INVALID_ARGUMENT;
    }

    std::ifstream f(input_path, std::ios::binary);
    if (!f.good())
    {
        g_last_error = "file not found or not readable";
        return ZS_DWF_FILE_NOT_FOUND;
    }

    // This stub proves the ABI and loading path.
    // Replace this block with DWF Toolkit parsing when DWFToolkit-7.7 is added.
    const std::string json = std::string("{") +
        "\"success\":false," +
        "\"errorCode\":\"unsupported_dwf_rendering\"," +
        "\"errorMessage\":\"Native stub is loaded, but DWF Toolkit parsing is not implemented yet. Add DWFToolkit-7.7 and implement DwfNativeAdapter.\"," +
        "\"sourcePath\":\"" + json_escape(input_path) + "\"," +
        "\"kind\":1," +
        "\"isZipBased\":false," +
        "\"pages\":[]," +
        "\"entries\":[]," +
        "\"properties\":{}" +
        "}";

    if (!copy_json(json, output_json, output_json_size))
        return ZS_DWF_OUTPUT_FAILED;

    g_last_error.clear();
    return ZS_DWF_OK;
}

extern "C" ZS_DWF_API int zs_dwf_render_page(
    const char* input_path,
    int page_index,
    const char* output_path,
    int width_px,
    int height_px,
    int dpi,
    char* output_json,
    int output_json_size)
{
    if (!input_path || std::strlen(input_path) == 0)
    {
        g_last_error = "input_path is required";
        return ZS_DWF_INVALID_ARGUMENT;
    }
    if (!output_path || std::strlen(output_path) == 0)
    {
        g_last_error = "output_path is required";
        return ZS_DWF_INVALID_ARGUMENT;
    }

    auto result = zs::dwf::oda::render_dwf_or_w2d_page_to_png(
        input_path,
        page_index,
        output_path,
        width_px,
        height_px,
        dpi);

    const std::string json = result.json.empty()
        ? (std::string("{") +
            "\"success\":false," +
            "\"errorCode\":\"" + json_escape(result.error_code.c_str()) + "\"," +
            "\"errorMessage\":\"" + json_escape(result.error_message.c_str()) + "\"," +
            "\"sourcePath\":\"" + json_escape(input_path) + "\"," +
            "\"pageIndex\":" + std::to_string(page_index) + "," +
            "\"outputPath\":\"" + json_escape(output_path) + "\"," +
            "\"outputFiles\":[]," +
            "\"toolName\":\"oda_whiptk_minimal_renderer\"" +
            "}")
        : result.json;

    if (!copy_json(json, output_json, output_json_size))
        return ZS_DWF_OUTPUT_FAILED;

    g_last_error = result.error_message;
    return result.success ? ZS_DWF_OK : result.result_code;
}

extern "C" ZS_DWF_API int zs_w2d_render_file(
    const char* input_path,
    const char* output_path,
    int width_px,
    int height_px,
    int dpi,
    char* output_json,
    int output_json_size)
{
    if (!input_path || std::strlen(input_path) == 0)
    {
        g_last_error = "input_path is required";
        return ZS_DWF_INVALID_ARGUMENT;
    }
    if (!output_path || std::strlen(output_path) == 0)
    {
        g_last_error = "output_path is required";
        return ZS_DWF_INVALID_ARGUMENT;
    }

    auto result = zs::dwf::w2d::render_w2d_file_to_png(input_path, output_path, 0, width_px, height_px, dpi);
    const std::string json = result.json.empty()
        ? (std::string("{") +
            "\"success\":false," +
            "\"errorCode\":\"" + json_escape(result.error_code.c_str()) + "\"," +
            "\"errorMessage\":\"" + json_escape(result.error_message.c_str()) + "\"," +
            "\"sourcePath\":\"" + json_escape(input_path) + "\"," +
            "\"outputPath\":\"" + json_escape(output_path) + "\"," +
            "\"outputFiles\":[]," +
            "\"toolName\":\"oda_whiptk_minimal_renderer\"" +
            "}")
        : result.json;

    if (!copy_json(json, output_json, output_json_size))
        return ZS_DWF_OUTPUT_FAILED;

    g_last_error = result.error_message;
    return result.success ? ZS_DWF_OK : result.result_code;
}

extern "C" ZS_DWF_API const char* zs_dwf_get_last_error()
{
    return g_last_error.c_str();
}
