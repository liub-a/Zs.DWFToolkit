namespace Zs.DWFToolkit.Models;

public sealed class DwfDocumentInfo
{
    public bool Success { get; set; }
    public string? ErrorCode { get; set; }
    public string? ErrorMessage { get; set; }
    public string SourcePath { get; set; } = string.Empty;
    public DwfFileKind Kind { get; set; }
    public bool IsZipBased { get; set; }
    public int PageCount => Pages.Count;
    public List<DwfPageInfo> Pages { get; set; } = new();
    public List<DwfPackageEntryInfo> Entries { get; set; } = new();
    public Dictionary<string, string> Properties { get; set; } = new(StringComparer.OrdinalIgnoreCase);
}
