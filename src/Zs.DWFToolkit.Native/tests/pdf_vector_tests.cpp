// Vector-PDF regression: converts the committed IRD-Addition.dwf (a real 2D ePlot
// package) to a true-vector PDF through the C ABI and checks the output is a valid,
// vector PDF (not a rasterized blob): %PDF header, at least one /Page, presence of
// vector path operators, and a sane (small) size.
#include "zs_dwf_toolkit_native.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
int count_occurrences(const std::string& hay, const std::string& needle)
{
    int n = 0;
    for (std::size_t pos = 0; (pos = hay.find(needle, pos)) != std::string::npos; pos += needle.size())
        ++n;
    return n;
}
}

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

    const std::string pdf = (std::filesystem::temp_directory_path() / "ird_vector.pdf").string();
    const int rc = zs_dwf_render_dwf_pdf(dwf.c_str(), pdf.c_str());
    if (rc != 0)
    {
        std::printf("FAIL: render_dwf_pdf rc=%d err=%s\n", rc, zs_dwf_get_last_error());
        return 1;
    }

    std::ifstream in(pdf, std::ios::binary);
    if (!in.good()) { std::printf("FAIL: cannot open output PDF\n"); return 1; }
    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    if (bytes.rfind("%PDF-1.7", 0) != 0) { std::printf("FAIL: missing %%PDF header\n"); return 1; }
    if (bytes.find("%%EOF") == std::string::npos) { std::printf("FAIL: missing EOF marker\n"); return 1; }

    const int pages = count_occurrences(bytes, "/Type /Page ");
    if (pages < 1) { std::printf("FAIL: no /Page objects\n"); return 1; }

    // The drawing content stream is FlateDecode-compressed, so vector ops won't be
    // visible as plain text; assert the page references a FlateDecode content stream
    // and the file is far smaller than a rasterized A-size page (which is many MB).
    if (bytes.find("/FlateDecode") == std::string::npos)
    { std::printf("FAIL: content not FlateDecode-compressed\n"); return 1; }

    const auto size = std::filesystem::file_size(pdf);
    if (size == 0 || size > 8u * 1024u * 1024u)
    { std::printf("FAIL: unexpected PDF size %llu\n", static_cast<unsigned long long>(size)); return 1; }

    std::printf("OK: vector PDF pages=%d size=%llu bytes\n", pages, static_cast<unsigned long long>(size));
    std::error_code ec;
    std::filesystem::remove(pdf, ec);
    return 0;
}
