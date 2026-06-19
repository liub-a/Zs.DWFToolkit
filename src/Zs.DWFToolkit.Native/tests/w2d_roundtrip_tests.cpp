// Integration test (ODA build only): writes a real W2D stream with the WHIP
// Toolkit, renders it through our C ABI, and checks a non-trivial PNG comes out.
// This exercises the actual WT_File parsing + W2dWhipRenderer path end to end.
#include "whiptk/whip_toolkit.h"
#include "zs_dwf_toolkit_native.h"
#include "render/MinimalPng.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using zs::dwf::native_render::Rgba;
using zs::dwf::native_render::read_png_rgba;

namespace
{
int g_failures = 0;
void expect(bool ok, const std::string& msg)
{
    if (!ok) { std::printf("FAIL: %s\n", msg.c_str()); ++g_failures; }
}

bool write_sample_w2d(const std::string& path)
{
    WT_File file;
    file.set_filename(path.c_str());
    file.set_file_mode(WT_File::File_Write);
    if (file.open() != WT_Result::Success)
        return false;

    // Draw in black; the default WHIP color is white and would be invisible on
    // the white canvas background.
    WT_Color black(0, 0, 0);
    if (black.serialize(file) != WT_Result::Success) { file.close(); return false; }

    // A diagonal polyline plus a triangle, in a 0..1000 logical space.
    WT_Logical_Point line[2] = { WT_Logical_Point(0, 0), WT_Logical_Point(1000, 1000) };
    WT_Polyline polyline(2, line, WD_True);
    if (polyline.serialize(file) != WT_Result::Success) { file.close(); return false; }

    WT_Logical_Point tri[3] = { WT_Logical_Point(200, 100), WT_Logical_Point(800, 100), WT_Logical_Point(500, 700) };
    WT_Polygon polygon(3, tri, WD_True);
    if (polygon.serialize(file) != WT_Result::Success) { file.close(); return false; }

    WT_Text text(WT_Logical_Point(120, 500), WT_String("ABC"));
    if (text.serialize(file) != WT_Result::Success) { file.close(); return false; }

    // A 2x2 color-mapped image (red/blue) in the lower-right, to exercise the
    // mapped-image decode path end to end.
    WT_RGBA32 palette[2] = { WT_RGBA32(255, 0, 0, 255), WT_RGBA32(0, 0, 255, 255) };
    WT_Color_Map cmap(2, palette, file);
    WT_Byte indices[4] = { 0, 1, 1, 0 };
    WT_Image image(2, 2, WT_Image::Mapped, 1, &cmap, 4, indices,
                   WT_Logical_Point(650, 50), WT_Logical_Point(950, 350), WD_True);
    if (image.serialize(file) != WT_Result::Success) { file.close(); return false; }

    return file.close() == WT_Result::Success;
}
}

int main()
{
    namespace fs = std::filesystem;
    const std::string w2d = (fs::temp_directory_path() / "zs_roundtrip.w2d").string();
    const std::string png = (fs::temp_directory_path() / "zs_roundtrip.png").string();
    std::error_code ec;
    fs::remove(w2d, ec);
    fs::remove(png, ec);

    if (!write_sample_w2d(w2d))
    {
        std::printf("FAIL: could not write sample W2D\n");
        return 1;
    }
    expect(fs::exists(w2d) && fs::file_size(w2d) > 0, "sample W2D written");

    std::vector<char> json(1 << 16, 0);
    const int rc = zs_w2d_render_file(w2d.c_str(), png.c_str(), 200, 200, 96, json.data(), static_cast<int>(json.size()));
    expect(rc == 0, std::string("zs_w2d_render_file returned 0 (got ") + std::to_string(rc) + ", err: " + zs_dwf_get_last_error() + ")");
    expect(fs::exists(png) && fs::file_size(png) > 100, "rendered PNG is non-trivial");

    const std::string j(json.data());
    expect(j.find("\"success\":true") != std::string::npos, "render JSON reports success");
    expect(j.find("\"polyline\":1") != std::string::npos, "render JSON counts the polyline");
    expect(j.find("\"transform\"") != std::string::npos, "render JSON includes a transform");

    // Pixel regression: decode the PNG and verify the drawing is actually painted
    // (guards against the white-on-white / blank-output class of bug) while the
    // corners stay background-white.
    int w = 0, h = 0;
    std::vector<Rgba> px;
    std::string perr;
    if (read_png_rgba(png, w, h, px, perr))
    {
        expect(w == 200 && h == 200, "decoded PNG is 200x200");
        int dark = 0;
        for (const auto& p : px)
            if (p.r < 64 && p.g < 64 && p.b < 64) ++dark;
        expect(dark > 100, std::string("rendered geometry produces black pixels (got ") + std::to_string(dark) + ")");
        const Rgba corner = px.front();
        expect(corner.r > 200 && corner.g > 200 && corner.b > 200, "top-left corner stays background white");

        // The mapped image (red/blue palette) should put saturated red or blue
        // somewhere on the canvas (proves mapped-image decode, not a gray placeholder).
        int colored = 0;
        for (const auto& p : px)
        {
            const bool red = p.r > 180 && p.g < 80 && p.b < 80;
            const bool blue = p.b > 180 && p.r < 80 && p.g < 80;
            if (red || blue) ++colored;
        }
        expect(colored > 0, std::string("mapped image decodes to palette colors (got ") + std::to_string(colored) + ")");
    }
    else
    {
        expect(false, std::string("read_png_rgba failed: ") + perr);
    }

    if (g_failures == 0) { std::printf("W2D round-trip test passed.\n"); return 0; }
    std::printf("%d W2D round-trip assertion(s) failed.\n", g_failures);
    return 1;
}
