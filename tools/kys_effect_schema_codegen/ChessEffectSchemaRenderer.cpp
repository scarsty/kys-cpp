#include "ChessEffectSchemaRenderer.h"

#include "ChessBattleEffectSemantics.h"
#include "ChessEffectAuthoringDescriptors.h"

#include <glaze/json.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <expected>
#include <format>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace KysChess::EffectSchemaCodegen
{
namespace
{

using namespace EffectAuthoring;

struct JsonValue
{
    using Array = std::vector<JsonValue>;
    using Object = std::vector<std::pair<std::string, JsonValue>>;
    using Data = std::variant<std::nullptr_t, bool, std::int64_t, std::string, Array, Object>;

    Data data = nullptr;

    JsonValue() = default;
    JsonValue(bool value) : data(value) {}
    JsonValue(int value) : data(static_cast<std::int64_t>(value)) {}
    JsonValue(std::size_t value) : data(static_cast<std::int64_t>(value)) {}
    JsonValue(std::string value) : data(std::move(value)) {}
    JsonValue(std::string_view value) : data(std::string(value)) {}
    JsonValue(const char* value) : data(std::string(value)) {}
    JsonValue(Array value) : data(std::move(value)) {}
    JsonValue(Object value) : data(std::move(value)) {}
};

JsonValue object(std::initializer_list<std::pair<std::string, JsonValue>> fields)
{
    return JsonValue::Object(fields);
}

JsonValue array(std::initializer_list<JsonValue> values)
{
    return JsonValue::Array(values);
}

JsonValue stringArray(std::span<const std::string_view> values)
{
    JsonValue::Array result;
    result.reserve(values.size());
    for (const auto value : values) result.emplace_back(value);
    return result;
}

JsonValue stringArray(const std::vector<std::string_view>& values)
{
    return stringArray(std::span(values));
}

JsonValue::Object& asObject(JsonValue& value)
{
    return std::get<JsonValue::Object>(value.data);
}

const JsonValue::Object& asObject(const JsonValue& value)
{
    return std::get<JsonValue::Object>(value.data);
}

JsonValue* findProperty(JsonValue& value, std::string_view name)
{
    auto& fields = asObject(value);
    const auto found = std::ranges::find(fields, name, [](const auto& field)
    {
        return std::string_view(field.first);
    });
    return found == fields.end() ? nullptr : &found->second;
}

void appendProperty(JsonValue& value, std::string name, JsonValue property)
{
    assert(!findProperty(value, name));
    asObject(value).emplace_back(std::move(name), std::move(property));
}

bool replaceProperty(JsonValue& value, std::string_view name, JsonValue property)
{
    if (auto* existing = findProperty(value, name))
    {
        *existing = std::move(property);
        return true;
    }
    return false;
}

std::expected<std::string, std::string> serializeJsonString(std::string_view value)
{
    const auto result = glz::write_json(std::string(value));
    if (!result) return std::unexpected(glz::format_error(result.error()));
    return *result;
}

std::expected<void, std::string> appendJson(
    const JsonValue& value,
    int indentation,
    std::string& output)
{
    const auto indent = [&](int depth)
    {
        output.append(static_cast<std::size_t>(depth * 2), ' ');
    };
    if (std::holds_alternative<std::nullptr_t>(value.data))
    {
        output += "null";
    }
    else if (const auto* boolean = std::get_if<bool>(&value.data))
    {
        output += *boolean ? "true" : "false";
    }
    else if (const auto* integer = std::get_if<std::int64_t>(&value.data))
    {
        output += std::to_string(*integer);
    }
    else if (const auto* string = std::get_if<std::string>(&value.data))
    {
        const auto serialized = serializeJsonString(*string);
        if (!serialized) return std::unexpected(serialized.error());
        output += *serialized;
    }
    else if (const auto* values = std::get_if<JsonValue::Array>(&value.data))
    {
        if (values->empty())
        {
            output += "[]";
            return {};
        }
        output += "[\n";
        for (std::size_t index = 0; index < values->size(); ++index)
        {
            indent(indentation + 1);
            if (auto result = appendJson((*values)[index], indentation + 1, output); !result)
                return result;
            output += index + 1 == values->size() ? "\n" : ",\n";
        }
        indent(indentation);
        output += ']';
    }
    else
    {
        const auto& fields = std::get<JsonValue::Object>(value.data);
        if (fields.empty())
        {
            output += "{}";
            return {};
        }
        output += "{\n";
        for (std::size_t index = 0; index < fields.size(); ++index)
        {
            indent(indentation + 1);
            const auto key = serializeJsonString(fields[index].first);
            if (!key) return std::unexpected(key.error());
            output += *key;
            output += ": ";
            if (auto result = appendJson(fields[index].second, indentation + 1, output); !result)
                return result;
            output += index + 1 == fields.size() ? "\n" : ",\n";
        }
        indent(indentation);
        output += '}';
    }
    return {};
}

std::expected<std::string, std::string> serialize(const JsonValue& document)
{
    std::string output;
    if (auto result = appendJson(document, 0, output); !result)
        return std::unexpected(result.error());
    output += '\n';
    return output;
}

JsonValue objectSchema(
    JsonValue::Object properties,
    std::vector<std::string_view> required = {},
    bool additionalProperties = false)
{
    auto schema = object({
        { "type", "object" },
        { "properties", JsonValue(std::move(properties)) },
        { "additionalProperties", additionalProperties },
    });
    if (!required.empty()) appendProperty(schema, "required", stringArray(required));
    return schema;
}

JsonValue enumSchema(std::span<const AuthorEnumLabel> labels)
{
    JsonValue::Array values;
    values.reserve(labels.size());
    for (const auto& label : labels) values.emplace_back(label.name);
    return object({
        { "type", "string" },
        { "enum", JsonValue(std::move(values)) },
    });
}

JsonValue enumSchema(std::span<const std::string_view> labels)
{
    return object({
        { "type", "string" },
        { "enum", stringArray(labels) },
    });
}

JsonValue reference(std::string_view name)
{
    return object({{ "$ref", std::format("#/$defs/{}", name) }});
}

std::string contextualName(std::string_view base, EffectEvent event)
{
    return std::format("{}_{}", base, static_cast<int>(event));
}

enum class AuthoringContext
{
    TopLevel,
    StatusBehavior,
    LayeredStatusBehavior,
};

constexpr bool isStatusBehaviorContext(AuthoringContext context)
{
    return context != AuthoringContext::TopLevel;
}

constexpr bool supportsPerLayerValues(AuthoringContext context)
{
    return context == AuthoringContext::LayeredStatusBehavior;
}

std::string contextualName(
    std::string_view base,
    EffectEvent event,
    AuthoringContext context)
{
    switch (context)
    {
    case AuthoringContext::TopLevel:
        return contextualName(base, event);
    case AuthoringContext::StatusBehavior:
        return std::format("{}_{}_status", base, static_cast<int>(event));
    case AuthoringContext::LayeredStatusBehavior:
        return std::format("{}_{}_layered_status", base, static_cast<int>(event));
    }
    std::unreachable();
}

JsonValue effectNumberReference(EffectEvent event, AuthoringContext context)
{
    return reference(contextualName("effectNumber", event, context));
}
JsonValue selectorReference(EffectEvent event, AuthoringContext context)
{
    return reference(contextualName("selector", event, context));
}
JsonValue actionNodeReference(EffectEvent event, AuthoringContext context)
{
    return reference(contextualName("actionNode", event, context));
}
JsonValue conditionReference(EffectEvent event, AuthoringContext context)
{
    return reference(contextualName("condition", event, context));
}

JsonValue actionListSchema(EffectEvent event, AuthoringContext context)
{
    return object({
        { "type", "array" },
        { "minItems", 1 },
        { "items", actionNodeReference(event, context) },
    });
}

JsonValue conditionListSchema(EffectEvent event, AuthoringContext context)
{
    return object({
        { "type", "array" },
        { "items", conditionReference(event, context) },
    });
}

std::expected<JsonValue, std::string> schemaForShape(
    PayloadNodeShape shape,
    EffectEvent event,
    AuthoringContext context)
{
    switch (shape)
    {
    case PayloadNodeShape::Any: return object({});
    case PayloadNodeShape::Scalar:
        return object({{ "type", array({ "string", "integer", "number", "boolean" }) }});
    case PayloadNodeShape::String: return object({{ "type", "string" }});
    case PayloadNodeShape::Integer: return object({{ "type", "integer" }});
    case PayloadNodeShape::Boolean: return object({{ "type", "boolean" }});
    case PayloadNodeShape::Map: return object({{ "type", "object" }});
    case PayloadNodeShape::Sequence: return object({{ "type", "array" }});
    case PayloadNodeShape::Number: return effectNumberReference(event, context);
    case PayloadNodeShape::Selector: return selectorReference(event, context);
    case PayloadNodeShape::ActionNode: return actionNodeReference(event, context);
    case PayloadNodeShape::ActionList: return actionListSchema(event, context);
    case PayloadNodeShape::ConditionList: return conditionListSchema(event, context);
    case PayloadNodeShape::StringOrSequence:
        return object({{ "oneOf", array({
            object({{ "type", "string" }}),
            object({
                { "type", "array" },
                { "items", object({{ "type", "string" }}) },
            }),
        }) }});
    }
    return std::unexpected("未知 payload node shape");
}

std::expected<JsonValue, std::string> descriptorObject(
    const PayloadDescriptor& descriptor,
    EffectEvent event,
    AuthoringContext context);

std::expected<JsonValue, std::string> schemaForField(
    const PayloadFieldDescriptor& field,
    EffectEvent event,
    AuthoringContext context)
{
    switch (field.schemaReference)
    {
    case PayloadSchemaReference::EffectNumber: return effectNumberReference(event, context);
    case PayloadSchemaReference::Selector: return selectorReference(event, context);
    case PayloadSchemaReference::ActionNode: return actionNodeReference(event, context);
    case PayloadSchemaReference::ActionList: return actionListSchema(event, context);
    case PayloadSchemaReference::ConditionList: return conditionListSchema(event, context);
    case PayloadSchemaReference::Timing:
    {
        JsonValue::Array labels;
        for (const auto& timing : timingDescriptors())
        {
            if (context == AuthoringContext::TopLevel
                && timing.event == EffectEvent::StatusPersistent) continue;
            labels.emplace_back(timing.name);
        }
        return object({
            { "type", "string" },
            { "enum", JsonValue(std::move(labels)) },
        });
    }
    case PayloadSchemaReference::Payload:
        if (!field.nestedPayload) return std::unexpected("nested payload metadata 遺失");
        return descriptorObject(*field.nestedPayload, event, context);
    case PayloadSchemaReference::PayloadList:
    {
        if (!field.nestedPayload) return std::unexpected("nested payload list metadata 遺失");
        auto item = descriptorObject(*field.nestedPayload, event, context);
        if (!item) return item;
        return object({
            { "type", "array" },
            { "minItems", 1 },
            { "items", std::move(*item) },
        });
    }
    case PayloadSchemaReference::None: break;
    }

    if (!field.enumLabels) return schemaForShape(field.shape, event, context);

    std::vector<AuthorEnumLabel> allowedLabels;
    for (const auto& label : field.enumLabels->labels)
    {
        bool allowed = true;
        if (field.enumLabels->name == "EffectNumberBase")
            allowed = effectNumberBaseAllowedAtEvent(
                static_cast<EffectNumberBase>(label.value), event)
                && effectNumberBaseAllowedInAuthoringContext(
                    static_cast<EffectNumberBase>(label.value),
                    isStatusBehaviorContext(context))
                && (event != EffectEvent::StatusPersistent
                    || statusNumberBindingPhase(static_cast<EffectNumberBase>(label.value))
                        != StatusNumberBindingPhase::EventLive);
        else if (field.enumLabels->name == "EffectSelectorKind")
            allowed = effectSelectorKindAllowedAtEvent(
                static_cast<EffectSelectorKind>(label.value), event)
                && (isStatusBehaviorContext(context)
                    || static_cast<EffectSelectorKind>(label.value)
                        != EffectSelectorKind::StatusHolder)
                && (event != EffectEvent::StatusPersistent
                    || static_cast<EffectSelectorKind>(label.value)
                        == EffectSelectorKind::StatusHolder);
        else if (field.enumLabels->name == "EffectRequiredTarget")
            allowed = effectRequiredTargetAllowedAtEvent(
                static_cast<EffectRequiredTarget>(label.value), event);
        if (allowed) allowedLabels.push_back(label);
    }
    const auto labels = std::span<const AuthorEnumLabel>(allowedLabels);
    if (field.shape == PayloadNodeShape::String)
        return enumSchema(labels);
    if (field.shape == PayloadNodeShape::Sequence)
    {
        return object({
            { "type", "array" },
            { "items", enumSchema(labels) },
        });
    }
    if (field.shape == PayloadNodeShape::StringOrSequence)
    {
        return object({{ "oneOf", array({
            enumSchema(labels),
            object({
                { "type", "array" },
                { "items", enumSchema(labels) },
            }),
        }) }});
    }
    return std::unexpected(std::format(
        "enum field「{}」使用不支援的 shape", field.name));
}

JsonValue sourceStatusQuantitySchema()
{
    std::vector<std::string_view> labels;
    for (const auto& catalog : statusCatalogEntries())
    {
        if (catalog.quantity != StatusQuantityModel::None
            && catalog.quantity != StatusQuantityModel::Internal)
        {
            labels.push_back(battleStatusLabel(catalog.status));
        }
    }
    return enumSchema(labels);
}

std::expected<JsonValue, std::string> descriptorObject(
    const PayloadDescriptor& descriptor,
    EffectEvent event,
    AuthoringContext context)
{
    JsonValue::Object properties;
    properties.reserve(descriptor.fields.size());
    std::vector<std::string_view> required;
    bool hasValueField{};
    bool hasPerLayerValueField{};
    for (const auto& field : descriptor.fields)
    {
        hasValueField = hasValueField || field.name == "數值";
        hasPerLayerValueField = hasPerLayerValueField || field.name == "每層數值";
    }
    for (const auto& field : descriptor.fields)
    {
        if (field.name == "每層數值" && !supportsPerLayerValues(context))
            continue;
        if (&descriptor == &effectNumberDescriptor())
        {
            if (field.name == "實際生命傷害百分比"
                && !effectEventHas(event, EffectEventCapability::Damage)) continue;
            if (field.name == "目標目前護盾百分比"
                && !effectEventHas(event, EffectEventCapability::CurrentShield)) continue;
            if (event == EffectEvent::StatusPersistent
                && (field.name == "目標最大生命百分比"
                    || field.name == "目標目前生命百分比"
                    || field.name == "目標目前護盾百分比"
                    || field.name == "目標目前冷卻百分比"
                    || field.name == "實際生命傷害百分比")) continue;
        }
        auto schema = [&]() -> std::expected<JsonValue, std::string>
        {
            if (&descriptor == &ruleDescriptor()
                && field.name == "觀察範圍")
            {
                std::vector<AuthorEnumLabel> labels;
                for (const auto& label : field.enumLabels->labels)
                {
                    const auto scope = static_cast<EffectObservationScope>(label.value);
                    if (effectObservationScopeAllowedAtEvent(
                            scope,
                            event,
                            isStatusBehaviorContext(context)))
                        labels.push_back(label);
                }
                return enumSchema(std::span<const AuthorEnumLabel>(labels));
            }
            if (&descriptor == &effectNumberDescriptor()
                && field.name == "來源狀態數量")
                return sourceStatusQuantitySchema();
            return schemaForField(field, event, context);
        }();
        if (!schema) return std::unexpected(std::format(
            "payload「{}」欄位「{}」: {}",
            descriptor.name,
            field.name,
            schema.error()));
        properties.emplace_back(std::string(field.name), std::move(*schema));
        if (field.required) required.push_back(field.name);
    }

    if (descriptor.dynamicKeyClass == PayloadDynamicKeyClass::BattleAttribute)
    {
        for (const auto& attribute : battleAttributeDescriptor().labels)
        {
            auto schema = schemaForShape(descriptor.dynamicValueShape, event, context);
            if (!schema) return std::unexpected(schema.error());
            properties.emplace_back(std::string(attribute.name), std::move(*schema));
        }
    }
    else if (descriptor.dynamicKeyClass != PayloadDynamicKeyClass::None
        && descriptor.dynamicKeyClass != PayloadDynamicKeyClass::NamedAction)
    {
        return std::unexpected("未知 dynamic key class");
    }

    auto schema = objectSchema(std::move(properties), std::move(required));
    if (hasValueField && hasPerLayerValueField)
    {
        JsonValue::Array alternatives;
        alternatives.push_back(object({{ "required", array({ "數值" }) }}));
        if (supportsPerLayerValues(context))
            alternatives.push_back(object({{ "required", array({ "每層數值" }) }}));
        appendProperty(schema, "oneOf", JsonValue(std::move(alternatives)));
    }
    if (descriptor.minimumProperties > 0)
        appendProperty(schema, "minProperties", descriptor.minimumProperties);
    if (!descriptor.dynamicAlternativeField.empty())
    {
        JsonValue::Array alternatives;
        alternatives.push_back(object({{
            "required", array({ descriptor.dynamicAlternativeField }),
        }}));
        for (const auto& attribute : battleAttributeDescriptor().labels)
        {
            alternatives.push_back(object({{
                "required", array({ attribute.name }),
            }}));
        }
        appendProperty(schema, "anyOf", JsonValue(std::move(alternatives)));
    }
    return schema;
}

const PayloadFieldDescriptor* descriptorField(
    const PayloadDescriptor& descriptor,
    std::string_view name)
{
    const auto found = std::ranges::find(descriptor.fields, name,
        &PayloadFieldDescriptor::name);
    return found == descriptor.fields.end() ? nullptr : &*found;
}

JsonValue positiveIntegerSchema()
{
    return object({
        { "type", "integer" },
        { "minimum", 1 },
    });
}

JsonValue constantStringSchema(std::string_view value)
{
    return object({
        { "type", "string" },
        { "const", value },
    });
}

JsonValue requiredTrueSchema()
{
    return object({
        { "type", "boolean" },
        { "const", true },
    });
}

std::expected<JsonValue, std::string> statusReapplicationSchema(
    const PayloadDescriptor& descriptor,
    BattleStatusKind status)
{
    const auto* field = descriptorField(descriptor, "重複套用");
    if (!field || !field->enumLabels)
        return std::unexpected("套用狀態 descriptor 缺少重複套用 enum");
    std::vector<AuthorEnumLabel> labels;
    for (const auto& label : field->enumLabels->labels)
    {
        const auto policy = static_cast<StatusReapplicationPolicy>(label.value);
        if (statusReapplicationPolicyAllowed(status, policy))
            labels.push_back(label);
    }
    return enumSchema(std::span<const AuthorEnumLabel>(labels));
}

JsonValue statusBehaviorRuleListSchema(StatusQuantityModel quantity)
{
    return reference(quantity == StatusQuantityModel::Layers
        ? "layeredStatusBehaviorRuleList"
        : "statusBehaviorRuleList");
}

std::expected<JsonValue, std::string> statusApplicationBranch(
    const PayloadDescriptor& descriptor,
    const StatusCatalogEntry& catalog,
    EffectEvent event,
    AuthoringContext context,
    std::optional<StatusQuantityOperationId> selectedQuantityOperation = std::nullopt)
{
    JsonValue::Object properties;
    std::vector<std::string_view> required{ "狀態" };
    properties.emplace_back(
        "狀態",
        constantStringSchema(battleStatusLabel(catalog.status)));

    if (catalog.duration == StatusDurationModel::RequiredPositive)
    {
        const auto* duration = descriptorField(descriptor, "持續幀數");
        if (!duration) return std::unexpected("套用狀態 descriptor 缺少持續幀數");
        auto schema = schemaForField(*duration, event, context);
        if (!schema) return std::unexpected(schema.error());
        properties.emplace_back("持續幀數", std::move(*schema));
        required.push_back("持續幀數");
    }

    const auto addPositiveInteger = [&](std::string_view name)
    {
        properties.emplace_back(std::string(name), positiveIntegerSchema());
        required.push_back(name);
    };
    const auto allowedQuantityOperations = statusQuantityOperations(catalog.quantity);
    if (catalog.quantity == StatusQuantityModel::Internal)
    {
        return std::unexpected("執行期狀態不可產生作者 schema");
    }
    if (!selectedQuantityOperation && allowedQuantityOperations.size() == 1)
        selectedQuantityOperation = allowedQuantityOperations.front();
    if (allowedQuantityOperations.size() > 1 && !selectedQuantityOperation)
        return std::unexpected("狀態 schema 缺少數量操作分支");
    if (selectedQuantityOperation)
    {
        if (!std::ranges::contains(allowedQuantityOperations, *selectedQuantityOperation))
            return std::unexpected("狀態 schema 使用了不相容的數量操作分支");
        for (const auto field : statusQuantityOperationFields(*selectedQuantityOperation))
            addPositiveInteger(statusQuantityFieldLabel(field));
    }

    for (const auto fieldId : statusNamedNumberFields(catalog.status))
    {
        const auto& field = statusNamedNumberFieldCatalogEntry(fieldId);
        const auto* descriptorValue = descriptorField(descriptor, field.label);
        if (!descriptorValue)
        {
            return std::unexpected(std::format(
                "套用狀態 descriptor 缺少{}", field.label));
        }
        auto schema = schemaForField(*descriptorValue, event, context);
        if (!schema) return std::unexpected(schema.error());
        properties.emplace_back(std::string(field.label), std::move(*schema));
        if (field.required) required.push_back(field.label);
    }

    if (statusReapplicationPolicyRequired(catalog.status))
    {
        auto schema = statusReapplicationSchema(descriptor, catalog.status);
        if (!schema) return std::unexpected(schema.error());
        properties.emplace_back("重複套用", std::move(*schema));
        required.push_back("重複套用");
    }

    if (catalog.authorable
        && (catalog.behaviorClassification == StatusBehaviorClassification::Profiled
            || catalog.behaviorClassification == StatusBehaviorClassification::OpenMarker))
    {
        properties.emplace_back(
            "效果",
            statusBehaviorRuleListSchema(catalog.quantity));
        if (catalog.behaviorClassification == StatusBehaviorClassification::Profiled
            || catalog.behaviorClassification == StatusBehaviorClassification::OpenMarker)
            required.push_back("效果");
    }
    return objectSchema(std::move(properties), std::move(required));
}

std::expected<JsonValue, std::string> closedStatusApplicationSchema(
    const PayloadDescriptor& descriptor,
    EffectEvent event,
    AuthoringContext context)
{
    JsonValue::Array variants;
    for (const auto& catalog : statusCatalogEntries())
    {
        if (!catalog.authorable || catalog.status == BattleStatusKind::Poison) continue;
        const auto quantityOperations = statusQuantityOperations(catalog.quantity);
        if (quantityOperations.size() > 1)
        {
            for (const auto operation : quantityOperations)
            {
                auto branch = statusApplicationBranch(
                    descriptor, catalog, event, context, operation);
                if (!branch) return std::unexpected(branch.error());
                variants.push_back(std::move(*branch));
            }
        }
        else
        {
            auto branch = statusApplicationBranch(descriptor, catalog, event, context);
            if (!branch) return std::unexpected(branch.error());
            variants.push_back(std::move(*branch));
        }
    }
    return object({{ "oneOf", JsonValue(std::move(variants)) }});
}

JsonValue closedPoisonEffectSchema(EffectEvent event)
{
    static_cast<void>(event);
    return statusBehaviorRuleListSchema(StatusQuantityModel::TriggerCharges);
}

std::expected<JsonValue, std::string> closedPoisonApplicationSchema(
    const PayloadDescriptor& descriptor,
    EffectEvent event,
    AuthoringContext context,
    bool allowSameEventMerge)
{
    const auto* duration = descriptorField(descriptor, "持續幀數");
    if (!duration) return std::unexpected("施加中毒 descriptor 缺少持續幀數");
    auto durationSchema = schemaForField(*duration, event, context);
    if (!durationSchema) return std::unexpected(durationSchema.error());
    const auto quantityOperations = statusQuantityOperations(
        StatusQuantityModel::TriggerCharges);
    if (quantityOperations.size() != 1)
        return std::unexpected("中毒 schema 需要唯一的觸發次數操作");
    const auto quantityFields = statusQuantityOperationFields(
        quantityOperations.front());
    if (quantityFields.size() != 1)
        return std::unexpected("中毒 schema 的觸發次數操作需要唯一欄位");
    const auto quantityLabel = statusQuantityFieldLabel(quantityFields.front());

    const auto branch = [&](bool aggregates, JsonValue durationValue)
    {
        JsonValue::Object properties{
            { "持續幀數", std::move(durationValue) },
            { std::string(quantityLabel), positiveIntegerSchema() },
            { "重複套用", constantStringSchema(
                statusReapplicationPolicyLabel(
                    aggregates
                        ? StatusReapplicationPolicy::KeepHigherDamage
                        : StatusReapplicationPolicy::ReplaceExistingPoison)) },
            { "效果", closedPoisonEffectSchema(event) },
        };
        std::vector<std::string_view> required{
            "持續幀數", quantityLabel, "重複套用", "效果",
        };
        if (aggregates)
        {
            properties.emplace_back(
                "同事件合併",
                constantStringSchema("合計傷害百分比"));
            required.push_back("同事件合併");
        }
        return objectSchema(std::move(properties), std::move(required));
    };
    JsonValue::Array variants;
    if (allowSameEventMerge
        && context == AuthoringContext::TopLevel
        && event == EffectEvent::HitBeforeDamage)
    {
        variants.push_back(branch(true, *durationSchema));
    }
    variants.push_back(branch(false, std::move(*durationSchema)));
    return object({{ "oneOf", JsonValue(std::move(variants)) }});
}

struct NamedSchema
{
    std::string_view name;
    JsonValue schema;
};

bool isExclusiveStatusAttackInterceptor(std::string_view name)
{
    return name == "使本次施放攻擊落空"
        || name == "使本次受到攻擊落空";
}

JsonValue persistentModifierMetadataExclusion()
{
    return object({{ "not", object({{ "anyOf", array({
        object({{ "required", array({ "持續幀數" }) }}),
        object({{ "required", array({ "合併方式" }) }}),
        object({{ "required", array({ "層數上限" }) }}),
        object({{ "required", array({ "疊加範圍" }) }}),
    }) }}) }});
}

JsonValue persistentDamageModifierBranch(
    std::string_view perspective,
    std::string_view stage,
    std::string_view channel,
    std::string_view operation)
{
    auto branch = object({{ "properties", object({
        { "方位", constantStringSchema(perspective) },
        { "階段", constantStringSchema(stage) },
        { "傷害種類", constantStringSchema(channel) },
        { "方式", constantStringSchema(operation) },
    }) }});
    if (perspective == "承受")
        appendProperty(branch, "required", array({ "方位" }));
    return branch;
}

void constrainPersistentActionSchemas(std::vector<NamedSchema>& schemas)
{
    std::erase_if(schemas, [](const NamedSchema& named)
    {
        return named.name != "屬性修正"
            && named.name != "傷害修正"
            && named.name != "治療交易修正"
            && named.name != "抵擋非處決正傷害";
    });
    for (auto& named : schemas)
    {
        if (named.name == "屬性修正")
        {
            auto* properties = findProperty(named.schema, "properties");
            assert(properties);
            const bool replacedAttribute = replaceProperty(
                *properties, "屬性", constantStringSchema("速度"));
            const bool replacedOperation = replaceProperty(
                *properties, "方式", constantStringSchema("百分比加算"));
            assert(replacedAttribute && replacedOperation);
            appendProperty(named.schema, "allOf", array({
                persistentModifierMetadataExclusion(),
            }));
        }
        else if (named.name == "傷害修正")
        {
            appendProperty(named.schema, "allOf", array({
                persistentModifierMetadataExclusion(),
                object({{ "oneOf", array({
                    persistentDamageModifierBranch("造成", "防禦前", "招式", "百分比加算"),
                    persistentDamageModifierBranch("承受", "防禦前", "全部", "百分比加算"),
                    persistentDamageModifierBranch("承受", "最終", "全部", "百分比加算"),
                    persistentDamageModifierBranch("承受", "最終", "全部", "單次承傷上限"),
                }) }}),
            }));
        }
    }
}

void constrainStatusBehaviorExactRuntimeActions(std::vector<NamedSchema>& schemas)
{
    for (auto& named : schemas)
    {
        if (named.name == "強制移動")
        {
            appendProperty(named.schema, "not", object({{
                "required", array({ "距離像素" }),
            }}));
        }
        else if (named.name == "修改施放")
        {
            auto* properties = findProperty(named.schema, "properties");
            assert(properties);
            const bool rangeConstrained = replaceProperty(
                *properties,
                "射程模式",
                constantStringSchema("保留"));
            const bool mobilityConstrained = replaceProperty(
                *properties,
                "機動政策",
                constantStringSchema("保留"));
            assert(rangeConstrained && mobilityConstrained);
            appendProperty(named.schema, "not", object({{ "anyOf", array({
                object({{ "required", array({ "彈道速度百分比" }) }}),
                object({{ "required", array({ "最小選擇距離" }) }}),
                object({{ "required", array({ "追加彈道數" }) }}),
            }) }}));
        }
        else if (named.name == "修改攻擊")
        {
            appendProperty(named.schema, "allOf", array({ object({{
                "properties", object({{
                    "執行行為", object({
                        { "properties", object({{
                            "類型", constantStringSchema("擴張螺旋"),
                        }}) },
                        { "required", array({ "類型" }) },
                    }),
                }}),
            }}) }));
        }
    }
}

std::expected<std::vector<NamedSchema>, std::string> actionPayloads(
    EffectEvent event,
    AuthoringContext context,
    bool allowPoisonSameEventMerge = false)
{
    std::vector<NamedSchema> result;
    result.reserve(actionDescriptors().size());
    for (const auto& descriptor : actionDescriptors())
    {
        if (!descriptor.eventAllowed(event)) continue;
        if (context == AuthoringContext::TopLevel
            && descriptor.payloadKind == ActionPayloadKind::StatusContext) continue;
        std::expected<JsonValue, std::string> schema = [&]()
            -> std::expected<JsonValue, std::string>
        {
            if (descriptor.name == "套用狀態")
                return closedStatusApplicationSchema(*descriptor.payload, event, context);
            if (descriptor.name == "施加中毒")
            {
                return closedPoisonApplicationSchema(
                    *descriptor.payload,
                    event,
                    context,
                    allowPoisonSameEventMerge);
            }
            return descriptorObject(*descriptor.payload, event, context);
        }();
        if (!schema) return std::unexpected(schema.error());
        if (descriptor.payloadKind == ActionPayloadKind::Conditional)
        {
            auto* properties = findProperty(*schema, "properties");
            assert(properties);
            auto condition = object({
                { "type", "array" },
                { "minItems", 1 },
                { "items", conditionReference(event, context) },
            });
            if (!replaceProperty(*properties, "條件", std::move(condition)))
                return std::unexpected("條件分支 descriptor 缺少條件欄位");
        }
        result.push_back({ descriptor.name, std::move(*schema) });
    }
    if (isStatusBehaviorContext(context))
    {
        constrainStatusBehaviorExactRuntimeActions(result);
        if (event == EffectEvent::StatusPersistent)
            constrainPersistentActionSchemas(result);
    }
    return result;
}

std::expected<std::vector<NamedSchema>, std::string> macroPayloads(
    EffectEvent event,
    AuthoringContext context)
{
    std::vector<NamedSchema> result;
    if (isStatusBehaviorContext(context)
        && event == EffectEvent::StatusPersistent) return result;
    result.reserve(macroDescriptors().size());
    for (const auto& descriptor : macroDescriptors())
    {
        if (!descriptor.eventAllowed(event)) continue;
        JsonValue schema;
        if (descriptor.payloadKind == MacroPayloadKind::Number)
        {
            schema = effectNumberReference(event, context);
        }
        else
        {
            auto payload = descriptorObject(*descriptor.payload, event, context);
            if (!payload) return std::unexpected(payload.error());
            schema = descriptor.payloadKind == MacroPayloadKind::Heal
                ? object({{ "oneOf", array({ effectNumberReference(event, context), std::move(*payload) }) }})
                : std::move(*payload);
        }
        result.push_back({ descriptor.name, std::move(schema) });
    }
    return result;
}

std::expected<JsonValue, std::string> commonDefinitions()
{
    JsonValue::Object definitions;
    for (const auto context : {
             AuthoringContext::TopLevel,
             AuthoringContext::StatusBehavior,
             AuthoringContext::LayeredStatusBehavior })
    {
        std::array<bool, static_cast<std::size_t>(EffectEvent::StatusPersistent) + 1> generated{};
        for (const auto& timing : timingDescriptors())
        {
            const auto eventIndex = static_cast<std::size_t>(timing.event);
            if (generated[eventIndex]) continue;
            generated[eventIndex] = true;

            auto selectorMap = descriptorObject(selectorDescriptor(), timing.event, context);
            auto numberMap = descriptorObject(effectNumberDescriptor(), timing.event, context);
            if (!selectorMap) return std::unexpected(selectorMap.error());
            if (!numberMap) return std::unexpected(numberMap.error());

            std::vector<AuthorEnumLabel> selectorLabels;
            for (const auto& label : selectorKindDescriptor().labels)
            {
                const auto selector = static_cast<EffectSelectorKind>(label.value);
                if (effectSelectorKindAllowedAtEvent(selector, timing.event)
                    && (isStatusBehaviorContext(context)
                        || selector != EffectSelectorKind::StatusHolder))
                    selectorLabels.push_back(label);
            }
            definitions.emplace_back(
                contextualName("selector", timing.event, context),
                object({{ "oneOf", array({
                    enumSchema(std::span<const AuthorEnumLabel>(selectorLabels)),
                    std::move(*selectorMap),
                }) }}));
            definitions.emplace_back(
                contextualName("effectNumber", timing.event, context),
                object({{ "oneOf", array({
                    object({{ "type", "integer" }}),
                    std::move(*numberMap),
                }) }}));

            const auto conditionAllowed = [&](const ConditionDescriptor& descriptor)
            {
                return descriptor.eventConstraint().allows(timing.event)
                    && (isStatusBehaviorContext(context)
                        || !descriptor.statusBehaviorOnly);
            };
            JsonValue::Array conditionVariants;
            std::vector<std::string_view> scalarConditions;
            for (const auto& descriptor : conditionDescriptors())
            {
                if (!conditionAllowed(descriptor)) continue;
                if (descriptor.form == ConditionAuthorForm::Scalar
                    || descriptor.form == ConditionAuthorForm::ScalarOrMap)
                    scalarConditions.push_back(descriptor.name);
            }
            if (!scalarConditions.empty())
                conditionVariants.push_back(enumSchema(std::span(scalarConditions)));
            for (const auto& descriptor : conditionDescriptors())
            {
                if (!conditionAllowed(descriptor)) continue;
                if (descriptor.form == ConditionAuthorForm::SingleParameter)
                {
                    assert(descriptor.payload->fields.size() == 1);
                    auto value = schemaForField(
                        descriptor.payload->fields.front(), timing.event, context);
                    if (!value) return std::unexpected(value.error());
                    if (descriptor.payload->fields.front().shape == PayloadNodeShape::Sequence)
                        appendProperty(*value, "minItems", 1);
                    conditionVariants.push_back(objectSchema(
                        {{ std::string(descriptor.name), std::move(*value) }},
                        { descriptor.name }));
                }
                else if (descriptor.form == ConditionAuthorForm::Map
                    || descriptor.form == ConditionAuthorForm::ScalarOrMap)
                {
                    auto payload = descriptorObject(*descriptor.payload, timing.event, context);
                    if (!payload) return std::unexpected(payload.error());
                    conditionVariants.push_back(objectSchema(
                        {{ std::string(descriptor.name), std::move(*payload) }},
                        { descriptor.name }));
                }
            }
            definitions.emplace_back(
                contextualName("condition", timing.event, context),
                object({{ "oneOf", JsonValue(std::move(conditionVariants)) }}));

            auto actions = actionPayloads(timing.event, context);
            auto macros = macroPayloads(timing.event, context);
            if (!actions) return std::unexpected(actions.error());
            if (!macros) return std::unexpected(macros.error());
            std::vector<NamedSchema> namedSchemas = std::move(*actions);
            namedSchemas.insert(
                namedSchemas.end(),
                std::make_move_iterator(macros->begin()),
                std::make_move_iterator(macros->end()));

            JsonValue::Array actionVariants;
            actionVariants.reserve(namedSchemas.size());
            for (const auto& named : namedSchemas)
            {
                // 攻擊攔截器在獨立的命中前仲裁階段執行。將它排除於動作列表
                // （以及條件分支）之外，只允許直接提升的單一動作寫法，避免
                // 同規則的其他命令被攔截階段忽略。
                if (isStatusBehaviorContext(context)
                    && isExclusiveStatusAttackInterceptor(named.name))
                {
                    continue;
                }
                actionVariants.push_back(objectSchema(
                    {{ std::string(named.name), named.schema }},
                    { named.name }));
            }
            definitions.emplace_back(
                contextualName("actionNode", timing.event, context),
                object({{ "oneOf", JsonValue(std::move(actionVariants)) }}));
        }
    }

    for (const auto context : {
             AuthoringContext::TopLevel,
             AuthoringContext::StatusBehavior,
             AuthoringContext::LayeredStatusBehavior })
    {
        JsonValue::Array ruleVariants;
        std::size_t timingIndex{};
        for (const auto& timing : timingDescriptors())
        {
            if (context == AuthoringContext::TopLevel
                && timing.event == EffectEvent::StatusPersistent) continue;
            auto actions = actionPayloads(timing.event, context, true);
            auto macros = macroPayloads(timing.event, context);
            if (!actions) return std::unexpected(actions.error());
            if (!macros) return std::unexpected(macros.error());
            std::vector<NamedSchema> namedSchemas = std::move(*actions);
            namedSchemas.insert(
                namedSchemas.end(),
                std::make_move_iterator(macros->begin()),
                std::make_move_iterator(macros->end()));

            auto rule = descriptorObject(ruleDescriptor(), timing.event, context);
            if (!rule) return std::unexpected(rule.error());
            auto* ruleProperties = findProperty(*rule, "properties");
            assert(ruleProperties);
            replaceProperty(*ruleProperties, "時機", object({{ "const", timing.name }}));
            for (const auto& named : namedSchemas)
                appendProperty(*ruleProperties, std::string(named.name), named.schema);

            if (isStatusBehaviorContext(context)
                && timing.event == EffectEvent::StatusPersistent)
            {
                const bool replacedObservation = replaceProperty(
                    *ruleProperties,
                    "觀察範圍",
                    constantStringSchema("狀態持有者事件來源"));
                const bool replacedCastMatch = replaceProperty(
                    *ruleProperties,
                    "施放匹配",
                    constantStringSchema("綁定武功"));
                const bool replacedTarget = replaceProperty(
                    *ruleProperties,
                    "目標",
                    constantStringSchema("狀態持有者"));
                assert(replacedObservation && replacedCastMatch && replacedTarget);
                appendProperty(*rule, "not", object({{ "anyOf", array({
                    object({{ "required", array({ "條件" }) }}),
                    object({{ "required", array({ "機率" }) }}),
                    object({{ "required", array({ "次數" }) }}),
                    object({{ "required", array({ "同來源冷卻幀數" }) }}),
                    object({{ "required", array({ "間隔幀數" }) }}),
                    object({{ "required", array({ "每N次事件" }) }}),
                    object({{ "required", array({ "觸發限制" }) }}),
                    object({{ "required", array({ "重複次數" }) }}),
                }) }}));
            }

            if (context == AuthoringContext::TopLevel
                && timing.event == EffectEvent::HitBeforeDamage)
            {
                appendProperty(*rule, "allOf", array({ object({
                    { "if", object({
                        { "required", array({ "施加中毒" }) },
                        { "properties", object({
                            { "施加中毒", object({
                                { "required", array({ "同事件合併" }) },
                            }) },
                        }) },
                    }) },
                    { "then", object({
                        { "properties", object({
                            { "目標", constantStringSchema("命中目標") },
                        }) },
                        { "not", object({{ "anyOf", array({
                            object({{ "required", array({ "條件" }) }}),
                            object({{ "required", array({ "機率" }) }}),
                            object({{ "required", array({ "次數" }) }}),
                            object({{ "required", array({ "同來源冷卻幀數" }) }}),
                            object({{ "required", array({ "間隔幀數" }) }}),
                            object({{ "required", array({ "每N次事件" }) }}),
                            object({{ "required", array({ "觸發限制" }) }}),
                            object({{ "required", array({ "重複次數" }) }}),
                        }) }}) },
                    }) },
                }) }));
            }

            JsonValue::Array actionChoice;
            actionChoice.push_back(object({{ "required", array({ "動作" }) }}));
            for (const auto& named : namedSchemas)
                actionChoice.push_back(object({{ "required", array({ named.name }) }}));
            appendProperty(*rule, "oneOf", JsonValue(std::move(actionChoice)));

            if (timing.intervalPolicy == TimingIntervalPolicy::Forbidden)
                appendProperty(*rule, "not", object({{ "required", array({ "間隔幀數" }) }}));
            else if (timing.intervalPolicy == TimingIntervalPolicy::RequiredPositive)
            {
                replaceProperty(*rule, "required", array({ "時機", "間隔幀數" }));
                replaceProperty(*ruleProperties, "間隔幀數", object({
                    { "type", "integer" },
                    { "minimum", 1 },
                }));
            }

            const auto ruleName = context == AuthoringContext::TopLevel
                ? std::format("rule_{}", timingIndex++)
                : context == AuthoringContext::LayeredStatusBehavior
                    ? std::format("layeredStatusRule_{}", timingIndex++)
                    : std::format("statusRule_{}", timingIndex++);
            definitions.emplace_back(ruleName, std::move(*rule));
            ruleVariants.push_back(reference(ruleName));
        }
        definitions.emplace_back(
            context == AuthoringContext::TopLevel
                ? "ruleList"
                : context == AuthoringContext::LayeredStatusBehavior
                    ? "layeredStatusBehaviorRuleList"
                    : "statusBehaviorRuleList",
            object({
                { "type", "array" },
                { "minItems", 1 },
                { "items", object({{ "oneOf", JsonValue(std::move(ruleVariants)) }}) },
            }));
    }
    return JsonValue(std::move(definitions));
}

std::expected<JsonValue, std::string> rootSchema(std::string_view kind)
{
    const auto ruleList = reference("ruleList");
    const auto cardSummary = object({
        { "type", "array" },
        { "minItems", 1 },
        { "maxItems", 2 },
        { "description", "玩家卡片完整句子；${效果/0/欄位} 或 ${管理規則/0/欄位} 引用同項目數值。" },
        { "items", object({
            { "type", "string" },
            { "pattern", "^[^\\r\\n：:]+。$" },
        }) },
    });
    if (kind == "combos")
    {
        const auto integerList = object({
            { "type", "array" },
            { "items", object({{ "type", "integer" }}) },
        });
        const auto threshold = objectSchema({
            { "卡片摘要", cardSummary },
            { "人數", object({{ "type", "integer" }}) },
            { "名稱", object({{ "type", "string" }}) },
            { "效果", ruleList },
        }, { "人數", "名稱" }, true);
        const auto combo = objectSchema({
            { "名稱", object({{ "type", "string" }}) },
            { "成員", integerList },
            { "反向羈絆", object({{ "type", "boolean" }}) },
            { "星級羈絆加成", object({{ "type", "boolean" }}) },
            { "閾值", object({
                { "type", "array" },
                { "items", threshold },
            }) },
        }, { "名稱", "成員", "閾值" }, true);
        return objectSchema({{
            "羈絆", object({
                { "type", "array" },
                { "items", combo },
            }),
        }}, { "羈絆" }, true);
    }
    if (kind == "equipment")
    {
        const auto binding = objectSchema({
            { "角色ID", object({
                { "type", "array" },
                { "items", object({{ "type", "integer" }}) },
            }) },
            { "效果", ruleList },
        }, { "角色ID" }, true);
        const auto equipment = objectSchema({
            { "裝備ID", object({{ "type", "integer" }}) },
            { "層級", object({{ "type", "integer" }}) },
            { "裝備類型", object({{ "type", "integer" }}) },
            { "效果", ruleList },
            { "裝備羈絆", object({
                { "type", "array" },
                { "items", binding },
            }) },
        }, { "裝備ID", "層級", "裝備類型" }, true);
        return objectSchema({{
            "裝備列表", object({
                { "type", "array" },
                { "items", equipment },
            }),
        }}, { "裝備列表" }, true);
    }
    if (kind == "magic_effects")
    {
        const auto magic = objectSchema({
            { "卡片摘要", cardSummary },
            { "武功", object({{ "type", "integer" }}) },
            { "名稱", object({{ "type", "string" }}) },
            { "效果", ruleList },
        }, { "武功", "名稱", "效果" });
        return objectSchema({
            { "絕招", object({
                { "type", "array" },
                { "items", magic },
            }) },
        }, { "絕招" });
    }
    if (kind == "neigong")
    {
        const auto tier = objectSchema({
            { "層級", object({{ "type", "integer" }}) },
            { "武功", object({
                { "type", "array" },
                { "items", object({{ "type", "integer" }}) },
            }) },
        }, { "層級", "武功" });
        return objectSchema({
            { "選擇數量", object({{ "type", "integer" }}) },
            { "Boss可選層級", object({{ "type", "object" }}) },
            { "層級分配", object({
                { "type", "array" },
                { "items", tier },
            }) },
            { "名稱", object({{ "type", "object" }}) },
            { "效果", object({
                { "type", "object" },
                { "additionalProperties", ruleList },
            }) },
        }, { "選擇數量", "層級分配", "效果" }, true);
    }
    return std::unexpected(std::format("未知 schema kind「{}」", kind));
}

std::expected<std::string, std::string> schemaFor(std::string_view kind)
{
    auto root = rootSchema(kind);
    auto definitions = commonDefinitions();
    if (!root) return std::unexpected(root.error());
    if (!definitions) return std::unexpected(definitions.error());

    JsonValue::Object document{
        { "$schema", "https://json-schema.org/draft/2020-12/schema" },
        { "$id", std::format(
            "https://kys-cpp.local/schemas/chess_effects/chess_{}.schema.json", kind) },
        { "title", std::format("KYS Chess {} effect authoring schema", kind) },
    };
    auto rootFields = std::move(asObject(*root));
    document.insert(
        document.end(),
        std::make_move_iterator(rootFields.begin()),
        std::make_move_iterator(rootFields.end()));
    document.emplace_back("$defs", std::move(*definitions));
    return serialize(JsonValue(std::move(document)));
}

}

std::expected<RenderedSchemaFiles, std::string> renderChessEffectSchemas()
{
    static constexpr std::array kinds{
        std::string_view{ "combos" },
        std::string_view{ "equipment" },
        std::string_view{ "magic_effects" },
        std::string_view{ "neigong" },
    };
    RenderedSchemaFiles files;
    for (std::size_t index = 0; index < kinds.size(); ++index)
    {
        auto content = schemaFor(kinds[index]);
        if (!content) return std::unexpected(content.error());
        files[index] = {
            std::format("chess_{}.schema.json", kinds[index]),
            std::move(*content),
        };
    }
    return files;
}

}
