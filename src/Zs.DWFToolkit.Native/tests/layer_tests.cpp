// Layer reading test (ODA build only): writes a W2D with WHIP layers (WT_Layer)
// using the toolkit, then reads them back via collect_w2d_file_layers and checks
// the names/numbers/order. Validates the layer-collection hook independent of any
// particular sample (many AutoCAD plate-plot DWFs carry no layers at all).
#include "whiptk/whip_toolkit.h"
#include "w2d/W2dWhipRenderer.h"
#include "render/MinimalPng.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
int g_failures = 0;
void expect(bool ok, const std::string& msg)
{
    if (!ok) { std::printf("FAIL: %s\n", msg.c_str()); ++g_failures; }
}

bool write_layered_w2d(const std::string& path)
{
    WT_File file;
    file.set_filename(path.c_str());
    file.set_file_mode(WT_File::File_Write);
    if (file.open() != WT_Result::Success)
        return false;

    WT_Color black(0, 0, 0);
    if (black.serialize(file) != WT_Result::Success) { file.close(); return false; }

    // Layer 10 "Walls" with a polyline.
    WT_Layer walls(file, 10, "Walls");
    if (walls.serialize(file) != WT_Result::Success) { file.close(); return false; }
    WT_Logical_Point l1[2] = { WT_Logical_Point(0, 0), WT_Logical_Point(1000, 1000) };
    WT_Polyline p1(2, l1, WD_True);
    if (p1.serialize(file) != WT_Result::Success) { file.close(); return false; }

    // Layer 20 with a Unicode (CJK) name + a polygon, to check UTF-8 round-trip.
    const WT_Unsigned_Integer16 cjk[] = { 0x6807, 0x6CE8, 0 }; // 标注
    WT_Layer dims(file, 20, cjk);
    if (dims.serialize(file) != WT_Result::Success) { file.close(); return false; }
    WT_Logical_Point tri[3] = { WT_Logical_Point(200, 100), WT_Logical_Point(800, 100), WT_Logical_Point(500, 700) };
    WT_Polygon poly(3, tri, WD_True);
    if (poly.serialize(file) != WT_Result::Success) { file.close(); return false; }

    file.close();
    return true;
}
}

int main()
{
    const std::string w2d = (std::filesystem::temp_directory_path() / "zs_layer_test.w2d").string();
    if (!write_layered_w2d(w2d)) { std::printf("FAIL: could not write layered W2D\n"); return 1; }

    std::vector<zs::dwf::w2d::W2dLayer> layers;
    const auto rr = zs::dwf::w2d::collect_w2d_file_layers(w2d, layers);
    expect(rr.success, "collect_w2d_file_layers should succeed");
    expect(layers.size() == 2, "expected 2 layers, got " + std::to_string(layers.size()));
    if (layers.size() == 2)
    {
        expect(layers[0].number == 10, "layer[0] number == 10");
        expect(layers[0].name == "Walls", "layer[0] name == Walls, got '" + layers[0].name + "'");
        expect(layers[1].number == 20, "layer[1] number == 20");
        // 标注 = E6 A0 87 E6 B3 A8 in UTF-8.
        expect(layers[1].name == "\xE6\xA0\x87\xE6\xB3\xA8", "layer[1] name CJK UTF-8 round-trip");
    }

    // Phase 2: hiding layer 10 (the polyline) should remove ink vs the full render.
    auto count_nonwhite = [](const std::string& png) -> long {
        int w = 0, h = 0; std::vector<zs::dwf::native_render::Rgba> px; std::string e;
        if (!zs::dwf::native_render::read_png_rgba(png, w, h, px, e)) return -1;
        long n = 0; for (const auto& p : px) if (!(p.r == 255 && p.g == 255 && p.b == 255)) ++n;
        return n;
    };
    const std::string pngAll = (std::filesystem::temp_directory_path() / "zs_layer_all.png").string();
    const std::string pngHid = (std::filesystem::temp_directory_path() / "zs_layer_hid.png").string();
    const auto rAll = zs::dwf::w2d::render_w2d_file_to_png(w2d, pngAll, 0, 400, 400, 96, nullptr);
    std::vector<int> hide = { 10 };
    const auto rHid = zs::dwf::w2d::render_w2d_file_to_png(w2d, pngHid, 0, 400, 400, 96, &hide);
    expect(rAll.success && rHid.success, "both renders succeed");
    const long nAll = count_nonwhite(pngAll), nHid = count_nonwhite(pngHid);
    expect(nAll > 0, "full render has ink");
    expect(nHid >= 0 && nHid < nAll, "hiding layer 10 reduces ink (" + std::to_string(nHid) + " < " + std::to_string(nAll) + ")");

    std::error_code ec;
    std::filesystem::remove(w2d, ec);
    std::filesystem::remove(pngAll, ec);
    std::filesystem::remove(pngHid, ec);
    if (g_failures == 0) std::printf("OK: %zu layers read; hide-layer ink %ld -> %ld\n", layers.size(), nAll, nHid);
    return g_failures == 0 ? 0 : 1;
}
