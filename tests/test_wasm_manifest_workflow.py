from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class WasmManifestWorkflowTests(unittest.TestCase):
    def test_every_wasm_asset_workflow_refreshes_the_manifest(self):
        for script_name in ("build.ps1", "rebuild.ps1", "package.ps1"):
            script = (ROOT / "wasm" / script_name).read_text(encoding="utf-8-sig")
            self.assertIn("Write-WasmAssetManifest", script, script_name)

    def test_package_refreshes_manifest_after_copying_release_assets(self):
        script = (ROOT / "wasm" / "package.ps1").read_text(encoding="utf-8-sig")
        copy_position = script.index("Copy-ReleaseGameAssets")
        manifest_position = script.index("Write-WasmAssetManifest")
        self.assertGreater(manifest_position, copy_position)


if __name__ == "__main__":
    unittest.main()
