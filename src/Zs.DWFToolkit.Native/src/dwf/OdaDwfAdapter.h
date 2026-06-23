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

// Reads package-level info (sections -> pages) from a DWF/DWFx file and returns a
// JSON document matching the DwfDocumentInfo contract in docs/NATIVE_INTEGRATION.md.
// Only meaningful when built with ZS_DWF_WITH_ODA_DWFTK; returns an
// unsupported_dwf_rendering JSON otherwise.
struct InfoResult
{
    bool success{false};
    int result_code{0};
    std::string json;
};

InfoResult get_dwf_info_json(const std::string& input_path);

// Renders every 2D page of a DWF/DWFx (or a bare W2D stream) to a single
// multi-page, true-vector PDF written to output_pdf_path. Returns an
// unsupported result when built without ZS_DWF_WITH_ODA_DWFTK.
zs::dwf::w2d::RenderResult render_dwf_to_pdf(const std::string& input_path, const std::string& output_pdf_path);

} // namespace zs::dwf::oda
