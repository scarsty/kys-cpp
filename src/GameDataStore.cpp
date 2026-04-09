#include "GameDataStore.h"

#include <glaze/json.hpp>

namespace KysChess
{

void GameDataStore::reset() {
    *this = GameDataStore{};
}

}

template <>
struct glz::meta<KysChess::Difficulty> {
    using enum KysChess::Difficulty;
    static constexpr auto value = glz::enumerate(Easy, Normal, Hard);
};
