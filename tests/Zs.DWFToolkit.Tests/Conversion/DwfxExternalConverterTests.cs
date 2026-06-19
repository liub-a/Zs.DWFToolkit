using FluentAssertions;
using Zs.DWFToolkit.Abstractions;
using Zs.DWFToolkit.Conversion;
using Zs.DWFToolkit.Models;
using Xunit;

namespace Zs.DWFToolkit.Tests.Conversion;

public sealed class DwfxExternalConverterTests
{
    private sealed class FakeNativeRenderer : INativeDwfRenderer
    {
        public bool IsAvailable { get; init; }
        public int RenderCalls { get; private set; }

        public Task<DwfDocumentInfo> GetInfoAsync(string sourcePath, CancellationToken ct = default)
            => Task.FromResult(new DwfDocumentInfo { Success = false });

        public Task<DwfRenderResult> RenderPageAsync(string sourcePath, int pageIndex, string outputPath, DwfRenderOptions options, CancellationToken ct = default)
        {
            RenderCalls++;
            return Task.FromResult(new DwfRenderResult { Success = false, ErrorCode = DwfErrorCodes.NativeRenderFailed });
        }

        public Task<DwfRenderResult> RenderW2dFileAsync(string sourcePath, string outputPath, DwfRenderOptions options, CancellationToken ct = default)
            => Task.FromResult(new DwfRenderResult { Success = false });
    }

    [Fact]
    public async Task ConvertToImagesAsync_ReturnsFileNotFound_WhenSourceMissing()
    {
        var converter = new DwfxExternalConverter(nativeRenderer: new FakeNativeRenderer());

        var result = await converter.ConvertToImagesAsync("/no/file.dwf", Path.GetTempPath());

        result.Success.Should().BeFalse();
        result.ErrorCode.Should().Be(DwfErrorCodes.FileNotFound);
    }

    [Fact]
    public async Task ConvertToImagesAsync_ReturnsUnsupported_ForPlainDwf_WhenNativeUnavailable()
    {
        var dwf = TestData.WritePackage(".dwf", new Dictionary<string, byte[]>
        {
            ["sheet.w2d"] = new byte[] { 1, 2, 3, 4 }
        });
        var outDir = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        var converter = new DwfxExternalConverter(nativeRenderer: new FakeNativeRenderer { IsAvailable = false });
        try
        {
            var result = await converter.ConvertToImagesAsync(dwf, outDir, new DwfRenderOptions { PreferNativeDwfRenderer = true });

            result.Success.Should().BeFalse();
            result.ErrorCode.Should().Be(DwfErrorCodes.UnsupportedDwfRendering);
        }
        finally { File.Delete(dwf); if (Directory.Exists(outDir)) Directory.Delete(outDir, true); }
    }

    [Fact]
    public async Task ConvertToImagesAsync_ReturnsUnsupported3dDwf_ForW3dOnlyPackage()
    {
        var dwf = TestData.WritePackage(".dwf", new Dictionary<string, byte[]>
        {
            ["model.w3d"] = new byte[] { 9, 9, 9 }
        });
        var outDir = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        var converter = new DwfxExternalConverter(nativeRenderer: new FakeNativeRenderer { IsAvailable = true });
        try
        {
            var result = await converter.ConvertToImagesAsync(dwf, outDir, new DwfRenderOptions { PreferNativeDwfRenderer = true });

            result.Success.Should().BeFalse();
            result.ErrorCode.Should().Be(DwfErrorCodes.Unsupported3dDwf);
        }
        finally { File.Delete(dwf); if (Directory.Exists(outDir)) Directory.Delete(outDir, true); }
    }
}
