#include "ChessEffectSchemaRenderer.h"

#include "ChessBattleEffectSemantics.h"
#include "ChessGameplayEffect.h"

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

JsonValue reference(std::string_view name)
{
    return object({{"$ref", std::format("#/$defs/{}", name)}});
}

std::expected<JsonValue, std::string> commonDefinitions()
{
    JsonValue::Object definitions;
    JsonValue::Array choices;
    for (const auto& entry : gameplayEffectCatalog())
    {
        JsonValue::Object properties{{"類型", object({{"const", entry.name}})}};
        std::vector<std::string_view> required{"類型"};
        for (const auto& field : entry.parameters)
        {
            properties.emplace_back(field.name, object({
                {"type", "integer"}, {"minimum", field.minimum}, {"maximum", field.maximum},
            }));
            required.push_back(field.name);
        }
        definitions.emplace_back(entry.name, objectSchema(std::move(properties), std::move(required)));
        choices.push_back(reference(entry.name));
    }
    definitions.emplace_back("effectList", object({
        {"type", "array"}, {"items", object({{"oneOf", JsonValue(std::move(choices))}})},
    }));
    return JsonValue(std::move(definitions));
}

std::expected<JsonValue, std::string> rootSchema(std::string_view kind)
{
    const auto effectList = reference("effectList");
    if (kind == "combos")
    {
        const auto integerList = object({
            { "type", "array" },
            { "items", object({{ "type", "integer" }}) },
        });
        const auto threshold = objectSchema({
            { "人數", object({{ "type", "integer" }}) },
            { "名稱", object({{ "type", "string" }}) },
            { "效果", effectList },
            { "管理規則", object({{ "type", "array" }}) },
        }, { "人數", "名稱" });
        const auto combo = objectSchema({
            { "名稱", object({{ "type", "string" }}) },
            { "成員", integerList },
            { "反向羈絆", object({{ "type", "boolean" }}) },
            { "星級羈絆加成", object({{ "type", "boolean" }}) },
            { "閾值", object({
                { "type", "array" },
                { "items", threshold },
            }) },
        }, { "名稱", "成員", "閾值" });
        return objectSchema({{
            "羈絆", object({
                { "type", "array" },
                { "items", combo },
            }),
        }}, { "羈絆" });
    }
    if (kind == "equipment")
    {
        const auto binding = objectSchema({
            { "角色ID", object({
                { "type", "array" },
                { "items", object({{ "type", "integer" }}) },
            }) },
            { "效果", effectList },
            { "管理規則", object({{ "type", "array" }}) },
        }, { "角色ID" });
        const auto equipment = objectSchema({
            { "裝備ID", object({{ "type", "integer" }}) },
            { "層級", object({{ "type", "integer" }}) },
            { "裝備類型", object({{ "type", "integer" }}) },
            { "效果", effectList },
            { "管理規則", object({{ "type", "array" }}) },
            { "裝備羈絆", object({
                { "type", "array" },
                { "items", binding },
            }) },
        }, { "裝備ID", "層級", "裝備類型" });
        return objectSchema({{
            "裝備列表", object({
                { "type", "array" },
                { "items", equipment },
            }),
        }}, { "裝備列表" });
    }
    if (kind == "magic_effects")
    {
        const auto magic = objectSchema({
            { "武功", object({{ "type", "integer" }}) },
            { "名稱", object({{ "type", "string" }}) },
            { "效果", effectList },
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
            { "追加選項費用", object({{ "type", "integer" }}) },
            { "選擇數量", object({{ "type", "integer" }}) },
            { "Boss可選層級", object({{ "type", "object" }}) },
            { "層級分配", object({
                { "type", "array" },
                { "items", tier },
            }) },
            { "名稱", object({{ "type", "object" }}) },
            { "效果", object({
                { "type", "object" },
                { "additionalProperties", effectList },
            }) },
        }, { "選擇數量", "層級分配", "效果" });
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
