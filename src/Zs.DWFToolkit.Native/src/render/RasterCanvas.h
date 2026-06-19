#pragma once
#include "MinimalPng.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace zs::dwf::native_render
{

struct PointD
{
    double x{0};
    double y{0};
};

struct BoxD
{
    double min_x{std::numeric_limits<double>::infinity()};
    double min_y{std::numeric_limits<double>::infinity()};
    double max_x{-std::numeric_limits<double>::infinity()};
    double max_y{-std::numeric_limits<double>::infinity()};

    bool valid() const { return min_x <= max_x && min_y <= max_y; }

    void include(double x, double y)
    {
        min_x = std::min(min_x, x);
        min_y = std::min(min_y, y);
        max_x = std::max(max_x, x);
        max_y = std::max(max_y, y);
    }

    void include(const BoxD& b)
    {
        if (!b.valid()) return;
        include(b.min_x, b.min_y);
        include(b.max_x, b.max_y);
    }
};

class RasterCanvas
{
public:
    RasterCanvas(int width, int height, BoxD logical_box);

    int width() const { return _width; }
    int height() const { return _height; }
    const BoxD& logical_box() const { return _logical_box; }
    double scale() const { return _scale; }
    double offset_x() const { return _offset_x; }
    double offset_y() const { return _offset_y; }
    const std::vector<Rgba>& pixels() const { return _pixels; }

    PointD to_pixel(PointD p) const;

    void clear(Rgba c);
    void draw_line(PointD a, PointD b, Rgba color, int thickness);
    void draw_polyline(const std::vector<PointD>& pts, Rgba color, int thickness, bool closed = false);
    // Like draw_polyline but stroked as a dash pattern (on/off lengths in pixels).
    // The dash phase is carried across vertices so corners stay continuous.
    void draw_dashed_polyline(const std::vector<PointD>& pts, Rgba color, int thickness,
                              double dash_on_px, double dash_off_px, bool closed = false);
    void fill_polygon(const std::vector<PointD>& pts, Rgba color);
    // Fills the polygon interior with diagonal hatch lines spaced spacing_px apart
    // (45 degrees; back_diagonal flips the slope). Approximates W2D hatch fills.
    void hatch_polygon(const std::vector<PointD>& pts, Rgba color, int spacing_px, bool back_diagonal = false);
    // Fills a set of contours with the even-odd rule, so inner contours act as
    // holes. Used for W2D contour sets.
    void fill_contours(const std::vector<std::vector<PointD>>& contours, Rgba color);
    void draw_ellipse(PointD center, double radius_x, double radius_y, double tilt_rad, Rgba color, int thickness);
    void fill_ellipse(PointD center, double radius_x, double radius_y, double tilt_rad, Rgba color);
    void draw_text_marker(PointD position, int glyph_count, Rgba color, int thickness);
    // Draws a filled disc of radius_px pixels centred at the given logical point
    // (screen-constant size). Used for polymarkers.
    void fill_marker(PointD center, int radius_px, Rgba color);

    // Alpha-composites an 8-bit coverage bitmap (e.g. a FreeType glyph) at pixel
    // (dst_x, dst_y) top-left, using `color` modulated by per-pixel coverage.
    // `pitch` is the bytes-per-row stride of the coverage buffer.
    void blend_coverage(int dst_x, int dst_y, const std::uint8_t* coverage,
                        int w, int h, int pitch, Rgba color);

    // Blit a row-major RGBA image (src_w x src_h) into the logical rectangle
    // [min..max] using nearest-neighbour sampling. The rectangle is mapped through
    // the same logical->pixel transform as vector primitives so images line up
    // with the drawing.
    void draw_image(const BoxD& logical_rect,
                    int src_w,
                    int src_h,
                    const std::vector<Rgba>& src_pixels);

    // Restrict subsequent drawing to the pixels inside this logical rectangle
    // (mapped through the same transform). Used for W2D viewport/clip regions.
    void set_clip(const BoxD& logical_rect);
    void clear_clip();

private:
    void set_pixel(int x, int y, Rgba color);
    void draw_disc(int cx, int cy, int radius, Rgba color);

    int _width;
    int _height;
    BoxD _logical_box;
    double _scale{1};
    double _offset_x{0};
    double _offset_y{0};
    bool _has_clip{false};
    int _clip_x0{0};
    int _clip_y0{0};
    int _clip_x1{0};
    int _clip_y1{0};
    std::vector<Rgba> _pixels;
};

} // namespace zs::dwf::native_render
