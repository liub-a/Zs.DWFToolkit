using System.IO.Compression;
using System.Xml.Linq;
using Zs.DWFToolkit.Abstractions;
using Zs.DWFToolkit.Internal;
using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Services;

/// <summary>
/// Reads package-level information from DWF/DWFx/XPS files.
/// This class intentionally uses heuristics because DWF variants differ by producer.
/// </summary>
public sealed class DwfDocumentReader : IDwfDocumentReader
{
    public Task<DwfDocumentInfo> ReadInfoAsync(string sourcePath, CancellationToken cancellationToken = default)
    {
        if (string.IsNullOrWhiteSpace(sourcePath))
        {
            return Task.FromResult(Fail(sourcePath, DwfErrorCodes.InvalidArgument, "sourcePath is required."));
        }

        if (!File.Exists(sourcePath))
        {
            return Task.FromResult(Fail(sourcePath, DwfErrorCodes.FileNotFound, "File not found."));
        }

        try
        {
            var extKind = FileKindDetector.Detect(sourcePath);
            var isZip = FileKindDetector.IsZipBased(sourcePath);
            var kind = FileKindDetector.DetectFromZipEntries(sourcePath, extKind);

            var info = new DwfDocumentInfo
            {
                Success = true,
                ErrorCode = DwfErrorCodes.Ok,
                SourcePath = sourcePath,
                Kind = kind,
                IsZipBased = isZip
            };

            if (!isZip)
            {
                info.Success = false;
                info.ErrorCode = DwfErrorCodes.UnsupportedFormat;
                info.ErrorMessage = "The file is not a ZIP/OPC style DWF package. Native DWF Toolkit integration may be required.";
                return Task.FromResult(info);
            }

            using var archive = ZipFile.OpenRead(sourcePath);
            info.Entries = archive.Entries
                .Where(e => !string.IsNullOrEmpty(e.Name))
                .Select(PackageEntryClassifier.ToInfo)
                .OrderBy(e => e.FullName, StringComparer.OrdinalIgnoreCase)
                .ToList();

            foreach (var kv in TryReadContentTypes(archive))
                info.Properties[kv.Key] = kv.Value;

            info.Pages = DiscoverPages(archive, info.Entries, kind);
            AttachThumbnails(info);

            return Task.FromResult(info);
        }
        catch (InvalidDataException ex)
        {
            return Task.FromResult(Fail(sourcePath, DwfErrorCodes.InvalidPackage, ex.Message));
        }
        catch (Exception ex)
        {
            return Task.FromResult(Fail(sourcePath, DwfErrorCodes.InvalidPackage, ex.ToString()));
        }
    }

    private static DwfDocumentInfo Fail(string? sourcePath, string code, string message) => new()
    {
        Success = false,
        ErrorCode = code,
        ErrorMessage = message,
        SourcePath = sourcePath ?? string.Empty,
        Kind = sourcePath == null ? DwfFileKind.Unknown : FileKindDetector.Detect(sourcePath)
    };

    private static Dictionary<string, string> TryReadContentTypes(ZipArchive archive)
    {
        var dict = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var entry = archive.GetEntry("[Content_Types].xml");
        if (entry == null) return dict;

        try
        {
            using var s = entry.Open();
            var doc = XDocument.Load(s);
            foreach (var el in doc.Descendants())
            {
                var ext = el.Attribute("Extension")?.Value;
                var part = el.Attribute("PartName")?.Value;
                var contentType = el.Attribute("ContentType")?.Value;
                if (!string.IsNullOrWhiteSpace(ext) && !string.IsNullOrWhiteSpace(contentType))
                    dict[$"ext:{ext}"] = contentType!;
                if (!string.IsNullOrWhiteSpace(part) && !string.IsNullOrWhiteSpace(contentType))
                    dict[$"part:{part}"] = contentType!;
            }
        }
        catch
        {
            // Content type metadata is optional for our inspection flow.
        }
        return dict;
    }

    private static List<DwfPageInfo> DiscoverPages(ZipArchive archive, List<DwfPackageEntryInfo> entries, DwfFileKind kind)
    {
        var pages = new List<DwfPageInfo>();

        if (kind is DwfFileKind.Dwfx or DwfFileKind.Xps)
        {
            var fixedPages = entries.Where(e => e.IsPossibleXpsPage).ToList();
            for (var i = 0; i < fixedPages.Count; i++)
            {
                var entryInfo = fixedPages[i];
                var page = new DwfPageInfo
                {
                    PageIndex = i,
                    PageName = Path.GetFileNameWithoutExtension(entryInfo.FullName),
                    GraphicsResourcePath = entryInfo.FullName,
                    IsXpsFixedPage = true,
                    Has2dGraphics = true,
                    Unit = "xps_dip"
                };

                var zipEntry = archive.GetEntry(entryInfo.FullName);
                if (zipEntry != null)
                    TryFillXpsPageSize(zipEntry, page);

                pages.Add(page);
            }
        }

        if (pages.Count == 0)
        {
            var w2d = entries.Where(e => e.IsPossibleW2d).ToList();
            for (var i = 0; i < w2d.Count; i++)
            {
                pages.Add(new DwfPageInfo
                {
                    PageIndex = i,
                    PageName = Path.GetFileNameWithoutExtension(w2d[i].FullName),
                    GraphicsResourcePath = w2d[i].FullName,
                    Has2dGraphics = true,
                    Unit = "dwf_internal"
                });
            }
        }

        // 3D DWF (eModel/.w3d) has no 2D graphics this lightweight reader can preview;
        // surface it explicitly so conversion can return unsupported_3d_dwf instead of a
        // generic failure.
        if (pages.Count == 0)
        {
            var w3d = entries.Where(e => e.IsPossibleW3d).ToList();
            for (var i = 0; i < w3d.Count; i++)
            {
                pages.Add(new DwfPageInfo
                {
                    PageIndex = i,
                    PageName = Path.GetFileNameWithoutExtension(w3d[i].FullName),
                    GraphicsResourcePath = w3d[i].FullName,
                    Has3dGraphics = true,
                    Unit = "dwf_internal"
                });
            }
        }

        if (pages.Count == 0)
        {
            // Some DWF files only expose images/thumbnails to this lightweight reader.
            var images = entries.Where(e => e.IsImage).ToList();
            for (var i = 0; i < images.Count; i++)
            {
                pages.Add(new DwfPageInfo
                {
                    PageIndex = i,
                    PageName = Path.GetFileNameWithoutExtension(images[i].FullName),
                    ThumbnailResourcePath = images[i].FullName,
                    Unit = "image"
                });
            }
        }

        return pages;
    }

    private static void TryFillXpsPageSize(ZipArchiveEntry entry, DwfPageInfo page)
    {
        try
        {
            using var s = entry.Open();
            var doc = XDocument.Load(s);
            var root = doc.Root;
            if (root == null) return;

            if (double.TryParse(root.Attribute("Width")?.Value, out var w)) page.Width = w;
            if (double.TryParse(root.Attribute("Height")?.Value, out var h)) page.Height = h;
        }
        catch
        {
            // keep unknown
        }
    }

    private static void AttachThumbnails(DwfDocumentInfo info)
    {
        var thumbs = info.Entries.Where(e => e.IsPossibleThumbnail).ToList();
        if (thumbs.Count == 0)
            thumbs = info.Entries.Where(e => e.IsImage).Take(info.Pages.Count == 0 ? 1 : info.Pages.Count).ToList();

        for (var i = 0; i < info.Pages.Count && i < thumbs.Count; i++)
        {
            info.Pages[i].ThumbnailResourcePath ??= thumbs[i].FullName;
        }
    }
}
