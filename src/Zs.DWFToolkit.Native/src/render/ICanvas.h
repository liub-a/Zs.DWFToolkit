#pragma once
#include "MinimalPng.h" // Rgba
#include <algorithm>
#include <limits>
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

// Abstract drawing sink for the W2D opcode handlers in W2dWhipRenderer. Geometry is
// passed in logical (drawing) coordinates; the concrete canvas maps it to its own
// output space (pixels for RasterCanvas, PDF points for PdfVectorCanvas). Line
// weights / dash lengths / hatch spacing arrive pre-scaled by scale() at the call
// sites, so a sink's scale() defines its output-units-per-drawing-unit factor.
class ICanvas
{
public:
    virtual ~ICanvas() = default;

    // Output units per drawing unit (pixels/unit for raster, points/unit for PDF).
    virtual double scale() const = 0;

    virtual void draw_polyline(const std::vector<PointD>& pts, Rgba color, int thickness, bool closed = false) = 0;
    virtual void draw_dashed_polyline(const std::vector<PointD>& pts, Rgba color, int thickness,
                                      double dash_on, double dash_off, bool closed = false) = 0;
    virtual void fill_polygon(const std::vector<PointD>& pts, Rgba color) = 0;
    virtual void hatch_polygon(const std::vector<PointD>& pts, Rgba color, int spacing, bool back_diagonal = false) = 0;
    virtual void fill_contours(const std::vector<std::vector<PointD>>& contours, Rgba color) = 0;
    virtual void draw_ellipse(PointD center, double radius_x, double radius_y, double tilt_rad, Rgba color, int thickness) = 0;
    virtual void fill_ellipse(PointD center, double radius_x, double radius_y, double tilt_rad, Rgba color) = 0;
    virtual void draw_text_marker(PointD position, int glyph_count, Rgba color, int thickness) = 0;
    virtual void fill_marker(PointD center, int radius, Rgba color) = 0;
    virtual void draw_image(const BoxD& logical_rect, int src_w, int src_h, const std::vector<Rgba>& src_pixels) = 0;
    virtual void set_clip(const BoxD& logical_rect) = 0;
    virtual void set_clip_contours(const std::vector<std::vector<PointD>>& contours) = 0;
    virtual void clear_clip() = 0;

    // Renders Unicode text (UTF-16 code units) with its baseline at the logical
    // position, at height_units drawing units, rotated rotation_deg CCW. Returns
    // false if text could not be rendered (caller falls back to a marker).
    virtual bool draw_text(const unsigned short* codepoints, int count, PointD position_logical,
                           double height_units, double rotation_deg, Rgba color) = 0;
};

} // namespace zs::dwf::native_render
