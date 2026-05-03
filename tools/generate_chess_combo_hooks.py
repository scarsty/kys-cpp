from __future__ import annotations

import collections
import copy
from pathlib import Path

import yaml


ROOT = Path(__file__).resolve().parents[1]
SOURCE_PATH = ROOT / "config" / "chess_combos.yaml"
OUTPUT_PATH = ROOT / "config" / "chess_combos_hooks.yaml"
SCHEMA_PATH = ROOT / "config" / "chess_effect_schema.yaml"
REPORT_PATH = ROOT / "docs" / "battle_effect_trigger_migration.md"


SETUP_TYPES = {
    "生命加成", "攻击加成", "防御加成", "速度加成",
    "生命百分比", "攻击百分比", "防御百分比", "速度百分比",
    "全队生命加成", "全队攻击加成", "全队防御加成", "全队速度加成",
    "全队生命百分比", "全队攻击百分比", "全队防御百分比", "全队速度百分比",
    "防御削减", "冷却缩减", "回蓝加成", "僵直抗性", "僵直护盾",
    "护盾值", "护盾生命比", "护盾僵直抗性", "七截分身", "弹反",
    "保护挪移", "处决挪移", "闪击", "初次格挡",
    "滑步攻击", "滑步概率提升", "远程化", "单次承伤上限", "格挡绝招反击",
}
OUT_OF_BATTLE_TYPES = {
    "金币加成", "免费刷新", "每勝生命加成", "每胜生命加成",
    "每勝攻防加成", "每胜攻防加成", "计作羁绊", "选择战场",
}
SPECIAL_HANDLING_HOOKS = {"经济结算", "商店阶段", "战前选场", "羁绊解析", "战斗胜利结算"}
FRAME_TYPES = {"生命回复", "治疗光环", "固定治疗光环", "周期免伤", "周期绝招", "回血"}
ATTACK_CALC_TYPES = {
    "固定加伤", "暴击几率", "暴击伤害", "每N次双倍", "技能伤害",
    "当前内力加伤", "连击增伤", "连击蓄力",
}
DEFENSE_CALC_TYPES = {
    "固定减伤", "格挡几率", "闪避几率", "闪避后暴击", "伤害减免",
    "同敌减伤", "同敌闪避", "技能反弹", "冷却延长反击",
}
HURT_AFTER_TYPES = {"受伤无敌"}
WOULD_DIE_TYPES = {"死亡庇护"}
ON_HIT_TYPES = {
    "穿甲几率", "穿甲百分比", "穿甲", "眩晕", "击退几率", "中毒伤害",
    "中毒增伤", "命中回蓝", "命中回血", "吸取内力", "流血",
    "破罡", "封内", "攻击倾城", "攻击冷却延长", "伤害降低", "当前生命伤害",
    "全队回内", "螺旋流血弹", "范围追踪弹", "连锁弹", "群体施治百分比",
}
SKILL_FINISH_TYPES = {"技能后无敌"}
ULTIMATE_TYPES = {"绝招后退", "群体施治", "群体施治百分比", "绝招追加弹"}
KILL_TYPES = {"击杀回血", "击杀无敌", "击杀增攻", "嗜血"}
SELF_DEATH_TYPES = {"死亡医疗", "殉爆"}
ALLY_DEATH_TYPES = {"同袍之死", "护盾重获"}
SHIELD_BREAK_TYPES = {"护盾爆炸", "临时攻击加成", "自动绝招", "回内力"}
HEAL_TYPES = {"受治疗加速"}
BATTLE_START_TARGET_ENEMY_TYPES = {"敌方攻防削弱"}

DIRECT_TRIGGER_HOOKS = {
    "攻击命中时概率": "攻击命中",
    "被击中时概率": "被攻击命中",
    "护盾爆炸时": "护盾破裂",
    "绝招时": "绝招释放",
}

TARGET_BY_TYPE = {
    "穿甲": "命中目标",
    "斩杀": "命中目标",
    "封内": "命中目标",
    "破罡": "命中目标",
    "当前生命伤害": "敌方全体",
    "全队回内": "我方全体",
    "护盾值": "自身",
    "螺旋流血弹": "命中目标周围",
    "范围追踪弹": "附近敌人",
    "群体施治": "我方全体",
    "群体施治百分比": "我方全体",
    "死亡医疗": "同羁绊成员",
    "同袍之死": "自身",
    "护盾重获": "自身",
    "殉爆": "敌方范围",
    "敌方攻防削弱": "敌方优先目标",
}

LOW_CONFIDENCE_TYPES = {
    "中毒增伤": "目前是在伤害计算时检查目标中毒状态，不是单纯命中效果。",
    "受治疗加速": "目前依附治疗光环的成功治疗，缺少通用“受治疗后”事件。",
    "七截分身": "目前是战斗初始化复制单位，未定义召唤生命周期。",
    "敌方攻防削弱": "目前由场景刷新敌方优先 debuff，缺少独立 target selector。",
    "选择战场": "不是战斗中效果，应该移到战前规则或棋局规则。",
    "免费刷新": "不是战斗中效果，应该移到商店/经济规则。",
    "金币加成": "不是战斗中效果，应该移到经济结算规则。",
    "每勝生命加成": "不是单场战斗效果，应该移到持久成长结算规则。",
    "每胜生命加成": "不是单场战斗效果，应该移到持久成长结算规则。",
    "每勝攻防加成": "不是单场战斗效果，应该移到持久成长结算规则。",
    "每胜攻防加成": "不是单场战斗效果，应该移到持久成长结算规则。",
    "计作羁绊": "这是羁绊解析规则，不应该留在战斗效果执行器。",
    "冷却延长反击": "目前是防御方被命中时延长攻击者冷却，应该归入受击后反制阶段。",
}

TRIGGER_GAPS = {
    "友方低血狂暴": "这里的友方应该明确为同羁绊成员；语义是低血单位广播限时状态，需要 TeamEvent/BuffSource，而不是单个效果自己的 hook。",
    "低血量": "这是条件，不是 hook；实际挂钩要由效果类型决定，例如回血走每帧，攻击加成走攻击方伤害计算。",
    "最后存活": "这是队伍状态条件，不是 hook；需要由 BattleState 在死亡事件后维护。",
}

EXECUTOR_BY_TYPE = {
    "生命加成": "属性修正",
    "攻击加成": "属性修正",
    "防御加成": "属性修正",
    "速度加成": "属性修正",
    "生命百分比": "属性修正",
    "攻击百分比": "属性修正",
    "防御百分比": "属性修正",
    "速度百分比": "属性修正",
    "全队生命加成": "属性修正",
    "全队攻击加成": "属性修正",
    "全队防御加成": "属性修正",
    "全队速度加成": "属性修正",
    "全队生命百分比": "属性修正",
    "全队攻击百分比": "属性修正",
    "全队防御百分比": "属性修正",
    "全队速度百分比": "属性修正",
    "防御削减": "属性修正",
    "每勝生命加成": "成长属性修正",
    "每胜生命加成": "成长属性修正",
    "每勝攻防加成": "成长属性修正",
    "每胜攻防加成": "成长属性修正",
    "固定减伤": "伤害修正",
    "固定加伤": "伤害修正",
    "伤害减免": "伤害修正",
    "技能伤害": "伤害修正",
    "当前内力加伤": "伤害修正",
    "中毒增伤": "伤害修正",
    "单次承伤上限": "伤害上限",
    "命中回血": "治疗",
    "击杀回血": "治疗",
    "回血": "治疗",
    "生命回复": "治疗",
    "治疗光环": "治疗",
    "固定治疗光环": "治疗",
    "死亡医疗": "治疗",
    "群体施治": "治疗",
    "群体施治百分比": "治疗",
    "命中回蓝": "修改内力",
    "吸取内力": "修改内力",
    "回内力": "修改内力",
    "全队回内": "修改内力",
    "回蓝加成": "内力回复修正",
    "护盾值": "添加护盾",
    "护盾生命比": "添加护盾",
    "护盾重获": "添加护盾",
    "全队护盾": "添加护盾",
    "护盾爆炸": "护盾破裂效果",
    "眩晕": "添加状态",
    "封内": "添加状态",
    "破罡": "添加状态",
    "中毒伤害": "添加状态",
    "流血": "添加状态",
    "伤害降低": "添加状态",
    "临时攻击加成": "添加状态",
    "受伤无敌": "添加无敌帧",
    "技能后无敌": "添加无敌帧",
    "击杀无敌": "添加无敌帧",
    "死亡庇护": "死亡保护并添加无敌帧",
    "周期免伤": "定时添加无敌帧",
    "冷却缩减": "修改冷却",
    "冷却延长反击": "修改冷却",
    "攻击冷却延长": "修改冷却",
    "受治疗加速": "修改冷却",
    "自动绝招": "触发绝招",
    "周期绝招": "触发绝招",
    "格挡绝招反击": "触发绝招",
    "绝招追加弹": "生成投射物",
    "连锁弹": "生成投射物",
    "螺旋流血弹": "生成投射物",
    "范围追踪弹": "生成投射物",
    "当前生命伤害": "范围伤害",
    "殉爆": "范围伤害",
    "穿甲": "命中判定修正",
    "斩杀": "命中判定修正",
    "击退几率": "命中位移",
    "暴击几率": "战斗判定参数",
    "暴击伤害": "战斗判定参数",
    "格挡几率": "战斗判定参数",
    "闪避几率": "战斗判定参数",
    "闪避后暴击": "战斗判定参数",
    "每N次双倍": "计数判定",
    "初次格挡": "计数判定",
    "连击增伤": "堆叠修正",
    "同敌减伤": "堆叠修正",
    "同敌闪避": "堆叠修正",
    "击杀增攻": "战斗内属性修正",
    "同袍之死": "战斗内属性修正",
    "敌方攻防削弱": "战斗内属性修正",
    "僵直抗性": "战斗参数修正",
    "僵直护盾": "战斗参数修正",
    "护盾僵直抗性": "战斗参数修正",
    "技能反弹": "战斗参数修正",
    "弹反": "战斗参数修正",
    "滑步攻击": "战斗参数修正",
    "滑步概率提升": "战斗参数修正",
    "远程化": "战斗参数修正",
    "闪击": "战斗参数修正",
    "保护挪移": "强制位移",
    "处决挪移": "强制位移",
    "绝招后退": "强制位移",
    "金币加成": "经济修正",
    "免费刷新": "商店修正",
    "选择战场": "战前规则修正",
    "计作羁绊": "羁绊解析修正",
}

SPECIAL_SYSTEM_BY_HOOK = {
    "经济结算": "经济系统",
    "商店阶段": "商店系统",
    "战前选场": "战前规则",
    "羁绊解析": "羁绊解析",
    "战斗胜利结算": "成长结算",
}


def migration_strategy(hook: str, confidence: str) -> str:
    if hook in SPECIAL_HANDLING_HOOKS:
        return "特殊系统处理"
    if confidence != "高":
        return "专用执行器"
    return "共享执行器"


def infer_base_hook(effect_type: str) -> str:
    if effect_type in OUT_OF_BATTLE_TYPES:
        if effect_type == "金币加成":
            return "经济结算"
        if effect_type == "免费刷新":
            return "商店阶段"
        if effect_type == "选择战场":
            return "战前选场"
        if effect_type == "计作羁绊":
            return "羁绊解析"
        return "战斗胜利结算"
    if effect_type in FRAME_TYPES:
        return "每帧"
    if effect_type in ATTACK_CALC_TYPES:
        return "伤害计算前_攻击方"
    if effect_type in DEFENSE_CALC_TYPES:
        return "伤害计算前_防御方"
    if effect_type in HURT_AFTER_TYPES:
        return "受伤后"
    if effect_type in WOULD_DIE_TYPES:
        return "将死时"
    if effect_type in ON_HIT_TYPES:
        return "攻击命中"
    if effect_type in SKILL_FINISH_TYPES:
        return "技能结束后"
    if effect_type in ULTIMATE_TYPES:
        return "绝招释放"
    if effect_type in KILL_TYPES:
        return "击杀"
    if effect_type in SELF_DEATH_TYPES:
        return "自身死亡"
    if effect_type in ALLY_DEATH_TYPES:
        return "同羁绊成员死亡"
    if effect_type in SHIELD_BREAK_TYPES:
        return "护盾破裂"
    if effect_type in HEAL_TYPES:
        return "受治疗后"
    if effect_type in BATTLE_START_TARGET_ENEMY_TYPES or effect_type in SETUP_TYPES:
        return "战斗初始化"
    return "待设计"


def infer_target(effect_type: str, hook: str) -> str:
    if hook == "被攻击命中":
        return "攻击者"
    if effect_type in TARGET_BY_TYPE:
        return TARGET_BY_TYPE[effect_type]
    if effect_type.startswith("全队") or effect_type in {"治疗光环", "固定治疗光环"}:
        return "我方全体"
    if hook == "攻击命中":
        if effect_type in {"命中回血", "命中回蓝", "吸取内力", "连击增伤", "连击蓄力"}:
            return "自身"
        return "命中目标"
    if hook in {"击杀", "同羁绊成员死亡"}:
        return "自身"
    if hook == "自身死亡":
        return "依效果定义"
    return "自身"


def build_conditions(effect: dict, old_trigger: str | None) -> list[dict]:
    conditions = []
    trigger_value = effect.get("触发参数")
    if old_trigger in {"攻击命中时概率", "被击中时概率", "护盾爆炸时"} and trigger_value is not None:
        conditions.append({"类型": "概率", "数值": trigger_value, "单位": "百分比"})
    elif old_trigger == "低血量":
        conditions.append({"类型": "自身生命低于", "数值": trigger_value or 0, "单位": "百分比"})
    elif old_trigger == "最后存活":
        conditions.append({"类型": "最后存活"})
    elif old_trigger == "友方低血狂暴":
        conditions.append({"类型": "自身生命低于", "数值": trigger_value or 0, "单位": "百分比"})
        conditions.append({"类型": "套用到同羁绊成员", "排除自身": True})
    if effect.get("次数") is not None:
        conditions.append({"类型": "次数上限", "数值": effect["次数"]})
    if effect.get("持续帧数") is not None:
        conditions.append({"类型": "持续时间", "数值": effect["持续帧数"], "单位": "帧"})
    return conditions


def conditional_hook(effect_type: str) -> str:
    hook = infer_base_hook(effect_type)
    if hook == "战斗初始化":
        if effect_type in {"攻击加成", "攻击百分比"}:
            return "伤害计算前_攻击方"
        if effect_type in {"防御加成", "防御百分比", "伤害减免", "格挡几率"}:
            return "伤害计算前_防御方"
        return "每帧"
    return hook


def build_design(effect: dict) -> dict:
    effect_type = effect.get("类型", "")
    old_trigger = effect.get("触发")
    gaps = []
    confidence = "高"

    if old_trigger in DIRECT_TRIGGER_HOOKS:
        hook = DIRECT_TRIGGER_HOOKS[old_trigger]
    elif old_trigger == "低血量":
        hook = conditional_hook(effect_type)
        gaps.append(TRIGGER_GAPS[old_trigger])
        confidence = "中"
    elif old_trigger == "最后存活":
        hook = conditional_hook(effect_type)
        gaps.append(TRIGGER_GAPS[old_trigger])
        confidence = "中"
    elif old_trigger == "友方低血狂暴":
        hook = "每帧"
        gaps.append(TRIGGER_GAPS[old_trigger])
        confidence = "低"
    elif old_trigger:
        hook = "待设计"
        gaps.append(f"旧触发“{old_trigger}”没有对应的新挂钩。")
        confidence = "低"
    else:
        hook = infer_base_hook(effect_type)
        if hook == "待设计":
            gaps.append("效果类型没有明确推断规则，需要补 runtime 语义。")
            confidence = "低"

    if effect_type in LOW_CONFIDENCE_TYPES:
        gaps.append(LOW_CONFIDENCE_TYPES[effect_type])
        if confidence == "高":
            confidence = "中"

    target = "同羁绊成员" if old_trigger == "友方低血狂暴" else infer_target(effect_type, hook)
    design = {
        "挂钩": hook,
        "条件": build_conditions(effect, old_trigger),
        "作用目标": target,
        "来源触发": old_trigger or "无",
        "迁移信心": confidence,
        "迁移策略": migration_strategy(hook, confidence),
        "缺口": gaps,
    }
    if design["迁移策略"] == "专用执行器":
        design["执行器"] = "专用执行器"
        design["专用执行器"] = f"{effect_type}专用执行器"
    elif effect_type in EXECUTOR_BY_TYPE:
        design["执行器"] = EXECUTOR_BY_TYPE[effect_type]
    return design


def build_special_handling(effect: dict, design: dict) -> dict:
    hook = design["挂钩"]
    result = {
        "系统": SPECIAL_SYSTEM_BY_HOOK.get(hook, "特殊系统"),
        "时机": hook,
        "来源触发": design["来源触发"],
        "迁移信心": design["迁移信心"],
        "迁移策略": design["迁移策略"],
        "缺口": design["缺口"],
    }
    effect_type = effect.get("类型", "")
    if effect_type in EXECUTOR_BY_TYPE:
        result["执行器"] = EXECUTOR_BY_TYPE[effect_type]
    return result


def render_report(hook_counts, confidence_counts, old_trigger_counts, low_confidence_entries, unknown_types) -> str:
    lines = [
        "# 战斗效果触发 YAML 迁移",
        "",
        "生成的审查文件：`config/chess_combos_hooks.yaml`。",
        "",
        "这是第一片纯配置迁移。游戏仍然读取 `config/chess_combos.yaml`；新文件用来在改 runtime 前暴露触发设计缺口。",
        "",
        "## 建议结构",
        "",
        "每个效果保留原字段，并新增 `触发设计`：",
        "",
        "- `挂钩`: runtime 事件点。",
        "- `条件`: 概率、生命阈值、持续时间、次数上限等结构化谓词。",
        "- `作用目标`: 目标选择器，和效果执行本身分开。",
        "- `来源触发`: 原本重载的 `触发` 字段，保留给审查。",
        "- `迁移信心`: 推断映射是否足够安全。",
        "- `迁移策略`: 高信心条目走共享执行器；中/低信心的战斗条目先走专用执行器；局外条目走特殊系统处理。",
        "- `缺口`: 明确缺失的设计或 runtime 行为。",
        "",
        "非战斗效果不生成 `触发设计`，而是生成 `特殊处理`。这些条目属于经济、商店、战前、羁绊解析或成长结算系统。",
        "",
        "## 挂钩统计",
        "",
    ]
    lines.extend(f"- `{hook}`: {count}" for hook, count in sorted(hook_counts.items()))
    lines.extend(["", "## 迁移信心", ""])
    lines.extend(f"- `{conf}`: {count}" for conf, count in sorted(confidence_counts.items()))
    lines.extend(["", "## 旧触发映射", ""])
    lines.extend(f"- `{trig}`: {count}" for trig, count in sorted(old_trigger_counts.items()))
    lines.extend([
        "",
        "## 缺失设计",
        "",
        "- **同羁绊成员广播**: 旧 `友方低血狂暴` 应明确为同羁绊成员的限时状态广播，需要一等公民的 team event / buff source。",
        "- **条件不是挂钩**: `低血量` 和 `最后存活` 应该是 BattleState 谓词；效果类型决定它挂在每帧、攻击伤害计算还是防御伤害计算。",
        "- **伤害流水线阶段**: 需要攻击方伤害前、防御方伤害前、护盾吸收、伤害结算后、命中后、反射/投射物副作用等明确阶段。一个泛泛的 OnHit 不够。",
        "- **效果实例身份**: 当前次数用 `triggeredEffects` 下标计数；拆 hook 后需要稳定 effect instance id。",
        "- **目标选择器**: 敌方优先 debuff、死亡医疗、团队治疗、附近追踪弹、护盾重获等都需要复用 target selector，而不是散落在 scene loop。",
        "- **非战斗效果**: `金币加成`、`免费刷新`、`选择战场`、`计作羁绊`、胜利成长应该离开战斗效果执行器。",
        "- **共享执行器**: `受伤无敌`、`技能后无敌`、`击杀无敌` 都应该共用“添加无敌帧”执行器；治疗、护盾、内力、冷却、状态、投射物等也应共用对应执行器。",
        "- **已简化规则**: `流血持续` 不再作为效果存在；流血层数默认持续，不随 tick 消耗。",
        "- **效果参数**: `数值` 和 `附加参数` 的意义随效果变化。配置可以先迁移，但 runtime 最终应该按效果类型解析具名参数。",
        "- **表现和玩法拆分**: 投射物生成、护盾爆炸、自动绝招、滑步攻击应该产出 battle command / event；core 不应直接调渲染或音效。",
        "",
        "## 共享执行器候选",
        "",
        "- `属性修正`: 生命/攻击/防御/速度的固定值、百分比、全队版本。",
        "- `治疗`: 命中回血、击杀回血、生命回复、治疗光环、群体施治、死亡医疗。",
        "- `修改内力`: 命中回蓝、吸取内力、护盾破裂回内、全队回内。",
        "- `添加护盾`: 固定护盾、最大生命护盾、护盾重获。",
        "- `添加状态`: 眩晕、封内、中毒、流血、伤害降低、临时攻击加成。",
        "- `添加无敌帧`: 受伤无敌、技能后无敌、击杀无敌；`死亡庇护` 使用带死亡拦截的变体。",
        "- `修改冷却`: 冷却缩减、冷却延长反击、攻击冷却延长、受治疗加速。",
        "- `生成投射物`: 连锁弹、绝招追加弹、螺旋流血弹、范围追踪弹。",
        "- `触发绝招`: 自动绝招、周期绝招、格挡绝招反击。",
        "- `堆叠修正`: 连击增伤、同敌减伤、同敌闪避。",
        "",
        "## 专用执行器和特殊处理条目",
        "",
    ])
    if low_confidence_entries:
        for combo_name, threshold_name, effect_type, design in low_confidence_entries[:80]:
            gaps = "; ".join(design["缺口"]) if design["缺口"] else "mapping is inferred but should be reviewed"
            lines.append(f"- `{combo_name}` / `{threshold_name}` / `{effect_type}` -> `{design['挂钩']}` ({design['迁移信心']}, {design['迁移策略']}): {gaps}")
        if len(low_confidence_entries) > 80:
            lines.append(f"- ... 另外 {len(low_confidence_entries) - 80} 个条目省略；可在生成的 YAML 中搜索 `迁移信心` 和 `缺口`。")
    else:
        lines.append("- None.")
    lines.extend(["", "## 未识别类型", ""])
    if unknown_types:
        lines.extend(f"- `{typ}`: {count}" for typ, count in sorted(unknown_types.items()))
    else:
        lines.append("- 无；当前羁绊效果都至少有一个临时挂钩。")
    lines.extend([
        "",
        "## 下一片 Runtime",
        "",
        "不要再做一个 static 工具袋。建议拆成这些对象：",
        "",
        "- `BattleEffectDefinition`: parsed config, stable id, effect type, params, hook binding, conditions, target selector.",
        "- `BattleEffectRegistry`: owns effect type metadata and parameter schema.",
        "- `BattleEffectDispatcher`: receives battle events and asks hook groups to evaluate matching definitions.",
        "- `BattleEffectContext`: immutable event data plus controlled mutation/event-output APIs.",
        "- `BattleConditionEvaluator`: reusable predicate evaluation over unit/world/event state.",
        "- `BattleTargetSelector`: reusable target resolution for self, attacker, defender, allies, enemies, nearest, top threat, etc.",
        "",
        "第一轮 runtime 实作应该先让 parser 理解 `触发设计` 和 `chess_effect_schema.yaml`，并用测试对照旧行为。",
        "",
    ])
    return "\n".join(lines)


def render_schema(type_hooks: dict[str, set[str]], type_targets: dict[str, set[str]], type_confidence: dict[str, set[str]]) -> dict:
    hooks = [
        {"名称": "战斗初始化", "阶段": "setup", "是否战斗内": True},
        {"名称": "每帧", "阶段": "frame", "是否战斗内": True},
        {"名称": "技能结束后", "阶段": "action", "是否战斗内": True},
        {"名称": "绝招释放", "阶段": "action", "是否战斗内": True},
        {"名称": "攻击命中", "阶段": "damage", "是否战斗内": True},
        {"名称": "被攻击命中", "阶段": "damage", "是否战斗内": True},
        {"名称": "伤害计算前_攻击方", "阶段": "damage", "是否战斗内": True},
        {"名称": "伤害计算前_防御方", "阶段": "damage", "是否战斗内": True},
        {"名称": "受伤后", "阶段": "damage", "是否战斗内": True},
        {"名称": "护盾破裂", "阶段": "damage", "是否战斗内": True},
        {"名称": "将死时", "阶段": "death", "是否战斗内": True},
        {"名称": "击杀", "阶段": "death", "是否战斗内": True},
        {"名称": "自身死亡", "阶段": "death", "是否战斗内": True},
        {"名称": "同羁绊成员死亡", "阶段": "death", "是否战斗内": True},
        {"名称": "受治疗后", "阶段": "heal", "是否战斗内": True},
        {"名称": "战前选场", "阶段": "pre_battle", "是否战斗内": False},
        {"名称": "羁绊解析", "阶段": "combo_resolution", "是否战斗内": False},
        {"名称": "商店阶段", "阶段": "shop", "是否战斗内": False},
        {"名称": "经济结算", "阶段": "economy", "是否战斗内": False},
        {"名称": "战斗胜利结算", "阶段": "progression", "是否战斗内": False},
    ]
    conditions = [
        {"名称": "概率", "参数": ["数值", "单位"]},
        {"名称": "自身生命低于", "参数": ["数值", "单位"]},
        {"名称": "最后存活", "参数": []},
        {"名称": "套用到同羁绊成员", "参数": ["排除自身"]},
        {"名称": "次数上限", "参数": ["数值"]},
        {"名称": "持续时间", "参数": ["数值", "单位"]},
    ]
    target_selectors = [
        "自身", "攻击者", "命中目标", "命中目标周围", "同羁绊成员",
        "我方全体", "敌方全体", "敌方范围", "敌方优先目标", "附近敌人", "依效果定义",
    ]
    executors = [
        {
            "名称": "属性修正",
            "说明": "修改目标的基础属性或百分比属性；生命、攻击、防御、速度和全队版本都应共用。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "成长属性修正",
            "说明": "胜利或局外成长用的属性修正，不进入战斗 hook dispatcher。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "战斗内属性修正",
            "说明": "战斗过程中临时或永久修改角色属性，例如击杀增攻、同袍之死、敌方攻防削弱。",
            "参数": ["数值", "附加参数", "持续帧数"],
        },
        {
            "名称": "伤害修正",
            "说明": "在伤害流水线中加减固定伤害或乘百分比伤害。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "伤害上限",
            "说明": "按当前或最大生命限制单次承伤上限。",
            "参数": ["数值"],
        },
        {
            "名称": "治疗",
            "说明": "对目标回复生命；命中回血、击杀回血、生命回复、治疗光环、群体施治、死亡医疗共用。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "修改内力",
            "说明": "增加、消耗、吸取或给团队回复 MP。",
            "参数": ["数值"],
        },
        {
            "名称": "内力回复修正",
            "说明": "修改自然或战斗内回蓝倍率。",
            "参数": ["数值"],
        },
        {
            "名称": "添加护盾",
            "说明": "按固定值或最大生命百分比给目标添加护盾。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "护盾破裂效果",
            "说明": "护盾破裂时触发伤害、自动绝招、临时 buff 或资源回复。",
            "参数": ["数值", "附加参数", "持续帧数"],
        },
        {
            "名称": "添加状态",
            "说明": "给目标添加限时状态，例如眩晕、封内、中毒、流血、伤害降低、临时攻击加成。",
            "参数": ["数值", "附加参数", "持续帧数"],
        },
        {
            "名称": "添加无敌帧",
            "说明": "把目标 Invincible 提升或设置为指定帧数；受伤无敌、技能后无敌、击杀无敌应共用这个执行器。",
            "参数": ["数值"],
        },
        {
            "名称": "死亡保护并添加无敌帧",
            "说明": "在将死时保留 1 点生命并添加无敌帧；它复用无敌帧写入，但多了死亡拦截语义。",
            "参数": ["数值"],
        },
        {
            "名称": "定时添加无敌帧",
            "说明": "按间隔触发无敌帧，适用于周期免伤。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "修改冷却",
            "说明": "缩短或延长目标 CoolDown；冷却缩减、冷却延长反击、攻击冷却延长、受治疗加速共用。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "触发绝招",
            "说明": "在条件满足时请求角色释放绝招；自动绝招、周期绝招、格挡绝招反击共用。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "生成投射物",
            "说明": "生成额外投射物或投射物模式；连锁弹、绝招追加弹、螺旋流血弹、范围追踪弹共用。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "范围伤害",
            "说明": "对一组目标造成额外伤害，例如当前生命伤害和殉爆。",
            "参数": ["数值", "附加参数", "持续帧数"],
        },
        {
            "名称": "命中判定修正",
            "说明": "命中后改变本次攻击判定结果，例如穿甲或斩杀。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "命中位移",
            "说明": "命中后对目标施加击退或位移。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "战斗判定参数",
            "说明": "修改暴击、格挡、闪避等战斗判定参数。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "计数判定",
            "说明": "基于次数计数触发效果，例如每 N 次双倍和初次格挡。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "堆叠修正",
            "说明": "按目标或连击状态积累堆叠并参与伤害/防御/闪避计算。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "战斗参数修正",
            "说明": "设置战斗参数或能力开关，例如弹反、滑步攻击、远程化、僵直抗性。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "强制位移",
            "说明": "执行保护、处决或技能后的强制移动。",
            "参数": ["数值", "附加参数"],
        },
        {
            "名称": "专用执行器",
            "说明": "临时承接语义不确定或上下文不足的战斗效果；仍由 hook dispatcher 调度，但执行逻辑按效果类型硬编码。",
            "参数": ["数值", "附加参数", "持续帧数", "次数"],
        },
        {
            "名称": "经济修正",
            "说明": "经济系统处理，不进入战斗 hook dispatcher。",
            "参数": ["数值"],
        },
        {
            "名称": "商店修正",
            "说明": "商店系统处理，不进入战斗 hook dispatcher。",
            "参数": ["数值"],
        },
        {
            "名称": "战前规则修正",
            "说明": "战前规则处理，不进入战斗 hook dispatcher。",
            "参数": ["数值"],
        },
        {
            "名称": "羁绊解析修正",
            "说明": "羁绊解析阶段处理，不进入战斗 hook dispatcher。",
            "参数": ["名称"],
        },
    ]
    extra_allowed_hooks = {
        "眩晕": {"攻击命中", "被攻击命中"},
        "护盾值": {"战斗初始化", "攻击命中"},
        "绝招追加弹": {"绝招释放", "攻击命中"},
        "群体施治百分比": {"绝招释放", "攻击命中"},
        "技能后无敌": {"技能结束后"},
    }

    effect_types = []
    for effect_type in sorted(type_hooks):
        allowed_hooks = set(type_hooks[effect_type]) | extra_allowed_hooks.get(effect_type, set())
        confidence = sorted(type_confidence.get(effect_type, set()))
        has_special_hook = any(h in SPECIAL_HANDLING_HOOKS for h in allowed_hooks)
        needs_dedicated = any(c != "高" for c in confidence) and not has_special_hook
        has_shared = "高" in confidence and not has_special_hook
        schema_strategy = (
            "特殊系统处理" if has_special_hook
            else "混合" if needs_dedicated and has_shared
            else "专用执行器" if needs_dedicated
            else "共享执行器"
        )
        effect_schema = {
            "类型": effect_type,
            "默认挂钩": sorted(type_hooks[effect_type])[0],
            "允许挂钩": sorted(allowed_hooks),
            "默认目标": sorted(type_targets.get(effect_type, {"自身"}))[0],
            "处理器": EXECUTOR_BY_TYPE.get(effect_type, effect_type),
            "特殊系统处理": has_special_hook,
            "迁移策略": schema_strategy,
            "参数字段": ["数值", "附加参数", "持续帧数", "次数"],
            "通用能力": ["条件", "目标选择", "次数上限", "持续时间"],
            "需要复核": needs_dedicated,
        }
        if schema_strategy == "专用执行器":
            effect_schema["专用执行器"] = f"{effect_type}专用执行器"
        elif schema_strategy == "混合":
            effect_schema["不确定条目策略"] = "专用执行器"
        effect_types.append(effect_schema)

    return {
        "效果系统版本": 2,
        "说明": [
            "此文件是效果类型注册表草案，用来校验某个效果类型是否能绑定到某个挂钩。",
            "它表达可泛化的部分：挂钩调度、条件判断、目标选择、次数/持续时间、共享执行器；具体数值变更仍由效果处理器负责。",
            "当前内容从 config/chess_combos.yaml 推断，后续 runtime 应该把它变成 BattleEffectRegistry 的数据来源或测试基准。",
        ],
        "挂钩": hooks,
        "条件类型": conditions,
        "目标选择器": target_selectors,
        "共享执行器": executors,
        "效果类型": effect_types,
    }


def main() -> None:
    source = yaml.safe_load(SOURCE_PATH.read_text(encoding="utf-8"))
    migrated = {
        "效果系统版本": 2,
        "迁移来源": "config/chess_combos.yaml",
        "迁移说明": [
            "此文件是审查用的第一片迁移产物，尚未由游戏载入。",
            "原有效果字段保留，新增“触发设计”描述未来 hook/condition/effect 拆分。",
            "“迁移信心: 低/中”的战斗条目先进入专用执行器，避免把不确定语义塞进错误的共享执行器。",
        ],
        "羁绊": copy.deepcopy(source.get("羁绊", [])),
    }

    hook_counts = collections.Counter()
    confidence_counts = collections.Counter()
    old_trigger_counts = collections.Counter()
    low_confidence_entries = []
    unknown_types = collections.Counter()
    type_hooks = collections.defaultdict(set)
    type_targets = collections.defaultdict(set)
    type_confidence = collections.defaultdict(set)
    effect_count = 0

    for combo in migrated["羁绊"]:
        combo_name = combo.get("名称", "<unknown>")
        for threshold in combo.get("阈值", []) or []:
            threshold_name = threshold.get("名称", "<unknown>")
            for effect in threshold.get("效果", []) or []:
                effect_count += 1
                design = build_design(effect)
                if design["挂钩"] in SPECIAL_HANDLING_HOOKS:
                    effect["特殊处理"] = build_special_handling(effect, design)
                else:
                    effect["触发设计"] = design
                hook_counts[design["挂钩"]] += 1
                confidence_counts[design["迁移信心"]] += 1
                old_trigger_counts[design["来源触发"]] += 1
                effect_type = effect.get("类型", "<unknown>")
                type_hooks[effect_type].add(design["挂钩"])
                type_targets[effect_type].add(design["作用目标"])
                type_confidence[effect_type].add(design["迁移信心"])
                if design["挂钩"] == "待设计":
                    unknown_types[effect_type] += 1
                if design["缺口"] or design["迁移信心"] != "高":
                    low_confidence_entries.append((combo_name, threshold_name, effect_type, design))

    OUTPUT_PATH.write_text(
        yaml.safe_dump(migrated, allow_unicode=True, sort_keys=False, width=120),
        encoding="utf-8",
    )
    SCHEMA_PATH.write_text(
        yaml.safe_dump(render_schema(type_hooks, type_targets, type_confidence), allow_unicode=True, sort_keys=False, width=120),
        encoding="utf-8",
    )
    REPORT_PATH.write_text(
        render_report(hook_counts, confidence_counts, old_trigger_counts, low_confidence_entries, unknown_types),
        encoding="utf-8",
    )

    print(f"Generated {OUTPUT_PATH.relative_to(ROOT)} with {effect_count} effects")
    print(f"Generated {SCHEMA_PATH.relative_to(ROOT)}")
    print(f"Generated {REPORT_PATH.relative_to(ROOT)}")
    for hook, count in sorted(hook_counts.items()):
        print(f"{hook}: {count}")


if __name__ == "__main__":
    main()
