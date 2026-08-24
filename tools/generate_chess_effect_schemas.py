from __future__ import annotations

import argparse
import json
import re
from copy import deepcopy
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCHEMA_DIR = ROOT / "schemas" / "chess_effects"
SOURCE = ROOT / "src" / "ChessBattleEffects.cpp"
CPP_SOURCE = SOURCE.read_text(encoding="utf-8")


def cpp_initializers(source: str, type_name: str) -> list[str]:
    results: list[str] = []
    marker = re.compile(rf"\b{re.escape(type_name)}\s*\{{")
    for match in marker.finditer(source):
        index = match.end()
        depth = 1
        in_string = False
        in_raw = False
        escaped = False
        while index < len(source) and depth:
            if in_raw:
                if source.startswith(')"', index):
                    in_raw = False
                    index += 2
                    continue
            elif in_string:
                char = source[index]
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == '"':
                    in_string = False
            elif source.startswith('R"(', index):
                in_raw = True
                index += 3
                continue
            elif source[index] == '"':
                in_string = True
            elif source[index] == "{":
                depth += 1
            elif source[index] == "}":
                depth -= 1
                if depth == 0:
                    results.append(source[match.end():index])
                    break
            index += 1
        if depth:
            raise RuntimeError(f"無法解析 C++ initializer：{type_name}")
    return results


def cpp_arguments(initializer: str) -> list[str]:
    arguments: list[str] = []
    start = 0
    depth = 0
    in_string = False
    in_raw = False
    escaped = False
    index = 0
    while index < len(initializer):
        if in_raw:
            if initializer.startswith(')"', index):
                in_raw = False
                index += 2
                continue
        elif in_string:
            char = initializer[index]
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
        elif initializer.startswith('R"(', index):
            in_raw = True
            index += 3
            continue
        elif initializer[index] == '"':
            in_string = True
        elif initializer[index] in "{([":
            depth += 1
        elif initializer[index] in "})]":
            depth -= 1
        elif initializer[index] == "," and depth == 0:
            arguments.append(initializer[start:index].strip())
            start = index + 1
        index += 1
    tail = initializer[start:].strip()
    if tail:
        arguments.append(tail)
    return arguments


def cpp_string(value: str) -> str:
    value = value.strip()
    if value.startswith('R"(') and value.endswith(')"'):
        return value[3:-2]
    if value.startswith('"') and value.endswith('"'):
        return json.loads(value)
    if value == "{}":
        return ""
    raise RuntimeError(f"不是 C++ 字串 initializer：{value}")


def cpp_symbol(value: str, prefix: str = "") -> str:
    value = value.strip()
    if prefix and value.startswith(prefix):
        return value[len(prefix):]
    return value.lstrip("&")


def registry_block(marker: str) -> str:
    match = re.search(
        rf"static constexpr std::array {marker}\{{(.*?)\n\}};",
        CPP_SOURCE,
        re.DOTALL,
    )
    if not match:
        raise RuntimeError(f"找不到 C++ registry：{marker}")
    return match.group(1)


def typed_registry(marker: str, type_name: str) -> list[list[str]]:
    return [cpp_arguments(value) for value in cpp_initializers(registry_block(marker), type_name)]


def enum_metadata() -> dict[str, dict]:
    label_arrays: dict[str, list[str]] = {}
    for match in re.finditer(
        r"static constexpr std::array (\w+Labels)\{(.*?)\n\};",
        CPP_SOURCE,
        re.DOTALL,
    ):
        label_arrays[match.group(1)] = re.findall(
            r'authorLabel\(\s*"([^"]+)"', match.group(2)
        )
    descriptors: dict[str, dict] = {}
    for match in re.finditer(
        r'static constexpr AuthorEnumDescriptor (\w+)\{\s*"([^"]+)",\s*(\w+)',
        CPP_SOURCE,
    ):
        variable, name, labels = match.groups()
        descriptors[variable] = {"name": name, "labels": label_arrays[labels]}
    return descriptors


def parse_cpp_payload_metadata() -> dict[str, dict]:
    field_arrays: dict[str, list[dict]] = {}
    for match in re.finditer(
        r"static constexpr std::array (\w+)\{(.*?)\n\s*\};",
        CPP_SOURCE,
        re.DOTALL,
    ):
        fields: list[dict] = []
        for initializer in cpp_initializers(match.group(2), "PayloadFieldDescriptor"):
            args = cpp_arguments(initializer)
            if not args or not args[0].startswith('"'):
                continue
            fields.append({
                "name": cpp_string(args[0]),
                "required": args[1] == "true",
                "shape": cpp_symbol(args[2], "PayloadNodeShape::"),
                "schema_ref": cpp_symbol(args[5], "PayloadSchemaReference::")
                    if len(args) > 5 else "None",
                "enum": cpp_symbol(args[6])
                    if len(args) > 6 and args[6] != "nullptr" else None,
                "nested": cpp_symbol(args[7])
                    if len(args) > 7 and args[7] != "nullptr" else None,
            })
        if fields:
            field_arrays[match.group(1)] = fields
    field_arrays["emptyPayloadFields"] = []
    field_arrays["attributePercentageFields"] = []

    descriptors: dict[str, dict] = {}
    for match in re.finditer(r"static constexpr PayloadDescriptor (\w+)\s*\{", CPP_SOURCE):
        tail = CPP_SOURCE[match.start():]
        initializer_source = "PayloadDescriptor" + tail[tail.find("{"):]
        args = cpp_arguments(cpp_initializers(initializer_source, "PayloadDescriptor")[0])
        variable = match.group(1)
        fields = cpp_symbol(args[1])
        descriptors[variable] = {
            "name": cpp_string(args[0]),
            "fields": field_arrays.get(fields, []),
            "dynamic": cpp_symbol(args[3], "PayloadDynamicKeyClass::")
                if len(args) > 3 else "None",
            "dynamic_shape": cpp_symbol(args[4], "PayloadNodeShape::")
                if len(args) > 4 else "Any",
            "minimum_properties": int(args[7]) if len(args) > 7 else 0,
            "dynamic_alternative": cpp_string(args[8]) if len(args) > 8 else None,
        }
    return descriptors


ENUMS = enum_metadata()
PAYLOADS = parse_cpp_payload_metadata()
TIMING_ENTRIES = typed_registry("timingDescriptors", "TimingDescriptor")
ACTION_ENTRIES = typed_registry("actionDescriptors", "ActionDescriptor")
MACRO_ENTRIES = typed_registry("macroDescriptors", "MacroDescriptor")
CONDITION_DESCRIPTOR_ENTRIES = typed_registry("conditionDescriptors", "ConditionDescriptor")
TIMINGS = [cpp_string(entry[0]) for entry in TIMING_ENTRIES]
ACTIONS = [cpp_string(entry[0]) for entry in ACTION_ENTRIES]
MACROS = [cpp_string(entry[0]) for entry in MACRO_ENTRIES]
SELECTOR_NAMES = ENUMS["selectorKindEnum"]["labels"]
ATTRIBUTES = ENUMS["battleAttributeEnum"]["labels"]
ACTION_PAYLOAD_VARIABLES = {
    cpp_string(entry[0]): cpp_symbol(entry[3]) for entry in ACTION_ENTRIES
}
MACRO_PAYLOAD_VARIABLES = {
    cpp_string(entry[0]): cpp_symbol(entry[2]) for entry in MACRO_ENTRIES
}
MACRO_PAYLOAD_KINDS = {
    cpp_string(entry[0]): cpp_symbol(entry[1], "MacroPayloadKind::")
    for entry in MACRO_ENTRIES
}
CONDITION_ENTRIES = [{
    "name": cpp_string(entry[0]),
    "form": cpp_symbol(entry[2], "ConditionAuthorForm::"),
    "field": cpp_string(entry[3]) or None,
    "payload": cpp_symbol(entry[4]),
} for entry in CONDITION_DESCRIPTOR_ENTRIES]


def object_schema(properties: dict, required: list[str] | None = None, *, additional=False) -> dict:
    schema = {"type": "object", "properties": properties, "additionalProperties": additional}
    if required:
        schema["required"] = required
    return schema


def enum_schema(values: list[str]) -> dict:
    return {"type": "string", "enum": values}


NUMBER = {"$ref": "#/$defs/effectNumber"}
SELECTOR = {"$ref": "#/$defs/selector"}
ACTION_NODE = {"$ref": "#/$defs/actionNode"}
ACTION_LIST = {"type": "array", "minItems": 1, "items": ACTION_NODE}
CONDITION_LIST = {"type": "array", "items": {"$ref": "#/$defs/condition"}}
ANY_SCALAR = {"type": ["string", "integer", "number", "boolean"]}


def schema_for_shape(shape: str) -> dict:
    schemas = {
        "Any": {},
        "Scalar": deepcopy(ANY_SCALAR),
        "String": {"type": "string"},
        "Integer": {"type": "integer"},
        "Boolean": {"type": "boolean"},
        "Map": {"type": "object"},
        "Sequence": {"type": "array"},
        "Number": deepcopy(NUMBER),
        "Selector": deepcopy(SELECTOR),
        "ActionNode": deepcopy(ACTION_NODE),
        "ActionList": deepcopy(ACTION_LIST),
        "ConditionList": deepcopy(CONDITION_LIST),
        "StringOrSequence": {
            "oneOf": [
                {"type": "string"},
                {"type": "array", "items": {"type": "string"}},
            ]
        },
    }
    try:
        return schemas[shape]
    except KeyError as ex:
        raise RuntimeError(f"未支援的 C++ payload shape：{shape}") from ex


def schema_for_field(field: dict) -> dict:
    reference = field["schema_ref"]
    if reference == "EffectNumber":
        return deepcopy(NUMBER)
    if reference == "Selector":
        return deepcopy(SELECTOR)
    if reference == "ActionNode":
        return deepcopy(ACTION_NODE)
    if reference == "ActionList":
        return deepcopy(ACTION_LIST)
    if reference == "ConditionList":
        return deepcopy(CONDITION_LIST)
    if reference == "Timing":
        return enum_schema(TIMINGS)
    if reference == "Payload":
        return descriptor_object(field["nested"])
    if reference == "PayloadList":
        return {
            "type": "array",
            "minItems": 1,
            "items": descriptor_object(field["nested"]),
        }
    if reference != "None":
        raise RuntimeError(f"未支援的 C++ schema reference：{reference}")

    enum_variable = field["enum"]
    if not enum_variable:
        return schema_for_shape(field["shape"])
    values = ENUMS[enum_variable]["labels"]
    if field["shape"] == "String":
        return enum_schema(values)
    if field["shape"] == "Sequence":
        return {"type": "array", "items": enum_schema(values)}
    if field["shape"] == "StringOrSequence":
        return {
            "oneOf": [
                enum_schema(values),
                {"type": "array", "items": enum_schema(values)},
            ]
        }
    raise RuntimeError(
        f"enum field「{field['name']}」使用不支援的 shape：{field['shape']}"
    )


def descriptor_object(variable: str, overrides: dict[str, dict] | None = None) -> dict:
    descriptor = PAYLOADS[variable]
    properties = {
        field["name"]: schema_for_field(field)
        for field in descriptor["fields"]
    }
    for name, schema in (overrides or {}).items():
        if name not in properties:
            raise RuntimeError(f"{variable} schema override 指向未宣告欄位「{name}」")
        properties[name] = schema
    if descriptor["dynamic"] == "BattleAttribute":
        dynamic_schema = schema_for_shape(descriptor["dynamic_shape"])
        properties.update({name: deepcopy(dynamic_schema) for name in ATTRIBUTES})
    elif descriptor["dynamic"] not in ("None", "NamedAction"):
        raise RuntimeError(f"未支援的 dynamic key class：{descriptor['dynamic']}")
    required = [field["name"] for field in descriptor["fields"] if field["required"]]
    schema = object_schema(properties, required, additional=False)
    if descriptor["minimum_properties"]:
        schema["minProperties"] = descriptor["minimum_properties"]
    if descriptor["dynamic_alternative"]:
        schema["anyOf"] = [
            {"required": [descriptor["dynamic_alternative"]]},
            *({"required": [name]} for name in ATTRIBUTES),
        ]
    return schema


def action_payloads() -> dict[str, dict]:
    overrides = {
        "條件分支": {
            "條件": {"type": "array", "minItems": 1, "items": {"$ref": "#/$defs/condition"}},
        },
    }
    return {
        name: descriptor_object(ACTION_PAYLOAD_VARIABLES[name], overrides.get(name))
        for name in ACTIONS
    }


def macro_payloads() -> dict[str, dict]:
    schemas: dict[str, dict] = {}
    for name in MACROS:
        kind = MACRO_PAYLOAD_KINDS[name]
        variable = MACRO_PAYLOAD_VARIABLES[name]
        if kind == "Number":
            schemas[name] = deepcopy(NUMBER)
        elif kind == "Heal":
            schemas[name] = {
                "oneOf": [deepcopy(NUMBER), descriptor_object(variable)]
            }
        elif kind == "AttributeBonus":
            schemas[name] = descriptor_object(variable)
        else:
            schemas[name] = descriptor_object(variable)
    return schemas


def common_definitions() -> dict:
    selector_map = descriptor_object("selectorPayload")
    selector = {
        "oneOf": [
            enum_schema(SELECTOR_NAMES),
            selector_map,
        ]
    }
    number = {
        "oneOf": [
            {"type": "integer"},
            descriptor_object("effectNumberPayload"),
        ]
    }
    scalar_conditions = [
        entry["name"] for entry in CONDITION_ENTRIES
        if entry["form"] in ("Scalar", "ScalarOrMap")
    ]
    condition_variants: list[dict] = [enum_schema(scalar_conditions)]
    for entry in CONDITION_ENTRIES:
        if entry["form"] == "SingleParameter":
            field = PAYLOADS[entry["payload"]]["fields"][0]
            value = schema_for_field(field)
            if field["shape"] == "Sequence":
                value["minItems"] = 1
            condition_variants.append(object_schema({entry["name"]: value}, [entry["name"]]))
        elif entry["form"] in ("Map", "ScalarOrMap"):
            condition_variants.append(object_schema(
                {entry["name"]: descriptor_object(entry["payload"])},
                [entry["name"]],
            ))

    payloads = action_payloads()
    payloads.update(macro_payloads())
    action_variants = [object_schema({name: payloads[name]}, [name]) for name in ACTIONS + MACROS]

    action_properties = {name: payloads[name] for name in ACTIONS + MACROS}
    rule = descriptor_object("rulePayload")
    rule["properties"].update(action_properties)
    action_choice = [{"required": ["動作"]}] + [{"required": [name]} for name in ACTIONS + MACROS]
    rule["oneOf"] = action_choice
    rule["allOf"] = [
        {"if": {"properties": {"時機": {"const": "每幀"}}, "required": ["時機"]},
         "then": {"not": {"required": ["間隔幀數"]}}},
        {"if": {"properties": {"時機": {"const": "每隔"}}, "required": ["時機"]},
         "then": {"required": ["間隔幀數"], "properties": {"間隔幀數": {"type": "integer", "minimum": 1}}}},
    ]
    return {
        "effectNumber": number,
        "selector": selector,
        "condition": {"oneOf": condition_variants},
        "actionNode": {"oneOf": action_variants},
        "rule": rule,
        "ruleList": {"type": "array", "items": {"$ref": "#/$defs/rule"}},
    }


def schema_for(kind: str) -> dict:
    definitions = common_definitions()
    rule_list = {"$ref": "#/$defs/ruleList"}
    if kind == "combos":
        threshold = object_schema({"效果": rule_list}, additional=True)
        combo = object_schema({"阈值": {"type": "array", "items": threshold}}, additional=True)
        root = object_schema({"羁绊": {"type": "array", "items": combo}}, additional=True)
    elif kind == "equipment":
        binding = object_schema({"效果": rule_list}, additional=True)
        equipment = object_schema({
            "效果": rule_list, "装备羁绊": {"type": "array", "items": binding},
        }, additional=True)
        root = object_schema({"装备列表": {"type": "array", "items": equipment}}, additional=True)
    elif kind == "magic_effects":
        magic = object_schema({
            "武功": {"type": "integer"}, "名稱": {"type": "string"}, "效果": rule_list,
        }, ["武功", "名稱", "效果"])
        root = object_schema({
            "啟用": {"type": "boolean"}, "絕招": {"type": "array", "items": magic},
        }, ["絕招"])
    elif kind == "neigong":
        root = object_schema({
            "效果": {"type": "object", "additionalProperties": rule_list},
        }, additional=True)
    else:
        raise ValueError(kind)
    return {
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "$id": f"https://kys-cpp.local/schemas/chess_effects/chess_{kind}.schema.json",
        "title": f"KYS Chess {kind} effect authoring schema",
        **root,
        "$defs": definitions,
    }


def assert_registries_match_cpp() -> None:
    if set(ACTIONS) != set(ACTION_PAYLOAD_VARIABLES):
        raise RuntimeError("action descriptor 缺少 payload descriptor")
    if set(MACROS) != set(MACRO_PAYLOAD_KINDS) or set(MACROS) != set(MACRO_PAYLOAD_VARIABLES):
        raise RuntimeError("macro descriptor 缺少 payload metadata")
    if len(CONDITION_ENTRIES) != len(CONDITION_DESCRIPTOR_ENTRIES):
        raise RuntimeError("condition descriptor metadata 無法完整解析")
    referenced = set(ACTION_PAYLOAD_VARIABLES.values())
    referenced.update(MACRO_PAYLOAD_VARIABLES.values())
    referenced.update(entry["payload"] for entry in CONDITION_ENTRIES)
    for descriptor in PAYLOADS.values():
        referenced.update(
            field["nested"] for field in descriptor["fields"] if field["nested"]
        )
    missing = referenced - set(PAYLOADS)
    if missing:
        raise RuntimeError(f"schema generator 找不到 payload descriptor：{sorted(missing)}")
    probe_payloads = set(re.findall(
        r"PayloadProbeDescriptor\{\s*&(\w+)", registry_block("payloadProbeDescriptors")
    ))
    if probe_payloads != set(PAYLOADS):
        raise RuntimeError("payload probe registry 未完整覆蓋 schema payload descriptors")
    enum_references = {
        field["enum"]
        for descriptor in PAYLOADS.values()
        for field in descriptor["fields"]
        if field["enum"]
    }
    if enum_references - set(ENUMS):
        raise RuntimeError(f"schema generator 找不到 enum descriptor：{sorted(enum_references - set(ENUMS))}")
    enum_registry = set(re.findall(r"&(\w+)", registry_block("authorEnumDescriptors")))
    if enum_registry != set(ENUMS):
        raise RuntimeError("author enum registry 與 enum descriptors 不一致")


def generated_files() -> dict[Path, str]:
    assert_registries_match_cpp()
    return {
        SCHEMA_DIR / f"chess_{kind}.schema.json": json.dumps(schema_for(kind), ensure_ascii=False, indent=2) + "\n"
        for kind in ("combos", "equipment", "magic_effects", "neigong")
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    stale: list[str] = []
    for path, content in generated_files().items():
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != content:
                stale.append(str(path.relative_to(ROOT)))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8", newline="\n")
    if stale:
        raise SystemExit("schema drift: " + ", ".join(stale))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
