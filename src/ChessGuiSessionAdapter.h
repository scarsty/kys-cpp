#pragma once

#include "ChessGameSession.h"
#include "ChessContextMenu.h"
#include "ChessGuiBattleFlow.h"

namespace KysChess
{

class ChessPreparationScreen;

class ChessGuiSessionAdapter
{
public:
    explicit ChessGuiSessionAdapter(ChessGameSession& session,
        std::array<std::optional<ChessTalentId>, 2> teamTalents = {})
        : session_(session), teamTalents_(teamTalents) {}

    ChessActionResult submitAction(const ChessAction& action) { return session_.submitAndDrain(action); }
    ChessGuiFlowResult runPreparedBattle() { return drainPreparedBattle(); }
    void showContextMenu();
    void showSystemMenu();

private:
    bool openPreparationCommand(ChessContextMenuAction command);
    ChessActionResult submitGuiAction(const ChessAction& action);
    void showBanManagement();
    bool chooseBan(const ChessLegalActionDescriptor& descriptor);
    void showEquipmentMenu();
    void showEquipmentInventory();
    void chooseEquipment(const ChessLegalActionDescriptor& descriptor);
    void chooseLegendary(const ChessLegalActionDescriptor& descriptor);
    void showOverviewMenu();
    void showOfflineBattle();
    bool runSystemMenu();
    void showEnemyReroll();
    void viewCombos();
    void viewChessPool();
    void viewNeigong();
    void viewEffects();
    void showGameGuide();
    void showSystemSettings();
    ChessGuiFlowResult chooseChallenge(const ChessLegalActionDescriptor& descriptor);
    ChessGuiFlowResult chooseReward(const ChessLegalActionDescriptor& descriptor);
    void chooseMap(const ChessLegalActionDescriptor& descriptor);
    ChessGuiFlowResult drainPreparedBattle();
    ChessGuiFlowResult drainRewards(bool stopBeforeForcedBan = false);
    void showBattlePreview() const;

    ChessGameSession& session_;
    std::array<std::optional<ChessTalentId>, 2> teamTalents_;
    std::shared_ptr<ChessPreparationScreen> preparation_;
};

}
