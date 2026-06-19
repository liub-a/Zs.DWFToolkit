// Verifies decode_group4 without a W2D sample: encode a known bitonal pattern to
// CCITT Group 4 with libtiff, pull the raw compressed strip via TIFFReadRawStrip,
// decode it through our Group4Decoder, and compare to the original bits.
#include "image/Group4Decoder.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <tiffio.h>

namespace
{
int g_failures = 0;
void expect(bool ok, const std::string& msg)
{
    if (!ok) { std::printf("FAIL: %s\n", msg.c_str()); ++g_failures; }
}
}

int main()
{
    const int W = 16, H = 8;
    // Original pattern: left half black (bit 1), right half white (bit 0).
    std::vector<std::uint8_t> expected(static_cast<size_t>(W) * H, 0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            expected[static_cast<size_t>(y) * W + x] = (x < W / 2) ? 1 : 0;

    const std::string path = (std::filesystem::temp_directory_path() / "g4_test.tif").string();

    // Encode to a CCITT G4 TIFF.
    TIFFSetWarningHandler(nullptr);
    TIFF* w = TIFFOpen(path.c_str(), "w");
    if (!w) { std::printf("FAIL: TIFFOpen write\n"); return 1; }
    TIFFSetField(w, TIFFTAG_IMAGEWIDTH, W);
    TIFFSetField(w, TIFFTAG_IMAGELENGTH, H);
    TIFFSetField(w, TIFFTAG_BITSPERSAMPLE, 1);
    TIFFSetField(w, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(w, TIFFTAG_COMPRESSION, COMPRESSION_CCITTFAX4);
    TIFFSetField(w, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
    TIFFSetField(w, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    TIFFSetField(w, TIFFTAG_ROWSPERSTRIP, H);
    const int stride = (W + 7) / 8;
    std::vector<std::uint8_t> row(static_cast<size_t>(stride));
    for (int y = 0; y < H; ++y)
    {
        std::fill(row.begin(), row.end(), 0);
        for (int x = 0; x < W; ++x)
            if (expected[static_cast<size_t>(y) * W + x])
                row[x >> 3] |= static_cast<std::uint8_t>(0x80 >> (x & 7));
        TIFFWriteScanline(w, row.data(), static_cast<uint32_t>(y), 0);
    }
    TIFFClose(w);

    // Pull the raw G4 strip and decode it.
    TIFF* r = TIFFOpen(path.c_str(), "r");
    if (!r) { std::printf("FAIL: TIFFOpen read\n"); return 1; }
    const tmsize_t raw_size = TIFFRawStripSize(r, 0);
    std::vector<std::uint8_t> raw(static_cast<size_t>(raw_size));
    const tmsize_t got = TIFFReadRawStrip(r, 0, raw.data(), raw_size);
    TIFFClose(r);
    std::error_code ec; std::filesystem::remove(path, ec);
    expect(got > 0, "read raw G4 strip");

    std::vector<std::uint8_t> decoded;
    const bool ok = zs::dwf::image::decode_group4(raw.data(), static_cast<size_t>(got), W, H, decoded);
    expect(ok, "decode_group4 succeeds");
    if (ok)
    {
        expect(decoded.size() == expected.size(), "decoded pixel count matches");
        bool same = decoded.size() == expected.size();
        for (size_t i = 0; same && i < expected.size(); ++i)
            if (decoded[i] != expected[i]) same = false;
        expect(same, "decoded bits match the original pattern");
    }

    if (g_failures == 0) { std::printf("Group4 decode test passed.\n"); return 0; }
    std::printf("%d Group4 assertion(s) failed.\n", g_failures);
    return 1;
}
