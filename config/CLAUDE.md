# config 模块文档

[根目录](../CLAUDE.md) > **config**

> 最后更新：2026-02-28 20:20:55

---

## 变更记录 (Changelog)

### 2026-02-28 20:20:55
- 初始化 config 模块文档
- 识别所有配置文件及其用途

---

## 模块职责

游戏配置文件目录，包含所有 YAML 和 INI 格式的配置文件。支持热重载（修改后重启游戏即可生效，无需重新编译）。

---

## 入口与启动

配置文件在游戏启动时自动加载：
- `kysmod.ini`：由 `Application::config()` 读取
- `chess_*.yaml`：由对应的 `Chess*::loadConfig()` 读取

---

## 对外接口

### kysmod.ini（主配置文件）

**核心配置项**：

```ini
[game]
battle_mode = 2          # 战斗模式：0=回合制，1=半即时，2=Hades，3=Sekiro
RESOLUTIONX = 640        # 窗口宽度
RESOLUTIONY = 440        # 窗口高度
title = Heroes Drift     # 窗口标题
simplified_chinese = 0   # 0=繁体，1=简体

[music]
VOLUME = 10              # 音乐音量（0-100）
VOLUMEWAV = 10           # 音效音量（0-100）
```

### chess_balance_*.yaml（平衡配置）

**三种难度**：
- `chess_balance_easy.yaml`：简单难度
- `chess_balance_normal.yaml`：正常难度
- `chess_balance_hard.yaml`：困难难度（如果存在）

**核心配置项**：

```yaml
星级加成:
  生命倍率: 0.4
  攻击倍率: 0.4
  防御倍率: 0.4
  速度倍率: 0.25
  固定生命: 275
  固定攻击: 55
  固定防御: 35

经济:
  初始金币: 15
  刷新费用: 2
  购买经验费用: 5
  战斗经验: 2
  利息百分比: 10
  利息上限: 3

商店权重:
  - [85, 15, 0, 0, 0]    # 0级
  - [70, 30, 0, 0, 0]    # 1级
  # ... 共10级

敌人表:
  - [[1, 1], [1, 1]]     # 第1关
  - [[1, 1], [2, 1]]     # 第2关
  # ... 共28关
```

### chess_pool.yaml（棋池配置）

定义商店中可出现的所有棋子及其费用等级。

```yaml
- 费用: 1
  角色:
    - 130  # 柯鎮惡
    - 63   # 程英
- 费用: 2
  角色:
    - 54   # 袁承志
    - 55   # 黃蓉
# ... 共5个费用等级
```

### chess_combos.yaml（羁绊配置）

定义角色之间的羁绊效果。

```yaml
羁绊:
  - 名称: "逍遥派"
    角色: [1, 2, 3]
    效果:
      - 人数: 2
        描述: "攻击+10%"
      - 人数: 4
        描述: "攻击+20%，速度+15%"
```

### chess_neigong.yaml（内功配置）

定义内功系统的配置。

```yaml
刷新费用: 4
选择数量: 3
Boss可选层级:
  0: [1]
  1: [1, 2]
  2: [1, 2, 3]
```

### chess_challenge.yaml（远征挑战配置）

定义远征挑战的敌人和奖励。

```yaml
远征挑战:
  - 名称: "聚贤庄内"
    描述: "挑战乔峰"
    敌人: [[50, 3]]
    奖励:
      - 类型: 获取金币
        数值: 10
```

---

## 关键依赖与配置

### 依赖库
- YAML-CPP：解析 YAML 文件

### 文件路径
配置文件通过 `GameUtil::PATH() + "config/"` 访问。

---

## 数据模型

### 配置加载流程
1. 游戏启动时读取 `kysmod.ini`
2. 根据 `battle_mode` 加载对应的战斗系统
3. 加载 `chess_balance_*.yaml`（根据难度选择）
4. 加载 `chess_pool.yaml`、`chess_combos.yaml` 等

### 配置验证
- 如果配置文件格式错误，控制台会输出错误信息
- 缺失的配置项会使用默认值

---

## 测试与质量

### 配置测试
- 修改配置后重启游戏验证
- 检查控制台输出是否有错误信息

---

## 常见问题 (FAQ)

### Q: 如何切换难度？
A: 修改游戏代码中加载的配置文件名，或直接编辑 `chess_balance_normal.yaml`。

### Q: 配置文件修改后不生效？
A: 确保重启了游戏，配置文件在启动时加载。

### Q: 如何查看角色 ID？
A: 使用 SQLite 工具打开 `game/save/0.db`，查看 `role` 表。

---

## 相关文件清单

- `kysmod.ini`：主配置文件
- `chess_balance_easy.yaml`：简单难度平衡配置
- `chess_balance_normal.yaml`：正常难度平衡配置
- `chess_pool.yaml`：棋池配置
- `chess_combos.yaml`：羁绊配置
- `chess_neigong.yaml`：内功配置
- `chess_challenge.yaml`：远征挑战配置
