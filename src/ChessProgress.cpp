#include "ChessProgress.h"

#include "ChessBalance.h"

namespace KysChess
{

bool BattleProgress::isBossFight() const
{
    return (fight_ + 1) % ChessBalance::config().bossInterval == 0;
}

bool BattleProgress::isGameComplete() const
{
    return fight_ >= ChessBalance::config().totalFights;
}

ChessProgress::ChessProgress(const GameDataStore& store)
{
    positionSwapEnabled_ = store.positionSwapEnabled;
    battleProgress_.setFight(store.fight);
    obtainedNeigong_ = store.obtainedNeigong;
    completedChallenges_ = store.completedChallenges;
}

void ChessProgress::exportTo(GameDataStore& store) const
{
    store.fight = battleProgress_.getFight();
    store.positionSwapEnabled = positionSwapEnabled_;
    store.obtainedNeigong = obtainedNeigong_;
    store.completedChallenges = completedChallenges_;
}

}