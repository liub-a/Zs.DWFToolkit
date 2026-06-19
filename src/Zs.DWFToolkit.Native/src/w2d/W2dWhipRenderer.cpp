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
#include "../text/TextRenderer.h"
#include "../image/Group4Decoder.h"
#include <csetjmp>
#include <cstdio>
extern "C" {
#include <jpeglib.h>
}
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
        int image_count{0};
        int polytriangle_count{0};
        int contour_set_count{0};
        int unsupported_count{0};
        zs::dwf::text::TextRenderer* text{nullptr};
    };

    W2dContext* ctx(WT_File& file)
    {
        return static_cast<W2dContext*>(file.heuristics().user_data());
    }

    // The current rendition visibility, updated by WT_Visibility/WT_Layer opcodes
    // via their default processing. Invisible geometry (e.g. hidden layers) is
    // skipped during the painting pass.
    bool is_visible(WT_File& file)
    {
        return file.rendition().visibility().visible() != 0;
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

    // Maps the current line pattern to dash on/off lengths in pixels (scaled to the
    // canvas). Returns {0,0} for a solid line. First-pass approximation: a couple of
    // representative dash/dot cadences rather than the exact WHIP pattern tables.
    std::pair<double, double> current_dash(WT_File& file, const RasterCanvas* canvas)
    {
        const double s = (canvas ? canvas->scale() : 1.0);
        const double unit = std::max(1.0, 6.0 * s);
        switch (file.rendition().line_pattern().pattern_id())
        {
            case WT_Line_Pattern::Solid:
                return {0.0, 0.0};
            case WT_Line_Pattern::Dotted:
                return {std::max(1.0, unit * 0.25), unit};
            default: // Dashed and the various long/short dash variants
                return {unit * 2.0, unit};
        }
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
        else if (c->canvas && is_visible(file))
        {
            const auto [on, off] = current_dash(file, c->canvas);
            const int thick = current_thickness(file, c->canvas);
            if (on > 0.0)
                c->canvas->draw_dashed_polyline(pts, current_color(file), thick, on, off);
            else
                c->canvas->draw_polyline(pts, current_color(file), thick);
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
        else if (c->canvas && is_visible(file))
        {
            const auto color = current_color(file);
            // A non-solid fill pattern is rendered as a diagonal hatch; otherwise
            // the interior is solid-filled.
            const WT_Fill_Pattern::WT_Pattern_ID fp = file.rendition().fill_pattern().pattern_id();
            if (fp != WT_Fill_Pattern::Solid && fp != WT_Fill_Pattern::Illegal)
            {
                const int spacing = std::max(4, static_cast<int>(std::llround(8.0 * c->canvas->scale())));
                c->canvas->hatch_polygon(pts, color, spacing);
            }
            else
            {
                c->canvas->fill_polygon(pts, color);
            }
            c->canvas->draw_polyline(pts, color, current_thickness(file, c->canvas), true);
        }
        return WT_Result::Success;
    }

    WT_Result on_contour_set(WT_Contour_Set& item, WT_File& file)
    {
        W2dContext* c = ctx(file);
        if (!c) return WT_Result::Internal_Error;
        c->contour_set_count++;

        // Slice the flat point array into per-contour rings using the counts list.
        std::vector<std::vector<PointD>> rings;
        const WT_Logical_Point* pts = item.points();
        const WT_Integer32* counts = item.counts();
        if (pts && counts)
        {
            int idx = 0;
            for (int ci = 0; ci < item.contours(); ++ci)
            {
                std::vector<PointD> ring;
                for (int k = 0; k < counts[ci]; ++k, ++idx)
                    ring.push_back({ static_cast<double>(pts[idx].m_x), static_cast<double>(pts[idx].m_y) });
                rings.push_back(std::move(ring));
            }
        }

        if (c->collecting)
        {
            for (const auto& r : rings) include_points(c->bounds, r);
        }
        else if (c->canvas && is_visible(file))
        {
            c->canvas->fill_contours(rings, current_color(file));
        }
        return WT_Result::Success;
    }

    WT_Result on_polytriangle(WT_Polytriangle& item, WT_File& file)
    {
        W2dContext* c = ctx(file);
        if (!c) return WT_Result::Internal_Error;
        c->polytriangle_count++;
        const auto pts = points_from_set(item);
        if (c->collecting)
        {
            include_points(c->bounds, pts);
        }
        else if (c->canvas && is_visible(file))
        {
            // Points form a triangle strip: each new vertex closes a triangle with
            // the previous two.
            const auto color = current_color(file);
            for (std::size_t i = 2; i < pts.size(); ++i)
                c->canvas->fill_polygon({ pts[i - 2], pts[i - 1], pts[i] }, color);
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
        else if (c->canvas && is_visible(file))
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
        else if (c->canvas && is_visible(file))
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
        else if (c->canvas && is_visible(file))
        {
            const Rgba color = current_color(file);
            const auto pen = c->canvas->to_pixel(p);
            // Font height is in drawing units; map to pixels via the canvas scale.
            const int font_units = file.rendition().font().height().height();
            int px_height = static_cast<int>(std::llround(font_units * c->canvas->scale()));
            if (px_height < 6) px_height = 12;

            bool drawn = false;
            if (c->text && c->text->ready() && item.string().ascii())
            {
                drawn = c->text->draw(*c->canvas, item.string().ascii(),
                                      static_cast<int>(std::llround(pen.x)),
                                      static_cast<int>(std::llround(pen.y)),
                                      px_height, color);
            }
            if (!drawn)
                c->canvas->draw_text_marker(p, glyphs, color, current_thickness(file, c->canvas));
        }
        return WT_Result::Success;
    }

    struct JpegErrorMgr { jpeg_error_mgr base; std::jmp_buf jmp; };
    void jpeg_error_exit(j_common_ptr cinfo) { std::longjmp(reinterpret_cast<JpegErrorMgr*>(cinfo->err)->jmp, 1); }

    // Minimal in-memory libjpeg source manager (the bundled libjpeg predates
    // jpeg_mem_src, so we supply our own).
    void jpeg_mem_init(j_decompress_ptr) {}
    boolean jpeg_mem_fill(j_decompress_ptr cinfo)
    {
        // The whole buffer is provided up front; signal EOI on any further read.
        static const JOCTET eoi[2] = { 0xFF, JPEG_EOI };
        cinfo->src->next_input_byte = eoi;
        cinfo->src->bytes_in_buffer = 2;
        return TRUE;
    }
    void jpeg_mem_skip(j_decompress_ptr cinfo, long num_bytes)
    {
        if (num_bytes <= 0) return;
        if (static_cast<size_t>(num_bytes) > cinfo->src->bytes_in_buffer)
            num_bytes = static_cast<long>(cinfo->src->bytes_in_buffer);
        cinfo->src->next_input_byte += num_bytes;
        cinfo->src->bytes_in_buffer -= static_cast<size_t>(num_bytes);
    }
    void jpeg_mem_term(j_decompress_ptr) {}

    // Decodes a JPEG byte buffer into RGBA. Returns false on any libjpeg error.
    bool decode_jpeg(const WT_Byte* data, std::size_t size, std::vector<Rgba>& out, int& w, int& h)
    {
        jpeg_decompress_struct cinfo{};
        JpegErrorMgr err{};
        cinfo.err = jpeg_std_error(&err.base);
        err.base.error_exit = jpeg_error_exit;
        if (setjmp(err.jmp)) { jpeg_destroy_decompress(&cinfo); return false; }

        jpeg_create_decompress(&cinfo);
        jpeg_source_mgr src{};
        src.init_source = jpeg_mem_init;
        src.fill_input_buffer = jpeg_mem_fill;
        src.skip_input_data = jpeg_mem_skip;
        src.resync_to_restart = jpeg_resync_to_restart;
        src.term_source = jpeg_mem_term;
        src.next_input_byte = reinterpret_cast<const JOCTET*>(data);
        src.bytes_in_buffer = size;
        cinfo.src = &src;
        if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) { jpeg_destroy_decompress(&cinfo); return false; }
        cinfo.out_color_space = JCS_RGB;
        jpeg_start_decompress(&cinfo);

        w = static_cast<int>(cinfo.output_width);
        h = static_cast<int>(cinfo.output_height);
        const int comps = cinfo.output_components; // 3 for RGB
        out.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
        std::vector<unsigned char> scan(static_cast<std::size_t>(w) * static_cast<std::size_t>(comps));
        for (int y = 0; cinfo.output_scanline < cinfo.output_height; ++y)
        {
            unsigned char* rowp = scan.data();
            jpeg_read_scanlines(&cinfo, &rowp, 1);
            for (int x = 0; x < w; ++x)
            {
                const unsigned char* p = scan.data() + static_cast<std::size_t>(x) * comps;
                out[static_cast<std::size_t>(y) * w + x] = Rgba{ p[0], p[1], p[2], 255 };
            }
        }
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return true;
    }

    // Reads a WT_Image into RGBA, handling raw RGB/RGBA, color-mapped/indexed, and
    // JPEG. Returns false for formats we cannot decode (bitonal/Group3X/Group4),
    // so the caller falls back to a placeholder.
    bool image_to_rgba(const WT_Image& img, std::vector<Rgba>& out, int& w, int& h)
    {
        w = img.columns();
        h = img.rows();
        if (w <= 0 || h <= 0 || img.data() == nullptr)
            return false;

        const WT_Byte* data = img.data();
        // format() returns the raw WT_Byte opcode tag, not the enum type.
        const WT_Byte fmt = img.format();
        const std::size_t pixel_count = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
        const WT_Integer32 data_size = img.data_size();
        out.resize(pixel_count);

        if (fmt == static_cast<WT_Byte>(WT_Image::RGBA))
        {
            if (static_cast<std::size_t>(data_size) < pixel_count * 4)
                return false;
            for (std::size_t i = 0; i < pixel_count; ++i)
            {
                const WT_Byte* p = data + i * 4;
                out[i] = { p[0], p[1], p[2], p[3] };
            }
            return true;
        }
        if (fmt == static_cast<WT_Byte>(WT_Image::RGB))
        {
            if (static_cast<std::size_t>(data_size) < pixel_count * 3)
                return false;
            for (std::size_t i = 0; i < pixel_count; ++i)
            {
                const WT_Byte* p = data + i * 3;
                out[i] = { p[0], p[1], p[2], 255 };
            }
            return true;
        }
        if (fmt == static_cast<WT_Byte>(WT_Image::Mapped) ||
            fmt == static_cast<WT_Byte>(WT_Image::Indexed))
        {
            const WT_Color_Map* cm = img.color_map();
            if (!cm || static_cast<std::size_t>(data_size) < pixel_count)
                return false;
            for (std::size_t i = 0; i < pixel_count; ++i)
            {
                const WT_RGBA32 c = cm->map(data[i]);
                out[i] = { c.m_rgb.r, c.m_rgb.g, c.m_rgb.b, c.m_rgb.a };
            }
            return true;
        }
        if (fmt == static_cast<WT_Byte>(WT_Image::JPEG))
        {
            return decode_jpeg(data, static_cast<std::size_t>(data_size), out, w, h);
        }
        // Group4X fax: decode the bitstream to 1bpp indices, then map through the
        // 2-entry color map (requires ZS_DWF_WITH_TIFF; otherwise falls through).
        if (fmt == 0x0D) // WD_IMAGE_GROUP4X_MAPPED_EXT_OPCODE
        {
            const WT_Color_Map* cm = img.color_map();
            std::vector<std::uint8_t> idx;
            if (!cm || !zs::dwf::image::decode_group4(data, static_cast<std::size_t>(data_size), w, h, idx))
                return false;
            for (std::size_t i = 0; i < pixel_count && i < idx.size(); ++i)
            {
                const WT_RGBA32 c = cm->map(idx[i]);
                out[i] = { c.m_rgb.r, c.m_rgb.g, c.m_rgb.b, c.m_rgb.a };
            }
            return true;
        }
        // Bitonal/Group3X/Group4 fax encodings still need a decoder we do not ship.
        return false;
    }

    WT_Result on_image(WT_Image& item, WT_File& file)
    {
        W2dContext* c = ctx(file);
        if (!c) return WT_Result::Internal_Error;
        c->image_count++;

        BoxD rect;
        rect.include(static_cast<double>(item.min_corner().m_x), static_cast<double>(item.min_corner().m_y));
        rect.include(static_cast<double>(item.max_corner().m_x), static_cast<double>(item.max_corner().m_y));

        if (c->collecting)
        {
            c->bounds.include(rect);
            return WT_Result::Success;
        }
        if (!c->canvas || !is_visible(file))
            return WT_Result::Success;

        // Fax-coded images (bitonal / Group3X) are decompressed to an 8bpp mapped
        // image via the toolkit, then decoded through the mapped path below.
        const WT_Byte ifmt = item.format();
        if (ifmt == static_cast<WT_Byte>(WT_Image::Bitonal_Mapped))
            item.convert_bitonal_to_group_3X();
        if (item.format() == static_cast<WT_Byte>(WT_Image::Group3X_Mapped))
            item.convert_group_3X_to_mapped();

        std::vector<Rgba> pixels;
        int iw = 0, ih = 0;
        if (image_to_rgba(item, pixels, iw, ih))
        {
            c->canvas->draw_image(rect, iw, ih, pixels);
        }
        else
        {
            // Placeholder for formats we cannot decode yet (e.g. Group4X fax).
            c->unsupported_count++;
            std::vector<Rgba> gray(1, Rgba{ 220, 220, 220, 255 });
            c->canvas->draw_image(rect, 1, 1, gray);
        }
        return WT_Result::Success;
    }

    // Approximates the W2D viewport clip as the axis-aligned bounding box of its
    // boundary contour. Non-rectangular clips are over-inclusive (bbox), never
    // under-inclusive, so no in-bounds content is wrongly dropped.
    WT_Result on_viewport(WT_Viewport& item, WT_File& file)
    {
        W2dContext* c = ctx(file);
        if (!c || c->collecting || !c->canvas)
            return WT_Viewport::default_process(item, file);

        const WT_Contour_Set* contour = item.contour();
        if (!contour || contour->total_points() <= 0 || contour->points() == nullptr)
        {
            c->canvas->clear_clip();
            return WT_Viewport::default_process(item, file);
        }

        BoxD clip;
        const WT_Logical_Point* pts = contour->points();
        for (int i = 0; i < contour->total_points(); ++i)
            clip.include(static_cast<double>(pts[i].m_x), static_cast<double>(pts[i].m_y));
        c->canvas->set_clip(clip);
        return WT_Viewport::default_process(item, file);
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
        file.set_image_action(on_image);
        file.set_polytriangle_action(on_polytriangle);
        file.set_contour_set_action(on_contour_set);
        file.set_viewport_action(on_viewport);
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

    // Supersample: render into a 2x canvas and box-downsample for anti-aliasing.
    const int ss = 2;
    RasterCanvas canvas(width_px * ss, height_px * ss, collector.bounds);
    zs::dwf::text::TextRenderer text_renderer;
    W2dContext painter;
    painter.collecting = false;
    painter.bounds = collector.bounds;
    painter.canvas = &canvas;
    painter.text = &text_renderer;
    result = process_w2d(w2d_path, painter);
    if (result != WT_Result::Success)
    {
        const std::string name = wt_result_name(result);
        return RenderResult{false, 1007, "w2d_render_failed", "Failed to render W2D stream: " + name, ""};
    }

    const auto final_pixels = downsample_box(canvas.pixels(), width_px * ss, height_px * ss, ss);
    std::string png_error;
    if (!write_png_rgba(output_path, width_px, height_px, final_pixels, png_error))
    {
        return RenderResult{false, 1008, "output_failed", png_error, ""};
    }
    // Transform maps drawing coords to FINAL (downsampled) pixels.
    const double final_scale = canvas.scale() / ss;
    const double final_offx = canvas.offset_x() / ss;
    const double final_offy = canvas.offset_y() / ss;

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
         << "\"text\":" << painter.text_count << ","
         << "\"image\":" << painter.image_count << ","
         << "\"polytriangle\":" << painter.polytriangle_count << "},"
         << "\"transform\":{"
         << "\"pageWidth\":" << (collector.bounds.max_x - collector.bounds.min_x) << ","
         << "\"pageHeight\":" << (collector.bounds.max_y - collector.bounds.min_y) << ","
         << "\"unit\":\"dwf_internal\","
         << "\"imageWidth\":" << width_px << ","
         << "\"imageHeight\":" << height_px << ","
         << "\"scaleX\":" << final_scale << ","
         << "\"scaleY\":" << final_scale << ","
         << "\"offsetX\":" << final_offx << ","
         << "\"offsetY\":" << final_offy << ","
         << "\"flipY\":true},"
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
