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
    // Fallback face (CJK etc.) from the embedded Droid Sans Fallback; optional.
    FT_Face fb = nullptr;
    if (FT_New_Memory_Face(lib, kEmbeddedFallbackFont, static_cast<FT_Long>(kEmbeddedFallbackFontSize), 0, &fb) != 0)
        fb = nullptr;

    _library = lib;
    _face = face;
    _fallback = fb;
    _ready = true;
#endif
}

TextRenderer::~TextRenderer()
{
#ifdef ZS_DWF_WITH_FREETYPE
    if (_face) FT_Done_Face(static_cast<FT_Face>(_face));
    if (_fallback) FT_Done_Face(static_cast<FT_Face>(_fallback));
    if (_library) FT_Done_FreeType(static_cast<FT_Library>(_library));
#endif
}

bool TextRenderer::draw(RasterCanvas& canvas, const unsigned short* codepoints, int count,
                        int pen_x, int pen_y, int pixel_height, double rotation_deg, Rgba color)
{
#ifdef ZS_DWF_WITH_FREETYPE
    if (!_ready || !codepoints || count <= 0) return false;
    auto primary = static_cast<FT_Face>(_face);
    auto fallback = static_cast<FT_Face>(_fallback);
    const FT_UInt px = static_cast<FT_UInt>(pixel_height > 0 ? pixel_height : 16);

    const double rad = rotation_deg * 3.14159265358979323846 / 180.0;
    const bool rotated = rotation_deg != 0.0;
    FT_Matrix m;
    m.xx = static_cast<FT_Fixed>(std::cos(rad) * 0x10000L);
    m.xy = static_cast<FT_Fixed>(-std::sin(rad) * 0x10000L);
    m.yx = static_cast<FT_Fixed>(std::sin(rad) * 0x10000L);
    m.yy = static_cast<FT_Fixed>(std::cos(rad) * 0x10000L);

    auto configure = [&](FT_Face f) {
        if (!f) return;
        FT_Set_Pixel_Sizes(f, 0, px);
        FT_Set_Transform(f, rotated ? &m : nullptr, nullptr);
    };
    configure(primary);
    configure(fallback);

    // Sub-pixel pen position; canvas y grows down, FT advance y up.
    double pen_x_d = pen_x;
    double pen_y_d = pen_y;
    for (int i = 0; i < count; ++i)
    {
        // Pick the face that actually has the glyph: primary (Latin) first, then
        // the CJK fallback.
        FT_Face f = primary;
        if (FT_Get_Char_Index(primary, codepoints[i]) == 0 &&
            fallback && FT_Get_Char_Index(fallback, codepoints[i]) != 0)
            f = fallback;

        if (FT_Load_Char(f, codepoints[i], FT_LOAD_RENDER) != 0)
            continue;
        const FT_GlyphSlot g = f->glyph;
        const FT_Bitmap& bm = g->bitmap;
        canvas.blend_coverage(static_cast<int>(std::lround(pen_x_d)) + g->bitmap_left,
                              static_cast<int>(std::lround(pen_y_d)) - g->bitmap_top,
                              bm.buffer, static_cast<int>(bm.width), static_cast<int>(bm.rows),
                              bm.pitch, color);
        pen_x_d += (g->advance.x / 64.0);
        pen_y_d -= (g->advance.y / 64.0);
    }
    if (rotated) { FT_Set_Transform(primary, nullptr, nullptr); if (fallback) FT_Set_Transform(fallback, nullptr, nullptr); }
    return true;
#else
    (void)canvas; (void)codepoints; (void)count; (void)pen_x; (void)pen_y;
    (void)pixel_height; (void)rotation_deg; (void)color;
    return false;
#endif
}

} // namespace zs::dwf::text
