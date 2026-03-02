#pragma once
#include "BattleSceneAct.h"
#include <SDL3/SDL.h>
#include <vector>

// ============================================================================
// 真气特效渲染器 - 统一版本
// 合并 QiParticleSystem (形状) + QiEnergySystem (功法)
// ============================================================================

namespace QiEffect
{

// ============================================================================
// 一、核心枚举（视觉表现）
// ============================================================================

// 真气形状（基于武功类型）
enum class Shape
{
    FistWave,      // 拳劲：圆形冲击波（降龙十八掌风格）
    SwordQi,       // 剑气：细长锋利的气刃
    BladeQi,       // 刀气：宽厚霸道的气刃
    PalmWind,      // 掌风：扇形气浪
    SpecialAura,   // 特殊：飘带/灵气球
};

// 真气属性（影响颜色）
enum class Attribute
{
    None,          // 无属性（白色）
    Metal,         // 金（白金色）
    Wood,          // 木（绿色）
    Water,         // 水（蓝色）
    Wind,          // 风（青绿）
    Fire,          // 火（红色）
    Earth,         // 土（黄色）
    Yang,          // 阳（金黄色）
    Yin,           // 阴（紫黑色）
};

// 真气压缩度（影响密度和范围）
enum class Compression
{
    Dispersed,     // 松散（范围大，密度低）
    Normal,        // 正常
    Compressed,    // 压缩（范围小，密度高）
    Explosive,     // 爆发（极度压缩）
};

// ============================================================================
// 二、视觉参数结构
// ============================================================================

struct VisualParams
{
    Shape shape = Shape::SpecialAura;
    Attribute attribute = Attribute::None;
    Compression compression = Compression::Normal;

    // 动画参数
    float turbulence = 0.1f;        // 扰动度（0=笔直，1=飘逸）
    float brightness = 1.0f;        // 亮度倍率
    float temperature = 1.0f;       // 温度（影响颜色）
    float speed_multiplier = 1.0f;  // 速度倍率

    // 形状特定参数
    int particle_count = 50;        // 粒子数量
    float trail_length = 50.0f;     // 拖尾长度
    float width_max = 36.0f;        // 最大宽度
    float width_min = 3.0f;         // 最小宽度
};

// ============================================================================
// 三、真气粒子（简化版，仅用于视觉）
// ============================================================================

struct Particle
{
    Pointf pos;
    Pointf velocity;
    float size;
    float brightness;
    float lifetime;
    float max_lifetime;
    uint32_t seed;
};

// ============================================================================
// DrawList：按 RGBA 分桶批处理点渲染
// ============================================================================

struct DrawList
{
    struct Bucket
    {
        Uint8 r = 0, g = 0, b = 0, a = 0;
        std::vector<SDL_FPoint> points;
    };

    void addPoint(int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
    void flush(SDL_Renderer* renderer);

private:
    static uint32_t makeKey(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
    {
        return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
    }

    std::unordered_map<uint32_t, size_t> bucket_index_;
    std::vector<Bucket> buckets;
};

// ============================================================================
// 四、真气特效渲染器
// ============================================================================

class Renderer
{
public:
    Renderer();
    ~Renderer() = default;

    // 渲染武功特效（主入口）
    void render(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae, int current_frame);

    // 调试：可视化弹道轨迹
    void renderDebugTrajectory(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae);

    // 设置全局参数
    void setGlobalBrightness(float brightness) { global_brightness_ = brightness; }
    void setEnableParticles(bool enable) { enable_particles_ = enable; }
    void setShowDebugTrajectory(bool enable) { show_debug_trajectory_ = enable; }

private:
    // 根据武功类型推断视觉参数
    VisualParams inferParams(const Magic* magic) const;

    // 头部：具象化形状模板（新）
    void renderHead(DrawList& draw_list, const BattleSceneAct::AttackEffect& ae,
                    const VisualParams& params, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 尾部：保留现有逻辑（输出到 DrawList）
    void renderTrailByShape(DrawList& draw_list, const BattleSceneAct::AttackEffect& ae,
                            const VisualParams& params, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // === 形状渲染函数 ===

    // 1. 拳劲：圆形冲击波（3层扩散波纹）
    void renderFistWave(DrawList& draw_list, const BattleSceneAct::AttackEffect& ae,
                       const VisualParams& params, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 2. 剑气：细长锋利的气刃拖尾
    void renderSwordQi(DrawList& draw_list, const BattleSceneAct::AttackEffect& ae,
                      const VisualParams& params, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 3. 刀气：宽厚霸道的气刃拖尾
    void renderBladeQi(DrawList& draw_list, const BattleSceneAct::AttackEffect& ae,
                      const VisualParams& params, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 4. 掌风：扇形气浪
    void renderPalmWind(DrawList& draw_list, const BattleSceneAct::AttackEffect& ae,
                       const VisualParams& params, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 5. 特殊飘带：通用拖尾效果
    void renderSpecialAura(DrawList& draw_list, const BattleSceneAct::AttackEffect& ae,
                          const VisualParams& params, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // === 辅助渲染函数 ===

    // 粒子特效（起点散发）
    void renderParticles(DrawList& draw_list, const BattleSceneAct::AttackEffect& ae,
                        const VisualParams& params, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 命中范围圆圈（调试用）
    void renderHitRangeCircle(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                             Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // === 颜色和工具函数 ===

    // 获取属性颜色
    void getAttributeColor(Attribute attr, bool is_friendly, float temperature,
                          Uint8& r, Uint8& g, Uint8& b) const;

    // 计算压缩度影响（宽度/密度倍率）
    float getCompressionMultiplier(Compression comp) const;

    // 生成确定性随机种子
    uint32_t generateSeed(const BattleSceneAct::AttackEffect& ae) const;

    // Perlin 噪声（湍流）
    float perlinNoise(float x) const;

    // 平滑插值
    float smoothstep(float t) const;

    // 获取 8 方向
    int getDirection8(const BattleSceneAct::AttackEffect& ae) const;

    // 形状名称映射
    const char* templateNameForShape(Shape shape) const;

private:
    float global_brightness_ = 1.0f;
    bool enable_particles_ = true;
    bool show_hit_range_ = true;  // 调试用：显示命中范围圆圈
    bool show_debug_trajectory_ = true;  // 调试用：显示弹道轨迹
};

// ============================================================================
// 五、物理定律（保留接口，供以后扩展）
// ============================================================================

namespace PhysicsLaws
{
    // 密度衰减定律：density(d) = density₀ × e^(-d/λ)
    inline float densityDecay(float initial_density, float distance, float decay_length = 50.0f)
    {
        return initial_density * std::exp(-distance / decay_length);
    }

    // 压缩倍率
    inline float compressionMultiplier(Compression comp)
    {
        switch (comp)
        {
            case Compression::Dispersed: return 0.3f;
            case Compression::Normal: return 1.0f;
            case Compression::Compressed: return 3.0f;
            case Compression::Explosive: return 10.0f;
            default: return 1.0f;
        }
    }

    // 扰动缩放：速度越快 → 扰动越小
    inline float turbulenceScale(float speed)
    {
        return 1.0f / (1.0f + speed * 0.05f);
    }

    // 五行相克倍率（保留，供以后伤害计算使用）
    inline float attributeCounter(Attribute attacker, Attribute defender)
    {
        // 金克木，木克土，土克水，水克火，火克金
        if (attacker == Attribute::Metal && defender == Attribute::Wood) return 1.5f;
        if (attacker == Attribute::Wood && defender == Attribute::Earth) return 1.5f;
        if (attacker == Attribute::Earth && defender == Attribute::Water) return 1.5f;
        if (attacker == Attribute::Water && defender == Attribute::Fire) return 1.5f;
        if (attacker == Attribute::Fire && defender == Attribute::Metal) return 1.5f;

        // 阴阳相克
        if (attacker == Attribute::Yang && defender == Attribute::Yin) return 1.3f;
        if (attacker == Attribute::Yin && defender == Attribute::Yang) return 1.3f;

        return 1.0f;
    }
}

} // namespace QiEffect
