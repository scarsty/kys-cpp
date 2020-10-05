#include "Script.h"
#include "Event.h"
#include "EventMacro.h"
#include "PotConv.h"
#include "convert.h"
#include <array>

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

int Script::runScript(const std::string& filename)
{
    std::string content = convert::readStringFromFile(filename);
    fmt::print("{}\n", content.c_str());
    std::transform(content.begin(), content.end(), content.begin(), ::tolower);
    luaL_loadbuffer(lua_state_, content.c_str(), content.size(), "code");
    int r = lua_pcall(lua_state_, 0, 0, 0);
    if (r)
    {
        fmt::print("\nError: {}\n", lua_tostring(lua_state_, -1));
    }
    return r;
}

int Script::registerEventFunctions()
{
#define REGISTER_INSTRUCT_ALIAS(name, function) \
    { \
        auto function = [](lua_State* L) -> int \
        { \
            if (Event::getInstance()->isExiting()) { return 0; } \
            constexpr std::size_t arg_count = arg_counter<decltype(&Event::function), Event>::value; \
            std::array<int, arg_count> args; \
            for (int i = 0; i < arg_count; i++) \
            { \
                args[i] = lua_tonumber(L, i + 1); \
            } \
            return runner(&Event::function, Event::getInstance(), args, L); \
        }; \
        std::string name_str = name; \
        std::transform(name_str.begin(), name_str.end(), name_str.begin(), ::tolower); \
        lua_register(lua_state_, name_str.c_str(), function); \
    }

#define REGISTER_INSTRUCT(function) REGISTER_INSTRUCT_ALIAS(#function, function)

    auto blank = [](lua_State* L) -> int
    {
        return 0;
    };

    REGISTER_INSTRUCT(oldTalk);
    REGISTER_INSTRUCT(addItem);
    REGISTER_INSTRUCT(modifyEvent);
    REGISTER_INSTRUCT(isUsingItem);
    REGISTER_INSTRUCT(askBattle);
    REGISTER_INSTRUCT(tryBattle);
    REGISTER_INSTRUCT(forceExit);
    REGISTER_INSTRUCT(changeMainMapMusic);
    REGISTER_INSTRUCT(askJoin);
    REGISTER_INSTRUCT(join);
    REGISTER_INSTRUCT(askRest);
    REGISTER_INSTRUCT(rest);
    REGISTER_INSTRUCT(lightScence);
    REGISTER_INSTRUCT(darkScence);
    REGISTER_INSTRUCT(dead);
    REGISTER_INSTRUCT(inTeam);
    REGISTER_INSTRUCT(setSubMapLayerData);
    REGISTER_INSTRUCT(haveItemBool);
    REGISTER_INSTRUCT(oldSetScencePosition);
    REGISTER_INSTRUCT(teamIsFull);
    REGISTER_INSTRUCT(leaveTeam);
    REGISTER_INSTRUCT(zeroAllMP);
    REGISTER_INSTRUCT(setRoleUsePoison);
    REGISTER_INSTRUCT(subMapViewFromTo);
    REGISTER_INSTRUCT(add3EventNum);
    REGISTER_INSTRUCT(playAnimation);
    REGISTER_INSTRUCT(checkRoleMorality);
    REGISTER_INSTRUCT(checkRoleAttack);
    REGISTER_INSTRUCT(walkFromTo);
    REGISTER_INSTRUCT(checkEnoughMoney);
    REGISTER_INSTRUCT(addItemWithoutHint);
    REGISTER_INSTRUCT(oldLearnMagic);
    REGISTER_INSTRUCT(addIQ);
    REGISTER_INSTRUCT(setRoleMagic);
    REGISTER_INSTRUCT(checkRoleSexual);
    REGISTER_INSTRUCT(addMorality);
    REGISTER_INSTRUCT(changeSubMapPic);
    REGISTER_INSTRUCT(openSubMap);
    REGISTER_INSTRUCT(setTowards);
    REGISTER_INSTRUCT(roleAddItem);
    REGISTER_INSTRUCT(checkFemaleInTeam);
    REGISTER_INSTRUCT(haveItemBool);
    REGISTER_INSTRUCT(play2Amination);
    REGISTER_INSTRUCT(addSpeed);
    REGISTER_INSTRUCT(addMaxMP);
    REGISTER_INSTRUCT(addAttack);
    REGISTER_INSTRUCT(addMaxHP);
    REGISTER_INSTRUCT(setMPType);
    REGISTER_INSTRUCT(checkHave5Item);
    REGISTER_INSTRUCT(askSoftStar);
    REGISTER_INSTRUCT(showMorality);
    REGISTER_INSTRUCT(showFame);
    REGISTER_INSTRUCT(openAllSubMap);
    REGISTER_INSTRUCT(checkEventID);
    REGISTER_INSTRUCT(addFame);
    REGISTER_INSTRUCT(breakStoneGate);
    REGISTER_INSTRUCT(fightForTop);
    REGISTER_INSTRUCT(allLeave);
    REGISTER_INSTRUCT(checkSubMapPic);
    REGISTER_INSTRUCT(check14BooksPlaced);
    REGISTER_INSTRUCT(backHome);
    REGISTER_INSTRUCT(setSexual);
    REGISTER_INSTRUCT(shop);
    REGISTER_INSTRUCT(playMusic);
    REGISTER_INSTRUCT(playWave);

    REGISTER_INSTRUCT(instruct_0);
    REGISTER_INSTRUCT(instruct_1);
    REGISTER_INSTRUCT(instruct_2);
    REGISTER_INSTRUCT(instruct_33);
    REGISTER_INSTRUCT(instruct_4);
    REGISTER_INSTRUCT(instruct_5);
    REGISTER_INSTRUCT(instruct_6);
    REGISTER_INSTRUCT(instruct_7);
    REGISTER_INSTRUCT(instruct_8);
    REGISTER_INSTRUCT(instruct_9);
    REGISTER_INSTRUCT(instruct_10);
    REGISTER_INSTRUCT(instruct_11);
    REGISTER_INSTRUCT(instruct_12);
    REGISTER_INSTRUCT(instruct_13);
    REGISTER_INSTRUCT(instruct_14);
    REGISTER_INSTRUCT(instruct_15);
    REGISTER_INSTRUCT(instruct_16);
    REGISTER_INSTRUCT(instruct_17);
    REGISTER_INSTRUCT(instruct_18);
    REGISTER_INSTRUCT(instruct_19);
    REGISTER_INSTRUCT(instruct_20);
    REGISTER_INSTRUCT(instruct_21);
    REGISTER_INSTRUCT(instruct_22);
    REGISTER_INSTRUCT(instruct_23);
    REGISTER_INSTRUCT(instruct_24);
    REGISTER_INSTRUCT(instruct_25);
    REGISTER_INSTRUCT(instruct_26);
    REGISTER_INSTRUCT(instruct_27);
    REGISTER_INSTRUCT(instruct_28);
    REGISTER_INSTRUCT(instruct_29);
    REGISTER_INSTRUCT(instruct_30);
    REGISTER_INSTRUCT(instruct_31);
    REGISTER_INSTRUCT(instruct_32);
    REGISTER_INSTRUCT(instruct_33);
    REGISTER_INSTRUCT(instruct_34);
    REGISTER_INSTRUCT(instruct_35);
    REGISTER_INSTRUCT(instruct_36);
    REGISTER_INSTRUCT(instruct_37);
    REGISTER_INSTRUCT(instruct_38);
    REGISTER_INSTRUCT(instruct_39);
    REGISTER_INSTRUCT(instruct_40);
    REGISTER_INSTRUCT(instruct_41);
    REGISTER_INSTRUCT(instruct_42);
    REGISTER_INSTRUCT(instruct_43);
    REGISTER_INSTRUCT(instruct_44);
    REGISTER_INSTRUCT(instruct_45);
    REGISTER_INSTRUCT(instruct_46);
    REGISTER_INSTRUCT(instruct_47);
    REGISTER_INSTRUCT(instruct_48);
    REGISTER_INSTRUCT(instruct_49);
    REGISTER_INSTRUCT(instruct_50);
    REGISTER_INSTRUCT(instruct_51);
    REGISTER_INSTRUCT(instruct_52);
    REGISTER_INSTRUCT(instruct_53);
    REGISTER_INSTRUCT(instruct_54);
    REGISTER_INSTRUCT(instruct_55);
    REGISTER_INSTRUCT(instruct_56);
    REGISTER_INSTRUCT(instruct_57);
    REGISTER_INSTRUCT(instruct_58);
    REGISTER_INSTRUCT(instruct_59);
    REGISTER_INSTRUCT(instruct_60);
    REGISTER_INSTRUCT(instruct_61);
    REGISTER_INSTRUCT(instruct_62);
    REGISTER_INSTRUCT(instruct_63);
    REGISTER_INSTRUCT(instruct_64);
    lua_register(lua_state_, "instruct_65", blank);
    REGISTER_INSTRUCT(instruct_66);
    REGISTER_INSTRUCT(instruct_67);

    //REGISTER_INSTRUCT(instruct_50e, VOID_7);

    return 0;
}
