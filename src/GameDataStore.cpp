#include "GameDataStore.h"
#include "SQLite3Wrapper.h"
#include "Save.h"
#include <glaze/json.hpp>
#include <format>

namespace KysChess
{

namespace
{

constexpr int kGameDataStoreSchemaVersion = 1;
constexpr auto kGameDataStoreWriteOptions = glz::opts{.prettify = true};
constexpr auto kGameDataStoreReadOptions = glz::opts{.error_on_unknown_keys = false};

void dropLegacyGameDataTables(SQLite3Wrapper& db)
{
    db.execute("drop table if exists chess_state");
    db.execute("drop table if exists chess_collection");
    db.execute("drop table if exists chess_shop");
    db.execute("drop table if exists chess_neigong");
    db.execute("drop table if exists chess_challenges");
    db.execute("drop table if exists chess_equipment_inv");
}

}

void GameDataStore::reset() {
    *this = GameDataStore{};
}

void GameDataStore::save(SQLite3Wrapper& db) const {
    std::string payload;
    if (const auto writeResult = glz::write<kGameDataStoreWriteOptions>(*this, payload); writeResult) {
        return;
    }

    dropLegacyGameDataTables(db);
    db.execute("drop table if exists chess_game_data_store");
    db.execute("create table chess_game_data_store(schema_version int not null, payload text not null)");
    db.execute(std::format(
        "insert into chess_game_data_store(schema_version, payload) values({}, '{}')",
        kGameDataStoreSchemaVersion, payload));
}

void GameDataStore::load(SQLite3Wrapper& db) {
    reset();

    auto stmt = db.query("select schema_version, payload from chess_game_data_store limit 1");
    if (!stmt.isValid() || !stmt.step()) {
        return;
    }

    if (stmt.getColumnInt(0) != kGameDataStoreSchemaVersion) {
        return;
    }

    const char* payload = stmt.getColumnText(1);
    if (!payload) {
        return;
    }

    if (const auto readResult = glz::read<kGameDataStoreReadOptions>(*this, std::string(payload)); readResult) {
        reset();
    }
}

}

template <>
struct glz::meta<KysChess::Difficulty> {
    using enum KysChess::Difficulty;
    static constexpr auto value = glz::enumerate(Easy, Normal);
};
