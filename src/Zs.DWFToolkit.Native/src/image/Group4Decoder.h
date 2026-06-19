#pragma once
#include <cstdint>
#include <vector>

namespace zs::dwf::image
{

// Decodes a raw CCITT Group 4 (T.6) fax bitstream of the given dimensions into
// one byte per pixel (0 or 1, the raw bit value, row-major). Returns false if the
// decode fails or TIFF support was not compiled in (ZS_DWF_WITH_TIFF).
bool decode_group4(const std::uint8_t* data, std::size_t size, int cols, int rows,
                   std::vector<std::uint8_t>& out_indices);

} // namespace zs::dwf::image
