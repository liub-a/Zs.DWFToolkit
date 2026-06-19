using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Abstractions;

public interface IDwfPackageWriter
{
    Task<DwfPackageWriteResult> ExtractAllAsync(string sourcePath, string outputDirectory, CancellationToken cancellationToken = default);

    Task<DwfPackageWriteResult> CopyPackageAsync(string sourcePath, string destinationPath, CancellationToken cancellationToken = default);

    Task<DwfPackageWriteResult> ReplaceEntryAsync(
        string sourcePath,
        string destinationPath,
        string entryName,
        string replacementFilePath,
        CancellationToken cancellationToken = default);
}
