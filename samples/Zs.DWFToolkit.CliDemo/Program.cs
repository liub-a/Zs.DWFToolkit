using System.Text.Json;
using Zs.DWFToolkit.Models;
using Zs.DWFToolkit.Services;

var app = new CliApp(args);
await app.RunAsync();

internal sealed class CliApp
{
    private readonly string[] _args;
    private readonly ZsDwfToolkit _toolkit = new();
    private static readonly JsonSerializerOptions JsonOptions = new() { WriteIndented = true };

    public CliApp(string[] args) => _args = args;

    public async Task RunAsync()
    {
        if (_args.Length == 0 || _args[0] is "-h" or "--help" or "help")
        {
            PrintHelp();
            return;
        }

        var command = _args[0].ToLowerInvariant();
        try
        {
            switch (command)
            {
                case "info":
                    await InfoAsync();
                    break;
                case "thumb":
                case "thumbnail":
                    await ThumbnailAsync();
                    break;
                case "to-images":
                case "images":
                    await ToImagesAsync();
                    break;
                case "to-pdf":
                case "pdf":
                    await ToPdfAsync();
                    break;
                default:
                    Console.Error.WriteLine($"Unknown command: {command}");
                    PrintHelp();
                    Environment.ExitCode = 2;
                    break;
            }
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex);
            Environment.ExitCode = 1;
        }
    }

    private async Task InfoAsync()
    {
        var input = RequireArg(1, "input file");
        var result = await _toolkit.ReadInfoAsync(input);
        Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        Environment.ExitCode = result.Success ? 0 : 1;
    }

    private async Task ThumbnailAsync()
    {
        var input = RequireArg(1, "input file");
        var output = GetOption("--out") ?? Path.Combine(Environment.CurrentDirectory, "thumbnail.bin");
        var result = await _toolkit.ExtractBestThumbnailAsync(input, output);
        Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        Environment.ExitCode = result.Success ? 0 : 1;
    }

    private async Task ToImagesAsync()
    {
        var input = RequireArg(1, "input file");
        var outDir = GetOption("--out-dir") ?? GetOption("--out") ?? Path.Combine(Environment.CurrentDirectory, "dwf-images");
        var options = ReadOptions();
        var result = await _toolkit.ConvertToImagesAsync(input, outDir, options);
        Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        Environment.ExitCode = result.Success ? 0 : 1;
    }

    private async Task ToPdfAsync()
    {
        var input = RequireArg(1, "input file");
        var output = GetOption("--out") ?? Path.ChangeExtension(input, ".pdf");
        var options = ReadOptions();
        var result = await _toolkit.ConvertToPdfAsync(input, output, options);
        Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        Environment.ExitCode = result.Success ? 0 : 1;
    }

    private DwfRenderOptions ReadOptions()
    {
        var options = new DwfRenderOptions();
        if (int.TryParse(GetOption("--dpi"), out var dpi)) options.Dpi = dpi;
        if (int.TryParse(GetOption("--width"), out var width)) options.WidthPx = width;
        if (int.TryParse(GetOption("--height"), out var height)) options.HeightPx = height;
        if (int.TryParse(GetOption("--timeout"), out var timeout)) options.Timeout = TimeSpan.FromSeconds(timeout);
        if (!string.IsNullOrWhiteSpace(GetOption("--format"))) options.ImageFormat = GetOption("--format")!;
        options.MutoolPath = GetOption("--mutool");
        options.GhostXpsPath = GetOption("--gxps");
        options.PreferNativeDwfRenderer = HasFlag("--native");
        options.CreatePdfFromImagesFallback = !HasFlag("--no-image-pdf-fallback");
        options.KeepTemporaryFiles = HasFlag("--keep-temp");
        if (long.TryParse(GetOption("--min-image-bytes"), out var minBytes)) options.RasterImageMinBytes = minBytes;
        if (double.TryParse(GetOption("--pdf-width"), out var pdfWidth)) options.PdfPageWidthPoints = pdfWidth;
        if (double.TryParse(GetOption("--pdf-height"), out var pdfHeight)) options.PdfPageHeightPoints = pdfHeight;
        if (double.TryParse(GetOption("--pdf-margin"), out var pdfMargin)) options.PdfMarginPoints = pdfMargin;
        return options;
    }

    private string RequireArg(int index, string name)
    {
        if (_args.Length <= index)
            throw new ArgumentException($"Missing {name}.");
        return _args[index];
    }

    private bool HasFlag(string name) => _args.Any(a => string.Equals(a, name, StringComparison.OrdinalIgnoreCase));

    private string? GetOption(string name)
    {
        for (var i = 0; i < _args.Length - 1; i++)
        {
            if (string.Equals(_args[i], name, StringComparison.OrdinalIgnoreCase))
                return _args[i + 1];
        }
        return null;
    }

    private static void PrintHelp()
    {
        Console.WriteLine("""
Zs.DWFToolkit.CliDemo

Usage:
  Zs.DWFToolkit.CliDemo info <file.dwf|file.dwfx>
  Zs.DWFToolkit.CliDemo thumbnail <file.dwf|file.dwfx> --out <thumb.jpg>
  Zs.DWFToolkit.CliDemo to-images <file.dwfx|file.dwf> --out-dir <dir> [--dpi 200] [--format png] [--mutool /path/mutool] [--gxps /path/gxps] [--native]
  Zs.DWFToolkit.CliDemo to-pdf <file.dwfx|file.dwf> --out <output.pdf> [--mutool /path/mutool] [--gxps /path/gxps] [--native]

Options:
  --no-image-pdf-fallback    Disable ordinary DWF native-rendered image-to-PDF assembly.
  --min-image-bytes <n>      Legacy diagnostic raster extractor threshold; not used by conversion by default.
  --pdf-width <points>       Fixed PDF page width for simple image PDF writer.
  --pdf-height <points>      Fixed PDF page height for simple image PDF writer.
  --pdf-margin <points>      Margin for simple image PDF writer.
  --keep-temp                Keep temporary images generated while creating PDF.

Notes:
  - DWFx/XPS image/PDF conversion uses external tools such as MuPDF mutool or GhostXPS/gxps.
  - Plain DWF conversion requires native rendering when --native is enabled.
  - Without a real native W2D/DWF renderer, ordinary DWF conversion fails instead of returning incomplete thumbnails/previews.
  - Use the thumbnail command explicitly when you only need a best-effort thumbnail.
""");
    }
}
