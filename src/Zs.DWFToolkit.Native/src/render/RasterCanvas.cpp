#include "RasterCanvas.h"
#include <cstdlib>

namespace zs::dwf::native_render
{
namespace
{
    constexpr double kPi = 3.141592653589793238462643383279502884;

    int clamp_int(int v, int lo, int hi)
    {
        return std::max(lo, std::min(hi, v));
    }
}

RasterCanvas::RasterCanvas(int width, int height, BoxD logical_box)
    : _width(width), _height(height), _logical_box(logical_box)
{
    if (!_logical_box.valid())
        _logical_box = BoxD{0, 0, 1000, 1000};

    const double logical_w = std::max(1.0, _logical_box.max_x - _logical_box.min_x);
    const double logical_h = std::max(1.0, _logical_box.max_y - _logical_box.min_y);
    const double scale_x = static_cast<double>(_width) / logical_w;
    const double scale_y = static_cast<double>(_height) / logical_h;
    _scale = std::min(scale_x, scale_y);

    const double used_w = logical_w * _scale;
    const double used_h = logical_h * _scale;
    _offset_x = (static_cast<double>(_width) - used_w) * 0.5;
    _offset_y = (static_cast<double>(_height) - used_h) * 0.5;

    _pixels.resize(static_cast<std::size_t>(_width) * static_cast<std::size_t>(_height));
    clear({255, 255, 255, 255});
}

PointD RasterCanvas::to_pixel(PointD p) const
{
    const double x = _offset_x + (p.x - _logical_box.min_x) * _scale;
    const double y = _offset_y + (_logical_box.max_y - p.y) * _scale;
    return {x, y};
}

void RasterCanvas::clear(Rgba c)
{
    std::fill(_pixels.begin(), _pixels.end(), c);
}

void RasterCanvas::set_pixel(int x, int y, Rgba color)
{
    if (x < 0 || y < 0 || x >= _width || y >= _height)
        return;
    if (_has_clip && (x < _clip_x0 || y < _clip_y0 || x > _clip_x1 || y > _clip_y1))
        return;
    _pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(_width) + static_cast<std::size_t>(x)] = color;
}

void RasterCanvas::set_clip(const BoxD& logical_rect)
{
    if (!logical_rect.valid())
    {
        clear_clip();
        return;
    }
    const auto a = to_pixel({ logical_rect.min_x, logical_rect.max_y });
    const auto b = to_pixel({ logical_rect.max_x, logical_rect.min_y });
    _clip_x0 = clamp_int(static_cast<int>(std::floor(std::min(a.x, b.x))), 0, _width - 1);
    _clip_x1 = clamp_int(static_cast<int>(std::ceil(std::max(a.x, b.x))), 0, _width - 1);
    _clip_y0 = clamp_int(static_cast<int>(std::floor(std::min(a.y, b.y))), 0, _height - 1);
    _clip_y1 = clamp_int(static_cast<int>(std::ceil(std::max(a.y, b.y))), 0, _height - 1);
    _has_clip = true;
}

void RasterCanvas::clear_clip()
{
    _has_clip = false;
}

void RasterCanvas::draw_disc(int cx, int cy, int radius, Rgba color)
{
    radius = std::max(0, radius);
    for (int y = cy - radius; y <= cy + radius; ++y)
    {
        for (int x = cx - radius; x <= cx + radius; ++x)
        {
            const int dx = x - cx;
            const int dy = y - cy;
            if (dx * dx + dy * dy <= radius * radius)
                set_pixel(x, y, color);
        }
    }
}

void RasterCanvas::draw_line(PointD a, PointD b, Rgba color, int thickness)
{
    const auto pa = to_pixel(a);
    const auto pb = to_pixel(b);
    int x0 = static_cast<int>(std::llround(pa.x));
    int y0 = static_cast<int>(std::llround(pa.y));
    int x1 = static_cast<int>(std::llround(pb.x));
    int y1 = static_cast<int>(std::llround(pb.y));

    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    const int radius = std::max(0, thickness / 2);

    for (;;)
    {
        draw_disc(x0, y0, radius, color);
        if (x0 == x1 && y0 == y1)
            break;
        const int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

void RasterCanvas::draw_polyline(const std::vector<PointD>& pts, Rgba color, int thickness, bool closed)
{
    if (pts.size() < 2)
        return;
    for (std::size_t i = 1; i < pts.size(); ++i)
        draw_line(pts[i - 1], pts[i], color, thickness);
    if (closed)
        draw_line(pts.back(), pts.front(), color, thickness);
}

void RasterCanvas::draw_dashed_polyline(const std::vector<PointD>& pts, Rgba color, int thickness,
                                        double dash_on_px, double dash_off_px, bool closed)
{
    if (pts.size() < 2)
        return;
    if (dash_on_px <= 0.0)
    {
        draw_polyline(pts, color, thickness, closed);
        return;
    }
    const double off = std::max(0.0, dash_off_px);
    const double period = dash_on_px + off;
    const int radius = std::max(0, thickness / 2);

    std::vector<PointD> px;
    px.reserve(pts.size());
    for (const auto& p : pts)
        px.push_back(to_pixel(p));
    if (closed)
        px.push_back(px.front());

    double phase = 0.0; // distance into the current period, carried across segments
    for (std::size_t i = 1; i < px.size(); ++i)
    {
        const double x0 = px[i - 1].x, y0 = px[i - 1].y;
        const double x1 = px[i].x, y1 = px[i].y;
        const double seg_len = std::hypot(x1 - x0, y1 - y0);
        if (seg_len < 1e-9)
            continue;
        const double ux = (x1 - x0) / seg_len;
        const double uy = (y1 - y0) / seg_len;
        for (double d = 0.0; d <= seg_len; d += 1.0)
        {
            const double pos = std::fmod(phase + d, period);
            if (pos < dash_on_px)
                draw_disc(static_cast<int>(std::llround(x0 + ux * d)),
                          static_cast<int>(std::llround(y0 + uy * d)), radius, color);
        }
        phase = std::fmod(phase + seg_len, period);
    }
}

void RasterCanvas::fill_polygon(const std::vector<PointD>& pts, Rgba color)
{
    if (pts.size() < 3)
        return;

    std::vector<PointD> p;
    p.reserve(pts.size());
    for (auto v : pts)
        p.push_back(to_pixel(v));

    double min_y = p[0].y;
    double max_y = p[0].y;
    for (const auto& v : p)
    {
        min_y = std::min(min_y, v.y);
        max_y = std::max(max_y, v.y);
    }

    const int y0 = clamp_int(static_cast<int>(std::floor(min_y)), 0, _height - 1);
    const int y1 = clamp_int(static_cast<int>(std::ceil(max_y)), 0, _height - 1);
    std::vector<double> nodes;

    for (int y = y0; y <= y1; ++y)
    {
        nodes.clear();
        const double scan_y = static_cast<double>(y) + 0.5;
        for (std::size_t i = 0, j = p.size() - 1; i < p.size(); j = i++)
        {
            const auto& pi = p[i];
            const auto& pj = p[j];
            if ((pi.y < scan_y && pj.y >= scan_y) || (pj.y < scan_y && pi.y >= scan_y))
            {
                const double x = pi.x + (scan_y - pi.y) / (pj.y - pi.y) * (pj.x - pi.x);
                nodes.push_back(x);
            }
        }
        std::sort(nodes.begin(), nodes.end());
        for (std::size_t k = 0; k + 1 < nodes.size(); k += 2)
        {
            int x_start = clamp_int(static_cast<int>(std::ceil(nodes[k])), 0, _width - 1);
            int x_end = clamp_int(static_cast<int>(std::floor(nodes[k + 1])), 0, _width - 1);
            for (int x = x_start; x <= x_end; ++x)
                set_pixel(x, y, color);
        }
    }
}

void RasterCanvas::draw_ellipse(PointD center, double radius_x, double radius_y, double tilt_rad, Rgba color, int thickness)
{
    std::vector<PointD> pts;
    pts.reserve(145);
    const double cos_t = std::cos(tilt_rad);
    const double sin_t = std::sin(tilt_rad);
    for (int i = 0; i <= 144; ++i)
    {
        const double t = 2.0 * kPi * static_cast<double>(i) / 144.0;
        const double x = radius_x * std::cos(t);
        const double y = radius_y * std::sin(t);
        pts.push_back({ center.x + x * cos_t - y * sin_t, center.y + x * sin_t + y * cos_t });
    }
    draw_polyline(pts, color, thickness, false);
}

void RasterCanvas::fill_ellipse(PointD center, double radius_x, double radius_y, double tilt_rad, Rgba color)
{
    std::vector<PointD> pts;
    pts.reserve(144);
    const double cos_t = std::cos(tilt_rad);
    const double sin_t = std::sin(tilt_rad);
    for (int i = 0; i < 144; ++i)
    {
        const double t = 2.0 * kPi * static_cast<double>(i) / 144.0;
        const double x = radius_x * std::cos(t);
        const double y = radius_y * std::sin(t);
        pts.push_back({ center.x + x * cos_t - y * sin_t, center.y + x * sin_t + y * cos_t });
    }
    fill_polygon(pts, color);
}

void RasterCanvas::draw_image(const BoxD& logical_rect,
                             int src_w,
                             int src_h,
                             const std::vector<Rgba>& src_pixels)
{
    if (src_w <= 0 || src_h <= 0 ||
        src_pixels.size() < static_cast<std::size_t>(src_w) * static_cast<std::size_t>(src_h) ||
        !logical_rect.valid())
        return;

    const auto top_left = to_pixel({ logical_rect.min_x, logical_rect.max_y });
    const auto bottom_right = to_pixel({ logical_rect.max_x, logical_rect.min_y });

    const int px0 = clamp_int(static_cast<int>(std::floor(std::min(top_left.x, bottom_right.x))), 0, _width - 1);
    const int px1 = clamp_int(static_cast<int>(std::ceil(std::max(top_left.x, bottom_right.x))), 0, _width - 1);
    const int py0 = clamp_int(static_cast<int>(std::floor(std::min(top_left.y, bottom_right.y))), 0, _height - 1);
    const int py1 = clamp_int(static_cast<int>(std::ceil(std::max(top_left.y, bottom_right.y))), 0, _height - 1);

    const double dst_w = std::max(1, px1 - px0);
    const double dst_h = std::max(1, py1 - py0);

    for (int y = py0; y <= py1; ++y)
    {
        const double v = (dst_h <= 0) ? 0.0 : static_cast<double>(y - py0) / dst_h;
        const int sy = clamp_int(static_cast<int>(v * src_h), 0, src_h - 1);
        for (int x = px0; x <= px1; ++x)
        {
            const double u = (dst_w <= 0) ? 0.0 : static_cast<double>(x - px0) / dst_w;
            const int sx = clamp_int(static_cast<int>(u * src_w), 0, src_w - 1);
            const Rgba& c = src_pixels[static_cast<std::size_t>(sy) * static_cast<std::size_t>(src_w) + static_cast<std::size_t>(sx)];
            if (c.a == 0)
                continue;
            set_pixel(x, y, c);
        }
    }
}

void RasterCanvas::draw_text_marker(PointD position, int glyph_count, Rgba color, int thickness)
{
    const int len = std::max(1, glyph_count);
    const double logical_w = (_logical_box.max_x - _logical_box.min_x) / std::max(50.0, static_cast<double>(_width) / 12.0) * len;
    const double logical_h = (_logical_box.max_y - _logical_box.min_y) / 120.0;
    std::vector<PointD> box = {
        position,
        {position.x + logical_w, position.y},
        {position.x + logical_w, position.y + logical_h},
        {position.x, position.y + logical_h}
    };
    draw_polyline(box, color, std::max(1, thickness), true);
}

} // namespace zs::dwf::native_render
