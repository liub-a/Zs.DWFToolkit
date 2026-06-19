// Dependency-light unit tests for the RasterCanvas primitives. No ODA toolkit is
// required, so these run in the default native build. Each EXPECT records a
// failure; main returns non-zero if any fail so CTest reports the result.
#include "render/RasterCanvas.h"

#include <cstdio>
#include <string>

using namespace zs::dwf::native_render;

namespace
{
int g_failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::printf("FAIL: %s\n", message.c_str());
        ++g_failures;
    }
}

// 100x100 canvas mapped to logical [0,0]-[100,100]; scale 1, no centering offset
// (square logical box in a square canvas), so logical (x,y) maps to pixel
// (x, 100-y).
BoxD unit_box() { return BoxD{0.0, 0.0, 100.0, 100.0}; }

const Rgba& pixel_at(const RasterCanvas& c, int x, int y)
{
    return c.pixels()[static_cast<std::size_t>(y) * static_cast<std::size_t>(c.width()) + static_cast<std::size_t>(x)];
}

bool is_color(const Rgba& p, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    return p.r == r && p.g == g && p.b == b;
}
}

int main()
{
    const Rgba red{255, 0, 0, 255};
    const Rgba blue{0, 0, 255, 255};

    // Background starts white.
    {
        RasterCanvas c(100, 100, unit_box());
        expect(is_color(pixel_at(c, 50, 50), 255, 255, 255), "canvas initializes white");
    }

    // A filled polygon paints its interior.
    {
        RasterCanvas c(100, 100, unit_box());
        std::vector<PointD> square = {{20, 20}, {80, 20}, {80, 80}, {20, 80}};
        c.fill_polygon(square, red);
        expect(is_color(pixel_at(c, 50, 50), 255, 0, 0), "fill_polygon paints interior");
        expect(is_color(pixel_at(c, 5, 5), 255, 255, 255), "fill_polygon leaves exterior white");
    }

    // A polyline draws on the canvas.
    {
        RasterCanvas c(100, 100, unit_box());
        std::vector<PointD> line = {{0, 50}, {100, 50}};
        c.draw_polyline(line, blue, 1);
        expect(is_color(pixel_at(c, 50, 50), 0, 0, 255), "draw_polyline paints along the line");
    }

    // draw_image blits a source buffer into the logical rect (nearest neighbour).
    {
        RasterCanvas c(100, 100, unit_box());
        std::vector<Rgba> src = {red, red, red, red}; // 2x2 all-red
        c.draw_image(BoxD{10, 10, 90, 90}, 2, 2, src);
        expect(is_color(pixel_at(c, 50, 50), 255, 0, 0), "draw_image fills the target rect");
        expect(is_color(pixel_at(c, 2, 2), 255, 255, 255), "draw_image leaves outside untouched");
    }

    // Fully transparent image pixels are skipped (alpha 0).
    {
        RasterCanvas c(100, 100, unit_box());
        std::vector<Rgba> src = {Rgba{255, 0, 0, 0}};
        c.draw_image(BoxD{10, 10, 90, 90}, 1, 1, src);
        expect(is_color(pixel_at(c, 50, 50), 255, 255, 255), "draw_image skips transparent pixels");
    }

    if (g_failures == 0)
    {
        std::printf("All RasterCanvas tests passed.\n");
        return 0;
    }
    std::printf("%d RasterCanvas test(s) failed.\n", g_failures);
    return 1;
}
