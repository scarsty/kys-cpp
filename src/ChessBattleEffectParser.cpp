#include "ChessBattleEffectParser.h"
#include "ChessBattleEffectSemantics.h"
#include "ChessBattleEffectValidation.h"
#include "ChessEffectAuthoringDescriptors.h"
#include "ChessEffectAuthoringMetadata.h"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <iterator>
#include <ranges>
#include <set>
#include <span>
#include <string_view>
#include <type_traits>

namespace KysChess::EffectAuthoring
{

std::span<const TimingDescriptor> timingDescriptors()
{
    return Detail::Metadata::timingDescriptors;
}

std::span<const ConditionDescriptor> conditionDescriptors()
{
    return Detail::Metadata::conditionDescriptors;
}

std::span<const ActionDescriptor> actionDescriptors()
{
    return Detail::Metadata::actionDescriptors;
}

std::span<const MacroDescriptor> macroDescriptors()
{
    return Detail::Metadata::macroDescriptors;
}

const AuthorEnumDescriptor& battleAttributeDescriptor()
{
    return Detail::Metadata::battleAttributeEnum;
}

const AuthorEnumDescriptor& selectorKindDescriptor()
{
    return Detail::Metadata::selectorKindEnum;
}

const PayloadDescriptor& effectNumberDescriptor()
{
    return Detail::Metadata::effectNumberPayload;
}

const PayloadDescriptor& selectorDescriptor()
{
    return Detail::Metadata::selectorPayload;
}

const PayloadDescriptor& ruleDescriptor()
{
    return Detail::Metadata::rulePayload;
}

const TimingDescriptor* findTimingDescriptor(std::string_view name)
{
    const auto descriptors = timingDescriptors();
    const auto found = std::ranges::find(descriptors, name, &TimingDescriptor::name);
    return found == descriptors.end() ? nullptr : &*found;
}

const ConditionDescriptor* findConditionDescriptor(std::string_view name)
{
    const auto descriptors = conditionDescriptors();
    const auto found = std::ranges::find(descriptors, name, &ConditionDescriptor::name);
    return found == descriptors.end() ? nullptr : &*found;
}

const ActionDescriptor* findActionDescriptor(std::string_view name)
{
    const auto descriptors = actionDescriptors();
    const auto found = std::ranges::find(descriptors, name, &ActionDescriptor::name);
    return found == descriptors.end() ? nullptr : &*found;
}

const MacroDescriptor* findMacroDescriptor(std::string_view name)
{
    const auto descriptors = macroDescriptors();
    const auto found = std::ranges::find(descriptors, name, &MacroDescriptor::name);
    return found == descriptors.end() ? nullptr : &*found;
}

}  // namespace KysChess::EffectAuthoring

namespace KysChess::EffectAuthoring::Detail::Metadata
{

bool parseStatusKind(std::string_view label, BattleStatusKind& out, std::string& error)
{
    const auto parsed = parseLabel<BattleStatusKind>(label, statusKindEnum);
    if (!parsed)
    {
        error = std::format("未知狀態「{}」", label);
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
    const auto parsed = parseLabel<EffectStateSlot>(label, stateSlotEnum);
    if (!parsed)
    {
        error = std::format("未知狀態槽「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool isDynamicPayloadKey(PayloadDynamicKeyClass keyClass, std::string_view key)
{
    if (keyClass == PayloadDynamicKeyClass::None) return false;
    if (keyClass == PayloadDynamicKeyClass::BattleAttribute)
    {
        return std::ranges::any_of(
            battleAttributeLabels,
            [=](const auto& entry) { return entry.name == key; });
    }
    assert(keyClass == PayloadDynamicKeyClass::NamedAction);
    return findActionDescriptor(key) || findMacroDescriptor(key);
}

}  // namespace KysChess::EffectAuthoring::Detail::Metadata

namespace KysChess
{
namespace
{

using namespace EffectAuthoring::Detail::Metadata;

using namespace EffectAuthoring;

enum class ConditionNodeForm
{
    Scalar,
    SingleParameter,
    NamedPayload,
};

bool parseConditionPayload(
    const ConditionDescriptor& descriptor,
    const YAML::Node& payload,
    ConditionNodeForm form,
    EffectCondition& out,
    std::string& error)
{
    const auto type = descriptor.name;
    std::optional<PayloadView> payloadView;
    if (form == ConditionNodeForm::NamedPayload)
    {
        payloadView.emplace(payload, *descriptor.payload);
        if (!payloadView->validate(error)) return false;
    }
    else if (form == ConditionNodeForm::Scalar)
    {
        if (!descriptor.payload->fields.empty()
            && descriptor.form != ConditionAuthorForm::ScalarOrMap)
        {
            error = std::format("條件「{}」需要參數，不能使用 scalar 簡式", type);
            return false;
        }
    }
    else
    {
        if (descriptor.payload->fields.size() != 1
            || descriptor.payload->fields.front().name != descriptor.singleParameterField)
        {
            error = std::format("條件「{}」不支援單參數簡式", type);
            return false;
        }
        if (!validatePayloadNodeShape(payload, descriptor.payload->fields.front(), error)) return false;
    }
    const auto valueNode = [&](std::string_view field)
    {
        return form == ConditionNodeForm::SingleParameter
            ? payload
            : (*payloadView)[field];
    };
    const auto readInt = [&](std::string_view field, int& value)
    {
        const auto valueField = valueNode(field);
        if (!valueField)
        {
            error = std::format("缺少「{}」欄位", field);
            return false;
        }
        try
        {
            value = valueField.as<int>();
            return true;
        }
        catch (const YAML::Exception& ex)
        {
            error = std::format("「{}」不是有效整數: {}", field, ex.what());
            return false;
        }
    };
    const auto readString = [&](std::string_view field, std::string& value)
    {
        const auto valueField = valueNode(field);
        if (!valueField)
        {
            error = std::format("缺少「{}」欄位", field);
            return false;
        }
        try
        {
            value = valueField.as<std::string>();
            return true;
        }
        catch (const YAML::Exception& ex)
        {
            error = std::format("「{}」不是有效字串: {}", field, ex.what());
            return false;
        }
    };
    const auto readStrings = [&](std::string_view field, std::vector<std::string>& values)
    {
        const auto valueField = valueNode(field);
        if (!valueField || !valueField.IsSequence() || valueField.size() == 0)
        {
            error = std::format("「{}」必須是非空列表", field);
            return false;
        }
        try
        {
            values.reserve(valueField.size());
            for (const auto& value : valueField) values.push_back(value.as<std::string>());
            return true;
        }
        catch (const YAML::Exception& ex)
        {
            error = std::format("「{}」含有無效字串: {}", field, ex.what());
            return false;
        }
    };

    if (type == "僅限絕招")
    {
        out = IsUltimateCondition{};
    }
    else if (type == "施放武功為效果來源")
    {
        out = CastUsesEffectSourceMagicCondition{};
    }
    else if (type == "僅限主彈道")
    {
        out = IsMainProjectileCondition{};
    }
    else if (type == "僅限根攻擊")
    {
        out = IsRootAttackCondition{};
    }
    else if (type == "自身生命不高於")
    {
        int percent{};
        if (!readInt("百分比", percent)) return false;
        out = SourceHpRatioAtMostCondition{ percent };
    }
    else if (type == "自身生命低於")
    {
        int percent{};
        if (!readInt("百分比", percent)) return false;
        out = SourceHpRatioBelowCondition{ percent };
    }
    else if (type == "自身為最後存活")
    {
        out = SourceIsLastAliveCondition{};
    }
    else if (type == "目標生命不高於")
    {
        int percent{};
        if (!readInt("百分比", percent)) return false;
        out = TargetHpRatioAtMostCondition{ percent };
    }
    else if (type == "目標非無敵")
    {
        out = TargetNotInvincibleCondition{};
    }
    else if (type == "自身有狀態")
    {
        std::string state;
        BattleStatusKind parsed{};
        if (!readString("狀態", state)
            || !parseStatusKind(state, parsed, error)) return false;
        out = SourceHasStateCondition{ parsed };
    }
    else if (type == "目標有狀態")
    {
        std::string state;
        BattleStatusKind parsed{};
        if (!readString("狀態", state)
            || !parseStatusKind(state, parsed, error)) return false;
        out = TargetHasStateCondition{ parsed };
    }
    else if (type == "目標有此來源狀態")
    {
        std::string state;
        BattleStatusKind parsed{};
        if (!readString("狀態", state)
            || !parseStatusKind(state, parsed, error)) return false;
        out = TargetHasStateFromEffectOwnerCondition{ parsed };
    }
    else if (type == "自身層數至少")
    {
        std::string stack;
        BattleStatusKind parsed{};
        int count{};
        if (!readString("狀態", stack)
            || !parseStatusKind(stack, parsed, error)
            || !readInt("層數", count)) return false;
        out = SourceStackAtLeastCondition{ parsed, count };
    }
    else if (type == "其他存活友軍使用此武功")
    {
        out = OtherLivingAllyUsesBoundMagicCondition{};
    }
    else if (type == "不同目標數至少")
    {
        int count{};
        if (!readInt("數量", count)) return false;
        out = CastDistinctTargetCountAtLeastCondition{ count };
    }
    else if (type == "攻擊序號")
    {
        int ordinal{};
        if (!readInt("序號", ordinal)) return false;
        out = AttackOrdinalEqualsCondition{ ordinal };
    }
    else if (type == "治療種類符合" || type == "傷害種類符合")
    {
        const auto fieldName = type == "治療種類符合" ? "治療種類" : "傷害種類";
        std::vector<std::string> labels;
        if (!readStrings(fieldName, labels)) return false;
        if (type == "治療種類符合") out = HealKindInCondition{ std::move(labels) };
        else out = DamageKindInCondition{ std::move(labels) };
    }
    else if (type == "傷害來自招式")
    {
        out = DamageOriginIsAttackCondition{};
    }
    else if (type == "已接受命中")
    {
        AcceptedHitCondition condition;
        if (form == ConditionNodeForm::Scalar)
        {
            out = condition;
            return true;
        }
        if (form != ConditionNodeForm::NamedPayload
            || !optionalBool(*payloadView, "需要正傷害", condition.requirePositiveDamage, error)) return false;
        out = condition;
    }
    else if (type == "事件目標屬於綁定來源")
    {
        out = EventTargetBelongsToBoundSourceCondition{};
    }
    else if (type == "傷害造成死亡")
    {
        out = DamageKilledTargetCondition{};
    }
    else if (type == "受益者施放前滿內")
    {
        out = TargetMpWasFullBeforeCastCondition{};
    }
    else if (type == "有合法隨機目標")
    {
        out = RandomSelectionAvailableCondition{};
    }
    else
    {
        error = std::format("未知條件「{}」", type);
        return false;
    }
    return !payloadView || payloadView->finish(error);
}

bool parseConditionNode(const YAML::Node& node, EffectCondition& out, std::string& error)
{
    if (!node)
    {
        error = "缺少條件";
        return false;
    }
    if (node.IsScalar())
    {
        const auto type = node.as<std::string>();
        const auto* descriptor = findConditionDescriptor(type);
        if (!descriptor
            || (descriptor->form != ConditionAuthorForm::Scalar
                && descriptor->form != ConditionAuthorForm::ScalarOrMap))
        {
            error = std::format("條件「{}」不可使用 scalar 外形", type);
            return false;
        }
        return parseConditionPayload(
            *descriptor, node, ConditionNodeForm::Scalar, out, error);
    }
    if (!node.IsMap())
    {
        error = "條件必須是映射表或簡式名稱";
        return false;
    }
    if (!validateUniqueKeys(node, error)) return false;
    if (node.size() != 1)
    {
        error = "具名條件必須恰有一個條件欄位";
        return false;
    }
    const auto entry = *node.begin();
    const auto type = entry.first.as<std::string>();
    const auto* descriptor = findConditionDescriptor(type);
    if (!descriptor)
    {
        error = std::format("未知條件「{}」", type);
        return false;
    }
    if (descriptor->form == ConditionAuthorForm::Scalar)
    {
        error = std::format("無參數條件「{}」請寫成 scalar 列表項目", type);
        return false;
    }
    if (descriptor->form == ConditionAuthorForm::SingleParameter)
    {
        return parseConditionPayload(
            *descriptor,
            entry.second,
            ConditionNodeForm::SingleParameter,
            out,
            error);
    }
    if (!entry.second.IsMap())
    {
        error = std::format("條件「{}」的 payload 必須是映射表", type);
        return false;
    }
    return parseConditionPayload(
        *descriptor,
        entry.second,
        ConditionNodeForm::NamedPayload,
        out,
        error);
}

bool parseStackPolicy(const YAML::Node& node, EffectStackPolicy& out, std::string& error)
{
    if (!node) return true;
    const auto label = node.as<std::string>();
    const auto parsed = parseLabel<EffectStackPolicy>(label, stackPolicyEnum);
    if (!parsed)
    {
        error = std::format("未知合併方式「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseDamageChannel(std::string_view label, DamageChannel& out, std::string& error)
{
    const auto parsed = parseLabel<DamageChannel>(label, damageChannelEnum);
    if (!parsed)
    {
        error = std::format("未知傷害種類「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseResourceLabel(
    std::string_view label,
    BattleResource& out,
    std::string& error)
{
    const auto parsed = parseLabel<BattleResource>(label, resourceEnum);
    if (!parsed)
    {
        error = std::format("未知資源「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseResourceChangeKindLabel(
    std::string_view label,
    ResourceChangeKind& out,
    std::string& error)
{
    const auto parsed = parseLabel<ResourceChangeKind>(label, resourceChangeKindEnum);
    if (!parsed)
    {
        error = std::format("未知資源變更方式「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

template <typename Node>
bool parseResourceMetadata(
    const Node& node,
    ChangeResourceAction& action,
    std::string& error)
{
    if (node["轉移目標"])
    {
        EffectSelector selector;
        if (!parseSelectorNode(node["轉移目標"], selector, error)) return false;
        action.transferDestination = std::move(selector);
    }
    if (const auto healKind = node["治療種類"])
    {
        const auto label = healKind.as<std::string>();
        const auto parsed = parseLabel<EffectHealKind>(label, healKindEnum);
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
        const auto parsed = parseLabel<EffectHealSourcePolicy>(label, healSourcePolicyEnum);
        if (!parsed)
        {
            error = std::format("未知治療來源政策「{}」", label);
            return false;
        }
        action.healSourcePolicy = *parsed;
    }
    return true;
}

template <typename Node>
bool parseAttributeModifierQualifiers(
    const Node& node,
    ModifyAttributeAction& out,
    std::string& error)
{
    if (!optionalInt(node, "持續幀數", out.durationFrames, error)
        || !parseStackPolicy(node["合併方式"], out.stack, error)
        || !optionalBool(node, "每層", out.perStack, error)) return false;
    if (node["層數上限"])
    {
        int limit{};
        if (!requiredInt(node, "層數上限", limit, error)) return false;
        out.stackLimit = limit;
    }
    if (const auto scope = node["疊加範圍"])
    {
        const auto label = scope.as<std::string>();
        const auto parsed = parseLabel<EffectStackScope>(label, stackScopeEnum);
        if (!parsed)
        {
            error = std::format("未知疊加範圍「{}」", label);
            return false;
        }
        out.stackScope = *parsed;
    }
    return true;
}

bool parseActionPayload(
    const EffectAuthoring::ActionDescriptor& descriptor,
    PayloadView& node,
    EffectAction& out,
    std::string& error);
bool parseAuthorActionNode(
    const YAML::Node& node,
    std::vector<EffectAction>& out,
    std::string& error);

bool parseActionList(const YAML::Node& node, std::vector<EffectAction>& out, std::string& error)
{
    if (!node || !node.IsSequence() || node.size() == 0)
    {
        error = "動作必須是非空列表";
        return false;
    }
    out.clear();
    for (std::size_t i = 0; i < node.size(); ++i)
    {
        std::vector<EffectAction> actions;
        if (!parseAuthorActionNode(node[i], actions, error))
        {
            error = std::format("動作#{}: {}", i + 1, error);
            return false;
        }
        out.insert(
            out.end(),
            std::make_move_iterator(actions.begin()),
            std::make_move_iterator(actions.end()));
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
        const auto parsed = parseLabel<BorrowedRuleActionCategory>(
            label, borrowedRuleActionCategoryEnum);
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
        const auto parsed = parseLabel<CopiedMagicCondition>(label, copiedMagicConditionEnum);
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

consteval bool attributeNamesAreUniqueAndReservedFieldsAreDisjoint()
{
    for (std::size_t i = 0; i < battleAttributeLabels.size(); ++i)
    {
        for (std::size_t j = i + 1; j < battleAttributeLabels.size(); ++j)
            if (battleAttributeLabels[i].name == battleAttributeLabels[j].name) return false;
        for (const auto& field : attributeBonusFields)
            if (battleAttributeLabels[i].name == field.name) return false;
    }
    return true;
}

static_assert(attributeNamesAreUniqueAndReservedFieldsAreDisjoint());

bool parseAttribute(std::string_view label, BattleAttribute& out, std::string& error)
{
    const auto parsed = parseLabel<BattleAttribute>(label, battleAttributeEnum);
    if (!parsed)
    {
        error = std::format("未知屬性「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

template <typename Node>
bool parseAttackPatternFields(const Node& node, AttackPattern& pattern, std::string& error)
{
    if (const auto style = node["樣式"])
    {
        const auto label = style.as<std::string>();
        const auto parsed = parseLabel<AttackPatternKind>(label, attackPatternKindEnum);
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
    PayloadView payload(node, attackRuntimeBehaviorPayload);
    if (!payload.validate(error)) return false;
    std::string type;
    if (!requiredString(payload, "類型", type, error)) return false;

    const auto kind = parseLabel<AttackRuntimeBehaviorKind>(type, attackRuntimeBehaviorKindEnum);
    if (!kind)
    {
        error = std::format("未知攻擊執行行為「{}」", type);
        return false;
    }
    if (*kind == AttackRuntimeBehaviorKind::ProjectileBounce)
    {
        ProjectileBounceAttackBehavior behavior;
        if (!requiredInt(payload, "追加命中次數", behavior.additionalHits, error)
            || !requiredInt(payload, "機率", behavior.chancePct, error)
            || !requiredInt(payload, "範圍像素", behavior.rangePixels, error)) return false;
        out = behavior;
        return payload.finish(error);
    }
    if (*kind == AttackRuntimeBehaviorKind::NearbyTracking)
    {
        NearbyTrackingAttackBehavior behavior;
        if (!requiredInt(payload, "範圍像素", behavior.rangePixels, error)
            || !requiredInt(payload, "傷害倍率", behavior.damagePct, error)) return false;
        out = behavior;
        return payload.finish(error);
    }
    if (*kind == AttackRuntimeBehaviorKind::DelayedAlternate)
    {
        DelayedAlternateAttackBehavior behavior;
        if (!requiredInt(payload, "延遲幀數", behavior.delayFrames, error)
            || !requiredInt(payload, "傷害倍率", behavior.damagePct, error)
            || !requiredInt(payload, "攻擊者獲得格擋機率",
                behavior.attackerBlockGainChancePct,
                error)) return false;
        out = behavior;
        return payload.finish(error);
    }
    if (*kind == AttackRuntimeBehaviorKind::ExpandingSpiral)
    {
        ExpandingSpiralAttackBehavior behavior;
        if (!requiredInt(payload, "彈道數量", behavior.projectileCount, error)
            || !requiredInt(payload, "流血層數", behavior.bleedStacks, error)) return false;
        out = behavior;
        return payload.finish(error);
    }
    assert(false);
    return false;
}

bool parsePropagation(const YAML::Node& node, CastPropagationPolicy& out, std::string& error)
{
    if (!node) return true;
    const auto label = node.as<std::string>();
    const auto parsed = parseLabel<CastPropagationPolicy>(label, propagationPolicyEnum);
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
    PayloadView payload(node, areaModifierPayload);
    if (!payload.validate(error)) return false;
    out = {};
    std::string type;
    if (!requiredString(payload, "類型", type, error)) return false;
    const auto kind = parseLabel<AreaModifierKind>(type, areaModifierKindEnum);
    if (!kind)
    {
        error = std::format("未知區域修正類型「{}」", type);
        return false;
    }
    out.kind = *kind;
    const auto relation = payload["關係"];
    if (!relation)
    {
        error = "區域修正缺少「關係」欄位";
        return false;
    }
    {
        const auto label = relation.as<std::string>();
        const auto parsed = parseLabel<EffectTeamFilter>(label, areaRelationEnum);
        if (!parsed)
        {
            error = std::format("未知區域關係「{}」", label);
            return false;
        }
        out.relation = *parsed;
    }
    if (const auto attribute = payload["屬性"])
    {
        if (!parseAttribute(attribute.as<std::string>(), out.attribute, error)) return false;
    }
    if (payload["數值"] && !parseEffectNumberNode(payload["數值"], out.amount, error)) return false;
    if (!optionalInt(payload, "百分比", out.percent, error)) return false;
    if (const auto damageKind = payload["傷害種類"])
    {
        if (!parseDamageChannel(damageKind.as<std::string>(), out.damageChannel, error)) return false;
    }
    if (payload["追蹤"])
    {
        bool tracking{};
        if (!optionalBool(payload, "追蹤", tracking, error)) return false;
        out.tracking = tracking;
    }
    if (payload["彈速百分比"])
    {
        int value{};
        if (!requiredInt(payload, "彈速百分比", value, error)) return false;
        out.speedPct = value;
    }
    if (payload["彈道壓制百分比"])
    {
        int value{};
        if (!requiredInt(payload, "彈道壓制百分比", value, error)) return false;
        out.projectilePressurePct = value;
    }
    if (const auto direction = payload["阻擋方向"])
    {
        const auto label = direction.as<std::string>();
        const auto parsed = parseLabel<ForceMoveDirection>(label, areaBlockedDirectionEnum);
        if (!parsed)
        {
            error = std::format("未知強制移動方向「{}」", label);
            return false;
        }
        out.blockedDirection = *parsed;
    }
    const auto overlap = payload["重疊方式"];
    if (!overlap)
    {
        error = "區域修正缺少「重疊方式」欄位";
        return false;
    }
    {
        const auto label = overlap.as<std::string>();
        const auto parsed = parseLabel<AreaOverlapPolicy>(label, areaOverlapPolicyEnum);
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
        if (!payload["屬性"] || !payload["數值"])
        {
            error = "屬性區域修正需要「屬性」與「數值」";
            return false;
        }
        if (unexpected(payload["百分比"] || payload["傷害種類"] || payload["追蹤"]
                || payload["彈速百分比"] || payload["彈道壓制百分比"] || payload["阻擋方向"], "非屬性修正")) return false;
        if (out.overlap == AreaOverlapPolicy::Any)
        {
            error = "數值屬性區域修正不可使用「任一」重疊方式";
            return false;
        }
    }
    else if (out.kind == AreaModifierKind::OutgoingDamage)
    {
        if (!payload["百分比"] || !payload["傷害種類"])
        {
            error = "造成傷害區域修正需要「百分比」與「傷害種類」";
            return false;
        }
        if (unexpected(payload["屬性"] || payload["數值"] || payload["追蹤"]
                || payload["彈速百分比"] || payload["彈道壓制百分比"] || payload["阻擋方向"], "非傷害修正")) return false;
        if (out.overlap == AreaOverlapPolicy::Any)
        {
            error = "數值傷害區域修正不可使用「任一」重疊方式";
            return false;
        }
    }
    else if (out.kind == AreaModifierKind::AttackSpawn)
    {
        if (!payload["追蹤"] && !payload["彈速百分比"] && !payload["彈道壓制百分比"])
        {
            error = "攻擊生成區域修正至少需要一個修正欄位";
            return false;
        }
        if (unexpected(payload["屬性"] || payload["數值"] || payload["百分比"]
                || payload["傷害種類"] || payload["阻擋方向"], "非攻擊生成修正")) return false;
        if (payload["追蹤"] && out.overlap != AreaOverlapPolicy::Any)
        {
            error = "追蹤布林修正必須使用「任一」重疊方式";
            return false;
        }
        if ((payload["彈速百分比"] || payload["彈道壓制百分比"])
            && out.overlap == AreaOverlapPolicy::Any)
        {
            error = "數值攻擊生成修正不可使用「任一」重疊方式";
            return false;
        }
    }
    else
    {
        if (!payload["阻擋方向"])
        {
            error = "強制移動免疫需要「阻擋方向」";
            return false;
        }
        if (unexpected(payload["屬性"] || payload["數值"] || payload["百分比"]
                || payload["傷害種類"] || payload["追蹤"] || payload["彈速百分比"]
                || payload["彈道壓制百分比"], "非強制移動免疫")) return false;
        if (out.overlap != AreaOverlapPolicy::Any)
        {
            error = "強制移動免疫必須使用「任一」重疊方式";
            return false;
        }
    }
    if (out.tracking) out.trackingOverlap = out.overlap;
    if (out.speedPct) out.speedOverlap = out.overlap;
    if (out.projectilePressurePct) out.projectilePressureOverlap = out.overlap;
    return payload.finish(error);
}

bool parseActionPayload(
    const EffectAuthoring::ActionDescriptor& descriptor,
    PayloadView& node,
    EffectAction& out,
    std::string& error)
{
    const auto type = descriptor.name;
    if (type == "屬性修正")
    {
        ModifyAttributeAction action;
        std::string attribute;
        std::string operation;
        if (!requiredString(node, "屬性", attribute, error)
            || !requiredString(node, "方式", operation, error)
            || !parseAttribute(attribute, action.attribute, error)
            || !parseEffectNumberNode(node["數值"], action.amount, error)) return false;
        const auto parsedOperation = parseLabel<AttributeOperation>(operation, attributeOperationEnum);
        if (!parsedOperation)
        {
            error = std::format("未知屬性運算「{}」", operation);
            return false;
        }
        action.operation = *parsedOperation;
        if (!parseAttributeModifierQualifiers(node, action, error)) return false;
        out.value = std::move(action);
        return true;
    }
    if (type == "傷害修正")
    {
        ModifyDamageAction action;
        std::string stage;
        std::string channel;
        std::string operation;
        if (const auto perspective = node["方位"])
        {
            const auto label = perspective.as<std::string>();
            const auto parsed = parseLabel<DamageModifierPerspective>(
                label, damageModifierPerspectiveEnum);
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
        const auto parsedStage = parseLabel<DamageModifierStage>(stage, damageModifierStageEnum);
        const auto parsedOperation = parseLabel<DamageModifierOperation>(
            operation, damageModifierOperationEnum);
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
            const auto parsed = parseLabel<EffectStackScope>(label, stackScopeEnum);
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
        ChangeResourceAction action;
        std::string resource;
        std::string kind;
        if (!requiredString(node, "資源", resource, error)
            || !requiredString(node, "方式", kind, error)
            || !parseResourceLabel(resource, action.resource, error)
            || !parseResourceChangeKindLabel(kind, action.kind, error)
            || !parseEffectNumberNode(node["數值"], action.amount, error)
            || !parseResourceMetadata(node, action, error)) return false;
        out.value = std::move(action);
        return true;
    }
    if (type == "治療交易修正")
    {
        ModifyHealTransactionAction action;
        std::string operation;
        if (!requiredString(node, "方式", operation, error)) return false;
        const auto parsedOperation = parseLabel<HealModifierOperation>(
            operation, healModifierOperationEnum);
        if (!parsedOperation)
        {
            error = std::format("未知治療修正方式「{}」", operation);
            return false;
        }
        action.operation = *parsedOperation;
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
        ApplyStatusAction action;
        std::string status;
        if (!requiredString(node, "狀態", status, error)
            || !parseStatusKind(status, action.status, error)
            || !optionalInt(node, "層數", action.stacks, error)
            || !parseStackPolicy(node["合併方式"], action.stack, error)) return false;
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
        ConsumeStatusAction action;
        std::string status;
        if (!requiredString(node, "狀態", status, error)
            || !parseStatusKind(status, action.status, error)
            || !optionalInt(node, "層數", action.stacks, error)) return false;
        if (const auto source = node["狀態來源"])
        {
            const auto label = source.as<std::string>();
            const auto parsed = parseLabel<StatusSourceMatch>(label, statusSourceMatchEnum);
            if (!parsed)
            {
                error = std::format("未知狀態來源「{}」", label);
                return false;
            }
            action.source = *parsed;
        }
        if (const auto depleted = node["最後一層"])
        {
            std::vector<EffectAction> nestedActions;
            if (!parseAuthorActionNode(depleted, nestedActions, error)) return false;
            if (nestedActions.size() != 1)
            {
                error = "消耗最後一層需要恰好一個套用狀態動作";
                return false;
            }
            auto nested = std::move(nestedActions.front());
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
            const auto parsed = parseLabel<StatusRemovalOrder>(label, statusRemovalOrderEnum);
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
            const auto parsed = parseLabel<DamageAreaKind>(label, damageAreaKindEnum);
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
            PayloadView projectile(projectileNode, areaProjectilePayload);
            if (!projectile.validate(error)) return false;
            AreaProjectileDamageDelivery delivery;
            std::string visual;
            if (!requiredInt(projectile, "範圍格數", delivery.rangeTiles, error)
                || !requiredInt(projectile, "最多目標", delivery.maximumTargets, error)
                || !requiredInt(projectile, "眩暈幀數", delivery.stunFrames, error)
                || !optionalBool(projectile, "追蹤事件來源", delivery.trackEventSource, error)
                || !requiredString(projectile, "特效", visual, error)) return false;
            const auto parsedVisual = parseLabel<AreaProjectileVisual>(visual, areaProjectileVisualEnum);
            if (!parsedVisual)
            {
                error = std::format("未知區域投射物特效「{}」", visual);
                return false;
            }
            delivery.visual = *parsedVisual;
            if (!projectile.finish(error)) return false;
            action.areaProjectiles = delivery;
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "修改攻擊")
    {
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
            const auto parsed = parseLabel<AttackTargetPolicy>(label, attackTargetPolicyEnum);
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
        const auto parsedDirection = parseLabel<ForceMoveDirection>(
            direction, forceMoveDirectionEnum);
        const auto parsedCollision = parseLabel<ForceMoveCollision>(
            collision, forceMoveCollisionEnum);
        const auto parsedBlocked = parseLabel<ForceMoveBlockedResult>(
            blocked, forceMoveBlockedResultEnum);
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
        const auto parsedShape = parseLabel<AreaShape>(shape, areaShapeEnum);
        const auto parsedAnchor = parseLabel<AreaAnchor>(anchor, areaAnchorEnum);
        const auto parsedDeath = parseLabel<AreaSourceDeathPolicy>(death, areaSourceDeathPolicyEnum);
        const auto parsedMerge = parseLabel<AreaMergePolicy>(merge, areaMergePolicyEnum);
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
            const auto parsed = parseLabel<CastRangeMode>(label, castRangeModeEnum);
            if (!parsed)
            {
                error = std::format("未知射程模式「{}」", label);
                return false;
            }
            action.rangeMode = *parsed;
        }
        if (const auto mobility = node["機動政策"])
        {
            const auto label = mobility.as<std::string>();
            const auto parsed = parseLabel<CastMobilityPolicy>(label, castMobilityPolicyEnum);
            if (!parsed)
            {
                error = std::format("未知施放機動政策「{}」", label);
                return false;
            }
            action.mobility = *parsed;
        }
        if (const auto autoUltimate = node["自動絕招"])
        {
            PayloadView autoUltimateView(autoUltimate, autoUltimatePayload);
            if (!autoUltimateView.validate(error)) return false;
            AutoUltimateCastRequest request;
            if (!optionalBool(autoUltimateView, "消耗內力", request.consumeMp, error)
                || !optionalBool(autoUltimateView, "顯示公告", request.announce, error)
                || !autoUltimateView.finish(error)) return false;
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
    if (descriptor.mechanism)
    {
        const auto mechanism = *descriptor.mechanism;
        if (mechanism == StateMachineMechanism::ChangeStateValue)
        {
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
        else if (mechanism == StateMachineMechanism::TransferStateValue)
        {
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
        else if (mechanism == StateMachineMechanism::RecordMaximumSkillDamage)
        {
            RecordMaximumDamageAction action;
            if (!parseEffectStateSlot(node["狀態槽"], action.slot, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == StateMachineMechanism::ConsumeRecordAsDamage
            || mechanism == StateMachineMechanism::ConsumeRecordAsShield)
        {
            ConsumeRecordedMaximumAction action;
            action.destination = mechanism == StateMachineMechanism::ConsumeRecordAsShield
                ? StateValueDestination::ShieldAmount
                : StateValueDestination::DamageAmount;
            if (!parseEffectStateSlot(node["狀態槽"], action.slot, error)
                || !optionalInt(node, "百分比", action.percent, error)
                || !optionalBool(node, "消耗後清除", action.clearAfterConsume, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == StateMachineMechanism::StartDamageAbsorption)
        {
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
        else if (mechanism == StateMachineMechanism::SettleDamageAbsorption)
        {
            SettleDamageAbsorptionAction action;
            if (!parseEffectStateSlot(node["狀態槽"], action.slot, error)
                || !optionalInt(node, "百分比", action.returnedPct, error)) return false;
            if (node["目標"] && !parseSelectorNode(node["目標"], action.target, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == StateMachineMechanism::BorrowEffectRules)
        {
            BorrowEffectRulesAction action;
            if (!parseSelectorNode(node["目標"], action.sourceUnits, error)
                || !parseEffectNumberNode(node["來源數量"], action.sourceCount, error)
                || !parseBorrowedRuleFilter(node["允許動作類別"], action.filter, error)
                || !parsePropagation(node["傳播政策"], action.propagation, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == StateMachineMechanism::CopyAttackDefinition)
        {
            CopyAttackDefinitionAction action;
            if (!parseSelectorNode(node["目標"], action.sourceUnits, error)
                || !parseCopiedMagicFilter(node["可選武功條件"], action.filter, error)
                || !optionalInt(node, "來源數量", action.copyCount, error)
                || !parsePropagation(node["傳播政策"], action.propagation, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == StateMachineMechanism::SettleRemainingStatusDamage)
        {
            SettleRemainingStatusDamageAction action;
            action.status = BattleStatusKind::Poison;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == StateMachineMechanism::GenerateClones)
        {
            GenerateClonesAction action;
            if (!requiredInt(node, "數量", action.count, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == StateMachineMechanism::PreventDeath)
        {
            PreventDeathAction action;
            if (!requiredInt(node, "無敵幀數", action.invincibilityFrames, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (mechanism == StateMachineMechanism::ConfigureProtectReposition
            || mechanism == StateMachineMechanism::ConfigureExecuteReposition)
        {
            ConfigureRescueRepositionAction action;
            action.mode = mechanism == StateMachineMechanism::ConfigureProtectReposition
                ? RescueRepositionMode::Protect
                : RescueRepositionMode::Execute;
            if (!requiredInt(node, "次數", action.activations, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else assert(false);
        return true;
    }
    if (type == "條件分支")
    {
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

bool parseNamedResourceAction(
    std::string_view name,
    const YAML::Node& node,
    EffectAction& out,
    std::string& error)
{
    PayloadView payload(node, resourceMacroPayload);
    if (!payload.validate(error)) return false;
    ChangeResourceAction action;
    std::string resource;
    if (!requiredString(payload, "資源", resource, error)
        || !parseResourceLabel(resource, action.resource, error)
        || !parseEffectNumberNode(payload["數值"], action.amount, error)) return false;
    if (name == "回復資源") action.kind = ResourceChangeKind::Restore;
    else if (name == "獲得資源") action.kind = ResourceChangeKind::Grant;
    else action.kind = ResourceChangeKind::Drain;
    if (!parseResourceMetadata(payload, action, error)
        || !payload.finish(error)) return false;
    out.value = std::move(action);
    return true;
}

bool parseNonEmptyEffectNumber(
    const YAML::Node& node,
    EffectNumber& out,
    std::string& error)
{
    if (node && node.IsMap() && node.size() == 0)
    {
        error = "簡式數值不可是空映射表";
        return false;
    }
    return parseEffectNumberNode(node, out, error);
}

bool parseAttributeBonusMacro(
    const YAML::Node& node,
    std::vector<EffectAction>& out,
    std::string& error)
{
    PayloadView payload(node, attributeBonusPayload);
    if (!payload.validate(error)) return false;

    ModifyAttributeAction qualifierPrototype;
    if (!parseAttributeModifierQualifiers(payload, qualifierPrototype, error)) return false;
    std::set<BattleAttribute> allAttributes;
    std::vector<ModifyAttributeAction> fixedActions;
    std::vector<ModifyAttributeAction> percentageActions;
    const auto appendAttribute = [&](
        std::string_view label,
        const YAML::Node& value,
        AttributeOperation operation,
        std::vector<ModifyAttributeAction>& actions)
    {
        BattleAttribute attribute{};
        if (!parseAttribute(label, attribute, error)) return false;
        if (!allAttributes.insert(attribute).second)
        {
            error = std::format("屬性加成重複指定屬性「{}」", label);
            return false;
        }
        if (!value.IsScalar())
        {
            error = std::format("屬性加成「{}」必須是 scalar 數字", label);
            return false;
        }
        ModifyAttributeAction action = qualifierPrototype;
        action.attribute = attribute;
        action.operation = operation;
        if (!parseEffectNumberNode(value, action.amount, error)) return false;
        actions.push_back(std::move(action));
        return true;
    };

    for (const auto& [label, value] : payload.dynamicEntries())
    {
        if (!appendAttribute(label, value, AttributeOperation::FlatAdd, fixedActions))
            return false;
    }
    if (const auto percentage = payload["百分比"])
    {
        if (!percentage.IsMap() || percentage.size() == 0)
        {
            error = "屬性加成「百分比」必須是非空映射表";
            return false;
        }
        PayloadView percentagePayload(percentage, attributePercentagePayload);
        if (!percentagePayload.validate(error)) return false;
        for (const auto& [label, value] : percentagePayload.dynamicEntries())
        {
            if (!appendAttribute(
                    label,
                    value,
                    AttributeOperation::PercentAdd,
                    percentageActions)) return false;
        }
        if (!percentagePayload.finish(error)) return false;
    }
    if (fixedActions.empty() && percentageActions.empty())
    {
        error = "屬性加成至少需要一個屬性";
        return false;
    }

    out.clear();
    const auto authoringOrder = [](BattleAttribute attribute)
    {
        if (attribute == BattleAttribute::DodgeChance)
            return static_cast<int>(BattleAttribute::CriticalChance);
        if (attribute == BattleAttribute::CriticalChance)
            return static_cast<int>(BattleAttribute::DodgeChance);
        return static_cast<int>(attribute);
    };
    const auto byCanonicalAttribute = [&](const auto& left, const auto& right)
    {
        return authoringOrder(left.attribute) < authoringOrder(right.attribute);
    };
    std::ranges::sort(fixedActions, byCanonicalAttribute);
    std::ranges::sort(percentageActions, byCanonicalAttribute);
    for (auto& action : fixedActions) out.push_back(EffectAction{ std::move(action) });
    for (auto& action : percentageActions) out.push_back(EffectAction{ std::move(action) });
    return payload.finish(error);
}

bool parseNamedAction(
    std::string_view name,
    const YAML::Node& payload,
    std::vector<EffectAction>& out,
    std::string& error)
{
    out.clear();
    if (const auto* descriptor = findActionDescriptor(name))
    {
        if (payload.IsMap() && payload["類型"])
        {
            error = std::format("具名動作「{}」不可包含舊式「類型」欄位", name);
            return false;
        }
        PayloadView payloadView(payload, *descriptor->payload);
        if (!payloadView.validate(error)) return false;
        EffectAction action;
        if (!parseActionPayload(*descriptor, payloadView, action, error)
            || !payloadView.finish(error)) return false;
        if (action.value.index() != descriptor->variantIndex)
        {
            error = std::format("動作「{}」dispatch 到錯誤的 typed variant", name);
            return false;
        }
        out.push_back(std::move(action));
        return true;
    }
    const auto* macro = findMacroDescriptor(name);
    if (!macro)
    {
        error = std::format("未知動作「{}」", name);
        return false;
    }
    if (payload.IsMap() && payload["類型"])
    {
        error = std::format("具名動作「{}」不可包含舊式「類型」欄位", name);
        return false;
    }
    if (macro->payloadKind == MacroPayloadKind::AttributeBonus)
        return parseAttributeBonusMacro(payload, out, error);
    if (macro->payloadKind == MacroPayloadKind::Poison)
    {
        PayloadView poisonPayload(payload, *macro->payload);
        if (!poisonPayload.validate(error)) return false;
        ApplyStatusAction action;
        action.status = BattleStatusKind::Poison;
        if (!requiredInt(poisonPayload, "層數", action.stacks, error)
            || action.stacks <= 0
            || !parseEffectNumberNode(poisonPayload["持續幀數"], action.duration.emplace(), error)
            || !parseEffectNumberNode(poisonPayload["強度"], action.potency, error))
        {
            if (error.empty()) error = "施毒層數必須是正整數";
            return false;
        }
        action.stackLimit = action.stacks;
        action.stack = EffectStackPolicy::KeepStrongest;
        action.aggregatePotencyWithinEvent = true;
        if (const auto mode = poisonPayload["模式"])
        {
            const auto label = mode.as<std::string>();
            if (label != "取代重設")
            {
                error = std::format("未知施毒模式「{}」", label);
                return false;
            }
            action.stack = EffectStackPolicy::Replace;
            action.aggregatePotencyWithinEvent = false;
        }
        if (const auto duration = effectiveConstantEffectNumberValue(*action.duration))
        {
            action.durationFrames = *duration;
            action.duration.reset();
        }
        if (!poisonPayload.finish(error)) return false;
        out.push_back(EffectAction{ std::move(action) });
        return true;
    }
    if (name == "回復資源" || name == "獲得資源" || name == "奪取資源")
    {
        EffectAction action;
        if (!parseNamedResourceAction(name, payload, action, error)) return false;
        out.push_back(std::move(action));
        return true;
    }
    if (name == "回復內力" || name == "獲得護盾")
    {
        ChangeResourceAction action;
        action.resource = name == "回復內力" ? BattleResource::Mp : BattleResource::Shield;
        action.kind = name == "回復內力" ? ResourceChangeKind::Restore : ResourceChangeKind::Grant;
        if (!parseNonEmptyEffectNumber(payload, action.amount, error)) return false;
        out.push_back(EffectAction{ std::move(action) });
        return true;
    }
    if (name == "回復生命")
    {
        ChangeResourceAction action;
        action.resource = BattleResource::Hp;
        action.kind = ResourceChangeKind::Restore;
        if (payload.IsMap() && payload.size() == 0)
        {
            error = "回復生命不可使用空映射表";
            return false;
        }
        const bool metadataPayload = payload.IsMap()
            && (payload["數值"] || payload["治療種類"] || payload["來源政策"]);
        if (metadataPayload)
        {
            PayloadView healPayload(payload, *macro->payload);
            if (!healPayload.validate(error)
                || !parseNonEmptyEffectNumber(healPayload["數值"], action.amount, error)
                || !parseResourceMetadata(healPayload, action, error)
                || !healPayload.finish(error)) return false;
        }
        else if (!parseNonEmptyEffectNumber(payload, action.amount, error)) return false;
        out.push_back(EffectAction{ std::move(action) });
        return true;
    }
    if (name == "忽略防禦" || name == "單次承傷上限")
    {
        ModifyDamageAction action;
        if (!parseNonEmptyEffectNumber(payload, action.amount, error)) return false;
        if (name == "忽略防禦")
        {
            action.perspective = DamageModifierPerspective::Outgoing;
            action.stage = DamageModifierStage::BeforeDefense;
            action.channel = DamageChannel::Skill;
            action.operation = DamageModifierOperation::IgnoreDefensePercent;
        }
        else
        {
            action.perspective = DamageModifierPerspective::Incoming;
            action.stage = DamageModifierStage::Final;
            action.channel = DamageChannel::All;
            action.operation = DamageModifierOperation::CapSingleHitAtMaxHpPercent;
        }
        out.push_back(EffectAction{ std::move(action) });
        return true;
    }
    if (name == "擊退" || name == "拉近")
    {
        PayloadView movePayload(payload, *macro->payload);
        if (!movePayload.validate(error)) return false;
        ForceMoveAction action;
        action.direction = name == "擊退"
            ? ForceMoveDirection::AwayFromSource
            : ForceMoveDirection::TowardSource;
        action.collision = ForceMoveCollision::StopBeforeBlocked;
        action.blocked = ForceMoveBlockedResult::Shorten;
        if (!optionalInt(movePayload, "距離格數", action.distanceTiles, error)
            || !optionalInt(movePayload, "距離像素", action.distancePixels, error)
            || !optionalInt(movePayload, "鎖定幀數", action.lockFrames, error)
            || !movePayload.finish(error)) return false;
        out.push_back(EffectAction{ std::move(action) });
        return true;
    }

    error = std::format("動作巨集「{}」尚未實作", name);
    return false;
}

bool parseAuthorActionNode(
    const YAML::Node& node,
    std::vector<EffectAction>& out,
    std::string& error)
{
    if (!node || !node.IsMap())
    {
        error = "動作必須是映射表";
        return false;
    }
    if (!validateUniqueKeys(node, error)) return false;
    if (node.size() != 1)
    {
        error = "動作必須恰有一個具名動作欄位";
        return false;
    }
    const auto entry = *node.begin();
    return parseNamedAction(
        entry.first.as<std::string>(),
        entry.second,
        out,
        error);
}

}  // namespace

bool parseEffectRule(
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
        PayloadView payload(node, rulePayload);
        if (!payload.validate(error)) return fail(error);
        std::string promotedAction;
        for (const auto& [key, value] : payload.dynamicEntries())
        {
            static_cast<void>(value);
            if (!promotedAction.empty())
                return fail("規則只能提升一個具名動作");
            promotedAction = key;
        }
        if (payload["動作"] && !promotedAction.empty())
            return fail("規則不可同時使用提升動作與「動作」列表");
        if (!payload["動作"] && promotedAction.empty())
            return fail("規則需要一個提升動作或非空「動作」列表");

        out = {};
        out.id = id;
        std::size_t automaticConditionCount{};
        std::string timing;
        if (!requiredString(payload, "時機", timing, error)) return fail(error);
        const auto* timingDescriptor = findTimingDescriptor(timing);
        if (!timingDescriptor) return fail(std::format("未知時機「{}」", timing));
        out.event = timingDescriptor->event;
        out.selector.kind = timingDescriptor->defaultTarget;
        if (timingDescriptor->intervalPolicy == TimingIntervalPolicy::Forbidden
            && payload["間隔幀數"])
        {
            return fail("時機「每幀」禁止「間隔幀數」");
        }
        if (timingDescriptor->intervalPolicy == TimingIntervalPolicy::RequiredPositive)
        {
            if (!payload["間隔幀數"])
                return fail("時機「每隔」需要「間隔幀數」");
            if (!requiredInt(payload, "間隔幀數", out.intervalFrames, error)) return fail(error);
            if (out.intervalFrames <= 0)
                return fail("時機「每隔」的「間隔幀數」必須是正整數");
        }
        if (timingDescriptor->intent != TimingIntent::None)
        {
            out.conditions.push_back(DamagePerspectiveCondition{
                timingDescriptor->intent == TimingIntent::DamageReceived
                    ? DamagePerspective::Received
                    : DamagePerspective::Dealt,
            });
            if (timingDescriptor->intent == TimingIntent::Kill)
                out.conditions.push_back(DamageKilledTargetCondition{});
            automaticConditionCount = out.conditions.size();
        }
        if (const auto observation = payload["觀察範圍"])
        {
            const auto label = observation.as<std::string>();
            const auto parsed = parseLabel<EffectObservationScope>(label, observationScopeEnum);
            if (!parsed) return fail(std::format("未知觀察範圍「{}」", label));
            out.observation = *parsed;
        }
        if (const auto castMatch = payload["施放匹配"])
        {
            const auto label = castMatch.as<std::string>();
            const auto parsed = parseLabel<EffectCastMatch>(label, castMatchEnum);
            if (!parsed) return fail(std::format("未知施放匹配「{}」", label));
            out.castMatch = *parsed;
        }
        if (payload["目標"] && !parseSelectorNode(payload["目標"], out.selector, error)) return fail(error);
        if (const auto conditions = payload["條件"])
        {
            if (!conditions.IsSequence()) return fail("「條件」必須是列表");
            for (std::size_t index = 0; index < conditions.size(); ++index)
            {
                EffectCondition condition;
                if (!parseConditionNode(conditions[index], condition, error))
                    return fail(std::format("條件#{}: {}", index + 1, error));
                const auto automatic = std::ranges::find_if(
                    out.conditions.begin(),
                    out.conditions.begin() + static_cast<std::ptrdiff_t>(automaticConditionCount),
                    [&](const EffectCondition& candidate)
                    {
                        return candidate.index() == condition.index();
                    });
                if (automatic != out.conditions.begin()
                    + static_cast<std::ptrdiff_t>(automaticConditionCount))
                {
                    *automatic = std::move(condition);
                    continue;
                }
                out.conditions.push_back(std::move(condition));
            }
        }
        if (!optionalInt(payload, "機率", out.chancePct, error)
            || !optionalInt(payload, "次數", out.maxActivations, error)
            || !optionalInt(payload, "同來源冷卻幀數", out.sharedCooldownFrames, error)
            || (timingDescriptor->intervalPolicy != TimingIntervalPolicy::RequiredPositive
                && !optionalInt(payload, "間隔幀數", out.intervalFrames, error))
            || !optionalInt(payload, "每N次事件", out.everyNthEvent, error)) return fail(error);
        if (const auto activationLimit = payload["觸發限制"])
        {
            EffectActivationLimit parsed;
            if (!parseActivationLimitNode(activationLimit, parsed, error)) return fail(error);
            out.activationLimit = parsed;
        }
        if (payload["重複次數"])
        {
            EffectNumber count;
            if (!parseEffectNumberNode(payload["重複次數"], count, error)) return fail(error);
            out.repetitionCount = std::move(count);
        }
        if (!promotedAction.empty())
        {
            if (!parseNamedAction(promotedAction, payload[promotedAction], out.actions, error))
                return fail(error);
        }
        else if (!parseActionList(payload["動作"], out.actions, error)) return fail(error);
        if (std::ranges::any_of(out.actions, [](const EffectAction& action)
            {
                const auto* move = std::get_if<ForceMoveAction>(&action.value);
                return move && move->distancePixels > 0;
            })
            && (out.actions.size() != 1
                || out.everyNthEvent > 0
                || out.activationLimit
                || out.repetitionCount
                || out.maxActivations > 0
                || out.sharedCooldownFrames > 0))
        {
            return fail("像素擊退／拉近屬於精確階段，必須是唯一動作，且不可設定一般規則觸發記帳欄位");
        }
        if (!payload.finish(error)
            || !validateEffectRule(out, error)) return fail(error);
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        return fail(std::format("解析效果規則時發生 YAML 異常: {}", ex.what()));
    }
}

bool validateEffectAuthoringDescriptorProbes(std::string& error)
{
    error.clear();
    try
    {
        const auto conditionAuthorNode = [](const ConditionDescriptor& descriptor, const YAML::Node& payload)
        {
            if (descriptor.form == ConditionAuthorForm::Scalar)
                return YAML::Node(std::string(descriptor.name));
            YAML::Node author(YAML::NodeType::Map);
            if (descriptor.form == ConditionAuthorForm::SingleParameter)
                author[std::string(descriptor.name)] = payload[std::string(descriptor.singleParameterField)];
            else
                author[std::string(descriptor.name)] = payload;
            return author;
        };
        const auto runPayloadProbe = [&](const PayloadProbeDescriptor& probe, const YAML::Node& payload)
        {
            switch (probe.kind)
            {
            case PayloadProbeKind::EffectNumber:
            {
                EffectNumber value;
                return parseEffectNumberNode(payload, value, error);
            }
            case PayloadProbeKind::Selector:
            {
                EffectSelector selector;
                return parseSelectorNode(payload, selector, error);
            }
            case PayloadProbeKind::Condition:
            {
                const auto* descriptor = findConditionDescriptor(probe.authorName);
                assert(descriptor);
                EffectCondition condition;
                return parseConditionNode(conditionAuthorNode(*descriptor, payload), condition, error);
            }
            case PayloadProbeKind::Action:
            case PayloadProbeKind::Macro:
            {
                std::vector<EffectAction> actions;
                return parseNamedAction(probe.authorName, payload, actions, error);
            }
            case PayloadProbeKind::ActivationLimit:
            {
                EffectActivationLimit limit;
                return parseActivationLimitNode(payload, limit, error);
            }
            case PayloadProbeKind::AttackRuntimeBehavior:
            {
                AttackRuntimeBehavior behavior;
                return parseAttackRuntimeBehavior(payload, behavior, error);
            }
            case PayloadProbeKind::AreaModifier:
            {
                AreaModifier modifier;
                return parseAreaModifierNode(payload, modifier, error);
            }
            case PayloadProbeKind::AreaProjectile:
            {
                auto parent = YAML::Load(std::string(dealDamagePayload.minimalProbe));
                parent["區域投射物"] = payload;
                std::vector<EffectAction> actions;
                return parseNamedAction("造成傷害", parent, actions, error);
            }
            case PayloadProbeKind::AutoUltimate:
            {
                auto parent = YAML::Load(std::string(modifyCastPayload.minimalProbe));
                parent["自動絕招"] = payload;
                std::vector<EffectAction> actions;
                return parseNamedAction("修改施放", parent, actions, error);
            }
            case PayloadProbeKind::AttributePercentage:
            {
                YAML::Node parent(YAML::NodeType::Map);
                parent["百分比"] = payload;
                std::vector<EffectAction> actions;
                return parseNamedAction("屬性加成", parent, actions, error);
            }
            case PayloadProbeKind::Rule:
            {
                ChessDiagnosticCollector diagnostics;
                EffectRule rule;
                if (parseEffectRule(
                        payload,
                        rule,
                        EffectRuleId{ 1 },
                        "descriptor probe",
                        diagnostics.sink())) return true;
                if (!diagnostics.diagnostics().empty())
                    error = diagnostics.diagnostics().back().message;
                return false;
            }
            }
            assert(false);
            return false;
        };

        for (const auto& descriptor : EffectAuthoring::actionDescriptors())
        {
            std::vector<EffectAction> actions;
            if (!parseNamedAction(
                    descriptor.name,
                    YAML::Load(std::string(descriptor.payload->minimalProbe)),
                    actions,
                    error))
            {
                error = std::format("動作 descriptor「{}」probe 失敗: {}", descriptor.name, error);
                return false;
            }
            if (actions.size() != 1 || actions.front().value.index() != descriptor.variantIndex)
            {
                error = std::format("動作 descriptor「{}」dispatch 到錯誤 variant", descriptor.name);
                return false;
            }
            if (descriptor.mechanism)
            {
                const auto* machine = std::get_if<StateMachineAction>(&actions.front().value);
                if (!machine
                    || machine->index()
                        != stateMachineMechanismVariantIndex(*descriptor.mechanism))
                {
                    error = std::format(
                        "機制 descriptor「{}」dispatch 到錯誤狀態機 variant",
                        descriptor.name);
                    return false;
                }

                bool acceptedAllowedEvent = false;
                bool rejectedDisallowedEvent = false;
                std::string lastValidationError;
                for (const auto& timing : KysChess::EffectAuthoring::timingDescriptors())
                {
                    EffectRule rule;
                    rule.event = timing.event;
                    rule.selector.kind = timing.defaultTarget;
                    rule.actions = actions;
                    std::string validationError;
                    const bool valid = validateEffectRule(rule, validationError);
                    if (descriptor.eventAllowed(timing.event))
                    {
                        if (valid) acceptedAllowedEvent = true;
                        else lastValidationError = std::move(validationError);
                    }
                    else if (!valid)
                    {
                        rejectedDisallowedEvent = true;
                    }
                }
                if (!acceptedAllowedEvent || !rejectedDisallowedEvent)
                {
                    error = std::format(
                        "機制 descriptor「{}」未通過允許/拒絕事件驗證{}{}",
                        descriptor.name,
                        lastValidationError.empty() ? "" : "：",
                        lastValidationError);
                    return false;
                }
            }
        }
        for (const auto& descriptor : EffectAuthoring::conditionDescriptors())
        {
            EffectCondition condition;
            const auto payload = YAML::Load(std::string(descriptor.payload->minimalProbe));
            if (!parseConditionNode(conditionAuthorNode(descriptor, payload), condition, error))
            {
                error = std::format("條件 descriptor「{}」probe 失敗: {}", descriptor.name, error);
                return false;
            }
            if (condition.index() != descriptor.variantIndex)
            {
                error = std::format("條件 descriptor「{}」dispatch 到錯誤 variant", descriptor.name);
                return false;
            }
        }
        for (const auto& descriptor : EffectAuthoring::macroDescriptors())
        {
            std::vector<EffectAction> actions;
            if (!parseNamedAction(
                    descriptor.name,
                    YAML::Load(std::string(descriptor.payload->minimalProbe)),
                    actions,
                    error))
            {
                error = std::format("動作巨集 descriptor「{}」probe 失敗: {}", descriptor.name, error);
                return false;
            }
            if (actions.empty())
            {
                error = std::format("動作巨集 descriptor「{}」probe 未產生動作", descriptor.name);
                return false;
            }
        }
        for (const auto& probe : payloadProbeDescriptors)
        {
            const auto run = [&](const YAML::Node& payload, std::string_view field)
            {
                error.clear();
                if (runPayloadProbe(probe, payload)) return true;
                error = std::format(
                    "payload descriptor「{}」{}probe 失敗: {}",
                    probe.payload->name,
                    field.empty() ? "最小 " : std::format("欄位「{}」", field),
                    error);
                return false;
            };
            if (!run(YAML::Load(std::string(probe.payload->minimalProbe)), {})) return false;
            for (const auto& field : probe.payload->fields)
            {
                const auto context = field.probeContext.empty()
                    ? probe.payload->minimalProbe
                    : field.probeContext;
                auto payload = YAML::Load(std::string(context));
                payload[std::string(field.name)] = YAML::Load(std::string(field.probeValue));
                if (!run(payload, field.name)) return false;
            }
            if (probe.payload->dynamicKeyClass != PayloadDynamicKeyClass::None)
            {
                auto payload = YAML::Load(std::string(probe.payload->minimalProbe));
                payload[std::string(probe.payload->dynamicProbeKey)] =
                    YAML::Load(std::string(probe.payload->dynamicProbeValue));
                if (!run(payload, probe.payload->dynamicProbeKey)) return false;
            }
        }
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        error = std::format("descriptor probe YAML 無效: {}", ex.what());
        return false;
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

bool parseMagicEffects(
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

bool loadMagicEffectsFile(
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
