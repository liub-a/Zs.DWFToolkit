param(
    [string]$Archive
)

$ErrorActionPreference = "Stop"
$RootDir = Resolve-Path (Join-Path $PSScriptRoot "..")
$DefaultArchive = Join-Path $RootDir "third_party/archives/DWFToolkit-7.7-src-ODA-essential.tar.gz"
if ([string]::IsNullOrWhiteSpace($Archive)) {
    $Archive = $DefaultArchive
}

$Target = Join-Path $RootDir "third_party/DWFToolkit-7.7"
$TmpDir = Join-Path $RootDir "third_party/.tmp-dwftk-import"

if (!(Test-Path $Archive)) {
    throw "Archive not found: $Archive. Usage: ./scripts/setup-oda-dwftoolkit.ps1 -Archive C:\path\DWFToolkit-7.7-src-ODA.zip"
}

if (Test-Path $Target) { Remove-Item $Target -Recurse -Force }
if (Test-Path $TmpDir) { Remove-Item $TmpDir -Recurse -Force }

if ($Archive.EndsWith(".tar.gz") -or $Archive.EndsWith(".tgz")) {
    tar -xzf $Archive -C (Join-Path $RootDir "third_party")
}
elseif ($Archive.EndsWith(".zip")) {
    New-Item -ItemType Directory -Path $TmpDir | Out-Null
    Expand-Archive -Path $Archive -DestinationPath $TmpDir -Force
    $Extracted = Join-Path $TmpDir "DWFToolkit-7.7"
    if (!(Test-Path $Extracted)) {
        throw "Expected DWFToolkit-7.7 folder inside zip."
    }
    Move-Item $Extracted $Target
    Remove-Item $TmpDir -Recurse -Force
}
else {
    throw "Unsupported archive type: $Archive"
}

Write-Host "ODA-modified DWFToolkit extracted to: $Target"
