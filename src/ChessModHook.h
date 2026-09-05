#pragma once

#include "ChessBalance.h"

#include <string>

class SQLite3Wrapper;
class Save;

namespace KysChess
{

class ChessGameSession;
struct ChessSaveSceneState;
struct ChessSessionCheckpoint;

class ChessMod
{
public:
    explicit ChessMod(ChessGameSession& session);
    ~ChessMod();

    void onSubSceneEntrance(int submapId);
    bool interceptEvent(int submapId, int eventId);
    bool blockExit(int submapId) const;
    void showContextMenu();
    void showSystemMenu();

private:
    ChessGameSession& session_;
};

class ChessModHook
{
public:
    static void initializeSaveState(::Save& save);
    static ChessSaveSceneState initialSaveSceneState();
    static bool overrideNewGame(int& scene, int& x, int& y, int& event, Difficulty difficulty, ChessTalentId talent);
    static bool canSaveCheckpoint();
    static ChessSessionCheckpoint exportCheckpoint();
    static bool isCheckpointReadable(
        const ChessSessionCheckpoint& checkpoint,
        std::string& error);
    static bool importCheckpoint(const ChessSessionCheckpoint& checkpoint, ::Save& save);
};

}    // namespace KysChess
