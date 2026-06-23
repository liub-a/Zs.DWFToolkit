#pragma once
#include <string>

namespace zs::dwf::pdf { struct PdfPage; }

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

// Renders a single W2D stream to one true-vector PDF page (filled into out_page:
// content stream + media box in points + any embedded raster sub-images). The page
// size is derived from the drawing bounds, preserving aspect ratio. Returns
// success=false (with error_code/message) when ODA is unavailable or parsing
// fails; the vocabulary mirrors render_w2d_file_to_png.
RenderResult render_w2d_file_to_pdf_page(
    const std::string& w2d_path,
    zs::dwf::pdf::PdfPage& out_page);

} // namespace zs::dwf::w2d
