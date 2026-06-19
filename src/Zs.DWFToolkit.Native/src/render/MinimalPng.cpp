#include "MinimalPng.h"
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

namespace zs::dwf::native_render
{
namespace
{
    void write_u32_be(std::vector<std::uint8_t>& out, std::uint32_t v)
    {
        out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xff));
        out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xff));
        out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
        out.push_back(static_cast<std::uint8_t>(v & 0xff));
    }

    std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t* data, std::size_t len)
    {
        static std::array<std::uint32_t, 256> table{};
        static bool initialized = false;
        if (!initialized)
        {
            for (std::uint32_t i = 0; i < 256; ++i)
            {
                std::uint32_t c = i;
                for (int k = 0; k < 8; ++k)
                    c = (c & 1U) ? (0xedb88320U ^ (c >> 1U)) : (c >> 1U);
                table[i] = c;
            }
            initialized = true;
        }

        crc = ~crc;
        for (std::size_t i = 0; i < len; ++i)
            crc = table[(crc ^ data[i]) & 0xffU] ^ (crc >> 8U);
        return ~crc;
    }

    std::uint32_t adler32(const std::vector<std::uint8_t>& data)
    {
        constexpr std::uint32_t mod = 65521U;
        std::uint32_t a = 1;
        std::uint32_t b = 0;
        for (auto byte : data)
        {
            a = (a + byte) % mod;
            b = (b + a) % mod;
        }
        return (b << 16U) | a;
    }

    void append_chunk(std::vector<std::uint8_t>& png, const char type[4], const std::vector<std::uint8_t>& payload)
    {
        write_u32_be(png, static_cast<std::uint32_t>(payload.size()));
        const auto type_start = png.size();
        png.push_back(static_cast<std::uint8_t>(type[0]));
        png.push_back(static_cast<std::uint8_t>(type[1]));
        png.push_back(static_cast<std::uint8_t>(type[2]));
        png.push_back(static_cast<std::uint8_t>(type[3]));
        png.insert(png.end(), payload.begin(), payload.end());
        const auto crc = crc32_update(0, png.data() + type_start, 4 + payload.size());
        write_u32_be(png, crc);
    }

    std::vector<std::uint8_t> zlib_store_blocks(const std::vector<std::uint8_t>& raw)
    {
        std::vector<std::uint8_t> z;
        z.reserve(raw.size() + raw.size() / 65535 + 16);
        // zlib header: CMF/FLG for no compression/fastest. 0x7801 is valid.
        z.push_back(0x78);
        z.push_back(0x01);

        std::size_t offset = 0;
        while (offset < raw.size())
        {
            const std::size_t remaining = raw.size() - offset;
            const std::uint16_t block_len = static_cast<std::uint16_t>(remaining > 65535 ? 65535 : remaining);
            const bool is_final = (offset + block_len) == raw.size();
            z.push_back(is_final ? 0x01 : 0x00); // BFINAL + BTYPE=00
            z.push_back(static_cast<std::uint8_t>(block_len & 0xff));
            z.push_back(static_cast<std::uint8_t>((block_len >> 8) & 0xff));
            const std::uint16_t nlen = static_cast<std::uint16_t>(~block_len);
            z.push_back(static_cast<std::uint8_t>(nlen & 0xff));
            z.push_back(static_cast<std::uint8_t>((nlen >> 8) & 0xff));
            z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset), raw.begin() + static_cast<std::ptrdiff_t>(offset + block_len));
            offset += block_len;
        }

        write_u32_be(z, adler32(raw));
        return z;
    }
}

bool write_png_rgba(const std::string& path, int width, int height, const std::vector<Rgba>& pixels, std::string& error)
{
    if (width <= 0 || height <= 0)
    {
        error = "invalid PNG dimensions";
        return false;
    }
    if (pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
    {
        error = "pixel buffer size does not match dimensions";
        return false;
    }

    const std::size_t raw_stride = 1 + static_cast<std::size_t>(width) * 4;
    if (raw_stride > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(height))
    {
        error = "PNG image is too large";
        return false;
    }

    std::vector<std::uint8_t> raw;
    raw.resize(raw_stride * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y)
    {
        auto* row = raw.data() + static_cast<std::size_t>(y) * raw_stride;
        row[0] = 0; // filter type 0
        for (int x = 0; x < width; ++x)
        {
            const auto& px = pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
            auto* p = row + 1 + static_cast<std::size_t>(x) * 4;
            p[0] = px.r;
            p[1] = px.g;
            p[2] = px.b;
            p[3] = px.a;
        }
    }

    std::vector<std::uint8_t> png;
    png.reserve(raw.size() + 128);
    constexpr std::uint8_t sig[] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    png.insert(png.end(), std::begin(sig), std::end(sig));

    std::vector<std::uint8_t> ihdr;
    write_u32_be(ihdr, static_cast<std::uint32_t>(width));
    write_u32_be(ihdr, static_cast<std::uint32_t>(height));
    ihdr.push_back(8); // bit depth
    ihdr.push_back(6); // color type RGBA
    ihdr.push_back(0); // compression
    ihdr.push_back(0); // filter
    ihdr.push_back(0); // interlace
    append_chunk(png, "IHDR", ihdr);

    auto idat = zlib_store_blocks(raw);
    append_chunk(png, "IDAT", idat);
    append_chunk(png, "IEND", {});

    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        error = "failed to open PNG output path";
        return false;
    }
    out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    if (!out.good())
    {
        error = "failed to write PNG file";
        return false;
    }
    return true;
}

namespace
{
    std::uint32_t read_u32_be(const std::uint8_t* p)
    {
        return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
               (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
    }

    // Inflates the zlib stored-block stream produced by zlib_store_blocks.
    bool inflate_stored(const std::vector<std::uint8_t>& z, std::vector<std::uint8_t>& out, std::string& error)
    {
        if (z.size() < 2) { error = "zlib stream too short"; return false; }
        std::size_t i = 2; // skip CMF/FLG
        for (;;)
        {
            if (i + 5 > z.size()) { error = "truncated deflate block header"; return false; }
            const std::uint8_t bfinal_btype = z[i++];
            if ((bfinal_btype & 0x06) != 0) { error = "only stored deflate blocks are supported"; return false; }
            const std::uint16_t len = static_cast<std::uint16_t>(z[i] | (z[i + 1] << 8));
            i += 4; // len + nlen
            if (i + len > z.size()) { error = "truncated stored block data"; return false; }
            out.insert(out.end(), z.begin() + static_cast<std::ptrdiff_t>(i), z.begin() + static_cast<std::ptrdiff_t>(i + len));
            i += len;
            if ((bfinal_btype & 0x01) != 0) break;
        }
        return true;
    }
}

bool read_png_rgba(const std::string& path, int& width, int& height, std::vector<Rgba>& pixels, std::string& error)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) { error = "failed to open PNG"; return false; }
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.size() < 8 || data[0] != 0x89 || data[1] != 'P') { error = "not a PNG"; return false; }

    std::size_t pos = 8;
    std::vector<std::uint8_t> idat;
    width = height = 0;
    while (pos + 8 <= data.size())
    {
        const std::uint32_t len = read_u32_be(&data[pos]);
        const char* type = reinterpret_cast<const char*>(&data[pos + 4]);
        const std::size_t body = pos + 8;
        if (body + len + 4 > data.size()) { error = "truncated chunk"; return false; }
        if (std::string(type, 4) == "IHDR")
        {
            width = static_cast<int>(read_u32_be(&data[body]));
            height = static_cast<int>(read_u32_be(&data[body + 4]));
        }
        else if (std::string(type, 4) == "IDAT")
        {
            idat.insert(idat.end(), data.begin() + static_cast<std::ptrdiff_t>(body), data.begin() + static_cast<std::ptrdiff_t>(body + len));
        }
        else if (std::string(type, 4) == "IEND")
        {
            break;
        }
        pos = body + len + 4; // skip data + CRC
    }

    if (width <= 0 || height <= 0) { error = "missing IHDR"; return false; }

    std::vector<std::uint8_t> raw;
    if (!inflate_stored(idat, raw, error)) return false;

    const std::size_t stride = 1 + static_cast<std::size_t>(width) * 4;
    if (raw.size() < stride * static_cast<std::size_t>(height)) { error = "decoded data too small"; return false; }

    pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y)
    {
        const std::uint8_t* row = raw.data() + static_cast<std::size_t>(y) * stride;
        if (row[0] != 0) { error = "unsupported PNG row filter"; return false; }
        for (int x = 0; x < width; ++x)
        {
            const std::uint8_t* p = row + 1 + static_cast<std::size_t>(x) * 4;
            pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
                Rgba{ p[0], p[1], p[2], p[3] };
        }
    }
    return true;
}

std::vector<Rgba> downsample_box(const std::vector<Rgba>& src, int w, int h, int factor)
{
    if (factor <= 1 || w <= 0 || h <= 0)
        return src;
    const int dw = w / factor;
    const int dh = h / factor;
    std::vector<Rgba> dst(static_cast<std::size_t>(dw) * static_cast<std::size_t>(dh));
    const int n = factor * factor;
    for (int dy = 0; dy < dh; ++dy)
    {
        for (int dx = 0; dx < dw; ++dx)
        {
            unsigned int r = 0, g = 0, b = 0, a = 0;
            for (int sy = 0; sy < factor; ++sy)
            {
                const std::size_t row = static_cast<std::size_t>(dy * factor + sy) * static_cast<std::size_t>(w);
                for (int sx = 0; sx < factor; ++sx)
                {
                    const Rgba& p = src[row + static_cast<std::size_t>(dx * factor + sx)];
                    r += p.r; g += p.g; b += p.b; a += p.a;
                }
            }
            dst[static_cast<std::size_t>(dy) * static_cast<std::size_t>(dw) + static_cast<std::size_t>(dx)] =
                Rgba{ static_cast<std::uint8_t>(r / n), static_cast<std::uint8_t>(g / n),
                      static_cast<std::uint8_t>(b / n), static_cast<std::uint8_t>(a / n) };
        }
    }
    return dst;
}

} // namespace zs::dwf::native_render
