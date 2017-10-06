#include "BattleScene.h"
#include "MainScene.h"
#include <iostream>
#include <string>
#include <math.h>
#include <algorithm>
#include "Save.h"
#include "Menu.h"
#include "others/libconvert.h"
#include "File.h"
#include "GameUtil.h"
#include "Random.h"
#include "UIStatus.h"
#include "Font.h"

BattleScene::BattleScene()
{
    full_window_ = 1;
    earth_layer_.resize(COORD_COUNT);
    building_layer_.resize(COORD_COUNT);
    role_layer_.resize(COORD_COUNT);
    select_layer_.resize(COORD_COUNT);
    effect_layer_.resize(COORD_COUNT);
    battle_menu_ = new BattleMenu();
    battle_menu_->setBattleScene(this);
    battle_menu_->setPosition(160, 200);
    head_self_ = new Head();
    addChild(head_self_);
    ui_status_ = new UIStatus();
    ui_status_->setVisible(false);
    addChild(ui_status_, 300, 0);
    battle_cursor_ = new BattleCursor();
    battle_cursor_->setBattleScene(this);
    //battle_operator_->setOperator(&select_x_, &select_y_, &select_layer_, &effect_layer_);
    save_ = Save::getInstance();
}

BattleScene::BattleScene(int id) : BattleScene()
{
    setID(id);
}

BattleScene::~BattleScene()
{
    delete battle_menu_;
    delete battle_cursor_;
}

void BattleScene::setID(int id)
{
    battle_id_ = id;
    info_ = BattleMap::getInstance()->getBattleInfo(id);

    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 0, &earth_layer_.data(0));
    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 1, &building_layer_.data(0));

    role_layer_.setAll(-1);
    select_layer_.setAll(-1);
    effect_layer_.setAll(-1);
}

void BattleScene::draw()
{
    auto r0 = battle_roles_[0];  //当前正在行动中的角色
    Engine::getInstance()->setRenderAssistTexture();
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, render_center_x_ * 2, render_center_y_ * 2);
#ifndef _DEBUG0
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int i1 = man_x_ + i + (sum / 2);
            int i2 = man_y_ - i + (sum - sum / 2);
            auto p = getPositionOnRender(i1, i2, man_x_, man_y_);
            p.x += x_;
            p.y += y_;
            if (!isOutLine(i1, i2))
            {
                int num = earth_layer_.data(i1, i2) / 2;
                BP_Color color = { 255, 255, 255, 255 };
                if (battle_cursor_->isRunning())
                {
                    if (select_layer_.data(i1, i2) < 0)
                    {
                        color = { 64, 64, 64, 255 };
                    }
                    else
                    {
                        color = { 128, 128, 128, 255 };
                    }
                    if (battle_cursor_->getMode() == BattleCursor::Action)
                    {
                        if (effect_layer_.data(i1, i2) >= 0)
                        {
                            if (select_layer_.data(i1, i2) < 0)
                            {
                                color = { 160, 160, 160, 255 };
                            }
                            else
                            {
                                color = { 192, 192, 192, 255 };
                            }
                        }
                    }
                    if (i1 == select_x_ && i2 == select_y_)
                    {
                        color = { 255, 255, 255, 255 };
                    }
                }

                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y, color);
                }
            }
        }
    }
#endif
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int i1 = man_x_ + i + (sum / 2);
            int i2 = man_y_ - i + (sum - sum / 2);
            auto p = getPositionOnRender(i1, i2, man_x_, man_y_);
            p.x += x_;
            p.y += y_;
            if (!isOutLine(i1, i2))
            {
                int num = building_layer_.data(i1, i2) / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                }
                num = role_layer_.data(i1, i2);
                if (num >= 0)
                {
                    auto r = Save::getInstance()->getRole(num);
                    std::string path = convert::formatString("fight/fight%03d", r->HeadID);
                    BP_Color color = { 255, 255, 255, 255 };
                    if (battle_cursor_->isRunning())
                    {
                        color = { 128, 128, 128, 255 };
                        if (effect_layer_.data(i1, i2) >= 0)
                        {
                            if (r0->ActTeam == 0 && battle_roles_[0]->Team == r->Team)
                            {
                                color = { 255, 255, 255, 255 };
                            }
                            else if (r0->ActTeam != 0 && battle_roles_[0]->Team != r->Team)
                            {
                                uint8_t r = uint8_t(127 + RandomClassical::rand(128));
                                color = { 255, 255, 255, 255 };
                            }
                        }
                    }
                    int pic;
                    if (r == r0)
                    {
                        pic = calRolePic(r, action_type_, action_frame_);
                    }
                    else
                    {
                        pic = calRolePic(r);
                    }
                    TextureManager::getInstance()->renderTexture(path, pic, p.x, p.y, color);
                }
                if (effect_id_ >= 0 && effect_layer_.data(i1, i2) >= 0)
                {
                    std::string path = convert::formatString("eft/eft%03d", effect_id_);
                    num = effect_frame_ + RandomClassical::rand(10) - RandomClassical::rand(10);
                    TextureManager::getInstance()->renderTexture(path, num, p.x, p.y, { 255, 255, 255, 255 }, 224);
                }
            }
        }
    }
    Engine::getInstance()->renderAssistTextureToWindow();

    //ui的设定
    if (ui_status_->getVisible())
    {
        int r = role_layer_.data(select_x_, select_y_);
        ui_status_->setRole(Save::getInstance()->getRole(r));
    }
}

void BattleScene::dealEvent(BP_Event& e)
{
    //选择位于人物数组中的第一个人
    auto r = battle_roles_[0];
    man_x_ = r->X();
    man_y_ = r->Y();
    select_x_ = r->X();
    select_y_ = r->Y();
    head_self_->setRole(r);
    head_self_->setState(Element::Pass);

    int select_act = 0;

    //注意这里只判断是否为自动？
    if (r->Auto == 0)
    {
        select_act = battle_menu_->runAsRole(r);
    }
    else
    {
        //此处应写AI部分？
    }

    switch (select_act)
    {
    case 0: actMove(r); break;
    case 1: actUseMagic(r); break;
    case 2: actUsePoison(r); break;
    case 3: actDetoxification(r); break;
    case 4: actMedcine(r); break;
    case 5: actUseItem(r); break;
    case 6: actWait(r); break;
    case 7: actStatus(r); break;
    case 8: actAuto(r); break;
    case 9: actRest(r); break;
    default:
        //默认值为什么都不做
        break;
    }

    //如果此人成功行动过，则清除行动状态，放到队尾
    if (r->Acted)
    {
        r->Acted = 0;
        r->Moved = 0;
        battle_roles_.erase(battle_roles_.begin());
        battle_roles_.push_back(r);
    }
    //清除被击退的人物
    clearDead();
}

void BattleScene::onEntrance()
{
    calViewRegion();

    //注意此时才能得到窗口的大小，用来设置头像的位置
    head_self_->setPosition(100, 100);

    //设置全部角色的位置层，避免今后出错
    for (auto& r : Save::getInstance()->roles_)
    {
        r.setPoitionLayer(&role_layer_);
        r.Team = 2;  //先全部设置成不存在的阵营
        r.Auto = 1;
    }

    //首先设置位置和阵营，其他的后面统一处理
    //队友
    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        auto r = Save::getInstance()->getRole(Save::getInstance()->Team[i]);
        if (r)
        {
            battle_roles_.push_back(r);
            r->setPosition(info_->TeamMateX[i], info_->TeamMateY[i]);
            r->Team = 0;
            r->Auto = 0;
        }
    }
    //敌方
    for (int i = 0; i < BATTLE_ENEMY_COUNT; i++)
    {
        auto r = Save::getInstance()->getRole(info_->Enemy[i]);
        if (r)
        {
            battle_roles_.push_back(r);
            r->setPosition(info_->EnemyX[i], info_->EnemyY[i]);
            r->Team = 1;
            r->Auto = 1;
        }
    }

    //初始状态
    for (auto r : battle_roles_)
    {
        setRoleInitState(r);
    }
    //排序
    sortRoles();
}

void BattleScene::onExit()
{
    //清空全部角色的位置层
    for (auto& r : Save::getInstance()->roles_)
    {
        r.setPoitionLayer(nullptr);
    }
}

void BattleScene::setRoleInitState(Role* r)
{
    r->Acted = 0;
    r->ExpGot = 0;
    r->ShowString = "";
    r->FightingFrame = 0;
    r->Auto = 0;

    //读取动作帧数
    bool frame_readed = false;
    //注意这个判断不太准，应该在构造函数里面设一个不合理的初值
    /*for (int i = 0; i < 5; i++)
    {
        if (r->FightFrame[i] > 0)
        {
            frame_readed = true;
            break;
        }
    }*/
    if (!frame_readed)
    {
        readFightFrame(r);
    }

    //寻找离自己最近的敌方，设置面向
    int min_distance = COORD_COUNT * COORD_COUNT;
    Role* r_near;
    for (auto r1 : battle_roles_)
    {
        if (r->Team != r1->Team)
        {
            int dis = calDistance(r, r1);
            if (dis < min_distance)
            {
                r_near = r1;
                min_distance = dis;
            }
        }
    }

    r->Face = (int)calFace(r->X(), r->Y(), r_near->X(), r_near->Y());
    r->Face = RandomClassical::rand(4);  //没头苍蝇随意选择面向
}

void BattleScene::readFightFrame(Role* r)
{
    for (int i = 0; i < 5; i++)
    {
        r->FightFrame[i] = 0;
    }
    std::string file = convert::formatString("../game/resource/fight/fight%03d/fightframe.txt", r->HeadID);
    std::string frame_txt = convert::readStringFromFile(file);
    std::vector<int> frames;
    convert::findNumbers(frame_txt, &frames);
    for (int i = 0; i < frames.size() / 2; i++)
    {
        r->FightFrame[frames[i * 2]] = frames[i * 2 + 1];
    }
}

//角色排序
void BattleScene::sortRoles()
{
    std::sort(battle_roles_.begin(), battle_roles_.end(), compareRole);
}

//角色排序的规则
bool BattleScene::compareRole(Role* r1, Role* r2)
{
    return r1->Speed > r2->Speed;
}

//计算可移动步数(考虑装备)
int BattleScene::calMoveStep(Role* r)
{
    int speed = r->Speed;
    if (r->Equip1 >= 0)
    {
        auto i = Save::getInstance()->getItem(r->Equip1);
        speed += i->AddSpeed;
    }
    if (r->Equip2 >= 0)
    {
        auto i = Save::getInstance()->getItem(r->Equip2);
        speed += i->AddSpeed;
    }
    return speed / 15 + 1;
}

//依据动作帧数计算角色的贴图编号
int BattleScene::calRolePic(Role* r, int style, int frame)
{
    if (r->FightFrame[style] <= 0)
    {
        style = -1;
    }
    if (style == -1)
    {
        for (int i = 0; i < 5; i++)
        {
            if (r->FightFrame[i] > 0)
            {
                return r->FightFrame[i] * r->Face;
            }
        }
    }
    else
    {
        int total = 0;
        for (int i = 0; i < 5; i++)
        {
            if (i == style)
            {
                return total + r->FightFrame[style] * r->Face + frame;
            }
            total += r->FightFrame[i] * 4;
        }
    }
    return r->Face;
}

//计算可以被选择的范围，会改写选择层
//mode含义：0-移动，受步数和障碍影响；1攻击用毒医疗等仅受步数影响；2查看状态，全都能选；3仅能选直线的格子
void BattleScene::calSelectLayer(Role* r, int mode, int step)
{
    if (mode == 0)
    {
        select_layer_.setAll(-1);
        std::vector<Point> cal_stack;
        cal_stack.push_back({ r->X(), r->Y() });
        int count = 0;
        while (step >= 0)
        {
            std::vector<Point> cal_stack_next;
            for (auto p : cal_stack)
            {
                select_layer_.data(p.x, p.y) = step;
                auto check_next = [&](Point p1)->void
                {
                    //未计算过且可以走的格子参与下一步的计算
                    if (canWalk(p1.x, p1.y) && select_layer_.data(p1.x, p1.y) == -1)
                    {
                        cal_stack_next.push_back(p1);
                        count++;
                    }
                };
                //检测是否在敌方身旁，视情况打开此选项
                if (!isNearEnemy(r, p.x, p.y))
                {
                    //检测4个相邻点
                    check_next({ p.x - 1, p.y });
                    check_next({ p.x + 1, p.y });
                    check_next({ p.x, p.y - 1 });
                    check_next({ p.x, p.y + 1 });
                }
                if (count >= COORD_COUNT * COORD_COUNT) { break; }  //最多计算次数，避免死掉
            }
            cal_stack = cal_stack_next;
            step--;
        }
    }
    else if (mode == 1)
    {
        for (int ix = 0; ix < COORD_COUNT; ix++)
        {
            for (int iy = 0; iy < COORD_COUNT; iy++)
            {
                select_layer_.data(ix, iy) = step - calDistance(ix, iy, r->X(), r->Y());
            }
        }
    }
    else if (mode == 2)
    {
        select_layer_.setAll(0);
    }
    else if (mode == 3)
    {
        for (int ix = 0; ix < COORD_COUNT; ix++)
        {
            for (int iy = 0; iy < COORD_COUNT; iy++)
            {
                int dx = abs(ix - r->X());
                int dy = abs(iy - r->Y());
                if (dx == 0 && dy <= step || dy == 0 && dx <= step)
                {
                    select_layer_.data(ix, iy) = 0;
                }
                else
                {
                    select_layer_.data(ix, iy) = -1;
                }
            }
        }
    }
}

void BattleScene::calEffectLayer(Role* r, Magic* m, int level_index)
{
    effect_layer_.setAll(-1);

    //若未指定武学，则认为只选择一个点
    if (m == nullptr)
    {
        effect_layer_.data(select_x_, select_y_) = 0;
        return;
    }

    if (m->AttackAreaType == 1 || m->AttackAreaType == 3)
    {
        int x = select_x_, y = select_y_;
        //effect_layer_.setAll(-1);
        int dis = m->AttackDistance[Save::getInstance()->getRoleLearnedMagicLevelIndex(r, m)];
        for (int ix = x - dis; ix <= x + dis; ix++)
        {
            for (int iy = y - dis; iy <= y + dis; iy++)
            {
                if (!isOutLine(ix, iy))
                {
                    effect_layer_.data(ix, iy) = 0;
                }
            }
        }
    }
}

bool BattleScene::canSelect(int x, int y)
{
    return (!isOutLine(x, y) && select_layer_.data(x, y) >= 0);
}

void BattleScene::walk(Role* r, int x, int y, Towards t)
{
    if (canWalk(x, y))
    {
        r->setPosition(x, y);
    }
}

bool BattleScene::canWalk(int x, int y)
{
    if (isOutLine(x, y) || isBuilding(x, y) || isWater(x, y) || isRole(x, y))
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool BattleScene::isBuilding(int x, int y)
{
    return building_layer_.data(x, y) > 0;
}

bool BattleScene::isOutLine(int x, int y)
{
    return (x < 0 || x >= COORD_COUNT || y < 0 || y >= COORD_COUNT);
}

bool BattleScene::isWater(int x, int y)
{
    int num = earth_layer_.data(x, y) / 2;
    if (num >= 179 && num <= 181
        || num == 261 || num == 511
        || num >= 662 && num <= 665
        || num == 674)
    {
        return true;
    }
    return false;
}

bool BattleScene::isRole(int x, int y)
{
    return role_layer_.data(x, y) > 0;
}

bool BattleScene::isOutScreen(int x, int y)
{
    return (abs(man_x_ - x) >= 16 || abs(man_y_ - y) >= 20);
}

bool BattleScene::isNearEnemy(Role* r, int x, int y)
{
    for (auto r1 : battle_roles_)
    {
        if (r->Team != r1->Team && calDistance(r1->X(), r1->Y(), x, y) <= 1)
        {
            return true;
        }
    }
    return false;
}

void BattleScene::actMove(Role* r)
{
    int step = calMoveStep(r);
    calSelectLayer(r, 0, step);
    battle_cursor_->setRoleAndMagic(r);
    battle_cursor_->setMode(BattleCursor::Move);
    if (battle_cursor_->run() == 0)
    {
        r->setPrevPosition(r->X(), r->Y());
        moveAnimation(r, select_x_, select_y_);
        r->Moved = 1;
    }
}

void BattleScene::actUseMagic(Role* r)
{
    auto magic_menu = new MenuText();
    std::vector<std::string> magic_names;
    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        auto m = Save::getInstance()->getRoleLearnedMagic(r, i);
        if (m)
        {
            magic_names.push_back(convert::formatString("%s%7d", m->Name, r->MagicLevel[i]));
        }
    }
    magic_menu->setStrings(magic_names);
    magic_menu->setPosition(160, 200);

    while (true)
    {
        int select_magic = magic_menu->run();
        if (select_magic < 0) { break; }

        r->ActTeam = 1;
        auto magic = Save::getInstance()->getRoleLearnedMagic(r, select_magic);
        //level_index表示从0到9，而level从0到999
        int level_index = r->getMagicLevelIndex(select_magic);
        //计算可选择的范围
        if (magic->AttackAreaType == 0 || magic->AttackAreaType == 3)
        {
            calSelectLayer(r, 1, magic->SelectDistance[level_index]);
        }
        else
        {
            calSelectLayer(r, 3, magic->SelectDistance[level_index]);
        }
        //选择目标
        battle_cursor_->setMode(BattleCursor::Action);
        battle_cursor_->setRoleAndMagic(r, magic, level_index);
        calEffectLayer(r, magic, level_index);
        int selected = battle_cursor_->run();
        //取消选择目标则重新进入选武功
        if (selected < 0)
        {
            continue;
        }
        else
        {
            //播放攻击画面，计算伤害
            useMagicAnimation(r, magic);
            calAllHurt(r, magic);
            showNumberAnimation();
            r->Acted = 1;
            break;
        }
    }
    delete magic_menu;
}

void BattleScene::actUsePoison(Role* r)
{
    calSelectLayer(r, 1, r->UsePoison / 15 + 1);
    battle_cursor_->setMode(BattleCursor::Action);
    battle_cursor_->setRoleAndMagic(r);
    r->ActTeam = 1;
    calEffectLayer(r);
    int selected = battle_cursor_->run();
    if (selected >= 0)
    {
        actionAnimation(r, 0, 30);
        showNumberAnimation();
        r->Acted = 1;
    }
}

void BattleScene::actDetoxification(Role* r)
{
    calSelectLayer(r, 1, r->Detoxification / 15 + 1);
    battle_cursor_->setMode(BattleCursor::Action);
    battle_cursor_->setRoleAndMagic(r);
    r->ActTeam = 0;
    calEffectLayer(r);
    int selected = battle_cursor_->run();
    if (selected >= 0)
    {
        actionAnimation(r, 0, 36);
        showNumberAnimation();
        r->Acted = 1;
    }
}

void BattleScene::actMedcine(Role* r)
{
    calSelectLayer(r, 1, r->Medcine / 15 + 1);
    battle_cursor_->setMode(BattleCursor::Action);
    battle_cursor_->setRoleAndMagic(r);
    r->ActTeam = 0;
    calEffectLayer(r);
    int selected = battle_cursor_->run();
    if (selected >= 0)
    {
        actionAnimation(r, 0, 0);
        showNumberAnimation();
        r->Acted = 1;
    }
}

void BattleScene::actUseItem(Role* r)
{

}

void BattleScene::actWait(Role* r)
{
    battle_roles_.erase(battle_roles_.begin());
    battle_roles_.push_back(r);
}

void BattleScene::actStatus(Role* r)
{
    head_self_->setVisible(false);
    ui_status_->setVisible(true);
    battle_cursor_->getHead()->setVisible(false);

    calSelectLayer(r, 2, 0);
    battle_cursor_->setRoleAndMagic(r);
    battle_cursor_->setMode(BattleCursor::Check);
    battle_cursor_->run();

    head_self_->setVisible(true);
    ui_status_->setVisible(false);
    battle_cursor_->getHead()->setVisible(true);
}

void BattleScene::actAuto(Role* r)
{

}

void BattleScene::actRest(Role* r)
{
    r->PhysicalPower = GameUtil::limit(r->PhysicalPower + 5, 0, MAX_PHYSICAL_POWER);
    r->HP = GameUtil::limit(r->HP + 0.05 * r->MaxHP, 0, MAX_HP);
    r->MP = GameUtil::limit(r->MP + 0.05 * r->MaxMP, 0, MAX_MP);
    r->Acted = 1;
}

//移动动画
void BattleScene::moveAnimation(Role* r, int x, int y)
{
    //从目标往回找确定路线
    std::vector<Point> way;
    auto check_next = [&](Point p1, int step)->bool
    {
        if (canSelect(p1.x, p1.y) && select_layer_.data(p1.x, p1.y) == step)
        {
            way.push_back(p1);
            return true;
        }
        return false;
    };

    way.push_back({ x, y });
    for (int i = select_layer_.data(x, y); i < select_layer_.data(r->X(), r->Y()); i++)
    {
        int x1 = way.back().x, y1 = way.back().y;
        if (check_next({ x1 - 1, y1 }, i + 1)) { continue; }
        if (check_next({ x1 + 1, y1 }, i + 1)) { continue; }
        if (check_next({ x1, y1 - 1 }, i + 1)) { continue; }
        if (check_next({ x1, y1 + 1 }, i + 1)) { continue; }
    }

    for (int i = way.size() - 1; i >= 0; i--)
    {
        r->Face = calFace(r->X(), r->Y(), way[i].x, way[i].y);
        r->setPosition(way[i].x, way[i].y);
        drawAll();
        checkEventAndPresent(100);
    }
    r->setPosition(x, y);
    r->Moved = 1;
    select_layer_.setAll(-1);
}

//使用武学动画
void BattleScene::useMagicAnimation(Role* r, Magic* m)
{
    if (r && m)
    {
        actionAnimation(r, m->MagicType, m->EffectID);
    }
}

void BattleScene::actionAnimation(Role* r, int action_type, int effect_id)
{
    if (r->X() != select_x_ || r->Y() != select_y_)
    {
        r->Face = calFace(r->X(), r->Y(), select_x_, select_y_);
    }
    auto frame_count = r->FightFrame[action_type];
    action_type_ = action_type;
    for (action_frame_ = 0; action_frame_ < frame_count; action_frame_++)
    {
        drawAll();
        checkEventAndPresent(25);
    }
    action_frame_ = frame_count - 1;
    effect_id_ = effect_id;
    auto path = convert::formatString("eft/eft%03d", effect_id_);
    auto effect_count = TextureManager::getInstance()->getTextureGroupCount(path);
    for (effect_frame_ = 0; effect_frame_ < effect_count + 10; effect_frame_++)
    {
        x_ = RandomClassical::rand(5) - RandomClassical::rand(5);
        y_ = RandomClassical::rand(5) - RandomClassical::rand(5);
        drawAll();
        checkEventAndPresent(25);
    }
    action_frame_ = 0;
    action_type_ = -1;
    effect_frame_ = 0;
    effect_id_ = -1;
    x_ = 0;
    y_ = 0;
}

//r1使用武功magic攻击r2的伤害
int BattleScene::calHurt(Role* r1, Role* r2, Magic* magic)
{
    int level_index = Save::getInstance()->getRoleLearnedMagicLevelIndex(r1, magic);
    int v = r1->Attack - r2->Defence + magic->Attack[level_index] / 3;
    v += RandomClassical::rand(10) - RandomClassical::rand(10);
    return v;
}

//计算全部人物的伤害
int BattleScene::calAllHurt(Role* r, Magic* m)
{
    int total = 0;
    for (auto r1 : battle_roles_)
    {
        if (r1->Team != r->Team)
        {
            int hurt = calHurt(r, r1, m);
            r1->ShowString = convert::formatString("-%d", hurt);
            r1->ShowColor = { 255, 20, 20, 255 };
            r1->HP -= hurt;
            total += hurt;
        }
    }
    return total;
}

void BattleScene::showNumberAnimation()
{
    int size = 28;
    for (int i = 0; i <= 10; i++)
    {
        drawAll();
        for (auto r : battle_roles_)
        {
            //等于0不显示个（待定）
            if (r->ShowString.size() > 0)
            {
                auto p = getPositionOnWindow(r->X(), r->Y(), man_x_, man_y_);
                int x = p.x - size * r->ShowString.size() / 4;
                int y = p.y - 75 - i * 2;
                Font::getInstance()->draw(r->ShowString, size, x, y, r->ShowColor, 255 - 20 * i);
            }
        }
        checkEventAndPresent(50);
    }
    //清除所有人的显示
    for (auto r : battle_roles_)
    {
        r->ShowString = "";
    }
}

void BattleScene::clearDead()
{
    std::vector<Role*> alive;
    for (auto r : battle_roles_)
    {
        if (r->HP > 0)
        {
            alive.push_back(r);
        }
        else
        {
            r->setPosition(-1, -1);
        }
    }
    //如需要退场动画放在这里
    battle_roles_ = alive;
}

