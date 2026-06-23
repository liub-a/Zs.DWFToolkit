#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace zs::dwf::pdf
{

// A raster sub-image to embed on a page as an /Image XObject. Pixels are row-major
// RGB (8 bits/component); embedded uncompressed (no zlib dependency). The content
// stream references it as "/Im<index> Do" under a current transformation matrix.
struct PdfImage
{
    int width{0};
    int height{0};
    std::vector<std::uint8_t> rgb; // width*height*3 bytes
};

// One PDF page: a MediaBox in points and a content stream of PDF drawing operators
// (uncompressed). Coordinates in the content stream are PDF user space (origin
// bottom-left, y up) within [0,media_w_pt] x [0,media_h_pt].
struct PdfPage
{
    double media_w_pt{612};
    double media_h_pt{792};
    std::string content;
    std::vector<PdfImage> images;
};

// Assembles a multi-page PDF (PDF 1.7) and writes it to path. Content streams and
// images are embedded uncompressed, so the writer has no external dependencies.
// Returns false and sets error on failure.
bool write_pdf(const std::string& path, const std::vector<PdfPage>& pages, std::string& error);

} // namespace zs::dwf::pdf
