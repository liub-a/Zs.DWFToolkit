#pragma once
#include "../render/RasterCanvas.h"
#include <string>

namespace zs::dwf::text
{

// Renders text into a RasterCanvas using FreeType when the library is available
// (ZS_DWF_WITH_FREETYPE) and a usable font is found. When unavailable, ready()
// returns false and callers fall back to a placeholder box.
class TextRenderer
{
public:
    TextRenderer();
    ~TextRenderer();
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    bool ready() const { return _ready; }

    // Draws `utf8` text with its baseline starting at pixel (pen_x, pen_y), at the
    // given pixel height, in `color`. Returns false if rendering was not possible.
    bool draw(zs::dwf::native_render::RasterCanvas& canvas,
              const std::string& utf8,
              int pen_x,
              int pen_y,
              int pixel_height,
              zs::dwf::native_render::Rgba color);

private:
    bool _ready{false};
    void* _library{nullptr}; // FT_Library
    void* _face{nullptr};    // FT_Face
};

} // namespace zs::dwf::text
