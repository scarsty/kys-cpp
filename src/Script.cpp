#include "Script.h"
#include "EventMacro.h"
#include "PotConv.h"
#include "convert.h"

Script::Script()
{
    lua_state_ = luaL_newstate();

    luaL_openlibs(lua_state_);
    luaopen_base(lua_state_);
    luaopen_table(lua_state_);
    luaopen_math(lua_state_);
    luaopen_string(lua_state_);

    registerEventFunctions();
}

Script::~Script()
{
    lua_close(lua_state_);
}

int Script::runScript(std::string filename)
{
    std::string content = convert::readStringFromFile(filename);
    printf("%s\n", content.c_str());
    std::transform(content.begin(), content.end(), content.begin(), ::tolower);
    luaL_loadbuffer(lua_state_, content.c_str(), content.size(), "code");
    int r = lua_pcall(lua_state_, 0, 0, 0);
    if (r)
    {
        printf("\nError: %s\n", lua_tostring(lua_state_, -1));
    }
    return r;
}

int Script::registerEventFunctions()
{
#define _I(i) (lua_tonumber(L, i))

#define EVENT_VOID(function, ...) { if (!Event::getInstance()->isExiting()) { Event::getInstance()->function(__VA_ARGS__); } return 0; }
#define EVENT_BOOL(function, ...) { if (!Event::getInstance()->isExiting()) { lua_pushboolean(L, Event::getInstance()->function(__VA_ARGS__)); return 1; } return 0; }

#define VOID_0(function) { EVENT_VOID(function); }
#define VOID_1(function) { EVENT_VOID(function, _I(1)); }
#define VOID_2(function) { EVENT_VOID(function, _I(1), _I(2)); }
#define VOID_3(function) { EVENT_VOID(function, _I(1), _I(2), _I(3)); }
#define VOID_4(function) { EVENT_VOID(function, _I(1), _I(2), _I(3), _I(4)); }
#define VOID_5(function) { EVENT_VOID(function, _I(1), _I(2), _I(3), _I(4), _I(5)); }
#define VOID_6(function) { EVENT_VOID(function, _I(1), _I(2), _I(3), _I(4), _I(5), _I(6)); }
#define VOID_7(function) { EVENT_VOID(function, _I(1), _I(2), _I(3), _I(4), _I(5), _I(6), _I(7)); }
#define VOID_8(function) { EVENT_VOID(function, _I(1), _I(2), _I(3), _I(4), _I(5), _I(6), _I(7), _I(8)); }
#define VOID_9(function) { EVENT_VOID(function, _I(1), _I(2), _I(3), _I(4), _I(5), _I(6), _I(7), _I(8), _I(9)); }
#define VOID_10(function) { EVENT_VOID(function, _I(1), _I(2), _I(3), _I(4), _I(5), _I(6), _I(7), _I(8), _I(9), _I(10)); }
#define VOID_11(function) { EVENT_VOID(function, _I(1), _I(2), _I(3), _I(4), _I(5), _I(6), _I(7), _I(8), _I(9), _I(10), _I(11)); }
#define VOID_12(function) { EVENT_VOID(function, _I(1), _I(2), _I(3), _I(4), _I(5), _I(6), _I(7), _I(8), _I(9), _I(10), _I(11), _I(12)); }
#define VOID_13(function) { EVENT_VOID(function, _I(1), _I(2), _I(3), _I(4), _I(5), _I(6), _I(7), _I(8), _I(9), _I(10), _I(11), _I(12), _I(13)); }

#define BOOL_0(function) { EVENT_BOOL(function); }
#define BOOL_1(function) { EVENT_BOOL(function, _I(1)); }
#define BOOL_2(function) { EVENT_BOOL(function, _I(1), _I(2));}
#define BOOL_3(function) { EVENT_BOOL(function, _I(1), _I(2), _I(3)); }
#define BOOL_4(function) { EVENT_BOOL(function, _I(1), _I(2), _I(3), I(4));}
#define BOOL_5(function) { EVENT_BOOL(function, _I(1), _I(2), _I(3), _I(4), _I(5)); }

#define REGISTER_INSTRUCT(function, INSTRUCT_TYPE) \
    { \
        auto function = [](lua_State* L) -> int \
        { \
            INSTRUCT_TYPE(function); \
        }; \
        std::string name = #function; \
        std::transform(name.begin(), name.end(), name.begin(), ::tolower); \
        lua_register(lua_state_, name.c_str(), function); \
    }

#define REGISTER_INSTRUCT_ALIAS(name, function, INSTRUCT_TYPE) \
    { \
        auto name = [](lua_State* L) -> int \
        { \
            INSTRUCT_TYPE(function); \
        }; \
        lua_register(lua_state_, #name, name); \
    }

    auto blank = [](lua_State* L) -> int
    {
        return 0;
    };

    REGISTER_INSTRUCT(oldTalk, VOID_3);
    REGISTER_INSTRUCT(addItem, VOID_2);
    REGISTER_INSTRUCT(modifyEvent, VOID_13);
    REGISTER_INSTRUCT(isUsingItem, BOOL_1);
    REGISTER_INSTRUCT(askBattle, BOOL_0);
    REGISTER_INSTRUCT(tryBattle, BOOL_2);
    REGISTER_INSTRUCT(forceExit, VOID_0);
    REGISTER_INSTRUCT(changeMainMapMusic, VOID_1);
    REGISTER_INSTRUCT(askJoin, BOOL_0);
    REGISTER_INSTRUCT(join, VOID_1);
    REGISTER_INSTRUCT(askRest, BOOL_0);
    REGISTER_INSTRUCT(rest, VOID_0);
    REGISTER_INSTRUCT(lightScence, VOID_0);
    REGISTER_INSTRUCT(darkScence, VOID_0);
    REGISTER_INSTRUCT(dead, VOID_0);
    REGISTER_INSTRUCT(inTeam, BOOL_1);
    REGISTER_INSTRUCT(setSubMapLayerData, VOID_5);
    REGISTER_INSTRUCT(haveItemBool, BOOL_1);
    REGISTER_INSTRUCT(oldSetScencePosition, VOID_2);
    REGISTER_INSTRUCT(teamIsFull, BOOL_0);
    REGISTER_INSTRUCT(leaveTeam, VOID_1);
    REGISTER_INSTRUCT(zeroAllMP, VOID_0);
    REGISTER_INSTRUCT(setRoleUsePoison, VOID_2);
    REGISTER_INSTRUCT(subMapViewFromTo, VOID_4);
    REGISTER_INSTRUCT(add3EventNum, VOID_5);
    REGISTER_INSTRUCT(playAnimation, VOID_3);
    REGISTER_INSTRUCT(checkRoleMorality, BOOL_3);
    REGISTER_INSTRUCT(checkRoleAttack, BOOL_3);
    REGISTER_INSTRUCT(walkFromTo, VOID_4);
    REGISTER_INSTRUCT(checkEnoughMoney, BOOL_1);
    REGISTER_INSTRUCT(addItemWithoutHint, VOID_2);
    REGISTER_INSTRUCT(oldLearnMagic, VOID_3);
    REGISTER_INSTRUCT(addIQ, VOID_2);
    REGISTER_INSTRUCT(setRoleMagic, VOID_4);
    REGISTER_INSTRUCT(checkRoleSexual, BOOL_1);
    REGISTER_INSTRUCT(addMorality, VOID_1);
    REGISTER_INSTRUCT(changeSubMapPic, VOID_4);
    REGISTER_INSTRUCT(openSubMap, VOID_1);
    REGISTER_INSTRUCT(setTowards, VOID_1);
    REGISTER_INSTRUCT(roleAddItem, VOID_3);
    REGISTER_INSTRUCT(checkFemaleInTeam, BOOL_0);
    REGISTER_INSTRUCT(haveItemBool, BOOL_1);
    REGISTER_INSTRUCT(play2Amination, VOID_6);
    REGISTER_INSTRUCT(addSpeed, VOID_2);
    REGISTER_INSTRUCT(addMaxMP, VOID_2);
    REGISTER_INSTRUCT(addAttack, VOID_2);
    REGISTER_INSTRUCT(addMaxHP, VOID_2);
    REGISTER_INSTRUCT(setMPType, VOID_2);
    REGISTER_INSTRUCT(checkHave5Item, BOOL_5);
    REGISTER_INSTRUCT(askSoftStar, VOID_0);
    REGISTER_INSTRUCT(showMorality, VOID_0);
    REGISTER_INSTRUCT(showFame, VOID_0);
    REGISTER_INSTRUCT(openAllSubMap, VOID_0);
    REGISTER_INSTRUCT(checkEventID, BOOL_2);
    REGISTER_INSTRUCT(addFame, VOID_1);
    REGISTER_INSTRUCT(breakStoneGate, VOID_0);
    REGISTER_INSTRUCT(fightForTop, VOID_0);
    REGISTER_INSTRUCT(allLeave, VOID_0);
    REGISTER_INSTRUCT(checkSubMapPic, BOOL_3);
    REGISTER_INSTRUCT(check14BooksPlaced, BOOL_0);
    REGISTER_INSTRUCT(backHome, VOID_6);
    REGISTER_INSTRUCT(setSexual, VOID_2);
    REGISTER_INSTRUCT(shop, VOID_0);
    REGISTER_INSTRUCT(playMusic, VOID_1);
    REGISTER_INSTRUCT(playWave, VOID_1);

    REGISTER_INSTRUCT(instruct_0, VOID_0);
    REGISTER_INSTRUCT(instruct_1, VOID_3);
    REGISTER_INSTRUCT(instruct_2, VOID_2);
    REGISTER_INSTRUCT(instruct_3, VOID_13);
    REGISTER_INSTRUCT(instruct_4, BOOL_1);
    REGISTER_INSTRUCT(instruct_5, BOOL_0);
    REGISTER_INSTRUCT(instruct_6, BOOL_2);
    REGISTER_INSTRUCT(instruct_7, VOID_0);
    REGISTER_INSTRUCT(instruct_8, VOID_1);
    REGISTER_INSTRUCT(instruct_9, BOOL_0);
    REGISTER_INSTRUCT(instruct_10, VOID_1);
    REGISTER_INSTRUCT(instruct_11, BOOL_0);
    REGISTER_INSTRUCT(instruct_12, VOID_0);
    REGISTER_INSTRUCT(instruct_13, VOID_0);
    REGISTER_INSTRUCT(instruct_14, VOID_0);
    REGISTER_INSTRUCT(instruct_15, VOID_0);
    REGISTER_INSTRUCT(instruct_16, BOOL_1);
    REGISTER_INSTRUCT(instruct_17, VOID_5);
    REGISTER_INSTRUCT(instruct_18, BOOL_1);
    REGISTER_INSTRUCT(instruct_19, VOID_2);
    REGISTER_INSTRUCT(instruct_20, BOOL_0);
    REGISTER_INSTRUCT(instruct_21, VOID_1);
    REGISTER_INSTRUCT(instruct_22, VOID_0);
    REGISTER_INSTRUCT(instruct_23, VOID_2);
    REGISTER_INSTRUCT(instruct_24, VOID_0);
    REGISTER_INSTRUCT(instruct_25, VOID_4);
    REGISTER_INSTRUCT(instruct_26, VOID_5);
    REGISTER_INSTRUCT(instruct_27, VOID_3);
    REGISTER_INSTRUCT(instruct_28, BOOL_3);
    REGISTER_INSTRUCT(instruct_29, BOOL_3);
    REGISTER_INSTRUCT(instruct_30, VOID_4);
    REGISTER_INSTRUCT(instruct_31, BOOL_1);
    REGISTER_INSTRUCT(instruct_32, VOID_2);
    REGISTER_INSTRUCT(instruct_33, VOID_3);
    REGISTER_INSTRUCT(instruct_34, VOID_2);
    REGISTER_INSTRUCT(instruct_35, VOID_4);
    REGISTER_INSTRUCT(instruct_36, BOOL_1);
    REGISTER_INSTRUCT(instruct_37, VOID_1);
    REGISTER_INSTRUCT(instruct_38, VOID_4);
    REGISTER_INSTRUCT(instruct_39, VOID_1);
    REGISTER_INSTRUCT(instruct_40, VOID_1);
    REGISTER_INSTRUCT(instruct_41, VOID_3);
    REGISTER_INSTRUCT(instruct_42, BOOL_0);
    REGISTER_INSTRUCT(instruct_43, BOOL_1);
    REGISTER_INSTRUCT(instruct_44, VOID_6);
    REGISTER_INSTRUCT(instruct_45, VOID_2);
    REGISTER_INSTRUCT(instruct_46, VOID_2);
    REGISTER_INSTRUCT(instruct_47, VOID_2);
    REGISTER_INSTRUCT(instruct_48, VOID_2);
    REGISTER_INSTRUCT(instruct_49, VOID_2);
    REGISTER_INSTRUCT(instruct_50, BOOL_5);
    REGISTER_INSTRUCT(instruct_51, VOID_0);
    REGISTER_INSTRUCT(instruct_52, VOID_0);
    REGISTER_INSTRUCT(instruct_53, VOID_0);
    REGISTER_INSTRUCT(instruct_54, VOID_0);
    REGISTER_INSTRUCT(instruct_55, BOOL_2);
    REGISTER_INSTRUCT(instruct_56, VOID_1);
    REGISTER_INSTRUCT(instruct_57, VOID_0);
    REGISTER_INSTRUCT(instruct_58, VOID_0);
    REGISTER_INSTRUCT(instruct_59, VOID_0);
    REGISTER_INSTRUCT(instruct_60, BOOL_3);
    REGISTER_INSTRUCT(instruct_61, BOOL_0);
    REGISTER_INSTRUCT(instruct_62, VOID_6);
    REGISTER_INSTRUCT(instruct_63, VOID_2);
    REGISTER_INSTRUCT(instruct_64, VOID_0);
    lua_register(lua_state_, "instruct_65", blank);
    REGISTER_INSTRUCT(instruct_66, VOID_1);
    REGISTER_INSTRUCT(instruct_67, VOID_1);

    REGISTER_INSTRUCT(instruct_50e, VOID_7);

    return 0;
}
