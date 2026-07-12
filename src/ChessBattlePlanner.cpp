#include "ChessBattlePlanner.h"

#include "BattleSetupFactory.h"
#include "ChessBattleMapCatalog.h"
#include "ChessCombo.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <format>
#include <numeric>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace KysChess
{
namespace
{

struct EnemySynergyTuning
{
    static constexpr int MinimumCandidateAttempts = 10;
    static constexpr int MaximumCandidateAttempts = 20;
    static constexpr int BaseCandidateAttempts = 6;
    static constexpr int CandidateAttemptsPerSlot = 2;

    static constexpr int GuidedAttemptChancePercent = 70;
    static constexpr int ReplacementFollowThroughChancePercent = 75;
    static constexpr int MaximumReplacementBudgetRoll = 3;
    static constexpr int TopCandidatePoolCap = 4;
    static constexpr int TopCandidatePoolDivisor = 3;
    static constexpr int TopCandidatePoolBias = 1;

    static constexpr int OwnedMemberScore = 4;
    static constexpr int NearMissOneScore = 18;
    static constexpr int NearMissTwoScore = 7;
    static constexpr int ActiveComboBaseScore = 90;
    static constexpr int ActiveThresholdCountScore = 25;
    static constexpr int ActiveThresholdTierScore = 20;
    static constexpr int ActiveMemberScore = 5;
    static constexpr int StarSynergyBonusScore = 10;
    static constexpr int DuplicatePenalty = 35;
};

struct EnemyLineupCandidate
{
    std::vector<int> roleIds;
    std::vector<int> stars;
    int score{};
    int randomTiebreak{};
    int sourceOrder{};
};

std::vector<const ChessRoleDefinition*> tierCandidates(
    const ChessGameContent& content,
    int tier)
{
    std::vector<const ChessRoleDefinition*> result;
    for (const int roleId : content.poolRoleIds())
    {
        const auto* role = content.role(roleId);
        if (role && role->Cost == tier)
        {
            result.push_back(role);
        }
    }
    std::ranges::sort(result, {}, [](const auto* role) { return role->ID; });
    return result;
}

std::vector<int> tierCandidateIds(const ChessGameContent& content, int tier)
{
    std::vector<int> result;
    for (const auto* role : tierCandidates(content, tier))
    {
        result.push_back(role->ID);
    }
    return result;
}

std::vector<BalanceConfig::EnemySlot> campaignSlots(
    const ChessSessionState& state,
    const ChessGameContent& content)
{
    const auto& table = content.balance().enemyTable;
    if (!table.empty())
    {
        return table[std::min(state.fight, static_cast<int>(table.size()) - 1)];
    }
    const int deployed = static_cast<int>(std::ranges::count_if(state.roster, [](const auto& entry) {
        return entry.second.deployed;
    }));
    return std::vector<BalanceConfig::EnemySlot>(std::max(content.balance().minBattleSize, deployed));
}

int pickEnemyRoleId(
    const std::vector<int>& tierRoleIds,
    ChessRunRandom& random,
    const std::unordered_set<int>& usedRoleIds)
{
    assert(!tierRoleIds.empty());
    std::vector<int> available;
    available.reserve(tierRoleIds.size());
    for (const int roleId : tierRoleIds)
    {
        if (!usedRoleIds.contains(roleId))
        {
            available.push_back(roleId);
        }
    }
    const auto& candidates = available.empty() ? tierRoleIds : available;
    return candidates[random.nextInt(ChessRngStream::EnemyLineup, static_cast<int>(candidates.size()))];
}

EnemyLineupCandidate buildRandomEnemyLineup(
    const std::vector<BalanceConfig::EnemySlot>& slots,
    const ChessGameContent& content,
    ChessRunRandom& random)
{
    EnemyLineupCandidate candidate;
    candidate.roleIds.reserve(slots.size());
    candidate.stars.reserve(slots.size());
    std::unordered_set<int> usedRoleIds;
    for (const auto& slot : slots)
    {
        const auto roleIds = tierCandidateIds(content, slot.tier);
        assert(!roleIds.empty());
        const int roleId = pickEnemyRoleId(roleIds, random, usedRoleIds);
        candidate.roleIds.push_back(roleId);
        candidate.stars.push_back(std::min(slot.star, 3));
        usedRoleIds.insert(roleId);
    }
    return candidate;
}

EnemyLineupCandidate buildIndependentEnemyLineup(
    const std::vector<BalanceConfig::EnemySlot>& slots,
    const ChessGameContent& content,
    ChessRunRandom& random)
{
    EnemyLineupCandidate candidate;
    candidate.roleIds.reserve(slots.size());
    candidate.stars.reserve(slots.size());
    for (const auto& slot : slots)
    {
        const auto roles = tierCandidates(content, slot.tier);
        assert(!roles.empty());
        candidate.roleIds.push_back(roles[random.nextInt(
            ChessRngStream::EnemyLineup,
            static_cast<int>(roles.size()))]->ID);
        candidate.stars.push_back(std::min(slot.star, 3));
    }
    return candidate;
}

ChessSessionState lineupState(const EnemyLineupCandidate& candidate)
{
    ChessSessionState state;
    for (int index = 0; index < static_cast<int>(candidate.roleIds.size()); ++index)
    {
        ChessSessionPiece piece;
        piece.instanceId = index + 1;
        piece.roleId = candidate.roleIds[index];
        piece.star = candidate.stars[index];
        piece.deployed = true;
        state.roster.emplace(piece.instanceId, piece);
    }
    return state;
}

int scoreEnemyLineup(const EnemyLineupCandidate& candidate, const ChessGameContent& content)
{
    const auto state = lineupState(candidate);
    int score = 0;
    for (const auto& combo : content.combos())
    {
        if (combo.isAntiCombo || combo.thresholds.empty())
        {
            continue;
        }
        const auto progress = evaluateChessComboProgress(state, content, combo);
        if (progress.physicalCount >= 2)
        {
            score += progress.physicalCount * EnemySynergyTuning::OwnedMemberScore;
            int bestMissing = INT_MAX;
            for (const auto& threshold : combo.thresholds)
            {
                if (threshold.count >= 2)
                {
                    bestMissing = std::min(
                        bestMissing,
                        std::max(0, threshold.count - progress.effectiveCount));
                }
            }
            if (bestMissing == 1)
            {
                score += EnemySynergyTuning::NearMissOneScore;
            }
            else if (bestMissing == 2)
            {
                score += EnemySynergyTuning::NearMissTwoScore;
            }
        }

        if (progress.activeThresholdIndex >= 0)
        {
            const auto& threshold = combo.thresholds[progress.activeThresholdIndex];
            score += EnemySynergyTuning::ActiveComboBaseScore;
            score += threshold.count * EnemySynergyTuning::ActiveThresholdCountScore;
            score += progress.activeThresholdIndex * EnemySynergyTuning::ActiveThresholdTierScore;
            score += progress.effectiveCount * EnemySynergyTuning::ActiveMemberScore;
            if (combo.starSynergyBonus)
            {
                score += EnemySynergyTuning::StarSynergyBonusScore;
            }
        }
    }

    std::unordered_set<int> uniqueRoleIds;
    int duplicates = 0;
    for (const int roleId : candidate.roleIds)
    {
        if (!uniqueRoleIds.insert(roleId).second)
        {
            ++duplicates;
        }
    }
    return score - duplicates * EnemySynergyTuning::DuplicatePenalty;
}

std::array<int, 6> countEnemySlotsByTier(const std::vector<BalanceConfig::EnemySlot>& slots)
{
    std::array<int, 6> counts{};
    for (const auto& slot : slots)
    {
        assert(slot.tier >= 1 && slot.tier <= 5);
        ++counts[slot.tier];
    }
    return counts;
}

int countFeasibleComboMembersForSlots(
    const ComboDef& combo,
    const std::array<int, 6>& slotCounts,
    const ChessGameContent& content)
{
    std::array<int, 6> comboTierCounts{};
    for (const int roleId : combo.memberRoleIds)
    {
        const auto* role = content.role(roleId);
        if (role && role->Cost >= 1 && role->Cost <= 5)
        {
            ++comboTierCounts[role->Cost];
        }
    }
    int feasible = 0;
    for (int tier = 1; tier <= 5; ++tier)
    {
        feasible += std::min(slotCounts[tier], comboTierCounts[tier]);
    }
    return feasible;
}

std::vector<const ComboDef*> collectFeasibleEnemyCombos(
    const std::vector<BalanceConfig::EnemySlot>& slots,
    const ChessGameContent& content)
{
    std::vector<const ComboDef*> result;
    const auto slotCounts = countEnemySlotsByTier(slots);
    for (const auto& combo : content.combos())
    {
        if (combo.isAntiCombo || combo.thresholds.empty())
        {
            continue;
        }
        const int feasibleMembers = countFeasibleComboMembersForSlots(combo, slotCounts, content);
        if (std::ranges::any_of(combo.thresholds, [&](const auto& threshold) {
                return threshold.count >= 2 && threshold.count <= feasibleMembers;
            }))
        {
            result.push_back(&combo);
        }
    }
    return result;
}

int chooseTargetThreshold(
    const ComboDef& combo,
    int feasibleMembers,
    ChessRunRandom& random)
{
    std::vector<int> thresholds;
    for (const auto& threshold : combo.thresholds)
    {
        if (threshold.count >= 2 && threshold.count <= feasibleMembers)
        {
            thresholds.push_back(threshold.count);
        }
    }
    assert(!thresholds.empty());
    return thresholds[random.nextInt(ChessRngStream::EnemyLineup, static_cast<int>(thresholds.size()))];
}

void nudgeEnemyLineupTowardCombo(
    EnemyLineupCandidate& candidate,
    const std::vector<BalanceConfig::EnemySlot>& slots,
    const ComboDef& combo,
    const ChessGameContent& content,
    ChessRunRandom& random)
{
    const int feasibleMembers = countFeasibleComboMembersForSlots(
        combo,
        countEnemySlotsByTier(slots),
        content);
    const int targetThreshold = chooseTargetThreshold(combo, feasibleMembers, random);

    std::unordered_map<int, int> presentCounts;
    for (const int roleId : candidate.roleIds)
    {
        ++presentCounts[roleId];
    }
    std::vector<int> slotOrder(candidate.roleIds.size());
    std::iota(slotOrder.begin(), slotOrder.end(), 0);
    for (int index = static_cast<int>(slotOrder.size()) - 1; index > 0; --index)
    {
        std::swap(slotOrder[index], slotOrder[random.nextInt(ChessRngStream::EnemyLineup, index + 1)]);
    }

    int replacementBudget = 1 + random.nextInt(
        ChessRngStream::EnemyLineup,
        std::min(EnemySynergyTuning::MaximumReplacementBudgetRoll, std::max(1, targetThreshold)));
    for (const int slotIndex : slotOrder)
    {
        if (replacementBudget <= 0)
        {
            break;
        }
        const auto progress = evaluateChessComboProgress(lineupState(candidate), content, combo);
        if (progress.effectiveCount >= targetThreshold)
        {
            break;
        }

        const int currentRoleId = candidate.roleIds[slotIndex];
        if (std::ranges::contains(combo.memberRoleIds, currentRoleId))
        {
            continue;
        }
        std::vector<int> replacements;
        for (const int roleId : combo.memberRoleIds)
        {
            const auto* role = content.role(roleId);
            if (role
                && role->Cost == slots[slotIndex].tier
                && !presentCounts.contains(roleId))
            {
                replacements.push_back(roleId);
            }
        }
        if (replacements.empty()
            || random.nextInt(ChessRngStream::EnemyLineup, 100)
                >= EnemySynergyTuning::ReplacementFollowThroughChancePercent)
        {
            continue;
        }

        const int replacement = replacements[random.nextInt(
            ChessRngStream::EnemyLineup,
            static_cast<int>(replacements.size()))];
        if (--presentCounts[currentRoleId] == 0)
        {
            presentCounts.erase(currentRoleId);
        }
        candidate.roleIds[slotIndex] = replacement;
        ++presentCounts[replacement];
        --replacementBudget;
    }
}

bool synergyCandidateBefore(const EnemyLineupCandidate& lhs, const EnemyLineupCandidate& rhs)
{
    if (lhs.score != rhs.score) return lhs.score > rhs.score;
    if (lhs.randomTiebreak != rhs.randomTiebreak) return lhs.randomTiebreak > rhs.randomTiebreak;
    if (lhs.roleIds != rhs.roleIds) return lhs.roleIds < rhs.roleIds;
    if (lhs.stars != rhs.stars) return lhs.stars < rhs.stars;
    return lhs.sourceOrder < rhs.sourceOrder;
}

bool antiSynergyCandidateBefore(const EnemyLineupCandidate& lhs, const EnemyLineupCandidate& rhs)
{
    if (lhs.score != rhs.score) return lhs.score < rhs.score;
    if (lhs.randomTiebreak != rhs.randomTiebreak) return lhs.randomTiebreak > rhs.randomTiebreak;
    if (lhs.roleIds != rhs.roleIds) return lhs.roleIds < rhs.roleIds;
    if (lhs.stars != rhs.stars) return lhs.stars < rhs.stars;
    return lhs.sourceOrder < rhs.sourceOrder;
}

template <class Comparator>
EnemyLineupCandidate selectTopEnemyCandidate(
    std::vector<EnemyLineupCandidate> candidates,
    ChessRunRandom& random,
    Comparator comparator)
{
    std::ranges::sort(candidates, comparator);
    const int topCount = std::min(
        EnemySynergyTuning::TopCandidatePoolCap,
        std::max(
            1,
            static_cast<int>(candidates.size()) / EnemySynergyTuning::TopCandidatePoolDivisor
                + EnemySynergyTuning::TopCandidatePoolBias));
    return candidates[random.nextInt(ChessRngStream::EnemyLineup, topCount)];
}

EnemyLineupCandidate generateSynergyBiasedEnemyLineup(
    const std::vector<BalanceConfig::EnemySlot>& slots,
    const ChessGameContent& content,
    ChessRunRandom& random)
{
    const auto feasibleCombos = collectFeasibleEnemyCombos(slots, content);
    const int attemptCount = std::clamp(
        EnemySynergyTuning::BaseCandidateAttempts
            + static_cast<int>(slots.size()) * EnemySynergyTuning::CandidateAttemptsPerSlot,
        EnemySynergyTuning::MinimumCandidateAttempts,
        EnemySynergyTuning::MaximumCandidateAttempts);
    std::vector<EnemyLineupCandidate> candidates;
    candidates.reserve(attemptCount);
    for (int attempt = 0; attempt < attemptCount; ++attempt)
    {
        auto candidate = buildRandomEnemyLineup(slots, content, random);
        if (!feasibleCombos.empty()
            && random.nextInt(ChessRngStream::EnemyLineup, 100)
                < EnemySynergyTuning::GuidedAttemptChancePercent)
        {
            const auto* combo = feasibleCombos[random.nextInt(
                ChessRngStream::EnemyLineup,
                static_cast<int>(feasibleCombos.size()))];
            nudgeEnemyLineupTowardCombo(candidate, slots, *combo, content, random);
        }
        candidate.score = scoreEnemyLineup(candidate, content);
        candidate.randomTiebreak = random.nextInt(ChessRngStream::EnemyLineup, INT_MAX);
        candidate.sourceOrder = attempt;
        candidates.push_back(std::move(candidate));
    }
    return selectTopEnemyCandidate(std::move(candidates), random, synergyCandidateBefore);
}

EnemyLineupCandidate generateAntiSynergyEnemyLineup(
    const std::vector<BalanceConfig::EnemySlot>& slots,
    const ChessGameContent& content,
    ChessRunRandom& random)
{
    std::vector<EnemyLineupCandidate> candidates;
    candidates.reserve(EnemySynergyTuning::MaximumCandidateAttempts);
    for (int attempt = 0; attempt < EnemySynergyTuning::MaximumCandidateAttempts; ++attempt)
    {
        auto candidate = buildRandomEnemyLineup(slots, content, random);
        candidate.score = scoreEnemyLineup(candidate, content);
        candidate.randomTiebreak = random.nextInt(ChessRngStream::EnemyLineup, INT_MAX);
        candidate.sourceOrder = attempt;
        candidates.push_back(std::move(candidate));
    }
    return selectTopEnemyCandidate(std::move(candidates), random, antiSynergyCandidateBefore);
}

EnemyLineupCandidate campaignEnemyLineup(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const std::vector<BalanceConfig::EnemySlot>& slots,
    ChessRunRandom& random)
{
    if (content.difficulty() == Difficulty::Easy)
    {
        return buildIndependentEnemyLineup(slots, content, random);
    }
    const bool noSynergy = std::ranges::contains(
        content.balance().noSynergyFights,
        state.fight + 1);
    return noSynergy
        ? generateAntiSynergyEnemyLineup(slots, content, random)
        : generateSynergyBiasedEnemyLineup(slots, content, random);
}

void appendAllies(
    PreparedChessBattle& battle,
    const ChessSessionState& state)
{
    int unitId = 1;
    for (const auto& [instanceId, piece] : state.roster)
    {
        if (!piece.deployed)
        {
            continue;
        }
        const auto equipmentItem = [&](int instanceId) {
            const auto found = state.equipmentInventory.find(instanceId);
            return found == state.equipmentInventory.end() ? -1 : found->second.itemId;
        };
        battle.units.push_back({
            unitId++,
            instanceId,
            piece.roleId,
            0,
            piece.star,
            equipmentItem(piece.weaponInstanceId),
            equipmentItem(piece.armorInstanceId),
            piece.fightsWon,
        });
    }
}

void assignEnemyEquipment(
    PreparedChessBattle& battle,
    const ChessSessionState& state,
    const ChessGameContent& content,
    ChessRunRandom& random)
{
    int maxTier = 0;
    int count = 0;
    bool both = false;
    for (const auto& level : content.balance().enemyEquipmentLevels)
    {
        if (state.fight + 1 >= level.fight)
        {
            maxTier = level.maxTier;
            count = level.count;
            both = level.equipBoth;
        }
    }
    if (count == 0)
    {
        return;
    }

    std::vector<const EquipmentDef*> eligible;
    std::vector<const EquipmentDef*> weapons;
    std::vector<const EquipmentDef*> armor;
    for (const auto& equipment : content.equipment())
    {
        if (equipment.tier > maxTier)
        {
            continue;
        }
        eligible.push_back(&equipment);
        if (equipment.equipType == 0)
        {
            weapons.push_back(&equipment);
        }
        else if (equipment.equipType == 1)
        {
            armor.push_back(&equipment);
        }
    }
    const auto orderEquipment = [](auto& values) {
        std::ranges::sort(values, {}, [](const auto* equipment) { return equipment->itemId; });
    };
    orderEquipment(eligible);
    orderEquipment(weapons);
    orderEquipment(armor);

    std::vector<PreparedChessBattleUnit*> enemies;
    for (auto& unit : battle.units)
    {
        if (unit.team == 1)
        {
            enemies.push_back(&unit);
        }
    }
    std::ranges::sort(enemies, [](const auto* lhs, const auto* rhs) {
        return std::tuple{-lhs->star, lhs->unitId}
            < std::tuple{-rhs->star, rhs->unitId};
    });
    enemies.resize(std::min(count, static_cast<int>(enemies.size())));
    for (auto* enemy : enemies)
    {
        if (both)
        {
            if (!weapons.empty())
            {
                enemy->weaponItemId = weapons[random.nextInt(
                    ChessRngStream::EnemyEquipment,
                    static_cast<int>(weapons.size()))]->itemId;
            }
            if (!armor.empty())
            {
                enemy->armorItemId = armor[random.nextInt(
                    ChessRngStream::EnemyEquipment,
                    static_cast<int>(armor.size()))]->itemId;
            }
        }
        else if (!eligible.empty())
        {
            const auto* equipment = eligible[random.nextInt(
                ChessRngStream::EnemyEquipment,
                static_cast<int>(eligible.size()))];
            (equipment->equipType == 0 ? enemy->weaponItemId : enemy->armorItemId) = equipment->itemId;
        }
    }
}

void finishPreparation(
    PreparedChessBattle& battle,
    const ChessSessionState& state,
    const ChessGameContent& content,
    ChessRunRandom& random)
{
    const int allyCount = static_cast<int>(std::ranges::count_if(battle.units, [](const auto& unit) { return unit.team == 0; }));
    const int enemyCount = static_cast<int>(battle.units.size()) - allyCount;
    battle.mapCandidates = ChessBattleMapCatalog::fittingMapIds(content, allyCount, enemyCount);
    const bool canChooseMap = chessRosterHasActiveComboEffect(
        state,
        content,
        EffectType::BattleMapChoice);
    if (!battle.mapCandidates.empty()
        && (!canChooseMap || battle.mapCandidates.size() == 1))
    {
        battle.chosenMapId = battle.mapCandidates[random.nextInt(
            ChessRngStream::MapSelection,
            static_cast<int>(battle.mapCandidates.size()))];
    }
    if (battle.chosenMapId >= 0)
    {
        BattleSetupFactory::populateBaseFormation(battle, content);
    }
    battle.battleSeed = static_cast<std::uint32_t>(
        random.nextBounded(ChessRngStream::BattleSeed, static_cast<std::uint64_t>(UINT_MAX) + 1));
}

}

PreparedChessBattle ChessBattlePlanner::prepareCampaign(
    const ChessSessionState& state,
    const ChessGameContent& content,
    ChessRunRandom& random)
{
    PreparedChessBattle battle;
    battle.kind = PreparedChessBattleKind::Campaign;
    battle.stableBattleId = std::format("campaign:{}", state.fight + 1);
    battle.preparationCheckpoint = random.checkpointPreparation();
    appendAllies(battle, state);
    int unitId = static_cast<int>(battle.units.size()) + 1;
    const auto slots = campaignSlots(state, content);
    const auto lineup = campaignEnemyLineup(state, content, slots, random);
    assert(lineup.roleIds.size() == slots.size());
    assert(lineup.stars.size() == slots.size());
    for (int index = 0; index < static_cast<int>(slots.size()); ++index)
    {
        battle.units.push_back({unitId++, -1, lineup.roleIds[index], 1, lineup.stars[index]});
    }
    assignEnemyEquipment(battle, state, content, random);
    finishPreparation(battle, state, content, random);
    return battle;
}

PreparedChessBattle ChessBattlePlanner::prepareChallenge(
    const ChessSessionState& state,
    const ChessGameContent& content,
    ChessRunRandom& random,
    const BalanceConfig::ChallengeDef& challenge)
{
    PreparedChessBattle battle;
    battle.kind = PreparedChessBattleKind::Challenge;
    battle.stableBattleId = challenge.name;
    battle.preparationCheckpoint = random.checkpointPreparation();
    appendAllies(battle, state);
    int unitId = static_cast<int>(battle.units.size()) + 1;
    for (const auto& enemy : challenge.enemies)
    {
        battle.units.push_back({
            unitId++,
            -1,
            enemy.roleId,
            1,
            enemy.star,
            enemy.weaponId,
            enemy.armorId,
        });
    }
    finishPreparation(battle, state, content, random);
    return battle;
}

}
