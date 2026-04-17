#include "ScriptCifa.h"
#include "Event.h"
#include "EventMacro.h"
#include "Font.h"
#include "NewSave.h"
#include "PotConv.h"
#include "filefunc.h"
#include "strfunc.h"
#include <algorithm>
#include <functional>

// Cifa版的rModifier：通过字段名或偏移量读写角色/道具等数据
static cifa::Object cModifier(const char* data_name, auto getDataFromIndex, cifa::ObjectVector& args)
{
    if (args.empty()) { return {}; }
    int index = int(args[0]);
    std::string name;
    int offset = -1;
    if (args.size() > 1)
    {
        if (args[1].isNumber())
        {
            offset = int(args[1]);
        }
        else
        {
            name = std::string(args[1]);
        }
    }
    int n = (int)args.size();
    for (auto& info : NewSave::getFieldInfo(data_name))
    {
        if (name == info.name || name == info.name0 || offset == info.offset / (int)sizeof(int))
        {
            char* p = (char*)(std::invoke(getDataFromIndex, Save::getInstance(), index)) + info.offset;
            if (info.type == 0)
            {
                if (n == 2)
                {
                    return cifa::Object(double(*(int*)p));
                }
                else if (n >= 3)
                {
                    *(int*)p = int(args[2]);
                }
            }
            else
            {
                if (n == 2)
                {
                    return cifa::Object(std::string(p));
                }
                else if (n >= 3)
                {
                    std::string str = std::string(args[2]);
                    memset(p, 0, info.length);
                    memcpy(p, str.c_str(), str.size());
                }
            }
            break;
        }
    }
    return {};
}

int p50_32_value_c = 0;
int p50_32_pos_c = INT_MAX;

ScriptCifa::ScriptCifa()
{
    registerEventFunctions();
}

int ScriptCifa::runScript(const std::string& filename)
{
    std::string content = filefunc::readFileToString(filename);
    LOG("{}\n", content);
    std::transform(content.begin(), content.end(), content.begin(), ::tolower);
    return runScriptString(content);
}

int ScriptCifa::runScriptString(const std::string& content)
{
    try
    {
        cifa_.run_script(content);
        if (cifa_.has_error())
        {
            cifa_.print_errors();
            cifa_.set_output_error(true);
            return 1;
        }
        return 0;
    }
    catch (const ScriptExitException&)
    {
        return 0;
    }
    catch (const std::exception& e)
    {
        LOG("\nError: {}\n", e.what());
        return 1;
    }
}

int ScriptCifa::registerEventFunctions()
{
#define REGISTER_CIFA_ALIAS(name, function) \
    { \
        std::string name_str = name; \
        std::transform(name_str.begin(), name_str.end(), name_str.begin(), ::tolower); \
        cifa_.register_function(name_str, [](cifa::ObjectVector& args) -> cifa::Object \
        { \
            return runner(&Event::function, Event::getInstance(), args); \
        }); \
    }

#define REGISTER_CIFA(function) REGISTER_CIFA_ALIAS(#function, function)

    // exit - 立即终止脚本执行
    cifa_.register_function("exit", [](cifa::ObjectVector&) -> cifa::Object
    {
        throw ScriptExitException{};
        return {};
    });

    REGISTER_CIFA(oldTalk);
    REGISTER_CIFA(addItem);
    REGISTER_CIFA(modifyEvent);
    REGISTER_CIFA(isUsingItem);
    REGISTER_CIFA(askBattle);
    REGISTER_CIFA(tryBattle);
    REGISTER_CIFA(forceExit);
    REGISTER_CIFA(changeMainMapMusic);
    REGISTER_CIFA(askJoin);
    REGISTER_CIFA(join);
    REGISTER_CIFA(askRest);
    REGISTER_CIFA(rest);
    REGISTER_CIFA(lightScene);
    REGISTER_CIFA(darkScene);
    REGISTER_CIFA(dead);
    REGISTER_CIFA(inTeam);
    REGISTER_CIFA(setSubMapLayerData);
    REGISTER_CIFA(haveItemBool);
    REGISTER_CIFA(oldSetScenePosition);
    REGISTER_CIFA(teamIsFull);
    REGISTER_CIFA(leaveTeam);
    REGISTER_CIFA(zeroAllMP);
    REGISTER_CIFA(setRoleUsePoison);
    REGISTER_CIFA(subMapViewFromTo);
    REGISTER_CIFA(add3EventNum);
    REGISTER_CIFA(playAnimation);
    REGISTER_CIFA(checkRoleMorality);
    REGISTER_CIFA(checkRoleAttack);
    REGISTER_CIFA(walkFromTo);
    REGISTER_CIFA(checkEnoughMoney);
    REGISTER_CIFA(addItemWithoutHint);
    REGISTER_CIFA(oldLearnMagic);
    REGISTER_CIFA(addIQ);
    REGISTER_CIFA(setRoleMagic);
    REGISTER_CIFA(checkRoleSexual);
    REGISTER_CIFA(addMorality);
    REGISTER_CIFA(changeSubMapPic);
    REGISTER_CIFA(openSubMap);
    REGISTER_CIFA(setTowards);
    REGISTER_CIFA(roleAddItem);
    REGISTER_CIFA(checkFemaleInTeam);
    REGISTER_CIFA(play2Amination);
    REGISTER_CIFA(play3Amination);
    REGISTER_CIFA(addSpeed);
    REGISTER_CIFA(addMaxMP);
    REGISTER_CIFA(addAttack);
    REGISTER_CIFA(addMaxHP);
    REGISTER_CIFA(setMPType);
    REGISTER_CIFA(checkHave5Item);
    REGISTER_CIFA(askSoftStar);
    REGISTER_CIFA(showMorality);
    REGISTER_CIFA(showFame);
    REGISTER_CIFA(openAllSubMap);
    REGISTER_CIFA(checkEventID);
    REGISTER_CIFA(addFame);
    REGISTER_CIFA(breakStoneGate);
    REGISTER_CIFA(fightForTop);
    REGISTER_CIFA(allLeave);
    REGISTER_CIFA(checkSubMapPic);
    REGISTER_CIFA(check14BooksPlaced);
    REGISTER_CIFA(backHome);
    REGISTER_CIFA(setSexual);
    REGISTER_CIFA(shop);
    REGISTER_CIFA(playMusic);
    REGISTER_CIFA(playWave);

    REGISTER_CIFA(instruct_0);
    REGISTER_CIFA(instruct_1);
    REGISTER_CIFA(instruct_2);
    REGISTER_CIFA(instruct_3);
    REGISTER_CIFA(instruct_4);
    REGISTER_CIFA(instruct_5);
    REGISTER_CIFA(instruct_6);
    REGISTER_CIFA(instruct_7);
    REGISTER_CIFA(instruct_8);
    REGISTER_CIFA(instruct_9);
    REGISTER_CIFA(instruct_10);
    REGISTER_CIFA(instruct_11);
    REGISTER_CIFA(instruct_12);
    REGISTER_CIFA(instruct_13);
    REGISTER_CIFA(instruct_14);
    REGISTER_CIFA(instruct_15);
    REGISTER_CIFA(instruct_16);
    REGISTER_CIFA(instruct_17);
    REGISTER_CIFA(instruct_18);
    REGISTER_CIFA(instruct_19);
    REGISTER_CIFA(instruct_20);
    REGISTER_CIFA(instruct_21);
    REGISTER_CIFA(instruct_22);
    REGISTER_CIFA(instruct_23);
    REGISTER_CIFA(instruct_24);
    REGISTER_CIFA(instruct_25);
    REGISTER_CIFA(instruct_26);
    REGISTER_CIFA(instruct_27);
    REGISTER_CIFA(instruct_28);
    REGISTER_CIFA(instruct_29);
    REGISTER_CIFA(instruct_30);
    REGISTER_CIFA(instruct_31);
    REGISTER_CIFA(instruct_32);
    REGISTER_CIFA(instruct_33);
    REGISTER_CIFA(instruct_34);
    REGISTER_CIFA(instruct_35);
    REGISTER_CIFA(instruct_36);
    REGISTER_CIFA(instruct_37);
    REGISTER_CIFA(instruct_38);
    REGISTER_CIFA(instruct_39);
    REGISTER_CIFA(instruct_40);
    REGISTER_CIFA(instruct_41);
    REGISTER_CIFA(instruct_42);
    REGISTER_CIFA(instruct_43);
    REGISTER_CIFA(instruct_44);
    REGISTER_CIFA(instruct_45);
    REGISTER_CIFA(instruct_46);
    REGISTER_CIFA(instruct_47);
    REGISTER_CIFA(instruct_48);
    REGISTER_CIFA(instruct_49);
    REGISTER_CIFA(instruct_50);
    REGISTER_CIFA(instruct_51);
    REGISTER_CIFA(instruct_52);
    REGISTER_CIFA(instruct_53);
    REGISTER_CIFA(instruct_54);
    REGISTER_CIFA(instruct_55);
    REGISTER_CIFA(instruct_56);
    REGISTER_CIFA(instruct_57);
    REGISTER_CIFA(instruct_58);
    REGISTER_CIFA(instruct_59);
    REGISTER_CIFA(instruct_60);
    REGISTER_CIFA(instruct_61);
    REGISTER_CIFA(instruct_62);
    REGISTER_CIFA(instruct_63);
    REGISTER_CIFA(instruct_64);
    // instruct_65 is a no-op (blank)
    cifa_.register_function("instruct_65", [](cifa::ObjectVector&) -> cifa::Object { return {}; });
    REGISTER_CIFA(instruct_66);
    REGISTER_CIFA(instruct_67);

    // instruct_50e - 特殊处理，有副作用参数
    cifa_.register_function("instruct_50e", [](cifa::ObjectVector& args) -> cifa::Object
    {
        std::vector<int> iargs(7, 0);
        for (int i = 0; i < 7 && i < (int)args.size(); i++)
        {
            iargs[i] = int(args[i]);
        }
        Event::getInstance()->instruct_50e(iargs[0], iargs[1], iargs[2], iargs[3], iargs[4], iargs[5], iargs[6], &p50_32_pos_c, &p50_32_value_c);
        return {};
    });

    // talk / newtalk - 支持字符串和数字混合参数
    auto newTalkFunc = [](cifa::ObjectVector& args) -> cifa::Object
    {
        std::vector<int> iargs(7, -1);
        std::string str;
        int i1 = 1;
        for (int i = 0; i < (int)args.size(); i++)
        {
            if (args[i].isNumber())
            {
                if (i1 < 7) { iargs[i1++] = int(args[i]); }
            }
            else
            {
                if (str.empty()) { str = std::string(args[i]); }
            }
        }
        if (str.empty() && !args.empty()) { str = std::string(args[0]); }
        Event::getInstance()->newTalk(str, iargs[1], iargs[2], iargs[3]);
        return {};
    };
    cifa_.register_function("newtalk", newTalkFunc);
    cifa_.register_function("talk", newTalkFunc);

    cifa_.register_function("getitemcountinbag", [](cifa::ObjectVector& args) -> cifa::Object
    {
        int i = args.empty() ? 0 : int(args[0]);
        return cifa::Object(double(Save::getInstance()->getItemCountInBag(i)));
    });

    cifa_.register_function("autosave", [](cifa::ObjectVector&) -> cifa::Object
    {
        Save::getInstance()->save(11);
        return {};
    });

    cifa_.register_function("runsql", [](cifa::ObjectVector& args) -> cifa::Object
    {
        if (!args.empty())
        {
            std::string cmd = std::string(args[0]);
            Save::getInstance()->runSql(cmd);
        }
        return {};
    });

    auto roleFunc = [](cifa::ObjectVector& args) -> cifa::Object
    {
        return cModifier("Role", &Save::getRole, args);
    };
    cifa_.register_function("getrole", roleFunc);
    cifa_.register_function("setrole", roleFunc);

    auto itemFunc = [](cifa::ObjectVector& args) -> cifa::Object
    {
        return cModifier("Item", &Save::getItem, args);
    };
    cifa_.register_function("getitem", itemFunc);
    cifa_.register_function("setitem", itemFunc);

    auto magicFunc = [](cifa::ObjectVector& args) -> cifa::Object
    {
        return cModifier("Magic", &Save::getMagic, args);
    };
    cifa_.register_function("getmagic", magicFunc);
    cifa_.register_function("setmagic", magicFunc);

    auto subMapInfoFunc = [](cifa::ObjectVector& args) -> cifa::Object
    {
        return cModifier("SubMapInfo", &Save::getSubMapInfo, args);
    };
    cifa_.register_function("getsubmapinfo", subMapInfoFunc);
    cifa_.register_function("setsubmapinfo", subMapInfoFunc);

    auto shopFunc = [](cifa::ObjectVector& args) -> cifa::Object
    {
        return cModifier("Shop", &Save::getShop, args);
    };
    cifa_.register_function("getshop", shopFunc);
    cifa_.register_function("setshop", shopFunc);

    cifa_.register_function("drawrect", [](cifa::ObjectVector& args) -> cifa::Object
    {
        if (args.size() < 4) { return {}; }
        int x = int(args[0]), y = int(args[1]), w = int(args[2]), h = int(args[3]);
        Engine::getInstance()->fillColor({ 0, 0, 128 }, x, y, w, h);
        return {};
    });

    cifa_.register_function("drawstring", [](cifa::ObjectVector& args) -> cifa::Object
    {
        if (args.size() < 3) { return {}; }
        std::string str = std::string(args[0]);
        int x = int(args[1]), y = int(args[2]);
        Font::getInstance()->draw(str, 20, x, y, { 255, 255, 255, 255 });
        return {};
    });

    cifa_.register_function("gettalk", [](cifa::ObjectVector& args) -> cifa::Object
    {
        int index = args.empty() ? 0 : int(args[0]);
        std::string str = Event::getInstance()->getTalkContent(index);
        return cifa::Object(str);
    });

    cifa_.register_function("getitemamount", [](cifa::ObjectVector& args) -> cifa::Object
    {
        int id = args.empty() ? 0 : int(args[0]);
        return cifa::Object(double(Save::getInstance()->getItemCountInBag(id)));
    });

    return 0;
}
