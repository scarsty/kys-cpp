#include "QiEnergySystem.h"
#include "Head.h"
#include <cmath>
#include <algorithm>

namespace QiPhysics
{

// ============================================================================
// QiField 实现
// ============================================================================

float QiField::sampleDensity(const Pointf& pos) const
{
    float distance = (pos - center).norm();
    if (distance > radius) return 0.0f;

    // 高斯分布密度场
    float normalized_dist = distance / radius;
    float density_at_point = density * exp(-normalized_dist * normalized_dist * 2.0f);

    // 应用压缩倍率
    density_at_point *= QiLaws::compressionMultiplier(compression);

    return density_at_point;
}

Pointf QiField::sampleVelocity(const Pointf& pos) const
{
    float distance = (pos - center).norm();
    if (distance > radius) return {0.0f, 0.0f};

    // 径向流速（从中心向外，或向内）
    Pointf radial_dir = pos - center;
    if (radial_dir.norm() > 0.1f)
    {
        radial_dir.normTo(1.0f);
    }

    // 基础流速（根据距离衰减）
    float speed_factor = 1.0f - distance / radius;
    Pointf velocity = flow_direction;
    velocity.normTo(flow_speed * speed_factor);

    return velocity;
}

// ============================================================================
// 1. 降龙十八掌：压缩 → 爆发
// ============================================================================

void DragonPalmScript::initFlow(std::vector<QiParticle>& particles,
                                const BattleSceneAct::AttackEffect& ae,
                                Role* source)
{
    // 第一阶段：压缩（粒子向发射点聚集）
    compression_phase_ = 0.0f;

    Pointf launch_pos = source ? source->Pos : ae.Pos;

    // 生成密集粒子群（在发射点附近）
    const int PARTICLE_COUNT = 100;
    for (int i = 0; i < PARTICLE_COUNT; ++i)
    {
        QiParticle p;
        p.pos = launch_pos;
        p.pos.x += (rand() / (float)RAND_MAX - 0.5f) * 10.0f;  // 初始聚集在10px范围内
        p.pos.y += (rand() / (float)RAND_MAX - 0.5f) * 10.0f;

        p.velocity = {0.0f, 0.0f};  // 初始静止
        p.acceleration = {0.0f, 0.0f};

        p.density = 2.0f;  // 高密度
        p.temperature = 1.5f;  // 高温（能量集中）
        p.turbulence = 0.05f;  // 极低扰动

        p.lifetime = 1.0f;
        p.max_lifetime = 1.0f;

        p.attribute = QiAttribute::Yang;  // 阳刚属性
        p.state = QiState::External;

        p.source = source;
        p.target = nullptr;
        p.seed = i;

        particles.push_back(p);
    }
}

void DragonPalmScript::updateFlow(std::vector<QiParticle>& particles,
                                  QiField& field,
                                  float delta_time)
{
    // 压缩阶段进度：0→1
    compression_phase_ += delta_time * 5.0f;  // 0.2秒内完成压缩

    if (compression_phase_ < 1.0f)
    {
        // === 阶段1：向中心压缩 ===
        field.compression = QiCompression::Compressed;

        for (auto& p : particles)
        {
            // 向场中心施加吸引力
            Pointf to_center = field.center - p.pos;
            float distance = to_center.norm();
            if (distance > 0.1f)
            {
                to_center.normTo(1.0f);
                p.acceleration = to_center;
                p.acceleration.x *= 200.0f;  // 强吸引力
                p.acceleration.y *= 200.0f;
            }

            // 密度增加
            p.density += delta_time * 2.0f;
            p.temperature += delta_time * 3.0f;
        }
    }
    else
    {
        // === 阶段2：爆发释放 ===
        field.compression = QiCompression::Explosive;

        for (auto& p : particles)
        {
            // 沿流动方向高速推送
            if (p.velocity.norm() < 1.0f)  // 初次爆发
            {
                p.velocity = field.flow_direction;
                p.velocity.normTo(field.flow_speed * 3.0f);  // 3倍速
            }

            // 密度衰减
            p.density *= (1.0f - delta_time * 0.5f);
            p.temperature *= (1.0f - delta_time * 0.8f);

            // 极低扰动（笔直推进）
            p.turbulence = 0.02f;
        }
    }
}

// ============================================================================
// 2. 护体真气：环绕屏障
// ============================================================================

void ShieldQiScript::initFlow(std::vector<QiParticle>& particles,
                              const BattleSceneAct::AttackEffect& ae,
                              Role* source)
{
    if (!source) return;

    Pointf center = source->Pos;

    // 生成环绕粒子（圆形分布）
    const int PARTICLE_COUNT = 80;
    for (int i = 0; i < PARTICLE_COUNT; ++i)
    {
        QiParticle p;

        // 圆形分布
        float angle = (float)i / PARTICLE_COUNT * 2.0f * M_PI;
        float radius = shield_radius_;

        p.pos.x = center.x + radius * cos(angle);
        p.pos.y = center.y + radius * sin(angle);

        // 切向速度（环绕运动）
        p.velocity.x = -sin(angle) * 30.0f;
        p.velocity.y = cos(angle) * 30.0f;

        p.acceleration = {0.0f, 0.0f};

        p.density = 1.5f;
        p.temperature = 0.8f;
        p.turbulence = 0.1f;  // 低扰动

        p.lifetime = 999.0f;  // 持续存在
        p.max_lifetime = 999.0f;

        p.attribute = QiAttribute::Yang;
        p.state = QiState::Shield;

        p.source = source;
        p.target = nullptr;
        p.seed = i;

        particles.push_back(p);
    }
}

void ShieldQiScript::updateFlow(std::vector<QiParticle>& particles,
                                QiField& field,
                                float delta_time)
{
    field.compression = QiCompression::Normal;

    for (auto& p : particles)
    {
        if (!p.source) continue;

        // 保持在护体半径上
        Pointf to_source = p.pos - p.source->Pos;
        float current_radius = to_source.norm();

        if (current_radius > 0.1f)
        {
            to_source.normTo(1.0f);

            // 径向力：维持在 shield_radius_
            float radius_error = current_radius - shield_radius_;
            Pointf radial_force = to_source;
            radial_force.x *= -radius_error * 5.0f;  // 弹簧力
            radial_force.y *= -radius_error * 5.0f;

            p.acceleration = radial_force;

            // 切向力：环绕运动
            Pointf tangent = {-to_source.y, to_source.x};
            tangent.normTo(50.0f);
            p.velocity = tangent;
        }

        // 密度脉动（呼吸效果）
        p.density = 1.5f + 0.5f * sin(p.seed + field.flow_speed * 0.1f);
    }
}

// ============================================================================
// 3. 疗伤真气：低扰动、慢速传导
// ============================================================================

void HealingQiScript::initFlow(std::vector<QiParticle>& particles,
                               const BattleSceneAct::AttackEffect& ae,
                               Role* source)
{
    if (!source) return;

    Pointf start = source->Pos;

    // 生成柔和粒子流
    const int PARTICLE_COUNT = 50;
    for (int i = 0; i < PARTICLE_COUNT; ++i)
    {
        QiParticle p;
        p.pos = start;
        p.pos.x += (rand() / (float)RAND_MAX - 0.5f) * 5.0f;
        p.pos.y += (rand() / (float)RAND_MAX - 0.5f) * 5.0f;

        // 低速流动
        p.velocity = ae.Velocity;
        p.velocity.normTo(20.0f);  // 慢速

        p.acceleration = {0.0f, 0.0f};

        p.density = 0.8f;  // 低密度（柔和）
        p.temperature = 0.5f;  // 低温（温和）
        p.turbulence = 0.02f;  // 极低扰动（平滑）

        p.lifetime = 2.0f;
        p.max_lifetime = 2.0f;

        p.attribute = QiAttribute::Wood;  // 木属性（生机）
        p.state = QiState::Healing;

        p.source = source;
        p.target = nullptr;  // TODO: 设置为目标角色
        p.seed = i;

        particles.push_back(p);
    }
}

void HealingQiScript::updateFlow(std::vector<QiParticle>& particles,
                                 QiField& field,
                                 float delta_time)
{
    field.compression = QiCompression::Dispersed;

    for (auto& p : particles)
    {
        // 保持低速、平滑流动
        if (p.velocity.norm() > 20.0f)
        {
            p.velocity.normTo(20.0f);
        }

        // 密度缓慢衰减
        p.density *= (1.0f - delta_time * 0.1f);

        // 极低扰动（几乎笔直）
        p.turbulence = 0.02f;
    }
}

// ============================================================================
// 4. 剑气：极低扰动、笔直锐利
// ============================================================================

void SwordQiScript::initFlow(std::vector<QiParticle>& particles,
                             const BattleSceneAct::AttackEffect& ae,
                             Role* source)
{
    Pointf start = source ? source->Pos : ae.Pos;
    Pointf direction = ae.Velocity;
    direction.normTo(1.0f);

    // 生成细长粒子束
    const int PARTICLE_COUNT = 60;
    for (int i = 0; i < PARTICLE_COUNT; ++i)
    {
        QiParticle p;

        // 沿方向排列
        float offset = i * 1.0f;
        p.pos.x = start.x + direction.x * offset;
        p.pos.y = start.y + direction.y * offset;

        // 高速直线运动
        p.velocity = direction;
        p.velocity.normTo(ae.Velocity.norm() * 1.5f);  // 1.5倍速

        p.acceleration = {0.0f, 0.0f};

        p.density = 2.0f;
        p.temperature = 1.0f;
        p.turbulence = 0.01f;  // 极低扰动（笔直）

        p.lifetime = 0.8f;
        p.max_lifetime = 0.8f;

        p.attribute = QiAttribute::Metal;  // 金属性（锐利）
        p.state = QiState::External;

        p.source = source;
        p.target = nullptr;
        p.seed = i;

        particles.push_back(p);
    }
}

void SwordQiScript::updateFlow(std::vector<QiParticle>& particles,
                               QiField& field,
                               float delta_time)
{
    field.compression = QiCompression::Compressed;

    for (auto& p : particles)
    {
        // 保持笔直（无湍流）
        p.turbulence = 0.01f;

        // 速度衰减极慢（保持锐利）
        p.velocity.x *= (1.0f - delta_time * 0.05f);
        p.velocity.y *= (1.0f - delta_time * 0.05f);

        // 密度缓慢衰减
        p.density *= (1.0f - delta_time * 0.3f);
    }
}

// ============================================================================
// 5. 小周天：循环流动
// ============================================================================

void SmallHeavenScript::initFlow(std::vector<QiParticle>& particles,
                                 const BattleSceneAct::AttackEffect& ae,
                                 Role* source)
{
    if (!source) return;

    Pointf center = source->Pos;

    // 生成循环粒子（椭圆路径）
    const int PARTICLE_COUNT = 40;
    for (int i = 0; i < PARTICLE_COUNT; ++i)
    {
        QiParticle p;

        // 初始分布在椭圆路径上
        float angle = (float)i / PARTICLE_COUNT * 2.0f * M_PI;
        float radius_x = 30.0f;  // 椭圆长轴
        float radius_y = 20.0f;  // 椭圆短轴

        p.pos.x = center.x + radius_x * cos(angle);
        p.pos.y = center.y + radius_y * sin(angle);

        // 切向速度（沿椭圆运动）
        p.velocity.x = -radius_x * sin(angle) * 2.0f;
        p.velocity.y = radius_y * cos(angle) * 2.0f;

        p.acceleration = {0.0f, 0.0f};

        p.density = 1.0f;
        p.temperature = 0.8f;
        p.turbulence = 0.05f;

        p.lifetime = 999.0f;  // 持续循环
        p.max_lifetime = 999.0f;

        p.attribute = QiAttribute::Yang;
        p.state = QiState::Internal;

        p.source = source;
        p.target = nullptr;
        p.seed = i;

        particles.push_back(p);
    }
}

void SmallHeavenScript::updateFlow(std::vector<QiParticle>& particles,
                                   QiField& field,
                                   float delta_time)
{
    field.compression = QiCompression::Normal;

    circulation_progress_ += delta_time * 0.5f;  // 2秒一个周期
    if (circulation_progress_ > 1.0f) circulation_progress_ -= 1.0f;

    for (auto& p : particles)
    {
        if (!p.source) continue;

        // 计算椭圆路径位置
        float angle = circulation_progress_ * 2.0f * M_PI + p.seed * 0.1f;
        float radius_x = 30.0f;
        float radius_y = 20.0f;

        Pointf target_pos;
        target_pos.x = p.source->Pos.x + radius_x * cos(angle);
        target_pos.y = p.source->Pos.y + radius_y * sin(angle);

        // 向目标位置施加拉力
        Pointf to_target = target_pos - p.pos;
        p.acceleration = to_target;
        p.acceleration.x *= 10.0f;
        p.acceleration.y *= 10.0f;

        // 密度周期变化（循环时提纯）
        p.density = 1.0f + 0.3f * sin(circulation_progress_ * 2.0f * M_PI);
        p.temperature = 0.8f + 0.2f * sin(circulation_progress_ * 2.0f * M_PI);
    }
}

// ============================================================================
// QiSystem 实现
// ============================================================================

QiSystem::QiSystem()
{
    particles_.reserve(MAX_PARTICLES);
    fields_.reserve(MAX_FIELDS);

    // 注册默认脚本
    registerScript("降龙十八掌", std::make_shared<DragonPalmScript>());
    registerScript("护体真气", std::make_shared<ShieldQiScript>());
    registerScript("疗伤真气", std::make_shared<HealingQiScript>());
    registerScript("剑气", std::make_shared<SwordQiScript>());
    registerScript("小周天", std::make_shared<SmallHeavenScript>());
}

void QiSystem::registerScript(const std::string& name, std::shared_ptr<QiFlowScript> script)
{
    scripts_[name] = script;
}

void QiSystem::createFlow(const BattleSceneAct::AttackEffect& ae, const std::string& script_name)
{
    std::string name = script_name.empty() ? inferScript(ae.UsingMagic) : script_name;

    auto it = scripts_.find(name);
    if (it == scripts_.end())
    {
        LOG("[QiSystem] Script not found: {}\n", name);
        return;
    }

    auto script = it->second;

    // 创建真气场
    QiField field;
    field.center = ae.Pos;
    field.radius = 100.0f;
    field.density = 1.0f;
    field.flow_speed = ae.Velocity.norm();
    field.flow_direction = ae.Velocity;
    if (field.flow_direction.norm() > 0.1f)
    {
        field.flow_direction.normTo(1.0f);
    }
    field.attribute = QiAttribute::Yang;  // TODO: 从武功推断
    field.compression = QiCompression::Normal;

    fields_.push_back(field);

    // 初始化粒子
    script->initFlow(particles_, ae, ae.Attacker);
    active_flows_[&ae] = script;

    LOG("[QiSystem] Created flow: {}, particles={}\n", name, particles_.size());
}

void QiSystem::update(float delta_time)
{
    // 更新所有激活的流
    for (auto& [ae, script] : active_flows_)
    {
        if (!fields_.empty())
        {
            script->updateFlow(particles_, fields_[0], delta_time);
        }
    }

    // 更新粒子物理
    for (auto it = particles_.begin(); it != particles_.end(); )
    {
        auto& p = *it;

        // 更新寿命
        p.lifetime -= delta_time;
        if (p.lifetime <= 0.0f && p.state != QiState::Shield && p.state != QiState::Internal)
        {
            it = particles_.erase(it);
            continue;
        }

        // 更新速度和位置
        p.velocity.x += p.acceleration.x * delta_time;
        p.velocity.y += p.acceleration.y * delta_time;

        p.pos.x += p.velocity.x * delta_time;
        p.pos.y += p.velocity.y * delta_time;

        // 应用湍流（Perlin噪声）
        if (p.turbulence > 0.01f)
        {
            float turb_x = perlinNoise(p.pos.x * 0.05f, global_seed_, p.seed) * p.turbulence * 10.0f;
            float turb_y = perlinNoise(p.pos.y * 0.05f, global_seed_, p.seed + 1000) * p.turbulence * 10.0f;

            p.pos.x += turb_x * delta_time;
            p.pos.y += turb_y * delta_time;
        }

        ++it;
    }

    global_seed_ += 1;
}

void QiSystem::render(SDL_Renderer* renderer, const Pointf& camera_offset)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (const auto& p : particles_)
    {
        renderParticle(renderer, p, camera_offset);
    }
}

void QiSystem::clear()
{
    particles_.clear();
    fields_.clear();
    active_flows_.clear();
}

std::string QiSystem::inferScript(const Magic* magic) const
{
    if (!magic) return "剑气";

    // 只根据武功类型（MagicType）推断
    switch (magic->MagicType)
    {
        case 1: return "降龙十八掌";  // 拳法 → 爆发型（压缩-爆发）
        case 2: return "剑气";        // 剑法 → 锐利型（笔直）
        case 3: return "剑气";        // 刀法 → 同剑气（笔直）
        case 4: return "剑气";        // 特殊 → 默认剑气
        default: return "剑气";
    }
}

void QiSystem::renderParticle(SDL_Renderer* renderer, const QiParticle& particle,
                              const Pointf& camera_offset)
{
    // 计算屏幕位置
    int screen_x = (int)(particle.pos.x - camera_offset.x);
    int screen_y = (int)(particle.pos.y / 2.0f - camera_offset.y);

    // 计算颜色（基于属性和温度）
    Uint8 r, g, b;
    getAttributeColor(particle.attribute, particle.temperature, r, g, b);

    // 计算透明度（基于密度和寿命）
    float life_ratio = particle.lifetime / particle.max_lifetime;
    if (life_ratio > 1.0f) life_ratio = 1.0f;

    Uint8 alpha = (Uint8)(particle.density * life_ratio * 150.0f);
    if (alpha < 5) return;

    SDL_SetRenderDrawColor(renderer, r, g, b, alpha);

    // 渲染粒子（简单圆形）
    int radius = std::max(1, (int)(particle.density * 2.0f));
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            if (dx * dx + dy * dy <= radius * radius)
            {
                SDL_RenderPoint(renderer, screen_x + dx, screen_y + dy);
            }
        }
    }
}

void QiSystem::getAttributeColor(QiAttribute attr, float temperature,
                                 Uint8& r, Uint8& g, Uint8& b) const
{
    // 基础颜色
    switch (attr)
    {
        case QiAttribute::Metal:
            r = 220; g = 220; b = 240;  // 白金色
            break;
        case QiAttribute::Wood:
            r = 120; g = 220; b = 120;  // 绿色
            break;
        case QiAttribute::Water:
            r = 100; g = 180; b = 255;  // 蓝色
            break;
        case QiAttribute::Fire:
            r = 255; g = 120; b = 80;   // 红色
            break;
        case QiAttribute::Earth:
            r = 200; g = 160; b = 100;  // 黄色
            break;
        case QiAttribute::Yang:
            r = 255; g = 220; b = 100;  // 金黄色
            break;
        case QiAttribute::Yin:
            r = 160; g = 100; b = 200;  // 紫黑色
            break;
        default:
            r = 220; g = 220; b = 220;  // 白色
            break;
    }

    // 温度影响（高温偏红，低温偏蓝）
    if (temperature > 1.0f)
    {
        r = std::min(255, (int)(r * temperature));
        g = (Uint8)(g / temperature);
        b = (Uint8)(b / temperature);
    }
    else if (temperature < 0.8f)
    {
        r = (Uint8)(r * temperature);
        g = (Uint8)(g * temperature);
        b = std::min(255, (int)(b / temperature));
    }
}

float QiSystem::perlinNoise(float x, float y, uint32_t seed) const
{
    int xi = (int)x;
    int yi = (int)y;
    float xf = x - xi;
    float yf = y - yi;

    auto hash = [](int x, int y, uint32_t s) -> float
    {
        int n = x + y * 57 + s * 131;
        n = (n << 13) ^ n;
        return ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f - 1.0f;
    };

    float g00 = hash(xi, yi, seed);
    float g10 = hash(xi + 1, yi, seed);
    float g01 = hash(xi, yi + 1, seed);
    float g11 = hash(xi + 1, yi + 1, seed);

    float tx = smoothstep(xf);
    float ty = smoothstep(yf);

    float i0 = g00 * (1.0f - tx) + g10 * tx;
    float i1 = g01 * (1.0f - tx) + g11 * tx;

    return i0 * (1.0f - ty) + i1 * ty;
}

float QiSystem::smoothstep(float t) const
{
    return t * t * (3.0f - 2.0f * t);
}

} // namespace QiPhysics
