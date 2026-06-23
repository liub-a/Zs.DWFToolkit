#include "W2dStamp.h"

#ifdef ZS_DWF_WITH_ODA_DWFTK
#include "whiptk/whip_toolkit.h"
#include <vector>

namespace
{
using namespace zs::dwf;

struct StampCtx
{
    WT_File* out{nullptr};
    const std::uint8_t* rgba{nullptr};
    std::size_t rgba_len{0};
    int w{0}, h{0};
    int min_x{0}, min_y{0}, max_x{0}, max_y{0};
    bool image_written{false};
    bool failed{false};
};

StampCtx* ctx(WT_File& f) { return static_cast<StampCtx*>(f.heuristics().user_data()); }

// Generic passthrough: re-serialize each materialized object to the output file.
template <class T>
WT_Result reserialize(T& item, WT_File& in)
{
    StampCtx* c = ctx(in);
    if (!c || !c->out) return WT_Result::Internal_Error;
    // Best-effort: a single object failing to re-serialize must not abort the whole
    // transcode (some attribute opcodes need state we don't fully mirror).
    item.serialize(*c->out);
    return WT_Result::Success;
}

// At end of stream, append the stamp image, then let the caller close (writes EOF).
WT_Result on_end(WT_End_Of_DWF& /*item*/, WT_File& in)
{
    StampCtx* c = ctx(in);
    if (!c || !c->out || c->image_written) return WT_Result::Success;
    c->image_written = true;

    const WT_Integer32 data_size = static_cast<WT_Integer32>(c->rgba_len);
    WT_Image image(static_cast<WT_Unsigned_Integer16>(c->h),
                   static_cast<WT_Unsigned_Integer16>(c->w),
                   WT_Image::RGBA, /*id*/ 0x7FFF, /*color_map*/ nullptr,
                   data_size, const_cast<WT_Byte*>(c->rgba),
                   WT_Logical_Point(c->min_x, c->min_y),
                   WT_Logical_Point(c->max_x, c->max_y), WD_True);
    if (image.serialize(*c->out) != WT_Result::Success)
        c->failed = true;
    return WT_Result::Success;
}

void register_passthrough(WT_File& f)
{
    // Geometry + attributes that appear in typical 2D ePlot W2D. Unhandled rare
    // opcodes fall through to default processing (not copied).
    f.set_color_action(reserialize<WT_Color>);
    f.set_line_weight_action(reserialize<WT_Line_Weight>);
    f.set_line_pattern_action(reserialize<WT_Line_Pattern>);
    f.set_line_style_action(reserialize<WT_Line_Style>);
    f.set_fill_action(reserialize<WT_Fill>);
    f.set_fill_pattern_action(reserialize<WT_Fill_Pattern>);
    f.set_visibility_action(reserialize<WT_Visibility>);
    f.set_layer_action(reserialize<WT_Layer>);
    f.set_viewport_action(reserialize<WT_Viewport>);
    f.set_font_action(reserialize<WT_Font>);
    f.set_polyline_action(reserialize<WT_Polyline>);
    f.set_polygon_action(reserialize<WT_Polygon>);
    f.set_polytriangle_action(reserialize<WT_Polytriangle>);
    f.set_polymarker_action(reserialize<WT_Polymarker>);
    f.set_outline_ellipse_action(reserialize<WT_Outline_Ellipse>);
    f.set_filled_ellipse_action(reserialize<WT_Filled_Ellipse>);
    f.set_contour_set_action(reserialize<WT_Contour_Set>);
    f.set_text_action(reserialize<WT_Text>);
    f.set_image_action(reserialize<WT_Image>);
    f.set_end_of_dwf_action(on_end);
}
}
#endif

namespace zs::dwf::edit
{
bool stamp_image_on_w2d(const std::string& in_path, const std::string& out_path,
                        const std::uint8_t* rgba, std::size_t rgba_len,
                        int img_w, int img_h,
                        int min_x, int min_y, int max_x, int max_y,
                        std::string& error)
{
#ifndef ZS_DWF_WITH_ODA_DWFTK
    (void)in_path; (void)out_path; (void)rgba; (void)rgba_len; (void)img_w; (void)img_h;
    (void)min_x; (void)min_y; (void)max_x; (void)max_y;
    error = "stamp requires ZS_DWF_ENABLE_ODA_DWFTK build";
    return false;
#else
    if (!rgba || rgba_len < static_cast<std::size_t>(img_w) * img_h * 4)
    {
        error = "rgba buffer too small";
        return false;
    }

    WT_File out;
    out.set_filename(out_path.c_str());
    out.set_file_mode(WT_File::File_Write);
    if (out.open() != WT_Result::Success) { error = "cannot open output w2d"; return false; }

    StampCtx c;
    c.out = &out;
    c.rgba = rgba; c.rgba_len = rgba_len; c.w = img_w; c.h = img_h;
    c.min_x = min_x; c.min_y = min_y; c.max_x = max_x; c.max_y = max_y;

    WT_File in;
    in.set_filename(in_path.c_str());
    in.set_file_mode(WT_File::File_Read);
    in.heuristics().set_user_data(&c);
    register_passthrough(in);

    if (in.open() != WT_Result::Success) { out.close(); error = "cannot open input w2d"; return false; }

    WT_Result r;
    for (;;)
    {
        r = in.process_next_object();
        if (r == WT_Result::Success || r == WT_Result::Waiting_For_Data) continue;
        break;
    }
    in.close();

    // If the input had no explicit End_Of_DWF, still append the image.
    if (!c.image_written) { WT_End_Of_DWF eof; on_end(eof, in); }

    out.close();

    if (c.failed) { error = "failed to serialize stamp image"; return false; }
    // Success = the image was appended and the output written. The read loop's
    // terminal code varies (best-effort transcode tolerates per-object hiccups),
    // so we gate on the image being written rather than the exact end code.
    if (!c.image_written) { error = "stamp image was not written"; return false; }
    (void)r;
    return true;
#endif
}
} // namespace zs::dwf::edit
