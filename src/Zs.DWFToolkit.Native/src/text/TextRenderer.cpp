#include "TextRenderer.h"

#include <cmath>
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

bool TextRenderer::draw(RasterCanvas& canvas, const unsigned short* codepoints, int count,
                        int pen_x, int pen_y, int pixel_height, double rotation_deg, Rgba color)
{
#ifdef ZS_DWF_WITH_FREETYPE
    if (!_ready || !codepoints || count <= 0) return false;
    auto face = static_cast<FT_Face>(_face);
    if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixel_height > 0 ? pixel_height : 16)) != 0)
        return false;

    const double rad = rotation_deg * 3.14159265358979323846 / 180.0;
    const bool rotated = rotation_deg != 0.0;
    if (rotated)
    {
        // 16.16 fixed-point rotation matrix (FreeType y axis points up).
        FT_Matrix m;
        m.xx = static_cast<FT_Fixed>(std::cos(rad) * 0x10000L);
        m.xy = static_cast<FT_Fixed>(-std::sin(rad) * 0x10000L);
        m.yx = static_cast<FT_Fixed>(std::sin(rad) * 0x10000L);
        m.yy = static_cast<FT_Fixed>(std::cos(rad) * 0x10000L);
        FT_Set_Transform(face, &m, nullptr);
    }
    else
    {
        FT_Set_Transform(face, nullptr, nullptr);
    }

    // Sub-pixel pen position in 26.6 advances; canvas y grows down, FT advance y up.
    double pen_x_d = pen_x;
    double pen_y_d = pen_y;
    for (int i = 0; i < count; ++i)
    {
        if (FT_Load_Char(face, codepoints[i], FT_LOAD_RENDER) != 0)
            continue;
        const FT_GlyphSlot g = face->glyph;
        const FT_Bitmap& bm = g->bitmap;
        canvas.blend_coverage(static_cast<int>(std::lround(pen_x_d)) + g->bitmap_left,
                              static_cast<int>(std::lround(pen_y_d)) - g->bitmap_top,
                              bm.buffer, static_cast<int>(bm.width), static_cast<int>(bm.rows),
                              bm.pitch, color);
        pen_x_d += (g->advance.x / 64.0);
        pen_y_d -= (g->advance.y / 64.0);
    }
    if (rotated) FT_Set_Transform(face, nullptr, nullptr);
    return true;
#else
    (void)canvas; (void)codepoints; (void)count; (void)pen_x; (void)pen_y;
    (void)pixel_height; (void)rotation_deg; (void)color;
    return false;
#endif
}

} // namespace zs::dwf::text
