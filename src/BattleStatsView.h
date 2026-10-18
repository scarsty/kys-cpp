#pragma once

#include "ChessBattleResultModel.h"
#include "RunNode.h"

class BattleStatsView : public RunNode
{
public:
    ~BattleStatsView() override;
    void setupPostBattle(
        const BattlePostBattleSummary& summary,
        const BattleReport& report,
        const KysChess::ChessGameContent& content,
        const KysChess::PreparedChessBattle& prepared,
        const std::array<std::optional<KysChess::ChessTalentId>, 2>& talents,
        std::string context);

    void draw() override;
    void dealEvent(EngineEvent& event) override;
    PointerResult onPointerEvent(const PointerEvent& event) override;
    void onPointerInputReset() override;
    void backRun() override;

private:
    void activate(int action);
    int hit(SDL_FPoint position) const;
    SDL_Rect actionRect(int action) const;
    void drawTeam(int side, SDL_Rect frame);
    void drawTeamSummary(int side, SDL_Rect frame);

    BattlePostBattleSummary summary_;
    const BattleReport* report_{};
    const KysChess::ChessGameContent* content_{};
    std::array<KysChess::ChessBattleResultTeam, 2> teams_;
    std::string context_;
    Texture* background_{};
    int battleLogOpenFrame_ = -1;
    bool battleLogShown_ = false;
    int pressed_ = -1;
    int focused_{};
    bool keyboardReleaseArmed_ = false;
    bool gamepadReleaseArmed_ = false;
};
