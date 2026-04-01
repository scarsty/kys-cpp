#include "ChessProgress.h"

#include "ChessBalance.h"
#include "SystemSettings.h"

#include <set>

namespace KysChess
{

namespace
{

std::vector<int> sanitizeObtainedNeigong(const std::vector<int>& values)
{
    std::set<int> seen;
    std::vector<int> sanitized;
    sanitized.reserve(values.size());
    for (int magicId : values)
    {
        if (seen.insert(magicId).second)
        {
            sanitized.push_back(magicId);
        }
    }
    return sanitized;
}

}  // namespace

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
    obtainedNeigong_ = sanitizeObtainedNeigong(store.obtainedNeigong);
    completedChallenges_ = store.completedChallenges;
}

void ChessProgress::exportTo(GameDataStore& store) const
{
    store.fight = battleProgress_.getFight();
    store.obtainedNeigong = sanitizeObtainedNeigong(obtainedNeigong_);
    store.completedChallenges = completedChallenges_;
}

void ChessProgress::addNeigong(int magicId)
{
    if (std::find(obtainedNeigong_.begin(), obtainedNeigong_.end(), magicId) == obtainedNeigong_.end())
    {
        obtainedNeigong_.push_back(magicId);
    }
}

void ChessProgress::setObtainedNeigong(std::vector<int> value)
{
    obtainedNeigong_ = sanitizeObtainedNeigong(value);
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