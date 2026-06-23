#pragma once
#include "../render/ICanvas.h"
#include "MinimalPdf.h"
#include <string>
#include <vector>

namespace zs::dwf::pdf
{

using zs::dwf::native_render::BoxD;
using zs::dwf::native_render::ICanvas;
using zs::dwf::native_render::PointD;
using zs::dwf::native_render::Rgba;

// An ICanvas that emits a PDF content stream (vector operators) instead of pixels.
// Drawing-space coordinates are mapped to PDF user space (points, origin
// bottom-left, y up) so output is true vector — small and resolution-independent.
// Text is rendered as filled glyph outlines (FreeType) when available.
class PdfVectorCanvas : public ICanvas
{
public:
    // logical_box is the drawing bounding box; the page is page_w_pt x page_h_pt
    // points. The drawing is mapped to fill the page.
    PdfVectorCanvas(BoxD logical_box, double page_w_pt, double page_h_pt);
    ~PdfVectorCanvas() override;
    PdfVectorCanvas(const PdfVectorCanvas&) = delete;
    PdfVectorCanvas& operator=(const PdfVectorCanvas&) = delete;

    double page_width_pt() const { return _page_w; }
    double page_height_pt() const { return _page_h; }

    // Finalizes (closes any open clip) and returns the page content + images.
    PdfPage take_page();

    double scale() const override { return _scale; }

    void draw_polyline(const std::vector<PointD>& pts, Rgba color, int thickness, bool closed = false) override;
    void draw_dashed_polyline(const std::vector<PointD>& pts, Rgba color, int thickness,
                              double dash_on, double dash_off, bool closed = false) override;
    void fill_polygon(const std::vector<PointD>& pts, Rgba color) override;
    void hatch_polygon(const std::vector<PointD>& pts, Rgba color, int spacing, bool back_diagonal = false) override;
    void fill_contours(const std::vector<std::vector<PointD>>& contours, Rgba color) override;
    void draw_ellipse(PointD center, double radius_x, double radius_y, double tilt_rad, Rgba color, int thickness) override;
    void fill_ellipse(PointD center, double radius_x, double radius_y, double tilt_rad, Rgba color) override;
    void draw_text_marker(PointD position, int glyph_count, Rgba color, int thickness) override;
    void fill_marker(PointD center, int radius, Rgba color) override;
    void draw_image(const BoxD& logical_rect, int src_w, int src_h, const std::vector<Rgba>& src_pixels) override;
    void set_clip(const BoxD& logical_rect) override;
    void set_clip_contours(const std::vector<std::vector<PointD>>& contours) override;
    void clear_clip() override;
    bool draw_text(const unsigned short* codepoints, int count, PointD position_logical,
                   double height_units, double rotation_deg, Rgba color) override;

private:
    PointD map(PointD p) const; // drawing coords -> PDF points
    void emit_stroke_state(Rgba color, int thickness);
    void emit_fill_color(Rgba color);
    void ellipse_path(PointD center, double rx, double ry, double tilt_rad); // appends bezier path (mapped)

    BoxD _box;
    double _page_w{0};
    double _page_h{0};
    double _sx{1};
    double _sy{1};
    double _scale{1};
    bool _clip_open{false};
    std::string _content;
    std::vector<PdfImage> _images;

    bool _ft_ready{false};
    void* _ft_library{nullptr};
    void* _ft_face{nullptr};
    void* _ft_fallback{nullptr};
};

} // namespace zs::dwf::pdf
