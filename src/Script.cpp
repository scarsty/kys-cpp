#include "Script.h"
#include "others/libconvert.h"
#include "EventMacro.h"

Script Script::script_;

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
    std::transform(content.begin(), content.end(), content.begin(), ::tolower);
    luaL_loadbuffer(lua_state_, content.c_str(), content.size(), "code");
    return lua_pcall(lua_state_, 0, 0, 0);
}

int Script::registerEventFunctions()
{
#define _I(i) (lua_tonumber(L, i))

#define VOID_PARA0(function) { Event::getInstance()->function(); return 0; }
#define VOID_PARA1(function) { Event::getInstance()->function(_I(1)); return 0; }
#define VOID_PARA2(function) { Event::getInstance()->function(_I(1),_I(2)); return 0; }
#define VOID_PARA3(function) { Event::getInstance()->function(_I(1),_I(2),_I(3)); return 0; }
#define VOID_PARA4(function) { Event::getInstance()->function(_I(1),_I(2),_I(3),_I(4)); return 0; }
#define VOID_PARA5(function) { Event::getInstance()->function(_I(1),_I(2),_I(3),_I(4),_I(5)); return 0; }
#define VOID_PARA6(function) { Event::getInstance()->function(_I(1),_I(2),_I(3),_I(4),_I(5),_I(6)); return 0; }
#define VOID_PARA7(function) { Event::getInstance()->function(_I(1),_I(2),_I(3),_I(4),_I(5),_I(6),_I(7)); return 0; }
#define VOID_PARA8(function) { Event::getInstance()->function(_I(1),_I(2),_I(3),_I(4),_I(5),_I(6),_I(7),_I(8)); return 0; }
#define VOID_PARA9(function) { Event::getInstance()->function(_I(1),_I(2),_I(3),_I(4),_I(5),_I(6),_I(7),_I(8),_I(9)); return 0; }
#define VOID_PARA10(function) { Event::getInstance()->function(_I(1),_I(2),_I(3),_I(4),_I(5),_I(6),_I(7),_I(8),_I(9),_I(10)); return 0; }
#define VOID_PARA11(function) { Event::getInstance()->function((_I(1),_I(2),_I(3),_I(4),_I(5),_I(6),_I(7),_I(8),_I(9),_I(10),_I(11)); return 0; }
#define VOID_PARA12(function) { Event::getInstance()->function(_I(1),_I(2),_I(3),_I(4),_I(5),_I(6),_I(7),_I(8),_I(9),_I(10),_I(11),_I(12)); return 0; }
#define VOID_PARA13(function) { Event::getInstance()->function(_I(1),_I(2),_I(3),_I(4),_I(5),_I(6),_I(7),_I(8),_I(9),_I(10),_I(11),_I(12),_I(13)); return 0; }

#define BOOL_PARA0(function) { lua_pushboolean(L,Event::getInstance()->function()); return 1; }
#define BOOL_PARA1(function) { lua_pushboolean(L,Event::getInstance()->function(_I(1))); return 1; }
#define BOOL_PARA2(function) { lua_pushboolean(L,Event::getInstance()->function(_I(1),_I(2))); return 1; }
#define BOOL_PARA3(function) { lua_pushboolean(L,Event::getInstance()->function(_I(1),_I(2),_I(3))); return 1; }
#define BOOL_PARA4(function) { lua_pushboolean(L,Event::getInstance()->function(_I(1),_I(2),_I(3), I(4))); return 1; }
#define BOOL_PARA5(function) { lua_pushboolean(L,Event::getInstance()->function(_I(1),_I(2),_I(3),_I(4),_I(5))); return 1; }

#define REGISTER_INSTRUCT(function, PARA) \
    { \
        auto function = [](lua_State* L)->int \
        { \
            PARA(function); \
        }; \
        std::string name = #function;\
        std::transform(name.begin(), name.end(), name.begin(), ::tolower); \
        lua_register(lua_state_, name.c_str(), function); \
    }

#define REGISTER_INSTRUCT_ALIAS(name, function, PARA) \
    { \
        auto name = [](lua_State* L)->int \
        { \
            PARA(function); \
        }; \
        lua_register(lua_state_, #name, name); \
    }

    auto blank = [](lua_State * L)->int
    {
        return 0;
    };

    REGISTER_INSTRUCT(oldTalk, VOID_PARA3);
    REGISTER_INSTRUCT(addItem, VOID_PARA2);
    REGISTER_INSTRUCT(modifyEvent, VOID_PARA13);
    REGISTER_INSTRUCT(isUsingItem, BOOL_PARA1);
    REGISTER_INSTRUCT(askBattle, BOOL_PARA0);
    REGISTER_INSTRUCT(tryBattle, BOOL_PARA2);
    REGISTER_INSTRUCT(changeMainMapMusic, VOID_PARA1);
    REGISTER_INSTRUCT(askJoin, BOOL_PARA0);
    REGISTER_INSTRUCT(join, VOID_PARA1);
    REGISTER_INSTRUCT(askRest, BOOL_PARA0);
    REGISTER_INSTRUCT(rest, VOID_PARA0);
    REGISTER_INSTRUCT(lightScence, VOID_PARA0);
    REGISTER_INSTRUCT(darkScence, VOID_PARA0);
    REGISTER_INSTRUCT(dead, VOID_PARA0);
    REGISTER_INSTRUCT(inTeam, BOOL_PARA1);
    REGISTER_INSTRUCT(setSubMapLayerData, VOID_PARA5);
    REGISTER_INSTRUCT(haveItemBool, BOOL_PARA1);
    REGISTER_INSTRUCT(oldSetScencePosition, VOID_PARA2);
    REGISTER_INSTRUCT(teamIsFull, BOOL_PARA0);
    REGISTER_INSTRUCT(leaveTeam, VOID_PARA1);
    REGISTER_INSTRUCT(zeroAllMP, VOID_PARA0);
    REGISTER_INSTRUCT(setRoleUsePoison, VOID_PARA2);
    REGISTER_INSTRUCT(subMapViewFromTo, VOID_PARA4);
    REGISTER_INSTRUCT(add3EventNum, VOID_PARA5);
    REGISTER_INSTRUCT(playAnimation, VOID_PARA3);
    REGISTER_INSTRUCT(checkRoleMorality, BOOL_PARA3);
    REGISTER_INSTRUCT(checkRoleAttack, BOOL_PARA3);
    REGISTER_INSTRUCT(walkFromTo, VOID_PARA4);
    REGISTER_INSTRUCT(checkEnoughMoney, BOOL_PARA1);
    REGISTER_INSTRUCT(addItemWithoutHint, VOID_PARA2);
    REGISTER_INSTRUCT(oldLearnMagic, VOID_PARA3);
    REGISTER_INSTRUCT(addIQ, VOID_PARA2);
    REGISTER_INSTRUCT(setRoleMagic, VOID_PARA4);
    REGISTER_INSTRUCT(checkRoleSexual, BOOL_PARA1);
    REGISTER_INSTRUCT(addMorality, VOID_PARA1);
    REGISTER_INSTRUCT(changeSubMapPic, VOID_PARA4);
    REGISTER_INSTRUCT(openSubMap, VOID_PARA1);
    REGISTER_INSTRUCT(setTowards, VOID_PARA1);
    REGISTER_INSTRUCT(roleAddItem, VOID_PARA3);
    REGISTER_INSTRUCT(checkFemaleInTeam, BOOL_PARA0);
    REGISTER_INSTRUCT(haveItemBool, BOOL_PARA1);
    REGISTER_INSTRUCT(play2Amination, VOID_PARA6);
    REGISTER_INSTRUCT(addSpeed, VOID_PARA2);
    REGISTER_INSTRUCT(addMaxMP, VOID_PARA2);
    REGISTER_INSTRUCT(addAttack, VOID_PARA2);
    REGISTER_INSTRUCT(addMaxHP, VOID_PARA2);
    REGISTER_INSTRUCT(setMPType, VOID_PARA2);
    REGISTER_INSTRUCT(checkHave5Item, BOOL_PARA5);
    REGISTER_INSTRUCT(askSoftStar, VOID_PARA0);
    REGISTER_INSTRUCT(showMorality, VOID_PARA0);
    REGISTER_INSTRUCT(showFame, VOID_PARA0);
    REGISTER_INSTRUCT(openAllSubMap, VOID_PARA0);
    REGISTER_INSTRUCT(checkEventID, BOOL_PARA2);
    REGISTER_INSTRUCT(addFame, VOID_PARA1);
    REGISTER_INSTRUCT(breakStoneGate, VOID_PARA0);
    REGISTER_INSTRUCT(fightForTop, VOID_PARA0);
    REGISTER_INSTRUCT(allLeave, VOID_PARA0);
    REGISTER_INSTRUCT(checkSubMapPic, BOOL_PARA3);
    REGISTER_INSTRUCT(check14BooksPlaced, BOOL_PARA0);
    REGISTER_INSTRUCT(backHome, VOID_PARA0);
    REGISTER_INSTRUCT(setSexual, VOID_PARA2);
    REGISTER_INSTRUCT(shop, VOID_PARA0);
    REGISTER_INSTRUCT(playMusic, VOID_PARA1);
    REGISTER_INSTRUCT(playWave, VOID_PARA1);

    lua_register(lua_state_, "instruct_0", blank);
    REGISTER_INSTRUCT(instruct_1, VOID_PARA3);
    REGISTER_INSTRUCT(instruct_2, VOID_PARA2);
    REGISTER_INSTRUCT(instruct_3, VOID_PARA13);
    REGISTER_INSTRUCT(instruct_4, BOOL_PARA1);
    REGISTER_INSTRUCT(instruct_5, BOOL_PARA0);
    REGISTER_INSTRUCT(instruct_6, BOOL_PARA2);
    lua_register(lua_state_, "instruct_7", blank);
    REGISTER_INSTRUCT(instruct_8, VOID_PARA1);
    REGISTER_INSTRUCT(instruct_9, BOOL_PARA0);
    REGISTER_INSTRUCT(instruct_10, VOID_PARA1);
    REGISTER_INSTRUCT(instruct_11, BOOL_PARA0);
    REGISTER_INSTRUCT(instruct_12, VOID_PARA0);
    REGISTER_INSTRUCT(instruct_13, VOID_PARA0);
    REGISTER_INSTRUCT(instruct_14, VOID_PARA0);
    REGISTER_INSTRUCT(instruct_15, VOID_PARA0);
    REGISTER_INSTRUCT(instruct_16, BOOL_PARA1);
    REGISTER_INSTRUCT(instruct_17, VOID_PARA5);
    REGISTER_INSTRUCT(instruct_18, BOOL_PARA1);
    REGISTER_INSTRUCT(instruct_19, VOID_PARA2);
    REGISTER_INSTRUCT(instruct_20, BOOL_PARA0);
    REGISTER_INSTRUCT(instruct_21, VOID_PARA1);
    REGISTER_INSTRUCT(instruct_22, VOID_PARA0);
    REGISTER_INSTRUCT(instruct_23, VOID_PARA2);
    lua_register(lua_state_, "instruct_24", blank);
    REGISTER_INSTRUCT(instruct_25, VOID_PARA4);
    REGISTER_INSTRUCT(instruct_26, VOID_PARA5);
    REGISTER_INSTRUCT(instruct_27, VOID_PARA3);
    REGISTER_INSTRUCT(instruct_28, BOOL_PARA3);
    REGISTER_INSTRUCT(instruct_29, BOOL_PARA3);
    REGISTER_INSTRUCT(instruct_30, VOID_PARA4);
    REGISTER_INSTRUCT(instruct_31, BOOL_PARA1);
    REGISTER_INSTRUCT(instruct_32, VOID_PARA2);
    REGISTER_INSTRUCT(instruct_33, VOID_PARA3);
    REGISTER_INSTRUCT(instruct_34, VOID_PARA2);
    REGISTER_INSTRUCT(instruct_35, VOID_PARA4);
    REGISTER_INSTRUCT(instruct_36, BOOL_PARA1);
    REGISTER_INSTRUCT(instruct_37, VOID_PARA1);
    REGISTER_INSTRUCT(instruct_38, VOID_PARA4);
    REGISTER_INSTRUCT(instruct_39, VOID_PARA1);
    REGISTER_INSTRUCT(instruct_40, VOID_PARA1);
    REGISTER_INSTRUCT(instruct_41, VOID_PARA3);
    REGISTER_INSTRUCT(instruct_42, BOOL_PARA0);
    REGISTER_INSTRUCT(instruct_43, BOOL_PARA1);
    REGISTER_INSTRUCT(instruct_44, VOID_PARA6);
    REGISTER_INSTRUCT(instruct_45, VOID_PARA2);
    REGISTER_INSTRUCT(instruct_46, VOID_PARA2);
    REGISTER_INSTRUCT(instruct_47, VOID_PARA2);
    REGISTER_INSTRUCT(instruct_48, VOID_PARA2);
    REGISTER_INSTRUCT(instruct_49, VOID_PARA2);
    REGISTER_INSTRUCT(instruct_50, BOOL_PARA5);
    REGISTER_INSTRUCT(instruct_51, VOID_PARA0);
    REGISTER_INSTRUCT(instruct_52, VOID_PARA0);
    REGISTER_INSTRUCT(instruct_53, VOID_PARA0);
    REGISTER_INSTRUCT(instruct_54, VOID_PARA0);
    REGISTER_INSTRUCT(instruct_55, BOOL_PARA2);
    REGISTER_INSTRUCT(instruct_56, VOID_PARA1);
    REGISTER_INSTRUCT(instruct_57, VOID_PARA0);
    REGISTER_INSTRUCT(instruct_58, VOID_PARA0);
    REGISTER_INSTRUCT(instruct_59, VOID_PARA0);
    REGISTER_INSTRUCT(instruct_60, BOOL_PARA3);
    REGISTER_INSTRUCT(instruct_61, BOOL_PARA0);
    REGISTER_INSTRUCT(instruct_62, VOID_PARA0);
    REGISTER_INSTRUCT(instruct_63, VOID_PARA2);
    REGISTER_INSTRUCT(instruct_64, VOID_PARA0);
    lua_register(lua_state_, "instruct_65", blank);
    REGISTER_INSTRUCT(instruct_66, VOID_PARA1);
    REGISTER_INSTRUCT(instruct_67, VOID_PARA1);

    REGISTER_INSTRUCT(instruct_50e, VOID_PARA7);

    return 0;
}

