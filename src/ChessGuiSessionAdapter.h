#pragma once

#include "ChessGameSession.h"
#include "ChessGuiBattleFlow.h"

namespace KysChess
{

class ChessGuiSessionAdapter
{
public:
    explicit ChessGuiSessionAdapter(ChessGameSession& session) : session_(session) {}

    ChessActionResult submitAction(const ChessAction& action) { return session_.submitAndDrain(action); }
    ChessGuiFlowResult runPreparedBattle() { return drainPreparedBattle(); }
    void showContextMenu();

private:
    ChessActionResult submitGuiAction(const ChessAction& action);
    ChessGuiFlowResult chooseAndSubmit(const ChessActionOffer& offer);
    void showShop();
    void chooseChess(ChessActionType actionType);
    void chooseDeployment();
    void showBanManagement();
    void chooseBan(const ChessActionOffer& offer);
    void showEquipmentMenu();
    void showEquipmentInventory();
    void chooseEquipment(const ChessActionOffer& offer);
    void chooseLegendary(const ChessActionOffer& offer);
    void showOverviewMenu();
    void showPositionSwap();
    void showEnemyReroll();
    void viewCombos();
    void viewChessPool();
    void viewNeigong();
    void showGameGuide();
    void showSystemSettings();
    ChessGuiFlowResult chooseChallenge(const ChessActionOffer& offer);
    ChessGuiFlowResult chooseReward(const ChessActionOffer& offer);
    void chooseMap(const ChessActionOffer& offer);
    void chooseSwap(const ChessActionOffer& offer);
    ChessGuiFlowResult drainPreparedBattle();
    ChessGuiFlowResult drainRewards(bool stopBeforeForcedBan = false);
    void showBattlePreview() const;

    ChessGameSession& session_;
};

}
