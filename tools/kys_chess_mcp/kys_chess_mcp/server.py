from __future__ import annotations

from collections import deque
import hashlib
import json
import os
from pathlib import Path
import subprocess
import threading
from typing import Annotated, Any, Literal

from pydantic import Field


Difficulty = Literal["easy", "normal", "hard"]
Detail = Literal["compact", "full"]
ActionDetail = Literal["summary", "compact", "full"]
PreparedBattleDetail = Literal["summary", "compact", "full"]
AUTO_SAVE_SLOT = "autosave"
AUTO_SAVE_LABEL = "自動存檔"
Seed = Annotated[
    str,
    Field(
        pattern=r"^0x[0-9a-fA-F]{16}$",
        description="0x 前綴加上固定 16 位十六進位數字，例如 0x000000000000BEEF",
    ),
]


class CliTransportError(RuntimeError):
    def __init__(self, message: str, exit_code: int | None = None):
        super().__init__(message)
        self.exit_code = exit_code


def default_cli_path() -> Path:
    configured = os.environ.get("KYS_CHESS_CLI")
    if configured:
        return Path(configured)
    root = Path(__file__).resolve().parents[3]
    return root / "x64" / "Debug" / "kys_chess_cli.exe"


class CliSession:
    """Owns one protocol-clean CLI JSONL process."""

    def __init__(
        self,
        executable: Path | str | None = None,
        extra_args: list[str] | None = None,
        save_dir: Path | str | None = None,
        autosave: bool = True,
    ):
        self._command = [str(executable or default_cli_path()), "--jsonl", *(extra_args or [])]
        configured_save_dir = os.environ.get("KYS_CHESS_MCP_SAVE_DIR")
        local_app_data = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local"))
        self._save_dir = Path(save_dir or configured_save_dir or local_app_data / "kys_chess_mcp" / "saves")
        self._save_dir.mkdir(parents=True, exist_ok=True)
        self._process: subprocess.Popen[str]
        self._stderr_thread: threading.Thread
        self._next_id = 1
        self._next_internal_id = 1
        self._lock = threading.Lock()
        self._diagnostics: deque[str] = deque(maxlen=200)
        self._has_active_session = False
        self._autosave_enabled = autosave
        self._start_process()

    def _start_process(self) -> None:
        self._process = subprocess.Popen(
            self._command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            bufsize=1,
        )
        self._stderr_thread = threading.Thread(
            target=self._drain_stderr,
            args=(self._process,),
            daemon=True,
        )
        self._stderr_thread.start()

    def _drain_stderr(self, process: subprocess.Popen[str]) -> None:
        assert process.stderr is not None
        for line in process.stderr:
            self._diagnostics.append(line.rstrip())

    def _exchange(self, request_id: int | str, method: str, params: dict[str, Any] | None) -> dict[str, Any]:
        exit_code = self._process.poll()
        if exit_code is not None:
            raise CliTransportError(f"棋局程序已結束，exit={exit_code}", exit_code)
        payload = {"id": request_id, "method": method, "params": params or {}}
        assert self._process.stdin is not None
        assert self._process.stdout is not None
        try:
            self._process.stdin.write(json.dumps(payload, ensure_ascii=False) + "\n")
            self._process.stdin.flush()
            line = self._process.stdout.readline()
        except (BrokenPipeError, OSError) as error:
            raise CliTransportError(f"無法與棋局程序通訊：{error}", self._process.poll()) from error
        if not line:
            try:
                exit_code = self._process.wait(timeout=0.25)
            except subprocess.TimeoutExpired:
                exit_code = self._process.poll()
            raise CliTransportError("棋局程序未傳回回應", exit_code)
        try:
            response = json.loads(line)
        except json.JSONDecodeError as error:
            raise CliTransportError(f"棋局程序傳回無效 JSON：{error}", self._process.poll()) from error
        if response.get("id") != request_id:
            raise CliTransportError("棋局程序回應識別碼不相符", self._process.poll())
        return response

    def _internal_request(self, method: str, params: dict[str, Any]) -> dict[str, Any]:
        request_id = f"mcp-internal-{self._next_internal_id}"
        self._next_internal_id += 1
        return self._exchange(request_id, method, params)

    def _slot_path(self, slot: str) -> Path:
        digest = hashlib.sha256(slot.encode("utf-8")).hexdigest()
        return self._save_dir / f"{digest}.json"

    def _persist_save(self, slot: str, payload: str) -> None:
        target = self._slot_path(slot)
        temporary = target.with_suffix(".tmp")
        temporary.write_text(
            json.dumps({"slot": slot, "payload": payload}, ensure_ascii=False),
            encoding="utf-8",
        )
        temporary.replace(target)

    def _persistent_save_summaries(self) -> list[dict[str, Any]]:
        summaries: list[dict[str, Any]] = []
        for path in sorted(self._save_dir.glob("*.json")):
            try:
                stored = json.loads(path.read_text(encoding="utf-8"))
                checkpoint = json.loads(stored["payload"])
                state = checkpoint["state"]
                replay_records = [
                    json.loads(line)
                    for line in checkpoint.get("replay_jsonl", "").splitlines()
                    if line.strip()
                ]
                header = next(
                    (record for record in replay_records if record.get("record") == "header"),
                    {},
                )
                summaries.append({
                    "slot": str(stored["slot"]),
                    "occupied": True,
                    "revision": int(checkpoint.get("save_revision", 0)),
                    "label": str(checkpoint.get("label", "")),
                    "fight": int(state.get("fight", 0)),
                    "level": int(state.get("level", 0)),
                    "money": int(state.get("money", 0)),
                    "roster_count": len(state.get("roster", {})),
                    "replay_sequence": sum(
                        record.get("record") == "decision" for record in replay_records
                    ),
                    "state_hash": str(checkpoint.get("snapshot_hash", "")),
                    "compatible": None,
                    "compatibility_scope": "建立棋局後依遊戲版本與難度判定",
                    "difficulty": header.get("difficulty"),
                    "game_version": checkpoint.get("game_version"),
                    "persisted": True,
                })
            except (OSError, KeyError, TypeError, ValueError) as error:
                self._diagnostics.append(f"[MCP 存檔] 無法列出 {path.name}：{error}")
        return summaries

    def _restore_persisted_saves(self) -> None:
        for path in sorted(self._save_dir.glob("*.json")):
            try:
                stored = json.loads(path.read_text(encoding="utf-8"))
                slot = stored["slot"]
                payload = stored["payload"]
            except (OSError, KeyError, json.JSONDecodeError) as error:
                self._diagnostics.append(f"[MCP 存檔] 無法讀取 {path.name}：{error}")
                continue
            response = self._internal_request("import_save", {"slot": slot, "payload": payload})
            if not response.get("ok"):
                self._diagnostics.append(
                    f"[MCP 存檔] 無法還原欄位「{slot}」：{response.get('error_message', '未知錯誤')}"
                )

    def _update_autosave(self) -> None:
        if not self._autosave_enabled:
            return
        saved = self._internal_request(
            "save_game",
            {"slot": AUTO_SAVE_SLOT, "label": AUTO_SAVE_LABEL},
        )
        if not saved.get("ok"):
            self._diagnostics.append(
                f"[MCP 自動存檔] 無法建立存檔：{saved.get('error_message', '未知錯誤')}"
            )
            return
        exported = self._internal_request("export_save", {"slot": AUTO_SAVE_SLOT})
        if not exported.get("ok"):
            self._diagnostics.append(
                f"[MCP 自動存檔] 無法匯出存檔：{exported.get('error_message', '未知錯誤')}"
            )
            return
        try:
            self._persist_save(AUTO_SAVE_SLOT, exported["result"]["payload"])
        except (OSError, KeyError, TypeError) as error:
            self._diagnostics.append(f"[MCP 自動存檔] 無法寫入持久存檔：{error}")

    def _recover_transport(self, request_id: int, error: CliTransportError) -> dict[str, Any]:
        session_lost = self._has_active_session
        self._has_active_session = False
        if self._process.poll() is None:
            self._process.terminate()
            try:
                self._process.wait(timeout=1)
            except subprocess.TimeoutExpired:
                self._process.kill()
                self._process.wait(timeout=1)
        self._stderr_thread.join(timeout=0.5)
        diagnostics = list(self._diagnostics)
        self._close_process_streams()
        restarted = False
        restart_error = ""
        try:
            self._start_process()
            restarted = True
        except OSError as start_error:
            restart_error = str(start_error)
        diagnostic_text = "\n".join(diagnostics[-40:])
        message = str(error)
        if diagnostic_text:
            message += f"\nCLI 診斷：\n{diagnostic_text}"
        if restart_error:
            message += f"\n重新啟動失敗：{restart_error}"
        elif restarted:
            message += "\nCLI 已重新啟動；原本的記憶體內棋局已遺失，請先建立新棋局，再載入持久存檔。"
        return {
            "id": request_id,
            "ok": False,
            "error_code": "cli_process_exited",
            "error_message": message,
            "exit_code": error.exit_code,
            "diagnostics": diagnostics,
            "restarted": restarted,
            "session_lost": session_lost,
        }

    def request(self, method: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
        with self._lock:
            request_id = self._next_id
            self._next_id += 1
            if method == "list_saves" and not self._has_active_session:
                return {
                    "id": request_id,
                    "ok": True,
                    "result": self._persistent_save_summaries(),
                }
            try:
                response = self._exchange(request_id, method, params)
                if response.get("ok") and method == "new":
                    self._has_active_session = True
                    self._restore_persisted_saves()
                elif (
                    response.get("ok")
                    and method == "act"
                    and response.get("result", {}).get("accepted") is True
                ):
                    self._update_autosave()
                elif response.get("ok") and method == "load_game":
                    self._update_autosave()
                elif response.get("ok") and method == "save_game":
                    slot = str((params or {})["slot"])
                    exported = self._internal_request("export_save", {"slot": slot})
                    if exported.get("ok"):
                        self._persist_save(slot, exported["result"]["payload"])
                elif response.get("ok") and method == "import_save":
                    slot = str((params or {})["slot"])
                    self._persist_save(slot, str((params or {})["payload"]))
            except CliTransportError as error:
                return self._recover_transport(request_id, error)
            return response

    def diagnostics(self) -> list[str]:
        return list(self._diagnostics)

    def diagnostic_status(self) -> dict[str, Any]:
        return {
            "cli_running": self._process.poll() is None,
            "active_in_memory_session": self._has_active_session,
            "persistent_save_directory": str(self._save_dir),
            "autosave_enabled": self._autosave_enabled,
            "autosave_slot": AUTO_SAVE_SLOT,
            "diagnostics": self.diagnostics(),
        }

    def _close_process_streams(self) -> None:
        if self._process.stdin is not None:
            self._process.stdin.close()
        if self._process.stdout is not None:
            self._process.stdout.close()
        if self._process.stderr is not None:
            self._process.stderr.close()

    def close(self) -> None:
        with self._lock:
            if self._process.poll() is None:
                if self._process.stdin is not None:
                    self._process.stdin.close()
                try:
                    self._process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self._process.terminate()
                    self._process.wait(timeout=5)
            self._stderr_thread.join(timeout=0.5)
            self._close_process_streams()

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
        """建立可驗證的新棋局；seed 必須是 0x 前綴加上固定 16 位十六進位數字；若要接續上次進度，建立同難度棋局後先載入 autosave。"""
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
    def get_diagnostics() -> dict[str, Any]:
        """取得 CLI 執行狀態、持久存檔目錄及最近的原生 stderr 診斷。"""
        status = getattr(cli, "diagnostic_status", None)
        return status() if status else {"diagnostics": []}

    @server.tool()
    def list_legal_actions() -> dict[str, Any]:
        """列出目前合法操作、每種操作的 action_schema、可直接提交的 example，以及帶名稱與說明的候選值。"""
        return cli.request("legal_actions")

    @server.tool()
    def take_action(action: dict[str, Any], detail: ActionDetail = "summary") -> dict[str, Any]:
        """提交 action；summary 只回變更，compact 回精簡現況，full 才含完整除錯與驗證資料。"""
        return cli.request("act", {"action": action, "detail": detail})

    @server.tool()
    def inspect_shop_slot(slot: int) -> dict[str, Any]:
        """分析單一商店欄位的價格、持有份數、合成結果、羈絆變化與當前抽取機率。"""
        return cli.request("inspect_shop_slot", {"slot": slot})

    @server.tool()
    def inspect_shop() -> dict[str, Any]:
        """一次分析目前全部商店欄位及當前等級的費用機率。"""
        return cli.request("inspect_shop")

    @server.tool()
    def get_shop_odds(level: int | None = None) -> dict[str, Any]:
        """取得指定或目前等級的商店費用機率與實際可用角色池。"""
        params = {} if level is None else {"level": level}
        return cli.request("get_shop_odds", params)

    @server.tool()
    def inspect_chess_instance(chess_instance_id: int) -> dict[str, Any]:
        """檢視棋子實例的實際屬性、裝備、升星進度、出戰狀態與羈絆貢獻。"""
        return cli.request("inspect_chess_instance", {"chess_instance_id": chess_instance_id})

    @server.tool()
    def inspect_bans() -> dict[str, Any]:
        """檢視目前禁棋、剩餘容量、依費用分組的可選角色及生效時機。"""
        return cli.request("inspect_bans")

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
    def inspect_challenge(challenge_name: str) -> dict[str, Any]:
        """依繁體中文名稱檢視遠征的權威敵人星級、裝備與獎勵。"""
        return cli.request("inspect_challenge", {"challenge_name": challenge_name})

    @server.tool()
    def inspect_prepared_battle(
        detail: PreparedBattleDetail = "summary",
    ) -> dict[str, Any]:
        """檢視已準備戰鬥；summary 只含地圖、棋盤與座標，compact 增加裝備與啟用羈絆，full 才含完整除錯資料。"""
        return cli.request("inspect_prepared_battle", {"detail": detail})

    @server.tool()
    def inspect_last_battle() -> dict[str, Any]:
        """完整檢視上一場戰鬥的開局棋盤、結構化效果軌跡與逐單位統計。"""
        return cli.request("inspect_last_battle")

    @server.tool()
    def list_saves() -> dict[str, Any]:
        """不需先建立棋局即可列出持久存檔與 autosave；建立棋局後才會判定相容性並可載入。"""
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
        """替換目前狀態、亂數與重播前綴並更新 autosave；回應會明示 discarded_active_actions，存檔目錄仍保留。"""
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

    return server


def main() -> None:
    session = CliSession()
    try:
        create_server(session).run()
    finally:
        session.close()


if __name__ == "__main__":
    main()
