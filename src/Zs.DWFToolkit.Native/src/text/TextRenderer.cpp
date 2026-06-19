#include "TextRenderer.h"

#include <cstdio>
#include <cstdlib>

#ifdef ZS_DWF_WITH_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

namespace zs::dwf::text
{
using zs::dwf::native_render::RasterCanvas;
using zs::dwf::native_render::Rgba;

#ifdef ZS_DWF_WITH_FREETYPE
namespace
{
    // Tries an env override first, then common system font paths across platforms.
    const char* candidate_font_path()
    {
        if (const char* env = std::getenv("ZS_DWF_FONT"))
            return env;
        static const char* const candidates[] = {
            "/System/Library/Fonts/Supplemental/Arial.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
            "/Library/Fonts/Arial.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "C:\\Windows\\Fonts\\arial.ttf",
        };
        for (const char* p : candidates)
        {
            if (FILE* f = std::fopen(p, "rb")) { std::fclose(f); return p; }
        }
        return nullptr;
    }
}
#endif

TextRenderer::TextRenderer()
{
#ifdef ZS_DWF_WITH_FREETYPE
    const char* font = candidate_font_path();
    if (!font)
        return;
    FT_Library lib;
    if (FT_Init_FreeType(&lib) != 0)
        return;
    FT_Face face;
    if (FT_New_Face(lib, font, 0, &face) != 0)
    {
        FT_Done_FreeType(lib);
        return;
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
