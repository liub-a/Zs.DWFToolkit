#include "Group4Decoder.h"

#ifdef ZS_DWF_WITH_TIFF
#include <cstring>
#include <tiffio.h>
#endif

namespace zs::dwf::image
{

#ifdef ZS_DWF_WITH_TIFF
namespace
{
    // Wraps the raw G4 bitstream in a minimal single-strip little-endian TIFF so
    // libtiff's CCITT Fax4 codec can decode it. All tag values fit in 4 bytes.
    std::vector<std::uint8_t> build_g4_tiff(const std::uint8_t* data, std::size_t size, int cols, int rows)
    {
        struct Entry { std::uint16_t tag, type; std::uint32_t count, value; };
        const std::uint32_t strip_off = 8;
        const std::uint32_t ifd_off = static_cast<std::uint32_t>(8 + size);

        // type: 3 = SHORT, 4 = LONG. SHORT values occupy the high-order? In TIFF the
        // value is left-justified within the 4-byte field for the file's byte order;
        // for little-endian a SHORT just sits in the low 2 bytes.
        const Entry entries[] = {
            {256, 4, 1, static_cast<std::uint32_t>(cols)},  // ImageWidth
            {257, 4, 1, static_cast<std::uint32_t>(rows)},  // ImageLength
            {258, 3, 1, 1},                                  // BitsPerSample
            {259, 3, 1, 4},                                  // Compression = CCITT G4
            {262, 3, 1, 0},                                  // Photometric = WhiteIsZero
            {273, 4, 1, strip_off},                          // StripOffsets
            {277, 3, 1, 1},                                  // SamplesPerPixel
            {278, 4, 1, static_cast<std::uint32_t>(rows)},   // RowsPerStrip
            {279, 4, 1, static_cast<std::uint32_t>(size)},   // StripByteCounts
        };
        const std::uint16_t n = static_cast<std::uint16_t>(sizeof(entries) / sizeof(entries[0]));

        std::vector<std::uint8_t> t;
        auto u16 = [&](std::uint16_t v){ t.push_back(v & 0xff); t.push_back((v >> 8) & 0xff); };
        auto u32 = [&](std::uint32_t v){ for (int i = 0; i < 4; ++i) t.push_back((v >> (8 * i)) & 0xff); };

        u16(0x4949); u16(42); u32(ifd_off);          // header (II)
        t.insert(t.end(), data, data + size);         // strip at offset 8
        u16(n);                                        // IFD entry count
        for (const auto& e : entries) { u16(e.tag); u16(e.type); u32(e.count); u32(e.value); }
        u32(0);                                        // next IFD = none
        return t;
    }

    struct MemReader { const std::uint8_t* buf; tmsize_t size; tmsize_t pos; };

    tmsize_t mem_read(thandle_t h, void* p, tmsize_t n)
    {
        auto* m = static_cast<MemReader*>(h);
        if (m->pos >= m->size) return 0;
        const tmsize_t k = (n < m->size - m->pos) ? n : m->size - m->pos;
        std::memcpy(p, m->buf + m->pos, static_cast<size_t>(k));
        m->pos += k;
        return k;
    }
    tmsize_t mem_write(thandle_t, void*, tmsize_t) { return 0; }
    toff_t mem_seek(thandle_t h, toff_t off, int whence)
    {
        auto* m = static_cast<MemReader*>(h);
        tmsize_t base = whence == SEEK_CUR ? m->pos : (whence == SEEK_END ? m->size : 0);
        m->pos = base + static_cast<tmsize_t>(off);
        return static_cast<toff_t>(m->pos);
    }
    int mem_close(thandle_t) { return 0; }
    toff_t mem_size(thandle_t h) { return static_cast<toff_t>(static_cast<MemReader*>(h)->size); }
}
#endif

bool decode_group4(const std::uint8_t* data, std::size_t size, int cols, int rows,
                   std::vector<std::uint8_t>& out_indices)
{
#ifndef ZS_DWF_WITH_TIFF
    (void)data; (void)size; (void)cols; (void)rows; (void)out_indices;
    return false;
#else
    if (!data || size == 0 || cols <= 0 || rows <= 0)
        return false;

    const auto tiff_bytes = build_g4_tiff(data, size, cols, rows);
    MemReader m{ tiff_bytes.data(), static_cast<tmsize_t>(tiff_bytes.size()), 0 };

    // Silence libtiff's default stderr warning/error handlers.
    TIFFSetWarningHandler(nullptr);
    TIFF* tif = TIFFClientOpen("g4", "rm", &m, mem_read, mem_write, mem_seek, mem_close, mem_size, nullptr, nullptr);
    if (!tif)
        return false;

    const tmsize_t row_bytes = TIFFScanlineSize(tif);
    std::vector<std::uint8_t> scan(static_cast<size_t>(row_bytes));
    out_indices.assign(static_cast<size_t>(cols) * static_cast<size_t>(rows), 0);

    bool ok = true;
    for (int y = 0; y < rows; ++y)
    {
        if (TIFFReadScanline(tif, scan.data(), static_cast<uint32_t>(y)) < 0) { ok = false; break; }
        for (int x = 0; x < cols; ++x)
        {
            const std::uint8_t bit = (scan[x >> 3] >> (7 - (x & 7))) & 1;
            out_indices[static_cast<size_t>(y) * cols + x] = bit;
        }
    }
    TIFFClose(tif);
    return ok;
#endif
}

} // namespace zs::dwf::image
