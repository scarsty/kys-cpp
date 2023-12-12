#pragma once
#include "BattleScene.h"
#include "Font.h"
#include "Head.h"
#include "Save.h"
#include "UIKeyConfig.h"
#include <deque>
#include <unordered_map>

class BattleSceneSekiro : public BattleScene
{
public:
    BattleSceneSekiro();
    virtual void draw() override;

protected:
    Pointf pos_;    //坐标为俯视，而非在画面的位置，其中y需除以2画在上面
    double gravity_ = -4;
    double friction_ = 0.1;

    std::deque<Role*> enemies_;
    std::deque<Role> enemies_obj_;

    std::vector<std::shared_ptr<Head>> heads_;
    std::vector<std::shared_ptr<Head>> head_boss_;

    bool is_running_ = false;    //主角是否在跑动
    Role* role_ = nullptr;
};
