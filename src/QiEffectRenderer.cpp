#include "QiEffectRenderer.h"
#include "Head.h"
#include <cmath>
#include <algorithm>

namespace QiEffect
{

// ============================================================================
// Renderer 实现
// ============================================================================

Renderer::Renderer()
{
}

void Renderer::render(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae, int current_frame)
{
    if (ae.Frame >= ae.TotalFrame || !ae.UsingMagic) return;
    if (!ae.Defender.empty()) return;  // 命中后消失

    // 推断视觉参数
    VisualParams params = inferParams(ae.UsingMagic);

    // 计算基础透明度（随时间衰减）
    float max_frame = ae.TotalFrame * 1.25f;
    float progress = (float)ae.Frame / max_frame;
    if (progress > 1.0f) progress = 1.0f;

    Uint8 base_alpha = (Uint8)(100 * (1.0f - progress) * global_brightness_);
    if (base_alpha <= 5) return;

    // 获取属性颜色
    bool is_friendly = (ae.Attacker && ae.Attacker->Team == 0);
    Uint8 r, g, b;
    getAttributeColor(params.attribute, is_friendly, params.temperature, r, g, b);

    // 设置混合模式
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 根据形状渲染
    renderByShape(renderer, ae, params, r, g, b, base_alpha);

    // 粒子特效（可选）
    if (enable_particles_)
    {
        renderParticles(renderer, ae, params, r, g, b, base_alpha);
    }

    // 调试：命中范围圆圈
    if (show_hit_range_)
    {
        renderHitRangeCircle(renderer, ae, r, g, b, base_alpha);
    }
}

// ============================================================================
// 推断逻辑
// ============================================================================

VisualParams Renderer::inferParams(const Magic* magic) const
{
    VisualParams params;

    if (!magic)
    {
        // 默认参数
        params.shape = Shape::SpecialAura;
        params.attribute = Attribute::None;
        return params;
    }

    // 根据武功类型推断形状和属性
    switch (magic->MagicType)
    {
        case 1:  // 拳法 → 圆形冲击波（降龙十八掌风格）
            params.shape = Shape::FistWave;
            params.attribute = Attribute::Yang;
            params.compression = Compression::Explosive;
            params.turbulence = 0.05f;
            params.particle_count = 80;
            params.temperature = 1.5f;  // 高温（金黄偏红）
            break;

        case 2:  // 剑法 → 细长剑气
            params.shape = Shape::SwordQi;
            params.attribute = Attribute::Metal;
            params.compression = Compression::Compressed;
            params.turbulence = 0.01f;  // 极低扰动（笔直）
            params.trail_length = 40.0f;
            params.width_max = 16.0f;
            params.width_min = 2.0f;
            params.particle_count = 40;
            params.temperature = 0.9f;  // 正常温度
            break;

        case 3:  // 刀法 → 宽厚刀气
            params.shape = Shape::BladeQi;
            params.attribute = Attribute::Fire;
            params.compression = Compression::Normal;
            params.turbulence = 0.03f;  // 低扰动
            params.trail_length = 35.0f;
            params.width_max = 48.0f;
            params.width_min = 6.0f;
            params.particle_count = 50;
            params.temperature = 1.3f;  // 高温（偏红）
            break;

        case 4:  // 特殊 → 飘带/灵气
            params.shape = Shape::SpecialAura;
            params.attribute = Attribute::Yin;
            params.compression = Compression::Normal;
            params.turbulence = 0.08f;
            params.trail_length = 50.0f;
            params.width_max = 36.0f;
            params.width_min = 3.0f;
            params.particle_count = 50;
            params.temperature = 0.7f;  // 低温（偏蓝紫）
            break;

        default:  // 其他 → 掌风
            params.shape = Shape::PalmWind;
            params.attribute = Attribute::Wind;
            params.compression = Compression::Dispersed;
            params.turbulence = 0.1f;
            params.particle_count = 60;
            params.temperature = 1.0f;
            break;
    }

    return params;
}

void Renderer::renderByShape(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                             const VisualParams& params, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    switch (params.shape)
    {
        case Shape::FistWave:
            renderFistWave(renderer, ae, params, r, g, b, alpha);
            break;
        case Shape::SwordQi:
            renderSwordQi(renderer, ae, params, r, g, b, alpha);
            break;
        case Shape::BladeQi:
            renderBladeQi(renderer, ae, params, r, g, b, alpha);
            break;
        case Shape::PalmWind:
            renderPalmWind(renderer, ae, params, r, g, b, alpha);
            break;
        case Shape::SpecialAura:
        default:
            renderSpecialAura(renderer, ae, params, r, g, b, alpha);
            break;
    }
}

// ============================================================================
// 形状渲染实现
// ============================================================================

void Renderer::renderFistWave(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                              const VisualParams& params, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 拳劲：圆形冲击波（3层扩散波纹）
    int cx = ae.Pos.x;
    int cy = ae.Pos.y / 2;

    const int MAX_RADIUS = 50;
    const int WAVE_COUNT = 3;

    // 压缩倍率影响范围
    float comp_mult = getCompressionMultiplier(params.compression);
    int actual_radius = (int)(MAX_RADIUS * comp_mult);

    for (int wave = 0; wave < WAVE_COUNT; ++wave)
    {
        float wave_offset = (ae.Frame + wave * 8) % 30;
        int radius = (int)(wave_offset * 2.0f);
        if (radius > actual_radius) continue;

        float alpha_ratio = 1.0f - (float)radius / actual_radius;
        Uint8 wave_alpha = (Uint8)(alpha * alpha_ratio);
        if (wave_alpha < 10) continue;

        SDL_SetRenderDrawColor(renderer, r, g, b, wave_alpha);

        // Bresenham 圆算法
        int x = 0, y = radius;
        int d = 3 - 2 * radius;

        auto drawPoints = [&](int px, int py)
        {
            SDL_RenderPoint(renderer, cx + px, cy + py);
            SDL_RenderPoint(renderer, cx - px, cy + py);
            SDL_RenderPoint(renderer, cx + px, cy - py);
            SDL_RenderPoint(renderer, cx - px, cy - py);
            SDL_RenderPoint(renderer, cx + py, cy + px);
            SDL_RenderPoint(renderer, cx - py, cy + px);
            SDL_RenderPoint(renderer, cx + py, cy - px);
            SDL_RenderPoint(renderer, cx - py, cy - px);
        };

        while (y >= x)
        {
            drawPoints(x, y);
            x++;
            if (d > 0)
            {
                y--;
                d = d + 4 * (x - y) + 10;
            }
            else
            {
                d = d + 4 * x + 6;
            }
        }
    }
}

void Renderer::renderSwordQi(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                             const VisualParams& params, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 剑气：细长锋利的气刃拖尾
    if (!ae.Attacker) return;

    float dir_x = ae.Velocity.x;
    float dir_y = ae.Velocity.y;
    float magnitude = std::sqrt(dir_x * dir_x + dir_y * dir_y);

    if (magnitude < 0.1f)
    {
        dir_x = ae.Pos.x - ae.Attacker->Pos.x;
        dir_y = ae.Pos.y - ae.Attacker->Pos.y;
        magnitude = std::sqrt(dir_x * dir_x + dir_y * dir_y);
        if (magnitude < 0.1f) return;
    }

    dir_x /= magnitude;
    dir_y /= magnitude;

    int TRAIL_LENGTH = (int)params.trail_length;
    const float SEGMENT_SPACING = 1.2f;

    int head_x = ae.Pos.x;
    int head_y = ae.Pos.y / 2;

    float time = ae.Frame * 0.18f;

    for (int i = 0; i < TRAIL_LENGTH; ++i)
    {
        float ratio = (float)i / TRAIL_LENGTH;
        float offset = i * SEGMENT_SPACING;

        float x = head_x - dir_x * offset;
        float y = head_y - dir_y * offset * 0.5f;

        // 宽度：从粗到细
        int width = (int)(params.width_max - (params.width_max - params.width_min) * ratio);
        if (width < 2) width = 2;

        // 透明度：从亮到暗
        float alpha_ratio = 1.0f - ratio * 0.8f;
        Uint8 segment_alpha = (Uint8)(alpha * alpha_ratio);
        if (segment_alpha < 10) continue;

        // 极低湍流（剑气笔直）
        float turbulence = perlinNoise(i * 0.1f + time) * params.turbulence * 10.0f;
        int render_x = (int)(x + dir_y * turbulence);
        int render_y = (int)(y - dir_x * turbulence * 0.5f);

        SDL_SetRenderDrawColor(renderer, r, g, b, segment_alpha);

        for (int w = 0; w < width; ++w)
        {
            int px = render_x - width / 2 + w;
            SDL_RenderPoint(renderer, px, render_y);
        }
    }
}

void Renderer::renderBladeQi(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                             const VisualParams& params, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 刀气：宽厚霸道的气刃拖尾
    if (!ae.Attacker) return;

    float dir_x = ae.Velocity.x;
    float dir_y = ae.Velocity.y;
    float magnitude = std::sqrt(dir_x * dir_x + dir_y * dir_y);

    if (magnitude < 0.1f)
    {
        dir_x = ae.Pos.x - ae.Attacker->Pos.x;
        dir_y = ae.Pos.y - ae.Attacker->Pos.y;
        magnitude = std::sqrt(dir_x * dir_x + dir_y * dir_y);
        if (magnitude < 0.1f) return;
    }

    dir_x /= magnitude;
    dir_y /= magnitude;

    int TRAIL_LENGTH = (int)params.trail_length;
    const float SEGMENT_SPACING = 1.8f;

    int head_x = ae.Pos.x;
    int head_y = ae.Pos.y / 2;

    float time = ae.Frame * 0.18f;

    for (int i = 0; i < TRAIL_LENGTH; ++i)
    {
        float ratio = (float)i / TRAIL_LENGTH;
        float offset = i * SEGMENT_SPACING;

        float x = head_x - dir_x * offset;
        float y = head_y - dir_y * offset * 0.5f;

        // 宽度：从粗到细（刀气宽厚）
        int width = (int)(params.width_max - (params.width_max - params.width_min) * ratio);
        if (width < 4) width = 4;

        // 透明度：从亮到暗
        float alpha_ratio = 1.0f - ratio * 0.7f;
        Uint8 segment_alpha = (Uint8)(alpha * alpha_ratio);
        if (segment_alpha < 10) continue;

        // 低湍流（刀气较直）
        float turbulence = perlinNoise(i * 0.1f + time) * params.turbulence * 15.0f;
        int render_x = (int)(x + dir_y * turbulence);
        int render_y = (int)(y - dir_x * turbulence * 0.5f);

        SDL_SetRenderDrawColor(renderer, r, g, b, segment_alpha);

        for (int w = 0; w < width; ++w)
        {
            int px = render_x - width / 2 + w;
            SDL_RenderPoint(renderer, px, render_y);
        }
    }
}

void Renderer::renderPalmWind(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                              const VisualParams& params, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 掌风：扇形气浪
    if (!ae.Attacker) return;

    float dir_x = ae.Velocity.x;
    float dir_y = ae.Velocity.y;
    float magnitude = std::sqrt(dir_x * dir_x + dir_y * dir_y);

    if (magnitude < 0.1f)
    {
        dir_x = ae.Pos.x - ae.Attacker->Pos.x;
        dir_y = ae.Pos.y - ae.Attacker->Pos.y;
        magnitude = std::sqrt(dir_x * dir_x + dir_y * dir_y);
        if (magnitude < 0.1f) return;
    }

    int cx = ae.Pos.x;
    int cy = ae.Pos.y / 2;

    float base_angle = std::atan2(dir_y, dir_x);
    const float ANGLE_SPAN = M_PI / 3.0f;  // 60度扇形
    const int MAX_RADIUS = 60;

    // 压缩度影响范围（松散时范围更大）
    float comp_mult = getCompressionMultiplier(params.compression);
    int actual_radius = (int)(MAX_RADIUS * comp_mult);

    for (float angle = -ANGLE_SPAN / 2.0f; angle <= ANGLE_SPAN / 2.0f; angle += 0.05f)
    {
        for (int radius = 10; radius < actual_radius; radius += 3)
        {
            float total_angle = base_angle + angle;
            int px = cx + (int)(radius * std::cos(total_angle));
            int py = cy + (int)(radius * std::sin(total_angle) * 0.5f);

            float alpha_ratio = 1.0f - (float)radius / actual_radius;
            Uint8 segment_alpha = (Uint8)(alpha * alpha_ratio);
            if (segment_alpha < 10) continue;

            SDL_SetRenderDrawColor(renderer, r, g, b, segment_alpha);
            SDL_RenderPoint(renderer, px, py);
        }
    }
}

void Renderer::renderSpecialAura(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                                 const VisualParams& params, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 特殊飘带：通用拖尾效果
    if (!ae.Attacker) return;

    float dir_x = ae.Velocity.x;
    float dir_y = ae.Velocity.y;
    float magnitude = std::sqrt(dir_x * dir_x + dir_y * dir_y);

    if (magnitude < 0.1f)
    {
        dir_x = ae.Pos.x - ae.Attacker->Pos.x;
        dir_y = ae.Pos.y - ae.Attacker->Pos.y;
        magnitude = std::sqrt(dir_x * dir_x + dir_y * dir_y);
        if (magnitude < 0.1f) return;
    }

    dir_x /= magnitude;
    dir_y /= magnitude;

    int RIBBON_LENGTH = (int)params.trail_length;
    const float SEGMENT_SPACING = 1.5f;

    int head_x = ae.Pos.x;
    int head_y = ae.Pos.y / 2;

    float time = ae.Frame * 0.18f;

    for (int i = 0; i < RIBBON_LENGTH; ++i)
    {
        float ratio = (float)i / RIBBON_LENGTH;
        float offset = i * SEGMENT_SPACING;

        float x = head_x - dir_x * offset;
        float y = head_y - dir_y * offset * 0.5f;

        // 宽度：从粗到细
        int width = (int)(params.width_max - (params.width_max - params.width_min) * ratio);
        if (width < 3) width = 3;

        // 透明度：从亮到暗
        float alpha_ratio = 1.0f - ratio * 0.7f;
        Uint8 segment_alpha = (Uint8)(alpha * alpha_ratio);
        if (segment_alpha < 10) continue;

        // 湍流（飘逸效果）
        float turbulence =
            perlinNoise(i * 0.1f + time * 2.0f) * 1.5f +
            perlinNoise(i * 0.3f + time * 5.0f) * 0.6f +
            perlinNoise(i * 0.8f + time * 12.0f) * 0.2f;

        turbulence *= params.turbulence * 10.0f;

        int render_x = (int)(x + dir_y * turbulence);
        int render_y = (int)(y - dir_x * turbulence * 0.5f);

        SDL_SetRenderDrawColor(renderer, r, g, b, segment_alpha);

        for (int w = 0; w < width; ++w)
        {
            int px = render_x - width / 2 + w;
            SDL_RenderPoint(renderer, px, render_y);
        }
    }
}

// ============================================================================
// 辅助渲染
// ============================================================================

void Renderer::renderParticles(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                               const VisualParams& params, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    if (!ae.Attacker) return;

    int origin_x = ae.Attacker->Pos.x;
    int origin_y = ae.Attacker->Pos.y / 2;

    uint32_t seed = generateSeed(ae);
    auto rand_int = [&seed]() -> uint32_t
    {
        seed = seed * 1103515245 + 12345;
        return (seed / 65536) % 32768;
    };

    int particle_count = params.particle_count / 5;  // 减少粒子数
    for (int i = 0; i < particle_count; ++i)
    {
        int p_x = origin_x + (rand_int() % 20) - 10;
        int p_y = origin_y + (rand_int() % 20) - 10;

        Uint8 p_alpha = alpha / 4;
        if (p_alpha < 10) continue;

        SDL_SetRenderDrawColor(renderer, r, g, b, p_alpha);

        int size = (rand_int() % 2 == 0) ? 1 : 2;
        for (int dy = 0; dy < size; ++dy)
        {
            for (int dx = 0; dx < size; ++dx)
            {
                SDL_RenderPoint(renderer, p_x + dx, p_y + dy);
            }
        }
    }
}

void Renderer::renderHitRangeCircle(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                                    Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    const int HIT_RADIUS = 36;  // = TILE_W * 2
    SDL_SetRenderDrawColor(renderer, r, g, b, (Uint8)(alpha * 0.15f));

    int cx = ae.Pos.x;
    int cy = ae.Pos.y / 2;

    int x = 0, y = HIT_RADIUS;
    int d = 3 - 2 * HIT_RADIUS;

    auto drawPoints = [&](int px, int py)
    {
        SDL_RenderPoint(renderer, cx + px, cy + py);
        SDL_RenderPoint(renderer, cx - px, cy + py);
        SDL_RenderPoint(renderer, cx + px, cy - py);
        SDL_RenderPoint(renderer, cx - px, cy - py);
        SDL_RenderPoint(renderer, cx + py, cy + px);
        SDL_RenderPoint(renderer, cx - py, cy + px);
        SDL_RenderPoint(renderer, cx + py, cy - px);
        SDL_RenderPoint(renderer, cx - py, cy - px);
    };

    while (y >= x)
    {
        drawPoints(x, y);
        x++;
        if (d > 0)
        {
            y--;
            d = d + 4 * (x - y) + 10;
        }
        else
        {
            d = d + 4 * x + 6;
        }
    }
}

// ============================================================================
// 工具函数
// ============================================================================

void Renderer::getAttributeColor(Attribute attr, bool is_friendly, float temperature,
                                 Uint8& r, Uint8& g, Uint8& b) const
{
    // 基础颜色
    switch (attr)
    {
        case Attribute::Metal:  // 金（白金色）
            r = 220; g = 220; b = 240;
            break;
        case Attribute::Wood:   // 木（绿色）
            r = 120; g = 220; b = 120;
            break;
        case Attribute::Water:  // 水（蓝色）
            r = 100; g = 180; b = 255;
            break;
        case Attribute::Fire:   // 火（红色）
            r = 255; g = 120; b = 80;
            break;
        case Attribute::Earth:  // 土（黄色）
            r = 200; g = 160; b = 100;
            break;
        case Attribute::Yang:   // 阳（金黄色）
            r = 255; g = 220; b = 100;
            break;
        case Attribute::Yin:    // 阴（紫黑色）
            r = 160; g = 100; b = 200;
            break;
        default:  // None（白色）
            r = 220; g = 220; b = 220;
            break;
    }

    // 温度影响（高温偏红，低温偏蓝）
    if (temperature > 1.0f)
    {
        r = std::min(255, (int)(r * temperature));
        g = (Uint8)(g / temperature);
        b = (Uint8)(b / temperature);
    }
    else if (temperature < 0.9f)
    {
        r = (Uint8)(r * temperature);
        g = (Uint8)(g * temperature);
        b = std::min(255, (int)(b / temperature));
    }

    // 敌我识别：敌军降低亮度
    if (!is_friendly)
    {
        r = (Uint8)(r * 0.7f);
        g = (Uint8)(g * 0.7f);
        b = (Uint8)(b * 0.7f);
    }
}

float Renderer::getCompressionMultiplier(Compression comp) const
{
    return PhysicsLaws::compressionMultiplier(comp);
}

uint32_t Renderer::generateSeed(const BattleSceneAct::AttackEffect& ae) const
{
    return (uint32_t)(ae.Pos.x * 1337 + ae.Pos.y * 31337 + (ae.Attacker ? ae.Attacker->ID : 0));
}

float Renderer::perlinNoise(float x) const
{
    int xi = (int)x;
    float xf = x - xi;

    auto hash = [](int i) -> float
    {
        i = (i << 13) ^ i;
        return ((i * (i * i * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f - 1.0f;
    };

    float g0 = hash(xi);
    float g1 = hash(xi + 1);

    float t = smoothstep(xf);
    return g0 * (1.0f - t) + g1 * t;
}

float Renderer::smoothstep(float t) const
{
    return t * t * (3.0f - 2.0f * t);
}

} // namespace QiEffect
