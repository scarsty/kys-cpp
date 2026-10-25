import gzip
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from wasm.compress_wasm import compress_wasm


class WasmCompressionTests(unittest.TestCase):
    def test_gzip_is_reproducible_and_preserves_the_executable(self):
        with tempfile.TemporaryDirectory(dir=ROOT / 'tmp') as directory:
            source = Path(directory) / 'game.wasm'
            binary = b'\0asm\1\0\0\0' + bytes(range(256)) * 20
            source.write_bytes(binary)
            output = compress_wasm(source)
            compressed = output.read_bytes()
            self.assertEqual(gzip.decompress(compressed), binary)
            self.assertEqual(source.read_bytes(), binary)
            self.assertLess(len(compressed), len(binary))
            self.assertEqual(compress_wasm(source).read_bytes(), compressed)

    def test_deployment_validators_require_gzip_and_reject_raw_and_oversized_executables(self):
        with tempfile.TemporaryDirectory(dir=ROOT / 'tmp') as directory:
            script = Path(directory) / 'validate.ps1'
            script.write_text(r'''
param([string]$Repository, [string]$Stage)
$ErrorActionPreference = 'Stop'
. (Join-Path $Repository 'wasm/WasmCommon.ps1')
Add-Type -AssemblyName System.IO.Compression.FileSystem
$dist = Join-Path $Stage 'dist'
foreach ($name in Get-WasmDeploymentArtifactNames) {
    $path = Join-Path $dist $name
    New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
    [IO.File]::WriteAllText($path, 'fixture kyschess.html')
}
function Check-Package([bool]$ShouldPass) {
    $zip = Join-Path $Stage "$([guid]::NewGuid()).zip"
    [IO.Compression.ZipFile]::CreateFromDirectory($dist, $zip)
    foreach ($check in @(
        { Assert-WasmDeploymentDirectory -DistDir $dist },
        { Assert-WasmDeploymentArchive -PackagePath $zip }
    )) {
        $passed = $true
        try { & $check } catch { $passed = $false }
        if ($passed -ne $ShouldPass) { throw 'Unexpected deployment validation result' }
    }
}
Check-Package $true
$raw = Join-Path $dist 'kyschess.wasm'
[IO.File]::WriteAllText($raw, 'raw executable')
Check-Package $false
Remove-Item -LiteralPath $raw
$gzip = [IO.File]::OpenWrite((Join-Path $dist 'kyschess.wasm.gz'))
$gzip.SetLength(25MB + 1)
$gzip.Dispose()
Check-Package $false
''', encoding='utf-8')
            completed = subprocess.run(
                ['pwsh', '-NoProfile', '-File', str(script), str(ROOT), directory],
                text=True, encoding='utf-8', capture_output=True,
            )
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)


if __name__ == '__main__':
    unittest.main()
