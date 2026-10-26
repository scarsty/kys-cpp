#!/usr/bin/env python3
"""產生目前版本的整備畫面測試資料與隔離的 WASM 注入頁面。"""

import argparse
from copy import deepcopy
import html
import json
import os
from pathlib import Path
import subprocess

import yaml


ROOT = Path(__file__).resolve().parents[1]
NEW_GAME = {"difficulty": "hard", "seed": "0x0000000000000001", "detail": "compact"}


def run_cli(cli, data_root, requests):
    completed = subprocess.run(
        [str(cli), "--jsonl", "--data-root", str(data_root),
         "--config-root", str(ROOT / "config")],
        input="".join(json.dumps({"id": index, "method": method, "params": params},
                                 ensure_ascii=False) + "\n"
                      for index, (method, params) in enumerate(requests)),
        capture_output=True, text=True, encoding="utf-8", check=True, cwd=ROOT,
    )
    responses = [json.loads(line) for line in completed.stdout.splitlines()]
    if len(responses) != len(requests):
        raise RuntimeError(f"CLI 回應數量不符：{completed.stderr}")
    for response in responses:
        if not response["ok"]:
            raise RuntimeError(json.dumps(response, ensure_ascii=False))
    return [response["result"] for response in responses]


def fixture_page(shell, checkpoint, slug, asset_base):
    # 測試頁完全使用記憶體存檔，包含後續自動存檔也不碰 IndexedDB。
    mount = "FS.mount(IDBFS, {}, '/persist');"
    if shell.count(mount) != 1:
        raise RuntimeError("WASM 持久化掛載已變更，請更新測試頁產生器")
    page = shell.replace(mount, "FS.mount(MEMFS, {}, '/persist');")
    page = page.replace("<head>", f'<head>\n    <base href="{html.escape(asset_base, quote=True)}">', 1)
    settings = {"preparationGuideSeen": True, "simplifiedChinese": False,
                "musicVolume": 0, "soundVolume": 0}
    injection = """<script>
        const initializeGame = Module.onRuntimeInitialized;
        Module.onRuntimeInitialized = function() {
            FS.writeFile('/persist/4.json', SAVE_PAYLOAD);
            FS.writeFile('/persist/setting.json', SETTINGS_PAYLOAD);
            document.documentElement.dataset.chessUiFixture = FIXTURE_NAME;
            initializeGame.call(this);
        };
    </script>
    <script src="kyschess.js" async></script>"""
    slot = {"scene": {"inSubMap": 53, "subMapX": 21, "subMapY": 54, "faceTowards": 1},
            "checkpoint": checkpoint}
    for token, value in (("SAVE_PAYLOAD", json.dumps(slot, ensure_ascii=False)),
                         ("SETTINGS_PAYLOAD", json.dumps(settings)),
                         ("FIXTURE_NAME", slug)):
        injection = injection.replace(token, json.dumps(value, ensure_ascii=False).replace("<", "\\u003c"))
    marker = "{{{ SCRIPT }}}"
    if page.count(marker) != 1:
        raise RuntimeError("WASM 指令碼標記已變更，請更新測試頁產生器")
    return page.replace(marker, injection)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=ROOT / "x64/Debug/kys_chess_cli.exe")
    parser.add_argument("--data-root", type=Path, default=ROOT / "work/game-dev")
    parser.add_argument("--output", type=Path, default=ROOT / "tmp/chess-ui-fixtures")
    args = parser.parse_args()
    output = args.output.resolve()
    build = ROOT / "wasm/build"
    if not (build / "kyschess.wasm").is_file():
        raise RuntimeError("請先執行 wasm/build.ps1")
    shell = (ROOT / "wasm/shell.html").read_text(encoding="utf-8")
    combos = yaml.safe_load((ROOT / "config/chess_combos.yaml").read_text(encoding="utf-8"))["羈絆"]
    capacity_combo = next(combo for combo in combos if any(
        rule["類型"] == "出戰加員" for threshold in combo["閾值"]
        for rule in threshold.get("管理規則", [])))
    members = capacity_combo["成員"]
    threshold = next(level["人數"] for level in capacity_combo["閾值"] if any(
        rule["類型"] == "出戰加員" for rule in level.get("管理規則", [])))
    partial = capacity_combo["閾值"][0]["人數"]
    balance = yaml.safe_load((ROOT / "config/chess_balance_hard.yaml").read_text(encoding="utf-8"))
    equipment = yaml.safe_load((ROOT / "config/chess_equipment.yaml").read_text(encoding="utf-8"))["裝備列表"]
    challenges = yaml.safe_load((ROOT / "config/chess_challenge.yaml").read_text(encoding="utf-8"))["遠征挑戰"]

    _, _, exported = run_cli(args.cli.resolve(), args.data_root.resolve(), [
        ("new", NEW_GAME), ("save_game", {"slot": "ui"}), ("export_save", {"slot": "ui"})])
    base = exported["checkpoint"]
    roles = list(reversed(yaml.safe_load((ROOT / "config/chess_pool.yaml").read_text(encoding="utf-8"))["角色"]))
    others = [role_id for role_id in roles if role_id not in members]
    # 先由原生規則取得滿級容量，再產生門檻兩側及失效超員的畫面。
    base["state"]["level"] = balance["最高等級"]
    _, _, _, at_max = run_cli(args.cli.resolve(), args.data_root.resolve(), [
        ("new", NEW_GAME), ("import_save", {"slot": "ui", "checkpoint": base}),
        ("load_game", {"slot": "ui"}), ("observe", {"detail": "compact"})])
    capacity = at_max["game_state"]["base_deployment"]
    cases = [("no-zhijiang", 0, capacity, 0),
             ("partial-zhijiang", partial, capacity, 0),
             ("active-zhijiang", threshold, capacity + 1, 1),
             ("lost-zhijiang", threshold - 1, capacity + 1, 0),
             ("equipment-dense", threshold - 1, capacity + 1, 0)]
    output.mkdir(parents=True, exist_ok=True)
    asset_base = Path(os.path.relpath(build, output)).as_posix() + "/"
    summaries = []
    for slug, member_count, deployed, bonus in cases:
        checkpoint = deepcopy(base)
        state = checkpoint["state"]
        team = members[:member_count] + others[:deployed - member_count]
        bench = [role_id for role_id in roles if role_id not in team][:balance["背包上限"]]
        state.update(money=100, fight=12, roster={}, formationSlots=list(range(1, deployed + 1)),
                     seenRoleIds=sorted(set(team + bench)), nextChessInstanceId=len(team + bench) + 1)
        state["formationSlots"] += [-1] * (len(base["state"]["formationSlots"]) - deployed)
        for index, role_id in enumerate(team + bench, 1):
            state["roster"][str(index)] = {
                "instanceId": index, "roleId": role_id, "star": 3, "deployed": index <= deployed,
                "weaponInstanceId": -1, "armorInstanceId": -1, "fightsWon": 12,
                "luckStacks": 0, "lateBloomerChosen": False}
        if slug == "equipment-dense":
            # 雙類型皆超過一頁，並保留換裝提示、已分配標記與超員底列。
            inventory = state["equipmentInventory"] = {}
            for equipment_type, field in ((0, "weaponInstanceId"), (1, "armorInstanceId")):
                entries = [entry for entry in equipment if entry["裝備類型"] == equipment_type][:10]
                for index, entry in enumerate(entries):
                    instance_id = len(inventory) + 1
                    wearer = index + 1 if index < 2 else -1
                    inventory[str(instance_id)] = {
                        "instanceId": instance_id, "itemId": entry["裝備ID"],
                        "assignedChessInstanceId": wearer}
                    if wearer >= 0:
                        state["roster"][str(wearer)][field] = instance_id
            state["nextEquipmentInstanceId"] = len(inventory) + 1
            state["completedChallengeNames"] = [challenges[0]["名稱"]]
        # 重新保存，讓狀態雜湊、重播 footer 與序列化欄位由目前的原生程式產生。
        _, _, _, observed, _, exported = run_cli(args.cli.resolve(), args.data_root.resolve(), [
            ("new", NEW_GAME), ("import_save", {"slot": "ui", "checkpoint": checkpoint}),
            ("load_game", {"slot": "ui"}), ("observe", {"detail": "compact"}),
            ("save_game", {"slot": "ui", "label": f"UI 合成測試：{slug}"}),
            ("export_save", {"slot": "ui"})])
        observed = observed["game_state"]
        expected = (deployed, capacity + bonus, bonus, max(0, deployed - capacity - bonus))
        actual = tuple(observed[key] for key in (
            "deployed_count", "maximum_deployment", "deployment_bonus", "deployment_overflow"))
        if actual != expected:
            raise RuntimeError(f"{slug} 情境不符：預期 {expected}，實際 {actual}")
        checkpoint = exported["checkpoint"]
        (output / f"{slug}.json").write_text(json.dumps(checkpoint, ensure_ascii=False, indent=2), encoding="utf-8")
        (output / f"{slug}.html").write_text(fixture_page(shell, checkpoint, slug, asset_base), encoding="utf-8")
        summaries.append({"case": slug, "deployed": deployed, "capacity": capacity + bonus,
                          "bonus": bonus, "overflow": actual[3],
                          "bonus_sources": observed["deployment_bonus_sources"]})
        print(f"{slug}: {deployed}/{capacity + bonus}, +{bonus}, 超員 {actual[3]}")
    (output / "manifest.json").write_text(json.dumps(summaries, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"輸出：{output}")
    print("從專案根目錄啟動 HTTP server，再開啟輸出目錄中的 HTML；選擇繼續棋局進入整備，收起後可按 Q 重開。")


if __name__ == "__main__":
    main()
