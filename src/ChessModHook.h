#pragma once

#include <memory>

class SQLite3Wrapper;

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
    GameState& gameState_;
    std::unique_ptr<ChessSelector> selector_;
};

class ChessModHook
{
public:
    static bool overrideNewGame(int& scene, int& x, int& y, int& event);
    static GameDataStore exportGameData();
    static void importGameData(const GameDataStore& store);
};

}    // namespace KysChess
