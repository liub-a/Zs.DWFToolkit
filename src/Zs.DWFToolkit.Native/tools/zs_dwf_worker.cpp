// Standalone native render worker. The managed library runs this out-of-process
// (with a timeout) so a crash or hang in the native renderer or the underlying DWF
// toolkit cannot take down the host process. It prints the result JSON to stdout
// and exits with the native result code.
//
// Usage:
//   zs_dwf_worker info   <input>
//   zs_dwf_worker page   <input> <page> <output> <width> <height> <dpi>
//   zs_dwf_worker w2d    <input> <output> <width> <height> <dpi>
#include "zs_dwf_toolkit_native.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
constexpr int kJsonBuffer = 1 << 20; // 1 MiB result buffer

int usage()
{
    std::fprintf(stderr,
        "usage:\n"
        "  zs_dwf_worker info <input>\n"
        "  zs_dwf_worker page <input> <page> <output> <width> <height> <dpi>\n"
        "  zs_dwf_worker w2d  <input> <output> <width> <height> <dpi>\n");
    return ZS_DWF_INVALID_ARGUMENT;
}

void emit(const std::vector<char>& json)
{
    std::fputs(json.data(), stdout);
    std::fputc('\n', stdout);
}
}

int main(int argc, char** argv)
{
    if (argc < 3)
        return usage();

    const std::string cmd = argv[1];
    std::vector<char> json(kJsonBuffer, 0);
    int rc = ZS_DWF_INVALID_ARGUMENT;

    if (cmd == "info" && argc == 3)
    {
        rc = zs_dwf_get_info_json(argv[2], json.data(), kJsonBuffer);
    }
    else if (cmd == "page" && argc == 8)
    {
        rc = zs_dwf_render_page(argv[2], std::atoi(argv[3]), argv[4],
                                std::atoi(argv[5]), std::atoi(argv[6]), std::atoi(argv[7]),
                                json.data(), kJsonBuffer);
    }
    else if (cmd == "w2d" && argc == 7)
    {
        rc = zs_w2d_render_file(argv[2], argv[3],
                                std::atoi(argv[4]), std::atoi(argv[5]), std::atoi(argv[6]),
                                json.data(), kJsonBuffer);
    }
    else if (cmd == "stamp" && argc == 11)
    {
        // stamp <in.w2d> <out.w2d> <rgba_file> <w> <h> <minx> <miny> <maxx> <maxy>
        FILE* f = std::fopen(argv[4], "rb");
        if (!f) { std::fprintf(stderr, "cannot open rgba file\n"); return ZS_DWF_FILE_NOT_FOUND; }
        std::fseek(f, 0, SEEK_END);
        long n = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        std::vector<unsigned char> rgba(n > 0 ? n : 0);
        if (n > 0 && std::fread(rgba.data(), 1, n, f) != static_cast<size_t>(n)) { std::fclose(f); return ZS_DWF_INTERNAL_ERROR; }
        std::fclose(f);
        rc = zs_w2d_stamp_image(argv[2], argv[3], rgba.data(), static_cast<int>(rgba.size()),
                                std::atoi(argv[5]), std::atoi(argv[6]),
                                std::atoi(argv[7]), std::atoi(argv[8]), std::atoi(argv[9]), std::atoi(argv[10]));
        std::fprintf(stderr, "stamp rc=%d %s\n", rc, zs_dwf_get_last_error());
        return rc;
    }
    else
    {
        return usage();
    }

    if (json[0] != '\0')
        emit(json);
    else
        std::fprintf(stderr, "%s\n", zs_dwf_get_last_error());
    return rc;
}
