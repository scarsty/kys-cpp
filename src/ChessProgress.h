#pragma once

#include <set>
#include <vector>

#include "GameDataStore.h"

namespace KysChess
{

class BattleProgress {
public:
    BattleProgress() : fight_(0) {}

    void advance() { fight_++; }
    int getFight() const { return fight_; }
    void setFight(int f) { fight_ = f; }
    bool isBossFight() const;
    bool isGameComplete() const;

private:
    int fight_;
};

class ChessProgress
{
public:
    ChessProgress() = default;
    explicit ChessProgress(const GameDataStore& store);

    void exportTo(GameDataStore& store) const;

    bool isPositionSwapEnabled() const;
    void setPositionSwapEnabled(bool value);

    const std::vector<int>& getObtainedNeigong() const { return obtainedNeigong_; }
    void addNeigong(int magicId) { obtainedNeigong_.push_back(magicId); }
    void setObtainedNeigong(std::vector<int> value) { obtainedNeigong_ = std::move(value); }

    bool isChallengeCompleted(int idx) const { return completedChallenges_.contains(idx); }
    void completeChallenge(int idx) { completedChallenges_.insert(idx); }

    BattleProgress& battleProgress() { return battleProgress_; }
    const BattleProgress& battleProgress() const { return battleProgress_; }

private:
    BattleProgress battleProgress_;
    std::vector<int> obtainedNeigong_;
    std::set<int> completedChallenges_;
};

}