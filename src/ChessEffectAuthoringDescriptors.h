#pragma once

#include "ChessBattleEffectConstraints.h"
#include "ChessBattleEffectTypes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

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

    constexpr EffectEventConstraint eventConstraint() const
    {
        return effectConditionConstraint(variantIndex);
    }
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

enum class EffectAuthoringTier
{
    Primitive,
    Specialized,
};

enum class StateMachineMechanism
{
    ChangeStateValue,
    TransferStateValue,
    RecordMaximumSkillDamage,
    ConsumeRecordAsDamage,
    ConsumeRecordAsShield,
    StartDamageAbsorption,
    SettleDamageAbsorption,
    BorrowEffectRules,
    CopyAttackDefinition,
    SettleRemainingStatusDamage,
    GenerateClones,
    PreventDeath,
    ConfigureProtectReposition,
    ConfigureExecuteReposition,
};

constexpr std::size_t stateMachineMechanismVariantIndex(
    StateMachineMechanism mechanism)
{
    switch (mechanism)
    {
    case StateMachineMechanism::ChangeStateValue: return 0;
    case StateMachineMechanism::TransferStateValue: return 1;
    case StateMachineMechanism::RecordMaximumSkillDamage: return 2;
    case StateMachineMechanism::ConsumeRecordAsDamage:
    case StateMachineMechanism::ConsumeRecordAsShield: return 3;
    case StateMachineMechanism::StartDamageAbsorption: return 4;
    case StateMachineMechanism::SettleDamageAbsorption: return 5;
    case StateMachineMechanism::BorrowEffectRules: return 6;
    case StateMachineMechanism::CopyAttackDefinition: return 7;
    case StateMachineMechanism::SettleRemainingStatusDamage: return 8;
    case StateMachineMechanism::GenerateClones: return 9;
    case StateMachineMechanism::PreventDeath: return 10;
    case StateMachineMechanism::ConfigureProtectReposition:
    case StateMachineMechanism::ConfigureExecuteReposition: return 11;
    }
    std::unreachable();
}

struct ActionDescriptor
{
    std::string_view name;
    std::size_t variantIndex;
    ActionPayloadKind payloadKind;
    const PayloadDescriptor* payload;
    EffectAuthoringTier tier = EffectAuthoringTier::Primitive;
    std::optional<StateMachineMechanism> mechanism;

    constexpr bool eventAllowed(EffectEvent event) const
    {
        return mechanism
            ? effectStateMachineActionAllowedAtEvent(
                stateMachineMechanismVariantIndex(*mechanism), event)
            : effectActionAllowedAtEvent(variantIndex, event);
    }
};

enum class MacroPayloadKind
{
    AttributeBonus,
    Poison,
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
    std::size_t actionVariantIndex;

    constexpr bool eventAllowed(EffectEvent event) const
    {
        return effectActionAllowedAtEvent(actionVariantIndex, event);
    }
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
