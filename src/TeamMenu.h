#pragma once
#include "Menu.h"
#include "Head.h"

//用于选择队伍中的角色，可以传入一个item作为过滤，为空会显示所有人
class TeamMenu : public Menu
{
public:
    TeamMenu();
    ~TeamMenu();

    std::vector<Head*> heads_;

    std::vector<int> selected_;

    Item* item = nullptr;

    virtual void onEntrance() override;

    int mode_;   //为0是单选，为1是多选

};

