#include "GameState.h"

#include "Save.h"

namespace KysChess
{

GameState::GameState()
    : roleSave_(*Save::getInstance())
{
    constructDefaultServices();
}

void GameState::constructDefaultServices()
{
    random_.emplace();
    economy_.emplace();
    equipmentInventory_.emplace();
    roster_.emplace(roleSave_, *equipmentInventory_);
    shop_.emplace(*random_, roleSave_);
    progress_.emplace();
}

void GameState::constructServicesFromStore(const GameDataStore& store)
{
    random_.emplace(store);
    economy_.emplace(store);
    equipmentInventory_.emplace(store);
    roster_.emplace(roleSave_, *equipmentInventory_, store);
    shop_.emplace(*random_, roleSave_, store);
    progress_.emplace(store);
}

// --------------- Lifecycle ---------------------

void GameState::reset()
{
    // The random class is not copyable
    GameDataStore::operator=(GameDataStore{});

    constructDefaultServices();
    ChessBalance::setDifficulty(difficulty());
}

GameDataStore GameState::exportStore() const
{
    GameDataStore exported = *this;
    random().exportTo(exported);
    economy().exportTo(exported);
    equipmentInventory().exportTo(exported);
    roster().exportTo(exported);
    shop().exportTo(exported);
    progress().exportTo(exported);
    return exported;
}

void GameState::importStore(const GameDataStore& store)
{
    GameDataStore::operator=(store);
    ChessBalance::setDifficulty(difficulty());
    constructServicesFromStore(store);
}

}
