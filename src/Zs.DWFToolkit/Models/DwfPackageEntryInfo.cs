namespace Zs.DWFToolkit.Models;

public sealed record DwfPackageEntryInfo(
    string FullName,
    long Length,
    string Extension,
    bool IsImage,
    bool IsXml,
    bool IsPossibleThumbnail,
    bool IsPossibleW2d,
    bool IsPossibleXpsPage,
    bool IsPossibleW3d = false);
