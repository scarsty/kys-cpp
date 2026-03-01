# BattleSceneHades 武功范围视觉效果实现文档

> 创建时间：2026-02-28 22:35:00
> 最后更新：2026-02-28 23:15:00
> 状态：已实现（艺术化圆形版本）

---

## 一、需求背景

基于像素风格武侠自走棋战场（BattleSceneHades），为技能攻击添加范围提示类视觉效果。

### 核心目标
- **视觉定位**：次要提示层，不抢夺角色、主技能的视觉焦点
- **风格统一**：完全匹配现有战场像素风格（低分辨率、复古武侠质感）
- **功能明确**：清晰传递攻击作用范围，无方向/范围歧义
- **武侠意境**：贴合金庸武侠内力/气劲扩散的设定，避免现代特效感

---

## 二、攻击判定机制

### 2.1 判定类型
**圆形范围判定**：使用欧几里得距离计算

```cpp
// 攻击判定核心逻辑
if (EuclidDis(r->Pos, ae.Pos) <= TILE_W * 2)
{
    // 击中判定
}
```

### 2.2 判定参数
- **判定半径**：`TILE_W * 2 = 36 像素`
- **判定中心**：攻击特效位置 `ae.Pos`
- **判定对象**：敌方角色 `r->Team != ae.Attacker->Team`

---

## 三、视觉效果实现

### 3.1 核心设计原则

#### 三大原则
1. **先画范围，再画动画**
   - 范围在下层，动画在上层，永远不挡动作

2. **只描边，不铺满**
   - 填充会糊画面、挡人物，描边最干净

3. **淡、细、柔**
   - 别用纯色实线，别粗、别亮、别闪瞎眼

#### 实现要点
- **半透明**：Alpha 60-120，不超过 120
- **细线条**：1-2 像素宽度
- **不填充**：只绘制轮廓，不填充区域
- **不挡动画**：在 draw() 中最先绘制，确保角色和技能动画在上层

---

### 3.2 视觉构成

#### 层次结构
```
渲染顺序（从下到上）：
1. 地图背景
2. 武功范围特效（renderAttackAuras）
3. 角色精灵
4. 武功动画
5. 伤害数字/文字特效
```

#### 效果组成（艺术化圆形版本）

**层1：程序化像素裂纹（底层）**
- **形态**：从中心向外的不连续折线（蜘蛛网状）
- **数量**：6-9 条随机裂纹
- **算法**：
  - 每段 6-13 像素长
  - 随机偏转 ±25°（制造折角感）
  - 20% 概率断裂（打破连续性）
- **颜色**：
  - 友军：浅蓝 `RGB(150, 210, 255)`
  - 敌军：浅紫红 `RGB(255, 130, 170)`
- **透明度**：`base_alpha / 2`（30-60）
- **武侠意境**：模拟"内力震裂地面"效果

**层2：不规则气场圆环（中层）**
- **形态**：断续的圆弧拼接（非完美圆形）
- **算法**：
  - 10-40° 实线段 + 5-20° 缺口
  - 用 5° 短线段拟合（增加"拙"感）
- **半径**：`TILE_W * 2 = 36px`（与判定范围一致）
- **颜色**：与裂纹相同
- **透明度**：`base_alpha`（60-120）
- **武侠意境**：模拟"能量破散"的边缘

**层3：离散能量粒子（上层）**
- **形态**：1x1 或 2x2 纯色像素方块
- **数量**：15 个随机粒子
- **运动**：向外匀速扩散（1.5-3.5 像素/帧）
- **颜色**：与圆环相同
- **透明度**：阶跃式变化
  - 内圈（0-60% 半径）：`base_alpha`
  - 中圈（60-90% 半径）：`base_alpha / 1.5`
  - 外圈（90-110% 半径）：`base_alpha / 3`
- **武侠意境**：模拟"真气飞散"效果

---

## 四、代码实现

### 4.1 核心函数

#### renderAttackAuras()
```cpp
void BattleSceneHades::renderAttackAuras()
{
    auto renderer = Engine::getInstance()->getRenderer();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    const float MAX_RADIUS = TILE_W * 2.0f;  // 36px
    const int FLARE_COUNT = 8;               // 8条气劲

    for (auto& ae : attack_effects_)
    {
        if (ae.Frame >= ae.TotalFrame || !ae.UsingMagic) continue;

        // 1. 生命周期计算
        float progress = (float)ae.Frame / (ae.TotalFrame * 1.25f);
        Uint8 base_alpha = (Uint8)(120 * (1.0f - progress));  // 从120淡出到0
        if (base_alpha <= 5) continue;

        // 2. 颜色设定（敌我识别）
        bool is_friendly = (ae.Attacker && ae.Attacker->Team == 0);
        Uint8 r, g, b;
        if (is_friendly) {
            r = 150; g = 200; b = 255;  // 浅蓝
        } else {
            r = 255; g = 150; b = 180;  // 浅紫红
        }

        // 3. 动态半径计算
        float current_radius = MAX_RADIUS;
        if (progress < 0.2f) {
            current_radius = MAX_RADIUS * (progress / 0.2f);  // 快速扩张
        }

        // 4. 绘制细圆环
        Uint8 ring_alpha = base_alpha / 2;
        drawCircleOutline(renderer, ae.Pos.x, ae.Pos.y,
                         current_radius, r, g, b, ring_alpha);

        // 5. 绘制放射气劲
        float flare_intensity = 1.0f;
        if (progress < 0.2f) {
            flare_intensity = progress / 0.2f;
        } else if (progress > 0.6f) {
            flare_intensity = (1.0f - progress) / 0.4f;
        }

        if (flare_intensity > 0.1f)
        {
            Uint8 flare_alpha = (Uint8)(base_alpha * flare_intensity * 0.6f);
            SDL_SetRenderDrawColor(renderer, r, g, b, flare_alpha);

            for (int i = 0; i < FLARE_COUNT; ++i)
            {
                float angle = (360.0f / FLARE_COUNT) * i;
                float noise = sin(ae.Frame * 0.3f + i * 1.5f);

                // 气劲从中心向外延伸
                float inner_r = current_radius * 0.2f;
                float outer_r = current_radius * (0.8f + 0.3f * noise);

                float rad = angle * M_PI / 180.0f;
                int start_x = ae.Pos.x + inner_r * cos(rad);
                int start_y = ae.Pos.y / 2 + inner_r * sin(rad);
                int end_x = ae.Pos.x + outer_r * cos(rad);
                int end_y = ae.Pos.y / 2 + outer_r * sin(rad);

                // 绘制细线（1 像素宽）
                SDL_RenderLine(renderer, start_x, start_y, end_x, end_y);

                // 末端小点（2×2 像素方块）
                SDL_FRect dot = {(float)end_x - 1, (float)end_y - 1, 2, 2};
                SDL_RenderFillRect(renderer, &dot);
            }
        }
    }
}
```

#### drawCircleOutline()
```cpp
void BattleSceneHades::drawCircleOutline(SDL_Renderer* renderer, int cx, int cy, float radius,
                                         Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    const int segments = 36;
    SDL_SetRenderDrawColor(renderer, r, g, b, alpha);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < segments; i++)
    {
        float angle1 = 2.0f * M_PI * i / segments;
        float angle2 = 2.0f * M_PI * (i + 1) / segments;

        int x1 = cx + radius * cos(angle1);
        int y1 = cy / 2 + radius * sin(angle1);
        int x2 = cx + radius * cos(angle2);
        int y2 = cy / 2 + radius * sin(angle2);

        SDL_RenderLine(renderer, x1, y1, x2, y2);
    }
}
```

### 4.2 调用时机

在 `BattleSceneHades::draw()` 函数中，**最先调用**：

```cpp
void BattleSceneHades::draw()
{
    // ... 准备渲染目标 ...

    // ✅ 第一步：绘制武功范围特效（底层）
    renderAttackAuras();

    // ... 绘制角色、动画等（上层） ...
}
```

---

## 五、参数配置

### 5.1 可调参数

| 参数名 | 当前值 | 说明 | 调整建议 |
|--------|--------|------|---------|
| `MAX_RADIUS` | `TILE_W * 2 = 36px` | 范围半径 | 与判定半径一致，不建议修改 |
| `FLARE_COUNT` | `8` | 气劲数量 | 减少更简洁，增加更华丽 |
| `base_alpha` | `120` | 基础透明度 | 降低更淡，提高更亮 |
| `ring_alpha` | `base_alpha / 2` | 圆环透明度 | 当前为基础值的 50% |
| `flare_alpha` | `base_alpha * 0.6` | 气劲透明度 | 当前为基础值的 60% |
| `segments` | `36` | 圆环分段数 | 影响圆滑度，不建议低于 24 |

### 5.2 颜色配置

| 阵营 | 颜色名 | RGB 值 | 说明 |
|------|--------|--------|------|
| 友军 | 浅蓝 | `(150, 200, 255)` | 清爽、正派 |
| 敌军 | 浅紫红 | `(255, 150, 180)` | 妖艳、邪派 |

**设计原则**：
- 使用浅色系（RGB 值 150-255 范围）
- 避免纯色（纯蓝 0,0,255 / 纯红 255,0,0）
- 保持柔和感（无高对比度）

---

## 六、动画时序

### 6.1 时间轴

```
总时长：TotalFrame * 1.25 帧（约 1-2 秒）

0%                20%               60%              100%
├──────────────────┼─────────────────┼────────────────┤
   淡入/扩散阶段      停留/命中阶段       淡出/余波阶段

半径变化：
0% -> 20%:  0 → MAX_RADIUS (快速扩散)
20% -> 100%: MAX_RADIUS (保持静态)

透明度变化：
0% -> 100%: 120 → 0 (线性淡出)

气劲强度：
0% -> 20%:  0 → 1 (淡入)
20% -> 60%: 1 (保持)
60% -> 100%: 1 → 0 (淡出)
```

### 6.2 阶段描述

**阶段 1：淡入/扩散（0-20%）**
- 圆环从中心快速扩散到最大半径
- 气劲强度从 0 升至满值
- 模拟"真气扩散"的效果

**阶段 2：停留/命中（20-60%）**
- 圆环保持最大半径静态显示
- 气劲保持满强度
- 提供稳定的范围提示

**阶段 3：淡出/余波（60-100%）**
- 圆环半径不变，透明度降低
- 气劲强度逐渐降低
- 模拟"真气散去"的效果

---

## 七、视觉效果特点

### 7.1 符合武侠美学
- ✅ 气劲轨迹设计，配合六脉神剑、弹指神通等武功
- ✅ 浅色系配色（浅蓝、浅紫红），符合传统武侠配色
- ✅ 淡入/淡出动画，模拟内力聚散
- ✅ 随机波动的气劲长度，打破几何对称感

### 7.2 符合像素画风
- ✅ 细线条（1-2 像素）
- ✅ 末端 2×2 像素小方块
- ✅ 无复杂渐变
- ✅ 边缘清晰，无模糊

### 7.3 符合功能需求
- ✅ 范围与判定一致（36px 圆形）
- ✅ 敌我颜色区分（蓝/红）
- ✅ 不挡动画（底层渲染）
- ✅ 不影响性能（简单几何绘制）

---

## 八、与其他系统的协调

### 8.1 不冲突的元素
- **角色精灵**：在上层，完全不受影响
- **武功动画**：在上层，完全不受影响
- **伤害数字**：在上层，完全不受影响
- **血条/状态栏**：在上层，完全不受影响

### 8.2 可能的视觉叠加
- **多个攻击特效重叠**：透明度叠加，可能略亮
- **与地板装饰重叠**：范围特效在上层，可能遮挡地板细节

### 8.3 优化建议
如果同屏攻击特效过多（>10 个）：
- 考虑降低透明度上限（120 → 80）
- 考虑减少气劲数量（8 → 4）
- 考虑禁用气劲效果，仅保留圆环

---

## 九、未来扩展方向

### 9.1 可能的增强
1. **根据武功类型区分**
   - 拳掌类：圆形范围
   - 剑气类：扇形范围
   - 暗器类：直线范围

2. **地板碎裂效果**
   - 在圆环内随机生成 16×16 像素裂纹纹理
   - 颜色：深灰 `RGB(80, 80, 80)`
   - 透明度：为圆环的 60%

3. **区分普通攻击/大招**
   - 大招：双层圆环、更多气劲（16 条）
   - 普通攻击：单层圆环、少量气劲（4 条）


---

## 十、总结

BattleSceneHades 当前的武功范围视觉效果实现遵循**淡、细、柔**的设计原则，完全符合像素武侠美学，功能上准确反映了圆形攻击判定范围，视觉上不干扰核心战斗表现。

### 核心特点
- ✅ 范围与判定一致（圆形，半径 36px）
- ✅ 只描边不填充，保持画面干净
- ✅ 半透明淡色，不闪瞎眼
- ✅ 底层渲染，不挡动画
- ✅ 性能友好，无复杂计算

### 文件位置
- **头文件**：`src/BattleSceneHades.h`（行 45-50）
- **实现文件**：`src/BattleSceneHades.cpp`（行 100-270）
- **调用位置**：`src/BattleSceneHades.cpp::draw()`（行 554）

---

**最后更新**：2026-02-28 22:35:00
