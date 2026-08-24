#include "ChessEffectSchemaRenderer.h"

#include "ChessEffectAuthoringDescriptors.h"

#include <glaze/json.hpp>

#include <algorithm>
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

JsonValue effectNumberReference() { return reference("effectNumber"); }
JsonValue selectorReference() { return reference("selector"); }
JsonValue actionNodeReference() { return reference("actionNode"); }
JsonValue conditionReference() { return reference("condition"); }

JsonValue actionListSchema()
{
    return object({
        { "type", "array" },
        { "minItems", 1 },
        { "items", actionNodeReference() },
    });
}

JsonValue conditionListSchema()
{
    return object({
        { "type", "array" },
        { "items", conditionReference() },
    });
}

std::expected<JsonValue, std::string> schemaForShape(PayloadNodeShape shape)
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
    case PayloadNodeShape::Number: return effectNumberReference();
    case PayloadNodeShape::Selector: return selectorReference();
    case PayloadNodeShape::ActionNode: return actionNodeReference();
    case PayloadNodeShape::ActionList: return actionListSchema();
    case PayloadNodeShape::ConditionList: return conditionListSchema();
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

std::expected<JsonValue, std::string> descriptorObject(const PayloadDescriptor& descriptor);

std::expected<JsonValue, std::string> schemaForField(const PayloadFieldDescriptor& field)
{
    switch (field.schemaReference)
    {
    case PayloadSchemaReference::EffectNumber: return effectNumberReference();
    case PayloadSchemaReference::Selector: return selectorReference();
    case PayloadSchemaReference::ActionNode: return actionNodeReference();
    case PayloadSchemaReference::ActionList: return actionListSchema();
    case PayloadSchemaReference::ConditionList: return conditionListSchema();
    case PayloadSchemaReference::Timing:
    {
        JsonValue::Array labels;
        for (const auto& timing : timingDescriptors()) labels.emplace_back(timing.name);
        return object({
            { "type", "string" },
            { "enum", JsonValue(std::move(labels)) },
        });
    }
    case PayloadSchemaReference::Payload:
        if (!field.nestedPayload) return std::unexpected("nested payload metadata 遺失");
        return descriptorObject(*field.nestedPayload);
    case PayloadSchemaReference::PayloadList:
    {
        if (!field.nestedPayload) return std::unexpected("nested payload list metadata 遺失");
        auto item = descriptorObject(*field.nestedPayload);
        if (!item) return item;
        return object({
            { "type", "array" },
            { "minItems", 1 },
            { "items", std::move(*item) },
        });
    }
    case PayloadSchemaReference::None: break;
    }

    if (!field.enumLabels) return schemaForShape(field.shape);
    if (field.shape == PayloadNodeShape::String)
        return enumSchema(field.enumLabels->labels);
    if (field.shape == PayloadNodeShape::Sequence)
    {
        return object({
            { "type", "array" },
            { "items", enumSchema(field.enumLabels->labels) },
        });
    }
    if (field.shape == PayloadNodeShape::StringOrSequence)
    {
        return object({{ "oneOf", array({
            enumSchema(field.enumLabels->labels),
            object({
                { "type", "array" },
                { "items", enumSchema(field.enumLabels->labels) },
            }),
        }) }});
    }
    return std::unexpected(std::format(
        "enum field「{}」使用不支援的 shape", field.name));
}

std::expected<JsonValue, std::string> descriptorObject(const PayloadDescriptor& descriptor)
{
    JsonValue::Object properties;
    properties.reserve(descriptor.fields.size());
    std::vector<std::string_view> required;
    for (const auto& field : descriptor.fields)
    {
        auto schema = schemaForField(field);
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
            auto schema = schemaForShape(descriptor.dynamicValueShape);
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

struct NamedSchema
{
    std::string_view name;
    JsonValue schema;
};

std::expected<std::vector<NamedSchema>, std::string> actionPayloads()
{
    std::vector<NamedSchema> result;
    result.reserve(actionDescriptors().size());
    for (const auto& descriptor : actionDescriptors())
    {
        auto schema = descriptorObject(*descriptor.payload);
        if (!schema) return std::unexpected(schema.error());
        if (descriptor.payloadKind == ActionPayloadKind::Conditional)
        {
            auto* properties = findProperty(*schema, "properties");
            assert(properties);
            auto condition = object({
                { "type", "array" },
                { "minItems", 1 },
                { "items", conditionReference() },
            });
            if (!replaceProperty(*properties, "條件", std::move(condition)))
                return std::unexpected("條件分支 descriptor 缺少條件欄位");
        }
        result.push_back({ descriptor.name, std::move(*schema) });
    }
    return result;
}

std::expected<std::vector<NamedSchema>, std::string> macroPayloads()
{
    std::vector<NamedSchema> result;
    result.reserve(macroDescriptors().size());
    for (const auto& descriptor : macroDescriptors())
    {
        JsonValue schema;
        if (descriptor.payloadKind == MacroPayloadKind::Number)
        {
            schema = effectNumberReference();
        }
        else
        {
            auto payload = descriptorObject(*descriptor.payload);
            if (!payload) return std::unexpected(payload.error());
            schema = descriptor.payloadKind == MacroPayloadKind::Heal
                ? object({{ "oneOf", array({ effectNumberReference(), std::move(*payload) }) }})
                : std::move(*payload);
        }
        result.push_back({ descriptor.name, std::move(schema) });
    }
    return result;
}

std::expected<JsonValue, std::string> commonDefinitions()
{
    auto selectorMap = descriptorObject(selectorDescriptor());
    auto numberMap = descriptorObject(effectNumberDescriptor());
    if (!selectorMap) return std::unexpected(selectorMap.error());
    if (!numberMap) return std::unexpected(numberMap.error());
    auto selector = object({{ "oneOf", array({
        enumSchema(selectorKindDescriptor().labels),
        std::move(*selectorMap),
    }) }});
    auto number = object({{ "oneOf", array({
        object({{ "type", "integer" }}),
        std::move(*numberMap),
    }) }});

    JsonValue::Array conditionVariants;
    std::vector<std::string_view> scalarConditions;
    for (const auto& descriptor : conditionDescriptors())
    {
        if (descriptor.form == ConditionAuthorForm::Scalar
            || descriptor.form == ConditionAuthorForm::ScalarOrMap)
            scalarConditions.push_back(descriptor.name);
    }
    conditionVariants.push_back(enumSchema(std::span(scalarConditions)));
    for (const auto& descriptor : conditionDescriptors())
    {
        if (descriptor.form == ConditionAuthorForm::SingleParameter)
        {
            assert(descriptor.payload->fields.size() == 1);
            auto value = schemaForField(descriptor.payload->fields.front());
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
            auto payload = descriptorObject(*descriptor.payload);
            if (!payload) return std::unexpected(payload.error());
            conditionVariants.push_back(objectSchema(
                {{ std::string(descriptor.name), std::move(*payload) }},
                { descriptor.name }));
        }
    }

    auto actions = actionPayloads();
    auto macros = macroPayloads();
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
        actionVariants.push_back(objectSchema(
            {{ std::string(named.name), named.schema }},
            { named.name }));
    }

    auto rule = descriptorObject(ruleDescriptor());
    if (!rule) return std::unexpected(rule.error());
    auto* ruleProperties = findProperty(*rule, "properties");
    assert(ruleProperties);
    for (const auto& named : namedSchemas)
        appendProperty(*ruleProperties, std::string(named.name), named.schema);

    JsonValue::Array actionChoice;
    actionChoice.push_back(object({{ "required", array({ "動作" }) }}));
    for (const auto& named : namedSchemas)
        actionChoice.push_back(object({{ "required", array({ named.name }) }}));
    appendProperty(*rule, "oneOf", JsonValue(std::move(actionChoice)));
    appendProperty(*rule, "allOf", array({
        object({
            { "if", object({
                { "properties", object({{ "時機", object({{ "const", "每幀" }}) }}) },
                { "required", array({ "時機" }) },
            }) },
            { "then", object({{ "not", object({{ "required", array({ "間隔幀數" }) }}) }}) },
        }),
        object({
            { "if", object({
                { "properties", object({{ "時機", object({{ "const", "每隔" }}) }}) },
                { "required", array({ "時機" }) },
            }) },
            { "then", object({
                { "required", array({ "間隔幀數" }) },
                { "properties", object({{ "間隔幀數", object({
                    { "type", "integer" },
                    { "minimum", 1 },
                }) }}) },
            }) },
        }),
    }));

    return object({
        { "effectNumber", std::move(number) },
        { "selector", std::move(selector) },
        { "condition", object({{ "oneOf", JsonValue(std::move(conditionVariants)) }}) },
        { "actionNode", object({{ "oneOf", JsonValue(std::move(actionVariants)) }}) },
        { "rule", std::move(*rule) },
        { "ruleList", object({
            { "type", "array" },
            { "items", reference("rule") },
        }) },
    });
}

std::expected<JsonValue, std::string> rootSchema(std::string_view kind)
{
    const auto ruleList = reference("ruleList");
    if (kind == "combos")
    {
        const auto threshold = objectSchema({{ "效果", ruleList }}, {}, true);
        const auto combo = objectSchema({{
            "阈值", object({
                { "type", "array" },
                { "items", threshold },
            }),
        }}, {}, true);
        return objectSchema({{
            "羁绊", object({
                { "type", "array" },
                { "items", combo },
            }),
        }}, {}, true);
    }
    if (kind == "equipment")
    {
        const auto binding = objectSchema({{ "效果", ruleList }}, {}, true);
        const auto equipment = objectSchema({
            { "效果", ruleList },
            { "装备羁绊", object({
                { "type", "array" },
                { "items", binding },
            }) },
        }, {}, true);
        return objectSchema({{
            "装备列表", object({
                { "type", "array" },
                { "items", equipment },
            }),
        }}, {}, true);
    }
    if (kind == "magic_effects")
    {
        const auto magic = objectSchema({
            { "武功", object({{ "type", "integer" }}) },
            { "名稱", object({{ "type", "string" }}) },
            { "效果", ruleList },
        }, { "武功", "名稱", "效果" });
        return objectSchema({
            { "啟用", object({{ "type", "boolean" }}) },
            { "絕招", object({
                { "type", "array" },
                { "items", magic },
            }) },
        }, { "絕招" });
    }
    if (kind == "neigong")
    {
        return objectSchema({{
            "效果", object({
                { "type", "object" },
                { "additionalProperties", ruleList },
            }),
        }}, {}, true);
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
