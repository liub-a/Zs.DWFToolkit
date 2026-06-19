namespace Zs.DWFToolkit.Internal;

internal static class SafePath
{
    public static void EnsureParentDirectory(string path)
    {
        var dir = Path.GetDirectoryName(Path.GetFullPath(path));
        if (!string.IsNullOrWhiteSpace(dir))
            Directory.CreateDirectory(dir);
    }

    public static void EnsureDirectory(string path)
    {
        Directory.CreateDirectory(Path.GetFullPath(path));
    }
}
