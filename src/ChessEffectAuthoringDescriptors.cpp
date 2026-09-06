#include "ChessEffectAuthoringDescriptors.h"
#include "ChessEffectAuthoringMetadata.h"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <ranges>

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
