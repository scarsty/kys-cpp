#include "QiParticleSystem.h"
#include "Head.h"
#include <cmath>
#include <algorithm>

namespace QiPhysics
{

// ============================================================================
// QiEffectSystem 实现
// ============================================================================

QiEffectSystem::QiEffectSystem()
    : global_seed_(12345)
{
    particles_.reserve(MAX_PARTICLES);
}

void QiEffectSystem::update(float delta_time)
{
    // 更新所有粒子
    for (auto it = particles_.begin(); it != particles_.end(); )
    {
        auto& p = *it;

        // 更新寿命
        p.lifetime -= delta_time;
        if (p.lifetime <= 0.0f)
        {
            it = particles_.erase(it);
            continue;
        }

        // 更新位置
        p.pos.x += p.velocity.x * delta_time;
        p.pos.y += p.velocity.y * delta_time;

        // 应用湍流（Perlin噪声）
        float turbulence_x = perlinNoise(p.pos.x * 0.05f + global_seed_) * params_.turbulence_strength;
        float turbulence_y = perlinNoise(p.pos.y * 0.05f + global_seed_) * params_.turbulence_strength;
        p.pos.x += turbulence_x * delta_time;
        p.pos.y += turbulence_y * delta_time;

        // 应用重力
        p.velocity.y += params_.gravity_y * delta_time;

        // 旋转更新（剑气、刀气）
        if (p.shape == QiShape::SwordQi || p.shape == QiShape::BladeQi)
        {
            p.rotation += delta_time * 0.5f;  // 缓慢旋转
        }

        ++it;
    }

    global_seed_ += 1;  // 更新全局种子
}

void QiEffectSystem::render(SDL_Renderer* renderer, const Pointf& camera_offset)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (const auto& p : particles_)
    {
        renderParticle(renderer, p, camera_offset);
    }
}

void QiEffectSystem::createQiFlow(const BattleSceneAct::AttackEffect& ae)
{
    if (!ae.UsingMagic) return;

    // 推断形状和属性
    QiShape shape = inferShapeFromMagic(ae.UsingMagic);
    QiAttribute attr = inferAttributeFromMagic(ae.UsingMagic);

    // 根据形状生成不同数量的粒子
    int particle_count = params_.particle_spawn_rate;
    if (shape == QiShape::FistAura) particle_count *= 2;       // 拳劲更密集
    if (shape == QiShape::PalmWind) particle_count = (int)(particle_count * 1.5f);  // 掌风略密集

    spawnParticles(ae, shape, attr, particle_count);

    // 属性特效
    if (attr == QiAttribute::Fire && rand() / (float)RAND_MAX < params_.fire_splash_chance)
    {
        createFireSplash(ae.Pos, ae.Velocity, ae.Frame);
    }
    if (attr == QiAttribute::Ice && rand() / (float)RAND_MAX < params_.ice_crystal_chance)
    {
        createIceCrystal(ae.Pos, ae.Velocity, ae.Frame);
    }
}

void QiEffectSystem::clear()
{
    particles_.clear();
}

// ============================================================================
// 私有方法：推断逻辑
// ============================================================================

QiShape QiEffectSystem::inferShapeFromMagic(const Magic* magic) const
{
    if (!magic) return QiShape::Ribbon;

    switch (magic->MagicType)
    {
        case 1:  // 拳法
            return QiShape::FistAura;
        case 2:  // 剑法
            return QiShape::SwordQi;
        case 3:  // 刀法
            return QiShape::BladeQi;
        case 4:  // 特殊
            return QiShape::SpecialOrb;
        default:
            return QiShape::PalmWind;  // 默认掌风
    }
}

QiAttribute QiEffectSystem::inferAttributeFromMagic(const Magic* magic) const
{
    if (!magic) return QiAttribute::None;

    // 根据武功名称推断属性（可扩展为配置文件）
    std::string name = magic->Name;

    if (name.find("火") != std::string::npos || name.find("焰") != std::string::npos)
        return QiAttribute::Fire;
    if (name.find("冰") != std::string::npos || name.find("寒") != std::string::npos)
        return QiAttribute::Ice;
    if (name.find("雷") != std::string::npos || name.find("电") != std::string::npos)
        return QiAttribute::Thunder;
    if (name.find("毒") != std::string::npos)
        return QiAttribute::Poison;
    if (name.find("阴") != std::string::npos || name.find("幽") != std::string::npos)
        return QiAttribute::Yin;

    // 根据武功类型默认属性
    switch (magic->MagicType)
    {
        case 1: return QiAttribute::Yang;    // 拳法 → 阳刚
        case 2: return QiAttribute::Wind;    // 剑法 → 风
        case 3: return QiAttribute::Fire;    // 刀法 → 火
        case 4: return QiAttribute::Yin;     // 特殊 → 阴
        default: return QiAttribute::None;
    }
}

// ============================================================================
// 私有方法：粒子生成
// ============================================================================

void QiEffectSystem::spawnParticles(const BattleSceneAct::AttackEffect& ae,
                                    QiShape shape, QiAttribute attribute, int count)
{
    if (particles_.size() >= MAX_PARTICLES) return;

    for (int i = 0; i < count; ++i)
    {
        if (particles_.size() >= MAX_PARTICLES) break;

        QiParticle p;
        p.pos = ae.Pos;
        p.attribute = attribute;
        p.shape = shape;
        p.max_lifetime = params_.particle_lifetime;
        p.lifetime = p.max_lifetime;
        p.seed = global_seed_ + i;

        // 根据形状调整初始速度
        float speed_variance = params_.particle_speed_variance;
        float base_speed = ae.Velocity.norm();

        switch (shape)
        {
            case QiShape::SwordQi:
                // 剑气：沿速度方向，窄锥形
                p.velocity = ae.Velocity;
                p.velocity.x += (rand() / (float)RAND_MAX - 0.5f) * base_speed * 0.2f;
                p.velocity.y += (rand() / (float)RAND_MAX - 0.5f) * base_speed * 0.2f;
                p.size = 8.0f + rand() / (float)RAND_MAX * 4.0f;
                p.rotation = atan2(ae.Velocity.y, ae.Velocity.x);
                break;

            case QiShape::BladeQi:
                // 刀气：沿速度方向，宽锥形
                p.velocity = ae.Velocity;
                p.velocity.x += (rand() / (float)RAND_MAX - 0.5f) * base_speed * 0.5f;
                p.velocity.y += (rand() / (float)RAND_MAX - 0.5f) * base_speed * 0.5f;
                p.size = 12.0f + rand() / (float)RAND_MAX * 6.0f;
                p.rotation = atan2(ae.Velocity.y, ae.Velocity.x);
                break;

            case QiShape::FistAura:
                // 拳劲：360度扩散
                {
                    float angle = (rand() / (float)RAND_MAX) * 2.0f * M_PI;
                    float speed = base_speed * (0.5f + rand() / (float)RAND_MAX * 0.5f);
                    p.velocity.x = cos(angle) * speed;
                    p.velocity.y = sin(angle) * speed;
                    p.size = 6.0f + rand() / (float)RAND_MAX * 4.0f;
                    p.rotation = 0.0f;
                }
                break;

            case QiShape::PalmWind:
                // 掌风：扇形扩散
                {
                    float base_angle = atan2(ae.Velocity.y, ae.Velocity.x);
                    float spread = M_PI / 3.0f;  // 60度扇形
                    float angle = base_angle + (rand() / (float)RAND_MAX - 0.5f) * spread;
                    float speed = base_speed * (0.7f + rand() / (float)RAND_MAX * 0.3f);
                    p.velocity.x = cos(angle) * speed;
                    p.velocity.y = sin(angle) * speed;
                    p.size = 5.0f + rand() / (float)RAND_MAX * 3.0f;
                    p.rotation = angle;
                }
                break;

            default:  // Ribbon / SpecialOrb
                p.velocity = ae.Velocity;
                p.velocity.x += (rand() / (float)RAND_MAX - 0.5f) * base_speed * speed_variance;
                p.velocity.y += (rand() / (float)RAND_MAX - 0.5f) * base_speed * speed_variance;
                p.size = 4.0f + rand() / (float)RAND_MAX * 3.0f;
                p.rotation = 0.0f;
                break;
        }

        p.brightness = 1.0f;
        particles_.push_back(p);
    }
}

// ============================================================================
// 私有方法：真气衰减定律
// ============================================================================

float QiEffectSystem::calculateBrightness(const QiParticle& particle, float distance_from_source) const
{
    // 公式：亮度 = 初始亮度 × e^(-距离 / 衰减系数)
    float life_ratio = particle.lifetime / particle.max_lifetime;
    float distance_decay = exp(-distance_from_source / params_.brightness_decay_coeff);

    return life_ratio * distance_decay * particle.brightness;
}

float QiEffectSystem::calculateSize(const QiParticle& particle, float distance_from_source) const
{
    // 公式：大小 = 初始大小 × e^(-距离 / 衰减系数)
    float life_ratio = particle.lifetime / particle.max_lifetime;
    float distance_decay = exp(-distance_from_source / params_.size_decay_coeff);

    return particle.size * life_ratio * distance_decay;
}

// ============================================================================
// 私有方法：颜色映射
// ============================================================================

void QiEffectSystem::getAttributeColor(QiAttribute attr, Uint8& r, Uint8& g, Uint8& b) const
{
    switch (attr)
    {
        case QiAttribute::Yang:     // 阳刚（金黄）
            r = 255; g = 200; b = 80;
            break;
        case QiAttribute::Yin:      // 阴柔（紫黑）
            r = 140; g = 80; b = 180;
            break;
        case QiAttribute::Fire:     // 火（赤红）
            r = 255; g = 100; b = 60;
            break;
        case QiAttribute::Ice:      // 冰（冰蓝）
            r = 120; g = 200; b = 255;
            break;
        case QiAttribute::Thunder:  // 雷（紫电）
            r = 200; g = 150; b = 255;
            break;
        case QiAttribute::Wind:     // 风（青绿）
            r = 150; g = 255; b = 180;
            break;
        case QiAttribute::Poison:   // 毒（暗绿）
            r = 100; g = 180; b = 80;
            break;
        default:  // None（白色）
            r = 220; g = 220; b = 220;
            break;
    }
}

// ============================================================================
// 私有方法：渲染
// ============================================================================

void QiEffectSystem::renderParticle(SDL_Renderer* renderer, const QiParticle& particle,
                                    const Pointf& camera_offset)
{
    // 计算屏幕位置（2.5D视角）
    int screen_x = (int)(particle.pos.x - camera_offset.x);
    int screen_y = (int)(particle.pos.y / 2.0f - camera_offset.y);

    // 计算衰减
    float distance = particle.pos.norm();  // 简化：使用距原点距离
    float brightness = calculateBrightness(particle, distance);
    float size = calculateSize(particle, distance);

    if (brightness < 0.05f || size < 1.0f) return;

    // 获取颜色
    Uint8 r, g, b;
    getAttributeColor(particle.attribute, r, g, b);

    // 阴属性：降低透明度
    float alpha_mult = (particle.attribute == QiAttribute::Yin) ? params_.yin_transparency : 1.0f;
    Uint8 alpha = (Uint8)(brightness * 255.0f * alpha_mult);

    SDL_SetRenderDrawColor(renderer, r, g, b, alpha);

    Pointf screen_pos = { (float)screen_x, (float)screen_y };

    // 根据形状渲染
    switch (particle.shape)
    {
        case QiShape::SwordQi:
            renderSwordQi(renderer, particle, screen_pos, r, g, b, alpha);
            break;
        case QiShape::BladeQi:
            renderBladeQi(renderer, particle, screen_pos, r, g, b, alpha);
            break;
        case QiShape::FistAura:
            renderFistAura(renderer, particle, screen_pos, r, g, b, alpha);
            break;
        case QiShape::PalmWind:
            renderPalmWind(renderer, particle, screen_pos, r, g, b, alpha);
            break;
        default:  // Ribbon / SpecialOrb
            // 简单圆形粒子
            int radius = (int)(size / 2.0f);
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
            break;
    }
}

void QiEffectSystem::renderSwordQi(SDL_Renderer* renderer, const QiParticle& particle,
                                   const Pointf& screen_pos, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 剑气：细长锋利的气刃（长方形）
    int length = (int)(particle.size * 3.0f);
    int width = std::max(2, (int)(particle.size / 2.0f));

    SDL_SetRenderDrawColor(renderer, r, g, b, alpha);

    // 沿旋转方向绘制
    float cos_r = cos(particle.rotation);
    float sin_r = sin(particle.rotation);

    for (int i = 0; i < length; ++i)
    {
        for (int w = -width / 2; w <= width / 2; ++w)
        {
            int px = (int)(screen_pos.x + i * cos_r - w * sin_r);
            int py = (int)(screen_pos.y + i * sin_r + w * cos_r);
            SDL_RenderPoint(renderer, px, py);
        }
    }
}

void QiEffectSystem::renderBladeQi(SDL_Renderer* renderer, const QiParticle& particle,
                                   const Pointf& screen_pos, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 刀气：宽厚霸道的气刃（梯形）
    int length = (int)(particle.size * 2.5f);
    int width_base = (int)(particle.size);
    int width_tip = std::max(2, (int)(particle.size / 3.0f));

    SDL_SetRenderDrawColor(renderer, r, g, b, alpha);

    float cos_r = cos(particle.rotation);
    float sin_r = sin(particle.rotation);

    for (int i = 0; i < length; ++i)
    {
        float ratio = (float)i / length;
        int width = (int)(width_base * (1.0f - ratio) + width_tip * ratio);

        for (int w = -width / 2; w <= width / 2; ++w)
        {
            int px = (int)(screen_pos.x + i * cos_r - w * sin_r);
            int py = (int)(screen_pos.y + i * sin_r + w * cos_r);
            SDL_RenderPoint(renderer, px, py);
        }
    }
}

void QiEffectSystem::renderFistAura(SDL_Renderer* renderer, const QiParticle& particle,
                                    const Pointf& screen_pos, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 拳劲：圆形冲击波（空心圆）
    int radius = (int)(particle.size);
    int thickness = std::max(1, radius / 3);

    SDL_SetRenderDrawColor(renderer, r, g, b, alpha);

    // Bresenham 圆算法（空心）
    for (int r_offset = 0; r_offset < thickness; ++r_offset)
    {
        int current_radius = radius - r_offset;
        int x = 0, y = current_radius;
        int d = 3 - 2 * current_radius;

        auto drawCirclePoints = [&](int cx, int cy)
        {
            SDL_RenderPoint(renderer, (int)screen_pos.x + cx, (int)screen_pos.y + cy);
            SDL_RenderPoint(renderer, (int)screen_pos.x - cx, (int)screen_pos.y + cy);
            SDL_RenderPoint(renderer, (int)screen_pos.x + cx, (int)screen_pos.y - cy);
            SDL_RenderPoint(renderer, (int)screen_pos.x - cx, (int)screen_pos.y - cy);
            SDL_RenderPoint(renderer, (int)screen_pos.x + cy, (int)screen_pos.y + cx);
            SDL_RenderPoint(renderer, (int)screen_pos.x - cy, (int)screen_pos.y + cx);
            SDL_RenderPoint(renderer, (int)screen_pos.x + cy, (int)screen_pos.y - cx);
            SDL_RenderPoint(renderer, (int)screen_pos.x - cy, (int)screen_pos.y - cx);
        };

        while (y >= x)
        {
            drawCirclePoints(x, y);
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

void QiEffectSystem::renderPalmWind(SDL_Renderer* renderer, const QiParticle& particle,
                                    const Pointf& screen_pos, Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 掌风：扇形气浪
    int radius = (int)(particle.size * 1.5f);
    float angle_span = M_PI / 4.0f;  // 45度扇形

    SDL_SetRenderDrawColor(renderer, r, g, b, alpha);

    // 绘制扇形边缘线条
    for (float angle = -angle_span / 2.0f; angle <= angle_span / 2.0f; angle += 0.1f)
    {
        float total_angle = particle.rotation + angle;
        for (int r_step = 0; r_step < radius; r_step += 2)
        {
            int px = (int)(screen_pos.x + r_step * cos(total_angle));
            int py = (int)(screen_pos.y + r_step * sin(total_angle) * 0.5f);  // 2.5D压缩
            SDL_RenderPoint(renderer, px, py);
        }
    }
}

// ============================================================================
// 属性特效
// ============================================================================

void QiEffectSystem::createFireSplash(const Pointf& pos, const Pointf& velocity, uint32_t seed)
{
    // 火焰溅射：生成小火花粒子
    for (int i = 0; i < 5; ++i)
    {
        if (particles_.size() >= MAX_PARTICLES) break;

        QiParticle spark;
        spark.pos = pos;
        spark.attribute = QiAttribute::Fire;
        spark.shape = QiShape::Ribbon;
        spark.max_lifetime = 0.2f;
        spark.lifetime = spark.max_lifetime;
        spark.seed = seed + i;

        float angle = (rand() / (float)RAND_MAX) * 2.0f * M_PI;
        float speed = 50.0f + rand() / (float)RAND_MAX * 50.0f;
        spark.velocity.x = cos(angle) * speed;
        spark.velocity.y = sin(angle) * speed;
        spark.size = 2.0f + rand() / (float)RAND_MAX * 2.0f;
        spark.brightness = 1.5f;
        spark.rotation = 0.0f;

        particles_.push_back(spark);
    }
}

void QiEffectSystem::createIceCrystal(const Pointf& pos, const Pointf& velocity, uint32_t seed)
{
    // 冰晶粒子：缓慢漂浮
    if (particles_.size() >= MAX_PARTICLES) return;

    QiParticle crystal;
    crystal.pos = pos;
    crystal.attribute = QiAttribute::Ice;
    crystal.shape = QiShape::Ribbon;
    crystal.max_lifetime = 0.8f;
    crystal.lifetime = crystal.max_lifetime;
    crystal.seed = seed;

    crystal.velocity = velocity;
    crystal.velocity.x *= 0.3f;  // 减速
    crystal.velocity.y *= 0.3f;
    crystal.size = 6.0f;
    crystal.brightness = 1.2f;
    crystal.rotation = 0.0f;

    particles_.push_back(crystal);
}

// ============================================================================
// 辅助函数
// ============================================================================

float QiEffectSystem::perlinNoise(float x) const
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

float QiEffectSystem::smoothstep(float t) const
{
    return t * t * (3.0f - 2.0f * t);
}

// ============================================================================
// QiRibbonRenderer 实现
// ============================================================================

void QiRibbonRenderer::render(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                              int current_frame)
{
    if (ae.Frame >= ae.TotalFrame || !ae.UsingMagic) return;
    if (!ae.Defender.empty()) return;  // 命中后消失

    QiShape shape = inferShape(ae.UsingMagic);
    QiAttribute attr = inferAttribute(ae.UsingMagic);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    renderByShape(renderer, ae, shape, attr);
}

QiShape QiRibbonRenderer::inferShape(const Magic* magic) const
{
    if (!magic) return QiShape::Ribbon;

    switch (magic->MagicType)
    {
        case 1: return QiShape::FistAura;    // 拳法
        case 2: return QiShape::SwordQi;     // 剑法
        case 3: return QiShape::BladeQi;     // 刀法
        case 4: return QiShape::SpecialOrb;  // 特殊
        default: return QiShape::PalmWind;
    }
}

QiAttribute QiRibbonRenderer::inferAttribute(const Magic* magic) const
{
    if (!magic) return QiAttribute::None;

    std::string name = magic->Name;

    if (name.find("火") != std::string::npos || name.find("焰") != std::string::npos)
        return QiAttribute::Fire;
    if (name.find("冰") != std::string::npos || name.find("寒") != std::string::npos)
        return QiAttribute::Ice;
    if (name.find("雷") != std::string::npos || name.find("电") != std::string::npos)
        return QiAttribute::Thunder;
    if (name.find("毒") != std::string::npos)
        return QiAttribute::Poison;
    if (name.find("阴") != std::string::npos || name.find("幽") != std::string::npos)
        return QiAttribute::Yin;

    switch (magic->MagicType)
    {
        case 1: return QiAttribute::Yang;
        case 2: return QiAttribute::Wind;
        case 3: return QiAttribute::Fire;
        case 4: return QiAttribute::Yin;
        default: return QiAttribute::None;
    }
}

void QiRibbonRenderer::renderByShape(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                                     QiShape shape, QiAttribute attribute)
{
    // 计算基础透明度
    float max_frame = ae.TotalFrame * 1.25f;
    Uint8 base_alpha = (Uint8)(80 * (max_frame - ae.Frame) / max_frame);
    if (base_alpha <= 5) return;

    // 获取颜色
    bool is_friendly = (ae.Attacker && ae.Attacker->Team == 0);
    Uint8 r, g, b;
    getAttributeColor(attribute, is_friendly, r, g, b);

    // 根据形状调用不同的渲染函数
    switch (shape)
    {
        case QiShape::SwordQi:
            renderSwordQiTrail(renderer, ae, r, g, b, base_alpha);
            break;
        case QiShape::BladeQi:
            renderBladeQiTrail(renderer, ae, r, g, b, base_alpha);
            break;
        case QiShape::FistAura:
            renderFistAuraWave(renderer, ae, r, g, b, base_alpha);
            break;
        case QiShape::PalmWind:
            renderPalmWindFan(renderer, ae, r, g, b, base_alpha);
            break;
        default:
            renderMainRibbon(renderer, ae, r, g, b, base_alpha);
            break;
    }

    renderRibbonParticles(renderer, ae, r, g, b, base_alpha);
}

void QiRibbonRenderer::renderSwordQiTrail(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                                          Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 剑气：细长锋利的气刃拖尾
    if (!ae.Attacker) return;

    float dir_x = ae.Velocity.x;
    float dir_y = ae.Velocity.y;
    float magnitude = sqrt(dir_x * dir_x + dir_y * dir_y);
    if (magnitude < 0.1f)
    {
        dir_x = ae.Pos.x - ae.Attacker->Pos.x;
        dir_y = ae.Pos.y - ae.Attacker->Pos.y;
        magnitude = sqrt(dir_x * dir_x + dir_y * dir_y);
        if (magnitude < 0.1f) return;
    }
    dir_x /= magnitude;
    dir_y /= magnitude;

    const int TRAIL_LENGTH = 40;
    const float SEGMENT_SPACING = 1.2f;
    const float WIDTH_MAX = 16.0f;  // 剑气窄
    const float WIDTH_MIN = 2.0f;

    int head_x = ae.Pos.x;
    int head_y = ae.Pos.y / 2;

    for (int i = 0; i < TRAIL_LENGTH; ++i)
    {
        float ratio = (float)i / TRAIL_LENGTH;
        float offset = i * SEGMENT_SPACING;

        float x = head_x - dir_x * offset;
        float y = head_y - dir_y * offset * 0.5f;

        int width = (int)(WIDTH_MAX - (WIDTH_MAX - WIDTH_MIN) * ratio);
        if (width < 2) width = 2;

        float alpha_ratio = 1.0f - ratio * 0.8f;
        Uint8 segment_alpha = (Uint8)(alpha * alpha_ratio);
        if (segment_alpha < 10) continue;

        SDL_SetRenderDrawColor(renderer, r, g, b, segment_alpha);

        int render_x = (int)x;
        int render_y = (int)y;

        // 绘制细长的剑气
        for (int w = 0; w < width; ++w)
        {
            int px = render_x - width / 2 + w;
            SDL_RenderPoint(renderer, px, render_y);
        }
    }
}

void QiRibbonRenderer::renderBladeQiTrail(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                                          Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 刀气：宽厚霸道的气刃拖尾
    if (!ae.Attacker) return;

    float dir_x = ae.Velocity.x;
    float dir_y = ae.Velocity.y;
    float magnitude = sqrt(dir_x * dir_x + dir_y * dir_y);
    if (magnitude < 0.1f)
    {
        dir_x = ae.Pos.x - ae.Attacker->Pos.x;
        dir_y = ae.Pos.y - ae.Attacker->Pos.y;
        magnitude = sqrt(dir_x * dir_x + dir_y * dir_y);
        if (magnitude < 0.1f) return;
    }
    dir_x /= magnitude;
    dir_y /= magnitude;

    const int TRAIL_LENGTH = 35;
    const float SEGMENT_SPACING = 1.8f;
    const float WIDTH_MAX = 48.0f;  // 刀气宽
    const float WIDTH_MIN = 6.0f;

    int head_x = ae.Pos.x;
    int head_y = ae.Pos.y / 2;

    for (int i = 0; i < TRAIL_LENGTH; ++i)
    {
        float ratio = (float)i / TRAIL_LENGTH;
        float offset = i * SEGMENT_SPACING;

        float x = head_x - dir_x * offset;
        float y = head_y - dir_y * offset * 0.5f;

        int width = (int)(WIDTH_MAX - (WIDTH_MAX - WIDTH_MIN) * ratio);
        if (width < 4) width = 4;

        float alpha_ratio = 1.0f - ratio * 0.7f;
        Uint8 segment_alpha = (Uint8)(alpha * alpha_ratio);
        if (segment_alpha < 10) continue;

        SDL_SetRenderDrawColor(renderer, r, g, b, segment_alpha);

        int render_x = (int)x;
        int render_y = (int)y;

        // 绘制宽厚的刀气
        for (int w = 0; w < width; ++w)
        {
            int px = render_x - width / 2 + w;
            SDL_RenderPoint(renderer, px, render_y);
        }
    }
}

void QiRibbonRenderer::renderFistAuraWave(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                                          Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 拳劲：圆形冲击波扩散
    int cx = ae.Pos.x;
    int cy = ae.Pos.y / 2;

    const int MAX_RADIUS = 40;
    const int WAVE_COUNT = 3;

    for (int wave = 0; wave < WAVE_COUNT; ++wave)
    {
        float wave_offset = (ae.Frame + wave * 10) % 30;
        int radius = (int)(wave_offset * 1.5f);
        if (radius > MAX_RADIUS) continue;

        float alpha_ratio = 1.0f - (float)radius / MAX_RADIUS;
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

void QiRibbonRenderer::renderPalmWindFan(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                                         Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 掌风：扇形气浪
    if (!ae.Attacker) return;

    float dir_x = ae.Velocity.x;
    float dir_y = ae.Velocity.y;
    float magnitude = sqrt(dir_x * dir_x + dir_y * dir_y);
    if (magnitude < 0.1f)
    {
        dir_x = ae.Pos.x - ae.Attacker->Pos.x;
        dir_y = ae.Pos.y - ae.Attacker->Pos.y;
        magnitude = sqrt(dir_x * dir_x + dir_y * dir_y);
        if (magnitude < 0.1f) return;
    }

    int cx = ae.Pos.x;
    int cy = ae.Pos.y / 2;

    float base_angle = atan2(dir_y, dir_x);
    const float ANGLE_SPAN = M_PI / 3.0f;  // 60度扇形
    const int MAX_RADIUS = 50;

    for (float angle = -ANGLE_SPAN / 2.0f; angle <= ANGLE_SPAN / 2.0f; angle += 0.05f)
    {
        for (int radius = 10; radius < MAX_RADIUS; radius += 3)
        {
            float total_angle = base_angle + angle;
            int px = cx + (int)(radius * cos(total_angle));
            int py = cy + (int)(radius * sin(total_angle) * 0.5f);

            float alpha_ratio = 1.0f - (float)radius / MAX_RADIUS;
            Uint8 segment_alpha = (Uint8)(alpha * alpha_ratio);
            if (segment_alpha < 10) continue;

            SDL_SetRenderDrawColor(renderer, r, g, b, segment_alpha);
            SDL_RenderPoint(renderer, px, py);
        }
    }
}

void QiRibbonRenderer::renderMainRibbon(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                                        Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 默认飘带（与原 AuraEffectRenderer 类似，但简化）
    if (!ae.Attacker) return;

    float dir_x = ae.Velocity.x;
    float dir_y = ae.Velocity.y;
    float magnitude = sqrt(dir_x * dir_x + dir_y * dir_y);
    if (magnitude < 0.1f)
    {
        dir_x = ae.Pos.x - ae.Attacker->Pos.x;
        dir_y = ae.Pos.y - ae.Attacker->Pos.y;
        magnitude = sqrt(dir_x * dir_x + dir_y * dir_y);
        if (magnitude < 0.1f) return;
    }
    dir_x /= magnitude;
    dir_y /= magnitude;

    const int RIBBON_LENGTH = 50;
    const float SEGMENT_SPACING = 1.5f;
    const float WIDTH_MAX = 36.0f;
    const float WIDTH_MIN = 3.0f;

    int head_x = ae.Pos.x;
    int head_y = ae.Pos.y / 2;

    float time = ae.Frame * 0.18f;

    for (int i = 0; i < RIBBON_LENGTH; ++i)
    {
        float ratio = (float)i / RIBBON_LENGTH;
        float offset = i * SEGMENT_SPACING;

        float x = head_x - dir_x * offset;
        float y = head_y - dir_y * offset * 0.5f;

        int width = (int)(WIDTH_MAX - (WIDTH_MAX - WIDTH_MIN) * ratio);
        if (width < 3) width = 3;

        float alpha_ratio = 1.0f - ratio * 0.7f;
        Uint8 segment_alpha = (Uint8)(alpha * alpha_ratio);
        if (segment_alpha < 10) continue;

        SDL_SetRenderDrawColor(renderer, r, g, b, segment_alpha);

        int render_x = (int)x;
        int render_y = (int)y;

        for (int w = 0; w < width; ++w)
        {
            int px = render_x - width / 2 + w;
            SDL_RenderPoint(renderer, px, render_y);
        }
    }
}

void QiRibbonRenderer::renderRibbonParticles(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                                             Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
{
    // 粒子效果（简化）
    if (!ae.Attacker) return;

    int origin_x = ae.Attacker->Pos.x;
    int origin_y = ae.Attacker->Pos.y / 2;

    uint32_t seed = generateSeed(ae);
    auto rand_int = [&seed]() -> uint32_t
    {
        seed = seed * 1103515245 + 12345;
        return (seed / 65536) % 32768;
    };

    const int particle_count = 10;
    for (int i = 0; i < particle_count; ++i)
    {
        int p_x = origin_x + (rand_int() % 20) - 10;
        int p_y = origin_y + (rand_int() % 20) - 10;

        Uint8 p_alpha = alpha / 4;
        if (p_alpha < 10) continue;

        SDL_SetRenderDrawColor(renderer, r, g, b, p_alpha);
        SDL_RenderPoint(renderer, p_x, p_y);
    }
}

void QiRibbonRenderer::getAttributeColor(QiAttribute attr, bool is_friendly,
                                         Uint8& r, Uint8& g, Uint8& b) const
{
    switch (attr)
    {
        case QiAttribute::Yang:
            r = 255; g = 200; b = 80;
            break;
        case QiAttribute::Yin:
            r = 140; g = 80; b = 180;
            break;
        case QiAttribute::Fire:
            r = 255; g = 100; b = 60;
            break;
        case QiAttribute::Ice:
            r = 120; g = 200; b = 255;
            break;
        case QiAttribute::Thunder:
            r = 200; g = 150; b = 255;
            break;
        case QiAttribute::Wind:
            r = 150; g = 255; b = 180;
            break;
        case QiAttribute::Poison:
            r = 100; g = 180; b = 80;
            break;
        default:
            r = 220; g = 220; b = 220;
            break;
    }

    // 敌军降低亮度
    if (!is_friendly)
    {
        r = (Uint8)(r * 0.7f);
        g = (Uint8)(g * 0.7f);
        b = (Uint8)(b * 0.7f);
    }
}

uint32_t QiRibbonRenderer::generateSeed(const BattleSceneAct::AttackEffect& ae) const
{
    return (uint32_t)(ae.Pos.x * 1337 + ae.Pos.y * 31337 + (ae.Attacker ? ae.Attacker->ID : 0));
}

float QiRibbonRenderer::perlinNoise(float x) const
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

float QiRibbonRenderer::smoothstep(float t) const
{
    return t * t * (3.0f - 2.0f * t);
}

} // namespace QiPhysics
