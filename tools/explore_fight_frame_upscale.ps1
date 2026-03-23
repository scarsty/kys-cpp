param(
    [Parameter(Mandatory = $true)]
    [string]$ZipPath,
    [string]$OutRoot = "work\game-dev\resource\fight\_explore",
    [int]$Scale = 8,
    [int]$PreviewFrame = -1,
    [string]$Scale2xExecutable = "work\scale2x-4.0-windows-x86\scalex.exe"
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-Stem([string]$Path) {
    return [System.IO.Path]::GetFileNameWithoutExtension($Path)
}

function Ensure-Directory([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Read-IndexKa([string]$Path) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if (($bytes.Length % 4) -ne 0) {
        throw "index.ka size must be divisible by 4: $Path"
    }
    $pairs = New-Object System.Collections.Generic.List[object]
    for ($i = 0; $i -lt $bytes.Length; $i += 4) {
        $dx = [BitConverter]::ToInt16($bytes, $i)
        $dy = [BitConverter]::ToInt16($bytes, $i + 2)
        $pairs.Add([PSCustomObject]@{
                dx = [int]$dx
                dy = [int]$dy
            })
    }
    return $pairs
}

function Write-IndexKa([string]$Path, $Pairs, [int]$ScaleFactor) {
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
    try {
        $writer = New-Object System.IO.BinaryWriter($stream)
        try {
            foreach ($pair in $Pairs) {
                $writer.Write([int16]($pair.dx * $ScaleFactor))
                $writer.Write([int16]($pair.dy * $ScaleFactor))
            }
        }
        finally {
            $writer.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

function Get-InterpolationMode([string]$Method) {
    switch ($Method) {
        "nearest" { return [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor }
        "bicubic" { return [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic }
        default { throw "Unknown method: $Method" }
    }
}

function Get-PixelOffsetMode([string]$Method) {
    switch ($Method) {
        "nearest" { return [System.Drawing.Drawing2D.PixelOffsetMode]::Half }
        "bicubic" { return [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality }
        default { throw "Unknown method: $Method" }
    }
}

function Resize-Bitmap([System.Drawing.Bitmap]$Source, [int]$Width, [int]$Height, [string]$Method) {
    $result = New-Object System.Drawing.Bitmap $Width, $Height
    $graphics = [System.Drawing.Graphics]::FromImage($result)
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.InterpolationMode = Get-InterpolationMode $Method
        $graphics.PixelOffsetMode = Get-PixelOffsetMode $Method
        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.DrawImage($Source, 0, 0, $Width, $Height)
    }
    finally {
        $graphics.Dispose()
    }
    return $result
}

function Invoke-Scale2x([string]$ExecutablePath, [string]$InputPath, [string]$OutputPath, [int]$ScaleFactor) {
    if (-not (Test-Path -LiteralPath $ExecutablePath)) {
        throw "Scale2x executable not found: $ExecutablePath"
    }

    $outputDir = Split-Path -Parent $OutputPath
    Ensure-Directory $outputDir

    & $ExecutablePath -k $ScaleFactor $InputPath $OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "Scale2x failed for $InputPath -> $OutputPath (exit code $LASTEXITCODE)"
    }
}

function Get-FrameFiles([string]$DirPath) {
    return Get-ChildItem -LiteralPath $DirPath -Filter "*.png" |
        Sort-Object { [int]$_.BaseName }
}

function Choose-PreviewFrame($FrameFiles) {
    $bestIndex = 0
    $bestScore = -1
    for ($i = 0; $i -lt $FrameFiles.Count; $i++) {
        $bitmap = [System.Drawing.Bitmap]::FromFile($FrameFiles[$i].FullName)
        try {
            $score = $bitmap.Width * $bitmap.Height
            if ($score -gt $bestScore) {
                $bestScore = $score
                $bestIndex = [int]$FrameFiles[$i].BaseName
            }
        }
        finally {
            $bitmap.Dispose()
        }
    }
    return $bestIndex
}

function Copy-Metadata([string]$SourceDir, [string]$TargetDir) {
    foreach ($name in @("fightframe.txt", "fightframe.ka")) {
        $sourcePath = Join-Path $SourceDir $name
        if (Test-Path -LiteralPath $sourcePath) {
            Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $TargetDir $name) -Force
        }
    }
}

function Export-ScaledVariant([string]$SourceDir, [string]$TargetDir, [int]$ScaleFactor, [string]$Method, [string]$Scale2xExecutable) {
    Ensure-Directory $TargetDir
    $frameFiles = Get-FrameFiles $SourceDir
    foreach ($frame in $frameFiles) {
        $outputPath = Join-Path $TargetDir $frame.Name
        switch ($Method) {
            "nearest" {
                $bitmap = [System.Drawing.Bitmap]::FromFile($frame.FullName)
                try {
                    $resized = Resize-Bitmap $bitmap ($bitmap.Width * $ScaleFactor) ($bitmap.Height * $ScaleFactor) $Method
                    try {
                        $resized.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
                    }
                    finally {
                        $resized.Dispose()
                    }
                }
                finally {
                    $bitmap.Dispose()
                }
            }
            "bicubic" {
                $bitmap = [System.Drawing.Bitmap]::FromFile($frame.FullName)
                try {
                    $resized = Resize-Bitmap $bitmap ($bitmap.Width * $ScaleFactor) ($bitmap.Height * $ScaleFactor) $Method
                    try {
                        $resized.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
                    }
                    finally {
                        $resized.Dispose()
                    }
                }
                finally {
                    $bitmap.Dispose()
                }
            }
            "scale2x" {
                Invoke-Scale2x $Scale2xExecutable $frame.FullName $outputPath $ScaleFactor
            }
            default {
                throw "Unknown method: $Method"
            }
        }
    }

    Copy-Metadata $SourceDir $TargetDir
    $indexPairs = Read-IndexKa (Join-Path $SourceDir "index.ka")
    Write-IndexKa (Join-Path $TargetDir "index.ka") $indexPairs $ScaleFactor
}

function Compress-Directory([string]$SourceDir, [string]$ZipFilePath) {
    if (Test-Path -LiteralPath $ZipFilePath) {
        Remove-Item -LiteralPath $ZipFilePath -Force
    }
    [System.IO.Compression.ZipFile]::CreateFromDirectory($SourceDir, $ZipFilePath)
}

function New-LabelFont([float]$Size, [System.Drawing.FontStyle]$Style = [System.Drawing.FontStyle]::Regular) {
    foreach ($name in @("Consolas", "Segoe UI", "Arial")) {
        try {
            return New-Object System.Drawing.Font($name, $Size, $Style)
        }
        catch {
        }
    }
    throw "No usable font found"
}

function Draw-CheckBackground($Graphics, [int]$X, [int]$Y, [int]$Width, [int]$Height, [int]$Step) {
    $brushA = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 40, 43, 51))
    $brushB = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 32, 35, 42))
    try {
        for ($yy = 0; $yy -lt $Height; $yy += $Step) {
            for ($xx = 0; $xx -lt $Width; $xx += $Step) {
                $brush = if (((($xx / $Step) + ($yy / $Step)) % 2) -eq 0) { $brushA } else { $brushB }
                $Graphics.FillRectangle($brush, $X + $xx, $Y + $yy, $Step, $Step)
            }
        }
    }
    finally {
        $brushA.Dispose()
        $brushB.Dispose()
    }
}

function Create-FrameSheet([string]$SourceDir, [string]$OutPath) {
    $frameFiles = Get-FrameFiles $SourceDir
    $cols = 6
    $cellWidth = 160
    $cellHeight = 170
    $rows = [int][Math]::Ceiling($frameFiles.Count / $cols)
    $sheetWidth = $cols * $cellWidth
    $sheetHeight = $rows * $cellHeight
    $sheet = New-Object System.Drawing.Bitmap $sheetWidth, $sheetHeight
    $graphics = [System.Drawing.Graphics]::FromImage($sheet)
    $labelFont = New-LabelFont 11
    $labelBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(230, 232, 236, 240))
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb(255, 20, 22, 28))
        for ($i = 0; $i -lt $frameFiles.Count; $i++) {
            $col = $i % $cols
            $row = [int][Math]::Floor($i / $cols)
            $x = $col * $cellWidth
            $y = $row * $cellHeight
            Draw-CheckBackground $graphics $x $y $cellWidth $cellHeight 10

            $bitmap = [System.Drawing.Bitmap]::FromFile($frameFiles[$i].FullName)
            try {
                $scale = [Math]::Min(4.0, [Math]::Min(($cellWidth - 20) / [double]$bitmap.Width, ($cellHeight - 28) / [double]$bitmap.Height))
                $width = [int][Math]::Round($bitmap.Width * $scale)
                $height = [int][Math]::Round($bitmap.Height * $scale)
                $targetX = $x + [int](($cellWidth - $width) / 2)
                $targetY = $y + $cellHeight - $height - 10
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
                $graphics.DrawImage($bitmap, $targetX, $targetY, $width, $height)
            }
            finally {
                $bitmap.Dispose()
            }

            $graphics.DrawString($frameFiles[$i].BaseName, $labelFont, $labelBrush, $x + 6, $y + 4)
        }
    }
    finally {
        $labelBrush.Dispose()
        $labelFont.Dispose()
        $graphics.Dispose()
    }
    $sheet.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $sheet.Dispose()
}

function Draw-PanelSprite($Graphics, [string]$ImagePath, [int]$PanelX, [int]$PanelY, [int]$PanelWidth, [int]$PanelHeight) {
    Draw-CheckBackground $Graphics $PanelX $PanelY $PanelWidth $PanelHeight 12
    $groundPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(130, 190, 200, 210), 2)
    try {
        $Graphics.DrawLine($groundPen, $PanelX + 28, $PanelY + $PanelHeight - 48, $PanelX + $PanelWidth - 28, $PanelY + $PanelHeight - 48)
    }
    finally {
        $groundPen.Dispose()
    }

    $bitmap = [System.Drawing.Bitmap]::FromFile($ImagePath)
    try {
        $targetX = $PanelX + [int](($PanelWidth - $bitmap.Width) / 2)
        $targetY = $PanelY + $PanelHeight - $bitmap.Height - 48
        $Graphics.DrawImage($bitmap, $targetX, $targetY, $bitmap.Width, $bitmap.Height)
    }
    finally {
        $bitmap.Dispose()
    }
}

function Create-PreviewImage(
    [string]$OutPath,
    [string]$Title,
    [string]$Subtitle,
    $Panels
) {
    $width = 1280
    $height = 720
    $preview = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($preview)
    $titleFont = New-LabelFont 28 ([System.Drawing.FontStyle]::Bold)
    $panelFont = New-LabelFont 16 ([System.Drawing.FontStyle]::Bold)
    $metaFont = New-LabelFont 12
    $titleBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 244, 246, 248))
    $metaBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(220, 210, 218, 230))
    $panelBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(235, 236, 240, 245))
    $gradient = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        ([System.Drawing.Rectangle]::new(0, 0, $width, $height)),
        [System.Drawing.Color]::FromArgb(255, 16, 20, 28),
        [System.Drawing.Color]::FromArgb(255, 28, 36, 48),
        65.0
    )
    try {
        $graphics.FillRectangle($gradient, 0, 0, $width, $height)
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.DrawString($Title, $titleFont, $titleBrush, 48, 34)
        $graphics.DrawString($Subtitle, $metaFont, $metaBrush, 50, 78)

        $panelY = 150
        $panelWidth = 360
        $panelHeight = 500
        $panelGap = 40
        $startX = 60

        for ($i = 0; $i -lt $Panels.Count; $i++) {
            $panelX = $startX + $i * ($panelWidth + $panelGap)
            $graphics.DrawString($Panels[$i].label, $panelFont, $panelBrush, $panelX, 118)

            $displayScale = [int]$Panels[$i].scale
            if ($displayScale -gt 1) {
                $bitmap = [System.Drawing.Bitmap]::FromFile($Panels[$i].image)
                try {
                    $scaled = Resize-Bitmap $bitmap ($bitmap.Width * $displayScale) ($bitmap.Height * $displayScale) "nearest"
                    $tempPath = Join-Path ([System.IO.Path]::GetDirectoryName($OutPath)) ("_preview_tmp_{0}_{1}.png" -f $i, [System.IO.Path]::GetFileNameWithoutExtension($OutPath))
                    try {
                        $scaled.Save($tempPath, [System.Drawing.Imaging.ImageFormat]::Png)
                    }
                    finally {
                        $scaled.Dispose()
                    }
                    try {
                        Draw-PanelSprite $graphics $tempPath $panelX $panelY $panelWidth $panelHeight
                    }
                    finally {
                        if (Test-Path -LiteralPath $tempPath) {
                            Remove-Item -LiteralPath $tempPath -Force
                        }
                    }
                }
                finally {
                    $bitmap.Dispose()
                }
            }
            else {
                Draw-PanelSprite $graphics $Panels[$i].image $panelX $panelY $panelWidth $panelHeight
            }
        }
    }
    finally {
        $gradient.Dispose()
        $panelBrush.Dispose()
        $metaBrush.Dispose()
        $titleBrush.Dispose()
        $metaFont.Dispose()
        $panelFont.Dispose()
        $titleFont.Dispose()
        $graphics.Dispose()
    }
    $preview.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $preview.Dispose()
}

$workspaceRoot = Split-Path $PSScriptRoot -Parent
$resolvedScale2xExecutable = if ([System.IO.Path]::IsPathRooted($Scale2xExecutable)) { $Scale2xExecutable } else { Join-Path $workspaceRoot $Scale2xExecutable }

$resolvedInput = (Resolve-Path $ZipPath).Path
$inputItem = Get-Item -LiteralPath $resolvedInput
$resolvedOutRoot = if (Test-Path -LiteralPath $OutRoot) { (Resolve-Path $OutRoot).Path } else { [System.IO.Path]::GetFullPath($OutRoot) }
Ensure-Directory $resolvedOutRoot

$stem = Get-Stem $resolvedInput
if ($inputItem.PSIsContainer -and $stem.EndsWith("_src")) {
    $stem = $stem.Substring(0, $stem.Length - 4)
}
$jobRoot = Join-Path $resolvedOutRoot "${stem}_upscale"
Ensure-Directory $jobRoot

$sourceDir = $null
if ($inputItem.PSIsContainer) {
    $sourceDir = $resolvedInput
}
else {
    $extractDir = Join-Path $jobRoot "source"
    if (Test-Path -LiteralPath $extractDir) {
        Remove-Item -LiteralPath $extractDir -Recurse -Force
    }
    Ensure-Directory $extractDir
    [System.IO.Compression.ZipFile]::ExtractToDirectory($resolvedInput, $extractDir)
    $sourceDir = $extractDir
}

$frameFiles = Get-FrameFiles $sourceDir
if ($frameFiles.Count -eq 0) {
    throw "No png frames found in $resolvedInput"
}

$previewId = if ($PreviewFrame -ge 0) { $PreviewFrame } else { Choose-PreviewFrame $frameFiles }

$nearestDir = Join-Path $jobRoot "nearest_x$Scale"
$bicubicDir = Join-Path $jobRoot "bicubic_x$Scale"
$scale2xDir = Join-Path $jobRoot "scale2x_x2"
Export-ScaledVariant $sourceDir $nearestDir $Scale "nearest" $resolvedScale2xExecutable
Export-ScaledVariant $sourceDir $bicubicDir $Scale "bicubic" $resolvedScale2xExecutable
Export-ScaledVariant $sourceDir $scale2xDir 2 "scale2x" $resolvedScale2xExecutable

$nearestZip = Join-Path $jobRoot "${stem}_nearest_x${Scale}.zip"
$bicubicZip = Join-Path $jobRoot "${stem}_bicubic_x${Scale}.zip"
$scale2xZip = Join-Path $jobRoot "${stem}_scale2x_x2.zip"
Compress-Directory $nearestDir $nearestZip
Compress-Directory $bicubicDir $bicubicZip
Compress-Directory $scale2xDir $scale2xZip

$sheetPath = Join-Path $jobRoot "${stem}_frames_x4.png"
Create-FrameSheet $sourceDir $sheetPath

$scale2xSheetPath = Join-Path $jobRoot "${stem}_scale2x_frames_x2.png"
Create-FrameSheet $scale2xDir $scale2xSheetPath

$previewPath = Join-Path $jobRoot "${stem}_preview_1280x720.png"
$previewPanels = @(
    @{ label = "Source x4 (current pixels)"; image = (Join-Path $sourceDir "$previewId.png"); scale = 4 }
    @{ label = "Upscaled x$Scale nearest"; image = (Join-Path $nearestDir "$previewId.png"); scale = 1 }
    @{ label = "Upscaled x$Scale bicubic"; image = (Join-Path $bicubicDir "$previewId.png"); scale = 1 }
)
Create-PreviewImage -OutPath $previewPath -Title "fight001 upscale study" -Subtitle "Target mockup: 1280x720  |  Selected frame: $previewId  |  Source bundle: $stem" -Panels $previewPanels

$scale2xPreviewPath = Join-Path $jobRoot "${stem}_scale2x_preview_1280x720.png"
$scale2xPreviewPanels = @(
    @{ label = "Source x4 (current pixels)"; image = (Join-Path $sourceDir "$previewId.png"); scale = 4 }
    @{ label = "Nearest x2"; image = (Join-Path $sourceDir "$previewId.png"); scale = 2 }
    @{ label = "Scale2x x2"; image = (Join-Path $scale2xDir "$previewId.png"); scale = 1 }
)
Create-PreviewImage -OutPath $scale2xPreviewPath -Title "fight001 scale2x study" -Subtitle "Target mockup: 1280x720  |  Selected frame: $previewId  |  Source bundle: $stem" -Panels $scale2xPreviewPanels

[PSCustomObject]@{
    source = $resolvedInput
    output_root = $jobRoot
    preview_frame = $previewId
    frame_count = $frameFiles.Count
    preview_png = $previewPath
    frame_sheet_png = $sheetPath
    scale2x_preview_png = $scale2xPreviewPath
    scale2x_frame_sheet_png = $scale2xSheetPath
    nearest_zip = $nearestZip
    bicubic_zip = $bicubicZip
    scale2x_zip = $scale2xZip
} | ConvertTo-Json -Depth 4
