#pragma once
#include <string>
#include <vector>

namespace zs::dwf::pdf { struct PdfPage; }

namespace zs::dwf::w2d
{

// A 2D layer (WHIP WT_Layer) collected from a W2D stream. Visibility is not stored
// in the W2D format (the toolkit defaults it to true), so `visible` is always true
// in practice; the field is kept for the JSON contract and future use.
struct W2dLayer
{
    int number{0};
    std::string name;
    bool visible{true};
};

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
    int dpi,
    const std::vector<int>* hidden_layers = nullptr);

// Renders a single W2D stream to one true-vector PDF page (filled into out_page:
// content stream + media box in points + any embedded raster sub-images). The page
// size is derived from the drawing bounds, preserving aspect ratio. Returns
// success=false (with error_code/message) when ODA is unavailable or parsing
// fails; the vocabulary mirrors render_w2d_file_to_png.
RenderResult render_w2d_file_to_pdf_page(
    const std::string& w2d_path,
    zs::dwf::pdf::PdfPage& out_page);

// Parses a W2D stream and collects its WHIP layers (WT_Layer) in order of
// appearance, de-duplicated by layer number, into out_layers. No rendering is
// performed (parse-only), so this is cheap. Returns success=false (with an
// error_code) when ODA is unavailable or the stream cannot be parsed.
RenderResult collect_w2d_file_layers(
    const std::string& w2d_path,
    std::vector<W2dLayer>& out_layers);

} // namespace zs::dwf::w2d
