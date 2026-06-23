#pragma once
#include <cstdint>
#include <string>

namespace zs::dwf::edit
{

// Stamps an RGBA image onto a W2D stream: copies every opcode from in_path to
// out_path and appends a WT_Image at the given logical rectangle (drawn on top).
// rgba is row-major width*height*4 bytes. Returns true on success. Requires the
// ODA build (ZS_DWF_WITH_ODA_DWFTK); otherwise returns false.
bool stamp_image_on_w2d(const std::string& in_path,
                        const std::string& out_path,
                        const std::uint8_t* rgba, std::size_t rgba_len,
                        int img_w, int img_h,
                        int min_x, int min_y, int max_x, int max_y,
                        std::string& error);

} // namespace zs::dwf::edit
