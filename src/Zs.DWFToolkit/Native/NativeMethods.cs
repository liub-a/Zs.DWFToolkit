using System.Runtime.InteropServices;
using System.Text;

namespace Zs.DWFToolkit.Native;

internal static partial class NativeMethods
{
    private const string LibraryName = "zs_dwf_toolkit_native";

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "zs_dwf_get_info_json")]
    internal static extern int GetInfoJson(string inputPath, StringBuilder outputJson, int outputJsonSize);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "zs_dwf_render_page")]
    internal static extern int RenderPage(
        string inputPath,
        int pageIndex,
        string outputPath,
        int widthPx,
        int heightPx,
        int dpi,
        StringBuilder outputJson,
        int outputJsonSize);


    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "zs_w2d_render_file")]
    internal static extern int RenderW2dFile(
        string inputPath,
        string outputPath,
        int widthPx,
        int heightPx,
        int dpi,
        StringBuilder outputJson,
        int outputJsonSize);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "zs_dwf_get_last_error")]
    internal static extern IntPtr GetLastErrorPtr();

    /// <summary>
    /// Invokes the native library directly so a missing/incompatible binary surfaces
    /// as DllNotFoundException/BadImageFormatException to the caller. Unlike
    /// <see cref="GetLastErrorString"/> this does not swallow load failures.
    /// </summary>
    internal static void ProbeLoad() => _ = GetLastErrorPtr();

    internal static string? GetLastErrorString()
    {
        try
        {
            var ptr = GetLastErrorPtr();
            return ptr == IntPtr.Zero ? null : Marshal.PtrToStringAnsi(ptr);
        }
        catch
        {
            return null;
        }
    }
}
