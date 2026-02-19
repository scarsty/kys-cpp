#include "ChessCombo.h"
#include "Chess.h"
#include "Save.h"
#include "GameUtil.h"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <print>

namespace KysChess
{

std::vector<ComboDef> ChessCombo::allCombos_;
std::map<int, RoleComboState> ChessCombo::activeStates_;

// Effect type name mapping (Chinese → enum)
static const std::map<std::string, EffectType> effectTypeMap = {
    {"生命加成", EffectType::FlatHP}, {"攻击加成", EffectType::FlatATK},
    {"防御加成", EffectType::FlatDEF}, {"速度加成", EffectType::FlatSPD},
    {"生命百分比", EffectType::PctHP}, {"攻击百分比", EffectType::PctATK},
    {"防御百分比", EffectType::PctDEF}, {"防御削减", EffectType::NegPctDEF},
    {"固定减伤", EffectType::FlatDmgReduction}, {"格挡几率", EffectType::BlockChance},
    {"闪避几率", EffectType::DodgeChance}, {"闪避后暴击", EffectType::DodgeThenCrit},
    {"暴击几率", EffectType::CritChance}, {"暴击倍率", EffectType::CritMultiplier},
    {"每N次双倍", EffectType::EveryNthDouble}, {"穿甲几率", EffectType::ArmorPenChance},
    {"穿甲百分比", EffectType::ArmorPenPct}, {"眩晕几率", EffectType::StunChance},
    {"击退几率", EffectType::KnockbackChance}, {"中毒伤害", EffectType::PoisonDOT},
    {"中毒增伤", EffectType::PoisonDmgAmp}, {"命中回蓝", EffectType::MPOnHit},
    {"吸取内力", EffectType::MPDrain}, {"回蓝加成", EffectType::MPRecoveryBonus},
    {"技能伤害", EffectType::SkillDmgPct}, {"技能反弹", EffectType::SkillReflectPct},
    {"冷却缩减", EffectType::CDR}, {"护盾生命比", EffectType::ShieldPctMaxHP},
    {"护盾冷却", EffectType::ShieldCDR}, {"治疗光环", EffectType::HealAuraPct},
    {"受治疗加速", EffectType::HealedATKSPDBoost}, {"生命回复", EffectType::HPRegenPct},
    {"冰冻抗性", EffectType::FreezeReductionPct}, {"免控帧数", EffectType::ControlImmunityFrames},
    {"击杀回血", EffectType::KillHealPct}, {"击杀无敌", EffectType::KillInvincFrames},
    {"技能后无敌", EffectType::PostSkillInvincFrames}, {"伤害减免", EffectType::DmgReductionPct},
};

static const std::map<std::string, Trigger> triggerMap = {
    {"低血量", Trigger::WhileLowHP},
    {"友方低血狂暴", Trigger::AllyLowHPBurst},
};

bool ChessCombo::loadFromYaml(const std::string& path)
{
    YAML::Node root;
    try { root = YAML::LoadFile(path); }
    catch (const YAML::Exception& e)
    {
        std::print("【羁绊配置】无法读取文件 {}: {}\n", path, e.what());
        return false;
    }

    if (!root["羁绊"])
    {
        std::print("【羁绊配置】文件缺少「羁绊」根节点\n");
        return false;
    }

    auto save = Save::getInstance();
    std::vector<ComboDef> combos;
    int idx = 0;

    for (const auto& node : root["羁绊"])
    {
        ComboDef def;
        if (!node["名称"]) { std::print("【羁绊配置】第{}个羁绊缺少「名称」\n", idx + 1); return false; }
        def.name = node["名称"].as<std::string>();
        def.id = static_cast<ComboId>(idx);
        def.isAntiCombo = node["反向羁绊"] && node["反向羁绊"].as<bool>();

        // Resolve member names → IDs
        if (!node["成员"]) { std::print("【羁绊配置】「{}」缺少「成员」\n", def.name); return false; }
        for (const auto& member : node["成员"])
        {
            auto name = member.as<std::string>();
            auto* role = save->getRoleByName(name);
            if (!role)
            {
                std::print("【羁绊配置】「{}」成员「{}」找不到对应角色\n", def.name, name);
                return false;
            }
            def.memberRoleIds.push_back(role->ID);
        }

        // Parse thresholds
        if (!node["阈值"]) { std::print("【羁绊配置】「{}」缺少「阈值」\n", def.name); return false; }
        for (const auto& tNode : node["阈值"])
        {
            ComboThreshold thresh;
            if (!tNode["人数"] || !tNode["名称"])
            {
                std::print("【羁绊配置】「{}」阈值缺少「人数」或「名称」\n", def.name);
                return false;
            }
            thresh.count = tNode["人数"].as<int>();
            thresh.name = tNode["名称"].as<std::string>();

            if (!tNode["效果"]) { std::print("【羁绊配置】「{}」阈值「{}」缺少「效果」\n", def.name, thresh.name); return false; }
            for (const auto& eNode : tNode["效果"])
            {
                ComboEffect eff;
                auto typeName = eNode["类型"].as<std::string>();
                auto etIt = effectTypeMap.find(typeName);
                if (etIt == effectTypeMap.end())
                {
                    std::print("【羁绊配置】「{}」效果类型「{}」无法识别\n", def.name, typeName);
                    return false;
                }
                eff.type = etIt->second;
                eff.value = eNode["数值"].as<int>();
                if (eNode["附加参数"]) eff.value2 = eNode["附加参数"].as<int>();
                if (eNode["触发"])
                {
                    auto trigName = eNode["触发"].as<std::string>();
                    auto trIt = triggerMap.find(trigName);
                    if (trIt == triggerMap.end())
                    {
                        std::print("【羁绊配置】「{}」触发类型「{}」无法识别\n", def.name, trigName);
                        return false;
                    }
                    eff.trigger = trIt->second;
                    if (eNode["触发参数"]) eff.trigger_value = eNode["触发参数"].as<int>();
                    if (eNode["持续帧数"]) eff.value2 = eNode["持续帧数"].as<int>();
                }
                thresh.effects.push_back(eff);
            }
            def.thresholds.push_back(thresh);
        }
        combos.push_back(std::move(def));
        idx++;
    }

    allCombos_ = std::move(combos);
    std::print("【羁绊配置】成功加载{}个羁绊\n", allCombos_.size());
    return true;
}

const std::vector<ComboDef>& ChessCombo::getAllCombos()
{
    if (allCombos_.empty())
    {
        if (!loadFromYaml(GameUtil::PATH() + "config/chess_combos.yaml"))
            std::print("【羁绊配置】加载失败，羁绊系统将不可用\n");
    }
    return allCombos_;
}

std::vector<ActiveCombo> ChessCombo::detectCombos(const std::vector<Chess>& selected)
{
    std::set<int> ids;
    for (auto& c : selected)
        if (c.role) ids.insert(c.role->ID);

    std::vector<ActiveCombo> result;
    for (auto& combo : allCombos_)
    {
        ActiveCombo ac;
        ac.id = combo.id;
        for (int rid : combo.memberRoleIds)
            if (ids.count(rid)) { ac.memberCount++; ac.memberRoleIds.insert(rid); }
        if (ac.memberCount == 0) continue;

        if (combo.isAntiCombo)
        {
            // 独行: fewer = stronger. [0]=1member, [1]=2, [2]=3+
            ac.activeThresholdIdx = (ac.memberCount == 1) ? 0 : (ac.memberCount == 2) ? 1 : 2;
        }
        else
        {
            for (int i = (int)combo.thresholds.size() - 1; i >= 0; --i)
                if (ac.memberCount >= combo.thresholds[i].count) { ac.activeThresholdIdx = i; break; }
        }
        result.push_back(ac);
    }
    return result;
}

std::map<int, RoleComboState> ChessCombo::buildComboStates(const std::vector<ActiveCombo>& active)
{
    std::map<int, RoleComboState> states;
    for (auto& ac : active)
    {
        if (ac.activeThresholdIdx < 0) continue;
        auto& combo = allCombos_[(int)ac.id];
        auto& thresh = combo.thresholds[ac.activeThresholdIdx];
        for (int rid : ac.memberRoleIds)
        {
            auto& s = states[rid];
            for (auto& e : thresh.effects)
            {
                // Route triggered effects to dedicated RoleComboState fields
                if (e.trigger == Trigger::WhileLowHP)
                {
                    s.lowHPThresholdPct = std::max(s.lowHPThresholdPct, e.trigger_value);
                    if (e.type == EffectType::PctATK) s.lowHPAtkPct += e.value;
                    else if (e.type == EffectType::FlatSPD) s.lowHPSpdFlat += e.value;
                    continue;
                }
                if (e.trigger == Trigger::AllyLowHPBurst)
                {
                    s.lowHPThresholdPct = std::max(s.lowHPThresholdPct, e.trigger_value);
                    s.berserkATKPct += e.value;
                    if (e.value2) s.berserkDuration = std::max(s.berserkDuration, e.value2);
                    continue;
                }

                switch (e.type)
                {
                case EffectType::FlatHP:  s.flatHP += e.value; break;
                case EffectType::FlatATK: s.flatATK += e.value; break;
                case EffectType::FlatDEF: s.flatDEF += e.value; break;
                case EffectType::FlatSPD: s.flatSPD += e.value; break;
                case EffectType::PctHP:   s.pctHP += e.value; break;
                case EffectType::PctATK:  s.pctATK += e.value; break;
                case EffectType::PctDEF:  s.pctDEF += e.value; break;
                case EffectType::NegPctDEF: s.pctDEF -= e.value; break;
                case EffectType::FlatDmgReduction: s.flatDmgReduction += e.value; break;
                case EffectType::BlockChance: s.blockChancePct += e.value; break;
                case EffectType::DodgeChance: s.dodgeChancePct += e.value; break;
                case EffectType::DodgeThenCrit: s.dodgeThenCrit = true; break;
                case EffectType::CritChance: s.critChancePct += e.value; break;
                case EffectType::CritMultiplier: s.critMultiplier = std::max(s.critMultiplier, e.value); break;
                case EffectType::EveryNthDouble: s.everyNthDouble = e.value; break;
                case EffectType::ArmorPenChance: s.armorPenChancePct += e.value; break;
                case EffectType::ArmorPenPct: s.armorPenPct = std::max(s.armorPenPct, e.value); break;
                case EffectType::StunChance: s.stunChancePct += e.value; if (e.value2) s.stunFrames = std::max(s.stunFrames, e.value2); break;
                case EffectType::KnockbackChance: s.knockbackChancePct += e.value; break;
                case EffectType::PoisonDOT: s.poisonDOTPct += e.value; if (e.value2) s.poisonDuration = std::max(s.poisonDuration, e.value2); break;
                case EffectType::PoisonDmgAmp: s.poisonDmgAmpPct += e.value; break;
                case EffectType::MPOnHit: s.mpOnHit += e.value; break;
                case EffectType::MPDrain: s.mpDrain += e.value; break;
                case EffectType::MPRecoveryBonus: s.mpRecoveryBonusPct += e.value; break;
                case EffectType::SkillDmgPct: s.skillDmgPct += e.value; break;
                case EffectType::SkillReflectPct: s.skillReflectPct = std::max(s.skillReflectPct, e.value); break;
                case EffectType::CDR: s.cdrPct += e.value; break;
                case EffectType::ShieldPctMaxHP: s.shieldPctMaxHP += e.value; break;
                case EffectType::ShieldCDR: s.shieldCDRPct += e.value; break;
                case EffectType::HealAuraPct: s.healAuraPct = std::max(s.healAuraPct, e.value); if (e.value2) s.healAuraInterval = e.value2; break;
                case EffectType::HealedATKSPDBoost: s.healedATKSPDBoostPct += e.value; break;
                case EffectType::HPRegenPct: s.hpRegenPct += e.value; if (e.value2) s.hpRegenInterval = e.value2; break;
                case EffectType::FreezeReductionPct: s.freezeReductionPct += e.value; break;
                case EffectType::ControlImmunityFrames: s.controlImmunityFrames = e.value; break;
                case EffectType::KillHealPct: s.killHealPct += e.value; break;
                case EffectType::KillInvincFrames: s.killInvincFrames = std::max(s.killInvincFrames, e.value); break;
                case EffectType::PostSkillInvincFrames: s.postSkillInvincFrames = std::max(s.postSkillInvincFrames, e.value); break;
                case EffectType::DmgReductionPct: s.dmgReductionPct += e.value; break;
                }
            }
        }
    }
    return states;
}

void ChessCombo::applyStatBuffs(const std::map<int, RoleComboState>& states)
{
    activeStates_ = states;
    auto save = Save::getInstance();
    for (auto& [roleId, s] : activeStates_)
    {
        Role* role = save->getRole(roleId);
        if (!role) continue;

        // Flat first
        role->MaxHP += s.flatHP;
        role->Attack += s.flatATK;
        role->Defence += s.flatDEF;
        role->Speed += s.flatSPD;

        // Then percent (multiplies star-boosted + flat base)
        if (s.pctHP != 0) role->MaxHP = static_cast<int>(role->MaxHP * (1.0 + s.pctHP / 100.0));
        if (s.pctATK != 0) role->Attack = static_cast<int>(role->Attack * (1.0 + s.pctATK / 100.0));
        if (s.pctDEF != 0) role->Defence = static_cast<int>(role->Defence * (1.0 + s.pctDEF / 100.0));

        role->HP = role->MaxHP;

        // Initialize shield
        if (s.shieldPctMaxHP > 0)
            activeStates_[roleId].shield = role->MaxHP * s.shieldPctMaxHP / 100;
    }
}

const std::map<int, RoleComboState>& ChessCombo::getActiveStates() { return activeStates_; }
std::map<int, RoleComboState>& ChessCombo::getMutableStates() { return activeStates_; }
void ChessCombo::clearActiveStates() { activeStates_.clear(); }

std::vector<ComboId> ChessCombo::getCombosForRole(int roleId)
{
    std::vector<ComboId> result;
    for (auto& combo : getAllCombos())
        for (int rid : combo.memberRoleIds)
            if (rid == roleId) { result.push_back(combo.id); break; }
    return result;
}

}  // namespace KysChess
