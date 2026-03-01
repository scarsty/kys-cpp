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

    // 命中检测：如果已经命中敌人，立即消失
    if (!ae.Defender.empty()) return;

    // 1. 生命周期计算
    float max_frame = ae.TotalFrame * 1.25f;
    float progress = (float)ae.Frame / max_frame;
    if (progress > 1.0f) progress = 1.0f;

    // 淡出透明度（降低基础透明度，避免画面混乱）
    Uint8 base_alpha = (Uint8)(80 * (max_frame - ae.Frame) / max_frame);  // 150 → 80（降低47%）
    if (base_alpha <= 5) return;

    // 2. 颜色设定（敌我识别，降低饱和度）
    bool is_friendly = (ae.Attacker && ae.Attacker->Team == 0);
    Uint8 r, g, b;
    if (is_friendly) {
        r = 80; g = 180; b = 120;  // 友军：淡青绿色（降低饱和度）
    } else {
        r = 200; g = 100; b = 100;   // 敌军：淡红色（降低饱和度）
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

    // 计算飘带方向（使用速度向量）
    float dir_x = ae.Velocity.x;
    float dir_y = ae.Velocity.y;
    float magnitude = sqrt(dir_x * dir_x + dir_y * dir_y);

    // 如果没有速度，使用从角色到特效的方向
    if (magnitude < 0.1f) {
        dir_x = ae.Pos.x - ae.Attacker->Pos.x;
        dir_y = ae.Pos.y - ae.Attacker->Pos.y;
        magnitude = sqrt(dir_x * dir_x + dir_y * dir_y);
        if (magnitude < 0.1f) return;
    }

    // 归一化方向
    dir_x /= magnitude;
    dir_y /= magnitude;

    // 飘带参数（固定长度的彗星拖尾）
    const float WIDTH_MAX = 36.0f;  // 头部最大宽度
    const float WIDTH_MIN = 3.0f;   // 尾部最小宽度
    const int RIBBON_LENGTH = 50;   // 飘带固定长度（段数）
    const float SEGMENT_SPACING = 1.5f;  // 每段间距（像素）

    // 头部位置：特效位置（跟随武功移动）
    int head_x = ae.Pos.x;
    int head_y = ae.Pos.y / 2;

    // 时间参数（用于动画）
    float time = ae.Frame * 0.18f;

    // 逐段绘制飘带（从头部向后拖尾）
    for (int i = 0; i < RIBBON_LENGTH; ++i) {
        // 进度：0=头部（特效），1=尾部（向后拖尾）
        float ratio = (float)i / RIBBON_LENGTH;

        // 当前段向后的偏移距离
        float offset_distance = i * SEGMENT_SPACING;

        // === 流体动力学：分段延迟跟随 ===
        // 后段追赶前段，产生柔软的延迟感
        float delay_offset = ratio * 0.08f;  // 减小延迟系数（0.12→0.08）
        float delayed_offset = offset_distance + delay_offset * 8.0f;

        // 位置计算（从头部向后拖尾）
        float x = head_x - dir_x * delayed_offset;
        float y = head_y - dir_y * delayed_offset / 2;

        // 宽度：从粗到细（头部→尾部）
        float width = WIDTH_MAX - (WIDTH_MAX - WIDTH_MIN) * ratio;

        // 透明度：从亮到暗（头部→尾部）
        float alpha_ratio = 1.0f - ratio * 0.7f;  // 100%→30%
        Uint8 segment_alpha = (Uint8)(alpha * alpha_ratio);
        if (segment_alpha < 10) continue;

        // === 流体动力学：减小湍流摆动 ===
        float turbulence =
            perlinNoise(i * 0.1f + time * 2.0f) * 1.5f +      // 大尺度波动（3.5→1.5）
            perlinNoise(i * 0.3f + time * 5.0f) * 0.6f +      // 中等波动（1.2→0.6）
            perlinNoise(i * 0.8f + time * 12.0f) * 0.2f;      // 细节抖动（0.4→0.2）

        // 速度影响：速度越快，湍流影响越小（飘带越直）
        float speed = sqrt(ae.Velocity.x * ae.Velocity.x + ae.Velocity.y * ae.Velocity.y);
        float turbulence_scale = 1.0f / (1.0f + speed * 0.02f);
        turbulence *= turbulence_scale;

        // 添加湍流偏移（垂直于前进方向）
        int render_x = (int)(x + dir_y * turbulence);
        int render_y = (int)(y - dir_x * turbulence / 2);

        SDL_SetRenderDrawColor(renderer, r, g, b, segment_alpha);

        // 绘制当前段（精确宽度）
        int width_int = (int)width;
        for (int w = 0; w < width_int; ++w) {
            int px = render_x - width_int / 2 + w;
            SDL_RenderPoint(renderer, px, render_y);
        }
    }

    // === 绘制实际命中范围圆圈（极淡，避免画面混乱） ===
    // 如果还是太乱，可以注释掉这整块代码
    const int HIT_RADIUS = 36;
    SDL_SetRenderDrawColor(renderer, r, g, b, (Uint8)(alpha * 0.12f));  // 12% 透明度（极淡）

    // 绘制圆圈轮廓（Bresenham 圆算法）
    int cx = head_x;
    int cy = head_y;
    int x_circle = 0;
    int y_circle = HIT_RADIUS;
    int d = 3 - 2 * HIT_RADIUS;

    auto drawCirclePoints = [&](int x, int y) {
        SDL_RenderPoint(renderer, cx + x, cy + y);
        SDL_RenderPoint(renderer, cx - x, cy + y);
        SDL_RenderPoint(renderer, cx + x, cy - y);
        SDL_RenderPoint(renderer, cx - x, cy - y);
        SDL_RenderPoint(renderer, cx + y, cy + x);
        SDL_RenderPoint(renderer, cx - y, cy + x);
        SDL_RenderPoint(renderer, cx + y, cy - x);
        SDL_RenderPoint(renderer, cx - y, cy - x);
    };

    while (y_circle >= x_circle) {
        drawCirclePoints(x_circle, y_circle);
        x_circle++;
        if (d > 0) {
            y_circle--;
            d = d + 4 * (x_circle - y_circle) + 10;
        } else {
            d = d + 4 * x_circle + 6;
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
