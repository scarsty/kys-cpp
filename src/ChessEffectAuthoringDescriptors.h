#pragma once

#include "ChessBattleEffectTypes.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace KysChess::EffectAuthoring
{

struct AuthorEnumLabel
{
    std::string_view name;
    std::int64_t value;
};

struct AuthorEnumDescriptor
{
    std::string_view name;
    std::span<const AuthorEnumLabel> labels;
};

enum class PayloadNodeShape
{
    Any,
    Scalar,
    String,
    Integer,
    Boolean,
    Map,
    Sequence,
    Number,
    Selector,
    ActionNode,
    ActionList,
    ConditionList,
    StringOrSequence,
};

enum class PayloadSchemaReference
{
    None,
    EffectNumber,
    Selector,
    ActionNode,
    ActionList,
    ConditionList,
    Timing,
    Payload,
    PayloadList,
};

struct PayloadDescriptor;

struct PayloadFieldDescriptor
{
    std::string_view name;
    bool required;
    PayloadNodeShape shape;
    std::string_view probeValue{};
    std::string_view probeContext{};
    PayloadSchemaReference schemaReference = PayloadSchemaReference::None;
    const AuthorEnumDescriptor* enumLabels = nullptr;
    const PayloadDescriptor* nestedPayload = nullptr;
};

enum class PayloadDynamicKeyClass
{
    None,
    BattleAttribute,
    NamedAction,
};

struct PayloadDescriptor
{
    std::string_view name;
    std::span<const PayloadFieldDescriptor> fields;
    std::string_view minimalProbe;
    PayloadDynamicKeyClass dynamicKeyClass = PayloadDynamicKeyClass::None;
    PayloadNodeShape dynamicValueShape = PayloadNodeShape::Any;
    std::string_view dynamicProbeKey{};
    std::string_view dynamicProbeValue{};
    std::size_t minimumProperties{};
    std::string_view dynamicAlternativeField{};
};

enum class TimingIntervalPolicy
{
    Unrestricted,
    Forbidden,
    RequiredPositive,
};

enum class TimingIntent
{
    None,
    DamageDealt,
    DamageReceived,
    Kill,
};

struct TimingDescriptor
{
    std::string_view name;
    EffectEvent event;
    EffectSelectorKind defaultTarget;
    TimingIntervalPolicy intervalPolicy;
    TimingIntent intent;
};

enum class ConditionAuthorForm
{
    Scalar,
    SingleParameter,
    Map,
    ScalarOrMap,
};

struct ConditionDescriptor
{
    std::string_view name;
    std::size_t variantIndex;
    ConditionAuthorForm form;
    std::string_view singleParameterField;
    const PayloadDescriptor* payload;
};

enum class ActionPayloadKind
{
    AttributeModifier,
    DamageModifier,
    ResourceChange,
    HealTransactionModifier,
    ApplyStatus,
    ConsumeStatus,
    RemoveStatus,
    Damage,
    Attack,
    ForceMove,
    Area,
    Cast,
    StateMachine,
    Conditional,
};

struct ActionDescriptor
{
    std::string_view name;
    std::size_t variantIndex;
    ActionPayloadKind payloadKind;
    const PayloadDescriptor* payload;
};

enum class MacroPayloadKind
{
    AttributeBonus,
    Resource,
    Number,
    Heal,
    ForceMove,
};

struct MacroDescriptor
{
    std::string_view name;
    MacroPayloadKind payloadKind;
    const PayloadDescriptor* payload;
};

std::span<const TimingDescriptor> timingDescriptors();
std::span<const ConditionDescriptor> conditionDescriptors();
std::span<const ActionDescriptor> actionDescriptors();
std::span<const MacroDescriptor> macroDescriptors();

const AuthorEnumDescriptor& battleAttributeDescriptor();
const AuthorEnumDescriptor& selectorKindDescriptor();
const PayloadDescriptor& effectNumberDescriptor();
const PayloadDescriptor& selectorDescriptor();
const PayloadDescriptor& ruleDescriptor();

const TimingDescriptor* findTimingDescriptor(std::string_view name);
const ConditionDescriptor* findConditionDescriptor(std::string_view name);
const ActionDescriptor* findActionDescriptor(std::string_view name);
const MacroDescriptor* findMacroDescriptor(std::string_view name);

}
