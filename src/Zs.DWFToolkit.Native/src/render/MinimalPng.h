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

// Reads back a PNG written by write_png_rgba (8-bit RGBA, filter 0, zlib stored
// blocks). Intended for tests/regression; not a general-purpose PNG decoder.
bool read_png_rgba(const std::string& path, int& width, int& height, std::vector<Rgba>& pixels, std::string& error);

// Box-downsamples a (w x h) image by an integer factor, averaging each factor x
// factor block. Used for supersampled anti-aliasing. w and h must be multiples of
// factor. Returns the (w/factor x h/factor) image.
std::vector<Rgba> downsample_box(const std::vector<Rgba>& src, int w, int h, int factor);

} // namespace zs::dwf::native_render
