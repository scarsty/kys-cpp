from __future__ import annotations

import importlib.util
import struct
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_PATH = REPO_ROOT / "tools" / "pack_chess_smap_zip.py"


def load_packer_module():
    tools_path = str(SCRIPT_PATH.parent)
    if tools_path not in sys.path:
        sys.path.insert(0, tools_path)
    spec = importlib.util.spec_from_file_location("pack_chess_smap_zip", SCRIPT_PATH)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def battle_info_record(module, battle_id: int, battlefield_id: int) -> bytes:
    record = bytearray(module.BATTLE_INFO_RECORD_SIZE)
    struct.pack_into("<h", record, module.BATTLE_INFO_ID_OFFSET, battle_id)
    struct.pack_into(
        "<h",
        record,
        module.BATTLE_INFO_BATTLEFIELD_ID_OFFSET,
        battlefield_id,
    )
    return bytes(record)


class PackChessSmapZipTests(unittest.TestCase):
    def test_resolves_curated_catalog_ids_from_packaged_war_sta(self) -> None:
        module = load_packer_module()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            catalog_path = root / "ChessBattleMapCatalog.cpp"
            catalog_path.write_text(
                """
const std::vector<ChessBattleMapCatalogEntry>& ChessBattleMapCatalog::entries()
{
    static const std::vector<ChessBattleMapCatalogEntry> maps{
        {
            13,
            20,
            {{25, 27}},
            {{26, 25}},
            "十面埋伏",
        },
        {
            6,
            10,
            {{30, 39}},
            {{31, 39}},
            "迷洞巷戰",
        },
    };
    return maps;
}
""".strip(),
                encoding="utf-8",
            )

            war_sta_path = root / "war.sta"
            payload = b"".join(
                [
                    battle_info_record(module, 6, 16),
                    battle_info_record(module, 13, 4),
                    battle_info_record(module, 99, 7),
                    battle_info_record(module, 6, 99),
                ]
            )
            with zipfile.ZipFile(root / "war.sta.zip", "w") as archive:
                archive.writestr("war.sta", payload)

            resolved = module.resolve_curated_battlefields(catalog_path, war_sta_path)

        self.assertEqual(resolved, [(13, 4), (6, 16)])

    def test_reports_catalog_ids_missing_from_war_sta(self) -> None:
        module = load_packer_module()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            catalog_path = root / "ChessBattleMapCatalog.cpp"
            catalog_path.write_text(
                """
static const std::vector<ChessBattleMapCatalogEntry> maps{
    {
        21,
        10,
        {{33, 27}},
        {{36, 26}},
        "庭院圍戰",
    },
};
return maps;
""".strip(),
                encoding="utf-8",
            )
            war_sta_path = root / "war.sta"
            war_sta_path.write_bytes(battle_info_record(module, 6, 16))

            with self.assertRaisesRegex(RuntimeError, r"missing from .*war\.sta: 21"):
                module.resolve_curated_battlefields(catalog_path, war_sta_path)


if __name__ == "__main__":
    unittest.main()
