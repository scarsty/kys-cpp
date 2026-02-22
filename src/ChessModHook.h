#pragma once

class SQLite3Wrapper;

namespace KysChess
{

class ChessModHook
{
public:
    static bool overrideNewGame(int& scene, int& x, int& y, int& event);
    static void onSubSceneEntrance(int submap_id);
    static bool interceptEvent(int submap_id, int event_id);
    static bool blockExit(int submap_id);
    static void showMenu();

    // Persist/restore GameData to/from SQLite save DB
    static void saveGameData(SQLite3Wrapper& db);
    static void loadGameData(SQLite3Wrapper& db);
};

}    // namespace KysChess
