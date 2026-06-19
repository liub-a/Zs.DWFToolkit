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
    void fill_polygon(const std::vector<PointD>& pts, Rgba color);
    void draw_ellipse(PointD center, double radius_x, double radius_y, double tilt_rad, Rgba color, int thickness);
    void fill_ellipse(PointD center, double radius_x, double radius_y, double tilt_rad, Rgba color);
    void draw_text_marker(PointD position, int glyph_count, Rgba color, int thickness);

private:
    void set_pixel(int x, int y, Rgba color);
    void draw_disc(int cx, int cy, int radius, Rgba color);

    int _width;
    int _height;
    BoxD _logical_box;
    double _scale{1};
    double _offset_x{0};
    double _offset_y{0};
    std::vector<Rgba> _pixels;
};

} // namespace zs::dwf::native_render
