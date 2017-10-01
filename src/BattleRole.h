#pragma once
#include "Types.h"

class BattleRole
{
public:
    BattleRole();
    ~BattleRole();

    int RoleID, Team;
private:
    int X_, Y_;
public:
    int Face, Dead, Step, Acted;
    int Pic, ShowNumber, showgongji, showfangyu, szhaoshi, Progress, round, speed; //15
    int ExpGot, Auto, Show, Wait, frozen, killed, Knowledge, LifeAdd, Zhuanzhu, pozhao, wanfang; //24
    Role* getRole() { return nullptr; }
    void setPosition(int x, int y) {}
    int X() { return X_; }
    int Y() { return Y_; }
private:
    //BattleMapArray* role_layer_;
};

