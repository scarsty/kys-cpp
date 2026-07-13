param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDir,

    [Parameter(Mandatory = $true)]
    [string]$Destination
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$source = [System.IO.Path]::GetFullPath($SourceDir).TrimEnd('\', '/')
if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "Source directory does not exist: $source"
}

$destinationDir = Split-Path -Parent $Destination
if ($destinationDir -and -not (Test-Path -LiteralPath $destinationDir -PathType Container)) {
    New-Item -ItemType Directory -Path $destinationDir | Out-Null
}

if (Test-Path -LiteralPath $Destination) {
    Remove-Item -LiteralPath $Destination -Force
}

$zip = [System.IO.Compression.ZipFile]::Open($Destination, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    Get-ChildItem -LiteralPath $source -File -Recurse | ForEach-Object {
        $entryName = $_.FullName.Substring($source.Length + 1).Replace('\', '/')
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $zip,
            $_.FullName,
            $entryName,
            [System.IO.Compression.CompressionLevel]::Fastest
        ) | Out-Null
    }
}
finally {
    $zip.Dispose()
}