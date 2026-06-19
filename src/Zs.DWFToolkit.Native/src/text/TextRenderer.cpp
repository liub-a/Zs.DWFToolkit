#include "TextRenderer.h"

#include <cstdlib>

#ifdef ZS_DWF_WITH_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include "embedded_font.h"
#endif

namespace zs::dwf::text
{
using zs::dwf::native_render::RasterCanvas;
using zs::dwf::native_render::Rgba;

TextRenderer::TextRenderer()
{
#ifdef ZS_DWF_WITH_FREETYPE
    FT_Library lib;
    if (FT_Init_FreeType(&lib) != 0)
        return;

    FT_Face face = nullptr;
    // Optional override: ZS_DWF_FONT points at an external TTF. Default uses the
    // font compiled into the library, so no system font is required.
    if (const char* env = std::getenv("ZS_DWF_FONT"))
        FT_New_Face(lib, env, 0, &face);
    if (!face)
    {
        if (FT_New_Memory_Face(lib, kEmbeddedFont, static_cast<FT_Long>(kEmbeddedFontSize), 0, &face) != 0)
        {
            FT_Done_FreeType(lib);
            return;
        }
    }
    _library = lib;
    _face = face;
    _ready = true;
#endif
}

TextRenderer::~TextRenderer()
{
#ifdef ZS_DWF_WITH_FREETYPE
    if (_face) FT_Done_Face(static_cast<FT_Face>(_face));
    if (_library) FT_Done_FreeType(static_cast<FT_Library>(_library));
#endif
}

bool TextRenderer::draw(RasterCanvas& canvas, const std::string& utf8,
                        int pen_x, int pen_y, int pixel_height, Rgba color)
{
#ifdef ZS_DWF_WITH_FREETYPE
    if (!_ready) return false;
    auto face = static_cast<FT_Face>(_face);
    if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixel_height > 0 ? pixel_height : 16)) != 0)
        return false;

    int pen = pen_x;
    for (unsigned char ch : utf8)
    {
        if (FT_Load_Char(face, ch, FT_LOAD_RENDER) != 0)
            continue;
        const FT_GlyphSlot g = face->glyph;
        const FT_Bitmap& bm = g->bitmap;
        canvas.blend_coverage(pen + g->bitmap_left, pen_y - g->bitmap_top,
                              bm.buffer, static_cast<int>(bm.width), static_cast<int>(bm.rows),
                              bm.pitch, color);
        pen += static_cast<int>(g->advance.x >> 6);
    }
    return true;
#else
    (void)canvas; (void)utf8; (void)pen_x; (void)pen_y; (void)pixel_height; (void)color;
    return false;
#endif
}

} // namespace zs::dwf::text
