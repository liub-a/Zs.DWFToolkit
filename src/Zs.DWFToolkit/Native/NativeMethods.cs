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

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "zs_dwf_get_last_error")]
    internal static extern IntPtr GetLastErrorPtr();

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
