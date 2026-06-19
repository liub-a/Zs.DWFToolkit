#include "OdaDwfAdapter.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef ZS_DWF_WITH_ODA_DWFTK
#include "dwfcore/Core.h"
#include "dwfcore/Exception.h"
#include "dwfcore/File.h"
#include "dwfcore/InputStream.h"
#include "dwfcore/MIME.h"
#include "dwf/package/Constants.h"
#include "dwf/package/Manifest.h"
#include "dwf/package/Resource.h"
#include "dwf/package/Section.h"
#include "dwf/package/reader/PackageReader.h"
#endif

namespace zs::dwf::oda
{
using zs::dwf::w2d::RenderResult;
using zs::dwf::w2d::render_w2d_file_to_png;

namespace
{
    std::string json_escape_str(const std::string& value)
    {
        std::string out;
        out.reserve(value.size() + 8);
        for (char ch : value)
        {
            switch (ch)
            {
                case '\\': out += "\\\\"; break;
                case '"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += ch; break;
            }
        }
        return out;
    }

#ifdef ZS_DWF_WITH_ODA_DWFTK
    std::string narrow(const wchar_t* value)
    {
        if (!value) return {};
        std::string s;
        for (const wchar_t* p = value; *p; ++p)
        {
            const auto ch = static_cast<unsigned int>(*p);
            s.push_back(ch < 128 ? static_cast<char>(ch) : '?');
        }
        return s;
    }

    std::string to_utf8(const DWFCore::DWFString& value)
    {
        char* raw = nullptr;
        try
        {
            const size_t n = value.getUTF8(&raw);
            std::string out(raw ? raw : "", n);
            if (raw) DWFCORE_FREE_MEMORY(raw);
            return out;
        }
        catch (...)
        {
            if (raw) DWFCORE_FREE_MEMORY(raw);
            return narrow(static_cast<const wchar_t*>(value));
        }
    }

    std::string make_temp_w2d_path(int page_index)
    {
        const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        const auto base = std::filesystem::temp_directory_path();
        std::ostringstream name;
        name << "zs_dwf_page_" << page_index << "_" << now << ".w2d";
        return (base / name.str()).string();
    }

    bool extract_stream_to_file(DWFCore::DWFInputStream& stream, const std::string& out_path, std::string& error)
    {
        std::ofstream out(out_path, std::ios::binary);
        if (!out)
        {
            error = "failed to create temporary W2D file";
            return false;
        }

        std::vector<unsigned char> buffer(64 * 1024);
        while (stream.available() > 0)
        {
            const size_t got = stream.read(buffer.data(), buffer.size());
            if (got == 0)
                break;
            out.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(got));
            if (!out.good())
            {
                error = "failed to write temporary W2D file";
                return false;
            }
        }
        return true;
    }

    DWFToolkit::DWFResource* first_w2d_resource(DWFToolkit::DWFSection& section)
    {
        try
        {
            section.readDescriptor();
        }
        catch (...)
        {
            // Some sections may not have a descriptor reader available. Continue
            // with the resources already present from the manifest.
        }

        DWFToolkit::DWFResourceContainer::ResourceIterator* by_role = section.findResourcesByRole(DWFXML::kzRole_Graphics2d);
        if (by_role)
        {
            for (; by_role->valid(); by_role->next())
            {
                DWFToolkit::DWFResource* r = by_role->get();
                if (r && (r->mime() == DWFCore::DWFMIME::kzMIMEType_W2D || r->role() == DWFXML::kzRole_Graphics2d))
                {
                    DWFCORE_FREE_OBJECT(by_role);
                    return r;
                }
            }
            DWFCORE_FREE_OBJECT(by_role);
        }

        DWFToolkit::DWFResourceContainer::ResourceIterator* by_mime = section.findResourcesByMIME(DWFCore::DWFMIME::kzMIMEType_W2D);
        if (by_mime)
        {
            DWFToolkit::DWFResource* r = by_mime->valid() ? by_mime->get() : nullptr;
            DWFCORE_FREE_OBJECT(by_mime);
            return r;
        }

        return nullptr;
    }
#endif
}

RenderResult render_dwf_or_w2d_page_to_png(
    const std::string& input_path,
    int page_index,
    const std::string& output_path,
    int width_px,
    int height_px,
    int dpi)
{
#ifndef ZS_DWF_WITH_ODA_DWFTK
    (void)input_path;
    (void)page_index;
    (void)output_path;
    (void)width_px;
    (void)height_px;
    (void)dpi;
    return RenderResult{
        false,
        1007,
        "unsupported_dwf_rendering",
        "Native DWF/W2D rendering requires building with ZS_DWF_ENABLE_ODA_DWFTK=ON.",
        ""
    };
#else
    try
    {
        DWFCore::DWFFile file(input_path.c_str());
        DWFToolkit::DWFPackageReader reader(file);
        DWFToolkit::DWFPackageReader::tPackageInfo info;
        reader.getPackageInfo(info);

        if (info.eType == DWFToolkit::DWFPackageReader::eW2DStream ||
            info.eType == DWFToolkit::DWFPackageReader::eDWFStream)
        {
            return render_w2d_file_to_png(input_path, output_path, page_index, width_px, height_px, dpi);
        }

        if (info.eType != DWFToolkit::DWFPackageReader::eDWFPackage &&
            info.eType != DWFToolkit::DWFPackageReader::eDWFXPackage)
        {
            return RenderResult{false, 1004, "unsupported_format", "Input is not a DWF package or W2D stream", ""};
        }

        DWFToolkit::DWFManifest& manifest = reader.getManifest();
        DWFToolkit::DWFManifest::SectionIterator* sections = manifest.getSections();
        if (!sections)
            return RenderResult{false, 1006, "page_not_found", "DWF manifest contains no sections", ""};

        int current_index = 0;
        DWFToolkit::DWFResource* resource = nullptr;
        for (; sections->valid(); sections->next())
        {
            DWFToolkit::DWFSection* section = sections->get();
            if (!section)
                continue;
            if (current_index == page_index)
            {
                resource = first_w2d_resource(*section);
                break;
            }
            ++current_index;
        }
        DWFCORE_FREE_OBJECT(sections);

        if (!resource)
            return RenderResult{false, 1006, "w2d_resource_not_found", "No 2D W2D graphics resource found for the requested DWF page", ""};

        DWFCore::DWFInputStream* stream = reader.extract(resource->href(), false);
        if (!stream)
            return RenderResult{false, 1007, "w2d_extract_failed", "DWF Toolkit failed to extract the W2D resource", ""};

        const std::string temp_w2d = make_temp_w2d_path(page_index);
        std::string extract_error;
        const bool extracted = extract_stream_to_file(*stream, temp_w2d, extract_error);
        DWFCORE_FREE_OBJECT(stream);
        if (!extracted)
            return RenderResult{false, 1008, "w2d_extract_failed", extract_error, ""};

        RenderResult rr = render_w2d_file_to_png(temp_w2d, output_path, page_index, width_px, height_px, dpi);
        std::error_code ec;
        std::filesystem::remove(temp_w2d, ec);
        return rr;
    }
    catch (const DWFCore::DWFException& ex)
    {
        return RenderResult{false, 1007, "dwf_toolkit_exception", narrow(ex.type()) + ": " + narrow(ex.message()), ""};
    }
    catch (const std::exception& ex)
    {
        return RenderResult{false, 1999, "native_exception", ex.what(), ""};
    }
    catch (...)
    {
        return RenderResult{false, 1999, "native_exception", "Unknown native exception", ""};
    }
#endif
}

InfoResult get_dwf_info_json(const std::string& input_path)
{
#ifndef ZS_DWF_WITH_ODA_DWFTK
    (void)input_path;
    InfoResult r;
    r.success = false;
    r.result_code = 1007;
    r.json = std::string("{") +
        "\"success\":false," +
        "\"errorCode\":\"unsupported_dwf_rendering\"," +
        "\"errorMessage\":\"Native get_info requires building with ZS_DWF_ENABLE_ODA_DWFTK=ON.\"," +
        "\"sourcePath\":\"" + json_escape_str(input_path) + "\"," +
        "\"kind\":1,\"isZipBased\":false,\"pages\":[],\"entries\":[],\"properties\":{}" +
        "}";
    return r;
#else
    try
    {
        DWFCore::DWFFile file(input_path.c_str());
        DWFToolkit::DWFPackageReader reader(file);
        DWFToolkit::DWFPackageReader::tPackageInfo info;
        reader.getPackageInfo(info);

        const bool is_package =
            info.eType == DWFToolkit::DWFPackageReader::eDWFPackage ||
            info.eType == DWFToolkit::DWFPackageReader::eDWFXPackage;

        std::ostringstream pages;
        int page_count = 0;

        if (is_package)
        {
            DWFToolkit::DWFManifest& manifest = reader.getManifest();
            DWFToolkit::DWFManifest::SectionIterator* sections = manifest.getSections();
            if (sections)
            {
                for (; sections->valid(); sections->next())
                {
                    DWFToolkit::DWFSection* section = sections->get();
                    if (!section)
                        continue;

                    const std::string title = to_utf8(section->title());
                    DWFToolkit::DWFResource* w2d = first_w2d_resource(*section);
                    const std::string href = w2d ? to_utf8(w2d->href()) : std::string();

                    if (page_count > 0)
                        pages << ",";
                    pages << "{"
                          << "\"pageIndex\":" << page_count << ","
                          << "\"pageName\":\"" << json_escape_str(title) << "\","
                          << "\"graphicsResourcePath\":\"" << json_escape_str(href) << "\","
                          << "\"has2dGraphics\":" << (w2d ? "true" : "false") << ","
                          << "\"unit\":\"dwf_internal\""
                          << "}";
                    ++page_count;
                }
                DWFCORE_FREE_OBJECT(sections);
            }
        }

        InfoResult r;
        r.success = is_package;
        r.result_code = is_package ? 0 : 1004;
        std::ostringstream json;
        json << "{"
             << "\"success\":" << (is_package ? "true" : "false") << ","
             << "\"errorCode\":\"" << (is_package ? "ok" : "unsupported_format") << "\","
             << "\"errorMessage\":" << (is_package ? "null" : "\"Input is not a DWF/DWFx package\"") << ","
             << "\"sourcePath\":\"" << json_escape_str(input_path) << "\","
             << "\"kind\":1,"
             << "\"isZipBased\":true,"
             << "\"pages\":[" << pages.str() << "],"
             << "\"entries\":[],"
             << "\"properties\":{}"
             << "}";
        r.json = json.str();
        return r;
    }
    catch (const DWFCore::DWFException& ex)
    {
        InfoResult r;
        r.success = false;
        r.result_code = 1007;
        r.json = std::string("{") +
            "\"success\":false,\"errorCode\":\"dwf_toolkit_exception\"," +
            "\"errorMessage\":\"" + json_escape_str(narrow(ex.type()) + ": " + narrow(ex.message())) + "\"," +
            "\"sourcePath\":\"" + json_escape_str(input_path) + "\"," +
            "\"kind\":1,\"isZipBased\":true,\"pages\":[],\"entries\":[],\"properties\":{}}";
        return r;
    }
    catch (const std::exception& ex)
    {
        InfoResult r;
        r.success = false;
        r.result_code = 1999;
        r.json = std::string("{") +
            "\"success\":false,\"errorCode\":\"native_exception\"," +
            "\"errorMessage\":\"" + json_escape_str(ex.what()) + "\"," +
            "\"sourcePath\":\"" + json_escape_str(input_path) + "\"," +
            "\"kind\":1,\"isZipBased\":true,\"pages\":[],\"entries\":[],\"properties\":{}}";
        return r;
    }
#endif
}

} // namespace zs::dwf::oda
