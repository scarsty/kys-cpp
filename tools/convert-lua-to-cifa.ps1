# convert-lua-to-cifa.ps1
# 将 script/event/ka*.lua 转换为 cifa 风格，输出到 script/event-cifa/<number>.c
# 使用方法（从项目根目录运行）：
#   .\tools\convert-lua-to-cifa.ps1 -InputDir "D:\...\script\event" -OutputDir "D:\...\script\event-cifa"
#
# 转换规则：
#   if COND then   ->  if (COND)\n{
#   end; / end     ->  }
#   or / and       ->  || / &&
#   -- comment     ->  // comment
#   文件名: ka<N>.lua -> <N>.c
#   格式：前花括号单独一行，缩进4个空格

param(
    [Parameter(Mandatory=$true)]
    [string]$InputDir,
    [Parameter(Mandatory=$true)]
    [string]$OutputDir
)

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$files = Get-ChildItem $InputDir -Filter "ka*.lua" | Sort-Object {
    [int]($_.BaseName -replace 'ka', '')
}

$count = 0
foreach ($file in $files) {
    $num = $file.BaseName -replace 'ka', ''
    $outFile = Join-Path $OutputDir "$num.c"

    # 读取 UTF-8 源文件
    $content = [System.IO.File]::ReadAllText($file.FullName, [System.Text.Encoding]::UTF8)

    # ---- 结构转换 ----

    # 1. if COND then  →  if (COND)（暂不加花括号，格式化阶段统一处理）
    $content = [regex]::Replace($content, '(?m)^(\s*)if\s+(.+?)\s+then\s*$', '$1if ($2)')

    # 2. end; 或 end (单独成行)  →  }
    $content = [regex]::Replace($content, '(?m)^\s*end\s*;?\s*$', '}')

    # 3. Lua bool operators
    $content = $content.Replace(' or ',  ' || ')
    $content = $content.Replace(' and ', ' && ')

    # 3.1 if (expr == false) -> if (!expr)
    $content = [regex]::Replace($content, '(?m)^(\s*)if\s*\(\s*(.+?)\s*==\s*false\s*\)\s*$', '$1if (!$2)')

    # 4. Lua comment --  ->  //
    $content = $content.Replace('--', '//')

    # ---- 格式化：前花括号独行 + 4空格缩进 ----
    $lines  = $content -split '\r?\n'
    $depth  = 0
    $outLines = [System.Collections.Generic.List[string]]::new()

    foreach ($line in $lines) {
        $t = $line.Trim()
        if ($t -eq '') {
            $outLines.Add('')
            continue
        }
        if ($t -eq '}') {
            if ($depth -gt 0) { $depth-- }
            $outLines.Add(('    ' * $depth) + '}')
            continue
        }
        # if 语句：先输出条件行，再输出独立的 {
        if ($t -match '^if \(') {
            $outLines.Add(('    ' * $depth) + $t)
            $outLines.Add(('    ' * $depth) + '{')
            $depth++
            continue
        }
        $outLines.Add(('    ' * $depth) + $t)
    }

    $content = $outLines -join "`n"

    # 写出 UTF-8（无 BOM）
    [System.IO.File]::WriteAllText($outFile, $content, $utf8NoBom)
    $count++
}

Write-Host "Done: $count files -> $OutputDir"
