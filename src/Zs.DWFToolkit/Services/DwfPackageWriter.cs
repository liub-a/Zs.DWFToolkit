using System.IO.Compression;
using Zs.DWFToolkit.Abstractions;
using Zs.DWFToolkit.Internal;
using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Services;

/// <summary>
/// Low-level ZIP/OPC package writer for DWF/DWFx packages.
/// This is not semantic CAD editing. It is useful for extraction, copying and controlled resource replacement.
/// </summary>
public sealed class DwfPackageWriter : IDwfPackageWriter
{
    public async Task<DwfPackageWriteResult> ExtractAllAsync(
        string sourcePath,
        string outputDirectory,
        CancellationToken cancellationToken = default)
    {
        if (!File.Exists(sourcePath)) return Fail(sourcePath, null, DwfErrorCodes.FileNotFound, "File not found.");
        if (!FileKindDetector.IsZipBased(sourcePath)) return Fail(sourcePath, null, DwfErrorCodes.UnsupportedFormat, "Only ZIP/OPC style packages can be extracted by this writer.");

        var root = Path.GetFullPath(outputDirectory);
        var rootWithSeparator = root.EndsWith(Path.DirectorySeparatorChar) ? root : root + Path.DirectorySeparatorChar;
        Directory.CreateDirectory(root);
        var written = new List<string>();

        try
        {
            using var archive = ZipFile.OpenRead(sourcePath);
            foreach (var entry in archive.Entries.Where(e => !string.IsNullOrEmpty(e.Name)))
            {
                cancellationToken.ThrowIfCancellationRequested();
                var target = Path.GetFullPath(Path.Combine(root, entry.FullName.Replace('/', Path.DirectorySeparatorChar)));
                if (!target.Equals(root, StringComparison.OrdinalIgnoreCase) && !target.StartsWith(rootWithSeparator, StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException($"Unsafe package entry path: {entry.FullName}");

                Directory.CreateDirectory(Path.GetDirectoryName(target)!);
                await using var input = entry.Open();
                await using var output = File.Create(target);
                await input.CopyToAsync(output, cancellationToken).ConfigureAwait(false);
                written.Add(target);
            }

            return new DwfPackageWriteResult
            {
                Success = true,
                ErrorCode = DwfErrorCodes.Ok,
                SourcePath = sourcePath,
                OutputPath = outputDirectory,
                EntriesWritten = written.Count,
                WrittenEntries = written
            };
        }
        catch (Exception ex)
        {
            return Fail(sourcePath, outputDirectory, DwfErrorCodes.OutputFailed, ex.ToString());
        }
    }

    public async Task<DwfPackageWriteResult> CopyPackageAsync(
        string sourcePath,
        string destinationPath,
        CancellationToken cancellationToken = default)
    {
        if (!File.Exists(sourcePath)) return Fail(sourcePath, destinationPath, DwfErrorCodes.FileNotFound, "File not found.");
        SafePath.EnsureParentDirectory(destinationPath);
        await using var input = File.OpenRead(sourcePath);
        await using var output = File.Create(destinationPath);
        await input.CopyToAsync(output, cancellationToken).ConfigureAwait(false);
        return new DwfPackageWriteResult
        {
            Success = true,
            ErrorCode = DwfErrorCodes.Ok,
            SourcePath = sourcePath,
            OutputPath = destinationPath,
            EntriesWritten = 1,
            WrittenEntries = new List<string> { destinationPath }
        };
    }

    public async Task<DwfPackageWriteResult> ReplaceEntryAsync(
        string sourcePath,
        string destinationPath,
        string entryName,
        string replacementFilePath,
        CancellationToken cancellationToken = default)
    {
        if (!File.Exists(sourcePath)) return Fail(sourcePath, destinationPath, DwfErrorCodes.FileNotFound, "Source file not found.");
        if (!File.Exists(replacementFilePath)) return Fail(sourcePath, destinationPath, DwfErrorCodes.FileNotFound, "Replacement file not found.");
        if (!FileKindDetector.IsZipBased(sourcePath)) return Fail(sourcePath, destinationPath, DwfErrorCodes.UnsupportedFormat, "Only ZIP/OPC style packages can be modified by this writer.");

        var normalized = entryName.Replace('\\', '/').TrimStart('/');
        SafePath.EnsureParentDirectory(destinationPath);
        var written = new List<string>();

        try
        {
            using var inputArchive = ZipFile.OpenRead(sourcePath);
            using var outputArchive = ZipFile.Open(destinationPath, ZipArchiveMode.Create);
            var replaced = false;

            foreach (var entry in inputArchive.Entries)
            {
                cancellationToken.ThrowIfCancellationRequested();
                var name = entry.FullName.Replace('\\', '/');
                var outEntry = outputArchive.CreateEntry(entry.FullName, CompressionLevel.Optimal);
                await using var outStream = outEntry.Open();

                if (string.Equals(name, normalized, StringComparison.OrdinalIgnoreCase))
                {
                    await using var replacement = File.OpenRead(replacementFilePath);
                    await replacement.CopyToAsync(outStream, cancellationToken).ConfigureAwait(false);
                    replaced = true;
                }
                else
                {
                    await using var inStream = entry.Open();
                    await inStream.CopyToAsync(outStream, cancellationToken).ConfigureAwait(false);
                }

                written.Add(entry.FullName);
            }

            if (!replaced)
            {
                var outEntry = outputArchive.CreateEntry(normalized, CompressionLevel.Optimal);
                await using var outStream = outEntry.Open();
                await using var replacement = File.OpenRead(replacementFilePath);
                await replacement.CopyToAsync(outStream, cancellationToken).ConfigureAwait(false);
                written.Add(normalized);
            }

            return new DwfPackageWriteResult
            {
                Success = true,
                ErrorCode = DwfErrorCodes.Ok,
                SourcePath = sourcePath,
                OutputPath = destinationPath,
                EntriesWritten = written.Count,
                WrittenEntries = written
            };
        }
        catch (Exception ex)
        {
            return Fail(sourcePath, destinationPath, DwfErrorCodes.OutputFailed, ex.ToString());
        }
    }

    private static DwfPackageWriteResult Fail(string sourcePath, string? outputPath, string code, string message) => new()
    {
        Success = false,
        ErrorCode = code,
        ErrorMessage = message,
        SourcePath = sourcePath,
        OutputPath = outputPath
    };
}
