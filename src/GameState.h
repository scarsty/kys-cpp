#pragma once
#include "ChessEconomy.h"
#include "ChessEquipmentInventory.h"
#include "Chess.h"
#include "GameDataStore.h"
#include "ChessProgress.h"
#include "ChessRandom.h"
#include "ChessRoster.h"
#include "ChessRoleSave.h"
#include "ChessShop.h"

#include <algorithm>
#include <optional>

namespace KysChess
{

// State management layer - owns lifecycle, persistence, RNG, and concrete chess subsystems.
class GameState : private GameDataStore
{
public:
    static GameState& get() {
        static GameState state;
        return state;
    }

    void reset(Difficulty difficulty);
    GameDataStore exportStore() const;
    void importStore(const GameDataStore& store);

public:
    ChessRoleSave& roleSave() { return roleSave_; }
    const ChessRoleSave& roleSave() const { return roleSave_; }
    ChessEquipmentInventory& equipmentInventory() { return *equipmentInventory_; }
    const ChessEquipmentInventory& equipmentInventory() const { return *equipmentInventory_; }
    ChessRoster& roster() { return *roster_; }
    const ChessRoster& roster() const { return *roster_; }
    ChessShop& shop() { return *shop_; }
    const ChessShop& shop() const { return *shop_; }
    ChessProgress& progress() { return *progress_; }
    const ChessProgress& progress() const { return *progress_; }
    ChessEconomy& economy() { return *economy_; }
    const ChessEconomy& economy() const { return *economy_; }
    ChessRandom& random() { return *random_; }
    const ChessRandom& random() const { return *random_; }
    Difficulty& difficulty() { return GameDataStore::difficulty; }
    const Difficulty& difficulty() const { return GameDataStore::difficulty; }
    int& banBaseCount() { return GameDataStore::banBaseCount; }
    const int& banBaseCount() const { return GameDataStore::banBaseCount; }
    int& banCountPerLevel() { return GameDataStore::banCountPerLevel; }
    const int& banCountPerLevel() const { return GameDataStore::banCountPerLevel; }
    int maxBanCount() const {
        int count = std::max(0, banBaseCount() + economy().getLevel() * banCountPerLevel());
        for (auto& unlock : ChessBalance::config().banUnlocks)
        {
            if (progress().battleProgress().getFight() >= unlock.afterFight)
                count += unlock.slots;
        }
        return count;
    }
    int maxBanTier() const {
        int tier = 5;
        auto& unlocks = ChessBalance::config().banUnlocks;
        if (!unlocks.empty())
        {
            tier = 0;
            for (auto& unlock : unlocks)
            {
                if (progress().battleProgress().getFight() >= unlock.afterFight)
                    tier = std::max(tier, unlock.maxTier);
            }
        }
        return tier;
    }
    bool hasBanSystem() const { return banBaseCount() > 0 || banCountPerLevel() > 0 || !ChessBalance::config().banUnlocks.empty(); }
    std::set<int>& seenRoleIds() { return GameDataStore::seenRoleIds; }
    const std::set<int>& seenRoleIds() const { return GameDataStore::seenRoleIds; }
    std::set<int>& bannedRoleIds() { return GameDataStore::bannedRoleIds; }
    const std::set<int>& bannedRoleIds() const { return GameDataStore::bannedRoleIds; }
    void syncBanRuleFromBalance();
    void hydrateBanRuleFromBalanceIfMissing();

private:
    GameState();

    void constructDefaultServices();
    void constructServicesFromStore(const GameDataStore& store);

    ChessRoleSave roleSave_;
    std::optional<ChessRandom> random_;
    std::optional<ChessEquipmentInventory> equipmentInventory_;
    std::optional<ChessRoster> roster_;
    std::optional<ChessShop> shop_;
    std::optional<ChessProgress> progress_;
    std::optional<ChessEconomy> economy_;
};

}
