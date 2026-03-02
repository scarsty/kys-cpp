#include "QiEffectConfig.h"
#include "GameUtil.h"
#include "yaml-cpp/yaml.h"
#include <print>

namespace QiEffect
{
namespace
{

static bool parseShape(const std::string& s, Shape& out)
{
    if (s == "Fist" || s == "FistWave") { out = Shape::FistWave; return true; }
    if (s == "Sword" || s == "SwordQi") { out = Shape::SwordQi; return true; }
    if (s == "Blade" || s == "BladeQi") { out = Shape::BladeQi; return true; }
    if (s == "Palm" || s == "PalmWind") { out = Shape::PalmWind; return true; }
    if (s == "Ribbon" || s == "SpecialAura") { out = Shape::SpecialAura; return true; }
    return false;
}

static bool parseAttribute(const std::string& s, Attribute& out)
{
    if (s == "None") { out = Attribute::None; return true; }
    if (s == "Metal") { out = Attribute::Metal; return true; }
    if (s == "Wood") { out = Attribute::Wood; return true; }
    if (s == "Water") { out = Attribute::Water; return true; }
    if (s == "Wind") { out = Attribute::Wind; return true; }
    if (s == "Fire") { out = Attribute::Fire; return true; }
    if (s == "Earth") { out = Attribute::Earth; return true; }
    if (s == "Yang") { out = Attribute::Yang; return true; }
    if (s == "Yin") { out = Attribute::Yin; return true; }
    return false;
}

static bool parseCompression(const std::string& s, Compression& out)
{
    if (s == "Dispersed") { out = Compression::Dispersed; return true; }
    if (s == "Normal") { out = Compression::Normal; return true; }
    if (s == "Compressed") { out = Compression::Compressed; return true; }
    if (s == "Explosive") { out = Compression::Explosive; return true; }
    return false;
}

static void applyNodeToParams(const YAML::Node& n, VisualParams& p)
{
    if (!n) return;
    if (auto v = n["shape"]) parseShape(v.as<std::string>(), p.shape);
    if (auto v = n["attribute"]) parseAttribute(v.as<std::string>(), p.attribute);
    if (auto v = n["compression"]) parseCompression(v.as<std::string>(), p.compression);

    if (auto v = n["turbulence"]) p.turbulence = v.as<float>();
    if (auto v = n["brightness"]) p.brightness = v.as<float>();
    if (auto v = n["temperature"]) p.temperature = v.as<float>();
    if (auto v = n["speed_multiplier"]) p.speed_multiplier = v.as<float>();

    if (auto v = n["particle_count"]) p.particle_count = v.as<int>();
    if (auto v = n["trail_length"]) p.trail_length = v.as<float>();
    if (auto v = n["width_max"]) p.width_max = v.as<float>();
    if (auto v = n["width_min"]) p.width_min = v.as<float>();
}

}  // namespace

QiEffectConfig& QiEffectConfig::instance()
{
    static QiEffectConfig inst;
    return inst;
}

void QiEffectConfig::ensureLoaded()
{
    if (loaded_) return;
    loaded_ = loadFromDisk();
}

bool QiEffectConfig::loadFromDisk()
{
    const std::string path = GameUtil::PATH() + "config/qi_effects.yaml";
    YAML::Node root;
    try { root = YAML::LoadFile(path); }
    catch (const YAML::Exception& e)
    {
        std::print("【真气特效配置】无法读取文件 {}: {}\n", path, e.what());
        return false;
    }

    by_magic_id_.clear();
    by_magic_type_.clear();
    defaults_ = VisualParams{};

    YAML::Node cfg = root["qi_effects"] ? root["qi_effects"] : root;

    if (auto n = cfg["defaults"]) applyNodeToParams(n, defaults_);

    if (auto n = cfg["by_magic_id"])
    {
        for (const auto& it : n)
        {
            const int id = it.first.as<int>();
            VisualParams p = defaults_;
            applyNodeToParams(it.second, p);
            by_magic_id_[id] = p;
        }
    }

    if (auto n = cfg["by_magic_type"])
    {
        for (const auto& it : n)
        {
            const int type = it.first.as<int>();
            VisualParams p = defaults_;
            applyNodeToParams(it.second, p);
            by_magic_type_[type] = p;
        }
    }

    return true;
}

bool QiEffectConfig::tryGetParams(const Magic* magic, VisualParams& out) const
{
    if (!loaded_) return false;

    if (!magic)
    {
        out = defaults_;
        return true;
    }

    if (auto it = by_magic_id_.find(magic->ID); it != by_magic_id_.end())
    {
        out = it->second;
        return true;
    }

    if (auto it = by_magic_type_.find(magic->MagicType); it != by_magic_type_.end())
    {
        out = it->second;
        return true;
    }

    return false;
}

}  // namespace QiEffect
