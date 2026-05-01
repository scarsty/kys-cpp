#include "ChessCombo.h"
#include "Chess.h"
#include "ChessPool.h"
#include "Font.h"
#include "Save.h"
#include "GameUtil.h"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <print>

namespace KysChess
{

namespace
{

bool isTeamwideComboEffect(EffectType effectType)
{
    switch (effectType)
    {
    case EffectType::TeamFlatHP:
    case EffectType::TeamFlatATK:
    case EffectType::TeamFlatDEF:
    case EffectType::TeamFlatSPD:
    case EffectType::TeamPctHP:
    case EffectType::TeamPctATK:
    case EffectType::TeamPctDEF:
    case EffectType::TeamPctSPD:
        return true;
    default:
        return false;
    }
}

}  // namespace

std::vector<ComboDef> loadFromYaml(const std::string& path)
{
    YAML::Node root;
    try { root = YAML::LoadFile(path); }
    catch (const YAML::Exception& e)
    {
        std::print("【羁绊配置】无法读取文件 {}: {}\n", path, e.what());
        return {};
    }

    if (!root["羁绊"])
    {
        std::print("【羁绊配置】文件缺少「羁绊」根节点\n");
        return {};
    }

    std::vector<ComboDef> combos;
    int idx = 0;

    for (const auto& node : root["羁绊"])
    {
        ComboDef def;
        if (!node["名称"]) { std::print("【羁绊配置】第{}个羁绊缺少「名称」\n", idx + 1); return {}; }
        def.name = Font::getInstance()->S2T(node["名称"].as<std::string>());
        def.id = idx;
        def.isAntiCombo = node["反向羁绊"] && node["反向羁绊"].as<bool>();
        def.starSynergyBonus = node["星级羁绊加成"] && node["星级羁绊加成"].as<bool>();

        if (!node["成员"]) { std::print("【羁绊配置】「{}」缺少「成员」\n", def.name); return {}; }
        for (const auto& member : node["成员"])
            def.memberRoleIds.push_back(member.as<int>());

        if (!node["阈值"]) { std::print("【羁绊配置】「{}」缺少「阈值」\n", def.name); return {}; }
        for (const auto& tNode : node["阈值"])
        {
            ComboThreshold thresh;
            if (!tNode["人数"] || !tNode["名称"])
            {
                std::print("【羁绊配置】「{}」阈值缺少「人数」或「名称」\n", def.name);
                return {};
            }
            thresh.count = tNode["人数"].as<int>();
            thresh.name = Font::getInstance()->S2T(tNode["名称"].as<std::string>());

            if (!tNode["效果"]) { std::print("【羁绊配置】「{}」阈值「{}」缺少「效果」\n", def.name, thresh.name); return {}; }
            for (const auto& eNode : tNode["效果"])
            {
                ComboEffect eff;
                auto effectContext = std::format("羁绊「{}」阈值「{}」效果#{}", def.name, thresh.name, thresh.effects.size() + 1);
                if (!ChessBattleEffects::parseEffect(eNode, eff, effectContext))
                    return {};
                thresh.effects.push_back(eff);
            }
            def.thresholds.push_back(thresh);
        }
        combos.push_back(std::move(def));
        idx++;
    }

    std::print("【羁绊配置】成功加载{}个羁绊\n", combos.size());
    return combos;
}

std::map<int, int> ChessCombo::buildStarMap(const std::vector<Chess>& selected)
{
    std::map<int, int> result;
    for (auto& ch : selected)
        if (ch.role) result[ch.role->ID] = ch.star;
    return result;
}

const std::vector<ComboDef>& ChessCombo::getAllCombos()
{
    static std::vector<ComboDef> allCombos{loadFromYaml(GameUtil::PATH() + "config/chess_combos.yaml")};
    return allCombos;
}

std::vector<ActiveCombo> ChessCombo::detectCombos(const std::vector<Chess>& selected)
{
    std::map<int, int> starByRole;
    std::map<std::string, std::set<int>> actAsByComboName;
    for (auto& c : selected)
    {
        if (!c.role) continue;
        starByRole[c.role->ID] = c.star;
        for (const auto& comboName : c.actAsComboNames)
        {
            actAsByComboName[comboName].insert(c.role->ID);
        }
    }

    std::vector<ActiveCombo> result;
    for (auto& combo : getAllCombos())
    {
        ActiveCombo ac;
        ac.id = combo.id;
        auto addMember = [&](int rid)
        {
            if (!starByRole.count(rid)) return;
            if (ac.memberRoleIds.insert(rid).second)
            {
                ac.memberCount++;
            }
        };
        for (int rid : combo.memberRoleIds)
            addMember(rid);
        if (auto it = actAsByComboName.find(combo.name); it != actAsByComboName.end())
        {
            for (int rid : it->second)
                addMember(rid);
        }
        if (ac.memberCount == 0) continue;

        ac.physicalMemberCount = ac.memberCount;
        if (combo.starSynergyBonus)
            for (int rid : ac.memberRoleIds)
                ac.memberCount += starByRole[rid] - 1;

        if (combo.isAntiCombo)
        {
            int bestId = -1, bestTier = -1;
            for (int rid : ac.memberRoleIds)
            {
                auto* role = Save::getInstance()->getRole(rid);
                int t = role ? role->Cost : -1;
                if (t > bestTier) { bestTier = t; bestId = rid; }
            }
            ac.memberRoleIds.clear();
            ac.memberRoleIds.insert(bestId);
            ac.memberCount = 1;
            ac.physicalMemberCount = 1;
            ac.isAntiCombo = true;
            ac.activeThresholdIdx = 0;
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
        auto& combo = getAllCombos()[(int)ac.id];
        auto& thresh = combo.thresholds[ac.activeThresholdIdx];
        for (int rid : ac.memberRoleIds)
            for (auto& e : thresh.effects)
            {
                if (isTeamwideComboEffect(e.type)) continue;
                ChessBattleEffects::applyEffect(states[rid], e, ac.id);
            }
    }
    return states;
}

std::vector<ComboEffect> ChessCombo::collectGlobalEffects(const std::vector<ActiveCombo>& active)
{
    std::vector<ComboEffect> result;
    for (auto& ac : active)
    {
        if (ac.activeThresholdIdx < 0) continue;
        auto& combo = getAllCombos()[(int)ac.id];
        auto& thresh = combo.thresholds[ac.activeThresholdIdx];
        for (auto& effect : thresh.effects)
        {
            if (isTeamwideComboEffect(effect.type))
            {
                result.push_back(effect);
            }
        }
    }
    return result;
}

int ChessCombo::computeTeamFlatShieldBonus(const std::map<int, RoleComboState>& states)
{
    int totalShield = 0;
    std::set<int> seenComboIds;
    for (const auto& [roleId, state] : states)
    {
        for (const auto& effect : state.appliedEffects)
        {
            if (effect.type != EffectType::FlatShield || effect.trigger != Trigger::Always || effect.value <= 0)
            {
                continue;
            }

            if (effect.sourceComboId >= 0)
            {
                if (!seenComboIds.insert(effect.sourceComboId).second)
                {
                    continue;
                }
            }

            totalShield += effect.value;
        }
    }
    return totalShield;
}

void ChessCombo::applyStatBuffs(const std::map<int, RoleComboState>& states)
{
    activeStates_ = states;
    auto save = Save::getInstance();
    int teamFlatShield = computeTeamFlatShieldBonus(states);
    for (auto& [roleId, s] : activeStates_)
    {
        Role* role = save->getRole(roleId);
        if (!role) continue;

        role->MaxHP += s.flatHP;
        role->Attack += s.flatATK;
        role->Defence += s.flatDEF;
        role->Speed += s.flatSPD;

        if (s.pctHP != 0) role->MaxHP = static_cast<int>(role->MaxHP * (1.0 + s.pctHP / 100.0));
        if (s.pctATK != 0) role->Attack = static_cast<int>(role->Attack * (1.0 + s.pctATK / 100.0));
        if (s.pctDEF != 0) role->Defence = static_cast<int>(role->Defence * (1.0 + s.pctDEF / 100.0));
        if (s.pctSPD != 0) role->Speed = static_cast<int>(role->Speed * (1.0 + s.pctSPD / 100.0));

        role->HP = role->MaxHP;

        if (s.shieldPctMaxHP > 0)
            activeStates_[roleId].shield += role->MaxHP * s.shieldPctMaxHP / 100;
        if (teamFlatShield > 0)
            activeStates_[roleId].shield += teamFlatShield;
    }
}

const std::map<int, RoleComboState>& ChessCombo::getActiveStates() { return activeStates_; }
std::map<int, RoleComboState>& ChessCombo::getMutableStates() { return activeStates_; }
void ChessCombo::clearActiveStates() { activeStates_.clear(); }

std::vector<int> ChessCombo::getCombosForRole(int roleId)
{
    std::vector<int> result;
    for (auto& combo : getAllCombos())
        for (int rid : combo.memberRoleIds)
            if (rid == roleId) { result.push_back(combo.id); break; }
    return result;
}

void ChessCombo::transferAntiCombo(int deadRoleId, const std::vector<Role*>& allRoles)
{
    auto combos = getCombosForRole(deadRoleId);
    for (auto cid : combos)
    {
        auto& combo = getAllCombos()[(int)cid];
        if (!combo.isAntiCombo) continue;

        int deadTeam = -1;
        for (auto r : allRoles)
            if (r->ID == deadRoleId) { deadTeam = r->Team; break; }
        if (deadTeam == -1) continue;

        int bestId = -1, bestTier = -1;
        for (int mid : combo.memberRoleIds)
        {
            for (auto r : allRoles)
            {
                if (r->ID == mid && r->Team == deadTeam && r->Dead == 0)
                {
                    int tier = r->Cost;
                    if (tier > bestTier) { bestTier = tier; bestId = mid; }
                    break;
                }
            }
        }

        if (bestId != -1 && activeStates_.count(bestId) == 0)
        {
            auto& thresh = combo.thresholds[0];
            for (auto& e : thresh.effects)
                ChessBattleEffects::applyEffect(activeStates_[bestId], e, cid);
        }
    }
}

int ChessCombo::calculateGoldBonus(const std::vector<ActiveCombo>& active, const std::vector<Chess>& survivors)
{
    auto& combos = getAllCombos();
    std::set<int> survivorIds;
    int maxStar = 0;
    for (auto& chess : survivors)
    {
        survivorIds.insert(chess.role->ID);
        maxStar = std::max(maxStar, chess.star);
    }

    for (auto& ac : active)
    {
        if (ac.activeThresholdIdx < 0) continue;
        auto& combo = combos[static_cast<int>(ac.id)];
        auto& thresh = combo.thresholds[ac.activeThresholdIdx];

        for (auto& effect : thresh.effects)
        {
            if (effect.type == EffectType::GoldCoefficient)
            {
                for (int roleId : ac.memberRoleIds)
                {
                    if (survivorIds.count(roleId))
                        return maxStar * effect.value;
                }
            }
        }
    }
    return 0;
}

bool ChessCombo::hasActiveEffect(const std::vector<ActiveCombo>& active, EffectType effectType)
{
    auto& combos = getAllCombos();
    for (const auto& ac : active)
    {
        if (ac.activeThresholdIdx < 0)
        {
            continue;
        }
        auto& threshold = combos[static_cast<int>(ac.id)].thresholds[ac.activeThresholdIdx];
        for (const auto& effect : threshold.effects)
        {
            if (effect.type == effectType)
            {
                return true;
            }
        }
    }
    return false;
}

}  // namespace KysChess
