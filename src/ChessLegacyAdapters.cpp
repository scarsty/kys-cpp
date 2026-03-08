#include "ChessLegacyAdapters.h"

#include "Audio.h"
#include "ChessManager.h"
#include "Engine.h"
#include "Save.h"
#include "TextBox.h"

namespace KysChess
{

namespace
{

class LegacyChessGameStateAdapter : public IChessGameState
{
public:
    int getMoney() const override { return GameState::get().getMoney(); }
    void make(int amount) override { GameState::get().make(amount); }
    bool spend(int amount) override { return GameState::get().spend(amount); }

    int getExp() const override { return GameState::get().getExp(); }
    int getExpForNextLevel() const override { return GameState::get().getExpForNextLevel(); }
    int getLevel() const override { return GameState::get().getLevel(); }
    void increaseExp(int amount) override { GameState::get().increaseExp(amount); }
    int getMaxDeploy() const override { return GameState::get().getMaxDeploy(); }

    bool wouldMerge(Role* role, int star) const override { return GameState::get().wouldMerge(role, star); }
    bool canMerge(Role* role, int star) const override { return GameState::get().canMerge(role, star); }
    const std::map<ChessInstanceID, Chess>& getCollection() const override { return GameState::get().getCollection(); }
    std::map<RoleAndStar, int> getChessCountMap() const override { return GameState::get().getChessCountMap(); }
    std::vector<Chess> getSelectedForBattle() const override { return GameState::get().getSelectedForBattle(); }
    void setSelectedInstanceIds(const std::vector<ChessInstanceID>& selected) override { GameState::get().setSelectedInstanceIds(selected); }

    Chess createChess(Role* role, int star) override { return GameState::get().createChess(role, star); }
    void updateToCollection(Chess chess) override { GameState::get().updateToCollection(chess); }
    void removeChess(ChessInstanceID chessInstanceId) override { GameState::get().removeChess(chessInstanceId); }
    Chess upgradeChessInstance(ChessInstanceID instanceId, int newStar) override { return GameState::get().upgradeChessInstance(instanceId, newStar); }
    std::vector<Chess> getChessByStarAndTier(int star, int maxTier) const override { return GameState::get().getChessByStarAndTier(star, maxTier); }
    Chess findChessByInstanceId(ChessInstanceID chessInstanceId) const override { return GameState::get().findChessByInstanceId(chessInstanceId); }

    bool isShopLocked() const override { return GameState::get().isShopLocked(); }
    void setShopLocked(bool value) override { GameState::get().setShopLocked(value); }
    ChessPool& chessPool() override { return GameState::get().chessPool; }
    const ChessPool& chessPool() const override { return GameState::get().chessPool; }
    BattleProgress& battleProgress() override { return GameState::get().battleProgress; }
    const BattleProgress& battleProgress() const override { return GameState::get().battleProgress; }

    const std::vector<int>& getObtainedNeigong() const override { return GameState::get().getObtainedNeigong(); }
    void addNeigong(int magicId) override { GameState::get().addNeigong(magicId); }

    bool isChallengeCompleted(int idx) const override { return GameState::get().isChallengeCompleted(idx); }
    void completeChallenge(int idx) override { GameState::get().completeChallenge(idx); }

    bool isPositionSwapEnabled() const override { return GameState::get().isPositionSwapEnabled(); }
    void setPositionSwapEnabled(bool enabled) override { GameState::get().setPositionSwapEnabled(enabled); }

    void storeEquipmentItem(int itemId) override { GameState::get().storeEquipmentItem(itemId); }
    ChessStoredItemStats getStoredItemStats(int itemId) const override {
        auto stats = GameState::get().getStoredItemStats(itemId);
        return {stats.totalCount, stats.equippedCount, stats.availableInstanceId};
    }

    unsigned int getEnemyCallCount() const override { return GameState::get().getEnemyCallCount(); }
    void setEnemyCallCount(unsigned int count) override { GameState::get().setEnemyCallCount(count); }
    void restoreRand() override { GameState::get().restoreRand(); }
    int shopRandInt(int n) override { return GameState::get().shopRandInt(n); }
    int enemyRandInt(int n) override { return GameState::get().enemyRandInt(n); }
};

class LegacyChessCatalogAdapter : public IChessCatalog
{
public:
    Role* getRole(int roleId) const override { return Save::getInstance()->getRole(roleId); }
    Item* getItem(int itemId) const override { return Save::getInstance()->getItem(itemId); }
    const std::vector<Role*>& getRoles() const override { return Save::getInstance()->getRoles(); }
};

class LegacyChessFeedbackAdapter : public IChessFeedback
{
public:
    void showMessage(const std::string& text, int fontSize) override {
        auto box = std::make_shared<TextBox>();
        box->setText(text);
        box->setFontSize(fontSize);
        box->runCentered(Engine::getInstance()->getUIHeight() / 2);
    }

    void playUpgradeSound() override {
        Audio::getInstance()->playESound(72);
    }
};

LegacyChessGameStateAdapter g_legacyChessGameState;
LegacyChessCatalogAdapter g_legacyChessCatalog;
LegacyChessFeedbackAdapter g_legacyChessFeedback;

}

IChessGameState& legacyChessGameState()
{
    return g_legacyChessGameState;
}

IChessCatalog& legacyChessCatalog()
{
    return g_legacyChessCatalog;
}

IChessFeedback& legacyChessFeedback()
{
    return g_legacyChessFeedback;
}

ChessContext makeLegacyChessContext()
{
    return {legacyChessGameState(), legacyChessCatalog(), legacyChessFeedback()};
}

}