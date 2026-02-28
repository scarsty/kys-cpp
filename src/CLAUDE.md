# src 模块文档

[根目录](../CLAUDE.md) > **src**

> 最后更新：2026-02-28 20:20:55

---

## 变更记录 (Changelog)

### 2026-02-28 20:20:55
- 初始化 src 模块文档
- 识别战斗系统、自走棋系统、UI 系统等子模块

---

## 模块职责

游戏核心源代码目录，包含所有 C++ 实现文件。主要子系统包括：

- **战斗系统**：BattleScene 及其子类（**以 BattleSceneHades 为主要参考**）
- **自走棋系统**：Chess* 系列类（棋池、羁绊、平衡、内功）
- **UI 系统**：UI*、Menu、TextBox 等界面组件
- **场景系统**：Scene、MainScene、SubScene、TitleScene
- **资源管理**：Save、TextureManager、Audio、GrpIdxFile
- **引擎核心**：Engine、Application、RunNode、Camera

---

## 入口与启动

### 主入口
**文件**：`kys.cpp`

```cpp
int main(int argc, char* argv[])
{
    // 设置控制台编码为 UTF-8
    system("chcp 65001");

    // 解析游戏路径参数
    if (argc >= 2) {
        GameUtil::PATH() = argv[1];
    }

    // 启动应用
    Application app;
    return app.run();
}
```

### 应用初始化
**文件**：`Application.h` / `Application.cpp`

- `Application::config()`：读取 `config/kysmod.ini` 配置
- `Application::run()`：初始化引擎并启动主循环

---

## 对外接口

### 战斗系统接口

#### BattleSceneHades（主要战斗模式）
**文件**：`BattleSceneHades.h` / `BattleSceneHades.cpp`

**核心方法**：
- `dealEvent(EngineEvent& e)`：战场主循环
- `Action(Role* r)`：角色行动逻辑
- `AI(Role* r)`：AI 决策
- `calMagicHurt(Role* r1, Role* r2, Magic* magic, int dis)`：伤害计算
- `checkResult()`：战斗结果判定

**特性**：
- 自动战棋模式（无需玩家操作）
- 帧驱动战斗系统
- 弹射攻击系统
- 大招系统（内力满触发）
- 连击计数
- 僵直与震动效果
- 冲刺残影效果

#### BattleScene（战斗基类）
**文件**：`BattleScene.h` / `BattleScene.cpp`

**核心数据结构**：
- `battle_roles_`：所有参战角色
- `earth_layer_`、`building_layer_`：地图层
- `role_layer_`：角色层
- `battle_menu_`：战斗菜单
- `battle_cursor_`：战斗光标

### 自走棋系统接口

#### ChessBalance（平衡系统）
**文件**：`ChessBalance.h` / `ChessBalance.cpp`

- `loadConfig()`：加载 `chess_balance_*.yaml` 配置
- `getStarBonus()`：获取星级加成
- `getEnemyTable()`：获取敌人表

#### ChessPool（棋池系统）
**文件**：`ChessPool.h` / `ChessPool.cpp`

- `loadConfig()`：加载 `chess_pool.yaml` 配置
- `getRolesByTier(int tier)`：获取指定费用等级的角色列表

#### ChessCombo（羁绊系统）
**文件**：`ChessCombo.h` / `ChessCombo.cpp`

- `loadConfig()`：加载 `chess_combos.yaml` 配置
- `calculateCombos()`：计算当前激活的羁绊
- `applyComboEffects()`：应用羁绊效果

### UI 系统接口

#### UI（UI 基类）
**文件**：`UI.h` / `UI.cpp`

- `draw()`：绘制 UI
- `dealEvent(EngineEvent& e)`：处理事件

#### UIStatus（角色状态界面）
**文件**：`UIStatus.h` / `UIStatus.cpp`

- 显示角色属性、武功、物品

#### UIShop（商店界面）
**文件**：`UIShop.h` / `UIShop.cpp`

- 购买/出售棋子
- 刷新商店
- 购买经验

---

## 关键依赖与配置

### 编译依赖
- SDL3、SDL3_image、SDL3_ttf
- BASS、BASSMIDI
- SQLite3
- YAML-CPP
- Lua 5.4
- mlcc 子模块

### 预处理宏
- `WITH_SMALLPOT1`：启用视频播放
- `USE_BASS`：使用 BASS 音频库
- `WITH_NETWORK`：启用网络对战
- `WIN32`：Windows 平台
- `NOMINMAX`：禁用 Windows.h 的 min/max 宏

### 配置文件
- `config/kysmod.ini`：主配置
- `config/chess_*.yaml`：自走棋配置

---

## 数据模型

### 核心数据结构
**文件**：`Types.h`

#### Role（角色）
```cpp
struct Role {
    int ID;
    int HeadID;
    int Level;
    int HP, MaxHP;
    int MP, MaxMP;
    int Attack, Defence, Speed;
    int Fist, Sword, Knife, Unusual;  // 武功熟练度
    int Dead;
    int Frozen;  // 僵直帧数
    int Team;    // 0=我方，1=敌方
    // ... 更多属性
};
```

#### Magic（武功）
```cpp
struct Magic {
    int ID;
    int Name;
    int AttackType;  // 攻击范围类型
    int Hurt;        // 威力
    int HurtType;    // 伤害类型
    // ... 更多属性
};
```

#### Item（物品）
```cpp
struct Item {
    int ID;
    int Name;
    int ItemType;
    int User;
    // ... 更多属性
};
```

### 地图数据结构
**文件**：`Types.h`

```cpp
template <typename T>
struct MapSquare {
    void resize(int x);
    T& data(int x, int y);
    int size();
};

using MapSquareInt = MapSquare<MAP_INT>;
```

---

## 测试与质量

### 当前测试状态
- 无自动化测试
- 主要通过游戏运行验证功能

### 调试工具
- `BattleSceneHades` 包含调试功能：
  - 空格键暂停战斗
  - 绘制攻击范围（红色圆圈）
  - 显示冷却时间

---

## 常见问题 (FAQ)

### Q: 如何切换战斗模式？
A: 修改 `config/kysmod.ini` 中的 `battle_mode`：
- 0 = 回合制
- 1 = 半即时
- 2 = Hades 风格（自动战棋，**主要模式**）
- 3 = Sekiro 风格

### Q: 如何添加新角色到棋池？
A:
1. 修改 `config/chess_pool.yaml`，在对应费用等级下添加角色 ID
2. 使用 SQLite 工具打开 `game/save/0.db`，修改 `role` 表中的角色属性

### Q: 如何调整平衡性？
A: 修改 `config/chess_balance_*.yaml`，包括：
- 星级加成
- 经济参数
- 敌人表
- 商店权重

### Q: 战斗系统的主要参考是哪个？
A: **BattleSceneHades**（`BattleSceneHades.h` / `.cpp`）是当前主要的战斗模式，所有战斗相关的开发和参考都应以此为准。

---

## 相关文件清单

### 战斗系统
- `BattleScene.h` / `.cpp`：战斗场景基类
- `BattleSceneAct.h` / `.cpp`：即时战斗基类
- `BattleSceneHades.h` / `.cpp`：自动战棋（**主要参考**）
- `BattleSceneSekiro.h` / `.cpp`：只狼风格
- `BattleScenePaper.h` / `.cpp`：纸片战斗
- `BattleMap.h` / `.cpp`：战斗地图
- `BattleCursor.h` / `.cpp`：战斗光标
- `BattleMenu.h` / `.cpp`：战斗菜单
- `BattleNetwork.h` / `.cpp`：网络对战

### 自走棋系统
- `Chess.h` / `.cpp`：自走棋主逻辑
- `ChessBalance.h` / `.cpp`：平衡系统
- `ChessPool.h` / `.cpp`：棋池系统
- `ChessCombo.h` / `.cpp`：羁绊系统
- `ChessNeigong.h` / `.cpp`：内功系统
- `ChessSelector.h` / `.cpp`：棋子选择器
- `ChessUIStatus.h` / `.cpp`：自走棋状态界面
- `ChessBattleEffects.h` / `.cpp`：战斗效果
- `ChessModHook.h` / `.cpp`：模组钩子

### UI 系统
- `UI.h` / `.cpp`：UI 基类
- `UIStatus.h` / `.cpp`：角色状态界面
- `UIShop.h` / `.cpp`：商店界面
- `UIItem.h` / `.cpp`：物品界面
- `UISave.h` / `.cpp`：存档界面
- `UISystem.h` / `.cpp`：系统界面
- `UIKeyConfig.h` / `.cpp`：按键配置界面
- `UIRoleStatusMenu.h` / `.cpp`：角色状态菜单
- `UIStatusDrawable.h` / `.cpp`：状态绘制组件

### 场景系统
- `Scene.h` / `.cpp`：场景基类
- `MainScene.h` / `.cpp`：主地图场景
- `SubScene.h` / `.cpp`：子场景
- `TitleScene.h` / `.cpp`：标题场景

### 资源管理
- `Save.h` / `.cpp`：存档系统
- `TextureManager.h` / `.cpp`：纹理管理
- `Audio.h` / `.cpp`：音频管理
- `GrpIdxFile.h` / `.cpp`：资源文件读取

### 引擎核心
- `Application.h` / `.cpp`：应用入口
- `Engine.h` / `.cpp`：引擎核心
- `RunNode.h` / `.cpp`：节点系统
- `Camera.h` / `.cpp`：摄像机
- `Event.h` / `.cpp`：事件系统

### 其他组件
- `Head.h` / `.cpp`：角色头像
- `Menu.h` / `.cpp`：菜单
- `TextBox.h` / `.cpp`：文本框
- `Button.h` / `.cpp`：按钮
- `Point.h` / `.cpp`：坐标点
- `Weather.h` / `.cpp`：天气效果
- `Cloud.h` / `.cpp`：云朵效果
- `ParticleSystem.h` / `.cpp`：粒子系统
- `ChemistryEngine.h` / `.cpp`：化学引擎（特效）

### 工具类
- `GameUtil.h` / `.cpp`：游戏工具函数
- `Types.h` / `.cpp`：类型定义
- `Random.h`：随机数生成
- `Script.h` / `.cpp`：脚本系统

---

## 文件统计

- **总文件数**：约 150+ 个 C++ 源文件
- **代码行数**：估计 50,000+ 行
- **主要语言**：C++23
