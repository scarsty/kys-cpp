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
    ChessGuiFlowResult chooseAndSubmit(const ChessLegalActionDescriptor& descriptor);
    void showShop();
    void chooseChess(ChessActionType actionType);
    void chooseDeployment();
    void showBanManagement();
    void chooseBan(const ChessLegalActionDescriptor& descriptor);
    void showEquipmentMenu();
    void showEquipmentInventory();
    void chooseEquipment(const ChessLegalActionDescriptor& descriptor);
    void chooseLegendary(const ChessLegalActionDescriptor& descriptor);
    void showOverviewMenu();
    void showPositionSwap();
    void showEnemyReroll();
    void viewCombos();
    void viewChessPool();
    void viewNeigong();
    void showGameGuide();
    void showSystemSettings();
    ChessGuiFlowResult chooseChallenge(const ChessLegalActionDescriptor& descriptor);
    ChessGuiFlowResult chooseReward(const ChessLegalActionDescriptor& descriptor);
    void chooseMap(const ChessLegalActionDescriptor& descriptor);
    void chooseSwap(const ChessLegalActionDescriptor& descriptor);
    ChessGuiFlowResult drainPreparedBattle();
    ChessGuiFlowResult drainRewards(bool stopBeforeForcedBan = false);
    void showBattlePreview() const;

    ChessGameSession& session_;
};

}
