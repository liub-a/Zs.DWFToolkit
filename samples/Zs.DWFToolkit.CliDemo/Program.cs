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
                case "extract":
                    await ExtractAsync();
                    break;
                case "copy":
                    await CopyAsync();
                    break;
                case "replace-entry":
                    await ReplaceEntryAsync();
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

    private async Task ExtractAsync()
    {
        var input = RequireArg(1, "input file");
        var outDir = GetOption("--out-dir") ?? GetOption("--out") ?? Path.Combine(Environment.CurrentDirectory, "dwf-extract");
        var result = await _toolkit.ExtractAllAsync(input, outDir);
        Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        Environment.ExitCode = result.Success ? 0 : 1;
    }

    private async Task CopyAsync()
    {
        var input = RequireArg(1, "input file");
        var output = GetOption("--out") ?? throw new ArgumentException("--out is required.");
        var result = await _toolkit.CopyPackageAsync(input, output);
        Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        Environment.ExitCode = result.Success ? 0 : 1;
    }

    private async Task ReplaceEntryAsync()
    {
        var input = RequireArg(1, "input file");
        var output = GetOption("--out") ?? throw new ArgumentException("--out is required.");
        var entry = GetOption("--entry") ?? throw new ArgumentException("--entry is required.");
        var file = GetOption("--file") ?? throw new ArgumentException("--file is required.");
        var result = await _toolkit.ReplaceEntryAsync(input, output, entry, file);
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
        options.ExtractThumbnailFallback = !HasFlag("--no-thumb-fallback");
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
  Zs.DWFToolkit.CliDemo to-images <file.dwfx|file.dwf> --out-dir <dir> [--dpi 200] [--format png] [--mutool /path/mutool] [--native]
  Zs.DWFToolkit.CliDemo to-pdf <file.dwfx> --out <output.pdf> [--mutool /path/mutool] [--gxps /path/gxps]
  Zs.DWFToolkit.CliDemo extract <file.dwf|file.dwfx> --out-dir <dir>
  Zs.DWFToolkit.CliDemo copy <file.dwf|file.dwfx> --out <copy.dwf>
  Zs.DWFToolkit.CliDemo replace-entry <file.dwf|file.dwfx> --entry <package/path> --file <replacement> --out <new.dwf>

Notes:
  - DWFx/XPS image/PDF conversion uses external tools such as MuPDF mutool or GhostXPS.
  - Plain DWF conversion needs a real native DWF/W2D renderer. The included native project is a bridge/stub.
  - Plain DWF fallback can extract embedded thumbnail images.
""");
    }
}
