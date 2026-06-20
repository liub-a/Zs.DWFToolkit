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
