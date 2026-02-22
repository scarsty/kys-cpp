#include "ChessModHook.h"
#include "ChessCombo.h"
#include "ChessSelector.h"
#include "Menu.h"
#include "SQLite3Wrapper.h"
#include "TempStore.h"
#include "UISave.h"
#include <format>

namespace KysChess
{

static constexpr int MOD_SCENE_ID = 53;    // 擂鼓山

bool ChessModHook::overrideNewGame(int& scene, int& x, int& y, int& event)
{
    scene = MOD_SCENE_ID;
    x = 21;
    y = 54;
    event = -1;
    GameData::get().reset();
    GameData::get().initRand();
    GameData::get().setMoney(ChessBalance::config().initialMoney);
    ChessCombo::clearActiveStates();
    return true;
}

bool ChessModHook::interceptEvent(int submap_id, int event_id)
{
    if (submap_id == MOD_SCENE_ID && event_id > 0)
    {
        ChessSelector::showContextMenu();
        return true;
    }
    return false;
}

bool ChessModHook::blockExit(int submap_id)
{
    return submap_id == MOD_SCENE_ID;
}

void ChessModHook::showMenu()
{
    auto menu = std::make_shared<MenuText>();
    menu->setStrings({ "讀取進度", "保存進度" });
    menu->setFontSize(24);
    menu->arrange(0, 0, 0, 32);
    menu->runAtPosition(200, 200);
    int r = menu->getResult();
    if (r == 0 || r == 1)
    {
        auto ui_save = std::make_shared<UISave>();
        ui_save->setMode(r);
        ui_save->setFontSize(22);
        ui_save->runAtPosition(200, 260);
    }
}

void ChessModHook::saveGameData(SQLite3Wrapper& db)
{
    auto& gd = GameData::get();

    // Save scalar state
    db.execute("drop table if exists chess_state");
    db.execute("create table chess_state(money int,exp int,level int,fight int,shop_seed int,shop_calls int,enemy_seed int,enemy_calls int,shop_locked int)");
    db.execute(std::format("insert into chess_state values({},{},{},{},{},{},{},{},{})",
        gd.getMoney(), gd.getExp(), gd.getLevel(),
        gd.battleProgress.getFight(),
        gd.shopSeed_, gd.shopCallCount_, gd.enemySeed_, gd.enemyCallCount_,
        gd.isShopLocked() ? 1 : 0));

    // Save chess collection
    db.execute("drop table if exists chess_collection");
    db.execute("create table chess_collection(role_id int,star int,count int)");
    for (auto& [chess, count] : gd.collection.getChess())
    {
        db.execute(std::format("insert into chess_collection values({},{},{})",
            chess.role->ID, chess.star, count));
    }

    // Save selected-for-battle
    db.execute("drop table if exists chess_selected");
    db.execute("create table chess_selected(role_id int,star int)");
    for (auto& c : gd.getSelectedForBattle())
    {
        db.execute(std::format("insert into chess_selected values({},{})",
            c.role->ID, c.star));
    }

    // Save obtained neigong
    db.execute("drop table if exists chess_neigong");
    db.execute("create table chess_neigong(magic_id int)");
    for (int mid : gd.getObtainedNeigong())
        db.execute(std::format("insert into chess_neigong values({})", mid));

    // Save completed challenges
    db.execute("drop table if exists chess_challenges");
    db.execute("create table chess_challenges(idx int)");
    for (int idx : gd.getCompletedChallenges())
        db.execute(std::format("insert into chess_challenges values({})", idx));
}

void ChessModHook::loadGameData(SQLite3Wrapper& db)
{
    auto& gd = GameData::get();

    // Load scalar state
    auto stmt = db.query("select * from chess_state");
    if (stmt.isValid() && stmt.step())
    {
        gd.setMoney(stmt.getColumnInt(0));
        gd.setExp(stmt.getColumnInt(1));
        gd.setLevel(stmt.getColumnInt(2));
        gd.battleProgress.setFight(stmt.getColumnInt(3));
        gd.shopSeed_ = stmt.getColumnInt(4);
        gd.shopCallCount_ = stmt.getColumnInt(5);
        gd.enemySeed_ = stmt.getColumnInt(6);
        gd.enemyCallCount_ = stmt.getColumnInt(7);
        gd.restoreRand();
        gd.setShopLocked(stmt.getColumnInt(8) != 0);
    }

    // Load chess collection
    gd.collection = ChessCollection{};
    auto stmt2 = db.query("select * from chess_collection");
    if (stmt2.isValid())
    {
        while (stmt2.step())
        {
            int role_id = stmt2.getColumnInt(0);
            int star = stmt2.getColumnInt(1);
            int count = stmt2.getColumnInt(2);
            auto role = Save::getInstance()->getRole(role_id);
            if (role)
            {
                for (int i = 0; i < count; ++i)
                    gd.collection.addChess({ role, star });
            }
        }
    }

    // Load selected-for-battle
    std::vector<Chess> selected;
    auto stmt3 = db.query("select * from chess_selected");
    if (stmt3.isValid())
    {
        while (stmt3.step())
        {
            auto role = Save::getInstance()->getRole(stmt3.getColumnInt(0));
            if (role)
                selected.push_back({ role, stmt3.getColumnInt(1) });
        }
    }
    gd.setSelectedForBattle(selected);

    // Load obtained neigong
    std::vector<int> neigong;
    auto stmt4 = db.query("select magic_id from chess_neigong");
    if (stmt4.isValid())
        while (stmt4.step())
            neigong.push_back(stmt4.getColumnInt(0));
    gd.setObtainedNeigong(std::move(neigong));

    // Load completed challenges
    std::set<int> challenges;
    auto stmt5 = db.query("select idx from chess_challenges");
    if (stmt5.isValid())
        while (stmt5.step())
            challenges.insert(stmt5.getColumnInt(0));
    gd.setCompletedChallenges(std::move(challenges));
}

}    // namespace KysChess
