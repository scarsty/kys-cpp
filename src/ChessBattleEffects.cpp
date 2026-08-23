#include "ChessBattleEffects.h"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <set>
#include <span>
#include <string_view>
#include <type_traits>

namespace KysChess
{

namespace
{

template <typename Enum>
std::optional<Enum> parseLabel(
    std::string_view label,
    std::initializer_list<std::pair<std::string_view, Enum>> labels)
{
    for (const auto& [candidate, value] : labels)
    {
        if (candidate == label) return value;
    }
    return std::nullopt;
}

bool validateKnownKeys(
    const YAML::Node& node,
    std::initializer_list<std::string_view> allowed,
    std::string& error)
{
    if (!node || !node.IsMap())
    {
        error = "節點必須是映射表";
        return false;
    }
    for (const auto& entry : node)
    {
        const auto key = entry.first.as<std::string>();
        if (std::ranges::find(allowed, key) == allowed.end())
        {
            error = std::format("未知欄位「{}」", key);
            return false;
        }
    }
    return true;
}

bool requiredString(const YAML::Node& node, std::string_view key, std::string& value, std::string& error)
{
    const auto field = node[std::string(key)];
    if (!field)
    {
        error = std::format("缺少「{}」欄位", key);
        return false;
    }
    try
    {
        value = field.as<std::string>();
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        error = std::format("「{}」不是有效字串: {}", key, ex.what());
        return false;
    }
}

bool optionalInt(const YAML::Node& node, std::string_view key, int& value, std::string& error)
{
    const auto field = node[std::string(key)];
    if (!field) return true;
    try
    {
        value = field.as<int>();
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        error = std::format("「{}」不是有效整數: {}", key, ex.what());
        return false;
    }
}

bool requiredInt(const YAML::Node& node, std::string_view key, int& value, std::string& error)
{
    if (!node[std::string(key)])
    {
        error = std::format("缺少「{}」欄位", key);
        return false;
    }
    return optionalInt(node, key, value, error);
}

bool parseActivationLimitNode(
    const YAML::Node& node,
    EffectActivationLimit& out,
    std::string& error)
{
    if (!validateKnownKeys(node, { "範圍", "次數" }, error)) return false;

    std::string scopeLabel;
    if (!requiredString(node, "範圍", scopeLabel, error)) return false;
    const auto scope = parseLabel<EffectActivationScope>(scopeLabel, {
        { "每次施放每個目標", EffectActivationScope::PerCastPerTarget },
    });
    if (!scope)
    {
        error = std::format("未知觸發限制範圍「{}」", scopeLabel);
        return false;
    }

    out = {};
    out.scope = *scope;
    return requiredInt(node, "次數", out.maxEvaluations, error);
}

bool optionalBool(const YAML::Node& node, std::string_view key, bool& value, std::string& error)
{
    const auto field = node[std::string(key)];
    if (!field) return true;
    try
    {
        value = field.as<bool>();
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        error = std::format("「{}」不是有效布林值: {}", key, ex.what());
        return false;
    }
}

bool optionalBool(const YAML::Node& node,
                  std::string_view key,
                  std::optional<bool>& value,
                  std::string& error)
{
    const auto field = node[std::string(key)];
    if (!field) return true;
    try
    {
        value = field.as<bool>();
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        error = std::format("「{}」不是有效布林值: {}", key, ex.what());
        return false;
    }
}

bool parseDamageKindLabel(
    std::string_view label,
    BattleDamageKind& out,
    std::string& error)
{
    const auto parsed = parseLabel<BattleDamageKind>(label, {
        { "物理", BattleDamageKind::Physical }, { "招式", BattleDamageKind::Skill },
        { "純粹", BattleDamageKind::Pure }, { "中毒", BattleDamageKind::Poison },
        { "流血", BattleDamageKind::Bleed },
        { "特效", BattleDamageKind::Effect }, { "反彈", BattleDamageKind::Reflected },
        { "處決", BattleDamageKind::Execute },
    });
    if (!parsed)
    {
        error = std::format("未知傷害種類「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseStatusKind(std::string_view label, BattleStatusKind& out, std::string& error);
bool parseEffectStateSlot(const YAML::Node& node, EffectStateSlot& out, std::string& error);

bool parseEffectNumberNode(const YAML::Node& node, EffectNumber& out, std::string& error)
{
    if (!node)
    {
        error = "缺少「數值」欄位";
        return false;
    }
    if (node.IsScalar())
    {
        try
        {
            out = {};
            out.flat = node.as<int>();
            return true;
        }
        catch (const YAML::Exception& ex)
        {
            error = std::format("數值不是有效整數: {}", ex.what());
            return false;
        }
    }
    if (!validateKnownKeys(node, { "基準", "乘數基準", "狀態", "狀態槽", "固定", "百分比", "取整", "最小", "最大" }, error)) return false;
    out = {};
    auto readBase = [&](const YAML::Node& base, EffectNumberBase& destination)
    {
        const auto label = base.as<std::string>();
        const auto parsed = parseLabel<EffectNumberBase>(label, {
            { "固定值", EffectNumberBase::Constant },
            { "來源星級", EffectNumberBase::SourceStar },
            { "來源攻擊", EffectNumberBase::SourceAttack },
            { "來源最大生命", EffectNumberBase::SourceMaxHp },
            { "來源已損生命比例", EffectNumberBase::SourceMissingHpRatio },
            { "來源目前內力比例", EffectNumberBase::SourceCurrentMpRatio },
            { "目標最大生命", EffectNumberBase::TargetMaxHp },
            { "目標目前生命", EffectNumberBase::TargetCurrentHp },
            { "目標目前護盾", EffectNumberBase::TargetCurrentShield },
            { "目標目前冷卻", EffectNumberBase::TargetCurrentCooldown },
            { "實際生命傷害", EffectNumberBase::FinalHpDamage },
            { "累計狀態值", EffectNumberBase::AccumulatedStateValue },
            { "來源狀態強度", EffectNumberBase::SourceStatusPotency },
            { "來源狀態層數", EffectNumberBase::SourceStatusStacks },
            { "狀態槽值", EffectNumberBase::StoredStateValue },
        });
        if (!parsed)
        {
            error = std::format("未知數值基準「{}」", label);
            return false;
        }
        destination = *parsed;
        return true;
    };
    if (const auto base = node["基準"])
    {
        if (!readBase(base, out.base)) return false;
    }
    if (const auto multiplier = node["乘數基準"])
    {
        EffectNumberBase parsedMultiplier{};
        if (!readBase(multiplier, parsedMultiplier)) return false;
        out.multiplierBase = parsedMultiplier;
    }
    if (const auto status = node["狀態"])
    {
        BattleStatusKind parsedStatus{};
        const auto label = status.as<std::string>();
        if (!parseStatusKind(label, parsedStatus, error)) return false;
        out.status = std::string(battleStatusLabel(parsedStatus));
    }
    if (node["狀態槽"])
    {
        EffectStateSlot slot{};
        if (!parseEffectStateSlot(node["狀態槽"], slot, error)) return false;
        out.stateSlot = slot;
    }
    if (!optionalInt(node, "固定", out.flat, error)
        || !optionalInt(node, "百分比", out.percent, error)) return false;
    if (const auto rounding = node["取整"])
    {
        const auto label = rounding.as<std::string>();
        const auto parsed = parseLabel<EffectRounding>(label, {
            { "向零", EffectRounding::TowardZero },
            { "向下", EffectRounding::Floor },
            { "向上", EffectRounding::Ceil },
            { "四捨五入", EffectRounding::Nearest },
        });
        if (!parsed)
        {
            error = std::format("未知取整方式「{}」", label);
            return false;
        }
        out.rounding = *parsed;
    }
    if (node["最小"])
    {
        int value{};
        if (!requiredInt(node, "最小", value, error)) return false;
        out.minimum = value;
    }
    if (node["最大"])
    {
        int value{};
        if (!requiredInt(node, "最大", value, error)) return false;
        out.maximum = value;
    }
    return true;
}

bool parseSelectorNode(const YAML::Node& node, EffectSelector& out, std::string& error)
{
    out = {};
    std::string label;
    if (node.IsScalar())
    {
        label = node.as<std::string>();
    }
    else
    {
        if (!validateKnownKeys(node, { "類型", "數量", "半徑格數", "方形邊長", "隊伍", "平手", "排除效果擁有者", "武功", "武器類型", "必含目標" }, error)
            || !requiredString(node, "類型", label, error)) return false;
        if (!optionalInt(node, "數量", out.count, error)
            || !optionalInt(node, "半徑格數", out.radiusTiles, error)
            || !optionalInt(node, "方形邊長", out.squareSideTiles, error)
            || !optionalBool(node, "排除效果擁有者", out.excludeOwner, error)
            || !optionalInt(node, "武功", out.requiredMagicId, error)
            || !optionalInt(node, "武器類型", out.requiredWeaponType, error)) return false;
        if (const auto team = node["隊伍"])
        {
            const auto teamLabel = team.as<std::string>();
            const auto parsed = parseLabel<EffectTeamFilter>(teamLabel, {
                { "不限", EffectTeamFilter::Any },
                { "友方", EffectTeamFilter::Ally },
                { "敵方", EffectTeamFilter::Enemy },
            });
            if (!parsed)
            {
                error = std::format("未知隊伍篩選「{}」", teamLabel);
                return false;
            }
            out.team = *parsed;
        }
        if (const auto tie = node["平手"])
        {
            const auto tieLabel = tie.as<std::string>();
            const auto parsed = parseLabel<EffectTieBreak>(tieLabel, {
                { "單位ID", EffectTieBreak::UnitId },
                { "戰鬥亂數", EffectTieBreak::BattleRandom },
            });
            if (!parsed)
            {
                error = std::format("未知平手規則「{}」", tieLabel);
                return false;
            }
            out.tieBreak = *parsed;
        }
        if (const auto required = node["必含目標"])
        {
            const auto label = required.as<std::string>();
            const auto parsed = parseLabel<EffectRequiredTarget>(label, {
                { "自身", EffectRequiredTarget::Self },
                { "來源單位", EffectRequiredTarget::SourceUnit },
                { "交易目標", EffectRequiredTarget::TransactionTarget },
                { "命中目標", EffectRequiredTarget::HitTarget },
                { "原攻擊目標", EffectRequiredTarget::OriginalAttackTarget },
            });
            if (!parsed)
            {
                error = std::format("未知必含目標「{}」", label);
                return false;
            }
            out.requiredTarget = *parsed;
        }
    }

    const auto kind = parseLabel<EffectSelectorKind>(label, {
        { "自身", EffectSelectorKind::Self },
        { "來源單位", EffectSelectorKind::SourceUnit },
        { "交易目標", EffectSelectorKind::TransactionTarget },
        { "命中目標", EffectSelectorKind::HitTarget },
        { "原攻擊目標", EffectSelectorKind::OriginalAttackTarget },
        { "羈絆成員", EffectSelectorKind::ComboMembers },
        { "所有存活單位", EffectSelectorKind::AllLivingUnits },
        { "友軍", EffectSelectorKind::Allies },
        { "全隊", EffectSelectorKind::Allies },
        { "敵軍", EffectSelectorKind::Enemies },
        { "所有敵人", EffectSelectorKind::Enemies },
        { "最低生命友軍", EffectSelectorKind::LowestHpAllies },
        { "最低內力友軍", EffectSelectorKind::LowestMpAllies },
        { "最高內力敵人", EffectSelectorKind::HighestMpEnemy },
        { "最強敵人", EffectSelectorKind::StrongestEnemies },
        { "最近敵人", EffectSelectorKind::NearestEnemies },
        { "最遠敵人", EffectSelectorKind::FarthestEnemy },
        { "半徑內單位", EffectSelectorKind::UnitsInRadius },
        { "方形內單位", EffectSelectorKind::UnitsInSquare },
        { "指定武器友軍", EffectSelectorKind::AlliesUsingWeapon },
    });
    if (!kind)
    {
        error = std::format("未知目標選擇器「{}」", label);
        return false;
    }
    out.kind = *kind;
    if (!node.IsScalar())
    {
        bool knownFields = false;
        switch (out.kind)
        {
        case EffectSelectorKind::Self:
        case EffectSelectorKind::SourceUnit:
        case EffectSelectorKind::TransactionTarget:
        case EffectSelectorKind::HitTarget:
        case EffectSelectorKind::OriginalAttackTarget:
            knownFields = validateKnownKeys(node, { "類型", "排除效果擁有者" }, error);
            break;
        case EffectSelectorKind::ComboMembers:
        case EffectSelectorKind::AllLivingUnits:
        case EffectSelectorKind::Allies:
        case EffectSelectorKind::Enemies:
            knownFields = validateKnownKeys(node, { "類型", "數量", "平手", "排除效果擁有者", "武功", "必含目標" }, error);
            break;
        case EffectSelectorKind::LowestHpAllies:
        case EffectSelectorKind::LowestMpAllies:
        case EffectSelectorKind::StrongestEnemies:
        case EffectSelectorKind::NearestEnemies:
            knownFields = validateKnownKeys(node, { "類型", "數量", "平手", "排除效果擁有者", "必含目標" }, error);
            break;
        case EffectSelectorKind::HighestMpEnemy:
        case EffectSelectorKind::FarthestEnemy:
            knownFields = validateKnownKeys(node, { "類型", "平手", "排除效果擁有者" }, error);
            break;
        case EffectSelectorKind::UnitsInRadius:
            knownFields = validateKnownKeys(node, { "類型", "數量", "半徑格數", "隊伍", "平手", "排除效果擁有者", "必含目標" }, error);
            break;
        case EffectSelectorKind::UnitsInSquare:
            knownFields = validateKnownKeys(node, { "類型", "數量", "方形邊長", "隊伍", "平手", "排除效果擁有者", "必含目標" }, error);
            break;
        case EffectSelectorKind::AlliesUsingWeapon:
            knownFields = validateKnownKeys(node, { "類型", "排除效果擁有者", "武器類型", "必含目標" }, error);
            break;
        }
        if (!knownFields) return false;
    }
    if (out.count < 0)
    {
        error = "目標數量不可為負數";
        return false;
    }
    if (out.kind == EffectSelectorKind::AlliesUsingWeapon && out.requiredWeaponType < 0)
    {
        error = "指定武器友軍需要非負「武器類型」";
        return false;
    }
    return true;
}

bool parseConditionNode(const YAML::Node& node, EffectCondition& out, std::string& error)
{
    if (!node || !node.IsMap())
    {
        error = "條件必須是映射表";
        return false;
    }
    std::string type;
    if (!requiredString(node, "類型", type, error)) return false;
    const auto known = [&](std::initializer_list<std::string_view> keys)
    {
        return validateKnownKeys(node, keys, error);
    };
    if (type == "僅限絕招")
    {
        if (!known({ "類型" })) return false;
        out = IsUltimateCondition{};
    }
    else if (type == "武功相符")
    {
        int magicId{};
        if (!known({ "類型", "武功" }) || !requiredInt(node, "武功", magicId, error)) return false;
        out = MagicIdEqualsCondition{ magicId };
    }
    else if (type == "僅限主彈道")
    {
        if (!known({ "類型" })) return false;
        out = IsMainProjectileCondition{};
    }
    else if (type == "僅限根攻擊")
    {
        if (!known({ "類型" })) return false;
        out = IsRootAttackCondition{};
    }
    else if (type == "自身生命不高於")
    {
        int percent{};
        if (!known({ "類型", "百分比" }) || !requiredInt(node, "百分比", percent, error)) return false;
        out = SourceHpRatioAtMostCondition{ percent };
    }
    else if (type == "自身生命低於")
    {
        int percent{};
        if (!known({ "類型", "百分比" }) || !requiredInt(node, "百分比", percent, error)) return false;
        out = SourceHpRatioBelowCondition{ percent };
    }
    else if (type == "自身為最後存活")
    {
        if (!known({ "類型" })) return false;
        out = SourceIsLastAliveCondition{};
    }
    else if (type == "目標生命不高於")
    {
        int percent{};
        if (!known({ "類型", "百分比" }) || !requiredInt(node, "百分比", percent, error)) return false;
        out = TargetHpRatioAtMostCondition{ percent };
    }
    else if (type == "目標非無敵")
    {
        if (!known({ "類型" })) return false;
        out = TargetNotInvincibleCondition{};
    }
    else if (type == "自身有狀態")
    {
        std::string state;
        if (!known({ "類型", "狀態" }) || !requiredString(node, "狀態", state, error)) return false;
        out = SourceHasStateCondition{ std::move(state) };
    }
    else if (type == "目標有狀態")
    {
        std::string state;
        if (!known({ "類型", "狀態" }) || !requiredString(node, "狀態", state, error)) return false;
        out = TargetHasStateCondition{ std::move(state) };
    }
    else if (type == "目標有此來源狀態")
    {
        std::string state;
        BattleStatusKind parsed{};
        if (!known({ "類型", "狀態" })
            || !requiredString(node, "狀態", state, error)
            || !parseStatusKind(state, parsed, error)) return false;
        out = TargetHasStateFromEffectOwnerCondition{
            std::string(battleStatusLabel(parsed)),
        };
    }
    else if (type == "自身層數至少")
    {
        std::string stack;
        int count{};
        if (!known({ "類型", "狀態", "層數" })
            || !requiredString(node, "狀態", stack, error)
            || !requiredInt(node, "層數", count, error)) return false;
        out = SourceStackAtLeastCondition{ std::move(stack), count };
    }
    else if (type == "其他存活友軍使用武功")
    {
        int magicId{};
        if (!known({ "類型", "武功" }) || !requiredInt(node, "武功", magicId, error)) return false;
        out = OtherLivingAllyUsesMagicCondition{ magicId };
    }
    else if (type == "不同目標數至少")
    {
        int count{};
        if (!known({ "類型", "數量" }) || !requiredInt(node, "數量", count, error)) return false;
        out = CastDistinctTargetCountAtLeastCondition{ count };
    }
    else if (type == "攻擊序號")
    {
        int ordinal{};
        if (!known({ "類型", "序號" }) || !requiredInt(node, "序號", ordinal, error)) return false;
        out = AttackOrdinalEqualsCondition{ ordinal };
    }
    else if (type == "治療種類符合" || type == "傷害種類符合")
    {
        const auto fieldName = type == "治療種類符合" ? "治療種類" : "傷害種類";
        if (!known({ "類型", fieldName })) return false;
        const auto values = node[fieldName];
        if (!values || !values.IsSequence() || values.size() == 0)
        {
            error = std::format("「{}」必須是非空列表", fieldName);
            return false;
        }
        std::vector<std::string> labels;
        labels.reserve(values.size());
        for (const auto& value : values) labels.push_back(value.as<std::string>());
        if (type == "治療種類符合") out = HealKindInCondition{ std::move(labels) };
        else out = DamageKindInCondition{ std::move(labels) };
    }
    else if (type == "傷害來自招式")
    {
        if (!known({ "類型" })) return false;
        out = DamageOriginIsAttackCondition{};
    }
    else if (type == "已接受命中")
    {
        AcceptedHitCondition condition;
        if (!known({ "類型", "需要正傷害", "排除反彈" })
            || !optionalBool(node, "需要正傷害", condition.requirePositiveDamage, error)
            || !optionalBool(node, "排除反彈", condition.excludeReflected, error)) return false;
        out = condition;
    }
    else if (type == "事件目標屬於綁定來源")
    {
        if (!known({ "類型" })) return false;
        out = EventTargetBelongsToBoundSourceCondition{};
    }
    else if (type == "傷害造成死亡")
    {
        if (!known({ "類型" })) return false;
        out = DamageKilledTargetCondition{};
    }
    else if (type == "傷害方位")
    {
        std::string perspective;
        if (!known({ "類型", "方位" }) || !requiredString(node, "方位", perspective, error)) return false;
        const auto parsed = parseLabel<DamagePerspective>(perspective, {
            { "造成", DamagePerspective::Dealt },
            { "承受", DamagePerspective::Received },
        });
        if (!parsed)
        {
            error = std::format("未知傷害方位「{}」", perspective);
            return false;
        }
        out = DamagePerspectiveCondition{ *parsed };
    }
    else if (type == "受益者施放前滿內")
    {
        if (!known({ "類型" })) return false;
        out = TargetMpWasFullBeforeCastCondition{};
    }
    else if (type == "有合法隨機目標")
    {
        if (!known({ "類型" })) return false;
        out = RandomSelectionAvailableCondition{};
    }
    else
    {
        error = std::format("未知條件類型「{}」", type);
        return false;
    }
    return true;
}

bool parseStackPolicy(const YAML::Node& node, EffectStackPolicy& out, std::string& error)
{
    if (!node) return true;
    const auto label = node.as<std::string>();
    const auto parsed = parseLabel<EffectStackPolicy>(label, {
        { "獨立", EffectStackPolicy::Independent },
        { "刷新", EffectStackPolicy::Refresh },
        { "取代", EffectStackPolicy::Replace },
        { "保留最強", EffectStackPolicy::KeepStrongest },
        { "增加層數", EffectStackPolicy::AddStack },
    });
    if (!parsed)
    {
        error = std::format("未知合併方式「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseStatusKind(std::string_view label, BattleStatusKind& out, std::string& error)
{
    const auto parsed = parseLabel<BattleStatusKind>(label, {
        { "中毒", BattleStatusKind::Poison },
        { "流血", BattleStatusKind::Bleed },
        { "眩暈", BattleStatusKind::Stun },
        { "封內", BattleStatusKind::MpBlocked },
        { "寒毒", BattleStatusKind::ColdPoison },
        { "枯骨", BattleStatusKind::WitheredBone },
        { "七星", BattleStatusKind::SevenStarMark },
        { "化勁", BattleStatusKind::NeutralizeForce },
        { "刺目", BattleStatusKind::Blinded },
        { "下一次攻擊落空", BattleStatusKind::NextAttackMiss },
        { "傷害抵擋", BattleStatusKind::DamageBlockLayer },
        { "單次承傷上限", BattleStatusKind::SingleHitCapLayer },
        { "戰意", BattleStatusKind::BattleSpirit },
        { "真氣", BattleStatusKind::TrueQi },
        { "毒爆", BattleStatusKind::PoisonExplosion },
        { "無影", BattleStatusKind::Shadowless },
        { "下一次攻擊必定暴擊", BattleStatusKind::NextAttackCritical },
    });
    if (!parsed)
    {
        error = std::format("未知狀態「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseDamageChannel(std::string_view label, DamageChannel& out, std::string& error)
{
    const auto parsed = parseLabel<DamageChannel>(label, {
        { "招式", DamageChannel::Skill },
        { "持續傷害", DamageChannel::Dot },
        { "特效", DamageChannel::Effect },
        { "反彈", DamageChannel::Reflected },
        { "全部", DamageChannel::All },
    });
    if (!parsed)
    {
        error = std::format("未知傷害種類「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseEffectStateSlot(const YAML::Node& node, EffectStateSlot& out, std::string& error)
{
    if (!node)
    {
        error = "缺少「狀態槽」欄位";
        return false;
    }
    const auto label = node.as<std::string>();
    const auto parsed = parseLabel<EffectStateSlot>(label, {
        { "最大招式生命傷害", EffectStateSlot::MaximumSkillHpDamage },
        { "本次施放最高生命傷害", EffectStateSlot::CastMaximumHpDamage },
        { "累計吸收傷害", EffectStateSlot::AbsorbedDamage },
        { "永久施放進展", EffectStateSlot::PermanentCastProgress },
    });
    if (!parsed)
    {
        error = std::format("未知狀態槽「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseActionNode(const YAML::Node& node, EffectAction& out, std::string& error);

bool parseActionList(const YAML::Node& node, std::vector<EffectAction>& out, std::string& error)
{
    if (!node || !node.IsSequence() || node.size() == 0)
    {
        error = "動作必須是非空列表";
        return false;
    }
    out.clear();
    out.reserve(node.size());
    for (std::size_t i = 0; i < node.size(); ++i)
    {
        EffectAction action;
        if (!parseActionNode(node[i], action, error))
        {
            error = std::format("動作#{}: {}", i + 1, error);
            return false;
        }
        out.push_back(std::move(action));
    }
    return true;
}

bool parseBorrowedRuleFilter(
    const YAML::Node& node,
    BorrowedRuleFilter& out,
    std::string& error)
{
    if (!node || !node.IsSequence() || node.size() == 0)
    {
        error = "允許動作類別必須是非空列表";
        return false;
    }
    out.allowedActionCategories.clear();
    std::set<BorrowedRuleActionCategory> seen;
    for (const auto& value : node)
    {
        const auto label = value.as<std::string>();
        const auto parsed = parseLabel<BorrowedRuleActionCategory>(label, {
            { "屬性修正", BorrowedRuleActionCategory::AttributeModifier },
            { "傷害修正", BorrowedRuleActionCategory::DamageModifier },
            { "資源變更", BorrowedRuleActionCategory::ResourceChange },
            { "治療交易修正", BorrowedRuleActionCategory::HealTransactionModifier },
            { "狀態", BorrowedRuleActionCategory::Status },
            { "傷害", BorrowedRuleActionCategory::Damage },
            { "攻擊", BorrowedRuleActionCategory::Attack },
            { "強制移動", BorrowedRuleActionCategory::ForcedMovement },
            { "區域", BorrowedRuleActionCategory::Area },
            { "修改施放", BorrowedRuleActionCategory::Cast },
            { "狀態值", BorrowedRuleActionCategory::StateValue },
            { "傷害記憶", BorrowedRuleActionCategory::DamageMemory },
            { "傷害吸收", BorrowedRuleActionCategory::DamageAbsorption },
            { "狀態傷害結算", BorrowedRuleActionCategory::StatusDamageSettlement },
        });
        if (!parsed)
        {
            error = std::format("未知可借用動作類別「{}」", label);
            return false;
        }
        if (!seen.insert(*parsed).second)
        {
            error = std::format("可借用動作類別「{}」重複", label);
            return false;
        }
        out.allowedActionCategories.push_back(*parsed);
    }
    return true;
}

bool parseCopiedMagicFilter(
    const YAML::Node& node,
    CopiedMagicFilter& out,
    std::string& error)
{
    if (!node || !node.IsSequence() || node.size() == 0)
    {
        error = "可選武功條件必須是非空列表";
        return false;
    }
    out.conditions.clear();
    std::set<CopiedMagicCondition> seen;
    for (const auto& value : node)
    {
        const auto label = value.as<std::string>();
        const auto parsed = parseLabel<CopiedMagicCondition>(label, {
            { "有絕招攻擊定義", CopiedMagicCondition::HasUltimateAttackDefinition },
            { "排除複製與借用遞迴", CopiedMagicCondition::ExcludesRecursiveEffects },
        });
        if (!parsed)
        {
            error = std::format("未知可選武功條件「{}」", label);
            return false;
        }
        if (!seen.insert(*parsed).second)
        {
            error = std::format("可選武功條件「{}」重複", label);
            return false;
        }
        out.conditions.push_back(*parsed);
    }
    return true;
}

bool parseAttribute(std::string_view label, BattleAttribute& out, std::string& error)
{
    const auto parsed = parseLabel<BattleAttribute>(label, {
        { "最大生命", BattleAttribute::MaxHp },
        { "攻擊", BattleAttribute::Attack },
        { "防禦", BattleAttribute::Defence },
        { "速度", BattleAttribute::Speed },
        { "暴擊率", BattleAttribute::CriticalChance },
        { "暴擊傷害", BattleAttribute::CriticalDamage },
        { "閃避率", BattleAttribute::DodgeChance },
        { "格擋率", BattleAttribute::BlockChance },
        { "傷害減免", BattleAttribute::DamageReduction },
        { "技能傷害", BattleAttribute::SkillDamage },
        { "彈道壓制傷害", BattleAttribute::ProjectilePressureDamage },
        { "冷卻縮減", BattleAttribute::CooldownReduction },
        { "內力回復加成", BattleAttribute::MpRecoveryBonus },
        { "僵直抗性", BattleAttribute::StaggerResistance },
        { "彈道反射率", BattleAttribute::ProjectileReflectChance },
        { "技能反彈百分比", BattleAttribute::SkillReflectPercent },
        { "格擋絕招反擊率", BattleAttribute::CounterUltimateBlockChance },
        { "閃避後暴擊", BattleAttribute::CriticalAfterDodge },
        { "滑步機率", BattleAttribute::DashChance },
        { "攻擊冷卻延長率", BattleAttribute::OutgoingCooldownExtensionChance },
        { "攻擊冷卻延長百分比", BattleAttribute::OutgoingCooldownExtensionPercent },
        { "受擊冷卻延長反擊率", BattleAttribute::IncomingCooldownExtensionChance },
        { "受擊冷卻延長百分比", BattleAttribute::IncomingCooldownExtensionPercent },
    });
    if (!parsed)
    {
        error = std::format("未知屬性「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseAttackPatternFields(const YAML::Node& node, AttackPattern& pattern, std::string& error)
{
    if (const auto style = node["樣式"])
    {
        const auto label = style.as<std::string>();
        const auto parsed = parseLabel<AttackPatternKind>(label, {
            { "保留", AttackPatternKind::Preserve },
            { "扇形", AttackPatternKind::Fan },
            { "側翼", AttackPatternKind::Flanks },
            { "同落點延遲", AttackPatternKind::SamePointSequence },
            { "多目標", AttackPatternKind::MultiTarget },
            { "最近其他敵人殘影", AttackPatternKind::EchoNearestOthers },
        });
        if (!parsed)
        {
            error = std::format("未知攻擊樣式「{}」", label);
            return false;
        }
        pattern.kind = *parsed;
    }
    return optionalInt(node, "數量", pattern.projectileCount, error)
        && optionalInt(node, "展開角度", pattern.spreadDegrees, error)
        && optionalInt(node, "間隔幀數", pattern.intervalFrames, error);
}

bool parseAttackRuntimeBehavior(
    const YAML::Node& node,
    AttackRuntimeBehavior& out,
    std::string& error)
{
    if (!node || !node.IsMap())
    {
        error = "攻擊執行行為必須是映射表";
        return false;
    }
    std::string type;
    if (!requiredString(node, "類型", type, error)) return false;

    if (type == "彈道彈射")
    {
        if (!validateKnownKeys(node, {
                "類型", "追加命中次數", "機率", "範圍像素" }, error)) return false;
        ProjectileBounceAttackBehavior behavior;
        if (!requiredInt(node, "追加命中次數", behavior.additionalHits, error)
            || !requiredInt(node, "機率", behavior.chancePct, error)
            || !requiredInt(node, "範圍像素", behavior.rangePixels, error)) return false;
        out = behavior;
        return true;
    }
    if (type == "範圍追蹤")
    {
        if (!validateKnownKeys(node, {
                "類型", "範圍像素", "傷害倍率" }, error)) return false;
        NearbyTrackingAttackBehavior behavior;
        if (!requiredInt(node, "範圍像素", behavior.rangePixels, error)
            || !requiredInt(node, "傷害倍率", behavior.damagePct, error)) return false;
        out = behavior;
        return true;
    }
    if (type == "延遲替代攻擊")
    {
        if (!validateKnownKeys(node, {
                "類型", "延遲幀數", "傷害倍率", "攻擊者獲得格擋機率" }, error)) return false;
        DelayedAlternateAttackBehavior behavior;
        if (!requiredInt(node, "延遲幀數", behavior.delayFrames, error)
            || !requiredInt(node, "傷害倍率", behavior.damagePct, error)
            || !requiredInt(node, "攻擊者獲得格擋機率",
                behavior.attackerBlockGainChancePct,
                error)) return false;
        out = behavior;
        return true;
    }
    if (type == "擴張螺旋")
    {
        if (!validateKnownKeys(node, {
                "類型", "彈道數量", "流血層數" }, error)) return false;
        ExpandingSpiralAttackBehavior behavior;
        if (!requiredInt(node, "彈道數量", behavior.projectileCount, error)
            || !requiredInt(node, "流血層數", behavior.bleedStacks, error)) return false;
        out = behavior;
        return true;
    }

    error = std::format("未知攻擊執行行為「{}」", type);
    return false;
}

bool parsePropagation(const YAML::Node& node, CastPropagationPolicy& out, std::string& error)
{
    if (!node) return true;
    const auto label = node.as<std::string>();
    const auto parsed = parseLabel<CastPropagationPolicy>(label, {
        { "來源全部規則", CastPropagationPolicy::SourceRules },
        { "僅來源命中規則", CastPropagationPolicy::SourceHitRulesOnly },
        { "不傳播大招規則", CastPropagationPolicy::SuppressUltimateRules },
        { "借用大招規則", CastPropagationPolicy::BorrowedUltimateRules },
        { "不傳播效果規則", CastPropagationPolicy::NoEffectRules },
    });
    if (!parsed)
    {
        error = std::format("未知傳播政策「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseAreaModifierNode(const YAML::Node& node, AreaModifier& out, std::string& error)
{
    if (!validateKnownKeys(node, {
            "類型", "關係", "屬性", "數值", "百分比", "傷害種類", "追蹤",
            "彈速百分比", "彈道壓制百分比", "阻擋方向", "重疊方式" }, error)) return false;
    out = {};
    std::string type;
    if (!requiredString(node, "類型", type, error)) return false;
    const auto kind = parseLabel<AreaModifierKind>(type, {
        { "屬性修正", AreaModifierKind::Attribute },
        { "造成傷害修正", AreaModifierKind::OutgoingDamage },
        { "攻擊生成修正", AreaModifierKind::AttackSpawn },
        { "強制移動免疫", AreaModifierKind::ForcedMoveImmunity },
    });
    if (!kind)
    {
        error = std::format("未知區域修正類型「{}」", type);
        return false;
    }
    out.kind = *kind;
    const auto relation = node["關係"];
    if (!relation)
    {
        error = "區域修正缺少「關係」欄位";
        return false;
    }
    {
        const auto label = relation.as<std::string>();
        const auto parsed = parseLabel<EffectTeamFilter>(label, {
            { "友方", EffectTeamFilter::Ally },
            { "敵方", EffectTeamFilter::Enemy },
        });
        if (!parsed)
        {
            error = std::format("未知區域關係「{}」", label);
            return false;
        }
        out.relation = *parsed;
    }
    if (const auto attribute = node["屬性"])
    {
        if (!parseAttribute(attribute.as<std::string>(), out.attribute, error)) return false;
    }
    if (node["數值"] && !parseEffectNumberNode(node["數值"], out.amount, error)) return false;
    if (!optionalInt(node, "百分比", out.percent, error)) return false;
    if (const auto damageKind = node["傷害種類"])
    {
        if (!parseDamageChannel(damageKind.as<std::string>(), out.damageChannel, error)) return false;
    }
    if (node["追蹤"])
    {
        bool tracking{};
        if (!optionalBool(node, "追蹤", tracking, error)) return false;
        out.tracking = tracking;
    }
    if (node["彈速百分比"])
    {
        int value{};
        if (!requiredInt(node, "彈速百分比", value, error)) return false;
        out.speedPct = value;
    }
    if (node["彈道壓制百分比"])
    {
        int value{};
        if (!requiredInt(node, "彈道壓制百分比", value, error)) return false;
        out.projectilePressurePct = value;
    }
    if (const auto direction = node["阻擋方向"])
    {
        const auto label = direction.as<std::string>();
        const auto parsed = parseLabel<ForceMoveDirection>(label, {
            { "遠離來源", ForceMoveDirection::AwayFromSource },
            { "接近來源", ForceMoveDirection::TowardSource },
            { "接近指定點", ForceMoveDirection::TowardPoint },
        });
        if (!parsed)
        {
            error = std::format("未知強制移動方向「{}」", label);
            return false;
        }
        out.blockedDirection = *parsed;
    }
    const auto overlap = node["重疊方式"];
    if (!overlap)
    {
        error = "區域修正缺少「重疊方式」欄位";
        return false;
    }
    {
        const auto label = overlap.as<std::string>();
        const auto parsed = parseLabel<AreaOverlapPolicy>(label, {
            { "相加", AreaOverlapPolicy::Add },
            { "保留最強", AreaOverlapPolicy::KeepStrongest },
            { "任一", AreaOverlapPolicy::Any },
        });
        if (!parsed)
        {
            error = std::format("未知區域重疊方式「{}」", label);
            return false;
        }
        out.overlap = *parsed;
    }

    const auto unexpected = [&](bool condition, std::string_view field)
    {
        if (!condition) return false;
        error = std::format("區域修正「{}」不接受「{}」欄位", type, field);
        return true;
    };
    if (out.kind == AreaModifierKind::Attribute)
    {
        if (!node["屬性"] || !node["數值"])
        {
            error = "屬性區域修正需要「屬性」與「數值」";
            return false;
        }
        if (unexpected(node["百分比"] || node["傷害種類"] || node["追蹤"]
                || node["彈速百分比"] || node["彈道壓制百分比"] || node["阻擋方向"], "非屬性修正")) return false;
        if (out.overlap == AreaOverlapPolicy::Any)
        {
            error = "數值屬性區域修正不可使用「任一」重疊方式";
            return false;
        }
    }
    else if (out.kind == AreaModifierKind::OutgoingDamage)
    {
        if (!node["百分比"] || !node["傷害種類"])
        {
            error = "造成傷害區域修正需要「百分比」與「傷害種類」";
            return false;
        }
        if (unexpected(node["屬性"] || node["數值"] || node["追蹤"]
                || node["彈速百分比"] || node["彈道壓制百分比"] || node["阻擋方向"], "非傷害修正")) return false;
        if (out.overlap == AreaOverlapPolicy::Any)
        {
            error = "數值傷害區域修正不可使用「任一」重疊方式";
            return false;
        }
    }
    else if (out.kind == AreaModifierKind::AttackSpawn)
    {
        if (!node["追蹤"] && !node["彈速百分比"] && !node["彈道壓制百分比"])
        {
            error = "攻擊生成區域修正至少需要一個修正欄位";
            return false;
        }
        if (unexpected(node["屬性"] || node["數值"] || node["百分比"]
                || node["傷害種類"] || node["阻擋方向"], "非攻擊生成修正")) return false;
        if (node["追蹤"] && out.overlap != AreaOverlapPolicy::Any)
        {
            error = "追蹤布林修正必須使用「任一」重疊方式";
            return false;
        }
        if ((node["彈速百分比"] || node["彈道壓制百分比"])
            && out.overlap == AreaOverlapPolicy::Any)
        {
            error = "數值攻擊生成修正不可使用「任一」重疊方式";
            return false;
        }
    }
    else
    {
        if (!node["阻擋方向"])
        {
            error = "強制移動免疫需要「阻擋方向」";
            return false;
        }
        if (unexpected(node["屬性"] || node["數值"] || node["百分比"]
                || node["傷害種類"] || node["追蹤"] || node["彈速百分比"]
                || node["彈道壓制百分比"], "非強制移動免疫")) return false;
        if (out.overlap != AreaOverlapPolicy::Any)
        {
            error = "強制移動免疫必須使用「任一」重疊方式";
            return false;
        }
    }
    if (out.tracking) out.trackingOverlap = out.overlap;
    if (out.speedPct) out.speedOverlap = out.overlap;
    if (out.projectilePressurePct) out.projectilePressureOverlap = out.overlap;
    return true;
}

bool parseActionNode(const YAML::Node& node, EffectAction& out, std::string& error)
{
    if (!node || !node.IsMap())
    {
        error = "動作必須是映射表";
        return false;
    }
    std::string type;
    if (!requiredString(node, "類型", type, error)) return false;

    if (type == "屬性修正")
    {
        if (!validateKnownKeys(node, { "類型", "屬性", "方式", "數值", "持續幀數", "合併方式", "層數上限", "每層", "疊加範圍" }, error)) return false;
        ModifyAttributeAction action;
        std::string attribute;
        std::string operation;
        if (!requiredString(node, "屬性", attribute, error)
            || !requiredString(node, "方式", operation, error)
            || !parseAttribute(attribute, action.attribute, error)
            || !parseEffectNumberNode(node["數值"], action.amount, error)) return false;
        const auto parsedOperation = parseLabel<AttributeOperation>(operation, {
            { "固定加算", AttributeOperation::FlatAdd },
            { "百分比加算", AttributeOperation::PercentAdd },
            { "覆寫", AttributeOperation::Override },
            { "乘算", AttributeOperation::Multiply },
            { "至少為", AttributeOperation::AtLeast },
        });
        if (!parsedOperation)
        {
            error = std::format("未知屬性運算「{}」", operation);
            return false;
        }
        action.operation = *parsedOperation;
        if (!optionalInt(node, "持續幀數", action.durationFrames, error)
            || !parseStackPolicy(node["合併方式"], action.stack, error)
            || !optionalBool(node, "每層", action.perStack, error)) return false;
        if (node["層數上限"])
        {
            int limit{};
            if (!requiredInt(node, "層數上限", limit, error)) return false;
            action.stackLimit = limit;
        }
        if (const auto scope = node["疊加範圍"])
        {
            const auto label = scope.as<std::string>();
            const auto parsed = parseLabel<EffectStackScope>(label, {
                { "共用", EffectStackScope::Shared },
                { "事件來源", EffectStackScope::EventSource },
            });
            if (!parsed)
            {
                error = std::format("未知疊加範圍「{}」", label);
                return false;
            }
            action.stackScope = *parsed;
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "傷害修正")
    {
        if (!validateKnownKeys(node, { "類型", "方位", "階段", "傷害種類", "方式", "數值", "持續幀數", "合併方式", "層數上限", "疊加範圍" }, error)) return false;
        ModifyDamageAction action;
        std::string stage;
        std::string channel;
        std::string operation;
        if (const auto perspective = node["方位"])
        {
            const auto label = perspective.as<std::string>();
            const auto parsed = parseLabel<DamageModifierPerspective>(label, {
                { "造成", DamageModifierPerspective::Outgoing },
                { "承受", DamageModifierPerspective::Incoming },
            });
            if (!parsed)
            {
                error = std::format("未知傷害修正方位「{}」", label);
                return false;
            }
            action.perspective = *parsed;
        }
        if (!requiredString(node, "階段", stage, error)
            || !requiredString(node, "傷害種類", channel, error)
            || !requiredString(node, "方式", operation, error)
            || !parseEffectNumberNode(node["數值"], action.amount, error)) return false;
        const auto parsedStage = parseLabel<DamageModifierStage>(stage, {
            { "防禦前", DamageModifierStage::BeforeDefense },
            { "防禦後", DamageModifierStage::AfterDefense },
            { "最終", DamageModifierStage::Final },
        });
        const auto parsedOperation = parseLabel<DamageModifierOperation>(operation, {
            { "固定加算", DamageModifierOperation::FlatAdd },
            { "百分比加算", DamageModifierOperation::PercentAdd },
            { "乘算", DamageModifierOperation::Multiply },
            { "忽略防禦百分比", DamageModifierOperation::IgnoreDefensePercent },
            { "單次承傷上限", DamageModifierOperation::CapSingleHitAtMaxHpPercent },
            { "低於最大生命百分比時處決", DamageModifierOperation::ExecuteBelowMaxHpPercent },
        });
        if (!parsedStage || !parseDamageChannel(channel, action.channel, error) || !parsedOperation)
        {
            if (error.empty()) error = "未知傷害修正階段或方式";
            return false;
        }
        action.stage = *parsedStage;
        action.operation = *parsedOperation;
        if (!optionalInt(node, "持續幀數", action.durationFrames, error)
            || !parseStackPolicy(node["合併方式"], action.stack, error)) return false;
        if (node["層數上限"])
        {
            int limit{};
            if (!requiredInt(node, "層數上限", limit, error)) return false;
            action.stackLimit = limit;
        }
        if (const auto scope = node["疊加範圍"])
        {
            const auto label = scope.as<std::string>();
            const auto parsed = parseLabel<EffectStackScope>(label, {
                { "共用", EffectStackScope::Shared },
                { "事件來源", EffectStackScope::EventSource },
            });
            if (!parsed)
            {
                error = std::format("未知疊加範圍「{}」", label);
                return false;
            }
            action.stackScope = *parsed;
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "資源變更")
    {
        if (!validateKnownKeys(node, {
                "類型", "資源", "方式", "數值", "轉移目標",
                "治療種類", "來源政策" }, error)) return false;
        ChangeResourceAction action;
        std::string resource;
        std::string kind;
        if (!requiredString(node, "資源", resource, error)
            || !requiredString(node, "方式", kind, error)
            || !parseEffectNumberNode(node["數值"], action.amount, error)) return false;
        const auto parsedResource = parseLabel<BattleResource>(resource, {
            { "生命", BattleResource::Hp }, { "內力", BattleResource::Mp },
            { "護盾", BattleResource::Shield }, { "狀態護盾", BattleResource::StatusShield },
            { "僵直護盾", BattleResource::StaggerShield },
            { "目前冷卻", BattleResource::ActiveCooldown },
            { "僵直吸收幀數", BattleResource::ControlImmunityFrames },
            { "無敵幀數", BattleResource::InvincibilityFrames },
        });
        const auto parsedKind = parseLabel<ResourceChangeKind>(kind, {
            { "回復", ResourceChangeKind::Restore }, { "奪取", ResourceChangeKind::Drain },
            { "獲得", ResourceChangeKind::Grant }, { "移除", ResourceChangeKind::Remove },
            { "轉移", ResourceChangeKind::Transfer },
            { "至少刷新至", ResourceChangeKind::RefreshToAtLeast },
        });
        if (!parsedResource || !parsedKind)
        {
            error = "未知資源或資源變更方式";
            return false;
        }
        action.resource = *parsedResource;
        action.kind = *parsedKind;
        if (node["轉移目標"])
        {
            EffectSelector selector;
            if (!parseSelectorNode(node["轉移目標"], selector, error)) return false;
            action.transferDestination = std::move(selector);
        }
        if (const auto healKind = node["治療種類"])
        {
            const auto label = healKind.as<std::string>();
            const auto parsed = parseLabel<EffectHealKind>(label, {
                { "直接", EffectHealKind::Direct },
                { "隊伍", EffectHealKind::Team },
                { "光環", EffectHealKind::Aura },
                { "命中", EffectHealKind::OnHit },
                { "擊殺獎勵", EffectHealKind::KillReward },
                { "死亡醫療", EffectHealKind::DeathMedical },
                { "救援", EffectHealKind::Rescue },
                { "生命回復", EffectHealKind::Regeneration },
                { "吸血", EffectHealKind::Lifesteal },
            });
            if (!parsed)
            {
                error = std::format("未知治療種類「{}」", label);
                return false;
            }
            action.healKind = *parsed;
        }
        if (const auto sourcePolicy = node["來源政策"])
        {
            const auto label = sourcePolicy.as<std::string>();
            if (label == "來源必須存活")
                action.healSourcePolicy = EffectHealSourcePolicy::RequireAlive;
            else if (label == "允許死亡來源")
                action.healSourcePolicy = EffectHealSourcePolicy::AllowDead;
            else
            {
                error = std::format("未知治療來源政策「{}」", label);
                return false;
            }
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "治療交易修正")
    {
        if (!validateKnownKeys(node, { "類型", "方式", "百分比", "治療種類" }, error)) return false;
        ModifyHealTransactionAction action;
        std::string operation;
        if (!requiredString(node, "方式", operation, error)) return false;
        if (operation == "阻止") action.operation = HealModifierOperation::Block;
        else if (operation == "受到治療乘算") action.operation = HealModifierOperation::MultiplyReceived;
        else
        {
            error = std::format("未知治療修正方式「{}」", operation);
            return false;
        }
        if (action.operation == HealModifierOperation::Block && node["百分比"])
        {
            error = "阻止治療不可填寫「百分比」";
            return false;
        }
        if (action.operation == HealModifierOperation::MultiplyReceived && !node["百分比"])
        {
            error = "受到治療乘算需要「百分比」";
            return false;
        }
        if (!optionalInt(node, "百分比", action.percent, error)
            || action.percent < 0 || action.percent > 100)
        {
            if (error.empty()) error = "治療乘算百分比必須介於 0 與 100";
            return false;
        }
        const auto kinds = node["治療種類"];
        if (!kinds || !kinds.IsSequence() || kinds.size() == 0)
        {
            error = "治療種類必須是非空列表";
            return false;
        }
        for (const auto& kind : kinds) action.kinds.push_back(kind.as<std::string>());
        out.value = std::move(action);
        return true;
    }
    if (type == "套用狀態")
    {
        if (!validateKnownKeys(node, { "類型", "狀態", "持續幀數", "套用次數", "層數", "強度", "次要強度", "合併方式", "層數上限", "同事件合計強度" }, error)) return false;
        ApplyStatusAction action;
        std::string status;
        if (!requiredString(node, "狀態", status, error)
            || !parseStatusKind(status, action.status, error)
            || !optionalInt(node, "層數", action.stacks, error)
            || !parseStackPolicy(node["合併方式"], action.stack, error)
            || !optionalBool(node, "同事件合計強度", action.aggregatePotencyWithinEvent, error)) return false;
        if (const auto duration = node["持續幀數"])
        {
            if (duration.IsScalar())
            {
                if (!requiredInt(node, "持續幀數", action.durationFrames, error)) return false;
            }
            else
            {
                EffectNumber formula;
                if (!parseEffectNumberNode(duration, formula, error)) return false;
                action.duration = std::move(formula);
            }
        }
        if (node["套用次數"])
        {
            EffectNumber count;
            if (!parseEffectNumberNode(node["套用次數"], count, error)) return false;
            action.applicationCount = std::move(count);
        }
        if (node["強度"] && !parseEffectNumberNode(node["強度"], action.potency, error)) return false;
        if (node["次要強度"] && !parseEffectNumberNode(node["次要強度"], action.secondaryPotency, error)) return false;
        if (node["層數上限"])
        {
            int limit{};
            if (!requiredInt(node, "層數上限", limit, error)) return false;
            action.stackLimit = limit;
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "消耗狀態")
    {
        if (!validateKnownKeys(node, { "類型", "狀態", "層數", "狀態來源", "最後一層" }, error)) return false;
        ConsumeStatusAction action;
        std::string status;
        if (!requiredString(node, "狀態", status, error)
            || !parseStatusKind(status, action.status, error)
            || !optionalInt(node, "層數", action.stacks, error)) return false;
        if (const auto source = node["狀態來源"])
        {
            const auto label = source.as<std::string>();
            if (label == "不限") action.source = StatusSourceMatch::Any;
            else if (label == "效果擁有者") action.source = StatusSourceMatch::EffectOwner;
            else
            {
                error = std::format("未知狀態來源「{}」", label);
                return false;
            }
        }
        if (const auto depleted = node["最後一層"])
        {
            EffectAction nested;
            if (!parseActionNode(depleted, nested, error)) return false;
            const auto* statusAction = std::get_if<ApplyStatusAction>(&nested.value);
            if (!statusAction)
            {
                error = "消耗最後一層目前只允許套用狀態";
                return false;
            }
            action.whenDepleted = *statusAction;
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "移除狀態")
    {
        if (!validateKnownKeys(node, { "類型", "狀態", "僅負面", "僅控制", "解除目前動作僵直", "數量", "順序" }, error)) return false;
        RemoveStatusAction action;
        if (!optionalBool(node, "僅負面", action.negativeOnly, error)
            || !optionalBool(node, "僅控制", action.controlOnly, error)
            || !optionalBool(node, "解除目前動作僵直", action.clearCurrentActionStagger, error)
            || !optionalInt(node, "數量", action.count, error)) return false;
        if (const auto statuses = node["狀態"])
        {
            const auto append = [&](const YAML::Node& item)
            {
                BattleStatusKind status{};
                if (!parseStatusKind(item.as<std::string>(), status, error)) return false;
                action.statuses.push_back(status);
                return true;
            };
            if (statuses.IsSequence())
            {
                for (const auto& status : statuses) if (!append(status)) return false;
            }
            else if (!append(statuses)) return false;
        }
        if (const auto order = node["順序"])
        {
            const auto label = order.as<std::string>();
            const auto parsed = parseLabel<StatusRemovalOrder>(label, {
                { "最長剩餘", StatusRemovalOrder::LongestRemaining },
                { "最舊", StatusRemovalOrder::Oldest },
                { "最新", StatusRemovalOrder::Newest },
            });
            if (!parsed)
            {
                error = std::format("未知狀態移除順序「{}」", label);
                return false;
            }
            action.order = *parsed;
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "造成傷害")
    {
        if (!validateKnownKeys(node, {
                "類型", "數值", "交易次數", "傷害種類", "範圍", "半徑格數",
                "方形邊長", "同目標命中上限", "套用傷害修正", "觸發受傷無敵",
                "區域投射物" }, error)) return false;
        DealDamageAction action;
        std::string kind;
        if (!requiredString(node, "傷害種類", kind, error)
            || !parseEffectNumberNode(node["數值"], action.amount, error)
            || !parseDamageKindLabel(kind, action.kind, error)) return false;
        if (node["交易次數"])
        {
            EffectNumber count;
            if (!parseEffectNumberNode(node["交易次數"], count, error)) return false;
            action.transactionCount = std::move(count);
        }
        if (const auto area = node["範圍"])
        {
            const auto label = area.as<std::string>();
            const auto parsed = parseLabel<DamageAreaKind>(label, {
                { "單體", DamageAreaKind::SingleTarget },
                { "圓形", DamageAreaKind::Circle },
                { "方形", DamageAreaKind::Square },
            });
            if (!parsed)
            {
                error = std::format("未知傷害範圍「{}」", label);
                return false;
            }
            action.area.kind = *parsed;
        }
        if (!optionalInt(node, "半徑格數", action.area.radiusTiles, error)
            || !optionalInt(node, "方形邊長", action.area.squareSideTiles, error)
            || !optionalInt(node, "同目標命中上限", action.perCast.perTargetLimit, error)
            || !optionalBool(node, "套用傷害修正", action.appliesDamageModifiers, error)
            || !optionalBool(node, "觸發受傷無敵", action.triggersHurtInvincibility, error)) return false;
        if (const auto projectileNode = node["區域投射物"])
        {
            if (!validateKnownKeys(projectileNode, {
                    "範圍格數", "最多目標", "眩暈幀數", "追蹤事件來源", "特效" }, error)) return false;
            AreaProjectileDamageDelivery delivery;
            std::string visual;
            if (!requiredInt(projectileNode, "範圍格數", delivery.rangeTiles, error)
                || !requiredInt(projectileNode, "最多目標", delivery.maximumTargets, error)
                || !requiredInt(projectileNode, "眩暈幀數", delivery.stunFrames, error)
                || !optionalBool(projectileNode, "追蹤事件來源", delivery.trackEventSource, error)
                || !requiredString(projectileNode, "特效", visual, error)) return false;
            const auto parsedVisual = parseLabel<AreaProjectileVisual>(visual, {
                { "死亡爆炸", AreaProjectileVisual::DeathBlast },
                { "護盾爆炸", AreaProjectileVisual::ShieldBlast },
            });
            if (!parsedVisual)
            {
                error = std::format("未知區域投射物特效「{}」", visual);
                return false;
            }
            delivery.visual = *parsedVisual;
            action.areaProjectiles = delivery;
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "修改攻擊")
    {
        if (!validateKnownKeys(node, {
                "類型", "樣式", "數量", "展開角度", "間隔幀數", "傷害倍率", "貫穿",
                "追蹤", "視為主彈道", "同目標命中上限", "目標政策", "傳播政策", "追加至基礎攻擊",
                "攻擊來源", "傷害數值", "傷害種類", "執行行為" }, error)) return false;
        ModifyAttackAction action;
        if (!parseAttackPatternFields(node, action.pattern, error)
            || !optionalInt(node, "傷害倍率", action.strengthPct, error)
            || !optionalBool(node, "貫穿", action.through, error)
            || !optionalBool(node, "追蹤", action.tracking, error)
            || !optionalBool(node, "視為主彈道", action.mainProjectile, error)
            || !optionalInt(node, "同目標命中上限", action.sameTargetHitLimit, error)
            || !optionalBool(node, "追加至基礎攻擊", action.addToBaseAttack, error)
            || !parsePropagation(node["傳播政策"], action.propagation, error)) return false;
        if (node["攻擊來源"])
        {
            EffectSelector selector;
            if (!parseSelectorNode(node["攻擊來源"], selector, error)) return false;
            action.source = std::move(selector);
        }
        if (node["傷害數值"])
        {
            EffectNumber number;
            if (!parseEffectNumberNode(node["傷害數值"], number, error)) return false;
            action.damageOverride = number;
        }
        if (const auto kind = node["傷害種類"])
        {
            const auto label = kind.as<std::string>();
            BattleDamageKind parsed{};
            if (!parseDamageKindLabel(label, parsed, error)) return false;
            action.damageKind = parsed;
        }
        if (const auto policy = node["目標政策"])
        {
            const auto label = policy.as<std::string>();
            const auto parsed = parseLabel<AttackTargetPolicy>(label, {
                { "保留", AttackTargetPolicy::Preserve }, { "選擇目標", AttackTargetPolicy::SelectedTargets },
                { "同落點", AttackTargetPolicy::SamePoint }, { "同目標", AttackTargetPolicy::SameTarget },
            });
            if (!parsed)
            {
                error = std::format("未知攻擊目標政策「{}」", label);
                return false;
            }
            action.targets = *parsed;
        }
        if (node["執行行為"]
            && !parseAttackRuntimeBehavior(node["執行行為"], action.runtimeBehavior, error)) return false;
        out.value = std::move(action);
        return true;
    }
    if (type == "強制移動")
    {
        if (!validateKnownKeys(node, {
                "類型", "方向", "距離格數", "距離像素", "鎖定幀數", "碰撞", "受阻結果" }, error)) return false;
        ForceMoveAction action;
        std::string direction;
        std::string collision;
        std::string blocked;
        if (!requiredString(node, "方向", direction, error)
            || !optionalInt(node, "距離格數", action.distanceTiles, error)
            || !optionalInt(node, "距離像素", action.distancePixels, error)
            || !optionalInt(node, "鎖定幀數", action.lockFrames, error)
            || !requiredString(node, "碰撞", collision, error)
            || !requiredString(node, "受阻結果", blocked, error)) return false;
        const auto parsedDirection = parseLabel<ForceMoveDirection>(direction, {
            { "遠離來源", ForceMoveDirection::AwayFromSource },
            { "接近來源", ForceMoveDirection::TowardSource },
        });
        const auto parsedCollision = parseLabel<ForceMoveCollision>(collision, {
            { "佔位前停止", ForceMoveCollision::StopBeforeOccupied },
            { "阻擋前停止", ForceMoveCollision::StopBeforeBlocked },
            { "遇阻停止", ForceMoveCollision::StopBeforeBlocked },
        });
        const auto parsedBlocked = parseLabel<ForceMoveBlockedResult>(blocked, {
            { "停止", ForceMoveBlockedResult::Stop },
            { "縮短", ForceMoveBlockedResult::Shorten },
        });
        if (!parsedDirection)
        {
            error = std::format("未知強制移動方向「{}」", direction);
            return false;
        }
        if (!parsedCollision)
        {
            error = std::format("未知強制移動碰撞方式「{}」", collision);
            return false;
        }
        if (!parsedBlocked)
        {
            error = std::format("未知強制移動受阻結果「{}」", blocked);
            return false;
        }
        action.direction = *parsedDirection;
        action.collision = *parsedCollision;
        action.blocked = *parsedBlocked;
        out.value = std::move(action);
        return true;
    }
    if (type == "建立區域")
    {
        if (!validateKnownKeys(node, { "類型", "形狀", "半徑格數", "方形邊長", "錨點", "持續幀數", "來源死亡", "合併方式", "區域修正" }, error)) return false;
        CreateAreaAction action;
        std::string shape;
        std::string anchor;
        std::string death;
        std::string merge;
        if (!requiredString(node, "形狀", shape, error)
            || !requiredString(node, "錨點", anchor, error)
            || !requiredString(node, "來源死亡", death, error)
            || !requiredString(node, "合併方式", merge, error)
            || !requiredInt(node, "持續幀數", action.durationFrames, error)
            || !optionalInt(node, "半徑格數", action.radiusTiles, error)
            || !optionalInt(node, "方形邊長", action.squareSideTiles, error)) return false;
        const auto parsedShape = parseLabel<AreaShape>(shape, {
            { "圓形", AreaShape::Circle }, { "棋格方形", AreaShape::GridSquare },
        });
        const auto parsedAnchor = parseLabel<AreaAnchor>(anchor, {
            { "命中位置", AreaAnchor::HitPosition }, { "跟隨來源", AreaAnchor::FollowSourceUnit },
        });
        const auto parsedDeath = parseLabel<AreaSourceDeathPolicy>(death, {
            { "保留至到期", AreaSourceDeathPolicy::PersistUntilExpiry },
            { "立即移除", AreaSourceDeathPolicy::RemoveImmediately },
        });
        const auto parsedMerge = parseLabel<AreaMergePolicy>(merge, {
            { "獨立", AreaMergePolicy::Independent },
            { "同來源刷新", AreaMergePolicy::RefreshSameSource },
            { "同來源取代", AreaMergePolicy::ReplaceSameSource },
        });
        if (!parsedShape || !parsedAnchor || !parsedDeath || !parsedMerge)
        {
            error = "未知區域形狀、錨點、死亡或合併政策";
            return false;
        }
        action.shape = *parsedShape;
        action.anchor = *parsedAnchor;
        action.sourceDeath = *parsedDeath;
        action.merge = *parsedMerge;
        const auto modifiers = node["區域修正"];
        if (!modifiers || !modifiers.IsSequence() || modifiers.size() == 0)
        {
            error = "建立區域需要非空區域修正列表";
            return false;
        }
        for (const auto& modifierNode : modifiers)
        {
            AreaModifier modifier;
            if (!parseAreaModifierNode(modifierNode, modifier, error)) return false;
            action.modifiers.push_back(std::move(modifier));
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "修改施放")
    {
        if (!validateKnownKeys(node, {
                "類型", "內力消耗", "射程模式", "彈道速度百分比", "最小選擇距離",
                "追加彈道數", "機動政策", "自動絕招", "免費追加施放", "傳播政策",
                "樣式", "數量", "展開角度", "間隔幀數" }, error)) return false;
        ModifyCastAction action;
        if (node["內力消耗"])
        {
            EffectNumber number;
            if (!parseEffectNumberNode(node["內力消耗"], number, error)) return false;
            action.mpCost = number;
        }
        if (const auto range = node["射程模式"])
        {
            const auto label = range.as<std::string>();
            if (label == "保留") action.rangeMode = CastRangeMode::Preserve;
            else if (label == "遠程") action.rangeMode = CastRangeMode::Ranged;
            else
            {
                error = std::format("未知射程模式「{}」", label);
                return false;
            }
        }
        if (const auto mobility = node["機動政策"])
        {
            const auto label = mobility.as<std::string>();
            const auto parsed = parseLabel<CastMobilityPolicy>(label, {
                { "保留", CastMobilityPolicy::Preserve },
                { "滑步攻擊", CastMobilityPolicy::DashAttack },
                { "閃擊", CastMobilityPolicy::BlinkAttack },
            });
            if (!parsed)
            {
                error = std::format("未知施放機動政策「{}」", label);
                return false;
            }
            action.mobility = *parsed;
        }
        if (const auto autoUltimate = node["自動絕招"])
        {
            if (!validateKnownKeys(autoUltimate, { "消耗內力", "顯示公告" }, error)) return false;
            AutoUltimateCastRequest request;
            if (!optionalBool(autoUltimate, "消耗內力", request.consumeMp, error)
                || !optionalBool(autoUltimate, "顯示公告", request.announce, error)) return false;
            action.autoUltimate = request;
        }
        if (!optionalInt(node, "彈道速度百分比", action.projectileSpeedPct, error)
            || !optionalInt(node, "最小選擇距離", action.minimumSelectDistance, error)
            || !optionalInt(node, "追加彈道數", action.additionalProjectiles, error)
            || !optionalBool(node, "免費追加施放", action.freeAdditionalCast, error)
            || !parsePropagation(node["傳播政策"], action.propagation, error)) return false;
        if (node["樣式"] || node["數量"] || node["展開角度"] || node["間隔幀數"])
        {
            AttackPattern pattern;
            if (!parseAttackPatternFields(node, pattern, error)) return false;
            action.replacementPattern = pattern;
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "狀態機")
    {
        std::string mechanism;
        if (!requiredString(node, "機制", mechanism, error)) return false;
        if (mechanism == "變更狀態值")
        {
            if (!validateKnownKeys(node, { "類型", "機制", "狀態槽", "增量", "最小", "最大" }, error)) return false;
            ChangeStateValueAction action;
            if (!parseEffectStateSlot(node["狀態槽"], action.slot, error)
                || !requiredInt(node, "增量", action.delta, error)) return false;
            if (node["最小"])
            {
                int value{};
                if (!requiredInt(node, "最小", value, error)) return false;
                action.minimum = value;
            }
            if (node["最大"])
            {
                int value{};
                if (!requiredInt(node, "最大", value, error)) return false;
                action.maximum = value;
            }
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == "轉移狀態值")
        {
            if (!validateKnownKeys(
                    node,
                    { "類型", "機制", "來源狀態槽", "目標狀態槽" },
                    error)) return false;
            TransferStateValueAction action;
            if (!parseEffectStateSlot(
                    node["來源狀態槽"],
                    action.sourceSlot,
                    error)
                || !parseEffectStateSlot(
                    node["目標狀態槽"],
                    action.destinationSlot,
                    error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == "記錄最大招式生命傷害")
        {
            if (!validateKnownKeys(node, { "類型", "機制", "狀態槽" }, error)) return false;
            RecordMaximumDamageAction action;
            if (!parseEffectStateSlot(node["狀態槽"], action.slot, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == "消耗記錄為傷害" || mechanism == "消耗記錄為護盾")
        {
            if (!validateKnownKeys(
                    node,
                    { "類型", "機制", "狀態槽", "百分比", "消耗後清除" },
                    error)) return false;
            ConsumeRecordedMaximumAction action;
            action.destination = mechanism.ends_with("護盾") ? StateValueDestination::ShieldAmount : StateValueDestination::DamageAmount;
            if (!parseEffectStateSlot(node["狀態槽"], action.slot, error)
                || !optionalInt(node, "百分比", action.percent, error)
                || !optionalBool(node, "消耗後清除", action.clearAfterConsume, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == "開始傷害吸收")
        {
            if (!validateKnownKeys(node, {
                    "類型", "機制", "狀態槽", "百分比", "持續幀數", "死亡結算",
                    "結算目標", "結算傷害種類", "結算百分比" }, error)) return false;
            StartDamageAbsorptionAction action;
            std::string settlementDamageKind;
            if (!parseEffectStateSlot(node["狀態槽"], action.slot, error)
                || !requiredInt(node, "百分比", action.absorbedPct, error)
                || !requiredInt(node, "持續幀數", action.durationFrames, error)
                || !optionalBool(node, "死亡結算", action.settleOnSourceDeath, error)
                || !parseSelectorNode(node["結算目標"], action.settlementTarget, error)
                || !requiredString(node, "結算傷害種類", settlementDamageKind, error)
                || !parseDamageKindLabel(settlementDamageKind, action.settlementDamageKind, error)
                || !requiredInt(node, "結算百分比", action.returnedPct, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == "結算傷害吸收")
        {
            if (!validateKnownKeys(node, { "類型", "機制", "狀態槽", "百分比", "目標" }, error)) return false;
            SettleDamageAbsorptionAction action;
            if (!parseEffectStateSlot(node["狀態槽"], action.slot, error)
                || !optionalInt(node, "百分比", action.returnedPct, error)) return false;
            if (node["目標"] && !parseSelectorNode(node["目標"], action.target, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == "借用效果規則")
        {
            if (!validateKnownKeys(node, {
                    "類型", "機制", "目標", "來源數量", "允許動作類別", "傳播政策" }, error)) return false;
            BorrowEffectRulesAction action;
            if (!parseSelectorNode(node["目標"], action.sourceUnits, error)
                || !parseEffectNumberNode(node["來源數量"], action.sourceCount, error)
                || !parseBorrowedRuleFilter(node["允許動作類別"], action.filter, error)
                || !parsePropagation(node["傳播政策"], action.propagation, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == "複製攻擊定義")
        {
            if (!validateKnownKeys(node, {
                    "類型", "機制", "目標", "來源數量", "可選武功條件", "傳播政策" }, error)) return false;
            CopyAttackDefinitionAction action;
            if (!parseSelectorNode(node["目標"], action.sourceUnits, error)
                || !parseCopiedMagicFilter(node["可選武功條件"], action.filter, error)
                || !optionalInt(node, "來源數量", action.copyCount, error)
                || !parsePropagation(node["傳播政策"], action.propagation, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == "結算剩餘狀態傷害")
        {
            if (!validateKnownKeys(node, { "類型", "機制", "狀態" }, error)) return false;
            SettleRemainingStatusDamageAction action;
            std::string status;
            if (!requiredString(node, "狀態", status, error)
                || !parseStatusKind(status, action.status, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == "生成分身")
        {
            if (!validateKnownKeys(node, { "類型", "機制", "數量" }, error)) return false;
            GenerateClonesAction action;
            if (!requiredInt(node, "數量", action.count, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == "死亡庇護")
        {
            if (!validateKnownKeys(node, { "類型", "機制", "無敵幀數" }, error)) return false;
            PreventDeathAction action;
            if (!requiredInt(node, "無敵幀數", action.invincibilityFrames, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == "保護挪移" || mechanism == "處決挪移")
        {
            if (!validateKnownKeys(node, { "類型", "機制", "次數" }, error)) return false;
            ConfigureRescueRepositionAction action;
            action.mode = mechanism == "保護挪移"
                ? RescueRepositionMode::Protect
                : RescueRepositionMode::Execute;
            if (!requiredInt(node, "次數", action.activations, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else
        {
            error = std::format("未知狀態機機制「{}」", mechanism);
            return false;
        }
        return true;
    }
    if (type == "條件分支")
    {
        if (!validateKnownKeys(node, { "類型", "條件", "成立", "否則" }, error)) return false;
        const auto conditions = node["條件"];
        if (!conditions || !conditions.IsSequence() || conditions.size() == 0)
        {
            error = "條件分支需要非空條件列表";
            return false;
        }
        auto conditional = std::make_shared<ConditionalEffectAction>();
        for (const auto& conditionNode : conditions)
        {
            EffectCondition condition;
            if (!parseConditionNode(conditionNode, condition, error)) return false;
            conditional->conditions.push_back(std::move(condition));
        }
        if (!parseActionList(node["成立"], conditional->whenTrue, error)) return false;
        if (node["否則"] && !parseActionList(node["否則"], conditional->whenFalse, error)) return false;
        out.value = std::move(conditional);
        return true;
    }

    error = std::format("未知動作類型「{}」", type);
    return false;
}

}  // namespace

namespace
{

// The description pipeline deliberately keeps the parsed effect payload typed
// until the final rendering pass.  These nodes are private because the only
// public description API is effectDescription(EffectRule, style).
struct EffectDescriptionNode;
using EffectDescriptionChild = std::unique_ptr<EffectDescriptionNode>;

enum class DescriptionSubjectId
{
    None,
    RuleEvent,
    SelectedTargets,
    RecordedState,
};

struct DescriptionSubject
{
    DescriptionSubjectId id = DescriptionSubjectId::None;
};

enum class DescriptionPredicate
{
    ModifyAttribute = 0,
    ModifyDamage,
    ChangeResource,
    ModifyHealTransaction,
    ApplyStatus,
    ConsumeStatus,
    RemoveStatus,
    DealDamage,
    ModifyAttack,
    ForceMove,
    CreateArea,
    ModifyCast,
    StateMachine,
    Conditional,
    Trigger,
    RuleLimits,
    ResetRecordedState,
    RecordDamageAbsorption,
    SettleDamageAbsorption,
};

static_assert(
    static_cast<std::size_t>(DescriptionPredicate::Conditional) + 1
    == std::variant_size_v<EffectActionValue>);

struct DescriptionActionArgument
{
    EffectActionValue value;
};

struct DescriptionTriggerArgument
{
    EffectEvent event{};
    EffectObservationScope observation = EffectObservationScope::Owner;
    EffectCastMatch castMatch = EffectCastMatch::BoundMagic;
};

struct DescriptionResetStateArgument
{
    EffectStateSlot slot{};
};

struct DescriptionDamageAbsorptionRecordArgument
{
    int absorbedPct{};
    int durationFrames{};
};

struct DescriptionDamageAbsorptionSettlementArgument
{
    EffectSelector target;
    BattleDamageKind damageKind = BattleDamageKind::Pure;
    int returnedPct{};
    bool settleOnSourceDeath{};
};

using DescriptionArgument = std::variant<
    DescriptionActionArgument,
    DescriptionTriggerArgument,
    DescriptionResetStateArgument,
    DescriptionDamageAbsorptionRecordArgument,
    DescriptionDamageAbsorptionSettlementArgument>;

struct DescriptionChanceQualifier
{
    int percent{};
    auto operator<=>(const DescriptionChanceQualifier&) const = default;
};

struct DescriptionDurationFramesQualifier
{
    int frames{};
    auto operator<=>(const DescriptionDurationFramesQualifier&) const = default;
};

struct DescriptionStackPolicyQualifier
{
    EffectStackPolicy policy{};
    auto operator<=>(const DescriptionStackPolicyQualifier&) const = default;
};

struct DescriptionStackLimitQualifier
{
    int count{};
    auto operator<=>(const DescriptionStackLimitQualifier&) const = default;
};

struct DescriptionPerStackQualifier
{
    bool enabled{};
    auto operator<=>(const DescriptionPerStackQualifier&) const = default;
};

struct DescriptionStackScopeQualifier
{
    EffectStackScope scope{};
    auto operator<=>(const DescriptionStackScopeQualifier&) const = default;
};

struct DescriptionMaxActivationsQualifier
{
    int count{};
    auto operator<=>(const DescriptionMaxActivationsQualifier&) const = default;
};

struct DescriptionSharedCooldownQualifier
{
    int frames{};
    auto operator<=>(const DescriptionSharedCooldownQualifier&) const = default;
};

struct DescriptionIntervalQualifier
{
    int frames{};
    auto operator<=>(const DescriptionIntervalQualifier&) const = default;
};

struct DescriptionEveryNthEventQualifier
{
    int count{};
    auto operator<=>(const DescriptionEveryNthEventQualifier&) const = default;
};

struct DescriptionRepetitionCountQualifier
{
    EffectNumber count;

    bool operator==(const DescriptionRepetitionCountQualifier& other) const
    {
        return count.base == other.count.base
            && count.multiplierBase == other.count.multiplierBase
            && count.status == other.count.status
            && count.stateSlot == other.count.stateSlot
            && count.flat == other.count.flat
            && count.percent == other.count.percent
            && count.rounding == other.count.rounding
            && count.minimum == other.count.minimum
            && count.maximum == other.count.maximum;
    }
};

struct DescriptionActivationLimitQualifier
{
    EffectActivationScope scope{};
    int count{};
    auto operator<=>(const DescriptionActivationLimitQualifier&) const = default;
};

using DescriptionQualifier = std::variant<
    DescriptionChanceQualifier,
    DescriptionDurationFramesQualifier,
    DescriptionStackPolicyQualifier,
    DescriptionStackLimitQualifier,
    DescriptionPerStackQualifier,
    DescriptionStackScopeQualifier,
    DescriptionMaxActivationsQualifier,
    DescriptionSharedCooldownQualifier,
    DescriptionIntervalQualifier,
    DescriptionEveryNthEventQualifier,
    DescriptionRepetitionCountQualifier,
    DescriptionActivationLimitQualifier>;

struct DescriptionClause
{
    DescriptionSubject subject{};
    DescriptionPredicate predicate{};
    std::vector<DescriptionArgument> arguments;
    std::vector<DescriptionQualifier> qualifiers;
};

enum class DescriptionSequenceKind
{
    Rule,
    Actions,
};

struct DescriptionSequence
{
    DescriptionSequenceKind kind = DescriptionSequenceKind::Actions;
    std::vector<EffectDescriptionChild> steps;
};

struct DescriptionSimultaneous
{
    std::vector<EffectDescriptionChild> actions;
    std::vector<DescriptionQualifier> qualifiers;
};

struct DescriptionStatusDepletedCondition
{
    BattleStatusKind status{};
};

using DescriptionConditionValue = std::variant<
    EffectCondition,
    DescriptionStatusDepletedCondition>;

struct DescriptionCondition
{
    DescriptionConditionValue value;
};

enum class DescriptionConditionalScope
{
    Rule,
    PerTarget,
};

struct DescriptionConditional
{
    DescriptionConditionalScope scope = DescriptionConditionalScope::PerTarget;
    std::vector<DescriptionCondition> conditions;
    std::vector<DescriptionQualifier> qualifiers;
    EffectDescriptionChild whenTrue;
    EffectDescriptionChild whenFalse;
};

struct DescriptionTargetSet
{
    EffectSelector selector;
};

struct DescriptionForEach
{
    DescriptionTargetSet targets{};
    EffectDescriptionChild body;
};

struct DescriptionStateCycle
{
    EffectDescriptionChild record;
    EffectDescriptionChild consume;
    EffectDescriptionChild reset;
    std::optional<int> initialValue;
};

struct EffectDescriptionNode
{
    std::variant<
        DescriptionClause,
        DescriptionSequence,
        DescriptionSimultaneous,
        DescriptionConditional,
        DescriptionForEach,
        DescriptionStateCycle> value;

    template<typename T>
    explicit EffectDescriptionNode(T node)
        : value(std::move(node))
    {
    }
};

template<typename T>
EffectDescriptionChild makeDescriptionNode(T node)
{
    return std::make_unique<EffectDescriptionNode>(std::move(node));
}

std::string ruleEventLabel(EffectEvent event, bool compact)
{
    switch (event)
    {
    case EffectEvent::BattleInitialized: return compact ? "開戰" : "戰鬥開始時";
    case EffectEvent::FrameAdvanced: return compact ? "每幀" : "每幀";
    case EffectEvent::UltimateCooldownFinished: return compact ? "絕招冷卻完成" : "絕招冷卻完成時";
    case EffectEvent::CastPlanned: return compact ? "施放規劃" : "規劃施放時";
    case EffectEvent::AttackCommitted: return compact ? "出手" : "出手時";
    case EffectEvent::UltimateCommitted: return compact ? "絕招" : "施放絕招時";
    case EffectEvent::AttackSpawned: return compact ? "彈道生成" : "攻擊生成時";
    case EffectEvent::MainProjectileBeforeDamage: return compact ? "主彈命中" : "絕招主彈道命中、傷害結算前";
    case EffectEvent::HitBeforeDamage: return compact ? "命中前" : "命中且傷害結算前";
    case EffectEvent::DamageResolved: return compact ? "傷害後" : "傷害結算後";
    case EffectEvent::HealAttempted: return compact ? "治療前" : "嘗試治療時";
    case EffectEvent::HealApplied: return compact ? "治療後" : "實際治療後";
    case EffectEvent::CastContinuation: return compact ? "絕招延續" : "本次絕招第一輪攻擊完成後";
    case EffectEvent::CastSettled: return compact ? "絕招結算" : "本次絕招全部攻擊結算完成後";
    case EffectEvent::ShieldBroken: return compact ? "破盾" : "護盾破裂時";
    case EffectEvent::UnitDied: return compact ? "死亡" : "自身死亡時";
    case EffectEvent::AllyDied: return compact ? "友軍死亡" : "友軍死亡時";
    }
    assert(false);
    return {};
}

std::string selectorLabel(const EffectSelector& selector, bool compact)
{
    const auto countSuffix = selector.count > 0 ? std::format("{}人", selector.count) : std::string{};
    std::string result;
    switch (selector.kind)
    {
    case EffectSelectorKind::Self: result = "自身"; break;
    case EffectSelectorKind::SourceUnit: result = "來源單位"; break;
    case EffectSelectorKind::TransactionTarget: result = "交易目標"; break;
    case EffectSelectorKind::HitTarget: result = "命中目標"; break;
    case EffectSelectorKind::OriginalAttackTarget: result = "原攻擊目標"; break;
    case EffectSelectorKind::ComboMembers: result = "羈絆成員"; break;
    case EffectSelectorKind::AllLivingUnits:
        result = selector.count > 0 ? std::format("存活單位{}", countSuffix) : "所有存活單位";
        break;
    case EffectSelectorKind::Allies:
        result = selector.count > 0 ? std::format("友軍{}", countSuffix) : "全隊";
        break;
    case EffectSelectorKind::Enemies:
        result = selector.count > 0 ? std::format("敵軍{}", countSuffix) : "所有敵人";
        break;
    case EffectSelectorKind::LowestHpAllies:
        result = selector.count > 0 ? std::format("生命比例最低{}", countSuffix) : "生命比例最低的友軍";
        break;
    case EffectSelectorKind::LowestMpAllies:
        result = selector.count > 0 ? std::format("內力最低{}", countSuffix) : "內力最低的友軍";
        break;
    case EffectSelectorKind::HighestMpEnemy: result = "內力最高的敵人"; break;
    case EffectSelectorKind::StrongestEnemies:
        result = selector.count > 0
            ? std::format("最強{}名敵人", selector.count)
            : "最強的敵人";
        break;
    case EffectSelectorKind::NearestEnemies:
        result = selector.count > 0
            ? std::format("最近{}名敵人", selector.count)
            : "最近的敵人";
        break;
    case EffectSelectorKind::FarthestEnemy: result = "最遠的敵人"; break;
    case EffectSelectorKind::UnitsInRadius:
    {
        const auto relation = selector.team == EffectTeamFilter::Ally ? "友軍"
            : selector.team == EffectTeamFilter::Enemy ? "敵軍"
            : "單位";
        result = selector.count > 0
            ? std::format("{}格內至多{}名{}", selector.radiusTiles, selector.count, relation)
            : std::format("{}格內所有{}", selector.radiusTiles, relation);
        break;
    }
    case EffectSelectorKind::UnitsInSquare:
    {
        const auto relation = selector.team == EffectTeamFilter::Ally ? "友軍"
            : selector.team == EffectTeamFilter::Enemy ? "敵軍"
            : "單位";
        result = selector.count > 0
            ? std::format("{}×{}範圍內至多{}名{}",
                selector.squareSideTiles,
                selector.squareSideTiles,
                selector.count,
                relation)
            : std::format("{}×{}範圍內所有{}",
                selector.squareSideTiles,
                selector.squareSideTiles,
                relation);
        break;
    }
    case EffectSelectorKind::AlliesUsingWeapon:
        result = compact
            ? std::format("友方武器{}", selector.requiredWeaponType)
            : std::format("使用武器類型{}的友軍", selector.requiredWeaponType);
        break;
    }
    assert(!result.empty());
    if (selector.excludeOwner)
    {
        result += "（不含自身）";
    }
    if (selector.requiredMagicId >= 0)
    {
        result += std::format("（使用武功{}）", selector.requiredMagicId);
    }
    if (selector.requiredTarget)
    {
        switch (*selector.requiredTarget)
        {
        case EffectRequiredTarget::Self: result += "（必含自身）"; break;
        case EffectRequiredTarget::SourceUnit: result += "（必含來源單位）"; break;
        case EffectRequiredTarget::TransactionTarget: result += "（必含交易目標）"; break;
        case EffectRequiredTarget::HitTarget: result += "（必含命中目標）"; break;
        case EffectRequiredTarget::OriginalAttackTarget: result += "（必含原攻擊目標）"; break;
        }
    }
    if (selector.tieBreak == EffectTieBreak::BattleRandom)
    {
        if (selector.kind == EffectSelectorKind::Enemies && selector.count > 0)
            result = "隨機" + result;
        else
            result += "（隨機決定同順位）";
    }
    return result;
}

std::string_view damageKindLabel(BattleDamageKind kind)
{
    switch (kind)
    {
    case BattleDamageKind::Physical: return "物理";
    case BattleDamageKind::Skill: return "招式";
    case BattleDamageKind::Pure: return "純粹";
    case BattleDamageKind::Poison: return "中毒";
    case BattleDamageKind::Bleed: return "流血";
    case BattleDamageKind::Effect: return "特效";
    case BattleDamageKind::Reflected: return "反彈";
    case BattleDamageKind::Execute: return "處決";
    }
    assert(false);
    return {};
}

std::string numberLabel(const EffectNumber& number)
{
    std::string base;
    const bool attackScaledByMissingHp = number.base == EffectNumberBase::SourceAttack
        && number.multiplierBase == EffectNumberBase::SourceMissingHpRatio;
    if (attackScaledByMissingHp)
    {
        base = std::format("目前攻擊×已損生命比例×{}%", number.percent);
    }
    else switch (number.base)
    {
    case EffectNumberBase::Constant:
        break;
    case EffectNumberBase::SourceStar:
        base = number.percent % 100 == 0
            ? std::format("星級×{}", number.percent / 100)
            : std::format("星級×{}%", number.percent);
        break;
    case EffectNumberBase::SourceAttack: base = std::format("攻擊的{}%", number.percent); break;
    case EffectNumberBase::SourceMaxHp: base = std::format("自身{}%最大生命", number.percent); break;
    case EffectNumberBase::SourceMissingHpRatio: base = std::format("已損生命比例的{}%", number.percent); break;
    case EffectNumberBase::SourceCurrentMpRatio: base = std::format("目前內力比例的{}%", number.percent); break;
    case EffectNumberBase::TargetMaxHp: base = std::format("目標{}%最大生命", number.percent); break;
    case EffectNumberBase::TargetCurrentHp: base = std::format("目標目前生命的{}%", number.percent); break;
    case EffectNumberBase::TargetCurrentShield: base = std::format("目標目前護盾的{}%", number.percent); break;
    case EffectNumberBase::TargetCurrentCooldown: base = std::format("目標目前冷卻的{}%", number.percent); break;
    case EffectNumberBase::FinalHpDamage: base = std::format("實際生命傷害的{}%", number.percent); break;
    case EffectNumberBase::AccumulatedStateValue: base = std::format("累計值的{}%", number.percent); break;
    case EffectNumberBase::SourceStatusPotency: base = std::format("{}強度的{}%", number.status, number.percent); break;
    case EffectNumberBase::SourceStatusStacks: base = std::format("{}層數的{}%", number.status, number.percent); break;
    case EffectNumberBase::StoredStateValue: base = std::format("狀態槽值的{}%", number.percent); break;
    }

    if (number.multiplierBase && !attackScaledByMissingHp)
    {
        switch (*number.multiplierBase)
        {
        case EffectNumberBase::Constant: base += "×固定基準"; break;
        case EffectNumberBase::SourceStar: base += "×星級"; break;
        case EffectNumberBase::SourceAttack: base += "×來源攻擊"; break;
        case EffectNumberBase::SourceMaxHp: base += "×來源最大生命"; break;
        case EffectNumberBase::SourceMissingHpRatio: base += "×來源已損生命比例"; break;
        case EffectNumberBase::SourceCurrentMpRatio: base += "×來源目前內力比例"; break;
        case EffectNumberBase::TargetMaxHp: base += "×目標最大生命"; break;
        case EffectNumberBase::TargetCurrentHp: base += "×目標目前生命"; break;
        case EffectNumberBase::TargetCurrentShield: base += "×目標目前護盾"; break;
        case EffectNumberBase::TargetCurrentCooldown: base += "×目標目前冷卻"; break;
        case EffectNumberBase::FinalHpDamage: base += "×實際生命傷害"; break;
        case EffectNumberBase::AccumulatedStateValue: base += "×累計狀態值"; break;
        case EffectNumberBase::SourceStatusPotency: base += std::format("×{}強度", number.status); break;
        case EffectNumberBase::SourceStatusStacks: base += std::format("×{}層數", number.status); break;
        case EffectNumberBase::StoredStateValue: base += "×狀態槽值"; break;
        }
    }
    if (number.base == EffectNumberBase::Constant)
    {
        return std::to_string(number.flat);
    }
    if (number.flat == 0)
    {
        return base;
    }
    return number.flat > 0
        ? std::format("{}＋{}", base, number.flat)
        : std::format("{}{}", base, number.flat);
}

std::string boundedNumberLabel(const EffectNumber& number)
{
    auto result = numberLabel(number);
    if (number.rounding == EffectRounding::Ceil)
    {
        result += "（向上取整）";
    }
    else if (number.rounding == EffectRounding::Floor)
    {
        result += "（向下取整）";
    }
    else if (number.rounding == EffectRounding::Nearest)
    {
        result += "（四捨五入）";
    }
    if (number.minimum)
    {
        result += std::format("·至少{}", *number.minimum);
    }
    if (number.maximum)
    {
        result += std::format("·至多{}", *number.maximum);
    }
    return result;
}

std::string_view borrowedRuleActionCategoryLabel(
    BorrowedRuleActionCategory category)
{
    switch (category)
    {
    case BorrowedRuleActionCategory::AttributeModifier: return "屬性修正";
    case BorrowedRuleActionCategory::DamageModifier: return "傷害修正";
    case BorrowedRuleActionCategory::ResourceChange: return "資源變更";
    case BorrowedRuleActionCategory::HealTransactionModifier: return "治療交易修正";
    case BorrowedRuleActionCategory::Status: return "狀態";
    case BorrowedRuleActionCategory::Damage: return "傷害";
    case BorrowedRuleActionCategory::Attack: return "攻擊";
    case BorrowedRuleActionCategory::ForcedMovement: return "強制移動";
    case BorrowedRuleActionCategory::Area: return "區域";
    case BorrowedRuleActionCategory::Cast: return "修改施放";
    case BorrowedRuleActionCategory::StateValue: return "狀態值";
    case BorrowedRuleActionCategory::DamageMemory: return "傷害記憶";
    case BorrowedRuleActionCategory::DamageAbsorption: return "傷害吸收";
    case BorrowedRuleActionCategory::StatusDamageSettlement: return "狀態傷害結算";
    }
    assert(false);
    return {};
}

std::string borrowedRuleFilterLabel(const BorrowedRuleFilter& filter)
{
    std::string result;
    for (const auto category : filter.allowedActionCategories)
    {
        if (!result.empty())
        {
            result += "、";
        }
        result += borrowedRuleActionCategoryLabel(category);
    }
    return result;
}

std::string_view copiedMagicConditionLabel(CopiedMagicCondition condition)
{
    switch (condition)
    {
    case CopiedMagicCondition::HasUltimateAttackDefinition:
        return "有絕招攻擊定義";
    case CopiedMagicCondition::ExcludesRecursiveEffects:
        return "排除複製與借用遞迴";
    }
    assert(false);
    return {};
}

std::string copiedMagicFilterLabel(const CopiedMagicFilter& filter)
{
    std::string result;
    for (const auto condition : filter.conditions)
    {
        if (!result.empty())
        {
            result += "、";
        }
        result += copiedMagicConditionLabel(condition);
    }
    return result;
}

std::string attributeLabel(BattleAttribute attribute, bool compact)
{
    switch (attribute)
    {
    case BattleAttribute::MaxHp: return compact ? "生命" : "最大生命";
    case BattleAttribute::Attack: return compact ? "攻" : "攻擊";
    case BattleAttribute::Defence: return compact ? "防" : "防禦";
    case BattleAttribute::Speed: return "速度";
    case BattleAttribute::CriticalChance: return compact ? "暴擊" : "暴擊率";
    case BattleAttribute::CriticalDamage: return "暴擊傷害";
    case BattleAttribute::DodgeChance: return compact ? "閃避" : "閃避率";
    case BattleAttribute::BlockChance: return compact ? "格擋" : "格擋率";
    case BattleAttribute::DamageReduction: return "傷害減免";
    case BattleAttribute::SkillDamage: return "技能傷害";
    case BattleAttribute::ProjectilePressureDamage: return compact ? "彈壓傷" : "彈道壓制傷害";
    case BattleAttribute::CooldownReduction: return "冷卻縮減";
    case BattleAttribute::MpRecoveryBonus: return compact ? "回內加成" : "內力回復加成";
    case BattleAttribute::StaggerResistance: return compact ? "僵抗" : "僵直抗性";
    case BattleAttribute::ProjectileReflectChance: return compact ? "彈反" : "彈道反射率";
    case BattleAttribute::SkillReflectPercent: return "技能反彈百分比";
    case BattleAttribute::CounterUltimateBlockChance: return compact ? "格擋反招" : "格擋絕招反擊率";
    case BattleAttribute::CriticalAfterDodge: return "閃避後暴擊";
    case BattleAttribute::DashChance: return "滑步機率";
    case BattleAttribute::OutgoingCooldownExtensionChance: return "攻擊冷卻延長率";
    case BattleAttribute::OutgoingCooldownExtensionPercent: return "攻擊冷卻延長百分比";
    case BattleAttribute::IncomingCooldownExtensionChance: return "受擊冷卻延長反擊率";
    case BattleAttribute::IncomingCooldownExtensionPercent: return "受擊冷卻延長百分比";
    }
    assert(false);
    return {};
}

std::string joinedDescriptionLabels(std::span<const std::string> labels)
{
    std::string result;
    for (const auto& label : labels)
    {
        if (!result.empty()) result += "、";
        result += label;
    }
    return result;
}

std::string conditionLabel(const EffectCondition& condition, bool compact)
{
    return std::visit(
        [compact](const auto& typed) -> std::string
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, IsUltimateCondition>) return "僅限絕招";
            else if constexpr (std::is_same_v<T, MagicIdEqualsCondition>) return compact ? std::format("武功{}", typed.magicId) : std::format("武功ID為{}", typed.magicId);
            else if constexpr (std::is_same_v<T, IsMainProjectileCondition>) return "僅限主彈道";
            else if constexpr (std::is_same_v<T, IsRootAttackCondition>) return "僅限根攻擊";
            else if constexpr (std::is_same_v<T, SourceHpRatioAtMostCondition>) return std::format("自身生命不高於{}%", typed.percent);
            else if constexpr (std::is_same_v<T, SourceHpRatioBelowCondition>) return std::format("自身生命低於{}%", typed.percent);
            else if constexpr (std::is_same_v<T, SourceIsLastAliveCondition>) return "自身為最後存活";
            else if constexpr (std::is_same_v<T, TargetHpRatioAtMostCondition>) return std::format("目標生命不高於{}%", typed.percent);
            else if constexpr (std::is_same_v<T, TargetNotInvincibleCondition>) return "目標非無敵";
            else if constexpr (std::is_same_v<T, SourceHasStateCondition>) return std::format("自身有{}", typed.state);
            else if constexpr (std::is_same_v<T, TargetHasStateCondition>) return std::format("目標有{}", typed.state);
            else if constexpr (std::is_same_v<T, TargetHasStateFromEffectOwnerCondition>) return std::format("目標有此來源的{}", typed.state);
            else if constexpr (std::is_same_v<T, SourceStackAtLeastCondition>) return std::format("{}至少{}層", typed.stack, typed.count);
            else if constexpr (std::is_same_v<T, OtherLivingAllyUsesMagicCondition>) return std::format("另一名武功{}使用者存活", typed.magicId);
            else if constexpr (std::is_same_v<T, CastDistinctTargetCountAtLeastCondition>) return std::format("本次命中至少{}名不同敵人", typed.count);
            else if constexpr (std::is_same_v<T, AttackOrdinalEqualsCondition>) return std::format("第{}道攻擊", typed.ordinal + 1);
            else if constexpr (std::is_same_v<T, HealKindInCondition>)
                return std::format("治療種類為{}", joinedDescriptionLabels(typed.kinds));
            else if constexpr (std::is_same_v<T, DamageOriginIsAttackCondition>) return "傷害來自招式";
            else if constexpr (std::is_same_v<T, DamageKilledTargetCondition>) return "該次傷害造成死亡";
            else if constexpr (std::is_same_v<T, AcceptedHitCondition>)
            {
                std::string result = "已接受命中";
                if (typed.requirePositiveDamage) result += "且傷害為正";
                if (typed.excludeReflected) result += "且不是反彈";
                return result;
            }
            else if constexpr (std::is_same_v<T, EventTargetBelongsToBoundSourceCondition>)
                return "事件目標屬於此效果來源";
            else if constexpr (std::is_same_v<T, DamagePerspectiveCondition>)
                return typed.perspective == DamagePerspective::Dealt ? "自身造成的傷害" : "自身承受的傷害";
            else if constexpr (std::is_same_v<T, DamageKindInCondition>)
                return std::format("傷害種類為{}", joinedDescriptionLabels(typed.kinds));
            else if constexpr (std::is_same_v<T, TargetMpWasFullBeforeCastCondition>) return "受益者施放前內力已滿";
            else if constexpr (std::is_same_v<T, RandomSelectionAvailableCondition>) return "有合法隨機目標";
            else static_assert(false, "Unhandled effect condition");
        },
        condition);
}

bool descriptionQualifierIsSuppressed(
    std::span<const DescriptionQualifier> suppressed,
    const DescriptionQualifier& qualifier)
{
    return std::ranges::find(suppressed, qualifier) != suppressed.end();
}

std::string_view stackPolicyLabel(EffectStackPolicy policy, bool compact)
{
    switch (policy)
    {
    case EffectStackPolicy::Independent: return compact ? "獨立" : "各次效果獨立存在";
    case EffectStackPolicy::Refresh: return compact ? "刷新" : "重複套用只刷新時間";
    case EffectStackPolicy::Replace: return compact ? "取代" : "重複套用會取代既有效果";
    case EffectStackPolicy::KeepStrongest: return compact ? "取強" : "只保留最強效果";
    case EffectStackPolicy::AddStack: return compact ? "疊層" : "重複套用會增加層數";
    }
    assert(false);
    return {};
}

std::string_view damageChannelLabel(DamageChannel channel)
{
    switch (channel)
    {
    case DamageChannel::Skill: return "招式傷害";
    case DamageChannel::Dot: return "持續傷害";
    case DamageChannel::Effect: return "特效傷害";
    case DamageChannel::Reflected: return "反彈傷害";
    case DamageChannel::All: return "所有傷害";
    }
    assert(false);
    return {};
}

std::string_view damageChannelSourceLabel(DamageChannel channel)
{
    switch (channel)
    {
    case DamageChannel::Skill: return "招式";
    case DamageChannel::Dot: return "持續效果";
    case DamageChannel::Effect: return "特效";
    case DamageChannel::Reflected: return "反彈";
    case DamageChannel::All: return "任意來源";
    }
    assert(false);
    return {};
}

std::string_view damageStageLabel(DamageModifierStage stage)
{
    switch (stage)
    {
    case DamageModifierStage::BeforeDefense: return "防禦結算前";
    case DamageModifierStage::AfterDefense: return "防禦結算後";
    case DamageModifierStage::Final: return "最終結算";
    }
    assert(false);
    return {};
}

std::string_view healKindLabel(EffectHealKind kind)
{
    switch (kind)
    {
    case EffectHealKind::Direct: return "直接治療";
    case EffectHealKind::Team: return "隊伍治療";
    case EffectHealKind::Aura: return "治療光環";
    case EffectHealKind::OnHit: return "命中治療";
    case EffectHealKind::KillReward: return "擊殺治療";
    case EffectHealKind::DeathMedical: return "死亡醫療";
    case EffectHealKind::Rescue: return "救援治療";
    case EffectHealKind::Regeneration: return "持續回復";
    case EffectHealKind::Lifesteal: return "吸血";
    }
    assert(false);
    return {};
}

void appendTimedStackQualifiers(
    std::string& result,
    int durationFrames,
    EffectStackPolicy stack,
    const std::optional<int>& stackLimit,
    EffectStackScope stackScope,
    bool perStack,
    bool compact,
    std::span<const DescriptionQualifier> suppressed)
{
    const DescriptionQualifier duration = DescriptionDurationFramesQualifier{ durationFrames };
    if (durationFrames > 0 && !descriptionQualifierIsSuppressed(suppressed, duration))
        result += std::format("·{}幀", durationFrames);

    const DescriptionQualifier stackPolicy = DescriptionStackPolicyQualifier{ stack };
    if (stack != EffectStackPolicy::Independent
        && !descriptionQualifierIsSuppressed(suppressed, stackPolicy))
    {
        result += compact ? "·" : "；";
        result += stackPolicyLabel(stack, compact);
    }

    if (stackLimit)
    {
        const DescriptionQualifier limit = DescriptionStackLimitQualifier{ *stackLimit };
        if (!descriptionQualifierIsSuppressed(suppressed, limit))
            result += std::format("·最多{}層", *stackLimit);
    }
    if (perStack)
    {
        const DescriptionQualifier qualifier = DescriptionPerStackQualifier{ true };
        if (!descriptionQualifierIsSuppressed(suppressed, qualifier)) result += "·數值按每層計算";
    }
    if (stackScope == EffectStackScope::EventSource)
    {
        const DescriptionQualifier qualifier = DescriptionStackScopeQualifier{ stackScope };
        if (!descriptionQualifierIsSuppressed(suppressed, qualifier)) result += "·各事件來源分別疊加";
    }
}

std::string renderDescriptionActionArgument(
    const EffectActionValue& action,
    bool compact,
    std::span<const DescriptionQualifier> suppressed = {});

std::string renderDescriptionActionArgument(
    const EffectActionValue& action,
    bool compact,
    std::span<const DescriptionQualifier> suppressed)
{
    return std::visit(
        [compact, suppressed](const auto& typed) -> std::string
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, ModifyAttributeAction>)
            {
                const auto attribute = attributeLabel(typed.attribute, compact);
                const auto amount = boundedNumberLabel(typed.amount);
                std::string result;
                switch (typed.operation)
                {
                case AttributeOperation::FlatAdd:
                    result = typed.amount.base == EffectNumberBase::Constant
                        ? std::format("{}{:+}", attribute, typed.amount.flat)
                        : std::format("{}增加{}", attribute, amount);
                    break;
                case AttributeOperation::PercentAdd:
                    result = typed.amount.base == EffectNumberBase::Constant
                        ? std::format("{}{:+}%", attribute, typed.amount.flat)
                        : std::format("{}增加{}", attribute, amount);
                    break;
                case AttributeOperation::Override:
                    result = std::format("{}改為{}", attribute, amount);
                    break;
                case AttributeOperation::Multiply:
                    result = typed.amount.base == EffectNumberBase::Constant
                        ? std::format("{}乘以{}%", attribute, amount)
                        : std::format("{}乘以{}", attribute, amount);
                    break;
                case AttributeOperation::AtLeast:
                    result = std::format("{}至少為{}", attribute, amount);
                    break;
                }
                appendTimedStackQualifiers(
                    result,
                    typed.durationFrames,
                    typed.stack,
                    typed.stackLimit,
                    typed.stackScope,
                    typed.perStack,
                    compact,
                    suppressed);
                return result;
            }
            else if constexpr (std::is_same_v<T, ModifyDamageAction>)
            {
                const auto percentAmount = [&]
                {
                    auto amount = boundedNumberLabel(typed.amount);
                    if (typed.amount.base == EffectNumberBase::Constant) amount += "%";
                    return amount;
                };
                const auto perspective = typed.perspective == DamageModifierPerspective::Outgoing
                    ? "造成的"
                    : "承受的";
                const auto context = std::format(
                    "{}{}（{}）",
                    perspective,
                    damageChannelLabel(typed.channel),
                    damageStageLabel(typed.stage));
                std::string result;
                if (typed.operation == DamageModifierOperation::IgnoreDefensePercent)
                    result = std::format("{}忽略{}防禦", context, percentAmount());
                else if (typed.operation == DamageModifierOperation::CapSingleHitAtMaxHpPercent)
                    result = std::format("{}下一次單次承傷不超過{}最大生命", context, percentAmount());
                else if (typed.operation == DamageModifierOperation::ExecuteBelowMaxHpPercent)
                    result = std::format("{}普通傷害後生命低於{}最大生命時處決", context, percentAmount());
                else
                {
                    auto amount = boundedNumberLabel(typed.amount);
                    if ((typed.operation == DamageModifierOperation::PercentAdd
                            || typed.operation == DamageModifierOperation::Multiply)
                        && typed.amount.base == EffectNumberBase::Constant)
                    {
                        amount += "%";
                    }
                    const auto operation = typed.operation == DamageModifierOperation::FlatAdd
                        ? "加算"
                        : typed.operation == DamageModifierOperation::PercentAdd
                        ? "百分比加算"
                        : "乘以";
                    result = std::format("{}{}{}", context, operation, amount);
                }
                appendTimedStackQualifiers(
                    result,
                    typed.durationFrames,
                    typed.stack,
                    typed.stackLimit,
                    typed.stackScope,
                    false,
                    compact,
                    suppressed);
                return result;
            }
            else if constexpr (std::is_same_v<T, ChangeResourceAction>)
            {
                const auto resource = typed.resource == BattleResource::Hp ? "生命"
                    : typed.resource == BattleResource::Mp ? "內力"
                    : typed.resource == BattleResource::Shield ? "護盾"
                    : typed.resource == BattleResource::StatusShield ? "狀態護盾"
                    : typed.resource == BattleResource::StaggerShield ? "僵直護盾"
                    : typed.resource == BattleResource::ActiveCooldown ? "目前冷卻"
                    : typed.resource == BattleResource::ControlImmunityFrames ? "僵直吸收幀數"
                    : "無敵幀數";
                const auto verb = typed.kind == ResourceChangeKind::Restore ? "回復"
                    : typed.kind == ResourceChangeKind::Drain ? "奪取"
                    : typed.kind == ResourceChangeKind::Grant ? "獲得"
                    : typed.kind == ResourceChangeKind::Remove ? "移除"
                    : typed.kind == ResourceChangeKind::Transfer ? "轉移"
                    : "至少刷新至";
                const bool numberIncludesResource = typed.resource == BattleResource::Hp
                    && (typed.amount.base == EffectNumberBase::SourceMaxHp
                        || typed.amount.base == EffectNumberBase::TargetMaxHp
                        || typed.amount.base == EffectNumberBase::TargetCurrentHp
                        || typed.amount.base == EffectNumberBase::FinalHpDamage);
                auto result = std::format(
                    "{}{}{}",
                    verb,
                    boundedNumberLabel(typed.amount),
                    numberIncludesResource ? "" : resource);
                if (typed.kind == ResourceChangeKind::Transfer && typed.transferDestination)
                    result += std::format("至{}", selectorLabel(*typed.transferDestination, compact));
                if (typed.resource == BattleResource::Hp
                    && typed.kind == ResourceChangeKind::Restore)
                {
                    result += std::format("·{}", healKindLabel(typed.healKind));
                    if (typed.healSourcePolicy == EffectHealSourcePolicy::AllowDead)
                        result += "·來源死亡仍可生效";
                }
                return result;
            }
            else if constexpr (std::is_same_v<T, ModifyHealTransactionAction>)
            {
                auto result = typed.operation == HealModifierOperation::Block
                    ? "無法受到治療"
                    : std::format("受到的治療改為{}%", typed.percent);
                result += std::format("·限{}", joinedDescriptionLabels(typed.kinds));
                return result;
            }
            else if constexpr (std::is_same_v<T, ApplyStatusAction>)
            {
                auto result = std::format("施加{}", battleStatusLabel(typed.status));
                if (typed.stacks != 1) result += std::format("{}層", typed.stacks);
                if (typed.applicationCount)
                    result += std::format("·獨立{}次", boundedNumberLabel(*typed.applicationCount));
                if (typed.potency.base != EffectNumberBase::Constant || typed.potency.flat != 0)
                    result += std::format("·強度{}", boundedNumberLabel(typed.potency));
                if (typed.secondaryPotency.base != EffectNumberBase::Constant || typed.secondaryPotency.flat != 0)
                    result += std::format("·次要強度{}", boundedNumberLabel(typed.secondaryPotency));
                if (typed.duration) result += std::format("·{}幀", boundedNumberLabel(*typed.duration));
                appendTimedStackQualifiers(
                    result,
                    typed.duration ? 0 : typed.durationFrames,
                    typed.stack,
                    typed.stackLimit,
                    EffectStackScope::Shared,
                    false,
                    compact,
                    suppressed);
                if (typed.aggregatePotencyWithinEvent) result += "·同事件合計強度";
                return result;
            }
            else if constexpr (std::is_same_v<T, ConsumeStatusAction>)
            {
                auto result = std::format("消耗{}{}層", battleStatusLabel(typed.status), typed.stacks);
                if (typed.source == StatusSourceMatch::EffectOwner) result += "·僅此來源";
                assert(!typed.whenDepleted
                    && "depleted status branches must be rendered through DescriptionConditional");
                return result;
            }
            else if constexpr (std::is_same_v<T, RemoveStatusAction>)
            {
                std::string result;
                if (typed.controlOnly) result = "清除全部控制狀態";
                else if (typed.negativeOnly) result = typed.count == 0 ? "清除全部負面狀態" : std::format("清除{}個負面狀態", typed.count);
                else if (!typed.statuses.empty())
                {
                    result = "移除";
                    for (const auto status : typed.statuses)
                    {
                        if (!result.ends_with("移除")) result += "、";
                        result += battleStatusLabel(status);
                    }
                    if (typed.count > 0) result += std::format("中的{}個", typed.count);
                }
                if (typed.count > 0)
                {
                    result += typed.order == StatusRemovalOrder::LongestRemaining
                        ? "·優先最長剩餘"
                        : typed.order == StatusRemovalOrder::Oldest
                        ? "·優先最早套用"
                        : "·優先最新套用";
                }
                if (typed.clearCurrentActionStagger)
                {
                    if (!result.empty()) result += "並";
                    result += "解除目前動作僵直（保留位置與動作）";
                }
                return result;
            }
            else if constexpr (std::is_same_v<T, DealDamageAction>)
            {
                const auto kind = std::format("{}傷害", damageKindLabel(typed.kind));
                const auto area = typed.area.kind == DamageAreaKind::Square
                    ? std::format("{}×{}範圍", typed.area.squareSideTiles, typed.area.squareSideTiles)
                    : typed.area.kind == DamageAreaKind::Circle
                    ? std::format("{}格範圍", typed.area.radiusTiles)
                    : std::string{};
                auto result = std::format("{}造成{}{}", area, boundedNumberLabel(typed.amount), kind);
                if (typed.transactionCount)
                    result += std::format("·獨立{}次", boundedNumberLabel(*typed.transactionCount));
                if (!typed.appliesDamageModifiers)
                    result += "·不套用傷害修正";
                if (!typed.triggersHurtInvincibility)
                    result += "·不觸發受傷無敵";
                if (typed.perCast.perTargetLimit > 0)
                    result += std::format("·每次施放對同一目標最多命中{}次", typed.perCast.perTargetLimit);
                if (typed.areaProjectiles)
                {
                    const auto& delivery = *typed.areaProjectiles;
                    result += std::format(
                        "·{}區域追蹤彈·{}格·最多{}目標",
                        delivery.visual == AreaProjectileVisual::DeathBlast
                            ? "死亡爆炸"
                            : "護盾爆炸",
                        delivery.rangeTiles,
                        delivery.maximumTargets);
                    if (delivery.trackEventSource) result += "·必含存活事件來源";
                    if (delivery.stunFrames > 0)
                        result += std::format("·眩暈{}幀", delivery.stunFrames);
                }
                return result;
            }
            else if constexpr (std::is_same_v<T, ModifyAttackAction>)
            {
                const auto pattern = typed.pattern.kind == AttackPatternKind::Fan ? "扇形"
                    : typed.pattern.kind == AttackPatternKind::Flanks ? "側翼"
                    : typed.pattern.kind == AttackPatternKind::SamePointSequence ? "同落點延遲"
                    : typed.pattern.kind == AttackPatternKind::MultiTarget ? "多目標"
                    : typed.pattern.kind == AttackPatternKind::EchoNearestOthers ? "殘影"
                    : "原樣式";
                auto result = std::format("{}{}×{}·{}%傷害",
                    pattern,
                    typed.mainProjectile ? "主彈" : "非主彈",
                    typed.pattern.projectileCount,
                    typed.strengthPct);
                if (typed.pattern.spreadDegrees > 0)
                    result += std::format("·展開{}度", typed.pattern.spreadDegrees);
                if (typed.through) result += *typed.through ? "·貫穿" : "·不貫穿";
                if (typed.tracking) result += *typed.tracking ? "·追蹤" : "·不追蹤";
                if (typed.sameTargetHitLimit > 0) result += std::format("·同目標{}次", typed.sameTargetHitLimit);
                if (typed.pattern.intervalFrames > 0) result += std::format("·間隔{}幀", typed.pattern.intervalFrames);
                if (typed.propagation == CastPropagationPolicy::SourceHitRulesOnly) result += "·僅觸發來源命中規則";
                else if (typed.propagation == CastPropagationPolicy::SuppressUltimateRules) result += "·不觸發大招效果";
                else if (typed.propagation == CastPropagationPolicy::BorrowedUltimateRules) result += "·觸發借用的大招規則";
                else if (typed.propagation == CastPropagationPolicy::NoEffectRules) result += "·不觸發任何效果規則";
                if (typed.addToBaseAttack) result += "·追加至基礎攻擊";
                if (typed.targets == AttackTargetPolicy::SelectedTargets) result += "·選擇目標";
                else if (typed.targets == AttackTargetPolicy::SamePoint) result += "·同落點";
                else if (typed.targets == AttackTargetPolicy::SameTarget) result += "·同目標";
                if (typed.source) result += std::format("·由{}出手", selectorLabel(*typed.source, true));
                if (typed.damageOverride)
                {
                    result += std::format("·{}", boundedNumberLabel(*typed.damageOverride));
                    if (typed.damageKind)
                        result += std::format("{}傷害", damageKindLabel(*typed.damageKind));
                    else
                        result += "傷害（沿用原傷害種類）";
                }
                else if (typed.damageKind)
                {
                    result += std::format("·傷害種類改為{}", damageKindLabel(*typed.damageKind));
                }
                std::visit(
                    [&](const auto& behavior)
                    {
                        using B = std::decay_t<decltype(behavior)>;
                        if constexpr (std::is_same_v<B, ProjectileBounceAttackBehavior>)
                        {
                            result += std::format(
                                "·彈射追加命中{}次·{}%·{}像素",
                                behavior.additionalHits,
                                behavior.chancePct,
                                behavior.rangePixels);
                        }
                        else if constexpr (std::is_same_v<B, NearbyTrackingAttackBehavior>)
                        {
                            result += std::format(
                                "·{}像素內產生{}%傷害追蹤彈",
                                behavior.rangePixels,
                                behavior.damagePct);
                        }
                        else if constexpr (std::is_same_v<B, DelayedAlternateAttackBehavior>)
                        {
                            result += std::format(
                                "·延遲{}幀替代目標追擊·{}%傷害·{}%獲得格擋",
                                behavior.delayFrames,
                                behavior.damagePct,
                                behavior.attackerBlockGainChancePct);
                        }
                        else if constexpr (std::is_same_v<B, ExpandingSpiralAttackBehavior>)
                        {
                            result += std::format(
                                "·擴張螺旋彈×{}·流血{}層",
                                behavior.projectileCount,
                                behavior.bleedStacks);
                        }
                    },
                    typed.runtimeBehavior);
                return result;
            }
            else if constexpr (std::is_same_v<T, ForceMoveAction>)
            {
                const auto direction = typed.direction == ForceMoveDirection::AwayFromSource
                    ? "擊退"
                    : typed.direction == ForceMoveDirection::TowardSource
                    ? "拉近"
                    : "移向指定點";
                auto result = typed.distancePixels > 0
                    ? std::format("{}{}像素並鎖定{}幀", direction, typed.distancePixels, typed.lockFrames)
                    : std::format("{}{}格並鎖定{}幀", direction, typed.distanceTiles, typed.lockFrames);
                result += typed.collision == ForceMoveCollision::StopBeforeOccupied
                    ? "·在佔位前停止"
                    : "·在阻擋地形前停止";
                result += typed.blocked == ForceMoveBlockedResult::Shorten
                    ? "·受阻時縮短位移"
                    : "·受阻時取消位移";
                return result;
            }
            else if constexpr (std::is_same_v<T, CreateAreaAction>)
            {
                auto result = typed.shape == AreaShape::Circle
                    ? std::format("建立半徑{}格、持續{}幀的區域", typed.radiusTiles, typed.durationFrames)
                    : std::format("建立{}×{}、持續{}幀的區域", typed.squareSideTiles, typed.squareSideTiles, typed.durationFrames);
                result += typed.anchor == AreaAnchor::HitPosition ? "·固定於命中位置" : "·跟隨來源單位";
                result += typed.sourceDeath == AreaSourceDeathPolicy::PersistUntilExpiry
                    ? "·來源死亡後持續至到期"
                    : "·來源死亡時立即移除";
                result += typed.merge == AreaMergePolicy::Independent
                    ? "·各區域獨立"
                    : typed.merge == AreaMergePolicy::RefreshSameSource
                    ? "·同來源刷新時間"
                    : "·同來源取代舊區域";
                const auto relationLabel = [](EffectTeamFilter relation)
                {
                    return relation == EffectTeamFilter::Ally ? "友方"
                        : relation == EffectTeamFilter::Enemy ? "敵方"
                        : "所有單位";
                };
                const auto overlapLabel = [](AreaOverlapPolicy overlap)
                {
                    return overlap == AreaOverlapPolicy::Add ? "相加"
                        : overlap == AreaOverlapPolicy::KeepStrongest ? "取最強"
                        : "任一成立";
                };
                for (const auto& modifier : typed.modifiers)
                {
                    result += compact ? "·" : "；";
                    if (modifier.kind == AreaModifierKind::Attribute)
                    {
                        const auto amount = modifier.amount.base == EffectNumberBase::Constant
                            ? std::format("{:+}", modifier.amount.flat)
                            : std::format("增加{}", boundedNumberLabel(modifier.amount));
                        result += std::format("區域內{}{}{}",
                            relationLabel(modifier.relation),
                            attributeLabel(modifier.attribute, compact),
                            amount);
                    }
                    else if (modifier.kind == AreaModifierKind::OutgoingDamage)
                    {
                        result += std::format("區域內{}造成的{}{}%",
                            relationLabel(modifier.relation),
                            damageChannelLabel(modifier.damageChannel),
                            modifier.percent >= 0 ? std::format("+{}", modifier.percent) : std::to_string(modifier.percent));
                    }
                    else if (modifier.kind == AreaModifierKind::AttackSpawn)
                    {
                        result += std::format("區域內{}的新彈道", relationLabel(modifier.relation));
                        if (modifier.tracking) result += *modifier.tracking ? "·可追蹤" : "·不可追蹤";
                        if (modifier.speedPct) result += std::format("·彈速{}%", *modifier.speedPct);
                        if (modifier.projectilePressurePct) result += std::format("·彈道壓制傷害{}%", *modifier.projectilePressurePct);
                    }
                    else
                    {
                        const auto direction = modifier.blockedDirection == ForceMoveDirection::AwayFromSource
                            ? "擊退"
                            : modifier.blockedDirection == ForceMoveDirection::TowardSource
                            ? "拉近"
                            : "移向指定點";
                        result += std::format("區域內{}免疫{}", relationLabel(modifier.relation), direction);
                    }
                    result += std::format("·重疊{}", overlapLabel(modifier.overlap));
                    if (modifier.trackingOverlap)
                        result += std::format("·追蹤重疊{}", overlapLabel(*modifier.trackingOverlap));
                    if (modifier.speedOverlap)
                        result += std::format("·彈速重疊{}", overlapLabel(*modifier.speedOverlap));
                    if (modifier.projectilePressureOverlap)
                        result += std::format("·彈壓重疊{}", overlapLabel(*modifier.projectilePressureOverlap));
                }
                return result;
            }
            else if constexpr (std::is_same_v<T, ModifyCastAction>)
            {
                std::string result;
                const auto append = [&](std::string fragment)
                {
                    if (!result.empty()) result += compact ? "·" : "，";
                    result += fragment;
                };
                if (typed.mpCost) append(std::format("實際消耗{}內力", boundedNumberLabel(*typed.mpCost)));
                if (typed.rangeMode == CastRangeMode::Ranged) append("武功遠程化");
                else if (typed.rangeMode == CastRangeMode::Preserve) append("保留原射程模式");
                if (typed.projectileSpeedPct > 0)
                    append(std::format("彈道速度{}%", typed.projectileSpeedPct));
                if (typed.minimumSelectDistance > 0)
                    append(std::format("最小選擇距離{}", typed.minimumSelectDistance));
                if (typed.additionalProjectiles > 0)
                    append(std::format("追加彈道{}枚", typed.additionalProjectiles));
                if (typed.mobility == CastMobilityPolicy::DashAttack) append("啟用滑步攻擊");
                else if (typed.mobility == CastMobilityPolicy::BlinkAttack) append("啟用閃擊");
                if (typed.autoUltimate)
                {
                    append(std::format(
                        "自動施放絕招（{}內力，{}公告）",
                        typed.autoUltimate->consumeMp ? "消耗" : "不消耗",
                        typed.autoUltimate->announce ? "顯示" : "不顯示"));
                }
                if (typed.replacementPattern)
                {
                    const auto& pattern = *typed.replacementPattern;
                    const auto patternName = pattern.kind == AttackPatternKind::Fan ? "扇形"
                        : pattern.kind == AttackPatternKind::Flanks ? "側翼"
                        : pattern.kind == AttackPatternKind::SamePointSequence ? "同落點延遲"
                        : pattern.kind == AttackPatternKind::MultiTarget ? "多目標"
                        : pattern.kind == AttackPatternKind::EchoNearestOthers ? "殘影"
                        : "原樣式";
                    auto replacement = std::format("替換攻擊樣式為{}×{}", patternName, pattern.projectileCount);
                    if (pattern.spreadDegrees > 0) replacement += std::format("·展開{}度", pattern.spreadDegrees);
                    if (pattern.intervalFrames > 0) replacement += std::format("·間隔{}幀", pattern.intervalFrames);
                    append(std::move(replacement));
                }
                if (typed.freeAdditionalCast) append("免費追加相同施放");
                if (typed.propagation == CastPropagationPolicy::SourceHitRulesOnly)
                    append("僅觸發來源命中規則");
                else if (typed.propagation == CastPropagationPolicy::SuppressUltimateRules)
                    append("不再次觸發大招效果");
                else if (typed.propagation == CastPropagationPolicy::BorrowedUltimateRules)
                    append("觸發借用的大招規則");
                else if (typed.propagation == CastPropagationPolicy::NoEffectRules)
                    append("不觸發任何效果規則");
                return result;
            }
            else if constexpr (std::is_same_v<T, StateMachineAction>)
            {
                return std::visit(
                    [compact](const auto& machine) -> std::string
                    {
                        using M = std::decay_t<decltype(machine)>;
                        if constexpr (std::is_same_v<M, ChangeStateValueAction>)
                        {
                            auto result = std::format("狀態值{:+}", machine.delta);
                            if (machine.minimum) result += std::format("·下限{}", *machine.minimum);
                            if (machine.maximum) result += std::format("·上限{}", *machine.maximum);
                            return result;
                        }
                        else if constexpr (std::is_same_v<M, TransferStateValueAction>)
                            return "轉移記錄值至另一狀態槽並清空來源";
                        else if constexpr (std::is_same_v<M, RecordMaximumDamageAction>)
                            return std::format("記錄最大單次{}生命傷害", damageChannelSourceLabel(machine.channel));
                        else if constexpr (std::is_same_v<M, ConsumeRecordedMaximumAction>)
                        {
                            const auto amount = machine.percent == 100 ? "等量" : std::format("{}%", machine.percent);
                            auto result = machine.clearAfterConsume
                                ? (machine.destination == StateValueDestination::DamageAmount
                                    ? std::format("消耗記錄值並附加{}純粹傷害", amount)
                                    : std::format("消耗記錄值並獲得{}護盾", amount))
                                : (machine.destination == StateValueDestination::DamageAmount
                                    ? std::format("讀取記錄值並附加{}純粹傷害", amount)
                                    : std::format("讀取記錄值並獲得{}護盾", amount));
                            return result;
                        }
                        else if constexpr (std::is_same_v<M, StartDamageAbsorptionAction>)
                        {
                            assert(false && "damage absorption must be rendered through DescriptionStateCycle");
                            return {};
                        }
                        else if constexpr (std::is_same_v<M, SettleDamageAbsorptionAction>)
                        {
                            auto result = std::format(
                                "將累計吸收值的{}%以{}傷害結算給{}",
                                machine.returnedPct,
                                damageKindLabel(machine.damageKind),
                                selectorLabel(machine.target, compact));
                            if (!machine.clearAfterSettle) result += "·保留累計值";
                            return result;
                        }
                        else if constexpr (std::is_same_v<M, BorrowEffectRulesAction>)
                        {
                            return std::format(
                                "借用{}的大招規則·數量{}·允許類別[{}]·不含複製與借用遞迴·傳播借用規則",
                                selectorLabel(machine.sourceUnits, compact),
                                boundedNumberLabel(machine.sourceCount),
                                borrowedRuleFilterLabel(machine.filter));
                        }
                        else if constexpr (std::is_same_v<M, CopyAttackDefinitionAction>)
                        {
                            return std::format(
                                "複製{}的絕招武功攻擊·數量{}·條件[{}]·不傳播大招規則",
                                selectorLabel(machine.sourceUnits, compact),
                                machine.copyCount,
                                copiedMagicFilterLabel(machine.filter));
                        }
                        else if constexpr (std::is_same_v<M, SettleRemainingStatusDamageAction>)
                        {
                            return std::format(
                                "立即結算剩餘{}傷害",
                                battleStatusLabel(machine.status));
                        }
                        else if constexpr (std::is_same_v<M, GenerateClonesAction>)
                            return std::format("生成{}個分身", machine.count);
                        else if constexpr (std::is_same_v<M, PreventDeathAction>)
                            return std::format("首次致命傷害鎖定1生命並獲得{}幀無敵", machine.invincibilityFrames);
                        else if constexpr (std::is_same_v<M, ConfigureRescueRepositionAction>)
                            return std::format(
                                "每場可觸發{}次{}",
                                machine.activations,
                                machine.mode == RescueRepositionMode::Protect ? "保護挪移" : "處決挪移");
                        else static_assert(false, "Unhandled state machine action");
                    },
                    typed);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
            {
                assert(false && "conditional actions must be rendered through DescriptionConditional");
                return {};
            }
            else static_assert(false, "Unhandled effect action");
        },
        action);
}

std::vector<DescriptionQualifier> actionDescriptionQualifiers(
    const EffectActionValue& action)
{
    std::vector<DescriptionQualifier> qualifiers;
    const auto appendTimed = [&](
        int durationFrames,
        EffectStackPolicy stack,
        const std::optional<int>& stackLimit,
        EffectStackScope stackScope,
        bool perStack)
    {
        if (durationFrames > 0)
            qualifiers.emplace_back(DescriptionDurationFramesQualifier{ durationFrames });
        if (stack != EffectStackPolicy::Independent)
            qualifiers.emplace_back(DescriptionStackPolicyQualifier{ stack });
        if (stackLimit)
            qualifiers.emplace_back(DescriptionStackLimitQualifier{ *stackLimit });
        if (perStack)
            qualifiers.emplace_back(DescriptionPerStackQualifier{ true });
        if (stackScope == EffectStackScope::EventSource)
            qualifiers.emplace_back(DescriptionStackScopeQualifier{ stackScope });
    };

    if (const auto* modifier = std::get_if<ModifyAttributeAction>(&action))
    {
        appendTimed(
            modifier->durationFrames,
            modifier->stack,
            modifier->stackLimit,
            modifier->stackScope,
            modifier->perStack);
    }
    else if (const auto* modifier = std::get_if<ModifyDamageAction>(&action))
    {
        appendTimed(
            modifier->durationFrames,
            modifier->stack,
            modifier->stackLimit,
            modifier->stackScope,
            false);
    }
    else if (const auto* status = std::get_if<ApplyStatusAction>(&action))
    {
        appendTimed(
            status->duration ? 0 : status->durationFrames,
            status->stack,
            status->stackLimit,
            EffectStackScope::Shared,
            false);
    }
    return qualifiers;
}

std::vector<DescriptionQualifier> sharedActionDescriptionQualifiers(
    const std::vector<EffectAction>& actions)
{
    assert(actions.size() > 1);
    // Runtime stack domains include actionOrder.  Sibling actions therefore
    // have distinct stack keys even when their duration and policy happen to
    // have equal values.  Keep their qualifiers on each clause until the
    // payload gains an explicit shared stack key.
    return {};
}

bool statusActionsDependOnOrder(
    const EffectActionValue& first,
    const EffectActionValue& second)
{
    const auto statusOf = [](const EffectActionValue& action)
        -> std::optional<BattleStatusKind>
    {
        if (const auto* apply = std::get_if<ApplyStatusAction>(&action)) return apply->status;
        if (const auto* consume = std::get_if<ConsumeStatusAction>(&action)) return consume->status;
        if (const auto* state = std::get_if<StateMachineAction>(&action))
        {
            if (const auto* settle = std::get_if<SettleRemainingStatusDamageAction>(state))
                return settle->status;
        }
        return std::nullopt;
    };
    const auto removalAffects = [](const RemoveStatusAction& removal, BattleStatusKind status)
    {
        return removal.negativeOnly
            || removal.controlOnly
            || std::ranges::contains(removal.statuses, status);
    };

    const auto firstStatus = statusOf(first);
    const auto secondStatus = statusOf(second);
    if (firstStatus && secondStatus && *firstStatus == *secondStatus) return true;
    if (const auto* removal = std::get_if<RemoveStatusAction>(&first))
        if (secondStatus && removalAffects(*removal, *secondStatus)) return true;
    if (const auto* removal = std::get_if<RemoveStatusAction>(&second))
        if (firstStatus && removalAffects(*removal, *firstStatus)) return true;
    return false;
}

bool actionPairDependsOnOrder(
    const EffectActionValue& first,
    const EffectActionValue& second)
{
    if (std::holds_alternative<std::shared_ptr<ConditionalEffectAction>>(first)
        || std::holds_alternative<std::shared_ptr<ConditionalEffectAction>>(second))
        return true;
    if (std::holds_alternative<StateMachineAction>(first)
        || std::holds_alternative<StateMachineAction>(second))
        return true;
    if (std::holds_alternative<DealDamageAction>(first)
        || std::holds_alternative<DealDamageAction>(second))
        return true;
    if (statusActionsDependOnOrder(first, second)) return true;

    if (const auto* lhs = std::get_if<ModifyAttributeAction>(&first))
        if (const auto* rhs = std::get_if<ModifyAttributeAction>(&second))
            return lhs->attribute == rhs->attribute;
    if (const auto* lhs = std::get_if<ModifyDamageAction>(&first))
        if (const auto* rhs = std::get_if<ModifyDamageAction>(&second))
            return lhs->perspective == rhs->perspective
                && lhs->stage == rhs->stage
                && lhs->channel == rhs->channel;
    if (const auto* lhs = std::get_if<ChangeResourceAction>(&first))
        if (const auto* rhs = std::get_if<ChangeResourceAction>(&second))
            return lhs->resource == rhs->resource;
    if (std::holds_alternative<ModifyAttackAction>(first)
        && std::holds_alternative<ModifyAttackAction>(second))
        return true;
    if (std::holds_alternative<ModifyCastAction>(first)
        && std::holds_alternative<ModifyCastAction>(second))
        return true;
    return false;
}

bool actionsDependOnOrder(const std::vector<EffectAction>& actions)
{
    for (std::size_t lhs = 0; lhs < actions.size(); ++lhs)
    {
        for (std::size_t rhs = lhs + 1; rhs < actions.size(); ++rhs)
        {
            if (actionPairDependsOnOrder(actions[lhs].value, actions[rhs].value))
                return true;
        }
    }
    return false;
}

DescriptionClause actionDescriptionClause(const EffectActionValue& action)
{
    assert(!std::holds_alternative<std::shared_ptr<ConditionalEffectAction>>(action));
    DescriptionClause clause;
    clause.subject.id = DescriptionSubjectId::SelectedTargets;
    clause.predicate = static_cast<DescriptionPredicate>(action.index());
    clause.arguments.emplace_back(DescriptionActionArgument{ action });
    clause.qualifiers = actionDescriptionQualifiers(action);
    return clause;
}

EffectDescriptionChild actionListDescriptionNode(
    const std::vector<EffectAction>& actions);

EffectDescriptionChild actionDescriptionNode(const EffectAction& action)
{
    if (const auto* conditional = std::get_if<std::shared_ptr<ConditionalEffectAction>>(
            &action.value))
    {
        assert(*conditional);
        DescriptionConditional node;
        node.scope = DescriptionConditionalScope::PerTarget;
        for (const auto& condition : (*conditional)->conditions)
            node.conditions.push_back({ DescriptionConditionValue{ condition } });
        node.whenTrue = actionListDescriptionNode((*conditional)->whenTrue);
        node.whenFalse = actionListDescriptionNode((*conditional)->whenFalse);
        return makeDescriptionNode(std::move(node));
    }

    if (const auto* consume = std::get_if<ConsumeStatusAction>(&action.value);
        consume && consume->whenDepleted)
    {
        auto baseValue = action.value;
        auto& baseConsume = std::get<ConsumeStatusAction>(baseValue);
        const auto depletedStatus = baseConsume.status;
        const auto depletedAction = *baseConsume.whenDepleted;
        baseConsume.whenDepleted.reset();

        DescriptionSequence sequence;
        sequence.kind = DescriptionSequenceKind::Actions;
        sequence.steps.push_back(makeDescriptionNode(
            actionDescriptionClause(baseValue)));

        DescriptionConditional depleted;
        depleted.scope = DescriptionConditionalScope::PerTarget;
        depleted.conditions.push_back({
            DescriptionConditionValue{
                DescriptionStatusDepletedCondition{ depletedStatus },
            },
        });
        depleted.whenTrue = actionDescriptionNode(EffectAction{
            EffectActionValue{ depletedAction },
        });
        sequence.steps.push_back(makeDescriptionNode(std::move(depleted)));
        return makeDescriptionNode(std::move(sequence));
    }

    auto clause = makeDescriptionNode(actionDescriptionClause(action.value));
    const auto* state = std::get_if<StateMachineAction>(&action.value);
    if (!state) return clause;

    DescriptionStateCycle cycle;
    if (std::holds_alternative<RecordMaximumDamageAction>(*state))
    {
        cycle.record = std::move(clause);
        cycle.initialValue = 0;
        return makeDescriptionNode(std::move(cycle));
    }
    if (const auto* consume = std::get_if<ConsumeRecordedMaximumAction>(state))
    {
        cycle.consume = std::move(clause);
        if (consume->clearAfterConsume)
        {
            DescriptionClause reset;
            reset.subject.id = DescriptionSubjectId::RecordedState;
            reset.predicate = DescriptionPredicate::ResetRecordedState;
            reset.arguments.emplace_back(DescriptionResetStateArgument{ consume->slot });
            cycle.reset = makeDescriptionNode(std::move(reset));
        }
        return makeDescriptionNode(std::move(cycle));
    }
    if (const auto* absorption = std::get_if<StartDamageAbsorptionAction>(state))
    {
        DescriptionClause record;
        record.subject.id = DescriptionSubjectId::RecordedState;
        record.predicate = DescriptionPredicate::RecordDamageAbsorption;
        record.arguments.emplace_back(DescriptionDamageAbsorptionRecordArgument{
            absorption->absorbedPct,
            absorption->durationFrames,
        });
        cycle.record = makeDescriptionNode(std::move(record));

        DescriptionClause consume;
        consume.subject.id = DescriptionSubjectId::RecordedState;
        consume.predicate = DescriptionPredicate::SettleDamageAbsorption;
        consume.arguments.emplace_back(DescriptionDamageAbsorptionSettlementArgument{
            absorption->settlementTarget,
            absorption->settlementDamageKind,
            absorption->returnedPct,
            absorption->settleOnSourceDeath,
        });
        cycle.consume = makeDescriptionNode(std::move(consume));

        DescriptionClause reset;
        reset.subject.id = DescriptionSubjectId::RecordedState;
        reset.predicate = DescriptionPredicate::ResetRecordedState;
        reset.arguments.emplace_back(DescriptionResetStateArgument{ absorption->slot });
        cycle.reset = makeDescriptionNode(std::move(reset));
        cycle.initialValue = 0;
        return makeDescriptionNode(std::move(cycle));
    }
    if (const auto* settle = std::get_if<SettleDamageAbsorptionAction>(state))
    {
        cycle.consume = std::move(clause);
        if (settle->clearAfterSettle)
        {
            DescriptionClause reset;
            reset.subject.id = DescriptionSubjectId::RecordedState;
            reset.predicate = DescriptionPredicate::ResetRecordedState;
            reset.arguments.emplace_back(DescriptionResetStateArgument{ settle->slot });
            cycle.reset = makeDescriptionNode(std::move(reset));
        }
        return makeDescriptionNode(std::move(cycle));
    }
    return clause;
}

EffectDescriptionChild actionListDescriptionNode(
    const std::vector<EffectAction>& actions)
{
    if (actions.empty()) return {};
    if (actions.size() == 1) return actionDescriptionNode(actions.front());

    if (actionsDependOnOrder(actions))
    {
        DescriptionSequence sequence;
        sequence.kind = DescriptionSequenceKind::Actions;
        for (const auto& action : actions)
            sequence.steps.push_back(actionDescriptionNode(action));
        return makeDescriptionNode(std::move(sequence));
    }

    DescriptionSimultaneous simultaneous;
    simultaneous.qualifiers = sharedActionDescriptionQualifiers(actions);
    for (const auto& action : actions)
        simultaneous.actions.push_back(actionDescriptionNode(action));
    return makeDescriptionNode(std::move(simultaneous));
}

EffectDescriptionNode buildEffectDescriptionAst(const EffectRule& rule)
{
    DescriptionSequence root;
    root.kind = DescriptionSequenceKind::Rule;

    DescriptionClause trigger;
    trigger.subject.id = DescriptionSubjectId::RuleEvent;
    trigger.predicate = DescriptionPredicate::Trigger;
    trigger.arguments.emplace_back(DescriptionTriggerArgument{
        rule.event,
        rule.observation,
        rule.castMatch,
    });
    root.steps.push_back(makeDescriptionNode(std::move(trigger)));

    DescriptionConditional guard;
    guard.scope = DescriptionConditionalScope::Rule;
    for (const auto& condition : rule.conditions)
        guard.conditions.push_back({ DescriptionConditionValue{ condition } });
    if (rule.chancePct < 100)
        guard.qualifiers.emplace_back(DescriptionChanceQualifier{ rule.chancePct });
    if (rule.intervalFrames > 0)
        guard.qualifiers.emplace_back(DescriptionIntervalQualifier{ rule.intervalFrames });
    if (rule.everyNthEvent > 0)
        guard.qualifiers.emplace_back(DescriptionEveryNthEventQualifier{ rule.everyNthEvent });
    if (rule.repetitionCount)
        guard.qualifiers.emplace_back(DescriptionRepetitionCountQualifier{ *rule.repetitionCount });
    DescriptionForEach targets;
    targets.targets.selector = rule.selector;
    targets.body = actionListDescriptionNode(rule.actions);
    guard.whenTrue = makeDescriptionNode(std::move(targets));
    root.steps.push_back(makeDescriptionNode(std::move(guard)));

    DescriptionClause limits;
    limits.predicate = DescriptionPredicate::RuleLimits;
    if (rule.maxActivations > 0)
        limits.qualifiers.emplace_back(DescriptionMaxActivationsQualifier{ rule.maxActivations });
    if (rule.sharedCooldownFrames > 0)
        limits.qualifiers.emplace_back(DescriptionSharedCooldownQualifier{ rule.sharedCooldownFrames });
    if (rule.activationLimit)
    {
        limits.qualifiers.emplace_back(DescriptionActivationLimitQualifier{
            rule.activationLimit->scope,
            rule.activationLimit->maxEvaluations,
        });
    }
    root.steps.push_back(makeDescriptionNode(std::move(limits)));
    return EffectDescriptionNode{ std::move(root) };
}

struct DescriptionRenderContext
{
    EffectDescriptionStyle style{};
    std::optional<EffectEvent> event;
    std::span<const DescriptionQualifier> suppressedActionQualifiers;
};

std::string renderDescriptionNode(
    const EffectDescriptionNode& node,
    DescriptionRenderContext context);

std::string renderRuleQualifier(
    const DescriptionQualifier& qualifier,
    bool compact)
{
    return std::visit(
        [compact](const auto& typed) -> std::string
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, DescriptionChanceQualifier>)
                return compact ? std::format("·{}%", typed.percent) : std::format("，有{}%機率", typed.percent);
            else if constexpr (std::is_same_v<T, DescriptionMaxActivationsQualifier>)
                return compact ? std::format("·最多{}次", typed.count) : std::format("；最多啟用{}次", typed.count);
            else if constexpr (std::is_same_v<T, DescriptionSharedCooldownQualifier>)
                return compact ? std::format("·同來源冷卻{}幀", typed.frames) : std::format("；同來源共用{}幀冷卻", typed.frames);
            else if constexpr (std::is_same_v<T, DescriptionIntervalQualifier>)
                return compact ? std::format("·每{}幀", typed.frames) : std::format("；每{}幀一次", typed.frames);
            else if constexpr (std::is_same_v<T, DescriptionEveryNthEventQualifier>)
                return compact ? std::format("·每{}次", typed.count) : std::format("；每{}次符合事件啟用一次", typed.count);
            else if constexpr (std::is_same_v<T, DescriptionRepetitionCountQualifier>)
                return compact
                    ? std::format("·依序×{}", boundedNumberLabel(typed.count))
                    : std::format("；依序重複{}次", boundedNumberLabel(typed.count));
            else if constexpr (std::is_same_v<T, DescriptionActivationLimitQualifier>)
            {
                assert(typed.scope == EffectActivationScope::PerCastPerTarget);
                return compact
                    ? std::format("·每施放每目標{}次", typed.count)
                    : std::format("；每次施放對同一目標最多判定{}次", typed.count);
            }
            else return {};
        },
        qualifier);
}

std::string renderGuardQualifier(
    const DescriptionQualifier& qualifier,
    bool compact)
{
    if (const auto* interval = std::get_if<DescriptionIntervalQualifier>(&qualifier))
        return compact ? std::format("·每{}幀", interval->frames) : std::format("；每{}幀一次", interval->frames);
    if (const auto* nth = std::get_if<DescriptionEveryNthEventQualifier>(&qualifier))
        return compact ? std::format("·每{}次", nth->count) : std::format("；每{}次符合事件啟用一次", nth->count);
    return renderRuleQualifier(qualifier, compact);
}

std::string renderSharedActionQualifiers(
    std::span<const DescriptionQualifier> qualifiers,
    bool compact)
{
    std::string result;
    for (const auto& qualifier : qualifiers)
    {
        std::visit(
            [&](const auto& typed)
            {
                using T = std::decay_t<decltype(typed)>;
                if constexpr (std::is_same_v<T, DescriptionDurationFramesQualifier>)
                    result += compact ? std::format("·{}幀", typed.frames) : std::format("，持續{}幀", typed.frames);
                else if constexpr (std::is_same_v<T, DescriptionStackPolicyQualifier>)
                {
                    result += compact ? "·" : "；";
                    result += stackPolicyLabel(typed.policy, compact);
                }
                else if constexpr (std::is_same_v<T, DescriptionStackLimitQualifier>)
                    result += std::format("·最多{}層", typed.count);
                else if constexpr (std::is_same_v<T, DescriptionPerStackQualifier>)
                    result += "·數值按每層計算";
                else if constexpr (std::is_same_v<T, DescriptionStackScopeQualifier>)
                    result += "·各事件來源分別疊加";
            },
            qualifier);
    }
    return result;
}

std::string renderDescriptionClause(
    const DescriptionClause& clause,
    DescriptionRenderContext context)
{
    const bool compact = context.style == EffectDescriptionStyle::Compact;
    if (clause.predicate == DescriptionPredicate::Trigger)
    {
        const auto& trigger = std::get<DescriptionTriggerArgument>(
            clause.arguments.front());
        auto result = ruleEventLabel(trigger.event, compact);
        if (trigger.observation == EffectObservationScope::OwnerTeamEventSource)
            result += compact ? "·同隊來源" : "（由效果擁有者同隊事件來源觸發）";
        else if (trigger.observation == EffectObservationScope::EventTarget)
            result += compact ? "·事件目標" : "（由效果擁有者成為事件目標時觸發）";
        if (trigger.castMatch == EffectCastMatch::OwnerAnyCast)
            result += compact ? "·任意施放" : "（效果擁有者任意施放）";
        return result;
    }
    if (clause.predicate == DescriptionPredicate::RuleLimits)
    {
        std::string result;
        for (const auto& qualifier : clause.qualifiers)
            result += renderRuleQualifier(qualifier, compact);
        return result;
    }
    if (clause.predicate == DescriptionPredicate::ResetRecordedState)
        return compact ? "清空" : "清空記錄";
    if (clause.predicate == DescriptionPredicate::RecordDamageAbsorption)
    {
        const auto& record = std::get<DescriptionDamageAbsorptionRecordArgument>(
            clause.arguments.front());
        return std::format(
            "{}幀內記錄並吸收所受傷害的{}%",
            record.durationFrames,
            record.absorbedPct);
    }
    if (clause.predicate == DescriptionPredicate::SettleDamageAbsorption)
    {
        const auto& settlement = std::get<DescriptionDamageAbsorptionSettlementArgument>(
            clause.arguments.front());
        auto result = std::format(
            "將累計吸收值的{}%以{}傷害結算給{}",
            settlement.returnedPct,
            damageKindLabel(settlement.damageKind),
            selectorLabel(settlement.target, compact));
        if (settlement.settleOnSourceDeath)
            result += compact ? "·來源死亡時立即結算" : "；來源死亡時立即結算";
        return result;
    }

    const auto& action = std::get<DescriptionActionArgument>(
        clause.arguments.front());
    assert(clause.predicate == static_cast<DescriptionPredicate>(action.value.index()));
    return renderDescriptionActionArgument(
        action.value,
        compact,
        context.suppressedActionQualifiers);
}

std::string renderDescriptionConditions(
    std::span<const DescriptionCondition> conditions,
    DescriptionConditionalScope scope,
    std::optional<EffectEvent> event,
    bool compact)
{
    std::string result;
    for (const auto& condition : conditions)
    {
        const auto* parsed = std::get_if<EffectCondition>(&condition.value);
        if (parsed
            && scope == DescriptionConditionalScope::Rule
            && std::holds_alternative<MagicIdEqualsCondition>(*parsed))
        {
            continue;
        }
        if (parsed
            && scope == DescriptionConditionalScope::Rule
            && event == EffectEvent::UltimateCommitted
            && std::holds_alternative<IsUltimateCondition>(*parsed))
        {
            continue;
        }
        if (!result.empty()) result += compact ? "＋" : "且";
        if (parsed)
        {
            result += conditionLabel(*parsed, compact);
        }
        else
        {
            const auto& depleted = std::get<DescriptionStatusDepletedCondition>(
                condition.value);
            result += std::format("{}最後一層已消耗", battleStatusLabel(depleted.status));
        }
    }
    return result;
}

std::string renderDescriptionNode(
    const EffectDescriptionNode& node,
    DescriptionRenderContext context)
{
    const bool compact = context.style == EffectDescriptionStyle::Compact;
    return std::visit(
        [&](const auto& typed) -> std::string
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, DescriptionClause>)
            {
                return renderDescriptionClause(typed, context);
            }
            else if constexpr (std::is_same_v<T, DescriptionSequence>)
            {
                std::string result;
                if (typed.kind == DescriptionSequenceKind::Rule)
                {
                    if (!typed.steps.empty())
                    {
                        const auto* trigger = std::get_if<DescriptionClause>(
                            &typed.steps.front()->value);
                        if (trigger && trigger->predicate == DescriptionPredicate::Trigger)
                        {
                            const auto& argument = std::get<DescriptionTriggerArgument>(
                                trigger->arguments.front());
                            context.event = argument.event;
                        }
                    }
                    for (const auto& step : typed.steps)
                        if (step) result += renderDescriptionNode(*step, context);
                    return result;
                }
                const std::string_view separator = compact ? "→" : "，接著";
                for (const auto& step : typed.steps)
                {
                    if (!step) continue;
                    if (!result.empty()) result += separator;
                    result += renderDescriptionNode(*step, context);
                }
                return result;
            }
            else if constexpr (std::is_same_v<T, DescriptionSimultaneous>)
            {
                std::string result;
                auto nested = context;
                nested.suppressedActionQualifiers = typed.qualifiers;
                const std::string_view separator = compact ? "／" : "、";
                for (const auto& action : typed.actions)
                {
                    if (!action) continue;
                    if (!result.empty()) result += separator;
                    result += renderDescriptionNode(*action, nested);
                }
                result += renderSharedActionQualifiers(typed.qualifiers, compact);
                return result;
            }
            else if constexpr (std::is_same_v<T, DescriptionConditional>)
            {
                const auto conditions = renderDescriptionConditions(
                    typed.conditions,
                    typed.scope,
                    context.event,
                    compact);
                if (typed.scope == DescriptionConditionalScope::Rule)
                {
                    std::string result;
                    if (!conditions.empty()) result += compact ? std::format("·{}", conditions) : std::format("，{}", conditions);
                    for (const auto& qualifier : typed.qualifiers)
                        result += renderGuardQualifier(qualifier, compact);
                    if (typed.whenTrue) result += renderDescriptionNode(*typed.whenTrue, context);
                    return result;
                }

                const auto whenTrue = typed.whenTrue
                    ? renderDescriptionNode(*typed.whenTrue, context)
                    : std::string{};
                const auto whenFalse = typed.whenFalse
                    ? renderDescriptionNode(*typed.whenFalse, context)
                    : std::string{};
                const bool fullMpOverride = std::ranges::any_of(
                    typed.conditions,
                    [](const DescriptionCondition& condition)
                    {
                        const auto* parsed = std::get_if<EffectCondition>(&condition.value);
                        return parsed
                            && std::holds_alternative<TargetMpWasFullBeforeCastCondition>(
                                *parsed);
                    });
                if (fullMpOverride && !whenFalse.empty())
                {
                    return compact
                        ? std::format("{}；滿內改{}", whenFalse, whenTrue)
                        : std::format("{}；個別受益者施放前若已滿內，改為{}", whenFalse, whenTrue);
                }
                if (whenFalse.empty())
                    return compact ? std::format("若{}·{}", conditions, whenTrue) : std::format("若{}，{}", conditions, whenTrue);
                return compact
                    ? std::format("{}?{}:{}", conditions, whenTrue, whenFalse)
                    : std::format("若{}則{}，否則{}", conditions, whenTrue, whenFalse);
            }
            else if constexpr (std::is_same_v<T, DescriptionForEach>)
            {
                const auto target = selectorLabel(typed.targets.selector, compact);
                const auto body = typed.body
                    ? renderDescriptionNode(*typed.body, context)
                    : std::string{};
                return compact
                    ? std::format("·{}·{}", target, body)
                    : std::format("，對{}{}", target, body);
            }
            else if constexpr (std::is_same_v<T, DescriptionStateCycle>)
            {
                std::string result;
                const auto append = [&](const EffectDescriptionChild& phase)
                {
                    if (!phase) return;
                    if (!result.empty()) result += compact ? "→" : "，隨後";
                    result += renderDescriptionNode(*phase, context);
                };
                append(typed.record);
                append(typed.consume);
                append(typed.reset);
                if (typed.initialValue)
                    result += compact
                        ? std::format("·初始{}", *typed.initialValue)
                        : std::format("；首次記錄值為{}", *typed.initialValue);
                return result;
            }
            else static_assert(false, "Unhandled effect description node");
        },
        node.value);
}

std::string renderFullEffectDescription(const EffectDescriptionNode& ast)
{
    return renderDescriptionNode(
        ast,
        DescriptionRenderContext{ .style = EffectDescriptionStyle::Full }) + "。";
}

std::string renderCompactEffectDescription(const EffectDescriptionNode& ast)
{
    return renderDescriptionNode(
        ast,
        DescriptionRenderContext{ .style = EffectDescriptionStyle::Compact });
}

bool eventHasHitPayload(EffectEvent event)
{
    return event == EffectEvent::MainProjectileBeforeDamage
        || event == EffectEvent::HitBeforeDamage;
}

bool eventHasDamagePayload(EffectEvent event)
{
    return event == EffectEvent::DamageResolved;
}

bool eventHasDamageOriginPayload(EffectEvent event)
{
    return eventHasDamagePayload(event)
        || event == EffectEvent::ShieldBroken
        || event == EffectEvent::UnitDied
        || event == EffectEvent::AllyDied;
}

bool eventHasHealPayload(EffectEvent event)
{
    return event == EffectEvent::HealAttempted
        || event == EffectEvent::HealApplied;
}

bool eventHasCastAggregatePayload(EffectEvent event)
{
    return event == EffectEvent::CastContinuation
        || event == EffectEvent::CastSettled;
}

bool eventHasCastProvenance(EffectEvent event)
{
    switch (event)
    {
    case EffectEvent::CastPlanned:
    case EffectEvent::AttackCommitted:
    case EffectEvent::UltimateCommitted:
    case EffectEvent::AttackSpawned:
    case EffectEvent::MainProjectileBeforeDamage:
    case EffectEvent::HitBeforeDamage:
    case EffectEvent::CastContinuation:
    case EffectEvent::CastSettled:
        return true;
    default:
        return false;
    }
}

bool validateSelectorAtEvent(const EffectSelector& selector, EffectEvent event, std::string& error)
{
    if (selector.kind == EffectSelectorKind::HitTarget && !eventHasHitPayload(event) && !eventHasDamagePayload(event))
    {
        error = "命中目標選擇器需要命中或傷害事件";
        return false;
    }
    if (selector.kind == EffectSelectorKind::TransactionTarget
        && !eventHasDamagePayload(event)
        && !eventHasHealPayload(event)
        && event != EffectEvent::ShieldBroken
        && event != EffectEvent::UnitDied
        && event != EffectEvent::AllyDied)
    {
        error = "交易目標選擇器需要交易事件";
        return false;
    }
    if (selector.kind == EffectSelectorKind::OriginalAttackTarget
        && event != EffectEvent::CastPlanned
        && event != EffectEvent::AttackCommitted
        && event != EffectEvent::UltimateCommitted
        && event != EffectEvent::AttackSpawned
        && !eventHasHitPayload(event)
        && !eventHasDamagePayload(event)
        && !eventHasCastAggregatePayload(event))
    {
        error = "原攻擊目標選擇器需要施放、攻擊、命中、傷害或施放聚合事件";
        return false;
    }
    if (selector.requiredTarget)
    {
        EffectSelector required;
        switch (*selector.requiredTarget)
        {
        case EffectRequiredTarget::Self: required.kind = EffectSelectorKind::Self; break;
        case EffectRequiredTarget::SourceUnit: required.kind = EffectSelectorKind::SourceUnit; break;
        case EffectRequiredTarget::TransactionTarget:
            required.kind = EffectSelectorKind::TransactionTarget;
            break;
        case EffectRequiredTarget::HitTarget: required.kind = EffectSelectorKind::HitTarget; break;
        case EffectRequiredTarget::OriginalAttackTarget:
            required.kind = EffectSelectorKind::OriginalAttackTarget;
            break;
        }
        if (!validateSelectorAtEvent(required, event, error))
        {
            error = "必含目標不支援此事件：" + error;
            return false;
        }
    }
    return true;
}

bool validateSelectorSchema(const EffectSelector& selector, std::string& error)
{
    if (selector.count < 0)
    {
        error = "目標數量不可為負數";
        return false;
    }
    if (selector.kind == EffectSelectorKind::UnitsInRadius && selector.radiusTiles <= 0)
    {
        error = "半徑選擇器需要正半徑";
        return false;
    }
    if (selector.kind == EffectSelectorKind::UnitsInSquare
        && (selector.squareSideTiles <= 0 || selector.squareSideTiles % 2 == 0))
    {
        error = "方形選擇器邊長必須是正奇數";
        return false;
    }
    if (selector.kind == EffectSelectorKind::AlliesUsingWeapon && selector.requiredWeaponType < 0)
    {
        error = "指定武器友軍需要非負武器類型";
        return false;
    }
    if (selector.requiredTarget)
    {
        switch (selector.kind)
        {
        case EffectSelectorKind::Self:
        case EffectSelectorKind::SourceUnit:
        case EffectSelectorKind::TransactionTarget:
        case EffectSelectorKind::HitTarget:
        case EffectSelectorKind::OriginalAttackTarget:
        case EffectSelectorKind::HighestMpEnemy:
        case EffectSelectorKind::FarthestEnemy:
            error = "單一目標選擇器不能再指定必含目標";
            return false;
        case EffectSelectorKind::ComboMembers:
        case EffectSelectorKind::AllLivingUnits:
        case EffectSelectorKind::Allies:
        case EffectSelectorKind::Enemies:
        case EffectSelectorKind::LowestHpAllies:
        case EffectSelectorKind::LowestMpAllies:
        case EffectSelectorKind::StrongestEnemies:
        case EffectSelectorKind::NearestEnemies:
        case EffectSelectorKind::UnitsInRadius:
        case EffectSelectorKind::UnitsInSquare:
        case EffectSelectorKind::AlliesUsingWeapon:
            break;
        }
    }
    return true;
}

bool selectorHasSingleResult(const EffectSelector& selector)
{
    switch (selector.kind)
    {
    case EffectSelectorKind::Self:
    case EffectSelectorKind::SourceUnit:
    case EffectSelectorKind::TransactionTarget:
    case EffectSelectorKind::HitTarget:
    case EffectSelectorKind::OriginalAttackTarget:
    case EffectSelectorKind::HighestMpEnemy:
    case EffectSelectorKind::FarthestEnemy:
        return true;
    case EffectSelectorKind::ComboMembers:
    case EffectSelectorKind::AllLivingUnits:
    case EffectSelectorKind::Allies:
    case EffectSelectorKind::Enemies:
    case EffectSelectorKind::LowestHpAllies:
    case EffectSelectorKind::LowestMpAllies:
    case EffectSelectorKind::StrongestEnemies:
    case EffectSelectorKind::NearestEnemies:
    case EffectSelectorKind::UnitsInRadius:
    case EffectSelectorKind::UnitsInSquare:
    case EffectSelectorKind::AlliesUsingWeapon:
        return selector.count == 1;
    }
    assert(false);
    return false;
}

bool validateConditionAtEvent(const EffectCondition& condition, EffectEvent event, std::string& error)
{
    return std::visit(
        [&](const auto& typed) -> bool
        {
            using T = std::decay_t<decltype(typed)>;
            const auto reject = [&](std::string message)
            {
                error = std::move(message);
                return false;
            };
            if constexpr (std::is_same_v<T, SourceHpRatioAtMostCondition>
                || std::is_same_v<T, SourceHpRatioBelowCondition>
                || std::is_same_v<T, TargetHpRatioAtMostCondition>)
            {
                if (typed.percent < 0 || typed.percent > 100) return reject("生命比例條件必須介於 0 與 100");
                if constexpr (std::is_same_v<T, TargetHpRatioAtMostCondition>)
                {
                    if (!eventHasHitPayload(event) && !eventHasDamagePayload(event) && !eventHasHealPayload(event))
                        return reject("目標生命條件需要命中、傷害或治療事件");
                }
            }
            else if constexpr (std::is_same_v<T, IsMainProjectileCondition>
                || std::is_same_v<T, IsRootAttackCondition>
                || std::is_same_v<T, AttackOrdinalEqualsCondition>)
            {
                if (event != EffectEvent::AttackSpawned && !eventHasHitPayload(event) && !eventHasDamagePayload(event))
                    return reject("攻擊來源條件需要攻擊、命中或傷害事件");
                if constexpr (std::is_same_v<T, AttackOrdinalEqualsCondition>)
                    if (typed.ordinal < 0) return reject("攻擊序號不可為負數");
            }
            else if constexpr (std::is_same_v<T, CastDistinctTargetCountAtLeastCondition>)
            {
                if (!eventHasCastAggregatePayload(event)) return reject("不同目標數條件需要施放聚合事件");
                if (typed.count <= 0) return reject("不同目標數門檻必須為正數");
            }
            else if constexpr (std::is_same_v<T, HealKindInCondition>)
            {
                if (!eventHasHealPayload(event)) return reject("治療種類條件需要治療事件");
                if (typed.kinds.empty()) return reject("治療種類條件不可為空");
            }
            else if constexpr (std::is_same_v<T, DamageOriginIsAttackCondition>)
            {
                if (!eventHasDamageOriginPayload(event)) return reject("傷害來源條件需要傷害、破盾或死亡事件");
            }
            else if constexpr (std::is_same_v<T, DamageKilledTargetCondition>)
            {
                if (!eventHasDamagePayload(event)) return reject("擊殺條件需要傷害結算事件");
            }
            else if constexpr (std::is_same_v<T, AcceptedHitCondition>)
            {
                if (!eventHasDamagePayload(event)) return reject("接受命中條件需要傷害結算事件");
            }
            else if constexpr (std::is_same_v<T, EventTargetBelongsToBoundSourceCondition>)
            {
                if (event != EffectEvent::AllyDied && event != EffectEvent::UnitDied)
                    return reject("來源成員條件需要死亡事件");
            }
            else if constexpr (std::is_same_v<T, DamagePerspectiveCondition>)
            {
                if (!eventHasDamagePayload(event)) return reject("傷害方位條件需要傷害結算事件");
            }
            else if constexpr (std::is_same_v<T, DamageKindInCondition>)
            {
                if (!eventHasHitPayload(event) && !eventHasDamagePayload(event)) return reject("傷害種類條件需要命中或傷害事件");
                if (typed.kinds.empty()) return reject("傷害種類條件不可為空");
            }
            else if constexpr (std::is_same_v<T, TargetMpWasFullBeforeCastCondition>)
            {
                if (event != EffectEvent::CastPlanned
                    && event != EffectEvent::AttackCommitted
                    && event != EffectEvent::UltimateCommitted)
                {
                    return reject("施放前滿內條件需要施放規劃或提交事件");
                }
            }
            else if constexpr (std::is_same_v<T, MagicIdEqualsCondition>)
            {
                if (typed.magicId < 0) return reject("武功 ID 不可為負數");
            }
            else if constexpr (std::is_same_v<T, SourceStackAtLeastCondition>)
            {
                if (typed.count <= 0) return reject("狀態層數門檻必須為正數");
            }
            return true;
        },
        condition);
}

bool validateEffectNumberAtEvent(const EffectNumber& number, EffectEvent event, std::string& error)
{
    const auto usesBase = [&](EffectNumberBase base)
    {
        return number.base == base || number.multiplierBase == base;
    };
    const bool usesStatus = usesBase(EffectNumberBase::SourceStatusPotency)
        || usesBase(EffectNumberBase::SourceStatusStacks);
    if (usesStatus != !number.status.empty())
    {
        error = usesStatus
            ? "來源狀態數值基準需要指定狀態"
            : "只有來源狀態數值基準可指定狀態";
        return false;
    }
    const bool usesStoredState = usesBase(EffectNumberBase::StoredStateValue);
    if (usesStoredState != number.stateSlot.has_value())
    {
        error = usesStoredState
            ? "狀態槽值基準需要指定狀態槽"
            : "只有狀態槽值基準可指定狀態槽";
        return false;
    }
    if (number.multiplierBase
        && (number.base == EffectNumberBase::Constant || *number.multiplierBase == EffectNumberBase::Constant))
    {
        error = "乘數基準只能與兩個非固定數值基準組合";
        return false;
    }
    if (number.minimum && number.maximum && *number.minimum > *number.maximum)
    {
        error = "數值最小值不可大於最大值";
        return false;
    }
    const auto needsFinalDamage = number.base == EffectNumberBase::FinalHpDamage
        || number.multiplierBase == EffectNumberBase::FinalHpDamage;
    if (needsFinalDamage && !eventHasDamagePayload(event))
    {
        error = "實際生命傷害公式只能用於傷害結算事件";
        return false;
    }
    const auto needsCurrentShield = number.base == EffectNumberBase::TargetCurrentShield
        || number.multiplierBase == EffectNumberBase::TargetCurrentShield;
    if (needsCurrentShield
        && !eventHasHitPayload(event)
        && !eventHasDamagePayload(event)
        && event != EffectEvent::ShieldBroken)
    {
        error = "目標目前護盾公式需要命中、傷害或破盾事件";
        return false;
    }
    const auto needsCurrentCooldown = number.base == EffectNumberBase::TargetCurrentCooldown
        || number.multiplierBase == EffectNumberBase::TargetCurrentCooldown;
    if (needsCurrentCooldown
        && event != EffectEvent::FrameAdvanced
        && event != EffectEvent::DamageResolved)
    {
        error = "目標目前冷卻公式需要每幀或傷害結算事件";
        return false;
    }
    return true;
}

bool effectNumberCannotBeNegative(const EffectNumber& number)
{
    if (number.maximum && *number.maximum < 0)
    {
        return false;
    }
    if (number.minimum && *number.minimum >= 0)
    {
        return true;
    }

    const bool scaledValueCanBeNegative = number.base != EffectNumberBase::Constant
        && number.percent < 0;
    return !scaledValueCanBeNegative && number.flat >= 0;
}

bool effectNumberMustBePositive(const EffectNumber& number)
{
    if (!effectNumberCannotBeNegative(number)
        || (number.maximum && *number.maximum <= 0))
    {
        return false;
    }
    return number.flat > 0
        || (number.minimum && *number.minimum > 0);
}

template<class Value>
bool isNonEmptyUniqueAllowList(const std::vector<Value>& values)
{
    if (values.empty())
    {
        return false;
    }
    std::set<Value> unique(values.begin(), values.end());
    return unique.size() == values.size();
}

bool validateActionPayload(const EffectAction& action, EffectEvent event, std::string& error)
{
    return std::visit(
        [&](const auto& typed) -> bool
        {
            using T = std::decay_t<decltype(typed)>;
            const auto reject = [&](std::string message)
            {
                error = std::move(message);
                return false;
            };
            if constexpr (std::is_same_v<T, ModifyAttributeAction>)
            {
                if (!validateEffectNumberAtEvent(typed.amount, event, error)) return false;
                if (typed.durationFrames < 0) return reject("屬性修正持續幀數不可為負數");
                if (typed.stack == EffectStackPolicy::AddStack && !typed.stackLimit)
                    return reject("增加層數的屬性修正需要層數上限");
                if (typed.stackLimit && *typed.stackLimit <= 0) return reject("屬性修正層數上限必須為正數");
                if (typed.stackLimit && typed.stack != EffectStackPolicy::AddStack)
                    return reject("只有增加層數的屬性修正可指定層數上限");
                if (typed.perStack && typed.stack != EffectStackPolicy::AddStack)
                    return reject("只有增加層數的屬性修正可指定每層計算");
            }
            else if constexpr (std::is_same_v<T, ModifyDamageAction>)
            {
                if (!validateEffectNumberAtEvent(typed.amount, event, error)) return false;
                if (typed.durationFrames < 0) return reject("傷害修正持續幀數不可為負數");
                if (typed.stack == EffectStackPolicy::AddStack && !typed.stackLimit)
                    return reject("增加層數的傷害修正需要層數上限");
                if (typed.stackLimit && *typed.stackLimit <= 0)
                    return reject("傷害修正層數上限必須為正數");
                if (typed.stackLimit && typed.stack != EffectStackPolicy::AddStack)
                    return reject("只有增加層數的傷害修正可指定層數上限");
                if (typed.operation == DamageModifierOperation::IgnoreDefensePercent
                    && (typed.perspective != DamageModifierPerspective::Outgoing
                        || typed.stage != DamageModifierStage::BeforeDefense))
                    return reject("忽略防禦百分比只支援造成方的防禦前階段");
                if (typed.operation == DamageModifierOperation::CapSingleHitAtMaxHpPercent
                    && (typed.perspective != DamageModifierPerspective::Incoming
                        || typed.stage != DamageModifierStage::Final))
                    return reject("單次承傷上限只支援承受方的最終階段");
                if (typed.operation == DamageModifierOperation::ExecuteBelowMaxHpPercent
                    && (typed.perspective != DamageModifierPerspective::Outgoing
                        || typed.stage != DamageModifierStage::Final))
                    return reject("處決門檻只支援造成方的最終階段");
                if (typed.operation == DamageModifierOperation::Multiply
                    && typed.perspective == DamageModifierPerspective::Incoming
                    && typed.stage == DamageModifierStage::BeforeDefense)
                    return reject("承受方的防禦前階段不支援乘算傷害修正");
            }
            else if constexpr (std::is_same_v<T, ChangeResourceAction>)
            {
                if (!validateEffectNumberAtEvent(typed.amount, event, error)) return false;
                if (!effectNumberCannotBeNegative(typed.amount))
                    return reject("資源變更數值不可產生負數");
                if ((typed.kind == ResourceChangeKind::Transfer) != typed.transferDestination.has_value())
                    return reject("只有轉移資源需要且必須提供轉移目標");
                if (typed.kind == ResourceChangeKind::RefreshToAtLeast
                    && typed.resource != BattleResource::Shield
                    && typed.resource != BattleResource::StatusShield
                    && typed.resource != BattleResource::StaggerShield
                    && typed.resource != BattleResource::ControlImmunityFrames
                    && typed.resource != BattleResource::InvincibilityFrames)
                    return reject("至少刷新只支援護盾、狀態護盾、僵直護盾、僵直吸收幀數或無敵幀數");
                if (typed.resource != BattleResource::Hp
                    && (typed.healKind != EffectHealKind::Direct
                        || typed.healSourcePolicy != EffectHealSourcePolicy::RequireAlive))
                    return reject("非生命資源不可指定治療種類或來源政策");
                if (typed.transferDestination)
                {
                    if (!validateSelectorSchema(*typed.transferDestination, error)
                        || !validateSelectorAtEvent(*typed.transferDestination, event, error)) return false;
                    if (!selectorHasSingleResult(*typed.transferDestination))
                        return reject("轉移目標選擇器必須只選取一個單位");
                }
            }
            else if constexpr (std::is_same_v<T, ModifyHealTransactionAction>)
            {
                if (typed.kinds.empty()) return reject("治療交易修正需要治療種類");
                if (typed.operation == HealModifierOperation::MultiplyReceived
                    && (typed.percent < 0 || typed.percent > 100)) return reject("治療乘算百分比必須介於 0 與 100");
            }
            else if constexpr (std::is_same_v<T, ApplyStatusAction>)
            {
                if (!validateEffectNumberAtEvent(typed.potency, event, error)
                    || !validateEffectNumberAtEvent(typed.secondaryPotency, event, error)) return false;
                if (typed.applicationCount)
                {
                    if (!validateEffectNumberAtEvent(*typed.applicationCount, event, error)) return false;
                    if (!effectNumberMustBePositive(*typed.applicationCount))
                        return reject("狀態套用次數必須保證為正數");
                }
                if (typed.duration
                    && (!validateEffectNumberAtEvent(*typed.duration, event, error)
                        || !effectNumberCannotBeNegative(*typed.duration))) return false;
                if (typed.durationFrames < 0)
                    return reject("狀態持續幀數不可為負數");
                if (typed.stacks <= 0) return reject("狀態層數必須為正數");
                if (typed.stack == EffectStackPolicy::AddStack && !typed.stackLimit)
                    return reject("增加層數的狀態需要層數上限");
                if (typed.stackLimit && *typed.stackLimit <= 0) return reject("狀態層數上限必須為正數");
                if (typed.aggregatePotencyWithinEvent
                    && (typed.status != BattleStatusKind::Poison
                        || typed.stack != EffectStackPolicy::KeepStrongest
                        || typed.secondaryPotency.base != EffectNumberBase::Constant
                        || typed.secondaryPotency.flat != 0
                        || !typed.stackLimit
                        || *typed.stackLimit != typed.stacks))
                {
                    return reject("同事件合計強度只支援保留最強的中毒，且必須明確指定相同的正層數與層數上限，不可指定次要強度");
                }
            }
            else if constexpr (std::is_same_v<T, ConsumeStatusAction>)
            {
                if (typed.stacks <= 0) return reject("消耗狀態層數必須為正數");
                if (typed.whenDepleted)
                {
                    if (typed.whenDepleted->applicationCount)
                        return reject("消耗最後一層套用的狀態不可指定套用次數");
                    EffectAction nested{ EffectActionValue{ *typed.whenDepleted } };
                    if (!validateActionPayload(nested, event, error)) return false;
                }
            }
            else if constexpr (std::is_same_v<T, RemoveStatusAction>)
            {
                if (typed.statuses.empty()
                    && !typed.negativeOnly
                    && !typed.controlOnly
                    && !typed.clearCurrentActionStagger)
                    return reject("移除狀態需要狀態列表或篩選條件");
                if (typed.count < 0) return reject("移除狀態數量不可為負數");
            }
            else if constexpr (std::is_same_v<T, DealDamageAction>)
            {
                if (!validateEffectNumberAtEvent(typed.amount, event, error)) return false;
                if (typed.transactionCount
                    && (!validateEffectNumberAtEvent(*typed.transactionCount, event, error)
                        || !effectNumberCannotBeNegative(*typed.transactionCount))) return false;
                if (typed.area.kind == DamageAreaKind::Circle && typed.area.radiusTiles <= 0)
                    return reject("圓形傷害範圍需要正半徑");
                if (typed.area.kind == DamageAreaKind::Square
                    && (typed.area.squareSideTiles <= 0 || typed.area.squareSideTiles % 2 == 0))
                    return reject("方形傷害範圍邊長必須是正奇數");
                if (typed.perCast.perTargetLimit < 0) return reject("同目標命中上限不可為負數");
                if (typed.areaProjectiles)
                {
                    const auto& delivery = *typed.areaProjectiles;
                    if (delivery.rangeTiles <= 0)
                        return reject("區域投射物範圍格數必須為正數");
                    if (delivery.maximumTargets < 0)
                        return reject("區域投射物最多目標不可為負數");
                    if (delivery.stunFrames < 0)
                        return reject("區域投射物眩暈幀數不可為負數");
                    if (typed.area.kind != DamageAreaKind::SingleTarget)
                        return reject("區域投射物不可再指定立即傷害範圍");
                    if (typed.perCast.perTargetLimit != 0)
                        return reject("區域投射物不可指定同施放命中上限");
                }
            }
            else if constexpr (std::is_same_v<T, ModifyAttackAction>)
            {
                if (typed.pattern.projectileCount <= 0) return reject("攻擊彈道數量必須為正數");
                if (typed.pattern.intervalFrames < 0) return reject("攻擊間隔幀數不可為負數");
                if (typed.strengthPct < 0 || typed.sameTargetHitLimit < 0) return reject("攻擊倍率與同目標上限不可為負數");
                if (typed.source)
                {
                    if (!validateSelectorSchema(*typed.source, error)
                        || !validateSelectorAtEvent(*typed.source, event, error)) return false;
                    if (!selectorHasSingleResult(*typed.source))
                        return reject("攻擊來源選擇器必須只選取一個單位");
                }
                if (typed.damageOverride && !validateEffectNumberAtEvent(*typed.damageOverride, event, error)) return false;
                if (!std::visit(
                        [&](const auto& behavior) -> bool
                        {
                            using B = std::decay_t<decltype(behavior)>;
                            if constexpr (std::is_same_v<B, std::monostate>)
                            {
                                return true;
                            }
                            else if constexpr (std::is_same_v<B, ProjectileBounceAttackBehavior>)
                            {
                                if (behavior.additionalHits > 0
                                    && behavior.chancePct >= 0
                                    && behavior.chancePct <= 100
                                    && behavior.rangePixels > 0) return true;
                                return reject("彈道彈射需要正追加命中次數、0 至 100 機率與正像素範圍");
                            }
                            else if constexpr (std::is_same_v<B, NearbyTrackingAttackBehavior>)
                            {
                                if (behavior.rangePixels > 0 && behavior.damagePct > 0) return true;
                                return reject("範圍追蹤需要正像素範圍與正傷害倍率");
                            }
                            else if constexpr (std::is_same_v<B, DelayedAlternateAttackBehavior>)
                            {
                                if (behavior.delayFrames > 0
                                    && behavior.damagePct > 0
                                    && behavior.attackerBlockGainChancePct >= 0
                                    && behavior.attackerBlockGainChancePct <= 100) return true;
                                return reject("延遲替代攻擊需要正延遲、正傷害倍率與 0 至 100 格擋機率");
                            }
                            else if constexpr (std::is_same_v<B, ExpandingSpiralAttackBehavior>)
                            {
                                if (behavior.projectileCount > 0 && behavior.bleedStacks > 0) return true;
                                return reject("擴張螺旋需要正彈道數量與正流血層數");
                            }
                        },
                        typed.runtimeBehavior)) return false;
            }
            else if constexpr (std::is_same_v<T, ForceMoveAction>)
            {
                if (typed.distanceTiles < 0 || typed.distancePixels < 0
                    || (typed.distanceTiles > 0) == (typed.distancePixels > 0))
                    return reject("強制移動必須且只能指定一種正距離");
                if (typed.lockFrames <= 0) return reject("強制移動鎖定幀數必須為正數");
            }
            else if constexpr (std::is_same_v<T, CreateAreaAction>)
            {
                if (typed.durationFrames <= 0 || typed.modifiers.empty()) return reject("區域需要正持續時間與非空修正");
                if (typed.shape == AreaShape::Circle && typed.radiusTiles <= 0) return reject("圓形區域需要正半徑");
                if (typed.shape == AreaShape::GridSquare
                    && (typed.squareSideTiles <= 0 || typed.squareSideTiles % 2 == 0))
                    return reject("棋格方形區域邊長必須是正奇數");
                for (const auto& modifier : typed.modifiers)
                    if (!validateEffectNumberAtEvent(modifier.amount, event, error)) return false;
            }
            else if constexpr (std::is_same_v<T, ModifyCastAction>)
            {
                if (!typed.mpCost
                    && !typed.rangeMode
                    && typed.projectileSpeedPct == 0
                    && typed.minimumSelectDistance == 0
                    && typed.additionalProjectiles == 0
                    && typed.mobility == CastMobilityPolicy::Preserve
                    && !typed.autoUltimate
                    && !typed.replacementPattern
                    && !typed.freeAdditionalCast)
                    return reject("修改施放至少需要一項變更");
                if (typed.mpCost && !validateEffectNumberAtEvent(*typed.mpCost, event, error)) return false;
                if (typed.projectileSpeedPct < 0
                    || typed.minimumSelectDistance < 0
                    || typed.additionalProjectiles < 0)
                    return reject("施放彈道速度、最小選擇距離與追加彈道數不可為負數");
            }
            else if constexpr (std::is_same_v<T, StateMachineAction>)
            {
                return std::visit(
                    [&](const auto& machine) -> bool
                    {
                        using M = std::decay_t<decltype(machine)>;
                        if constexpr (std::is_same_v<M, ChangeStateValueAction>)
                        {
                            if (machine.delta == 0) return reject("狀態值增量不可為零");
                            if (machine.minimum && machine.maximum
                                && *machine.minimum > *machine.maximum)
                                return reject("狀態值最小值不可大於最大值");
                        }
                        else if constexpr (std::is_same_v<M, TransferStateValueAction>)
                        {
                            if (machine.sourceSlot == machine.destinationSlot)
                                return reject("狀態值轉移的來源與目標狀態槽不可相同");
                            if ((machine.sourceSlot == EffectStateSlot::CastMaximumHpDamage
                                    || machine.destinationSlot == EffectStateSlot::CastMaximumHpDamage)
                                && !eventHasCastProvenance(event))
                                return reject("本次施放狀態槽需要具有施放識別的事件");
                        }
                        else if constexpr (std::is_same_v<M, ConsumeRecordedMaximumAction>)
                        {
                            if (machine.percent < 0) return reject("消耗記錄百分比不可為負數");
                        }
                        else if constexpr (std::is_same_v<M, StartDamageAbsorptionAction>)
                        {
                            if (machine.absorbedPct < 0 || machine.absorbedPct > 100 || machine.durationFrames <= 0)
                                return reject("傷害吸收需要 0 至 100 百分比與正持續時間");
                            if (machine.returnedPct < 0)
                                return reject("傷害吸收結算百分比不可為負數");
                            if (!validateSelectorSchema(machine.settlementTarget, error)
                                || !validateSelectorAtEvent(machine.settlementTarget, event, error)) return false;
                        }
                        else if constexpr (std::is_same_v<M, SettleDamageAbsorptionAction>)
                        {
                            if (machine.returnedPct < 0) return reject("傷害吸收結算百分比不可為負數");
                            if (!validateSelectorSchema(machine.target, error)
                                || !validateSelectorAtEvent(machine.target, event, error)) return false;
                        }
                        else if constexpr (std::is_same_v<M, BorrowEffectRulesAction>)
                        {
                            if (event != EffectEvent::CastPlanned)
                                return reject("借用效果規則只允許用於施放規劃事件");
                            if (machine.propagation != CastPropagationPolicy::BorrowedUltimateRules)
                                return reject("借用效果規則必須使用借用大招規則傳播政策");
                            if (!validateEffectNumberAtEvent(machine.sourceCount, event, error)
                                || !validateSelectorSchema(machine.sourceUnits, error)
                                || !validateSelectorAtEvent(machine.sourceUnits, event, error)) return false;
                            if (!effectNumberCannotBeNegative(machine.sourceCount))
                                return reject("借用效果規則的來源數量不可產生負數");
                            if (!isNonEmptyUniqueAllowList(
                                    machine.filter.allowedActionCategories))
                                return reject("借用效果規則需要非空且不重複的動作類別 allow-list");
                        }
                        else if constexpr (std::is_same_v<M, CopyAttackDefinitionAction>)
                        {
                            if (event != EffectEvent::UltimateCommitted)
                                return reject("複製攻擊定義只允許用於絕招提交事件");
                            if (machine.propagation != CastPropagationPolicy::SuppressUltimateRules)
                                return reject("複製攻擊定義必須使用不傳播大招規則傳播政策");
                            if (!machine.sourceUnits.excludeOwner)
                                return reject("複製攻擊定義的來源選擇器必須排除效果擁有者");
                            if (machine.copyCount <= 0) return reject("複製攻擊數量必須為正數");
                            if (!validateSelectorSchema(machine.sourceUnits, error)
                                || !validateSelectorAtEvent(machine.sourceUnits, event, error)) return false;
                            if (!isNonEmptyUniqueAllowList(machine.filter.conditions))
                                return reject("複製攻擊定義需要非空且不重複的可選武功條件 allow-list");
                            if (!std::ranges::contains(
                                    machine.filter.conditions,
                                    CopiedMagicCondition::HasUltimateAttackDefinition))
                                return reject("複製攻擊定義必須限制為有絕招攻擊定義的武功");
                            if (!std::ranges::contains(
                                    machine.filter.conditions,
                                    CopiedMagicCondition::ExcludesRecursiveEffects))
                                return reject("複製攻擊定義必須排除複製與借用遞迴");
                        }
                        else if constexpr (std::is_same_v<M, SettleRemainingStatusDamageAction>)
                        {
                            if (machine.status != BattleStatusKind::Poison)
                                return reject("剩餘狀態傷害結算目前只支援中毒");
                        }
                        else if constexpr (std::is_same_v<M, GenerateClonesAction>)
                        {
                            if (event != EffectEvent::BattleInitialized)
                                return reject("生成分身只允許用於戰鬥初始化事件");
                            if (machine.count <= 0) return reject("生成分身數量必須為正數");
                        }
                        else if constexpr (std::is_same_v<M, PreventDeathAction>)
                        {
                            if (event != EffectEvent::BattleInitialized)
                                return reject("死亡庇護只允許用於戰鬥初始化事件");
                            if (machine.invincibilityFrames <= 0)
                                return reject("死亡庇護無敵幀數必須為正數");
                        }
                        else if constexpr (std::is_same_v<M, ConfigureRescueRepositionAction>)
                        {
                            if (event != EffectEvent::BattleInitialized)
                                return reject("挪移次數只允許用於戰鬥初始化事件");
                            if (machine.activations <= 0) return reject("挪移次數必須為正數");
                        }
                        return true;
                    },
                    typed);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
            {
                if (!typed || typed->conditions.empty() || typed->whenTrue.empty())
                    return reject("條件分支需要條件與成立動作");
                for (const auto& condition : typed->conditions)
                    if (!validateConditionAtEvent(condition, event, error)) return false;
                for (const auto& nested : typed->whenTrue)
                    if (!validateActionPayload(nested, event, error)) return false;
                for (const auto& nested : typed->whenFalse)
                    if (!validateActionPayload(nested, event, error)) return false;
            }
            return true;
        },
        action.value);
}

bool isActionAllowedAtEvent(const EffectAction& action, EffectEvent event, std::string& error)
{
    return std::visit(
        [&](const auto& typed) -> bool
        {
            using T = std::decay_t<decltype(typed)>;
            auto reject = [&](std::string message)
            {
                error = std::move(message);
                return false;
            };

            if constexpr (std::is_same_v<T, ModifyHealTransactionAction>)
            {
                return event == EffectEvent::HealAttempted
                    || reject("治療交易修正只能用於 HealAttempted");
            }
            else if constexpr (std::is_same_v<T, ModifyCastAction>)
            {
                if (event == EffectEvent::CastPlanned) return true;
                if (event == EffectEvent::CastContinuation
                    && typed.freeAdditionalCast
                    && !typed.mpCost
                    && !typed.rangeMode
                    && typed.projectileSpeedPct == 0
                    && typed.minimumSelectDistance == 0
                    && typed.additionalProjectiles == 0
                    && typed.mobility == CastMobilityPolicy::Preserve
                    && !typed.autoUltimate
                    && !typed.replacementPattern)
                {
                    return true;
                }
                if ((event == EffectEvent::FrameAdvanced || event == EffectEvent::ShieldBroken)
                    && typed.autoUltimate
                    && !typed.mpCost
                    && !typed.rangeMode
                    && typed.projectileSpeedPct == 0
                    && typed.minimumSelectDistance == 0
                    && typed.additionalProjectiles == 0
                    && typed.mobility == CastMobilityPolicy::Preserve
                    && !typed.replacementPattern
                    && !typed.freeAdditionalCast)
                {
                    return true;
                }
                return reject("修改施放只能用於施放規劃；施放延續只允許免費 child cast；每幀與護盾破裂只允許自動絕招");
            }
            else if constexpr (std::is_same_v<T, ModifyAttackAction>)
            {
                return event == EffectEvent::CastPlanned
                    || event == EffectEvent::AttackCommitted
                    || event == EffectEvent::UltimateCommitted
                    || event == EffectEvent::AttackSpawned
                    || event == EffectEvent::MainProjectileBeforeDamage
                    || event == EffectEvent::CastContinuation
                    || reject("修改攻擊不支援此事件");
            }
            else if constexpr (std::is_same_v<T, ModifyDamageAction>)
            {
                return event == EffectEvent::BattleInitialized
                    || event == EffectEvent::UltimateCommitted
                    || event == EffectEvent::HitBeforeDamage
                    || event == EffectEvent::MainProjectileBeforeDamage
                    || event == EffectEvent::DamageResolved
                    || reject("傷害修正不支援此事件");
            }
            else if constexpr (std::is_same_v<T, ForceMoveAction>)
            {
                return event == EffectEvent::HitBeforeDamage
                    || event == EffectEvent::MainProjectileBeforeDamage
                    || reject("強制移動只允許命中傷害前事件");
            }
            else if constexpr (std::is_same_v<T, CreateAreaAction>)
            {
                return event == EffectEvent::AttackCommitted
                    || event == EffectEvent::UltimateCommitted
                    || event == EffectEvent::MainProjectileBeforeDamage
                    || event == EffectEvent::HitBeforeDamage
                    || event == EffectEvent::UnitDied
                    || reject("建立區域不支援此事件");
            }
            else if constexpr (std::is_same_v<T, DealDamageAction>)
            {
                return event != EffectEvent::CastPlanned
                    && event != EffectEvent::AttackSpawned
                    && event != EffectEvent::HealAttempted
                    || reject("造成傷害不支援此事件");
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
            {
                if (!typed) return reject("條件動作不可為空");
                for (const auto& nested : typed->whenTrue)
                    if (!isActionAllowedAtEvent(nested, event, error)) return false;
                for (const auto& nested : typed->whenFalse)
                    if (!isActionAllowedAtEvent(nested, event, error)) return false;
                return true;
            }
            else
            {
                return event != EffectEvent::HealAttempted
                    || reject("HealAttempted 只允許治療交易修正");
            }
        },
        action.value);
}

enum class ExactRuntimeActionClass
{
    Ordinary,
    Exact,
    UnsupportedComposition,
};

ExactRuntimeActionClass exactRuntimeActionClass(const EffectAction& action)
{
    return std::visit([&](const auto& typed)
    {
        using T = std::decay_t<decltype(typed)>;
        if constexpr (std::is_same_v<T, ModifyCastAction>)
        {
            const bool exact = typed.rangeMode == CastRangeMode::Ranged
                || typed.projectileSpeedPct > 0
                || typed.minimumSelectDistance > 0
                || typed.additionalProjectiles > 0
                || typed.mobility != CastMobilityPolicy::Preserve;
            if (!exact)
            {
                return ExactRuntimeActionClass::Ordinary;
            }
            const bool hasOrdinaryFields = typed.mpCost.has_value()
                || typed.autoUltimate.has_value()
                || typed.replacementPattern.has_value()
                || typed.freeAdditionalCast
                || typed.propagation != CastPropagationPolicy::SourceRules;
            return hasOrdinaryFields
                ? ExactRuntimeActionClass::UnsupportedComposition
                : ExactRuntimeActionClass::Exact;
        }
        else if constexpr (std::is_same_v<T, ModifyAttackAction>)
        {
            const bool exact = !std::holds_alternative<std::monostate>(typed.runtimeBehavior)
                && !std::holds_alternative<ExpandingSpiralAttackBehavior>(typed.runtimeBehavior);
            if (!exact)
            {
                return ExactRuntimeActionClass::Ordinary;
            }
            const bool hasOrdinaryFields = typed.pattern.kind != AttackPatternKind::Preserve
                || typed.pattern.projectileCount != 1
                || typed.pattern.spreadDegrees != 0
                || typed.pattern.intervalFrames != 0
                || typed.strengthPct != 100
                || typed.through.has_value()
                || typed.tracking.has_value()
                || !typed.mainProjectile
                || typed.sameTargetHitLimit != 0
                || typed.targets != AttackTargetPolicy::Preserve
                || typed.propagation != CastPropagationPolicy::SourceRules
                || typed.addToBaseAttack
                || typed.source.has_value()
                || typed.damageOverride.has_value()
                || typed.damageKind.has_value();
            return hasOrdinaryFields
                ? ExactRuntimeActionClass::UnsupportedComposition
                : ExactRuntimeActionClass::Exact;
        }
        else if constexpr (std::is_same_v<T, ForceMoveAction>)
        {
            return typed.distancePixels > 0
                ? ExactRuntimeActionClass::Exact
                : ExactRuntimeActionClass::Ordinary;
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
        {
            assert(typed);
            const auto containsExact = [&](const auto& actions)
            {
                return std::ranges::any_of(actions, [](const EffectAction& nested)
                {
                    return exactRuntimeActionClass(nested)
                        != ExactRuntimeActionClass::Ordinary;
                });
            };
            return containsExact(typed->whenTrue) || containsExact(typed->whenFalse)
                ? ExactRuntimeActionClass::UnsupportedComposition
                : ExactRuntimeActionClass::Ordinary;
        }
        else
        {
            return ExactRuntimeActionClass::Ordinary;
        }
    }, action.value);
}

bool exactRuntimeActionConsumesRuleChance(const EffectAction& action)
{
    if (const auto* movement = std::get_if<ForceMoveAction>(&action.value))
    {
        return movement->distancePixels > 0;
    }
    const auto* attack = std::get_if<ModifyAttackAction>(&action.value);
    return attack
        && std::holds_alternative<NearbyTrackingAttackBehavior>(
            attack->runtimeBehavior);
}

bool validateBattleInitializedNumber(const EffectNumber& number, std::string& error)
{
    const auto supported = [](EffectNumberBase base)
    {
        switch (base)
        {
        case EffectNumberBase::Constant:
        case EffectNumberBase::SourceStar:
        case EffectNumberBase::SourceAttack:
        case EffectNumberBase::SourceMaxHp:
        case EffectNumberBase::SourceCurrentMpRatio:
        case EffectNumberBase::TargetMaxHp:
        case EffectNumberBase::TargetCurrentHp:
            return true;
        case EffectNumberBase::SourceMissingHpRatio:
        case EffectNumberBase::TargetCurrentShield:
        case EffectNumberBase::TargetCurrentCooldown:
        case EffectNumberBase::FinalHpDamage:
        case EffectNumberBase::AccumulatedStateValue:
        case EffectNumberBase::SourceStatusPotency:
        case EffectNumberBase::SourceStatusStacks:
        case EffectNumberBase::StoredStateValue:
            return false;
        }
        return false;
    };
    if (!supported(number.base)
        || (number.multiplierBase && !supported(*number.multiplierBase)))
    {
        error = "戰鬥初始化數值公式使用了初始化快照未提供的資料";
        return false;
    }
    return true;
}

bool validateBattleInitializedSelector(const EffectSelector& selector, std::string& error)
{
    if (selector.tieBreak != EffectTieBreak::UnitId)
    {
        error = "戰鬥初始化目標選擇必須使用單位 ID 平手規則";
        return false;
    }
    if (selector.requiredMagicId >= 0 || selector.requiredWeaponType >= 0)
    {
        error = "戰鬥初始化快照不提供武功或武器篩選資料";
        return false;
    }
    if ((selector.kind == EffectSelectorKind::Self
            || selector.kind == EffectSelectorKind::SourceUnit)
        && selector.excludeOwner)
    {
        error = "戰鬥初始化的自身或來源單位不可排除效果擁有者";
        return false;
    }
    switch (selector.kind)
    {
    case EffectSelectorKind::Self:
    case EffectSelectorKind::SourceUnit:
    case EffectSelectorKind::ComboMembers:
    case EffectSelectorKind::AllLivingUnits:
    case EffectSelectorKind::Allies:
    case EffectSelectorKind::Enemies:
    case EffectSelectorKind::LowestHpAllies:
    case EffectSelectorKind::LowestMpAllies:
    case EffectSelectorKind::HighestMpEnemy:
    case EffectSelectorKind::StrongestEnemies:
    case EffectSelectorKind::NearestEnemies:
    case EffectSelectorKind::FarthestEnemy:
    case EffectSelectorKind::UnitsInRadius:
    case EffectSelectorKind::UnitsInSquare:
        return true;
    case EffectSelectorKind::TransactionTarget:
    case EffectSelectorKind::HitTarget:
    case EffectSelectorKind::OriginalAttackTarget:
    case EffectSelectorKind::AlliesUsingWeapon:
        error = "戰鬥初始化不支援此目標選擇器";
        return false;
    }
    return false;
}

bool validateBattleInitializedCondition(const EffectCondition& condition, std::string& error)
{
    if (std::holds_alternative<SourceIsLastAliveCondition>(condition)
        || std::holds_alternative<TargetNotInvincibleCondition>(condition))
    {
        return true;
    }
    error = "戰鬥初始化條件使用了初始化事件未提供或固定不變的資料";
    return false;
}

bool initializedCoreAttribute(BattleAttribute attribute)
{
    return attribute == BattleAttribute::MaxHp
        || attribute == BattleAttribute::Attack
        || attribute == BattleAttribute::Defence
        || attribute == BattleAttribute::Speed;
}

bool validateBattleInitializedAction(
    const EffectAction& action,
    const EffectSelector& ruleSelector,
    std::string& error)
{
    const auto reject = [&](std::string message)
    {
        error = std::move(message);
        return false;
    };
    return std::visit([&](const auto& typed) -> bool
    {
        using T = std::decay_t<decltype(typed)>;
        if constexpr (std::is_same_v<T, ModifyAttributeAction>)
        {
            if (!validateBattleInitializedNumber(typed.amount, error)) return false;
            if (initializedCoreAttribute(typed.attribute)
                && (typed.operation != AttributeOperation::FlatAdd
                    && typed.operation != AttributeOperation::PercentAdd))
                return reject("初始化核心屬性只支援固定加算或百分比加算");
            if (initializedCoreAttribute(typed.attribute)
                && (typed.durationFrames != 0
                    || typed.stack != EffectStackPolicy::Independent
                    || typed.stackLimit
                    || typed.perStack
                    || typed.stackScope != EffectStackScope::Shared))
                return reject("初始化核心屬性必須是永久、共用且獨立的修正");
            return true;
        }
        else if constexpr (std::is_same_v<T, ModifyDamageAction>)
        {
            return validateBattleInitializedNumber(typed.amount, error);
        }
        else if constexpr (std::is_same_v<T, ChangeResourceAction>)
        {
            if (!validateBattleInitializedNumber(typed.amount, error)) return false;
            if (typed.kind == ResourceChangeKind::Drain
                || typed.kind == ResourceChangeKind::Transfer)
                return reject("戰鬥初始化不支援奪取或轉移資源");
            if (typed.resource == BattleResource::Hp
                || typed.resource == BattleResource::ActiveCooldown)
                return reject("戰鬥初始化不可變更生命或目前冷卻");
            return true;
        }
        else if constexpr (std::is_same_v<T, ApplyStatusAction>)
        {
            if (!validateBattleInitializedNumber(typed.potency, error)
                || !validateBattleInitializedNumber(typed.secondaryPotency, error))
                return false;
            if (typed.duration
                && !validateBattleInitializedNumber(*typed.duration, error))
                return false;
            return !typed.applicationCount
                || validateBattleInitializedNumber(*typed.applicationCount, error);
        }
        else if constexpr (std::is_same_v<T, StateMachineAction>)
        {
            return std::visit([&](const auto& machine) -> bool
            {
                using M = std::decay_t<decltype(machine)>;
                if constexpr (std::is_same_v<M, GenerateClonesAction>)
                {
                    return ruleSelector.kind == EffectSelectorKind::Self
                        || reject("生成分身的戰鬥初始化規則必須以自身為目標");
                }
                else if constexpr (std::is_same_v<M, PreventDeathAction>
                    || std::is_same_v<M, ConfigureRescueRepositionAction>)
                {
                    return true;
                }
                else
                {
                    return reject("戰鬥初始化不支援此狀態機機制");
                }
            }, typed);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
        {
            assert(typed);
            for (const auto& condition : typed->conditions)
                if (!validateBattleInitializedCondition(condition, error)) return false;
            for (const auto& nested : typed->whenTrue)
                if (!validateBattleInitializedAction(nested, ruleSelector, error)) return false;
            for (const auto& nested : typed->whenFalse)
                if (!validateBattleInitializedAction(nested, ruleSelector, error)) return false;
            return true;
        }
        else
        {
            return reject("戰鬥初始化不支援此動作類型");
        }
    }, action.value);
}

bool validateBattleInitializedRule(const EffectRule& rule, std::string& error)
{
    if (rule.observation != EffectObservationScope::Owner)
    {
        error = "戰鬥初始化只支援效果擁有者觀察";
        return false;
    }
    if (rule.castMatch != EffectCastMatch::BoundMagic)
    {
        error = "戰鬥初始化不提供施放識別";
        return false;
    }
    if (rule.chancePct != 100
        || rule.maxActivations != 0
        || rule.sharedCooldownFrames != 0
        || rule.intervalFrames != 0
        || rule.everyNthEvent != 0
        || rule.repetitionCount
        || rule.activationLimit)
    {
        error = "戰鬥初始化必須省略機率、次數、冷卻、間隔、重複與觸發限制欄位";
        return false;
    }
    if (!validateBattleInitializedSelector(rule.selector, error)) return false;
    for (const auto& condition : rule.conditions)
        if (!validateBattleInitializedCondition(condition, error)) return false;
    for (const auto& action : rule.actions)
        if (!validateBattleInitializedAction(action, rule.selector, error)) return false;
    return true;
}

}  // namespace

std::string effectDescription(const EffectRule& rule, EffectDescriptionStyle style)
{
    const auto ast = buildEffectDescriptionAst(rule);
    return style == EffectDescriptionStyle::Full
        ? renderFullEffectDescription(ast)
        : renderCompactEffectDescription(ast);
}

bool validateEffectRule(const EffectRule& rule, std::string& error)
{
    error.clear();
    if (rule.castMatch == EffectCastMatch::OwnerAnyCast
        && rule.observation != EffectObservationScope::Owner)
    {
        error = "效果擁有者任意施放匹配只支援效果擁有者觀察";
        return false;
    }
    if (rule.castMatch == EffectCastMatch::OwnerAnyCast
        && !eventHasCastProvenance(rule.event))
    {
        error = "效果擁有者任意施放匹配需要具有施放識別的事件";
        return false;
    }
    if (rule.observation == EffectObservationScope::OwnerTeamEventSource
        && rule.event != EffectEvent::MainProjectileBeforeDamage
        && rule.event != EffectEvent::HitBeforeDamage)
    {
        error = "同隊事件來源觀察目前只支援命中傷害前事件";
        return false;
    }
    if (rule.chancePct < 0 || rule.chancePct > 100)
    {
        error = "觸發機率必須介於 0 與 100";
        return false;
    }
    if (rule.maxActivations < 0)
    {
        error = "啟用次數上限不可為負數";
        return false;
    }
    if (rule.sharedCooldownFrames < 0)
    {
        error = "同來源冷卻幀數不可為負數";
        return false;
    }
    if (rule.intervalFrames < 0)
    {
        error = "間隔幀數不可為負數";
        return false;
    }
    if (rule.intervalFrames > 0 && rule.event != EffectEvent::FrameAdvanced)
    {
        error = "間隔幀數只允許用於每幀事件";
        return false;
    }
    if (rule.everyNthEvent < 0)
    {
        error = "每N次事件不可為負數";
        return false;
    }
    if (rule.everyNthEvent == 1)
    {
        error = "每N次事件為1沒有意義，請省略此欄位";
        return false;
    }
    if (rule.repetitionCount)
    {
        if (!validateEffectNumberAtEvent(*rule.repetitionCount, rule.event, error))
            return false;
        if (!effectNumberMustBePositive(*rule.repetitionCount))
        {
            error = "規則重複次數必須保證為正數";
            return false;
        }
        const auto targetScoped = [](EffectNumberBase base)
        {
            return base == EffectNumberBase::TargetMaxHp
                || base == EffectNumberBase::TargetCurrentHp
                || base == EffectNumberBase::TargetCurrentShield
                || base == EffectNumberBase::TargetCurrentCooldown;
        };
        if (targetScoped(rule.repetitionCount->base)
            || (rule.repetitionCount->multiplierBase
                && targetScoped(*rule.repetitionCount->multiplierBase)))
        {
            error = "規則重複次數必須使用來源或事件範圍的數值，不可依個別目標而異";
            return false;
        }
    }
    if (rule.activationLimit)
    {
        if (rule.activationLimit->maxEvaluations <= 0)
        {
            error = "觸發限制次數必須大於零";
            return false;
        }
        switch (rule.activationLimit->scope)
        {
        case EffectActivationScope::PerCastPerTarget:
            if (!eventHasCastProvenance(rule.event))
            {
                error = "每次施放每個目標的觸發限制需要具有施放識別的事件";
                return false;
            }
            break;
        default:
            error = "未知的效果觸發限制範圍";
            return false;
        }
    }
    if (rule.actions.empty())
    {
        error = "規則至少需要一個動作";
        return false;
    }
    if (!validateSelectorSchema(rule.selector, error)
        || !validateSelectorAtEvent(rule.selector, rule.event, error)) return false;
    for (const auto& condition : rule.conditions)
    {
        if (!validateConditionAtEvent(condition, rule.event, error)) return false;
    }
    if (rule.event == EffectEvent::DamageResolved
        && std::ranges::any_of(rule.conditions, [](const EffectCondition& condition)
            {
                return std::holds_alternative<IsMainProjectileCondition>(condition);
            })
        && !std::ranges::any_of(rule.conditions, [](const EffectCondition& condition)
            {
                return std::holds_alternative<DamageOriginIsAttackCondition>(condition);
            }))
    {
        error = "傷害結算事件使用主彈道條件時也需要「傷害來自招式」條件";
        return false;
    }
    for (const auto& action : rule.actions)
    {
        if (!isActionAllowedAtEvent(action, rule.event, error)
            || !validateActionPayload(action, rule.event, error)) return false;
    }
    if (rule.event == EffectEvent::BattleInitialized
        && !validateBattleInitializedRule(rule, error)) return false;
    const bool hasExactRuntimeAction = std::ranges::any_of(
        rule.actions,
        [](const EffectAction& action)
        {
            return exactRuntimeActionClass(action) == ExactRuntimeActionClass::Exact;
        });
    const bool hasUnsupportedComposition = std::ranges::any_of(
        rule.actions,
        [](const EffectAction& action)
        {
            return exactRuntimeActionClass(action)
                == ExactRuntimeActionClass::UnsupportedComposition;
        });
    const bool hasOrdinaryAction = std::ranges::any_of(
        rule.actions,
        [](const EffectAction& action)
        {
            return exactRuntimeActionClass(action) == ExactRuntimeActionClass::Ordinary;
        });
    if (hasUnsupportedComposition || (hasExactRuntimeAction && hasOrdinaryAction))
    {
        error = "精確 runtime 階段動作不可與一般動作混合，也不可放在條件分支內；請拆成不同效果規則";
        return false;
    }
    const bool exactRuntimeChanceIsConsumed = rule.actions.size() == 1
        && exactRuntimeActionConsumesRuleChance(rule.actions.front());
    if (hasExactRuntimeAction
        && ((rule.chancePct != 100 && !exactRuntimeChanceIsConsumed)
            || rule.everyNthEvent > 0
            || rule.repetitionCount
            || rule.activationLimit
            || rule.maxActivations > 0
            || rule.sharedCooldownFrames > 0))
    {
        error = "精確 runtime 階段規則不可設定其消費端未直接處理的規則機率、每N次事件、觸發限制、觸發次數或同來源冷卻；runtime 消費端不共用一般規則的觸發記帳";
        return false;
    }
    return true;
}

bool ChessBattleEffects::parseEffectRule(
    const YAML::Node& node,
    EffectRule& out,
    EffectRuleId id,
    const std::string& context,
    const ChessDiagnosticSink& diagnostics)
{
    const auto mark = node.Mark();
    auto fail = [&](const std::string& message)
    {
        const auto detail = !mark.is_null()
            ? std::format("「{}」(第{}行，第{}列) {}", context, mark.line + 1, mark.column + 1, message)
            : std::format("「{}」{}", context, message);
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "效果規則", detail);
        return false;
    };

    try
    {
        std::string error;
        if (!validateKnownKeys(node, { "事件", "觀察範圍", "施放匹配", "目標", "條件", "機率", "次數", "同來源冷卻幀數", "間隔幀數", "每N次事件", "觸發限制", "重複次數", "動作" }, error))
            return fail(error);

        std::string eventLabel;
        if (!requiredString(node, "事件", eventLabel, error)) return fail(error);
        const auto event = parseLabel<EffectEvent>(eventLabel, {
            { "常駐", EffectEvent::BattleInitialized },
            { "戰鬥初始化", EffectEvent::BattleInitialized },
            { "每幀", EffectEvent::FrameAdvanced },
            { "絕招冷卻完成", EffectEvent::UltimateCooldownFinished },
            { "施放規劃", EffectEvent::CastPlanned },
            { "攻擊提交", EffectEvent::AttackCommitted },
            { "絕招提交", EffectEvent::UltimateCommitted },
            { "攻擊生成", EffectEvent::AttackSpawned },
            { "主彈道命中傷害前", EffectEvent::MainProjectileBeforeDamage },
            { "命中傷害前", EffectEvent::HitBeforeDamage },
            { "傷害結算後", EffectEvent::DamageResolved },
            { "治療嘗試", EffectEvent::HealAttempted },
            { "治療套用", EffectEvent::HealApplied },
            { "施放延續", EffectEvent::CastContinuation },
            { "施放結算完成", EffectEvent::CastSettled },
            { "護盾破裂", EffectEvent::ShieldBroken },
            { "單位死亡", EffectEvent::UnitDied },
            { "友軍死亡", EffectEvent::AllyDied },
        });
        if (!event) return fail(std::format("未知事件「{}」", eventLabel));

        out = {};
        out.id = id;
        out.event = *event;
        if (const auto observation = node["觀察範圍"])
        {
            const auto label = observation.as<std::string>();
            if (label == "效果擁有者") out.observation = EffectObservationScope::Owner;
            else if (label == "效果擁有者同隊事件來源")
                out.observation = EffectObservationScope::OwnerTeamEventSource;
            else if (label == "事件目標")
                out.observation = EffectObservationScope::EventTarget;
            else return fail(std::format("未知觀察範圍「{}」", label));
        }
        if (const auto castMatch = node["施放匹配"])
        {
            const auto label = castMatch.as<std::string>();
            if (label == "綁定武功") out.castMatch = EffectCastMatch::BoundMagic;
            else if (label == "效果擁有者任意施放")
                out.castMatch = EffectCastMatch::OwnerAnyCast;
            else return fail(std::format("未知施放匹配「{}」", label));
        }
        if (node["目標"] && !parseSelectorNode(node["目標"], out.selector, error)) return fail(error);
        if (const auto conditions = node["條件"])
        {
            if (!conditions.IsSequence()) return fail("「條件」必須是列表");
            for (std::size_t index = 0; index < conditions.size(); ++index)
            {
                EffectCondition condition;
                if (!parseConditionNode(conditions[index], condition, error))
                    return fail(std::format("條件#{}: {}", index + 1, error));
                out.conditions.push_back(std::move(condition));
            }
        }
        if (!optionalInt(node, "機率", out.chancePct, error)
            || !optionalInt(node, "次數", out.maxActivations, error)
            || !optionalInt(node, "同來源冷卻幀數", out.sharedCooldownFrames, error)
            || !optionalInt(node, "間隔幀數", out.intervalFrames, error)
            || !optionalInt(node, "每N次事件", out.everyNthEvent, error)) return fail(error);
        if (const auto activationLimit = node["觸發限制"])
        {
            EffectActivationLimit parsed;
            if (!parseActivationLimitNode(activationLimit, parsed, error)) return fail(error);
            out.activationLimit = parsed;
        }
        if (node["重複次數"])
        {
            EffectNumber count;
            if (!parseEffectNumberNode(node["重複次數"], count, error)) return fail(error);
            out.repetitionCount = std::move(count);
        }
        if (!parseActionList(node["動作"], out.actions, error)) return fail(error);
        if (!validateEffectRule(out, error)) return fail(error);
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        return fail(std::format("解析效果規則時發生 YAML 異常: {}", ex.what()));
    }
}

namespace
{

bool reportMagicLoadError(
    const YAML::Node& node,
    const std::string& context,
    const std::string& message,
    const ChessDiagnosticSink& diagnostics)
{
    const auto mark = node.Mark();
    if (!mark.is_null())
    {
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "武功效果", std::format("「{}」(第{}行，第{}列) {}", context, mark.line + 1, mark.column + 1, message));
    }
    else
    {
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "武功效果", std::format("「{}」{}", context, message));
    }
    return false;
}

}  // namespace

bool ChessBattleEffects::parseMagicEffects(
    const YAML::Node& root,
    std::vector<ChessMagicEffectDefinition>& out,
    const std::string& context,
    const ChessDiagnosticSink& diagnostics)
{
    out.clear();
    if (!root || !root.IsMap())
    {
        return reportMagicLoadError(root, context, "根節點必須是映射表", diagnostics);
    }

    bool effectsEnabled = true;
    if (const auto enabled = root["啟用"])
    {
        try
        {
            effectsEnabled = enabled.as<bool>();
        }
        catch (const YAML::Exception& ex)
        {
            return reportMagicLoadError(enabled, context, std::format("「啟用」欄位不是有效布林值: {}", ex.what()), diagnostics);
        }
    }

    std::string rootError;
    if (!validateKnownKeys(root, { "啟用", "絕招" }, rootError))
    {
        return reportMagicLoadError(root, context, rootError, diagnostics);
    }
    const auto entries = root["絕招"];
    if (!entries)
    {
        return reportMagicLoadError(root, context, "缺少「絕招」根節點", diagnostics);
    }
    if (!entries.IsSequence())
    {
        return reportMagicLoadError(entries, context, "「絕招」必須是列表", diagnostics);
    }

    std::set<int> seenMagicIds;
    std::vector<ChessMagicEffectDefinition> parsedDefinitions;
    for (std::size_t definitionIndex = 0; definitionIndex < entries.size(); ++definitionIndex)
    {
        const auto entryNode = entries[definitionIndex];
        std::string entryError;
        if (!validateKnownKeys(entryNode, { "武功", "名稱", "效果" }, entryError))
            return reportMagicLoadError(entryNode, context, entryError, diagnostics);

        ChessMagicEffectDefinition definition;
        definition.enabled = effectsEnabled;
        try
        {
            if (!entryNode["武功"] || !entryNode["名稱"])
                return reportMagicLoadError(entryNode, context, "絕招項目需要「武功」與「名稱」", diagnostics);
            definition.magicId = entryNode["武功"].as<int>();
            definition.name = entryNode["名稱"].as<std::string>();
            definition.purpose = "絕招";
        }
        catch (const YAML::Exception& ex)
        {
            return reportMagicLoadError(entryNode, context, std::format("絕招欄位解析失敗: {}", ex.what()), diagnostics);
        }
        if (definition.magicId < 0)
            return reportMagicLoadError(entryNode, context, "「武功」必須是非負整數", diagnostics);
        if (!seenMagicIds.insert(definition.magicId).second)
            return reportMagicLoadError(entryNode, context, std::format("武功 {} 重複定義", definition.magicId), diagnostics);

        const auto rules = entryNode["效果"];
        if (!rules || !rules.IsSequence() || rules.size() == 0)
            return reportMagicLoadError(entryNode, context, std::format("武功 {} 缺少有效「效果」列表", definition.magicId), diagnostics);
        for (std::size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
        {
            EffectRule rule;
            const auto stableId = EffectRuleId{
                static_cast<std::uint64_t>(static_cast<std::uint32_t>(definition.magicId)) << 32
                | static_cast<std::uint64_t>(ruleIndex),
            };
            if (!parseEffectRule(
                    rules[ruleIndex],
                    rule,
                    stableId,
                    std::format("{}:絕招「{}」規則#{}", context, definition.name, ruleIndex + 1),
                    diagnostics))
            {
                return false;
            }
            if (rule.selector.kind == EffectSelectorKind::ComboMembers)
            {
                return reportMagicLoadError(
                    rules[ruleIndex],
                    context,
                    std::format("武功 {} 的規則不可使用羈絆成員選擇器", definition.magicId),
                    diagnostics);
            }
            if (rule.event == EffectEvent::BattleInitialized)
            {
                return reportMagicLoadError(
                    rules[ruleIndex],
                    context,
                    std::format(
                        "武功 {} 的戰鬥初始化規則不會參與初始化流程",
                        definition.magicId),
                    diagnostics);
            }
            definition.rules.push_back(std::move(rule));
        }
        parsedDefinitions.push_back(std::move(definition));
    }
    out = std::move(parsedDefinitions);
    return true;
}

bool ChessBattleEffects::loadMagicEffectsFile(
    const std::string& path,
    std::vector<ChessMagicEffectDefinition>& out,
    const ChessDiagnosticSink& diagnostics)
{
    try
    {
        return parseMagicEffects(YAML::LoadFile(path), out, path, diagnostics);
    }
    catch (const YAML::Exception& ex)
    {
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "武功效果", std::format("讀取「{}」失敗: {}", path, ex.what()));
        out.clear();
        return false;
    }
}

}  // namespace KysChess
