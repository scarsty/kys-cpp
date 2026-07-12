from __future__ import annotations

from collections import deque
import json
import os
from pathlib import Path
import subprocess
import threading
from typing import Annotated, Any, Literal

from pydantic import Field


Difficulty = Literal["easy", "normal", "hard"]
Detail = Literal["compact", "full"]
Seed = Annotated[str, Field(pattern=r"^0x[0-9a-fA-F]{16}$")]


def default_cli_path() -> Path:
    configured = os.environ.get("KYS_CHESS_CLI")
    if configured:
        return Path(configured)
    root = Path(__file__).resolve().parents[3]
    return root / "x64" / "Debug" / "kys_chess_cli.exe"


class CliSession:
    """Owns one protocol-clean CLI JSONL process."""

    def __init__(self, executable: Path | str | None = None, extra_args: list[str] | None = None):
        command = [str(executable or default_cli_path()), "--jsonl"]
        command.extend(extra_args or [])
        self._process = subprocess.Popen(
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            bufsize=1,
        )
        self._next_id = 1
        self._lock = threading.Lock()
        self._diagnostics: deque[str] = deque(maxlen=200)
        self._stderr_thread = threading.Thread(target=self._drain_stderr, daemon=True)
        self._stderr_thread.start()

    def _drain_stderr(self) -> None:
        assert self._process.stderr is not None
        for line in self._process.stderr:
            self._diagnostics.append(line.rstrip())

    def request(self, method: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
        with self._lock:
            if self._process.poll() is not None:
                raise RuntimeError(f"棋局程序已結束，exit={self._process.returncode}")
            request_id = self._next_id
            self._next_id += 1
            payload = {"id": request_id, "method": method, "params": params or {}}
            assert self._process.stdin is not None
            assert self._process.stdout is not None
            self._process.stdin.write(json.dumps(payload, ensure_ascii=False) + "\n")
            self._process.stdin.flush()
            line = self._process.stdout.readline()
            if not line:
                raise RuntimeError("棋局程序未傳回回應")
            response = json.loads(line)
            if response.get("id") != request_id:
                raise RuntimeError("棋局程序回應識別碼不相符")
            return response

    def diagnostics(self) -> list[str]:
        return list(self._diagnostics)

    def close(self) -> None:
        if self._process.poll() is not None:
            return
        if self._process.stdin is not None:
            self._process.stdin.close()
        try:
            self._process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self._process.terminate()
            self._process.wait(timeout=5)
        if self._process.stdout is not None:
            self._process.stdout.close()
        if self._process.stderr is not None:
            self._process.stderr.close()

    def __enter__(self) -> "CliSession":
        return self

    def __exit__(self, exc_type, exc, traceback) -> None:
        self.close()


def create_server(session: CliSession | None = None):
    from mcp.server.fastmcp import FastMCP

    cli = session or CliSession()
    server = FastMCP("KYS 自走棋")

    @server.tool()
    def new_game(
        difficulty: Difficulty = "normal",
        seed: Seed = "0x0000000000000001",
        position_swap_enabled: bool = True,
        detail: Detail = "full",
    ) -> dict[str, Any]:
        """建立可驗證的新棋局；seed 必須是固定 16 位十六進位字串。"""
        return cli.request(
            "new",
            {
                "difficulty": difficulty,
                "seed": seed,
                "position_swap_enabled": position_swap_enabled,
                "detail": detail,
            },
        )

    @server.tool()
    def observe_game(detail: Detail = "compact") -> dict[str, Any]:
        """取得棋局；compact 只含變動狀態，full 含完整定義；載入存檔會替換時間線並捨棄目前後綴。"""
        return cli.request("observe", {"detail": detail})

    @server.tool()
    def list_legal_actions() -> dict[str, Any]:
        """列出目前合法操作、每種操作的 action_schema、可直接提交的 example，以及帶名稱與說明的候選值。"""
        return cli.request("legal_actions")

    @server.tool()
    def take_action(action: dict[str, Any], detail: Detail = "compact") -> dict[str, Any]:
        """提交完整 action；compact 戰報只含決策摘要，完整軌跡可用 inspect_last_battle；戰敗仍會自動補回完整角色資料。"""
        return cli.request("act", {"action": action, "detail": detail})

    @server.tool()
    def inspect_role(role_id: int) -> dict[str, Any]:
        """檢視一名角色的完整屬性、各星級武學威力、範圍與羈絆。"""
        return cli.request("inspect_role", {"role_id": role_id})

    @server.tool()
    def inspect_combo(combo_name: str) -> dict[str, Any]:
        """依繁體中文名稱檢視羈絆成員、目前進度、門檻及效果。"""
        return cli.request("inspect_combo", {"combo_name": combo_name})

    @server.tool()
    def inspect_equipment(item_id: int) -> dict[str, Any]:
        """檢視裝備的基礎屬性、特殊效果、計入羈絆及角色專屬加成。"""
        return cli.request("inspect_equipment", {"item_id": item_id})

    @server.tool()
    def inspect_prepared_battle() -> dict[str, Any]:
        """完整檢視目前已準備戰鬥的地圖、地形、雙方屬性、武學、裝備及啟用羈絆。"""
        return cli.request("inspect_prepared_battle")

    @server.tool()
    def inspect_last_battle() -> dict[str, Any]:
        """完整檢視上一場戰鬥的開局棋盤、結構化效果軌跡與逐單位統計。"""
        return cli.request("inspect_last_battle")

    @server.tool()
    def list_saves() -> dict[str, Any]:
        """列出外部存檔目錄；載入任何欄位都會回到該前綴並捨棄目前較新的行動。"""
        return cli.request("list_saves")

    @server.tool()
    def inspect_save(slot: str) -> dict[str, Any]:
        """唯讀檢視自包含存檔，不改變棋局、亂數或重播。"""
        return cli.request("inspect_save", {"slot": slot})

    @server.tool()
    def save_game(slot: str, label: str = "") -> dict[str, Any]:
        """在穩定決策邊界覆寫欄位；不消耗亂數，也不加入遊戲重播。"""
        return cli.request("save_game", {"slot": slot, "label": label})

    @server.tool()
    def load_game(slot: str) -> dict[str, Any]:
        """替換目前狀態、亂數與重播前綴；回應會明示 discarded_active_actions，存檔目錄仍保留。"""
        return cli.request("load_game", {"slot": slot})

    @server.tool()
    def export_save(slot: str) -> dict[str, Any]:
        """匯出可攜、自包含且含完整重播前綴的存檔，不啟用它。"""
        return cli.request("export_save", {"slot": slot})

    @server.tool()
    def import_save(slot: str, payload: str) -> dict[str, Any]:
        """驗證並存入可攜存檔；不會靜默載入或替換目前時間線。"""
        return cli.request("import_save", {"slot": slot, "payload": payload})

    @server.tool()
    def export_replay() -> dict[str, Any]:
        """匯出目前選定時間線的權威 JSONL 重播。"""
        return cli.request("export_replay")

    @server.tool()
    def verify_replay(replay_jsonl: str) -> dict[str, Any]:
        """由不可變規則、根種子及接受的決策重新建立全新棋局並驗證重播。"""
        return cli.request("verify_replay", {"replay_jsonl": replay_jsonl})

    return server


def main() -> None:
    session = CliSession()
    try:
        create_server(session).run()
    finally:
        session.close()


if __name__ == "__main__":
    main()
