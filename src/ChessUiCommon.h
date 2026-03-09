#pragma once

#include "ChessCombo.h"
#include "ChessEconomy.h"
#include "ChessEquipmentInventory.h"
#include "ChessManager.h"
#include "ChessProgress.h"
#include "ChessRandom.h"
#include "ChessRoleSave.h"
#include "ChessRoster.h"
#include "ChessSelectorPresenter.h"
#include "ChessShop.h"

#include <string>
#include <vector>

namespace KysChess
{

struct ChessSelectorServices
{
    ChessRoleSave& roleSave;
    ChessEquipmentInventory& equipmentInventory;
    ChessRoster& roster;
    ChessShop& shop;
    ChessProgress& progress;
    ChessEconomy& economy;
    ChessRandom& random;
};

ChessSelectorPresenter& chessPresenter();
void showChessMessage(const std::string& text, int fontSize = 32);
void playChessUpgradeSound();
ChessManager makeChessManager(ChessRoster& roster, ChessEquipmentInventory& equipmentInventory, ChessEconomy& economy);
ChessManager makeChessManager(const ChessSelectorServices& services);
std::string comboEffectDesc(const ComboEffect& eff);
std::string comboEffectCompactDesc(const ComboEffect& eff);
std::vector<std::string> wrapDisplayText(const std::string& text, int maxWidth);

}    // namespace KysChess