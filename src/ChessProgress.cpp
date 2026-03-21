#include "ChessProgress.h"

#include "ChessBalance.h"
#include "SystemSettings.h"

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
    battleProgress_.setFight(store.fight);
    obtainedNeigong_ = store.obtainedNeigong;
    completedChallenges_ = store.completedChallenges;
}

void ChessProgress::exportTo(GameDataStore& store) const
{
    store.fight = battleProgress_.getFight();
    store.obtainedNeigong = obtainedNeigong_;
    store.completedChallenges = completedChallenges_;
}

bool ChessProgress::isPositionSwapEnabled() const
{
    return SystemSettings::getInstance()->positionSwapEnabled();
}

void ChessProgress::setPositionSwapEnabled(bool value)
{
    SystemSettings::getInstance()->setPositionSwapEnabled(value);
}

}