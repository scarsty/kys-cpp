#include "BattleCreator.h"
#include "DynamicLibrary.h"

#ifdef _WIN32
#define _STDCALL __stdcall
#define _DLLEXPORT __declspec(dllexport)
#else
#define _STDCALL
#define _DLLEXPORT
#endif

BattleScene* BattleCreator::createBattleScene()
{
    typedef void*(_STDCALL * MYFUNC)();
    MYFUNC func = nullptr;
    func = (MYFUNC)DynamicLibrary::getFunction("battle", "_battlescene@0");
    if (func)
    {
        auto bs = (BattleScene*)func();
        return bs;
    }
    else
    {
        auto bs = new BattleScene();
        return bs;
    }
}
