#include "zs_dwf_toolkit_native.h"
#include <cstring>
#include <string>
#include <fstream>
#include <vector>
#include "dwf/OdaDwfAdapter.h"
#include "w2d/W2dWhipRenderer.h"
#include "edit/W2dStamp.h"

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
    f.close();

    // When built with ZS_DWF_ENABLE_ODA_DWFTK the adapter parses the DWF package
    // and returns real section/page info. Otherwise it returns an
    // unsupported_dwf_rendering JSON so the managed layer can fall back to its own
    // ZIP/OPC reader.
    auto info = zs::dwf::oda::get_dwf_info_json(input_path);

    if (!copy_json(info.json, output_json, output_json_size))
        return ZS_DWF_OUTPUT_FAILED;

    g_last_error = info.success ? std::string() : std::string("native get_info returned a non-success result");
    return info.success ? ZS_DWF_OK : info.result_code;
}

static int render_page_impl(
    const char* input_path,
    int page_index,
    const char* output_path,
    int width_px,
    int height_px,
    int dpi,
    const std::vector<int>* hidden_layers,
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
        dpi,
        hidden_layers);

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
    return render_page_impl(input_path, page_index, output_path, width_px, height_px, dpi,
                            nullptr, output_json, output_json_size);
}

// Same as zs_dwf_render_page but hides the layers whose numbers are in
// hidden_layers[0..hidden_count). Pass null/0 to render all layers.
extern "C" ZS_DWF_API int zs_dwf_render_page_ex(
    const char* input_path,
    int page_index,
    const char* output_path,
    int width_px,
    int height_px,
    int dpi,
    const int* hidden_layers,
    int hidden_count,
    char* output_json,
    int output_json_size)
{
    std::vector<int> hidden;
    if (hidden_layers && hidden_count > 0)
        hidden.assign(hidden_layers, hidden_layers + hidden_count);
    return render_page_impl(input_path, page_index, output_path, width_px, height_px, dpi,
                            hidden.empty() ? nullptr : &hidden, output_json, output_json_size);
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

extern "C" ZS_DWF_API int zs_dwf_render_dwf_pdf(
    const char* input_path,
    const char* output_pdf_path)
{
    if (!input_path || std::strlen(input_path) == 0 ||
        !output_pdf_path || std::strlen(output_pdf_path) == 0)
    {
        g_last_error = "input_path and output_pdf_path are required";
        return ZS_DWF_INVALID_ARGUMENT;
    }
    auto result = zs::dwf::oda::render_dwf_to_pdf(input_path, output_pdf_path);
    g_last_error = result.success ? std::string() : (result.error_code + ": " + result.error_message);
    return result.success ? ZS_DWF_OK : result.result_code;
}

extern "C" ZS_DWF_API int zs_w2d_stamp_image(
    const char* input_w2d_path,
    const char* output_w2d_path,
    const unsigned char* rgba,
    int rgba_len,
    int img_w,
    int img_h,
    int min_x,
    int min_y,
    int max_x,
    int max_y)
{
    if (!input_w2d_path || !output_w2d_path || !rgba || rgba_len <= 0 || img_w <= 0 || img_h <= 0)
    {
        g_last_error = "invalid argument";
        return ZS_DWF_INVALID_ARGUMENT;
    }
    std::string error;
    const bool ok = zs::dwf::edit::stamp_image_on_w2d(
        input_w2d_path, output_w2d_path, rgba, static_cast<std::size_t>(rgba_len),
        img_w, img_h, min_x, min_y, max_x, max_y, error);
    if (!ok)
    {
        g_last_error = error;
        return ZS_DWF_RENDER_FAILED;
    }
    g_last_error.clear();
    return ZS_DWF_OK;
}

extern "C" ZS_DWF_API const char* zs_dwf_get_last_error()
{
    return g_last_error.c_str();
}
