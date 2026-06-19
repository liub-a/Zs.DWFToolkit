#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace zs::dwf::native_render
{

struct Rgba
{
    std::uint8_t r{0};
    std::uint8_t g{0};
    std::uint8_t b{0};
    std::uint8_t a{255};
};

bool write_png_rgba(const std::string& path, int width, int height, const std::vector<Rgba>& pixels, std::string& error);

} // namespace zs::dwf::native_render
