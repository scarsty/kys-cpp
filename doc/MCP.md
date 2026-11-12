# MCP（CLI / stdio）

## 启动和客户端配置

桌面版可作为本地 MCP Server 启动，仍显示游戏窗口，不需要 Node.js、额外服务或网络端口：

```powershell
.\x64\Release\kys.exe "D:\path\to\game" --mcp-stdio
```

在 VS Code 的 MCP 配置中添加（替换为实际绝对路径）：

```json
{
  "servers": {
    "kys": {
      "type": "stdio",
      "command": "D:\\kys-all\\_cpp\\kys-cpp\\x64\\Release\\kys.exe",
      "args": ["D:\\kys-all\\_cpp\\kys-cpp\\work\\game-dev", "--mcp-stdio"]
    }
  }
}
```

客户端启动并拥有一个独立游戏进程；这不是连接已运行游戏的接口。普通启动不启用 MCP。MCP 模式的 stdout 仅用于换行分隔的 JSON-RPC，游戏日志转至 stderr；关闭 stdin 会退出该游戏进程。需要有效游戏资源及可用的图形/音频环境，并非无界面模式。

## 工具

| 工具 | 参数 | 功能 |
|---|---|---|
| `get_game_state` | 无 | 当前输入上下文、地图、已提交的玩家坐标、移动和加载状态，以及 UI 尺寸和画面区域 |
| `get_party` | 无 | 队伍成员的等级、生命和内力；未加载时为空 |
| `get_inventory` | 无 | 非空背包槽位；未加载时为空 |
| `get_ui_state` | 无 | 当前运行节点、可见子控件、选中索引、对话、控件文字和逻辑坐标 `bounds` |
| `press_key` | `key` | 单次 UI 按键：`up`、`down`、`left`、`right`、`confirm`、`cancel`、`page_up`、`page_down` |
| `click_mouse` | `x`、`y`、`button` | 逻辑 UI 坐标下的单击，`button` 为 `left` 或 `right`；不移动系统鼠标 |
| `capture_screen` | 无 | 下一帧含最终 UI 的 PNG 图像 |

`press_key` 通过当前运行节点的正常事件分发处理，不直接修改场景或存档。它返回的是已接受，不保证操作已完成；应先查询 UI，每次发一个键，然后重新观察。动画和战斗期间拒绝控制，方向键不模拟持续按住，因此不能用来保证地图连续行走。图片按钮可能只有索引而没有文字；UI 树最多向下递归四层、每节点检查 64 个子控件，可配合截图识别。确认/取消沿用游戏行为，可能引起读档或退出，请谨慎授权。

## 鼠标点击和坐标

`click_mouse` 使用整数逻辑 UI 坐标，范围由 `get_game_state` 的 `ui_width` / `ui_height` 给出；`get_ui_state` 的 `bounds` 提供控件的逻辑位置和大小。截图为窗口像素，不能直接作为 UI 坐标：可按 `get_game_state.present_rect` 的偏移和尺寸映射回逻辑 UI。黑边不属于可点击区域。

例如调用 `click_mouse`，参数为：

```json
{ "x": 640, "y": 565, "button": "left" }
```

点击分三次事件分发：移动、按下、松开。事件及鼠标位置查询均作用于游戏内部，不移动系统指针或点击其他应用。键盘与鼠标共用待处理限制，动画和战斗期间拒绝新控制请求；返回 `accepted` 后应等待并重新查询 UI。暂不提供双击、拖拽或滚轮。

## 协议和限制

支持 MCP `initialize` / `notifications/initialized`、`ping`、`tools/list` 和 `tools/call`，协议协商提供 `2025-06-18`（兼容 `2024-11-05`）。所有游戏访问在主线程完成，输入以非阻塞方式轮询；单条请求上限 64 KiB。不开放任意脚本、文件访问、存档修改或联机控制。取消通知不会回滚已经接受的按键。

## 验证

Windows 集成冒烟测试（会打开临时游戏窗口，要求已编译及资源就绪）：

```powershell
.\tools\Test-McpStdio.ps1
# 也可传 -Executable 和 -GameDirectory
```

测试覆盖握手、工具列表、状态读取、键盘操作、鼠标左右键、坐标校验、嵌套菜单、PNG 截图、错误请求恢复、日志分离和 stdin 关闭退出。
