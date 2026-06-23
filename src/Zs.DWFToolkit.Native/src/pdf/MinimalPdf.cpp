#include "MinimalPdf.h"

#include <cstdio>
#include <fstream>
#include <sstream>

#ifdef ZS_DWF_WITH_ZLIB
#include <zlib.h>
#endif

namespace zs::dwf::pdf
{
namespace
{
    // Deflates raw into a zlib stream when zlib is available; returns true and sets
    // out, or false (out untouched) so the caller embeds raw bytes uncompressed.
    bool deflate_bytes(const std::string& raw, std::string& out)
    {
#ifdef ZS_DWF_WITH_ZLIB
        if (raw.empty()) return false;
        uLongf cap = compressBound(static_cast<uLong>(raw.size()));
        std::string buf;
        buf.resize(cap);
        if (compress2(reinterpret_cast<Bytef*>(buf.data()), &cap,
                      reinterpret_cast<const Bytef*>(raw.data()), static_cast<uLong>(raw.size()),
                      Z_BEST_COMPRESSION) != Z_OK)
            return false;
        buf.resize(cap);
        out.swap(buf);
        return true;
#else
        (void)raw; (void)out;
        return false;
#endif
    }

    // Appends a PDF indirect object, recording its byte offset for the xref table.
    // Returns the (1-based) object id just written.
    int emit_object(std::string& out, std::vector<std::size_t>& offsets, const std::string& body)
    {
        const int id = static_cast<int>(offsets.size()); // offsets[0] is the free head
        offsets.push_back(out.size());
        out += std::to_string(id);
        out += " 0 obj\n";
        out += body;
        out += "\nendobj\n";
        return id;
    }

    // Same as emit_object but the body is a stream object whose binary payload is
    // written verbatim (caller supplies the dictionary, sans /Length, in dict_head).
    int emit_stream_object(std::string& out, std::vector<std::size_t>& offsets,
                           const std::string& dict_head, const std::string& stream_bytes)
    {
        const int id = static_cast<int>(offsets.size());
        offsets.push_back(out.size());
        out += std::to_string(id);
        out += " 0 obj\n<< ";
        out += dict_head;
        out += " /Length ";
        out += std::to_string(stream_bytes.size());
        out += " >>\nstream\n";
        out += stream_bytes;
        out += "\nendstream\nendobj\n";
        return id;
    }
}

bool write_pdf(const std::string& path, const std::vector<PdfPage>& pages, std::string& error)
{
    if (pages.empty())
    {
        error = "no pages to write";
        return false;
    }

    std::string out = "%PDF-1.7\n%\xE2\xE3\xCF\xD3\n";
    // offsets[id] = byte offset of object id; index 0 is the free-list head sentinel.
    std::vector<std::size_t> offsets;
    offsets.push_back(0);

    // Reserve ids 1 (Catalog) and 2 (Pages) by pre-recording placeholders so page
    // objects can reference "2 0 R" while we emit them first.
    const int catalog_id = 1;
    const int pages_id = 2;
    offsets.push_back(0); // id 1 placeholder (Catalog), patched below
    offsets.push_back(0); // id 2 placeholder (Pages), patched below

    std::vector<int> page_ids;
    page_ids.reserve(pages.size());

    for (const auto& page : pages)
    {
        // Emit image XObjects first so the page Resources can reference them.
        std::vector<int> image_ids;
        image_ids.reserve(page.images.size());
        for (const auto& img : page.images)
        {
            if (img.width <= 0 || img.height <= 0 ||
                img.rgb.size() < static_cast<std::size_t>(img.width) * img.height * 3)
                continue;
            std::ostringstream dict;
            dict << "/Type /XObject /Subtype /Image /Width " << img.width
                 << " /Height " << img.height
                 << " /ColorSpace /DeviceRGB /BitsPerComponent 8";
            std::string bytes(reinterpret_cast<const char*>(img.rgb.data()),
                              static_cast<std::size_t>(img.width) * img.height * 3);
            std::string deflated;
            if (deflate_bytes(bytes, deflated))
            {
                dict << " /Filter /FlateDecode";
                bytes.swap(deflated);
            }
            image_ids.push_back(emit_stream_object(out, offsets, dict.str(), bytes));
        }

        std::string content_bytes = page.content;
        std::string content_dict;
        std::string content_deflated;
        if (deflate_bytes(content_bytes, content_deflated))
        {
            content_dict = "/Filter /FlateDecode";
            content_bytes.swap(content_deflated);
        }
        const int content_id = emit_stream_object(out, offsets, content_dict, content_bytes);

        std::ostringstream xobj;
        for (std::size_t i = 0; i < image_ids.size(); ++i)
            xobj << "/Im" << i << ' ' << image_ids[i] << " 0 R ";

        std::ostringstream page_dict;
        page_dict << "<< /Type /Page /Parent " << pages_id << " 0 R "
                  << "/MediaBox [0 0 " << page.media_w_pt << ' ' << page.media_h_pt << "] "
                  << "/Contents " << content_id << " 0 R "
                  << "/Resources << /ProcSet [/PDF /ImageC]";
        if (!image_ids.empty())
            page_dict << " /XObject << " << xobj.str() << ">>";
        page_dict << " >> >>";
        page_ids.push_back(emit_object(out, offsets, page_dict.str()));
    }

    // Patch the Pages object (id 2).
    {
        offsets[pages_id] = out.size();
        std::ostringstream kids;
        for (int id : page_ids) kids << id << " 0 R ";
        out += std::to_string(pages_id);
        out += " 0 obj\n<< /Type /Pages /Kids [ ";
        out += kids.str();
        out += "] /Count ";
        out += std::to_string(page_ids.size());
        out += " >>\nendobj\n";
    }
    // Patch the Catalog object (id 1).
    {
        offsets[catalog_id] = out.size();
        out += std::to_string(catalog_id);
        out += " 0 obj\n<< /Type /Catalog /Pages ";
        out += std::to_string(pages_id);
        out += " 0 R >>\nendobj\n";
    }

    // Cross-reference table.
    const std::size_t xref_pos = out.size();
    const int obj_count = static_cast<int>(offsets.size()); // includes the free head
    out += "xref\n0 ";
    out += std::to_string(obj_count);
    out += "\n0000000000 65535 f \n";
    for (int id = 1; id < obj_count; ++id)
    {
        char line[24];
        std::snprintf(line, sizeof(line), "%010zu 00000 n \n", offsets[id]);
        out += line;
    }
    out += "trailer\n<< /Size ";
    out += std::to_string(obj_count);
    out += " /Root ";
    out += std::to_string(catalog_id);
    out += " 0 R >>\nstartxref\n";
    out += std::to_string(xref_pos);
    out += "\n%%EOF\n";

    std::ofstream f(path, std::ios::binary);
    if (!f.good())
    {
        error = "cannot open output PDF for writing: " + path;
        return false;
    }
    f.write(out.data(), static_cast<std::streamsize>(out.size()));
    if (!f.good())
    {
        error = "failed writing PDF bytes: " + path;
        return false;
    }
    return true;
}

} // namespace zs::dwf::pdf
