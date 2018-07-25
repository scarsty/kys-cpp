#include "BattleMod.h"
#include "battle.h"

_DLLEXPORT void* _STDCALL battlescene()
{
    auto p = new BattleMod::BattleModifier();
    return p;
}