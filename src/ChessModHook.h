#pragma once

#include "ChessBalance.h"

class SQLite3Wrapper;
class Save;

namespace KysChess
{

class ChessSelector;
class GameState;
struct GameDataStore;

class ChessMod
{
public:
    explicit ChessMod(GameState& gameState);
    ~ChessMod();

    void onSubSceneEntrance(int submapId);
    bool interceptEvent(int submapId, int eventId);
    bool blockExit(int submapId) const;
    void showMenu();
    void showContextMenu();

private:
    ChessSelector makeSelector() const;

    GameState& gameState_;
};

class ChessModHook
{
public:
    static void initializeSaveState(::Save& save);
    static bool overrideNewGame(int& scene, int& x, int& y, int& event, Difficulty difficulty);
    static GameDataStore exportGameData();
    static void importGameData(const GameDataStore& store);
};

}    // namespace KysChess
