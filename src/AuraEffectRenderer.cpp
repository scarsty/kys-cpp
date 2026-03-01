#include "AuraEffectRenderer.h"
#include "Head.h"
#include <cmath>

// 生成确定性随机种子
uint32_t AuraEffectRenderer::generateSeed(const BattleSceneAct::AttackEffect& ae) const
{
    return (uint32_t)(ae.Pos.x * 1337 + ae.Pos.y * 31337 + (ae.Attacker ? ae.Attacker->ID : 0));
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

// 绘制真气飘带（沿攻击方向/弹道）
void AuraEffectRenderer::renderQiRibbon(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                                        Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 计算飘带方向（使用速度向量）
    float dir_x = ae.Velocity.x;
    float dir_y = ae.Velocity.y;
    float magnitude = sqrt(dir_x * dir_x + dir_y * dir_y);

    // 如果没有速度，使用默认向右方向
    if (magnitude < 0.1f) {
        dir_x = 1.0f;
        dir_y = 0.0f;
        magnitude = 1.0f;
    }

    // 归一化方向
    dir_x /= magnitude;
    dir_y /= magnitude;

    // 飘带参数
    const int RIBBON_LENGTH = 40;  // 飘带长度（像素段数）
    const int RIBBON_WIDTH_START = 4;  // 起始宽度（细）
    const int RIBBON_WIDTH_END = 72;   // 末端宽度（粗，匹配攻击范围72px）

    int cx = ae.Pos.x;
    int cy = ae.Pos.y / 2;

    // 逐段绘制飘带（向后延伸，飘逸效果）
    for (int i = 0; i < RIBBON_LENGTH; ++i) {
        // 当前段的位置（沿方向反向延伸）
        float segment_ratio = (float)i / RIBBON_LENGTH;
        float offset = i * 2.0f;  // 每段间隔2像素

        // 自然弯曲：正弦波扰动（飘逸效果）
        float curve_offset = sin(ae.Frame * 0.1f + i * 0.15f) * 3.0f;

        // 计算当前段坐标（向后延伸，垂直方向弯曲）
        int x = cx - dir_x * offset + dir_y * curve_offset;
        int y = cy - dir_y * offset / 2 - dir_x * curve_offset / 2;

        // 宽度渐变：从4像素→72像素（细→粗）
        int width = RIBBON_WIDTH_START + (i * (RIBBON_WIDTH_END - RIBBON_WIDTH_START) / RIBBON_LENGTH);

        // 透明度渐变：从满值→20%（末端更透）
        Uint8 segment_alpha = (Uint8)(alpha * (1.0f - segment_ratio * 0.8f));
        if (segment_alpha < 10) continue;

        SDL_SetRenderDrawColor(renderer, r, g, b, segment_alpha);

        // 绘制当前段（精确宽度）
        for (int w = 0; w < width; ++w) {
            int px = x - width / 2 + w;
            SDL_RenderPoint(renderer, px, y);
        }
    }
}

// 绘制飘带末端散点粒子
void AuraEffectRenderer::renderRibbonParticles(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                                               Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 计算飘带方向
    float dir_x = ae.Velocity.x;
    float dir_y = ae.Velocity.y;
    float magnitude = sqrt(dir_x * dir_x + dir_y * dir_y);

    if (magnitude < 0.1f) {
        dir_x = 1.0f;
        dir_y = 0.0f;
        magnitude = 1.0f;
    }

    dir_x /= magnitude;
    dir_y /= magnitude;

    uint32_t local_seed = generateSeed(ae);
    auto rand_int = [&local_seed]() -> uint32_t {
        local_seed = local_seed * 1103515245 + 12345;
        return (local_seed / 65536) % 32768;
    };

    int cx = ae.Pos.x;
    int cy = ae.Pos.y / 2;

    // 飘带末端区域（40-60像素）
    const int particle_count = 12;
    for (int i = 0; i < particle_count; ++i) {
        // 末端位置 + 随机偏移
        float base_offset = 40.0f + (rand_int() % 20);
        float side_offset = (rand_int() % 10) - 5.0f;

        int p_x = cx - dir_x * base_offset + dir_y * side_offset;
        int p_y = cy - dir_y * base_offset / 2 - dir_x * side_offset / 2;

        // 淡透明度
        Uint8 p_alpha = alpha / 3;
        if (p_alpha < 15) continue;

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
