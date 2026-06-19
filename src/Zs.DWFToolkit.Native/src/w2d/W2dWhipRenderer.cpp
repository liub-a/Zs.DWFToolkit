#include "W2dWhipRenderer.h"
#include "../render/RasterCanvas.h"
#include "../render/MinimalPng.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef ZS_DWF_WITH_ODA_DWFTK
#include "whiptk/whip_toolkit.h"
#endif

namespace zs::dwf::w2d
{
using namespace zs::dwf::native_render;

namespace
{
    std::string json_escape(const std::string& value)
    {
        std::string out;
        out.reserve(value.size() + 8);
        for (char ch : value)
        {
            switch (ch)
            {
                case '\\': out += "\\\\"; break;
                case '"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += ch; break;
            }
        }
        return out;
    }

#ifdef ZS_DWF_WITH_ODA_DWFTK
    struct W2dContext
    {
        bool collecting{true};
        BoxD bounds;
        RasterCanvas* canvas{nullptr};
        int polyline_count{0};
        int polygon_count{0};
        int outline_ellipse_count{0};
        int filled_ellipse_count{0};
        int text_count{0};
        int unsupported_count{0};
    };

    W2dContext* ctx(WT_File& file)
    {
        return static_cast<W2dContext*>(file.heuristics().user_data());
    }

    Rgba current_color(WT_File& file)
    {
        const WT_RGBA32 rgba = file.rendition().color().rgba();
        return { rgba.m_rgb.r, rgba.m_rgb.g, rgba.m_rgb.b, rgba.m_rgb.a };
    }

    int current_thickness(WT_File& file, const RasterCanvas* canvas)
    {
        const int w = file.rendition().line_weight().weight_value();
        if (w <= 0 || canvas == nullptr)
            return 1;
        return std::max(1, std::min(24, static_cast<int>(std::llround(static_cast<double>(w) * canvas->scale()))));
    }

    std::vector<PointD> points_from_set(const WT_Point_Set& set)
    {
        std::vector<PointD> pts;
        pts.reserve(static_cast<std::size_t>(std::max(0, set.count())));
        const WT_Logical_Point* p = set.points();
        for (int i = 0; i < set.count(); ++i)
            pts.push_back({ static_cast<double>(p[i].m_x), static_cast<double>(p[i].m_y) });
        return pts;
    }

    void include_points(BoxD& b, const std::vector<PointD>& pts)
    {
        for (const auto& p : pts)
            b.include(p.x, p.y);
    }

    WT_Result on_polyline(WT_Polyline& item, WT_File& file)
    {
        W2dContext* c = ctx(file);
        if (!c) return WT_Result::Internal_Error;
        c->polyline_count++;
        const auto pts = points_from_set(item);
        if (c->collecting)
        {
            include_points(c->bounds, pts);
        }
        else if (c->canvas)
        {
            c->canvas->draw_polyline(pts, current_color(file), current_thickness(file, c->canvas));
        }
        return WT_Result::Success;
    }

    WT_Result on_polygon(WT_Polygon& item, WT_File& file)
    {
        W2dContext* c = ctx(file);
        if (!c) return WT_Result::Internal_Error;
        c->polygon_count++;
        const auto pts = points_from_set(item);
        if (c->collecting)
        {
            include_points(c->bounds, pts);
        }
        else if (c->canvas)
        {
            const auto color = current_color(file);
            c->canvas->fill_polygon(pts, color);
            c->canvas->draw_polyline(pts, color, current_thickness(file, c->canvas), true);
        }
        return WT_Result::Success;
    }

    void include_ellipse(BoxD& b, const WT_Ellipse& e)
    {
        const double cx = static_cast<double>(e.position().m_x);
        const double cy = static_cast<double>(e.position().m_y);
        const double r = static_cast<double>(std::max(std::abs(e.major()), std::abs(e.minor())));
        b.include(cx - r, cy - r);
        b.include(cx + r, cy + r);
    }

    WT_Result on_outline_ellipse(WT_Outline_Ellipse& item, WT_File& file)
    {
        W2dContext* c = ctx(file);
        if (!c) return WT_Result::Internal_Error;
        c->outline_ellipse_count++;
        if (c->collecting)
        {
            include_ellipse(c->bounds, item);
        }
        else if (c->canvas)
        {
            c->canvas->draw_ellipse(
                { static_cast<double>(item.position().m_x), static_cast<double>(item.position().m_y) },
                static_cast<double>(std::abs(item.major())),
                static_cast<double>(std::abs(item.minor())),
                item.tilt_radian(),
                current_color(file),
                current_thickness(file, c->canvas));
        }
        return WT_Result::Success;
    }

    WT_Result on_filled_ellipse(WT_Filled_Ellipse& item, WT_File& file)
    {
        W2dContext* c = ctx(file);
        if (!c) return WT_Result::Internal_Error;
        c->filled_ellipse_count++;
        if (c->collecting)
        {
            include_ellipse(c->bounds, item);
        }
        else if (c->canvas)
        {
            c->canvas->fill_ellipse(
                { static_cast<double>(item.position().m_x), static_cast<double>(item.position().m_y) },
                static_cast<double>(std::abs(item.major())),
                static_cast<double>(std::abs(item.minor())),
                item.tilt_radian(),
                current_color(file));
        }
        return WT_Result::Success;
    }

    WT_Result on_text(WT_Text& item, WT_File& file)
    {
        W2dContext* c = ctx(file);
        if (!c) return WT_Result::Internal_Error;
        c->text_count++;
        const PointD p{ static_cast<double>(item.position().m_x), static_cast<double>(item.position().m_y) };
        const int glyphs = std::max(1, item.string().length());
        if (c->collecting)
        {
            c->bounds.include(p.x, p.y);
            c->bounds.include(p.x + glyphs * 64.0, p.y + 128.0);
        }
        else if (c->canvas)
        {
            c->canvas->draw_text_marker(p, glyphs, current_color(file), current_thickness(file, c->canvas));
        }
        return WT_Result::Success;
    }

    WT_Result on_view(WT_View& item, WT_File& file)
    {
        W2dContext* c = ctx(file);
        if (c && c->collecting)
        {
            const auto& v = item.view();
            c->bounds.include(v.m_min.m_x, v.m_min.m_y);
            c->bounds.include(v.m_max.m_x, v.m_max.m_y);
        }
        return WT_View::default_process(item, file);
    }

    WT_Result process_w2d(const std::string& path, W2dContext& c)
    {
        WT_File file;
        file.set_filename(path.c_str());
        file.set_file_mode(WT_File::File_Read);
        file.heuristics().set_user_data(&c);
        file.set_polyline_action(on_polyline);
        file.set_polygon_action(on_polygon);
        file.set_outline_ellipse_action(on_outline_ellipse);
        file.set_filled_ellipse_action(on_filled_ellipse);
        file.set_text_action(on_text);
        file.set_view_action(on_view);

        WT_Result result = file.open();
        if (result != WT_Result::Success)
            return result;

        for (;;)
        {
            result = file.process_next_object();
            if (result == WT_Result::Success || result == WT_Result::Waiting_For_Data)
                continue;
            if (result == WT_Result::End_Of_DWF_Opcode_Found || result == WT_Result::End_Of_File_Error)
            {
                file.close();
                return WT_Result::Success;
            }
            file.close();
            return result;
        }
    }

    std::string wt_result_name(WT_Result r)
    {
        switch (static_cast<int>(r))
        {
            case static_cast<int>(WT_Result::Success): return "Success";
            case static_cast<int>(WT_Result::Corrupt_File_Error): return "Corrupt_File_Error";
            case static_cast<int>(WT_Result::End_Of_File_Error): return "End_Of_File_Error";
            case static_cast<int>(WT_Result::File_Open_Error): return "File_Open_Error";
            case static_cast<int>(WT_Result::Not_A_DWF_File_Error): return "Not_A_DWF_File_Error";
            case static_cast<int>(WT_Result::Unsupported_DWF_Opcode): return "Unsupported_DWF_Opcode";
            case static_cast<int>(WT_Result::Unsupported_DWF_Extension_Error): return "Unsupported_DWF_Extension_Error";
            case static_cast<int>(WT_Result::DWF_Package_Format): return "DWF_Package_Format";
            default: return "WT_Result_" + std::to_string(static_cast<int>(r));
        }
    }
#endif
}

RenderResult render_w2d_file_to_png(
    const std::string& w2d_path,
    const std::string& output_path,
    int page_index,
    int width_px,
    int height_px,
    int dpi)
{
#ifndef ZS_DWF_WITH_ODA_DWFTK
    (void)w2d_path;
    (void)output_path;
    (void)page_index;
    (void)width_px;
    (void)height_px;
    (void)dpi;
    return RenderResult{
        false,
        1007,
        "unsupported_dwf_rendering",
        "W2D rendering requires building native library with ZS_DWF_ENABLE_ODA_DWFTK=ON.",
        ""
    };
#else
    if (width_px <= 0 || height_px <= 0)
    {
        return RenderResult{false, 1001, "invalid_argument", "width_px and height_px must be positive", ""};
    }

    std::ifstream probe(w2d_path, std::ios::binary);
    if (!probe.good())
    {
        return RenderResult{false, 1002, "file_not_found", "W2D input file is not readable", ""};
    }

    W2dContext collector;
    collector.collecting = true;
    WT_Result result = process_w2d(w2d_path, collector);
    if (result != WT_Result::Success)
    {
        const std::string name = wt_result_name(result);
        return RenderResult{false, 1007, "w2d_parse_failed", "Failed to parse W2D stream: " + name, ""};
    }

    if (!collector.bounds.valid())
    {
        return RenderResult{false, 1007, "w2d_empty_bounds", "W2D stream did not produce a drawable/view bounding box", ""};
    }

    // Add a small logical margin so line weights at the extents are not clipped.
    const double dx = std::max(1.0, (collector.bounds.max_x - collector.bounds.min_x) * 0.01);
    const double dy = std::max(1.0, (collector.bounds.max_y - collector.bounds.min_y) * 0.01);
    collector.bounds.min_x -= dx;
    collector.bounds.max_x += dx;
    collector.bounds.min_y -= dy;
    collector.bounds.max_y += dy;

    RasterCanvas canvas(width_px, height_px, collector.bounds);
    W2dContext painter;
    painter.collecting = false;
    painter.bounds = collector.bounds;
    painter.canvas = &canvas;
    result = process_w2d(w2d_path, painter);
    if (result != WT_Result::Success)
    {
        const std::string name = wt_result_name(result);
        return RenderResult{false, 1007, "w2d_render_failed", "Failed to render W2D stream: " + name, ""};
    }

    std::string png_error;
    if (!write_png_rgba(output_path, width_px, height_px, canvas.pixels(), png_error))
    {
        return RenderResult{false, 1008, "output_failed", png_error, ""};
    }

    std::ostringstream json;
    json << "{"
         << "\"success\":true,"
         << "\"errorCode\":null,"
         << "\"errorMessage\":null,"
         << "\"sourcePath\":\"" << json_escape(w2d_path) << "\","
         << "\"pageIndex\":" << page_index << ","
         << "\"outputPath\":\"" << json_escape(output_path) << "\","
         << "\"outputFiles\":[\"" << json_escape(output_path) << "\"],"
         << "\"imageWidth\":" << width_px << ","
         << "\"imageHeight\":" << height_px << ","
         << "\"dpi\":" << dpi << ","
         << "\"toolName\":\"oda_whiptk_minimal_renderer\","
         << "\"w2dStats\":{"
         << "\"polyline\":" << painter.polyline_count << ","
         << "\"polygon\":" << painter.polygon_count << ","
         << "\"outlineEllipse\":" << painter.outline_ellipse_count << ","
         << "\"filledEllipse\":" << painter.filled_ellipse_count << ","
         << "\"text\":" << painter.text_count << "},"
         << "\"bounds\":{"
         << "\"minX\":" << collector.bounds.min_x << ","
         << "\"minY\":" << collector.bounds.min_y << ","
         << "\"maxX\":" << collector.bounds.max_x << ","
         << "\"maxY\":" << collector.bounds.max_y << "}"
         << "}";

    return RenderResult{true, 0, "", "", json.str()};
#endif
}

} // namespace zs::dwf::w2d
