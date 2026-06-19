// Pixel/coverage regression against the public ODA "ocean_90.w2d" sample.
// The sample is not committed (third-party); CI downloads it and points
// ZS_DWF_OCEAN_W2D at it. Without the env var the test is a no-op pass so local
// runs without the file still succeed. Assertions use exact primitive counts
// (deterministic from parsing) and a tolerant non-white pixel band (rasterization
// may vary slightly across platforms).
#include "zs_dwf_toolkit_native.h"
#include "render/MinimalPng.h"

#include <cstdio>
#include <cstdlib>
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

// Returns the integer following "\"key\":" in the JSON, or -1.
long json_int(const std::string& j, const std::string& key)
{
    const auto pos = j.find("\"" + key + "\":");
    if (pos == std::string::npos) return -1;
    return std::strtol(j.c_str() + pos + key.size() + 3, nullptr, 10);
}
}

int main()
{
    const char* w2d = std::getenv("ZS_DWF_OCEAN_W2D");
    if (!w2d || !*w2d)
    {
        std::printf("ZS_DWF_OCEAN_W2D not set; skipping ocean regression.\n");
        return 0;
    }

    const std::string png = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/ocean_reg.png";
    std::vector<char> json(1 << 18, 0);
    const int rc = zs_w2d_render_file(w2d, png.c_str(), 1000, 800, 96, json.data(), static_cast<int>(json.size()));
    expect(rc == 0, std::string("render rc=0 (got ") + std::to_string(rc) + ")");
    const std::string j(json.data());

    const long polyline = json_int(j, "polyline");
    const long polygon = json_int(j, "polygon");
    const long text = json_int(j, "text");

    int w = 0, h = 0;
    std::vector<Rgba> px;
    std::string err;
    long nonwhite = -1;
    if (read_png_rgba(png, w, h, px, err))
    {
        nonwhite = 0;
        for (const auto& p : px)
            if (!(p.r > 245 && p.g > 245 && p.b > 245)) ++nonwhite;
    }
    else
    {
        expect(false, "read_png_rgba: " + err);
    }

    std::printf("OCEAN BASELINE: polyline=%ld polygon=%ld text=%ld dims=%dx%d nonwhite=%ld\n",
                polyline, polygon, text, w, h, nonwhite);

#ifdef ZS_DWF_OCEAN_ASSERT
    // Exact primitive counts (parser is deterministic).
    expect(polyline == ZS_DWF_OCEAN_POLYLINE, "polyline count matches baseline");
    expect(polygon == ZS_DWF_OCEAN_POLYGON, "polygon count matches baseline");
    expect(w == 1000 && h == 800, "output dimensions match");
    // Non-white pixels: allow +/-15% for rasterization differences.
    const long lo = ZS_DWF_OCEAN_NONWHITE * 85 / 100;
    const long hi = ZS_DWF_OCEAN_NONWHITE * 115 / 100;
    expect(nonwhite >= lo && nonwhite <= hi,
           std::string("non-white pixels in band [") + std::to_string(lo) + "," + std::to_string(hi) + "]");
#endif

    if (g_failures == 0) { std::printf("ocean regression passed.\n"); return 0; }
    std::printf("%d ocean regression assertion(s) failed.\n", g_failures);
    return 1;
}
