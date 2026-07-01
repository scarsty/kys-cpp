[CmdletBinding()]
param(
    [ValidateSet('All', 'Android', 'Windows', 'Wasm')]
    [string[]]$Target = @('All'),

    [string]$SourceIcon,
    [string]$AndroidResRoot,
    [string]$WindowsIcon,
    [string]$WasmRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($SourceIcon))
{
    $SourceIcon = Join-Path $repoRoot 'assets\app_icon.png'
}
if ([string]::IsNullOrWhiteSpace($AndroidResRoot))
{
    $AndroidResRoot = Join-Path $repoRoot 'android\app\src\main\res'
}
if ([string]::IsNullOrWhiteSpace($WindowsIcon))
{
    $WindowsIcon = Join-Path $repoRoot 'src\kys.ico'
}
if ([string]::IsNullOrWhiteSpace($WasmRoot))
{
    $WasmRoot = Join-Path $repoRoot 'wasm'
}

Add-Type -AssemblyName System.Drawing

function Test-IconTarget
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    return $Target -contains 'All' -or $Target -contains $Name
}

function New-ParentDirectory
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent))
    {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
}

function Invoke-WithResizedBitmap
{
    param(
        [Parameter(Mandatory = $true)]
        [System.Drawing.Image]$SourceImage,

        [Parameter(Mandatory = $true)]
        [int]$Size,

        [Parameter(Mandatory = $true)]
        [scriptblock]$Action
    )

    $bitmap = [System.Drawing.Bitmap]::new($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try
    {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try
        {
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.DrawImage($SourceImage, 0, 0, $Size, $Size)
        }
        finally
        {
            $graphics.Dispose()
        }

        return & $Action $bitmap
    }
    finally
    {
        $bitmap.Dispose()
    }
}

function Save-ResizedPng
{
    param(
        [Parameter(Mandatory = $true)]
        [System.Drawing.Image]$SourceImage,

        [Parameter(Mandatory = $true)]
        [int]$Size,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    New-ParentDirectory -Path $Path
    Invoke-WithResizedBitmap -SourceImage $SourceImage -Size $Size -Action {
        param([System.Drawing.Bitmap]$Bitmap)
        $Bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }

    Write-Host "Wrote $Path (${Size}x${Size})"
}

function New-ResizedPngBytes
{
    param(
        [Parameter(Mandatory = $true)]
        [System.Drawing.Image]$SourceImage,

        [Parameter(Mandatory = $true)]
        [int]$Size
    )

    $pngBytes = Invoke-WithResizedBitmap -SourceImage $SourceImage -Size $Size -Action {
        param([System.Drawing.Bitmap]$Bitmap)
        $stream = [System.IO.MemoryStream]::new()
        try
        {
            $Bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
            return ,$stream.ToArray()
        }
        finally
        {
            $stream.Dispose()
        }
    }

    return ,[byte[]]$pngBytes
}

function Write-IcoFile
{
    param(
        [Parameter(Mandatory = $true)]
        [System.Drawing.Image]$SourceImage,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $sizes = @(256, 128, 96, 72, 64, 48, 32, 24, 16)
    $frames = foreach ($size in $sizes)
    {
        [pscustomobject]@{
            Size = $size
            Bytes = (New-ResizedPngBytes -SourceImage $SourceImage -Size $size)
        }
    }

    New-ParentDirectory -Path $Path
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
    try
    {
        $writer = [System.IO.BinaryWriter]::new($stream)
        try
        {
            $writer.Write([uint16]0)
            $writer.Write([uint16]1)
            $writer.Write([uint16]$frames.Count)

            $offset = 6 + (16 * $frames.Count)
            foreach ($frame in $frames)
            {
                $directorySize = if ($frame.Size -eq 256) { 0 } else { $frame.Size }
                $bytes = [byte[]]$frame.Bytes
                $writer.Write([byte]$directorySize)
                $writer.Write([byte]$directorySize)
                $writer.Write([byte]0)
                $writer.Write([byte]0)
                $writer.Write([uint16]0)
                $writer.Write([uint16]32)
                $writer.Write([uint32]$bytes.Length)
                $writer.Write([uint32]$offset)
                $offset += $bytes.Length
            }

            foreach ($frame in $frames)
            {
                $writer.Write([byte[]]$frame.Bytes)
            }
        }
        finally
        {
            $writer.Dispose()
        }
    }
    finally
    {
        $stream.Dispose()
    }

    Write-Host "Wrote $Path"
}

function Write-AndroidIcons
{
    param(
        [Parameter(Mandatory = $true)]
        [System.Drawing.Image]$SourceImage
    )

    $targets = @(
        @{ Folder = 'mipmap-mdpi'; Size = 48 },
        @{ Folder = 'mipmap-hdpi'; Size = 72 },
        @{ Folder = 'mipmap-xhdpi'; Size = 96 },
        @{ Folder = 'mipmap-xxhdpi'; Size = 144 },
        @{ Folder = 'mipmap-xxxhdpi'; Size = 192 },
        @{ Folder = 'drawable'; File = 'ic_launcher_foreground.png'; Size = 432 }
    )

    foreach ($targetInfo in $targets)
    {
        $outputDir = Join-Path $AndroidResRoot $targetInfo.Folder
        $fileName = if ($targetInfo.ContainsKey('File')) { $targetInfo.File } else { 'ic_launcher.png' }
        Save-ResizedPng -SourceImage $SourceImage -Size $targetInfo.Size -Path (Join-Path $outputDir $fileName)
    }
}

function Write-WasmIcons
{
    param(
        [Parameter(Mandatory = $true)]
        [System.Drawing.Image]$SourceImage
    )

    Save-ResizedPng -SourceImage $SourceImage -Size 32 -Path (Join-Path $WasmRoot 'favicon.png')
    Save-ResizedPng -SourceImage $SourceImage -Size 180 -Path (Join-Path $WasmRoot 'apple-touch-icon.png')
    Save-ResizedPng -SourceImage $SourceImage -Size 192 -Path (Join-Path $WasmRoot 'icon-192.png')
    Save-ResizedPng -SourceImage $SourceImage -Size 512 -Path (Join-Path $WasmRoot 'icon-512.png')

    $manifestPath = Join-Path $WasmRoot 'site.webmanifest'
    $manifest = @'
{
  "name": "\u91d1\u7fa4\u81ea\u8d70\u68cb",
  "short_name": "\u91d1\u7fa4\u81ea\u8d70\u68cb",
  "display": "fullscreen",
  "background_color": "#000000",
  "theme_color": "#000000",
  "icons": [
    {
      "src": "icon-192.png",
      "sizes": "192x192",
      "type": "image/png"
    },
    {
      "src": "icon-512.png",
      "sizes": "512x512",
      "type": "image/png"
    }
  ]
}
'@

    [System.IO.File]::WriteAllText($manifestPath, $manifest + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
    Write-Host "Wrote $manifestPath"
}

if (-not (Test-Path $SourceIcon))
{
    throw "Source icon not found: $SourceIcon"
}

$resolvedSource = (Resolve-Path $SourceIcon).Path
$sourceImage = [System.Drawing.Image]::FromFile($resolvedSource)
try
{
    if (Test-IconTarget -Name 'Android')
    {
        Write-AndroidIcons -SourceImage $sourceImage
    }

    if (Test-IconTarget -Name 'Windows')
    {
        Write-IcoFile -SourceImage $sourceImage -Path $WindowsIcon
    }

    if (Test-IconTarget -Name 'Wasm')
    {
        Write-WasmIcons -SourceImage $sourceImage
    }
}
finally
{
    $sourceImage.Dispose()
}
