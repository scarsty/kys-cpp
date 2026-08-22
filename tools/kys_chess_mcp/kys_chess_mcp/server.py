from __future__ import annotations

from collections import deque
from dataclasses import dataclass
import json
import os
from pathlib import Path
import subprocess
import threading
from typing import Any


AUTO_SAVE_SLOT = "autosave"
CURRENT_DEPLOYMENT_ENV = "KYS_CHESS_MCP_CURRENT"


class CliTransportError(RuntimeError):
    def __init__(self, message: str, exit_code: int | None = None):
        super().__init__(message)
        self.exit_code = exit_code


class RuntimeReloadError(RuntimeError):
    pass


@dataclass(frozen=True)
class RuntimeDeployment:
    version: str
    activation: str
    executable: Path


class RuntimeDeploymentResolver:
    def __init__(self, manifest_path: Path | str):
        self._manifest_path = Path(manifest_path).resolve()

    def resolve(self) -> RuntimeDeployment:
        manifest = json.loads(self._manifest_path.read_text(encoding="utf-8"))
        executable = Path(manifest["executable"])
        if not executable.is_absolute():
            executable = self._manifest_path.parent / executable
        return RuntimeDeployment(
            version=str(manifest["version"]),
            activation=str(manifest["activation"]),
            executable=executable.resolve(),
        )


class FixedRuntimeDeploymentResolver:
    def __init__(self, executable: Path | str):
        path = Path(executable).resolve()
        self._deployment = RuntimeDeployment(str(path), str(path), path)

    def resolve(self) -> RuntimeDeployment:
        return self._deployment


def default_install_root() -> Path:
    local_app_data = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local"))
    return local_app_data / "kys_chess_mcp"


class CliSession:
    """Owns one protocol-clean CLI JSONL process."""

    def __init__(
        self,
        executable: Path | str | None = None,
        extra_args: list[str] | None = None,
        save_dir: Path | str | None = None,
        deployment_manifest: Path | str | None = None,
    ):
        assert executable is None or deployment_manifest is None
        configured_manifest = os.environ.get(CURRENT_DEPLOYMENT_ENV)
        if executable is not None:
            self._runtime_resolver = FixedRuntimeDeploymentResolver(executable)
        else:
            self._runtime_resolver = RuntimeDeploymentResolver(
                deployment_manifest or configured_manifest or default_install_root() / "current.json")
        self._extra_args = list(extra_args or [])
        configured_save_dir = os.environ.get("KYS_CHESS_MCP_SAVE_DIR")
        save_directory = Path(save_dir or configured_save_dir or default_install_root() / "saves")
        save_directory.mkdir(parents=True, exist_ok=True)
        self._autosave_file = save_directory / "autosave.json"
        self._process: subprocess.Popen[str]
        self._stderr_thread: threading.Thread
        self._next_id = 1
        self._next_internal_id = 1
        self._lock = threading.Lock()
        self._diagnostics: deque[str] = deque(maxlen=200)
        self._has_active_session = False
        self._blocked_deployment: RuntimeDeployment | None = None
        self._last_runtime_reload: dict[str, Any] | None = None
        self._start_process()

    def _start_process(self, deployment: RuntimeDeployment | None = None) -> None:
        selected = deployment or self._runtime_resolver.resolve()
        command = [
            str(selected.executable),
            "--jsonl",
            "--autosave-file",
            str(self._autosave_file),
        ]
        command.extend(self._extra_args)
        process = subprocess.Popen(
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            bufsize=1,
        )
        self._deployment = selected
        self._command = command
        self._process = process
        self._stderr_thread = threading.Thread(
            target=self._drain_stderr,
            args=(self._process,),
            daemon=True,
        )
        self._stderr_thread.start()
        try:
            catalog = self._internal_request("mcp_tools", {})
            if not catalog.get("ok"):
                raise RuntimeReloadError(catalog.get("error_message", "原生程序未提供 MCP 工具目錄"))
            self._tool_catalog = list(catalog["result"]["tools"])
        except (CliTransportError, KeyError, TypeError, RuntimeReloadError):
            self._stop_process()
            raise

    def _stop_process(self) -> None:
        if self._process.poll() is None:
            if self._process.stdin is not None:
                self._process.stdin.close()
            try:
                self._process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._process.terminate()
                try:
                    self._process.wait(timeout=1)
                except subprocess.TimeoutExpired:
                    self._process.kill()
                    self._process.wait(timeout=1)
        self._stderr_thread.join(timeout=0.5)
        self._close_process_streams()

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

    def tool_catalog(self) -> list[dict[str, Any]]:
        return list(self._tool_catalog)

    def _capture_reload_checkpoint(self) -> None:
        saved = self._internal_request(
            "save_game",
            {"slot": AUTO_SAVE_SLOT, "label": "自動存檔"},
        )
        if not saved.get("ok"):
            raise RuntimeReloadError(saved.get("error_message", "無法建立熱更新檢查點"))

    def _restore_reload_checkpoint(self) -> None:
        resumed = self._internal_request(
            "resume_game",
            {"detail": "compact"},
        )
        if not resumed.get("ok"):
            raise RuntimeReloadError(resumed.get("error_message", "無法接續熱更新檢查點"))
        self._has_active_session = True

    @staticmethod
    def _deployment_summary(deployment: RuntimeDeployment) -> dict[str, str]:
        return {
            "version": deployment.version,
            "activation": deployment.activation,
            "executable": str(deployment.executable),
        }

    def _runtime_reload_failure(
        self,
        request_id: int,
        previous: RuntimeDeployment,
        requested: RuntimeDeployment,
        error: Exception,
        rollback_error: Exception | None = None,
        rollback_performed: bool = True,
    ) -> dict[str, Any]:
        session_lost = rollback_error is not None
        status = "failed" if session_lost else "rolled_back"
        message = "新執行期與先前執行期都無法還原目前棋局。"
        if not session_lost and not rollback_performed:
            status = "unchanged"
            message = "無法保存目前棋局；仍使用先前執行期。"
        elif not session_lost:
            message = "新執行期無法還原目前棋局；已回復先前執行期。"
        details = {
            "status": status,
            "previous": self._deployment_summary(previous),
            "requested": self._deployment_summary(requested),
            "error": str(error),
            "session_lost": session_lost,
        }
        if rollback_error is not None:
            details["rollback_error"] = str(rollback_error)
        self._last_runtime_reload = details
        self._diagnostics.append(
            f"[MCP 熱更新] {requested.version} 切換失敗：{error}"
        )
        return {
            "id": request_id,
            "ok": False,
            "error_code": "runtime_reload_failed",
            "error_message": message,
            "runtime_reload": details,
            "session_lost": session_lost,
        }

    def _reload_runtime_if_changed(self, request_id: int) -> dict[str, Any] | None:
        try:
            requested = self._runtime_resolver.resolve()
        except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
            self._diagnostics.append(f"[MCP 熱更新] 無法讀取目前部署：{error}")
            return None
        if requested == self._deployment:
            return None
        if requested == self._blocked_deployment:
            return None
        self._blocked_deployment = None

        previous = self._deployment
        previous_tool_catalog = self._tool_catalog
        session_checkpointed = False
        if self._has_active_session:
            try:
                self._capture_reload_checkpoint()
                session_checkpointed = True
            except (CliTransportError, RuntimeReloadError) as error:
                return self._runtime_reload_failure(
                    request_id,
                    previous,
                    requested,
                    error,
                    rollback_performed=False,
                )

        self._stop_process()
        self._has_active_session = False
        try:
            self._start_process(requested)
            if session_checkpointed:
                self._restore_reload_checkpoint()
        except (CliTransportError, OSError, KeyError, TypeError, RuntimeReloadError) as error:
            try:
                self._stop_process()
                self._start_process(previous)
                if session_checkpointed:
                    self._restore_reload_checkpoint()
            except (CliTransportError, OSError, KeyError, TypeError, RuntimeReloadError) as rollback_error:
                self._has_active_session = False
                return self._runtime_reload_failure(
                    request_id,
                    previous,
                    requested,
                    error,
                    rollback_error,
                )
            self._blocked_deployment = requested
            return self._runtime_reload_failure(request_id, previous, requested, error)

        details = {
            "status": "succeeded",
            "previous": self._deployment_summary(previous),
            "current": self._deployment_summary(requested),
            "session_restored": session_checkpointed,
            "tools_changed": previous_tool_catalog != self._tool_catalog,
        }
        self._last_runtime_reload = details
        self._diagnostics.append(
            f"[MCP 熱更新] 已由 {previous.version} 切換至 {requested.version}"
        )
        return details

    def _recover_transport(self, request_id: int, error: CliTransportError) -> dict[str, Any]:
        session_lost = self._has_active_session
        self._has_active_session = False
        self._stop_process()
        diagnostics = list(self._diagnostics)
        restarted = False
        restart_error = ""
        try:
            self._start_process()
            restarted = True
        except (CliTransportError, OSError, KeyError, TypeError, RuntimeReloadError) as start_error:
            restart_error = str(start_error)
        diagnostic_text = "\n".join(diagnostics[-40:])
        message = str(error)
        if diagnostic_text:
            message += f"\nCLI 診斷：\n{diagnostic_text}"
        if restart_error:
            message += f"\n重新啟動失敗：{restart_error}"
        elif restarted:
            message += "\nCLI 已重新啟動；非預期程序終止不會猜測要接續哪一個工作階段。"
        return {
            "id": request_id,
            "ok": False,
            "error_code": "cli_process_exited",
            "error_message": message,
            "exit_code": error.exit_code,
            "diagnostics": diagnostics,
            "restarted": restarted,
            "session_restored": False,
            "session_lost": session_lost,
        }

    def request(self, method: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
        with self._lock:
            request_id = self._next_id
            self._next_id += 1
            runtime_reload = self._reload_runtime_if_changed(request_id)
            if runtime_reload and runtime_reload.get("error_code"):
                return runtime_reload
            if method == "get_diagnostics":
                response = {
                    "id": request_id,
                    "ok": True,
                    "result": self.diagnostic_status(),
                }
                if runtime_reload:
                    response["runtime_reload"] = runtime_reload
                return response
            try:
                response = self._exchange(request_id, method, params)
                if response.get("ok") and method in ("new", "resume_game"):
                    self._has_active_session = True
            except CliTransportError as error:
                return self._recover_transport(request_id, error)
            if runtime_reload:
                response["runtime_reload"] = runtime_reload
            return response

    def diagnostics(self) -> list[str]:
        return list(self._diagnostics)

    def diagnostic_status(self) -> dict[str, Any]:
        try:
            available = self._deployment_summary(self._runtime_resolver.resolve())
        except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
            available = {"error": str(error)}
        native: dict[str, Any]
        try:
            native_response = self._internal_request("get_diagnostics", {})
            native = native_response.get("result", native_response)
        except CliTransportError as error:
            native = {"error": str(error)}
        return {
            "cli_running": self._process.poll() is None,
            "active_in_memory_session": self._has_active_session,
            "runtime": self._deployment_summary(self._deployment),
            "available_runtime": available,
            "last_runtime_reload": self._last_runtime_reload,
            "native": native,
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
            self._stop_process()

    def __enter__(self) -> "CliSession":
        return self

    def __exit__(self, exc_type, exc, traceback) -> None:
        self.close()


def mcp_tools(session: CliSession):
    from mcp.types import Tool

    return [
        Tool(
            name=definition["name"],
            description=definition["description"],
            inputSchema=definition["inputSchema"],
        )
        for definition in session.tool_catalog()
    ]


def dispatch_mcp_tool(
    session: CliSession,
    name: str,
    arguments: dict[str, Any],
) -> dict[str, Any]:
    definition = next(
        (tool for tool in session.tool_catalog() if tool["name"] == name),
        None,
    )
    if definition is None:
        raise ValueError(f"未知 MCP 工具：{name}")
    return session.request(definition["native_method"], arguments)


def create_server(session: CliSession | None = None):
    from mcp.server import Server
    from mcp.types import CallToolResult, TextContent

    cli = session or CliSession()
    server = Server(
        "KYS 自走棋",
        version="0.1.0",
        instructions="以工具操作可驗證的 KYS 自走棋工作階段。",
    )

    @server.list_tools()
    async def list_tools():
        return mcp_tools(cli)

    @server.call_tool()
    async def call_tool(name: str, arguments: dict[str, Any]):
        response = dispatch_mcp_tool(cli, name, arguments)
        if response.get("runtime_reload", {}).get("tools_changed"):
            await server.request_context.session.send_tool_list_changed()
        return CallToolResult(
            content=[TextContent(
                type="text",
                text=json.dumps(
                    response,
                    ensure_ascii=False,
                    separators=(",", ":"),
                ),
            )],
            structuredContent=response,
            isError=not response.get("ok", False),
        )

    return server


def main() -> None:
    import anyio
    from mcp.server.lowlevel.server import NotificationOptions
    from mcp.server.stdio import stdio_server

    async def run() -> None:
        session = CliSession()
        try:
            server = create_server(session)
            async with stdio_server() as (read_stream, write_stream):
                await server.run(
                    read_stream,
                    write_stream,
                    server.create_initialization_options(
                        NotificationOptions(tools_changed=True)
                    ),
                )
        finally:
            session.close()

    anyio.run(run)


if __name__ == "__main__":
    main()
