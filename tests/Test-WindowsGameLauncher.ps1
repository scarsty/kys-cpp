param(
    [Parameter(Mandatory = $true)]
    [string]$LauncherPath,

    [Parameter(Mandatory = $true)]
    [string]$GameExecutablePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot '..\tools\ReleasePackagingCommon.ps1')

function Assert-Launcher
{
    param([string]$Package, [string]$WorkingDirectory, [string]$LauncherName)
    Start-Process -FilePath (Join-Path $Package $LauncherName) -WorkingDirectory $WorkingDirectory -Wait
    $resultPath = Join-Path $Package 'bin\kys.exe.txt'
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    while (-not (Test-Path -LiteralPath $resultPath) -and [DateTime]::UtcNow -lt $deadline)
    {
        Start-Sleep -Milliseconds 50
    }
    $result = [System.IO.File]::ReadAllLines($resultPath)
    if ($result.Count -ne 4 -or $result[0] -ne $Package -or $result[1] -ne '1' -or
        $result[2] -ne 'game' -or $result[3] -ne 'no-console')
    {
        throw "Launcher used the wrong executable, working directory, arguments, or console mode: $Package"
    }
    Remove-Item -LiteralPath $resultPath
}

$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('kys-launcher-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $testRoot | Out-Null
try
{
    $original = Join-Path $testRoot 'original'
    New-Item -ItemType Directory -Path (Join-Path $original 'bin') | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $original 'game') | Out-Null
    # Use a console application to verify the real game's launch mode too.
    $probeSource = @'
using System;
using System.ComponentModel;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
public class LauncherProbe
{
    [DllImport("kernel32.dll")]
    private static extern IntPtr GetConsoleWindow();
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr module);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr FindResourceW(IntPtr module, IntPtr name, IntPtr type);
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr LoadResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll")]
    private static extern IntPtr LockResource(IntPtr resource);
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint SizeofResource(IntPtr module, IntPtr resource);

    private static byte[] ResourceBytes(IntPtr module, int id, int type)
    {
        IntPtr resource = FindResourceW(module, new IntPtr(id), new IntPtr(type));
        if (resource == IntPtr.Zero) throw new Win32Exception();
        IntPtr loaded = LoadResource(module, resource);
        if (loaded == IntPtr.Zero) throw new Win32Exception();
        byte[] bytes = new byte[SizeofResource(module, resource)];
        Marshal.Copy(LockResource(loaded), bytes, 0, bytes.Length);
        return bytes;
    }

    public static string IconHash(string path)
    {
        // Load resources without loading the executable's DLL dependencies.
        IntPtr module = LoadLibraryExW(path, IntPtr.Zero, 0x22);
        if (module == IntPtr.Zero) throw new Win32Exception();
        try
        {
            using (var stream = new MemoryStream())
            using (var sha = SHA256.Create())
            {
                // IDI_ICON1 from the shared kys.rc; include every image size.
                byte[] group = ResourceBytes(module, 101, 14);
                stream.Write(group, 0, group.Length);
                int count = BitConverter.ToUInt16(group, 4);
                for (int i = 0; i < count; ++i)
                {
                    int id = BitConverter.ToUInt16(group, 6 + i * 14 + 12);
                    byte[] icon = ResourceBytes(module, id, 3);
                    stream.Write(icon, 0, icon.Length);
                }
                return Convert.ToBase64String(sha.ComputeHash(stream.ToArray()));
            }
        }
        finally { FreeLibrary(module); }
    }

    public static void Main(string[] args)
    {
        string result = Assembly.GetExecutingAssembly().Location + ".txt";
        File.WriteAllLines(result + ".tmp", new[] {
            Environment.CurrentDirectory, args.Length.ToString(), args[0],
            GetConsoleWindow() == IntPtr.Zero ? "no-console" : "console" });
        File.Move(result + ".tmp", result);
    }
}
'@
    Add-Type -TypeDefinition $probeSource
    Add-Type -TypeDefinition ($probeSource.Replace('LauncherProbe', 'GameProbe')) `
        -OutputAssembly (Join-Path $original 'bin\kys.exe') -OutputType ConsoleApplication

    $LauncherPath = [System.IO.Path]::GetFullPath($LauncherPath)
    $GameExecutablePath = [System.IO.Path]::GetFullPath($GameExecutablePath)
    if ([LauncherProbe]::IconHash($LauncherPath) -ne [LauncherProbe]::IconHash($GameExecutablePath))
    {
        throw 'Launcher icon resources differ from the game executable'
    }
    $launcherBytes = [System.IO.File]::ReadAllBytes($LauncherPath)
    $peOffset = [BitConverter]::ToInt32($launcherBytes, 0x3c)
    if ([BitConverter]::ToUInt16($launcherBytes, $peOffset + 92) -ne 2)
    {
        throw 'Launcher must use the Windows GUI subsystem'
    }

    $launcherName = '金群自走棋.exe'
    Copy-Item -LiteralPath $LauncherPath -Destination (Join-Path $original $launcherName)
    $zip = Join-Path $testRoot 'package.zip'
    New-ZipFromDirectory -SourceDir $original -ZipPath $zip
    $extracted = Join-Path $testRoot '解壓 空格 & (test) !'
    [System.IO.Compression.ZipFile]::ExtractToDirectory($zip, $extracted)

    foreach ($package in @($original, $extracted))
    {
        Assert-Launcher -Package $package -WorkingDirectory $testRoot -LauncherName $launcherName
    }

    # Relocate both packages so the original absolute target no longer exists.
    $relocated = Join-Path $testRoot 'relocated'
    Move-Item -LiteralPath $extracted -Destination $relocated
    Move-Item -LiteralPath $original -Destination (Join-Path $testRoot 'old-original')
    Assert-Launcher -Package $relocated -WorkingDirectory $testRoot -LauncherName $launcherName
    Write-Host 'Windows launcher tests passed (matching icons, no console, original, extracted alongside original, relocated).'
}
finally
{
    $resolvedRoot = [System.IO.Path]::GetFullPath($testRoot)
    $tempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolvedRoot.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase))
    {
        throw "Test cleanup escaped temporary directory: $resolvedRoot"
    }
    Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
}
