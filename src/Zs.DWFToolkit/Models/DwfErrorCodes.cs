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

    /// <summary>Package inspection failed (corrupt/unreadable DWF/DWFx structure).</summary>
    public const string ReadFailed = "read_failed";

    // Codes emitted by the native W2D/DWF renderer; declared here so managed and
    // native layers share one vocabulary when surfacing native errors.
    public const string PageNotFound = "page_not_found";
    public const string W2dResourceNotFound = "w2d_resource_not_found";
    public const string W2dExtractFailed = "w2d_extract_failed";
    public const string W2dParseFailed = "w2d_parse_failed";
    public const string W2dEmptyBounds = "w2d_empty_bounds";
    public const string W2dRenderFailed = "w2d_render_failed";
    public const string DwfToolkitException = "dwf_toolkit_exception";
    public const string NativeException = "native_exception";
}
