namespace Zs.DWFToolkit.Models;

public sealed class DwfPackageWriteResult
{
    public bool Success { get; set; }
    public string? ErrorCode { get; set; }
    public string? ErrorMessage { get; set; }
    public string SourcePath { get; set; } = string.Empty;
    public string? OutputPath { get; set; }
    public int EntriesWritten { get; set; }
    public List<string> WrittenEntries { get; set; } = new();
}
