#pragma once
#include "FunctionTrait.h"
#include "Menu.h"
#include "Random.h"
#include "RunNode.h"
#include "SubScene.h"
#include "Talk.h"

//event_id表示在kdef中的编号，event_index表示在场景中的编号

class Event
{
private:
    Event();
    virtual ~Event();

public:
    static Event* getInstance()
    {
        static Event e;
        return &e;
    }

private:
    std::vector<int> offset, length;
    std::vector<std::string> talk_contents_;
    std::vector<std::vector<int>> kdef_;

    int leave_event_0_;
    std::vector<int> leave_event_id_;

    //两个对话，用于上面和下面，两个可以同时显示
    //视需要可增加更多
    std::shared_ptr<RunNode> talk_box_;
    std::shared_ptr<Talk> talk_box_up_, talk_box_down_;

    //专用于显示确认和取消选项
    std::shared_ptr<MenuText> menu2_;
    //专用于显示一个文本框
    std::shared_ptr<TextBox> text_box_;
    int event_id_ = -1;

    RandomDouble rand_;

public:
    bool loadEventData();    //加载事件数据
    //这里再设计
    bool callEvent(int event_id, RunNode* subscene = nullptr, int supmap_id = -1, int item_id = -1, int event_index = -1, int x = -1, int y = -1);    //调用指令的内容写这里

private:
    SubScene* subscene_;
    int submap_id_;
    int x_, y_;
    int event_index_;
    int item_id_;
    Item* item_;
    //Save* save_;
    bool exit_ = false;
    int use_script_ = 0;

private:
    SubMapInfo* getSubMapRecordFromID(int submap_id);

public:
    int getLeaveEvent(Role* role);
    void callLeaveEvent(Role* role);
    void forceExit();
    void setUseScript(int u);
    bool isExiting() { return exit_; }

public:
    //以下大部分参数为int，请注意游戏数据中使用的是int16_t，有降低效率的可能
    //void clear() {}
    void oldTalk(int talk_id, int head_id, int style);
    void addItem(int item_id, int count);
    void modifyEvent(int submap_id, int event_index, int cannotWalk, int Num, int Event1, int Event2, int Event3,
        int BeginPic1, int EndPic, int BeginPic2, int PicDelay, int x, int y);
    bool isUsingItem(int item_id);
    bool askBattle();
    bool tryBattle(int battle_id, int get_exp);
    void changeMainMapMusic(int music_id);
    bool askJoin();
    void join(int role_id);
    bool askRest();
    void rest();
    void lightScence();
    void darkScence();
    void dead();
    bool inTeam(int role_id);
    void setSubMapLayerData(int submap_id, int layer, int x, int y, int v);
    bool haveItemBool(int item_id);
    void oldSetScencePosition(int x, int y);
    bool teamIsFull();
    void leaveTeam(int role_id);
    void zeroAllMP();
    void setRoleUsePoison(int role_id, int v);
    //void blank() {}
    void subMapViewFromTo(int x0, int y0, int x1, int y1);
    void add3EventNum(int submap_id, int event_index, int v1, int v2, int v3);
    void playAnimation(int event_index, int begin_pic, int end_pic);
    bool checkRoleMorality(int role_id, int low, int high);
    bool checkRoleAttack(int role_id, int low, int high);
    void walkFromTo(int x0, int y0, int x1, int y1);
    bool checkEnoughMoney(int money_count);
    void addItemWithoutHint(int item_id, int count);
    void oldLearnMagic(int role_id, int magic_id, int no_display);
    void addIQ(int role_id, int value);
    void setRoleMagic(int role_id, int magic_index_role, int magic_id, int level);
    bool checkRoleSexual(int sexual);
    void addMorality(int value);
    void changeSubMapPic(int submap_id, int layer, int old_pic, int new_pic);
    void openSubMap(int submap_id);
    void setTowards(int towards);
    void roleAddItem(int role_id, int item_id, int count);
    bool checkFemaleInTeam();
    void play2Amination(int event_index1, int begin_pic1, int end_pic1, int event_index2, int begin_pic2, int end_pic2);
    void play3Amination(int event_index1, int begin_pic1, int end_pic1, int event_index2, int begin_pic2, int event_index3, int begin_pic3);
    void addSpeed(int role_id, int value);
    void addMaxMP(int role_id, int value);
    void addAttack(int role_id, int value);
    void addMaxHP(int role_id, int value);
    void setMPType(int role_id, int value);
    bool checkHave5Item(int item_id1, int item_id2, int item_id3, int item_id4, int item_id5);
    void askSoftStar();
    void showMorality();
    void showFame();
    void openAllSubMap();
    bool checkEventID(int event_index, int value);
    void addFame(int value);
    void breakStoneGate();
    void fightForTop();
    void allLeave();
    bool checkSubMapPic(int submap_id, int event_index, int pic);
    bool check14BooksPlaced();
    void backHome(int event_index1, int begin_pic1, int end_pic1, int event_index2, int begin_pic2, int end_pic2);
    void setSexual(int role_id, int value);
    void shop();
    void playMusic(int music_id);
    void playWave(int wave_id);

    void arrangeBag();
    void clearTalkBox();
    void blank() {}

private:
    int x50[0x10000];

public:
    int e_GetValue(int bit, int t, int x)
    {
        int i = t & (1 << bit);
        if (i == 0)
        {
            return x;
        }
        else
        {
            return x50[x];
        }
    }

    //扩展的50指令，传入下一个指令的指针，某一条需要
    void instruct_50e(int code, int e1, int e2, int e3, int e4, int e5, int e6, int* code_ptr = nullptr);

public:
    void print_e(const std::vector<int>& e, int i, int size)
    {
        auto v = std::vector<int>(e.begin() + i, e.begin() + i + size);
        fmt::print("{}\n", v);
    }

    template <typename F, typename C, std::size_t... I>
    typename std::enable_if<check_return_type<F, C, void>::value, void>::type
    runner_impl(F f, C* c, const std::vector<int>& e, int& i, std::index_sequence<I...>)
    {
        auto cur_i = i;
        i += sizeof...(I) + 1;
        print_e(e, cur_i + 1, sizeof...(I));
        (c->*f)(e[I + cur_i + 1]...);
    }

    template <typename F, typename C, std::size_t... I>
    typename std::enable_if<check_return_type<F, C, bool>::value, void>::type
    runner_impl(F f, C* c, const std::vector<int>& e, int& i, std::index_sequence<I...>)
    {
        auto cur_i = i;
        i += sizeof...(I) + 1;
        print_e(e, cur_i + 1, sizeof...(I) + 2);
        if ((c->*f)(e[I + cur_i + 1]...))
        {
            i += e[i];
        }
        else
        {
            i += e[i + 1];
        }
        i += 2;
    }

    template <typename F, typename C>
    void runner(F f, C* c, const std::vector<int>& e, int& i)
    {
        runner_impl(f, c, e, i, std::make_index_sequence<arg_counter<F, C>::value>{});
    }
};
