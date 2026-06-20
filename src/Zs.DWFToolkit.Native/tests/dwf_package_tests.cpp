// Full DWF-package regression: renders page 0 of the committed IRD-Addition.dwf
// (a real 2D ePlot package) through the C ABI and checks a non-trivial PNG with
// content (the package->section->W2D->raster path end to end).
#include "zs_dwf_toolkit_native.h"
#include "render/MinimalPng.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

using zs::dwf::native_render::Rgba;
using zs::dwf::native_render::read_png_rgba;

int main()
{
    std::string dwf;
#ifdef ZS_DWF_DATA_DIR
    dwf = std::string(ZS_DWF_DATA_DIR) + "/IRD-Addition.dwf";
#endif
    if (dwf.empty() || !std::filesystem::exists(dwf))
    {
        std::printf("IRD sample not available; skipping.\n");
        return 0;
    }

    const std::string png = (std::filesystem::temp_directory_path() / "ird_page0.png").string();
    std::vector<char> json(1 << 18, 0);
    const int rc = zs_dwf_render_page(dwf.c_str(), 0, png.c_str(), 1200, 900, 96, json.data(), static_cast<int>(json.size()));
    if (rc != 0)
    {
        std::printf("FAIL: render rc=%d err=%s\n", rc, zs_dwf_get_last_error());
        return 1;
    }

    int w = 0, h = 0;
    std::vector<Rgba> px;
    std::string err;
    if (!read_png_rgba(png, w, h, px, err)) { std::printf("FAIL: %s\n", err.c_str()); return 1; }

    int failures = 0;
    if (!(w == 1200 && h == 900)) { std::printf("FAIL: dims %dx%d\n", w, h); ++failures; }
    long nonwhite = 0;
    for (const auto& p : px)
        if (!(p.r > 245 && p.g > 245 && p.b > 245)) ++nonwhite;
    // A title sheet has plenty of ink but is far from full; expect a sane band.
    if (nonwhite < 5000 || nonwhite > static_cast<long>(px.size()) / 2)
    {
        std::printf("FAIL: non-white pixels out of range: %ld\n", nonwhite);
        ++failures;
    }

    std::error_code ec; std::filesystem::remove(png, ec);
    if (failures == 0) { std::printf("DWF package render test passed (nonwhite=%ld).\n", nonwhite); return 0; }
    return 1;
}
