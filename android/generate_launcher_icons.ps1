param(
    [string]$SourceIcon = (Join-Path $PSScriptRoot 'kys_chess_icon.png'),
    [string]$ResRoot = (Join-Path $PSScriptRoot 'app\src\main\res')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

$targets = @(
    @{ Folder = 'mipmap-mdpi'; Size = 48 },
    @{ Folder = 'mipmap-hdpi'; Size = 72 },
    @{ Folder = 'mipmap-xhdpi'; Size = 96 },
    @{ Folder = 'mipmap-xxhdpi'; Size = 144 },
    @{ Folder = 'mipmap-xxxhdpi'; Size = 192 },
    @{ Folder = 'drawable'; File = 'ic_launcher_foreground.png'; Size = 432 }
)

if (-not (Test-Path $SourceIcon)) {
    throw "Source icon not found: $SourceIcon"
}

$resolvedSource = (Resolve-Path $SourceIcon).Path
$sourceImage = [System.Drawing.Image]::FromFile($resolvedSource)

try {
    foreach ($target in $targets) {
        $outputDir = Join-Path $ResRoot $target.Folder
        $outputFile = if ($target.ContainsKey('File')) { Join-Path $outputDir $target.File } else { Join-Path $outputDir 'ic_launcher.png' }

        New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

        $bitmap = [System.Drawing.Bitmap]::new($target.Size, $target.Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([System.Drawing.Color]::Transparent)
                $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.DrawImage($sourceImage, 0, 0, $target.Size, $target.Size)
            } finally {
                $graphics.Dispose()
            }

            $bitmap.Save($outputFile, [System.Drawing.Imaging.ImageFormat]::Png)
        } finally {
            $bitmap.Dispose()
        }

        Write-Host "Wrote $outputFile ($($target.Size)x$($target.Size))"
    }
} finally {
    $sourceImage.Dispose()
}