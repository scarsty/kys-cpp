#include "BattleScene.h"
#include "BattleNetwork.h"    //必须在Audio之前

#include "Audio.h"
#include "DrawableOnCall.h"
#include "Event.h"
#include "File.h"
#include "Font.h"
#include "GameUtil.h"
#include "MainScene.h"
#include "Menu.h"
#include "PotConv.h"
#include "Random.h"
#include "Save.h"
#include "ShowExp.h"
#include "ShowRoleDifference.h"
#include "TeamMenu.h"
#include "UIStatus.h"
#include "Util.h"
#include <algorithm>
#include <iostream>
#include <math.h>
#include <string>

RandomDouble BattleScene::rng_;

BattleScene::BattleScene()
{
    full_window_ = 1;
    COORD_COUNT = BATTLEMAP_COORD_COUNT;

    earth_layer_ = new MapSquareInt(COORD_COUNT);
    building_layer_ = new MapSquareInt(COORD_COUNT);
    select_layer_ = new MapSquareInt(COORD_COUNT);
    effect_layer_ = new MapSquareInt(COORD_COUNT);
    battle_menu_ = new BattleActionMenu();

    role_layer_ = new MapSquare<Role*>(COORD_COUNT);

    battle_menu_->setBattleScene(this);
    battle_menu_->setPosition(160, 200);
    head_self_ = new Head();
    addChild(head_self_);
    battle_cursor_ = new BattleCursor();
    battle_cursor_->setBattleScene(this);
    save_ = Save::getInstance();
    semi_real_ = GameUtil::getInstance()->getInt("game", "semi_real", 0);
}

BattleScene::BattleScene(int id)
    : BattleScene()
{
    setID(id);
}

BattleScene::~BattleScene()
{
    delete battle_menu_;
    delete battle_cursor_;
    Util::safe_delete({ &earth_layer_, &building_layer_, &select_layer_, &effect_layer_ });
    delete role_layer_;
}

void BattleScene::setID(int id)
{
    battle_id_ = id;
    info_ = BattleMap::getInstance()->getBattleInfo(id);

    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 0, earth_layer_);
    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 1, building_layer_);

    role_layer_->setAll(nullptr);
    select_layer_->setAll(-1);
    effect_layer_->setAll(-1);
}

void BattleScene::draw()
{
    Engine::getInstance()->setRenderAssistTexture();
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, render_center_x_ * 2, render_center_y_ * 2);

    //地面是否需要亮度的变化，自动人物或者选择位置部分没有运行
    bool need_change_earth_color_ = battle_cursor_->isRunning() && !acting_role_->isAuto();

    //一整块地面
    if (earth_texture_)
    {
        BP_Color c = { 255, 255, 255, 255 };
        if (need_change_earth_color_)
        {
            c = { 64, 64, 64, 255 };    //如果地面需要亮度变化，则以画最暗的为主
        }
        Engine::getInstance()->setColor(earth_texture_, c);
        auto p = getPositionOnWholeEarth(man_x_, man_y_);
        int w = render_center_x_ * 2;
        int h = render_center_y_ * 2;
        //获取的是中心位置，如贴图应减掉屏幕尺寸的一半
        BP_Rect rect0 = { p.x - render_center_x_ - x_, p.y - render_center_y_ - y_, w, h }, rect1 = { 0, 0, w, h };
        Engine::getInstance()->renderCopy(earth_texture_, &rect0, &rect1, 1);
    }

#ifndef _DEBUG
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int ix = man_x_ + i + (sum / 2);
            int iy = man_y_ - i + (sum - sum / 2);
            auto p = getPositionOnRender(ix, iy, man_x_, man_y_);
            p.x += x_;
            p.y += y_;
            if (!isOutLine(ix, iy))
            {
                int num = earth_layer_->data(ix, iy) / 2;
                BP_Color color = { 255, 255, 255, 255 };
                bool need_draw = true;
                if (need_change_earth_color_)
                {
                    if (select_layer_->data(ix, iy) < 0)
                    {
                        color = { 64, 64, 64, 255 };
                        need_draw = earth_texture_ == nullptr;
                    }
                    else
                    {
                        color = { 128, 128, 128, 255 };
                    }
                    if (battle_cursor_->getMode() == BattleCursor::Action)
                    {
                        if (haveEffect(ix, iy))
                        {
                            if (!canSelect(ix, iy))
                            {
                                color = { 160, 160, 160, 255 };
                            }
                            else
                            {
                                color = { 192, 192, 192, 255 };
                            }
                        }
                    }
                    if (ix == select_x_ && iy == select_y_)
                    {
                        color = { 255, 255, 255, 255 };
                    }
                }
                if (need_draw && num > 0)
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
            int ix = man_x_ + i + (sum / 2);
            int iy = man_y_ - i + (sum - sum / 2);
            auto p = getPositionOnRender(ix, iy, man_x_, man_y_);
            p.x += x_;
            p.y += y_;
            if (!isOutLine(ix, iy))
            {
                int num = building_layer_->data(ix, iy) / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                }
                auto r = role_layer_->data(ix, iy);
                if (r)
                {
                    std::string path = convert::formatString("fight/fight%03d", r->HeadID);
                    BP_Color color = { 255, 255, 255, 255 };
                    uint8_t alpha = 255;
                    if (battle_cursor_->isRunning() && !acting_role_->isAuto())
                    {
                        color = { 128, 128, 128, 255 };
                        if (inEffect(acting_role_, r))
                        {
                            color = { 255, 255, 255, 255 };
                        }
                    }
                    int pic;
                    if (r == acting_role_)
                    {
                        pic = calRolePic(r, action_type_, action_frame_);
                    }
                    else
                    {
                        pic = calRolePic(r);
                    }
                    if (r->HP <= 0)
                    {
                        alpha = dead_alpha_;
                    }
                    TextureManager::getInstance()->renderTexture(path, pic, p.x, p.y, color, alpha);
                    renderExtraRoleInfo(r, p.x, p.y);
                }
                if (effect_id_ >= 0 && haveEffect(ix, iy))
                {
                    std::string path = convert::formatString("eft/eft%03d", effect_id_);
                    int dis = calDistance(acting_role_->X(), acting_role_->Y(), ix, iy);
                    num = effect_frame_ - dis + rng_.rand_int(3) - rng_.rand_int(3);
                    TextureManager::getInstance()->renderTexture(path, num, p.x, p.y, { 255, 255, 255, 255 }, 224);
                }
            }
        }
    }
    Engine::getInstance()->renderAssistTextureToWindow();

    if (semi_real_)
    {
        int h = Engine::getInstance()->getWindowHeight();
        TextureManager::getInstance()->renderTexture("title", 202, 200, h - 100);
        for (auto r : battle_roles_)
        {
            int x = 300 + r->Progress / 2;
            uint8_t alpha = 255;
            if (r->HP <= 0)
            {
                alpha = dead_alpha_;
            }
            TextureManager::getInstance()->renderTexture("head", r->HeadID, x, h - 100, { 255, 255, 255, 255 }, alpha, 0.25, 0.25);
        }
    }

    if (result_ >= 0)
    {
        Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    }

    //printf("Battle scene drawn\n");
}

void BattleScene::dealEvent(BP_Event& e)
{
    if (battle_roles_.empty())
    {
        exitWithResult(0);
    }

    Role* role = nullptr;
    if (semi_real_ == 0)
    {
        //选择位于人物数组中的第一个人
        role = battle_roles_[0];

        //若第一个人已经行动过，说明所有人都行动了，则清除行动状态，重排人物
        if (role->Acted != 0)
        {
            resetRolesAct();
            sortRoles();
            role = battle_roles_[0];
        }
    }
    else
    {
        //首先试图选出一人
        for (auto r : battle_roles_)
        {
            if (r->Progress > 1000)
            {
                role = r;
                break;
            }
        }
        //无法选出人，增加所有人进度条，继续
        if (role == nullptr)
        {
            for (auto r : battle_roles_)
            {
                r->Progress += r->Speed / 4;
            }
            return;
        }
    }

    acting_role_ = role;

    //定位
    man_x_ = role->X();
    man_y_ = role->Y();
    select_x_ = role->X();
    select_y_ = role->Y();
    head_self_->setRole(role);
    head_self_->setState(Element::Pass);

    //行动
    action(role);

    //如果此人成功行动过，则放到队尾
    if (role->Acted)
    {
        if (semi_real_ == 0)
        {
            battle_roles_.erase(battle_roles_.begin());
            battle_roles_.push_back(role);
        }
        else
        {
            role->Progress -= 1000;
            resetRolesAct();
        }
        poisonEffect(role);
    }

    //清除被击退的人物
    clearDead();

    //检测战斗结果
    int battle_result = checkResult();

    //我方胜
    if (battle_result >= 0)
    {
        result_ = battle_result;
        if (result_ == 0 || result_ == 1 && fail_exp_)
        {
            calExpGot();
        }
        setExit(true);
    }
}

void BattleScene::dealEvent2(BP_Event& e)
{
    if (isPressCancel(e))
    {
        for (auto r : battle_roles_)
        {
            if (r->Team == 0 && r->Auto == 1)    //注意：auto为其他值的不能取消
            {
                r->Auto = 0;
            }
        }
    }
}

void BattleScene::onEntrance()
{
    calViewRegion();
    Audio::getInstance()->playMusic(info_->Music);
    //注意此时才能得到窗口的大小，用来设置头像的位置
    head_self_->setPosition(80, 100);

    Element::addOnRootTop(MainScene::getInstance()->getWeather());

    //earth_texture_ = Engine::getInstance()->createARGBRenderedTexture(COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);
    //Engine::getInstance()->setRenderTarget(earth_texture_);
    ////二者之差是屏幕中心与大纹理的中心的距离
    //for (int i1 = 0; i1 < COORD_COUNT; i1++)
    //{
    //    for (int i2 = 0; i2 < COORD_COUNT; i2++)
    //    {
    //        auto p = getPositionOnWholeEarth(i1, i2);
    //        int num = earth_layer_->data(i1, i2) / 2;
    //        //无高度地面
    //        if (num > 0)
    //        {
    //            TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
    //        }
    //    }
    //}
    //Engine::getInstance()->resetRenderTarget();

    readBattleInfo();
    //初始状态
    for (auto r : battle_roles_)
    {
        setRoleInitState(r);
        r->Progress = 0;
    }
    //排序
    sortRoles();
    acting_role_ = battle_roles_[0];
}

void BattleScene::onExit()
{
    //清空全部角色的位置层
    for (auto r : Save::getInstance()->getRoles())
    {
        r->setRolePositionLayer(nullptr);
    }
    Element::removeFromRoot(MainScene::getInstance()->getWeather());
    if (earth_texture_)
    {
        Engine::destroyTexture(earth_texture_);
    }
}

void BattleScene::backRun()
{
}

void BattleScene::setupRolePosition(Role* r, int team, int x, int y)
{
    if (r == nullptr)
    {
        return;
    }
    r->setPosition(x, y);
    r->Team = team;
    readFightFrame(r);
    r->FaceTowards = rng_.rand_int(4);
    battle_roles_.push_back(r);
}

//读取战斗信息，确定是选人物还是自动人物
void BattleScene::readBattleInfo()
{
    man_x_ = COORD_COUNT / 2;
    man_y_ = COORD_COUNT / 2;

    if (network_)
    {
        // 网络处理
        unsigned int seed;
        int friends;
        std::vector<RoleSave> sandBoxRoles;
        network_->getResults(seed, friends, sandBoxRoles);
        rng_.set_seed(seed + 1);
        Save::getInstance()->resetRData(sandBoxRoles);
        //设置全部角色的位置层，避免今后出错
        for (auto r : Save::getInstance()->getRoles())
        {
            r->setRolePositionLayer(role_layer_);
            r->Team = 2;    //先全部设置成不存在的阵营
            r->Auto = 1;
        }
        // 这里代码写的很差，不过先这样吧
        if (network_->isHost())
        {
            // friends_占据前几个
            for (int i = 0; i < friends; i++)
            {
                auto r = Save::getInstance()->getRole(i);
                setupRolePosition(r, 0, info_->TeamMateX[i], info_->TeamMateY[i]);
                friends_.push_back(r);
            }
            // 因为知道friends_大小，可得知几个敌人
            for (int i = 0; i < sandBoxRoles.size() - friends; i++)
            {
                auto r = Save::getInstance()->getRole(i + friends);
                setupRolePosition(r, 1, info_->EnemyX[i], info_->EnemyY[i]);
            }
        }
        else
        {
            // friends_占据后几个
            // 先对方
            for (int i = 0; i < sandBoxRoles.size() - friends; i++)
            {
                auto r = Save::getInstance()->getRole(i);
                // TeamMateX实际是对面的
                setupRolePosition(r, 1, info_->TeamMateX[i], info_->TeamMateY[i]);
            }
            // 然后是自己
            for (int i = 0; i < friends; i++)
            {
                int friend_idx = sandBoxRoles.size() - friends + i;
                auto r = Save::getInstance()->getRole(friend_idx);
                setupRolePosition(r, 0, info_->EnemyX[i], info_->EnemyY[i]);
                friends_.push_back(r);
            }
        }
    }
    else
    {
        // 正常战斗
        //设置全部角色的位置层，避免今后出错
        for (auto r : Save::getInstance()->getRoles())
        {
            r->setRolePositionLayer(role_layer_);
            r->Team = 2;    //先全部设置成不存在的阵营
            r->Auto = 1;
        }

        //首先设置位置和阵营，其他的后面统一处理
        //敌方
        for (int i = 0; i < BATTLE_ENEMY_COUNT; i++)
        {
            auto r = Save::getInstance()->getRole(info_->Enemy[i]);
            if (r)
            {
                battle_roles_.push_back(r);
                r->setPosition(info_->EnemyX[i], info_->EnemyY[i]);
                r->Team = 1;
                readFightFrame(r);
                r->FaceTowards = rng_.rand_int(4);
                man_x_ = r->X();
                man_y_ = r->Y();
            }
        }

        //判断是不是有自动战斗人物
        if (info_->AutoTeamMate[0] >= 0)
        {
            for (int i = 0; i < TEAMMATE_COUNT; i++)
            {
                auto r = Save::getInstance()->getRole(info_->AutoTeamMate[i]);
                if (r)
                {
                    friends_.push_back(r);
                    r->Auto = 2;    //由AI控制
                }
            }
        }
        else
        {
            auto team_menu = new TeamMenu();
            team_menu->setMode(1);
            team_menu->run();
            friends_ = team_menu->getRoles();
            delete team_menu;
        }
        //队友
        for (int i = 0; i < friends_.size(); i++)
        {
            auto r = friends_[i];
            if (r)
            {
                battle_roles_.push_back(r);
                r->setPosition(info_->TeamMateX[i], info_->TeamMateY[i]);
                r->Team = 0;
            }
        }
    }

    //视角转至第一个敌人
    if (battle_roles_.size() > 0)
    {
        man_x_ = battle_roles_[0]->X();
        man_y_ = battle_roles_[0]->Y();
    }
}

void BattleScene::setRoleInitState(Role* r)
{
    r->Acted = 0;
    r->ExpGot = 0;
    r->clearShowStrings();
    r->FightingFrame = 0;
    r->Moved = 0;
    r->AI_Action = -1;
    r->Show.Effect = -1;
    r->Show.BattleHurt = 0;
    r->Show.ProgressChange = 0;
    r->Progress = 0;

    GameUtil::limit2(r->HP, r->MaxHP / 10, r->MaxHP);
    GameUtil::limit2(r->MP, r->MaxMP / 10, r->MaxMP);

    // 对方==1 则 自动
    r->Auto = r->Team;

    if (network_)
    {
        r->Competing = true;
        r->Networked = r->Team == 1;
        r->Auto = 0;
    }

    if (network_ || r->Team == 1)
    {
        // 联网，或者敌方都有回复状态的优待
        r->PhysicalPower = (std::max)(r->PhysicalPower, 90);
        r->HP = r->MaxHP;
        r->MP = r->MaxMP;
        r->Poison = 0;
        r->Hurt = 0;
    }

    //读取动作帧数
    readFightFrame(r);

    setFaceTowardsNearest(r);
    //r->FaceTowards = RandomClassical::rand(4);  //没头苍蝇随意选择面向
}

void BattleScene::setFaceTowardsNearest(Role* r, bool in_effect /*= false*/)
{
    //寻找离自己最近的敌方，设置面向
    int min_distance = COORD_COUNT * COORD_COUNT;
    Role* r_near = nullptr;
    for (auto r1 : battle_roles_)
    {
        if (!in_effect && r->Team != r1->Team || in_effect && inEffect(r, r1))
        {
            int dis = calRoleDistance(r, r1);
            if (dis < min_distance)
            {
                r_near = r1;
                min_distance = dis;
            }
        }
    }
    if (r_near)
    {
        auto tw = calTowards(r->X(), r->Y(), r_near->X(), r_near->Y());
        if (tw != Towards_None)
        {
            r->FaceTowards = tw;
        }
    }
}

void BattleScene::readFightFrame(Role* r)
{
    if (r->FightFrame[0] >= 0)
    {
        return;
    }
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

void BattleScene::sortRoles()
{
    if (semi_real_ == 0)
    {
        std::sort(battle_roles_.begin(), battle_roles_.end(), [](Role* r1, Role* r2)
        {
            return std::tie(r1->Speed, r1->ID) > std::tie(r2->Speed, r2->ID);
        });
    }
    else
    {
        std::sort(battle_roles_.begin(), battle_roles_.end(), [](Role* r1, Role* r2)
        {
            return std::tie(r1->Progress, r1->ID) > std::tie(r2->Progress, r2->ID);
        });
    }
}

void BattleScene::resetRolesAct()
{
    for (auto r : battle_roles_)
    {
        r->Acted = 0;
        r->Moved = 0;
        r->setPosition(r->X(), r->Y());
    }
}

int BattleScene::calMoveStep(Role* r)
{
    if (r->Moved)
    {
        return 0;
    }
    int speed = r->Speed;
    if (r->Equip0 >= 0)
    {
        auto i = Save::getInstance()->getItem(r->Equip0);
        speed += i->AddSpeed;
    }
    if (r->Equip1 >= 0)
    {
        auto i = Save::getInstance()->getItem(r->Equip1);
        speed += i->AddSpeed;
    }
    return speed / 15 + 1;
}

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
                return r->FightFrame[i] * r->FaceTowards;
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
                return total + r->FightFrame[style] * r->FaceTowards + frame;
            }
            total += r->FightFrame[i] * 4;
        }
    }
    return r->FaceTowards;
}

void BattleScene::calSelectLayer(int x, int y, int team, int mode, int step /*= 0*/)
{
    if (mode == 0)
    {
        select_layer_->setAll(-1);
        std::vector<Point> cal_stack;
        select_layer_->data(x, y) = step;
        cal_stack.push_back({ x, y });
        int count = 0;
        while (step >= 0)
        {
            std::vector<Point> cal_stack_next;
            auto check_next = [&](Point p1) -> void
            {
                //未计算过且可以走的格子参与下一步的计算
                if (select_layer_->data(p1.x, p1.y) == -1 && canWalk(p1.x, p1.y))
                {
                    select_layer_->data(p1.x, p1.y) = step - 1;
                    cal_stack_next.push_back(p1);
                    count++;
                }
            };
            for (auto p : cal_stack)
            {
                //检测是否在敌方身旁，视情况打开此选项
                if (!isNearEnemy(team, p.x, p.y) || (p.x == x && p.y == y))
                {
                    //检测4个相邻点
                    check_next({ p.x - 1, p.y });
                    check_next({ p.x + 1, p.y });
                    check_next({ p.x, p.y - 1 });
                    check_next({ p.x, p.y + 1 });
                }
                if (count >= COORD_COUNT * COORD_COUNT)
                {
                    break;
                }    //最多计算次数，避免死掉
            }
            if (cal_stack_next.size() == 0)
            {
                break;
            }    //无新的点，结束
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
                select_layer_->data(ix, iy) = step - calDistance(ix, iy, x, y);
            }
        }
    }
    else if (mode == 2)
    {
        select_layer_->setAll(0);
    }
    else if (mode == 3)
    {
        for (int ix = 0; ix < COORD_COUNT; ix++)
        {
            for (int iy = 0; iy < COORD_COUNT; iy++)
            {
                int dx = abs(ix - x);
                int dy = abs(iy - y);
                if (dx == 0 && dy <= step || dy == 0 && dx <= step)
                {
                    select_layer_->data(ix, iy) = 0;
                }
                else
                {
                    select_layer_->data(ix, iy) = -1;
                }
            }
        }
        select_layer_->data(x, y) = -1;
    }
    else
    {
        select_layer_->setAll(-1);
        select_layer_->data(x, y) = 0;
    }
}

void BattleScene::calSelectLayer(Role* r, int mode, int step /*= 0*/)
{
    calSelectLayer(r->X(), r->Y(), r->Team, mode, step);
}

void BattleScene::calSelectLayerByMagic(int x, int y, int team, Magic* magic, int level_index)
{
    if (magic->AttackAreaType == 0 || magic->AttackAreaType == 3)
    {
        calSelectLayer(x, y, team, 1, magic->SelectDistance[level_index]);
    }
    else if (magic->AttackAreaType == 1)
    {
        calSelectLayer(x, y, team, 3, magic->SelectDistance[level_index]);
    }
    else
    {
        calSelectLayer(x, y, team, 4);
    }
}

void BattleScene::calEffectLayer(int x, int y, int select_x, int select_y, Magic* m /*= nullptr*/, int level_index /*= 0*/)
{
    effect_layer_->setAll(-1);

    //若未指定武学，则认为只选择一个点
    if (m == nullptr || m->AttackAreaType == 0)
    {
        effect_layer_->data(select_x, select_y) = 0;
        return;
    }

    //此处比较累赘，就这样吧
    if (m->AttackAreaType == 1)
    {
        int tw = calTowards(x, y, select_x, select_y);
        int dis = m->SelectDistance[level_index];
        for (int ix = x - dis; ix <= x + dis; ix++)
        {
            for (int iy = y - dis; iy <= y + dis; iy++)
            {
                if (!isOutLine(ix, iy) && (x == ix || y == iy) && calTowards(x, y, ix, iy) == tw)
                {
                    effect_layer_->data(ix, iy) = 0;
                }
            }
        }
    }
    else if (m->AttackAreaType == 2)
    {
        int dis = m->SelectDistance[level_index];
        for (int ix = x - dis; ix <= x + dis; ix++)
        {
            for (int iy = y - dis; iy <= y + dis; iy++)
            {
                if (!isOutLine(ix, iy) && (x == ix || y == iy))
                {
                    effect_layer_->data(ix, iy) = 0;
                }
            }
        }
    }
    else if (m->AttackAreaType == 3)
    {
        int dis = m->AttackDistance[level_index];
        for (int ix = select_x - dis; ix <= select_x + dis; ix++)
        {
            for (int iy = select_y - dis; iy <= select_y + dis; iy++)
            {
                if (!isOutLine(ix, iy))
                {
                    effect_layer_->data(ix, iy) = 0;
                }
            }
        }
    }
}

bool BattleScene::inEffect(Role* r1, Role* r2)
{
    if (haveEffect(r2->X(), r2->Y()))
    {
        if (r1->ActTeam == 0 && r1->Team == r2->Team)
        {
            return true;
        }
        else if (r1->ActTeam != 0 && r1->Team != r2->Team)
        {
            return true;
        }
    }
    return false;
}

bool BattleScene::canSelect(int x, int y)
{
    return (!isOutLine(x, y) && select_layer_->data(x, y) >= 0);
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
    return building_layer_->data(x, y) > 0;
}

bool BattleScene::isWater(int x, int y)
{
    int num = earth_layer_->data(x, y) / 2;
    if (num >= 179 && num <= 181 || num == 261 || num == 511 || num >= 662 && num <= 665 || num == 674)
    {
        return true;
    }
    return false;
}

bool BattleScene::isRole(int x, int y)
{
    return role_layer_->data(x, y);
}

bool BattleScene::isOutScreen(int x, int y)
{
    return (abs(man_x_ - x) >= 16 || abs(man_y_ - y) >= 20);
}

bool BattleScene::isNearEnemy(int team, int x, int y)
{
    for (auto r1 : battle_roles_)
    {
        if (team != r1->Team && calDistance(r1->X(), r1->Y(), x, y) <= 1)
        {
            return true;
        }
    }
    return false;
}

Role* BattleScene::getSelectedRole()
{
    return role_layer_->data(select_x_, select_y_);
}

void BattleScene::action(Role* r)
{
    actionAnimation_ = nullptr;

    if (network_ && r->Team == 1)
    {
        // 网络连接，并且是对方行动
        // 获取对方行动
        receiveAction(r);
        battle_menu_->setResult(r->Network_Action);
    }
    else
    {
        // 己方行动
        battle_menu_->runAsRole(r);
    }

    r->Network_Action = battle_menu_->getResult();
    std::string str = battle_menu_->getResultString();

    //这里如果用整型表示返回，添加新项就太复杂了
    if (str == "移動")
    {
        actMove(r);
    }
    else if (str == "武學")
    {
        actUseMagic(r);
    }
    else if (str == "用毒")
    {
        actUsePoison(r);
    }
    else if (str == "解毒")
    {
        actDetoxification(r);
    }
    else if (str == "醫療")
    {
        actMedicine(r);
    }
    else if (str == "暗器")
    {
        actUseHiddenWeapon(r);
    }
    else if (str == "藥品")
    {
        actUseDrug(r);
    }
    else if (str == "等待")
    {
        actWait(r);
    }
    else if (str == "狀態")
    {
        actStatus(r);
    }
    else if (str == "自動")
    {
        actAuto(r);
        // 定为废操作 不然对面也自动了
        r->Network_Action = -1;
    }
    else if (str == "結束")
    {
        actRest(r);
    }

    //下一个行动的，菜单项从第一个开始
    if (r->Acted)
    {
        battle_menu_->setStartItem(0);
    }

    // 己方行动，传输
    if (network_ && r->Team == 0)
    {
        sendAction(r);
    }

    // 跑动画
    if (actionAnimation_)
    {
        actionAnimation_();
    }

    // 播放完动画，清空显示
    // 需要手动清理一下其他人的显示效果
    for (auto r : battle_roles_)
    {
        r->Show.clear();
    }
}

void BattleScene::actMove(Role* r)
{
    int step = calMoveStep(r);
    calSelectLayer(r, 0, step);
    battle_cursor_->setRoleAndMagic(r);
    battle_cursor_->setMode(BattleCursor::Move);
    if (battle_cursor_->run() == 0)
    {
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 2, 0, Role::getMaxValue()->PhysicalPower);
        r->setPrevPosition(r->X(), r->Y());
        r->Network_MoveX = select_x_;
        r->Network_MoveY = select_y_;
        r->ExpGot += 1;
        r->Moved = 1;
        actionAnimation_ = [this, r]()
        {
            moveAnimation(r, select_x_, select_y_);
        };
    }
}

void BattleScene::actUseMagic(Role* r)
{
    BattleMagicMenu magic_menu;
    while (true)
    {
        Magic* magic = nullptr;
        if (r->Networked)
        {
            magic = r->Network_Magic;
        }
        else
        {
            magic_menu.setStartItem(r->SelectedMagic);
            magic_menu.runAsRole(r);
            magic = magic_menu.getMagic();
            r->SelectedMagic = magic_menu.getResult();
        }
        if (magic == nullptr)
        {
            break;
        }    //可能是退出游戏，或者是没有选武功
        r->ActTeam = 1;
        //level_index表示从0到9，而level从0到999
        int level_index = r->getMagicLevelIndex(magic->ID);
        calSelectLayerByMagic(r->X(), r->Y(), r->Team, magic, level_index);
        //选择目标
        battle_cursor_->setMode(BattleCursor::Action);
        battle_cursor_->setRoleAndMagic(r, magic, level_index);
        int selected = battle_cursor_->run();
        //取消选择目标则重新进入选武功
        if (selected < 0)
        {
            continue;
        }
        else
        {
            r->Network_ActionX = select_x_;
            r->Network_ActionY = select_y_;
            r->Network_Magic = magic;
            actUseMagicSub(r, magic);
            break;
        }
    }
}

void BattleScene::actUseMagicSub(Role* r, Magic* magic)
{
    // 每次攻击，每个人的文字动画数据
    std::vector<std::vector<Role::ActionShowInfo>> multi_shows;

    for (int i = 0; i <= GameUtil::sign(r->AttackTwice); i++)
    {
        int level_index = r->getMagicLevelIndex(magic->ID);
        //计算伤害
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 3, 0, Role::getMaxValue()->PhysicalPower);
        r->MP = GameUtil::limit(r->MP - magic->calNeedMP(level_index), 0, r->MaxMP);
        calMagiclHurtAllEnemies(r, magic);

        // 做显示部分，由于多次攻击，并且数据动画分离，需要分开保存显示信息
        multi_shows.emplace_back();
        for (auto r : battle_roles_)
        {
            if (r->Show.BattleHurt != 0)
            {
                if (magic->HurtType == 0)
                {
                    r->addShowString(convert::formatString("-%d", r->Show.BattleHurt), { 255, 20, 20, 255 });
                }
                else if (magic->HurtType == 1)
                {
                    r->addShowString(convert::formatString("-%d", r->Show.BattleHurt), { 160, 32, 240, 255 });
                    // 吸内力不做渐变显示，麻烦
                    r->Show.BattleHurt = 0;
                }
            }
            multi_shows.back().push_back(r->Show);
            r->Show.clear();
        }

        //武学等级增加
        auto index = r->getMagicOfRoleIndex(magic);
        if (index >= 0)
        {
            r->MagicLevel[index] += 1 + rng_.rand_int(2);
            GameUtil::limit2(r->MagicLevel[index], 0, MAX_MAGIC_LEVEL);
        }
        printf("%s %s level is %d\n", PotConv::to_read(r->Name).c_str(), PotConv::to_read(magic->Name).c_str(), r->MagicLevel[index]);
    }
    r->Acted = 1;

    // multi_shows需要复制，因为已经离开此栈
    actionAnimation_ = [this, r, magic, multi_shows]() mutable
    {
        for (int i = 0; i < multi_shows.size(); i++)
        {
            // 播放攻击画面
            showMagicName(magic->Name);
            useMagicAnimation(r, magic);

            // 渐变效果
            std::vector<std::pair<int&, int>> animated_changes;
            for (int j = 0; j < multi_shows[i].size(); j++)
            {
                // 读取保存的文字动画数据
                battle_roles_[j]->Show = multi_shows[i][j];
                // 绑定，生命值和伤害值
                animated_changes.emplace_back(battle_roles_[j]->HP, -battle_roles_[j]->Show.BattleHurt);
                animated_changes.emplace_back(battle_roles_[j]->Progress, battle_roles_[j]->Show.ProgressChange);
            }
            showNumberAnimation(2, true, animated_changes);
        }
    };
}

void BattleScene::actUsePoison(Role* r)
{
    calSelectLayer(r, 1, calActionStep(r->UsePoison));
    battle_cursor_->setMode(BattleCursor::Action);
    battle_cursor_->setRoleAndMagic(r);
    r->ActTeam = 1;
    int selected = battle_cursor_->run();
    if (selected >= 0)
    {
        r->Network_ActionX = select_x_;
        r->Network_ActionY = select_y_;
        auto r2 = getSelectedRole();
        if (r2)
        {
            int v = GameUtil::usePoison(r, r2);
            r2->addShowString(convert::formatString("%+d", v), { 20, 255, 20, 255 });
            r->ExpGot += v;
        }
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 3, 0, Role::getMaxValue()->PhysicalPower);
        r->Acted = 1;
        actionAnimation_ = [this, r]()
        {
            actionAnimation(r, 0, 30);
            showNumberAnimation();
        };
    }
}

void BattleScene::actDetoxification(Role* r)
{
    calSelectLayer(r, 1, calActionStep(r->Detoxification));
    battle_cursor_->setMode(BattleCursor::Action);
    battle_cursor_->setRoleAndMagic(r);
    r->ActTeam = 0;
    int selected = battle_cursor_->run();
    if (selected >= 0)
    {
        r->Network_ActionX = select_x_;
        r->Network_ActionY = select_y_;
        auto r2 = getSelectedRole();
        if (r2)
        {
            int v = GameUtil::detoxification(r, r2);
            r2->addShowString(convert::formatString("-%d", -v), { 20, 200, 255, 255 });
            r->ExpGot += v;
        }
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 5, 0, Role::getMaxValue()->PhysicalPower);
        r->Acted = 1;
        actionAnimation_ = [this, r]()
        {
            actionAnimation(r, 0, 36);
            showNumberAnimation();
        };
    }
}

void BattleScene::actMedicine(Role* r)
{
    calSelectLayer(r, 1, calActionStep(r->Medicine));
    battle_cursor_->setMode(BattleCursor::Action);
    battle_cursor_->setRoleAndMagic(r);
    r->ActTeam = 0;
    int selected = battle_cursor_->run();
    if (selected >= 0)
    {
        r->Network_ActionX = select_x_;
        r->Network_ActionY = select_y_;
        auto r2 = getSelectedRole();
        if (r2)
        {
            int v = GameUtil::medicine(r, r2);
            r2->addShowString(convert::formatString("-%d", abs(v)), { 255, 255, 200, 255 });
            r->ExpGot += v;
        }
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 5, 0, Role::getMaxValue()->PhysicalPower);
        r->Acted = 1;
        actionAnimation_ = [this, r]()
        {
            actionAnimation(r, 0, 0);
            showNumberAnimation();
        };
    }
}

void BattleScene::actUseHiddenWeapon(Role* r)
{
    // 网络交流，不管物品
    BattleItemMenu item_menu;
    item_menu.setRole(r);
    item_menu.setForceItemType(3);
    item_menu.runAtPosition(300, 0);
    auto item = item_menu.getCurrentItem();
    if (item)
    {
        calSelectLayer(r, 1, calActionStep(r->HiddenWeapon));
        battle_cursor_->setMode(BattleCursor::Action);
        battle_cursor_->setRoleAndMagic(r);
        r->ActTeam = 1;
        int selected = battle_cursor_->run();
        if (selected >= 0)
        {
            auto r2 = getSelectedRole();
            int v = 0;
            if (r2)
            {
                v = calHiddenWeaponHurt(r, r2, item);
                r2->Show.BattleHurt = v;
                r2->addShowString(convert::formatString("-%d", v), { 255, 20, 20, 255 });
            }
            r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 5, 0, Role::getMaxValue()->PhysicalPower);
            item_menu.addItem(item, -1);
            r->ExpGot += v;
            r->Acted = 1;
            actionAnimation_ = [this, item, r, r2]()
            {
                showMagicName(item->Name);
                actionAnimation(r, 0, item->HiddenWeaponEffectID);
                std::vector<std::pair<int&, int>> animated_changes;
                if (r2)
                {
                    animated_changes.emplace_back(r2->HP, -r2->Show.BattleHurt);
                }
                showNumberAnimation(2, true, animated_changes);
            };
        }
    }
}

void BattleScene::actUseDrug(Role* r)
{
    // 网络交流，不管物品
    BattleItemMenu item_menu;
    item_menu.setForceItemType(2);
    item_menu.setRole(r);
    item_menu.runAtPosition(300, 0);

    auto item = item_menu.getCurrentItem();
    if (item)
    {
        Role r0 = *r;
        GameUtil::useItem(r, item);
        item_menu.addItem(item, -1);
        r->Acted = 1;
        actionAnimation_ = [this, r, r0, item]() mutable
        {
            ShowRoleDifference df(&r0, r);
            df.setText(convert::formatString("%s服用%s", r->Name, item->Name));
            df.setShowHead(false);
            df.setPosition(250, 220);
            df.setStayFrame(40);
            df.run();
        };
    }
}

void BattleScene::actWait(Role* r)
{
    //等待，将自己插入到最后一个没行动的人的后面
    for (int i = 1; i < battle_roles_.size(); i++)
    {
        if (battle_roles_[i]->Acted == 0)
        {
            battle_roles_.erase(battle_roles_.begin());
            battle_roles_.insert(battle_roles_.begin() + i, r);
        }
    }
}

void BattleScene::actStatus(Role* r)
{
    head_self_->setVisible(false);
    battle_cursor_->getHead()->setVisible(false);
    battle_cursor_->getUIStatus()->setVisible(true);

    calSelectLayer(r, 2, 0);
    battle_cursor_->setRoleAndMagic(r);
    battle_cursor_->setMode(BattleCursor::Check);
    battle_cursor_->run();

    head_self_->setVisible(true);
    battle_cursor_->getHead()->setVisible(true);
    battle_cursor_->getUIStatus()->setVisible(false);
}

void BattleScene::actAuto(Role* r)
{
    for (auto r : battle_roles_)
    {
        r->Auto = 1;
    }
}

void BattleScene::actRest(Role* r)
{
    if (!r->Moved)
    {
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower + 5, 0, Role::getMaxValue()->PhysicalPower);
        r->HP = GameUtil::limit(r->HP + 0.05 * r->MaxHP, 0, r->MaxHP);
        r->MP = GameUtil::limit(r->MP + 0.05 * r->MaxMP, 0, r->MaxMP);
    }
    r->Acted = 1;
}

void BattleScene::moveAnimation(Role* r, int x, int y)
{
    //从目标往回找确定路线
    std::vector<Point> way;
    auto check_next = [&](Point p1, int step) -> bool
    {
        if (canSelect(p1.x, p1.y) && select_layer_->data(p1.x, p1.y) == step)
        {
            way.push_back(p1);
            return true;
        }
        return false;
    };

    way.push_back({ x, y });
    for (int i = select_layer_->data(x, y); i < select_layer_->data(r->X(), r->Y()); i++)
    {
        int x1 = way.back().x, y1 = way.back().y;
        if (check_next({ x1 - 1, y1 }, i + 1))
        {
            continue;
        }
        if (check_next({ x1 + 1, y1 }, i + 1))
        {
            continue;
        }
        if (check_next({ x1, y1 - 1 }, i + 1))
        {
            continue;
        }
        if (check_next({ x1, y1 + 1 }, i + 1))
        {
            continue;
        }
    }

    for (int i = way.size() - 2; i >= 0; i--)
    {
        if (exit_)
        {
            break;
        }
        r->FaceTowards = calTowards(r->X(), r->Y(), way[i].x, way[i].y);
        r->setPosition(way[i].x, way[i].y);
        //setPosition(r->X(), r->Y());
        drawAndPresent(2);
    }
    r->setPosition(x, y);
    r->Moved = 1;
    select_layer_->setAll(-1);
}

//使用武学动画
void BattleScene::useMagicAnimation(Role* r, Magic* m)
{
    if (r && m)
    {
        Audio::getInstance()->playASound(m->SoundID);    //这里播放音效严格说不正确，不管了
        actionAnimation(r, m->MagicType, m->EffectID, r->Attack / 20);
    }
}

void BattleScene::actionAnimation(Role* r, int style, int effect_id, int shake /*= 0*/)
{
    if (r->X() != select_x_ || r->Y() != select_y_)
    {
        r->FaceTowards = calTowards(r->X(), r->Y(), select_x_, select_y_);
    }
    if (r->isAuto())
    {
        //自动的情況下面向一个敌人，否则看着很奇怪
        setFaceTowardsNearest(r, true);
    }
    auto frame_count = r->FightFrame[style];
    action_type_ = style;
    //Audio::getInstance()->playASound(style);
    for (action_frame_ = 0; action_frame_ < frame_count; action_frame_++)
    {
        // 如果有特效动画，抬手1帧后运行
        if (action_frame_ == 1)
        {
            if (r->Show.Effect != -1 || !r->Show.ShowStrings.empty())
            {
                showNumberAnimation(2, false);
                r->Show.clear();
                action_frame_ = 0;
                continue;
            }
        }
        if (exit_)
        {
            break;
        }
        drawAndPresent(animation_delay_);
    }
    action_frame_ = frame_count - 1;
    effect_id_ = effect_id;
    auto path = convert::formatString("eft/eft%03d", effect_id_);
    auto effect_count = TextureManager::getInstance()->getTextureGroupCount(path);
    Audio::getInstance()->playESound(effect_id);

    //由近到远的动画效果
    int min_dis = 9999;
    int max_dis = 0;
    for (int ix = 0; ix < COORD_COUNT; ix++)
    {
        for (int iy = 0; iy < COORD_COUNT; iy++)
        {
            if (haveEffect(ix, iy))
            {
                int dis = calDistance(r->X(), r->Y(), ix, iy);
                min_dis = (std::min)(min_dis, dis);
                max_dis = (std::max)(max_dis, dis);
            }
        }
    }

    for (effect_frame_ = -min_dis; effect_frame_ < effect_count + max_dis + 1; effect_frame_++)
    {
        if (exit_)
        {
            break;
        }
        if (shake > 0)
        {
            x_ = rng_.rand_int(shake) - rng_.rand_int(shake);
            y_ = rng_.rand_int(shake) - rng_.rand_int(shake);
        }
        drawAndPresent(animation_delay_);
    }
    action_frame_ = 0;
    action_type_ = -1;
    effect_frame_ = 0;
    effect_id_ = -1;
    x_ = 0;
    y_ = 0;
}

//r1使用武功magic攻击r2的伤害，结果为一正数
int BattleScene::calMagicHurt(Role* r1, Role* r2, Magic* magic)
{
    int level_index = Save::getInstance()->getRoleLearnedMagicLevelIndex(r1, magic);
    level_index = magic->calMaxLevelIndexByMP(r1->MP, level_index);
    if (magic->HurtType == 0)
    {
        if (r1->MP <= 10)
        {
            return 1 + rng_.rand_int(10);
        }
        int attack = r1->Attack + magic->Attack[level_index] / 3;
        int defence = r2->Defence;

        //装备的效果
        if (r1->Equip0 >= 0)
        {
            auto i = Save::getInstance()->getItem(r1->Equip0);
            attack += i->AddAttack;
        }
        if (r1->Equip1 >= 0)
        {
            auto i = Save::getInstance()->getItem(r1->Equip1);
            attack += i->AddAttack;
        }
        if (r2->Equip0 >= 0)
        {
            auto i = Save::getInstance()->getItem(r2->Equip0);
            defence += i->AddDefence;
        }
        if (r2->Equip1 >= 0)
        {
            auto i = Save::getInstance()->getItem(r2->Equip1);
            defence += i->AddDefence;
        }

        int v = attack - defence;
        //距离衰减
        int dis = calRoleDistance(r1, r2);
        v = v / exp((dis - 1) / 10);

        v += rng_.rand_int(10) - rng_.rand_int(10);
        if (v < 10)
        {
            v = 1 + rng_.rand_int(10);
        }
        //v = 999;  //测试用
        return v;
    }
    else if (magic->HurtType == 1)
    {
        int v = magic->HurtMP[level_index];
        v += rng_.rand_int(10) - rng_.rand_int(10);
        if (v < 10)
        {
            v = 1 + rng_.rand_int(10);
        }
        return v;
    }
    return 0;
}

//计算全部人物的伤害
int BattleScene::calMagiclHurtAllEnemies(Role* r, Magic* m, bool simulation)
{
    int total = 0;
    for (auto r2 : battle_roles_)
    {
        //非我方且被击中（即所在位置的效果层非负）
        if (r2->Team != r->Team && haveEffect(r2->X(), r2->Y()))
        {
            int hurt = calMagicHurt(r, r2, m);
            if (!simulation)
            {
                r2->Show.BattleHurt = hurt;
                if (m->HurtType == 0)
                {
                    r2->Show.BattleHurt = GameUtil::limit(r2->Show.BattleHurt, -(r2->MaxHP - r2->HP), r2->HP);
                    r->ExpGot += r2->Show.BattleHurt;
                    if (r2->HP == r2->Show.BattleHurt)
                    {
                        r->ExpGot += r2->Show.BattleHurt / 2;
                    }
                    r2->Show.ProgressChange = -hurt / 5;
                }
                else if (m->HurtType == 1)
                {
                    int prevMP = r2->MP;
                    r2->MP = GameUtil::limit(r2->MP - hurt, 0, r2->MaxMP);
                    r->MP = GameUtil::limit(r->MP + hurt, 0, r->MaxMP);
                    int hurt1 = prevMP - r2->MP;
                    r->ExpGot += hurt1 / 2;
                }
            }
            else
            {
                //这里是计算ai分数
                if (r->AttackTwice)
                {
                    hurt *= 2;
                }
                if (m->HurtType == 0)
                {
                    if (hurt >= r2->HP)
                    {
                        hurt = 1.25 * r2->HP;
                    }
                }
                else if (m->HurtType == 1)
                {
                    hurt /= 5;
                }
            }
            total += hurt;
        }
    }
    return total;
}

//返回值为一正数
int BattleScene::calHiddenWeaponHurt(Role* r1, Role* r2, Item* item)
{
    int v = r1->HiddenWeapon - item->AddHP;
    int dis = calRoleDistance(r1, r2);
    v = v / exp((dis - 1) / 10);
    v += rng_.rand_int(10) - rng_.rand_int(10);
    if (v < 1)
    {
        v = 1;
    }
    return v;
}

void BattleScene::showMagicName(std::string name)
{
    auto magic_name = new TextBox();
    magic_name->setText(name);
    magic_name->setPosition(450, 150);
    magic_name->setFontSize(20);
    magic_name->setStayFrame(40);
    magic_name->run();
    delete magic_name;
}

//显示数字，同时显示打退进度条
// delay: 延迟，默认2
// floating: 数字是否漂浮
// std::vector<std::pair<int&, int>> 渐变
// first为渐变对象，second为渐变数值
void BattleScene::showNumberAnimation(int delay, bool floating, const std::vector<std::pair<int&, int>>& animated_changes)
{
    //判断是否有需要显示的数字
    bool need_show = false;
    int single_loop_frames = 15;
    for (auto r : battle_roles_)
    {
        if (!animated_changes.empty() || !r->Show.ShowStrings.empty())
        {
            need_show = true;
        }
        if (r->Show.Effect != -1)
        {
            need_show = true;
            auto path = convert::formatString("eft/eft%03d", r->Show.Effect);
            auto effect_count = TextureManager::getInstance()->getTextureGroupCount(path);
            single_loop_frames = (std::max)(single_loop_frames, effect_count);
        }
    }
    if (!need_show)
    {
        return;
    }
    int total_frames = single_loop_frames * delay;
    for (int i_frame = 0; i_frame < single_loop_frames; i_frame++)
    {
        if (exit_)
        {
            break;
        }
        auto drawNumber = [&](void*) -> void
        {
            for (auto r : battle_roles_)
            {
                auto p = getPositionOnWindow(r->X(), r->Y(), man_x_, man_y_);
                // 有越界保护，直接显示就好了
                if (r->Show.Effect != -1)
                {
                    auto path = convert::formatString("eft/eft%03d", r->Show.Effect);
                    TextureManager::getInstance()->renderTexture(path, i_frame, p.x, p.y, { 255, 255, 255, 255 }, 224);
                }
                if (!r->Show.ShowStrings.empty())
                {
                    int y_pos = -75;
                    for (int i_show = 0; i_show < r->Show.ShowStrings.size(); i_show++)
                    {
                        auto& show_string = r->Show.ShowStrings[i_show];
                        int x = p.x - show_string.Size * show_string.Text.size() / 4;
                        int y = p.y - i_frame * 2 + y_pos;
                        if (!floating)
                        {
                            // 调整一下
                            y = p.y - total_frames + y_pos;
                        }
                        Font::getInstance()->draw(show_string.Text, show_string.Size, x, y, show_string.Color, 255 - 255 / total_frames * i_frame);
                        y_pos += show_string.Size + 2;
                    }
                }
            }
            // 渐变
            for (auto& change : animated_changes)
            {
                change.first += change.second / total_frames;
            }
        };
        drawAndPresent(delay, drawNumber);
    }

    // 渐变不能被整除，所以加回来，再重新算
    for (auto& change : animated_changes)
    {
        change.first += change.second - total_frames * (change.second / total_frames);
    }
}

void BattleScene::renderExtraRoleInfo(Role* r, int x, int y)
{
    if (r == nullptr)
    {
        return;
    }
    // 画个血条
    BP_Color outline_color = { 0, 0, 0, 128 };
    BP_Color background_color = { 0, 255, 0, 128 };    // 我方绿色
    if (r->Team == 1)
    {
        // 敌方红色
        background_color = { 255, 0, 0, 128 };
    }
    int hp_max_w = 24;
    int hp_x = x - hp_max_w/2;
    int hp_y = y - 60;
    int hp_h = 3;
    double perc = ((double)r->HP / r->MaxHP);
    if (perc < 0)
    {
        perc = 0;
    }

    double alpha = 1;
    if (r->HP <= 0)
    {
        alpha = dead_alpha_ / 255.0;
    }
    BP_Rect r0 = { hp_x, hp_y, hp_max_w, hp_h };
    Engine::getInstance()->renderSquareTexture(&r0, outline_color, 128 * alpha);
    BP_Rect r1 = { hp_x, hp_y, int(perc * hp_max_w), hp_h };
    Engine::getInstance()->renderSquareTexture(&r1, background_color, 192 * alpha);


    //Engine::getInstance()->fillColor(background_color, hp_x, hp_y, perc * hp_max_w, hp_h);
    // 严禁吐槽，画框框
    //Engine::getInstance()->fillColor(outline_color, hp_x, hp_y, hp_max_w, 1);
    //Engine::getInstance()->fillColor(outline_color, hp_x, hp_y + hp_h, hp_max_w, 1);
    //Engine::getInstance()->fillColor(outline_color, hp_x, hp_y, 1, hp_h);
    //Engine::getInstance()->fillColor(outline_color, hp_x + hp_max_w, hp_y, 1, hp_h);
}

void BattleScene::clearDead()
{
    //判断是否有人应退场
    bool found_dead = false;
    for (auto r : battle_roles_)
    {
        if (r->HP <= 0)
        {
            found_dead = true;
            break;
        }
    }
    if (!found_dead)
    {
        return;
    }

    //退场动画，清理人物
    for (int i = 0; i <= 25; i++)
    {
        dead_alpha_ = 255 - i * 10;
        if (dead_alpha_ < 0)
        {
            dead_alpha_ = 0;
        }
        drawAndPresent(animation_delay_);
    }
    dead_alpha_ = 255;

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
    battle_roles_ = alive;
}

//中毒的效果
void BattleScene::poisonEffect(Role* r)
{
    if (r)
    {
        //抗毒高者会自动解毒
        r->Poison -= r->AntiPoison;
        GameUtil::limit2(r->Poison, 0, Role::getMaxValue()->Poison);
        r->HP -= r->Poison / 3;
        //最低扣到1点
        if (r->HP < 1)
        {
            r->HP = 1;
        }
    }
}

int BattleScene::getTeamMateCount(int team)
{
    int count = 0;
    for (auto r : battle_roles_)
    {
        if (r->Team == team)
        {
            count++;
        }
    }
    return count;
}

//检查是否有一方全灭
//返回负值表示仍需持续，返回非负则为胜利方的team标记
//实际上只是检测我方人数与当前总人数是否相等或者为0
//更复杂的判断请使用set或者map
//0-我方胜，1-敌方胜，-1-胜负未分
int BattleScene::checkResult()
{
    int team0 = getTeamMateCount(0);
    if (team0 == battle_roles_.size())
    {
        return 0;
    }
    if (team0 == 0)
    {
        return 1;
    }
    return -1;
}

void BattleScene::calExpGot()
{
    head_self_->setVisible(false);

    std::vector<Role*> alive_teammate;
    if (result_ == 0)
    {
        for (auto r : battle_roles_)
        {
            if (r->Team == 0)
            {
                alive_teammate.push_back(r);
            }
        }
    }
    else
    {
        alive_teammate = friends_;
    }
    if (alive_teammate.empty())
    {
        return;
    }
    //还在场的人获得经验
    for (auto r : alive_teammate)
    {
        r->ExpGot += info_->Exp / alive_teammate.size();
    }

    auto show_exp = new ShowExp();
    show_exp->setRoles(alive_teammate);
    show_exp->run();
    delete show_exp;

    //升级，修炼物品
    auto diff = new ShowRoleDifference();
    for (auto r : alive_teammate)
    {
        if (exit_)
        {
            break;
        }

        Role r0 = *r;    //用于比较的状态

        auto item = Save::getInstance()->getItem(r->PracticeItem);

        if (r->Level >= Role::getMaxValue()->Level)
        {
            //已满级，全加到物品经验
            r->ExpForItem += r->ExpGot;
        }
        else if (item)
        {
            //未满级，平分经验
            r->Exp += r->ExpGot / 2;
            r->ExpForItem += r->ExpGot / 2;
        }
        else
        {
            //其余情况全加到人物经验
            r->Exp += r->ExpGot;
        }
        r->ExpForMakeItem += r->ExpGot;

        //避免越界
        if (r->Exp < r0.Exp)
        {
            r->Exp = Role::getMaxValue()->Exp;
        }
        if (r->ExpForItem < r0.ExpForItem)
        {
            r->ExpForItem = Role::getMaxValue()->Exp;
        }

        //升级
        int change = 0;
        while (GameUtil::canLevelUp(r))
        {
            GameUtil::levelUp(r);
            change++;
        }
        if (change)
        {
            diff->setTwinRole(&r0, r);
            diff->setText("升級");
            diff->run();
        }

        //修炼秘笈
        if (item)
        {
            r0 = *r;
            change = 0;
            while (GameUtil::canFinishedItem(r))
            {
                GameUtil::useItem(r, item);
                change++;
            }
            if (change)
            {
                diff->setTwinRole(&r0, r);
                diff->setText(convert::formatString("修煉%s成功", item->Name));
                diff->run();
            }
            if (item->MakeItem[0] >= 0 && r->ExpForMakeItem >= item->NeedExpForMakeItem && Event::getInstance()->haveItemBool(item->NeedMaterial))
            {
                std::vector<ItemList> make_item;
                for (int i = 0; i < 5; i++)
                {
                    if (item->MakeItem[i] >= 0)
                    {
                        make_item.push_back({ item->MakeItem[i], item->MakeItemCount[i] });
                    }
                }
                int index = rng_.rand_int(make_item.size());
                Event::getInstance()->addItem(make_item[index].item_id, make_item[index].count);
                Event::getInstance()->addItemWithoutHint(item->NeedMaterial, -1);
                r->ExpForMakeItem = 0;
            }
        }
    }
    delete diff;
}

void BattleScene::setupNetwork(std::unique_ptr<BattleNetwork> net, int battle_id)
{
    network_ = std::move(net);
    setID(battle_id);
}

void BattleScene::receiveAction(Role* r)
{
    // 如果是敌人，则获取敌人行动，假装敌人是个AI
    // 己方行动的时候也要同时记录AI
    BattleNetwork::SerializableBattleAction action;
    auto f = [](DrawableOnCall* d)
    {
        Font::getInstance()->draw("等待对方玩家行动...", 40, 30, 30, { 200, 200, 50, 255 });
    };
    DrawableOnCall waitThis(f);
    // getOpponentAction读取完毕会调用此函数关闭显示
    auto exit = [&waitThis, this](std::error_code err, std::size_t bytes)
    {
        printf("recv %s\n", err.message().c_str());
        waitThis.setExit(true);
        if (err)
        {
            this->setExit(true);
        }
    };
    // 打开后既开始获取数据
    waitThis.setEntrance([this, &action, exit]()
    {
        network_->getOpponentAction(action, exit);
    });
    waitThis.run();
    // 这里返回后，就已经获得action
    action.print();
    r->Network_Action = action.Action;
    r->Network_ActionX = action.ActionX;
    r->Network_ActionY = action.ActionY;
    r->Network_Item = Save::getInstance()->getItem(action.itemID);
    r->Network_Magic = Save::getInstance()->getMagic(action.magicID);
    r->Network_MoveX = action.MoveX;
    r->Network_MoveY = action.MoveY;
    r->Networked = true;
}

void BattleScene::sendAction(Role* r)
{
    if (r->Network_Action == -1)
    {
        return;
    }
    BattleNetwork::SerializableBattleAction action;
    action.Action = r->Network_Action;
    action.ActionX = r->Network_ActionX;
    action.ActionY = r->Network_ActionY;
    if (r->Network_Item)
    {
        action.itemID = r->Network_Item->ID;
    }
    else
    {
        action.itemID = -1;
    }
    if (r->Network_Magic)
    {
        action.magicID = r->Network_Magic->ID;
    }
    else
    {
        action.magicID = 0;
    }
    action.MoveX = r->Network_MoveX;
    action.MoveY = r->Network_MoveY;
    action.print();
    network_->sendMyAction(action);
}