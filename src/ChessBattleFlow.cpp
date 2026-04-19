#include "ChessBattleFlow.h"

#include "BattleMap.h"
#include "BattleStatsView.h"
#include "ChessBalance.h"
#include "ChessCombo.h"
#include "ChessDetailPanels.h"
#include "ChessEquipment.h"
#include "ChessMenuHelpers.h"
#include "ChessRewardFlow.h"
#include "ChessShopFlow.h"
#include "DynamicChessMap.h"
#include "Talk.h"
#include "TextBox.h"
#include "UISave.h"

#include <algorithm>
#include <array>
#include <climits>
#include <numeric>
#include <unordered_map>
#include <unordered_set>

namespace KysChess
{

namespace
{

struct EnemySynergyTuning
{
    static constexpr int kMinCandidateAttempts = 10;
    static constexpr int kMaxCandidateAttempts = 20;
    static constexpr int kBaseCandidateAttempts = 6;
    static constexpr int kCandidateAttemptsPerSlot = 2;

    static constexpr int kGuidedAttemptChancePct = 70;
    static constexpr int kReplacementFollowThroughChancePct = 75;
    static constexpr int kMaxReplacementBudgetRoll = 3;
    static constexpr int kTopCandidatePoolCap = 4;
    static constexpr int kTopCandidatePoolDivisor = 3;
    static constexpr int kTopCandidatePoolBias = 1;

    static constexpr int kOwnedMemberScore = 4;
    static constexpr int kNearMissOneScore = 18;
    static constexpr int kNearMissTwoScore = 7;
    static constexpr int kActiveComboBaseScore = 90;
    static constexpr int kActiveThresholdCountScore = 25;
    static constexpr int kActiveThresholdTierScore = 20;
    static constexpr int kActiveMemberScore = 5;
    static constexpr int kStarSynergyBonusScore = 10;
    static constexpr int kDuplicatePenalty = 35;
};

struct EnemyLineupCandidate
{
    std::vector<int> ids;
    std::vector<int> stars;
    int score = 0;
    int tiebreak = 0;
};

std::vector<Chess> makeEnemyChessSelection(const EnemyLineupCandidate& candidate, ChessRoleSave& roleSave)
{
    std::vector<Chess> result;
    result.reserve(candidate.ids.size());
    for (size_t i = 0; i < candidate.ids.size(); ++i)
    {
        if (auto* role = roleSave.getRole(candidate.ids[i]))
        {
            result.push_back({role, i < candidate.stars.size() ? candidate.stars[i] : 1});
        }
    }
    return result;
}

int scoreEnemyLineup(const EnemyLineupCandidate& candidate, ChessRoleSave& roleSave)
{
    auto pieces = makeEnemyChessSelection(candidate, roleSave);
    auto starByRole = ChessCombo::buildStarMap(pieces);
    auto activeCombos = ChessCombo::detectCombos(pieces);
    auto& combos = ChessCombo::getAllCombos();

    int score = 0;
    for (const auto& combo : combos)
    {
        if (combo.isAntiCombo || combo.thresholds.empty())
        {
            continue;
        }

        auto owned = computeOwnership(combo, starByRole);
        if (owned.owned < 2)
        {
            continue;
        }

        score += owned.owned * EnemySynergyTuning::kOwnedMemberScore;

        int bestMissing = INT_MAX;
        for (const auto& threshold : combo.thresholds)
        {
            if (threshold.count < 2)
            {
                continue;
            }
            bestMissing = std::min(bestMissing, std::max(0, threshold.count - owned.effective));
        }

        if (bestMissing == 1)
        {
            score += EnemySynergyTuning::kNearMissOneScore;
        }
        else if (bestMissing == 2)
        {
            score += EnemySynergyTuning::kNearMissTwoScore;
        }
    }

    for (const auto& active : activeCombos)
    {
        if (active.activeThresholdIdx < 0 || active.isAntiCombo)
        {
            continue;
        }

        const auto& combo = combos[active.id];
        const auto& threshold = combo.thresholds[active.activeThresholdIdx];
        score += EnemySynergyTuning::kActiveComboBaseScore;
        score += threshold.count * EnemySynergyTuning::kActiveThresholdCountScore;
        score += active.activeThresholdIdx * EnemySynergyTuning::kActiveThresholdTierScore;
        score += active.memberCount * EnemySynergyTuning::kActiveMemberScore;
        if (combo.starSynergyBonus)
        {
            score += EnemySynergyTuning::kStarSynergyBonusScore;
        }
    }

    std::unordered_set<int> uniqueIds;
    int duplicates = 0;
    for (int id : candidate.ids)
    {
        if (!uniqueIds.insert(id).second)
        {
            duplicates++;
        }
    }
    score -= duplicates * EnemySynergyTuning::kDuplicatePenalty;

    return score;
}

int pickEnemyRoleId(const std::vector<int>& tierRoles, ChessRandom& random, const std::unordered_set<int>& usedIds)
{
    if (tierRoles.empty())
    {
        return -1;
    }

    std::vector<int> available;
    available.reserve(tierRoles.size());
    for (int roleId : tierRoles)
    {
        if (!usedIds.contains(roleId))
        {
            available.push_back(roleId);
        }
    }

    if (!available.empty())
    {
        return available[random.enemyRandInt(static_cast<int>(available.size()))];
    }
    return tierRoles[random.enemyRandInt(static_cast<int>(tierRoles.size()))];
}

EnemyLineupCandidate buildRandomEnemyLineup(
    const std::vector<BalanceConfig::EnemySlot>& slots,
    ChessPool& pool,
    ChessRandom& random)
{
    EnemyLineupCandidate candidate;
    candidate.ids.reserve(slots.size());
    candidate.stars.reserve(slots.size());

    std::unordered_set<int> usedIds;
    for (const auto& slot : slots)
    {
        auto roleId = pickEnemyRoleId(pool.getRolesOfTier(slot.tier), random, usedIds);
        if (roleId < 0)
        {
            continue;
        }

        candidate.ids.push_back(roleId);
        candidate.stars.push_back(std::min(slot.star, 3));
        usedIds.insert(roleId);
    }

    return candidate;
}

std::array<int, 6> countEnemySlotsByTier(const std::vector<BalanceConfig::EnemySlot>& slots)
{
    std::array<int, 6> counts{};
    for (const auto& slot : slots)
    {
        if (slot.tier >= 1 && slot.tier <= 5)
        {
            counts[slot.tier]++;
        }
    }
    return counts;
}

int countFeasibleComboMembersForSlots(
    const ComboDef& combo,
    const std::array<int, 6>& slotCounts,
    ChessRoleSave& roleSave)
{
    std::array<int, 6> comboTierCounts{};
    for (int roleId : combo.memberRoleIds)
    {
        auto* role = roleSave.getRole(roleId);
        if (!role || role->Cost < 1 || role->Cost > 5)
        {
            continue;
        }
        comboTierCounts[role->Cost]++;
    }

    int feasible = 0;
    for (int tier = 1; tier <= 5; ++tier)
    {
        feasible += std::min(slotCounts[tier], comboTierCounts[tier]);
    }
    return feasible;
}

std::vector<int> collectFeasibleEnemyCombos(
    const std::vector<BalanceConfig::EnemySlot>& slots,
    ChessRoleSave& roleSave)
{
    std::vector<int> result;
    auto slotCounts = countEnemySlotsByTier(slots);
    auto& combos = ChessCombo::getAllCombos();

    for (const auto& combo : combos)
    {
        if (combo.isAntiCombo || combo.thresholds.empty())
        {
            continue;
        }

        int feasibleMembers = countFeasibleComboMembersForSlots(combo, slotCounts, roleSave);
        bool thresholdReachable = false;
        for (const auto& threshold : combo.thresholds)
        {
            if (threshold.count >= 2 && threshold.count <= feasibleMembers)
            {
                thresholdReachable = true;
                break;
            }
        }

        if (thresholdReachable)
        {
            result.push_back(combo.id);
        }
    }

    return result;
}

int chooseTargetThreshold(const ComboDef& combo, int feasibleMembers, ChessRandom& random)
{
    std::vector<int> thresholds;
    for (const auto& threshold : combo.thresholds)
    {
        if (threshold.count >= 2 && threshold.count <= feasibleMembers)
        {
            thresholds.push_back(threshold.count);
        }
    }

    if (thresholds.empty())
    {
        return -1;
    }

    return thresholds[random.enemyRandInt(static_cast<int>(thresholds.size()))];
}

void nudgeEnemyLineupTowardCombo(
    EnemyLineupCandidate& candidate,
    const std::vector<BalanceConfig::EnemySlot>& slots,
    int comboId,
    ChessRoleSave& roleSave,
    ChessRandom& random)
{
    auto& combo = ChessCombo::getAllCombos()[comboId];
    int feasibleMembers = countFeasibleComboMembersForSlots(combo, countEnemySlotsByTier(slots), roleSave);
    int targetThreshold = chooseTargetThreshold(combo, feasibleMembers, random);
    if (targetThreshold < 0)
    {
        return;
    }

    std::unordered_map<int, int> presentCounts;
    for (int roleId : candidate.ids)
    {
        presentCounts[roleId]++;
    }
    std::vector<int> slotOrder(candidate.ids.size());
    std::iota(slotOrder.begin(), slotOrder.end(), 0);
    for (int i = static_cast<int>(slotOrder.size()) - 1; i > 0; --i)
    {
        std::swap(slotOrder[i], slotOrder[random.enemyRandInt(i + 1)]);
    }

    int replacementBudget = 1 + random.enemyRandInt(std::min(
        EnemySynergyTuning::kMaxReplacementBudgetRoll,
        std::max(1, targetThreshold)));
    for (int slotIndex : slotOrder)
    {
        if (replacementBudget <= 0 || slotIndex >= static_cast<int>(slots.size()))
        {
            break;
        }

        auto currentPieces = makeEnemyChessSelection(candidate, roleSave);
        auto owned = computeOwnership(combo, ChessCombo::buildStarMap(currentPieces));
        if (owned.effective >= targetThreshold)
        {
            break;
        }

        int currentRoleId = candidate.ids[slotIndex];
        if (std::find(combo.memberRoleIds.begin(), combo.memberRoleIds.end(), currentRoleId) != combo.memberRoleIds.end())
        {
            continue;
        }

        int slotTier = slots[slotIndex].tier;
        std::vector<int> replacements;
        for (int roleId : combo.memberRoleIds)
        {
            auto* role = roleSave.getRole(roleId);
            if (!role || role->Cost != slotTier || presentCounts.contains(roleId))
            {
                continue;
            }
            replacements.push_back(roleId);
        }

        if (replacements.empty())
        {
            continue;
        }

        if (random.enemyRandInt(100) >= EnemySynergyTuning::kReplacementFollowThroughChancePct)
        {
            continue;
        }

        int replacement = replacements[random.enemyRandInt(static_cast<int>(replacements.size()))];
        if (--presentCounts[currentRoleId] <= 0)
        {
            presentCounts.erase(currentRoleId);
        }
        candidate.ids[slotIndex] = replacement;
        presentCounts[replacement]++;
        replacementBudget--;
    }
}

EnemyLineupCandidate generateSynergyBiasedEnemyLineup(
    const std::vector<BalanceConfig::EnemySlot>& slots,
    ChessPool& pool,
    ChessRoleSave& roleSave,
    ChessRandom& random)
{
    auto feasibleCombos = collectFeasibleEnemyCombos(slots, roleSave);
    int attemptCount = std::clamp(
        EnemySynergyTuning::kBaseCandidateAttempts + static_cast<int>(slots.size()) * EnemySynergyTuning::kCandidateAttemptsPerSlot,
        EnemySynergyTuning::kMinCandidateAttempts,
        EnemySynergyTuning::kMaxCandidateAttempts);

    std::vector<EnemyLineupCandidate> candidates;
    candidates.reserve(attemptCount);
    for (int attempt = 0; attempt < attemptCount; ++attempt)
    {
        auto candidate = buildRandomEnemyLineup(slots, pool, random);
        if (!feasibleCombos.empty() && random.enemyRandInt(100) < EnemySynergyTuning::kGuidedAttemptChancePct)
        {
            int comboId = feasibleCombos[random.enemyRandInt(static_cast<int>(feasibleCombos.size()))];
            nudgeEnemyLineupTowardCombo(candidate, slots, comboId, roleSave, random);
        }

        candidate.score = scoreEnemyLineup(candidate, roleSave);
        candidate.tiebreak = random.enemyRandInt(INT_MAX);
        candidates.push_back(std::move(candidate));
    }

    std::sort(candidates.begin(), candidates.end(), [](const EnemyLineupCandidate& left, const EnemyLineupCandidate& right) {
        if (left.score != right.score)
        {
            return left.score > right.score;
        }
        return left.tiebreak > right.tiebreak;
    });

    int topCount = std::min(
        EnemySynergyTuning::kTopCandidatePoolCap,
        std::max(
            1,
            static_cast<int>(candidates.size()) / EnemySynergyTuning::kTopCandidatePoolDivisor + EnemySynergyTuning::kTopCandidatePoolBias));
    return candidates[random.enemyRandInt(topCount)];
}

EnemyLineupCandidate generateAntiSynergyEnemyLineup(
    const std::vector<BalanceConfig::EnemySlot>& slots,
    ChessPool& pool,
    ChessRoleSave& roleSave,
    ChessRandom& random)
{
    int attemptCount = EnemySynergyTuning::kMaxCandidateAttempts;
    std::vector<EnemyLineupCandidate> candidates;
    candidates.reserve(attemptCount);

    for (int attempt = 0; attempt < attemptCount; ++attempt)
    {
        auto candidate = buildRandomEnemyLineup(slots, pool, random);
        candidate.score = scoreEnemyLineup(candidate, roleSave);
        candidate.tiebreak = random.enemyRandInt(INT_MAX);
        candidates.push_back(std::move(candidate));
    }

    // Sort ascending: prefer lowest synergy score
    std::sort(candidates.begin(), candidates.end(), [](const EnemyLineupCandidate& left, const EnemyLineupCandidate& right) {
        if (left.score != right.score)
        {
            return left.score < right.score;
        }
        return left.tiebreak > right.tiebreak;
    });

    int topCount = std::min(
        EnemySynergyTuning::kTopCandidatePoolCap,
        std::max(1, static_cast<int>(candidates.size()) / EnemySynergyTuning::kTopCandidatePoolDivisor + EnemySynergyTuning::kTopCandidatePoolBias));
    return candidates[random.enemyRandInt(topCount)];
}

}    // namespace

ChessBattleFlow::ChessBattleFlow(const ChessSelectorServices& services, ChessRewardFlow& rewardFlow)
    : services_(services)
    , rewardFlow_(rewardFlow)
{
}

void ChessBattleFlow::selectForBattle()
{
    auto manager = makeChessManager(services_);
    auto& pieces = services_.roster.items();
    if (pieces.empty())
    {
        return;
    }
    int maxSelection = services_.economy.getMaxDeploy();

    for (;;)
    {
        std::vector<ChessMenuEntry> entries;
        int selectedCount = 0;
        for (const auto& [id, chess] : services_.roster.items())
        {
            entries.push_back({chess, chess.selectedForBattle ? "[戰]" : ""});
            if (chess.selectedForBattle) ++selectedCount;
        }
        std::sort(entries.begin(), entries.end(), [](const ChessMenuEntry& left, const ChessMenuEntry& right) {
            return std::make_pair(left.chess.selectedForBattle ? 0 : 1, left.chess.id) < std::make_pair(right.chess.selectedForBattle ? 0 : 1, right.chess.id);
        });

        auto menuData = chessPresenter().buildChessMenuData(entries);
        for (size_t i = 0; i < static_cast<size_t>(selectedCount); ++i)
        {
            menuData.colors[i] = {255, 215, 0, 255};
        }

        IndexedMenuConfig menuConfig;
        menuConfig.perPage = 12;
        menuConfig.fontSize = 32;
        auto shopMenu = makeShopIndexedMenuSetup(menuData, menuConfig);
        auto menu = makeChessMenu(
            std::format("選擇出戰棋子 {}/{} 背包{}/{}", selectedCount, maxSelection, manager.getBenchCount(), ChessBalance::config().benchSize),
            menuData,
            shopMenu.config,
            {std::make_shared<ComboInfoPanel>(manager, shopMenu.panels.combo)});
        menu->run();

        int selectedIdx = menu->getResult();
        if (selectedIdx < 0)
        {
            return;
        }

        auto chess = entries[selectedIdx].chess;
        if (chess.selectedForBattle)
        {
            chess.selectedForBattle = false;
            services_.roster.update(chess);
        }
        else if (selectedCount >= maxSelection)
        {
            showChessMessage(std::format("最多只能選擇{}個棋子", maxSelection));
        }
        else
        {
            chess.selectedForBattle = true;
            services_.roster.update(chess);
        }
    }
}

void ChessBattleFlow::enterBattle()
{
    auto manager = makeChessManager(services_);
    auto& battleProgress = services_.progress.battleProgress();
    if (battleProgress.isGameComplete())
    {
        showChessMessage("恭喜通關！");
        return;
    }

    auto selectedChess = manager.getSelectedForBattle();
    if (selectedChess.empty())
    {
        showChessMessage("請先選擇出戰棋子");
        return;
    }

    auto savedEnemyCallCount = services_.random.getEnemyCallCount();
    std::vector<int> enemyIds;
    std::vector<int> enemyStars;
    int fightNum = battleProgress.getFight();
    bool isBoss = battleProgress.isBossFight();
    auto& config = ChessBalance::config();
    int tableIdx = std::min(fightNum, static_cast<int>(config.enemyTable.size()) - 1);
    auto& slots = config.enemyTable[tableIdx];

    if (ChessBalance::getDifficulty() != Difficulty::Easy)
    {
        bool noSynergy = false;
        for (int f : config.noSynergyFights)
        {
            if (f == fightNum + 1)    // config is 1-indexed
            {
                noSynergy = true;
                break;
            }
        }

        if (noSynergy)
        {
            auto candidate = generateAntiSynergyEnemyLineup(
                slots,
                services_.shop.pool(),
                services_.roleSave,
                services_.random);
            if (candidate.ids.size() == slots.size() && candidate.stars.size() == slots.size())
            {
                enemyIds = std::move(candidate.ids);
                enemyStars = std::move(candidate.stars);
            }
        }
        else
        {
            auto candidate = generateSynergyBiasedEnemyLineup(
                slots,
                services_.shop.pool(),
                services_.roleSave,
                services_.random);
            if (candidate.ids.size() == slots.size() && candidate.stars.size() == slots.size())
            {
                enemyIds = std::move(candidate.ids);
                enemyStars = std::move(candidate.stars);
            }
        }
    }

    if (enemyIds.size() != slots.size() || enemyStars.size() != slots.size())
    {
        enemyIds.clear();
        enemyStars.clear();
        for (auto& slot : slots)
        {
            auto role = services_.shop.pool().selectEnemyFromPool(slot.tier);
            enemyIds.push_back(role->ID);
            enemyStars.push_back(std::min(slot.star, 3));
        }
    }

    DynamicBattleRoles roles;
    for (auto& chess : selectedChess)
    {
        roles.teammate_ids.push_back(chess.role->ID);
        roles.teammate_stars.push_back(chess.star);
        roles.teammate_instances.push_back(chess.id.value);
    }
    roles.enemy_ids = enemyIds;
    roles.enemy_stars = enemyStars;
    roles.enemy_weapons.resize(enemyIds.size(), -1);
    roles.enemy_armors.resize(enemyIds.size(), -1);

    int maxTier = 0;
    int equipCount = 0;
    bool equipBoth = false;
    for (auto& level : config.enemyEquipmentLevels)
    {
        if (fightNum + 1 >= level.fight)
        {
            maxTier = level.maxTier;
            equipCount = level.count;
            equipBoth = level.equipBoth;
        }
    }
    if (maxTier > 0 && equipCount > 0)
    {
        auto tierEquip = ChessEquipment::getByTier(maxTier);
        if (!tierEquip.empty())
        {
            std::vector<const EquipmentDef*> weaponEquip;
            std::vector<const EquipmentDef*> armorEquip;
            if (equipBoth)
            {
                for (auto* equip : tierEquip)
                {
                    if (equip->equipType == 0)
                        weaponEquip.push_back(equip);
                    else if (equip->equipType == 1)
                        armorEquip.push_back(equip);
                }
            }

            std::vector<int> indices(enemyStars.size());
            std::iota(indices.begin(), indices.end(), 0);
            std::sort(indices.begin(), indices.end(), [&](int a, int b) { return enemyStars[a] > enemyStars[b]; });
            int equipSlots = std::min(equipCount, static_cast<int>(indices.size()));
            for (int i = 0; i < equipSlots; ++i)
            {
                int enemyIndex = indices[i];
                if (equipBoth)
                {
                    if (!weaponEquip.empty())
                    {
                        auto* equip = weaponEquip[services_.random.enemyRandInt(static_cast<int>(weaponEquip.size()))];
                        roles.enemy_weapons[enemyIndex] = equip->itemId;
                    }
                    if (!armorEquip.empty())
                    {
                        auto* equip = armorEquip[services_.random.enemyRandInt(static_cast<int>(armorEquip.size()))];
                        roles.enemy_armors[enemyIndex] = equip->itemId;
                    }
                }
                else
                {
                    auto* equip = tierEquip[services_.random.enemyRandInt(static_cast<int>(tierEquip.size()))];
                    if (equip->equipType == 0) roles.enemy_weapons[enemyIndex] = equip->itemId;
                    else roles.enemy_armors[enemyIndex] = equip->itemId;
                }
            }
        }
    }

    int battleSeed = static_cast<int>(services_.random.enemyRandInt(INT_MAX));
    int result = runBattle(roles, selectedChess, -1, battleSeed);

    for (const auto& chess : selectedChess)
    {
        chess.role->HP = chess.role->MaxHP;
        chess.role->MP = chess.role->MaxMP;
    }

    if (result == 0)
    {
        std::vector<Chess> survivors;
        for (auto& chess : selectedChess)
        {
            if (chess.role->HP > 0)
                survivors.push_back(chess);
        }
        auto combos = ChessCombo::detectCombos(selectedChess);
        int goldBonus = ChessCombo::calculateGoldBonus(combos, survivors);

        int reward = config.rewardBase + fightNum * config.rewardGrowth / (config.totalFights - 1) + (isBoss ? config.bossRewardBonus : 0);
        int interest = std::min(services_.economy.getMoney() * config.interestPercent / 100, config.interestMax);
        services_.economy.make(reward + interest + goldBonus);
        int expGain = isBoss ? config.bossBattleExp : config.battleExp;
        int oldLevel = services_.economy.getLevel();
        services_.economy.increaseExp(expGain);
        if (isBoss)
        {
            rewardFlow_.showNeigongReward();
        }
        rewardFlow_.showEquipmentReward();
        battleProgress.advance();
        if (!services_.shop.isLocked())
        {
            services_.shop.pool().refresh(services_.economy.getLevel());
        }

        auto text = std::make_shared<TextBox>();
        std::string levelMsg = (services_.economy.getLevel() > oldLevel) ? std::format(" 升級！等級{}", services_.economy.getLevel() + 1) : "";
        std::string nextInfo = battleProgress.isGameComplete() ? " 通關！"
            : battleProgress.isBossFight() ? std::format(" 下一關：第{}關(Boss)", battleProgress.getFight() + 1)
            : std::format(" 下一關：第{}關", battleProgress.getFight() + 1);
        std::string interestMsg = interest > 0 ? std::format("(利息+${})", interest) : "";
        std::string bonusMsg = goldBonus > 0 ? std::format("(丐帮+${})", goldBonus) : "";
        text->setText(std::format("勝利！獲得${}{}{} 經驗+{}{}{}", reward, interestMsg, bonusMsg, expGain, levelMsg, nextInfo));
        text->setFontSize(32);
        text->runCentered(Engine::getInstance()->getUIHeight() / 2);

        if (!battleProgress.isGameComplete())
        {
            for (const auto& unlock : config.banUnlocks)
            {
                if (battleProgress.getFight() == unlock.afterFight)
                {
                    ChessShopFlow shopFlow(services_);
                    shopFlow.showForcedBanSelection(unlock.slots, unlock.maxTier);
                }
            }
        }

        if (battleProgress.isGameComplete())
        {
            auto outro = std::make_shared<Talk>(std::format("少俠果然不凡！珍瓏棋局已破。若有興趣，可嘗試「遠征挑戰」，那裡有更強的對手等著你。當然，你也可以嘗試其他羈絆組合，探索更多可能。"), 115);
            outro->run();
        }
        UISave::autoSave();
    }
    else
    {
        services_.random.setEnemyCallCount(savedEnemyCallCount);
        services_.random.restore();
        auto text = std::make_shared<TextBox>();
        text->setText("戰鬥失敗！請調整陣容後再試");
        text->setFontSize(32);
        text->runCentered(Engine::getInstance()->getUIHeight() / 2);
    }
}

int ChessBattleFlow::runBattle(const DynamicBattleRoles& roles, const std::vector<Chess>& allyChess, int battle_id, int seed, bool countFightsWon)
{
    battle_id = DynamicChessMap::resolveBattleId(roles, services_.random, battle_id);

    ChessManager chessManager(services_.roster, services_.equipmentInventory, services_.economy);
    std::vector<Chess> enemyChessVec;
    for (size_t i = 0; i < roles.enemy_ids.size(); ++i)
    {
        auto* role = services_.roleSave.getRole(roles.enemy_ids[i]);
        if (role)
        {
            enemyChessVec.push_back({role, i < roles.enemy_stars.size() ? roles.enemy_stars[i] : 1});
        }
    }

    auto allyCombos = ChessCombo::detectCombos(allyChess);
    auto enemyCombos = ChessCombo::detectCombos(enemyChessVec);
    {
        auto info = BattleMap::getInstance()->getBattleInfo(battle_id);
        int musicId = info ? info->Music : -1;
        auto view = std::make_shared<BattleStatsView>(services_.roleSave, chessManager);
        view->setupPreBattle(
            allyChess,
            roles.enemy_ids,
            roles.enemy_stars,
            allyCombos,
            enemyCombos,
            battle_id,
            musicId,
            roles.enemy_weapons,
            roles.enemy_armors,
            roles.teammate_weapons,
            roles.teammate_armors);
        view->run();
    }

    auto battle = DynamicChessMap::createBattle(roles, services_.random, services_.roleSave, services_.progress, chessManager, battle_id);
    battle->setCountFightsWon(countFightsWon);
    if (seed >= 0)
    {
        battle->rand_.set_seed(static_cast<unsigned int>(seed));
    }
    battle->run();

    {
        auto view = std::make_shared<BattleStatsView>(services_.roleSave, chessManager);
        view->setupPostBattle(battle->getFriendsObj(), battle->getEnemiesObj(), battle->getTracker(), battle->getResult());
        view->run();
    }

    return battle->getResult();
}

}    // namespace KysChess
