namespace Zs.DWFToolkit.Models;

public static class DwfErrorCodes
{
    public const string Ok = "ok";
    public const string FileNotFound = "file_not_found";
    public const string InvalidArgument = "invalid_argument";
    public const string InvalidPackage = "invalid_package";
    public const string UnsupportedFormat = "unsupported_format";
    public const string UnsupportedDwfRendering = "unsupported_dwf_rendering";
    public const string Unsupported3dDwf = "unsupported_3d_dwf";
    public const string ExternalToolNotFound = "external_tool_not_found";
    public const string ExternalToolFailed = "external_tool_failed";
    public const string NativeLibraryNotFound = "native_library_not_found";
    public const string NativeRenderFailed = "native_render_failed";
    public const string Timeout = "timeout";
    public const string OutputFailed = "output_failed";
    public const string NoThumbnail = "no_thumbnail";
}
