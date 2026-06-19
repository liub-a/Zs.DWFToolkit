#pragma once
#include "../w2d/W2dWhipRenderer.h"
#include <string>

namespace zs::dwf::oda
{

zs::dwf::w2d::RenderResult render_dwf_or_w2d_page_to_png(
    const std::string& input_path,
    int page_index,
    const std::string& output_path,
    int width_px,
    int height_px,
    int dpi);

} // namespace zs::dwf::oda
