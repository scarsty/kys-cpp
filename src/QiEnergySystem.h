#pragma once
#include "BattleSceneAct.h"
#include <SDL3/SDL.h>
#include <vector>
#include <functional>
#include <string>

// ============================================================================
// 真气能量场系统 - 完整物理模型
// ============================================================================

namespace QiPhysics
{

// 前置声明
struct QiParticle;
struct QiField;
class QiFlowScript;

// ============================================================================
// 一、真气状态枚举
// ============================================================================

// 真气形态（内外两态）
enum class QiState
{
    Internal,      // 内功：在经脉中循环
    External,      // 外放：释放到战场空间
    Shield,        // 护体：环绕角色形成屏障
    Healing,       // 疗伤：传导给他人
};

// 真气属性（五行 + 阴阳）
enum class QiAttribute
{
    None = 0,      // 无属性
    Metal,         // 金（白色，锐利）
    Wood,          // 木（绿色，生机）
    Water,         // 水（蓝色，柔韧）
    Fire,          // 火（红色，爆裂）
    Earth,         // 土（黄色，厚重）
    Yang,          // 阳（金黄，刚猛）
    Yin,           // 阴（紫黑，阴柔）
};

// 真气压缩度
enum class QiCompression
{
    Dispersed,     // 松散（范围大，威力低）
    Normal,        // 正常
    Compressed,    // 压缩（范围小，威力高）
    Explosive,     // 爆发（极度压缩后释放）
};

// ============================================================================
// 二、真气粒子（能量微粒）
// ============================================================================

struct QiParticle
{
    Pointf pos;               // 位置
    Pointf velocity;          // 速度
    Pointf acceleration;      // 加速度

    float density;            // 密度（影响亮度和威力）
    float temperature;        // 温度（影响颜色）
    float turbulence;         // 扰动度（0=笔直，1=飘逸）

    float lifetime;           // 剩余寿命
    float max_lifetime;       // 最大寿命

    QiAttribute attribute;    // 属性
    QiState state;            // 状态

    Role* source;             // 发源角色
    Role* target;             // 目标角色（疗伤时）

    uint32_t seed;            // 随机种子
};

// ============================================================================
// 三、真气场（空间能量密度场）
// ============================================================================

struct QiField
{
    Pointf center;            // 场中心
    float radius;             // 作用半径
    float density;            // 平均密度
    float flow_speed;         // 流速
    Pointf flow_direction;    // 流向

    QiAttribute attribute;    // 属性
    QiCompression compression;// 压缩度

    // 密度场采样（返回指定点的真气密度）
    float sampleDensity(const Pointf& pos) const;

    // 速度场采样（返回指定点的流速和方向）
    Pointf sampleVelocity(const Pointf& pos) const;
};

// ============================================================================
// 四、功法脚本（真气行为定义）
// ============================================================================

class QiFlowScript
{
public:
    virtual ~QiFlowScript() = default;

    // 初始化真气流
    virtual void initFlow(std::vector<QiParticle>& particles,
                         const BattleSceneAct::AttackEffect& ae,
                         Role* source) = 0;

    // 每帧更新真气流
    virtual void updateFlow(std::vector<QiParticle>& particles,
                           QiField& field,
                           float delta_time) = 0;

    // 获取脚本名称
    virtual std::string getName() const = 0;
};

// ============================================================================
// 五、内置功法脚本
// ============================================================================

// 1. 降龙十八掌：瞬间压缩 → 高速直线推送 → 爆发
class DragonPalmScript : public QiFlowScript
{
public:
    void initFlow(std::vector<QiParticle>& particles,
                 const BattleSceneAct::AttackEffect& ae,
                 Role* source) override;

    void updateFlow(std::vector<QiParticle>& particles,
                   QiField& field,
                   float delta_time) override;

    std::string getName() const override { return "降龙十八掌"; }

private:
    float compression_phase_ = 0.0f;  // 压缩阶段：0=压缩中，1=爆发
};

// 2. 护体真气：全身密度场拉满，流动向外扩散形成屏障
class ShieldQiScript : public QiFlowScript
{
public:
    void initFlow(std::vector<QiParticle>& particles,
                 const BattleSceneAct::AttackEffect& ae,
                 Role* source) override;

    void updateFlow(std::vector<QiParticle>& particles,
                   QiField& field,
                   float delta_time) override;

    std::string getName() const override { return "护体真气"; }

private:
    float shield_radius_ = 50.0f;
};

// 3. 疗伤真气：低扰动、高纯度、慢速流入他人经脉
class HealingQiScript : public QiFlowScript
{
public:
    void initFlow(std::vector<QiParticle>& particles,
                 const BattleSceneAct::AttackEffect& ae,
                 Role* source) override;

    void updateFlow(std::vector<QiParticle>& particles,
                   QiField& field,
                   float delta_time) override;

    std::string getName() const override { return "疗伤真气"; }
};

// 4. 剑气：极低扰动、笔直锐利、高速穿透
class SwordQiScript : public QiFlowScript
{
public:
    void initFlow(std::vector<QiParticle>& particles,
                 const BattleSceneAct::AttackEffect& ae,
                 Role* source) override;

    void updateFlow(std::vector<QiParticle>& particles,
                   QiField& field,
                   float delta_time) override;

    std::string getName() const override { return "剑气"; }
};

// 5. 小周天：真气沿任督二脉循环一圈
class SmallHeavenScript : public QiFlowScript
{
public:
    void initFlow(std::vector<QiParticle>& particles,
                 const BattleSceneAct::AttackEffect& ae,
                 Role* source) override;

    void updateFlow(std::vector<QiParticle>& particles,
                   QiField& field,
                   float delta_time) override;

    std::string getName() const override { return "小周天"; }

private:
    float circulation_progress_ = 0.0f;  // 循环进度：0→1
};

// ============================================================================
// 六、真气系统主控
// ============================================================================

class QiSystem
{
public:
    QiSystem();
    ~QiSystem() = default;

    // 初始化功法脚本
    void registerScript(const std::string& name, std::shared_ptr<QiFlowScript> script);

    // 为攻击特效创建真气流
    void createFlow(const BattleSceneAct::AttackEffect& ae, const std::string& script_name = "");

    // 每帧更新
    void update(float delta_time);

    // 渲染
    void render(SDL_Renderer* renderer, const Pointf& camera_offset);

    // 清空
    void clear();

private:
    // 根据武功类型自动选择脚本
    std::string inferScript(const Magic* magic) const;

    // 渲染粒子
    void renderParticle(SDL_Renderer* renderer, const QiParticle& particle,
                       const Pointf& camera_offset);

    // 获取属性颜色
    void getAttributeColor(QiAttribute attr, float temperature,
                          Uint8& r, Uint8& g, Uint8& b) const;

    // Perlin 噪声（湍流）
    float perlinNoise(float x, float y, uint32_t seed) const;

    // 平滑插值
    float smoothstep(float t) const;

private:
    std::vector<QiParticle> particles_;
    std::vector<QiField> fields_;

    std::map<std::string, std::shared_ptr<QiFlowScript>> scripts_;
    std::map<const BattleSceneAct::AttackEffect*, std::shared_ptr<QiFlowScript>> active_flows_;

    uint32_t global_seed_ = 12345;

    static constexpr size_t MAX_PARTICLES = 3000;
    static constexpr size_t MAX_FIELDS = 50;
};

// ============================================================================
// 七、真气物理定律
// ============================================================================

namespace QiLaws
{
    // 密度衰减：density(d) = density₀ × e^(-d/λ)
    inline float densityDecay(float initial_density, float distance, float decay_length = 50.0f)
    {
        return initial_density * exp(-distance / decay_length);
    }

    // 压缩倍率：compression → density multiplier
    inline float compressionMultiplier(QiCompression comp)
    {
        switch (comp)
        {
            case QiCompression::Dispersed: return 0.3f;
            case QiCompression::Normal: return 1.0f;
            case QiCompression::Compressed: return 3.0f;
            case QiCompression::Explosive: return 10.0f;
            default: return 1.0f;
        }
    }

    // 扰动衰减：速度越快 → 扰动越小
    inline float turbulenceScale(float speed)
    {
        return 1.0f / (1.0f + speed * 0.05f);
    }

    // 属性克制倍率（五行相克）
    inline float attributeCounter(QiAttribute attacker, QiAttribute defender)
    {
        // 金克木，木克土，土克水，水克火，火克金
        if (attacker == QiAttribute::Metal && defender == QiAttribute::Wood) return 1.5f;
        if (attacker == QiAttribute::Wood && defender == QiAttribute::Earth) return 1.5f;
        if (attacker == QiAttribute::Earth && defender == QiAttribute::Water) return 1.5f;
        if (attacker == QiAttribute::Water && defender == QiAttribute::Fire) return 1.5f;
        if (attacker == QiAttribute::Fire && defender == QiAttribute::Metal) return 1.5f;

        // 阴阳相克
        if (attacker == QiAttribute::Yang && defender == QiAttribute::Yin) return 1.3f;
        if (attacker == QiAttribute::Yin && defender == QiAttribute::Yang) return 1.3f;

        return 1.0f;
    }
}

} // namespace QiPhysics
