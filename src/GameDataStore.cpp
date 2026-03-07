#include "GameDataStore.h"
#include "SQLite3Wrapper.h"
#include "Save.h"
#include <format>

namespace KysChess
{

void GameDataStore::reset() {
    *this = GameDataStore{};
}

void GameDataStore::save(SQLite3Wrapper& db) const {
    // Scalar state
    db.execute("drop table if exists chess_state");
    db.execute("create table chess_state(money int,exp int,level int,fight int,shop_seed int,shop_calls int,enemy_seed int,enemy_calls int,shop_locked int,difficulty int,position_swap int,next_chess_instance_id int, next_equip_instance_id int)");
    db.execute(std::format("insert into chess_state values({},{},{},{},{},{},{},{},{},{},{},{},{})",
        money, exp, level, fight, shopSeed, shopCallCount, enemySeed, enemyCallCount,
        shopLocked ? 1 : 0, static_cast<int>(difficulty), positionSwapEnabled ? 1 : 0, nextChessInstanceId, nextEquipInstanceId));

    // Collection
    db.execute("drop table if exists chess_collection");
    db.execute("create table chess_collection(role_id int,star int,instance_id int, selectd_for_battle int, weapon_instance_id, armor_instance_id)");
    for (auto& c : storedCollection)
        db.execute(std::format("insert into chess_collection values({},{},{},{},{},{})", c.roleId, c.star, c.chessInstanceId, c.selectedForBattle ? 1 : 0, c.weaponInstanceId, c.armorInstanceId));

    // Shop
    db.execute("drop table if exists chess_shop");
    db.execute("create table chess_shop(role_id int,tier int)");
    for (auto& [roleId, tier] : currentShop)
        db.execute(std::format("insert into chess_shop values({},{})", roleId, tier));

    // Neigong
    db.execute("drop table if exists chess_neigong");
    db.execute("create table chess_neigong(magic_id int)");
    for (int mid : obtainedNeigong)
        db.execute(std::format("insert into chess_neigong values({})", mid));

    // Challenges
    db.execute("drop table if exists chess_challenges");
    db.execute("create table chess_challenges(idx int)");
    for (int idx : completedChallenges)
        db.execute(std::format("insert into chess_challenges values({})", idx));

    // Equipment inventory
    db.execute("drop table if exists chess_equipment_inv");
    db.execute("create table chess_equipment_inv(equip_instance_id int, item_id int)");
    for (auto& [equipInstanceId, itemId] : equipmentInventory) {
        db.execute(std::format("insert into chess_equipment_inv values({},{})", equipInstanceId, itemId));
    }
}

void GameDataStore::load(SQLite3Wrapper& db) {
    reset();

    // Scalar state
    auto stmt = db.query("select * from chess_state");
    if (stmt.isValid() && stmt.step()) {
        money = stmt.getColumnInt(0);
        exp = stmt.getColumnInt(1);
        level = stmt.getColumnInt(2);
        fight = stmt.getColumnInt(3);
        shopSeed = stmt.getColumnInt(4);
        shopCallCount = stmt.getColumnInt(5);
        enemySeed = stmt.getColumnInt(6);
        enemyCallCount = stmt.getColumnInt(7);
        shopLocked = stmt.getColumnInt(8) != 0;
        difficulty = static_cast<Difficulty>(stmt.getColumnInt(9));
        positionSwapEnabled = stmt.getColumnInt(10) != 0;
        nextChessInstanceId = stmt.getColumnInt(11),
        nextEquipInstanceId = stmt.getColumnInt(12);
    }

    // Collection
    auto stmt2 = db.query("select * from chess_collection");
    if (stmt2.isValid()) {
        while (stmt2.step()) {
            StoredChess c;
            c.roleId = stmt2.getColumnInt(0);
            c.star = stmt2.getColumnInt(1);
            c.chessInstanceId = stmt2.getColumnInt(2);
            c.selectedForBattle = stmt2.getColumnInt(3) == 1;
            c.weaponInstanceId = stmt2.getColumnInt(4);
            c.armorInstanceId = stmt2.getColumnInt(5);
            storedCollection.push_back(c);
        }
    }

    // Shop
    auto stmt4 = db.query("select * from chess_shop");
    if (stmt4.isValid()) {
        while (stmt4.step())
            currentShop.emplace_back(stmt4.getColumnInt(0), stmt4.getColumnInt(1));
    }

    // Neigong
    auto stmt5 = db.query("select * from chess_neigong");
    if (stmt5.isValid()) {
        while (stmt5.step())
            obtainedNeigong.push_back(stmt5.getColumnInt(0));
    }

    // Challenges
    auto stmt6 = db.query("select * from chess_challenges");
    if (stmt6.isValid()) {
        while (stmt6.step())
            completedChallenges.insert(stmt6.getColumnInt(0));
    }

    // Equipment inventory
    auto stmt8 = db.query("select * from chess_equipment_inv");
    if (stmt8.isValid()) {
        while (stmt8.step())
            equipmentInventory[stmt8.getColumnInt(0)] = stmt8.getColumnInt(1);
    }
}

}
