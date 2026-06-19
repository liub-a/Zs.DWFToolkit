namespace Zs.DWFToolkit.Models;

public sealed class DwfRenderResult
{
    public bool Success { get; set; }
    public string? ErrorCode { get; set; }
    public string? ErrorMessage { get; set; }
    public string SourcePath { get; set; } = string.Empty;
    public int? PageIndex { get; set; }
    public string? OutputPath { get; set; }
    public string? OutputDirectory { get; set; }
    public List<string> OutputFiles { get; set; } = new();
    public DwfPageTransform? Transform { get; set; }
    public string? ToolName { get; set; }
    public string? StdOut { get; set; }
    public string? StdErr { get; set; }
}
