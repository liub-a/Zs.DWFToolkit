#pragma once
#include <string>

namespace zs::dwf::w2d
{

struct RenderResult
{
    bool success{false};
    int result_code{1007};
    std::string error_code{"unsupported_dwf_rendering"};
    std::string error_message;
    std::string json;
};

RenderResult render_w2d_file_to_png(
    const std::string& w2d_path,
    const std::string& output_path,
    int page_index,
    int width_px,
    int height_px,
    int dpi);

} // namespace zs::dwf::w2d
