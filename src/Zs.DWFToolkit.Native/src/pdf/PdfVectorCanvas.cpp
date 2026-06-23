#include "PdfVectorCanvas.h"

#include <cmath>
#include <cstdio>

#ifdef ZS_DWF_WITH_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include "embedded_font.h"
#endif

namespace zs::dwf::pdf
{
namespace
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kKappa = 0.5522847498307936; // circle bezier control factor

    // Appends a number with 2-decimal precision (compact PDF coordinates).
    void num(std::string& s, double v)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", v);
        s += buf;
    }

    void pt(std::string& s, double x, double y, const char* op)
    {
        num(s, x); s += ' '; num(s, y); s += ' '; s += op; s += '\n';
    }

    void color_op(std::string& s, Rgba c, const char* op)
    {
        num(s, c.r / 255.0); s += ' ';
        num(s, c.g / 255.0); s += ' ';
        num(s, c.b / 255.0); s += ' '; s += op; s += '\n';
    }
}

PdfVectorCanvas::PdfVectorCanvas(BoxD logical_box, double page_w_pt, double page_h_pt)
    : _box(logical_box), _page_w(page_w_pt), _page_h(page_h_pt)
{
    const double bw = (_box.max_x - _box.min_x);
    const double bh = (_box.max_y - _box.min_y);
    _sx = (bw > 0) ? _page_w / bw : 1.0;
    _sy = (bh > 0) ? _page_h / bh : 1.0;
    _scale = 0.5 * (_sx + _sy);

#ifdef ZS_DWF_WITH_FREETYPE
    FT_Library lib;
    if (FT_Init_FreeType(&lib) == 0)
    {
        FT_Face face = nullptr;
        if (FT_New_Memory_Face(lib, zs::dwf::text::kEmbeddedFont,
                               static_cast<FT_Long>(zs::dwf::text::kEmbeddedFontSize), 0, &face) == 0)
        {
            FT_Face fb = nullptr;
            if (FT_New_Memory_Face(lib, zs::dwf::text::kEmbeddedFallbackFont,
                                   static_cast<FT_Long>(zs::dwf::text::kEmbeddedFallbackFontSize), 0, &fb) != 0)
                fb = nullptr;
            _ft_library = lib;
            _ft_face = face;
            _ft_fallback = fb;
            _ft_ready = true;
        }
        else
        {
            FT_Done_FreeType(lib);
        }
    }
#endif
}

PdfVectorCanvas::~PdfVectorCanvas()
{
#ifdef ZS_DWF_WITH_FREETYPE
    if (_ft_face) FT_Done_Face(static_cast<FT_Face>(_ft_face));
    if (_ft_fallback) FT_Done_Face(static_cast<FT_Face>(_ft_fallback));
    if (_ft_library) FT_Done_FreeType(static_cast<FT_Library>(_ft_library));
#endif
}

PointD PdfVectorCanvas::map(PointD p) const
{
    return { (p.x - _box.min_x) * _sx, (p.y - _box.min_y) * _sy };
}

PdfPage PdfVectorCanvas::take_page()
{
    if (_clip_open)
    {
        _content += "Q\n";
        _clip_open = false;
    }
    PdfPage page;
    page.media_w_pt = _page_w;
    page.media_h_pt = _page_h;
    page.content = std::move(_content);
    page.images = std::move(_images);
    return page;
}

void PdfVectorCanvas::emit_stroke_state(Rgba color, int thickness)
{
    color_op(_content, color, "RG");
    num(_content, std::max(1, thickness) * 1.0);
    _content += " w\n";
}

void PdfVectorCanvas::emit_fill_color(Rgba color)
{
    color_op(_content, color, "rg");
}

void PdfVectorCanvas::draw_polyline(const std::vector<PointD>& pts, Rgba color, int thickness, bool closed)
{
    if (pts.size() < 2) return;
    emit_stroke_state(color, thickness);
    const PointD a = map(pts[0]);
    pt(_content, a.x, a.y, "m");
    for (std::size_t i = 1; i < pts.size(); ++i)
    {
        const PointD p = map(pts[i]);
        pt(_content, p.x, p.y, "l");
    }
    if (closed) _content += "h\n";
    _content += "S\n";
}

void PdfVectorCanvas::draw_dashed_polyline(const std::vector<PointD>& pts, Rgba color, int thickness,
                                           double dash_on, double dash_off, bool closed)
{
    if (pts.size() < 2) return;
    _content += '['; num(_content, std::max(0.5, dash_on)); _content += ' ';
    num(_content, std::max(0.5, dash_off)); _content += "] 0 d\n";
    draw_polyline(pts, color, thickness, closed);
    _content += "[] 0 d\n"; // reset to solid
}

void PdfVectorCanvas::fill_polygon(const std::vector<PointD>& pts, Rgba color)
{
    if (pts.size() < 3) return;
    emit_fill_color(color);
    const PointD a = map(pts[0]);
    pt(_content, a.x, a.y, "m");
    for (std::size_t i = 1; i < pts.size(); ++i)
    {
        const PointD p = map(pts[i]);
        pt(_content, p.x, p.y, "l");
    }
    _content += "h\nf\n";
}

void PdfVectorCanvas::hatch_polygon(const std::vector<PointD>& pts, Rgba color, int spacing, bool back_diagonal)
{
    if (pts.size() < 3) return;
    // Clip to the polygon, then stroke diagonal lines across its bounding box.
    BoxD bb;
    for (const auto& p : pts) { const PointD m = map(p); bb.include(m.x, m.y); }
    if (!bb.valid()) return;
    const double step = std::max(2.0, spacing * 1.0);

    _content += "q\n";
    const PointD a = map(pts[0]);
    pt(_content, a.x, a.y, "m");
    for (std::size_t i = 1; i < pts.size(); ++i) { const PointD p = map(pts[i]); pt(_content, p.x, p.y, "l"); }
    _content += "h\nW n\n"; // set clip to polygon (nonzero)

    color_op(_content, color, "RG");
    _content += "0.50 w\n";
    const double w = bb.max_x - bb.min_x;
    const double h = bb.max_y - bb.min_y;
    // Family of 45-degree lines y = x + c (or y = -x + c) covering the bbox.
    const double cstart = back_diagonal ? (bb.min_y - bb.max_x) : (bb.min_y - bb.max_x);
    (void)cstart;
    for (double off = -(w + h); off <= (w + h); off += step)
    {
        double x0, y0, x1, y1;
        if (!back_diagonal)
        {
            x0 = bb.min_x;             y0 = bb.min_y + off;
            x1 = bb.max_x;             y1 = bb.min_y + off + w;
        }
        else
        {
            x0 = bb.min_x;             y0 = bb.max_y - off;
            x1 = bb.max_x;             y1 = bb.max_y - off - w;
        }
        pt(_content, x0, y0, "m");
        pt(_content, x1, y1, "l");
    }
    _content += "S\nQ\n";
}

void PdfVectorCanvas::fill_contours(const std::vector<std::vector<PointD>>& contours, Rgba color)
{
    bool any = false;
    std::string body;
    for (const auto& ring : contours)
    {
        if (ring.size() < 3) continue;
        const PointD a = map(ring[0]);
        pt(body, a.x, a.y, "m");
        for (std::size_t i = 1; i < ring.size(); ++i) { const PointD p = map(ring[i]); pt(body, p.x, p.y, "l"); }
        body += "h\n";
        any = true;
    }
    if (!any) return;
    emit_fill_color(color);
    _content += body;
    _content += "f*\n"; // even-odd so inner rings are holes
}

void PdfVectorCanvas::ellipse_path(PointD center, double rx, double ry, double tilt_rad)
{
    // Four cubic-bezier arcs approximating the (possibly tilted) ellipse, mapped.
    const double ct = std::cos(tilt_rad);
    const double st = std::sin(tilt_rad);
    auto ep = [&](double ex, double ey) -> PointD {
        // point on axis-aligned ellipse (ex,ey in [-1,1] scaled by rx,ry), tilted
        const double lx = ex * rx;
        const double ly = ey * ry;
        return map({ center.x + lx * ct - ly * st, center.y + lx * st + ly * ct });
    };
    const double k = kKappa;
    const PointD p0 = ep(1, 0), p1 = ep(0, 1), p2 = ep(-1, 0), p3 = ep(0, -1);
    const PointD c0a = ep(1, k), c0b = ep(k, 1);
    const PointD c1a = ep(-k, 1), c1b = ep(-1, k);
    const PointD c2a = ep(-1, -k), c2b = ep(-k, -1);
    const PointD c3a = ep(k, -1), c3b = ep(1, -k);
    pt(_content, p0.x, p0.y, "m");
    num(_content, c0a.x); _content += ' '; num(_content, c0a.y); _content += ' ';
    num(_content, c0b.x); _content += ' '; num(_content, c0b.y); _content += ' ';
    num(_content, p1.x);  _content += ' '; num(_content, p1.y);  _content += " c\n";
    num(_content, c1a.x); _content += ' '; num(_content, c1a.y); _content += ' ';
    num(_content, c1b.x); _content += ' '; num(_content, c1b.y); _content += ' ';
    num(_content, p2.x);  _content += ' '; num(_content, p2.y);  _content += " c\n";
    num(_content, c2a.x); _content += ' '; num(_content, c2a.y); _content += ' ';
    num(_content, c2b.x); _content += ' '; num(_content, c2b.y); _content += ' ';
    num(_content, p3.x);  _content += ' '; num(_content, p3.y);  _content += " c\n";
    num(_content, c3a.x); _content += ' '; num(_content, c3a.y); _content += ' ';
    num(_content, c3b.x); _content += ' '; num(_content, c3b.y); _content += ' ';
    num(_content, p0.x);  _content += ' '; num(_content, p0.y);  _content += " c\n";
    _content += "h\n";
}

void PdfVectorCanvas::draw_ellipse(PointD center, double rx, double ry, double tilt_rad, Rgba color, int thickness)
{
    emit_stroke_state(color, thickness);
    ellipse_path(center, rx, ry, tilt_rad);
    _content += "S\n";
}

void PdfVectorCanvas::fill_ellipse(PointD center, double rx, double ry, double tilt_rad, Rgba color)
{
    emit_fill_color(color);
    ellipse_path(center, rx, ry, tilt_rad);
    _content += "f\n";
}

void PdfVectorCanvas::draw_text_marker(PointD position, int glyph_count, Rgba color, int thickness)
{
    // Light outline box approximating the text extent (fallback when no glyphs).
    const double h = 8.0 * _scale;
    const double w = std::max(1, glyph_count) * h * 0.6;
    const std::vector<PointD> box = {
        position,
        { position.x + w / std::max(_sx, 1e-9), position.y },
        { position.x + w / std::max(_sx, 1e-9), position.y + h / std::max(_sy, 1e-9) },
        { position.x, position.y + h / std::max(_sy, 1e-9) }
    };
    draw_polyline(box, color, std::max(1, thickness), true);
}

void PdfVectorCanvas::fill_marker(PointD center, int radius, Rgba color)
{
    const PointD c = map(center);
    const double r = std::max(1, radius) * 1.0;
    emit_fill_color(color);
    num(_content, c.x - r); _content += ' '; num(_content, c.y - r); _content += ' ';
    num(_content, 2 * r);   _content += ' '; num(_content, 2 * r);   _content += " re\nf\n";
}

void PdfVectorCanvas::draw_image(const BoxD& logical_rect, int src_w, int src_h, const std::vector<Rgba>& src_pixels)
{
    if (src_w <= 0 || src_h <= 0 ||
        src_pixels.size() < static_cast<std::size_t>(src_w) * src_h)
        return;
    PdfImage img;
    img.width = src_w;
    img.height = src_h;
    img.rgb.resize(static_cast<std::size_t>(src_w) * src_h * 3);
    for (std::size_t i = 0; i < static_cast<std::size_t>(src_w) * src_h; ++i)
    {
        const Rgba& s = src_pixels[i];
        // Composite over white using the alpha (PDF image is opaque RGB).
        const double a = s.a / 255.0;
        img.rgb[i * 3 + 0] = static_cast<std::uint8_t>(s.r * a + 255 * (1 - a));
        img.rgb[i * 3 + 1] = static_cast<std::uint8_t>(s.g * a + 255 * (1 - a));
        img.rgb[i * 3 + 2] = static_cast<std::uint8_t>(s.b * a + 255 * (1 - a));
    }
    const std::size_t index = _images.size();
    _images.push_back(std::move(img));

    const PointD lo = map({ logical_rect.min_x, logical_rect.min_y });
    const PointD hi = map({ logical_rect.max_x, logical_rect.max_y });
    const double w = hi.x - lo.x;
    const double h = hi.y - lo.y;
    _content += "q\n";
    num(_content, w); _content += " 0 0 ";
    num(_content, h); _content += ' ';
    num(_content, lo.x); _content += ' '; num(_content, lo.y); _content += " cm\n";
    _content += "/Im"; _content += std::to_string(index); _content += " Do\nQ\n";
}

void PdfVectorCanvas::set_clip(const BoxD& logical_rect)
{
    if (_clip_open) { _content += "Q\n"; _clip_open = false; }
    const PointD lo = map({ logical_rect.min_x, logical_rect.min_y });
    const PointD hi = map({ logical_rect.max_x, logical_rect.max_y });
    _content += "q\n";
    num(_content, lo.x); _content += ' '; num(_content, lo.y); _content += ' ';
    num(_content, hi.x - lo.x); _content += ' '; num(_content, hi.y - lo.y); _content += " re\nW n\n";
    _clip_open = true;
}

void PdfVectorCanvas::set_clip_contours(const std::vector<std::vector<PointD>>& contours)
{
    if (_clip_open) { _content += "Q\n"; _clip_open = false; }
    bool any = false;
    std::string body;
    for (const auto& ring : contours)
    {
        if (ring.size() < 3) continue;
        const PointD a = map(ring[0]);
        pt(body, a.x, a.y, "m");
        for (std::size_t i = 1; i < ring.size(); ++i) { const PointD p = map(ring[i]); pt(body, p.x, p.y, "l"); }
        body += "h\n";
        any = true;
    }
    if (!any) return;
    _content += "q\n";
    _content += body;
    _content += "W* n\n";
    _clip_open = true;
}

void PdfVectorCanvas::clear_clip()
{
    if (_clip_open) { _content += "Q\n"; _clip_open = false; }
}

#ifdef ZS_DWF_WITH_FREETYPE
namespace
{
    struct GlyphBuilder
    {
        std::string* out{nullptr};
        double pen_x{0}, pen_y{0};
        double cos_r{1}, sin_r{0};
        double cur_x{0}, cur_y{0};

        PointD xf(double gx, double gy) const
        {
            // glyph coords (points, y-up) rotated then offset to the pen baseline.
            return { pen_x + gx * cos_r - gy * sin_r, pen_y + gx * sin_r + gy * cos_r };
        }
    };

    int gb_move(const FT_Vector* to, void* user)
    {
        auto* b = static_cast<GlyphBuilder*>(user);
        const PointD p = b->xf(to->x / 64.0, to->y / 64.0);
        b->cur_x = p.x; b->cur_y = p.y;
        pt(*b->out, p.x, p.y, "m");
        return 0;
    }
    int gb_line(const FT_Vector* to, void* user)
    {
        auto* b = static_cast<GlyphBuilder*>(user);
        const PointD p = b->xf(to->x / 64.0, to->y / 64.0);
        b->cur_x = p.x; b->cur_y = p.y;
        pt(*b->out, p.x, p.y, "l");
        return 0;
    }
    int gb_conic(const FT_Vector* control, const FT_Vector* to, void* user)
    {
        auto* b = static_cast<GlyphBuilder*>(user);
        const PointD q = b->xf(control->x / 64.0, control->y / 64.0);
        const PointD e = b->xf(to->x / 64.0, to->y / 64.0);
        // Quadratic -> cubic control points.
        const double c1x = b->cur_x + 2.0 / 3.0 * (q.x - b->cur_x);
        const double c1y = b->cur_y + 2.0 / 3.0 * (q.y - b->cur_y);
        const double c2x = e.x + 2.0 / 3.0 * (q.x - e.x);
        const double c2y = e.y + 2.0 / 3.0 * (q.y - e.y);
        num(*b->out, c1x); *b->out += ' '; num(*b->out, c1y); *b->out += ' ';
        num(*b->out, c2x); *b->out += ' '; num(*b->out, c2y); *b->out += ' ';
        num(*b->out, e.x); *b->out += ' '; num(*b->out, e.y); *b->out += " c\n";
        b->cur_x = e.x; b->cur_y = e.y;
        return 0;
    }
    int gb_cubic(const FT_Vector* c1, const FT_Vector* c2, const FT_Vector* to, void* user)
    {
        auto* b = static_cast<GlyphBuilder*>(user);
        const PointD p1 = b->xf(c1->x / 64.0, c1->y / 64.0);
        const PointD p2 = b->xf(c2->x / 64.0, c2->y / 64.0);
        const PointD e = b->xf(to->x / 64.0, to->y / 64.0);
        num(*b->out, p1.x); *b->out += ' '; num(*b->out, p1.y); *b->out += ' ';
        num(*b->out, p2.x); *b->out += ' '; num(*b->out, p2.y); *b->out += ' ';
        num(*b->out, e.x);  *b->out += ' '; num(*b->out, e.y);  *b->out += " c\n";
        b->cur_x = e.x; b->cur_y = e.y;
        return 0;
    }
}
#endif

bool PdfVectorCanvas::draw_text(const unsigned short* codepoints, int count, PointD position_logical,
                                double height_units, double rotation_deg, Rgba color)
{
#ifdef ZS_DWF_WITH_FREETYPE
    if (!_ft_ready || !codepoints || count <= 0) return false;
    auto primary = static_cast<FT_Face>(_ft_face);
    auto fallback = static_cast<FT_Face>(_ft_fallback);

    int px = static_cast<int>(std::llround(height_units * _scale));
    if (px < 4) px = 12;
    if (px > 4000) px = 4000;

    const double rad = rotation_deg * kPi / 180.0;
    const PointD pen0 = map(position_logical);

    GlyphBuilder b;
    b.out = &_content;
    b.pen_x = pen0.x; b.pen_y = pen0.y;
    b.cos_r = std::cos(rad); b.sin_r = std::sin(rad);

    FT_Outline_Funcs funcs;
    funcs.move_to = gb_move;
    funcs.line_to = gb_line;
    funcs.conic_to = gb_conic;
    funcs.cubic_to = gb_cubic;
    funcs.shift = 0;
    funcs.delta = 0;

    emit_fill_color(color);
    bool emitted = false;
    for (int i = 0; i < count; ++i)
    {
        const unsigned int cp = codepoints[i];
        FT_Face face = primary;
        FT_UInt gi = FT_Get_Char_Index(primary, cp);
        if (gi == 0 && fallback)
        {
            const FT_UInt fgi = FT_Get_Char_Index(fallback, cp);
            if (fgi != 0) { face = fallback; gi = fgi; }
        }
        FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(px));
        if (gi != 0 && FT_Load_Glyph(face, gi, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING) == 0)
        {
            if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE)
            {
                FT_Outline_Decompose(&face->glyph->outline, &funcs, &b);
                emitted = true;
            }
            const double adv = face->glyph->advance.x / 64.0;
            b.pen_x += adv * b.cos_r;
            b.pen_y += adv * b.sin_r;
        }
        else
        {
            // Unmapped glyph: advance by roughly half an em so spacing is sane.
            const double adv = px * 0.5;
            b.pen_x += adv * b.cos_r;
            b.pen_y += adv * b.sin_r;
        }
    }
    if (emitted) _content += "f\n"; // nonzero winding fills glyphs with holes
    return emitted;
#else
    (void)codepoints; (void)count; (void)position_logical;
    (void)height_units; (void)rotation_deg; (void)color;
    return false;
#endif
}

} // namespace zs::dwf::pdf
