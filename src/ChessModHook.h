#pragma once

#include "ChessBalance.h"

class SQLite3Wrapper;
class Save;

namespace KysChess
{

class ChessGameSession;
struct GameDataStore;

class ChessMod
{
public:
    explicit ChessMod(ChessGameSession& session);
    ~ChessMod();

    void onSubSceneEntrance(int submapId);
    bool interceptEvent(int submapId, int eventId);
    bool blockExit(int submapId) const;
    void showMenu();
    void showContextMenu();

private:
    ChessGameSession& session_;
};

class ChessModHook
{
public:
    static void initializeSaveState(::Save& save);
    static bool overrideNewGame(int& scene, int& x, int& y, int& event, Difficulty difficulty);
    static GameDataStore exportGameData();
    static bool isGameDataReadable(const GameDataStore& store);
    static bool importGameData(const GameDataStore& store, ::Save& save);
};

}    // namespace KysChess
