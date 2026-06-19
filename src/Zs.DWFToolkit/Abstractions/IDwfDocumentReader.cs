using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Abstractions;

public interface IDwfDocumentReader
{
    Task<DwfDocumentInfo> ReadInfoAsync(string sourcePath, CancellationToken cancellationToken = default);
}
