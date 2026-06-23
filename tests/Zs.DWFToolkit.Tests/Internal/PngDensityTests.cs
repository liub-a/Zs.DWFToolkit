using System.Buffers.Binary;
using Xunit;
using Zs.DWFToolkit.Internal;

namespace Zs.DWFToolkit.Tests.Internal;

public class PngDensityTests
{
    [Fact]
    public void TrySetDpi_InsertsPhysChunkWithMatchingDensity()
    {
        var path = Path.Combine(Path.GetTempPath(), "zs-phys-" + Guid.NewGuid().ToString("N") + ".png");
        File.WriteAllBytes(path, TestData.BuildPng(100, 80));
        try
        {
            Assert.True(PngDensity.TrySetDpi(path, 150));

            var bytes = File.ReadAllBytes(path);
            var phys = FindChunk(bytes, "pHYs");
            Assert.NotNull(phys);

            var expectedPpu = (uint)Math.Round(150 / 0.0254);
            Assert.Equal(expectedPpu, BinaryPrimitives.ReadUInt32BigEndian(phys.AsSpan(0, 4)));
            Assert.Equal(expectedPpu, BinaryPrimitives.ReadUInt32BigEndian(phys.AsSpan(4, 4)));
            Assert.Equal(1, phys[8]); // unit = metres
        }
        finally
        {
            File.Delete(path);
        }
    }

    [Fact]
    public void TrySetDpi_ReturnsFalseForNonPng()
    {
        var path = Path.Combine(Path.GetTempPath(), "zs-phys-" + Guid.NewGuid().ToString("N") + ".jpg");
        File.WriteAllBytes(path, TestData.BuildJpeg(10, 10));
        try
        {
            Assert.False(PngDensity.TrySetDpi(path, 150));
        }
        finally
        {
            File.Delete(path);
        }
    }

    private static byte[]? FindChunk(byte[] png, string type)
    {
        var pos = 8; // signature
        while (pos + 8 <= png.Length)
        {
            var len = BinaryPrimitives.ReadInt32BigEndian(png.AsSpan(pos, 4));
            var t = System.Text.Encoding.ASCII.GetString(png, pos + 4, 4);
            if (t == type) return png.AsSpan(pos + 8, len).ToArray();
            pos += 8 + len + 4;
        }
        return null;
    }
}
