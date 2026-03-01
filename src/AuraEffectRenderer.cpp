#include "AuraEffectRenderer.h"
#include "Head.h"
#include <cmath>

// 生成确定性随机种子
uint32_t AuraEffectRenderer::generateSeed(const BattleSceneAct::AttackEffect& ae) const
{
    return (uint32_t)(ae.Pos.x * 1337 + ae.Pos.y * 31337 + (ae.Attacker ? ae.Attacker->ID : 0));
}

// 平滑插值函数（3次 Hermite 插值）
float AuraEffectRenderer::smoothstep(float t) const
{
    return t * t * (3.0f - 2.0f * t);
}

// 简化版 Perlin 噪声（1D）
float AuraEffectRenderer::perlinNoise(float x) const
{
    int xi = (int)x;
    float xf = x - xi;

    // 伪随机梯度（基于整数坐标）
    auto hash = [](int i) -> float {
        i = (i << 13) ^ i;
        return ((i * (i * i * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f - 1.0f;
    };

    float g0 = hash(xi);
    float g1 = hash(xi + 1);

    // 平滑插值
    float t = smoothstep(xf);
    return g0 * (1.0f - t) + g1 * t;
}

// 渲染主函数
void AuraEffectRenderer::render(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae, int current_frame)
{
    if (ae.Frame >= ae.TotalFrame || !ae.UsingMagic) return;

    // 1. 生命周期计算
    float max_frame = ae.TotalFrame * 1.25f;
    float progress = (float)ae.Frame / max_frame;
    if (progress > 1.0f) progress = 1.0f;

    // 淡出透明度
    Uint8 base_alpha = (Uint8)(150 * (max_frame - ae.Frame) / max_frame);
    if (base_alpha <= 5) return;

    // 2. 颜色设定（敌我识别，红绿色便于测试）
    bool is_friendly = (ae.Attacker && ae.Attacker->Team == 0);
    Uint8 r, g, b;
    if (is_friendly) {
        r = 50; g = 255; b = 100;  // 友军：鲜绿色
    } else {
        r = 255; g = 50; b = 50;   // 敌军：鲜红色
    }

    // 3. 分层渲染
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    renderQiRibbon(renderer, ae, r, g, b, base_alpha);
    renderRibbonParticles(renderer, ae, r, g, b, base_alpha);
}

// 绘制真气飘带（从角色流向特效，头部最亮）
void AuraEffectRenderer::renderQiRibbon(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                                        Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 检查是否有攻击者
    if (!ae.Attacker) return;

    // 起点：角色位置
    Pointf start_pos = ae.Attacker->Pos;
    // 终点：特效位置（头部）
    Pointf end_pos = ae.Pos;

    // 计算从起点到终点的向量
    float dx = end_pos.x - start_pos.x;
    float dy = end_pos.y - start_pos.y;
    float distance = sqrt(dx * dx + dy * dy);

    // 如果距离太小，不绘制
    if (distance < 1.0f) return;

    // 归一化方向
    float dir_x = dx / distance;
    float dir_y = dy / distance;

    // 飘带参数（调整为更细的宽度）
    const float WIDTH_MAX = 24.0f;  // 头部最大宽度（缩小到原来的1/3）
    const float WIDTH_MIN = 3.0f;   // 尾部最小宽度

    // 分段数量（根据距离动态调整）
    int segments = (int)(distance / 2.0f);  // 每2像素一段
    if (segments < 10) segments = 10;  // 最少10段
    if (segments > 100) segments = 100;  // 最多100段

    // 时间参数（用于动画，加快飘动速度）
    float time = ae.Frame * 0.18f;  // 从 0.08 提升到 0.18（2.25倍速）

    // 逐段绘制飘带（从起点角色到终点特效）
    for (int i = 0; i <= segments; ++i) {
        // 进度：0=起点（角色），1=终点（特效头部）
        float ratio = (float)i / segments;

        // === 流体动力学：分段延迟跟随 ===
        // 后段追赶前段，产生柔软的延迟感
        float delay_offset = (1.0f - ratio) * 0.12f;  // 减小延迟（0.15→0.12）
        float delayed_ratio = ratio - delay_offset;
        if (delayed_ratio < 0.0f) delayed_ratio = 0.0f;

        // 位置插值（使用延迟后的比例）
        float x = start_pos.x + dx * delayed_ratio;
        float y = start_pos.y + dy * delayed_ratio;

        // 宽度：从细到粗（角色→特效）
        float width = WIDTH_MIN + (WIDTH_MAX - WIDTH_MIN) * ratio;

        // 透明度：从暗到亮（角色→特效）
        float alpha_ratio = 0.3f + 0.7f * ratio;  // 30%→100%
        Uint8 segment_alpha = (Uint8)(alpha * alpha_ratio);
        if (segment_alpha < 10) continue;

        // === 流体动力学：多层 Perlin 噪声湍流（减小抖动幅度）===
        float turbulence =
            perlinNoise(i * 0.1f + time * 2.0f) * 3.5f +      // 大尺度波动（6.0→3.5）
            perlinNoise(i * 0.3f + time * 5.0f) * 1.2f +      // 中等波动（2.5→1.2）
            perlinNoise(i * 0.8f + time * 12.0f) * 0.4f;      // 细节抖动（0.8→0.4）

        // 速度影响：速度越快，湍流影响越小（飘带越直）
        float speed = sqrt(ae.Velocity.x * ae.Velocity.x + ae.Velocity.y * ae.Velocity.y);
        float turbulence_scale = 1.0f / (1.0f + speed * 0.02f);  // 速度快时衰减
        turbulence *= turbulence_scale;

        // 添加湍流偏移（垂直于前进方向）
        int render_x = (int)(x + dir_y * turbulence);
        int render_y = (int)(y / 2 - dir_x * turbulence / 2);

        SDL_SetRenderDrawColor(renderer, r, g, b, segment_alpha);

        // 绘制当前段（精确宽度）
        int width_int = (int)width;
        for (int w = 0; w < width_int; ++w) {
            int px = render_x - width_int / 2 + w;
            SDL_RenderPoint(renderer, px, render_y);
        }
    }
}

// 绘制飘带起点散点粒子（从角色位置散发）
void AuraEffectRenderer::renderRibbonParticles(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                                               Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 检查是否有攻击者
    if (!ae.Attacker) return;

    // 粒子从角色位置散发
    int origin_x = ae.Attacker->Pos.x;
    int origin_y = ae.Attacker->Pos.y / 2;

    // 计算飘带方向
    float dx = ae.Pos.x - ae.Attacker->Pos.x;
    float dy = ae.Pos.y - ae.Attacker->Pos.y;
    float magnitude = sqrt(dx * dx + dy * dy);

    if (magnitude < 0.1f) return;

    float dir_x = dx / magnitude;
    float dir_y = dy / magnitude;

    uint32_t local_seed = generateSeed(ae);
    auto rand_int = [&local_seed]() -> uint32_t {
        local_seed = local_seed * 1103515245 + 12345;
        return (local_seed / 65536) % 32768;
    };

    // 角色周围散发的粒子（真气涌出效果）
    const int particle_count = 15;
    for (int i = 0; i < particle_count; ++i) {
        // 起点附近随机偏移（沿飘带方向小范围散布）
        float forward_offset = (rand_int() % 15);  // 向前0-15像素
        float side_offset = (rand_int() % 12) - 6.0f;  // 左右±6像素

        int p_x = origin_x + dir_x * forward_offset + dir_y * side_offset;
        int p_y = origin_y + dir_y * forward_offset / 2 - dir_x * side_offset / 2;

        // 淡透明度（起点粒子相对暗淡）
        Uint8 p_alpha = alpha / 4;
        if (p_alpha < 10) continue;

        SDL_SetRenderDrawColor(renderer, r, g, b, p_alpha);

        // 1x1 或 2x2 像素点
        int size = (rand_int() % 2 == 0) ? 1 : 2;
        for (int dy = 0; dy < size; ++dy) {
            for (int dx = 0; dx < size; ++dx) {
                SDL_RenderPoint(renderer, p_x + dx, p_y + dy);
            }
        }
    }
}
