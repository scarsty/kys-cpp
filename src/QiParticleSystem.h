#pragma once
#include "BattleSceneAct.h"
#include <SDL3/SDL.h>
#include <vector>
#include <cstdint>

// ============================================================================
// 真气物理系统 - 武侠风格粒子特效
// ============================================================================

namespace QiPhysics
{

// 真气形状枚举（基于武功类型）
enum class QiShape
{
    Ribbon,        // 飘带（默认）
    SwordQi,       // 剑气：细长锋利的气刃
    BladeQi,       // 刀气：宽厚霸道的气刃
    FistAura,      // 拳劲：圆形冲击波
    PalmWind,      // 掌风：扇形气浪
    SpecialOrb,    // 特殊：球形灵气
};

// 真气属性枚举（颜色与效果）
enum class QiAttribute
{
    None = 0,      // 无属性（白色）
    Yang,          // 阳刚（金黄色，拳法）
    Yin,           // 阴柔（紫黑色，半透明）
    Fire,          // 火（赤红色，溅射火花）
    Ice,           // 冰（冰蓝色，冰晶粒子、减速）
    Thunder,       // 雷（紫电，电光粒子）
    Wind,          // 风（青绿色，流动感强）
    Poison,        // 毒（暗绿色，瘴气粒子）
};

// 真气粒子结构
struct QiParticle
{
    Pointf pos;               // 粒子位置
    Pointf velocity;          // 粒子速度
    float lifetime;           // 剩余寿命（秒）
    float max_lifetime;       // 最大寿命
    float size;               // 粒子大小
    float brightness;         // 亮度（0-1）
    QiAttribute attribute;    // 属性（颜色与特效）
    QiShape shape;            // 形状
    float rotation;           // 旋转角度（用于剑气、刀气）
    uint32_t seed;            // 随机种子（用于确定性动画）
};

// 真气物理参数
struct QiPhysicsParams
{
    // 衰减参数
    float distance_decay_rate = 0.02f;        // 距离衰减率
    float brightness_decay_coeff = 50.0f;     // 亮度衰减系数（e^(-d/coeff)）
    float size_decay_coeff = 80.0f;           // 大小衰减系数

    // 粒子生成参数
    int particle_spawn_rate = 5;              // 每帧生成粒子数
    float particle_lifetime = 0.5f;           // 粒子寿命（秒）
    float particle_speed_variance = 0.3f;     // 速度随机性

    // 属性特效参数
    float fire_splash_chance = 0.3f;          // 火焰溅射概率
    float ice_crystal_chance = 0.2f;          // 冰晶生成概率
    float yin_transparency = 0.6f;            // 阴属性透明度

    // 物理参数
    float turbulence_strength = 2.0f;         // 湍流强度
    float gravity_y = 0.0f;                   // Y方向重力（0=无重力）
};

// 真气特效系统
class QiEffectSystem
{
public:
    QiEffectSystem();
    ~QiEffectSystem() = default;

    // 更新系统（每帧调用）
    void update(float delta_time);

    // 渲染所有粒子
    void render(SDL_Renderer* renderer, const Pointf& camera_offset);

    // 为攻击特效创建真气流
    void createQiFlow(const BattleSceneAct::AttackEffect& ae);

    // 清空所有粒子
    void clear();

    // 设置物理参数
    void setParams(const QiPhysicsParams& params) { params_ = params; }
    const QiPhysicsParams& getParams() const { return params_; }

private:
    // 根据武功类型推断真气形状
    QiShape inferShapeFromMagic(const Magic* magic) const;

    // 根据武功类型推断真气属性
    QiAttribute inferAttributeFromMagic(const Magic* magic) const;

    // 生成粒子
    void spawnParticles(const BattleSceneAct::AttackEffect& ae, QiShape shape,
                       QiAttribute attribute, int count);

    // 计算衰减后的亮度
    float calculateBrightness(const QiParticle& particle, float distance_from_source) const;

    // 计算衰减后的大小
    float calculateSize(const QiParticle& particle, float distance_from_source) const;

    // 获取属性颜色
    void getAttributeColor(QiAttribute attr, Uint8& r, Uint8& g, Uint8& b) const;

    // 渲染单个粒子（根据形状）
    void renderParticle(SDL_Renderer* renderer, const QiParticle& particle,
                        const Pointf& camera_offset);

    // 渲染剑气形状
    void renderSwordQi(SDL_Renderer* renderer, const QiParticle& particle,
                      const Pointf& screen_pos, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 渲染刀气形状
    void renderBladeQi(SDL_Renderer* renderer, const QiParticle& particle,
                      const Pointf& screen_pos, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 渲染拳劲形状
    void renderFistAura(SDL_Renderer* renderer, const QiParticle& particle,
                       const Pointf& screen_pos, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 渲染掌风形状
    void renderPalmWind(SDL_Renderer* renderer, const QiParticle& particle,
                       const Pointf& screen_pos, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 属性特效：火焰溅射
    void createFireSplash(const Pointf& pos, const Pointf& velocity, uint32_t seed);

    // 属性特效：冰晶粒子
    void createIceCrystal(const Pointf& pos, const Pointf& velocity, uint32_t seed);

    // Perlin噪声（用于湍流）
    float perlinNoise(float x) const;

    // 平滑插值
    float smoothstep(float t) const;

private:
    std::vector<QiParticle> particles_;       // 所有活跃粒子
    QiPhysicsParams params_;                  // 物理参数
    uint32_t global_seed_;                    // 全局随机种子

    // 粒子池（避免频繁分配）
    static constexpr size_t MAX_PARTICLES = 2000;
};

// 真气飘带渲染器（重构版）
class QiRibbonRenderer
{
public:
    QiRibbonRenderer() = default;
    ~QiRibbonRenderer() = default;

    // 渲染真气飘带（自动推断形状和属性）
    void render(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                int current_frame);

private:
    // 推断武功形状
    QiShape inferShape(const Magic* magic) const;

    // 推断武功属性
    QiAttribute inferAttribute(const Magic* magic) const;

    // 根据形状渲染
    void renderByShape(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                      QiShape shape, QiAttribute attribute);

    // 绘制主飘带（默认形状）
    void renderMainRibbon(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                         Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 绘制剑气
    void renderSwordQiTrail(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                           Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 绘制刀气
    void renderBladeQiTrail(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                           Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 绘制拳劲
    void renderFistAuraWave(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                           Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 绘制掌风
    void renderPalmWindFan(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                          Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 绘制飘带粒子
    void renderRibbonParticles(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                              Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 获取属性颜色
    void getAttributeColor(QiAttribute attr, bool is_friendly, Uint8& r, Uint8& g, Uint8& b) const;

    // 生成确定性随机种子
    uint32_t generateSeed(const BattleSceneAct::AttackEffect& ae) const;

    // Perlin噪声
    float perlinNoise(float x) const;

    // 平滑插值
    float smoothstep(float t) const;
};

} // namespace QiPhysics
