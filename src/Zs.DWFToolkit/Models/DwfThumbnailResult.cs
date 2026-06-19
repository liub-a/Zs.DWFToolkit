namespace Zs.DWFToolkit.Models;

public sealed class DwfThumbnailResult
{
    public bool Success { get; set; }
    public string? ErrorCode { get; set; }
    public string? ErrorMessage { get; set; }
    public string? ResourcePath { get; set; }
    public string? OutputPath { get; set; }
    public long BytesWritten { get; set; }
}
